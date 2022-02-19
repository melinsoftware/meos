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
#include "autocomplete.h"
#include "RunnerDB.h"

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
    runnerId = 0;
    gdi.setText("Name", L"");
    gdi.setText("Bib", L"");
    gdi.selectItemByData("RCourse", 0);
    updateNumShort(gdi, 0, 0);
    //Don't clear club and class


    gdi.setText("CardNo", L"");
    gdi.enableInput("Start");
    gdi.setText("Start", makeDash(L"-"));
    gdi.enableInput("Finish");
    gdi.setText("Finish", makeDash(L"-"));

    gdi.setText("Time", makeDash(L"-"));
    gdi.setText("Points", L"");
    gdi.selectItemByData("Status", 0);

    gdi.clearList("Punches");
    gdi.clearList("Course");

    gdi.selectItemByData("Runners", -1);
    gdi.setInputFocus("Name", true);
    if (gdi.hasWidget("MultiR")) {
      gdi.clearList("MultiR");
      gdi.disableInput("MultiR");
    }
    gdi.enableEditControls(false);
    enableControlButtons(gdi, false, false);
    disablePunchCourse(gdi);

    if (gdi.hasWidget("EditTeam"))
      gdi.disableInput("EditTeam");
    gdi.setText("RunnerInfo", L"", true);

    gdi.setText("TimeAdjust", makeDash(L"-"));
    gdi.setText("PointAdjust", L"");

    if (gdi.hasWidget("StatusIn")) {
      gdi.selectFirstItem("StatusIn");
      gdi.setText("PlaceIn", L"");
      gdi.setText("TimeIn", makeDash(L"-"));
      if (gdi.hasWidget("PointIn"))
        gdi.setText("PointIn", L"");
    }

    return;
  }

  gdi.enableEditControls(true);
  disablePunchCourse(gdi);

  pRunner parent = r->getMultiRunner(0);

  r->synchronizeAll();
  vector<int> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
  
  gdi.selectItemByData("Runners", parent->getId());

  runnerId = r->getId();

  gdi.setText("Name", r->getNameRaw());
  wstring bib = r->getBib();

  if (gdi.hasWidget("Bib")) {
    gdi.setText("Bib", bib);
    bool controlBib = r->getTeam() == 0 || (r->getClassRef(true) && r->getClassRef(true)->getBibMode() == BibFree);
    gdi.setInputStatus("Bib", controlBib);
  }

  auto *ci = gdi.setText("Club", r->getClub());
  if (ci) {
    ci->setExtra(r->getClubId());
  }
  oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterNone);
  gdi.addItem("RClass", lang.tl("Ingen klass"), 0);
  gdi.selectItemByData("RClass", r->getClassId(true));

  if (gdi.hasWidget("EditTeam")) {
    gdi.setInputStatus("EditTeam", r->getTeam() != 0);

    if (r->getTeam()) {
      gdi.setText("Team", r->getTeam()->getName());
    }
    else
      gdi.setText("Team", L"");
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

  wstring out;
  for (size_t k = 0; k < delta.size(); k++) {
    out += itow(place[k]);
    if (k < placeAcc.size())
      out += L" (" + itow(placeAcc[k]) + L")";

    if (after[k] > 0)
      out += L" +" + getTimeMS(after[k]);

    if (k < afterAcc.size() && afterAcc[k]>0)
      out += L" (+" + getTimeMS(afterAcc[k]) + L")";

    if (delta[k] > 0)
      out += L" B: " + getTimeMS(delta[k]);

    out += L" | ";

  }
  gdi.restore("fantom", false);
  gdi.setRestorePoint("fantom");
  gdi.addStringUT(0, out);
  gdi.refresh();
#endif

  if (gdi.hasWidget("MultiR")) {
    int numMulti = parent->getNumMulti();
    if (numMulti == 0) {
      gdi.clearList("MultiR");
      gdi.disableInput("MultiR");
      lastRace = 0;
    }
    else {
      gdi.clearList("MultiR");
      gdi.enableInput("MultiR");

      for (int k = 0; k < numMulti + 1; k++) {
        gdi.addItem("MultiR", lang.tl("Lopp X#" + itos(k + 1)), k);
      }
      gdi.selectItemByData("MultiR", r->getRaceNo());
    }
  }
  oe->fillCourses(gdi, "RCourse", true);
  wstring crsName;
  if (r->getCourse(false))
    crsName = r->getCourse(false)->getName();

  wstring courseType = lang.tl(" [Klassens bana]");
  pClass cClass = r->getClassRef(false);
  if (cClass && (cClass->hasCoursePool() || r->getClassRef(true)->hasCoursePool())) {
    if (!crsName.empty())
      courseType = L", ... ";
    courseType += L" [" + lang.tl("Banpool") + L"]";
  }
  else if (crsName.empty())
    crsName += L" ";

  gdi.addItem("RCourse", crsName + courseType, 0);
  gdi.selectItemByData("RCourse", r->getCourseId());
  updateNumShort(gdi, r->getCourse(false), r);

  int cno = r->getCardNo();
  gdi.setText("CardNo", cno > 0 ? itow(cno) : L"");

  warnDuplicateCard(gdi, cno, r);

  gdi.check("RentCard", r->isHiredCard());
  bool hasFee = gdi.hasWidget("Fee");

  if (hasFee)
    gdi.setText("Fee", oe->formatCurrency(parent->getDI().getInt("Fee")));

  bool canEditClass = parent == r || (parent->getClassRef(false) &&
                                      parent->getClassRef(false)->getQualificationFinal());

  gdi.setInputStatus("RClass", canEditClass);
  gdi.enableInput("CardNo");
  gdi.enableInput("RentCard");
  if (hasFee)
    gdi.setInputStatus("Fee", parent == r);

  if(gdi.hasWidget("Club"))
    gdi.setInputStatus("Club", parent == r);

  enableControlButtons(gdi, true, r->isVacant());

  //gdi.setText("Start", r->getStartTimeS());
  gdi.setInputStatus("Start", canSetStart(r));

  //gdi.setText("Finish", r->getFinishTimeS());
  gdi.setInputStatus("Finish", canSetFinish(r));

  //gdi.setText("Time", r->getRunningTimeS(true));
  //gdi.setText("Points", itow(r->getRogainingPoints(false)));

  //gdi.selectItemByData("Status", r->getStatus());
  //gdi.setText("RunnerInfo", lang.tl(r->getProblemDescription()), true);
  updateStatus(gdi, r);

  if (gdi.hasWidget("StatusIn")) {
    gdi.selectItemByData("StatusIn", r->getInputStatus());
    int ip = r->getInputPlace();
    if (ip > 0)
      gdi.setText("PlaceIn", ip);
    else
      gdi.setText("PlaceIn", makeDash(L"-"));

    gdi.setText("TimeIn", r->getInputTimeS());
    if (gdi.hasWidget("PointIn"))
      gdi.setText("PointIn", r->getInputPoints());
  }

  pCard pc = r->getCard();

  pCourse pcourse = r->getCourse(true);

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

  gdioutput *gdi_settings = getExtraWindow("ecosettings", false);
  if (gdi_settings) {
    TabRunner &dst = dynamic_cast<TabRunner&>(*gdi_settings->getTabs().get(TabType::TRunnerTab));
    dst.loadEconomy(*gdi_settings, *r);
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
  wstring expr;
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
        gdi.addTimeoutMilli(500, "Search", runnerSearchCB).setData(inputId, expr);
      }
    }
  }
  else if (type == GUI_TIMER) {

    TimerInfo &ti = *(TimerInfo *)(data);

    if (inputId != ti.getData())
      return 0;

    expr = ti.getDataString();
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
      ((InputInfo *)gdi.setText("SearchText", L""))->setFgColor(colorDefault);
    }
  }

  if (showNow) {
    unordered_set<int> filter;

    if (type == GUI_TIMER)
      gdi.setWaitCursor(true);

    if (filterMore) {

      oe->findRunner(expr, 0, lastFilter, filter);
      lastSearchExpr = expr;
      // Filter more
      if (filter.empty()) {
        vector< pair<wstring, size_t> > runners;
        runners.push_back(make_pair(lang.tl(L"Ingen matchar 'X'#" + expr), -1));
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

      vector< pair<wstring, size_t> > runners;
      oe->fillRunners(runners, !formMode, formMode ? 0 : oEvent::RunnerFilterShowAll, filter);

      if (filter.size() == runners.size()){
      }
      else if (filter.empty()) {
        runners.clear();
        runners.push_back(make_pair(lang.tl(L"Ingen matchar 'X'#" + expr), -1));
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
  oe->synchronizeList(oListId::oLCardId);
  TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
  tsi.storedInfo.clear();

  bool create=(runnerId==0);

  if (create)
    return 0;

  wstring name=gdi.getText("Name");

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
    if (gdi.ask(L"Vill du skapa en ny klass?")) {
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
    if (!r->matchName(name) || (r->getClubId() != clubId && clubId != 0))
      r->updateFromDB(name, clubId, classId, cardNo, r->getBirthYear(), false);
  }

  if (cardNoChanged && cardNo>0) {
    pRunner warnCardDupl = warnDuplicateCard(cardNo, r);
    if (warnCardDupl) {
      gdi.alert(L"Varning: Brickan X används redan av Y.#" + itow(cardNo) + L"#" + warnCardDupl->getCompleteIdentification()); 
    }
  }

  if (r) {
    runnerId=r->getId();
    RunnerStatus originalStatus = r->getStatus();
    r->setName(name, true);

    if (dynamic_cast<InputInfo &>(gdi.getBaseInfo("PTime")).changed()) {
      savePunchTime(r, gdi);
    }

    bool noSetStatus = false;
    if (cardNo > 0 && r->getCard() && 
      r->getCard()->getCardNo() != cardNo && r->getCardNo() != cardNo) {
        if (gdi.ask(L"Vill du koppla isär X från inläst bricka Y?#" + r->getName() + 
                    L"#" + r->getCard()->getCardNoString())) {
        r->setStatus(StatusUnknown, true, oBase::ChangeType::Update, false);
        r->setCard(0);
        r->setFinishTime(0);
        r->synchronize(true);
        gdi.setText("Finish", makeDash(L"-"));
        noSetStatus = true;
      }
    }

    if (cardNo > 0 && cardNo != r->getCardNo() && oe->hasNextStage()) {
      if (gdi.ask(L"Vill du använda den nya brickan till alla etapper?")) {
        r->setTransferCardNoNextStage(true);
      }
    }

    if (r->getCardNo() != cardNo) {
      r->setCardNo(cardNo, true);
    }
    const bool hireChecked = gdi.isChecked("RentCard");
    const bool hireState = r->isHiredCard();
    if (hireChecked && !hireState) {
      r->getDI().setInt("CardFee", oe->getBaseCardFee());
    }
    else if (!hireChecked && hireState) {
      r->getDI().setInt("CardFee", 0);
    }

    if (gdi.hasWidget("Fee"))
      r->getDI().setInt("Fee", oe->interpretCurrency(gdi.getText("Fee")));

    gdioutput *gdi_settings = getExtraWindow("ecosettings", false);
    if (gdi_settings) {
      TabRunner &dst = dynamic_cast<TabRunner&>(*gdi_settings->getTabs().get(TabType::TRunnerTab));
      dst.getEconomyHandler(*r)->save(*gdi_settings);
    }

    r->setStartTimeS(gdi.getText("Start"));
    r->setFinishTimeS(gdi.getText("Finish"));

    if (gdi.hasWidget("NumShort")) {
      r->setNumShortening(gdi.getSelectedItem("NumShort").first);
    }

    if (gdi.hasWidget("TimeAdjust")) {
      int t = convertAbsoluteTimeMS(gdi.getText("TimeAdjust"));
      if (t != NOTIME)
        r->setTimeAdjustment(t);
    }
    if (gdi.hasWidget("PointAdjust")) {
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
      else if (r->getTeam()->getClassId(true) != classId && r->getClassId(true) != classId) {
        gdi.alert("Deltagarens klass styrs av laget.");
        classId = r->getTeam()->getClassId(false);
      }
    }
    
    bool readStatusIn = true;
    if (r->getClassId(true) != classId && r->getInputStatus() != StatusNotCompetiting && r->hasInputData()) {
      if (gdi.ask(L"Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) {
        r->resetInputData();
        readStatusIn = false;
      }
    }

    r->setClassId(classId, true);

    if (gdi.hasWidget("Bib")) {
      const wstring &bib = gdi.getText("Bib");
      wchar_t pat[32];
      int num = oClass::extractBibPattern(bib, pat);
      bool lockedForking = r->getClassRef(true) && r->getClassRef(true)->lockedForking();

      r->setBib(bib, num, num>0 && !lockedForking);
    }

    int crsId = gdi.getSelectedItem("RCourse").first;
    if (crsId > 0 && r->getCourseId() != -1) {
      pClass cls = r->getClassRef(true);
      pCourse crs = oe->getCourse(crsId);
      vector<pCourse> courses;
      if (cls && crs) {
        cls->getCourses(r->getLegNumber(), courses);
        set<int> crsIds;
        for (auto c : courses)
          crsIds.insert(c->getId());

        if (crsIds.count(crsId) == 0) {
          vector<pRunner> clsRunner;
          oe->getRunners(cls->getId(), -1, clsRunner, false);
          bool ok = clsRunner.size() < 2;
          for (auto cr : clsRunner) {
            if (cr->getCourseId() > 0 && !crsIds.count(cr->getCourseId())) {
              ok = true;
              break;
            }
          }

          if (!ok) {
            if (!gdi.ask(L"ask:usecourseinclass"))
              crsId = 0;
          }
        }
      }
    }


    r->setCourseId(crsId);

    RunnerStatus sIn = (RunnerStatus)gdi.getSelectedItem("Status").first;
    bool checkStatus = sIn != originalStatus;
    if (r->getStatus() != sIn && !noSetStatus) {
      r->setStatus(sIn, true, oBase::ChangeType::Update);
    }
    r->addClassDefaultFee(false);
    vector<int> mp;
    r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);

    if (r->getClassId(true) != classId && r->getClassId(false) != classId) {
      gdi.alert("Deltagarens klass styrs av laget.");
    }

    if (checkStatus && sIn != r->getStatus())
      gdi.alert("Status matchar inte data i löparbrickan.");

    if (gdi.hasWidget("StatusIn") && readStatusIn) {
      r->setInputStatus(RunnerStatus(gdi.getSelectedItem("StatusIn").first));
      r->setInputPlace(gdi.getTextNo("PlaceIn"));
      r->setInputTime(gdi.getText("TimeIn"));
      if (gdi.hasWidget("PointIn"))
        r->setInputPoints(gdi.getTextNo("PointIn"));
    }

    r->synchronizeAll();
    
    if (r->getClassRef(false) && r->getClassRef(false)->hasClassGlobalDependence()) {
      set<int> cls;
      cls.insert(r->getClassId(false));
      oe->reEvaluateAll(cls, false);
    }

    if (r->getClassRef(false))
      r->getClassRef(false)->updateFinalClasses(r, false);
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
      wstring searchText = gdi.getText("SearchText");
      bool formMode = currentMode == 0;
      unordered_set<int> foo;
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
      gdioutput *gdi_new = createExtraWindow(uniqueTag("kiosk"), makeDash(L"MeOS - ") + oe->getName(), gdi.getWidth() + 64 + gdi.scaleLength(120));
      if (gdi_new) {
        TabRunner &tr = dynamic_cast<TabRunner &>(*gdi_new->getTabs().get(TRunnerTab));
        tr.currentMode = currentMode;
        tr.runnerId = runnerId;
        tr.ownWindow = true;
        tr.loadPage(*gdi_new);
      }
    }
    else if (bi.id == "Kiosk") {
      if (gdi.ask(L"ask:kiosk")) {
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
      for (size_t k=0; k<unknown.size(); k++) {
        if (unknown[k]->getStatus()==StatusUnknown) {
          unknown[k]->setStatus(StatusDNS, true, oBase::ChangeType::Update);
          unknown[k]->setFlag(oAbstractRunner::FlagAutoDNS, true);
          unknown[k]->synchronize(true);
        }
      }
      //Reevaluate and synchronize all
      oe->reEvaluateAll(set<int>(), true);
      clearInForestData();
      showInForestList(gdi);
    }
    else if (bi.id == "UndoSetDNS") {
      vector<pRunner> runners;
      oe->getRunners(0, 0, runners, true);
      for (pRunner r : runners) {
        if (r->getStatus() == StatusDNS && r->hasFlag(oAbstractRunner::FlagAutoDNS)) {
          r->setStatus(StatusUnknown, true, oBase::ChangeType::Update);
          r->setFlag(oAbstractRunner::FlagAutoDNS, false);
          r->synchronize(true);
        }
      }
      //Reevaluate and synchronize all
      oe->reEvaluateAll(set<int>(), true);
      clearInForestData();
      showInForestList(gdi);
    }
    else if (bi.id=="SetUnknown") {
      for (size_t k=0; k<known_dns.size(); k++) {
        if (known_dns[k]->getStatus()==StatusDNS) {
          known_dns[k]->setStatus(StatusUnknown, true, oBase::ChangeType::Update);
          known_dns[k]->synchronize(true);
        }
      }
      //Reevaluate and synchronize all
      oe->reEvaluateAll(set<int>(), true);
      clearInForestData();
      showInForestList(gdi);
    }
    else if (bi.id == "RemoveVacant") {
      if (gdi.ask(L"Vill du radera alla vakanser från tävlingen?")) {
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

      gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
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
        wstring name = gdi.getText("Name");
        pRunner r = oe->getRunner(runnerId, 0);
        if (!name.empty() && r && r->getName() != name && r->getNameRaw() != name) {
          if (gdi.ask(L"Vill du lägga till deltagaren 'X'?#" + name)) {
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
      int clsId = lbi.data;
      if (clsId > 0) {
        pClass tCls = oe->getClass(clsId);
        if (tCls && tCls->getParentClass() != nullptr && tCls->getParentClass() != tCls) {
          clsId = tCls->getParentClass()->getId();
        }
      }
      if (clsId <= 0)
        clsId = oe->getFirstClassId(false);

      r->setClassId(clsId, true);

      fillRunnerList(gdi);
      oe->fillClubs(gdi, "Club");
      selectRunner(gdi, r);
      gdi.setInputFocus("Name", true);
    }
    else if (bi.id=="Remove") {
      if (!runnerId)
        return 0;

      if (gdi.ask(L"Vill du verkligen ta bort löparen?")) {
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
    else if (bi.id == "Economy") {
      if (!runnerId)
        return 0;
      pRunner r = oe->getRunner(runnerId, 0);
      if (getExtraWindow("ecosettings", true) == 0) {
        gdioutput *settings = createExtraWindow("ecosettings", L"Economy", gdi.scaleLength(550), gdi.scaleLength(350), true);
        TabRunner &dst = dynamic_cast<TabRunner&>(*settings->getTabs().get(TabType::TRunnerTab));
        dst.loadEconomy(*settings, *r);
      }
    }
    else if (bi.id=="NoStart") {
      if (!runnerId)
        return 0;
      pRunner r = oe->getRunner(runnerId, 0);
      r = r->getMultiRunner(0);

      if (r && gdi.ask(L"Bekräfta att deltagaren har lämnat återbud.")) {
        if (r->getStartTime()>0) {
          pRunner newRunner = oe->addRunnerVacant(r->getClassId(true));
          newRunner->cloneStartTime(r);
          newRunner->setStartNo(r->getStartNo(), oBase::ChangeType::Update);
          if (r->getCourseId())
            newRunner->setCourseId(r->getCourseId());
          newRunner->synchronizeAll();
        }

        for (int k=0;k<r->getNumMulti()+1; k++) {
          pRunner rr = r->getMultiRunner(k);
          if (rr) {
            rr->setStartTime(0, true, oBase::ChangeType::Update);
            //rr->setStartNo(0, false);
            rr->setStatus(StatusCANCEL, true, oBase::ChangeType::Update);
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
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
      r->synchronize();
      card->fillPunches(gdi, "Punches", r->getCourse(true));

      gdi.setText("Time", r->getRunningTimeS(true));
      gdi.selectItemByData("Status", r->getStatus());
    }
    else if (bi.id=="Check") {
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo &ii = *(InputInfo *)data;

    if (oe->useRunnerDb() && ii.id == "Name") {
      auto &db = oe->getRunnerDatabase();
      bool show = false;
      if (ii.text.length() > 1) {
        auto dbClub = extractClub(oe, gdi);
        auto rw = db.getRunnerSuggestions(ii.text, dbClub ? dbClub->getId() : 0, 10);
        if (!rw.empty()) {
          auto &ac = gdi.addAutoComplete(ii.id);
          ac.setAutoCompleteHandler(this);

          ac.setData(TabSI::getRunnerAutoCompelete(db, rw, dbClub));
          ac.show();
          show = true;
        }
      }
      if (!show) {
        gdi.clearAutoComplete(ii.id);
      }
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

          if (ii.changedInput() && oe->hasHiredCardData()) {
            gdi.check("RentCard", oe->isHiredCard(cardNo));
          }
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
      else {
        pRunner r = oe->getRunner(runnerId, 0);
        gdioutput *gdi_settings = getExtraWindow("ecosettings", false);
        if (gdi_settings) {
          TabRunner &dst = dynamic_cast<TabRunner&>(*gdi_settings->getTabs().get(TabType::TRunnerTab));
          dst.getEconomyHandler(*r)->save(*gdi_settings);
        }
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
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TRunnerTab, 0);
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

        wstring crsName;
        if (crsToUse) {
          crsToUse->fillCourse(gdi, "Course");
          autoGrowCourse(gdi);
          crsName = crsToUse->getName() + L" ";
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
        oe->getRunnersByCardNo(ei.getData(), true, oEvent::CardLookupProperty::Any, rs);
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
  else if (type == GUI_COMBOCHANGE) {
    ListBoxInfo &combo = *(ListBoxInfo *)(data);
    bool show = false;
    if (oe->useRunnerDb() && combo.id == "Club" && combo.text.length() > 1) {
      auto clubs = oe->getRunnerDatabase().getClubSuggestions(combo.text, 20);
      if (!clubs.empty()) {
        auto &ac = gdi.addAutoComplete(combo.id);
        ac.setAutoCompleteHandler(this);
        vector<AutoCompleteRecord> items;
        for (auto club : clubs)
          items.emplace_back(club->getDisplayName(), club->getName(), club->getId());

        ac.setData(items);
        ac.show();
        show = true;
      }
    }
    if (!show) {
      gdi.clearAutoComplete(combo.id);
    }
  }
  else if (type==GUI_CLEAR) {
    gdioutput *gdi_settings = getExtraWindow("ecosettings", false);
    if (gdi_settings) 
      gdi_settings->closeWindow();
    
    if (runnerId>0 && currentMode == 0)
      save(gdi, runnerId, true);
    
    return true;
  }
  else if (type == GUI_LINK) {
    int id = static_cast<TextInfo*>(data)->getExtraInt();
    oRunner *vacancy = oe->getRunner(id, 0);

    if (vacancy==0 || vacancy->getClassId(false)==0)
      return -1;

    pRunner r = oe->getRunner(runnerId, 0);

    vacancy->synchronize();
    r->synchronize();

    if (r==0)
      return -1;

    wchar_t bf[1024];
    swprintf_s(bf, lang.tl("Bekräfta att %s byter klass till %s.").c_str(),
                         r->getName().c_str(), vacancy->getClass(true).c_str());
    if (gdi.ask(wstring(L"#") + bf)) {

      vacancy->synchronize();
      if (!vacancy->isVacant())
        throw meosException("Starttiden är upptagen.");

      oRunner temp(oe, 0);
      temp.setTemporary();
      temp.setBib(r->getBib(), 0, false);
      temp.setStartNo(r->getStartNo(), oBase::ChangeType::Update);
      temp.setClassId(r->getClassId(true), true);
      temp.apply(oBase::ChangeType::Update, nullptr);
      temp.cloneStartTime(r);

      r->setClassId(vacancy->getClassId(true), true);
      // Remove or create multi runners
      r->createMultiRunner(true, true);
      r->apply(oBase::ChangeType::Quiet, nullptr);
      r->cloneStartTime(vacancy);
      r->setBib(vacancy->getBib(), 0, false);
      r->setStartNo(vacancy->getStartNo(), oBase::ChangeType::Update);

      if (oe->hasPrevStage()) {
        if (gdi.ask(L"Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) 
          r->resetInputData();
      }

      vacancy->setClassId(temp.getClassId(true), true);
      // Remove or create multi runners
      vacancy->createMultiRunner(true, true);
      vacancy->apply(oBase::ChangeType::Update, nullptr);
      vacancy->cloneStartTime(&temp);
      vacancy->setBib(temp.getBib(), 0, false);
      vacancy->setStartNo(temp.getStartNo(), oBase::ChangeType::Update);

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
  gdi.addTable(oCard::getTable(oe), gdi.getCX(), gdi.getCY() + gdi.scaleLength(15));
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

    if (r == 0)
      return -1;

    r->synchronize();
    if (!r->isVacant())
      throw std::exception("Starttiden är upptagen.");

    wstring name = gdi.getText("Name");

    if (name.empty())
      throw std::exception("Alla deltagare måste ha ett namn.");

    int cardNo = gdi.getTextNo("CardNo");

    if (cardNo != r->getCardNo() && oe->checkCardUsed(gdi, *r, cardNo))
      return 0;

    int clubId = 0;
    wstring club;
    int birthYear = 0;

    if (gdi.hasWidget("Club")) {
      club = gdi.getText("Club");
      pClub pc = oe->getClubCreate(0, club);
      if (pc)
        clubId = pc->getId();
    }
    r->updateFromDB(name, clubId, r->getClassId(false), cardNo, birthYear, false);
    r->setName(name, true);
    r->setCardNo(cardNo, true);
    r->setClub(club);

    int fee = 0;
    if (gdi.hasWidget("Fee")) {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("Fee", lbi) && lbi.data == -1) {
        lastFee = L"@";
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
    r->getDI().setInt("EntryTime", getLocalAbsTime());
    r->addClassDefaultFee(false);
    int cardFee = 0;

    if (gdi.isChecked("RentCard")) {
      cardFee = oe->getBaseCardFee();
      r->getDI().setInt("CardFee", cardFee);
    }
    else
      r->getDI().setInt("CardFee", 0);
    
    if (cardFee < 0)
      cardFee = 0;
    fee = r->getDCI().getInt("Fee");
    
    TabSI::writePayMode(gdi, fee + cardFee, *r);  

    if (gdi.hasWidget("AllStages")) {
      r->setFlag(oRunner::FlagTransferSpecified, true);
      r->setFlag(oRunner::FlagTransferNew, gdi.isChecked("AllStages"));
    }

    if (oe->hasPrevStage()) {
      if (gdi.ask(L"Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) 
        r->resetInputData();
    }

    r->synchronizeAll();
    
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    
    // Print start certificate
    tsi.generateStartInfo(gdi, *r);

    showVacancyList(gdi, "", r->getClassId(true));
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
      tsi.storedInfo.storedFee = L"@";
      // Use class default fee
    }
    else
      tsi.storedInfo.storedFee = gdi.getText("Fee", true);
    
    tsi.storedInfo.allStages = gdi.isChecked("AllStages");
    tsi.storedInfo.rentState = gdi.isChecked("RentCard");
    tsi.storedInfo.hasPaid = gdi.isChecked("Paid");
    tsi.storedInfo.payMode =  gdi.hasWidget("PayMode") ? gdi.getSelectedItem("PayMode").first : 0;
    return 1;
  }
  return 0;
}

void TabRunner::setCardNo(gdioutput &gdi, int cardNo) {
  pRunner db_r=oe->dbLookUpByCard(cardNo);

  if (cardNo > 0 && oe->hasHiredCardData()) {
    gdi.check("RentCard", oe->isHiredCard(cardNo));
  }

  if (db_r) {
    gdi.setText("Name", db_r->getName());
    gdi.setText("Club", db_r->getClub());
  }
}

void TabRunner::showRunnerReport(gdioutput &gdi) {
  gdi.clearPage(true);
  currentMode = 5;

  if (!ownWindow && !oe->isReadOnly())
    addToolbar(gdi);
  else if (oe->isReadOnly())
    gdi.addString("", fontLarge, makeDash(L"MeOS - Resultatkiosk")).setColor(colorDarkBlue);

  gdi.dropLine();

  gdi.pushX();
  gdi.fillRight();

  gdi.addSelection("ReportRunner", 300, 300, RunnerCB);
  oe->fillRunners(gdi, "ReportRunner", true, oEvent::RunnerFilterShowAll | oEvent::RunnerCompactMode);
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
  
  if (runnerId > 0) {
    runnersToReport.resize(1);
    runnersToReport[0] = make_pair(runnerId, false);
  }
  generateRunnerReport(*oe, gdi, runnersToReport);

  if (runnersToReport.size() == 1)
    runnerId = runnersToReport[0].first;
}

 void TabRunner::generateRunnerReport(oEvent &oe, gdioutput &gdi, vector<pair<int, bool>> &runnersToReport) {
   oe.synchronizeList({ oListId::oLRunnerId, oListId::oLTeamId, oListId::oLPunchId });
  gdi.fillDown();

  cTeam t = 0;
  set<int> clsSet;
  for (size_t k = 0; k < runnersToReport.size(); k++) {
    pRunner r = oe.getRunner(runnersToReport[k].first, 0);
    if (!r)
      continue;
    clsSet.insert(r->getClassId(true));
    if (r && r->getTeam()) {
      pClass cls = r->getClassRef(true);
      if (cls && cls->getClassType() != oClassRelay)
        continue;

      if (t == 0)
        t = r->getTeam();
    }
  }
  oe.calculateResults(clsSet, oEvent::ResultType::PreliminarySplitResults, true);
  oe.calculateResults(clsSet, oEvent::ResultType::ClassResult);

  if (t == 0) {
    for (size_t k = 0; k < runnersToReport.size(); k++)
      runnerReport(oe, gdi, runnersToReport[k].first, runnersToReport[k].second);
  }
  else {
    oe.calculateTeamResults(clsSet, oEvent::ResultType::ClassResult);

    set<int> selectedRunners;
    bool selHasRes = false;
    for (size_t k = 0; k < runnersToReport.size(); k++) {
      selectedRunners.insert(runnersToReport[k].first);
      pRunner r = oe.getRunner(runnersToReport[k].first, 0);
      if (r && r->hasOnCourseResult())
        selHasRes = true;
    }

    wstring tInfo = t->getName();
    if (t->statusOK(true)) {
      tInfo += L", "  + t->getRunningTimeS(true) +  lang.tl(", Placering: ") + t->getPlaceS();
      if (t->getTimeAfter(-1) > 0)
        tInfo += L", +" + formatTime(t->getTimeAfter(-1));
    }
    else if (t->getStatusComputed() != StatusUnknown) {
      tInfo += L" " + t->getStatusS(true, true);
    }

    gdi.addStringUT(fontMediumPlus, t->getClass(true));
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
          runnerReport(oe, gdi, r->getId(), !selected);
        }
        else {
          runnerReport(oe, gdi, r->getId(), !nextSelected);
        }

        visitedSelected |= selected;
      }
    }
  }
}

void TabRunner::runnerReport(oEvent &oe, gdioutput &gdi, int id, bool compact) {
  pRunner r = oe.getRunner(id, 0);
  if (!r || ! r->getClassRef(false))
    return;
  
  gdi.pushX();
  gdi.fillDown();
  if (r->getTeam() == 0) {
    gdi.addStringUT(fontMediumPlus, r->getClass(true));
    gdi.addStringUT(boldLarge, r->getCompleteIdentification());
  }
  else {
    wstring s;
    if (r->getTeam())
      s += r->getClassRef(false)->getLegNumber(r->getLegNumber());

    s += L": " + r->getName();
    gdi.addStringUT(boldText, s);
  }

  wstring str;
  if (r->getTeam() == 0) {
    str = oe.formatListString(lRunnerTimeStatus,  r);
  }
  else {
    str = oe.formatListString(lTeamLegTimeStatus,  r);
    str += L" (" + oe.formatListString(lRunnerTimeStatus,  r) + L")";
  }
  
  gdi.dropLine(0.3);

  if (r->statusOK(true)) {
    int total, finished,  dns;
    r->getClassRef(true)->getNumResults(r->getLegNumber(), total, finished, dns);

    if (r->getTeam() == 0) {
      gdi.addString("", fontMediumPlus, L"Tid: X, nuvarande placering Y/Z.#" + str + L"#" + r->getPlaceS() + L"#" + itow(finished));
    }
    else {
      int place = r->getTeam()->getLegPlace(r->getLegNumber(), false);
      if (place > 0 && place < 10000) {
        gdi.addString("", fontMediumPlus, L"Tid: X, nuvarande placering Y/Z.#" + str + L"#" + itow(place) + L"#" + itow(finished));
      }
      else {
        gdi.addStringUT(fontMediumPlus, str).setColor(colorRed);
      }
    }
  }
  else if (r->getStatusComputed() != StatusUnknown) {
    gdi.addStringUT(fontMediumPlus, str).setColor(colorRed);
  }

  gdi.popX();
  gdi.fillRight();

  if (r->getStartTime() > 0)
    gdi.addString("", fontMedium, L"Starttid: X  #" + r->getStartTimeCompact());

  if (r->getFinishTime() > 0)
    gdi.addString("", fontMedium, L"Måltid: X  #" + r->getFinishTimeS());

  const wstring &after = oe.formatListString(lRunnerTimeAfter, r);
  if (!after.empty()) {
    gdi.addString("", fontMedium, L"Tid efter: X  #" + after);
  }

  const wstring &lost = oe.formatListString(lRunnerLostTime, r);
  if (!lost.empty()) {
    gdi.addString("", fontMedium, L"Bomtid: X  #" + lost).setColor(colorDarkRed);
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
      wstring name = crs->getControlOrdinal(k);
      if ( k < end) {
        pControl ctrl = crs->getControl(k);
        if (ctrl && ctrl->getFirstNumber() > 0)
          name += L" (" + itow(ctrl->getFirstNumber()) + L")";
        gdi.addString("", yp, cx, boldText, L"Kontroll X#" + name, limit);
      }
      else
        gdi.addStringUT(yp, cx, boldText, name, limit);

      wstring split = r->getSplitTimeS(k, false);

      int bestTime = 0;
      if ( k < int(after.size()) && after[k] >= 0)
        bestTime = r->getSplitTime(k, false) - after[k];

      GDICOLOR color = colorDefault;
      if (k < int(after.size()) ) {
        if (after[k] > 0)
          split += L" (" + itow(place[k]) + L", +"  + getTimeMS(after[k]) + L")";
        else if (place[k] == 1)
          split += lang.tl(" (sträckseger)");
        else if (place[k] > 0)
          split += L" " + itow(place[k]);

        if (after[k] >= 0 && after[k]<=int(bestTime * 0.03))
          color = colorLightGreen;
      }
      gdi.addStringUT(yp + lh, cx, fontMedium, split, limit);

      if (k>0 && k < int(placeAcc.size())) {
        split = r->getPunchTimeS(k, false);
        wstring pl = placeAcc[k] > 0 ? itow(placeAcc[k]) : L"-";
        if (k < int(afterAcc.size()) ) {
          if (afterAcc[k] > 0)
            split += L" (" + pl + L", +"  + getTimeMS(afterAcc[k]) + L")";
        else if (placeAcc[k] == 1)
          split += lang.tl(" (ledare)");
          else if (placeAcc[k] > 0)
            split += L" " + pl;
        }
        gdi.addStringUT(yp + 2*lh, cx, fontMedium, split, limit).setColor(colorDarkBlue);
      }

      if (k < int(delta.size()) && delta[k] > 0 ) {
        gdi.addString("", yp + 3*lh, cx, fontMedium, L"Bomtid: X#" + getTimeMS(delta[k]));

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
    oe.getPunchesForRunner(r->getId(), true, punches);

    int lastT = r->getStartTime();
    for (size_t k = 0; k < punches.size(); k++) {

      wstring name = punches[k]->getType();
      wstring realName;
      if (_wtoi(name.c_str()) > 0) {
        const pCourse rCrs = r->getCourse(false);
        if (rCrs) {
          vector<pControl> crsCtrl;
          rCrs->getControls(crsCtrl);
          for(size_t j = 0; j < crsCtrl.size(); j++) {
            if (crsCtrl[j]->hasNumber(_wtoi(name.c_str()))) {
              if (crsCtrl[j]->hasName())
                realName = crsCtrl[j]->getName();

              break;
            }
          }
        }
        if (realName.empty())
          gdi.addString("", yp, cx, boldText, L"Kontroll X#" + name, limit);
        else
          gdi.addStringUT(yp, cx, boldText, realName, limit);
      }
      else
        gdi.addStringUT(yp, cx, boldText, name, limit);

      int t = punches[k]->getAdjustedTime();
      if (t>0) {
        int st = r->getStartTime();
        gdi.addString("", yp + lh, cx, normalText, L"Klocktid: X#" + oe.getAbsTime(t), limit);
        if (st > 0 && t > st) {
          wstring split = formatTimeHMS(t-st);
          if (lastT>0 && st != lastT && lastT < t)
            split += L" (" + getTimeMS(t-lastT) + L")";
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
      oe->generateListInfo(par, info);
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
    
    gdi.addInput("CardNo", tsi.storedInfo.storedCardNo, 8, VacancyCB, L"Bricka:");
    tsi.setCardNumberField("CardNo");
    
    //Remember to clear SI-link when page is cleared.
    gdi.setPostClearCb(VacancyCB);
    gdi.setOnClearCb(VacancyCB);

    gdi.dropLine(1.2);
    gdi.addCheckbox("RentCard", "Hyrd", 0, tsi.storedInfo.rentState);
    gdi.dropLine(-1.2);

    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
      gdi.addCombo("Club", 220, 300, RunnerCB, L"Klubb:");
      oe->fillClubs(gdi, "Club");
      gdi.setText("Club", tsi.storedInfo.storedClub);
    }

    gdi.addInput("Name", tsi.storedInfo.storedName, 16, RunnerCB, L"Namn:");

    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy)) {
      if (!tsi.storedInfo.storedFee.empty())
        lastFee = tsi.storedInfo.storedFee;

      gdi.addCombo("Fee", 60, 150, 0, L"Avgift:");
      oe->fillFees(gdi, "Fee", false, true);
      gdi.autoGrow("Fee");

      if (!lastFee.empty() && lastFee != L"@") {
        gdi.setText("Fee", lastFee);
      }
      else {
        gdi.selectFirstItem("Fee");
      }
      gdi.dropLine(1.2);

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
  bool hasDNS;
  oe->analyseDNS(unknown_dns, known_dns, known, unknown, hasDNS);
  if (!unknown.empty()) {
    gdi.dropLine();
    gdi.dropLine(0.5);
    gdi.addString("", 1, "Löpare, Status Okänd, som saknar registrering");
    listRunners(gdi, unknown, true);
  }
  else {
    if (hasDNS) {
      BaseInfo &bi = gdi.getBaseInfo("SetDNS");
      bi.id = "UndoSetDNS";
      gdi.setTextTranslate(bi.id, L"Återställ <Ej Start> till <Status Okänd>");
    }
    else {
      gdi.disableInput("SetDNS");
    }
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
    out.clear();
    sprintf_s(bf, "%d.", int(k+1));
    gdi.addStringUT(yp, xp, 0, bf);
    gdi.addStringUT(yp, xp+gdi.scaleLength(40), 0, r[k]->getNameAndRace(true), gdi.scaleLength(190));
    gdi.addStringUT(yp, xp+gdi.scaleLength(200), 0, r[k]->getClass(true), gdi.scaleLength(140));
    gdi.addStringUT(yp, xp + gdi.scaleLength(350), 0, r[k]->getClub(), +gdi.scaleLength(190));
    int c = r[k]->getCardNo();
    if (c>0) {
      {
        vector<pRunner> o2;
        oe->getRunnersByCardNo(c, false, oEvent::CardLookupProperty::SkipNoStart, o2);
        for (pRunner r : o2) {
          if (!r->skip())
            out.push_back(r);
        }
      }
      if (out.size() <= 1) {
        gdi.addStringUT(yp, xp+gdi.scaleLength(550), 0, "(" + itos(c) + ")", 190);
      }
      else {
        TextInfo &ti = gdi.addStringUT(yp, xp+gdi.scaleLength(550), 0, L"(" + itow(c) + lang.tl(", reused card") + L")", gdi.scaleLength(150));
        wstring tt;
        for (size_t j = 0; j < out.size(); j++) {
          if (out[j] == r[k]->getMultiRunner(0))
            continue;
          if (!tt.empty())
            tt += L", ";
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

      wstring name = c->getOwner() ? c->getOwner()->getName() : makeDash(L"-");
      gdi.addString("", 0, L"Nuvarande innehavare: X.#" + name);

      gdi.dropLine(1);
      gdi.pushX();
      gdi.fillRight();
      gdi.addListBox("Card", 150, 300, 0, L"Vald bricka:");
      c->fillPunches(gdi, "Card", 0);
      gdi.disableInput("Card");

      gdi.pushX();
      gdi.fillRight();
      gdi.popX();

      gdi.fillDown();
      gdi.addListBox("Runners", 350, 300, 0, L"Deltagare:");
      gdi.setTabStops("Runners", 200, 300);
      oe->fillRunners(gdi, "Runners", true, oEvent::RunnerFilterShowAll);
      if (c->getOwner())
        gdi.selectItemByData("Runners", c->getOwner()->getId());

      gdi.popX();
      gdi.fillRight();
      gdi.addInput("SearchText", L"", 15).setBgColor(colorLightCyan);
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

const wstring &TabRunner::getSearchString() const {
  return lang.tl(L"Sök (X)#Ctrl+F");
}

void disablePunchCourseChange(gdioutput &gdi)
{
  gdi.disableInput("SaveC");
  gdi.disableInput("RemoveC");
  gdi.disableInput("PTime");
  gdi.setText("PTime", L"");
  gdi.selectItemByData("Punches", -1);
}

void disablePunchCourse(gdioutput &gdi)
{
  disablePunchCourseAdd(gdi);
  disablePunchCourseChange(gdi);
}

void TabRunner::updateStatus(gdioutput &gdi, pRunner r) {
  if (!r) return;

  gdi.setText("Start", r->getStartTimeS());
  gdi.setText("Finish", r->getFinishTimeS());
  gdi.setText("Time", r->getRunningTimeS(false));
  gdi.setText("Points", itow(r->getRogainingPoints(false, false)));

  gdi.selectItemByData("Status", r->getStatus());
  auto ri = r->getRaceInfo();
  BaseInfo *bi = gdi.setText("RunnerInfo", ri.first, true);
  TextInfo *ti = dynamic_cast<TextInfo*>(bi);
  assert(ti);
  if (ti) {
    if (ri.second > 0)
      ti->setColor(GDICOLOR::colorGreen);
    else if (ri.second < 0)
      ti->setColor(GDICOLOR::colorRed);
    else
      ti->setColor(GDICOLOR::colorDefault);
  }
}

int TabRunner::punchesCB(gdioutput &gdi, int type, void *data) {
  DWORD rid = runnerId;
  if (!rid)
    return 0;

  pRunner r = oe->getRunner(rid, 0);

  if (!r) {
    gdi.alert("Deltagaren måste sparas innan stämplingar kan hanteras.");
    return 0;
  }
  if (type == GUI_LISTBOXSELECT) {
    ListBoxInfo bi = *(ListBoxInfo *)data;
    if (bi.id == "Course") {
      if (signed(bi.data) >= 0) {
        pCourse pc = r->getCourse(true);
        if (!pc) return 0;

        return gdi.sendCtrlMessage("AddC");
      }
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo bi = *(ListBoxInfo *)data;

    if (bi.id == "Punches") {
      if (bi.data != -1) {
        pCard card = r->getCard();
        if (!card) return 0;
        pPunch punch = card->getPunchByIndex(bi.data);
        if (!punch)
          throw meosException("Punch not found.");

        wstring ptime;
        if (punch->getTimeInt() > 0)
         ptime = punch->getTime();

        gdi.setText("PTime", ptime);
        
        gdi.enableInput("SaveC");
        
        gdi.enableInput("RemoveC");
        gdi.enableInput("PTime");
      }
      else {
        gdi.disableInput("SaveC");
        gdi.disableInput("RemoveC");
        gdi.setText("PTime", L"");
        gdi.disableInput("PTime");
      }
      disablePunchCourseAdd(gdi);
    }
    else if (bi.id == "Course") {
      if (signed(bi.data) >= 0) {
        pCourse pc = r->getCourse(true);
        if (!pc) return 0;
        gdi.enableInput("AddC");
        gdi.enableInput("AddAllC");
      }
      else {
        gdi.disableInput("AddC");
        gdi.disableInput("AddAllC");
      }
      disablePunchCourseChange(gdi);
    }
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo bi = *(ButtonInfo *)data;
    pCard card = r->getCard();

    if (!card) {
      if (!gdi.ask(L"ask:addpunches"))
        return 0;

      card = oe->allocateCard(r);

      card->setCardNo(r->getCardNo());
      vector<int> mp;
      r->addPunches(card, mp);
    }

    if (bi.id == "AddC") {
      vector<int> mp;
      r->synchronize();
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);

      pCourse pc = r->getCourse(true);

      if (!pc) return 0;

      ListBoxInfo lbi;

      if (!gdi.getSelectedItem("Course", lbi))
        return 0;

      oControl *oc = pc->getControl(lbi.data);

      if (!oc) return 0;
      vector<int> nmp;

      if (oc->getStatus() == oControl::StatusRogaining) {
        r->evaluateCard(true, nmp, oc->getFirstNumber(), oBase::ChangeType::Update); //Add this punch
      }
      else {
        for (size_t k = 0; k < mp.size(); k++) {
          if (oc->hasNumber(mp[k]))
            r->evaluateCard(true, nmp, mp[k], oBase::ChangeType::Update); //Add this punch
        }
      }
      //synchronize SQL
      card->synchronize();
      r->synchronizeAll(true);
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      r->hasManuallyUpdatedTimeStatus();
      card->fillPunches(gdi, "Punches", pc);
      updateStatus(gdi, r);
    }
    else if (bi.id == "AddAllC") {
      vector<int> mp;
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      vector<int>::iterator it = mp.begin();

      while (it != mp.end()) {
        vector<int> nmp;
        r->evaluateCard(true, nmp, *it, oBase::ChangeType::Update); //Add this punch
        ++it;
        if (nmp.empty())
          break;
      }

      //synchronize SQL
      card->synchronize();
      r->synchronizeAll(true);
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      card->fillPunches(gdi, "Punches", r->getCourse(true));
      r->hasManuallyUpdatedTimeStatus();
      updateStatus(gdi, r);
    }
    else if (bi.id == "SaveC") {
      if (!savePunchTime(r, gdi))
        return false;
      ListBoxInfo lbi;

      if (!gdi.getSelectedItem("Punches", lbi))
        return 0;
      /*
            pCard pc=r->getCard();

            if (!pc) return 0;

            pPunch pp = pc->getPunchByIndex(lbi.data);

            if (!pp)
              throw meosException("Punch not found.");

            pc->setPunchTime(pp, gdi.getText("PTime"));

            r->evaluateCard(true, mp);

            //synchronize SQL
            card->synchronize();*/
      vector<int> mp;
      r->synchronize();
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      r->hasManuallyUpdatedTimeStatus();
      card->fillPunches(gdi, "Punches", r->getCourse(true));
      updateStatus(gdi, r);
      gdi.selectItemByData("Punches", lbi.data);
    }
  }
  return 0;
}

bool TabRunner::savePunchTime(pRunner r, gdioutput &gdi) {
  ListBoxInfo lbi;
  pCard card = r->getCard();
  if (!card)
    return false;

  if (!gdi.getSelectedItem("Punches", lbi))
    return 0;

  pPunch pp = card->getPunchByIndex(lbi.data);

  if (!pp)
    return false;
    
  card->setPunchTime(pp, gdi.getText("PTime"));

  vector<int> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);

  //synchronize SQL
  card->synchronize();
  r->synchronizeAll();
  return true;
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
  int basex = gdi.getCX();

  if (currentMode == 1) {
    addToolbar(gdi);
    gdi.dropLine(1);
    gdi.addTable(oRunner::getTable(oe), basex, gdi.getCY());
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

  gdi.addInput("SearchText", getSearchString(), 13, runnerSearchCB, L"",
    L"Sök på namn, bricka eller startnummer.").isEdit(false)
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
  gdi.addInput("Name", L"", 16, RunnerCB, L"Namn:");

  if (oe->hasBib(true, false)) {
    gdi.addInput("Bib", L"", 4, 0, L"Nr", L"Nummerlapp");
  }
  gdi.popX();
  gdi.dropLine(3);

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    gdi.fillDown();
    gdi.addCombo("Club", 220, 300, RunnerCB, L"Klubb:");
    oe->fillClubs(gdi, "Club");
    gdi.pushX();
  }

  if (oe->hasTeam()) {
    gdi.fillRight();
    gdi.addInput("Team", L"", 16, 0, L"Lag:").isEdit(false);
    gdi.disableInput("Team");
    gdi.fillDown();
    gdi.dropLine(0.9);
    gdi.addButton("EditTeam", "...", RunnerCB, "Hantera laget");
    gdi.popX();
  }

  gdi.fillRight();
  bool hasEco = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy);
  gdi.addSelection("RClass", hasEco ? 130 : 170, 300, RunnerCB, L"Klass:");
  oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterNone);
  gdi.addItem("RClass", lang.tl("Ingen klass"), 0);

  if (hasEco) {
    //gdi.addInput("Fee", L"", 4, 0, L"Avgift:");
    gdi.fillDown();
    gdi.dropLine();
    gdi.addButton("Economy", "Ekonomi...", RunnerCB, "");

  }
  else {
    gdi.fillDown();
    gdi.dropLine(3);
  }

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

  if (!oe->getMeOSFeatures().withoutCourses(*oe)) {
    gdi.addSelection("RCourse", numSL == 0 ? 220 : 180, 300, RunnerCB, L"Bana:");
    oe->fillCourses(gdi, "RCourse", true);
    gdi.addItem("RCourse", lang.tl("[Klassens bana]"), 0);

    if (numSL > 0) {
      gdi.fillDown();
      gdi.addSelection("NumShort", 60, 300, RunnerCB, L"Avkortning:");
      vector< pair<wstring, size_t> > data;
      if (numSL == 1) {
        data.push_back(make_pair(lang.tl("Nej"), 0));
        data.push_back(make_pair(lang.tl("Ja"), 1));
      }
      else {
        data.push_back(make_pair(lang.tl("Nej"), 0));
        for (int i = 1; i <= numSL; i++) {
          data.push_back(make_pair(itow(i), i));
        }
      }
      gdi.addItem("NumShort", data);
      gdi.popX();
    }
  }

  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("CardNo", L"", 8, RunnerCB, L"Bricka:");
  gdi.dropLine(1);
  gdi.addCheckbox("RentCard", "Hyrd", 0, false);

  gdi.dropLine(2);
  gdi.popX();

  gdi.addInput("Start", L"", 8, 0, L"Starttid:");
  gdi.addInput("Finish", L"", 8, 0, L"Måltid:");

  const bool timeAdjust = oe->getMeOSFeatures().hasFeature(MeOSFeatures::TimeAdjust);
  const bool pointAdjust = oe->getMeOSFeatures().hasFeature(MeOSFeatures::PointAdjust);

  if (timeAdjust || pointAdjust) {
    gdi.dropLine(3);
    gdi.popX();
    if (timeAdjust) {
      gdi.addInput("TimeAdjust", L"", 8, 0, L"Tidstillägg:");
    }
    if (pointAdjust) {
      gdi.addInput("PointAdjust", L"", 8, 0, L"Poängavdrag:");
    }
  }
  gdi.dropLine(3);
  gdi.popX();

  gdi.addInput("Time", L"", 8, 0, L"Tid:").isEdit(false).ignore(true);
  gdi.disableInput("Time");

  if (oe->hasRogaining()) {
    gdi.addInput("Points", L"", 5, 0, L"Poäng:").isEdit(false).ignore(true);
    gdi.disableInput("Points");
  }

  gdi.fillDown();
  gdi.addSelection("Status", 100, 80, 0, L"Status:", L"tooltip_explain_status");
  oe->fillStatus(gdi, "Status");
  gdi.autoGrow("Status");
  gdi.popX();
  gdi.selectItemByData("Status", 0);

  gdi.addString("RunnerInfo", 0, "").setColor(colorRed);
  gdi.dropLine(0.4);
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
 
    gdi.addSelection("StatusIn", 100, 160, 0, L"Status:", L"tooltip_explain_status");
    oe->fillStatus(gdi, "StatusIn");
    gdi.selectItemByData("Status", 0);
    gdi.addInput("PlaceIn", L"", 5, 0, L"Placering:");
    int xmax = gdi.getCX() + dx;
    gdi.setCX(ccx);
    gdi.dropLine(3);
    gdi.addInput("TimeIn", L"", 5, 0, L"Tid:");
    if (oe->hasRogaining()) {
      gdi.addInput("PointIn", L"", 5, 0, L"Poäng:");
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
  gdi.addListBox("Punches", 150, 300, PunchesCB, L"Stämplingar:").ignore(true);
  gdi.addButton("RemoveC", "Ta bort stämpling >>", RunnerCB);

  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("PTime", L"", 8, 0, L"", L"Stämplingstid");
  gdi.fillDown();
  gdi.addButton("SaveC", "Spara tid", PunchesCB);
  gdi.popX();
  gdi.dropLine();
  int contX = gdi.getCX();
  int contY = gdi.getCY();

  gdi.newColumn();
  gdi.dropLine(2.5);
  gdi.fillDown();
  gdi.addListBox("Course", 140, 300, PunchesCB, L"Banmall:").ignore(true);
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
                "Sträcktider", RunnerCB, "", false, false).isEdit(true).setExtra(0);
  gdi.addButton("PrintSettings", "...", RunnerCB, "Inställningar").isEdit(true).setExtra(0);

  gdi.dropLine(2.5);
  gdi.setCX(contX);
  gdi.addButton(gdi.getCX(), gdi.getCY(), gdi.scaleLength(120), "SplitPrint", 
                "Startbevis", RunnerCB, "", false, false).isEdit(true).setExtra(1);
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
    lastSearchExpr = L"";
    ((InputInfo *)gdi.setText("SearchText", getSearchString()))->setFgColor(colorGreyBlue);
      lastFilter.clear();
  }
}

bool TabRunner::canSetStart(pRunner r) const {
  pClass pc = r->getTeam() ? r->getTeam()->getClassRef(false) : r->getClassRef(true);
  if (r->getTeam() && pc->isQualificationFinalBaseClass())
    pc = r->getClassRef(true);

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

  if (!r->getCard() && cno != 0) {
    vector<pRunner> allR;
    oe->getRunnersByCardNo(cno, false, oEvent::CardLookupProperty::Any, allR);
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
    gdi.updateToolTip("CardNo", L"Brickan används av X.#" + warnCardDupl->getCompleteIdentification());
    cardNo.refresh();
  }
  else {
    if (cardNo.getBgColor() != colorDefault) {
      cardNo.setBgColor(colorDefault);
      gdi.updateToolTip("CardNo", L"");
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
    auto shInfo = allCrs[k]->getShorterVersion();
    pCourse cCourse = allCrs[k];
    int count = 0;
    if (shInfo.second) {
      pCourse sh = shInfo.second;
      touch.clear();
      while (sh && !touch.count(sh->getId())) {
        cCourse = sh;
        count++;
        map<int, int>::iterator r = known.find(sh->getId());
        if (r != known.end()) {
          count += r->second;
          break;
        }
        touch.insert(sh->getId());
        shInfo = sh->getShorterVersion();
        sh = shInfo.second;
      }
    }

    if (shInfo.first && !shInfo.second) {
      // Course with loops
      int nl = cCourse->getNumLoops();
      if (nl > 0)
      count += nl - 1;
    }

    known[allCrs[k]->getId()] = count;
    res = max(res, count);
  }
  return res;
}

void TabRunner::updateNumShort(gdioutput &gdi, pCourse crs, pRunner r) {
  if (gdi.hasWidget("NumShort")) {
    if (crs && crs->getShorterVersion().first) {
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

void TabRunner::EconomyHandler::init(oRunner &r) {
  oe = r.getEvent();
  runnerId = r.getId();
}

oRunner &TabRunner::EconomyHandler::getRunner() const {
  pRunner p = oe->getRunner(runnerId, 0);
  if (!p)
    throw meosException("Löpare saknas");
  return *p;
}

TabRunner::EconomyHandler *TabRunner::getEconomyHandler(oRunner &r) {
  if (!ecoHandler)
    ecoHandler = make_shared<EconomyHandler>();

  ecoHandler->init(r);
  return ecoHandler.get();
}

void TabRunner::EconomyHandler::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GuiEventType::GUI_BUTTON) {
    ButtonInfo &bi = dynamic_cast<ButtonInfo &>(info);
    if (bi.id == "Close") {
      save(gdi);
      gdi.closeWindow();
    }
    else if (bi.id == "Save") {
      save(gdi);
    }
    else if (bi.id == "Cancel") {
      TabRunner &dst = dynamic_cast<TabRunner&>(*gdi.getTabs().get(TabType::TRunnerTab));
      dst.loadEconomy(gdi, getRunner());
    }
  }
  else if (type == GuiEventType::GUI_INPUTCHANGE) {
    InputInfo &ii = dynamic_cast<InputInfo &>(info);
    if (ii.id == "Fee") {
      gdi.check("ModFee", ii.changed() || getRunner().hasFlag(oAbstractRunner::FlagFeeSpecified));
    }
    else if (ii.id == "PaidAmount") {
      int paid = oe->interpretCurrency(ii.text);
      gdi.setInputStatus("PayMode", paid != 0);
    }
  }
}

void TabRunner::EconomyHandler::save(gdioutput &gdi) {
  oRunner &r = getRunner();
  if (r.getTeam() == 0) {
    r.getDI().setDate("EntryDate", gdi.getText("EntryDate"));
    int t = convertAbsoluteTimeHMS(gdi.getText("EntryTime"), -1);
    r.getDI().setInt("EntryTime", t);
  }
  int fee = oe->interpretCurrency(gdi.getText("Fee"));
  if (r.getClassRef(true)) {
    int def = r.getClassRef(true)->getEntryFee(r.getEntryDate(), r.getBirthAge());
    r.setFlag(oAbstractRunner::FlagFeeSpecified, def != fee);
  }
  r.getDI().setInt("Fee", fee);
  int cf = oe->interpretCurrency(gdi.getText("Card"));
  if (cf > 0 || cf == 0 && r.getDCI().getInt("CardFee") != -1)
    r.getDI().setInt("CardFee", cf);
  int paid = oe->interpretCurrency(gdi.getText("PaidAmount"));
  r.getDI().setInt("Paid", paid);

  if (paid != 0) {
    int m = gdi.getSelectedItem("").first;
    if (m != 1000)
      r.getDI().setInt("PayMode", m);
  }
}

void TabRunner::loadEconomy(gdioutput &gdi, oRunner &r) {
  gdi.clearPage(false);
  gdi.fillDown();
  gdi.pushX();
  gdi.addString("", fontMediumPlus, L"Ekonomihantering, X#" + r.getCompleteIdentification());
  auto h = getEconomyHandler(r);

  gdi.fillRight();
  gdi.addInput("EntryDate", r.getEntryDate(true), 10, 0, L"Anmälningsdatum:");
  gdi.fillDown();
  gdi.addInput("EntryTime", formatTime(r.getDCI().getInt("EntryTime")), 10, 0, L"Anmälningstid:");
  gdi.setInputStatus("EntryDate", r.getTeam() == 0);
  gdi.setInputStatus("EntryTime", r.getTeam() == 0);

  gdi.popX();
  gdi.dropLine();

  gdi.fillRight();
  gdi.addInput("Fee", oe->formatCurrency(r.getDCI().getInt("Fee")), 5, 0, L"Avgift:").setHandler(h);
  int cf = r.getDCI().getInt("CardFee");
  if (cf == -1) // Borrowed, zero fee
    cf = 0;
  gdi.addInput("Card", oe->formatCurrency(cf), 5, 0, L"Brickhyra:");
  int paid = r.getDCI().getInt("Paid");
  gdi.addInput("PaidAmount", oe->formatCurrency(paid), 5, 0, L"Betalat:").setHandler(h);
  gdi.fillDown();
  gdi.dropLine();
  vector< pair<wstring, size_t> > pm;
  oe->getPayModes(pm);
  int mypm = r.getDCI().getInt("PayMode");
  //pm.insert(pm.begin(), make_pair(lang.tl(L"Faktureras"), 1000));
  gdi.addSelection("PayMode", 110, 100, SportIdentCB);
  gdi.addItem("PayMode", pm);
  gdi.selectItemByData("PayMode", mypm);
  gdi.autoGrow("PayMode");
  gdi.setInputStatus("PayMode", paid != 0);

  gdi.dropLine();
  gdi.popX();

  gdi.addString("", 1, "Manuellt gjorda justeringar");
  gdi.fillRight();

  gdi.addCheckbox("ModFee", "Avgift", 0, r.hasFlag(oAbstractRunner::FlagFeeSpecified));
  gdi.disableInput("ModFee");

  gdi.addCheckbox("ModCls", "Klass", 0, r.hasFlag(oAbstractRunner::FlagUpdateClass));
  gdi.disableInput("ModCls");

  gdi.fillDown();
  gdi.addCheckbox("ModCls", "Namn", 0, r.hasFlag(oAbstractRunner::FlagUpdateName));
  gdi.disableInput("ModCls");

  gdi.fillRight();
  gdi.addButton("Cancel", "Ångra").setHandler(h);
  gdi.addButton("Close", "Stäng").setHandler(h);
  gdi.addButton("Save", "Spara").setHandler(h);
  gdi.refresh();
}

void TabRunner::handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) {
  auto bi = gdi.setText(info.getTarget(), info.getCurrent().c_str());
  if (bi) {
    int ix = info.getCurrentInt();
    bi->setExtra(ix);
    if (info.getTarget() == "Name") {
      auto &db = oe->getRunnerDatabase();
      auto runner = db.getRunnerByIndex(ix);

      if (runner && gdi.hasWidget("Club") && gdi.getText("Club").empty()) {
        pClub club = db.getClub(runner->dbe().clubNo);
        if (club)
          gdi.setText("Club", club->getName());
      }
      if (runner && runner->dbe().cardNo > 0 && gdi.hasWidget("CardNo") && gdi.getText("CardNo").empty()) {
        gdi.setText("CardNo", runner->dbe().cardNo);
      }
    }
  }
  gdi.clearAutoComplete("");
  gdi.TabFocus(1);
}

pClub TabRunner::extractClub(oEvent *oe, gdioutput &gdi) {
  oClub *dbClub = nullptr;
  if (gdi.hasWidget("Club")) {
    auto &db = oe->getRunnerDatabase();
    int clubId = gdi.getExtraInt("Club");
    if (clubId >= 0) {
      dbClub = db.getClub(clubId);
      if (dbClub && !stringMatch(dbClub->getName(), gdi.getText("Club")))
        dbClub = nullptr;
    }
    if (dbClub == nullptr) {
      dbClub = db.getClub(gdi.getText("Club"));
    }
  }
  return dbClub;
}
