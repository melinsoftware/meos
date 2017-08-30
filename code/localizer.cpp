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
#include "localizer.h"
#include <fstream>
#include <vector>
#include "meos_util.h"
#include "random.h"
#include "oFreeImport.h"

class LocalizerImpl
{
  string language;
  map<string, string> table;
  map<string, string> unknown;
  void loadTable(const vector<string> &raw, const string &language);
  mutable oWordList *givenNames;

public:

  const oWordList &getGivenNames() const;

  void translateAll(const LocalizerImpl &all);

  const string &translate(const string &str, bool &found);

  void saveUnknown(const string &file);
  void saveTable(const string &file);
  void loadTable(const string &file, const string &language);
  void loadTable(int resource, const string &language);

  void clear();
  LocalizerImpl(void);
  ~LocalizerImpl(void);
};

Localizer::LocalizerInternal::LocalizerInternal(void)
{
  impl = new LocalizerImpl();
  implBase = 0;
  owning = true;
  user = 0;
}

Localizer::LocalizerInternal::~LocalizerInternal(void)
{
  if (user) {
    user->owning = true;
    impl = 0;
    implBase = 0;
  }
  else {
    delete impl;
    delete implBase;
  }
}

void Localizer::LocalizerInternal::set(Localizer &lio) {
  Localizer::LocalizerInternal &li = *lio.linternal;
  if (li.user || user)
    throw std::exception("Runtime error");

  if (owning) {
    delete impl;
    delete implBase;
  }

  implBase = li.implBase;
  impl = li.impl;
  li.user = this;
}

vector<string> Localizer::LocalizerInternal::getLangResource() const {
  vector<string> v;
  for (map<string, string>::const_iterator it = langResource.begin(); it !=langResource.end(); ++it)
    v.push_back(it->first);

  return v;
}

const oWordList &Localizer::LocalizerInternal::getGivenNames() const {
  return impl->getGivenNames();
}

LocalizerImpl::LocalizerImpl(void)
{
  givenNames = 0;
}

LocalizerImpl::~LocalizerImpl(void)
{
  if (givenNames)
    delete givenNames;
}

const string &Localizer::LocalizerInternal::tl(const string &str) const {
  bool found;
  const string *ret = &impl->translate(str, found);
  if (found || !implBase)
    return *ret;

  ret = &implBase->translate(str, found);
  return *ret;
}

const string &LocalizerImpl::translate(const string &str, bool &found)
{
  found = false;
  static int i = 0;
  const int bsize = 17;
  static string value[bsize];
  int len = str.length();

  if (len==0)
    return str;

  if (str[0]=='#') {
    i = (i + 1)%bsize;
    value[i] = str.substr(1);
    found = true;
    return value[i];
  }

  if (str[0]==',' || str[0]==' ' || str[0]=='.'
       || str[0]==':'  || str[0]==';' || str[0]=='<' || str[0]=='>' || str[0]=='-' || str[0]==0x96) {
    unsigned k=1;
    while(str[k] && (str[k]==' ' || str[k]=='.' || str[k]==':' || str[k]=='<' || str[k]=='>'
           || str[k]=='-' || str[k]==0x96))
      k++;

    if (k<str.length()) {
      string sub = str.substr(k);
      i = (i + 1)%bsize;
      value[i] = str.substr(0, k) + translate(sub, found);
      return value[i];
    }
  }

  map<string, string>::const_iterator it = table.find(str);
  if (it != table.end()) {
    found = true;
    return it->second;
  }

  int subst = str.find_first_of('#');
  if (subst != str.npos) {
    string s = translate(str.substr(0, subst), found);
    vector<string> split_vec;
    split(str.substr(subst+1), "#", split_vec);
    split_vec.push_back("");
    const char *subsymb = "XYZW";
    size_t subpos = 0;
    string ret;
    size_t lastpos = 0;
    for (size_t k = 0; k<s.size(); k++) {
      if (subpos>=split_vec.size() || subpos>=4)
        break;
      if (s[k] == subsymb[subpos]) {
        if (k>0 && isalnum(s[k-1]))
          continue;
        if (k+1 < s.size() && isalnum(s[k+1]))
          continue;
        ret += s.substr(lastpos, k-lastpos);
        ret += split_vec[subpos];
        lastpos = k+1;
        subpos++;
      }
    }
    if (lastpos<s.size())
      ret += s.substr(lastpos);

    i = (i + 1)%bsize;
    swap(value[i], ret);
    return value[i];
  }


  char last = str[len-1];
  if (last != ':' && last != '.' && last != ' ' && last != ',' &&
      last != ';' && last != '<' && last != '>' && last != '-' && last != 0x96) {
#ifdef _DEBUG
    if (str.length()>1)
      unknown[str] = "";
#endif
    found = false;
    i = (i + 1)%bsize;
    value[i] = str;
    return value[i];
  }

  string suffix;
  int pos = str.find_last_not_of(last);

  while(pos>0) {
    char last = str[pos];
    if (last != ':' && last != ' ' && last != ',' && last != '.' &&
        last != ';' && last != '<' && last != '>' && last != '-' && last != 0x96)
      break;

    pos = str.find_last_not_of(last, pos);
  }

  suffix = str.substr(pos+1);

  string key = str.substr(0, str.length()-suffix.length());
  it = table.find(key);
  if (it != table.end()) {
    i = (i + 1)%bsize;
    value[i] = it->second + suffix;
    found = true;
    return value[i];
  }
#ifdef _DEBUG
  if (key.length() > 1)
    unknown[key] = "";
#endif

  found = false;
  i = (i + 1)%bsize;
  value[i] = str;
  return value[i];
}
const string newline = "\n";

void LocalizerImpl::saveUnknown(const string &file)
{
  if (!unknown.empty()) {
    ofstream fout(file.c_str(), ios::trunc|ios::out);
    for (map<string, string>::iterator it = unknown.begin(); it!=unknown.end(); ++it) {
      string value = it->second;
      string key = it->first;
      if (value.empty()) {
        value = key;

        int nl = value.find(newline);
        int n2 = value.find(".");

        if (nl!=string::npos || n2!=string::npos) {
          while (nl!=string::npos) {
            value.replace(nl, newline.length(), "\\n");
            nl = value.find(newline);
          }
          key = "help:" + itos(value.length()) + itos(value.find_first_of("."));
        }
      }
      fout << key << " = " << value << endl;
    }
  }
}


const oWordList &LocalizerImpl::getGivenNames() const {
  if (givenNames == 0) {
    char bf[260];
    getUserFile(bf, "given.mwd");
    givenNames = new oWordList();
    try {
      givenNames->load(bf);
    } catch(std::exception &) {}
  }
  return *givenNames;
}

#ifndef MEOSDB

void Localizer::LocalizerInternal::loadLangResource(const string &name) {
  map<string,string>::iterator it = langResource.find(name);
  if (it == langResource.end())
    throw std::exception("Unknown language");

  string &res = it->second;

  int i = atoi(res.c_str());
  if (i > 0)
    impl->loadTable(i, name);
  else
    impl->loadTable(res, name);
}

void Localizer::LocalizerInternal::addLangResource(const string &name, const string &resource) {
  langResource[name] = resource;
  if (implBase == 0) {
    implBase = new LocalizerImpl();
    implBase->loadTable(atoi(resource.c_str()), name);
  }
}

void Localizer::LocalizerInternal::debugDump(const string &untranslated, const string &translated) const {
  if (implBase) {
    impl->translateAll(*implBase);
  }
  impl->saveUnknown(untranslated);
  impl->saveTable(translated);
}

void LocalizerImpl::translateAll(const LocalizerImpl &all) {
  map<string, string>::const_iterator it;
  bool f;
  for (it = all.table.begin(); it != all.table.end(); ++it) {
    translate(it->first, f);
    if (!f) {
      unknown[it->first] = it->second;
    }
  }
}

void LocalizerImpl::saveTable(const string &file)
{
  ofstream fout((language+"_"+file).c_str(), ios::trunc|ios::out);
  for (map<string, string>::iterator it = table.begin(); it!=table.end(); ++it) {
    string value = it->second;
    int nl = value.find(newline);
    while (nl!=string::npos) {
      value.replace(nl, newline.length(), "\\n");
      nl = value.find(newline);
    }
    fout << it->first << " = " << value << endl;
  }
}

void LocalizerImpl::loadTable(int id, const string &language)
{
  string sname = "#"+itos(id);
  const char *name = sname.c_str();
  HRSRC hResInfo = FindResource(0, name, "#300");
  HGLOBAL hGlobal = LoadResource(0, hResInfo);

  if (hGlobal==0)
    throw std::exception("Resource not found");

  int size = SizeofResource(0, hResInfo);

  const char *lang_src = (char *)LockResource(hGlobal);
  char *lang = new char[size];
  memcpy(lang, lang_src, size);
  char *bf;
  vector<string> raw;
  int pos = 0;
  while (pos < size) {
    bf = &lang[pos];
    while(pos<size && lang[pos] != '\n' && lang[pos] != '\r')
      pos++;

    if (lang[pos]=='\n' || lang[pos]=='\r') {
      lang[pos] = 0;
      if (strlen(bf)>0 && bf[0] != '#')
        raw.push_back(bf);
      pos++;
      if (pos<size && (lang[pos]=='\n' || lang[pos]=='\r'))
        pos++;
    }
  }

  delete[] lang;
  loadTable(raw, language);
}

void LocalizerImpl::loadTable(const string &file, const string &language)
{
  clear();
  ifstream fin(file.c_str(), ios::in);

  if (!fin.good())
    return;

  int line=0;
  char bf[8*1024];
  while (!fin.eof()) {
    line++;
    fin.getline(bf, 8*1024);
  }

  fin.seekg(0);
  fin.clear();
  fin.seekg(0);

  vector<string> raw;
  raw.reserve(line);
  while (!fin.eof()) {
    bf[0] = 0;
    fin.getline(bf, 8*1024);
    if (bf[0]!=0 && bf[0]!='#')
      raw.push_back(bf);
  }

  loadTable(raw, language);
}


void LocalizerImpl::loadTable(const vector<string> &raw, const string &language)
{
  vector<int> order(raw.size());
  for (size_t k = 0; k<raw.size(); k++)
    order[k] = k;

  // Optimize insertation order
  permute(order);

  table.clear();
  this->language = language;
  for (size_t k=0;k<raw.size();k++) {
    const string &s = raw[order[k]];
    int pos = s.find_first_of('=');

    if (pos==string::npos)
      throw std::exception("Bad file format.");
    int spos = pos;
    int epos = pos+1;
    while (spos>0 && s[spos-1]==' ')
      spos--;

    while (unsigned(epos)<s.size() && s[epos]==' ')
      epos++;

    string key = s.substr(0, spos);
    string value = s.substr(epos);

    int nl = value.find("\\n");
    while (nl!=string::npos) {
      value.replace(nl, 2, newline);
      nl = value.find("\\n");
    }

    table[key] = value;
  }
}

#endif

void LocalizerImpl::clear()
{
  table.clear();
  unknown.clear();
  language.clear();
}

bool Localizer::capitalizeWords() const {
  return tl("Lyssna") == "Listen";
}

