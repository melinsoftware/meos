#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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
  void processTeamLineups(oEvent &oe, const xmlList &updates);
  void processEntries(oEvent &oe, const xmlList &entries);

  void processPunches(oEvent &oe, const xmlList &punches);
  void processPunches(oEvent &oe, list< vector<wstring> > &rocData);

  bool hasSaveMachine() const final {
    return true;
  }

  void saveMachine(oEvent &oe, const wstring &guiInterval) final;
  void loadMachine(oEvent &oe, const wstring &name) final;

public:

  int processButton(gdioutput &gdi, ButtonInfo &bi);

  void updateLabel(gdioutput& gdi);

  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;
  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  static void controlMappingView(gdioutput& gdi, GUICALLBACK cb, int widgetId);
  OnlineInput *clone() const {return new OnlineInput(*this);}
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  OnlineInput() : AutoMachine("Onlineinput", Machines::mOnlineInput), cmpId(0), importCounter(1),
                    bytesImported(0), lastSync(0), lastImportedId(0), useROCProtocol(false), useUnitId(false) {}
  ~OnlineInput();
  friend class TabAuto;
};
