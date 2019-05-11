/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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
#include <cassert>
#include <algorithm>

#include "oEvent.h"
#include "gdioutput.h"
#include "oDataContainer.h"

#include "random.h"

#include "meos.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "oEventDraw.h"
#include "meosexception.h"

int ClassInfo::sSortOrder=0;

DrawInfo::DrawInfo() {
  changedVacancyInfo = true;
  changedExtraInfo = true;
  vacancyFactor = 0.05;
  extraFactor = 0.1;
  minVacancy = 1;
  maxVacancy = 10;
  baseInterval = 60;
  minClassInterval = 120;
  maxClassInterval = 180;
  nFields = 10;
  firstStart = 3600;
  maxCommonControl = 3;
  allowNeighbourSameCourse = true;
  coursesTogether = false;
  // Statistics output from optimize start order
  numDistinctInit = -1;
  numRunnerSameInitMax = -1;
  minimalStartDepth = -1;
}

bool ClassInfo::operator <(ClassInfo &ci)
{
  if (sSortOrder==0) {
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
      return firstStart<ci.firstStart;
  }
  else
    return firstStart<ci.firstStart;

}

struct ClassBlockInfo{
  int FirstControl;
  int nRunners;
  int Depth;

  int FirstStart;

  bool operator<(ClassBlockInfo &ci);
};

bool ClassBlockInfo::operator <(ClassBlockInfo &ci)
{
  return Depth<ci.Depth;
}

namespace {

  void getLargestClub(map<int, vector<pRunner> > &clubRunner, vector<pRunner> &largest)
  {
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

  void getRange(int size, vector<int> &p) {
    p.resize(size);
    for (size_t k = 0; k < p.size(); k++)
      p[k] = k;
  }

  void drawSOFTMethod(vector<pRunner> &runners, bool handleBlanks) {
    if (runners.empty())
      return;

    //Group runners per club
    map<int, vector<pRunner> > clubRunner;

    for (size_t k = 0; k < runners.size(); k++) {
      int clubId = runners[k] ? runners[k]->getClubId() : -1;
      clubRunner[clubId].push_back(runners[k]);
    }

    vector< vector<pRunner> > runnerGroups(1);

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
      maxGroup = max(maxGroup, runnerGroups[k].size());

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

  void drawMeOSMethod(vector<pRunner> &runners) {
    if (runners.empty())
      return;
    map<int, vector<pRunner>> runnersPerClub;
    for (pRunner r : runners)
      runnersPerClub[r->getClubId()].push_back(r);
    vector<pair<int, int>> sizeClub;
    for (auto &rc : runnersPerClub)
      sizeClub.emplace_back(rc.second.size(), rc.first);

    sort(sizeClub.rbegin(), sizeClub.rend());

    int targetGroupSize = max<int>(runners.size()/20, sizeClub.front().first);

    vector<vector<pRunner>> groups(1);
    for (auto &sc : sizeClub) {
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

    for (auto &group : groups) {
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
      takeMaxGroupInterval = max(2u, groups.size() - 2);

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

  bool isFree(const DrawInfo &di, vector< vector<pair<int, int> > > &StartField, int nFields,
              int FirstPos, int PosInterval, ClassInfo &cInfo)
  {
    int Type = cInfo.unique;
    int courseId = cInfo.courseId;

    int nEntries = cInfo.nRunners;
    bool disallowNeighbors = !di.allowNeighbourSameCourse;
    // Adjust first pos to make room for extra (before first start)
    if (cInfo.nExtra > 0) {
      int newFirstPos = FirstPos - cInfo.nExtra * PosInterval;
      while (newFirstPos < 0)
        newFirstPos += PosInterval;
      int extra = (FirstPos - newFirstPos) / PosInterval;
      nEntries += extra;
      FirstPos = newFirstPos;
    }

    //Check if free at all...
    for (int k = 0; k < nEntries; k++) {
      bool hasFree = false;
      for (int f = 0; f < nFields; f++) {
        size_t ix = FirstPos + k * PosInterval;
        int t = StartField[f][ix].first;

        if (disallowNeighbors) {
          int prevT = -1, nextT = -1;
          if (PosInterval > 1 && ix + 1 < StartField[f].size())
            nextT = StartField[f][ix + 1].second;
          if (PosInterval > 1 && ix > 0)
            prevT = StartField[f][ix - 1].second;

          if ((nextT > 0 && nextT == courseId) || (prevT > 0 && prevT == courseId))
            return false;
        }
        if (t == 0)
          hasFree = true;
        else if (t == Type)
          return false;//Type of course occupied. Cannot put it here;
      }

      if (!hasFree) return false;//No free start position.
    }

    return true;
  }

  bool insertStart(vector< vector< pair<int, int> > > &StartField, int nFields, ClassInfo &cInfo)
  {
    int Type = cInfo.unique;
    int courseId = cInfo.courseId;
    int nEntries = cInfo.nRunners;
    int FirstPos = cInfo.firstStart;
    int PosInterval = cInfo.interval;

    // Adjust first pos to make room for extra (before first start)
    if (cInfo.nExtra > 0) {
      int newFirstPos = FirstPos - cInfo.nExtra * PosInterval;
      while (newFirstPos < 0)
        newFirstPos += PosInterval;
      int extra = (FirstPos - newFirstPos) / PosInterval;
      nEntries += extra;
      FirstPos = newFirstPos;
    }

    for (int k = 0; k < nEntries; k++) {
      bool HasFree = false;

      for (int f = 0; f < nFields && !HasFree; f++) {
        if (StartField[f][FirstPos + k * PosInterval].first == 0) {
          StartField[f][FirstPos + k * PosInterval].first = Type;
          StartField[f][FirstPos + k * PosInterval].second = courseId;
          HasFree = true;
        }
      }

      if (!HasFree)
        return false; //No free start position. Fail.
    }

    return true;
  }
}

void oEvent::optimizeStartOrder(gdioutput &gdi, DrawInfo &di, vector<ClassInfo> &cInfo)
{
  if (Classes.size()==0)
    return;

  struct StartParam {
    int nControls;
    int alternator;
    double badness;
    int last;
    StartParam() : nControls(1), alternator(1), badness(1000), last(90000000) {}
  };

  StartParam opt;
  bool found = false;
  int nCtrl = 1;//max(1, di.maxCommonControl-2);
  const int maxControlDiff = di.maxCommonControl < 1000 ? di.maxCommonControl : 10;
  bool checkOnlyClass = di.maxCommonControl == 1000;
  while (!found) {

    StartParam optInner;
    for (int alt = 0; alt <= 20 && !found; alt++) {
      vector< vector<pair<int, int> > > startField(di.nFields);
      optimizeStartOrder(startField, di, cInfo, nCtrl, alt);

      int overShoot = 0;
      int overSum = 0;
      int numOver = 0;
      for (size_t k=0;k<cInfo.size();k++) {
        const ClassInfo &ci = cInfo[k];
        if (ci.overShoot>0) {
          numOver++;
          overShoot = max (overShoot, ci.overShoot);
          overSum += ci.overShoot;
        }
        //laststart=max(laststart, ci.firstStart+ci.nRunners*ci.interval);
      }
      double avgShoot = double(overSum)/cInfo.size();
      double badness = overShoot==0 ? 0 : overShoot / avgShoot;

      if (badness<optInner.badness) {
        optInner.badness = badness;
        optInner.alternator = alt;
        optInner.nControls = nCtrl;

        //Find last starter
        optInner.last = 0;
        for (int k=0;k<di.nFields;k++) {
          for (size_t j=0;j<startField[k].size(); j++)
            if (startField[k][j].first)
              optInner.last = max(optInner.last, int(j));
        }
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

    if (nCtrl>maxControlDiff) //We need some limit
      found = true;
  }

  vector< vector<pair<int, int> > > startField(di.nFields);
  optimizeStartOrder(startField, di, cInfo, opt.nControls, opt.alternator);

  gdi.addString("", 0, "Identifierar X unika inledningar på banorna.#" + itos(di.numDistinctInit));
  gdi.addString("", 0, "Största gruppen med samma inledning har X platser.#" + itos(di.numRunnerSameInitMax));
  gdi.addString("", 0, "Antal löpare på vanligaste banan X.#" + itos(di.numRunnerSameCourseMax));
  gdi.addString("", 0, "Kortast teoretiska startdjup utan krockar är X minuter.#" + itos(di.minimalStartDepth/60));
  gdi.dropLine();
  //Find last starter
  int last = opt.last;

  int laststart=0;
  for (size_t k=0;k<cInfo.size();k++) {
    const ClassInfo &ci = cInfo[k];
    laststart=max(laststart, ci.firstStart+(ci.nRunners-1)*ci.interval);
  }

  gdi.addString("", 0, "Faktiskt startdjup: X minuter.#" + itos(((last+1) * di.baseInterval)/60));

  gdi.addString("", 1, L"Sista start (nu tilldelad): X.#" +
                        oe->getAbsTime(laststart*di.baseInterval+di.firstStart));

  gdi.dropLine();

  int nr;
  int T=0;
  int sum=0;
  gdi.addString("", 1, "Antal startande per intervall (inklusive redan lottade):");
  string str="";
  int empty=4;

  while (T <= last) {
    nr=0;
    for(size_t k=0;k<startField.size();k++){
      if (startField[k][T].first)
        nr++;
    }
    T++;
    sum+=nr;
    if (nr!=0) empty=4;
    else empty--;

    char bf[20];
    sprintf_s(bf, 20, "%d ", nr);
    str+=bf;
  }

  gdi.addStringUT(10, str);
  gdi.dropLine();
}

int optimalLayout(int interval, vector< pair<int, int> > &classes) {
  sort(classes.begin(), classes.end());

  vector<int> chaining(interval, 0);

  for (int k = int(classes.size())-1 ; k >= 0; k--) {
    int ix = 0;
    // Find free position
    for (int i = 1; i<interval; i++) {
      if (chaining[i] < chaining[ix])
        ix = i;
    }
    int nr = classes[k].first;
    if (chaining[ix] > 0)
      nr += classes[k].second;

    chaining[ix] += 1 + interval*(nr-1);
  }

  int last = chaining[0];
  for (int i = 1; i<interval; i++) {
    last = max(chaining[i], last);
  }

  return last;
}

void oEvent::loadDrawSettings(const set<int> &classes, DrawInfo &drawInfo, vector<ClassInfo> &cInfo) const {
  drawInfo.firstStart = 3600 * 22;
  drawInfo.minClassInterval = 3600;
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
        reducedStart.insert(fs%iv);
      }
    }
  }

  drawInfo.baseInterval = drawInfo.minClassInterval;
  int lastStart = -1;
  for (set<int>::iterator it = reducedStart.begin(); it != reducedStart.end(); ++it) {
    if (lastStart == -1)
      lastStart = *it;
    else {
      drawInfo.baseInterval = min(drawInfo.baseInterval, *it-lastStart);
      lastStart = *it;
    }
  }

  map<int, int> runnerPerGroup;
  map<int, int> runnerPerCourse;

  cInfo.clear();
  cInfo.resize(classes.size());
  int i = 0;
  for (set<int>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
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
      cInfo[i].firstStart = fs;
      cInfo[i].unique = pc->getCourseId();
      if (cInfo[i].unique == 0)
        cInfo[i].unique = pc->getId() * 10000;
      cInfo[i].firstStart = (fs - drawInfo.firstStart) / drawInfo.baseInterval;
      cInfo[i].interval = iv / drawInfo.baseInterval;
      cInfo[i].nVacant = pc->getDrawVacant();
      cInfo[i].nExtra = pc->getDrawNumReserved();
      auto spec = pc->getDrawSpecification();
      cInfo[i].hasFixedTime = spec.count(oClass::DrawSpecified::FixedTime) != 0;
      cInfo[i].nExtraSpecified = spec.count(oClass::DrawSpecified::Extra) != 0;
      cInfo[i].nVacantSpecified = spec.count(oClass::DrawSpecified::Vacant) != 0;

      cInfo[i].nRunners = pc->getNumRunners(true, true, true) + cInfo[i].nVacant;

      if (cInfo[i].nRunners>0) {
        runnerPerGroup[cInfo[i].unique] += cInfo[i].nRunners;
        runnerPerCourse[cInfo[i].courseId] += cInfo[i].nRunners;
      }
      drawInfo.classes[*it] = cInfo[i];
      i++;
    }
  }

  for (size_t k = 0; k<cInfo.size(); k++) {
    cInfo[k].nRunnersGroup = runnerPerGroup[cInfo[k].unique];
    cInfo[k].nRunnersCourse = runnerPerCourse[cInfo[k].courseId];
  }
}

void oEvent::optimizeStartOrder(vector< vector<pair<int, int> > > &StartField, DrawInfo &di,
                                vector<ClassInfo> &cInfo, int useNControls, int alteration)
{

  if (di.firstStart<=0)
    di.firstStart = 0;

  if (di.minClassInterval < di.baseInterval) {
    throw meosException("Startintervallet får inte vara kortare än basintervallet.");
  }

  map<int, ClassInfo> otherClasses;
  cInfo.clear();
  oClassList::iterator c_it;
  map<int, int> runnerPerGroup;
  map<int, int> runnerPerCourse;
  int nRunnersTot = 0;
  for (c_it=Classes.begin(); c_it != Classes.end(); ++c_it) {
    bool drawClass = di.classes.count(c_it->getId())>0;
    ClassInfo *cPtr = 0;

    if (!drawClass) {
      otherClasses[c_it->getId()] = ClassInfo(&*c_it);
      cPtr = &otherClasses[c_it->getId()];
    }
    else
      cPtr = &di.classes[c_it->getId()];

    ClassInfo &ci =  *cPtr;
    pCourse pc = c_it->getCourse();

    if (pc && useNControls < 1000) {
      if (useNControls>0 && pc->nControls>0)
        ci.unique = 1000000 + pc->getIdSum(useNControls);
      else
        ci.unique = 10000 + pc->getId();

      ci.courseId = pc->getId();
    }
    else
      ci.unique = ci.classId;

    if (!drawClass)
      continue;

    int nr = c_it->getNumRunners(true, true, true);
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

    if (ci.nRunners>0) {
      nRunnersTot += ci.nRunners + ci.nExtra;
      cInfo.push_back(ci);
      runnerPerGroup[ci.unique] += ci.nRunners + ci.nExtra;
      runnerPerCourse[ci.courseId] += ci.nRunners + ci.nExtra;
    }
  }

  int maxGroup = 0;
  int maxCourse = 0;
  int maxNRunner = 0;
  int a = 1 + (alteration % 7);
  int b = (alteration % 3);
  int c = alteration % 5;

  for (size_t k = 0; k<cInfo.size(); k++) {
    maxNRunner = max(maxNRunner, cInfo[k].nRunners);
    cInfo[k].nRunnersGroup = runnerPerGroup[cInfo[k].unique];
    cInfo[k].nRunnersCourse = runnerPerCourse[cInfo[k].courseId];
    maxGroup = max(maxGroup, cInfo[k].nRunnersGroup);
    maxCourse = max(maxCourse, cInfo[k].nRunnersCourse);
    cInfo[k].sortFactor = cInfo[k].nRunners * a + cInfo[k].nRunnersGroup * b + cInfo[k].nRunnersCourse * c;
  }

  di.numDistinctInit = runnerPerGroup.size();
  di.numRunnerSameInitMax = maxGroup;
  di.numRunnerSameCourseMax = maxCourse;
  // Calculate the theoretical best end position to use.
  int bestEndPos = 0;

  for (map<int, int>::iterator it = runnerPerGroup.begin(); it != runnerPerGroup.end(); ++it) {
    vector< pair<int, int> > classes;
    for (size_t k = 0; k<cInfo.size(); k++) {
      if (cInfo[k].unique == it->first)
        classes.push_back(make_pair(cInfo[k].nRunners, cInfo[k].nExtra));
    }
    int optTime = optimalLayout(di.minClassInterval/di.baseInterval, classes);
    bestEndPos = max(optTime, bestEndPos);
  }

  if (nRunnersTot > 0)
    bestEndPos = max(bestEndPos, nRunnersTot / di.nFields);

  bestEndPos = max(bestEndPos, maxCourse * 2);

  di.minimalStartDepth = bestEndPos * di.baseInterval;

  ClassInfo::sSortOrder = 0;
  sort(cInfo.begin(), cInfo.end());

  int maxSize = di.minClassInterval * maxNRunner;

  // Special case for constant time start
  if (di.baseInterval==0) {
    di.baseInterval = 1;
    di.minClassInterval = 0;
  }

  // Calculate an estimated maximal class intervall
  for (size_t k = 0; k < cInfo.size(); k++) {
    int quotient = maxSize/(cInfo[k].nRunners*di.baseInterval);

    if (quotient*di.baseInterval > di.maxClassInterval)
      quotient=di.maxClassInterval/di.baseInterval;

    if (cInfo[k].nRunnersGroup >= maxGroup)
      quotient = di.minClassInterval / di.baseInterval;

    if (!cInfo[k].hasFixedTime)
      cInfo[k].interval = quotient;
  }

  for(int m=0;m < di.nFields;m++)
    StartField[m].resize(3000);

  int alternator = 0;

  // Fill up with non-drawn classes
  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    int st = it->getStartTime();
    int relSt = st-di.firstStart;
    int relPos = relSt / di.baseInterval;

    if (st>0 && relSt>=0 && relPos<3000 && (relSt%di.baseInterval) == 0) {
      if (otherClasses.count(it->getClassId(false))==0)
        continue;

      if (!di.startName.empty() && it->Class && it->Class->getStart()!=di.startName)
        continue;

      ClassInfo &ci = otherClasses[it->getClassId(false)];
      int k = 0;
      while(true) {
        if (k==StartField.size()) {
          StartField.push_back(vector< pair<int, int> >());
          StartField.back().resize(3000);
        }
        if (StartField[k][relPos].first==0) {
          StartField[k][relPos].first = ci.unique;
          StartField[k][relPos].second = ci.courseId;
          break;
        }
        k++;
      }
    }
  }

  // Fill up classes with fixed starttime
  for (size_t k = 0; k < cInfo.size(); k++) {
    if (cInfo[k].hasFixedTime) {
      insertStart(StartField, di.nFields, cInfo[k]);
    }
  }

  if (di.minClassInterval == 0) {
    // Set fixed start time
    for (size_t k = 0; k < cInfo.size(); k++) {
      if (cInfo[k].hasFixedTime)
        continue;
      cInfo[k].firstStart = di.firstStart;
      cInfo[k].interval = 0;
    }
  }
  else {
    // Do the distribution
    for (size_t k = 0; k < cInfo.size(); k++) {
      if (cInfo[k].hasFixedTime)
        continue;

      int minPos = 1000000;
      int minEndPos = 1000000;
      int minInterval=cInfo[k].interval;

      for (int i = di.minClassInterval/di.baseInterval; i<=cInfo[k].interval; i++) {

        int startpos = alternator % max(1, (bestEndPos - cInfo[k].nRunners * i)/3);
        startpos = 0;
        int ipos = startpos;
        int t = 0;

        while( !isFree(di, StartField, di.nFields, ipos, i, cInfo[k]) ) {
          t++;

          // Algorithm to randomize start position
          // First startpos -> bestEndTime, then 0 -> startpos, then remaining
          if (t<(bestEndPos-startpos))
            ipos = startpos + t;
          else {
            ipos = t - (bestEndPos-startpos);
            if (ipos>=startpos)
              ipos  = t;
          }
        }

        int endPos = ipos + i*cInfo[k].nRunners;
        if (endPos < minEndPos || endPos < bestEndPos) {
          minEndPos = endPos;
          minPos = ipos;
          minInterval = i;
        }
      }

      cInfo[k].firstStart = minPos;
      cInfo[k].interval = minInterval;
      cInfo[k].overShoot = max(minEndPos - bestEndPos, 0);
      insertStart(StartField, di.nFields, cInfo[k]);

      alternator += alteration;
    }
  }
}

void oEvent::drawRemaining(DrawMethod method, bool placeAfter)
{
  DrawType drawType = placeAfter ? DrawType::RemainingAfter : DrawType::RemainingBefore;

  for (oClassList::iterator it = Classes.begin(); it !=  Classes.end(); ++it) {
    vector<ClassDrawSpecification> spec;
    spec.push_back(ClassDrawSpecification(it->getId(), 0, 0, 0, 0));

    drawList(spec, method, 1, drawType);
  }
}

void oEvent::drawList(const vector<ClassDrawSpecification> &spec, 
                      DrawMethod method, int pairSize, DrawType drawType) {

  autoSynchronizeLists(false);
  assert(pairSize > 0);
  oRunnerList::iterator it;

  int VacantClubId=getVacantClub(false);
  map<int, int> clsId2Ix;
  set<int> clsIdClearVac;

  const bool multiDay = hasPrevStage();

  for (size_t k = 0; k < spec.size(); k++) {
    pClass pc = getClass(spec[k].classID);

    if (!pc)
      throw std::exception("Klass saknas");

    if (spec[k].vacances>0 && pc->getClassType()==oClassRelay)
      throw std::exception("Vakanser stöds ej i stafett.");

    if (spec[k].vacances>0 && (spec[k].leg>0 || pc->getParentClass()))
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
      for (it=Runners.begin(); it != Runners.end(); ++it) {
        if (clsIdClearVac.count(it->getClassId(true))) {
          if (it->isRemoved())
            continue;
          if (it->tInTeam)
            continue; // Cannot remove team runners
          if (it->getClubId()==VacantClubId) {
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
          oe->addRunnerVacant(spec[k].classID);
        }
      }
    }

    for (it=Runners.begin(); it != Runners.end(); ++it) {
      int cid = it->getClassId(true);
      if (!it->isRemoved() && clsId2Ix.count(cid)) {
        if (it->getStatus() == StatusNotCompetiting)
          continue;
        int ix = clsId2Ix[cid];
        if (it->legToRun() == spec[ix].leg || spec[ix].leg == -1) {
          runners.push_back(&*it);
          spec[ix].ntimes++;
        }
      }
    }
  }
  else {
    // Find first/last start in class and interval:
    vector<int> first(spec.size(), 7*24*3600);
    vector<int> last(spec.size(), 0);
    set<int> cinterval;
    int baseInterval = 10*60;

    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (!it->isRemoved() && clsId2Ix.count(it->getClassId(true))) {
        if (it->getStatus() == StatusNotCompetiting)
          continue;

        int st = it->getStartTime();
        int ix = clsId2Ix[it->getClassId(false)];
          
        if (st>0) {
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
    int t=0;
    for (set<int>::iterator sit = cinterval.begin(); sit!=cinterval.end();++sit) {
      if ( (*sit-t) > 0)
        baseInterval = min(baseInterval, (*sit-t));
      t = *sit;
    }

    for (size_t k = 0; k < spec.size(); k++) {
      if (drawType == DrawType::RemainingBefore)
        spec[k].firstStart = first[k] - runners.size()*baseInterval;
      else
        spec[k].firstStart = last[k] + baseInterval;

      spec[k].interval = baseInterval;

      if (last[k] == 0 || spec[k].firstStart<=0 ||  baseInterval == 10*60) {
        // Fallback if incorrect specification.
        spec[k].firstStart = 3600;
        spec[k].interval = 2*60;
      }
    }
  }

  if (runners.empty())
    return;

  vector<int> stimes(runners.size());
  int nr = 0;
  for (size_t k = 0; k < spec.size(); k++) {
    for (int i = 0; i < spec[k].ntimes; i++) {
      int kx = i/pairSize;
      stimes[nr++] = spec[k].firstStart + spec[k].interval * kx;
    }
  }
  
  if (spec.size() > 1)
    sort(stimes.begin(), stimes.end());

  if (gdibase.isTest())
    InitRanom(0,0);

  switch (method) {
  case DrawMethod::SOFT:
    drawSOFTMethod(runners, true);
    break;
  case DrawMethod::MeOS:
    drawMeOSMethod(runners);
    break;
  case DrawMethod::Random:
    permute(stimes);
    break;
  default:
    throw 0;
  }

  int minStartNo = Runners.size();
  vector<pair<int, int>> newStartNo;
  for(unsigned k=0;k<stimes.size(); k++) {
    runners[k]->setStartTime(stimes[k], true, false, false);
    runners[k]->synchronize();
    minStartNo = min(minStartNo, runners[k]->getStartNo());
    newStartNo.emplace_back(stimes[k], k);
  }

  sort(newStartNo.begin(), newStartNo.end());
  //CurrentSortOrder = SortByStartTime;
  //sort(runners.begin(), runners.end());

  if (minStartNo == 0)
    minStartNo = nextFreeStartNo + 1;

  for(size_t k=0; k<runners.size(); k++) {
    pClass pCls = runners[k]->getClassRef(true);
    if (pCls && pCls->lockedForking() || runners[k]->getLegNumber() > 0)
      continue;
    runners[k]->updateStartNo(newStartNo[k].second + minStartNo);
  }

  nextFreeStartNo = max<int>(nextFreeStartNo, minStartNo + stimes.size());
}

void oEvent::drawListClumped(int ClassID, int FirstStart, int Interval, int Vacances)
{
  pClass pc=getClass(ClassID);

  if (!pc)
    throw std::exception("Klass saknas");

  if (Vacances>0 && pc->getClassType()!=oClassIndividual)
    throw std::exception("Lottningsmetoden stöds ej i den här klassen.");

  oRunnerList::iterator it;
  int nRunners=0;

  autoSynchronizeLists(false);

  while (Vacances>0) {
    addRunnerVacant(ClassID);
    Vacances--;
  }

  for (it=Runners.begin(); it != Runners.end(); ++it)
    if (it->Class && it->Class->Id==ClassID) nRunners++;

  if (nRunners==0) return;

  int *stimes=new int[nRunners];


  //Number of start groups
  //int ngroups=(nRunners/5)
  int ginterval;

  if (nRunners>=Interval)
    ginterval=10;
  else if (Interval/nRunners>60){
    ginterval=40;
  }
  else if (Interval/nRunners>30){
    ginterval=20;
  }
  else if (Interval/nRunners>20){
    ginterval=15;
  }
  else if (Interval/nRunners>10){
    ginterval=1;
  }
  else ginterval=10;

  int nGroups=Interval/ginterval+1; //15 s. per interval.
  int k;

  if (nGroups>0){

    int MaxRunnersGroup=max((2*nRunners)/nGroups, 4)+GetRandomNumber(2);
    int *sgroups=new int[nGroups];

    for(k=0;k<nGroups; k++)
      sgroups[k]=FirstStart+((ginterval*k+2)/5)*5;

    if (nGroups>5){
      //Remove second group...
      sgroups[1]=sgroups[nGroups-1];
      nGroups--;
    }

    if (nGroups>9 && ginterval<60 && (GetRandomBit() || GetRandomBit() || GetRandomBit())){
      //Remove third group...
      sgroups[2]=sgroups[nGroups-1];
      nGroups--;

      if (nGroups>13 &&  ginterval<30 && (GetRandomBit() || GetRandomBit() || GetRandomBit())){
        //Remove third group...
        sgroups[3]=sgroups[nGroups-1];
        nGroups--;


        int ng=4; //Max two minutes pause
        while(nGroups>10 && (nRunners/nGroups)<MaxRunnersGroup && ng<8 && (GetRandomBit() || GetRandomBit())){
          //Remove several groups...
          sgroups[ng]=sgroups[nGroups-1];
          nGroups--;
          ng++;
        }
      }
    }

    //Permute some of the groups (not first and last group)
    if (nGroups>5){
      permute(sgroups+2, nGroups-2);

      //Remove some random groups (except first and last).
      for(k=2;k<nGroups; k++){
        if ((nRunners/nGroups)<MaxRunnersGroup && nGroups>5){
          sgroups[k]=sgroups[nGroups-1];
          nGroups--;
        }
      }
    }

    //Premute all groups;
    permute(sgroups, nGroups);

    int *counters=new int[nGroups];
    memset(counters, 0, sizeof(int)*nGroups);

    stimes[0]=FirstStart;
    stimes[1]=FirstStart+Interval;

    for(k=2;k<nRunners; k++){
      int g=GetRandomNumber(nGroups);

      if (counters[g]<=2 && GetRandomBit()){
        //Prefer already large groups
        g=(g+1)%nGroups;
      }

      if (counters[g]>MaxRunnersGroup){
        g=(g+3)%nGroups;
      }

      if (sgroups[g]==FirstStart){
        //Avoid first start
        if (GetRandomBit() || GetRandomBit())
          g=(g+1)%nGroups;
      }

      if (counters[g]>MaxRunnersGroup){
        g=(g+2)%nGroups;
      }

      if (counters[g]>MaxRunnersGroup){
        g=(g+2)%nGroups;
      }

      stimes[k]=sgroups[g];
      counters[g]++;
    }

    delete[] sgroups;
    delete[] counters;

  }
  else{
    for(k=0;k<nRunners; k++) stimes[k]=FirstStart;
  }


  permute(stimes, nRunners);

  k=0;

  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->Class && it->Class->Id == ClassID) {
      it->setStartTime(stimes[k++], true, false, false);
      it->StartNo = k;
      it->synchronize();
    }
  }
  reCalculateLeaderTimes(ClassID);

  delete[] stimes;
}

void oEvent::automaticDrawAll(gdioutput &gdi, const wstring &firstStart,
                              const wstring &minIntervall, const wstring &vacances,
                              bool lateBefore, DrawMethod method, int pairSize)
{
  gdi.refresh();
  const int leg = 0;
  const double extraFactor = 0.0;
  int drawn = 0;

  int baseInterval = convertAbsoluteTimeMS(minIntervall)/2;

  if (baseInterval == 0) {
    gdi.fillDown();
    int iFirstStart = getRelativeTime(firstStart);

    if (iFirstStart>0)
      gdi.addString("", 1, "Gemensam start");
    else {
      gdi.addString("", 1, "Nollställer starttider");
      iFirstStart = 0;
    }
    gdi.refreshFast();
    gdi.dropLine();
    for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
      if (it->isRemoved())
        continue;
      vector<ClassDrawSpecification> spec;
      spec.push_back(ClassDrawSpecification(it->getId(), 0, iFirstStart, 0, 0));
      oe->drawList(spec, DrawMethod::Random, 1, DrawType::DrawAll);
    }
    return;
  }

  if (baseInterval<1 || baseInterval>60*60)
    throw std::exception("Felaktigt tidsformat för intervall");

  int iFirstStart = getRelativeTime(firstStart);

  if (iFirstStart<=0)
    throw std::exception("Felaktigt tidsformat för första start");

  double vacancy = _wtof(vacances.c_str())/100;

  gdi.fillDown();
  gdi.addString("", 1, "Automatisk lottning").setColor(colorGreen);
  gdi.addString("", 0, "Inspekterar klasser...");
  gdi.refreshFast();

  set<int> notDrawn;
  getNotDrawnClasses(notDrawn, false);

  set<int> needsCompletion;
  getNotDrawnClasses(needsCompletion, true);

  for(set<int>::iterator it = notDrawn.begin(); it!=notDrawn.end(); ++it)
    needsCompletion.erase(*it);

  //Start with not drawn classes
  map<wstring, int> starts;
  map<pClass, int> runnersPerClass;

  // Count number of runners per start
  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    if (it->skip())
      continue;
    if (it->tLeg != leg)
      continue;
    if (it->isVacant() && notDrawn.count(it->getClassId(false))==1)
      continue;
    pClass pc = it->Class;

    if (pc && pc->hasFreeStart())
      continue;

    if (pc)
      ++starts[pc->getStart()];

    ++runnersPerClass[pc];
  }

  while ( !starts.empty() ) {
    // Select smallest start
    int runnersStart = Runners.size()+1;
    wstring start;
    for ( map<wstring, int>::iterator it = starts.begin(); it != starts.end(); ++it) {
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
    for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
      if (it->getStart() != start)
        continue;
      if (it->hasFreeStart())
        continue;

      maxRunners = max(maxRunners, runnersPerClass[&*it]);
    }

    if (maxRunners==0)
      continue;

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

    int optimalParallel = runnersStart / (maxRunners*2); // Min is every second interval

    di.nFields = max(3, min (optimalParallel + 2, 15));
    di.baseInterval = baseInterval;
    di.extraFactor = extraFactor;
    di.firstStart = iFirstStart;
    di.minClassInterval = baseInterval * 2;
    di.maxClassInterval = di.minClassInterval;

    di.minVacancy = 1;
    di.maxVacancy = 100;
    di.vacancyFactor = vacancy;

    di.startName = start;

    for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
      if (it->getStart() != start)
        continue;
      if (notDrawn.count(it->getId())==0)
        continue; // Only not drawn classes
      if (it->hasFreeStart())
      continue;

      di.classes[it->getId()] = ClassInfo(&*it);
    }

    if (di.classes.size()==0)
      continue;

    gdi.dropLine();
    gdi.addStringUT(1, lang.tl(L"Optimerar startfördelning ") + start);
    gdi.refreshFast();
    gdi.dropLine();
    vector<ClassInfo> cInfo;
    optimizeStartOrder(gdi, di, cInfo);


    int laststart=0;
    for (size_t k=0;k<cInfo.size();k++) {
      const ClassInfo &ci = cInfo[k];
      laststart=max(laststart, ci.firstStart+ci.nRunners*ci.interval);
    }

    gdi.addStringUT(1, lang.tl("Sista start (nu tilldelad)") + L": " +
                    getAbsTime((laststart)*di.baseInterval+di.firstStart));
    gdi.dropLine();
    gdi.refreshFast();

    for (size_t k=0;k<cInfo.size();k++) {
      const ClassInfo &ci = cInfo[k];

      if (getClass(ci.classId)->getClassType() == oClassRelay) {
        gdi.addString("", 0, L"Hoppar över stafettklass: X#" +
                    getClass(ci.classId)->getName()).setColor(colorRed);
        continue;
      }

      gdi.addString("", 0, L"Lottar: X#" + getClass(ci.classId)->getName());
      vector<ClassDrawSpecification> spec;
      spec.push_back(ClassDrawSpecification(ci.classId, leg, 
                                  di.firstStart + di.baseInterval * ci.firstStart, 
                                  di.baseInterval * ci.interval, ci.nVacant));

      drawList(spec, method, pairSize, DrawType::DrawAll);
      gdi.scrollToBottom();
      gdi.refreshFast();
      drawn++;
    }
  }

  // Classes that need completion
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (needsCompletion.count(it->getId())==0)
      continue;
    if (it->hasFreeStart())
      continue;

    gdi.addStringUT(0, lang.tl(L"Lottar efteranmälda: ") + it->getName());

    vector<ClassDrawSpecification> spec;
    spec.push_back(ClassDrawSpecification(it->getId(), leg, 0, 0, 0));
    drawList(spec, method, 1, lateBefore ? DrawType::RemainingBefore : DrawType::RemainingAfter);

    gdi.scrollToBottom();
    gdi.refreshFast();
    drawn++;
  }

  gdi.dropLine();

  if (drawn==0)
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
  if (classId<=0)
    return;

  pClass pc=getClass(classId);

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
  for (size_t k = 0; k< trunner.size(); k++) // Only treat specified leg
    if (trunner[k]->tLeg == leg)
      runner.push_back(trunner[k]);

  if (runner.empty())
    return;

  // Make sure patrol members use the same time
  vector<int> adjustedTimes(runner.size());
  for (size_t k = 0; k<runner.size(); k++) {
    if (runner[k]->inputStatus == StatusOK && runner[k]->inputTime>0) {
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

  for (size_t k = 0; k<runner.size(); k++) {
    times[k].second = k;
    if (runner[k]->inputStatus == StatusOK && adjustedTimes[k]>0) {
      if (scale != 1.0)
        times[k].first = int(floor(double(adjustedTimes[k]) * scale + 0.5));
      else
        times[k].first = adjustedTimes[k];
    }
    else {
      times[k].first = 3600 * 24 * 7 + runner[k]->inputStatus;
      if (runner[k]->isVacant())
        times[k].first += 10; // Vacansies last
    }
  }
  // Sorted by name in input
  stable_sort(times.begin(), times.end());

  int delta = times[0].first;

  if (delta >= 3600*24*7)
    delta = 0;

  int reverseDelta = 0;
  if (reverse) {
    for (size_t k = 0; k<times.size(); k++) {
      if ((times[k].first - delta) < maxTime)
        reverseDelta = times[k].first;
    }
  }
  int odd = 0;
  int breakIndex = -1;
  for (size_t k = 0; k<times.size(); k++) {
    pRunner r = runner[times[k].second];

    if ((times[k].first - delta) < maxTime && breakIndex == -1) {
      if (!reverse)
        r->setStartTime(firstTime + times[k].first - delta, true, false, false);
      else
        r->setStartTime(firstTime - times[k].first + reverseDelta, true, false, false);
    }
    else if (!reverse) {
      if (breakIndex == -1)
        breakIndex = k;

      r->setStartTime(restartTime + ((k - breakIndex)/pairSize) * interval, true, false, false);
    }
    else {
      if (breakIndex == -1) {
        breakIndex = times.size() - 1;
        odd = times.size() % 2;
      }

      r->setStartTime(restartTime + ((breakIndex - k + odd)/pairSize) * interval, true, false, false);
    }
    r->synchronize(true);
  }
  reCalculateLeaderTimes(classId);
}
