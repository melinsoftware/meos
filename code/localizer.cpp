/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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
#include "random.h"
#include "oFreeImport.h"
#include "meos_util.h"

const string &toUTF8(const wstring &winput) {
  string &output = StringCache::getInstance().get();
  size_t alloc = winput.length()*2;
  output.resize(alloc);
  WideCharToMultiByte(CP_UTF8, 0, winput.c_str(), winput.length()+1, (char *)output.c_str(), alloc, 0, 0);
  output.resize(strlen(output.c_str()));
  return output;
}

const wstring &fromUTF(const string &input) {
  wstring &output = StringCache::getInstance().wget();
  output.resize(input.length()+2, 0);
  int res = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &output[0], output.size() * sizeof(wchar_t));
  output.resize(res-1);
  return output;
}

class LocalizerImpl
{
  wstring language;
  map<wstring, wstring> table;
  map<wstring, wstring> unknown;
  void loadTable(const vector<string> &raw, const wstring &language);
  mutable oWordList *givenNames;

public:

  const oWordList &getGivenNames() const;

  void translateAll(const LocalizerImpl &all);

  const wstring &translate(const wstring &str, bool &found);

  void saveUnknown(const wstring &file);
  void saveTable(const wstring &file);
  void loadTable(const wstring &file, const wstring &language);
  void loadTable(int resource, const wstring &language);

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

vector<wstring> Localizer::LocalizerInternal::getLangResource() const {
  vector<wstring> v;
  for (map<wstring, wstring>::const_iterator it = langResource.begin(); it !=langResource.end(); ++it)
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

const wstring &Localizer::LocalizerInternal::tl(const wstring &str) const {
  bool found;
  const wstring *ret = &impl->translate(str, found);
  if (found || !implBase)
    return *ret;

  ret = &implBase->translate(str, found);
  return *ret;
}

const wstring &LocalizerImpl::translate(const wstring &str, bool &found)
{
  found = false;
  static int i = 0;
  const int bsize = 17;
  static wstring value[bsize];
  int len = str.length();

  if (len==0)
    return _EmptyWString;

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
      wstring sub = str.substr(k);
      i = (i + 1)%bsize;
      value[i] = str.substr(0, k) + translate(sub, found);
      return value[i];
    }
  }

  map<wstring, wstring>::const_iterator it = table.find(str);
  if (it != table.end()) {
    found = true;
    return it->second;
  }

  int subst = str.find_first_of('#');
  if (subst != str.npos) {
    wstring s = translate(str.substr(0, subst), found);
    vector<wstring> split_vec;
    split(str.substr(subst+1), L"#", split_vec);
    split_vec.push_back(L"");
    const wchar_t *subsymb = L"XYZW";
    size_t subpos = 0;
    wstring ret;
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


  wchar_t last = str[len-1];
  if (last != ':' && last != '.' && last != ' ' && last != ',' &&
      last != ';' && last != '<' && last != '>' && last != '-' && last != 0x96) {
#ifdef _DEBUG
    if (str.length()>1)
      unknown[str] = L"";
#endif
    found = false;
    i = (i + 1)%bsize;
    value[i] = str;
    return value[i];
  }

  wstring suffix;
  int pos = str.find_last_not_of(last);

  while(pos>0) {
    wchar_t last = str[pos];
    if (last != ':' && last != ' ' && last != ',' && last != '.' &&
        last != ';' && last != '<' && last != '>' && last != '-' && last != 0x96)
      break;

    pos = str.find_last_not_of(last, pos);
  }

  suffix = str.substr(pos+1);

  wstring key = str.substr(0, str.length()-suffix.length());
  it = table.find(key);
  if (it != table.end()) {
    i = (i + 1)%bsize;
    value[i] = it->second + suffix;
    found = true;
    return value[i];
  }
#ifdef _DEBUG
  if (key.length() > 1 && _wtoi(key.c_str()) == 0)
    unknown[key] = L"";
#endif

  found = false;
  i = (i + 1)%bsize;
  value[i] = str;
  return value[i];
}
const wstring newline = L"\n";

void LocalizerImpl::saveUnknown(const wstring &file)
{
  if (!unknown.empty()) {
    ofstream fout(file.c_str(), ios::trunc|ios::out);
    for (map<wstring, wstring>::iterator it = unknown.begin(); it!=unknown.end(); ++it) {
      wstring value = it->second;
      wstring key = it->first;
      if (value.empty()) {
        value = key;

        int nl = value.find(newline);
        int n2 = value.find(L".");

        if (nl!=string::npos || n2!=string::npos) {
          while (nl!=string::npos) {
            value.replace(nl, newline.length(), L"\\n");
            nl = value.find(newline);
          }
          key = L"help:" + itow(value.length()) + itow(value.find_first_of('.'));
        }
      }
      fout << toUTF8(key) << " = " << toUTF8(value) << endl;
    }
  }
}


const oWordList &LocalizerImpl::getGivenNames() const {
  if (givenNames == 0) {
    wchar_t bf[260];
    getUserFile(bf, L"wgiven.mwd");
    givenNames = new oWordList();
    try {
      givenNames->load(bf);
    } catch(std::exception &) {}
  }
  return *givenNames;
}

#ifndef MEOSDB

void Localizer::LocalizerInternal::loadLangResource(const wstring &name) {
  map<wstring,wstring>::iterator it = langResource.find(name);
  if (it == langResource.end())
    throw std::exception("Unknown language");

  wstring &res = it->second;

  int i = _wtoi(res.c_str());
  if (i > 0)
    impl->loadTable(i, name);
  else
    impl->loadTable(res, name);
}

void Localizer::LocalizerInternal::addLangResource(const wstring &name, const wstring &resource) {
  langResource[name] = resource;
  if (implBase == 0) {
    implBase = new LocalizerImpl();
    implBase->loadTable(_wtoi(resource.c_str()), name);
  }
}

void Localizer::LocalizerInternal::debugDump(const wstring &untranslated, const wstring &translated) const {
  if (implBase) {
    impl->translateAll(*implBase);
  }
  impl->saveUnknown(untranslated);
  impl->saveTable(translated);
}

void LocalizerImpl::translateAll(const LocalizerImpl &all) {
  map<wstring, wstring>::const_iterator it;
  bool f;
  for (it = all.table.begin(); it != all.table.end(); ++it) {
    translate(it->first, f);
    if (!f) {
      unknown[it->first] = it->second;
    }
  }
}

void LocalizerImpl::saveTable(const wstring &file)
{
  ofstream fout(language+L"_"+file, ios::trunc|ios::out);
  for (map<wstring, wstring>::iterator it = table.begin(); it!=table.end(); ++it) {
    wstring value = it->second;
    int nl = value.find(newline);
    while (nl!=string::npos) {
      value.replace(nl, newline.length(), L"\\n");
      nl = value.find(newline);
    }
    fout << toUTF8(it->first) << " = " << toUTF8(value) << endl;
  }
}

void LocalizerImpl::loadTable(int id, const wstring &language)
{
  wstring sname = L"#"+itow(id);
  const wchar_t *name = sname.c_str();
  HRSRC hResInfo = FindResource(0, name, L"#300");
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

void LocalizerImpl::loadTable(const wstring &file, const wstring &language)
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


void LocalizerImpl::loadTable(const vector<string> &raw, const wstring &language)
{
  vector<int> order(raw.size());
  for (size_t k = 0; k<raw.size(); k++)
    order[k] = k;

  // Optimize insertation order
  permute(order);

  table.clear();
  this->language = language;
  string nline = "\n";
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
      value.replace(nl, 2, nline);
      nl = value.find("\\n");
    }

    static int translate = 0;

    if (translate) {
      wstring output, okey;
      output.reserve(value.size()+1);
      output.resize(value.size(), 0);
      MultiByteToWideChar(1251, MB_PRECOMPOSED, value.c_str(), value.size(), &output[0], output.size() * sizeof(wchar_t));
   
      okey.reserve(key.size()+1);
      okey.resize(key.size(), 0);
      MultiByteToWideChar(1252, MB_PRECOMPOSED, key.c_str(), key.size(), &okey[0], okey.size() * sizeof(wchar_t));

      table[okey] = output;
    }
    else
      table[fromUTF(key)] = fromUTF(value);
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
  return tl("Lyssna") == L"Listen";
}

const wstring &Localizer::tl(const string &str) const {
  if (str.length() == 0)
    return _EmptyWString;
  wstring key(str.begin(), str.end());
  for (size_t k = 0; k < key.size(); k++) {
    key[k] = 0xFF&key[k];
  }
  return linternal->tl(key);
}


const wstring Localizer::tl(const wstring &str, bool cap) const {
  wstring w = linternal->tl(str);
  if (capitalizeWords())
    ::capitalizeWords(w);

  return w;
}
