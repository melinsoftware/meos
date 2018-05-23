#pragma once
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

#include "TabAuto.h"
#include <deque>
#include "oPunch.h"

class InfoCompetition;

class OnlineInput :
  public AutoMachine
{
protected:
  wstring url;
  int cmpId;
  wstring unitId;
  int lastImportedId;
  int importCounter;
  int bytesImported;
  DWORD lastSync;

  bool useROCProtocol;
  bool useUnitId;

  deque<wstring> info;
  map<int, oPunch::SpecialPunch> specialPunches;

  void addInfo(const wstring &line) {
    if (info.size() >= 10)
      info.pop_back();
    info.push_front(line);
  }

  void fillMappings(gdioutput &gdi) const;

  void processCards(gdioutput &gdi, oEvent &oe, const xmlList &cards);
  void processUpdates(oEvent &oe, const xmlList &updates);
  void processEntries(oEvent &oe, const xmlList &entries);

  void processPunches(oEvent &oe, const xmlList &punches);
  void processPunches(oEvent &oe, list< vector<wstring> > &rocData);
public:

  int processButton(gdioutput &gdi, ButtonInfo &bi);

  void save(oEvent &oe, gdioutput &gdi);
  void settings(gdioutput &gdi, oEvent &oe, bool created);
  OnlineInput *clone() const {return new OnlineInput(*this);}
  void status(gdioutput &gdi);
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast);
  OnlineInput() : AutoMachine("Onlineinput"), cmpId(0), importCounter(1),
                    bytesImported(0), lastSync(0), lastImportedId(0), useROCProtocol(false), useUnitId(false) {}
  ~OnlineInput();
  friend class TabAuto;
};
