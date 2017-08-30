/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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

#include "stdafx.h"

#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "gdiconstants.h"

#include "csvparser.h"
#include "SportIdent.h"
#include "meos_util.h"
#include "TabRunner.h"
#include "TabTeam.h"
#include "TabList.h"
#include "Table.h"
#include <cassert>
#include "oListInfo.h"
#include "TabSI.h"
#include "intkeymapimpl.hpp"
#include "meosexception.h"
#include "MeOSFeatures.h"

int SportIdentCB(gdioutput *gdi, int type, void *data);

TabRunner::TabRunner(oEvent *poe):TabBase(poe)
{
  clearCompetitionData();
}

TabRunner::~TabRunner(void)
{
}

void disablePunchCourse(gdioutput &gdi);

bool TabRunner::loadPage(gdioutput &gdi, int runnerId)
{
  oe->checkDB();
  assert(this);

  if (currentMode == 5) {
    this->runnerId = runnerId;
    loadPage(gdi);
  }
  else {
    currentMode = 0;
    loadPage(gdi);
    pRunner r=oe->getRunner(runnerId, 0);
    if (r){
      selectRunner(gdi, r);
      return true;
    }
  }
  return false;
}

void TabRunner::enableControlButtons(gdioutput &gdi, bool enable, bool vacant)
{
  if (enable) {
    gdi.enableInput("Remove");
    gdi.enableInput("Save");
    gdi.enableInput("Undo");
    if (vacant) {
      gdi.disableInput("Move", true);
      gdi.disableInput("NoStart", true);
    }
    else {
      gdi.enableInput("Move", true);
      gdi.enableInput("NoStart", true);
    }
  }
  else {
    gdi.disableInput("Remove");
    gdi.disableInput("Save");
    gdi.disableInput("Undo");
    gdi.disableInput("Move", true);
    gdi.disableInput("NoStart", true);
  }
}

void TabRunner::selectRunner(gdioutput &gdi, pRunner r) {
  if (!r) {
    runnerId=0;
    gdi.setText("Name", "");
    gdi.setText("Bib", "");
    gdi.selectItemByData("RCourse", 0);
    updateNumShort(gdi, 0, 0);
    //Don't clear club and class


    gdi.setText("CardNo", "");
    gdi.enableInput("Start");
    gdi.setText("Start", "-");
    gdi.enableInput("Finish");
    gdi.setText("Finish", "-");

    gdi.setText("Time", "-");
    gdi.setText("Points", "");
    gdi.selectItemByData("Status", 0);

    gdi.clearList("Punches");
    gdi.clearList("Course");

    gdi.selectItemByData("Runners", -1);
    gdi.setInputFocus("Name", true);
    if (gdi.hasField("MultiR")) {
      gdi.clearList("MultiR");
      gdi.disableInput("MultiR");
    }
    gdi.enableEditControls(false);
    enableControlButtons(gdi, false, false);
    disablePunchCourse(gdi);

    if (gdi.hasField("EditTeam"))
      gdi.disableInput("EditTeam");
    gdi.setText("RunnerInfo", "", true);

    gdi.setText("TimeAdjust", "-");
    gdi.setText("PointAdjust", "");

    if (gdi.hasField("StatusIn")) {
      gdi.selectFirstItem("StatusIn");
      gdi.setText("PlaceIn", "");
      gdi.setText("TimeIn", "-");
      if (gdi.hasField("PointIn"))
        gdi.setText("PointIn", "");
    }

    return;
  }

  gdi.enableEditControls(true);
  disablePunchCourse(gdi);

  pRunner parent=r->getMultiRunner(0);

  r->synchronizeAll();
  //r->apply(false);
  vector<int> mp;
  r->evaluateCard(true, mp, 0, false);
  /*
  if (parent!=r) {
    parent->synchronize();
    parent->apply(false);
  }*/

  gdi.selectItemByData("Runners", parent->getId());

  runnerId=r->getId();

  gdi.setText("Name", r->getNameRaw());
  string bib = r->getBib();

  if (gdi.hasField("Bib")) {
    gdi.setText("Bib", bib);
    bool controlBib = r->getTeam() == 0 || (r->getClassRef() && r->getClassRef()->getBibMode() == BibFree);
    gdi.setInputStatus("Bib", controlBib);
  }

  gdi.setText("Club", r->getClub());

  oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterNone);
  gdi.addItem("RClass", lang.tl("Ingen klass"), 0);
  gdi.selectItemByData("RClass", r->getClassId());

  if (gdi.hasField("EditTeam")) {
    gdi.setInputStatus("EditTeam", r->getTeam()!=0);

    if (r->getTeam()) {
      gdi.setText("Team", r->getTeam()->getName());
    }
    else
      gdi.setText("Team", "");
  }

  gdi.setText("TimeAdjust", getTimeMS(r->getTimeAdjustment()));
  gdi.setText("PointAdjust", -r->getPointAdjustment());

#ifdef _DEBUG
  vector<int> delta;
  vector<int> place;
  vector<int> after;
  vector<int> placeAcc;
  vector<int> afterAcc;

  r->getSplitAnalysis(delta);
  r->getLegTimeAfter(after);
  r->getLegPlaces(place);

  r->getLegTimeAfterAcc(afterAcc);
  r->getLegPlacesAcc(placeAcc);

  string out;
  for (size_t k = 0; k<delta.size(); k++) {
    out += itos(place[k]);
    if (k<placeAcc.size())
      out += " (" + itos(placeAcc[k]) + ")";

    if (after[k]>0)
      out+= " +" + getTimeMS(after[k]);

    if (k<afterAcc.size() && afterAcc[k]>0)
      out+= " (+" + getTimeMS(afterAcc[k]) + ")";

    if (delta[k]>0)
      out+= " B: " + getTimeMS(delta[k]);

    out += " | ";

  }
  gdi.restore("fantom", false);
  gdi.setRestorePoint("fantom");
  gdi.addStringUT(0, out);
  gdi.refresh();
#endif

  if (gdi.hasField("MultiR")) {
    int numMulti=parent->getNumMulti();
    if (numMulti==0) {
      gdi.clearList("MultiR");
      gdi.disableInput("MultiR");
      lastRace=0;
    }
    else {
      char bf[32];
      gdi.clearList("MultiR");
      gdi.enableInput("MultiR");

      for (int k=0;k<numMulti+1;k++) {
        sprintf_s(bf, lang.tl("Lopp %d").c_str(), k+1);
        gdi.addItem("MultiR", bf, k);
      }
      gdi.selectItemByData("MultiR", r->getRaceNo());
    }
  }
  oe->fillCourses(gdi, "RCourse", true);
  string crsName = r->getCourse(false) ? r->getCourse(false)->getName() + " " : "";
  gdi.addItem("RCourse", crsName + lang.tl("[Klassens bana]"), 0);
  gdi.selectItemByData("RCourse", r->getCourseId());
  updateNumShort(gdi, r->getCourse(false), r);

  int cno = parent->getCardNo();
  gdi.setText("CardNo", cno>0 ? itos(cno) : "");

  warnDuplicateCard(gdi, cno, r);

  gdi.check("RentCard", parent->getDI().getInt("CardFee") != 0);
  bool hasFee = gdi.hasField("Fee");

  if (hasFee)
    gdi.setText("Fee", oe->formatCurrency(parent->getDI().getInt("Fee")));

  if (parent != r) {
    gdi.disableInput("CardNo");
    gdi.disableInput("RentCard");
    if (hasFee)
      gdi.disableInput("Fee");
    gdi.disableInput("RClass");
  }
  else {
    gdi.enableInput("CardNo");
    gdi.enableInput("RentCard");
    if (hasFee)
      gdi.enableInput("Fee");
    gdi.enableInput("RClass");
  }

  enableControlButtons(gdi, true, r->isVacant());

  gdi.setText("Start", r->getStartTimeS());
  gdi.setInputStatus("Start", canSetStart(r));

  gdi.setText("Finish", r->getFinishTimeS());
  gdi.setInputStatus("Finish", canSetFinish(r));

  gdi.setText("Time", r->getRunningTimeS());
  gdi.setText("Points", itos(r->getRogainingPoints(false)));

  gdi.selectItemByData("Status", r->getStatus());
  gdi.setText("RunnerInfo", lang.tl(r->getProblemDescription()), true);

  if (gdi.hasField("StatusIn")) {
    gdi.selectItemByData("StatusIn", r->getInputStatus());
    int ip = r->getInputPlace();
    if (ip > 0)
      gdi.setText("PlaceIn", ip);
    else
      gdi.setText("PlaceIn", MakeDash("-"));

    gdi.setText("TimeIn", r->getInputTimeS());
    if (gdi.hasField("PointIn"))
      gdi.setText("PointIn", r->getInputPoints());
  }

  pCard pc=r->getCard();

  pCourse pcourse=r->getCourse(true);

  if (pc) {
    gdi.setTabStops("Punches", 70);
    pc->fillPunches(gdi, "Punches", pcourse);
  }
  else gdi.clearList("Punches");

  if (pcourse) {
    pcourse->synchronize();
    gdi.setTabStops("Course", 50);
    pcourse->fillCourse(gdi, "Course");
    autoGrowCourse(gdi);
    gdi.enableInput("AddAllC");
  }
  else {
    gdi.clearList("Course");
    gdi.disableInput("AddAllC");
  }
}

int RunnerCB(gdioutput *gdi, int type, void *data)
{
  TabRunner &tc = dynamic_cast<TabRunner &>(*gdi->getTabs().get(TRunnerTab));

  return tc.runnerCB(*gdi, type, data);
}

int PunchesCB(gdioutput *gdi, int type, void *data)
{
  TabRunner &tc = dynamic_cast<TabRunner &>(*gdi->getTabs().get(TRunnerTab));
  return tc.punchesCB(*gdi, type, data);
}

int VacancyCB(gdioutput *gdi, int type, void *data)
{
  TabRunner &tc = dynamic_cast<TabRunner &>(*gdi->getTabs().get(TRunnerTab));

  return tc.vacancyCB(*gdi, type, data);
}

int runnerSearchCB(gdioutput *gdi, int type, void *data)
{
  TabRunner &tc = dynamic_cast<TabRunner &>(*gdi->getTabs().get(TRunnerTab));

  return tc.searchCB(*gdi, type, data);
}

int TabRunner::searchCB(gdioutput &gdi, int type, void *data) {
  static DWORD editTick = 0;
  string expr;
  bool showNow = false;
  bool filterMore = false;

  if (type == GUI_INPUTCHANGE) {
    inputId++;
    InputInfo &ii = *(InputInfo *)(data);
    expr = trim(ii.text);
    filterMore = expr.length() > lastSearchExpr.length() &&
                  expr.substr(0, lastSearchExpr.length()) == lastSearchExpr;
    editTick = GetTickCount();
    if (expr != lastSearchExpr) {
      int nr = oe->getNumRunners();
      if (timeToFill < 50 || (filterMore && (timeToFill * lastFilter.size())/nr < 50))
        showNow = true;
      else {// Delay filter
        gdi.addTimeoutMilli(500, "Search: " + expr, runnerSearchCB).setExtra((void *)inputId);
      }
    }
  }
  else if (type == GUI_TIMER) {

    TimerInfo &ti = *(TimerInfo *)(data);

    if (inputId != int(ti.getExtra()))
      return 0;

    expr = ti.id.substr(8);
    filterMore = expr.length() > lastSearchExpr.length() &&
              expr.substr(0, lastSearchExpr.length()) == lastSearchExpr;
    showNow = true;
  }
  else if (type == GUI_EVENT) {
    EventInfo &ev = *(EventInfo *)(data);
    if (ev.getKeyCommand() == KC_FIND) {
      gdi.setInputFocus("SearchText", true);
    }
    else if (ev.getKeyCommand() == KC_FINDBACK) {
      gdi.setInputFocus("SearchText", false);
    }
  }
  else if (type == GUI_FOCUS) {
    InputInfo &ii = *(InputInfo *)(data);

    if (ii.text == getSearchString()) {
      ((InputInfo *)gdi.setText("SearchText", ""))->setFgColor(colorDefault);
    }
  }

  if (showNow) {
    stdext::hash_set<int> filter;

    if (type == GUI_TIMER)
      gdi.setWaitCursor(true);

    if (filterMore) {

      oe->findRunner(expr, 0, lastFilter, filter);
      lastSearchExpr = expr;
      // Filter more
      if (filter.empty()) {
        vector< pair<string, size_t> > runners;
        runners.push_back(make_pair(lang.tl("Ingen matchar 'X'#" + expr), -1));
        gdi.addItem("Runners", runners);
      }
      else
        gdi.filterOnData("Runners", filter);

      filter.swap(lastFilter);
    }
    else {
      lastFilter.clear();
      oe->findRunner(expr, 0, lastFilter, filter);
      lastSearchExpr = expr;

      bool formMode = currentMode == 0;

      vector< pair<string, size_t> > runners;
      oe->fillRunners(runners, !formMode, formMode ? 0 : oEvent::RunnerFilterShowAll, filter);

      if (filter.size() == runners.size()){
      }
      else if (filter.empty()) {
        runners.clear();
        runners.push_back(make_pair(lang.tl("Ingen matchar 'X'#" + expr), -1));
      }

      filter.swap(lastFilter);
      gdi.addItem("Runners", runners);
    }

    if (lastFilter.size() == 1) {
      pRunner r = oe->getRunner(*lastFilter.begin(), 0);
      selectRunner(gdi, r);
    }
    if (type == GUI_TIMER)
      gdi.setWaitCursor(false);
  }

  return 0;
}

pRunner TabRunner::save(gdioutput &gdi, int runnerId, bool willExit) {
  oe->synchronizeList(oLCardId, true, true);
  TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
  tsi.storedInfo.clear();

  bool create=(runnerId==0);

  if (create)
    return 0;

  string name=gdi.getText("Name");

  if (name.empty())
    throw std::exception("Alla deltagare måste ha ett namn.");

  int cardNo = gdi.getTextNo("CardNo");

  ListBoxInfo lbi;
  gdi.getSelectedItem("Club", lbi);

  int clubId = 0;
  if (!lbi.text.empty()) {
    pClub pc = oe->getClub(lbi.text);
    if (!pc)
      pc=oe->addClub(lbi.text);
    pc->synchronize();

    clubId = pc->getId();
  }

  gdi.getSelectedItem("RClass", lbi);

  int classId = 0;
  if (signed(lbi.data)<=0 && oe->getNumClasses() == 0) {
    if (gdi.ask("Vill du skapa en ny klass?")) {
      pClass pc=oe->addClass(oe->getAutoClassName());
      pc->synchronize();
      classId = pc->getId();
    }
  }
  else
    classId = lbi.data;

  int year = 0;
  pRunner r;
  bool cardNoChanged = false;
  if (runnerId==0) {
    cardNoChanged = true;
    r = oe->addRunner(name, clubId, classId, cardNo, year, true);
    r->setCardNo(0, false, false); // Reset to get auto card match
  }
  else {
    r = oe->getRunner(runnerId, 0);
    if (r==0)
      throw meosException("Internal error runner index");

    cardNoChanged = r->getCardNo() != cardNo;
    if (r->getName() != name || (r->getClubId() != clubId && clubId != 0))
      r->updateFromDB(name, clubId, classId, cardNo, r->getBirthYear());
  }

  if (cardNoChanged && cardNo>0) {
    pRunner warnCardDupl = warnDuplicateCard(cardNo, r);
    if (warnCardDupl) {
      gdi.alert("Varning: Brickan X används redan av Y.#" + itos(cardNo) + "#" + warnCardDupl->getCompleteIdentification()); 
    }
  }

  if (r) {
    runnerId=r->getId();
    RunnerStatus originalStatus = r->getStatus();
    r->setName(name, true);

    if (gdi.hasField("Bib")) {
      const string &bib = gdi.getText("Bib");
      char pat[32];
      int num = oClass::extractBibPattern(bib, pat);
      r->setBib(bib, num, num>0, false);
    }

    bool noSetStatus = false;
    if (cardNo > 0 && r->getCard() && 
      r->getCard()->getCardNo() != cardNo && r->getCardNo() != cardNo) {
        if (gdi.ask("Vill du koppla isär X från inläst bricka Y?#" + r->getName() + 
                    "#" + r->getCard()->getCardNoString())) {
        r->setStatus(StatusUnknown, true, false, false);
        r->setCard(0);
        r->setFinishTime(0);
        r->synchronize(true);
        gdi.setText("Finish", "");
        noSetStatus = true;
      }
    }

    if (cardNo > 0 && cardNo != r->getCardNo() && oe->hasNextStage()) {
      if (gdi.ask("Vill du använda den nya brickan till alla etapper?")) {
        r->setTransferCardNoNextStage(true);
      }
    }

    r->setCardNo(cardNo, true);
    if (gdi.isChecked("RentCard"))
      r->getDI().setInt("CardFee", oe->getDI().getInt("CardFee"));
    else
      r->getDI().setInt("CardFee", 0);

    if (gdi.hasField("Fee"))
      r->getDI().setInt("Fee", oe->interpretCurrency(gdi.getText("Fee")));

    r->setStartTimeS(gdi.getText("Start"));
    r->setFinishTimeS(gdi.getText("Finish"));

    if (gdi.hasField("NumShort")) {
      r->setNumShortening(gdi.getSelectedItem("NumShort").first);
    }

    if (gdi.hasField("TimeAdjust")) {
      int t = convertAbsoluteTimeMS(gdi.getText("TimeAdjust"));
      if (t != NOTIME)
        r->setTimeAdjustment(t);
    }
    if (gdi.hasField("PointAdjust")) {
      r->setPointAdjustment(-gdi.getTextNo("PointAdjust"));
    }

    r->setClubId(clubId);

    if (!willExit) {
      oe->fillClubs(gdi, "Club");
      gdi.setText("Club", r->getClub());
    }

    pClass pNewCls = oe->getClass(classId);
    if (pNewCls && pNewCls->getClassType() == oClassRelay) {
      if (!r->getTeam()) {
        gdi.alert("För att delta i en lagklass måste deltagaren ingå i ett lag.");
        classId = 0;
      }
      else if (r->getTeam()->getClassId() != classId && r->getClassId() != classId) {
        gdi.alert("Deltagarens klass styrs av laget.");
        classId = r->getTeam()->getClassId();
      }
    }
    
    bool readStatusIn = true;
    if (r->getClassId() != classId && r->getInputStatus() != StatusNotCompetiting && r->hasInputData()) {
      if (gdi.ask("Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) {
        r->resetInputData();
        readStatusIn = false;
      }
    }

    r->setClassId(classId, true);

    r->setCourseId(gdi.getSelectedItem("RCourse").first);

    RunnerStatus sIn = (RunnerStatus)gdi.getSelectedItem("Status").first;
    bool checkStatus = sIn != originalStatus;
    if (r->getStatus() != sIn && !noSetStatus) {
      r->setStatus(sIn, true, false);
    }
    r->addClassDefaultFee(false);
    vector<int> mp;
    r->evaluateCard(true, mp, 0, true);

    if (r->getClassId() != classId) {
      gdi.alert("Deltagarens klass styrs av laget.");
    }

    if (checkStatus && sIn != r->getStatus())
      gdi.alert("Status matchar inte data i löparbrickan.");

    if (gdi.hasField("StatusIn") && readStatusIn) {
      r->setInputStatus(RunnerStatus(gdi.getSelectedItem("StatusIn").first));
      r->setInputPlace(gdi.getTextNo("PlaceIn"));
      r->setInputTime(gdi.getText("TimeIn"));
      if (gdi.hasField("PointIn"))
        r->setInputPoints(gdi.getTextNo("PointIn"));
    }

    r->synchronizeAll();

    if (r->getClassRef() && r->getClassRef()->hasClassGlobalDependance()) {
      set<int> cls;
      cls.insert(r->getClassId());
      oe->reEvaluateAll(cls, false);
    }
  }
  else
    runnerId=0;

  if (!willExit) {
    fillRunnerList(gdi);

    if (create)
      selectRunner(gdi, 0);
    else
      selectRunner(gdi, r);
  }
  return r;
}

int TabRunner::runnerCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON){
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Search") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Runners", lbi);
      string searchText = gdi.getText("SearchText");
      bool formMode = currentMode == 0;
      stdext::hash_set<int> foo;
      fillRunnerList(gdi);
      //oe->fillRunners(gdi, "Runners", !formMode, formMode ? 0 : oEvent::RunnerFilterShowAll);
      pRunner r=oe->findRunner(searchText, lbi.data, foo, foo);

      if (r) {
        if (formMode)
          selectRunner(gdi, r);
        gdi.selectItemByData("Runners", r->getId());
      }
      else
        gdi.alert("Löparen hittades inte");
    }
    else if (bi.id == "ShowAll") {
      fillRunnerList(gdi);
    }
    else if (bi.id=="Pair") {
      ListBoxInfo lbi;
      pRunner r=0;
      if (gdi.getSelectedItem("Runners", lbi) &&
                      (r=oe->getRunner(lbi.data, 0))!=0) {
        int cid = bi.getExtraInt();
        pCard card = oe->getCard(cid);
        if (!card)
          throw meosException("Internal error");
        card->synchronize();
        if (card->isRemoved())
          throw meosException("Card was removed");

        int newCardId=card->getId();

        int oldCardId=r->setCard(newCardId);
        gdi.restore("CardTable");
        Table &t=gdi.getTable();
        t.reloadRow(newCardId);
        if (oldCardId)
          t.reloadRow(oldCardId);
        gdi.enableTables();
        gdi.refresh();
      }
    }
    else if (bi.id == "Window") {
      gdioutput *gdi_new = createExtraWindow(uniqueTag("kiosk"), MakeDash("MeOS - " + oe->getName()), gdi.getWidth() + 64 + gdi.scaleLength(120));
      if (gdi_new) {
        TabRunner &tr = dynamic_cast<TabRunner &>(*gdi_new->getTabs().get(TRunnerTab));
        tr.currentMode = currentMode;
        tr.runnerId = runnerId;
        tr.ownWindow = true;
        tr.loadPage(*gdi_new);
      }
    }
    else if (bi.id == "Kiosk") {
      if (gdi.ask("ask:kiosk")) {
        oe->setReadOnly();
        oe->updateTabs();
        loadPage(gdi);
      }
    }
    else if (bi.id == "ListenReadout") {
      listenToPunches = gdi.isChecked(bi.id);
    }
    else if (bi.id=="Unpair") {
      ListBoxInfo lbi;
      int cid = bi.getExtraInt();
      pCard c = oe->getCard(cid);

      if (c->getOwner())
        c->getOwner()->setCard(0);

      gdi.restore("CardTable");
      Table &t=gdi.getTable();
      t.reloadRow(c->getId());
      gdi.enableTables();
      gdi.refresh();
    }
    else if (bi.id=="TableMode") {
      gdi.canClear();
      currentMode = 1;
      loadPage(gdi);
    }
    else if (bi.id=="FormMode") {
      if (currentMode != 0) {
        currentMode = 0;
        gdi.canClear();
        gdi.enableTables();
        loadPage(gdi);
      }
    }
    else if (bi.id=="Vacancy") {
      if (currentMode != 4) {
        gdi.canClear();
        showVacancyList(gdi, "add");
      }
    }
    else if (bi.id == "ReportMode") {
      if (currentMode != 5) {
        gdi.canClear();
        showRunnerReport(gdi);
      }
    }
    else if (bi.id=="Cards") {
      if (currentMode != 3) {
        gdi.canClear();
        showCardsList(gdi);
      }
    }
    else if (bi.id=="InForest") {
      if (currentMode != 2) {
        gdi.canClear();
        showInForestList(gdi);
      }
    }
    else if (bi.id=="CancelInForest") {
      clearInForestData();
      loadPage(gdi);
    }
    else if (bi.id=="CancelReturn") {
      loadPage(gdi);
    }
    else if (bi.id=="SetDNS") {
      for (size_t k=0; k<unknown.size(); k++)
        if (unknown[k]->getStatus()==StatusUnknown)
          unknown[k]->setStatus(StatusDNS, true, false);

      //Reevaluate and synchronize all
      oe->reEvaluateAll(set<int>(), true);
      clearInForestData();
      showInForestList(gdi);
    }
    else if (bi.id=="SetUnknown") {
      for (size_t k=0; k<known_dns.size(); k++)
        if (known_dns[k]->getStatus()==StatusDNS)
          known_dns[k]->setStatus(StatusUnknown, true, false);

      //Reevaluate and synchronize all
      oe->reEvaluateAll(set<int>(), true);
      clearInForestData();
      showInForestList(gdi);
    }
    else if (bi.id == "RemoveVacant") {
      if (gdi.ask("Vill du radera alla vakanser från tävlingen?")) {
        oe->removeVacanies(0);
        gdi.disableInput(bi.id.c_str());
      }
    }
    else if (bi.id=="Cancel") {
      gdi.restore("CardTable");
      gdi.enableTables();
      gdi.refresh();
    }
    else if (bi.id=="SplitPrint") {
      if (!runnerId)
        return 0;
      pRunner r=oe->getRunner(runnerId, 0);
      if (!r) return 0;

      gdioutput gdiprint(2.0, gdi.getEncoding(), gdi.getHWND(), splitPrinter);
      if (bi.getExtraInt() == 0)
        r->printSplits(gdiprint);
      else
        r->printStartInfo(gdiprint);
      gdiprint.print(oe, 0, false, true);
      gdiprint.fetchPrinterSettings(splitPrinter);
    }
    else if (bi.id == "PrintSettings") {
      if (runnerId)
        save(gdi, runnerId, true);
      TabList::splitPrintSettings(*oe, gdi, false, TRunnerTab, (TabList::PrintSettingsSelection)bi.getExtraInt());
    }
    else if (bi.id == "EditTeam") {
      pRunner r = oe->getRunner(runnerId, 0);
      if (r && r->getTeam()) {
        save(gdi, runnerId, true);

        TabTeam *tt = (TabTeam *)gdi.getTabs().get(TTeamTab);
        tt->loadPage(gdi, r->getTeam()->getId());
      }
    }
    else if (bi.id=="Save") {
      save(gdi, runnerId, false);
      return true;
    }
    else if (bi.id=="Undo") {
      selectRunner(gdi, oe->getRunner(runnerId, 0));
      return true;
    }
    else if (bi.id=="Add") {
      if (runnerId>0) {
        string name = gdi.getText("Name");
        pRunner r = oe->getRunner(runnerId, 0);
        if (!name.empty() && r && r->getName() != name && r->getNameRaw() != name) {
          if (gdi.ask("Vill du lägga till deltagaren 'X'?#" + name)) {
            r = oe->addRunner(name, 0, 0, 0,0, false);
            runnerId = r->getId();
          }
          save(gdi, runnerId, false);
          return true;
        }

        save(gdi, runnerId, true);
      }
      ListBoxInfo lbi;
      gdi.getSelectedItem("RClass", lbi);

      pRunner r = oe->addRunner(oe->getAutoRunnerName(), 0,0,0,0, false);
      if (signed(lbi.data)>0)
        r->setClassId(lbi.data, true);
      else
        r->setClassId(oe->getFirstClassId(false), true);

      fillRunnerList(gdi);
      oe->fillClubs(gdi, "Club");
      selectRunner(gdi, r);
      gdi.setInputFocus("Name", true);
    }
    else if (bi.id=="Remove") {
      if (!runnerId)
        return 0;

      if (gdi.ask("Vill du verkligen ta bort löparen?")) {
        if (oe->isRunnerUsed(runnerId))
          gdi.alert("Löparen ingår i ett lag och kan inte tas bort.");
        else {
          pRunner r = oe->getRunner(runnerId, 0);
          if (r)
            r->remove();
          fillRunnerList(gdi);
          //oe->fillRunners(gdi, "Runners");
          selectRunner(gdi, 0);
        }
      }
    }
    else if (bi.id=="NoStart") {
      if (!runnerId)
        return 0;
      pRunner r = oe->getRunner(runnerId, 0);
      r = r->getMultiRunner(0);

      if (r && gdi.ask("Bekräfta att deltagaren har lämnat återbud.")) {
        if (r->getStartTime()>0) {
          pRunner newRunner = oe->addRunnerVacant(r->getClassId());
          newRunner->cloneStartTime(r);
          newRunner->setStartNo(r->getStartNo(), false);
          if (r->getCourseId())
            newRunner->setCourseId(r->getCourseId());
          newRunner->synchronizeAll();
        }

        for (int k=0;k<r->getNumMulti()+1; k++) {
          pRunner rr = r->getMultiRunner(k);
          if (rr) {
            rr->setStartTime(0, true, false);
            rr->setStartNo(0, false);
            rr->setStatus(StatusDNS, true, false);
            rr->setCardNo(0, false);
          }
        }
        r->synchronizeAll();
        selectRunner(gdi, r);
      }
    }
    else if (bi.id == "Move") {
      pRunner r = oe->getRunner(runnerId, 0);

      if (!runnerId || !r)
        return 0;
      gdi.clearPage(true);

      gdi.pushX();
      gdi.fillRight();
      gdi.addString("", boldLarge, "Klassbyte");
      gdi.addStringUT(boldLarge, r->getName()).setColor(colorDarkRed);

      //gdi.fillRight();
      //gdi.addString("", 0, "Deltagare:");
      gdi.fillDown();
      gdi.dropLine(2);
      gdi.popX();
      gdi.addString("", 0, "Välj en vakant plats nedan.");

      oe->generateVacancyList(gdi, RunnerCB);

      gdi.dropLine();
      gdi.addButton("CancelReturn", "Avbryt", RunnerCB);
    }
    else if (bi.id == "RemoveC") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Punches", lbi);

      DWORD rid=runnerId;
      if (!rid)
        return 0;

      pRunner r=oe->getRunner(rid, 0);

      if (!r) return 0;

      pCard card=r->getCard();

      if (!card) return 0;

      card->deletePunch(card->getPunchByIndex(lbi.data));
      card->synchronize();
      //Update runner
      vector<int> mp;
      r->evaluateCard(true, mp, 0, true);

      card->fillPunches(gdi, "Punches", r->getCourse(true));

      gdi.setText("Time", r->getRunningTimeS());
      gdi.selectItemByData("Status", r->getStatus());
    }
    else if (bi.id=="Check") {
    }
  }
  else if (type==GUI_INPUT) {
    InputInfo ii=*(InputInfo *)data;

    if (ii.id=="CardNo") {
      int cardNo = gdi.getTextNo("CardNo");
      if (runnerId) {
        pRunner r = oe->getRunner(runnerId, 0);
        if (r) {
          warnDuplicateCard(gdi, cardNo, r);
        }
      }
    }
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Runners") {
      if (gdi.isInputChanged("")) {
        pRunner r = oe->getRunner(runnerId, 0);
        bool newName = r && r->getName() != gdi.getText("Name");

        save(gdi, runnerId, true);

        if (newName)
          fillRunnerList(gdi);
      }

      if (bi.data == -1) {
        fillRunnerList(gdi);
        return 0;
      }

      pRunner r=oe->getRunner(bi.data, 0);

      if (r==0)
        throw meosException("Internal error runner index");

      if (lastRace<=r->getNumMulti())
        r=r->getMultiRunner(lastRace);

      oe->fillClubs(gdi, "Club");
      selectRunner(gdi, r);
    }
    else if (bi.id=="ReportRunner") {
      if (bi.data == -1) {
        fillRunnerList(gdi);
        return 0;
      }
      runnerId = bi.data;
      //loadPage(gdi);
      PostMessage(gdi.getTarget(), WM_USER + 2, TRunnerTab, 0);
    }
    else if (bi.id=="RClass") {
      gdi.selectItemByData("RCourse", 0);
      pCourse crsToUse = 0;
      
      if (bi.data==0) {
        gdi.clearList("Course");
      }
      else {
        pClass Class=oe->getClass(bi.data);

        if (Class) {
          crsToUse = Class->getCourse();

          pRunner rTmp;
          if (crsToUse == 0 && (rTmp = oe->getRunner(runnerId, 0)) != 0) {
            crsToUse = Class->getCourse(rTmp->getLegNumber(), rTmp->getStartNo());
          }
        }

        string crsName;
        if (crsToUse) {
          crsToUse->fillCourse(gdi, "Course");
          autoGrowCourse(gdi);
          crsName = crsToUse->getName() + " ";
        }
        else {
          gdi.clearList("Course");
        }

        ListBoxInfo rcrs;
        gdi.getSelectedItem("RCourse", rcrs);
        oe->fillCourses(gdi, "RCourse", true);
        gdi.addItem("RCourse", crsName + lang.tl("[Klassens bana]"), 0);
        gdi.selectItemByData("RCourse", rcrs.data);
      }

      updateNumShort(gdi, crsToUse, oe->getRunner(runnerId, 0));
    }
    else if (bi.id=="RCourse") {
      pCourse crsToUse = 0;
      pRunner r=oe->getRunner(runnerId, 0);
        
      if (bi.data==0) {
        gdi.clearList("Course");
        if (r) {  //Fix for multi classes, course depends on runner.
          ListBoxInfo lbi;
          gdi.getSelectedItem("RClass", lbi);
          if (signed(lbi.data)>0) {
            r->setClassId(lbi.data, true);
            r = oe->getRunner(runnerId, 0);
            if (r) {
              r->synchronize(true);
              crsToUse = r->getCourse(true);
            }
          }
        }
        if (!r) {
          pClass Class=oe->getClass(gdi.getSelectedItem("RClass").first);
          if (Class)
            crsToUse = Class->getCourse();
        }
      }
      else {
        crsToUse=oe->getCourse(bi.data);
      }
      if (crsToUse) {
        crsToUse->fillCourse(gdi, "Course");
        autoGrowCourse(gdi);
        updateNumShort(gdi, crsToUse, r);
      }
    }
    else if (bi.id=="MultiR") {
      pRunner r=oe->getRunner(runnerId, 0);
      lastRace=bi.data;
      if (r)
        selectRunner(gdi, r->getMultiRunner(bi.data));
    }
  }
  else if (type==GUI_EVENT) {
    EventInfo ei=*(EventInfo *)data;

    if (ei.id=="LoadRunner") {
      pRunner r=oe->getRunner(ei.getData(), 0);
      if (r) {
        selectRunner(gdi, r);
        gdi.selectItemByData("Runners", r->getId());
      }
    }
    else if (ei.id=="CellAction") {
      //oBase *b=static_cast<oBase *>(ei.getExtra());
      oBase *b = oe->getCard(ei.getExtraInt());
      
      cellAction(gdi, ei.getData(), b);
    }
    else if ((ei.id == "DataUpdate") && listenToPunches && currentMode == 5) {
      if (ei.getData() > 0) {
        runnerId = ei.getData();
      }
      loadPage(gdi);
    }
    else if ((ei.id == "ReadCard") &&
            (listenToPunches || oe->isReadOnly()) && currentMode == 5) {
      if (ei.getData() > 0) {
        vector<pRunner> rs;
        oe->getRunnersByCard(ei.getData(), rs);
        if (!rs.empty()) {
          runnersToReport.resize(rs.size());
          for (size_t k = 0; k<rs.size(); k++)
            runnersToReport[k] = make_pair(rs[k]->getId(), false);
        }
        runnerId = 0;
      }
      loadPage(gdi);
    }
  }
  else if (type==GUI_CLEAR) {
    if (runnerId>0 && currentMode == 0)
      save(gdi, runnerId, true);

    return true;
  }
  else if (type == GUI_LINK) {
    int id = static_cast<TextInfo*>(data)->getExtraInt();
    oRunner *vacancy = oe->getRunner(id, 0);

    if (vacancy==0 || vacancy->getClassId()==0)
      return -1;

    pRunner r = oe->getRunner(runnerId, 0);

    vacancy->synchronize();
    r->synchronize();

    if (r==0)
      return -1;

    char bf[1024];
    sprintf_s(bf, lang.tl("Bekräfta att %s byter klass till %s.").c_str(),
                  r->getName().c_str(), vacancy->getClass().c_str());
    if (gdi.ask(string("#") + bf)) {

      vacancy->synchronize();
      if (!vacancy->isVacant())
        throw std::exception("Starttiden är upptagen.");

      oRunner temp(oe, 0);
      temp.setTemporary();
      temp.setBib(r->getBib(), 0, false, false);
      temp.setStartNo(r->getStartNo(), false);
      temp.setClassId(r->getClassId(), true);
      temp.apply(false, 0, false);
      temp.cloneStartTime(r);

      r->setClassId(vacancy->getClassId(), true);
      // Remove or create multi runners
      r->createMultiRunner(true, true);
      r->apply(false, 0, false);
      r->cloneStartTime(vacancy);
      r->setBib(vacancy->getBib(), 0, false, false);
      r->setStartNo(vacancy->getStartNo(), false);

      if (oe->hasPrevStage()) {
        if (gdi.ask("Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) 
          r->resetInputData();
      }

      vacancy->setClassId(temp.getClassId(), true);
      // Remove or create multi runners
      vacancy->createMultiRunner(true, true);
      vacancy->apply(false, 0, false);
      vacancy->cloneStartTime(&temp);
      vacancy->setBib(temp.getBib(), 0, false, false);
      vacancy->setStartNo(temp.getStartNo(), false);

      r->synchronizeAll();
      vacancy->synchronizeAll();

      loadPage(gdi);
      selectRunner(gdi, r);
    }
  }
  return 0;
}

void TabRunner::showCardsList(gdioutput &gdi)
{
  currentMode = 3;
  gdi.clearPage(false);
  gdi.dropLine(0.5);
  gdi.addString("", boldLarge, "Hantera löparbrickor");
  gdi.addString("", 10, "help:14343");
  addToolbar(gdi);
  gdi.dropLine();
  cardModeStartY=gdi.getCY();
  Table *t=oe->getCardsTB();
  gdi.addTable(t,gdi.getCX(),gdi.getCY()+15);
  gdi.registerEvent("CellAction", RunnerCB);
  gdi.refresh();
}

int TabRunner::vacancyCB(gdioutput &gdi, int type, void *data)
{
  if (type == GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
      

    if (bi.id == "VacancyAdd") {
      showVacancyList(gdi, "add");
    }
    else if (bi.id == "PrinterSetup") {
      tsi.setPrintStartInfo(true);
      TabList::splitPrintSettings(*oe, gdi, true, TRunnerTab, TabList::StartInfo);
    }
    else if (bi.id == "CancelVacant") {
      tsi.storedInfo.clear();
      loadPage(gdi);
    }
  }
  else if (type == GUI_LINK) {
    int id = static_cast<TextInfo*>(data)->getExtraInt();
    oRunner *r = oe->getRunner(id, 0);

    if (r==0)
      return -1;

    r->synchronize();
    if (!r->isVacant())
      throw std::exception("Starttiden är upptagen.");

    string name = gdi.getText("Name");

    if (name.empty())
      throw std::exception("Alla deltagare måste ha ett namn.");

    int cardNo = gdi.getTextNo("CardNo");

    if (cardNo!=r->getCardNo() && oe->checkCardUsed(gdi, *r, cardNo))
      return 0;

    string club = gdi.getText("Club");
    int birthYear = 0;
    pClub pc = oe->getClubCreate(0, club);

    r->updateFromDB(name, pc->getId(), r->getClassId(), cardNo, birthYear);

    r->setName(name, true);
    r->setCardNo(cardNo, true);

    r->setClub(club);
    int fee = 0;
    if (gdi.hasField("Fee")) {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("Fee", lbi) && lbi.data == -1) {
        lastFee = "@";
        // Use class default fee
      }
      else {
        r->setFlag(oRunner::FlagFeeSpecified, true);
        fee = oe->interpretCurrency(gdi.getText("Fee"));
        lastFee = oe->formatCurrency(fee);
        r->getDI().setInt("Fee", fee);
      }
    }

    r->getDI().setDate("EntryDate", getLocalDate());
    r->addClassDefaultFee(false);
    int cardFee = 0;

    if (gdi.isChecked("RentCard")) {
      cardFee = oe->getDI().getInt("CardFee");
      r->getDI().setInt("CardFee", cardFee);
    }
    else
      r->getDI().setInt("CardFee", 0);


    
    if (cardFee < 0)
      cardFee = 0;
    fee = r->getDCI().getInt("Fee");
    
    TabSI::writePayMode(gdi, fee + cardFee, *r);  

    if (gdi.hasField("AllStages")) {
      r->setFlag(oRunner::FlagTransferSpecified, true);
      r->setFlag(oRunner::FlagTransferNew, gdi.isChecked("AllStages"));
    }

    if (oe->hasPrevStage()) {
      if (gdi.ask("Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) 
        r->resetInputData();
    }

    r->synchronizeAll();
    
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    
    // Print start certificate
    tsi.generateStartInfo(gdi, *r);

    showVacancyList(gdi, "", r->getClassId());
  }
  else if (type==GUI_INPUT) {
    InputInfo ii=*(InputInfo *)data;

    if (ii.id=="CardNo") {
      int cardNo = gdi.getTextNo("CardNo");
      if (gdi.getText("Name").empty())
        setCardNo(gdi, cardNo);
    }
  }
  else if (type==GUI_POSTCLEAR) {
    // Clear out SI-link
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    tsi.setCardNumberField("");

    return true;
  }
  else if (type == GUI_CLEAR) {
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    tsi.storedInfo.clear();
    tsi.storedInfo.storedName = gdi.getText("Name");
    tsi.storedInfo.storedCardNo = gdi.getText("CardNo");
    tsi.storedInfo.storedClub = gdi.getText("Club");
    ListBoxInfo lbi;
    if (gdi.getSelectedItem("Fee", lbi) && lbi.data == -1) {
      tsi.storedInfo.storedFee = "@";
      // Use class default fee
    }
    else
      tsi.storedInfo.storedFee = gdi.getText("Fee", true);
    
    tsi.storedInfo.allStages = gdi.isChecked("AllStages");
    tsi.storedInfo.rentState = gdi.isChecked("RentCard");
    tsi.storedInfo.hasPaid = gdi.isChecked("Paid");
    tsi.storedInfo.payMode =  gdi.hasField("PayMode") ? gdi.getSelectedItem("PayMode").first : 0;
    return 1;
  }
  return 0;
}

void TabRunner::setCardNo(gdioutput &gdi, int cardNo)
{
    pRunner db_r=oe->dbLookUpByCard(cardNo);

  if (db_r) {
    gdi.setText("Name", db_r->getName());
    gdi.setText("Club", db_r->getClub());
  }
}

void TabRunner::showRunnerReport(gdioutput &gdi)
{
  gdi.clearPage(true);
  currentMode = 5;

  if (!ownWindow && !oe->isReadOnly())
    addToolbar(gdi);
  else if (oe->isReadOnly())
    gdi.addString("", fontLarge, MakeDash("MeOS - Resultatkiosk")).setColor(colorDarkBlue);

   gdi.dropLine();

  gdi.pushX();
  gdi.fillRight();

  gdi.addSelection("ReportRunner", 300, 300, RunnerCB);
  oe->fillRunners(gdi, "ReportRunner", true, oEvent::RunnerFilterShowAll|oEvent::RunnerCompactMode);
  gdi.selectItemByData("ReportRunner", runnerId);

  if (!oe->isReadOnly()) {
    if (!ownWindow) {
      gdi.addButton("Kiosk", "Resultatkiosk", RunnerCB);
      gdi.addButton("Window", "Eget fönster", RunnerCB, "Öppna i ett nytt fönster.");
    }
    gdi.dropLine(0.2);
    gdi.addCheckbox("ListenReadout", "Visa senast inlästa deltagare", RunnerCB, listenToPunches);
  }

  gdi.dropLine(3);
  gdi.popX();
  gdi.registerEvent("DataUpdate", RunnerCB);
  gdi.registerEvent("ReadCard", RunnerCB);

  gdi.fillDown();
  oe->calculateResults(oEvent::RTClassResult);

  if (runnerId > 0) {
    runnersToReport.resize(1);
    runnersToReport[0] = make_pair(runnerId, false);
  }

  cTeam t = 0;
  for (size_t k = 0; k < runnersToReport.size(); k++) {
    pRunner r = oe->getRunner(runnersToReport[k].first, 0);
    if (r && r->getTeam()) {
      pClass cls = oe->getClass(r->getClassId());
      if (cls && cls->getClassType() == oClassPatrol)
        continue;

      if (t == 0)
        t = r->getTeam();
    }
  }

  if (runnersToReport.size() == 1)
    runnerId = runnersToReport[0].first;

  if (t == 0) {
    for (size_t k = 0; k < runnersToReport.size(); k++)
      runnerReport(gdi, runnersToReport[k].first, runnersToReport[k].second);
  }
  else {
    oe->calculateTeamResults(false);

    set<int> selectedRunners;
    bool selHasRes = false;
    for (size_t k = 0; k < runnersToReport.size(); k++) {
      selectedRunners.insert(runnersToReport[k].first);
      pRunner r = oe->getRunner(runnersToReport[k].first, 0);
      if (r->getStatus() != StatusUnknown)
        selHasRes = true;
    }

    string tInfo = t->getName();
    if (t->statusOK()) {
      tInfo += ", "  + t->getRunningTimeS() +  lang.tl(".S Placering: ") + t->getPlaceS();
      if (t->getTimeAfter(-1) > 0)
        tInfo += ", +" + formatTime(t->getTimeAfter(-1));
    }
    else if (t->getStatus() != StatusUnknown) {
      tInfo += " " + t->getStatusS();
    }

    gdi.addStringUT(fontMediumPlus, t->getClass());
    gdi.addStringUT(boldLarge, tInfo);
    gdi.dropLine();

    bool visitedSelected = false;
    for (int leg = 0; leg < t->getNumRunners(); leg++) {
      if (selHasRes && visitedSelected)
          break;

      pRunner r = t->getRunner(leg);
      pRunner nextR = t->getRunner(leg + 1);
      bool nextSelected = nextR && selectedRunners.count(nextR->getId());

      if (r) {
        bool selected = selectedRunners.count(r->getId()) > 0;
      
        if (selHasRes) {
          runnerReport(gdi, r->getId(), !selected);
        }
        else {
          runnerReport(gdi, r->getId(), !nextSelected);
        }

        visitedSelected |= selected;
      }
    }
  }
}

void TabRunner::runnerReport(gdioutput &gdi, int id, bool compact) {
  pRunner r = oe->getRunner(id, 0);
  if (!r || ! r->getClassRef())
    return;

  gdi.pushX();
  gdi.fillDown();
  if (r->getTeam() == 0) {
    gdi.addStringUT(fontMediumPlus, r->getClass());
    gdi.addStringUT(boldLarge, r->getCompleteIdentification());
  }
  else {
    string s;
    if (r->getTeam())
      s += r->getClassRef()->getLegNumber(r->getLegNumber());

    s += ": " + r->getName();
    gdi.addStringUT(boldText, s);
  }

  string str;
  if (r->getTeam() == 0) {
    str = oe->formatListString(lRunnerTimeStatus,  r);
  }
  else {
    str = oe->formatListString(lTeamLegTimeStatus,  r);
    str += " (" + oe->formatListString(lRunnerTimeStatus,  r) + ")";
  }
  
  gdi.dropLine(0.3);

  if (r->statusOK()) {
    int total, finished,  dns;
    oe->getNumClassRunners(r->getClassId(), r->getLegNumber(), total, finished, dns);

    if (r->getTeam() == 0) {
      gdi.addString("", fontMediumPlus, "Tid: X, nuvarande placering Y/Z.#" + str + "#" + r->getPlaceS() + "#" + itos(finished));
    }
    else {
      int place = r->getTeam()->getLegPlace(r->getLegNumber(), false);
      if (place > 0 && place < 10000) {
        gdi.addString("", fontMediumPlus, "Tid: X, nuvarande placering Y/Z.#" + str + "#" + itos(place) + "#" + itos(finished));
      }
      else {
        gdi.addStringUT(fontMediumPlus, str).setColor(colorRed);
      }
    }
  }
  else if (r->getStatus() != StatusUnknown) {
    gdi.addStringUT(fontMediumPlus, str).setColor(colorRed);
  }

  gdi.popX();
  gdi.fillRight();

  if (r->getStartTime() > 0)
    gdi.addString("", fontMedium, "Starttid: X  #" + r->getStartTimeCompact());

  if (r->getFinishTime() > 0)
    gdi.addString("", fontMedium, "Måltid: X  #" + r->getFinishTimeS());

  const string &after = oe->formatListString(lRunnerTimeAfter, r);
  if (!after.empty()) {
    gdi.addString("", fontMedium, "Tid efter: X  #" + after);
  }

  const string &lost = oe->formatListString(lRunnerMissedTime,  r);
  if (!lost.empty()) {
    gdi.addString("", fontMedium, "Bomtid: X  #" + lost).setColor(colorDarkRed);
  }

  gdi.popX();
  gdi.dropLine(2.5);
  gdi.fillDown();

  if (compact)
    return;

  pCourse crs = r->getCourse(true);

  int xp = gdi.getCX();
  int yp = gdi.getCY();
  int xw = gdi.scaleLength(130);
  int cx = xp;
  int limit = (9*xw)/10;
  int lh = gdi.getLineHeight();

  if (crs && r->getStatus() != StatusUnknown) {
    int nc = crs->getNumControls();
    vector<int> delta;
    vector<int> place;
    vector<int> after;
    vector<int> placeAcc;
    vector<int> afterAcc;

    r->getSplitAnalysis(delta);
    r->getLegTimeAfter(after);
    r->getLegPlaces(place);

    r->getLegTimeAfterAcc(afterAcc);
    r->getLegPlacesAcc(placeAcc);
    int end = crs->useLastAsFinish() ? nc - 1 : nc;
    int start = crs->useFirstAsStart() ? 1 : 0;
    for (int k = start; k<=end; k++) {
      string name = crs->getControlOrdinal(k);
      if ( k < end) {
        pControl ctrl = crs->getControl(k);
        if (ctrl && ctrl->getFirstNumber() > 0)
          name += " (" + itos(ctrl->getFirstNumber()) + ")";
        gdi.addString("", yp, cx, boldText, "Kontroll X#" + name, limit);
      }
      else
        gdi.addStringUT(yp, cx, boldText, name, limit);

      string split = r->getSplitTimeS(k, false);

      int bestTime = 0;
      if ( k < int(after.size()) && after[k] >= 0)
        bestTime = r->getSplitTime(k, false) - after[k];

      GDICOLOR color = colorDefault;
      if (k < int(after.size()) ) {
        if (after[k] > 0)
          split += " (" + itos(place[k]) + ", +"  + getTimeMS(after[k]) + ")";
        else if (place[k] == 1)
          split += lang.tl(" (sträckseger)");
        else if (place[k] > 0)
          split += " " + itos(place[k]);

        if (after[k] >= 0 && after[k]<=int(bestTime * 0.03))
          color = colorLightGreen;
      }
      gdi.addStringUT(yp + lh, cx, fontMedium, split, limit);

      if (k>0 && k < int(placeAcc.size())) {
        split = r->getPunchTimeS(k, false);
        string pl = placeAcc[k] > 0 ? itos(placeAcc[k]) : "-";
        if (k < int(afterAcc.size()) ) {
          if (afterAcc[k] > 0)
            split += " (" + pl + ", +"  + getTimeMS(afterAcc[k]) + ")";
        else if (placeAcc[k] == 1)
          split += lang.tl(" (ledare)");
          else if (placeAcc[k] > 0)
            split += " " + pl;
        }
        gdi.addStringUT(yp + 2*lh, cx, fontMedium, split, limit).setColor(colorDarkBlue);
      }

      if (k < int(delta.size()) && delta[k] > 0 ) {
        gdi.addString("", yp + 3*lh, cx, fontMedium, "Bomtid: X#" + getTimeMS(delta[k]));

        color = (delta[k] > bestTime * 0.5  && delta[k]>60 ) ?
                  colorMediumDarkRed : colorMediumRed;
      }

      RECT rc;
      rc.top = yp - 2;
      rc.bottom = yp + lh*5 - 4;
      rc.left = cx - 4;
      rc.right = cx + xw - 8;

      gdi.addRectangle(rc, color);

      cx += xw;

      if (k % 6 == 5) {
        cx = xp;
        yp += lh * 5;
      }
    }
  }
  else {
    vector<pFreePunch> punches;
    oe->getPunchesForRunner(r->getId(), punches);

    int lastT = r->getStartTime();
    for (size_t k = 0; k < punches.size(); k++) {

      string name = punches[k]->getType();
      string realName;
      if (atoi(name.c_str()) > 0) {
        const pCourse rCrs = r->getCourse(false);
        if (rCrs) {
          vector<pControl> crsCtrl;
          rCrs->getControls(crsCtrl);
          for(size_t j = 0; j < crsCtrl.size(); j++) {
            if (crsCtrl[j]->hasNumber(atoi(name.c_str()))) {
              if (crsCtrl[j]->hasName())
                realName = crsCtrl[j]->getName();

              break;
            }
          }
        }
        if (realName.empty())
          gdi.addString("", yp, cx, boldText, "Kontroll X#" + name, limit);
        else
          gdi.addStringUT(yp, cx, boldText, realName, limit);
      }
      else
        gdi.addStringUT(yp, cx, boldText, name, limit);

      int t = punches[k]->getAdjustedTime();
      if (t>0) {
        int st = r->getStartTime();
        gdi.addString("", yp + lh, cx, normalText, "Klocktid: X#" + oe->getAbsTime(t), limit);
        if (st > 0 && t > st) {
          string split = formatTimeHMS(t-st);
          if (lastT>0 && st != lastT && lastT < t)
            split += " (" + getTimeMS(t-lastT) + ")";
          gdi.addStringUT(yp + 2*lh, cx, normalText, split, limit);
        }
      }

      if (punches[k]->isStart() || punches[k]->getControlNumber() >= 30) {
        lastT = t;
      }

      GDICOLOR color = colorDefault;

      RECT rc;
      rc.top = yp - 2;
      rc.bottom = yp + lh*5 - 4;
      rc.left = cx - 4;
      rc.right = cx + xw - 8;

      gdi.addRectangle(rc, color);
      cx += xw;
      if (k % 6 == 5) {
        cx = xp;
        yp += lh * 5;
      }
    }
  }

  gdi.dropLine(3);
  gdi.popX();
}


void TabRunner::showVacancyList(gdioutput &gdi, const string &method, int classId)
{
  gdi.clearPage(true);
  currentMode = 4;

  if (method == "") {
    gdi.dropLine(0.5);
    gdi.addString("", boldLarge, "Tillsatte vakans");
    addToolbar(gdi);
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    tsi.storedInfo.clear();

    gdi.dropLine();

    gdi.fillRight();
    gdi.pushX();
    gdi.addButton("VacancyAdd", "Tillsätt ytterligare vakans", VacancyCB);
    //gdi.addButton("Cancel", "Återgå", VacancyCB);
    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(2);

    if (classId==0)
      oe->generateVacancyList(gdi, 0);
    else {
      oListParam par;
      par.selection.insert(classId);
      oListInfo info;
      par.listCode = EStdStartList;
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);
    }
  }
  else if (method == "add") {
    gdi.dropLine(0.5);
    gdi.addString("", boldLarge, "Tillsätt vakans");
    addToolbar(gdi);

    gdi.dropLine();

    gdi.fillRight();

    int bx = gdi.getCX();
    int by = gdi.getCY();

    gdi.setCX(bx + gdi.scaleLength(10));
    gdi.setCY(by + gdi.scaleLength(10));
    gdi.pushX();

    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    tsi.storedInfo.checkAge();
    
    gdi.addInput("CardNo", tsi.storedInfo.storedCardNo, 8, VacancyCB, "Bricka:");
    tsi.setCardNumberField("CardNo");
    
    //Remember to clear SI-link when page is cleared.
    gdi.setPostClearCb(VacancyCB);
    gdi.setOnClearCb(VacancyCB);

    gdi.dropLine(1.2);
    gdi.addCheckbox("RentCard", "Hyrd", 0, tsi.storedInfo.rentState);
    gdi.dropLine(-1.2);

    gdi.addInput("Name", tsi.storedInfo.storedName, 16, 0, "Namn:");

    gdi.addCombo("Club", 220, 300, 0, "Klubb:");
    oe->fillClubs(gdi, "Club");
    gdi.setText("Club", tsi.storedInfo.storedClub);

    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy)) {
      if (!tsi.storedInfo.storedFee.empty())
        lastFee = tsi.storedInfo.storedFee;

      gdi.addCombo("Fee", 60, 150, SportIdentCB, "Avgift:");
      oe->fillFees(gdi, "Fee", true);
      gdi.autoGrow("Fee");

      if (!lastFee.empty() && lastFee != "@") {
        gdi.setText("Fee", lastFee);
      }
      else {
        gdi.selectFirstItem("Fee");
      }
      gdi.dropLine(1.2);

      //gdi.addCheckbox("Paid", "Kontant betalning", 0, tsi.storedInfo.hasPaid);
      tsi.generatePayModeWidget(gdi);
      gdi.dropLine(-0.2);
    }
    else {
      gdi.dropLine();
    }
    
    gdi.dropLine(2.8);
    gdi.popX();
    gdi.addCheckbox("StartInfo", "Skriv ut startbevis", SportIdentCB, tsi.hasPrintStartInfo(), "Skriv ut startbevis för deltagaren");
    
    if (oe->hasNextStage())
      gdi.addCheckbox("AllStages", "Anmäl till efterföljande etapper", 0, tsi.storedInfo.allStages);
    
    gdi.dropLine(-0.2);
    gdi.addButton("PrinterSetup", "Skrivarinställningar...", VacancyCB, "Skrivarinställningar för sträcktider och startbevis");
    gdi.setCX(gdi.getCX() + gdi.scaleLength(40));
    gdi.addButton("CancelVacant", "Avbryt", VacancyCB);

    gdi.fillDown();
    gdi.dropLine(2.5);
    RECT rc = {bx, by, gdi.getWidth(), gdi.getCY() + gdi.scaleLength(20)};
    gdi.addRectangle(rc, colorLightCyan);

    gdi.setCX(bx);
    gdi.pushX();
    gdi.dropLine();

    gdi.addString("", fontMediumPlus, "Välj klass och starttid nedan");
    oe->generateVacancyList(gdi, VacancyCB);
    gdi.setInputFocus("CardNo");
  }
  else if (method == "move") {

  }
}


void TabRunner::clearInForestData()
{
  unknown_dns.clear();
  known_dns.clear();
  known.clear();
  unknown.clear();
}

void TabRunner::showInForestList(gdioutput &gdi)
{
  gdi.clearPage(false);
  currentMode = 2;
  gdi.dropLine(0.5);
  gdi.addString("", boldLarge, "Hantera kvar-i-skogen");
  addToolbar(gdi);
  gdi.dropLine();
  gdi.setRestorePoint("Help");
  gdi.addString("", 10, "help:425188");
  gdi.dropLine();
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("Import", "Importera stämplingar...", SportIdentCB).setExtra(1);
  
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Vacancy)) {
    vector<pRunner> rr;
    oe->getRunners(0, 0, rr, false);
    bool hasVac = false;
    for (size_t k = 0; k < rr.size(); k++) {
      if (rr[k]->isVacant()) {
        hasVac = true;
        break;
      }
    }
    if (hasVac) {
      gdi.addButton("RemoveVacant", "Radera vakanser", RunnerCB);
    }
  }
  gdi.addButton("SetDNS", "Sätt okända löpare utan registrering till <Ej Start>", RunnerCB);
  gdi.fillDown();
  gdi.addButton("SetUnknown", "Återställ löpare <Ej Start> med registrering till <Status Okänd>", RunnerCB);
  gdi.dropLine();
  gdi.popX();

  clearInForestData();
  oe->analyseDNS(unknown_dns, known_dns, known, unknown);
  oe->setupCardHash(false);
  if (!unknown.empty()) {
    gdi.dropLine();
    gdi.dropLine(0.5);
    gdi.addString("", 1, "Löpare, Status Okänd, som saknar registrering");
    listRunners(gdi, unknown, true);
  }
  else {
    gdi.disableInput("SetDNS");
  }

  if (!known.empty()) {
    gdi.dropLine();
    gdi.addString("", 1, "Löpare, Status Okänd, med registrering (kvar-i-skogen)");
    gdi.dropLine(0.5);
    listRunners(gdi, known, false);
  }

  if (!known_dns.empty()) {
    gdi.dropLine();
    gdi.addString("", 1, "Löpare, Ej Start, med registrering (kvar-i-skogen!?)");
    gdi.dropLine(0.5);
    listRunners(gdi, known_dns, false);
  }
  else
    gdi.disableInput("SetUnknown");

  oe->setupCardHash(true);

  if (known.empty() && unknown.empty() && known_dns.empty()) {
    gdi.addString("", 10, "inforestwarning");
  }

  gdi.refresh();
}

void TabRunner::listRunners(gdioutput &gdi, const vector<pRunner> &r, bool filterVacant) const
{
  char bf[64];
  int yp = gdi.getCY();
  int xp = gdi.getCX();
  vector<pRunner> out;
  for (size_t k=0; k<r.size(); k++) {
    if (filterVacant && r[k]->isVacant())
      continue;
    sprintf_s(bf, "%d.", k+1);
    gdi.addStringUT(yp, xp, 0, bf);
    gdi.addStringUT(yp, xp+40, 0, r[k]->getNameAndRace(true), 190);
    gdi.addStringUT(yp, xp+200, 0, r[k]->getClass(), 140);
    gdi.addStringUT(yp, xp+350, 0, r[k]->getClub(), 190);
    int c = r[k]->getCardNo();
    if (c>0) {
      oe->getRunnersByCardNo(c, true, true, out);
      if (out.size() <= 1) {
        gdi.addStringUT(yp, xp+550, 0, "(" + itos(c) + ")", 190);
      }
      else {
        TextInfo &ti = gdi.addStringUT(yp, xp+550, 0, "(" + itos(c) + ", " + lang.tl("reused card") + ")", 100);
        string tt;
        for (size_t j = 0; j < out.size(); j++) {
          if (out[j] == r[k]->getMultiRunner(0))
            continue;
          if (!tt.empty())
            tt += ", ";
          tt += out[j]->getName();
        }
        gdi.addToolTip(ti.id, tt, 0, &ti.textRect);
      }
    }
    yp += gdi.getLineHeight();
  }
}

void TabRunner::cellAction(gdioutput &gdi, DWORD id, oBase *obj)
{
  if (id==TID_RUNNER || id==TID_CARD) {
    gdi.disableTables();
    pCard c=dynamic_cast<pCard>(obj);
    if (c) {
      gdi.setRestorePoint("CardTable");
      int orgx=gdi.getCX();
      gdi.dropLine(1);
      gdi.setCY(cardModeStartY);
      gdi.scrollTo(orgx, cardModeStartY);
      gdi.addString("", fontMediumPlus, "Para ihop bricka X med en deltagare#" + itos(c->getCardNo())).setColor(colorDarkGreen);
      gdi.dropLine(0.5);

      string name = c->getOwner() ? c->getOwner()->getName() : MakeDash("-");
      gdi.addString("", 0, "Nuvarande innehavare: X.#" + name);

      gdi.dropLine(1);
      gdi.pushX();
      gdi.fillRight();
      gdi.addListBox("Card", 150, 300, 0, "Vald bricka:");
      c->fillPunches(gdi, "Card", 0);
      gdi.disableInput("Card");

      gdi.pushX();
      gdi.fillRight();
      gdi.popX();

      gdi.fillDown();
      gdi.addListBox("Runners", 350, 300, 0, "Deltagare:");
      gdi.setTabStops("Runners", 200, 300);
      oe->fillRunners(gdi, "Runners", true, oEvent::RunnerFilterShowAll);
      if (c->getOwner())
        gdi.selectItemByData("Runners", c->getOwner()->getId());

      gdi.popX();
      gdi.fillRight();
      gdi.addInput("SearchText", "", 15).setBgColor(colorLightCyan);
      gdi.addButton("Search", "Sök deltagare", RunnerCB, "Sök på namn, bricka eller startnummer.");

      gdi.popX();
      gdi.dropLine(3);
      gdi.addButton("Pair", "Para ihop", RunnerCB).setExtra(c->getId());
      gdi.addButton("Unpair", "Sätt som oparad", RunnerCB).setExtra(c->getId());
      gdi.addButton("Cancel", "Avbryt", RunnerCB);
      gdi.fillDown();
      gdi.popX();
      gdi.refresh();
    }
  }
}

void disablePunchCourseAdd(gdioutput &gdi)
{
  gdi.disableInput("AddC");
  gdi.disableInput("AddAllC");
  gdi.selectItemByData("Course", -1);
}

const string &TabRunner::getSearchString() const {
  return lang.tl("Sök (X)#Ctrl+F");
}

void disablePunchCourseChange(gdioutput &gdi)
{
  gdi.disableInput("SaveC");
  gdi.disableInput("RemoveC");
  gdi.disableInput("PTime");
  gdi.setText("PTime", "");
  gdi.selectItemByData("Punches", -1);

}

void disablePunchCourse(gdioutput &gdi)
{
  disablePunchCourseAdd(gdi);
  disablePunchCourseChange(gdi);
}

void UpdateStatus(gdioutput &gdi, pRunner r)
{
  if (!r) return;

  gdi.setText("Start", r->getStartTimeS());
  gdi.setText("Finish", r->getFinishTimeS());
  gdi.setText("Time", r->getRunningTimeS());
  gdi.selectItemByData("Status", r->getStatus());
  gdi.setText("RunnerInfo", lang.tl(r->getProblemDescription()), true);
}

int TabRunner::punchesCB(gdioutput &gdi, int type, void *data)
{
  DWORD rid=runnerId;
  if (!rid)
    return 0;

  pRunner r=oe->getRunner(rid, 0);

  if (!r){
    gdi.alert("Deltagaren måste sparas innan stämplingar kan hanteras.");
    return 0;
  }


  if (type==GUI_LISTBOX){
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Punches") {
      if (bi.data != -1) {
        pCard card=r->getCard();
        if (!card) return 0;
        pPunch punch = card->getPunchByIndex(bi.data);
        if (!punch) 
          throw meosException("Punch not found.");

        string ptime=punch->getTime();//;card->getPunchTime(punch);

        if (ptime!="") {
          gdi.enableInput("SaveC");
          gdi.setText("PTime", ptime);
        }
        gdi.enableInput("RemoveC");
        gdi.enableInput("PTime");
      }
      else {
        gdi.disableInput("SaveC");
        gdi.disableInput("RemoveC");
        gdi.setText("PTime", "");
      }
      disablePunchCourseAdd(gdi);
    }
    else if (bi.id=="Course") {
      if (signed(bi.data)>=0) {
        pCourse pc=r->getCourse(true);

        if (!pc) return 0;

        gdi.enableInput("AddC");
        gdi.enableInput("AddAllC");
      }
      else{
        gdi.disableInput("AddC");
        gdi.disableInput("AddAllC");
      }
      disablePunchCourseChange(gdi);
    }
  }
  else if (type==GUI_BUTTON){
    ButtonInfo bi=*(ButtonInfo *)data;
    pCard card=r->getCard();

    if (!card){
      if (!gdi.ask("ask:addpunches"))
        return 0;

      card=oe->allocateCard(r);

      card->setCardNo(r->getCardNo());
      vector<int> mp;
      r->addPunches(card, mp);

    }

    if (bi.id=="AddC"){
      vector<int> mp;
      r->evaluateCard(true, mp);

      pCourse pc=r->getCourse(true);

      if (!pc) return 0;

      ListBoxInfo lbi;

      if (!gdi.getSelectedItem("Course", lbi))
        return 0;

      oControl *oc=pc->getControl(lbi.data);

      if (!oc) return 0;
      vector<int> nmp;

      if (oc->getStatus() == oControl::StatusRogaining) {
        r->evaluateCard(true, nmp, oc->getFirstNumber()); //Add this punch
      }
      else {
        for (size_t k = 0; k<mp.size(); k++) {
          if (oc->hasNumber(mp[k]))
            r->evaluateCard(true, nmp, mp[k]); //Add this punch
        }
      }
      //synchronize SQL
      card->synchronize();
      r->synchronize(true);
      r->evaluateCard(true, mp);
      card->fillPunches(gdi, "Punches", pc);
      UpdateStatus(gdi, r);
    }
    else if (bi.id=="AddAllC"){
      vector<int> mp;
      r->evaluateCard(true, mp);
      vector<int>::iterator it=mp.begin();


      while(it!=mp.end()){
        vector<int> nmp;
        r->evaluateCard(true, nmp, *it); //Add this punch
        ++it;
        if (nmp.empty())
          break;
      }

      //synchronize SQL
      card->synchronize();
      r->synchronize(true);
      r->evaluateCard(true, mp);
      card->fillPunches(gdi, "Punches", r->getCourse(true));
      UpdateStatus(gdi, r);
    }
    else if (bi.id=="SaveC"){
      //int time=oe->GetRelTime();

      ListBoxInfo lbi;

      if (!gdi.getSelectedItem("Punches", lbi))
        return 0;

      pCard pc=r->getCard();

      if (!pc) return 0;

      pPunch pp = pc->getPunchByIndex(lbi.data);
      
      if (!pp)
        throw meosException("Punch not found.");

      pc->setPunchTime(pp, gdi.getText("PTime"));

      vector<int> mp;
      r->evaluateCard(true, mp);

      //synchronize SQL
      card->synchronize();
      r->synchronize();
      r->evaluateCard(true, mp);
      card->fillPunches(gdi, "Punches", r->getCourse(true));
      UpdateStatus(gdi, r);
      gdi.selectItemByData("Punches", lbi.data);
    }
  }
  return 0;
}

bool TabRunner::loadPage(gdioutput &gdi)
{
  oe->reEvaluateAll(set<int>(), true);

  if (oe->isReadOnly())
    currentMode = 5; // Evaluate result
  else {
    clearInForestData();
    gdi.selectTab(tabId);
  }
  gdi.clearPage(false);
  int basex=gdi.getCX();

  if (currentMode == 1) {
    Table *tbl=oe->getRunnersTB();
    addToolbar(gdi);
    gdi.dropLine(1);
    gdi.addTable(tbl, basex, gdi.getCY());
    return true;
  }
  else if (currentMode == 2) {
    showInForestList(gdi);
    return true;
  }
  else if (currentMode == 3) {
    showCardsList(gdi);
    return true;
  }
  else if (currentMode == 4) {
    showVacancyList(gdi, "add");
    return true;
  }
  else if (currentMode == 5) {
    showRunnerReport(gdi);
    return true;
  }

  currentMode = 0;

  gdi.pushX();
  gdi.dropLine(0.5);
  gdi.addString("", boldLarge, "Deltagare");
  gdi.fillRight();
  gdi.registerEvent("SearchRunner", runnerSearchCB).setKeyCommand(KC_FIND);
  gdi.registerEvent("SearchRunnerBack", runnerSearchCB).setKeyCommand(KC_FINDBACK);

  gdi.addInput("SearchText", getSearchString(), 13, runnerSearchCB, "",
            "Sök på namn, bricka eller startnummer.").isEdit(false)
            .setBgColor(colorLightCyan).ignore(true);
  gdi.dropLine(-0.2);
  //gdi.addButton("Search", "Sök", RunnerCB, "Sök på namn, bricka eller startnummer.");
  gdi.addButton("ShowAll", "Visa alla", RunnerCB).isEdit(false);
  gdi.dropLine(2);
  gdi.popX();

  gdi.fillDown();
  gdi.addListBox("Runners", 206, 420, RunnerCB).isEdit(false).ignore(true);
  gdi.setInputFocus("Runners");
  fillRunnerList(gdi);

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Vacancy)) {
    gdi.fillRight();
    gdi.addButton("Move", "Klassbyte", RunnerCB);
    gdi.addButton("NoStart", "Återbud", RunnerCB);
  }

  gdi.newColumn();
  gdi.fillDown();

  gdi.dropLine(1);
  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("Name", "", 16, 0, "Namn:");

  if (oe->hasBib(true, false)) {
    gdi.addInput("Bib", "", 4, 0, "Nr", "Nummerlapp");
  }
  gdi.popX();
  gdi.dropLine(3);

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    gdi.fillDown();
    gdi.addCombo("Club", 220, 300, 0, "Klubb:");
    oe->fillClubs(gdi, "Club");
    gdi.pushX();
  }

  if (oe->hasTeam()) {
    gdi.fillRight();
    gdi.addInput("Team", "", 16, 0, "Lag:").isEdit(false);
    gdi.disableInput("Team");
    gdi.fillDown();
    gdi.dropLine(0.9);
    gdi.addButton("EditTeam", "...", RunnerCB, "Hantera laget");
    gdi.popX();
  }

  gdi.fillRight();
  gdi.addSelection("RClass", 150, 300, RunnerCB, "Klass:");
  oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterNone);
  gdi.addItem("RClass", lang.tl("Ingen klass"), 0);

  gdi.fillDown();

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy))
    gdi.addInput("Fee", "", 6, 0, "Avgift:");
  else
    gdi.dropLine(3);

  gdi.dropLine(0.4);

  if (oe->hasMultiRunner()) {
    gdi.fillRight();
    gdi.popX();
    gdi.addString("", 0, "Välj lopp:");
    gdi.fillDown();
    gdi.dropLine(-0.2);
    gdi.addSelection("MultiR", 160, 100, RunnerCB);
  }

  gdi.popX();

  int numSL = numShorteningLevels();
  if (numSL > 0)
    gdi.fillRight();

  gdi.addSelection("RCourse", numSL == 0 ? 220 : 180, 300, RunnerCB, "Bana:");
  oe->fillCourses(gdi, "RCourse", true);
  gdi.addItem("RCourse", lang.tl("[Klassens bana]"), 0);

  if (numSL > 0) {
    gdi.fillDown();
    gdi.addSelection("NumShort", 60, 300, RunnerCB, "Avkortning:");
    vector< pair<string, size_t> > data;
    if (numSL == 1) {
      data.push_back(make_pair(lang.tl("Nej"), 0));
      data.push_back(make_pair(lang.tl("Ja"), 1));
    }
    else {
      data.push_back(make_pair(lang.tl("Nej"), 0));
      for (int i = 1; i <= numSL; i++) {
        data.push_back(make_pair(itos(i), i));
      }
    }
    gdi.addItem("NumShort", data);
    gdi.popX();
  }

  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("CardNo", "", 8, RunnerCB, "Bricka:");
  gdi.dropLine(1);
  gdi.addCheckbox("RentCard", "Hyrd", 0, false);

  gdi.dropLine(2);
  gdi.popX();

  gdi.addInput("Start", "", 8, 0, "Starttid:");
  gdi.addInput("Finish", "", 8, 0, "Måltid:");

  const bool timeAdjust = oe->getMeOSFeatures().hasFeature(MeOSFeatures::TimeAdjust);
  const bool pointAdjust = oe->getMeOSFeatures().hasFeature(MeOSFeatures::PointAdjust);

  if (timeAdjust || pointAdjust) {
    gdi.dropLine(3);
    gdi.popX();
    if (timeAdjust) {
      gdi.addInput("TimeAdjust", "", 8, 0, "Tidstillägg:");
    }
    if (pointAdjust) {
      gdi.addInput("PointAdjust", "", 8, 0, "Poängavdrag:");
    }
  }
  gdi.dropLine(3);
  gdi.popX();

  gdi.addInput("Time", "", 8, 0, "Tid:").isEdit(false).ignore(true);
  gdi.disableInput("Time");

  if (oe->hasRogaining()) {
    gdi.addInput("Points", "", 5, 0, "Poäng:").isEdit(false).ignore(true);
    gdi.disableInput("Points");
  }

  gdi.fillDown();
  gdi.addSelection("Status", 100, 80, 0, "Status:", "tooltip_explain_status");
  oe->fillStatus(gdi, "Status");
  gdi.autoGrow("Status");
  gdi.popX();
  gdi.selectItemByData("Status", 0);

  gdi.addString("RunnerInfo", 1, "").setColor(colorRed);

  const bool multiDay = oe->hasPrevStage();

  if (multiDay) {
     gdi.dropLine(1.2);
 
    int xx = gdi.getCX();
    int yy = gdi.getCY();
    gdi.dropLine(0.5);
    gdi.fillDown();
    int dx = int(gdi.getLineHeight()*0.7);
    int ccx = xx + dx;
    gdi.setCX(ccx);
    gdi.addString("", 1, "Resultat från tidigare etapper");
    gdi.dropLine(0.3);
    gdi.fillRight();
 
    gdi.addSelection("StatusIn", 100, 160, 0, "Status:", "tooltip_explain_status");
    oe->fillStatus(gdi, "StatusIn");
    gdi.selectItemByData("Status", 0);
    gdi.addInput("PlaceIn", "", 5, 0, "Placering:");
    int xmax = gdi.getCX() + dx;
    gdi.setCX(ccx);
    gdi.dropLine(3);
    gdi.addInput("TimeIn", "", 5, 0, "Tid:");
    if (oe->hasRogaining()) {
      gdi.addInput("PointIn", "", 5, 0, "Poäng:");
    }
    gdi.dropLine(3);
    RECT rc;
    rc.right = xx;
    rc.top = yy;
    rc.left = max(xmax, gdi.getWidth()-dx);
    rc.bottom = gdi.getCY();

    gdi.addRectangle(rc, colorLightGreen, true, false);
    gdi.dropLine();
    gdi.popX();
  }
  else
     gdi.dropLine(1.0);
 
  gdi.fillRight();
  gdi.addButton("Save", "Spara", RunnerCB, "help:save").setDefault();
  gdi.addButton("Undo", "Ångra", RunnerCB);
  gdi.dropLine(2.2);
  gdi.popX();
  gdi.addButton("Remove", "Radera", RunnerCB);
  gdi.addButton("Add", "Ny deltagare", RunnerCB);
  enableControlButtons(gdi, false, false);
  gdi.fillDown();

  gdi.newColumn();
  int hx = gdi.getCX();
  int hy = gdi.getCY();
  gdi.setCX(hx + gdi.scaleLength(5));

  gdi.dropLine(2.5);
  gdi.addListBox("Punches", 150, 300, PunchesCB, "Stämplingar:").ignore(true);
  gdi.addButton("RemoveC", "Ta bort stämpling >>", RunnerCB);

  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("PTime", "", 8, 0, "", "Stämplingstid");
  gdi.fillDown();
  gdi.addButton("SaveC", "Spara tid", PunchesCB);
  gdi.popX();
  gdi.dropLine();
  int contX = gdi.getCX();
  int contY = gdi.getCY();

  gdi.newColumn();
  gdi.dropLine(2.5);
  gdi.fillDown();
  gdi.addListBox("Course", 140, 300, PunchesCB, "Banmall:").ignore(true);
  gdi.addButton("AddC", "<< Lägg till stämpling", PunchesCB);
  gdi.addButton("AddAllC", "<< Lägg till alla", PunchesCB);

  gdi.synchronizeListScroll("Punches", "Course");
  disablePunchCourse(gdi);

  gdi.setCX(contX);
  gdi.setCY(contY);
  gdi.addString("", fontMediumPlus, "Utskrift");
  
  gdi.dropLine(0.2);
  gdi.fillRight();
  gdi.addButton(gdi.getCX(), gdi.getCY(), gdi.scaleLength(120), "SplitPrint", 
                "Skriv ut sträcktider", RunnerCB, "", false, false).isEdit(true).setExtra(0);
  gdi.addButton("PrintSettings", "...", RunnerCB, "Inställningar").isEdit(true).setExtra(0);

  gdi.dropLine(2.5);
  gdi.setCX(contX);
  gdi.addButton(gdi.getCX(), gdi.getCY(), gdi.scaleLength(120), "SplitPrint", 
                "Skriv ut startbevis", RunnerCB, "", false, false).isEdit(true).setExtra(1);
  gdi.addButton("PrintSettings", "...", RunnerCB, "Inställningar").isEdit(true).setExtra(1);
  gdi.pushY();

  int by = gdi.getHeight();
  int bx = gdi.getWidth();
  RECT box = {hx-gdi.scaleLength(5), hy, bx + gdi.scaleLength(5), by};
  gdi.addString("", hy + gdi.scaleLength(5), hx + gdi.scaleLength(5), fontMediumPlus, "Brickhantering");
  gdi.addRectangle(box, colorLightBlue, true, false).set3D(true).id = "CardRect";

  gdi.fillDown();
  gdi.setCY(gdi.getHeight());
  gdi.setCX(gdi.scaleLength(100));

  //gdi.addString("", 10, "help:41072");

  gdi.registerEvent("LoadRunner", RunnerCB);

  gdi.setOnClearCb(RunnerCB);

  addToolbar(gdi);

  selectRunner(gdi, oe->getRunner(runnerId, 0));
  gdi.refresh();
  return true;
}

void TabRunner::addToolbar(gdioutput &gdi) {

  const int button_w=gdi.scaleLength(130);
  int dx = 2;

  gdi.addButton(dx, 2, button_w, "FormMode",
    "Formulärläge", RunnerCB, "", false, true).fixedCorner();
  gdi.check("FormMode", currentMode==0);
  dx += button_w;

  gdi.addButton(dx, 2, button_w, "TableMode",
            "Tabelläge", RunnerCB, "", false, true).fixedCorner();
  gdi.check("TableMode", currentMode==1);
  dx += button_w;

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::InForest)) {
    gdi.addButton(dx, 2, button_w, "InForest",
              "Kvar-i-skogen", RunnerCB, "", false, true).fixedCorner();
    gdi.check("InForest", currentMode==2);
    dx += button_w;
  }

  gdi.addButton(dx, 2 ,button_w, "Cards",
            "Hantera brickor", RunnerCB, "", false, true).fixedCorner();
  gdi.check("Cards", currentMode==3);
  dx += button_w;

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Vacancy)) {
    gdi.addButton(dx, 2 ,button_w, "Vacancy",
              "Vakanser", RunnerCB, "", false, true).fixedCorner();
    gdi.check("Vacancy", currentMode==4);
    dx += button_w;
  }

  gdi.addButton(dx, 2 ,button_w, "ReportMode",
            "Rapportläge", RunnerCB, "", false, true).fixedCorner();
  gdi.check("ReportMode", currentMode==5);
  dx += button_w;

}

void TabRunner::fillRunnerList(gdioutput &gdi) {
  bool formMode = currentMode == 0;
  timeToFill = GetTickCount();
  oe->fillRunners(gdi, "Runners", !formMode, formMode ? 0 : oEvent::RunnerFilterShowAll);
  timeToFill = GetTickCount() - timeToFill;
  if (formMode) {
    lastSearchExpr = "";
    ((InputInfo *)gdi.setText("SearchText", getSearchString()))->setFgColor(colorGreyBlue);
      lastFilter.clear();
  }
}

bool TabRunner::canSetStart(pRunner r) const {
  pClass pc = r->getTeam() ? r->getTeam()->getClassRef() : r->getClassRef();

  if (pc && pc->getNumStages() > 0) {
    StartTypes st = pc->getStartType(r->getLegNumber());
    if (st != STDrawn)
      return false;
  }
  if (pc && !pc->ignoreStartPunch()) {
    int startType = oPunch::PunchStart;
    if (r->getCourse(false))
      startType = r->getCourse(false)->getStartPunchType();

    pCard c = r->getCard();
    if (c && c->getPunchByType(startType))
      return false;
  }
  return true;
}

bool TabRunner::canSetFinish(pRunner r) const {
  pCard c = r->getCard();
  int finishPunch = oPunch::PunchFinish;
  if (r->getCourse(false))
    finishPunch = r->getCourse(false)->getFinishPunchType();
  if (c && c->getPunchByType(finishPunch))
    return false;

  return true;
}

pRunner TabRunner::warnDuplicateCard(int cno, pRunner r)  {
  pRunner warnCardDupl = 0;

  if (!r->getCard()) {
    vector<pRunner> allR;
    oe->getRunners(0, 0, allR, false);
    for (size_t k = 0; k < allR.size(); k++) {
      if (!r->canShareCard(allR[k], cno)) {
        warnCardDupl = allR[k];
        break;
      }
    }
  }
  return warnCardDupl;
}

void TabRunner::warnDuplicateCard(gdioutput &gdi, int cno, pRunner r) {
  pRunner warnCardDupl = warnDuplicateCard(cno, r);

  InputInfo &cardNo = dynamic_cast<InputInfo &>(gdi.getBaseInfo("CardNo"));
  if (warnCardDupl) {
    cardNo.setBgColor(colorLightRed);
    gdi.updateToolTip("CardNo", "Brickan används av X.#" + warnCardDupl->getCompleteIdentification());
    cardNo.refresh();
  }
  else {
    if (cardNo.getBgColor() != colorDefault) {
      cardNo.setBgColor(colorDefault);
      gdi.updateToolTip("CardNo", "");
      cardNo.refresh();
    }
  }
}

int TabRunner::numShorteningLevels() const {
  vector<pCourse> allCrs;
  oe->getCourses(allCrs);
  set<int> touch;
  map<int, int> known;
  int res = 0;
  for (size_t k = 0; k < allCrs.size(); k++) {
    pCourse sh = allCrs[k]->getShorterVersion();
    touch.clear();
    int count = 0;
    while (sh && !touch.count(sh->getId())) {
      count++;
      map<int, int>::iterator r = known.find(sh->getId());
      if (r != known.end()) {
        count += r->second;
        break;
      }
      touch.insert(sh->getId());
      sh = sh->getShorterVersion();
    }
    known[allCrs[k]->getId()] = count;
    res = max(res, count);
  }
  return res;
}

void TabRunner::updateNumShort(gdioutput &gdi, pCourse crs, pRunner r) {
  if (gdi.hasField("NumShort")) {
    if (crs && crs->getShorterVersion()) {
      gdi.enableInput("NumShort");
      if (r)
        gdi.selectItemByData("NumShort", r->getNumShortening());
      else
        gdi.selectFirstItem("NumShort");
    }
    else {
      gdi.disableInput("NumShort");
      gdi.selectFirstItem("NumShort");
    }
  }
}

void TabRunner::clearCompetitionData() {
  inputId = 0;
  lastRace=0;
  currentMode = 0;
  runnerId=0;
  timeToFill = 0;
  ownWindow = false;
  listenToPunches = false;
}

void TabRunner::autoGrowCourse(gdioutput &gdi) {
  ListBoxInfo &crsCtrl = dynamic_cast<ListBoxInfo &>(gdi.getBaseInfo("Course"));
  int wInit = crsCtrl.getWidth();
  if (gdi.autoGrow("Course")) {
    RectangleInfo &rc = gdi.getRectangle("CardRect");
    int wAfter = crsCtrl.getWidth();
    rc.changeDimension(gdi, wAfter - wInit, 0);
    gdi.refresh();
  }
}
