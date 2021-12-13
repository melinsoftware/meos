/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2021 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "StdAfx.h"

#include "qualification_final.h"

#include "meos_util.h"
#include "meosexception.h"
#include <set>
#include <algorithm>
#include "xmlparser.h"
#include "oRunner.h"
#include "localizer.h"

pair<int, int> QualificationFinal::getNextFinal(int instance, int orderPlace, int numSharedPlaceNext) const {
  pair<int, int> key(instance, orderPlace);
  int iter = 0;
  while (numSharedPlaceNext >= 0) {
    auto res = sourcePlaceToFinalOrder.find(key);
    if (res != sourcePlaceToFinalOrder.end()) {
      if (iter >= 2) { // For  three in a shared last place
        pair<int, int> key2 = key;
        int extraSub = ((iter + 1) % 2);
        key2.second -= extraSub;
        auto res2 = sourcePlaceToFinalOrder.find(key2);
        if (res2 != sourcePlaceToFinalOrder.end()) {
          auto ans = res2->second;
          ans.second += iter + extraSub;
          return ans;
        }
      }


      auto ans = res->second;
      ans.second += iter;
      return ans;
    }
    --key.second;
    --numSharedPlaceNext;
    ++iter;
  }

  return make_pair(0, -1);
}

bool QualificationFinal::noQualification(int instance) const {
  if (size_t(instance) >= classDefinition.size())
    return false;

  return classDefinition[instance].qualificationMap.empty() &&
         classDefinition[instance].timeQualifications.empty();
}


void QualificationFinal::getBaseClassInstances(set<int> &base) const {
  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (noQualification(k))
      base.insert(k+1);
    else break;
  }

}

void QualificationFinal::import(const wstring &file) {
  xmlparser xml;
  xml.read(file);

  auto qr = xml.getObject("QualificationRules");
  xmlList levels;
  qr.getObjects("Level", levels);
  map<int, int> idToIndex;
  map<int, set<int> > qualificationRelations;
  int numBaseLevels = 0;
  int iLevel = 0;
  for (size_t iLevel = 0; iLevel < levels.size(); iLevel++) {
    auto &level = levels[iLevel];
    wstring rankS;
    level.getObjectString("distribution", rankS);
    bool rankSort = false;
    if (rankS == L"Ranking")
      rankSort = true;
    else if (!rankS.empty())
      throw meosException(L"Unknown distribution: " + rankS);

    xmlList classes;
    level.getObjects("Class", classes);
    for (auto &cls : classes) {
      wstring name;
      cls.getObjectString("Name", name);
      if (name.empty())
        throw meosException("Klassen måste ha ett namn.");
      int classId = cls.getObjectInt("id");
      if (!(classId>0))
        throw meosException("Id must be a positive integer.");
      if (idToIndex.count(classId))
        throw meosException("Duplicate class with id " + itos(classId));

      xmlList rules;
      cls.getObjects("Qualification", rules);
      if (rules.empty())
        numBaseLevels = 1; // Instance zero is not used as qualification,

      idToIndex[classId] = classDefinition.size() + numBaseLevels;
      classDefinition.push_back(Class());
      classDefinition.back().name = name;
      classDefinition.back().rankLevel = rankSort;
      for (auto &qf : rules) {
        int place = qf.getObjectInt("place");
        if (place > 0) {
          int id = qf.getObjectInt("id");
          if (id == 0 && iLevel == 0)
            classDefinition.back().qualificationMap.push_back(make_pair(0, place));
          else if (idToIndex.count(id)) {
            classDefinition.back().qualificationMap.push_back(make_pair(idToIndex[id], place));
            qualificationRelations[classId].insert(id);
          }
          else
            throw meosException("Unknown class with id " + itos(id));
        }
        else {
          string time;
          qf.getObjectString("place", time);
          if (time == "time") {
            string ids;
            qf.getObjectString("id", ids);
            vector<string> vid;
            split(ids, ",;", vid);
            vector<int> ivid;
            for (auto &s : vid) {
              int i = atoi(s.c_str());
              if (!idToIndex.count(i))
                throw meosException("Unknown class with id " + itos(i));
              ivid.push_back(idToIndex[i]);
            }

            if (ivid.empty())
              throw meosException(L"Empty time qualification for " + name);
            classDefinition.back().timeQualifications.push_back(ivid);
          }
          else throw meosException("Unknown classification rule " + time);
        }
      }
    }
  }
  /*classDefinition.resize(3);
  classDefinition[0].name = L"Semi A";
  classDefinition[1].name = L"Semi B";
  classDefinition[2].name = L"Final";

  for (int i = 0; i < 4; i++) {
    classDefinition[0].qualificationMap.push_back(make_pair(0, i * 2 + 1));
    classDefinition[1].qualificationMap.push_back(make_pair(0, i * 2 + 2));
    classDefinition[2].qualificationMap.push_back(make_pair(i%2+1, i/2 + 1));
  }
  */
  initgmap(true);
}

void QualificationFinal::init(const wstring &def) {
  serializedFrom = def;
  vector <wstring> races, rtdef, rdef; 
  split(def, L"|", races);
  classDefinition.resize(races.size());
  bool valid = true;
  bool first = true;
  for (size_t k = 0; k < races.size(); k++) {
    bool rankLevel = false;
    if (races[k].size() > 0 && races[k][0] == 'R') {
      rankLevel = true;
      races[k] = races[k].substr(1);
    }
    split(races[k], L"T", rtdef);
    classDefinition[k].qualificationMap.clear();
    classDefinition[k].timeQualifications.clear();
    classDefinition[k].rankLevel = rankLevel;

    if (first && rtdef.empty())
      continue;

    valid = rtdef.size() > 0;
    if (!valid)
      break;
    first = false;
    
    split(rtdef[0], L";", rdef);
    valid = rdef.size() > 0 && rdef.size()%2 == 0;
    if (!valid)
      break;

    for (size_t j = 0; j < rdef.size(); j+=2) {
      size_t src = _wtoi(rdef[j].c_str());
      if (src > k) {
        valid = false;
        break;
      }
      const wstring &rd = rdef[j + 1];
      size_t d1 = _wtoi(rd.c_str());
      size_t d2 = d1;
      size_t range = rd.find_first_of('-', 0);
      if (range < rd.size())
        d2 = _wtoi(rd.c_str()+range+1);

      if (d1 > d2) {
        valid = false;
        break;
      }

      while (d1 <= d2) {
        classDefinition[k].qualificationMap.push_back(make_pair(int(src), int(d1)));
        d1++;
      }
    }

    for (size_t i = 1; valid && i < rtdef.size(); i++) {
      split(rtdef[i], L";", rdef);
      classDefinition[k].timeQualifications.push_back(vector<int>());
      for (size_t j = 0; valid && j < rdef.size(); j += 2) {
        size_t src = _wtoi(rdef[j].c_str());
        if (src > k) {
          valid = false;
          break;
        }
        classDefinition[k].timeQualifications.back().push_back(src);
      }
    }
  }

  if (!valid)
    classDefinition.clear();

  initgmap(false);
}

void QualificationFinal::encode(wstring &output) const {
  output.clear();

  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (k > 0)
      output.append(L"|");

    if (classDefinition[k].rankLevel)
      output.append(L"R");

    auto &qm = classDefinition[k].qualificationMap;

    for (size_t j = 0; j < qm.size(); j++) {
      if (j > 0)
        output.append(L";");

      size_t i = j;
      while ((i + 1) < qm.size() && qm[i + 1].first == qm[i].first
              && qm[i + 1].second == qm[i].second+1) {
        i++;
      }
      output.append(itow(qm[j].first) + L";");
      if (i <= j + 1) {
        output.append(itow(qm[j].second));
      }
      else {
        output.append(itow(qm[j].second) + L"-" + itow(qm[i].second));
        j = i;
      } 
    }

    auto &tqm = classDefinition[k].timeQualifications;
    for (auto &source :  tqm) {
      output.append(L"T");
      for (size_t i = 0; i < source.size(); i++) {
        if (i > 0)
          output.append(L";");
        output.append(itow(source[i]));
      }
    }
  }

  serializedFrom = output;
}

int QualificationFinal::getNumStages(int stage) const {
  if (stage == 0 || classDefinition[stage-1].qualificationMap.empty())
    return 1;

  set<int> races;
  for (auto &qm : classDefinition[stage - 1].qualificationMap) {
    if (qm.first == stage)
      throw meosException("Invalid qualification scheme");
    races.insert(qm.first);
  }

  int def = 0;
  for (int r : races)
    def = max(def, 1 + getNumStages(r));

  return def;
}

void QualificationFinal::initgmap(bool check) {
  sourcePlaceToFinalOrder.clear();
  levels.resize(getNumLevels(), LevelInfo::Normal);

  for (int ix = 0; ix < (int)classDefinition.size(); ix++) {
    auto &c = classDefinition[ix];
    if (c.rankLevel)
      levels[getLevel(ix+1)] = LevelInfo::RankSort;

    for (int k = 0; k < (int)c.qualificationMap.size(); k++) {
      const pair<int, int> &sd = c.qualificationMap[k];
      if (check && sourcePlaceToFinalOrder.count(sd))
        throw meosException(L"Inconsistent qualification rule, X#" + c.name + + L"/" + itow(sd.first));

      sourcePlaceToFinalOrder[sd] = make_pair(ix+1, k);
    }
  }
}

int QualificationFinal::getLevel(int instance) const {
  if (instance == 0)
    return 0;
  instance--;
  if (classDefinition[instance].level >= 0)
    return classDefinition[instance].level;
  int level = 0;
  int src = instance;
  while (!classDefinition[instance].qualificationMap.empty()) {
    instance = classDefinition[instance].qualificationMap.front().first - 1;
    if (instance < 0)
      break;
    level++;
    if (size_t(level) > classDefinition.size())
      throw meosException("Internal error");
  }
  classDefinition[src].level = level;
  return level;
}

bool QualificationFinal::isFinalClass(int instance) const {
  return instance > 0 && sourcePlaceToFinalOrder.count(make_pair(instance, 1)) == 0;
}

int QualificationFinal::getNumLevels() const {
  return getLevel(classDefinition.size() - 1) + 1;
}

int QualificationFinal::getMinInstance(int level) const {
  int minInst = classDefinition.size() - 1;
  for (int i = classDefinition.size() - 1; i >= 0; i--) {
    if (i == 0 && minInst == 1)
      break; // No need to include base instance.
    if (getLevel(i) == level)
      minInst = i;
  }
  return minInst;
}


int QualificationFinal::getHeatFromClass(int finalClassId, int baseClassId) const {
  if (baseClassId == baseId) {
    int fci = (finalClassId - baseId) / maxClassId;

    if (fci * maxClassId + baseClassId == finalClassId)
      return fci;
    else
      return -1;
  }

  return 0;
}

/** Clear previous staus data*/
void QualificationFinal::prepareCalculations() {
  storedInfoLookup.clear();
  storedInfo.clear();
}

/** Retuns the final class and the order within that class. */
void QualificationFinal::setupNextFinal(pRunner r, int instance, int orderPlace, int numSharedPlaceNext) {
  storedInfo.resize(getNumClasses());

  pair<int, int> res = getNextFinal(instance, orderPlace, numSharedPlaceNext);
  int level = getLevel(instance);
  storedInfo[level].emplace_back(r, res.first, res.second);
}

/** Do internal calculations. */
void QualificationFinal::computeFinals() {
  if (storedInfo.empty())
    return;
  int nl = getNumLevels();
  for (int level = 1; level < nl; level++) {
    vector<int> placeCount(classDefinition.size(), 0);
    if (levels[level] == LevelInfo::RankSort) {
      vector< pair<int, int> > rankIx;
      vector< pair<int, int> > placeQueue;
      
      set<int> numClassCount;
      auto &si = storedInfo[level-1];
      if (si.empty())
        continue;
      rankIx.reserve(si.size());
      placeQueue.reserve(si.size());
      
      for (int i = 0; size_t(i) < si.size(); i++) {
        if (si[i].instance > 0) {
          int rank = si[i].r->getRanking();
          rankIx.emplace_back(rank, i);
          placeQueue.emplace_back(si[i].order, si[i].instance);
          numClassCount.insert(si[i].instance);
        }
      }
      int numClass = numClassCount.size();
      vector<int> rotmap;
      if (numClass == 2) {
        rotmap = { 1,0 };
      }
      else if (numClass == 3) {
        rotmap = { 2,0,1 };
      }
      else if (numClass == 4) {
        rotmap = { 3, 2, 0, 1 };
      }
      else if (numClass == 5) {
        rotmap = { 4, 3, 0, 2, 1 };
      }
      else if (numClass == 6) {
        rotmap = { 5, 4, 3, 0, 2, 1 };
      }
      else {
        rotmap.resize(numClass);
        for (int k = 0; k < numClass; k++)
          rotmap[k] = (k + 1) % numClass;
      }

      sort(rankIx.begin(), rankIx.end());
      sort(placeQueue.begin(), placeQueue.end());
      vector<int> placementOrder;
      for (size_t ix = 0; ix < placeQueue.size(); ix++) {
        placementOrder.push_back(ix);
      }
      for (size_t i = numClass; i + numClass <= placementOrder.size(); i += numClass * 2) {
        reverse(placementOrder.begin() + i, placementOrder.begin() + i + numClass);
      }
      reverse(placementOrder.begin(), placementOrder.end());
      /*
      vector<int> work;

      int ix = placeQueue.size() - 1;
      int iteration = 0;
      bool versionA = false;
      while (ix>=0) {
        work.clear();
        for (int i = 0; i < numClass && ix >= 0; i++) {
          work.push_back(ix--);
        }
        if (versionA) {
          if (work.size() == numClass)
            rotate(work.begin(), work.begin() + iteration, work.end());
          iteration = rotmap[iteration];
        }
        else {
          if (iteration % 2 == 1)
            reverse(work.begin(), work.end());
          iteration++;
        }

        for (size_t i = 1; i <= work.size(); i++) {
          placementOrder[ix + i] = work[work.size() - i];
        }
      }*/

      for (auto &rankRes : rankIx) {
        si[rankRes.second].instance = placeQueue[placementOrder.back()].second;
        si[rankRes.second].order = placeQueue[placementOrder.back()].first;
        placementOrder.pop_back();
      }
    }
  }

  for (auto & si : storedInfo) {
    for (auto &res : si) {
      storedInfoLookup[res.r->getId()] = &res;
    }
  }
}

/** Retuns the final class and the order within that class. */
pair<int, int> QualificationFinal::getNextFinal(int runnerId) const {
  auto res = storedInfoLookup.find(runnerId);
  if (res == storedInfoLookup.end()) {
    return make_pair(0, -1); 
  }
  
  return make_pair(res->second->instance, res->second->order);
}

void QualificationFinal::printScheme(oClass * cls, gdioutput &gdi) const {
  vector<wstring> cname;
//  vector<map<int, int> > targetCls(classDefinition.size());

  for (size_t i = 0; i < classDefinition.size(); i++) {
    pClass inst = cls->getVirtualClass(i + 1);
    cname.push_back(inst ? inst->getName() : lang.tl("Saknad klass"));
 //   for (auto d : classDefinition[i].qualificationMap) {
 //     targetCls[i].push_back(d.first);
 //   }
  }

  int ylimit = gdi.getHeight();
  gdi.pushY();
  for (size_t i = 0; i < classDefinition.size(); i++) {
    gdi.dropLine();

    gdi.addStringUT(1, itow(i+1) + L" " + cname[i]);
    bool rankingBased = false;
    vector<wstring> dst;

    for (int place = 1; place < 20; place++) {
      auto res = sourcePlaceToFinalOrder.find(make_pair(i + 1, place));
      
      if (res != sourcePlaceToFinalOrder.end()) {
        if (classDefinition[res->second.first - 1].rankLevel)
          rankingBased = true;
        dst.push_back(itow(place) + L". ➞ " + cname[res->second.first - 1]);
      }
      else break;
    }

    if (rankingBased) {
      gdi.addString("", 0, L"X går vidare, klass enligt ranking#" +  itow(dst.size()));
    }
    else {
      for (auto &c : dst) {
        gdi.addStringUT(0, c);
      }
    }

    if (gdi.getCY() > ylimit) {
      gdi.newColumn();
      gdi.popY();
      gdi.pushX();

    }
  }

  gdi.refresh();
}
