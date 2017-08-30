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

#include "stdafx.h"
#include "oFreeImport.h"
#include "oEvent.h"
#include <algorithm>
#include "gdioutput.h"
#include "gdifonts.h"
#include "localizer.h"
#include "meosexception.h"

#include "meos_util.h"

#include "io.h"
#include "fcntl.h"
#include "sys/stat.h"
#include <cassert>
#include <algorithm>

void oWordDB::insert(const wchar_t *s)
{
  str.insert(s);
}

bool oWordDB::lookup(const wchar_t *s) const
{
  return str.count(s)==1;
}

const char *oWordDB::deserialize(const char *bf, const char *end)
{
  BYTE s=bf[0];
  bf++;
  str.clear();
  for (int k=0;k<s;k++) {
    wstring ns((wchar_t*)bf);
    //wstring n(ns.begin(), ns.end());
    bf+=(ns.size()+1)*sizeof(wchar_t);

    if (bf>end)
      throw std::exception("Internal error deserializing wordlist.");

    str.insert(ns);
  }
  return bf;
}

char *oWordDB::serialize(char *bf) const
{
  //Randomize order for better search performace on reload.
  BYTE s=BYTE(str.size());
  vector<wstring> rnd(s);
  int k=0;
  for (set<wstring>::const_iterator it=str.begin(); it!=str.end(); ++it)
    rnd[k++]=*it;

  bf[0]=s;
  bf++;
  for (k=0;k<s;k++) {
    int i=rand() % rnd.size();
    int byteSize = (rnd[i].size()+1) * sizeof(wchar_t);
    memcpy(bf, rnd[i].c_str(), byteSize);
    bf+=byteSize;
    swap(rnd[i], rnd.back());
    rnd.pop_back();
  }
  return bf;
}

int oWordDB::serialSize() const
{
  int s=1;
  for(set<wstring>::const_iterator it=str.begin(); it!=str.end(); ++it)
    s+=(it->size()+1)*sizeof(wchar_t);

  return s;
}

pWordDatabase oWordDB::split()
{
  if (str.size()>=hashSplitSize) {
    oWordIndexHash *db=new oWordIndexHash(false);

    set<wstring>::iterator it;

    for(it=str.begin();it!=str.end();++it)
      db->insert(it->c_str());

    delete this;
    return db;
  }
  else return this;
}

size_t oWordDB::size()
{
  return str.size();
}

void oWordIndexHash::clear()
{
  MapTable::iterator it;

  for(it=unMapped.begin(); it!=unMapped.end(); ++it)
    if (it->second)
      delete it->second;

  unMapped.clear();

  for(int k=0;k<hashTableSize;k++)
    if (hashTable[k]) {
      delete hashTable[k];
      hashTable[k]=0;
    }
}

const char *oWordIndexHash::deserialize(const char *bf, const char *end)
{
  clear();
  unsigned short s=*((short *)&bf[0]);
  bf+=2;

  for (int k=0;k<s;k++) {
    unsigned short a=*((short *)&bf[0]);
    unsigned short b=*((short *)&bf[2]);
    pWordDatabase db=0;
    bf+=4;
    if (a) {
      if (b==1)
        db = new oWordDB;
      else if (b==2)
        db = new oWordIndexHash(false);
      else
        throw std::exception("Internal error deserilizing wordlist.");

      bf = db->deserialize(bf, end);
    }

    DWORD i=a-indexMapStart;
    if (i<=(indexMapEnd-indexMapStart)) {
      if (hashTable[i])
        throw std::exception("Internal error deserilizing wordlist.");
      else hashTable[i]=db;
    }
    else
      unMapped[a]=db;
  }
  return bf;
}

char *oWordIndexHash::serialize(char *bf) const
{
  unsigned short s = WORD(unMapped.size());
  for (int k=0;k<hashTableSize;k++)
    if (hashTable[k])
      s++;

  *((short *)&bf[0])=s;
  bf+=2;

  for(MapTable::const_iterator it=unMapped.begin(); it!=unMapped.end(); ++it) {
    *((short *)&bf[0]) = it->first;
    if (it->first) {
      *((short *)&bf[2]) = it->second->getType();
      bf = it->second->serialize(bf+4);
    }
    else { //Empty string.
      *((short *)&bf[2]) = 0;
      bf+=4;
    }
  }

  for (int k=0;k<hashTableSize;k++) {
    if (hashTable[k]) {
      *((short *)&bf[0]) = k + indexMapStart ;
      *((short *)&bf[2]) = hashTable[k]->getType();
      bf = hashTable[k]->serialize(bf+4);
    }
  }
  return bf;
}

int oWordIndexHash::serialSize() const
{
  int s=2;
  for (int k=0;k<hashTableSize;k++) {
    if (hashTable[k])
      s+=hashTable[k]->serialSize()+4;
  }
  for(MapTable::const_iterator it=unMapped.begin(); it!=unMapped.end(); ++it)
    s+=it->second ? it->second->serialSize()+4 : 4;

  return s;
}

void oWordIndexHash::insert(const wchar_t *s)
{
  DWORD i=s[0]-indexMapStart;

  if (i<=(indexMapEnd-indexMapStart)) {
    if (!hashTable[i]) {
      hashTable[i]=new oWordDB();
      hashTable[i]->insert(s+1);
    }
    else {
      hashTable[i]=hashTable[i]->split();
      hashTable[i]->insert(s+1);
    }
  }
  else {
    if (s[0]==0)
      unMapped[0]=0; //Empty string
    else {
      MapTable::iterator it=unMapped.find(s[0]);
      if (it==unMapped.end()) {
        pWordDatabase db(hashAll ? pWordDatabase(new oWordIndexHash(false)):
                                   pWordDatabase(new oWordDB()));
        db->insert(s+1);
        unMapped[s[0]]=db;
      }
      else {
        if (it->second) {
          it->second=it->second->split();
          it->second->insert(s+1);
        }
        else
          assert(s[0]==0);
      }
    }
  }
}

bool oWordIndexHash::lookup(const wchar_t *s) const
{
  DWORD i = s[0] - indexMapStart;

  if (i<=(indexMapEnd-indexMapStart)) {
    if (!hashTable[i])
      return false;
    else
      return hashTable[i]->lookup(s+1);
  }
  else {
    MapTable::const_iterator it=unMapped.find(s[0]);
    if (it==unMapped.end())
      return false;
    else if (s[0]==0)
      return true;
    else
      return it->second->lookup(s+1);
  }
}

oWordIndexHash::oWordIndexHash(bool hashAll_) : hashAll(hashAll_)
{
  for(int k=0;k<hashTableSize;k++)
    hashTable[k]=0;
}

oWordIndexHash::~oWordIndexHash()
{
  clear();
}

oWordList::oWordList() : wh(true) {}

oWordList::~oWordList() {}

void oWordList::insert(const wchar_t *s)
{
  int len=wcslen(s);
  if (len<511) {
    wchar_t bf[512];
    wcscpy_s(bf, s);
    CharLowerBuff(bf, len);
    wh.insert(bf);
  }
  else wh.insert(s);
}

bool oWordList::lookup(const wchar_t *s) const
{
  int len=wcslen(s);
  if (len<511) {
    wchar_t bf[512];
    wcscpy_s(bf, s);
    CharLowerBuff(bf, len);
    return wh.lookup(bf);
  }
  else return wh.lookup(s);
}

void oWordList::serialize(vector<char> &serial) const
{
  int s=wh.serialSize();
  serial.resize(s);
  wh.serialize(&serial[0]);
}

void oWordList::deserialize(const vector<char> &serial)
{
  wh.clear();
  wh.deserialize(&serial[0], &serial[0]+serial.size());
}

void oWordList::save(const wstring &file) const
{
  int f=-1;
  _wsopen_s(&f, file.c_str(), _O_BINARY|_O_CREAT|_O_TRUNC|_O_WRONLY,
            _SH_DENYWR, _S_IREAD|_S_IWRITE);

  if (f!=-1) {
    vector<char> serial;
    serialize(serial);
    char *hdr="WWDB";

    _write(f, hdr, 4);
    DWORD s=serial.size();
    _write(f, &s, 4);
    _write(f, &serial[0], s);

    _close(f);
  }
  else throw std::exception("Could not save word database.");
}

void oWordList::load(const wstring &file)
{
  wstring ex = L"Bad word database. " +file;
  int f=-1;
  _wsopen_s(&f, file.c_str(), _O_BINARY|_O_RDONLY,
            _SH_DENYWR, _S_IREAD|_S_IWRITE);

  if (f!=-1) {
    char hdr[5]={0,0,0,0,0};
    _read(f, hdr, 4);

    if ( strcmp(hdr, "WWDB")!=0 )
      throw meosException(ex);

    DWORD s=0;
    _read(f, &s, 4);

    vector<char> serial(s);

    if (_read(f, &serial[0], s)!=s)
      throw meosException(ex);

    _close(f);

    deserialize(serial);
  }
  else throw meosException(ex);
}

oFreeImport::oFreeImport(void)
{
  
  for(int k=1;k<30;k++)
    separator.insert(k);

  separator.insert(' ');
  separator.insert(',');
  separator.insert(';');
  separator.insert('(');
  separator.insert(')');
  separator.insert('/');

  loaded=false;
}

oFreeImport::~oFreeImport(void)
{
}

wchar_t *trim(wchar_t *str)
{
  int k=wcslen(str)-1;
  while(k>=0 && (iswspace(str[k]) || str[k] == 160))
    str[k--] = 0;

  while(iswspace(*str) || str[k]==160)
    str++;

  return str;
}

wchar_t *oFreeImport::extractPart(wchar_t *&str, int &wordCount) const
{
  wordCount=0;
  while (separator.count(*str))
    str++;

  if (!*str)
    return str;

  wchar_t *out=str;

  wordCount=1;
  while (*str && (!separator.count(*str) || *str==' ')) {
    if (*str==' ') {
      while (*++str==' ');
      if (separator.count(*str) || !*str) {
        if (*str)
          *str++=0;
        return trim(out);
      }
      wordCount++;
    }
    str++;
  }
  if (*str)
    *str++=0;
  return trim(out);
}

wchar_t *oFreeImport::extractWord(wchar_t *&str, int &count) const {
  count=0;
  while (separator.count(*str))
    str++;

  if (!*str)
    return str;

  wchar_t *out=str;

  count=1;
  while (*str && !separator.count(*str)) {
    count++;
    str++;
  }
  if (*str)
    *str++=0;

  return out;
}

wchar_t *oFreeImport::extractLine(wchar_t *&str, int &count) const
{
  count=0;

  while (*str=='\n')
    str++;

  wchar_t *out=str;

  while (*str && *str!='\n') {
    str++;
    count++;
  }

  if (*str)
    *str++=0;

  return out;
}


oEntryPerson::oEntryPerson(const wstring &clb)
{
  club = clb;
  cardNo=0;
}

void oEntryPerson::swap()
{
  if (!name1.empty())
    std::swap(name1, name2);
}

int oEntryPerson::nameCount() const {
  if (!name2.empty())
    return countWords(name1.c_str()) + countWords(name2.c_str());
  else
    return countWords(name1.c_str());
}

void oEntryBlock::clear(const wstring &rulingClub, const wstring &rulingClass)
{
  ePersons.clear();
  eClub = rulingClub;
  eClass = rulingClass;
  eStartTime.clear();
  canInsertName = true;
  isClassSet = false; // Count only explicit set class
  nClubsSet = rulingClub.empty() ? 0 : 1;
}

oEntryBlock::oEntryBlock(const oFreeImport &importer) : freeImporter(importer)
{
  canInsertName=true;
  nClubsSet = 0;
  isClassSet = false;
}

oEntryBlock::oEntryBlock(const oEntryBlock &eb) : freeImporter(eb.freeImporter)
{
  ePersons = eb.ePersons;
  eClub = eb.eClub;
  eClass = eb.eClass;
  eStartTime = eb.eStartTime;
  canInsertName = eb.canInsertName;
  isClassSet = eb.isClassSet;
  nClubsSet = eb.nClubsSet;
}

void oEntryBlock::operator=(const oEntryBlock &eb)
{
  ePersons = eb.ePersons;
  eClub = eb.eClub;
  eClass = eb.eClass;
  eStartTime = eb.eStartTime;
  canInsertName = eb.canInsertName;
  isClassSet = eb.isClassSet;
  nClubsSet = eb.nClubsSet;
}

void oEntryBlock::setClass(const wchar_t *s)
{
  eClass=s;
  if (!ePersons.empty())
    canInsertName=false;
  isClassSet = true;
  completeName();
}

void oEntryBlock::setClub(const wchar_t *s)
{
  eClub = s;
  nClubsSet++;
  for (size_t k = 0; k<ePersons.size(); k++)
    if (ePersons[k].club.empty())
      ePersons[k].club = eClub;
  if (nClubsSet == ePersons.size())
    ePersons.back().club = eClub;

  if (int(ePersons.size())>=freeImporter.getExpectedNumRunners(eClass))
    canInsertName=false;

  completeName();
}

void oEntryBlock::completeName() {
  if (!ePersons.empty() && !ePersons.back().name1.empty()
                         && ePersons.back().name2.empty())
    ePersons.back().name2=L"*";
}

void oEntryBlock::setStartTime(const wchar_t *s)
{
  eStartTime=s;
  if (!ePersons.empty())
    canInsertName=false;

  completeName();
}

void oEntryBlock::setCardNo(int c)
{
  if (ePersons.empty() || ePersons.back().cardNo!=0)
    ePersons.push_back(oEntryPerson(eClub));
  
  for (size_t i = 0; i < ePersons.size(); i++) {
    if (ePersons[i].cardNo == 0) {
      ePersons[i].cardNo=c;
      break;
    }
  }
  completeName();
}

bool oEntryBlock::needCard() const {  
  if (ePersons.empty())
    return true;

  for (size_t i = 0; i < ePersons.size(); i++) {
    if (ePersons[i].cardNo == 0) {
      return true;
    }
  }
  return false;
}

wstring oEntryBlock::getTeamName() const
{
  set<wstring> clubs;
  if (!eClub.empty())
    clubs.insert(eClub);
  for (size_t k = 0; k<ePersons.size(); k++)
    if (!ePersons[k].club.empty())
      clubs.insert(ePersons[k].club);

  if (clubs.size() == 1)
    return *clubs.begin();
  else if (clubs.size() > 1) {
    wstring tname;
    for (set<wstring>::iterator it = clubs.begin(); it != clubs.end(); ++it) {
      if (!tname.empty())
        tname += L"/";
      tname += *it;
    }
    return tname;
  }
  else
    return lang.tl("Lag");
}

wstring oEntryBlock::getName(int k) const
{
  if (size_t(k)>=ePersons.size())
    return lang.tl("N.N.");

  wstring n=ePersons[k].name1;

  if (!ePersons[k].name2.empty())
    n+=L" "+ePersons[k].name2;

  return n;
}

wstring oEntryBlock::getClub(int k) const
{
  if (size_t(k)>=ePersons.size())
    return eClub;

  const wstring &c=ePersons[k].club;
  if (!c.empty())
    return c;

  return eClub;
}


int oEntryBlock::getCard(int k) const
{
  if (k<int(ePersons.size()))
    return ePersons[k].cardNo;
  else
    return 0;
}


vector<wstring> oEntryBlock::getPersons() const
{
  vector<wstring> names;
  wstring n;
  wchar_t bf[256];
  for (size_t k=0;k<ePersons.size();k++) {
    n+=ePersons[k].name1;

    if (!ePersons[k].name2.empty())
      n+=L" "+ePersons[k].name2;

    if (ePersons[k].cardNo>0) {
      swprintf_s(bf, L" (%d)", ePersons[k].cardNo);
      n+=bf;
    }


    names.push_back(n);
    n.clear();
  }
  return names;
}

int oEntryBlock::nameCount()
{
  if (ePersons.empty())
    return 0;
  int space=0;
  const wstring &name1=ePersons.back().name1;
  const wstring &name2=ePersons.back().name2;

  if (!name1.empty()) {
    space++;
    for (size_t k=0;k<name1.size();k++)
      if (name1[k]==' ')
        space++;
  }
  if (!name2.empty()) {
    space++;
    for (size_t k=0;k<name1.size();k++)
      if (name1[k]==' ')
        space++;
  }
  return space;
}

void oEntryBlock::addPerson(const wchar_t *s, bool complete)
{
  if (ePersons.empty())
    ePersons.push_back(oEntryPerson(eClub));

  if (ePersons.back().nameCount()>=2 && countWords(s)>=2)
    completeName();

  if (complete) {
    if (!ePersons.back().name1.empty())
      ePersons.push_back(oEntryPerson(eClub));

    ePersons.back().name1=s;
  }
  else {
    if (ePersons.back().name1.empty())
      ePersons.back().name1=s;
    else if (ePersons.back().name2.empty())
      ePersons.back().name2=s;
    else {
      ePersons.push_back(oEntryPerson(eClub));
      ePersons.back().name1=s;
    }
  }
}

bool oEntryBlock::acceptMoreClubs(int expectedNumRunners) const
{
  return nClubsSet < int(ePersons.size()) ||
         nClubsSet < expectedNumRunners;
}

bool oEntryBlock::expectMoreNames(int expectedNumRunners) const
{
  return int(ePersons.size()) < expectedNumRunners ||
         (expectedNumRunners == ePersons.size() &&
                 ePersons.back().nameCount()<=1);
}


void oEntryBlock::cleanEntry()
{
  for (size_t k=0;k<ePersons.size(); k++) {
    if (ePersons[k].name1.empty()) {
      ePersons.resize(k, oEntryPerson(L"")); //Delete this entry
      return;
    }
    else if (ePersons[k].name2==L"*") //Fix completed names.
      ePersons[k].name2.clear();
  }
}

/*
bool oFreeImport::analyze(vector<bool> &b, int &offset, int &delta) const
{
  vector<int> d;
  d.reserve(b.size()/3);

  for (size_t k=0;k<b.size();k++)
    if (b[k]) d.push_back(k);

  if (d.size()<2) {
    delta=0;
    offset=0;
    return false;
  }

  offset=d[0];

  delta=0;
  for (size_t k=1;k<d.size();k++)
    delta=gcd(delta, d[k]-d[k-1]);

  if (delta>1) {
    int x=offset;
    while(unsigned(x)<b.size())
      b[x]=true, x+=delta;

    return true;
  }
  else
    return false;

}*/

bool oFreeImport::isCard(const wchar_t *p) const
{
  int k=_wtoi(p);
  return (p[0]=='0' && p[1]==0) || (k>=1000 && k<10000000);
}

bool oFreeImport::isTime(const wstring &m) const
{
  if (m.empty() || m.length()>9)
    return false;

  for(size_t k=0;k<m.length();k++)
    if (!isdigit(BYTE(m[k])) && !(m[k]==':' || m[k]=='.'))
      return false;

  int hour=_wtoi(m.c_str());
  if (hour<0 || hour>23)
    return false;

  int minute=0;
  int second=0;
  int kp=m.find_first_of(':');

  if (kp>0) {
    wstring mtext=m.substr(kp+1);
    minute=_wtoi(mtext.c_str());

    if (minute<0 || minute>60)
      return false;

    kp=mtext.find_last_of(':');

    if (kp>0) {
      second=_wtoi(mtext.substr(kp+1).c_str());
      if (second<0 || second>60)
        return false;
    }
  }
  int t=hour*3600+minute*60+second;
  if (t<=0)
    return false;

  return true;
}

bool oFreeImport::isCrap(const wchar_t *p) const
{
  int s=wcslen(p);

  for (int k=0;k<s;k++) {
    if (p[k]=='(' || p[k]==')' ||
        (p[k]>='0' && p[k]<='9'))
      return true;
  }
  return false;
}

bool oFreeImport::isName(const wchar_t *p) const
{
  wchar_t bf[1024];
  wcscpy_s(bf, p);
  wchar_t *str=bf;
  int c=1;
  vector<const wchar_t *> w;

  while(c>0) {
    const wchar_t *out= extractWord(str,c);
    if (c>0)
      w.push_back(out);
  }
  if (w.size()==1)
    return familyDB.lookup(w[0]) || givenDB.lookup(w[0]);
  else if (w.size()<4) {
    int n=0;
    int t=(w.size()+1)/2;
    for (size_t k=0;k<w.size();k++) {
      n+=familyDB.lookup(w[k]) || givenDB.lookup(w[k]) ? 1:0;
      if (isCard(w[k]))
        return false;
      if (classDB.lookup(w[k]))
        return false;
    }
    return n>=t;
  }

  return false;
}

void oFreeImport::analyzePart(wchar_t *part, const MatchPattern &ptrn, int nNamesPerPart,
                              oEntryBlock &entry, vector<oEntryBlock> &entries, bool allowName)
{
  vector<const wchar_t *> words;
  int count=1;

  while (count>0) {
    const wchar_t *out=extractWord(part, count);
    if (count>0)
      words.push_back(out);
  }

  allowName &= !entry.hasName() || nNamesPerPart!=1;

  vector<bool> isNameV(words.size(), false);
  vector<bool> isClub(words.size(), false);
  vector<bool> isClass(words.size(), false);
  vector<bool> isCardV(words.size(), false);
  vector<bool> isTimeV(words.size(), false);
  vector<bool> used(words.size(), false);

  int names=0;
  int classes=0;
  int clubs=0;
  int cards=0;
  int times=0;

  for (size_t j=0;j<words.size();j++) {
    if (familyDB.lookup(words[j]) || givenDB.lookup(words[j])) {
      isNameV[j]=true;
      names++;
    }
    if (clubDB.lookup(words[j])) {
      isClub[j]=true;
      clubs++;
    }
    if (classDB.lookup(words[j])) {
      isClass[j]=true;
      classes++;
    }
    if (isCard(words[j])) {
      isCardV[j]=true;
      cards++;
    }
    if (isTime(words[j])) {
      isTimeV[j]=true;
      times++;
    }
  }

  if (allowName) {
    if (names>0 && (clubs+classes+cards+times)<1) {
      if (!entry.canInsertName) {
        entry.cleanEntry();
        entries.push_back(entry);
        entry.clear(rulingClub, rulingClass);
      }
      wstring n;
      for (size_t j=0;j<words.size();j++) {
        if (!isCrap(words[j]))
          n+=wstring(words[j])+L" ";
      }
      n.resize(n.size()-1);
      entry.addPerson(n.c_str(), false);
      lastInsertedType = Name;
      return;
    }
  }
  wstring cls;
  wstring clb;
  wstring name;
  Types lastType = Name; //Name
  if (entry.nameCount()>0 && !entry.expectMoreNames(getExpectedNumRunners(entry.eClass))
    && !allowName)
    lastType = Club; //Guess club

  for (size_t k=0;k<words.size();) {
    if (isTimeV[k]) {
      entry.setStartTime(words[k]);
      lastInsertedType = Time;
      used[k]=true, k++;
      if (entry.expectMoreNames(getExpectedNumRunners(entry.eClass)))
        lastType = Name;
      else lastType = Unknown;
    }
    else if (isCardV[k]) {
      entry.setCardNo(_wtoi(words[k]));
      lastInsertedType = Card;
      used[k]=true, k++;
      lastType = Name;
      if (entry.expectMoreNames(getExpectedNumRunners(entry.eClass)))
        lastType = Name;
      else lastType = Unknown;
    }
    else if (isClass[k]) {
      while(k<words.size() && isClass[k])
        cls+=wstring(words[k])+L" ", used[k]=true, k++;
      lastType = Class;
    }
    else if (isClub[k]) {
      while(k<words.size() && isClub[k])
        clb+=wstring(words[k])+L" ", used[k]=true, k++;
      lastType = Club;
    }
    else if (isNameV[k] && allowName) {
      while(k<words.size() && isNameV[k]) {
        if (!isCrap(words[k]))
          name+=wstring(words[k])+L" ";
        used[k]=true, k++;
      }
      lastType = Name;
    }
    else {
      if (lastType == Name) {
        if (!isCrap(words[k]))
          name+=wstring(words[k])+L" ";
        k++;
      }
      else if (lastType == Class)
        cls+=wstring(words[k])+L" ", k++;
      else if (lastType == Club)
        clb+=wstring(words[k])+L" ", k++;
      else k++;
    }
  }

  if (!clb.empty()) {
    if (lastInsertedType == None)
      rulingClub = clb;

    clb.resize(clb.size()-1);
    entry.setClub(clb.c_str());
    lastInsertedType = Club;
  }
  if (!cls.empty()) {
    if (lastInsertedType == None)
      rulingClass = cls;
    cls.resize(cls.size()-1);
    entry.setClass(cls.c_str());
    lastInsertedType = Class;
  }
  if (!name.empty()) {
    name.resize(name.size()-1);
    entry.addPerson(name.c_str(), words.size()>2);
    lastInsertedType = Name;
  }
}

void oFreeImport::test(const oRunnerList &li)
{
  oRunnerList::const_iterator it;
  //classDB.insert("H21");
  //classDB.insert("Herrar");

  for(it=li.begin();it!=li.end();++it) {
    givenDB.insert(it->getGivenName().c_str());
    familyDB.insert(it->getFamilyName().c_str());
    clubDB.insert(it->getClub().c_str());
    classDB.insert(it->getClass().c_str());
  }


  wstring b(L"0"), c(L"a2"), d(L"a");

  const int end='z'+5;
  for(int k='a';k<=end+1;k++)
    if (k>end)
      givenDB.insert(b.c_str());
    else {
      wstring z=b+wchar_t(k);
      for( int j='a';j<=end+1;j++) {
        if (j>end)
          givenDB.insert(z.c_str());
        else {
          givenDB.insert( (z+wchar_t(j)+d).c_str() );
        }
      }
    }

  givenDB.save(L"test.mwd");
  oWordList wl;
  wl.load(L"test.mwd");
  wl.save(L"test2.mwd");
  for(it=li.begin();it!=li.end();++it)
    givenDB.insert(it->getGivenName().c_str());

  wl.save(L"test3.mwd");
}

/** Must not return 0 */
int oFreeImport::getExpectedNumRunners(const wstring &cls) const
{
  wstring key(canonizeName(cls.c_str()));
  map<wstring,int>::const_iterator it = runnersPerClass.find(key);

  if (it != runnersPerClass.end())
    return it->second;
  else
    return 1;
}

void oFreeImport::init(const oRunnerList &r, const oClubList &clb, const oClassList &cls)
{
  runnersPerClass.clear();
  vector<wstring> split_vector;
  wstring sep(L" ");

  for(oRunnerList::const_iterator it=r.begin();it!=r.end();++it) {
    givenDB.insert(it->getGivenName().c_str());
    wstring f=it->getFamilyName();
    familyDB.insert(f.c_str());

    split(f, sep, split_vector);
    if (split_vector.size()>1)
      for(size_t k=0;k<split_vector.size();k++) {
        familyDB.insert(split_vector[k].c_str());
      }
  }

  for(oClubList::const_iterator it=clb.begin();it!=clb.end();++it) {
    clubDB.insert(it->getName().c_str());
    split(it->getName().c_str(), sep, split_vector);
    if (split_vector.size()>1)
      for(size_t k=0;k<split_vector.size();k++) {
        clubDB.insert(split_vector[k].c_str());
      }
  }

  for(oClassList::const_iterator it=cls.begin();it!=cls.end();++it) {
    wstring nname(it->getName().begin(), it->getName().end());
    classDB.insert(nname.c_str());
    split(nname.c_str(), sep, split_vector);
    if (split_vector.size()>1)
      for(size_t k=0;k<split_vector.size();k++) {
        classDB.insert(split_vector[k].c_str());
      }
    int numrunner = it->getNumDistinctRunners();
    wstring key(canonizeName(nname.c_str()));
    runnersPerClass[key] = numrunner;
  }

  const wchar_t *clsPrefix[8] = {L"h", L"d", L"m", L"w",
                       L"herrar", L"damer", L"men", L"women"};

  wchar_t bf[64];

  vector<int> ages;

  for (int k=10;k<=16;k++)
    ages.push_back(k);
  ages.push_back(18);
  ages.push_back(20);
  ages.push_back(21);
  for (int k=35;k<=100;k+=5)
    ages.push_back(k);

  for (int k=0;k<8;k++) {
    classDB.insert(clsPrefix[k]);

    for (size_t j=0; j<ages.size();j++) {
      swprintf_s(bf, L"%d", ages[j]);
      classDB.insert(bf);

      swprintf_s(bf, L"%s%d", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s %d", clsPrefix[k], ages[j]);
      classDB.insert(bf);

      swprintf_s(bf, L"%s%dM", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%dL", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%dK", clsPrefix[k], ages[j]);
      classDB.insert(bf);

      swprintf_s(bf, L"%s %dM", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s %dL", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s %dK", clsPrefix[k], ages[j]);
      classDB.insert(bf);

      swprintf_s(bf, L"%s%d M", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%d L", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%d K", clsPrefix[k], ages[j]);
      classDB.insert(bf);

      swprintf_s(bf, L"%s%d Lång", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%d Kort", clsPrefix[k], ages[j]);
      classDB.insert(bf);
      swprintf_s(bf, L"%s%d Motion", clsPrefix[k], ages[j]);
      classDB.insert(bf);

      if (ages[j]>=18 && ages[j]<=21) {
        swprintf_s(bf, L"%s %dE", clsPrefix[k], ages[j]);
        classDB.insert(bf);
        swprintf_s(bf, L"%s %d Elit", clsPrefix[k], ages[j]);
        classDB.insert(bf);
        swprintf_s(bf, L"%s%dE", clsPrefix[k], ages[j]);
        classDB.insert(bf);
        swprintf_s(bf, L"%s%d Elit", clsPrefix[k], ages[j]);
        classDB.insert(bf);
      }
    }
  }
  classDB.insert(L"L");
  classDB.insert(L"Lång");
  classDB.insert(L"E");
  classDB.insert(L"Elit");
  classDB.insert(L"Elite");
  classDB.insert(L"Adult");
  classDB.insert(L"Vuxen");
  classDB.insert(L"Insk");
  classDB.insert(L"Insk.");
  classDB.insert(L"Inskolning");
  classDB.insert(L"M");
  classDB.insert(L"ÖM");
  classDB.insert(L"Motion");
  classDB.insert(L"K");
  classDB.insert(L"Kort");
  classDB.insert(L"Mellan");
  classDB.insert(L"Svår");
  classDB.insert(L"Lätt");
  classDB.insert(L"Kortlätt");
  classDB.insert(L"Långsvår");
  classDB.insert(L"km");
  classDB.insert(L"Short");
  classDB.insert(L"Long");
  classDB.insert(L"Öppen");
  classDB.insert(L"Ö");
  classDB.insert(L"Ungdom");
  classDB.insert(L"Ung.");
  classDB.insert(L"U");
  classDB.insert(L"U1");
  classDB.insert(L"U2");
  classDB.insert(L"U3");
  classDB.insert(L"U4");

  for (int j=1;j<10;j++) {
    swprintf_s(bf, L"Ö%d", j);
    classDB.insert(bf);
    swprintf_s(bf, L"Ö %d", j);
    classDB.insert(bf);
    swprintf_s(bf, L"Öppen %d", j);
    classDB.insert(bf);
    swprintf_s(bf, L"D%d", j);
    classDB.insert(bf);
    swprintf_s(bf, L"Direkt %d", j);
    classDB.insert(bf);
  }
}

bool oFreeImport::isHeaderWord(const wstring &word) const {
  if (headerWords.empty()) {
    headerWords.insert(canonizeName(L"namn"));
    headerWords.insert(canonizeName(L"klass"));
    headerWords.insert(canonizeName(L"ålder"));
    headerWords.insert(canonizeName(L"anmälda"));
    headerWords.insert(canonizeName(L"anmälningar"));
    headerWords.insert(canonizeName(L"deltagare"));
    headerWords.insert(canonizeName(L"löpare"));
    headerWords.insert(canonizeName(L"bricka"));
    headerWords.insert(canonizeName(L"starttid"));
    headerWords.insert(canonizeName(L"nummer"));
    headerWords.insert(canonizeName(L"startnummer"));
    headerWords.insert(canonizeName(L"bricknummer"));
    headerWords.insert(canonizeName(L"nummer"));
    headerWords.insert(canonizeName(L"klubb"));
    headerWords.insert(canonizeName(L"klassnamn"));
    headerWords.insert(canonizeName(L"pinne"));
    headerWords.insert(canonizeName(L"nummerlapp"));
    headerWords.insert(canonizeName(L"SI"));
    headerWords.insert(canonizeName(L"förening"));
    headerWords.insert(canonizeName(L"rank"));
    headerWords.insert(canonizeName(L"ranking"));
    headerWords.insert(canonizeName(L"nr-lapp"));
    headerWords.insert(canonizeName(L"tid"));

    headerWords.insert(canonizeName(L"name"));
    headerWords.insert(canonizeName(L"age"));
    headerWords.insert(canonizeName(L"class"));
    headerWords.insert(canonizeName(L"start"));
    headerWords.insert(canonizeName(L"time"));
    headerWords.insert(canonizeName(L"club"));
    headerWords.insert(canonizeName(L"card"));
  }
  return headerWords.count(canonizeName(word.c_str()))>0;
}

bool match_char(wchar_t c, const wstring &sep) {
  for (size_t j = 0; j<sep.size(); j++)
    if (c == sep[j])
      return true;

  return false;
}

bool oFreeImport::analyzeHeaders(vector<wchar_t *> &line) const
{
  if (line.empty())
    return true;

  int header = 0;
  int nonheader = 0;
  bool forceNoHeader = false;
  vector<wstring> firstWord;
  const wstring separators = L" :,.=";
  for (size_t k = 0; k < line.size(); k++) {
    vector<wstring> words;
    split(line[k], separators, words);
    if (words.empty())
      firstWord.push_back(L"");
    else
      firstWord.push_back(words.front());

    for (size_t j = 0; j < words.size(); j++) {
      if (words[j].empty())
        continue;
      if (isHeaderWord(words[j]) )
        header++;
      else {
        nonheader++;
        // If the header contains a real name, class or club, do not reject
        if (isName(words[j].c_str()))
          forceNoHeader = true;

        if (classDB.lookup(words[j].c_str()))
          forceNoHeader = true;
        if (clubDB.lookup(words[j].c_str()) || clubDB.lookup(line[k]))
          forceNoHeader = true;
      }
    }
  }

  if (!forceNoHeader && header > nonheader)
    return true;

  // Remove header words
  for (size_t k = line.size()-1; k < line.size(); k--) {
    if (isHeaderWord(line[k])) {
      line.erase(line.begin()+k);
    }
    else if (isHeaderWord(firstWord[k])) {
      line[k] += firstWord[k].size();
      int j = 0;
      while (line[k][j] && match_char(line[k][j], separators))
        j++;
      line[k] += j;
    }
  }
  return false;
}

const int CLEARROW = 1;
const int IGNOREROW = 2;
const int COMPLETEROW = 4;

int oFreeImport::preAnalyzeRow(vector<wchar_t *> &p,
                               const vector<MatchPattern> &ptrn,
                               vector<int> &classified)
{
  int stat = 0;

  int names = 0;
  int clubs = 0;
  int cards = 0;
  int classes = 0;
  int unknown = 0;

  for (size_t j=0;j<p.size();j++) {
    int type=-1;
    if (ptrn[j].isClub() && clubDB.lookup(p[j]))
      type=0;
    else if (ptrn[j].isClass() && classDB.lookup(p[j]))
      type=1;
    else if (ptrn[j].isTime() && isTime(p[j]))
      type=2;
    else if (ptrn[j].isCard() && isCard(p[j]))
      type=3;
    else if (ptrn[j].isName() && isName(p[j]))
      type=4;
    else if (clubDB.lookup(p[j]))
      type=0;
    else if (classDB.lookup(p[j]))
      type=1;
    else if (isTime(p[j]))
      type=2;
    else if (isCard(p[j]))
      type=3;
    else if (isName(p[j]))
      type=4;

    if (type==-1 && ptrn.size()>1) {
      if (ptrn[j].isClub())
        type=0;
      else if (ptrn[j].isClass())
        type=1;
      else if (ptrn[j].isCard() && _wtoi(p[j])>0)
        type=3;
    }

    classified.push_back(type);
    if (type == 1)
      classes++;
    else if (type==3)
      cards++;
    else if (type==4)
      names++;
    else if (type==0)
      clubs++;
    else
      unknown++;

    const wchar_t *canP = canonizeName(p[j]);
    if (wcscmp(canP, L"vakant")==0 || wcscmp(canP, L"vacant")==0)
      stat|=IGNOREROW;
  }

  if (clubs>0 && names>0 && cards>0)
    stat|=COMPLETEROW;

  if (p.size()==1 && (classes>0 || clubs>0))
    stat|=CLEARROW;

  while (cards > names && names>0) {
    int idx = -1;
    for (size_t k = 0; k<classified.size(); k++) {
      if (classified[k] == 3) {
        int a = idx>=0 ? _wtoi(p[idx]) : -1;
        if (a==-1)
          idx = k;
        else {
          int b = _wtoi(p[k]);
          if (b>0 && b<a)
            idx = k;
        }
      }
    }
    if (idx>=0) {
      classified[idx] = -2;
      cards--;
      unknown++;
    }
  }

  for (int k = p.size()-1; k>=0;k--) {
    if (classified[k] == -2) {
      classified.erase(classified.begin()+k);
      p.erase(p.begin()+k);
    }
  }
  return stat;
}


void oFreeImport::extractEntries(wchar_t *str, vector<oEntryBlock> &entries)
{
  entries.clear();

  vector< vector<wchar_t *> > parts;
  parts.reserve(wcslen(str)/24);
  set<int> lineTypes;
  int count=1;

  while(count>0) {
    wchar_t *line=extractLine(str, count);

    if (count>0) {
      parts.push_back( vector<wchar_t *>() );

      while(count>0) {
        wchar_t *out=extractPart(line, count);
        if (count>0)
          parts.back().push_back(out);
      }
      if ( analyzeHeaders(parts.back()) ) {
        parts.pop_back();
        count = 1;
      }
      else {
        lineTypes.insert(parts.back().size());
        count=1;
      }
    }

  }

  map<int, vector<MatchPattern> > patterns;
  map<int, int> nNames; //Number of name entries per pattern
  //Analyze line types
  for(set<int>::iterator it=lineTypes.begin();
                      it!=lineTypes.end();++it) {
    vector<MatchPattern> mp(*it);
    int limit=0;
    for (size_t k=0;k<parts.size();k++) {
      if (parts[k].size()==*it) {
        limit++;
        const vector<wchar_t *> &p=parts[k];
        for (int j=0;j<*it; j++) {
          if (classDB.lookup(p[j]))
            mp[j].nClass++;
          if (clubDB.lookup(p[j]))
            mp[j].nClub++;
          if (isName(p[j])) {
            mp[j].nName++;
            if (countWords(p[j])==1)
              mp[j].singleName++;
          }
          if (isCard(p[j]))
            mp[j].nCard++;
          if (isTime(p[j]))
            mp[j].nTime++;
        }
      }
    }

    if (limit==1)
      limit=0;
    else if (limit<=3)
      limit=1;
    else if (limit<=10)
      limit=2;
    else
      limit=limit/5;

    int nn=0;

    for (size_t k=0;k<mp.size();k++)
      if (mp[k].isName(limit))
        nn++;

    // Check that single names (Andersson, Karl) are neighbors.
    for (size_t k=0;k<mp.size();k++)
      if (mp[k].isName(limit) &&
          (k+1>=mp.size() || !mp[k+1].isName(limit)) &&
          (k==0 || !mp[k-1].isName(limit)) ) {
        mp[k].singleName=false;
      }

    nNames[*it]=nn;
    swap(patterns[*it], mp);
  }

  entries.reserve(parts.size());
  oEntryBlock entry(*this);

  vector<const char *> words;
  rulingClub = L"";
  rulingClass = L"";
  lastInsertedType = None;
  for (size_t k=0;k<parts.size();k++) {
    //Number of read names (add 2 for a complete name, 1 for a partial)
    int readNames=0;

    vector<wchar_t *> &p=parts[k];

    vector<int> classified;
    int res = preAnalyzeRow(p, patterns[p.size()], classified);

    int s = p.size();
    vector<MatchPattern> &ptrn=patterns[s];

    if (ptrn.empty())
      ptrn.resize(s);
    assert(ptrn.size() == s);

    lastInsertedType = None;
    if (res & CLEARROW) {
      entry.cleanEntry();
      if (entry.hasName()) {
        entries.push_back(entry);
      }
      entry.clear(rulingClub, rulingClass);
    }
    else if (res & COMPLETEROW) {
      entry.cleanEntry();
      if (entry.hasName()) {
        entries.push_back(entry);
      }
      entry.clear(rulingClub, rulingClass);
    }
    if (res & IGNOREROW)
      continue;

    for (int j=0;j<s;j++) {
      Types type = Unknown;
      if (ptrn[j].isClub() && clubDB.lookup(p[j]))
        type = Club;
      else if (ptrn[j].isClass() && classDB.lookup(p[j]))
        type = Class;
      else if (ptrn[j].isTime() && isTime(p[j]))
        type = Time;
      else if (ptrn[j].isCard() && isCard(p[j]))
        type = Card;
      else if (ptrn[j].isName() && isName(p[j]))
        type = Name;
      else if (clubDB.lookup(p[j]))
        type = Club;
      else if (classDB.lookup(p[j]))
        type = Class;
      else if (isTime(p[j]))
        type = Time;
      else if (isCard(p[j]))
        type = Card;
      else if (isName(p[j]) && entry.nameCount()<=1)
        type = Name;

      if (type==Unknown && ptrn.size()>1) {
        if (ptrn[j].isClub())
          type = Club;
        else if (ptrn[j].isClass())
          type = Class;
        else if (ptrn[j].isCard() && _wtoi(p[j])>0)
          type = Card;
      }

      //We have a suggested name, but there should only
      //be one name and we already got one.
      if ((type == Name && entry.hasName() && (2*nNames[s])<=readNames))
        type = Unknown;

      if (type==Name && isCrap(p[j]))
        type = Unknown;

      if (type == Unknown)
        analyzePart(p[j], ptrn[j], nNames[s], entry,
                      entries, (2*nNames[s])>readNames);
      else if (type == Club) {
        bool moreClubs = entry.acceptMoreClubs(getExpectedNumRunners(entry.eClass));
        if (!moreClubs && lastInsertedType == Club)
          continue;

        if (!moreClubs && entry.hasName()) {
          readNames=0;
          entry.cleanEntry();
          entries.push_back(entry);
          entry.clear(rulingClub, rulingClass);
        }
        else if (j<=1 && s<=2)
          // Set a ruling club if it first on a line containing at most 2 parts. (We allow club+class)
          rulingClub = p[j];

        lastInsertedType = Club;
        entry.setClub(p[j]);
        // If we have an odd number of names read
        // we were expecting a completing name,
        // but none came, so we complete what we have instead.
        if (readNames&1) {
          readNames++;
          entry.completeName();
        }
      }
      else if (type == Class) {
        if (lastInsertedType == Class)
          continue;

        if (entry.hasClass() && entry.hasName()) {
          readNames=0;
          entry.cleanEntry();
          entries.push_back(entry);
          entry.clear(rulingClub, rulingClass);
        }
        else if (j<=1 && s<=2)
          // Set a ruling class if it first on a line containing at most 2 parts. (We allow club+class)
          rulingClass = p[j];

        lastInsertedType = Class;
        entry.setClass(p[j]);
        if (readNames&1) {
          readNames++;
          entry.completeName();
        }
      }
      else if (type == Time) {
        if (lastInsertedType == Time)
          continue;

        if (!entry.eStartTime.empty()) {
          if (entry.ePersons.size()>1 && !entry.hasName())
            entry.ePersons.pop_back(); //Warning

          if (entry.hasName()) {
            readNames=0;
            entry.cleanEntry();
            entries.push_back(entry);
            entry.clear(rulingClub, rulingClass);
          }
        }
        lastInsertedType = Time;
        entry.setStartTime(p[j]);
        if (readNames&1) {
          readNames++;
          entry.completeName();
        }
      }
      else if (type == Card) {
        if (lastInsertedType == Card && !entry.needCard())
          continue;

        lastInsertedType = Card;
        entry.setCardNo(_wtoi(p[j]));
        if (readNames&1) {
          readNames++;
          entry.completeName();
        }
      }
      else if (type == Name) {
        lastInsertedType = Name;
        bool complete = ptrn[j].isCompleteName();
        entry.addPerson(p[j], complete);
        readNames += complete ? 2:1;
      }
    }// End loop over parts

    if (entry.hasName()) {
      entry.cleanEntry();
      entries.push_back(entry);
      entry.clear(rulingClub, rulingClass);
    }
    else if (res & COMPLETEROW) {
      entry.cleanEntry();
      if (entry.hasName())
        entries.push_back(entry); // Else lost data
      entry.clear(rulingClub, rulingClass);
    }
  }

  for (size_t k=1;k<entries.size(); k++) {
    if (entries[k].eClub.empty())
      entries[k].eClub=entries[k-1].eClub;

    if (entries[k].eClass.empty())
      entries[k].eClass=entries[k-1].eClass;
  }

  for (int k=int(entries.size())-2;k>=0; k--) {
    if (entries[k].eClub.empty())
      entries[k].eClub=entries[k+1].eClub;

    if (entries[k].eClass.empty())
      entries[k].eClass=entries[k+1].eClass;
  }
}

void oFreeImport::showEntries(gdioutput &gdi, const vector<oEntryBlock> &entries)
{
  gdi.setCX(20);
  for (size_t k=0;k<entries.size();k++) {
    gdi.pushX();
    int x=gdi.getCX();
    int y=gdi.getCY();
    gdi.addStringUT(y, x, 0, entries[k].eStartTime);
    vector<wstring> names = entries[k].getPersons();
    for (size_t j=0;j<names.size();j++) {
      gdi.addStringUT(y+gdi.getLineHeight()*j, x+50, 0, names[j]).setColor(colorRed);
      wstring clb = entries[k].getClub(j);
      gdi.addStringUT(y+gdi.getLineHeight()*j, x+300, 0, clb);
    }
    gdi.addStringUT(y, x+500, 0, entries[k].eClass);
    gdi.dropLine(0.1);
    gdi.popX();
  }
  gdi.refresh();
}

void oFreeImport::addEntries(pEvent oe, const vector<oEntryBlock> &entries)
{
  map<int, int> teamno; //For automatic name generation Club/Class
  for (size_t k=0;k<entries.size();k++) {
    assert(!entries[k].eClass.empty());
    pClass pc=oe->getClass(entries[k].eClass);

    if (!pc) {
      pc=oe->addClass(entries[k].eClass);
      if (entries[k].getNumPersons()>1)
        pc->setNumStages(entries[k].getNumPersons());
      pc->synchronize();
    }

    if (pc) {
      int nr=pc->getNumDistinctRunners();
      if (nr==1) {
        pRunner r=oe->addRunner(entries[k].getName(0),
                                entries[k].getClub(0), pc->getId(),
                                entries[k].getCard(0), 0, true);

        r->setStartTimeS(entries[k].eStartTime);
        r->setCardNo(entries[k].getCard(0), false);

        r->addClassDefaultFee(false);
        r->synchronize();
      }
      else {
        pClub club=oe->getClubCreate(0, entries[k].eClub);
        wstring team = entries[k].getTeamName();
        //int id=pc->getId() + 10000 * (club ? club->getId() : 0);

        //char tname[256];
        //sprintf_s(tname, "%s %d", entries[k].eClub.c_str(), ++teamno[id]);
        pTeam t=oe->addTeam(team, club ? club->getId() : 0, pc->getId());
        if (t) {
          t->setStartNo(t->getId(), false);

          for (int j=0;j<max(nr, entries[k].getNumPersons());j++) {
            pRunner r=oe->addRunner(entries[k].getName(j), entries[k].getClub(j),
                                    pc->getId(), entries[k].getCard(j), 0, false);
            r->setCardNo(entries[k].getCard(j), false);
            r->addClassDefaultFee(false);
            t->setRunner(j, r, true);
          }

          t->apply(true, 0, false);
        }
      }
    }
  }
  oe->makeUniqueTeamNames();
}

void oFreeImport::load()
{
  wchar_t bf[260];
  bool warn=false;
  getUserFile(bf, L"wfamily.mwd");
  try {
    familyDB.load(bf);
  } catch(std::exception &) {warn=true;}
  getUserFile(bf, L"wgiven.mwd");
  try {
    givenDB.load(bf);
  } catch(std::exception &) {warn=true;}
  getUserFile(bf, L"wclub.mwd");
  try {
    clubDB.load(bf);
  } catch(std::exception &) {warn=true;}
  getUserFile(bf, L"wclass.mwd");
  try {
    classDB.load(bf);
  } catch(std::exception &) {warn=true;}

  loaded=true;
}

#include "RunnerDB.h"

void oFreeImport::buildDatabases(oEvent &oe) {
  vector<wstring> given;
  vector<wstring> family;
  oe.getRunnerDatabase().getAllNames(given, family);

  for (size_t k = 0; k < given.size(); k++)
    givenDB.insert(given[k].c_str());

  for (size_t k = 0; k < given.size(); k++)
    familyDB.insert(family[k].c_str());

  const vector<oDBClubEntry> &clubs = oe.getRunnerDatabase().getClubDB(false);
  for (size_t k = 0; k< clubs.size(); k++) {
    clubDB.insert(clubs[k].getName().c_str());
    //clubs[k].altNames
  }

  //oe.init(*this);

  save();
}

void oEvent::init(oFreeImport &fi) {
  fi.init(Runners, Clubs, Classes);
}

void oFreeImport::save() const
{
  wchar_t bf[260];
  wstring bu;
  getUserFile(bf, L"wfamily.mwd");
  bu=wstring(bf)+L".old";
  _wremove(bu.c_str());
  _wrename(bf, bu.c_str());
  familyDB.save(bf);
  getUserFile(bf, L"wgiven.mwd");
  bu=wstring(bf)+L".old";
  _wremove(bu.c_str());
  _wrename(bf, bu.c_str());
  givenDB.save(bf);
  getUserFile(bf, L"wclub.mwd");
  bu=wstring(bf)+L".old";
  _wremove(bu.c_str());
  _wrename(bf, bu.c_str());
  clubDB.save(bf);
  getUserFile(bf, L"wclass.mwd");
  bu=wstring(bf)+L".old";
  _wremove(bu.c_str());
  _wrename(bf, bu.c_str());
  classDB.save(bf);
}
