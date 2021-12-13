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

#include "stdafx.h"

#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "csvparser.h"
#include "SportIdent.h"
#include "meos_util.h"
#include "gdifonts.h"
#include "table.h"
#include <cassert>
#include "MeOSFeatures.h"

#include "TabControl.h"

TabControl::TabControl(oEvent *poe):TabBase(poe)
{
  clearCompetitionData();
}

TabControl::~TabControl(void)
{
}

void TabControl::selectControl(gdioutput &gdi,  pControl pc)
{
  if (pc) {
    pc->synchronize();

    if (pc->getStatus() == oControl::StatusStart ||
          pc->getStatus() == oControl::StatusFinish) {
      gdi.selectItemByData("Controls", pc->getId());
      gdi.selectItemByData("Status", oControl::StatusOK);
      gdi.setText("ControlID", makeDash(L"-"), true);

      gdi.setText("Code", L"");
      gdi.setText("Name", pc->getName());
      gdi.setText("TimeAdjust", L"00:00");
      gdi.setText("MinTime", makeDash(L"-"));
      gdi.setText("Point", L"");
      controlId = pc->getId();

      gdi.enableInput("Remove");
      gdi.enableInput("Save");
      gdi.enableEditControls(false);
      gdi.enableInput("Name");
    }
    else {
      gdi.selectItemByData("Controls", pc->getId());
      gdi.selectItemByData("Status", pc->getStatus());
      const int numVisit = pc->getNumVisitors(true);
      const int numVisitExp = pc->getNumVisitors(false);
      
      wstring info;
      if (numVisit > 0) {
         info = L"Antal besökare X, genomsnittlig bomtid Y, största bomtid Z#" +
           itow(numVisit) + L" (" + itow(numVisitExp) + L")#" + getTimeMS(pc->getMissedTimeTotal() / numVisit) +
          L"#" + getTimeMS(pc->getMissedTimeMax());
      }
      else if (numVisitExp > 0) {
        info = L"Förväntat antal besökare: X#" + itow(numVisitExp);
      }
      gdi.setText("ControlID", itow(pc->getId()), true);

      wstring winfo = lang.tl(info);
      gdi.setText("Info", winfo, true);
      gdi.setText("Code", pc->codeNumbers());
      gdi.setText("Name", pc->getName());
      gdi.setText("TimeAdjust", pc->getTimeAdjustS());
      gdi.setText("MinTime", pc->getMinTimeS());
      if (gdi.hasWidget("Point"))
        gdi.setText("Point", pc->getRogainingPointsS());

      controlId = pc->getId();

      gdi.enableInput("Remove");
      gdi.enableInput("Save");
      gdi.enableInput("Visitors");
      gdi.enableInput("Courses");
      gdi.enableEditControls(true);

      oControl::ControlStatus st = pc->getStatus();
      if (st == oControl::StatusRogaining || st == oControl::StatusNoTiming || st == oControl::StatusBadNoTiming)
        gdi.disableInput("MinTime");

      if (st == oControl::StatusNoTiming || st == oControl::StatusBadNoTiming)
        gdi.disableInput("TimeAdjust");

      if (gdi.hasWidget("Point") && st != oControl::StatusRogaining)
        gdi.disableInput("Point");
    }
  }
  else {
    gdi.selectItemByData("Controls", -1);
    gdi.selectItemByData("Status", oControl::StatusOK);
    gdi.setText("Code", L"");
    gdi.setText("Name", L"");
    controlId = 0;

    gdi.setText("ControlID", makeDash(L"-"), true);
    gdi.setText("TimeAdjust", L"00:00");
    if (gdi.hasWidget("Point"))
      gdi.setText("Point", L"");

    gdi.disableInput("Remove");
    gdi.disableInput("Save");
    gdi.disableInput("Visitors");
    gdi.disableInput("Courses");

    gdi.enableEditControls(false);
  }
}

int ControlsCB(gdioutput *gdi, int type, void *data)
{
  TabControl &tc = dynamic_cast<TabControl &>(*gdi->getTabs().get(TControlTab));
  return tc.controlCB(*gdi, type, data);
}

void TabControl::save(gdioutput &gdi)
{
  if (controlId==0)
    return;

  DWORD pcid = controlId;

  pControl pc;
  pc = oe->getControl(pcid, false);

  if (!pc)
    throw std::exception("Internal error");
  if (pc->getStatus() != oControl::StatusFinish && pc->getStatus() != oControl::StatusStart) {
    if (!pc->setNumbers(gdi.getText("Code")))
      gdi.alert("Kodsiffran måste vara ett heltal. Flera kodsiffror måste separeras med komma.");

    pc->setStatus(oControl::ControlStatus(gdi.getSelectedItem("Status").first));
    pc->setTimeAdjust(gdi.getText("TimeAdjust"));
    if (pc->getStatus() != oControl::StatusRogaining) {
      if (pc->getStatus() != oControl::StatusNoTiming && pc->getStatus() != oControl::StatusBadNoTiming)
        pc->setMinTime(gdi.getText("MinTime"));
      pc->setRogainingPoints(0);
    }
    else {
      if (gdi.hasWidget("Point")) {
        pc->setMinTime(0);
        pc->setRogainingPoints(gdi.getTextNo("Point"));
      }
    }
  }

  pc->setName(gdi.getText("Name"));

  pc->synchronize();
  vector< pair<wstring, size_t> > d;
  oe->fillControls(d, oEvent::CTAll);
  gdi.addItem("Controls", d);

  oe->reEvaluateAll(set<int>(), true);

  selectControl(gdi, pc);
}

static void visitorTable(Table &table, void *ptr) {
  TabControl *view = (TabControl *)ptr;
  view->visitorTable(table);
}

static void courseTable(Table &table, void *ptr) {
  TabControl *view = (TabControl *)ptr;
  view->courseTable(table);
}

void TabControl::courseTable(Table &table) const {
  vector<pRunner> r;
  oe->getRunners(0, 0, r, false);
  map<int, int> runnersPerCourse;
  for (size_t k = 0; k < r.size(); k++) {
    pCourse c = r[k]->getCourse(false);
    int id = c != 0 ? c->getId() : 0;
    ++runnersPerCourse[id];
  }

  vector<pCourse> crs;
  oe->getCourses(crs);

  int ix = 1;
  for (size_t k = 0; k < crs.size(); k++) {
    vector<pControl> pc;
    crs[k]->getControls(pc);
    int used = 0;
    for (size_t j = 0; j < pc.size(); j++) {
      if (pc[j]->getId() == controlId) {
        used++;
      }
    }

    oCourse &it = *crs[k];
    if (used > 0) {
      table.addRow(ix++, &it);

      int row = 0;
      table.set(row++, it, TID_ID, itow(it.getId()), false);
      table.set(row++, it, TID_MODIFIED, it.getTimeStamp(), false);

      table.set(row++, it, TID_COURSE, crs[k]->getName(), false);
      table.set(row++, it, TID_INDEX, itow(used), false);
      table.set(row++, it, TID_RUNNER, itow(runnersPerCourse[crs[k]->getId()]), false);
    }
  }
}

void TabControl::visitorTable(Table &table) const {
  vector<pCard> c;
  oe->getCards(c);
  pControl pc = oe->getControl(controlId, false);

  if (!pc)
    return;
  vector<int> n;
  pc->getNumbers(n);
  struct PI {
    PI(int c, int type, int t) : card(c), code(type), time(t) {}
    int card;
    int code;
    int time;
    bool operator<(const PI&c) const {
      if (card != c.card)
        return card < c.card;
      else if (code != c.code)
        return code < c.code;
      else
        return time < c.time;
    }
    bool operator==(const PI&c) const {
      return c.card == card && c.code == code && c.time == time;
    }
  };
  set<PI> registeredPunches;
  vector<pPunch> p;
  int ix=1;
  for (size_t k = 0; k< c.size(); k++) {
    oCard &it = *c[k];

    it.getPunches(p);
    //oPunch *punch = it.getPunchByType(pc->getFirstNumber()); //XXX

    for (size_t j = 0; j < p.size(); j++) {

      vector<int>::iterator res = find(n.begin(), n.end(), p[j]->getTypeCode());
      if (res != n.end()) {
        oPunch *punch = p[j];
        registeredPunches.insert(PI(it.getCardNo(), p[j]->getTypeCode(), p[j]->getAdjustedTime()));
        table.addRow(ix++, &it);

        int row = 0;
        table.set(row++, it, TID_ID, itow(it.getId()), false);
        table.set(row++, it, TID_MODIFIED, it.getTimeStamp(), false);

        pRunner r = it.getOwner();
        if (r) {
          table.set(row++, it, TID_RUNNER, r->getName(), false);
          table.set(row++, it, TID_COURSE, r->getCourseName(), false);
        }
        else {
          table.set(row++, it, TID_RUNNER, L"-", false);
          table.set(row++, it, TID_COURSE, L"-", false);
        }
        table.set(row++, it, TID_FEE, punch->isUsedInCourse() ?
                  lang.tl("Ja") : lang.tl("Nej"), false);
        table.set(row++, it, TID_CARD, it.getCardNoString(), false);

        table.set(row++, it, TID_STATUS, punch->getTime(), false);
        table.set(row++, it, TID_CONTROL, punch->getType(), false);
        table.set(row++, it, TID_CODES, j>0 ? p[j-1]->getType() : L"-", true);
      }
    }
  }
}

int TabControl::controlCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Save")
      save(gdi);
    else if (bi.id=="Add") {
      bool rogaining = false;
      if (controlId>0) {
        save(gdi);
        pControl pc = oe->getControl(controlId, false);
        rogaining = pc && pc->getStatus() == oControl::StatusRogaining;
      }
      pControl pc = oe->addControl(0,oe->getNextControlNumber(), L"");

      if (rogaining)
        pc->setStatus(oControl::StatusRogaining);
      vector< pair<wstring, size_t> > d;
      oe->fillControls(d, oEvent::CTAll);
      gdi.addItem("Controls", d);
      selectControl(gdi, pc);
    }
    else if (bi.id=="Remove") {

      DWORD cid = controlId;
      if (cid==0) {
        gdi.alert("Ingen kontroll vald vald.");
        return 0;
      }

      if (oe->isControlUsed(cid))
        gdi.alert("Kontrollen används och kan inte tas bort.");
      else
        oe->removeControl(cid);

      vector< pair<wstring, size_t> > d;
      oe->fillControls(d, oEvent::CTAll);
      gdi.addItem("Controls", d);
      selectControl(gdi, 0);
    }
    else if (bi.id=="SwitchMode") {
      if (!tableMode)
        save(gdi);
      tableMode = !tableMode;
      loadPage(gdi);
    }
    else if (bi.id=="Visitors") {
      save(gdi);

      shared_ptr<Table> table=make_shared<Table>(oe, 20, L"Kontroll X#" + itow(controlId), "controlvisitor");

      table->addColumn("Id", 70, true, true);
      table->addColumn("Ändrad", 70, false);

      table->addColumn("Namn", 150, false);
      table->addColumn("Bana", 70, false);
      table->addColumn("På banan", 70, false);
      table->addColumn("Bricka", 70, true, true);

      table->addColumn("Tidpunkt", 70, false);
      table->addColumn("Stämpelkod", 70, true);
      table->addColumn("Föregående kontroll", 70, false);
      table->setGenerator(::visitorTable, this);
      table->setTableProp(0);
      table->update();
      gdi.clearPage(false);
      int xp=gdi.getCX();
      gdi.fillDown();
      gdi.addButton("Show", "Återgå", ControlsCB);
      gdi.addTable(table, xp, gdi.getCY());
      gdi.refresh();
    }
    else if (bi.id=="Courses") {
      auto table=make_shared<Table>(oe, 20, L"Kontroll X#" + itow(controlId), "controlcourse");

      table->addColumn("Id", 70, true, true);
      table->addColumn("Ändrad", 70, false);

      table->addColumn("Bana", 70, false, true);
      table->addColumn("Förekomst", 70, true, true);
      table->addColumn("Antal deltagare", 70, true, true);

      table->setGenerator(::courseTable, this);
      table->setTableProp(0);
      table->update();
      gdi.clearPage(false);
      int xp=gdi.getCX();
      gdi.fillDown();
      gdi.addButton("Show", "Återgå", ControlsCB);
      gdi.addTable(table, xp, gdi.getCY());
      gdi.refresh();
    }
    else if (bi.id=="Show") {
      loadPage(gdi);
    }
  }
  else if (type==GUI_LISTBOX){
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Controls") {
      if (gdi.isInputChanged(""))
        save(gdi);

      pControl pc=oe->getControl(bi.data, false);
      if (!pc)
        throw std::exception("Internal error");

      selectControl(gdi, pc);
    }
    else if (bi.id == "Status" ) {
      gdi.setInputStatus("MinTime",  bi.data != oControl::StatusRogaining && bi.data != oControl::StatusNoTiming && bi.data != oControl::StatusBadNoTiming, true);
      gdi.setInputStatus("Point",  bi.data == oControl::StatusRogaining, true);
      gdi.setInputStatus("TimeAdjust", bi.data != oControl::StatusNoTiming && bi.data != oControl::StatusBadNoTiming, true);
    }
  }
  else if (type==GUI_CLEAR) {
    if (controlId>0)
      save(gdi);

    return true;
  }
  return 0;
}


bool TabControl::loadPage(gdioutput &gdi)
{
  oe->checkDB();
  gdi.selectTab(tabId);
  gdi.clearPage(false);
  int xp=gdi.getCX();

  const int button_w=gdi.scaleLength(90);
  string switchMode;
  switchMode=tableMode ? "Formulärläge" : "Tabelläge";
  gdi.addButton(2, 2, button_w, "SwitchMode", switchMode,
    ControlsCB, "Välj vy", false, false).fixedCorner();

  if (tableMode) {
    gdi.addTable(oControl::getTable(oe), xp, gdi.scaleLength(30));
    return true;
  }

  gdi.fillDown();
  gdi.addString("", boldLarge, "Kontroller");

  gdi.pushY();
  gdi.addListBox("Controls", 250, 530, ControlsCB).isEdit(false).ignore(true);
  gdi.setTabStops("Controls", 40, 160);

  vector< pair<wstring, size_t> > d;
  oe->fillControls(d, oEvent::CTAll);
  gdi.addItem("Controls", d);

  gdi.newColumn();
  gdi.fillDown();
  gdi.popY();

  gdi.pushX();
  gdi.fillRight();
  gdi.addString("", 1, "Kontrollens ID-nummer:");
  gdi.fillDown();
  gdi.addString("ControlID", 1, "#-").setColor(colorGreen);
  gdi.popX();

  gdi.fillRight();
  gdi.addInput("Name", L"", 16, 0, L"Kontrollnamn:");

  gdi.addSelection("Status", 150, 100, ControlsCB, L"Status:", L"Ange om kontrollen fungerar och hur den ska användas.");
  oe->fillControlStatus(gdi, "Status");

  gdi.addInput("TimeAdjust", L"", 6, 0, L"Tidsjustering:");
  gdi.fillDown();
  gdi.addInput("MinTime", L"", 6, 0, L"Minsta sträcktid:");

  gdi.popX();
  gdi.dropLine(0.5);
  gdi.addString("", 10, "help:9373");

  gdi.fillRight();

  gdi.dropLine(0.5);
  gdi.addInput("Code", L"", 16, 0, L"Stämpelkod(er):");

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Rogaining)) {
    gdi.addInput("Point", L"", 6, 0, L"Rogaining-poäng:");
  }
  gdi.popX();
  gdi.dropLine(3.5);

  gdi.fillRight();
  gdi.addButton("Save", "Spara", ControlsCB, "help:save");
  gdi.disableInput("Save");
  gdi.addButton("Remove", "Radera", ControlsCB);
  gdi.disableInput("Remove");
  gdi.addButton("Courses", "Banor...", ControlsCB);
  gdi.addButton("Visitors", "Besökare...", ControlsCB);
  gdi.addButton("Add", "Ny kontroll", ControlsCB);

  gdi.dropLine(2.5);
  gdi.popX();

  gdi.fillDown();

  gdi.addString("Info", 0, "").setColor(colorGreen);

  gdi.dropLine(1.5);
  gdi.addString("", 10, "help:89064");

  selectControl(gdi, oe->getControl(controlId, false));

  gdi.setOnClearCb(ControlsCB);

  gdi.refresh();
  return true;
}

void TabControl::clearCompetitionData() {
  tableMode = false;
  controlId = 0;
}
