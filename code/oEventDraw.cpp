/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"

#include <vector>
#include <deque>
#include <set>
#include <algorithm>
#include <chrono>
#include <random>

#include "oEvent.h"
#include "gdioutput.h"

#include "random.h"

#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "oEventDraw.h"
#include "meosexception.h"

int ClassInfo::sSortOrder = 0;

bool ClassInfo::showWarning(const wstring& w) const {
  if (warnHash == 0 && w.empty())
    return true;
  else if (warnHash != 0 && w.empty()) {
    warnHash = 0;
    return false;
  }
  else {
    uint64_t warnHashNew = 0;
    for (int j = 0; j < w.length(); j++) {
      warnHashNew = warnHashNew * 997 + w[j];
    }
    if (warnHashNew == 0)
      warnHashNew = 1; // Will not happen in practice

    if (warnHashNew != warnHash) {
      warnHash = warnHashNew;
      return false;
    }
    return true;
  }

}


DrawInfo::DrawInfo() = default;

bool ClassInfo::operator<(const ClassInfo& ci) const {
  if (sSortOrder == 0) {
    return sortFactor > ci.sortFactor;
  }
  else if (sSortOrder == 2) {
    return pc->getSortIndex() < ci.pc->getSortIndex();
  }
  else if (sSortOrder == 3) {
    if (unique != ci.unique) {
      if (ci.nRunnersGroup != nRunnersGroup)
        return nRunnersGroup > ci.nRunnersGroup;
      else
        return unique < ci.unique;
    }
    else
      return firstStart < ci.firstStart;
  }
  else
    return firstStart < ci.firstStart;
}

struct ClassBlockInfo {
  int FirstControl;
  int nRunners;
  int Depth;

  int FirstStart;

  bool operator<(const ClassBlockInfo& ci) const {
    return Depth < ci.Depth;
  }
};

namespace {

  void getLargestClub(map<int, vector<pRunner> >& clubRunner, vector<pRunner>& largest) {
    size_t maxClub = 0;
    for (map<int, vector<pRunner> >::iterator it =
      clubRunner.begin(); it != clubRunner.end(); ++it) {
      maxClub = max(maxClub, it->second.size());
    }

    for (map<int, vector<pRunner> >::iterator it =
      clubRunner.begin(); it != clubRunner.end(); ++it) {
      if (it->second.size() == maxClub) {
        swap(largest, it->second);
        clubRunner.erase(it);
        return;
      }
    }
  }

  void getRange(int size, vector<int>& p) {
    p.resize(size);
    for (size_t k = 0; k < p.size(); k++)
      p[k] = k;
  }

  void drawSOFTMethod(vector<pRunner>& runners, bool handleBlanks) {
    if (runners.empty())
      return;

    //Group runners per club
    map<int, vector<pRunner>> clubRunner;

    for (size_t k = 0; k < runners.size(); k++) {
      int clubId = runners[k] ? runners[k]->getClubId() : -1;
      clubRunner[clubId].push_back(runners[k]);
    }

    vector<vector<pRunner>> runnerGroups(1);

    // Find largest club
    getLargestClub(clubRunner, runnerGroups[0]);

    int largeSize = runnerGroups[0].size();
    int ngroups = (runners.size() + largeSize - 1) / largeSize;
    runnerGroups.resize(ngroups);

    while (!clubRunner.empty()) {
      // Find the smallest available group
      unsigned small = runners.size() + 1;
      int cgroup = -1;
      for (size_t k = 1; k < runnerGroups.size(); k++)
        if (runnerGroups[k].size() < small) {
          cgroup = k;
          small = runnerGroups[k].size();
        }

      // Add the largest remaining group to the smallest.
      vector<pRunner> largest;
      getLargestClub(clubRunner, largest);
      runnerGroups[cgroup].insert(runnerGroups[cgroup].end(), largest.begin(), largest.end());
    }

    unsigned maxGroup = runnerGroups[0].size();

    //Permute the first group
    vector<int> pg(maxGroup);
    getRange(pg.size(), pg);
    permute(pg);
    vector<pRunner> pr(maxGroup);
    for (unsigned k = 0; k < maxGroup; k++)
      pr[k] = runnerGroups[0][pg[k]];
    runnerGroups[0] = pr;

    //Find the largest group
    for (size_t k = 1; k < runnerGroups.size(); k++)
      maxGroup = max<int>(maxGroup, runnerGroups[k].size());

    if (handleBlanks) {
      //Give all groups same size (fill with 0)
      for (size_t k = 1; k < runnerGroups.size(); k++)
        runnerGroups[k].resize(maxGroup);
    }

    // Apply algorithm recursivly to groups with several clubs
    for (size_t k = 1; k < runnerGroups.size(); k++)
      drawSOFTMethod(runnerGroups[k], true);

    // Permute the order of groups
    vector<int> p(runnerGroups.size());
    getRange(p.size(), p);
    permute(p);

    // Write back result
    int index = 0;
    for (unsigned level = 0; level < maxGroup; level++) {
      for (size_t k = 0; k < runnerGroups.size(); k++) {
        int gi = p[k];
        if (level < runnerGroups[gi].size() && (runnerGroups[gi][level] != 0 || !handleBlanks))
          runners[index++] = runnerGroups[gi][level];
      }
    }

    if (handleBlanks)
      runners.resize(index);
  }

  void drawMeOSMethod(vector<pRunner>& runners) {
    if (runners.empty())
      return;

    map<int, vector<pRunner>> runnersPerClub;
    for (pRunner r : runners)
      runnersPerClub[r->getClubId()].push_back(r);
    vector<pair<int, int>> sizeClub;
    for (auto& rc : runnersPerClub)
      sizeClub.emplace_back(rc.second.size(), rc.first);

    sort(sizeClub.rbegin(), sizeClub.rend());

    int targetGroupSize = max<int>(runners.size() / 20, sizeClub.front().first);

    vector<vector<pRunner>> groups(1);
    for (auto& sc : sizeClub) {
      int currentSize = groups.back().size();
      int newSize = currentSize + sc.first;
      if (abs(currentSize - targetGroupSize) < abs(newSize - targetGroupSize)) {
        groups.emplace_back();
      }
      groups.back().insert(groups.back().end(), runnersPerClub[sc.second].begin(),
        runnersPerClub[sc.second].end());
    }

    size_t nRunnerTot = runners.size();

    if (groups.front().size() > (nRunnerTot + 2) / 2 && groups.size() > 1) {
      // We cannot distribute without clashes -> move some to other groups to prevent tail of same club
      int toMove = groups.front().size() - (nRunnerTot + 2) / 2;
      for (int i = 0; i < toMove; i++) {
        int dest = 1 + i % (groups.size() - 1);
        groups[dest].push_back(groups.front().back());
        groups.front().pop_back();
      }
    }

    // Permute groups
    size_t maxGroupSize = 0;
    vector<int> pv;

    for (auto& group : groups) {
      pv.clear();
      for (size_t i = 0; i < group.size(); i++) {
        pv.push_back(i);
      }
      permute(pv);
      vector<pRunner> tg;
      tg.reserve(group.size());
      for (int i : pv)
        tg.push_back(group[i]);
      tg.swap(group);
      maxGroupSize = max(maxGroupSize, group.size());

    }

    runners.clear();
    size_t takeMaxGroupInterval;
    if (groups.size() > 10)
      takeMaxGroupInterval = groups.size() / 4;
    else
      takeMaxGroupInterval = max<int>(2u, groups.size() - 2);

    deque<int> recentGroups;

    int ix = 0;

    if (maxGroupSize * 2 > nRunnerTot) {
      takeMaxGroupInterval = 2;
      ix = 1;
    }

    while (true) {
      ix++;
      pair<size_t, int> currentMaxGroup(0, -1);
      int otherNonEmpty = -1;
      size_t nonEmptyGroups = 0;
      for (size_t gx = 0; gx < groups.size(); gx++) {
        if (groups[gx].empty())
          continue;

        nonEmptyGroups++;

        if (groups[gx].size() > currentMaxGroup.first) {
          if (otherNonEmpty == -1 || groups[otherNonEmpty].size() < currentMaxGroup.first)
            otherNonEmpty = currentMaxGroup.second;

          currentMaxGroup.first = groups[gx].size();
          currentMaxGroup.second = gx;
        }
        else {
          if (otherNonEmpty == -1 || groups[otherNonEmpty].size() < groups[gx].size())
            otherNonEmpty = gx;
        }
      }
      if (currentMaxGroup.first == 0)
        break; // Done

      int groupToUse = currentMaxGroup.second;
      if (ix != takeMaxGroupInterval) {
        // Select some other group
        for (size_t attempt = 0; attempt < groups.size() * 2; attempt++) {
          int g = GetRandomNumber(groups.size());
          if (!groups[g].empty() && count(recentGroups.begin(), recentGroups.end(), g) == 0) {
            groupToUse = g;
            break;
          }
        }
      }
      else {
        ix = 0;
      }

      if (!recentGroups.empty()) { //Make sure to avoid duplicates of same group (if possible)
        if (recentGroups.back() == groupToUse && otherNonEmpty != -1)
          groupToUse = otherNonEmpty;
      }

      // Try to spread groups by ensuring that the same group is not used near itself
      recentGroups.push_back(groupToUse);
      if (recentGroups.size() > takeMaxGroupInterval || recentGroups.size() >= nonEmptyGroups)
        recentGroups.pop_front();

      runners.push_back(groups[groupToUse].back());
      groups[groupToUse].pop_back();
    }
  }
}


int DrawOptimAlgo::optimalLayout(int interval, vector<pair<int, int>>& classes) {
  sort(classes.begin(), classes.end());

  vector<int> chaining(interval, 0);

  for (int k = int(classes.size()) - 1; k >= 0; k--) {
    int ix = 0;
    // Find free position
    for (int i = 1; i < interval; i++) {
      if (chaining[i] < chaining[ix])
        ix = i;
    }
    int nr = classes[k].first;
    if (chaining[ix] > 0)
      nr += classes[k].second;

    chaining[ix] += 1 + interval * (nr - 1);
  }

  int last = chaining[0];
  for (int i = 1; i < interval; i++) {
    last = max(chaining[i], last);
  }
  return last;
}

bool DrawOptimAlgo::isFree(const DrawInfo& di, int nFields, int firstPos, int posInterval, ClassInfo& cInfo) const {
  int courseId = cInfo.courseId;

  int nEntries = cInfo.nRunners;
  bool disallowNeighbors = !di.allowNeighbourSameCourse;
  // Adjust first pos to make room for extra (before first start)
  if (cInfo.nExtra > 0) {
    int newFirstPos = firstPos - cInfo.nExtra * posInterval;
    while (newFirstPos < 0)
      newFirstPos += posInterval;
    int extra = (firstPos - newFirstPos) / posInterval;
    nEntries += extra;
    firstPos = newFirstPos;
  }

  //Check if free at all...
  for (int k = 0; k < nEntries; k++) {
    bool hasFree = false;
    size_t ix = firstPos + k * posInterval;

    for (int f = 0; f < max<int>(nFields, startField.size()); f++) {
      if (f >= startField.size()) {
        hasFree = true;
        break;
      }

      if (disallowNeighbors) {
        int prevT = -1, nextT = -1;
        if (posInterval > 1 && size_t(ix + 1) < startField[f].size())
          nextT = startField[f][ix + 1].courseId;
        if (posInterval > 1 && size_t(ix - 1) < startField[f].size())
          prevT = startField[f][ix - 1].courseId;

        if ((nextT > 0 && nextT == courseId) || (prevT > 0 && prevT == courseId))
          return false;
      }

      if (ix >= startField[f].size())
        hasFree = true;
      else {
        auto &sf = startField[f][ix];
        if (sf.isFree() && f < nFields)
          hasFree = true;
        else if (sf.occupied(cInfo.unique, cInfo.speed, cInfo.courseId))
          return false;//Type of course occupied. Cannot put it here;
      }
    }

    if (!hasFree) return false;//No free start position.
  }

  return true;
}

void DrawOptimAlgo::insertStart(const ClassInfo& cInfo) {
  int nEntries = cInfo.nRunners;
  int firstPos = cInfo.firstStart;

  // Adjust first pos to make room for extra (before first start)
  if (cInfo.nExtra > 0) {
    int newFirstPos = firstPos - cInfo.nExtra * cInfo.interval;
    while (newFirstPos < 0)
      newFirstPos += cInfo.interval;
    int extra = (firstPos - newFirstPos) / cInfo.interval;
    nEntries += extra;
    firstPos = newFirstPos;
  }

  if (startField.empty())
    startField.emplace_back();

  for (int k = 0; k < nEntries; k++) {
    int ix = firstPos + k * cInfo.interval;

    for (int f = 0; f <= startField.size(); f++) {
      if (ix >= startField[f].size())
        startField[f].resize(ix + 60);

      if (startField[f][ix].isFree()) {
        startField[f][ix].setUnique(cInfo.unique, cInfo.speed);
        startField[f][ix].courseId = cInfo.courseId;
        startField[f][ix].classId = cInfo.classId;
        break;
      }
      else if (f + 1 == startField.size()) {
        // Allocate one more line
        startField.emplace_back();
      }
    }
  }
}

int DrawOptimAlgo::initData(DrawInfo& di, vector<ClassInfo>& cInfo, int useNControls) {
  //OutputDebugString((L"Use NC:" + itow(useNControls)).c_str());

  otherClasses.clear();
  cInfo.clear();

  runnerPerGroup.clear();
  runnerPerCourse.clear();

  int nRunnersTot = 0;
  for (auto c_it : classes) {
    auto drawClass = di.classes.find(c_it->getId());
    ClassInfo* cPtr = nullptr;

    if (drawClass == di.classes.end()) {
      cPtr = &(otherClasses[c_it->getId()] = ClassInfo(&*c_it));
    }
    else
      cPtr = &drawClass->second;

    ClassInfo& ci = *cPtr;
    pCourse pc = c_it->getCourse();

    if (pc && useNControls < 1000) {
      ci.unique = 0;
      if (useNControls > 0 && pc->getNumControls() > 0)
        ci.unique = 1000 * 1000000 + pc->getIdSum(useNControls) % (1000000 * 999);

      if (ci.unique == 0)
        ci.unique = 10000 + pc->getId();

      ci.courseId = pc->getId();
    }
    else
      ci.unique = ci.classId;

    if (drawClass == di.classes.end())
      continue;

    int nr = 0;
    if (ci.startGroupId == 0)
      nr = c_it->getNumRunners(true, true, true);
    else {
      vector<pRunner> cr;
      oe->getRunners(c_it->getId(), 0, cr, false);
      for (pRunner r : cr) {
        if (r->getStatus() == StatusNotCompetiting || r->getStatus() == StatusCANCEL)
          continue;
        if (r->getStartGroup(true) == ci.startGroupId)
          nr++;
      }
    }

    if (ci.nVacant == -1 || !ci.nVacantSpecified || di.changedVacancyInfo) {
      // Auto initialize
      int nVacancies = int(nr * di.vacancyFactor + 0.5);
      nVacancies = max(nVacancies, di.minVacancy);
      nVacancies = min(nVacancies, di.maxVacancy);
      nVacancies = max(nVacancies, 0);

      if (di.vacancyFactor == 0)
        nVacancies = 0;

      ci.nVacant = nVacancies;
      ci.nVacantSpecified = false;
    }

    if (!ci.nExtraSpecified || di.changedExtraInfo) {
      // Auto initialize
      ci.nExtra = max(int(nr * di.extraFactor + 0.5), 1);

      if (di.extraFactor == 0)
        ci.nExtra = 0;
      ci.nExtraSpecified = false;
    }

    ci.nRunners = nr + ci.nVacant;

    if (ci.nRunners > 0) {
      nRunnersTot += ci.nRunners + ci.nExtra;
      cInfo.push_back(ci);
      runnerPerGroup[ci.unique] += ci.nRunners + ci.nExtra;
      runnerPerCourse[ci.courseId] += ci.nRunners + ci.nExtra;
    }
  }
  return nRunnersTot;
}

void DrawOptimAlgo::computeBestStartDepth(DrawInfo& di, vector<ClassInfo>& cInfo, int nFields, int useNControls, int alteration) {
  if (di.firstStart <= 0)
    di.firstStart = 0;

  int nRunnersTot = initData(di, cInfo, useNControls);

  maxGroup = 0;
  maxCourse = 0;
  maxNRunner = 0;
  int a = 1 + (alteration % 7);
  int b = (alteration % 3);
  int c = alteration % 5;

  for (size_t k = 0; k < cInfo.size(); k++) {
    maxNRunner = max(maxNRunner, cInfo[k].nRunners);
    cInfo[k].nRunnersGroup = runnerPerGroup[cInfo[k].unique];
    cInfo[k].nRunnersCourse = runnerPerCourse[cInfo[k].courseId];
    maxGroup = max(maxGroup, cInfo[k].nRunnersGroup);
    maxCourse = max(maxCourse, cInfo[k].nRunnersCourse);
    cInfo[k].sortFactor = cInfo[k].nRunners * a + cInfo[k].nRunnersGroup * b + cInfo[k].nRunnersCourse * c;
  }

  /*for (size_t k = 0; k < cInfo.size(); k++) {
    auto pc = oe->getClass(cInfo[k].classId);
    auto c = pc->getCourse();
    wstring cc;
    for (int i = 0; i < 3; i++)
      cc += itow(c->getControl(i)->getId()) + L",";
    wstring w = pc->getName() + L"; " + cc + L"; " + itow(cInfo[k].unique) + L"; " + itow(cInfo[k].nRunners) + L"\n";
    OutputDebugString(w.c_str());
  }*/

  di.numDistinctInit = runnerPerGroup.size();
  di.numRunnerSameInitMax = maxGroup;
  di.numRunnerSameCourseMax = maxCourse;
  // Calculate the theoretical best end position to use.
  bestEndPos = 0;

  for (map<int, int>::iterator it = runnerPerGroup.begin(); it != runnerPerGroup.end(); ++it) {
    vector< pair<int, int> > classes;
    for (size_t k = 0; k < cInfo.size(); k++) {
      if (cInfo[k].unique == it->first)
        classes.push_back(make_pair(cInfo[k].nRunners, cInfo[k].nExtra));
    }
    int optTime = optimalLayout(di.minClassInterval / di.baseInterval, classes);
    bestEndPos = max(optTime, bestEndPos);
  }

  if (nRunnersTot > 0)
    bestEndPos = max(bestEndPos, nRunnersTot / nFields);

  if (!di.allowNeighbourSameCourse)
    bestEndPos = max(bestEndPos, maxCourse * 2 - 1);
  else
    bestEndPos = max(bestEndPos, maxCourse);
}

int DrawOptimAlgo::countStarts(int pos) const {
  int nr = 0;
  for (auto& field : startField) {
    if (pos < field.size() && !field[pos].isFree())
      nr++;
  }
  return nr;
}

DrawOptimAlgo::DrawOptimAlgo(oEvent* oe) : oe(oe) {
  oe->getClasses(classes, false);
  oe->getRunners(-1, -1, runners);
}

void DrawOptimAlgo::optimizeStartOrder(DrawInfo& di,
  vector<ClassInfo>& cInfo,
  int nFields,
  int useNControls,
  int seed) {

  startField.clear();

  std::default_random_engine rnd(seed);
  std::uniform_int_distribution<std::mt19937::result_type> dist1000(0, 1000);

  if (bestEndPosGlobal == 0) {
    bestEndPosGlobal = 100000;
    for (int i = 1; i <= di.maxCommonControl && i <= 10; i++) {
      int ncc = i;
      if (i >= 10)
        ncc = 0;
      computeBestStartDepth(di, cInfo, nFields, ncc, 0);
      bestEndPosGlobal = min(bestEndPos, bestEndPosGlobal);
    }

    di.minimalStartDepth = bestEndPosGlobal * di.baseInterval;
  }

  computeBestStartDepth(di, cInfo, nFields, useNControls, seed);

  ClassInfo::sSortOrder = 0;
  sort(cInfo.begin(), cInfo.end());

  int maxSize = di.minClassInterval * maxNRunner;

  // Special case for constant time start
  if (di.baseInterval == 0) {
    di.baseInterval = 1;
    di.minClassInterval = 0;
  }

  int startGroup = 0;

  // Calculate an estimated maximal class intervall
  for (size_t k = 0; k < cInfo.size(); k++) {
    if (cInfo[k].startGroupId != 0)
      startGroup = cInfo[k].startGroupId; // Need to be constant
    int quotient = maxSize / (cInfo[k].nRunners * di.baseInterval);

    if (quotient * di.baseInterval > di.maxClassInterval)
      quotient = di.maxClassInterval / di.baseInterval;

    if (cInfo[k].nRunnersGroup >= maxGroup)
      quotient = di.minClassInterval / di.baseInterval;

    if (cInfo[k].hint != ClassInfo::Hint::Fixed)
      cInfo[k].interval = quotient;
  }

  int alternator = 0;

  insertExisting(di, startGroup);

  // Fill up classes with fixed starttime
  for (size_t k = 0; k < cInfo.size(); k++) {
    if (cInfo[k].hint == ClassInfo::Hint::Fixed) {
      insertStart(cInfo[k]);
    }
  }

  if (di.minClassInterval == 0) {
    // Set fixed start time
    for (size_t k = 0; k < cInfo.size(); k++) {
      if (cInfo[k].hint == ClassInfo::Hint::Fixed)
        continue;
      cInfo[k].firstStart = di.firstStart;
      cInfo[k].firstStartComputed = di.firstStart;
      cInfo[k].interval = 0;
    }
  }
  else {
    // Do the distribution
    bool oldAlgo = false;

    if (oldAlgo) {
      for (size_t k = 0; k < cInfo.size(); k++) {
        if (cInfo[k].hint == ClassInfo::Hint::Fixed)
          continue;

        int minPos = 1000000;
        int minEndPos = 1000000;
        int minInterval = cInfo[k].interval;
        int startV = di.minClassInterval / di.baseInterval;
        int endV = cInfo[k].interval;

        if (cInfo[k].fixedInterval != 0) {
          endV = startV = cInfo[k].fixedInterval;
        }

        for (int i = startV; i <= endV; i++) {

          int startpos = alternator % max(1, (bestEndPos - cInfo[k].nRunners * i) / 3);
          startpos = 0;
          int ipos = startpos;
          int t = 0;

          while (!isFree(di, nFields, ipos, i, cInfo[k])) {
            t++;

            // Algorithm to randomize start position
            // First startpos -> bestEndTime, then 0 -> startpos, then remaining
            if (t < (bestEndPos - startpos))
              ipos = startpos + t;
            else {
              ipos = t - (bestEndPos - startpos);
              if (ipos >= startpos)
                ipos = t;
            }
          }

          int endPos = ipos + i * cInfo[k].nRunners;
          if (endPos < minEndPos || endPos < bestEndPos) {
            minEndPos = endPos;
            minPos = ipos;
            minInterval = i;
          }
        }

        cInfo[k].firstStartComputed = cInfo[k].firstStart = minPos;
        cInfo[k].interval = minInterval;
        cInfo[k].overShoot = max(minEndPos - bestEndPosGlobal, 0);
        insertStart(cInfo[k]);

        alternator += seed;
      }
    }
    else {
      // New algorithm
      struct DrawClassesCourseInfo {
        int numTotal = 0;
        vector<int> clsIx;

        int layoutDepth(int interval, bool allowNeighbourSameCourse, const vector<ClassInfo>& cInfo) const {
          vector<int> last(interval);
          return layoutDepth(last, interval, allowNeighbourSameCourse, cInfo);
        }

        /** Get approximation of minimal start depth for group with specified interval */
        int layoutDepth(vector<int>& last, int interval, bool allowNeighbourSameCourse, const vector<ClassInfo>& cInfo) const {

          // Select the best start interval
          auto getBestIx = [&last]() {
            int best = 0;
            for (int j = 1; j < last.size(); j++) {
              if (last[j] < last[best])
                best = j;
            }
            return best;
            };

          if (!allowNeighbourSameCourse) {
            int depth = 0;
            for (int ix : clsIx) {
              if (cInfo[ix].fixedInterval)
                depth += (cInfo[ix].nRunners + cInfo[ix].nExtra) * cInfo[ix].fixedInterval;
              else
                depth += (cInfo[ix].nRunners + cInfo[ix].nExtra) * interval;
            }
            int best = getBestIx();
            last[best] += depth;
          }
          else {
            for (int ix : clsIx) {
              int depth;
              if (cInfo[ix].fixedInterval)
                depth = (cInfo[ix].nRunners + cInfo[ix].nExtra) * cInfo[ix].fixedInterval;
              else
                depth = (cInfo[ix].nRunners + cInfo[ix].nExtra) * interval;

              int best = getBestIx();
              last[best] += depth + best;
            }
          }

          return *max_element(last.begin(), last.end());
        }
      };

      struct DrawGroupInfo {
        map<pair<int, int>, DrawClassesCourseInfo> classesPerCourse;

        void addClass(int ix, int crsId, int num) {
          auto& crs = classesPerCourse[make_pair(-num, crsId)];
          crs.numTotal = num;
          crs.clsIx.push_back(ix);
        }

        vector<int> getClassIx() const {
          vector<int> out;
          for (auto& [key, crs] : classesPerCourse) {
            for (int ix : crs.clsIx)
              out.push_back(ix);
          }
          return out;
        }

        vector<int> getCourseDistribution() {
          vector<int> out;
          out.reserve(classesPerCourse.size());
          for (auto& [key, crs] : classesPerCourse) {
            out.push_back(crs.numTotal);
          }
        }

        /** Get approximation of minimal start depth for group with specified interval */
        int layoutDepth(int interval, bool allowNeighbourSameCourse, const vector<ClassInfo>& cInfo) const {
          vector<int> last(interval);
          for (auto& [key, crsGroup] : classesPerCourse) {
            crsGroup.layoutDepth(last, interval, allowNeighbourSameCourse, cInfo);
          }
          return *max_element(last.begin(), last.end());
        }
      };

      map<tuple<int, int, int>, DrawGroupInfo> classesPerGroup;

      for (size_t k = 0; k < cInfo.size(); k++) {
        if (cInfo[k].hint == ClassInfo::Hint::Fixed)
          continue;
        int numGroup = runnerPerGroup[cInfo[k].unique];
        int numCrs = runnerPerCourse[cInfo[k].courseId];
        int prio = 0;
        if (cInfo[k].hint == ClassInfo::Hint::Early)
          prio = -1;
        else if (cInfo[k].hint == ClassInfo::Hint::Late)
          prio = 1;

        //cInfo[k].hint
        auto& dgi = classesPerGroup[tuple(prio, -numGroup, cInfo[k].unique)];
        dgi.addClass(k, cInfo[k].courseId, numCrs);
      }

      for (auto& [key, groupInfo] : classesPerGroup) {
        vector<int> clsGroup = groupInfo.getClassIx();
        ClassInfo::Hint prio = std::get<0>(key) == 0 ? ClassInfo::Hint::Default : 
                    std::get<0>(key) < 0 ? ClassInfo::Hint::Early : ClassInfo::Hint::Late;

        // Determine start intervals to use for group
        int minV = 100;
        int maxV = 0;
        for (int k : clsGroup) {
          int startV = di.minClassInterval / di.baseInterval;
          int endV = di.maxClassInterval / di.baseInterval;
          if (cInfo[k].fixedInterval != 0) {
            endV = startV = cInfo[k].fixedInterval;
          }
          else {
            startV = min(cInfo[k].interval, startV);
            endV = max(cInfo[k].interval, endV);
          }
          minV = min(minV, startV);
          maxV = max(maxV, endV);
        }

        vector<pair<int, int>> overshootWithInterval;

        for (int i = minV; i <= maxV; i++) {
          int depth = groupInfo.layoutDepth(i, di.allowNeighbourSameCourse, cInfo);
          overshootWithInterval.emplace_back(max(0, depth - bestEndPosGlobal), i);
        }

        sort(overshootWithInterval.begin(), overshootWithInterval.end());
        int allowIx = 0;
        while ((allowIx + 1) < overshootWithInterval.size() && overshootWithInterval[allowIx + 1].first == overshootWithInterval[0].first)
          allowIx++;

        int sx = seed % (allowIx + 1);
        int groupInterval = overshootWithInterval[sx].second;

        auto transformPos = [&cInfo, this](int classIx, int interval, int ipos) {
          if (cInfo[classIx].hint != ClassInfo::Hint::Late)
            return ipos;
          else {
            int zp = bestEndPosGlobal - max(0, cInfo[classIx].nRunners * interval);
            int rPos = zp - ipos;
            return rPos >= 0 ? rPos : ipos;
          }
        };


        for (auto& [key2, crsInfo] : groupInfo.classesPerCourse) {
          std::shuffle(crsInfo.clsIx.begin(), crsInfo.clsIx.end(), rnd);

          int depthEstimate = crsInfo.layoutDepth(groupInterval, di.allowNeighbourSameCourse, cInfo);

          // Estimate suitble average gap between classes (pack to zero extra gap for early/late)
          int gapTot = prio != ClassInfo::Hint::Default ? 0 : max(0, bestEndPosGlobal - depthEstimate - bestEndPosGlobal / 10);
          int ipos = 0;

          for (int k : crsInfo.clsIx) {
            int interval = groupInterval;
            if (cInfo[k].fixedInterval != 0)
              interval = cInfo[k].fixedInterval;

            if (prio != ClassInfo::Hint::Late) {
              while (!isFree(di, nFields, ipos, interval, cInfo[k])) {
                ipos++;
                gapTot--;
              }
            }
            else {
              while (!isFree(di, nFields, transformPos(k, interval, ipos), interval, cInfo[k])) {
                ipos++;
                gapTot--;
              }
            }
            int localOptim = countStarts(ipos);
            int useOff = 0;
            for (int off = 1; off < interval; off++) {
              if (isFree(di, nFields, transformPos(k, interval, ipos+off), interval, cInfo[k])) {
                int cnt = countStarts(ipos + off);
                if (cnt < localOptim) {
                  useOff = off;
                  localOptim = cnt;
                }
              }
            }

            cInfo[k].firstStartComputed = cInfo[k].firstStart = transformPos(k, interval, ipos + useOff);
            cInfo[k].interval = interval;
            cInfo[k].overShoot = max(cInfo[k].firstStart + interval * cInfo[k].nRunners - bestEndPosGlobal, 0);
            insertStart(cInfo[k]);

            int gap = max(0, gapTot) / crsInfo.clsIx.size();
            if (!di.allowNeighbourSameCourse) {
              ipos += interval * cInfo[k].nRunners;
              if (gap > 0) 
                ipos += dist1000(rnd) % gap;
            }
            else
              ipos = 0;
          }
        }

        /*int minPos = 1000000;
        int minEndPos = 1000000;
        int minInterval = cInfo[k].interval;
        int startV = di.minClassInterval / di.baseInterval;
        int endV = cInfo[k].interval;

        if (cInfo[k].fixedInterval != 0) {
          endV = startV = cInfo[k].fixedInterval;
        }

        for (int i = startV; i <= endV; i++) {

          int startpos = alternator % max(1, (bestEndPos - cInfo[k].nRunners * i) / 3);
          startpos = 0;
          int ipos = startpos;
          int t = 0;

          while (!isFree(di, di.nFields, ipos, i, cInfo[k])) {
            t++;

            // Algorithm to randomize start position
            // First startpos -> bestEndTime, then 0 -> startpos, then remaining
            if (t < (bestEndPos - startpos))
              ipos = startpos + t;
            else {
              ipos = t - (bestEndPos - startpos);
              if (ipos >= startpos)
                ipos = t;
            }
          }

          int endPos = ipos + i * cInfo[k].nRunners;
          if (endPos < minEndPos || endPos < bestEndPos) {
            minEndPos = endPos;
            minPos = ipos;
            minInterval = i;
          }
        }

        cInfo[k].firstStart = minPos;
        cInfo[k].interval = minInterval;
        cInfo[k].overShoot = max(minEndPos - bestEndPosGlobal, 0);
        insertStart(cInfo[k]);

        alternator += alteration;*/
      }
    }
  }
}

void DrawOptimAlgo::insertExisting(const DrawInfo& di, int startGroup) {
  // Fill up with non-drawn classes
  for (auto& it : runners) {
    if (it->isRemoved())
      continue;
    int st = it->getStartTime();
    int relSt = st - di.firstStart;
    int relPos = relSt / di.baseInterval;
    constexpr int maxRelPosition = 60 * 24 * 2;

    if (st > 0 && relSt >= 0 && relPos < maxRelPosition && (relSt % di.baseInterval) == 0) {
      int cid = it->getClassId(true);
      if (otherClasses.count(cid) == 0) {
        if (startGroup == 0 || startGroup == it->getStartGroup(true))
          continue;
      }
      pClass cls = it->getClassRef(true);
      if (cls) {
        if (!di.startName.empty() && cls->getStart() != di.startName)
          continue;

        if (cls->hasFreeStart())
          continue;
      }

      int unique, courseId;

      if (auto res = otherClasses.find(cid); res != otherClasses.end()) {
        unique = res->second.unique;
        courseId = res->second.courseId;
      }
      else {
        if (auto res = di.classes.find(cid); res != di.classes.end()) {
          unique = res->second.unique;
          courseId = res->second.courseId;
        }
        else {
          unique = 12345678;
          courseId = it->getCourse(false) ? it->getCourse(false)->getId() : 0;
        }
      }

      int k = 0;
      while (true) {
        if (k >= (di.nFieldsMax + 1) * 2)
          break; // Do not allow uncontrolled growth. Position already (more than) full.

        if (k < startField.size() && relPos < startField[k].size() &&
          startField[k][relPos].sameUnique(unique) &&
          startField[k][relPos].courseId == courseId) {
          break; // Already represented
        }

        if (k == startField.size()) {
          startField.emplace_back();
        }
        if (relPos >= startField[k].size())
          startField[k].resize(relPos + 60);

        if (startField[k][relPos].isFree()) {
          startField[k][relPos].setUnique(unique, ClassInfo::Speed::Normal);
          startField[k][relPos].courseId = courseId;
          startField[k][relPos].classId = cls ? cls->getId() : 0;
          break;
        }
        k++;
      }
    }
  }
}

int DrawOptimAlgo::getLastStart() const {
  int last = -1;
  for (auto& field : startField) {
    for (int j = last + 1; j < field.size(); j++) {
      if (!field[j].isFree())
        last = std::max(last, j);
    }
  }
  return last;
}

vector<int> DrawOptimAlgo::getNumStartPerInterval() {
  vector<int> res(getLastStart() + 1);
  for (auto& field : startField) {
    for (int j = 0; j < field.size() && j < res.size(); j++) {
      if (!field[j].isFree())
        ++res[j];
    }
  }
  return res;
}

vector<tuple<int, wstring, DrawOptimAlgo::InfoType>> DrawOptimAlgo::checkDrawSettings(const DrawInfo& di, vector<ClassInfo>& cInfo) {

  startField.clear();
  vector<tuple<int, wstring, InfoType>> problems;
  DrawInfo diC = di;
  initData(diC, cInfo, di.maxCommonControl);

  insertExisting(diC, 0);

  // Fill up classes to draw
  for (size_t k = 0; k < cInfo.size(); k++) {
    insertStart(cInfo[k]);
  }

  set<pair<int, int>> duplicateClassCrs;
  set<pair<int, int>> duplicateClassUni;
  set<pair<int, int>> courseNbClass;

  vector<int> clsTmp;
  auto insertDuplicateCrs = [this, &clsTmp, &duplicateClassCrs](int pos, int crsId) {
    clsTmp.clear();
    for (int j = 0; j < startField.size(); j++) {
      if (pos < startField[j].size() && startField[j][pos].courseId == crsId) {
        clsTmp.push_back(startField[j][pos].classId);
      }
    }
    for (int i = 0; i < clsTmp.size(); i++) {
      for (int j = i + 1; j < clsTmp.size(); j++) {
        int a = clsTmp[i];
        int b = clsTmp[j];
        if (a < b)
          duplicateClassCrs.emplace(a, b);
        else
          duplicateClassCrs.emplace(b, a);
      }
    }
  };

  auto insertDuplicateUni = [this, &clsTmp, &duplicateClassUni](int pos, int uni) {
    clsTmp.clear();
    for (int j = 0; j < startField.size(); j++) {
      if (pos < startField[j].size() && startField[j][pos].sameUnique(uni)) {
        clsTmp.push_back(startField[j][pos].classId);
      }
    }
    for (int i = 0; i < clsTmp.size(); i++) {
      for (int j = i + 1; j < clsTmp.size(); j++) {
        int a = clsTmp[i];
        int b = clsTmp[j];
        if (a < b)
          duplicateClassUni.emplace(a, b);
        else
          duplicateClassUni.emplace(b, a);
      }
    }
  };

  auto insertCourseNb = [this, &clsTmp, &courseNbClass](int pos, int crsId) {
    clsTmp.clear();
    int cls = -1;
    for (int j = 0; j < startField.size(); j++) {
      if (pos < startField[j].size()) {
        if (startField[j][pos].courseId == crsId) {
          if (cls != startField[j][pos].classId) {
            clsTmp.push_back(startField[j][pos].classId);
            cls = startField[j][pos].classId;
          }
        }
        if (startField[j][pos - 1].courseId == crsId) {
          if (cls != startField[j][pos-1].classId) {
            clsTmp.push_back(startField[j][pos-1].classId); 
            cls = startField[j][pos - 1].classId;
          }
        }
      }
    }

    for (int i = 0; i < clsTmp.size(); i++) {
      for (int j = i + 1; j < clsTmp.size(); j++) {
        int a = clsTmp[i];
        int b = clsTmp[j];
        if (a < b)
          courseNbClass.emplace(a, b);
        else
          courseNbClass.emplace(b, a);
      }
    }
  };

  // Check for problems...
  set<int> crs, uni, crsPrev;
  for (int pos = 0; pos < startField[0].size(); pos++) {
    crs.clear();
    uni.clear();
    for (int j = 0; j < startField.size(); j++) {
      if (pos < startField[j].size() && !startField[j][pos].isFree()) {
        auto& sf = startField[j][pos];
        if (sf.courseId > 0 && !crs.insert(sf.courseId).second)
          insertDuplicateCrs(pos, sf.courseId);
        if (!uni.insert(sf.getUnique()).second)
          insertDuplicateUni(pos, sf.getUnique());
      }
    }
    for (int cid : crs) {
      if (crsPrev.count(cid))
        insertCourseNb(pos, cid);
    }
    crsPrev.swap(crs);
  }

  struct ClassProblem {
    vector<int> dupCourse;
    vector<int> nbCourse;
    vector<int> dupUni;
  };

  set<int> classIdCheck;
  for (auto& ch : cInfo)
    classIdCheck.insert(ch.classId);


  map<int, ClassProblem> problemsPerClass;
  for (auto [c1, c2] : duplicateClassCrs) {
    if (classIdCheck.count(c1))
      problemsPerClass[c1].dupCourse.push_back(c2);
    if (classIdCheck.count(c2))
      problemsPerClass[c2].dupCourse.push_back(c1);
  }
  for (auto [c1, c2] : duplicateClassUni) {
    if (classIdCheck.count(c1))
      problemsPerClass[c1].dupUni.push_back(c2);
    if (classIdCheck.count(c2))
      problemsPerClass[c2].dupUni.push_back(c1);
  }
  for (auto [c1, c2] : courseNbClass) {
    if (classIdCheck.count(c1))
      problemsPerClass[c1].nbCourse.push_back(c2);
    if (classIdCheck.count(c2))
      problemsPerClass[c2].nbCourse.push_back(c1);
  }

  for (auto& [cid, problem] : problemsPerClass) {
    if (!problem.dupCourse.empty()) {
      wstring cls;
      for (int clsId : problem.dupCourse) {
        pClass c = oe->getClass(clsId);
        if (c) {
          if (cls.length() > 20) {
            cls += L"\u2026";//...
            break;
          }
          if (!cls.empty())
            cls += L", ";
          cls += c->getName();
        }
      }
      problems.emplace_back(cid, L"Samma bana och starttid i X.#" + cls, InfoType::Warning);
    }
    else if (!di.allowNeighbourSameCourse && !problem.nbCourse.empty()) {
      wstring cls;
      for (int clsId : problem.nbCourse) {
        pClass c = oe->getClass(clsId);
        if (c) {
          if (cls.length() > 20) {
            cls += L"\u2026";//...
            break;
          }
          if (!cls.empty())
            cls += L", ";
          cls += c->getName();
        }
      }
      problems.emplace_back(cid, L"Samma bana på angränsande starttid: X.#" + cls, InfoType::Warning);
    }
    else if (!problem.dupUni.empty()) {
      wstring cls;
      for (int clsId : problem.dupUni) {
        pClass c = oe->getClass(clsId);
        if (c) {
          if (cls.length() > 20) {
            cls += L"\u2026";//...
            break;
          }
          if (!cls.empty())
            cls += L", ";
          cls += c->getName();
        }
      }
      problems.emplace_back(cid, L"Samma inledning och starttid i X.#" + cls, InfoType::Info);
    }
  }

  return problems;
}

void oEvent::optimizeStartOrder(vector<pair<int, wstring>>& outLines, DrawInfo& di, vector<ClassInfo>& cInfo)
{
  if (Classes.size() == 0)
    return;

  struct StartParam {
    int nControls = 1;
    int alternator = 1;
    double badness = 1000;
    int nFields = 10;
    int last = 90000000;
  };

  StartParam opt;
  bool found = false;
  int nCtrl = 1;//max(1, di.maxCommonControl-2);
  const int maxControlDiff = di.maxCommonControl < 1000 ? di.maxCommonControl : 10;
  bool checkOnlyClass = di.maxCommonControl == 1000;

  DrawOptimAlgo drawOptim(this);

  while (!found) {

    StartParam optInner;
    for (int alt = 0; alt <= 20 && !found; alt++) {
      int nFieldsOpt = di.nFieldsMax;
      int nFields = di.nFieldsMax;
      int currentLastStart = 100000;

      vector<int> startPerInterval;
      int overShoot = 0;

      while (nFields > 0) {
        drawOptim.optimizeStartOrder(di, cInfo, nFields, nCtrl, alt);
        int last = drawOptim.getLastStart();
        if (last <= currentLastStart) {
          startPerInterval = drawOptim.getNumStartPerInterval();

          overShoot = 0;
          for (size_t k = 0; k < cInfo.size(); k++) {
            const ClassInfo& ci = cInfo[k];
            if (ci.overShoot > 0) {
              overShoot = max(overShoot, ci.overShoot);
            }
          }

          currentLastStart = last;
          nFieldsOpt = nFields;
          nFields--;
        }
        else {
          break;
        }
      }

      constexpr int numSample = 4;
      int sumStarts[numSample];
      for (int j = 0; j < 4; j++) {
        int s = (j * startPerInterval.size()) / 4;
        int e = ((j + 1) * startPerInterval.size()) / 4;
        sumStarts[j] = 0;
        for (int i = s; i < e; i++)
          sumStarts[j] += startPerInterval[i];
      }

      auto minmax = std::minmax_element(sumStarts, sumStarts + numSample);
      double div = 1.0 + double(*minmax.first) / double(*minmax.second);

      //double avgShoot = double(overSum) / cInfo.size();
      double badness = overShoot == 0 ? 1 / div : (overShoot + 1) / div;

      if (badness < optInner.badness) {
        optInner.badness = badness;
        optInner.alternator = alt;
        optInner.nControls = nCtrl;

        //Find last starter
        optInner.last = currentLastStart;
        optInner.nFields = nFieldsOpt;
      }
    }

    if (optInner.last < opt.last)
      opt = optInner;

    if (opt.badness < 2.0 && !checkOnlyClass) {
      found = true;
    }

    if (!found) {
      nCtrl++;
    }

    if (nCtrl == 4 && checkOnlyClass)
      nCtrl = 1000;

    if (nCtrl > maxControlDiff) //We need some limit
      found = true;
  }

  drawOptim.optimizeStartOrder(di, cInfo, opt.nFields, opt.nControls, opt.alternator);

  outLines.emplace_back(0, L"Identifierar X unika inledningar på banorna.#" + itow(di.numDistinctInit));
  outLines.emplace_back(0, L"Största gruppen med samma inledning har X platser.#" + itow(di.numRunnerSameInitMax));
  outLines.emplace_back(0, L"Antal löpare på vanligaste banan X.#" + itow(di.numRunnerSameCourseMax));
  outLines.emplace_back(0, L"Kortast teoretiska startdjup utan krockar är X minuter.#" + itow(di.minimalStartDepth / timeConstMinute));
  outLines.emplace_back(0, L"");
  //Find last starter
  int last = opt.last;

  int laststart = 0;
  for (size_t k = 0; k < cInfo.size(); k++) {
    const ClassInfo& ci = cInfo[k];
    laststart = max(laststart, ci.firstStart + (ci.nRunners - 1) * ci.interval);
  }

  outLines.emplace_back(0, L"Faktiskt startdjup: X minuter.#" + itow(((last + 1) * di.baseInterval) / timeConstMinute));

  outLines.emplace_back(1, L"Sista start (nu tilldelad): X.#" +
    oe->getAbsTime(laststart * di.baseInterval + di.firstStart));

  outLines.emplace_back(0, L"# ");
  
  outLines.emplace_back(fontMediumPlus, L"Antal startande per intervall (inklusive redan lottade)");
  wstring str;

  auto countStarts = drawOptim.getNumStartPerInterval();
  int accRunners = 0;
  for (int j = 0; j < countStarts.size(); j++) {
    int t = di.firstStart + j * di.baseInterval;
    if (str.empty()) {
      str = L"#(" + ((t % timeConstMinute == 0) ? oe->getAbsTimeHM(t) : oe->getAbsTime(t)) + L")   ";
    }
    int nr = countStarts[j];
    wchar_t bf[20];
    swprintf_s(bf, L"%d ", nr);
    accRunners += nr;
    str += bf;
    if (j % 60 == 59) {
      if ((j + 1) < countStarts.size()) {
        str += L"  (" + itow(accRunners) + L")";
        accRunners = 0;
        outLines.emplace_back(0, str);
        str.clear();
      }
    }
    else if (j % 10 == 9)
      str += L"  ";   
  }

  if (!str.empty()) {
    int t = (countStarts.size() - 1) * di.baseInterval + di.firstStart;
    wstring tstr = ((t % timeConstMinute == 0) ? oe->getAbsTimeHM(t) : oe->getAbsTime(t));
    str += L"  (" + itow(accRunners) + L", " + tstr + L")";
    outLines.emplace_back(0, str);
  }
  /*for (int nr : countStarts) {
    wchar_t bf[20];
    swprintf_s(bf, L"%d ", nr);
    str += bf;
  }

  outLines.emplace_back(10, str);*/
  outLines.emplace_back(0, L"");
}

void oEvent::loadDrawSettings(const set<int>& classes, DrawInfo& drawInfo, vector<ClassInfo>& cInfo) const {
  drawInfo.firstStart = timeConstHour * 22;
  drawInfo.minClassInterval = timeConstHour;
  drawInfo.maxClassInterval = 1;
  drawInfo.minVacancy = 10;
  drawInfo.maxVacancy = 1;
  drawInfo.changedExtraInfo = false;
  drawInfo.changedVacancyInfo = false;
  set<int> reducedStart;
  for (set<int>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
    pClass pc = oe->getClass(*it);
    if (pc) {
      int fs = pc->getDrawFirstStart();
      int iv = pc->getDrawInterval();
      if (iv > 0 && fs > 0) {
        drawInfo.firstStart = min(drawInfo.firstStart, fs);
        drawInfo.minClassInterval = min(drawInfo.minClassInterval, iv);
        drawInfo.maxClassInterval = max(drawInfo.maxClassInterval, iv);
        drawInfo.minVacancy = min(drawInfo.minVacancy, pc->getDrawVacant());
        drawInfo.maxVacancy = max(drawInfo.maxVacancy, pc->getDrawVacant());
        reducedStart.insert(fs % iv);
      }
    }
  }

  drawInfo.baseInterval = drawInfo.minClassInterval;
  int lastStart = -1;
  for (set<int>::iterator it = reducedStart.begin(); it != reducedStart.end(); ++it) {
    if (lastStart == -1)
      lastStart = *it;
    else {
      drawInfo.baseInterval = min(drawInfo.baseInterval, *it - lastStart);
      lastStart = *it;
    }
  }

  map<int, int> runnerPerGroup;
  map<int, int> runnerPerCourse;

  cInfo.clear();
  cInfo.resize(classes.size());
  int i = 0;
  drawInfo.classes.clear();
  for (auto it = classes.begin(); it != classes.end(); ++it) {
    pClass pc = oe->getClass(*it);
    if (pc) {
      int fs = pc->getDrawFirstStart();
      int iv = pc->getDrawInterval();
      if (iv <= 0)
        iv = drawInfo.minClassInterval;
      if (fs <= 0)
        fs = drawInfo.firstStart; //Fallback

      cInfo[i].pc = pc;
      cInfo[i].classId = *it;
      cInfo[i].courseId = pc->getCourseId();
      cInfo[i].firstStartComputed = cInfo[i].firstStart = fs;
      cInfo[i].unique = pc->getCourseId();
      if (cInfo[i].unique == 0)
        cInfo[i].unique = pc->getId() * 10000;
      cInfo[i].firstStartComputed = cInfo[i].firstStart = (fs - drawInfo.firstStart) / drawInfo.baseInterval;
      cInfo[i].interval = iv / drawInfo.baseInterval;
      cInfo[i].nVacant = pc->getDrawVacant();
      cInfo[i].nExtra = pc->getDrawNumReserved();
      auto spec = pc->getDrawSpecification();
      const bool hasFixedTime = spec.count(oClass::DrawSpecified::FixedTime) != 0;
      if (spec.count(oClass::DrawSpecified::FixedInterval) || hasFixedTime)
        cInfo[i].fixedInterval = cInfo[i].interval;

      if (hasFixedTime)
        cInfo[i].hint = ClassInfo::Hint::Fixed;
      else if (spec.count(oClass::DrawSpecified::Late))
        cInfo[i].hint = ClassInfo::Hint::Late;
      else if (spec.count(oClass::DrawSpecified::Early))
        cInfo[i].hint = ClassInfo::Hint::Early;
      else
        cInfo[i].hint = ClassInfo::Hint::Default;

      cInfo[i].nExtraSpecified = spec.count(oClass::DrawSpecified::Extra) != 0;
      cInfo[i].nVacantSpecified = spec.count(oClass::DrawSpecified::Vacant) != 0;

      cInfo[i].nRunners = pc->getNumRunners(true, true, true) + cInfo[i].nVacant;

      if (cInfo[i].nRunners > 0) {
        runnerPerGroup[cInfo[i].unique] += cInfo[i].nRunners;
        runnerPerCourse[cInfo[i].courseId] += cInfo[i].nRunners;
      }
      drawInfo.classes[*it] = cInfo[i];
      i++;
    }
  }

  for (size_t k = 0; k < cInfo.size(); k++) {
    cInfo[k].nRunnersGroup = runnerPerGroup[cInfo[k].unique];
    cInfo[k].nRunnersCourse = runnerPerCourse[cInfo[k].courseId];
  }
}

int oEvent::computeMostCommonStartInterval() const {
  map<int, vector<int>> assignedStartTimes;
  for (auto& r : Runners) {
    if (!r.isRemoved() && r.startTime > 0 && r.Class) {
      if (r.tLeg > 0) {
        int st = r.Class->getStartType(r.tLeg);
        if (st == StartTypes::STChange || st == StartTypes::STPursuit)
          continue;
      }
      if (r.Card) {
        bool hasStart = false;
        for (auto& p : r.Card->punches) {
          if (p.isStart()) {
            hasStart = true;
            break;
          }
        }
        if (hasStart)
          continue;
      }
      assignedStartTimes[r.getClassId(true)].push_back(r.startTime);
    }
  }
  map<int, int> intervalCount;
  for (auto& [id, startTimes] : assignedStartTimes) {
    sort(startTimes.begin(), startTimes.end());
    for (int k = 1; k < startTimes.size(); k++) {
      int iv = startTimes[k] - startTimes[k - 1];
      if (iv < 5 * timeConstSecond || (iv % timeConstSecond) != 0)
        continue;
      iv /= timeConstSecond;
      if ((iv <= 30 && (iv % 5) == 0) || (iv <= 60 && (iv % 10) == 0) ||
        (iv <= 120 && (iv % 15) == 0) || (iv % 30) == 0) {
        ++intervalCount[iv];
      }
    }
  }

  pair<int, int> best(60 * 2, 0);
  for (auto& freq : intervalCount) {
    if (freq.second > best.second)
      best = freq;
  }

  return best.first * timeConstSecond;
}

void oEvent::drawRemaining(const set<int>& classSel, DrawMethod method, bool placeAfter)
{
  bool all = classSel.size() == 1 && *classSel.begin() == -1;
  DrawType drawType = placeAfter ? DrawType::RemainingAfter : DrawType::RemainingBefore;
  int iv = computeMostCommonStartInterval();
  for (auto& it : Classes) {
    if (it.isRemoved())
      continue;
    if (all || classSel.count(it.getId())) {
      vector<ClassDrawSpecification> spec;
      spec.emplace_back(it.getId(), 0, timeConstHour, iv, 0, VacantPosition::Mixed);
      drawList(spec, method, 1, drawType);
    }
  }
}

struct GroupInfo {
  int firstStart = 0;
  int unassigned = 0;
  int ix;
  vector<int> rPerGroup;
  vector<int> vacantPerGroup;
};

namespace {
  bool sameFamily(pRunner a, pRunner b) {
    wstring af = a->getFamilyName();
    wstring bf = b->getFamilyName();
    if (af.length() == bf.length())
      return af == bf;
    if (af.length() > bf.length())
      swap(af, bf);

    vector<wstring> bff;
    split(bf, L" -", bff);
    for (wstring& bfs : bff) {
      if (bfs == af)
        return true;
    }

    return false;
    //return af.find(bf) != wstring::npos || bf.find(af) != wstring::npos;
  }

  template<typename RND>
  void groupUnassignedRunners(vector<pRunner>& rIn,
    vector<pair<vector<pRunner>, bool>>& rGroups,
    int maxPerGroup, RND rnd) {

    map<int, vector<pRunner>> families;
    for (pRunner& r : rIn) {
      int fam = r->getDCI().getInt("Family");
      if (fam != 0) {
        families[fam].push_back(r);
        r = nullptr;
      }
    }

    map<int, vector<pRunner>> clubs;
    for (pRunner& r : rIn) {
      if (r)
        clubs[r->getClubId()].push_back(r);
    }

    // Merge families to clubs, if appropriate
    for (auto& fam : families) {
      int cid = fam.second[0]->getClubId();
      if (cid != 0 && clubs.count(cid) && int(clubs[cid].size() + fam.second.size()) < maxPerGroup) {
        clubs[cid].insert(clubs[cid].end(), fam.second.begin(), fam.second.end());
        fam.second.clear();
      }
    }

    vector<vector<pRunner>> rawGroups;
    for (auto& g : families) {
      if (g.second.size() > 0) {
        rawGroups.emplace_back();
        rawGroups.back().swap(g.second);
      }
    }

    for (auto& g : clubs) {
      if (g.second.size() > 0) {
        rawGroups.emplace_back();
        rawGroups.back().swap(g.second);
      }
    }

    shuffle(rawGroups.begin(), rawGroups.end(), rnd);

    stable_sort(rawGroups.begin(), rawGroups.end(),
      [](const vector<pRunner>& a, const vector<pRunner>& b) {return (a.size() / 4) < (b.size() / 4); });

    for (auto& g : rawGroups) {
      if (int(g.size()) <= maxPerGroup)
        rGroups.emplace_back(g, false);
      else {
        int nSplit = (g.size() + maxPerGroup - 1) / maxPerGroup;
        sort(g.begin(), g.end(), [](const pRunner& a, const pRunner& b) {
          wstring n1 = a->getFamilyName();
          wstring n2 = b->getFamilyName();
          return n1 < n2;
          });

        vector<vector<pRunner>> famGroups(1);
        for (pRunner r : g) {
          if (famGroups.back().empty())
            famGroups.back().push_back(r);
          else {
            if (!sameFamily(famGroups.back().back(), r))
              famGroups.emplace_back();
            famGroups.back().push_back(r);
          }
        }

        shuffle(famGroups.begin(), famGroups.end(), rnd);

        stable_sort(famGroups.begin(), famGroups.end(),
          [](const vector<pRunner>& a, const vector<pRunner>& b) {return (a.size() / 4) < (b.size() / 4); });

        size_t nPerGroup = g.size() / nSplit + 1;
        bool brk = false;
        while (!famGroups.empty()) {
          rGroups.emplace_back(vector<pRunner>(), brk);
          auto& dst = rGroups.back().first;
          brk = true;
          while (!famGroups.empty() && dst.size() + famGroups.back().size() <= nPerGroup) {
            dst.insert(dst.end(), famGroups.back().begin(), famGroups.back().end());
            famGroups.pop_back();
          }
        }
      }
    }
  }

  void printGroups(gdioutput& gdibase, const list<oRunner>& Runners) {
    map<int, vector<const oRunner*>> rbg;
    for (const oRunner& r : Runners) {
      rbg[r.getStartGroup(true)].push_back(&r);
    }
    gdibase.dropLine();
    gdibase.addString("", 0, "List groups");
    gdibase.dropLine();

    map<int, int> ccCount;
    for (auto rr : rbg) {
      auto& vr = rr.second;
      sort(vr.begin(), vr.end(), [](const oRunner* a, const oRunner* b) {
        if (a->getClassId(true) != b->getClassId(true))
          return a->getClassId(true) < b->getClassId(true);
        else
          return a->getClubId() < b->getClubId(); });

      gdibase.dropLine();
      gdibase.addString("", 1, "Group: " + itos(rr.first));
      int cls = -1;

      for (const oRunner* r : vr) {
        if (cls != r->getClassId(true)) {
          gdibase.dropLine();
          gdibase.addString("", 1, r->getClass(true));
          cls = r->getClassId(true);
        }
        ++ccCount[cls];
        gdibase.addString("", 0, itow(ccCount[cls]) + L": " + r->getCompleteIdentification(oRunner::IDType::OnlyThis));
      }
    }
  }
}

void oEvent::drawListStartGroups(const vector<ClassDrawSpecification>& spec,
  DrawMethod method, int pairSize, DrawType drawType,
  bool limitGroupSize,
  DrawInfo* diIn) {
  int nParallel = -1;
  if (diIn)
    nParallel = diIn->nFieldsMax;

  constexpr bool logOutput = false;
  auto& sgMap = getStartGroups(true);
  if (sgMap.empty())
    throw meosException("No start group defined");

  map<int, GroupInfo> gInfo;
  vector<pair<int, int>> orderedStartGroups;

  auto overlap = [](const StartGroupInfo& a, const StartGroupInfo& b) {
    if (a.firstStart >= b.firstStart && a.firstStart < b.lastStart)
      return true;
    if (a.lastStart > b.firstStart && a.lastStart <= b.lastStart)
      return true;
    if (b.firstStart >= a.firstStart && b.firstStart < a.lastStart)
      return true;
    if (b.lastStart > a.firstStart && b.lastStart <= a.lastStart)
      return true;
    return false;
    };

  auto formatSG = [this](int id, const StartGroupInfo& a) {
    wstring w = itow(id);
    if (!a.name.empty())
      w += L"/" + a.name;

    w += L" (" + getAbsTime(a.firstStart) + makeDash(L"-") + getAbsTime(a.lastStart) + L")";
    return w;
    };

  for (auto& sg : sgMap) {
    orderedStartGroups.emplace_back(sg.second.firstStart, sg.first);
    // Check overlaps
    for (auto& sgOther : sgMap) {
      if (sg.first == sgOther.first)
        continue;

      if (overlap(sg.second, sgOther.second)) {
        wstring s1 = formatSG(sg.first, sg.second), s2 = formatSG(sgOther.first, sgOther.second);
        throw meosException(L"Startgrupperna X och Y överlappar.#" + s1 + L"#" + s2);
      }

    }

  }
  // Order by start time
  sort(orderedStartGroups.begin(), orderedStartGroups.end());

  int fs = orderedStartGroups[0].first; // First start
  vector<pair<int, int>> rPerGroupTotal;
  map<int, int> gId2Ix;
  for (auto& sg : orderedStartGroups) {
    gId2Ix[sg.second] = rPerGroupTotal.size();
    rPerGroupTotal.emplace_back(0, sg.second);
  }

  for (size_t k = 0; k < spec.size(); k++) {
    auto& s = spec[k];
    auto& gi = gInfo[s.classID];
    gi.ix = k;
    gi.firstStart = fs;
    gi.rPerGroup.resize(sgMap.size());
  }

  vector<pRunner> unassigned;
  int total = 0;
  for (auto& r : Runners) {
    if (r.isRemoved() || r.isVacant())
      continue;
    r.tmpStartGroup = 0;
    int clsId = r.getClassId(true);
    auto res = gInfo.find(clsId);
    if (res != gInfo.end()) {
      total++;
      int id = r.getStartGroup(false);
      auto idRes = gId2Ix.find(id);
      if (idRes != gId2Ix.end()) {
        ++res->second.rPerGroup[idRes->second];
        ++rPerGroupTotal[idRes->second].first;
      }
      else {
        if (id > 0)
          throw meosException(L"Startgrupp med id X tilldelad Y finns inte.#" + itow(id) +
            L"#" + r.getCompleteIdentification(oRunner::IDType::OnlyThis));
        ++res->second.unassigned;
        unassigned.push_back(&r);
      }
    }
  }

  vector<pair<vector<pRunner>, bool>> uaGroups;
  unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
  auto rnd = std::default_random_engine(seed);

  int maxPerGroup = Runners.size();
  if (limitGroupSize)
    maxPerGroup = max(int(Runners.size() / sgMap.size()) / 4, 4);

  groupUnassignedRunners(unassigned, uaGroups, maxPerGroup, rnd);

  int nPerGroupAvg = (total * 9) / (sgMap.size() * 10);
  shuffle(rPerGroupTotal.begin(), rPerGroupTotal.end(), rnd);

  // Assign to groups
  while (!uaGroups.empty()) {
    stable_sort(rPerGroupTotal.begin(), rPerGroupTotal.end(),
      [](const pair<int, int>& a, const pair<int, int>& b) {return (a.first / 4) < (b.first / 4); });

    // Setup map to next start group
    map<int, int> nextGroup;
    auto getNextGroup = [&](int ix) {
      if (nextGroup.empty()) {
        for (size_t k = 0; k < rPerGroupTotal.size(); k++) {
          int srcId = rPerGroupTotal[k].second;
          for (size_t j = 0; j < orderedStartGroups.size(); j++) {
            if (orderedStartGroups[j].second == srcId) {
              int nextGrpId;
              if (j + 1 < orderedStartGroups.size())
                nextGrpId = orderedStartGroups[j + 1].second;
              else
                nextGrpId = orderedStartGroups[0].second;

              for (size_t kk = 0; kk < rPerGroupTotal.size(); kk++) {
                if (rPerGroupTotal[kk].second == nextGrpId) {
                  nextGroup[k] = kk;
                  break;
                }
              }
              break;
            }
          }
        }
      }
      return nextGroup[ix];
      };

    int nextGroupToUse = -1;
    for (size_t k = 0; k < rPerGroupTotal.size(); k++) {
      if (nextGroupToUse == -1)
        nextGroupToUse = k;

      int& nGroup = rPerGroupTotal[nextGroupToUse].first;
      int groupId = rPerGroupTotal[nextGroupToUse].second;
      int currentGroup = nextGroupToUse;
      nextGroupToUse = -1;

      if (logOutput) {
        gdibase.dropLine();
        gdibase.addString("", 1, "Group: " + itos(groupId) + " (" + itos(nGroup) + ")");
      }

      while (nGroup <= nPerGroupAvg && !uaGroups.empty()) {
        for (pRunner ua : uaGroups.back().first) {
          ua->tmpStartGroup = groupId;
          auto& gi = gInfo[ua->getClassId(true)];
          int j = gId2Ix[groupId];
          ++gi.rPerGroup[j];
          nGroup++;
        }
        bool skip = uaGroups.back().second;
        uaGroups.pop_back();
        if (skip) {
          nextGroupToUse = getNextGroup(currentGroup);
          break; // Assign to next group (same club)
        }
      }
    }
    nPerGroupAvg++; // Ensure convergance
  }

  //  vacantPerGroup
  for (size_t k = 0; k < spec.size(); k++) {
    auto& gi = gInfo[spec[k].classID];
    gi.vacantPerGroup.resize(sgMap.size());
    for (int j = 0; j < spec[k].vacances; j++)
      ++gi.vacantPerGroup[GetRandomNumber(sgMap.size())];
  }

  if (logOutput)
    printGroups(gdibase, Runners);

  map<int, int> rPerGroup;
  // Ensure not too many competitors in same class per groups
  vector<map<int, map<int, int>>> countClassGroupClub(spec.size());
  vector<pRunner> rl;
  map<int, double> classFractions;
  map<pair<int, int>, double> clubFractions;
  int rTot = 0;
  for (size_t k = 0; k < spec.size(); k++) {
    auto& countGroupClub = countClassGroupClub[k];
    int cls = spec[k].classID;
    getRunners(cls, 0, rl, false);
    classFractions[cls] = (double)rl.size();
    rTot += rl.size();
    for (pRunner r : rl) {
      int gid = r->getStartGroup(true);
      ++rPerGroup[gid];
      int club = r->getClubId();
      if (club != 0) {
        ++clubFractions[make_pair(club, cls)];
        ++countGroupClub[gid][club]; // Count per club
      }
      ++countGroupClub[gid][0]; // Count all
    }
  }
  double q = 1.0 / rTot;
  for (auto& e : classFractions)
    e.second *= q;

  for (auto& e : clubFractions)
    e.second *= (q / classFractions[e.first.second]);

  vector<tuple<int, int, int, int>> moveFromGroupClassClub;
  // For each start group, count class members in each class and per club
  vector<pair<int, int>> numberClub;
  for (size_t j = 0; j < orderedStartGroups.size(); j++) {
    auto& g = orderedStartGroups[j];
    int groupId = g.second;

    for (size_t k = 0; k < spec.size(); k++) {
      auto& countGroupClub = countClassGroupClub[k];
      int cls = spec[k].classID;
      const int nTotal = countGroupClub[groupId][0];
      int clubSwitch = 0; // 0 any club, or specified club id.
      int numNeedSwitch = 0;
      if (nTotal > 3) {
        numberClub.clear();
        for (auto& clubCount : countGroupClub[groupId]) {
          if (clubCount.first != 0)
            numberClub.emplace_back(clubCount.second, clubCount.first);
        }
        if (numberClub.empty())
          continue;

        sort(numberClub.begin(), numberClub.end());
        int clbId = numberClub.back().second;
        int limit = max<int>(nTotal / 2, int(nTotal * clubFractions[make_pair(clbId, cls)]));
        int limitTot = int(rPerGroup[groupId] * max(0.3, classFractions[cls]));
        if (numberClub.size() == 1 || numberClub.back().first > limit) {
          numNeedSwitch = numberClub.back().first - limit;
          clubSwitch = clbId;
        }
        else if (nTotal > limitTot) {
          // Move from any club
          numNeedSwitch = nTotal - limitTot;
          clubSwitch = 0;
        }
      }

      if (numNeedSwitch > 0) {
        moveFromGroupClassClub.emplace_back(groupId, cls, clubSwitch, numNeedSwitch);
      }
    }
  }

  if (moveFromGroupClassClub.size() > 0 && limitGroupSize) {
    vector<vector<pRunner>> runnersPerGroup(orderedStartGroups.size());
    map<int, int> groupId2Ix;
    for (size_t j = 0; j < orderedStartGroups.size(); j++)
      groupId2Ix[orderedStartGroups[j].second] = j;

    for (size_t k = 0; k < spec.size(); k++) {
      getRunners(spec[k].classID, 0, rl, false);
      for (pRunner r : rl) {
        runnersPerGroup[groupId2Ix[r->tmpStartGroup]].push_back(r);
      }
    }

    for (size_t j = 0; j < orderedStartGroups.size(); j++) {
      shuffle(runnersPerGroup[j].begin(), runnersPerGroup[j].end(), rnd);
      stable_sort(runnersPerGroup[j].begin(), runnersPerGroup[j].end(), [](pRunner a, pRunner b) {
        int specA = a->getStartGroup(false);
        int specB = b->getStartGroup(false);
        if (specA != specB)
          return specA < specB;
        return a->getEntryDate(false) > b->getEntryDate(false);
        });
    }

    map<int, map<pair<int, int>, int>> classClubCountByGroup;
    for (auto ms : moveFromGroupClassClub) {
      int groupId = std::get<0>(ms);
      int clsId = std::get<1>(ms);
      int clubId = std::get<2>(ms);
      int cnt = std::get<3>(ms);
      classClubCountByGroup[groupId][make_pair(clsId, clubId)] = cnt;
    }

    for (size_t j = 0; j < orderedStartGroups.size(); j++) {
      int thisGroupId = orderedStartGroups[j].second;
      auto& classClubCount = classClubCountByGroup[thisGroupId];
      for (pRunner& r : runnersPerGroup[j]) {
        if (!r)
          continue;
        int cls = r->getClassId(true);
        int club = r->getClubId();
        auto res = classClubCount.find(make_pair(cls, club));
        int type = 0;
        if (res != classClubCount.end()) {
          if (res->second > 0) {
            --res->second;
            type = 1;
          }
        }
        else {
          res = classClubCount.find(make_pair(cls, 0));
          if (res != classClubCount.end()) {
            if (res->second > 0) {
              --res->second;
              type = 2;
            }
          }
        }
        if (type == 0)
          continue; //do not move

        constexpr int done = 100;
        for (int iter = 0; iter < 5; iter++) {
          int nextG = -1;
          if ((iter & 1) == 0)
            nextG = j + (iter / 2 + 1);
          else
            nextG = j - (iter / 2 + 1);

          if (size_t(nextG) >= runnersPerGroup.size())
            continue;

          int nextGroupId = orderedStartGroups[nextG].second;
          auto& nextClassClubCount = classClubCountByGroup[nextGroupId];

          if (type == 1 && nextClassClubCount.find(make_pair(cls, club)) != nextClassClubCount.end())
            continue;

          for (pRunner& rr : runnersPerGroup[nextG]) {
            if (rr) {
              if (type == 1 && rr->getClassId(true) == cls && rr->getClubId() != club) {
                rr->tmpStartGroup = orderedStartGroups[j].second;
                r->tmpStartGroup = orderedStartGroups[nextG].second;
                rr = nullptr;
                r = nullptr;
                iter = done;
                break;
              }
              else if (type == 2 && rr->getClassId(true) != cls) {
                rr->tmpStartGroup = orderedStartGroups[j].second;
                r->tmpStartGroup = orderedStartGroups[nextG].second;
                rr = nullptr;
                r = nullptr;
                iter = done;
                break;
              }
            }
          }
        }

      }
    }
  }

  if (logOutput) {
    gdibase.addString("", 0, "Reordered groups");
    printGroups(gdibase, Runners);
  }

  if (spec.size() == 1) {
    for (size_t j = 0; j < orderedStartGroups.size(); j++) {
      auto& sg = orderedStartGroups[j];
      int groupId = sg.second;
      int firstStart = getStartGroup(groupId).firstStart;

      vector<ClassDrawSpecification> specLoc = spec;
      for (size_t k = 0; k < specLoc.size(); k++) {
        auto& gi = gInfo[specLoc[k].classID];
        specLoc[k].startGroup = groupId;
        specLoc[k].firstStart = max(gi.firstStart, firstStart);
        specLoc[k].vacances = gi.vacantPerGroup[j];
        gi.firstStart = specLoc[k].firstStart + specLoc[k].interval * (specLoc[k].vacances + gi.rPerGroup[j]);
      }
      drawList(specLoc, method, pairSize, drawType);
    }
  }
  else {
    int leg = spec[0].leg;
    VacantPosition vp = VacantPosition::Mixed;
    DrawInfo di;
    di.baseInterval = timeConstMinute;
    di.allowNeighbourSameCourse = true;

    di.extraFactor = 0;
    di.minClassInterval = di.baseInterval * 2;
    di.maxClassInterval = di.baseInterval * 8;

    di.minVacancy = 1;
    di.maxVacancy = 100;
    di.vacancyFactor = 0;
    if (diIn) {
      di.baseInterval = diIn->baseInterval;
      di.minClassInterval = diIn->minClassInterval;
      di.maxCommonControl = diIn->maxCommonControl;
      di.allowNeighbourSameCourse = diIn->allowNeighbourSameCourse;
    }

    // Update runner per group/class counters
    for (auto& gi : gInfo) {
      fill(gi.second.rPerGroup.begin(), gi.second.rPerGroup.end(), 0);
    }

    rPerGroup.clear();
    for (size_t k = 0; k < spec.size(); k++) {
      getRunners(spec[k].classID, 0, rl, false);
      auto& gi = gInfo[spec[k].classID];
      for (pRunner r : rl) {
        r->setStartTime(0, true, oBase::ChangeType::Update, false);
        int gid = r->getStartGroup(true);
        ++rPerGroup[gid];
        ++gi.rPerGroup[gId2Ix[gid]];
      }
    }

    map<int, int> rPerGroup;
    for (auto& gt : rPerGroupTotal)
      rPerGroup[gt.second] = gt.first;

    for (size_t j = 0; j < orderedStartGroups.size(); j++) {
      auto& sg = orderedStartGroups[j];
      int groupId = sg.second;

      int firstStart = getStartGroup(groupId).firstStart;

      int nspos = (oe->getStartGroup(groupId).lastStart - firstStart) / di.baseInterval;
      int optimalParallel = rPerGroup[groupId] / nspos;

      if (nParallel <= 0)
        di.nFieldsMax = max(3, min(optimalParallel + 2, 100));
      else
        di.nFieldsMax = nParallel;

      di.firstStart = firstStart;
      di.changedVacancyInfo = false;

      vector<ClassInfo> cInfo;
      vector<pair<int, wstring>> outLines;
      di.vacancyFactor = 0;
      auto group = sgMap.find(groupId);
      int length = max(300, group->second.lastStart - group->second.firstStart);
      int slots = length / di.baseInterval;
      di.classes.clear();
      for (size_t k = 0; k < spec.size(); k++) {
        auto& gi = gInfo[spec[k].classID];
        int rClassGroup = gi.rPerGroup[j];
        if (rClassGroup == 0)
          continue;

        di.classes[spec[k].classID] = ClassInfo(getClass(spec[k].classID));
        ClassInfo& ci = di.classes[spec[k].classID];
        ci.startGroupId = groupId;
        if (gi.firstStart > firstStart) {
          // Handled by distributor
          // ci.firstStart = (gi.firstStart - firstStart) / di.baseInterval;
          // ci.hasFixedTime = true;
        }
        ci.nVacant = gi.vacantPerGroup[j];
        ci.nVacantSpecified = true;
        ci.nExtraSpecified = true;

        if (rClassGroup == 1)
          ci.fixedInterval = 4;
        else {
          int q = slots / (rClassGroup - 1);
          if (q >= 12)
            ci.fixedInterval = 8;
          else if (q >= 6)
            ci.fixedInterval = 4;
          else
            ci.fixedInterval = 2;
        }
        if (ci.fixedInterval > 2)
          ci.interval = ci.fixedInterval;
        else
          ci.interval = spec[k].interval / di.baseInterval;
      }

      if (logOutput) {
        gdibase.dropLine();
        gdibase.addString("", 1, "Behandlar grupp: " + itos(groupId));
      }

      optimizeStartOrder(outLines, di, cInfo);

      if (logOutput) {
        for (auto& ol : outLines) {
          gdibase.addString("", ol.first, ol.second);
        }
      }

      int laststart = 0;
      for (size_t k = 0; k < cInfo.size(); k++) {
        const ClassInfo& ci = cInfo[k];
        laststart = max(laststart, ci.firstStart + ci.nRunners * ci.interval);
      }

      //gdi.addStringUT(1, lang.tl("Sista start (nu tilldelad)") + L": " +
      //                getAbsTime((laststart)*di.baseInterval + di.firstStart));


      for (size_t k = 0; k < cInfo.size(); k++) {
        ClassInfo& ci = cInfo[k];
        vector<ClassDrawSpecification> specx;
        specx.emplace_back(ci.classId, leg,
          di.firstStart + di.baseInterval * ci.firstStart,
          di.baseInterval * ci.interval, ci.nVacant, vp);

        auto& gi = gInfo[cInfo[k].classId];
        gi.firstStart = specx[0].firstStart + specx[0].interval * (specx[0].vacances + gi.rPerGroup[j]);

        specx.back().startGroup = groupId;
        drawList(specx, method, pairSize, drawType);
      }
    }
  }
}

void oEvent::drawList(const vector<ClassDrawSpecification>& spec,
  DrawMethod method, int pairSize, DrawType drawType) {

  autoSynchronizeLists(false);
  assert(pairSize > 0);
  oRunnerList::iterator it;

  int VacantClubId = getVacantClub(false);
  map<int, int> clsId2Ix;
  set<int> clsIdClearVac;

  const bool multiDay = hasPrevStage();

  for (size_t k = 0; k < spec.size(); k++) {
    pClass pc = getClass(spec[k].classID);

    if (!pc)
      throw std::exception("Klass saknas");

    if (spec[k].vacances > 0 && pc->getClassType() == oClassRelay)
      throw std::exception("Vakanser stöds ej i stafett.");

    if (spec[k].vacances > 0 && (spec[k].leg > 0 || pc->getParentClass()))
      throw std::exception("Det går endast att sätta in vakanser på sträcka 1.");

    if (size_t(spec[k].leg) < pc->legInfo.size()) {
      pc->setStartType(spec[k].leg, STDrawn, true); //Automatically change start method
    }
    else if (spec[k].leg == -1) {
      for (size_t j = 0; j < pc->legInfo.size(); j++)
        pc->setStartType(j, STDrawn, true); //Automatically change start method
    }
    pc->synchronize(true);
    clsId2Ix[spec[k].classID] = k;
    if (!multiDay && spec[k].leg == 0 && pc->getParentClass() == 0)
      clsIdClearVac.insert(spec[k].classID);
  }

  vector<pRunner> runners;
  runners.reserve(Runners.size());

  if (drawType == DrawType::DrawAll) {

    if (!clsIdClearVac.empty()) {
      //Only remove vacances on leg 0.
      vector<int> toRemove;
      //Remove old vacances
      for (it = Runners.begin(); it != Runners.end(); ++it) {
        if (clsIdClearVac.count(it->getClassId(true))) {
          if (it->isRemoved())
            continue;
          if (it->tInTeam)
            continue; // Cannot remove team runners
          int k = clsId2Ix.find(it->getClassId(true))->second;
          if (spec[k].startGroup > 0 &&
            it->getStartGroup(true) > 0 &&
            it->getStartGroup(true) != spec[k].startGroup)
            continue;
          if (it->getClubId() == VacantClubId) {
            toRemove.push_back(it->getId());
          }
        }
      }

      removeRunner(toRemove);
      toRemove.clear();
      //loop over specs, check clsIdClearVac...

      for (size_t k = 0; k < spec.size(); k++) {
        if (!clsIdClearVac.count(spec[k].classID))
          continue;
        for (int i = 0; i < spec[k].vacances; i++) {
          oe->addRunnerVacant(spec[k].classID)->setStartGroup(spec[k].startGroup);
        }
      }
    }

    for (it = Runners.begin(); it != Runners.end(); ++it) {
      int cid = it->getClassId(true);
      if (!it->isRemoved() && clsId2Ix.count(cid)) {
        if (it->getStatus() == StatusNotCompetiting || it->getStatus() == StatusCANCEL)
          continue;
        int ix = clsId2Ix[cid];
        if (spec[ix].startGroup != 0 && it->getStartGroup(true) != spec[ix].startGroup)
          continue;

        if (it->legToRun() == spec[ix].leg || spec[ix].leg == -1 || it->getClassId(false) != cid) {
          runners.push_back(&*it);
          spec[ix].ntimes++;
        }
      }
    }
  }
  else {
    // Find first/last start in class and interval:
    vector<int> first(spec.size(), 7 * 24 * timeConstHour);
    vector<int> last(spec.size(), 0);
    set<int> cinterval;
    int baseInterval = 10 * timeConstMinute;

    for (it = Runners.begin(); it != Runners.end(); ++it) {
      if (!it->isRemoved() && clsId2Ix.count(it->getClassId(true))) {
        if (it->getStatus() == StatusNotCompetiting || it->getStatus() == StatusCANCEL)
          continue;

        int st = it->getStartTime();
        int ix = clsId2Ix[it->getClassId(false)];

        if (st > 0) {
          first[ix] = min(first[ix], st);
          last[ix] = max(last[ix], st);
          cinterval.insert(st);
        }
        else {
          spec[ix].ntimes++;
          runners.push_back(&*it);
        }
      }
    }

    // Find start interval
    int t = 0;
    if (cinterval.size() == 1)
      baseInterval = spec[0].interval;

    for (set<int>::iterator sit = cinterval.begin(); sit != cinterval.end(); ++sit) {
      if ((*sit - t) > 0)
        baseInterval = min(baseInterval, (*sit - t));
      t = *sit;
    }

    for (size_t k = 0; k < spec.size(); k++) {
      if (drawType == DrawType::RemainingBefore)
        spec[k].firstStart = first[k] - runners.size() * baseInterval;
      else
        spec[k].firstStart = last[k] + baseInterval;

      spec[k].interval = baseInterval;

      if (last[k] == 0 || spec[k].firstStart <= 0 || baseInterval == 10 * timeConstMinute) {
        // Fallback if incorrect specification.
        spec[k].firstStart = timeConstHour;
        spec[k].interval = 2 * timeConstMinute;
      }
    }
  }

  if (runners.empty())
    return;

  vector<int> stimes(runners.size());
  int nr = 0;
  for (size_t k = 0; k < spec.size(); k++) {
    for (int i = 0; i < spec[k].ntimes; i++) {
      int kx = i / pairSize;
      stimes[nr++] = spec[k].firstStart + spec[k].interval * kx;
    }
  }

  if (spec.size() > 1)
    sort(stimes.begin(), stimes.end());

  if (gdibase.isTest())
    InitRanom(0, 0);


  vector<pRunner> vacant;
  VacantPosition vp = spec[0].vacantPosition;
  if (vp != VacantPosition::Mixed) {
    // Move vacants to a dedicated container
    for (size_t k = 0; k < runners.size(); k++) {
      if (runners[k]->isVacant()) {
        vacant.push_back(runners[k]);
        std::swap(runners[k], runners.back());
        runners.pop_back();
        k--;
      }
    }
  }

  switch (method) {
  case DrawMethod::SOFT:
    drawSOFTMethod(runners, true);
    break;
  case DrawMethod::MeOS:
    drawMeOSMethod(runners);
    break;
  case DrawMethod::Random:
  {
    vector<int> pv(runners.size());
    for (size_t k = 0; k < pv.size(); k++)
      pv[k] = k;
    permute(pv);
    vector<pRunner> r2(runners.size());
    for (size_t k = 0; k < pv.size(); k++)
      r2[k] = runners[pv[k]];
    r2.swap(runners);
  }
  break;
  default:
    throw 0;
  }

  if (vp == VacantPosition::First) {
    runners.insert(runners.begin(), vacant.begin(), vacant.end());
  }
  else if (vp == VacantPosition::Last) {
    runners.insert(runners.end(), vacant.begin(), vacant.end());
  }

  int minStartNo = Runners.size();
  vector<pair<int, int>> newStartNo;
  for (unsigned k = 0; k < stimes.size(); k++) {
    runners[k]->setStartTime(stimes[k], true, ChangeType::Update, false);
    runners[k]->synchronize();
    minStartNo = min(minStartNo, runners[k]->getStartNo());
    newStartNo.emplace_back(stimes[k], k);
  }
  /*
  gdibase.dropLine();
  gdibase.addString("", 1, L"Draw: " + oe->getClass(spec[0].classID)->getName());
  for (unsigned k = 0; k < stimes.size(); k++) {
    gdibase.addString("", 0, runners[k]->getCompleteIdentification() + L"  " + runners[k]->getStartTimeS());
  }
  */
  sort(newStartNo.begin(), newStartNo.end());
  //CurrentSortOrder = SortByStartTime;
  //sort(runners.begin(), runners.end());

  if (minStartNo == 0)
    minStartNo = nextFreeStartNo + 1;

  for (size_t k = 0; k < runners.size(); k++) {
    pClass pCls = runners[k]->getClassRef(true);
    if (pCls && pCls->lockedForking() || runners[k]->getLegNumber() > 0)
      continue;
    runners[k]->updateStartNo(newStartNo[k].second + minStartNo);
  }

  nextFreeStartNo = max<int>(nextFreeStartNo, minStartNo + stimes.size());
}

void oEvent::drawListClumped(int ClassID, int FirstStart, int Interval, int Vacances)
{
  pClass pc = getClass(ClassID);

  if (!pc)
    throw std::exception("Klass saknas");

  if (Vacances > 0 && pc->getClassType() != oClassIndividual)
    throw std::exception("Lottningsmetoden stöds ej i den här klassen.");

  oRunnerList::iterator it;
  int nRunners = 0;

  autoSynchronizeLists(false);

  while (Vacances > 0) {
    addRunnerVacant(ClassID);
    Vacances--;
  }

  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->Class && it->Class->Id == ClassID && !it->isRemoved()) nRunners++;
  }
  if (nRunners == 0) return;

  int* stimes = new int[nRunners];


  //Number of start groups
  //int ngroups=(nRunners/5)
  int ginterval;

  if (nRunners >= Interval)
    ginterval = 10 * timeConstSecond;
  else if (Interval / nRunners > 60 * timeConstSecond) {
    ginterval = 40 * timeConstSecond;
  }
  else if (Interval / nRunners > 30 * timeConstSecond) {
    ginterval = 20 * timeConstSecond;
  }
  else if (Interval / nRunners > 20 * timeConstSecond) {
    ginterval = 15 * timeConstSecond;
  }
  else if (Interval / nRunners > 10 * timeConstSecond) {
    ginterval = 12 * timeConstSecond;
  }
  else ginterval = 10 * timeConstSecond;

  int nGroups = Interval / ginterval + 1; //15 s. per interval.
  nGroups = min(nGroups, 2 * nRunners + 1);
  int k;

  if (nGroups > 0) {

    int MaxRunnersGroup = max((2 * nRunners) / nGroups, 4) + GetRandomNumber(2);
    vector<int> sgroups(nGroups);

    for (k = 0; k < nGroups; k++)
      sgroups[k] = FirstStart + ((ginterval * k + 2) / 5) * 5;

    if (nGroups > 5) {
      //Remove second group...
      sgroups[1] = sgroups[nGroups - 1];
      nGroups--;
    }

    if (nGroups > 9 && ginterval < 60 && (GetRandomBit() || GetRandomBit() || GetRandomBit())) {
      //Remove third group...
      sgroups[2] = sgroups[nGroups - 1];
      nGroups--;

      if (nGroups > 13 && ginterval < 30 && (GetRandomBit() || GetRandomBit() || GetRandomBit())) {
        //Remove third group...
        sgroups[3] = sgroups[nGroups - 1];
        nGroups--;


        int ng = 4; //Max two minutes pause
        while (nGroups > 10 && (nRunners / nGroups) < MaxRunnersGroup && ng < 8 && (GetRandomBit() || GetRandomBit())) {
          //Remove several groups...
          sgroups[ng] = sgroups[nGroups - 1];
          nGroups--;
          ng++;
        }
      }
    }

    //Permute some of the groups (not first and last group = group 0 and 1)
    if (nGroups > 5) {
      permute(sgroups.data() + 2, nGroups - 2);

      //Remove some random groups (except first and last = group 0 and 1).
      for (k = 2; k < nGroups; k++) {
        if ((nRunners / nGroups) < MaxRunnersGroup && nGroups > 5) {
          sgroups[k] = sgroups[nGroups - 1];
          nGroups--;
        }
      }
    }

    //Premute all groups;
    permute(sgroups.data(), nGroups);

    vector<int> counters(nGroups);

    stimes[0] = FirstStart;
    stimes[1] = FirstStart + Interval;

    for (k = 2; k < nRunners; k++) {
      int g = GetRandomNumber(nGroups);

      if (counters[g] <= 2 && GetRandomBit()) {
        //Prefer already large groups
        g = (g + 1) % nGroups;
      }

      if (counters[g] > MaxRunnersGroup) {
        g = (g + 3) % nGroups;
      }

      if (sgroups[g] == FirstStart) {
        //Avoid first start
        if (GetRandomBit() || GetRandomBit())
          g = (g + 1) % nGroups;
      }

      if (counters[g] > MaxRunnersGroup) {
        g = (g + 2) % nGroups;
      }

      if (counters[g] > MaxRunnersGroup) {
        g = (g + 2) % nGroups;
      }

      stimes[k] = sgroups[g];
      counters[g]++;
    }
  }
  else {
    for (k = 0; k < nRunners; k++) stimes[k] = FirstStart;
  }


  permute(stimes, nRunners);

  k = 0;

  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->Class && it->Class->Id == ClassID && !it->isRemoved()) {
      it->setStartTime(stimes[k++], true, oBase::ChangeType::Update, false);
      it->StartNo = k;
      it->synchronize();
    }
  }
  reCalculateLeaderTimes(ClassID);

  delete[] stimes;
}

void oEvent::automaticDrawAll(gdioutput& gdi,
  const wstring& firstStart,
  const wstring& minIntervall,
  const wstring& vacances,
  VacantPosition vp,
  bool lateBefore,
  bool allowNeighbourSameCourse,
  DrawMethod method,
  int pairSize) {
  gdi.refresh();
  const int leg = 0;
  const double extraFactor = 0.0;
  int drawn = 0;

  int baseInterval = convertAbsoluteTimeMS(minIntervall) / 2;

  if (baseInterval == 0) {
    gdi.fillDown();
    int iFirstStart = getRelativeTime(firstStart);

    if (iFirstStart > 0)
      gdi.addString("", 1, "Gemensam start");
    else {
      gdi.addString("", 1, "Nollställer starttider");
      iFirstStart = 0;
    }
    gdi.refreshFast();
    gdi.dropLine();
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (it->isRemoved())
        continue;
      vector<ClassDrawSpecification> spec;
      spec.emplace_back(it->getId(), 0, iFirstStart, 0, 0, vp);
      drawList(spec, DrawMethod::Random, 1, DrawType::DrawAll);
    }
    return;
  }

  if (baseInterval<timeConstSecond || baseInterval>timeConstHour)
    throw std::exception("Felaktigt tidsformat för intervall");

  int iFirstStart = getRelativeTime(firstStart);

  if (iFirstStart <= 0)
    throw std::exception("Felaktigt tidsformat för första start");

  double vacancy = _wtof(vacances.c_str()) / 100;

  gdi.fillDown();
  gdi.addString("", 1, "Automatisk lottning").setColor(colorGreen);
  gdi.addString("", 0, "Inspekterar klasser...");
  gdi.refreshFast();

  set<int> notDrawn;
  getNotDrawnClasses(notDrawn, false);

  set<int> needsCompletion;
  getNotDrawnClasses(needsCompletion, true);

  for (set<int>::iterator it = notDrawn.begin(); it != notDrawn.end(); ++it)
    needsCompletion.erase(*it);

  //Start with not drawn classes
  map<wstring, int> starts;
  map<pClass, int> runnersPerClass;

  // Count number of runners per start
  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip())
      continue;
    if (it->tLeg != leg)
      continue;
    if (it->isVacant() && notDrawn.count(it->getClassId(false)) == 1)
      continue;
    pClass pc = it->Class;

    if (pc && pc->hasFreeStart())
      continue;

    if (pc)
      ++starts[pc->getStart()];

    ++runnersPerClass[pc];
  }

  while (!starts.empty()) {
    // Select smallest start
    int runnersStart = Runners.size() + 1;
    wstring start;
    for (map<wstring, int>::iterator it = starts.begin(); it != starts.end(); ++it) {
      if (runnersStart > it->second) {
        start = it->first;
        runnersStart = it->second;
      }
    }
    starts.erase(start);

    // Estimate parameters for start
    DrawInfo di;
    int maxRunners = 0;

    // Find largest class in start;
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (it->getStart() != start)
        continue;
      if (it->hasFreeStart() || it->hasRequestStart())
        continue;

      maxRunners = max(maxRunners, runnersPerClass[&*it]);
    }

    if (maxRunners == 0)
      continue;

    if (getStartGroups(true).size() == 0) {

      int maxParallell = 15;

      if (runnersStart < 100)
        maxParallell = 4;
      else if (runnersStart < 300)
        maxParallell = 6;
      else if (runnersStart < 700)
        maxParallell = 10;
      else if (runnersStart < 1000)
        maxParallell = 12;
      else
        maxParallell = 15;

      int optimalParallel = runnersStart / (maxRunners * 2); // Min is every second interval

      di.nFieldsMax = max(3, min(optimalParallel + 2, 15));
      di.baseInterval = baseInterval;
      di.extraFactor = extraFactor;
      di.firstStart = iFirstStart;
      di.minClassInterval = baseInterval * 2;
      di.maxClassInterval = di.minClassInterval;

      di.minVacancy = 1;
      di.maxVacancy = 100;
      di.vacancyFactor = vacancy;
      di.allowNeighbourSameCourse = allowNeighbourSameCourse;

      di.startName = start;

      for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
        if (it->getStart() != start)
          continue;
        if (notDrawn.count(it->getId()) == 0)
          continue; // Only not drawn classes
        if (it->hasFreeStart() || it->hasRequestStart())
          continue;

        di.classes[it->getId()] = ClassInfo(&*it);
      }

      if (di.classes.size() == 0)
        continue;

      gdi.dropLine();
      gdi.addStringUT(1, lang.tl(L"Optimerar startfördelning ") + start);
      gdi.refreshFast();
      gdi.dropLine();
      vector<ClassInfo> cInfo;
      vector<pair<int, wstring>> outLines;
      optimizeStartOrder(outLines, di, cInfo);
      for (auto& ol : outLines)
        gdi.addString("", ol.first, ol.second);

      int laststart = 0;
      for (size_t k = 0; k < cInfo.size(); k++) {
        const ClassInfo& ci = cInfo[k];
        laststart = max(laststart, ci.firstStart + ci.nRunners * ci.interval);
      }

      gdi.addStringUT(1, lang.tl("Sista start (nu tilldelad)") + L": " +
        getAbsTime((laststart)*di.baseInterval + di.firstStart));
      gdi.dropLine();
      gdi.refreshFast();

      for (size_t k = 0; k < cInfo.size(); k++) {
        const ClassInfo& ci = cInfo[k];

        if (getClass(ci.classId)->getClassType() == oClassRelay) {
          gdi.addString("", 0, L"Hoppar över stafettklass: X#" +
            getClass(ci.classId)->getName()).setColor(colorRed);
          continue;
        }

        gdi.addString("", 0, L"Lottar: X#" + getClass(ci.classId)->getName());
        vector<ClassDrawSpecification> spec;
        spec.emplace_back(ci.classId, leg,
          di.firstStart + di.baseInterval * ci.firstStart,
          di.baseInterval * ci.interval, ci.nVacant, vp);

        drawList(spec, method, pairSize, DrawType::DrawAll);
        gdi.scrollToBottom();
        gdi.refreshFast();
        drawn++;
      }
    }
    else {
      vector<ClassDrawSpecification> spec;
      for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
        if (it->getStart() != start)
          continue;
        if (notDrawn.count(it->getId()) == 0)
          continue; // Only not drawn classes
        if (it->hasFreeStart() || it->hasRequestStart())
          continue;
        //int classID, int leg, int firstStart, int interval, int vacances, oEvent::VacantPosition vp)
        spec.emplace_back(it->getId(), 0, 0, 120, 1, VacantPosition::Mixed);
        drawn++;
      }

      if (spec.size() == 0)
        continue;
      try {
        drawListStartGroups(spec, method, pairSize, DrawType::DrawAll);
      }
      catch (meosException& ex) {
        gdi.addString("", 1, ex.wwhat()).setColor(colorRed);
        // Relay classes?
        gdi.dropLine();
        gdi.refreshFast();
        return;
      }
    }
  }

  // Classes that need completion
  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (needsCompletion.count(it->getId()) == 0)
      continue;
    if (it->hasFreeStart() || it->hasRequestStart())
      continue;

    gdi.addStringUT(0, lang.tl(L"Lottar efteranmälda: ") + it->getName());

    vector<ClassDrawSpecification> spec;
    spec.emplace_back(it->getId(), leg, 0, 0, 0, vp);
    drawList(spec, method, 1, lateBefore ? DrawType::RemainingBefore : DrawType::RemainingAfter);

    gdi.scrollToBottom();
    gdi.refreshFast();
    drawn++;
  }

  gdi.dropLine();

  if (drawn == 0)
    gdi.addString("", 1, "Klart: inga klasser behövde lottas.").setColor(colorGreen);
  else
    gdi.addString("", 1, "Klart: alla klasser lottade.").setColor(colorGreen);
  // Relay classes?
  gdi.dropLine();
  gdi.refreshFast();
}

void oEvent::drawPersuitList(int classId, int firstTime, int restartTime,
  int maxTime, int interval,
  int pairSize, bool reverse, double scale) {
  if (classId <= 0)
    return;

  pClass pc = getClass(classId);

  if (!pc)
    throw std::exception("Klass saknas");

  const int leg = 0;
  if (size_t(leg) < pc->legInfo.size()) {
    pc->legInfo[leg].startMethod = STDrawn; //Automatically change start method
  }

  vector<pRunner> trunner;
  getRunners(classId, 0, trunner);

  vector<pRunner> runner;
  runner.reserve(trunner.size());
  for (size_t k = 0; k < trunner.size(); k++) // Only treat specified leg
    if (trunner[k]->tLeg == leg)
      runner.push_back(trunner[k]);

  if (runner.empty())
    return;

  // Make sure patrol members use the same time
  vector<int> adjustedTimes(runner.size());
  for (size_t k = 0; k < runner.size(); k++) {
    if (runner[k]->inputStatus == StatusOK && runner[k]->inputTime > 0) {
      int it = runner[k]->inputTime;
      if (runner[k]->tInTeam) {
        for (size_t j = 0; j < runner[k]->tInTeam->Runners.size(); j++) {
          int it2 = runner[k]->tInTeam->Runners[j]->inputTime;
          if (it2 > 0)
            it = max(it, it2);
        }
      }
      adjustedTimes[k] = it;
    }
  }

  vector< pair<int, int> > times(runner.size());

  for (size_t k = 0; k < runner.size(); k++) {
    times[k].second = k;
    if (runner[k]->inputStatus == StatusOK && adjustedTimes[k] > 0) {
      if (scale != 1.0)
        times[k].first = int(floor(double(adjustedTimes[k]) * scale + 0.5));
      else
        times[k].first = adjustedTimes[k];
    }
    else {
      times[k].first = timeConstHour * 24 * 7 + runner[k]->inputStatus;
      if (runner[k]->isVacant())
        times[k].first += 10; // Vacansies last
    }
  }
  // Sorted by name in input
  stable_sort(times.begin(), times.end());

  int delta = times[0].first;

  if (delta >= timeConstHour * 24 * 7)
    delta = 0;

  int reverseDelta = 0;
  if (reverse) {
    for (size_t k = 0; k < times.size(); k++) {
      if ((times[k].first - delta) < maxTime)
        reverseDelta = times[k].first;
    }
  }
  int odd = 0;
  int breakIndex = -1;
  for (size_t k = 0; k < times.size(); k++) {
    pRunner r = runner[times[k].second];

    if ((times[k].first - delta) < maxTime && breakIndex == -1) {
      if (!reverse)
        r->setStartTime(firstTime + times[k].first - delta, true, ChangeType::Update, false);
      else
        r->setStartTime(firstTime - times[k].first + reverseDelta, true, ChangeType::Update, false);
    }
    else if (!reverse) {
      if (breakIndex == -1)
        breakIndex = k;

      r->setStartTime(restartTime + ((k - breakIndex) / pairSize) * interval, true, ChangeType::Update, false);
    }
    else {
      if (breakIndex == -1) {
        breakIndex = times.size() - 1;
        odd = times.size() % 2;
      }

      r->setStartTime(restartTime + ((breakIndex - k + odd) / pairSize) * interval, true, ChangeType::Update, false);
    }
    r->synchronize(true);
  }
  reCalculateLeaderTimes(classId);
}

int oEvent::requestStartTime(int runnerId, int afterThisTime, int minTimeInterval,
  int lastAllowedTime, int maxParallel,
  bool allowSameCourse, bool allowSameCourseNeighbour,
  bool allowSameFirstControl, bool allowClubNeighbour) {
  synchronizeList({ oListId::oLRunnerId, oListId::oLClassId, oListId::oLCourseId });
  pRunner runner = getRunner(runnerId, 0);
  if (!runner || !runner->Class)
    return -1;

  assert(minTimeInterval > 0);
  pCourse crs = runner->getCourse(false);
  pControl firstC = !allowSameFirstControl && crs ? crs->getControl(0) : nullptr;
  wstring start = crs ? crs->getStart() : L"";

  set<int> usedStart;
  set<int> usedStartClass;
  set<int> usedClassClub;
  map<int, int> countStartSlots;
  int timeDivider = std::min(minTimeInterval, timeConstMinute);
  int slotDist = minTimeInterval / timeDivider;
  int closestStartSlotClass = numeric_limits<int>::max();
  int desiredSlot = afterThisTime / timeDivider;

  for (const oRunner& r : Runners) {
    if (r.isRemoved() || r.getStatus() == StatusNotCompetiting || r.getStatus() == StatusCANCEL)
      continue;

    if (r.getStartTime() <= 0)
      continue;

    // If needs to be inserted in map, otherwise just compute closest
    bool neededInMap = r.getStartTime() >= afterThisTime - timeDivider * slotDist;

    pCourse otherCrs = r.getCourse(false);
    int slot = r.getStartTime() / timeDivider;
    if (maxParallel > 0 && (!otherCrs || otherCrs->getStart() == start))
      if (neededInMap)
        ++countStartSlots[slot];

    if (r.Class == runner->Class) {
      if (abs(slot - desiredSlot) < abs(closestStartSlotClass - desiredSlot))
        closestStartSlotClass = slot;
      if (neededInMap) {
        usedStart.insert(slot);
        usedStartClass.insert(slot);
        if (!allowClubNeighbour && r.Club == runner->Club && r.Club && r.getClubId() != oe->getVacantClubIfExist(true)) {
          usedClassClub.insert(slot);
        }
      }
    }
    else if (!allowSameCourse && otherCrs == crs) {
      usedStart.insert(slot);
      if (allowSameCourseNeighbour) {
        if (abs(slot - slotDist) < abs(closestStartSlotClass - desiredSlot))
          closestStartSlotClass = slot;

        if (neededInMap)
          usedStartClass.insert(slot);
      }
    }
    else if (!allowSameFirstControl && otherCrs && otherCrs->getControl(0) == firstC) {
      if (neededInMap)
        usedStart.insert(slot);
    }
  }

  if (closestStartSlotClass < 10000) {
    if (closestStartSlotClass > desiredSlot) {
      int off = (closestStartSlotClass - desiredSlot) % slotDist;
      if (off > 0) {
        afterThisTime += off * timeDivider;
      }
    }
    else if (closestStartSlotClass < desiredSlot) {
      int off = (desiredSlot - closestStartSlotClass) % slotDist;
      if (off > 0) {
        int slotAdjust = slotDist - off;
        afterThisTime += slotAdjust * timeDivider;
      }
    }
  }
  for (int startTime = afterThisTime; startTime <= lastAllowedTime; startTime += timeDivider) {
    int slot = startTime / timeDivider;
    if (usedStart.count(slot))
      continue;
    if (maxParallel > 0) {
      auto res = countStartSlots.find(slot);
      if (res != countStartSlots.end() && res->second >= maxParallel)
        continue;
    }

    if (usedClassClub.count(slot - slotDist)) {
      startTime += (slotDist - 1) * timeDivider;
      continue;
    }
    if (usedClassClub.count(slot + slotDist)) {
      startTime += (2 * slotDist - 1) * timeDivider;
      continue;
    }

    bool ok = true;
    for (int j = 1; j < slotDist; j++) {
      if (usedStartClass.count(slot + j)) {
        ok = false;
        startTime += (j + slotDist - 1) * timeDivider; // Jump forward (optimize)
        break;
      }
      if (usedStartClass.count(slot - j)) {
        ok = false;
        break;
      }
    }
    if (ok)
      return startTime;
  }
  return -1; // No time found
}
