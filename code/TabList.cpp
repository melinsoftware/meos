/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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
#include "csvparser.h"
#include "SportIdent.h"
#include "oListInfo.h"
#include "TabList.h"
#include "TabRunner.h"
#include "TabTeam.h"
#include "TabSI.h"
#include "TabAuto.h"
#include "meos_util.h"
#include <cassert>
#include "classconfiginfo.h"
#include "metalist.h"
#include "gdifonts.h"
#include "listeditor.h"
#include "meosexception.h"
#include "pdfwriter.h"
#include "methodeditor.h"
#include "MeOSFeatures.h"
#include "liveresult.h"
#include "animationdata.h"
#include <algorithm>
#include "HTMLWriter.h"

const static int CUSTOM_OFFSET = 10;
const static int NUMTEXTSAMPLE = 13;

const static int ForcePageBreak = 1024;
const static int IgnoreLimitPer = 512;
const static int AddTeamClasses = 4;
const static int AddPatrolClasses = 3;
const static int AddRogainingClasses = 2;
const static int AddAllClasses = 1;

TabList::TabList(oEvent *poe):TabBase(poe)
{
  listEditor = 0;
  methodEditor = 0;

  lastHtmlTarget = poe->getPropertyString("LastExportTarget", L"");
  clearCompetitionData();
}

TabList::~TabList(void)
{
  delete listEditor;
  delete methodEditor;
  listEditor = 0;
  methodEditor = 0;

  for (size_t k = 0; k < liveResults.size(); k++) {
    delete liveResults[k];
    liveResults[k] = 0;
  }
  liveResults.clear();
}

int ListsCB(gdioutput *gdi, int type, void *data);

int ListsEventCB(gdioutput *gdi, int type, void *data)
{
  if (type!=GUI_EVENT)
    return -1;

  TabList &tc = dynamic_cast<TabList &>(*gdi->getTabs().get(TListTab));

  tc.rebuildList(*gdi);

  return 0;
}

void TabList::rebuildList(gdioutput &gdi)
{
  if (!SelectedList.empty()) {
    ButtonInfo bi;
    bi.id=SelectedList;
    noReEvaluate = true;
    ListsCB(&gdi, GUI_BUTTON, &bi);
    noReEvaluate = false;
  }
}

int openRunnerTeamCB(gdioutput *gdi, int type, void *data)
{
  if (type==GUI_LINK && data) {
    TextInfo *ti = static_cast<TextInfo *>(data);
    int id = ti->getExtraInt();

    if (id != 0 && ti->id == "T" && gdi->canClear()) {
      TabTeam &tt = dynamic_cast<TabTeam &>(*gdi->getTabs().get(TTeamTab));
      tt.loadPage(*gdi, id);
    }
    else if (id != 0 && ti->id == "R" && gdi->canClear()) {
      TabRunner &tr = dynamic_cast<TabRunner &>(*gdi->getTabs().get(TRunnerTab));
      tr.loadPage(*gdi, id);
    }
  }
  return 0;
}

int NoStartRunnerCB(gdioutput *gdi, int type, void *data)
{
  if (type==GUI_LINK)
  {
    TextInfo *ti=(TextInfo *)data;
    int id=atoi(ti->id.c_str());

    TabList &tc = dynamic_cast<TabList &>(*gdi->getTabs().get(TListTab));
    pRunner p = tc.getEvent()->getRunner(id, 0);

    if (p) {
      p->setStatus(StatusDNS, true, false);
      p->synchronize();
      ti->callBack=0;
      ti->highlight=false;
      ti->active=false;
      ti->color=RGB(255,0,0);
      gdi->setText(ti->id, L"Ej start", true);
    }
  }
  return 0;
}

int ListsCB(gdioutput *gdi, int type, void *data)
{
  TabList &tc = dynamic_cast<TabList &>(*gdi->getTabs().get(TListTab));

  return tc.listCB(*gdi, type, data);
}

int TabList::baseButtons(gdioutput &gdi, int extraButtons) {
  gdi.addButton(gdi.getWidth()+20, 15,  gdi.scaleLength(120),
                "Cancel", ownWindow ? "Stäng" : "Återgå", ListsCB, "", true, false);

  gdi.addButton(gdi.getWidth()+20, 18+gdi.getButtonHeight(), gdi.scaleLength(120),
                "Print", "Skriv ut...", ListsCB, "Skriv ut listan.", true, false);
  if (!ownWindow) {
    gdi.addButton(gdi.getWidth() + 20, 21 + 2 * gdi.getButtonHeight(), gdi.scaleLength(120),
                  "HTMLDesign", "Webb...", ListsCB, "Spara för webben.", true, false);
  }
  gdi.addButton(gdi.getWidth()+20, 24+3*gdi.getButtonHeight(), gdi.scaleLength(120),
                "PDF", "PDF...", ListsCB, "Spara som PDF.", true, false);
  gdi.addButton(gdi.getWidth()+20, 27+4*gdi.getButtonHeight(), gdi.scaleLength(120),
                "Copy", "Kopiera", ListsCB, "Kopiera till urklipp.", true, false);

  int ypos = 30+5*gdi.getButtonHeight();

  if (extraButtons == 1) {
    int w, h;
    gdi.addButton(gdi.getWidth()+20, ypos, gdi.scaleLength(120),
              "EditInForest", "edit_in_forest", ListsCB, "", true, false).getDimension(gdi, w, h);
    ypos += h + 3;
  }

  return ypos;
}

void TabList::generateList(gdioutput &gdi, bool forceUpdate)
{
  if (currentList.getListCode() == EFixedLiveResult) {
    liveResult(gdi, currentList);

    int baseY = 15;
    if (!gdi.isFullScreen()) {
      gdi.addButton(gdi.getWidth() + 20, baseY, gdi.scaleLength(120),
        "Cancel", ownWindow ? "Stäng" : "Återgå", ListsCB, "", true, false);

      baseY += 3 + gdi.getButtonHeight();
      gdi.addButton(gdi.getWidth() + 20, baseY, gdi.scaleLength(120),
        "FullScreenLive", "Fullskärm", ListsCB, "Visa listan i fullskärm", true, false);
    }
    SelectedList = "GeneralList";
    return;
  }

  DWORD storedWidth = 0;
  int oX = 0;
  int oY = 0;
  if (gdi.hasData("GeneralList")) {
    if (!forceUpdate && !currentList.needRegenerate(*oe))
      return;
    gdi.takeShownStringsSnapshot();
    oX = gdi.getOffsetX();
    oY = gdi.getOffsetY();
    gdi.getData("GeneralList", storedWidth);
    gdi.restoreNoUpdate("GeneralList");
  }
  else {
    gdi.clearPage(false);
    if (currentList.getParam().filterMaxPer > 0 && !ownWindow && !gdi.isFullScreen()) {
      gdi.addInfoBox("infofilter", L"Visar de X bästa#" + itow(currentList.getParam().filterMaxPer), 6000, 0);
    }
  }
  gdi.setRestorePoint("GeneralList");

  currentList.setCallback(ownWindow ? 0 : openRunnerTeamCB);
  const auto &par = currentList.getParam();
  int bgColor = par.bgColor;

  if (bgColor == -1 && par.screenMode == 1) {
    bgColor = RGB(255, 255, 255);
  }

  gdi.setColorMode(bgColor,
                   -1,
                   par.fgColor,
                   par.bgImage);
  try {
    oe->generateList(gdi, !noReEvaluate, currentList, false);
  }
  catch (const meosException &ex) {
    wstring err = lang.tl(ex.wwhat());
    gdi.addString("", 1, L"List Error: X#" + err).setColor(colorRed);
  }
  bool wasAnimation = false;
  if (par.screenMode == 1 && !par.lockUpdate) {
    setAnimationMode(gdi);
    wasAnimation = true;
  }
  else {
    gdi.setOffset(oX, oY, false);
  }

  int currentWidth = gdi.getWidth();
  gdi.setData("GeneralList", currentWidth);

  if (!hideButtons) {
    int extra = 0;
    if (currentList.getListCode() == EFixedInForest)
      extra = 1;

    int baseY = baseButtons(gdi, extra);

    if (!ownWindow) {
      gdi.addButton(gdi.getWidth()+20, baseY, gdi.scaleLength(120),
              "Window", "Eget fönster", ListsCB, "Öppna i ett nytt fönster.", true, false);

      gdi.addButton(gdi.getWidth()+20, baseY + 3 + 1*gdi.getButtonHeight(), gdi.scaleLength(120),
                    "Automatic", "Automatisera", ListsCB, "Skriv ut eller exportera listan automatiskt.", true, false);

      baseY += 2*(3+gdi.getButtonHeight());
    }
    
    if (currentList.supportUpdateClasses()) {
      baseY += 3 + gdi.getButtonHeight();
      gdi.addButton(gdi.getWidth() + 20, baseY, gdi.scaleLength(120),
                    "ClassSelection", "Klassval...", ListsCB, "Välj klasser", true, false);
    }
    baseY += 3 + gdi.getButtonHeight();
    gdi.addButton(gdi.getWidth() + 20, baseY, gdi.scaleLength(120),
      "ListDesign", "Utseende...", ListsCB, "Justera visningsinställningar", true, false);

    if (!currentList.getParam().saved && !oe->isReadOnly()) {
      baseY += 3 + gdi.getButtonHeight();
      gdi.addButton(gdi.getWidth()+20, baseY,  gdi.scaleLength(120),
                    "Remember", "Kom ihåg listan...", ListsCB, "Spara den här listan som en favoritlista", true, false);
    }
  }

  gdi.registerEvent("DataUpdate", ListsEventCB);
  gdi.setData("DataSync", 1);
  if (currentList.needPunchCheck() != oListInfo::PunchMode::NoPunch)
    gdi.setData("PunchSync", 1);
  gdi.registerEvent("GeneralList", ListsCB);
  gdi.setOnClearCb(ListsCB);
  SelectedList="GeneralList";

  if (!wasAnimation) {
    if (abs(int(currentWidth - storedWidth)) < 5) {
      gdi.refreshSmartFromSnapshot(true);
    }
    else
      gdi.refresh();
  }
}

int TabList::listCB(gdioutput &gdi, int type, void *data)
{
  int index;
  if (type==GUI_BUTTON || type==GUI_EVENT) {
    BaseInfo *tbi = 0;
    ButtonInfo button;
    EventInfo evnt;
    if (type == GUI_BUTTON) {
      button = *(ButtonInfo *)data;
      tbi = &button;
    }
    else if (type == GUI_EVENT) {
      evnt = *(EventInfo *)data;
      tbi = &evnt;
    }
    else
      throw 0;

    BaseInfo &bi=*tbi;

    if (bi.id=="Cancel") {
      if (ownWindow)
        gdi.closeWindow();
      else {
        gdioutput *gdi_settings = getExtraWindow("html_settings", false);
        if (gdi_settings) {
          gdi_settings->closeWindow();
        }
        gdi_settings = getExtraWindow("save_list", false);
        if (gdi_settings) {
          gdi_settings->closeWindow();
        }
        SelectedList = "";
        currentListType = EStdNone;
        loadPage(gdi);
      }
    }
    else if (bi.id=="Print") {
      gdi.print(oe, 0, true, false, currentList.getParam().pageBreak);
    }
    else if (bi.id=="Copy") {
      ostringstream fout;
      HTMLWriter::writeTableHTML(gdi, fout, L"MeOS", true, 0, 1.0);
      string res = fout.str();
      gdi.copyToClipboard(res, L"");
    }
    else if (bi.id=="PDF") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Portable Document Format (PDF)", L"*.pdf"));

      wstring file=gdi.browseForSave(ext, L"pdf", index);

      if (!file.empty()) {
        pdfwriter pdf;
        pdf.generatePDF(gdi, file, oe->getName() + L", " + currentList.getName(),
                                   oe->getDCI().getString("Organizer"), gdi.getTL(), 
                                   currentList.getParam().pageBreak);
        gdi.openDoc(file.c_str());
      }
    }
    else if (bi.id == "ClassSelection") {
      gdioutput *gdi_settings = getExtraWindow("list_class", true);
      if (!gdi_settings) {
        gdi_settings = createExtraWindow("list_class", lang.tl("Klassval"), gdi.scaleLength(350), gdi.scaleLength(600), true);
      }
      if (gdi_settings) {
        loadClassSettings(*gdi_settings, gdi.getTag());
      }
    }
    else if (bi.id == "ListDesign") {
      gdioutput *html_settings = getExtraWindow("html_settings", false);
      if (html_settings) {
        html_settings->closeWindow();
      }

      gdioutput *gdi_settings = getExtraWindow("list_settings", true);
      if (!gdi_settings) {
        gdi_settings = createExtraWindow("list_settings", lang.tl("Inställningar"), gdi.scaleLength(600), gdi.scaleLength(400));
      }
      if (gdi_settings) {
        loadSettings(*gdi_settings, gdi.getTag());
      }
    }
    else if (bi.id == "HTMLDesign") {
      gdioutput *design_settings = getExtraWindow("list_settings", false);
      if (design_settings) {
        design_settings->closeWindow();
      }

      gdioutput *gdi_settings = getExtraWindow("html_settings", true);
      if (!gdi_settings) {
        gdi_settings = createExtraWindow("html_settings", lang.tl("HTML Export"), gdi.scaleLength(600), gdi.scaleLength(500), true);
      }
      if (gdi_settings) {
        htmlSettings(*gdi_settings, gdi.getTag());
      }
    }
    else if (bi.id == "Window" || bi.id == "AutoScroll" ||
             bi.id == "FullScreen" || bi.id == "FullScreenLive") {
      gdioutput *gdi_new;
      TabList *tl_new = this;
      if (!ownWindow) {
        auto nw = makeOwnWindow(gdi);
        if (nw.first) {
          tl_new = nw.second;
          gdi_new = nw.first;
        }
      }
      else
        gdi_new = &gdi;

      gdioutput *gdi_settings = getExtraWindow("html_settings", false);
      if (gdi_settings) {
        gdi_settings->closeWindow();
      }

      if (gdi_new && bi.id == "AutoScroll" || bi.id == "FullScreen") {
        tl_new->hideButtons = true;
        
        gdi_new->alert("help:fullscreen");

        if (bi.id == "FullScreen")
          gdi_new->setFullScreen(true);

        int h = gdi_new->setHighContrastMaxWidth();
        tl_new->loadPage(*gdi_new);
        double sec = 6.0;
        double delta = h * 20. / (1000. * sec);
        gdi_new->setAutoScroll(delta);
      }
      else if (gdi_new && bi.id == "FullScreenLive") {
        gdi_new->setFullScreen(true);
        tl_new->hideButtons = true;
        tl_new->loadPage(*gdi_new);
      }
    }
    else if (bi.id == "Remember") {
      gdioutput *gdi_settings = getExtraWindow("save_list", true);
      if (!gdi_settings) {
        gdi_settings = createExtraWindow("save_list", lang.tl("Kom ihåg listan"), gdi.scaleLength(450), gdi.scaleLength(500), true);
      }
      if (gdi_settings) {
        loadRememberList(*gdi_settings, gdi.getTag());
      }
    }
    else if (bi.id == "ShowSaved") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("SavedInstance", lbi)) {
        oListParam &par = oe->getListContainer().getParam(lbi.data);

        oe->generateListInfo(par, gdi.getLineHeight(), currentList);
        currentList.getParam().sourceParam = lbi.data;
        generateList(gdi);
      }
    }
    else if (bi.id == "RenameSaved") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("SavedInstance", lbi)) {
        const oListParam &par = oe->getListContainer().getParam(lbi.data);
        gdi.clearPage(true);
        gdi.addString("", boldLarge, L"Döp om X#" + par.getName());
        gdi.setData("ParamIx", lbi.data);
        gdi.dropLine();
        gdi.fillRight();
        gdi.addInput("Name", par.getName(), 36);
        gdi.setInputFocus("Name", true);
        gdi.addButton("DoRenameSaved", "Döp om", ListsCB).setDefault();
        gdi.addButton("Cancel", "Avbryt", ListsCB).setCancel();
        gdi.dropLine(3);
      }
    }
    else if (bi.id == "DoRenameSaved") {
      int ix = int(gdi.getData("ParamIx"));
      oListParam &par = oe->getListContainer().getParam(ix);
      wstring name = gdi.getText("Name");
      par.setName(name);
      loadPage(gdi);
    }
    else if (bi.id == "MergeSaved") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("SavedInstance", lbi)) {
        //oe->getListContainer().mergeParam(0, lbi.data);
        const oListParam &par = oe->getListContainer().getParam(lbi.data);
        gdi.clearPage(true);
        gdi.addString("", boldLarge, L"Slå ihop X#" + par.getName());
        gdi.setData("ParamIx", lbi.data);
        gdi.dropLine();
        gdi.addListBox("Merge", 350, 250, 0, L"Slå ihop med:");
        vector < pair<wstring, size_t> > cand;
        oe->getListContainer().getMergeCandidates(lbi.data, cand);
        gdi.addItem("Merge", cand);
        gdi.addCheckbox("ShowTitle", "Visa rubrik mellan listorna", 0, false);
        gdi.fillRight();
        gdi.addButton("DoMerge", "Slå ihop", ListsCB).setDefault();
        gdi.addButton("Cancel", "Avbryt", ListsCB).setCancel();
        gdi.dropLine(3);
      }
    }
    else if (bi.id == "DoMerge") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("Merge", lbi)) {
        int mergeWidth = lbi.data;
        int base = (int)gdi.getData("ParamIx");
        oe->synchronize(false);
        bool showTitle = gdi.isChecked("ShowTitle");
        oe->getListContainer().mergeParam(mergeWidth, base, showTitle);
        oe->synchronize(true);
        loadPage(gdi);
      }
      return 0;
    }
    else if (bi.id == "SplitSaved") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("SavedInstance", lbi)) {
        oe->synchronize(false);
        oe->getListContainer().split(lbi.data);
        oe->synchronize(true);
        loadPage(gdi);
        return 0;
      }
    }
    else if (bi.id == "RemoveSaved") {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("SavedInstance", lbi)) {
        if (gdi.ask(L"Vill du ta bort 'X'?#" + lbi.text)) {
          oe->synchronize(false);
          oe->getListContainer().removeParam(lbi.data);
          oe->synchronize(true);
          loadPage(gdi);
        }
      }
      return 0;
    }
    else if (bi.id == "Automatic") {
      gdioutput *gdi_settings = getExtraWindow("html_settings", false);
      wstring htmlTarget;
      if (gdi_settings) {
        htmlTarget = lastHtmlTarget;
        gdi_settings->closeWindow();
      }
      PrintResultMachine prm(60*10, currentList);
      if (!htmlTarget.empty()) {
        prm.setHTML(htmlTarget, currentList.getParam().timePerPage / 1000);
      }
      tabAutoAddMachinge(prm);
      dynamic_cast<TabAuto *>(gdi.getTabs().get(TAutoTab))->loadPage(gdi, true);
    }
    else if (bi.id == "WideFormat") {
      enableWideFormat(gdi, gdi.isChecked(bi.id));
    }
    else if (bi.id=="SelectAll") {
      set<int> lst;
      lst.insert(-1);
      gdi.setSelection("ListSelection", lst);

      if (gdi.hasField("ResultType")) {
        ListBoxInfo entry;
        gdi.getSelectedItem("ResultType", entry);
        gdi.setInputStatus("Generate", int(entry.data) >= 0);
      }
    }
    else if (bi.id=="SelectNone") {
      set<int> lst;
      gdi.setSelection("ListSelection", lst);
      if (gdi.hasField("ResultType")) {
        gdi.setInputStatus("Generate", false);
      }
    }
    else if (bi.id=="CancelPS") {
      gdi.getTabs().get(TabType(bi.getExtraInt()))->loadPage(gdi);
    }
    else if (bi.id=="SavePS") {
      const char *ctype = (char *)gdi.getData("Type");
      saveExtraLines(*oe, ctype, gdi);

      if (gdi.hasField("SplitAnalysis")) {
        int aflag = (gdi.isChecked("SplitAnalysis") ? 0 : 1) + (gdi.isChecked("Speed") ? 0 : 2)
                    + (gdi.isChecked("Results") ? 0 : 4);
        oe->getDI().setInt("Analysis", aflag);
      }

       
       if (gdi.hasField("WideFormat")) {
         bool wide = gdi.isChecked("WideFormat");
         oe->setProperty("WideSplitFormat", wide);
    
         if (wide && gdi.hasField("NumPerPage")) {
           pair<int, bool> res = gdi.getSelectedItem("NumPerPage");
           if (res.second)
             oe->setProperty("NumSplitsOnePage", res.first);

           int no = gdi.getTextNo("MaxWaitTime"); 
           if (no >= 0)
             oe->setProperty("SplitPrintMaxWait", no);
         }
       }
      gdi.getTabs().get(TabType(bi.getExtraInt()))->loadPage(gdi);
    }
    else if (bi.id == "PrinterSetup") {
      ((TabSI *)gdi.getTabs().get(TSITab))->printerSetup(gdi);
    }
    else if (bi.id=="Generate") {
      ListBoxInfo lbi;
      bool advancedResults = false;
      if (gdi.getSelectedItem("ListType", lbi)) {
        currentListType=EStdListType(lbi.data);
      }
      else if (gdi.getSelectedItem("ResultType", lbi)) {
        currentListType = getTypeFromResultIndex(lbi.data);
        lastSelectedResultList = lbi.data;
        advancedResults = true;
      }
      else return 0;

      oListParam par;
      gdi.getSelectedItem("ResultSpecialTo", lbi);
      par.useControlIdResultTo = lbi.data;

      gdi.getSelectedItem("ResultSpecialFrom", lbi);
      par.useControlIdResultFrom = lbi.data;

      gdi.getSelectedItem("LegNumber", lbi);
      par.setLegNumberCoded(lbi.data);
      lastLeg = lbi.data;

      gdi.getSelection("ListSelection", par.selection);
      if (advancedResults)
        lastResultClassSelection = par.selection;
      par.filterMaxPer = gdi.getTextNo("ClassLimit");
      par.inputNumber = gdi.getTextNo("InputNumber");
      lastInputNumber = itow(par.inputNumber);

      par.pageBreak = gdi.isChecked("PageBreak");
      par.showHeader = gdi.isChecked("ShowHeader");

      par.listCode = (EStdListType)currentListType;
      par.showInterTimes = gdi.isChecked("ShowInterResults");
      par.showSplitTimes = gdi.isChecked("ShowSplits");
      par.splitAnalysis = gdi.isChecked("SplitAnalysis");
      par.setCustomTitle(gdi.getText("Title"));
      par.useLargeSize = gdi.isChecked("UseLargeSize");

      lastLimitPer = par.filterMaxPer;
      lastInterResult = par.showInterTimes;
      lastSplitState = par.showSplitTimes;
      lastLargeSize = par.useLargeSize;

      oe->generateListInfo(par, gdi.getLineHeight(), currentList);

      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="GeneralList") {
      if (SelectedList=="GeneralList") {
        generateList(gdi);
      }
      else
        loadGeneralList(gdi);
    }
    else if (bi.id == "EditList") {
      if (!listEditor)
        listEditor = new ListEditor(oe);

      gdi.clearPage(false);
      listEditor->show(gdi);
      gdi.refresh();
    }
    else if (bi.id == "EditMethod") {
      if (!methodEditor)
        methodEditor = new MethodEditor(oe);

      gdi.clearPage(false);
      methodEditor->show(gdi);
      gdi.refresh();
    }
    else if (bi.id=="ResultIndividual") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getResultIndividual(par, cnf);
      cnf.getIndividual(par.selection);
      readSettings(gdi, par, true);

      oe->generateListInfo(par, gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
     else if (bi.id=="ResultIndSplit") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getIndividual(par.selection);
      par.listCode = EStdResultList;
      
      readSettings(gdi, par, true);
      par.showSplitTimes = true;
      par.setLegNumberCoded(-1);
      
      oe->generateListInfo(par, gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="StartIndividual") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getStartIndividual(par, cnf);
      readSettings(gdi, par, false);

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="StartClub") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      getStartClub(par);
      readSettings(gdi, par, false);

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="ResultClub") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getResultClub(par, cnf);

      readSettings(gdi, par, false);
      par.splitAnalysis = gdi.isChecked("SplitAnalysis");
      
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="PreReport") {
      SelectedList=bi.id;
      gdi.clearPage(false);
      oe->generatePreReport(gdi);
      baseButtons(gdi, 0);
      gdi.refresh();
    }
    else if (bi.id=="InForestList") {
      SelectedList=bi.id;
      gdi.clearPage(false);

      gdi.registerEvent("DataUpdate", ListsEventCB);
      gdi.setData("DataSync", 1);
      gdi.registerEvent(bi.id, ListsCB);

      oe->generateInForestList(gdi, openRunnerTeamCB, NoStartRunnerCB);
      baseButtons(gdi, 1);
      gdi.refresh();
    }
    else if (bi.id=="TeamStartList") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getStartTeam(par, cnf);
      readSettings(gdi, par, false);

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="RaceNStart") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      int race = bi.getExtraInt();
      par.setLegNumberCoded(race);
      par.listCode = EStdIndMultiStartListLeg;
      readSettings(gdi, par, false);

      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getRaceNStart(race, par.selection);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="LegNStart") {
      oe->sanityCheck(gdi, false);
      oListParam par;
      readSettings(gdi, par, false);

      int race = bi.getExtraInt();
      par.setLegNumberCoded(race);
      par.listCode = EStdTeamStartListLeg;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getLegNStart(race, par.selection);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="TeamResults") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getResultTeam(par, cnf);
      readSettings(gdi, par, true); 
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="MultiResults") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      readSettings(gdi, par, true);
      par.listCode = EStdIndMultiResultListAll;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getRaceNRes(0, par.selection);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="RaceNRes") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      readSettings(gdi, par, true);

      int race = bi.getExtraInt();
      par.setLegNumberCoded(race);
      par.listCode = EStdIndMultiResultListLeg;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getRaceNRes(race, par.selection);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="LegNResult") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      readSettings(gdi, par, true); 
      int race = bi.getExtraInt();
      par.setLegNumberCoded(race);
      par.listCode = oe->getListContainer().getType("legresult");
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      cnf.getLegNRes(race, par.selection);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="RogainingResultList") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      getResultRogaining(par, cnf);
      readSettings(gdi, par, true);

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="CourseReport") {
      oe->sanityCheck(gdi, false);

      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      vector<oListParam> par;
      
      if (cnf.hasTeamClass()) {
        par.emplace_back();
        readSettings(gdi, par.back(), false);
        
        par.back().listCode = ETeamCourseList;
        cnf.getTeamClass(par.back().selection);
      }
      
      if (cnf.hasIndividual()) {
        par.emplace_back();
        readSettings(gdi, par.back(), false);

        par.back().listCode = EIndCourseList;
        par.back().showInterTitle = false;
        par.back().setLegNumberCoded(-1);
        cnf.getIndividual(par.back().selection);
      }

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="HiredCards") {
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      oListParam par;
      par.listCode = EStdRentedCard;
      par.showHeader = gdi.isChecked("ShowHeader");
      par.setLegNumberCoded(-1);
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="PriceList") {
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      oListParam par;
      cnf.getIndividual(par.selection);
      par.listCode = EIndPriceList;
      par.showHeader = gdi.isChecked("ShowHeader");
      par.filterMaxPer = gdi.getSelectedItem("ClassLimit").first;
      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id=="MinuteStartList") {
      oe->sanityCheck(gdi, false);
      SelectedList=bi.id;
      gdi.clearPage(false);

      gdi.registerEvent("DataUpdate", ListsEventCB);
      gdi.setData("DataSync", 1);
      gdi.registerEvent(bi.id, ListsCB);
      currentList.getParam().pageBreak = true;
      oe->generateMinuteStartlist(gdi);
      baseButtons(gdi, 0);
      gdi.refresh();
    }
    else if (bi.id=="ResultList") {
      settingsResultList(gdi);
    }
    else if (bi.id.substr(0, 7) == "Result:" ||
             bi.id.substr(0, 7) == "StartL:" || 
             bi.id.substr(0, 7) == "GenLst:") {
      bool isReport = bi.id.substr(0, 7) == "GenLst:";
      int baseType = bi.getExtraInt() & 0xFF;
      int flags = bi.getExtraInt() & 0xFF00;
      bool isStartList = bi.id.substr(0, 7) == "StartL:";
      bool allClasses = baseType == AddAllClasses;
      bool rogaining = baseType == AddRogainingClasses;
      bool patrol = baseType == AddPatrolClasses;
      bool team = baseType == AddTeamClasses;
      oe->sanityCheck(gdi, bi.id.substr(0, 7) == "Result:");
      oListParam par;
      par.listCode = oe->getListContainer().getType(bi.id.substr(7));
      readSettings(gdi, par, !isStartList);
      if ((flags & IgnoreLimitPer) == IgnoreLimitPer || isReport)
        par.filterMaxPer = 0;

      if ((flags & ForcePageBreak) == ForcePageBreak)
        par.pageBreak = true;

      par.setLegNumberCoded(-1);
      if (patrol) {
        ClassConfigInfo cnf;
        oe->getClassConfigurationInfo(cnf);
        cnf.getPatrol(par.selection);
      }
      else if (rogaining) {
        ClassConfigInfo cnf;
        oe->getClassConfigurationInfo(cnf);
        cnf.getRogaining(par.selection);
      }
      else if (team) {
        ClassConfigInfo cnf;
        oe->getClassConfigurationInfo(cnf);
        cnf.getRelay(par.selection);
      }
      else if (!isReport && !allClasses) {
        ClassConfigInfo cnf;
        oe->getClassConfigurationInfo(cnf);
        cnf.getIndividual(par.selection);
        cnf.getPatrol(par.selection);
      }

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id == "KnockoutTotal") {
      oe->sanityCheck(gdi, true);
      oListParam par;
      par.listCode = oe->getListContainer().getType("knockout-total");
      readSettings(gdi, par, true);
      par.setLegNumberCoded(-1);

      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      par.selection = set<int>(cnf.knockout.begin(), cnf.knockout.end());
      oe->generateListInfo(par, gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id == "LapCount" || bi.id == "LapCountExtra") {
      oe->sanityCheck(gdi, true);
      bool extra = bi.id == "LapCountExtra";
      oListParam par;
      par.listCode = oe->getListContainer().getType(extra ? "lapcountextra" : "lapcount");
      readSettings(gdi, par, true);
      par.setLegNumberCoded(-1);

      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      if (extra)
        par.selection = set<int>(cnf.lapcountextra.begin(), cnf.lapcountextra.end());
      else
        par.selection = set<int>(cnf.lapcountsingle.begin(), cnf.lapcountsingle.end());

      oe->generateListInfo(par, gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id == "CustomList") {
      oe->synchronize();
      oe->sanityCheck(gdi, false);
      oListParam par;
      int index = bi.getExtraInt();
      par.listCode = oe->getListContainer().getType(index);
      readSettings(gdi, par, true);
      par.setLegNumberCoded(-1);
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);

      oListInfo::EBaseType type = oe->getListContainer().getList(index).getListType();
      if (oListInfo::addRunners(type))
        cnf.getIndividual(par.selection);
      if (oListInfo::addPatrols(type))
        cnf.getPatrol(par.selection);
      if (oListInfo::addTeams(type))
        cnf.getTeamClass(par.selection);

      oe->generateListInfo(par,  gdi.getLineHeight(), currentList);
      currentList.setCallback(openRunnerTeamCB);
      generateList(gdi);
      gdi.refresh();
    }
    else if (bi.id == "ImportCustom") {

      MetaListContainer &lc = oe->getListContainer();
      vector< pair<wstring, int> >  installedLists;
      set<string> installedId;
      for (int k = 0; k < lc.getNumLists(); k++) {
        if (lc.isExternal(k)) {
          MetaList &mc = lc.getList(k);
          installedLists.push_back(make_pair(mc.getListName(), k));
          mc.initUniqueIndex();
          if (!mc.getUniqueId().empty())
            installedId.insert(mc.getUniqueId());
        }
      }

      vector< pair<wstring, pair<string, wstring> > > lists;
      lc.enumerateLists(lists);
      if (lists.empty() && installedLists.empty()) {
        bi.id = "BrowseList";
        return listCB(gdi, GUI_BUTTON, &bi);
      }


      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Tillgängliga listor");
      int xx = gdi.getCX() + gdi.scaleLength(360);
      int bx = gdi.getCX();
      if (!installedLists.empty()) {
        gdi.dropLine();
        gdi.addString("", 1, "Listor i tävlingen");
        gdi.fillRight();
        gdi.pushX();
        for (size_t k = 0; k < installedLists.size(); k++) {
          gdi.addStringUT(0, installedLists[k].first).setColor(colorDarkGreen);
          gdi.setCX(xx);
          gdi.addString("RemoveInstalled", 0, "Ta bort", ListsCB).setExtra(installedLists[k].second);
          gdi.addString("EditInstalled", 0, "Redigera", ListsCB).setExtra(installedLists[k].second);
          gdi.dropLine();
          if (k+1 < installedLists.size()) {
            RECT rc = {bx, gdi.getCY(),gdi.getCX(),gdi.getCY()+1};
            gdi.addRectangle(rc, colorDarkBlue);
            gdi.dropLine(0.1);
          }
          gdi.popX();
        }
        gdi.fillDown();
        gdi.popX();
      }

      if (!lists.empty()) {
        gdi.dropLine(2);
        gdi.addString("", 1, "Installerbara listor");
        gdi.fillRight();
        gdi.pushX();

        for (size_t k = 0; k < lists.size(); k++) {
          gdi.addStringUT(0, lists[k].first, 0);
          if (!installedId.count(lists[k].second.first)) {
            gdi.setCX(xx);
            gdi.addString("CustomList", 0, "Lägg till", ListsCB).setColor(colorDarkGreen).setExtra(k);
            gdi.addString("RemoveList", 0, "Radera permanent", ListsCB).setColor(colorDarkRed).setExtra(k);
          }
          gdi.dropLine();

          if (k+1 < lists.size() && !installedId.count(lists[k].second.first)) {
            RECT rc = {bx, gdi.getCY(),gdi.getCX(),gdi.getCY()+1};
            gdi.addRectangle(rc, colorDarkBlue);
            gdi.dropLine(0.1);
          }
          gdi.popX();

        }
        gdi.fillDown();
        gdi.popX();
      }

      gdi.dropLine(2);
      gdi.fillRight();
      gdi.addButton("BrowseList", "Bläddra...", ListsCB);
      gdi.addButton("Cancel", "Återgå", ListsCB).setCancel();
      gdi.refresh();
    }
    else if (bi.id == "BrowseList") {
      vector< pair<wstring, wstring> > filter;
      filter.push_back(make_pair(L"xml-data", L"*.xml;*.meoslist"));
      wstring file = gdi.browseForOpen(filter, L"xml");
      if (!file.empty()) {
        xmlparser xml;
        xml.read(file);
        xmlobject xlist = xml.getObject(0);
        oe->synchronize();
        oe->getListContainer().load(MetaListContainer::ExternalList, xlist, false);
        oe->synchronize(true);

        loadPage(gdi);
      }
    }
    else if (bi.id == "EditInForest") {
      TabRunner &rt = dynamic_cast<TabRunner &>(*gdi.getTabs().get(TRunnerTab));
      rt.showInForestList(gdi);
    }
    else if (bi.id == "SplitAnalysis") {
      oe->setProperty("splitanalysis", gdi.isChecked(bi.id));
    }
    else if (bi.id == "PageBreak") {
      oe->setProperty("pagebreak", gdi.isChecked(bi.id));
    }
    else if (bi.id == "ShowHeader") {
      oe->setProperty("showheader", gdi.isChecked(bi.id));
    }
    else if (bi.id == "ShowInterResults"){
      oe->setProperty("intertime", gdi.isChecked(bi.id));
    }
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo lbi=*(ListBoxInfo *)data;

    if (lbi.id == "NumPerPage") {
      enableWideFormat(gdi, true);
    }
    else if (lbi.id == "SavedInstance") {
      int ix = lbi.data;
      bool split = oe->getListContainer().canSplit(ix);
      gdi.setInputStatus("SplitSaved", split);
    }
    else if (lbi.id=="ListType"){
      EStdListType type = EStdListType(lbi.data);
      selectGeneralList(gdi, type);
    }
    else if (lbi.id == "ListSelection") {
      gdi.getSelection(lbi.id, lastClassSelection);
      if (gdi.hasField("ResultType")) {
        ListBoxInfo entry;
        gdi.getSelectedItem("ResultType", entry);
        gdi.setInputStatus("Generate", !lastClassSelection.empty() && int(entry.data) >= 0);
      }
    }
    else if (lbi.id == "ClassLimit"){
      oe->setProperty("classlimit", lbi.data);
    }
    else if (lbi.id=="ResultType") {
      setResultOptionsFromType(gdi, lbi.data);
    }
    else if (lbi.id=="ResultSpecialTo") {
      oe->setProperty("ControlTo", lbi.data);
    }
    else if (lbi.id=="ResultSpecialFrom") {
      oe->setProperty("ControlFrom", lbi.data);
    }
  }
  else if (type==GUI_CLEAR) {
    offsetY=gdi.getOffsetY();
    offsetX=gdi.getOffsetX();
    leavingList(gdi.getTag());
    return true;
  }
  else if (type == GUI_LINK) {
    TextInfo ti = *(TextInfo *)data;
    if (ti.id == "CustomList") {
      vector< pair<wstring, pair<string, wstring> > > lists;
      oe->getListContainer().enumerateLists(lists);
      size_t ix = ti.getExtraSize();
      if (ix < lists.size()) {
        xmlparser xml;
        xml.read(lists[ix].second.second);
        xmlobject xlist = xml.getObject(0);

        oe->synchronize(false);
        oe->getListContainer().load(MetaListContainer::ExternalList, xlist, false);
        oe->synchronize(true);
        oe->loadGeneralResults(true);
      }
      ButtonInfo bi;
      bi.id = "ImportCustom";
      listCB(gdi, GUI_BUTTON, &bi);

    }
    else if (ti.id == "RemoveList") {
      vector< pair<wstring, pair<string, wstring> > > lists;
      oe->getListContainer().enumerateLists(lists);
      size_t ix = ti.getExtraSize();
      if (ix < lists.size()) {
        if (gdi.ask(L"Vill du ta bort 'X'?#" + lists[ix].first)) {
          DeleteFile(lists[ix].second.second.c_str());
        }
      }
      ButtonInfo bi;
      bi.id = "ImportCustom";
      listCB(gdi, GUI_BUTTON, &bi);
    }
    else if (ti.id == "RemoveInstalled") {
      int ix = ti.getExtraInt();
      if (gdi.ask(L"Vill du ta bort 'X'?#" + oe->getListContainer().getList(ix).getListName())) {

        oe->synchronize(false);
        oe->getListContainer().removeList(ix);
        oe->synchronize(true);

        ButtonInfo bi;
        bi.id = "ImportCustom";
        listCB(gdi, GUI_BUTTON, &bi);
      }
    }
    else if (ti.id == "EditInstalled") {
      int ix = ti.getExtraInt();
      if (!listEditor)
        listEditor = new ListEditor(oe);
      gdi.clearPage(false);
      listEditor->load(oe->getListContainer(), ix);
      listEditor->show(gdi);
      gdi.refresh();
    }
  }

  return 0;
}

pair<gdioutput *, TabList *> TabList::makeOwnWindow(gdioutput &gdi) {
  gdioutput *gdi_new = createExtraWindow(uniqueTag("list"), makeDash(L"MeOS - ") + currentList.getName(), gdi.getWidth() + 64 + gdi.scaleLength(120));
  TabList *tl_new = 0;
  if (gdi_new) {
    TabList &tl = dynamic_cast<TabList &>(*gdi_new->getTabs().get(TListTab));
    tl.currentList = currentList;
    tl.SelectedList = SelectedList;
    tl.ownWindow = true;
    tl.loadPage(*gdi_new);
    tl_new = &tl;
    changeListSettingsTarget(gdi, *gdi_new);
    SelectedList = "";
    currentList = oListInfo();
    loadPage(gdi);
  }
  return make_pair(gdi_new, tl_new);
}

void TabList::enableFromTo(oEvent &oe, gdioutput &gdi, bool from, bool to) {
  vector< pair<wstring, size_t> > d;
  oe.fillControls(d, oEvent::CTCourseControl);

  if (from) {
    gdi.enableInput("ResultSpecialFrom");
    vector< pair<wstring, size_t> > ds;
    ds.push_back(make_pair(lang.tl("Start"), 0));
    ds.insert(ds.end(), d.begin(), d.end());
    gdi.addItem("ResultSpecialFrom", ds);
    if (!gdi.selectItemByData("ResultSpecialFrom", oe.getPropertyInt("ControlFrom", 0))) {
      gdi.selectItemByData("ResultSpecialFrom", 0); // Fallback
    }
  }
  else {
    gdi.clearList("ResultSpecialFrom");
    gdi.disableInput("ResultSpecialFrom");
  }

  if (to) {
    gdi.enableInput("ResultSpecialTo");
    gdi.addItem("ResultSpecialTo", d);
    gdi.addItem("ResultSpecialTo", lang.tl("Mål"), 0);
    if (!gdi.selectItemByData("ResultSpecialTo", oe.getPropertyInt("ControlTo", 0))) {
      gdi.selectItemByData("ResultSpecialTo", 0); // Fallback
    }
  }
  else {
    gdi.clearList("ResultSpecialTo");
    gdi.disableInput("ResultSpecialTo");
  }
}

void TabList::selectGeneralList(gdioutput &gdi, EStdListType type)
{
  oListInfo li;
  oe->getListType(type, li);
  oe->setProperty("ListType", type);
  if (li.supportClasses) {
    gdi.enableInput("ListSelection");
    oe->fillClasses(gdi, "ListSelection", oEvent::extraNone, oEvent::filterNone);
    if (lastClassSelection.empty())
      lastClassSelection.insert(-1);
    gdi.setSelection("ListSelection", lastClassSelection);
  }
  else {
    gdi.clearList("ListSelection");
    gdi.disableInput("ListSelection");
  }

  gdi.setInputStatus("UseLargeSize", li.supportLarge);
  gdi.setInputStatus("InputNumber", li.supportParameter);
  
  gdi.setInputStatus("SplitAnalysis", li.supportSplitAnalysis);
  gdi.setInputStatus("ShowInterResults", li.supportInterResults);
  gdi.setInputStatus("PageBreak", li.supportPageBreak);
  gdi.setInputStatus("ClassLimit", li.supportClassLimit);
  //gdi.setInputStatus("Title", li.supportCustomTitle);
  
  if (li.supportLegs) {
    //gdi.enableInput("LegNumber");
    //oe->fillLegNumbers(gdi, "LegNumber", li.isTeamList(), true);
    set<int> clsUnused;
    vector< pair<wstring, size_t> > out;
    oe->fillLegNumbers(clsUnused, li.isTeamList(), true, out);
    gdi.addItem("LegNumber", out);
    gdi.setInputStatus("LegNumber", !out.empty());    
  }
  else {
    gdi.disableInput("LegNumber");
    gdi.clearList("LegNumber");
  }

  enableFromTo(*oe, gdi, li.supportFrom, li.supportTo);
}

void TabList::makeClassSelection(gdioutput &gdi) {
  gdi.fillDown();
  gdi.addListBox("ListSelection", 250, 300, ListsCB, L"Urval:", L"", true);

  gdi.pushX();
  gdi.fillRight();
  gdi.dropLine(0.5);

  gdi.addButton("SelectAll", "Välj allt", ListsCB);
  gdi.addButton("SelectNone", "Välj inget", ListsCB);
  gdi.popX();
}

void TabList::loadGeneralList(gdioutput &gdi)
{
  oe->sanityCheck(gdi, false);
  gdi.fillDown();
  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Skapa generell lista");
  gdi.dropLine(0.8);
  gdi.pushY();
  gdi.addSelection("ListType", 250, 300, ListsCB, L"Lista:");
  oe->fillListTypes(gdi, "ListType", 0);

  makeClassSelection(gdi);
  
  gdi.setCX(gdi.scaleLength(290)+30);
  gdi.pushX();
  gdi.popY();
  gdi.fillDown();
  gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser / klubbar", ListsCB, oe->getPropertyInt("pagebreak", 0)!=0);
  gdi.addCheckbox("ShowHeader", "Visa rubrik", ListsCB, oe->getPropertyInt("showheader", 0) != 0);

  gdi.addCheckbox("ShowInterResults", "Visa mellantider", ListsCB, oe->getPropertyInt("intertime", 1)!=0, "Mellantider visas för namngivna kontroller.");
  gdi.addCheckbox("SplitAnalysis", "Med sträcktidsanalys", ListsCB, oe->getPropertyInt("splitanalysis", 1)!=0);
  gdi.addCheckbox("UseLargeSize", "Använd stor font", 0, lastLargeSize);

  if (lastLimitPer == -1) {
    lastLimitPer = oe->getPropertyInt("classlimit", 0);
  }
  wstring lastClassLimit;
  if (lastLimitPer > 0)
    lastClassLimit = itow(lastLimitPer);
  
  gdi.addInput("ClassLimit", lastClassLimit, 5, 0, L"Begränsa antal per klass:");
  gdi.dropLine();

  makeFromTo(gdi);
  /*gdi.fillRight();
  gdi.pushX();
  gdi.addSelection("ResultSpecialFrom", 140, 300, ListsCB, "Från kontroll:");
  gdi.disableInput("ResultSpecialFrom");

  gdi.addSelection("ResultSpecialTo", 140, 300, ListsCB, "Till kontroll:");
  gdi.disableInput("ResultSpecialTo");

  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);
  */
  gdi.addSelection("LegNumber", 140, 300, ListsCB, L"Sträcka:");
  gdi.disableInput("LegNumber");

  gdi.addInput("InputNumber", lastInputNumber, 5, 0, L"Listparameter:", L"Ett värde vars tolkning beror på listan.");
  gdi.disableInput("InputNumber");
  gdi.popX();

  if (oe->getPropertyInt("ListType", 0) > 0) {
    gdi.selectItemByData("ListType", oe->getPropertyInt("ListType", 0));
    selectGeneralList(gdi, EStdListType(oe->getPropertyInt("ListType", 0)));
  }


  gdi.dropLine(3);
  gdi.addInput("Title", L"", 32, ListsCB, L"Egen listrubrik:");

  gdi.dropLine();
  gdi.fillRight();
  gdi.addButton("Generate", "Generera", ListsCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", ListsCB).setCancel();

  gdi.refresh();
}

static int getListIx(const map<string, int> &tag2ListIx, 
                     set<int> &usedListIx, 
                     const char *tag, int fallback) {
  map<string, int>::const_iterator res = tag2ListIx.find(tag);
  if (res != tag2ListIx.end()) {
    usedListIx.insert(res->second);
    return res->second;
  }
  return fallback;
}

void TabList::makeFromTo(gdioutput &gdi) {
  gdi.fillRight();
  gdi.pushX();

  gdi.addSelection("ResultSpecialFrom", 140, 300, ListsCB, L"Från kontroll:");
  gdi.disableInput("ResultSpecialFrom");

  gdi.addSelection("ResultSpecialTo", 140, 300, ListsCB, L"Till kontroll:");
  gdi.disableInput("ResultSpecialTo");

  gdi.popX();
  gdi.dropLine(3);
}

class TargetSettings : public GuiHandler {
  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) final {
    string target;
    if (!gdi.getData("target", target))
      return;
    gdioutput *dest_gdi = getExtraWindow(target, false);
    if (!dest_gdi)
      return;

    TabBase *tb = dest_gdi->getTabs().get(TabType::TListTab);
    TabList *list = dynamic_cast<TabList *>(tb);

    if (list) {
      handle(list, gdi, info, type, *dest_gdi);
    }
  }
public:
  virtual void handle(TabList *list, gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) = 0;
  virtual ~TargetSettings() {}
};

class HTMLSettings : public TargetSettings {
  void handle(TabList *list, gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) override {
    list->handleHTMLSettings(gdi, info, type, dest_gdi);
  }
};


class ListSettings : public TargetSettings {
  void handle(TabList *list, gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) override {
    list->handleListSettings(gdi, info, type, dest_gdi);
  }
};

class ClassSettings : public TargetSettings {
public:
  void handle(TabList *list, gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) override {
    list->handleClassSettings(gdi, info, type, dest_gdi);
  }
};

class RememberList : public TargetSettings {
public:
  void handle(TabList *list, gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) override {
    list->handleRememberSettings(gdi, info, type, dest_gdi);
  }
};

HTMLSettings htmlClass;
ListSettings settingsClass;
ClassSettings settingsClassSelection;
RememberList settingsRememberList;

void TabList::changeListSettingsTarget(gdioutput &oldWindow, gdioutput &newWindow) {
  gdioutput *gdi_settings = getExtraWindow("list_settings", true);
  if (gdi_settings) {
    string oldTag;
    gdi_settings->getData("target", oldTag);
    if (oldWindow.getTag() == oldTag)
      gdi_settings->setData("target", newWindow.getTag());
  }
}

void TabList::leavingList(const string &wnd) {
  gdioutput *gdi_settings = getExtraWindow("list_settings", false);
  if (gdi_settings) {
    string oldTag;
    gdi_settings->getData("target", oldTag);
    if (wnd == oldTag)
      gdi_settings->closeWindow();
  }

  gdi_settings = getExtraWindow("html_settings", false);
  if (gdi_settings) {
    string oldTag;
    gdi_settings->getData("target", oldTag);
    if (wnd == oldTag)
      gdi_settings->closeWindow();
  }
  
  gdi_settings = getExtraWindow("save_list", false);
  if (gdi_settings) {
    string oldTag;
    gdi_settings->getData("target", oldTag);
    if (wnd == oldTag)
      gdi_settings->closeWindow();
  }
}

void TabList::loadRememberList(gdioutput &gdi, string targetTag) {
  gdi.clearPage(false);
  gdi.setCX(10);
  gdi.setCY(15);
  gdi.setColorMode(RGB(242, 240, 250));
  gdi.setData("target", targetTag);
  settingsTarget = targetTag;
  gdi.addString("", fontMediumPlus, L"Spara 'X'#" + currentList.getName());
  gdi.dropLine(0.5);
  oListParam &par = currentList.getParam();
  wstring baseName = par.getDefaultName();

  if (!par.selection.empty()) {
    vector<pClass> classes;
    oe->getClasses(classes, false);
    if (par.selection.size() != classes.size()) {
      wstring cls;
      for (pClass c : classes) {
        if (par.selection.count(c->getId())) {
          if (cls.length() > 16) {
            cls += L"...";
            break;
          }
          if (!cls.empty())
            cls += L", ";
          cls += c->getName();
        }
      }
      baseName += L"; " + cls;
    }
  }
  if (!par.getLegName().empty())
    baseName += L"; " + lang.tl(L"Sträcka X#" + par.getLegName());
  
  gdi.addInput("Name", baseName, 40, 0, L"Namn:");
  gdi.dropLine();

  gdi.addCheckbox("DoMerge", "Slå ihop med befintlig lista", nullptr, false).setHandler(&settingsRememberList);
  gdi.addListBox("Merge", 350, 250);
  vector < pair<wstring, size_t> > cand;
  oe->getListContainer().getMergeCandidates(-1, cand);
  gdi.addItem("Merge", cand);
  gdi.dropLine(-0.2);
  gdi.addCheckbox("ShowTitle", "Visa rubrik mellan listorna", 0, false);
  gdi.dropLine(0.5);
  gdi.disableInput("Merge");
  gdi.disableInput("ShowTitle");
  gdi.fillRight();

  gdi.pushX();
  gdi.addButton("SaveList", "OK").setDefault().setHandler(&settingsRememberList);
  gdi.addButton("Cancel", "Avbryt").setCancel().setHandler(&settingsRememberList);
  gdi.dropLine(3);
  gdi.popX();

  gdi.refresh();
}

void TabList::handleRememberSettings(gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) {
  if (type == GUI_BUTTON) {
    ButtonInfo bi = static_cast<ButtonInfo&>(info);
    if (bi.id == "Cancel")
      gdi.closeWindow();
    else if (bi.id == "SaveList") {
      oListParam &par = currentList.getParam();
      //wstring baseName = par.getDefaultName();
      wstring baseName = gdi.getText("Name");
      baseName = oe->getListContainer().makeUniqueParamName(baseName);
      par.setName(baseName);
      oe->synchronize(false);
      par.sourceParam = oe->getListContainer().addListParam(par);
      
      if (gdi.isChecked("DoMerge")) {
        ListBoxInfo lbi;
        if (gdi.getSelectedItem("Merge", lbi)) {
          int mergeWidth = lbi.data;
          bool showTitle = gdi.isChecked("ShowTitle");
          oe->getListContainer().mergeParam(mergeWidth, par.sourceParam, showTitle);
        }
      }
      oe->synchronize(true);

      dest_gdi.removeControl("Remember");
      gdi.closeWindow();
    }
    else if (bi.id == "DoMerge") {
      bool merge = gdi.isChecked(bi.id);
      gdi.setInputStatus("Merge", merge);
      gdi.setInputStatus("ShowTitle", merge);
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo lbi = static_cast<ListBoxInfo&>(info);
  }
}



static void addAnimationSettings(gdioutput &gdi, oListParam &dst) {
  DWORD cx, cy;
  gdi.getData("xmode", cx);
  gdi.getData("ymode", cy);

  gdi.setCX(cx);
  gdi.setCY(cy);
  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("Time", itow(dst.timePerPage), 5, 0, L"Visningstid:");
  gdi.addSelection("NPage", 70, 200, 0, L"Sidor per skärm:");
  for (int i = 1; i <= 8; i++)
    gdi.addItem("NPage", itow(i), i);
  if (dst.nColumns == 0)
    dst.nColumns = 1;
  gdi.selectItemByData("NPage", dst.nColumns);
  gdi.addInput("Margin", itow(dst.margin) + L" %", 5, 0, L"Marginal:");
  gdi.dropLine(1);
  gdi.addCheckbox("Animate", "Animation", 0, dst.animate);
}

static void saveAnimationSettings(gdioutput &gdi, oListParam &dst) {
  dst.timePerPage = gdi.getTextNo("Time");
  dst.nColumns = gdi.getSelectedItem("NPage").first;
  dst.animate = gdi.isChecked("Animate");
  dst.margin = gdi.getTextNo("Margin");
}

void TabList::loadSettings(gdioutput &gdi, string targetTag) {
  gdi.clearPage(false);
  gdi.setCX(10);
  gdi.setCY(15);
  gdi.setColorMode(RGB(242, 240, 250));
  gdi.setData("target", targetTag);
  settingsTarget = targetTag;
  gdi.addString("", fontMediumPlus, L"Visningsinställningar för 'X'#" + currentList.getName());

  gdi.dropLine(0.5);
  gdi.addSelection("Background", 200, 100, 0, L"Bakgrund:").setHandler(&settingsClass);
  gdi.addItem("Background", lang.tl("Standard"), 0);
  gdi.addItem("Background", lang.tl("Färg"), 1);
  //gdi.addItem("Background", lang.tl("Bild"), 2);
  tmpSettingsParam = currentList.getParam();
  int bgColor = currentList.getParam().bgColor;
  int fgColor = currentList.getParam().fgColor;
  bool useColor = bgColor != -1;
  gdi.selectItemByData("Background", useColor ? 1 : 0);
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("BGColor", "Bakgrundsfärg...").setHandler(&settingsClass).setExtra(bgColor);
  gdi.setInputStatus("BGColor", useColor);
  gdi.addButton("FGColor", "Textfärg...").setHandler(&settingsClass).setExtra(fgColor);
 
  gdi.popX();

  gdi.dropLine(3);
  gdi.addSelection("Mode", 200, 100, 0, L"Visning:").setHandler(&settingsClass);
  gdi.addItem("Mode", lang.tl("Fönster"), 0);
  gdi.addItem("Mode", lang.tl("Fönster (rullande)"), 3);
  gdi.addItem("Mode", lang.tl("Fullskärm (sidvis)"), 1);
  gdi.addItem("Mode", lang.tl("Fullskärm (rullande)"), 2);
  gdi.selectItemByData("Mode", tmpSettingsParam.screenMode);
  gdi.popX();
  gdi.dropLine(3);

  gdi.setData("xmode", gdi.getCX());
  gdi.setData("ymode", gdi.getCY());
  gdi.dropLine(3);

  gdi.addButton("ApplyList", "Verkställ").setHandler(&settingsClass);
  
  if (tmpSettingsParam.screenMode == 1)
    addAnimationSettings(gdi, tmpSettingsParam);
  
  RECT rc;
  rc.left = gdi.getWidth() + gdi.scaleLength(80);
  rc.right = rc.left + gdi.scaleLength(150);
  rc.top = 20;
  
  gdi.addString("", rc.top, rc.left, 1, "Exempel");
  rc.top += (gdi.getLineHeight() * 3) / 2;
  
  rc.bottom = rc.top + gdi.scaleLength(200);
  gdi.addRectangle(rc, bgColor != -1 ? GDICOLOR(bgColor) : GDICOLOR(colorTransparent)).id = "Background";
  string val = "123. Abc MeOS";
  int key = rand()%12;
  for (int i = 0; i < NUMTEXTSAMPLE; i++) {
    gdi.addString("Sample" + itos(i), rc.top + 3 + gdi.getLineHeight()*i, 
                 rc.left + 3 + 5*i, i == 0 ? boldText : normalText, "#" + val).setColor(GDICOLOR(fgColor));
    string val2 = val;
    for (int j = 0; j < 13; j++) {
      val2[j] = val[((j+1)*(key+1)) % 13];
    }
    val = val2;
  }
  gdi.refresh();
}

void TabList::handleListSettings(gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) {
  if (type == GUI_BUTTON) {
    ButtonInfo bi = static_cast<ButtonInfo&>(info);
    if (bi.id == "BGColor") {
      wstring c = oe->getPropertyString("Colors", L"");
      int res = gdi.selectColor(c, bi.getExtraInt());
      if (res > -1) {
        info.setExtra(res);
        oe->setProperty("Colors", c);
        RectangleInfo &rc = gdi.getRectangle("Background");
        rc.setColor(GDICOLOR(res));
        gdi.refreshFast();
      }
    }
    else if (bi.id == "FGColor") {
      wstring c = oe->getPropertyString("Colors", L"");
      int inC = bi.getExtraInt();
      if (inC == -1)
        inC = RGB(255,255,255);
      int res = gdi.selectColor(c, inC);
      if (res > -1) {
        info.setExtra(res);
        oe->setProperty("Colors", c);
        for (int i = 0; i < NUMTEXTSAMPLE; i++) {
          BaseInfo &bi = gdi.getBaseInfo(("Sample" + itos(i)).c_str());
          dynamic_cast<TextInfo &>(bi).setColor(GDICOLOR(res));
        }
        gdi.refreshFast();
      }
    }
    else if (bi.id == "ApplyList") {
      oListParam &param = currentList.getParam();
      param.lockUpdate = true;
      int type = gdi.getSelectedItem("Background").first;
      if (type == 1) 
        param.bgColor = gdi.getExtraInt("BGColor");
      else
        param.bgColor = -1;

      param.fgColor = gdi.getExtraInt("FGColor");
      param.screenMode = gdi.getSelectedItem("Mode").first;
      if (param.screenMode == 1) {
        saveAnimationSettings(gdi, param);
      }
      TabList *dest = this;
      gdioutput *dgdi = &dest_gdi;
      int mode = param.screenMode;
      if (param.screenMode == 2 || param.screenMode == 3) {
        dgdi->alert("help:fullscreen");
      }

      if ((mode==1 || mode==2) && !dest_gdi.isFullScreen()) {
        // Require fullscreen
        if (!ownWindow) {
          auto nw = makeOwnWindow(dest_gdi);
          dest = nw.second;
          dgdi = nw.first;
        }
        dgdi->setFullScreen(true);
        dest->hideButtons = true;
      }
      else if ((mode == 0 || mode == 3) && dest_gdi.isFullScreen()) {
        dest_gdi.setFullScreen(false);
        hideButtons = false;
      }

      if (mode == 2 || mode == 3) {
        if (!dest->ownWindow) {
          auto nw = makeOwnWindow(dest_gdi);
          dest = nw.second;
          dgdi = nw.first;
        }
        dest->hideButtons = true;
        int h = dgdi->setHighContrastMaxWidth();
        dest->loadPage(*dgdi);
        double sec = 6.0;
        double delta = h * 20. / (1000. * sec);
        dgdi->setAutoScroll(delta);
      }
      else {
        dest->loadPage(*dgdi);
      }
      dest->currentList.getParam().lockUpdate = false;
      param.lockUpdate = false;

      SetForegroundWindow(dgdi->getHWNDMain());
      SetWindowPos(dgdi->getHWNDMain(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
      
      if (mode == 1) {
        dest->setAnimationMode(*dgdi);
        dgdi->refresh();
        dest->generateList(*dgdi, true);
      }

      if (param.screenMode == 2 || param.screenMode == 3) {
        gdi.closeWindow();
      }
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo lbi = static_cast<ListBoxInfo&>(info);
    if (lbi.id == "Background") {
      gdi.setInputStatus("BGColor", lbi.data == 1);
      BaseInfo &bi = gdi.getBaseInfo("BGColor");
      if (lbi.data == 1 && bi.getExtraInt() == -1)
        bi.setExtra(int(RGB(255,255,255)));

      RectangleInfo &rc = gdi.getRectangle("Background");
      rc.setColor(GDICOLOR(lbi.data == 1 ? bi.getExtraInt() : colorTransparent));
      gdi.refreshFast();
    }
    else if (lbi.id == "Mode") {
      if (lbi.data == 1) {
        addAnimationSettings(gdi, tmpSettingsParam);
      }
      else {
        if (gdi.hasField("Time")) 
          saveAnimationSettings(gdi, tmpSettingsParam);

        gdi.removeControl("Time");
        gdi.removeControl("NPage");
        gdi.removeControl("Margin");
        gdi.removeControl("Animate");
      }
      gdi.refresh();
    }
  }
}

namespace {
  void htmlDetails(gdioutput &gdi, oListParam &tmpSettingsParam, wstring &info, bool withExtra) {
    gdi.restoreNoUpdate("htmlDetails");
    gdi.setRestorePoint("htmlDetails");
    gdi.pushX();
    if (withExtra) {
      gdi.fillDown();
      gdi.addString("", 0, info);
      gdi.dropLine(0.3);
      gdi.fillRight();
      gdi.addInput("Margin", itow(tmpSettingsParam.margin) + L" %", 5, 0, L"Marginal:").setHandler(&htmlClass);
      gdi.addInput("Scale", itow(int(tmpSettingsParam.htmlScale*100)) + L" %", 5, 0, L"Skalfaktor:").setHandler(&htmlClass);
      if (tmpSettingsParam.nColumns <= 0)
        tmpSettingsParam.nColumns = 1;

      gdi.addInput("Columns", itow(tmpSettingsParam.nColumns), 5, 0, L"Kolumner:").setHandler(&htmlClass);
      gdi.addInput("Time", itow(tmpSettingsParam.timePerPage) + L"ms", 5, 0, L"Visningstid:").setHandler(&htmlClass);

      gdi.popX();
      gdi.dropLine(3.4);

      gdi.addCheckbox("UseRows", "Begränsa antal rader per sida", 0, tmpSettingsParam.htmlRows>0).setHandler(&htmlClass);
      gdi.dropLine(-0.4);
      gdi.addInput("Rows", itow(tmpSettingsParam.htmlRows), 5).setHandler(&htmlClass);
      gdi.setInputStatus("Rows", tmpSettingsParam.htmlRows > 0);

      gdi.popX();
      gdi.dropLine(3.0);
    }
    else {
      gdi.fillRight();
      gdi.addInput("Scale", itow(int(tmpSettingsParam.htmlScale * 100)) + L" %", 5, 0, L"Skalfaktor:").setHandler(&htmlClass);
      gdi.popX();
      gdi.dropLine(3.4);

      gdi.addCheckbox("Reload", "Automatisk omladdning", 0, tmpSettingsParam.timePerPage>999).setHandler(&htmlClass);
      gdi.dropLine(-0.4);
      gdi.addInput("ReloadTime", itow(tmpSettingsParam.timePerPage/1000) + L" s", 5).setHandler(&htmlClass);
      gdi.setInputStatus("Reload", tmpSettingsParam.timePerPage>999);

      gdi.popX();
      gdi.dropLine(3.0);
    }
    gdi.fillRight();
    gdi.addButton("ApplyList", "Lagra inställningar").setHandler(&htmlClass);
    if (tmpSettingsParam.sourceParam != -1)
      gdi.disableInput("ApplyList");
    gdi.addButton("Automatic", "Automatisera", 0, "Skriv ut eller exportera listan automatiskt.").setHandler(&htmlClass);
    gdi.addButton("HTML", "Exportera").setHandler(&htmlClass);
  }
}

void TabList::htmlSettings(gdioutput &gdi, string targetTag) {
  gdi.clearPage(false);
  gdi.setCX(10);
  gdi.setCY(15);
  gdi.setColorMode(RGB(242, 240, 250));
  gdi.setData("target", targetTag);
  settingsTarget = targetTag;
  gdi.addString("", fontMediumPlus, L"HTML Export för 'X'#" + currentList.getName());
  tmpSettingsParam = currentList.getParam();

  vector<HTMLWriter::TemplateInfo> htmlTmpl;
  HTMLWriter::enumTemplates(HTMLWriter::TemplateType::List, htmlTmpl);
  vector <pair<wstring, size_t>> items;
  gdi.dropLine(0.5);
  gdi.addString("", 10, "htmlhelp");

  items.emplace_back(lang.tl("Strukturerat webbdokument (html)"), 0);
  items.emplace_back(lang.tl("Formaterat webbdokument (html)"), 1);
  
  int id = 10;
  htmlTemplateTag2Id.clear();
  html2IdToInfo.clear();
  htmlTemplateTag2Id["free"] = 1;
  htmlTemplateTag2Id["table"] = 0;
  
  for (auto &t : htmlTmpl) {
    items.emplace_back(lang.tl(t.name), ++id);
    htmlTemplateTag2Id[t.tag] = id;
    html2IdToInfo[id] = t.desc;
  }
  gdi.addSelection("Format", 200, 100, 0, L"Format:").setHandler(&htmlClass);
  if (!htmlTemplateTag2Id.count(tmpSettingsParam.htmlTypeTag))
    tmpSettingsParam.htmlTypeTag = "free";

  gdi.addItem("Format", items);
  int tid = htmlTemplateTag2Id[tmpSettingsParam.htmlTypeTag];
  gdi.selectItemByData("Format", tid);
  
  htmlDetails(gdi, tmpSettingsParam, html2IdToInfo[tid], tid > 5);
  
  gdi.refresh();
}

void TabList::handleHTMLSettings(gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) {
  if (type == GUI_BUTTON) {
    int typeIx = gdi.getSelectedItem("Format").first;
    string typeTag = "free";
    for (auto &m : htmlTemplateTag2Id) {
      if (m.second == typeIx) {
        typeTag = m.first;
        break;
      }
    }
    int margin = -1;
    int rows = 0;
    int cols = 0;
    int time_ms = 0;
    double scale = 0;

    if (gdi.hasField("Margin")) {
      margin = gdi.getTextNo("Margin");
      if (gdi.isChecked("UseRows"))
        rows = gdi.getTextNo("Rows");
      cols = gdi.getTextNo("Columns");
      time_ms = gdi.getTextNo("Time");
      scale = _wtoi(gdi.getText("Scale").c_str())*0.01;
    }
    else {
      scale = _wtoi(gdi.getText("Scale").c_str())*0.01;
      if (gdi.isChecked("Reload"))
        time_ms = 1000 * _wtoi(gdi.getText("ReloadTime").c_str());
    }
    ButtonInfo bi = static_cast<ButtonInfo&>(info);
    if (bi.id == "ApplyList" || bi.id =="Automatic") {
      oListParam &param = currentList.getParam();
      param.htmlTypeTag = typeTag;

      if (margin > 0) {
        param.htmlRows = rows;
        param.htmlScale = scale;
        param.timePerPage = time_ms;
        param.margin = margin;
        param.nColumns = cols;
      }
      else {
        param.htmlScale = scale;
        param.timePerPage = time_ms;
      }

      if (bi.id == "ApplyList") {
        if (param.sourceParam != -1) {
          auto &dest = oe->getListContainer().getParam(param.sourceParam);
          if (margin > 0) {
            dest.htmlRows = rows;
            dest.htmlScale = scale;
            dest.timePerPage = time_ms;
            dest.margin = margin;
            dest.nColumns = cols;
          }
          else {
            dest.htmlScale = scale;
            dest.timePerPage = time_ms;
          }

          dest.htmlTypeTag = typeTag;
        }
        else {
          dest_gdi.sendCtrlMessage("Remember");
        }
        gdi.disableInput("ApplyList");
      }
      else {
        if (lastHtmlTarget.empty()) {
          WCHAR bf[260];
          bf[0] = 0;
          GetCurrentDirectory(260, bf);
          lastHtmlTarget = bf;
          lastHtmlTarget += L"\\exported.html";
        }
        dest_gdi.sendCtrlMessage("Automatic");
      }
    }
    else if (bi.id == "HTML") {
      oListParam &param = currentList.getParam();

      int index = 0;
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Webbdokument", L"*.html;*.htm"));

      wstring file = gdi.browseForSave(ext, L"html", index);

      if (!file.empty()) {
        HTMLWriter::reset(); // Force template reload
        HTMLWriter::write(dest_gdi, file, oe->getName(), 
                          param.getContentsDescriptor(*oe), 
                          param.pageBreak,
                          typeTag, 0, rows, cols, time_ms, margin, scale);
        gdi.openDoc(file.c_str());
        lastHtmlTarget = file;
        oe->setProperty("LastExportTarget", file);
      }
    }
    else if (bi.id == "UseRows") {
      gdi.setInputStatus("Rows", gdi.isChecked(bi.id));
      gdi.enableInput("ApplyList");
    }
    else if (bi.id == "Reload") {
      gdi.setInputStatus("ReloadTime", gdi.isChecked(bi.id));
      gdi.enableInput("ApplyList");
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo lbi = dynamic_cast<ListBoxInfo &>(info);
    if (lbi.id == "Format") {
      htmlDetails(gdi, tmpSettingsParam, html2IdToInfo[lbi.data], lbi.data > 5);
      gdi.enableInput("ApplyList");
      gdi.refresh();
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    gdi.enableInput("ApplyList");
  }
}

void TabList::loadClassSettings(gdioutput &gdi, string targetTag) {
  gdi.clearPage(false);
  gdi.setCX(gdi.scaleLength(10));
  gdi.setCY(gdi.scaleLength(15));
  gdi.setColorMode(RGB(242, 240, 250));
  gdi.setData("target", targetTag);
  settingsTarget = targetTag;
  gdi.addString("", fontMediumPlus, L"Klassval för 'X'#" + currentList.getName());
  gdi.dropLine(0.5);
  gdi.pushX();

//    int sx = gdi.getCX() - gdi.scaleLength(5);
//  int sy = gdi.getCY();
//  gdi.dropLine(0.7);
  
  makeClassSelection(gdi);

  oEvent::ClassFilter ct = currentList.isTeamList() ? oEvent::filterOnlyMulti : oEvent::filterNone;

  oe->fillClasses(gdi, "ListSelection", oEvent::extraNone, ct);
  gdi.setSelection("ListSelection", currentList.getParam().selection);
  gdi.dropLine(2.5);

  gdi.fillDown();
  gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser", 0, currentList.getParam().pageBreak).setHandler(&settingsClassSelection);
  gdi.addCheckbox("ShowHeader", "Visa rubrik", 0, currentList.getParam().showHeader).setHandler(&settingsClassSelection);

  gdi.dropLine(-0.3);
  gdi.addInput("Heading", currentList.getParam().getCustomTitle(wstring(L"")), 28, 0, L"Egen listrubrik:");
  gdi.setInputStatus("Heading", currentList.getParam().showHeader);
  gdi.dropLine(0.5);
  gdi.fillRight();
  gdi.addButton("UpdateClass", "Uppdatera").setDefault().setHandler(&settingsClassSelection);
  gdi.addButton("Cancel", "Avbryt").setCancel().setHandler(&settingsClassSelection);
  gdi.dropLine(2.5);
  gdi.popX();
  //
 // int ey = gdi.getCY() + gdi.scaleLength(4);
//  int ex = gdi.getWidth();
//  RECT rc = { sx, sy, ex, ey };
//  gdi.addRectangle(rc, colorLightBlue);

  gdi.refresh();
}

void TabList::handleClassSettings(gdioutput &gdi, BaseInfo &info, GuiEventType type, gdioutput &dest_gdi) {
  if (type == GUI_BUTTON) {
    ButtonInfo bi = static_cast<ButtonInfo&>(info);
    if (bi.id == "Cancel")
      gdi.closeWindow();
    else if (bi.id == "UpdateClass") {
      oListParam &param = currentList.getParam();
      param.lockUpdate = true;
      gdi.getSelection("ListSelection", param.selection);
      bool pb = gdi.isChecked("PageBreak");
      param.pageBreak = pb;
      
      bool sh = gdi.isChecked("ShowHeader");
      param.showHeader = sh;

      param.setCustomTitle(makeDash(trim(gdi.getText("Heading"))));

      loadPage(dest_gdi);
      param.lockUpdate = false;
      gdi.closeWindow();
    }
    else if (bi.id == "PageBreak" || bi.id == "ShowHeader") {
      gdi.setInputStatus("Heading", gdi.isChecked("ShowHeader"));
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo lbi = static_cast<ListBoxInfo&>(info);
  }
}


void TabList::settingsResultList(gdioutput &gdi)
{
  lastFilledResultClassType = -1;
  oe->sanityCheck(gdi, true);
  gdi.fillDown();
  gdi.clearPage(false);
  gdi.addString("", boldLarge, makeDash(L"Resultatlista - inställningar"));

  //gdi.addSelection("ListType", 200, 300, ListsCB, "Lista");
  //oe->fillListTypes(gdi, "ListType", 0);
  const int boxHeight = 380;
  gdi.pushY();
  gdi.fillDown();
  gdi.addListBox("ListSelection", 200, boxHeight, ListsCB, L"Urval:", L"", true);

  gdi.dropLine(0.5);
  gdi.fillRight();
  gdi.addButton("SelectAll", "Välj allt", ListsCB);
  gdi.addButton("SelectNone", "Välj inget", ListsCB);

  gdi.popY();
  gdi.setCX(gdi.scaleLength(250));
  infoCX = gdi.getCX();
  infoCY = gdi.getCY() + gdi.scaleLength(boxHeight) + 2 *gdi.getLineHeight();
  
  gdi.fillDown();
  gdi.addString("", 0, "Typ av lista:");
  gdi.pushX();
  gdi.fillRight();

  gdi.addListBox("ResultType", 180, boxHeight, ListsCB);
  
  vector< pair<wstring, size_t> > lists;
  vector< pair<wstring, size_t> > dlists;
  const MetaListContainer &lc = oe->getListContainer();
  lc.getLists(dlists, false, true, !oe->hasTeam());
  set<int> usedListIx;
  map<string, int> tag2ListIx;
  for (size_t k = 0; k < dlists.size(); k++) {
    int ix = dlists[k].second;
    if (lc.isInternal(ix))
      tag2ListIx[lc.getTag(ix)] = dlists[k].second + CUSTOM_OFFSET;
  }
  lists.reserve(dlists.size() + 10);

  lists.push_back(make_pair(lang.tl(L"Individuell"), 1));
  
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Patrol)) 
    lists.push_back(make_pair(lang.tl(L"Patrull"), 2));

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Relay)) {
    lists.push_back(make_pair(lang.tl(L"Stafett - total"), 3));
    lists.push_back(make_pair(lang.tl(L"Stafett - sammanställning"), 4));
    
    lists.push_back(make_pair(lang.tl(L"Stafett - sträcka"), 
                    getListIx(tag2ListIx, usedListIx, "legresult", 5)));
  }
  
  lists.push_back(make_pair(lang.tl(L"Allmänna resultat"), 6));
  
  size_t startIx = lists.size();
  for (size_t k = 0; k < dlists.size(); k++) {
    int ix = dlists[k].second + CUSTOM_OFFSET;
    if (usedListIx.count(ix))
      continue;
    lists.push_back(make_pair(dlists[k].first, ix));
  }
  sort(lists.begin() + startIx, lists.end());

  gdi.addItem("ResultType", lists);
  gdi.autoGrow("ResultType");

  int lastSelectedResultListOK = -1;
  for (size_t k = 0; k < lists.size(); k++) {
    if (lastSelectedResultList == lists[k].second)
      lastSelectedResultListOK = lists[k].second;
  }

  if (lastSelectedResultListOK >= 0)
    gdi.selectItemByData("ResultType", lastSelectedResultListOK);

  gdi.fillDown();
  gdi.pushX();
  gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser", ListsCB, oe->getPropertyInt("pagebreak", 0)!=0);
  gdi.addCheckbox("ShowHeader", "Visa rubrik", ListsCB, oe->getPropertyInt("showheader", 1) != 0);

  gdi.addCheckbox("ShowInterResults", "Visa mellantider", 0, lastInterResult,
            "Mellantider visas för namngivna kontroller.");
  gdi.addCheckbox("ShowSplits", "Lista med sträcktider", 0, lastSplitState);
  gdi.addCheckbox("UseLargeSize", "Använd stor font", 0, lastLargeSize);

  gdi.fillRight();
  gdi.popX();
  gdi.addString("", 0, "Topplista, N bästa:");
  gdi.dropLine(-0.2);
 
  if (lastLimitPer == -1) {
    lastLimitPer = oe->getPropertyInt("classlimit", 0);
  }
  wstring lastClassLimit;
  if (lastLimitPer > 0)
    lastClassLimit = itow(lastLimitPer);
  
  gdi.addInput("ClassLimit", lastClassLimit, 5, 0);
  gdi.popX();
  gdi.dropLine(2); 
  gdi.addString("", 0, "Listparameter:");
  gdi.dropLine(-0.2);
  gdi.addInput("InputNumber", lastInputNumber, 5, 0, L"", L"Ett värde vars tolkning beror på listan.");
  gdi.disableInput("InputNumber");
  gdi.popX();
  gdi.dropLine(2);

  makeFromTo(gdi);

  gdi.addSelection("LegNumber", 140, 300, ListsCB, L"Sträcka:");
  gdi.disableInput("LegNumber");
  gdi.popX();

  gdi.dropLine(3);
  gdi.addInput("Title", L"", 32, ListsCB, L"Egen listrubrik:");
  gdi.popX();

  gdi.dropLine(3.5);
  createListButtons(gdi);

  gdi.setRestorePoint("ListInfo");
  infoCY = max(infoCY, gdi.getCY());
  gdi.setCX(infoCX);
  gdi.setCY(infoCY);
  
  if (lastSelectedResultListOK >= 0)
    setResultOptionsFromType(gdi, lastSelectedResultListOK);
  gdi.refresh(); 
}

void TabList::createListButtons(gdioutput &gdi) {
  gdi.fillRight();
  gdi.addButton("Generate", "Skapa listan", ListsCB).setDefault();
  gdi.disableInput("Generate");
  gdi.addButton("Cancel", "Avbryt", ListsCB).setCancel();
  gdi.popX();
}

void checkWidth(gdioutput &gdi) {
  int h,w;
  gdi.getTargetDimension(w, h);
  w = max (w, gdi.scaleLength(300));
  if (gdi.getCX() + gdi.scaleLength(110) > w) {
    gdi.popX();
    gdi.dropLine(2.5);
  }
}

bool TabList::loadPage(gdioutput &gdi, const string &command) {
  SelectedList = command;
  offsetX = 0;
  offsetY = 0;
  return loadPage(gdi);
}

bool TabList::loadPage(gdioutput &gdi)
{
  oe->checkDB();
  oe->synchronize();
  gdi.selectTab(tabId);
  noReEvaluate = false;
  gdi.clearPage(false);
  if (SelectedList!="") {
    ButtonInfo bi;
    bi.id=SelectedList;
    ListsCB(&gdi, GUI_BUTTON, &bi);
    //gdi.SendCtrlMessage(SelectedList);
    gdi.setOffset(offsetX, offsetY, false);
    return 0;
  }

  // Make sure all lists are loaded
  oListInfo li;
  oe->getListType(EStdNone, li);

  gdi.addString("", boldLarge, "Listor och sammanställningar");

  gdi.addString("", 10, "help:30750");

  ClassConfigInfo cnf;
  oe->getClassConfigurationInfo(cnf);
  gdi.pushX();
  if (!cnf.empty()) {
    gdi.dropLine(1);
    gdi.addString("", fontMediumPlus, "Startlistor").setColor(colorDarkGrey);
    gdi.fillRight();
    if (cnf.hasIndividual()) {
      gdi.addButton("StartIndividual", "Individuell", ListsCB);
      checkWidth(gdi);
      if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs))
        gdi.addButton("StartClub", "Klubbstartlista", ListsCB);
    }
    if (cnf.hasPatrol()) {
      checkWidth(gdi);
      gdi.addButton("StartL:patrolstart", "Patrull", ListsCB).setExtra(AddPatrolClasses);
    }
    for (size_t k = 0; k < cnf.raceNStart.size(); k++) {
      if (cnf.raceNStart[k].size() > 0) {
        checkWidth(gdi);
        gdi.addButton("RaceNStart", "Lopp X#" + itos(k + 1), ListsCB,
                      "Startlista ett visst lopp.").setExtra(k);
      }
    }
    if (cnf.hasRelay()) {
      checkWidth(gdi);
      gdi.addButton("TeamStartList", "Stafett (sammanställning)", ListsCB);
    }

    for (size_t k = 0; k < cnf.legNStart.size(); k++) {
      if (cnf.legNStart[k].size() > 0) {
        checkWidth(gdi);
        gdi.addButton("LegNStart", "Sträcka X#" + itos(k + 1), ListsCB).setExtra(k);
      }
    }

    checkWidth(gdi);
    gdi.addButton("MinuteStartList", "Minutstartlista", ListsCB);

    if (cnf.isMultiStageEvent()) {
      checkWidth(gdi);
      gdi.addButton("StartL:inputresult", "Input Results", ListsCB);
    }

    gdi.dropLine(3);
    gdi.fillDown();
    gdi.popX();
    gdi.addString("", fontMediumPlus, "Resultatlistor").setColor(colorDarkGrey);
    gdi.fillRight();

    if (cnf.hasIndividual()) {
      gdi.addButton("ResultIndividual", "Individuell", ListsCB);
      checkWidth(gdi);
    }

    if (cnf.hasPatrol()) {
      gdi.addButton("Result:patrolresult", "Patrull", ListsCB).setExtra(AddPatrolClasses);
      checkWidth(gdi);
    }

    if (cnf.hasIndividual()) {

      gdi.addButton("Result:liveresultradio", "Liveresultat", ListsCB);
      checkWidth(gdi);

      gdi.addButton("Result:latestresult", "Latest Results", ListsCB).setExtra(AddAllClasses);
      checkWidth(gdi);

      if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
        gdi.addButton("ResultClub", "Klubbresultat", ListsCB);
        checkWidth(gdi);
      }

      gdi.addButton("ResultIndSplit", "Sträcktider", ListsCB);


      if (cnf.isMultiStageEvent()) {
        checkWidth(gdi);
        gdi.addButton("Result:stageresult", "Etappresultat", ListsCB);

        checkWidth(gdi);
        gdi.addButton("Result:finalresult", "Slutresultat", ListsCB);
      }
    }

    if (!cnf.knockout.empty()) {
      checkWidth(gdi);
      gdi.addButton("KnockoutTotal", "Knockout sammanställning", ListsCB);
    }


    if (!cnf.lapcountsingle.empty()) {
      checkWidth(gdi);
      gdi.addButton("LapCount", "Varvräkning", ListsCB);
    }

    if (!cnf.lapcountextra.empty()) {
      checkWidth(gdi);
      gdi.addButton("LapCountExtra", "Varvräkning med mellantid", ListsCB);
    }

    bool hasMulti = false;
    for (size_t k = 0; k < cnf.raceNStart.size(); k++) {
      if (cnf.raceNRes[k].size() > 0) {
        checkWidth(gdi);
        gdi.addButton("RaceNRes", "Lopp X#" + itos(k + 1), ListsCB,
                      "Resultat för ett visst lopp.").setExtra(k);
        hasMulti = true;
      }
    }

    if (hasMulti) {
      checkWidth(gdi);
      gdi.addButton("MultiResults", "Alla lopp", ListsCB, "Individuell resultatlista, sammanställning av flera lopp.");
    }
    if (cnf.hasRelay()) {
      checkWidth(gdi);
      gdi.addButton("TeamResults", "Stafett (sammanställning)", ListsCB);
    }

    for (map<int, vector<int> >::const_iterator it = cnf.legResult.begin(); it != cnf.legResult.end(); ++it) {
      checkWidth(gdi);
      gdi.addButton("LegNResult", "Sträcka X#" + itos(it->first + 1), ListsCB).setExtra(it->first);
    }

    if (cnf.hasRogaining()) {
      checkWidth(gdi);
      gdi.addButton("Result:rogainingind", "Rogaining", ListsCB).setExtra(AddRogainingClasses);
    }

    checkWidth(gdi);
    gdi.addButton("ResultList", "Avancerat...", ListsCB);

    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(3);
  }

  MetaListContainer &lc = oe->getListContainer();
  if (lc.getNumLists(MetaListContainer::ExternalList) > 0) {
    gdi.addString("", fontMediumPlus, "Egna listor").setColor(colorDarkGrey);

    gdi.fillRight();
    gdi.pushX();

    for (int k = 0; k < lc.getNumLists(); k++) {
      if (lc.isExternal(k)) {
        MetaList &mc = lc.getList(k);
        checkWidth(gdi);
        gdi.addButton("CustomList", mc.getListName(), ListsCB).setExtra(k);
      }
    }

    gdi.popX();
    gdi.dropLine(3);
    gdi.fillDown();
  }

  vector< pair<wstring, size_t> > savedParams;
  lc.getListParam(savedParams);
  if (savedParams.size() > 0) {
    gdi.addString("", fontMediumPlus, "Sparade listval").setColor(colorDarkGrey);
    gdi.fillRight();
    gdi.pushX();

    gdi.addSelection("SavedInstance", 250, 200, ListsCB);
    gdi.addItem("SavedInstance", savedParams);
    gdi.autoGrow("SavedInstance");
    gdi.selectFirstItem("SavedInstance");
    gdi.addButton("ShowSaved", "Visa", ListsCB);
    gdi.addButton("RenameSaved", "Döp om", ListsCB);
    gdi.addButton("MergeSaved", "Slå ihop...", ListsCB);
    gdi.addButton("SplitSaved", "Dela upp...", ListsCB);

    bool split = oe->getListContainer().canSplit(savedParams[0].second);
    gdi.setInputStatus("SplitSaved", split);
    
    gdi.addButton("RemoveSaved", "Ta bort", ListsCB);
    gdi.popX();
    gdi.dropLine(3);
    gdi.fillDown();
  }

  gdi.addString("", fontMediumPlus, "Rapporter").setColor(colorDarkGrey);

  gdi.fillRight();
  gdi.pushX();

  gdi.addButton("InForestList", "Kvar-i-skogen", ListsCB, "tooltip:inforest").setExtra(IgnoreLimitPer);
  if (cnf.hasIndividual()) {
    gdi.addButton("PriceList", "Prisutdelningslista", ListsCB);
  }
  gdi.addButton("PreReport", "Kör kontroll inför tävlingen...", ListsCB);
  checkWidth(gdi);

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy) && cnf.hasIndividual()) {
    gdi.addButton("GenLst:unexpectedfee", "Unexpected Fee", ListsCB);
    checkWidth(gdi);
  }
  
  if (cnf.hasMultiCourse) {
    gdi.addButton("CourseReport", "Bantilldelning", ListsCB).setExtra(IgnoreLimitPer);
    checkWidth(gdi);

    if (cnf.hasTeamClass()) {
      gdi.addButton("GenLst:courseteamtable", "Gafflingar i tabellformat", ListsCB,
                    "Från den här listan kan man skapa etiketter att klistra på kartor");
      checkWidth(gdi);
    }    
  }

  if (cnf.hasTeamClass()) {
    gdi.addButton("GenLst:teamchanges", "Lagändringblankett", ListsCB).setExtra(AddTeamClasses | ForcePageBreak);
    checkWidth(gdi);
  }
  bool hasVac = false;
  bool hasAPIEntry = false;
  {
    vector<pRunner> rr;
    oe->getRunners(0, 0, rr, false);
    for (pRunner r : rr) {
      if (r->isVacant()) {
        hasVac = true;
        break;
      }
    }

    for (pRunner r : rr) {
      if (r->hasFlag(oRunner::FlagAddedViaAPI)) {
        hasAPIEntry = true;
        break;
      }
    }
  }

  if (hasVac) {
    gdi.addButton("GenLst:vacnacy", "Vakanser", ListsCB);
    checkWidth(gdi);
  }

  if (hasAPIEntry) {
    gdi.addButton("GenLst:apientry", "EFilterAPIEntry", ListsCB);
    checkWidth(gdi);
  }

  if (oe->getMeOSFeatures().withCourses(oe)) {
    gdi.addButton("GenLst:courseusage", "Bananvändning", ListsCB);
    checkWidth(gdi);

    gdi.addButton("GenLst:controloverview", "Kontroller", ListsCB);
    checkWidth(gdi);

    gdi.addButton("GenLst:controlstatistics", "Control Statistics", ListsCB);
    checkWidth(gdi);
  }

  if (cnf.hasRentedCard)
    gdi.addButton("HiredCards", "Hyrbricksrapport", ListsCB).setExtra(IgnoreLimitPer);

  gdi.popX();

  gdi.dropLine(3);
  RECT rc;
  rc.left = gdi.getCX();
  gdi.setCX(rc.left + gdi.scaleLength(10));
  rc.top = gdi.getCY();
  gdi.dropLine(0.5);
  gdi.addString("", fontMediumPlus, "Inställningar").setColor(colorDarkGrey);
  gdi.setCX(rc.left + gdi.scaleLength(10));
  gdi.dropLine(2);

  gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser / klubbar", ListsCB, oe->getPropertyInt("pagebreak", 0)!=0);
  gdi.addCheckbox("ShowHeader", "Visa rubrik", ListsCB, oe->getPropertyInt("showheader", 1) != 0);
  gdi.addCheckbox("SplitAnalysis", "Med sträcktidsanalys", ListsCB, oe->getPropertyInt("splitanalysis", 1)!=0);

  gdi.setCX(rc.left + gdi.scaleLength(10));
  gdi.dropLine(2);
  gdi.addString("", 0, "Begränsning, antal visade per klass: ");
  gdi.dropLine(-0.2);
  gdi.addSelection("ClassLimit", 70, 350, ListsCB);
  gdi.addItem("ClassLimit", lang.tl("Ingen"), 0);
  for (int k = 1; k<=12+9; k++) {
    int v = k;
    if (v>12)
      v=(v-11)*10;
    gdi.addItem("ClassLimit", itow(v), v);
  }
  gdi.selectItemByData("ClassLimit", oe->getPropertyInt("classlimit", 0));
  
  gdi.dropLine(2);
  rc.bottom = gdi.getCY();
  rc.right = gdi.getWidth() - gdi.scaleLength(5);
  gdi.addRectangle(rc, colorLightBlue);
  gdi.dropLine(1.5);
  gdi.popX();
  gdi.addButton("GeneralList", "Alla listor...", ListsCB);
  gdi.addButton("EditList", "Redigera lista...", ListsCB);

  gdi.addButton("ImportCustom", "Hantera egna listor...", ListsCB);
  checkWidth(gdi);
  gdi.addButton("EditMethod", "Result Modules...", ListsCB);

  //gdi.registerEvent("DataUpdate", ListsEventCB);
  gdi.refresh();

  gdi.setOnClearCb(ListsCB);

  offsetY=0;
  offsetX=0;
  gdi.refresh();
  return true;
}

void TabList::enableWideFormat(gdioutput &gdi, bool wide) {
  if (gdi.hasField("NumPerPage")) {
    gdi.setInputStatus("NumPerPage", wide);

    bool needTime = gdi.getSelectedItem("NumPerPage").first != 1;
    gdi.setInputStatus("MaxWaitTime", wide & needTime);
  }
}

void TabList::splitPrintSettings(oEvent &oe, gdioutput &gdi, bool setupPrinter, 
                                  TabType returnMode, PrintSettingsSelection type)
{
  if (!gdi.canClear())
    return;

  gdi.clearPage(false);
  gdi.fillDown();
  if (type == Splits)
    gdi.addString("", boldLarge, "Inställningar sträcktidsutskrift");
  else
    gdi.addString("", boldLarge, "Inställningar startbevis");

  gdi.dropLine();

  gdi.fillRight();
  gdi.pushX();
  if (setupPrinter) {
    gdi.addButton("PrinterSetup", "Skrivare...", ListsCB, "Skrivarinställningar");
    gdi.dropLine(0.3);
  }

 
  if (!oe.empty() && type == Splits) {
    bool withSplitAnalysis = (oe.getDCI().getInt("Analysis") & 1) == 0;
    bool withSpeed = (oe.getDCI().getInt("Analysis") & 2) == 0;
    bool withResult = (oe.getDCI().getInt("Analysis") & 4) == 0;

    gdi.addCheckbox("SplitAnalysis", "Med sträcktidsanalys", 0, withSplitAnalysis);
    gdi.addCheckbox("Speed", "Med km-tid", 0, withSpeed);
    gdi.addCheckbox("Results", "Med resultat", 0, withResult);

  }
  gdi.popX();
  gdi.fillDown();
  char *ctype = type == Splits ? "SPExtra" : "EntryExtra";
  customTextLines(oe, ctype, gdi);

  if (type == Splits) {
    const bool wideFormat = oe.getPropertyInt("WideSplitFormat", 0) == 1;
    gdi.addCheckbox("WideFormat", "Sträcktider i kolumner (för standardpapper)", ListsCB, wideFormat);

    if (returnMode == TSITab) {
      int printLen = oe.getPropertyInt("NumSplitsOnePage", 3);
      vector< pair<wstring, size_t> > nsp;
      for (size_t j = 1; j < 8; j++)
        nsp.push_back(make_pair(itow(j), j));
      gdi.addSelection("NumPerPage", 90, 200, ListsCB, L"Max antal brickor per sida");
      gdi.addItem("NumPerPage", nsp);
      gdi.selectItemByData("NumPerPage", printLen);

      int maxWait = oe.getPropertyInt("SplitPrintMaxWait", 60);
      gdi.addInput("MaxWaitTime", itow(maxWait), 8, 0, L"Längsta tid i sekunder att vänta med utskrift");

      enableWideFormat(gdi, wideFormat);
    }
  }


  gdi.fillRight();
  gdi.setData("Type", ctype);
  gdi.addButton("SavePS", "OK", ListsCB).setDefault().setExtra(returnMode);
  gdi.addButton("CancelPS", "Avbryt", ListsCB).setCancel().setExtra(returnMode);

  gdi.refresh();
}

void TabList::saveExtraLines(oEvent &oe, const char *dataField, gdioutput &gdi) {
  vector< pair<wstring, int> > lines;
  for (int k = 0; k < 5; k++) {
    string row = "row"+itos(k);
    string key = "font"+itos(k);
    ListBoxInfo lbi;
    gdi.getSelectedItem(key, lbi);
    wstring r = gdi.getText(row);
    lines.push_back(make_pair(r, lbi.data));
  }
  oe.setExtraLines(dataField, lines);
}

void TabList::customTextLines(oEvent &oe, const char *dataField, gdioutput &gdi) {
  gdi.dropLine(2.5);
  gdi.addString("", fontMediumPlus, "Egna textrader");
  gdi.dropLine(0.3);
  gdi.addString("", 10, "help:custom_text_lines");
  gdi.dropLine(0.8);
  int yp = gdi.getCY();

  vector< pair<wstring, size_t> > fonts;
  vector< pair<wstring, int> > lines;

  MetaListPost::getAllFonts(fonts);
  oe.getExtraLines(dataField, lines);
  int xp = gdi.getCX();
  for (int k = 0; k < 5; k++) {
    gdi.fillRight();
    gdi.pushX();
    string row = "row"+itos(k);
    gdi.addInput(row, L"", 24);
    string key = "font"+itos(k);
    gdi.addSelection(key, 100, 100);
    gdi.addItem(key, fonts);
    if (lines.size() > size_t(k)) {
      gdi.selectItemByData(key.c_str(), lines[k].second);
      gdi.setText(row, lines[k].first);
    }
    else
      gdi.selectFirstItem(key);
    
    xp = max(xp, gdi.getCX());
    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(2);
  }
  gdi.pushX();
  gdi.pushY();

  gdi.setCX(xp);
  gdi.setCY(yp);
  gdi.addListBox("Symbols", 500, 160);
  gdi.setTabStops("Symbols", 300);
  vector < pair<wstring, size_t>> symb;
  MetaList::fillSymbols(symb);
  gdi.addItem("Symbols", symb);
  gdi.popX();
  gdi.popY();
}

void TabList::liveResult(gdioutput &gdi, oListInfo &li) {
  if (liveResults.empty() || !liveResults.back()) {
    liveResults.push_back(0);
    LiveResult *lr = new LiveResult(oe);
    liveResults.back() = lr;
  }
  liveResults.back()->showTimer(gdi, li);
}

EStdListType TabList::getTypeFromResultIndex(int ix) const {
  switch(ix) {
    case 1:
      return EStdResultList;
    case 2:
      return oe->getListContainer().getType("patrolresult");
    case 3:
      return EStdTeamResultListAll;
    case 4:
      return EStdTeamResultList;
    //case 5:
    //  return EStdTeamResultListLeg;
    case 6:
      return EGeneralResultList;
    default:
      if (ix >= CUSTOM_OFFSET) {
        int index = ix - CUSTOM_OFFSET;
        const string &uid = oe->getListContainer().getList(index).getUniqueId();
        return oe->getListContainer().getCodeFromUnqiueId(uid);
      }
  }
  return EStdNone;
}

void TabList::setResultOptionsFromType(gdioutput &gdi, int data) {
  bool builtIn = data < CUSTOM_OFFSET;
  wstring info, title;
  bool hasResMod = false;
  oListInfo li;
  EStdListType type = getTypeFromResultIndex(data);
  oe->getListType(type, li);
  ListBoxInfo lbi;
  if (gdi.getSelectedItem("LegNumber", lbi) && int(lbi.data) >= 0)
    lastLeg = lbi.data;
           
  if (builtIn) {
    enableFromTo(*oe, gdi, data==1 || data==6, data==1 || data==6);

    if (data==6) {
      gdi.enableInput("UseLargeSize");
    }
    else {
      gdi.disableInput("UseLargeSize");
      gdi.check("UseLargeSize", false);
    }

    gdi.setInputStatus("ShowInterResults", builtIn);
    gdi.setInputStatus("ShowSplits", builtIn);
      

    set<int> clsUnused;
    vector< pair<wstring, size_t> > out;
      
    oe->fillLegNumbers(clsUnused, li.isTeamList(), true, out);
    gdi.addItem("LegNumber", out);
    gdi.setInputStatus("LegNumber", !out.empty());
    
    if (!out.empty() && lastLeg >= 0)
      gdi.selectItemByData("LegNumber", lastLeg);
  
    //oe->fillLegNumbers(gdi, "LegNumber", li.isTeamList(), true);
    gdi.setInputStatus("InputNumber", false);
  }
  else {
      
    gdi.setInputStatus("UseLargeSize", li.supportLarge);
    gdi.setInputStatus("InputNumber", li.supportParameter);
  
    //gdi.setInputStatus("SplitAnalysis", li.supportSplitAnalysis);
    //gdi.setInputStatus("ShowInterResults", li.supportInterResults);
    //gdi.setInputStatus("PageBreak", li.supportPageBreak);
    //gdi.setInputStatus("ClassLimit", li.supportClassLimit);
      
    if (li.supportLegs) {
      gdi.enableInput("LegNumber");
      //oe->fillLegNumbers(gdi, "LegNumber", li.isTeamList(), true);
      set<int> clsUnused;
      vector< pair<wstring, size_t> > out;
      oe->fillLegNumbers(clsUnused, li.isTeamList(), true, out);
      gdi.addItem("LegNumber", out);
      if (!out.empty() && lastLeg >= 0)
        gdi.selectItemByData("LegNumber", lastLeg);
      gdi.setInputStatus("LegNumber", !out.empty());
    }
    else {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("LegNumber", lbi) && int(lbi.data) >= 0)
        lastLeg = lbi.data;
      gdi.disableInput("LegNumber");
      gdi.clearList("LegNumber");
    }

    enableFromTo(*oe, gdi, li.supportFrom, li.supportTo);
  }

  if (!builtIn) {
    int index = data - CUSTOM_OFFSET;
    const MetaList &ml = oe->getListContainer().getList(index);
    info = ml.getListInfo(*oe);
    title = ml.getListName();
    hasResMod = !ml.getResultModule().empty();
  }

  if (info.empty() && gdi.getText("ListInfo", true).empty()) {
    // Do nothing
  }
  else {
    gdi.restore("ListInfo", false);
    gdi.setCX(infoCX);
    gdi.setCY(infoCY);

    if (!info.empty()) {
      gdi.setRestorePoint("ListInfo");
      gdi.fillDown();
      gdi.addString("", fontMediumPlus, title);
      gdi.dropLine(0.3);
      gdi.addString("ListInfo", 10, info);
      gdi.dropLine(0.7);
    }
    //createListButtons(gdi);
    gdi.refresh();
  }
  oEvent::ClassFilter ct = li.isTeamList() ? oEvent::filterOnlyMulti : oEvent::filterNone;
  set<int> curSel;
  gdi.getSelection("ListSelection", curSel);

  if (lastFilledResultClassType != ct) {
    lastFilledResultClassType = ct;
    lastResultClassSelection.insert(curSel.begin(), curSel.end());
    oe->fillClasses(gdi, "ListSelection", oEvent::extraNone, ct);
    gdi.setSelection("ListSelection", lastResultClassSelection);
  }

  gdi.setInputStatus("Generate", data >= 0 && !lastResultClassSelection.empty());
}

void TabList::clearCompetitionData() {
  SelectedList = "";
  lastResultClassSelection.clear();

  ownWindow = false;
  hideButtons = false;
  currentListType=EStdNone;
  noReEvaluate = false;

  infoCX = 0;
  infoCY = 0;

  lastLimitPer = -1;
  lastInterResult = false;
  lastSplitState = false;
  lastLargeSize = false;

  lastSelectedResultList = -1;
  lastLeg = 0;
  lastFilledResultClassType = -1;

  delete listEditor;
  delete methodEditor;
  listEditor = 0;
  methodEditor = 0;
}

void TabList::setAnimationMode(gdioutput &gdi) {
  auto par = currentList.getParam();
  gdi.setAnimationMode(make_shared<AnimationData>(gdi, par.timePerPage, par.nColumns,
    par.margin, par.animate, par.pageBreak));
}

void TabList::getStartIndividual(oListParam &par, ClassConfigInfo &cnf){
  par.listCode = EStdStartList;
  par.setLegNumberCoded(-1);
  cnf.getIndividual(par.selection);
}

void TabList::getStartClub(oListParam &par) {
  par.listCode = EStdClubStartList;
  par.setLegNumberCoded(-1);
}

void TabList::getResultIndividual(oListParam &par, ClassConfigInfo &cnf) {
  cnf.getIndividual(par.selection);
  par.listCode = EStdResultList;
  par.showInterTimes = true;
  par.setLegNumberCoded(-1);
}

void TabList::getResultClub(oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = EStdClubResultList;
  par.setLegNumberCoded(-1);
  cnf.getIndividual(par.selection);
  cnf.getPatrol(par.selection);
}

void TabList::getStartPatrol(oEvent &oe, oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = oe.getListContainer().getType("patrolstart");
  cnf.getPatrol(par.selection);
}

void TabList::getResultPatrol(oEvent &oe, oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = oe.getListContainer().getType("patrolresult");
  cnf.getPatrol(par.selection);
}

void TabList::getStartTeam(oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = EStdTeamStartList;
  cnf.getRelay(par.selection);
  par.setLegNumberCoded(0);
}

void TabList::getResultTeam(oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = EStdTeamResultListAll;
  cnf.getRelay(par.selection);
}

void TabList::getResultRogaining(oListParam &par, ClassConfigInfo &cnf) {
  par.listCode = ERogainingInd;
  cnf.getRogaining(par.selection);
}

void TabList::getPublicLists(oEvent &oe, vector<oListParam> &lists) {
  lists.clear();

  ClassConfigInfo cnf;
  oe.getClassConfigurationInfo(cnf);
  if (!cnf.empty()) {
    if (cnf.hasIndividual()) {
      lists.push_back(oListParam());
      getStartIndividual(lists.back(), cnf);

      if (oe.getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
        lists.push_back(oListParam());
        getStartClub(lists.back());
      }
    }

    if (cnf.hasRelay()) {
      lists.push_back(oListParam());
      getStartTeam(lists.back(), cnf);
    }
    if (cnf.hasPatrol()) {
      lists.push_back(oListParam());
      getStartPatrol(oe, lists.back(), cnf);
    }

    if (cnf.isMultiStageEvent()) {
      //gdi.addButton("StartL:inputresult", "Input Results", ListsCB);
    }

    if (cnf.hasIndividual()) {
      lists.push_back(oListParam());
      getResultIndividual(lists.back(), cnf);
      if (oe.getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
        lists.push_back(oListParam());
        getResultClub(lists.back(), cnf);
      }

      //gdi.addButton("ResultIndSplit", "Sträcktider", ListsCB);

      if (cnf.isMultiStageEvent()) {
        //gdi.addButton("Result:stageresult", "Etappresultat", ListsCB);

        //gdi.addButton("Result:finalresult", "Slutresultat", ListsCB);
      }
    }
    if (cnf.hasRelay()) {
      lists.push_back(oListParam());
      getResultTeam(lists.back(), cnf);
    }
    if (cnf.hasPatrol()) {
      lists.push_back(oListParam());
      getResultPatrol(oe, lists.back(), cnf);
    }

  }

  MetaListContainer &lc = oe.getListContainer();

  vector< pair<wstring, size_t> > savedParams;
  lc.getListParam(savedParams);
  for (auto &p : savedParams) {
    oListParam &par = lc.getParam(p.second);
    lists.push_back(par);
  }

  if (cnf.hasIndividual()) {
    //gdi.addButton("PriceList", "Prisutdelningslista", ListsCB);
  }
}

void TabList::readSettings(gdioutput &gdi, oListParam &par, bool forResult) {
  par.pageBreak = gdi.isChecked("PageBreak");
  par.showHeader = gdi.isChecked("ShowHeader");

  if (forResult) {
    par.splitAnalysis = gdi.isChecked("SplitAnalysis");
    par.filterMaxPer = gdi.getSelectedItem("ClassLimit").first;
  }
}
