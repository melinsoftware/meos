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
#include "stdafx.h"

#include <algorithm>
#include "generalresult.h"
#include "oEvent.h"
#include "meos_util.h"
#include "oListInfo.h"
#include "meosexception.h"
#include "localizer.h"

extern gdioutput *gdi_main;


GeneralResultCtr::GeneralResultCtr(const char *tagIn, const wstring &nameIn, const shared_ptr<GeneralResult> &ptrIn) {
  name = nameIn;
  tag = tagIn;
  ptr = ptrIn;
}

GeneralResultCtr::GeneralResultCtr(const wstring &file, const shared_ptr<DynamicResult> &ptrIn) {
  ptr = ptrIn;
  name = ptrIn->getName(false);
  tag = ptrIn->getTag();
  fileSource = file;
}

bool GeneralResultCtr::operator<(const GeneralResultCtr &c) const {
  return CompareString(LOCALE_USER_DEFAULT, 0,
                       name.c_str(), name.length(),
                       c.name.c_str(), c.name.length()) == CSTR_LESS_THAN;
}

const wstring& GeneralResultCtr::getName() const {
  if (!name.empty() && name[0] == '%')
    return lang.tl(name.substr(1));
  else return name;
}

GeneralResultCtr::GeneralResultCtr(const GeneralResultCtr &ctr) {
  ptr = ctr.ptr;
  name = ctr.name;
  tag = ctr.tag;
  fileSource = ctr.fileSource;
}

void GeneralResultCtr::operator=(const GeneralResultCtr &ctr) {
  if (this == &ctr)
    return;
  name = ctr.name;
  ptr = ctr.ptr;
  tag = ctr.tag;
  fileSource = ctr.fileSource;
}

bool GeneralResultCtr::isDynamic() const {
  return !fileSource.empty();
}

GeneralResult::GeneralResult(void) {
  context = 0;
}

GeneralResult::~GeneralResult(void) {
}

// Needed to generate template instances
void GRINSTANCE() {
  vector<oRunner *> a;
  vector<oTeam *> b;
  GeneralResult gr;
  gr.sort(a, SortByFinishTime);
  gr.sort(b, SortByFinishTime);
}

const string &GeneralResult::getTimeStamp() const {
  return gdioutput::narrow(getMeosDate());
}

bool GeneralResult::isRogaining() const {
  return false;
}

void GeneralResult::setContext(const oListParam *contextIn) {
  context = contextIn;
}

void GeneralResult::clearContext() {
  context = 0;
}

int GeneralResult::getListParamTimeToControl() const {
  if (context)
    return context->useControlIdResultTo;
  else
    return 0; // No context in method editor
}

int GeneralResult::getListParamTimeFromControl() const {
  if (context)
    return context->useControlIdResultFrom;
  else
    return 0; // No context in method editor
}

struct GRSortInfo {
  int principalSort;
  pair<int, int> score;
  oAbstractRunner *tr;

  bool operator<(const GRSortInfo &other) const {
    if (principalSort != other.principalSort)
      return principalSort < other.principalSort;
    else if (score != other.score)
      return score < other.score;

    const wstring &as = tr->getBib();
    const wstring &bs = other.tr->getBib();
    if (as != bs)
      return compareBib(as, bs);
    else
      return tr->getName() < other.tr->getName();
  }
};

void GeneralResult::calculateTeamResults(vector<oTeam *> &teams, 
                                         bool classResult, 
                                         oListInfo::ResultType resType, 
                                         bool sortTeams, 
                                         int inputNumber) const {
  if (teams.empty())
    return;

  if (lockPrepare>2)
    throw meosException("Bad cyclic call");
  lockPrepare++;

  try {
    set<int> clsId;
    vector<pRunner> runners;
    for (pTeam t : teams) {
      clsId.insert(t->getClassId(true));
      for (int j = 0; j < t->getNumRunners(); j++) {
        pRunner r = t->getRunner(j);
        if (r) {
          runners.push_back(r);
          clsId.insert(r->getClassId(true));
        }
      }
    }
    prepareCalculations(*teams[0]->oe, classResult, clsId, runners, teams, inputNumber);

    vector<GRSortInfo> teamScore(teams.size());
    for (size_t k = 0; k < teams.size(); k++) {
      if (resType == oListInfo::Classwise) {
        teamScore[k].principalSort = teams[k]->Class ? teams[k]->Class->getSortIndex() * 50000
          + teams[k]->Class->getId() : 0;
      }
      else
        teamScore[k].principalSort = 0;

      prepareCalculations(*teams[k], classResult);
      teams[k]->tmpResult.runningTime = teams[k]->getStartTime(); //XXX
      teams[k]->tmpResult.runningTime = deduceTime(*teams[k]);
      teams[k]->tmpResult.status = deduceStatus(*teams[k]);
      teams[k]->tmpResult.points = deducePoints(*teams[k]);

      teamScore[k].score = score(*teams[k], teams[k]->tmpResult.status,
                                 teams[k]->tmpResult.runningTime,
                                 teams[k]->tmpResult.points);

      storeOutput(teams[k]->tmpResult.outputTimes,
                  teams[k]->tmpResult.outputNumbers);

      teamScore[k].tr = teams[k];

    }

    ::sort(teamScore.begin(), teamScore.end());
    int place = 1;
    int iPlace = 1;
    int leadtime = 0;
    for (size_t k = 0; k < teamScore.size(); k++) {
      if (k > 0 && teamScore[k - 1].principalSort != teamScore[k].principalSort) {
        place = 1;
        iPlace = 1;
      }
      else if (k > 0 && teamScore[k - 1].score != teamScore[k].score) {
        place = iPlace;
      }

      if (teamScore[k].tr->tmpResult.status == StatusOK) {
        teamScore[k].tr->tmpResult.place = place;
        iPlace++;
        if (place == 1) {
          leadtime = teamScore[k].tr->tmpResult.runningTime;
          teamScore[k].tr->tmpResult.timeAfter = 0;
        }
        else {
          teamScore[k].tr->tmpResult.timeAfter = teamScore[k].tr->tmpResult.runningTime - leadtime;
        }
      }
      else {
        teamScore[k].tr->tmpResult.place = 0;
        teamScore[k].tr->tmpResult.timeAfter = 0;
      }
    }

    if (sortTeams) {
      for (size_t k = 0; k < teamScore.size(); k++) {
        teams[k] = (oTeam *)teamScore[k].tr;
      }
    }
    lockPrepare--;
  }
  catch (...) {
    lockPrepare--;
    throw;
  }
}

void GeneralResult::sortTeamMembers(vector<oRunner *> &runners) const {
  vector<GRSortInfo> runnerScore(runners.size());
  for (size_t k = 0; k < runners.size(); k++) {
    runnerScore[k].principalSort = 0;
    runnerScore[k].score = runners[k]->tmpResult.internalScore;
    //runnerScore[k].score = score(*runners[k], runners[k]->tmpResult.status,
    //                                          runners[k]->tmpResult.runningTime,
    //                                          runners[k]->tmpResult.points);

    runnerScore[k].tr = runners[k];
  }

  ::sort(runnerScore.begin(), runnerScore.end());

  for (size_t k = 0; k < runners.size(); k++) {
    runners[k] = pRunner(runnerScore[k].tr);
  }
}

template<class T> void GeneralResult::sort(vector<T *> &rt, SortOrder so) const {
  PrincipalSort ps = ClassWise;

  if (so == CourseResult || so == CourseStartTime)
    ps = CourseWise;

  else if (so == SortByName || so == SortByFinishTimeReverse ||
           so == SortByFinishTime || so == SortByStartTime)
    ps = None;

  vector< pair<int, oAbstractRunner *> > arr(rt.size());
  const int maxT = timeConstHour * 100;
  for (size_t k = 0; k < rt.size(); k++) {
    arr[k].first = 0;
    if (ps == ClassWise) {
      pClass sclass = rt[k]->getClassRef(true);
      arr[k].first = sclass ? sclass->getSortIndex() : 0;
    }
    else if (ps == CourseWise) {
      oRunner *r = dynamic_cast<oRunner *>(rt[k]);
      arr[k].first = r && r->getCourse(false) ? r->getCourse(false)->getId() : 0;
    }
    arr[k].second = rt[k];
    int ord = 0;
    const oAbstractRunner::TempResult &tr = rt[k]->getTempResult(0);
    if (so == SortByFinishTime || so == ClassFinishTime) {
      ord = tr.getFinishTime();
      if (ord == 0 || tr.getStatus() > 1)
        ord = maxT;
    }
    else if (so == SortByFinishTimeReverse) {
      ord = tr.getFinishTime();
      if (ord == 0 || tr.getStatus() > 1)
        ord = maxT;
      else
        ord = maxT - ord;
    }
    else if (so == SortByStartTime || so == ClassStartTime ||
             so == ClassStartTimeClub || so == ClubClassStartTime) {
      ord = tr.getStartTime();
    }

    arr[k].first = arr[k].first * maxT * 10 + ord;
  }

  stable_sort(arr.begin(), arr.end());

  for (size_t k = 0; k < rt.size(); k++) {
    rt[k] = (T*)arr[k].second;
  }
}

void GeneralResult::calculateIndividualResults(vector<oRunner *> &runners,
                                               bool classResult,
                                               oListInfo::ResultType resType,
                                               bool sortRunners,
                                               int inputNumber) const {

  if (runners.empty())
    return;

  if (lockPrepare > 2)
    throw meosException("Bad cyclic call");
  lockPrepare++;

  try {

    set<int> clsId;
    for (pRunner r : runners) {
      clsId.insert(r->getClassId(true));
    }
    vector<pTeam> noTeams;
    prepareCalculations(*runners[0]->oe, classResult, clsId, runners, noTeams, inputNumber);
    //bool classSort = resType == oListInfo::Global ? false : true;
    vector<GRSortInfo> runnerScore(runners.size());
    for (size_t k = 0; k < runners.size(); k++) {
      const oRunner *r = runners[k];
      if (resType == oListInfo::Classwise) {
        runnerScore[k].principalSort = r->Class ? r->Class->getSortIndex() * 50000
          + r->Class->getId() : 0;
      }
      else if (resType == oListInfo::Legwise) {
        runnerScore[k].principalSort = r->Class ? r->Class->getSortIndex() * 50000
          + r->Class->getId() : 0;

        int ln = r->getLegNumber();
        const oTeam *pt = r->getTeam();
        if (pt) {
          const oClass *tcls = pt->getClassRef(false);
          if (tcls && tcls->getClassType() == oClassRelay) {
            int dummy;
            tcls->splitLegNumberParallel(r->getLegNumber(), ln, dummy);
          }
        }
        runnerScore[k].principalSort = runnerScore[k].principalSort * 50 + ln;
      }
      else if (resType == oListInfo::Coursewise) {
        pCourse crs = r->getCourse(false);
        runnerScore[k].principalSort = crs ? crs->getId() : 0;
      }
      else
        runnerScore[k].principalSort = 0;

      int from = getListParamTimeFromControl();
      if (from <= 0) {
        runners[k]->tmpResult.startTime = r->getStartTime();
      }
      else {
        int rt;
        RunnerStatus stat;
        runners[k]->getSplitTime(from, stat, rt);
        if (stat == StatusOK)
          runners[k]->tmpResult.startTime = runners[k]->getStartTime() + rt;
        else
          runners[k]->tmpResult.startTime = runners[k]->getStartTime();
      }

      prepareCalculations(*runners[k], classResult);
      runners[k]->tmpResult.runningTime = deduceTime(*runners[k], runners[k]->tmpResult.startTime);
      runners[k]->tmpResult.status = deduceStatus(*runners[k]);
      runners[k]->tmpResult.points = deducePoints(*runners[k]);

      runnerScore[k].score = score(*runners[k], runners[k]->tmpResult.status,
                                   runners[k]->tmpResult.runningTime,
                                   runners[k]->tmpResult.points, false);

      storeOutput(runners[k]->tmpResult.outputTimes,
                  runners[k]->tmpResult.outputNumbers);

      runnerScore[k].tr = runners[k];

    }

    ::sort(runnerScore.begin(), runnerScore.end());
    int place = 1;
    int iPlace = 1;
    int leadtime = 0;
    for (size_t k = 0; k < runnerScore.size(); k++) {
      if (k > 0 && runnerScore[k - 1].principalSort != runnerScore[k].principalSort) {
        place = 1;
        iPlace = 1;
      }
      else if (k > 0 && runnerScore[k - 1].score != runnerScore[k].score) {
        place = iPlace;
      }

      if (runnerScore[k].tr->tmpResult.status == StatusOK) {
        runnerScore[k].tr->tmpResult.place = place;
        iPlace++;
        if (place == 1) {
          leadtime = runnerScore[k].tr->tmpResult.runningTime;
          runnerScore[k].tr->tmpResult.timeAfter = 0;
        }
        else {
          runnerScore[k].tr->tmpResult.timeAfter = runnerScore[k].tr->tmpResult.runningTime - leadtime;
        }
      }
      else {
        runnerScore[k].tr->tmpResult.place = 0;
        runnerScore[k].tr->tmpResult.timeAfter = 0;
      }
    }

    if (sortRunners) {
      for (size_t k = 0; k < runnerScore.size(); k++) {
        runners[k] = (oRunner *)runnerScore[k].tr;
      }
    }
  }
  catch (...) {
    lockPrepare--;
    throw;
  }
  lockPrepare--;
}

void GeneralResult::prepareCalculations(oEvent &oe, bool classResult, const set<int> &cls, vector<pRunner> &runners, vector<pTeam> &teams, int inputNumber) const {
}

void GeneralResult::prepareCalculations(oTeam &team, bool classResult) const {
  int nr = team.getNumRunners();
  for (int j = 0; j < nr; j++) {
    pRunner r = team.getRunner(j);
    if (r) {
      prepareCalculations(*r, classResult);
      r->tmpResult.runningTime = deduceTime(*r, r->getStartTime()); //XXX
      r->tmpResult.status = deduceStatus(*r);
      r->tmpResult.place = 0;//XXX?
      r->tmpResult.timeAfter = 0;//XXX?
      r->tmpResult.points = deducePoints(*r);
      r->tmpResult.internalScore = score(*r, r->tmpResult.status,
                                         r->tmpResult.runningTime,
                                         r->tmpResult.points, true);

      storeOutput(r->tmpResult.outputTimes,
                  r->tmpResult.outputNumbers);
    }
  }
}

void GeneralResult::prepareCalculations(oRunner &runner, bool classResult) const {
  int from = getListParamTimeFromControl();
  runner.tmpResult.startTime = runner.getStartTime();

  if (from > 0) {
    int rt;
    RunnerStatus stat;
    runner.getSplitTime(from, stat, rt);
    if (stat == StatusOK)
      runner.tmpResult.startTime += rt;
  }
}

void GeneralResult::storeOutput(vector<int> &times, vector<int> &numbers) const {
}

pair<int, int> GeneralResult::score(oTeam &team, RunnerStatus st, int rt, int points) const {
  return make_pair((100 * RunnerStatusOrderMap[st] + team.getNumShortening()) * 100000 - points,  + rt);
}

RunnerStatus GeneralResult::deduceStatus(oTeam &team) const {
  return team.deduceComputedStatus();
}

int GeneralResult::deduceTime(oTeam &team) const {
  return team.deduceComputedRunningTime();//team.getRunningTime(false);
}

int GeneralResult::deducePoints(oTeam &team) const {
  return team.deduceComputedPoints();//team.getRogainingPoints(false, false);
}

pair<int,int> GeneralResult::score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const {
  if (asTeamMember) {
    return make_pair(runner.getLegNumber(), 0);
  }
  else {
    return make_pair((100 * RunnerStatusOrderMap[st] + runner.getNumShortening()) * 100000 - points,  time);
  }
}

RunnerStatus GeneralResult::deduceStatus(oRunner &runner) const {
  return runner.getStatus();
}

int GeneralResult::deduceTime(oRunner &runner, int startTime) const {
  return runner.getRunningTime(false);
}

int GeneralResult::deducePoints(oRunner &runner) const {
  return runner.getRogainingPoints(false, false);
}

pair<int, int> ResultAtControl::score(oTeam &team, RunnerStatus st, int time, int points) const {
  return GeneralResult::score(team, st, time, points);
}

RunnerStatus ResultAtControl::deduceStatus(oTeam &team) const {
  return GeneralResult::deduceStatus(team);
}

int ResultAtControl::deduceTime(oTeam &team) const {
  return GeneralResult::deduceTime(team);
}

int ResultAtControl::deducePoints(oTeam &team) const {
  return GeneralResult::deducePoints(team);
}

pair<int, int> ResultAtControl::score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const {
  if (asTeamMember)
    return make_pair(runner.getLegNumber(), 0);
  
  return make_pair(RunnerStatusOrderMap[st], time);
}

RunnerStatus TotalResultAtControl::deduceStatus(oRunner &runner) const {
  RunnerStatus singleStat = ResultAtControl::deduceStatus(runner);
  if (singleStat != StatusOK)
    return singleStat;

  RunnerStatus inputStatus = StatusOK;
  if (runner.getTeam() && getListParamTimeFromControl() <= 0) {
    // Only use input time when start time is used
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0 && t->getClassRef(false)) {
      // Find base leg
      int legIx = runner.getLegNumber();
      const pClass cls = t->getClassRef(false);
      while (legIx > 0 && (cls->isParallel(legIx) || cls->isOptional(legIx)))
        legIx--;
      if (legIx > 0)
        inputStatus = t->getLegStatus(legIx - 1, false, true);
    }
    else {
      inputStatus = t->getInputStatus();
    }
  }
  else {
    inputStatus = runner.getInputStatus();
  }

  return inputStatus; // Single status is OK.
}

int TotalResultAtControl::deduceTime(oRunner &runner, int startTime) const {
  int singleTime = ResultAtControl::deduceTime(runner, startTime);

  if (singleTime == 0)
    return 0;

  int inputTime = 0;
  if (runner.getTeam() && getListParamTimeFromControl() <= 0) {
    // Only use input time when start time is used
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0 && t->getClassRef(false)) {
      // Find base leg
      int legIx = runner.getLegNumber();
      const pClass cls = t->getClassRef(false);
      while (legIx > 0 && (cls->isParallel(legIx) || cls->isOptional(legIx)))
        legIx--;
      if (legIx > 0)
        inputTime = t->getLegRunningTime(legIx - 1, false, true);
    }
    else {
      inputTime = t->getInputTime();
    }
  }
  else {
    inputTime = runner.getInputTime();
  }

  return singleTime + inputTime;
}

pair<int,int> TotalResultAtControl::score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const {
  if (asTeamMember)
    return make_pair(runner.getLegNumber(), 0);
  const int TK = timeConstHour * 24 * 7;
  RunnerStatus inputStatus = StatusOK;

  if (runner.getTeam()) {
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0) {
      inputStatus = t->getLegStatus(runner.getLegNumber() - 1, false, true);
    }
    else {
      inputStatus = t->getInputStatus();
    }
  }
  else {
    inputStatus = runner.getInputStatus();
  }

  if (st != StatusUnknown)
    st = max(inputStatus, st);

  return make_pair(RunnerStatusOrderMap[st], time);
}

RunnerStatus ResultAtControl::deduceStatus(oRunner &runner) const {
  int fc = getListParamTimeToControl();
  if (fc > 0) {
    RunnerStatus stat;
    int rt;
    runner.getSplitTime(fc, stat, rt);
    return stat;
  }
  RunnerStatus st = runner.getStatus();
  if (st == StatusUnknown && runner.getRunningTime(false) > 0)
    return StatusOK;
  return st;
}

int ResultAtControl::deduceTime(oRunner &runner, int startTime) const {
  int fc = getListParamTimeToControl();

  if (fc > 0) {
    RunnerStatus stat;
    int rt;
    runner.getSplitTime(fc, stat, rt);

    if (stat == StatusOK)
      return runner.getStartTime() + rt - startTime;
  }
  else if (runner.getFinishTime() > 0) {
    return runner.getFinishTime() - startTime;
  }

  return 0;
}

int ResultAtControl::deducePoints(oRunner &runner) const {
  return 0;
}

int DynamicResult::instanceCount = 0;
map<string, DynamicResult::DynamicMethods> DynamicResult::symb2Method;
map<DynamicResult::DynamicMethods, pair<string, string> > DynamicResult::method2SymbName;

DynamicResult::DynamicResult() {
  builtIn = false;
  readOnly = false;
  isCompiled = false;
  methods.resize(_Mlast);
  instanceCount++;

  if (method2SymbName.empty()) {
    addSymbol(MDeduceRStatus, "RunnerStatus", "Status calculation for runner");
    addSymbol(MDeduceRTime, "RunnerTime", "Time calculation for runner");
    addSymbol(MDeduceRPoints, "RunnerPoints", "Point calculation for runner");
    addSymbol(MRScore, "RunnerScore", "Result score calculation for runner");

    addSymbol(MDeduceTStatus, "TeamStatus", "Status calculation for team");
    addSymbol(MDeduceTTime, "TeamTime", "Time calculation for team");
    addSymbol(MDeduceTPoints, "TeamPoints", "Point calculation for team");
    addSymbol(MTScore, "TeamScore", "Result score calculation for team");
  }
}

DynamicResult::DynamicResult(const DynamicResult &resIn) {
  instanceCount++;
  isCompiled = false;
  name = resIn.name;
  tag = resIn.tag;
  readOnly = false;

  description = resIn.description;
  origin = resIn.origin;
  timeStamp = resIn.timeStamp;
  annotation = resIn.annotation;
  builtIn = resIn.builtIn;
  methods.resize(_Mlast);
  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].source = resIn.methods[k].source;
    methods[k].description = resIn.methods[k].description;
  }
}

void DynamicResult::operator=(const DynamicResult &resIn) {
  clear();
  isCompiled = false;
  name = resIn.name;
  tag = resIn.tag;
  description = resIn.description;
  origin = resIn.origin;
  timeStamp = resIn.timeStamp;
  annotation = resIn.annotation;
  readOnly = resIn.readOnly;
  builtIn = resIn.builtIn;
  methods.resize(_Mlast);
  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].source = resIn.methods[k].source;
    methods[k].description = resIn.methods[k].description;
    methods[k].pn = 0;
  }

}

void DynamicResult::addSymbol(DynamicMethods method, const char *symb, const char *name) {
  if (method2SymbName.count(method) || symb2Method.count(symb))
    throw meosException("Method symbol used");
  method2SymbName[method] = make_pair(symb, name);
  symb2Method[symb] = method;
}

DynamicResult::~DynamicResult() {
  instanceCount--;
  if (instanceCount == 0) {
    method2SymbName.clear();
    symb2Method.clear();
  }
}

DynamicResult::MethodInfo::MethodInfo() {
  pn = 0;
}

DynamicResult::MethodInfo::~MethodInfo() {
}

const ParseNode *DynamicResult::getMethod(DynamicMethods method) const {
  return methods[method].pn;
}

const string &DynamicResult::getMethodSource(DynamicMethods method) const {
  return methods[method].source;
}

void DynamicResult::setMethodSource(DynamicMethods method, const string &source) {
  methods[method].source = source;
  methods[method].pn = 0;
  methods[method].pn = parser.parse(source);
}

RunnerStatus DynamicResult::toStatus(int status) const {
  switch (status) {
  case StatusUnknown:
    return StatusUnknown;
  case StatusOK:
    return StatusOK;
  case StatusMP:
    return StatusMP;
  case StatusDNF:
    return StatusDNF;
  case StatusDNS:
    return StatusDNS;
  case StatusCANCEL:
    return StatusCANCEL;
  case StatusNotCompetiting:
    return StatusNotCompetiting;
  case StatusDQ:
    return StatusDQ;
  case StatusMAX:
    return StatusMAX;
  case StatusOutOfCompetition:
    return StatusOutOfCompetition;
  case StatusNoTiming:
    return StatusNoTiming;
  default:
    throw meosException("Unknown status code X#" + itos(status));
  }
}

pair<int,int> DynamicResult::score(oTeam &team, RunnerStatus st, int time, int points) const {
  if (getMethod(MTScore)) {
    parser.addSymbol("ComputedTime", time / timeConstSecond);
    parser.addSymbol("ComputedStatus", st);
    parser.addSymbol("ComputedPoints", points);
    return make_pair(0, getMethod(MTScore)->evaluate(parser));
  }
  else if (getMethodSource(MTScore).empty())
    return GeneralResult::score(team, st, time, points);
  else throw meosException("Syntax error");
}

RunnerStatus DynamicResult::deduceStatus(oTeam &team) const {
  if (getMethod(MDeduceTStatus))
    return toStatus(getMethod(MDeduceTStatus)->evaluate(parser));
  else if (getMethodSource(MDeduceTStatus).empty())
    return GeneralResult::deduceStatus(team);
  else throw meosException("Syntax error");
}

int DynamicResult::deduceTime(oTeam &team) const {
  if (getMethod(MDeduceTTime))
    return getMethod(MDeduceTTime)->evaluate(parser) * timeConstSecond + team.getSubSeconds();
  else if (getMethodSource(MDeduceTTime).empty())
    return GeneralResult::deduceTime(team);
  else throw meosException("Syntax error");
}

int DynamicResult::deducePoints(oTeam &team) const {
  if (getMethod(MDeduceTPoints))
    return getMethod(MDeduceTPoints)->evaluate(parser);
  else if (getMethodSource(MDeduceTPoints).empty())
    return GeneralResult::deducePoints(team);
  else throw meosException("Syntax error");
}

pair<int,int> DynamicResult::score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const {
  if (getMethod(MRScore)) {
    parser.addSymbol("ComputedTime", time / timeConstSecond);
    parser.addSymbol("ComputedStatus", st);
    parser.addSymbol("ComputedPoints", points);
    return make_pair(0, getMethod(MRScore)->evaluate(parser));
  }
  else if (getMethodSource(MRScore).empty())
    return GeneralResult::score(runner, st, time, points, asTeamMember);
  else throw meosException("Syntax error");
}

RunnerStatus DynamicResult::deduceStatus(oRunner &runner) const {
  if (getMethod(MDeduceRStatus))
    return toStatus(getMethod(MDeduceRStatus)->evaluate(parser));
  else if (getMethodSource(MDeduceRStatus).empty())
    return GeneralResult::deduceStatus(runner);
  else throw meosException("Syntax error");
}

int DynamicResult::deduceTime(oRunner &runner, int startTime) const {
  if (getMethod(MDeduceRTime))
    return getMethod(MDeduceRTime)->evaluate(parser) * timeConstSecond + runner.getSubSeconds();
  else if (getMethodSource(MDeduceRTime).empty())
    return GeneralResult::deduceTime(runner, startTime);
  else throw meosException("Syntax error");
}

int DynamicResult::deducePoints(oRunner &runner) const {
  if (getMethod(MDeduceRPoints))
    return getMethod(MDeduceRPoints)->evaluate(parser);
  else if (getMethodSource(MDeduceRPoints).empty())
    return GeneralResult::deducePoints(runner);
  else throw meosException("Syntax error");

}

void DynamicResult::save(const wstring &file) const {
  xmlparser xml;
  xml.openOutput(file.c_str(), true);
  save(xml);
  xml.closeOut();
}

extern oEvent *gEvent;

void DynamicResult::save(xmlparser &xml) const {
  xml.startTag("MeOSResultCalculationSet");
  xml.write("Name", name);
  xml.write("Tag", gdioutput::widen(tag));
  xml.write("Description", description);
  if (origin.empty())
    origin = gEvent->getName() + L" (" + getLocalDate() + L")";
  xml.write("Origin", origin);
  xml.write("Date", getLocalTime());
  //  xml.write("Tag", tag);
  //  xml.write("UID", getUniqueId());

  for (map<string, DynamicMethods>::const_iterator it = symb2Method.begin(); it != symb2Method.end(); ++it) {
    if (!methods[it->second].source.empty()) {
      xml.startTag("Rule", "name", it->first);
      xml.write("Description", methods[it->second].description);
      xml.write("Method", methods[it->second].source);
      xml.endTag();
    }
  }
  xml.endTag();
}

void DynamicResult::clear() {
  parser.clear();
  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].pn = nullptr;
    methods[k].source.clear();
    methods[k].description.clear();
  }
}

void DynamicResult::load(const wstring &file) {
  xmlparser xml;
  xml.read(file.c_str());
  xmlobject xDef = xml.getObject("MeOSResultCalculationSet");
  load(xDef);
}

void DynamicResult::load(const xmlobject &xDef) {
  if (!xDef)
    throw meosException("Ogiltigt filformat");

  clear();

  xDef.getObjectString("Name", name);
  xDef.getObjectString("Description", description);
  xDef.getObjectString("Tag", tag);

  xDef.getObjectString("Origin", origin);
  xDef.getObjectString("Date", timeStamp);
  //xDef.getObjectString("UID", uniqueIndex);
  xmlList rules;
  xDef.getObjects("Rule", rules);
  for (size_t k = 0; k < rules.size(); k++) {
    string rn;
    rules[k].getObjectString("name", rn);
    map<string, DynamicMethods>::const_iterator res = symb2Method.find(rn);
    if (res == symb2Method.end())
      throw meosException("Unknown result rule X.#" + rn);

    rules[k].getObjectString("Description", methods[res->second].description);
    rules[k].getObjectString("Method", methods[res->second].source);
  }
}

void DynamicResult::compile(bool forceRecompile) const {
  if (isCompiled)
    return;

  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].pn = 0;
  }
  parser.clear();

  pair<wstring, wstring> err;
  for (size_t k = 0; k < methods.size(); k++) {
    if (!methods[k].source.empty()) {
      try {
        methods[k].pn = parser.parse(methods[k].source);
      }
      catch (const meosException &ex) {
        if (err.first.empty()) {
          err.first = gdioutput::widen(method2SymbName[DynamicMethods(k)].second);
          err.second = lang.tl(ex.wwhat());
        }
      }
    }
  }
  if (!err.first.empty()) {
    throw meosException(L"Error in result module X, method Y (Z)#" + name + L"#" + err.first + L"#" + err.second);
  }
}

void DynamicResult::getMethodTypes(vector< pair<DynamicMethods, string> > &mt) const {
  mt.clear();
  for (map<DynamicMethods, pair<string, string> >::const_iterator it = method2SymbName.begin(); it != method2SymbName.end(); ++it)
    mt.push_back(make_pair(it->first, it->second.second));

  return;
}

void DynamicResult::declareSymbols(DynamicMethods m, bool clear) const {
  if (clear)
    parser.clearSymbols();
  const bool isRunner = m == MRScore ||
    m == MDeduceRPoints ||
    m == MDeduceRStatus ||
    m == MDeduceRTime;

  parser.declareSymbol("Status", "Runner/team status", false);
  parser.declareSymbol("Start", "Runner/team start time", false);
  parser.declareSymbol("Finish", "Runner/team finish time", false);
  parser.declareSymbol("Time", "Runner/team running time", false);
  parser.declareSymbol("Place", "Runner/team place", false);
  parser.declareSymbol("Points", "Runner/team rogaining points", false);
  parser.declareSymbol("PointReduction", "Automatic rogaining point reduction", false);
  parser.declareSymbol("PointOvertime", "Runner/team rogaining overtime", false);
  parser.declareSymbol("PointGross", "Rogaining points before automatic reduction", false);

  parser.declareSymbol("PointAdjustment", "Runner/team rogaining points adjustment", false);
  parser.declareSymbol("TimeAdjustment", "Runner/team time adjustment", false);

  parser.declareSymbol("TotalStatus", "Runner/team total status", false);
  parser.declareSymbol("TotalTime", "Runner/team total running time", false);
  parser.declareSymbol("TotalPlace", "Runner/team total place", false);

  parser.declareSymbol("InputStatus", "Runner/team input status", false);
  parser.declareSymbol("InputTime", "Runner/team input running time", false);
  parser.declareSymbol("InputPlace", "Runner/team input place", false);
  parser.declareSymbol("InputPoints", "Runner/team input points", false);

  parser.declareSymbol("StageStatus", "Runner/team earlier stage statuses", true);
  parser.declareSymbol("StageTime", "Runner/team earlier stage running times", true);
  parser.declareSymbol("StagePlace", "Runner/team earlier stage places", true);
  parser.declareSymbol("StagePoints", "Runner/team earlier stage points", true);

  parser.declareSymbol("Fee", "Runner/team fee", false);

  parser.declareSymbol("ClubId", "Club id number", false);
  parser.declareSymbol("DistrictId", "District id number", false);
  parser.declareSymbol("Bib", "Nummerlapp", false);

  parser.declareSymbol("InputNumber", "User input number", false);
  parser.declareSymbol("Shorten", "Number of shortenings", false);

  parser.declareSymbol("DataA", "Extra assigned data (A)", false);
  parser.declareSymbol("DataB", "Extra assigned data (B)", false);

  parser.declareSymbol("ClassDataA", "Extra assigned class data (A)", false);
  parser.declareSymbol("ClassDataB", "Extra assigned class data (B)", false);

  if (isRunner) {
    parser.declareSymbol("CardPunches", "Runner's card, punch codes", true);
    parser.declareSymbol("CardTimes", "Runner's card, punch times", true);
    parser.declareSymbol("CardControls", "Runner's card, matched control ids (-1 for unmatched punches)", true);

    parser.declareSymbol("Course", "Runner's course", true);
    parser.declareSymbol("CourseId", "Runner's course id", false);
    parser.declareSymbol("CourseLength", "Length of course", false);

    parser.declareSymbol("SplitTimes", "Runner's split times", true);
    parser.declareSymbol("SplitTimesAccumulated", "Runner's total running time to control", true);

    parser.declareSymbol("LegTimeDeviation", "Deviation +/- from expected time on course leg", true);
    parser.declareSymbol("LegTimeAfter", "Time after leg winner", true);
    parser.declareSymbol("LegPlace", "Place on course leg", true);
    parser.declareSymbol("Leg", "Leg number in team, zero indexed", false);
    parser.declareSymbol("BirthYear", "Year of birth", false);
    parser.declareSymbol("Age", "Age (on last day of current year)", false);
    parser.declareSymbol("AgeLowLimit", "Age below or equal implies youth", false);
    parser.declareSymbol("AgeHighLimit", "Age above or equal implies senior/pensioner", false);
    parser.declareSymbol("CheckTime", "Runner check time", false);
  }
  else {
    parser.declareSymbol("RunnerStatus", "Status for each team member", true);
    parser.declareSymbol("RunnerTime", "Running time for each team member", true);
    parser.declareSymbol("RunnerStart", "Start time for each team member", true);
    parser.declareSymbol("RunnerFinish", "Finish time for each team member", true);
    parser.declareSymbol("RunnerPoints", "Rogaining points for each team member", true);

    parser.declareSymbol("RunnerCardPunches", "Punch codes for each team member", true, true);
    parser.declareSymbol("RunnerCardTimes", "Punch times for each team member", true, true);
    parser.declareSymbol("RunnerCardControls", "Matched control ids (-1 for unmatched) for each team member", true, true);

    parser.declareSymbol("RunnerDataA", "Extra assigned data (A) for each team member", true);
    parser.declareSymbol("RunnerDataB", "Extra assigned data (B) for each team member", true);

    parser.declareSymbol("RunnerCourse", "Runner's course", true, true);
    parser.declareSymbol("RunnerSplitTimes", "Runner's split times", true, true);

    parser.declareSymbol("RunnerOutputTimes", "Runner's method output times", true, true);
    parser.declareSymbol("RunnerOutputNumbers", "Runner's method output numbers", true, true);

    parser.declareSymbol("PatrolRogainingScore", "Patrol score, rogaining", false);
    parser.declareSymbol("PatrolRogainingReduction", "Patrol score reduction", false);
    parser.declareSymbol("PatrolRogainingOvertime", "Patrol overtime", false);
  }

  parser.declareSymbol("MaxTime", "Maximum allowed running time", false);

  parser.declareSymbol("StatusUnknown", "Status code for an unknown result", false);
  parser.declareSymbol("StatusOK", "Status code for a valid result", false);
  parser.declareSymbol("StatusMP", "Status code for a missing punch", false);
  parser.declareSymbol("StatusDNF", "Status code for not finishing", false);
  parser.declareSymbol("StatusDNS", "Status code for not starting", false);
  parser.declareSymbol("StatusCANCEL", "Status code for cancelled entry", false);
  parser.declareSymbol("StatusMAX", "Status code for a time over the maximum", false);
  parser.declareSymbol("StatusDQ", "Status code for disqualification", false);
  parser.declareSymbol("StatusOutOfCompetition", "Status code for running out-of-competition", false);
  parser.declareSymbol("StatusNotCompetiting", "Status code for not competing", false);
  parser.declareSymbol("StatusNoTiming", "Status code for no timing", false);

  parser.declareSymbol("ShortestClassTime", "Shortest time in class", false);

  if (m == MRScore || m == MTScore) {
    parser.declareSymbol("ComputedTime", "Time as computed by your time method", false);
    parser.declareSymbol("ComputedPoints", "Points as computed by your point method", false);
    parser.declareSymbol("ComputedStatus", "Status as computed by your status method", false);
  }
}

void DynamicResult::getSymbols(vector< pair<wstring, size_t> > &symb) const {
  symb.clear();
  parser.getSymbols(symb);
}

void DynamicResult::getSymbolInfo(int ix, wstring &name, wstring &desc) const {
  parser.getSymbolInfo(ix, name, desc);
}

void DynamicResult::prepareCalculations(oEvent &oe,
                                        bool classResult,
                                        const set<int> &clsSelection, 
                                        vector<pRunner> &runners, 
                                        vector<pTeam> &teams, 
                                        int inputNumber) const {
  compile(false);


  oDataConstInterface odc = oe.getDCI();
  lowAgeLimit = odc.getInt("YouthAge");
  highAgeLimit = odc.getInt("SeniorAge");

  oEvent::ResultType single, total;
  if (classResult) {
    single = oEvent::ResultType::ClassResultDefault;
    total = oEvent::ResultType::TotalResultDefault;
  }
  else {
    single = oEvent::ResultType::ClassResult;
    total = oEvent::ResultType::TotalResult;
  }
  oe.calculateResults(clsSelection, single);
  oe.calculateResults(clsSelection, total);
  vector<pTeam> *tPtr = &teams;
  vector<pTeam> prepTeam;
  if (teams.empty()) {
    vector<pTeam> allTeam;
    oe.getTeams(0, allTeam, false);
    if (clsSelection.empty())
      prepTeam.swap(allTeam);
    else {
      for (pTeam t : allTeam)
        if (clsSelection.count(t->getClassId(false))) {
          prepTeam.push_back(t);
        }
    }
    tPtr = &prepTeam;
  }

  oe.calculateTeamResults(*tPtr, single);
  oe.calculateTeamResults(*tPtr, total);

  declareSymbols(MRScore, true);
  if (teams.size() > 0) {
    declareSymbols(MTScore, false);
    for (pTeam t : teams)
      t->resetResultCalcCache();

    oe.calculateTeamResults(teams, single);
    oe.calculateTeamResults(teams, total);
  }

  parser.addSymbol("StatusUnknown", StatusUnknown);
  parser.addSymbol("StatusOK", StatusOK);
  parser.addSymbol("StatusMP", StatusMP);
  parser.addSymbol("StatusDNF", StatusDNF);
  parser.addSymbol("StatusCANCEL", StatusCANCEL);
  parser.addSymbol("StatusDNS", StatusDNS);
  parser.addSymbol("StatusMAX", StatusMAX);
  parser.addSymbol("StatusDQ", StatusDQ);
  parser.addSymbol("StatusOutOfCompetition", StatusOutOfCompetition);
  parser.addSymbol("StatusNoTiming", StatusNoTiming);
  parser.addSymbol("StatusNotCompetiting", StatusNotCompetiting);

  parser.addSymbol("MaxTime", oe.getMaximalTime());
  parser.addSymbol("InputNumber", inputNumber);
}

void DynamicResult::prepareCommon(oAbstractRunner &runner, bool classResult) const {
  bool useComputed = classResult == false;
  parser.clearVariables();
  int st = runner.getStatus();
  int ft = runner.getFinishTime();
  if (st == StatusUnknown && ft > 0)
    st = StatusOK;
  parser.addSymbol("Status", st);
  parser.addSymbol("Start", runner.getStartTime() / timeConstSecond);
  parser.addSymbol("Finish", ft / timeConstSecond);
  parser.addSymbol("Time", runner.getRunningTime(useComputed) / timeConstSecond);
  parser.addSymbol("Place", runner.getPlace(false));
  parser.addSymbol("Points", runner.getRogainingPoints(useComputed, false));
  parser.addSymbol("PointReduction", runner.getRogainingReduction(useComputed));
  parser.addSymbol("PointOvertime", runner.getRogainingOvertime(useComputed) / timeConstSecond);
  parser.addSymbol("PointGross", runner.getRogainingPointsGross(useComputed));

  parser.addSymbol("PointAdjustment", runner.getPointAdjustment());
  parser.addSymbol("TimeAdjustment", runner.getTimeAdjustment(true) / timeConstSecond);

  parser.addSymbol("TotalStatus", runner.getTotalStatus(false));
  parser.addSymbol("TotalTime", runner.getTotalRunningTime()/timeConstSecond);
  parser.addSymbol("TotalPlace", runner.getTotalPlace(false));

  parser.addSymbol("InputStatus", runner.getInputStatus());
  parser.addSymbol("InputTime", runner.getInputTime() / timeConstSecond);
  parser.addSymbol("InputPlace", runner.getInputPlace());
  parser.addSymbol("InputPoints", runner.getInputPoints());
  parser.addSymbol("Shorten", runner.getNumShortening());

  parser.addSymbol("DataA", runner.getDCI().getInt("DataA"));
  parser.addSymbol("DataB", runner.getDCI().getInt("DataB"));

  pClass cls = runner.getClassRef(true);
  if (cls) {
    parser.addSymbol("ClassDataA", cls->getDCI().getInt("DataA"));
    parser.addSymbol("ClassDataB", cls->getDCI().getInt("DataB"));
  }
  else {
    parser.addSymbol("ClassDataA", 0);
    parser.addSymbol("ClassDataB", 0);
  }

  vector<RunnerStatus> inst;
  vector<int> times;
  vector<int> points;
  vector<int> places;
  vector<int> iinst;
  runner.getInputResults(inst, times, points, places);
  for (RunnerStatus s : inst)
    iinst.push_back(s);

  for (int &t : times)
    t /= timeConstSecond;

  parser.addSymbol("StageStatus", iinst);
  parser.addSymbol("StageTime", times);
  parser.addSymbol("StagePlace", places);
  parser.addSymbol("StagePoints", points);

  parser.addSymbol("InputStatus", runner.getInputStatus());
  parser.addSymbol("InputTime", runner.getInputTime() / timeConstSecond);
  parser.addSymbol("InputPlace", runner.getInputPlace());
  parser.addSymbol("InputPoints", runner.getInputPoints());

  parser.addSymbol("Fee", runner.getDCI().getInt("Fee"));

  const pClub pc = runner.getClubRef();
  if (pc) {
    parser.addSymbol("ClubId", pc->getId());
    parser.addSymbol("DistrictId", pc->getDCI().getInt("District"));
  }
  else {
    parser.addSymbol("ClubId", 0);
    parser.addSymbol("DistrictId", 0);
  }
  parser.addSymbol("Bib", _wtoi(runner.getBib().c_str()));
}

void DynamicResult::prepareCalculations(oTeam &team, bool classResult) const {
  GeneralResult::prepareCalculations(team, classResult);
  prepareCommon(team, classResult);
  int nr = team.getNumRunners();
  vector<int> status(nr), time(nr), start(nr), finish(nr), points(nr), dataA(nr), dataB(nr);
  vector< vector<int> > runnerOutputTimes(nr);
  vector< vector<int> > runnerOutputNumbers(nr);

  for (int k = 0; k < nr; k++) {
    pRunner r = team.getRunner(k);
    if (r) {
      const oAbstractRunner::TempResult &res = r->getTempResult();
      status[k] = res.getStatus();
      time[k] = res.getRunningTime() / timeConstSecond;
      if (time[k] > 0 && status[k] == StatusUnknown)
        status[k] = StatusOK;
      start[k] = res.getStartTime() / timeConstSecond;
      finish[k] = res.getFinishTime() / timeConstSecond;
      points[k] = res.getPoints();
      if (classResult) {
        r->updateComputedResultFromTemp();
      }

      runnerOutputTimes[k] = res.outputTimes;
      runnerOutputNumbers[k] = res.outputNumbers;
      dataA[k] = r->getDCI().getInt("DataA");
      dataB[k] = r->getDCI().getInt("DataB");
    }
  }
  parser.removeSymbol("CardControls");
  parser.removeSymbol("CardPunches");
  parser.removeSymbol("CardTimes");
  parser.removeSymbol("Course");
  parser.removeSymbol("CourseLength");
  parser.removeSymbol("CourseId");
  parser.removeSymbol("LegPlace");
  parser.removeSymbol("LegTimeAfter");
  parser.removeSymbol("LegTimeDeviation");
  parser.removeSymbol("Leg");
  parser.removeSymbol("SplitTimes");
  parser.removeSymbol("SplitTimesAccumulated");
  parser.removeSymbol("LegTimeDeviation");
  parser.removeSymbol("BirthYear");
  parser.removeSymbol("Age");
  
  parser.addSymbol("RunnerOutputNumbers", runnerOutputNumbers);
  parser.addSymbol("RunnerOutputTimes", runnerOutputTimes);

  parser.addSymbol("RunnerStatus", status);
  parser.addSymbol("RunnerTime", time);
  parser.addSymbol("RunnerStart", start);
  parser.addSymbol("RunnerFinish", finish);
  parser.addSymbol("RunnerPoints", points);

  parser.addSymbol("RunnerDataA", dataA);
  parser.addSymbol("RunnerDataB", dataB);

  parser.addSymbol("PatrolRogainingScore", team.getRogainingPatrolPoints(false));
  parser.addSymbol("PatrolRogainingReduction", team.getRogainingPatrolReduction());
  parser.addSymbol("PatrolRogainingOvertime", team.getRogainingPatrolOvertime());

  parser.addSymbol("RunnerCardPunches", team.getResultCache(oTeam::RCCCardPunches));
  parser.addSymbol("RunnerCardTimes", team.getResultCache(oTeam::RCCCardTimes));
  parser.addSymbol("RunnerCardControls", team.getResultCache(oTeam::RCCCardControls));

  parser.addSymbol("RunnerCourse", team.getResultCache(oTeam::RCCCourse));
  parser.addSymbol("RunnerSplitTimes", team.getResultCache(oTeam::RCCSplitTime));

  pClass cls = team.getClassRef(true);
  if (cls) {
    int nl = max<int>(1, cls->getNumStages() - 1);
    parser.addSymbol("ShortestClassTime", cls->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, nl, false, false) / timeConstSecond);
  }
}

void DynamicResult::prepareCalculations(oRunner &runner, bool classResult) const {
  GeneralResult::prepareCalculations(runner, classResult);
  prepareCommon(runner, classResult);
  pCard pc = runner.getCard();
  if (pc) {
    vector<pPunch> punches;
    pc->getPunches(punches);
    int np = 0;
    for (size_t k = 0; k < punches.size(); k++) {
      if (punches[k]->getTypeCode() >= 30)
        np++;
    }
    vector<int> times(np);
    vector<int> codes(np);
    vector<int> controls(np);
    int ip = 0;
    for (size_t k = 0; k < punches.size(); k++) {
      if (punches[k]->getTypeCode() >= 30) {
        times[ip] = punches[k]->getAdjustedTime() / timeConstSecond;
        codes[ip] = punches[k]->getTypeCode();
        controls[ip] = punches[k]->isUsedInCourse() ? punches[k]->getControlId() : -1;
        ip++;
      }
    }
    parser.addSymbol("CardPunches", codes);
    parser.addSymbol("CardTimes", times);
    parser.addSymbol("CardControls", controls);

    pTeam t = runner.getTeam();
    if (t) {
      int leg = runner.getLegNumber();
      t->setResultCache(oTeam::RCCCardTimes, leg, times);
      t->setResultCache(oTeam::RCCCardPunches, leg, codes);
      t->setResultCache(oTeam::RCCCardControls, leg, controls);
    }
  }
  else {
    vector<int> e;
    parser.addSymbol("CardPunches", e);
    parser.addSymbol("CardTimes", e);
    parser.addSymbol("CardControls", e);
  }

  pCourse crs = runner.getCourse(true);
  const vector<SplitData> &sp = runner.getSplitTimes(false);

  if (crs) {
    vector<int> eCrs;
    vector<int> eSplitTime;
    vector<int> eAccTime;
    eCrs.reserve(crs->getNumControls());
    eSplitTime.reserve(crs->getNumControls());
    eAccTime.reserve(crs->getNumControls());
    int start = runner.getStartTime();
    int st = runner.getStartTime();
    for (int k = 0; k < crs->getNumControls(); k++) {
      pControl ctrl = crs->getControl(k);
      if (ctrl->isSingleStatusOK()) {
        eCrs.push_back(ctrl->getFirstNumber());
        if (size_t(k) < sp.size()) {
          if (sp[k].hasTime()) {
            eAccTime.push_back((sp[k].getTime(false) - start) / timeConstSecond);
            eSplitTime.push_back((sp[k].getTime(false) - st) / timeConstSecond);
            st = sp[k].getTime(false);
          }
          else if (sp[k].getStatus() == SplitData::SplitStatus::NoTime) {
            eAccTime.push_back( (st - start) / timeConstSecond);
            eSplitTime.push_back(0);
          }
          else if (sp[k].isMissing()) {
            eAccTime.push_back(0);
            eSplitTime.push_back(-1);
          }
        }
      }
    }
    if (runner.getFinishTime() > 0) {
      eAccTime.push_back((runner.getFinishTime() - start) / timeConstSecond);
      eSplitTime.push_back((runner.getFinishTime() - st) / timeConstSecond);
    }
    else if (!eAccTime.empty()) {
      eAccTime.push_back(0);
      eSplitTime.push_back(-1);
    }

    parser.addSymbol("CourseLength", crs->getLength());
    parser.addSymbol("CourseId", crs->getId());
    parser.addSymbol("Course", eCrs);
    parser.addSymbol("SplitTimes", eSplitTime);
    parser.addSymbol("SplitTimesAccumulated", eAccTime);
    pTeam t = runner.getTeam();
    if (t) {
      int leg = runner.getLegNumber();
      t->setResultCache(oTeam::RCCCourse, leg, eCrs);
      t->setResultCache(oTeam::RCCSplitTime, leg, eSplitTime);
    }
  }
  else {
    vector<int> e;
    parser.addSymbol("CourseLength", -1);
    parser.addSymbol("CourseId", 0);
    parser.addSymbol("Course", e);
    parser.addSymbol("SplitTimes", e);
    parser.addSymbol("SplitTimesAccumulated", e);
  }

  pClass cls = runner.getClassRef(true);
  if (cls) {
    int nl = runner.getLegNumber();
    parser.addSymbol("ShortestClassTime", cls->getBestLegTime(oClass::AllowRecompute::Yes, nl, false) / timeConstSecond);
  }
  vector<int> delta;
  vector<int> place;
  vector<int> after;
  runner.getSplitAnalysis(delta);
  runner.getLegTimeAfter(after);
  runner.getLegPlaces(place);

  parser.addSymbol("LegTimeDeviation", delta);
  parser.addSymbol("LegTimeAfter", after);
  parser.addSymbol("LegPlace", place);
  parser.addSymbol("Leg", runner.getLegNumber());
  parser.addSymbol("BirthYear", runner.getBirthYear());
  int ba = runner.getBirthAge();
  parser.addSymbol("Age", ba);
  parser.addSymbol("AgeLowLimit", lowAgeLimit);
  parser.addSymbol("AgeHighLimit", highAgeLimit);

  parser.addSymbol("CheckTime", runner.getCheckTime() / timeConstSecond);
}

void DynamicResult::storeOutput(vector<int> &times, vector<int> &numbers) const {
  parser.takeVariable("OutputTimes", times);
  parser.takeVariable("OutputNumbers", numbers);
}

int checksum(const string &str);
int checksum(const wstring &str);

long long DynamicResult::getHashCode() const {
  long long hc = 1;
  for (size_t k = 0; k < methods.size(); k++) {
    hc = hc * 997 + checksum(methods[k].source);
  }
  return hc;
}

string DynamicResult::undecorateTag(const string &inputTag) {
  int ix = inputTag.rfind("-v");
  int limit = inputTag.length() - 4;
  if (ix > 0 && ix != inputTag.npos && ix >= limit)
    return inputTag.substr(0, ix);
  else
    return inputTag;
}

bool DynamicResult::isRogaining() const {
  return !methods[DynamicMethods::MDeduceRPoints].source.empty();
}

wstring DynamicResult::getName(bool withAnnotation) const {
  if (annotation.empty() || !withAnnotation)
    return name;
  else
    return name + L" (" + annotation + L")";
}

void DynamicResult::debugDumpVariables(gdioutput &gdi, bool includeSymbols) const {
  gdi.fillDown();
  gdi.dropLine();
  int c1 = gdi.getCX();
  int c2 = c1 + gdi.scaleLength(170);
  if (includeSymbols) {
    gdi.addString("", 1, "Symboler");
    parser.dumpSymbols(gdi, c1, c2);
    gdi.dropLine();
  }
  else {
    gdi.addString("", 1, "Variabler");
    parser.dumpVariables(gdi, c1, c2);
    gdi.dropLine();
  }
}

void GeneralResult::calculateIndividualResults(vector<pRunner> &runners,
                                               const pair<int, int> & controlId,
                                               bool totalResults,
                                               bool inclForestRunners,
                                               bool inclPreliminary,
                                               const string &resTag,
                                               oListInfo::ResultType resType,
                                               int inputNumber,
                                               oEvent &oe,
                                               vector<GeneralResultInfo> &results) {

  results.reserve(runners.size());

  if (resTag.empty() && resType == ClassWise) {
    GeneralResultInfo ri;
    if (controlId.second == oPunch::PunchFinish &&
        controlId.first == oPunch::PunchStart) {

      if (!totalResults) {
        set<int> clsId;
        for (pRunner r : runners) {
          clsId.insert(r->getClassId(true));
        }
        oe.calculateResults(clsId, oEvent::ResultType::ClassResult, inclPreliminary);
        for (pRunner r : runners) {
          ri.status = r->getStatus();
          if (ri.status == StatusUnknown) {
            if (r->getFinishTime() == 0) {
              if (!inclForestRunners)
                continue;
              else {
                if (r->getClubId() == cVacantId)
                  continue;
                ri.status = StatusUnknown;
              }
            }
            else {
              ri.status = StatusOK; // Preliminary status
            }
          }
          if (ri.status == StatusOK)
            ri.place = r->getPlace();
          else
            ri.place = 0;

          ri.score = r->getRogainingPoints(true, false);
          ri.time = r->getRunningTime(true);
          ri.src = r;
          results.push_back(ri);
        }
      }
      else {
        oe.calculateResults(set<int>(), oEvent::ResultType::TotalResult, inclPreliminary);
        for (pRunner r : runners) {
          ri.status = r->getTotalStatus(false);
          if (ri.status == StatusUnknown && r->getInputStatus() == StatusOK) {
            if (r->getFinishTime() == 0) {
              if (!inclForestRunners)
                continue;
              else {
                if (r->getClubId() == cVacantId)
                  continue;
                ri.status = StatusUnknown;
              }
            }
            else {
              ri.status = StatusOK; // Preliminary status
            }
          }
          if (ri.status == StatusOK)
            ri.place = r->getTotalPlace();
          else
            ri.place = 0;

          ri.score = r->getRogainingPoints(true, true);
          ri.time = r->getTotalRunningTime();
          ri.src = r;
          results.push_back(ri);
        }
      }
    }
    else {
      oe.calculateSplitResults(controlId.first, controlId.second);

      ri.score = 0; // Undefined
      for (pRunner r : runners) {
        ri.status = r->getTempStatus();
        if (ri.status == StatusUnknown) {
          continue;
        }
        if (ri.status == StatusMP && r->getStatus() != StatusMP)
          continue; // Control that is not needed requested

        if (ri.status == StatusOK)
          ri.place = r->getPlace();
        else
          ri.place = 0;

        ri.time = r->getTempTime();

        if (ri.time <= 0 && ri.status == StatusOK)
          continue;
        ri.src = r;
        results.push_back(ri);
      }
    }
  }
  else {
    wstring srcDMY;
    GeneralResult *gResult = nullptr;
    shared_ptr<GeneralResult> specialInstance;
    oListParam param;
    param.useControlIdResultTo = controlId.second;
    param.useControlIdResultFrom = controlId.first;

    if (!resTag.empty())
      gResult = oe.getGeneralResult(resTag, srcDMY).get();
    else {
      if (controlId.second == oPunch::PunchFinish &&
          controlId.first == oPunch::PunchStart && !totalResults)
        specialInstance = make_shared<GeneralResult>();
      else if (!totalResults)
        specialInstance = make_shared<ResultAtControl>();
      else
        specialInstance = make_shared<TotalResultAtControl>();

      gResult = specialInstance.get();
    }
    gResult->setContext(&param);
    gResult->calculateIndividualResults(runners, false, resType, true, inputNumber);
    gResult->clearContext();
    map<int, bool> mapHasCourse;
    GeneralResultInfo ri;
    for (pRunner r : runners) {
      const auto &tmp = r->getTempResult(0);
      ri.status = tmp.getStatus();
      if (ri.status == StatusUnknown) {
        if (r->getFinishTime() == 0) {
          if (!inclForestRunners)
            continue;
        }
        else
          ri.status = StatusOK; // Preliminary status
      }
      if (ri.status == StatusOK)
        ri.place = tmp.getPlace();
      else
        ri.place = 0;

      if ((controlId.first != oPunch::PunchStart ||
          controlId.second != oPunch::PunchFinish) && ri.status == StatusMP && r->getStatus() != StatusMP) {
        if (!inclForestRunners)
          continue;
      }

      ri.score = tmp.getPoints();
      ri.time = tmp.getRunningTime();


      if (controlId.first != oPunch::PunchStart && ri.time <= 0 && ri.status == StatusOK)
        continue; // Wrong order, skip

      ri.src = r;
      results.push_back(ri);
    }
  }
}

shared_ptr<GeneralResult::BaseResultContext>
GeneralResult::calculateTeamResults(vector<pTeam> &teams,
                                    int leg,
                                    const pair<int, int> &controlId,
                                    bool totalResults,
                                    const string &resTag,
                                    oListInfo::ResultType resType,
                                    int inputNumber,
                                    oEvent &oe,
                                    vector<GeneralResultInfo> &results) {
  shared_ptr<BaseResultContext> out = make_shared<BaseResultContext>();
  out->leg = leg;
  out->controlId = controlId;
  out->totalResults = totalResults;
  results.reserve(teams.size());

  if (resTag.empty()) {
    out->useModule = false;
    if (controlId.second == oPunch::PunchFinish) {
      oe.calculateTeamResults(teams, oEvent::ResultType::ClassResult);
      GeneralResultInfo ri;
      for (pTeam r : teams) {
        ri.status = r->getStatus();
        if (ri.status == StatusUnknown) {
          if (r->getFinishTime() == 0)
            continue;
          ri.status = StatusOK; // Preliminary status
        }
        if (ri.status == StatusOK)
          ri.place = r->getPlace();
        else
          ri.place = 0;

        ri.score = r->getRogainingPoints(true, false);
        ri.status = r->getStatus();
        ri.time = r->getRunningTime(true);
        ri.src = r;
        results.push_back(ri);
      }
    }
    else {
      set<int> clsId;
      for (pTeam t : teams)
        clsId.insert(t->getClassId(false));

      oe.calculateTeamResultAtControl(clsId, leg, controlId.second, totalResults);
      GeneralResultInfo ri;
      for (pTeam r : teams) {
        const auto &tmp = r->getTempResult(0);
        ri.status = tmp.getStatus();
        if (ri.status == StatusUnknown || ri.status == StatusMP)
          continue;
        ri.place = tmp.getPlace();
        ri.score = tmp.getPoints();
        ri.time = tmp.getRunningTime();
        ri.src = r;
        results.push_back(ri);
      }
    }
  }
  else {
    out->useModule = true;
    wstring srcDMY;
    auto &gResult = oe.getGeneralResult(resTag, srcDMY);
    gResult->calculateTeamResults(teams, false, resType, true, inputNumber);
    GeneralResultInfo ri;
    for (pTeam r : teams) {
      const auto &tmp = r->getTempResult(0);
      ri.status = tmp.getStatus();
      if (ri.status == StatusUnknown) {
        if (r->getFinishTime() == 0)
          continue;
        ri.status = StatusOK; // Preliminary status
      }
      if (ri.status == StatusOK)
        ri.place = tmp.getPlace();
      else
        ri.place = 0;

      ri.score = tmp.getPoints();
      ri.time = tmp.getRunningTime();
      ri.src = r;
      results.push_back(ri);
    }
  }

  return out;
}

int GeneralResult::GeneralResultInfo::getNumSubresult(const BaseResultContext &context) const {
  int cid = src->getClassId(false);
  if (cid == 0)
    return 0;

  if (context.resIntervalCache.count(cid)) {
    const auto &cached = context.resIntervalCache[cid];
    return cached.second - cached.first;
  }
  else {
    const pClass cls = src->getClassRef(false);
    int leg = context.leg == -1 ? cls->getNumStages() : context.leg;

    int nb = cls->getNextBaseLeg(leg);
    if (nb == -1)
      nb = cls->getNumStages();

    int pb = cls->getPreceedingLeg(leg) + 1;

    context.resIntervalCache[cid] = make_pair(pb, nb);
    return nb - pb;
  }
}

bool GeneralResult::GeneralResultInfo::getSubResult(const BaseResultContext &context,
                                                    int ix, GeneralResultInfo &out) const {

  if (getNumSubresult(context) == 0)
    return false; // Ensure cache is setup
  int base = context.resIntervalCache[src->getClassId(false)].first;
  pRunner r = pTeam(src)->getRunner(base + ix);
  if (r == 0)
    return false;
  out.place = 0; // Not defined
  out.src = r;

  if (context.useModule) {
    out.score = r->tmpResult.getPoints();
    out.status = r->tmpResult.getStatus();
    out.time = r->tmpResult.getRunningTime();
  }
  else {
    if (context.controlId.second == oPunch::PunchFinish) {
      out.score = r->getRogainingPoints(true, false);
      out.status = r->getStatus();
      out.time = r->getRunningTime(true);
    }
    else {
      out.score = 0; // Undefined
      r->getSplitTime(context.controlId.second, out.status, out.time);
    }
  }

  if (out.status == StatusUnknown && out.time > 0)
    out.status = StatusOK;

  if (out.status == StatusUnknown)
    return false;

  return true;
}
