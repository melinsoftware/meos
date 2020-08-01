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
#include <cassert>

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "csvparser.h"
#include "SportIdent.h"
#include "gdifonts.h"
#include "IOF30Interface.h"
#include "meosexception.h"
#include "MeOSFeatures.h"
#include "oEventDraw.h"
#include "oListInfo.h"

#include "TabCourse.h"
#include "TabCompetition.h"
#include "meos_util.h"
#include "pdfwriter.h"

TabCourse::TabCourse(oEvent *poe):TabBase(poe)
{
  clearCompetitionData();
}

TabCourse::~TabCourse(void)
{
}

void LoadCoursePage(gdioutput &gdi);
void LoadClassPage(gdioutput &gdi);

void TabCourse::selectCourse(gdioutput &gdi, pCourse pc)
{
  if (gdi.hasWidget("Rogaining")) {
    gdi.setText("TimeLimit", L"");
    gdi.disableInput("TimeLimit");
    gdi.setText("PointLimit", L"");
    gdi.disableInput("PointLimit");
    gdi.setText("PointReduction", L"");
    gdi.disableInput("PointReduction");
    gdi.check("ReductionPerMinute", false);
    gdi.disableInput("ReductionPerMinute");
    gdi.selectItemByData("Rogaining", 0);
  }

  if (pc) {
    pc->synchronize();

    wstring uis =  pc->getControlsUI();
    gdi.setText("Controls", uis);

    gdi.setText("CourseExpanded", encodeCourse(uis, pc->getMaximumRogainingTime() > 0,
                pc->useFirstAsStart(), pc->useLastAsFinish()), true);

    gdi.setText("Name", pc->getName());

    gdi.setTextZeroBlank("Length", pc->getLength());
    gdi.setTextZeroBlank("Climb", pc->getDI().getInt("Climb"));
    gdi.setTextZeroBlank("NumberMaps", pc->getNumberMaps());

    gdi.check("FirstAsStart", pc->useFirstAsStart());
    gdi.check("LastAsFinish", pc->useLastAsFinish());

    if (gdi.hasWidget("Rogaining")) {
      int rt = pc->getMaximumRogainingTime();
      int rp = pc->getMinimumRogainingPoints();

      if ( rt > 0 ) {
        gdi.selectItemByData("Rogaining", 1);
        gdi.enableInput("TimeLimit");
        gdi.setText("TimeLimit", formatTimeHMS(rt));
        gdi.enableInput("PointReduction");
        gdi.setText("PointReduction", itow(pc->getRogainingPointsPerMinute()));
        gdi.enableInput("ReductionPerMinute");
        gdi.check("ReductionPerMinute", pc->getDCI().getInt("RReductionMethod") != 0);
      }
      else if (rp > 0) {
        gdi.selectItemByData("Rogaining", 2);
        gdi.enableInput("PointLimit");
        gdi.setText("PointLimit", itow(rp));
      }
    }

    courseId = pc->getId();
    gdi.enableInput("Remove");
    gdi.enableInput("Save");

    gdi.selectItemByData("Courses", pc->getId());
    gdi.setText("CourseProblem", lang.tl(pc->getCourseProblems()), true);
    vector<pClass> cls;
    vector<pCourse> crs;
    oe->getClasses(cls, true);
    wstring usedInClasses;
    for (size_t k = 0; k < cls.size(); k++) {
      int nleg = max<int>(cls[k]->getNumStages(), 1);
      int nlegwithcrs = 0;
      vector<wstring> usage;
      set<int> allClassCrs;
      for (int j = 0; j < nleg; j++) {
        cls[k]->getCourses(j, crs);
        if (!crs.empty())
          nlegwithcrs++;

        bool done = false;
        for (size_t i = 0; i < crs.size(); i++) {
          if (!crs[i])
            continue;
          allClassCrs.insert(crs[i]->getId());
          if (!done && crs[i] == pc) {
            usage.push_back(cls[k]->getLegNumber(j));
            done = true; // Cannot break, fill allClasssCrs
          }
        }
      }
      wstring add;
      if (usage.size() == nleg || 
          (usage.size() == nlegwithcrs && nlegwithcrs > 0) || 
          (!usage.empty() && allClassCrs.size() == 1)) {
        add = cls[k]->getName();
      }
      else if (!usage.empty()) {
        add = cls[k]->getName();
        add += L" (";
        for (size_t i = 0; i < usage.size(); i++) {
          if (i > 0)
            add += L", ";
          add += usage[i];
        }
        add += L")";
      }
      
      if (!add.empty()) {
        if (!usedInClasses.empty())
          usedInClasses += L", ";
        usedInClasses += add;
      }
    }
    gdi.setText("CourseUse", usedInClasses, true);
    pCourse shortens = pc->getLongerVersion();
    if (shortens)
      gdi.setTextTranslate("Shortens", L"Avkortar: X#" + shortens->getName(), true);
    else
      gdi.setText("Shortens", L"", true);

    gdi.enableEditControls(true);

    fillCourseControls(gdi, pc->getControlsUI());
    int cc = pc->getCommonControl();
    gdi.check("WithLoops", cc != 0);
    gdi.setInputStatus("CommonControl", cc != 0);
    if (cc) {
      gdi.selectItemByData("CommonControl", cc);
    }

    fillOtherCourses(gdi, *pc, cc != 0);
    auto sh = pc->getShorterVersion();
    gdi.check("Shorten", sh.first);
    gdi.setInputStatus("ShortCourse", sh.first);
    gdi.selectItemByData("ShortCourse", sh.second ? sh.second->getId() : 0);
  }
  else {
    gdi.setText("Name", L"");
    gdi.setText("Controls", L"");
    gdi.setText("CourseExpanded", L"");

	  gdi.setText("Length", L"");
	  gdi.setText("Climb", L"");
	  gdi.setText("NumberMaps", L"");
    gdi.check("FirstAsStart", false);
    gdi.check("LastAsFinish", false);
    courseId = 0;
    gdi.disableInput("Remove");
    gdi.disableInput("Save");
    gdi.selectItemByData("Courses", -1);
    gdi.setText("CourseProblem", L"", true);
    gdi.setText("CourseUse", L"", true);
    gdi.setText("Shortens", L"", true);
    gdi.check("WithLoops", false);
    gdi.clearList("CommonControl");
    gdi.setInputStatus("CommonControl", false);
    gdi.check("Shorten", false);
    gdi.clearList("ShortCourse");
    gdi.setInputStatus("ShortCourse", false);

    gdi.enableEditControls(false);
  }
  gdi.refreshFast();
  gdi.setInputStatus("DrawCourse", pc != 0);  
}

int CourseCB(gdioutput *gdi, int type, void *data) {
  TabCourse &tc = dynamic_cast<TabCourse &>(*gdi->getTabs().get(TCourseTab));
  return tc.courseCB(*gdi, type, data);
}

void TabCourse::save(gdioutput &gdi, int canSwitchViewMode) {
  DWORD cid = courseId;

  pCourse pc;
  wstring name=gdi.getText("Name");

  if (cid == 0 && name.empty())
    return;

  if (name.empty()) {
    gdi.alert("Banan måste ha ett namn.");
    return;
  }

  bool create=false;
  if (cid>0)
    pc=oe->getCourse(cid);
  else {
    pc=oe->addCourse(name);
    create=true;
  }

  bool firstAsStart = gdi.isChecked("FirstAsStart");
  bool lastAsFinish = gdi.isChecked("LastAsFinish");
  bool oldFirstAsStart = pc->useFirstAsStart();
  if (!oldFirstAsStart && firstAsStart) {
    vector<pRunner> cr;
    oe->getRunners(0, pc->getId(), cr, false);
    bool hasRes = false;
    for (size_t k = 0; k < cr.size(); k++) {
      if (cr[k]->getCard() != 0) {
        hasRes = true;
        break;
      }
    }
    if (hasRes) {
      firstAsStart = gdi.ask(L"ask:firstasstart");
    }
  }


  pc->setName(name);
  bool changedCourse = pc->importControls(gdi.narrow(gdi.getText("Controls")), true, true);
  pc->setLength(gdi.getTextNo("Length"));
  pc->getDI().setInt("Climb", gdi.getTextNo("Climb"));
  pc->setNumberMaps(gdi.getTextNo("NumberMaps"));
  pc->firstAsStart(firstAsStart);
  pc->lastAsFinish(lastAsFinish);

  if (gdi.isChecked("WithLoops")) {
    int cc = gdi.getTextNo("CommonControl");
    if (cc == 0)
      throw meosException("Ange en varvningskontroll för banan");
    pc->setCommonControl(cc);
  }
  else
    pc->setCommonControl(0);

  if (gdi.isChecked("Shorten")) {
    ListBoxInfo ci;
    if (gdi.getSelectedItem("ShortCourse", ci) && oe->getCourse(ci.data)) {
      pc->setShorterVersion(true, oe->getCourse(ci.data));
    }
    else if (gdi.isChecked("WithLoops")) {
      pc->setShorterVersion(true, nullptr);
    }
    else
      throw meosException("Ange en avkortad banvariant");
  }
  else
    pc->setShorterVersion(false, 0);

  if (gdi.hasWidget("Rogaining")) {
    string t;
    pc->setMaximumRogainingTime(convertAbsoluteTimeMS(gdi.getText("TimeLimit")));
    pc->setMinimumRogainingPoints(_wtoi(gdi.getText("PointLimit").c_str()));
    int pr = _wtoi(gdi.getText("PointReduction").c_str());
    pc->setRogainingPointsPerMinute(pr);
    if (pr > 0) {
      int rmethod = gdi.isChecked("ReductionPerMinute") ? 1 : 0;
      pc->getDI().setInt("RReductionMethod", rmethod);
    }
  }

  pc->synchronize();//Update SQL

  oe->fillCourses(gdi, "Courses");
  oe->reEvaluateCourse(pc->getId(), true);

  if (canSwitchViewMode != 2 && changedCourse && pc->getLegLengths().size() > 2) {
    if (canSwitchViewMode == 1) {
      if(gdi.ask(L"ask:updatelegs")) {
        gdi.sendCtrlMessage("LegLengths");
        return;
      }
    }
    else {
      gdi.alert("warn:updatelegs");
    }
  }

  if (gdi.getData("FromClassPage", cid)) {
    assert(false);
  }
  else if (addedCourse || create)
    selectCourse(gdi, 0);
  else
    selectCourse(gdi, pc);
}

int TabCourse::courseCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Save") {
      save(gdi, 1);
    }
    else if (bi.id == "SwitchMode") {
      if (!tableMode)
        save(gdi, true);
      tableMode = !tableMode;
      loadPage(gdi);
    }
    else if (bi.id == "LegLengths") {
      save(gdi, 2);
      
      pCourse pc = oe->getCourse(courseId);
      if (!pc || pc->getNumControls() == 0) {
        return 0;
      }
      gdi.clearPage(false);
      gdi.addString("", boldLarge, L"Redigera sträcklängder för X#" + pc->getName());
      gdi.dropLine();
      int w = gdi.scaleLength(120);
      int xp = gdi.getCX() + w;
      int yp = gdi.getCY();
      gdi.addString("", 1, "Sträcka:");
      gdi.addString("", yp, xp, 1, "Längd:");

      for (int i = 0; i <= pc->getNumControls(); i++) {
        int len = pc->getLegLength(i);
        pControl cbegin = pc->getControl(i-1);
        wstring begin = i == 0 ? lang.tl("Start") : (cbegin ? cbegin->getName() : L"");
        pControl cend = pc->getControl(i);
        wstring end = i == pc->getNumControls() ? lang.tl("Mål") : (cend ? cend->getName() : L"");
        gdi.pushX();
        gdi.fillRight();
        gdi.addStringUT(0, begin + makeDash(L" - ") + end + L":").xlimit = w-10;
        gdi.setCX(xp);
        gdi.fillDown();
        gdi.addInput("c" + itos(i), len > 0 ? itow(len) : L"", 8);
        gdi.popX();
        if (i < pc->getNumControls()) {
          RECT rc;
          rc.left = gdi.getCX() + gdi.getLineHeight();
          rc.right = rc.left + (3*w)/2;
          rc.top = gdi.getCY() + 2;
          rc.bottom = gdi.getCY() + 4;
          gdi.addRectangle(rc, colorDarkBlue, false);
        }
      }

      gdi.dropLine();
      gdi.fillRight();
      gdi.addButton("Cancel", "Avbryt", CourseCB).setCancel();
      gdi.addButton("SaveLegLen", "Spara", CourseCB).setDefault();
      gdi.setOnClearCb(CourseCB);
      gdi.setData("EditLengths", 1);
      gdi.refresh();
    }
    else if (bi.id == "SaveLegLen") {
      saveLegLengths(gdi);
      loadPage(gdi);
    }
    else if (bi.id=="BrowseCourse") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Alla banfiler", L"*.xml;*.csv;*.txt"));
      ext.push_back(make_pair(L"Banor, OCAD semikolonseparerat", L"*.csv;*.txt"));
      ext.push_back(make_pair(L"Banor, IOF (xml)", L"*.xml"));

      wstring file=gdi.browseForOpen(ext, L"csv");

      if (file.length()>0)
        gdi.setText("FileName", file);
    }
    else if (bi.id=="Print") {
      gdi.print(oe);
    }
    else if (bi.id=="PDF") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Portable Document Format (PDF)", L"*.pdf"));

      int index;
      wstring file=gdi.browseForSave(ext, L"pdf", index);

      if (!file.empty()) {
        pdfwriter pdf;
        pdf.generatePDF(gdi, file, L"Report", L"MeOS", gdi.getTL(), true);
        gdi.openDoc(file.c_str());
      }
    }
    else if (bi.id == "WithLoops") {
      bool w = gdi.isChecked(bi.id);
      gdi.setInputStatus("CommonControl", w);
      if (w && gdi.getTextNo("CommonControl") == 0)
        gdi.selectFirstItem("CommonControl");

      pCourse pc = oe->getCourse(courseId);
      if (pc) {
        pair<int, bool> sel = gdi.getSelectedItem("ShortCourse");
        fillOtherCourses(gdi, *pc, w);
        if (!w && sel.first == 0)
          sel.second = false;

        if (sel.second) {
          gdi.selectItemByData("ShortCourse", sel.first);
        }
        else if (w) {
          gdi.selectItemByData("ShortCourse", 0);
        }
      }
    }
    else if (bi.id == "Shorten") {
      bool w = gdi.isChecked(bi.id);
      gdi.setInputStatus("ShortCourse", w);
      if (w) {
        ListBoxInfo clb;
        if (!gdi.getSelectedItem("ShortCoursse", clb) || clb.data <= 0)
          gdi.selectFirstItem("CommonControl");
      }
    }
    else if (bi.id == "ExportCourses") {
      int FilterIndex=0;
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"IOF CourseData, version 3.0 (xml)", L"*.xml"));
      wstring save = gdi.browseForSave(ext, L"xml", FilterIndex);
      if (save.length()>0) {
        IOF30Interface iof30(oe, false);
        xmlparser xml;
        xml.openOutput(save.c_str(), false);
        iof30.writeCourses(xml);
        xml.closeOut();
      }
    }
    else if (bi.id=="ImportCourses") {
      setupCourseImport(gdi, CourseCB);
    }
    else if (bi.id=="DoImportCourse") {
      wstring filename = gdi.getText("FileName");
      if (filename.empty())
        return 0;
      gdi.disableInput("DoImportCourse");
      gdi.disableInput("Cancel");
      gdi.disableInput("BrowseCourse");
      gdi.disableInput("AddClasses");

      try {
        TabCourse::runCourseImport(gdi, filename, oe, gdi.isChecked("AddClasses"));
      }
      catch (std::exception &) {
        gdi.enableInput("DoImportCourse");
        gdi.enableInput("Cancel");
        gdi.enableInput("BrowseCourse");
        gdi.enableInput("AddClasses");
        throw;
      }
      gdi.addButton("Cancel", "OK", CourseCB);
      gdi.dropLine();
      gdi.refresh();
    }
    else if (bi.id == "DrawCourse") {
      save(gdi, true);
      pCourse crs = oe->getCourse(courseId);
      if (crs == 0)
        throw meosException("Ingen bana vald.");
      vector<pClass> cls;
      oe->getClasses(cls, true);
      wstring clsNames;
      bool hasAsked = false;
      courseDrawClasses.clear();
      for (size_t k = 0; k < cls.size(); k++) {
        if (cls[k]->getCourseId() != courseId)
          continue;
        if (!hasAsked &&oe->classHasResults(cls[k]->getId())) {
          hasAsked = true;
          if (!gdi.ask(L"warning:drawresult"))
            return 0;
        }
        courseDrawClasses.emplace_back(cls[k]->getId(), 0, -1, -1, 0, oEvent::VacantPosition::Mixed);
        if (!clsNames.empty())
          clsNames += L", ";
        clsNames += cls[k]->getName();
      }
      if (courseDrawClasses.empty())
        throw meosException("Ingen klass använder banan.");

      gdi.clearPage(false);
      gdi.addString("", boldLarge, L"Lotta klasser med banan X#" + crs->getName());
      gdi.addStringUT(0, clsNames);
      gdi.dropLine();
      gdi.pushX();

      gdi.fillRight();
      int firstStart = 3600;
      int interval = 2*60;
      int vac = 1;
      gdi.addInput("FirstStart", oe->getAbsTime(firstStart), 10, 0, L"Första start:");
      gdi.addInput("Interval", formatTime(interval), 10, 0, L"Startintervall (min):");
      gdi.addInput("Vacances", itow(vac), 10, 0, L"Antal vakanser:");
      gdi.fillDown();
      gdi.popX();
      gdi.dropLine(3);
      gdi.addSelection("Method", 200, 200, 0, L"Metod:");
      gdi.addItem("Method", lang.tl("Lottning") + L" (MeOS)", int(oEvent::DrawMethod::MeOS));
      gdi.addItem("Method", lang.tl("Lottning"), int(oEvent::DrawMethod::Random));
      gdi.addItem("Method", lang.tl("SOFT-lottning"), int(oEvent::DrawMethod::SOFT));

      gdi.selectItemByData("Method", (int)getDefaultMethod());
      gdi.dropLine(0.9);
      gdi.fillRight();
      gdi.addButton("DoDrawCourse", "Lotta", CourseCB).setDefault();
      gdi.addButton("Cancel", "Avbryt", CourseCB).setCancel();
      gdi.dropLine();
      gdi.fillDown();
      gdi.refresh();
    }
    else if (bi.id == "DoDrawCourse") {
      wstring firstStart = gdi.getText("FirstStart");
      wstring minInterval = gdi.getText("Interval");
      int vacances = gdi.getTextNo("Vacances");
      int fs = oe->getRelativeTime(firstStart);
      int iv = convertAbsoluteTimeMS(minInterval);
      oEvent::DrawMethod method = oEvent::DrawMethod(gdi.getSelectedItem("Method").first);
      courseDrawClasses[0].firstStart = fs;
      courseDrawClasses[0].vacances = vacances;
      courseDrawClasses[0].interval = iv;

      for (size_t k = 1; k < courseDrawClasses.size(); k++) {
        vector<pRunner> r;
        oe->getRunners(courseDrawClasses[k-1].classID, 0, r, false);
        int vacDelta = vacances;
        for (size_t i = 0; i < r.size(); i++) {
          if (r[i]->isVacant()) 
            vacDelta--;
        }

        courseDrawClasses[k].firstStart = courseDrawClasses[k-1].firstStart + (r.size() + vacDelta) * iv;
        courseDrawClasses[k].vacances = vacances;
        courseDrawClasses[k].interval = iv;
      }

      oe->drawList(courseDrawClasses, method, 1, oEvent::DrawType::DrawAll); 

      oe->addAutoBib();

      gdi.clearPage(false);
      gdi.addButton("Cancel", "Återgå", CourseCB);

      oListParam par;
      oListInfo info;
      par.listCode = EStdStartList;
      for (size_t k=0; k<courseDrawClasses.size(); k++)
        par.selection.insert(courseDrawClasses[k].classID);

      oe->generateListInfo(par, info);
      oe->generateList(gdi, false, info, true);
      gdi.refresh();
    }
    else if (bi.id=="Add") {
      if (courseId>0) {
        wstring ctrl = gdi.getText("Controls");
        wstring name = gdi.getText("Name");
        pCourse pc = oe->getCourse(courseId);
        if (pc && !name.empty() && !ctrl.empty() &&  pc->getControlsUI() != ctrl) {
          if (name == pc->getName()) {
            // Make name unique if same name
            int len = name.length();
            if (len > 2 && (isdigit(name[len-1]) || isdigit(name[len-2]))) {
              ++name[len-1]; // course 1 ->  course 2, course 1a -> course 1b
            }
            else
              name += L" 2";
          }
          if (gdi.ask(L"Vill du lägga till banan 'X' (Y)?#" + name + L"#" + ctrl)) {
            pc = oe->addCourse(name);
            courseId = pc->getId();
            gdi.setText("Name", name);
            save(gdi, 1);
            return true;
          }
        }
        save(gdi, 1);
      }
      pCourse pc = oe->addCourse(oe->getAutoCourseName());
      pc->synchronize();
      oe->fillCourses(gdi, "Courses");
      selectCourse(gdi, pc);
      gdi.setInputFocus("Name", true);
      addedCourse = true;
    }
    else if (bi.id=="Remove"){
      DWORD cid = courseId;
      if (cid==0)
        throw meosException("Ingen bana vald.");

      if (oe->isCourseUsed(cid))
        gdi.alert("Banan används och kan inte tas bort.");
      else
        oe->removeCourse(cid);

      oe->fillCourses(gdi, "Courses");

      selectCourse(gdi, 0);
    }
    else if (bi.id == "FirstAsStart" || bi.id == "LastAsFinish") {
      refreshCourse(gdi.getText("Controls"), gdi);
    }
    else if (bi.id=="Cancel"){
      LoadPage("Banor");
    }
  }
  else if (type==GUI_LISTBOX){
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Courses") {
      if (gdi.isInputChanged(""))
        save(gdi, 0);

      pCourse pc=oe->getCourse(bi.data);
      selectCourse(gdi, pc);
      addedCourse = false;
    }
    else if (bi.id=="Rogaining") {
      wstring t;
      t = gdi.getText("TimeLimit");
      if (!t.empty() && _wtoi(t.c_str()) > 0)
        time_limit = t;
      t = gdi.getText("PointLimit");
      if (!t.empty())
        point_limit = t;
      t = gdi.getText("PointReduction");
      if (!t.empty())
        point_reduction = t;

      wstring tl, pl, pr;
      if (bi.data == 1) {
        tl = time_limit;
        pr = point_reduction;
      }
      else if (bi.data == 2) {
        pl = point_limit;
      }

      gdi.setInputStatus("TimeLimit", !tl.empty());
      gdi.setText("TimeLimit", tl);

      gdi.setInputStatus("PointLimit", !pl.empty());
      gdi.setText("PointLimit", pl);

      gdi.setInputStatus("PointReduction", !pr.empty());
      gdi.setInputStatus("ReductionPerMinute", !pr.empty());
      gdi.setText("PointReduction", pr);

    }
  }
  else if (type == GUI_INPUT) {
    InputInfo ii=*(InputInfo *)data;
    if (ii.id == "Controls") {
      int current = gdi.getTextNo("CommonControl");
      fillCourseControls(gdi, ii.text);
      if (gdi.isChecked("WithLoops") && current != 0)
        gdi.selectItemByData("CommonControl", current);
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo &ii=*(InputInfo *)data;
     
    if (ii.id == "Controls") {
      refreshCourse(ii.text, gdi);
    }
  }
  else if (type==GUI_CLEAR) {
    if (gdi.hasData("EditLengths")) {
      saveLegLengths(gdi);
      return true;
    }
    if (courseId>0)
      save(gdi, 0);

    return true;
  }
  return 0;
}

bool TabCourse::loadPage(gdioutput &gdi) {
  oe->checkDB();
  gdi.selectTab(tabId);

  DWORD ClassID=0, RunnerID=0;

  time_limit = L"01:00:00";
  point_limit = L"10";
  point_reduction = L"1";

  gdi.getData("ClassID", ClassID);
  gdi.getData("RunnerID", RunnerID);

  gdi.clearPage(false);
  int xp = gdi.getCX();

  gdi.setData("ClassID", ClassID);
  gdi.setData("RunnerID", RunnerID);

  string switchMode;
  const int button_w = gdi.scaleLength(90);
  switchMode = tableMode ? "Formulärläge" : "Tabelläge";
  gdi.addButton(2, 2, button_w, "SwitchMode", switchMode,
                CourseCB, "Välj vy", false, false).fixedCorner();

  if (tableMode) {
    gdi.addTable(oCourse::getTable(oe), xp, gdi.scaleLength(30));
    return true;
  }

  gdi.addString("", boldLarge, "Banor");
  gdi.pushY();

  gdi.fillDown();
  gdi.addListBox("Courses", 250, 360, CourseCB, L"Banor (antal kontroller)").isEdit(false).ignore(true);
  gdi.setTabStops("Courses", 240);

  oe->fillCourses(gdi, "Courses");

  gdi.dropLine(0.7);
  gdi.pushX();
  gdi.addString("", boldText, "Funktioner");
  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("ImportCourses", "Importera från fil...", CourseCB);
  gdi.addButton("ExportCourses", "Exportera...", CourseCB);
  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("DrawCourse", "Lotta starttider..", CourseCB);
  gdi.disableInput("DrawCourse");
  gdi.newColumn();
  gdi.fillDown();

  gdi.popY();

  gdi.addString("", 10, "help:25041");

  gdi.dropLine(0.5);
  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("Name", L"", 20, 0, L"Namn:");
  gdi.fillDown();
  gdi.addInput("NumberMaps", L"", 6, 0, L"Antal kartor:");
  
  gdi.popX();

  vector<pCourse> allCrs;
  oe->getCourses(allCrs);
  size_t mlen = 0;
  for (size_t k = 0; k < allCrs.size(); k++) {
    mlen = max(allCrs[k]->getControlsUI().length()/2+5, mlen);
  }

  gdi.addInput("Controls", L"", max(48u, mlen), CourseCB, L"Kontroller:");
  gdi.dropLine(0.3);
  gdi.addString("CourseExpanded", 0, "...").setColor(colorDarkGreen);
  gdi.dropLine(0.5);
  gdi.addString("", 10, "help:12662");
  gdi.dropLine(1);

  gdi.fillRight();
  gdi.addInput("Climb", L"", 8, 0, L"Climb (m):");
  gdi.addInput("Length", L"", 8, 0, L"Längd (m):");
  gdi.dropLine(0.9);
  gdi.fillDown();
  gdi.addButton("LegLengths", "Redigera sträcklängder...", CourseCB).isEdit(true);
  gdi.dropLine(0.5);
  gdi.popX();

  gdi.fillRight();
  gdi.addCheckbox("FirstAsStart", "Använd första kontrollen som start", CourseCB);
  gdi.fillDown();
  gdi.addCheckbox("LastAsFinish", "Använd sista kontrollen som mål", CourseCB);
  gdi.popX();

  gdi.fillRight();
  gdi.addCheckbox("WithLoops", "Bana med slingor", CourseCB);
  gdi.setCX(gdi.getCX()+ gdi.scaleLength(20));
  gdi.addString("", 0, "Varvningskontroll:");
  gdi.fillDown();
  gdi.dropLine(-0.2);
  gdi.addSelection("CommonControl", 50, 200, 0, L"", L"En bana med slingor tillåter deltagaren att ta slingorna i valfri ordning");

  gdi.dropLine(0.2);
  gdi.popX();

  gdi.fillRight();
  gdi.addCheckbox("Shorten", "Med avkortning", CourseCB);
  gdi.setCX(gdi.getCX()+ gdi.scaleLength(20));
  gdi.addString("", 0, "Avkortad banvariant:");
  gdi.dropLine(-0.2);
  gdi.addSelection("ShortCourse", 150, 200, 0, L"", L"info_shortening");
  gdi.addString("Shortens", 0, "");

  gdi.fillDown();
  
  gdi.dropLine(2.5);
  gdi.popX();

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Rogaining)) {
    RECT rc;
    rc.top = gdi.getCY() -5;
    rc.left = gdi.getCX();
    gdi.setCX(gdi.getCX()+gdi.scaleLength(10));

    gdi.addString("", 1, "Rogaining");
    gdi.dropLine(0.5);
    gdi.fillRight();
    gdi.addSelection("Rogaining", 120, 80, CourseCB);
    gdi.addItem("Rogaining", lang.tl("Ingen rogaining"), 0);
    gdi.addItem("Rogaining", lang.tl("Tidsgräns"), 1);
    gdi.addItem("Rogaining", lang.tl("Poänggräns"), 2);

    gdi.setCX(gdi.getCX()+gdi.scaleLength(20));
    gdi.dropLine(-0.8);
    int cx = gdi.getCX();
    gdi.addInput("PointLimit", L"", 8, 0, L"Poänggräns:").isEdit(false);
    gdi.addInput("TimeLimit", L"", 8, 0, L"Tidsgräns:").isEdit(false);
    gdi.addInput("PointReduction", L"", 8, 0, L"Poängavdrag (per minut):").isEdit(false);
    gdi.dropLine(3.5);
    rc.right = gdi.getCX() + 5;
    gdi.setCX(cx);
    gdi.fillDown();
    gdi.addCheckbox("ReductionPerMinute", "Poängavdrag per påbörjad minut");

    rc.bottom = gdi.getCY() + 5;
    gdi.addRectangle(rc, colorLightBlue, true);
  }

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(1);
  gdi.addString("CourseUse", 0, "").setColor(colorDarkBlue);
  gdi.dropLine();
  gdi.addString("CourseProblem", 1, "").setColor(colorRed);
  gdi.dropLine(2);
  gdi.fillRight();
  gdi.addButton("Save", "Spara", CourseCB, "help:save").setDefault();
  gdi.addButton("Remove", "Radera", CourseCB);
  gdi.addButton("Add", "Ny bana", CourseCB);
  gdi.disableInput("Remove");
  gdi.disableInput("Save");

  selectCourse(gdi, oe->getCourse(courseId));
  gdi.setOnClearCb(CourseCB);

  gdi.refresh();

  return true;
}

void TabCourse::runCourseImport(gdioutput& gdi, const wstring &filename,
                                oEvent *oe, bool addClasses) {
  if (csvparser::iscsv(filename)  != csvparser::CSV::NoCSV) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, "Importerar OCAD csv-fil...");
    gdi.refreshFast();
    csvparser csv;
    if (csv.importOCAD_CSV(*oe, filename, addClasses)) {
      gdi.addString("", 1, "Klart.").setColor(colorGreen);
    }
    else gdi.addString("", 0, "Operationen misslyckades.").setColor(colorRed);
    gdi.popX();
    gdi.dropLine(2.5);
    gdi.fillDown();
  }
  else {
    set<int> noFilter;
    string noType;
    oe->importXML_EntryData(gdi, filename.c_str(), addClasses, false, noFilter, noType);
  }
  if (addClasses) {
    // There is specific course-class matching inside the import of each format,
    // that uses additional information. Here we try to match based on a generic approach.
    vector<pClass> cls;
    vector<pCourse> crs;
    oe->getClasses(cls, false);
    oe->getCourses(crs);

    map<wstring, pCourse> name2Course;
    map<int, vector<pClass> > course2Class;
    for (size_t k = 0; k < crs.size(); k++)
      name2Course[crs[k]->getName()] = crs[k];
    bool hasMissing = false;
    for (size_t k = 0; k < cls.size(); k++) {
      vector<pCourse> usedCrs;
      cls[k]->getCourses(-1, usedCrs);

      if (usedCrs.empty()) {
        map<wstring, pCourse>::iterator res = name2Course.find(cls[k]->getName());
        if (res != name2Course.end()) {
          usedCrs.push_back(res->second);
          if (cls[k]->getNumStages()==0) {
            cls[k]->setCourse(res->second);
          }
          else {
            for (size_t i = 0; i<cls[k]->getNumStages(); i++)
              cls[k]->addStageCourse(i, res->second->getId(), -1);
          }
        }
        else {
          hasMissing = true;
        }
      }

      for (size_t j = 0; j < usedCrs.size(); j++) {
        course2Class[usedCrs[j]->getId()].push_back(cls[k]);
      }
    }

    for (size_t k = 0; k < crs.size(); k++) {
      pClass bestClass;
      if (hasMissing && (bestClass = oe->getBestClassMatch(crs[k]->getName())) != 0) {
        vector<pCourse> usedCrs;
        bestClass->getCourses(-1, usedCrs);
        if (usedCrs.empty()) {
          course2Class[crs[k]->getId()].push_back(bestClass);
          if (bestClass->getNumStages()==0) {
            bestClass->setCourse(crs[k]);
          }
          else {
            for (size_t i = 0; i<bestClass->getNumStages(); i++)
              bestClass->addStageCourse(i, crs[k]->getId(), -1);
          }
        }
      }
    }

    gdi.addString("", 1, "Klasser");
    int yp = gdi.getCY();
    int xp = gdi.getCX();
    int w = gdi.scaleLength(200);
    for (size_t k = 0; k < cls.size(); k++) {
      vector<pCourse> usedCrs;
      cls[k]->getCourses(-1, usedCrs);
      wstring c;
      for (size_t j = 0; j < usedCrs.size(); j++) {
        if (j>0)
          c += L", ";
        c += usedCrs[j]->getName();
      }
      TextInfo &ci = gdi.addStringUT(yp, xp, 0, cls[k]->getName(), w);
      if (c.empty()) {
        c = makeDash(L"-");
        ci.setColor(colorRed);
      }
      gdi.addStringUT(yp, xp + w, 0, c);
      yp += gdi.getLineHeight();
    }

    gdi.dropLine();
    gdi.addString("", 1, "Banor");
    yp = gdi.getCY();
    for (size_t k = 0; k < crs.size(); k++) {
      wstring c;
      vector<pClass> usedCls = course2Class[crs[k]->getId()];
      for (size_t j = 0; j < usedCls.size(); j++) {
        if (j>0)
          c += L", ";
        c += usedCls[j]->getName();
      }
      TextInfo &ci = gdi.addStringUT(yp, xp, 0, crs[k]->getName(), w);
      if (c.empty()) {
        c = makeDash(L"-");
        ci.setColor(colorRed);
      }
      gdi.addStringUT(yp, xp + w, 0, c);
      yp += gdi.getLineHeight();
    }
    gdi.dropLine();
  }

  gdi.addButton(gdi.getWidth()+20, 45,  gdi.scaleLength(baseButtonWidth),
                "Print", "Skriv ut...", CourseCB,
                "Skriv ut listan.", true, false);
  gdi.addButton(gdi.getWidth()+20, 75,  gdi.scaleLength(baseButtonWidth),
                "PDF", "PDF...", CourseCB,
                "Spara som PDF.", true, false);

  gdi.setWindowTitle(oe->getTitleName());
  oe->updateTabs();
  gdi.refresh();
}

void TabCourse::setupCourseImport(gdioutput& gdi, GUICALLBACK cb) {

  gdi.clearPage(true);
  gdi.addString("", 2, "Importera banor/klasser");
  gdi.addString("", 0, "help:importcourse");
  gdi.dropLine();

  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("FileName", L"", 48, 0, L"Filnamn:");
  gdi.dropLine();
  gdi.fillDown();
  gdi.addButton("BrowseCourse", "Bläddra...", CourseCB);

  gdi.dropLine(0.5);
  gdi.popX();

  gdi.fillDown();
  gdi.addCheckbox("AddClasses", "Lägg till klasser", 0, true);

  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("DoImportCourse", "Importera", cb).setDefault();
  gdi.fillDown();
  gdi.addButton("Cancel", "Avbryt", cb).setCancel();
  gdi.setInputFocus("FileName");
  gdi.popX();
}

void TabCourse::fillCourseControls(gdioutput &gdi, const wstring &ctrl) {
  vector<int> nr;
  oCourse::splitControls(gdi.narrow(ctrl), nr);

  vector< pair<wstring, size_t> > item;
  map<int, int> used;
  for (size_t k = 0; k < nr.size(); k++) {
    pControl pc = oe->getControl(nr[k], false);
    if (pc) {
      if (pc->getStatus() == oControl::StatusOK)
        ++used[pc->getFirstNumber()];
    }
    else
      ++used[nr[k]];
  }

  set<int> added;
  for (int i = 10; i > 0; i--) {
    for (map<int, int>::iterator it = used.begin(); it != used.end(); ++it) {
      if (it->second >= i && !added.count(it->first)) {
        added.insert(it->first);
        item.push_back(make_pair(itow(it->first), it->first));
      }
    }
  }

  gdi.clearList("CommonControl");
  gdi.addItem("CommonControl", item);
}

void TabCourse::fillOtherCourses(gdioutput &gdi, oCourse &crs, bool withLoops) {
  vector< pair<wstring, size_t> > ac;
  oe->fillCourses(ac, true);
  set<int> skipped;
  skipped.insert(crs.getId());
  pCourse longer = crs.getLongerVersion();
  int iter = 20;
  while (longer && --iter>0) {
    skipped.insert(longer->getId());
    longer = longer->getLongerVersion();
  }
  
  vector< pair<wstring, size_t> > out;
  if (withLoops)
    out.emplace_back(lang.tl("Färre slingor"), 0);
  
  for (size_t k = 0; k < ac.size(); k++) {
    if (!skipped.count(ac[k].second))
      out.push_back(ac[k]);
  }

  gdi.clearList("ShortCourse");
  gdi.addItem("ShortCourse", out);
}

void TabCourse::saveLegLengths(gdioutput &gdi) {
  pCourse pc = oe->getCourse(courseId);
  if (!pc) 
    return;
      
  pc->synchronize(false);
  wstring lstr;
  bool gotAny = false;
  for (int i = 0; i <= pc->getNumControls(); i++) {
    wstring t = trim(gdi.getText("c" + itos(i)));
    if (t.empty())
      t = L"0";
    else
      gotAny = true;

    if (i == 0)
      lstr = t;
    else
      lstr += L";" + t;
  }
  if (!gotAny)
    lstr = L"";
        
  pc->importLegLengths(gdi.narrow(lstr), true);
  pc->synchronize(true);
}

oEvent::DrawMethod TabCourse::getDefaultMethod() const {
  int dm = oe->getPropertyInt("DefaultDrawMethod", int(oEvent::DrawMethod::MeOS));
  if (dm == (int)oEvent::DrawMethod::Random)
    return oEvent::DrawMethod::Random;
  if (dm == (int)oEvent::DrawMethod::MeOS)
    return oEvent::DrawMethod::MeOS;
  else
    return oEvent::DrawMethod::SOFT;
}

void TabCourse::clearCompetitionData() {
  courseId = 0;
  addedCourse = false;
  tableMode = false;
}

void TabCourse::refreshCourse(const wstring &text, gdioutput &gdi) {
  bool firstAsStart = gdi.isChecked("FirstAsStart");
  bool lastAsFinish = gdi.isChecked("LastAsFinish");
  bool rogaining = gdi.hasWidget("Rogaining") && gdi.getSelectedItem("Rogaining").first == 1;
  
  wstring controls = encodeCourse(text, rogaining, firstAsStart, lastAsFinish);

  if (controls != gdi.getText("CourseExpanded"))
    gdi.setText("CourseExpanded", controls, true);
}

wstring TabCourse::encodeCourse(const wstring &in, bool rogaining, bool firstStart, bool lastFinish) {
  vector<int> newC;
  string ins;
  wide2String(in, ins);
  oCourse::splitControls(ins, newC);
  wstring dash = makeDash(L"-");
  wstring out;
  out.reserve(in.length() * 2);
  wstring bf;

  if (!rogaining) {
    for (size_t i = 0; i < newC.size(); ++i) {
      if (i == 0 && (newC.size() > 1 || firstStart)) {
        out += lang.tl("Start");
        if (firstStart)
          out += L"(" + itow(newC[i]) + L")";
        else
          out += dash + formatControl(newC[i], bf);

        if (newC.size() == 1) {
          out += dash + lang.tl("Mål");
          break;
        }
        continue;
      }
      else
        out += dash;

      if (i + 1 == newC.size()) {
        if (i == 0) {
          out = lang.tl("Start") + dash;
        }
        if (lastFinish)
          out += lang.tl("Mål") + L"(" + itow(newC[i]) + L")";
        else
          out += formatControl(newC[i], bf) + dash + lang.tl("Mål");
      }
      else {
        out += formatControl(newC[i], bf);
      }
    }
  }
  else {
    int pcnt = 0;
    for (size_t i = 0; i < newC.size(); ++i) {
      if (i > 0)
        out += L"; ";
      auto pc = oe->getControl(newC[i]);
      if (pc)
        pcnt += pc->getRogainingPoints();

      out += formatControl(newC[i], bf);
    }

    if (pcnt > 0)
      out += L" = " + itow(pcnt) + L"p";
  }
  return out;
}

const wstring &TabCourse::formatControl(int id, wstring &bf) const {
  pControl ctrl = oe->getControl(id, false);
  if (ctrl) {
    bf = ctrl->getString();
    return bf;
  }
  else
    return itow(id);
}
