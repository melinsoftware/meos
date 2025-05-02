#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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

#include "tabbase.h"
#include "gdiconstants.h"
#include "oFreeImport.h"
#include "importformats.h"

class PrefsEditor;

class TabCompetition :
  public TabBase
{
  wstring eventorBase;
  wstring iofExportVersion;
  void textSizeControl(gdioutput &gdi) const;

  bool showConnectionPage;
  bool importFile(HWND hWnd, gdioutput &gdi);
  bool exportFileAs(HWND hWnd, gdioutput &gdi);

  bool save(gdioutput &gdi, bool write = true);

  void loadRunnerDB(gdioutput &gdi, int tableToShow, bool updateTableOnly);

  // Events from Eventor
  vector<CompetitionInfo> events;
  shared_ptr<PrefsEditor> prefsEditor;

  oFreeImport fi;
  wstring entryText;
  vector<oEntryBlock> entries;
  void loadConnectionPage(gdioutput &gdi);

  wstring defaultServer;
  wstring defaultName;
  wstring defaultPwd;
  wstring defaultPort;

  void copyrightLine(gdioutput &gdi) const;
  void loadAboutPage(gdioutput &gdi) const;

  void updateWarning(gdioutput &gdi) const;

  int organizorId;

  int lastChangeClassType;

  struct {
    wstring name;
    wstring careOf;
    wstring street;
    wstring city;
    wstring zipCode;
    wstring account;
    wstring email;
  } eventor;

  int getOrganizer(bool updateEvent);
  void getAPIKey(vector< pair<wstring, wstring> > &key) const;
  void getEventorCompetitions(gdioutput &gdi,
                              const wstring &fromDate,
                              vector<CompetitionInfo> &events) const;

  void saveSettings(gdioutput &gdi);
  
  void getEventorCmpData(gdioutput &gdi, int id,
                         const wstring &eventFile,
                         const wstring &clubFile,
                         const wstring &classFile,
                         const wstring &entryFile,
                         const wstring &dbFile) const;

  void loadMultiEvent(gdioutput &gdi);
  void saveMultiEvent(gdioutput &gdi);

  string eventorOrigin; // The command used when checking eventor
  bool checkEventor(gdioutput &gdi, ButtonInfo &bi);

  pair<bool, bool> hasPersonExtId() const;

  bool useEventor() const;
  bool useEventorUTC() const;

  void openCompetition(gdioutput &gdi, int id);
  void selectTransferClasses(gdioutput &gdi, bool expand);

  // Welcome page for new users
  void welcomeToMeOS(gdioutput &gdi);

  // Class id for last selected class for entry
  int lastSelectedClass;

  set<int> allTransfer;

  void checkReadyForResultExport(gdioutput &gdi,  const set<int> &classFilter, bool checkVacant);

  void displayRunners(gdioutput &gdi, const vector<pRunner> &changedClass) const;

  void meosFeatures(gdioutput &gdi, bool newGuide);

  void newCompetitionGuide(gdioutput &gdi, int step);
  void createNewCmp(gdioutput &gdi, bool useExisting);

  void entryForm(gdioutput &gdi, bool isGuide);
  FlowOperation saveEntries(gdioutput &gdi, bool removeRemoved, int classOffset, bool isGuide);
  
  FlowOperation checkStageFilter(gdioutput &gdi, const wstring &fname, set<int> &filter, pair<string, string> &preferredIdProvider);
  
  static void setExportOptionsStatus(gdioutput &gdi, int format);

  void selectStartlistOptions(gdioutput &gdi);
  void selectExportSplitOptions(gdioutput& gdi);

  void showSelectId(std::pair<bool, bool>& priSecondId, gdioutput& gdi);
  pair<string, string> TabCompetition::getPreferredIdTypes(gdioutput& gdi);

  void saveExtraFields(gdioutput& gdi, oEvent::ExtraFieldContext type);

  void entryChoice(gdioutput &gdi);
  void createCompetition(gdioutput &gdi);

  void importDefaultHiredCards(gdioutput& gdi);

  void listBackups(gdioutput &gdi);

  shared_ptr<GuiHandler> mergeHandler;
  void mergeCompetition(gdioutput &gdi);
  wstring mergeFile;

  wstring constructBase(const wstring &r, const wstring &mt) const;

protected:
  void clearCompetitionData();

public:

  static void selectExportSplitOptions(gdioutput& gdi, oEvent* oe, const set<int>& allTransfer, const ExportSplitsData *data);

  static void readExportSplitSettings(gdioutput& gdi, oEvent* oe, set<int>& allTransfer, ExportSplitsData &data);

  static void exportSplitsData(oEvent* oe, const wstring& save,
    const set<int>& allTransfer,
    const ExportSplitsData& data, bool openDocument);

  void loadSettings(gdioutput& gdi);
  void showExtraFields(gdioutput& gdi, oEvent::ExtraFieldContext type);

  const char * getTypeStr() const {return "TCmpTab";}
  TabType getType() const {return TCmpTab;}

  void saveMeosFeatures(gdioutput &gdi, bool write);
  void updateFeatureStatus(gdioutput &gdi);

  void setEventorServer(const wstring &server);
  void setEventorUTC(bool useUTC);

  int competitionCB(gdioutput &gdi, GuiEventType type, BaseInfo *data);
  int restoreCB(gdioutput &gdi, GuiEventType type, BaseInfo *data);
  int newGuideCB(gdioutput &gdi, GuiEventType type, BaseInfo *data);

  bool loadPage(gdioutput &gdi);
  TabCompetition(oEvent *oe);
  ~TabCompetition(void);
};
