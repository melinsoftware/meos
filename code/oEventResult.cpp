/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "oDataContainer.h"

#include "meosException.h"
#include "TabBase.h"
#include "meos.h"
#include "meos_util.h"
#include "generalresult.h"
#include "metalist.h"
#include "TabList.h"
#include "listeditor.h"

template<typename T> struct ResultCalcData {
  int groupId;
  int score;
  bool operator<(const ResultCalcData<T> &c) const {
    if (groupId != c.groupId)
      return groupId < c.groupId;
    else
      return score < c.score;
  }
  T *dst;

  ResultCalcData() {}
  ResultCalcData(int g, int s, T* d) : groupId(g), score(s), dst(d) {}
};

template<typename T, typename Apply> void calculatePlace(vector<ResultCalcData<T>> &data, Apply apply) {
  int groupId = -1;
  int cPlace = 0, vPlace = 0, cScore = 0;
  bool invalidClass = false;
  bool useResults = true;
  sort(data.begin(), data.end());
  for (auto &it : data) {
    // Start new "class"
    if (groupId != it.groupId) {
      groupId = it.groupId;
      cPlace = 0;
      vPlace = 0;
      cScore = 0;
      pClass cls = it.dst->getClassRef(true);
      useResults = true;// cls ? cls->getNoTiming() == false : true;
      invalidClass = cls ? cls->getClassStatus() != oClass::ClassStatus::Normal : false;
    }

    if (invalidClass) {
      apply(it, 0);
    }
    else {
      int tPlace = 0;
      if (it.score > 0) {
        cPlace++;
        if (it.score > cScore)
          vPlace = cPlace;

        cScore = it.score;
        if (useResults)
          tPlace = vPlace;
      }
      else
        tPlace = 0;

      apply(it, tPlace);
    }
  }
}

void oEvent::calculateSplitResults(int controlIdFrom, int controlIdTo) {
  oRunnerList::iterator it;

  for (it=Runners.begin(); it!=Runners.end(); ++it) {
    int st = 0;
    if (controlIdFrom > 0 && controlIdFrom != oPunch::PunchStart) {
      RunnerStatus stat;
      it->getSplitTime(controlIdFrom, stat, st);
      if (stat != StatusOK) {
        it->tempStatus = stat;
        it->tempRT = 0;
        continue;
      }
    }
    if (controlIdTo == 0 || controlIdTo == oPunch::PunchFinish) {
      it->tempRT = max(0, it->FinishTime - (st + it->tStartTime) );
      if (it->tempRT > 0)
        it->tempRT += it->getTimeAdjustment();
      it->tempStatus = it->tStatus;
    }
    else {
      int ft = 0;
      it->getSplitTime(controlIdTo, it->tempStatus, ft);
      if (it->tempStatus==StatusOK && it->tStatus > StatusOK)
        it->tempStatus=it->tStatus;

      it->tempRT = max(0, ft - st);
    }
  }

  Runners.sort(oRunner::sortSplit);
  int cClassId=-1;
  int cPlace=0;
  int vPlace=0;
  int cTime=0;

  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getClassId(true)!=cClassId){
      cClassId=it->getClassId(true);
      cPlace=0;
      vPlace=0;
      cTime=0;
      it->Class->tLegLeaderTime=9999999;
    }

    if (it->tempStatus==StatusOK) {
      cPlace++;

      if (it->Class)
        it->Class->tLegLeaderTime=min(it->tempRT, it->Class->tLegLeaderTime);

      if (it->tempRT>cTime)
        vPlace=cPlace;

      cTime=it->tempRT;

      it->tPlace.update(*this, vPlace, false); // XXX User other result container
    }
    else
      it->tPlace.update(*this, 0, false);
  }
}

void oEvent::calculateResults(const set<int> &classes, ResultType resultType, bool includePreliminary) const {
  static bool resultCalculationLock = false; 

  if (resultCalculationLock) {
    assert(resultType == ResultType::ClassResultDefault || resultType == ResultType::TotalResultDefault);
    return;
  }

  if (resultType == ResultType::PreliminarySplitResults) {
    computePreliminarySplitResults(classes);
    return;
  }

  const bool defaultResult = resultType == ResultType::ClassResultDefault || resultType == ResultType::TotalResultDefault;
  const bool standardResults = resultType == ResultType::ClassResult || resultType == ResultType::TotalResult;
  
  const bool individualResults = resultType == ResultType::ClassResult;
  const bool totalResults = resultType == ResultType::TotalResult || resultType == ResultType::TotalResultDefault;
  const bool courseResults = resultType == ResultType::CourseResult;
  const bool classCourseResults = resultType == ResultType::ClassCourseResult;
  const bool specialResult = classCourseResults || courseResults;

  bool all = classes.empty() || specialResult;
  vector<pair<string, vector<oRunner *>>> runnersByResultModule;
  set<int> clsWithResultModule;
  map<int, int> classToResultModule;
  set<int> rgClasses;
    
  map<string, int> resultModuleToIndex;
  for (auto &cls : Classes) {
    if (!cls.isRemoved() && (all || classes.count(cls.getId()))) {

      if (cls.isRogaining() && cls.getResultModuleTag().empty()) 
        rgClasses.insert(cls.getId());
      
      string t;        
      if (!defaultResult)
        t = cls.getResultModuleTag();

      if (!t.empty() && !specialResult) {
        if (!resultModuleToIndex.count(t)) {
          resultModuleToIndex[t] = runnersByResultModule.size();
          pair<string, vector<oRunner *>> empty(t, {});
          runnersByResultModule.emplace_back(empty);
        }
        classToResultModule[cls.getId()] = resultModuleToIndex[t];
      }
      else {
        classToResultModule[cls.getId()] = -1;

        if (!t.empty()) // For special resultr
          clsWithResultModule.insert(cls.getId());
      }
    }
  }

  vector<const oRunner *> runners;
  {
    vector<pRunner> runnersCls;
    if (all)
      getRunners({}, runnersCls);
    else
      getRunners(classes, runnersCls);

    runners.reserve(runnersCls.size());
    bool resOK = true;

    for (auto it : runnersCls) {
      oRunner &r = *it;
      int cid = r.getClassId(true);
      auto c = classToResultModule.find(cid);
      if (c != classToResultModule.end() && c->second != -1) {
        runnersByResultModule[c->second].second.push_back(&r);
      }
      runners.push_back(&r);
      
      if (resOK) {
        if (courseResults && r.tCoursePlace.isOld(*this))
          resOK = false;
        else if (classCourseResults && r.tCourseClassPlace.isOld(*this))
          resOK = false;
        else if (totalResults && r.tTotalPlace.isOld(*this))
          resOK = false;
        else if (r.tPlace.isOld(*this))
          resOK = false;
      }
    }

    if (resOK)
      return;
  }
  // Reset computed status/time
  
  bool useComputedResult = false;
  if (specialResult && !clsWithResultModule.empty()) {
    // Calculate standard results and setup computed time/status. This is for coursewise result etc.
    calculateResults(clsWithResultModule, oEvent::ResultType::ClassResult, includePreliminary);
    runnersByResultModule.clear();
    useComputedResult = true;
  }
  else {
    // Reset leader times
    for (auto &cls : Classes) {
      if (!cls.isRemoved() && (all || classes.count(cls.getId()))) {
        for (unsigned leg = 0; leg < cls.getNumStages(); leg++)
          cls.getLeaderInfo(oClass::AllowRecompute::No,leg).resetComputed(oClass::LeaderInfo::Type::Leg);
      }
    }

    for (auto r : runners) {
      r->tComputedPoints = -1;
      r->tComputedTime = r->getRunningTime(false);
      r->tComputedStatus = r->getStatus();
    }
  }

  calculateRunnerResults(resultType, rgClasses, runners, useComputedResult, includePreliminary);

  if (!runnersByResultModule.empty()) {
    oEvent::ResultType otherType = totalResults ? oEvent::ResultType::ClassResult : oEvent::ResultType::TotalResult;
    calculateRunnerResults(otherType, rgClasses, runners, false, includePreliminary);
    
    resultCalculationLock = true;
    try {
      for (auto &resCalc : runnersByResultModule) {
        wstring dmy;
        auto &ge = oe->getGeneralResult(resCalc.first, dmy);
        ge->calculateIndividualResults(resCalc.second, true, oListInfo::ResultType::Classwise, false, 0);

        for (pRunner r : resCalc.second) {
          r->updateComputedResultFromTemp();
          r->tPlace.update(*oe, r->getTempResult().getPlace(), false);
          if (r->tComputedStatus == StatusOK && r->tComputedTime>0) {
            pClass cls = r->getClassRef(true);
            cls->getLeaderInfo(oClass::AllowRecompute::No, 
                               cls->mapLeg(r->getLegNumber())).updateComputed(r->tComputedTime, oClass::LeaderInfo::Type::Leg);
          }
        }
      }
    }
    catch (...) {
      resultCalculationLock = false;
      throw;
    }
    resultCalculationLock = false;
  }
}

void oEvent::calculateRunnerResults(ResultType resultType,
                                    const set<int> &rgClasses,
                                    vector<const oRunner*> &runners, 
                                    bool useComputedResult,
                                    bool includePreliminary) const {
  
  const bool individualResults = resultType == ResultType::ClassResult;
  const bool totalResults = resultType == ResultType::TotalResult || resultType == ResultType::TotalResultDefault;
  const bool courseResults = resultType == ResultType::CourseResult;
  const bool classCourseResults = resultType == ResultType::ClassCourseResult;

  typedef ResultCalcData<const oRunner> DT;
  vector<DT> resData;
  resData.reserve(runners.size());
  int groupId, score;
  for (auto it : runners) {
    int clsId = it->getClassId(true);
    if (classCourseResults) {
      const pCourse crs = it->getCourse(false);
      groupId = it->getClassId(true) * 997 + (crs ? crs->getId() : 0);
    }
    else if (courseResults) {
      const pCourse crs = it->getCourse(false);
      groupId = crs ? crs->getId() : 0;
    }
    else {
      groupId = clsId * 100 + (it->tDuplicateLeg + 10 * it->tLegEquClass);
    }

    if (rgClasses.count(clsId)) {
      RunnerStatus st;
      if (totalResults)
        st = useComputedResult ? it->getStatusComputed() : it->getStatus();
      else
        st = it->getTotalStatus();

      if (st == StatusOK)
        score = numeric_limits<int>::max() - (3600 * 24 * 7 * max(1, 1 + it->getRogainingPoints(useComputedResult, totalResults)) - it->getRunningTime(false));
      else
        score = -1;
    }
    else if (!totalResults) {
      RunnerStatus st = useComputedResult ? it->getStatusComputed() : it->getStatus();
      if (st == StatusOK || (includePreliminary && st == StatusUnknown && it->FinishTime > 0))
        score = it->getRunningTime(useComputedResult) + it->getNumShortening() * 3600 * 24 * 8;
      else
        score = -1;
    }
    else {
      int tt = it->getTotalRunningTime(it->FinishTime, useComputedResult, true);
      RunnerStatus totStat = it->getTotalStatus();
      if (totStat == StatusOK || (includePreliminary && totStat == StatusUnknown && it->inputStatus == StatusOK) && tt > 0)
        score = tt;
      else
        score = -1;
    }

    resData.emplace_back(groupId, score, it);
  }

  bool useStdResultCtr = resultType == ResultType::ClassResultDefault || resultType == ResultType::TotalResultDefault;

  if (courseResults)
    calculatePlace(resData, [this, useStdResultCtr](DT &res, int value) {res.dst->tCoursePlace.update(*this, value, useStdResultCtr); });
  else if (classCourseResults)
    calculatePlace(resData, [this, useStdResultCtr](DT &res, int value) {res.dst->tCourseClassPlace.update(*this, value, useStdResultCtr); });
  else if (totalResults)
    calculatePlace(resData, [this, useStdResultCtr](DT &res, int value) {res.dst->tTotalPlace.update(*this, value, useStdResultCtr); });
  else
    calculatePlace(resData, [this, useStdResultCtr](DT &res, int value) {res.dst->tPlace.update(*this, value, useStdResultCtr); });
}

bool oEvent::calculateTeamResults(vector<const oTeam*> &teams, int leg, ResultType resType) {
  oTeamList::iterator it;

  bool hasRunner;
  if (resType == ResultType::TotalResult)
    hasRunner = sortTeams(ClassTotalResult, leg, true, teams);
  else
    hasRunner = sortTeams(ClassDefaultResult, leg, true, teams);

  if (!hasRunner)
    return false;

  int cClassId = 0;
  int cPlace = 0;
  int vPlace = 0;
  int cTime = 0;
  bool invalidClass = false;
  oTeam::ComputedLegResult res;

  for (auto it : teams) {
    if (it->isRemoved())
      continue;

    if (it->Class && it->Class->Id != cClassId) {
      cClassId = it->Class->Id;
      cPlace = 0;
      vPlace = 0;
      cTime = 0;
      invalidClass = it->Class->getClassStatus() != oClass::ClassStatus::Normal;
    }

    int sleg;
    if (leg == -1)
      sleg = it->Runners.size() - 1;
    else
      sleg = leg;

    if (size_t(leg) >= it->Runners.size())
      continue;

    int p;
    if (invalidClass) {
      p = 0;
    }
    else if (it->tmpCachedStatus == StatusOK) {
      cPlace++;

      if (it->tmpSortTime > cTime)
        vPlace = cPlace;

      cTime = it->tmpSortTime;

      p = vPlace;
    }
    else {
      p = 0;
    }

    bool tmpDefaultResult = resType == ResultType::ClassResultDefault || resType == ResultType::TotalResultDefault;
    if (resType == ResultType::TotalResult || resType == ResultType::TotalResultDefault) {
      it->getTeamPlace(sleg).totalP.update(*this, p, tmpDefaultResult);
    }
    else {
      it->getTeamPlace(sleg).p.update(*this, p, tmpDefaultResult);
      res.version = tmpDefaultResult ? -1 : dataRevision;
      res.status = it->tmpCachedStatus;
      res.time = it->tmpDefinedTime;
      it->setComputedResult(sleg, res);
    }
  }
  return true;
}

void oEvent::calculateTeamResults(const set<int> &classSelection, ResultType resType) {
  set<int> classSelectionC;
  set<int> classSelectionM;
  bool allC = resType == ResultType::ClassResultDefault || resType == ResultType::TotalResult;
  bool totalResult = resType == ResultType::TotalResult;
  if (classSelection.empty()) {
    for (auto &c : Classes) {
      if (!c.isRemoved()) {
        if (allC || c.getResultModuleTag().empty())
          classSelectionC.insert(c.getId());
        else
          classSelectionM.insert(c.getId());
      }
    }
  }
  else {
    for (int id : classSelection) {
      pClass c = getClass(id);
      if (c) {
        if (c->getResultModuleTag().empty())
          classSelectionC.insert(id);
        else
          classSelectionM.insert(id);
      }
    }
  }

  vector<const oTeam *> teams;
  vector<oTeam *> teamsMod;
  bool resultOK = true;
  teams.reserve(Teams.size());
  for (auto &t : Teams) {
    if (t.isRemoved())
      continue;

    if (classSelectionC.count(t.getClassId(true))) {
      if (resultOK && !t.isResultUpdated(totalResult))
        resultOK = false;
      teams.push_back(&t);
    }
    else if (classSelectionM.count(t.getClassId(true))) {
      if (resultOK && !t.isResultUpdated(totalResult))
        resultOK = false;
      teamsMod.push_back(&t);
    }
  }

  if (resultOK)
    return;

  for (int i = 0; i < maxRunnersTeam; i++) {
    if (!calculateTeamResults(teams, i, resType))
      break;
  }

  if (!teamsMod.empty())
    calculateModuleTeamResults(classSelectionM, teamsMod);
}

void oEvent::calculateTeamResults(const vector<pTeam> &teams, ResultType resultType) {
  bool allC = resultType == ResultType::ClassResultDefault || resultType == ResultType::TotalResultDefault;
  bool totalResult = resultType == ResultType::TotalResult || resultType == ResultType::TotalResultDefault;

  set<int> cls, clsMod;
  for (pTeam t : teams)
    cls.insert(t->getClassId(true));

  if (!allC) {
    for (int id : cls) {
      pClass c = getClass(id);
      if (c) {
        if (!c->getResultModuleTag().empty())
          clsMod.insert(id);
      }
    }
  }
  vector<const oTeam *> teamsStd;
  vector<oTeam *> teamsMod;
  bool resultOK = true;
  teamsStd.reserve(teams.size());
  if (!clsMod.empty())
    teamsMod.reserve(teams.size());
  for (auto &t : teams) {
    if (resultOK && !t->isResultUpdated(totalResult))
      resultOK = false;

    if (clsMod.count(t->getClassId(true)))
      teamsMod.push_back(t);
    else
      teamsStd.push_back(t);
  }

  if (resultOK)
    return;

  for (int i = 0; i < maxRunnersTeam; i++) {
    if (!calculateTeamResults(teamsStd, i, resultType))
      break;
  }

  if (!teamsMod.empty())
    calculateModuleTeamResults(clsMod, teamsMod);
}

void oEvent::calculateModuleTeamResults(const set<int> &cls, vector<oTeam *> &teams) {
  map<int, string> cls2Mod;
  set<int> rgClasses;
  for (int id : cls) {
    pClass c = getClass(id);
    if (c->isRogaining())
      rgClasses.insert(id);
    for (unsigned leg = 0; leg < c->getNumStages(); leg++) {
      c->getLeaderInfo(oClass::AllowRecompute::No, leg).resetComputed(oClass::LeaderInfo::Type::Total);
      c->getLeaderInfo(oClass::AllowRecompute::No, leg).resetComputed(oClass::LeaderInfo::Type::TotalInput);
    }

    cls2Mod[c->Id] = c->getResultModuleTag();
  }
  map<string, vector<oTeam*>> teamByResultModule;
  for (auto t : teams) {
    teamByResultModule[cls2Mod[t->getClassId(true)]].push_back(t);
  }
  typedef ResultCalcData<const oTeam> DT;
  typedef ResultCalcData<const oRunner> DR;

  vector<DR> legResultsData;
  legResultsData.reserve(Runners.size());

  for (auto &resCalc : teamByResultModule) {
    wstring dmy;
    
    auto &ge = oe->getGeneralResult(resCalc.first, dmy);
    ge->calculateTeamResults(resCalc.second, true, oListInfo::ResultType::Classwise, false, 0);
    vector<DT> resData;
    resData.reserve(resCalc.second.size());

    for (pTeam t : resCalc.second) {
      pClass teamClass = t->getClassRef(true);
      
      t->tComputedTime = t->getTempResult().getRunningTime();
      t->tComputedPoints = t->getTempResult().getPoints();
      t->tComputedStatus = t->getTempResult().getStatus();
      bool ok = true;
      int timeAcc = 0, timePar = 0;

      int totScore = -1;
      int clsId = t->getClassId(true);
      if (t->tComputedStatus == StatusOK && t->inputStatus == StatusOK) {
        if (rgClasses.count(clsId))
          totScore = numeric_limits<int>::max() - (7 * 24 * 3600 * max(1, (1 + t->getRogainingPoints(true, true))) - (t->tComputedTime + t->inputTime));
        else
          totScore = t->tComputedTime + t->inputTime;
      }

      resData.emplace_back(clsId, totScore, t);

      for  (int i = 0; i < t->getNumRunners(); i++) {
        t->getTeamPlace(i).p.update(*this, t->getTempResult().getPlace(), false);
        t->getTeamPlace(i).totalP.update(*this, t->getTempResult().getPlace(), false);
        oTeam::ComputedLegResult res;
        res.version = dataRevision;
        int legTime = 0;
        bool lastLeg = i + 1 == t->getNumRunners();

        if (t->Runners[i]) {
          res.status = t->Runners[i]->getTempResult().getStatus();
          res.time = t->Runners[i]->getTempResult().getRunningTime();
          legTime = res.time;

          auto lt = teamClass->getLegType(i);
          if (res.status == StatusOK) {
            if (lt == LTParallel || lt == LTParallelOptional)
              timePar = max(timePar, res.time);
            else if (lt != LTIgnore && lt != LTExtra)
              timePar = res.time;

            teamClass->getLeaderInfo(oClass::AllowRecompute::No, i).updateComputed(res.time, oClass::LeaderInfo::Type::Leg);
          }
          else {
            ok = false;
          }
          
          if (ok || (lastLeg && t->tComputedStatus == StatusOK)) {
            if (lastLeg)
              legTime = t->tComputedTime;
            else
              legTime = timeAcc + timePar;
            teamClass->getLeaderInfo(oClass::AllowRecompute::No, i).updateComputed(legTime, oClass::LeaderInfo::Type::Total);
            teamClass->getLeaderInfo(oClass::AllowRecompute::No, i).updateComputed(t->getInputTime() + legTime, oClass::LeaderInfo::Type::TotalInput);
          }
          
          auto ltNext = teamClass->getLegType(i + 1);
          if (ltNext == LTNormal || ltNext == LTSum)
            timeAcc += timePar;
          
          timePar = 0;

          t->Runners[i]->tComputedTime = res.time;
          t->Runners[i]->tComputedStatus = res.status;
          t->Runners[i]->tComputedPoints = t->Runners[i]->getTempResult().getPoints();

          int legScore = -1;
          if (res.status == StatusOK) {
            if (rgClasses.count(clsId))
              legScore = numeric_limits<int>::max() - (7 * 24 * 3600 * max(1, (1 + t->Runners[i]->tComputedPoints)) - res.time);
            else
              legScore = res.time;
          }

          pClass rCls = t->Runners[i]->getClassRef(true);
          int leg = rCls->mapLeg(i);
          legResultsData.emplace_back(rCls->getId() * 256 + leg, legScore, t->Runners[i]);
        }
        else {
          if (!teamClass->isOptional(i))
            ok = false;
        }
        if (t->tComputedStatus == StatusOK || 
            t->tComputedStatus == StatusOutOfCompetition ||
            t->tComputedStatus == StatusNoTiming)
          res.time = legTime;

        if (lastLeg) {
          res.time = t->tComputedTime;
          res.status = t->tComputedStatus;
        }

        t->setComputedResult(i, res);
      }
    }

    calculatePlace(legResultsData, [this](DR &res, int value) {
      res.dst->tPlace.update(*this, value, false);
      res.dst->tTotalPlace.update(*this, value, false); });
    
    // Calculate and store total result
    calculatePlace(resData, [this](DT &res, int value) {
      for (int i = 0; i < res.dst->getNumRunners(); i++) {
        res.dst->getTeamPlace(i).totalP.update(*this, value, false);
      }});
  }
}

const shared_ptr<GeneralResult> &oEvent::getGeneralResult(const string &tag, wstring &sourceFileOut) const {
  for (int i = 0; i < 2; i++) {
    if (i>0)
      loadGeneralResults(false, true);
    for (size_t k = 0; k < generalResults.size(); k++) {
      if (tag == generalResults[k].tag) {
        if (generalResults[k].ptr == 0)
          throw meosException("Internal error");
        sourceFileOut = generalResults[k].fileSource;
        if (sourceFileOut == L"*")
          sourceFileOut = L"";

        return generalResults[k].ptr;
      }
    }
  }
  throw meosException("There is no result module with X as identifier.#" + tag);
}

void oEvent::loadGeneralResults(bool forceReload, bool loadFromDisc) const {
//  OutputDebugString("Load General Results\n");
  wchar_t bf[260];
  getUserFile(bf, L"");
  vector<wstring> res;
  expandDirectory(bf, L"*.rules", res);
  vector<wstring> res2;
  expandDirectory(bf, L"*.brules", res2);

  DynamicResult dr;
  pair<wstring, wstring> err;

  vector<GeneralResultCtr> newGeneralResults;
  set<wstring> loaded;
  map<string, int> tags;
  set<long long> loadedRes;
  if (forceReload) {
    // Keep only built-in (non-dynamic) result modules
    for (size_t k = 0; k < generalResults.size(); k++) {
      if (!generalResults[k].isDynamic())
        newGeneralResults.push_back(generalResults[k]);
    }
    generalResults.clear();
  }
  else {
    // Mark all already loaded results as loaded
    for (size_t k = 0; k < generalResults.size(); k++) {
      if (generalResults[k].isDynamic()) {
        loaded.insert(generalResults[k].fileSource);
        tags.emplace(generalResults[k].tag, k);
        loadedRes.insert(dynamic_cast<DynamicResult &>(*generalResults[k].ptr).getHashCode());
      }
    }
    swap(generalResults, newGeneralResults);
  }

  vector<DynamicResultRef> rmAll;
  getListContainer().getGeneralResults(rmAll);

  // Get the open list from list editor
  TabList &tl = dynamic_cast<TabList &>(*gdibase.getTabs().get(TListTab));
  ListEditor *le = tl.getListEditorPtr();
  if (le) {
    MetaList *editorList = le->getCurrentList();
    if (editorList) {
      vector<DynamicResultRef> rm;
      editorList->getDynamicResults(rm);
      rmAll.insert(rmAll.end(), rm.begin(), rm.end());
    }
  }
  
  for (size_t ii = 1; ii <= rmAll.size(); ii++) {
    size_t i = rmAll.size() - ii;
    if (!rmAll[i].res)
      continue;
    long long hash = rmAll[i].res->getHashCode();
    string newTag = rmAll[i].res->getTag();

    //string db = "Load result " + newTag + ", h=" + itos(hash) + "\n";
    //OutputDebugStringA(db.c_str());
    int setIx = -1;
    if (tags.count(newTag)) {
      int ix = tags[newTag];

      if (loadedRes.count(hash) || newGeneralResults[ix].isImplicit())
        setIx = ix; // Already loaded
      
      if (setIx == -1) {

        if (!rmAll[i].res->retaggable()) {
          //assert(newTag == rmAll.back().res->getTag());
          assert(newGeneralResults[ix].tag == newTag);
          int n = 1;
          string baseTag = newTag = DynamicResult::undecorateTag(newTag);
          while (tags.count(newTag)) {
            newTag = baseTag + "-v" + itos(++n);
          }
          if (newGeneralResults[ix].isDynamic()) {
            tags[newTag] = ix;
            newGeneralResults[ix].tag = newTag;
          }
        }
        else {
          int n = 1;
          string baseTag = newTag = DynamicResult::undecorateTag(newTag);
          while (tags.count(newTag)) {
            newTag = baseTag + "-v" + itos(++n);
          }

          string db = "Retag " + newTag + "\n";
          OutputDebugStringA(db.c_str());

          if (rmAll[i].ctr)
            rmAll[i].ctr->retagResultModule(newTag, true);
        }
      }
    }
    auto &drp = rmAll[i].res;
    drp->setAnnotation(rmAll[i].getAnnotation());
    
    if (setIx == -1) {
      tags.emplace(drp->getTag(), newGeneralResults.size());
      newGeneralResults.emplace_back(wstring(L"*"), drp);
    }
    else {
      newGeneralResults[setIx] = GeneralResultCtr(wstring(L"*"), drp);
    }
  }

  if (loadFromDisc) {
    size_t builtIn = res2.size();
    for (size_t k = 0; k < res.size(); k++)
      res2.push_back(res[k]);

    for (size_t k = 0; k < res2.size(); k++) {
      try {
        if (loaded.count(res2[k]))
          continue;

        dr.load(res2[k]);
        string tag = DynamicResult::undecorateTag(dr.getTag());
        int iter = 1;
        while (tags.count(tag)) 
          tag = dr.getTag() + + "_v" + itos(++iter);
        if (iter > 1)
          dr.setTag(tag);

        auto drp = make_shared<DynamicResult>(dr);
        if (loadedRes.count(drp->getHashCode()))
          continue;

        tags.emplace(dr.getTag(), newGeneralResults.size());
        if (k < builtIn)
          drp->setBuiltIn();

        loadedRes.insert(drp->getHashCode());
        newGeneralResults.push_back(GeneralResultCtr(res2[k], drp));
      }
      catch (meosException &ex) {
        if (err.first.empty()) {
          err.first = res2[k];
          err.second = ex.wwhat();
        }
      }
      catch (std::exception &ex) {
        if (err.first.empty()) {
          err.first = res2[k];
          err.second = gdibase.widen(ex.what());
        }
      }
    }
  }

  swap(newGeneralResults, generalResults);
  size_t ndIx;
  for (ndIx = 0; ndIx < generalResults.size(); ndIx++) {
    if (generalResults[ndIx].isDynamic())
      break;
  }
  sort(generalResults.begin() + ndIx, generalResults.end());
  if (!err.first.empty())
    throw meosException(L"Error loading X (Y)#" + err.first + L"#" + err.second);
}

void oEvent::setGeneralResultContext(const oListParam *ctx) {
  for (auto &gr : generalResults) {
    if (ctx)
      gr.ptr->setContext(ctx);
    else
      gr.ptr->clearContext();
  }
}


void oEvent::getGeneralResults(bool onlyEditable, vector< pair<int, pair<string, wstring> > > &tagNameList, bool includeDate) const {
  tagNameList.clear();
  map<wstring, int> count;
  map<pair<wstring, string>, int> countDate;
  for (size_t k = 0; k < generalResults.size(); k++) {
    string date = generalResults[k].ptr->getTimeStamp().substr(0,10);
    ++count[generalResults[k].name];
    ++countDate[make_pair(generalResults[k].name, date)];
  }

  for (size_t k = 0; k < generalResults.size(); k++) {
    if (!onlyEditable || generalResults[k].isDynamic()) {
      tagNameList.push_back(make_pair(100 + k, make_pair(generalResults[k].tag, lang.tl(generalResults[k].name))));
      if (count[generalResults[k].name] > 1) {
        size_t res = generalResults[k].tag.find_last_of('v');
        if (res != string::npos) {
          string version = generalResults[k].tag.substr(res);
          tagNameList.back().second.second += L", " + gdioutput::widen(version);
        }
        if (includeDate) {
          const string &datetime = generalResults[k].ptr->getTimeStamp();
          if (!datetime.empty()) {
            string date = datetime.substr(0, 10);
            if (countDate[make_pair(generalResults[k].name, date)] > 1) {
              tagNameList.back().second.second += L", " + gdioutput::widen(datetime);
            }
            else {
              tagNameList.back().second.second += L", " + gdioutput::widen(date);
            }
          }
        }
      }
    }
  }
}

struct TeamResultContainer {
  pTeam team;
  int runningTime;
  RunnerStatus status;
  
  bool operator<(const TeamResultContainer &o) const {

    pClass cls = team->getClassRef(false);
    pClass ocls = o.team->getClassRef(false);

    if (cls != ocls) {
      int so = cls ? cls->getSortIndex() : 0;
      int oso = ocls ? ocls->getSortIndex() : 0;
      if (so != oso)
        return so < oso;
    }

    if (status != o.status)
      return status < o.status;

    if (runningTime != o.runningTime)
      return runningTime < o.runningTime;

    return false;
  }
};

void oEvent::calculateTeamResultAtControl(const set<int> &classId, int leg, int courseControlId, bool totalResults) {
  vector<TeamResultContainer> objs;
  objs.reserve(Teams.size());
  oSpeakerObject temp;
  for (auto &t : Teams) {
    if (t.isRemoved())
      continue;

    if (!classId.empty() && !classId.count(t.getClassId(false)))
      continue;
    temp.reset();
    t.fillSpeakerObject(leg, courseControlId, -1, totalResults, temp);
    if (!temp.owner)
      continue;
    TeamResultContainer trc;
    trc.runningTime = temp.runningTime.time;
    trc.status = temp.status;
    trc.team = &t;
    objs.push_back(trc);
  }
  
  sort(objs.begin(), objs.end());

  int cClass = -1;
  int cPlace = -1;
  int placeCounter = -1;
  int cTime = 0;
  for (size_t i = 0; i < objs.size(); i++) {
    pTeam team = objs[i].team;
    int c = team->getClassId(false);
    if (c != cClass) {
      cClass = c;
      placeCounter = 1;
      cTime = -1;
    }
    else {
      placeCounter++;
    }

    if (cTime != objs[i].runningTime) {
      cPlace = placeCounter;
    }

    team->tmpResult.startTime = team->getStartTime();
    team->tmpResult.status = objs[i].status;
    team->tmpResult.runningTime = objs[i].runningTime;
    team->tmpResult.place = objs[i].status == StatusOK ? cPlace : 0;
    team->tmpResult.points = 0; // Not supported
  }
}

void oEvent::computePreliminarySplitResults(const set<int> &classes) const {
  bool allClasses = classes.empty();
  map<pair<int, int>, vector<const oRunner *>> runnerByClassLeg;
  for (auto &r : Runners) {
    r.tOnCourseResults.clear();
    r.currentControlTime.first = 1;
    r.currentControlTime.second = 100000;

    if (r.isRemoved() || r.getClassId(false) == 0)
      continue;
    int cls = r.getClassId(true);
    if (!allClasses && classes.count(cls) == 0)
      continue;
    int leg = r.getLegNumber();
    if (r.getClassRef(false)->getQualificationFinal())
      leg = 0;

    r.setupRunnerStatistics();
    runnerByClassLeg[make_pair(cls, leg)].push_back(&r);
  }

  map<pair<int, int>, int> courseCCid2CourseIx;
  for (auto &c : Courses) {
    if (c.isRemoved())
      continue;
    for (int ix = 0; ix < c.getNumControls(); ix++) {
      int ccid = c.getCourseControlId(ix);
      courseCCid2CourseIx[make_pair(c.getId(), ccid)] = ix;
    }
    courseCCid2CourseIx[make_pair(c.getId(), oPunch::PunchFinish)] = c.getNumControls();
  }

  map<pair<int, int>, set<int>> classLeg2ExistingCCId;
  for (auto &p : punches) {
    if (p.isRemoved() || p.isHiredCard())
      continue;
    pRunner r = p.getTiedRunner();
    if (!r)
      continue;
    if (!p.isCheck() && r->getCourse(false) == nullptr)
      r->tOnCourseResults.hasAnyRes = true; // Register all punches for runners without course

    pClass cls = r->getClassRef(false);
    if (r->getCourse(false) && cls) {
      int ccId = p.getCourseControlId();
      if (ccId <= 0)
        continue;
      int crs = r->getCourse(false)->getId();
      int time = p.getTimeInt() - r->getStartTime(); //XXX Team time
      r->tOnCourseResults.emplace_back(ccId, courseCCid2CourseIx[make_pair(crs, ccId)], time);
      int clsId = r->getClassId(true);
      int leg = r->getLegNumber();
      if (cls->getQualificationFinal())
        leg = 0;

      classLeg2ExistingCCId[make_pair(clsId, leg)].insert(ccId);
    }
  }
  // Add missing punches from card
  for (auto &r : Runners) {
    if (r.isRemoved() || !r.Card || r.getClassId(false) == 0)
      continue;
    int clsId = r.getClassId(true);
    int leg = r.getLegNumber();
    if (r.getClassRef(false)->getQualificationFinal())
      leg = 0;

    const set<int> &expectedCCid = classLeg2ExistingCCId[make_pair(clsId, leg)];
    size_t nRT = 0;
    for (auto &radioTimes : r.tOnCourseResults.res) {
      if (expectedCCid.count(radioTimes.courseControlId))
        nRT++;
    }

    if (nRT < expectedCCid.size()) {
      pCourse crs = r.getCourse(true);
      for (auto &p : r.Card->punches) {
        if (p.tIndex >= 0 && p.tIndex < crs->getNumControls()) {
          int ccId = crs->getCourseControlId(p.tIndex);
          if (expectedCCid.count(ccId)) {
            bool added = false;
            for (auto &stored : r.tOnCourseResults.res) {
              if (stored.courseControlId == ccId) {
                added = true;
                break;
              }
            }
            if (!added) {
              int time = p.getTimeInt() - r.getStartTime(); //XXX Team time
              r.tOnCourseResults.emplace_back(ccId, p.tIndex, time);
            }
          }
        }
      }
    }
  }

  vector<tuple<int, int, int>> timeRunnerIx;
  for (auto rList : runnerByClassLeg) {
    auto &rr = rList.second;
    pClass cls = getClass(rList.first.first);
    assert(cls);
    bool totRes = cls->getNumStages() > 1;

    set<int> &legCCId = classLeg2ExistingCCId[rList.first];
    legCCId.insert(oPunch::PunchFinish);
    for (const int ccId : legCCId) {
      // Leg with negative sign
      int negLeg = 0;
      timeRunnerIx.clear();
      int nRun = rr.size();
      if (ccId == oPunch::PunchFinish) {
        negLeg = -1000; //Finish, smallest number
        for (int j = 0; j < nRun; j++) {
          auto r = rr[j];
          if (r->prelStatusOK(true, false)) {
            int time;
            if (!r->tInTeam || !totRes)
              time = r->getRunningTime(true);
            else {
              time = r->tInTeam->getLegRunningTime(r->tLeg, true, false);
            }
            int ix = -1;
            int nr = r->tOnCourseResults.res.size();
            for (int i = 0; i < nr; i++) {
              if (r->tOnCourseResults.res[i].courseControlId == ccId) {
                ix = i;
                break;
              }
            }
            if (ix == -1) {
              ix = r->tOnCourseResults.res.size();
              int nc = 0;
              pCourse crs = r->getCourse(false);
              if (crs)
                nc = crs->getNumControls();
              r->tOnCourseResults.emplace_back(ccId, nc, time);
            }
            timeRunnerIx.emplace_back(time, j, ix);
          }
        }
      }
      else {
        for (int j = 0; j < nRun; j++) {
          auto r = rr[j];
          int nr = r->tOnCourseResults.res.size();
          for (int i = 0; i < nr; i++) {
            if (r->tOnCourseResults.res[i].courseControlId == ccId) {
              timeRunnerIx.emplace_back(r->tOnCourseResults.res[i].time, j, i);
              negLeg = min(negLeg, -r->tOnCourseResults.res[i].controlIx);
              break;
            }
          }
        }
      }
      sort(timeRunnerIx.begin(), timeRunnerIx.end());

      int place = 0;
      int time = 0;
      int leadTime = 0;
      int numPlace = timeRunnerIx.size();
      for (int i = 0; i < numPlace; i++) {
        int ct = get<0>(timeRunnerIx[i]);
        if (time != ct) {
          time = ct;
          place = i + 1;
          if (leadTime == 0)
            leadTime = time;
        }
        auto r = rr[get<1>(timeRunnerIx[i])];
        int locIx = get<2>(timeRunnerIx[i]);
        r->tOnCourseResults.res[locIx].place = place;
        r->tOnCourseResults.res[locIx].after = time - leadTime;

        int &legWithTimeIndexNeg = r->currentControlTime.first;
        if (negLeg < legWithTimeIndexNeg) {
          legWithTimeIndexNeg = negLeg;
          r->currentControlTime.second = ct;
        }
      }
    }
  }
}
