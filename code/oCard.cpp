/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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

// oCard.cpp: implementation of the oCard class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "oCard.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "table.h"
#include "Localizer.h"
#include "meos_util.h"

#include <algorithm>
#include "xmlparser.h"

#include "cardmanager.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oCard::oCard(oEvent *poe): oBase(poe)
{
  Id=oe->getFreeCardId();
  cardNo=0;
  readId=0;
  tOwner=0;
}

oCard::oCard(oEvent *poe, int id): oBase(poe)
{
  Id=id;
  cardNo=0;
  readId=0;
  tOwner=0;
  oe->qFreeCardId = max(id, oe->qFreeCardId);
}

oCard::~oCard()
{

}

bool oCard::write(xmlparser &xml, bool secondary) {
  if (Removed) return true;
  if (!secondary)
    xml.startTag("Card");
  else
    xml.startTag("Card2");

  xml.write("CardNo", cardNo);
  xml.write("Punches", getPunchString());
  xml.write("ReadId", int(readId));
  xml.write("Voltage", miliVolt);
  xml.write("BatteryDate", batteryDate);
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.endTag();

  return true;
}

void oCard::set(const xmlobject &xo) {
  xmlList xl;
  xo.getObjects(xl);
  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("CardNo")){
      cardNo = it->getInt();
    }
    else if (it->is("Voltage")) {
      miliVolt = it->getInt();
    }
    else if (it->is("BatteryDate")) {
      batteryDate = it->getInt();
    }
    else if (it->is("Punches")){
      importPunches(it->getRawStr());
    }
    else if (it->is("ReadId")){
      readId = it->getInt(); // Coded as signed int
    }
    else if (it->is("Id")){
      Id = it->getInt();
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRawStr());
    }
  }
}

pair<int, int> oCard::getCardHash() const {
  int a = cardNo;
  int b = readId;

  for (auto &p : punches) {
    a = a * 31 + p.punchTime * 997 + p.getTypeCode();
    b = b * 41 + p.punchTime * 97 + p.getTypeCode();
  }
  return make_pair(a, b);
}

void oCard::setCardNo(int c)
{
  if (cardNo!=c)
    updateChanged();

  cardNo=c;
}

const wstring &oCard::getCardNoString() const {
  return itow(cardNo);
}

void oCard::addPunch(int type, int time, int matchControlId, int unit, PunchOrigin origin) {
  oPunch p(oe);
  p.punchTime = time;
  p.type = type;
  p.tMatchControlId = matchControlId;
  p.isUsed =  matchControlId!=0;
  p.punchUnit = unit;
  if (origin == PunchOrigin::Original)
    p.origin = p.computeOrigin(oe->getZeroTimeNum() + time, type);
  else if (origin == PunchOrigin::Manual)
    p.origin = -1;

  if (punches.empty())
    punches.push_back(p);
  else {
    oPunch oldBack = punches.back();
    if (oldBack.isFinish()) { //Make sure finish is last.
      punches.pop_back();
      punches.push_back(p);
      punches.push_back(oldBack);
    }
    else
      punches.push_back(p);
  }
  updateChanged();
}

const string &oCard::getPunchString() const {
  punchString.clear();
  punchString.reserve(punches.size() * 16);
  for(auto &p : punches) {
    p.appendCodeString(punchString);
  }
  return punchString;
}

void oCard::importPunches(const string &s) {
  int startpos = 0;
  int endpos;

  endpos = s.find_first_of(';', startpos);
  punches.clear();

  while (endpos != string::npos) {
    oPunch p(oe);
    p.decodeString(s.c_str() + startpos);
    punches.push_back(p);
    startpos = endpos + 1;
    endpos = s.find_first_of(';', startpos);
  }
  return;
}

bool oCard::fillPunches(const oCard *card2, 
                        gdioutput& gdi, 
                        const string& name, 
                        const oCourse* crs) const {
  auto pList = getPunchList(card2);
  bool considerAllUsed = tOwner == nullptr;

  gdi.clearList(name);

  bool showStart = crs ? !crs->useFirstAsStart() : true;
  bool showFinish = crs ? !crs->useLastAsFinish() : true;

  bool hasStart = false;
  bool hasFinish = false;
  bool extra = false;
  int k = 0;
  int currentTimeAdjust = 0;
  pControl ctrl = 0;

  int matchPunch = 0;
  int punchRemain = 1;
  bool hasRogaining = false;
  if (crs) {
    ctrl = crs->getControl(matchPunch);
    hasRogaining = crs->hasRogaining();
  }
  if (ctrl)
    punchRemain = ctrl->getNumMulti();

  map<int, pair<int, const oPunch *>> rogainingIndex;

  if (crs) {
    for (int i = 0; i < pList.size(); i++) {
      if (pList[i]->tRogainingIndex >= 0) {
        rogainingIndex[pList[i]->tRogainingIndex] = make_pair(i, pList[i]);
      }
    }
  }

  for (int i = 0; i < pList.size(); i++) {
    auto it = pList[i];
    if (!hasStart && !it->isStart()) {
      if (it->isUsed && crs) {
        if (showStart)
          gdi.addItem(name, lang.tl("Start") + L"\t\u2013", -1);
        hasStart = true;
      }
    }

    if (crs && it->tRogainingIndex != -1)
      continue;

    {
      if (it->isStart())
        hasStart = true;
      else if (crs && it->isUsed && !it->isFinish() && !it->isCheck()) {
        while (ctrl && it->tMatchControlId != ctrl->getId()) {
          if (ctrl->isRogaining(hasRogaining)) {
            if (rogainingIndex.count(matchPunch) == 1)
              gdi.addItem(name, rogainingIndex[matchPunch].second->getString(considerAllUsed),
                rogainingIndex[matchPunch].first);
            else
              gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
          }
          else {
            while (0 < punchRemain--) {
              gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
            }
          }
          // Next control
          ctrl = crs ? crs->getControl(++matchPunch) : nullptr;
          punchRemain = ctrl ? ctrl->getNumMulti() : 1;
        }
      }


      if ((!crs || it->isUsed) || (showFinish && it->isFinish()) || (showStart && it->isStart())) {
        if (it->isFinish() && hasRogaining && crs) {
          while (ctrl) {
            if (ctrl->isRogaining(hasRogaining)) {
              // Check if we have reach finihs without adding rogaining punches
              while (ctrl && ctrl->isRogaining(hasRogaining)) {
                if (rogainingIndex.count(matchPunch) == 1)
                  gdi.addItem(name, rogainingIndex[matchPunch].second->getString(considerAllUsed),
                    rogainingIndex[matchPunch].first);
                else
                  gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
                ctrl = crs->getControl(++matchPunch);
              }
              punchRemain = ctrl ? ctrl->getNumMulti() : 1;
            }
            else {
              gdi.addItem(name, L"\u2013\t\u2013", -1);
              ctrl = crs->getControl(++matchPunch);
            }
          }
        }

        if (it->isFinish() && crs) { //Add missing punches before the finish
          while (ctrl) {
            gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
            ctrl = crs->getControl(++matchPunch);
          }
        }

        wstring tadj;
        if (currentTimeAdjust != it->tTimeAdjust.second) {
          int adj = it->tTimeAdjust.second - currentTimeAdjust;
          currentTimeAdjust = it->tTimeAdjust.second;
          if (adj > 0)
            tadj = L" +" + formatTime(adj);
          else
            tadj = L" -" + formatTime(-adj);
        }
        gdi.addItem(name, it->getString(considerAllUsed) + tadj, i);

        if (!(it->isFinish() || it->isStart())) {
          punchRemain--;
          if (punchRemain <= 0) {
            // Next contol
            ctrl = crs ? crs->getControl(++matchPunch) : 0;

            // Match rogaining here
            while (ctrl && ctrl->isRogaining(hasRogaining)) {
              if (rogainingIndex.count(matchPunch) == 1)
                gdi.addItem(name, rogainingIndex[matchPunch].second->getString(considerAllUsed),
                  rogainingIndex[matchPunch].first);
              else
                gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
              ctrl = crs->getControl(++matchPunch);
            }
            punchRemain = ctrl ? ctrl->getNumMulti() : 1;
          }
        }
      }
      else
        extra = true;
      k++;
      if (it->isFinish() && showFinish)
        hasFinish = true;
    }
  }

  if (!hasStart && showStart)
    gdi.addItem(name, lang.tl("Start") + L"\t\u2013", -1);

  if (!hasFinish && showFinish) {
    while (ctrl) {
      if (ctrl->isRogaining(hasRogaining)) {
        // Check if we have reach finihs without adding rogaining punches
        while (ctrl && ctrl->isRogaining(hasRogaining)) {
          if (rogainingIndex.count(matchPunch) == 1)
            gdi.addItem(name, rogainingIndex[matchPunch].second->getString(considerAllUsed),
              rogainingIndex[matchPunch].first);
          else
            gdi.addItem(name, L"\u2013\t\u2013", getUnmatchedPunchId(matchPunch));
          ctrl = crs->getControl(++matchPunch);
        }
        punchRemain = ctrl ? ctrl->getNumMulti() : 1;
      }
      else {
        gdi.addItem(name, L"-\t-", getUnmatchedPunchId(matchPunch));
        ctrl = crs->getControl(++matchPunch);
      }
    }
    gdi.addItem(name, lang.tl("Mål") + L"\t\u2013", -1);
  }

  if (extra) {
    //Show punches that are not used.
    k = 0;
    gdi.addItem(name, L"", -1);
    gdi.addItem(name, lang.tl("Extra stämplingar"), -1);
    for (int i = 0; i < pList.size(); i++) {
      if (!pList[i]->isUsed && !(pList[i]->isFinish() && showFinish) && !(pList[i]->isStart() && showStart))
        gdi.addItem(name, pList[i]->getString(considerAllUsed), i);
    }
  }
  return true;
}

/*
void oCard::insertPunchAfter(int pos, int type, int time) {
  if (pos==1023)
    return;

  oPunchList::iterator it;

  oPunch punch(oe);
  punch.punchTime=time;
  punch.type=type;

  int k=-1;
  for (it=punches.begin(); it != punches.end(); ++it) {
    if (k==pos) {
      updateChanged();
      punches.insert(it, punch);
      return;
    }
    k++;
  }

  updateChanged();
  //Insert last
  punches.push_back(punch);
}
*/
pPunch oCard::findPunch(int type, int time) const {
  for (auto &pp : punches) {
    if (type == pp.type && std::abs(pp.getTimeInt() - time) < 5 * timeConstSecond)
      return pPunch(&pp);
  }
  return nullptr;
}

void oCard::deletePunch(pCard card2, int globalIndex) {
  auto pList = getPunchList(card2);
  pPunch pp;

  if (globalIndex < pList.size() && pList[globalIndex])
    pp = pList[globalIndex];
  else
    throw std::exception("Punch not found");

  int type = pp->getTypeCode();
  int time = pp->getTimeInt();

  if (deletePunch(pp)){
    if (card2) {
      pp = card2->findPunch(type, time);
      if (pp) 
        card2->deletePunch(pp);
    }
  }
  else if (card2) {
    card2->deletePunch(pp);
  }
}

bool oCard::deletePunch(pPunch p) {
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (&*it == p) {
      punches.erase(it);
      updateChanged();
      return true;
    }
  }

  return false;
}

wstring oCard::getInfo() const {
  wchar_t bf[128];
  swprintf_s(bf, lang.tl("Löparbricka %d").c_str(), cardNo);
  return bf;
}

oPunch *oCard::getPunch(const pPunch punch) {
  int k=0;
  oPunchList::iterator it;

  for (it=punches.begin(); it != punches.end(); ++it) {
    if (&*it == punch) return &*it;
    k++;
  }
  return 0;
}

oPunch *oCard::getPunchByType(int type) const {
  for (auto &p : punches) {
    if (p.type == type)
      return pPunch(&p);
  }

  return nullptr;
}

oPunch *oCard::getPunchById(int courseControlId) const {
  pair<int, int> idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  oPunchList::const_iterator it;
  pPunch res = 0;
  for (it=punches.begin(); it != punches.end(); ++it) {
    if (it->tMatchControlId==idix.first) {
      res = pPunch(&*it);
      if (idix.second == 0)
        return res;
      --idix.second; // Not this match, decrease count
    }
  }
  return nullptr; // Punch not found
}


oPunch *oCard::getPunchByIndex(const oCard *card2, int ix) const {
  auto pList = getPunchList(card2);
  if (ix < pList.size())
    return pPunch(pList[ix]);
  return nullptr;
}

void oCard::setReadId(const DeviceCard &card) {
  updateChanged();
  readId=card.calculateHash();
}

bool oCard::isCardRead(const DeviceCard &card) const {
  if (readId==card.calculateHash())
    return true;
  else return false;
}

void oCard::getDeviceCard(const pCard secondaryCard, DeviceCard &card) const {
  card.clear(0);
  card.CardNumber = cardNo;
  card.convertedTime = ConvertedTimeStatus::Done;
  if (!secondaryCard) {
    for (auto &p : punches) {
      if (p.type >= 30)
        card.Punch[card.nPunch++].Code = p.type;
    }
  }
  else { 
    auto pl = merge(*secondaryCard);
    for (auto &p : pl) {
      if (p->type >= 30)
        card.Punch[card.nPunch++].Code = p->type;
    }
  }
}

pRunner oCard::getOwner() const {
  return tOwner && !tOwner->isRemoved() ? tOwner : 0;
}

bool oCard::setPunchTime(const pPunch punch, const wstring& time) {
  oPunch* op = getPunch(punch);
  if (!op) return false;

  DWORD ot = op->punchTime;
  op->setTime(time);

  if (ot != op->punchTime)
    updateChanged();

  return true;
}


pCard oEvent::getCard(int Id) const
{  // Do allow removed cards
  if (Id < int(Cards.size() / 2)) {
    for (oCardList::const_iterator it = Cards.begin(); it != Cards.end(); ++it){
      if (it->Id==Id)
        return const_cast<pCard>(&*it);
    }
  }
  else {
    for (oCardList::const_reverse_iterator it = Cards.rbegin(); it != Cards.rend(); ++it){
      if (it->Id==Id)
        return const_cast<pCard>(&*it);
    }
  }
  return 0;
}

void oEvent::getCards(vector<pCard>& cards, bool synchronize, bool onlyUnpaired) {
  if (synchronize)
    synchronizeList(oListId::oLCardId);

  cards.clear();
  cards.reserve(Cards.size());

  for (auto &c : Cards) {
    if (!c.isRemoved()) {
      if (onlyUnpaired && c.getOwner() != nullptr)
        continue;

      cards.push_back(&c);
    }
  }
}

pCard oEvent::addCard(const oCard &oc)
{
  if (oc.Id<=0)
    return 0;

  Cards.push_back(oc);
  Cards.back().tOwner = nullptr;
  Cards.back().addToEvent(this, &oc);
  qFreeCardId = max(oc.Id, qFreeCardId);
  return &Cards.back();
}

pCard oEvent::getCardByNumber(int cno) const
{
  oCardList::const_reverse_iterator it;
  pCard second = 0;
  for (it=Cards.rbegin(); it != Cards.rend(); ++it){
    if (!it->isRemoved() && it->cardNo==cno) {
      if (it->getOwner() == nullptr)
        return const_cast<pCard>(&*it);
      else if (second == 0)
        second = const_cast<pCard>(&*it);
    }
  }
  return second;
}

bool oEvent::isCardRead(const SICard &card) const
{
  oCardList::const_iterator it;

  for(it=Cards.begin(); it!=Cards.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->cardNo==card.CardNumber && it->isCardRead(card))
      return true;
  }

  return false;
}

const shared_ptr<Table> &oCard::getTable(oEvent *oe) {
  if (!oe->hasTable("cards")) {
    auto table = make_shared<Table>(oe, 20, L"Brickor", "cards");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Bricka", 120, true);
    table->addColumn("Deltagare", 200, false);
    table->addColumn("Spänning", 70, false);
    table->addColumn("Batteridatum", 100, false);

    table->addColumn("Starttid", 70, false);
    table->addColumn("Måltid", 70, false);
    table->addColumn("Stämplingar", 70, true);

    table->setTableProp(Table::CAN_DELETE);
    oe->setTable("cards", table);
  }
  
  return oe->getTable("cards");
}

void oEvent::generateCardTableData(Table &table, oCard *addCard)
{
  if (addCard) {
    addCard->addTableRow(table);
    return;
  }

  oCardList::iterator it;
  synchronizeList({ oListId::oLCardId, oListId::oLRunnerId });

  for (it=Cards.begin(); it!=Cards.end(); ++it) {
    if (!it->isRemoved()) {
      it->addTableRow(table);
    }
  }
}

void oCard::addTableRow(Table &table) const {

  wstring runner(lang.tl("Oparad"));
  if (getOwner())
    runner = tOwner->getNameAndRace(true);

  oCard &it = *pCard(this);
  table.addRow(getId(), &it);

  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, it, TID_CARD, getCardNoString(), true, cellAction);

  table.set(row++, it, TID_RUNNER, runner, true, cellAction);

  table.set(row++, it, TID_VOLTAGE, getCardVoltage(), false, cellAction);

  table.set(row++, it, TID_BATTERYDATE, getBatteryDate(), false, cellAction);


  oPunch *p=getPunchByType(oPunch::PunchStart);
  wstring time;
  if (p)
    time = p->getTime(false, SubSecond::Auto);
  else
    time = makeDash(L"-");
  table.set(row++, it, TID_START, time, false, cellEdit);

  p = getPunchByType(oPunch::PunchFinish);
  if (p)
    time = p->getTime(false, SubSecond::Auto);
  else
    time = makeDash(L"-");
  
  table.set(row++, it, TID_FINISH, time, false, cellEdit);

  table.set(row++, it, TID_COURSE, itow(getNumPunches()), false, cellEdit);
}

oDataContainer &oCard::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  throw std::exception("Unsupported");
}

int oCard::getSplitTime(int startTime, const oPunch *punch) const {
  return getSplitTime(punches, startTime, punch);
}

int oCard::getSplitTime(const oPunchList & punches, int startTime, const oPunch * punch) {
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (&*it == punch) {
      int t = it->getAdjustedTime();
      if (t<=0)
        return -1;

      if (startTime > 0)
        return t - startTime;
      else
        return -1;
    }
    else if (it->isUsed)
      startTime = it->getAdjustedTime();
  }
  return -1;
}

wstring oCard::getRogainingSplit(int ix, int startTime) const {
  oPunchList::const_iterator it;
  for (it = punches.begin(); it != punches.end(); ++it) {
    int t = it->getAdjustedTime();
    if (0 == ix--) {
      if (t > 0 && t > startTime)
        return formatTime(t - startTime);
    }
    if (it->isUsed)
      startTime = t;
  }
 return makeDash(L"-");
}

void oCard::remove() {
  if (oe)
    oe->removeCard(Id);
}

bool oCard::canRemove() const {
  return getOwner() == 0;
}

pair<int, int> oCard::getTimeRange() const {
  pair<int, int> t(24*timeConstHour, 0);
  bool finishLock = false;

  for(auto it = punches.begin(); it != punches.end(); ++it) {
    if (it->hasTime()) {
      int pt = it->getTimeInt();
      t.first = min(t.first, pt);
      if (it->isFinish()) {
        t.second = it->getTimeInt(); // Do not allow times after finish
        finishLock = true;
      }
      if (!finishLock && !it->isCheck() && !it->isStart())
        t.second = max(t.second, pt);
    }
  }
  return t;
}

void oCard::getPunches(vector<pPunch> &punchesOut) const {
  punchesOut.clear();
  punchesOut.reserve(punches.size());
  for(oPunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    punchesOut.push_back(pPunch(&*it));
  }
}

void oCard::setupFromRadioPunches(oRunner &r) {
  oe->synchronizeList(oListId::oLPunchId);
  vector<pFreePunch> p;
  oe->getPunchesForRunner(r.getId(), true, p);

  for (size_t k = 0; k < p.size(); k++)
    addPunch(p[k]->type, p[k]->punchTime, 0, p[k]->punchUnit, p[k]->isOriginal() ? oCard::PunchOrigin::Original : oCard::PunchOrigin::Manual);
  
  cardNo = r.getCardNo();
  readId = ConstructedFromPunches; //Indicates
}

void oCard::changedObject() {
  if (tOwner)
    tOwner->changedObject();

  oe->sqlCards.changed = true;
}

int oCard::getNumControlPunches(int startPunchType, int finishPunchType) const {
  int count = 0;
  for(oPunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    if (it->isFinish(finishPunchType) || it->isCheck() || it->isStart(startPunchType)) {
      continue;
    }
    count++;
  }
  return count;
}

void oCard::adaptTimes(int startTime) {
  int st = -1;
  oPunchList::iterator it = punches.begin();
  while (it != punches.end()) {
    if (it->hasTime()) {
      st = it->getTimeInt();
      break;
    }
    ++it;
  }
  
  if (st == -1)
    return;

  const int h24 = 24 * timeConstHour;
  int offset = st / h24;
  if (offset > 0) {
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime() && it->getTimeInt() < offset * h24)
        return; // Inconsistent, do nothing
    }
         
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime())
        it->punchTime -= offset * h24;
    }
    updateChanged();
  }

  if (startTime >= h24) {
    offset = startTime / h24;
    for (it = punches.begin(); it != punches.end(); ++it) {
      if (it->hasTime())
        it->punchTime += offset * h24;
    }
    updateChanged();
  }
}

wstring oCard::getCardVoltage() const {
  return getCardVoltage(miliVolt);
}

wstring oCard::getCardVoltage(int miliVolt) {
  if (miliVolt <= 10)
    return L"";
  int vi = miliVolt / 1000;
  int vd = (miliVolt % 1000) / 10;

  wchar_t bf[64];
  swprintf_s(bf, L"%d.%02d V", vi, vd);
  return bf;
}

oCard::BatteryStatus oCard::isCriticalCardVoltage() const {
  return isCriticalCardVoltage(miliVolt);
}

oCard::BatteryStatus oCard::isCriticalCardVoltage(int miliVolt)  {
  if (miliVolt > 10 && miliVolt < 2445)
    return BatteryStatus::Bad;
  else if (miliVolt > 10 && miliVolt <= 2710)
    return BatteryStatus::Warning;

  return BatteryStatus::OK;
}

wstring oCard::getBatteryDate() const {
  if (batteryDate > 0)
    return formatDate(batteryDate, false);
  return _EmptyWString;
}

oCard::PunchOrigin oCard::isOriginalCard() const {
  bool isUnknown = false;
  for (auto& p : punches) {
    if (p.origin == 0) {
      isUnknown = true;
    }
    else {
      if (!p.isOriginal())
        return PunchOrigin::Manual;
    }
  }
  if (isUnknown)
    return PunchOrigin::Unknown;
  else
    return PunchOrigin::Original;
}

int oCard::getStartPunchCode() const {
  auto it = punches.begin();
  if (it != punches.end()) {
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
      return it->getPunchUnit();
    ++it;
    if (it != punches.end()) {
      if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
        return it->getPunchUnit();
    }
  }
  return 0;
}

int oCard::getStartTime(int ptype) const {
  auto it = punches.begin();
  if (ptype == oPunch::SpecialPunch::PunchStart) {
    if (it != punches.end()) {
      if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
        return it->getTimeInt();
      ++it;
      if (it != punches.end()) {
        if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
          return it->getTimeInt();
      }
    }
  }
  else {
    for (auto &p : punches) {
      if (p.getTypeCode() == ptype)
        return p.getTimeInt();
    }
  }
  return -1;
}


int oCard::getFinishPunchCode() const {
  auto it = punches.rbegin();
  if (it != punches.rend()) {
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchFinish)
      return it->getPunchUnit();
  }
  return 0;
}

pair<int, pControl> oCard::getWrongPunch(const oCourse& crs, const oControl& ctrl) {
  int ix = 0;
  map<int, pPunch> indexToWrongPunches;
  vector<int> crsToPunch(crs.getNumControls(), -1);
  vector<pPunch> controlPunches;
  for (oPunch &p : punches) {
    if (p.isStart() || p.isFinish() || p.isCheck())
      continue;

    if (!p.isUsedInCourse())
      indexToWrongPunches[ix] = &p;
    else if (p.tIndex < crsToPunch.size())
      crsToPunch[p.tIndex] = ix;

    controlPunches.push_back(&p);
    ++ix;
  }

  pPunch wrongPunch = nullptr;

  int seedPunchIx = -1;
  for (int j = 0; j < crs.getNumControls(); j++) {
    if (crs.getControl(j) == &ctrl) {
      if (seedPunchIx + 1 < controlPunches.size() && !controlPunches[seedPunchIx + 1]->isUsedInCourse()) {
        wrongPunch = controlPunches[seedPunchIx + 1];
        break;
      }
    }
    else {
      if (crsToPunch[j] >= 0)
        seedPunchIx = crsToPunch[j];
    }
  }

  if (wrongPunch != nullptr) {
    for (auto& ctrl : oe->Controls) {
      if (ctrl.isRemoved())
        continue;
      if (ctrl.getNNumbers() > 0 && ctrl.hasNumber(wrongPunch->getTypeCode()))
        return make_pair(wrongPunch->getTypeCode(), &ctrl);
    }

    return make_pair(wrongPunch->getTypeCode(), nullptr);
  }

  return make_pair(0, nullptr);
}

bool oCard::unexpectedOrder(int startTime) const {
  int lastTime = startTime;
  for (auto &p : punches) {
    if (p.isCheck() || p.isStart() || p.getTimeInt() <= 0)
      continue;
   
    if (p.getTypeCode() >= 30 && !p.isUsedInCourse())
      continue;

    if (p.getTimeInt() < startTime)
      return true;

    startTime = p.getTimeInt();
  }

  return false;
}

vector<pPunch> oCard::merge(const oCard &secondaryCard) const {
  vector<pPunch> out;
  out.reserve(punches.size() + 8);
  auto &sPunches = secondaryCard.punches;
  auto mainIt = punches.begin();
  auto secondaryIt = sPunches.begin();

  constexpr int timeLimit = timeConstSecond * 10;

  while (mainIt != punches.end() && secondaryIt != sPunches.end()) {
    if (mainIt->getTypeCode() == secondaryIt->getTypeCode() && 
         std::abs(mainIt->getTimeInt() - secondaryIt->getTimeInt()) < timeLimit) {
      out.push_back(pPunch(&(*mainIt)));
      ++mainIt;
      ++secondaryIt;
    }
    else if (mainIt->getTimeInt() < secondaryIt->getTimeInt()) {
      out.push_back(pPunch(&(*mainIt)));
      ++mainIt;
    }
    else {
      out.push_back(pPunch(&(*secondaryIt)));
      ++secondaryIt;
    }
  }

  while (mainIt != punches.end()) {
    out.push_back(pPunch(&(*mainIt)));
    ++mainIt;
  }

  while (secondaryIt != sPunches.end()) {
    out.push_back(pPunch(&(*secondaryIt)));
    ++secondaryIt;
  }

  return out;
}

void oCard::constructUnitedPL(const oCard &secondaryCard, oPunchList &pl) const {
  pl.clear();
  auto united = merge(secondaryCard);
  for (auto &u : united) 
    pl.push_back(*u);
}

vector<pPunch> oCard::getPunchList(oCard *secondaryCard) {
  vector<pPunch> out;
  if (!secondaryCard) {
    out.reserve(punches.size());
    for (auto &p : punches)
      out.push_back(&p);
  }
  else {
    out = merge(*secondaryCard);
  }

  return out;
}

vector<const oPunch*> oCard::getPunchList(const oCard *secondaryCard) const{
  vector<const oPunch *> out;
  if (!secondaryCard) {
    out.reserve(punches.size());
    for (auto &p : punches)
      out.push_back(&p);
  }
  else {
    auto outX = merge(*secondaryCard);
    out.reserve(outX.size());
    for (auto p : outX) 
      out.push_back(p);
  }

  return out;
}


void oCard::insert(const oPunch &position, const oPunch toInsert) {
  // Insert by exact position
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (&(*it) == &position) {
      punches.insert(it, toInsert);
      return;
    }
  }
  // Insert by time (from secondary card)
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (it->getTimeInt() > position.getTimeInt()) {
      if (it != punches.begin())
        --it;
      punches.insert(it, toInsert);
      return;
    }
  }

  // Insert last
  punches.push_back(toInsert); 
}
