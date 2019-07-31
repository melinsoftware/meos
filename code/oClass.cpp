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

// oClass.cpp: implementation of the oClass class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#define DODECLARETYPESYMBOLS

#include <cassert>
#include "oClass.h"
#include "oEvent.h"
#include "Table.h"
#include "meos_util.h"
#include <limits>
#include "Localizer.h"
#include <algorithm>
#include "inthashmap.h"
#include "intkeymapimpl.hpp"
#include "SportIdent.h"
#include "MeOSFeatures.h"
#include "gdioutput.h"
#include "gdistructures.h"
#include "meosexception.h"
#include "random.h"
#include "qualification_final.h"
#include "generalresult.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oClass::oClass(oEvent *poe): oBase(poe)
{
  getDI().initData();
  Course=0;
  Id=oe->getFreeClassId();
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = 0;
  tLegAccTimeToPlace = 0;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;
  parentClass = 0;
}

oClass::oClass(oEvent *poe, int id): oBase(poe)
{
  getDI().initData();
  Course=0;
  if (id == 0)
    id = oe->getFreeClassId();
  Id=id;
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = 0;
  tLegAccTimeToPlace = 0;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;

  parentClass = 0;
}

oClass::~oClass()
{
  if (tLegTimeToPlace)
    delete tLegTimeToPlace;
  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
}

bool oClass::Write(xmlparser &xml)
{
  if (Removed) return true;
  xml.startTag("Class");

  xml.write("Id", Id);
  xml.write("Updated", Modified.getStamp());
  xml.write("Name", Name);

  if (Course)
    xml.write("Course", Course->Id);

  if (MultiCourse.size()>0)
    xml.write("MultiCourse", codeMultiCourse());

  if (legInfo.size()>0)
    xml.write("LegMethod", codeLegMethod());

  getDI().write(xml);
  xml.endTag();

  return true;
}


void oClass::Set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id = it->getInt();
    }
    else if (it->is("Name")){
      Name = it->getw();
      if (Name.size() > 1 && Name.at(0) == '%') {
        Name = lang.tl(Name.substr(1));
      }
    }
    else if (it->is("Course")){
      Course = oe->getCourse(it->getInt());
    }
    else if (it->is("MultiCourse")){
      set<int> cid;
      vector< vector<int> > multi;
      parseCourses(it->getRaw(), multi, cid);
      importCourses(multi);
    }
    else if (it->is("LegMethod")){
      importLegMethod(it->getRaw());
    }
    else if (it->is("oData")){
      getDI().set(*it);
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRaw());
    }
  }

  // Reinit temporary data
  getNoTiming();
}

void oClass::importCourses(const vector< vector<int> > &multi)
{
  MultiCourse.resize(multi.size());

  for (size_t k=0;k<multi.size();k++) {
    MultiCourse[k].resize(multi[k].size());
    for (size_t j=0; j<multi[k].size(); j++) {
      MultiCourse[k][j] = oe->getCourse(multi[k][j]);
    }
  }
  setNumStages(MultiCourse.size());
}

set<int> &oClass::getMCourseIdSet(set<int> &in) const
{
  in.clear();
  for (size_t k=0;k<MultiCourse.size();k++) {
    for (size_t j=0; j<MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        in.insert(MultiCourse[k][j]->getId());
    }
  }
  return in;
}

string oClass::codeMultiCourse() const
{
  vector< vector<pCourse> >::const_iterator stage_it;
  string str;
  char bf[16];

  for (stage_it=MultiCourse.begin();stage_it!=MultiCourse.end(); ++stage_it) {
    vector<pCourse>::const_iterator it;
    for (it=stage_it->begin();it!=stage_it->end(); ++it) {
      if (*it){
        sprintf_s(bf, 16, " %d", (*it)->getId());
        str+=bf;
      }
      else str+=" 0";
    }
    str += ";";
  }

  if (str.length() == 1)
    return "@"; // Special code for the case of one stage and no course
  else if (str.length()>0) {
    return trim(str.substr(0, str.length()-1));
  }
  //if (str.length()>0)
 //   return trim(str);
  else return "";
}

void oClass::parseCourses(const string &courses,
                          vector< vector<int> > &multi,
                          set<int> &courseId)
{
  courseId.clear();
  multi.clear();
  if (courses.empty())
    return;

  const char *str=courses.c_str();

  vector<int> empty;
  multi.push_back(empty);
  int n_stage=0;

  while (*str && isspace(*str))
    str++;

  while (*str) {
    int cid=atoi(str);

    if (cid) {
      multi[n_stage].push_back(cid);
      courseId.insert(cid);
    }

    while (*str && (*str!=';' && *str!=' ')) str++;

    if (*str==';') {
      str++;
      while (*str && *str==' ') str++;
      n_stage++;
      multi.push_back(empty);
    }
    else {
      if (*str) str++;
    }
  }
}

string oLegInfo::codeLegMethod() const
{
  char bf[256];
  sprintf_s(bf, "(%s:%s:%d:%d:%d:%d)", StartTypeNames[startMethod],
                             LegTypeNames[legMethod],
                             legStartData, legRestartTime,
                             legRopeTime, duplicateRunner);
  return bf;
}

void oLegInfo::importLegMethod(const string &leg)
{
  //Defaults
  startMethod=STTime;
  legMethod=LTNormal;
  legStartData = 0;
  legRestartTime = 0;

  size_t begin=leg.find_first_of('(');

  if (begin==leg.npos)
    return;
  begin++;

  string coreLeg=leg.substr(begin, leg.find_first_of(')')-begin);

  vector< string > legsplit;
  split(coreLeg, ":", legsplit);

  if (legsplit.size()>=1) {
    for( int st = 0 ; st < nStartTypes ; ++st ) {
      if ( legsplit[0]==StartTypeNames[st] ) {
        startMethod=(StartTypes)st;
        break;
      }
    }
  }
  if (legsplit.size()>=2) {
    for( int t = 0 ; t < nLegTypes ; ++t ) {
      if ( legsplit[1]==LegTypeNames[t] ) {
        legMethod=(LegTypes)t;
        break;
      }
    }
  }

  if (legsplit.size()>=3)
    legStartData = atoi(legsplit[2].c_str());

  if (legsplit.size()>=4)
    legRestartTime = atoi(legsplit[3].c_str());

  if (legsplit.size()>=5)
    legRopeTime = atoi(legsplit[4].c_str());

  if (legsplit.size()>=6)
    duplicateRunner = atoi(legsplit[5].c_str());
}

string oClass::codeLegMethod() const
{
  string code;
  for(size_t k=0;k<legInfo.size();k++) {
    if (k>0) code+="*";
    code+=legInfo[k].codeLegMethod();
  }
  return code;
}

wstring oClass::getInfo() const
{
  return L"Klass " + Name;
}

void oClass::importLegMethod(const string &legMethods)
{
  vector< string > legsplit;
  split(legMethods, "*", legsplit);

  legInfo.clear();
  for (size_t k=0;k<legsplit.size();k++) {
    oLegInfo oli;
    oli.importLegMethod(legsplit[k]);
    legInfo.push_back(oli);
  }

  // Ensure we got valid data
  for (size_t k=0;k<legsplit.size();k++) {
    if (legInfo[k].duplicateRunner!=-1) {
      if ( unsigned(legInfo[k].duplicateRunner)<legInfo.size() )
        legInfo[legInfo[k].duplicateRunner].duplicateRunner=-1;
      else
        legInfo[k].duplicateRunner=-1;
    }
  }
  setNumStages(legInfo.size());
  apply();
}

string oClass::getCountTypeKey(int leg, CountKeyType type, bool countVacant) {
  return itos(leg) + ":" + itos(type) + (countVacant ? "V" : "");
}

int oClass::getNumRunners(bool checkFirstLeg, bool noCountVacant, bool noCountNotCompeting) const {
  if (tTypeKeyToRunnerCount.first != oe->dataRevision) {
    for (auto &c : oe->Classes) {
      c.tTypeKeyToRunnerCount.second.clear();
      c.tTypeKeyToRunnerCount.first = oe->dataRevision;
    }
  }
  string key = getCountTypeKey(checkFirstLeg ? 0 : -1, 
                               noCountNotCompeting ? CountKeyType::All : CountKeyType::IncludeNotCompeting,
                               !noCountVacant);

  auto res = tTypeKeyToRunnerCount.second.find(key);
  if (res != tTypeKeyToRunnerCount.second.end())
    return res->second;

  unordered_map<int, int> nRunners;
  for (auto &r : oe->Runners) {
    if (r.isRemoved() || !r.Class)
      continue;
    if (checkFirstLeg && (r.tLeg > 0 && !r.Class->isQualificationFinalBaseClass()))
      continue;
    if (noCountVacant && r.isVacant())
      continue;
    if (noCountNotCompeting && r.getStatus() == StatusNotCompetiting)
      continue;

    int id = r.getClassId(true);
    ++nRunners[id];
  }
  
  for (auto &c : oe->Classes) {
    if (!c.isRemoved())
      c.tTypeKeyToRunnerCount.second[key] = nRunners[c.Id];
  }
  return nRunners[Id];
}

void oClass::getNumResults(int leg, int &total, int &finished, int &dns) const {
  if (tTypeKeyToRunnerCount.first != oe->dataRevision) {
    for (auto &c : oe->Classes) {
      c.tTypeKeyToRunnerCount.second.clear();
      c.tTypeKeyToRunnerCount.first = oe->dataRevision;
    }
  }
  string keyTot = getCountTypeKey(leg, CountKeyType::ExpectedStarting, false);
  string keyFinished = getCountTypeKey(leg, CountKeyType::Finished, false);
  string keyDNS = getCountTypeKey(leg, CountKeyType::DNS, false);

  auto rTot = tTypeKeyToRunnerCount.second.find(keyTot);
  auto rFinished = tTypeKeyToRunnerCount.second.find(keyFinished);
  auto rDNS = tTypeKeyToRunnerCount.second.find(keyDNS);

  if (rTot != tTypeKeyToRunnerCount.second.end() &&
      rFinished != tTypeKeyToRunnerCount.second.end() &&
      rDNS != tTypeKeyToRunnerCount.second.end()) {
    total = rTot->second;
    finished = rFinished->second;
    dns = rDNS->second;
    return;
  }

  struct Cnt {
    bool team = false;
    bool singleClass = false;
    int maxleg = 0;
    int total = 0;
    int finished = 0;
    int dns = 0;
  };

  //Search runners
  unordered_map<int, Cnt> cnt;

  for (auto &c : oe->Classes) {
    if (c.isRemoved())
      continue;

    ClassType ct = c.getClassType();
    auto &cc = cnt[c.Id];
    cc.maxleg = c.getLastStageIndex();
    if (ct == oClassKnockout)
      cc.singleClass = true || cc.maxleg == 1;

    if (!(ct == oClassIndividual || ct == oClassIndividRelay || ct == oClassKnockout))
      cnt[c.Id].team = true;
  }

  for (auto &r : oe->Runners) {
    if (r.isRemoved() || !r.Class || r.tStatus == StatusNotCompetiting || r.tStatus == StatusCANCEL)
      continue;

    auto &c = cnt[r.getClassId(true)];
    if (c.team)
      continue;

    int tleg = leg > 0 ? leg : c.maxleg;

    if (r.tLeg == tleg || c.singleClass) {
      c.total++;

      if (r.tStatus != StatusUnknown)
        c.finished++;
      
      if (r.tStatus == StatusDNS)
        c.dns++;
    }
  }

  for (auto &t : oe->Teams) {
    if (t.isRemoved() || !t.Class || t.tStatus == StatusNotCompetiting || t.tStatus == StatusCANCEL)
      continue;

    auto &c = cnt[t.getClassId(true)];
    if (!c.team)
      continue;

    c.total++;

    if (t.tStatus != StatusUnknown || t.getLegStatus(leg, false) != StatusUnknown)
      c.finished++;
  }

  for (auto &c : oe->Classes) {
    auto &cc = cnt[c.Id];

    c.tTypeKeyToRunnerCount.second[keyDNS] = cc.dns;
    c.tTypeKeyToRunnerCount.second[keyFinished] = cc.finished;
    c.tTypeKeyToRunnerCount.second[keyTot] = cc.total;
  }
  auto &cc = cnt[Id];

  dns = cc.dns;
  total = cc.total;
  finished = cc.finished;
}

void oClass::setCourse(pCourse c)
{
  if (Course!=c){
    if (MultiCourse.size() == 1) {
      // MultiCourse wich is in fact only one course, (e.g. for fixed start time in class). Keep in synch.
      if (c != 0) {
        if (MultiCourse[0].size() == 1)
          MultiCourse[0][0] = c;
        else if (MultiCourse[0].size() == 0)
          MultiCourse[0].push_back(c);
      }
      else {
        if (MultiCourse[0].size() == 1)
          MultiCourse[0].pop_back();
      }
    }
    Course=c;
    tCoursesChanged = true;
    updateChanged();
    // Update start from course
    if (Course && !Course->getStart().empty()) {
      setStart(Course->getStart());
    }
  }
}

void oClass::setName(const wstring &name)
{
  if (getName() != name) {
    Name=name;
    updateChanged();
  }
}

oDataContainer &oClass::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast< vector <vector<wstring> >* >(&oDataStr);
  return *oe->oClassData;
}

pClass oEvent::getClassCreate(int Id, const wstring &createName, set<wstring> &exactMatch) {
  if (Id>0) {
    oClassList::iterator it;
    for (it=Classes.begin(); it != Classes.end(); ++it) {
      if (it->Id==Id && !it->isRemoved()) {

        if (compareClassName(createName, it->getName())) {
          if (it!=Classes.begin())
            Classes.splice(Classes.begin(), Classes, it, Classes.end());
		  return &Classes.front();
        }
        else {
          Id=0; //Bad Id
          break;
        }
      }
    }
  }

  if (createName.empty() && Id>0) {
    oClass c(this, Id);
    c.setName(getAutoClassName());
    return addClass(c);
  }
  else {
	  bool exact = exactMatch.count(createName) > 0;

    //Check if class exist under different id
    for (auto &c : Classes) {
      if (c.isRemoved())
        continue;
      
      if (!exact && exactMatch.count(c.Name) == 0 && compareClassName(c.Name, createName)) {
        return &c;
      }
      if (exact && c.Name == createName) {
        return &c;
      }
    }

    if (Id<=0)
      Id=getFreeClassId();

    oClass c(this, Id);
    c.Name = createName;
    exactMatch.insert(createName);
    //No! Create class with this Id
    pClass pc=addClass(c);

    //Not found. Auto add...
    return pc;
  }
}

bool oEvent::getClassesFromBirthYear(int year, PersonSex sex, vector<int> &classes) const {
  classes.clear();

  int age = year>0 ? getThisYear() - year : 0;

  int bestMatchClass = -1;
  int bestMatchDist = 1000;

  for (oClassList::const_iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    PersonSex clsSex = it->getSex();
    if (clsSex == sFemale && sex == sMale)
      continue;
    if (clsSex == sMale && sex == sFemale)
      continue;

    int distance = 1000;
    if (age>0) {
      int high, low;
      it->getAgeLimit(low, high);

      if (high>0 && age>high)
        continue;

      if (low>0 && age<low)
        continue;

      if (high>0)
        distance = high - age;

      if (low>0)
        distance = min(distance, age-low);

      if (distance < bestMatchDist) {
        if (bestMatchClass != -1)
          classes.push_back(bestMatchClass);
        // Add best class last
        bestMatchClass = it->getId();
        bestMatchDist = distance;
      }
      else
        classes.push_back(it->getId());
    }
    else
      classes.push_back(it->getId());
  }

  // Add best class last
  if (bestMatchClass != -1) {
    classes.push_back(bestMatchClass);
    return true;
  }
  return false;
}



static bool clsSortFunction (pClass i, pClass j) {
  return (*i < *j);
}

void oEvent::getClasses(vector<pClass> &classes, bool sync) const {
  if (sync) {
    oe->synchronizeList(oListId::oLCourseId);
    oe->reinitializeClasses();
  }
  
  classes.clear();
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    classes.push_back(pClass(&*it));
  }

  sort(classes.begin(), classes.end(), clsSortFunction);
}


pClass oEvent::getBestClassMatch(const wstring &cname) const {
  return getClass(cname);
}

pClass oEvent::getClass(const wstring &cname) const
{
  for (oClassList::const_iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->isRemoved() && compareClassName(cname, it->Name))
      return pClass(&*it);
  }
  return 0;
}

pClass oEvent::getClass(int Id) const {
  if (Id<=0)
    return nullptr;

  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (it->Id==Id && !it->isRemoved()) {
      return pClass(&*it);
    }
  }
  return nullptr;
}

pClass oEvent::addClass(const wstring &pname, int CourseId, int classId)
{
  if (classId > 0){
    pClass pOld=getClass(classId);
    if (pOld)
      return 0;
  }

  oClass c(this, classId);
  c.Name=pname;

  if (CourseId>0)
    c.Course=getCourse(CourseId);

  Classes.push_back(c);
  Classes.back().addToEvent();
  Classes.back().synchronize();
  updateTabs();
  return &Classes.back();
}

pClass oEvent::addClass(oClass &c)
{
  if (c.Id==0)
    return 0;
  else {
    pClass pOld=getClass(c.getId());
    if (pOld)
      return 0;
  }

  Classes.push_back(c);
  Classes.back().addToEvent();

  if (!Classes.back().existInDB() && !c.isImplicitlyCreated()) {
    Classes.back().changed = true;
    Classes.back().synchronize();
  }
  return &Classes.back();
}

bool oClass::fillStageCourses(gdioutput &gdi, int stage,
                              const string &name) const
{
  if (unsigned(stage)>=MultiCourse.size())
    return false;

  gdi.clearList(name);
  const vector<pCourse> &Stage=MultiCourse[stage];
  vector<pCourse>::const_iterator it;
  string out;
  string str="";
  wchar_t bf[128];
  int m=0;

  for (it=Stage.begin(); it!=Stage.end(); ++it) {
    swprintf_s(bf, L"%d: %s", ++m, (*it)->getName().c_str());
    gdi.addItem(name, bf, (*it)->getId());
  }

  return true;
}

bool oClass::addStageCourse(int iStage, int courseId, int index)
{
  return addStageCourse(iStage, oe->getCourse(courseId), index);
}

bool oClass::addStageCourse(int iStage, pCourse pc, int index)
{
  if (unsigned(iStage)>=MultiCourse.size())
    return false;

  vector<pCourse> &stage=MultiCourse[iStage];

  if (pc) {
    tCoursesChanged = true;
    if (index == -1 || size_t(index) >= stage.size())
      stage.push_back(pc);
    else {
      stage.insert(stage.begin() + index, pc);
    }
    updateChanged();
    return true;
  }
  return false;
}

bool oClass::moveStageCourse(int stage, int index, int offset) {
  if (unsigned(stage) >= MultiCourse.size())
    return false;

  vector<pCourse> &stages = MultiCourse[stage];

  if (offset == -1 && size_t(index) < stages.size() && index > 0) {
    swap(stages[index - 1], stages[index]);
    updateChanged();
    return true;
  }
  else if (offset == 1 && size_t(index + 1) < stages.size() && index >= 0) {
    swap(stages[index + 1], stages[index]);
    updateChanged();
    return true;
  }
  return false;
}


void oClass::clearStageCourses(int stage) {
  if (size_t(stage) < MultiCourse.size())
    MultiCourse[stage].clear();
}

bool oClass::removeStageCourse(int iStage, int CourseId, int position)
{
  if (unsigned(iStage)>=MultiCourse.size())
    return false;

  vector<pCourse> &Stage=MultiCourse[iStage];

  if ( !(DWORD(position)<Stage.size()))
    return false;

  if (Stage[position]->getId()==CourseId){
    tCoursesChanged = true;
    Stage.erase(Stage.begin()+position);
    updateChanged();
    return true;
  }

  return false;
}

void oClass::setNumStages(int no)
{
  if (no>=0) {
    if (MultiCourse.size() != no)
      updateChanged();
    MultiCourse.resize(no);
    legInfo.resize(no);
    tLeaderTime.resize(max(no, 1));
  }
  oe->updateTabs();
}

void oClass::getTrueStages(vector<oClass::TrueLegInfo > &stages) const
{
  stages.clear();
  if (!legInfo.empty()) {
    for (size_t k = 0; k+1 < legInfo.size(); k++) {
      if (legInfo[k].trueLeg != legInfo[k+1].trueLeg) {
        stages.push_back(TrueLegInfo(k, legInfo[k].trueLeg));
      }
    }
    stages.push_back(TrueLegInfo(legInfo.size()-1, legInfo.back().trueLeg));

    for (size_t k = 0; k <stages.size(); k++) {
      stages[k].nonOptional = k > 0 ? stages[k-1].first + 1: 0;
      while(stages[k].nonOptional <= stages[k].first) {
        if (!legInfo[stages[k].nonOptional].isOptional())
          break;
        else
          stages[k].nonOptional++;
      }
    }
  }
  else {
    stages.push_back(TrueLegInfo(0,1));
    stages.back().nonOptional = -1;
  }

}

bool oClass::startdataIgnored(int i) const
{
  StartTypes st=getStartType(i);
  LegTypes lt=getLegType(i);

  if (lt==LTIgnore || lt==LTExtra || lt==LTParallel || lt == LTParallelOptional)
    return true;

  if (st==STChange || st==STDrawn)
    return true;

  return false;
}

bool oClass::restartIgnored(int i) const
{
  StartTypes st=getStartType(i);
  LegTypes lt=getLegType(i);

  if (lt==LTIgnore || lt==LTExtra || lt==LTParallel || lt == LTParallelOptional || lt == LTGroup)
    return true;

  if (st==STTime || st==STDrawn)
    return true;

  return false;
}

void oClass::fillStartTypes(gdioutput &gdi, const string &name, bool firstLeg)
{
  gdi.clearList(name);

  gdi.addItem(name, lang.tl("Starttid"), STTime);
  if (!firstLeg)
    gdi.addItem(name, lang.tl("Växling"), STChange);
  gdi.addItem(name, lang.tl("Tilldelad"), STDrawn);
  if (!firstLeg)
    gdi.addItem(name, lang.tl("Jaktstart"), STHunting);
}

StartTypes oClass::getStartType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].startMethod;
  else return STDrawn;
}

LegTypes oClass::getLegType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legMethod;
  else return LTNormal;
}

int oClass::getStartData(int leg) const
{
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legStartData;
  else return 0;
}

int oClass::getRestartTime(int leg) const
{
  leg = mapLeg(leg);

  if (leg > 0 && (isParallel(leg) || isOptional(leg)) )
    return getRestartTime(leg-1);

  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legRestartTime;
  else return 0;
}


int oClass::getRopeTime(int leg) const {
  leg = mapLeg(leg);

  if (leg > 0 && (isParallel(leg) || isOptional(leg)) )
    return getRopeTime(leg-1);
  if (unsigned(leg)<legInfo.size()) {
    return legInfo[leg].legRopeTime;
  }
  else return 0;
}


wstring oClass::getStartDataS(int leg) const
{
  leg = mapLeg(leg);

  int s=getStartData(leg);
  StartTypes t=getStartType(leg);

  if (t==STTime || t==STHunting) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STChange || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

wstring oClass::getRestartTimeS(int leg) const
{
  leg = mapLeg(leg);

  int s=getRestartTime(leg);
  StartTypes t=getStartType(leg);

  if (t==STChange || t==STHunting) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STTime || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

wstring oClass::getRopeTimeS(int leg) const
{
  leg = mapLeg(leg);

  int s=getRopeTime(leg);
  StartTypes t=getStartType(leg);

  if (t==STChange || t==STHunting) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STTime || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

int oClass::getLegRunner(int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    if (legInfo[leg].duplicateRunner==-1)
      return leg;
    else
      return legInfo[leg].duplicateRunner;

  return leg;
}

int oClass::getLegRunnerIndex(int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    if (legInfo[leg].duplicateRunner==-1)
      return 0;
    else {
      int base=legInfo[leg].duplicateRunner;
      int index=1;
      for (int k=base+1;k<leg;k++)
        if (legInfo[k].duplicateRunner==base)
          index++;
      return index;
    }

  return leg;
}


void oClass::setLegRunner(int leg, int runnerNo)
{
  bool changed=false;
  if (leg==runnerNo)
    runnerNo=-1; //Default
  else {
    if (runnerNo<leg) {
      setLegRunner(runnerNo, runnerNo);
    }
    else {
      setLegRunner(runnerNo, leg);
      runnerNo=-1;
    }
  }

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].duplicateRunner!=runnerNo;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].duplicateRunner=runnerNo;

  if (changed)
    updateChanged();
}

bool oClass::checkStartMethod() {
  StartTypes st = STTime;
  bool error = false;
  for (size_t j = 0; j < legInfo.size(); j++) {
    if (!legInfo[j].isParallel())
      st = legInfo[j].startMethod;
    else if ((legInfo[j].startMethod == STChange || legInfo[j].startMethod == STHunting) && st != legInfo[j].startMethod) {
      legInfo[j].startMethod = STDrawn;
      error = true;
    }
  }
  return error;
}

void oClass::setStartType(int leg, StartTypes st, bool throwError)
{
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].startMethod!=st;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].startMethod=st;

  bool error = checkStartMethod();

  if (changed || error)
    updateChanged();

  if (error && throwError) {
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg+1));
  }
}

void oClass::setLegType(int leg, LegTypes lt)
{
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legMethod!=lt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].legMethod=lt;

  bool error = checkStartMethod();

  if (changed || error) {
    apply();
    updateChanged();
  }

  if (error) {
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg+1));
  }
}

bool oClass::setStartData(int leg, const wstring &s) {
  int rt;
  StartTypes styp=getStartType(leg);
  if (styp==STTime || styp==STHunting)
    rt=oe->getRelativeTime(s);
  else
    rt=_wtoi(s.c_str());

  return setStartData(leg, rt);
}

bool oClass::setStartData(int leg, int value) {
  bool changed = false;
  if (unsigned(leg)<legInfo.size())
    changed = legInfo[leg].legStartData!=value;
  else if (leg>=0) {
    changed = true;
    legInfo.resize(leg+1);
  }
  legInfo[leg].legStartData = value;

  if (changed)
    updateChanged();
  return changed;
}

void oClass::setRestartTime(int leg, const wstring &t)
{
  int rt=oe->getRelativeTime(t);
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legRestartTime!=rt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }
  legInfo[leg].legRestartTime=rt;

  if (changed)
    updateChanged();
}

void oClass::setRopeTime(int leg, const wstring &t)
{
  int rt=oe->getRelativeTime(t);
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legRopeTime!=rt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }
  legInfo[leg].legRopeTime=rt;

  if (changed)
    updateChanged();
}


void oClass::fillLegTypes(gdioutput &gdi, const string &name)
{
  vector< pair<wstring, size_t> > types;
  types.push_back( make_pair(lang.tl("Normal"), LTNormal));
  types.push_back( make_pair(lang.tl("Parallell"), LTParallel));
  types.push_back( make_pair(lang.tl("Valbar"), LTParallelOptional));
  types.push_back( make_pair(lang.tl("Extra"), LTExtra));
  types.push_back( make_pair(lang.tl("Summera"), LTSum));
  types.push_back( make_pair(lang.tl("Medlöpare"), LTIgnore));
  types.push_back( make_pair(lang.tl("Gruppera"), LTGroup));

  gdi.addItem(name, types);
}

void oEvent::fillClasses(gdioutput &gdi, const string &id, ClassExtra extended, ClassFilter filter)
{
  vector< pair<wstring, size_t> > d;
  oe->fillClasses(d, extended, filter);
  gdi.addItem(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillClasses(vector< pair<wstring, size_t> > &out,
                                                          ClassExtra extended, ClassFilter filter)
{
  set<int> undrawn;
  set<int> hasRunner;
  out.clear();
  if (extended == extraDrawn) {
    oRunnerList::iterator rit;

    for (rit=Runners.begin(); rit != Runners.end(); ++rit) {
      bool needTime = true;
      if (rit->isRemoved())
        continue;

      pClass pc = rit->getClassRef(true);
      if (pc) {
        if (pc->getNumStages() > 0 && pc->getStartType(rit->tLeg) != STDrawn)
          needTime = false;
      }
      if (rit->tStartTime==0 && needTime)
        undrawn.insert(rit->getClassId(true));
      hasRunner.insert(rit->getClassId(true));
    }
  }
  else if (extended == extraNumMaps)
    calculateNumRemainingMaps(false);

  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  reinitializeClasses();
  Classes.sort();//Sort by Id

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed) {

      if (filter==filterOnlyMulti && it->getNumStages()<=1)
        continue;
      else if (filter==filterOnlySingle && it->getNumStages()>1)
        continue;
      else if (filter==filterOnlyDirect && !it->getAllowQuickEntry())
        continue;

      if (extended == extraNone)
        out.push_back(make_pair(it->Name, it->Id));
        //gdi.addItem(name, it->Name, it->Id);
      else if (extended == extraDrawn) {
        wchar_t bf[256];

        if (it->MultiCourse.size() > 0 && it->getStartType(0) == STTime)
          swprintf_s(bf, L"%s\t%s", it->Name.c_str(), it->getStartDataS(0).c_str());
        else if (undrawn.count(it->getId()) || !hasRunner.count(it->getId()))
          swprintf_s(bf, L"%s", it->Name.c_str());
        else {
          swprintf_s(bf, L"%s\t[S]", it->Name.c_str());
        }
        out.push_back(make_pair(wstring(bf), it->Id));
        //gdi.addItem(name, bf, it->Id);
      }
      else if (extended == extraNumMaps) {
        wchar_t bf[256];
        int nmaps = it->getNumRemainingMaps(false);
        if (nmaps != numeric_limits<int>::min())
          swprintf_s(bf, L"%s (%d %s)", it->Name.c_str(), nmaps, lang.tl(L"kartor").c_str());
        else
          swprintf_s(bf, L"%s ( - %s)", it->Name.c_str(), lang.tl(L"kartor").c_str());

        out.push_back(make_pair(wstring(bf), it->Id));
      }
    }
  }
  return out;
}

void oEvent::getNotDrawnClasses(set<int> &classes, bool someMissing)
{
  set<int> drawn;
  classes.clear();

  oRunnerList::iterator rit;

  synchronizeList(oListId::oLRunnerId);
  for (rit=Runners.begin(); rit != Runners.end(); ++rit) {
    if (rit->tStartTime>0)
      drawn.insert(rit->getClassId(true));
    else if (someMissing)
      classes.insert(rit->getClassId(true));
  }

  // Return all classe where some runner has no start time
  if (someMissing)
    return;

  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  // Return classes where no runner has a start time
  for (it=Classes.begin(); it != Classes.end(); ++it) {
    if (drawn.count(it->getId())==0)
      classes.insert(it->getId());
  }
}


void oEvent::getAllClasses(set<int> &classes)
{
  classes.clear();

  oClassList::const_iterator it;
  synchronizeList(oListId::oLClassId);

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed){
      classes.insert(it->getId());
    }
  }
}

bool oEvent::fillClassesTB(gdioutput &gdi)//Table mode
{
  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  reinitializeClasses();
  Classes.sort();//Sort by Id

  int dx[4]={100, 100, 50, 100};
  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  y+=lh/2;

  int xp=x;
  gdi.addString("", y, xp, 0, "Klass"); xp+=dx[0];
  gdi.addString("", y, xp, 0, "Bana"); xp+=dx[1];
  gdi.addString("", y, xp, 0, "Deltagare"); xp+=dx[2];
  y+=(3*lh)/2;

  for (it = Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed){
      int xp=x;

      gdi.addString("", y, xp, 0, it->getName(), dx[0]); xp+=dx[0];

      pCourse pc=it->getCourse();
      if (pc) gdi.addString("", y, xp, 0, pc->getName(), dx[1]);
      else gdi.addString("", y, xp, 0, "-", dx[1]);
      xp+=dx[1];

      char num[10];
      _itoa_s(it->getNumRunners(false, false, false), num, 10);

      gdi.addString("", y, xp, 0, num, dx[2]);
      xp+=dx[2];

      y+=lh;
    }
  }
  return true;
}

bool oClass::isCourseUsed(int Id) const
{
  if (Course && Course->getId()==Id)
    return true;

  if (hasMultiCourse()){
    for(unsigned i=0;i<getNumStages(); i++) {
      const vector<pCourse> &pv=MultiCourse[i];
      for(unsigned j=0; j<pv.size(); j++)
        if (pv[j]->getId()==Id) return true;
    }
  }

  return false;
}

bool oClass::hasTrueMultiCourse() const {
  if (MultiCourse.empty())
    return false;
  return MultiCourse.size()>1 || hasCoursePool() || tShowMultiDialog ||
         (MultiCourse.size()==1 && MultiCourse[0].size()>1);
}


wstring oClass::getLength(int leg) const {
  leg = mapLeg(leg);

  wchar_t bf[64];
  if (hasMultiCourse()){
    int minlen=1000000;
    int maxlen=0;

    for(unsigned i=0;i<getNumStages(); i++) {
      if (i == leg || leg == -1) {
        const vector<pCourse> &pv=MultiCourse[i];
        for(unsigned j=0; j<pv.size(); j++) {
          int l=pv[j]->getLength();
          minlen=min(l, minlen);
          maxlen=max(l, maxlen);
        }
      }
    }

    if (maxlen==0)
      return _EmptyWString;
    else if (minlen==0)
      minlen=maxlen;

    if ( (maxlen-minlen)<100 )
      swprintf_s(bf, L"%d", maxlen);
    else
      swprintf_s(bf, L"%d - %d", minlen, maxlen);

    return makeDash(bf);
  }
  else if (Course && Course->getLength()>0) {
    return Course->getLengthS();
  }
  return _EmptyWString;
}

bool oClass::hasUnorderedLegs() const {
  return getDCI().getInt("Unordered") != 0;
}

void oClass::setUnorderedLegs(bool order) {
  getDI().setInt("Unordered", order);
}

void oClass::getParallelRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const {
  parLegRangeMin = leg;
  while (parLegRangeMin > 0 && size_t(parLegRangeMin) < legInfo.size()) {
    if (legInfo[parLegRangeMin].isParallel())
      parLegRangeMin--;
    else 
      break;
  }
  parLegRangeMax = leg;
  while (size_t(parLegRangeMax+1) < legInfo.size()) {
    if (legInfo[parLegRangeMax+1].isParallel() || legInfo[parLegRangeMax+1].isOptional())
      parLegRangeMax++;
    else 
      break;
  }
}

void oClass::getParallelCourseGroup(int leg, int startNo, vector< pair<int, pCourse> > &group) const {
  group.clear();
  // Assume hasUnorderedLegs
  /*if (!hasUnorderedLegs()) {
    pCourse crs = Course;
    if (leg < MultiCourse.size()) {
      int size = MultiCourse[leg].size();
      if (size > 0) {
        crs = MultiCourse[leg][startNo%leg];
      }
    }
    group.push_back(make_pair(leg, crs));
    return;
  }
  else*/ {
    // Find first leg in group
    while (leg > 0 && size_t(leg) < legInfo.size()) {
      if (legInfo[leg].isParallel())
        leg--;
      else 
        break;
    }
    if (startNo <= 0)
      startNo = 1; // Use first course

    // Fill in all legs in the group
    do {
      if (size_t(leg) < MultiCourse.size()) {
        int size = MultiCourse[leg].size();
        if (size > 0) {
          pCourse crs = MultiCourse[leg][(startNo-1)%size];
          group.push_back(make_pair(leg, crs));
        }
      }
      leg++;
    }
    while (size_t(leg) < legInfo.size() && 
           legInfo[leg].isParallel());
  }
}

pCourse oClass::selectParallelCourse(const oRunner &r, const SICard &sic) {
  synchronize();
  pCourse rc = 0; //Best match course
  vector< pair<int, pCourse> > group;
  getParallelCourseGroup(r.getLegNumber(), r.getStartNo(), group);
  cTeam t = r.getTeam();
  if (t && group.size() > 1) {
    for (size_t k = 0; k < group.size(); k++) {
      pRunner tr = t->getRunner(group[k].first);
      if (!tr) 
        continue;

      tr->synchronize();
      if (tr->Course) {
        // The course is assigned. Remove from group
        for (size_t j = 0; j < group.size(); j++) {
          if (group[j].second == tr->Course) {
            group[j].second = 0;
            break;
          }
        }
      }
    }

    // Select best match of available courses
    int distance=-1000;
    for (size_t k = 0; k < group.size(); k++) {
      if (group[k].second) {
        int d = group[k].second->distance(sic);
        if (d >= 0) {
          if (distance < 0) 
            distance = 1000;

          if (d<distance) {
            distance = d;
            rc = group[k].second;
          }
        }
        else if (distance < 0 && d > distance) {
          distance=d;
          rc = group[k].second;
        }
      }
    }
  }

  return rc;
}


pCourse oClass::getCourse(int leg, unsigned fork, bool getSampleFromRunner) const
{
  leg = mapLeg(leg);

  if (size_t(leg) < MultiCourse.size()) {
    const vector<pCourse> &courses=MultiCourse[leg];
    if (courses.size()>0) {
      int index = fork;
      if (index>0)
        index = (index-1) % courses.size();

      return courses[index];
    }
  }

  if (!getSampleFromRunner)
    return 0;
  else {
    pCourse res = 0;
    for (oRunnerList::iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
      if (it->getClassRef(true) == this && it->Course) {
        if (it->tLeg == leg)
          return it->Course;
        else
          res = it->Course; // Might find better candidate later
      }
    }
    return res;
  }
}

pCourse oClass::getCourse(bool getSampleFromRunner) const {
  pCourse res;
  if (MultiCourse.size() == 1 && MultiCourse[0].size() == 1)
    res = MultiCourse[0][0];
  else
    res = Course;

  if (!res && getSampleFromRunner)
    res = getCourse(0,0, true);

  return res;
}

void oClass::getCourses(int leg, vector<pCourse> &courses) const {
  leg = mapLeg(leg);

  //leg == -1 -> all courses
  courses.clear();
  if (leg <= 0 && Course)
    courses.push_back(Course);

  for (size_t cl = 0; cl < MultiCourse.size(); cl++) {
    if (leg>= 0 && cl != leg)
      continue;
    const vector<pCourse> &mc = MultiCourse[cl];
    for (size_t k = 0; k < mc.size(); k++)
      if (find(courses.begin(), courses.end(), mc[k]) == courses.end())
        courses.push_back(mc[k]);
  }

  // Add shortened versions
  for (size_t k = 0; k < courses.size(); k++) {
    pCourse sht = courses[k]->getShorterVersion().second;
    int maxIter = 10;
    while (sht && --maxIter >= 0 ) {
      if (find(courses.begin(), courses.end(), sht) == courses.end())
        courses.push_back(sht);
      sht = sht->getShorterVersion().second;
    }
  }
}

ClassType oClass::getClassType() const
{
  if (legInfo.size()==2 && (legInfo[1].isParallel() ||
                           legInfo[1].legMethod==LTIgnore) )
    return oClassPatrol;
  else if (legInfo.size()>=2) {
    if (isQualificationFinalBaseClass())
      return oClassKnockout;

    for(size_t k=1;k<legInfo.size();k++)
      if (legInfo[k].duplicateRunner!=0)
        return oClassRelay;

    return oClassIndividRelay;
  }
  else
    return oClassIndividual;
}

int oClass::getNumMultiRunners(int leg) const
{
  int ndup=0;
  for (size_t k=0;k<legInfo.size();k++) {
    if (leg==legInfo[k].duplicateRunner || (legInfo[k].duplicateRunner==-1 && k==leg))
      ndup++;
  }
  if (legInfo.empty())
    ndup++; //If no multi-course, we run at least one race.

  return ndup;
}

int oClass::getNumParallel(int leg) const
{
  int nleg = legInfo.size();
  if (leg>=nleg)
    return 1;

  int nP = 1;
  int i = leg;
  while (++i<nleg && legInfo[i].isParallel())
    nP++;

  i = leg;
  while (i>=0 && legInfo[i--].isParallel())
    nP++;
  return nP;
}

int oClass::getNumDistinctRunners() const
{
  if (legInfo.empty())
    return 1;

  int ndist=0;
  for (size_t k=0;k<legInfo.size();k++) {
    if (legInfo[k].duplicateRunner==-1)
      ndist++;
  }
  return ndist;
}

int oClass::getNumDistinctRunnersMinimal() const
{
  if (legInfo.empty())
    return 1;

  int ndist=0;
  for (size_t k=0;k<legInfo.size();k++) {
    LegTypes lt = legInfo[k].legMethod;
    if (legInfo[k].duplicateRunner==-1 && (lt != LTExtra && lt != LTIgnore && lt != LTParallelOptional) )
      ndist++;
  }
  return max(ndist, 1);
}

void oClass::resetLeaderTime() {
  for (size_t k = 0; k<tLeaderTime.size(); k++)
    tLeaderTime[k].reset();

  tBestTimePerCourse.clear();
}

bool oClass::hasCoursePool() const
{
  return getDCI().getInt("HasPool")!=0;
}

void oClass::setCoursePool(bool p)
{
  if (hasCoursePool() != p) {
    getDI().setInt("HasPool", p);
    tCoursesChanged = true;
  }
}

pCourse oClass::selectCourseFromPool(int leg, const SICard &card) const {
  leg = mapLeg(leg);

  int Distance=-1000;
  const oCourse *rc=0; //Best match course

  if (MultiCourse.size()==0)
    return Course;

  if (unsigned(leg)>=MultiCourse.size())
    return Course;

  // First = course to check, second = course to assign. First could be a shortened version.
  vector<pair<pCourse, pCourse> > layer(MultiCourse[leg].size());

  for (size_t k = 0; k < layer.size(); k++) {
    layer[k].first = MultiCourse[leg][k];
    layer[k].second = MultiCourse[leg][k];
  }

  while (Distance < 0 && !layer.empty()) {

    for (size_t k=0;k < layer.size(); k++) {
      
      if (layer[k].first) {
        int d = layer[k].first->distance(card);

        if (d>=0) {
          if (Distance<0) Distance=1000;

          if (d<Distance) {
            Distance=d;
            rc = layer[k].second;
          }
        }
        else {
          if (Distance<0 && d>Distance) {
            Distance=d;
            rc = layer[k].second;
          }
        }
      }
    }

    if (Distance < 0) {
      // If we have found no acceptable match, try the shortened courses, if any
      vector< pair<pCourse, pCourse> > shortenedLayer;
      for (size_t k=0;k < layer.size(); k++) {
        if (layer[k].first) {
          pCourse sw = layer[k].first->getShorterVersion().second;
          if (sw)
            shortenedLayer.push_back(make_pair(sw, layer[k].second));
        }
      }
      swap(layer, shortenedLayer);
    }
  }

  return const_cast<pCourse>(rc);
}

void oClass::updateChangedCoursePool() {
  if (!tCoursesChanged)
    return;

  bool hasPool = hasCoursePool();
  vector< set<pCourse> > crs;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    crs.push_back(set<pCourse>());
    for (size_t j = 0; j < MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        crs.back().insert(MultiCourse[k][j]);
    }
  }

  SICard card(ConvertedTimeStatus::Unknown);
  oRunnerList::iterator it;
  for (it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->isRemoved() || it->getClassRef(true) != this)
      continue;

    if (size_t(it->tLeg) >= crs.size() || crs[it->tLeg].empty())
      continue;

    if (!hasPool) {
      if (it->Course) {
        it->setCourseId(0);
        it->synchronize();
      }
    }
    else {
      bool correctCourse = crs[it->tLeg].count(it->Course) > 0;
      if ((!correctCourse || (correctCourse && it->tStatus == StatusMP)) && it->Card) {
        it->Card->getSICard(card);
        pCourse crs = selectCourseFromPool(it->tLeg, card);
        if (crs != it->Course) {
          it->setCourseId(crs->getId());
          it->synchronize();
        }
      }
    }
  }
  tCoursesChanged = false;
}

int oClass::getBestLegTime(int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)>=tLeaderTime.size())
    return 0;
  else
    return tLeaderTime[leg].bestTimeOnLeg;
}

int oClass::getBestTimeCourse(int courseId) const
{
  map<int, int>::const_iterator res = tBestTimePerCourse.find(courseId);
  if (res == tBestTimePerCourse.end())
    return 0;
  else
    return res->second;
}

int oClass::getBestInputTime(int leg) const
{
  leg = mapLeg(leg);

  if (unsigned(leg)>=tLeaderTime.size())
    return 0;
  else 
    return tLeaderTime[leg].inputTime;
}

int oClass::getTotalLegLeaderTime(int leg, bool includeInput) const
{
  leg = mapLeg(leg);

  if (unsigned(leg)>=tLeaderTime.size())
    return 0;
  else {
    if (includeInput)
      return tLeaderTime[leg].totalLeaderTimeInput;
    else
      return tLeaderTime[leg].totalLeaderTime;
  }
}

void oClass::mergeClass(int classIdSec) {
  vector<pTeam> t;
  vector<pRunner> r;
  vector<pRunner> rThis;

  oe->getRunners(classIdSec, 0, rThis, true);

  // Update teams
  oe->getTeams(classIdSec, t, true);
  
  for (size_t k = 0; k < t.size(); k++) {
    pTeam it = t[k];
    it->Class = this;
    it->updateChanged();
    for (size_t k=0;k<it->Runners.size();k++)  {
      if (it->Runners[k]) {
        it->Runners[k]->Class = this;
        it->Runners[k]->updateChanged();
      }
    }
    it->synchronize(); //Synchronizes runners also  
  }

  oe->getRunners(classIdSec, 0, r, false);
  // Update runners
  for (size_t k = 0; k < r.size(); k++) {
    pRunner it = r[k];
    it->Class = this;
    it->updateChanged();
    it->synchronize();
  }

  // Check heats
  
  int maxHeatThis = 0;
  bool missingHeatThis = false, uniqueHeatThis = true;
  for (size_t k = 0; k < rThis.size(); k++) {
    int heat = rThis[k]->getDCI().getInt("Heat");
    if (heat == 0)
      missingHeatThis = true;
    if (maxHeatThis != 0 && heat != maxHeatThis)
      uniqueHeatThis = false;
    maxHeatThis = max(maxHeatThis, heat);
  }

  int maxHeatOther = 0;
  bool missingHeatOther = false, uniqueHeatOther = true;
  for (size_t k = 0; k < r.size(); k++) {
    int heat = r[k]->getDCI().getInt("Heat");
    if (heat == 0)
      missingHeatOther = true;
    if (maxHeatOther != 0 && heat != maxHeatOther)
      uniqueHeatOther = false;
    maxHeatOther = max(maxHeatOther, heat);
  }
  int heatForNext = 1;
  if (missingHeatThis) {
    for (size_t k = 0; k < rThis.size(); k++) {
      int heat = rThis[k]->getDCI().getInt("Heat");
      if (heat == 0) {
        if (uniqueHeatThis && maxHeatThis > 0)
          heat = maxHeatThis; // Some runners are missing the heat info. Fill in.
        else {
          // If maxHeatthis> 0, data somehow corrupted:
          // Some runners have heat, but not unqiue, 
          // others are missing. Heats not well defined.
          heat = maxHeatThis + 1; 
        }
      }
      heatForNext = max(heatForNext, heat+1);
      rThis[k]->getDI().setInt("Heat", heat);
    }
  }

  if (missingHeatOther) {
    for (size_t k = 0; k < r.size(); k++) {
      int heat = r[k]->getDCI().getInt("Heat");
      if (heat == 0) {
        if (maxHeatOther == 0)
          heat = heatForNext; // No runner had a heat, set to next heat
        else if (uniqueHeatOther)
          heat = maxHeatOther; // Some runner missing the heat. Use the defined heat.
        else
          heat = maxHeatOther + 1; // Data corrupted, see above. Make a unique heat.
      }
      r[k]->getDI().setInt("Heat", heat);
    }
  }
  // Write back
  for (size_t k = 0; k < t.size(); k++) {
    t[k]->synchronize(true); //Synchronizes runners also  
  }
  for (size_t k = 0; k < r.size(); k++) {
    r[k]->synchronize(true);
  }
  for (size_t k = 0; k < rThis.size(); k++) {
    rThis[k]->synchronize(true);
  }

  oe->removeClass(classIdSec);
}

void oClass::getSplitMethods(vector< pair<wstring, size_t> > &methods) {
  methods.clear();
  methods.push_back(make_pair(lang.tl("Dela klubbvis"), SplitClub));
  methods.push_back(make_pair(lang.tl("Dela slumpmässigt"), SplitRandom));
  methods.push_back(make_pair(lang.tl("Dela efter ranking"), SplitRank));
  methods.push_back(make_pair(lang.tl("Dela efter placering"), SplitResult));
  methods.push_back(make_pair(lang.tl("Dela efter tid"), SplitTime));
  methods.push_back(make_pair(lang.tl("Jämna klasser (ranking)"), SplitRankEven));
  methods.push_back(make_pair(lang.tl("Jämna klasser (placering)"), SplitResultEven));
  methods.push_back(make_pair(lang.tl("Jämna klasser (tid)"), SplitTimeEven));
}

class ClassSplit {
private:
  map<int, int> clubSize;
  map<int, int> idSplit;
  map<int, int> clubSplit;
  vector<const oAbstractRunner*> runners;
  void splitClubs(const vector<int> &parts);
  void valueSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);
  void valueEvenSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);

public: 
  static int evaluateTime(const oAbstractRunner &r) {
    if (r.getInputStatus() == StatusOK) {
      int t = r.getInputTime();
      if (t > 0)
        return t;
      else
        return 3600 * 24 * 8;
    }
    else {
      return 3600 * 24 * 8 + r.getId();
    }
  }

  static int evaluateResult(const oAbstractRunner &r) {
    int baseRes;
    if (r.getInputStatus() == StatusOK) {
      int t = r.getInputPlace();
      
      if (t == 0) {
        const oRunner *rr = dynamic_cast<const oRunner *>(&r);
        if (rr && rr->getTeam() && rr->getLegNumber() > 0) {
          const pRunner rPrev = rr->getTeam()->getRunner(rr->getLegNumber() - 1);
          if (rPrev && rPrev->getStatus() == StatusOK)
            t = rPrev->getPlace();
        }
      }
      
      if (t > 0)
        baseRes = t;
      else
        baseRes = 99999;
    }
    else {
      baseRes = 99999 + r.getInputStatus();
    }
    return r.getDCI().getInt("Heat") + 1000 * baseRes;
  }

  static int evaluatePoints(const oAbstractRunner &r) {
    if (r.getInputStatus() == StatusOK) {
      int p = r.getInputPoints();
      if (p > 0)
        return 1000*1000*1000 - p;
      else
        return 1000*1000*1000;
    }
    else {
      return 1000*1000*1000 + r.getInputStatus();
    }
  }

private:
  int evaluate(const oAbstractRunner &r, ClassSplitMethod method) {
    switch (method) {
      case SplitRank:
      case SplitRankEven:
        return r.getRanking();
      case SplitTime:
      case SplitTimeEven:
        return evaluateTime(r);
      case SplitResult:
      case SplitResultEven:
        return evaluateResult(r);
      default:
       throw meosException("Not yet implemented");
    }
  }
public:
  void addMember(const oAbstractRunner &r) {
    ++clubSize[r.getClubId()];
    runners.push_back(&r);
  }

  void split(const vector<int> &parts, ClassSplitMethod method);

  int getClassIndex(const oAbstractRunner &r) {
    if (clubSplit.count(r.getClubId()))
      return clubSplit[r.getClubId()];
    else if (idSplit.count(r.getId()))
      return idSplit[r.getId()];
    throw meosException("Internal split error");
  }
};

void ClassSplit::split(const vector<int> &parts, ClassSplitMethod method) {
  switch (method) {
    case SplitClub:
      splitClubs(parts);
    break;

    case SplitRank:
    case SplitTime:
    case SplitResult: {
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = evaluate(*runners[k], method);
      }
      valueSplit(parts, v);
    } break;

    case SplitRankEven:
    case SplitTimeEven:
    case SplitResultEven: {
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = evaluate(*runners[k], method);
      }
      valueEvenSplit(parts, v);
    } break;


    case SplitRandom: {
      vector<int> r(runners.size());
      for (size_t k = 0; k < r.size(); k++) {
        r[k] = k;
      }
      permute(r);
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = r[k];
      }
      valueEvenSplit(parts, v);
      break;
    }
    default:
      throw meosException("Not yet implemented");
  }
}

void ClassSplit::splitClubs(const vector<int> &parts) {
  vector<int> classSize(parts);
  while ( !clubSize.empty() ) {
    // Find largest club
    int club=0;
    int size=0;
    for (map<int, int>::iterator it=clubSize.begin(); it!=clubSize.end(); ++it) {
      if (it->second>size) {
        club = it->first;
        size = it->second;
      }
    }
    clubSize.erase(club);
    // Find smallest class (e.g. highest number of remaining)
    int nrunner = -1000000;
    int cid = 0;

    for(size_t k = 0; k < parts.size(); k++) {
      if (classSize[k]>nrunner) {
        nrunner = classSize[k];
        cid = k;
      }
    }

    //Store result
    clubSplit[club] = cid;
    classSize[cid] -= size;
  }
}

void ClassSplit::valueSplit(const vector<int> &parts, vector< pair<int, int> > &valueId) {
  sort(valueId.begin(), valueId.end());

  int partIx = 0;
  int partCount = 0;
  for (size_t k = 0; k < valueId.size(); ) {
    int refValue = valueId[k].first;
    for (; k < valueId.size() && valueId[k].first == refValue; k++) {
      idSplit[valueId[k].second] = partIx;
      partCount++;
    } 

    if (k < valueId.size() && partCount >= parts[partIx] && size_t(partIx + 1) < parts.size()) {
      partIx++;
      partCount = 0;
    }
  }

  if (partIx == 0) {
    throw meosException("error:invalidmethod");
  }
}

void ClassSplit::valueEvenSplit(const vector<int> &parts, vector< pair<int, int> > &valueId) {
  sort(valueId.begin(), valueId.end());
  if (valueId.empty() || valueId.front().first == valueId.back().first) {
    throw meosException("error:invalidmethod");
  }

  vector<int> count(parts.size());
  bool odd = true;
  bool useRandomAssign = false;

  for (size_t k = 0; k < valueId.size(); ) {
    vector<int> distr;

    for (size_t j = 0; k < valueId.size() && j < parts.size(); j++) {
      if (count[j] < parts[j]) {
        distr.push_back(valueId[k++].second);
      }
    }
    if (distr.empty()) {
      idSplit[valueId[k++].second] = parts.size()-1; // Out of space, use last for rest
    }
    else {
      if (useRandomAssign) {
        permute(distr); //Random assignment to groups
      }
      else {
        // Use reverse/forward distribution. Swedish SM rules
        if (odd) 
          reverse(distr.begin(), distr.end());
        odd = !odd;
      }

      for (size_t j = 0; j < parts.size(); j++) {
        if (count[j] < parts[j]) {
          ++count[j];
          idSplit[distr.back()] = j;
          distr.pop_back();
        }
      }
    }
  }
}


void oClass::splitClass(ClassSplitMethod method, const vector<int> &parts, vector<int> &outClassId) {
  if (parts.size() <= 1)
    return;
  bool qf = false;
  set<int> clsIdSrc;
  clsIdSrc.insert(getId());

  if (getQualificationFinal()) {
    // Works for base classes
    set<int> base;
    getQualificationFinal()->getBaseClassInstances(base);
    assert(base.size() == parts.size());
    qf = true;
    for (int inst : base)
      clsIdSrc.insert(getVirtualClass(inst)->getId());
  }

  bool defineHeats = method == SplitRankEven || method == SplitResultEven;
  
  ClassSplit cc;
  vector<pTeam> t;
  vector<pRunner> r;
  
  if (!qf && oe->classHasTeams(getId()) ) {
    for (int clsId : clsIdSrc) {
      vector<pTeam> tTmp;
      oe->getTeams(clsId, tTmp, true);
      for (auto tk : tTmp) {
        t.push_back(tk);
        cc.addMember(*tk);
      }
    }
  }
  else {
    for (int clsId : clsIdSrc) {
      vector<pRunner> rTmp;
      oe->getRunners(clsId, 0, rTmp, true);
      for (auto rk : rTmp) {
        if (qf && rk->getLegNumber() != 0)
          continue;

        r.push_back(rk);
        cc.addMember(*rk);
      }
    }
  }
  
  // Split teams.
  cc.split(parts, method);

  vector<pClass> pcv(parts.size());
  outClassId.resize(parts.size());
  if (qf) {
    set<int> base;
    getQualificationFinal()->getBaseClassInstances(base);
    int ix = 0;
    for (int inst : base) {
      pcv[ix] = getVirtualClass(inst);
      outClassId[ix] = pcv[ix]->getId();
      ix++;
    }
  }
  else {
    pcv[0] = this;
    outClassId[0] = getId();

    pcv[0]->getDI().setInt("Heat", defineHeats ? 1 : 0);
    pcv[0]->synchronize(true);

    int lastSI = getDI().getInt("SortIndex");
    for (size_t k = 1; k < parts.size(); k++) {
      pcv[k] = oe->addClass(getName() + makeDash(L"-") + itow(k + 1), getCourseId());
      if (pcv[k]) {
        // Find suitable sort index
        lastSI = pcv[k]->getSortIndex(lastSI + 1);

        memcpy(pcv[k]->oData, oData, sizeof(oData));

        pcv[k]->getDI().setInt("SortIndex", lastSI);
        pcv[k]->getDI().setInt("Heat", defineHeats ? k + 1 : 0);
        pcv[k]->synchronize();
      }

      outClassId[k] = pcv[k]->getId();
    }

    setName(getName() + makeDash(L"-1"));
    synchronize();
  }

  for (size_t k = 0; k < t.size(); k++) {
    pTeam it = t[k];
    int clsIx = cc.getClassIndex(*it);
    it->Class = pcv[clsIx];
    it->updateChanged();
    for (size_t k=0;k<it->Runners.size();k++) {
      if (it->Runners[k]) {
        if (defineHeats)
          it->getDI().setInt("Heat", clsIx+1);
        it->Runners[k]->Class = it->Class;
        it->Runners[k]->updateChanged();
      }
    }
    it->synchronize(); //Synchronizes runners also
  }

  for (size_t k = 0; k < r.size(); k++) {
    pRunner it = r[k];
    int clsIx = cc.getClassIndex(*it);
    if (qf) {
      it->getDI().setInt("Heat", clsIx + 1);
    }
    else {
      it->Class = pcv[clsIx];
      if (defineHeats)
        it->getDI().setInt("Heat", clsIx + 1);
    }
    it->updateChanged();
    it->synchronize();
  }
}

void oClass::getAgeLimit(int &low, int &high) const
{
  low = getDCI().getInt("LowAge");
  high = getDCI().getInt("HighAge");
}

void oClass::setAgeLimit(int low, int high)
{
  getDI().setInt("LowAge", low);
  getDI().setInt("HighAge", high);
}

int oClass::getExpectedAge() const
{
  int low, high;
  getAgeLimit(low, high);

  if (low>0 && high>0)
    return (low+high)/2;

  if (low==0 && high>0)
    return high-3;

  if (low>0 && high==0)
    return low + 1;


  // Try to guess age from class name
  for (size_t k=0; k<Name.length(); k++) {
    if (Name[k]>='0' && Name[k]<='9') {
      int age = _wtoi(&Name[k]);
      if (age>=10 && age<100) {
        if (age>=10 && age<=20)
          return age - 1;
        else if (age==21)
          return 28;
        else if (age>=35)
          return age + 2;
      }
    }
  }

  return 0;
}

void oClass::setSex(PersonSex sex)
{
  getDI().setString("Sex", encodeSex(sex));
}

PersonSex oClass::getSex() const
{
  return interpretSex(getDCI().getString("Sex"));
}

void oClass::setStart(const wstring &start)
{
  getDI().setString("StartName", start);
}

wstring oClass::getStart() const
{
  return getDCI().getString("StartName");
}

void oClass::setBlock(int block)
{
  getDI().setInt("StartBlock", block);
}

int oClass::getBlock() const
{
  return getDCI().getInt("StartBlock");
}

void oClass::setAllowQuickEntry(bool quick)
{
  getDI().setInt("AllowQuickEntry", quick);
}

bool oClass::getAllowQuickEntry() const
{
  return getDCI().getInt("AllowQuickEntry")!=0;
}

void oClass::setNoTiming(bool quick)
{
  tNoTiming = quick ? 1 : 0;
  getDI().setInt("NoTiming", quick);
}

BibMode oClass::getBibMode() const {
  const wstring &bm = getDCI().getString("BibMode");
  wchar_t b = bm.c_str()[0];
  if (b == 'A')
    return BibAdd;
  else if (b == 'F')
    return BibFree;
  else if (b == 'L')
    return BibLeg;
  else
    return BibSame;
}

void oClass::setBibMode(BibMode bibMode) {
  wstring res;
  switch (bibMode) {
  case BibAdd:
    res = L"A";
    break;
  case BibFree:
    res = L"F";
    break;
  case BibLeg:
    res = L"L";
    break;
  case BibSame:
    res = L"";
    break;
  default:
    throw meosException("Invalid bib mode");
  }

  getDI().setString("BibMode", res);
}


bool oClass::getNoTiming() const {
  if (tNoTiming!=0 && tNoTiming!=1)
    tNoTiming = getDCI().getInt("NoTiming")!=0 ? 1 : 0;
  return tNoTiming!=0;
}

void oClass::setIgnoreStartPunch(bool ignoreStartPunch) {
  tIgnoreStartPunch = ignoreStartPunch;
  getDI().setInt("IgnoreStart", ignoreStartPunch);
}

bool oClass::ignoreStartPunch() const {
  if (tIgnoreStartPunch!=0 && tIgnoreStartPunch!=1)
    tIgnoreStartPunch = getDCI().getInt("IgnoreStart")!=0 ? 1 : 0;
  return tIgnoreStartPunch != 0;
}

void oClass::setFreeStart(bool quick)
{
  getDI().setInt("FreeStart", quick);
}

bool oClass::hasFreeStart() const
{
  bool fs = getDCI().getInt("FreeStart") != 0;
  return fs;
}

void oClass::setDirectResult(bool quick)
{
  getDI().setInt("DirectResult", quick);
}

bool oClass::hasDirectResult() const
{
  return getDCI().getInt("DirectResult") != 0;
}


void oClass::setType(const wstring &start)
{
  getDI().setString("ClassType", start);
}

wstring oClass::getType() const
{
  return getDCI().getString("ClassType");
}

void oEvent::fillStarts(gdioutput &gdi, const string &id)
{
  vector< pair<wstring, size_t> > d;
  oe->fillStarts(d);
  gdi.addItem(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillStarts(vector< pair<wstring, size_t> > &out)
{
  out.clear();
  set<wstring> starts;
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (!it->getStart().empty())
      starts.insert(it->getStart());
  }

  if (starts.empty())
    starts.insert(lang.tl(L"Start") + L" 1");

  for (set<wstring>::iterator it = starts.begin(); it!=starts.end(); ++it) {
    //gdi.addItem(id, *it);
    out.push_back(make_pair(*it, 0));
  }
  return out;
}

void oEvent::fillClassTypes(gdioutput &gdi, const string &id)
{
  vector< pair<wstring, size_t> > d;
  oe->fillClassTypes(d);
  gdi.addItem(id, d);
}

ClassMetaType oClass::interpretClassType() const {
  int lowAge;
  int highAge;
  getAgeLimit(lowAge, highAge);

  if (highAge>0 && highAge <= 16)
    return ctYouth;

  map<wstring, ClassMetaType> types;
  oe->getPredefinedClassTypes(types);

  wstring type = getType();

  for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it) {
    if (type == it->first || type == lang.tl(it->first))
      return it->second;
  }

  if (oe->classTypeNameToType.empty()) {
    // Lazy readout of baseclasstypes
    wchar_t path[_MAX_PATH];
    getUserFile(path, L"baseclass.xml");
    xmlparser xml;
    xml.read(path);
    xmlobject cType = xml.getObject("BaseClassTypes");
    xmlList xtypes;
    cType.getObjects("Type", xtypes);
    for (size_t k = 0; k<xtypes.size(); k++) {
      wstring name = xtypes[k].getAttrib("name").wget();
      wstring typeS = xtypes[k].getAttrib("class").wget();
      ClassMetaType mtype = ctUnknown;
      if (stringMatch(typeS, L"normal"))
        mtype = ctNormal;
      else if (stringMatch(typeS, L"elite"))
        mtype = ctElite;
      else if (stringMatch(typeS, L"youth"))
        mtype = ctYouth;
      else if (stringMatch(typeS, L"open"))
        mtype = ctOpen;
      else if (stringMatch(typeS, L"exercise"))
        mtype = ctExercise;
      else if (stringMatch(typeS, L"training"))
        mtype = ctTraining;
      else {
        wstring err = L"Unknown type X#" + typeS;
        throw meosException(err);
      }
      oe->classTypeNameToType[name] = mtype;
    }
  }

  if (oe->classTypeNameToType.count(type) == 1)
    return oe->classTypeNameToType[type];

  return ctUnknown;
}

void oClass::assignTypeFromName(){
  wstring type = getType();
  if (type.empty()) {
    wstring prefix, suffix;
    extractAnyNumber(Name, prefix, suffix);
    int age = getExpectedAge();

    ClassMetaType mt = ctUnknown;
    if (age>=18) {
      if (stringMatch(suffix, lang.tl(L"Elit")) || wcschr(suffix.c_str(), 'E'))
        mt = ctElite;
      else if (stringMatch(suffix, lang.tl(L"Motion")) || wcschr(suffix.c_str(), 'M'))
        mt = ctExercise;
      else
        mt = ctNormal;
    }
    else if (age>=10 && age<=16) {
      mt = ctYouth;
    }
    else if (age<10) {
      if (stringMatch(prefix, lang.tl(L"Ungdom")) || wcschr(prefix.c_str(), 'U')
          || stringMatch(prefix, L"insk") || stringMatch(prefix, lang.tl(L"Inskolning")))
        mt = ctYouth;
      else if (stringMatch(suffix, lang.tl(L"Motion")) || wcschr(suffix.c_str(), 'M'))
        mt = ctExercise;
      else
        mt = ctOpen;
    }

    map<wstring, ClassMetaType> types;
    oe->getPredefinedClassTypes(types);

    for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it) {
      if (it->second == mt) {
        setType(lang.tl(it->first));
        return;
      }
    }
  }
}

void oEvent::getPredefinedClassTypes(map<wstring, ClassMetaType> &types) const {
  types.clear();
  types[L"Elit"] = ctElite;
  types[L"Vuxen"] = ctNormal;
  types[L"Ungdom"] = ctYouth;
  types[L"Motion"] = ctExercise;
  types[L"Öppen"] = ctOpen;
  types[L"Träning"] = ctTraining;
}

const vector< pair<wstring, size_t> > &oEvent::fillClassTypes(vector< pair<wstring, size_t> > &out)
{
  out.clear();
  set<wstring> cls;
  bool allHasType = !Classes.empty();
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!it->getType().empty())
      cls.insert(it->getType());
    else
      allHasType = false;
  }

  if (!allHasType) {
    map<wstring, ClassMetaType> types;
    getPredefinedClassTypes(types);

    for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it)
      cls.insert(lang.tl(it->first));
  }

  for (set<wstring>::iterator it = cls.begin(); it!=cls.end(); ++it) {
    //gdi.addItem(id, *it);
    out.push_back(make_pair(*it, 0));
  }
  return out;
}

int oClass::getNumRemainingMaps(bool forceRecalculate) const {
  oe->calculateNumRemainingMaps(forceRecalculate);

  int numMaps = tMapsRemaining;

  if (Course && Course->tMapsRemaining != numeric_limits<int>::min()) {
    if (numMaps == numeric_limits<int>::min())
      numMaps = Course->tMapsRemaining;
    else
      numMaps = min(numMaps, Course->tMapsRemaining);
  }  
  return numMaps;
}

void oClass::setNumberMaps(int nm) {
  getDI().setInt("NumberMaps", nm);
}

int oClass::getNumberMaps(bool rawAttribute) const {
  int nm = getDCI().getInt("NumberMaps");

  if (rawAttribute)
    return nm;

  if (nm == 0 && Course)
    nm = Course->getNumberMaps();

  return nm;
}

void oEvent::getStartBlocks(vector<int> &blocks, vector<wstring> &starts) const
{
  oClassList::const_iterator it;
  set<pair<wstring, int>> bs;
  for (it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    
    bs.emplace(it->getStart(), it->getBlock());
  }
  blocks.clear();
  starts.clear();

  for (auto &v : bs) {
    blocks.push_back(v.second);
    starts.push_back(v.first);
  }
}

Table *oEvent::getClassTB()//Table mode
{
  if (tables.count("class") == 0) {
    Table *table=new Table(this, 20, L"Klasser", "classes");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);
    oe->oClassData->buildTableCol(table);
    tables["class"] = table;
    table->addOwnership();
  }

  tables["class"]->update();
  return tables["class"];
}

void oEvent::generateClassTableData(Table &table, oClass *addClass)
{
  if (addClass) {
    addClass->addTableRow(table);
    return;
  }

  synchronizeList(oListId::oLClassId);
  oClassList::iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->isRemoved())
      it->addTableRow(table);
  }
}

void oClass::addTableRow(Table &table) const {
  pClass it = pClass(this);
  table.addRow(getId(), it);

  int row = 0;
  table.set(row++, *it, TID_ID, itow(getId()), false);
  table.set(row++, *it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, *it, TID_CLASSNAME, getName(), true);
  oe->oClassData->fillTableCol(*this, table, true);
}



bool oClass::inputData(int id, const wstring &input,
                       int inputId, wstring &output, bool noUpdate)
{
  synchronize(false);

  if (id>1000) {
    return oe->oClassData->inputData(this, id, input,
                                       inputId, output, noUpdate);
  }
  switch(id) {
    case TID_CLASSNAME:
      setName(input);
      synchronize();
      output=getName();
      return true;
    break;
  }

  return false;
}

void oClass::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oClassData->fillInput(oData, id, 0, out, selected);
    return;
  }

  if (id==TID_COURSE) {
    out.clear();
    oe->fillCourses(out, true);
    out.push_back(make_pair(lang.tl(L"Ingen bana"), 0));
    //gdi.selectItemByData(controlId.c_str(), Course ? Course->getId() : 0);
    selected = Course ? Course->getId() : 0;
  }
}


void oClass::getStatistics(const set<int> &feeLock, int &entries, int &started) const
{
  oRunnerList::const_iterator it;
  entries = 0;
  started = 0;
  for (it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->skip() || it->isVacant())
      continue;
    if (it->getStatus() == StatusNotCompetiting)
      continue;

    if (it->getClassId(false)==Id) {
      if (feeLock.empty() || feeLock.count(it->getDCI().getInt("Fee"))) {
        entries++;
        if (it->getStatus()!= StatusUnknown && it->getStatus()!= StatusDNS && it->tStatus != StatusCANCEL)
          started++;
      }
    }
  }
}

bool oClass::isSingleRunnerMultiStage() const
{
  return getNumStages()>1 && getNumDistinctRunnersMinimal()==1;
}

int oClass::getEntryFee(const wstring &date, int age) const
{
  oDataConstInterface odc = oe->getDCI();
  wstring oentry = odc.getDate("OrdinaryEntry");
  bool late = date > oentry && oentry>=L"2010-01-01";
  bool reduced = false;

  if (age > 0) {
    int low = odc.getInt("YouthAge");
    int high = odc.getInt("SeniorAge");
    reduced = age <= low || (high > 0 && age >= high);
  }

  if (reduced) {
    int high = getDCI().getInt("HighClassFeeRed");
    int normal = getDCI().getInt("ClassFeeRed");

    // Only return these fees if set
    if (high>0 && late)
      return high;
    else if (normal>0)
      return normal;
  }

  if (late)
    return getDCI().getInt("HighClassFee");
  else
    return getDCI().getInt("ClassFee");
}

void oClass::addClassDefaultFee(bool resetFee) {
  int fee = getDCI().getInt("ClassFee");

  if (fee == 0 || resetFee) {
    assignTypeFromName();
    ClassMetaType type = interpretClassType();
   // if (type.empty())
    switch (type) {
      case ctElite:
        fee = oe->getDCI().getInt("EliteFee");
      break;
      case ctYouth:
        fee = oe->getDCI().getInt("YouthFee");
      break;
      default:
        fee = oe->getDCI().getInt("EntryFee");
    }

    int reducedFee = oe->getDCI().getInt("YouthFee");

    double factor = 1.0 + 0.01 * _wtof(oe->getDCI().getString("LateEntryFactor").c_str());
    int lateFee = fee;
    int lateReducedFee = reducedFee;

    if (factor > 1) {
      lateFee = int(fee*factor + 0.5);
      lateReducedFee = int(reducedFee*factor + 0.5);
    }
    getDI().setInt("ClassFee", fee);
    getDI().setInt("HighClassFee", lateFee);
    getDI().setInt("ClassFeeRed", reducedFee);
    getDI().setInt("HighClassFeeRed", lateReducedFee);
  }
}

void oClass::reinitialize(bool force) const {
  if (!force && isInitialized)
    return;
  isInitialized = true; // Prevent recursion

  int ix = getDCI().getInt("SortIndex");
  if (ix == 0) {
    ix = getSortIndex(getId()*10);
    const_cast<oClass*>(this)->getDI().setInt("SortIndex", ix);
  }
  tSortIndex = ix;

  tMaxTime = getDCI().getInt("MaxTime");
  if (tMaxTime == 0 && oe) {
    tMaxTime = oe->getMaximalTime();
  }

  wstring wInfo = getDCI().getString("Qualification");
  if (!wInfo.empty()) {
    if (qualificatonFinal && !qualificatonFinal->matchSerialization(wInfo))
      clearQualificationFinal();

    if (!qualificatonFinal)
      qualificatonFinal = make_shared<QualificationFinal>(MaxClassId, Id);

    qualificatonFinal->init(wInfo);
    virtualClasses.resize(getNumQualificationFinalClasses());

    int nc = qualificatonFinal->getNumClasses();
    for (int i = 1; i <= nc; i++)
      getVirtualClass(i);
  }
  else {
    clearQualificationFinal();
  }

  tNoTiming = -1;
  tIgnoreStartPunch = -1;
}

void oClass::clearQualificationFinal() const {
  if (!qualificatonFinal)
    return;

  int nc = qualificatonFinal->getNumClasses();
  for (pClass pc : virtualClasses) {
    if (pc)
      pc->parentClass = nullptr;
  }

  virtualClasses.clear();
  qualificatonFinal.reset(); 
}

void oEvent::reinitializeClasses() const {
  for (auto &c : Classes)
    c.reinitialize(true);
}

int oClass::getSortIndex(int candidate) const {
  int major = numeric_limits<int>::max();
  int minor = 0;

  for (oClassList::iterator it = oe->Classes.begin(); it != oe->Classes.end(); ++it) {
    int ix = it->getDCI().getInt("SortIndex");
    if (ix>0) {
      if (ix>candidate && ix<major)
        major = ix;

      if (ix<candidate && ix>minor)
        minor = ix;
    }
  }

  // If the gap is less than 10 (which is the default), optimize
  if (major < numeric_limits<int>::max() && minor>0 && ((major-candidate)<10 || (candidate-minor)<10))
    return (major+minor)/2;
  else
    return candidate;
}

void oClass::apply() {
  int trueLeg = 0;
  int trueSubLeg = 0;

  for (size_t k = 0; k<legInfo.size(); k++) {
    oLegInfo &li = legInfo[k];
    LegTypes lt = li.legMethod;
    if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
      trueLeg++;
      trueSubLeg = 0;
    }
    else
      trueSubLeg++;

    if (trueSubLeg == 0 && (k+1) < legInfo.size()) {
      LegTypes nt = legInfo[k+1].legMethod;
      if (nt == LTParallel || nt == LTParallelOptional || nt == LTExtra || nt == LTIgnore)
        trueSubLeg = 1;
    }
    li.trueLeg = trueLeg;
    li.trueSubLeg = trueSubLeg;
    if (trueSubLeg == 0)
      li.displayLeg = itos(trueLeg);
    else
      li.displayLeg = itos(trueLeg) + "." + itos(trueSubLeg);
  }
}

class LegResult
{
private:
  inthashmap rmap;
public:
  void addTime(int from, int to, int time);
  int getTime(int from, int to) const;
};

void LegResult::addTime(int from, int to, int time)
{
  int key = from + (to<<15);
  int value;
  if (rmap.lookup(key, value))
    time = min(value, time);
  rmap[key] = time;
}

int LegResult::getTime(int from, int to) const
{
  int key = from + (to<<15);
  int value;
  if (rmap.lookup(key, value))
    return value;
  else
    return 0;
}

void oClass::clearSplitAnalysis()
{
#ifdef _DEBUG
  if (!tSplitAnalysisData.empty())
    OutputDebugString((L"Clear splits " + Name + L"\n").c_str());
#endif
  tFirstStart.clear();
  tLastStart.clear();

  tSplitAnalysisData.clear();
  tCourseLegLeaderTime.clear();
  tCourseAccLegLeaderTime.clear();

  if (tLegLeaderTime)
    delete tLegTimeToPlace;
  tLegTimeToPlace = 0;

  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
  tLegAccTimeToPlace = 0;

  tSplitRevision++;

  oe->classChanged(this, false);
}

void oClass::insertLegPlace(int from, int to, int time, int place)
{
  if (tLegTimeToPlace) {
    int key = time + (to + from*256)*8013;
    tLegTimeToPlace->insert(key, place);
  }
}

int oClass::getLegPlace(int ifrom, int ito, int time) const
{
  if (tLegTimeToPlace) {
    int key = time + (ito + ifrom*256)*8013;
    int place;
    if (tLegTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}

void oClass::insertAccLegPlace(int courseId, int controlNo, int time, int place)
{ /*
  char bf[256];
  sprintf_s(bf, "Insert to %d, %d, time %d\n", courseId, controlNo, time);
  OutputDebugString(bf);
  */
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId*128)*16013;
    tLegAccTimeToPlace->insert(key, place);
  }
}

void oClass::getStartRange(int leg, int &firstStart, int &lastStart) const {
  leg = mapLeg(leg);

  if (tFirstStart.empty()) {
    size_t s = getLastStageIndex() + 1;
    assert(s>0);
    vector<int> lFirstStart, lLastStart;
    lFirstStart.resize(s, 3600 * 24 * 365);
    lLastStart.resize(s, 0);
    for (oRunnerList::iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
      if (it->isRemoved() || it->getClassRef(true) != this)
        continue;
      if (it->needNoCard())
        continue;
      size_t tleg = mapLeg(it->tLeg);
      if (tleg < s) {
        lFirstStart[tleg] = min<unsigned>(lFirstStart[tleg], it->tStartTime);
        lLastStart[tleg] = max<signed>(lLastStart[tleg], it->tStartTime);
      }
    }
    swap(tLastStart, lLastStart);
    swap(tFirstStart, lFirstStart);
  }
  if (size_t(leg) < tFirstStart.size()) {
    firstStart = tFirstStart[leg];
    lastStart = tLastStart[leg];
  }
  else if (!tFirstStart.empty()) {
    firstStart = tFirstStart[0];
    lastStart = tLastStart[0];
    for (size_t k = 1; k < tFirstStart.size(); k++) {
      if (lastStart > 0) {
        firstStart = min(tFirstStart[k], firstStart);
        lastStart = max(tLastStart[k], lastStart);
      }
    }
  }
  else {
    firstStart = 0;
    lastStart = 0;
  }
}

int oClass::getAccLegPlace(int courseId, int controlNo, int time) const
{/*
  char bf[256];
  sprintf_s(bf, "Get from %d,  %d, time %d\n", courseId, controlNo, time);
  OutputDebugString(bf);
  */
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId*128)*16013;
    int place;
    if (tLegAccTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}


void oClass::calculateSplits() {
  clearSplitAnalysis();
  set<pCourse> cSet;
  map<int, vector<int> > legToTime;

  for (size_t k=0;k<MultiCourse.size();k++) {
    for (size_t j=0; j<MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        cSet.insert(MultiCourse[k][j]);
    }
  }
  if (getCourse())
    cSet.insert(getCourse());

  LegResult legRes;
  LegResult legBestTime;
  vector<pRunner> rCls;

  if (isQualificationFinalBaseClass() || isQualificationFinalBaseClass()) {
    for (auto &r : oe->Runners) {
      if (!r.isRemoved() && r.getClassRef(true) == this)
        rCls.push_back(&r);
    }
  }
  else {
    for (auto &r : oe->Runners) {
      if (!r.isRemoved() && r.Class == this)
        rCls.push_back(&r);
    }
  }

  for (set<pCourse>::iterator cit = cSet.begin(); cit!= cSet.end(); ++cit)  {
    pCourse pc = *cit;
    // Store all split times in a matrix
    const unsigned nc = pc->getNumControls();
    if (nc == 0)
      return;

    vector< vector<int> > splits(nc+1);
    vector< vector<int> > splitsAcc(nc+1);
    vector<bool> acceptMissingPunch(nc+1, true);

    for (pRunner it : rCls) {
      pCourse tpc = it->getCourse(false);
      if (tpc != pc || tpc == 0)
        continue;

      const vector<SplitData> &sp = it->getSplitTimes(true);
      const int s = min<int>(nc, sp.size());

      for (int k = 0; k < s; k++) {
        if (sp[k].time > 0 && acceptMissingPunch[k]) {
          pControl ctrl = tpc->getControl(k);
          // If there is a
          if (ctrl && ctrl->getStatus() != oControl::StatusBad && ctrl->getStatus() != oControl::StatusOptional)
            acceptMissingPunch[k] = false;
        }
      }
    }

    for (pRunner it : rCls) {
      pCourse tpc = it->getCourse(false);

      if (tpc != pc)
        continue;

      const vector<SplitData> &sp = it->getSplitTimes(true);
      const int s = min<int>(nc, sp.size());

      vector<int> &tLegTimes = it->tLegTimes;
      tLegTimes.resize(nc + 1);
      bool ok = true;

      for (int k = 0; k < s; k++) {
        if (sp[k].time > 0) {
          if (ok) {
            // Store accumulated times
            int t = sp[k].time - it->tStartTime;
            if (it->tStartTime>0 && t>0)
              splitsAcc[k].push_back(t);
          }

          if (k == 0) { // start -> first
            int t = sp[0].time - it->tStartTime;
            if (it->tStartTime>0 && t>0) {
              splits[k].push_back(t);
              tLegTimes[k] = t;
            }
            else
              tLegTimes[k] = 0;
          }
          else { // control -> control
            int t = sp[k].time - sp[k-1].time;
            if (sp[k-1].time>0 && t>0) {
              splits[k].push_back(t);
              tLegTimes[k] = t;
            }
            else
              tLegTimes[k] = 0;
          }
        }
        else
          ok = acceptMissingPunch[k];
      }

      // last -> finish
      if (sp.size() == nc && sp[nc-1].time>0 && it->FinishTime > 0) {
        int t = it->FinishTime - sp[nc-1].time;
        if (t>0) {
          splits[nc].push_back(t);
          tLegTimes[nc] = t;
          if (it->statusOK() && (it->FinishTime - it->tStartTime) > 0) {
            splitsAcc[nc].push_back(it->FinishTime - it->tStartTime);
          }
        }
        else
          tLegTimes[nc] = 0;
      }
    }

    if (splits.size()>0 && tLegTimeToPlace == 0) {
      tLegTimeToPlace = new inthashmap(splits.size() * splits[0].size());
      tLegAccTimeToPlace = new inthashmap(splits.size() * splits[0].size());
    }

    vector<int> &accLeaderTime = tCourseAccLegLeaderTime[pc->getId()];

    for (size_t k = 0; k < splits.size(); k++) {

      // Calculate accumulated best times and places
      if (!splitsAcc[k].empty()) {
        sort(splitsAcc[k].begin(), splitsAcc[k].end());
        accLeaderTime.push_back(splitsAcc[k].front()); // Store best time

        int place = 1;
        for (size_t j = 0; j < splitsAcc[k].size(); j++) {
          if (j>0 && splitsAcc[k][j-1]<splitsAcc[k][j])
            place = j+1;
          insertAccLegPlace(pc->getId(), k, splitsAcc[k][j], place);
        }
      }
      else {
        // Bad control / missing times
        int t = 0;
        if (!accLeaderTime.empty())
          t = accLeaderTime.back();
        accLeaderTime.push_back(t); // Store time from previous leg
      }

      sort(splits[k].begin(), splits[k].end());
      const size_t ntimes = splits[k].size();
      if (ntimes == 0)
        continue;

      int from = pc->getCommonControl(), to = pc->getCommonControl(); // Represents start/finish
      if (k < nc && pc->getControl(k))
        to = pc->getControl(k)->getId();
      if (k>0 && pc->getControl(k-1))
        from = pc->getControl(k-1)->getId();

      for (size_t j = 0; j < ntimes; j++)
        legToTime[256*from + to].push_back(splits[k][j]);

      int time = 0;
      if (ntimes < 5)
        time = splits[k][0]; // Best time
      else if (ntimes < 12)
        time = (splits[k][0]+splits[k][1]) / 2; //Average best time
      else {
        int nval = ntimes/6;
        for (int r = 1; r <= nval; r++)// "Best fraction", skip winner
          time += splits[k][r];
        time /= nval;
      }

      legRes.addTime(from, to, time);
      legBestTime.addTime(from, to, splits[k][0]); // Add leader time
    }
  }

  // Loop and sort times for each leg run in this class
  for (map<int, vector<int> >::iterator cit = legToTime.begin(); cit != legToTime.end(); ++cit) {
    int key = cit->first;
    vector<int> &times = cit->second;
    sort(times.begin(), times.end());
    int jsiz = times.size();
    for (int j = 0; j<jsiz; ++j) {
      if (j==0 || times[j-1]<times[j])
        insertLegPlace(0, key, times[j], j+1);
    }
  }

  for (set<pCourse>::iterator cit = cSet.begin(); cit != cSet.end(); ++cit)  {
    pCourse pc = *cit;
    const unsigned nc = pc->getNumControls();
    vector<int> normRes(nc+1);
    vector<int> bestRes(nc+1);
    int cc = pc->getCommonControl();
    for (size_t k = 0; k <= nc; k++) {
      int from = cc, to = cc; // Represents start/finish
      if (k < nc && pc->getControl(k))
        to = pc->getControl(k)->getId();
      if (k>0 && pc->getControl(k-1))
        from = pc->getControl(k-1)->getId();
      normRes[k] = legRes.getTime(from, to);
      bestRes[k] = legBestTime.getTime(from, to);
    }

    swap(tSplitAnalysisData[pc->getId()], normRes);
    swap(tCourseLegLeaderTime[pc->getId()], bestRes);
  }
}

bool oClass::isRogaining() const {
  if (Course)
    return Course->getMaximumRogainingTime() > 0;

  for (size_t k = 0;k<MultiCourse.size(); k++)
    for (size_t j = 0;k<MultiCourse[j].size(); j++)
      if (MultiCourse[k][j])
        return MultiCourse[k][j]->getMaximumRogainingTime() > 0;

  return false;
}

void oClass::remove()
{
  if (oe)
    oe->removeClass(Id);
}

bool oClass::canRemove() const
{
  return !oe->isClassUsed(Id);
}

int oClass::getMaximumRunnerTime() const {
  reinitialize(false);
  return tMaxTime;
}

int oClass::getNumLegNoParallel() const {
  int nl = 1;
  for (size_t k = 1; k < legInfo.size(); k++) {
    if (!legInfo[k].isParallel())
      nl++;
  }
  return nl;
}

bool oClass::splitLegNumberParallel(int leg, int &legNumber, int &legOrder) const {
  legNumber = 0;
  legOrder = 0;
  if (legInfo.empty())
    return false;

  int stop = min<int>(leg, legInfo.size() - 1);
  int k;
  for (k = 0; k < stop; k++) {
    if (legInfo[k+1].isParallel() || legInfo[k+1].isOptional())
      legOrder++;
    else {
      legOrder = 0;
      legNumber++;
    }
  }
  if (legOrder == 0) {
    if (k+1 < int(legInfo.size()) && (legInfo[k+1].isParallel() || legInfo[k+1].isOptional()))
      return true;
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return false;
  }
  return true;
}

int oClass::getLegNumberLinear(int legNumberIn, int legOrderIn) const {
  if (legNumberIn == 0 && legOrderIn == 0)
    return 0;

  int legNumber = 0;
  int legOrder = 0;
  for (size_t k = 1; k < legInfo.size(); k++) {
    if (legInfo[k].isParallel() || legInfo[k].isOptional())
      legOrder++;
    else {
      legOrder = 0;
      legNumber++;
    }
    if (legNumberIn == legNumber && legOrderIn == legOrder)
      return k;
  }

  return -1;
}

/** Return an actual linear index for this class. */
int oClass::getLinearIndex(int index, bool isLinear) const {
  if (legInfo.empty())
    return 0;
  if (size_t(index) >= legInfo.size())
    return legInfo.size() - 1; // -1 to last leg

  return isLinear ? index : getLegNumberLinear(index, 0);
}

wstring oClass::getLegNumber(int leg) const {
  int legNumber, legOrder;
  bool par = splitLegNumberParallel(leg, legNumber, legOrder);
  wchar_t bf[16];
  if (par) {
    char symb = 'a' + legOrder;
    swprintf_s(bf, L"%d%c", legNumber + 1, symb);
  }
  else {
    swprintf_s(bf, L"%d", legNumber + 1);
  }
  return bf;
}

oClass::ClassStatus oClass::getClassStatus() const {
  if (tStatusRevision != oe->dataRevision) {
    wstring s = getDCI().getString("Status");
    if (s == L"I")
      tStatus =  Invalid;
    else if (s == L"IR")
      tStatus = InvalidRefund;
    else
      tStatus = Normal;

    tStatusRevision = oe->dataRevision;
  }
  return tStatus;
}

void oClass::clearCache(bool recalculate) {
  if (recalculate)
    oe->reCalculateLeaderTimes(getId());
  clearSplitAnalysis();
  tResultInfo.clear();//Do on competitor remove!
}


bool oClass::wasSQLChanged(int leg, int control) const {
  if (oe->globalModification)
    return true;

  map<int, set<int> >::const_iterator res = sqlChangedControlLeg.find(-1);
  if (res != sqlChangedControlLeg.end()) {
    if (leg == -1 || res->second.count(-1) || res->second.count(leg))
      return true;
  }

  if (control != -1) {
    if (control == -2) // Any control
      return sqlChangedControlLeg.size() > 0;
    res = sqlChangedControlLeg.find(control);
    if (res != sqlChangedControlLeg.end()) {
      if (leg == -1 || res->second.count(-1) || res->second.count(leg))
        return true;
    }
  }

  res = sqlChangedLegControl.find(leg);
  if (res != sqlChangedLegControl.end()) {
    if (control == -1 || res->second.count(-1) || res->second.count(control))
      return true;
  }

  return false;
}

void oClass::markSQLChanged(int leg, int control) {
  sqlChangedControlLeg[control].insert(leg);
  sqlChangedLegControl[leg].insert(control);
  oe->classChanged(this, false);
}

void oClass::changedObject() {
  markSQLChanged(-1,-1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
}

static void checkMissing(const map< pair<int, int>, int > &master,
                         const map< pair<int, int>, int > &check,
                         set< pair<int, int> > &controlProblems) {
  assert(master.size() >= check.size());

  for ( map< pair<int, int>, int >::const_iterator it = master.begin();
        it != master.end(); ++it) {
    map< pair<int, int>, int >::const_iterator res = check.find(it->first);
    if (res == check.end() || res->second != it->second)
      controlProblems.insert(it->first);
  }
}

// Check if forking is fair
bool oClass::checkForking(vector< vector<int> > &legOrder,
                          vector< vector<int> > &forks,
                          set< pair<int, int> > &unfairLegs) const {
  legOrder.clear();
  forks.clear();
  map<long long, int> hashes;
  int max = 1;
  set<int> factors;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    if (MultiCourse[k].size() > 1 && factors.count(MultiCourse[k].size()) == 0) {
      max *= MultiCourse[k].size();
      factors.insert(MultiCourse[k].size()); // This is an over estimate. Should consider prime factors to get it exact
    }
  }

  legOrder.reserve(max);
  for (int k = 0; k < max; k++) {
    vector<int> order;
    long long hash = 0;
    for (size_t j = 0; j< MultiCourse.size(); j++) {
      if (getLegType(j) == LTExtra || getLegType(j) == LTIgnore)
        continue;
      if (!MultiCourse[j].empty()) {
        int ix = k % MultiCourse[j].size();
        int cid = MultiCourse[j][ix]->getId();
        order.push_back(cid);
        hash = hash * 997 + cid;
      }
    }
    if (order.empty())
      continue;

    if (hashes.count(hash) == 0) {
      hashes[hash] = legOrder.size();
      legOrder.push_back(order);
    }
    else {
      int test = hashes[hash];
      if (legOrder[test] != order) {
        // Test for hash collision. Will not happen...
        bool exist = false;
        for (size_t i = 0; i < legOrder.size(); i++) {
          if (legOrder[i] == order) {
            exist = true;
            break;
          }
        }
        if (!exist) {
          legOrder.push_back(order);
        }
      }
    }
  }


  for (oTeamList::const_iterator it = oe->Teams.begin(); it != oe->Teams.end(); ++it) {
    if (it->skip() || it->Class != this)
      continue;
    vector<int> order;
    bool valid = true;
    long long hash = 0;
    for (size_t j = 0; j < it->Runners.size(); j++) {
      pCourse crs;
      if (it->Runners[j] && (crs = it->Runners[j]->getCourse(false)) != 0) {
        if (it->Runners[j]->getNumShortening() > 0) {
          valid = false;
          break;
        }
        int cid = crs->getId();
        order.push_back(cid);
        hash = hash * 997 + cid;
      }
      else {
        valid = false;
        break;
      }
    }
    if (!valid) 
      continue;
    if (hashes.count(hash) == 0) {
      hashes[hash] = legOrder.size();
      legOrder.push_back(order);
    }
    else {
      int test = hashes[hash];
      if (legOrder[test] != order) {
        // Test for hash collision. Will not happen...
        bool exist = false;
        for (size_t i = 0; i < legOrder.size(); i++) {
          if (legOrder[i] == order) {
            exist = true;
            break;
          }
        }
        if (!exist) {
          legOrder.push_back(order);
        }
      }
    }
  }

  vector< vector<int> > controlOrder(legOrder.size());
  vector< map< pair<int, int>, int > > countLegsPerFork(legOrder.size());

  for(size_t k = 0; k < legOrder.size(); k++) {
    for (size_t j = 0; j < legOrder[k].size(); j++) {
      pCourse pc = oe->getCourse(legOrder[k][j]);
      if (pc) {
        controlOrder[k].push_back(-1); // Finish/start
        for (int i = 0; i < pc->nControls; i++) {
          int id = pc->Controls[i]->nNumbers == 1 ? pc->Controls[i]->Numbers[0] : pc->Controls[i]->getId();
          controlOrder[k].push_back(id);
        }
      }
    }
    if (controlOrder[k].size() > 0)
      controlOrder[k].push_back(-1); // Finish

    for (size_t j = 1; j < controlOrder[k].size(); j++) {
      int s = controlOrder[k][j-1];
      int e = controlOrder[k][j];
      ++countLegsPerFork[k][make_pair(s, e)];
    }
  }

  unfairLegs.clear();
  for (size_t k = 1; k < countLegsPerFork.size(); k++) {
    if (countLegsPerFork[0].size() >= countLegsPerFork[k].size())
      checkMissing(countLegsPerFork[0], countLegsPerFork[k], unfairLegs);
    else
      checkMissing(countLegsPerFork[k], countLegsPerFork[0], unfairLegs);
  }

  forks = controlOrder;

  return unfairLegs.empty();
}

long long oClass::setupForkKey(const vector<int> indices, const vector< vector< vector<int> > > &courseKeys, vector<int> &legs) {
  size_t sr = 0;
  int mergeIx[maxRunnersTeam];
  const vector<int> *pCrs[maxRunnersTeam];
  int npar = indices.size();
  for (int k = 0; k < npar; k++) {
    if (indices[k]>=0) {
      pCrs[k] = &courseKeys[k][indices[k]];
      sr += pCrs[k]->size();
    }
    else
      pCrs[k] = 0;

    mergeIx[k] = 0;
  }
  if (legs.size() <= sr)
    legs.resize(sr+1);

 
  size_t nleg = 0;
  while (nleg < sr) {
    int best = -1;
    for (int i = 0; i < npar; i++) {
      if (!pCrs[i])
        continue;
      int s = pCrs[i]->size();
      if (mergeIx[i] < s && (best == -1 || (*pCrs[i])[mergeIx[i]] < (*pCrs[best])[mergeIx[best]]) )
        best = i;
    }
    if (best == -1)
      break;
    legs[nleg++] = (*pCrs[best])[mergeIx[best]];
    ++mergeIx[best];
  }

  /*for (size_t k = 0; k < indices.size(); k++) {
    if (indices[k]>=0) {
      const vector<int> &crs = courseKeys[k][indices[k]];
      for (size_t j = 0; j < crs.size(); j++)
        legs[nleg++] = crs[j];
      //legs.insert(crs.begin(), crs.end());
      //vector<int> tl;
      //tl.resize(legs.size() + crs.size());
      //merge(legs.begin(), legs.end(), crs.begin(), crs.end(), tl.begin());
      //tl.swap(legs);
    }
  }*/

  /*for (size_t k = 0; k < indices.size(); k++) {
    if (indices[k]>=0) {
      const vector<int> &crs = courseKeys[k][indices[k]];
      //legs.insert(crs.begin(), crs.end());
      vector<int> tl;
      tl.resize(legs.size() + crs.size());
      merge(legs.begin(), legs.end(), crs.begin(), crs.end(), tl.begin());
      tl.swap(legs);
    }
  }*/
  unsigned long long key = 0;
  for (size_t k = 0; k < nleg; k++)
    key = key * 1057 + legs[k];

  return key;
}

pair<int, int> oClass::autoForking(const vector< vector<int> > &inputCourses) {
  if (inputCourses.size() != getNumStages())
    throw meosException("Internal error");
  int legs = inputCourses.size();
  vector<int> nf(legs);
  vector<unsigned long long> prod(legs);
  vector<int> ix(legs);
  vector< vector< vector<int> > > courseKeys(legs);
  vector< vector<pCourse> > pCourses(legs);

  unsigned long long N = 1;
  for (int k = 0; k < legs; k++) {
    prod[k] = N;
    if (!inputCourses[k].empty()) {
      N *= inputCourses[k].size();
      nf[k] = inputCourses[k].size();
    }

    // Setup course keys
    courseKeys[k].resize(inputCourses[k].size());
    for (size_t j = 0; j < inputCourses[k].size(); j++) {
      pCourse pc = oe->getCourse(inputCourses[k][j]);
      pCourses[k].push_back(pc);
      if (pc) {
        for (int c = 0; c <= pc->getNumControls(); c++) {
          int from = c == 0 ? 1 : pc->getControl(c-1)->getId();
          int to = c == pc->getNumControls() ? 2 : pc->getControl(c)->getId();
          int key = from + to*997;
          courseKeys[k][j].push_back(key);
        }
        sort(courseKeys[k][j].begin(), courseKeys[k][j].end());
      }
    }
  }

  // Sample if there are very many combinations.
  int sampleFactor = 1;
  while(N > 10000000) {
    sampleFactor *= 13;
    N /= 13;
  }
  size_t Ns = size_t(N);
  map<long long, int> count;
  vector<int> ws;
  for (size_t k = 0; k < Ns; k ++) {
    for (int j = 0; j < legs; j++) {
      unsigned long long D = k * sampleFactor;
      if (nf[j]>0) {
        ix[j] = int((D/prod[j] + j) % nf[j]);
      }
      else ix[j] = -1;
    }
    unsigned long long key = setupForkKey(ix, courseKeys, ws);
    ++count[key];
  }

  // Select the key generating best forking
  long long keyToUse = -1;
  int mv = 0;
  for (map<long long, int>::iterator it = count.begin(); it != count.end(); ++it) {
    if (it->second > mv) {
      keyToUse = it->first;
      mv = it->second;
    }
  }
  count.clear();

  // Clear old forking
  for (int j = 0; j < legs; j++) {
    if (nf[j]>0) {
      clearStageCourses(j);
    }
  }
  set<int> coursesUsed;
  set<long long> generatedForkKeys;

  vector< vector<pCourse> > courseMatrix(legs);
  for (size_t k = 0; k < Ns; k++) {
    long long forkKey = 0;
    for (int j = 0; j < legs; j++) {
      unsigned long long D = (k * 997)%Ns * sampleFactor;
      if (nf[j]>0) {
        ix[j] = int((D/prod[j] + j) % nf[j]);
        forkKey = forkKey * 997 + ix[j];
      }
      else ix[j] = -1;
    }
    unsigned long long key = setupForkKey(ix, courseKeys, ws);
    if (key == keyToUse && generatedForkKeys.count(forkKey) == 0) {
      generatedForkKeys.insert(forkKey);
      for (int j = 0; j < legs; j++) {
        if (nf[j] > 0) {
          coursesUsed.insert(pCourses[j][ix[j]]->getId());
          courseMatrix[j].push_back(pCourses[j][ix[j]]);
          //addStageCourse(j, pCourses[j][ix[j]]);
        }
      }
    }
    if (generatedForkKeys.size() > 200)
      break;
  }
  vector<int> fperm;
  for (size_t j = 0; j < courseMatrix.size(); j++) {
    if (courseMatrix[j].empty())
      continue;

    // Take the first used course.
    fperm.resize(courseMatrix[j].size());
    for (size_t i = 0; i < fperm.size(); i++)
      fperm[i] = i;
    break;
  }
  permute(fperm);
  int lastSet = -1;
  for (int j = 0; j < legs; j++) {
    if (nf[j] > 0) {
      lastSet = j;
      for (size_t k = 0; k < courseMatrix[j].size(); k++) {
        if (k < fperm.size()) {
          addStageCourse(j, courseMatrix[j][fperm[k]], -1);
        }
        else {
          addStageCourse(j, courseMatrix[j][k], -1);
        }
      }
    }
    else if (lastSet >= 0 && getLegType(j) == LTExtra) {
      MultiCourse[j] = MultiCourse[lastSet];
      // getCourses(lastSet, courses);
      //clearStageCourses(j);
      //for (size_t k = 0; k < courses.size(); k++)
      //  addStageCourse(j, courses[k]->getId());
    }
    else {
      lastSet = -1;
    }
  }

  return make_pair(generatedForkKeys.size(), coursesUsed.size());
}

int oClass::extractBibPattern(const wstring &bibInfo, wchar_t pattern[32]) {
  int number = 0;

  if (bibInfo.empty())
    pattern[0] = 0;
  else {
    number = 0;
    int pIndex = 0;
    bool hasNC = false;

    for (size_t j = 0; j < bibInfo.size() && j < 10; j++) {
      if (bibInfo[j]>='0' && bibInfo[j]<='9') {
        if (!hasNC) {
          pattern[pIndex++] = '%';
          pattern[pIndex++] = 'd';
          hasNC = true;
        }
        number = 10 * number + bibInfo[j]-'0';
      }
      else if (bibInfo[j] != '%' && bibInfo[j] > 32)
        pattern[pIndex++] = bibInfo[j]; 
    }
    if (!hasNC) {
      pattern[pIndex++] = '%';
      pattern[pIndex++] = 'd';
      number = 1;
    }
    pattern[pIndex] = 0;
  }
  return number;
}

AutoBibType oClass::getAutoBibType() const {
  wstring bib = getDCI().getString("Bib");
  if (bib.empty()) // Manual
    return AutoBibManual;
  else if (bib == L"*") // Consecutive
    return AutoBibConsecutive;
  else if (bib == L"-") // No bib
    return AutoBibNone;
  else
    return AutoBibExplicit;
}

bool oClass::hasAnyCourse(const set<int> &crsId) const {
  if (Course && crsId.count(Course->getId()))
    return true;

  for (size_t j = 0; j < MultiCourse.size(); j++) {
    for (size_t k = 0; k < MultiCourse[j].size(); k++) {
      if (MultiCourse[j][k] && crsId.count(MultiCourse[j][k]->getId()))
        return true;
    }
  }

  return false;
}

bool oClass::usesCourse(const oCourse &crs) const {
  if (Course == &crs)
    return true;

  for (size_t j = 0; j < MultiCourse.size(); j++) {
    for (size_t k = 0; k < MultiCourse[j].size(); k++) {
      if (MultiCourse[j][k] == &crs)
        return true;
    }
  }

  return false;
}

void oClass::extractBibPatterns(oEvent &oe, map<int, pair<wstring, int> > &patterns) {
  vector<pTeam> t;
  oe.getTeams(0, t, true);
  vector<pRunner> r;
  oe.getRunners(0, 0, r, true);
  patterns.clear();
  wchar_t pattern[32];
  for (size_t k = t.size(); k > 0; k--) {
    int cls = t[k-1]->getClassId(true);
    if (cls == 0)
      continue;
    const wstring &bib = t[k-1]->getBib();
    if (!bib.empty()) {
      int num = extractBibPattern(bib, pattern);
      if (num > 0) {
        pair<wstring, int> &val = patterns[cls];
        if (num > val.second) {
          val.second = num;
          val.first = pattern;
        }
      }
    }
  }

  for (size_t k = r.size(); k > 0; k--) {
    if (r[k-1]->getTeam() != 0)
      continue;
    int cls = r[k-1]->getClassId(true);
    if (cls == 0)
      continue;
    const wstring &bib = r[k-1]->getBib();
    if (!bib.empty()) {
      int num = extractBibPattern(bib, pattern);
      if (num > 0) {
        pair<wstring, int> &val = patterns[cls];
        if (num < val.second) {
          val.second = num;
          val.first = pattern;
        }
      }
    }
  }
}

pair<int, wstring> oClass::getNextBib(map<int, pair<wstring, int> > &patterns) {
  map<int, pair<wstring, int> >::iterator it = patterns.find(Id);
  if (it != patterns.end() && it->second.second > 0) {
    wchar_t bib[32];
    swprintf_s(bib, it->second.first.c_str(), ++it->second.second);
    return make_pair(it->second.second, bib);  
  }
  return make_pair(0, _EmptyWString);
}

pair<int, wstring> oClass::getNextBib() {
  vector<pTeam> t;
  oe->getTeams(Id, t, true);
  set<int> bibs;
  wchar_t pattern[32];

  if (!t.empty()) {
    for (size_t k = 0; k < t.size(); k++) {
      const wstring &bib = t[k]->getBib();
      if (!bib.empty()) {
        int num = extractBibPattern(bib, pattern);
        if (num > 0) {
          bibs.insert(num);
        }
      }
    }
  }
  else {
    vector<pRunner> r;
    oe->getRunners(Id, 0, r, true);
 
    for (size_t k = 0; k < r.size(); k++) {
      if (r[k]->getTeam() != 0)
        continue;
      const wstring &bib = r[k]->getBib();
      if (!bib.empty()) {
        int num = extractBibPattern(bib, pattern);
        if (num > 0) {
          bibs.insert(num);
        }
      }
    }
  }

  if (bibs.empty())
    return make_pair(0, _EmptyWString);
  int candidate = -1;
  for (set<int>::iterator it = bibs.begin(); it != bibs.end(); ++it) {
    if (candidate > 0 && *it != candidate) {
      break;
    }
    candidate = *it + 1;
  }

  wchar_t bib[32];
  swprintf_s(bib, pattern, candidate);
  return make_pair(candidate, bib);
}

int oClass::getNumForks() const {
  set<int> factors;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    int f = MultiCourse[k].size();
    if (f <= 1)
      continue;
    bool skip = false;
    for (set<int>::iterator it = factors.begin(); it != factors.end(); ++it) {
      int of = *it;
      if (of == f) {
        skip = true;
        break;
      }
      else if (f < of && (of % f) == 0) {
        skip = true;
        break;
      }
      else if (of < f && (f % of) == 0) {
        factors.erase(it);
        it = factors.begin();
        continue;
      }
    }
    if (!skip)
      factors.insert(f);
  }
  int res = 1;
  for (set<int>::iterator it = factors.begin(); it != factors.end(); ++it) {
    res *= *it;
  }
  return res;
}

void oClass::getSeedingMethods(vector< pair<wstring, size_t> > &methods) {
  methods.clear();
  methods.push_back(make_pair(lang.tl("Resultat"), SeedResult));
  methods.push_back(make_pair(lang.tl("Tid"), SeedTime));
  methods.push_back(make_pair(lang.tl("Ranking"), SeedRank));
  methods.push_back(make_pair(lang.tl("Poäng"), SeedPoints));
}

void oClass::drawSeeded(ClassSeedMethod seed, int leg, int firstStart, 
                        int interval, const vector<int> &groups,
                        bool noClubNb, bool reverseOrder, int pairSize) {
  vector<pRunner> r;
  oe->getRunners(Id, 0, r, true);
  vector< pair<int, int> > seedIx;
  if (seed == SeedResult) {
    oe->reEvaluateAll(set<int>(), true);
    oe->calculateResults({}, oEvent::ResultType::ClassResult, false);
  }
  for (size_t k = 0; k < r.size(); k++) {
    if (r[k]->tLeg != leg && leg != -1)
      continue;

    pair<int,int> sx;
    sx.second = k;
    if (seed == SeedRank)
      sx.first = r[k]->getRanking();
    else if (seed == SeedResult)
      sx.first = ClassSplit::evaluateResult(*r[k]);
    else if (seed == SeedTime)
      sx.first = ClassSplit::evaluateTime(*r[k]);
    else if (seed == SeedPoints)
      sx.first = ClassSplit::evaluatePoints(*r[k]);
    else
      throw meosException("Not yet implemented");

    seedIx.push_back(sx);
  }

  sort(seedIx.begin(), seedIx.end());

  if (seedIx.empty() || seedIx.front().first == seedIx.back().first) {
    throw meosException("error:invalidmethod");
  }

  vector<size_t> seedSpec;
  if (groups.size() == 1) {
    size_t added = 0;
    if (groups[0] <= 0)
      throw meosException("Internal error");
    while (added < seedIx.size()) {
      seedSpec.push_back(groups[0]);
      added += groups[0];
    }
  }
  else {
    size_t added = 0;
    for (size_t k = 0; k < groups.size(); k++) {
      if (groups[k] <= 0)
        throw meosException("Internal error");
     
      seedSpec.push_back(groups[k]);
      added += groups[k];
    }
    if (added < seedIx.size())
      seedSpec.push_back(seedIx.size() - added);
  }

  list< vector<int> > seedGroups(1);
  size_t groupIx = 0;
  for (size_t k = 0; k < seedIx.size(); ) {
    if (groupIx < seedSpec.size() && seedGroups.back().size() >= seedSpec[groupIx]) {
      groupIx++;
      seedGroups.push_back(vector<int>());
    }

    int value = seedIx[k].first;
    while (k < seedIx.size() && seedIx[k].first == value) {
      seedGroups.back().push_back(seedIx[k].second);
      k++;
    }
  }

  vector<pRunner> startOrder;

  for (list< vector<int> >::iterator it = seedGroups.begin();
       it != seedGroups.end(); ++it) {
    vector<int> &g = *it;
    permute(g);
    for (size_t k = 0; k < g.size(); k++) {
      startOrder.push_back(r[g[k]]);
    }
  }
  
  if (noClubNb) {
    set<int> pushed_back;
    for (size_t k = 1; k < startOrder.size(); k++) {
      int idMe = startOrder[k]->getClubId();
      if (idMe != 0 && idMe == startOrder[k-1]->getClubId()) {
        // Make sure the runner with worst ranking is moved back. (Swedish SM rules)
        bool skipRank = pushed_back.count(startOrder[k - 1]->getId()) != 0;
        if (!skipRank &&  startOrder[k-1]->getRanking() > startOrder[k]->getRanking())
          swap(startOrder[k-1], startOrder[k]);
        pushed_back.insert(startOrder[k]->getId());
        vector<pair<int, pRunner> > rqueue;
        rqueue.push_back(make_pair(k, startOrder[k]));
        for (size_t j = k + 1; j < startOrder.size(); j++) {
          if (idMe != startOrder[j]->getClubId()) {
            pushed_back.insert(startOrder[j]->getId());
            swap(startOrder[j], startOrder[k]); // k-1 now has a non-club nb behind
            rqueue.push_back(make_pair(j, pRunner(0)));
            // Shift the queue
            for (size_t q = 1; q < rqueue.size(); q++) {
              startOrder[rqueue[q].first] = rqueue[q-1].second;
            }
            break;
          }
          else {
            rqueue.push_back(make_pair(j, startOrder[j]));
          }

        }
        /*for (size_t j = k + 1; j < startOrder.size(); j++) {
          swap(startOrder[j], startOrder[j-1]);
          if (idMe != startOrder[j]->getClubId() && j+1 < startOrder.size() &&
              idMe != startOrder[j+1]->getClubId()) {
            break;
          }
        }*/
      }
    }
    // Handle special case where the two last have same club.
    int last = startOrder.size() - 1;
    if (last >= 3) {
      int lastClub = startOrder[last]->getClubId();
      if ( lastClub == startOrder[last-1]->getClubId() &&
           lastClub != startOrder[last-2]->getClubId() &&
           lastClub != startOrder[last-3]->getClubId() ) {
        swap(startOrder[last-1], startOrder[last-2]);
      }
    }
  }

  if (!reverseOrder)
    reverse(startOrder.begin(), startOrder.end());
  
  for (size_t k = 0; k < startOrder.size(); k++) {
    int kx = k/pairSize;
    startOrder[k]->setStartTime(firstStart + interval * kx, true, false, true);
    startOrder[k]->synchronize(true);
  }
}

bool oClass::hasClassGlobalDependence() const {
  for (size_t k = 0; k < legInfo.size(); k++) { 
    if (legInfo[k].startMethod == STHunting)
      return true;
  }
  return false;
}

int oClass::getDrawFirstStart() const {
  return getDCI().getInt("FirstStart");    
}

void oClass::setDrawFirstStart(int st) {
  getDI().setInt("FirstStart", st);
}

int oClass::getDrawInterval() const {
  return getDCI().getInt("StartInterval");    
}

void oClass::setDrawInterval(int st) {
  getDI().setInt("StartInterval", st);
}

int oClass::getDrawVacant() const {
  return getDCI().getInt("Vacant");    
}

void oClass::setDrawVacant(int st) {
  getDI().setInt("Vacant", st);
}

int oClass::getDrawNumReserved() const {
  return getDCI().getInt("Reserved") & 0xFF;    
}

void oClass::setDrawNumReserved(int st) {
  int v = getDCI().getInt("Reserved") & 0xFF00;
  getDI().setInt("Reserved", v|st);
}

void oClass::setDrawSpecification(const vector<DrawSpecified> &spec) {
  int flag = 0;
  for (auto ds : spec) {
    flag |= int(ds);
  }
  int v = getDrawNumReserved();
  getDI().setInt("Reserved", v | (flag<<8));
}

set<oClass::DrawSpecified> oClass::getDrawSpecification() const {
  int v = (getDCI().getInt("Reserved") & 0xFF00) >> 8;
  set<DrawSpecified> res;
 
  for (auto dk : DrawKeys) {
    if (int(dk) & v)
      res.insert(dk);
  }
  return res;
}

void oClass::initClassId(oEvent &oe) {
  vector<pClass> cls;
  oe.getClasses(cls, true);
  map<long long, wstring> id2Cls;
  for (size_t k = 0; k < cls.size(); k++) {
    long long extId = cls[k]->getExtIdentifier();
    if (extId > 0) {
      if (id2Cls.count(extId)) {
        throw meosException(L"Klasserna X och Y har samma externa id. Använd tabelläget för att ändra id.#" +
                            id2Cls[extId] + L"#" + cls[k]->getName()); 
      }
      id2Cls[extId] = cls[k]->getName();
    } 
  }
  // Generate external identifiers when not set
  for (size_t k = 0; k < cls.size(); k++) {
    long long extId = cls[k]->getExtIdentifier();
    if (extId <= 0) {
      long long id = cls[k]->getId();
      while (id2Cls.count(id)) {
        id += 100000;
      }
      id2Cls[id] = cls[k]->getName();
      cls[k]->setExtIdentifier(id);
    }
  }
}

int oClass::getNextBaseLeg(int leg) const {
  for (size_t k = leg + 1; k  < legInfo.size(); k++) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return k;
  }
  return -1; // No next leg
}

int oClass::getPreceedingLeg(int leg) const {
  if (size_t(leg) >= legInfo.size())
    leg = legInfo.size() - 1;
  for (int k = leg; k > 0; k--) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return k-1;
  }
  return -1;
}

bool oClass::lockedForking() const {
  return (getDCI().getInt("Locked") & 1) == 1;
}

void oClass::lockedForking(bool locked) {
  int current = getDCI().getInt("Locked");
  getDI().setInt("Locked", locked ? (current | 1) : (current & ~1));
}

bool oClass::lockedClassAssignment() const {
  return (getDCI().getInt("Locked") & 2) == 2;
}

void oClass::lockedClassAssignment(bool locked) {
  int current = getDCI().getInt("Locked");
  getDI().setInt("Locked", locked ? (current | 2) : (current & ~2));
}

oClass *oClass::getVirtualClass(int instance, bool allowCreation) {
  if (instance == 0)
    return this;
  if (parentClass)
    return parentClass->getVirtualClass(instance, allowCreation);

  if (size_t(instance) < virtualClasses.size() && virtualClasses[instance])
    return virtualClasses[instance];

  if (instance >= getNumQualificationFinalClasses())
    return this; // Invalid
  virtualClasses.resize(getNumQualificationFinalClasses());
  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return virtualClasses[instance];
  }
  configureInstance(instance, allowCreation);
  if (virtualClasses[instance])
    return virtualClasses[instance];
  return this; // Fallback
}

const pClass oClass::getVirtualClass(int instance) const {
  if (instance == 0)
    return pClass(this);
  if (parentClass)
    return parentClass->getVirtualClass(instance);

  if (size_t(instance) < virtualClasses.size() && virtualClasses[instance])
    return virtualClasses[instance];

  if (instance >= getNumQualificationFinalClasses())
    return pClass(this); // Invalid
  virtualClasses.resize(getNumQualificationFinalClasses());

  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return virtualClasses[instance];
  }
  configureInstance(instance, false);
  if (virtualClasses[instance])
    return virtualClasses[instance];
  return pClass(this); // Fallback
}

void oClass::configureInstance(int instance, bool allowCreation) const {
  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return;
  }
  if (!allowCreation)
    return;

  oClass copy(*this);
  copy.Id = Id + instance * MaxClassId;
  copy.setExtIdentifier(copy.Id);
  copy.Name += makeDash(L"-") + qualificatonFinal->getInstanceName(instance);
  copy.sqlUpdated.clear();
  copy.parentClass = pClass(this);
  copy.tSortIndex += instance;
  copy.getDI().setInt("SortIndex", copy.tSortIndex);
  copy.legInfo.clear();
  copy.MultiCourse.clear();
  copy.getDI().setString("Qualification", L"");
  virtualClasses[instance] = oe->addClass(copy);
}

int oClass::getNumQualificationFinalClasses() const {
  reinitialize(false);
  if (qualificatonFinal)
    return qualificatonFinal->getNumClasses()+1;
  return 0;
}

void oClass::loadQualificationFinalScheme(const wstring &fileName) {
  auto qf = make_shared<QualificationFinal>(MaxClassId, Id);
  qf->import(fileName);
  wstring enc;
  qf->encode(enc);
  int ns = qf->getNumStages();
  setNumStages(ns);
  for (int i = 1; i < ns; i++) {
    setStartType(i, StartTypes::STDrawn, true);
    setLegType(i, LegTypes::LTNormal);
    setLegRunner(i, 0);
  }
  // Clear any old scheme
  clearQualificationFinal();
  qualificatonFinal = qf;
  getDI().setString("Qualification", enc);
  for (int i = 0; i < qualificatonFinal->getNumClasses(); i++) {
    pClass inst = getVirtualClass(i+1, true);
    inst->synchronize();
  }
  synchronize();
  for (oRunner &r : oe->Runners) {
    if (r.getClassRef(false) == this) {
      pTeam t = r.getTeam();
      if (t == 0) {
        t = oe->addTeam(r.getName(), r.getClubId(), getId());
        t->setStartNo(r.getStartNo(), false);
        t->setRunner(0, &r, true);
      }
      r.synchronizeAll();
    }
  }
}

void oClass::updateFinalClasses(oRunner *causingResult, bool updateStartNumbers) {
  if (!qualificatonFinal)
    return;
  assert(!causingResult || causingResult->Class == this);

  //oe->gdibase.addStringUT(0, L"UF:" + getName() + L" for " + (causingResult ? causingResult->getName() : L"-"));

  int instance = causingResult ? causingResult->classInstance() : 0;
  pClass currentInst = getVirtualClass(instance, false);
  if (qualificatonFinal->isFinalClass(instance))
    return; // Final class
  if (instance > 0) {
    int baseLevel = qualificatonFinal->getLevel(instance);
    instance = qualificatonFinal->getMinInstance(baseLevel);
  }
  int maxDepth = getNumStages();
  bool needIter = true;
  int limit = virtualClasses.size() - 1;
  bool wasReset = false;

  while (needIter && --maxDepth > 0) {
    needIter = false;
    if (size_t(instance) >= virtualClasses.size())
      break; // Final class

    vector< vector<pRunner> > classSplit(virtualClasses.size());
    vector<pRunner> nonQualified;

    for (oRunner &r : oe->Runners) {
      if (r.isRemoved() || !r.Class)
        continue;

      if (r.Class != this && (r.Class->getId() % MaxClassId) != getId())
        continue;

      int inst = r.Class == this ? r.classInstance() : (r.Class->getId() - getId()) / MaxClassId;

      if (inst == 0 && r.tLeg > 0) {
        if (r.tLeg < maxDepth - 1 && r.Class == this)
          nonQualified.push_back(&r);
        continue; // Only allow base class for leg 0.
      }
      if (inst < instance || inst >= limit)
        continue;

      classSplit[inst].push_back(&r);
    }

    // Reset non-qualified
    if (!wasReset) {
      for (size_t i = 0; i < nonQualified.size(); i++) {
        pRunner r = nonQualified[i];
        pRunner next = r->getMultiRunner(r->tLeg + 1);
        if (next && next->getClassRef(true) != this) {
          pClass nextCls = next->getClassRef(true);
          if (!nextCls->lockedClassAssignment()) {
            wasReset = true;
            next->getDI().setInt("Heat", 0);
            nonQualified.push_back(next);
          }
        }
      }
      if (wasReset) { // Only do this once.
        maxDepth++;
        needIter = true;
        continue; // Redo
      }
    }
    GeneralResult gr;
    qualificatonFinal->prepareCalculations();

    for (int i = instance; i < limit; i++) {
      if (classSplit[i].empty())
        continue;

      if (i == 0 && qualificatonFinal->noQualification(i)) {
        set<int> allowed;
        qualificatonFinal->getBaseClassInstances(allowed);
        // Place all in this group
        for (int rix = classSplit[0].size() - 1; rix >= 0; rix--) {
          pRunner r = classSplit[0][rix];
          auto di = r->getDI();
          int oldHeat = di.getInt("Heat");
          if (allowed.count(oldHeat) || classSplit.size() < 2)
            continue;
          // Take the smallest group. User can set heat explicitly of other distribution is wanted.
          int heat = 1;
          for (int i : allowed) {
            if (size_t(i) < classSplit.size() &&
                classSplit[heat].size() > classSplit[i].size())
              heat = i;
          }
          if (heat != oldHeat) {
            bool lockedStartList = getVirtualClass(heat)->lockedClassAssignment() ||
              getVirtualClass(oldHeat)->lockedClassAssignment();

            if (!lockedStartList) {
              classSplit[heat].push_back(r);
              classSplit[0].erase(classSplit[0].begin() + rix);
              pClass oldClass = r->getClassRef(true);
              oldClass->markSQLChanged(-1, 0);
              di.setInt("Heat", heat);
              r->classInstanceRev.first = -1;
              r->synchronize();
            }
          }
        }
      }

      gr.calculateIndividualResults(classSplit[i], oListInfo::Classwise, true, 0);
      int lastPlace = 0, orderPlace = 1;
      int numEqual = 0;
      for (size_t k = 0; k < classSplit[i].size(); k++) {
        auto &res = classSplit[i][k]->getTempResult();
        //int heat = 0;
        if (res.getStatus() == StatusOK) {
          int place = res.getPlace();
          if (lastPlace == place)
            numEqual++;
          else
            numEqual = 0;

          qualificatonFinal->setupNextFinal(classSplit[i][k], i, orderPlace, numEqual);
          //auto nextFinal = qualificatonFinal->getNextFinal(i, orderPlace, numEqual);
          //heat = nextFinal.first;
          lastPlace = place;
        }
        orderPlace++;
      }
    }

    qualificatonFinal->computeFinals();

    for (int i = instance; i < limit; i++) {
      if (classSplit[i].empty())
        continue;

      for (size_t k = 0; k < classSplit[i].size(); k++) {
        oRunner &thisRunner = *classSplit[i][k];
        pRunner runnerToChange = thisRunner.getMultiRunner(thisRunner.getRaceNo() + 1);

        if (runnerToChange) {
          auto res = qualificatonFinal->getNextFinal(thisRunner.getId());
          int heat = res.first;

          auto di = runnerToChange->getDI();
          int oldHeat = di.getInt("Heat");
          
          if (heat != oldHeat) {
            bool lockedStartList = (heat != 0 && getVirtualClass(heat)->lockedClassAssignment()) ||
                                   getVirtualClass(oldHeat)->lockedClassAssignment();

            if (!lockedStartList) {
              pClass oldClass = runnerToChange->getClassRef(true);
              oldClass->markSQLChanged(-1, 0);
              di.setInt("Heat", heat);
              runnerToChange->classInstanceRev.first = -1;
              //oe->gdibase.addStringUT(0, L"HU:" + thisRunner.getName() + L" " + itow(oldHeat) + L"->" + itow(heat));
              runnerToChange->apply(false, nullptr, false);
              runnerToChange->synchronize();
              if (runnerToChange->getFinishTime() > 0)
                needIter = true;
            }
          }
        }
      }
      if (needIter) {
        instance = i+1; // Need not process last class again
        break;
      }
    }
  }
}

vector<pair<wstring, size_t>> oClass::getAllFees() const {
  set<int> fees;
  int f = getDCI().getInt("ClassFee");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("ClassFeeRed");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("HighClassFee");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("HighClassFeeRed");
  if (f > 0)
    fees.insert(f);

  if (fees.empty()) {
    f = oe->getDCI().getInt("EliteFee");
    if (f > 0)
      fees.insert(f);

    f = oe->getDCI().getInt("EntryFee");
    if (f > 0)
      fees.insert(f);

    f = oe->getDCI().getInt("YouthFee");
    if (f > 0)
      fees.insert(f);
  }
  vector< pair<wstring, size_t> > ff;
  for (set<int>::iterator it = fees.begin(); it != fees.end(); ++it)
    ff.emplace_back(oe->formatCurrency(*it), *it);

  return ff;
}

bool oEvent::hasAnyRestartTime() const {
  for (auto &c : Classes) {
    if (c.isRemoved())
      continue;

    for (auto &leg : c.legInfo) {
      if (leg.legRopeTime > 0 && leg.legRestartTime > 0)
        return true;
    }
  }

  return false;
}
