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

      it->tPlace.update(*this, vPlace); // XXX User other result container
    }
    else
      it->tPlace.update(*this, 99000+it->tStatus);
  }
}

void oEvent::calculateResults(const set<int> &classes, ResultType resultType, bool includePreliminary) const {
  if (resultType == ResultType::PreliminarySplitResults) {
    computePreliminarySplitResults(classes);
    return;
  }
  const bool totalResults = resultType == ResultType::TotalResult;
  const bool courseResults = resultType == ResultType::CourseResult;
  const bool classCourseResults = resultType == ResultType::ClassCourseResult;
 
  bool all = classes.empty();
  vector<const oRunner *> runners;
  runners.reserve(Runners.size());
  for (auto &r : Runners) {
    if (r.isRemoved())
      continue;

    if (!all && !classes.count(r.getClassId(true)))
      continue;
    runners.push_back(&r);
  }

  if (classCourseResults)
    sortRunners(ClassCourseResult, runners);
  else if (courseResults)
    sortRunners(CourseResult, runners);
  else if (!totalResults)
    sortRunners(ClassResult, runners);
  else
    sortRunners(ClassTotalResult, runners);

  oRunnerList::iterator it;

  int cClassId=-1;
  int cPlace=0;
  int vPlace=0;
  int cTime=0;
  int cDuplicateLeg=0;
  int cLegEquClass = 0;
  bool invalidClass = false;
  bool useResults = false;
  for (auto it : runners) {
    
    // Start new "class"
    if (classCourseResults) {
      const pCourse crs = it->getCourse(false);
      int crsId = it->getClassId(true) * 997 + (crs ? crs->getId() : 0);
      if (crsId != cClassId) {
        cClassId = crsId;
        cPlace=0;
        vPlace=0;
        cTime=0;
        useResults = it->Class ? !it->Class->getNoTiming() : false;
        invalidClass = it->Class ? it->Class->getClassStatus() != oClass::Normal : false;
      }
    }
    else if (courseResults) {
      const pCourse crs = it->getCourse(false);
      int crsId = crs ? crs->getId() : 0;
      if (crsId != cClassId) {
        cClassId = crsId;
        useResults = crs != 0;
        cPlace=0;
        vPlace=0;
        cTime=0;
      }
    }
    else if (it->getClassId(true) != cClassId || it->tDuplicateLeg!=cDuplicateLeg || it->tLegEquClass != cLegEquClass) {
      cClassId=it->getClassId(true);
      useResults = it->Class ? !it->Class->getNoTiming() : false;
      cPlace=0;
      vPlace=0;
      cTime=0;
      cDuplicateLeg = it->tDuplicateLeg;
      cLegEquClass = it->tLegEquClass;

      invalidClass = it->Class ? it->Class->getClassStatus() != oClass::Normal : false;
    }

    // Calculate results
    if (invalidClass) {
      it->tTotalPlace = 0;
      it->tPlace.update(*this, 0);
    }
    else if (!totalResults) {
      int tPlace = 0;

      if (it->tStatus==StatusOK || (includePreliminary && it->tStatus == StatusUnknown && it->FinishTime > 0)){
        cPlace++;

        int rt = it->getRunningTime() + it->getNumShortening() * 3600 * 24* 8;

        if (rt > cTime)
          vPlace=cPlace;

        cTime = rt;

        if (useResults && cTime > 0)
          tPlace = vPlace;
      }
      else
        tPlace = 99000 + it->tStatus;

      if (!classCourseResults)
        it->tPlace.update(*this, tPlace);
      else
        it->tCoursePlace = tPlace;
    }
    else {
      int tt = it->getTotalRunningTime(it->FinishTime, true);

      RunnerStatus totStat = it->getTotalStatus();
      if (totStat == StatusOK || (includePreliminary && totStat == StatusUnknown) && tt>0) {
        cPlace++;

        if (tt > cTime)
          vPlace = cPlace;

        cTime = tt;

        if (useResults)
          it->tTotalPlace = vPlace;
        else
          it->tTotalPlace = 0;
      }
      else
        it->tTotalPlace = 99000 + it->tStatus;
    }
  }
}

void oEvent::calculateRogainingResults(const set<int> &classSelection) const {
  const bool all = classSelection.empty();
  vector<const oRunner *> runners;
  runners.reserve(Runners.size());
  for (auto &r : Runners) {
    if (r.isRemoved())
      continue;

    if (!all && !classSelection.count(r.getClassId(true)))
      continue;

    if (r.Class && r.Class->isRogaining()) {
      runners.push_back(&r);
    }
  }
  sortRunners(ClassPoints, runners);

  int cClassId=-1;
  int cPlace = 0;
  int vPlace = 0;
  int cTime = numeric_limits<int>::min();
  int cDuplicateLeg=0;
  bool useResults = false;
  bool isRogaining = false;
  bool invalidClass = false;

  for (auto it : runners) {
    if (it->getClassId(true)!=cClassId || it->tDuplicateLeg!=cDuplicateLeg) {
      cClassId = it->getClassId(true);
      useResults = it->Class ? !it->Class->getNoTiming() : false;
      cPlace = 0;
      vPlace = 0;
      cTime = numeric_limits<int>::min();
      cDuplicateLeg = it->tDuplicateLeg;
      invalidClass = it->Class ? it->Class->getClassStatus() != oClass::Normal : false;
    }

    if (invalidClass) {
      it->tTotalPlace = 0;
      it->tPlace.update(*this, 0);
    }
    else if (it->tStatus==StatusOK) {
      cPlace++;

      int cmpRes = 3600 * 24 * 7 * it->tRogainingPoints - it->getRunningTime();

      if (cmpRes != cTime)
        vPlace = cPlace;

      cTime = cmpRes;

      if (useResults)
        it->tPlace.update(*this, vPlace);
      else
        it->tPlace.update(*this, 0);
    }
    else
      it->tPlace.update(*this, 99000 + it->tStatus);
  }
}

bool oEvent::calculateTeamResults(int leg, bool totalMultiday)
{
  oTeamList::iterator it;

  bool hasRunner;
  if (totalMultiday)
    hasRunner = sortTeams(ClassTotalResult, leg, true);
  else
    hasRunner = sortTeams(ClassResult, leg, true);

  if (!hasRunner)
    return false;

  int cClassId=0;
  int cPlace=0;
  int vPlace=0;
  int cTime=0;
  bool invalidClass = false;

  for (it=Teams.begin(); it != Teams.end(); ++it){
    if (it->isRemoved())
      continue;

    if (it->Class && it->Class->Id!=cClassId){
      cClassId=it->Class->Id;
      cPlace=0;
      vPlace=0;
      cTime=0;
      invalidClass = it->Class->getClassStatus() != oClass::Normal;
    }

    int sleg;
    if (leg==-1)
      sleg=it->Runners.size()-1;
    else
      sleg=leg;
    int p;
    if (invalidClass) {
      p = 0;
    }
    else if (it->_cachedStatus == StatusOK){
      cPlace++;

      if (it->_sortTime>cTime)
        vPlace=cPlace;

      cTime = it->_sortTime;

      p = vPlace;
    }
    else {
      p = 99000+it->_sortStatus; //XXX Set to zero!?
    }

    if (totalMultiday)
        it->_places[sleg].totalP = p;
    else
        it->_places[sleg].p = p;
    }
  return true;
}

void oEvent::calculateTeamResults(bool multidayTotal)
{
  for(int i=0;i<maxRunnersTeam;i++) {
    if (!calculateTeamResults(i, multidayTotal))
      return;
  }
}

GeneralResult &oEvent::getGeneralResult(const string &tag, wstring &sourceFileOut) const {
  for (int i = 0; i < 2; i++) {
    if (i>0)
      loadGeneralResults(false);
    for (size_t k = 0; k < generalResults.size(); k++) {
      if (tag == generalResults[k].tag) {
        if (generalResults[k].ptr == 0)
          throw meosException("Internal error");
        sourceFileOut = generalResults[k].fileSource;
        if (sourceFileOut == L"*")
          sourceFileOut = L"";

        return *generalResults[k].ptr;
      }
    }
  }
  throw meosException("Result module not found: " + tag);
}

void oEvent::loadGeneralResults(bool forceReload) const {
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
  set<string> tags;
  set<long long> loadedRes;
  for (size_t k = 0; k < generalResults.size(); k++) {
    if (forceReload) {
      if (!generalResults[k].isDynamic())
        newGeneralResults.push_back(generalResults[k]);
    }
    else if (generalResults[k].isDynamic()) {
      loaded.insert(generalResults[k].fileSource);
      tags.insert(generalResults[k].tag);
      loadedRes.insert(dynamic_cast<DynamicResult &>(*generalResults[k].ptr).getHashCode());
    }
  }
  if (forceReload)
    generalResults.clear();
  else
    swap(generalResults, newGeneralResults);

  size_t builtIn = res2.size();
  for (size_t k = 0; k < res.size(); k++)
    res2.push_back(res[k]);

  for (size_t k = 0; k < res2.size(); k++) {
    try {
      if (loaded.count(res2[k]))
        continue;

      dr.load(res2[k]);
      while (tags.count(dr.getTag())) {
        dr.setTag(dr.getTag() + "x");
      }

      tags.insert(dr.getTag());
      DynamicResult *drp = new DynamicResult(dr);
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
  vector<DynamicResultRef> rmAll;
  for (int k = 0; k < getListContainer().getNumLists(); k++) {
    vector<DynamicResultRef> rm;
    //if (!getListContainer().isExternal(k))
    //  continue;
    getListContainer().getList(k).getDynamicResults(rm);

    if (!getListContainer().isExternal(k)) {
      for (size_t j = 0; j < rm.size(); j++) {
        if (rm[j].res)
          rm[j].res->setReadOnly();
      }
    }

    rmAll.insert(rmAll.end(), rm.begin(), rm.end());
  }

  // Get the open list from list editor
  TabList &tl = dynamic_cast<TabList &>(*gdibase.getTabs().get(TListTab));
  ListEditor *le = tl.getListeditor();
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

    string db = "Load result " + newTag + ", h=" + itos(hash) + "\n";
//    OutputDebugString(db.c_str());

    if (loadedRes.count(hash) && tags.count(newTag))
      continue; // Already loaded

    if (tags.count(newTag)) {
      int n = 1;
      newTag = DynamicResult::undecorateTag(newTag);
      while(tags.count(newTag + "-v" + itos(n))) {
        n++;
      }
      newTag += "-v" + itos(n);

      string db = "Retag " + newTag + "\n";
//      OutputDebugString(db.c_str());

      rmAll[i].ctr->retagResultModule(newTag, true);
    }
    
    tags.insert(rmAll[i].res->getTag());
    DynamicResult *drp = new DynamicResult(*rmAll[i].res);
    if (rmAll[i].res->isReadOnly())
      drp->setReadOnly();
    drp->setAnnotation(rmAll[i].ctr->getListName());
    wstring file = L"*";
    newGeneralResults.push_back(GeneralResultCtr(file, drp));
  }

  swap(newGeneralResults, generalResults);
  if (!err.first.empty())
    throw meosException(L"Error loading X (Y)#" + err.first + L"#" + err.second);
}


void oEvent::getGeneralResults(bool onlyEditable, vector< pair<int, pair<string, wstring> > > &tagNameList, bool includeDate) const {
  tagNameList.clear();
  for (size_t k = 0; k < generalResults.size(); k++) {
    if (!onlyEditable || generalResults[k].isDynamic()) {
      tagNameList.push_back(make_pair(100 + k, make_pair(generalResults[k].tag, lang.tl(generalResults[k].name))));
      if (includeDate && generalResults[k].isDynamic()) {
        const DynamicResult &dr = dynamic_cast<const DynamicResult &>(*generalResults[k].ptr);
        const wstring &date = gdibase.widen(dr.getTimeStamp());
        if (!date.empty())
          tagNameList.back().second.second += L" [" + date + L"]";
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
          if (r->prelStatusOK()) {
            int time;
            if (!r->tInTeam || !totRes)
              time = r->getRunningTime();
            else {
              time = r->tInTeam->getLegRunningTime(r->tLeg, false);
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
