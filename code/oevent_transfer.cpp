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
#include <random>

#include "oEvent.h"
#include "oDataContainer.h"
#include "metalist.h"
#include "generalresult.h"

#include "meosException.h"
#include "meos.h"
#include "meos_util.h"
#include "intkeymapimpl.hpp"

#include "MeOSFeatures.h"



void oEvent::merge(oEvent &src, oEvent *base, bool allowRemove, int &numAdd, int &numRemove, int &numUpdate) {
  numAdd = 0;
  numRemove = 0;
  numUpdate = 0;

  wstring mergeTag = src.getMergeTag();
  wstring mergeTime = getMergeInfo(mergeTag);
  wstring baseTime, reverseMerge;
  string addMinTime = src.getLastModified();
  if (src.currentNameId == currentNameId) {
    // Get base version time
    if (baseTime.empty()) {
      baseTime = src.getDCI().getString("ImportStamp");
      if (baseTime.empty())
        baseTime = getDCI().getString("ImportStamp");
    }
   
    reverseMerge = src.getMergeInfo(getMergeTag());
    if (reverseMerge.empty())
      reverseMerge = baseTime;
  }

  if (mergeTime.empty())
    mergeTime = baseTime;

  string previousMergeTime(mergeTime.begin(), mergeTime.end());
  string thisMergeTime;
  string bt(reverseMerge.begin(), reverseMerge.end());
  set<int> rControl, rRunner, rTeam, rCourse, rClub, rClass;

  auto updateNewItem = [&addMinTime, &numAdd](oBase *pNew, const oBase &src) {
    if (pNew) {
      numAdd++;
      pNew->merge(src, nullptr);
      pNew->synchronize();
      if (pNew->Modified.getStamp() < addMinTime)
        pNew->Modified.setStamp(addMinTime);
    }
  };

  auto mergeItem = [&numUpdate](oBase *pExisting, const oBase &src, const oBase *baseItem) {
    numUpdate++;
    string oldStamp = pExisting->Modified.getStamp();
    pExisting->merge(src, baseItem);
    if (pExisting->Modified.getStamp() < oldStamp)
      pExisting->Modified.setStamp(oldStamp);
  };

  auto getBaseMap = [](auto &list) {
    map<int, oBase *> ret;
    for (auto &c : list) 
      ret.emplace(c.getId(), &c);    
    return ret;
  };
  map<int, oBase*> baseMap;

  auto computeRemove = [&bt, &baseMap](const auto &list, const set<int> &existing, set<int> &remove) {
    for (auto &c : list) {
      if (!baseMap.empty() && !baseMap.count(c.Id))
        continue; // Did non exist in base -> not removed
      if (!c.isRemoved() && !existing.count(c.Id) && c.getStamp() < bt)
        remove.insert(c.Id);
    }
  };
  
  // Swap id
  auto changeBaseId = [&baseMap](int oldId, int newId) {
    oBase *rOld = nullptr, *rNew = nullptr;
    auto ob = baseMap.find(newId);
    if (ob != baseMap.end() && ob->second) {
      rOld = ob->second;
      rOld->changeId(oldId);
    }
    ob = baseMap.find(oldId);
    if (ob != baseMap.end() && ob->second) {
      rNew = ob->second;
      rNew->changeId(newId);
    }
    baseMap[newId] = rNew;
    baseMap[oldId] = rOld;
  };

  auto getBaseObject = [&baseMap](const oBase &src) {
    auto res = baseMap.find(src.getId());
    if (res != baseMap.end())
      return res->second;
    else
      return (oBase*)nullptr;
  };

  {
    map<int, pControl> ctrl;
    for (oControl &c : Controls) {
      if (!c.isRemoved())
        ctrl[c.Id] = &c;
    }

    if (base)
      baseMap = getBaseMap(base->Controls);

    set<int> srcControl;
    for (const oControl &c : src.Controls) {
      const oBase *baseObj = getBaseObject(c);
      const string &stmp = c.getStamp();
      if (!c.isRemoved()) {
        if (stmp > previousMergeTime) {
          if (stmp > thisMergeTime)
            thisMergeTime = stmp;
          auto mc = ctrl.find(c.Id);
          if (mc != ctrl.end()) {
            mergeItem(mc->second, c, baseObj);
          }
          else {
            pControl pNew = addControl(c);
            updateNewItem(pNew, c);
          }
        }
        srcControl.insert(c.Id);
      }
    }

    computeRemove(Controls, srcControl, rControl);
  }

  {
    map<int, pCourse> crs;
    for (oCourse &c : Courses) {
      if (!c.isRemoved())
        crs[c.Id] = &c;
    }

    if (base)
      baseMap = getBaseMap(base->Courses);

    set<int> srcCourse;
    for (const oCourse &c : src.Courses) {
      const oBase *baseObj = getBaseObject(c);
      const string &stmp = c.getStamp();
      if (!c.isRemoved()) {
        bool okMerge = stmp > previousMergeTime;
        if (stmp > thisMergeTime)
          thisMergeTime = stmp;
        auto mc = crs.find(c.Id);
        if (mc != crs.end()) {
          if (okMerge)
            mergeItem(mc->second, c, baseObj);
        }
        else if (okMerge) {
          pCourse pNew = addCourse(c);          
          updateNewItem(pNew, c);
        }
        srcCourse.insert(c.Id);
      }
    }

    computeRemove(Courses, srcCourse, rCourse);
  }

  {
    map<int, pClass> cls;
    map<wstring, pClass> clsN;

    for (oClass &c : Classes) {
      if (!c.isRemoved()) {
        cls[c.Id] = &c;
        clsN[c.Name] = &c;
      }
    }
    
    if (base)
      baseMap = getBaseMap(base->Classes);

    set<int> srcClass;
    for (oClass &c : src.Classes) {
      oBase *baseObj = getBaseObject(c);
      const string &stmp = c.getStamp();
      bool merged = false;
      if (!c.isRemoved()) {
        bool okMerge = stmp > previousMergeTime;

        if (stmp > thisMergeTime)
          thisMergeTime = stmp;
        auto mc = cls.find(c.Id);

        if (mc != cls.end()) {
          if (compareClassName(mc->second->Name, c.Name)) {
            if (okMerge)
              mergeItem(mc->second, c, baseObj);
            merged = true;
          }
        }

        auto updateIdCls = [&](int id) {
          changeBaseId(c.Id, id);
          pClass other = src.getClass(id);          
          if (other) 
            other->changeId(c.Id);
          c.changeId(id);          
        };

        if (!merged) {
          auto mcN = clsN.find(c.Name);
          if (mcN != clsN.end()) {
            if (okMerge)
              mergeItem(mcN->second, c, baseObj);
            merged = true;
            updateIdCls(mcN->second->Id);
          }
        }

        if (!merged && okMerge) {
          if (cls.count(c.Id)) {
            int newId = max(getFreeClassId(), src.getFreeClassId());
            c.changeId(newId);
          }
          pClass pNew = addClass(c);
          updateNewItem(pNew, c);
        }

        srcClass.insert(c.Id);
      }
    }

    computeRemove(Classes, srcClass, rClass);
  }

  { // Removing card not supported --> maybe too riskful...
    map<int, pCard> crd;
    map<pair<int, int>, pCard> crdByHash;
    for (oCard &c : Cards) {
      if (!c.isRemoved()) {
        crd[c.Id] = &c;
        crdByHash[c.getCardHash()] = &c;
      }
    }
    
    if (base)
      baseMap = getBaseMap(base->Cards);

    for (oCard &c : src.Cards) {
      const oBase *baseObj = getBaseObject(c);
      const string &stmp = c.getStamp();
      if (!c.isRemoved() && stmp > previousMergeTime) {
        if (stmp > thisMergeTime)
          thisMergeTime = stmp;

        bool merged = false;
        auto mc = crd.find(c.Id);
        if (mc != crd.end()) {
          auto p1 = c.getNumPunches() > 0 ? c.getPunchByIndex(c.getNumPunches() - 1)->getTimeInt() : 0;
          auto c2 = mc->second;
          auto p2 = c2->getNumPunches() > 0 ? c2->getPunchByIndex(c2->getNumPunches() - 1)->getTimeInt() : 0;
          if (p1 == p2)
            mergeItem(mc->second, c, baseObj), merged = true;
        }
        
        auto updateIdCrd = [&](int id) {
          changeBaseId(c.Id, id);
          pCard other = src.getCard(id);
          if (other) 
            other->changeId(c.Id);
          c.changeId(id);
        };

        if (!merged) {
          auto mcN = crdByHash.find(c.getCardHash());
          if (mcN != crdByHash.end()) {
            mergeItem(mcN->second, c, baseObj);
            merged = true;
            updateIdCrd(mcN->second->Id);
          }
        }

        if (!merged) {
          if (crd.count(c.Id)) {
            int newId = max(getFreeCardId(), src.getFreeCardId());
            c.changeId(newId);
          }
          pCard pNew = addCard(c);
          updateNewItem(pNew, c);
        }
      }
    }
  }

  {
    map<int, pClub> clb;
    map<int64_t, pClub> clbByExt;
    map<wstring, pClub> clbByName;

    for (oClub &c : Clubs) {
      if (!c.isRemoved()) {
        clb[c.Id] = &c;
        if (c.getExtIdentifier() != 0)
          clbByExt[c.getExtIdentifier()] = &c;
        clbByName[c.getName()] = &c;
      }
    }
    
    if (base)
      baseMap = getBaseMap(base->Clubs);

    set<int> srcClub;
    for (oClub &c : src.Clubs) {
      const oBase *baseObj = getBaseObject(c);
      const string &stmp = c.getStamp();
      if (!c.isRemoved()) {
        bool okMerge = stmp > previousMergeTime;

        if (stmp > thisMergeTime)
          thisMergeTime = stmp;

        bool merged = false;
        auto mc = clb.find(c.Id);
        if (mc != clb.end()) {
          if ((c.getExtIdentifier() != 0 && c.getExtIdentifier() == mc->second->getExtIdentifier())
              || c.getName() == mc->second->getName()) {
            if (okMerge)
              mergeItem(mc->second, c, baseObj);
            merged = true;
          }
        }

        auto updateIdClb = [&](int id) {
          changeBaseId(c.Id, id);
          pClub other = src.getClub(id);
          if (other)
            other->changeId(c.Id);
          c.changeId(id);
        };

        if (!merged && c.getExtIdentifier() != 0) {
          auto mcN = clbByExt.find(c.getExtIdentifier());
          if (mcN != clbByExt.end()) {
            if (okMerge)
              mergeItem(mcN->second, c, baseObj);
            merged = true;
            updateIdClb(mcN->second->Id);
          }
        }

        if (!merged) {
          auto mcN = clbByName.find(c.getName());
          if (mcN != clbByName.end()) {
            if (okMerge)
              mergeItem(mcN->second, c, baseObj);
            merged = true;
            updateIdClb(mcN->second->Id);
          }
        }

        if (!merged && okMerge) {
          if (clb.count(c.Id)) {
            int newId = max(getFreeClubId(), src.getFreeClubId());
            c.changeId(newId);
          }
          pClub pNew = addClub(c);
          updateNewItem(pNew, c);
        }

        srcClub.insert(c.Id);
      }
    }
    
    computeRemove(Clubs, srcClub, rClub);
  }
  
  {
    map<int, pRunner> rn;
    map<int64_t, pRunner> rnByExt;
    map<pair<int, wstring>, pRunner> rnByCardName;
    
    if (base)
      baseMap = getBaseMap(base->Runners);

    for (oRunner &r : Runners) {
      if (!r.isRemoved()) {
        rn[r.Id] = &r;
        if (r.getExtIdentifier() != 0)
          rnByExt[r.getExtIdentifier()] = &r;
        rnByCardName[make_pair(r.getCardNo(), r.sName)] = &r;
      }
    }
    set<int> srcRunner;
    for (oRunner &r : src.Runners) {
      const oBase *baseObj = getBaseObject(r);
      const string &stmp = r.getStamp();
      if (!r.isRemoved()) {
        bool okMerge = stmp > previousMergeTime;
        if (stmp > thisMergeTime)
          thisMergeTime = stmp;

        bool merged = false;
        auto mc = rn.find(r.Id);
        if (mc != rn.end()) {
          if ((r.getExtIdentifier() != 0 && r.getExtIdentifier() == mc->second->getExtIdentifier())
              || (r.sName == mc->second->sName && r.getClubId() == mc->second->getClubId())
              || r.getCardNo() == mc->second->getCardNo()
              || mc->second->isVacant()) {

            if (okMerge)
              mergeItem(mc->second, r, baseObj);
            merged = true;
          }
        }

        auto updateIdR = [&](int id) {
          changeBaseId(r.Id, id);
          pRunner other = src.getRunner(id, 0);          
          if (other) 
            other->changeId(r.Id);
          r.changeId(id);
        };

        if (!merged && r.getExtIdentifier() != 0) {
          auto mcN = rnByExt.find(r.getExtIdentifier());
          if (mcN != rnByExt.end()) {
            if (okMerge)
              mergeItem(mcN->second, r, baseObj);
            merged = true;
            updateIdR(mcN->second->Id);
          }
        }

        if (!merged) {
          auto mcN = rnByCardName.find(make_pair(r.getCardNo(), r.sName));
          if (mcN != rnByCardName.end()) {
            if (okMerge)
              mergeItem(mcN->second, r, baseObj);
            merged = true;
            updateIdR(mcN->second->Id);
          }
        }

        if (!merged && okMerge) {
          if (rn.count(r.Id)) {
            int newId = max(getFreeRunnerId(), src.getFreeRunnerId());
            r.changeId(newId);
          }
          pRunner pNew = addRunner(r, false);
          updateNewItem(pNew, r);
        }

        srcRunner.insert(r.Id);
      }
    }

    computeRemove(Runners, srcRunner, rRunner);
  }
  
  {
    map<int, pTeam> tm;
    map<pair<int, wstring>, pTeam> tmByClassName;

    for (oTeam &t : Teams) {
      if (!t.isRemoved()) {
        tm[t.Id] = &t;
        tmByClassName[make_pair(t.getClassId(false), t.getName())] = &t;
      }
    }

    if (base)
      baseMap = getBaseMap(base->Teams);

    set<int> srcTeam;
    for (oTeam &t : src.Teams) {
      const oBase *baseObj = getBaseObject(t);
      const string &stmp = t.getStamp();
      if (!t.isRemoved()) {
        bool okMerge = stmp > previousMergeTime;
        if (stmp > thisMergeTime)
          thisMergeTime = stmp;

        bool merged = false;
        auto mc = tm.find(t.Id);
        if (mc != tm.end()) {
          if (t.getClubId() == mc->second->getClubId()) {
            if (okMerge)
              mergeItem(mc->second, t, baseObj);
            merged = true;
          }
        }

        auto updateIdT = [&](int id) {
          changeBaseId(t.Id, id);
          pTeam other = src.getTeam(id);
          if (other)
            other->changeId(t.Id);
          t.changeId(id);
        };
          
        if (!merged) {
          auto mcN = tmByClassName.find(make_pair(t.getClassId(false), t.getName()));
          if (mcN != tmByClassName.end()) {
            if (okMerge)
              mergeItem(mcN->second, t, baseObj);
            merged = true;
            updateIdT(mcN->second->Id);
          }
        }

        if (!merged && okMerge) {
          if (tm.count(t.Id)) {
            int newId = max(getFreeTeamId(), src.getFreeTeamId());
            t.changeId(newId);
          }
          pTeam pNew = addTeam(t, false);
          updateNewItem(pNew, t);
        }

        srcTeam.insert(t.Id);
      }
    }

    computeRemove(Teams, srcTeam, rTeam);
  }

  auto removeEnts = [&numRemove](const set<int> &ids, auto get) {
    for (int id : ids) {
      pBase b = get(id);
      if (b && b->canRemove()) {
        b->remove();
        numRemove++;
      }
    }
  };

  if (allowRemove) {
    removeEnts(rTeam, [this](int id) -> pBase {return getTeam(id); });
    removeEnts(rRunner, [this](int id) -> pBase {return getRunner(id, 0); });
    removeEnts(rClub, [this](int id) -> pBase {return getClub(id); });
    removeEnts(rClass, [this](int id) -> pBase {return getClass(id); });
    removeEnts(rCourse, [this](int id) -> pBase {return getCourse(id); });
    removeEnts(rControl, [this](int id) -> pBase {return getControl(id); });
  }

  wstring mtOut(thisMergeTime.begin(), thisMergeTime.end());
  addMergeInfo(mergeTag, mtOut);
  synchronize();
}

wstring oEvent::cloneCompetition(bool cloneRunners, bool cloneTimes,
                                 bool cloneCourses, bool cloneResult, bool addToDate) {

  if (cloneResult) {
    cloneTimes = true;
    cloneCourses = true;
  }
  if (cloneTimes)
    cloneRunners = true;

  oEvent ce(gdibase);
  ce.newCompetition(Name);
  ce.ZeroTime = ZeroTime;
  ce.Date = Date;

  if (addToDate) {
    SYSTEMTIME st;
    convertDateYMS(Date, st, false);
    __int64 absD = SystemTimeToInt64Second(st);
    absD += 3600 * 24;
    ce.Date = convertSystemDate(Int64SecondToSystemTime(absD));
  }
  int len = Name.length();
  if (len > 2 && isdigit(Name[len - 1]) && !isdigit(Name[len - 2])) {
    ++ce.Name[len - 1]; // E1 -> E2
  }
  else
    ce.Name += L" E2";

  memcpy(ce.oData, oData, sizeof(oData));

  for (oClubList::iterator it = Clubs.begin(); it != Clubs.end(); ++it) {
    if (it->isRemoved())
      continue;
    pClub pc = ce.addClub(it->name, it->Id);
    memcpy(pc->oData, it->oData, sizeof(pc->oData));
  }

  if (cloneCourses) {
    for (oControlList::iterator it = Controls.begin(); it != Controls.end(); ++it) {
      if (it->isRemoved())
        continue;
      pControl pc = ce.addControl(it->Id, 100, it->Name);
      pc->setNumbers(it->codeNumbers());
      pc->Status = it->Status;
      memcpy(pc->oData, it->oData, sizeof(pc->oData));
    }

    for (oCourseList::iterator it = Courses.begin(); it != Courses.end(); ++it) {
      if (it->isRemoved())
        continue;
      pCourse pc = ce.addCourse(it->Name, it->Length, it->Id);
      pc->importControls(it->getControls(), false, false);
      pc->legLengths = it->legLengths;
      memcpy(pc->oData, it->oData, sizeof(pc->oData));
    }
  }

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    pClass pc = ce.addClass(it->Name, 0, it->Id);
    memcpy(pc->oData, it->oData, sizeof(pc->oData));
    pc->setNumStages(it->getNumStages());
    pc->legInfo = it->legInfo;

    if (cloneCourses) {
      pc->Course = ce.getCourse(it->getCourseId());
      pc->MultiCourse = it->MultiCourse; // Points to wrong competition, but valid for now...
    }
  }

  if (cloneRunners) {
    for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved())
        continue;

      oRunner r(&ce, it->Id);
      r.sName = it->sName;
      r.getRealName(r.sName, r.tRealName);
      r.StartNo = it->StartNo;
      r.cardNumber = it->cardNumber;
      r.Club = ce.getClub(it->getClubId());
      r.Class = ce.getClass(it->getClassId(false));

      if (cloneCourses)
        r.Course = ce.getCourse(it->getCourseId());

      pRunner pr = ce.addRunner(r, false);

      pr->decodeMultiR(it->codeMultiR());
      memcpy(pr->oData, it->oData, sizeof(pr->oData));

      if (cloneTimes) {
        pr->startTime = it->startTime;
      }

      if (cloneResult) {
        if (it->Card) {
          pr->Card = ce.addCard(*it->Card);
          pr->Card->tOwner = pr;
        }
        pr->FinishTime = it->FinishTime;
        pr->status = it->status;
      }
    }

    for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
      if (it->skip())
        continue;

      oTeam t(&ce, it->Id);

      t.sName = it->sName;
      t.StartNo = it->StartNo;
      t.Club = ce.getClub(it->getClubId());
      t.Class = ce.getClass(it->getClassId(false));

      if (cloneTimes)
        t.startTime = it->startTime;

      pTeam pt = ce.addTeam(t, false);
      memcpy(pt->oData, it->oData, sizeof(pt->oData));

      pt->Runners.resize(it->Runners.size());
      for (size_t k = 0; k<it->Runners.size(); k++) {
        int id = it->Runners[k] ? it->Runners[k]->Id : 0;
        if (id)
          pt->Runners[k] = ce.getRunner(id, 0);
      }

      t.apply(ChangeType::Update, nullptr);
    }

    for (oRunnerList::iterator it = ce.Runners.begin(); it != ce.Runners.end(); ++it) {
      it->createMultiRunner(false, false);
    }
  }

  vector<pRunner> changedClass, changedClassNoResult, assignedVacant, newEntries, notTransfered, noAssign;
  set<int> dummy;
  transferResult(ce, dummy, TransferAnyway, false, changedClass, changedClassNoResult, assignedVacant, newEntries, notTransfered, noAssign);

  vector<pTeam> newEntriesT, notTransferedT, noAssignT;
  transferResult(ce, TransferAnyway, newEntriesT, notTransferedT, noAssignT);

  int eventNumberCurrent = getStageNumber();
  if (eventNumberCurrent <= 0) {
    eventNumberCurrent = 1;
    setStageNumber(eventNumberCurrent);
  }

  ce.getDI().setString("PreEvent", currentNameId);
  ce.setStageNumber(eventNumberCurrent + 1);
  getDI().setString("PostEvent", ce.currentNameId);

  int nf = getMeOSFeatures().getNumFeatures();
  for (int k = 0; k < nf; k++) {
    MeOSFeatures::Feature f = getMeOSFeatures().getFeature(k);
    if (getMeOSFeatures().hasFeature(f))
      ce.getMeOSFeatures().useFeature(f, true, ce);
  }

  // Transfer lists and list configurations.
  if (listContainer) {
    loadGeneralResults(false, false);
    swap(ce.generalResults, generalResults);
    try {
      ce.listContainer = new MetaListContainer(&ce, *listContainer);
      ce.save();
    }
    catch (...) {
      swap(ce.generalResults, generalResults);
      throw;
    }

    swap(ce.generalResults, generalResults);
  }
  return ce.CurrentFile;
}

void oEvent::transferListsAndSave(const oEvent &src) {
  src.loadGeneralResults(false, false);
  swap(src.generalResults, generalResults);
  try {
    src.getListContainer().synchronizeTo(getListContainer());
    save();
  }
  catch (...) {
    swap(src.generalResults, generalResults);
    throw;
  }

  swap(src.generalResults, generalResults);
}


bool checkTargetClass(pRunner target, pRunner source,
                      const oClassList &Classes,
                      const vector<pRunner> &targetVacant,
                      vector<pRunner> &changedClass,
                      oEvent::ChangedClassMethod changeClassMethod) {
  if (changeClassMethod == oEvent::TransferAnyway)
    return true;

  if (!compareClassName(target->getClass(false), source->getClass(false))) {
    // Store all vacant positions in the right class
    int targetClass = -1;

    if (target->getStatus() == StatusOK) {
      // There is already a result. Do not change class!
      return false;
    }

    if (changeClassMethod == oEvent::TransferNoResult)
      return false; // Do not allow change class, do not transfer result

    for (oClassList::const_iterator cit = Classes.begin(); cit != Classes.end(); ++cit) {
      if (cit->isRemoved())
        continue;
      if (compareClassName(cit->getName(), source->getClass(false))) {
        targetClass = cit->getId();

        if (targetClass == source->getClassId(false) || cit->getName() == source->getClass(false))
          break; // Assume exact match
      }
    }

    if (targetClass != -1) {
      set<int> vacantIx;
      for (size_t j = 0; j < targetVacant.size(); j++) {
        if (!targetVacant[j])
          continue;
        if (targetVacant[j]->getClassId(false) == targetClass)
          vacantIx.insert(j);
      }
      int posToUse = -1;
      if (vacantIx.size() == 1)
        posToUse = *vacantIx.begin();
      else if (vacantIx.size() > 1) {
        wstring srcBib = source->getBib();
        if (srcBib.length() > 0) {
          for (set<int>::iterator tit = vacantIx.begin(); tit != vacantIx.end(); ++tit) {
            if (targetVacant[*tit]->getBib() == srcBib) {
              posToUse = *tit;
              break;
            }
          }
        }

        if (posToUse == -1)
          posToUse = *vacantIx.begin();
      }

      if (posToUse != -1) {
        // Change class or change class vacant
        changedClass.push_back(target);

        int oldStart = target->getStartTime();
        wstring oldBib = target->getBib();
        int oldSN = target->getStartNo();
        int oldClass = target->getClassId(false);
        pRunner tgt = targetVacant[posToUse];
        target->cloneStartTime(tgt);
        target->setBib(tgt->getBib(), 0, false);
        target->setStartNo(tgt->getStartNo(), oBase::ChangeType::Update);
        target->setClassId(tgt->getClassId(false), false);

        tgt->setStartTime(oldStart, true, oBase::ChangeType::Update);
        tgt->setBib(oldBib, 0, false);
        tgt->setStartNo(oldSN, oBase::ChangeType::Update);
        tgt->setClassId(oldClass, false);
        return true; // Changed to correct class
      }
      else if (changeClassMethod == oEvent::ChangeClass) {
        // Simpliy change class
        target->setClassId(targetClass, false);
        return true;
      }
    }
    return false; // Wrong class, ChangeClass (but failed)
  }

  return true; // Same class, OK
}


size_t levenshtein_distance(const wstring &s, const wstring &t) {
  size_t n = s.length() + 1;
  size_t m = t.length() + 1;
  vector<size_t> d(n * m);
   
  for (size_t i = 1, im = 0; i < m; ++i, ++im) {
    for (size_t j = 1, jn = 0; j < n; ++j, ++jn)  {
      if (s[jn] == t[im]) {
        d[(i * n) + j] = d[((i - 1) * n) + (j - 1)];
      }
      else {
        d[(i * n) + j] = min(d[(i - 1) * n + j] + 1, /* A deletion. */
                             min(d[i * n + (j - 1)] + 1, /* An insertion. */
                             d[(i - 1) * n + (j - 1)] + 1)); /* A substitution. */
      }
    }
  }

  return d[n * m - 1];
}

void oEvent::transferResult(oEvent &ce,
                            const set<int> &allowNewEntries,
                            ChangedClassMethod changeClassMethod,
                            bool transferAllNoCompete,
                            vector<pRunner> &changedClass,
                            vector<pRunner> &changedClassNoResult,
                            vector<pRunner> &assignedVacant,
                            vector<pRunner> &newEntries,
                            vector<pRunner> &notTransfered,
                            vector<pRunner> &noAssignmentTarget) {

  inthashmap processed(ce.Runners.size());
  inthashmap used(Runners.size());

  changedClass.clear();
  changedClassNoResult.clear();
  assignedVacant.clear();
  newEntries.clear();
  notTransfered.clear();
  noAssignmentTarget.clear();

  // Setup class map this id -> dst id
  map<int, int> classMap;
  for (auto &c : Classes) {
    pClass dCls = ce.getClass(c.Id);
    if (dCls && compareClassName(dCls->getName(), c.getName())) {
      classMap[c.Id] = c.Id;
      continue;
    }
    dCls = ce.getClass(c.getName());
    if (dCls)
      classMap[c.Id] = dCls->getId();
  }

  vector<pRunner> targetRunners;
  vector<pRunner> targetVacant;

  targetRunners.reserve(ce.Runners.size());
  for (oRunnerList::iterator it = ce.Runners.begin(); it != ce.Runners.end(); ++it) {
    if (!it->skip()) {
      if (!it->isVacant())
        targetRunners.push_back(&*it);
      else
        targetVacant.push_back(&*it);
    }
  }

  calculateResults({}, ResultType::TotalResult);
  // Lookup by id
  for (size_t k = 0; k < targetRunners.size(); k++) {
    pRunner it = targetRunners[k];
    pRunner r = getRunner(it->Id, 0);
    if (!r)
      continue;

    __int64 id1 = r->getExtIdentifier();
    __int64 id2 = it->getExtIdentifier();

    if (id1>0 && id2>0 && id1 != id2)
      continue;

    wstring cnA = canonizeName(it->getName().c_str());
    wstring cnB = canonizeName(r->getName().c_str());
    wstring ccnA = canonizeName(it->getClub().c_str());
    wstring ccnB = canonizeName(r->getClub().c_str());

    if (levenshtein_distance(cnA, cnB) > 3)
      continue; // Too different

    if ((id1>0 && id1 == id2) ||
      (r->cardNumber>0 && r->cardNumber == it->cardNumber) ||
        (it->getName() == r->getName()) || (cnA == cnB && ccnA == ccnB)) {
      processed.insert(it->Id, 1);
      used.insert(r->Id, 1);
      if (checkTargetClass(it, r, ce.Classes, targetVacant, changedClass, changeClassMethod))
        it->setInputData(*r);
      else {
        it->resetInputData();
        changedClassNoResult.push_back(it);
      }
    }
  }

  if (processed.size() < int(targetRunners.size())) {
    // Lookup by card
    int v;
    for (size_t k = 0; k < targetRunners.size(); k++) {
      pRunner it = targetRunners[k];
      if (processed.lookup(it->Id, v))
        continue;
      if (it->getCardNo() > 0) {
        pRunner r = getRunnerByCardNo(it->getCardNo(), 0, CardLookupProperty::Any);

        if (!r || used.lookup(r->Id, v))
          continue;

        __int64 id1 = r->getExtIdentifier();
        __int64 id2 = it->getExtIdentifier();

        if (id1>0 && id2>0 && id1 != id2)
          continue;

        if ((id1>0 && id1 == id2) || (it->getName() == r->getName() && it->getClub() == r->getClub())) {
          processed.insert(it->Id, 1);
          used.insert(r->Id, 1);
          if (checkTargetClass(it, r, ce.Classes, targetVacant, changedClass, changeClassMethod))
            it->setInputData(*r);
          else {
            it->resetInputData();
            changedClassNoResult.push_back(it);
          }
        }
      }
    }
  }

  int v = -1;

  // Store remaining runners
  vector<pRunner> remainingRunners;
  for (oRunnerList::iterator it2 = Runners.begin(); it2 != Runners.end(); ++it2) {
    if (it2->skip() || used.lookup(it2->Id, v))
      continue;
    if (it2->isVacant())
      continue; // Ignore vacancies on source side

    remainingRunners.push_back(&*it2);
  }

  if (processed.size() < int(targetRunners.size()) && !remainingRunners.empty()) {
    // Lookup by name / ext id
    vector<int> cnd;
    for (size_t k = 0; k < targetRunners.size(); k++) {
      pRunner it = targetRunners[k];
      if (processed.lookup(it->Id, v))
        continue;

      __int64 id1 = it->getExtIdentifier();

      cnd.clear();
      for (size_t j = 0; j < remainingRunners.size(); j++) {
        pRunner src = remainingRunners[j];
        if (!src)
          continue;

        if (id1 > 0) {
          __int64 id2 = src->getExtIdentifier();
          if (id2 == id1) {
            cnd.clear();
            cnd.push_back(j);
            break; //This is the one, if they have the same Id there will be a unique match below
          }
        }
        if (it->getName() == src->getName() && it->getClub() == src->getClub())
          cnd.push_back(j);
      }

      if (cnd.size() == 1) {
        pRunner &src = remainingRunners[cnd[0]];
        processed.insert(it->Id, 1);
        used.insert(src->Id, 1);
        if (checkTargetClass(it, src, ce.Classes, targetVacant, changedClass, changeClassMethod)) {
          it->setInputData(*src);
        }
        else {
          it->resetInputData();
          changedClassNoResult.push_back(it);
        }
        src = 0;
      }
      else if (cnd.size() > 0) { // More than one candidate
        int winnerIx = -1;
        int point = -1;
        for (size_t j = 0; j < cnd.size(); j++) {
          pRunner src = remainingRunners[cnd[j]];
          int p = 0;
          if (src->getClass(false) == it->getClass(false))
            p += 1;
          if (src->getBirthYear() == it->getBirthYear())
            p += 2;
          if (p > point) {
            winnerIx = cnd[j];
            point = p;
          }
        }

        if (winnerIx != -1) {
          processed.insert(it->Id, 1);
          pRunner winner = remainingRunners[winnerIx];
          remainingRunners[winnerIx] = 0;

          used.insert(winner->Id, 1);
          if (checkTargetClass(it, winner, ce.Classes, targetVacant, changedClass, changeClassMethod)) {
            it->setInputData(*winner);
          }
          else {
            it->resetInputData();
            changedClassNoResult.push_back(it);
          }
        }
      }
    }
  }

  vector<pair<wstring, pRunner>> remainingNamesSource;
  vector<pair<wstring, pRunner>> remainingNamesDest;
  for (auto &r : Runners) {
    if (r.skip() || used.lookup(r.Id, v))
      continue;
    if (r.isVacant())
      continue; // Ignore vacancies on source side

    remainingNamesSource.emplace_back(canonizeName(r.getName().c_str()), &r);
  }

  for (auto &r : ce.Runners) {
    if (r.skip() || processed.lookup(r.Id, v))
      continue;
    if (r.isVacant())
      continue; // Ignore vacancies on source side

    remainingNamesDest.emplace_back(canonizeName(r.getName().c_str()), &r);
  }
  
  if (!remainingNamesSource.empty() && remainingNamesDest.empty()) {
    sort(remainingNamesSource.begin(), remainingNamesSource.end());
    sort(remainingNamesDest.begin(), remainingNamesDest.end());

    int srcPointer = 0;
    int dstPointer = 0;
  }
  

  // Transfer vacancies
  for (size_t k = 0; k < remainingRunners.size(); k++) {
    pRunner src = remainingRunners[k];
    if (!src || used.lookup(src->Id, v))
      continue;

    bool forceSkip = src->hasFlag(oAbstractRunner::FlagTransferSpecified) &&
      !src->hasFlag(oAbstractRunner::FlagTransferNew);

    if (forceSkip) {
      notTransfered.push_back(src);
      continue;
    }

    pRunner targetVacant = ce.getRunner(src->getId(), 0);
    if (targetVacant && targetVacant->isVacant() && compareClassName(targetVacant->getClass(false), src->getClass(false))) {
      targetVacant->setName(src->sName, false);
      targetVacant->setClub(src->getClub());
      targetVacant->setCardNo(src->getCardNo(), false);
      targetVacant->cloneData(src);
      assignedVacant.push_back(targetVacant);
    }
    else {
      pClass dstClass = ce.getClass(src->getClassId(false));
      if (dstClass && compareClassName(dstClass->getName(), src->getClass(false))) {
        if ((!src->hasFlag(oAbstractRunner::FlagTransferSpecified) && allowNewEntries.count(src->getClassId(false)))
            || src->hasFlag(oAbstractRunner::FlagTransferNew)) {
          if (src->getClubId() > 0)
            ce.getClubCreate(src->getClubId(), src->getClub());

          if (!classMap.count(src->getClassId(false)))
            continue;

          pRunner dst = ce.addRunner(src->sName, src->getClub(), classMap[src->getClassId(false)],
                                     src->getCardNo(), src->getBirthYear(), true);
          dst->cloneData(src);
          dst->setInputData(*src);
          newEntries.push_back(dst);
        }
        else if (transferAllNoCompete) {
          if (src->getClubId() > 0)
            ce.getClubCreate(src->getClubId(), src->getClub());

          if (!classMap.count(src->getClassId(false)))
            continue;

          pRunner dst = ce.addRunner(src->sName, src->getClub(), classMap[src->getClassId(false)],
                                     0, src->getBirthYear(), true);
          dst->cloneData(src);
          dst->setInputData(*src);
          dst->setStatus(StatusNotCompetiting, true, ChangeType::Update);
          notTransfered.push_back(dst);
        }
        else
          notTransfered.push_back(src);
      }
    }
  }

  // Runners on target side not assigned a result
  for (size_t k = 0; k < targetRunners.size(); k++) {
    if (targetRunners[k] && !processed.count(targetRunners[k]->Id)) {
      noAssignmentTarget.push_back(targetRunners[k]);
      if (targetRunners[k]->inputStatus == StatusUnknown ||
        (targetRunners[k]->inputStatus == StatusOK && targetRunners[k]->inputTime == 0)) {
        targetRunners[k]->inputStatus = StatusNotCompetiting;
      }
    }
  }
}

void oEvent::transferResult(oEvent &ce,
                            ChangedClassMethod changeClassMethod,
                            vector<pTeam> &newEntries,
                            vector<pTeam> &notTransfered,
                            vector<pTeam> &noAssignmentTarget) {

  inthashmap processed(ce.Teams.size());
  inthashmap used(Teams.size());

  newEntries.clear();
  notTransfered.clear();
  noAssignmentTarget.clear();

  vector<pTeam> targetTeams;

  targetTeams.reserve(ce.Teams.size());
  for (oTeamList::iterator it = ce.Teams.begin(); it != ce.Teams.end(); ++it) {
    if (!it->skip()) {
      targetTeams.push_back(&*it);
    }
  }

  calculateTeamResults(set<int>(), ResultType::TotalResult);
  // Lookup by id
  for (size_t k = 0; k < targetTeams.size(); k++) {
    pTeam it = targetTeams[k];
    pTeam t = getTeam(it->Id);
    if (!t)
      continue;

    __int64 id1 = t->getExtIdentifier();
    __int64 id2 = it->getExtIdentifier();

    if (id1>0 && id2>0 && id1 != id2)
      continue;

    if ((id1>0 && id1 == id2) || (it->sName == t->sName && it->getClub() == t->getClub())) {
      processed.insert(it->Id, 1);
      used.insert(t->Id, 1);
      it->setInputData(*t);
      //checkTargetClass(it, r, ce.Classes, targetVacant, changedClass);
    }
  }

  int v = -1;

  // Store remaining runners
  vector<pTeam> remainingTeams;
  for (oTeamList::iterator it2 = Teams.begin(); it2 != Teams.end(); ++it2) {
    if (it2->skip() || used.lookup(it2->Id, v))
      continue;
    if (it2->isVacant())
      continue; // Ignore vacancies on source side

    remainingTeams.push_back(&*it2);
  }

  if (processed.size() < int(targetTeams.size()) && !remainingTeams.empty()) {
    // Lookup by name / ext id
    vector<int> cnd;
    for (size_t k = 0; k < targetTeams.size(); k++) {
      pTeam it = targetTeams[k];
      if (processed.lookup(it->Id, v))
        continue;

      __int64 id1 = it->getExtIdentifier();

      cnd.clear();
      for (size_t j = 0; j < remainingTeams.size(); j++) {
        pTeam src = remainingTeams[j];
        if (!src)
          continue;

        if (id1 > 0) {
          __int64 id2 = src->getExtIdentifier();
          if (id2 == id1) {
            cnd.clear();
            cnd.push_back(j);
            break; //This is the one, if they have the same Id there will be a unique match below
          }
        }

        if (it->sName == src->sName && it->getClub() == src->getClub())
          cnd.push_back(j);
      }

      if (cnd.size() == 1) {
        pTeam &src = remainingTeams[cnd[0]];
        processed.insert(it->Id, 1);
        used.insert(src->Id, 1);
        it->setInputData(*src);
        //checkTargetClass(it, src, ce.Classes, targetVacant, changedClass);
        src = 0;
      }
      else if (cnd.size() > 0) { // More than one candidate
        int winnerIx = -1;
        int point = -1;
        for (size_t j = 0; j < cnd.size(); j++) {
          pTeam src = remainingTeams[cnd[j]];
          int p = 0;
          if (src->getClass(false) == it->getClass(false))
            p += 1;
          if (p > point) {
            winnerIx = cnd[j];
            point = p;
          }
        }

        if (winnerIx != -1) {
          processed.insert(it->Id, 1);
          pTeam winner = remainingTeams[winnerIx];
          remainingTeams[winnerIx] = 0;

          used.insert(winner->Id, 1);
          it->setInputData(*winner);
          //checkTargetClass(it, winner, ce.Classes, targetVacant, changedClass);
        }
      }
    }
  }
  /*
  // Transfer vacancies
  for (size_t k = 0; k < remainingRunners.size(); k++) {
  pRunner src = remainingRunners[k];
  if (!src || used.lookup(src->Id, v))
  continue;

  pRunner targetVacant = ce.getRunner(src->getId(), 0);
  if (targetVacant && targetVacant->isVacant() && compareClassName(targetVacant->getClass(), src->getClass()) ) {
  targetVacant->setName(src->getName());
  targetVacant->setClub(src->getClub());
  targetVacant->setCardNo(src->getCardNo(), false);
  targetVacant->cloneData(src);
  assignedVacant.push_back(targetVacant);
  }
  else {
  pClass dstClass = ce.getClass(src->getClassId());
  if (dstClass && compareClassName(dstClass->getName(), src->getClass())) {
  if (allowNewEntries.count(src->getClassId())) {
  if (src->getClubId() > 0)
  ce.getClubCreate(src->getClubId(), src->getClub());
  pRunner dst = ce.addRunner(src->getName(), src->getClubId(), src->getClassId(),
  src->getCardNo(), src->getBirthYear(), true);
  dst->cloneData(src);
  dst->setInputData(*src);
  newEntries.push_back(dst);
  }
  else if (transferAllNoCompete) {
  if (src->getClubId() > 0)
  ce.getClubCreate(src->getClubId(), src->getClub());
  pRunner dst = ce.addRunner(src->getName(), src->getClubId(), src->getClassId(),
  0, src->getBirthYear(), true);
  dst->cloneData(src);
  dst->setInputData(*src);
  dst->setStatus(StatusNotCompetiting);
  notTransfered.push_back(dst);
  }
  else
  notTransfered.push_back(src);
  }
  }
  }

  // Runners on target side not assigned a result
  for (size_t k = 0; k < targetRunners.size(); k++) {
  if (targetRunners[k] && !processed.count(targetRunners[k]->Id)) {
  noAssignmentTarget.push_back(targetRunners[k]);
  if (targetRunners[k]->inputStatus == StatusUnknown ||
  (targetRunners[k]->inputStatus == StatusOK && targetRunners[k]->inputTime == 0)) {
  targetRunners[k]->inputStatus = StatusNotCompetiting;
  }
  }

  }*/
}

void oAbstractRunner::merge(const oBase &input, const oBase *baseIn) {
  const oAbstractRunner &src = dynamic_cast<const oAbstractRunner&>(input);
  const oAbstractRunner *base = dynamic_cast<const oAbstractRunner*>(baseIn);

  if (base == nullptr || base->sName != src.sName)
    setName(src.sName, false);

  if (base == nullptr || base->startTime != src.startTime)
    setStartTime(src.startTime, true, ChangeType::Update, false);

  if (base == nullptr || base->FinishTime != src.FinishTime)
    setFinishTime(src.FinishTime);

  if (base == nullptr || base->status != src.status)
    setStatus(src.status, true, ChangeType::Update);

  if (base == nullptr || base->StartNo != src.StartNo)
    setStartNo(src.StartNo, ChangeType::Update);

  if (base == nullptr || base->getClub() != src.getClub())
    setClubId(src.getClubId());

  if (base == nullptr || base->getClass(false) != src.getClass(false))
    setClassId(src.getClassId(false), false);
  
  if (base == nullptr || base->inputPlace != src.inputPlace)
    setInputPlace(src.inputPlace);

  if (base == nullptr || base->inputTime != src.inputTime) {
    if (inputTime != src.inputTime) {
      inputTime = src.inputTime;
      updateChanged();
    }
  }
  if (base == nullptr || base->inputStatus != src.inputStatus)
    setInputStatus(src.inputStatus);
  
  if (base == nullptr || base->inputPoints != src.inputPoints)
   setInputPoints(src.inputPoints);
}

void oRunner::merge(const oBase &input, const oBase *baseIn) {
  oAbstractRunner::merge(input, baseIn);

  const oRunner &src = dynamic_cast<const oRunner&>(input);
  const oRunner *base = dynamic_cast<const oRunner*>(baseIn);

  if (base == nullptr || base->getCourseId() != src.getCourseId())
    setCourseId(src.getCourseId());
  
  if ((base == nullptr && src.getCardId() != 0) || (base != nullptr && base->getCardId() != src.getCardId()))
    setCard(src.getCardId());

  if (base == nullptr || base->getCardNo() != src.getCardNo())
    setCardNo(src.getCardNo(), false);

  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oTeam::merge(const oBase &input, const oBase *baseIn) {
  oAbstractRunner::merge(input, baseIn);

  const oTeam &src = dynamic_cast<const oTeam&>(input);
  const oTeam *base = dynamic_cast<const oTeam*>(baseIn);

  auto getRId = [](const oTeam &t, int ix) {
    pRunner r = t.getRunner(ix);
    return r ? r->getId() : 0;
  };
  
  bool chR;
  if (base) {
    chR = base->Runners.size() != src.Runners.size();
    if (!chR) {
      for (size_t i = 0; i < src.Runners.size(); i++) {
        if (getRId(src, i) != getRId(*base, i))
          chR = true;
      }
    }
  }
  else chR = true;

  if (chR) {
    bool same = src.Runners.size() == Runners.size();
    vector<int> r(src.Runners.size());
    for (size_t i = 0; i < src.Runners.size(); i++) {
      if (src.Runners[i]) {
        r[i] = src.Runners[i]->Id;
        src.Runners[i]->tInTeam = nullptr;
      }
      if (same) {
        int rc = Runners[i] ? Runners[i]->Id : 0;
        if (rc != r[i])
          same = false;
      }
    }
    
    if (!same) {
      importRunners(r);
      updateChanged();
    }
  }
  
  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oControl::merge(const oBase &input, const oBase *base) {
  const oControl &src = dynamic_cast<const oControl&>(input);
  if (src.Name.length() > 0)
    setName(src.Name);
  setNumbers(src.codeNumbers());
  setStatus(src.getStatus());
  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oCourse::merge(const oBase &input, const oBase *baseIn) {
  const oCourse &src = dynamic_cast<const oCourse&>(input);
  const oCourse *base = dynamic_cast<const oCourse*>(baseIn);

  if ((base == nullptr || base->Name != src.Name) && (src.Name.length() > 0))
    setName(src.Name);
  if (!base || base->Length != src.Length)
    setLength(src.Length);

  importControls(src.getControls(), true, false);
  importLegLengths(src.getLegLengths(), true);

  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oClass::merge(const oBase &input, const oBase *base) {
  const oClass &src = dynamic_cast<const oClass&>(input);

  if (src.Name.length() > 0)
    setName(src.Name, true);
  setCourse(oe->getCourse(src.getCourseId()));

  if (src.MultiCourse.size() > 0) {
    vector<vector<pCourse>> mcCopy = MultiCourse;
    set<int> cid;
    vector< vector<int> > multi;
    parseCourses(src.codeMultiCourse(), multi, cid);
    importCourses(multi);

    if (mcCopy != MultiCourse)
      updateChanged();
  }
  else {
    setNumStages(0);
  }

  if (src.legInfo.size() > 0) {
    if (codeLegMethod() != src.codeLegMethod()) {
      importLegMethod(src.codeLegMethod());
      updateChanged();
    }
  }

  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oClub::merge(const oBase &input, const oBase *base) {
  const oClub &src = dynamic_cast<const oClub&>(input);

  setName(src.getName());
  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

void oCard::merge(const oBase &input, const oBase *base) {
  const oCard &src = dynamic_cast<const oCard&>(input);

  setCardNo(src.getCardNo());
  if (readId != src.readId) {
    readId = src.readId;
    updateChanged();
  }
  if (getPunchString() != src.getPunchString()) {
    importPunches(src.getPunchString());
    updateChanged();
  }
  synchronize(true);
}

void oPunch::merge(const oBase &input, const oBase *base) {
  const oPunch &src = dynamic_cast<const oPunch&>(input);
// Not implemented
}


void oFreePunch::merge(const oBase &input, const oBase *base) {
  const oFreePunch &src = dynamic_cast<const oFreePunch&>(input);
  // Not implemented
}


void oEvent::merge(const oBase &srcIn, const oBase *base) {
}


wstring oEvent::getMergeTag(bool forceReset) {
  wstring sm = getDCI().getString("MergeTag");
  if (sm.empty() || forceReset) {
    static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

    random_device r;
    mt19937 e1(r());

    wchar_t s[13];
    for (int i = 0; i < 12; ++i) {
      s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[12] = 0;
    getDI().setString("MergeTag", s);
    synchronize(true);
    sm = s;
  }
  return sm;
}

wstring oEvent::getMergeInfo(const wstring &tag) const {
  wstring mv = getDCI().getString("MergeInfo");
  vector<wstring> mvv;
  split(mv, L":", mvv);
  for (size_t j = 0; j + 1 < mvv.size(); j += 2) {
    if (mvv[j] == tag)
      return mvv[j + 1];
  }
  return L"";
}

void oEvent::addMergeInfo(const wstring &tag, const wstring &version) {
  wstring mv = getDCI().getString("MergeInfo");
  vector<wstring> mvv;
  split(mv, L":", mvv);
  bool ok = false;
  for (size_t j = 0; j + 1 < mvv.size(); j += 2) {
    if (mvv[j] == tag)
      mvv[j + 1] = version, ok = true;
  }
  if (!ok) {
    mvv.push_back(tag);
    mvv.push_back(version);
  }
  unsplit(mvv, L":", mv);
  getDI().setString("MergeInfo", mv);
}

string oEvent::getLastModified() const {
  string s;

  auto maxModified = [&s](auto &cnt) {
    for (auto &b : cnt)
      if (!b.isRemoved() && b.getStamp() > s)
        s = b.getStamp();
  };

  maxModified(Controls);
  maxModified(Courses);
  maxModified(Classes);
  maxModified(Cards);
  maxModified(Clubs);
  maxModified(Runners);
  maxModified(Teams);
  // maxModified(punches); xxx ignored

  return s;
}
