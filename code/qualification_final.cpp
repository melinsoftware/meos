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

pair<int, int> QualificationFinal::getPrelFinalFromPlace(int instance, int orderPlace, int numSharedPlaceNext) {
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
          ++numExtraAssigned[ans.first]; // More than specified
          return ans;
        }
      }

      auto ans = res->second;
      ans.second += iter;
      if (iter > 0)
        ++numExtraAssigned[ans.first]; // More than specified

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
  if (instance > 0 && classDefinition[instance].level > classDefinition[instance - 1].level)
    return false; // Second level
  return classDefinition[instance].qualificationMap.empty() &&
         classDefinition[instance].numTimeQualifications == 0 &&
    classDefinition[instance].extraQualification == QFClass::ExtraQualType::None;
}

void QualificationFinal::getBaseClassInstances(set<int> &base) const {
  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (noQualification(k))
      base.insert(k+1);
    else break;
  }

}

void QualificationFinal::exportXML(const wstring& file) const {
  xmlparser xml;
  xml.openOutputT(file.c_str(), false, "QualificationRules");
  int cLevel = -1;

  for (int j = 0; j < classDefinition.size(); j++) {
    int level = getLevel(j+1);
    if (level != cLevel) {
      if (cLevel >= 0)
        xml.endTag();

      cLevel = level;
      if (classDefinition[j].rankLevel)
        xml.startTag("Level", "distribution", "Ranking");
      else
        xml.startTag("Level");
    }

    vector<wstring> pv;
    pv.emplace_back(L"name");
    pv.emplace_back(classDefinition[j].name.empty() ? lang.tl("Kval") + itow(j + 1) : classDefinition[j].name);
    pv.emplace_back(L"id");
    pv.emplace_back(itow(j + 1));
    xml.startTag("Class", pv);

    vector<pair<string, wstring>> psv(2);;
    psv[0].first = "id";
    psv[1].first = "place";
 
    for (auto& qmap : classDefinition[j].qualificationMap) {
      psv[0].second = itow(qmap.first);
      psv[1].second = itow(qmap.second);
      xml.write("Qualification", psv, L"");
    }

    if (classDefinition[j].numTimeQualifications > 0) {
      psv[0].first = "time";
      psv[0].second = itow(classDefinition[j].numTimeQualifications);
      psv.resize(1);
      xml.write("Qualification", psv, L"");
    }

    if (classDefinition[j].extraQualification != QFClass::ExtraQualType::None) {
      psv[0].first = "type";
      switch (classDefinition[j].extraQualification) {
      case QFClass::ExtraQualType::All:
        psv.resize(1);
        psv[0].second = L"All";
        break;
      case QFClass::ExtraQualType::NBest:
        psv.resize(2);
        psv[0].second = L"Best";
        psv[1].first = "number";
        psv[1].second = itow(classDefinition[j].extraQualData);
        break;
      case QFClass::ExtraQualType::TimeLimit:
        psv.resize(2);
        psv[0].second = L"Time";
        psv[1].first = "limit";
        psv[1].second = formatTimeMS(classDefinition[j].extraQualData, false);
        break;
      }
      xml.write("Remaining", psv, L"");
    }

    xml.endTag();
  }
  xml.closeOut();
}

void QualificationFinal::importXML(const wstring &file) {
  xmlparser xml;
  xml.read(file);

  auto qr = xml.getObject("QualificationRules");
  xmlList levels;
  qr.getObjects("Level", levels);
  map<int, int> idToIndex;
  map<int, set<int> > qualificationRelations;
  int numBaseLevels = 0;
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

      cls.getObjectString("name", name);
      if (name.empty())
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

      
      xmlobject remaining = cls.getObject("Remaining");

      if (rules.empty() && !remaining)
        numBaseLevels = 1; // Instance zero is not used as qualification,

      idToIndex[classId] = classDefinition.size() + numBaseLevels;
      classDefinition.emplace_back();
      classDefinition.back().level = iLevel;
      if (remaining) {
        wstring rtype;
        remaining.getObjectString("type", rtype);
        if (rtype == L"All") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::All;
        }
        else if (rtype == L"Best") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::NBest;
          classDefinition.back().extraQualData = remaining.getObjectInt("number");
          if (classDefinition.back().extraQualData > 10000 || classDefinition.back().extraQualData < 0)
            classDefinition.back().extraQualData = 0;
        }
        else if (rtype == L"Time") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::TimeLimit;
          wstring wt;
          remaining.getObjectString("limit", wt);
          classDefinition.back().extraQualData = convertAbsoluteTimeMS(wt);
          if (classDefinition.back().extraQualData == NOTIME || classDefinition.back().extraQualData < 0)
            classDefinition.back().extraQualData = 0;

        }
      }

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
          int numTime = qf.getObjectInt("time");
          if (numTime > 0) 
            classDefinition.back().numTimeQualifications += numTime;
          else if (qf.got("time")) {
            throw meosException(L"Empty time qualification for " + name);
          }
          else throw meosException(L"Unknown classification rule for " + name);
        }
      }
    }
  }
  initgmap(true);
}

void QualificationFinal::init(const wstring &def) {
  serializedFrom = def;
  vector<wstring> races, rtdef, rdef; 
  split(def, L"|", races);
  classDefinition.resize(races.size());
  bool valid = true;
  bool first = true;
  for (size_t k = 0; k < races.size(); k++) {
    if (races[k].size() > 0 && races[k][0] == '@') {
      // Explicit level, since 4.0
      classDefinition[k].level = _wtoi(races[k].c_str() + 1);
      int mx = 1;
      while (mx < races[k].size() && races[k][mx] != '@')
        mx++;
      int mx2 = mx + 1;
      while (mx2 < races[k].size() && races[k][mx2] != '@')
        mx2++; 

      if (mx2 >= races[k].size() || classDefinition[k].level > k) {
        valid = false;
        break;
      }
      races[k][mx2] = 0;
      classDefinition[k].name = races[k].c_str() + mx + 1;
      races[k] = races[k].substr(mx2 + 1);
    }
    bool rankLevel = false;
    if (races[k].size() > 0 && races[k][0] == 'R') {
      rankLevel = true;
      races[k] = races[k].substr(1);
    }
    
    if (races[k].size() > 0 && QFClass::deserialType(races[k][0]) != QFClass::ExtraQualType::None) {
      first = false;
      classDefinition[k].extraQualification = QFClass::deserialType(races[k][0]);
      if (classDefinition[k].extraQualification == QFClass::ExtraQualType::NBest ||
        classDefinition[k].extraQualification == QFClass::ExtraQualType::TimeLimit) {
        classDefinition[k].extraQualData = _wtoi(races[k].c_str() + 1);
      }
      int end = 1;
      while (end < races[k].size() && races[k][end-1] != ';')
        end++;
      races[k] = races[k].substr(end);
    }
    split(races[k], L"T", rtdef);
    classDefinition[k].qualificationMap.clear();
    classDefinition[k].numTimeQualifications = 0;
    classDefinition[k].rankLevel = rankLevel;

    if (rtdef.empty())
      continue; // Remaining qualified

    first = false;

    split(rtdef[0], L";", rdef);
    bool thisValid = rdef.size()%2 == 0 &&
         (rdef.size() > 0 || (rtdef.size() == 2 && !rtdef[1].empty()));
    
    if (!thisValid)
      continue;

    for (size_t j = 0; j < rdef.size(); j+=2) {
      int src = _wtoi(rdef[j].c_str());
      if (src > k || src <= 0) {
        thisValid = false;
        break;
      }
      const wstring &rd = rdef[j + 1];
      int d1 = _wtoi(rd.c_str());
      if (d1 < 1 || d1>1000) {
        thisValid = false;
        break;
      }
      int d2 = d1;
      size_t range = rd.find_first_of('-', 0);
      if (range < rd.size())
        d2 = _wtoi(rd.c_str()+range+1);

      if (d1 > d2) {
        thisValid = false;
        break;
      }

      while (d1 <= d2) {
        classDefinition[k].qualificationMap.emplace_back(src, d1);
        d1++;
      }
    }

    if (!thisValid)
      classDefinition[k].qualificationMap.clear();

    if (rtdef.size() > 1) {
      int numTime = _wtoi(rtdef[1].c_str());
      classDefinition[k].numTimeQualifications = numTime;
    }
  }

  if (!valid)
    classDefinition.clear();

  initgmap(false);
}


void QualificationFinal::setClasses(const vector<QFClass>& def) {
  classDefinition = def;
  initgmap(true);
  wstring tmp;
  encode(tmp);
}

wstring QualificationFinal::validName(const wstring& name) {
  wstring out;
  out.reserve(name.size());
  for (int j = 0; j < name.length(); j++)
    if (isValidNameChar(name[j]))
      out.push_back(name[j]);

  return out;
}

bool QualificationFinal::isValidNameChar(wchar_t c) {
  if (c == 0 || c == '|' || c == '@')
    return false;

  return true;
}

void QualificationFinal::encode(wstring &output) const {
  output.clear();

  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (k > 0)
      output.append(L"|");

    output.append(L"@"+ itow(getLevel(k+1)) + L"@" + validName(classDefinition[k].name) + L"@");
    
    if (classDefinition[k].rankLevel)
      output.append(L"R");

    output.append(classDefinition[k].serialExtra());

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

    if (classDefinition[k].numTimeQualifications > 0) {
      output.append(L"T");
      output.append(itow(classDefinition[k].numTimeQualifications));
    }
  }

  serializedFrom = output;
}

wstring QFClass::serialExtra() const {
  if (extraQualification == ExtraQualType::All)
    return L"A;";
  else if (extraQualification == ExtraQualType::TimeLimit)
    return L"L" + itow(extraQualData) + L";";
  else if (extraQualification == ExtraQualType::NBest)
    return L"B" + itow(extraQualData) + L";";
  return L"";
}

int QFClass::getMinQualInst() const {
  if (qualificationMap.empty())
    return -1;
  int ret = 1024;
  for (auto& cp : qualificationMap)
    ret = min(ret, cp.first);
  return ret;
}

void QualificationFinal::initgmap(bool check) {
  level2DefningInstance.clear();
  sourcePlaceToFinalOrder.clear();
  levels.clear();
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
  return instance > 0 && getLevel(instance) + 1 == getNumLevels();
}

int QualificationFinal::getNumLevels() const {
  return getLevel(classDefinition.size()) + 1;
}

int QualificationFinal::getMinInstance(const int levelIn) const {
  int level = levelIn;
  auto res = level2DefningInstance.find(levelIn);
  if (res != level2DefningInstance.end())
    return res->second;

  int minInst = classDefinition.size() - 1;
  for (int i = classDefinition.size() - 1; i >= 0; i--) {
    if (i == 0 && minInst == 1)
      break; // No need to include base instance.
    int thisLevel = getLevel(i+1);
    if (thisLevel > level) {
      int minQualInst = classDefinition[i].getMinQualInst();
      if (minQualInst >= 0) {
        // If there are direct qualification to this level from an 
        // earlier level, we must start from that level.
        int qualLevel = getLevel(minQualInst+1);
        if (qualLevel < level) {
          level = qualLevel;
          i = classDefinition.size(); // Restart
        }
      }
    }
    else if (thisLevel == level)
      minInst = i;
  }

  level2DefningInstance[levelIn] = minInst;
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
  numExtraAssigned.clear();
}

/** Retuns the final class and the order within that class. */
void QualificationFinal::provideQualificationResult(pRunner r, int instance, int orderPlace, int numSharedPlace) {
  storedInfo.resize(getNumClasses());

  pair<int, int> res = getPrelFinalFromPlace(instance, orderPlace, numSharedPlace);
  int level = getLevel(instance);
  storedInfo[level].emplace_back(r, res.first, res.second);
}

void QualificationFinal::provideUnqualified(int level, pRunner r) {
  storedInfo.resize(getNumClasses());
  storedInfo[level].emplace_back(r, -1, storedInfo[level].size());
}

/** Do internal calculations. */
void QualificationFinal::computeFinals() {
  if (storedInfo.empty())
    return;
  int nl = getNumLevels();

  // Handle time qualifications
  vector<pair<int, ResultInfo*>> timeToIx;
  set<pair<int, ResultInfo*>> remainingTimeQualified;
  int lastLevel = -1;
  vector<int> levelClasses;

  auto distributeRemaning = [&]() {
    if (levelClasses.empty())
      return;
    int cIx = 0;
    for (auto& tq : remainingTimeQualified) {
      tq.second->instance = levelClasses[cIx];
      tq.second->order = 100 + cIx;
      cIx++;
      if (cIx >= levelClasses.size())
        cIx = 0;
    }
  };

  for (int j = 1; j < classDefinition.size(); j++) {
    int level = getLevel(j + 1) - 1;
    if (level != lastLevel) {
      distributeRemaning();
      levelClasses.clear();
      remainingTimeQualified.clear();
      lastLevel = level;
    }
    levelClasses.push_back(j + 1);
    int NT = classDefinition[j].numTimeQualifications;
    auto res = numExtraAssigned.find(j + 1);
    if (res != numExtraAssigned.end()) {
      NT -= res->second; // Already assigned from (duplicate) places
    }
    for (int i = 0; i < NT; i++) {
      timeToIx.clear();
      int order = 0;
      // Select the best time from unqualified competitors
      if (level < storedInfo.size()) {
        for (auto& res : storedInfo[level]) {
          if (res.instance == 0) {
            int t = res.r->getRunningTime(false);
            if (timeToIx.empty() || t <= timeToIx.front().first) {
              // Select the current best time

              if (!timeToIx.empty() && t < timeToIx.front().first)
                timeToIx.clear(); // Found better time

              timeToIx.emplace_back(t, &res);
            }
          }
          else {
            order = std::max(res.order, order);
          }
        }
      }
      if (!timeToIx.empty()) {
        timeToIx[0].second->instance = j + 1;
        timeToIx[0].second->order = ++order;
        for (int j = 1; j < timeToIx.size(); j++) {
          // Qualified by time, but not yet in a race (tie position)
          remainingTimeQualified.insert(timeToIx[j]);
        }
      }
    }
  }
  // Allow both to advance on tie
  distributeRemaning();

  for (int j = 1; j < classDefinition.size(); j++) {
    int level = getLevel(j + 1) - 1;

    if (classDefinition[j].extraQualification == QFClass::ExtraQualType::All) {
      // All remaining
      for (auto& si : storedInfo[level]) {
        if (si.instance <= 0)
          si.instance = j + 1;
      }
    }
    else if (classDefinition[j].extraQualification == QFClass::ExtraQualType::NBest) {
      // N best competitors
      int count = 0;
      for (auto& si : storedInfo[level]) {
        if (si.instance <= 0) {
          si.instance = j + 1;
          if (++count >= classDefinition[j].extraQualData)
            break;
        }
      }
    }
    else if (classDefinition[j].extraQualification == QFClass::ExtraQualType::TimeLimit) {
      // Fixed time limit
      for (auto& si : storedInfo[level]) {
        if (si.instance == 0 && 
          si.r->getStatus() == StatusOK &&
          si.r->getRunningTime(false) < classDefinition[j].extraQualData){
          si.instance = j + 1;
        }
      }
    }
  }


  for (int level = 1; level < nl; level++) {
    vector<int> placeCount(classDefinition.size(), 0);
    if (levels[level] == LevelInfo::RankSort) {
      vector<pair<int, int>> rankIx;
      vector<pair<int, int>> placeQueue;
      
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
  
  return make_pair(max(res->second->instance, 0), res->second->order);
}

void QualificationFinal::printScheme(const oClass& cls, gdioutput &gdi) const {
  vector<wstring> cname;

  for (size_t i = 0; i < classDefinition.size(); i++) {
    pClass inst = cls.getVirtualClass(i + 1);
    cname.push_back(inst ? inst->getName() : lang.tl("Saknad klass"));
  }

  int ylimit = gdi.getHeight();
  gdi.pushY();
  for (size_t i = 0; i < classDefinition.size(); i++) {
    gdi.dropLine();
    gdi.addStringUT(1, itow(i+1) + L" " + cname[i]);
    
    if (getLevel(i + 1) == 0) {
      //gdi.addStringUT(italicText, "Kval").setColor(GDICOLOR::colorDarkGrey);
    }
    else {
      gdi.addString("", italicText, classDefinition[i].getQualInfo()).setColor(GDICOLOR::colorDarkGreen);
    }
    gdi.dropLine(0.2);
    int rankingBased = 0;
    vector<wstring> dst;
    int misCntLimit = 0;
    for (int place = 1; place < 100; place++) {
      auto res = sourcePlaceToFinalOrder.find(make_pair(i + 1, place));
      
      if (res != sourcePlaceToFinalOrder.end()) {
        int level = getLevel(res->second.first);
        if (levels[level] == LevelInfo::RankSort)
          ++rankingBased;
        else
          dst.push_back(itow(place) + L". ➞ " + cname[res->second.first - 1]);
      }
      else {
        if (++misCntLimit > 10)
          break;
      }
    }

    if (rankingBased) {
      gdi.addString("", 0, L"  X går vidare, klass enligt ranking#" +  itow(rankingBased));
    }
    for (auto &c : dst) {
      gdi.addStringUT(0, L"  " + c);
    }
    
    if (gdi.getCY() > ylimit) {
      gdi.newColumn();
      gdi.popY();
      gdi.pushX();
    }
  }

  gdi.refresh();
}

wstring QFClass::getQualInfo() const {
  if (extraQualification == ExtraQualType::All)
    return L"Alla övriga";
  else if (extraQualification == ExtraQualType::NBest)
    return L"X bästa#" + itow(extraQualData);
  else if (extraQualification == ExtraQualType::TimeLimit)
    return L"Tidsgräns X#" + formatTimeMS(extraQualData, false);

  int nq = qualificationMap.size() + numTimeQualifications;
  if (nq > 0 && numTimeQualifications == 0)
    return L"X kvalificerade#" + itow(nq);
  else if (nq > 0)
    return L"X kvalificerade#" + itow(qualificationMap.size()) + L" + " + itow(numTimeQualifications);
  
  return L"Ingen";
}


bool QualificationFinal::hasRemainingClass() const {
  for (auto& c : classDefinition) {
    if (c.extraQualification != QFClass::ExtraQualType::None)
      return true;
  }
  return false;
}
