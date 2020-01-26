#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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
#include <map>
#include <set>
#include <string>
#include <vector>

class LocalizerImpl;
class oWordList;

class Localizer {
  class LocalizerInternal {
  private:
    map<wstring, wstring> langResource;
    LocalizerImpl *impl;
    LocalizerImpl *implBase;

    bool owning;
    LocalizerInternal *user;

  public:

    void debugDump(const wstring &untranslated, const wstring &translated) const;

    vector<wstring> getLangResource() const;
    void loadLangResource(const wstring &name);
    void addLangResource(const wstring &name, const wstring &resource);

    /** Translate string */
    const wstring &tl(const wstring &str) const;

    void set(Localizer &li);

    /** Get database with given names */
    const oWordList &getGivenNames() const;

    LocalizerInternal();
    ~LocalizerInternal();
  };
private:
  LocalizerInternal *linternal;

public:
  bool capitalizeWords() const;

  LocalizerInternal &get() {return *linternal;}
  const wstring &tl(const string &str) const;
  const wstring &tl(const wstring &str) const {return linternal->tl(str);}
  
  const wstring tl(const wstring &str, bool cap) const;

  void init() {linternal = new LocalizerInternal();}
  void unload() {delete linternal; linternal = 0;}

  Localizer() : linternal(0) {}
  ~Localizer() {unload();}
};

extern Localizer lang;
