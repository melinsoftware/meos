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
#include "csvparser.h"
#include "SportIdent.h"
#include "classconfiginfo.h"
#include "onlineresults.h"
#include "onlineinput.h"
#include "RestService.h"

#include "TabAuto.h"
#include "TabSI.h"
#include "meos_util.h"
#include <shellapi.h>

#include "gdiconstants.h"
#include "meosexception.h"

static TabAuto *tabAuto = 0;
int AutoMachine::uniqueId = 1;

extern HWND hWndMain;
extern HWND hWndWorkspace;

TabAuto::TabAuto(oEvent *poe):TabBase(poe)
{
  synchronize=false;
  synchronizePunches=false;
}

AutoMachine *AutoMachine::getMachine(int id) {
  if (tabAuto)
    return tabAuto->getMachine(id);
  throw meosException("Internal error");
}

AutoMachine* AutoMachine::construct(Machines ms) {
  switch(ms) {
  case mPrintResultsMachine:
    return new PrintResultMachine(0);
  case mSplitsMachine:
    return new SplitsMachine();
  case mPrewarningMachine:
    return new PrewarningMachine();
  case mPunchMachine:
    return new PunchMachine();
  case mOnlineInput:
    return new OnlineInput();
  case mOnlineResults:
    return new OnlineResults();
  case mSaveBackup:
    return new SaveMachine();
  case mInfoService:
    return new RestService();
  }
  throw meosException("Invalid machine");
}

AutoMachine *TabAuto::getMachine(int id) {
  if (id == 0)
    return 0;
  list<AutoMachine *>::iterator it;
  for (it=machines.begin(); it!=machines.end(); ++it) {
    if (*it != 0 && (*it)->getId() == id) {
      return *it;
    }
  }
  throw meosException("Service X not found.#" + itos(id));
}

TabAuto::~TabAuto(void)
{
  list<AutoMachine *>::iterator it;
  for (it=machines.begin(); it!=machines.end(); ++it) {
    delete *it;
    *it=0;
  }
  tabAuto=0;
}


void tabAutoKillMachines()
{
  if (tabAuto)
    tabAuto->killMachines();
}

void tabAutoRegister(TabAuto *ta)
{
  tabAuto=ta;
}

void tabAutoAddMachinge(const AutoMachine &am)
{
  if (tabAuto) {
    tabAuto->addMachine(am);
  }
}

void tabForceSync(gdioutput &gdi, pEvent oe)
{
  if (tabAuto)
    tabAuto->syncCallback(gdi);
}

int AutomaticCB(gdioutput *gdi, int type, void *data)
{
  if (!tabAuto)
    throw std::exception("tabAuto undefined.");

  switch(type){
    case GUI_BUTTON: {
      //Make a copy
      ButtonInfo bu=*static_cast<ButtonInfo *>(data);
      return tabAuto->processButton(*gdi, bu);
             }
    case GUI_LISTBOX:{
      ListBoxInfo lbi=*static_cast<ListBoxInfo *>(data);
      return tabAuto->processListBox(*gdi, lbi);
             }
  }
  return 0;
}

void TabAuto::syncCallback(gdioutput &gdi)
{
  wstring msg;
  try {
    list<AutoMachine *>::iterator it;
    for (it=machines.begin(); it!=machines.end(); ++it) {
      AutoMachine *am=*it;
      if (am && am->synchronize && !am->isEditMode())
        am->process(gdi, oe, SyncDataUp);
    }
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = gdi.widen(ex.what());
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }
  if (!msg.empty()) {
    gdi.alert(msg);
    gdi.setWaitCursor(false);
  }

}

void TabAuto::updateSyncInfo()
{
  list<AutoMachine *>::iterator it;
  synchronize=false;
  synchronizePunches=false;

  for (it=machines.begin(); it!=machines.end(); ++it) {
    AutoMachine *am=*it;
    if (am){
      am->synchronize= am->synchronize || am->synchronizePunches;
      synchronize=synchronize || am->synchronize;
      synchronizePunches=synchronizePunches || am->synchronizePunches;
    }
  }
}

void TabAuto::timerCallback(gdioutput &gdi)
{
  DWORD tc=GetTickCount();

  list<AutoMachine *>::iterator it;
  bool reload=false;

  for (it=machines.begin(); it!=machines.end(); ++it) {
    AutoMachine *am=*it;
    if (am && am->interval && tc >= am->timeout && !am->isEditMode()) {
      am->process(gdi, oe, SyncTimer);
      setTimer(am);
      reload=true;
    }
  }

  DWORD d=0;
  if (reload && !editMode && gdi.getData("AutoPage", d) && d)
    loadPage(gdi);
 }

void TabAuto::setTimer(AutoMachine *am)
{
  DWORD tc=GetTickCount();

  if (am->interval>0) {
    DWORD to=am->interval*1000+tc;

    if (to<tc) { //Warp-around. No reasonable way to handle it
      to=DWORD(0);
      am->interval=0;
    }

    am->timeout=to;
  }
}

int TabAuto::processButton(gdioutput &gdi, const ButtonInfo &bu)
{

  if (bu.id=="GenerateCMP") {
#ifndef MEOSDB
    int nClass=gdi.getTextNo("nClass");
    int nRunner=gdi.getTextNo("nRunner");

    if (nRunner>0 &&
      gdi.ask(L"Vill du dumpa aktuellt tävling och skapa en testtävling?")) {
      oe->generateTestCompetition(nClass, nRunner, gdi.isChecked("UseRelay"));
      gdi.getTabs().get(TCmpTab)->loadPage(gdi);
      return 0;
    }
#endif
  }
  else if (bu.id == "BrowseFolder") {
    const wchar_t *edit = bu.getExtra();
    wstring currentPath = gdi.getText(gdi.narrow(edit));
    wstring newPath = gdi.browseForFolder(currentPath, 0);
    if (!newPath.empty())
      gdi.setText(edit, newPath);
  }
  else if (bu.id == "StartBackup") {
    SaveMachine *sm=dynamic_cast<SaveMachine*>(getMachine(bu.getExtraInt()));
    if (sm) 
      sm->saveSettings(gdi);
    updateSyncInfo();
    loadPage(gdi);
  }
  else if (bu.id=="Result") {
    PrintResultMachine *sm=dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mPrintResultsMachine);
  }
  else if (bu.id == "BrowseFile") {
    static int index = 0;
    vector< pair<wstring, wstring> > ext;
    ext.push_back(make_pair(L"Webbdokument", L"*.html;*.htm"));

    wstring file = gdi.browseForSave(ext, L"html", index);
    if (!file.empty())
      gdi.setText("ExportFile", file);
  }
  else if (bu.id == "BrowseScript") {
    vector< pair<wstring, wstring> > ext;
    ext.push_back(make_pair(L"Skript", L"*.bat;*.exe;*.js"));

    wstring file = gdi.browseForOpen(ext, L"bat");
    if (!file.empty())
      gdi.setText("ExportScript", file);
  }
  else if (bu.id == "DoExport") {
    bool stat = gdi.isChecked(bu.id);
    gdi.setInputStatus("ExportFile", stat);
    gdi.setInputStatus("ExportScript", stat);
    gdi.setInputStatus("BrowseFile", stat);
    gdi.setInputStatus("BrowseScript", stat);
    gdi.setInputStatus("HTMLRefresh", stat);
    gdi.setInputStatus("StructuredExport", stat);
  }
  else if (bu.id == "DoPrint") {
    bool stat = gdi.isChecked(bu.id);
    gdi.setInputStatus("PrinterSetup", stat);
  }
  else if (bu.id=="Splits") {
    SplitsMachine *sm=dynamic_cast<SplitsMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mSplitsMachine);
  }
  else if (bu.id=="Prewarning") {
    PrewarningMachine *sm=dynamic_cast<PrewarningMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mPrewarningMachine);
  }
  else if (bu.id=="Punches") {
    PunchMachine *sm=dynamic_cast<PunchMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mPunchMachine);
  }
  else if (bu.id=="OnlineResults") {
    OnlineResults *sm=dynamic_cast<OnlineResults*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mOnlineResults);
  }
  else if (bu.id=="OnlineInput") {
    OnlineInput *sm=dynamic_cast<OnlineInput*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mOnlineInput);
  }
  else if (bu.id=="SaveBackup") {
    SaveMachine *sm=dynamic_cast<SaveMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, mSaveBackup);
  }
  else if (bu.id == "InfoService") {
    settings(gdi, getMachine(bu.getExtraInt()), mInfoService);
  }
  else if (bu.id=="StartResult") {
#ifndef MEOSDB
    wstring minute=gdi.getText("Interval");
    int t=convertAbsoluteTimeMS(minute);

    if (t<2 || t>7200) {
      gdi.alert("Intervallet måste anges på formen MM:SS.");
    }
    else {
      PrintResultMachine *prm=dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));

      if (prm) {
        prm->interval = t;
        prm->doExport = gdi.isChecked("DoExport");
        prm->doPrint = gdi.isChecked("DoPrint");
        prm->exportFile = gdi.getText("ExportFile");
        prm->exportScript = gdi.getText("ExportScript");
        prm->structuredExport = gdi.isChecked("StructuredExport");
        prm->htmlRefresh = gdi.isChecked("HTMLRefresh") ? t : 0;
        if (!prm->readOnly) {
          gdi.getSelection("Classes", prm->classesToPrint);

          ListBoxInfo lbi;
          if (gdi.getSelectedItem("ListType", lbi)) {
            oListParam par;
            par.selection=prm->classesToPrint;
            par.listCode = EStdListType(lbi.data);
            par.pageBreak = gdi.isChecked("PageBreak");
            par.showInterTimes = gdi.isChecked("ShowInterResults");
            par.splitAnalysis = gdi.isChecked("SplitAnalysis");
            int legNr = gdi.getSelectedItem("LegNumber").first;
            if (legNr >= 0)
              par.setLegNumberCoded(legNr);
            else
              par.setLegNumberCoded(0);

            oe->generateListInfo(par, gdi.getLineHeight(), prm->listInfo);
          }
        }
        prm->po.onlyChanged = gdi.isChecked("OnlyChanged");
        prm->pageBreak = gdi.isChecked("PageBreak");
        prm->showInterResult = gdi.isChecked("ShowInterResults");
        prm->splitAnalysis = gdi.isChecked("SplitAnalysis");
        prm->synchronize=true; //To force continuos data sync.
        setTimer(prm);
      }
      updateSyncInfo();
      loadPage(gdi);
    }
#endif
  }
  else if (bu.id=="StartSplits") {
    
    wstring ivt = gdi.getText("Interval");
    
    int iv = gdi.getTextNo("Interval");
    const wstring &file=gdi.getText("FileName");

    if (!ivt.empty() && (iv < 1 || iv > 7200)) {
      throw meosException(L"Ogiltigt antal sekunder: X#" + gdi.getText("Interval"));
    }

    if (file.empty()) {
      throw meosException("Filnamnet får inte vara tomt");
    }

    //Try exporting.
    oe->exportIOFSplits(oEvent::IOF20, file.c_str(), true, false,
                        set<int>(), -1, false, true, true, false);
    SplitsMachine *sm=dynamic_cast<SplitsMachine*>(getMachine(bu.getExtraInt()));

    if (sm) {
      sm->interval=iv;

      sm->file=file;
      sm->synchronize=true;
      setTimer(sm);
    }
    updateSyncInfo();
    loadPage(gdi);
  }
  else if (bu.id=="Save") { // General save
    AutoMachine *sm=getMachine(bu.getExtraInt());
    if (sm) {
      sm->save(*oe, gdi);
      setTimer(sm);
    }
    updateSyncInfo();
    loadPage(gdi);
  }
  else if (bu.id=="StartPrewarning") {
    PrewarningMachine *pwm=dynamic_cast<PrewarningMachine*>(getMachine(bu.getExtraInt()));

    if (pwm) {
      pwm->waveFolder=gdi.getText("WaveFolder");
      gdi.getSelection("Controls", pwm->controls);

      oe->synchronizeList(oLPunchId);
      oe->clearPrewarningSounds();

      pwm->synchronizePunches=true;
      pwm->controlsSI.clear();
      for (set<int>::iterator it=pwm->controls.begin();it!=pwm->controls.end();++it) {
        pControl pc=oe->getControl(*it, false);

        if (pc)
          pwm->controlsSI.insert(pc->Numbers, pc->Numbers+pc->nNumbers);
      }
    }
    updateSyncInfo();
    loadPage(gdi);
  }
  else if (bu.id=="StartPunch") {

    wstring minute=gdi.getText("Interval");
    int t=_wtoi(minute.c_str());

    if (t<1 || t>7200) {
      throw meosException(L"Ogiltigt antal sekunder: X#" + minute);
    }
    else {
      PunchMachine *pm=dynamic_cast<PunchMachine*>(getMachine(bu.getExtraInt()));

      if (pm) {
        pm->interval=t;
        pm->radio=gdi.getTextNo("Radio");
        setTimer(pm);
      }
    }
    updateSyncInfo();
    loadPage(gdi);
  }
  else if (bu.id == "Cancel") {
    loadPage(gdi);
  }
  else if (bu.id == "Stop") {
    if (bu.getExtraInt())
      stopMachine(getMachine(bu.getExtraInt()));

    updateSyncInfo(); 
    loadPage(gdi);
  }
  else if (bu.id == "PrinterSetup") {
    PrintResultMachine *prm =
          dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));

    if (prm) {
      gdi.printSetup(prm->po);
    }
  }
  else if (bu.id == "PrintNow") {
     PrintResultMachine *prm =
          dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));

     if (prm) {
       prm->process(gdi, oe, SyncNone);
       setTimer(prm);
       loadPage(gdi);
     }
  }
  else if (bu.id == "SelectAll") {
    const wchar_t *ctrl = bu.getExtra();
    set<int> lst;
    lst.insert(-1);
    gdi.setSelection(ctrl, lst);
  }
  else if (bu.id == "SelectNone") {
    const wchar_t *ctrl= bu.getExtra();
    set<int> lst;
    gdi.setSelection(ctrl, lst);
  }
  else if (bu.id == "TestVoice") {
    PrewarningMachine *pwm=dynamic_cast<PrewarningMachine*>(getMachine(bu.getExtraInt()));

    if (pwm)
      oe->tryPrewarningSounds(pwm->waveFolder, rand()%400+1);
  }
  else if ( bu.id == "WaveBrowse") {
    wstring wf=gdi.browseForFolder(gdi.getText("WaveFolder"), 0);

    if (wf.length()>0)
      gdi.setText("WaveFolder", wf);
  }
  else if ( bu.id == "BrowseSplits") {
    int index=0;
    vector< pair<wstring, wstring> > ext;
    ext.push_back(make_pair(L"Sträcktider", L"*.xml"));

    wstring wf = gdi.browseForSave(ext, L"xml", index);

    if (!wf.empty())
      gdi.setText("FileName", wf);
  }

  return 0;
}

int TabAuto::processListBox(gdioutput &gdi, const ListBoxInfo &bu)
{
  return 0;
}

bool TabAuto::stopMachine(AutoMachine *am)
{
  list<AutoMachine *>::iterator it;
  for (it=machines.begin(); it!=machines.end(); ++it)
    if (am==*it)  {
      if (am->stop()) {
        delete am;
        machines.erase(it);
        return true;
      }
    }
  return false;
}

void TabAuto::settings(gdioutput &gdi, AutoMachine *sm, Machines ms) {
    editMode=true;
    bool createNew = (sm==0);
    if (!sm) {
      sm = AutoMachine::construct(ms);
      machines.push_back(sm);
    }

    gdi.restore("", false);
    gdi.dropLine();
    int cx = gdi.getCX();
    int cy = gdi.getCY();
    int d = gdi.scaleLength(6);
    gdi.setCX(cx + d);
    sm->setEditMode(true);
    sm->settings(gdi, *oe, createNew);
    int w = gdi.getWidth();
    int h = gdi.getHeight();

    RECT rc;
    rc.top = cy - d;
    rc.bottom = h + d;
    rc.left = cx - d;
    rc.right = w + d;
    gdi.addRectangle(rc, colorLightBlue, true, true);
    gdi.refresh();
}

void TabAuto::killMachines()
{
  while(!machines.empty()) {
    machines.back()->stop();
    delete machines.back();
    machines.pop_back();
  }
  AutoMachine::resetGlobalId();
}

bool TabAuto::loadPage(gdioutput &gdi)
{
  oe->checkDB();
  tabAuto=this;
  editMode=false;
  gdi.selectTab(tabId);
  DWORD isAP = 0;
  gdi.getData("AutoPage", isAP);
  int storedOY = 0;
  int storedOX = 0;
  if (isAP) {
    storedOY = gdi.GetOffsetY();
    storedOX = gdi.GetOffsetX();
  }

  gdi.clearPage(false);
  gdi.setData("AutoPage", 1);
  gdi.addString("", boldLarge, "Automater");
  gdi.setRestorePoint();

  gdi.addString("", 10, "help:10000");

  gdi.dropLine();
  gdi.addString("", fontMediumPlus, "Tillgängliga automater").setColor(colorDarkBlue);
  gdi.dropLine();
  gdi.fillRight();
  gdi.pushX();
  gdi.addButton("Result", "Resultatutskrift / export", AutomaticCB, "tooltip:resultprint");
  gdi.addButton("OnlineResults", "Resultat online", AutomaticCB, "Publicera resultat direkt på nätet");
  gdi.addButton("OnlineInput", "Inmatning online", AutomaticCB, "Hämta stämplingar m.m. från nätet");
  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("SaveBackup", "Säkerhetskopiering", AutomaticCB);
  gdi.addButton("InfoService", "Informationsserver", AutomaticCB);
  gdi.addButton("Punches", "Stämplingstest", AutomaticCB, "Simulera inläsning av stämplar");
  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("Splits", "Sträcktider (WinSplits)", AutomaticCB, "Spara sträcktider till en fil för automatisk synkronisering med WinSplits");
  gdi.addButton("Prewarning", "Förvarningsröst", AutomaticCB, "tooltip:voice");


  gdi.fillDown();
  gdi.dropLine(3);
  gdi.popX();

  if (!machines.empty()) {
    gdi.addString("", fontMediumPlus, "Startade automater").setColor(colorDarkBlue);;
    list<AutoMachine *>::iterator it;

    int baseX = gdi.getCX();
    int dx = gdi.scaleLength(6);

    for (it=machines.begin(); it!=machines.end(); ++it) {
      AutoMachine *am=*it;
      if (am) {
        RECT rc;
        rc.left = baseX;
        rc.right = gdi.scaleLength(500);
        rc.top = gdi.getCY();
        gdi.dropLine(0.5);
        gdi.setCX(baseX+dx);
        am->setEditMode(false);
        am->status(gdi);
        gdi.setCX(baseX);
        gdi.dropLine(0.5);
        rc.bottom = gdi.getCY();
        gdi.addRectangle(rc, colorLightGreen, true, true);
        gdi.dropLine();
      }
    }
    gdi.dropLine();
  }

  if (isAP) {
    gdi.setOffset(storedOY, storedOY, true);
  }

  gdi.refresh();
  return true;
}

void AutoMachine::settingsTitle(gdioutput &gdi, char *title) {
  gdi.dropLine(0.5);
  gdi.addString("", fontMediumPlus, title).setColor(colorDarkBlue);
  gdi.dropLine(0.5);
}

void AutoMachine::startCancelInterval(gdioutput &gdi, char *startCommand, bool created, IntervalType type, const wstring &intervalIn) {
  gdi.pushX();
  gdi.fillRight();
  if (type == IntervalMinute)
    gdi.addInput("Interval", intervalIn, 7, 0, L"Tidsintervall (MM:SS):");
  else if (type == IntervalSecond)
    gdi.addInput("Interval", intervalIn, 7, 0, L"Tidsintervall (sekunder):");
  gdi.dropLine(1);
  gdi.addButton(startCommand, created ? "Starta automaten" : "OK", AutomaticCB).setExtra(getId());
  if (!created)
    gdi.addButton("Cancel", "Avbryt", AutomaticCB).setExtra(getId());

  gdi.addButton("Stop", created ? "Avbryt" : "Stoppa automaten", AutomaticCB).setExtra(getId());

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(2.5);
  int dx = gdi.scaleLength(3);
  RECT rc;
  rc.left = gdi.getCX() - dx;
  rc.right = rc.left + gdi.scaleLength(450);
  rc.top = gdi.getCY();
  rc.bottom = rc.top + dx;
  gdi.addRectangle(rc, colorDarkBlue, false, false);
  gdi.dropLine();
}


void PrintResultMachine::settings(gdioutput &gdi, oEvent &oe, bool created) {
  settingsTitle(gdi, "Resultatutskrift / export");
  wstring time=created ? L"10:00" : getTimeMS(interval);
  startCancelInterval(gdi, "StartResult", created, IntervalMinute, time);

  if (created) {
    oe.getAllClasses(classesToPrint);
  }

  gdi.pushX();
  gdi.fillRight();
  gdi.addCheckbox("DoPrint", "Skriv ut", AutomaticCB, doPrint);
  gdi.dropLine(-0.5);
  gdi.addButton("PrinterSetup", "Skrivare...", AutomaticCB, "Välj skrivare...").setExtra(getId());

  gdi.dropLine(4);
  gdi.popX();
  gdi.addCheckbox("DoExport", "Exportera", AutomaticCB, doExport);
  gdi.dropLine(-1);
  int cx = gdi.getCX();
  gdi.addInput("ExportFile", exportFile, 32, 0, L"Fil att exportera till:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseFile", "Bläddra...", AutomaticCB);
  gdi.setCX(cx);
  gdi.dropLine(2.3);
  gdi.addCheckbox("StructuredExport", "Strukturerat exportformat", 0, structuredExport);
  gdi.addCheckbox("HTMLRefresh", "HTML med AutoRefresh", 0, htmlRefresh != 0);
  gdi.dropLine(1.2);
  gdi.setCX(cx);
  gdi.addInput("ExportScript", exportScript, 32, 0, L"Skript att köra efter export:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseScript", "Bläddra...", AutomaticCB);
  gdi.dropLine(3);
  gdi.popX();

  gdi.setInputStatus("ExportFile", doExport);
  gdi.setInputStatus("ExportScript", doExport);
  gdi.setInputStatus("BrowseFile", doExport);
  gdi.setInputStatus("BrowseScript", doExport);
  gdi.setInputStatus("StructuredExport", doExport);
  gdi.setInputStatus("HTMLRefresh", doExport);
  gdi.setInputStatus("PrinterSetup", doPrint);

  if (!readOnly) {
    gdi.fillDown();
    gdi.addString("", 1, "Listval");
    gdi.dropLine();
    gdi.fillRight();
    gdi.addListBox("Classes", 150,300,0, L"", L"", true);
    gdi.pushX();
    gdi.fillDown();
    vector< pair<wstring, size_t> > d;
    gdi.addItem("Classes", oe.fillClasses(d, oEvent::extraNone, oEvent::filterNone));
    gdi.setSelection("Classes", classesToPrint);

    gdi.addSelection("ListType", 200, 100, 0, L"Lista");
    oe.fillListTypes(gdi, "ListType", 1);
    if (notShown) {
      notShown = false;
      ClassConfigInfo cnf;
      oe.getClassConfigurationInfo(cnf);
      int type = EStdResultListLARGE;
      if (cnf.hasRelay())
        type = EStdTeamAllLegLARGE;
      else if (cnf.hasPatrol())
        type = EStdPatrolResultListLARGE;

      gdi.selectItemByData("ListType", type);
    }
    else
      gdi.selectItemByData("ListType", listInfo.getListCode());

    gdi.addSelection("LegNumber", 140, 300, 0, L"Sträcka:");
    set<int> clsUnused;
    vector< pair<wstring, size_t> > out;
    oe.fillLegNumbers(clsUnused, listInfo.isTeamList(), true, out);
    gdi.addItem("LegNumber", out);
    gdi.selectItemByData("LegNumber", listInfo.getLegNumberCoded());

    gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser", 0, pageBreak);
    gdi.addCheckbox("ShowInterResults", "Visa mellantider", 0, showInterResult,
                                        "Mellantider visas för namngivna kontroller.");
    gdi.addCheckbox("SplitAnalysis", "Med sträcktidsanalys", 0, splitAnalysis);

    gdi.addCheckbox("OnlyChanged", "Skriv endast ut ändade sidor", 0, po.onlyChanged);

    gdi.popX();
    gdi.addButton("SelectAll", "Välj allt", AutomaticCB, "").setExtra(L"Classes");
    gdi.popX();
    gdi.addButton("SelectNone", "Välj inget", AutomaticCB, "").setExtra(L"Classes");
  }
  else {
    gdi.fillDown();
    gdi.addString("", 1, L"Lista av typ 'X'#" + listInfo.getName());
    gdi.addCheckbox("OnlyChanged", "Skriv endast ut ändade sidor", 0, po.onlyChanged);
  }
}

void PrintResultMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast)
{
  if (lock)
    return;
  
  if (ast!=SyncDataUp) {
    wstring printError;
    lock = true;
    try {
      gdioutput gdiPrint("print", gdi.getScale(), gdi.getCP());
      gdiPrint.clearPage(false);
      oe->generateList(gdiPrint, true, listInfo, false);
      if (doPrint) {
        gdiPrint.refresh();
        try {
          gdiPrint.print(po, oe);
        }
        catch (const meosException &ex) {
          printError = ex.wwhat();
          if (printError.empty())
            printError = L"Printing failed (X: Y) Z#Auto#0#Unknown";
        }
      }
      if (doExport) {
        if (!exportFile.empty()) {
          if (structuredExport)
            gdiPrint.writeTableHTML(exportFile, oe->getName(), htmlRefresh);
          else
            gdiPrint.writeHTML(exportFile, oe->getName(), htmlRefresh);

          if (!exportScript.empty()) {
            ShellExecute(NULL, NULL, exportScript.c_str(), exportFile.c_str(), NULL, SW_HIDE);
          }
        }
      }
    }
    catch (...) {
      lock = false;
      throw;
    }
    lock = false;

    if (!printError.empty() && !errorLock) {
      errorLock = true;
      gdi.alert(printError);
      errorLock = false;
    }
  }
}

void PrintResultMachine::status(gdioutput &gdi)
{
  gdi.fillRight();
  gdi.pushX();
  gdi.addString("", 0, name);
  gdi.addString("", 0, listInfo.getName());
  gdi.dropLine();
  if (doExport) {
    gdi.popX();
    gdi.addString("", 0, "Målfil: ");
    gdi.addStringUT(0, exportFile).setColor(colorRed);
    gdi.dropLine();
  }
  gdi.fillRight();
  gdi.popX();
  if (interval>0){
    gdi.addString("", 0, "Automatisk utskrift / export: ");
    gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign, (GetTickCount()-timeout)/1000);
  }
  else {

  }
  gdi.popX();
  gdi.dropLine(2);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.addButton("PrintNow", "Exportera nu", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("Result", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void PrewarningMachine::settings(gdioutput &gdi, oEvent &oe, bool created) {
  settingsTitle(gdi, "Förvarningsröst");
  startCancelInterval(gdi, "StartPrewarning", created, IntervalNone, L"");

  gdi.addString("", 10, "help:computer_voice");

  gdi.pushX();
  gdi.fillRight();
  gdi.addInput("WaveFolder", waveFolder, 32, 0, L"Ljudfiler, baskatalog.");

  gdi.fillDown();
  gdi.dropLine();
  gdi.addButton("WaveBrowse", "Bläddra...", AutomaticCB);
  gdi.popX();

  gdi.addListBox("Controls", 100, 200, 0, L"", L"", true);
  gdi.pushX();
  gdi.fillDown();
  vector< pair<wstring, size_t> > d;
  oe.fillControls(d, oEvent::CTCourseControl);
  gdi.addItem("Controls", d);
  gdi.setSelection("Controls", controls);
  gdi.popX();
  gdi.addButton("SelectAll", "Välj alla", AutomaticCB, "").setExtra(L"Controls");
  gdi.popX();
}

void PrewarningMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast)
{
  oe->playPrewarningSounds(waveFolder, controlsSI);
}

void PrewarningMachine::status(gdioutput &gdi)
{
  gdi.addString("", 1, name);

  string info="Förvarning på (SI-kod): ";
  bool first=true;

  if (controls.empty())
    info+="alla stämplingar";
  else {
    for (set<int>::iterator it=controlsSI.begin();it!=controlsSI.end();++it) {
      char bf[32];
      _itoa_s(*it, bf, 10);

      if (!first) info+=", ";
      else       first=false;

      info+=bf;
    }
  }
  gdi.addString("", 0, info);
  gdi.fillRight();
  gdi.pushX();

  gdi.popX();
  gdi.dropLine(0.3);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.addButton("TestVoice", "Testa rösten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("Prewarning", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void PunchMachine::settings(gdioutput &gdi, oEvent &oe, bool created) {
  settingsTitle(gdi, "Test av stämplingsinläsningar");
  wstring time=created ? L"10" : itow(interval);
  startCancelInterval(gdi, "StartPunch", created, IntervalSecond, time);

  gdi.addString("", 10, "help:simulate");

  gdi.pushX();
  gdi.fillRight();
  gdi.dropLine();

  gdi.addString("", 0, "Radiotider, kontroll:");
  gdi.dropLine(-0.2);
  gdi.addInput("Radio", L"", 6, 0);

  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(5);
  gdi.addString("", 1, "Generera testtävling");
  gdi.fillRight();
  gdi.addInput("nRunner", L"100", 10, 0, L"Antal löpare");
  gdi.addInput("nClass", L"10", 10, 0, L"Antal klasser");
  gdi.dropLine();
  gdi.addCheckbox("UseRelay", "Med stafettklasser");
  gdi.addButton("GenerateCMP", "Generera testtävling", AutomaticCB);
}

void PunchMachine::status(gdioutput &gdi)
{
  gdi.addString("", 1, name);
  gdi.fillRight();
  gdi.pushX();
  if (interval>0){
    gdi.addString("", 0, "Stämplar om: ");
    gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign|timeSeconds, (GetTickCount()-timeout)/1000);
    gdi.addString("", 0, "(sekunder)");
  }
  else {

  }
  gdi.popX();
  gdi.dropLine(2);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("Punches", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void PunchMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast)
{
#ifndef MEOSDB
  SICard sic;
  oe->generateTestCard(sic);
  SportIdent &si = TabSI::getSI(gdi);
  if (!sic.empty()) {
    if (!radio) si.addCard(sic);
  }
  else gdi.addInfoBox("", L"Failed to generate card.", interval*2);

  if (radio && !sic.empty()) {
    pRunner r=oe->getRunnerByCardNo(sic.CardNumber, 0, false);
    if (r && r->getCardNo()) {
      sic.CardNumber=r->getCardNo();
      sic.PunchOnly=true;
      sic.nPunch=1;
      sic.Punch[0].Code=radio;
      si.addCard(sic);
    }
  }
#endif
}

void SplitsMachine::settings(gdioutput &gdi, oEvent &oe, bool created) {
  wstring time;
  if (interval>0)
    time = itow(interval);
  else if (created)
    time = L"30";

  settingsTitle(gdi, "Sträcktider / WinSplits");
  startCancelInterval(gdi, "StartSplits", created, IntervalSecond, time);

  gdi.addString("",  0, "Intervall (sekunder). Lämna blankt för att uppdatera när "
                                        "tävlingsdata ändras.");
  gdi.dropLine();

  gdi.addString("", 10, "help:winsplits_auto");


  gdi.dropLine();
  gdi.fillRight();
  gdi.addInput("FileName", file, 30, 0, L"Filnamn:");
  gdi.dropLine(0.9);
  gdi.addButton("BrowseSplits", "Bläddra...", AutomaticCB);

  gdi.popX();
  gdi.dropLine(2);
  
}

void SplitsMachine::status(gdioutput &gdi)
{
  gdi.addString("", 1, name);
  if (!file.empty()) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, L"Fil: X#" + file);

    if (interval>0){
      gdi.popX();
      gdi.dropLine(1);
      gdi.addString("", 0, "Skriver sträcktider om: ");
      gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign|timeSeconds, (GetTickCount()-timeout)/1000);
      gdi.addString("", 0, "(sekunder)");
    }
    else {
      gdi.dropLine(1);
      gdi.addString("", 0, "Skriver sträcktider när tävlingsdata ändras.");
    }

    gdi.popX();
  }
  gdi.dropLine(2);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("Splits", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void SplitsMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast)
{
  if ((interval>0 && ast==SyncTimer) || (interval==0 && ast==SyncDataUp)) {
    if (!file.empty())
      oe->exportIOFSplits(oEvent::IOF20, file.c_str(), true, false, classes,
                          leg, false, true, true, false);
  }
}

void SaveMachine::status(gdioutput &gdi) {
  gdi.addString("", 1, name);
  if (!baseFile.empty()) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, L"Destination: X#" + baseFile);

    if (interval>0){
      gdi.popX();
      gdi.dropLine(1);
      gdi.addString("", 0, "Säkerhetskopierar om: ");
      gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign, (GetTickCount()-timeout)/1000);
    }
    
    gdi.popX();
  }
  gdi.dropLine(2);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("SaveBackup", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void SaveMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  if (interval>0 && ast==SyncTimer) {
    if (!baseFile.empty()) {
      wstring file = baseFile + L"meos_backup_" + oe->getDate() + L"_" + itow(saveIter++) + L".xml";
      oe->autoSynchronizeLists(true);
      oe->save(file);
    }
  }
}

void SaveMachine::settings(gdioutput &gdi, oEvent &oe, bool created) {
  settingsTitle(gdi, "Säkerhetskopiering");
  wstring time=created ? L"10:00" : getTimeMS(interval);
  startCancelInterval(gdi, "StartBackup", created, IntervalMinute, time);

  int cx = gdi.getCX();
  gdi.addInput("BaseFile", baseFile, 32, 0, L"Mapp:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseFolder", "Bläddra...", AutomaticCB).setExtra(L"BaseFile");
  gdi.setCX(cx);
}

void SaveMachine::saveSettings(gdioutput &gdi) {
  
  wstring minute=gdi.getText("Interval");
  int t=convertAbsoluteTimeMS(minute);

  if (t<2 || t>7200) {
    throw meosException("Intervallet måste anges på formen MM:SS.");
  }
  wstring f = gdi.getText("BaseFile");
  if (f.empty()) {
    throw meosException("Filnamnet får inte vara tomt");
  }

  if (*f.rbegin() != '\\' && *f.rbegin() != '/')
    f += L"\\";

  wstring sample = f + L"sample.txt";
  ofstream fout(sample.c_str(), ios_base::trunc|ios_base::out);
  bool bad = false;
  if (fout.bad()) 
    bad = true;
  else {
    fout << "foo" << endl;
    fout.close();
    bad = fout.bad();
    _wremove(sample.c_str());
  }
  if (bad)
    throw meosException(L"Ogiltig destination X#" + f);

  baseFile = f;
  interval = t;
}

void TabAuto::clearCompetitionData() {
}
