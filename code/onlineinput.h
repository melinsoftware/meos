#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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
#include "permission.h"

class InfoCompetition;
class xmlobject;
typedef vector<xmlobject> xmlList;

class OnlineInput :
  public AutoMachine, public GuiHandler
{
protected:
  map<int, oPunch::SpecialPunch> specialPunches;
  oEvent* settingsOE = nullptr;
  wstring url;
  wstring cmpId;
  wstring unitId;
  wstring passwd;

  EntryPermissionClass epClass = EntryPermissionClass::None;
  EntryPermissionType epType = EntryPermissionType::None;

  int lastImportedId;
  int importCounter;
  int bytesImported;
  DWORD lastSync;

  wstring errorLogFile;
  
  enum class Type {
    MIP,
    ROC,
    SICenter,
  };
  Type serverType = Type::MIP;
  bool useUnitId;

  deque<wstring> info;
  shared_ptr<InfoCompetition> mipCmp;

  InfoCompetition &getMipCmp(bool forceReset = false);

  void addInfo(const wstring &line) {
    if (info.size() >= 10)
      info.pop_back();
    info.push_front(line);
  }

  enum class MipEntryStatus {
    Failed,
    EntryOK,
    UpdatedOK,
  };

  struct MipEntryInfo {
    int id = 0;
    int meosId = 0;
    wstring statusMessage;
    MipEntryStatus status = MipEntryStatus::Failed;

    MipEntryInfo() = default;
    MipEntryInfo(int id, int meosId, MipEntryStatus status, const wstring &msg) :
      id(id), meosId(meosId), status(status), statusMessage(msg) {}
  };

  void fillMappings(oEvent& oe, gdioutput &gdi);
  void processMIP(oEvent &ie, const wstring &inputFile, wstring &responePost);

  void processCards(oEvent &oe, const xmlList &cards);
  void processTeamLineups(oEvent &oe, const xmlList &updates);
  void processEntries(oEvent &oe, const xmlList &entries, vector<MipEntryInfo> &status);

  void processPunches(oEvent &oe, const xmlList &punches);
  void processPunches(oEvent &oe, list< vector<wstring> > &rocData);
  void processPunchesSICenter(oEvent& oe, const wstring& filename);

  bool hasSaveMachine() const final {
    return true;
  }

  void saveMachine(oEvent &oe, const wstring &guiInterval) final;
  void loadMachine(oEvent &oe, const wstring &name) final;

  time_t getZeroTimeMSLinuxEpoch(const oEvent &oe) const;
  int mapPunch(int code) const;

  void updateEntryStatus(gdioutput &gdi);

public:
  static wstring sanitizeId(const wstring &in);

  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) override;

  int processButton(gdioutput &gdi, ButtonInfo &bi);
  int processListBox(gdioutput& gdi, ListBoxInfo& bi);
  int processLink(gdioutput &gdi, TextInfo &bi);

  void updateLabel(gdioutput& gdi);

  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;
  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  static void controlMappingView(gdioutput& gdi, oEvent *oe, GUICALLBACK cb, int widgetId);
  shared_ptr<AutoMachine> clone() const final { 
    return make_shared<OnlineInput>(*this);
  }
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  OnlineInput() : AutoMachine("Onlineinput", Machines::mOnlineInput), cmpId(L"0"), importCounter(1),
                    bytesImported(0), lastSync(0), lastImportedId(0), useUnitId(false) {}
  ~OnlineInput() = default;
  friend class TabAuto;
};
