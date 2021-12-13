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

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "gdifonts.h"

#include "csvparser.h"
#include "SportIdent.h"
#include "meos_util.h"
#include "oListInfo.h"

#include "TabSpeaker.h"
#include "TabList.h"
#include "TabRunner.h"
#include "speakermonitor.h"
#include "meosexception.h"

#include <cassert>

//Base position for speaker buttons
#define SPEAKER_BASE_X 40
vector<string> getExtraWindows();

TabSpeaker::TabSpeaker(oEvent *poe):TabBase(poe)
{
  classId=0;
  ownWindow = false;

  lastControlToWatch = 0;
  lastClassToWatch = 0;
  watchLevel = oTimeLine::PMedium;
  watchNumber = 5;
  speakerMonitor = 0;
}

TabSpeaker::~TabSpeaker()
{
  delete speakerMonitor;
}


int tabSpeakerCB(gdioutput *gdi, int type, void *data)
{
  TabSpeaker &ts = dynamic_cast<TabSpeaker &>(*gdi->getTabs().get(TSpeakerTab));

  switch(type){
    case GUI_BUTTON: {
      //Make a copy
      ButtonInfo bu=*static_cast<ButtonInfo *>(data);
      return ts.processButton(*gdi, bu);
    }
    case GUI_LISTBOX:{
      ListBoxInfo lbi=*static_cast<ListBoxInfo *>(data);
      return ts.processListBox(*gdi, lbi);
    }
    case GUI_EVENT: {
      EventInfo ei=*static_cast<EventInfo *>(data);
      return ts.handleEvent(*gdi, ei);
    }
    case GUI_CLEAR:
      return ts.onClear(*gdi);
    case GUI_TIMEOUT:
    case GUI_TIMER:
      ts.updateTimeLine(*gdi);
    break;
  }
  return 0;
}

int TabSpeaker::handleEvent(gdioutput &gdi, const EventInfo &ei)
{
  updateTimeLine(gdi);
  return 0;
}

namespace {
  class ReportMode : public GuiHandler {
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) final {
      TabBase *tb = gdi.getTabs().get(TabType::TSpeakerTab);
      TabSpeaker *list = dynamic_cast<TabSpeaker *>(tb);
      if (list) {
        auto oe = list->getEvent();
        if (type == GUI_INPUTCHANGE) {
          InputInfo &ii = dynamic_cast<InputInfo &>(info);
          int nr = _wtoi(ii.text.c_str());
          if (nr > 0) {
            pRunner r = oe->getRunnerByBibOrStartNo(ii.text, false);
            if (r) {
              list->setSelectedRunner(*r);
              gdi.sendCtrlMessage("Report");
            }
          }

        }
      }
    }
  public:
    virtual ~ReportMode() {}
  };

  ReportMode reportHandler;
}

int TabSpeaker::processButton(gdioutput &gdi, const ButtonInfo &bu)
{
  if (bu.id=="Settings") {
    if (controlsToWatch.empty()) {
      // Get default
      vector<pControl> ctrl;
      oe->getControls(ctrl, true);
      for (size_t k = 0; k < ctrl.size(); k++) {
        if (ctrl[k]->isValidRadio()) {
          vector<int> cc;
          ctrl[k]->getCourseControls(cc);
          controlsToWatch.insert(cc.begin(), cc.end());
        }
      }
    }
    gdi.restore("settings");
    gdi.unregisterEvent("DataUpdate");
    gdi.fillDown();
    gdi.addString("", boldLarge, "Speakerstöd");
    gdi.addString("", 0, "help:speaker_setup");
    gdi.dropLine(1);
    gdi.addCheckbox("ShortNames", "Use initials in names", 0, oe->getPropertyInt("SpeakerShortNames", false) != 0);
    gdi.dropLine(0.5);

    gdi.pushX();
    gdi.fillRight();
    gdi.addListBox("Classes", 200, 300, 0,L"Klasser", L"", true);
    
    auto pos = gdi.getPos();
    
    gdi.setCY(gdi.getHeight());
    gdi.popX();
    gdi.fillRight();
    gdi.addButton("AllClass", "Alla", tabSpeakerCB);
    gdi.addButton("NoClass", "Inga", tabSpeakerCB);

    oe->fillClasses(gdi, "Classes", oEvent::extraNone, oEvent::filterNone);
    gdi.setSelection("Classes", classesToWatch);

    gdi.fillRight();
    gdi.setPos(pos);

    gdi.addListBox("Controls", 200, 300, 0, L"Kontroller", L"", true);
    gdi.pushX();
    gdi.fillDown();

    vector< pair<wstring, size_t> > d;
    oe->fillControls(d, oEvent::CTCourseControl);
    gdi.addItem("Controls", d);

    gdi.setSelection("Controls", controlsToWatch);

    gdi.dropLine();
    gdi.addButton("OK", "OK", tabSpeakerCB).setDefault();
    gdi.addButton("Cancel", "Avbryt", tabSpeakerCB).setCancel();

    gdi.refresh();
  }
  else if (bu.id == "AllClass") {
    set<int> lst;
    lst.insert(-1);
    gdi.setSelection("Classes", lst);
  }
  else if (bu.id == "NoClass") {
    set<int> lst;
    gdi.setSelection("Classes", lst);
  }
  else if (bu.id=="ZoomIn") {
    gdi.scaleSize(1.05);
  }
  else if (bu.id=="ZoomOut") {
    gdi.scaleSize(1.0/1.05);
  }
  else if (bu.id=="Manual") {
    gdi.unregisterEvent("DataUpdate");
    gdi.restore("settings");
    gdi.fillDown();
    gdi.addString("", boldLarge, "Inmatning av mellantider");
    gdi.dropLine(0.5);
    manualTimePage(gdi);
  }
  else if (bu.id == "PunchTable") {
    gdi.clearPage(false);
    gdi.addButton("Cancel", "Stäng", tabSpeakerCB);
    gdi.dropLine();
    gdi.addTable(oFreePunch::getTable(oe), gdi.getCX(), gdi.getCY());
    gdi.refresh();
  }
  else if (bu.id == "Report") {
    classId = -2;
    if (gdi.hasData("ReportMode")) {
      gdi.restore("ReportMode", false);
    }
    else {
      gdi.restore("speaker", false);
      gdi.pushX();
      gdi.fillRight();
      gdi.addSelection("ReportRunner", 300, 300, tabSpeakerCB);
      gdi.dropLine(0.2);
      gdi.addString("", 0, "Nummerlapp:");
      gdi.dropLine(-0.2);
      gdi.addInput("FindRunner", L"", 6, 0, L"", L"Nummerlapp").setHandler(&reportHandler);
      gdi.setRestorePoint("ReportMode");
    }

    gdi.setData("ReportMode", 1);
    oe->fillRunners(gdi, "ReportRunner", true, oEvent::RunnerFilterShowAll | oEvent::RunnerCompactMode);
    gdi.selectItemByData("ReportRunner", runnerId);

    gdi.dropLine(3);
    gdi.popX();
    gdi.registerEvent("DataUpdate", tabSpeakerCB);
    vector<pair<int, bool>> runnersToReport;
    if (runnerId > 0) {
      runnersToReport.emplace_back(runnerId, false);
    }
    TabRunner::generateRunnerReport(*oe, gdi, runnersToReport);
    gdi.refresh();
  }
  else if (bu.id == "Priority") {
    gdi.clearPage(false);
    gdi.addString("", boldLarge, "Bevakningsprioritering");
    gdi.addString("", 10, "help:speakerprio");
    gdi.dropLine();
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, "Klass:");
    gdi.addSelection("Class", 200, 200, tabSpeakerCB, L"", L"Välj klass");
    oe->fillClasses(gdi, "Class", oEvent::extraNone, oEvent::filterNone);
    gdi.addButton("ClosePri", "Stäng", tabSpeakerCB);
    gdi.dropLine(2);
    gdi.popX();
    gdi.refresh();
  }
  else if (bu.id == "ClosePri") {
    savePriorityClass(gdi);
    loadPage(gdi);
  }
  else if (bu.id == "LiveResult") {
    gdioutput *gdi_new = createExtraWindow(uniqueTag("list"), makeDash(L"MeOS - Live"), gdi.getWidth() + 64 + gdi.scaleLength(baseButtonWidth));
       
    gdi_new->clearPage(false);
    gdi_new->addString("", boldLarge, "Liveresultat");
    gdi_new->addString("", 10, "help:liveresultat");
    gdi_new->dropLine();
    gdi_new->pushY();
    gdi_new->pushX();

    TabList::makeClassSelection(*gdi_new);
    oe->fillClasses(*gdi_new, "ListSelection", oEvent::extraNone, oEvent::filterNone);
    
    gdi_new->popY();
    gdi_new->setCX(gdi_new->getCX() + gdi_new->scaleLength(280));
    gdi_new->fillRight();
    gdi_new->pushX();
    TabList::makeFromTo(*gdi_new);
    TabList::enableFromTo(*oe, *gdi_new, true, true);

    gdi_new->fillRight();
    gdi.dropLine();
    gdi_new->addButton("StartLive", "Starta", tabSpeakerCB).setDefault();
    gdi_new->addButton("CancelClose", "Avbryt", tabSpeakerCB).setCancel();

    gdi_new->refresh();
  }
  else if (bu.id == "StartLive") {
    TabList &ts = dynamic_cast<TabList &>(*gdi.getTabs().get(TListTab));
        
    oListInfo li;
    oListParam &par = li.getParam();;
    par.useControlIdResultTo = gdi.getSelectedItem("ResultSpecialTo").first;
    par.useControlIdResultFrom = gdi.getSelectedItem("ResultSpecialFrom").first;
    gdi.getSelection("ListSelection", par.selection);
    
    gdi.clearPage(false);
    gdi.setFullScreen(true);
    gdi.hideBackground(true);
    ts.liveResult(gdi, li);
  }
  else if (bu.id == "Events") {
    gdi.restore("classes");
    classId = -1;
    drawTimeLine(gdi);
  }
  else if (bu.id == "Window") {
    oe->setupTimeLineEvents(0);

    gdioutput *gdi_new = createExtraWindow(uniqueTag("speaker"), makeDash(L"MeOS - Speakerstöd"), gdi.getWidth() + 64 + gdi.scaleLength(baseButtonWidth));
    if (gdi_new) {
      TabSpeaker &tl = dynamic_cast<TabSpeaker &>(*gdi_new->getTabs().get(TSpeakerTab));
      tl.ownWindow = true;
      tl.loadPage(*gdi_new);
      //oe->renderTimeLineEvents(*gdi_new);
    }
    loadPage(gdi);
  }
  else if (bu.id == "SaveWindows") {
    if (!gdi.ask(L"ask:savespeaker"))
      return 0;

    vector<string> tags = getExtraWindows();
    vector< multimap<string, wstring> > speakerSettings;
    
    for (size_t i = 0; i < tags.size(); i++) {
      if (tags[i] != "main" && tags[i].substr(0, 7) != "speaker")
        continue;
      gdioutput *gdi = getExtraWindow(tags[i], false);
      if (gdi) {
        TabBase *tb = gdi->getTabs().get(TabType::TSpeakerTab);
        if (tb) {
          speakerSettings.push_back(multimap<string, wstring>());
          ((TabSpeaker *)tb)->getSettings(*gdi, speakerSettings.back());
        }
      }
    }
    saveSettings(speakerSettings);
  }
  else if (bu.id == "LoadWindows") {
    if (!gdi.ask(L"ask:loadspeaker"))
      return 0;
    vector< multimap<string, wstring> > speakerSettings;

    loadSettings(speakerSettings);
    if (speakerSettings.empty())
      throw meosException("Inställningarna är ogiltiga");
    for (size_t k = 1; k < speakerSettings.size(); k++) {
      gdioutput *gdi_new = createExtraWindow(uniqueTag("speaker"), makeDash(L"MeOS - Speakerstöd"), gdi.getWidth() + 64 + gdi.scaleLength(baseButtonWidth));
      if (gdi_new) {
        TabSpeaker &tl = dynamic_cast<TabSpeaker &>(*gdi_new->getTabs().get(TSpeakerTab));
        tl.ownWindow = true;
        tl.importSettings(*gdi_new, speakerSettings[k]);
        tl.loadPage(*gdi_new);
      }
    }
    importSettings(gdi, speakerSettings[0]);
    loadPage(gdi);
  }
  else if (bu.id=="StoreTime") {
    storeManualTime(gdi);
  }
  else if (bu.id=="Cancel") {
    loadPage(gdi);
  }
  else if (bu.id=="CancelClose") {
    gdi.closeWindow();
  }
  else if (bu.id=="OK") {
    gdi.getSelection("Classes", classesToWatch);
    gdi.getSelection("Controls", controlsToWatch);
    if (controlsToWatch.empty())
      controlsToWatch.insert(-2); // Non empty but no control

    for (set<int>::iterator it=controlsToWatch.begin();it!=controlsToWatch.end();++it) {
      pControl pc=oe->getControl(*it, false);
      if (pc) {
        pc->setRadio(true);
        pc->synchronize(true);
      }
    }
    oe->setProperty("SpeakerShortNames", (int)gdi.isChecked("ShortNames"));
    loadPage(gdi);
  }
  else if (bu.id.substr(0, 3)=="cid" ) {
    classId=atoi(bu.id.substr(3, string::npos).c_str());
    bool shortNames = oe->getPropertyInt("SpeakerShortNames", false) != 0;
    generateControlList(gdi, classId);
    gdi.setRestorePoint("speaker");
    gdi.setRestorePoint("SpeakerList");

    if (selectedControl.count(classId)==1)
      oe->speakerList(gdi, classId, selectedControl[classId].getLeg(),
                                    selectedControl[classId].getControl(),
                                    selectedControl[classId].getPreviousControl(),
                                    selectedControl[classId].isTotal(),
                                    shortNames);
  }
  else if (bu.id.substr(0, 4)=="ctrl") {
    bool shortNames = oe->getPropertyInt("SpeakerShortNames", false) != 0;
    int ctrl = atoi(bu.id.substr(4,
      string::npos).c_str());
    int ctrlPrev = bu.getExtraInt();
    selectedControl[classId].setControl(ctrl, ctrlPrev);
    gdi.restore("speaker");
    oe->speakerList(gdi, classId, selectedControl[classId].getLeg(), ctrl, ctrlPrev,
                                  selectedControl[classId].isTotal(), shortNames);
  }

  return 0;
}

void TabSpeaker::drawTimeLine(gdioutput &gdi) {
  gdi.restoreNoUpdate("SpeakerList");
  gdi.setRestorePoint("SpeakerList");

  gdi.fillRight();
  gdi.pushX();


  const bool multiDay = oe->hasPrevStage();

  gdi.dropLine(0.1);

  if (multiDay) {
    gdi.dropLine(0.2);
    gdi.addString("", 0, "Resultat:");
    gdi.dropLine(-0.2);

    gdi.addSelection("MultiStage", 100, 100, tabSpeakerCB);
    gdi.setCX(gdi.getCX() + gdi.getLineHeight()*2);
    gdi.addItem("MultiStage", lang.tl("Etappresultat"), 0);
    gdi.addItem("MultiStage", lang.tl("Totalresultat"), 1);
    gdi.selectItemByData("MultiStage", getSpeakerMonitor()->useTotalResults() ? 1 : 0);    
  }

  gdi.dropLine(0.2);
  gdi.addString("", 0, "Filtrering:");
  gdi.dropLine(-0.2);
  gdi.addSelection("DetailLevel", 160, 100, tabSpeakerCB);
  gdi.setCX(gdi.getCX() + gdi.getLineHeight()*2);
  gdi.addItem("DetailLevel", lang.tl("Alla händelser"), oTimeLine::PLow);
  gdi.addItem("DetailLevel", lang.tl("Viktiga händelser"), oTimeLine::PMedium);
  gdi.addItem("DetailLevel", lang.tl("Avgörande händelser"), oTimeLine::PHigh);
  gdi.selectItemByData("DetailLevel", watchLevel);

  gdi.dropLine(0.2);
  gdi.addString("", 0, "Antal:");
  gdi.dropLine(-0.2);
  gdi.addSelection("WatchNumber", 160, 200, tabSpeakerCB);
  gdi.addItem("WatchNumber", lang.tl("X senaste#5"), 5);
  gdi.addItem("WatchNumber", lang.tl("X senaste#10"), 10);
  gdi.addItem("WatchNumber", lang.tl("X senaste#20"), 20);
  gdi.addItem("WatchNumber", lang.tl("X senaste#50"), 50);
  gdi.addItem("WatchNumber", L"Alla", 0);
  gdi.selectItemByData("WatchNumber", watchNumber);
  gdi.dropLine(2);
  gdi.popX();

  wstring cls;
  for (set<int>::iterator it = classesToWatch.begin(); it != classesToWatch.end(); ++it) {
    pClass pc = oe->getClass(*it);
    if (pc) {
      if (!cls.empty())
        cls += L", ";
      cls += oe->getClass(*it)->getName();
    }
  }
  gdi.fillDown();
  gdi.addString("", 1, L"Bevakar händelser i X#" + cls);
  gdi.dropLine();

  gdi.setRestorePoint("TimeLine");
  updateTimeLine(gdi);
}

void TabSpeaker::updateTimeLine(gdioutput &gdi) {
  int storedY = gdi.getOffsetY();
  int storedHeight = gdi.getHeight();
  bool refresh = gdi.hasData("TimeLineLoaded");

  if (refresh)
    gdi.takeShownStringsSnapshot();

  gdi.restoreNoUpdate("TimeLine");
  gdi.setRestorePoint("TimeLine");
  gdi.setData("TimeLineLoaded", 1);

  gdi.registerEvent("DataUpdate", tabSpeakerCB);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);

  gdi.pushX(); gdi.pushY();
  gdi.updatePos(0,0,0, storedHeight);
  gdi.popX(); gdi.popY();
  gdi.setOffsetY(storedY);

  SpeakerMonitor &sm = *getSpeakerMonitor();
  int limit = 1000;
  switch (watchLevel) {
  case oTimeLine::PHigh:
    limit = 3;
    break;
  case oTimeLine::PMedium:
    limit = 10;
    break;
  }

  sm.setLimits(limit, watchNumber);
  sm.setClassFilter(classesToWatch, controlsToWatch);
  sm.show(gdi);

  if (refresh)
    gdi.refreshSmartFromSnapshot(false);
  else {
    if (storedHeight == gdi.getHeight())
      gdi.refreshFast();
    else
      gdi.refresh();
  }
}

/*
void TabSpeaker::updateTimeLine(gdioutput &gdi) {
  int storedY = gdi.GetOffsetY();
  int storedHeight = gdi.getHeight();
  bool refresh = gdi.hasData("TimeLineLoaded");

  if (refresh)
    gdi.takeShownStringsSnapshot();

  gdi.restoreNoUpdate("TimeLine");
  gdi.setRestorePoint("TimeLine");
  gdi.setData("TimeLineLoaded", 1);

  gdi.registerEvent("DataUpdate", tabSpeakerCB);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);

  gdi.pushX(); gdi.pushY();
  gdi.updatePos(0,0,0, storedHeight);
  gdi.popX(); gdi.popY();
  gdi.SetOffsetY(storedY);

  int nextEvent = oe->getTimeLineEvents(classesToWatch, events, shownEvents, 0);

  oe->updateComputerTime();
  int timeOut = nextEvent * 1000 - oe->getComputerTimeMS();

  string str = "Now: " + oe->getAbsTime(oe->getComputerTime()) + " Next:" + oe->getAbsTime(nextEvent) + " Timeout:" + itos(timeOut) + "\n";
  OutputDebugString(str.c_str());

  if (timeOut > 0) {
    gdi.addTimeoutMilli(timeOut, "timeline", tabSpeakerCB);
    //gdi.addTimeout(timeOut, tabSpeakerCB);
  }
  bool showClass = classesToWatch.size()>1;

  oListInfo li;
  const int classWidth = gdi.scaleLength(li.getMaxCharWidth(oe, classesToWatch, lClassName, "", normalText, false));
  const int timeWidth = gdi.scaleLength(60);
  const int totWidth = gdi.scaleLength(450);
  const int extraWidth = gdi.scaleLength(200);
  string dash = MakeDash("- ");
  const int extra = 5;

  int limit = watchNumber > 0 ? watchNumber : events.size();
  for (size_t k = events.size()-1; signed(k) >= 0; k--) {
    oTimeLine &e = events[k];

    if (e.getPriority() < watchLevel)
      continue;

    if (--limit < 0)
      break;

    int t = e.getTime();
    string msg = t>0 ? oe->getAbsTime(t) : MakeDash("--:--:--");
    pRunner r = pRunner(e.getSource(*oe));

    RECT rc;
    rc.top = gdi.getCY();
    rc.left = gdi.getCX();

    int xp = rc.left + extra;
    int yp = rc.top + extra;

    bool largeFormat = e.getPriority() >= oTimeLine::PHigh && k + 15 >= events.size();

    if (largeFormat) {
      gdi.addStringUT(yp, xp, 0, msg);

      int dx = 0;
      if (showClass) {
        pClass pc = oe->getClass(e.getClassId());
        if (pc) {
          gdi.addStringUT(yp, xp + timeWidth, fontMediumPlus, pc->getName());
          dx = classWidth;
        }
      }

      if (r) {
        string bib = r->getBib();
        if (!bib.empty())
          msg = bib + ", ";
        else
          msg = "";
        msg += r->getCompleteIdentification();
        int xlimit = totWidth + extraWidth - (timeWidth + dx);
        gdi.addStringUT(yp, xp + timeWidth + dx, fontMediumPlus, msg, xlimit);
      }

      yp += int(gdi.getLineHeight() * 1.5);
      msg = dash + lang.tl(e.getMessage());
      gdi.addStringUT(yp, xp + 10, breakLines, msg, totWidth);

      const string &detail = e.getDetail();

      if (!detail.empty()) {
        gdi.addString("", gdi.getCY(), xp + 20, 0, detail).setColor(colorDarkGrey);
      }

      if (r && e.getType() == oTimeLine::TLTFinish && r->getStatus() == StatusOK) {
        splitAnalysis(gdi, xp + 20, gdi.getCY(), r);
      }

      GDICOLOR color = colorLightRed;

      switch (e.getType()) {
        case oTimeLine::TLTFinish:
          if (r && r->statusOK())
            color = colorLightGreen;
          else
            color = colorLightRed;
          break;
        case oTimeLine::TLTStart:
          color = colorLightYellow;
          break;
        case oTimeLine::TLTRadio:
          color = colorLightCyan;
          break;
        case oTimeLine::TLTExpected:
          color = colorLightBlue;
          break;
      }

      rc.bottom = gdi.getCY() + extra;
      rc.right = rc.left + totWidth + extraWidth + 2 * extra;
      gdi.addRectangle(rc, color, true);
      gdi.dropLine(0.5);
    }
    else {
      if (showClass) {
        pClass pc = oe->getClass(e.getClassId());
        if (pc )
          msg += " (" + pc->getName() + ") ";
        else
          msg += " ";
      }
      else
        msg += " ";

      if (r) {
        msg += r->getCompleteIdentification() + " ";
      }
      msg += lang.tl(e.getMessage());
      gdi.addStringUT(gdi.getCY(), gdi.getCX(), breakLines, msg, totWidth);

      const string &detail = e.getDetail();

      if (!detail.empty()) {
        gdi.addString("", gdi.getCY(), gdi.getCX()+50, 0, detail).setColor(colorDarkGrey);
      }

      if (r && e.getType() == oTimeLine::TLTFinish && r->getStatus() == StatusOK) {
        splitAnalysis(gdi, xp, gdi.getCY(), r);
      }
      gdi.dropLine(0.5);
    }
  }

  if (refresh)
    gdi.refreshSmartFromSnapshot(false);
  else {
    if (storedHeight == gdi.getHeight())
      gdi.refreshFast();
    else
      gdi.refresh();
  }
}
*/
void TabSpeaker::splitAnalysis(gdioutput &gdi, int xp, int yp, pRunner r)
{
  if (!r)
    return;

  vector<int> delta;
  r->getSplitAnalysis(delta);
  wstring timeloss = lang.tl("Bommade kontroller: ");
  pCourse pc = 0;
  bool first = true;
  const int charlimit = 90;
  for (size_t j = 0; j<delta.size(); ++j) {
    if (delta[j] > 0) {
      if (pc == 0) {
        pc = r->getCourse(true);
        if (pc == 0)
          break;
      }

      if (!first)
        timeloss += L" | ";
      else
        first = false;

      timeloss += pc->getControlOrdinal(j) + L". " + formatTime(delta[j]);
    }
    if (timeloss.length() > charlimit || (!timeloss.empty() && !first && j+1 == delta.size())) {
      gdi.addStringUT(yp, xp, 0, timeloss).setColor(colorDarkRed);
      yp += gdi.getLineHeight();
      timeloss = L"";
    }
  }
  if (first) {
    gdi.addString("", yp, xp, 0, "Inga bommar registrerade").setColor(colorDarkGreen);
  }
}

pCourse deduceSampleCourse(pClass pc, vector<oClass::TrueLegInfo > &stages, int leg);

void TabSpeaker::generateControlList(gdioutput &gdi, int classId)
{
  pClass pc=oe->getClass(classId);

  if (!pc)
    return;

  bool keepLegs = false;
  if (gdi.hasWidget("Leg")) {
    DWORD clsSel = 0;
    if (gdi.getData("ClassSelection", clsSel) && clsSel == pc->getId()) {
      gdi.restore("LegSelection", true);
      keepLegs = true;
    }
  }

  if (!keepLegs)
    gdi.restore("classes", true);

  gdi.setData("ClassSelection", pc->getId());

  pCourse course=0;

  int h,w;
  gdi.getTargetDimension(w, h);

  gdi.fillDown();

  int bw=gdi.scaleLength(100);
  int nbtn=max((w-80)/bw, 1);
  bw=(w-80)/nbtn;
  int basex=SPEAKER_BASE_X;
  int basey=gdi.getCY()+4;

  int cx=basex;
  int cy=basey;
  int cb=1;

  vector<oClass::TrueLegInfo> stages;
  pc->getTrueStages(stages);
  if (pc->getQualificationFinal()) {
    while (stages.size() > 1)
      stages.pop_back(); //Ignore for qualification race
  }
  int leg = selectedControl[pc->getId()].getLeg();
  const bool multiDay = oe->hasPrevStage();

  if (stages.size()>1 || multiDay) {
    if (!keepLegs) {
      gdi.setData("CurrentY", cy);
      gdi.addSelection(cx, cy+2, "Leg", int(bw/gdi.getScale())-5, 100, tabSpeakerCB);
      bool total = selectedControl[pc->getId()].isTotal();
      if (leg == 0 && stages[0].first != 0) {
        leg = stages[0].first;
        selectedControl[pc->getId()].setLeg(total, leg);
      }

      if (stages.size() > 1) {
        for (size_t k=0; k<stages.size(); k++) {
          gdi.addItem("Leg", lang.tl("Sträcka X#" + itos(stages[k].second)), stages[k].first);
        }
        if (multiDay) {
          for (size_t k=0; k<stages.size(); k++) {
            gdi.addItem("Leg", lang.tl("Sträcka X (total)#" + itos(stages[k].second)), 1000 + stages[k].first);
          }
        }
      }
      else if (stages.size() == 1) {
         if (leg == 0 && stages[0].first != 0)
           leg = stages[0].first;
         gdi.addItem("Leg", lang.tl("Etappresultat"), stages[0].first);
         gdi.addItem("Leg", lang.tl("Totalresultat"), 1000 + stages[0].first);
      }

      gdi.selectItemByData("Leg", leg + (total ? 1000 : 0));
      gdi.setRestorePoint("LegSelection");
    }
    else {
      gdi.getData("CurrentY", *(DWORD*)&cy);
    }
    cb+=1;
    cx+=1*bw;
  }
  else if (stages.size() == 1) {
    selectedControl[classId].setLeg(false, stages[0].first);
    leg = stages[0].first;
  }

  course = deduceSampleCourse(pc, stages, leg);
  /*
  int courseLeg = leg;
  for (size_t k = 0; k <stages.size(); k++) {
    if (stages[k].first == leg) {
      courseLeg = stages[k].nonOptional;
      break;
    }
  }

  if (pc->hasMultiCourse())
    course = pc->getCourse(courseLeg, 0, true);
  else
    course = pc->getCourse(true);
  */
  vector<pControl> controls;

  if (course)
    course->getControls(controls);
  int previousControl = 0;
  for (size_t k = 0; k < controls.size(); k++) {
    int cid = course->getCourseControlId(k);
    if (controlsToWatch.count(cid) ) {

      if (selectedControl[classId].getControl() == -1) {
        // Default control
        selectedControl[classId].setControl(cid, previousControl);
      }

      char bf[16];
      sprintf_s(bf, "ctrl%d", cid);
      wstring name = course->getRadioName(cid);
      /*if (controls[k]->hasName()) {
        name = "#" + controls[k]->getName();
        if (controls[k]->getNumberDuplicates() > 1)
          name += "-" + itos(numbering++);
      }
      else {
        name = lang.tl("radio X#" + itos(numbering++));
        capitalize(name);
        name = "#" + name;
      }
      */
      wstring tooltip = lang.tl("kontroll X (Y)#" + itos(k+1) +"#" + itos(controls[k]->getFirstNumber()));
      capitalize(tooltip);
      ButtonInfo &bi = gdi.addButton(cx, cy, bw, bf, L"#" + name, tabSpeakerCB, L"#" + tooltip, false, false);
      bi.setExtra(previousControl);
      previousControl = cid;
      cx+=bw;

      cb++;
      if (cb>nbtn) {
        cb=1;
        cy+=gdi.getButtonHeight()+4;
        cx=basex;
      }
    }
  }
  gdi.fillDown();
  char bf[16];
  sprintf_s(bf, "ctrl%d", oPunch::PunchFinish);
  gdi.addButton(cx, cy, bw, bf, "Mål", tabSpeakerCB, "", false, false).setExtra(previousControl);

  if (selectedControl[classId].getControl() == -1) {
    // Default control
    selectedControl[classId].setControl(oPunch::PunchFinish, previousControl);
  }

  gdi.popX();
}

int TabSpeaker::deducePreviousControl(int classId, int leg, int control) {
  pClass pc = oe->getClass(classId);
  if (pc == 0)
    return -1;
  vector<oClass::TrueLegInfo> stages;
  pc->getTrueStages(stages);
  pCourse course = deduceSampleCourse(pc, stages, leg);
  vector<pControl> controls;
  if (course)
    course->getControls(controls);
  int previousControl = 0;
  for (size_t k = 0; k < controls.size(); k++) {
    int cid = course->getCourseControlId(k);
    if (controlsToWatch.count(cid)) {
      if (cid == control)
        return previousControl;

      previousControl = cid;
    }
  }
  if (control == oPunch::PunchFinish)
    return previousControl;
  
  return -1;
}

int TabSpeaker::processListBox(gdioutput &gdi, const ListBoxInfo &bu)
{
  if (bu.id == "Leg") {
    if (classId > 0) {
      selectedControl[classId].setLeg(bu.data >= 1000, bu.data % 1000);
      generateControlList(gdi, classId);
      gdi.setRestorePoint("speaker");
      gdi.setRestorePoint("SpeakerList");

      bool shortNames = oe->getPropertyInt("SpeakerShortNames", false) != 0;
      oe->speakerList(gdi, classId, selectedControl[classId].getLeg(),
                      selectedControl[classId].getControl(),
                      selectedControl[classId].getPreviousControl(),
                      selectedControl[classId].isTotal(),
                      shortNames);
    }
  }
  else if (bu.id == "MultiStage") {
    getSpeakerMonitor()->useTotalResults(bu.data != 0);
    updateTimeLine(gdi);
  }
  else if (bu.id == "DetailLevel") {
    watchLevel = oTimeLine::Priority(bu.data);
    shownEvents.clear();
    events.clear();

    updateTimeLine(gdi);
  }
  else if (bu.id == "WatchNumber") {
    watchNumber = bu.data;
    updateTimeLine(gdi);
  }
  else if (bu.id == "Class") {
    savePriorityClass(gdi);
    int classId = int(bu.data);
    loadPriorityClass(gdi, classId);
  }
  else if (bu.id == "ReportRunner") {
    runnerId = bu.data;
    gdi.sendCtrlMessage("Report");
  }
  return 0;
}

bool TabSpeaker::loadPage(gdioutput &gdi) {
  oe->checkDB();

  gdi.clearPage(false);
  gdi.pushX();
  gdi.setRestorePoint("settings");

  gdi.pushX();
  gdi.fillDown();

  int h,w;
  gdi.getTargetDimension(w, h);

  int bw = gdi.scaleLength(100);
  int numBtn = max((w - gdi.scaleLength(80)) / bw, 1);
  bw = (w - 80) / numBtn;
  int basex = SPEAKER_BASE_X;
  int basey=gdi.getCY();

  int cx=basex;
  int cy=basey;
  
  vector<pClass> clsToWatch;
  for (int cid : classesToWatch) {
    pClass pc = oe->getClass(cid);
    if (pc) {
      clsToWatch.push_back(pc);
    }
  }

  sort(clsToWatch.begin(), clsToWatch.end(), [](const pClass &a, const pClass &b) {return *a < *b; });

  int bwCls = bw;
  TextInfo ti;
  for (auto pc : clsToWatch) {
    ti.xp = 0;
    ti.yp = 0;
    ti.format = 0;
    ti.text = pc->getName();
    gdi.calcStringSize(ti);
    bwCls = max(bwCls, ti.realWidth+ gdi.scaleLength(10));
  }
  int limitX = w - bw / 3;

  for (auto pc : clsToWatch) {
    char classid[32];
    sprintf_s(classid, "cid%d", pc->getId());

    if (cx > basex && (cx + bwCls) >= limitX) {
      cx = basex; 
      cy += gdi.getButtonHeight() + 4;
    }

    gdi.addButton(cx, cy, bwCls-2, classid, L"#" + pc->getName(), tabSpeakerCB, L"", false, false);
    cx += bwCls;
  }

  bool pm = false;
  int db = 0;
  if (classesToWatch.empty()) {
    gdi.addString("", boldLarge, "Speakerstöd");
    gdi.dropLine();
    cy=gdi.getCY();
    cx=gdi.getCX();
  }
  else {
    if ((cx + db) > basex && (cx + db + bw) >= limitX) {
      cx = basex; db = 0;
      cy += gdi.getButtonHeight() + 4;
    }
    gdi.addButton(cx+db, cy, bw-2, "Events", "Händelser", tabSpeakerCB, "Löpande information om viktiga händelser i tävlingen", false, false);
    db += bw;
    pm = true;
  }

  if ((cx + db) > basex && (cx + db + bw) >= limitX) {
    cx = basex; db = 0;
    cy += gdi.getButtonHeight() + 4;
  }
  gdi.addButton(cx + db, cy, bw - 2, "Report", "Rapportläge", tabSpeakerCB, "Visa detaljerad rapport för viss deltagare", false, false);
  db += bw;

  if (pm) {
    gdi.addButton(cx + db, cy, bw / 5, "ZoomIn", "+", tabSpeakerCB, "Zooma in (Ctrl + '+')", false, false);
    db += bw / 5 + 2;
    gdi.addButton(cx + db, cy, bw / 5, "ZoomOut", makeDash(L"-"), tabSpeakerCB, L"Zooma ut (Ctrl + '-')", false, false);
    db += bw / 5 + 2;
  }

  if ((cx + db) > basex && (cx + db + bw) >= limitX) {
    cx = basex; db = 0;
    cy += gdi.getButtonHeight() + 4;
  }
  gdi.addButton(cx+db, cy, bw-2, "Settings", "Inställningar...", tabSpeakerCB, "Välj vilka klasser och kontroller som bevakas", false, false);
  db += bw;
  
  if ((cx + db) > basex && (cx + db + bw) >= limitX) {
    cx = basex; db = 0;
    cy += gdi.getButtonHeight() + 4;
  }
  gdi.addButton(cx+db, cy, bw-2, "Manual", "Tidsinmatning", tabSpeakerCB, "Mata in radiotider manuellt", false, false);
  db += bw;

  if ((cx + db) > basex && (cx + db + bw) >= limitX) {
    cx = basex; db = 0;
    cy += gdi.getButtonHeight() + 4;
  }
  gdi.addButton(cx+db, cy, bw-2, "PunchTable", "Stämplingar", tabSpeakerCB, "Visa en tabell över alla stämplingar", false, false);
  db += bw;

  if ((cx + db) > basex && (cx + db + bw) >= limitX) {
    cx = basex; db = 0;
    cy += gdi.getButtonHeight() + 4;
  }
  gdi.addButton(cx+db, cy, bw-2, "LiveResult", "Direkt tidtagning", tabSpeakerCB, "Visa rullande tider mellan kontroller i helskärmsläge", false, false);
  db += bw;

  if (!ownWindow) {
    if ((cx + db) > basex && (cx + db + bw) >= limitX) {
      cx = basex; db = 0;
      cy += gdi.getButtonHeight() + 4;
    }
    gdi.addButton(cx+db, cy, bw-2, "Priority", "Prioritering", tabSpeakerCB, "Välj löpare att prioritera bevakning för", false, false);
    db += bw;


    if ((cx + db) > basex && (cx + db + bw) >= limitX) {
      cx = basex; db = 0;
      cy += gdi.getButtonHeight() + 4;
    }
    gdi.addButton(cx+db, cy, bw-2, "Window", "Nytt fönster", tabSpeakerCB, "", false, false);
    db += bw;


    if (getExtraWindows().size() == 1) {
      wstring sf = getSpeakerSettingsFile();
      if (fileExists(sf)) {
        if ((cx + db) > basex && (cx + db + bw) >= limitX) {
          cx = basex; db = 0;
          cy += gdi.getButtonHeight() + 4;
        }
        gdi.addButton(cx + db, cy, bw - 2, "LoadWindows", "Återskapa", tabSpeakerCB, "Återskapa tidigare sparade fönster- och speakerinställningar", false, false);
        db += bw;
      }
    }
    else {
      if ((cx + db) > basex && (cx + db + bw) >= limitX) {
        cx = basex; db = 0;
        cy += gdi.getButtonHeight() + 4;
      }
      gdi.addButton(cx + db, cy, bw - 2, "SaveWindows", "Spara", tabSpeakerCB, "Spara fönster- och speakerinställningar på datorn", false, false);
      db += bw;
    }
  }

  gdi.setRestorePoint("classes");

  if (classId == -1) {
    string btn = "Events";
    if (gdi.hasWidget(btn))
      gdi.sendCtrlMessage(btn);
  }
  else if (classId == -2) {
    string btn = "Report";
    if (gdi.hasWidget(btn))
      gdi.sendCtrlMessage(btn);
  }
  else if (classId > 0) {
    string btn = "cid" + itos(classId);
    if (gdi.hasWidget(btn))
      gdi.sendCtrlMessage(btn);
  }

  gdi.refresh();
  return true;
}

void TabSpeaker::clearCompetitionData()
{
  controlsToWatch.clear();
  classesToWatch.clear();
  selectedControl.clear();
  classId=0;
  lastControl.clear();

  lastControlToWatch = 0;
  lastClassToWatch = 0;

  shownEvents.clear();
  events.clear();

  delete speakerMonitor;
  speakerMonitor = 0;
  runnerId = -1;
}

void TabSpeaker::manualTimePage(gdioutput &gdi) const
{
  gdi.setRestorePoint("manual");

  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("Control", lastControl, 5, 0, L"Kontroll");
  gdi.addInput("Runner", L"", 6, 0, L"Löpare");
  gdi.addInput("Time", L"", 8, 0, L"Tid");
  gdi.dropLine();
  gdi.addButton("StoreTime", "Spara", tabSpeakerCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", tabSpeakerCB).setCancel();
  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);
  gdi.addString("", 10, "help:14692");

  gdi.setInputFocus("Runner");
  gdi.refresh();
}

void TabSpeaker::storeManualTime(gdioutput &gdi)
{
  char bf[256];

  int punch=gdi.getTextNo("Control");

  if (punch<=0)
    throw std::exception("Kontrollnummer måste anges.");

  lastControl=gdi.getText("Control");
  const wstring &r_str=gdi.getText("Runner");
  wstring time=gdi.getText("Time");

  if (time.empty())
    time=getLocalTimeOnly();

  int itime=oe->getRelativeTime(time);

  if (itime <= 0)
    throw std::exception("Ogiltig tid.");

  pRunner r=oe->getRunnerByBibOrStartNo(r_str, false);
  int r_no = _wtoi(r_str.c_str());
  if (!r)
    r=oe->getRunnerByCardNo(r_no, itime, oEvent::CardLookupProperty::Any);

  wstring Name;
  int sino=r_no;
  if (r) {
    Name = r->getName();
    sino = r->getCardNo();
  }
  else
    Name = lang.tl("Okänd");

  if (sino <= 0) {
    sprintf_s(bf, "Ogiltigt bricknummer.#%d", sino);
    throw std::exception(bf);
  }

  oe->addFreePunch(itime, punch, sino, true);

  gdi.restore("manual", false);
  gdi.addString("", 0, L"Löpare: X, kontroll: Y, kl Z#" + Name + L"#" + oPunch::getType(punch) + L"#" +  oe->getAbsTime(itime));

  manualTimePage(gdi);
}

void TabSpeaker::loadPriorityClass(gdioutput &gdi, int classId) {

  gdi.restore("PrioList");
  gdi.setRestorePoint("PrioList");
  gdi.setOnClearCb(tabSpeakerCB);
  runnersToSet.clear();
  vector<pRunner> r;
  oe->getRunners(classId, 0, r);

  int x = gdi.getCX();
  int y = gdi.getCY()+2*gdi.getLineHeight();
  int dist = gdi.scaleLength(25);
  int dy = int(gdi.getLineHeight()*1.3);
  for (size_t k = 0; k<r.size(); k++) {
    if (r[k]->skip() /*|| r[k]->getLeg>0*/)
      continue;
    int pri = r[k]->getSpeakerPriority();
    int id = r[k]->getId();
    gdi.addCheckbox(x,y,"A" + itos(id), "", 0, pri>0);
    gdi.addCheckbox(x+dist,y,"B" + itos(id), "", 0, pri>1);
    gdi.addStringUT(y-dist/3, x+dist*2, 0, r[k]->getCompleteIdentification());
    runnersToSet.push_back(id);
    y += dy;
  }
  gdi.refresh();
}

void TabSpeaker::savePriorityClass(gdioutput &gdi) {
  oe->synchronizeList({ oListId::oLRunnerId,oListId::oLTeamId });

  for (size_t k = 0; k<runnersToSet.size(); k++) {
    pRunner r = oe->getRunner(runnersToSet[k], 0);
    if (r) {
      int id = runnersToSet[k];
      if (!gdi.hasWidget("A" + itos(id))) {
        runnersToSet.clear(); //Page not loaded. Abort.
        return;
      }

      bool a = gdi.isChecked("A" + itos(id));
      bool b = gdi.isChecked("B" + itos(id));
      int pri = (a?1:0) + (b?1:0);
      pTeam t = r->getTeam();
      if (t) {
        t->setSpeakerPriority(pri);
        t->synchronize(true);
      }
      else {
        r->setSpeakerPriority(pri);
        r->synchronize(true);
      }
    }
  }
}

bool TabSpeaker::onClear(gdioutput &gdi) {
  if (!runnersToSet.empty())
    savePriorityClass(gdi);

  return true;
}
SpeakerMonitor *TabSpeaker::getSpeakerMonitor() {
  if (speakerMonitor == 0)
    speakerMonitor = new SpeakerMonitor(*oe);

  return speakerMonitor;
}

void TabSpeaker::getSettings(gdioutput &gdi, multimap<string, wstring> &settings) {
  RECT rc;
  gdi.getWindowsPosition(rc);
  settings.insert(make_pair("left", itow(rc.left)));
  settings.insert(make_pair("right", itow(rc.right)));
  settings.insert(make_pair("top", itow(rc.top)));
  settings.insert(make_pair("bottom", itow(rc.bottom)));

  for (auto clsId : classesToWatch) {
    pClass cls = oe->getClass(clsId);
    if (cls)
      settings.insert(make_pair("class", cls->getName()));

    if (classId == clsId) {
      settings.insert(make_pair("currentClass", cls->getName()));
      if (selectedControl.count(clsId)) {
        int cControl = selectedControl[clsId].getControl();
        int cLeg = selectedControl[clsId].getLeg();
        bool cTotal = selectedControl[clsId].isTotal();
        settings.insert(make_pair("currentControl", itow(cControl)));
        settings.insert(make_pair("currentLeg", itow(cLeg)));
        settings.insert(make_pair("currentTotal", itow(cTotal)));
      }
    }
  }

  if (classId == -1) {
    settings.insert(make_pair("currentClass", L"@Events"));
  }
  else if (classId == -2) {
    settings.insert(make_pair("currentClass", L"@Report"));
  }

  for (auto ctrl : controlsToWatch) {
    settings.insert(make_pair("control", itow(ctrl)));
  }
}

int get(const multimap<string, wstring> &settings, const char *p) {
  auto res = settings.find(p);
  if (res != settings.end())
    return _wtoi(res->second.c_str());

  return 0;
}

void TabSpeaker::importSettings(gdioutput &gdi, multimap<string, wstring> &settings) {
  classId = 0;
  classesToWatch.clear();
  controlsToWatch.clear();
  selectedControl.clear();
  int ctrl = 0, leg = 0, total = 0;

  for (auto &s : settings) {
    if (s.first == "currentClass") {
      if (s.second == L"@Events") {
        classId = -1;
      }
      else if (s.second == L"@Report") {
        classId = -2;
      }
      else {
        pClass cls = oe->getClass(s.second);
        classId = cls ? cls->getId() : 0;
        if (classId > 0) {
          ctrl = get(settings, "currentControl");
          leg = get(settings, "currentLeg");
          total = get(settings, "currentTotal");
        }
      }
    }
    else if (s.first == "class") {
      pClass cls = oe->getClass(s.second);
      if (cls)
        classesToWatch.insert(cls->getId());
    }
    else if (s.first == "control") {
      int ctrl = _wtoi(s.second.c_str());
      pControl pc = oe->getControl(ctrl);
      if (pc) {
        controlsToWatch.insert(pc->getId());
      }
    }
  }

  int previousControl = deducePreviousControl(classId, leg, ctrl);
  if (previousControl != -1) {
    selectedControl[classId].setLeg(total != 0, leg);
    selectedControl[classId].setControl(ctrl, previousControl);
  }

  RECT rc;
  if (settings.find("left") == settings.end() ||
      settings.find("right") == settings.end() ||
      settings.find("top") == settings.end() ||
      settings.find("bottom") == settings.end())
    throw meosException("Inställningarna är ogiltiga");
  
  rc.left = get(settings, "left");
  rc.right = get(settings, "right");
  rc.top = get(settings, "top");
  rc.bottom = get(settings, "bottom");

  RECT desktop;
  gdi.getVirtualScreenSize(desktop);
  if (rc.right > rc.left && rc.bottom > rc.top &&
      rc.right > 50 && rc.left < (desktop.right - 50) &&
      rc.bottom > 50 && rc.top < (desktop.bottom - 50))
    gdi.setWindowsPosition(rc); 
}

wstring TabSpeaker::getSpeakerSettingsFile() {
  wchar_t path[260];
  getUserFile(path, L"speaker.xml");
  return path;
}

void TabSpeaker::loadSettings(vector< multimap<string, wstring> > &settings) {
  settings.clear();
  xmlparser reader;
  reader.read(getSpeakerSettingsFile());
  xmlobject sp = reader.getObject("Speaker");
  if (!sp)
    return;

  xmlList xmlsettings;
  sp.getObjects(xmlsettings);
  
  for (auto &s : xmlsettings) {
    settings.push_back(multimap<string, wstring>());
    xmlList allS;
    s.getObjects(allS);
    for (auto &prop : allS) {
      settings.back().insert(make_pair(prop.getName(), prop.getw()));
    }
  }
}

void TabSpeaker::saveSettings(const vector< multimap<string, wstring> > &settings) {
  xmlparser d;
  d.openOutput(getSpeakerSettingsFile().c_str(), false);
  d.startTag("Speaker");
  for (auto &s : settings) {
    d.startTag("SpeakerWindow");
    for (auto &prop : s) {
      d.write(prop.first.c_str(), prop.second);
    }
    d.endTag();
  }
  d.endTag();
  d.closeOut();
}
