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

#include "tabbase.h"
#include "oEventDraw.h"

class TabClass :
  public TabBase
{
  struct PursuitSettings {
    bool use;
    int firstTime;
    int maxTime;

    PursuitSettings(oClass &c) {
      firstTime = 3600;
      use = c.interpretClassType() != ctOpen;
      maxTime = 3600;
    }
  };

  map<int, PursuitSettings> pSettings;
  int pSavedDepth;
  int pFirstRestart;
  double pTimeScaling;
  int pInterval;


  class HandleCloseWindow : public GuiHandler {
    TabClass *tabClass;
    HandleCloseWindow(const HandleCloseWindow&);
    HandleCloseWindow &operator=(const HandleCloseWindow&);
  public:
    HandleCloseWindow() : tabClass(0) {}
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    friend class TabClass;
  };
  HandleCloseWindow handleCloseWindow;


  bool EditChanged;
  int ClassId;
  int currentStage;
  wstring storedNStage;
  wstring storedStart;
  oEvent::PredefinedTypes storedPredefined;
  bool showForkingGuide;

  bool checkClassSelected(const gdioutput &gdi) const;
  void save(gdioutput &gdi, bool skipReload);
  void legSetup(gdioutput &gdi);
  vector<ClassInfo> cInfo;
  void saveDrawSettings() const;
  
  map<int, ClassInfo> cInfoCache;

  DrawInfo drawInfo;
  void setMultiDayClass(gdioutput &gdi, bool hasMulti, DrawMethod defaultMethod);
  set<DrawMethod> getSupportedDrawMethods(bool multiDay) const;

  void drawDialog(gdioutput &gdi, DrawMethod method, const oClass &cls);

  void pursuitDialog(gdioutput &gdi);

  bool warnDrawStartTime(gdioutput &gdi, int time);
  bool warnDrawStartTime(gdioutput &gdi, const wstring &firstStart);

  void static clearPage(gdioutput &gdi, bool autoRefresh);

  bool hasWarnedStartTime;
  bool hasWarnedDirect;
  bool tableMode;
  DrawMethod lastDrawMethod;
  int lastSeedMethod;
  bool lastSeedPreventClubNb;
  bool lastSeedReverse;
  wstring lastSeedGroups;
  int lastPairSize;
  wstring lastFirstStart;
  wstring lastInterval;
  wstring lastNumVac;
  wstring lastScaleFactor;
  wstring lastMaxAfter;

  bool lastHandleBibs;
  // Generate a table with class settings
  void showClassSettings(gdioutput &gdi);

  void visualizeField(gdioutput &gdi);

  // Read input from the table with class settings
  void readClassSettings(gdioutput &gdi);

  // Prepare for drawing by declaring starts and blocks
  void prepareForDrawing(gdioutput &gdi);

  void showClassSelection(gdioutput &gdi, int &bx, int &by, GUICALLBACK classesCB) const;

  // Set simultaneous start in a class
  void simultaneous(int classId, const wstring &time);

  void updateFairForking(gdioutput &gdi, pClass pc) const;
  void selectCourses(gdioutput &gdi, int legNo);
  bool showMulti(bool singleOnly) const;

  void defineForking(gdioutput &gdi, bool clearSettings);
  vector< vector<int> > forkingSetup;
  static const wchar_t *getCourseLabel(bool pool);

  void getClassSettingsTable(gdioutput &gdi, GUICALLBACK cb);
  void saveClassSettingsTable(gdioutput &gdi, set<int> &classModifiedFee, bool &modifiedBib);

  static wstring getBibCode(AutoBibType bt, const wstring &key);

  void setParallelOptions(const string &sdKey, gdioutput &gdi, pClass pc, int  legno);
  
  void updateStartData(gdioutput &gdi, pClass pc, int leg, bool updateDependent, bool forceWrite);

  void updateSplitDistribution(gdioutput &gdi, int numInClass, int tot) const;

  DrawMethod getDefaultMethod(const set<DrawMethod> &allowedValues) const;

  void enableLoadSettings(gdioutput &gdi);

  void readDrawInfo(gdioutput &gdi, DrawInfo &drawInfo);
  void writeDrawInfo(gdioutput &gdi, const DrawInfo &drawInfo);

  static vector< pair<wstring, size_t> > getPairOptions();

  void setLockForkingState(gdioutput &gdi, bool poolState, bool lockState);

  void loadBasicDrawSetup(gdioutput &gdi, int &bx, int &by, const wstring& firstStart, 
                          int maxNumControl, const wstring& minInterval, const wstring& vacances, const set<int> &clsId);

  void loadReadyToDistribute(gdioutput &gdi, int &bx, int &by);
public:
  
  void clearCompetitionData();

  void closeWindow(gdioutput &gdi);

  void saveClassSettingsTable(gdioutput &gdi);
  void multiCourse(gdioutput &gdi, int nLeg);
  bool loadPage(gdioutput &gdi);
  void selectClass(gdioutput &gdi, int cid);

  int classCB(gdioutput &gdi, int type, void *data);
  int multiCB(gdioutput &gdi, int type, void *data);

  const char * getTypeStr() const {return "TClassTab";}
  TabType getType() const {return TClassTab;}

  friend int DrawClassesCB(gdioutput *gdi, int type, void *data);

  TabClass(oEvent *oe);
  ~TabClass(void);
};
