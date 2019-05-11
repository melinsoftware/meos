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

// oCourse.cpp: implementation of the oCourse class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "oCourse.h"
#include "oEvent.h"
#include "SportIdent.h"
#include <limits>
#include "Localizer.h"
#include "meos_util.h"
#include "meosexception.h"
#include <cassert>
#include "gdioutput.h"
#include <algorithm>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#include "intkeymapimpl.hpp"

oCourse::oCourse(oEvent *poe) : oBase(poe)
{
  getDI().initData();
  nControls=0;
  Length=0;
  clearCache();
  tMapsUsed = -1;
  tMapsUsedNoVacant = -1;
  Id=oe->getFreeCourseId();
}

oCourse::oCourse(oEvent *poe, int id) : oBase(poe)
{
  getDI().initData();
  nControls=0;
  Length=0;
  clearCache();
  if (id == 0)
    id = oe->getFreeCourseId();
  Id=id;  
  tMapsUsed = -1;
  tMapsUsedNoVacant = -1;
  oe->qFreeCourseId = max(id, oe->qFreeCourseId);
}

oCourse::~oCourse()
{
}

wstring oCourse::getInfo() const
{
  return L"Bana " + Name;
}

bool oCourse::Write(xmlparser &xml)
{
  if (Removed) return true;

  xml.startTag("Course");

  xml.write("Id", Id);
  xml.write("Updated", Modified.getStamp());
  xml.write("Name", Name);
  xml.write("Length", Length);
  xml.write("Controls", getControls());
  xml.write("Legs", getLegLengths());

  getDI().write(xml);
  xml.endTag();

  return true;
}

void oCourse::Set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id=it->getInt();
    }
    else if (it->is("Length")){
      Length=it->getInt();
    }
    else if (it->is("Name")){
      Name=it->getw();
    }
    else if (it->is("Controls")){
      importControls(it->getRaw(), false);
    }
    else if (it->is("Legs")) {
      importLegLengths(it->getRaw(), false);
    }
    else if (it->is("oData")){
      getDI().set(*it);
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRaw());
    }
  }
}

string oCourse::getLegLengths() const
{
  string str;
  for(size_t m=0; m<legLengths.size(); m++){
    if (m>0)
      str += ";";
    str+=itos(legLengths[m]);
  }
  return str;
}

string oCourse::getControls() const
{
  string str="";
  char bf[16];
  for(int m=0;m<nControls;m++){
    sprintf_s(bf, 16, "%d;", Controls[m]->Id);
    str+=bf;
  }

  return str;
}

wstring oCourse::getControlsUI() const
{
  wstring str;
  wchar_t bf[16];
  int m;

  for(m=0;m<nControls-1;m++){
    swprintf_s(bf, 16, L"%d, ", Controls[m]->Id);
    str += bf;
  }

  if (m<nControls){
    swprintf_s(bf, 16, L"%d", Controls[m]->Id);
    str += bf;
  }

  return str;
}

vector<wstring> oCourse::getCourseReadable(int limit) const
{
  vector<wstring> res;

  wstring str;
  if (!useFirstAsStart())
    str = lang.tl("Start").substr(0, 1);
  int m;

  vector<pControl> rg;
  bool needFinish = false;
  bool rogaining = hasRogaining();
  for (m=0; m<nControls; m++) {
    if (Controls[m]->isRogaining(rogaining))
      rg.push_back(Controls[m]);
    else {
      if (!str.empty())
        str += L"-";
      str += Controls[m]->getLongString();
      needFinish = true;
    }
    if (str.length() >= size_t(limit)) {
      res.push_back(str);
      str.clear();
    }
  }

  if (needFinish && !useLastAsFinish()) {
    if (!str.empty())
      str += L"-";
    str += lang.tl("Mål").substr(0,1);
  }
  if (!str.empty()) {
    if (str.length()<5 && !res.empty())
      res.back().append(str);
    else
      res.push_back(str);
    str.clear();
  }

  if (!rg.empty()) {
    str = lang.tl("Rogaining: ");
    for (size_t k = 0; k<rg.size(); k++) {
      if (k>0)
        str += L", ";

      if (str.length() >= size_t(limit)) {
        res.push_back(str);
        str.clear();
      }
      str += rg[k]->getLongString();
    }
    if (!str.empty()) {
      res.push_back(str);
    }
  }

  return res;
}

pControl oCourse::addControl(int Id)
{
  pControl pc = doAddControl(Id);
  updateChanged();
  return pc;
}

pControl oCourse::doAddControl(int Id)
{
  if (nControls<NControlsMax) {
    pControl c=oe->getControl(Id, true);
    if (c==0)
      throw meosException("Felaktig kontroll");
    Controls[nControls++]=c;
    return c;
  }
  else
    throw meosException("För många kontroller.");
}

void oCourse::splitControls(const string &ctrls, vector<int> &nr) {
  const char *str=ctrls.c_str();

  nr.clear();

  while (*str) {
    int cid=atoi(str);

    while(*str && (*str!=';' && *str!=',' && *str!=' ')) str++;
    while(*str && (*str==';' || *str==',' || *str==' ')) str++;

    if (cid>0)
      nr.push_back(cid);
  }
}

bool oCourse::importControls(const string &ctrls, bool updateLegLengths) {
  int oldNC = nControls;
  vector<int> oldC;
  for (int k = 0; k<nControls; k++)
    oldC.push_back(Controls[k] ? Controls[k]->getId() : 0);

  nControls = 0;

  vector<int> newC;
  splitControls(ctrls, newC);

  for (size_t k = 0; k< newC.size(); k++)
    doAddControl(newC[k]);

  bool changed = nControls != oldNC;

  if (changed && updateLegLengths && legLengths.size() > 0) {
    int oldIndex = 0;
    int newIndex = 0;
    vector<int> newLen(nControls + 1);
    bool lastOK = true;
    while (newIndex < nControls) {
      if (oldIndex < int(oldC.size())) {
        if (oldC[oldIndex] == newC[newIndex]) {
          if (lastOK && oldIndex < int(legLengths.size())) {
            newLen[newIndex] = legLengths[oldIndex];
          }
          lastOK = true;
          oldIndex++;
        }
        else {
          lastOK = false;
          int forward = oldIndex + 1;
          while(forward < int(oldC.size())) {
            if (oldC[forward] == newC[newIndex]) {
              oldIndex = forward + 1;
              lastOK = true;
              break;
            }
            forward++;
          }
        }
      }
      else {
        lastOK = false;
      }
      newIndex++;
    }

    if (lastOK) {
      newLen.back() = legLengths.back();
    }
    swap(newLen, legLengths);
  }

  for (int k = 0; !changed && k<nControls; k++)
    changed |= oldC[k] != Controls[k]->getId();

  if (changed) {
    updateChanged();

    oe->punchIndex.clear();
  }

  return changed;
}

void oCourse::importLegLengths(const string &legs, bool setChanged)
{
  vector<string> splits;
  split(legs, ";", splits);

  bool changed = false;

  if (legLengths.size() != splits.size()) {
    legLengths.resize(splits.size());
    changed = true;
  }
  for (size_t k = 0; k<legLengths.size(); k++) {
    int val = atoi(splits[k].c_str());
    if (legLengths[k] != val)
      changed = true;
    legLengths[k] = val;
  }

  if (changed && setChanged) 
    updateChanged();
}

oControl *oCourse::getControl(int index) const
{
  if (index>=0 && index<nControls)
    return Controls[index];
  else return 0;
}

int oCourse::getLegLength(int index) const {
  if (size_t(index) < legLengths.size()) {
    return legLengths[index];
  }
  return 0;
}

bool oCourse::fillCourse(gdioutput &gdi, const string &name)
{
  int finishIx = useLastAsFinish() ? nControls - 1 : -1;
  int startIx = useFirstAsStart() ? 0 : -1;

  oPunchList::iterator it;
  bool rogaining = hasRogaining();
  gdi.clearList(name);
  int offset = 1;
  if (startIx == -1)
    gdi.addItem(name, lang.tl("Start"), -1);
  for (int k=0;k<nControls;k++) {
    wstring c = Controls[k]->getString();
    if (c.length() > 32)
      c= c.substr(0, 32) + L"...";
    if (k == startIx)
      c += L" (" + lang.tl("Start") + L")";
    else if (k == finishIx)
      c += L" (" + lang.tl("Mål") + L")";

    int multi = Controls[k]->getNumMulti();
    int submulti = 0;
    wchar_t bf[256];
    if (Controls[k]->isRogaining(rogaining)) {
      swprintf_s(bf, 64, L"R\t%s", c.c_str());
      offset--;
    }
    else if (multi == 1) {
      swprintf_s(bf, 64, L"%d\t%s", k+offset, c.c_str());
    }
    else
      swprintf_s(bf, 64, L"%d%c\t%s", k+offset, 'A', c.c_str());
    gdi.addItem(name, bf, k);
    while (multi>1) {
      submulti++;
      swprintf_s(bf, 64, L"%d%c\t-:-", k+offset, 'A'+submulti);
      gdi.addItem(name, bf, -1);
      multi--;
    }
    offset += submulti;
  }
  if (finishIx == -1)
    gdi.addItem(name, lang.tl("Mål"), -1);

  return true;
}

void oCourse::getControls(vector<pControl> &pc)
{
  pc.clear();
  pc.reserve(nControls);
  for(int k=0;k<nControls;k++){
    pc.push_back(Controls[k]);
  }
}

int oCourse::distance(const SICard &card)
{
  int matches=0;

  set<int> rogaining;
  vector< map<int, int> > allowedControls;
  allowedControls.reserve(nControls);
  set<int> commonCode;
  if (hasRogaining()) {
    for (int k=0;k<nControls;k++) {
      if (Controls[k]->isRogaining(true)) {
        for (int j = 0; j < Controls[k]->nNumbers; j++)
          rogaining.insert(Controls[k]->Numbers[j]);
      }
    }
  }

  int toMatch = 0;
  size_t orderIndex = 0;
  for (int k=0;k<nControls;k++) {
    if (Controls[k]->isRogaining(hasRogaining()) || 
        Controls[k]->getStatus() == oControl::StatusBad || 
        Controls[k]->getStatus() == oControl::StatusOptional)
      continue;

    if (Controls[k]->getStatus() == oControl::StatusMultiple) {
      for (int j = 0; j < Controls[k]->nNumbers; j++) {
        if (allowedControls.size() <= orderIndex)
          allowedControls.resize(orderIndex+1);
        for (int i = 0; i < Controls[k]->nNumbers; i++) {
          ++allowedControls[orderIndex][Controls[k]->Numbers[i]];
        }
        orderIndex++;
        toMatch++;
      }
    }
    else {
      if (allowedControls.size() <= orderIndex)
        allowedControls.resize(orderIndex+1);

      for (int j = 0; j < Controls[k]->nNumbers; j++) {
        ++allowedControls[orderIndex][Controls[k]->Numbers[j]];
      }
      orderIndex++;
      toMatch++;
    }

    if (getCommonControl() == Controls[k]->getId()) {
      orderIndex = 0;
      commonCode.insert(Controls[k]->Numbers, Controls[k]->Numbers+Controls[k]->nNumbers);
    }
  }

  size_t matchIndex = 0;
  for (unsigned k=0; k<card.nPunch && matches < toMatch; k++) {
    for (unsigned j = k; j < card.nPunch; j++) {
      if (matchIndex < allowedControls.size() &&
             allowedControls[matchIndex].count(card.Punch[j].Code) &&
             allowedControls[matchIndex][card.Punch[j].Code] > 0) {
        --allowedControls[matchIndex][card.Punch[j].Code];
        k = j;
        matches++;
        break;
      }
    }
    matchIndex++;
    if (commonCode.count(card.Punch[k].Code))
      matchIndex = 0;
  }

  if (matches==toMatch) {
    //This course is OK. Extra controls?
    return card.nPunch-toMatch; //Positive return
  }
  else {
    return matches-toMatch; //Negative return;
  }
  return 0;
}

wstring oCourse::getLengthS() const
{
  return itow(getLength());
}

void oCourse::setName(const wstring &n)
{
  if (Name!=n){
    Name=n;
    updateChanged();
  }
}

void oCourse::setLength(int le)
{
  if (le<0 || le > 1000000)
    le = 0;

  if (Length!=le){
    Length=le;
    updateChanged();
  }
}

oDataContainer &oCourse::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = 0;
  return *oe->oCourseData;
}

pCourse oEvent::getCourseCreate(int Id)
{
  oCourseList::iterator it;
  for (it=Courses.begin(); it != Courses.end(); ++it) {
    if (it->Id==Id)
      return &*it;
  }
  if (Id>0) {
    oCourse c(this, Id);
    c.setName(getAutoCourseName());
    return addCourse(c);
  }
  else {
    return addCourse(getAutoCourseName());
  }
}

pCourse oEvent::getCourse(int Id) const {
  if (Id==0)
    return 0;

  pCourse value;
  if (courseIdIndex.lookup(Id,value))
    return value;

  return 0;
}

pCourse oEvent::getCourse(const wstring &n) const {
  oCourseList::const_iterator it;

  for (it=Courses.begin(); it != Courses.end(); ++it) {
    if (it->Name==n)
      return pCourse(&*it);
  }
  return 0;
}

void oEvent::fillCourses(gdioutput &gdi, const string &id, bool simple)
{
  vector< pair<wstring, size_t> > d;
  oe->fillCourses(d, simple);
  gdi.addItem(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillCourses(vector< pair<wstring, size_t> > &out, bool simple)
{
  out.clear();
  oCourseList::iterator it;
  synchronizeList(oListId::oLCourseId);

  Courses.sort();

  vector< pair<pCourse, pair<pCourse, bool>> > ac;
  ac.reserve(Courses.size());
  map<int,int> id2ix;
  for (it=Courses.begin(); it != Courses.end(); ++it) {
    if (!it->Removed){
      id2ix[it->getId()] = ac.size();
      ac.push_back(make_pair(pCourse(&*it), make_pair(pCourse(0), false)));
    }
  }

  for (size_t k = 0; k < ac.size(); k++) {
    pCourse sh = ac[k].first->getShorterVersion().second;
    if (sh != 0) {
      int ix = id2ix[sh->getId()];
      if (!ac[ix].second.first)
        ac[ix].second.first = ac[k].first;
      else
        ac[ix].second.second = true;
    }
  }

  wstring b;
  for (size_t k = 0; k < ac.size(); k++) {
    pCourse it = ac[k].first;

    if (simple) //gdi.addItem(name, it->Name, it->Id);
      out.push_back(make_pair(it->Name, it->Id));
    else {
      b = it->Name;
      if (ac[k].second.first) {
        b += L" < " + ac[k].second.first->Name;
        if (ac[k].second.second)
          b += L", ...";
      }
      b += L"\t(" + itow(it->nControls) + L")";
      if (!it->getCourseProblems().empty())
        b = L"[!] " + b;
      out.push_back(make_pair(b, it->Id));
    }
  }
  return out;
}

void oCourse::setNumberMaps(int block)
{
  getDI().setInt("NumberMaps", block);
}

int oCourse::getNumberMaps() const
{
  return getDCI().getInt("NumberMaps");
}

int oCourse::getNumUsedMaps(bool noVacant) const {
  if (tMapsUsed == -1)
    oe->calculateNumRemainingMaps(false);

  if (noVacant)
    return tMapsUsedNoVacant;
  else
    return tMapsUsed;
}

void oCourse::setStart(const wstring &start, bool sync)
{
  if (getDI().setString("StartName", start)) {
    if (sync)
      synchronize();
    oClassList::iterator it;
    for (it=oe->Classes.begin();it!=oe->Classes.end();++it) {
      if (it->getCourse()==this) {
        it->setStart(start);
        if (sync)
          it->synchronize();
      }
    }
  }
}

wstring oCourse::getStart() const
{
  return getDCI().getString("StartName");
}

void oEvent::calculateNumRemainingMaps(bool forceRecalculate) {
  if (!forceRecalculate) {
    if (dataRevision == tCalcNumMapsDataRevision)
      return;
  }

  synchronizeList({ oListId::oLCourseId, oListId::oLClassId, oListId::oLTeamId, oListId::oLRunnerId });
  
  for (auto &cit : Courses) {
    if (cit.isRemoved())
      continue;

    int numMaps = cit.getNumberMaps();
    if (numMaps == 0)
      cit.tMapsRemaining = numeric_limits<int>::min();
    else
      cit.tMapsRemaining = numMaps;

    cit.tMapsUsed = 0;
    cit.tMapsUsedNoVacant = 0;
  }

  for (auto &cit : Classes) {
    if (cit.isRemoved())
      continue;

    int numMaps = cit.getNumberMaps(true);
    if (numMaps == 0)
      cit.tMapsRemaining = numeric_limits<int>::min();
    else
      cit.tMapsRemaining = numMaps;

    cit.tMapsUsed = 0;
    cit.tMapsUsedNoVacant = 0;
  }

  for (oRunnerList::const_iterator it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved() && it->getStatus() != StatusDNS && it->getStatus() != StatusCANCEL) {
      pCourse pc = it->getCourse(false);
      if (pc) {
        if (pc->tMapsRemaining != numeric_limits<int>::min())
          pc->tMapsRemaining--;

        pc->tMapsUsed++;
        if (!it->isVacant())
          pc->tMapsUsedNoVacant++;
      }
      pClass cls = it->getClassRef(true);
      if (cls) {
        if (cls->tMapsRemaining != numeric_limits<int>::min())
          cls->tMapsRemaining--;

        cls->tMapsUsed++;
        if (!it->isVacant())
          cls->tMapsUsedNoVacant++;
      }
    }
  }

  // Count maps used for vacant team positions

  for (oTeamList::const_iterator it=Teams.begin(); it != Teams.end(); ++it) {
    if (!it->isRemoved()) {
      for (size_t j = 0; j < it->Runners.size(); j++) {
        pRunner r = it->Runners[j];
        if (r)
          continue; // Already included

        if (it->Class) {
          it->Class->tMapsUsed++;

          if (it->Class->tMapsRemaining != numeric_limits<int>::min())
            it->Class->tMapsRemaining--;

          const vector<pCourse> &courses = it->Class->MultiCourse[j];
          if (courses.size()>0) {
            int index = it->StartNo;
            if (index > 0)
              index = (index-1) % courses.size();
            pCourse tCrs = courses[index];
            if (tCrs) {
              tCrs->tMapsUsed++;

              if (tCrs->tMapsRemaining != numeric_limits<int>::min())
                tCrs->tMapsRemaining--;
            }
          }
        }
      }
    }    
  }

  tCalcNumMapsDataRevision = dataRevision;
}

int oCourse::getIdSum(int nC) {

  int id = 0;
  for (int k = 0; k<min(nC, nControls); k++)
    id = 31 * id + (Controls[k] ? Controls[k]->getId() : 0);

  if (id == 0)
    return getId();

  return id;
}

void oCourse::setLegLengths(const vector<int> &legs) {
  if (legs.size() == nControls +1 || legs.empty()) {
    bool diff = legs.size() != legLengths.size();
    if (!diff) {
      for (size_t k = 0; k<legs.size(); k++)
        if (legs[k] != legLengths[k])
          diff = true;
    }
    if (diff) {
      updateChanged();
      legLengths = legs;
    }
  }
  else
    throw std::exception("Invalid parameter value");
}

double oCourse::getPartOfCourse(int start, int end) const
{
  if (end == 0)
    end = nControls;

  if (legLengths.size() != nControls +1 || start <= end ||
      unsigned(start) >= legLengths.size() ||
      unsigned(end) >= legLengths.size() || Length==0)
    return 0.0;

  int dist = 0;
  for (int k = start; k<end; k++)
    dist += legLengths[k];

  return max(1.0, double(dist) / double(Length));
}


const wstring &oCourse::getControlOrdinal(int controlIndex) const
{
  if ( (controlIndex + 1 == nControls && useLastAsFinish())  || controlIndex == nControls)
    return lang.tl("Mål");

  if (oe->dataRevision != cacheDataRevision)
    clearCache();

  if (size_t(controlIndex) < cachedControlOrdinal.size() && !cachedControlOrdinal[controlIndex].empty())
    return cachedControlOrdinal[controlIndex];

  if (controlIndex > nControls)
    throw meosException("Invalid index");
  cachedControlOrdinal.resize(nControls);

  int o = useFirstAsStart() ? 0 : 1;
  bool rogaining = hasRogaining();

  for (int k = 0; k<controlIndex && k<nControls; k++) {
    if (Controls[k] && !Controls[k]->isRogaining(rogaining))
      o++;
  }
  cachedControlOrdinal[controlIndex] = itow(o);
  return cachedControlOrdinal[controlIndex];
}

void oCourse::setRogainingPointsPerMinute(int p)
{
  getDI().setInt("RReduction", p);
}

int oCourse::getRogainingPointsPerMinute() const
{
  return getDCI().getInt("RReduction");
}

int oCourse::calculateReduction(int overTime) const
{
  int reduction = 0;
  if (overTime > 0) {
    int method = getDCI().getInt("RReductionMethod");
    if (method == 0) // Linear model
      reduction = (59 + overTime * getRogainingPointsPerMinute()) / 60;
    else // Time (minute) discrete model
      reduction = ((59 + overTime) / 60) * getRogainingPointsPerMinute();
  }
  return reduction;
}


void oCourse::setMinimumRogainingPoints(int p)
{
  cachedHasRogaining = 0;
  getDI().setInt("RPointLimit", p);
}

int oCourse::getMinimumRogainingPoints() const
{
  return getDCI().getInt("RPointLimit");
}

void oCourse::setMaximumRogainingTime(int p)
{
  cachedHasRogaining = 0;
  if (p == NOTIME)
    p = 0;
  getDI().setInt("RTimeLimit", p);
}

int oCourse::getMaximumRogainingTime() const
{
  return getDCI().getInt("RTimeLimit");
}

bool oCourse::hasRogaining() const {
  if (oe->dataRevision != cacheDataRevision)
    clearCache();

  if (cachedHasRogaining>0)
    return cachedHasRogaining == 2;

  bool r = getMaximumRogainingTime() > 0 || getMinimumRogainingPoints() > 0;
  cachedHasRogaining = r ? 2 : 1;
  return r;
}

void oCourse::clearCache() const {
  cachedHasRogaining = 0;
  cachedControlOrdinal.clear();
  cacheDataRevision = oe->dataRevision;
  oe->tCalcNumMapsDataRevision = -1;
  tMapsUsed = -1;
  tMapsUsedNoVacant = -1;
}

wstring oCourse::getCourseProblems() const
{
  int max_time = getMaximumRogainingTime();
  int min_point = getMinimumRogainingPoints();

  if (max_time > 0) {
    for (int k = 0; k<nControls; k++) {
      if (Controls[k]->isRogaining(true))
        return L"";
    }
    return L"Banan saknar rogainingkontroller.";
  }
  else if (min_point > 0) {
    int max_p = 0;
    for (int k = 0; k<nControls; k++) {
      if (Controls[k]->isRogaining(true))
        max_p += Controls[k]->getRogainingPoints();
    }

    if (max_p < min_point) {
      return L"Banans kontroller ger för få poäng för att täcka poängkravet.";
    }
  }
  return L"";
}

void oCourse::remove()
{
  if (oe)
    oe->removeCourse(Id);
}

bool oCourse::canRemove() const
{
  return !oe->isCourseUsed(Id);
}

void oCourse::changeId(int newId) {
  pCourse old = oe->courseIdIndex[Id];
  if (old == this)
    oe->courseIdIndex.remove(Id);

  oBase::changeId(newId);

  oe->courseIdIndex[newId] = this;
}

bool oCourse::useFirstAsStart() const {
  return getDCI().getInt("FirstAsStart") != 0;
}

bool oCourse::useLastAsFinish() const {
  return getDCI().getInt("LastAsFinish") != 0;
}

void oCourse::firstAsStart(bool f) {
  getDI().setInt("FirstAsStart", f ? 1:0);
}

void oCourse::lastAsFinish(bool f) {
  getDI().setInt("LastAsFinish", f ? 1:0);
}

int oCourse::getFinishPunchType() const {
  if (useLastAsFinish() && nControls > 0)
    return Controls[nControls - 1]->Numbers[0];
  else
    return oPunch::PunchFinish;
}

int oCourse::getStartPunchType() const {
  if (useFirstAsStart() && nControls > 0)
    return Controls[0]->Numbers[0];
  else
    return oPunch::PunchStart;
}

void oEvent::getCourses(vector<pCourse> &crs) const{
  crs.clear();
  for (oCourseList::const_iterator it = Courses.begin(); it != Courses.end(); ++it) {
    if (it->isRemoved())
      continue;
    crs.push_back(pCourse(&*it));
  }
}

int oCourse::getCommonControl() const {
  return getDCI().getInt("CControl");
}


int oCourse::getNumLoops() const {
  int cc = getCommonControl();
  if (cc == 0)
    return 0;
  bool wasCC = true;
  int loopCount = 0;
  for (int i = 0; i < nControls; i++) {
    if (Controls[i]->getId() == cc)
      wasCC = true;
    else if (wasCC) {
      loopCount++;
      wasCC = false;
    }
  }
  return loopCount;
}

void oCourse::setCommonControl(int ctrlId) {
  if (ctrlId != 0) {
    int found = 0;
    for (int k = 0; k < nControls; k++) {
      if (Controls[k]->getId() == ctrlId)
        found++;
    }
    if (found == 0)
      throw meosException("Kontroll X finns inte på banan#" + itos(ctrlId));
  }
  getDI().setInt("CControl", ctrlId);
}

pCourse oCourse::getAdapetedCourse(const oCard &card, oCourse &tmpCourse, int &numShorten) const {
  /*adaptedToOriginalCardOrder.resize(nControls + 1);
  for (int k = 0; k < nControls + 1; k++)
    adaptedToOriginalCardOrder[k] = k;*/
  int cc = getCommonControl();
  if (cc == 0)
    return pCourse(this);

  vector<int> ccIndex;
  vector<vector<pControl>> loopKeys;
  if (!constructLoopKeys(cc, loopKeys, ccIndex))
    return pCourse(this);
  
  bool firstAsStart = ccIndex[0] == 0;

  vector<vector<int>> punchSequence;

  vector<pPunch> punches;
  card.getPunches(punches);

  punchSequence.push_back(vector<int>());
  for (size_t k = 0; k < punches.size(); k++) {
    int code = punches[k]->getTypeCode();
    if (code < 10)
      continue; // Start, Finish etc.
    if (code == cc && !punchSequence.back().empty())
      punchSequence.push_back(vector<int>());
    else {
      if (code != cc)
        punchSequence.back().push_back(code);
    }
  }

  map<int, vector< pair<int,int> > > preferences;
  for (size_t k = 0; k < punchSequence.size(); k++) {
    for (size_t j = 0; j < loopKeys.size(); j++) {
      int v = matchLoopKey(punchSequence[k], loopKeys[j]);
      if (v < 1000)
        preferences[v].push_back(make_pair(k, j));
    }
  }

  vector<int> assignedKeys(loopKeys.size(), -1);
  vector<int> usedPunches(punchSequence.size());
  int assigned = 0;
  for (map<int, vector< pair<int,int> > >::iterator it = preferences.begin(); it != preferences.end(); ++it) {
    vector< pair<int,int> > &bestMatches = it->second;
    map<int, vector<int> > sortedBestMatches;
    vector< pair<int, int> > sortKey(loopKeys.size());
    for (size_t j = 0; j < bestMatches.size(); j++) {
      int loopIndex = bestMatches[j].second;
      sortKey[loopIndex].second = loopIndex;
      ++sortKey[loopIndex].first;
      sortedBestMatches[loopIndex].push_back(bestMatches[j].first);
    }

    sort(sortKey.begin(), sortKey.end());

    for (size_t j = 0; j < sortKey.size(); j++) {
      if (sortKey[j].first == 0)
        continue;
      int loopIndex = sortKey[j].second;
      if (assignedKeys[loopIndex] != -1)
        continue;
      vector<int> &bm = sortedBestMatches[loopIndex];
      for (size_t k = 0; k < bm.size(); k++) {
        if (usedPunches[bm[k]] == 0) {
          usedPunches[bm[k]] = 1;
          assignedKeys[loopIndex] = bm[k];
          assigned++;
          break;
        }
      }
      if (assigned == assignedKeys.size())
        break;
    }
    if (assigned == assignedKeys.size())
      break;
  }

  vector<int> loopOrder;
  map<int, int> keyToIndex;
  assert(ccIndex.size() == assignedKeys.size());
  for (size_t k = 0; k < assignedKeys.size(); k++) {
    if (assignedKeys[k] != -1) {
      keyToIndex[assignedKeys[k]] = k;
    }
  }

  for (map<int, int>::iterator it = keyToIndex.begin(); it != keyToIndex.end(); ++it) {
    loopOrder.push_back(it->second);
  }

  int checksum = (ccIndex.size() * (ccIndex.size()-1))/2;

  // Add remaining, unmatched, loops in defined order
  for (size_t k = 0; k < ccIndex.size(); k++) {
    if (assignedKeys[k] == -1)
      loopOrder.push_back(k);
    checksum-=loopOrder[k];
  }
  assert(checksum == 0 && loopOrder.size() == ccIndex.size());

  tmpCourse.cacheDataRevision = cacheDataRevision;
  tmpCourse.cachedControlOrdinal.clear();
  tmpCourse.cachedHasRogaining = cachedHasRogaining;
  memcpy(tmpCourse.oData, oData, sizeof(oData));
  tmpCourse.nControls = 0;
  tmpCourse.Length = Length;
  tmpCourse.Name = Name;
  tmpCourse.sqlUpdated = "TMP"; // Mark as tmp to prevent accidental write to DB
  tmpCourse.tMapToOriginalOrder.clear();
  tmpCourse.tMapToOriginalOrder.reserve(nControls+1);

  if (firstAsStart) {
    tmpCourse.tMapToOriginalOrder.push_back(0);
    tmpCourse.Controls[tmpCourse.nControls++] = Controls[0];
    if (0 < legLengths.size())
      tmpCourse.legLengths.push_back(legLengths[0]);
  }
  bool allowShorten = getShorterVersion().first;
  numShorten = 0;
  bool lastAsFinish = useLastAsFinish() || Controls[nControls-1]->getId() == cc;

  int endIx = lastAsFinish ? nControls - 1 : nControls;

  for (size_t k = 0; k< loopOrder.size(); k++) {
    if (allowShorten && assignedKeys[loopOrder[k]] == -1) {
      numShorten++;
      continue;
    }
    int start = ccIndex[loopOrder[k]];
    int end = size_t(loopOrder[k] + 1) < ccIndex.size() ? ccIndex[loopOrder[k] + 1] : endIx;
    for (int i = start + 1; i < end; i++) {
      tmpCourse.tMapToOriginalOrder.push_back(i);
      tmpCourse.Controls[tmpCourse.nControls++] = Controls[i];
      if (size_t(i) < legLengths.size())
        tmpCourse.legLengths.push_back(legLengths[i]);
    }
    tmpCourse.tMapToOriginalOrder.push_back(end);
    if (k + 1 < loopOrder.size()) {
      int currentCC = ccIndex[k+1];
      tmpCourse.Controls[tmpCourse.nControls++] = Controls[currentCC];
      if (size_t(end) < legLengths.size())
        tmpCourse.legLengths.push_back(legLengths[end]);
    }
  }

  if (numShorten > 0) {
    //Shortened course. Do not duplicate last.
    if (lastAsFinish && !useLastAsFinish()) {
      lastAsFinish = false;  
    }
    else {
      if (tmpCourse.nControls > 0) {
        tmpCourse.tMapToOriginalOrder.pop_back();
        tmpCourse.nControls--;
        if (!legLengths.empty())
          tmpCourse.legLengths.pop_back();
      }
    }
  }
  if (lastAsFinish) {
    tmpCourse.tMapToOriginalOrder.push_back(nControls);
    tmpCourse.Controls[tmpCourse.nControls++] = Controls[nControls - 1];
    if (size_t(nControls-1) < legLengths.size())
      tmpCourse.legLengths.push_back(legLengths[nControls-1]);
  }

  if (!allowShorten) {
    assert(tmpCourse.nControls == nControls);
    assert(tmpCourse.tMapToOriginalOrder.size() == nControls + 1);
  }

  if (!legLengths.empty()) {
    tmpCourse.legLengths.push_back(legLengths.back());
    if (!allowShorten) {
      assert(tmpCourse.legLengths.size() == legLengths.size());
    }
  }
  tmpCourse.Id = Id;
  return &tmpCourse;
}

bool oCourse::isAdapted() const {
  return tMapToOriginalOrder.size() > 0;
}

int oCourse::getAdaptionId() const {
  int key = 0;
  for (size_t j = 0; j < tMapToOriginalOrder.size(); j++)
    key = key * 97 + tMapToOriginalOrder[j];
  return key;
}

int oCourse::matchLoopKey(const vector<int> &punches, const vector<pControl> &key) {
  if (key.empty())
    return 999;
  size_t ix = -1;
  for (size_t k = 0; k < key.size(); k++) {
    int code = key[k]->getFirstNumber();
    while (++ix < punches.size()) {
      if (punches[ix] == code) {
        code = -1;
        break;
      }
    }
    if (code != -1)
      return 1000;
  }
  return ix;
}

bool oCourse::constructLoopKeys(int cc, vector< vector<pControl> > &loopKeys, vector<int> &ccIndex) const {
  bool firstAsStart = useFirstAsStart();
  if (firstAsStart) { // Only if it is a unique control
    for (int k = 1; k < nControls; k++) {
      if (Controls[k] == Controls[0]) {
        firstAsStart = false;
        break;
      }
    }
  }

  if (Controls[0]->getId() == cc)
    firstAsStart = true; // Handle a course that starts with the loop control

  bool lastAsFinish = useLastAsFinish();
  if (lastAsFinish) { // Only if it is a unique control
    for (int k = 0; k < nControls - 1; k++) {
      if (Controls[k] == Controls[nControls-1]) {
        lastAsFinish = false;
        break;
      }
    }
  }

  int startIx = firstAsStart ? 1 : 0;
  int endIx = lastAsFinish ? nControls : nControls-1;

  ccIndex.push_back(startIx-1);
  for (int k = startIx; k < endIx; k++) {
    if (Controls[k]->getId() == cc)
      ccIndex.push_back(k);
  }
  if (ccIndex.size() <= 1)
    return false;

  loopKeys.clear();
  loopKeys.resize(ccIndex.size());

  int keyIndex = 1;
  bool changed = true;
  bool enough = false;
  while(changed && !enough) {
    changed = false;
    for (size_t k = 0; k < ccIndex.size(); k++) {
      int keyIx = ccIndex[k] + keyIndex;
      int nextIx = (k + 1) < ccIndex.size() ? ccIndex[k+1] : nControls;
      if (keyIx < nextIx && Controls[keyIx]->isSingleStatusOK() && Controls[keyIx]->nNumbers == 1) {
        loopKeys[k].push_back(Controls[keyIx]);
        changed = true;
      }
    }
    keyIndex++;
    if (changed) {
      enough = false;
      set<__int64> hashes;
      for (size_t k = 0; k < loopKeys.size(); k++) {
        __int64 h = loopKeys[k].size();
        for (size_t j = 0; j < loopKeys[k].size(); j++) {
          h = h * 997 + loopKeys[k][j]->Numbers[0];
        }
        hashes.insert(h);
      }
      enough = hashes.size() == loopKeys.size();
    }
  }

  return enough;
}

void oCourse::changedObject() {
  if (oe)
    oe->globalModification = true;
}

int oCourse::getCourseControlId(int controlIx) const {
  if (controlIx >= nControls) {
    assert(false);
    return -1;
  }

  int id = Controls[controlIx] ? Controls[controlIx]->getId() : 0;
  if (id == 0)
    return 0;

  int count = 0;
  for (int j = 0; j < controlIx; j++) {
    if (Controls[j] && Controls[j]->Id == id)
      count++;
  }

  return oControl::getCourseControlIdFromIdIndex(id, count);
}

wstring oCourse::getRadioName(int courseControlId) const {
  pair<int,int> idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  pControl pc = 0;
  int numRadio = 0;
  int clsix = 1;
  for (int k = 0; k < nControls; k++) {
    if (Controls[k]) {
      if (Controls[k]->isValidRadio())
        numRadio++;

      if (Controls[k]->Id == idix.first) {
        if (idix.second == 0) {
          pc = Controls[k];
          break;
        }
        else {
          clsix++;
          idix.second--;
        }
      }
    }
  }

  if (pc == 0)
    return L"?";

  wstring name;
  if (pc->hasName()) {
    name = pc->getName();
    if (pc->getNumberDuplicates() > 1)
      name += makeDash(L"-" + itow(clsix));
  }
  else {
    name = lang.tl("radio X#" + itos(numRadio));
    capitalize(name);
  }

  return name;
}

// Returns the next shorter course, if any, null otherwise
pair<bool, pCourse> oCourse::getShorterVersion() const {
  int ix = getDCI().getInt("Shorten");
  if (ix == -1)
    return make_pair(true, nullptr);
  auto c = oe->getCourse(ix);
  return make_pair(c != 0, c);
}

// Returns the next longer course, if any, null otherwise. Note that this method is slow.
pCourse oCourse::getLongerVersion() const {
  oCourseList::const_iterator it;
  for (it = oe->Courses.begin(); it != oe->Courses.end(); ++it) {
    int ix = it->getDCI().getInt("Shorten");
    if (ix == Id)
      return pCourse(&*it);
  }
  return 0;
}

void oCourse::setShorterVersion(bool activeShortening, pCourse shorten) {
  if (activeShortening)
    getDI().setInt("Shorten", shorten != 0 ? shorten->getId() : -1);
  else
    getDI().setInt("Shorten", 0);
}

bool oCourse::hasControl(const oControl *ctrl) const {
  for (int i = 0; i < nControls; i++) {
    if (Controls[i] == ctrl)
      return true;
  }
  return false;
}

bool oCourse::hasControlCode(int code) const {
  for (int i = 0; i < nControls; i++) {
    if (Controls[i]->hasNumber(code))
      return true;
  }
  return false;
}

void oCourse::getClasses(vector<pClass> &usageClass) const  {
  vector<pClass> cls;
  oe->getClasses(cls, false);

  for (size_t k = 0; k < cls.size(); k++) {
    if (cls[k]->usesCourse(*this))
      usageClass.push_back(cls[k]);
  }
}

