/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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
#include <cassert>

#include "meosexception.h"
#include "TabTeam.h"
#include "TabRunner.h"
#include "MeOSFeatures.h"
#include "RunnerDB.h"
#include "autocomplete.h"

#include "TabSI.h"

TabTeam::TabTeam(oEvent *poe):TabBase(poe)
{
  clearCompetitionData();
}

TabTeam::~TabTeam(void)
{
}

int TeamCB(gdioutput *gdi, int type, void *data)
{
  TabTeam &tt = dynamic_cast<TabTeam &>(*gdi->getTabs().get(TTeamTab));

  return tt.teamCB(*gdi, type, data);
}

int teamSearchCB(gdioutput *gdi, int type, void *data)
{
  TabTeam &tc = dynamic_cast<TabTeam &>(*gdi->getTabs().get(TTeamTab));

  return tc.searchCB(*gdi, type, data);
}

int TabTeam::searchCB(gdioutput &gdi, int type, void *data) {
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
        gdi.addTimeoutMilli(500, "Search", teamSearchCB).setData(inputId, expr);
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

      oe->findTeam(expr, 0, filter);
      lastSearchExpr = expr;
      // Filter more
      if (filter.empty()) {
        vector< pair<wstring, size_t> > runners;
        runners.push_back(make_pair(lang.tl(L"Ingen matchar 'X'#" + expr), -1));
        gdi.addItem("Teams", runners);
      }
      else
        gdi.filterOnData("Teams", filter);
    }
    else {
      oe->findTeam(expr, 0, filter);
      lastSearchExpr = expr;

      vector< pair<wstring, size_t> > runners;
      oe->fillTeams(runners);

      if (filter.size() == runners.size()){
      }
      else if (filter.empty()) {
        runners.clear();
        runners.push_back(make_pair(lang.tl(L"Ingen matchar 'X'#" + expr), -1));
      }
      else {
        vector< pair<wstring, size_t> > runners2;

        for (size_t k = 0; k<runners.size(); k++) {
          if (filter.count(runners[k].second) == 1) {
            runners2.push_back(make_pair(wstring(), runners[k].second));
            runners2.back().first.swap(runners[k].first);
          }
        }
        runners.swap(runners2);
      }

      gdi.addItem("Teams", runners);
    }

    if (type == GUI_TIMER)
      gdi.setWaitCursor(false);
  }

  return 0;
}


void TabTeam::selectTeam(gdioutput &gdi, pTeam t)
{
  if (t){
    t->synchronize();
    t->evaluate(oBase::ChangeType::Quiet);

    teamId=t->getId();

    gdi.enableEditControls(true);
    gdi.enableInput("Save");
    gdi.enableInput("Undo");
    gdi.enableInput("Remove");

    oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterOnlyMulti);
    gdi.selectItemByData("RClass", t->getClassId(false));
    gdi.selectItemByData("Teams", t->getId());

    if (gdi.hasWidget("StatusIn")) {
      gdi.selectItemByData("StatusIn", t->getInputStatus());
      int ip = t->getInputPlace();
      if (ip > 0)
        gdi.setText("PlaceIn", ip);
      else
        gdi.setText("PlaceIn", makeDash(L"-"));

      gdi.setText("TimeIn", t->getInputTimeS());
      if (gdi.hasWidget("PointIn"))
        gdi.setText("PointIn", t->getInputPoints());
    }

    if (gdi.hasWidget("NoRestart")) {
      gdi.check("NoRestart", t->preventRestart());
    }

    loadTeamMembers(gdi, 0, 0, t);
  }
  else {
    teamId=0;

    gdi.enableEditControls(false);
    gdi.disableInput("Save");
    gdi.disableInput("Undo");
    gdi.disableInput("Remove");

    ListBoxInfo lbi;
    gdi.getSelectedItem("RClass", lbi);

    gdi.selectItemByData("Teams", -1);

    if (gdi.hasWidget("StatusIn")) {
      gdi.selectFirstItem("StatusIn");
      gdi.setText("PlaceIn", L"");
      gdi.setText("TimeIn", makeDash(L"-"));
      if (gdi.hasWidget("PointIn"))
        gdi.setText("PointIn", L"");
    }

    loadTeamMembers(gdi, lbi.data, 0, 0);
  }

  updateTeamStatus(gdi, t);
  gdi.refresh();
}

void TabTeam::updateTeamStatus(gdioutput &gdi, pTeam t)
{
  if (!t) {
    gdi.setText("Name", L"");
    if (gdi.hasWidget("StartNo"))
      gdi.setText("StartNo", L"");
    if (gdi.hasWidget("Club"))
      gdi.setText("Club", L"");
    bool hasFee = gdi.hasWidget("Fee");
    if (hasFee) {
      gdi.setText("Fee", L"");
    }
    gdi.setText("Start", makeDash(L"-"));
    gdi.setText("Finish", makeDash(L"-"));
    gdi.setText("Time", makeDash(L"-"));
    gdi.selectItemByData("Status", 0);
    gdi.setText("TimeAdjust", makeDash(L"-"));
    gdi.setText("PointAdjust", makeDash(L"-"));
    gdi.setText("TeamInfo", L"", true);

    return;
  }

  gdi.setText("Name", t->getName());
  if (gdi.hasWidget("StartNo"))
    gdi.setText("StartNo", t->getBib());

  if (gdi.hasWidget("Club"))
    gdi.setText("Club", t->getClub());
  bool hasFee = gdi.hasWidget("Fee");
  if (hasFee) {
    gdi.setText("Fee", oe->formatCurrency(t->getDI().getInt("Fee")));
  }

  gdi.setText("Start", t->getStartTimeS());
  gdi.setText("Finish",t->getFinishTimeS());
  gdi.setText("Time", t->getRunningTimeS(true));
  gdi.setText("TimeAdjust", getTimeMS(t->getTimeAdjustment()));
  gdi.setText("PointAdjust", -t->getPointAdjustment());
  gdi.selectItemByData("Status", t->getStatus());

  auto ri = t->getRaceInfo();
  BaseInfo *bi = gdi.setText("TeamInfo", ri.first, true);
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

bool TabTeam::save(gdioutput &gdi, bool dontReloadTeams) {
  if (teamId==0)
    return 0;

  DWORD tid=teamId;
  wstring name=gdi.getText("Name");

  if (name.empty()) {
    gdi.alert("Alla lag måste ha ett namn.");
    return 0;
  }

  bool create=false;

  pTeam t;
  if (tid==0) {
    t=oe->addTeam(name);
    create=true;
  }
  else t=oe->getTeam(tid);

  teamId=t->getId();
  bool bibModified = false;
  if (t) {
    t->setName(name, true);
    if (gdi.hasWidget("StartNo")) {
      const wstring &bib = gdi.getText("StartNo");
      if (bib != t->getBib()) {
        bibModified = true;
        wchar_t pat[32];
        int no = oClass::extractBibPattern(bib, pat);
        t->setBib(bib, no, no > 0);
      }
    }
    wstring start = gdi.getText("Start");
    t->setStartTimeS(start);
    if (t->getRunner(0))
      t->getRunner(0)->setStartTimeS(start);

    t->setFinishTimeS(gdi.getText("Finish"));

    if (gdi.hasWidget("Fee"))
      t->getDI().setInt("Fee", oe->interpretCurrency(gdi.getText("Fee")));


    if (gdi.hasWidget("NoRestart"))
      t->preventRestart(gdi.isChecked("NoRestart"));
    
    t->apply(oBase::ChangeType::Quiet, nullptr);

    if (gdi.hasWidget("Club")) {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Club", lbi);

      if (!lbi.text.empty()) {
        pClub pc=oe->getClub(lbi.text);
        if (!pc)
          pc = oe->addClub(lbi.text);
        pc->synchronize();
      }

      t->setClub(lbi.text);
      if (!dontReloadTeams)
        oe->fillClubs(gdi, "Club");
      gdi.setText("Club", lbi.text);
    }
    ListBoxInfo lbi;
    gdi.getSelectedItem("Status", lbi);

    RunnerStatus sIn = (RunnerStatus)lbi.data;
    // Must be done AFTER all runners are set. But setting runner can modify status, so decide here.
    bool setTeamStatus = (!oAbstractRunner::isResultStatus(sIn) || sIn == StatusOK) && (t->getStatus() != sIn);
    bool checkStatus = (sIn != t->getStatus());

    if (sIn == StatusUnknown && !oAbstractRunner::isResultStatus(t->getStatus()))
      t->setTeamMemberStatus(StatusUnknown);
    else if ((RunnerStatus)lbi.data != t->getStatus())
      t->setStatus((RunnerStatus)lbi.data, true, oBase::ChangeType::Update);

    gdi.getSelectedItem("RClass", lbi);

    int classId = lbi.data;
    bool newClass = t->getClassId(false) != classId;
    set<int> classes;
    bool globalDep = false;
    if (t->getClassRef(false))
      globalDep = t->getClassRef(false)->hasClassGlobalDependence();

    classes.insert(classId);
    classes.insert(t->getClassId(false));

    bool readStatusIn = true;
    if (newClass && t->getInputStatus() != StatusNotCompetiting && t->hasInputData()) {
      if (gdi.ask(L"Vill du sätta resultatet från tidigare etapper till <Deltar ej>?")) {
        t->resetInputData();
        readStatusIn = false;
      }
    }

    if (newClass && !bibModified) {
      pClass pc = oe->getClass(classId);
      if (pc) {
        pair<int, wstring> snoBib = pc->getNextBib();
        if (snoBib.first > 0) {
          t->setBib(snoBib.second, snoBib.first, true);
        }
      }
    }
    
    t->setClassId(classId, true);

    if (gdi.hasWidget("TimeAdjust")) {
      int time = convertAbsoluteTimeMS(gdi.getText("TimeAdjust"));
      if (time != NOTIME)
        t->setTimeAdjustment(time);
    }
    if (gdi.hasWidget("PointAdjust")) {
      t->setPointAdjustment(-gdi.getTextNo("PointAdjust"));
    }

    if (gdi.hasWidget("StatusIn") && readStatusIn) {
      t->setInputStatus(RunnerStatus(gdi.getSelectedItem("StatusIn").first));
      t->setInputPlace(gdi.getTextNo("PlaceIn"));
      t->setInputTime(gdi.getText("TimeIn"));
      if (gdi.hasWidget("PointIn"))
        t->setInputPoints(gdi.getTextNo("PointIn"));
    }

    pClass pc=oe->getClass(classId);

    if (pc) {
      globalDep |= pc->hasClassGlobalDependence();

      for (unsigned i=0;i<pc->getNumStages(); i++) {
        char bf[16];
        sprintf_s(bf, "R%d", i);
        if (!gdi.hasWidget("SI" + itos(i))) // Skip if field not loaded in page
          continue;

        if (pc->getLegRunner(i)==i) {

          const wstring name=gdi.getText(bf);
          if (name.empty()) { //Remove
            t->removeRunner(gdi, true, i);
          }
          else {
            pRunner r=t->getRunner(i);
            char bf2[16];
            sprintf_s(bf2, "SI%d", i);
            int cardNo = gdi.getTextNo(bf2);

            if (r) {
              bool newName = !r->matchName(name);
              int oldId = gdi.getExtraInt(bf);
              // Same runner set
              if (oldId == r->getId()) {
                if (newName) {
                  r->updateFromDB(name, r->getClubId(), r->getClassId(false),
                                  cardNo, 0, false);
                  r->setName(name, true);
                }
                r->setCardNo(cardNo, true);
                if (gdi.isChecked("RENT" + itos(i)))
                  r->getDI().setInt("CardFee", oe->getBaseCardFee());
                else
                  r->getDI().setInt("CardFee", 0);

                r->synchronize(true);
                continue;
              }

              if (newName) {
                if (!t->getClub().empty())
                  r->setClub(t->getClub());
                r->resetPersonalData();
                r->updateFromDB(name, r->getClubId(), r->getClassId(false),
                                cardNo, 0, false);
              }
            }
            else 
              r = oe->addRunner(name, t->getClubId(), t->getClassId(false), cardNo, 0, false);

            r->setName(name, true);
            r->setCardNo(cardNo, true);
            if (gdi.isChecked("RENT" + itos(i)))
              r->getDI().setInt("CardFee", oe->getBaseCardFee());
            else
              r->getDI().setInt("CardFee", 0);

            r->synchronize();
            t->setRunner(i, r, true);
          }
        }
      }
    }

    if (setTeamStatus)
      t->setTeamMemberStatus(sIn);

    if (t->checkValdParSetup()) {
      gdi.alert("Laguppställningen hade fel, som har rättats");
    }

    if (t->getRunner(0))
      t->getRunner(0)->setStartTimeS(start);

    t->applyBibs();
    t->evaluate(oBase::ChangeType::Update);

    if (globalDep)
      oe->reEvaluateAll(classes, false);

    if (!dontReloadTeams) {
      fillTeamList(gdi);
      //updateTeamStatus(gdi, t);
    }
    if (checkStatus && sIn != t->getStatus()) {
      gdi.alert("Status matchar inte deltagarnas status.");
    }
  }

  if (create) {
    selectTeam(gdi, 0);
    gdi.setInputFocus("Name", true);
  }
  else if (!dontReloadTeams) {
    selectTeam(gdi, t);
  }
  return true;
}

int TabTeam::teamCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Save") {
      return save(gdi, false);
    }
    if (bi.id == "Cancel") {
       loadPage(gdi);
       return 0;
    }
    else if (bi.id=="TableMode") {
      if (currentMode == 0 && teamId>0)
        save(gdi, true);

      currentMode = 1;
      loadPage(gdi);
    }
    else if (bi.id=="FormMode") {
      if (currentMode != 0) {
        currentMode = 0;
        gdi.enableTables();
        loadPage(gdi);
      }
    }
    else if (bi.id=="Undo") {
      pTeam t = oe->getTeam(teamId);
      selectTeam(gdi, t);
      return 0;
    }
    else if (bi.id=="Search") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Teams", lbi);
      oe->fillTeams(gdi, "Teams");
      unordered_set<int> foo;
      pTeam t=oe->findTeam(gdi.getText("SearchText"), lbi.data, foo);

      if (t) {
        selectTeam(gdi, t);
        gdi.selectItemByData("Teams", t->getId());
      }
      else
        gdi.alert("Laget hittades inte");
    }
    else if (bi.id == "ImportTeams") {
      if (teamId>0)
        save(gdi, true);
      showTeamImport(gdi);
    }
    else if (bi.id == "DoImportTeams") {
      doTeamImport(gdi);
    }
    else if (bi.id == "AddTeamMembers") {
       if (teamId>0)
        save(gdi, true);
      showAddTeamMembers(gdi);
    }
    else if (bi.id == "DoAddTeamMembers") {
      doAddTeamMembers(gdi);
    }
    else if (bi.id == "SaveTeams") {
      saveTeamImport(gdi, bi.getExtraInt() != 0);
    }
    else if (bi.id == "ShowAll") {
      fillTeamList(gdi);
    }
    else if (bi.id == "DirectEntry") {
      gdi.restore("DirectEntry", false);
      gdi.fillDown();
      gdi.dropLine();
      gdi.addString("", boldText, "Direktanmälan");
      gdi.addString("", 0, "Du kan använda en SI-enhet för att läsa in bricknummer.");
      gdi.dropLine(0.2);
   
      int leg = bi.getExtraInt();
      gdi.fillRight();
      gdi.addInput("DirName", L"", 16, TeamCB, L"Namn:");
      gdi.addInput("DirCard", L"", 8, TeamCB, L"Bricka:");

      TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
      tsi.setCardNumberField("DirCard");
      gdi.setPostClearCb(TeamCB);
      bool rent = false;
      gdi.dropLine(1.1);
   
      gdi.addCheckbox("DirRent", "Hyrd", 0, rent);
      gdi.dropLine(-0.2);
  
      gdi.addButton("DirOK", "OK", TeamCB).setDefault().setExtra(leg);
      gdi.addButton("Cancel", "Avbryt", TeamCB).setCancel();
     
      gdi.disableInput("DirOK");
      gdi.refreshFast();
    }
    else if (bi.id == "DirOK") {
      pTeam t = oe->getTeam(teamId);
      if (!t || !t->getClassRef(false))
        return 0;

      int leg = bi.getExtraInt();
      wstring name = gdi.getText("DirName");
      int storedId = gdi.getBaseInfo("DirName").getExtraInt();
      
      int card = gdi.getTextNo("DirCard");
      
      
      if (card <= 0 || name.empty())
        throw meosException("Internal error"); //Cannot happen
      
      pRunner r = 0;
      if (storedId > 0) {
        r = oe->getRunner(storedId, 0);
        if (r != 0 && (r->getName() != name || r->getCardNo() != card))
          r = 0; // Ignore match
      }
      
      bool rExists = r != 0;
      
      pRunner old = oe->getRunnerByCardNo(card, 0, oEvent::CardLookupProperty::CardInUse);
      if (old && r != old) {
        throw meosException(L"Brickan används av X.#" + old->getName() );
      }

      pClub clb = 0;
      if (!rExists) {
        pRunner rOrig = oe->getRunnerByCardNo(card, 0, oEvent::CardLookupProperty::Any);
        if (rOrig)
          clb = rOrig->getClubRef();
      }

      bool rent = gdi.isChecked("DirRent");

      if (r == 0) {
        r = oe->addRunner(name, clb ? clb->getId() : t->getClubId(), t->getClassId(false), card, 0, false);
      }
      if (rent)
        r->getDI().setInt("CardFee", oe->getBaseCardFee());

      t->synchronize();
      pRunner oldR = t->getRunner(leg);
      t->setRunner(leg, 0, false);
      t->synchronize(true);
      if (oldR) {
        if (rExists) {
          switchRunners(t, leg, r, oldR);
        }
        else {
          oldR->setClassId(0, true);
          vector<int> mp;
          oldR->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
          oldR->synchronize(true);
          t->setRunner(leg, r, true);
          t->checkValdParSetup();

          if (oldR->isAnnonumousTeamMember())
            oe->removeRunner({ oldR->getId() });
          t->synchronize(true);
        }
      }
      else {
        t->setRunner(leg, r, true);
        t->checkValdParSetup();
        t->synchronize(true);
      }
      selectTeam(gdi, t);
    }
    else if (bi.id == "Browse") {
      const wchar_t *target = bi.getExtra();
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Laguppställning", L"*.csv;*.txt"));
      wstring fileName = gdi.browseForOpen(ext, L"csv");
      if (!fileName.empty())
        gdi.setText(target, fileName);
    }
    else if (bi.id == "ChangeKey") {
      pTeam t = oe->getTeam(teamId);
      if (!t || !t->getClassRef(false))
        return 0;

      pClass  pc = t->getClassRef(false);
      gdi.restore("ChangeKey", false);
      gdi.fillRight();
      gdi.pushX();
      gdi.dropLine();
      gdi.addSelection("ForkKey", 100, 400, 0, L"Gafflingsnyckel:");
      int nf = pc->getNumForks();
      vector< pair<wstring, size_t> > keys;
      for (int f = 0; f < nf; f++) {
        keys.push_back( make_pair(itow(f+1), f));
      }
      int currentKey = max(t->getStartNo()-1, 0) % nf;
      gdi.addItem("ForkKey", keys);
      gdi.selectItemByData("ForkKey", currentKey);

      gdi.dropLine(0.9);
      gdi.addButton("SaveKey", "Ändra", TeamCB);
      gdi.refreshFast();
    }
    else if (bi.id == "SaveKey") {
      pTeam t = oe->getTeam(teamId);
      if (!t || !t->getClassRef(false))
        return 0;
      
      pClass  pc = t->getClassRef(false);
      int nf = pc->getNumForks();
      ListBoxInfo lbi;
      gdi.getSelectedItem("ForkKey", lbi);
      for (int k = 0; k < nf; k++) {
        for (int j = 0; j < 2; j++) {
          int newSno = t->getStartNo();
          if (j == 0)
            newSno += k;
          else 
            newSno -=k;

          if (newSno <= 0)
            continue;

          int currentKey = max(newSno-1, 0) % nf;
          if (currentKey == lbi.data) { 
            t->setStartNo(newSno, oBase::ChangeType::Update);
            t->synchronize(true);
            t->evaluate(oBase::ChangeType::Update);
            selectTeam(gdi, t);
            return 0;
          }
        }
      }
    }
    else if (bi.id.substr(0,2)=="MR") {
      int leg = atoi(bi.id.substr(2, string::npos).c_str());

      if (teamId != 0) {
        save(gdi, false);
        pTeam t = oe->getTeam(teamId);
        if (t != 0) {
          pRunner r = t->getRunner(leg);
          if (r) {
            TabRunner *tr = (TabRunner *)gdi.getTabs().get(TRunnerTab);
            tr->loadPage(gdi, r->getId());
          }
        }
      }
    }
    else if (bi.id.substr(0,2)=="DR") {
      int leg=atoi(bi.id.substr(2, string::npos).c_str());
      pTeam t = oe->getTeam(teamId);
      if (t == 0)
        return 0;
      pClass pc = t->getClassRef(false);
      if (pc == 0)
        return 0;

      gdi.restore("SelectR", false);
      gdi.setRestorePoint("SelectR");
      gdi.dropLine();
      gdi.fillDown();

      gdi.addString("", fontMediumPlus, L"Välj löpare för sträcka X#" + pc->getLegNumber(leg));
      gdi.setData("Leg", leg);
      
      gdi.setRestorePoint("DirectEntry");
      gdi.addString("", 0, "help:teamwork");
      gdi.dropLine(0.5);

      gdi.addButton("DirectEntry", "Direktanmälan...", TeamCB).setExtra(leg);
      set<int> presented;
      gdi.pushX();
      gdi.fillRight();
      int w,h;
      gdi.getTargetDimension(w, h);
      w = max(w, gdi.getWidth());
      int limit = max(w - gdi.scaleLength(150), gdi.getCX() + gdi.scaleLength(200));
      set< pair<wstring, int> > rToList;
      set<int> usedR;
      wstring anon = lang.tl("N.N.");
      set<int> clubs;
      
      for (int i = 0; i < t->getNumRunners(); i++) {
        if (pc->getLegRunnerIndex(i) == 0) {
          pRunner r = t->getRunner(i);
          if (!r)
            continue;
          if (r->getClubId() > 0)
            clubs.insert(r->getClubId()); // Combination teams
          if (r && r->getName() != anon) {
            rToList.insert( make_pair(r->getName(), r->getId()));
          }
        }
      }
      showRunners(gdi, "Från laget", rToList, limit, usedR);
      vector<pRunner> clsR;
      oe->getRunners(0, 0, clsR, true);
      rToList.clear();
      if (t->getClubId() > 0) 
        clubs.insert(t->getClubId());
      
      if (!clubs.empty()) {
        for (size_t i = 0; i < clsR.size(); i++) {
          if (clsR[i]->getRaceNo() > 0)
            continue;
          if (clubs.count(clsR[i]->getClubId()) == 0)
            continue;
          if (clsR[i]->getClassId(false) != t->getClassId(false))
            continue;
          if (clsR[i]->getName() == anon)
            continue;
          rToList.insert( make_pair(clsR[i]->getName(), clsR[i]->getId()));
        }

        showRunners(gdi, "Från klassen", rToList, limit, usedR);
        rToList.clear();
     
        for (size_t i = 0; i < clsR.size(); i++) {
          if (clsR[i]->getRaceNo() > 0)
            continue;
          if (clubs.count(clsR[i]->getClubId()) == 0)
            continue;
          if (clsR[i]->getName() == anon)
            continue;
          rToList.insert( make_pair(clsR[i]->getName(), clsR[i]->getId()));
        }
        showRunners(gdi, "Från klubben", rToList, limit, usedR);
      }
      
     
      vector< pair<wstring, size_t> > otherR;

      for (size_t i = 0; i < clsR.size(); i++) {
        if (clsR[i]->getRaceNo() > 0)
          continue;
        if (clsR[i]->getName() == anon)
          continue;
        if (usedR.count(clsR[i]->getId()))
          continue;
        const wstring &club = clsR[i]->getClub();
        wstring id = clsR[i]->getName() + L", " + clsR[i]->getClass(false);
        if (!club.empty())
          id += L" (" + club + L")";
        
        otherR.push_back(make_pair(id, clsR[i]->getId()));
      }
      gdi.fillDown();
      if (!otherR.empty()) {
        gdi.addString("", 1, "Övriga");
        gdi.fillRight();
        gdi.addSelection("SelectR", 250, 400, TeamCB);
        gdi.addButton("SelectRunner", "OK", TeamCB).setExtra(leg);
        gdi.fillDown();
        gdi.addButton("Cancel", "Avbryt", TeamCB);
        
        gdi.addItem("SelectR", otherR);
      }
      else { 
        gdi.addButton("Cancel", "Avbryt", TeamCB);
      }
      gdi.refresh();
    }
    else if (bi.id=="SelectRunner") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("SelectR", lbi);
      pRunner r = oe->getRunner(lbi.data, 0);
      if (r == 0) {
        throw meosException("Ingen deltagare vald.");
      }
      int leg = gdi.getDataInt("Leg");
      
      pTeam t = oe->getTeam(teamId);
      processChangeRunner(gdi, t, leg, r);
    }
    else if (bi.id=="Add") {
      if (teamId>0) {

        wstring name = gdi.getText("Name");
        pTeam t = oe->getTeam(teamId);
        if (!name.empty() && t && t->getName() != name) {
          if (gdi.ask(L"Vill du lägga till laget 'X'?#" + name)) {
            t = oe->addTeam(name);
            teamId = t->getId();
          }
          save(gdi, false);
          return true;
        }

        save(gdi, false);
      }

      pTeam t = oe->addTeam(oe->getAutoTeamName());

      ListBoxInfo lbi;
      gdi.getSelectedItem("RClass", lbi);

      int clsId;
      if (signed(lbi.data)>0)
        clsId = lbi.data;
      else
        clsId = oe->getFirstClassId(true);

      pClass pc = oe->getClass(clsId);
      if (pc) {
        pair<int, wstring> snoBib = pc->getNextBib();
        if (snoBib.first > 0) {
          t->setBib(snoBib.second, snoBib.first, true);
        }
      }

      t->setClassId(clsId, true);
      t->synchronize();

      fillTeamList(gdi);
      //oe->fillTeams(gdi, "Teams");
      selectTeam(gdi, t);

      //selectTeam(gdi, 0);
      //gdi.selectItemByData("Teams", -1);
      gdi.setInputFocus("Name", true);
    }
    else if (bi.id=="Remove") {
      DWORD tid=teamId;

      if (tid==0)
        throw std::exception("Inget lag valt.");

      pTeam t = oe->getTeam(tid);

      if (!t || t->isRemoved()) {
        selectTeam(gdi, 0);
      }
      else if (gdi.ask(L"Vill du verkligen ta bort laget?")) {
        vector<int> runners;
        vector<int> noRemove;
        for (int k = 0; k < t->getNumRunners(); k++) {
          pRunner r = t->getRunner(k);
          if (r && r->getRaceNo() == 0) {
            if (r->getCard() == 0)
              runners.push_back(r->getId());
            else
              noRemove.push_back(r->getId());
          }
        }
        oe->removeTeam(tid);
        oe->removeRunner(runners);

        for (size_t k = 0; k<noRemove.size(); k++) {
          pRunner r = oe->getRunner(noRemove[k], 0);
          if (r) {
            r->setClassId(0, true);
            r->synchronize();
          }
        }

        fillTeamList(gdi);
        //oe->fillTeams(gdi, "Teams");
        selectTeam(gdi, 0);
        gdi.selectItemByData("Teams", -1);
      }
    }
  }
  else if (type == GUI_LINK) {
    TextInfo ti = dynamic_cast<TextInfo &>(*(BaseInfo *)data);
    if (ti.id == "SelectR") {
      int leg = gdi.getDataInt("Leg");
      pTeam t = oe->getTeam(teamId);
      int rid = ti.getExtraInt();
      pRunner r = oe->getRunner(rid, 0);
      processChangeRunner(gdi, t, leg, r);
    }
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Teams") {
      if (gdi.isInputChanged("")) {
        pTeam t = oe->getTeam(teamId);
        bool newName = t && t->getName() != gdi.getText("Name");
        bool newBib = gdi.hasWidget("StartNo") && t && t->getBib() != gdi.getText("StartNo");
        save(gdi, true);

        if (newName || newBib) {
          fillTeamList(gdi);
        }
      }
      pTeam t=oe->getTeam(bi.data);
      selectTeam(gdi, t);
    }
    else if (bi.id=="RClass") { //New class selected.
      DWORD tid=teamId;
      //gdi.getData("TeamID", tid);

      if (tid){
        pTeam t=oe->getTeam(tid);
        pClass pc=oe->getClass(bi.data);

        if (pc && pc->getNumDistinctRunners() == shownDistinctRunners &&
          pc->getNumStages() == shownRunners) {
            // Keep team setup, i.e. do nothing
        }
        else if (t && pc && (t->getClassId(false)==bi.data
                || t->getNumRunners()==pc->getNumStages()) )
          loadTeamMembers(gdi, 0,0,t);
        else
          loadTeamMembers(gdi, bi.data, 0, t);
      }
      else loadTeamMembers(gdi, bi.data, 0, 0);
    }
    else {

      ListBoxInfo lbi;
      gdi.getSelectedItem("RClass", lbi);

      if (signed(lbi.data)>0){
        pClass pc=oe->getClass(lbi.data);

        if (pc) {
          for(unsigned i=0;i<pc->getNumStages();i++){
            char bf[16];
            sprintf_s(bf, "R%d", i);
            if (bi.id==bf){
              pRunner r=oe->getRunner(bi.data, 0);
              if (r) {
                sprintf_s(bf, "SI%d", i);
                int cno = r->getCardNo();
                gdi.setText(bf, cno > 0 ? itow(cno) : L"");
                warnDuplicateCard(gdi, bf, cno, r);
              }
            }
          }
        }
      }
    }
  }
  else if (type==GUI_INPUTCHANGE) {
    InputInfo &ii=*(InputInfo *)data;
    pClass pc=oe->getClass(classId);
    if (pc){
      for(unsigned i=0;i<pc->getNumStages();i++){
        char bf[16];
        sprintf_s(bf, "R%d", i);
        if (ii.id == bf) {
          for (unsigned k=i+1; k<pc->getNumStages(); k++) {
            if (pc->getLegRunner(k)==i) {
              sprintf_s(bf, "R%d", k);
              gdi.setText(bf, ii.text);
            }
          }
          break;
        }
      }
    }

    if (ii.id == "DirName" || ii.id == "DirCard") {
      int cno = gdi.getTextNo("DirCard");
      if (cno > 0 && oe->hasHiredCardData()) {
        gdi.check("DirRent", oe->isHiredCard(cno));
      }
      gdi.setInputStatus("DirOK", !gdi.getText("DirName").empty() && cno > 0);

    }

    if (oe->useRunnerDb() && ii.id == "DirName") {
      auto &db = oe->getRunnerDatabase();
      bool show = false;
      if (ii.text.length() > 1) {
        auto dbClub = TabRunner::extractClub(oe, gdi);
        auto rw = db.getRunnerSuggestions(ii.text, dbClub ? dbClub->getId() : 0, 10);
        if (dbClub != nullptr && rw.empty()) // Default: Any club
          rw = db.getRunnerSuggestions(ii.text, 0, 10);

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
  else if (type == GUI_INPUT) {
    InputInfo &ii=*(InputInfo *)data;
    if (ii.id == "DirName" || ii.id == "DirCard") {
      gdi.setInputStatus("DirOK", !gdi.getText("DirName").empty() &&
                                  gdi.getTextNo("DirCard") > 0);

    }
    if (ii.id == "DirCard") {
      int cno = gdi.getTextNo("DirCard");
      if (cno > 0 && gdi.getText("DirName").empty()) {
        bool matched = false;
        pRunner r = oe->getRunnerByCardNo(cno, 0, oEvent::CardLookupProperty::ForReadout);
        if (r && (r->getStatus() == StatusUnknown || r->getStatus() == StatusDNS) ) {
          // Switch to exactly this runner. Has not run before
          gdi.setText("DirName", r->getName())->setExtra(r->getId());
          matched = true;
        }
        else {
          r = oe->getRunnerByCardNo(cno, 0, oEvent::CardLookupProperty::Any);
          if (r) {
            // Copy only the name.
            gdi.setText("DirName", r->getName())->setExtra(0);
            matched = true;
          }
          else if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::RunnerDb)) {
            const RunnerWDBEntry *rdb = oe->getRunnerDatabase().getRunnerByCard(cno);
            if (rdb) {
              wstring name;
              rdb->getName(name);
              gdi.setText("DirName", name)->setExtra(0);
              matched = true;
            }
          }
        }
        if (r)
          gdi.check("DirRent", r->getDCI().getInt("CardFee") != 0);
        if (matched)
          gdi.setInputStatus("DirOK", true);
      }
    }
    pClass pc=oe->getClass(classId);
    if (pc) {
      for (unsigned i = 0; i < pc->getNumStages(); i++) {
        if (ii.id == "SI" + itos(i)) {
          int cardNo = _wtoi(ii.text.c_str());
          pTeam t = oe->getTeam(teamId);
          if (t) {
            pRunner r = t->getRunner(i);
            if (ii.changedInput() && oe->hasHiredCardData()) {
              gdi.check("RENT" + itos(i), oe->isHiredCard(cardNo));
            }
            warnDuplicateCard(gdi, ii.id, cardNo, r);
          }
          break;
        }
      }
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
    if (teamId>0)
      save(gdi, true);

    return true;
  }
  else if (type==GUI_POSTCLEAR) {
    // Clear out SI-link
    TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
    tsi.setCardNumberField("");
    return true;
  }
  return 0;
}


void TabTeam::loadTeamMembers(gdioutput &gdi, int ClassId, int ClubId, pTeam t)
{
  if (ClassId==0)
    if (t) ClassId=t->getClassId(false);

  classId=ClassId;
  gdi.restore("",false);

  pClass pc=oe->getClass(ClassId);
  if (!pc) return;

  shownRunners = pc->getNumStages();
  shownDistinctRunners = pc->getNumDistinctRunners();

  gdi.setRestorePoint();
  gdi.newColumn();

  gdi.fillDown();
  char bf[16];
  char bf_si[16];
  int xp = gdi.getCX();
  int yp = gdi.getCY();
  int numberPos = xp;
  xp += gdi.scaleLength(25);
  int dx[6] = {0, 184, 220, 290, 316, 364};
  dx[1] = gdi.getInputDimension(18).first + gdi.scaleLength(4);
  for (int i = 2; i<6; i++)
    dx[i] = dx[1] + gdi.scaleLength(dx[i]-188);

  gdi.addString("", yp, xp + dx[0], 0, "Namn:");
  gdi.addString("", yp, xp + dx[2], 0, "Bricka:");
  gdi.addString("", yp, xp + dx[3], 0, "Hyrd:");
  gdi.addString("", yp, xp + dx[5], 0, "Status:");
  gdi.dropLine(0.5);

  for (unsigned i = 0; i < pc->getNumStages(); i++) {
    yp = gdi.getCY() - gdi.scaleLength(3);

    sprintf_s(bf, "R%d", i);
    gdi.pushX();
    bool hasSI = false;
    gdi.addStringUT(yp, numberPos, 0, pc->getLegNumber(i) + L".");
    if (pc->getLegRunner(i) == i) {

      gdi.addInput(xp + dx[0], yp, bf, L"", 18, TeamCB);//Name
      gdi.addButton(xp + dx[1], yp - 2, gdi.scaleLength(28), "DR" + itos(i), "<>", TeamCB, "Knyt löpare till sträckan.", false, false); // Change
      sprintf_s(bf_si, "SI%d", i);
      hasSI = true;
      gdi.addInput(xp + dx[2], yp, bf_si, L"", 5, TeamCB).setExtra(i); //Si

      gdi.addCheckbox(xp + dx[3], yp + gdi.scaleLength(10), "RENT" + itos(i), "", 0, false); //Rentcard
    }
    else {
      gdi.addInput(xp + dx[0], yp, bf, L"", 18, 0);//Name
      gdi.disableInput(bf);
    }
    gdi.addButton(xp + dx[4], yp - 2, gdi.scaleLength(38), "MR" + itos(i), "...", TeamCB, "Redigera deltagaren.", false, false); // Change

    gdi.addString(("STATUS" + itos(i)).c_str(), yp + gdi.scaleLength(5), xp + dx[5], 0, "#MMMMMMMMMMMMMMMM");
    gdi.setText("STATUS" + itos(i), L"", false);
    gdi.dropLine(0.5);
    gdi.popX();

    if (t) {
      pRunner r = t->getRunner(i);
      if (r) {
        gdi.setText(bf, r->getNameRaw())->setExtra(r->getId());

        if (hasSI) {
          int cno = r->getCardNo();
          gdi.setText(bf_si, cno > 0 ? itow(cno) : L"");
          warnDuplicateCard(gdi, bf_si, cno, r);
          gdi.check("RENT" + itos(i), r->getDCI().getInt("CardFee") != 0);
        }
        string sid = "STATUS" + itos(i);
        if (r->statusOK(true)) {
          TextInfo * ti = (TextInfo *)gdi.setText(sid, L"OK, " + r->getRunningTimeS(true), false);
          if (ti)
            ti->setColor(colorGreen);
        }
        else if (r->getStatusComputed() != StatusUnknown) {
          TextInfo * ti = (TextInfo *)gdi.setText(sid, r->getStatusS(false, true) + L", " + r->getRunningTimeS(true), false);
          if (ti)
            ti->setColor(colorRed);
        }
      }
    }
  }

  gdi.setRestorePoint("SelectR");
  gdi.addString("", 1, "help:7618");
  gdi.dropLine();
  int numF = pc->getNumForks();
  
  if (numF>1 && t) {
    gdi.addString ("", 1, "Gafflingsnyckel X#" + itos(1+(max(t->getStartNo()-1, 0) % numF))).setColor(colorGreen);
    wstring crsList;
    bool hasCrs = false;
    for (size_t k = 0; k < pc->getNumStages(); k++) {
      pCourse crs = pc->getCourse(k, t->getStartNo());
      wstring cS; 
      if (crs != 0) {
        cS = crs->getName();
        hasCrs = true;
      }
      else
        cS = makeDash(L"-");
    
      if (!crsList.empty())
        crsList += L", ";
      crsList += cS;

      if (hasCrs && crsList.length() > 50) {
        gdi.addStringUT(0, crsList);
        crsList.clear();
      }
    }
    if (hasCrs && !crsList.empty()) {
      gdi.addStringUT(0, crsList);
    }

    if (hasCrs) {
      gdi.dropLine(0.5);
      gdi.setRestorePoint("ChangeKey");
      gdi.addButton("ChangeKey", "Ändra lagets gaffling", TeamCB);
    }
  }
  gdi.refresh();
}

bool TabTeam::loadPage(gdioutput &gdi, int id) {
  teamId = id;
  return loadPage(gdi);
}

bool TabTeam::loadPage(gdioutput &gdi)
{
  shownRunners = 0;
  shownDistinctRunners = 0;

  oe->checkDB();
  oe->reEvaluateAll(set<int>(), true);

  gdi.selectTab(tabId);
  gdi.clearPage(false);

  if (currentMode == 1) {
    addToolbar(gdi);
    gdi.dropLine(1);
    gdi.addTable(oTeam::getTable(oe), gdi.getCX(), gdi.getCY());
    return true;
  }

  gdi.fillDown();
  gdi.addString("", boldLarge, "Lag(flera)");

  gdi.pushX();
  gdi.fillRight();

  gdi.registerEvent("SearchRunner", teamSearchCB).setKeyCommand(KC_FIND);
  gdi.registerEvent("SearchRunnerBack", teamSearchCB).setKeyCommand(KC_FINDBACK);
  gdi.addInput("SearchText", L"", 17, teamSearchCB, L"", L"Sök på namn, bricka eller startnummer.").isEdit(false).setBgColor(colorLightCyan).ignore(true);
  gdi.dropLine(-0.2);
  gdi.addButton("ShowAll", "Visa alla", TeamCB).isEdit(false);

  gdi.dropLine(2);
  gdi.popX();
  gdi.fillDown();
  gdi.addListBox("Teams", 250, 440, TeamCB, L"", L"").isEdit(false).ignore(true);
  gdi.setInputFocus("Teams");
  fillTeamList(gdi);

  int posXForButtons = gdi.getCX();
  int posYForButtons = gdi.getCY();
  
  gdi.newColumn();
  gdi.fillDown();
  gdi.pushX();
  gdi.addInput("Name", L"", 24, 0, L"Lagnamn:");

  gdi.fillRight();
  bool drop = false;
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Bib)) {
    gdi.addInput("StartNo", L"", 4, 0, L"Nr:", L"Nummerlapp");
    drop = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy);
  }

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    gdi.addCombo("Club", 180, 300, TeamCB, L"Klubb:");
    oe->fillClubs(gdi, "Club");
    drop = true;
  }

  if (drop) {
    gdi.dropLine(3);
    gdi.popX();
  }

  gdi.addSelection("RClass", 170, 300, TeamCB, L"Klass:");
  oe->fillClasses(gdi, "RClass", oEvent::extraNone, oEvent::filterOnlyMulti);
  gdi.addItem("RClass", lang.tl("Ny klass"), 0);

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy))
    gdi.addInput("Fee", L"", 5, 0, L"Avgift:");

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(3);

  gdi.pushX();
  gdi.fillRight();

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

  /*if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::TimeAdjust)) {
    gdi.addInput("TimeAdjust", "", 5, 0, "Tidstillägg:");
  }
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::PointAdjust)) {
    gdi.addInput("PointAdjust", "", 5, 0, "Poängavdrag:");
  }*/

  gdi.fillDown();
  gdi.dropLine(3);
  gdi.popX();

  gdi.pushX();
  gdi.fillRight();

  gdi.addInput("Time", L"", 6, 0, L"Tid:").isEdit(false).ignore(true);
  gdi.disableInput("Time");

  gdi.fillDown();
  gdi.addSelection("Status", 100, 160, 0, L"Status:", L"tooltip_explain_status");
  oe->fillStatus(gdi, "Status");

  gdi.popX();
  gdi.selectItemByData("Status", 0);

  gdi.addString("TeamInfo", 0, "").setColor(colorRed);
  gdi.dropLine(0.4);

  if (oe->hasAnyRestartTime()) {
    gdi.addCheckbox("NoRestart", "Förhindra omstart", 0, false, "Förhindra att laget deltar i någon omstart");
  }
  gdi.dropLine(1.5);

  const bool multiDay = oe->hasPrevStage();

  if (multiDay) {
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
    gdi.dropLine(1.5);
    gdi.popX();
  }
  
  gdi.fillRight();
  gdi.addButton("Save", "Spara", TeamCB, "help:save");
  gdi.disableInput("Save");
  gdi.addButton("Undo", "Ångra", TeamCB);
  gdi.disableInput("Undo");

  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("Remove", "Radera", TeamCB);
  gdi.disableInput("Remove");
  gdi.addButton("Add", "Nytt lag", TeamCB);

  gdi.setOnClearCb(TeamCB);

  addToolbar(gdi);
  
  RECT rc;
  rc.left = posXForButtons;
  rc.top = posYForButtons;

  gdi.setCY(posYForButtons + gdi.scaleLength(10));
  gdi.setCX(posXForButtons + gdi.getLineHeight());
  gdi.fillDown();
  gdi.addString("", fontMediumPlus, "Verktyg");
  gdi.dropLine(0.3);
  gdi.fillRight();
  gdi.addButton("ImportTeams", "Importera laguppställningar", TeamCB);
  gdi.addButton("AddTeamMembers", "Skapa anonyma lagmedlemmar", TeamCB, "Fyll obesatta sträckor i alla lag med anonyma tillfälliga lagmedlemmar (N.N.)");
  rc.right = gdi.getCX() + gdi.getLineHeight();
  gdi.dropLine(2);
  rc.bottom = gdi.getHeight();
  gdi.addRectangle(rc, colorLightBlue);

  gdi.setRestorePoint();

  selectTeam(gdi, oe->getTeam(teamId));

  gdi.refresh();
  return true;
}

void TabTeam::fillTeamList(gdioutput &gdi) {
  timeToFill = GetTickCount();
  oe->fillTeams(gdi, "Teams");
  timeToFill = GetTickCount() - timeToFill;
  lastSearchExpr = L"";
  ((InputInfo *)gdi.setText("SearchText", getSearchString()))->setFgColor(colorGreyBlue);
    lastFilter.clear();
}


const wstring &TabTeam::getSearchString() const {
  return lang.tl(L"Sök (X)#Ctrl+F");
}

void TabTeam::addToolbar(gdioutput &gdi) const {

  const int button_w=gdi.scaleLength(130);

  gdi.addButton(2+0*button_w, 2, button_w, "FormMode",
    "Formulärläge", TeamCB, "", false, true).fixedCorner();
  gdi.check("FormMode", currentMode==0);

  gdi.addButton(2+1*button_w, 2, button_w, "TableMode",
            "Tabelläge", TeamCB, "", false, true).fixedCorner();
  gdi.check("TableMode", currentMode==1);

}

void TabTeam::showTeamImport(gdioutput &gdi) {
  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Importera laguppställningar");

  gdi.addString("", 10, "help:teamlineup");
  gdi.dropLine();
  gdi.setRestorePoint("TeamLineup");
  gdi.pushX();

  gdi.fillRight();
  gdi.addInput("FileName", L"", 40, 0, L"Filnamn:");
  gdi.dropLine(0.9);
  gdi.addButton("Browse", "Bläddra", TeamCB).setExtra(L"FileName");
  gdi.dropLine(3);
  gdi.popX();
  gdi.fillDown();
  gdi.addCheckbox("OnlyExisting", "Använd befintliga deltagare", 0, false,
    "Knyt redan anmälda deltagare till laget (identifiera genom namn och/eller bricka)");
  gdi.fillRight();
  gdi.addButton("DoImportTeams", "Importera", TeamCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", TeamCB).setCancel();

  gdi.refresh();
}

void TabTeam::doTeamImport(gdioutput &gdi) {
  wstring file = gdi.getText("FileName");
  bool useExisting = gdi.isChecked("OnlyExisting");


  csvparser csv;
  map<wstring, int> classNameToNumber;
  vector<pClass> cls;
  oe->getClasses(cls, true);
  for (size_t k = 0; k < cls.size();k++) {
    classNameToNumber[cls[k]->getName()] = cls[k]->getNumStages();
  }
  gdi.fillDown();
  csv.importTeamLineup(file, classNameToNumber, teamLineup);

  gdi.restore("TeamLineup", false);

  gdi.dropLine();
  for (size_t k = 0; k < teamLineup.size(); k++) {
    wstring tdesc = teamLineup[k].teamClass + L", " + teamLineup[k].teamName;
    if (!teamLineup[k].teamClub.empty())
      tdesc += L", " + teamLineup[k].teamClub;
    gdi.addStringUT(1, tdesc);
    for (size_t j = 0; j < teamLineup[k].members.size(); j++) {
      TeamLineup::TeamMember &member = teamLineup[k].members[j];
      if (member.name.empty())
        continue;

      wstring mdesc = L" " + itow(j+1) + L". ";
      bool warn = false;
      
      if (useExisting) {
        pRunner r = findRunner(member.name, member.cardNo);
        if (r != 0)
          mdesc += r->getCompleteIdentification();
        else {
          mdesc += member.name + lang.tl(L" (ej funnen)");
          warn = true;
        }
      }
      else {
        mdesc += member.name + L" (" + itow(member.cardNo) + L") " + member.club;
      }

      if (!member.course.empty()) {
        if (oe->getCourse(member.course))
          mdesc += L" : " + member.course;
        else {
          mdesc += L" : " + lang.tl(L"Banan saknas");
          warn = true;
        }
      }

      if (!member.cls.empty()) {
        if (oe->getClass(member.cls))
          mdesc += L" [" + member.cls + L"]";
        else {
          mdesc += L" " + lang.tl(L"Klassen saknas");
          warn = true;
        }
      }

      TextInfo &ti = gdi.addStringUT(0, mdesc);
      if (warn)
        ti.setColor(colorRed);
    }
    gdi.dropLine();
  }
  gdi.fillRight();
  gdi.addButton("ImportTeams", "<< Bakåt", TeamCB);
  gdi.addButton("SaveTeams", "Spara laguppställningar", TeamCB).setDefault().setExtra(useExisting);
  gdi.addButton("Cancel", "Avbryt", TeamCB).setCancel();
  gdi.refresh();
}

void TabTeam::saveTeamImport(gdioutput &gdi, bool useExisting) {
  for (size_t k = 0; k < teamLineup.size(); k++) {
    pClub club = !teamLineup[k].teamClub.empty() ? oe->getClubCreate(0, teamLineup[k].teamClub) : 0;
    pTeam t = oe->addTeam(teamLineup[k].teamName, club ? club->getId() : 0, oe->getClass(teamLineup[k].teamClass)->getId());

    for (size_t j = 0; j < teamLineup[k].members.size(); j++) {
      TeamLineup::TeamMember &member = teamLineup[k].members[j];
      if (member.name.empty())
        continue;

      pRunner r = 0;
      if (useExisting) {
        r = findRunner(member.name, member.cardNo);
        if (r && !member.course.empty()) {
          pCourse pc = oe->getCourse(member.course);
          r->setCourseId(pc ? pc->getId() : 0);
        }
        if (r && !member.cls.empty()) {
          pClass rcls = oe->getClass(member.cls);
          r->setClassId(rcls ? rcls->getId() : 0, true);
        }
      }
      else {
        r = oe->addRunner(member.name, member.club, 0, member.cardNo, 0, false);

        if (r && !member.course.empty()) {
          pCourse pc = oe->getCourse(member.course);
          r->setCourseId(pc ? pc->getId() : 0);
        }

        if (r && !member.cls.empty()) {
          pClass rcls = oe->getClass(member.cls);
          r->setClassId(rcls ? rcls->getId() : 0, true);
        }
      }

      t->setRunner(j, r, false);
      if (r)
        r->synchronize(true);
    }

    t->synchronize();
    gdi.dropLine();
  }
  loadPage(gdi);
}

pRunner TabTeam::findRunner(const wstring &name, int cardNo) const {
  wstring n = canonizeName(name.c_str());

  if (cardNo != 0) {
    vector<pRunner> pr;
    oe->getRunnersByCardNo(cardNo, true, oEvent::CardLookupProperty::Any, pr);
    for (size_t k = 0; k < pr.size(); k++) {
      wstring a = canonizeName(pr[k]->getName().c_str());
      if (a == n)
        return pr[k];
    }
  }
  else {
    vector<pRunner> pr;
    oe->getRunners(0, 0, pr, false);
    for (size_t k = 0; k < pr.size(); k++) {
      wstring a = canonizeName(pr[k]->getName().c_str());
      if (a == n)
        return pr[k];
    }
  }
  return 0;
}

void TabTeam::showAddTeamMembers(gdioutput &gdi) {
  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Tillsätt tillfälliga anonyma lagmedlemmar");

  gdi.addString("", 10, "help:anonymous_team");
  gdi.dropLine();
  gdi.pushX();

  gdi.fillDown();
  gdi.addInput("Name", lang.tl("N.N."), 24, 0, L"Anonymt namn:");
  gdi.fillDown();
  gdi.addCheckbox("OnlyRequired", "Endast på obligatoriska sträckor", 0, true);
  gdi.addCheckbox("WithFee", "Med anmälningsavgift (lagets klubb)", 0, true);
  
  gdi.fillRight();
  gdi.addButton("DoAddTeamMembers", "Tillsätt", TeamCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", TeamCB).setCancel();

  gdi.refresh();
}

void TabTeam::doAddTeamMembers(gdioutput &gdi) {
  vector<pTeam> t;
  oe->getTeams(0, t, true);
  bool onlyReq = gdi.isChecked("OnlyRequired");
  bool withFee = gdi.isChecked("WithFee");
  wstring nn = gdi.getText("Name");

  for (size_t k = 0; k < t.size(); k++) {
    pTeam mt = t[k];
    pClass cls = mt->getClassRef(false); 
    if (cls == 0)
      continue;
    bool ch = false;
    for (int j = 0; j < mt->getNumRunners(); j++) {
      if (mt->getRunner(j) == 0) {
        LegTypes lt = cls->getLegType(j);
        if (onlyReq && lt == LTExtra || lt == LTIgnore || lt == LTParallelOptional)
           continue;
        pRunner r = 0;
        if (withFee) {
          r = oe->addRunner(nn, mt->getClubId(), 0, 0, 0, false);  
          r->synchronize();
          mt->setRunner(j, r, false);
          r->addClassDefaultFee(true);
        }
        else {
          r = oe->addRunnerVacant(0);
          r->setName(nn, false);
            //oe->addRunner(nn, oe->getVacantClub(), 0, 0, 0, false);
          r->synchronize();
          mt->setRunner(j, r, false);
        }
        ch = true;
      }
    }
    if (ch) {
      mt->apply(oBase::ChangeType::Update, nullptr);
      mt->synchronize();
      for (int j = 0; j < mt->getNumRunners(); j++) {
        if (mt->getRunner(j) != 0) {
          mt->getRunner(j)->synchronize();
        }
      }
    }
  }

  loadPage(gdi);
}

void TabTeam::showRunners(gdioutput &gdi, const char *title, 
                          const set< pair<wstring, int> > &rToList, 
                          int limitX, set<int> &usedR) {

  if (rToList.empty())
    return;

  bool any = false;
  for(set< pair<wstring, int> >::const_iterator it = rToList.begin(); it != rToList.end(); ++it) {
    if (usedR.count(it->second))
      continue;
    usedR.insert(it->second);
    
    if (!any) {
      gdi.addString("", boldText, title);
      gdi.dropLine(1.2);
      gdi.popX();
      any = true;
    }

    if (gdi.getCX() > limitX) {
      gdi.dropLine(1.5);
      gdi.popX();
    }

    gdi.addString("SelectR", 0, L"#" + it->first, TeamCB).setExtra(it->second);
  }

  if (any) {
    gdi.dropLine(2);
    gdi.popX();
  }
}

void TabTeam::processChangeRunner(gdioutput &gdi, pTeam t, int leg, pRunner r) {
  if (r && t && leg < t->getNumRunners()) {
    pRunner oldR = t->getRunner(leg);
    gdioutput::AskAnswer ans = gdioutput::AnswerNo;
    if (r == oldR) {
      gdi.restore("SelectR");
      return;
    }
    else if (oldR) {
      if (r->getTeam()) {
        ans = gdi.askCancel(L"Vill du att X och Y byter sträcka?#" +
                              r->getName() + L"#" + oldR->getName());
      }
      else {
        ans = gdi.askCancel(L"Vill du att X tar sträckan istället för Y?#" +
                              r->getName() + L"#" + oldR->getName());
      }
    }
    else {
      ans = gdi.askCancel(L"Vill du att X går in i laget?#" + r->getName());
    }

    if (ans == gdioutput::AnswerNo)
      return;
    else if (ans == gdioutput::AnswerCancel) {
      gdi.restore("SelectR");
      return;
    }

    save(gdi, true);
    vector<int> mp;
    switchRunners(t, leg, r, oldR);
    /*if (r->getTeam()) {
      pTeam otherTeam = r->getTeam();
      int otherLeg = r->getLegNumber();
      otherTeam->setRunner(otherLeg, oldR, true);
      if (oldR)
        oldR->evaluateCard(true, mp, 0, true);
      otherTeam->checkValdParSetup();
      otherTeam->apply(true, 0, false);
      otherTeam->synchronize(true);
    }
    else if (oldR) {
      t->setRunner(leg, 0, false);
      t->synchronize(true);
      oldR->setClassId(r->getClassId(), true);
      oldR->evaluateCard(true, mp, 0, true);
      oldR->synchronize(true);
    }

    t->setRunner(leg, r, true);
    r->evaluateCard(true, mp, 0, true);
    t->checkValdParSetup();
    t->apply(true, 0, false);
    t->synchronize(true);*/
    loadPage(gdi);
  }
}

void TabTeam::switchRunners(pTeam t, int leg, pRunner r, pRunner oldR) {
  vector<int> mp;
  bool removeAnnonumousTeamMember = false;
  
  if (r->getTeam()) {
    pTeam otherTeam = r->getTeam();
    int otherLeg = r->getLegNumber();
    otherTeam->setRunner(otherLeg, oldR, true);
    if (oldR)
      oldR->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    otherTeam->checkValdParSetup();
    otherTeam->apply(oBase::ChangeType::Update, nullptr);
    otherTeam->synchronize(true);
  }
  else if (oldR) {
    t->setRunner(leg, 0, false);
    t->synchronize(true);
    if (r->getClassRef(false) && r->getClassRef(false)->isTeamClass())
      oldR->setClassId(0, false);
    else
      oldR->setClassId(r->getClassId(false), true);
    removeAnnonumousTeamMember = oldR->isAnnonumousTeamMember();
    oldR->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    oldR->synchronize(true);
  }

  t->setRunner(leg, r, true);
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
  t->checkValdParSetup();
  t->apply(oBase::ChangeType::Update, nullptr);
  t->synchronize(true);

  if (removeAnnonumousTeamMember)
    oe->removeRunner({ oldR->getId() });
}

void TabTeam::clearCompetitionData() {
  shownRunners = 0;
  shownDistinctRunners = 0;
  teamId = 0;
  inputId = 0;
  timeToFill = 0;
  currentMode = 0;
}

bool TabTeam::warnDuplicateCard(gdioutput &gdi, string id, int cno, pRunner r) {
  pRunner warnCardDupl = 0;

  if (r && !r->getCard() && cno != 0) {
    vector<pRunner> allR;
    oe->getRunnersByCardNo(cno, true, oEvent::CardLookupProperty::Any, allR);
    for (pRunner ar : allR) {
      if (!r->canShareCard(ar, cno)) {
        warnCardDupl = ar;
        break;
      }
    }
  }

  InputInfo &cardNo = dynamic_cast<InputInfo &>(gdi.getBaseInfo(id.c_str()));
  if (warnCardDupl) {
    cardNo.setBgColor(colorLightRed);
    gdi.updateToolTip(id, L"Brickan används av X.#" + warnCardDupl->getCompleteIdentification());
    cardNo.refresh();
    return warnCardDupl->getTeam() == r->getTeam();
  }
  else {
    if (cardNo.getBgColor() != colorDefault) {
      cardNo.setBgColor(colorDefault);
      gdi.updateToolTip(id, L"");
      cardNo.refresh();
    }
    return false;
  }
}

void TabTeam::handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) {
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
      if (runner && runner->dbe().cardNo > 0 && gdi.hasWidget("DirCard") && gdi.getText("DirCard").empty()) {
        gdi.setText("DirCard", runner->dbe().cardNo);
      }
    }
  }
  gdi.clearAutoComplete("");
  gdi.TabFocus(1);
}
