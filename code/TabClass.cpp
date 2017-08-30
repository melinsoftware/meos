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
#include <cassert>

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
#include "oListInfo.h"
#include "TabClass.h"
#include "ClassConfigInfo.h"
#include "meosException.h"
#include "gdifonts.h"
#include "oEventDraw.h"
#include "MeOSFeatures.h"

extern pEvent gEvent;
const char *visualDrawWindow = "visualdraw";

TabClass::TabClass(oEvent *poe):TabBase(poe)
{
  handleCloseWindow.tabClass = this;
  clearCompetitionData();
}

void TabClass::clearCompetitionData() {
  pSettings.clear();
  pSavedDepth = 3600;
  pFirstRestart = 3600;
  pTimeScaling = 1.0;
  pInterval = 120;

  currentStage = -1;
  EditChanged = false;
  ClassId=0;
  tableMode = false;
  showForkingGuide = false;
  storedNStage = "3";
  storedStart = "";
  storedPredefined = oEvent::PredefinedTypes(-1);
  cInfoCache.clear();
  hasWarnedDirect = false;

  lastSeedMethod = -1;
  lastSeedPreventClubNb = true;
  lastSeedReverse = false;
  lastSeedGroups = "1";
  lastPairSize = 1;
  lastFirstStart = "";
  lastInterval = "2:00";
  lastNumVac = "0";
  lastHandleBibs = false;
  lastScaleFactor = "1.0";
  lastMaxAfter = "60:00";

  gdioutput *gdi = getExtraWindow(visualDrawWindow, false);
  if (gdi) {
    gdi->closeWindow();
  }
}

TabClass::~TabClass(void)
{
}

bool ClassInfoSortStart(ClassInfo &ci1, ClassInfo &ci2)
{
  return ci1.firstStart>ci2.firstStart;
}

void TabClass::HandleCloseWindow::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GUI_EVENT) {
    EventInfo &ei = dynamic_cast<EventInfo &>(info);
    if (ei.id ==  "CloseWindow") {
      tabClass->closeWindow(gdi);   
    }
  }
}

void TabClass::closeWindow(gdioutput &gdi) {
}

int ClassesCB(gdioutput *gdi, int type, void *data)
{
  TabClass &tc = dynamic_cast<TabClass &>(*gdi->getTabs().get(TClassTab));
  return tc.classCB(*gdi, type, data);
}

int MultiCB(gdioutput *gdi, int type, void *data)
{
  TabClass &tc = dynamic_cast<TabClass &>(*gdi->getTabs().get(TClassTab));
  return tc.multiCB(*gdi, type, data);
}

int DrawClassesCB(gdioutput *gdi, int type, void *data)
{
  TabClass &tc = dynamic_cast<TabClass &>(*gdi->getTabs().get(TClassTab));
  
  if (type == GUI_LISTBOX) {
    tc.enableLoadSettings(*gdi);
  }
  else if (type == GUI_INPUT || type == GUI_INPUTCHANGE) {
    InputInfo &ii = *(InputInfo *)data;
    if (ii.id.length() > 1) {
      int id = atoi(ii.id.substr(1).c_str());
      if (id > 0 && ii.changed()) {
        string key = "C" + itos(id);
        TextInfo &ti = dynamic_cast<TextInfo&>(gdi->getBaseInfo(key.c_str()));
        ti.setColor(colorRed);
        gdi->enableInput("DrawAdjust");
        gdi->refreshFast();
      }
    }
  }
  return 0;
}


int TabClass::multiCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="ChangeLeg") {
      gdi.dropLine();
      legSetup(gdi);
      gdi.refresh();
    }
    else if (bi.id == "CommonStart") {
      gdi.setInputStatus("CommonStartTime", gdi.isChecked(bi.id));
    }
    else if (bi.id == "CoursePool") {
      string strId = "StageCourses_expl";
      gdi.setTextTranslate(strId, getCourseLabel(gdi.isChecked(bi.id)), true);
    }
    else if (bi.id == "DefineForking") {
      if (!checkClassSelected(gdi))
        return false;
      save(gdi, true);
      EditChanged=true;
      defineForking(gdi, true);
      return true;
    }
    else if (bi.id == "ApplyForking") {
      showForkingGuide = false;
      pClass pc = oe->getClass(ClassId);

      vector<pRunner> allR;
      oe->getRunners(ClassId, 0, allR, false);
      bool doClear = false;
      for (size_t k = 0; k < allR.size(); k++) {
        if (allR[k]->getCourseId() != 0) {
          if (!doClear) {
            if (gdi.ask("Vill du nollställa alla manuellt tilldelade banor?"))
              doClear = true;
            else 
              break;
          }
          if (doClear)
            allR[k]->setCourseId(0);
        }
      }
      pair<int,int> res = pc->autoForking(forkingSetup);
      gdi.alert("Created X distinct forkings using Y courses.#" +
                 itos(res.first) + "#" + itos(res.second));
      loadPage(gdi);
      EditChanged=true;
    }
    else if (bi.id == "AssignCourses") {
      set<int> selectedCourses, selectedLegs;
      gdi.getSelection("AllCourses", selectedCourses);
      gdi.getSelection("AllStages", selectedLegs);
      for (set<int>::iterator it = selectedLegs.begin(); it != selectedLegs.end(); ++it) {
        int leg = *it;
        forkingSetup[leg].clear();
        forkingSetup[leg].insert(forkingSetup[leg].begin(), selectedCourses.begin(), selectedCourses.end());
      }

      bool empty = true;
      for (size_t k = 0; k < forkingSetup.size(); k++) {
        if (forkingSetup[k].empty()) {
          gdi.setText("leg"+ itos(k), lang.tl("Leg X: Do not modify.#" + itos(k+1)));
        }
        else {
          empty = false;
          string crs;
          for (size_t j = 0; j < forkingSetup[k].size(); j++) {
            if (j>0)
              crs += ", ";
            crs += oe->getCourse(forkingSetup[k][j])->getName();
            if (j > 3) {
              crs += "...";
              break;
            }
          }
          gdi.setText("leg"+ itos(k), lang.tl("Leg X: Use Y.#" + itos(k+1) + "#" + crs));
        }
      }
      gdi.setInputStatus("ApplyForking", !empty);
      gdi.setSelection("AllCourses", set<int>());
      gdi.setSelection("AllStages", set<int>());
      gdi.refresh();
    }
    else if (bi.id == "ClearCourses") {
      gdi.setSelection("AllCourses", set<int>());
      gdi.setSelection("AllStages", set<int>());
      gdi.disableInput("AssignCourses");
    }
    else if (bi.id == "ShowForking") {
      if (!checkClassSelected(gdi))
        return false;
      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Klassen finns ej.");

      vector< vector<int> > forks;
      set< pair<int, int> > unfairLegs;
      vector< vector<int> > legOrder;

      pc->checkForking(legOrder, forks, unfairLegs);

      gdioutput *gdi_new = getExtraWindow("fork", true);
      string title = lang.tl("Forkings for X#" + pc->getName());
      if (!gdi_new)
        gdi_new = createExtraWindow("fork", title,
                                     gdi.scaleLength(1024) );
      else
        gdi_new->setWindowTitle(title);

      gdi_new->clearPage(false);

      gdi_new->addString("", fontMediumPlus, "Forkings");

      for (size_t k = 0; k < forks.size(); k++) {
        gdi_new->dropLine(0.7);
        string ver = itos(k+1) + ": ";
        for (size_t j = 0; j < legOrder[k].size(); j++) {
          pCourse crs = oe->getCourse(legOrder[k][j]);
          if (crs) {
            if (j>0)
              ver += ", ";
            ver += crs->getName();
          }
        }
        gdi_new->addStringUT(1, ver);
        gdi_new->pushX();
        gdi_new->fillRight();
        for (size_t j = 0; j < forks[k].size(); j++) {
          string ctrl;
          if (forks[k][j] > 0)
            ctrl += itos(forks[k][j]);
          else {
            if (j == 0)
              ctrl += lang.tl("Start");
            else if (j + 1 == forks[k].size())
              ctrl += lang.tl("Mål");
            else
              ctrl += lang.tl("Växel");
          }
          int next = -100;
          if (j + 1 < forks[k].size()) {
            ctrl += ",";
            next = forks[k][j+1];
          }
          int prev = j>0 ? forks[k][j-1] : -100;

          bool warn = unfairLegs.count(make_pair(prev, forks[k][j])) != 0;// ||
                      //unfairLegs.count(make_pair(forks[k][j], next)) != 0;

          TextInfo &ti = gdi_new->addStringUT(italicText, ctrl);
          if (warn) {
            ti.setColor(colorRed);
            ti.format = boldText;
          }
          gdi.setCX(gdi.getCX() - gdi.scaleLength(4));
        }
        gdi_new->popX();
        gdi_new->fillDown();
        gdi_new->dropLine();
      }

      if (!unfairLegs.empty()) {
        gdi_new->dropLine();
        gdi_new->addString("", fontMediumPlus, "Unfair control legs");
        gdi_new->dropLine(0.5);
        for (set< pair<int, int> >::const_iterator p = unfairLegs.begin();
             p != unfairLegs.end(); ++p) {

          string f = p->first > 0 ? itos(p->first) : "Växel";
          string s = p->second > 0 ? itos(p->second) : "Växel";
          gdi_new->addStringUT(0, MakeDash(f + " - " + s));
        }
      }

      gdi_new->dropLine();
      gdi_new->addButton("CloseWindow", "Stäng", ClassesCB);
      gdi_new->refresh();
    }
    else if (bi.id == "OneCourse") {
      if (!checkClassSelected(gdi))
        return false;
      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Klassen finns ej.");
      pc->setNumStages(0);
      pc->synchronize();
      gdi.restore();
      gdi.enableInput("MultiCourse", true);
      gdi.enableInput("Courses");
    }
    else if (bi.id=="SetNStage") {
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);

      if (!pc)
        throw std::exception("Klassen finns ej.");

      int total, finished, dns;
      oe->getNumClassRunners(pc->getId(), 0, total, finished, dns);

      oEvent::PredefinedTypes newType = oEvent::PredefinedTypes(gdi.getSelectedItem("Predefined").first);
      int nstages = gdi.getTextNo("NStage");

      if (finished > 0) {
        if (gdi.ask("warning:has_results") == false)
          return false;
      }
      else if (total>0) {
        bool ok = false;
        ClassType ct = pc->getClassType();
        if (ct == oClassIndividual) {
          switch (newType) {
            case oEvent::PPatrolOptional:
            case oEvent::PPool:
            case oEvent::PPoolDrawn:
            case oEvent::PNoMulti:
            case oEvent::PForking:
              ok = true;
            break;
            case oEvent::PNoSettings:
              ok = (nstages == 1);
          }
        }

        if (!ok) {
          if (gdi.ask("warning:has_entries") == false)
            return false;
        }
      }
      storedPredefined = newType;

      if (nstages > 0)
        storedNStage = gdi.getText("NStage");
      else {
        storedNStage = "";
        if (newType != oEvent::PNoMulti)
          nstages = 1; //Fixed by type
      }

      if (nstages>0 && nstages<41) {
        string st=gdi.getText("StartTime");
        if (oe->convertAbsoluteTime(st)>0)
          storedStart = st;

        save(gdi, false); //Clears and reloads

        gdi.selectItemByData("Courses", -2);
        gdi.disableInput("Courses");
        oe->setupRelay(*pc, newType, nstages, st);

        if (gdi.hasField("MAdd")) {
          gdi.enableInput("MAdd");
          gdi.enableInput("MCourses");
          gdi.enableInput("MRemove");
        }
        pc->forceShowMultiDialog(true);
        selectClass(gdi, pc->getId());
      }
      else if (nstages==0){
        pc->setNumStages(0);
        pc->synchronize();
        gdi.restore();
        gdi.enableInput("MultiCourse", true);
        gdi.enableInput("Courses");
        oe->adjustTeamMultiRunners(pc);
      }
      else {
        gdi.alert("Antalet sträckor måste vara ett heltal mellan 0 och 40.");
      }
    }
    else if (bi.id.substr(0, 7)=="@Course") {
      int cnr=atoi(bi.id.substr(7).c_str());
      selectCourses(gdi, cnr);
    }
    else if (bi.id=="MAdd"){
      DWORD cid=ClassId;
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(cid);

      if (!pc)
        return false;

      if (currentStage>=0){
        ListBoxInfo lbi;
        if (gdi.getSelectedItem("MCourses", lbi)) {
          int courseid=lbi.data;

          pc->addStageCourse(currentStage, courseid);
          pc->fillStageCourses(gdi, currentStage, "StageCourses");

          pc->synchronize();
          oe->checkOrderIdMultipleCourses(cid);
        }
      }
      EditChanged=true;
    }
    else if (bi.id=="MRemove"){
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;

      pClass pc=oe->getClass(cid);

      if (!pc)
        return false;

      if (currentStage>=0){
        ListBoxInfo lbi;
        if (gdi.getSelectedItem("StageCourses", lbi)) {
          int courseid=lbi.data;
          pc->removeStageCourse(currentStage, courseid, lbi.index);
          pc->synchronize();
          pc->fillStageCourses(gdi, currentStage, "StageCourses");
        }
      }
    }
    EditChanged=true;
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id.substr(0, 7)=="LegType") {
      LegTypes lt = LegTypes(bi.data);
      
      int i=atoi(bi.id.substr(7).c_str());
      pClass pc=oe->getClass(ClassId);
      if (!pc)
        return false;
      if (lt == pc->getLegType(i))
        return 0;

      pc->setLegType(i, lt);
      char legno[10];
      sprintf_s(legno, "%d", i);
      updateStartData(gdi, pc, i, true, false);

      gdi.setInputStatus(string("Restart")+legno, !pc->restartIgnored(i), true);
      gdi.setInputStatus(string("RestartRope")+legno, !pc->restartIgnored(i), true);

      EditChanged=true;
    }
    else if (bi.id == "AllStages") {
      set<int> t;
      gdi.getSelection(bi.id, t);
      gdi.setInputStatus("AssignCourses", !t.empty());
    }
    else if (bi.id.substr(0, 9)=="StartType") {
      StartTypes st=StartTypes(bi.data);
      int i=atoi(bi.id.substr(9).c_str());
      pClass pc=oe->getClass(ClassId);
      if (!pc)
        return false;
      pc->setStartType(i, st, true);
      char legno[10];
      sprintf_s(legno, "%d", i);
      updateStartData(gdi, pc, i, true, false);
      
      gdi.setInputStatus(string("Restart")+legno, !pc->restartIgnored(i), true);
      gdi.setInputStatus(string("RestartRope")+legno, !pc->restartIgnored(i), true);

      EditChanged=true;
    }
    else if (bi.id == "Predefined") {
      bool nleg;
      bool start;
      oe->setupRelayInfo(oEvent::PredefinedTypes(bi.data), nleg, start);
      gdi.setInputStatus("NStage", nleg);
      gdi.setInputStatus("StartTime", start);

      string nl = gdi.getText("NStage");
      if (!nleg && nl != "-") {
        storedNStage = nl;
        gdi.setText("NStage", "-");
      }
      else if (nleg && nl == "-") {
        gdi.setText("NStage", storedNStage);
      }

      string st = gdi.getText("StartTime");
      if (!start && st != "-") {
        storedStart = st;
        gdi.setText("StartTime", "-");
      }
      else if (start && st == "-") {
        gdi.setText("StartTime", storedStart);
      }
    }
    else if (bi.id=="Courses") {
      EditChanged=true;
    }
  }
  else if (type==GUI_INPUTCHANGE){
    InputInfo ii=*(InputInfo *)data;

    EditChanged=true;
    if (ii.id=="NStage")
      gdi.enableInput("SetNStage");
    //else if (ii.id=="")
  }
  return 0;
}

int TabClass::classCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Cancel") {
      showForkingGuide = false;
      loadPage(gdi);
      return 0;
    }
    else if (bi.id == "UseAdvanced") {
      bool checked = gdi.isChecked("UseAdvanced");
      oe->setProperty("AdvancedClassSettings", checked);
      save(gdi, true);
      PostMessage(gdi.getTarget(), WM_USER + 2, TClassTab, 0);
    }
    else if (bi.id=="SwitchMode") {
      if (!tableMode)
        save(gdi, true);
      tableMode=!tableMode;
      loadPage(gdi);
    }
    else if (bi.id=="Restart") {
      save(gdi, true);
      gdi.clearPage(true);
      gdi.addString("", 2, "Omstart i stafettklasser");
      gdi.addString("", 10, "help:31661");
      gdi.addListBox("RestartClasses", 200, 250, 0, "Stafettklasser", "", true);
      oe->fillClasses(gdi, "RestartClasses", oEvent::extraNone, oEvent::filterOnlyMulti);
      gdi.pushX();
      gdi.fillRight();
      oe->updateComputerTime();
      int t=oe->getComputerTime()-(oe->getComputerTime()%60)+60;
      gdi.addInput("Rope", oe->getAbsTime(t), 6, 0, "Repdragningstid");
      gdi.addInput("Restart", oe->getAbsTime(t+600), 6, 0, "Omstartstid");
      gdi.dropLine(0.9);
      gdi.addButton("DoRestart","OK", ClassesCB);
      gdi.addButton("Cancel","Stäng", ClassesCB);
      gdi.fillDown();
      gdi.dropLine(3);
      gdi.popX();
    }
    else if (bi.id=="DoRestart") {
      set<int> cls;
      gdi.getSelection("RestartClasses", cls);
      gdi.fillDown();
      set<int>::iterator it;
      string ropeS=gdi.getText("Rope");
      int rope = oe->getRelativeTime(ropeS);
      string restartS=gdi.getText("Restart");
      int restart = oe->getRelativeTime(restartS);

      if (rope<=0) {
        gdi.alert("Ogiltig repdragningstid.");
        return 0;
      }
      if (restart<=0) {
        gdi.alert("Ogiltig omstartstid.");
        return 0;
      }
      if (restart<rope) {
        gdi.alert("Repdragningstiden måste ligga före omstartstiden.");
        return 0;
      }

      gdi.addString("", 0, "Sätter reptid (X) och omstartstid (Y) för:#" +
                    oe->getAbsTime(rope) + "#" + oe->getAbsTime(restart));

      for (it=cls.begin(); it!=cls.end(); ++it) {
        pClass pc=oe->getClass(*it);

        if (pc) {
          gdi.addStringUT(0, pc->getName());

          int ns=pc->getNumStages();

          for (int k=0;k<ns;k++) {
            pc->setRopeTime(k, ropeS);
            pc->setRestartTime(k, restartS);
          }
        }
      }
      gdi.scrollToBottom();
      gdi.refresh();
    }
    else if (bi.id=="SaveDrawSettings") {
      readClassSettings(gdi);
      for(size_t k=0; k<cInfo.size(); k++) {
        const ClassInfo &ci = cInfo[k];
        if (ci.pc) {
          // Save settings with class
          ci.pc->synchronize(false);
          ci.pc->setDrawFirstStart(drawInfo.firstStart + drawInfo.baseInterval * ci.firstStart);
          ci.pc->setDrawInterval(ci.interval * drawInfo.baseInterval);
          ci.pc->setDrawVacant(ci.nVacant);
          ci.pc->setDrawNumReserved(ci.nExtra);
          ci.pc->synchronize(true);
        }
      }
    }
    else if (bi.id=="DoDrawAll") {
      readClassSettings(gdi);
      int method = gdi.getSelectedItem("Method").first;
      bool soft = method == DMSOFT;
      int pairSize = gdi.getSelectedItem("PairSize").first;
      
      bool drawCoursebased = drawInfo.coursesTogether;

      map<int, vector<ClassDrawSpecification> > specs;
      for(size_t k=0; k<cInfo.size(); k++) {
        const ClassInfo &ci=cInfo[k];
        if (ci.pc) {
          // Save settings with class
          ci.pc->synchronize(false);
          ci.pc->setDrawFirstStart(drawInfo.firstStart + drawInfo.baseInterval * ci.firstStart);
          ci.pc->setDrawInterval(ci.interval * drawInfo.baseInterval);
          ci.pc->setDrawVacant(ci.nVacant);
          ci.pc->setDrawNumReserved(ci.nExtra);
          ci.pc->synchronize(true);
        }
        ClassDrawSpecification cds(ci.classId, 0, drawInfo.firstStart + drawInfo.baseInterval * ci.firstStart,
                                   drawInfo.baseInterval * ci.interval, ci.nVacant);
        if (drawCoursebased) {
          pCourse pCrs = oe->getClass(ci.classId)->getCourse();
          int id = pCrs ? pCrs->getId() : 101010101 + ci.classId;
          specs[id].push_back(cds);
        }
        else
          specs[ci.classId].push_back(cds);
      }

      for (map<int, vector<ClassDrawSpecification> >::iterator it = specs.begin();
           it != specs.end(); ++it) {
        oe->drawList(it->second, soft, pairSize, oEvent::drawAll);
      }

      oe->addAutoBib();

      gdi.clearPage(false);
      gdi.addButton("Cancel", "Återgå", ClassesCB);

      oListParam par;
      oListInfo info;
      par.listCode = EStdStartList;
      for (size_t k=0; k<cInfo.size(); k++)
        par.selection.insert(cInfo[k].classId);

      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);
      gdi.refresh();
    }
    else if (bi.id == "RemoveVacant") {
      if (gdi.ask("Vill du radera alla vakanser från tävlingen?")) {
        oe->removeVacanies(0);
        gdi.disableInput(bi.id.c_str());
      }
    }
    else if (bi.id == "SelectAll") {
      set<int> lst;
      oe->getAllClasses(lst);
      gdi.setSelection("Classes", lst);
      enableLoadSettings(gdi);
    }
    else if (bi.id == "SelectUndrawn") {
      set<int> lst;
      oe->getNotDrawnClasses(lst, false);
      gdi.setSelection("Classes", lst);
      enableLoadSettings(gdi);
    }
    else if (bi.id == "SelectStart") {
      int id = bi.getExtraInt();
      vector<int> blocks;
      vector<string> starts;
      oe->getStartBlocks(blocks, starts);
      if (size_t(id) < starts.size()) {
        string start = starts[id];
        set<int> lst;
        vector<pClass> cls;
        oe->getClasses(cls, true);
        for (size_t k = 0; k < cls.size(); k++) {
          if (cls[k]->getStart() == start)
            lst.insert(cls[k]->getId());
        }
        gdi.setSelection("Classes", lst);
      }
      enableLoadSettings(gdi);
    }
    else if (bi.id == "QuickSettings") {
      save(gdi, false);
      prepareForDrawing(gdi);
    }
    else if (bi.id == "DrawMode") {
      save(gdi, false);
      ClassId = 0;

      EditChanged=false;
      gdi.clearPage(true);

      gdi.addString("", boldLarge, "Lotta flera klasser");
      gdi.dropLine();

      gdi.pushX();
      gdi.fillRight();
      gdi.addInput("FirstStart", oe->getAbsTime(3600), 10, 0, "Första (ordinarie) start:");
      gdi.addInput("MinInterval", "2:00", 10, 0, "Minsta startintervall:");
      gdi.fillDown();
      gdi.addInput("Vacances", "5%", 10, 0, "Andel vakanser:");
      gdi.popX();

      gdi.addSelection("Method", 200, 200, 0, "Metod:");
      gdi.addItem("Method", lang.tl("Lottning"), DMRandom);
      gdi.addItem("Method", lang.tl("SOFT-lottning"), DMSOFT);
      gdi.selectItemByData("Method", getDefaultMethod(false));

      gdi.fillDown();
      gdi.addCheckbox("LateBefore", "Efteranmälda före ordinarie");
      gdi.dropLine();

      gdi.popX();
      gdi.fillRight();
      gdi.addButton("AutomaticDraw", "Automatisk lottning", ClassesCB).setDefault();
      gdi.addButton("DrawAll", "Manuell lottning", ClassesCB).setExtra(1);
      gdi.addButton("Simultaneous", "Gemensam start", ClassesCB);

      const bool multiDay = oe->hasPrevStage();

      if (multiDay)
        gdi.addButton("Pursuit", "Hantera jaktstart", ClassesCB);

      gdi.addButton("Cancel", "Återgå", ClassesCB).setCancel();

      gdi.dropLine(3);

      gdi.newColumn();

      gdi.addString("", 10, "help_autodraw");
    }
    else if (bi.id == "Pursuit") {
      pursuitDialog(gdi);
    }
    else if (bi.id == "SelectAllNoneP") {
      bool select = bi.getExtraInt() != 0;
      for (int k = 0; k < oe->getNumClasses(); k++) {
        gdi.check("PLUse" + itos(k), select);
        gdi.setInputStatus("First" + itos(k), select);
      }
    }
    else if (bi.id == "DoPursuit" || bi.id=="CancelPursuit" || bi.id == "SavePursuit") {
      bool cancel = bi.id=="CancelPursuit";
      
      int maxAfter = convertAbsoluteTimeMS(gdi.getText("MaxAfter"));
      int deltaRestart = convertAbsoluteTimeMS(gdi.getText("TimeRestart"));
      int interval = convertAbsoluteTimeMS(gdi.getText("Interval"));

      double scale = atof(gdi.getText("ScaleFactor").c_str());
      bool reverse = bi.getExtraInt() == 2;
      int pairSize = gdi.getSelectedItem("PairSize").first;
      
      pSavedDepth = maxAfter;
      pFirstRestart = deltaRestart;
      pTimeScaling = scale;
      pInterval = interval;

      oListParam par;

      for (int k = 0; k < oe->getNumClasses(); k++) {
        if (!gdi.hasField("PLUse" + itos(k)))
          continue;
        BaseInfo *bi = gdi.setText("PLUse" + itos(k), "", false);
        if (bi) {
          int id = bi->getExtraInt();
          bool checked = gdi.isChecked("PLUse" + itos(k));
          int first = oe->getRelativeTime(gdi.getText("First" + itos(k)));
          //int max = oe->getRelativeTime(gdi.getText("Max" + itos(k)));

          map<int, PursuitSettings>::iterator st = pSettings.find(id);
          if (st != pSettings.end()) {
            st->second.firstTime = first;
            st->second.maxTime = maxAfter;
            st->second.use = checked;
          }

          if (checked) {
            pClass pc = oe->getClass(id);
            if (pc)
              pc->setDrawFirstStart(first);
          }

          if (!cancel && checked) {
            oe->drawPersuitList(id, first, first + deltaRestart, maxAfter,
                                interval, pairSize, reverse, scale);
            par.selection.insert(id);
          }
        }
      }

      if (bi.id == "SavePursuit") {
         return 0;
      }

      if (cancel) {
        loadPage(gdi);
        return 0;
      }

      gdi.restore("Pursuit", false);

      gdi.dropLine();
      gdi.fillDown();

      oListInfo info;
      par.listCode = EStdStartList;
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);
      gdi.dropLine();
      gdi.addButton("Cancel", "Återgå", ClassesCB);
      gdi.refresh();
    }
    else if (bi.id.substr(0,5) == "PLUse") {
      int k = atoi(bi.id.substr(5).c_str());
      gdi.setInputStatus("First" + itos(k), gdi.isChecked(bi.id));
    }
    else if (bi.id == "AutomaticDraw") {
      string firstStart = gdi.getText("FirstStart");
      string minInterval = gdi.getText("MinInterval");
      string vacances = gdi.getText("Vacances");
      bool lateBefore = false;
      //bool pairwise = gdi.isChecked("Pairwise");
      int pairSize = 1;
      if (gdi.hasField("PairSize")) {
        pairSize = gdi.getSelectedItem("PairSize").first;
      }

      int method = gdi.getSelectedItem("Method").first;
      bool useSoft = method == DMSOFT;
      gdi.clearPage(true);
      oe->automaticDrawAll(gdi, firstStart, minInterval, vacances,
                            lateBefore, useSoft, pairSize);
      oe->addAutoBib();
      gdi.scrollToBottom();
      gdi.addButton("Cancel", "Återgå", ClassesCB);
    }
    else if (bi.id == "SelectMisses") {
      set<int> lst;
      oe->getNotDrawnClasses(lst, true);
      gdi.setSelection("Classes", lst);
      enableLoadSettings(gdi);
    }
    else if (bi.id == "SelectNone") {
      gdi.setSelection("Classes", set<int>());
      enableLoadSettings(gdi);
    }
    else if (bi.id == "Simultaneous") {
      string firstStart;// = oe->getAbsTime(3600);
      firstStart = gdi.getText("FirstStart");

      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Gemensam start");
      gdi.dropLine();
      int by = 0;
      int bx = gdi.getCX();

      showClassSelection(gdi, bx, by, 0);

      gdi.pushX();
      gdi.addInput("FirstStart", firstStart, 10, 0, "Starttid:");

      gdi.dropLine(4);
      gdi.popX();
      gdi.fillRight();
      gdi.addButton("AssignStart", "Tilldela", ClassesCB).isEdit(true);
      gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
      gdi.addButton("EraseStartAll", "Radera starttider...", ClassesCB).isEdit(true).setExtra(1);

      gdi.refresh();
    }
    else if (bi.id == "AssignStart") {
      set<int> classes;
      gdi.getSelection("Classes", classes);
      if (classes.empty()) {
        gdi.alert("Ingen klass vald.");
        return 0;
      }

      string time = gdi.getText("FirstStart");
      for (set<int>::iterator it = classes.begin(); it!=classes.end();++it) {
        simultaneous(*it, time);
      }

      bi.id = "Simultaneous";
      classCB(gdi, type, &bi);
    }
    else if (bi.id == "DrawAll") {
      int origin = bi.getExtraInt();
      string firstStart = oe->getAbsTime(3600);
      string minInterval = "2:00";
      string vacances = "5%";
      int maxNumControl = 1;
      int pairSize = 1;

      //bool pairwise = false;
      int by = 0;
      int bx = gdi.getCX();
      if (origin!=13) {
        if (origin!=1) {
          save(gdi, true);
          ClassId = 0;

          EditChanged=false;
        }
        else {
          firstStart = gdi.getText("FirstStart");
          minInterval = gdi.getText("MinInterval");
          vacances = gdi.getText("Vacances");
          //pairwise = gdi.isChecked("Pairwise");
          if (gdi.hasField("PairSize")) {
            pairSize = gdi.getSelectedItem("PairSize").first;
          }
        }

        gdi.clearPage(false);

        gdi.addString("", boldLarge, "Lotta flera klasser");
        gdi.dropLine(0.5);

        showClassSelection(gdi, bx, by, DrawClassesCB);
        vector<pClass> cls;
        oe->getClasses(cls, false);
        set<int> clsId;
        for (size_t k = 0; k < cls.size(); k++) {
          if (cls[k]->hasFreeStart())
            continue;
          if (cls[k]->getStartType(0) != STDrawn)
            continue;
          clsId.insert(cls[k]->getId());
        }

        gdi.setSelection("Classes", clsId);

        gdi.addString("", 1, "Grundinställningar");

        gdi.pushX();
        gdi.fillRight();

        gdi.addInput("FirstStart", firstStart, 10, 0, "Första start:");
        gdi.addInput("nFields", "10", 10, 0, "Max parallellt startande:");
        gdi.popX();
        gdi.dropLine(3);

        gdi.addSelection("MaxCommonControl", 150, 100, 0,
          "Max antal gemensamma kontroller:");

        vector< pair<string, size_t> > items;
        items.push_back(make_pair(lang.tl("Inga"), 1));
        items.push_back(make_pair(lang.tl("Första kontrollen"), 2));
        for (int k = 2; k<10; k++)
          items.push_back(make_pair(lang.tl("X kontroller#" + itos(k)), k+1));
        items.push_back(make_pair(lang.tl("Hela banan"), 1000));
        gdi.addItem("MaxCommonControl", items);
        gdi.selectItemByData("MaxCommonControl", maxNumControl);

        gdi.popX();
        gdi.dropLine(4);
        gdi.addCheckbox("AllowNeighbours", "Tillåt samma bana inom basintervall", 0, true);
        gdi.addCheckbox("CoursesTogether", "Lotta klasser med samma bana gemensamt", 0, false);
        
        gdi.popX();
        gdi.dropLine(2);
        gdi.addString("", 1, "Startintervall");
        gdi.dropLine(1.4);
        gdi.popX();
        gdi.fillRight();
        gdi.addInput("BaseInterval", "1:00", 10, 0, "Basintervall (min):");
        gdi.addInput("MinInterval", minInterval, 10, 0, "Minsta intervall i klass:");
        gdi.addInput("MaxInterval", minInterval, 10, 0, "Största intervall i klass:");

        gdi.popX();
        gdi.dropLine(4);
        gdi.addString("", 1, "Vakanser och efteranmälda");
        gdi.dropLine(1.4);
        gdi.popX();
        gdi.addInput("Vacances", vacances, 10, 0, "Andel vakanser:");
        gdi.addInput("VacancesMin", "1", 10, 0, "Min. vakanser (per klass):");
        gdi.addInput("VacancesMax", "10", 10, 0, "Max. vakanser (per klass):");

        gdi.popX();
        gdi.dropLine(3);

        gdi.addInput("Extra", "0%", 10, 0, "Förväntad andel efteranmälda:");

        gdi.dropLine(4);
        gdi.fillDown();
        gdi.popX();
        gdi.setRestorePoint("Setup");

      }
      else {
        gdi.restore("Setup");
        by = gdi.getHeight();
        gdi.enableEditControls(true);
      }

      gdi.fillRight();
      gdi.pushX();
      RECT rcPrepare;
      rcPrepare.left = gdi.getCX();
      rcPrepare.top = gdi.getCY();
      gdi.setCX(gdi.getCX() + gdi.getLineHeight());
      gdi.dropLine();
      gdi.addString("", fontMediumPlus, "Förbered lottning");
      gdi.dropLine(2.5);
      gdi.popX();
      gdi.setCX(gdi.getCX() + gdi.getLineHeight());
      gdi.addButton("PrepareDrawAll", "Fördela starttider...", ClassesCB).isEdit(true).setDefault();
      gdi.addButton("EraseStartAll", "Radera starttider...", ClassesCB).isEdit(true).setExtra(0);
      gdi.addButton("LoadSettings", "Hämta inställningar från föregående lottning", ClassesCB).isEdit(true);
      enableLoadSettings(gdi);

      gdi.dropLine(3);
      rcPrepare.bottom = gdi.getCY();
      rcPrepare.right = gdi.getWidth();
      gdi.addRectangle(rcPrepare, colorLightGreen);
      gdi.dropLine();
      gdi.popX();
      
      gdi.addString("", 1, "Efteranmälningar");
      gdi.dropLine(1.5);
      gdi.popX();
      gdi.addButton("DrawAllBefore", "Efteranmälda (före ordinarie)", ClassesCB).isEdit(true);
      gdi.addButton("DrawAllAfter", "Efteranmälda (efter ordinarie)", ClassesCB).isEdit(true);

      gdi.dropLine(4);
      gdi.popX();

      gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
      gdi.addButton("HelpDraw", "Hjälp...", ClassesCB, "");
      gdi.dropLine(3);

      by = max(by, gdi.getCY());

      gdi.setCX(bx);
      gdi.setCY(by);
      gdi.fillDown();
      gdi.dropLine();

      gdi.setRestorePoint("ReadyToDistribute");
      gdi.refresh();
    }
    else if (bi.id == "HelpDraw") {

      gdioutput *gdi_new = getExtraWindow("help", true);

      if (!gdi_new)
        gdi_new = createExtraWindow("help", MakeDash("MeOS - " + lang.tl("Hjälp")),  gdi.scaleLength(640));
      gdi_new->clearPage(true);
      gdi_new->addString("", boldLarge, "Lotta flera klasser");
      gdi_new->addString("", 10, "help_draw");
      gdi_new->dropLine();
      gdi_new->addButton("CloseWindow", "Stäng", ClassesCB);
    }
    else if (bi.id == "CloseWindow") {
      gdi.closeWindow();
    }
    else if (bi.id=="PrepareDrawAll") {
      set<int> classes;
      gdi.getSelection("Classes", classes);
      if (classes.empty()) {
        throw meosException("Ingen klass vald.");
      }
      gdi.restore("ReadyToDistribute");
      drawInfo.classes.clear();

      for (set<int>::iterator it = classes.begin(); it!=classes.end();++it) {
        map<int, ClassInfo>::iterator res = cInfoCache.find(*it);
        if ( res != cInfoCache.end() ) {
          res->second.hasFixedTime = false;
          drawInfo.classes[*it] = res->second;
        }
        else
          drawInfo.classes[*it] = ClassInfo(oe->getClass(*it));
      }

      readDrawInfo(gdi, drawInfo);
      if (drawInfo.baseInterval <= 0 || drawInfo.baseInterval == NOTIME)
        throw meosException("Ogiltigt basintervall.");
      if (drawInfo.minClassInterval <= 0 || drawInfo.minClassInterval == NOTIME)
        throw meosException("Ogiltigt minimalt intervall.");
      if (drawInfo.minClassInterval > drawInfo.maxClassInterval || drawInfo.maxClassInterval == NOTIME)
        throw meosException("Ogiltigt maximalt intervall. ");

      if (drawInfo.minClassInterval < drawInfo.baseInterval) {
        throw meosException("Startintervallet får inte vara kortare än basintervallet.");
      }

      if (drawInfo.minClassInterval % drawInfo.baseInterval != 0 || 
          drawInfo.maxClassInterval % drawInfo.baseInterval != 0) {
        throw meosException("Ett startintervall måste vara en multipel av basintervallet.");
      }

      gdi.enableEditControls(false);

      if (drawInfo.firstStart<=0) {
        drawInfo.baseInterval = 0;
        gdi.addString("", 0, "Raderar starttider...");
      }

      oe->optimizeStartOrder(gdi, drawInfo, cInfo);

      showClassSettings(gdi);
    }
    else if (bi.id == "LoadSettings") {
      set<int> classes;
      gdi.getSelection("Classes", classes);
      if (classes.empty()) {
        throw meosException("Ingen klass vald.");
      }
      gdi.restore("ReadyToDistribute");
      oe->loadDrawSettings(classes, drawInfo, cInfo);

      /*
      drawInfo.firstStart = 3600 * 22;
      drawInfo.minClassInterval = 3600;
      drawInfo.maxClassInterval = 1;
      drawInfo.minVacancy = 10;

      set<int> reducedStart;
      for (set<int>::iterator it = classes.begin(); it != classes.end(); ++it) {
        pClass pc = oe->getClass(*it);
        if (pc) {
          int fs = pc->getDrawFirstStart();
          int iv = pc->getDrawInterval();
          if (iv > 0 && fs > 0) {
            drawInfo.firstStart = min(drawInfo.firstStart, fs);
            drawInfo.minClassInterval = min(drawInfo.minClassInterval, iv);
            drawInfo.maxClassInterval = max(drawInfo.maxClassInterval, iv);
            drawInfo.minVacancy = min(drawInfo.minVacancy, pc->getDrawVacant());
            drawInfo.maxVacancy = max(drawInfo.maxVacancy, pc->getDrawVacant());
            reducedStart.insert(fs%iv);
          }
        }
      }

      drawInfo.baseInterval = drawInfo.minClassInterval;
      int lastStart = -1;
      for (set<int>::iterator it = reducedStart.begin(); it != reducedStart.end(); ++it) {
        if (lastStart == -1)
          lastStart = *it;
        else {
          drawInfo.baseInterval = min(drawInfo.baseInterval, *it-lastStart);
          lastStart = *it;
        }
      }

      cInfo.clear();
      cInfo.resize(classes.size());
      int i = 0;
      for (set<int>::iterator it = classes.begin(); it != classes.end(); ++it) {
        pClass pc = oe->getClass(*it);
        if (pc) {
          int fs = pc->getDrawFirstStart();
          int iv = pc->getDrawInterval();
          cInfo[i].pc = pc;
          cInfo[i].classId = *it;
          cInfo[i].courseId = pc->getCourseId();
          cInfo[i].firstStart = fs;
          cInfo[i].unique = pc->getCourseId();
          if (cInfo[i].unique == 0)
            cInfo[i].unique = pc->getId() * 10000;
          cInfo[i].firstStart = (fs - drawInfo.firstStart) / drawInfo.baseInterval;
          cInfo[i].interval = iv / drawInfo.baseInterval;
          cInfo[i].nVacant = pc->getDrawVacant();
          i++;
        }
      }
      */
      writeDrawInfo(gdi, drawInfo);
      gdi.enableEditControls(false);

      showClassSettings(gdi);
    }
    else if (bi.id == "VisualizeDraw") {
      readClassSettings(gdi);

      gdioutput *gdi_new = getExtraWindow(visualDrawWindow, true);
      if (!gdi_new)
        gdi_new = createExtraWindow(visualDrawWindow, MakeDash("MeOS - " + lang.tl("Visualisera startfältet")),  gdi.scaleLength(1000));

      gdi_new->clearPage(false);
      gdi_new->addString("", boldLarge, "Visualisera startfältet");
      gdi_new->dropLine();
      gdi_new->addString("", 0, "För muspekaren över en markering för att få mer information.");
      gdi_new->dropLine();
      visualizeField(*gdi_new);
      gdi_new->dropLine();
      gdi_new->addButton("CloseWindow", "Stäng", ClassesCB);
      gdi_new->registerEvent("CloseWindow", 0).setHandler(&handleCloseWindow);
      gdi_new->refresh();
    }
    else if (bi.id == "EraseStartAll") {
      set<int> classes;
      gdi.getSelection("Classes", classes);
      if (classes.empty()) {
        gdi.alert("Ingen klass vald.");
        return 0;
      }
      if (classes.size() == 1) {
        pClass pc = oe->getClass(*classes.begin());
        if (!pc || !gdi.ask("Vill du verkligen radera alla starttider i X?#" + pc->getName()))
          return 0;
      }
      else {
        if (!gdi.ask("Vill du verkligen radera starttider i X klasser?#" + itos(classes.size())) )
          return 0;
      }

      for (set<int>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
        vector<ClassDrawSpecification> spec;
        spec.push_back(ClassDrawSpecification(*it, 0, 0, 0, 0));
        oe->drawList(spec, false, 1, oEvent::drawAll);
      }

      if (bi.getExtraInt() == 1)
        bi.id = "Simultaneous";
      else
        bi.id = "DrawAll";

      bi.setExtra(1);
      classCB(gdi, type, &bi); // Reload draw dialog
    }
    else if (bi.id == "DrawAdjust") {
      readClassSettings(gdi);
      gdi.restore("ReadyToDistribute");
      oe->optimizeStartOrder(gdi, drawInfo, cInfo);
      showClassSettings(gdi);
    }
    else if (bi.id == "DrawAllAdjust") {
      readClassSettings(gdi);
      bi.id = "DrawAll";
      return classCB(gdi, type, &bi);
    }
    else if (bi.id == "DrawAllBefore" || bi.id == "DrawAllAfter") {
      oe->drawRemaining(true, bi.id == "DrawAllAfter");
      oe->addAutoBib();
      loadPage(gdi);
    }
    else if (bi.id=="DoDraw" || bi.id=="DoDrawAfter"  || bi.id=="DoDrawBefore"){
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;
      DrawMethod method = DrawMethod(gdi.getSelectedItem("Method").first);

      int interval = 0;
      if (gdi.hasField("Interval"))
        interval = convertAbsoluteTimeMS(gdi.getText("Interval"));

      int vacanses = 0;
      if (gdi.hasField("Vacanses"))
        vacanses = gdi.getTextNo("Vacanses");

      int leg = 0;
      if (gdi.hasField("Leg")) {
        leg = gdi.getSelectedItem("Leg").first;
      }

      string bib;
      bool doBibs = false;

      if (gdi.hasField("Bib")) {
        bib = gdi.getText("Bib");
        doBibs = gdi.isChecked("HandleBibs");
      }

      string time = gdi.getText("FirstStart");
      int t=oe->getRelativeTime(time);

      if (t<=0)
        throw std::exception("Ogiltig första starttid. Måste vara efter nolltid.");

      oEvent::DrawType type(oEvent::drawAll);
      if (bi.id=="DoDrawAfter")
        type = oEvent::remainingAfter;
      else if (bi.id=="DoDrawBefore")
        type = oEvent::remainingBefore;

      //bool pairwise = false;

//      if (gdi.hasField("Pairwise"))
 //       pairwise = gdi.isChecked("Pairwise");
      int pairSize = 1;
      if (gdi.hasField("PairSize")) {
        pairSize = gdi.getSelectedItem("PairSize").first;
      }

      int maxTime = 0, restartTime = 0;
      double scaleFactor = 1.0;

      if (gdi.hasField("TimeRestart"))
        restartTime = oe->getRelativeTime(gdi.getText("TimeRestart"));

      if (gdi.hasField("MaxAfter"))
        maxTime = convertAbsoluteTimeMS(gdi.getText("MaxAfter"));

      if (gdi.hasField("ScaleFactor"))
        scaleFactor = atof(gdi.getText("ScaleFactor").c_str());

      if (method == DMRandom || method == DMSOFT) {
        vector<ClassDrawSpecification> spec;
        spec.push_back(ClassDrawSpecification(cid, leg, t, interval, vacanses));
    
        oe->drawList(spec, method == DMSOFT, pairSize, type);
      }
      else if (method == DMClumped)
        oe->drawListClumped(cid, t, interval, vacanses);
      else if (method == DMPursuit || method == DMReversePursuit) {
        oe->drawPersuitList(cid, t, restartTime, maxTime,
                            interval, pairSize, 
                            method == DMReversePursuit, 
                            scaleFactor);
      }
      else if (method == DMSimultaneous) {
        simultaneous(cid, time);
      }
      else if (method == DMSeeded) {
        ListBoxInfo seedMethod;
        gdi.getSelectedItem("SeedMethod", seedMethod);
        string seedGroups = gdi.getText("SeedGroups");
        vector<string> out;
        split(seedGroups, " ,;", out);
        vector<int> sg;
        bool invalid = false;
        for (size_t k = 0; k < out.size(); k++) {
          if (trim(out[k]).empty())
            continue;
          int val = atoi(trim(out[k]).c_str()); 
          if (val <= 0)
            invalid = true;

          sg.push_back(val); 
        }        
        if (invalid || sg.empty())
          throw meosException("Ogiltig storlek på seedningsgrupper X.#" + seedGroups);

        bool noClubNb = gdi.isChecked("PreventClubNb");
        bool reverse = gdi.isChecked("ReverseSeedning"); 

        pClass pc=oe->getClass(ClassId);
        if (!pc)
          throw meosException("Class not found");

        pc->drawSeeded(ClassSeedMethod(seedMethod.data), leg, t, interval,
                                       sg, noClubNb, reverse, pairSize); 
      }
      else
        throw std::exception("Not implemented");

      if (doBibs)
        oe->addBib(cid, leg, bib);

      // Clear input
      gdi.restore("", false);
      gdi.addButton("Cancel", "Återgå", ClassesCB).setCancel();
      
      gdi.dropLine();

      oListParam par;
      par.selection.insert(cid);
      oListInfo info;
      par.listCode = EStdStartList;
      par.setLegNumberCoded(leg);
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);

      gdi.refresh();
      return 0;
    }
    else if (bi.id=="HandleBibs") {
      gdi.setInputStatus("Bib", gdi.isChecked("HandleBibs"));
    }
    else if (bi.id == "DoDeleteStart") {
      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw meosException("Class not found");

      if (!gdi.ask("Vill du verkligen radera alla starttider i X?#" + pc->getName()))
        return 0;

      int leg = 0;
      if (gdi.hasField("Leg")) {
        leg = gdi.getSelectedItem("Leg").first;
      }
      vector<ClassDrawSpecification> spec;
      spec.push_back(ClassDrawSpecification(ClassId, leg, 0, 0, 0));
    
      oe->drawList(spec, false, 1, oEvent::drawAll);
      loadPage(gdi);
    }
    else if (bi.id=="Draw") {
      save(gdi, true);
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;

      if (oe->classHasResults(cid)) {
        if (!gdi.ask("warning:drawresult"))
          return 0;
      }

      pClass pc=oe->getClass(cid);

      if (!pc)
        throw std::exception("Class not found");
      if (EditChanged)
        gdi.sendCtrlMessage("Save");

      gdi.clearPage(false);

      gdi.addString("", boldLarge, "Lotta klassen X#"+pc->getName());
      gdi.dropLine();
      gdi.pushX();
      gdi.setRestorePoint();

      gdi.fillDown();
      bool multiDay = oe->hasPrevStage();

      if (multiDay) {
        gdi.addCheckbox("HandleMultiDay", "Använd funktioner för fleretappsklass", ClassesCB, true);
      }

      gdi.addSelection("Method", 200, 200, ClassesCB, "Metod:");
      gdi.dropLine(1.5);
      gdi.popX();

      gdi.setRestorePoint("MultiDayDraw");

      lastDrawMethod = NOMethod;
      drawDialog(gdi, getDefaultMethod(true), *pc);
    }
    else if (bi.id == "HandleMultiDay") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Method", lbi);

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      if (gdi.isChecked(bi.id) && (lastDrawMethod == DMReversePursuit ||
                                   lastDrawMethod == DMPursuit)) {
        drawDialog(gdi, getDefaultMethod(false), *pc);
      }
      else
        setMultiDayClass(gdi, gdi.isChecked(bi.id), lastDrawMethod);

    }
    else if (bi.id=="Bibs") {
      save(gdi, true);
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;

      pClass pc=oe->getClass(cid);
      if (!pc)
        throw std::exception("Class not found");

      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Nummerlappar i X#" + pc->getName());
      gdi.dropLine();
      gdi.setRestorePoint("bib");
    
      gdi.addString("", 10, "help:bibs");
      gdi.dropLine();

      vector< pair<string, size_t> > bibOptions;
      vector< pair<string, size_t> > bibTeamOptions;
      bibOptions.push_back(make_pair(lang.tl("Manuell"), AutoBibManual));
      bibOptions.push_back(make_pair(lang.tl("Löpande"), AutoBibConsecutive));
      bibOptions.push_back(make_pair(lang.tl("Ingen"), AutoBibNone));
      bibOptions.push_back(make_pair(lang.tl("Automatisk"), AutoBibExplicit));
    
      gdi.fillRight();
      gdi.pushX();

      gdi.addSelection("BibSettings", 150, 100, ClassesCB, "Metod:");
      gdi.addItem("BibSettings", bibOptions);

      AutoBibType bt = pc->getAutoBibType();
      gdi.selectItemByData("BibSettings", bt);
      string bib = pc->getDCI().getString("Bib");
      
      if (pc->getNumDistinctRunners() > 1) {
        bibTeamOptions.push_back(make_pair(lang.tl("Oberoende"), BibFree));
        bibTeamOptions.push_back(make_pair(lang.tl("Samma"), BibSame));
        bibTeamOptions.push_back(make_pair(lang.tl("Ökande"), BibAdd));
        bibTeamOptions.push_back(make_pair(lang.tl("Sträcka"), BibLeg));
        gdi.addSelection("BibTeam", 80, 100, 0, "Lagmedlem:", "Ange relation mellan lagets och deltagarnas nummerlappar.");
        gdi.addItem("BibTeam", bibTeamOptions);
        gdi.selectItemByData("BibTeam", pc->getBibMode());
      }

      gdi.dropLine(1.1);
      gdi.addInput("Bib", "", 10, 0, "");
      gdi.dropLine(3);

      gdi.fillRight();
      gdi.popX();
      gdi.addString("", 0, "Antal reserverade nummerlappsnummer mellan klasser:");
      gdi.dropLine(-0.1);
      gdi.addInput("BibGap", itos(oe->getBibClassGap()), 5);
      gdi.dropLine(3);
      gdi.popX();
      gdi.fillRight();
      gdi.addButton("DoBibs", "Tilldela", ClassesCB).setDefault();
    
      gdi.setInputStatus("Bib", bt == AutoBibExplicit || bt == AutoBibManual);
      gdi.setInputStatus("BibGap", bt != AutoBibManual);
      
      if (bt != AutoBibManual)
        gdi.setTextTranslate("DoBibs",  "OK");
      if (bt == AutoBibExplicit)
        gdi.setText("Bib", bib);

      gdi.fillDown();
      gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
      gdi.popX();

      EditChanged=false;
      gdi.refresh();
    }
    else if (bi.id=="DoBibs") {
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;
      pClass pc = oe->getClass(cid);

      AutoBibType bt = AutoBibType(gdi.getSelectedItem("BibSettings").first);

      pair<int, bool> teamBib = gdi.getSelectedItem("TeamBib");
      if (teamBib.second) {
        pc->setBibMode(BibMode(teamBib.first));
      }

      pc->getDI().setString("Bib", getBibCode(bt, gdi.getText("Bib")));
      pc->synchronize();

      if (bt == AutoBibManual) {
        oe->addBib(cid, 0, gdi.getText("Bib"));
      }
      else {
        oe->setBibClassGap(gdi.getTextNo("BibGap"));
        oe->addAutoBib();
      }

      gdi.restore("bib", false);
      gdi.dropLine();
      gdi.addButton("Cancel", "Återgå", ClassesCB).setDefault();

      oListParam par;
      par.selection.insert(cid);
      oListInfo info;
      par.listCode = EStdStartList;
      par.setLegNumberCoded(0);
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);

      gdi.refresh();
      return 0;
    }
    else if (bi.id=="Split") {
      save(gdi, true);
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Dela klass: X#" + pc->getName());
      gdi.dropLine();
      int tot, fin, dns;
      oe->getNumClassRunners(pc->getId(), 0, tot, fin, dns);
      gdi.addString("", fontMediumPlus, "Antal deltagare: X#" + itos(tot));
      gdi.dropLine(1.2);
      gdi.pushX();
      gdi.fillRight();
      gdi.addSelection("Type", 200, 100, ClassesCB, "Typ av delning:");
      gdi.selectItemByData("Type", 1);
      vector< pair<string, size_t> > mt;
      oClass::getSplitMethods(mt);
      gdi.addItem("Type", mt);
      gdi.selectFirstItem("Type");

      gdi.addSelection("SplitInput", 100, 150, ClassesCB, "Antal klasser:").setExtra(tot);
      vector< pair<string, size_t> > sp;
      for (int k = 2; k < 10; k++)
        sp.push_back(make_pair(itos(k), k));
      gdi.addItem("SplitInput", sp);
      gdi.selectFirstItem("SplitInput");

      gdi.dropLine(3);
      gdi.popX();
      
      updateSplitDistribution(gdi, 2, tot);
    }
    else if (bi.id=="DoSplit") {
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      ListBoxInfo lbi;
      gdi.getSelectedItem("Type", lbi);

      int number = gdi.getSelectedItem("SplitInput").first;
      vector<int> parts(number);
      for (int k = 0; k < number; k++) {
        string id = "CLS" + itos(k);
        parts[k] = gdi.getTextNo(id, false);
      }

      vector<int> outClass;

      pc->splitClass(ClassSplitMethod(lbi.data), parts, outClass);
      
      gdi.clearPage(true);
      gdi.addButton("Cancel", "Återgå", ClassesCB);

      oListParam par;
      par.selection.insert(outClass.begin(), outClass.end());
      oListInfo info;
      par.listCode = EStdStartList;
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);
    }
    else if (bi.id=="Merge") {
      save(gdi, true);
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      vector< pair<string, size_t> > rawClass, cls; 
      oe->fillClasses(rawClass, oEvent::extraNone, oEvent::filterNone);
      int def = -1;
      bool next = false;
      for (size_t k = 0; k < rawClass.size(); k++) {
        if (rawClass[k].second == ClassId)
          next = true;
        else {
          cls.push_back(rawClass[k]);

          if (next) {
            next = false;
            def = rawClass[k].second;
          }
        }
      }
      if (cls.empty())
        throw std::exception("En klass kan inte slås ihop med sig själv.");

      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Slå ihop klass: X (denna klass behålls)#" + pc->getName());
      gdi.dropLine();
      gdi.addString("", 10, "help:12138");
      gdi.dropLine(2);
      gdi.pushX();
      gdi.fillRight();
      gdi.addSelection("Class", 150, 300, 0, "Klass att slå ihop:");
      gdi.addItem("Class", cls);
      if (def != -1)
        gdi.selectItemByData("Class", def);
      else
        gdi.selectFirstItem("Class");

      gdi.dropLine();
      gdi.addButton("DoMergeAsk", "Slå ihop", ClassesCB).setDefault();
      gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
      gdi.dropLine(3);
      gdi.popX();
    }
    else if (bi.id=="DoMergeAsk") {
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      pClass mergeClass = oe->getClass(gdi.getSelectedItem("Class").first);

      if (!mergeClass)
        throw std::exception("Ingen klass vald.");

      if (mergeClass->getId() == ClassId)
        throw std::exception("En klass kan inte slås ihop med sig själv.");

      if (gdi.ask("Vill du flytta löpare från X till Y och ta bort Z?#"
        + mergeClass->getName() + "#" + pc->getName() + "#" + mergeClass->getName())) {
        bi.id = "DoMerge";
        return classCB(gdi, type, &bi);
      }
      return false;
    }
    else if (bi.id=="DoMerge") {
      if (!checkClassSelected(gdi))
        return false;

      pClass pc=oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Class not found");

      ListBoxInfo lbi;
      gdi.getSelectedItem("Class", lbi);

      if (signed(lbi.data)<=0)
        throw std::exception("Ingen klass vald.");

      if (lbi.data==ClassId)
        throw std::exception("En klass kan inte slås ihop med sig själv.");

      pc->mergeClass(lbi.data);
      gdi.clearPage(true);
      gdi.addButton("Cancel", "Återgå", ClassesCB);

      oListParam par;
      par.selection.insert(ClassId);
      oListInfo info;
      par.listCode = EStdStartList;
      oe->generateListInfo(par, gdi.getLineHeight(), info);
      oe->generateList(gdi, false, info, true);
      gdi.refresh();
    }
    else if (bi.id=="MultiCourse") {
      save(gdi, false);
      multiCourse(gdi, 0);
      gdi.refresh();
    }
    else if (bi.id=="Save")
      save(gdi, false);
    else if (bi.id=="Add") {
      string name = gdi.getText("Name");
      pClass c = oe->getClass(ClassId);
      if (!name.empty() && c && c->getName() != name) {
        if (gdi.ask("Vill du lägga till klassen 'X'?#" + name)) {
          c = oe->addClass(name);
          ClassId = c->getId();
          save(gdi, false);
          return true;
        }
      }


      save(gdi, true);
      pClass pc = oe->addClass(oe->getAutoClassName(), 0);
      if (pc) {
        oe->fillClasses(gdi, "Classes", oEvent::extraDrawn, oEvent::filterNone);
        selectClass(gdi, pc->getId());
        gdi.setInputFocus("Name", true);
      }
    }
    else if (bi.id=="Remove") {
      EditChanged=false;
      if (!checkClassSelected(gdi))
        return false;

      DWORD cid=ClassId;

      if (oe->isClassUsed(cid))
        gdi.alert("Klassen används och kan inte tas bort.");
      else
        oe->removeClass(cid);

      oe->fillClasses(gdi, "Classes", oEvent::extraDrawn, oEvent::filterNone);
      ClassId = 0;
      selectClass(gdi, 0);
    }
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Classes") {
      if (gdi.isInputChanged(""))
        save(gdi, true);

      selectClass(gdi, bi.data);
    }
    else if (bi.id == "SplitInput") {
      int num = bi.data;
      updateSplitDistribution(gdi, num, bi.getExtraInt());
    }
    else if (bi.id=="Courses")
      EditChanged=true;
    else if (bi.id == "BibSettings") {
      AutoBibType bt = (AutoBibType)bi.data;
      gdi.setInputStatus("Bib", bt == AutoBibExplicit || bt == AutoBibManual);
      gdi.setInputStatus("BibGap", bt != AutoBibManual);
      if (bt != AutoBibManual)
        gdi.setTextTranslate("DoBibs",  "OK");
      else
        gdi.setTextTranslate("DoBibs",  "Tilldela");
    }
    else if (bi.id=="Type") {
      if (bi.data==1) {
        gdi.setTextTranslate("TypeDesc", "Antal klasser:", true);
        gdi.setText("SplitInput", "2");
      }
      else {
        gdi.setTextTranslate("TypeDesc", "Löpare per klass:", true);
        gdi.setText("SplitInput", "100");
      }
    }
    else if (bi.id == "Method") {
      pClass pc = oe->getClass(ClassId);
      if (!pc)
        throw std::exception("Nullpointer exception");

      drawDialog(gdi, DrawMethod(bi.data), *pc);
    }
  }
  else if (type==GUI_INPUTCHANGE) {
    //InputInfo ii=*(InputInfo *)data;

  }
  else if (type==GUI_CLEAR) {
    if (ClassId>0)
      save(gdi, true);
    if (EditChanged) {
      if (gdi.ask("Spara ändringar?"))
        gdi.sendCtrlMessage("Save");
    }
    return true;
  }
  return 0;
}

void TabClass::readClassSettings(gdioutput &gdi)
{
  for (size_t k=0;k<cInfo.size();k++) {
    ClassInfo &ci = cInfo[k];
    int id = ci.classId;
    const string &start = gdi.getText("S"+itos(id));
    const string &intervall = gdi.getText("I"+itos(id));
    int vacant = atoi(gdi.getText("V"+itos(id)).c_str());
    int reserved = atoi(gdi.getText("R"+itos(id)).c_str());

    int startPos = oe->getRelativeTime(start) - drawInfo.firstStart;

    if (drawInfo.firstStart == 0 && startPos == -1)
      startPos = 0;
    else if (startPos<0 || (startPos % drawInfo.baseInterval)!=0)
      throw std::exception("Ogiltig tid");

    startPos /= drawInfo.baseInterval;

    int intervalPos = convertAbsoluteTimeMS(intervall);

    if (intervalPos<0 || intervalPos == NOTIME || (intervalPos % drawInfo.baseInterval)!=0)
      throw std::exception("Ogiltigt startintervall");

    intervalPos /= drawInfo.baseInterval;

    if (ci.nVacant != vacant) {
      ci.nVacantSpecified = true;
      ci.nVacant = vacant;
    }

    if (ci.nExtra != reserved) {
      ci.nExtraSpecified = true;
      ci.nExtra = reserved;
    }
    // If times has been changed, mark this class to be kept fixed
    if (ci.firstStart != startPos || ci.interval!=intervalPos)
      ci.hasFixedTime = true;

    ci.firstStart = startPos;
    ci.interval = intervalPos;

    drawInfo.classes[ci.classId] = ci;

    cInfoCache[ci.classId] = ci;
    cInfoCache[ci.classId].hasFixedTime = false;
  }
}

void TabClass::visualizeField(gdioutput &gdi) {
  ClassInfo::sSortOrder = 3;
  sort(cInfo.begin(), cInfo.end());
  ClassInfo::sSortOrder = 0;

  vector<int> field;
  vector<int> index(cInfo.size(), -1);

  for (size_t k = 0;k < cInfo.size(); k++) {
    const ClassInfo &ci = cInfo[k];
    int laststart = ci.firstStart + (ci.nRunners-1) * ci.interval;

    for (size_t j = 0; j < field.size(); j++) {
      if (field[j] < ci.firstStart) {
        index[k] = j;
        field[j] = laststart;
        break;
      }
    }
    if (index[k] == -1) {
      index[k] = field.size();
      field.push_back(laststart);
    }
/*
    string first=oe->getAbsTime(ci.firstStart*drawInfo.baseInterval+drawInfo.firstStart);
    string last=oe->getAbsTime((laststart)*drawInfo.baseInterval+drawInfo.firstStart);
    pClass pc=oe->getClass(ci.classId);*/
  }

  map<int, int> groupNumber;
  map<int, string> groups;
  int freeNumber = 1;
  for (size_t k = 0;k < cInfo.size(); k++) {
    const ClassInfo &ci = cInfo[k];
    if (!groupNumber.count(ci.unique))
      groupNumber[ci.unique] = freeNumber++;

    pClass pc = oe->getClass(ci.classId);
    if (pc) {
      if (groups[ci.unique].empty())
        groups[ci.unique] = pc->getName();
      else if (groups[ci.unique].size() < 64)
        groups[ci.unique] += ", " + pc->getName();
      else
        groups[ci.unique] += "...";
    }
  }

  int marg = gdi.scaleLength(20);
  int xp = gdi.getCX() + marg;
  int yp = gdi.getCY() + marg;
  int h = gdi.scaleLength(12);
  int w = gdi.scaleLength(6);
  int maxx = xp, maxy = yp;

  RECT rc;
  for (size_t k = 0;k < cInfo.size(); k++) {
    const ClassInfo &ci = cInfo[k];
    rc.top = yp + index[k] * h;
    rc.bottom = rc.top + h - 1;
    int g = ci.unique;
    GDICOLOR color = GDICOLOR(RGB(((g * 30)&0xFF), ((g * 50)&0xFF), ((g * 70)&0xFF)));
    for (int j = 0; j<ci.nRunners; j++) {
      rc.left = xp + (ci.firstStart + j * ci.interval) * w;
      rc.right = rc.left + w-1;
      gdi.addRectangle(rc, color);
    }
    pClass pc = oe->getClass(ci.classId);
    if (pc) {
      string course = pc->getCourse() ? ", " + pc->getCourse()->getName() : "";
      string tip = "X (Y deltagare, grupp Z, W)#" + pc->getName() + course + "#" + itos(ci.nRunners) + "#" + itos(groupNumber[ci.unique])
                    + "#" + groups[ci.unique];
      rc.left = xp + ci.firstStart * w;
      int laststart = ci.firstStart + (ci.nRunners-1) * ci.interval;
      rc.right = xp + (laststart + 1) * w;
      gdi.addToolTip("", tip, 0, &rc);
      maxx = max<int>(maxx, rc.right);
      maxy = max<int>(maxy, rc.bottom);
    }
  }
  rc.left = xp - marg;
  rc.top = yp - marg;
  rc.bottom = maxy + marg;
  rc.right = maxx + marg;
  gdi.addRectangle(rc, colorLightYellow, true, true);

}

void TabClass::showClassSettings(gdioutput &gdi)
{
  ClassInfo::sSortOrder = 2;
  sort(cInfo.begin(), cInfo.end());
  ClassInfo::sSortOrder=0;

  int laststart=0;
  for (size_t k=0;k<cInfo.size();k++) {
    const ClassInfo &ci = cInfo[k];
    laststart=max(laststart, ci.firstStart+ci.nRunners*ci.interval);
  }

  int y = 0;
  int xp = gdi.getCX();
  const int width = 80;

  if (!cInfo.empty()) {
    gdi.dropLine();

    y = gdi.getCY();
    gdi.addString("", y, xp, 1, "Sammanställning, klasser:");
    gdi.addString("", y, xp+300, 0, "Första start:");
    gdi.addString("", y, xp+300+width, 0, "Intervall:");
    gdi.addString("", y, xp+300+width*2, 0, "Vakanser:");
    gdi.addString("", y, xp+300+width*3, 0, "Reserverade:");
  }

  gdi.pushX();
  for (size_t k=0;k<cInfo.size();k++) {
    const ClassInfo &ci = cInfo[k];
    char bf1[128];
    char bf2[128];
    int laststart = ci.firstStart + (ci.nRunners-1) * ci.interval;
    string first=oe->getAbsTime(ci.firstStart*drawInfo.baseInterval+drawInfo.firstStart);
    string last=oe->getAbsTime((laststart)*drawInfo.baseInterval+drawInfo.firstStart);
    pClass pc=oe->getClass(ci.classId);
    sprintf_s(bf1, "%s, %d", pc ? pc->getName().c_str(): "-", ci.nRunners);
    sprintf_s(bf2, "%d-[%d]-%d (%s-%s)", ci.firstStart, ci.interval, laststart, first.c_str(), last.c_str());

    gdi.fillRight();
    
    int id = ci.classId;
    gdi.addString("C" + itos(id), 0, "X platser. Startar Y#" + string(bf1) + "#" + bf2);
    y = gdi.getCY();
    gdi.addInput(xp+300, y, "S"+itos(id), first, 7, DrawClassesCB);
    gdi.addInput(xp+300+width, y, "I"+itos(id), formatTime(ci.interval*drawInfo.baseInterval), 7, DrawClassesCB);
    gdi.addInput(xp+300+width*2, y, "V"+itos(id), itos(ci.nVacant), 7, DrawClassesCB);
    gdi.addInput(xp+300+width*3, y, "R"+itos(id), itos(ci.nExtra), 7, DrawClassesCB);

    if (k%5 == 4)
      gdi.dropLine(1);

    gdi.dropLine(1.6);
    gdi.fillDown();
    gdi.popX();
  }

  gdi.dropLine();
  gdi.pushX();

  gdi.fillRight();
 
  gdi.addButton("VisualizeDraw", "Visualisera startfältet...", ClassesCB);
 
  gdi.addButton("SaveDrawSettings", "Spara starttider", ClassesCB, 
    "Spara inmatade tider i tävlingen utan att tilldela starttider.");

  gdi.addButton("DrawAllAdjust", "Ändra inställningar", ClassesCB,
        "Ändra grundläggande inställningar och gör en ny fördelning").setExtra(13);
 
  if (!cInfo.empty()) {
    gdi.addButton("DrawAdjust", "Uppdatera fördelning", ClassesCB,
      "Uppdatera fördelningen av starttider med hänsyn till manuella ändringar ovan");
       gdi.disableInput("DrawAdjust");
  }
  gdi.popX();
  gdi.dropLine(3);

  gdi.fillRight();

  if (!cInfo.empty()) {
    gdi.pushX();

    RECT rc;
    rc.left = gdi.getCX();
    rc.top = gdi.getCY();
    rc.bottom = rc.top + gdi.getButtonHeight() + gdi.scaleLength(22) + gdi.getLineHeight();
    gdi.setCX(rc.left + gdi.scaleLength(10));
    gdi.setCY(rc.top + gdi.scaleLength(10));
    
    gdi.addSelection("Method", 200, 200, 0, "Metod:");
    gdi.addItem("Method", lang.tl("Lottning"), DMRandom);
    gdi.addItem("Method", lang.tl("SOFT-lottning"), DMSOFT);

    gdi.selectItemByData("Method", getDefaultMethod(false));

    gdi.addSelection("PairSize", 150, 200, 0, "Tillämpa parstart:");
    gdi.addItem("PairSize", getPairOptions());
    gdi.selectItemByData("PairSize", 1);

    gdi.dropLine(0.9);

    gdi.addButton("DoDrawAll", "Utför lottning", ClassesCB);
    
    rc.right = gdi.getCX() + gdi.scaleLength(5);
    gdi.addRectangle(rc, colorLightGreen);
    gdi.setCX(rc.right + gdi.scaleLength(10));
  }

  gdi.addButton("Cancel", "Avbryt", ClassesCB);

  gdi.fillDown();
  gdi.dropLine(2);
  gdi.popX();

 
  gdi.fillDown();
  gdi.popX();
  gdi.scrollToBottom();
  gdi.updateScrollbars();
  gdi.refresh();
}

void TabClass::selectClass(gdioutput &gdi, int cid)
{
  oe->fillCourses(gdi, "Courses", true);
  gdi.addItem("Courses", lang.tl("Ingen bana"), -2);

  if (cid==0) {
    gdi.restore("", true);
    gdi.disableInput("MultiCourse", true);
    gdi.enableInput("Courses");
    gdi.enableEditControls(false);
    gdi.setText("Name", "");
    gdi.selectItemByData("Courses", -2);
    gdi.check("AllowQuickEntry", true);

    if (gdi.hasField("FreeStart"))
      gdi.check("FreeStart", false);
    if (gdi.hasField("IgnoreStart"))
      gdi.check("IgnoreStart", false);

    if (gdi.hasField("DirectResult"))
      gdi.check("DirectResult", false);

    gdi.check("NoTiming", false);

    ClassId=cid;
    EditChanged=false;

    gdi.disableInput("Remove");
    gdi.disableInput("Save");
    return;
  }

  pClass pc = oe->getClass(cid);

  if (!pc) {
    selectClass(gdi, 0);
    return;
  }

  gdi.enableEditControls(true);
  gdi.enableInput("Remove");
  gdi.enableInput("Save");

  pc->synchronize();
  gdi.setText("Name", pc->getName());

  gdi.setText("ClassType", pc->getType());
  gdi.setText("StartName", pc->getStart());
  if (pc->getBlock()>0)
    gdi.selectItemByData("StartBlock", pc->getBlock());
  else
    gdi.selectItemByData("StartBlock", -1);

  if (gdi.hasField("Status")) {
    vector< pair<string, size_t> > out;
    size_t selected = 0;
    pc->getDCI().fillInput("Status", out, selected);
    gdi.addItem("Status", out);
    gdi.selectItemByData("Status", selected);
  }

  gdi.check("AllowQuickEntry", pc->getAllowQuickEntry());
  gdi.check("NoTiming", pc->getNoTiming());

  if (gdi.hasField("FreeStart"))
    gdi.check("FreeStart", pc->hasFreeStart());

  if (gdi.hasField("IgnoreStart"))
    gdi.check("IgnoreStart", pc->ignoreStartPunch());

  if (gdi.hasField("DirectResult"))
    gdi.check("DirectResult", pc->hasDirectResult());

  ClassId=cid;

  if (pc->hasTrueMultiCourse()) {
    gdi.restore("", false);

    multiCourse(gdi, pc->getNumStages());
    gdi.refresh();

    gdi.addItem("Courses", lang.tl("Flera banor"), -3);
    gdi.selectItemByData("Courses", -3);
    gdi.disableInput("Courses");
    gdi.check("CoursePool", pc->hasCoursePool());
    
    if (gdi.hasField("Unordered"))
      gdi.check("Unordered", pc->hasUnorderedLegs());

    if (gdi.hasField("MCourses")) {
      oe->fillCourses(gdi, "MCourses", true);
      string strId = "StageCourses_expl";
      gdi.setTextTranslate(strId, getCourseLabel(pc->hasCoursePool()), true);
    }

    if (gdi.hasData("SimpleMulti")) {
      bool hasStart = pc->getStartType(0) == STTime;

      gdi.setInputStatus("CommonStartTime", hasStart);
      gdi.check("CommonStart", hasStart);
      if (hasStart)
        gdi.setText("CommonStartTime", pc->getStartDataS(0));
      else
        gdi.setText("CommonStartTime", MakeDash("-"));

    }
    else {
      updateFairForking(gdi, pc);

      int nstage=pc->getNumStages();
      gdi.setText("NStage", nstage);

      for (int k=0;k<nstage;k++) {
        char legno[10];
        sprintf_s(legno, "%d", k);

        gdi.selectItemByData((string("LegType")+legno).c_str(), pc->getLegType(k));
        gdi.selectItemByData((string("StartType")+legno).c_str(), pc->getStartType(k));
        updateStartData(gdi, pc, k, false, true);
        gdi.setInputStatus(string("Restart")+legno, !pc->restartIgnored(k), true);
        gdi.setInputStatus(string("RestartRope")+legno, !pc->restartIgnored(k), true);

        if (gdi.hasField(string("Restart")+legno))
          gdi.setText(string("Restart")+legno, pc->getRestartTimeS(k));
        if (gdi.hasField(string("RestartRope")+legno))
          gdi.setText(string("RestartRope")+legno, pc->getRopeTimeS(k));
        if (gdi.hasField(string("MultiR")+legno))
          gdi.selectItemByData((string("MultiR")+legno).c_str(), pc->getLegRunner(k));
      }
    }
  }
  else {
    gdi.restore("", true);
    gdi.enableInput("MultiCourse", true);
    gdi.enableInput("Courses");
    pCourse pcourse = pc->getCourse();
    gdi.selectItemByData("Courses", pcourse ? pcourse->getId():-2);
  }

  gdi.selectItemByData("Classes", cid);

  ClassId=cid;
  EditChanged=false;
}

void TabClass::legSetup(gdioutput &gdi)
{
  gdi.restore("RelaySetup");
  gdi.pushX();
  gdi.fillDown();

  gdi.addString("", 10, "help:relaysetup");
  gdi.dropLine();
  gdi.addSelection("Predefined", 150, 200, MultiCB, "Fördefinierade tävlingsformer:").ignore(true);
  oe->fillPredefinedCmp(gdi, "Predefined");
  if (storedPredefined == oEvent::PredefinedTypes(-1)) {
    bool hasPatrol = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Patrol);
    bool hasRelay = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Relay);
    if (hasRelay)
      storedPredefined = oEvent::PRelay;
    else if (hasPatrol)
      storedPredefined = oEvent::PPatrol;
    else
      storedPredefined = oEvent::PNoSettings;
  }

  gdi.selectItemByData("Predefined", storedPredefined);

  gdi.fillRight();
  gdi.addInput("NStage", storedNStage, 4, MultiCB, "Antal sträckor:").ignore(true);
  gdi.addInput("StartTime",  storedStart, 6, MultiCB, "Starttid (HH:MM:SS):");
  gdi.popX();

  bool nleg;
  bool start;
  oe->setupRelayInfo(storedPredefined, nleg, start);
  gdi.setInputStatus("NStage", nleg);
  gdi.setInputStatus("StartTime", start);

  gdi.fillRight();
  gdi.dropLine(3);
  gdi.addButton("SetNStage", "Verkställ", MultiCB);
  gdi.fillDown();
  gdi.addButton("Cancel", "Avbryt", ClassesCB);

  gdi.popX();
}


void TabClass::multiCourse(gdioutput &gdi, int nLeg) {
  currentStage=-1;

  bool simpleView = nLeg==1;

  bool showGuide = (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Relay) ||
                   oe->getMeOSFeatures().hasFeature(MeOSFeatures::Patrol)) && nLeg==0;

  if (nLeg == 0 && !showGuide) {
    pClass pc = oe->getClass(ClassId);
    if (pc) {
      pc->setNumStages(1);
      pc->setStartType(0, STDrawn, false);
      pc->forceShowMultiDialog(true);
      selectClass(gdi, ClassId);
      return;
    }
  }

  gdi.disableInput("MultiCourse", true);
  gdi.setRestorePoint();
  gdi.fillDown();
  gdi.newColumn();


  int cx=gdi.getCX(), cy=gdi.getCY();
  gdi.setCX(cx+10);
  gdi.setCY(cy+10);

  if (simpleView) {
    gdi.addString("", fontMediumPlus, "Gafflade banor");
  }
  else {
    gdi.addString("", 2, "Flera banor / stafett / patrull / banpool");
    gdi.addString("", 0, "Låt klassen ha mer än en bana eller sträcka");
    gdi.dropLine();
  }
  gdi.setRestorePoint("RelaySetup");

  if (showGuide) {
    legSetup(gdi);
    RECT rc;
    rc.left = cx;
    rc.right = gdi.getWidth()+10;
    rc.bottom = gdi.getCY()+10;
    rc.top = cy;
    gdi.addRectangle(rc, colorLightGreen, true, false).set3D(true).setColor2(colorLightCyan);
  }
  else if (simpleView) {
    gdi.fillRight();
    gdi.pushX();
    gdi.setData("SimpleMulti", 1);
    gdi.dropLine();
    gdi.addCheckbox("CommonStart", "Gemensam start", MultiCB, false);
    //gdi.dropLine(-1);
    gdi.addInput("CommonStartTime", "", 10, 0, "");

    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(2);
    gdi.addCheckbox("CoursePool", "Använd banpool", MultiCB, false,
                      "Knyt löparna till banor från en pool vid målgång.");

    gdi.addButton("OneCourse", "Endast en bana", MultiCB, "Använd endast en bana i klassen");
    gdi.setRestorePoint("Courses");
    selectCourses(gdi, 0);

    RECT rc;
    rc.left = cx;
    rc.right = gdi.getWidth()+10;
    rc.bottom = gdi.getCY()+10;
    rc.top = cy;
    gdi.addRectangle(rc, colorLightBlue, true, false).set3D(true);
  }
  else {
    gdi.pushX();
    gdi.fillRight();
    gdi.addButton("ChangeLeg", "Ändra klassinställningar...", MultiCB, "Starta en guide som hjälper dig göra klassinställningar");

    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(2);

    gdi.dropLine(0.5);
    int headYPos=gdi.getCY();
    gdi.dropLine(1.2);

    vector< pair<string, size_t> > legs;
    legs.reserve(nLeg);
    for (int j=0;j<nLeg;j++) {
      char bf[16];
      sprintf_s(bf, lang.tl("Str. %d").c_str(), j+1);
      legs.push_back( make_pair(bf, j) );
    }

    bool multipleRaces = oe->getMeOSFeatures().hasFeature(MeOSFeatures::MultipleRaces);
    bool hasRelay = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Relay);

    for (int k=0;k<nLeg;k++) {
      char legno[10];
      int headXPos[10];
      sprintf_s(legno, "%d", k);
      gdi.fillRight();

      headXPos[0]=gdi.getCX();
      gdi.addStringUT(gdi.getCY(), gdi.getCX(), 1, itos(k+1) + ".");
      gdi.setCX(headXPos[0] + gdi.scaleLength(30));
      //gdi.addInput(legno, "", 2);
      //gdi.setText(legno, k+1);
    //  gdi.disableInput(legno);

      headXPos[1]=gdi.getCX();
      string legType(string("LegType")+legno);
      gdi.addSelection(legType, 100, 200, MultiCB);
      oClass::fillLegTypes(gdi, legType);

      headXPos[2]=gdi.getCX();
      string startType(string("StartType")+legno);
      gdi.addSelection(startType, 90, 200, MultiCB);
      oClass::fillStartTypes(gdi, startType, k == 0 || !(hasRelay || multipleRaces));

      headXPos[3]=gdi.getCX();
      gdi.addInput(string("StartData")+legno, "", 8, MultiCB);

      if (multipleRaces) {
        string multir(string("MultiR")+legno);
        headXPos[4]=gdi.getCX();
        gdi.addSelection(multir, 60, 200, MultiCB);
        gdi.addItem(multir, legs);
      }
      if (hasRelay) {
        headXPos[5]=gdi.getCX();
        gdi.addInput(string("RestartRope")+legno, "", 5, MultiCB);

        headXPos[6]=gdi.getCX();
        gdi.addInput(string("Restart")+legno, "", 5, MultiCB);
      }

      gdi.dropLine(-0.1);
      gdi.addButton(string("@Course")+legno, "Banor...", MultiCB);

      gdi.fillDown();
      gdi.popX();
      gdi.dropLine(2.1);

      if (k==0) { //Add headers
        gdi.addString("", headYPos, headXPos[0], 0, "Str.");
        gdi.addString("", headYPos, headXPos[1], 0, "Sträcktyp:");
        gdi.addString("", headYPos, headXPos[2], 0, "Starttyp:");
        gdi.addString("", headYPos, headXPos[3], 0, "Starttid:");
        if (multipleRaces)
          gdi.addString("", headYPos, headXPos[4], 0, "Löpare:");
        if (hasRelay) {
          gdi.addString("", headYPos, headXPos[5], 0, "Rep:");
          gdi.addString("", headYPos, headXPos[6], 0, "Omstart:");
        }
      }
    }

    gdi.pushX();
    gdi.fillRight();
    gdi.addCheckbox("CoursePool", "Använd banpool", MultiCB, false,
                      "Knyt löparna till banor från en pool vid målgång.");
    gdi.addCheckbox("Unordered", "Oordnade parallella sträckor", MultiCB, false,
                      "Tillåt löpare inom en parallell grupp att springa gruppens banor i godtycklig ordning.");
    gdi.popX();
    gdi.fillRight();
    gdi.dropLine(1.7);
    gdi.addString("FairForking", 1, "The forking is fair.");
    gdi.setCX(gdi.getCX() + gdi.getLineHeight() * 5);
    gdi.dropLine(-0.3);
    gdi.addButton("ShowForking", "Show forking...", MultiCB);
    gdi.fillDown();
    gdi.addButton("DefineForking", "Define forking...", MultiCB);

    gdi.popX();
    RECT rc;
    rc.left = cx;
    rc.right = gdi.getWidth()+10;
    rc.bottom = gdi.getCY()+10;
    rc.top = cy;
    gdi.addRectangle(rc, colorLightBlue, true, false).set3D(true);

    gdi.setRestorePoint("Courses");

    if (nLeg==1)
      gdi.sendCtrlMessage("@Course0");

  }
  gdi.refresh();
}

bool TabClass::checkClassSelected(const gdioutput &gdi) const
{
  if (ClassId<=0) {
    gdi.alert("Ingen klass vald.");
    return false;
  }
  else return true;
}

void TabClass::save(gdioutput &gdi, bool skipReload)
{
  bool checkValid = EditChanged || gdi.isInputChanged("");
  DWORD cid=ClassId;

  pClass pc;
  string name = gdi.getText("Name");

  if (cid==0 && name.empty())
    return;

  if (name.empty())
    throw std::exception("Klassen måste ha ett namn.");

  bool create=false;

  if (cid>0)
    pc=oe->getClass(cid);
  else {
    pc=oe->addClass(name);
    create=true;
  }

  if (!pc)
    throw std::exception("Class not found.");

  ClassId=pc->getId();

  pc->setName(name);
  if (gdi.hasField("StartName"))
    pc->setStart(gdi.getText("StartName"));

  if (gdi.hasField("ClassType"))
    pc->setType(gdi.getText("ClassType"));

  if (gdi.hasField("StartBlock"))
    pc->setBlock(gdi.getTextNo("StartBlock"));

  if (gdi.hasField("Status")) {
    pc->getDI().setEnum("Status", gdi.getSelectedItem("Status").first);
  }

  if (gdi.hasField("CoursePool"))
    pc->setCoursePool(gdi.isChecked("CoursePool"));

  if (gdi.hasField("Unordered"))
    pc->setUnorderedLegs(gdi.isChecked("Unordered"));

  pc->setAllowQuickEntry(gdi.isChecked("AllowQuickEntry"));
  pc->setNoTiming(gdi.isChecked("NoTiming"));

  if (gdi.hasField("FreeStart"))
    pc->setFreeStart(gdi.isChecked("FreeStart"));

  if (gdi.hasField("IgnoreStart"))
    pc->setIgnoreStartPunch(gdi.isChecked("IgnoreStart"));

  if (gdi.hasField("DirectResult")) {
    bool withDirect = gdi.isChecked("DirectResult");

    if (withDirect && !pc->hasDirectResult() && !hasWarnedDirect) {
      if (gdi.ask("warning:direct_result"))
        hasWarnedDirect = true;
      else
        withDirect = false;
    }

    pc->setDirectResult(withDirect);
  }

  int crs = gdi.getSelectedItem("Courses").first;

  if (crs==0) {
    //Skapa ny bana...
    pCourse pcourse=oe->addCourse("Bana "+name);
    pc->setCourse(pcourse);
    pc->synchronize();
    return;
  }
  else if (crs==-2)
    pc->setCourse(0);
  else if (crs > 0)
    pc->setCourse(oe->getCourse(crs));

  if (pc->hasMultiCourse()) {

    if (gdi.hasData("SimpleMulti")) {
      bool sim = gdi.isChecked("CommonStart");
      if (sim) {
        pc->setStartType(0, STTime, true);
        pc->setStartData(0, gdi.getText("CommonStartTime"));
      }
      else {
        pc->setStartType(0, STDrawn, true);
      }
    }
    else {
      int nstage=pc->getNumStages();
      bool needAdjust = false;
      for (int k=0;k<nstage;k++) {
        char legno[10];
        sprintf_s(legno, "%d", k);

        if (!gdi.hasField(string("LegType")+legno))
          continue;

        pc->setLegType(k, LegTypes(gdi.getSelectedItem(string("LegType")+legno).first));

        pc->setStartType(k, StartTypes(gdi.getSelectedItem(string("StartType")+legno).first), true);

        if (pc->getStartType(k) == STChange) {
          int val = gdi.getSelectedItem(string("StartData")+legno).first;
          if (val <= -10)
            pc->setStartData(k, val + 10);
          else
            pc->setStartData(k, 0);
        }
        else { 
          pc->setStartData(k, gdi.getText(string("StartData")+legno));
        }
        string key;

        key = string("Restart")+legno;
        if (gdi.hasField(key))
          pc->setRestartTime(k, gdi.getText(key));

        key = string("RestartRope")+legno;

        if (gdi.hasField(key))
          pc->setRopeTime(k, gdi.getText(key));

        key = string("MultiR")+legno;
        if (gdi.hasField(key)) {
          int mr = gdi.getSelectedItem(key).first; 
          
          if (pc->getLegRunner(k) != mr)
            needAdjust = true;

          pc->setLegRunner(k, mr);
        }
      }

      if (needAdjust)
        oe->adjustTeamMultiRunners(pc);
    }
  }

  pc->addClassDefaultFee(false);
  pc->updateChangedCoursePool();
  pc->synchronize();
  oe->reCalculateLeaderTimes(pc->getId());
  set<int> cls;
  cls.insert(pc->getId());
  oe->reEvaluateAll(cls, true);

  oe->fillClasses(gdi, "Classes", oEvent::extraDrawn, oEvent::filterNone);
  EditChanged=false;
  if (!skipReload) {
    ClassId = 0;
    selectClass(gdi, pc->getId());
  }

  if (checkValid) {
    // Check/warn that starts blocks are set up correctly
    vector<int> b;
    vector<string> s;
    oe->getStartBlocks(b, s);
    oe->sanityCheck(gdi, false, pc->getId());
  }
}

struct ButtonData {
  ButtonData(const char *idIn,
             const char *labelIn,
             bool glob) : id(idIn), label(labelIn), global(glob) {}
  string id;
  string label;
  bool global;
};

bool TabClass::loadPage(gdioutput &gdi)
{
  oe->checkDB();
  oe->checkNecessaryFeatures();
  gdi.selectTab(tabId);
  gdi.clearPage(false);
  int xp=gdi.getCX();

  const int button_w=gdi.scaleLength(90);
  string switchMode;
  switchMode=tableMode ? "Formulärläge" : "Tabelläge";
  gdi.addButton(2, 2, button_w, "SwitchMode", switchMode,
    ClassesCB, "Välj vy", false, false).fixedCorner();

  if (tableMode) {
    Table *tbl=oe->getClassTB();
    gdi.addTable(tbl, xp, 30);
    return true;
  }

  if (showForkingGuide) {
    try {
      defineForking(gdi, false);
    }
    catch(...) {
      showForkingGuide = false;
      throw;
    }
    return true;
  }
  ClassConfigInfo cnf;
  oe->getClassConfigurationInfo(cnf);

  bool showAdvanced = oe->getPropertyInt("AdvancedClassSettings", 0) != 0;
  gdi.addString("", boldLarge, "Klasser");

  gdi.fillDown();
  gdi.addListBox("Classes", 200, showAdvanced ? 512 : 420, ClassesCB, "").isEdit(false).ignore(true);
  gdi.setTabStops("Classes", 185);
  oe->fillClasses(gdi, "Classes", oEvent::extraDrawn, oEvent::filterNone);

  gdi.newColumn();
  gdi.dropLine(2);

  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("Name", "", 14, ClassesCB, "Klassnamn:");
  bool sameLineNameCourse = true;
  if (showAdvanced) {
    gdi.addCombo("ClassType", 100, 300, 0, "Typ:");
    oe->fillClassTypes(gdi, "ClassType");
    sameLineNameCourse = false;
  }

  if (showMulti(false) || !sameLineNameCourse) {
    gdi.dropLine(3);
    gdi.popX();
  }

  gdi.addSelection("Courses", 120, 400, ClassesCB, "Bana:");
  oe->fillCourses(gdi, "Courses", true);
  gdi.addItem("Courses", lang.tl("Ingen bana"), -2);

  if (showMulti(false)) {
    gdi.dropLine(0.9);
    if (showMulti(true)) {
      gdi.addButton("MultiCourse", "Flera banor/stafett...", ClassesCB);
    }
    else {
      gdi.addButton("MultiCourse", "Gafflade banor...", ClassesCB);
    }
    gdi.disableInput("MultiCourse");
  }

  gdi.popX();
  if (showAdvanced) {
    gdi.dropLine(3);

    gdi.addCombo("StartName", 120, 300, 0, "Startnamn:");
    oe->fillStarts(gdi, "StartName");

    gdi.addSelection("StartBlock", 80, 300, 0, "Startblock:");

    for (int k=1;k<=100;k++) {
      char bf[16];
      sprintf_s(bf, "%d", k);
      gdi.addItem("StartBlock", bf, k);
    }

    gdi.popX();
    gdi.dropLine(3);
    gdi.addSelection("Status", 200, 300, 0, "Status:");
  }

  gdi.popX();
  gdi.dropLine(3.5);
  gdi.addCheckbox("AllowQuickEntry", "Tillåt direktanmälan", 0);
  gdi.addCheckbox("NoTiming", "Utan tidtagning", 0);

  if (showAdvanced) {
    gdi.dropLine(2);
    gdi.popX();
    gdi.addCheckbox("FreeStart", "Fri starttid", 0, false, "Klassen lottas inte, startstämpling");
    gdi.addCheckbox("IgnoreStart", "Ignorera startstämpling", 0, false, "Uppdatera inte starttiden vid startstämpling");

    gdi.dropLine(2);
    gdi.popX();
    
    gdi.addCheckbox("DirectResult", "Resultat vid målstämpling", 0, false,
                    "help:DirectResult");

  }
  gdi.dropLine(2);
  gdi.popX();

  gdi.fillDown();
  gdi.addString("", 1, "Funktioner");

  vector<ButtonData> func;
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::DrawStartList))
    func.push_back(ButtonData("Draw", "Lotta / starttider...", false));
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Bib))
    func.push_back(ButtonData("Bibs", "Nummerlappar...", false));
  if (cnf.hasTeamClass())
    func.push_back(ButtonData("Restart", "Omstart...", true));
  if (showAdvanced) {
    func.push_back(ButtonData("Merge", "Slå ihop klasser...", false));
    func.push_back(ButtonData("Split", "Dela klassen...", false));
  }

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::DrawStartList))
    func.push_back(ButtonData("DrawMode", "Lotta flera klasser", true));
  func.push_back(ButtonData("QuickSettings", "Snabbinställningar", true));

  if (showAdvanced && oe->getMeOSFeatures().hasFeature(MeOSFeatures::Vacancy)) {
    vector<pRunner> rr;
    oe->getRunners(0, 0, rr, false);
    bool hasVac = false;
    for (size_t k = 0; k < rr.size(); k++) {
      if (rr[k]->isVacant()) {
        hasVac = true;
        break;
      }
    }
    if (hasVac)
      func.push_back(ButtonData("RemoveVacant", "Radera vakanser", true));
  }

  gdi.dropLine(0.3);
  gdi.pushX();
  gdi.fillRight();

  for (size_t k = 0; k < func.size(); k++) {
    ButtonInfo &bi = gdi.addButton(func[k].id, func[k].label, ClassesCB);
    if (!func[k].global)
      bi.isEdit(true);
    if ( k % 2 == 1) {
      gdi.popX();
      gdi.dropLine(2.5);
    }
  }

  gdi.popX();
  gdi.dropLine(3);
  gdi.fillRight();
  gdi.addButton("Save", "Spara", ClassesCB).setDefault();
  gdi.disableInput("Save");
  gdi.addButton("Remove", "Radera", ClassesCB);
  gdi.disableInput("Remove");
  gdi.addButton("Add", "Ny klass", ClassesCB);

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(3);
  gdi.addCheckbox("UseAdvanced", "Visa avancerade funktioner", ClassesCB, showAdvanced).isEdit(false);

  gdi.setOnClearCb(ClassesCB);
  gdi.setRestorePoint();

  gdi.setCX(xp);
  gdi.setCY(gdi.getHeight());

  gdi.addString("", 10, "help:26963");

  selectClass(gdi, ClassId);

  EditChanged=false;
  gdi.refresh();

  return true;
}

bool TabClass::showMulti(bool singleOnly) const {
  const MeOSFeatures &mf = oe->getMeOSFeatures();
  if (!singleOnly)
    return mf.hasFeature(MeOSFeatures::Relay) || mf.hasFeature(MeOSFeatures::Patrol) || mf.hasFeature(MeOSFeatures::ForkedIndividual);
  else
    return mf.hasFeature(MeOSFeatures::Relay) || mf.hasFeature(MeOSFeatures::Patrol) ||  mf.hasFeature(MeOSFeatures::MultipleRaces);
}

static int classSettingsCB(gdioutput *gdi, int type, void *data)
{
  TabClass &tc = dynamic_cast<TabClass &>(*gdi->getTabs().get(TClassTab));

  static string lastStart = "Start 1";
  if (type==GUI_INPUT) {
    InputInfo ii=*(InputInfo *)data;

    if (ii.id.substr(0,4) == "Strt") {
      lastStart = ii.text;
    }
  }
  else if (type == GUI_FOCUS) {
    InputInfo ii=*(InputInfo *)data;
    if (ii.id.substr(0,4) == "Strt") {
      if (ii.text.empty()) {
        gdi->setText(ii.id, lastStart);
        gdi->setInputFocus(ii.id, true);
      }
    }
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo bi = *(ButtonInfo*)data;
    if (bi.id == "SaveCS") {
      tc.saveClassSettingsTable(*gdi);
    }
  }
  else if (type == GUI_CLEAR) {
    tc.saveClassSettingsTable(*gdi);
    return 1;
  }
  return 0;
}

void TabClass::saveClassSettingsTable(gdioutput &gdi) {
  set<int> modifiedFee;
  bool modifiedBib = false;
  
  saveClassSettingsTable(gdi, modifiedFee, modifiedBib);

  oe->synchronize(true);
  if (gdi.hasField("BibGap")) {
    int gap = gdi.getTextNo("BibGap");
    if (oe->getBibClassGap() != gap) {
      oe->setBibClassGap(gap);
      modifiedBib = true;
    }
  }
  
  if (!modifiedFee.empty() && oe->getNumRunners() > 0) {
    bool updateFee = gdi.ask("ask:changedclassfee");

    if (updateFee)
      oe->applyEventFees(false, true, false, modifiedFee);
  }

  if (modifiedBib && gdi.ask("Vill du uppdatera alla nummerlappar?")) {
    oe->addAutoBib();
  }
  oe->synchronize(true);
  gdi.sendCtrlMessage("Cancel");
}

void TabClass::prepareForDrawing(gdioutput &gdi) {
  gdi.clearPage(false);
  gdi.addString("", 2, "Klassinställningar");
  int baseLine = gdi.getCY();
  gdi.addString("", 10, "help:59395");
  gdi.pushX();
  
  int by = gdi.getCY();
  gdi.setCX(gdi.getWidth());
  gdi.setCY(baseLine);
  gdi.addString("", 10, "help:59395_more");
  
  gdi.setCY(max(gdi.getCY(), by)); 
  gdi.popX();
  gdi.dropLine();

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Bib)) {
    gdi.fillRight();
    gdi.addString("", 0, "Antal reserverade nummerlappsnummer mellan klasser:");
    gdi.dropLine(-0.1);
    gdi.addInput("BibGap", itos(oe->getBibClassGap()), 5);
    gdi.popX();
    gdi.dropLine(1.5);
    gdi.fillDown();
  }

  getClassSettingsTable(gdi, classSettingsCB);

  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("SaveCS", "Spara", classSettingsCB);
  gdi.addButton("Cancel", "Avbryt", ClassesCB);

  gdi.refresh();
}

void TabClass::drawDialog(gdioutput &gdi, DrawMethod method, const oClass &pc) {
  oe->setProperty("DefaultDrawMethod", method);

  if (lastDrawMethod == method)
    return;

  if (lastDrawMethod == DMPursuit && method == DMReversePursuit)
    return;
  if (lastDrawMethod == DMReversePursuit && method == DMPursuit)
    return;

  if (lastDrawMethod == DMRandom && method == DMSOFT)
    return;
  if (lastDrawMethod == DMSOFT && method == DMRandom)
    return;

  int firstStart = 3600,
      interval = 120,
      vac = atoi(lastNumVac.c_str());

  int pairSize = lastPairSize;

  if (gdi.hasField("FirstStart"))
    firstStart = oe->getRelativeTime(gdi.getText("FirstStart"));
  else if (!lastFirstStart.empty())
    firstStart = oe->getRelativeTime(lastFirstStart);

  if (gdi.hasField("Interval"))
    interval = convertAbsoluteTimeMS(gdi.getText("Interval"));
  else if (!lastInterval.empty())
    interval = convertAbsoluteTimeMS(lastInterval);

  if (gdi.hasField("PairSize")) {
    pairSize = gdi.getSelectedItem("PairSize").first;
  }
  gdi.restore("MultiDayDraw", false);

  const bool multiDay = oe->hasPrevStage();

  if (method == DMSeeded) {
    gdi.addString("", 10, "help:seeding_info");
    gdi.dropLine(1);
    gdi.pushX();
    gdi.fillRight();
    ListBoxInfo &seedmethod = gdi.addSelection("SeedMethod", 120, 100, 0, "Seedningskälla:");
    vector< pair<string, size_t> > methods;
    oClass::getSeedingMethods(methods);
    gdi.addItem("SeedMethod", methods);
    if (lastSeedMethod == -1)
      gdi.selectFirstItem("SeedMethod");
    else
      gdi.selectItemByData("SeedMethod", lastSeedMethod);
    seedmethod.setSynchData(&lastSeedMethod);
    gdi.addInput("SeedGroups", lastSeedGroups, 32, 0, "Seedningsgrupper:", 
                 "Ange en gruppstorlek (som repeteras) eller flera kommaseparerade gruppstorlekar").
                 setSynchData(&lastSeedGroups);
    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(3);
    gdi.addCheckbox("PreventClubNb", "Hindra att deltagare från samma klubb startar på angränsande tider", 
                    0, lastSeedPreventClubNb).setSynchData(&lastSeedPreventClubNb);
    gdi.addCheckbox("ReverseSeedning", "Låt de bästa start först", 0, lastSeedReverse).
                    setSynchData(&lastSeedReverse);
  }
  else {
    gdi.popX();
    gdi.addString("", 10, "help:41641");
    gdi.dropLine(1);
  }

  if (method == DMRandom || method == DMSOFT || method == DMPursuit
      || method == DMReversePursuit || method == DMSeeded) {
    gdi.addSelection("PairSize", 150, 200, 0, "Tillämpa parstart:").setSynchData(&lastPairSize);
    gdi.addItem("PairSize", getPairOptions());
    gdi.selectItemByData("PairSize", pairSize);
  }
  gdi.fillRight();

  gdi.addInput("FirstStart", oe->getAbsTime(firstStart), 10, 0, "Första start:").setSynchData(&lastFirstStart);

  if (method == DMPursuit || method == DMReversePursuit) {
    gdi.addInput("MaxAfter", lastMaxAfter, 10, 0, "Maxtid efter:", "Maximal tid efter ledaren för att delta i jaktstart").setSynchData(&lastMaxAfter);
    gdi.addInput("TimeRestart", oe->getAbsTime(firstStart + 3600),  8, 0, "Första omstartstid:");
    gdi.addInput("ScaleFactor", lastScaleFactor,  8, 0, "Tidsskalning:").setSynchData(&lastScaleFactor);
  }

  if (method != DMSimultaneous)
    gdi.addInput("Interval", formatTime(interval), 10, 0, "Startintervall (min):").setSynchData(&lastInterval);

  if (method == DMRandom || method == DMSOFT || method == DMClumped)
    gdi.addInput("Vacanses", itos(vac), 10, 0, "Antal vakanser:").setSynchData(&lastNumVac);

  if ((method == DMRandom || method == DMSOFT || method == DMSeeded) && pc.getNumStages() > 1 && pc.getClassType() != oClassPatrol) {
    gdi.addSelection("Leg", 90, 100, 0, "Sträcka:", "Sträcka att lotta");
    for (unsigned k = 0; k<pc.getNumStages(); k++)
      gdi.addItem("Leg", lang.tl("Sträcka X#" + itos(k+1)), k);

    gdi.selectFirstItem("Leg");
  }

  if (int(method) < 10) {
    gdi.popX();
    gdi.dropLine(3.5);

    gdi.fillRight();
    gdi.addCheckbox("HandleBibs", "Tilldela nummerlappar:", ClassesCB, lastHandleBibs).setSynchData(&lastHandleBibs);
    gdi.dropLine(-0.2);
    gdi.addInput("Bib", "", 10, 0, "", "Mata in första nummerlappsnummer, eller blankt för att ta bort nummerlappar");
    gdi.disableInput("Bib");
    gdi.fillDown();
    gdi.dropLine(2.5);
    gdi.popX();
  }
  else {
    gdi.popX();
    gdi.dropLine(3.5);
  }

  gdi.fillRight();
  
  if (method != DMSimultaneous)
    gdi.addButton("DoDraw", "Lotta klassen", ClassesCB, "Lotta om hela klassen");
  else
    gdi.addButton("DoDraw", "Tilldela", ClassesCB, "Tilldela starttider");

  if (method == DMRandom || method == DMSOFT) {
    gdi.addButton("DoDrawBefore", "Ej lottade, före", ClassesCB, "Lotta löpare som saknar starttid");
    gdi.addButton("DoDrawAfter", "Ej lottade, efter", ClassesCB, "Lotta löpare som saknar starttid");
  }

  gdi.addButton("DoDeleteStart", "Radera starttider", ClassesCB);

  gdi.fillDown();
  gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();

  gdi.popX();
  gdi.dropLine();

  setMultiDayClass(gdi, multiDay, method);

  EditChanged=false;
  gdi.refresh();

  lastDrawMethod = method;
}

void TabClass::setMultiDayClass(gdioutput &gdi, bool hasMulti, DrawMethod defaultMethod) {

  gdi.clearList("Method");
  gdi.addItem("Method", lang.tl("Lottning"), DMRandom);
  gdi.addItem("Method", lang.tl("SOFT-lottning"), DMSOFT);
  gdi.addItem("Method", lang.tl("Klungstart"), DMClumped);
  gdi.addItem("Method", lang.tl("Gemensam start"), DMSimultaneous);
  gdi.addItem("Method", lang.tl("Seedad lottning"), DMSeeded);
  
  if (hasMulti) {
    gdi.addItem("Method", lang.tl("Jaktstart"), DMPursuit);
    gdi.addItem("Method", lang.tl("Omvänd jaktstart"), DMReversePursuit);
  }
  else if (defaultMethod > 10)
    defaultMethod = getDefaultMethod(hasMulti);

  gdi.selectItemByData("Method", defaultMethod);

  if (gdi.hasField("Vacanses")) {
    gdi.setInputStatus("Vacanses", !hasMulti);
    gdi.setInputStatus("HandleBibs", !hasMulti);

    if (hasMulti) {
      gdi.check("HandleBibs", false);
      gdi.setInputStatus("Bib", false);
    }
  }

  if (gdi.hasField("DoDrawBefore")) {
    gdi.setInputStatus("DoDrawBefore", !hasMulti);
    gdi.setInputStatus("DoDrawAfter", !hasMulti);
  }
}

void TabClass::pursuitDialog(gdioutput &gdi) {
  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Jaktstart");
  gdi.dropLine();
  vector<pClass> cls;
  oe->getClasses(cls, true);

  gdi.setRestorePoint("Pursuit");

  gdi.pushX();

  gdi.fillRight();

  gdi.addInput("MaxAfter", formatTime(pSavedDepth), 10, 0, "Maxtid efter:", "Maximal tid efter ledaren för att delta i jaktstart");
  gdi.addInput("TimeRestart", "+" + formatTime(pFirstRestart),  8, 0, "Första omstartstid:",  "Ange tiden relativt klassens första start");
  gdi.addInput("Interval", formatTime(pInterval),  8, 0, "Startintervall:",  "Ange startintervall för minutstart");
  char bf[32];
  sprintf_s(bf, "%f", pTimeScaling);
  gdi.addInput("ScaleFactor", bf,  8, 0, "Tidsskalning:");

  gdi.dropLine(4);
  gdi.popX();
  gdi.fillDown();
  //xxx 
  //gdi.addCheckbox("Pairwise", "Tillämpa parstart", 0, false);
  gdi.addSelection("PairSize", 150, 200, 0, "Tillämpa parstart:");
  gdi.addItem("PairSize", getPairOptions());
  gdi.selectItemByData("PairSize", 1);

  int cx = gdi.getCX();
  int cy = gdi.getCY();

  const int len5 = gdi.scaleLength(5);
  const int len40 = gdi.scaleLength(30);
  const int len200 = gdi.scaleLength(200);

  gdi.addString("", cy, cx, 1, "Välj klasser");
  gdi.addString("", cy, cx + len200 + len40, 1, "Första starttid");
  cy += gdi.getLineHeight()*2;

  for (size_t k = 0; k<cls.size(); k++) {
    map<int, PursuitSettings>::iterator st = pSettings.find(cls[k]->getId());

    if (st == pSettings.end()) {
      pSettings.insert(make_pair(cls[k]->getId(), PursuitSettings(*cls[k])));
      st = pSettings.find(cls[k]->getId());
    }
    
    PursuitSettings &ps = st->second;
    int fs = cls[k]->getDrawFirstStart();
    if (fs > 0)
      ps.firstTime = fs;

    ButtonInfo &bi = gdi.addCheckbox(cx, cy + len5, "PLUse" + itos(k), "", ClassesCB, ps.use);
    bi.setExtra(cls[k]->getId());
    gdi.addStringUT(cy, cx + len40, 0, cls[k]->getName(), len200);

    gdi.addInput(cx + len200 + len40, cy, "First" + itos(k), oe->getAbsTime(ps.firstTime), 8);

    if (!ps.use)
      gdi.disableInput(("First" + itos(k)).c_str());

    cy += int(gdi.getLineHeight()*1.8);
  }

  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("SelectAllNoneP", "Välj alla", ClassesCB).setExtra(1);
  gdi.addButton("SelectAllNoneP", "Välj ingen", ClassesCB).setExtra(0);
  gdi.popX();
  gdi.dropLine(3);
  RECT rc;
  rc.left = gdi.getCX();
  rc.top = gdi.getCY();
  rc.bottom = rc.top + gdi.getButtonHeight() + gdi.scaleLength(17);
  gdi.setCX(rc.left + gdi.scaleLength(10));
  gdi.setCY(rc.top + gdi.scaleLength(10));
  gdi.addButton("DoPursuit", "Jaktstart", ClassesCB).setDefault().setExtra(1);
  gdi.addButton("DoPursuit", "Omvänd jaktstart", ClassesCB).setExtra(2);

  rc.right = gdi.getCX() + gdi.scaleLength(5);
  gdi.addRectangle(rc, colorLightGreen);
  gdi.setCX(rc.right + gdi.scaleLength(10));

  gdi.addButton("SavePursuit", "Spara starttider", ClassesCB, "Spara inmatade tider i tävlingen utan att tilldela starttider.");

  gdi.addButton("CancelPursuit", "Återgå", ClassesCB).setCancel();
  gdi.refresh();
}


void TabClass::showClassSelection(gdioutput &gdi, int &bx, int &by, GUICALLBACK classesCB) const {
  gdi.pushY();
  int cx = gdi.getCX();
  int width = gdi.scaleLength(230);
  gdi.addListBox("Classes", 200, 400, classesCB, "Klasser:", "", true);
  gdi.setTabStops("Classes", 185);
  gdi.fillRight();
  gdi.pushX();

  gdi.addButton("SelectAll", "Välj allt", ClassesCB,
                "Välj alla klasser").isEdit(true);

  gdi.addButton("SelectMisses", "Saknad starttid", ClassesCB,
    "Välj klasser där någon löpare saknar starttid").isEdit(true);

  gdi.dropLine(2.3);
  gdi.popX();

  gdi.addButton("SelectUndrawn", "Ej lottade", ClassesCB,
    "Välj klasser där alla löpare saknar starttid").isEdit(true);

  gdi.fillDown();
  gdi.addButton("SelectNone", "Välj inget", ClassesCB,
    "Avmarkera allt").isEdit(true);
  gdi.popX();

  vector<int> blocks;
  vector<string> starts;
  oe->getStartBlocks(blocks, starts);
  map<string, int> sstart;
  for (size_t k = 0; k < starts.size(); k++) {
    sstart.insert(make_pair(starts[k], k));
  }
  if (sstart.size() > 1) {
    gdi.fillRight();
    int cnt = 0;
    for (map<string, int>::reverse_iterator it = sstart.rbegin(); it != sstart.rend(); ++it) {
      if ((cnt & 1)==0 && cnt>0) {
        gdi.dropLine(2);
        gdi.popX();
      }
      string name = it->first;
      if (name.empty())
        name = lang.tl("övriga");
      gdi.addButton("SelectStart", "Välj X#" + name, ClassesCB, "").isEdit(true).setExtra(it->second);
      cnt++;
    }
    gdi.dropLine(2.5);
    gdi.popX();
    gdi.fillDown();
  }

  oe->fillClasses(gdi, "Classes", oEvent::extraDrawn, oEvent::filterNone);

  by = gdi.getCY()+gdi.getLineHeight();
  bx = gdi.getCX();
  //gdi.newColumn();
  gdi.setCX(cx+width);
  gdi.popY();
}

void TabClass::enableLoadSettings(gdioutput &gdi) {
  if (!gdi.hasField("LoadSettings"))
    return;
  set<int> sel;
  gdi.getSelection("Classes", sel);
  bool ok = !sel.empty();

  gdi.setInputStatus("PrepareDrawAll", ok);
  gdi.setInputStatus("EraseStartAll", ok);
  ok = false;
  for (set<int>::iterator it = sel.begin(); it != sel.end(); ++it) {
    pClass pc = oe->getClass(*it);

    if (pc) {
      if (pc->getDrawFirstStart() > 0 && pc->getDrawInterval() > 0) {
        ok = true;
        break;
      }
    }
  }

  gdi.setInputStatus("LoadSettings", ok);
}


void TabClass::simultaneous(int classId, string time) {
  pClass pc = oe->getClass(classId);

  if (!pc)
    throw exception();

  pc->getNumStages();
  if (pc->getNumStages() == 0) {
    pCourse crs = pc->getCourse();
    pc->setNumStages(1);
    if (crs)
      pc->addStageCourse(0, crs->getId());
  }

  pc->setStartType(0, STTime, false);
  pc->setStartData(0, time);
  pc->synchronize(true);
  pc->forceShowMultiDialog(false);
  oe->reCalculateLeaderTimes(pc->getId());
  set<int> cls;
  cls.insert(pc->getId());
  oe->reEvaluateAll(cls, true);
}

const char *TabClass::getCourseLabel(bool pool) {
  if (pool)
    return "Banpool:";
  else
    return "Sträckans banor:";
}

void TabClass::selectCourses(gdioutput &gdi, int legNo) {
  gdi.restore("Courses", false);
  gdi.setRestorePoint("Courses");
  char bf[128];
  pClass pc=oe->getClass(ClassId);

  if (!pc) {
    gdi.refresh();
    return;
  }
  currentStage = legNo;
  gdi.dropLine();
  gdi.pushX();
  gdi.fillRight();

  bool simpleView = pc->getNumStages() == 1;

  if (!simpleView) {
    sprintf_s(bf, lang.tl("Banor för %s, sträcka %d").c_str(), pc->getName().c_str(), legNo+1);
    gdi.addStringUT(1, bf);
    ButtonInfo &bi1 = gdi.addButton("@Course" + itos(legNo-1), "<< Föregående", MultiCB);
    if (legNo<=0)
      gdi.disableInput(bi1.id.c_str());
    ButtonInfo &bi2 = gdi.addButton("@Course" + itos(legNo+1), "Nästa >>", MultiCB);
    if (unsigned(legNo + 1) >= pc->getNumStages())
      gdi.disableInput(bi2.id.c_str());
    gdi.popX();
    gdi.dropLine(2.5);
  }
  gdi.fillRight();
  int x1=gdi.getCX();
  gdi.addListBox("StageCourses", 180, 200, MultiCB, getCourseLabel(pc->hasCoursePool())).ignore(true);
  pc->fillStageCourses(gdi, currentStage, "StageCourses");
  int x2=gdi.getCX();
  gdi.fillDown();
  gdi.addListBox("MCourses", 180, 200, MultiCB, "Banor:").ignore(true);
  oe->fillCourses(gdi, "MCourses", true);

  gdi.setCX(x1);
  gdi.fillRight();

  gdi.addButton("MRemove", "Ta bort markerad >>", MultiCB);
  gdi.setCX(x2);
  gdi.fillDown();

  gdi.addButton("MAdd", "<< Lägg till", MultiCB);
  gdi.setCX(x1);
  gdi.refresh();
  if (pc->getNumStages() > 1)
    gdi.scrollTo(gdi.getCX(), gdi.getCY());
}

void TabClass::updateFairForking(gdioutput &gdi, pClass pc) const {
  vector< vector<int> > forks;
  vector< vector<int> > forksC;
  set< pair<int, int> > unfairLegs;

  if (pc->checkForking(forksC, forks, unfairLegs)) {
    BaseInfo *bi = gdi.setText("FairForking", gdi.getText("FairForking"), false);
    TextInfo &text = dynamic_cast<TextInfo &>(*bi);
    text.setColor(colorGreen);
    gdi.setText("FairForking", lang.tl("The forking is fair."), true);
  }
  else {
    BaseInfo *bi = gdi.setText("FairForking", gdi.getText("FairForking"), false);
    TextInfo &text = dynamic_cast<TextInfo &>(*bi);
    text.setColor(colorRed);
    gdi.setText("FairForking", lang.tl("The forking is not fair."), true);
  }
}

void TabClass::defineForking(gdioutput &gdi, bool clearSettings) {
  pClass pc = oe->getClass(ClassId);
  if (clearSettings) {
    forkingSetup.clear();
    forkingSetup.resize(pc->getNumStages());
  }
  else if (forkingSetup.size() != pc->getNumStages())
    throw meosException("Internal error");

  showForkingGuide = true;
  gdi.clearPage(false);
  int tx = gdi.getCX();
  int ty = gdi.getCY();

  gdi.dropLine(2);
  gdi.pushY();
  gdi.addListBox("AllCourses", 180, 300, 0, "Banor:", "", true);
  oe->fillCourses(gdi, "AllCourses", true);
  int bxp = gdi.getCX();
  int byp = gdi.getCY();
  gdi.fillDown();

  gdi.newColumn();
  gdi.popY();
  gdi.addListBox("AllStages", 180, 300, MultiCB, "Legs:", "", true);
  int ns = pc->getNumStages();

  gdi.newColumn();
  gdi.fillDown();
  gdi.popY();
  gdi.addButton("AssignCourses", "Assign selected courses to selected legs", MultiCB);
  gdi.disableInput("AssignCourses");

  gdi.dropLine();
  gdi.addString("", boldText, "Forking setup");
  gdi.dropLine(0.5);
  for (int k = 0; k < ns; k++) {
    LegTypes lt = pc->getLegType(k);
    if (lt != LTIgnore) {
      gdi.addString("leg"+ itos(k), 0, "Leg X: Do not modify.#" + itos(k+1));
      gdi.addItem("AllStages", lang.tl("Leg X#" + itos(k+1)), k);
    }
  }

  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("ApplyForking", "Calculate and apply forking", MultiCB);
  gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
  gdi.disableInput("ApplyForking");

  gdi.setCX(bxp);
  gdi.setCY(byp);
  gdi.fillDown();
  gdi.addButton("ClearCourses", "Clear selections", MultiCB);

  gdi.addString("", 10, "help:assignforking");
  gdi.addString("", ty, tx, boldLarge, "Assign courses and apply forking to X#" + pc->getName());

  if (!clearSettings)
    gdi.sendCtrlMessage("AssignCourses");

  gdi.refresh();
}

void TabClass::getClassSettingsTable(gdioutput &gdi, GUICALLBACK cb) {
  
  vector<pClass> cls;
  oe->getClasses(cls, true);

  int yp = gdi.getCY();
  int a = gdi.scaleLength(160);
  int b = gdi.scaleLength(250);
  int c = gdi.scaleLength(300);
  int d = gdi.scaleLength(350);
  int e = gdi.scaleLength(510);
  int et = gdi.scaleLength(605);
  int f = gdi.scaleLength(510);
  int g = gdi.scaleLength(535);

  int ek1 = 0, ekextra = 0;
  bool useEco = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy);

  gdi.setOnClearCb(cb);

  if (useEco) {
    ek1 = gdi.scaleLength(70);
    ekextra = gdi.scaleLength(35);
    d += 4 * ek1 + ekextra;
    e += 4 * ek1 + ekextra;
    et += 4 * ek1 + ekextra;
    f += 4 * ek1 + ekextra;
    g += 4 * ek1 + ekextra;

    gdi.addString("", yp, c+ek1, 1, "Avgift");
    gdi.addString("", yp, c+2*ek1, 1, "Sen avgift");
    gdi.addString("", yp, c+3*ek1, 1, "Red. avgift");
    gdi.addString("", yp, c+4*ek1, 1, "Sen red. avgift");
  }

  gdi.addString("", yp, gdi.getCX(), 1, "Klass");
  gdi.addString("", yp, a, 1, "Start");
  gdi.addString("", yp, b, 1, "Block");
  gdi.addString("", yp, c, 1, "Index");
  gdi.addString("", yp, d, 1, "Bana");
  
  const bool useBibs = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Bib);
  const bool useTeam = oe->hasTeam();

  vector< pair<string, size_t> > bibOptions;
  vector< pair<string, size_t> > bibTeamOptions;
  
  if (useBibs) {
    gdi.addString("", yp, e, 1, "Nummerlapp");
    bibOptions.push_back(make_pair(lang.tl("Manuell"), 0));
    bibOptions.push_back(make_pair(lang.tl("Löpande"), 1));
    bibOptions.push_back(make_pair(lang.tl("Ingen"), 2));
    
    int bibW = gdi.scaleLength(100);
    
    if (useTeam) {
      gdi.addString("", yp, et, 1, "Lagmedlem");
    
      bibTeamOptions.push_back(make_pair(lang.tl("Oberoende"), BibFree));
      bibTeamOptions.push_back(make_pair(lang.tl("Samma"), BibSame));
      bibTeamOptions.push_back(make_pair(lang.tl("Ökande"), BibAdd));
      bibTeamOptions.push_back(make_pair(lang.tl("Sträcka"), BibLeg));
      bibW += gdi.scaleLength(85);
    }
    
    f += bibW;
    g += bibW;
  }
  
  gdi.addString("", yp, f, 1, "Direktanmälan");
  
  vector< pair<string,size_t> > arg;
  oe->fillCourses(arg, true);
  
  for (size_t k = 0; k < cls.size(); k++) {
    pClass it = cls[k];
    int yp = gdi.getCY();
    string id = itos(it->getId());
    gdi.addStringUT(0, it->getName(), 0);
    gdi.addInput(a, yp, "Strt"+id, it->getStart(), 7, cb);
    string blk = it->getBlock()>0 ? itos(it->getBlock()) : "";
    gdi.addInput(b, yp, "Blck"+id, blk, 4);
    gdi.addInput(c, yp, "Sort"+id, itos(it->getDCI().getInt("SortIndex")), 4);

    if (useEco) {
      gdi.addInput(c + ek1, yp, "Fee"+id, oe->formatCurrency(it->getDCI().getInt("ClassFee")), 5);
      gdi.addInput(c + 2*ek1, yp, "LateFee"+id, oe->formatCurrency(it->getDCI().getInt("HighClassFee")), 5);
      gdi.addInput(c + 3*ek1, yp, "RedFee"+id, oe->formatCurrency(it->getDCI().getInt("ClassFeeRed")), 5);
      gdi.addInput(c + 4*ek1, yp, "RedLateFee"+id, oe->formatCurrency(it->getDCI().getInt("HighClassFeeRed")), 5);
    }

    string crs = "Cors"+id;
    gdi.addSelection(d, yp, crs, 150, 400);

    if (it->hasTrueMultiCourse()) {
      gdi.addItem(crs, lang.tl("Flera banor"), -5);
      gdi.selectItemByData(crs.c_str(), -5);
      gdi.disableInput(crs.c_str());
    }
    else {
      gdi.addItem(crs, arg);
      gdi.selectItemByData(crs.c_str(), it->getCourseId());
    }
    
    if (useBibs)  {
      gdi.addCombo(e, yp, "Bib" + id, 90, 100, 0, "", "Ange löpande numrering eller första nummer i klassen.");
      gdi.addItem("Bib" + id, bibOptions);

      string bib = it->getDCI().getString("Bib");
      AutoBibType bt = it->getAutoBibType();
      if (bt != AutoBibExplicit)
        gdi.selectItemByData("Bib"+ id, bt);
      else
        gdi.setText("Bib"+ id, bib);

      if (useTeam && it->getNumDistinctRunners() > 1) {
        gdi.addSelection(et, yp, "BibTeam" + id, 80, 100, 0, "", "Ange relation mellan lagets och deltagarnas nummerlappar.");
        gdi.addItem("BibTeam" + id, bibTeamOptions);
        gdi.selectItemByData("BibTeam" + id, it->getBibMode());
      }
    }
    
    gdi.addCheckbox(g, yp, "Dirc"+id, "   ", 0, it->getAllowQuickEntry());
    
    gdi.dropLine(-0.3);

  }
}

void TabClass::saveClassSettingsTable(gdioutput &gdi, set<int> &classModifiedFee, bool &modifiedBib) {
  vector<pClass> cls;
  oe->getClasses(cls, true);
  classModifiedFee.clear();
  modifiedBib = false;

  for (size_t k = 0; k < cls.size(); k++) {
    pClass it = cls[k];

    string id = itos(it->getId());
    string start = gdi.getText("Strt"+id);
    int block = gdi.getTextNo("Blck"+id);
    int sort = gdi.getTextNo("Sort"+id);

    if (gdi.hasField("Fee" + id)) {
      int fee = oe->interpretCurrency(gdi.getText("Fee"+id));
      int latefee = oe->interpretCurrency(gdi.getText("LateFee"+id));
      int feered = oe->interpretCurrency(gdi.getText("RedFee"+id));
      int latefeered = oe->interpretCurrency(gdi.getText("RedLateFee"+id));

      int oFee = it->getDCI().getInt("ClassFee");
      int oLateFee = it->getDCI().getInt("HighClassFee");
      int oFeeRed = it->getDCI().getInt("ClassFeeRed");
      int oLateFeeRed = it->getDCI().getInt("HighClassFeeRed");

      if (oFee != fee || oLateFee != latefee ||
          oFeeRed != feered || oLateFeeRed != latefeered)
        classModifiedFee.insert(it->getId());

      it->getDI().setInt("ClassFee", fee);
      it->getDI().setInt("HighClassFee", latefee);
      it->getDI().setInt("ClassFeeRed", feered);
      it->getDI().setInt("HighClassFeeRed", latefeered);
    }

    if (gdi.hasField("Bib" + id)) {
      ListBoxInfo lbi;
      bool mod = false;
      if (gdi.getSelectedItem("Bib" + id, lbi)) {
        mod = it->getDI().setString("Bib", getBibCode(AutoBibType(lbi.data), "1"));
      }
      else {
        const string &v = gdi.getText("Bib" + id);
        mod = it->getDI().setString("Bib", v);
      }
      modifiedBib |= mod;

      if (gdi.hasField("BibTeam" + id)) {
        ListBoxInfo lbi;
        if (gdi.getSelectedItem("BibTeam" + id, lbi)) {
          if (it->getBibMode() != lbi.data)
            modifiedBib = true;
          it->setBibMode(BibMode(lbi.data));
        }
      }
    }

    int courseId = 0;
    ListBoxInfo lbi;
    if (gdi.getSelectedItem("Cors"+id, lbi))
      courseId = lbi.data;
    bool direct = gdi.isChecked("Dirc"+id);

    it->setStart(start);
    it->setBlock(block);
    if (courseId != -5)
      it->setCourse(oe->getCourse(courseId));
    it->getDI().setInt("SortIndex", sort);
    it->setAllowQuickEntry(direct);
  }
}

string TabClass::getBibCode(AutoBibType bt, const string &key) {
  if (bt == AutoBibManual)
    return "";
  else if (bt == AutoBibConsecutive)
    return "*";
  else if (bt == AutoBibNone)
    return "-";
  else 
    return key;
}

void TabClass::updateStartData(gdioutput &gdi, pClass pc, int leg, bool updateDependent, bool forceWrite) {
  string sdKey = "StartData"+itos(leg);
      
  BaseInfo &sdataBase = gdi.getBaseInfo(sdKey.c_str());
  StartTypes st = pc->getStartType(leg);

  if (st == STChange) {
    if (typeid(sdataBase) != typeid(ListBoxInfo)) {
      InputInfo sdII = dynamic_cast<InputInfo &>(sdataBase);
      gdi.removeControl(sdKey);
      gdi.addSelection(sdII.getX(), sdII.getY(), sdKey, sdII.getWidth(), 200, MultiCB);
      setParallelOptions(sdKey, gdi, pc, leg);
    }
    else if (forceWrite) {
      setParallelOptions(sdKey, gdi, pc, leg);
    }
  }
  else {
    if (typeid(sdataBase) != typeid(InputInfo)) {
      ListBoxInfo sdLBI = dynamic_cast<ListBoxInfo &>(sdataBase);
      gdi.removeControl(sdKey);
      string val = "-";
      gdi.addInput(sdLBI.getX(), sdLBI.getY(), sdKey, pc->getStartDataS(leg), 8, MultiCB);
    }
    else if (forceWrite) {
      gdi.setText(sdKey, pc->getStartDataS(leg), true);
    }
    gdi.setInputStatus(sdKey, !pc->startdataIgnored(leg));
  }

  if (updateDependent) {
    for (size_t j = 0; j < pc->getNumStages(); j++) {
      if (j != leg && pc->getStartType(leg) == STChange) {
        setParallelOptions("StartData"+itos(j), gdi, pc, j);
      }
    }
  }
}

void TabClass::setParallelOptions(const string &sdKey, gdioutput &gdi, pClass pc, int  legno) {
  int baseLeg = legno;
  while (baseLeg > 0 && pc->isParallel(baseLeg))
    baseLeg--;
  baseLeg--;
  int sd = pc->getStartData(legno);

  vector< pair<string, size_t> > opt;
  int defKey = 0;
  opt.push_back(make_pair(lang.tl("Ordnat"), 0));
  for (int k = 0; k <= baseLeg; k++) {
    if (!pc->isOptional(k)) {
      opt.push_back(make_pair(lang.tl("Str. X#" + itos(k+1)), (k-legno) - 10));
      if (sd == k-legno)
        defKey = sd - 10;
    }
  }
  if (defKey == 0) {
    pc->setStartData(legno, 0); 
  }

  gdi.addItem(sdKey, opt);
  gdi.selectItemByData(sdKey, defKey);
}

void TabClass::updateSplitDistribution(gdioutput &gdi, int num, int tot) const {
  gdi.restore("SplitDistr", false);
  gdi.setRestorePoint("SplitDistr");
  
  vector<int> distr;

  for (int k = 0; k < num; k++) {
    int frac = tot / (num-k);
    
    distr.push_back(frac);
    tot -= frac;
  }
  sort(distr.rbegin(), distr.rend());

  gdi.dropLine();
  gdi.fillDown();
  gdi.pushX();
  for (size_t k = 0; k < distr.size(); k++) {
    int yp = gdi.getCY();
    int xp = gdi.getCX();
    gdi.addString("", yp, xp, 0, "Klass X:#" + itos(k+1));
    gdi.addInput(xp + gdi.scaleLength(100), yp, "CLS" + itos(k), itos(distr[k]), 4);
    gdi.popX();
  }

  gdi.dropLine(1.5);
  gdi.fillRight();
  gdi.popX();
  gdi.addButton("DoSplit", "Dela", ClassesCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", ClassesCB).setCancel();
  gdi.popX();

  gdi.refresh();
}

DrawMethod TabClass::getDefaultMethod(bool allowPursuit) const {
  int dm = oe->getPropertyInt("DefaultDrawMethod", DMSOFT);
  if (!allowPursuit) {
    if (dm == DMRandom)
      return DMRandom;
    else
      return DMSOFT;
  }
  else {
    switch (dm) {
      case DMRandom:
        return DMRandom;
      case DMPursuit:
        return DMPursuit;
      case DMReversePursuit:
        return DMReversePursuit;
      case DMSeeded:
        return DMSeeded;
      default:
        return DMSOFT;
    }
  }
}

vector< pair<string, size_t> > TabClass::getPairOptions() {
  vector< pair<string, size_t> > res;

  res.push_back(make_pair(lang.tl("Ingen parstart"), 1));
  res.push_back(make_pair(lang.tl("Parvis (två och två)"), 2));
  for (int j = 3; j <= 10; j++) {
    res.push_back(make_pair(lang.tl("X och Y[N by N]#" + itos(j) + "#" + itos(j)), j));
  }
  return res;
}

void TabClass::readDrawInfo(gdioutput &gdi, DrawInfo &drawInfo) {
  drawInfo.maxCommonControl = gdi.getSelectedItem("MaxCommonControl").first;

  drawInfo.maxVacancy=gdi.getTextNo("VacancesMax");
  drawInfo.minVacancy=gdi.getTextNo("VacancesMin");
  drawInfo.vacancyFactor = 0.01*atof(gdi.getText("Vacances").c_str());
  drawInfo.extraFactor = 0.01*atof(gdi.getText("Extra").c_str());

  drawInfo.baseInterval=convertAbsoluteTimeMS(gdi.getText("BaseInterval"));
  drawInfo.allowNeighbourSameCourse = gdi.isChecked("AllowNeighbours");
  drawInfo.coursesTogether = gdi.isChecked("CoursesTogether");
  drawInfo.minClassInterval = convertAbsoluteTimeMS(gdi.getText("MinInterval"));
  drawInfo.maxClassInterval = convertAbsoluteTimeMS(gdi.getText("MaxInterval"));
  drawInfo.nFields = gdi.getTextNo("nFields");
  drawInfo.firstStart = oe->getRelativeTime(gdi.getText("FirstStart"));
}

void TabClass::writeDrawInfo(gdioutput &gdi, const DrawInfo &drawInfo) {
  gdi.selectItemByData("MaxCommonControl", drawInfo.maxCommonControl);

  gdi.setText("VacancesMax", drawInfo.maxVacancy);
  gdi.setText("VacancesMin", drawInfo.minVacancy);
  gdi.setText("Vacances", itos(int(drawInfo.vacancyFactor *100.0)) + "%");
  gdi.setText("Extra", itos(int(drawInfo.extraFactor * 100.0) ) + "%");

  gdi.setText("BaseInterval", formatTime(drawInfo.baseInterval));

  gdi.check("AllowNeighbours", drawInfo.allowNeighbourSameCourse);
  gdi.check("CoursesTogether", drawInfo.coursesTogether);
  gdi.setText("MinInterval", formatTime(drawInfo.minClassInterval));
  gdi.setText("MaxInterval", formatTime(drawInfo.maxClassInterval));
  gdi.setText("nFields", drawInfo.nFields);
  gdi.setText("FirstStart", oe->getAbsTime(drawInfo.firstStart));
}
