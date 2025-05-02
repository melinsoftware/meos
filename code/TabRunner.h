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
#include "Printer.h"
#include "autocompletehandler.h"
#include <deque>

class Table;
struct AutoCompleteRecord;
class RankScoreFormatter;

class TabRunner :
  public TabBase, AutoCompleteHandler
{
private:
  void addToolbar(gdioutput &gdi);

  const wstring &getSearchString() const;

  void setCardNo(gdioutput &gdi, int cardNo);

  void enableControlButtons(gdioutput &gdi, bool enable, bool vacant);

  void cellAction(gdioutput &gdi, DWORD id, oBase *obj);

  void showCardDetails(gdioutput &gdi, pCard c, bool autoSelect);

  void selectRunner(gdioutput &gdi, pRunner r);

  void updateCardStatus(const pCard& pc, gdioutput& gdi);
  void updatePunchStatus(const pPunch& punch, InputInfo* bb, gdioutput& gdi);

  int numReportRow = 1;
  int numReportColumn = 1;
  bool hideReportControls = false;
  bool showReportHeader = true;
  void addToReport(int cardNo, bool punchForShowReport);
  void addToReport(int id);
  
  string computeKeyForReport();
  
  wstring lastSearchExpr;
  unordered_set<int> lastFilter;
  DWORD timeToFill;
  int inputId;


  pair<wstring, int> getClassCourseDescription(pClass cls, int leg, pClass virtCls, pCourse crs, int crsId);

  int searchCB(gdioutput &gdi, GuiEventType type, BaseInfo* data);
  int runnerCB(gdioutput &gdi, GuiEventType type, BaseInfo* data);
  int punchesCB(gdioutput &gdi, GuiEventType type, BaseInfo* data);
  int vacancyCB(gdioutput &gdi, GuiEventType type, BaseInfo* data);

  enum class Mode {
    Form,
    Table,
    InForest,
    Cards,
    Vacancy,
    Report,
  };

  Mode currentMode;
  pRunner save(gdioutput &gdi, int runnerId, bool dontReloadRunners);
  void listRunners(gdioutput &gdi, const vector<pRunner> &r, bool filterVacant) const;

  void fillRunnerList(gdioutput &gdi);
  int cardModeStartY;
  int lastRace;
  wstring lastFee;
  int runnerId;
  bool ownWindow;
  bool listenToPunches;
  deque<pair<int, bool>> runnersToReport;

  vector<pRunner> unknown_dns;
  vector<pRunner> known_dns;
  vector<pRunner> known;
  vector<pRunner> unknown;
  void clearInForestData();
  bool savePunchTime(pRunner r, gdioutput &gdi);

  PrinterObject splitPrinter;
  static shared_ptr<RankScoreFormatter> rankFormatter;

  void showRunnerReport(gdioutput &gdi);

  
  static void runnerReport(oEvent &oe, gdioutput &gdi, 
                           int id, bool compactReport, 
                           int maxWidth, 
                           RECT& rc);

  static void teamReport(oEvent& oe, gdioutput& gdi,
                         const oTeam *team,
                         bool onlySelectedRunner,
                         const deque<pair<int, bool>> &runners,
                         int maxWidth,
                         RECT &rc);


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

  void loadEconomy(gdioutput &gdi, oRunner &r, gdioutput *gdiMain, TabRunner *mainTab);
  
  class EconomyHandler : public GuiHandler {
    int runnerId;
    oEvent *oe;
    gdioutput* gdiMain;
    TabRunner* mainTab;
    oRunner &getRunner() const;
    void updateColor(gdioutput& gdi);
    void init(oRunner& r);
  public:
    EconomyHandler(oRunner& r, gdioutput* gdiMain, TabRunner* mainTab) :
       gdiMain(gdiMain), mainTab(mainTab) { init(r); }
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    void save(gdioutput &gdi);
  };
  
  void getAutoCompleteUnpairedCards(gdioutput &gdi, const wstring& w, vector<AutoCompleteRecord>& records);

protected:
  void clearCompetitionData();
public:

  static void autoCompleteRunner(gdioutput& gdi, const RunnerWDBEntry* r);

  class CommentHandler : public GuiHandler {
    int runnerId;
    bool isTeam = false;
  protected:
    oAbstractRunner& getRunner() const;
    oEvent* oe;
    void doSave(gdioutput& gdi);
  public:
    CommentHandler(oAbstractRunner& r) : oe(r.getEvent()) {
      runnerId = r.getId(); isTeam = r.isTeam();
    }
    void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type);
    virtual void save(gdioutput& gdi);
  };

  static void renderComments(gdioutput& gdi, oAbstractRunner& r, bool newColumn, bool refresh);
  static void loadComments(gdioutput& gdi, oAbstractRunner& r, const shared_ptr<CommentHandler> &handler);

  static pClub extractClub(oEvent *oe, gdioutput &gdi);

  void handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) override;

  const char * getTypeStr() const {return "TRunnerTab";}
  TabType getType() const {return TRunnerTab;}

  void showInForestList(gdioutput &gdi);

  bool loadPage(gdioutput &gdi);
  bool loadPage(gdioutput &gdi, int runnerId);

  static int addExtraFields(const oEvent &oe, gdioutput& gdi, oEvent::ExtraFieldContext context);
  static void saveExtraFields(gdioutput& gdi, oBase &r);
  static void loadExtraFields(gdioutput& gdi, const oBase* r);

  static void generateRunnerReport(oEvent &oe, 
    gdioutput &gdi, 
    int numX, int numY,
    bool onlySelectedRunner,
    const deque<pair<int, bool>> &runnersToReport);

  TabRunner(oEvent *oe);
  ~TabRunner(void);

  friend int runnerSearchCB(gdioutput *gdi, GuiEventType type, BaseInfo *data);
  friend int RunnerCB(gdioutput *gdi, GuiEventType type, BaseInfo *data);
  friend int PunchesCB(gdioutput *gdi, GuiEventType type, BaseInfo *data);
  friend int VacancyCB(gdioutput *gdi, GuiEventType type, BaseInfo *data);
};
