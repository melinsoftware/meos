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
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"
#include <algorithm>
#include <cassert>

#include "oFreePunch.h"
#include "oEvent.h"
#include "Table.h"
#include "meos_util.h"
#include "Localizer.h"
#include "intkeymapimpl.hpp"
#include "socket.h"
#include "meosdb/sqltypes.h"
#include "gdioutput.h"

bool oFreePunch::disableHashing = false;

oFreePunch::oFreePunch(oEvent *poe, int card, int time, int type): oPunch(poe)
{
  Id=oe->getFreePunchId();
  CardNo = card;
  Time = time;
  Type = type;
  iHashType = 0;
  tRunnerId = 0;
}

oFreePunch::oFreePunch(oEvent *poe, int id): oPunch(poe)
{
  Id=id;
  oe->qFreePunchId = max(id, oe->qFreePunchId);
  iHashType = 0;
  tRunnerId = 0;
}

oFreePunch::~oFreePunch(void)
{
}

bool oFreePunch::Write(xmlparser &xml)
{
  if (Removed) return true;
  xml.startTag("Punch");
  xml.write("CardNo", CardNo);
  xml.write("Time", Time);
  xml.write("Type", Type);
  xml.write("Id", Id);
  xml.write("Updated", Modified.getStamp());
  xml.endTag();

  return true;
}

void oFreePunch::Set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("CardNo")){
      CardNo=it->getInt();
    }
    if (it->is("Type")){
      Type=it->getInt();
    }
    if (it->is("Time")){
      Time=it->getInt();
    }
    else if (it->is("Id")){
      Id=it->getInt();
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRaw());
    }
  }
}

bool oFreePunch::setCardNo(int cno, bool databaseUpdate) {
  if (cno != CardNo) {
    pRunner r1 = oe->getRunner(tRunnerId, 0);
    int oldControlId = tMatchControlId;
    //oe->punchIndex[itype].remove(CardNo); // Remove from index
    // Remove ourself from index
    oEvent::PunchIndexType &pi = oe->punchIndex[iHashType];
    oEvent::PunchConstIterator it = pi.find(CardNo);
    while (it != pi.end() && it->first == CardNo) {
      if (it->second == this) {
        pi.erase(it);
        break;
      }
      ++it;
    }
    oe->removeFromPunchHash(CardNo, Type, Time);
    rehashPunches(*oe, CardNo, 0);

    CardNo = cno;
    oe->insertIntoPunchHash(CardNo, Type, Time);

    rehashPunches(*oe, CardNo, this);
    pRunner r2 = oe->getRunner(tRunnerId, 0);

    if (r1 && oldControlId > 0)
      r1->markClassChanged(oldControlId);
    if (r2 && iHashType > 0)
      r2->markClassChanged(tMatchControlId);

    if (!databaseUpdate)
      updateChanged();

    return true;
  }
  return false;
}

void oFreePunch::remove()
{
  if (oe)
    oe->removeFreePunch(Id);
}

bool oFreePunch::canRemove() const
{
  return true;
}

Table *oEvent::getPunchesTB()//Table mode
{
  if (tables.count("punch") == 0) {
    Table *table=new Table(this, 20, L"Stämplingar", "punches");
    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 150, false);
    table->addColumn("Bricka", 70, true);
    table->addColumn("Kontroll", 70, true);
    table->addColumn("Tid", 70, false);
    table->addColumn("Löpare", 170, false);
    table->addColumn("Lag", 170, false);
    table->addColumn("Klass", 170, false);
    tables["punch"] = table;
    table->addOwnership();
  }

  tables["punch"]->update();
  return tables["punch"];
}

void oEvent::generatePunchTableData(Table &table, oFreePunch *addPunch)
{
  if (addPunch) {
    addPunch->addTableRow(table);
    return;
  }

  synchronizeList({ oListId::oLPunchId, oListId::oLRunnerId });
  oFreePunchList::iterator it;
  table.reserve(punches.size());
  for (it = punches.begin(); it != punches.end(); ++it){
    if (!it->isRemoved() && !it->isHiredCard()){
      it->addTableRow(table);
    }
  }
}

void oFreePunch::addTableRow(Table &table) const {
  oFreePunch &it = *pFreePunch(this);
  table.addRow(getId(), &it);
  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false, cellEdit);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false, cellEdit);
  table.set(row++, it, TID_CARD, itow(getCardNo()), true, cellEdit);
  table.set(row++, it, TID_CONTROL, getType(), true, cellEdit);
  table.set(row++, it, TID_TIME, getTime(), true, cellEdit);
  pRunner r = 0;
  if (CardNo > 0)
    r = oe->getRunnerByCardNo(CardNo, Time, oEvent::CardLookupProperty::Any);

  table.set(row++, it, TID_RUNNER, r ? r->getName() : L"?", false, cellEdit);

  if (r && r->getTeam())
    table.set(row++, it, TID_TEAM, r->getTeam()->getName(), false, cellEdit);
  else
    table.set(row++, it, TID_TEAM, L"", false, cellEdit);

  table.set(row++, it, TID_CLASSNAME, r ? r->getClass(true) : L"", false, cellEdit);
}

bool oFreePunch::inputData(int id, const wstring &input,
                           int inputId, wstring &output, bool noUpdate)
{
  synchronize(false);
  switch(id) {
    case TID_CARD:
      setCardNo(_wtoi(input.c_str()));
      synchronize(true);
      output = itow(CardNo);
      break;

    case TID_TIME:
      setTime(input);
      synchronize(true);
      output = getTime();
      break;

    case TID_CONTROL:
      setType(input);
      synchronize(true);
      output = getType();
  }
  return false;
}

void oFreePunch::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
}

void oFreePunch::setTimeInt(int t, bool databaseUpdate) {
  if (t != Time) {
    oe->removeFromPunchHash(CardNo, Type, Time);
    Time = t;
    oe->insertIntoPunchHash(CardNo, Type, Time);
    rehashPunches(*oe, CardNo, 0);
    if (!databaseUpdate)
      updateChanged();
  }
}

bool oFreePunch::setType(const wstring &t, bool databaseUpdate) {
  int type = _wtoi(t.c_str());
  int ttype = 0;
  if (type>0 && type<10000)
    ttype = type;
  else {
    if (t == lang.tl("Check"))
      ttype = oPunch::PunchCheck;
    else if (t == lang.tl("Mål"))
      ttype = oPunch::PunchFinish;
    if (t == lang.tl("Start"))
      ttype = oPunch::PunchStart;
  }
  if (ttype > 0 && ttype != Type) {
    oe->removeFromPunchHash(CardNo, Type, Time);
    Type = ttype;
    oe->insertIntoPunchHash(CardNo, Type, Time);
    int oldControlId = tMatchControlId;
    rehashPunches(*oe, CardNo, 0);

    pRunner r = oe->getRunner(tRunnerId, 0);

    if (r) {
      r->markClassChanged(tMatchControlId);
      if (oldControlId > 0)
        r->markClassChanged(oldControlId);
    }

    if (!databaseUpdate)
      updateChanged();

    return true;
  }
  return false;
}

void oFreePunch::rehashPunches(oEvent &oe, int cardNo, pFreePunch newPunch) {
  if (disableHashing || (cardNo == 0 && !oe.punchIndex.empty()) || oe.punches.empty())
    return;
  vector<pFreePunch> fp;

  if (oe.punchIndex.empty()) {
    // Rehash all punches. Ignore cardNo and newPunch (will be included automatically)
    fp.reserve(oe.punches.size());
    for (oFreePunchList::iterator pit = oe.punches.begin(); pit != oe.punches.end(); ++pit) {
      if (pit->isRemoved() || pit->isHiredCard())
        continue;
      fp.push_back(&(*pit));
    }

    sort(fp.begin(), fp.end(), FreePunchComp());
    disableHashing = true;
    try {
      for (size_t j = 0; j < fp.size(); j++) {
        pFreePunch punch = fp[j];
        punch->iHashType = oe.getControlIdFromPunch(punch->Time, punch->Type, punch->CardNo, true,
                                                    *punch);

        oEvent::PunchIndexType &card2Punch = oe.punchIndex[punch->iHashType];
        card2Punch.insert(make_pair(punch->CardNo, punch));
      }
    }
    catch(...) {
      disableHashing = false;
      throw;
    }
    disableHashing = false;
    return;
  }

  map<int, oEvent::PunchIndexType>::iterator it;
  fp.reserve(oe.punchIndex.size() + 1);

  // Get all punches for the specified card.
  for(it = oe.punchIndex.begin(); it != oe.punchIndex.end(); ++it) {
    pair<oEvent::PunchConstIterator, oEvent::PunchConstIterator> res = it->second.equal_range(cardNo);
    oEvent::PunchConstIterator pIter = res.first;
    while(pIter != res.second) {
      pFreePunch punch = pIter->second;
      assert(punch && punch->CardNo == cardNo);
      if (!punch->isRemoved()) {
        fp.push_back(punch);
      }
      ++pIter;
    }
    it->second.erase(res.first, res.second);
  }

  if (newPunch && !newPunch->isHiredCard())
    fp.push_back(newPunch);

  sort(fp.begin(), fp.end(), FreePunchComp());
  for (size_t j = 0; j < fp.size(); j++) {
    if (j>0 && fp[j-1] == fp[j])
      continue; //Skip duplicates
    pFreePunch punch = fp[j];
    punch->iHashType = oe.getControlIdFromPunch(punch->Time, punch->Type, cardNo, true, *punch);
    oEvent::PunchIndexType &card2Punch = oe.punchIndex[punch->iHashType];
    card2Punch.insert(make_pair(punch->CardNo, punch));
  }
}

//const int legHashConstant = 100000;
int oFreePunch::getControlHash(int courseControlId, int race) {
  int newId = courseControlId + race*100000000;
  return newId;
}

int oFreePunch::getControlIdFromHash(int hash, bool courseControlId) {
  int r = (hash%100000000);
  if (courseControlId)
    return r;
  else
    return oControl::getIdIndexFromCourseControlId(r).first;
}

int oEvent::getControlIdFromPunch(int time, int type, int card,
                                  bool markClassChanged, oFreePunch &punch) {
  pRunner r = getRunnerByCardNo(card, time, oEvent::CardLookupProperty::Any);
  punch.tRunnerId = -1;
  punch.tMatchControlId = type;
  if (r!=0) {
    punch.tRunnerId = r->Id;
  }
  int race = 0;
  if (type!=oPunch::PunchFinish) {
    pCourse c = r ? r->getCourse(false): 0;

    if (c!=0) {
      race = r->getRaceNo();
      for (int k=0; k<c->nControls; k++) {
        pControl ctrl=c->getControl(k);
        if (ctrl && ctrl->hasNumber(type)) {
          int courseControlId = c->getCourseControlId(k);
          pFreePunch p = getPunch(r->getId(), courseControlId, card);
          if (!p || (p && abs(p->Time-time)<60)) {
            ctrl->tHasFreePunchLabel = true;
            punch.tMatchControlId = ctrl->getId();
            punch.tIndex = k;
            int newId = oFreePunch::getControlHash(courseControlId, race);
            if (newId != punch.iHashType && markClassChanged && r) {
              r->markClassChanged(ctrl->getId());
              if (punch.iHashType > 0)
                r->markClassChanged(oFreePunch::getControlIdFromHash(punch.iHashType, false));
            }

            //Code controlId and runner race number into code
            return newId;
          }
        }
      }
    }
  }

  int newId = oFreePunch::getControlHash(type, 0);

  if (newId != punch.iHashType && markClassChanged && r) {
    r->markClassChanged(type);
    if (punch.iHashType > 0)
      r->markClassChanged(oFreePunch::getControlIdFromHash(punch.iHashType, false));
  }

  return newId;
}

void oEvent::getFreeControls(set<int> &controlId) const
{
  controlId.clear();
  for (map<int, PunchIndexType >::const_iterator it = punchIndex.begin(); it != punchIndex.end(); ++it) {
    int id = oFreePunch::getControlIdFromHash(it->first, false);
    controlId.insert(id);
  }
}

//set< pair<int,int> > readPunchHash;

void oEvent::insertIntoPunchHash(int card, int code, int time) {
  if (time > 0) {
    int p1 = time * 4096 + code;
    int p2 = card;
    readPunchHash.insert(make_pair(p1, p2));
  }
}

void oEvent::removeFromPunchHash(int card, int code, int time) {
  int p1 = time * 4096 + code;
  int p2 = card;
  readPunchHash.erase(make_pair(p1, p2));
}

bool oEvent::isInPunchHash(int card, int code, int time) {
  int p1 = time * 4096 + code;
  int p2 = card;
  return readPunchHash.count(make_pair(p1, p2)) > 0;
}


pFreePunch oEvent::addFreePunch(int time, int type, int card, bool updateStartFinish) {
  if (time > 0 && isInPunchHash(card, type, time))
    return 0;
  oFreePunch ofp(this, card, time, type);

  punches.push_back(ofp);
  pFreePunch fp=&punches.back();
  oFreePunch::rehashPunches(*this, card, fp);
  insertIntoPunchHash(card, type, time);

  if (fp->getTiedRunner() && oe->isClient() && oe->getPropertyInt("UseDirectSocket", true)!=0) {
    SocketPunchInfo pi;
    pi.runnerId = fp->getTiedRunner()->getId();
    pi.time = fp->getAdjustedTime();
    pi.status = fp->getTiedRunner()->getStatus();
    if (fp->getTypeCode() > 10)
      pi.iHashType = fp->getIHashType();
    else
      pi.iHashType = fp->getTypeCode();

    getDirectSocket().sendPunch(pi);
  }

  fp->updateChanged();
  fp->synchronize();
  pRunner tr = fp->getTiedRunner();

  if (tr != nullptr) {
    // Update start/finish time
    if (updateStartFinish) {
      int startType = oPunch::PunchStart;
      int finishType = oPunch::PunchFinish;
      pCourse pCrs = tr->getCourse(false);
      if (pCrs) {
        startType = pCrs->getStartPunchType();
        finishType = pCrs->getFinishPunchType();
      }

      if (type == startType || type == finishType) {
        if (tr->getStatus() == StatusUnknown && time > 0) {
          tr->synchronize();
          if (type == startType) {
            if (tr->getClassRef(false) && !tr->getClassRef(true)->ignoreStartPunch())
              tr->setStartTime(time, true, false);
          }
          else
            tr->setFinishTime(time);

          // Direct result
          if (type == finishType && tr->getClassRef(false) && tr->getClassRef(true)->hasDirectResult()) {
            if (tr->getCourse(false) == 0 && tr->getCard() == 0) {
              tr->setStatus(StatusOK, true, false, true);
            }
            else if (tr->getCourse(false) != 0 && tr->getCard() == 0) {
              pCard card = allocateCard(tr);
              card->setupFromRadioPunches(*tr);
              vector<int> mp;
              card->synchronize();
              tr->addPunches(card, mp);
            }
          }

          tr->synchronize(true);
        }
      }
    }

    pushDirectChange();
  }
  return fp;
}

pFreePunch oEvent::addFreePunch(oFreePunch &fp) {
  insertIntoPunchHash(fp.CardNo, fp.Type, fp.Time);
  punches.push_back(fp);
  pFreePunch fpz=&punches.back();
  oFreePunch::rehashPunches(*this, fp.CardNo, fpz);

  if (!fpz->existInDB() && HasDBConnection) {
    fpz->changed = true;
    fpz->synchronize();
  }
  return fpz;
}

void oEvent::removeFreePunch(int Id) {
  oFreePunchList::iterator it;

  for (it=punches.begin(); it != punches.end(); ++it) {
    if (it->Id==Id) {
      pRunner r = getRunner(it->tRunnerId, 0);
      if (r && r->Class) {
        r->markClassChanged(it->tMatchControlId);
        classChanged(r->Class, true);
      }
      pFreePunch fp = &*it;
      if (HasDBConnection)
        msRemove(fp);
      //punchIndex[it->itype].remove(it->CardNo);
      PunchIndexType &ix = punchIndex[it->iHashType];
      pair<PunchConstIterator, PunchConstIterator> res = ix.equal_range(it->CardNo);
      while (res.first != res.second) {
        if (res.first->second == fp) {
          PunchConstIterator rm = res.first;
          ++res.first;
          ix.erase(rm);
        }
        else
          ++res.first;
      }

      int cardNo = fp->CardNo;
      removeFromPunchHash(cardNo, fp->Type, fp->Time);
      punches.erase(it);
      oFreePunch::rehashPunches(*this, cardNo, 0);
      dataRevision++;
      return;
    }
  }
}

pFreePunch oEvent::getPunch(int Id) const
{
  oFreePunchList::const_iterator it;

  for (it=punches.begin(); it != punches.end(); ++it) {
    if (it->Id==Id) {
      if (it->isRemoved())
        return 0;
      return const_cast<pFreePunch>(&*it);
    }
  }
  return 0;
}

pFreePunch oEvent::getPunch(int runnerId, int courseControlId, int card) const
{
  //Lazy setup
  oFreePunch::rehashPunches(*oe, 0, 0);

  pRunner r = oe->getRunner(runnerId, 0);
  int runnerRace = r ? r->getRaceNo() : 0;
  map<int, oEvent::PunchIndexType>::const_iterator it1;

  int itype = oFreePunch::getControlHash(courseControlId, runnerRace);

  it1=punchIndex.find(itype);

  if (it1!=punchIndex.end()) {
    const oEvent::PunchIndexType &cIndex = it1->second;
    pair<oEvent::PunchConstIterator, oEvent::PunchConstIterator> res = cIndex.equal_range(card);
    oEvent::PunchConstIterator pIter = res.first;
    while(pIter != res.second) {
      pFreePunch punch = pIter->second;
      if (!punch->isRemoved()) {
        assert(punch && punch->CardNo == card);
        if (punch->tRunnerId == runnerId || runnerId == 0)
          return punch;
        ++pIter;
      }
    }
  }

  map<pair<int, int>, oFreePunch>::const_iterator res = advanceInformationPunches.find(make_pair(itype, card));
  if (res != advanceInformationPunches.end())
    return (pFreePunch)&res->second;

  return 0;
}

void oEvent::getPunchesForRunner(int runnerId, bool doSort, vector<pFreePunch> &runnerPunches) const {
  runnerPunches.clear();
  pRunner r = getRunner(runnerId, 0);
  if (r == 0)
    return;
  /*
  // Get times for when other runners used this card
  vector< pair<int, int> > times;
  int refCno = r->getCardNo();

  for (auto it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->Id == runnerId)
      continue;
    if (it->Card && it->getCardNo() == refCno) {
      pair<int, int> t = it->Card->getTimeRange();
      if (it->getStartTime() > 0)
        t.first = min(it->getStartTime(), t.first);

      if (it->getFinishTime() > 0)
        t.second = max(it->getFinishTime(), t.second);

      times.push_back(t);
    }
  }

  for (oFreePunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    if (it->CardNo == refCno) {
      if (it->isRemoved() || it->isHiredCard())
        continue;

      bool other = false;
      int t = it->Time;
      for (size_t k = 0; k<times.size(); k++) {
        if (t >= times[k].first && t <= times[k].second)
          other = true;
      }

      if (!other)
        runnerPunches.push_back(pFreePunch(&*it));
    }
  }
  */

  //Lazy setup
  oFreePunch::rehashPunches(*oe, 0, 0);

  int card = r->getCardNo();
  if (card == 0)
    return;

  for (auto &it1 :punchIndex) {
    const oEvent::PunchIndexType &cIndex = it1.second;
    pair<oEvent::PunchConstIterator, oEvent::PunchConstIterator> res = cIndex.equal_range(card);
    oEvent::PunchConstIterator pIter = res.first;
    while (pIter != res.second) {
      pFreePunch punch = pIter->second;
      if (!punch->isRemoved()) {
        assert(punch && punch->CardNo == card);
        if (punch->tRunnerId == runnerId || runnerId == 0)
          runnerPunches.push_back(punch);
        ++pIter;
      }
    }
  }
  
  if (doSort) {
    sort(runnerPunches.begin(), runnerPunches.end(), [](const oPunch *p1, const oPunch *p2)->bool {return p1->Time < p2->Time; });
  }
}


bool oEvent::advancePunchInformation(const vector<gdioutput *> &gdi, vector<SocketPunchInfo> &pi,
                                     bool fetchPunch, bool fetchFinish) {
  if (pi.empty())
    return false;

  bool m = false;
  for (size_t k = 0; k < pi.size(); k++) {
    pRunner r = getRunner(pi[k].runnerId, 0);
    if (!r)
      continue;
    if (pi[k].iHashType == oPunch::PunchFinish && fetchFinish) {
      if (r->getStatus() == StatusUnknown && r->getFinishTime() <= 0 && !r->isChanged()) {
        r->FinishTime = pi[k].time;
        r->tStatus = RunnerStatus(pi[k].status);
        r->status = RunnerStatus(pi[k].status); // Will be overwritten (do not set isChanged flag)
        if (r->Class) {
          r->markClassChanged(oPunch::PunchFinish);
          classChanged(r->Class, false);
        }
        m = true;
      }
    }
    else if (fetchPunch) {
      // controlId is already the encoded format including index and race
      if (getPunch(pi[k].runnerId, pi[k].iHashType, r->getCardNo()) == 0) {
        oFreePunch fp(this, 0, pi[k].time, pi[k].iHashType);
        fp.tRunnerId = pi[k].runnerId;
        fp.iHashType = pi[k].iHashType;
        fp.tIndex = 0;
        fp.tMatchControlId = oFreePunch::getControlIdFromHash(fp.iHashType, false);
        fp.changed = false;
		pair<int, int> hc(pi[k].iHashType, r->getCardNo());
        advanceInformationPunches.insert(make_pair(hc,fp));
        if (r->Class) {
          r->markClassChanged(oFreePunch::getControlIdFromHash(pi[k].iHashType, false));
          classChanged(r->Class, true);
        }
        m = true;
      }
    }
  }
  if (m) {
    dataRevision++;
    for (size_t k = 0; k<gdi.size(); k++) {
      if (gdi[k])
        gdi[k]->makeEvent("DataUpdate", "autosync", 0, 0, false);
    }
  }
  return m;
}

void oEvent::getLatestPunches(int firstTime, vector<const oFreePunch *> &punchesOut) const {
  for (map< pair<int, int>, oFreePunch>::const_iterator it = advanceInformationPunches.begin();
       it != advanceInformationPunches.end(); ++it) {
    int time = it->second.getModificationTime();
    if (time >= firstTime)
      punchesOut.push_back(&it->second);
  }

  for (oFreePunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    int time = it->getModificationTime();
    if (time >= firstTime)
      punchesOut.push_back(&*it);
  }
}

pRunner oFreePunch::getTiedRunner() const {
  return oe->getRunner(tRunnerId, 0);
}

void oFreePunch::changedObject() {
  pRunner r = getTiedRunner();
  if (r && tMatchControlId>0)
    r->markClassChanged(tMatchControlId);
}

bool oEvent::hasHiredCardData() {
  synchronizeList(oListId::oLPunchId);
  isHiredCard(0); 
  return !hiredCardHash.empty(); 
}

bool oEvent::isHiredCard(int cardNo) const {
  if (tHiredCardHashDataRevision != dataRevision) {
    hiredCardHash.clear();
    for (auto &p : punches) {
      if (!p.isRemoved() && p.isHiredCard())
        hiredCardHash.insert(p.getCardNo());
    }
    tHiredCardHashDataRevision = dataRevision;
  }
  return hiredCardHash.count(cardNo) > 0;
}

void oEvent::setHiredCard(int cardNo, bool flag) {
  if (cardNo <= 0)
    return;

  if (isHiredCard(cardNo) != flag) {
    if (flag) {
      addFreePunch(0, oPunch::HiredCard, cardNo, false);
      hiredCardHash.insert(cardNo);
      tHiredCardHashDataRevision = dataRevision;
    }
    else {
      hiredCardHash.erase(cardNo);
      for (auto it = punches.begin(); it != punches.end();) {
        if (!it->isRemoved() && it->isHiredCard() && it->CardNo == cardNo) {
          if (HasDBConnection)
            msRemove(&*it);

          auto toErase = it;
          ++it;
          punches.erase(toErase);
        }
        else {
          ++it;
        }
      }
      tHiredCardHashDataRevision = dataRevision;
    }
  }
}

vector<int> oEvent::getHiredCards() const {
  isHiredCard(0); // Update hash
  vector<int> r(hiredCardHash.begin(), hiredCardHash.end());
  return r;
}

void oEvent::clearHiredCards() {
  vector<int> toRemove;
  for (auto it = punches.begin(); it != punches.end();) {
    if (!it->isRemoved() && it->isHiredCard()) {
      if (HasDBConnection)
        msRemove(&*it);

      auto toErase = it;
      ++it;
      punches.erase(toErase);
    }
    else {
      ++it;
    }
  }
  hiredCardHash.clear();
}
