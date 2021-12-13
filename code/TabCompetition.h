#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2021 Melin Software HB

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

class PrefsEditor;
class ImportFormats;

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
  list<PrefsEditor> prefsEditor;

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
  void loadSettings(gdioutput &gdi);

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

  bool useEventor() const;
  bool useEventorUTC() const;

  void openCompetition(gdioutput &gdi, int id);
  void selectTransferClasses(gdioutput &gdi, bool expand);

  // Welcome page for new users
  void welcomeToMeOS(gdioutput &gdi);

  // Class id for last selected class for entry
  int lastSelectedClass;

  set<int> allTransfer;


  void checkReadyForResultExport(gdioutput &gdi, const set<int> &classFilter);

  void displayRunners(gdioutput &gdi, const vector<pRunner> &changedClass) const;

  void meosFeatures(gdioutput &gdi, bool newGuide);

  void newCompetitionGuide(gdioutput &gdi, int step);

  void entryForm(gdioutput &gdi, bool isGuide);
  FlowOperation saveEntries(gdioutput &gdi, bool removeRemoved, bool isGuide);
  
  FlowOperation checkStageFilter(gdioutput &gdi, const wstring &fname, set<int> &filter, string &preferredIdProvider);
  
  void setExportOptionsStatus(gdioutput &gdi, int format) const;

  void selectStartlistOptions(gdioutput &gdi);
  void selectExportSplitOptions(gdioutput &gdi);

  void entryChoice(gdioutput &gdi);
  void createCompetition(gdioutput &gdi);

  void listBackups(gdioutput &gdi);

  shared_ptr<GuiHandler> mergeHandler;
  void mergeCompetition(gdioutput &gdi);
  wstring mergeFile;

  wstring constructBase(const wstring &r, const wstring &mt) const;

protected:
  void clearCompetitionData();

public:
  const char * getTypeStr() const {return "TCmpTab";}
  TabType getType() const {return TCmpTab;}

  void saveMeosFeatures(gdioutput &gdi, bool write);
  void updateFeatureStatus(gdioutput &gdi);

  void setEventorServer(const wstring &server);
  void setEventorUTC(bool useUTC);

  int competitionCB(gdioutput &gdi, int type, void *data);
  int restoreCB(gdioutput &gdi, int type, void *data);
  int newGuideCB(gdioutput &gdi, int type, void *data);

  bool loadPage(gdioutput &gdi);
  TabCompetition(oEvent *oe);
  ~TabCompetition(void);
};
