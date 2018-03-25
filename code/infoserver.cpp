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


#include "stdafx.h"
#include "meos_util.h"
#include "infoserver.h"
#include "xmlparser.h"
#include "oEvent.h"
#include "download.h"
#include "progress.h"
#include "meosException.h"
#include "gdioutput.h"

void base64_encode(const vector<BYTE> &input, string &output);
extern gdioutput *gdi_main;

// Encode a vector vector int {{1}, {1,2,3}, {}, {4,5}} as "1;1,2,3;;4,5"
static void packIntInt(const vector< vector<int> > &v, wstring &def) {
  def = L"";
  for (size_t j = 0; j < v.size(); j++) {
    if (j>0)
      def += L";";
    for (size_t k = 0; k < v[j].size(); k++) {
      if (k>0)
        def += L",";
      def += itow(v[j][k]);
    }
  }
}

// Encode a vector vector int {1,2,3} as "1,2,3"
static void packInt(const vector<int> &v, wstring &def) {
  def = L"";
  for (size_t j = 0; j < v.size(); j++) {
    if (j>0)
      def += L",";
    def += itow(v[j]);
  }
}

InfoBase::InfoBase(int idIn) : id(idIn), committed(false){
}

InfoBase::InfoBase(const InfoBase &src) : id(src.id), committed(src.committed){
}

void InfoBase::operator=(const InfoBase &src) {
}

InfoBase::~InfoBase() {
}

int InfoBase::convertRelativeTime(const oBase &elem, int t) {
    return t+elem.getEvent()->getZeroTimeNum();
}

InfoCompetition::InfoCompetition(int id) : InfoBase(id) {
  forceComplete = true;
  includeTotal = false;
}

InfoRadioControl::InfoRadioControl(int id) : InfoBase(id) {
}

InfoClass::InfoClass(int id) : InfoBase(id) {
}

InfoOrganization::InfoOrganization(int id) : InfoBase(id) {
}

InfoBaseCompetitor::InfoBaseCompetitor(int id) : InfoBase(id) {
  organizationId = 0;
  classId = 0;
  status = 0;
  startTime = 0;
  runningTime = 0;
}

InfoCompetitor::InfoCompetitor(int id) : InfoBaseCompetitor(id) {
  totalStatus = 0;
  inputTime = 0;
  course = 0;
}

InfoTeam::InfoTeam(int id) : InfoBaseCompetitor(id) {
}


bool InfoCompetition::synchronize(oEvent &oe, bool onlyCmp, const set<int> &includeCls, const set<int> &ctrls) {
  bool changed = false;
  if (oe.getName() != name) {
    name = oe.getName();
    changed = true;
  }

  if (oe.getDate() != date) {
    date = oe.getDate();
    changed = true;
  }

  if (oe.getDCI().getString("Organizer") != organizer) {
    organizer = oe.getDCI().getString("Organizer");
    changed = true;
  }

  if (oe.getDCI().getString("Homepage") != homepage) {
    homepage = oe.getDCI().getString("Homepage");
    changed = true;
  }

  if (changed)
    needCommit(*this);
  
  if (onlyCmp)
    return changed;

  vector<pControl> ctrl;
  oe.getControls(ctrl, true);
  set<int> knownId;
  for (size_t k = 0; k < ctrl.size(); k++) {
    vector<int> ids;
    ctrl[k]->getCourseControls(ids);
    for (size_t j = 0; j < ids.size(); j++) {
      int wid = ids[j];
      if (!ctrls.count(ids[j])) 
        continue;
      knownId.insert(wid);
      map<int, InfoRadioControl>::iterator res = controls.find(wid);
      if (res == controls.end())
        res = controls.insert(make_pair(wid, InfoRadioControl(wid))).first;
      if (res->second.synchronize(*ctrl[k], ids.size() > 1 ? j+1 : 0))
        needCommit(res->second);
    }  
  }

  // Check if something was deleted
  for (map<int, InfoRadioControl>::iterator it = controls.begin(); it != controls.end();) {
    if (!knownId.count(it->first)) {
      controls.erase(it++);
      forceComplete = true;
    }
    else
      ++it;
  }
  knownId.clear();

  vector<pClass> cls;
  oe.getClasses(cls, false);
  for (size_t k = 0; k < cls.size(); k++) {
    int wid = cls[k]->getId();
    if (!includeCls.count(wid))
      continue;
    knownId.insert(wid);
    map<int, InfoClass>::iterator res = classes.find(wid);
    if (res == classes.end())
      res = classes.insert(make_pair(wid, InfoClass(wid))).first;
    if (res->second.synchronize(withCourse, *cls[k], ctrls))
      needCommit(res->second);
  }

  // Check if something was deleted
  for (map<int, InfoClass>::iterator it = classes.begin(); it != classes.end();) {
    if (!knownId.count(it->first)) {
      classes.erase(it++);
      forceComplete = true;
    }
    else
      ++it;
  }
  knownId.clear();

  vector<pClub> clb;
  oe.getClubs(clb, false);
  for (size_t k = 0; k < clb.size(); k++) {
    int wid = clb[k]->getId();
    knownId.insert(wid);
    map<int, InfoOrganization>::iterator res = organizations.find(wid);
    if (res == organizations.end())
      res = organizations.insert(make_pair(wid, InfoOrganization(wid))).first;
    if (res->second.synchronize(*clb[k]))
      needCommit(res->second);
  }

  // Check if something was deleted
  for (map<int, InfoOrganization>::iterator it = organizations.begin(); it != organizations.end();) {
    if (!knownId.count(it->first)) {
      organizations.erase(it++);
      forceComplete = true;
    }
    else
      ++it;
  }
  knownId.clear();

  vector<pTeam> t;
  oe.getTeams(0, t, false);
  for (size_t k = 0; k < t.size(); k++) {
    if (!includeCls.count(t[k]->getClassId(true)))
      continue;
    int wid = t[k]->getId();
    knownId.insert(wid);
    map<int, InfoTeam>::iterator res = teams.find(wid);
    if (res == teams.end())
      res = teams.insert(make_pair(wid, InfoTeam(wid))).first;
    if (res->second.synchronize(*t[k]))
      needCommit(res->second);
  }

  // Check if something was deleted
  for (map<int, InfoTeam>::iterator it = teams.begin(); it != teams.end();) {
    if (!knownId.count(it->first)) {
      teams.erase(it++);
      forceComplete = true;
    }
    else
      ++it;
  }
  knownId.clear();

  vector<pRunner> r;
  oe.getRunners(0, 0, r, false);
  for (size_t k = 0; k < r.size(); k++) {
    if (!includeCls.count(r[k]->getClassId(true)))
      continue;
    int wid = r[k]->getId();
    knownId.insert(wid);
    map<int, InfoCompetitor>::iterator res = competitors.find(wid);
    if (res == competitors.end())
      res = competitors.insert(make_pair(wid, InfoCompetitor(wid))).first;
    if (res->second.synchronize(*this, *r[k]))
      needCommit(res->second);
  }

  // Check if something was deleted
  for (map<int, InfoCompetitor>::iterator it = competitors.begin(); it != competitors.end();) {
    if (!knownId.count(it->first)) {
      competitors.erase(it++);
      forceComplete = true;
    }
    else
      ++it;
  }
  knownId.clear();

  return !toCommit.empty() || forceComplete;
}

void InfoCompetition::needCommit(InfoBase &obj) {
  toCommit.push_back(&obj);
}

bool InfoRadioControl::synchronize(oControl &c, int number) {
  wstring n = c.hasName() ? c.getName() : c.getString();
  if (number > 0)
    n = n + L"-" + itow(number);
  if (n == name)
    return false;
  else {
    name = n;
    modified();
  }
  return true;
}

void InfoRadioControl::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > prop;
  prop.push_back(make_pair("id", itow(getId())));
  xml.write("ctrl", prop, name);
}

bool InfoClass::synchronize(bool includeCourses, oClass &c, const set<int> &ctrls) {
  const wstring &n = c.getName();
  int no = c.getSortIndex();
  bool mod = false;
  
  vector< vector<int> > rc;
  size_t s = c.getNumStages();
  
  if (includeCourses) {
    set<int> crsSet;
    for (size_t i = 0; i <= s; i++) {
      vector<pCourse> crs;
      c.getCourses(i, crs);
      for (pCourse pc : crs)
        crsSet.insert(pc->getId());
    }
    vector<int> newCrs(crsSet.begin(), crsSet.end());

    if (newCrs != courses) {
      courses = newCrs;
      mod = true;
    }
  }

  if (s > 0) {
    linearLegNumberToActual.clear();

    for (size_t k = 0; k < s; k++) {
      if (!c.isParallel(k) && !c.isOptional(k)) {
        pCourse pc = c.getCourse(k, 0, true); // Get a course representative for the leg.

        rc.push_back(vector<int>());
        if (pc) {
          vector<pControl> ctrl;
          pc->getControls(ctrl);
          for (size_t j = 0; j < ctrl.size(); j++) {
            if (ctrls.count(pc->getCourseControlId(j))) {
              rc.back().push_back(pc->getCourseControlId(j));
            }
          }
        }
      }
      // Setup transformation map (flat to 2D)
      linearLegNumberToActual.push_back(max<int>(0, rc.size()-1));

    }
  }
  else {
    // Single stage
    linearLegNumberToActual.resize(1, 0);
    pCourse pc = c.getCourse(true); // Get a course representative for the leg.
    rc.push_back(vector<int>());
    if (pc) {
      vector<pControl> ctrl;
      pc->getControls(ctrl);
      for (size_t j = 0; j < ctrl.size(); j++) {
        if (ctrls.count(pc->getCourseControlId(j))) {
          rc.back().push_back(pc->getCourseControlId(j));
        }
      }
    }
  }

  if (radioControls != rc) {
    radioControls = rc;
    mod = true;
  }

  if (n != name || no != sortOrder) {
    name = n;
    sortOrder = no;
    mod = true;
  }

  if (mod)
    modified();
  return mod;
}

void InfoClass::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > prop;
  prop.push_back(make_pair("id", itow(getId())));
  prop.push_back(make_pair("ord", itow(sortOrder)));
  wstring def;
  packIntInt(radioControls, def);
  prop.push_back(make_pair("radio", def));
  if (courses.size() > 0) {
    packInt(courses, def);
    prop.push_back(make_pair("crs", def));
  }
  xml.write("cls", prop, name);
}

bool InfoOrganization::synchronize(oClub &c) {
  const wstring &n = c.getDisplayName();
  if (n == name)
    return false;
  else {
    name = n;
    modified();
  }
  return true;
}

void InfoOrganization::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > prop;
  prop.push_back(make_pair("id", itow(getId())));
  xml.write("org", prop, name);
}

void InfoCompetition::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > prop;
  prop.push_back(make_pair("date", date));
  prop.push_back(make_pair("organizer", organizer));
  prop.push_back(make_pair("homepage", homepage));
  xml.write("competition", prop, name);
}

void InfoBaseCompetitor::serialize(xmlbuffer &xml, bool diffOnly, int course) const {
  vector< pair<string, wstring> > prop;
  prop.reserve(10);
  prop.emplace_back("org", itow(organizationId));
  prop.emplace_back("cls", itow(classId));
  prop.emplace_back("stat", itow(status));
  prop.emplace_back("st", itow(startTime));
  prop.emplace_back("rt", itow(runningTime));
  if (course != 0)
    prop.emplace_back("crs", itow(course));

  if (!bib.empty()) {
    prop.emplace_back("bib", bib);
  }

  xml.write("base", prop, name);
}

bool InfoBaseCompetitor::synchronizeBase(oAbstractRunner &bc) {
  const wstring &n = bc.getName();
  bool ch = false;
  if (n != name) {
    name = n;
    ch = true;
  }

  int cid = bc.getClubId();
  if (cid != organizationId) {
    organizationId = cid;
    ch = true;
  }

  int cls = bc.getClassId(true);
  if (cls != classId) {
    classId = cls;
    ch = true;
  }

  int s = bc.getStatus();
  if (status != s) {
    status = s;
    ch = true;
  }

  int st = -1;
  if (bc.startTimeAvailable())
    st = convertRelativeTime(bc, bc.getStartTime()) * 10;
  
  if (st != startTime) {
    startTime = st;
    ch = true;
  }

  int rt = bc.getRunningTime() * 10;
  if (rt != runningTime) {
    runningTime = rt;
    ch = true;
  }

  wstring newBib = bc.getBib();
  if (bib != newBib) {
    bib = newBib;
    ch = true;
  }

  return ch;
}

bool InfoCompetitor::synchronize(bool useTotalResults, bool useCourse, oRunner &r) {
  bool ch = synchronizeBase(r);
  changeTotalSt = r.getEvent()->hasPrevStage() || r.getLegNumber()>0; // Always write full attributes
  
  int s = StatusOK;
  int legInput = 0;
  
  int oldCourse = course;
  if (useCourse) {
    auto crs = r.getCourse(false);
    course = crs ? crs->getId() : 0;
  }
  else {
    course = 0;
  }

  if (oldCourse != course)
    ch = true;

  pTeam t = r.getTeam();
  if (useTotalResults) {
    legInput = r.getTotalTimeInput() * 10;
    s = r.getTotalStatusInput();
  }
  else if (t && r.getLegNumber() > 0) {
    legInput = t->getLegRunningTime(r.getLegNumber() - 1, false) * 10;
    s  = t->getLegStatus(r.getLegNumber() - 1, false);
  }

  if (totalStatus != s) {
    totalStatus = s;
    ch = true;
    changeTotalSt = true;
  }

  if (legInput != inputTime) {
    inputTime = legInput;
    ch = true;
    changeTotalSt = true;
  }
   
  return ch;
}

bool InfoCompetitor::synchronize(const InfoCompetition &cmp, oRunner &r) {
  bool useTotalResults = cmp.includeTotalResults();
  bool inludeCourse = cmp.includeCourse();

  bool ch = synchronize(useTotalResults, inludeCourse, r);

  vector<RadioTime> newRT;
  if (r.getClassId(false) > 0)  {
    const vector<int> &radios = cmp.getControls(r.getClassId(false), r.getLegNumber());
    for (size_t k = 0; k < radios.size(); k++) {
      RadioTime radioTime;
      RunnerStatus s_split;
      radioTime.radioId = radios[k];
      r.getSplitTime(radioTime.radioId, s_split, radioTime.runningTime);

      if (radioTime.runningTime > 0) {
        radioTime.runningTime*=10;
        newRT.push_back(radioTime);
      }
    }
  }
  changeRadio = radioTimes.size() > 0;//false; // Always write full attributes
  if (newRT != radioTimes) {
    ch = true;
    changeRadio = true;
    radioTimes.swap(newRT);
  }

  if (ch)
    modified();
  return ch;
}

void InfoCompetitor::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > sprop;
  sprop.push_back(make_pair("id", itow(getId())));
  xmlbuffer &subTag = xml.startTag("cmp", sprop);
  InfoBaseCompetitor::serialize(subTag, diffOnly, course);

  if (radioTimes.size() > 0 && (!diffOnly || changeRadio)) {
    string radio;
    radio.reserve(radioTimes.size() * 12);
    for (size_t k = 0; k < radioTimes.size(); k++) {
      if (k>0)
        radio+=";";
      radio+=itos(radioTimes[k].radioId);
      radio+=",";
      radio+=itos(radioTimes[k].runningTime);
    }
     vector< pair<string, string> > eprop;
    subTag.write("radio", eprop, radio);
  }
  if (!diffOnly || changeTotalSt) {
    vector< pair<string, string> > prop;
    prop.push_back(make_pair("it", itos(inputTime)));
    prop.push_back(make_pair("tstat", itos(totalStatus)));
    subTag.write("input", prop, "");
  }
  xml.endTag();
}


bool InfoTeam::synchronize(oTeam &t) {
  bool ch = synchronizeBase(t);

  const pClass cls = t.getClassRef(true);
  if (cls) {
    vector< vector<int> > r;

    size_t s = cls->getNumStages();
    for (size_t k = 0; k < s; k++) {
      pRunner rr = t.getRunner(k);
      int rid = rr != 0 ? rr->getId() : 0;

      if (cls->isParallel(k) || cls->isOptional(k)) {
        if (r.empty())
          r.push_back(vector<int>()); // This is not a valid case, really
        r.back().push_back(rid);
      }
      else
        r.push_back(vector<int>(1, rid));
    }

    if (r != competitors) {
      r.swap(competitors);
      ch = true;
    }

  }
  if (ch)
    modified();
  return ch;
}

void InfoTeam::serialize(xmlbuffer &xml, bool diffOnly) const {
  vector< pair<string, wstring> > prop;
  prop.push_back(make_pair("id", itow(getId())));
  xmlbuffer &sub = xml.startTag("tm", prop);
  InfoBaseCompetitor::serialize(sub, diffOnly, 0);
  wstring def;
  packIntInt(competitors, def);
  prop.clear();
  sub.write("r", prop, def);
  sub.endTag();
}

const vector<int> &InfoCompetition::getControls(int classId, int legNumber) const {
  map<int, InfoClass>::const_iterator res = classes.find(classId);

  if (res != classes.end()) {
    if (size_t(legNumber) < res->second.linearLegNumberToActual.size())
      legNumber = res->second.linearLegNumberToActual[legNumber];
    else
      legNumber = 0;
    const vector< vector<int> > &c = res->second.radioControls;
    if (size_t(legNumber) < c.size())
      return c[legNumber];
  }
  throw meosException("Internal class definition error");
}

void InfoCompetition::getCompleteXML(xmlbuffer &xml) {
  xml.setComplete(true);
  serialize(xml, false);

  for(map<int, InfoRadioControl>::iterator it = controls.begin(); it != controls.end(); ++it) {
    it->second.serialize(xml, false);
  }

  for(map<int, InfoClass>::iterator it = classes.begin(); it != classes.end(); ++it) {
    it->second.serialize(xml, false);
  }

  for(map<int, InfoOrganization>::iterator it = organizations.begin(); it != organizations.end(); ++it) {
    it->second.serialize(xml, false);
  }

  for(map<int, InfoTeam>::iterator it = teams.begin(); it != teams.end(); ++it) {
    it->second.serialize(xml, false);
  }

  for(map<int, InfoCompetitor>::iterator it = competitors.begin(); it != competitors.end(); ++it) {
    it->second.serialize(xml, false);
  }
}

void InfoCompetition::getDiffXML(xmlbuffer &xml) {
  if (forceComplete) {
    getCompleteXML(xml);
    return;
  }
  xml.setComplete(false);
  for (list<InfoBase *>::iterator it = toCommit.begin(); it != toCommit.end(); ++it) {
    (*it)->serialize(xml, true);
  }
}

void InfoCompetition::commitComplete() {
  toCommit.clear();
  forceComplete = false;
}

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};

static int mod_table[] = {0, 2, 1};

void base64_encode(const vector<BYTE> &input, string &encoded_data) {

  size_t input_length = input.size();
  size_t output_length = 4 * ((input_length + 2) / 3);
  encoded_data.resize(output_length);

  for (size_t i = 0, j = 0; i < input_length;) {
    unsigned octet_a = i < input_length ? input[i++] : 0;
    unsigned octet_b = i < input_length ? input[i++] : 0;
    unsigned octet_c = i < input_length ? input[i++] : 0;

    unsigned triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_length % 3]; i++)
    encoded_data[output_length - 1 - i] = '=';
}

xmlbuffer &xmlbuffer::startTag(const char *tag, const vector< pair<string, wstring> > &prop) {
  blocks.push_back(block());
  blocks.back().tag = tag;
  blocks.back().prop = prop;
  blocks.back().subValues.push_back(xmlbuffer());
  return blocks.back().subValues.back();
}

void xmlbuffer::endTag() {
}

void xmlbuffer::write(const char *tag,
                      const vector< pair<string, string> > &prop,
                      const string &value) {
  blocks.push_back(block());
  blocks.back().tag = tag;
  for (size_t k = 0; k < prop.size(); k++)
    blocks.back().prop.push_back(make_pair(prop[k].first, gdi_main->widen(prop[k].second)));
  blocks.back().value = gdi_main->widen(value);
}

void xmlbuffer::write(const char *tag,
                      const vector< pair<string, wstring> > &prop,
                      const wstring &value) {
  blocks.push_back(block());
  blocks.back().tag = tag;
  blocks.back().prop = prop;
  blocks.back().value = value;
}



void xmlbuffer::startXML(xmlparser &xml, const wstring &dest) {
  xml.openOutput(dest.c_str(), false);
  if (complete) {
    xml.startTag("MOPComplete", "xmlns", "http://www.melin.nu/mop");
    complete = false;
  }
  else
  xml.startTag("MOPDiff", "xmlns", "http://www.melin.nu/mop");
}

bool xmlbuffer::commit(xmlparser &xml, int count) {
  while (count>0 && !blocks.empty()) {
    block &block = blocks.front();

    if (block.subValues.empty()) {
      xml.write(block.tag.c_str(), block.prop, block.value);
    }
    else {
      vector<wstring> p2;
      for (size_t k = 0; k< block.prop.size(); k++) {
        p2.push_back(gdi_main->widen(block.prop[k].first));
        p2.push_back(block.prop[k].second);
      }
      xml.startTag(block.tag.c_str(), p2);

      for (size_t k = 0; k < block.subValues.size(); k++)
        block.subValues[k].commit(xml, numeric_limits<int>::max());

      xml.endTag();
    }

    count--;
    blocks.pop_front();
  }

  return !blocks.empty();
}
