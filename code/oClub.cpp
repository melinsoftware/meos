/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2018 Melin Software HB

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

// oClub.cpp: implementation of the oClub class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "oClub.h"
#include "meos_util.h"

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "RunnerDB.h"
#include <cassert>
#include "Table.h"
#include "localizer.h"
#include "pdfwriter.h"

#include "intkeymapimpl.hpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oClub::oClub(oEvent *poe): oBase(poe)
{
  getDI().initData();
  Id=oe->getFreeClubId();
}

oClub::oClub(oEvent *poe, int id): oBase(poe)
{
  getDI().initData();
  Id=id;
  if (id != cVacantId && id != cNoClubId)
    oe->qFreeClubId = max(id, oe->qFreeClubId);
}


oClub::~oClub(){
}

wstring oClub::getInfo() const {
  return L"Club: " + name;
}

bool oClub::write(xmlparser &xml)
{
  if (Removed) return true;

  xml.startTag("Club");
  xml.write("Id", Id);
  xml.write("Updated", Modified.getStamp());
  xml.write("Name", name);
  for (size_t k=0;k<altNames.size(); k++)
    xml.write("AltName", altNames[k]);

  getDI().write(xml);

  xml.endTag();

  return true;
}

void oClub::set(const xmlobject &xo)
{
  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id=it->getInt();
    }
    else if (it->is("Name")){
      internalSetName(it->getw());
    }
    else if (it->is("oData")){
      getDI().set(*it);
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRaw());
    }
    else if (it->is("AltName")) {
      altNames.push_back(it->getw());
    }
  }
}

void oClub::internalSetName(const wstring &n)
{
  if (name != n) {
    name = n;
    const wchar_t *bf = name.c_str();
    int len = name.length();
    int ix = -1;
    for (int k=0;k <= len-9; k++) {
      if (bf[k] == 'S') {
        if (wcscmp(bf+k, L"Skid o OK")==0) {
          ix = k;
          break;
        }
        if (wcscmp(bf+k, L"Skid o OL")==0) {
          ix = k;
          break;
        }
      }
    }
    if (ix >= 0) {
      tPrettyName = name;
      if (wcscmp(bf+ix, L"Skid o OK")==0)
        tPrettyName.replace(ix, 9, L"SOK", 3);
      else if (wcscmp(bf+ix, L"Skid o OL")==0)
        tPrettyName.replace(ix, 9, L"SOL", 3);
    }
  }
}

void oClub::setName(const wstring &n)
{
  if (n != name) {
    internalSetName(n);
    updateChanged();
  }
}

oDataContainer &oClub::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = 0;
  return *oe->oClubData;
}

pClub oEvent::getClub(int Id) const
{
  if (Id<=0)
    return 0;

  pClub value;
  if (clubIdIndex.lookup(Id, value))
    return value;
  return 0;
}

pClub oEvent::getClub(const wstring &pname) const
{
  oClubList::const_iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it)
    if (it->name==pname)
      return pClub(&*it);

  return 0;
}

pClub oEvent::getClubCreate(int Id, const wstring &createName)
{
  if (Id > 0) {
    //map<int, pClub>::iterator mit=clubIdIndex.find(Id);
    //if (mit!=clubIdIndex.end()) {
    pClub value;
    if (clubIdIndex.lookup(Id, value)) {
      if (!trim(createName).empty() && _wcsicmp(value->getName().c_str(), trim(createName).c_str())!=0)
        Id = 0; //Bad, used Id.
      if (trim(createName).empty() || Id>0)
        return value;
    }
  }
  if (createName.empty()) {
    int id = oe->getVacantClub(true);
    //Not found. Auto add...
    return getClubCreate(id, lang.tl("Klubblös"));
  }
  else	{
    oClubList::iterator it;
    wstring tname = trim(createName);

    //Maybe club exist under different ID
    for (it=Clubs.begin(); it != Clubs.end(); ++it)
      if (_wcsicmp(it->name.c_str(), tname.c_str())==0)
        return &*it;

    //Else, create club.
    return addClub(tname, Id);
  }
}

pClub oEvent::addClub(const wstring &pname, int createId) {
  if (createId>0) {
    pClub pc = getClub(createId);
    if (pc)
      return pc;
  }

  pClub dbClub = oe->useRunnerDb() ? oe->runnerDB->getClub(pname) : 0;

  if (dbClub) {
    if (dbClub->getName() != pname) {
      pClub pc = getClub(dbClub->getName());
      if (pc)
        return pc;
    }

    if (createId<=0)
      if (getClub(dbClub->Id))
        createId = getFreeClubId(); //We found a db club, but Id is taken.
      else
        createId = dbClub->Id;

    oClub c(this, createId);
    c = *dbClub;
    c.Id = createId;
    Clubs.push_back(c);
  }
  else {
    if (createId==0)
      createId = getFreeClubId();

    oClub c(this, createId);
    c.setName(pname);
    Clubs.push_back(c);
  }
  Clubs.back().synchronize();
  clubIdIndex[Clubs.back().Id]=&Clubs.back();
  return &Clubs.back();
}

pClub oEvent::addClub(const oClub &oc)
{
  if (clubIdIndex.count(oc.Id)!=0)
    return clubIdIndex[oc.Id];

  Clubs.push_back(oc);
  if (!oc.existInDB())
    Clubs.back().synchronize();

  clubIdIndex[Clubs.back().Id]=&Clubs.back();
  return &Clubs.back();
}

void oEvent::fillClubs(gdioutput &gdi, const string &id)
{
  vector< pair<wstring, size_t> > d;
  oe->fillClubs(d);
  gdi.addItem(id, d);
}


const vector< pair<wstring, size_t> > & oEvent::fillClubs(vector< pair<wstring, size_t> > &out)
{
  out.clear();
  //gdi.clearList(name);
  synchronizeList(oLClubId);
  Clubs.sort();

  oClubList::iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it){
    if (!it->Removed)
      out.push_back(make_pair(it->name, it->Id));
  }

  return out;
}

void oClub::buildTableCol(oEvent *oe, Table *table) {
   oe->oClubData->buildTableCol(table);
}

#define TB_CLUBS "clubs"
Table *oEvent::getClubsTB()//Table mode
{
  if (tables.count("club") == 0) {
    Table *table=new Table(this, 20, L"Klubbar", TB_CLUBS);

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);
    oe->oClubData->buildTableCol(table);

    table->addColumn("Deltagare", 70, true);
    table->addColumn("Avgift", 70, true);
    table->addColumn("Betalat", 70, true);

    tables["club"] = table;
    table->addOwnership();
  }

  tables["club"]->update();
  return tables["club"];

}

void oEvent::generateClubTableData(Table &table, oClub *addClub)
{
  oe->setupClubInfoData();
  if (addClub) {
    addClub->addTableRow(table);
    return;
  }
  synchronizeList(oLClubId);
  oClubList::iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it){
    if (!it->isRemoved()){
      it->addTableRow(table);
    }
  }
}

int oClub::getTableId() const {
  return Id;
}

void oClub::addTableRow(Table &table) const {
  table.addRow(getTableId(), pClass(this));

  bool dbClub = table.getInternalName() != TB_CLUBS;
  bool canEdit =  dbClub ? !oe->isClient() : true;

  pClub it = pClub(this);
  int row = 0;
  table.set(row++, *it, TID_ID, itow(getId()), false);
  table.set(row++, *it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, *it, TID_CLUB, getName(), canEdit);
  row = oe->oClubData->fillTableCol(*this, table, canEdit);

  if (!dbClub) {
    table.set(row++, *it, TID_NUM, itow(tNumRunners), false);
    table.set(row++, *it, TID_FEE, oe->formatCurrency(tFee), false);
    table.set(row++, *it, TID_PAID, oe->formatCurrency(tPaid), false);
  }
}

bool oClub::inputData(int id, const wstring &input,
                        int inputId, wstring &output, bool noUpdate)
{
  synchronize(false);

  if (id>1000) {
    return oe->oClubData->inputData(this, id, input, inputId, output, noUpdate);
  }

  switch(id) {
    case TID_CLUB:
      setName(input);
      synchronize();
      output = getName();
      return true;
    break;
  }

  return false;
}

void oClub::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oClubData->fillInput(oData, id, 0, out, selected);
    return;
  }
}

void oEvent::mergeClub(int clubIdPri, int clubIdSec)
{
  if (clubIdPri==clubIdSec)
    return;

  pClub pc = getClub(clubIdPri);
  if (!pc)
    return;

  // Update teams
  for (oTeamList::iterator it = Teams.begin(); it!=Teams.end(); ++it) {
    if (it->getClubId() == clubIdSec) {
      it->Club = pc;
      it->updateChanged();
      it->synchronize();
    }
  }

  // Update runners
  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    if (it->getClubId() == clubIdSec) {
      it->Club = pc;
      it->updateChanged();
      it->synchronize();
    }
  }
  oe->removeClub(clubIdSec);
}

void oEvent::getClubs(vector<pClub> &c, bool sort) {
  if (sort) {
    synchronizeList(oLClubId);
    Clubs.sort();
  }
  c.clear();
  c.reserve(Clubs.size());

  for (oClubList::iterator it = Clubs.begin(); it != Clubs.end(); ++it) {
    if (!it->isRemoved())
     c.push_back(&*it);
  }
}

void oEvent::viewClubMembers(gdioutput &gdi, int clubId)
{
  sortRunners(ClassStartTime);
  sortTeams(ClassStartTime, 0, true);

  gdi.fillDown();
  gdi.dropLine();
  int nr = 0;
  int nt = 0;
  // Update teams
  for (oTeamList::iterator it = Teams.begin(); it!=Teams.end(); ++it) {
    if (it->skip())
      continue;
    if (it->getClubId() == clubId) {
      if (nt==0)
        gdi.addString("", 1, "Lag(flera)");
      gdi.addStringUT(0, it->getName() + L", " + it->getClass(false) );
      nt++;
    }
  }

  gdi.dropLine();
  // Update runners
  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    if (it->skip())
      continue;
    if (it->getClubId() == clubId) {
      if (nr==0)
        gdi.addString("", 1, "Löpare:");
      gdi.addStringUT(0, it->getName() + L", " + it->getClass(true) );
      nr++;
    }
  }
}

void oClub::addInvoiceLine(gdioutput &gdi, const InvoiceLine &line, InvoiceData &data) const {
  int &yp = data.yp;
  for (size_t k = 0; k < line.xposAndString.size(); k++) {
    const pair<int, pair<bool, wstring> > &entry = line.xposAndString[k];
    int xp = entry.first;
    bool right = entry.second.first;
    const wstring &str = entry.second.second;
    if (right)
      gdi.addStringUT(yp, xp, normalText|textRight, str);
    else
      gdi.addStringUT(yp, xp, normalText, str);
  }

  data.total_fee_amount += line.fee;
  data.total_rent_amount += line.rent;
  data.total_paid_amount += line.paid;
  if (line.paid > 0)
    data.paidPerMode[line.payMode] += line.paid;
  yp += data.lh;
}

void oClub::addRunnerInvoiceLine(const pRunner r, bool inTeam,
                                 const map<int, wstring> &definedPayModes, 
                                 const InvoiceData &data, 
                                 list<InvoiceLine> &lines) const {
  int xs = data.xs;
  lines.push_back(InvoiceLine());
  InvoiceLine &line = lines.back();

  if (r->getTeam() && !inTeam && !r->isPatrolMember())
    line.addString(xs, r->getName() + L" ("  + r->getTeam()->getName() + L")");
  else
    line.addString(xs + (inTeam ? 10 : 0), r->getName());
  
  wstring ts;
  if (!inTeam)
    line.addString(xs+data.clsPos, r->getClass(true));

  if (r->getStatus() == StatusUnknown)
    ts = L"-";
  else if (!data.multiDay) {
    if (r->getStatus()==StatusOK) {
      ClassType type = oClassIndividual;
      cTeam t = r->getTeam();
      if (t && r->getClassRef(false))
        type = r->getClassRef(false)->getClassType();

      if (type == oClassIndividRelay || type == oClassRelay) {
        int leg = r->getLegNumber();
        if (t->getLegStatus(leg, false) == StatusOK)
          ts =  t->getLegPlaceS(leg, false)+ L" (" + r->getRunningTimeS() + L")";
        else
          ts =  t->getLegStatusS(leg, false)+ L" (" + r->getRunningTimeS() +L")";
      }
      else
        ts =  r->getPrintPlaceS(true)+ L" (" + r->getRunningTimeS() + L")";
    }
    else
      ts =  r->getStatusS();
  }
  else {
    if (r->getTotalStatus()==StatusOK) {
      ts =  r->getPrintTotalPlaceS(true) + L" (" + r->getTotalRunningTimeS() + L")";
    }
    else if (r->getTotalStatus()!=StatusNotCompetiting)
      ts =  r->getStatusS();
    else {
      ts = r->getInputStatusS();
    }
  }

  int fee = r->getDCI().getInt("Fee");
  int card = r->getDCI().getInt("CardFee");
  int paid = r->getDCI().getInt("Paid");
  int pm = r->getPaymentMode();
  
  /*string payMode = "";
  map<int, string>::const_iterator res = definedPayModes.find(pm);
  if (res != definedPayModes.end())
    payMode = ", " + res->second;
  */
  if (r->getClassRef(false) && r->getClassRef(false)->getClassStatus() == oClass::InvalidRefund) {
    fee = 0;
    card = 0;
  }

  if (fee>0)
    line.addString(xs+data.feePos, oe->formatCurrency(fee), true);
  if (card > 0)
    line.addString(xs+data.cardPos, oe->formatCurrency(card), true);
  if (paid>0)
    line.addString(xs+data.paidPos, oe->formatCurrency(paid), true);
  line.fee= fee;
  if (card > 0)
    line.rent = card;
  line.paid = paid;
  line.payMode = pm;
  line.addString(xs+data.resPos, ts);
}

void oClub::addTeamInvoiceLine(const pTeam t, const map<int, wstring> &definedPayModes, 
                               const InvoiceData &data, list<InvoiceLine> &lines) const {
  lines.push_back(InvoiceLine());
  InvoiceLine &line = lines.back();

  int xs = data.xs;

  int fee = t->getDCI().getInt("Fee");
  int paid = t->getDCI().getInt("Paid");

  if (fee <= 0)
    return;

  line.addString(xs, t->getName());
  line.addString(xs+data.clsPos, t->getClass(false));
  wstring ts;

  if (t->getStatus() == StatusUnknown)
    ts = L"-";
  else  {
    if (t->getStatus()==StatusOK) {
      ts =  t->getPrintPlaceS(true) + L" (" + t->getRunningTimeS() + L")";
    }
    else
      ts =  t->getStatusS();
  }


  if (t->getClassRef(false) && t->getClassRef(false)->getClassStatus() == oClass::InvalidRefund) {
    fee = 0;
  }

  line.addString(xs+data.feePos, oe->formatCurrency(fee), true);
  line.addString(xs+data.paidPos, oe->formatCurrency(paid), true);
  line.fee = fee;
  line.paid = paid;
  line.payMode = t->getPaymentMode();
  line.addString(xs+data.resPos, ts);

  for (int j = 0; j < t->getNumRunners(); j++) {
    pRunner r = t->getRunner(j);
    if (r && r->getClubId() == t->getClubId()) {
      addRunnerInvoiceLine(r, true, definedPayModes, data, lines);
    }
  }
}

void oClub::generateInvoice(gdioutput &gdi, int &toPay, int &hasPaid,
                            const map<int, wstring> &definedPayModes, 
                            map<int, int> &paidPerMode) {
  wstring account = oe->getDI().getString("Account");
  wstring pdate = oe->getDI().getDate("PaymentDue");
  int pdateI = oe->getDI().getInt("PaymentDue");
  wstring organizer = oe->getDI().getString("Organizer");
  int number = getDCI().getInt("InvoiceNo");
  if (number == 0) {
    assignInvoiceNumber(*oe, false);
    number = getDCI().getInt("InvoiceNo");
  }
  gdi.fillDown();

  if (account.empty())
    gdi.addString("", 0, "Varning: Inget kontonummer angivet (Se tävlingsinställningar).").setColor(colorRed);

  if (pdateI == 0)
    gdi.addString("", 0, "Varning: Inget sista betalningsdatum angivet (Se tävlingsinställningar).").setColor(colorRed);

  if (organizer.empty())
    gdi.addString("", 0, "Varning: Ingen organisatör/avsändare av fakturan angiven (Se tävlingsinställningar).").setColor(colorRed);

  vector<pRunner> runners;
  oe->getClubRunners(getId(), runners);
  vector<pTeam> teams;
  oe->getClubTeams(getId(), teams);

  toPay = 0;
  hasPaid = 0;

  if (runners.empty() && teams.empty())
    return;

  int ys = gdi.getCY();
  int lh = gdi.getLineHeight();

  InvoiceData data(lh);

  data.xs = 30;
  const int &xs = data.xs;
  data.adrPos = gdi.scaleLength(350);
  data.clsPos = gdi.scaleLength(270);

  data.feePos = gdi.scaleLength(390);
  data.cardPos = gdi.scaleLength(440);
  data.paidPos = gdi.scaleLength(490);
  data.resPos = gdi.scaleLength(550);

  gdi.addString("", ys, xs+data.adrPos, boldHuge, "FAKTURA");
  if (number>0)
    gdi.addStringUT(ys+lh*3, xs+data.adrPos, fontMedium, lang.tl("Faktura nr")+ L": " + itow(number));
  int &yp = data.yp;

  yp = ys+lh;
  wstring ostreet = oe->getDI().getString("Street");
  wstring oaddress = oe->getDI().getString("Address");
  wstring oco = oe->getDI().getString("CareOf");

  if (!organizer.empty())
    gdi.addStringUT(yp, xs, fontMedium, organizer), yp+=lh;
  if (!oco.empty())
    gdi.addStringUT(yp, xs, fontMedium, oco), yp+=lh;
  if (!ostreet.empty())
    gdi.addStringUT(yp, xs, fontMedium, ostreet), yp+=lh;
  if (!oaddress.empty())
    gdi.addStringUT(yp, xs, fontMedium, oaddress), yp+=lh;

  yp+=lh;

  gdi.addStringUT(yp, xs, fontLarge, oe->getName());
  gdi.addStringUT(yp+lh*2, xs, fontMedium, oe->getDate());

  wstring co =  getDCI().getString("CareOf");
  wstring address =  getDCI().getString("Street");
  wstring city =  getDCI().getString("ZIP") + L" " + getDCI().getString("City");
  wstring country =  getDCI().getString("Country");

  int ayp = ys + 122;

  const int absX = oe->getPropertyInt("addressxpos", 125);
  int absY = oe->getPropertyInt("addressypos", 50);

  const int absYL = 5;
  gdi.addStringUT(ayp, xs+data.adrPos, fontMedium, getName()).setAbsPrintPos(absX,absY);
  ayp+=lh;
  absY+=absYL;

  if (!co.empty())
    gdi.addStringUT(ayp, xs+data.adrPos, fontMedium, co).setAbsPrintPos(absX,absY), ayp+=lh, absY+=absYL;

  if (!address.empty())
    gdi.addStringUT(ayp, xs+data.adrPos, fontMedium, address).setAbsPrintPos(absX,absY), ayp+=lh, absY+=absYL;

  if (!city.empty())
    gdi.addStringUT(ayp, xs+data.adrPos, fontMedium, city).setAbsPrintPos(absX,absY), ayp+=lh, absY+=absYL;

  if (!country.empty())
    gdi.addStringUT(ayp, xs+data.adrPos, fontMedium, country).setAbsPrintPos(absX,absY), ayp+=lh, absY+=absYL;

  yp = ayp+30;

  gdi.addString("", yp, xs, boldSmall, "Deltagare");
  gdi.addString("", yp, xs+data.clsPos, boldSmall, "Klass");

  gdi.addString("", yp, xs+data.feePos, boldSmall|textRight, "Avgift");
  gdi.addString("", yp, xs+data.cardPos, boldSmall|textRight, "Brickhyra");
  gdi.addString("", yp, xs+data.paidPos, boldSmall|textRight, "Betalat");

  gdi.addString("", yp, xs+data.resPos, boldSmall, "Resultat");

  yp += lh;
  data.multiDay = oe->hasPrevStage();

  list<InvoiceLine> lines;
  for (size_t k=0;k<runners.size(); k++) {
    cTeam team = runners[k]->getTeam();
    if (team && team->getDCI().getInt("Fee") > 0
      && team->getClubId() == runners[k]->getClubId())
      continue; // Show this line under the team.
    addRunnerInvoiceLine(runners[k], false, definedPayModes, data, lines);
  }

  for (size_t k=0;k<teams.size(); k++) {
    addTeamInvoiceLine(teams[k], definedPayModes, data, lines);
  }

  for (list<InvoiceLine>::iterator it = lines.begin(); it != lines.end(); ++it) {
    addInvoiceLine(gdi, *it, data);
  }

  yp += lh;
  gdi.addStringUT(yp, xs+data.feePos, boldText|textRight, oe->formatCurrency(data.total_fee_amount));
  gdi.addStringUT(yp, xs+data.cardPos, boldText|textRight, oe->formatCurrency(data.total_rent_amount));
  gdi.addStringUT(yp, xs+data.paidPos, boldText|textRight, oe->formatCurrency(data.total_paid_amount));

  yp+=lh*2;
  toPay = data.total_fee_amount+data.total_rent_amount-data.total_paid_amount;
  hasPaid = data.total_paid_amount;
  for (map<int,int>::iterator it = data.paidPerMode.begin(); it != data.paidPerMode.end(); ++it) {
    paidPerMode[it->first] += it->second;
  }
  gdi.addString("", yp, xs, boldText, L"Att betala: X#" + oe->formatCurrency(toPay));

  gdi.updatePos(gdi.scaleLength(710),0,0,0);

  yp+=lh*2;

  gdi.addStringUT(yp, xs, normalText, lang.tl(L"Vänligen betala senast ") 
                 + pdate + lang.tl(L" till ") + account + L".");
  gdi.dropLine(2);
  //gdi.addStringUT(gdi.getCY()-1, 1, pageNewPage, blank, 0, 0);

  vector< pair<wstring, int> > mlines;
  oe->getExtraLines("IVExtra", mlines);
  for (size_t k = 0; k < mlines.size(); k++) {
    gdi.addStringUT(mlines[k].second, mlines[k].first);
  }
  if (mlines.size()>0)
    gdi.dropLine(0.5);

  gdi.addStringUT(gdi.getCY()-1, xs, pageNewPage, "", 0, 0).setExtra(START_YP);
  gdi.dropLine();
}

void oEvent::getClubRunners(int clubId, vector<pRunner> &runners) const
{
  oRunnerList::const_iterator rit;
  runners.clear();

  for (rit=Runners.begin(); rit != Runners.end(); ++rit) {
    if (!rit->skip() && rit->getClubId() == clubId)
      runners.push_back(pRunner(&*rit));
  }
}

void oEvent::getClubTeams(int clubId, vector<pTeam> &teams) const
{
  oTeamList::const_iterator rit;
  teams.clear();

  for (rit=Teams.begin(); rit != Teams.end(); ++rit) {
    if (!rit->skip() && rit->getClubId() == clubId)
      teams.push_back(pTeam(&*rit));
  }
}

void oClub::definedPayModes(oEvent &oe, map<int, wstring> &definedPayModes) {
  vector< pair<wstring, size_t> > modes;
  oe.getPayModes(modes);
  if (modes.size() > 1) {
    for (size_t k = 0; k < modes.size(); k++) {
      definedPayModes[modes[k].second] = modes[k].first;
    }
  }
}

void oEvent::printInvoices(gdioutput &gdi, InvoicePrintType type,
                           const wstring &basePath, bool onlySummary) {

  map<int, wstring> definedPayModes;
  oClub::definedPayModes(*this, definedPayModes);
  map<int, int> paidPerMode;

  oClub::assignInvoiceNumber(*this, false);
  oClubList::iterator it;
  oe->calculateTeamResults(false);
  oe->sortTeams(ClassStartTime, 0, true);
  oe->calculateResults(RTClassResult);
  oe->sortRunners(ClassStartTime);
  int pay, paid;
  vector<int> fees, vpaid;
  set<int> clubId;
  fees.reserve(Clubs.size());
  int k=0;

  bool toFile = type > 10;

  wstring path = basePath;
  if (basePath.size() > 0 && *basePath.rbegin() != '\\' && *basePath.rbegin() != '/')
    path.push_back('\\');

  if (toFile) {
    ofstream fout;

    if (type == IPTElectronincHTML)
      fout.open((path + L"invoices.txt").c_str());


    for (it=Clubs.begin(); it != Clubs.end(); ++it) {
      if (!it->isRemoved()) {
        gdi.clearPage(false);
        int nr = it->getDCI().getInt("InvoiceNo");
        wstring filename;
        if (type == IPTElectronincHTML)
          filename = L"invoice" + itow(nr*197) + L".html";
        else if (type == IPTAllPDF)
          filename = lang.tl(L"Faktura ") + makeValidFileName(it->getDisplayName(), false) + L" (" + itow(nr) + L").pdf";
        else
          filename = lang.tl(L"Faktura ") + makeValidFileName(it->getDisplayName(), false) + L" (" + itow(nr) + L").html";
        wstring email = it->getDCI().getString("EMail");
        bool hasEmail = !(email.empty() || email.find_first_of('@') == email.npos);

        if (type == IPTElectronincHTML) {
          if (!hasEmail)
            continue;
        }

        it->generateInvoice(gdi, pay, paid, definedPayModes, paidPerMode);

        if (type == IPTElectronincHTML && pay > 0) {
          fout << it->getId() << ";" << gdi.toUTF8(it->getName()) << ";" <<
            nr << ";" << gdi.toUTF8(filename) << ";" << gdi.toUTF8(email) << ";"
                << gdi.toUTF8(formatCurrency(pay))  <<endl;
        }

        if (type == IPTAllPDF) {
          pdfwriter pdf;
          pdf.generatePDF(gdi, path + filename, lang.tl("Faktura"), L"", gdi.getTL());
        }
        else
          gdi.writeHTML(path + filename, lang.tl(L"Faktura"), 0);

        clubId.insert(it->getId());
        fees.push_back(pay);
        vpaid.push_back(paid);
      }
      gdi.clearPage(true);
    }
  }
  else {
    for (it=Clubs.begin(); it != Clubs.end(); ++it) {
      if (!it->isRemoved()) {

        wstring email = it->getDCI().getString("EMail");
        bool hasEmail = !(email.empty() || email.find_first_of('@') == email.npos);
        if (type == IPTNoMailPrint && hasEmail)
          continue;

        it->generateInvoice(gdi, pay, paid, definedPayModes, paidPerMode);
        clubId.insert(it->getId());
        fees.push_back(pay);
        vpaid.push_back(paid);
      }
    }
  }

  if (onlySummary)
    gdi.clearPage(true);
  k=0;
  gdi.dropLine(1);
  gdi.addString("", boldLarge, "Sammanställning, ekonomi");
  int yp = gdi.getCY() + 10;

  gdi.addString("", yp, 50, boldText, "Faktura nr");
  gdi.addString("", yp, 240, boldText|textRight, "KlubbId");

  gdi.addString("", yp, 250, boldText, "Klubb");
  gdi.addString("", yp, 550, boldText|textRight, "Faktura");
  gdi.addString("", yp, 620, boldText|textRight, "Kontant");

  yp+=gdi.getLineHeight()+3;

  int sum = 0, psum = 0;
  for (it=Clubs.begin(); it != Clubs.end(); ++it) {
    if (!it->isRemoved() && clubId.count(it->getId()) > 0) {

      gdi.addStringUT(yp, 50, fontMedium, itos(it->getDCI().getInt("InvoiceNo")));
      gdi.addStringUT(yp, 240, textRight|fontMedium, itos(it->getId()));
      gdi.addStringUT(yp, 250, fontMedium, it->getName());
      gdi.addStringUT(yp, 550, fontMedium|textRight, oe->formatCurrency(fees[k]));
      gdi.addStringUT(yp, 620, fontMedium|textRight, oe->formatCurrency(vpaid[k]));

      sum+=fees[k];
      psum+=vpaid[k];
      k++;
      yp+=gdi.getLineHeight();
    }
  }
  
  yp+=gdi.getLineHeight();
  gdi.fillDown();
  gdi.addStringUT(yp, 550, boldText|textRight, lang.tl(L"Totalt faktureras: ") + oe->formatCurrency(sum));
  gdi.addStringUT(yp+gdi.getLineHeight(), 550, boldText|textRight, lang.tl(L"Totalt kontant: ") + oe->formatCurrency(psum));
  gdi.dropLine(0.5);
  if (definedPayModes.size() > 1) {
    vector< pair<wstring, size_t> > modes;
    oe->getPayModes(modes);
    for (k = 0; k < int(modes.size()); k++) {
      int ppm = paidPerMode[k]; 
      if (ppm > 0) {
        gdi.addStringUT(gdi.getCY(), 550, normalText|textRight, modes[k].first + L": " + oe->formatCurrency(ppm));
      }
    }
  }
}

void oClub::updateFromDB()
{
  if (!oe->useRunnerDb())
    return;

  pClub pc = oe->runnerDB->getClub(Id);

  if (pc && !pc->sameClub(*this))
    pc = 0;

  if (pc==0)
    pc = oe->runnerDB->getClub(name);

  if (pc) {
    memcpy(oData, pc->oData, sizeof (oData));
    updateChanged();
  }
}

void oEvent::updateClubsFromDB()
{
  if (!oe->useRunnerDb())
    return;

  oClubList::iterator it;

  for (it=Clubs.begin();it!=Clubs.end();++it) {
    it->updateFromDB();
    it->synchronize();
  }
}

bool oClub::sameClub(const oClub &c)
{
  return _wcsicmp(name.c_str(), c.name.c_str())==0;
}

void oClub::remove()
{
  if (oe)
    oe->removeClub(Id);
}

bool oClub::canRemove() const
{
  return !oe->isClubUsed(Id);
}

void oEvent::setupClubInfoData() {
  if (tClubDataRevision == dataRevision || Clubs.empty())
    return;

  inthashmap fee(Clubs.size());
  inthashmap paid(Clubs.size());
  inthashmap runners(Clubs.size());

  // Individual fees
  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    oRunner &r = *it;
    if (r.Club) {
      int id = r.Club->Id;
      ++runners[id];
      oDataConstInterface di = r.getDCI();
      bool skip = r.Class && r.Class->getClassStatus() == oClass::InvalidRefund;

      if (!skip) {
        int cardFee = di.getInt("CardFee");
        if (cardFee < 0)
          cardFee = 0;
        fee[id] += di.getInt("Fee") + cardFee;
      }
      paid[id] += di.getInt("Paid");
    }
  }

  // Team fees
  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;
    oTeam &t = *it;
    if (t.Club) {
      int id = t.Club->Id;
      oDataConstInterface di = t.getDCI();
      bool skip = t.Class && t.Class->getClassStatus() == oClass::InvalidRefund;

      if (!skip) {
        fee[id] += di.getInt("Fee");
      }
      paid[id] += di.getInt("Paid");
    }
  }

  for (oClubList::iterator it = Clubs.begin(); it != Clubs.end(); ++it) {
    int id = it->Id;
    it->tFee = fee[id];
    it->tPaid = paid[id];
    it->tNumRunners = runners[id];
  }

  tClubDataRevision = dataRevision;
}


bool oClub::isVacant() const {
  return getId() == oe->getVacantClubIfExist(false);
}

void oClub::changeId(int newId) {
  pClub old = oe->clubIdIndex[Id];
  if (old == this)
    oe->clubIdIndex.remove(Id);

  oBase::changeId(newId);

  oe->clubIdIndex[newId] = this;
}

void oClub::clearClubs(oEvent &oe) {
  vector<pRunner> r;
  oe.getRunners(0, 0, r, false);

  for (size_t k = 0; k<r.size(); k++) {
    r[k]->setClubId(0);
    r[k]->synchronize(true);
  }

  vector<pTeam> t;
  oe.getTeams(0, t, false);

  for (size_t k = 0; k<t.size(); k++) {
    t[k]->setClubId(0);
    t[k]->synchronize(true);
  }

  vector<pClub> c;
  oe.getClubs(c, false);
  for (size_t k = 0; k<c.size(); k++) {
    oe.removeClub(c[k]->getId());
  }
}

void oClub::assignInvoiceNumber(oEvent &oe, bool reset) {
  oe.synchronizeList(oLClubId);
  oe.Clubs.sort();
  int numberStored = oe.getPropertyInt("FirstInvoice", 100);
  int number = numberStored;
  if (!reset) {
    int maxInvoice = 0;
    for (oClubList::iterator it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
      if (it->isRemoved())
        continue;
      int no = it->getDCI().getInt("InvoiceNo");
      maxInvoice = max(maxInvoice, no);
    }

    if (maxInvoice != 0)
      number = maxInvoice + 1;
    else
      reset = true; // All zero
  }

  for (oClubList::iterator it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (reset || it->getDCI().getInt("InvoiceNo") == 0) {
      it->getDI().setInt("InvoiceNo", number++);
      it->synchronize(true);
    }
  }

  if (number > numberStored)
    oe.setProperty("FirstInvoice", number);
}

int oClub::getFirstInvoiceNumber(oEvent &oe) {
  oe.synchronizeList(oLClubId);
  int number = 0;
  for (oClubList::iterator it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
    if (it->isRemoved())
      continue;
    int no = it->getDCI().getInt("InvoiceNo");
    if (no > 0) {
      if (number == 0)
        number = no;
      else
        number = min(number, no);
    }
  }
  return number;
}

void oClub::changedObject() {
  if (oe)
    oe->globalModification = true;
}

bool oClub::operator<(const oClub &c) const {
  return CompareString(LOCALE_USER_DEFAULT, 0,
                      name.c_str(), name.length(),
                      c.name.c_str(), c.name.length()) == CSTR_LESS_THAN;
}
