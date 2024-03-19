/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meos_util.h"
#include "TabCompetition.h"
#include "localizer.h"
#include "meosException.h"
#include "MeOSFeatures.h"
#include "importformats.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <io.h>
#include "csvparser.h"

int CompetitionCB(gdioutput *gdi, GuiEventType type, BaseInfo* data);

int NewGuideCB(gdioutput *gdi, GuiEventType type, BaseInfo* data) {
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));
  return tc.newGuideCB(*gdi, type, data);
}

int TabCompetition::newGuideCB(gdioutput &gdi, GuiEventType type, BaseInfo* data) {
  if (type == GUI_LINK) {
    TextInfo ti = *(TextInfo *)data;
    if (ti.id == "link") {
    }
  }
  else if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id == "ImportEntries") {
      newCompetitionGuide(gdi, 1);
    }
    else if (bi.id == "BasicSetup") {
      gdi.restore("entrychoice");
      entryChoice(gdi);
      gdi.refresh();
    }
    else if (bi.id == "DoImportEntries") {
      auto baseOnId = gdi.getSelectedItem("CmpSel");
      createCompetition(gdi);

      try {
        gdi.autoRefresh(true);
        FlowOperation res = saveEntries(gdi, false, 0, true);
        if (res != FlowContinue) {
          if (res == FlowCancel)
            newCompetitionGuide(gdi, 1);
          return 0;
        }
      }
      catch (std::exception &) {
        newCompetitionGuide(gdi, 1);
        throw;
      }
      gdi.restore("newcmp");
      gdi.setCX(gdi.getCX() + gdi.getLineHeight());
      gdi.pushX();
      gdi.setRestorePoint("entrychoice");

      if (!baseOnId.second || baseOnId.first <= 0)
        newCompetitionGuide(gdi, 2);
      else
        createNewCmp(gdi, true);
    }
    else if (bi.id == "NoEntries") {
      gdi.restore("entrychoice");
      auto baseOnId = gdi.getSelectedItem("CmpSel");
      if (baseOnId.second && baseOnId.first <= 0)
        newCompetitionGuide(gdi, 2);
      else
        createNewCmp(gdi, false);
    }
    else if (bi.id == "Cancel") {
      oe->clear();
      oe->updateTabs(true, false);
      loadPage(gdi);
    }
    else if (bi.id == "FAll") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);

      oe->getMeOSFeatures().useAll(*oe);
      createNewCmp(gdi, true);
    }
    else if (bi.id == "FBasic") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      oe->getMeOSFeatures().clear(*oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Clubs, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::RunnerDb, true, *oe);
     
      createNewCmp(gdi, true);
    }
    else if (bi.id == "FSelect") {
      newCompetitionGuide(gdi, 3);
    }
    else if (bi.id == "StoreFeatures") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      saveMeosFeatures(gdi, true);
      createNewCmp(gdi, true);
    }
    else if (bi.id == "FIndividual") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Speaker, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Economy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::EditClub, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Network, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Vacancy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::InForest, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::DrawStartList, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Bib, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::RunnerDb, true, *oe);

      createNewCmp(gdi, true);
    }
    else if (bi.id == "FNoCourses" || bi.id == "FNoCoursesRelay") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Speaker, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Economy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::EditClub, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Network, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Vacancy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::DrawStartList, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Bib, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::RunnerDb, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::NoCourses, true, *oe);

      if (bi.id == "FNoCoursesRelay")
        oe->getMeOSFeatures().useFeature(MeOSFeatures::Relay, true, *oe);

      createNewCmp(gdi, true);
    }
    else if (bi.id == "FForked") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Speaker, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Economy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::EditClub, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Network, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Vacancy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::InForest, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::DrawStartList, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Bib, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::RunnerDb, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::ForkedIndividual, true, *oe);

      createNewCmp(gdi, true);
    }
    else if (bi.id == "FTeam") {
      if (gdi.hasWidget("Name"))
        createCompetition(gdi);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Speaker, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Economy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::EditClub, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Network, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Vacancy, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::InForest, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::DrawStartList, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Bib, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::RunnerDb, true, *oe);
      oe->getMeOSFeatures().useFeature(MeOSFeatures::Relay, true, *oe);

      if (oe->hasMultiRunner())
        oe->getMeOSFeatures().useFeature(MeOSFeatures::MultipleRaces, true, *oe);

      createNewCmp(gdi, true);
    }
  }
  else if (type == GUI_INPUT) {
    InputInfo &ii = *(InputInfo*)data;
    if (ii.id == "FirstStart" || ii.id == "Date") {
      int t,d;
      SYSTEMTIME st;
      if (ii.id == "FirstStart") {
        t = convertAbsoluteTimeHMS(ii.text, -1);
        d = convertDateYMD(gdi.getText("Date"), st, true);
        ii.setBgColor(t == -1 ? colorLightRed: colorDefault);
      }
      else {
        t = convertAbsoluteTimeHMS(gdi.getText("FirstStart"), -1);
        d = convertDateYMD(ii.text, st, true);
        ii.setBgColor(d <= 0 ? colorLightRed: colorDefault);
      }

      if (t <= 0 || d <= 0) {
        gdi.setTextTranslate("AllowedInterval", L"Felaktigt datum/tid", true);
      }
      else {
        long long absT = SystemTimeToInt64TenthSecond(st);
        absT += max(0, t - timeConstHour);
        long long stopT = absT + 23 * timeConstHour;
        SYSTEMTIME start = Int64TenthSecondToSystemTime(absT);
        SYSTEMTIME end = Int64TenthSecondToSystemTime(stopT);
        wstring s = L"Tävlingen måste avgöras mellan X och Y.#" + convertSystemTime(start) + L"#" + convertSystemTime(end);
        gdi.setTextTranslate("AllowedInterval", s, true);
      }
    }

  }
  return 0;
}

void TabCompetition::newCompetitionGuide(gdioutput &gdi, int step) {
  static RECT rc;
  const int width = 600;
  if (step == 0) {
    oe->updateTabs(true, true);
    gdi.clearPage(false);
    gdi.addString("", boldLarge, "Ny tävling");
    gdi.dropLine();
    gdi.setRestorePoint("newcmp");

    rc.top = gdi.getCY();
    rc.left = gdi.getCX();
    gdi.dropLine();

    gdi.setCX(gdi.getCX() + gdi.getLineHeight());

    gdi.addString("", fontMediumPlus, "Namn och tidpunkt");

    gdi.dropLine(0.5);
    gdi.addInput("Name", lang.tl("Ny tävling"), 34, 0, L"Tävlingens namn:");

    gdi.pushX();
    gdi.fillRight();
    InputInfo &date = gdi.addInput("Date", getLocalDate(), 16, NewGuideCB, L"Datum (för första start):");

    gdi.addInput("FirstStart", L"07:00:00", 12, NewGuideCB, L"Första tillåtna starttid:");

    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(3.5);
    gdi.addString("AllowedInterval", 0, "");
    newGuideCB(gdi, GUI_INPUT, &date);
    gdi.dropLine(2);

    gdi.addSelection("CmpSel", 300, 400, CompetitionCB, L"Basera på en tidigare tävling:");
    gdi.addItem("CmpSel", lang.tl("Ingen[competition]"), -1);
    oe->fillCompetitions(gdi, "CmpSel", 0, L"", false);
    gdi.autoGrow("CmpSel");
    gdi.selectFirstItem("CmpSel");

    rc.right = rc.left + gdi.scaleLength(width);
    rc.bottom = gdi.getCY();
    gdi.addRectangle(rc, colorLightBlue, true);
    gdi.dropLine();
    gdi.setRestorePoint("entrychoice");
    entryChoice(gdi);
    gdi.refresh();
  }
  else if (step == 1) {
    gdi.restore("entrychoice");
    rc.top = gdi.getCY();
    gdi.fillDown();
    entryForm(gdi, true);
    rc.bottom = gdi.getCY();
    gdi.dropLine();
    gdi.addRectangle(rc, colorLightBlue, true);

    gdi.fillRight();
    gdi.addButton("BasicSetup", "<< Bakåt", NewGuideCB);
    gdi.addButton("DoImportEntries", "Importera", NewGuideCB);
    gdi.addButton("Cancel", "Avbryt", NewGuideCB).setCancel();

    gdi.popX();
    gdi.dropLine(2);
    gdi.fillDown();
    gdi.scrollToBottom();
    gdi.refresh();
  }
  else if (step == 2) {
    rc.top = gdi.getCY();
    oe->updateTabs(true, true);
    gdi.fillDown();
    gdi.dropLine();
    gdi.addString("", fontMediumPlus, "Funktioner i MeOS");
    gdi.dropLine(0.5);
    gdi.addString("", 10, "newcmp:featuredesc");
    gdi.dropLine();
    gdi.addString("", 1, "Välj vilka funktioner du vill använda");
    gdi.dropLine(0.5);
    gdi.fillRight();
    gdi.addString("", 1, "Individuellt").setColor(colorDarkBlue);
    gdi.popX();
    gdi.dropLine(1.2);

    gdi.addButton("FIndividual", "Individuell tävling", NewGuideCB);
    gdi.addButton("FForked", "Individuellt, gafflat", NewGuideCB);

    gdi.popX();
    gdi.dropLine(2);

    gdi.addButton("FBasic", "Endast grundläggande (enklast möjligt)", NewGuideCB);
    gdi.addButton("FNoCourses", "Endast tidtagning (utan banor)", NewGuideCB);

    gdi.popX();
    gdi.dropLine(3);
    
    gdi.addString("", 1, "Lag och stafett").setColor(colorDarkBlue);
    gdi.popX();
    gdi.dropLine(1.2);

    gdi.addButton("FTeam", "Tävling med lag", NewGuideCB);
    gdi.addButton("FNoCoursesRelay", "Endast tidtagning (utan banor), stafett", NewGuideCB);

    gdi.popX();
    gdi.dropLine(3);
    
    gdi.addString("", 1, "Övrigt").setColor(colorDarkBlue);
    gdi.popX();
    gdi.dropLine(1.2);

    gdi.addButton("FAll", "Alla funktioner", NewGuideCB);
    gdi.addButton("FSelect", "Välj från lista...", NewGuideCB);
    gdi.addButton("Cancel", "Avbryt", NewGuideCB).setCancel();

    if (oe->hasTeam()) {
      gdi.disableInput("FIndividual");
      gdi.disableInput("FForked");
      gdi.disableInput("FBasic");
      gdi.disableInput("FNoCourses");
    }

    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(3);

    rc.bottom = gdi.getCY();
    gdi.dropLine();
    gdi.addRectangle(rc, colorLightBlue, true);

    gdi.refresh();
  }
  else if (step == 3) {
    gdi.restore("entrychoice");
    RECT rcl = rc;
    rcl.top = gdi.getCY();
    gdi.fillDown();
    gdi.pushX();
    meosFeatures(gdi, true);
    rcl.bottom = gdi.getHeight();
    if (gdi.getWidth() >= rc.right + gdi.scaleLength(30))
      rcl.right = gdi.getWidth();
    gdi.dropLine();
    gdi.addRectangle(rcl, colorLightBlue, true);

    gdi.popX();
    gdi.dropLine();
    gdi.setCY(gdi.getHeight());
    gdi.fillRight();
    gdi.addButton("NoEntries", "<< Bakåt", NewGuideCB);
    gdi.addButton("StoreFeatures", "Skapa tävlingen", NewGuideCB);
    gdi.addButton("Cancel", "Avbryt", NewGuideCB).setCancel();

    gdi.popX();
    gdi.dropLine(2);
    gdi.fillDown();

    gdi.refresh();
  }
}

void TabCompetition::createNewCmp(gdioutput& gdi, bool useExisting) {
  if (!useExisting)
    createCompetition(gdi);
  
  gdi.clearPage(true);
  gdi.fillRight();
  gdi.addString("", fontMediumPlus, "Skapar tävling...");
  gdi.refresh();
  gdi.setWaitCursor(true);
  Sleep(400);
  oe->updateTabs(true, false);
  gdi.setWaitCursor(false);
  loadPage(gdi);
}

void TabCompetition::entryChoice(gdioutput &gdi) {
  gdi.fillRight();
  gdi.pushX();
  gdi.addButton("ImportEntries", "Importera anmälda", NewGuideCB);
  //gdi.addButton("FreeEntry", "Fri inmatning av deltagare", NewGuideCB);
  gdi.addButton("NoEntries", "Anmäl inga deltagare nu", NewGuideCB);
  gdi.addButton("Cancel", "Avbryt", NewGuideCB).setCancel();

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(2);
}

wstring getHiredCardDefault() {
  wchar_t path[_MAX_PATH];
  getUserFile(path, L"hired_card_default.csv");
  return path;
}

void resetSaveTimer();

void TabCompetition::createCompetition(gdioutput &gdi) {
  wstring name = gdi.getText("Name");
  wstring date = gdi.getText("Date");
  wstring start = gdi.getText("FirstStart");

  int t = convertAbsoluteTimeHMS(start, -1);
  if (t > 0 && t < timeConstHour * 24) {
    t = max(0, t - timeConstHour);
  }
  else
    throw meosException("Ogiltig tid");

  int baseOnId = gdi.getSelectedItem("CmpSel").first;

  if (baseOnId <= 0) {
    oe->newCompetition(L"tmp");
    oe->loadDefaults();
  }
  else {
    openCompetition(gdi, baseOnId);
    wstring cmpCopy = getTempFile();
    oe->save(cmpCopy, false);
    wstring rawName = oe->getName();
    size_t posPar = rawName.find_first_of('(');
    if (posPar != string::npos)
      rawName = rawName.substr(0, posPar);

    if (rawName.length() > 40)
      rawName = trim(rawName.substr(0, 38)) + L"...";

    wstring oldName = trim(rawName) + L" (" + oe->getDate() + L")";
    oe->open(cmpCopy, true, false, true);
    removeTempFile(cmpCopy);
    
    oe->clearData(true, true);
    oe->setAnnotation(lang.tl(L"Baserad på X#" + oldName));
    resetSaveTimer();
  }

  oe->setName(name, true);
  oe->setDate(date, true);
  oe->setZeroTime(formatTimeHMS(t), true);

  bool importHiredCard = true;
  if (importHiredCard) 
    importDefaultHiredCards(gdi);
 }

void TabCompetition::importDefaultHiredCards(gdioutput& gdi) {
  csvparser csv;
  list<vector<wstring>> data;
  wstring fn = getHiredCardDefault();
  if (fileExists(fn)) {
    try {
      csv.parse(fn, data);
      set<int> rentCards;
      for (auto& c : data) {
        for (wstring wc : c) {
          int cn = _wtoi(wc.c_str());
          if (cn > 0) {
            oe->setHiredCard(cn, true);
          }
        }
      }
    }
    catch (const meosException& ex) {
      gdi.addString("", 0, ex.wwhat()).setColor(colorRed);
    }
    catch (std::exception& ex) {
      gdi.addString("", 0, ex.what()).setColor(colorRed);
    }
  }
}
