/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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

#include "oEvent.h"
#include "gdioutput.h"
#include "SportIdent.h"
#include "onlineresults.h"
#include "onlineinput.h"
#include "RestService.h"
#include "printresultservice.h"

#include "TabAuto.h"
#include "TabSI.h"
#include "TabCompetition.h"
#include "meos_util.h"

#include "meosexception.h"
#include "machinecontainer.h"

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

shared_ptr<AutoMachine> AutoMachine::construct(Machines ms) {
  switch(ms) {
  case mPrintResultsMachine:
    return make_shared<PrintResultMachine>(0);
  case mSplitsMachine:
    return make_shared<SplitsMachine>();
  case mPrewarningMachine:
    return make_shared<PrewarningMachine>();
  case mPunchMachine:
    return make_shared<PunchMachine>();
  case mOnlineInput:
    return make_shared<OnlineInput>();
  case mOnlineResults:
    return make_shared<OnlineResults>();
  case mSaveBackup:
    return make_shared<SaveMachine>();
  case mInfoService:
    return make_shared<RestService>();
  }
  throw meosException("Invalid machine");
}

void TabAuto::removedList(EStdListType type) {
  vector<AutoMachine*> amToRemove;
  for (auto& ms : machines) {
    if (ms->requireList(type))
      amToRemove.push_back(ms.get());
  }
  for (AutoMachine* am : amToRemove)
    stopMachine(am);

  for (auto& m : oe->getMachineContainer().enumerate()) {
    Machines mtype = AutoMachine::getType(m.first);
    auto am = AutoMachine::construct(mtype);

    am->loadMachine(*oe, m.second);
    if (am->requireList(type))
      oe->getMachineContainer().erase(m.first, m.second);
  }
}

AutoMachine *TabAuto::getMachine(int id) {
  if (id == 0)
    return 0;
  
  for (auto &m : machines) {
    if (m != nullptr && m->getId() == id) {
      return m.get();
    }
  }
  throw meosException("Service X not found.#" + itos(id));
}

void AutoMachine::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  wstring oldMachineName = getMachineName();
  machineName = gdi.getText("MachineName");
  wstring newMachineName = getMachineName();
  oe.getMachineContainer().rename(getTypeString(), oldMachineName, newMachineName);
}

void AutoMachine::status(gdioutput &gdi) {
  gdi.pushX();
  if (machineName.empty())
    gdi.addString("", 1, name);
  else
    gdi.addString("", 1, L"#" + lang.tl(name) + L" (" + machineName + L")");
}

TabAuto::~TabAuto(void) {
  tabAuto = nullptr;
}

void TabAuto::tabAutoKillMachines(){
  if (tabAuto)
    tabAuto->killMachines();
}

void TabAuto::tabAutoRegister(TabAuto *ta)
{
  tabAuto=ta;
}

AutoMachine &TabAuto::tabAutoAddMachinge(const AutoMachine &am)
{
  if (tabAuto) 
    return tabAuto->addMachine(am);
  throw meosException("Internal error");
}

bool TabAuto::hasActiveReconnectionMachine()
{
  if (tabAuto)
    return tabAuto->hasActiveReconnection();
  return false;
}

bool TabAuto::hasActiveReconnection() const {
  for (auto am : machines) {
    if (am->getType() == Machines::mMySQLReconnect)
      return true;
  }

  return false;
}

void tabForceSync(gdioutput &gdi, pEvent oe) {
  if (tabAuto)
    tabAuto->syncCallback(gdi);
}

int AutomaticCB(gdioutput *gdi, GuiEventType type, BaseInfo* data) {
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

    case GuiEventType::GUI_POSTCLEAR:
    case GuiEventType::GUI_CLEAR:
      return tabAuto->clearPage(*gdi, type == GUI_POSTCLEAR);
  }
  return 0;
}

void TabAuto::syncCallback(gdioutput& gdi)
{
  list<AutoMachine*> toRemove;
  wstring msg;
  
  for (auto &am : machines) {
    try {      
      if (am && am->synchronize && !am->isEditMode())
        am->process(gdi, oe, SyncDataUp);
      if (am && am->removeMe())
        toRemove.push_back(am.get());
    }
    catch (meosException& ex) {
      msg = ex.wwhat();
    }
    catch (std::exception& ex) {
      msg = gdi.widen(ex.what());
    }
    catch (...) {
      msg = L"Ett okänt fel inträffade.";
    }
  }

  for (auto* r : toRemove) {
    stopMachine(r);
  }

  if (!msg.empty()) {
    gdi.alert(msg);
    gdi.setWaitCursor(false);
  }
}

void TabAuto::updateSyncInfo() {
  synchronize=false;
  synchronizePunches=false;

  for (auto &am : machines) {
    if (am){
      am->synchronize= am->synchronize || am->synchronizePunches;
      synchronize=synchronize || am->synchronize;
      synchronizePunches=synchronizePunches || am->synchronizePunches;
    }
  }
}

void TabAuto::timerCallback(gdioutput &gdi) {
  uint64_t tc = GetTickCount64();  
  bool reload=false;
  list<AutoMachine*> toRemove;
  wstring msg;

  for (auto &am : machines) {
    if (am && am->interval && tc >= am->timeout && !am->isEditMode()) {
      try {
        am->process(gdi, oe, SyncTimer);
      }
      catch (meosException& ex) {
        msg = ex.wwhat();
      }
      catch (std::exception& ex) {
        msg = gdi.widen(ex.what());
      }
      catch (...) {
        msg = L"Ett okänt fel inträffade.";
      }      
      reload = true;
      if (am->removeMe())
        toRemove.push_back(am.get());
      else 
        setTimer(am.get());
    }
  }
  for (auto* r : toRemove) {
    stopMachine(r);
  }

  DWORD d=0;
  if (reload && !editMode && gdi.getData("AutoPage", d) && d)
    loadPage(gdi, false);

  if (!msg.empty()) 
    throw meosException(msg);
 }

void TabAuto::setTimer(AutoMachine* am)
{
  uint64_t tc = GetTickCount64();

  if (am->interval > 0) {
    uint64_t to = am->interval * 1000 + tc;
    am->timeout = to;
  }
}


bool TabAuto::clearPage(gdioutput &gdi, bool postClear) {
  if (!postClear) {
    if (wasSaved) {
      AutoMachine* sm = getMachine(currentMachineEditId);
      if (sm && wasCreated)
        stopMachine(sm);
      if (sm)
        sm->cancelEdit();
    }
    else {
      auto ans = gdi.askCancel(wasCreated ? L"Vill du starta automaten?" : L"Vill du spara ändringar?");
      if (ans == gdioutput::AskAnswer::AnswerCancel)
        return false;
      else {
        AutoMachine* sm = getMachine(currentMachineEditId);
        if (sm) {
          if (ans == gdioutput::AskAnswer::AnswerYes) {
            sm->save(*oe, gdi, true);
            setTimer(sm);
            updateSyncInfo();
          }
          else {
            sm->cancelEdit();
            if (wasCreated)
              stopMachine(sm);
          }
        }
        return true;
      }
    }
  }
  else {
    currentMachineEditId = -1;
    wasSaved = false;
    wasCreated = false;
  }
  return true;
}

int TabAuto::processButton(gdioutput& gdi, const ButtonInfo& bu)
{

  if (bu.id == "GenerateCMP") {
    int nClass = gdi.getTextNo("nClass");
    int nRunner = gdi.getTextNo("nRunner");

    if (nRunner > 0 &&
      gdi.ask(L"Vill du dumpa aktuellt tävling och skapa en testtävling?")) {
      oe->generateTestCompetition(nClass, nRunner, gdi.isChecked("UseRelay"));
      gdi.getTabs().get(TCmpTab)->loadPage(gdi);
      return 0;
    }
  }
  else if (bu.id == "BrowseFolder") {
    const wchar_t* edit = bu.getExtra();
    wstring currentPath = gdi.getText(gdi.narrow(edit));
    wstring newPath = gdi.browseForFolder(currentPath, 0);
    if (!newPath.empty())
      gdi.setText(edit, newPath);
  }
  else if (bu.id == "Result") {
    settings(gdi, getMachine(bu.getExtraInt()), AutoMachine::State::Create, mPrintResultsMachine);
  }
  else if (bu.id == "BrowseFile") {
    static int index = 0;
    vector< pair<wstring, wstring> > ext;
    ext.push_back(make_pair(L"Webbdokument", L"*.html;*.htm"));

    wstring file = gdi.browseForSave(ext, L"html", index);
    if (!file.empty()) {
      gdi.setText("ExportFile", file);
      oe->setProperty("LastExportTarget", file);
    }
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
    if (gdi.hasWidget("HTMLRefresh")) {
      gdi.setInputStatus("HTMLRefresh", stat);
      gdi.setInputStatus("StructuredExport", stat);
    }
  }
  else if (bu.id == "DoPrint") {
    bool stat = gdi.isChecked(bu.id);
    gdi.setInputStatus("PrinterSetup", stat);
  }
  else if (bu.id == "Splits") {
    SplitsMachine* sm = dynamic_cast<SplitsMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, AutoMachine::State::Create, mSplitsMachine);
  }
  else if (bu.id == "Prewarning") {
    PrewarningMachine* sm = dynamic_cast<PrewarningMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, AutoMachine::State::Create, mPrewarningMachine);
  }
  else if (bu.id == "Punches") {
    PunchMachine* sm = dynamic_cast<PunchMachine*>(getMachine(bu.getExtraInt()));
    settings(gdi, sm, AutoMachine::State::Create, mPunchMachine);
  }
  else if (bu.id == "OnlineResults") {
    settings(gdi, getMachine(bu.getExtraInt()), AutoMachine::State::Create, mOnlineResults);
  }
  else if (bu.id == "OnlineInput") {
    settings(gdi, getMachine(bu.getExtraInt()), AutoMachine::State::Create, mOnlineInput);
  }
  else if (bu.id == "SaveBackup") {
    settings(gdi, getMachine(bu.getExtraInt()), AutoMachine::State::Create, mSaveBackup);
  }
  else if (bu.id == "InfoService") {
    settings(gdi, getMachine(bu.getExtraInt()), AutoMachine::State::Create, mInfoService);
  }
  else if (bu.id == "Save") { // General save
    AutoMachine* sm = getMachine(bu.getExtraInt());
    if (sm) {
      sm->save(*oe, gdi, true);
      setTimer(sm);
    }
    updateSyncInfo();
    loadPage(gdi, false);
  }
  else if (bu.id == "Cancel") {
    AutoMachine* sm = getMachine(bu.getExtraInt());
    if (sm) {
      sm->cancelEdit();
    }
    loadPage(gdi, false);
  }
  else if (bu.id == "Stop") {
    if (bu.getExtraInt())
      stopMachine(getMachine(bu.getExtraInt()));

    updateSyncInfo();
    loadPage(gdi, false);
  }
  else if (bu.id == "SaveMachine") {
    auto sm = getMachine(bu.getExtraInt());
    sm->save(*oe, gdi, false);
    wstring iv;
    if (gdi.hasWidget("Interval"))
      iv = gdi.getText("Interval");
    sm->saveMachine(*oe, iv);
    wasSaved = true;
    oe->updateChanged();
    oe->synchronize(false);
  }
  else if (bu.id == "Erase") {
    auto sm = getMachine(bu.getExtraInt());
    if (sm) {
      oe->getMachineContainer().erase(sm->getTypeString(), sm->getMachineName());
      oe->updateChanged();
      oe->synchronize();
      stopMachine(sm);
    }

    updateSyncInfo();
    loadPage(gdi, false);
  }
  else if (bu.id == "CreateLoad") {
    auto sm = oe->getMachineContainer().enumerate();
    int ix = bu.getExtraInt();
    if (ix < sm.size()) {
      auto& m = sm[ix];
      Machines type = AutoMachine::getType(m.first);
      auto am = AutoMachine::construct(type);

      machines.push_back(am);
      am->loadMachine(*oe, m.second);
      settings(gdi, am.get(), AutoMachine::State::Load, am->getType());
    }
  }
  else if (bu.id == "PrinterSetup") {
    PrintResultMachine* prm =
      dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));

    if (prm) {
      prm->printSetup(gdi);
    }
  }
  else if (bu.id == "PrintNow") {
    PrintResultMachine* prm =
      dynamic_cast<PrintResultMachine*>(getMachine(bu.getExtraInt()));

    if (prm) {
      prm->process(gdi, oe, SyncNone);
      setTimer(prm);
      loadPage(gdi, false);
    }
  }
  else if (bu.id == "SelectAll") {
    const wchar_t* ctrl = bu.getExtra();
    set<int> lst;
    lst.insert(-1);
    gdi.setSelection(ctrl, lst);
  }
  else if (bu.id == "SelectNone") {
    const wchar_t* ctrl = bu.getExtra();
    set<int> lst;
    gdi.setSelection(ctrl, lst);
  }
  else if (bu.id == "TestVoice") {
    PrewarningMachine* pwm = dynamic_cast<PrewarningMachine*>(getMachine(bu.getExtraInt()));

    if (pwm)
      oe->tryPrewarningSounds(pwm->waveFolder, rand() % 400 + 1);
  }
  else if (bu.id == "WaveBrowse") {
    wstring wf = gdi.browseForFolder(gdi.getText("WaveFolder"), 0);

    if (wf.length() > 0)
      gdi.setText("WaveFolder", wf);
  }
  else if (bu.id == "BrowseSplits") {
    int index = 0;
    vector< pair<wstring, wstring> > ext;
    ext.push_back(make_pair(L"Sträcktider", L"*.xml"));

    wstring wf = gdi.browseForSave(ext, L"xml", index);

    if (!wf.empty())
      gdi.setText("FileName", wf);
  }

  return 0;
}

int TabAuto::processListBox(gdioutput &gdi, const ListBoxInfo &bu) {
  return 0;
}

bool TabAuto::stopMachine(AutoMachine* am) {
  for (auto it = machines.begin(); it != machines.end(); ++it)
    if (am == it->get()) {
      if (am->stop()) {
        machines.erase(it);
        return true;
      }
    }
  return false;
}

void TabAuto::settings(gdioutput& gdi, AutoMachine* sm, AutoMachine::State state, Machines ms) {
  editMode = true;
  if (sm) {
    if (state == AutoMachine::State::Create)
      state = AutoMachine::State::Edit;

    ms = sm->getType();
  }
  else {
    state = AutoMachine::State::Create;
    auto sm_ptr = AutoMachine::construct(ms);
    machines.push_back(sm_ptr);
    sm = sm_ptr.get();
  }

  gdi.restore("", false);
  gdi.dropLine();
  int cx = gdi.getCX();
  int cy = gdi.getCY();
  int d = gdi.scaleLength(6);
  gdi.setCX(cx + d);
  sm->setEditMode(true);
  sm->settings(gdi, *oe, state);
  int w = gdi.getWidth();
  int h = gdi.getHeight();

  RECT rc;
  rc.top = cy - d;
  rc.bottom = h + d;
  rc.left = cx - d;
  rc.right = w + d;
  gdi.addRectangle(rc, colorLightBlue, true, true);
  gdi.setOnClearCb("auto", AutomaticCB);
  gdi.setPostClearCb("auto", AutomaticCB);
  currentMachineEditId = sm->getId();
  wasCreated = (state == AutoMachine::State::Create) || (state == AutoMachine::State::Load);
  wasSaved = false;
  gdi.refresh();
}

void TabAuto::killMachines() {
  while (!machines.empty()) {
    machines.back()->stop();
    machines.pop_back();
  }
  AutoMachine::resetGlobalId();
}

bool TabAuto::loadPage(gdioutput &gdi, bool showSettingsLast) {
  oe->checkDB();
  oe->synchronize();
  tabAuto=this;
  editMode=false;
  gdi.selectTab(tabId);
  DWORD isAP = 0;
  gdi.getData("AutoPage", isAP);
  int storedOY = 0;
  int storedOX = 0;
  if (isAP) {
    storedOY = gdi.getOffsetY();
    storedOX = gdi.getOffsetX();
  }

  gdi.clearPage(false);
  gdi.setData("AutoPage", 1);
  gdi.addString("", boldLarge, "Automater");
  gdi.setRestorePoint();
  gdi.fillDown();
  gdi.pushX();

  gdi.addString("", 10, "help:10000");
  
  auto sm = oe->getMachineContainer().enumerate();

  set<pair<string, wstring>> startedMachines;
  for (auto &m : machines) {
    string t;
    m->getType(t);
    startedMachines.emplace(t, m->machineName);
  }
  decltype(sm) sm2;
  for (auto &m : sm) {
    if (startedMachines.count(m) == 0)
      sm2.push_back(m);
  }
  sm2.swap(sm);

  if (sm.size() > 0) {
    gdi.dropLine();
    gdi.addStringUT(fontMediumPlus, lang.tl(L"Sparade automater", true)).setColor(colorDarkBlue);
    gdi.dropLine(0.3);
    gdi.fillRight();

    for (int ix = 0; ix < sm.size(); ix++) {
      if (ix > 0 && ix % 3 == 0) {
        gdi.dropLine(2.5);
        gdi.popX();
      }

      auto &m = sm[ix];
      Machines type = AutoMachine::getType(m.first);
      if (m.second == L"default")
        gdi.addButton("CreateLoad", AutoMachine::getDescription(type), AutomaticCB).setExtra(ix);
      else
        gdi.addButton("CreateLoad", L"#" + lang.tl(AutoMachine::getDescription(type)) 
                                + L" (" + m.second + L")", AutomaticCB).setExtra(ix);
    }
    gdi.popX();
    gdi.dropLine(2);
    gdi.fillDown();
  }

  gdi.dropLine();
  gdi.addStringUT(fontMediumPlus, lang.tl(L"Tillgängliga automater", true)).setColor(colorDarkBlue);
  gdi.dropLine(0.3);
  gdi.fillRight();
  gdi.addButton("Result", AutoMachine::getDescription(Machines::mPrintResultsMachine), AutomaticCB, L"tooltip:resultprint");
  gdi.addButton("OnlineResults", AutoMachine::getDescription(Machines::mOnlineResults), AutomaticCB, L"Publicera resultat direkt på nätet");
  gdi.addButton("OnlineInput", AutoMachine::getDescription(Machines::mOnlineInput), AutomaticCB, L"Hämta stämplingar m.m. från nätet");
  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("SaveBackup", AutoMachine::getDescription(Machines::mSaveBackup), AutomaticCB);
  gdi.addButton("InfoService", AutoMachine::getDescription(Machines::mInfoService), AutomaticCB);
  gdi.addButton("Punches", AutoMachine::getDescription(Machines::mPunchMachine), AutomaticCB, L"Simulera inläsning av stämplar");
  gdi.popX();
  gdi.dropLine(2.5);
  gdi.addButton("Splits", AutoMachine::getDescription(Machines::mSplitsMachine), AutomaticCB, L"Spara sträcktider till en fil för automatisk synkronisering med WinSplits");
  gdi.addButton("Prewarning", AutoMachine::getDescription(Machines::mPrewarningMachine), AutomaticCB, L"tooltip:voice");

  gdi.fillDown();
  gdi.dropLine(3);
  gdi.popX();

  if (!machines.empty()) {
    gdi.addStringUT(fontMediumPlus, lang.tl(L"Startade automater", true)).setColor(colorDarkBlue);
 
    int baseX = gdi.getCX();
    int dx = gdi.scaleLength(6);

    for (auto &am : machines) {
      if (am) {
        RECT rc;
        rc.left = baseX;
        rc.right = gdi.scaleLength(700);
        rc.top = gdi.getCY();
        gdi.dropLine(0.5);
        gdi.setCX(baseX+dx);
        am->setEditMode(false);
        am->status(gdi);
        gdi.dropLine(0.5);
        
        GDICOLOR color;
        if (am->getStatus() == AutoMachine::Status::Good)
          color = colorLightGreen;
        else {
          if (!am->getStatusMsg().empty()) {
            gdi.addString("", 1, am->getStatusMsg());
            gdi.dropLine();
          }
          if (am->getStatus() == AutoMachine::Status::Warning)
            color = colorLightYellow;
          else
            color = colorLightRed;
        }

        rc.bottom = gdi.getCY();
        gdi.setCX(baseX);

        gdi.addRectangle(rc, color, true, true);
        gdi.dropLine();
      }
    }
    gdi.dropLine();
  }

  if (isAP) {
    gdi.setOffset(storedOY, storedOY, true);
  }
  if (showSettingsLast && !machines.empty())
    settings(gdi, machines.rbegin()->get(), AutoMachine::State::Edit, Machines::Unknown);

  gdi.refresh();
  return true;
}

void AutoMachine::settingsTitle(gdioutput &gdi, const char *title) {
  gdi.fillDown();
  gdi.dropLine(0.5);
  gdi.addString("", fontMediumPlus, title).setColor(colorDarkBlue);
  gdi.dropLine(0.5);
}

void AutoMachine::startCancelInterval(gdioutput &gdi, oEvent &oe, const char *startCommand, State state, IntervalType type, const wstring &intervalIn) {
  gdi.pushX();
  gdi.fillRight();

  gdi.addInput("MachineName", machineName, 15, nullptr, L"Automatnamn:", L"Om du vill kan du namnge automaten");

  if (type == IntervalType::IntervalMinute)
    gdi.addInput("Interval", intervalIn, 7, 0, L"Tidsintervall (MM:SS):");
  else if (type == IntervalType::IntervalSecond)
    gdi.addInput("Interval", intervalIn, 7, 0, L"Tidsintervall (sekunder):");
  
  gdi.dropLine(1);
  gdi.addButton(startCommand, 
    (state == State::Create || state == State::Load) ? "Starta automaten" : "OK", AutomaticCB).setExtra(getId());

  if (hasSaveMachine()) {
    if (wasSaved())
      gdi.addButton("SaveMachine", "Uppdatera sparad", AutomaticCB).setExtra(getId());
    else
      gdi.addButton("SaveMachine", "Spara inställningar", AutomaticCB).setExtra(getId());
  }

  if (state == State::Load)
    gdi.addButton("Erase", "Ta bort sparad", AutomaticCB).setExtra(getId());

  if (state == State::Edit)
    gdi.addButton("Cancel", "Avbryt", AutomaticCB).setExtra(getId());
  
  gdi.addButton("Stop", (state == State::Create || state == State::Load) ? "Avbryt" : "Stoppa automaten", AutomaticCB).setExtra(getId());

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

void PrewarningMachine::settings(gdioutput &gdi, oEvent &oe, State state) {
  settingsTitle(gdi, "Förvarningsröst");
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalNone, L"");

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
  oe.fillControls(d, oEvent::ControlType::CourseControl);
  gdi.setItems("Controls", d);
  gdi.setSelection("Controls", controls);
  gdi.popX();
  gdi.addButton("SelectAll", "Välj alla", AutomaticCB, "").setExtra(L"Controls");
  gdi.popX();
}

void PrewarningMachine::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  waveFolder = gdi.getText("WaveFolder");
  gdi.getSelection("Controls", controls);

  oe.synchronizeList(oListId::oLPunchId);
  oe.clearPrewarningSounds();

  controlsSI.clear();
  for (set<int>::iterator it = controls.begin(); it != controls.end(); ++it) {
    pControl pc = oe.getControl(*it, false, false);
    if (pc) {
      vector<int> n;
      pc->getNumbers(n);
      controlsSI.insert(n.begin(), n.end());
    }
  }
  if (doProcess)
    synchronizePunches = true;
}

void PrewarningMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  processProtected(gdi, ast, [&]() {
    oe->playPrewarningSounds(waveFolder, controlsSI);
  });
}

void PrewarningMachine::status(gdioutput &gdi)
{
  AutoMachine::status(gdi);

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

void PunchMachine::settings(gdioutput &gdi, oEvent &oe, State state) {
  settingsTitle(gdi, "Test av stämplingsinläsningar");
  wstring time = state == State::Create ? L"10" : itow(interval);
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalSecond, time);

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
  gdi.addCheckbox("UseRelay", "Med stafettklasser", nullptr, false);
  gdi.addButton("GenerateCMP", "Generera testtävling", AutomaticCB);
}

void PunchMachine::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  wstring minute = gdi.getText("Interval");
  int t = _wtoi(minute.c_str());

  if (t<1 || t>7200) {
    throw meosException(L"Ogiltigt antal sekunder: X#" + minute);
  }
  else {
    interval = t;
    radio = gdi.getTextNo("Radio");
  }
}

void PunchMachine::status(gdioutput &gdi)
{
  AutoMachine::status(gdi);
  gdi.fillRight();
  gdi.pushX();
  if (interval>0){
    gdi.addString("", 0, "Stämplar om: ");
    gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign|timeSeconds, (GetTickCount64()-timeout)/1000);
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

void PunchMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  processProtected(gdi, ast, [&]() {
    SICard sic(ConvertedTimeStatus::Hour24);
    SportIdent& si = TabSI::getSI(gdi);

    if (radio == 0) {
      oe->generateTestCard(sic);
      if (!sic.empty()) {
        if (!radio) si.addCard(sic);
      }
      else gdi.addInfoBox("", L"Failed to generate card.", L"", BoxStyle::Header, interval * 2000);
    }
    else {
      SICard sic(ConvertedTimeStatus::Hour24);
      vector<pRunner> rr;
      oe->getRunners(0, 0, rr);
      vector<pRunner> cCand;
      vector<pFreePunch> pp;
      for (auto r : rr) {
        if (r->getStatus() == StatusUnknown && r->startTimeAvailable()) {
          auto pc = r->getCourse(false);
          if (radio < 10 || pc->hasControlCode(radio)) {
            pp.clear();
            oe->getPunchesForRunner(r->getId(), false, pp);
            bool hit = false;
            for (auto p : pp) {
              if (p->getTypeCode() == radio)
                hit = true;
            }
            if (!hit)
              cCand.push_back(r);
          }
        }
      }
      if (cCand.size() > 0) {
        int ix = rand() % cCand.size();
        pRunner r = cCand[ix];
        sic.convertedTime = ConvertedTimeStatus::Done;
        sic.CardNumber = r->getCardNo();
        sic.punchOnly = true;
        sic.nPunch = 1;
        sic.Punch[0].Code = radio;
        sic.Punch[0].Time = timeConstHour / 10 + rand() % (1200 * timeConstSecond) + r->getStartTime();
        si.addCard(sic);
      }
    }
  });
}

void SplitsMachine::settings(gdioutput &gdi, oEvent &oe, State state) {
  wstring time;
  if (interval>0)
    time = itow(interval);
  else if (state == State::Create)
    time = L"30";

  settingsTitle(gdi, "Export av resultat/sträcktider");
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalSecond, time);

  gdi.addString("",  0, "Intervall (sekunder). Lämna blankt för att uppdatera när "
                                        "tävlingsdata ändras.");
  gdi.dropLine();

  TabCompetition::selectExportSplitOptions(gdi, &oe, classes, &data);

  gdi.dropLine();
  gdi.fillRight();
  gdi.addInput("FileName", file, 30, 0, L"Filnamn:");
  gdi.dropLine(0.9);
  gdi.addButton("BrowseSplits", "Bläddra...", AutomaticCB);

  gdi.popX();
  gdi.dropLine(2);
}

void SplitsMachine::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  wstring ivt = gdi.getText("Interval");

  int iv = gdi.getTextNo("Interval");
  file = gdi.getText("FileName");

  if (!ivt.empty() && (iv < 1 || iv > 7200)) {
    throw meosException(L"Ogiltigt antal sekunder: X#" + gdi.getText("Interval"));
  }

  if (file.empty()) {
    throw meosException("Filnamnet får inte vara tomt");
  }

  TabCompetition::readExportSplitSettings(gdi, &oe, classes, data);

  if (doProcess) {
    checkWriteAccess(file);


    TabCompetition::exportSplitsData(&oe, file, classes, data, false);
    //Try exporting.
    /*oe.exportIOFSplits(oEvent::IOF20, file.c_str(), true, false,
                       set<int>(), make_pair("",""), -1, false, false, true, true, false, false);*/
    interval = iv;
    synchronize = true;
  }
}

void SplitsMachine::status(gdioutput &gdi) {
  AutoMachine::status(gdi);
  if (!file.empty()) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, L"Fil: X#" + file);

    if (interval>0){
      gdi.popX();
      gdi.dropLine(1);
      gdi.addString("", 0, "Skriver sträcktider om: ");
      gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign|timeSeconds, (GetTickCount64()-timeout)/1000);
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

void SplitsMachine::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  processProtected(gdi, ast, [&]() {
    if ((interval > 0 && ast == SyncTimer) || (interval == 0 && ast == SyncDataUp)) {
      if (!file.empty()) {
        TabCompetition::exportSplitsData(oe, file, classes, data, false);
      }
        /*oe->exportIOFSplits(oEvent::IOF20, file.c_str(),
          true, false, classes, make_pair("", ""),
          leg, false, false, true, true, false, false);*/
    }
  });
}

void SaveMachine::status(gdioutput &gdi) {
  AutoMachine::status(gdi);

  if (!baseFile.empty()) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, L"Destination: X#" + baseFile);

    if (interval>0){
      gdi.popX();
      gdi.dropLine(1);
      gdi.addString("", 0, "Säkerhetskopierar om: ");
      gdi.addTimer(gdi.getCY(),  gdi.getCX(), timerIgnoreSign, (GetTickCount64()-timeout)/1000);
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

      processProtected(gdi, ast, [&]() {
        wstring file;
        while (file.empty()) {
          file = baseFile + L"meos_backup_" + oe->getDate() + L"_" + itow(saveIter++) + L".meosxml";
          if (fileExists(file))
            file.clear();
          else {
            if (ast == AutoSyncType::SyncNone)
              checkWriteAccess(file);
          }
        }
        oe->autoSynchronizeLists(true);
        oe->save(file, false);
      });
    }
  }
}

void SaveMachine::settings(gdioutput &gdi, oEvent &oe, State state) {
  settingsTitle(gdi, "Säkerhetskopiering");
  wstring time=state == State::Create ? L"10:00" : formatTimeMS(interval * timeConstSecond, false, SubSecond::Off);
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalMinute, time);

  int cx = gdi.getCX();
  gdi.addInput("BaseFile", baseFile, 32, 0, L"Mapp:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseFolder", "Bläddra...", AutomaticCB).setExtra(L"BaseFile");
  gdi.setCX(cx);
}

void SaveMachine::save(oEvent& oe, gdioutput& gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  wstring minute = gdi.getText("Interval");
  int t = convertAbsoluteTimeMS(minute) / timeConstSecond;

  if (t < 2 || t>7200) {
    throw meosException("Intervallet måste anges på formen MM:SS.");
  }
  wstring f = gdi.getText("BaseFile");
  if (f.empty()) {
    throw meosException("Filnamnet får inte vara tomt");
  }

  if (*f.rbegin() != '\\' && *f.rbegin() != '/')
    f += L"\\";

  if (doProcess) {
    wstring sample = f + L"sample.txt";
    ofstream fout(sample.c_str(), ios_base::trunc | ios_base::out);
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

    interval = t;
  }
  baseFile = f;
}

void SaveMachine::saveMachine(oEvent& oe, const wstring& guiInterval) {
  AutoMachine::saveMachine(oe, guiInterval);
  auto& cnt = oe.getMachineContainer().set(getTypeString(), getMachineName());
  int t = convertAbsoluteTimeMS(guiInterval) / timeConstSecond;
  cnt.set("file", baseFile);
  cnt.set("interval", t);
}

void SaveMachine::loadMachine(oEvent& oe, const wstring& name) {
  auto* cnt = oe.getMachineContainer().get(getTypeString(), name);
  if (!cnt)
    return;

  AutoMachine::loadMachine(oe, name);
  baseFile = cnt->getString("file"); 
  interval = cnt->getInt("interval");
}

void TabAuto::clearCompetitionData() {
}

Machines AutoMachine::getType(const string &typeStr) {
  if (typeStr == "onlineinput")
    return Machines::mOnlineInput;
  else if (typeStr == "onlineresults")
    return Machines::mOnlineResults;
  else if (typeStr == "printresult")
    return Machines::mPrintResultsMachine;
  else if (typeStr == "backup")
    return Machines::mSaveBackup;
  else if (typeStr == "splits")
    return Machines::mSplitsMachine;
  else if (typeStr == "infoserver")
    return Machines::mInfoService;

  return Machines::Unknown;
}

wstring AutoMachine::getDescription(Machines type) {
  switch (type) {
  case mPrintResultsMachine:
    return L"Resultatutskrift / export";
  case mPunchMachine:
    return L"Stämplingstest";
  case mSplitsMachine:
    return L"Export av resultat/sträcktider";
  case mPrewarningMachine:
    return L"Förvarningsröst";
  case mOnlineResults:
    return L"Resultat online";
  case mOnlineInput:
    return L"Inmatning online";
  case mSaveBackup:
    return L"Säkerhetskopiering";
  case mInfoService:
    return L"Informationsserver";
  case mMySQLReconnect:
    return L"MySQL reconnect";
  default:
    return L"???";
  }
}

string AutoMachine::getTypeString(Machines type) {
  switch (type) {
  case mPrintResultsMachine:
    return "printresult";
  case mPunchMachine:
    return "punchtest";
  case mSplitsMachine:
    return "splits";
  case mPrewarningMachine:
    return "prewarning";
  case mOnlineResults:
    return "onlineresults";
  case mOnlineInput:
    return "onlineinput";
  case mSaveBackup:
    return "backup";
  case mInfoService:
    return "infoserver";
  case mMySQLReconnect:
    return "reconnect";
  default:
    return "???";
  }
}
