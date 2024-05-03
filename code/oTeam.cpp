/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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
#include "meos_util.h"
#include "oEvent.h"
#include <assert.h>
#include <algorithm>
#include "Table.h"
#include "localizer.h"
#include "meosException.h"
#include "gdioutput.h"

oTeam::oTeam(oEvent *poe): oAbstractRunner(poe, false)
{
  Id=oe->getFreeTeamId();
  getDI().initData();
  correctionNeeded = false;
  StartNo=0;
}

oTeam::oTeam(oEvent *poe, int id): oAbstractRunner(poe, true) {
  Id=id;
  oe->qFreeTeamId = max(id, oe->qFreeTeamId);
  getDI().initData();
  correctionNeeded = false;
  StartNo=0;
}

oTeam::~oTeam(void) {
  /*for(unsigned i=0; i<Runners.size(); i++) {
    if (Runners[i] && Runners[i]->tInTeam==this) {
      assert(Runners[i]->tInTeam!=this);
      Runners[i]=0;
    }
  }*/
}

void oTeam::prepareRemove()
{
  for(unsigned i=0; i<Runners.size(); i++)
    setRunnerInternal(i, 0);
}

bool oTeam::write(xmlparser &xml)
{
  if (Removed) return true;
  
  xml.startTag("Team");
  xml.write("Id", Id);
  xml.write("StartNo", StartNo);
  xml.write("Updated", getStamp());
  xml.write("Name", sName);
  xml.writeTime("Start", startTime);
  xml.writeTime("Finish", FinishTime);
  xml.write("Status", status);
  xml.write("Runners", getRunners());

  if (Club) xml.write("Club", Club->Id);
  if (Class) xml.write("Class", Class->Id);

  xml.write("InputPoint", inputPoints);
  if (inputStatus != StatusOK)
    xml.write("InputStatus", itos(inputStatus)); //Force write of 0
  xml.writeTime("InputTime", inputTime);
  xml.write("InputPlace", inputPlace);

  getDI().write(xml);
  xml.endTag();

  return true;
}

void oTeam::set(const xmlobject &xo)
{
  xmlList xl;
  xo.getObjects(xl);
  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id=it->getInt();
    }
    else if (it->is("Name")){
      sName=it->getWStr();
    }
    else if (it->is("StartNo")){
      StartNo=it->getInt();
    }
    else if (it->is("Start")){
      tStartTime = startTime = it->getRelativeTime();
    }
    else if (it->is("Finish")){
      FinishTime=it->getRelativeTime();
    }
    else if (it->is("Status")) {
      unsigned rawStatus = it->getInt();
      tStatus = status=RunnerStatus(rawStatus < 100u ? rawStatus : 0);
    }
    else if (it->is("Class")){
      Class=oe->getClass(it->getInt());
    }
    else if (it->is("Club")){
      Club=oe->getClub(it->getInt());
    }
    else if (it->is("Runners")){
      vector<int> r;
      decodeRunners(it->getRawStr(), r);
      importRunners(r);
    }
    else if (it->is("InputTime")) {
      inputTime = it->getRelativeTime();
    }
    else if (it->is("InputStatus")) {
      unsigned rawStatus = it->getInt();
      inputStatus = RunnerStatus(rawStatus < 100u ? rawStatus : 0);
    }
    else if (it->is("InputPoint")) {
      inputPoints = it->getInt();
    }
    else if (it->is("InputPlace")) {
      inputPlace = it->getInt();
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRawStr());
    }
    else if (it->is("oData")) {
      getDI().set(*it);
    }
  }
}

string oTeam::getRunners() const
{
  string str="";
  char bf[16];
  unsigned m=0;

  for(m=0;m<Runners.size();m++){
    if (Runners[m]){
      sprintf_s(bf, 16, "%d;", Runners[m]->getId());
      str+=bf;
    }
    else str+="0;";
  }
  return str;
}

void oTeam::decodeRunners(const string &rns, vector<int> &rid)
{
  const char *str=rns.c_str();
  rid.clear();

  int n=0;

  while (*str) {
    int cid=atoi(str);

    while(*str && (*str!=';' && *str!=',')) str++;
    if (*str==';'  || *str==',') str++;

    rid.push_back(cid);
    n++;
  }
}

void oTeam::importRunners(const vector<int> &rns) {
  Runners.resize(rns.size());
  for (size_t n=0;n<rns.size(); n++) {
    if (rns[n] > 0)
      Runners[n] = oe->getRunner(rns[n], 0);
    else
      Runners[n] = nullptr;

    pRunner r = Runners[n];
    if (r) {
      r->tInTeam = this;
      r->tLeg = n;
    }
  }
}

void oTeam::importRunners(const vector<pRunner> &rns)
{
  // Unlink old runners
  for (size_t k = 0; k<Runners.size(); k++) {
    pRunner r = Runners[k];
    if (r && r->tInTeam == this) {
      r->tInTeam = 0;
      r->tLeg = 0;
    }
  }

  Runners.resize(rns.size());
  for (size_t n = 0; n < rns.size(); n++) {
    Runners[n] = rns[n];
    if (rns[n] && isAddedToEvent()) {
      rns[n]->tInTeam = this;
      rns[n]->tLeg = n;
    }
  }
}

void oEvent::removeTeam(int Id)
{
  oTeamList::iterator it;
  for (it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->getId() == Id) {
      if (hasDBConnection() && !it->isRemoved())
        sqlRemove(&*it);
      dataRevision++;
      it->prepareRemove();
      Teams.erase(it);
      teamById.erase(Id);
      updateTabs();
      return;
    }
  }
}

void oTeam::setRunner(unsigned i, pRunner r, bool sync)
{
  if (i>=Runners.size()) {
    if (i>=0 && i<100)
      Runners.resize(i+1);
    else
      throw std::exception("Bad runner index");
  }

  if (Runners[i] == r)
    return;

  int oldRaceId = 0;
  pRunner tr = Runners[i];
  if (tr) {
    oldRaceId = tr->getDCI().getInt("RaceId");
    tr->getDI().setInt("RaceId", 0);
  }
  setRunnerInternal(i, r);

  if (r) {
    if (tStatus == StatusDNS)
      setStatus(StatusUnknown, true, ChangeType::Update);
    r->getDI().setInt("RaceId", oldRaceId);
    r->tInTeam=this;
    r->tLeg=i;
    r->createMultiRunner(true, sync);
  }

  if (Class) {
    int index=1; //Set multi runners
    for (unsigned k=i+1;k<Class->getNumStages(); k++) {
      if (Class->getLegRunner(k)==i) {
        if (r!=0) {
          pRunner mr=r->getMultiRunner(index++);

          if (mr) {
            mr->setName(r->getName(), true);
            mr->synchronize();
            setRunnerInternal(k, mr);
          }
        }
        else setRunnerInternal(k, 0);
      }
    }
  }
}

void oTeam::setRunnerInternal(int k, pRunner r)
{
  if (r == Runners[k]) {
    if (r) {
      r->tInTeam = this;
      r->tLeg = k;
    }
    return;
  }

  int specifiedCourse = 0;
  pRunner rOld = Runners[k];
  if (rOld) {
    int specifiedCourse = rOld->getCourseId();
    assert(rOld->tInTeam == 0 || rOld->tInTeam == this);
    rOld->tInTeam = 0;
    rOld->tLeg = 0;
  }

  // Reset old team
  if (r && r->tInTeam) {
    if (r->tInTeam->Runners[r->tLeg]) {
      r->tInTeam->Runners[r->tLeg] = nullptr;
      r->tInTeam->updateChanged();
      if (r->tInTeam != this)
        r->tInTeam->synchronize(true);
    }
  }

  Runners[k] = r;

  if (r) {
    if (specifiedCourse)
      r->setCourseId(specifiedCourse);
    r->tInTeam = this;
    r->tLeg = k;
    if (Class && (r->Class==nullptr || Class->getLegType(k) != LTGroup))
      r->setClassId(getClassId(false), false);
  }
  updateChanged();
}

wstring oTeam::getLegFinishTimeS(int leg, SubSecond mode) const
{
  if (leg==-1)
    leg=Runners.size()-1;

  if (unsigned(leg)<Runners.size() && Runners[leg])
    return Runners[leg]->getFinishTimeS(true, mode);
  else return L"-";
}

int oTeam::getLegToUse(int leg) const {
  if (Runners.empty())
    return 0;
  if (leg==-1)
    leg=Runners.size()-1;
  int oleg = leg;
  if (Class && !Runners[leg]) {
    LegTypes lt = Class->getLegType(leg);
    while (leg>=0 && (lt == LTParallelOptional || lt == LTExtra || lt == LTIgnore) && !Runners[leg]) {
      if (leg == 0)
        return oleg; //Suitable leg not found
      leg--;
      lt = Class->getLegType(leg);
    }
  }
  return leg;
}

int oTeam::getLegFinishTime(int leg) const
{
  leg = getLegToUse(leg);
  //if (leg==-1)
  //leg=Runners.size()-1;

  if (Class) {
    pClass pc = Class;
    LegTypes lt = pc->getLegType(leg);
    while (leg> 0  && (lt == LTIgnore ||
              (lt == LTExtra && (!Runners[leg] || Runners[leg]->getFinishTime() <= 0)) ) ) {
      leg--;
      lt = pc->getLegType(leg);
    }
  }

  if (unsigned(leg)<Runners.size() && Runners[leg]) {
    int ft = Runners[leg]->getFinishTime();
    if (Class) {
      bool extra = Class->getLegType(leg) == LTExtra ||
                   Class->getLegType(leg+1) == LTExtra; 

      bool par = Class->isParallel(leg) ||
                 Class->isParallel(leg+1); 

      if (extra) {
        ft = 0;
        // Minimum over extra legs
        int ileg = leg;
        while (ileg > 0 && Class->getLegType(ileg) == LTExtra)
          ileg--;

        while (size_t(ileg) < Class->getNumStages()) {
          int ift = 0;
          if (Runners[ileg]) {
            ift = Runners[ileg]->getFinishTimeAdjusted(true);
          }

          if (ift > 0) {
            if (ft == 0)
              ft = ift;
            else
              ft = min(ft, ift);
          }

          ileg++;
          if (Class->getLegType(ileg) != LTExtra)
            break;
        }
      }
      else if (par) {
        ft = 0;
        // Maximum over parallel legs
        int ileg = leg;
        while (ileg > 0 && Class->isParallel(ileg))
          ileg--;

        while (size_t(ileg) < Class->getNumStages()) {
          int ift = 0;
          if (Runners[ileg]) {
            ift = Runners[ileg]->getFinishTimeAdjusted(true);
          }

          if (ift > 0) {
            if (ft == 0)
              ft = ift;
            else
              ft = max(ft, ift);
          }

          ileg++;
          if (!Class->isParallel(ileg))
            break;
        }
      }
    }
    return ft;
  }
  else return 0;
}

int oTeam::getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const {
  int off = multidayTotal ? max(0, getInputTime()) : 0;
  if (!Class || leg == 0)
    return off;
  int pleg = Class->getPreceedingLeg(leg);
  if (pleg < 0)
    return off;

  return getLegRunningTime(pleg, false, multidayTotal);
}

int oTeam::getRunningTime(bool computedTime) const {
  return getLegRunningTime(-1, computedTime, false);
}

int oTeam::getLegRunningTime(int leg, bool computedTime, bool multidayTotal) const {
  if (computedTime) {
    leg = getLegToUse(leg);
    auto &cr = getComputedResult(leg);

    int addon = 0;
    if (multidayTotal)
      addon = inputTime;

    if (cr.version == oe->dataRevision) {
      if (cr.time > 0)
        return cr.time + addon; // Time adjustment already included in time ??
      else
        return 0;
    }
  }
  bool isLastLeg = (leg == -1 || leg + 1 == Runners.size());
  return getLegRunningTimeUnadjusted(leg, multidayTotal, false) + (isLastLeg ? getTimeAdjustment(false) : 0);
}

int oTeam::getLegRestingTime(int leg, bool useComputedRunnerTime) const {
  if (!Class)
    return 0;

  int rest = 0;
  int R = min<int>(Runners.size(), leg+1);
  for (int k = 1; k < R; k++) {
    if (Class->getStartType(k) == STPursuit && !Class->isParallel(k) &&
        Runners[k] && Runners[k-1]) {
         
      int ft = getLegRunningTimeUnadjusted(k-1, false, useComputedRunnerTime) + tStartTime;
      int st = Runners[k]->getStartTime();

      if (ft > 0 && st > 0)
        rest += st - ft;      
    }
  }
  return rest;
}
  

int oTeam::getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputedRunnerTime) const {
  leg = getLegToUse(leg);

  int addon = 0;
  if (multidayTotal)
    addon = inputTime;

  if (unsigned(leg)<Runners.size() && Runners[leg]) {
    if (Class) {
      pClass pc=Class;

      LegTypes lt = pc->getLegType(leg);
      LegTypes ltNext = pc->getLegType(leg+1);
      if (ltNext == LTParallel || ltNext == LTParallelOptional || ltNext == LTExtra) // If the following leg is parallel, then so is this.
        lt = ltNext;

      switch(lt) {
        case LTNormal:
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, true, false)) {
            int dt = leg>0 ? getLegRunningTimeUnadjusted(leg-1, false, useComputedRunnerTime)+Runners[leg]->getRunningTime(useComputedRunnerTime):0;
            return addon + max(Runners[leg]->getFinishTimeAdjusted(true) - 
                    (tStartTime + getLegRestingTime(leg, useComputedRunnerTime)), dt);
          }
          else return 0;
        break;

        case LTParallelOptional:
        case LTParallel: //Take the longest time of this runner and the previous
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, false, false)) {
            int pt=leg>0 ? getLegRunningTimeUnadjusted(leg-1, false, useComputedRunnerTime) : 0;
            int rest = getLegRestingTime(leg, useComputedRunnerTime);
            int finishT = Runners[leg]->getFinishTimeAdjusted(true);
            return addon + max(finishT-(tStartTime + rest), pt);
          }
          else return 0;
        break;

        case LTExtra: //Take the best time of this runner and the previous
          if (leg==0)
            return addon + max(Runners[leg]->getFinishTime()-tStartTime, 0);
          else {
            int baseLeg = leg;
            while (baseLeg > 0 && pc->getLegType(baseLeg) == LTExtra)
              baseLeg--;
            int baseTime = 0;
            if (baseLeg > 0)
              baseTime = getLegRunningTimeUnadjusted(baseLeg-1, multidayTotal, useComputedRunnerTime);
            else 
              baseTime = addon;
        
            int cLeg = baseLeg;
            int legTime = 0;
            bool bad = false;
            do {
              if (Runners[cLeg] && Runners[cLeg]->getFinishTime() > 0) {
                int rt = Runners[cLeg]->getRunningTime(useComputedRunnerTime);
                if (legTime == 0 || rt < legTime) {
                  bad = !Runners[cLeg]->prelStatusOK(useComputedRunnerTime, false, false);
                  legTime = rt;
                }
              }
              cLeg++;
            }
            while (pc->getLegType(cLeg) == LTExtra);

            if (bad || legTime == 0)
              return 0;
            else 
              return baseTime + legTime;
          }
        break;

        case LTSum:
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, false, false)) {
            if (leg==0)
              return addon + Runners[leg]->getRunningTime(useComputedRunnerTime);
            else {
              int prev = getLegRunningTimeUnadjusted(leg-1, multidayTotal, useComputedRunnerTime);
              if (prev == 0)
                return 0;
              else
                return Runners[leg]->getRunningTime(useComputedRunnerTime) + prev;
            }
          }
          else return 0;

        case LTIgnore:
          if (leg==0)
            return 0;
          else
            return getLegRunningTimeUnadjusted(leg-1, multidayTotal, useComputedRunnerTime);

        break;

        case LTGroup:
          if (Class->getResultModuleTag().empty())
            return 0;
          else {
            int dt = Runners[leg]->getRunningTime(useComputedRunnerTime);
            if (leg > 0)
              dt += getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime);
            return dt;
          }
      }
    }
    else {
      int dt=addon + max(Runners[leg]->getFinishTime()-tStartTime, 0);
      int dt2=0;

      if (leg > 0)
        dt2 = getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime) +
                        Runners[leg]->getRunningTime(useComputedRunnerTime);

      return max(dt, dt2);
    }
  }
  return 0;
}

wstring oTeam::getLegRunningTimeS(int leg, bool computedTime, bool multidayTotal, SubSecond mode) const
{
  if (leg==-1)
    leg = Runners.size()-1;

  int rt=getLegRunningTime(leg, computedTime, multidayTotal);
  const wstring &bf = formatTime(rt, mode);
  if (rt>0) {
    if ((unsigned(leg)<Runners.size() && Runners[leg] &&
      Class && Runners[leg]->getStartTime()==Class->getRestartTime(leg)) || getNumShortening(leg)>0)
      return L"*" + bf;
  }
  return bf;
}


RunnerStatus oTeam::getLegStatus(int leg, bool computed, bool multidayTotal) const
{
  if (leg==-1)
    leg=Runners.size()-1;

  if (unsigned(leg)>=Runners.size())
    return StatusUnknown;

  if (multidayTotal) {
    RunnerStatus s = getLegStatus(leg, computed, false);
    if (s == StatusUnknown && inputStatus != StatusNotCompetiting)
      return StatusUnknown;
    if (inputStatus == StatusUnknown)
      return StatusDNS;
    return max(inputStatus, s);
  }

  //Let the user specify a global team status disqualified.
  if (leg == (Runners.size()-1) && tStatus==StatusDQ)
    return tStatus;

  leg = getLegToUse(leg); // Ignore optional runners

  int s=0;

  if (!Class)
    return StatusUnknown;

  while(leg>0 && Class->getLegType(leg)==LTIgnore)
    leg--;

  if (computed) {
    auto &cr = getComputedResult(leg);
    if (cr.version == oe->dataRevision)
      return cr.status;
  }

  for (int i=0;i<=leg;i++) {
    // Ignore runners to be ignored
    while(i<leg && Class->getLegType(i)==LTIgnore)
      i++;

    int st=Runners[i] ? Runners[i]->getStatus(): StatusDNS;
    int bestTime=Runners[i] ? Runners[i]->getFinishTime():0;

    //When Type Extra is used, the runner with the best time
    //is used for change. Then the status of this runner
    //should be carried forward.
    if (Class) while( (i+1) < int(Runners.size()) && Class->getLegType(i+1)==LTExtra) {
      i++;

      if (Runners[i]) {
        if (bestTime==0 || (Runners[i]->getFinishTime()>0 &&
                         Runners[i]->getFinishTime()<bestTime) ) {
          st=Runners[i]->getStatus();
          bestTime = Runners[i]->getFinishTime();
        }
      }
    }

    if (st==0)
      return RunnerStatus(s==StatusOK ? 0 : s);

    s=max(s, st);
  }

  // Allow global status DNS
  if (s==StatusUnknown && tStatus==StatusDNS)
    return tStatus;

  return RunnerStatus(s);
}

RunnerStatus oTeam::deduceComputedStatus() const {
  int leg = Runners.size() - 1;
  leg = getLegToUse(leg); // Ignore optional runners
  int s = 0;
  if (!Class)
    return StatusUnknown;

  while (leg>0 && Class->getLegType(leg) == LTIgnore)
    leg--;

  for (int i = 0; i <= leg; i++) {
    // Ignore runners to be ignored
    while (i<leg && Class->getLegType(i) == LTIgnore)
      i++;

    int st = Runners[i] ? Runners[i]->getStatusComputed(false) : StatusDNS;
    int bestTime = Runners[i] ? Runners[i]->getFinishTime() : 0;

    //When Type Extra is used, the runner with the best time
    //is used for change. Then the status of this runner
    //should be carried forward.
    if (Class) while ((i + 1) < int(Runners.size()) && Class->getLegType(i + 1) == LTExtra) {
      i++;

      if (Runners[i]) {
        if (bestTime == 0 || (Runners[i]->getFinishTime()>0 &&
            Runners[i]->getFinishTime()<bestTime)) {
          st = Runners[i]->getStatusComputed(false);
          bestTime = Runners[i]->getFinishTime();
        }
      }
    }

    if (st == 0)
      return RunnerStatus(s == StatusOK ? 0 : s);

    s = max(s, st);
  }

  // Allow global status DNS
  if (s == StatusUnknown && tStatus == StatusDNS)
    return tStatus;
  return RunnerStatus(s);
}

int oTeam::deduceComputedRunningTime() const {
  return getLegRunningTimeUnadjusted(Runners.size() - 1, false, true) + getTimeAdjustment(false);
}

int oTeam::deduceComputedPoints() const {
  int pt = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k])
      pt += Runners[k]->getRogainingPoints(true, false);
  }
  pt = max(0, pt + getPointAdjustment());
  return pt;
}


const wstring &oTeam::getLegStatusS(int leg, bool computed, bool multidayTotal) const
{
  return oe->formatStatus(getLegStatus(leg, computed, multidayTotal), true);
}

RunnerStatus oTeam::getStatusComputed(bool allowUpdate) const {
  auto& p = getTeamPlace(Runners.size()-1);

  if (Class && allowUpdate && p.p.isOld(*oe)) {
    oe->calculateTeamResults(std::set<int>({ getClassId(true) }), oEvent::ResultType::ClassResult);
  }
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus;
}

int oTeam::getLegPlace(int leg, bool multidayTotal, bool allowUpdate) const {
  if (leg == -1)
    leg = Runners.size() - 1;

  if (Class) {
    while (size_t(leg) < Class->legInfo.size() && Class->legInfo[leg].legMethod == LTIgnore)
      leg--;
  }
  auto &p = getTeamPlace(leg);
  if (!multidayTotal) {
    if (Class && allowUpdate && p.p.isOld(*oe)) {
      oe->calculateTeamResults(std::set<int>({getClassId(true)}), oEvent::ResultType::ClassResult);
    }
    return p.p.get(!allowUpdate);
  }
  else {
    if (Class && allowUpdate && p.totalP.isOld(*oe)) {
      oe->calculateTeamResults(std::set<int>({ getClassId(true) }), oEvent::ResultType::TotalResult);
    }
    return p.totalP.get(!allowUpdate);
  }
}

wstring oTeam::getLegPlaceS(int leg, bool multidayTotal) const
{
  int p = getLegPlace(leg, multidayTotal);
  wchar_t bf[16];
  if (p > 0 && p < 10000) {
    swprintf_s(bf, L"%d", p);
    return bf;
  }
  return _EmptyWString;
}

wstring oTeam::getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const
{
  int p=getLegPlace(leg, multidayTotal);
  wchar_t bf[16];
  if (p>0 && p<10000){
    if (withDot) {
      swprintf_s(bf, L"%d.", p);
      return bf;
    }
    else
      return itow(p);
  }
  return _EmptyWString;
}

int oAbstractRunner::compareClubs(const oClub* ca, const oClub* cb) {
  if (ca != cb) {
    if (ca == nullptr && cb)
      return true;
    else if (cb == nullptr)
      return false;

    const wstring an = ca->getName();
    const wstring bn = cb->getName();
    int res = CompareString(LOCALE_USER_DEFAULT, 0,
      an.c_str(), an.length(),
      bn.c_str(), bn.length());

    if (res != CSTR_EQUAL)
      return res == CSTR_LESS_THAN;
  }
  return 2;
}

bool oTeam::compareResultClub(const oTeam& a, const oTeam& b) {
  pClub ca = a.getClubRef();
  pClub cb = b.getClubRef();
  if (ca != cb) {
    int cres = compareClubs(ca, cb);
    if (cres != 2)
      return cres != 0;
  }
  return compareResult(a, b);
}

bool oTeam::compareResult(const oTeam &a, const oTeam &b)
{
  if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) return a.Class->tSortIndex < b.Class->tSortIndex || (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }
  else if (a.tmpSortStatus != b.tmpSortStatus)
    return a.tmpSortStatus < b.tmpSortStatus;
  else if (a.tmpSortTime != b.tmpSortTime)
    return a.tmpSortTime < b.tmpSortTime;

  const wstring &as = a.getBib();
  const wstring &bs = b.getBib();
  if (as != bs) {
    return compareBib(as, bs);
  }

  int aix = a.getDCI().getInt("SortIndex");
  int bix = b.getDCI().getInt("SortIndex");
  if (aix != bix) {
    if (aix == 0)
      aix = numeric_limits<int>::max();
    if (bix == 0)
      bix = numeric_limits<int>::max();
    return aix < bix;
  }

  return CompareString(LOCALE_USER_DEFAULT, 0,
                       a.sName.c_str(), a.sName.length(),
                       b.sName.c_str(), b.sName.length()) == CSTR_LESS_THAN;
}

bool oTeam::compareResultNoSno(const oTeam &a, const oTeam &b)
{
  if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) return a.Class->tSortIndex < b.Class->tSortIndex || (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }
  else if (a.tmpSortStatus != b.tmpSortStatus)
    return a.tmpSortStatus<b.tmpSortStatus;
  else if (a.tmpSortTime != b.tmpSortTime)
    return a.tmpSortTime<b.tmpSortTime;

  int aix = a.getDCI().getInt("SortIndex");
  int bix = b.getDCI().getInt("SortIndex");
  if (aix != bix) {
    if (aix == 0)
      aix = numeric_limits<int>::max();
    if (bix == 0)
      bix = numeric_limits<int>::max();
    return aix < bix;
  }

  pClub ca = a.getClubRef();
  pClub cb = b.getClubRef();
  if (ca != cb) {
    int cres = compareClubs(ca, cb);
    if (cres != 2)
      return cres != 0;
  }

  return CompareString(LOCALE_USER_DEFAULT, 0,
                       a.sName.c_str(), a.sName.length(),
                       b.sName.c_str(), b.sName.length()) == CSTR_LESS_THAN;
}


bool oTeam::compareSNO(const oTeam &a, const oTeam &b) {
  const wstring &as = a.getBib();
  const wstring &bs = b.getBib();

  if (as != bs) {
    return compareBib(as, bs);
  }
  else if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) 
        return a.Class->tSortIndex < b.Class->tSortIndex || (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }

  return CompareString(LOCALE_USER_DEFAULT, 0,
                       a.sName.c_str(), a.sName.length(),
                       b.sName.c_str(), b.sName.length()) == CSTR_LESS_THAN;
}

bool oTeam::isRunnerUsed(int rId) const {
  for(unsigned i=0;i<Runners.size(); i++) {
    if (Runners[i] && Runners[i]->getId() == rId)
      return true;
  }
  return false;
}

void oTeam::setTeamMemberStatus(RunnerStatus dnsStatus)
{
  assert(!isResultStatus(dnsStatus) || dnsStatus == StatusOK);
  setStatus(dnsStatus, true, ChangeType::Update);
  for (unsigned i = 0; i < Runners.size(); i++) {
    if (Runners[i] && (!isResultStatus(Runners[i]->getStatus()) || ((dnsStatus == StatusOutOfCompetition || dnsStatus == StatusNoTiming) && Runners[i]->statusOK(false, false)))) {
      Runners[i]->setStatus(dnsStatus, true, ChangeType::Update);
    }
  }
  // Do not sync here
}

static void compressStartTimes(vector<int> &availableStartTimes, int finishTime)
{
  for (size_t j=0; j<availableStartTimes.size(); j++)
    finishTime = max(finishTime, availableStartTimes[j]);

  availableStartTimes.resize(1);
  availableStartTimes[0] = finishTime;
}

static void addStartTime(vector<int> &availableStartTimes, int finishTime)
{
  for (size_t k=0; k<availableStartTimes.size(); k++)
    if (finishTime >= availableStartTimes[k]) {
      availableStartTimes.insert(availableStartTimes.begin()+k, finishTime);
      return;
    }

  availableStartTimes.push_back(finishTime);
}

static int getBestStartTime(vector<int> &availableStartTimes) {
  if (availableStartTimes.empty())
    return 0;
  int t = availableStartTimes.back();
  availableStartTimes.pop_back();
  return t;
}

void oTeam::quickApply() {
  if (unsigned(status) >= 100)
    status = StatusUnknown; // Enforce valid
  
  if (Class && Runners.size()!=size_t(Class->getNumStages())) {
    for (size_t k = Class->getNumStages(); k < Runners.size(); k++) {
      pRunner tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = 0;
        tr->tLeg = 0;
        tr->tLegEquClass = 0;
        if (tr->Class == Class)
          tr->Class = nullptr;
        oe->classIdToRunnerHash.reset();
        tr->updateChanged();
      }
    }
    Runners.resize(Class->getNumStages()); 
  }

  for (size_t i = 0; i < Runners.size(); i++) {
    if (Runners[i]) {
      if (Runners[i]->isRemoved()) {
        // Could happen for database not in sync / invalid manual modification
        Runners[i]->tInTeam = nullptr;
        Runners[i]->tLeg = 0;
        Runners[i] = nullptr;
      }

      auto tit = Runners[i]->tInTeam;
      if (tit && tit!=this) {
        tit->correctRemove(Runners[i]);
      }
      Runners[i]->tInTeam=this;
      Runners[i]->tLeg=i;
    }
  }
}

void oTeam::apply(ChangeType changeType, pRunner source) {
  if (unsigned(status) >= 100)
    status = StatusUnknown; // Enforce correct status

  int lastStartTime = 0;
  RunnerStatus lastStatus = StatusUnknown;
  bool freeStart = Class ? Class->hasFreeStart() : false;
  int extraFinishTime = -1;

  if (Class && Runners.size() != size_t(Class->getNumStages())) {
    for (size_t k = Class->getNumStages(); k < Runners.size(); k++) {
      auto tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = nullptr;
        tr->tLeg = 0;
        tr->tLegEquClass = 0;
        if (tr->Class == Class)
          tr->Class = nullptr;
        oe->classIdToRunnerHash.reset();
        if (changeType == ChangeType::Update)
          tr->updateChanged();
      }
    }
    Runners.resize(Class->getNumStages());
  }
  tNumRestarts = 0;
  vector<int> availableStartTimes;
  for (size_t i_c = 0; i_c < Runners.size(); i_c++) {
    const size_t i = i_c;
    if (Runners[i] && Runners[i]->isRemoved()) {
      // Could happen for database not in sync / invalid manual modification
      Runners[i]->tInTeam = nullptr;
      Runners[i]->tLeg = 0;
      Runners[i] = nullptr;
    }

    if (changeType == ChangeType::Quiet && i > 0 && source != nullptr && Runners[i - 1] == source)
      return;

    if (!Runners[i] && Class) {
      unsigned lr = Class->getLegRunner(i);

      if (lr < i && Runners[lr]) {
        Runners[lr]->createMultiRunner(false, false);
        int dup = Class->getLegRunnerIndex(i);
        Runners[i] = Runners[lr]->getMultiRunner(dup);
      }
    }

    if (changeType == ChangeType::Update && Runners[i] && Class) {
      unsigned lr = Class->getLegRunner(i);
      if (lr == i && Runners[i]->tParentRunner) {
        pRunner parent = Runners[i]->tParentRunner;
        for (size_t kk = 0; kk < parent->multiRunner.size(); ++kk) {
          if (Runners[i] == parent->multiRunner[kk]) {
            pRunner tr = Runners[i];
            parent->multiRunner.erase(parent->multiRunner.begin() + kk);
            tr->tParentRunner = 0;
            tr->tDuplicateLeg = 0;
            parent->markForCorrection();
            parent->updateChanged();
            tr->markForCorrection();
            tr->updateChanged();
            break;
          }
        }
      }
    }
    // Avoid duplicates, same runner running more
    // than one leg
    //(note: quadric complexity, assume total runner count is low)
    if (Runners[i]) {
      for (size_t k = 0; k < i; k++)
        if (Runners[i] == Runners[k])
          Runners[i] = nullptr;
    }

    if (Runners[i]) {
      pRunner tr = Runners[i];
      pClass actualClass = tr->getClassRef(true);
      if (actualClass == nullptr)
        actualClass = Class;
      if (tr->tInTeam && tr->tInTeam != this) {
        tr->tInTeam->correctRemove(tr);
      }
      //assert(Runners[i]->tInTeam==0 || Runners[i]->tInTeam==this);
      tr->tInTeam = this;
      tr->tLeg = i;
      if (Class) {
        int unused;
        Class->splitLegNumberParallel(i, tr->tLegEquClass, unused);
      }
      else {
        tr->tLegEquClass = i;
      }

      if (actualClass == Class)
        tr->setStartNo(StartNo, changeType);//XXX

      LegTypes legType = Class ? Class->getLegType(i) : LTIgnore;

      if (tr->Class != Class && legType != LTGroup) {
        tr->Class = Class;
        oe->classIdToRunnerHash.reset();
        tr->updateChanged();
      }

      tr->tNeedNoCard = false;
      if (Class) {
        pClass pc = Class;

        //Ignored runners need no SI-card (used by SI assign function)
        if (legType == LTIgnore) {
          tr->tNeedNoCard = true;
          if (lastStatus != StatusUnknown) {
            tr->setStatus(max(tr->tStatus, lastStatus), false, changeType);
          }
        }
        else
          lastStatus = tr->getStatus();

        StartTypes st = actualClass == pc ? pc->getStartType(i) : actualClass->getStartType(0);
        LegTypes lt = legType;

        if ((lt == LTParallel || lt == LTParallelOptional) && i == 0) {
          pc->setLegType(0, LTNormal);
          throw std::exception("Första sträckan kan inte vara parallell.");
        }
        if (lt == LTIgnore || lt == LTExtra) {
          if (st != STDrawn)
            tr->setStartTime(lastStartTime, false, changeType);
          tr->tUseStartPunch = (st == STDrawn);
        }
        else { //Calculate start time.
          switch (st) {
          case STDrawn: //Do nothing
            if (lt == LTParallel || lt == LTParallelOptional) {
              tr->setStartTime(lastStartTime, false, changeType);
              tr->tUseStartPunch = false;
            }
            else
              lastStartTime = tr->getStartTime();

            break;

          case STTime: {
            bool prs = false;
            if (tr && tr->Card && freeStart) {
              pCourse crs = tr->getCourse(false);
              int startType = crs ? crs->getStartPunchType() : oPunch::PunchStart;
              oPunch* pnc = tr->Card->getPunchByType(startType);
              if (pnc && pnc->getAdjustedTime() > 0) {
                prs = true;
                lastStartTime = pnc->getAdjustedTime();
              }
            }
            if (!prs) {
              if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
                if (actualClass == pc)
                  lastStartTime = pc->getStartData(i);
                else
                  lastStartTime = actualClass->getStartData(0); // Qualification/final classes
              }
              tr->setStartTime(lastStartTime, false, changeType);
              tr->tUseStartPunch = false;
            }
          }
                     break;

          case STChange: {
            int probeIndex = 1;
            int startData = pc->getStartData(i);

            if (startData < 0) {
              // A specified leg
              probeIndex = -startData;
            }
            else {
              // Allow for empty slots when ignore/extra
              while ((i - probeIndex) >= 0 && !Runners[i - probeIndex]) {
                LegTypes tlt = pc->getLegType(i - probeIndex);
                if (tlt == LTIgnore || tlt == LTExtra || tlt == LTGroup)
                  probeIndex++;
                else
                  break;
              }
            }

            if ((i - probeIndex) >= 0 && Runners[i - probeIndex]) {
              int z = i - probeIndex;
              LegTypes tlt = pc->getLegType(z);
              int ft = 0;
              if (availableStartTimes.empty() || startData < 0) {

                if (!availableStartTimes.empty()) {
                  // Parallel, but there is a specification. Take one from parallel anyway.
                  ft = getBestStartTime(availableStartTimes);
                }

                //We are not involved in parallel legs
                ft = (tlt != LTIgnore) ? Runners[z]->getFinishTime() : 0;

                // Take the best time for extra runners
                while (z > 0 && (tlt == LTExtra || tlt == LTIgnore)) {
                  tlt = pc->getLegType(--z);
                  if (Runners[z]) {
                    int tft = Runners[z]->getFinishTime();
                    if (tft > 0 && tlt != LTIgnore)
                      ft = ft > 0 ? min(tft, ft) : tft;
                  }
                }
              }
              else {
                ft = getBestStartTime(availableStartTimes);
              }

              if (ft <= 0)
                ft = 0;

              int restart = pc->getRestartTime(i);
              int rope = pc->getRopeTime(i);

              if (((restart > 0 && rope > 0 && (ft == 0 || ft > rope)) || (ft == 0 && restart > 0)) &&
                !preventRestart() && !tr->preventRestart()) {
                ft = restart; //Runner in restart
                tNumRestarts++;
              }

              if (ft >= 0)
                tr->setStartTime(ft, false, changeType);
              tr->tUseStartPunch = false;
              lastStartTime = ft;
            }
            else {//The else below should only be run by mistake (for an incomplete team)
              tr->setStartTime(Class->getRestartTime(i), false, changeType);
              tr->tUseStartPunch = false;
            }
          }
                       break;

          case STPursuit: {
            bool setStart = false;
            if (i > 0 && Runners[i - 1]) {
              if (lt == LTNormal || lt == LTSum || availableStartTimes.empty()) {
                int rt = getLegRunningTimeUnadjusted(i - 1, false, false);

                if (rt > 0)
                  setStart = true;
                int leaderTime = pc->getTotalLegLeaderTime(oClass::AllowRecompute::NoUseOld, i - 1, false, false);
                int timeAfter = leaderTime > 0 ? rt - leaderTime : 0;

                if (rt > 0 && timeAfter >= 0)
                  lastStartTime = pc->getStartData(i) + timeAfter;

                int restart = pc->getRestartTime(i);
                int rope = pc->getRopeTime(i);

                RunnerStatus hst = getLegStatus(i - 1, false, false);
                if (hst != StatusUnknown && hst != StatusOK) {
                  setStart = true;
                  lastStartTime = restart;
                }

                if (restart > 0 && rope > 0 && (lastStartTime > rope) &&
                  !preventRestart() && !tr->preventRestart()) {
                  lastStartTime = restart; //Runner in restart
                  tNumRestarts++;
                }
                if (!availableStartTimes.empty()) {
                  // Single -> to parallel pursuit
                  if (setStart)
                    fill(availableStartTimes.begin(), availableStartTimes.end(), lastStartTime);
                  else
                    fill(availableStartTimes.begin(), availableStartTimes.end(), 0);

                  availableStartTimes.pop_back(); // Used one
                }
              }
              else if (lt == LTParallel || lt == LTParallelOptional) {
                lastStartTime = getBestStartTime(availableStartTimes);
                setStart = true;
              }

              if (tr->getFinishTime() > 0) {
                setStart = true;
                if (lastStartTime == 0)
                  lastStartTime = pc->getRestartTime(i);
              }
              if (!setStart)
                lastStartTime = 0;
            }
            else
              lastStartTime = 0;

            tr->tUseStartPunch = false;
            tr->setStartTime(lastStartTime, false, changeType);
          }
                        break;
          }
        }

        size_t nextNonPar = i + 1;
        while (nextNonPar < Runners.size() && pc->isOptional(nextNonPar) && !Runners[nextNonPar])
          nextNonPar++;

        int nextBaseLeg = nextNonPar;
        while (nextNonPar < Runners.size() && pc->isParallel(nextNonPar))
          nextNonPar++;

        // Extra finish time is used to split extra legs to parallel legs
        if (lt == LTExtra || pc->getLegType(i + 1) == LTExtra) {
          if (lt != LTExtra)
            extraFinishTime = -1;

          if (tr->getFinishTime() > 0) {
            if (extraFinishTime <= 0)
              extraFinishTime = tr->getFinishTime();
            else
              extraFinishTime = min(extraFinishTime, tr->getFinishTime());
          }
        }
        else
          extraFinishTime = -1;

        //Add available start times for parallel
        if (nextNonPar < Runners.size()) {
          st = pc->getStartType(nextNonPar);
          int finishTime = tr->getFinishTime();
          if (lt == LTExtra)
            finishTime = extraFinishTime;

          if (st == STDrawn || st == STTime)
            availableStartTimes.clear();
          else if (finishTime > 0) {
            int nRCurrent = pc->getNumParallel(i);
            int nRNext = pc->getNumParallel(nextBaseLeg);
            if (nRCurrent > 1 || nRNext > 1) {
              if (nRCurrent < nRNext) {
                // Going from single leg to parallel legs
                for (int j = 0; j < nRNext / nRCurrent; j++)
                  availableStartTimes.push_back(finishTime);
              }
              else if (nRNext == 1)
                compressStartTimes(availableStartTimes, finishTime);
              else
                addStartTime(availableStartTimes, finishTime);
            }
            else
              availableStartTimes.clear();
          }
        }
      }
    }
  }

  if (!Runners.empty() && Runners[0]) {
    setStartTime(Runners[0]->getStartTime(), false, changeType);
  }
  else if (Class && Class->getStartType(0) != STDrawn)
    setStartTime(Class->getStartData(0), false, changeType);

  setFinishTime(getLegFinishTime(-1));
  setStatus(getLegStatus(-1, false, false), false, changeType);
}

void oTeam::applyBibs() {
  BibMode bibMode = BibUndefined;
  wstring bib = getBib();

  for (size_t i = 0; i < Runners.size(); i++) {
    pRunner tr = Runners[i];
    if (tr) {
      pClass actualClass = tr->getClassRef(true);
      if (actualClass == nullptr)
        actualClass = Class;

      if (actualClass == Class)
        tr->setStartNo(StartNo, ChangeType::Update);

      if (bibMode == BibMode::BibUndefined && Class)
        bibMode = Class->getBibMode();

      if (!bib.empty()) {
        if (bibMode == BibSame)
          tr->setBib(bib, 0, false);
        else if (bibMode == BibAdd) {
          wchar_t pattern[32], bf[32];
          int ibib = oClass::extractBibPattern(bib, pattern) + i;
          swprintf_s(bf, pattern, ibib);
          tr->setBib(bf, 0, false);
        }
        else if (bibMode == BibLeg) {
          wstring rbib = bib + L"-" + Class->getLegNumber(i);
          tr->setBib(rbib, 0, false);
        }
      }
      else {
        if (bibMode == BibSame || bibMode == BibAdd || bibMode == BibLeg)
          tr->setBib(bib, 0, false);
      }
    }
  }
}

void oTeam::evaluate(ChangeType changeType) {
  apply(ChangeType::Quiet, nullptr);
  vector<int> mp;
  for(unsigned i=0;i<Runners.size(); i++) {
    if (Runners[i])
      Runners[i]->evaluateCard(false, mp, 0, changeType);
  }
  apply(changeType, nullptr);
  if (changeType == ChangeType::Update) {
    makeQuietChangePermanent();

    for (unsigned i = 0; i < Runners.size(); i++) {
      if (Runners[i]) {
        Runners[i]->synchronize(true);
      }
    }
    synchronize(true);
  }
}

void oTeam::correctRemove(pRunner r) {
  for(unsigned i=0;i<Runners.size(); i++)
    if (r != 0 && Runners[i] == r) {
      Runners[i] = nullptr;
      r->tInTeam = nullptr;
      r->tLeg = 0;
      r->tLegEquClass = 0;
      correctionNeeded = true;
      r->correctionNeeded = true;
    }
}

void oTeam::speakerLegInfo(int leg, int specifiedLeg, int courseControlId,
                           int &missingLeg, int &totalLeg, 
                           RunnerStatus &status, int &runningTime) const {
  missingLeg = 0;
  totalLeg = 0;
  bool prelRes = false;
  bool extra = false, firstExtra = true;
  for (int i = leg; i <= specifiedLeg; i++) {
    LegTypes lt=Class->getLegType(i);
    if (lt == LTExtra)
      extra = true;

    if (lt == LTSum || lt == LTNormal || lt == LTParallel || lt == LTParallelOptional || lt == LTExtra) {
      int lrt = 0;
      RunnerStatus lst = StatusUnknown;
      if (Runners[i]) {
        Runners[i]->getSplitTime(courseControlId, lst, lrt);
        totalLeg++;
      }
      else if (lt == LTParallelOptional) {
        lst = StatusOK; // No requrement
      }
      else if (!extra) {
        totalLeg++; // will never reach...
      }

      if (extra) {
        // Extra legs, take best result
        if (firstExtra && i > 0 && !Runners[i-1]) {
          missingLeg = 0;
          totalLeg = 0;
        }
        firstExtra = false;
        if (lrt>0 && (lrt < runningTime || runningTime == 0)) {
          runningTime = lrt;
          status = lst; // Take status/time from best runner
        }
        if (Runners[i] && lst == StatusUnknown) {
          if (lrt == 0)
            missingLeg++;
        }
      }
      else {
        if (lst > StatusOK) {
          status = lst;
          break;
        }
        else if (lst == StatusUnknown && lrt == 0) {
          missingLeg++;
        }
        else {
          runningTime = max(lrt, runningTime);
          if (lst == StatusUnknown)
            prelRes = true;
        }
      }
    }
  }

  if (missingLeg == 0 && status == StatusUnknown && !prelRes)
    status = StatusOK;
}

void oTeam::fillSpeakerObject(int leg, int courseControlId, int previousControlCourseId,
                              bool totalResult, oSpeakerObject &spk) const {
  if (leg==-1)
    leg = Runners.size()-1;
  oTeam *ths = (oTeam *)this;
  ths->apply(oBase::ChangeType::Quiet, leg < Runners.size() ? Runners[leg] : nullptr);
  spk.club = getName();
  spk.missingStartTime = true;
  //Defaults (if early return)

  if (totalResult && inputStatus > StatusOK)
    spk.status = spk.finishStatus = inputStatus;
  else
    spk.status = spk.finishStatus = StatusUnknown;

  if (!Class || unsigned(leg) >= Runners.size())
    return;

  // Ignore initial optional and not used legs
  while (leg > 0 && !Runners[leg]) {
    Class->isOptional(leg);
    leg--;
  }

  if (!Runners[leg])
    return;

  // Get many names for paralell legs
  int firstLeg = leg;
  int requestedLeg = leg;
  LegTypes lt=Class->getLegType(firstLeg--);
  while(firstLeg>=0 && (lt==LTIgnore || lt==LTParallel || lt==LTParallelOptional || lt==LTExtra) )
    lt=Class->getLegType(firstLeg--);

  spk.names.clear();
  for(int k=firstLeg+1;k<=leg;k++) {
    if (Runners[k]) {
      const wstring &n = Runners[k]->getName();
      spk.names.push_back(n);
    }
  }
  // Add start number
  if (spk.names.size() == 1 && Runners[firstLeg + 1])
    spk.bib = Runners[firstLeg + 1]->getBib();
  else
    spk.bib.clear();

  if (spk.bib.empty())
    spk.bib = getBib();

  if (courseControlId == 2) {
    unsigned nextLeg = leg + 1;
    while (nextLeg < Runners.size()) {
      if (Runners[nextLeg])
        spk.outgoingnames.push_back(Runners[nextLeg]->getName());

      nextLeg++;
      if (nextLeg < Runners.size()) {
        LegTypes lt=Class->getLegType(nextLeg);
        if (!(lt==LTIgnore || lt==LTParallel || lt==LTParallelOptional || lt==LTExtra))
          break;
      }
    }
  }

  int specifiedLeg = leg;
  leg = firstLeg+1; //This does not work well with parallel or extra...
  while (!Runners[leg]) // Ensure the leg is set
    leg++;
  
  int missingLeg = 0;

  int timeOffset=0;
  RunnerStatus inheritStatus = StatusUnknown;
  if (firstLeg>=0) {
    timeOffset = getLegRunningTime(firstLeg, true, totalResult);
    inheritStatus = getLegStatus(firstLeg, true, totalResult);
  }
  else if (totalResult) {
    timeOffset = getInputTime();
    inheritStatus = getInputStatus();
  }

  speakerLegInfo(leg, specifiedLeg, courseControlId,
                  missingLeg, spk.runnersTotalLeg, spk.status, spk.runningTimeLeg.time);

  spk.runnersFinishedLeg = spk.runnersTotalLeg - missingLeg;
  if (spk.runnersTotalLeg > 1) {
    spk.parallelScore = (spk.runnersFinishedLeg * 100) / spk.runnersTotalLeg;
  }
  if (previousControlCourseId > 0 && spk.status <= 1 && Class->getStartType(0) == STTime) {
    spk.useSinceLast = true;
    RunnerStatus pStat = StatusUnknown;
    int lastTime = 0;
    int dummy;
    speakerLegInfo(leg, specifiedLeg, previousControlCourseId,
                   missingLeg, dummy, pStat, lastTime);

    if (pStat == StatusOK) {
      if (spk.runningTimeLeg.time > 0) {
        spk.runningTimeSinceLast.time = spk.runningTimeLeg.time - lastTime;
        spk.runningTimeSinceLast.preliminary = spk.runningTimeSinceLast.time;
      }
      else if (spk.runningTimeLeg.time == 0) {
        spk.runningTimeSinceLast.preliminary = oe->getComputerTime() - lastTime;
        //string db = Name + " " + itos(lastTime) + " " + itos(spk.runningTimeSinceLast.preliminary) +"\n";
        //OutputDebugString(db.c_str());
      }
    }
  }

  if (spk.runningTimeLeg.time > 10)
    spk.timeSinceChange = oe->getComputerTime() - (spk.runningTimeLeg.time + Runners[leg]->tStartTime);
  else
    spk.timeSinceChange = -1;

  spk.owner=Runners[leg];
  spk.finishStatus=getLegStatus(specifiedLeg, true, totalResult);

  spk.missingStartTime = false;
  int stMax = 0;
  for (int i = leg; i <= requestedLeg; i++) {
    if (!Runners[i])
      continue;
    if (Class->getLegType(i) == LTIgnore || Class->getLegType(i) == LTGroup)
      continue;
    
    int st=Runners[i]->getStartTime();
    if (st <= 0)
      spk.missingStartTime = true;
    else {
      if (st > stMax) {
        spk.startTimeS = Runners[i]->getStartTimeCompact();
        stMax = st;
      }
    }
  }
  
  auto mapit = Runners[leg]->priority.find(courseControlId);
  if (mapit != Runners[leg]->priority.end())
    spk.priority = mapit->second;
  else
    spk.priority = 0;

  spk.runningTimeLeg.preliminary = 0;
  for (int i = leg; i <= requestedLeg; i++) {
    if (!Runners[i])
      continue;
    int pt = Runners[i]->getPrelRunningTime();
    if (Class->getLegType(i) == LTParallel)
      spk.runningTimeLeg.preliminary = max(spk.runningTimeLeg.preliminary, pt);
    else if (Class->getLegType(i) == LTExtra) {
      if (spk.runningTimeLeg.preliminary == 0)
        spk.runningTimeLeg.preliminary = pt;
      else if (pt > 0)
        spk.runningTimeLeg.preliminary = min(spk.runningTimeLeg.preliminary, pt);
    }
    else
      spk.runningTimeLeg.preliminary = pt;
  }

  if (inheritStatus>StatusOK)
    spk.status=inheritStatus;

  if (spk.status==StatusOK) {
    if (courseControlId == 2)
      spk.runningTime.time = getLegRunningTime(requestedLeg, true, totalResult); // Get official time

    if (spk.runningTime.time == 0)
      spk.runningTime.time = spk.runningTimeLeg.time + timeOffset; //Preliminary time

    spk.runningTime.preliminary = spk.runningTime.time;
    spk.runningTimeLeg.preliminary = spk.runningTimeLeg.time;
  }
  else if (spk.status==StatusUnknown && spk.finishStatus==StatusUnknown) {
    spk.runningTime.time = 0;// spk.runningTimeLeg.preliminary + timeOffset;
    spk.runningTime.preliminary = spk.runningTimeLeg.preliminary + timeOffset;
    if (spk.runningTimeLeg.time > 0) {
      spk.runningTime.time = spk.runningTimeLeg.time + timeOffset;
    }
    else {
      spk.runningTime.time = 0;
//      spk.runningTimeLeg.time = 0;// spk.runningTimeLeg.preliminary;
    }
  }
  else if (spk.status==StatusUnknown)
    spk.status=StatusMP;

  if (totalResult && inputStatus != StatusOK)
    spk.status = spk.finishStatus;
}

int oTeam::getTimeAfter(int leg, bool allowUpdate) const {
  if (leg == -1)
    leg = Runners.size() - 1;

  if (!Class || Class->tLeaderTime.size()<=unsigned(leg))
    return -1;

  int t=getLegRunningTime(leg, true, false);

  if (t<=0)
    return -1;

  return t-Class->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, leg, true, false);
}

int oTeam::getLegStartTime(int leg) const
{
  if (leg==0)
    return tStartTime;
  else if (unsigned(leg)<Runners.size() && Runners[leg])
    return Runners[leg]->getStartTime();
  else return 0;
}

wstring oTeam::getLegStartTimeS(int leg) const
{
  int s=getLegStartTime(leg);
  if (s>0)
    return oe->getAbsTime(s);
  else 
    return makeDash(L"-");
}

wstring oTeam::getLegStartTimeCompact(int leg) const
{
  int s=getLegStartTime(leg);
  if (s>0)
    if (oe->useStartSeconds())
      return oe->getAbsTime(s);
    else
      return oe->getAbsTimeHM(s);

  else return makeDash(L"-");
}

void oTeam::setBib(const wstring &bib, int bibnumerical, bool updateStartNo) {
  if (updateStartNo)
    updateStartNo = !Class || !Class->lockedForking();

  if (getDI().setString("Bib", bib)) {
    if (oe)
      oe->bibStartNoToRunnerTeam.clear();
  }

  if (updateStartNo) 
    setStartNo(bibnumerical, ChangeType::Update);
}

oDataContainer &oTeam::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<pvectorstr>(&dynamicData);
  return *oe->oTeamData;
}

pRunner oTeam::getRunner(unsigned leg) const {
  if (leg==-1)
    leg=Runners.size()-1;

  return leg<Runners.size() ? Runners[leg] : nullptr;
}

int oTeam::getRogainingPoints(bool computed, bool multidayTotal) const {
  if (computed && tComputedPoints > 0)
    return tComputedPoints;

  int pt = 0;
  bool simpleSum = false;
  if (simpleSum) {
    for (size_t k = 0; k < Runners.size(); k++) {
      if (Runners[k])
        pt += Runners[k]->getRogainingPoints(computed, false);
    }
  }
  else {
    std::set<int> rogainingControls;
    for (size_t k = 0; k < Runners.size(); k++) {
      if (Runners[k]) {
        pCard c = Runners[k]->getCard();
        pCourse crs = Runners[k]->getCourse(true);
        if (c && crs) {
          for (oPunchList::const_iterator it = c->punches.begin(); it != c->punches.end();++it) {
            if (rogainingControls.count(it->tMatchControlId) == 0) {
              rogainingControls.insert(it->tMatchControlId);
              pt += it->tRogainingPoints;
            }
          }
        }
      }
    }
  }
  pt = max(pt - getRogainingReduction(true), 0);
  // Manual point adjustment
  pt = max(0, pt + getPointAdjustment());

  if (multidayTotal)
    return pt + inputPoints;
  return pt;
}

int oTeam::getRogainingOvertime(bool computed) const {
  pCourse sampleCourse = 0;
  int overTime = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      if (sampleCourse == 0 && (Runners[k]->tRogainingPoints > 0 || Runners[k]->tReduction > 0))
        sampleCourse = Runners[k]->getCourse(false);
      overTime += Runners[k]->tRogainingOvertime;
    }
  }
  if (sampleCourse && computed) {
    if (tComputedTime > 0)
      overTime = max(0, tComputedTime - sampleCourse->getMaximumRogainingTime());
  }
  return overTime;
}

int oTeam::getRogainingPointsGross(bool computed) const {
  int gross = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      gross += Runners[k]->tRogainingPointsGross;
    }
  }
  return gross;
}
 

int oTeam::getRogainingReduction(bool computed) const {
  pCourse sampleCourse = 0;
  int overTime = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      if (sampleCourse == 0 &&(Runners[k]->tRogainingPoints > 0 || Runners[k]->tReduction > 0)) 
        sampleCourse = Runners[k]->getCourse(false);
      overTime += Runners[k]->tRogainingOvertime; 
    }
  }
  if (sampleCourse && computed) {
    if (tComputedTime > 0)
      overTime = max(0, tComputedTime - sampleCourse->getMaximumRogainingTime());

      return sampleCourse->calculateReduction(overTime);
  }
  else
    return 0;
}
 
void oTeam::remove()
{
  if (oe)
    oe->removeTeam(Id);
}

bool oTeam::canRemove() const
{
  return true;
}

wstring oTeam::getDisplayName() const {
  if (!Class)
    return sName;

  ClassType ct = Class->getClassType();
  if (ct == oClassIndividRelay || ct == oClassPatrol) {
    if (Club) {
      wstring cname = getDisplayClub();
      if (!cname.empty())
        return cname;
    }
  }
  return sName;
}

wstring oTeam::getDisplayClub() const {
  vector<pClub> clubs;
  if (Club)
    clubs.push_back(Club);
  for (size_t k = 0; k<Runners.size(); k++) {
    if (Runners[k] && Runners[k]->Club) {
      if (count(clubs.begin(), clubs.end(), Runners[k]->Club) == 0)
        clubs.push_back(Runners[k]->Club);
    }
  }
  if (clubs.size() == 1)
    return clubs[0]->getDisplayName();

  wstring res;
  for (size_t k = 0; k<clubs.size(); k++) {
    if (k == 0)
      res = clubs[k]->getDisplayName();
    else
      res += L" / " + clubs[k]->getDisplayName();
  }
  return res;
}

void oTeam::setInputData(const oTeam &t) {
  inputTime = t.getTotalRunningTime();
  inputStatus = t.getTotalStatus();
  inputPoints = t.getRogainingPoints(true, true);
  int tp = t.getTotalPlace(true);
  inputPlace = tp;

  oDataInterface dest = getDI();
  oDataConstInterface src = t.getDCI();

  dest.setInt("TransferFlags", src.getInt("TransferFlags"));
  
  dest.setString("Nationality", src.getString("Nationality"));
  dest.setString("Country", src.getString("Country"));
  dest.setInt("Fee", src.getInt("Fee"));
  dest.setInt("Paid", src.getInt("Paid"));
  dest.setInt("Taxable", src.getInt("Taxable"));

  int sn = t.getEvent()->getStageNumber();
  addToInputResult(sn - 1, &t);
}

void oEvent::getTeams(int classId, vector<pTeam> &t, bool sort) {
  if (sort) {
    synchronizeList(oListId::oLTeamId);
  }
  t.clear();
  if (Classes.size() > 0)
    t.reserve((Teams.size()*min<size_t>(Classes.size(), 4)) / Classes.size());

  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (classId == 0 || it->getClassId(false) == classId)
      t.push_back(&*it);
  }
}

void oEvent::getTeams(const set<int> &classId, vector<pTeam> &t, bool synch) {
  if (synch)
    synchronizeList(oListId::oLTeamId);

  getTeams(classId, t);
}

void oEvent::getTeams(const set<int> &classId, vector<pTeam> &t) const {
  t.clear();
  t.reserve(Teams.size());

  for (auto it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (classId.empty() || classId.count(it->getClassId(false)))
      t.push_back(const_cast<pTeam>(&*it));
  }
}

wstring oTeam::getEntryDate(bool dummy) const {
  oDataConstInterface dci = getDCI();
  int date = dci.getInt("EntryDate");
  if (date == 0 && !isVacant()) {
    auto di = (const_cast<oTeam *>(this)->getDI());
    di.setDate("EntryDate", getLocalDate());
    di.setInt("EntryTime", getLocalAbsTime());
  }
  return dci.getDate("EntryDate");
}

unsigned static nRunnerMaxStored = -1;

const shared_ptr<Table> &oTeam::getTable(oEvent *oe) {
  vector<pClass> cls;
  oe->getClasses(cls, true);
  unsigned nRunnerMax = 0;
  for (size_t k = 0; k < cls.size(); k++) {
    nRunnerMax = max(nRunnerMax, cls[k]->getNumStages());
  }

  bool forceUpdate = nRunnerMax != nRunnerMaxStored;

  if (forceUpdate || !oe->hasTable("team")) {

    auto table = make_shared<Table>(oe, 20, L"Lag(flera)", "teams");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);
    table->addColumn("Klass", 120, false);
    table->addColumn("Klubb", 120, false);

    table->addColumn("Start", 70, false, true);
    table->addColumn("Mål", 70, false, true);
    table->addColumn("Status", 70, false);
    table->addColumn("Tid", 70, false, true);
    table->addColumn("Poäng", 70, true, true);

    table->addColumn("Plac.", 70, true, true);
    table->addColumn("Start nr.", 70, true, false);

    for (unsigned k = 0; k < nRunnerMax; k++) {
      table->addColumn("Sträcka X#" + itos(k + 1), 200, false, false);
      table->addColumn("Bricka X#" + itos(k + 1), 70, true, true);
    }
    nRunnerMaxStored = nRunnerMax;

    oe->oTeamData->buildTableCol(table.get());
    table->addColumn("Tid in", 70, false, true);
    table->addColumn("Status in", 70, false, true);
    table->addColumn("Poäng in", 70, true);
    table->addColumn("Placering in", 70, true);

    oe->setTable("team", table);
  }

  return oe->getTable("team");
}

void oEvent::generateTeamTableData(Table &table, oTeam *addTeam)
{
  vector<pClass> cls;
  oe->getClasses(cls, true);
  unsigned nRunnerMax = 0;

  for (size_t k = 0; k < cls.size(); k++) {
    nRunnerMax = max(nRunnerMax, cls[k]->getNumStages());
  }

  oe->calculateTeamResults(set<int>({}), ResultType::ClassResult);

  if (nRunnerMax != nRunnerMaxStored)
    throw meosException("Internal table error: Restart MeOS");

  if (addTeam) {
    addTeam->addTableRow(table);
    return;
  }

  synchronizeList(oListId::oLTeamId);
  oTeamList::iterator it;
  table.reserve(Teams.size());
  for (it=Teams.begin(); it != Teams.end(); ++it){
    if (!it->skip()){
      it->addTableRow(table);
    }
  }
}

void oTeam::addTableRow(Table &table) const {
  oRunner &it = *pRunner(this);
  table.addRow(getId(), &it);

  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, it, TID_NAME, getName(), true);
  table.set(row++, it, TID_CLASSNAME, getClass(true), true, cellSelection);
  table.set(row++, it, TID_CLUB, getClub(), true, cellCombo);

  table.set(row++, it, TID_START, getStartTimeS(), true);
  table.set(row++, it, TID_FINISH, getFinishTimeS(false, SubSecond::Auto), true);
  table.set(row++, it, TID_STATUS, getStatusS(false, true), true, cellSelection);
  table.set(row++, it, TID_RUNNINGTIME, getRunningTimeS(true, SubSecond::Auto), false);
  int rp = getRogainingPoints(true, false);
  table.set(row++, it, TID_POINTS, rp ? itow(rp) : L"", false);

  table.set(row++, it, TID_PLACE, getPlaceS(), false);
  table.set(row++, it, TID_STARTNO, itow(getStartNo()), true);

  for (unsigned k = 0; k < nRunnerMaxStored; k++) {
    pRunner r = getRunner(k);
    if (r) {
      table.set(row++, it, 100+2*k, r->getUIName(), r->getRaceNo() == 0);
      table.set(row++, it, 101+2*k, itow(r->getCardNo()), true);
    }
    else {
      table.set(row++, it, 100+2*k, L"", Class && Class->getLegRunner(k) == k);
      table.set(row++, it, 101+2*k, L"", false);
    }
  }

  row = oe->oTeamData->fillTableCol(it, table, true);

  table.set(row++, it, TID_INPUTTIME, getInputTimeS(), true);
  table.set(row++, it, TID_INPUTSTATUS, getInputStatusS(), true, cellSelection);
  table.set(row++, it, TID_INPUTPOINTS, itow(inputPoints), true);
  table.set(row++, it, TID_INPUTPLACE, itow(inputPlace), true);
}

pair<int, bool> oTeam::inputData(int id, const wstring &input,
                                 int inputId, wstring &output, bool noUpdate) {
  int s,t;
  synchronize(false);

  if (id>1000) {
    const wstring &preBib = getDCI().getString("Bib");
    auto res = oe->oTeamData->inputData(this, id, input,
                                    inputId, output, noUpdate);
    
    const wstring &postBib = getDCI().getString("Bib");

    if (preBib != postBib) {
      wchar_t pat[32];
      int no = oClass::extractBibPattern(postBib, pat);
      if (no > 0)
        setStartNo(no, ChangeType::Update);

      applyBibs();
    }

    evaluate(oBase::ChangeType::Update);
    return res;
  }
  else if (id >= 100) {

    bool isName = (id&1) == 0;
    size_t ix = (id-100)/2;
    if (ix < Runners.size()) {
      if (Runners[ix]) {
        if (isName) {
          if (input.empty()) {
            removeRunner(oe->gdibase, false, ix);
          }
          else {
            Runners[ix]->setName(input, true);
            Runners[ix]->synchronize(true);
            output = Runners[ix]->getName();
          }
        }
        else {
          Runners[ix]->setCardNo(_wtoi(input.c_str()), true);
          Runners[ix]->synchronize(true);
          output = itow(Runners[ix]->getCardNo());
        }
      }
      else {
        if (isName && !input.empty() && Class) {
          pRunner r = oe->addRunner(input, getClubId(), getClassId(false), 0, L"", false);
          setRunner(ix, r, true);
          output = r->getName();
        }
      }
    }
  }

  switch(id) {

    case TID_NAME:
      setName(input, true);
      synchronize(true);
      output = getName();
      break;

    case TID_START:
      setStartTimeS(input);
      if (getRunner(0))
        getRunner(0)->setStartTimeS(input);
      t = getStartTime();
      apply(ChangeType::Update, nullptr);
      s = getStartTime();
      if (s != t)
        throw std::exception("Starttiden är definerad genom klassen eller löparens startstämpling.");
      synchronize(true);
      output = getStartTimeS();
    break;

    case TID_CLUB:
      {
        pClub pc = 0;
        if (inputId > 0)
          pc = oe->getClub(inputId);
        else
          pc = oe->getClubCreate(0, input);

        setClub(pc ? pc->getName() : L"");
        synchronize(true);
        output = getClub();
      }
      break;

    case TID_CLASSNAME:
      if (inputId == -1) {
        pClass c = oe->getClass(input);
        if (c)
          inputId = c->getId();
      }
      setClassId(inputId, true);
      adjustMultiRunners();

      synchronize(true);
      output = getClass(true);
      break;

    case TID_STATUS: {
      RunnerStatus sIn = getStatus();
      if (inputId >= 0) {
        sIn = RunnerStatus(inputId);
        if (!isResultStatus(sIn))
          setTeamMemberStatus(sIn);
        else
          setStatus(sIn, true, ChangeType::Update);

        apply(ChangeType::Update, nullptr);
      }
      RunnerStatus sOut = getStatus();
      if (sOut != sIn)
        throw meosException("Status matchar inte deltagarnas status.");
      output = getStatusS(false, true);
    }
    break;

    case TID_STARTNO:
      setStartNo(_wtoi(input.c_str()), ChangeType::Update);
      evaluate(oBase::ChangeType::Update);
      output = itow(getStartNo());
      break;

    case TID_INPUTSTATUS:
      if (inputId >= 0)
        setInputStatus(RunnerStatus(inputId));
      synchronize(true);
      output = getInputStatusS();
      break;

    case TID_INPUTTIME:
      setInputTime(input);
      synchronize(true);
      output = getInputTimeS();
      break;

    case TID_INPUTPOINTS:
      setInputPoints(_wtoi(input.c_str()));
      synchronize(true);
      output = itow(getInputPoints());
      break;

    case TID_INPUTPLACE:
      setInputPlace(_wtoi(input.c_str()));
      synchronize(true);
      output = itow(getInputPlace());
      break;

  }
  return make_pair(0, false);
}

void oTeam::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oRunnerData->fillInput(this, id, 0, out, selected);
    return;
  }
  else if (id==TID_CLASSNAME) {
    oe->fillClasses(out, oEvent::extraNone, oEvent::filterOnlyMulti);
    out.push_back(make_pair(lang.tl(L"Ingen klass"), 0));
    selected = getClassId(true);
  }
  else if (id==TID_CLUB) {
    oe->fillClubs(out);
    out.push_back(make_pair(lang.tl(L"Klubblös"), 0));
    selected = getClubId();
  }
  else if (id==TID_STATUS || id==TID_INPUTSTATUS) {
    oe->fillStatus(out);
    selected = getStatus();
  }
}

void oTeam::removeRunner(gdioutput &gdi, bool askRemoveRunner, int i) {
  pRunner p_old = getRunner(i);
  setRunner(i, 0, true);

  //Remove multi runners
  if (Class) {
    for (unsigned k = i+1;k < Class->getNumStages(); k++) {
      if (Class->getLegRunner(k)==i)
        setRunner(k, 0, true);
    }
  }

  //No need to delete multi runners. Disappears when parent is gone.
  if (p_old && !oe->isRunnerUsed(p_old->getId())){
    if (!askRemoveRunner || gdi.ask(L"Ska X raderas från tävlingen?#" + p_old->getName())){
      vector<int> oldR;
      oldR.push_back(p_old->getId());
      oe->removeRunner(oldR);
    }
    else {
      p_old->getDI().setInt("RaceId", 0); // Clear race id.
      p_old->setClassId(0, false); // Clear class
      p_old->synchronize(true);
    }
  }
}

int oTeam::getTeamFee() const {
  int f = getDCI().getInt("Fee");
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k])
      f += Runners[k]->getDCI().getInt("Fee");
  }
  return f;
}

void oTeam::markClassChanged(int controlId) {
  if (Class)
    Class->markSQLChanged(-1, controlId);
}

void oTeam::resetResultCalcCache() const {
  resultCalculationCache.resize(RCCLast);
  for (int k = 0; k < RCCLast; k++)
    resultCalculationCache[k].resize(Runners.size());
}

vector< vector<int> > &oTeam::getResultCache(ResultCalcCacheSymbol symb) const {
  return resultCalculationCache[symb];
}

void oTeam::setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int> &data) const {
  if (!resultCalculationCache.empty() && size_t(leg) < resultCalculationCache[symb].size())
    resultCalculationCache[symb][leg].swap(data);
}

int oTeam::getNumShortening() const {
  return getNumShortening(-1);
}

int oTeam::getNumShortening(int leg) const {
  int ns = 0;
  if (Class) {
    for (size_t k = 0; k < Runners.size() && k <= size_t(leg); k++) {
      if (Runners[k] && !Class->isOptional(k))
        ns += Runners[k]->getNumShortening();
    }
  }
  else {
   for (size_t k = 0; k < Runners.size() && k <= size_t(leg); k++) {
      if (Runners[k])
        ns += Runners[k]->getNumShortening();
    }
  }
  return ns;
}

bool oTeam::checkValdParSetup() {
  if (!Class)
    return false;
  bool cor = false;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (!Class->isOptional(k) && !Class->isParallel(k) && !Runners[k]) {
      int m = 1;
      while((m+k) < Runners.size() && (Class->isOptional(k+m) || Class->isParallel(k+m))) {
        if (Runners[m+k]) {
          // Move to where a runner is needed
          Runners[k] = Runners[k+m];
          Runners[k]->tLeg = k;
          Runners[k+m] = nullptr;
          updateChanged();
          cor = true;
          k+=m;
          break;
        }
        else 
          m++;
      }
    }
  }
  return cor;
}


int oTeam::getRanking() const {
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      return Runners[k]->getRanking();
    }
  }
  return MaxRankingConstant;
}

int oTeam::getRogainingPatrolPoints(bool multidayTotal) const {
  int madj = multidayTotal ? getInputPoints() : 0;

  if (tTeamPatrolRogainingAndVersion.first == oe->dataRevision)
    return tTeamPatrolRogainingAndVersion.second.points + madj;

  tTeamPatrolRogainingAndVersion.first = oe->dataRevision;
  tTeamPatrolRogainingAndVersion.second.reset();

  int reduction = 0;
  int overtime = 0;
  map<int, vector<pair<int, int>>> control2PunchTimeRunner;
  std::set<int> runnerToCheck;
  vector<pPunch> punches;
  for (pRunner r : Runners) {
    if (r) {
      pCourse pc = r->getCourse(false);
      if (r->getCard() && pc) {
        reduction = max(reduction, r->getRogainingReduction(false));
        overtime = max(overtime, r->getRogainingOvertime(false));
        int rid = r->getId();
        r->getCard()->getPunches(punches);
        for (auto p : punches) {
          if (p->anyRogainingMatchControlId > 0) {
            pControl ctrl = oe->getControl(p->anyRogainingMatchControlId);
            if (ctrl) {
              auto &cl = control2PunchTimeRunner[ctrl->getId()];
              cl.push_back(make_pair(p->getTimeInt(), rid));
            }
          }
        }
      }
      else if (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL)
        continue; // Accept missing punches

      runnerToCheck.insert(r->getId());
    }
  }
  int timeLimit = oe->getDCI().getInt("DiffTime");
  if (timeLimit == 0)
    timeLimit = 10000000;

  vector<pControl> acceptedControls;
  for (auto &ctrl : control2PunchTimeRunner) {
    int ctrlId = ctrl.first;
    auto &punchList = ctrl.second;
    sort(punchList.begin(), punchList.end()); // Sort times in order. Zero time means unknown time
    bool ok = false;
    for (size_t k = 0; !ok && k < punchList.size(); k++) {
      std::set<int> checked;
      for (size_t z = 0; z < punchList.size() && punchList[z].first <= 0; z++) {
        checked.insert(punchList[z].second); // Missing time. Accept any
        k = max(k, z);
      }

      if (k < punchList.size()) {
        int startTime = punchList[k].first;
        for (size_t j = k; j < punchList.size() && (punchList[j].first - startTime) < timeLimit; j++) {
          checked.insert(punchList[j].second); // Accept competitor if in time interval
        }
      }

      ok = checked.size() >= runnerToCheck.size();
    }

    if (ok) {
      acceptedControls.push_back(oe->getControl(ctrlId));
    }
  }
  int points = 0;
  for (pControl ctrl : acceptedControls) {
    points += ctrl->getRogainingPoints();
  }
  points = max(0, points + getPointAdjustment() - reduction);
  tTeamPatrolRogainingAndVersion.second.points = points;
  tTeamPatrolRogainingAndVersion.second.reduction = reduction;
  tTeamPatrolRogainingAndVersion.second.overtime = overtime;

  return tTeamPatrolRogainingAndVersion.second.points + madj;
}

int oTeam::getRogainingPatrolReduction() const {
  getRogainingPatrolPoints(false);
  return tTeamPatrolRogainingAndVersion.second.reduction;
}

int oTeam::getRogainingPatrolOvertime() const {
  getRogainingPatrolPoints(false);
  return tTeamPatrolRogainingAndVersion.second.overtime;
}

void oTeam::setClub(const wstring &clubName) {
  oAbstractRunner::setClub(clubName);
  propagateClub();
}

pClub oTeam::setClubId(int clubId) {
  oAbstractRunner::setClubId(clubId);
  propagateClub();
  return Club;
}

void oTeam::propagateClub() {
  
  if (Class && Class->getNumDistinctRunners() == 1) {
    for (pRunner r : Runners) {
      if (r && r->Club != Club) {
        r->Club = Club;
        r->updateChanged();
      }
    }
  }
}

const oTeam::ComputedLegResult &oTeam::getComputedResult(int leg) const {
  if (size_t(leg) < tComputedResults.size())
    return tComputedResults[leg];

  if (tComputedResults.empty())
    tComputedResults.resize(1);
  return tComputedResults[0];
}

void oTeam::setComputedResult(int leg, ComputedLegResult &comp) const {
  if (tComputedResults.size() < Runners.size())
    tComputedResults.resize(Runners.size());

  if (size_t(leg) >= tComputedResults.size())
    return;

  tComputedResults[leg] = comp;
}

oTeam::TeamPlace &oTeam::getTeamPlace(int leg) const {
  if (tPlace.size() != Runners.size())
    tPlace.resize(Runners.size());

  if (size_t(leg) < tPlace.size())
    return tPlace[leg];

  if (tComputedResults.empty() || tPlace.empty())
    tPlace.resize(1);
    
  return tPlace[0];
}

bool oTeam::isResultUpdated(bool totalResult) const {
  auto &p = getTeamPlace(Runners.size() - 1);
  if (!totalResult)
    return !p.p.isOld(*oe);
  else
    return !p.totalP.isOld(*oe);
}

const pair<wstring, int> oTeam::getRaceInfo() {
  pair<wstring, int> res;
  RunnerStatus baseStatus = getStatus();
  int rtActual = getRunningTime(false);

  if (isResultStatus(baseStatus) || (isPossibleResultStatus(baseStatus) && rtActual>0)) {
    int p = getPlace(true);
    int rtComp = getRunningTime(true);
    int pointsActual = getRogainingPoints(false, false);
    int pointsComp = getRogainingPoints(true, false);
    RunnerStatus compStatus = getStatusComputed(true);
    bool ok = compStatus == StatusOK || compStatus == StatusOutOfCompetition
      || compStatus == StatusNoTiming;
    res.second = ok ? 1 : -1;
    if (compStatus == baseStatus && rtComp == rtActual && pointsComp == pointsActual) {
      if (ok && p > 0)
        res.first = lang.tl("Placering: ") + itow(p) + L".";
    }
    else {
      if (ok) {
        res.first += lang.tl("Resultat: ");
        if (compStatus != baseStatus)
          res.first = oe->formatStatus(compStatus, true) + L", ";
        if (pointsActual != pointsComp)
          res.first += itow(pointsComp) + L", ";

        res.first += formatTime(rtComp);

        if (p > 0)
          res.first += L" (" + itow(p) + L")";
      }
      else if (!ok && compStatus != baseStatus) {
        res.first = lang.tl("Resultat: ") + oe->formatStatus(compStatus, true);
      }
      /*
      if (ok && getRogainingReduction(true) > 0) {
        tProblemDescription = L"Tidsavdrag: X poäng.#" + itow(getRogainingReduction(true));
      }

      if (!getProblemDescription().empty()) {
        if (!res.first.empty()) {
          if (res.first.back() != ')')
            res.first += L", ";
          else
            res.first += L" ";
        }
        res.first += lang.tl(getProblemDescription());
      }*/
    }
  }
  else {
    vector<oFreePunch*> pl;
    oe->synchronizeList(oListId::oLPunchId);
    oe->getPunchesForRunner(Id, true, pl);
    if (!pl.empty()) {
      res.first = lang.tl(L"Senast sedd: X vid Y.#" +
                          oe->getAbsTime(pl.back()->getTimeInt()) +
                          L"#" + pl.back()->getType());
    }
  }

  return res;
}

void oTeam::changedObject() {
  markClassChanged(-1);
  sqlChanged = true;
  oe->sqlTeams.changed = true;
}

bool oTeam::matchAbstractRunner(const oAbstractRunner* target) const {
  if (target == nullptr)
    return false;

  if (target == this)
    return true;

  const oRunner* r = dynamic_cast<const oRunner*>(target);
  if (r != nullptr) 
    return r->getTeam() == this;
  
  return false;
}

pRunner oTeam::getRunnerBestTimePar(int linearLegInput) const {
  if (!Class)
    return getRunner(linearLegInput);

  if (linearLegInput < 0)
    linearLegInput = Runners.size() - 1;
  int minL, maxL;
  Class->getParallelOptionalRange(linearLegInput, minL, maxL);

  int ft = numeric_limits<int>::max();
  pRunner res = nullptr;
  for (int i = minL; i <= maxL; i++) {
    if (size_t(i) < Runners.size())
      continue;
    if (res == nullptr)
      res = Runners[i]; // Ensure any

    if (i < Runners.size() && Runners[i] && Runners[i]->prelStatusOK(false, false, false)) {
      int f = Runners[i]->getFinishTime();
      if (f > 0 && f < ft) {
        ft = f;
        res = Runners[i];
      }
    }
  }
  return res;
}
