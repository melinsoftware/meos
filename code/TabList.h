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
#include "oListInfo.h"

class LiveResult;
class ListEditor;
class MethodEditor;

class TabList :
  public TabBase
{
protected:
  EStdListType currentListType;
  oListInfo currentList;
  string SelectedList;
  wstring lastInputNumber;
  int lastLimitPer;
  bool lastInterResult;
  bool lastSplitState;
  bool lastLargeSize;
  
  
  EStdListType getTypeFromResultIndex(int ix) const;

  int infoCX;
  int infoCY;

  static void createListButtons(gdioutput &gdi);

  void generateList(gdioutput &gdi, bool forceUpdate = false);
  void selectGeneralList(gdioutput &gdi, EStdListType type);

  int offsetY;
  int offsetX;
  set<int> lastClassSelection;
  vector<LiveResult *> liveResults;

  int lastSelectedResultList;
  set<int> lastResultClassSelection;
  int lastLeg;
  int lastFilledResultClassType;
  
  void setResultOptionsFromType(gdioutput &gdi, int data);


  bool hideButtons;
  bool ownWindow;
  ListEditor *listEditor;
  MethodEditor *methodEditor;

  bool noReEvaluate;
  
  int baseButtons(gdioutput &gdi, int extraButtons);

private:
  // Not supported, copy works not.
  TabList(const TabList &);
  const TabList &operator = (const TabList &);
  
  string settingsTarget;
  oListParam tmpSettingsParam;
  void changeListSettingsTarget(gdioutput &oldWindow, gdioutput &newWindow);
  void leavingList(const string &wnd);

  pair<gdioutput *, TabList *> makeOwnWindow(gdioutput &gdi);

  /** Set animation mode*/
  void setAnimationMode(gdioutput &gdi);

  static void getStartIndividual(oListParam &par, ClassConfigInfo &cnf);
  static void getStartClub(oListParam &par);
  static void getResultIndividual(oListParam &par, ClassConfigInfo &cnf);
  static void getResultClub(oListParam &par, ClassConfigInfo &cnf);

  static void getStartPatrol(oListParam &par, ClassConfigInfo &cnf);
  static void getResultPatrol(oListParam &par, ClassConfigInfo &cnf);

  static void getStartTeam(oListParam &par, ClassConfigInfo &cnf);
  static void getResultTeam(oListParam &par, ClassConfigInfo &cnf);

  static void getResultRogaining(oListParam &par, ClassConfigInfo &cnf);


public:
  /** Returns a collection of public lists. */
  void static getPublicLists(oEvent &oe, vector<oListParam> &lists);

  bool loadPage(gdioutput &gdi);
  bool loadPage(gdioutput &gdi, const string &command);
  
  // Clear up competition specific settings
  void clearCompetitionData();
  static void makeClassSelection(gdioutput &gdi);
  static void makeFromTo(gdioutput &gdi);
  static void enableFromTo(oEvent &oe, gdioutput &gdi, bool from, bool to);
  void liveResult(gdioutput &gdi, oListInfo &currentList);

  int listCB(gdioutput &gdi, int type, void *data);
  void loadGeneralList(gdioutput &gdi);
  void rebuildList(gdioutput &gdi);
  void settingsResultList(gdioutput &gdi);

  void loadSettings(gdioutput &gdi, string targetTag);
  void handleListSettings(gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi);
  enum PrintSettingsSelection {
    Splits = 0,
    StartInfo = 1,
  };

  static void splitPrintSettings(oEvent &oe, gdioutput &gdi, bool setupPrinter, TabType returnMode, PrintSettingsSelection type);
  static void customTextLines(oEvent &oe, const char *dataField, gdioutput &gdi);
  static void saveExtraLines(oEvent &oe, const char *dataField, gdioutput &gdi);
  static void enableWideFormat(gdioutput &gdi, bool wide);

  ListEditor *getListeditor() const {return listEditor;}

  const char * getTypeStr() const {return "TListTab";}
  TabType getType() const {return TListTab;}

  TabList(oEvent *oe);
  ~TabList(void);
  friend int ListsEventCB(gdioutput *gdi, int type, void *data);
};
