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
#include "RunnerDB.h"
#include "xmlparser.h"
#include "oRunner.h"
#include "Table.h"

#include "io.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "meos_util.h"
#include "oDataContainer.h"
#include "meosException.h"

#include <algorithm>
#include <cassert>
#include "intkeymapimpl.hpp"

#include "oEvent.h"

extern gdioutput *gdi_main;

int RunnerDB::cellEntryIndex = -1;

RunnerDB::RunnerDB(oEvent *oe_): oe(oe_)
{
  loadedFromServer = false;
  dataDate = 20100201;
  dataTime = 222222;
  runnerTable = 0;
  clubTable = 0;
}

RunnerDB::~RunnerDB(void)
{
  releaseTables();
}

RunnerDBEntry::RunnerDBEntry()
{
  memset(this, 0, sizeof(RunnerDBEntry));
}

RunnerWDBEntry::RunnerWDBEntry()
{
  name[0] = 0;
}

void RunnerWDBEntry::initName() const {
  if (name[0] == 0) {
    memset(name, 0, sizeof(wchar_t) * baseNameLength);
    const RunnerDBEntry &db = dbe();
    if (db.isUTF()) {
      int len = strlen(db.name);
      len = min(len+1, baseNameLengthUTF-1);
      int wlen = MultiByteToWideChar(CP_UTF8, 0, db.name, len, name, baseNameLength);
      if (wlen == 0)
        wlen = baseNameLength;
      name[wlen-1] = 0;
    }
    else {
      const wstring &wn = gdi_main->recodeToWide(db.name);
      wcsncpy_s(name, wn.c_str(), baseNameLength-1);
    }
  }
}

void RunnerWDBEntry::recode(const RunnerDBEntry &dest) const {
  initName();
  RunnerDBEntry &d = const_cast<RunnerDBEntry &>(dest);
  const int blen = min<int>(wcslen(name)+1, baseNameLength);
  int res = WideCharToMultiByte(CP_UTF8, 0, name, blen, d.name, baseNameLengthUTF-1, 0, 0);
  assert(res < baseNameLengthUTF);
  if (res == 0 && blen > 0)
    res = baseNameLengthUTF-1;
  d.name[res] = 0;
  d.setUTF();
}

RunnerDBEntryV1::RunnerDBEntryV1()
{
  memset(this, 0, sizeof(RunnerDBEntryV1));
}

RunnerDBEntryV2::RunnerDBEntryV2()
{
  memset(this, 0, sizeof(RunnerDBEntryV2));
}

RunnerDBEntryV3::RunnerDBEntryV3()
{
  memset(this, 0, sizeof(RunnerDBEntryV3));
}

void RunnerWDBEntry::getName(wstring &n) const
{
  initName();
  n=name;
}

const wchar_t *RunnerWDBEntry::getNameCstr() const {
  initName();
  return name;
}

void RunnerWDBEntry::setName(const wchar_t *n)
{
  const int blen = min<int>(wcslen(n)+1, baseNameLength);
  memset(name, 0, sizeof(wchar_t) * baseNameLength);
  memcpy(name, n, sizeof(wchar_t) * blen);
  name[baseNameLength-1]=0;

  RunnerDBEntry &d = dbe();
  int res = WideCharToMultiByte(CP_UTF8, 0, name, blen, d.name, baseNameLengthUTF-1, 0, 0);
  d.setUTF();
  assert(res < baseNameLengthUTF);
  if (res == 0 && blen > 0) {
    res = baseNameLengthUTF-1;
    d.name[res] = 0;
    name[0] = 0;
    initName();
  }
  d.name[res] = 0;
}

void RunnerWDBEntry::setNameUTF(const char *n)
{
  const int blen = min<int>(strlen(n)+1, baseNameLength);
  RunnerDBEntry &d = dbe();
  memcpy(d.name, n, blen);
  d.name[baseNameLength-1]=0;
  d.setUTF();
  name[0] = 0;
}

string RunnerDBEntry::getNationality() const
{
  if (national[0] < 30)
    return _EmptyString;

  string n("   ");
  n[0] = national[0];
  n[1] = national[1];
  n[2] = national[2];
  return n;
}

string RunnerDBEntry::getSex() const
{
  if (sex == 0)
    return _EmptyString;
  string n("W");
  n[0] = sex;
  return n;
}


wstring RunnerWDBEntry::getNationality() const
{
  const RunnerDBEntry &d = dbe();
  if (d.national[0] < 30)
    return _EmptyWString;

  wstring n(L"   ");
  n[0] = d.national[0];
  n[1] = d.national[1];
  n[2] = d.national[2];
  return n;
}

wstring RunnerWDBEntry::getSex() const
{
  if (dbe().sex == 0)
    return _EmptyWString;
  wstring n(L"W");
  n[0] = dbe().sex;
  return n;
}

wstring RunnerWDBEntry::getGivenName() const
{
  initName();
  return ::getGivenName(name);
}

wstring RunnerWDBEntry::getFamilyName() const
{
  initName();
  return ::getFamilyName(name);
}

__int64 RunnerWDBEntry::getExtId() const
{
  return dbe().extId;
}

void RunnerWDBEntry::setExtId(__int64 id)
{
  dbe().extId = id;
}

void RunnerDBEntryV2::init(const RunnerDBEntryV1 &dbe)
{
  memcpy(this, &dbe, sizeof(RunnerDBEntryV1));
  extId = 0;
}

void RunnerDBEntry::init(const RunnerDBEntryV2 &dbe)
{
  memcpy(name, dbe.name, 32);

  cardNo = dbe.cardNo;
  clubNo = dbe.clubNo;
  national[0] = dbe.national[0];
  national[1] = dbe.national[1];
  national[2] = dbe.national[2];
  sex = dbe.sex;
  birthYear = dbe.birthYear;
  reserved = dbe.reserved;
  extId = dbe.extId;
}

void RunnerDBEntry::init(const RunnerDBEntryV3 &dbe)
{
  memcpy(name, dbe.name, 56);

  cardNo = dbe.cardNo;
  clubNo = dbe.clubNo;
  national[0] = dbe.national[0];
  national[1] = dbe.national[1];
  national[2] = dbe.national[2];
  sex = dbe.sex;
  birthYear = dbe.birthYear;
  reserved = dbe.reserved;
  extId = dbe.extId;
}



void RunnerWDBEntry::init(RunnerDB *p, size_t ixin) {
  owner = p;
  ix = ixin;
}
  
RunnerWDBEntry *RunnerDB::addRunner(const wchar_t *name,
                                    __int64 extId,
                                    int club, int card)
{
  assert(rdb.size() == rwdb.size());
  rdb.push_back(RunnerDBEntry());
  rwdb.push_back(RunnerWDBEntry());
  rwdb.back().init(this, rdb.size()-1);

  RunnerWDBEntry &e=rwdb.back();
  RunnerDBEntry &en=rdb.back();
  
  en.cardNo = card;
  en.clubNo = club;
  e.setName(name);
  en.extId = extId;

  if (!check(en) ) {
    rdb.pop_back();
    rwdb.pop_back();
    return 0;
  } else {
    if (card>0)
      rhash[card]=rdb.size()-1;
    if (!idhash.empty())
      idhash[extId] = rdb.size()-1;
    if (!nhash.empty())
      nhash.insert(pair<wstring, int>(canonizeName(e.name), rdb.size()-1));
  }
  return &e;
}


RunnerWDBEntry *RunnerDB::addRunner(const char *nameUTF,
                                    __int64 extId,
                                    int club, int card)
{
  assert(rdb.size() == rwdb.size());
  rdb.push_back(RunnerDBEntry());
  rwdb.push_back(RunnerWDBEntry());
  rwdb.back().init(this, rdb.size()-1);

  RunnerWDBEntry &e=rwdb.back();
  RunnerDBEntry &en=rdb.back();
  
  en.cardNo = card;
  en.clubNo = club;
  e.setNameUTF(nameUTF);
  en.extId = extId;

  if (!check(en) ) {
    rdb.pop_back();
    rwdb.pop_back();
    return 0;
  } else {
    if (card>0)
      rhash[card]=rdb.size()-1;
    if (!idhash.empty())
      idhash[extId] = rdb.size()-1;
    if (!nhash.empty()) {
      wstring wn;
      e.getName(wn);
      nhash.insert(pair<wstring, int>(canonizeName(wn.c_str()), rdb.size()-1));
    }
  }
  return &e;
}

int RunnerDB::addClub(oClub &c, bool createNewId) {
  if (createNewId) {
    oDBClubEntry ce(c, cdb.size(), this);
    cdb.push_back(ce);
    int b = 0;
    while(++b<0xFFFF) {
      int off = (rand() & 0xFFFF);
      int newId = 10000 + off;
      int dummy;
      if (!chash.lookup(newId, dummy)) {
        cdb.back().Id = newId;
        chash[c.getId()]=cdb.size()-1;
        return newId;
      }
    }
    cdb.pop_back();
    throw meosException("Internal database error");
  }

  int value;
  if (!chash.lookup(c.getId(), value)) {
    oDBClubEntry ce(c, cdb.size(), this);
    cdb.push_back(ce);
    chash[c.getId()]=cdb.size()-1;
  }
  else {
    oDBClubEntry ce(c, value, this);
    cdb[value] = ce;
  }
  return c.getId();
}

void RunnerDB::importClub(oClub &club, bool matchName)
{
  pClub pc = getClub(club.getId());

  if (pc && !pc->sameClub(club)) {
    //The new id is used by some other club.
    //Remap old club first

    int oldId = pc->getId();
    int newId = chash.size() + 1;//chash.rbegin()->first + 1;

    for (size_t k=0; k<rdb.size(); k++)
      if (rdb[k].clubNo == oldId)
        rdb[k].clubNo = newId;

    chash[newId] = chash[oldId];
    pc = 0;
  }

  if (pc == 0 && matchName) {
    // Try to find the club under other id
    pc = getClub(club.getName());

    if ( pc!=0 ) {
      // If found under different id, remap to new id
      int oldId = pc->getId();
      int newId = club.getId();

      for (size_t k=0; k<rdb.size(); k++)
        if (rdb[k].clubNo == oldId)
          rdb[k].clubNo = newId;

      pClub base = &cdb[0];
      int index = (size_t(pc) - size_t(base)) / sizeof(oDBClubEntry);
      chash[newId] = index;
    }
  }

  if ( pc )
    *pc = club; // Update old club
  else {
    // Completely new club
    chash [club.getId()] = cdb.size();
    cdb.push_back(oDBClubEntry(club, cdb.size(), this));
  }
}

void RunnerDB::compactifyClubs()
{
  chash.clear();
  clubHash.clear();
  freeCIx = 0;

  map<int, int> clubmap;
  for (size_t k=0;k<cdb.size();k++)
    clubmap[cdb[k].getId()] = cdb[k].getId();

  for (size_t k=0;k<cdb.size();k++) {
    oDBClubEntry &ref = cdb[k];
    vector<int> compacted;
    for (size_t j=k+1;j<cdb.size(); j++) {
      if (_wcsicmp(ref.getName().c_str(),
              cdb[j].getName().c_str())==0)
        compacted.push_back(j);
    }

    if (!compacted.empty()) {
      int ba=ref.getDI().getDataAmountMeasure();
      oDBClubEntry *best=&ref;
      for (size_t j=0;j<compacted.size();j++) {
        int nba=cdb[compacted[j]].getDI().getDataAmountMeasure();
        if (nba>ba) {
          best = &cdb[compacted[j]];
          ba=nba;
        }
      }
      swap(ref, *best);

      //Update map
      for (size_t j=0;j<compacted.size();j++) {
        clubmap[cdb[compacted[j]].getId()] = ref.getId();
      }
    }
  }
}

RunnerWDBEntry *RunnerDB::getRunnerByCard(int card) const
{
  if (card == 0)
    return 0;

  int value;
  if (rhash.lookup(card, value))
    return (RunnerWDBEntry *)&rwdb[value];

  return 0;
}

RunnerWDBEntry *RunnerDB::getRunnerByIndex(size_t index) const {
  if (index >= rdb.size())
    throw meosException("Index out of bounds");

  return (RunnerWDBEntry *)&rwdb[index];
}


RunnerWDBEntry *RunnerDB::getRunnerById(__int64 extId) const
{
  if (extId == 0)
    return 0;

  setupIdHash();

  int value;

  if (idhash.lookup(extId, value))
    return (RunnerWDBEntry *)&rwdb[value];

  return 0;
}

RunnerWDBEntry *RunnerDB::getRunnerByName(const wstring &name, int clubId,
                                          int expectedBirthYear) const
{
  if (expectedBirthYear>0 && expectedBirthYear<100)
    expectedBirthYear = extendYear(expectedBirthYear);

  setupNameHash();
  vector<int> ix;
  wstring cname(canonizeName(name.c_str()));
  multimap<wstring, int>::const_iterator it = nhash.find(cname);

  while (it != nhash.end() && cname == it->first) {
    ix.push_back(it->second);
    ++it;
  }

  if (ix.empty())
    return 0;

  if (clubId == 0) {
    if (ix.size() == 1)
      return (RunnerWDBEntry *)&rwdb[ix[0]];
    else
      return 0; // Not uniquely defined.
  }

  // Filter on club
  vector<int> ix2;
  for (size_t k = 0;k<ix.size(); k++)
    if (rdb[ix[k]].clubNo == clubId || rdb[ix[k]].clubNo==0)
      ix2.push_back(ix[k]);

  if (ix2.empty())
    return 0;
  else if (ix2.size() == 1)
    return (RunnerWDBEntry *)&rwdb[ix2[0]];
  else if (expectedBirthYear > 0) {
    int bestMatch = 0;
    int bestYear = 0;
    for (size_t k = 0;k<ix2.size(); k++) {
      const RunnerWDBEntry &re = rwdb[ix2[k]];
      if (abs(re.dbe().birthYear-expectedBirthYear) < abs(bestYear-expectedBirthYear)) {
        bestMatch = ix2[k];
        bestYear = re.dbe().birthYear;
      }
    }
    if (bestYear>0)
      return (RunnerWDBEntry *)&rwdb[bestMatch];
  }

  return 0;
}

void RunnerDB::setupIdHash() const
{
  if (!idhash.empty())
    return;

  for (size_t k=0; k<rdb.size(); k++) {
    if (!rdb[k].isRemoved())
      idhash[rdb[k].extId] = int(k);
  }
}

void RunnerDB::setupNameHash() const
{
  if (!nhash.empty())
    return;

  for (size_t k=0; k<rdb.size(); k++) {
    if (!rdb[k].isRemoved()) {
      rwdb[k].initName();
      nhash.insert(pair<wstring, int>(canonizeName(rwdb[k].name), k));
    }
  }
}

void RunnerDB::setupCNHash() const
{
  if (!cnhash.empty())
    return;

  vector<wstring> split;
  for (size_t k=0; k<cdb.size(); k++) {
    if (cdb[k].isRemoved())
      continue;
    canonizeSplitName(cdb[k].getName(), split);
    for (size_t j = 0; j<split.size(); j++)
      cnhash.insert(pair<wstring, int>(split[j], k));
  }
}

static bool isVowel(int c) {
  return c=='a' || c=='e' || c=='i' ||
         c=='o' || c=='u' || c=='y' ||
         c=='å' || c=='ä' || c=='ö';
}

void RunnerDB::canonizeSplitName(const wstring &name, vector<wstring> &split)
{
  split.clear();
  const wchar_t *cname = name.c_str();
  int k = 0;
  for (k=0; cname[k]; k++)
    if (cname[k] != ' ')
      break;

  wchar_t out[128];
  int outp;
  while (cname[k]) {
    outp = 0;
    while(cname[k] != ' ' && cname[k] && outp<(sizeof(out)-1) ) {
      if (cname[k] == '-') {
        k++;
        break;
      }
      out[outp++] = toLowerStripped(cname[k]);
      k++;
    }
    out[outp] = 0;
    if (outp > 0) {
      for (int j=1; j<outp-1; j++) {
        if (out[j] == 'h') { // Idenity Thor and Tohr
          bool v1 = isVowel(out[j+1]);
          bool v2 = isVowel(out[j-1]);
          if (v1 && !v2)
            out[j] = out[j+1];
          else if (!v1 && v2)
            out[j] = out[j-1];
        }
      }

      if (outp>4 && out[outp-1]=='s')
        out[outp-1] = 0; // Identify Linköping och Linköpings
      split.push_back(out);
    }
    while(cname[k] == ' ')
      k++;
  }
}

bool RunnerDB::getClub(int clubId, wstring &club) const
{
  //map<int,int>::const_iterator it = chash.find(clubId);

  int value;
  if (chash.lookup(clubId, value)) {
  //if (it!=chash.end()) {
  //  int i=it->second;
    club=cdb[value].getName();
    return true;
  }
  return false;
}

oClub *RunnerDB::getClub(int clubId) const {
  int value;
  if (chash.lookup(clubId, value))
    return pClub(&cdb[value]);

  return 0;
}

oClub *RunnerDB::getClub(const wstring &name) const
{
  setupCNHash();
  vector<wstring> names;
  canonizeSplitName(name, names);
  vector< vector<int> > ix(names.size());
  set<int> iset;

  for (size_t k = 0; k<names.size(); k++) {
    multimap<wstring, int>::const_iterator it = cnhash.find(names[k]);

    while (it != cnhash.end() && names[k] == it->first) {
      ix[k].push_back(it->second);
      ++it;
    }

    if (ix[k].size() == 1 && names[k].length()>3)
      return pClub(&cdb[ix[k][0]]);

    if (iset.empty())
      iset.insert(ix[k].begin(), ix[k].end());
    else {
      set<int> im;
      for (size_t j = 0; j<ix[k].size(); j++) {
        if (iset.count(ix[k][j])==1)
          im.insert(ix[k][j]);
      }
      if (im.size() == 1) {
        int i = *im.begin();
        return pClub(&cdb[i]);
      }
      else if (!im.empty())
        swap(iset, im);
    }
  }

  // Exact compare
  for (set<int>::iterator it = iset.begin(); it != iset.end(); ++it) {
    pClub pc = pClub(&cdb[*it]);
    if (_wcsicmp(pc->getName().c_str(), name.c_str())==0)
      return pc;
  }

  wstring cname = canonizeName(name.c_str());
  // Looser compare
  for (set<int>::iterator it = iset.begin(); it != iset.end(); ++it) {
    pClub pc = pClub(&cdb[*it]);
    if (wcscmp(canonizeName(pc->getName().c_str()), cname.c_str()) == 0 )
      return pc;
  }

  double best = 1;
  double secondBest = 1;
  int bestIndex = -1;
  for (set<int>::iterator it = iset.begin(); it != iset.end(); ++it) {
    pClub pc = pClub(&cdb[*it]);

    double d = stringDistance(cname.c_str(), canonizeName(pc->getName().c_str()));

    if (d<best) {
      bestIndex = *it;
      secondBest = best;
      best = d;
    }
    else if (d<secondBest) {
      secondBest = d;
      if (d<=0.4)
        return 0; // Two possible clubs are too close. We cannot choose.
    }
  }

  if (best < 0.2 && secondBest>0.4)
    return pClub(&cdb[bestIndex]);

  return 0;
}

void RunnerDB::saveClubs(const wstring &file)
{
  xmlparser xml;

  xml.openOutputT(file.c_str(), true, "meosclubs");

  vector<oDBClubEntry>::iterator it;

  xml.startTag("ClubList");

  for (it=cdb.begin(); it != cdb.end(); ++it)
    it->write(xml);

  xml.endTag();

  xml.closeOut();
}

string RunnerDB::getDataDate() const
{
  char bf[128];
  if (dataTime<=0 && dataDate>0)
    sprintf_s(bf, "%04d-%02d-%02d", dataDate/10000,
                                    (dataDate/100)%100,
                                    dataDate%100);
  else if (dataDate>0)
    sprintf_s(bf, "%04d-%02d-%02d %02d:%02d:%02d", dataDate/10000,
                                                 (dataDate/100)%100,
                                                 dataDate%100,
                                                 (dataTime/3600)%24,
                                                 (dataTime/60)%60,
                                                 (dataTime)%60);
  else
    return "2011-01-01 00:00:00";

  return bf;
}

void RunnerDB::setDataDate(const string &date)
{
   int d = convertDateYMS(date.substr(0, 10), false);
   int t = date.length()>11 ? convertAbsoluteTimeHMS(date.substr(11), -1) : 0;

   if (d<=0)
     throw std::exception("Felaktigt datumformat");

   dataDate = d;
   if (t>0)
     dataTime = t;
   else
     dataTime = 0;
}

void RunnerDB::saveRunners(const wstring &file)
{
  int f=-1;
  _wsopen_s(&f, file.c_str(), _O_BINARY|_O_CREAT|_O_TRUNC|_O_WRONLY,
            _SH_DENYWR, _S_IREAD|_S_IWRITE);

  if (f!=-1) {
    int version = 5460004;
    _write(f, &version, 4);
    _write(f, &dataDate, 4);
    _write(f, &dataTime, 4);
    if (!rdb.empty())
      _write(f, &rdb[0], rdb.size()*sizeof(RunnerDBEntry));
    _close(f);
  }
  else throw std::exception("Could not save runner database.");
}

void RunnerDB::loadClubs(const wstring &file)
{
  xmlparser xml;

  xml.read(file);

  xmlobject xo;

  //Get clubs
  xo=xml.getObject("ClubList");
  if (xo) {
    clearClubs();
    loadedFromServer = false;

    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    cdb.clear();
    chash.clear();
    freeCIx = 0;
    cdb.reserve(xl.size());
    for (it=xl.begin(); it != xl.end(); ++it) {
      if (it->is("Club")){
        oDBClubEntry c(oe, cdb.size(), this);
        c.set(*it);
        int value;
        if (c.getId() == 0)
          continue;
        //if (chash.find(c.getId()) == chash.end()) {
        if (!chash.lookup(c.getId(), value)) {
          chash[c.getId()]=cdb.size();
          cdb.push_back(c);
          freeCIx = max(c.getId()+1, freeCIx);;
        }
      }
    }
  }

  bool checkClubs = false;

  if (checkClubs) {
    vector<wstring> problems;

    for (size_t k=0; k<cdb.size(); k++) {
      pClub pc = &cdb[k];
      pClub pc2 = getClub(pc->getName());
      if (!pc2)
        problems.push_back(pc->getName());
      else if (pc != pc2)
        problems.push_back(pc->getName() + L"-" + pc2->getName());
    }
    problems.begin();
  }
}

void RunnerDB::loadRunners(const wstring &file)
{
  wstring ex = L"Bad runner database. " + file;
  int f=-1;
  _wsopen_s(&f, file.c_str(), _O_BINARY|_O_RDONLY,
            _SH_DENYWR, _S_IREAD|_S_IWRITE);

  if (f!=-1) {
    clearRunners();
    loadedFromServer = false;

    int len = _filelength(f);
    if ( (len%sizeof(RunnerDBEntryV1) != 0) && (len % sizeof(RunnerDBEntryV3) != 12) && (len % sizeof(RunnerDBEntry) != 12)) {
      _close(f);
      return;//Failed
    }
    int nentry = 0;

    int version;
    version = 0;
    dataDate = 0;
    dataTime = 0;

    if (len % sizeof(RunnerDBEntry) == 12 || len % sizeof(RunnerDBEntryV2) == 12 || len % sizeof(RunnerDBEntryV3) == 12) {
      _read(f, &version, 4);
      _read(f, &dataDate, 4);
      _read(f, &dataTime, 4);
    }

    if (version == 5460002 || version == 5460003 || version == 5460004) {

      bool migrateV2 = false;
      bool migrateV3 = false;

      if (version == 5460002) {
        migrateV2 = true;
        nentry = (len - 12) / sizeof(RunnerDBEntryV2);
      }
      else if (version == 5460003) {
        nentry = (len - 12) / sizeof(RunnerDBEntryV3);
        migrateV3 = true;
      }
      else if (version == 5460004) {
        nentry = (len - 12) / sizeof(RunnerDBEntry);
      }

      rdb.resize(nentry);
      if (rdb.empty()) {
        _close(f);
        return;
      }
      rwdb.resize(rdb.size());

      if (!migrateV2 && !migrateV3) {
        _read(f, &rdb[0], len - 12);
        _close(f);
      }
      else if (migrateV2) {
        vector<RunnerDBEntryV2> rdbV2(nentry);
        _read(f, &rdbV2[0], len - 12);
        _close(f);

        for (int k = 0; k < nentry; k++) {
          rdb[k].init(rdbV2[k]);
          rwdb[k].init(this, k);
          if (!check(rdb[k]))
            throw meosException(ex);
        }
      }
      else if (migrateV3) {
        vector<RunnerDBEntryV3> rdbV3(nentry);
        _read(f, &rdbV3[0], len - 12);
        _close(f);

        for (int k = 0; k < nentry; k++) {
          rdb[k].init(rdbV3[k]);
          rwdb[k].init(this, k);
          if (!check(rdb[k]))
            throw meosException(ex);
        }
      }

      for (int k = 0; k < nentry; k++) {
        rwdb[k].init(this, k);
        if (!check(rdb[k]))
          throw meosException(ex);
      }
    }
    else { //V1
      dataDate = 0;
      dataTime = 0;

      nentry = len / sizeof(RunnerDBEntryV1);

      rdb.resize(nentry);
      if (rdb.empty()) {
         _close(f);
        return;
      }
      vector<RunnerDBEntryV1> rdbV1(nentry);
      _lseek(f, 0, SEEK_SET);
      _read(f, &rdbV1[0], len);
      _close(f);
      rwdb.resize(rdb.size());
      RunnerDBEntryV2 tmp;
      for (int k=0;k<nentry;k++) {
        tmp.init(rdbV1[k]);
        rdb[k].init(tmp);
        rwdb[k].init(this, k);
        if (!check(rdb[k]))
          throw meosException(ex);
      }
    }

    int ncard = 0;
    for (int k=0;k<nentry;k++)
      if (rdb[k].cardNo>0)
        ncard++;

    rhash.resize(ncard);

    for (int k=0;k<nentry;k++) {
      if (rdb[k].cardNo>0 && !rdb[k].isRemoved()) {
        rhash[rdb[k].cardNo]=k;
      }
    }
  }
  else throw meosException(ex);
}

bool RunnerDB::check(const RunnerDBEntry &rde) const
{
  if (rde.cardNo<0 || rde.cardNo>99999999
           || rde.name[baseNameLengthUTF-1]!=0 || rde.clubNo<0)
    return false;
  return true;
}

void RunnerDB::updateAdd(const oRunner &r, map<int, int> &clubIdMap)
{
  if (r.getExtIdentifier() > 0) {
    RunnerWDBEntry *dbe = getRunnerById(int(r.getExtIdentifier()));
    if (dbe) {
      dbe->dbe().cardNo = r.CardNo;
      return; // Do not change too much in runner from national database
    }
  }

  const pClub pc = r.Club;
  int localClubId = r.getClubId();

  if (pc) {
    if (clubIdMap.count(localClubId))
      localClubId = clubIdMap[localClubId];

    pClub dbClub = getClub(localClubId);
    bool wrongId = false;
    if (dbClub) {
      if (dbClub->getName() != pc->getName()) {
        dbClub = 0; // Wrong club!
        wrongId = true;
      }
    }

    if (dbClub == 0) {
      dbClub = getClub(r.getClub());
      if (dbClub) {
        localClubId = dbClub->getId();
        clubIdMap[pc->getId()] = localClubId;
      }
    }

    if (dbClub == 0) {
      localClubId = addClub(*pc, wrongId);
      if (wrongId)
        clubIdMap[pc->getId()] = localClubId;
    }
  }

  RunnerWDBEntry *dbe = getRunnerByCard(r.getCardNo());

  if (dbe == nullptr) {
    // Lookup by name
    setupNameHash();
    vector<int> ix;
    wstring cname(canonizeName(r.getName().c_str()));
    auto it = nhash.find(cname);

    while (it != nhash.end() && cname == it->first) {
      auto &dbr = rwdb[it->second];
      if (dbr.dbe().clubNo == localClubId) {
        dbe = &dbr;
        break;
      }
      ++it;
    }
  }

  if (dbe == nullptr) {
    dbe = addRunner(r.getName().c_str(), 0, localClubId, r.getCardNo());
    if (dbe)
      dbe->dbe().birthYear = r.getDCI().getInt("BirthYear");
  }
  else {
    if (dbe->getExtId() == 0) { // Only update entries not in national db.
      dbe->setName(r.getName().c_str());
      dbe->dbe().clubNo = localClubId;
      dbe->dbe().birthYear = r.getDCI().getInt("BirthYear");
    }
  }
}

void RunnerDB::getAllNames(vector<wstring> &givenName, vector<wstring> &familyName)
{
  givenName.reserve(rdb.size());
  familyName.reserve(rdb.size());
  for (size_t k=0;k<rwdb.size(); k++) {
    wstring gname(rwdb[k].getGivenName());
    wstring fname(rwdb[k].getFamilyName());
    if (!gname.empty())
      givenName.push_back(gname);
    if (!fname.empty())
      familyName.push_back(fname);
  }
}


void RunnerDB::clearClubs()
{
  clearRunners(); // Runners refer to clubs. Clear runners
  cnhash.clear();
  chash.clear();
  clubHash.clear(); // Autocomplete
  freeCIx = 0;
  cdb.clear();
  if (clubTable)
    clubTable->clear();

}

void RunnerDB::clearRunners()
{
  nhash.clear();
  idhash.clear();
  rhash.clear();
  runnerHash.clear(); // Autocomplete
  runnerHashByClub.clear(); // Autocomplete
  rdb.clear();
  rwdb.clear();
  if (runnerTable)
    runnerTable->clear();
}

const vector<oDBClubEntry> &RunnerDB::getClubDB(bool checkProblems) const {
  if (checkProblems) {
    for (size_t k = 0; k < cdb.size(); k++) {
      int v = -1;
      if (cdb[k].isRemoved())
        continue;

      // Mark id duplacates as removed
      if (!chash.lookup(cdb[k].getId(), v) || v != k) {
        const_cast<oDBClubEntry &>(cdb[k]).Removed = true;
      }
    }
  }
  return cdb;
}

const vector<RunnerWDBEntry> &RunnerDB::getRunnerDB() const {
  return rwdb;
}

const vector<RunnerDBEntry> &RunnerDB::getRunnerDBN() const {
  return rdb;
}

void RunnerDB::prepareLoadFromServer(int nrunner, int nclub) {
  loadedFromServer = true;
  clearClubs(); // Implicitly clears runners
  cdb.reserve(nclub);
  rdb.reserve(nrunner);
}

void RunnerDB::fillClubs(vector< pair<wstring, size_t> > &out) const {
  out.reserve(cdb.size());
  for (size_t k = 0; k<cdb.size(); k++) {
    if (!cdb[k].isRemoved()) {
      out.push_back(make_pair(cdb[k].getName(), cdb[k].getId()));
    }
  }
  sort(out.begin(), out.end());
}

oDBRunnerEntry::oDBRunnerEntry(oEvent *oe) : oBase(oe) {
  db = 0;
  index = -1;
}

oDBRunnerEntry::~oDBRunnerEntry() {}

void RunnerDB::generateRunnerTableData(Table &table, oDBRunnerEntry *addEntry)
{
  oe->getDBRunnersInEvent(runnerInEvent);
  if (addEntry) {
    addEntry->addTableRow(table);
    return;
  }

  table.reserve(rdb.size());
  oRDB.resize(rdb.size(), oDBRunnerEntry(oe));
  for (size_t k = 0; k<rdb.size(); k++){
    if (!rdb[k].isRemoved()) {
      oRDB[k].init(this, k);
      oRDB[k].addTableRow(table);
    }
  }
}

void RunnerDB::hasEnteredCompetition(__int64 extId) {
  if (runnerTable != 0 && extId!=0) {
    setupIdHash();
    int value;
    if (idhash.lookup(extId, value)) {
      try {
        runnerTable->reloadRow(value + 1);
      }
      catch (const std::exception &) {
        // Ignore any problems with the table.
      }
    }
  }
}

void RunnerDB::refreshTables() {
  if (runnerTable)
    refreshRunnerTableData(*runnerTable);

  if (clubTable)
    refreshClubTableData(*clubTable);
}

void RunnerDB::releaseTables() {
  if (runnerTable)
    runnerTable->releaseOwnership();
  runnerTable = 0;

  if (clubTable)
    clubTable->releaseOwnership();
  clubTable = 0;
}

Table *RunnerDB::getRunnerTB()//Table mode
{
  if (runnerTable == 0) {
    Table *table=new Table(oe, 20, L"Löpardatabasen", "runnerdb");

    table->addColumn("Index", 70, true, true);
    table->addColumn("Id", 70, true, true);
    table->addColumn("Namn", 200, false);
    table->addColumn("Klubb", 200, false);
    table->addColumn("SI", 70, true, true);
    table->addColumn("Nationalitet", 70, false, true);
    table->addColumn("Kön", 50, false, true);
    table->addColumn("Födelseår", 70, true, true);
    table->addColumn("Anmäl", 70, false, true);

    table->setTableProp(Table::CAN_INSERT|Table::CAN_DELETE|Table::CAN_PASTE);
    table->setClearOnHide(false);
    table->addOwnership();
    runnerTable = table;
  }
  int nr = 0;
  for (size_t k = 0; k < rdb.size(); k++) {
    if (!rdb[k].isRemoved())
      nr++;
  }

  if (runnerTable->getNumDataRows() != nr)
    runnerTable->update();
  return runnerTable;
}

void RunnerDB::generateClubTableData(Table &table, oClub *addEntry)
{
  if (addEntry) {
    addEntry->addTableRow(table);
    return;
  }

  table.reserve(cdb.size());
  for (size_t k = 0; k<cdb.size(); k++){
    if (!cdb[k].isRemoved())
      cdb[k].addTableRow(table);
  }
}

void RunnerDB::refreshClubTableData(Table &table) {
  for (size_t k = 0; k<cdb.size(); k++){
    if (!cdb[k].isRemoved()) {
      TableRow *row = table.getRowById(cdb[k].getTableId());
      if (row)
        row->setObject(cdb[k]);
    }
  }
}

void RunnerDB::refreshRunnerTableData(Table &table) {
  oe->getDBRunnersInEvent(runnerInEvent); //XXX
  for (size_t k = 0; k<oRDB.size(); k++){
    if (!oRDB[k].isRemoved()) {
      TableRow *row = table.getRowById(oRDB[k].getIndex() + 1);
      if (row) {
        row->setObject(oRDB[k]);
        
        oClass *val = 0;
        bool found = false;

        if (rdb[k].extId != 0)
          found = runnerInEvent.lookup(rdb[k].extId, val);


        if (found && row->getCellType(cellEntryIndex) == cellAction) {
          row->updateCell(cellEntryIndex, cellEdit, val->getName());
        }
        else if (!found && row->getCellType(cellEntryIndex) == cellEdit) {
          row->updateCell(cellEntryIndex, cellAction, L"@+");
        }
      }
    }
  }
}

Table *RunnerDB::getClubTB()//Table mode
{
  bool canEdit = !oe->isClient();

  if (clubTable == 0) {
    Table *table = new Table(oe, 20, L"Klubbdatabasen", "clubdb");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);
    oClub::buildTableCol(oe, table);

    if (canEdit)
      table->setTableProp(Table::CAN_DELETE|Table::CAN_INSERT|Table::CAN_PASTE);
    else
      table->setTableProp(0);

    table->setClearOnHide(false);
    table->addOwnership();
    clubTable = table;
  }

  int nr = 0;
  for (size_t k = 0; k < cdb.size(); k++) {
    if (!cdb[k].isRemoved())
      nr++;
  }

  if (clubTable->getNumDataRows() != nr)
    clubTable->update();
  return clubTable;
}


void oDBRunnerEntry::addTableRow(Table &table) const {
  bool canEdit = !oe->isClient();

  oDBRunnerEntry &it = *(oDBRunnerEntry *)(this);
  table.addRow(index+1, &it);
  if (!db)
    throw meosException("Not initialized");

  RunnerWDBEntry &r = db->rwdb[index];
  RunnerDBEntry &rn = r.dbe();
  
  int row = 0;
  table.set(row++, it, TID_INDEX, itow(index+1), false, cellEdit);

  wchar_t bf[16];
  oBase::converExtIdentifierString(rn.extId, bf);
  table.set(row++, it, TID_ID, bf, false, cellEdit);
  r.initName();
  table.set(row++, it, TID_NAME, r.name, canEdit, cellEdit);

  const pClub pc = db->getClub(rn.clubNo);
  if (pc)
    table.set(row++, it, TID_CLUB, pc->getName(), canEdit, cellSelection);
  else
    table.set(row++, it, TID_CLUB, L"", canEdit, cellSelection);

  table.set(row++, it, TID_CARD, rn.cardNo > 0 ? itow(rn.cardNo) : L"", canEdit, cellEdit);
  wchar_t nat[4] = {wchar_t(rn.national[0]), wchar_t(rn.national[1]), wchar_t(rn.national[2]), 0};

  table.set(row++, it, TID_NATIONAL, nat, canEdit, cellEdit);
  wchar_t sex[2] = {wchar_t(rn.sex), 0};
  table.set(row++, it, TID_SEX, sex, canEdit, cellEdit);
  table.set(row++, it, TID_YEAR, itow(rn.birthYear), canEdit, cellEdit);

  oClass *val = 0;
  bool found = false;

  if (rn.extId != 0)
    found = db->runnerInEvent.lookup(rn.extId, val);

  if (canEdit)
    table.setTableProp(Table::CAN_DELETE|Table::CAN_INSERT|Table::CAN_PASTE);
  else
    table.setTableProp(0);

  RunnerDB::cellEntryIndex = row;
  if (!found)
    table.set(row++, it, TID_ENTER, L"@+", false, cellAction);
  else
    table.set(row++, it, TID_ENTER, val ? val->getName() : L"", false, cellEdit);
}

const RunnerDBEntry &oDBRunnerEntry::getRunner() const {
 if (!db)
    throw meosException("Not initialized");
  return db->rdb[index];
}

bool oDBRunnerEntry::inputData(int id, const wstring &input,
                           int inputId, wstring &output, bool noUpdate)
{
  if (!db)
    throw meosException("Not initialized");
  RunnerWDBEntry &r = db->rwdb[index];
  RunnerDBEntry &rd = db->rdb[index];

  switch(id) {
    case TID_NAME:
      r.setName(input.c_str());
      r.getName(output);
      db->nhash.clear();
      db->runnerHash.clear();
      db->runnerHashByClub.clear();
      return true;
    case TID_CARD:
      db->rhash.remove(rd.cardNo);
      rd.cardNo = _wtoi(input.c_str());
      db->rhash.insert(rd.cardNo, index);
      if (rd.cardNo)
        output = itow(rd.cardNo);
      else
        output = L"";
      return true;
    case TID_NATIONAL:
      if (input.empty()) {
        rd.national[0] = 0;
        rd.national[1] = 0;
        rd.national[2] = 0;
      }
      else if (input.size() >= 2) {
        for (size_t i = 0; i < 3; i++) {
          rd.national[i] = i < input.size() ? input[i] : 0; 
        }
      }
      output = r.getNationality();
      break;
    case TID_SEX:
      rd.sex = char(input[0]);
      output = r.getSex();
      break;
    case TID_YEAR:
      rd.birthYear = short(_wtoi(input.c_str()));
      output = itow(r.getBirthYear());
      break;

    case TID_CLUB:
      rd.clubNo = inputId;
      output = input;
      break;
  }
  return false;
}

void oDBRunnerEntry::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  RunnerDBEntry &r = db->rdb[index];
  if (id==TID_CLUB) {
    db->fillClubs(out);
    out.push_back(make_pair(L"-", 0));
    selected = r.clubNo;
  }
}

void oDBRunnerEntry::remove() {
  RunnerWDBEntry &r = db->rwdb[index];
  r.remove();
  db->idhash.remove(r.dbe().extId);
  wstring cname(canonizeName(r.name));
  multimap<wstring, int>::const_iterator it = db->nhash.find(cname);

  while (it != db->nhash.end() && cname == it->first) {
    if (it->second == index) {
      db->nhash.erase(it);
      break;
    }
    ++it;
  }

  if (r.dbe().cardNo > 0) {
    int ix = -1;
    if (db->rhash.lookup(r.dbe().cardNo, ix) && ix == index) {
      db->rhash.remove(r.dbe().cardNo);
    }
  }
}

bool oDBRunnerEntry::canRemove() const {
  return true;
}

oDBRunnerEntry *RunnerDB::addRunner() {
  rdb.push_back(RunnerDBEntry());
  rwdb.push_back(RunnerWDBEntry());
  rwdb.back().init(this, rdb.size() -1);

  oRDB.push_back(oDBRunnerEntry(oe));
  oRDB.back().init(this, rdb.size() - 1);

  return &oRDB.back();
}

oClub *RunnerDB::addClub() {
  freeCIx = max<int>(freeCIx + 1, cdb.size());
  while (chash.count(freeCIx))
    freeCIx++;

  cdb.push_back(oDBClubEntry(oe, freeCIx, cdb.size(), this));
  chash.insert(freeCIx, cdb.size()-1);
  cnhash.clear();

  return &cdb.back();
}

oDataContainer &oDBRunnerEntry::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  throw meosException("Not implemented");
}

oDBClubEntry::oDBClubEntry(oEvent *oe, int id, int ix, RunnerDB *dbin) : oClub(oe, id) {
  index = ix;
  db = dbin;
}

oDBClubEntry::oDBClubEntry(const oClub &c, int ix, RunnerDB *dbin) : oClub(c) {
  index = ix;
  db = dbin;
}

oDBClubEntry::~oDBClubEntry() {
}

void oDBClubEntry::remove() {
  Removed = true;
  db->chash.remove(getId());

  vector<wstring> split;
  db->canonizeSplitName(getName(), split);
  for (size_t j = 0; j<split.size(); j++) {
    multimap<wstring, int>::const_iterator it = db->cnhash.find(split[j]);
    while (it != db->cnhash.end() && split[j] == it->first) {
      if (it->second == index) {
        db->cnhash.erase(it);
        break;
      }
      ++it;
    }
  }
}

bool oDBClubEntry::canRemove() const {
  return true;
}

int oDBClubEntry::getTableId() const {
  return index + 1;
}
  
// Link to narrow DB Entry
const RunnerDBEntry &RunnerWDBEntry::dbe() const {
  return owner->rdb[ix];
}

// Link to narrow DB Entry
RunnerDBEntry &RunnerWDBEntry::dbe() {
  return owner->rdb[ix];
}


void RunnerDB::ClubNodeHash::match(RunnerDB &db, set< pair<int, int> > &ix, const vector<wstring> &key, const wstring &skey) const {
  for (size_t k = 0; k < index.size(); k++) {
    int x = index[k];
    if (db.cdb[x].isRemoved())
      continue;
    const wstring &n = db.cdb[x].getCanonizedName();
    int nMatch = 0;
    for (size_t k = 0; k < key.size(); k++) {
      const wchar_t *str = wcsstr(n.c_str(), key[k].c_str());
      if (str != nullptr)
        nMatch++;
    }
    if (wcsstr(db.cdb[x].getCanonizedNameExact().c_str(), skey.c_str()) != nullptr)
      nMatch += 3;

    if (nMatch > 0)
      ix.insert(make_pair(nMatch, x));
  }
}

void RunnerDB::ClubNodeHash::setupHash(const wstring &key, int keyOffset, int ix) {
  index.push_back(ix);
}

void RunnerDB::RunnerClubNodeHash::setupHash(const wchar_t *key, int keyOffset, int ix) {
  index.push_back(ix);
}
  
void RunnerDB::RunnerClubNodeHash::match(RunnerDB &db, set< pair<int, int> > &ix, const vector<wstring> &key) const {
  wchar_t bf[256];
  for (size_t k = 0; k < index.size(); k++) {
    int x = index[k];
    if (db.rdb[x].isRemoved())
      continue;
    const wchar_t *n = db.rwdb[x].getNameCstr();
    int i = 0, di = 0;
    while (n[i]) {
      if (n[i] == ',') {
        i++;
        continue;
      }
      bf[di++] = toLowerStripped(n[i++]);
    }
    bf[di] = 0;
    int nMatch = 0;
    for (size_t k = 0; k < key.size(); k++) {
      const wchar_t *ref = key[k].c_str();
      const wchar_t *str = wcsstr(bf, ref);
      int add = 0;
      if (str == bf || (str != nullptr && (iswspace(str[-1]) || str[-1]=='-'))) {
        //Beginning of string or beginning of word
        int len = 0;
        while (str[len] && !iswspace(str[len]))
          len++;

        if (wcsncmp(ref, str, max<int>(len, key[k].length())) == 0)
          add = 3; // Points for full  name
        else
          add = 2; // Points for matching beginning of name

        /*if (k > 0 && k + 1 == key.size()) {
          add *= 2; // Last is full string
        }*/
      }
      else if (str != nullptr && key[k].length() > 3) { // Inner part of name
        add = 1;
      }
      if (nMatch > 1 && add > 1)
        nMatch = 10 * nMatch + add;
      else
        nMatch += add;
    }
    
    if (nMatch > 0)
      ix.insert(make_pair(nMatch, x));
  }
}

void RunnerDB::RunnerNodeHash::setupHash(const wchar_t *key, int keyOffset, int ix) {
  index.push_back(ix);
}

void RunnerDB::RunnerNodeHash::match(RunnerDB &db, set< pair<int, int> > &ix, const vector<wstring> &key) const {
  RunnerDB::RunnerClubNodeHash::match(db, ix, key);
}


void RunnerDB::setupAutoCompleteHash(AutoHashMode mode) {
  vector<wstring> names;

  if (mode == AutoHashMode::Clubs) {
    if (!clubHash.empty())
      return;

    for (size_t k = 0; k < cdb.size(); k++) {
      auto &c = cdb[k];
      canonizeSplitName(c.getName(), names);
      wstring ccn, ccne;
      ccne = canonizeName(c.getName().c_str());
      for (size_t j = 0; j < names.size(); j++) {
        const wstring &n = names[j];
        if (j > 0)
          ccn.append(L" ");
        ccn += n;
        int ikey = keyFromString(n, 0);
        clubHash[ikey].setupHash(n, 2, k);
      }
      c.setCanonizedName(std::move(ccn), std::move(ccne));
    }
  }
  else if (mode == AutoHashMode::RunnerClub) {
    if (!runnerHashByClub.empty())
      return;
    for (size_t k = 0; k < rwdb.size(); k++) {
      int key = rwdb[k].dbe().clubNo;
      if (key > 0) {
        runnerHashByClub[key].setupHash(rwdb[k].getNameCstr(), 2, k);
      }
    }
  }
  else if (mode == AutoHashMode::Runners) {
    if (!runnerHash.empty())
      return;
    wstring tn;
    wchar_t bf[256];
    vector<int> ps;
    for (size_t k = 0; k < rwdb.size(); k++) {
      auto &r = rwdb[k];
      const wchar_t *name = r.getNameCstr();
      int ix = 0, iy = 0;
      ps.resize(1, 0);
      while (name[ix]) {
        if (name[ix] != ',') {
          if (name[ix] == '-' || name[ix] == ' ') {
            bf[iy] = 0;
            ps.push_back(iy+1);
          }
          else 
            bf[iy] = toLowerStripped(name[ix]);
          iy++;
        }
        ix++;
      }
      bf[iy] = 0;
      for (int j : ps) {
        if (j < iy) {
          int ikey = keyFromString(bf + j);
          runnerHash[ikey].setupHash(bf, 2, k);
        }
      }
    }
  }
}

vector<pClub> RunnerDB::getClubSuggestions(const wstring &key, int limit) {
  setupAutoCompleteHash(AutoHashMode::Clubs);
  set<pair<int, int>> ix;
  vector<wstring> nn;
  wstring cankey = canonizeName(key.c_str());
  canonizeSplitName(key, nn);
  for (wstring &part : nn) {
    int ikey = keyFromString(part, 0);
    auto res = clubHash.find(ikey);
    if (res != clubHash.end()) {
      res->second.match(*this, ix, nn, cankey);
    }
  }
  vector<pClub> ret;
  vector< pair<int, int> > outOrder;
  outOrder.reserve(ix.size());
  for (const pair<int, int> &x : ix) {
    const wstring &name = cdb[x.second].getCanonizedNameExact();
  
    double sd = stringDistanceAssymetric(name, cankey);
    outOrder.emplace_back(-(int(10000.0*(10 * x.first - sd))), x.second);
  }

  sort(outOrder.begin(), outOrder.end());
  
  for (const pair<int, int> &x : outOrder) {
    ret.push_back(&cdb[x.second]);
    if (ret.size() > size_t(limit))
      break;
  }
  return ret;
}

vector<pair<RunnerWDBEntry *, int>> RunnerDB::getRunnerSuggestions(const wstring &key, int clubId, int limit) {
  if (clubId > 0)
    setupAutoCompleteHash(AutoHashMode::RunnerClub);
  else
    setupAutoCompleteHash(AutoHashMode::Runners);

  vector< pair<int, int> > outOrder;
  set<pair<int, int>> ix;
  wchar_t bf[256];
  int iy = 0;
  for (size_t k = 0; k < key.length(); k++) {
    if (key[k] != ',') {

      if (key[k] == '-')
        bf[k] = ' ';
      else
        bf[k] = toLowerStripped(key[k]);

      iy++;
      if (iy >= 255) {
        break;
      }
    }
  }
  bf[iy] = 0;
  wstring cankey = trim(bf);
  vector<pair<RunnerWDBEntry *, int>> ret;
  vector<wstring> nameParts;
  split(cankey, L" ", nameParts);
//  if (nameParts.size() > 1)
//    nameParts.push_back(cankey);

  if (clubId > 0) {
    auto res = runnerHashByClub.find(clubId);
    if (res != runnerHashByClub.end()) {
      res->second.match(*this, ix, nameParts);
    }
  }
  else {
    int np = nameParts.size();
    if (np > 1)
      np--;// Last is full string.
    for (int k = 0; k < np; k++) {
      wstring &part = nameParts[k];
      int ikey = keyFromString(part, 0);
      auto res = runnerHash.find(ikey);
      if (res != runnerHash.end())
        res->second.match(*this, ix, nameParts);
    }
    
  }
  if (ix.empty())
    return ret;

  outOrder.reserve(ix.size());
  wstring tname;
  int maxP = 0;
  for (auto itr = ix.rbegin(); itr != ix.rend(); ++itr) {
    auto &x = *itr;
    maxP = max(maxP, x.first);
    if (x.first <= (maxP - 10) || (outOrder.size() > size_t(limit) &&  x.first < maxP))
      break;
    const wchar_t *name = rwdb[x.second].getNameCstr();
    const wchar_t *cname = canonizeName(name);
    tname = cname;
    double sd = stringDistanceAssymetric(tname, cankey);
    outOrder.emplace_back(-(int(10000.0*(10 * x.first - sd))), x.second);
  }

  // Fine-sort on string distance
  sort(outOrder.begin(), outOrder.end());

  for (const pair<int, int> &x : outOrder) {
    ret.emplace_back(&rwdb[x.second], x.second);
    if (ret.size() > size_t(limit))
      break;
  }
  return ret;
}
