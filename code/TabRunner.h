#pragma once
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
#include "tabbase.h"
#include "Printer.h"
#include "autocompletehandler.h"

class Table;

class TabRunner :
  public TabBase, AutoCompleteHandler
{
private:
  void addToolbar(gdioutput &gdi);

  const wstring &getSearchString() const;

  void setCardNo(gdioutput &gdi, int cardNo);

  void enableControlButtons(gdioutput &gdi, bool enable, bool vacant);

  void cellAction(gdioutput &gdi, DWORD id, oBase *obj);

  void selectRunner(gdioutput &gdi, pRunner r);

  wstring lastSearchExpr;
  unordered_set<int> lastFilter;
  DWORD timeToFill;
  int inputId;
  int searchCB(gdioutput &gdi, int type, void *data);

  int runnerCB(gdioutput &gdi, int type, void *data);
  int punchesCB(gdioutput &gdi, int type, void *data);
  int vacancyCB(gdioutput &gdi, int type, void *data);

  int currentMode;
  pRunner save(gdioutput &gdi, int runnerId, bool dontReloadRunners);
  void listRunners(gdioutput &gdi, const vector<pRunner> &r, bool filterVacant) const;

  void fillRunnerList(gdioutput &gdi);
  int cardModeStartY;
  int lastRace;
  wstring lastFee;
  int runnerId;
  bool ownWindow;
  bool listenToPunches;
  vector< pair<int, bool> > runnersToReport;

  vector<pRunner> unknown_dns;
  vector<pRunner> known_dns;
  vector<pRunner> known;
  vector<pRunner> unknown;
  void clearInForestData();
  bool savePunchTime(pRunner r, gdioutput &gdi);

  PrinterObject splitPrinter;

  void showRunnerReport(gdioutput &gdi);
  static void runnerReport(oEvent &oe, gdioutput &gdi, int id, bool compactReport);

  void showVacancyList(gdioutput &gdi, const string &method="", int classId=0);
  void showCardsList(gdioutput &gdi);

  bool canSetStart(pRunner r) const;
  bool canSetFinish(pRunner r) const;

  void warnDuplicateCard(gdioutput &gdi, int cno, pRunner r);
  pRunner warnDuplicateCard(int cno, pRunner r);

  int numShorteningLevels() const;

  void updateNumShort(gdioutput &gdi, pCourse crs, pRunner r);
  static void updateStatus(gdioutput &gdi, pRunner r);
  static void autoGrowCourse(gdioutput &gdi);

  void loadEconomy(gdioutput &gdi, oRunner &r);

  class EconomyHandler : public GuiHandler {
    int runnerId;
    oEvent *oe;
    oRunner &getRunner() const;
  public:
    void init(oRunner &r);
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    void save(gdioutput &gdi);
  };

  shared_ptr<EconomyHandler> ecoHandler;
  EconomyHandler *getEconomyHandler(oRunner &r);

protected:
  void clearCompetitionData();
public:
  static pClub extractClub(oEvent *oe, gdioutput &gdi);

  void handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) override;

  const char * getTypeStr() const {return "TRunnerTab";}
  TabType getType() const {return TRunnerTab;}

  void showInForestList(gdioutput &gdi);

  bool loadPage(gdioutput &gdi);
  bool loadPage(gdioutput &gdi, int runnerId);

  static void generateRunnerReport(oEvent &oe, gdioutput &gdi,  vector<pair<int, bool>> &runnersToReport);

  TabRunner(oEvent *oe);
  ~TabRunner(void);

  friend int runnerSearchCB(gdioutput *gdi, int type, void *data);
  friend int RunnerCB(gdioutput *gdi, int type, void *data);
  friend int PunchesCB(gdioutput *gdi, int type, void *data);
  friend int VacancyCB(gdioutput *gdi, int type, void *data);
};
