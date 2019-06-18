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
#include "meos_util.h"
#include "TabCompetition.h"
#include "TabCourse.h"
#include "oFreeImport.h"
#include "localizer.h"
#include "oListInfo.h"
#include "download.h"
#include "progress.h"
#include "classconfiginfo.h"
#include "RunnerDB.h"
#include "gdifonts.h"
#include "meosException.h"
#include "meosdb/sqltypes.h"
#include "socket.h"
#include "iof30interface.h"
#include "MeOSFeatures.h"
#include "prefseditor.h"
#include "recorder.h"
#include "testmeos.h"
#include "importformats.h"
#include "HTMLWriter.h"
#include "metalist.h"

#include <Shellapi.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <io.h>

void Setup(bool overwrite, bool overWriteall);
void exportSetup();
void resetSaveTimer();
extern bool enableTests;

int ListsCB(gdioutput *gdi, int type, void *data);

TabCompetition::TabCompetition(oEvent *poe):TabBase(poe)
{
  eventorBase = poe->getPropertyString("EventorBase", L"https://eventor.orientering.se/api/");
  iofExportVersion = L"&version=3.0";
  defaultServer=L"localhost";
  defaultName=L"meos";
  organizorId = 0;
  lastSelectedClass = -1;
  allTransfer.insert(-1);
  lastChangeClassType = oEvent::ChangeClassVacant;
}

TabCompetition::~TabCompetition(void)
{
}

extern SportIdent *gSI;
extern HINSTANCE hInst;
extern HWND hWndMain;

bool TabCompetition::save(gdioutput &gdi, bool write)
{
  wstring name=gdi.getText("Name");

  if (name.empty()) {
    gdi.alert("Tävlingen måste ha ett namn");
    return 0;
  }

  wstring zt = gdi.getText("ZeroTime");
  bool longTimes = gdi.isChecked("LongTimes");
  wstring date = gdi.getText("Date");

  if (!checkValidDate(date))
    throw meosException(L"Felaktigt datum 'X' (Använd YYYY-MM-DD)#" + date);

  if (longTimes)
    zt = L"00:00:00";

  int newZT = convertAbsoluteTimeHMS(zt, -1);
  if (newZT < 0)
    throw meosException(L"Felaktigt tidsformat 'X' (Använd TT:MM:SS)#" + zt);

  int oldZT = convertAbsoluteTimeHMS(oe->getZeroTime(), -1);
  bool oldLT = oe->useLongTimes();
  wstring oldDate = oe->getDate();
  
  if ((newZT != oldZT ||
    longTimes != oldLT ||
    (longTimes && date != oldDate)) && oe->classHasResults(0)) {
    if (!gdi.ask(L"warn:changedtimezero")) {
      gdi.setText("ZeroTime", oe->getZeroTime());
      gdi.check("LongTimes", oe->useLongTimes());
      gdi.setText("Date", oe->getDate());
      return 0;
    }
}
  bool updateTimes = newZT != oldZT && oe->getNumRunners() > 0 && gdi.ask(L"ask:updatetimes");

  if (updateTimes) {
    int delta = oldZT - newZT;
    oe->updateStartTimes(delta);
  }
  
  oe->setDate(date);
  oe->useLongTimes(longTimes);
  oe->setName(gdi.getText("Name"));
  oe->setAnnotation(gdi.getText("Annotation"));
  oe->setZeroTime(zt);

  oe->synchronize();

  gdi.setWindowTitle(oe->getTitleName());
  gdi.setText("Date", oe->getDate());
  gdi.setText("ZeroTime", oe->getZeroTime());

  if (write) {
    gdi.setWaitCursor(true);
    resetSaveTimer();
    return oe->save();
  }
  else
    return true;
}

bool TabCompetition::importFile(HWND hWnd, gdioutput &gdi)
{
  vector< pair<wstring, wstring> > ext;
  ext.push_back(make_pair(L"xml-data", L"*.xml;*.bu?"));
  wstring fileName = gdi.browseForOpen(ext, L"xml");
  if (fileName.empty())
    return false;

  gdi.setWaitCursor(true);
  if (oe->open(fileName, true)) {
    gdi.setWindowTitle(oe->getTitleName());
    resetSaveTimer();
    return true;
  }

  return false;
}

bool TabCompetition::exportFileAs(HWND hWnd, gdioutput &gdi)
{
  int ix = 0;
  vector< pair<wstring, wstring> > ext;
  ext.push_back(make_pair(L"xml-data", L"*.xml"));
  wstring fileName = gdi.browseForSave(ext, L"xml", ix);
  if (fileName.empty())
    return false;

  gdi.setWaitCursor(true);
  if (!oe->save(fileName.c_str())) {
    gdi.alert(L"Fel: Filen " + fileName+ L" kunde inte skrivas.");
    return false;
  }

  return true;
}

int CompetitionCB(gdioutput *gdi, int type, void *data)
{
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));

  return tc.competitionCB(*gdi, type, data);
}


int restoreCB(gdioutput *gdi, int type, void *data)
{
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));

  return tc.restoreCB(*gdi, type, data);
}

void TabCompetition::loadConnectionPage(gdioutput &gdi)
{
  gdi.clearPage(false);
  showConnectionPage=true;
  gdi.addString("", boldLarge, "Anslutningar");

  if (oe->getServerName().empty()) {
    gdi.addString("", 10, "help:52726");
    gdi.pushX();
    gdi.dropLine();
    defaultServer = oe->getPropertyString("Server", defaultServer);
    defaultName = oe->getPropertyString("UserName", defaultName);
    defaultPort = oe->getPropertyString("Port", defaultPort);
    wstring client = oe->getPropertyString("Client", oe->getClientName());

    gdi.fillRight();
    gdi.addInput("Server", defaultServer, 16, 0, L"MySQL Server / IP-adress:", L"IP-adress eller namn på en MySQL-server");
    gdi.addInput("UserName", defaultName, 7, 0, L"Användarnamn:");
    gdi.addInput("PassWord", defaultPwd, 9, 0, L"Lösenord:").setPassword(true);
    gdi.addInput("Port", defaultPort, 4, 0, L"Port:");

    if (defaultServer.empty())
      gdi.setInputFocus("Server");
    else if (defaultName.empty())
      gdi.setInputFocus("UserName");
    else
      gdi.setInputFocus("PassWord");

    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(2.5);
    gdi.addInput("ClientName", client, 16, 0, L"Klientnamn:");
    gdi.dropLine();
    gdi.fillRight();
    gdi.addButton("ConnectToMySQL", "Anslut", CompetitionCB).setDefault();
  }
  else {
    gdi.addString("", 10, "help:50431");
    gdi.dropLine(1);
    gdi.pushX();
    gdi.fillRight();
    gdi.addString("", 1, "Ansluten till:");
    gdi.addStringUT(1, oe->getServerName()).setColor(colorGreen);
    gdi.popX();
    gdi.dropLine(2);
    gdi.addInput("ClientName", oe->getClientName(), 16, 0, L"Klientnamn:");
    gdi.dropLine();
    gdi.addButton("SaveClient", "Ändra", CompetitionCB);
    gdi.dropLine(2.5);

    gdi.popX();
    gdi.addString("", 1, "Öppnad tävling:");

    if (oe->empty())
      gdi.addString("", 1, "Ingen").setColor(colorRed);
    else {
      gdi.addStringUT(1, oe->getName()).setColor(colorGreen);

      if (oe->isClient())
        gdi.addString("", 1, "(på server)");
      else
        gdi.addString("", 1, "(lokalt)");

    }
    gdi.dropLine(2);
    gdi.popX();
    gdi.fillRight();

    if (!oe->isClient())
      gdi.addButton("UploadCmp", "Ladda upp öppnad tävling på server",CompetitionCB);

    if (oe->empty()) {
      gdi.disableInput("UploadCmp");
    }
    else {
      gdi.addButton("CloseCmp", "Stäng tävlingen", CompetitionCB);
      gdi.addButton("Delete", "Radera tävlingen", CompetitionCB);
    }
    gdi.dropLine(2);
    gdi.popX();
    if (oe->empty()) {
      wchar_t bf[260];
      getUserFile(bf, L"");
      oe->enumerateCompetitions(bf, L"*.meos");

      gdi.dropLine(1);
      gdi.fillRight();
      gdi.addListBox("ServerCmp", 320, 210,  CompetitionCB, L"Server");
      oe->fillCompetitions(gdi, "ServerCmp", 2);
      gdi.selectItemByData("ServerCmp", oe->getPropertyInt("LastCompetition", 0));

      gdi.fillDown();
      gdi.addListBox("LocalCmp", 320, 210, CompetitionCB, L"Lokalt");
      gdi.popX();
      oe->fillCompetitions(gdi, "LocalCmp", 1);
      gdi.selectItemByData("LocalCmp", oe->getPropertyInt("LastCompetition", 0));

      gdi.addCheckbox("UseDirectSocket", "Skicka och ta emot snabb förhandsinformation om stämplingar och resultat",
                      0, oe->getPropertyInt("UseDirectSocket", true) != 0);

      gdi.dropLine();
      gdi.fillRight();
      gdi.addButton("OpenCmp", "Öppna tävling", CompetitionCB).setDefault();
      gdi.addButton("Repair", "Reparera vald tävling", CompetitionCB);

      gdi.setInputStatus("Repair", gdi.getSelectedItem("ServerCmp").second, true);
    }
    else if (oe->isClient()) {
      gdi.fillDown();
      gdi.popX();
      oe->listConnectedClients(gdi);
      gdi.registerEvent("Connections", CompetitionCB);
      gdi.fillRight();
    }

    gdi.addButton("DisconnectMySQL", "Koppla ner databas", CompetitionCB);
  }
  gdi.addButton("Cancel", "Till huvudsidan", CompetitionCB).setCancel();
  gdi.fillDown();
  gdi.refresh();
}

bool TabCompetition::checkEventor(gdioutput &gdi, ButtonInfo &bi) {
  eventorOrigin = bi.id;

  if (organizorId == 0) {
    int clubId = getOrganizer(true);
    if (clubId == 0) {
      bi.id = "EventorAPI";
      competitionCB(gdi, GUI_BUTTON, &bi);
      return true;
    }
    else if (clubId == -1)
      throw std::exception("Kunde inte ansluta till Eventor");

    organizorId = clubId;
  }
  return false;
}

int eventorServer(gdioutput *gdi, int type, void *data) {
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));
  if (type == GUI_COMBO) {
    const ListBoxInfo &lbi = *((ListBoxInfo *)data);
    tc.setEventorServer(lbi.text);
  }
  else if (type == GUI_BUTTON) {
    const ButtonInfo &bi = *((ButtonInfo *)data);

    if (bi.id == "EventorUTC")
      tc.setEventorUTC(gdi->isChecked(bi.id));
  }
  return 0;
}

void TabCompetition::setEventorServer(const wstring &server) {
  eventorBase = server;
  oe->setProperty("EventorBase", server);
}

void TabCompetition::setEventorUTC(bool useUTC) {
  oe->setProperty("UseEventorUTC", useUTC);
}

bool TabCompetition::useEventorUTC() const {
  bool eventorUTC = oe->getPropertyInt("UseEventorUTC", 0) != 0;
  return eventorUTC;
}

enum StartMethod {SMCommon = 1, SMDrawn, SMFree, SMCustom};

int TabCompetition::competitionCB(gdioutput &gdi, int type, void *data)
{
  if (type == GUI_LINK) {
    TextInfo ti = *(TextInfo *)data;
    if (ti.id == "link") {
      wstring url = ti.text;
      ShellExecute(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    else if (ti.id == "fnpath") {
      ShellExecute(NULL, L"open", ti.text.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
  }
  else if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id == "CopyLink") {
      string url = gdi.narrow(gdi.getText("link"));

      if (OpenClipboard(gdi.getHWNDMain())) {
        EmptyClipboard();
        HGLOBAL hClipboardData;
        hClipboardData = GlobalAlloc(GMEM_DDESHARE,
                                     url.length()+1);

        char * pchData;
        pchData = (char*)GlobalLock(hClipboardData);

        strcpy_s(pchData, url.length()+1, LPCSTR(url.c_str()));

        GlobalUnlock(hClipboardData);

        SetClipboardData(CF_TEXT,hClipboardData);
        CloseClipboard();
      }
    }
    else if (bi.id == "LongTimes") {
      if (gdi.isChecked(bi.id)) {
        gdi.setTextTranslate("ZeroTimeHelp", L"help:long_times", true);
        gdi.disableInput("ZeroTime");
      }
      else {
        gdi.setTextTranslate("ZeroTimeHelp", L"help:zero_time", true);
        gdi.enableInput("ZeroTime");
      }
    }
    else if (bi.id=="Print") {
      gdi.print(oe);
    }
    else if (bi.id=="Setup") {
      gdi.clearPage(false);

      gdi.addString("", boldLarge, "Inställningar MeOS");
      gdi.dropLine();
      gdi.addString("", 10, "help:29191");
      gdi.dropLine();
      wchar_t FileNamePath[260];
      getUserFile(FileNamePath, L"");
      gdi.addStringUT(0, lang.tl(L"MeOS lokala datakatalog är: ") + FileNamePath);
      gdi.dropLine();

      gdi.addCombo("EventorServer", 320, 100, eventorServer, L"Eventor server:");

      vector<wstring> eventorCand;
      eventorCand.push_back(L"https://eventor.orientering.se/api/");

      gdi.addItem("EventorServer", eventorBase);
      for (size_t k = 0; k < eventorCand.size(); k++) {
        if (eventorBase != eventorCand[k])
          gdi.addItem("EventorServer", eventorCand[k]);
      }

      gdi.selectFirstItem("EventorServer");

      bool eventorUTC = oe->getPropertyInt("UseEventorUTC", 0) != 0;
      gdi.addCheckbox("EventorUTC", "Eventors tider i UTC (koordinerad universell tid)", eventorServer, eventorUTC);

      wchar_t bf[260];
      GetCurrentDirectory(260, bf);
      gdi.fillRight();
      gdi.pushX();
      gdi.dropLine();
      gdi.addInput("Source", bf, 40, 0, L"Källkatalog:");
      gdi.dropLine(0.8);
      gdi.addButton("SourceBrowse", "Bläddra...", CompetitionCB);
      gdi.dropLine(4);
      gdi.popX();
      gdi.fillRight();
      gdi.addButton("DoSetup", "Installera", CompetitionCB);
      gdi.addButton("ExportSetup", "Exportera", CompetitionCB);
      gdi.addButton("Cancel", "Stäng", CompetitionCB);
      gdi.dropLine(2);
      gdi.refresh();
    }
    else if (bi.id=="SourceBrowse") {
      wstring s = gdi.browseForFolder(gdi.getText("Source"), 0);
      if (!s.empty())
        gdi.setText("Source", s);
    }
    else if (bi.id=="DoSetup") {
      wstring source = gdi.getText("Source");
      if (SetCurrentDirectory(source.c_str())) {
        Setup(true, true);
        gdi.alert("Tillgängliga filer installerades. Starta om MeOS.");
        exit(0);
      }
      else
        throw std::exception("Operationen misslyckades");
    }
    else if (bi.id == "RunnerDatabase") {
      loadRunnerDB(gdi, 0, false);
      return 0;
    }
    else if (bi.id=="ExportSetup") {

      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Exportera inställningar och löpardatabaser");
      gdi.dropLine();
      gdi.addString("", 10, "help:15491");
      gdi.dropLine();
      wchar_t FileNamePath[260];
      getUserFile(FileNamePath, L"");
      gdi.addStringUT(0, lang.tl(L"MeOS lokala datakatalog är: ") + FileNamePath);

      gdi.dropLine();

      wchar_t bf[260];
      GetCurrentDirectory(260, bf);
      gdi.fillRight();
      gdi.pushX();
      gdi.dropLine();
      gdi.addInput("Source", bf, 40, 0, L"Destinationskatalog:");
      gdi.dropLine(0.8);
      gdi.addButton("SourceBrowse", "Bläddra...", CompetitionCB);
      gdi.dropLine(4);
      gdi.popX();
      gdi.fillRight();
      gdi.addButton("DoExportSetup", "Exportera", CompetitionCB).setDefault();
      gdi.addButton("CancelRunnerDatabase", "Avbryt", CompetitionCB).setCancel();
      gdi.dropLine(2);
      gdi.refresh();
    }
    else if (bi.id=="DoExportSetup") {
      wstring source = gdi.getText("Source");
      if (SetCurrentDirectory(source.c_str())) {
        exportSetup();
        gdi.alert("Inställningarna har exporterats.");
      }
    }
    else if (bi.id == "SaveTest") {
      vector< pair<wstring, wstring> > cpp;
      cpp.push_back(make_pair(L"Source", L"*.cpp"));
      int ix = 0;
      wstring fn = gdi.browseForSave(cpp, L".cpp", ix);
      if (!fn.empty())
        gdi.getRecorder().saveRecordings(gdi.narrow(fn));
    }
    else if (bi.id == "RunTest") {
      TestMeOS tm(oe, "base");
      tm.runAll();
      oe->clear();
      gdi.selectTab(tabId);
      tm.publish(gdi);
      gdi.addButton("Cancel", "Återgå", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id == "RunSpecificTest") {
      int test = gdi.getSelectedItem("Tests").first;
      TestMeOS tm(oe, "base");
      tm.runSpecific(test);
    }
    else if (bi.id == "LocalSettings") {
      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Ändra MeOS lokala systemegenskaper");
      gdi.dropLine(0.5);
      gdi.addString("", 0, "Vissa inställningar kräver omstart av MeOS för att ha effekt.");
      gdi.dropLine(0.5);
      gdi.addButton("Cancel", "Återgå", CompetitionCB);
      gdi.dropLine();

      if (prefsEditor.empty())
        prefsEditor.push_back(PrefsEditor(oe));

      prefsEditor.back().showPrefs(gdi);
      gdi.refresh();
    }
    else if (bi.id=="Test") {
      checkRentCards(gdi);
    }
    else if (bi.id=="Report") {
      gdi.clearPage(true);
      oe->generateCompetitionReport(gdi);

      gdi.addButton(gdi.getWidth()+20, 15, gdi.scaleLength(120), "Cancel",
                    "Återgå", CompetitionCB,  "", true, false);
      gdi.addButton(gdi.getWidth()+20, 18+gdi.getButtonHeight(), gdi.scaleLength(120), "Print",
                    "Skriv ut...", CompetitionCB,  "Skriv ut rapporten", true, false);
      gdi.refresh();

      //gdi.addButton("Cancel", "Avbryt", CompetitionCB);
    }
    else if (bi.id=="Features") {
      save(gdi, false);
      meosFeatures(gdi, false);
    }
    else if (bi.id == "SaveFeaures") {
      saveMeosFeatures(gdi, true);
      loadPage(gdi);
    }
    else if (bi.id=="Settings") {
      loadSettings(gdi);
    }
    else if (bi.id == "UseFraction") {
      gdi.setInputStatus("CurrencySeparator_odc", gdi.isChecked(bi.id));
    }
    else if (bi.id == "AddPayMode") {
      saveSettings(gdi);
      vector< pair<wstring, size_t> > modes;
      oe->getPayModes(modes);
      oe->setPayMode(modes.size(), lang.tl(L"Betalsätt"));
      loadSettings(gdi);
    }
    else if (bi.id == "RemovePayMode") {
      saveSettings(gdi);
      oe->setPayMode(bi.getExtraInt(), L"");
      loadSettings(gdi);
    }
    else if (bi.id=="SaveSettings") {
      saveSettings(gdi);
      loadPage(gdi);
    }
    else if (bi.id == "Exit") {
      PostMessage(gdi.getHWNDMain(), WM_CLOSE, 0, 0);
    }
    else if (bi.id == "Help") {
      wchar_t fn[MAX_PATH];
      getMeOSFile(fn, lang.tl(L"documentation").c_str());
      if (_waccess(fn, 0)==-1) {
        gdi.alert(wstring(L"Hittar inte hjälpfilen, X#") + fn);
        return 0;
      }

      gdi.openDoc(fn);
    }
    else if (bi.id=="Browse") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(lang.tl(L"Databaskälla"), L"*.xml;*.csv"));
      
      wstring f = gdi.browseForOpen(ext, L"xml");
      string id;
      if (!f.empty()) {
        InputInfo &ii = dynamic_cast<InputInfo &>(gdi.getBaseInfo(bi.getExtra()));
        gdi.setText(ii.id, f);
      }
    }
    else if (bi.id=="DBaseIn") {
      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Importera löpare och klubbar / distriktsregister");
      gdi.dropLine();
      gdi.addString("", 10, "help:runnerdatabase");
      gdi.dropLine(2);
      gdi.pushX();
      gdi.fillRight();
      gdi.addInput("ClubFile", L"", 40, 0, L"Filnamn IOF (xml) med klubbar");
      gdi.dropLine();
      gdi.addButton("Browse", "Bläddra...", CompetitionCB).setExtra(L"ClubFile");
      gdi.popX();
      gdi.dropLine(3);
      gdi.addInput("CmpFile", L"", 40, 0, L"Filnamn IOF (xml) eller OE (csv) med löpare");
      gdi.dropLine();
      gdi.addButton("Browse", "Bläddra...", CompetitionCB).setExtra(L"CmpFile");
      gdi.popX();

      gdi.dropLine(3);
      gdi.popX();
      gdi.addCheckbox("Clear", "Nollställ databaser", 0, true);
      gdi.dropLine(3);

      gdi.popX();
      gdi.addButton("DoDBaseIn", "Importera", CompetitionCB).setDefault();
      gdi.addButton("CancelRunnerDatabase", "Avbryt", CompetitionCB).setCancel();
      gdi.dropLine(3);
      gdi.fillDown();
      gdi.popX();
    }
    else if (bi.id=="DoDBaseIn") {
      gdi.enableEditControls(false);
      gdi.disableInput("DoDBaseIn");
      gdi.disableInput("CancelRunnerDatabase");

      gdi.setWaitCursor(true);
      gdi.addString("", 0, "Importerar...");
      bool clear = gdi.isChecked("Clear");
      wstring club = gdi.getText("ClubFile");
      wstring cmp = gdi.getText("CmpFile");
      if (club == cmp)
        club = L"";

      bool clubCsv = !club.empty() && csvparser::iscsv(club) != csvparser::CSV::NoCSV;
      bool cmpCsv = !cmp.empty() && csvparser::iscsv(cmp) != csvparser::CSV::NoCSV;
      
      if (cmpCsv) {
        if (!club.empty())
          throw meosException("Klubbfil får inte anges vid CSV import.");

        oe->importOECSV_Data(cmp, clear);
      }
      else {
       if (clubCsv)
          throw meosException("Klubbfil får inte anges vid CSV import.");

        oe->importXML_IOF_Data(club, cmp, clear);
      }
      
      gdi.dropLine();
      gdi.addButton("CancelRunnerDatabase", "Återgå", CompetitionCB);
      gdi.refresh();
      gdi.setWaitCursor(false);
    }
    else if (bi.id=="Reset") {
      if (gdi.ask(L"Vill då återställa inställningar och skriva över egna databaser?"))
        Setup(true, true);
    }
    else if (bi.id=="ConnectMySQL")
      loadConnectionPage(gdi);
    else if (bi.id=="SaveClient") {
      oe->setClientName(gdi.getText("ClientName"));
      if (gdi.getText("ClientName").length()>0)
          oe->setProperty("Client", gdi.getText("ClientName"));
    }
    else if (bi.id=="ConnectToMySQL") {
      bool s=oe->connectToMySQL(gdi.narrow(gdi.getText("Server")),
                                gdi.narrow(gdi.getText("UserName")),
                                gdi.narrow(gdi.getText("PassWord")),
                                gdi.getTextNo("Port"));

      if (s) {
        defaultServer=gdi.getText("Server");
        defaultName=gdi.getText("UserName");
        defaultPwd=gdi.getText("PassWord");
        defaultPort=gdi.getText("Port");

        oe->setClientName(gdi.getText("ClientName"));
        oe->setProperty("Server", defaultServer);
        oe->setProperty("UserName", defaultName);
        oe->setProperty("Port", defaultPort);
        if (gdi.getText("ClientName").length()>0)
          oe->setProperty("Client", gdi.getText("ClientName"));


        loadConnectionPage(gdi);
      }
    }
    else if (bi.id == "Repair") {
      if (!gdi.ask(L"ask:repair"))
        return 0;
      ListBoxInfo lbi;
      int id=0;
      if ( gdi.getSelectedItem("ServerCmp", lbi) )
        id=lbi.data;
      else
        throw meosException("Ingen tävling vald.");

      wstring nameId = oe->getNameId(id);
      vector<string> output;
      repairTables(gdi.narrow(nameId), output);
      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Reparerar tävlingsdatabasen");
      gdi.dropLine();
      for (size_t k = 0; k < output.size(); k++) {
        gdi.addStringUT(0, output[k]);
      }
      gdi.dropLine();
      gdi.addButton("Cancel", "Klart", CompetitionCB);

    }
    else if (bi.id=="DisconnectMySQL") {
      oe->closeDBConnection();
      loadConnectionPage(gdi);
    }
    else if (bi.id=="UploadCmp") {
      if (oe->uploadSynchronize())
        gdi.setWindowTitle(oe->getTitleName());

      if (oe->isClient() && oe->getPropertyInt("UseDirectSocket", true) != 0) {
        oe->getDirectSocket().startUDPSocketThread(gdi.getHWNDMain());
      }

      loadConnectionPage(gdi);
    }
    else if (bi.id == "MultiEvent") {
      loadMultiEvent(gdi);
    }
    else if (bi.id == "CloneEvent") {
      wstring ne = oe->cloneCompetition(true, false, false, false, false);
      oe->updateTabs(true);
    }
    else if (bi.id == "CloneCmp") {
      gdi.restore("MultiHeader");
      gdi.dropLine(3);
      gdi.fillDown();
      gdi.addString("", 1, "Skapar ny etapp").setColor(colorGreen);
      gdi.addString("", 0, "Överför anmälda");
      gdi.refreshFast();
      wstring ne = oe->cloneCompetition(true, false, false, false, true);

      gdi.addString("", 0, "Klart");
      gdi.dropLine();

      wchar_t bf[260];
      getUserFile(bf, L"");
      oe->enumerateCompetitions(bf, L"*.meos");
      oe->updateTabs(true);
      gdi.addButton("MultiEvent", "Återgå", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id == "SaveMulti") {
      saveMultiEvent(gdi);
    }
    else if (bi.id == "OpenPost" || bi.id == "OpenPre") {
      
      saveMultiEvent(gdi);

      wstring nameId = oe->getNameId(0);
      ListBoxInfo lbi;
      bool openPost = false;
      
      int theNumber = oe->getStageNumber();

      if (bi.id == "OpenPost") {
        gdi.getSelectedItem("PostEvent", lbi);
        openPost = true;
        if (theNumber == 0) {
          oe->setStageNumber(1);
          theNumber = 1;
        }
        theNumber++;
      }
      else {
        gdi.getSelectedItem("PreEvent", lbi);
        if (theNumber == 0) {
          oe->setStageNumber(2);
          theNumber = 2;
        }
        theNumber--;
      }

      int id = lbi.data;

      if (id>0) {
        oe->save();
        openCompetition(gdi, id);
        oe->getMeOSFeatures().useFeature(MeOSFeatures::SeveralStages, true, *oe);
        if (openPost) {
          oe->getDI().setString("PreEvent", nameId);
          if (theNumber > 1) {
            oe->setStageNumber(theNumber);
          }
        }
        else {
          oe->getDI().setString("PostEvent", nameId);
          if (theNumber >= 0) {
            oe->setStageNumber(theNumber);
          }
        }
        loadMultiEvent(gdi);
      }
    }
    else if (bi.id == "TransferData") {
      saveMultiEvent(gdi);

      ListBoxInfo lbi;
      gdi.getSelectedItem("PostEvent", lbi);
      if (int(lbi.data) == -2)
        throw std::exception("Nästa etapp är odefinierad.");

      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Överför resultat till nästa etapp");
      gdi.setData("PostEvent", lbi.data);
      gdi.dropLine();
      selectTransferClasses(gdi, false);
    }
    else if (bi.id == "SelectAll" || bi.id=="SelectNone") {
      set<int> s;
      if (bi.id=="SelectAll")
        s.insert(-1);
      gdi.setSelection("ClassNewEntries", s);
    }
    else if (bi.id == "ExpandTResults") {
      selectTransferClasses(gdi, true);
    }
    else if (bi.id == "DoTransferData") {
      bool transferNoCompet = true;
      gdi.disableInput("DoTransferData");
      gdi.disableInput("MultiEvent");
      gdi.disableInput("ExpandTResults", true);
      gdi.disableInput("SelectAll", true);
      gdi.disableInput("SelectNone", true);
      if (gdi.hasField("ClassNewEntries")) {
        gdi.getSelection("ClassNewEntries", allTransfer);
        transferNoCompet = gdi.isChecked("TransferEconomy");
      }
      else {
        //oe->getAllClasses(allTransfer);
        allTransfer.clear();
        transferNoCompet = false;
      }
      int id = (int)gdi.getData("PostEvent");
      oEvent::ChangedClassMethod method = oEvent::ChangedClassMethod(gdi.getSelectedItem("ChangeClassType").first);
      lastChangeClassType = method;

      wstring file = oe->getFileNameFromId(id);

      bool success = false;
      oEvent nextStage(gdi);

      if (!file.empty())
        success = nextStage.open(file.c_str(), false);

      if (success)
        success = nextStage.getNameId(0) == oe->getDCI().getString("PostEvent");

      if (success) {
        gdi.enableEditControls(false);
        gdi.dropLine(3);
        gdi.fillDown();
        gdi.addString("", 1, L"Överför resultat till X#" + nextStage.getName());
        gdi.refreshFast();

        vector<pRunner> changedClass, changedClassNoResult, assignedVacant,newEntries,notTransfered, failedTarget;

        oe->transferResult(nextStage, allTransfer,  method, transferNoCompet,
                           changedClass, changedClassNoResult, assignedVacant, 
                           newEntries, notTransfered, failedTarget);
        bool fixedProblem = false;

        if (!changedClass.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare har bytt klass:");
          displayRunners(gdi, changedClass);
        }

        if (!changedClassNoResult.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare har bytt klass (inget totalresultat):");
          displayRunners(gdi, changedClassNoResult);
        }


        if (!assignedVacant.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare har tilldelats en vakant plats:");
          displayRunners(gdi, assignedVacant);
        }

        if (!newEntries.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare är nyanmälda:");
          displayRunners(gdi, newEntries);
        }

        if (!notTransfered.empty() && transferNoCompet) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare deltar ej:");
          displayRunners(gdi, notTransfered);
        }
        else if (!notTransfered.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare överfördes ej:");
          displayRunners(gdi, notTransfered);
        }

        if (!failedTarget.empty()) {
          fixedProblem = true;
          gdi.dropLine();
          gdi.addString("", 1, "Följande deltagare är anmälda till nästa etapp men inte denna:");
          displayRunners(gdi, failedTarget);
        }

        vector<pTeam> newEntriesT, notTransferedT, failedTargetT;
        oe->transferResult(nextStage, method, newEntriesT, notTransferedT, failedTargetT);
        nextStage.transferListsAndSave(*oe);
        oe->updateTabs(true);
        gdi.dropLine();

        if (!fixedProblem) {
          gdi.addString("", 1, "Samtliga deltagare tilldelades resultat.").setColor(colorGreen);
        }
        else {
          gdi.addString("", 1, "Klart.").setColor(colorGreen);
        }

        gdi.dropLine();
        gdi.fillRight();
        gdi.addButton("MultiEvent", "Återgå", CompetitionCB);
        gdi.scrollToBottom();
        gdi.refresh();

      }
      else
        throw std::exception("Kunde inte lokalisera nästa etapp");
    }
    else if (bi.id == "UseEventor") {
      if (gdi.isChecked("UseEventor"))
        oe->setProperty("UseEventor", 1);
      else
        oe->setProperty("UseEventor", 2);
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TCmpTab, 0);
    }
    else if (bi.id == "EventorAPI") {
      assert(!eventorOrigin.empty());
      //DWORD d;
      //if (gdi.getData("ClearPage", d))
      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Nyckel för Eventor");
      gdi.dropLine();
      gdi.addString("", 10, "help:eventorkey");
      gdi.dropLine();
      gdi.addInput("apikey", L"", 40, 0, L"API-nyckel:");
      gdi.dropLine();
      gdi.fillRight();
      gdi.pushX();
      gdi.setRestorePoint("APIKey");
      gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();
      gdi.addButton("EventorAPISave", "Spara", CompetitionCB).setDefault();
      gdi.dropLine(3);
      gdi.popX();
    }
    else if (bi.id == "EventorAPISave") {
      wstring key = gdi.getText("apikey");
      oe->setPropertyEncrypt("apikey", gdi.narrow(key));

      int clubId = getOrganizer(false);

      if (clubId > 0) {
        gdi.restore("APIKey", false);
        gdi.fillDown();
        gdi.addString("", 1, "Godkänd API-nyckel").setColor(colorGreen);
        gdi.addString("", 0, L"Klubb: X#" + eventor.name);
        gdi.addStringUT(0, eventor.city);
        gdi.dropLine();
        gdi.addButton("APIKeyOK", "Fortsätt", CompetitionCB);
        gdi.refresh();
      }
      else {
        gdi.fillDown();
        gdi.dropLine();
        oe->setPropertyEncrypt("apikey", "");
        organizorId = 0;
        gdi.addString("", boldText, "Felaktig nyckel").setColor(colorRed);
        gdi.refresh();
      }
    }
    else if (bi.id == "APIKeyOK") {
      oe->setProperty("Organizer", eventor.name);
      wstring adr  = eventor.careOf.empty() ? eventor.street :
                      eventor.careOf + L", " + eventor.street;
      oe->setProperty("Street", adr);
      oe->setProperty("Address", eventor.zipCode + L" " + eventor.city);
      if (eventor.account.size() > 0)
        oe->setProperty("Account", eventor.account);
      if (eventor.email.size() > 0)
        oe->setProperty("EMail", eventor.email);
      assert(!eventorOrigin.empty());
      bi.id = eventorOrigin;
      eventorOrigin.clear();
      return competitionCB(gdi, type, &bi);
    }
    else if (bi.id == "EventorUpdateDB") {
      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Uppdatera löpardatabasen");
      gdi.setData("UpdateDB", 1);
      bi.id = "EventorImport";
      return competitionCB(gdi, type, &bi);
    }
    else if (bi.id == "SynchEventor") {
      if (checkEventor(gdi, bi))
        return 0;

      gdi.clearPage(true);
      //gdi.setData("EventorId", (int)oe->getExtIdentifier());
      //gdi.setData("UpdateDB", 1);
      gdi.addString("", boldLarge, "Utbyt tävlingsdata med Eventor");
      gdi.dropLine();

      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);

      gdi.fillRight();
      gdi.addButton("EventorEntries", "Hämta efteranmälningar", CompetitionCB);
      gdi.addButton("EventorUpdateDB", "Uppdatera löpardatabasen", CompetitionCB);
      gdi.addButton("EventorStartlist", "Publicera startlista", CompetitionCB, "Publicera startlistan på Eventor");

      if (!cnf.hasStartTimes())
        gdi.disableInput("EventorStartlist");

      gdi.addButton("EventorResult", "Publicera resultat", CompetitionCB, "Publicera resultat och sträcktider på Eventor och WinSplits online");

      if (!cnf.hasResults())
        gdi.disableInput("EventorResult");

      gdi.addButton("Cancel", "Avbryt", CompetitionCB);
      gdi.popX();
      gdi.dropLine(2);
      bi.id = "EventorImport";
      //competitionCB(gdi, type, &bi);
    }
    else if (bi.id == "EventorEntries") {
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      if (cnf.hasResults()) {
        if (!gdi.ask(L"Tävlingen har redan resultat. Vill du verkligen hämta anmälningar?"))
          return 0;
      }
      gdi.enableEditControls(false);
      gdi.enableInput("Cancel");
      gdi.dropLine(2);
      gdi.setData("EventorId", (int)oe->getExtIdentifier());
      gdi.setData("UpdateDB", DWORD(0));
      bi.id = "EventorImport";
      competitionCB(gdi, type, &bi);
    }
    else if (bi.id == "EventorStartlist") {
      gdi.clearPage(true);
      gdi.fillDown();
      gdi.dropLine();
      gdi.addString("", boldLarge, "Publicerar startlistan");

      gdi.dropLine();
      gdi.fillDown();
      gdi.addString("", 1, "Ansluter till Internet").setColor(colorGreen);

      gdi.refreshFast();
      Download dwl;
      dwl.initInternet();

      wstring startlist = getTempFile();
      bool eventorUTC = oe->getPropertyInt("UseEventorUTC", 0) != 0;
      oe->exportIOFStartlist(oEvent::IOF30, startlist.c_str(), eventorUTC, 
                             set<int>(), false, false, true);
      vector<wstring> fileList;
      fileList.push_back(startlist);

      wstring zipped = getTempFile();
      zip(zipped.c_str(), 0, fileList);
      ProgressWindow pw(gdi.getHWNDTarget());
      pw.init();
      vector<pair<wstring,wstring> > key;
      getAPIKey(key);

      wstring result = getTempFile();
      wstring error;
      try {
        dwl.postFile(eventorBase + L"import/startlist", zipped, result, key, pw);
      }
      catch (const meosException &ex) {
        error = ex.wwhat();
      }
      catch (std::exception &ex) {
        error = gdi.widen(ex.what());
        if (error.empty())
          error = L"Okänt fel";
      }

      if (!error.empty()) {
        gdi.fillRight();
        gdi.pushX();
        gdi.addString("", 1, "Operationen misslyckades: ");
        gdi.addString("", 0, error).setColor(colorRed);
        gdi.dropLine(2);
        gdi.popX();
        gdi.addButton("Cancel", "Avbryt", CompetitionCB);
        gdi.addButton(bi.id, "Försök igen", CompetitionCB);
        removeTempFile(startlist);
        removeTempFile(zipped);
        gdi.refresh();
        return 0;
      }

      removeTempFile(startlist);
      removeTempFile(zipped);
      gdi.addString("", 1, "Klart");

      xmlparser xml;
      xml.read(result.c_str());
      xmlobject obj = xml.getObject("ImportStartListResult");
      if (obj) {
        string url;
        obj.getObjectString("StartListUrl", url);
        if (url.length()>0) {
          gdi.fillRight();
          gdi.pushX();
          gdi.dropLine();
          gdi.addString("", 0, "Länk till startlistan:");
          gdi.addString("link", 0, url, CompetitionCB).setColor(colorRed);
        }
      }
      gdi.dropLine(3);
      gdi.popX();

      gdi.addButton("CopyLink", "Kopiera länken till urklipp", CompetitionCB);
      gdi.addButton("Cancel", "Återgå", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id == "EventorResult") {
      ClassConfigInfo cnf;
      oe->getClassConfigurationInfo(cnf);
      if (cnf.hasPatrol()) {
        if (!gdi.ask(L"När denna version av MeOS släpptes kunde Eventor "
                     "inte hantera resultat från patrullklasser. Vill du försöka ändå?"))
          return loadPage(gdi);
      }

      checkReadyForResultExport(gdi, set<int>());

      gdi.clearPage(true);
      gdi.fillDown();
      gdi.dropLine();
      gdi.addString("", boldLarge, "Publicerar resultat");

      gdi.dropLine();
      gdi.fillDown();
      gdi.addString("", 1, "Ansluter till Internet").setColor(colorGreen);

      gdi.refreshFast();
      Download dwl;
      dwl.initInternet();

      wstring resultlist = getTempFile();
      set<int> classes;
      bool eventorUTC = oe->getPropertyInt("UseEventorUTC", 0) != 0;
      oe->exportIOFSplits(oEvent::IOF30, resultlist.c_str(), false,
                          eventorUTC, classes, -1, false, true, 
                          false, true);
      vector<wstring> fileList;
      fileList.push_back(resultlist);

      wstring zipped = getTempFile();
      zip(zipped.c_str(), 0, fileList);
      ProgressWindow pw(gdi.getHWNDTarget());
      pw.init();
      vector<pair<wstring,wstring> > key;
      getAPIKey(key);

      wstring result = getTempFile();
      wstring error;
      try {
        dwl.postFile(eventorBase + L"import/resultlist", zipped, result, key, pw);
      }
      catch (const meosException &ex) {
        error = ex.wwhat();
      }
      catch (std::exception &ex) {
        error = gdi.widen(ex.what());
        if (error.empty())
          error = L"Okänt fel";
      }

      if (!error.empty()) {
        gdi.fillRight();
        gdi.pushX();
        gdi.addString("", 1, "Operationen misslyckades: ");
        gdi.addString("", 0, error).setColor(colorRed);
        gdi.dropLine(2);
        gdi.popX();
        gdi.addButton("Cancel", "Avbryt", CompetitionCB);
        gdi.addButton(bi.id, "Försök igen", CompetitionCB);
        removeTempFile(resultlist);
        removeTempFile(zipped);
        gdi.refresh();
        return 0;
      }

      removeTempFile(resultlist);
      removeTempFile(zipped);
      gdi.addString("", 1, "Klart");

      xmlparser xml;
      xml.read(result.c_str());
      xmlobject obj = xml.getObject("ImportResultListResult");
      if (obj) {
        string url;
        obj.getObjectString("ResultListUrl", url);
        if (url.length()>0) {
          gdi.fillRight();
          gdi.pushX();
          gdi.dropLine();
          gdi.addString("", 0, "Länk till resultatlistan:");
          gdi.addString("link", 0, url, CompetitionCB).setColor(colorRed);
        }
      }
      gdi.dropLine(3);
      gdi.popX();

      gdi.addButton("CopyLink", "Kopiera länken till urklipp", CompetitionCB);
      gdi.addButton("Cancel", "Återgå", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id == "Eventor") {
      if (checkEventor(gdi, bi))
        return 0;

      SYSTEMTIME st;
      GetLocalTime(&st);
      st.wYear--; // Include last years competitions
      getEventorCompetitions(gdi, convertSystemDate(st),  events);
      gdi.clearPage(true);

      gdi.addString("", boldLarge, "Hämta data från Eventor");

      gdi.dropLine();
      gdi.addButton("EventorAPI", "Anslutningsinställningar...", CompetitionCB);
      gdi.dropLine();
      gdi.fillRight();
      gdi.pushX();
      gdi.addCheckbox("EventorCmp", "Hämta tävlingsdata", CompetitionCB, true);
      gdi.addSelection("EventorSel", 300, 200);
      sort(events.begin(), events.end());
      st.wYear++; // Restore current time
      wstring now = convertSystemDate(st);

      int selected = 0; // Select next event by default
      for (int k = events.size()-1; k>=0; k--) {
        wstring n = events[k].Name + L" (" + events[k].Date + L")";
        gdi.addItem("EventorSel", n, k);
        if (now < events[k].Date || selected == 0)
          selected = k;
      }
      gdi.selectItemByData("EventorSel", selected);

      gdi.dropLine(3);
      gdi.popX();
      gdi.addCheckbox("EventorDb", "Uppdatera löpardatabasen", CompetitionCB, true);
      gdi.dropLine(3);
      gdi.popX();
      gdi.addButton("Cancel", "Avbryt", CompetitionCB);
      gdi.addButton("EventorNext", "Nästa >>", CompetitionCB);
    }
    else if (bi.id == "EventorCmp") {
      gdi.setInputStatus("EventorSel", gdi.isChecked(bi.id));
      gdi.setInputStatus("EventorNext", gdi.isChecked(bi.id) | gdi.isChecked("EventorDb"));
    }
    else if (bi.id == "EventorDb") {
      gdi.setInputStatus("EventorNext", gdi.isChecked(bi.id) | gdi.isChecked("EventorCmp"));
    }
    else if (bi.id == "EventorNext") {
      bool cmp = gdi.isChecked("EventorCmp");
      bool db = gdi.isChecked("EventorDb");
      ListBoxInfo lbi;
      gdi.getSelectedItem("EventorSel", lbi);
      const CompetitionInfo *ci = 0;
      if (lbi.data < events.size())
        ci = &events[lbi.data];

      gdi.clearPage(true);
      gdi.setData("UpdateDB", db);
      gdi.pushX();
      if (cmp && ci) {
        gdi.setData("EventIndex", lbi.data);
        gdi.setData("EventorId", ci->Id);
        gdi.addString("", boldLarge, L"Hämta tävlingsdata för X#" + ci->Name);
        gdi.dropLine(0.5);

        gdi.fillRight();
        gdi.pushX();

        int tt = convertAbsoluteTimeHMS(ci->firstStart, -1);
        wstring ttt = tt>0 ? ci->firstStart : L"";
        gdi.addInput("FirstStart", ttt, 10, 0, L"Första ordinarie starttid:", L"Skriv första starttid på formen HH:MM:SS");

        gdi.addSelection("StartType", 200, 150, 0, L"Startmetod", L"help:startmethod");
        gdi.addItem("StartType", lang.tl("Gemensam start"), SMCommon);
        gdi.addItem("StartType", lang.tl("Lottad startlista"), SMDrawn);
        gdi.addItem("StartType", lang.tl("Fria starttider"), SMFree);
        gdi.addItem("StartType", lang.tl("Jag sköter lottning själv"), SMCustom);
        gdi.selectFirstItem("StartType");
        gdi.fillDown();
        gdi.popX();
        gdi.dropLine(3);

        gdi.addInput("LastEntryDate", ci->lastNormalEntryDate, 10, 0, L"Sista ordinarie anmälningsdatum:");

        if (oe->getNumRunners() > 0) {
          gdi.addCheckbox("RemoveRemoved", "Ta bort eventuella avanmälda deltagare", 0, true);
        }

        gdi.addString("", boldText, "Importera banor");
        gdi.addString("", 10, "help:ocad13091");
        gdi.fillRight();
        gdi.dropLine();
        gdi.addInput("FileName", L"", 48, 0, L"Filnamn (OCAD banfil):");
        gdi.dropLine();
        gdi.fillDown();
        gdi.addButton("BrowseCourse", "Bläddra...", CompetitionCB);
      }
      else {
        gdi.addString("", boldLarge, "Hämta löpardatabasen");
        gdi.dropLine(0.5);

        bi.id = "EventorImport";
        return competitionCB(gdi, type, &bi);
      }

      gdi.dropLine(1);
      gdi.popX();
      gdi.setRestorePoint("DoEventor");
      gdi.fillRight();
      gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();
      gdi.addButton("EventorImport", "Hämta data från Eventor", CompetitionCB).setDefault();
      gdi.fillDown();
      gdi.popX();
    }
    else if (bi.id == "EventorImport") {
      const int diffZeroTime = 3600;
      DWORD id;
      DWORD db;
      gdi.getData("EventorId", id);
      gdi.getData("UpdateDB", db);

      DWORD eventIndex;
      gdi.getData("EventIndex", eventIndex);
      const CompetitionInfo *ci = 0;
      if (eventIndex < events.size())
        ci = &events[eventIndex];

      bool removeRemoved = true;
      if (gdi.hasField("RemoveRemoved"))
        removeRemoved = gdi.isChecked("RemoveRemoved");

      wstring course = gdi.getText("FileName", true);
      int startType = 0;
      const bool createNew = oe->getExtIdentifier() != id && id>0;
      int zeroTime = 0;
      int firstStart = 0;
      wstring lastEntry;
      if (id > 0 && createNew) {
        wstring fs = gdi.getText("FirstStart");
        int t = oEvent::convertAbsoluteTime(fs);
        if (t<0) {
          wstring msg = L"Ogiltig starttid: X#" + fs;
          throw meosException(msg);
        }
        firstStart = t;
        zeroTime = t - diffZeroTime;
        if (zeroTime<0)
          zeroTime += 3600*24;

        startType = gdi.getSelectedItem("StartType").first;
        lastEntry = gdi.getText("LastEntryDate");
      }

      if (gdi.hasField("EventorImport")) {
        gdi.disableInput("EventorImport");
        gdi.disableInput("FileName");
        gdi.disableInput("FirstStart");
      }

      wstring tEvent = getTempFile();
      wstring tClubs = getTempFile();
      wstring tClass = getTempFile();
      wstring tEntry = getTempFile();
      wstring tRunnerDB = db!= 0 ? getTempFile() : L"";
      gdi.dropLine(3);
      wstring error;
      try {
        getEventorCmpData(gdi, id, tEvent, tClubs, tClass, tEntry, tRunnerDB);
      }
      catch (const meosException &ex) {
        error = ex.wwhat();
      }
      catch (std::exception &ex) {
        error = gdi.widen(ex.what());
        if (error.empty())
          error = L"Okänt fel";
      }

      if (!error.empty()) {
        gdi.popX();
        gdi.dropLine();
        gdi.fillDown();
        gdi.addString("", 0, wstring(L"Fel: X#") + error).setColor(colorRed);
        gdi.addButton("Cancel", "Återgå", CompetitionCB);
        gdi.refresh();
        return 0;
      }

      gdi.fillDown();
      gdi.dropLine();

      if (db != 0) {
        gdi.addString("", 1, "Behandlar löpardatabasen").setColor(colorGreen);
        vector<wstring> extractedFiles;
        gdi.fillRight();
        gdi.addString("", 0 , "Packar upp löpardatabas...");
        gdi.refreshFast();
        unzip(tRunnerDB.c_str(), 0, extractedFiles);
        gdi.addString("", 0 , "OK");
        gdi.refreshFast();
        gdi.dropLine();
        gdi.popX();
        gdi.fillDown();
        removeTempFile(tRunnerDB);
        if (extractedFiles.size() != 1) {
          gdi.addString("", 0, L"Unexpected file contents: X#" + tRunnerDB).setColor(colorRed);
        }
        if (extractedFiles.empty())
          tRunnerDB.clear();
        else
          tRunnerDB = extractedFiles[0];
      }

      oe->importXML_IOF_Data(tClubs, tRunnerDB, true);
      removeTempFile(tClubs);

      if (id > 0) {
        gdi.dropLine();
        gdi.addString("", 1, "Behandlar tävlingsdata").setColor(colorGreen);
        set<int> noFilter;
        string noType;

        if (createNew && id>0) {
          gdi.addString("", 1, "Skapar ny tävling");
          oe->newCompetition(L"New");
          oe->importXML_EntryData(gdi, tEvent, false, false, noFilter, noType);
          oe->setZeroTime(formatTimeHMS(zeroTime));
          oe->getDI().setDate("OrdinaryEntry", lastEntry);
          if (ci) {
            if (!ci->account.empty())
              oe->getDI().setString("Account", ci->account);

            if (!ci->url.empty())
              oe->getDI().setString("Homepage", ci->url);
          }
        }
        removeTempFile(tEvent);

        oe->importXML_EntryData(gdi, tClass.c_str(), false, false, noFilter, noType);
        removeTempFile(tClass);

        set<int> stageFilter;
        string preferredIdType;
        checkStageFilter(gdi, tEntry, stageFilter, preferredIdType);
        oe->importXML_EntryData(gdi, tEntry.c_str(), false, removeRemoved, stageFilter, preferredIdType);
        removeTempFile(tEntry);

        if (!course.empty()) {
          gdi.dropLine();
          TabCourse::runCourseImport(gdi, course, oe, true);
        }

        set<int> clsWithRef;
        oTeam::checkClassesWithReferences(*oe, clsWithRef);
        if (!clsWithRef.empty()) {
          if (gdi.ask(L"ask:convert_to_patrol")) {
            oTeam::convertClassWithReferenceToPatrol(*oe, clsWithRef);
          }
        }

        bool drawn = false;
        if (createNew && startType>0) {
          gdi.scrollToBottom();
          gdi.dropLine();

          switch (startType) {
            case SMCommon:
              oe->automaticDrawAll(gdi, formatTimeHMS(firstStart), L"0", L"0", false, oEvent::DrawMethod::Random, 1);
              drawn = true;
              break;

            case SMDrawn:
              ClassConfigInfo cnf;
              oe->getClassConfigurationInfo(cnf);
              bool skip = false;
              if (!cnf.classWithoutCourse.empty()) {
                wstring cls = L"";
                for (size_t k = 0; k < cnf.classWithoutCourse.size(); k++) {
                  if (k>=5) {
                    cls += L"...";
                    break;
                  }
                  if (k>0)
                    cls += L", ";
                  cls += cnf.classWithoutCourse[k];
                }
                if (!gdi.ask(L"ask:missingcourse#" + cls)) {
                  gdi.addString("", 0, "Skippar lottning");
                  skip = true;
                }
              }
              if (!skip)  
                oe->automaticDrawAll(gdi, formatTimeHMS(firstStart), L"2:00", L"2", true, oEvent::DrawMethod::MeOS, 1);
              drawn = true;
              break;
          }
        }
      }

      gdi.dropLine();
      gdi.addString("", 1, "Klart").setColor(colorGreen);

      if (id > 0) {
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

        if (oe->hasTeam()) {
          oe->getMeOSFeatures().useFeature(MeOSFeatures::Relay, true, *oe);
        }
      }
      gdi.scrollToBottom();
      gdi.dropLine();
      if (gdi.hasField("Cancel"))
        gdi.disableInput("Cancel"); // Disable "cancel" above
      gdi.fillRight();
      if (id > 0)
        gdi.addButton("StartIndividual", "Visa startlistan", ListsCB);
      gdi.addButton("Cancel", "Återgå", CompetitionCB);
      gdi.refreshFast();

    }
    else if (bi.id == "Cancel"){
      loadPage(gdi);
    }
    else if (bi.id == "WelcomeOK") {
      gdi.scaleSize(1.0/gdi.getScale());
      oe->setProperty("FirstTime", 0);
      loadPage(gdi);
    }
    else if (bi.id == "dbtest") {

    }
    else if (bi.id=="FreeImport") {
      gdi.clearPage(true);
      gdi.addString("", 2, "Fri anmälningsimport");
      gdi.addString("", 10, "help:33940");
      gdi.dropLine(0.5);
      gdi.addInputBox("EntryText", 550, 280, entryText, 0, L"");
      gdi.dropLine(0.5);
      gdi.fillRight();
      gdi.addButton("PreviewImport", "Granska inmatning", CompetitionCB, "tooltip:analyze");
      gdi.addButton("Cancel", "Avbryt", CompetitionCB);
      gdi.addButton("Paste", "Klistra in", CompetitionCB, "tooltip:paste");
      gdi.addButton("ImportFile", "Importera fil...", CompetitionCB, "tooltip:import");
      gdi.addButton("ImportDB", "Bygg databaser...", CompetitionCB, "tooltip:builddata");
      gdi.fillDown();
    }
    else if (bi.id=="ImportDB") {
      if (!gdi.ask(L"help:146122"))
        return 0;

      gdi.setWaitCursor(true);
      oe->getFreeImporter(fi);
      fi.buildDatabases(*oe);
      fi.save();
      gdi.setWaitCursor(false);
    }
    else if (bi.id=="Paste") {
      gdi.pasteText("EntryText");
    }
    else if (bi.id=="ImportFile") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Textfiler", L"*.txt"));

      wstring file=gdi.browseForOpen(ext, L"txt");
      ifstream fin(file.c_str());
      char bf[1024];
      bf[0]='\r';//WCS
      bf[1]='\n';
      entryText.clear();
      while (fin.good() && !fin.eof()) {
        fin.getline(bf+2, 1024-2);
        entryText += gdi.recodeToWide(bf);
      }
      entryText+=L"\r\n";
      fin.close();

      gdi.setText("EntryText", entryText);
    }
    else if (bi.id=="PreviewImport") {
      oe->getFreeImporter(fi);
      entryText=gdi.getText("EntryText");
      gdi.clearPage(false);
      gdi.addString("", 2, "Förhandsgranskning, import");
      gdi.dropLine(0.5);
      wchar_t *bf=new wchar_t[entryText.length()+1];
      wcscpy_s(bf, entryText.length()+1, entryText.c_str());
      try {
        fi.extractEntries(bf, entries);
        delete[] bf;
      }
      catch (...) {
        delete[] bf;
        throw;
      }
      vector<pClass> cls;
      oe->getClasses(cls, true);
      for (size_t k = 0; k < entries.size(); k++) {
        if (entries[k].eClass.empty()) {
          if (!cls.empty()) {
            entries[k].eClass = cls.back()->getName(); // Fallback
          }
          else {
            entries[k].eClass = lang.tl("Klass");
          }
        }
      }
  
      fi.showEntries(gdi, entries);
      gdi.fillRight();
      gdi.dropLine(1);
      gdi.addButton("DoFreeImport", "Spara anmälningar", CompetitionCB);
      gdi.addButton("FreeImport", "Ändra", CompetitionCB);

      gdi.addButton("Cancel", "Avbryt", CompetitionCB);
      gdi.scrollToBottom();
    }
    else if (bi.id=="DoFreeImport") {
      fi.addEntries(oe, entries);
      entryText.clear();

      // Update qualification/final
      vector<pClass> cls;
      oe->getClasses(cls, false);
      for (pClass c : cls) {
        c->updateFinalClasses(0, false);
      }
      loadPage(gdi);
    }
    else if (bi.id=="Startlist") {
      save(gdi, false);
      oe->sanityCheck(gdi, false);
      selectStartlistOptions(gdi);
    }
    else if (bi.id=="BrowseExport" || bi.id=="BrowseExportResult") {
      int filterIndex = gdi.getSelectedItem("Type").first;
      vector< pair<wstring, wstring> > ext;
      ImportFormats::getExportFilters(bi.id!="BrowseExport", ext);
      wstring save = gdi.browseForSave(ext, L"xml", filterIndex);

      if (save.length() > 0) {
        gdi.setText("Filename", save);
        gdi.selectItemByData("Type", filterIndex);
        wchar_t *fn = gdi.getExtra("Filename");
        if (fn) {
          gdi.enableInput(gdi.narrow(fn).c_str());
        }
      }
    }
    else if (bi.id=="DoSaveStartlist") {
      wstring save = gdi.getText("Filename");
      if (save.empty())
        throw meosException("Filnamn kan inte vara tomt");

      bool individual = !gdi.hasField("ExportTeam") || gdi.isChecked("ExportTeam");

      bool includeStage = true;
      if (gdi.hasField("IncludeRaceNumber"))
        includeStage = gdi.isChecked("IncludeRaceNumber");

      gdi.getSelection("ClassNewEntries", allTransfer);
      ImportFormats::ExportFormats filterIndex = ImportFormats::setExportFormat(*oe, gdi.getSelectedItem("Type").first);
      int cSVLanguageHeaderIndex = gdi.getSelectedItem("LanguageType").first;
      
      gdi.setWaitCursor(true);

      if (filterIndex == ImportFormats::IOF30 || filterIndex == ImportFormats::IOF203) {
        bool useUTC = oe->getDCI().getInt("UTC") != 0;
        oe->exportIOFStartlist(filterIndex == ImportFormats::IOF30 ? oEvent::IOF30 : oEvent::IOF20,
                                save.c_str(), useUTC, allTransfer, individual, includeStage, false);
      }
      else if (filterIndex == ImportFormats::OE) {
        oe->exportOECSV(save.c_str(), cSVLanguageHeaderIndex, false);
      }
      else {
        oListParam par;
        par.listCode = EStdStartList;
        par.setLegNumberCoded(-1);
        oListInfo li;
        par.selection = allTransfer;
        oe->generateListInfo(par,  gdi.getLineHeight(), li);
        gdioutput tGdi("temp", gdi.getScale());
        oe->generateList(tGdi, true, li, false);
        HTMLWriter::writeTableHTML(tGdi, save, oe->getName(), 0, 1.0);
        tGdi.openDoc(save.c_str());
      }
      loadPage(gdi);
    }
    else if (bi.id=="Splits") {
      save(gdi, false);
      oe->sanityCheck(gdi, true);
      selectExportSplitOptions(gdi);
    }
    else if (bi.id == "DoSaveSplits") {
      wstring save = gdi.getText("Filename");
      if (save.empty())
        throw meosException("Filnamn kan inte vara tomt");

      //bool individual = !gdi.hasField("ExportTeam") || gdi.isChecked("ExportTeam");
      gdi.getSelection("ClassNewEntries", allTransfer);
      
      checkReadyForResultExport(gdi, allTransfer);

      ImportFormats::ExportFormats filterIndex = ImportFormats::setExportFormat(*oe, gdi.getSelectedItem("Type").first);
      int cSVLanguageHeaderIndex = gdi.getSelectedItem("LanguageType").first;
      bool includeSplits = gdi.isChecked("ExportSplitTimes");
      
      bool unroll = gdi.isChecked("UnrollLoops"); // If not applicable, field does not exist.
      bool includeStage = true;
      if (gdi.hasField("IncludeRaceNumber"))
        includeStage = gdi.isChecked("IncludeRaceNumber");

      gdi.setWaitCursor(true);
      if (filterIndex == ImportFormats::IOF30 || filterIndex == ImportFormats::IOF203) {
        oEvent::IOFVersion ver = filterIndex == ImportFormats::IOF30 ? oEvent::IOF30 : oEvent::IOF20;
        ClassConfigInfo cnf;
        oe->getClassConfigurationInfo(cnf);
        bool useUTC = oe->getDCI().getInt("UTC") != 0;

        if (!cnf.hasTeamClass()) {
          oe->exportIOFSplits(ver, save.c_str(), true, useUTC, 
                              allTransfer, -1, false, unroll, includeStage, false);
        }
        else {
          ListBoxInfo leglbi;
          gdi.getSelectedItem("LegType", leglbi);
          wstring file = save;
          if (leglbi.data == 2) {
            wstring fileBase;
            wstring fileEnd = file.substr(file.length()-4);
            if (_wcsicmp(fileEnd.c_str(), L".XML") == 0)
              fileBase = file.substr(0, file.length() - 4);
            else {
              fileEnd = L".xml";
              fileBase = file;
            }
            ClassConfigInfo cnf;
            oe->getClassConfigurationInfo(cnf);
            int legMax = cnf.getNumLegsTotal();
            for (int leg = 0; leg<legMax; leg++) {
              file = fileBase + L"_" + itow(leg+1) + fileEnd;
              oe->exportIOFSplits(ver, file.c_str(), true, useUTC, 
                                  allTransfer, leg, false, unroll, includeStage, false);
            }
          }
          else if (leglbi.data == 3) {
            oe->exportIOFSplits(ver, file.c_str(), true, useUTC, allTransfer, 
                                -1, true, unroll, includeStage, false);
          }
          else {
            int leg = leglbi.data == 1 ? -1 : leglbi.data - 10;
            oe->exportIOFSplits(ver, file.c_str(), true, useUTC, allTransfer, 
                                leg, false, unroll, includeStage, false);
          }
        }
      }
      else if (filterIndex == ImportFormats::OE) {
        oe->exportOECSV(save.c_str(), cSVLanguageHeaderIndex, includeSplits);
      }
      else {
        oListParam par;
        par.listCode = EStdResultList;
        par.showSplitTimes = true;
        par.setLegNumberCoded(-1);
        oListInfo li;
        oe->generateListInfo(par,  gdi.getLineHeight(), li);
        gdioutput tGdi("temp", gdi.getScale());
        oe->generateList(tGdi, true, li, false);
        HTMLWriter::writeTableHTML(tGdi, save, oe->getName(), 0, 1.0);
        tGdi.openDoc(save.c_str());
      }

      loadPage(gdi);
    }
    else if (bi.id=="SaveAs") {
      oe->sanityCheck(gdi, false);
      save(gdi, true);
      exportFileAs(hWndMain, gdi);
    }
    else if (bi.id=="Duplicate") {
      oe->duplicate();
      gdi.alert("Skapade en lokal kopia av tävlingen.");
    }
    else if (bi.id=="Import") {
      //Import complete competition
      importFile(hWndMain, gdi);
      loadPage(gdi);
    }
    else if (bi.id=="Restore") {
      listBackups(gdi);
    }
    else if (bi.id=="Save") {
      save(gdi, true);
      resetSaveTimer();
    }
    else if (bi.id=="CloseCmp") {
      gdi.setWaitCursor(true);
      if (!showConnectionPage)
        save(gdi, false);
      oe->save();
      oe->newCompetition(L"");
      resetSaveTimer();
      gdi.setWindowTitle(L"");
      if (showConnectionPage)
        loadConnectionPage(gdi);
      else
        loadPage(gdi);
      gdi.setWaitCursor(false);
    }
    else if (bi.id=="Delete" &&
      gdi.ask(L"Vill du verkligen radera tävlingen?")) {

      if (oe->isClient())
        oe->dropDatabase();
      else if (!oe->deleteCompetition())
        gdi.alert("Operation failed. It is not possible to delete competitions on server");

      oe->clearListedCmp();
      oe->newCompetition(L"");
      gdi.setWindowTitle(L"");
      loadPage(gdi);
    }
    else if (bi.id=="NewCmp") {
      bool guideMode = true;
      if (guideMode) {
        newCompetitionGuide(gdi, 0);
        return 0;
      }

      oe->newCompetition(lang.tl(L"Ny tävling"));
      gdi.setWindowTitle(L"");

      if (useEventor()) {
        int age = getRelativeDay() - oe->getPropertyInt("DatabaseUpdate", 0);
        if (age>60 && gdi.ask(L"help:dbage")) {
          bi.id = "EventorUpdateDB";
          if (checkEventor(gdi, bi))
            return 0;
          return competitionCB(gdi, type, &bi);
        }
      }

      loadPage(gdi);
      return 0;
    }
    else if (bi.id=="OpenCmp") {
      ListBoxInfo lbi;
      int id=0;
      bool frontPage=true;
      if (!gdi.getSelectedItem("CmpSel", lbi)) {
        frontPage=false;
        if ( gdi.getSelectedItem("ServerCmp", lbi) )
          id=lbi.data;
        else if ( gdi.getSelectedItem("LocalCmp", lbi) )
          id=lbi.data;
      }
      else id=lbi.data;

      if (id==0)
        throw meosException("Ingen tävling vald.");

      openCompetition(gdi, id);
     
      if (frontPage)
        loadPage(gdi);
      else {
        oe->setProperty("UseDirectSocket", gdi.isChecked("UseDirectSocket"));
        oe->verifyConnection();
        oe->validateClients();
        loadConnectionPage(gdi);
      }

      if (oe->isClient() && oe->getPropertyInt("UseDirectSocket", true) != 0) {
          oe->getDirectSocket().startUDPSocketThread(gdi.getHWNDMain());
      }
      return 0;
    }
    else if (bi.id=="BrowseCourse") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Banor, OCAD semikolonseparerat", L"*.csv;*.txt"));
      ext.push_back(make_pair(L"Banor, IOF (xml)", L"*.xml"));

      wstring file = gdi.browseForOpen(ext, L"csv");
      if (file.length()>0)
        gdi.setText("FileName", file);
    }
    else if (bi.id=="BrowseEntries") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Importerbara", L"*.xml;*.csv"));
      ext.push_back(make_pair(L"IOF (xml)", L"*.xml"));
      ext.push_back(make_pair(L"OE Semikolonseparerad (csv)", L"*.csv"));

      wstring file = gdi.browseForOpen(ext, L"xml");
      if (file.length()>0) {
        const wchar_t *ctrl = bi.getExtra();
        if (ctrl != 0)
          gdi.setText(ctrl, file);
      }
    }
    else if (bi.id=="Entries") {
      if (!save(gdi, false))
        return 0;

      gdi.clearPage(false);

      entryForm(gdi, false);

      gdi.pushX();
      gdi.fillRight();
      gdi.addButton("DoImport", "Importera", CompetitionCB);
      gdi.fillDown();
      gdi.addButton("Cancel", "Avbryt", CompetitionCB);
      gdi.popX();
      gdi.refresh();
    }
    else if (bi.id=="DoImport") {
      gdi.enableEditControls(false);
      gdi.disableInput("DoImport");
      gdi.disableInput("Cancel");
      gdi.disableInput("BrowseEntries");
      bool removeRemoved = gdi.isChecked("RemoveRemoved");
      try {
        gdi.autoRefresh(true);
        FlowOperation res = saveEntries(gdi, removeRemoved, false);

        if (res != FlowContinue) {
          if (res == FlowCancel)
            loadPage(gdi);
          return 0;
        }
      }
      catch (std::exception &) {
        gdi.enableEditControls(true);
        gdi.enableInput("DoImport");
        gdi.enableInput("Cancel");
        gdi.enableInput("BrowseEntries");
        gdi.refresh();
        throw;
      }

      gdi.addButton("Cancel", "OK", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id=="Courses") {
      if (!save(gdi, false))
        return 0;
      TabCourse::setupCourseImport(gdi, CompetitionCB);
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
      gdi.dropLine();
      gdi.addButton("Cancel", "OK", CompetitionCB);
      gdi.refresh();
    }
    else if (bi.id=="About") {
      loadAboutPage(gdi);
    }
    else if (bi.id == "DBEntry") {
      int classId = gdi.getSelectedItem("Classes").first;

      DWORD data;
      gdi.getData("RunnerIx", data);
      RunnerWDBEntry *dbr = oe->getRunnerDatabase().getRunnerByIndex(data);

      // Construct runner from database
      oRunner sRunner(oe, 0);
      sRunner.init(*dbr, false);
      pRunner added = oe->addRunnerFromDB(&sRunner, classId, true);
      if (added)
        added->synchronize();
      loadRunnerDB(gdi, 1, false);
    }
    else if (bi.id == "CancelRunnerDatabase") {
      if (!oe->empty())
        loadRunnerDB(gdi, 0, false);
      else
        loadPage(gdi);
    }
    else if (bi.id == "CancelEntry") {
      loadRunnerDB(gdi, 1, false);
    }
    else if (bi.id == "RunnerDB") {
      loadRunnerDB(gdi, 1, true);
    }
    else if (bi.id == "ClubDB") {
      loadRunnerDB(gdi, 2, true);
    }
    else if (bi.id == "ExportRunnerDB") {
      xmlparser xml;
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"IOF Löpardatabas, version 3.0 (xml)", L"*.xml"));
      int ix;
      wstring fileName = gdi.browseForSave(ext, L"xml", ix);
      if (fileName.empty())
        return false;

      gdi.setWaitCursor(true);
      xml.openOutput(fileName.c_str(), false);
      IOF30Interface writer(oe, false);
      writer.writeRunnerDB(oe->getRunnerDatabase(), xml);
      gdi.setWaitCursor(false);
    }
    else if (bi.id == "ExportClubDB") {
      xmlparser xml;
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"IOF Klubbdatabas, version 3.0 (xml)", L"*.xml"));
      int ix;
      wstring fileName = gdi.browseForSave(ext, L"xml", ix);
      if (fileName.empty())
        return false;

      gdi.setWaitCursor(true);
      xml.openOutput(fileName.c_str(), false);
      IOF30Interface writer(oe, false);
      writer.writeClubDB(oe->getRunnerDatabase(), xml);
      gdi.setWaitCursor(false);
    }
    else if (bi.id == "ClearDB") {
      if (gdi.ask(L"ask:cleardb")) {
        oe->getRunnerDatabase().clearClubs();
        oe->saveRunnerDatabase(L"database", true);
        if (oe->isClient()) {
          msUploadRunnerDB(oe);
        }
        loadRunnerDB(gdi, 0, false);
      }
    }
  }
  else if (type==GUI_LISTBOXSELECT) {
    ListBoxInfo lbi=*(ListBoxInfo *)data;
    if (lbi.id == "LocalCmp") {
      gdi.selectItemByData("ServerCmp", -1);
      gdi.sendCtrlMessage("OpenCmp");
    }
    else if (lbi.id == "ServerCmp") {
      gdi.selectItemByData("LocalCmp", -1);
      gdi.sendCtrlMessage("OpenCmp");
    }
  }
  else if (type==GUI_LISTBOX) {
    ListBoxInfo lbi=*(ListBoxInfo *)data;

    if (lbi.id=="LocalCmp") {
      gdi.selectItemByData("ServerCmp", -1);
      gdi.disableInput("Repair", true);
    }
    else if (lbi.id=="ServerCmp") {
      gdi.selectItemByData("LocalCmp", -1);
      gdi.enableInput("Repair", true);
    }
    else if (lbi.id=="TextSize") {
      int textSize = lbi.data;
      oe->setProperty("TextSize", textSize);
      gdi.setFont(textSize, oe->getPropertyString("TextFont", L"Arial"));
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TCmpTab, 0);
    }
    else if (lbi.id == "Language") {
      lang.get().loadLangResource(lbi.text);
      oe->updateTabs(true);
      oe->setProperty("Language", lbi.text);
      //gdi.setEncoding(interpetEncoding(lang.tl("encoding")));
      gdi.setFont(oe->getPropertyInt("TextSize", 0), oe->getPropertyString("TextFont", L"Arial"));
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TCmpTab, 0);
    }
    else if (lbi.id == "PreEvent") {
      gdi.setInputStatus("OpenPre", int(lbi.data)>0);
    }
    else if (lbi.id == "PostEvent") {
      bool hasPost = int(lbi.data)>0;
      gdi.setInputStatus("OpenPost", hasPost);
      gdi.setInputStatus("TransferData", hasPost);
      gdi.setInputStatus("CloneCmp", !hasPost);
    }
    else if (lbi.id == "StageNumber") {
      int nr = int(lbi.data);
      oe->setStageNumber(nr);
      oe->synchronize(true);
    }
    else if (lbi.id == "Type") {
      setExportOptionsStatus(gdi, lbi.data);
    }
  }
  else if (type== GUI_INPUT) {
    InputInfo ii=*(InputInfo *)data;
    if (ii.id == "Filename") {
      const wchar_t *fn = ii.getExtra();
      if (fn) {
        gdi.setInputStatus(gdi.narrow(fn).c_str(), !ii.text.empty());
      }
    }
    else if (ii.id == "NumStages") {
      int ns = gdi.getTextNo("NumStages");
      oe->setNumStages(ns);
      oe->synchronize(true);
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo ii=*(InputInfo *)data;
    if (ii.id == "Filename") {
      const wchar_t *fn = ii.getExtra();
      if (fn) {
        gdi.setInputStatus(gdi.narrow(fn).c_str(), !ii.text.empty());
      }
    }
  }
  else if (type==GUI_EVENT) {
    EventInfo ei=*(EventInfo *)data;

    if ( ei.id=="Connections" ) {
      wstring s=gdi.getText("ClientName");
      loadConnectionPage(gdi);
      gdi.setText("ClientName", s);
    }
    else if (ei.id=="CellAction") {
      string org = ei.getOrigin();
      if (org == "runnerdb") {
        int ix = ei.getExtraInt();
        const RunnerWDBEntry *pRdb = oe->getRunnerDatabase().getRunnerByIndex(ix);
        if (pRdb == 0)
          throw meosException("Internal error");

        const RunnerWDBEntry &rdb = *pRdb;
        vector<int> classes;
        bool suggest = oe->getClassesFromBirthYear(rdb.getBirthYear(), interpretSex(rdb.getSex()), classes);

        gdi.clearPage(true, false);
        if (suggest || find(classes.begin(), classes.end(), lastSelectedClass) == classes.end()) {
          if (classes.empty())
            lastSelectedClass = -1;
          else
            lastSelectedClass = classes.back();
        }

        wstring name;
        rdb.getName(name);
        gdi.addString("", boldLarge, L"Anmäl X#" + name);
        gdi.setData("RunnerIx", ix);
        gdi.dropLine();
        gdi.addSelection("Classes", 200, 300, 0, L"Klasser:");
        oe->fillClasses(gdi, "Classes", oEvent::extraNone, oEvent::filterNone);

        if (lastSelectedClass != -1)
          gdi.selectItemByData("Classes", lastSelectedClass);
        else
          gdi.selectFirstItem("Classes");

        gdi.dropLine();
        gdi.fillRight();
        gdi.addButton("DBEntry", "Anmäl", CompetitionCB).setDefault();
        gdi.addButton("CancelEntry", "Avbryt", CompetitionCB).setCancel();
        gdi.refresh();
      }
    }
  }
  else if (type==GUI_CLEAR) {
    if (gdi.isInputChanged("")) {
      if (gdi.hasField("SaveSettings")) {
        gdi.sendCtrlMessage("SaveSettings");
      }
      else {
        wstring name=gdi.getText("Name");

        if (!name.empty() && !oe->empty())
          save(gdi, false);
      }
    }
    return 1;
  }
  return 0;
}

void TabCompetition::openCompetition(gdioutput &gdi, int id) {
  gdi.setWaitCursor(true);
  wstring err;
  try {
    if (!oe->open(id)) {
      gdi.alert("Kunde inte öppna tävlingen.");
      return;
    }
  }
  catch (const meosException &ex) {
    err = ex.wwhat();
  }

  resetSaveTimer();
  oe->setProperty("LastCompetition", id);
  gdi.setWindowTitle(oe->getTitleName());
  oe->updateTabs();

  if (!err.empty()) {
    gdi.alert(err);
  }
}

int TabCompetition::restoreCB(gdioutput &gdi, int type, void *data) {
  TextInfo &ti = *(TextInfo *)data;
  int id = ti.getExtraInt();
  const BackupInfo &bi = oe->getBackup(id);

  if (ti.id == "") {
    wstring fi(bi.FullPath);
    if (!oe->open(fi, false)) {
      gdi.alert("Kunde inte öppna tävlingen.");
    }
    else {
      const wstring &name = oe->getName();
      if (name.find_last_of(L"}") != name.length()-1)
        oe->setName(name + L" {" + lang.tl(L"återställd") + L"}");

      oe->restoreBackup();

      gdi.setWindowTitle(oe->getTitleName());
      oe->updateTabs();
      resetSaveTimer();
      loadPage(gdi);
    }
  }
  else if (ti.id == "EraseBackup") {
    if (gdi.ask(L"Vill du ta bort alla säkerhetskopior på X?#" + bi.Name)) {
      gdi.setWaitCursor(true);
      oe->deleteBackups(bi);
      listBackups(gdi);
    }
  }
  return 0;
}

void TabCompetition::listBackups(gdioutput &gdi) {
  wchar_t bf[260];
  getUserFile(bf, L"");
  int yo = gdi.getOffsetY();
  gdi.clearPage(false);
  oe->enumerateBackups(bf);

  gdi.addString("", boldLarge|Capitalize, "Lagrade säkerhetskopior");
  gdi.addString("", 0, "help:restore_backup");
  gdi.dropLine(0.4);
  gdi.addButton("Cancel", "Återgå", CompetitionCB);
  gdi.dropLine();
  oe->listBackups(gdi, ::restoreCB);
  gdi.scrollTo(0, yo);
  gdi.refresh();
}

void TabCompetition::copyrightLine(gdioutput &gdi) const
{
  gdi.pushX();
  gdi.fillRight();

  gdi.addButton("Help", "Hjälp", CompetitionCB, "");
  gdi.addButton("About", "Om MeOS...", CompetitionCB);

  gdi.dropLine(0.4);
  gdi.fillDown();
  gdi.addString("", 0, makeDash(L"#Copyright © 2007-2019 Melin Software HB"));
  gdi.dropLine(1);
  gdi.popX();

  gdi.addString("", 0, getMeosFullVersion()).setColor(colorDarkRed);
  gdi.dropLine(0.2);
}

void TabCompetition::loadAboutPage(gdioutput &gdi) const
{
  gdi.clearPage(false);
  gdi.addString("", textImage, "513");

  gdi.addString("", fontMediumPlus, makeDash(L"Om MeOS - ett Mycket Enkelt OrienteringsSystem")).setColor(colorDarkBlue);
  gdi.dropLine(1);

  wchar_t FileNamePath[260];
  getUserFile(FileNamePath, L"");
  gdi.pushX();
  gdi.fillRight();
  gdi.addString("", 0, "MeOS lokala datakatalog är: ");
  gdi.fillDown();
  gdi.addString("fnpath", 0, FileNamePath, CompetitionCB);
  gdi.popX();
  gdi.dropLine(0.5);
  RECT rc = { gdi.getCX(), gdi.getCY(), gdi.getPageX(), gdi.getCY() + 2 };
  gdi.addRectangle(rc, colorBlack);
  gdi.dropLine(1.5);
  gdi.setCX(gdi.getCX() + gdi.scaleLength(20));

  gdi.addStringUT(1, makeDash(L"Copyright © 2007-2019 Melin Software HB"));
  gdi.dropLine();
  gdi.addStringUT(10, "The database connection used is MySQL++\nCopyright "
				  "(c) 1998 by Kevin Atkinson, (c) 1999, 2000 and 2001 by MySQL AB,"
				  "\nand (c) 2004-2007 by Educational Technology Resources, Inc.\n"
				  "The database used is MySQL, Copyright (c) 2008-2017 Oracle, Inc."
				  "\n\nGerman Translation by Erik Nilsson-Simkovics"
				  "\n\nDanish Translation by Michael Leth Jess and Chris Bagge"
				  "\n\nRussian Translation by Paul A. Kazakov and Albert Salihov"
				  "\n\nOriginal French Translation by Jerome Monclard"
				  "\n\nAdaption to French conditions and extended translation by Pierre Gaufillet"
				  "\n\nCzech Translation by Marek Kustka"
				  "\n\nHelp with English documentation: Torbjörn Wikström");

  gdi.dropLine();
  gdi.addString("", 0, "Det här programmet levereras utan någon som helst garanti. Programmet är ");
  gdi.addString("", 0, "fritt att använda och du är välkommen att distribuera det under vissa villkor,");
  gdi.addString("", 0, "se license.txt som levereras med programmet.");

  gdi.dropLine();
  vector<wstring> supp;
  vector<wstring> developSupp;
  getSupporters(supp, developSupp);

  gdi.addString("", fontMediumPlus, "MeOS utvecklinsstöd");
  for (size_t k = 0; k<developSupp.size(); k++)
    gdi.addStringUT(fontMedium, developSupp[k]).setColor(colorDarkGreen);
  
  gdi.dropLine();

  gdi.addString("", fontMediumPlus, "Vi stöder MeOS");
  for (size_t k = 0; k<supp.size(); k++)
    gdi.addStringUT(0, supp[k]);

  gdi.dropLine();
  gdi.addButton("Cancel", "Stäng", CompetitionCB);
  gdi.refresh();
}

bool TabCompetition::useEventor() const {
  return oe->getPropertyInt("UseEventor", 0) == 1;
}

bool TabCompetition::loadPage(gdioutput &gdi)
{
  if (oe->getPropertyInt("FirstTime", 1) == 1) {
    welcomeToMeOS(gdi);
    return true;
  }
  showConnectionPage=false;
  oe->checkDB();
  gdi.clearPage(true);
  gdi.fillDown();

  if (oe->empty()) {
    gdi.addString("", 2, "Välkommen till MeOS");
    gdi.addString("", 1, makeDash(L"#- ")+ lang.tl("ett Mycket Enkelt OrienteringsSystem")).setColor(colorDarkBlue);
    gdi.dropLine();

    if (oe->getPropertyInt("UseEventor", 0) == 0) {
      if ( gdi.ask(L"eventor:question#" + lang.tl("eventor:help")) )
        oe->setProperty("UseEventor", 1);
      else
        oe->setProperty("UseEventor", 2);
    }

    gdi.fillRight();
    gdi.pushX();

    gdi.addSelection("CmpSel", 300, 400, CompetitionCB, L"Välj tävling:");

    wchar_t bf[260];
    getUserFile(bf, L"");
    oe->enumerateCompetitions(bf, L"*.meos");
    oe->fillCompetitions(gdi, "CmpSel",0);
    gdi.autoGrow("CmpSel");
    gdi.selectFirstItem("CmpSel");

    int lastCmp = oe->getPropertyInt("LastCompetition", 0);
    gdi.selectItemByData("CmpSel", lastCmp);

    gdi.dropLine();
    gdi.addButton("OpenCmp", "Öppna", CompetitionCB, "Öppna vald tävling").setDefault();

    gdi.dropLine(4);
    gdi.popX();

    gdi.addButton("NewCmp", "Ny tävling", CompetitionCB, "Skapa en ny, tom, tävling");
    if (useEventor())
      gdi.addButton("Eventor", "Tävling från Eventor...", CompetitionCB, "Skapa en ny tävling med data från Eventor");
    gdi.addButton("ConnectMySQL", "Databasanslutning...", CompetitionCB, "Anslut till en server");

    gdi.popX();
    gdi.dropLine(2.5);
    gdi.addButton("Import", "Importera tävling...", CompetitionCB, "Importera en tävling från fil");
    gdi.addButton("Restore", "Återställ säkerhetskopia...", CompetitionCB, "Visa tillgängliga säkerhetskopior");
    gdi.addButton("LocalSettings", "Ändra lokala inställningar...", CompetitionCB);

    gdi.popX();
    gdi.dropLine(3);

    gdi.dropLine(2.3);
    textSizeControl(gdi);

    gdi.popX();
    gdi.dropLine(3);
    if (enableTests) {
      gdi.fillRight();
      gdi.addButton("SaveTest", "#Save test", CompetitionCB);
      gdi.addButton("RunTest", "#Run tests", CompetitionCB);
      gdi.addSelection("Tests", 200, 200, 0);
      vector< pair<wstring, size_t> > tests;
      TestMeOS tm(oe, "ALL");
      tm.getTests(tests);
      gdi.addItem("Tests", tests);
      gdi.selectFirstItem("Tests");
      gdi.addButton("RunSpecificTest", "#Run", CompetitionCB);
      
      gdi.dropLine(2);
      gdi.popX();
    }

    gdi.fillDown();
    
    copyrightLine(gdi);

    gdi.addButton(gdi.getPageX()-gdi.scaleLength(180),
                  gdi.getCY()-gdi.getButtonHeight(),
                  "Exit", "Avsluta", CompetitionCB);
    gdi.setInputFocus("CmpSel", true);
  }
  else {
    oe->checkNecessaryFeatures();
    gdi.selectTab(tabId);

    //gdi.addString("", 3, "MeOS");
    gdi.addString("", textImage, "513");
    gdi.dropLine();
    oe->synchronize();

    gdi.pushX();
    gdi.fillRight();
    gdi.addInput("Name", oe->getName(), 24, 0, L"Tävlingsnamn:");
    gdi.fillDown();

    gdi.addInput("Annotation", oe->getAnnotation(), 20, 0, L"Kommentar / version:")
       .setBgColor(colorLightCyan);
    gdi.popX();

    gdi.fillRight();
    gdi.addInput("Date", oe->getDate(), 8, 0, L"Datum:");
    gdi.addInput("ZeroTime", oe->getZeroTime(), 8, 0, L"Nolltid:");

    gdi.fillDown();
    gdi.dropLine(1.2);
    gdi.addCheckbox("LongTimes", "Aktivera stöd för tider över 24 timmar", CompetitionCB, oe->useLongTimes());

    if (false && oe->isClient()) {
      gdi.popX();
      gdi.disableInput("ZeroTime");
      gdi.disableInput("LongTimes");
      if (oe->useLongTimes())
        gdi.disableInput("Date");
    }
    else {
      gdi.popX();
      if (!oe->useLongTimes())
        gdi.addString("ZeroTimeHelp", 0, "help:zero_time");
      else {
        gdi.addString("ZeroTimeHelp", 0, "help:long_times");
        gdi.disableInput("ZeroTime");
      }
    }

    gdi.fillRight();
    gdi.dropLine();

    if (oe->getExtIdentifier() > 0 && useEventor()) {
      gdi.addButton("SynchEventor", "Eventorkoppling", CompetitionCB, "Utbyt tävlingsdata med Eventor");
    }

    gdi.addButton("Settings", "Tävlingsinställningar", CompetitionCB);
    gdi.addButton("Features", "MeOS Funktioner", CompetitionCB);

#ifdef _DEBUG
    gdi.addButton("Test", "Test", CompetitionCB);
#endif

    gdi.fillDown();
    gdi.popX();

    gdi.dropLine(3);

    //gdi.fillRight();
    //gdi.addCheckbox("UseEconomy", "Hantera klubbar och ekonomi", CompetitionCB, oe->useEconomy());
    //gdi.addCheckbox("UseSpeaker", "Använd speakerstöd", CompetitionCB, oe->getDCI().getInt("UseSpeaker")!=0);
    //gdi.popX();
    //gdi.dropLine(2);

    //gdi.addCheckbox("UseRunnerDb", "Använd löpardatabasen", CompetitionCB, oe->useRunnerDb());

    //gdi.popX();
    //gdi.dropLine(2);
    textSizeControl(gdi);

    gdi.dropLine(4);
    gdi.popX();
    gdi.fillRight();
    gdi.addButton("Save", "Spara", CompetitionCB, "help:save");
    gdi.addButton("SaveAs", "Spara som fil...", CompetitionCB, "");
    gdi.addButton("Duplicate", "Duplicera", CompetitionCB, "help:duplicate");

    gdi.addButton("Delete", "Radera", CompetitionCB);
    gdi.addButton("CloseCmp", "Stäng", CompetitionCB);

    gdi.dropLine(2.5);
    gdi.popX();

#ifdef D_DEBUG
    gdi.dropLine(2.5);
    gdi.popX();
    gdi.addButton("CloneEvent", "#! Klona", CompetitionCB);
#endif

    gdi.fillDown();
    gdi.popX();

    gdi.newColumn();
    gdi.dropLine(3);
    gdi.setCX(gdi.getCX()+gdi.scaleLength(60));

    RECT rc;
    rc.top = gdi.getCY() - gdi.scaleLength(30);
    rc.left = gdi.getCX() - gdi.scaleLength(30);

    int bw = gdi.scaleLength(150);
    gdi.addString("", 1, "Importera tävlingsdata");
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "Entries", "Anmälningar",
                  CompetitionCB, "",  false, false);
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "FreeImport", "Fri anmälningsimport",
                  CompetitionCB, "", false, false);
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "Courses", "Banor",
                  CompetitionCB, "", false, false);

    gdi.dropLine();
    gdi.addString("", 1, "Exportera tävlingsdata");
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "Startlist", "Startlista",
                  CompetitionCB, "Exportera startlista på fil", false, false);
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "Splits", "Resultat && sträcktider",
                  CompetitionCB, "Exportera resultat på fil", false, false);

    gdi.dropLine();
    gdi.addString("", 1, "Funktioner");
    if (oe->useRunnerDb()) {
      gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "RunnerDatabase", "Löpardatabasen",
                    CompetitionCB, "Visa och hantera löpardatabasen", false, false);
    }

    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::SeveralStages)) {
      gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "MultiEvent", "Hantera flera etapper",
                    CompetitionCB, "", false, false);
    }
    gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "SaveAs", "Säkerhetskopiera",
                  CompetitionCB, "", false, false);
    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Network)) {
      gdi.addButton(gdi.getCX(), gdi.getCY(), bw, "ConnectMySQL", "Databasanslutning",
                    CompetitionCB, "", false, false);
    }
    rc.bottom = gdi.getCY() + gdi.scaleLength(30);
    rc.right = rc.left + bw + gdi.scaleLength(60);

    gdi.addRectangle(rc, colorLightBlue);

    gdi.popX();

    gdi.dropLine(3);
    copyrightLine(gdi);

    gdi.setOnClearCb(CompetitionCB);
  }
  gdi.refresh();
  return true;
}

void TabCompetition::textSizeControl(gdioutput &gdi) const
{
  gdi.dropLine();
  int s = oe->getPropertyInt("TextSize", 0);
  const char *id="TextSize";
  gdi.fillRight();
  RECT rc;
  int x = gdi.getCX() + gdi.scaleLength(15);
  gdi.dropLine(-0.5);
  rc.top = gdi.getCY() - gdi.scaleLength(10);
  rc.left = gdi.getCX();

  gdi.setCX(x);

  gdi.addString("", 1, "Programinställningar");
  gdi.dropLine(2);
  gdi.setCX(x);
  //gdi.addString("", 0, "Textstorlek:");

  gdi.addSelection(id, 90, 200, CompetitionCB, L"Textstorlek:");
  gdi.addItem(id, lang.tl("Normal"), 0);
  gdi.addItem(id, lang.tl("Stor"), 1);
  gdi.addItem(id, lang.tl("Större"), 2);
  gdi.addItem(id, lang.tl("Störst"), 3);
  gdi.selectItemByData(id, s);

  id = "Language";
  gdi.addSelection(id, 150, 300, CompetitionCB, L"Språk:");
  vector<wstring> ln = lang.get().getLangResource();
  wstring current = oe->getPropertyString("Language", L"Svenska");
  int ix = -1;
  for (size_t k = 0; k<ln.size(); k++) {
    gdi.addItem(id, ln[k], k);
    if (ln[k] == current)
      ix = k;
  }
  gdi.selectItemByData(id, ix);

  if (oe->empty()) {
    gdi.setCX(gdi.getCX()+gdi.getLineHeight()*2);
    gdi.dropLine();
    gdi.addButton("Setup", "Inställningar...", CompetitionCB);

    rc.right = gdi.getCX() + gdi.scaleLength(15);

    gdi.setCX(x);
    gdi.dropLine(3);

    gdi.addCheckbox("UseEventor", "Använd Eventor", CompetitionCB,
          useEventor(), "eventor:help");

    rc.bottom = gdi.getCY() + gdi.scaleLength(25);
  }
  else {
    rc.right = gdi.getWidth();//gdi.getCX() + gdi.scaleLength(10);
    rc.bottom = gdi.getCY() + gdi.scaleLength(50);
  }

  gdi.addRectangle(rc, colorLightYellow);
  gdi.dropLine();
}

int TabCompetition::getOrganizer(bool updateEvent) {
  wstring apikey = oe->getPropertyStringDecrypt("apikey", "");
  if (apikey.empty())
    return 0;
  if (!isAscii(apikey))
    return 0;

  Download dwl;
  dwl.initInternet();
  vector< pair<wstring, wstring> > key;
  wstring file = getTempFile();
  key.push_back(pair<wstring, wstring>(L"ApiKey", apikey));
  wstring url = eventorBase + L"organisation/apiKey";
  try {
    dwl.downloadFile(url, file, key);
  }
  catch (dwException &ex) {
    if (ex.code == 403)
      return 0;
    else {
      throw std::exception("Kunde inte ansluta till Eventor.");
    }
  }
  catch (std::exception &) {
    throw std::exception("Kunde inte ansluta till Eventor.");
  }

  dwl.createDownloadThread();
  while (dwl.isWorking()) {
    Sleep(50);
  }

  int clubId = 0;

  xmlparser xml;
  xmlList xmlEvents;
  try {
    xml.read(file.c_str());
    xmlobject obj = xml.getObject("Organisation");
    if (obj) {
      clubId = obj.getObjectInt("OrganisationId");
      obj.getObjectString("Name", eventor.name);

      xmlobject ads = obj.getObject("Address");
      if (ads) {
        ads.getObjectString("careOf", eventor.careOf);
        ads.getObjectString("street", eventor.street);
        ads.getObjectString("city", eventor.city);
        ads.getObjectString("zipCode", eventor.zipCode);
      }

      xmlobject tele = obj.getObject("Tele");

      if (tele) {
        tele.getObjectString("mailAddress", eventor.email);
      }

      xmlobject aco = obj.getObject("Account");
      if (aco) {
        aco.getObjectString("AccountNo", eventor.account);
      }
    }
  }
  catch (std::exception &) {
    removeTempFile(file);
    throw;
  }

  removeTempFile(file);

  return clubId;
}

void TabCompetition::getAPIKey(vector< pair<wstring, wstring> > &key) const {
  wstring apikey = oe->getPropertyStringDecrypt("apikey", "");

  if (apikey.empty() || organizorId == 0)
    throw std::exception("Internal error");

  key.clear();
  key.push_back(pair<wstring, wstring>(L"ApiKey", apikey));
}

void TabCompetition::getEventorCompetitions(gdioutput &gdi,
                                            const wstring &fromDate,
                                            vector<CompetitionInfo> &events) const
{
  events.clear();

  vector< pair<wstring, wstring> > key;
  getAPIKey(key);

  wstring file = getTempFile();
  wstring url = eventorBase + L"events?fromDate=" + fromDate +
              L"&organisationIds=" + itow(organizorId) + L"&includeEntryBreaks=true";
  Download dwl;
  dwl.initInternet();

  try {
    dwl.downloadFile(url, file, key);
  }
  catch (std::exception &) {
    removeTempFile(file);
    throw;
  }

  dwl.createDownloadThread();
  while (dwl.isWorking()) {
    Sleep(100);
  }
  xmlparser xml;
  xmlList xmlEvents;

  try {
    xml.read(file.c_str());
    xmlobject obj = xml.getObject("EventList");
    obj.getObjects("Event", xmlEvents);
  }
  catch (std::exception &) {
    removeTempFile(file);
    throw;
  }

  removeTempFile(file);

  for (size_t k = 0; k < xmlEvents.size(); k++) {
    CompetitionInfo ci;
    xmlEvents[k].getObjectString("Name", ci.Name);
    ci.Id = xmlEvents[k].getObjectInt("EventId");
    xmlobject date = xmlEvents[k].getObject("StartDate");
    date.getObjectString("Date", ci.Date);
    if (date.getObject("Clock"))
      date.getObjectString("Clock", ci.firstStart);

    if (useEventorUTC()) {
      int offset = getTimeZoneInfo(ci.Date);
      int t = convertAbsoluteTimeISO(ci.firstStart);
      int nt = t - offset;
      int dayOffset = 0;
      if (nt < 0) {
        nt += 24*3600;
        dayOffset = -1;
      }
      else if (nt > 24*3600) {
        nt -= 24*3600;
        dayOffset = 1;
      }
      ci.firstStart = formatTimeHMS(nt);
      //TODO: Take dayoffset into account
    }

    xmlEvents[k].getObjectString("WebURL", ci.url);
    xmlobject aco = xmlEvents[k].getObject("Account");
    if (aco) {
      string type = aco.getAttrib("type").get();
      wstring no;
      aco.getObjectString("AccountNo", no);

      if (type == "bankGiro")
        ci.account = L"BG " + no;
      else if (type == "postalGiro")
        ci.account = L"PG " + no;
      else
        ci.account = no;
    }

    ci.lastNormalEntryDate = L"";
    xmlList entryBreaks;
    xmlEvents[k].getObjects("EntryBreak", entryBreaks);
    /* Mats Troeng explains Entry Break 2011-04-03:
    Efteranmälan i detta fall är satt som en tilläggsavgift (+50%) på ordinarie avgift.
    Tilläggsavgiften är aktiv 2011-04-13 -- 2011-04-20, medan den ordinarie avgiften är aktiv -- 2011-04-20. Man kan också
    definiera enligt ditt andra exempel i Eventor om man vill, men då måste man sätta ett fixt belopp i stället för en
    procentsats för efteranmälan eftersom det inte finns något belopp att beräkna procentsatsen på.

    För att få ut anmälningsstoppen för en tävling tittar man alltså på unionen av alla (ValidFromDate - 1 sekund)
    samt ValidToDate. I normalfallet är det två stycken, varav det första är ordinarie anmälningsstopp.
    För t ex O-Ringen som har flera anmälningsstopp blir det mer än två EntryBreaks.
    */
    for (size_t k = 0; k<entryBreaks.size(); k++) {
      xmlobject eBreak = entryBreaks[k].getObject("ValidFromDate");
      if (eBreak) {
        wstring breakDate;
        eBreak.getObjectString("Date", breakDate);

        SYSTEMTIME st;
        convertDateYMS(breakDate, st, false);
        __int64 time = SystemTimeToInt64Second(st) - 1;
        breakDate = convertSystemDate(Int64SecondToSystemTime(time));

        if (ci.lastNormalEntryDate.empty() || ci.lastNormalEntryDate >= breakDate)
          ci.lastNormalEntryDate = breakDate;
      }

      eBreak = entryBreaks[k].getObject("ValidToDate");
      if (eBreak) {
        wstring breakDate;
        eBreak.getObjectString("Date", breakDate);
        if (ci.lastNormalEntryDate.empty() || ci.lastNormalEntryDate >= breakDate)
          ci.lastNormalEntryDate = breakDate;

      }
    }

    events.push_back(ci);
  }
}

void TabCompetition::getEventorCmpData(gdioutput &gdi, int id,
                                       const wstring &eventFile,
                                       const wstring &clubFile,
                                       const wstring &classFile,
                                       const wstring &entryFile,
                                       const wstring &dbFile) const
{
  ProgressWindow pw(gdi.getHWNDTarget());
  pw.init();
  gdi.fillDown();
  gdi.addString("", 1, "Ansluter till Internet").setColor(colorGreen);
  gdi.dropLine(0.5);
  gdi.refreshFast();
  Download dwl;
  dwl.initInternet();

  pw.setProgress(1);
  vector< pair<wstring, wstring> > key;
  wstring apikey = oe->getPropertyStringDecrypt("apikey", "");
  key.push_back(pair<wstring, wstring>(L"ApiKey", apikey));

  gdi.fillRight();

  int prg = 0;
  int event_prg = dbFile.empty() ? 1000 / 4 : 1000/6;
  int club_prg = event_prg;

  if (id > 0) {
    gdi.addString("", 0, "Hämtar tävling...");
    gdi.refreshFast();
    dwl.downloadFile(eventorBase + L"export/event?eventId=" + itow(id) + iofExportVersion, eventFile, key);
    dwl.createDownloadThread();
    while (dwl.isWorking()) {
      Sleep(100);
    }
    if (!dwl.successful())
      throw std::exception("Download failed");

    prg += int(event_prg * 0.2);
    pw.setProgress(prg);
    gdi.addString("", 0, "OK");
    gdi.popX();
    gdi.dropLine();

    gdi.addString("", 0, "Hämtar klasser...");
    gdi.refreshFast();
    dwl.downloadFile(eventorBase + L"export/classes?eventId=" + itow(id) + iofExportVersion, classFile, key);
    dwl.createDownloadThread();
    while (dwl.isWorking()) {
      Sleep(100);
    }

    if (!dwl.successful())
      throw std::exception("Download failed");

    prg += event_prg;
    pw.setProgress(prg);
    gdi.addString("", 0, "OK");
    gdi.popX();
    gdi.dropLine();


    gdi.addString("", 0, "Hämtar anmälda...");
    gdi.refreshFast();
    dwl.downloadFile(eventorBase + L"export/entries?eventId=" + itow(id) + iofExportVersion, entryFile, key);
    dwl.createDownloadThread();
    while (dwl.isWorking()) {
      Sleep(100);
    }
    if (!dwl.successful())
      throw std::exception("Download failed");

    prg += int(event_prg * 1.8);
    pw.setProgress(prg);
    gdi.addString("", 0, "OK");
    gdi.popX();
    gdi.dropLine();
  }


  gdi.addString("", 0, "Hämtar klubbar...");
  gdi.refreshFast();
  dwl.downloadFile(eventorBase + L"export/clubs?" + iofExportVersion, clubFile, key);
  dwl.createDownloadThread();
  while (dwl.isWorking()) {
    Sleep(100);
  }
  if (!dwl.successful())
    throw std::exception("Download failed");

  prg += club_prg;
  pw.setProgress(prg);
  gdi.addString("", 0, "OK");
  gdi.popX();
  gdi.dropLine();

  if (dbFile.length() > 0) {
    gdi.addString("", 0, "Hämtar löpardatabasen...");
    gdi.refreshFast();
    dwl.downloadFile(eventorBase + L"export/cachedcompetitors?organisationIds=1&includePreselectedClasses=false&zip=true" + iofExportVersion, dbFile, key);
    dwl.createDownloadThread();
    while (dwl.isWorking()) {
      Sleep(100);
    }

    if (!dwl.successful())
      throw std::exception("Download failed");

    pw.setProgress(1000);
    gdi.addString("", 0, "OK");
  }

  gdi.popX();
  gdi.dropLine();
}

void TabCompetition::saveMultiEvent(gdioutput &gdi) {
  ListBoxInfo lbiPre, lbiPost;

  gdi.getSelectedItem("PreEvent", lbiPre);
  gdi.getSelectedItem("PostEvent", lbiPost);

  int idPost = lbiPost.data;
  int idPre = lbiPre.data;

  wstring nameIdPost = oe->getNameId(idPost);
  wstring nameIdPre = oe->getNameId(idPre);
  wstring nameId = oe->getNameId(0);
  if (nameIdPost == nameId || nameIdPre == nameId || (nameIdPost == nameIdPre && !nameIdPost.empty()))
    throw meosException("Ogiltig föregående/efterföljande etapp.");

  if (idPost == -2)
    oe->getDI().setString("PostEvent", L"");
  else if (!nameIdPost.empty())
    oe->getDI().setString("PostEvent", nameIdPost);

  if (idPre == -2)
    oe->getDI().setString("PreEvent", L"");
  else if (!nameIdPre.empty())
    oe->getDI().setString("PreEvent", nameIdPre);
}

void TabCompetition::loadMultiEvent(gdioutput &gdi) {
  if (oe->isClient()) {
    throw meosException("info:multieventnetwork");
  }

  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Hantera flera etapper");

  gdi.setRestorePoint("MultiHeader");
  gdi.dropLine();

  gdi.pushX();
  gdi.fillRight();

  wstring preEvent = oe->getDCI().getString("PreEvent");
  wstring postEvent = oe->getDCI().getString("PostEvent");

  gdi.addSelection("PreEvent", 300, 200, CompetitionCB, L"Föregående etapp:", L"Välj den etapp som föregår denna tävling");
  wchar_t bf[260];
  getUserFile(bf, L"");
  oe->enumerateCompetitions(bf, L"*.meos");

  oe->fillCompetitions(gdi, "PreEvent", 1, preEvent);
  gdi.addItem("PreEvent", lang.tl("Ingen / okänd"), -2);
  bool hasPre = !gdi.getText("PreEvent").empty();
  if (!hasPre)
    gdi.selectItemByData("PreEvent", -2);

  gdi.addSelection("PostEvent", 300, 200, CompetitionCB, L"Nästa etapp:", L"Välj den etapp som kommer efter denna tävling");
  oe->fillCompetitions(gdi, "PostEvent", 1, postEvent);
  gdi.addItem("PostEvent", lang.tl("Ingen / okänd"), -2);
  bool hasPost = !gdi.getText("PostEvent").empty();

  if (!hasPost)
    gdi.selectItemByData("PostEvent", -2);

  gdi.dropLine(5);
  gdi.popX();
  gdi.fillRight();

  int numStages = oe->getNumStages();
  gdi.addSelection("StageNumber", 100, 200, CompetitionCB, L"Denna etapps nummer:");
  gdi.addItem("StageNumber", lang.tl("Inget nummer"), -2);
  for (int k = 1; k <= 52; k++)
    gdi.addItem("StageNumber", lang.tl("Etapp X#" + itos(k)), k);
  int sn = oe->getStageNumber();
  if (sn>=1 && sn <= 52) {
    gdi.selectItemByData("StageNumber", sn);
    if (oe->hasNextStage())
      numStages = max(numStages, sn+1);
    else
      numStages = max(numStages, sn);

    oe->setNumStages(numStages);
    oe->synchronize(true);
  }
  else
    gdi.selectFirstItem("StageNumber");

  gdi.fillDown();
  gdi.addInput("NumStages", numStages > 0 ? itow(numStages) : _EmptyWString, 4, CompetitionCB, L"Totalt antal etapper:");

  gdi.fillRight();
  gdi.dropLine(2);
  gdi.addButton("OpenPre", "Öppna föregående", CompetitionCB, "Öppna nästa etapp");
  gdi.addButton("OpenPost", "Öppna nästa", CompetitionCB, "Öppna föregående etapp");

  gdi.dropLine(3);
  gdi.popX();

  gdi.addButton("SaveMulti", "Spara", CompetitionCB);
  gdi.addButton("CloneCmp", "Lägg till ny etapp...", CompetitionCB);
  gdi.addButton("TransferData", "Överför resultat till nästa etapp", CompetitionCB);
  gdi.addButton("Cancel", "Återgå", CompetitionCB);

  gdi.setInputStatus("OpenPre", hasPre);
  gdi.setInputStatus("OpenPost", hasPost);
  gdi.setInputStatus("TransferData", hasPost);
  gdi.setInputStatus("CloneCmp", !hasPost);

  gdi.refresh();
}

void TabCompetition::loadRunnerDB(gdioutput &gdi, int tableToShow, bool updateTableOnly) {
  if (!updateTableOnly) {
    gdi.clearPage(false);
    gdi.addString("", boldLarge, "Löpardatabasen");

    gdi.setRestorePoint("DBHeader");
  }
  else {
    gdi.restore("DBHeader", false);
  }
  gdi.dropLine();
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("RunnerDB", "Personer", CompetitionCB, "Visa löpardatabasen");
  gdi.addButton("ClubDB", "Klubbar", CompetitionCB, "Visa klubbdatabasen");
  gdi.addButton("DBaseIn", "Importera", CompetitionCB, "Importera IOF (xml)");
  if (useEventor())
    gdi.addButton("EventorUpdateDB", "Uppdatera", CompetitionCB, "Uppdatera från Eventor");
  gdi.addButton("ExportSetup", "Exportera", CompetitionCB, "Exportera på fil");
  gdi.addButton("Cancel", "Återgå", CompetitionCB);

  gdi.dropLine(3);
  gdi.popX();

  //if (tableToShow != 0) {
    gdi.fillRight();
    gdi.addButton("ExportRunnerDB", "Exportera personer (IOF-XML)", CompetitionCB);
    gdi.addButton("ExportClubDB", "Exportera klubbar (IOF-XML)", CompetitionCB);
    gdi.addButton("ClearDB", "Töm databasen", CompetitionCB);
    gdi.dropLine(3);
    gdi.popX();

    if (oe->isClient()) {
      gdi.fillDown();
      gdi.addString("", 10, "info:runnerdbonline");
      gdi.dropLine();
      //gdi.disableInput("ExportRunnerDB");
      //gdi.disableInput("ExportClubDB");
      gdi.disableInput("ClearDB");
    }
  //}

  if (tableToShow == 1) {
    oe->updateRunnerDatabase();
    Table *tb = oe->getRunnerDatabase().getRunnerTB();
    gdi.addTable(tb, 40, gdi.getCY());
    gdi.registerEvent("CellAction", CompetitionCB);
  }
  else if (tableToShow == 2) {
    oe->updateRunnerDatabase();
    Table *tb = oe->getRunnerDatabase().getClubTB();
    gdi.addTable(tb, 40,  gdi.getCY());
  }

  gdi.refresh();
}

void TabCompetition::welcomeToMeOS(gdioutput &gdi) {
  gdi.clearPage(false, false);
  gdi.scaleSize(1.8/gdi.getScale());
  gdi.dropLine(5);
  gdi.setCX(gdi.getCX() + 5*gdi.getLineHeight());

  gdi.addString("", 2, "Välkommen till MeOS");
  gdi.addString("", 1, makeDash(L"#- ")+ lang.tl("ett Mycket Enkelt OrienteringsSystem")).setColor(colorDarkBlue);
  gdi.dropLine();
  gdi.addString("", 0, getMeosFullVersion());
  gdi.dropLine(2);
  gdi.addStringUT(0, "Välj språk / Preferred language / Sprache");
  gdi.dropLine();
  gdi.fillRight();
  const char *id = "Language";
  gdi.addSelection(id, 90, 200, CompetitionCB);
  vector<wstring> ln = lang.get().getLangResource();
  wstring current = oe->getPropertyString("Language", L"Svenska");
  int ix = -1;
  for (size_t k = 0; k<ln.size(); k++) {
    gdi.addItem(id, ln[k], k);
    if (ln[k] == current)
      ix = k;
  }
  gdi.selectItemByData(id, ix);

  gdi.addButton("WelcomeOK", "OK", CompetitionCB);
  gdi.dropLine(8);
  gdi.updatePos(gdi.getWidth(), gdi.getCX(), 5*gdi.getLineHeight(), 0);
  gdi.refresh();
}

void TabCompetition::displayRunners(gdioutput &gdi, const vector<pRunner> &changedClass) const {
  for (size_t k = 0; k<changedClass.size(); k++) {
    gdi.addStringUT(0, changedClass[k]->getName() + L" (" + changedClass[k]->getClass(true) + L", " +
                       changedClass[k]->getStartTimeS() + L")");
  }
}

void TabCompetition::selectTransferClasses(gdioutput &gdi, bool expand) {
  gdi.restore("SelectTClass", false);
  gdi.setRestorePoint("SelectTClass");

  gdi.fillDown();
  gdi.addSelection("ChangeClassType", 300, 400, 0, L"Hantera deltagare som bytt klass:");
  gdi.addItem("ChangeClassType", lang.tl("Byt till vakansplats i rätt klass (om möjligt)"), oEvent::ChangeClassVacant);
  gdi.addItem("ChangeClassType", lang.tl("Byt till rätt klass (behåll eventuell starttid)"), oEvent::ChangeClass);
  gdi.addItem("ChangeClassType", lang.tl("Tillåt ny klass, inget totalresultat"), oEvent::TransferNoResult);
  gdi.addItem("ChangeClassType", lang.tl("Tillåt ny klass, behåll resultat från annan klass"), oEvent::TransferAnyway);
  gdi.selectItemByData("ChangeClassType", lastChangeClassType);
  gdi.autoGrow("ChangeClassType");

  if (expand) {
    gdi.fillDown();
    gdi.addListBox("ClassNewEntries", 200, 400, 0, L"Klasser där nyanmälningar ska överföras:", L"", true);
    oe->fillClasses(gdi, "ClassNewEntries", oEvent::extraNone, oEvent::filterNone);

    gdi.setSelection("ClassNewEntries", allTransfer);
    gdi.pushX();
    gdi.fillRight();
    gdi.addButton("SelectAll", "Välj allt", CompetitionCB);
    gdi.fillDown();
    gdi.addButton("SelectNone", "Välj inget", CompetitionCB);
    gdi.popX();
    gdi.addCheckbox("TransferEconomy", "Överför nya deltagare i ej valda klasser med status \"deltar ej\"");
    gdi.fillRight();
  }
  else {
    gdi.fillRight();
    gdi.addButton("ExpandTResults", "Välj klasser med nya anmälningar", CompetitionCB);
  }

  gdi.addButton("DoTransferData", "Överför resultat", CompetitionCB);
  gdi.addButton("MultiEvent", "Återgå", CompetitionCB);
  gdi.popX();
  gdi.dropLine();
  gdi.refresh();
}

static int ClearFeaturesCB(gdioutput *gdi, int type, void *data)
{
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));
  tc.saveMeosFeatures(*gdi, true);
  return 1;
}

static int CheckFeaturesCB(gdioutput *gdi, int type, void *data)
{
  TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi->getTabs().get(TCmpTab));
  tc.saveMeosFeatures(*gdi, false);
  tc.updateFeatureStatus(*gdi);
  return 0;
}

void TabCompetition::meosFeatures(gdioutput &gdi, bool newGuide) {
  if (!newGuide) {
    oe->checkNecessaryFeatures();
    gdi.clearPage(false);
    gdi.addString("", boldLarge, makeDash(L"MeOS - Funktioner"));
  }
  else {
    gdi.dropLine();
    gdi.addString("", fontMediumPlus, makeDash(L"MeOS - Funktioner"));
  }
  gdi.dropLine(0.5);

  const MeOSFeatures &mf = oe->getMeOSFeatures();
  int yp = gdi.getCY();
  int tx, ty;
  gdi.getTargetDimension(tx, ty);
  ty = max(ty-gdi.scaleLength(150), 300);
  int nf = mf.getNumFeatures();
  int maxLen = gdi.scaleLength(150);
  for (int k = 0; k < nf; k++) {
    if (mf.isHead(k)) {
      if (gdi.getCY() > ty) {
        //gdi.newColumn();
        gdi.setCX(gdi.getCX() + maxLen + gdi.scaleLength(10));
        maxLen = gdi.scaleLength(150);
        gdi.setCY(yp);
      }
      gdi.dropLine(0.6);
      TextInfo &ti = gdi.addString("", fontMediumPlus, mf.getHead(k));
      maxLen = max<int>(maxLen, ti.textRect.right - ti.textRect.left);
      gdi.dropLine(0.4);
    }
    else {
      MeOSFeatures::Feature f = mf.getFeature(k);
      ButtonInfo &bi = gdi.addCheckbox("feat" + gdi.narrow(mf.getCode(f)), mf.getDescription(f),
                                        CheckFeaturesCB, mf.hasFeature(f));
      maxLen = max<int>(maxLen, bi.width);

      if (mf.isRequired(f, *oe))
        gdi.setInputStatus("feat" + gdi.narrow(mf.getCode(f)), false);
    }
  }

  gdi.dropLine();

  if (!newGuide) {
    gdi.fillRight();
    gdi.addButton("SaveFeaures", "Spara", CompetitionCB).setDefault();
    gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();
    gdi.setOnClearCb(ClearFeaturesCB);

    gdi.refresh();
  }

}

void TabCompetition::updateFeatureStatus(gdioutput &gdi) {
  const MeOSFeatures &mf = oe->getMeOSFeatures();
  int nf = mf.getNumFeatures();
  for (int k = 0; k < nf; k++) {
    if (!mf.isHead(k)) {
      MeOSFeatures::Feature f = mf.getFeature(k);
      string id = "feat" + gdi.narrow(mf.getCode(f));
      gdi.check(id, mf.hasFeature(f));
      gdi.setInputStatus(id, !mf.isRequired(f, *oe));
    }
  }
  gdi.refresh();
}


void TabCompetition::saveMeosFeatures(gdioutput &gdi, bool write) {
  MeOSFeatures &mf = oe->getMeOSFeatures();

  int nf = mf.getNumFeatures();
  for (int k = 0; k < nf; k++) {
    if (!mf.isHead(k)) {
      MeOSFeatures::Feature f = mf.getFeature(k);
      string key = "feat" + gdi.narrow(mf.getCode(f));
      mf.useFeature(f, gdi.isChecked(key), *oe);
    }
  }
  if (write) {
    oe->getDI().setString("Features", mf.serialize());
    oe->synchronize(true);
  }
}

void TabCompetition::entryForm(gdioutput &gdi, bool isGuide) {
  if (isGuide) {
    gdi.dropLine(1);
    gdi.addString("", fontMediumPlus, "Importera tävlingsdata");
  }
  else
    gdi.addString("", 2, "Importera tävlingsdata");

  gdi.dropLine(0.5);
  gdi.addString("", 10, "help:import_entry_data");
  gdi.dropLine();

  gdi.pushX();

  gdi.fillRight();
  gdi.addInput("FileNameCmp", L"", 48, 0, L"Tävlingsinställningar (IOF, xml)");
  gdi.dropLine();
  gdi.addButton("BrowseEntries", "Bläddra...", CompetitionCB).setExtra(L"FileNameCmp");
  gdi.popX();

  gdi.dropLine(2.5);
  gdi.addInput("FileNameCls", L"", 48, 0, L"Klasser (IOF, xml)");
  gdi.dropLine();
  gdi.addButton("BrowseEntries", "Bläddra...", CompetitionCB).setExtra(L"FileNameCls");
  gdi.popX();

  gdi.dropLine(2.5);
  gdi.addInput("FileNameClb", L"", 48, 0, L"Klubbar (IOF, xml)");
  gdi.dropLine();
  gdi.addButton("BrowseEntries", "Bläddra...", CompetitionCB).setExtra(L"FileNameClb");
  gdi.popX();

  gdi.dropLine(2.5);
  gdi.addInput("FileName", L"", 48, 0, L"Anmälningar (IOF (xml) eller OE-CSV)");
  gdi.dropLine();
  gdi.addButton("BrowseEntries", "Bläddra...", CompetitionCB).setExtra(L"FileName");
  
  gdi.popX();
  gdi.dropLine(3.2);

  if (!isGuide && oe->getNumRunners() > 0) {
    gdi.addCheckbox("RemoveRemoved", "Ta bort eventuella avanmälda deltagare", 0, true);
  }
  gdi.popX();

  gdi.dropLine(2.5);
  gdi.addInput("FileNameRank", L"", 48, 0, L"Ranking (IOF, xml, csv)");
  gdi.dropLine();
  gdi.addButton("BrowseEntries", "Bläddra...", CompetitionCB).setExtra(L"FileNameRank");
  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(3);
}

TabCompetition::FlowOperation TabCompetition::saveEntries(gdioutput &gdi, bool removeRemoved, bool isGuide) {
  wstring filename[5];
  filename[0] = gdi.getText("FileNameCmp");
  filename[1] = gdi.getText("FileNameCls");
  filename[2] = gdi.getText("FileNameClb");
  filename[3] = gdi.getText("FileName");
  filename[4] = gdi.getText("FileNameRank");

  //csvparser csv;

  for (int i = 0; i<5; i++) {
    if (filename[i].empty())
      continue;

    gdi.addString("", 0, L"Behandlar: X#" + filename[i]);

    csvparser::CSV type = csvparser::iscsv(filename[i]);
    if (i == 4 && (type == csvparser::CSV::OE || type == csvparser::CSV::Unknown)) {
      // Ranking
      const wchar_t *File = filename[i].c_str();

      gdi.addString("", 0, "Importerar ranking...");
      gdi.refresh();
      gdi.setWaitCursor(true);
      vector<wstring> problems;
      csvparser csv;
      int count = csv.importRanking(*oe, File, problems);
      if (count > 0) {
        gdi.addString("", 0, "Klart. X värden tilldelade.#" + itos(count));
        if (!problems.empty()) {
          gdi.dropLine();
          gdi.addString("", 0, "Varning: Följande deltagare har ett osäkert resultat:");
          for (auto &p : problems)
            gdi.addStringUT(0, p).setColor(colorDarkRed);
        }
      }
      else gdi.addString("", 0, "Försöket misslyckades.");
    }
    else if (type != csvparser::CSV::NoCSV) {
      const wchar_t *File = filename[i].c_str();
      csvparser csv;
      if (type == csvparser::CSV::OE) {
        gdi.addString("", 0, "Importerar OE2003 csv-fil...");
        gdi.refresh();
        gdi.setWaitCursor(true);
        if (csv.importOE_CSV(*oe, File)) {
          gdi.addString("", 0, "Klart. X deltagare importerade.#" + itos(csv.nimport));
        }
        else gdi.addString("", 0, "Försöket misslyckades.");
      }
      else if (type == csvparser::CSV::OS) {
        gdi.addString("", 0, "Importerar OS2003 csv-fil...");
        gdi.refresh();
        gdi.setWaitCursor(true);
        if (csv.importOS_CSV(*oe, File)) {
          gdi.addString("", 0, "Klart. X lag importerade.#" + itos(csv.nimport));
        }
        else gdi.addString("", 0, "Försöket misslyckades.");
      }
      else if (type == csvparser::CSV::RAID) {
        gdi.addString("", 0, "Importerar RAID patrull csv-fil...");
        gdi.setWaitCursor(true);
        if (csv.importRAID(*oe, File)) {
          gdi.addString("", 0, "Klart. X patruller importerade.#" + itos(csv.nimport));
        }
        else gdi.addString("", 0, "Försöket misslyckades.");
      }
      else {
        gdi.addString("", 0, "Försöket misslyckades.");
      }
    }
    else {
      set<int> stageFilter;
      string preferredIdType;

      FlowOperation res = checkStageFilter(gdi, filename[i], stageFilter, preferredIdType);

      if (res != FlowContinue)
        return res;

      oe->importXML_EntryData(gdi, filename[i], false, removeRemoved, stageFilter, preferredIdType);
    }
    if (!isGuide) {
      gdi.setWindowTitle(oe->getTitleName());
      oe->updateTabs();
    }
  }

  set<int> clsWithRef;
  oTeam::checkClassesWithReferences(*oe, clsWithRef);
  if (!clsWithRef.empty()) {
    if (gdi.ask(L"ask:convert_to_patrol")) {
      oTeam::convertClassWithReferenceToPatrol(*oe, clsWithRef);
    }
  }

  // Update qualification/final
  vector<pClass> cls;
  oe->getClasses(cls, false);
  for (pClass c : cls) {
    c->updateFinalClasses(0, false);
  }
  return FlowContinue;
}

int stageInfoCB(gdioutput *gdi, int type, void *data)
{
  if (type == GUI_BUTTON) {
    ButtonInfo &bi = *(ButtonInfo *)data;
    bi.setExtra(1);
  }
  return 0;
}

void mainMessageLoop(HACCEL hAccelTable, DWORD time);

TabCompetition::FlowOperation TabCompetition::checkStageFilter(gdioutput & gdi,
                                                               const wstring & fname, 
                                                               set<int>& filter,
                                                               string &preferredIdProvider) {
  xmlparser xml;
  xml.read(fname);
  xmlobject xo = xml.getObject("EntryList");
  set<int> scanFilter;
  IOF30Interface reader(oe, false);
  vector<string> idProviders;
  if (xo) {
    if (xo.getAttrib("iofVersion")) {
      reader.prescanEntryList(xo, scanFilter);
      reader.getIdTypes(idProviders);
    }
  }
  bool stageFilter = scanFilter.size() > 1;
  bool idtype = idProviders.size() > 1;

  bool needUseInput = stageFilter || idtype;

  if (needUseInput) {
    gdi.enableEditControls(false, true);
    gdi.fillDown();
    gdi.pushX();
  }

  if (stageFilter) {
    gdi.dropLine(0.5);
    gdi.addString("", 0, "Det finns anmälningsdata för flera etapper.");
    gdi.dropLine(0.5);
    gdi.fillRight();
    gdi.addSelection("Stage", 150, 200, stageInfoCB, L"Välj etapp att importera:");
    gdi.addItem("Stage", lang.tl("Alla"), 0);
    for (int sn : scanFilter) {
      if (sn > 0)
        gdi.addItem("Stage", lang.tl("Etapp X#" + itos(sn)), sn);
    }
    int cn = oe->getStageNumber();
    if (cn > 0 && scanFilter.count(cn))
      gdi.selectItemByData("Stage", cn);
    else
      gdi.selectItemByData("Stage", 0);
  }

  if (idtype) {
    if (stageFilter) {
      gdi.popX();
      gdi.dropLine(2);
    }
    gdi.dropLine(0.5);
    gdi.addString("", 0, "Det finns multiplia Id-nummer för personer");
    gdi.dropLine(0.5);
    gdi.fillRight();
    gdi.addSelection("IdType", 150, 200, stageInfoCB, L"Välj vilken typ du vill importera:");
    int i = 0;
    for (string &sn : idProviders) {
      gdi.addItem("IdType", gdi.widen(sn), i++);
    }
  }

  if (needUseInput) {
    gdi.dropLine();
    gdi.addButton("OK_Stage", "OK", stageInfoCB);
    gdi.fillDown();
    gdi.addButton("Cancel_Stage", "Cancel", stageInfoCB);
    gdi.popX();
    gdi.dropLine();
    gdi.scrollToBottom();
    gdi.refresh();
    gdi.runSubCommand();
    while (gdi.hasField("OK_Stage") && gdi.getExtraInt("OK_Stage") == 0 && gdi.getExtraInt("Cancel_Stage") == 0) {
      mainMessageLoop(0, 10);
    }
    bool ok = false, cancel = false;

    if (gdi.hasField("OK_Stage")) {
      ok = gdi.getExtraInt("OK_Stage") != 0;
      cancel = gdi.getExtraInt("Cancel_Stage") != 0;

      gdi.removeControl("OK_Stage");
      gdi.removeControl("Cancel_Stage");
      if (stageFilter)
        gdi.disableInput("Stage");
      if (idtype)
        gdi.disableInput("IdType");
    }
    if (ok) {
      //OK was pressed
      if (scanFilter.size() > 1) {
        int stage = gdi.getSelectedItem("Stage").first;
        if (stage > 0) {
          filter.insert(stage);
          if (oe->getStageNumber() == 0) {
            oe->setStageNumber(stage);
            oe->getMeOSFeatures().useFeature(MeOSFeatures::SeveralStages, true, *oe);
          }
        }
      }

      if (idProviders.size() > 1) {
        ListBoxInfo lbi;
        if (gdi.getSelectedItem("IdType", lbi)) {
          preferredIdProvider = gdi.narrow(lbi.text);
        }
      }

      return FlowContinue;
    }
    else if (cancel)
      return FlowCancel;

    return FlowAborted; // User has started to do something else...
  }

  return FlowContinue;
}

void TabCompetition::selectStartlistOptions(gdioutput &gdi) {
  gdi.clearPage(true);
  gdi.addString("", boldLarge, "Exportera startlista");
  gdi.pushY();
  gdi.addListBox("ClassNewEntries", 250, 400, 0, L"Klassval:", L"", true);
  oe->fillClasses(gdi, "ClassNewEntries", oEvent::extraNone, oEvent::filterNone);

  gdi.setSelection("ClassNewEntries", allTransfer);
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("SelectAll", "Välj allt", CompetitionCB);
  gdi.fillDown();
  gdi.addButton("SelectNone", "Välj inget", CompetitionCB);
  gdi.popX();

  gdi.newColumn();
  gdi.pushX();
  gdi.popY();
  gdi.addSelection("Type", 250, 200, CompetitionCB, L"Exporttyp:");

  vector< pair<wstring, size_t> > types;
  ImportFormats::getExportFormats(types, false);
  gdi.addItem("Type", types);
  ImportFormats::ExportFormats format = ImportFormats::getDefaultExportFormat(*oe);
  gdi.selectItemByData("Type", format);

  vector< pair<wstring, size_t> > typeLanguages;
  ImportFormats::getOECSVLanguage(typeLanguages);
  
  gdi.addSelection("LanguageType", 250, 200, CompetitionCB, L"Export language:");
  gdi.addItem("LanguageType", typeLanguages);

  gdi.selectItemByData("LanguageType", ImportFormats::getDefaultCSVLanguage(*oe));
 
  
  ClassConfigInfo cnf;
  oe->getClassConfigurationInfo(cnf);

  if (oe->hasTeam()) {
    gdi.addCheckbox("ExportTeam", "Exportera individuella lopp istället för lag", 0, false);
  }
  if (oe->hasMultiRunner() || oe->getStageNumber() > 0)
    gdi.addCheckbox("IncludeRaceNumber", "Inkludera information om flera lopp per löpare", 0, true);
    
  setExportOptionsStatus(gdi, format);

  gdi.addInput("Filename", L"", 48, CompetitionCB,  L"Filnamn:").setExtra(L"DoSaveStartlist");
  gdi.fillRight();
  gdi.dropLine();
  gdi.addButton("BrowseExport", "Bläddra...",  CompetitionCB);
  gdi.addButton("DoSaveStartlist", "Exportera",  CompetitionCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();
  gdi.disableInput("DoSaveStartlist");
  gdi.refresh();
}

void TabCompetition::selectExportSplitOptions(gdioutput &gdi) {
  gdi.clearPage(false);
  gdi.addString("", boldLarge, "Export av resultat/sträcktider");
  gdi.dropLine();
  gdi.pushY();
  gdi.addListBox("ClassNewEntries", 250, 400, 0, L"Klassval:", L"", true);
  oe->fillClasses(gdi, "ClassNewEntries", oEvent::extraNone, oEvent::filterNone);

  gdi.setSelection("ClassNewEntries", allTransfer);
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("SelectAll", "Välj allt", CompetitionCB);
  gdi.fillDown();
  gdi.addButton("SelectNone", "Välj inget", CompetitionCB);
  gdi.popX();
  gdi.newColumn();
  gdi.popY();
  gdi.pushX();
  gdi.addSelection("Type", 250, 200, CompetitionCB, L"Exporttyp:");

  vector< pair<wstring, size_t> > types;
  ImportFormats::getExportFormats(types, true);

  gdi.addItem("Type", types);
  ImportFormats::ExportFormats format = ImportFormats::getDefaultExportFormat(*oe);
  gdi.selectItemByData("Type", format);

  vector< pair<wstring, size_t> > typeLanguages;
  ImportFormats::getOECSVLanguage(typeLanguages);
  
  gdi.addSelection("LanguageType", 250, 200, CompetitionCB, L"Export language:");
  gdi.addItem("LanguageType", typeLanguages);
  
  gdi.selectItemByData("LanguageType", ImportFormats::getDefaultCSVLanguage(*oe));
 
  gdi.addCheckbox("ExportSplitTimes", "Export split times", 0, oe->getPropertyInt("ExportCSVSplits", false) != 0);
  
  ClassConfigInfo cnf;
  oe->getClassConfigurationInfo(cnf);

  if (oe->hasTeam()) {
    gdi.addSelection("LegType", 300, 100, 0, L"Exportval, IOF-XML");
    gdi.addItem("LegType", lang.tl("Totalresultat"), 1);
    gdi.addItem("LegType", lang.tl("Alla lopp som individuella"), 3);
    gdi.addItem("LegType", lang.tl("Alla sträckor/lopp i separata filer"), 2);
    int legMax = cnf.getNumLegsTotal();
    for (int k = 0; k<legMax; k++) {
      gdi.addItem("LegType", lang.tl("Sträcka X#" + itos(k+1)), k+10);
    }
    gdi.selectFirstItem("LegType");
  }

  bool hasLoops = false;
  vector<pCourse> crs;
  oe->getCourses(crs);
  for (size_t k = 0; k < crs.size(); k++) {
    if (crs[k]->getCommonControl() != 0)
      hasLoops = true;
  }
  if (hasLoops)
    gdi.addCheckbox("UnrollLoops", "Unroll split times for loop courses", 0, true);

  if (oe->hasMultiRunner() || oe->getStageNumber() > 0)
    gdi.addCheckbox("IncludeRaceNumber", "Inkludera information om flera lopp per löpare", 0, true);

  setExportOptionsStatus(gdi, format);
  gdi.addInput("Filename", L"", 48, CompetitionCB,  L"Filnamn:").setExtra(L"DoSaveSplits");
  gdi.fillRight();
  gdi.dropLine();
  gdi.addButton("BrowseExportResult", "Bläddra...",  CompetitionCB);
  gdi.addButton("DoSaveSplits", "Exportera",  CompetitionCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();

  gdi.disableInput("DoSaveSplits");
  gdi.refresh();
}

void TabCompetition::setExportOptionsStatus(gdioutput &gdi, int format) const {
  if (gdi.hasField("LegType")) {
    gdi.setInputStatus("LegType", format == ImportFormats::IOF30 || format == ImportFormats::IOF203); // Enable on IOF-XML
  }
  if (gdi.hasField("ExportTeam")) {
    gdi.setInputStatus("ExportTeam", format == ImportFormats::IOF30); // Enable on IOF-XML
  }

  if (gdi.hasField("ExportSplitTimes")) {
    gdi.setInputStatus("ExportSplitTimes", format == ImportFormats::OE);
    if (format == ImportFormats::IOF203 || format == ImportFormats::IOF30)
      gdi.check("ExportSplitTimes", true);
  }
  
  if (gdi.hasField("IncludeRaceNumber")) {
    gdi.setInputStatus("IncludeRaceNumber", format == ImportFormats::IOF30); // Enable on IOF-XML
  }

  gdi.setInputStatus("LanguageType", format == ImportFormats::OE);
}

void TabCompetition::clearCompetitionData() {
}

void TabCompetition::loadSettings(gdioutput &gdi) {
  gdi.clearPage(false);

  gdi.addString("", boldLarge, "Tävlingsinställningar");
  gdi.dropLine(0.5);
  vector<string> fields;
  gdi.pushY();
  gdi.addString("", 1, "Adress och kontakt");
  fields.push_back("Organizer");
  fields.push_back("CareOf");
  fields.push_back("Street");
  fields.push_back("Address");
  fields.push_back("EMail");
  fields.push_back("Homepage");

  oe->getDI().buildDataFields(gdi, fields, 32);

  gdi.dropLine(0.3);
  gdi.addString("", 1, "Betalningsinformation");
  fields.clear();
  fields.push_back("Account");
  fields.push_back("PaymentDue");

  oe->getDI().buildDataFields(gdi, fields, 32);

  gdi.addString("", 1, "Tidszon");

  gdi.dropLine(0.3);
  gdi.addCheckbox("UTC", "Exportera tider i UTC", 0,
  oe->getDCI().getInt("UTC") == 1);


  gdi.newColumn();
  gdi.popY();

  gdi.addString("", 1, "Avgifter");
  fields.clear();
  gdi.fillRight();
  gdi.pushX();
  fields.push_back("CardFee");
  fields.push_back("EliteFee");
  fields.push_back("EntryFee");
  fields.push_back("YouthFee");

  oe->getDI().buildDataFields(gdi, fields, 6);

  gdi.popX();
  gdi.dropLine(3);

  fields.clear();
  fields.push_back("OrdinaryEntry");
  fields.push_back("LateEntryFactor");

  oe->getDI().buildDataFields(gdi, fields, 10);

  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);

  gdi.addString("", 1, "Åldersgränser, reducerad anmälningsavgift");
  fields.clear();
  fields.push_back("YouthAge");
  fields.push_back("SeniorAge");
  gdi.fillRight();
  oe->getDI().buildDataFields(gdi, fields, 10);

  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);


  gdi.addString("", 1, "Valuta");
  fields.clear();
  fields.push_back("CurrencySymbol");
  fields.push_back("CurrencyCode");

  gdi.fillRight();
  oe->getDI().buildDataFields(gdi, fields, 10);

  gdi.popX();
  gdi.dropLine(3);
  gdi.addCheckbox("PreSymbol", "Valutasymbol före", 0,
                  oe->getDCI().getInt("CurrencyPreSymbol") == 1);

  gdi.popX();
  gdi.dropLine(2.5);
  bool useFrac = oe->getDCI().getInt("CurrencyFactor") == 100;
  gdi.addCheckbox("UseFraction", "Tillåt decimaler", CompetitionCB,
                    useFrac, "Tillåt valutauttryck med decimaler");

  fields.clear();
  gdi.dropLine(-1);
  fields.push_back("CurrencySeparator");
  oe->getDI().buildDataFields(gdi, fields, 10);

  gdi.setInputStatus("CurrencySeparator_odc", useFrac);

  gdi.fillDown();
  gdi.popX();
  gdi.dropLine(3);

  gdi.fillDown();
  gdi.addString("", 1, "Tävlingsregler");
  fields.clear();
  gdi.fillRight();
  gdi.pushX();
  fields.push_back("MaxTime");
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Rogaining)) {
    fields.push_back("DiffTime");
  }
  gdi.fillDown();
  oe->getDI().buildDataFields(gdi, fields, 10);
  oe->getDI().fillDataFields(gdi);

  gdi.dropLine(1);
  int bottom = gdi.getCY();


  gdi.newColumn();
  gdi.popY();
  gdi.pushX();
  gdi.fillDown();
  gdi.addString("", 1, "Betalningsmetoder"); 
  gdi.dropLine();
  gdi.addString("", 10, "help:paymentmodes");
  gdi.dropLine();
  vector< pair<wstring, size_t> > modes;
  oe->getPayModes(modes);
  for (size_t k = 0; k < modes.size(); k++) {
    gdi.fillRight();
    string ms = itos(modes[k].second);
    gdi.addInput("M" + itos(k), modes[k].first, 24).setExtra(modes[k].second);
    if (k > 0)
      gdi.addButton(gdi.getCX(), gdi.getCY(), gdi.scaleLength(20), 
                    "RemovePayMode", makeDash(L"-"), CompetitionCB, 
                    L"Ta bort", false, false).setExtra(modes[k].second);
    if (k == 0) 
      gdi.addButton(gdi.getCX(), gdi.getCY(), gdi.scaleLength(20), 
                    "AddPayMode", "+", CompetitionCB, 
                    "Lägg till", false, false);

    gdi.dropLine(2.5);
    gdi.popX();
  }
  bottom = max(bottom, gdi.getCY());

  gdi.popX();
  gdi.setCY(bottom);
  gdi.fillRight();
  gdi.addButton("SaveSettings", "Spara", CompetitionCB).setDefault();
  gdi.addButton("Cancel", "Avbryt", CompetitionCB).setCancel();
  gdi.dropLine(2);
  gdi.setOnClearCb(CompetitionCB);
  gdi.refresh();

}

void TabCompetition::saveSettings(gdioutput &gdi) {
  vector<string> fields;
  vector<int> fees(4);
  fields.push_back("CardFee");
  fields.push_back("EliteFee");
  fields.push_back("EntryFee");
  fields.push_back("YouthFee");

  for (int k = 0; k<4; k++)
    fees[k] = oe->getDCI().getInt(fields[k]);
  wstring factor = oe->getDCI().getString("LateEntryFactor");
  oe->getDI().saveDataFields(gdi);

  bool changedFee = false;
  bool changedCardFee = false;

  for (int k = 0; k<4; k++) {
    if (fees[k] != oe->getDCI().getInt(fields[k])) {
      if (k > 0)
        changedFee = true;
      else {
        changedCardFee = true;
        if (oe->getDCI().getInt(fields[k]) == 0)
          oe->getDI().setInt(fields[k].c_str(), -1); // Disallow zero card fee. -1 means no fee.
      }
    }
  }
  if (factor != oe->getDCI().getString("LateEntryFactor"))
    changedFee = true;

  oe->getDI().setInt("UTC", gdi.isChecked("UTC") ? 1 : 0);

  oe->getDI().setInt("CurrencyFactor", gdi.isChecked("UseFraction") ? 100 : 1);
  oe->getDI().setInt("CurrencyPreSymbol", gdi.isChecked("PreSymbol") ? 1 : 0);
  oe->setCurrency(-1, L"", L"", false);

  vector< pair<wstring, size_t> > modes;
  oe->getPayModes(modes);
  for (size_t k = 0; k < modes.size(); k++) {
    string field = "M"+itos(k);
    if (gdi.hasField(field)) {
      wstring mode = gdi.getText("M"+itos(k));
      int id = gdi.getBaseInfo(field.c_str()).getExtraInt();
      oe->setPayMode(id, mode);
    }
  }

  // Read from model
  if (oe->isChanged()) {
    oe->setProperty("Organizer", oe->getDCI().getString("Organizer"));
    oe->setProperty("Street", oe->getDCI().getString("Street"));
    oe->setProperty("Address", oe->getDCI().getString("Address"));
    oe->setProperty("EMail", oe->getDCI().getString("EMail"));
    oe->setProperty("Homepage", oe->getDCI().getString("Homepage"));

    oe->setProperty("CardFee", oe->getDCI().getInt("CardFee"));
    oe->setProperty("EliteFee", oe->getDCI().getInt("EliteFee"));
    oe->setProperty("EntryFee", oe->getDCI().getInt("EntryFee"));
    oe->setProperty("YouthFee", oe->getDCI().getInt("YouthFee"));

    oe->setProperty("YouthAge", oe->getDCI().getInt("YouthAge"));
    oe->setProperty("SeniorAge", oe->getDCI().getInt("SeniorAge"));

    oe->setProperty("Account", oe->getDCI().getString("Account"));
    oe->setProperty("LateEntryFactor", oe->getDCI().getString("LateEntryFactor"));

    oe->setProperty("CurrencySymbol", oe->getDCI().getString("CurrencySymbol"));
    oe->setProperty("CurrencyFactor", oe->getDCI().getInt("CurrencyFactor"));
    oe->setProperty("CurrencyPreSymbol", oe->getDCI().getInt("CurrencyPreSymbol"));
    oe->setProperty("CurrencySeparator", oe->getDCI().getString("CurrencySeparator"));

    oe->setProperty("PayModes", oe->getDCI().getString("PayModes"));
  }
  oe->synchronize(true);
  set<int> dummy;
  if (changedFee && oe->getNumClasses() > 0) {
    bool updateFee = gdi.ask(L"ask:changedcmpfee");

    if (updateFee)
      oe->applyEventFees(true, true, changedCardFee, dummy);
  }
  else if (changedCardFee)
    oe->applyEventFees(false, false, true, dummy);
}

void TabCompetition::checkReadyForResultExport(gdioutput &gdi, const set<int> &classFilter) {
  vector<pRunner> runners;
  oe->getRunners(0, 0, runners, true);
  int numNoResult = 0;
  int numVacant = 0;

  for (pRunner r : runners) {
    if (!classFilter.empty() && !classFilter.count(r->getClassId(false)))
      continue;

    if (r->isVacant())
      numVacant++;
    else if (r->getStatus() == StatusUnknown && !r->needNoCard())
      numNoResult++;
  }

  if (numVacant > 0) {
    if (gdi.ask(L"ask:hasVacant")) {
      if (gdi.ask(L"Vill du radera alla vakanser från tävlingen?")) {
        if (classFilter.empty())
          oe->removeVacanies(0);
        else {
          for (int c : classFilter)
            oe->removeVacanies(c);
        }
      }
    }
  }

  if (numNoResult > 0) {
    gdi.alert(L"warn:missingResult#" + itow(numNoResult));
  }
}

void TabCompetition::checkRentCards(gdioutput &gdi) {  
  gdi.clearPage(false);

  wstring fn = gdi.browseForOpen({ make_pair(L"csv", L"*.csv") }, L"csv");
  if (!fn.empty()) {
    csvparser csv;
    list<vector<wstring>> data;
    csv.parse(fn, data);
    set<int> rentCards;
    for (auto &c : data) {
      if (c.size() > 0) {
        int cn = _wtoi(c[0].c_str());
        rentCards.insert(cn);
      }
    }

    vector<pRunner> runners;
    oe->getRunners(0, 0, runners);
    int bcf = oe->getBaseCardFee();
    for (pRunner r : runners) {
      if (rentCards.count(r->getCardNo()) && r->getDCI().getInt("CardFee") == 0) {
        gdi.addStringUT(0, r->getCompleteIdentification());
        r->getDI().setInt("CardFee", bcf);
      }
    }
  }

  gdi.dropLine();
  gdi.addButton("Cancel", "OK", CompetitionCB);
  gdi.refresh();
}
