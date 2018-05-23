#include "StdAfx.h"

#include "qualification_final.h"

#include "meos_util.h"
#include "meosexception.h"
#include <set>
#include <algorithm>
#include "xmlparser.h"

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
  vector <wstring> races, rtdef, rdef; 
  split(def, L"|", races);
  classDefinition.resize(races.size());
  bool valid = true;
  bool first = true;
  for (size_t k = 0; k < races.size(); k++) {
    split(races[k], L"T", rtdef);
    classDefinition[k].qualificationMap.clear();
    classDefinition[k].timeQualifications.clear();

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

  for (int ix = 0; ix < (int)classDefinition.size(); ix++) {
    auto &c = classDefinition[ix];

    for (int k = 0; k < (int)c.qualificationMap.size(); k++) {
      const pair<int, int> &sd = c.qualificationMap[k];
      if (check && sourcePlaceToFinalOrder.count(sd))
        throw meosException(L"Inconsistent qualification rule, X#" + c.name + + L"/" + itow(sd.first));

      sourcePlaceToFinalOrder[sd] = make_pair(ix+1, k);
    }
  }
}

int QualificationFinal::getHeatFromClass(int finalClassId, int baseClassId) const {
  if (baseClassId == baseId) {
    int fci = (finalClassId - baseId) / maxClassId;

    if (fci * maxClassId + baseClassId == finalClassId)
      return fci;
  }

  return 0;
}
