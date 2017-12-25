/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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

// oControl.cpp: implementation of the oControl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>

#include "oControl.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <cassert>
#include "Localizer.h"
#include "Table.h"
#include "MeOSFeatures.h"
#include <set>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

oControl::oControl(oEvent *poe): oBase(poe)
{
  getDI().initData();
  nNumbers=0;
  Status=StatusOK;
  tMissedTimeMax = 0;
  tMissedTimeTotal = 0;
  tNumVisitorsActual = 0;
  tNumVisitorsExpected = 0;
  tMissedTimeMedian = 0;
  tMistakeQuotient = 0;
  tNumRunnersRemaining = 0;
  tStatDataRevision = -1;

  tHasFreePunchLabel = false;
  tNumberDuplicates = 0;
}

oControl::oControl(oEvent *poe, int id): oBase(poe)
{
  Id = id;
  getDI().initData();
  nNumbers=0;
  Status=StatusOK;

  tMissedTimeMax = 0;
  tMissedTimeTotal = 0;
  tNumVisitorsActual = 0;
  tNumVisitorsExpected = 0;
  tMistakeQuotient = 0;
  tMissedTimeMedian = 0;
  tNumRunnersRemaining = 0;
  tStatDataRevision = -1;

  tHasFreePunchLabel = false;
  tNumberDuplicates = 0;
}

oControl::~oControl()
{
}

pair<int, int> oControl::getIdIndexFromCourseControlId(int courseControlId) {
  return make_pair(courseControlId % 100000, courseControlId / 100000);
}

int oControl::getCourseControlIdFromIdIndex(int controlId, int index) {
  assert(controlId < 100000);
  return controlId + index * 100000;
}


bool oControl::write(xmlparser &xml)
{
  if (Removed) return true;

  xml.startTag("Control");

  xml.write("Id", Id);
  xml.write("Updated", Modified.getStamp());
  xml.write("Name", Name);
  xml.write("Numbers", codeNumbers());
  xml.write("Status", Status);

  getDI().write(xml);
  xml.endTag();

  return true;
}

void oControl::set(int pId, int pNumber, wstring pName)
{
  Id=pId;
  Numbers[0]=pNumber;
  nNumbers=1;
  Name=pName;

  updateChanged();
}


void oControl::setStatus(ControlStatus st){
  if (st!=Status){
    Status=st;
    updateChanged();
  }
}

void oControl::setName(wstring name)
{
  if (name!=getName()){
    Name=name;
    updateChanged();
  }
}


void oControl::set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);
  nNumbers=0;
  Numbers[0]=0;

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id=it->getInt();
    }
    else if (it->is("Number")){
      Numbers[0]=it->getInt();
      nNumbers=1;
    }
    else if (it->is("Numbers")){
      decodeNumbers(it->getRaw());
    }
    else if (it->is("Status")){
      Status=(ControlStatus)it->getInt();
    }
    else if (it->is("Name")){
      Name=it->getw();
      if (Name.size() > 1 && Name.at(0) == '%') {
        Name = lang.tl(Name.substr(1));
      }
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRaw());
    }
    else if (it->is("oData")){
      getDI().set(*it);
    }
  }
}

int oControl::getFirstNumber() const {
  if (nNumbers > 0)
    return Numbers[0];
  else
    return 0;
}

wstring oControl::getString() {
  wchar_t bf[32];
  if (Status==StatusOK || Status==StatusNoTiming)
    return codeNumbers('|');
  else if (Status==StatusMultiple)
    return codeNumbers('+');
  else if (Status==StatusRogaining)
    return codeNumbers('|') + L", " + itow(getRogainingPoints()) + L"p";
  else
    swprintf_s(bf, 32, L"~%s", codeNumbers().c_str());
  return bf;
}

wstring oControl::getLongString()
{
  if (Status==StatusOK || Status==StatusNoTiming){
    if (nNumbers==1)
      return codeNumbers('|');
    else
      return wstring(lang.tl("VALFRI("))+codeNumbers(',')+L")";
  }
  else if (Status == StatusMultiple) {
    return wstring(lang.tl("ALLA("))+codeNumbers(',')+L")";
  }
  else if (Status == StatusRogaining)
    return wstring(lang.tl("RG("))+codeNumbers(',') + L"|" + itow(getRogainingPoints()) + L"p)";
  else
    return wstring(lang.tl("TRASIG("))+codeNumbers(',')+L")";
}

bool oControl::hasNumber(int i)
{
  for(int n=0;n<nNumbers;n++)
    if (Numbers[n]==i) {
      // Mark this number as checked
      checkedNumbers[n]=true;
      return true;
    }
  if (nNumbers>0)
    return false;
  else return true;
}

bool oControl::uncheckNumber(int i)
{
  for(int n=0;n<nNumbers;n++)
    if (Numbers[n]==i) {
      // Mark this number as checked
      checkedNumbers[n]=false;
      return true;
    }
  return false;
}

bool oControl::hasNumberUnchecked(int i)
{
  for(int n=0;n<nNumbers;n++)
    if (Numbers[n]==i && checkedNumbers[n]==0) {
      // Mark this number as checked
      checkedNumbers[n]=true;
      return true;
    }
  if (nNumbers>0)
    return false;
  else return true;
}


int oControl::getNumMulti()
{
  if (Status==StatusMultiple)
    return nNumbers;
  else
    return 1;
}


wstring oControl::codeNumbers(char sep) const
{
  wstring n;
  wchar_t bf[16];

  for(int i=0;i<nNumbers;i++){
    _itow_s(Numbers[i], bf, 16, 10);
    n+=bf;
    if (i+1<nNumbers)
      n+=sep;
  }
  return n;
}

bool oControl::decodeNumbers(string s)
{
  const char *str=s.c_str();

  nNumbers=0;

  while(*str){
    int cid=atoi(str);

    while(*str && (*str!=';' && *str!=',' && *str!=' ')) str++;
    while(*str && (*str==';' || *str==',' || *str==' ')) str++;

    if (cid>0 && cid<1024 && nNumbers<32)
      Numbers[nNumbers++]=cid;
  }

  if (Numbers==0){
    Numbers[0]=0;
    nNumbers=1;
    return false;
  }
  else return true;
}

bool oControl::setNumbers(const wstring &numbers)
{
  int nn=nNumbers;
  int bf[32];

  if (unsigned(nNumbers)<32)
    memcpy(bf, Numbers, sizeof(int)*nNumbers);
  string nnumbers(numbers.begin(), numbers.end());
  bool success=decodeNumbers(nnumbers);

  if (!success) {
    memcpy(Numbers, bf, sizeof(int)*nn);
    nNumbers = nn;
  }

  if (nNumbers!=nn || memcmp(bf, Numbers, sizeof(int)*nNumbers)!=0) {
    updateChanged();
    oe->punchIndex.clear();
  }

  return success;
}

wstring oControl::getName() const
{
	if (!Name.empty())
		return Name;
	else {
		wchar_t bf[16];
		swprintf_s(bf, L"[%d]", Id);
		return bf;
	}
}

wstring oControl::getIdS() const
{
	if (!Name.empty())
		return Name;
	else {
		wchar_t bf[16];
		swprintf_s(bf, L"%d", Id);
		return bf;
	}
}

oDataContainer &oControl::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = 0;
  return *oe->oControlData;
}

const vector< pair<wstring, size_t> > &oEvent::fillControls(vector< pair<wstring, size_t> > &out, oEvent::ControlType type)
{
  out.clear();
  oControlList::iterator it;
  synchronizeList(oLControlId);
  Controls.sort();

  if (type == oEvent::CTCourseControl) {
    vector<pControl> dmy;
    getControls(dmy, true);
  }

  wstring b;
  wchar_t bf[256];
  for (it=Controls.begin(); it != Controls.end(); ++it) {
    if (!it->Removed){
      b.clear();

      if (type==oEvent::CTAll) {
        if (it->Status == oControl::StatusFinish || it->Status == oControl::StatusStart) {
          b += it->Name;
        }
        else {
          if (it->Status == oControl::StatusOK || it->Status == oControl::StatusNoTiming)
            b += L"[OK]\t";
          else if (it->Status==oControl::StatusMultiple)
            b += L"[M]\t";
          else if (it->Status==oControl::StatusRogaining)
            b += L"[R]\t";
          else if (it->Status==oControl::StatusBad)
            b += makeDash(L"[-]\t");
          else if (it->Status==oControl::StatusOptional)
            b += makeDash(L"[O]\t");
          else b += L"[ ]\t";

          swprintf_s(bf, L" %s", it->codeNumbers(' ').c_str());
          b+=bf;

          if (it->Status==oControl::StatusRogaining)
            b+=L"\t(" + itow(it->getRogainingPoints()) + L"p)";
          else if (it->Name.length()>0) {
            b+=L"\t(" + it->Name + L")";
          }
        }
        out.push_back(make_pair(b, it->Id));
      }
      else if (type==oEvent::CTRealControl) {
        if (it->Status == oControl::StatusFinish || it->Status == oControl::StatusStart)
          continue;

        swprintf_s(bf, lang.tl("Kontroll %s").c_str(), it->codeNumbers(' ').c_str());
        b=bf;

        if (!it->Name.empty())
          b += L" (" + it->Name + L")";

        out.push_back(make_pair(b, it->Id));
      }
      else if (type==oEvent::CTCourseControl) {
        if (it->Status == oControl::StatusFinish || it->Status == oControl::StatusStart)
          continue;

        for (int i = 0; i < it->getNumberDuplicates(); i++) {
          swprintf_s(bf, lang.tl("Kontroll %s").c_str(), it->codeNumbers(' ').c_str());
          b = bf;

          if (it->getNumberDuplicates() > 1)
            b += L"-" + itow(i+1);

          if (!it->Name.empty())
            b += L" (" + it->Name + L")";

          out.push_back(make_pair(b, oControl::getCourseControlIdFromIdIndex(it->Id, i)));
        }
      }
    }
  }
  return out;
}

const vector< pair<wstring, size_t> > &oEvent::fillControlTypes(vector< pair<wstring, size_t> > &out)
{
  oControlList::iterator it;
  synchronizeList(oLControlId);
  out.clear();
  //gdi.clearList(name);
  out.clear();
  set<int> sicodes;

  for (it=Controls.begin(); it != Controls.end(); ++it){
    if (!it->Removed) {
      for (int k=0;k<it->nNumbers;k++)
        sicodes.insert(it->Numbers[k]);
    }
  }

  set<int>::iterator sit;
  wchar_t bf[32];
  /*gdi.addItem(name, lang.tl("Check"), oPunch::PunchCheck);
  gdi.addItem(name, lang.tl("Start"), oPunch::PunchStart);
  gdi.addItem(name, lang.tl("Mål"), oPunch::PunchFinish);*/
  out.push_back(make_pair(lang.tl("Check"), oPunch::PunchCheck));
  out.push_back(make_pair(lang.tl("Start"), oPunch::PunchStart));
  out.push_back(make_pair(lang.tl("Mål"), oPunch::PunchFinish));

  for (sit = sicodes.begin(); sit!=sicodes.end(); ++sit) {
    swprintf_s(bf, lang.tl("Kontroll %s").c_str(), itow(*sit).c_str());
    //gdi.addItem(name, bf, *sit);
    out.push_back(make_pair(bf, *sit));
  }
  return out;
}

void oControl::setupCache() const {
  if (tCache.dataRevision != oe->dataRevision) {
    tCache.timeAdjust = getDCI().getInt("TimeAdjust");
    tCache.minTime = getDCI().getInt("MinTime");
    tCache.dataRevision = oe->dataRevision;
  }
}

int oControl::getMinTime() const
{
  if (Status == StatusNoTiming)
    return 0;
  setupCache();
  return tCache.minTime;
}

int oControl::getTimeAdjust() const
{
  setupCache();
  return tCache.timeAdjust;
}

wstring oControl::getTimeAdjustS() const
{
  return getTimeMS(getTimeAdjust());
}

wstring oControl::getMinTimeS() const
{
  if (getMinTime()>0)
    return getTimeMS(getMinTime());
  else
    return makeDash(L"-");
}

int oControl::getRogainingPoints() const
{
  return getDCI().getInt("Rogaining");
}

wstring oControl::getRogainingPointsS() const
{
  int pt = getRogainingPoints();
  return pt != 0 ? itow(pt) : L"";
}

void oControl::setTimeAdjust(int v)
{
  getDI().setInt("TimeAdjust", v);
}

void oControl::setRadio(bool r)
{
  // 1 means radio, 2 means no radio, 0 means default
  getDI().setInt("Radio", r ? 1 : 2);
}

bool oControl::isValidRadio() const
{
  int flag = getDCI().getInt("Radio");
  if (flag == 0)
    return (tHasFreePunchLabel || hasName()) && getStatus() == oControl::StatusOK;
  else
    return flag == 1;
}

void oControl::setTimeAdjust(const wstring &s)
{
  setTimeAdjust(convertAbsoluteTimeMS(s));
}

void oControl::setMinTime(int v)
{
  if (v<0 || v == NOTIME)
    v = 0;
  getDI().setInt("MinTime", v);
}

void oControl::setMinTime(const wstring &s)
{
  setMinTime(convertAbsoluteTimeMS(s));
}

void oControl::setRogainingPoints(int v)
{
  getDI().setInt("Rogaining", v);
}

void oControl::setRogainingPoints(const string &s)
{
  setRogainingPoints(atoi(s.c_str()));
}

void oControl::startCheckControl()
{
  //Mark all numbers as unchecked.
  for (int k=0;k<nNumbers;k++)
    checkedNumbers[k]=false;
}

wstring oControl::getInfo() const
{
  return getName();
}

void oControl::addUncheckedPunches(vector<int> &mp, bool supportRogaining) const
{
  if (controlCompleted(supportRogaining))
    return;

  for (int k=0;k<nNumbers;k++)
    if (!checkedNumbers[k]) {
      mp.push_back(Numbers[k]);

      if (Status!=StatusMultiple)
        return;
    }
}

int oControl::getMissingNumber() const
{
  for (int k=0;k<nNumbers;k++)
    if (!checkedNumbers[k])
      return Numbers[k];

  assert(false);
  return Numbers[0];//This should not happen
}

bool oControl::controlCompleted(bool supportRogaining) const
{
  if (Status==StatusOK || Status==StatusNoTiming || ((Status == StatusRogaining) && !supportRogaining)) {
    //Check if any number is used.
    for (int k=0;k<nNumbers;k++)
      if (checkedNumbers[k])
        return true;

    //Return true only if there is no control
    return nNumbers==0;
  }
  else if (Status==StatusMultiple) {
    //Check if al numbers are used.
    for (int k=0;k<nNumbers;k++)
      if (!checkedNumbers[k])
        return false;

    return true;
  }
  else return true;
}

int oControl::getMissedTimeTotal() const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  return tMissedTimeTotal;
}

int oControl::getMissedTimeMax() const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  return tMissedTimeMax;
}

int oControl::getMissedTimeMedian() const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  return tMissedTimeMedian;
}

int oControl::getMistakeQuotient() const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  return tMistakeQuotient;
}


int oControl::getNumVisitors(bool actulaVisits) const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  if (actulaVisits)
    return tNumVisitorsActual;
  else
    return tNumVisitorsExpected;
}

int oControl::getNumRunnersRemaining() const {
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();

  return tNumRunnersRemaining;
}

void oEvent::setupControlStatistics() const {
  // Reset all times
  for (oControlList::const_iterator it = Controls.begin(); it != Controls.end(); ++it) {
    it->tMissedTimeMax = 0;
    it->tMissedTimeTotal = 0;
    it->tNumVisitorsActual = 0;
    it->tNumVisitorsExpected = 0;
    it->tNumRunnersRemaining = 0;
    it->tMissedTimeMedian = 0;
    it->tMistakeQuotient = 0;
    it->tStatDataRevision = dataRevision; // Mark as up-to-date
  }

  map<int, pair<int, vector<int> > > lostPerControl; // First is "actual" misses,
  vector<int> delta;
  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    pCourse pc = it->getCourse(true);
    if (!pc)
      continue;
    it->getSplitAnalysis(delta);

    int nc = pc->getNumControls();
    if (delta.size()>unsigned(nc)) {
      for (int i = 0; i<nc; i++) {
        pControl ctrl = pc->getControl(i);
        if (ctrl && delta[i]>0) {
          if (delta[i] < 10 * 60)
            ctrl->tMissedTimeTotal += delta[i];
          else
            ctrl->tMissedTimeTotal += 10*60; // Use max 10 minutes

          ctrl->tMissedTimeMax = max(ctrl->tMissedTimeMax, delta[i]);
        }

        if (delta[i] > 0) {
          lostPerControl[ctrl->getId()].second.push_back(delta[i]);
          ++lostPerControl[ctrl->getId()].first;
        }

        ctrl->tNumVisitorsActual++;
      }
    }

    if (!it->isVacant() && it->getStatus() != StatusDNS && it->getStatus() != StatusCANCEL
                        && it->getStatus() != StatusNotCompetiting) {

      for (int i = 0; i < nc; i++) {
        pControl ctrl = pc->getControl(i);
        ctrl->tNumVisitorsExpected++;

        if (it->getStatus() == StatusUnknown)
          ctrl->tNumRunnersRemaining++;
      }

    }
  }

  for (oControlList::const_iterator it = Controls.begin(); it != Controls.end(); ++it) {
    if (!it->isRemoved()) {
      int id = it->getId();

      map<int, pair<int, vector<int> > >::iterator res = lostPerControl.find(id);
      if (res != lostPerControl.end()) {
        if (!res->second.second.empty()) {
          sort(res->second.second.begin(), res->second.second.end());
          int avg = res->second.second[res->second.second.size() / 2];
          it->tMissedTimeMedian = avg;
        }
        it->tMistakeQuotient = (100 * res->second.first + 50) / it->tNumVisitorsActual; 
      }
    }
  }
}

bool oEvent::hasRogaining() const
{
  oControlList::const_iterator it;
  for (it=Controls.begin(); it != Controls.end(); ++it) {
    if (!it->Removed && it->isRogaining(true))
      return true;
  }
  return false;
}

const wstring oControl::getStatusS() const {
  //enum ControlStatus {StatusOK=0, StatusBad=1, StatusMultiple=2,
  //                    StatusStart = 4, StatusFinish = 5, StatusRogaining = 6};

  switch (getStatus()) {
    case StatusOK:
      return lang.tl("OK");
    case StatusBad:
      return lang.tl("Trasig");
    case StatusOptional:
      return lang.tl("Valfri");
    case StatusMultiple:
      return lang.tl("Multipel");
    case StatusRogaining:
      return lang.tl("Rogaining");
    case StatusStart:
      return lang.tl("Start");
    case StatusFinish:
      return lang.tl("Mål");
    case StatusNoTiming:
      return lang.tl("Utan tidtagning");
    default:
      return lang.tl("Okänd");
  }
}

void oEvent::fillControlStatus(gdioutput &gdi, const string& id) const
{
  vector< pair<wstring, size_t> > d;
  oe->fillControlStatus(d);
  gdi.addItem(id, d);
}


const vector< pair<wstring, size_t> > &oEvent::fillControlStatus(vector< pair<wstring, size_t> > &out) const
{
  out.clear();
  out.push_back(make_pair(lang.tl(L"OK"), oControl::StatusOK));
  out.push_back(make_pair(lang.tl(L"Multipel"), oControl::StatusMultiple));

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Rogaining))
    out.push_back(make_pair(lang.tl(L"Rogaining"), oControl::StatusRogaining));
  out.push_back(make_pair(lang.tl(L"Utan tidtagning"), oControl::StatusNoTiming));
  out.push_back(make_pair(lang.tl(L"Trasig"), oControl::StatusBad));
  out.push_back(make_pair(lang.tl(L"Valfri"), oControl::StatusOptional));

  return out;
}

Table *oEvent::getControlTB()//Table mode
{
  if (tables.count("control") == 0) {
    Table *table=new Table(this, 20, L"Kontroller", "controls");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 150, false);
    table->addColumn("Status", 70, false);
    table->addColumn("Stämpelkoder", 100, true);
    table->addColumn("Antal löpare", 70, true, true);
    table->addColumn("Bomtid (max)", 70, true, true);
    table->addColumn("Bomtid (medel)", 70, true, true);
    table->addColumn("Bomtid (median)", 70, true, true);

    oe->oControlData->buildTableCol(table);
    tables["control"] = table;
    table->addOwnership();

    table->setTableProp(Table::CAN_DELETE);
  }

  tables["control"]->update();
  return tables["control"];
}

void oEvent::generateControlTableData(Table &table, oControl *addControl)
{
  if (addControl) {
    addControl->addTableRow(table);
    return;
  }

  synchronizeList(oLControlId);
  oControlList::iterator it;

  for (it=Controls.begin(); it != Controls.end(); ++it){
    if (!it->isRemoved()){
      it->addTableRow(table);
    }
  }
}

void oControl::addTableRow(Table &table) const {
  oControl &it = *pControl(this);
  table.addRow(getId(), &it);

  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, it, TID_CONTROL, getName(), true);
  bool canEdit = getStatus() != oControl::StatusFinish && getStatus() != oControl::StatusStart;
  table.set(row++, it, TID_STATUS, getStatusS(), canEdit, cellSelection);
  table.set(row++, it, TID_CODES, codeNumbers(), true);

  int nv = getNumVisitors(true);
  table.set(row++, it, 50, itow(nv), false);
  table.set(row++, it, 51, nv > 0 ? formatTime(getMissedTimeMax()) : L"-", false);
  table.set(row++, it, 52, nv > 0 ? formatTime(getMissedTimeTotal()/nv) : L"-", false);
  table.set(row++, it, 53, nv > 0 ? formatTime(getMissedTimeMedian()) : L"-", false);

  oe->oControlData->fillTableCol(it, table, true);
}

bool oControl::inputData(int id, const wstring &input,
                       int inputId, wstring &output, bool noUpdate)
{
  synchronize(false);

  if (id>1000) {
    return oe->oControlData->inputData(this, id, input, inputId, output, noUpdate);
  }
  switch(id) {
    case TID_CONTROL:
      setName(input);
      synchronize();
      output=getName();
      return true;
    case TID_STATUS:
      setStatus(ControlStatus(inputId));
      synchronize(true);
      output = getStatusS();
      return true;
    case TID_CODES:
      bool stat = setNumbers(input);
      synchronize(true);
      output = codeNumbers();
      return stat;
    break;
  }

  return false;
}

void oControl::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oControlData->fillInput(oData, id, 0, out, selected);
    return;
  }

  if (id==TID_STATUS) {
    oe->fillControlStatus(out);
    selected = getStatus();
  }
}

void oControl::remove()
{
  if (oe)
    oe->removeControl(Id);
}

bool oControl::canRemove() const
{
  return !oe->isControlUsed(Id);
}

void oEvent::getControls(vector<pControl> &c, bool calculateCourseControls) const {
  c.clear();

  if (calculateCourseControls) {
    unordered_map<int, pControl> cById;
    for (oControlList::const_iterator it = Controls.begin(); it != Controls.end(); ++it) {
      if (it->isRemoved())
        continue;
      it->tNumberDuplicates = 0;
      cById[it->getId()] = pControl(&*it);
    }
    for (oCourseList::const_iterator it = Courses.begin(); it != Courses.end(); ++it) {
      map<int, int> count;
      for (int i = 0; i < it->nControls; i++) {
        ++count[it->Controls[i]->getId()];
      }
      for (map<int, int>::iterator it = count.begin(); it != count.end(); ++it) {
        unordered_map<int, pControl>::iterator res = cById.find(it->first);
        if (res != cById.end()) {
          res->second->tNumberDuplicates = max(res->second->tNumberDuplicates, it->second);
        }
      }
    }
  }

  for (oControlList::const_iterator it = Controls.begin(); it != Controls.end(); ++it) {
    if (it->isRemoved())
      continue;
    c.push_back(pControl(&*it));
  }
}

void oControl::getNumbers(vector<int> &numbers) const {
  numbers.resize(nNumbers);
  for (int i = 0; i < nNumbers; i++) {
    numbers[i] = Numbers[i];
  }
}

void oControl::changedObject() {
  if (oe)
    oe->globalModification = true;
}

int oControl::getNumberDuplicates() const {
  return tNumberDuplicates;
}

void oControl::getCourseControls(vector<int> &cc) const {
  cc.resize(tNumberDuplicates);
  for (int i = 0; i < tNumberDuplicates; i++) {
    cc[i] = getCourseControlIdFromIdIndex(Id, i);
  }
}

void oControl::getCourses(vector<pCourse> &crs) const {
  crs.clear();
  for (oCourseList::const_iterator it = oe->Courses.begin(); it != oe->Courses.end(); it++) {
    if (it->isRemoved())
      continue;

    if (it->hasControl(this))
      crs.push_back(pCourse(&*it));
  }
}

void oControl::getClasses(vector<pClass> &cls) const {
  vector<pCourse> crs;
  getCourses(crs);
  std::set<int> cid;
  for (size_t k = 0; k< crs.size(); k++) {
    cid.insert(crs[k]->getId());
  }

  for (oClassList::const_iterator it = oe->Classes.begin(); it != oe->Classes.end(); it++) {
    if (it->isRemoved())
      continue;

    if (it->hasAnyCourse(cid))
      cls.push_back(pClass(&*it));
  }
}

int oControl::getControlIdByName(const oEvent &oe, const string &name) {
  if (_stricmp(name.c_str(), "finish") == 0)
    return oPunch::PunchFinish;
  if (_stricmp(name.c_str(), "start") == 0)
    return oPunch::PunchStart;

  vector<pControl> ac;
  oe.getControls(ac, true);
  wstring wname = oe.gdiBase().recodeToWide(name);
  for (pControl c : ac) {
    if (_wcsicmp(c->getName().c_str(), wname.c_str()) == 0)
      return c->getId();
  }

  return 0;
}

