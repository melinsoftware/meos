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

#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>
#include <MMSystem.h>

#include <algorithm>

#include "oEvent.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "gdiconstants.h"

#include "csvparser.h"

#include "TabSI.h"
#include "TabAuto.h"
#include "TabList.h"
#include "TabCompetition.h"
#include "meos_util.h"
#include <cassert>
#include "TabRunner.h"
#include "onlineinput.h"
#include "meosexception.h"
#include "MeOSFeatures.h"
#include "RunnerDB.h"
#include "recorder.h"
#include "autocomplete.h"
#include "random.h"
#include "cardsystem.h"
#include <random>

constexpr bool addTestPort = false;
void tabForceSync(gdioutput& gdi, pEvent oe);
wstring getHiredCardDefault();

TabSI::TabSI(oEvent* poe) :TabBase(poe), activeSIC(ConvertedTimeStatus::Unknown) {
  editCardData.tabSI = this;
  directEntryGUI.tabSI = this;

  interactiveReadout = poe->getPropertyInt("Interactive", 1) != 0;
  useDatabase = poe->getPropertyInt("Database", 1) != 0;
  printSplits = false;
  printStartInfo = false;
  savedCardUniqueId = 1;

#ifdef _DEBUG
  showTestingPanel = true;
#endif // _DEBUG

  manualInput = poe->getPropertyInt("ManualInput", 0) == 1;

  mode = SIMode::ModeReadOut;

  modeName[SIMode::ModeReadOut] = "Avläsning/radiotider";
  modeName[SIMode::ModeAssignCards] = "Tilldela hyrbrickor";
  modeName[SIMode::ModeCheckCards] = "Avstämning hyrbrickor";
  modeName[SIMode::ModeRegisterCards] = "Registrera hyrbrickor";
  modeName[SIMode::ModeEntry] = "Anmälningsläge";
  modeName[SIMode::ModeCardData] = "Print card data";
  modeName[SIMode::ModeRequestStartTime] = "Boka starttid";

  currentAssignIndex = 0;

  lastClubId = 0;
  lastClassId = 0;

  minRunnerId = 0;
  inputId = 0;
  printErrorShown = false;
  splitPrinter.onlyChanged = false;
}

TabSI::~TabSI() = default;

static void entryTips(gdioutput& gdi) {
  gdi.fillDown();
  gdi.addString("", 10, "help:21576");
  gdi.dropLine(1);
  gdi.setRestorePoint("EntryLine");
}

void TabSI::logCard(gdioutput& gdi, const SICard& card) {
  if (!logger) {
    logger = make_shared<csvparser>();
    wstring readlog = L"sireadlog_" + getLocalTimeFileName() + L".csv";
    wchar_t file[260];
    wstring subfolder = makeValidFileName(oe->getName(), true);
    const wchar_t* sf = subfolder.empty() ? 0 : subfolder.c_str();
    getDesktopFile(file, readlog.c_str(), sf);
    logger->openOutput(file);
    vector<string> head = SICard::logHeader();
    logger->outputRow(head);
    logcounter = 0;
  }

  vector<string> log = card.codeLogData(gdi, ++logcounter);
  logger->outputRow(log);
}

extern SportIdent* gSI;
extern pEvent gEvent;

int SportIdentCB(gdioutput* gdi, GuiEventType type, BaseInfo * data) {
  TabSI& tsi = dynamic_cast<TabSI&>(*gdi->getTabs().get(TSITab));

  return tsi.siCB(*gdi, type, data);
}

string TabSI::typeFromSndType(SND s) {
  switch (s) {
  case SND::OK:
    return "SoundOK";
  case SND::NotOK:
    return "SoundNotOK";
  case SND::Leader:
    return "SoundLeader";
  case SND::ActionNeeded:
    return "SoundAction";
  default:
    assert(false);
  }
  return "";
}

int TabSI::siCB(gdioutput& gdi, GuiEventType type, BaseInfo * data) {
  if (type == GUI_BUTTON) {
    ButtonInfo bi = *(ButtonInfo*)data;

    if (bi.id == "LockFunction") {
      lockedFunction = true;
      loadPage(gdi);
    }
    else if (bi.id == "UnlockFunction") {
      lockedFunction = false;
      loadPage(gdi);
    }
    else if (bi.id == "ChangeMapping") {
      gdi.restore("Mapping", false);
      gdi.dropLine(4);
      gdi.popX();

      changeMapping(gdi);

      gdi.refresh();
    }
    else if (bi.id == "SaveMapping") {
      int ctrl = gdi.getTextNo("Code");
      if (ctrl < 1 || ctrl >= 1024)
        throw meosException("Ogiltig kontrollkod");
      ListBoxInfo lbi;
      if (!gdi.getSelectedItem("Function", lbi))
        throw meosException("Ogiltig funktion");
      oe->definePunchMapping(ctrl, (oPunch::SpecialPunch)lbi.data);
      fillMappings(gdi);
      gdi.setInputStatus("LoadLastMapping", false);
    }
    else if (bi.id == "RemoveMapping") {
      set<int> sel;
      gdi.getSelection("Mappings", sel);
      for (auto code : sel) 
        oe->definePunchMapping(code, oPunch::SpecialPunch::PunchUnused);
      fillMappings(gdi);
      gdi.setInputStatus("LoadLastMapping", false);
    }
    else if (bi.id == "LoadLastMapping") {
      auto cm = oe->getPropertyString("ControlMap", L"");
      oe->getDI().setString("ControlMap", cm);
      fillMappings(gdi);
      gdi.setInputStatus(bi.id, false);
    }
    else if (bi.id == "CloseMapping") {
      gdi.restore("SIPageLoaded");
      showReadoutMode(gdi);
      gdi.refresh();
    }
    /*else if (bi.id == "AllowStart")
      allowStart = gdi.isChecked(bi.id);
    else if (bi.id == "AllowControl")
      allowControl = gdi.isChecked(bi.id);
    else if (bi.id == "AllowFinish")
      allowFinish = gdi.isChecked(bi.id);*/
    else if (bi.id == "PlaySound") {
      oe->setProperty("PlaySound", gdi.isChecked(bi.id) ? 1 : 0);
    }
    else if (bi.id == "SoundChoice") {
      gdi.disableInput("SoundChoice");
      gdi.setRestorePoint("Sound");
      gdi.dropLine();
      gdi.fillDown();
      gdi.addString("", fontMediumPlus, "help:selectsound");
      gdi.dropLine(0.5);
      gdi.pushX();
      gdi.fillRight();

      auto addSoundWidget = [&gdi, this](const wchar_t* name, SND type, const wstring& label) {
        int itype = int(type);
        string nname = gdioutput::narrow(name);
        wstring fn = oe->getPropertyString(nname.c_str(), L"");
        bool doPlay = true;
        if (fn == L"none") {
          doPlay = false;
          fn = L"";
        }

        gdi.dropLine(1);
        gdi.addCheckbox(("DoPlay" + nname).c_str(), "", SportIdentCB, doPlay, "Använd").setExtra(itype);
        gdi.dropLine(-1);
        gdi.addInput("SoundFile", fn, 32, SportIdentCB, label).setExtra(itype);
        gdi.dropLine(0.8);
        gdi.addButton("BrowseSound", "Bläddra...", SportIdentCB).setExtra(itype);
        gdi.addButton("TestSound", "Testa", SportIdentCB).setExtra(itype);

        if (!doPlay) {
          gdi.setInputStatus("SoundFile", false, false, itype);
          gdi.setInputStatus("BrowseSound", false, false, itype);
          gdi.setInputStatus("TestSound", false, false, itype);
        }

        gdi.dropLine(3);
        gdi.popX();
      };

      addSoundWidget(L"SoundOK", SND::OK, L"Status OK:");
      addSoundWidget(L"SoundNotOK", SND::NotOK, L"Status inte OK (röd utgång):");
      addSoundWidget(L"SoundLeader", SND::Leader, L"Ny ledare i klassen:");
      addSoundWidget(L"SoundAction", SND::ActionNeeded, L"Åtgärd krävs:");

      gdi.addButton("CloseSound", "OK", SportIdentCB);
      gdi.popX();
      gdi.dropLine(3);
      gdi.scrollToBottom();
      gdi.refresh();
    }
    else if (bi.id == "BrowseSound") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Ljud (wav)", L"*.wav"));

      wstring file = gdi.browseForOpen(ext, L"wav");
      if (!file.empty()) {
        SND iType = SND(bi.getExtraInt());
        gdi.setText("SoundFile", file, false, int(iType));
        string name = typeFromSndType(iType);
        oe->setProperty(name.c_str(), file);
      }
    }
    else if (bi.id.substr(0, 6) == "DoPlay") {
      SND itype = SND(bi.getExtraInt());
      bool checked = gdi.isChecked(bi.id);
      gdi.setInputStatus("SoundFile", checked, false, int(itype));
      gdi.setInputStatus("BrowseSound", checked, false, int(itype));
      gdi.setInputStatus("TestSound", checked, false, int(itype));
      string name = typeFromSndType(itype);
      if (!checked) {
        oe->setProperty(name.c_str(), L"none");
      }
      else {
        wstring sf = gdi.getText("SoundFile", false, int(itype));
        oe->setProperty(name.c_str(), sf);
      }
    }
    else if (bi.id == "TestSound") {
      oe->setProperty("PlaySound", 1);
      gdi.check(bi.id, true);
      playReadoutSound(SND(bi.getExtraInt()));
    }
    else if (bi.id == "CloseSound") {
      gdi.restore("Sound", true);
      gdi.enableInput("SoundChoice");
    }
    else if (bi.id == "ClearMemory") {
      if (gdi.ask(L"Do you want to clear the card memory?")) {
        savedCards.clear();
        loadPage(gdi);
      }
    }
    else if (bi.id == "SaveMemory") {
      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Semikolonseparerad (csv)", L"*.csv"));

      int filterIx = 0;
      wstring file = gdi.browseForSave(ext, L"csv", filterIx);
      if (!file.empty()) {
        csvparser saver;
        saver.openOutput(file.c_str());
        vector<string> head = SICard::logHeader();
        saver.outputRow(head);
        int count = 0;
        for (auto it = savedCards.begin(); it != savedCards.end(); ++it) {
          vector<string> log = it->second.codeLogData(gdi, ++count);
          saver.outputRow(log);
        }
      }
    }
    else if (bi.id == "CreateCompetition") {
      createCompetitionFromCards(gdi);
    }
    else if (bi.id == "SIPassive") {
      wstring port = gdi.getText("ComPortName");
      if (gSI->openComListen(port.c_str(), gdi.getTextNo("BaudRate"))) {
        gSI->startMonitorThread(port.c_str());
        //loadPage(gdi);
        gdi.restore("", false);
        gdi.addString("", 1, L"Lyssnar på X.#" + port).setColor(colorDarkGreen);
        if (!gdi.hasData("ShowControlMapping") && gSI->isAnyOpenUnkownUnit()) {
          changeMapping(gdi);
        }
      }
      else
        gdi.addString("", 1, "FEL: Porten kunde inte öppnas").setColor(colorRed);
      gdi.dropLine();
      gdi.refresh();
    }
    else if (bi.id == "CancelTCP")
      gdi.restore("TCP");
    else if (bi.id == "StartTCP") {
      gSI->tcpAddPort(gdi.getTextNo("tcpPortNo"), 0);
      gdi.restore("TCP");
      gSI->startMonitorThread(L"TCP");

      printSIInfo(gdi, L"TCP");

      gdi.dropLine(0.5);
      refillComPorts(gdi);


      if (!gdi.hasData("ShowControlMapping") && gSI->isAnyOpenUnkownUnit()) {
        changeMapping(gdi);
      }

      gdi.refresh();
    }
    else if (bi.id == "StartSI") {
      wchar_t bf[64];
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("ComPort", lbi)) {

        if (lbi.data == 9999) {
          showTestingPanel = !showTestingPanel;
          loadPage(gdi);
          return 0;
        }
        
        swprintf_s(bf, 64, L"COM%d", lbi.getDataInt());
        wstring port = bf;

        if (lbi.text.substr(0, 3) == L"TCP")
          port = L"TCP";
        else if (lbi.text == L"TEST")
          port = L"TEST";

        if (gSI->isPortOpen(port)) {
          gSI->closeCom(port.c_str());
          gdi.addStringUT(0, lang.tl(L"Kopplar ifrån SportIdent på ") + port + lang.tl(L"... OK"));
          gdi.popX();
          gdi.dropLine();
          refillComPorts(gdi);
        }
        else {
          gdi.fillDown();
          if (port == L"TCP") {
            gdi.setRestorePoint("TCP");
            gdi.dropLine();
            gdi.pushX();
            gdi.fillRight();
            gdi.addInput("tcpPortNo", L"10000", 8, 0, L"Port för TCP:");
            gdi.dropLine();
            gdi.addButton("StartTCP", "Starta", SportIdentCB);
            gdi.addButton("CancelTCP", "Avbryt", SportIdentCB);
            gdi.dropLine(2);
            gdi.popX();
            gdi.fillDown();
            gdi.addString("", 10, "help:14070");
            gdi.scrollToBottom();
            gdi.refresh();
            return 0;
          }
          else if (port == L"TEST") {
            vector<pRunner> runners;
            oe->getRunners(0, 0, runners, false);
            for (pRunner r : runners) {
              if (r->getCard() || r->getCardNo() == 0)
                continue;
              vector<int> pl;
              auto c = r->getCourse(true);
              if (c) {
                pl = c->getControlNumbers();
                gSI->addTestCard(r->getCardNo(), pl);
              }
            }
          }

          gdi.addStringUT(0, lang.tl(L"Startar SI på ") + port + L"...");
          gdi.refresh();
          bool askListen = false;

          if (gSI->openCom(port.c_str())) {
            gSI->startMonitorThread(port.c_str());
            gdi.addStringUT(0, lang.tl(L"SI på ") + port + L": " + lang.tl(L"OK"));
            printSIInfo(gdi, port);

            const SI_StationInfo* si = gSI->findStation(port);
            if (si && !si->extended())
              gdi.addString("", boldText, "warn:notextended").setColor(colorDarkRed);
          }
          else {
            //Retry...
            Sleep(300);
            if (gSI->openCom(port.c_str())) {
              gSI->startMonitorThread(port.c_str());
              gdi.addStringUT(0, lang.tl(L"SI på ") + port + L": " + lang.tl(L"OK"));
              printSIInfo(gdi, port);

              const SI_StationInfo* si = gSI->findStation(port);
              if (si && !si->extended())
                gdi.addString("", boldText, "warn:notextended").setColor(colorDarkRed);
            }
            else {
              gdi.setRestorePoint();
              gdi.addStringUT(1, lang.tl(L"SI på ") + port + L": " + lang.tl(L"FEL, inget svar.")).setColor(colorRed);
              gdi.dropLine();
              gdi.refresh();

              if (gdi.ask(L"help:9615")) {

                gdi.pushX();
                gdi.fillRight();
                gdi.addInput("ComPortName", port, 10, 0, L"COM-Port:");
                //gdi.addInput("BaudRate", "4800", 10, 0, "help:baudrate");
                gdi.fillDown();
                gdi.addCombo("BaudRate", 130, 100, 0, L"help:baudrate");
                gdi.popX();
                gdi.addItem("BaudRate", L"4800", 4800);
                gdi.addItem("BaudRate", L"38400", 38400);
                gdi.selectItemByData("BaudRate", 38400);


                gdi.fillRight();
                gdi.addButton("SIPassive", "Lyssna...", SportIdentCB).setDefault();
                gdi.fillDown();
                gdi.addButton("Cancel", "Avbryt", SportIdentCB).setCancel();
                gdi.popX();
                askListen = true;
              }
            }
          }
          gdi.popX();
          gdi.dropLine();

          if (!askListen && !gdi.hasData("ShowControlMapping") && gSI->isAnyOpenUnkownUnit()) {
            changeMapping(gdi);
          }

          refillComPorts(gdi);
        }
        gdi.refresh();
      }
    }
    else if (bi.id == "SIInfo") {
      wchar_t bf[64];
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("ComPort", lbi))
      {
        if (lbi.text.substr(0, 3) == L"TCP")
          swprintf_s(bf, 64, L"TCP");
        else
          swprintf_s(bf, 64, L"COM%d", lbi.getDataInt());
        gdi.fillDown();
        gdi.addStringUT(0, lang.tl(L"Hämtar information om ") + wstring(bf) + L".");
        printSIInfo(gdi, bf);
        gdi.refresh();
      }
    }
    else if (bi.id == "AutoDetect")
    {
      gdi.fillDown();
      gdi.addString("", 0, "Söker efter SI-enheter... ");
      gdi.refresh();
      list<int> ports;
      if (!gSI->autoDetect(ports)) {
        gdi.addString("SIInfo", 0, "help:5422");
        gdi.refresh();
        return 0;
      }
      wchar_t bf[128];
      gSI->closeCom(0);

      while (!ports.empty()) {
        int p = ports.front();
        swprintf_s(bf, 128, L"COM%d", p);
        char bfn[128];
        sprintf_s(bfn, 128, "COM%d", p);

        gdi.addString((string("SIInfo") + bfn).c_str(), 0, L"#" + lang.tl(L"Startar SI på ") + wstring(bf) + L"...");
        gdi.refresh();
        if (gSI->openCom(bf)) {
          gSI->startMonitorThread(bf);
          gdi.addStringUT(0, lang.tl(L"SI på ") + wstring(bf) + L": " + lang.tl(L"OK"));
          printSIInfo(gdi, bf);

          const SI_StationInfo* si = gSI->findStation(bf);
          if (si && !si->extended())
            gdi.addString("", boldText, "warn:notextended").setColor(colorDarkRed);
        }
        else if (gSI->openCom(bf)) {
          gSI->startMonitorThread(bf);
          gdi.addStringUT(0, lang.tl(L"SI på ") + wstring(bf) + L": " + lang.tl(L"OK"));
          printSIInfo(gdi, bf);

          const SI_StationInfo* si = gSI->findStation(bf);
          if (si && !si->extended())
            gdi.addString("", boldText, "warn:notextended").setColor(colorDarkRed);
        }
        else gdi.addStringUT(0, lang.tl(L"SI på ") + wstring(bf) + L": " + lang.tl(L"FEL, inget svar"));

        gdi.refresh();
        gdi.popX();
        gdi.dropLine();
        ports.pop_front();
      }
    }
    else if (bi.id == "PrinterSetup") {
      if (mode == SIMode::ModeEntry || mode == SIMode::ModeRequestStartTime) {
        printStartInfo = true;
        TabList::splitPrintSettings(*oe, gdi, true, TSITab, TabList::StartInfo);
      }
      else {
        printSplits = true;
        TabList::splitPrintSettings(*oe, gdi, true, TSITab, TabList::Splits);
      }
    }
    else if (bi.id == "AutoTie") {
      gEvent->setProperty("AutoTie", gdi.isChecked(bi.id));
    }
    else if (bi.id == "AutoTieRent") {
      gEvent->setProperty("AutoTieRent", gdi.isChecked(bi.id));
    }
    else if (bi.id == "RentCardTie") {
      gEvent->setProperty("RentCard", gdi.isChecked(bi.id));
    }
    else if (bi.id == "EditEntryFields") {
      TabCompetition& tc = dynamic_cast<TabCompetition&>(*gdi.getTabs().get(TCmpTab));
      tc.loadSettings(gdi);
      gdi.selectItemByData("DataFields", int(oEvent::ExtraFieldContext::DirectEntry));
      tc.showExtraFields(gdi, oEvent::ExtraFieldContext::DirectEntry);
    }
    else if (bi.id == "TieOK") {
      tieCard(gdi);
    }
    else if (bi.id == "Interactive") {
      interactiveReadout = gdi.isChecked(bi.id);
      gEvent->setProperty("Interactive", interactiveReadout);

      if (mode == SIMode::ModeAssignCards) {
        gdi.restore("SIPageLoaded");
        showAssignCard(gdi, true);
      }
    }
    else if (bi.id == "MultipleStarts") {
      multipleStarts = gdi.isChecked(bi.id);
    }
    else if (bi.id == "Database") {
      useDatabase = gdi.isChecked(bi.id);
      gEvent->setProperty("Database", useDatabase);
    }
    else if (bi.id == "PrintSplits") {
      printSplits = gdi.isChecked(bi.id);
    }
    else if (bi.id == "StartInfo") {
      printStartInfo = gdi.isChecked(bi.id);
    }
    else if (bi.id == "UseManualInput") {
      manualInput = gdi.isChecked("UseManualInput");
      oe->setProperty("ManualInput", manualInput ? 1 : 0);
      gdi.restore("ManualInput");
      if (manualInput)
        showManualInput(gdi);
    }
    else if (bi.id == "ReadoutWindow") {
      gdioutput* gdi_settings = getExtraWindow("readout_view", true);
      if (!gdi_settings) {
        gdi_settings = createExtraWindow("readout_view", lang.tl("Brickavläsning"), gdi.scaleLength(800), gdi.scaleLength(600), false);
      }
      if (gdi_settings) {
        showReadoutStatus(*gdi_settings, nullptr, nullptr, nullptr, L"");
      }
    }
    else if (bi.id == "Import") {
      int origin = bi.getExtraInt();

      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"Semikolonseparerad (csv)", L"*.csv"));

      wstring file = gdi.browseForOpen(ext, L"csv");
      if (!file.empty()) {
        gdi.restore("Help");
        csvparser csv;
        csv.importCards(*oe, file.c_str(), cards);
        if (cards.empty()) {
          csv.importPunches(*oe, file.c_str(), punches);
          if (!punches.empty()) {
            gdi.dropLine(2);
            gdi.addString("", 1, "Inlästa stämplar");
            set<string> dates;
            showReadPunches(gdi, punches, dates);

            filterDate.clear();
            filterDate.push_back(lang.tl("Inget filter"));
            for (set<string>::iterator it = dates.begin(); it != dates.end(); ++it)
              filterDate.push_back(gdi.widen(*it));

            gdi.dropLine(2);
            gdi.scrollToBottom();
            gdi.fillRight();
            gdi.pushX();
            gdi.addSelection("ControlType", 150, 300, 0, L"Enhetstyp:");

            vector< pair<wstring, size_t> > d;
            oe->fillControlTypes(d);
            gdi.setItems("ControlType", d);
            // oe->fillControlTypes(gdi, "ControlType");
            gdi.selectItemByData("ControlType", oPunch::PunchCheck);

            gdi.addSelection("Filter", 150, 300, 0, L"Datumfilter:");
            for (size_t k = 0; k < filterDate.size(); k++) {
              gdi.addItem("Filter", filterDate[k], k);
            }
            gdi.selectItemByData("Filter", 0);
            gdi.dropLine(1);
            gdi.addButton("SavePunches", "Spara", SportIdentCB).setExtra(origin);
            gdi.addButton("Cancel", "Avbryt", SportIdentCB).setExtra(origin);
            gdi.fillDown();
            gdi.scrollToBottom();
            gdi.popX();
          }
          else {
            loadPage(gdi);
            throw std::exception("Felaktigt filformat");
          }
        }
        else {
          gdi.pushX();
          gdi.dropLine(3);

          gdi.addString("", 1, "Inlästa brickor");
          showReadCards(gdi, cards);
          gdi.dropLine();
          gdi.fillDown();
          if (interactiveReadout)
            gdi.addString("", 0, "Välj Spara för att lagra brickorna. Interaktiv inläsning är aktiverad.");
          else
            gdi.addString("", 0, "Välj Spara för att lagra brickorna. Interaktiv inläsning är INTE aktiverad.");

          gdi.fillRight();
          gdi.pushX();
          gdi.addButton("SaveCards", "Spara", SportIdentCB).setExtra(origin);
          gdi.addButton("Cancel", "Avbryt", SportIdentCB).setExtra(origin);
          gdi.fillDown();
          gdi.scrollToBottom();
        }
      }
    }
    else if (bi.id == "SavePunches") {
      int origin = bi.getExtraInt();
      ListBoxInfo lbi;
      gdi.getSelectedItem("ControlType", lbi);
      int type = lbi.data;
      gdi.getSelectedItem("Filter", lbi);
      bool dofilter = signed(lbi.data) > 0;
      string filter = lbi.data < filterDate.size() ? gdi.narrow(filterDate[lbi.data]) : "";

      gdi.restore("Help");
      for (size_t k = 0; k < punches.size(); k++) {
        if (dofilter && filter != punches[k].date)
          continue;
        oe->addFreePunch(punches[k].time, type, 0, punches[k].card, true, true);
      }
      punches.clear();
      if (origin == 1) {
        TabRunner& tc = dynamic_cast<TabRunner&>(*gdi.getTabs().get(TRunnerTab));
        tc.showInForestList(gdi);
      }
    }
    else if (bi.id == "SaveCards") {
      int origin = bi.getExtraInt();
      gdi.restore("Help");
      oe->synchronizeList({ oListId::oLCardId, oListId::oLRunnerId });
      for (size_t k = 0; k < cards.size(); k++)
        insertSICard(gdi, cards[k]);

      oe->reEvaluateAll(set<int>(), true);
      cards.clear();
      if (origin == 1) {
        TabRunner& tc = dynamic_cast<TabRunner&>(*gdi.getTabs().get(TRunnerTab));
        tc.showInForestList(gdi);
      }
    }
    else if (bi.id == "Save") {
      SICard sic(ConvertedTimeStatus::Hour24);
      sic.isDebugCard = true;

      sic.CheckPunch.Code = -1;
      sic.CardNumber = gdi.getTextNo("SI");
      int f = convertAbsoluteTimeHMS(gdi.getText("Finish"), oe->getZeroTimeNum());
      int s = convertAbsoluteTimeHMS(gdi.getText("Start"), oe->getZeroTimeNum());
      int c = convertAbsoluteTimeHMS(gdi.getText("Check"), oe->getZeroTimeNum());

      testStartTime.clear();
      testFinishTime.clear();
      testCheckTime.clear();
      testCardNumber = 0;

      if (f < s) {
        f += 24 * timeConstHour;
      }
      sic.FinishPunch.Time = f % (24 * timeConstHour);
      sic.StartPunch.Time = s % (24 * timeConstHour);
      sic.CheckPunch.Time = c % (24 * timeConstHour);
      
      if (!gdi.isChecked("HasFinish")) {
        sic.FinishPunch.Code = -1;
        sic.FinishPunch.Time = 0;
      }

      if (!gdi.isChecked("HasStart")) {
        sic.StartPunch.Code = -1;
        sic.StartPunch.Time = 0;
      }

      if (!gdi.isChecked("HasCheck")) {
        sic.CheckPunch.Code = -1;
        sic.CheckPunch.Time = 0;
      }

      if (NC > testControls.size())
        testControls.resize(NC);

      for (int i = 0; i < NC; i++)
        testControls[i] = gdi.getTextNo("C" + itos(i + 1));

      double t = 0.1;
      for (sic.nPunch = 0; sic.nPunch<unsigned(NC); sic.nPunch++) {
        int c = testControls[sic.nPunch];
        sic.Punch[sic.nPunch].Time = ((int(f * t + s * (1.0 - t))/ timeUnitsPerSecond) % (24 * timeConstSecPerHour)) * timeUnitsPerSecond;
        t += ((1.0 - t) * (sic.nPunch + 1) / 10.0) * ((rand() % 100) + 400.0) / 500.0;
        if ((sic.nPunch % 11) == 1 || 5 == (sic.nPunch % 8))
          t += min(0.2, 0.9 - t);
        if (sic.nPunch == 0 && c > 1000) {
          sic.miliVolt = c;
          c = c % 100;
        }
        sic.Punch[sic.nPunch].Code = c;
      }

      gdi.getRecorder().record("insertCard(" + itos(sic.CardNumber) + ", \"" + sic.serializePunches() + "\"); //Readout card");

      if (false) {
        sic.convertedTime = ConvertedTimeStatus::Hour12;
        sic.StartPunch.Time %= (12 * timeConstHour);
        sic.FinishPunch.Time %= (12 * timeConstHour);
        sic.CheckPunch.Time %= (12 * timeConstHour);
        for (unsigned i = 0; i < sic.nPunch; i++) {
          sic.Punch[i].Time %= (12 * timeConstHour);
        }
      }
      gSI->addCard(sic);
    }
    else if (bi.id == "SaveP") {
      SICard sic(ConvertedTimeStatus::Hour24);
      sic.clear(0);
      sic.isDebugCard = true;
      sic.FinishPunch.Code = -1;
      sic.CheckPunch.Code = -1;
      sic.StartPunch.Code = -1;
      sic.CardNumber = gdi.getTextNo("SI");
      int t = convertAbsoluteTimeHMS(gdi.getText("PunchTime"), oe->getZeroTimeNum());
      testPunchTime.clear();
      testCardNumber = 0;

      if (t > 0) {
        if (testType == oPunch::PunchStart) {
          sic.StartPunch.Time = t;
          sic.StartPunch.Code = 1;
        }
        else if (testType == oPunch::PunchFinish) {
          sic.FinishPunch.Time = t;
          sic.FinishPunch.Code = 1;
        }
        else if (testType == oPunch::PunchCheck) {
          sic.CheckPunch.Time = t;
          sic.CheckPunch.Code = 1;
        }
        else {
          sic.Punch[sic.nPunch].Code = gdi.getTextNo("ControlNumber");
          sic.Punch[sic.nPunch].Time = t;
          sic.nPunch = 1;
        }
        sic.punchOnly = true;
        sic.convertedTime = ConvertedTimeStatus::Hour24;
        gSI->addCard(sic);
        return 0;
      }
    }
    else if (bi.id == "Cancel") {
      int origin = bi.getExtraInt();
      activeSIC.clear(0);
      punches.clear();
      if (origin == 1) {
        TabRunner& tc = dynamic_cast<TabRunner&>(*gdi.getTabs().get(TRunnerTab));
        tc.showInForestList(gdi);
        return 0;
      }
      loadPage(gdi);

      checkMoreCardsInQueue(gdi);
      return 0;
    }
    else if (bi.id == "SaveUnpaired") {
      gdi.restore();
      processUnmatched(gdi, activeSIC, false);
      activeSIC.clear(0);
      return 0;
    }
    else if (bi.id == "OK1") {
      wstring name = gdi.getText("Runners");
      wstring club = gdi.getText("Club", true);

      if (name.length() == 0) {
        gdi.alert("Alla deltagare måste ha ett namn.");
        return 0;
      }

      pRunner r = 0;
      DWORD rid;
      bool lookup = true;

      if (gdi.getData("RunnerId", rid) && rid > 0) {
        r = gEvent->getRunner(rid, 0);

        if (r && r->getCard()) {
          if (!askOverwriteCard(gdi, r)) {
            r = 0;
            lookup = false;
          }
        }

        if (r && stringMatch(r->getName(), name)) {
          gdi.restore();
          //We have a match!
          SICard copy = activeSIC;
          activeSIC.clear(&activeSIC);
          processCard(gdi, r, copy);
          return 0;
        }
      }

      if (lookup) {
        r = gEvent->getRunnerByName(name, club);
        if (r && r->getCard()) {
          if (!askOverwriteCard(gdi, r))
            r = 0;
        }
      }

      if (r) {
        //We have a match!
        gdi.setData("RunnerId", r->getId());

        gdi.restore();
        SICard copy = activeSIC;
        activeSIC.clear(&activeSIC);
        processCard(gdi, r, copy);
        return 0;
      }

      //We have a new runner in our system
      gdi.fillRight();
      gdi.pushX();

      SICard si_copy = activeSIC;
      gEvent->convertTimes(nullptr, si_copy);

      //Find matching class...
      vector<pClass> classes;
      int dist = gEvent->findBestClass(activeSIC, classes);

      if (classes.size() == 1 && dist == 0 && si_copy.StartPunch.Time > 0 && classes[0]->getType() != L"tmp") {
        //We have a match!
        wstring club = gdi.getText("Club", true);

        if (club.length() == 0 && oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
          pClub noClub = oe->getClub(oe->getVacantClub(true));
          if (noClub) {
            noClub->synchronize();
            club = noClub->getName();
          }
          else
            club = lang.tl("Klubblös");
        }

        const wstring year;
        pRunner r = gEvent->addRunner(gdi.getText("Runners"), club,
          classes[0]->getId(), activeSIC.CardNumber, year, true);
        if (oe->isHiredCard(activeSIC.CardNumber)) {
          r->setRentalCard(true);
        }
        gdi.setData("RunnerId", r->getId());

        gdi.restore();
        SICard copy = activeSIC;
        activeSIC.clear(&activeSIC);
        processCard(gdi, r, copy);
        r->synchronize();
        return 0;
      }

      gdi.restore("restOK1", false);
      gdi.popX();
      gdi.dropLine(2);

      gdi.addInput("StartTime", gEvent->getAbsTime(si_copy.StartPunch.Time), 8, 0, L"Starttid:");

      gdi.addSelection("Classes", 200, 300, 0, L"Klass:");
      gEvent->fillClasses(gdi, "Classes", {}, oEvent::extraNone, oEvent::filterNone);
      gdi.setInputFocus("Classes");

      if (classes.size() > 0)
        gdi.selectItemByData("Classes", classes[0]->getId());

      gdi.dropLine();
      gdi.setRestorePoint("restOK2");

      if (oe->getNumClasses() > 0)
        gdi.addButton("OK2", "OK", SportIdentCB).setDefault();
      
      gdi.addButton("NewClass", "Skapa ny klass", SportIdentCB);

      gdi.fillDown();
      gdi.addButton("Cancel", "Avbryt inläsning", SportIdentCB).setCancel();

      gdi.popX();
      if (classes.size() > 0)
        gdi.addString("FindMatch", 0, "Press Enter to continue").setColor(colorGreen);
      gdi.dropLine();

      gdi.refresh();
      return 0;
    }
    else if (bi.id == "OK2")
    {
      //New runner in existing class...

      ListBoxInfo lbi;
      gdi.getSelectedItem("Classes", lbi);

      if (lbi.data == 0 || lbi.data == -1) {
        gdi.alert("Du måste välja en klass");
        return 0;
      }
      pClass pc = oe->getClass(lbi.data);
      if (pc && pc->getType() == L"tmp")
        pc->setType(L"");

      wstring club = gdi.getText("Club", true);

      if (club.empty() && oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs))
        club = lang.tl("Klubblös");

      const wstring year;
      pRunner r = gEvent->addRunner(gdi.getText("Runners"), club,
        lbi.data, activeSIC.CardNumber, year, true);

      if (activeSIC.CardNumber > 0 && oe->isHiredCard(activeSIC.CardNumber)) {
        r->setRentalCard(true);
      }

      r->setStartTimeS(gdi.getText("StartTime"));
      r->setCardNo(activeSIC.CardNumber, false);

      gdi.restore();
      SICard copy = activeSIC;
      activeSIC.clear(&activeSIC);
      processCard(gdi, r, copy);
    }
    else if (bi.id == "NewClass") {
      gdi.restore("restOK2", false);
      gdi.popX();
      gdi.dropLine(2);
      gdi.fillRight();
      gdi.pushX();

      gdi.addInput("ClassName", gEvent->getAutoClassName(), 10, 0, L"Klassnamn:");

      gdi.dropLine();
      gdi.addButton("OK3", "OK", SportIdentCB).setDefault();
      gdi.fillDown();
      gdi.addButton("Cancel", "Avbryt inläsning", SportIdentCB).setCancel();

      gdi.setInputFocus("ClassName", true);
      gdi.refresh();
      gdi.popX();
    }
    else if (bi.id == "OK3") {
      pCourse pc = 0;
      pClass pclass = 0;

      if (oe->getNumClasses() == 1 && oe->getClass(1) != 0 &&
        oe->getClass(1)->getType() == L"tmp" &&
        oe->getClass(1)->getNumRunners(false, false, false) == 0) {
        pclass = oe->getClass(1);
        pclass->setType(L"");
        pclass->setName(gdi.getText("ClassName"), true);
        pc = pclass->getCourse();
        if (pc)
          pc->setName(gdi.getText("ClassName"));
      }

      if (pc == 0 && !gEvent->getMeOSFeatures().hasFeature(MeOSFeatures::NoCourses)) {
        pc = gEvent->addCourse(gdi.getText("ClassName"));
        for (unsigned i = 0; i < activeSIC.nPunch; i++)
          pc->addControl(activeSIC.Punch[i].Code);
      }
      if (pclass == 0) {
        pclass = gEvent->addClass(gdi.getText("ClassName"), pc ? pc->getId() : 0);
      }
      else if (pc)
        pclass->setCourse(pc);

      const wstring year;
      pRunner r = gEvent->addRunner(gdi.getText("Runners"), gdi.getText("Club", true),
        pclass->getId(), activeSIC.CardNumber, year, true);

      if (activeSIC.CardNumber > 0 && oe->isHiredCard(activeSIC.CardNumber)) {
        r->setRentalCard(true);
      }
      r->setStartTimeS(gdi.getText("StartTime"));
      r->setCardNo(activeSIC.CardNumber, false);
      gdi.restore();
      SICard copy_sic = activeSIC;
      activeSIC.clear(&activeSIC);
      processCard(gdi, r, copy_sic);
    }
    else if (bi.id == "OK4") {
      //Existing runner in existing class...

      ListBoxInfo lbi;
      gdi.getSelectedItem("Classes", lbi);

      if (lbi.data == 0 || lbi.data == -1)
      {
        gdi.alert("Du måste välja en klass");
        return 0;
      }

      DWORD rid;
      pRunner r;

      if (gdi.getData("RunnerId", rid) && rid > 0)
        r = gEvent->getRunner(rid, 0);
      else r = gEvent->addRunner(lang.tl(L"Oparad bricka"), lang.tl("Okänd"), 0, 0, L"", false);

      r->setClassId(lbi.data, true);

      gdi.restore();
      SICard copy = activeSIC;
      activeSIC.clear(&activeSIC);
      processCard(gdi, r, copy);
    }
    else if (bi.id == "EntryOK") {
      storedInfo.clear();
      oe->synchronizeList({ oListId::oLRunnerId, oListId::oLCardId });

      wstring name = gdi.getText("Name");
      if (name.empty()) {
        gdi.alert("Alla deltagare måste ha ett namn.");
        return 0;
      }
      int rid = bi.getExtraInt();
      pRunner r = oe->getRunner(rid, 0);
      int cardNo = gdi.getTextNo("CardNo");

      if (oe->deprecateOldCards() && oe->getCardSystem().isDeprecated(cardNo)) {
        gdi.alert(L"Brickan är av äldre typ och kan inte användas.");
        return 0;
      }

      pRunner cardRunner = oe->getRunnerByCardNo(cardNo, 0, oEvent::CardLookupProperty::ForReadout);
      if (cardNo > 0 && cardRunner != 0 && cardRunner != r) {
        gdi.alert(L"Bricknummret är upptaget (X).#" + cardRunner->getName() + L", " + cardRunner->getClass(true));
        return 0;
      }

      ListBoxInfo lbi;
      gdi.getSelectedItem("Class", lbi);
      pClass clz = oe->getClass(lbi.data);
      if (!clz) {
        if (oe->getNumClasses() > 0) {
          gdi.alert(L"Ingen klass vald");
          return 0;
        }
        set<wstring> dmy;
        clz = oe->getClassCreate(0, lang.tl(L"Öppen klass"), dmy);
        lbi.data = clz->getId();
        clz->setAllowQuickEntry(true);
        clz->synchronize();
      }
      bool updated = false;
      const wstring year;
      bool warnClassFull = false;
      if (!r || r->getClassRef(false) != clz) {
        int numRemMaps = clz->getNumRemainingMaps(true);
        if (numRemMaps != numeric_limits<int>::min()) {
          if (clz->getNumRemainingMaps(true) > 0)
            warnedClassOutOfMaps.erase(clz->getId());
          else {
            warnClassFull = true;
            if (!warnedClassOutOfMaps.count(clz->getId())) {
              warnedClassOutOfMaps.insert(clz->getId());
              if (!gdi.ask(L"ask:outofmaps"))
                return 0;
            }
          }
        }
      }

      if (r == 0) {
        r = oe->addRunner(name, gdi.getText("Club", true), lbi.data, cardNo, year, true);
        r->setCardNo(0, false, false); // Clear to match below
      }
      else {
        int clubId = 0;
        if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
          wstring cname = gdi.getText("Club", true);

          if (!cname.empty()) {
            pClub club = oe->getClubCreate(0, cname);
            clubId = club->getId();
          }
        }
        int birthYear = 0;
        r->updateFromDB(name, clubId, lbi.data, cardNo, birthYear, false);
        r->setName(name, true);
        r->setClubId(clubId);
        r->setClassId(lbi.data, true);
        updated = true;
      }

      lastClubId = r->getClubId();
      lastClassId = r->getClassId(true);
      lastFee = gdi.getText("Fee", true);
      int lastFeeNum = oe->interpretCurrency(lastFee);

      r->setCardNo(cardNo, true);//XXX

      oDataInterface di = r->getDI();

      r->setRentalCard(gdi.isChecked("RentCard"));
      di.setInt("Fee", lastFeeNum);
      r->setFlag(oRunner::FlagFeeSpecified, true);

      int totFee = lastFeeNum + r->getRentalCardFee(true);
      writePayMode(gdi, totFee, *r);

      TabRunner::saveExtraFields(gdi, *r);

      if (gdi.isChecked("NoTiming"))
        r->setStatus(RunnerStatus::StatusNoTiming, true, oBase::ChangeType::Update, false);
      else if (r->getStatus() == RunnerStatus::StatusNoTiming)
        r->setStatus(RunnerStatus::StatusUnknown, true, oBase::ChangeType::Update, false);

      r->setFlag(oRunner::FlagTransferSpecified, gdi.hasWidget("AllStages"));
      r->setFlag(oRunner::FlagTransferNew, gdi.isChecked("AllStages"));

      if (gdi.hasWidget("StartTime")) 
        r->setStartTimeS(gdi.getText("StartTime"));
      
      wstring bib;
      if (gdi.hasWidget("Bib")) {
        wstring bibIn = gdi.getText("Bib");
        if (bibIn.empty()) {
          switch (r->autoAssignBib()) {
          case oRunner::BibAssignResult::Assigned:
            bib = L", " + lang.tl(L"Nummerlapp: ") + r->getBib();
            break;
          case oRunner::BibAssignResult::Failed:
            bib = L", " + lang.tl(L"Ingen nummerlapp");
            break;
          }
        }
        else {
          r->setBib(bibIn, 0, false);
          bib = L", " + lang.tl(L"Nummerlapp: ") + r->getBib();
        }
      }
      r->synchronize();

      gdi.restore("EntryLine");

      wchar_t bf[256];
      wstring cno = r->getCardNo() > 0 ? L"(" + itow(r->getCardNo()) + L"), " : L"";

      if (r->getClubId() != 0) {
        swprintf_s(bf, L"%s%s, %s", cno.c_str(), r->getClub().c_str(),
          r->getClass(true).c_str());
      }
      else {
        swprintf_s(bf, L"%s%s", cno.c_str(), r->getClass(true).c_str());
      }

      wstring info(bf);
      if (r->isRentalCard())
        info += lang.tl(L", Hyrbricka");

      vector< pair<wstring, size_t> > modes;
      oe->getPayModes(modes);
      wstring pm;
      if (modes.size() > 1 && size_t(r->getPaymentMode()) < modes.size())
        pm = L" (" + modes[r->getPaymentMode()].first + L")";
      if (r->getDI().getInt("Paid") > 0)
        info += lang.tl(L", Betalat") + pm;

      bool warnPayment = r->getDI().getInt("Paid") < totFee && (
        r->getClubRef() == 0 ||
        r->getClubId() == oe->getVacantClubIfExist(true) ||
        r->getClubId() == oe->getVacantClubIfExist(false));

      if (bib.length() > 0)
        info += bib;

      if (updated)
        info += lang.tl(L" [Uppdaterad anmälan]");

      gdi.pushX();
      gdi.fillRight();
      gdi.addString("ChRunner", 0, L"#" + r->getName(), SportIdentCB).setColor(colorGreen).setExtra(r->getId());
      gdi.fillDown();
      gdi.addStringUT(0, info, 0);
      gdi.popX();

      if (warnPayment) {
        gdi.addString("", fontMediumPlus, "Varning: avgiften kan ej faktureras").setColor(colorRed);
      }
      if (warnClassFull) {
        gdi.addString("", fontMediumPlus, "Varning: Kartorna är slut").setColor(colorRed);
      }

      generateStartInfo(gdi, *r, true);

      gdi.setRestorePoint("EntryLine");
      generateEntryLine(gdi, 0);
    }
    else if (bi.id == "EntryCancel") {
      gdi.restore("EntryLine");
      storedInfo.clear();
      generateEntryLine(gdi, 0);
    }
    else if (bi.id == "RentCard" || bi.id == "Paid" || bi.id == "AllStages") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "ManualOK") {
      if (runnerMatchedId == -1)
        throw meosException("Löparen hittades inte");

      bool useNow = gdi.getExtraInt("FinishTime") == 1;
      wstring time = useNow ? getLocalTimeOnly() : gdi.getText("FinishTime");

      int relTime = oe->getRelativeTime(time);
      if (relTime <= 0) {
        throw meosException("Ogiltig tid.");
      }
      bool ok = gdi.isChecked("StatusOK");
      bool dnf = gdi.isChecked("StatusDNF");

      pRunner r = oe->getRunner(runnerMatchedId, 0);
      if (r == 0)
        throw meosException("Löparen hittades inte");

      if (r->getStatus() != StatusUnknown) {
        if (!gdi.ask(L"X har redan ett resultat. Vi du fortsätta?#" + r->getCompleteIdentification(oRunner::IDType::OnlyThis)))
          return 0;
      }

      gdi.restore("ManualInput", false);

      SICard sic(ConvertedTimeStatus::Hour24);
      sic.runnerId = runnerMatchedId;
      sic.relativeFinishTime = relTime;
      sic.statusOK = ok;
      sic.statusDNF = dnf;

      gSI->addCard(sic);
    }
    else if (bi.id == "StatusOK") {
      bool ok = gdi.isChecked(bi.id);
      if (ok) {
        gdi.check("StatusDNF", false);
      }
    }
    else if (bi.id == "StatusDNF") {
      bool dnf = gdi.isChecked(bi.id);
      gdi.setInputStatus("StatusOK", !dnf);
      gdi.check("StatusOK", !dnf);
    }
    else if (bi.id == "RHCClear") {
      if (gdi.ask(L"Vill du tömma listan med hyrbrickor?")) {
        oe->clearHiredCards();
        loadPage(gdi);
      }
    }
    else if (bi.id == "RHCImport") {
      wstring fn = gdi.browseForOpen({ make_pair(L"Semikolonseparerad (csv)", L"*.csv") }, L"csv");
      if (!fn.empty()) {
        csvparser csv;
        list<vector<wstring>> data;
        csv.parse(fn, data);
        set<int> rentCards;
        for (auto& c : data) {
          for (wstring wc : c) {
            int cn = _wtoi(wc.c_str());
            if (cn > 0) {
              rentCards.insert(cn);
              oe->setHiredCard(cn, true);
              gdi.addStringUT(0, itos(cn)).setHandler(getResetHiredCardHandler());
            }
          }
        }

        writeDefaultHiredCards();

        gdi.scrollToBottom();
        gdi.refresh();
        vector<pRunner> runners;
        oe->getRunners(0, 0, runners);
        if (!runners.empty() && gdi.ask(L"Vill du sätta hyrbricka på befintliga löpare med dessa brickor?")) {
          for (pRunner r : runners) {
            if (rentCards.count(r->getCardNo()) && !r->isRentalCard()) {
              gdi.addStringUT(0, r->getCompleteIdentification(oRunner::IDType::OnlyThis));
              r->setRentalCard(true);
            }
          }
        }
        loadPage(gdi);
      }
    }
    else if (bi.id == "RHCExport") {
      int ix = 0;
      wstring fn = gdi.browseForSave({ make_pair(L"Semikolonseparerad (csv)", L"*.csv") }, L"csv", ix);
      if (!fn.empty()) {
        oe->synchronizeList(oListId::oLPunchId);

        auto hc = oe->getHiredCards();
        csvparser csv;
        csv.openOutput(fn);
        for (int c : hc)
          csv.outputRow(itos(c));
        csv.closeOutput();

        writeDefaultHiredCards();
      }
    }
    else if (bi.id == "RHCPrint") {
      gdioutput gdiPrint("print", gdi.getScale());
      gdiPrint.clearPage(false);

      gdiPrint.addString("", boldLarge, "Hyrbricksrapport");

      oe->synchronizeList(oListId::oLPunchId);
      auto hc = oe->getHiredCards();
      int dc = gdiPrint.scaleLength(70);
      int col = 0;
      gdiPrint.dropLine(2);
      int cx = gdiPrint.getCX();
      int cy = gdiPrint.getCY();

      for (int h : hc) {
        if (col >= 8) {
          col = 0;
          cy += gdiPrint.getLineHeight() * 2;
        }
        gdiPrint.addStringUT(cy, cx + col * dc, 0, itow(h));
        col++;
      }

      gdiPrint.refresh();
      gdiPrint.print(oe);
    }
    else if (bi.id == "CCSClear") {
      if (gdi.ask(L"Vill du göra om avbockningen från början igen?")) {
        checkedCardFlags.clear();
        gdi.restore("CCSInit", false);
        showCheckCardStatus(gdi, "fillrunner");
        showCheckCardStatus(gdi, "stat");
        gdi.refresh();
      }
    }
    else if (bi.id == "CCSReport") {
      gdi.restore("CCSInit", false);
      showCheckCardStatus(gdi, "stat");
      showCheckCardStatus(gdi, "report");
      gdi.refresh();
    }
    else if (bi.id == "CCSPrint") {
      //gdi.print(oe);
      gdioutput gdiPrint("print", gdi.getScale());
      gdiPrint.clearPage(false);

      int tCardPosX = cardPosX;
      int tCardPosY = cardPosY;
      int tCardOffsetX = cardOffsetX;
      int tCardCurrentCol = cardCurrentCol;

      showCheckCardStatus(gdiPrint, "stat");
      showCheckCardStatus(gdiPrint, "report");
      showCheckCardStatus(gdiPrint, "tickoff");

      cardPosX = tCardPosX;
      cardPosY = tCardPosY;
      cardOffsetX = tCardOffsetX;
      cardCurrentCol = tCardCurrentCol;

      gdiPrint.refresh();
      gdiPrint.print(oe);
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo bi = *(ListBoxInfo*)data;

    if (bi.id == "Runners") {
      pRunner r = gEvent->getRunner(bi.data, 0);
      if (r) {
        gdi.setData("RunnerId", bi.data);
        if (gdi.hasWidget("Club"))
          gdi.setText("Club", r->getClub());
        gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);
      }
    }
    else if (bi.id == "PayMode") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "ComPort") {
      bool active = true;
      if (bi.data == 9999) 
        active = showTestingPanel;
      else {
        wchar_t bf[64];
        if (bi.text.substr(0, 3) != L"TCP")
          swprintf_s(bf, 64, L"COM%d", bi.getDataInt());
        else
          wcscpy_s(bf, L"TCP");

        active = gSI->isPortOpen(bf);
      }

      if (active)
        gdi.setText("StartSI", lang.tl("Koppla ifrån"));
      else
        gdi.setText("StartSI", lang.tl("Aktivera"));
    }
    else if (bi.id == "ReadType") {
      gdi.restore("SIPageLoaded");
      mode = SIMode(bi.data);
      //gdi.setInputStatus("StartInfo", mode == ModeEntry);

      if (mode == SIMode::ModeAssignCards || mode == SIMode::ModeEntry) {
        if (mode == SIMode::ModeAssignCards) {
          showAssignCard(gdi, true);
        }
        else {
          checkBoxToolBar(gdi, { CheckBox::UseDB, CheckBox::PrintStart, CheckBox::ExtraDataFields });
          entryTips(gdi);
          generateEntryLine(gdi, 0);
        }
        /*gdi.setInputStatus("Interactive", mode == ModeAssignCards);
        gdi.setInputStatus("Database", mode != ModeAssignCards, true);
        gdi.disableInput("PrintSplits");

        gdi.disableInput("UseManualInput");*/
      }
      else if (mode == SIMode::ModeReadOut) {
        showReadoutMode(gdi);
      }
      else if (mode == SIMode::ModeCardData) {
        numSavedCardsOnCmpOpen = savedCards.size();
        showModeCardData(gdi);
      }
      else if (mode == SIMode::ModeCheckCards) {
        showCheckCardStatus(gdi, "init");
      }
      else if (mode == SIMode::ModeRegisterCards) {
        showRegisterHiredCards(gdi);
      }
      else if (mode == SIMode::ModeRequestStartTime) {
        showRequestStartTime(gdi);
      }
      updateReadoutFunction(gdi);
      gdi.refresh();
    }
    else if (bi.id == "Fee") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "NC") {
      readTestData(gdi);
      NC = bi.data;
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TSITab, 0);
    }
    else if (bi.id == "TestType") {
      readTestData(gdi);
      testType = bi.data;
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TSITab, 0);
    }
  }
  else if (type == GUI_LINK) {
    TextInfo ti = *(TextInfo*)data;
    if (ti.id == "ChRunner") {
      pRunner r = oe->getRunner(ti.getExtraInt(), 0);
      generateEntryLine(gdi, r);
    }
    else if (ti.id == "EditAssign") {
      int id = ti.getExtraInt();
      pRunner r = oe->getRunner(id, 0);
      if (r) {
        gdi.setText("CardNo", r->getCardNo());
        gdi.setText("RunnerId", r->getRaceIdentifier());
        gdi.setText("FindMatch", r->getCompleteIdentification(oRunner::IDType::OnlyThis), true);
        runnerMatchedId = r->getId();
      }
    }
    else if (ti.id == "edit") {
      int rId = ti.getExtraInt();
      if (oe->getRunner(rId, 0)) {
        TabRunner& tr = dynamic_cast<TabRunner&>(*gdi.getTabs().get(TRunnerTab));
        tr.loadPage(gdi, rId);
      }
    }
  }
  else if (type == GUI_COMBO) {
    ListBoxInfo bi = *(ListBoxInfo*)data;

    if (bi.id == "Fee") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "Runners") {
      DWORD rid;
      if ((gdi.getData("RunnerId", rid) && rid > 0) || !gdi.getText("Club", true).empty())
        return 0; // Selected from list

      if (!bi.text.empty() && showDatabase()) {
        pRunner db_r = oe->dbLookUpByName(bi.text, 0, 0, 0);
        if (!db_r && lastClubId)
          db_r = oe->dbLookUpByName(bi.text, lastClubId, 0, 0);

        if (db_r && gdi.hasWidget("Club")) {
          gdi.setText("Club", db_r->getClub());
        }
      }
      gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);

    }
  }
  else if (type == GUI_COMBOCHANGE) {
    ListBoxInfo bi = *(ListBoxInfo*)data;
    if (bi.id == "Runners") {

      if (!showDatabase()) {
        inputId++;
        gdi.addTimeoutMilli(300, "AddRunnerInteractive", SportIdentCB).setExtra(inputId);
      }

      bool show = false;
      if (showDatabase() && bi.text.length() > 1) {
        auto rw = oe->getRunnerDatabase().getRunnerSuggestions(bi.text, 0, 20);
        if (!rw.empty()) {
          auto& ac = gdi.addAutoComplete(bi.id);
          ac.setAutoCompleteHandler(this);
          vector<AutoCompleteRecord> items = getRunnerAutoCompelete(oe->getRunnerDatabase(), rw, 0);
          ac.setData(items);
          ac.show();
          show = true;
        }
      }
      if (!show) {
        gdi.clearAutoComplete(bi.id);
      }
    }
  }
  else if (type == GUI_EVENT) {
    EventInfo ev = *(EventInfo*)data;
    if (ev.id == "AutoComplete") {
      pRunner r = oe->getRunner(runnerMatchedId, 0);
      if (r) {
        gdi.clearAutoComplete("");
        gdi.setInputFocus("OK1");
        gdi.setText("Runners", r->getName());
        gdi.setData("RunnerId", runnerMatchedId);
        if (gdi.hasWidget("Club"))
          gdi.setText("Club", r->getClub());
        inputId = -1;
        gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);

      }
    }
  }
  else if (type == GUI_FOCUS) {
    InputInfo& ii = *(InputInfo*)data;

    if (ii.id == "FinishTime") {
      if (ii.getExtraInt() == 1) {
        ii.setExtra(0);
        ii.setFgColor(colorDefault);
        //gdi.refreshFast();
        gdi.setText(ii.id, L"", true);
      }
    }
  }
  else if (type == GUI_TIMER) {
    TimerInfo& ti = dynamic_cast<TimerInfo&>(*data);
    if (ti.id == "RequestedStart") {
      if (requestStartTimeHandler) {
        requestStartTimeHandler->handle(gdi, ti, type);
      }
      return 0;
    }
    else if (ti.id == "TieCard") {
      runnerMatchedId = ti.getExtraInt();
      tieCard(gdi);
      return 0;
    }

    if (inputId != ti.getExtraInt())
      return 0;

    if (ti.id == "RunnerId") {
      const wstring& text = gdi.getText(ti.id);
      int nr = _wtoi(text.c_str());

      pRunner r = 0;
      if (nr > 0) {
        r = getRunnerByIdentifier(nr);
        if (r == 0) {
          r = oe->getRunnerByBibOrStartNo(text, true);
          if (r == 0) {
            // Seek where a card is already defined
            r = oe->getRunnerByBibOrStartNo(text, false);
          }
        }
      }

      if (nr == 0 && text.size() > 2) {
        unordered_set<int> f1, f2;
        r = oe->findRunner(text, 0, f1, f2);
      }
      if (r != 0) {
        gdi.setText("FindMatch", r->getCompleteIdentification(oRunner::IDType::OnlyThis), true);
        runnerMatchedId = r->getId();
      }
      else {
        gdi.setText("FindMatch", L"", true);
        runnerMatchedId = -1;
      }

      gdi.setInputStatus("TieOK", runnerMatchedId != -1);

      if (runnerMatchedId != -1 && gdi.getTextNo("CardNo") > 0 && gdi.isChecked("AutoTie"))
        tieCard(gdi);
    }
    else if (ti.id == "Manual") {
      const wstring& text = gdi.getText(ti.id);
      int nr = _wtoi(text.c_str());

      pRunner r = 0;
      if (nr > 0) {
        r = oe->getRunnerByBibOrStartNo(text, false);
        if (r == 0)
          r = oe->getRunnerByCardNo(nr, 0, oEvent::CardLookupProperty::ForReadout);
      }

      if (nr == 0 && text.size() > 2) {
        unordered_set<int> f1, f2;
        r = oe->findRunner(text, 0, f1, f2);
      }
      if (r != 0) {
        gdi.setText("FindMatch", r->getCompleteIdentification(oRunner::IDType::OnlyThis), true);
        runnerMatchedId = r->getId();
      }
      else {
        gdi.setText("FindMatch", L"", true);
        runnerMatchedId = -1;
      }
    }
    else if (ti.id == "AddRunnerInteractive") {
      const wstring& text = gdi.getText("Runners");
      int nr = _wtoi(text.c_str());

      pRunner r = 0;
      if (nr > 0) {
        r = oe->getRunnerByBibOrStartNo(text, true);
      }

      if (nr == 0 && text.size() > 2) {
        unordered_set<int> f1, f2;
        r = oe->findRunner(text, 0, f1, f2);
      }
      if (r != 0) {
        gdi.setText("FindMatch", lang.tl(L"X (press Ctrl+Space to confirm)#" + r->getCompleteIdentification(oRunner::IDType::OnlyThis)), true);
        runnerMatchedId = r->getId();
      }
      else {
        gdi.setText("FindMatch", L"", true);
        runnerMatchedId = -1;
      }
    }
  }
  else if (type == GUI_INPUTCHANGE) {

    InputInfo ii = *(InputInfo*)data;
    if (ii.id == "RunnerId") {
      inputId++;
      gdi.addTimeoutMilli(300, ii.id, SportIdentCB).setExtra(inputId);
    }
    else if (ii.id == "Manual") {
      inputId++;
      gdi.addTimeoutMilli(300, ii.id, SportIdentCB).setExtra(inputId);
    }
    else if (ii.id == "CardNo" && mode == SIMode::ModeAssignCards) {
      gdi.setInputStatus("TieOK", runnerMatchedId != -1);
    }
    else if (ii.id == "SI") {
      pRunner r = oe->getRunnerByCardNo(_wtoi(ii.text.c_str()), 0, oEvent::CardLookupProperty::ForReadout);
      if (testType == 0 && r) {
        if (r->getStartTime() > 0) {
          gdi.setText("Start", r->getStartTimeS());
          gdi.check("HasStart", false);
          int f = r->getStartTime() + (2800 + rand() % 1200) * timeConstSecond;
          gdi.setText("Finish", oe->getAbsTime(f));
          int c = r->getStartTime() - (120 + rand() % 30) * timeConstSecond;
          gdi.setText("Check", oe->getAbsTime(c));
        }
        
        pCourse pc = r->getCourse(false);
        if (pc) {
          if (testControls.size() < pc->getNumControls())
            testControls.resize(pc->getNumControls());
          for (int n = 0; n < pc->getNumControls(); n++) {
            if (pc->getControl(n)) {
              testControls[n] = pc->getControl(n)->getFirstNumber();
              if (n < NC) 
                gdi.setText("C" + itos(n + 1), testControls[n]);
            }
          }
        }
      }
    }
  }
  else if (type == GUI_INPUT) {
    InputInfo& ii = *(InputInfo*)data;
    if (ii.id == "FinishTime") {
      if (ii.text.empty()) {
        ii.setExtra(1);
        ii.setFgColor(colorGreyBlue);
        gdi.setText(ii.id, lang.tl("Aktuell tid"), true);
      }
      else if (oe->getRelativeTime(ii.text) > 0) {
        ii.setExtra(0);
      }
    }
    else if (ii.id == "CardNo") {
      int cardNo = gdi.getTextNo("CardNo");

      if (mode == SIMode::ModeAssignCards) {
        if (gdi.hasWidget("AutoTieRent") && gdi.isChecked("AutoTieRent")) {
          gdi.check("RentCardTie", oe->isHiredCard(gdi.getTextNo("CardNo")));
        }
        if (runnerMatchedId != -1 && gdi.isChecked("AutoTie") && cardNo > 0)
          gdi.addTimeoutMilli(50, "TieCard", SportIdentCB).setExtra(runnerMatchedId);
      }
      else if (cardNo > 0) {
        if (ii.changedInput() && oe->hasHiredCardData())
          gdi.check("RentCard", oe->isHiredCard(cardNo));

        if (gdi.getText("Name").empty()) {
          SICard sic(ConvertedTimeStatus::Hour24);
          sic.clear(0);
          sic.CardNumber = cardNo;

          entryCard(gdi, sic);
        }
      }
    }
    else if (ii.id[0] == '*') {
      int si = _wtoi(ii.text.c_str());

      pRunner r = oe->getRunner(ii.getExtraInt(), 0);
      r->synchronize();

      if (r && r->getCardNo() != si) {
        if (si == 0 || !oe->checkCardUsed(gdi, *r, si)) {
          r->setCardNo(si, false);
          r->setRentalCard(true);
          r->synchronize();
        }

        if (r->getCardNo())
          gdi.setText(ii.id, r->getCardNo());
        else
          gdi.setText(ii.id, L"");
      }
    }
    else if (ii.id == "SoundFile") {
      SND iType = SND(ii.getExtraInt());
      string name = typeFromSndType(iType);
      oe->setProperty(name.c_str(), ii.text);
    }
  }
  else if (type == GUI_INFOBOX) {
    DWORD loaded;
    if (!gdi.getData("SIPageLoaded", loaded))
      loadPage(gdi);
  }
  else if (type == GUI_CLEAR) {
    if (mode == SIMode::ModeEntry) {
      storedInfo.clear();
      storedInfo.storedName = gdi.getText("Name");
      storedInfo.storedCardNo = gdi.getText("CardNo");
      storedInfo.storedClub = gdi.hasWidget("Club") ? gdi.getText("Club") : L"";
      storedInfo.storedFee = gdi.getText("Fee", true);

      ListBoxInfo lbi;
      gdi.getSelectedItem("Class", lbi);
      storedInfo.storedClassId = lbi.data;
      storedInfo.storedPhone = gdi.getText("Phone", true);
      storedInfo.storedStartTime = gdi.getText("StartTime", true);

      storedInfo.dataA = gdi.getText("DataA", true);
      storedInfo.dataB = gdi.getText("DataB", true);
      storedInfo.textA = gdi.getText("TextA", true);
      storedInfo.nationality = gdi.getText("Nationality", true);
      storedInfo.birthDate = gdi.getText("BirthDate", true);
      storedInfo.rank = gdi.getText("Rank", true);
      storedInfo.sex = PersonSex(gdi.hasWidget("Sex") ? gdi.getSelectedItem("Sex").first : PersonSex::sUnknown);

      storedInfo.allStages = gdi.isChecked("AllStages");
      storedInfo.rentState = gdi.isChecked("RentCard");
      storedInfo.hasPaid = gdi.isChecked("Paid");
      storedInfo.payMode = gdi.hasWidget("PayMode") ? gdi.getSelectedItem("PayMode").first : 0;
    }
    else if (mode == SIMode::ModeReadOut && interactiveReadout && !activeSIC.empty()) {
      CardQueue.push_back(activeSIC);
      activeSIC.clear(nullptr);
    }
    return 1;
  }

  return 0;
}

void TabSI::writeDefaultHiredCards() {
  csvparser csv;
  try {
    oe->synchronizeList(oListId::oLPunchId);
    wstring def = getHiredCardDefault();
    auto hc = oe->getHiredCards();
    csvparser csv;
    csv.openOutput(def);
    for (int c : hc)
      csv.outputRow(itos(c));
    csv.closeOutput();
  }
  catch (std::exception&) {

  }
}

bool TabSI::anyActivePort() const {
  if (!gSI) return false;

  if (showTestingPanel)
    return true;

  list<int> ports;
  gSI->EnumrateSerialPorts(ports);
  for (int port : ports) {
    wstring s = L"COM" + itow(port);
    if (gSI->isPortOpen(s))
      return true;
  }
  if (gSI->isPortOpen(L"TCP"))
    return true;

  return false;
}

void TabSI::refillComPorts(gdioutput& gdi) {
  if (!gSI) return;

  list<int> ports;
  gSI->EnumrateSerialPorts(ports);

  gdi.clearList("ComPort");
  ports.sort();
  wchar_t bf[256];
  int active = 0;
  int inactive = 0;
  while (!ports.empty()) {
    int p = ports.front();
    swprintf_s(bf, 256, L"COM%d", p);

    if (gSI->isPortOpen(bf)) {
      gdi.addItem("ComPort", wstring(bf) + L" [OK]", p);
      active = p;
    }
    else {
      gdi.addItem("ComPort", bf, p);
      inactive = p;
    }

    ports.pop_front();
  }

  if (gSI->isPortOpen(L"TCP")) {
    active = 10000;
    gdi.addItem("ComPort", L"TCP [OK]", active);
  }
  else
    gdi.addItem("ComPort", L"TCP", 0);

  gdi.addItem("ComPort", lang.tl("Testning"), 9999);
  if (showTestingPanel)
    active = 9999;

  if (addTestPort)
    gdi.addItem("ComPort", L"TEST");

  if (active) {
    gdi.selectItemByData("ComPort", active);
    gdi.setText("StartSI", lang.tl("Koppla ifrån"));
  }
  else {
    gdi.selectItemByData("ComPort", inactive);
    gdi.setText("StartSI", lang.tl("Aktivera"));
  }
}

void TabSI::showReadPunches(gdioutput& gdi, vector<PunchInfo>& punches, set<string>& dates)
{
  char bf[64];
  int yp = gdi.getCY();
  int xp = gdi.getCX();
  dates.clear();
  vector<int> off = { 40, 100, 280, 360 };
  int margin = gdi.scaleLength(5);
  for (int& o : off)
    o = gdi.scaleLength(o);

  for (size_t k = 0; k < punches.size(); k++) {
    if (k % 5 == 0)
      yp += gdi.scaleLength(6);

    sprintf_s(bf, "%d.", int(k + 1));
    gdi.addStringUT(yp, xp, 0, bf);

    pRunner r = oe->getRunnerByCardNo(punches[k].card, punches[k].time, oEvent::CardLookupProperty::Any);
    sprintf_s(bf, "%d", punches[k].card);
    gdi.addStringUT(yp, xp + off[0], 0, bf, off[1]-off[0] + margin);

    if (r != 0)
      gdi.addStringUT(yp, xp + off[1], 0, r->getName(), off[2] - off[1] + margin);

    if (punches[k].date[0] != 0) {
      gdi.addStringUT(yp, xp + off[2], 0, punches[k].date, off[3] - off[2] + margin);
      dates.insert(punches[k].date);
    }
    if (punches[k].time > 0)
      gdi.addStringUT(yp, xp + off[3], 0, oe->getAbsTime(punches[k].time));
    else
      gdi.addStringUT(yp, xp + off[3], 0, makeDash(L"-"));

    yp += gdi.getLineHeight();
  }
}

void TabSI::showReadCards(gdioutput& gdi, vector<SICard>& cards)
{
  char bf[64];
  int yp = gdi.getCY();
  int xp = gdi.getCX();
  vector<int> off = { 40, 100, 300 };
  int margin = gdi.scaleLength(5);
  for (int& o : off)
    o = gdi.scaleLength(o);

  for (size_t k = 0; k < cards.size(); k++) {
    if (k % 5 == 0)
      yp += gdi.scaleLength(6);
    sprintf_s(bf, "%d.", int(k + 1));
    gdi.addStringUT(yp, xp, 0, bf);

    pRunner r = oe->getRunnerByCardNo(cards[k].CardNumber, 0, oEvent::CardLookupProperty::Any);
    sprintf_s(bf, "%d", cards[k].CardNumber);
    gdi.addStringUT(yp, xp + off[0], 0, bf, off[1] - off[0] + margin);

    if (r != 0)
      gdi.addStringUT(yp, xp + off[1], 0, r->getName(), off[2] - off[1] + margin);

    gdi.addStringUT(yp, xp + off[2], 0, formatTimeHMS(cards[k].FinishPunch.Time));
    yp += gdi.getLineHeight();
  }
}

SportIdent& TabSI::getSI(const gdioutput& gdi) {
  if (!gSI) {
    HWND hWnd = gdi.getHWNDMain();
    gSI = new SportIdent(hWnd, 0, true);
  }
  return *gSI;
}

bool TabSI::loadPage(gdioutput& gdi) {
  gdi.clearPage(true);
  printErrorShown = false;
  gdi.pushX();
  gdi.selectTab(tabId);
  oe->checkDB();
  gdi.setData("SIPageLoaded", 1);

  if (!gSI) 
    getSI(gdi);

  if (firstLoadedAfterNew) {
    if (oe->getNumRunners() == 0)
      interactiveReadout = true;
    else if (oe->isClient())
      interactiveReadout = false;

    firstLoadedAfterNew = false;
  }

  if (showTestingPanel) {
    RECT rc;
    rc.left = gdi.getCX();
    rc.top = gdi.getCY();

    gdi.setCX(gdi.getCX() + gdi.scaleLength(4));
    gdi.setCY(gdi.getCY() + gdi.scaleLength(4));

    gdi.addString("", fontMediumPlus, "Inmatning Testning");

    gdi.fillRight();
    gdi.pushX();

    gdi.addSelection("TestType", 100, 100, SportIdentCB, L"Typ:");
    gdi.addItem("TestType", lang.tl("Avläsning"), 0);
    gdi.addItem("TestType", lang.tl("Mål"), 2);
    gdi.addItem("TestType", lang.tl("Start"), 1);
    gdi.addItem("TestType", lang.tl("Check"), 3);
    gdi.addItem("TestType", lang.tl("Radio"), 10);

    gdi.selectItemByData("TestType", testType);

    gdi.addInput("SI", testCardNumber > 0 ? itow(testCardNumber) : L"", 10, SportIdentCB, L"Bricknummer:");
       
    if (testType == 0) {
      int s = timeConstHour + (rand() % 60) * timeConstMinute;
      int f = s + timeConstHour / 2 + ((rand() % 3600) / 3) * timeConstSecond;
      wstring ss = oe->getAbsTime(s);
      wstring fs = oe->getAbsTime(f);
      wstring cs = oe->getAbsTime(s - (60 + rand() % 60) * timeConstSecond);

      if (!testStartTime.empty())
        ss = testStartTime;
      if (!testFinishTime.empty())
        fs = testFinishTime;
      if (!testCheckTime.empty())
        cs = testCheckTime;

      gdi.setCX(gdi.getCX() + gdi.getLineHeight());

      gdi.dropLine(1.6);
      gdi.addCheckbox("HasCheck", "", nullptr, useTestCheck);
      gdi.dropLine(-1.6);
      gdi.setCX(gdi.getCX() - gdi.getLineHeight());
      gdi.addInput("Check", cs, 6, 0, L"Check:");

      gdi.dropLine(1.6);
      gdi.addCheckbox("HasStart", "", nullptr, useTestStart);
      gdi.dropLine(-1.6);
      gdi.setCX(gdi.getCX() - gdi.getLineHeight());
      gdi.addInput("Start", ss, 6, 0, L"Start:");

      gdi.dropLine(1.6);
      gdi.addCheckbox("HasFinish", "", nullptr, useTestFinish);
      gdi.dropLine(-1.6);
      gdi.setCX(gdi.getCX() - gdi.getLineHeight());

      gdi.addInput("Finish", fs, 6, 0, L"Mål:");
      gdi.addSelection("NC", 45, 200, SportIdentCB, L"Stämplingar:");
      const int src[11] = { 33, 34, 45, 50, 36, 38, 59, 61, 62, 67, 100 };

      for (int i = 0; i < 32; i++)
        gdi.addItem("NC", itow(i), i);

      gdi.selectItemByData("NC", NC);

      for (int i = 0; i < NC; i++) {
        int level = min(i, NC - i) / 5;
        int c;
        if (i < NC / 2) {
          int ix = i % 6;
          c = src[ix] + level * 10;
          if (c == 100)
            c = 183;
        }
        else {
          int ix = 10 - (NC - i - 1) % 5;
          c = src[ix] + level * 10;
        }

        if (i < testControls.size())
          c = testControls[i];

        gdi.addInput("C" + itos(i + 1), itow(c), 3, 0, L"#C" + itow(i + 1));
      }

      gdi.dropLine();
      gdi.fillDown();
      gdi.addButton("Save", "Spara", SportIdentCB);
      gdi.popX();

    }
    else {
      int p = timeConstHour + (rand() % 60) * timeConstMinute;
      wstring ps = oe->getAbsTime(p);
      
      if (!testPunchTime.empty())
        ps = testPunchTime;
      
      if (testType == 10) 
        gdi.addInput("ControlNumber", itow(testRadioNumber), 6, 0, L"Kontroll:");

      gdi.addInput("PunchTime", ps, 6, 0, L"Tid:");

      gdi.dropLine();
      gdi.fillDown();
      gdi.addButton("SaveP", "Spara", SportIdentCB);
      gdi.popX();
    }
    rc.right = gdi.getWidth();
    rc.bottom = gdi.getCY();
    gdi.addRectangle(rc, GDICOLOR::colorLightMagenta);
    gdi.dropLine();
  }

  int xb = gdi.getCX();
  int yb = gdi.getCY();

  if (!oe->isKiosk()) {
    gdi.addString("", boldLarge, "SportIdent");
    gdi.dropLine();

    gdi.pushX();
    gdi.fillRight();
    gdi.addSelection("ComPort", 120, 200, SportIdentCB);
    gdi.addButton("StartSI", "#Aktivera+++", SportIdentCB);
    gdi.addButton("SIInfo", "Info", SportIdentCB);

    refillComPorts(gdi);

    gdi.addButton("AutoDetect", "Sök och starta automatiskt...", SportIdentCB);
    gdi.addButton("PrinterSetup", "Skrivarinställningar...", SportIdentCB, "Skrivarinställningar för sträcktider och startbevis");

    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(2.2);

    // Update pos
    xb = gdi.getCX();
    yb = gdi.getCY();

    if (!oe->empty()) {
      gdi.setCX(xb + gdi.scaleLength(10));
      gdi.setCY(yb + gdi.scaleLength(10));

      if (!lockedFunction) {
        gdi.fillRight();
        gdi.addString("", fontMediumPlus, "Funktion:");
        gdi.addSelection("ReadType", 200, 200, SportIdentCB);
        auto addItem = [this, &gdi](SIMode mode) {
          gdi.addItem("ReadType", lang.tl(modeName[mode]), int(mode));
        };
        addItem(SIMode::ModeReadOut);
        addItem(SIMode::ModeEntry);
        addItem(SIMode::ModeRequestStartTime);
        addItem(SIMode::ModeAssignCards);
        addItem(SIMode::ModeCheckCards);
        addItem(SIMode::ModeRegisterCards);
        addItem(SIMode::ModeCardData);

        gdi.selectItemByData("ReadType", int(mode));

        gdi.dropLine(-0.1);
        gdi.addButton("LockFunction", "Lås funktion", SportIdentCB);
        readoutFunctionX = gdi.getCX();
        readoutFunctionY = gdi.getCY();
        gdi.dropLine(0.3);

        gdi.addCheckbox("PlaySound", "Ljud", SportIdentCB, oe->getPropertyInt("PlaySound", 1) != 0,
          "Spela upp ett ljud för att indikera resultatet av brickavläsningen.");
        gdi.dropLine(-0.3);

        gdi.addButton("SoundChoice", "Ljudval...", SportIdentCB);

        gdi.dropLine(0.3);

        updateReadoutFunction(gdi);

        gdi.dropLine(2);
        gdi.setCX(xb + gdi.scaleLength(10));
      }
      else {
        gdi.addString("", fontMediumPlus, lang.tl(modeName[mode]));
        gdi.fillRight();
        gdi.addButton("UnlockFunction", "Lås upp", SportIdentCB);
        gdi.dropLine(0.2);
      }
    }
    else {
      mode = SIMode::ModeCardData;
    }
  }
  else { //Kiosk
    gdi.pushX();
  }

  optionBarPosY = gdi.getCY();
  optionBarPosX = gdi.getCX();
  check_toolbar_xb = xb;
  check_toolbar_yb = yb;

  gdi.popX();
  gdi.setRestorePoint("SIPageLoaded");

  if (mode == SIMode::ModeReadOut) {
    showReadoutMode(gdi);
  }
  else if (mode == SIMode::ModeAssignCards) {
    showAssignCard(gdi, true);
  }
  else if (mode == SIMode::ModeEntry) {
    checkBoxToolBar(gdi, { CheckBox::UseDB, CheckBox::PrintStart, CheckBox::ExtraDataFields });
    entryTips(gdi);
    generateEntryLine(gdi, 0);
  }
  else if (mode == SIMode::ModeCardData) {
    showModeCardData(gdi);
  }
  else if (mode == SIMode::ModeCheckCards) {
    showCheckCardStatus(gdi, "init");
  }
  else if (mode == SIMode::ModeRegisterCards) {
    showRegisterHiredCards(gdi);
  }
  else if (mode == SIMode::ModeRequestStartTime) {
    showRequestStartTime(gdi);
  }
  // Unconditional clear
  activeSIC.clear(0);

  checkMoreCardsInQueue(gdi);
  gdi.refresh();
  return true;
}

void TabSI::checkBoxToolBar(gdioutput& gdi, const set<CheckBox>& items) const {
  gdi.pushX();
  gdi.setCY(optionBarPosY);
  gdi.setCX(optionBarPosX);
  gdi.fillRight();

  if (!items.empty())
    gdi.dropLine(0.2);

  if (items.count(CheckBox::Interactive))
    gdi.addCheckbox("Interactive", "Interaktiv inläsning", SportIdentCB, interactiveReadout);

  if (items.count(CheckBox::UseDB) && (oe->empty() || oe->useRunnerDb()))
    gdi.addCheckbox("Database", "Använd löpardatabasen", SportIdentCB, useDatabase);

  if (items.count(CheckBox::PrintSplits))
    gdi.addCheckbox("PrintSplits", "Sträcktidsutskrift[check]", SportIdentCB, printSplits);

  if (items.count(CheckBox::PrintStart))
    gdi.addCheckbox("StartInfo", "Startbevis", SportIdentCB, printStartInfo, "Skriv ut startbevis för deltagaren");

  if (items.count(CheckBox::Manual))
    gdi.addCheckbox("UseManualInput", "Manuell inmatning", SportIdentCB, manualInput);

  if (items.count(CheckBox::SeveralTurns))
    gdi.addCheckbox("MultipleStarts", "Flera starter per deltagare", SportIdentCB, multipleStarts,
      "info:multiple_start");

  if (items.count(CheckBox::AutoTie))
    gdi.addCheckbox("AutoTie", "Knyt automatiskt efter inläsning", SportIdentCB, oe->getPropertyInt("AutoTie", 1) != 0);

  if (items.count(CheckBox::AutoTieRent) && oe->hasHiredCardData()) {
    gdi.addCheckbox("AutoTieRent", "Automatisk hyrbrickshantering genom registrerade hyrbrickor", SportIdentCB, oe->getPropertyInt("AutoTieRent", 1) != 0);
  }

  if (items.count(CheckBox::ExtraDataFields)) {
    gdi.dropLine(-0.2);
    gdi.addButton("EditEntryFields", "Extra datafält", SportIdentCB); // Change
  }
  if (!items.empty())
    gdi.dropLine(2);

  gdi.fillDown();

  if (!oe->empty()) {
    RECT rc = { check_toolbar_xb, check_toolbar_yb, gdi.getWidth(), gdi.getHeight() };
    gdi.addRectangle(rc, colorLightBlue);
  }
  gdi.popX();
  gdi.dropLine(1.5);

  gdi.fillDown();
  //  gdi.popY();
  gdi.popX();
}

void TabSI::showReadoutMode(gdioutput& gdi) {
  if (!oe->empty())
    checkBoxToolBar(gdi, { CheckBox::Interactive, CheckBox::UseDB, CheckBox::PrintSplits,
                          CheckBox::SeveralTurns, CheckBox::Manual });
  else {
    checkBoxToolBar(gdi, { CheckBox::UseDB, CheckBox::PrintSplits });
  }

  gdi.dropLine(0.5);
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("Import", "Importera från fil...", SportIdentCB);
  gdi.addButton("ReadoutWindow", "Öppna avläsningsfönster", SportIdentCB, "info:readoutwindow");

  if (oe->empty() || !getSI(gdi).isAnyOpenUnkownUnit())
    gdi.dropLine(3);
  else {
    gdi.setRestorePoint("Mapping");
    gdi.setData("ShowControlMapping", 1);
    gdi.fillRight();
    int cx = gdi.getCX() + gdi.scaleLength(10);
    int cy = gdi.getCY();
    gdi.setCX(cx);
    gdi.setCY(cy);
    gdi.addString("", boldText, "Tolkning av radiostämplingar med okänd typ");
    int maxX = gdi.getCX();
    
    gdi.setCX(cx);
    gdi.dropLine(1.1);
    auto mappings = getSI(gdi).getSpecialMappings();
    wstring check, start, finish;
    auto add = [](int code, wstring& dst) {
      if (!dst.empty())
        dst += L", ";
      dst += itow(code);
    };

    auto complete = [](wstring& dst) {
      if (dst.empty())
        dst = makeDash(L"-");
    };

    for (auto& v : mappings) {
      if (v.second == oPunch::SpecialPunch::PunchFinish)
        add(v.first, finish);
      else if (v.second == oPunch::SpecialPunch::PunchStart)
        add(v.first, start);
      else if (v.second == oPunch::SpecialPunch::PunchCheck)
        add(v.first, check);
    }
    complete(finish);
    complete(start);
    complete(check);
    gdi.addString("", 1, "Enhetskod:");
    
    if (!check.empty()) {
      gdi.addString("", 0, "Check:");
      gdi.setCX(gdi.getCX() - gdi.scaleLength(2));
      gdi.addStringUT(0, check);
    }
    if (!start.empty()) {
      gdi.addString("", 0, "Start:");
      gdi.setCX(gdi.getCX() - gdi.scaleLength(2));
      gdi.addStringUT(0, start);
    }
    if (!finish.empty()) {
      gdi.addString("", 0, "Mål:");
      gdi.setCX(gdi.getCX() - gdi.scaleLength(2));
      gdi.addStringUT(0, finish);
    }
    maxX = max(maxX, gdi.getCX());
    gdi.setCX(maxX);
    gdi.setCY(cy);

    gdi.addButton("ChangeMapping", "Ändra", SportIdentCB);
    maxX = max(maxX, gdi.getCX()) + gdi.scaleLength(10);

    RECT rc;
    rc.left = cx - gdi.scaleLength(5);
    rc.top = cy - gdi.scaleLength(10);
    rc.right = maxX;
    rc.bottom = gdi.getHeight() + gdi.scaleLength(4);//gdi.getCY() + gdi.scaleLength(5);
    gdi.fillDown();
    gdi.addRectangle(rc, GDICOLOR::colorLightCyan);
    gdi.dropLine(1);
  }

  gdi.fillDown();
  gdi.popX();

  gdi.setRestorePoint("Help");
  gdi.fillRight();
  gdi.addString("", 10, "info:readoutbase");
  gdi.fillDown();
  gdi.addString("", 10, "info:readoutmore");
  gdi.popX();
  gdi.setCY(gdi.getHeight());
  gdi.dropLine(0.5);

  renderReadCard(gdi, 100);

  if (gdi.isChecked("UseManualInput"))
    showManualInput(gdi);
}

void TabSI::updateReadoutFunction(gdioutput& gdi) {
  bool hide = mode != SIMode::ModeReadOut;
  gdi.hideWidget("SoundChoice", hide);
  gdi.hideWidget("PlaySound", hide);
}

void InsertSICard(gdioutput& gdi, SICard& sic) {
  TabSI& tsi = dynamic_cast<TabSI&>(*gdi.getTabs().get(TSITab));
  tsi.insertSICard(gdi, sic);
}

pRunner TabSI::autoMatch(const SICard& sic, pRunner db_r)
{
  assert(useDatabase);
  //Look up in database.
  if (!db_r)
    db_r = gEvent->dbLookUpByCard(sic.CardNumber);

  pRunner r = 0;

  if (db_r) {
    r = gEvent->getRunnerByName(db_r->getName(), db_r->getClub());

    if (!r) {
      vector<pClass> classes;
      int dist = gEvent->findBestClass(sic, classes);

      if (classes.size() == 1 && dist >= -1 && dist <= 1) { //Almost perfect match found. Assume it is it!
        r = gEvent->addRunnerFromDB(db_r, classes[0]->getId(), true);
        r->setCardNo(sic.CardNumber, false);

        if (oe->isHiredCard(sic.CardNumber)) {
          r->setRentalCard(true);
        }
      }
      else r = 0; //Do not assume too much...
    }
  }
  if (r && r->getCard() == 0)
    return r;
  else return 0;
}

void TabSI::insertSICard(gdioutput& gdi, SICard& sic)
{
  wstring msg;
  try {
    insertSICardAux(gdi, sic);
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

  if (!msg.empty())
    gdi.alert(msg);
}

void TabSI::insertSICardAux(gdioutput& gdi, SICard& sic)
{
  if (oe->isReadOnly()) {
    gdi.makeEvent("ReadCard", "insertSICard", sic.CardNumber, 0, true);
    return;
  }

  DWORD loaded;
  bool pageLoaded = gdi.getData("SIPageLoaded", loaded);

  if (pageLoaded && manualInput && mode == SIMode::ModeReadOut)
    gdi.restore("ManualInput");

  if (!pageLoaded && !insertCardNumberField.empty()) {
    if (gdi.insertText(insertCardNumberField, itow(sic.CardNumber)))
      return;
  }

  if (mode == SIMode::ModeAssignCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö", L"");
    }
    else assignCard(gdi, sic);
    return;
  }
  else if (mode == SIMode::ModeEntry) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö", L"");
    }
    else entryCard(gdi, sic);
    return;
  }
  if (mode == SIMode::ModeCheckCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö", L"");
    }
    else
      checkCard(gdi, sic, true);
    return;
  }
  else if (mode == SIMode::ModeRequestStartTime) {
    requestStartTime(gdi, sic);
    return;
  }
  else if (mode == SIMode::ModeRegisterCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö", L"");
    }
    else {
      registerHiredCard(gdi, sic);
    }
    return;
  }
  else if (mode == SIMode::ModeCardData) {
    if (sic.convertedTime == ConvertedTimeStatus::Hour12) {
      int locTime = getLocalAbsTime();
      int st = -1;
      if (sic.StartPunch.Code != -1)
        st = sic.StartPunch.Time;
      else if (sic.nPunch > 0 && sic.Punch[0].Code != -1)
        st = sic.Punch[0].Time;

      if (st == -1)
        st = (locTime + timeConstHour * 20) % (12 * timeConstHour);
      else {
        // We got a start time. Calculate running time
        if (sic.FinishPunch.Code != -1) {
          int rt = (sic.FinishPunch.Time - st + 12 * timeConstHour) % (12 * timeConstHour);

          // Adjust to local time at start;
          locTime = (locTime - rt + (24 * timeConstHour)) % (24 * timeConstHour);
        }
      }
      int zt1 = (st + 23 * timeConstHour) % (24 * timeConstHour);
      int zt2 = st + 11 * timeConstHour;
      int d1 = min(abs(locTime - zt1), abs(locTime - zt1 + timeConstHour * 24));
      int d2 = min(abs(locTime - zt2), abs(locTime - zt2 + timeConstHour * 24));

      if (d1 < d2)
        sic.analyseHour12Time(zt1);
      else
        sic.analyseHour12Time(zt2);
    }

    // Write read card to log
    logCard(gdi, sic);

    bool first = savedCards.size() == numSavedCardsOnCmpOpen;
    savedCards.push_back(make_pair(savedCardUniqueId++, sic));

    if (printSplits) {
      generateSplits(savedCards.back().first, gdi);
    }

    gdioutput* gdi_settings = getExtraWindow("readout_view", true);
    if (gdi_settings) {
      showReadoutStatus(*gdi_settings, nullptr, nullptr, &savedCards.back().second, L"");
    }

    if (savedCards.size() > 1 && pageLoaded) {
      RECT rc = { 30, gdi.getCY(), gdi.scaleLength(250), gdi.getCY() + 3 };
      gdi.addRectangle(rc);
    }

    if (pageLoaded) {
      gdi.enableInput("CreateCompetition", true);
      printCard(gdi, 0, savedCards.back().first, nullptr, false);
      gdi.dropLine();
      gdi.refreshFast();
      gdi.scrollToBottom();
    }

    if (first && !oe->empty())
      gdi.alert(L"warn:printmodeonly");

    return;
  }

  gEvent->synchronizeList({ oListId::oLCardId, oListId::oLRunnerId });

  if (sic.punchOnly) {
    processPunchOnly(gdi, sic);
    return;
  }
  pRunner r;
  if (sic.runnerId == 0) {
    r = oe->getRunnerByCardNo(sic.CardNumber, 0, oEvent::CardLookupProperty::ForReadout);

    if (!r && multipleStarts && !oe->isCardRead(sic)) {
      // Convert punch times to relative times.
      oe->convertTimes(nullptr, sic);
      int time = sic.getFirstTime();
      pRunner rOld = oe->getRunnerByCardNo(sic.CardNumber, time, oEvent::CardLookupProperty::Any);

      if (rOld) {
        // New entry
        vector<pClass> classes;
        oe->findBestClass(sic, classes);
        int classId = rOld->getClassId(false);
        if (classes.size() == 1)
          classId = classes[0]->getId();

        wstring given = rOld->getGivenName();
        wstring family = rOld->getFamilyName();
        size_t ep = family.find_last_of(')');
        size_t sp = family.find_last_of('(');

        int num = 1;
        if (ep != string::npos && sp != string::npos && sp + 1 < ep) {
          num = _wtoi(family.data() + sp + 1);
          if (num > 0) {
            family = trim(family.substr(0, ep - 2));
          }
        }
        if (classId == rOld->getClassId(false))
          family +=  + L" (" + itow(num + 1) + L")";

        r = oe->addRunner(L"tmp", rOld->getClub(),
          classId, sic.CardNumber, rOld->getBirthDate(), false);

        r->setName(family + L", " + given, true);
        r->setFlag(oAbstractRunner::TransferFlags::FlagNoDatabase, true);
      }
    }
  }
  else {
    r = gEvent->getRunner(sic.runnerId, 0);
    sic.CardNumber = r->getCardNo();
  }

  bool readBefore = sic.runnerId == 0 ? gEvent->isCardRead(sic) : false;

  bool sameCardNewRace = !readBefore && r && r->getCard();

  if (!pageLoaded) {
    if (sic.runnerId != 0)
      throw meosException("Internal error");
    //SIPage not loaded...

    if (!r && showDatabase())
      r = autoMatch(sic, 0);

    // Assign a class if not already done
    autoAssignClass(r, sic);

    if (interactiveReadout) {
      if (r && r->getClassId(false) && !readBefore && !sameCardNewRace) {
        //We can do a silent read-out...
        processCard(gdi, r, sic, true);
        return;
      }
      else {
        CardQueue.push_back(sic);
        gdi.addInfoBox("SIREAD", L"info:readout_action#" + gEvent->getCurrentTimeS() + L"#" + itow(sic.CardNumber),
                       L"", BoxStyle::Header, 0, SportIdentCB);
        playReadoutSound(SND::ActionNeeded);
        return;
      }
    }
    else {
      if (!readBefore) {
        if (r && r->getClassId(false) && !sameCardNewRace)
          processCard(gdi, r, sic, true);
        else {
          processUnmatched(gdi, sic, true);
        }
      }
      else
        gdi.addInfoBox("SIREAD", L"Brickan redan inläst.", L"", BoxStyle::Header, 0, SportIdentCB);
    }
    return;
  }
  else if (activeSIC.CardNumber) {
    //We are already in interactive mode...

    // Assign a class if not already done
    autoAssignClass(r, sic);

    if (r && r->getClassId(false) && !readBefore && !sameCardNewRace) {
      //We can do a silent read-out...
      processCard(gdi, r, sic, true);
      return;
    }

    wstring name;
    if (r)
      name = L" (" + r->getName() + L")";

    name = itow(sic.CardNumber) + name;
    CardQueue.push_back(sic);
    gdi.addInfoBox("SIREAD", L"info:readout_queue#" + gEvent->getCurrentTimeS() + L"#" + name, L"");
    playReadoutSound(SND::ActionNeeded);
    return;
  }

  if (readBefore) {
    //We stop processing of new cards, while working...
    // Thus cannot be in interactive mode
    activeSIC = sic;
    wchar_t bf[256];

    if (interactiveReadout) {
      swprintf_s(bf, L"SI X är redan inläst. Ska den läsas in igen?#%d", sic.CardNumber);

      if (!gdi.ask(bf)) {
        if (printSplits) {
          pRunner runner = getRunnerForCardSplitPrint(sic);
          if (runner)
            generateSplits(runner, gdi);
        }
        activeSIC.clear(0);
        if (manualInput)
          showManualInput(gdi);
        checkMoreCardsInQueue(gdi);
        return;
      }
    }
    else {
      if (printSplits) {
        pRunner runner = getRunnerForCardSplitPrint(sic);

        if (runner)
          generateSplits(runner, gdi);
      }

      gdi.dropLine();
      swprintf_s(bf, L"SI X är redan inläst. Använd interaktiv inläsning om du vill läsa brickan igen.#%d", sic.CardNumber);
      gdi.addString("", 0, bf).setColor(colorRed);
      gdi.dropLine();
      gdi.scrollToBottom();
      gdi.refresh();
      activeSIC.clear(0);
      playReadoutSound(SND::ActionNeeded);
      checkMoreCardsInQueue(gdi);
      return;
    }
  }

  pRunner db_r = 0;
  if (sic.runnerId == 0) {
    if (!readBefore)
      r = gEvent->getRunnerByCardNo(sic.CardNumber, 0, oEvent::CardLookupProperty::ForReadout);
    else
      r = getRunnerForCardSplitPrint(sic);

    if (!r && showDatabase()) {
      //Look up in database.
      db_r = gEvent->dbLookUpByCard(sic.CardNumber);
      if (db_r)
        r = autoMatch(sic, db_r);
    }
  }

  // If there is no class, auto create
  if (interactiveReadout && oe->getNumClasses() == 0) {
    gdi.fillDown();
    gdi.dropLine();
    gdi.addString("", 1, "Skapar saknad klass").setColor(colorGreen);
    gdi.dropLine();
    pCourse pc = nullptr;
    if (!oe->getMeOSFeatures().hasFeature(MeOSFeatures::NoCourses)) {
      pc = gEvent->addCourse(lang.tl("Okänd klass"));
      for (unsigned i = 0; i < sic.nPunch; i++)
        pc->addControl(sic.Punch[i].Code);
    }
    gEvent->addClass(lang.tl(L"Okänd klass"), pc ? pc->getId() : 0)->setType(L"tmp");
  }

  // Assign a class if not already done
  autoAssignClass(r, sic);

  if (r && r->getClassId(false) && !r->getCard()) {
    SICard copy = sic;
    activeSIC.clear(0);
    processCard(gdi, r, copy); //Everyting is OK
    if (gdi.isChecked("UseManualInput"))
      showManualInput(gdi);
  }
  else {
    if (interactiveReadout) {
      startInteractive(gdi, sic, r, db_r);
    }
    else {
      SICard copy = sic;
      activeSIC.clear(0);
      processUnmatched(gdi, sic, !pageLoaded);
    }
  }
}

pRunner TabSI::getRunnerForCardSplitPrint(const SICard& sic) const {
  pRunner runner = 0;
  vector<pRunner> out;
  oe->getRunnersByCardNo(sic.CardNumber, false, oEvent::CardLookupProperty::SkipNoStart, out);
  for (pRunner r : out) {
    if (!r->getCard())
      continue;
    if (runner == 0)
      runner = r;
    else {
      if (runner->getFinishTime() < r->getFinishTime())
        runner = r; // Take the last finisher
//      int nPunchBest = runner->getCard()->getNumControlPunches(oPunch::PunchStart, oPunch::PunchFinish);
//      int nPunchCurrent = r->getCard()->getNumControlPunches(oPunch::PunchStart, oPunch::PunchFinish);

//      if (abs(int(nPunchCurrent - activeSIC.nPunch)) < abs(int(nPunchBest - activeSIC.nPunch)))
      //  runner = r;
    }
  }
  return runner;
}

void TabSI::startInteractive(gdioutput& gdi, const SICard& sic, pRunner r, pRunner db_r)
{
  if (!r) {
    gdi.setRestorePoint();
    gdi.fillDown();
    gdi.dropLine();
    char bf[256];
    sprintf_s(bf, 256, "SI X inläst. Brickan är inte knuten till någon löpare (i skogen).#%d", sic.CardNumber);

    gdi.dropLine();
    gdi.addString("", 1, bf);
    gdi.dropLine();
    gdi.fillRight();
    gdi.pushX();

    gdi.addCombo("Runners", 300, 300, SportIdentCB, L"Namn:");
    gEvent->fillRunners(gdi, "Runners", false, oEvent::RunnerFilterOnlyNoResult);

    if (db_r) {
      gdi.setText("Runners", db_r->getName()); //Data from DB
    }
    else if (sic.firstName[0] || sic.lastName[0]) { //Data from SI-card
      gdi.setText("Runners", wstring(sic.lastName) + L", " + sic.firstName);
    }
    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
      gdi.addCombo("Club", 200, 300, 0, L"Klubb:").setHandler(&directEntryGUI);
      gEvent->fillClubs(gdi, "Club");

      if (db_r)
        gdi.setText("Club", db_r->getClub()); //Data from DB
    }
    if (gdi.getText("Runners").empty() || !gdi.hasWidget("Club"))
      gdi.setInputFocus("Runners");
    else
      gdi.setInputFocus("Club");

    //Process this card.
    activeSIC = sic;
    gdi.dropLine();
    gdi.setRestorePoint("restOK1");
    gdi.addButton("OK1", "OK", SportIdentCB).setDefault();
    gdi.addButton("SaveUnpaired", "Spara oparad bricka", SportIdentCB);
    gdi.fillDown();
    gdi.addButton("Cancel", "Avbryt inläsning", SportIdentCB).setCancel();
    gdi.popX();
    gdi.addString("FindMatch", 0, "").setColor(colorGreen);
    gdi.registerEvent("AutoComplete", SportIdentCB).setKeyCommand(KC_AUTOCOMPLETE);
    gdi.dropLine();
    gdi.scrollToBottom();
    gdi.setOnClearCb("si", SportIdentCB);
    gdi.refresh();
  }
  else {
    //Process this card.
    activeSIC = sic;

    //No class. Select...
    gdi.setRestorePoint();

    wchar_t bf[256];
    swprintf_s(bf, 256, L"SI X inläst. Brickan tillhör Y som saknar klass.#%d#%s",
      sic.CardNumber, r->getName().c_str());

    gdi.dropLine();
    gdi.addString("", 1, bf);

    gdi.fillRight();
    gdi.pushX();

    gdi.addSelection("Classes", 200, 300, 0, L"Klass:");
    gEvent->fillClasses(gdi, "Classes", {}, oEvent::extraNone, oEvent::filterNone);
    gdi.setInputFocus("Classes");
    //Find matching class...
    vector<pClass> classes;
    gEvent->findBestClass(sic, classes);
    if (classes.size() > 0)
      gdi.selectItemByData("Classes", classes[0]->getId());

    gdi.dropLine();

    gdi.addButton("OK4", "OK", SportIdentCB).setDefault();
    gdi.fillDown();

    gdi.popX();
    gdi.setData("RunnerId", r->getId());
    gdi.scrollToBottom();
    gdi.setOnClearCb("si", SportIdentCB);
    gdi.refresh();
  }
}

// Insert card without converting times and with/without runner
void TabSI::processInsertCard(const SICard& sic)
{
  if (oe->isCardRead(sic))
    return;

  pRunner runner = oe->getRunnerByCardNo(sic.CardNumber, 0, oEvent::CardLookupProperty::ForReadout);
  pCard card = oe->allocateCard(runner);
  card->setReadId(sic);
  card->setCardNo(sic.CardNumber);
  card->setMeasuredVoltage(sic.miliVolt);
  oCard::PunchOrigin origin = sic.isDebugCard ? oCard::PunchOrigin::Manual : oCard::PunchOrigin::Original;

  if (sic.CheckPunch.Code != -1)
    card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time, 0, sic.CheckPunch.Code, origin);

  if (sic.StartPunch.Code != -1)
    card->addPunch(oPunch::PunchStart, sic.StartPunch.Time, 0, sic.StartPunch.Code, origin);

  for (unsigned i = 0; i < sic.nPunch; i++)
    card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time, 0, 0, origin);

  if (sic.FinishPunch.Code != -1)
    card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time, 0, sic.FinishPunch.Code, origin);

  //Update to SQL-source
  card->synchronize();

  if (runner) {
    vector<int> mp;
    runner->addCard(card, mp);
  }
}

bool TabSI::processUnmatched(gdioutput& gdi, const SICard& csic, bool silent) {
  SICard sic(csic);
  StoredReadout rout;
  pCard card = gEvent->allocateCard(0);

  card->setReadId(csic);
  card->setCardNo(csic.CardNumber);
  card->setMeasuredVoltage(csic.miliVolt);

  rout.info = lang.tl(L"Okänd bricka ") + itow(sic.CardNumber) + L".";

  // Write read card to log
  logCard(gdi, sic);

  // Convert punch times to relative times.
  gEvent->convertTimes(nullptr, sic);
  oCard::PunchOrigin origin = sic.isDebugCard ? oCard::PunchOrigin::Manual : oCard::PunchOrigin::Original;

  if (sic.CheckPunch.Code != -1)
    card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time, 0, sic.CheckPunch.Code, origin);

  if (sic.StartPunch.Code != -1)
    card->addPunch(oPunch::PunchStart, sic.StartPunch.Time, 0, sic.StartPunch.Code, origin);

  for (unsigned i = 0; i < sic.nPunch; i++)
    card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time, 0, 0, origin);

  if (sic.FinishPunch.Code != -1)
    card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time, 0, sic.FinishPunch.Code, origin);
  else
    rout.warnings += lang.tl("Målstämpling saknas.");

  //Update to SQL-source
  card->synchronize();

  gdioutput* gdi_settings = getExtraWindow("readout_view", true);
  if (gdi_settings) {
    showReadoutStatus(*gdi_settings, nullptr, card, nullptr, L"");
  }

  rout.color = colorLightRed;

  if (!silent) {
    rout.render(gdi, rout.computeRC(gdi));

    if (gdi.isChecked("UseManualInput"))
      showManualInput(gdi);

    gdi.scrollToBottom();
  }
  else {
    gdi.addInfoBox("SIINFO", L"#" + rout.info, L"", BoxStyle::Header, 10000);
  }
  readCards.push_back(std::move(rout));
  gdi.makeEvent("DataUpdate", "sireadout", 0, 0, true);
  playReadoutSound(SND::ActionNeeded);
  checkMoreCardsInQueue(gdi);
  return true;
}

bool TabSI::processCard(gdioutput& gdi, pRunner runner, const SICard& csic, bool silent) {
  if (!runner)
    return false;
  if (runner->getClubId())
    lastClubId = runner->getClubId();

  runner = runner->getMatchedRunner(csic);

  int lh = gdi.getLineHeight();
  //Update from SQL-source
  runner->synchronize();

  if (!runner->getClassId(false))
    runner->setClassId(gEvent->addClass(lang.tl(L"Okänd klass"))->getId(), true);

  // Choose course from pool
  pClass cls = runner->getClassRef(false);
  pClass pclass = runner->getClassRef(true);
  if (cls && cls->hasCoursePool()) {
    unsigned leg = runner->legToRun();

    if (leg < cls->getNumStages()) {
      pCourse c = cls->selectCourseFromPool(leg, csic);
      if (c)
        runner->setCourseId(c->getId());
    }
  }
  else if (pclass && pclass != cls && pclass->hasCoursePool()) {
    pCourse c = pclass->selectCourseFromPool(0, csic);
    if (c)
      runner->setCourseId(c->getId());
  }

  if (cls && cls->hasUnorderedLegs()) {
    pCourse crs = cls->selectParallelCourse(*runner, csic);
    if (crs) {
      runner->setCourseId(crs->getId());
      runner->synchronize(true);
    }
  }

  if (!runner->getCourse(false) && !csic.isManualInput() && !oe->getMeOSFeatures().hasFeature(MeOSFeatures::NoCourses)) {

    if (pclass && !pclass->hasMultiCourse() && !pclass->hasDirectResult()) {
      pCourse pcourse = gEvent->addCourse(pclass->getName());
      pclass->setCourse(pcourse);

      for (unsigned i = 0; i < csic.nPunch; i++)
        pcourse->addControl(csic.Punch[i].Code);

      wchar_t msg[256];

      swprintf_s(msg, lang.tl(L"Skapade en bana för klassen %s med %d kontroller från brickdata (SI-%d)").c_str(),
        pclass->getName().c_str(), csic.nPunch, csic.CardNumber);

      if (silent)
        gdi.addInfoBox("SIINFO", wstring(L"#") + msg, L"", BoxStyle::Header, 15000);
      else
        gdi.addStringUT(0, msg);
    }
    else {
      if (!(pclass && pclass->hasDirectResult())) {
        const wchar_t* msg = L"Löpare saknar klass eller bana";

        if (silent)
          gdi.addInfoBox("SIINFO", msg, L"", BoxStyle::Header, 15000);
        else
          gdi.addString("", 0, msg);
      }
    }
  }

  pCourse pcourse = runner->getCourse(false);

  if (pcourse)
    pcourse->synchronize();
  else if (pclass && pclass->hasDirectResult())
    runner->setStatus(StatusOK, true, oBase::ChangeType::Update, false);
  SICard sic(csic);
  StoredReadout rout;
  rout.runnerId = runner->getId();

  if (!csic.isManualInput()) {
    pCard card = gEvent->allocateCard(runner);

    card->setReadId(csic);
    card->setCardNo(sic.CardNumber);
    card->setMeasuredVoltage(sic.miliVolt);

    rout.cardno = itow(sic.CardNumber);

    rout.info = runner->getName() + L" (" + rout.cardno + L"),   ";
    if (!runner->getClub().empty())
      rout.info += runner->getClub() + +L",   ";
    rout.info += runner->getClass(true);

    // Write read card to log
    logCard(gdi, sic);

    // Convert punch times to relative times.
    oe->convertTimes(runner, sic);
    pCourse prelCourse = runner->getCourse(false);
    const int finishPT = prelCourse ? prelCourse->getFinishPunchType() : oPunch::PunchFinish;
    bool hasFinish = false;
    oCard::PunchOrigin origin = sic.isDebugCard ? oCard::PunchOrigin::Manual : oCard::PunchOrigin::Original;

    if (sic.CheckPunch.Code != -1)
      card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time, 0, sic.CheckPunch.Code, origin);

    if (sic.StartPunch.Code != -1)
      card->addPunch(oPunch::PunchStart, sic.StartPunch.Time, 0, sic.StartPunch.Code, origin);

    for (unsigned i = 0; i < sic.nPunch; i++) {
      if (sic.Punch[i].Code == finishPT)
        hasFinish = true;
      card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time, 0, 0, origin);
    }
    if (sic.FinishPunch.Code != -1) {
      card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time, 0, sic.FinishPunch.Code, origin);
      if (finishPT == oPunch::PunchFinish)
        hasFinish = true;
    }

    if (!hasFinish)
      rout.warnings += lang.tl(L"Målstämpling saknas.");

    card->synchronize();
    runner->addCard(card, rout.MP);
    runner->synchronize(true);
    runner->hasManuallyUpdatedTimeStatus();
  }
  else {
    //Manual input
    rout.info = runner->getName() + L",   " + runner->getClub() + L",   " + runner->getClass(true);
    runner->setCard(0);

    if (csic.statusOK) {
      runner->setStatus(StatusOK, true, oBase::ChangeType::Update);
      runner->setFinishTime(csic.relativeFinishTime);
    }
    else if (csic.statusDNF) {
      runner->setStatus(StatusDNF, true, oBase::ChangeType::Update);
      runner->setFinishTime(0);
    }
    else {
      runner->setStatus(StatusMP, true, oBase::ChangeType::Update);
      runner->setFinishTime(csic.relativeFinishTime);
    }

    rout.cardno = makeDash(L"-");
    runner->evaluateCard(true, rout.MP, 0, oBase::ChangeType::Update);
    runner->synchronizeAll(true);
    runner->hasManuallyUpdatedTimeStatus();
  }

  //Update to SQL-source
  runner->synchronize();

  set<int> clsSet;
  wstring mpList;

  if (runner->getClassId(false))
    clsSet.insert(runner->getClassId(true));
  gEvent->calculateResults(clsSet, oEvent::ResultType::ClassResult);
  if (runner->getTeam())
    gEvent->calculateTeamResults(clsSet, oEvent::ResultType::ClassResult);

  if (runner->getStatusComputed(true) == StatusOK || isPossibleResultStatus(runner->getStatusComputed(true))) {
    wstring placeS = getPlace(runner);

    if (placeS == L"1")
      playReadoutSound(SND::Leader);
    else
      playReadoutSound(SND::OK);

    rout.color = colorLightGreen;
    rout.statusline = lang.tl(L"Status OK,    ") +
      lang.tl(L"Tid: ") + getTimeString(runner);
    if (!placeS.empty())
      rout.statusline += lang.tl(L",      Prel. placering: ") + placeS;
    rout.statusline += lang.tl(L",     Prel. bomtid: ") + runner->getMissedTimeS();

    rout.rentCard = runner->isRentalCard() || oe->isHiredCard(sic.CardNumber);
    
    if (!silent) {
      rout.render(gdi, rout.computeRC(gdi));
      gdi.scrollToBottom();
    }
    else {
      wstring msg = L"#" + runner->getName() + L" (" + rout.cardno + L")\n" +
        runner->getClub() + L". " + runner->getClass(true) +
        L"\n" + lang.tl("Tid:  ") + runner->getRunningTimeS(true, SubSecond::Auto) + lang.tl(L", Plats  ") + placeS;

      if (runner->isRentalCard())
        gdi.addInfoBox("SIINFO", msg, L"Hyrbricka", BoxStyle::SubLine, 10000);
      else
        gdi.addInfoBox("SIINFO", msg, L"", BoxStyle::SubLine, 10000);
    }
  }
  else {
    if (runner->payBeforeResult(false))
      rout.statusline = lang.tl("Betalning av anmälningsavgift inte registrerad");
    else
      rout.statusline = lang.tl(L"Status: ") + runner->getStatusS(true, true);

    playReadoutSound(SND::NotOK);
    if (!rout.MP.empty()) {
      for (int c : rout.MP) {
        if (!mpList.empty())
          mpList += L", ";
        mpList = mpList + itow(c);
      }
      mpList += lang.tl(" saknas.");
    }

    if (!mpList.empty())
      rout.statusline += L", (" + mpList + +L")";

    rout.color = colorLightRed;
    rout.rentCard = runner->isRentalCard() || oe->isHiredCard(sic.CardNumber);

    if (!silent) {
      rout.render(gdi, rout.computeRC(gdi));
      gdi.scrollToBottom();
    }
    else {
      wstring statusmsg = L"#" + runner->getName() + L" (" + rout.cardno + L")\n" +
        runner->getClub() + L". " + runner->getClass(true) +
        L"\n" + rout.statusline;

      gdi.addInfoBox("SIINFO", statusmsg, L"", BoxStyle::Header, 10000);
    }
  }

  gdioutput* gdi_settings = getExtraWindow("readout_view", true);
  if (gdi_settings) {
    showReadoutStatus(*gdi_settings, runner, nullptr, nullptr, mpList);
  }

  tabForceSync(gdi, gEvent);
  gdi.makeEvent("DataUpdate", "sireadout", runner ? runner->getId() : 0, 0, true);

  // Print splits
  if (printSplits)
    generateSplits(runner, gdi);

  activeSIC.clear(&csic);

  readCards.push_back(std::move(rout));
  checkMoreCardsInQueue(gdi);
  return true;
}

RECT TabSI::StoredReadout::computeRC(gdioutput& gdi) const {
  RECT rc;
  rc.left = gdi.scaleLength(15);
  rc.right = gdi.getWidth() - gdi.scaleLength(10);
  rc.top = gdi.getCY() + gdi.getLineHeight() - gdi.scaleLength(5);
  rc.bottom = rc.top + gdi.getLineHeight() * 2 + gdi.scaleLength(14);

  if (!warnings.empty())
    rc.bottom += gdi.getLineHeight();

  return rc;
}

void TabSI::StoredReadout::render(gdioutput& gdi, const RECT& rc) const {
  gdi.fillDown();
  gdi.addRectangle(rc, color, true);

  //gdi.addString("edit", rc.right - gdi.scaleLength(30), rc.top+gdi.scaleLength(4), textImage, "S" + itos(gdi.scaleLength(24)));
  if (runnerId > 0) {
    gdi.addImage("edit", rc.top + gdi.scaleLength(4), rc.right - gdi.scaleLength(30), 0,
      itow(IDI_MEOSEDIT), gdi.scaleLength(24), gdi.scaleLength(24), SportIdentCB).setExtra(runnerId);
  }
  int lh = gdi.getLineHeight();
  int marg = gdi.scaleLength(20);
  int tmarg = gdi.scaleLength(6);
  gdi.addStringUT(rc.top + 6, rc.left + marg, 1, info);
  if (!warnings.empty())
    gdi.addStringUT(rc.top + tmarg + 2 * lh, rc.left + marg, 0, warnings);

  gdi.addStringUT(rc.top + tmarg + lh, rc.left + marg, 0, statusline);

  if (rentCard)
    rentCardInfo(gdi, rc);
}

void TabSI::StoredReadout::rentCardInfo(gdioutput& gdi, const RECT& rcIn) {
  RECT rc;
  rc.left = rcIn.left;
  rc.right = rcIn.right;
  rc.top = rcIn.bottom;
  rc.bottom = rc.top + gdi.getLineHeight() + gdi.scaleLength(5);

  gdi.addRectangle(rc, colorYellow, true);
  gdi.addString("", rc.top + gdi.scaleLength(2), (rc.left + rc.right) / 2, 1 | textCenter, "Vänligen återlämna hyrbrickan.");
}

void TabSI::renderReadCard(gdioutput& gdi, int maxNumber) {
  if (readCards.empty())
    return;

  auto it = readCards.begin();
  int N = readCards.size();
  if (N > maxNumber)
    it = std::next(it, N - maxNumber);

  while (it != readCards.end()) {
    it->render(gdi, it->computeRC(gdi));
    ++it;
  }

  gdi.scrollToBottom();
}


wstring TabSI::getPlace(const oRunner* runner) {
  if (!runner->getClassRef(false))
    return L"";

  bool qfClass = runner->getClassId(false) != runner->getClassId(true);

  if (!qfClass) {
    if (runner->getClassRef(true)->getClassType() == ClassType::oClassPatrol) {
      wstring placeS = runner->getTeam()->getLegPlaceS(-1, false);
      if (placeS.empty())
        placeS = L"\u2026";

      return placeS;
    }
  }
  wstring placeS = (runner->getTeam() && !qfClass) ?
    runner->getTeam()->getLegPlaceS(runner->getLegNumber(), false) :
    runner->getPlaceS();

  return placeS;
}

wstring TabSI::getTimeString(const oRunner* runner) {
  bool qfClass = runner->getClassId(false) != runner->getClassId(true);
  wstring ts = runner->getRunningTimeS(true, SubSecond::Auto);
  if (!qfClass && runner->getTeam()) {
    cTeam t = runner->getTeam();
    if (t->getLegStatus(runner->getLegNumber(), true, false) == StatusOK) {
      ts += L" (" + t->getLegRunningTimeS(runner->getLegNumber(), true, false, SubSecond::Auto) + L")";
    }
  }
  return ts;
}

wstring TabSI::getTimeAfterString(const oRunner* runner) {
  bool qfClass = runner->getClassId(false) != runner->getClassId(true);
  int ta = runner->getTimeAfter();

  wstring ts;
  if (ta > 0)
    ts = L"+" + formatTime(ta);

  if (!qfClass && runner->getTeam()) {
    cTeam t = runner->getTeam();
    if (t->getLegStatus(runner->getLegNumber(), true, false) == StatusOK) {
      int tat = t->getTimeAfter(runner->getLegNumber(), true);
      if (tat > 0) {
        /*        if (ta == 0)
           ts = L"0:00";

         ts += L" (+" + formatTime(tat) + L")";
         */
        ts = L"+" + formatTime(tat);
      }
    }
  }
  return ts;
}

void TabSI::processPunchOnly(gdioutput& gdi, const SICard& csic)
{
  gdi.makeEvent("PunchCard", "sireadout", csic.CardNumber, 0, true);

  SICard sic = csic;
  DWORD loaded;
  gEvent->convertTimes(nullptr, sic);
  oFreePunch* ofp = nullptr;
  wstring accessError;
  if (sic.nPunch == 1) {
    ofp = gEvent->addFreePunch(sic.Punch[0].Time, sic.Punch[0].Code, 0, sic.CardNumber, true, !sic.isDebugCard);
  }
  else if (sic.FinishPunch.Time > 0) {
    ofp = gEvent->addFreePunch(sic.FinishPunch.Time, oPunch::PunchFinish, sic.FinishPunch.Code, sic.CardNumber, true, !sic.isDebugCard);
  }
  else if (sic.StartPunch.Time > 0) {
    ofp = gEvent->addFreePunch(sic.StartPunch.Time, oPunch::PunchStart, sic.StartPunch.Code, sic.CardNumber, true, !sic.isDebugCard);
  }
  else {
    ofp = gEvent->addFreePunch(sic.CheckPunch.Time, oPunch::PunchCheck, sic.CheckPunch.Code, sic.CardNumber, true, !sic.isDebugCard);
  }
  if (ofp) {
    pRunner r = ofp->getTiedRunner();
    if (gdi.getData("SIPageLoaded", loaded)) {
      //gEvent->getRunnerByCard(sic.CardNumber);

      if (r) {
        wstring str = r->getName() + lang.tl(" stämplade vid ") + ofp->getSimpleString();
        gdi.addStringUT(0, str);
        gdi.dropLine();
      }
      else {
        wstring str = itow(sic.CardNumber) + lang.tl(" (okänd) stämplade vid ") + ofp->getSimpleString();
        gdi.addStringUT(0, str);
        gdi.dropLine(0.3);
      }
      gdi.scrollToBottom();
    }

    tabForceSync(gdi, gEvent);
    gdi.makeEvent("DataUpdate", "sireadout", r ? r->getId() : 0, 0, true);
  }
  else if (!accessError.empty()) {
    playReadoutSound(SND::ActionNeeded);
    if (gdi.getData("SIPageLoaded", loaded)) {
      gdi.addString("", fontLarge, accessError).setColor(colorDarkRed);
      gdi.dropLine(0.3);
      gdi.scrollToBottom();
    }
    else
      gdi.addInfoBox("Access", accessError, L"");
  }

  checkMoreCardsInQueue(gdi);
  return;
}

void TabSI::entryCard(gdioutput& gdi, const SICard& sic)
{
  gdi.setText("CardNo", sic.CardNumber);

  if (oe->hasHiredCardData())
    gdi.check("RentCard", oe->isHiredCard(sic.CardNumber));

  wstring name;
  wstring club;
  bool setClub = false;
  int age = 0;
  if (showDatabase()) {
    pRunner db_r = oe->dbLookUpByCard(sic.CardNumber);

    if (db_r) {
      name = db_r->getNameRaw();
      club = db_r->getClub();
      setClub = true;
      age = db_r->getBirthAge();

      if (gdi.hasWidget("BirthDate")) 
        gdi.setText("BirthDate", db_r->getBirthDate());
      
      if (gdi.hasWidget("Nationality"))
        gdi.setText("Nationality", db_r->getNationality());

      if (gdi.hasWidget("Sex")) {
        int data = db_r->getSex();
        gdi.selectItemByData("Sex", data);
      }
    }
  }

  //Else get name from card
  if (name.empty() && (sic.firstName[0] || sic.lastName[0]))
    name = wstring(sic.lastName) + L", " + wstring(sic.firstName);

  gdi.setText("Name", name);
  if (gdi.hasWidget("Club") && (!club.empty() || setClub))
    gdi.setText("Club", club);

  if (club.empty() && !setClub && gdi.hasWidget("Club"))
    gdi.setInputFocus("Club");
  else if (name.empty())
    gdi.setInputFocus("Name");
  else
    gdi.setInputFocus("Class");

  int clsId = gdi.getSelectedItem("Class").first;
  pClass cls = oe->getClass(clsId);
  if (cls && age > 0) {
    directEntryGUI.updateFees(gdi, cls, age);
  }
  else {
    updateEntryInfo(gdi);
  }
}

void TabSI::assignCard(gdioutput& gdi, const SICard& sic)
{

  if (interactiveReadout) {
    pRunner rb = oe->getRunner(runnerMatchedId, 0);

    if (rb && oe->checkCardUsed(gdi, *rb, sic.CardNumber))
      return;

    gdi.setText("CardNo", sic.CardNumber);
    if (runnerMatchedId != -1 && gdi.isChecked("AutoTie"))
      tieCard(gdi);
    return;
  }

  int storedAssigneIndex = currentAssignIndex;
  //Try first current focus
  BaseInfo* ii = gdi.getInputFocus();
  wstring sicode = itow(sic.CardNumber);

  if (ii && ii->id[0] == '*') {
    currentAssignIndex = atoi(ii->id.c_str() + 1);
  }
  else { //If not correct focus, use internal counter
    char id[32];
    sprintf_s(id, "*%d", currentAssignIndex++);

    ii = gdi.setInputFocus(id);

    if (!ii) {
      currentAssignIndex = 0;
      sprintf_s(id, "*%d", currentAssignIndex++);
      ii = gdi.setInputFocus(id);
    }
  }

  if (ii && ii->getExtraInt()) {
    pRunner r = oe->getRunner(ii->getExtraInt(), 0);
    if (r) {
      if (oe->checkCardUsed(gdi, *r, sic.CardNumber)) {
        currentAssignIndex = storedAssigneIndex;
        return;
      }
      if (r->getCardNo() == 0 ||
        gdi.ask(L"Skriv över existerande bricknummer?")) {

        r->setCardNo(sic.CardNumber, false);
        r->setRentalCard(true);
        r->synchronize();
        gdi.setText(ii->id, sicode);
      }
    }
    gdi.TabFocus();
  }

  checkMoreCardsInQueue(gdi);
}

void TabSI::generateEntryLine(gdioutput& gdi, pRunner r) {
  oe->synchronizeList({ oListId::oLRunnerId, oListId::oLCardId });

  gdi.restore("EntryLine", false);
  gdi.setRestorePoint("EntryLine");
  gdi.dropLine(1);
  int xb = gdi.getCX();
  int yb = gdi.getCY();
  gdi.dropLine();
  gdi.setCX(xb + gdi.scaleLength(10));

  gdi.fillRight();

  gdi.pushX();
  storedInfo.checkAge();
  gdi.addInput("CardNo", storedInfo.storedCardNo, 8, SportIdentCB, L"Bricka:");

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    gdi.addCombo("Club", 180, 200, 0, L"Klubb:",
      L"Skriv första bokstaven i klubbens namn och tryck pil-ner för att leta efter klubben")
      .setHandler(&directEntryGUI);
    oe->fillClubs(gdi, "Club");
    if (storedInfo.storedClub.empty())
      gdi.selectItemByData("Club", lastClubId);
    else
      gdi.setText("Club", storedInfo.storedClub);
  }

  gdi.addInput("Name", storedInfo.storedName, 16, 0, L"Namn:").setHandler(&directEntryGUI);

  gdi.addSelection("Class", 150, 200, 0, L"Klass:").setHandler(&directEntryGUI);
  {
    vector< pair<wstring, size_t> > d;
    oe->fillClasses(d, oEvent::extraNumMaps, oEvent::filterOnlyDirect);
    if (d.empty() && oe->getNumClasses() > 0) {
      gdi.alert(L"Inga klasser tillåter direktanmälan. På sidan klasser kan du ändra denna egenskap.");
    }
    gdi.setItems("Class", d);
  }

  if (storedInfo.storedClassId > 0 && gdi.selectItemByData("Class", storedInfo.storedClassId)) {
  }
  else if (!gdi.selectItemByData("Class", lastClassId)) {
    gdi.selectFirstItem("Class");
  }

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy)) {
    gdi.addCombo("Fee", 60, 150, SportIdentCB, L"Anm. avgift:");
    oe->fillFees(gdi, "Fee", true, false);

    if (!storedInfo.storedFee.empty() && storedInfo.storedFee != L"@")
      gdi.setText("Fee", storedInfo.storedFee);
    else
      gdi.setText("Fee", lastFee);

    gdi.dropLine(1);
    generatePayModeWidget(gdi);
    gdi.dropLine(-1);
  }

  gdi.popX();
  gdi.dropLine(3.1);

  int cnt = TabRunner::addExtraFields(*oe, gdi, oEvent::ExtraFieldContext::DirectEntry);
  auto setIf = [&gdi](const string& wg, const wstring& val) {
    if (gdi.hasWidget(wg))
      gdi.setText(wg, val);
  };
  setIf("StartTime", storedInfo.storedStartTime);
  setIf("Phone", storedInfo.storedPhone);
  setIf("DataA", storedInfo.dataA);
  setIf("DataB", storedInfo.dataB);
  setIf("TextA", storedInfo.textA);
  setIf("Nationality", storedInfo.nationality);
  setIf("BirthDate", storedInfo.birthDate);
  setIf("Rank", storedInfo.rank);
  if (gdi.hasWidget("Sex"))
    gdi.selectItemByData("Sex", storedInfo.sex);

  if (cnt > 0) {
    gdi.setCX(gdi.getCX() + gdi.scaleLength(20));
    gdi.dropLine(0.2);
  }

  gdi.addCheckbox("RentCard", "Hyrbricka", SportIdentCB, storedInfo.rentState);
  gdi.addCheckbox("NoTiming", "Utan tidtagning", nullptr, false);

  if (oe->hasNextStage())
    gdi.addCheckbox("AllStages", "Anmäl till efterföljande etapper", SportIdentCB, storedInfo.allStages);

  if (r != 0) {
    if (r->getCardNo() > 0)
      gdi.setText("CardNo", r->getCardNo());

    gdi.setText("Name", r->getNameRaw());
    if (gdi.hasWidget("Club")) {
      gdi.selectItemByData("Club", r->getClubId());
    }
    gdi.selectItemByData("Class", r->getClassId(true));

    TabRunner::loadExtraFields(gdi, r);
    oDataConstInterface dci = r->getDCI();
    if (gdi.hasWidget("Fee"))
      gdi.setText("Fee", oe->formatCurrency(dci.getInt("Fee")));

    if (gdi.hasWidget("StartTime"))
      gdi.setText("StartTime", r->getStartTimeS());
  
    if (gdi.hasWidget("Bib"))
      gdi.setText("Bib", r->getBib());
    gdi.check("NoTiming", r->hasFlag(oAbstractRunner::TransferFlags::FlagNoTiming));
    gdi.check("RentCard", r->isRentalCard());
    if (gdi.hasWidget("Paid"))
      gdi.check("Paid", dci.getInt("Paid") > 0);
    else if (gdi.hasWidget("PayMode")) {
      int paidId = dci.getInt("Paid") > 0 ? r->getPaymentMode() : 1000;
      gdi.selectItemByData("PayMode", paidId);
    }

    if (gdi.hasWidget("AllStages")) {
      gdi.check("AllStages", r->hasFlag(oRunner::FlagTransferNew));
    }
  }

  gdi.popX();
  gdi.dropLine(2);
  gdi.addButton("EntryOK", "OK", SportIdentCB).setDefault().setExtra(r ? r->getId() : 0);
  gdi.addButton("EntryCancel", "Avbryt", SportIdentCB).setCancel();
  gdi.dropLine(0.1);
  gdi.addString("EntryInfo", fontMediumPlus, "").setColor(colorDarkRed);
  updateEntryInfo(gdi);
  gdi.setInputFocus("CardNo");
  gdi.dropLine(2);

  RECT rc = { xb, yb, gdi.getWidth(), gdi.getHeight() };
  gdi.addRectangle(rc, colorLightCyan);
  gdi.scrollToBottom();
  gdi.popX();
  gdi.setOnClearCb("si", SportIdentCB);
}

void TabSI::updateEntryInfo(gdioutput& gdi)
{
  int fee = oe->interpretCurrency(gdi.getText("Fee", true));
  if (gdi.isChecked("RentCard")) {
    int cardFee = oe->getDI().getInt("CardFee");
    if (cardFee > 0)
      fee += cardFee;
  }
  if (gdi.isChecked("AllStages")) {
    int nums = oe->getNumStages();
    int cs = oe->getStageNumber();
    if (nums > 0 && cs <= nums) {
      int np = nums - cs + 1;
      fee *= np;
    }
  }

  wstring method;
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy)) {
    bool invoice = true;
    if (gdi.hasWidget("PayMode")) {
      invoice = gdi.getSelectedItem("PayMode").first == 1000;
    }
    else
      invoice = !gdi.isChecked("Paid");

    if (!invoice)
      method = lang.tl(L"Att betala");
    else
      method = lang.tl(L"Faktureras");

    gdi.setText("EntryInfo", lang.tl(L"X: Y. Tryck <Enter> för att spara#" +
      method + L"#" + oe->formatCurrency(fee)), true);
  }
  else {
    gdi.setText("EntryInfo", lang.tl("Press Enter to continue"), true);

  }
}

void TabSI::generateSplits(const pRunner r, gdioutput& gdi)
{
  const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
  if (wideFormat) {
    addToPrintQueue(r);
    while (checkpPrintQueue(gdi));
  }
  else {
    gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
    if (r->payBeforeResult(false)) {
      gdiprint.addString("", 0, "Betalning av anmälningsavgift inte registrerad");
      gdiprint.dropLine(4);
      gdiprint.addStringUT(0, r->getCompleteIdentification(oRunner::IDType::OnlyThis));
    }
    else {
      vector<int> mp;
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      r->printSplits(gdiprint);
    }
    printProtected(gdi, gdiprint);
    //gdiprint.print(splitPrinter, oe, false, true);
  }
}

void TabSI::generateStartInfo(gdioutput& gdi, const oRunner& r, bool includeEconomy) {
  if (printStartInfo) {
    gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
    r.printStartInfo(gdiprint, includeEconomy);
    printProtected(gdi, gdiprint);
    //gdiprint.print(splitPrinter, oe, false, true);
  }
}

void TabSI::printerSetup(gdioutput& gdi)
{
  gdi.printSetup(splitPrinter);
  splitPrinter.onlyChanged = false;
}

void TabSI::checkMoreCardsInQueue(gdioutput& gdi) {
  // Create a local list to avoid stack overflow
  list<SICard> cards = std::move(CardQueue);
  CardQueue.clear();
  std::exception storedEx;
  bool fail = false;

  while (!cards.empty()) {
    SICard c = cards.front();
    cards.pop_front();
    try {
      gdi.removeFirstInfoBox("SIREAD");
      insertSICard(gdi, c);
    }
    catch (std::exception& ex) {
      fail = true;
      storedEx = ex;
    }
  }

  if (fail)
    throw storedEx;
}

bool TabSI::autoAssignClass(pRunner r, const SICard& sic) {
  if (r && r->getClassId(false) == 0) {
    vector<pClass> classes;
    int dist = oe->findBestClass(sic, classes);

    if (classes.size() == 1 && dist >= -1 && dist <= 1) // Allow at most one wrong punch
      r->setClassId(classes[0]->getId(), true);
  }

  return r && r->getClassId(false) != 0;
}

void TabSI::showManualInput(gdioutput& gdi) {
  runnerMatchedId = -1;
  gdi.setRestorePoint("ManualInput");
  gdi.fillDown();
  gdi.dropLine(0.7);

  int x = gdi.getCX();
  int y = gdi.getCY();

  gdi.setCX(x + gdi.scaleLength(15));
  gdi.dropLine();
  gdi.addString("", 1, "Manuell inmatning");
  gdi.fillRight();
  gdi.pushX();
  gdi.dropLine();
  gdi.addInput("Manual", L"", 20, SportIdentCB, L"Nummerlapp, SI eller Namn:");
  gdi.addInput("FinishTime", lang.tl("Aktuell tid"), 8, SportIdentCB, L"Måltid:").setFgColor(colorGreyBlue).setExtra(1);
  gdi.dropLine(1.2);
  gdi.addCheckbox("StatusOK", "Godkänd", SportIdentCB, true);
  gdi.addCheckbox("StatusDNF", "Utgått", SportIdentCB, false);
  gdi.dropLine(-0.3);
  gdi.addButton("ManualOK", "OK", SportIdentCB).setDefault();
  gdi.fillDown();
  gdi.dropLine(2);
  gdi.popX();
  gdi.addString("FindMatch", 0, "", 0).setColor(colorDarkGreen);
  gdi.dropLine();

  RECT rc;
  rc.left = x;
  rc.right = gdi.getWidth() - 10;
  rc.top = y;
  rc.bottom = gdi.getCY() + gdi.scaleLength(5);
  gdi.dropLine();
  gdi.addRectangle(rc, colorLightBlue);
  gdi.scrollToBottom();
}

void TabSI::tieCard(gdioutput& gdi) {
  int card = gdi.getTextNo("CardNo");
  pRunner r = oe->getRunner(runnerMatchedId, 0);

  if (r == 0)
    throw meosException("Invalid binding");

  if (oe->checkCardUsed(gdi, *r, card))
    return;

  if (r->getCardNo() > 0 && r->getCardNo() != card) {
    if (!gdi.ask(L"X har redan bricknummer Y. Vill du ändra det?#" + r->getName() + L"#" + itow(r->getCardNo())))
      return;
  }

  bool rent = false;
  if (!gdi.hasWidget("AutoTieRent") || !gdi.isChecked("AutoTieRent"))
    rent = gdi.isChecked("RentCardTie");
  else
    rent = oe->isHiredCard(card);

  r->synchronize();
  r->setCardNo(card, true, false);
  r->setRentalCard(rent);
  r->synchronize(true);

  gdi.restore("ManualTie");
  gdi.pushX();
  gdi.fillRight();
  gdi.addStringUT(italicText, getLocalTimeOnly());
  if (!r->getBib().empty())
    gdi.addStringUT(0, r->getBib(), 0);
  gdi.addStringUT(0, r->getName(), 0);

  if (r->getTeam() && r->getTeam()->getName() != r->getName())
    gdi.addStringUT(0, L"(" + r->getTeam()->getName() + L")", 0);
  else if (!r->getClub().empty())
    gdi.addStringUT(0, L"(" + r->getClub() + L")", 0);

  gdi.addStringUT(1, itos(r->getCardNo()), 0).setColor(colorDarkGreen);
  if (rent)
    gdi.addStringUT(0, L" (" + lang.tl("Hyrd") + L")  ");

  gdi.addString("EditAssign", 0, "Ändra", SportIdentCB).setExtra(r->getId());
  gdi.dropLine(1.5);
  gdi.popX();

  showAssignCard(gdi, false);
}

void TabSI::showAssignCard(gdioutput& gdi, bool showHelp) {
  if (interactiveReadout) {
    if (showHelp) {
      checkBoxToolBar(gdi, { CheckBox::Interactive, CheckBox::AutoTie, CheckBox::AutoTieRent });
      gdi.addString("", 10, L"Avmarkera 'X' för att hantera alla bricktildelningar samtidigt.#" + lang.tl("Interaktiv inläsning"));
      gdi.dropLine(0.5);
      gdi.setRestorePoint("ManualTie");
    }
  }
  else {
    if (showHelp) {
      checkBoxToolBar(gdi, { CheckBox::Interactive });
      gdi.addString("", 10, L"Markera 'X' för att hantera deltagarna en och en.#" + lang.tl("Interaktiv inläsning"));
    }

    gEvent->assignCardInteractive(gdi, SportIdentCB, sortAssignCards);
    return;
  }

  runnerMatchedId = -1;
  gdi.fillDown();
  gdi.dropLine(0.7);

  int x = gdi.getCX();
  int y = gdi.getCY();

  gdi.setCX(x + gdi.scaleLength(15));
  gdi.dropLine();
  gdi.addString("", 1, "Knyt bricka / deltagare");
  gdi.fillRight();
  gdi.pushX();
  gdi.dropLine();
  gdi.addInput("RunnerId", L"", 20, SportIdentCB, L"Nummerlapp, lopp-id eller namn:");
  gdi.addInput("CardNo", L"", 8, SportIdentCB, L"Bricknr:");
  gdi.dropLine(1.2);
  //gdi.addCheckbox("AutoTie", "Knyt automatiskt efter inläsning", SportIdentCB, oe->getPropertyInt("AutoTie", 1) != 0);
  gdi.addCheckbox("RentCardTie", "Hyrd", SportIdentCB, oe->getPropertyInt("RentCard", 0) != 0);

  gdi.dropLine(-0.3);
  gdi.addButton("TieOK", "OK", SportIdentCB).setDefault();
  gdi.disableInput("TieOK");
  gdi.setInputFocus("RunnerId");
  gdi.fillDown();
  gdi.dropLine(2);
  gdi.popX();
  gdi.addString("FindMatch", 0, "", 0).setColor(colorDarkGreen);
  gdi.dropLine();

  RECT rc;
  rc.left = x;
  rc.right = gdi.getWidth() + gdi.scaleLength(5);
  rc.top = y;
  rc.bottom = gdi.getCY() + gdi.scaleLength(5);
  gdi.dropLine();
  gdi.addRectangle(rc, colorLightBlue);
  gdi.scrollToBottom();
}

pRunner TabSI::getRunnerByIdentifier(int identifier) const {
  int id;
  if (identifierToRunnerId.lookup(identifier, id)) {
    pRunner r = oe->getRunner(id, 0);
    if (r && r->getRaceIdentifier() == identifier)
      return r;
    else
      minRunnerId = 0; // Map is out-of-date
  }

  if (identifier < minRunnerId)
    return 0;

  minRunnerId = MAXINT;
  identifierToRunnerId.clear();

  pRunner ret = 0;
  vector<pRunner> runners;
  oe->autoSynchronizeLists(false);
  oe->getRunners(0, 0, runners, false);
  for (size_t k = 0; k < runners.size(); k++) {
    if (runners[k]->getRaceNo() == 0) {
      int i = runners[k]->getRaceIdentifier();
      identifierToRunnerId.insert(i, runners[k]->getId());
      minRunnerId = min(minRunnerId, i);
      if (i == identifier)
        ret = runners[k];
    }
  }
  return ret;
}

bool TabSI::askOverwriteCard(gdioutput& gdi, pRunner r) const {
  return gdi.ask(L"ask:overwriteresult#" + r->getCompleteIdentification(oRunner::IDType::OnlyThis));
}

void TabSI::showModeCardData(gdioutput& gdi) {
  //  gdi.disableInput("Interactive", true);
  //  gdi.enableInput("Database", true);
  //  gdi.enableInput("PrintSplits");
  //  gdi.disableInput("StartInfo", true);
  //  gdi.disableInput("UseManualInput", true);
  checkBoxToolBar(gdi, { CheckBox::UseDB, CheckBox::PrintSplits });

  gdi.dropLine(0.5);
  //gdi.fillDown();
  gdi.pushX();
  //gdi.addStringUT(boldLarge, lang.tl(L"Print card data", true));
  //gdi.dropLine(0.2);
  gdi.fillRight();
  gdi.addButton("ClearMemory", "Clear Memory", SportIdentCB);
  gdi.addButton("SaveMemory", "Spara...", SportIdentCB);
  if (oe->empty()) {
    gdi.addButton("CreateCompetition", "Create Competition", SportIdentCB);
    if (savedCards.empty())
      gdi.disableInput("CreateCompetition");

#ifdef _DEBUG
    gdi.addButton("Import", "Importera från fil...", SportIdentCB);
#endif
  }
  gdi.fillDown();
  gdi.addButton("ReadoutWindow", "Eget fönster", SportIdentCB);

  gdi.dropLine();
  gdi.popX();
  gdi.addString("", 10, "help:analyzecard");

  gdi.dropLine(3);
  gdi.popX();
  bool first = true;
  for (auto it = savedCards.begin(); it != savedCards.end(); ++it) {
    gdi.dropLine(0.5);
    if (!first) {
      RECT rc = { 30, gdi.getCY(), gdi.scaleLength(250), gdi.getCY() + 3 };
      gdi.addRectangle(rc);
    }
    first = false;

    printCard(gdi, 0, it->first, &it->second, false);
  }
}

void TabSI::EditCardData::handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
  if (type == GUI_LINK) {
    TextInfo& ti = dynamic_cast<TextInfo&>(info);
    int cardId = ti.getExtraInt();
    SICard& card = tabSI->getCard(cardId);
    ti.id = "card" + itos(cardId);
    gdi.removeWidget("CardName");
    gdi.removeWidget("ClubName");
    gdi.removeWidget("OKCard");
    gdi.removeWidget("CancelCard");

    wstring name, club;
    if (card.firstName[0])
      name = (card.lastName[0] ? (wstring(card.lastName) + L", ") : L"") + wstring(card.firstName);
    club = card.club;
    bool noName = name.empty();
    bool noClub = club.empty();
    if (noName)
      name = lang.tl("Namn");
    if (noClub)
      club = lang.tl("Klubb");

    InputInfo& ii = gdi.addInput(ti.xp - 2, ti.yp - 2, "CardName", name, 18, 0);
    ii.setHandler(this);
    InputInfo& ii2 = gdi.addInput(ti.xp + ii.getWidth(), ti.yp - 2, "ClubName", club, 22, 0);
    ii2.setExtra(noClub).setHandler(this);
    ButtonInfo& bi = gdi.addButton(ii2.getX() + 2 + ii2.getWidth(), ti.yp - 4, "OKCard", "OK", 0);
    bi.setExtra(cardId).setHandler(this);
    bi.setDefault();
    int w, h;
    bi.getDimension(gdi, w, h);
    gdi.addButton(bi.xp + w + 4, ti.yp - 4, "CancelCard", "Avbryt", 0).setCancel().setHandler(this);
    gdi.setInputFocus(ii.id, noName);
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo bi = dynamic_cast<ButtonInfo&>(info);
    //OKCard or CancelCard
    if (bi.id == "OKCard") {
      int cardId = bi.getExtraInt();
      SICard& card = tabSI->getCard(cardId);
      wstring name = gdi.getText("CardName");
      wstring club = gdi.getBaseInfo("ClubName").getExtra() ? L"" : gdi.getText("ClubName");
      wstring given = getGivenName(name);
      wstring familty = getFamilyName(name);
      wcsncpy_s(card.firstName, given.c_str(), 20);
      wcsncpy_s(card.lastName, familty.c_str(), 20);
      wcsncpy_s(card.club, club.c_str(), 40);

      wstring s = name;
      if (!club.empty())
        s += L", " + club;
      gdi.setText("card" + itos(cardId), s, true);
    }

    gdi.removeWidget("CardName");
    gdi.removeWidget("ClubName");
    gdi.removeWidget("OKCard");
    gdi.removeWidget("CancelCard");
  }
  else if (type == GUI_FOCUS) {
    InputInfo& ii = dynamic_cast<InputInfo&>(info);
    if (ii.getExtraInt()) {
      ii.setExtra(0);
      gdi.setInputFocus(ii.id, true);
    }
  }
}

void TabSI::printCard(gdioutput& gdi, int lineBreak, int cardId, SICard* crdRef, bool forPrinter) const {
  const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
  if (!wideFormat && forPrinter)
    gdi.setCX(10);

  if (crdRef == nullptr)
    crdRef = &getCard(cardId);

  SICard& c = *crdRef;
  if (c.readOutTime[0] == 0)
    strcpy_s(c.readOutTime, getLocalTimeN().c_str());

  gdi.pushX();
  gdi.fillRight();
  wstring name, clubName;
  if (c.firstName[0] != 0) {
    name = wstring(c.firstName) + L" " + c.lastName;
    clubName = c.club;
  }
  else if (useDatabase) {
    const RunnerWDBEntry* r = oe->getRunnerDatabase().getRunnerByCard(c.CardNumber);
    if (r) {
      r->getName(name);
      const oClub* club = oe->getRunnerDatabase().getClub(r->dbe().clubNo);
      if (club) {
        clubName = club->getName();
        wcsncpy_s(c.club, clubName.c_str(), 20);
      }
      wstring given = r->getGivenName();
      wstring family = r->getFamilyName();
      wcsncpy_s(c.firstName, given.c_str(), 20);
      wcsncpy_s(c.lastName, family.c_str(), 20);
    }
  }

  gdi.addString("", 1, "Bricka X#" + itos(c.CardNumber));

  if (!forPrinter && name.empty())
    name = lang.tl("Okänd");

  if (!name.empty()) {
    if (!clubName.empty())
      name += L", " + clubName;
    gdi.fillDown();
    auto& res = gdi.addStringUT(0, name);
    if (cardId >= 0)
      res.setExtra(cardId).setHandler(&editCardData);
    gdi.popX();
  }
  gdi.fillDown();
  gdi.addStringUT(0, c.readOutTime);
  gdi.popX();

  if (c.miliVolt > 0) {
    auto stat = oCard::isCriticalCardVoltage(c.miliVolt);
    wstring warning;
    if (stat == oCard::BatteryStatus::Bad)
      warning = lang.tl("Replace[battery]");
    else if (stat == oCard::BatteryStatus::Warning)
      warning = lang.tl("Low");
    else
      warning = lang.tl("OK");

    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", normalText, L"Batteristatus:");
    gdi.addStringUT(boldText, oCard::getCardVoltage(c.miliVolt));
    gdi.fillDown();
    gdi.addStringUT(normalText, L"(" + warning + L")");
    gdi.dropLine(0.7);
    gdi.popX();
  }

  int start = NOTIME;
  if (c.CheckPunch.Code != -1)
    gdi.addString("", 0, L"Check: X#" + formatTimeHMS(c.CheckPunch.Time));

  if (c.StartPunch.Code != -1) {
    gdi.addString("", 0, L"Start: X#" + formatTimeHMS(c.StartPunch.Time));
    start = c.StartPunch.Time;
  }
  int xp = gdi.getCX();
  int xp2 = xp + gdi.scaleLength(25);
  int xp3 = xp2 + gdi.scaleLength(35);
  int xp4 = xp3 + gdi.scaleLength(60);
  int xp5 = xp4 + gdi.scaleLength(45);
  const int off = xp5 - xp + gdi.scaleLength(80);

  int baseCY = gdi.getCY();
  int maxCY = baseCY;
  int baseCX = gdi.getCX();
  int accTime = 0;
  int days = 0;

  if (lineBreak > 0) {
    int nRows = min<int>(1 + c.nPunch / lineBreak, 3);
    lineBreak = c.nPunch / nRows + 1;
  }

  for (unsigned k = 0; k < c.nPunch; k++) {
    int cy = gdi.getCY();
    gdi.addStringUT(cy, xp, 0, itos(k + 1) + ".");
    gdi.addStringUT(cy, xp2, 0, itos(c.Punch[k].Code));
    gdi.addStringUT(cy, xp3, 0, formatTimeHMS(c.Punch[k].Time % (24 * timeConstHour)));
    if (start != NOTIME) {
      int legTime = analyzePunch(c.Punch[k], start, accTime, days);
      if (legTime > 0)
        gdi.addStringUT(cy, xp5 - gdi.scaleLength(10), textRight, formatTime(legTime));

      gdi.addStringUT(cy, xp5 + gdi.scaleLength(40), textRight, formatTime(days * timeConstHour * 24 + accTime));
    }
    else {
      start = c.Punch[k].Time;
    }

    if (lineBreak > 0 && (k % lineBreak) == lineBreak - 1) {
      maxCY = max(maxCY, gdi.getCY());
      RECT rcc;
      rcc.top = baseCY;
      rcc.bottom = maxCY;
      rcc.left = xp5 + gdi.scaleLength(60);
      rcc.right = rcc.left + gdi.scaleLength(2);

      gdi.addRectangle(rcc, colorLightCyan);
      gdi.setCY(baseCY);
      xp += off;
      xp2 += off;
      xp3 += off;
      xp4 += off;
      xp5 += off;
    }
  }
  if (c.FinishPunch.Code != -1) {
    int cy = gdi.getCY();
    gdi.addString("", cy, xp, 0, "Mål");
    gdi.addStringUT(cy, xp3, 0, formatTimeHMS(c.FinishPunch.Time % (24 * timeConstHour)));

    if (start != NOTIME) {
      int legTime = analyzePunch(c.FinishPunch, start, accTime, days);
      if (legTime > 0)
        gdi.addStringUT(cy, xp5 - gdi.scaleLength(10), textRight, formatTime(legTime));

      gdi.addStringUT(cy, xp5 + gdi.scaleLength(40), textRight, formatTime(days * timeConstHour * 24 + accTime));
    }

    maxCY = max(maxCY, gdi.getCY());
    if (breakLines > 0) {
      gdi.setCY(maxCY);
      gdi.dropLine(2);
      gdi.setCX(baseCX);
    }

    gdi.addString("", 1, L"Time: X#" + formatTime(days * timeConstHour * 24 + accTime));
  }

  maxCY = max(maxCY, gdi.getCY());

  if (breakLines > 0) {
    gdi.setCY(maxCY);
    gdi.dropLine(2);
    gdi.setCX(baseCX);
  }

  if (forPrinter) {
    gdi.dropLine(1);

    vector<pair<wstring, int>> lines;
    oe->getExtraLines("SPExtra", lines);

    for (size_t k = 0; k < lines.size(); k++) {
      gdi.addStringUT(lines[k].second, lines[k].first);
    }
    if (lines.size() > 0)
      gdi.dropLine(0.5);

    gdi.addString("", fontSmall, "Av MeOS: www.melin.nu/meos");
  }
}

int TabSI::analyzePunch(SIPunch& p, int& start, int& accTime, int& days) {
  int newAccTime = p.Time - start;
  if (newAccTime < 0) {
    newAccTime += timeConstHour * 24;
    if (accTime > 12 * timeConstHour)
      days++;
  }
  else if (newAccTime < accTime - 12 * timeConstHour) {
    days++;
  }
  int legTime = newAccTime - accTime;
  accTime = newAccTime;
  return legTime;
}

void TabSI::generateSplits(int cardId, gdioutput& gdi) {
  gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
  printCard(gdiprint, 0, cardId, nullptr, true);
  printProtected(gdi, gdiprint);
}

void TabSI::printProtected(gdioutput& gdi, gdioutput& gdiprint) {
  try {
    gdiprint.print(splitPrinter, oe, false, true);
  }
  catch (meosException& ex) {
    DWORD loaded;
    if (gdi.getData("SIPageLoaded", loaded)) {
      gdi.dropLine();
      gdi.fillDown();
      gdi.addString("", 0, ex.wwhat(), 0).setColor(colorRed);
      gdi.dropLine();
      gdi.scrollToBottom();
    }
    else {
      if (!printErrorShown) {
        printErrorShown = true;
        gdi.alert(ex.wwhat());
        printErrorShown = false;
      }
    }
  }
}

void TabSI::createCompetitionFromCards(gdioutput& gdi) {
  oe->newCompetition(lang.tl(L"Ny tävling"));
  oe->loadDefaults();
  gdi.setWindowTitle(L"");
  map<size_t, int> hashCount;
  vector< pair<size_t, SICard*> > cards;
  int zeroTime = timeConstHour * 24;
  for (list<pair<int, SICard> >::iterator it = savedCards.begin(); it != savedCards.end(); ++it) {
    size_t hash = 0;
    if (it->second.StartPunch.Code != -1 && it->second.StartPunch.Time > 0)
      zeroTime = min<int>(zeroTime, it->second.StartPunch.Time);

    for (unsigned k = 0; k < it->second.nPunch; k++) {
      hash = 997 * hash + (it->second.Punch[k].Code - 30);
      if (it->second.Punch[k].Code != -1 && it->second.Punch[k].Time > 0)
        zeroTime = min<int>(zeroTime, it->second.Punch[k].Time);
    }
    pair<int, SICard*> p(hash, &it->second);
    ++hashCount[hash];
    cards.push_back(p);
  }

  int course = 0;
  for (size_t k = 0; k < cards.size(); k++) {
    if (!hashCount.count(cards[k].first))
      continue;
    int count = hashCount[cards[k].first];
    if (count < 5 && count < int(cards.size()) / 2)
      continue;

    pCourse pc = oe->addCourse(lang.tl("Bana ") + itow(++course));
    for (unsigned j = 0; j < cards[k].second->nPunch; j++) {
      pc->addControl(cards[k].second->Punch[j].Code);
    }
    oe->addClass(lang.tl(L"Klass ") + itow(course), pc->getId());
    hashCount.erase(cards[k].first);
  }

  // Add remaining classes if suitable
  for (size_t k = 0; k < cards.size(); k++) {
    if (!hashCount.count(cards[k].first))
      continue;
    int count = hashCount[cards[k].first];
    if (count == 1)
      continue; // Don't allow singelton runner classes

    vector<pClass> cls;
    int dist = oe->findBestClass(*cards[k].second, cls);

    if (abs(dist) > 3) {
      pCourse pc = oe->addCourse(lang.tl("Bana ") + itow(++course));
      for (unsigned j = 0; j < cards[k].second->nPunch; j++) {
        pc->addControl(cards[k].second->Punch[j].Code);
      }
      oe->addClass(lang.tl(L"Klass ") + itow(course), pc->getId());
      hashCount.erase(cards[k].first);
    }
  }

  // Define a new zero time
  zeroTime -= timeConstHour;
  if (zeroTime < 0)
    zeroTime += timeConstHour * 24;
  zeroTime -= zeroTime % (timeConstHour / 2);
  oe->setZeroTime(formatTime(zeroTime), false);

  // Add competitors
  for (size_t k = 0; k < cards.size(); k++) {
    if (oe->isCardRead(*cards[k].second))
      continue;

    vector<pClass> cls;
    oe->findBestClass(*cards[k].second, cls);

    if (!cls.empty()) {
      wstring name;
      if (cards[k].second->lastName[0])
        name = wstring(cards[k].second->lastName) + L", ";
      if (cards[k].second->firstName[0])
        name += cards[k].second->firstName;

      if (name.empty())
        name = lang.tl(L"Bricka X#" + itow(cards[k].second->CardNumber));

      oe->addRunner(name, wstring(cards[k].second->club), cls[0]->getId(),
        cards[k].second->CardNumber, L"", true);

      processInsertCard(*cards[k].second);
    }
  }

  TabList& tc = dynamic_cast<TabList&>(*gdi.getTabs().get(TListTab));
  tc.loadPage(gdi, "ResultIndividual");
}

void TabSI::StoredStartInfo::checkAge() {
  uint64_t t = GetTickCount64();
  const int minuteLimit = 3;
  if (t > age && (t - age) > (1000 * 60 * minuteLimit)) {
    clear();
  }
  age = t;
}

void TabSI::StoredStartInfo::clear() {
  age = GetTickCount64();
  storedName.clear();
  storedCardNo.clear();
  storedClub.clear();
  storedFee.clear();
  storedPhone.clear();
  dataA.clear();
  dataB.clear();
  textA.clear();
  sex = PersonSex::sUnknown;
  rank.clear();
  nationality.clear();
  birthDate.clear();
  rentState = false;
  storedStartTime.clear();
  hasPaid = false;
  payMode = 1000;
  //allStages = lastAllStages; // Always use last setting
  storedClassId = 0;
}

void TabSI::clearCompetitionData() {
  firstLoadedAfterNew = true;
  printSplits = false;
  interactiveReadout = oe->getPropertyInt("Interactive", 1) != 0;
  useDatabase = oe->getPropertyInt("Database", 1) != 0;
  printSplits = false;
  printStartInfo = false;
  manualInput = oe->getPropertyInt("ManualInput", 0) == 1;

  savedCardUniqueId = 1;
  checkedCardFlags.clear();
  currentAssignIndex = 0;
  warnedClassOutOfMaps.clear();

  lockedFunction = false;

  if (mode == SIMode::ModeCardData)
    mode = SIMode::ModeReadOut;

  readCards.clear();

  logger.reset();
  numSavedCardsOnCmpOpen = savedCards.size();

  requestStartTimeHandler.reset();

  sortAssignCards = SortOrder::Custom;

#ifdef _DEBUG
  showTestingPanel = !oe->gdiBase().isTest();
#else
  showTestingPanel = false;
#endif // _DEBUG

}

SICard& TabSI::getCard(int id) const {
  if (id < int(savedCards.size() / 2)) {
    for (list< pair<int, SICard> >::const_iterator it = savedCards.begin(); it != savedCards.end(); ++it) {
      if (it->first == id)
        return const_cast<SICard&>(it->second);
    }
  }
  else {
    for (list< pair<int, SICard> >::const_reverse_iterator it = savedCards.rbegin(); it != savedCards.rend(); ++it) {
      if (it->first == id)
        return const_cast<SICard&>(it->second);
    }
  }
  throw meosException("Interal error");
}

bool compareCardNo(const pRunner& r1, const pRunner& r2) {
  int c1 = r1->getCardNo();
  int c2 = r2->getCardNo();
  if (c1 != c2)
    return c1 < c2;
  int f1 = r1->getFinishTime();
  int f2 = r2->getFinishTime();
  if (f1 != f2)
    return f1 < f2;

  return false;
}

wstring TabSI::getCardInfo(bool param, vector<int>& count) const {
  if (!param) {
    assert(count.size() == 8);
    return L"Totalt antal unika avbockade brickor: X#" + itow(count[CNFCheckedAndUsed] +
      count[CNFChecked] +
      count[CNFCheckedNotRented] +
      count[CNFCheckedRentAndNotRent]);
  }
  count.clear();
  count.resize(8);
  for (map<int, CardNumberFlags>::const_iterator it = checkedCardFlags.begin();
    it != checkedCardFlags.end(); ++it) {
    ++count[it->second];
  }

  wstring msg = L"Uthyrda: X, Egna: Y, Avbockade uthyrda: Z#" + itow(count[CNFUsed] + count[CNFCheckedAndUsed]) +
    L"#" + itow(count[CNFNotRented] + count[CNFCheckedNotRented]) +
    L"#" + itow(count[CNFCheckedAndUsed]);

  return msg;
}

void TabSI::showRegisterHiredCards(gdioutput& gdi) {
  checkBoxToolBar(gdi, { });

  gdi.fillDown();
  gdi.addString("", 10, "help:registerhiredcards");

  gdi.dropLine();
  gdi.fillRight();
  gdi.pushX();
  gdi.addButton("RHCClear", "Nollställ", SportIdentCB);
  gdi.addButton("RHCImport", "Importera...", SportIdentCB);
  gdi.addButton("RHCExport", "Exportera...", SportIdentCB);
  gdi.addButton("RHCPrint", "Skriv ut...", SportIdentCB);
  gdi.popX();
  gdi.dropLine(3);
  gdi.fillDown();

  oe->synchronizeList(oListId::oLPunchId);
  auto hiredCards = oe->getHiredCards();
  for (int i : hiredCards) {
    gdi.addStringUT(0, itos(i)).setExtra(i).setHandler(getResetHiredCardHandler());
  }

  gdi.refresh();
}

void TabSI::showCheckCardStatus(gdioutput& gdi, const string& cmd) {
  vector<pRunner> r;
  const int cx = gdi.getCX();
  const int col1 = gdi.scaleLength(50);
  const int col2 = gdi.scaleLength(200);

  if (cmd == "init") {
    checkBoxToolBar(gdi, { });

    gdi.fillDown();
    gdi.addString("", 10, "help:checkcards");

    gdi.dropLine();
    gdi.fillRight();
    gdi.pushX();
    gdi.addButton("CCSReport", "Rapport", SportIdentCB);
    gdi.addButton("CCSClear", "Nollställ", SportIdentCB,
      "Nollställ minnet; markera alla brickor som icke avbockade");
    gdi.addButton("CCSPrint", "Skriv ut...", SportIdentCB);

    gdi.popX();
    gdi.dropLine(3);
    gdi.fillDown();
    gdi.setRestorePoint("CCSInit");
    showCheckCardStatus(gdi, "fillrunner");
    showCheckCardStatus(gdi, "stat");
    showCheckCardStatus(gdi, "tickoff");
    return;
  }
  else if (cmd == "fillrunner") {
    oe->getRunners(0, 0, r);

    for (size_t k = 0; k < r.size(); k++) {
      int cno = r[k]->getCardNo();
      if (cno == 0)
        continue;
      int cf = checkedCardFlags[cno];
      if (r[k]->getDI().getInt("CardFee") != 0)
        checkedCardFlags[cno] = CardNumberFlags(cf | CNFUsed);
      else
        checkedCardFlags[cno] = CardNumberFlags(cf | CNFNotRented);
    }
  }
  else if (cmd == "stat") {
    vector<int> count;
    gdi.addString("CardInfo", fontMediumPlus, getCardInfo(true, count));
    gdi.addString("CardTicks", 0, getCardInfo(false, count));
    if (count[CNFCheckedRentAndNotRent] + count[CNFRentAndNotRent] > 0) {
      oe->getRunners(0, 0, r);
      stable_sort(r.begin(), r.end(), compareCardNo);
      gdi.dropLine();
      string msg = "Brickor markerade som både uthyrda och egna: X#" + itos(count[CNFCheckedRentAndNotRent] + count[CNFRentAndNotRent]);
      gdi.addString("", 1, msg).setColor(colorDarkRed);
      gdi.dropLine(0.5);
      for (size_t k = 0; k < r.size(); k++) {
        int cno = r[k]->getCardNo();
        if (cno == 0 || r[k]->getRaceNo() > 0)
          continue;

        if (checkedCardFlags[cno] == CNFCheckedRentAndNotRent ||
          checkedCardFlags[cno] == CNFRentAndNotRent) {
          int yp = gdi.getCY();
          wstring cp = r[k]->getCompleteIdentification(oRunner::IDType::OnlyThis);
          bool hire = r[k]->isRentalCard();
          wstring info = hire ? (L" (" + lang.tl("Hyrd") + L")") : L"";
          gdi.addStringUT(yp, cx, 0, itow(cno) + info);
          gdi.addStringUT(yp, cx + col2, 0, cp);
        }
      }
    }
  }
  else if (cmd == "report") {
    oe->getRunners(0, 0, r);
    stable_sort(r.begin(), r.end(), compareCardNo);
    bool showHead = false;
    int count = 0;
    for (size_t k = 0; k < r.size(); k++) {
      int cno = r[k]->getCardNo();
      if (cno == 0)
        continue;
      if (r[k]->getRaceNo() > 0)
        continue;
      CardNumberFlags f = checkedCardFlags[cno];
      if (f == CNFRentAndNotRent || f == CNFUsed) {
        if (!showHead) {
          gdi.dropLine();
          string msg = "Uthyrda brickor som inte avbockats";
          gdi.addString("", fontMediumPlus, msg);
          gdi.fillDown();
          gdi.dropLine(0.5);
          showHead = true;
        }
        int yp = gdi.getCY();
        gdi.addStringUT(yp, cx, 0, itos(++count));
        gdi.addStringUT(yp, cx + col1, 0, itos(cno));
        wstring cp = r[k]->getCompleteIdentification(oRunner::IDType::OnlyThis);

        if (r[k]->getStatus() != StatusUnknown)
          cp += L" " + r[k]->getStatusS(true, true);
        else
          cp += makeDash(L" -");

        int s = r[k]->getStartTime();
        int f = r[k]->getFinishTime();
        if (s > 0 || f > 0) {
          cp += L", " + (s > 0 ? r[k]->getStartTimeS() : wstring(L"?")) + makeDash(L" - ")
            + (f > 0 ? r[k]->getFinishTimeS(false, SubSecond::Auto) : wstring(L"?"));
        }
        gdi.addStringUT(yp, cx + col2, 0, cp);
      }
    }

    if (!showHead) {
      gdi.dropLine();
      string msg = "Alla uthyrda brickor har bockats av.";
      gdi.addString("", fontMediumPlus, msg).setColor(colorGreen);
    }
  }
  else if (cmd == "tickoff") {
    SICard sic(ConvertedTimeStatus::Hour24);
    for (map<int, CardNumberFlags>::const_iterator it = checkedCardFlags.begin();
      it != checkedCardFlags.end(); ++it) {
      int stat = it->second;
      if (stat & CNFChecked) {
        sic.CardNumber = it->first;
        checkCard(gdi, sic, false);
      }
    }
    gdi.refresh();
    return;
  }
  checkHeader = false;
  gdi.dropLine();
}


class ResetHiredCard : public GuiHandler {
  oEvent* oe;

public:
  void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
    if (type == GuiEventType::GUI_LINK) {
      TextInfo& ti = dynamic_cast<TextInfo&>(info);
      int c = _wtoi(ti.text.c_str());
      if (gdi.ask(L"Vill du ta bort brickan från hyrbrickslistan?")) {
        oe->setHiredCard(c, false);
        ti.text = L"-";
        ti.setHandler(nullptr);
        gdi.refreshFast();
      }
    }
  }

  ResetHiredCard(oEvent* oe) : oe(oe) {}
};

GuiHandler* TabSI::getResetHiredCardHandler() {
  if (!resetHiredCardHandler)
    resetHiredCardHandler = make_shared<ResetHiredCard>(oe);

  return resetHiredCardHandler.get();
}

void TabSI::registerHiredCard(gdioutput& gdi, const SICard& sic) {
  if (!oe->isHiredCard(sic.CardNumber))
    oe->setHiredCard(sic.CardNumber, true);
  gdi.addStringUT(0, itos(sic.CardNumber)).setHandler(getResetHiredCardHandler());
  gdi.scrollToBottom();
  gdi.refresh();
}

void TabSI::checkCard(gdioutput& gdi, const SICard& card, bool updateAll) {
  bool wasChecked = (checkedCardFlags[card.CardNumber] & CNFChecked) != 0 && updateAll;

  checkedCardFlags[card.CardNumber] = CardNumberFlags(checkedCardFlags[card.CardNumber] | CNFChecked);
  vector<int> count;
  if (!checkHeader) {
    checkHeader = true;
    gdi.addString("", fontMediumPlus, "Avbockade brickor:");
    gdi.dropLine(0.5);
    cardPosX = gdi.getCX();
    cardPosY = gdi.getCY();
    cardOffsetX = gdi.scaleLength(60);
    cardNumCol = 12;
    cardCurrentCol = 0;
  }

  if (updateAll) {
    gdi.setTextTranslate("CardInfo", getCardInfo(true, count));
    gdi.setTextTranslate("CardTicks", getCardInfo(false, count));
  }
  TextInfo& ti = gdi.addStringUT(cardPosY, cardPosX + cardCurrentCol * cardOffsetX, 0, itos(card.CardNumber));
  if (wasChecked)
    ti.setColor(colorRed);
  if (++cardCurrentCol >= cardNumCol) {
    cardCurrentCol = 0;
    cardPosY += gdi.getLineHeight();
  }

  if (updateAll) {
    gdi.scrollToBottom();
    gdi.refreshFast();
  }
}

void TabSI::generatePayModeWidget(gdioutput& gdi) const {
  vector< pair<wstring, size_t> > pm;
  oe->getPayModes(pm);
  assert(pm.size() > 0);
  if (pm.size() == 1) {
    assert(pm[0].second == 0);
    gdi.dropLine(0.2);
    gdi.addCheckbox("Paid", L"#" + pm[0].first, SportIdentCB, storedInfo.hasPaid);
    gdi.dropLine(-0.2);
  }
  else {
    pm.insert(pm.begin(), make_pair(lang.tl(L"Faktureras"), 1000));
    gdi.addSelection("PayMode", 110, 100, SportIdentCB);
    gdi.setItems("PayMode", pm);
    gdi.selectItemByData("PayMode", storedInfo.payMode);
    gdi.autoGrow("PayMode");
  }
}

bool TabSI::writePayMode(gdioutput& gdi, int amount, oRunner& r) {
  int paid = 0;
  bool hasPaid = false;

  if (gdi.hasWidget("PayMode"))
    hasPaid = gdi.getSelectedItem("PayMode").first != 1000;

  bool fixPay = gdi.isChecked("Paid");
  if (hasPaid || fixPay) {
    paid = amount;
  }

  r.getDI().setInt("Paid", paid);
  if (hasPaid) {
    r.setPaymentMode(gdi.getSelectedItem("PayMode").first);
  }
  return hasPaid || fixPay;
}

void TabSI::addToPrintQueue(pRunner r) {
  uint64_t t = GetTickCount64();
  printPunchRunnerIdQueue.emplace_back(t, r->getId());
}

bool TabSI::checkpPrintQueue(gdioutput& gdi) {
  if (printPunchRunnerIdQueue.empty())
    return false;
  size_t printLen = oe->getPropertyInt("NumSplitsOnePage", 3);
  if (printPunchRunnerIdQueue.size() < printLen) {
    uint64_t t = GetTickCount64();
    unsigned diff = abs(int(t - printPunchRunnerIdQueue.front().first)) / 1000;

    if (diff < (unsigned)oe->getPropertyInt("SplitPrintMaxWait", 60))
      return false; // Wait a little longer
  }

  gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
  vector<int> mp;
  for (size_t m = 0; m < printLen && !printPunchRunnerIdQueue.empty(); m++) {
    int rid = printPunchRunnerIdQueue.front().second;
    printPunchRunnerIdQueue.pop_front();
    pRunner r = oe->getRunner(rid, 0);
    if (r) {
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Quiet);
      r->printSplits(gdiprint);
    }
    gdiprint.dropLine(4);
  }

  printProtected(gdi, gdiprint);
  //gdiprint.print(splitPrinter, oe, false, true);
  return true;
}

void TabSI::printSIInfo(gdioutput& gdi, const wstring& port) const {
  vector<pair<bool, wstring>> info;
  gdi.fillDown();
  gSI->getInfoString(port, info);
  for (size_t j = 0; j < info.size(); j++) {
    if (info[j].first)
      gdi.addStringUT(1, info[j].second).setColor(colorDarkRed);
    else
      gdi.addStringUT(0, info[j].second);
  }
}

oClub* TabSI::extractClub(gdioutput& gdi) const {
  auto& db = oe->getRunnerDatabase();
  oClub* dbClub = nullptr;
  if (gdi.hasWidget("Club")) {
    int clubId = gdi.getExtraInt("Club");
    if (clubId > 0) {
      dbClub = db.getClub(clubId - 1);
      if (dbClub && !stringMatch(dbClub->getName(), gdi.getText("Club")))
        dbClub = nullptr;
    }
    if (dbClub == nullptr) {
      dbClub = db.getClub(gdi.getText("Club"));
    }
  }
  return dbClub;
}

RunnerWDBEntry* TabSI::extractRunner(gdioutput& gdi) const {
  auto& db = oe->getRunnerDatabase();

  int rId = gdi.getExtraInt("Name");
  wstring name = gdi.getText("Name");
  RunnerWDBEntry* dbR = nullptr;
  if (rId > 0) {
    dbR = db.getRunnerByIndex(rId - 1);
    if (dbR) {
      wstring fname = dbR->getFamilyName();
      wstring gname = dbR->getGivenName();

      if (wcsstr(name.c_str(), fname.c_str()) == nullptr || wcsstr(name.c_str(), gname.c_str()) == nullptr)
        dbR = nullptr;
    }
  }
  if (dbR == nullptr) {
    oClub* dbClub = extractClub(gdi);
    dbR = db.getRunnerByName(name, dbClub ? dbClub->getId() : 0, 0);
  }
  return dbR;
}

void TabSI::DirectEntryGUI::updateFees(gdioutput& gdi, const pClass cls, int age) {
  int fee = cls->getEntryFee(getLocalDate(), age);
  auto fees = cls->getAllFees();
  gdi.setItems("Fee", fees);
  if (fee > 0) {
    gdi.selectItemByData("Fee", fee);
    gdi.setText("Fee", tabSI->oe->formatCurrency(fee));
  }
  else if (!fees.empty())
    gdi.selectFirstItem("Fee");

  tabSI->updateEntryInfo(gdi);
}

void TabSI::DirectEntryGUI::handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
  if (type == GUI_FOCUS) {
    InputInfo& ii = dynamic_cast<InputInfo&>(info);
    /*if (ii.getExtraInt()) {
      ii.setExtra(0);
      gdi.setInputFocus(ii.id, true);
    }*/
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo& lbi = dynamic_cast<ListBoxInfo&>(info);
    if (lbi.id == "Class") {
      int clsId = lbi.data;
      pClass cls = tabSI->oe->getClass(clsId);
      if (cls) {
        int age = 0;
        auto r = tabSI->extractRunner(gdi);
        if (r) {
          int year = r->getBirthYear();
          if (year > 0)
            age = getThisYear() - year;
        }
        updateFees(gdi, cls, age);
      }
    }
  }
  else if (type == GUI_COMBOCHANGE) {
    ListBoxInfo& combo = dynamic_cast<ListBoxInfo&>(info);
    bool show = false;
    if (tabSI->useDatabase && combo.id == "Club" && combo.text.length() > 1) {
      auto clubs = tabSI->oe->getRunnerDatabase().getClubSuggestions(combo.text, 20);
      if (!clubs.empty()) {
        auto& ac = gdi.addAutoComplete(combo.id);
        ac.setAutoCompleteHandler(this->tabSI);
        vector<AutoCompleteRecord> items;
        for (auto club : clubs)
          items.emplace_back(club->getDisplayName(), -int(items.size()), club->getName(), club->getId());

        ac.setData(items);
        ac.show();
        show = true;
      }
    }
    if (!show) {
      gdi.clearAutoComplete(combo.id);
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo& ii = dynamic_cast<InputInfo&>(info);
    bool show = false;
    if (tabSI->showDatabase() && ii.id == "Name") {
      auto& db = tabSI->oe->getRunnerDatabase();
      if (ii.text.length() > 1) {
        auto dbClub = tabSI->extractClub(gdi);
        auto rw = db.getRunnerSuggestions(ii.text, dbClub ? dbClub->getId() : 0, 10);
        if (!rw.empty()) {
          auto& ac = gdi.addAutoComplete(ii.id);
          ac.setAutoCompleteHandler(this->tabSI);

          ac.setData(getRunnerAutoCompelete(db, rw, dbClub));
          ac.show();
          show = true;
        }
      }
      if (!show) {
        gdi.clearAutoComplete(ii.id);
      }
    }
  }
}

vector<AutoCompleteRecord> TabSI::getRunnerAutoCompelete(RunnerDB& db, const vector< pair<RunnerWDBEntry*, int>>& rw, pClub dbClub) {
  vector<AutoCompleteRecord> items;
  set<wstring> used;
  wstring ns;
  map<wstring, int> strCount;
  for (int i = 0; i < 2; i++) {
    bool needRerun = false;
    for (auto r : rw) {
      if (dbClub) {
        ns = r.first->getNameCstr();
      }
      else {
        ns = r.first->getNameCstr();
        int clubId = r.first->dbe().clubNo;
        auto club = db.getClub(clubId);
        if (club) {
          ns += L", " + club->getDisplayName();
        }
      }
      if (i == 0)
        ++strCount[ns];

      if (strCount[ns] > 1) {
        needRerun = true;
        int y = r.first->getBirthYear();
        if (y > 0)
          ns += L" (" + itow(getThisYear() - y) + L")";
      }

      items.emplace_back(ns, -int(items.size()), r.first->getNameCstr(), r.second);
    }
    if (!needRerun)
      break;
    else if (i == 0) {
      items.clear();
    }
  }
  return items;
}

void TabSI::handleAutoComplete(gdioutput& gdi, AutoCompleteInfo& info) {
  auto bi = gdi.setText(info.getTarget(), info.getCurrent().c_str());
  if (bi) {
    int ix = info.getCurrentInt();

    bi->setExtra(ix + 1);
    if (bi->id == "Name" && ix >= 0) {
      auto r = oe->getRunnerDatabase().getRunnerByIndex(ix);
      int year = r ? r->getBirthYear() : 0;
      if (year > 0) {
        int clsId = gdi.getSelectedItem("Class").first;
        pClass cls = oe->getClass(clsId);
        if (cls) {
          directEntryGUI.updateFees(gdi, cls, getThisYear() - year);
        }
      }
      if (r) {
        if (gdi.hasWidget("Club") && r->dbe().clubNo) {
          if (gdi.getText("Club").empty()) {
            auto pclub = oe->getRunnerDatabase().getClub(r->dbe().clubNo);
            if (pclub)
              gdi.setText("Club", pclub->getName());
          }
        }
        if (gdi.hasWidget("CardNo") && r->dbe().cardNo) {
          if (gdi.getText("CardNo").empty())
            gdi.setText("CardNo", r->dbe().cardNo);
        }

        TabRunner::autoCompleteRunner(gdi, r);
      }
    }
    else if (bi->id == "Runners" && ix >= 0) {
      auto r = oe->getRunnerDatabase().getRunnerByIndex(ix);
      if (gdi.hasWidget("Club") && r->dbe().clubNo) {
        if (gdi.getText("Club").empty()) {
          auto pclub = oe->getRunnerDatabase().getClub(r->dbe().clubNo);
          if (pclub)
            gdi.setText("Club", pclub->getName());
        }
      }
    }
  }
  gdi.clearAutoComplete("");
  gdi.TabFocus(1);
}

bool TabSI::showDatabase() const {
  return useDatabase && oe->useRunnerDb();
}

void TabSI::playReadoutSound(SND type) {
  if (!oe->getPropertyInt("PlaySound", 1))
    return;
  int res = -1;
  wstring fn;
  switch (type) {
  case SND::OK:
    fn = oe->getPropertyString("SoundOK", L"");
    res = 50;
    break;
  case SND::NotOK:
    fn = oe->getPropertyString("SoundNotOK", L"");
    res = 52;
    if (fn == L"none") {
      fn = oe->getPropertyString("SoundAction", L"");
      res = 53;
    }
    break;
  case SND::Leader:
    fn = oe->getPropertyString("SoundLeader", L"");
    res = 51;
    if (fn == L"none") {
      fn = oe->getPropertyString("SoundOK", L"");
      res = 50;
    }
    break;
  case SND::ActionNeeded:
    fn = oe->getPropertyString("SoundAction", L"");
    res = 53;
    if (fn == L"none") {
      fn = oe->getPropertyString("SoundNotOK", L"");
      res = 52;
    }
    break;
  }

  if (fn == L"none")
    return;

  if (checkedSound.count(fn) || (!fn.empty() && fileExists(fn))) {
    playSoundFile(fn);
    checkedSound.insert(fn);
  }
  else {
    playSoundResource(res);
  }
}

void TabSI::playSoundResource(int res) const {
  PlaySound(MAKEINTRESOURCE(res), GetModuleHandle(nullptr), SND_RESOURCE | SND_ASYNC);
  //OutputDebugString((L"Play: " + itow(res)).c_str());

}

void TabSI::playSoundFile(const wstring& file) const {
  PlaySound(file.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
  //OutputDebugString(file.c_str());
}

void TabSI::showReadoutStatus(gdioutput& gdi, const oRunner* r,
  const oCard* oCrd, SICard* siCrd,
  const wstring& missingPunchList) {
  gdi.clearPage(false);
  gdi.hideBackground(true);
  int w, h;
  gdi.getTargetDimension(w, h);
  double minS = min(h / 500.0, w / 700.0);
  gdi.scaleSize(minS / gdi.getScale(), false, gdioutput::ScaleOperation::NoRefresh);

  int mrg = 20;
  int lh = gdi.getLineHeight(boldHuge, nullptr);
  bool addAutoClear = false;
  bool rentalCard = false;
  wstring cardVoltage;
  oCard::BatteryStatus bt = oCard::BatteryStatus::OK;
  RECT rc;
  rc.top = mrg;
  rc.bottom = h - mrg;
  rc.left = mrg;
  rc.right = w - mrg;
  GDICOLOR bgColor = GDICOLOR::colorDefault;

  if (r != nullptr) {
    if (r->isRentalCard() || oe->isHiredCard(r->getCardNo()))
      rentalCard = true;

    gdi.addStringUT(h / 3, mrg, boldHuge | textCenter, r->getCompleteIdentification(oRunner::IDType::OnlyThis), w - 2 * mrg);
    gdi.setCX(max(w / 8, mrg * 2));
    gdi.setCY(h / 3 + lh * 2);
    gdi.pushX();

    if (r->getCard()) {
      cardVoltage = r->getCard()->getCardVoltage();
      bt = r->getCard()->isCriticalCardVoltage();
    }

    if (r->isStatusUnknown(true, true)) {
      gdi.fillRight();
      if (r->getStartTime() > 0) {
        gdi.addString("", fontMediumPlus, "Start:");
        gdi.addStringUT(boldHuge, r->getStartTimeS());
      }
    }
    else if (r->isStatusOK(true, true)) {
      bgColor = colorLightGreen;
      gdi.fillRight();
      gdi.addStringUT(boldLarge, r->getClass(true));

      gdi.addString("", fontLarge, "   Start:").setColor(colorGreyBlue);
      gdi.addStringUT(boldLarge, r->getStartTimeS());
      gdi.addString("", fontLarge, "   Mål:").setColor(colorGreyBlue);
      gdi.addStringUT(boldLarge, r->getFinishTimeS(false, SubSecond::Auto));
      gdi.popX();
      gdi.dropLine(3);

      if (r->getStatusComputed(true) == StatusNoTiming ||
        (r->getClassRef(false) && r->getClassRef(true)->getNoTiming())) {
        gdi.addString("", fontMediumPlus, "Status:").setColor(colorGreyBlue);
        gdi.fillDown();
        gdi.addString("", boldHuge, "Godkänd");
        gdi.popX();
      }
      else {
        gdi.addString("", fontMediumPlus, " Tid:").setColor(colorGreyBlue);
        bool showPlace = r->getStatusComputed(true) == StatusOK;
        if (!showPlace)
          gdi.fillDown();

        wstring ts = getTimeString(r);
        gdi.addStringUT(boldHuge, ts);

        if (showPlace) {
          gdi.addString("", fontMediumPlus, " Placering:").setColor(colorGreyBlue);
          gdi.fillDown();
          gdi.addStringUT(boldHuge, getPlace(r) + L"  ");
        }
        gdi.popX();
        gdi.fillRight();
        wstring ta = getTimeAfterString(r);
        if (!ta.empty()) {
          gdi.addString("", fontMediumPlus, " Efter:").setColor(colorGreyBlue);
          gdi.addStringUT(boldHuge, ta + L"  ");
        }

        int miss = r->getMissedTime();
        if (miss > 0) {
          gdi.addString("", fontMediumPlus, " Bomtid:").setColor(colorGreyBlue);
          gdi.addStringUT(boldHuge, formatTime(miss, SubSecond::Off));
        }
      }
    }
    else if (r->payBeforeResult(false)) {
      bgColor = colorLightRed;
      gdi.fillDown();
      gdi.addString("", fontLarge, "Betalning av anmälningsavgift inte registrerad");
      gdi.dropLine();     
    }
    else {
      bgColor = colorLightRed;
      gdi.fillDown();
      gdi.addStringUT(fontLarge, r->getClass(true));
      gdi.dropLine();
      gdi.fillRight();
      gdi.addString("", fontMediumPlus, "Start:").setColor(colorGreyBlue);
      gdi.addStringUT(boldHuge, r->getStartTimeS());
      gdi.addString("", fontMediumPlus, " Mål:").setColor(colorGreyBlue);
      gdi.addStringUT(boldHuge, r->getFinishTimeS(false, SubSecond::Auto));
      gdi.addString("", fontMediumPlus, " Status:").setColor(colorGreyBlue);
      gdi.fillDown();
      gdi.addString("", boldHuge, r->getStatusS(true, true));
      gdi.popX();
      if (!missingPunchList.empty()) {
        gdi.fillRight();
        gdi.addString("", boldHuge, "Kontroll: ");
        gdi.fillDown();
        gdi.addStringUT(boldHuge, missingPunchList);
      }
    }
    addAutoClear = true;
  }
  else if (oCrd != nullptr) {
    if (oe->isHiredCard(oCrd->getCardNo()))
      rentalCard = true;

    bgColor = colorLightRed;
    gdi.addString("", h / 3, mrg, boldHuge | textCenter, "Okänd bricka", w - 2 * mrg);
    gdi.addString("", h / 3 + lh * 2, mrg, boldHuge | textCenter, itow(oCrd->getCardNo()), w - 2 * mrg);
    cardVoltage = oCrd->getCardVoltage();
    bt = oCrd->isCriticalCardVoltage();

    addAutoClear = true;
  }
  else if (siCrd != nullptr) {
    rentalCard = oe->isHiredCard(siCrd->CardNumber);

    printCard(gdi, 10, -1, siCrd, true);
    cardVoltage = oCard::getCardVoltage(siCrd->miliVolt);
    bt = oCard::isCriticalCardVoltage(siCrd->miliVolt);

    addAutoClear = true;
  }
  else {
    gdi.addImage("", h / 3, w / 2 - 64, 0, L"513", gdi.scaleLength(128));
  }

  gdi.dropLine(3);
  int cyBelow = gdi.getCY();

  if (bgColor != GDICOLOR::colorDefault)
    gdi.addRectangle(rc, bgColor);

  bool problem = bgColor == GDICOLOR::colorLightRed;

  int off = 0;
  if (problem) {
    RECT rcTop, rcBottom;
    rcTop.top = mrg+2;
    rcTop.bottom = max(h / 8, gdi.scaleLength(30));
    rcBottom.bottom = h - mrg - 2;
    rcBottom.top = h - max(h / 8, gdi.scaleLength(30));
    rcBottom.left = rcTop.left = mrg+2;
    rcBottom.right = rcTop.right = w - mrg-2;
    if (!rentalCard) 
      gdi.addRectangle(rcTop, GDICOLOR::colorRed);
    else {
      int mid = (rcTop.top + rcTop.bottom) / 2;
      rcTop.bottom = mid;
      off = mid;
      gdi.addRectangle(rcTop, GDICOLOR::colorRed);
    }
    gdi.addRectangle(rcBottom, GDICOLOR::colorRed);
  }

  rc.top += mrg / 2 + off;
  rc.right -= mrg / 2;
  rc.left += mrg / 2;
  rc.bottom = h / 6 + off;

  if (rentalCard) {
    gdi.addRectangle(rc, colorYellow);
    gdi.addString("", rc.top + (rc.bottom - rc.top) / 3, mrg, boldHuge | textCenter,
      "Vänligen återlämna hyrbrickan.", w - 3 * mrg);
  }

  if (!cardVoltage.empty()) {
    rc.top = cyBelow;
    rc.right -= mrg;
    rc.left += mrg;
    rc.bottom = h - mrg * 3;

    if (bt == oCard::BatteryStatus::OK)
      gdi.addRectangle(rc, colorMediumGreen);
    else if (bt == oCard::BatteryStatus::Warning)
      gdi.addRectangle(rc, colorMediumYellow);
    else
      gdi.addRectangle(rc, colorMediumRed);

    gdi.addString("", rc.top + mrg, rc.left + mrg, fontMediumPlus, "Batteristatus");
    gdi.setCY(min(rc.top + mrg, rc.bottom - gdi.scaleLength(mrg * 5)));
    gdi.setCX(w / 3);

    gdi.fillRight();
    gdi.dropLine();
    gdi.addString("", fontMediumPlus, "Spänning:");
    gdi.addStringUT(boldHuge, cardVoltage);

    wstring warning;
    if (bt == oCard::BatteryStatus::Bad)
      warning = lang.tl("Replace[battery]");
    else if (bt == oCard::BatteryStatus::Warning)
      warning = lang.tl("Low");
    else
      warning = lang.tl("OK");

    gdi.addStringUT(boldHuge, L"(" + warning + L")");
  }

  if (addAutoClear) {
    TimerInfo& ti = gdi.addTimeoutMilli(20000, "", nullptr);

    class LoadDef : public GuiHandler {
    public:
      TabSI* si;
      LoadDef(TabSI* si) : si(si) {}
      void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) final {
        si->showReadoutStatus(gdi, nullptr, nullptr, nullptr, L"");
      }
    };

    ti.setHandler(make_shared<LoadDef>(this));
  }

  gdi.refresh();
}

void TabSI::changeMapping(gdioutput& gdi) const {
  OnlineInput::controlMappingView(gdi, oe, SportIdentCB, 0);
  fillMappings(gdi);

  gdi.popX();
  gdi.fillDown();
  gdi.setCY(gdi.getHeight());
  gdi.addButton("CloseMapping", "Stäng", SportIdentCB);
  gdi.dropLine();
  gdi.scrollToBottom();
}

void TabSI::fillMappings(gdioutput& gdi) const {
  getSI(gdi).clearSpecialMappings(); // Synch with stored
  gdi.clearList("Mappings");
  for (auto &[code, type] : oe->getPunchMapping()) {
    if (code > 0 && code < 1024)
      getSI(gdi).addSpecialMapping(code, type); // Synch with stored
    gdi.addItem("Mappings", itow(code) + L" \u21A6 " + oPunch::getType(type, nullptr), code);
  }
}

void TabSI::readTestData(gdioutput& gdi) {
  testCardNumber = gdi.getTextNo("SI");
  if (gdi.hasWidget("PunchTime")) {
    testPunchTime = gdi.getText("PunchTime");
  }
  else {
    useTestCheck = gdi.isChecked("HasCheck");
    useTestStart = gdi.isChecked("HasStart");
    useTestFinish = gdi.isChecked("HasFinish");

    testStartTime = gdi.getText("Start");
    testFinishTime = gdi.getText("Finish");
    testCheckTime = gdi.getText("Check");

    if (NC > testControls.size())
      testControls.resize(NC);

    for (int i = 0; i < NC; i++) 
      testControls[i] = gdi.getTextNo("C" + itos(i+1));
  }
}


class RequestStart : public GuiHandler {
private:
  oEvent* oe;
  TabSI* si;
  bool active = false;
  int storedRunner = 0;
  int storedCard = 0;

  void save(gdioutput& gdi) {
    gdi.getSelection("Classes", selectedClasses);
    firstStart = oe->getRelativeTime(gdi.getText("FirstStart"));
    lastStart = oe->getRelativeTime(gdi.getText("LastStart"));
    if (lastStart > 0)
      oe->setProperty("RequestLastStart", gdi.getText("LastStart"));
    
    interval = convertAbsoluteTimeMS(gdi.getText("Interval"));
    if (interval != NOTIME)
      oe->setProperty("RequestStartInterval", gdi.getText("Interval"));
    
    allowSameCourse = gdi.isChecked("AllowSameCourse");
    allowSameCourseNeighbour = gdi.isChecked("AllowSameCourseNext");
    allowSameFirstControl = gdi.isChecked("AllowSameControl");
    allowClubNeighbour = gdi.isChecked("AllowSameClub");
    maxParallel = gdi.getTextNo("MaxParallel");

    oe->setProperty("RequestSameCourse", allowSameCourse);
    oe->setProperty("RequestSameCourseNeighbour", allowSameCourseNeighbour);
    oe->setProperty("RequestSameFirstControl", allowSameFirstControl);
    oe->setProperty("RequestClubNeighbour", allowClubNeighbour);
    oe->setProperty("RequestMaxParallel", maxParallel);

    timeToSuggestion = convertAbsoluteTimeMS(gdi.getText("TimeToFirstStart"));
    if (timeToSuggestion != NOTIME)
      oe->setProperty("RequestMinTime", gdi.getText("TimeToFirstStart"));

    provideSuggestions = gdi.isChecked("SuggestTimes");
    oe->setProperty("RequestProvideSuggestion", provideSuggestions);

    suggestionDistance = convertAbsoluteTimeMS(gdi.getText("TimeInterval"));
    
    if (provideSuggestions && suggestionDistance != NOTIME)
      oe->setProperty("RequestSuggestInterval", gdi.getText("TimeInterval")); 
  }
  
  void makeSuggestions(gdioutput& gdi, pRunner r, int cardNo, int now) {
    oe->synchronizeList(oListId::oLRunnerId);
    vector<int> times;
    int suggestionDist = this->suggestionDistance;
    if ((suggestionDist % interval) != 0)
      suggestionDist = suggestionDist - (suggestionDist % interval) + interval;

    int lastTime = now;
    while (lastTime <= lastStart) {
      int t = oe->requestStartTime(r->getId(), lastTime, interval, lastStart,
                                   maxParallel, allowSameCourse, allowSameCourseNeighbour,
                                   allowSameFirstControl, allowClubNeighbour);
      if (t > 0) {
        times.push_back(t);
        lastTime = t + suggestionDist;
        suggestionDist = (suggestionDist * 3) / 2;
        if (suggestionDist > timeConstMinute * 30)
          suggestionDist = timeConstMinute * 20;

        if ((suggestionDist % interval) != 0)
          suggestionDist = suggestionDist - (suggestionDist % interval) + interval;
      }
      else
        break;
    }
    
    gdi.restore("SelectStart");
    gdi.setRestorePoint("SelectStart");
    gdi.fillDown();
    gdi.dropLine(2);
    gdi.addString("", boldHuge, L"Välj starttid för X#" + r->getName() + L", " + r->getClub());
    gdi.dropLine(3);

    int bz = gdi.scaleLength(150);
    int xp = gdi.getCX();
    int base = xp;
    
    int yp = gdi.getCY();
    int w, h;
    gdi.getTargetDimension(w, h);
    int margin = gdi.scaleLength(20);
    
    auto raw = shared_from_this();

    
    for (int t : times) {
      wstring ts = (t % timeConstMinute) == 0 ? oe->getAbsTimeHM(t) : oe->getAbsTime(t);

      gdi.addButton(xp, yp, bz, bz, "SelectStart", ts, 
                    gdiFonts::boldHuge, nullptr, L"", false, false).setHandler(raw).setExtra(t);

      xp += bz + margin;
      if (xp > w - 2 * bz) {
        xp = base;
        yp += bz + margin;
      }
    }
    gdi.addButton(xp, yp, bz, bz, "Cancel", L"Avbryt",
      gdiFonts::boldLarge, SportIdentCB, L"", false, false).setCancel();

    storedRunner = r->getId();
    storedCard = cardNo;

    gdi.dropLine(4);
    gdi.refresh();
  }

public:

  bool showSettings() {
    return !active;
  }

  int firstStart = 0;
  int lastStart = 0;
  int interval = 0;
  int timeToSuggestion = 0;
  int suggestionDistance = 0;
  set<int> selectedClasses;
  int maxParallel = 10;
  bool allowSameCourse = false;
  bool allowSameCourseNeighbour = false;
  bool allowSameFirstControl = true;
  bool allowClubNeighbour = false;

  bool provideSuggestions = false;
  
  RequestStart(TabSI* si) : oe(si->getEvent()), si(si) {
    int now = oe->getRelativeTime(oe->getCurrentTimeS());
    now -= (now % (timeConstMinute*15)) - (timeConstMinute*15);
    if (now > timeConstHour * 20)
      now = timeConstHour;

    firstStart = max(timeConstHour, now);
    lastStart = oe->getRelativeTime(oe->getPropertyString("RequestLastStart", oe->getAbsTime(now)));
    lastStart = min(max(lastStart, firstStart + timeConstHour * 2), firstStart+timeConstHour*6);

    interval = convertAbsoluteTimeMS(oe->getPropertyString("RequestStartInterval", L"2:00"));
    if (interval == NOTIME)
      interval = timeConstMinute * 2;

    timeToSuggestion = convertAbsoluteTimeMS(oe->getPropertyString("RequestMinTime", L"30:00"));
    if (timeToSuggestion == NOTIME)
      timeToSuggestion = timeConstMinute * 30;

    provideSuggestions = oe->getPropertyBool("RequestProvideSuggestion", provideSuggestions);

    suggestionDistance = convertAbsoluteTimeMS(oe->getPropertyString("RequestSuggestInterval", L"10:00"));
    if (suggestionDistance == NOTIME)
      suggestionDistance = suggestionDistance * 10;

    allowSameCourse = oe->getPropertyBool("RequestSameCourse", allowSameCourse);
    allowSameCourseNeighbour = oe->getPropertyBool("RequestSameCourseNeighbour", allowSameCourseNeighbour);
    allowSameFirstControl = oe->getPropertyBool("RequestSameFirstControl", allowSameFirstControl);
    allowClubNeighbour = oe->getPropertyBool("RequestClubNeighbour", allowClubNeighbour);
    maxParallel = oe->getPropertyInt("RequestMaxParallel", maxParallel);

    vector<pClass> cls;
    oe->getClasses(cls, false);
    for (pClass c : cls) {
      if (c->hasRequestStart())
        selectedClasses.insert(c->getId());
    }
  };

  RequestStart(const RequestStart &) = delete;
  RequestStart& operator=(const RequestStart&) = delete;
  RequestStart(RequestStart&&) = delete;
  RequestStart& operator=(RequestStart&&) = delete;

  void loadPage(gdioutput& gdi, shared_ptr<RequestStart> &h) {
    bool settings = h->showSettings();
    TabSI& tsi = dynamic_cast<TabSI&>(*gdi.getTabs().get(TSITab));

    gdi.fillDown();
    if (oe->isKiosk()) {
      int width, height;
      gdi.getTargetDimension(width, height);
      gdi.addImage("", 0, L"513", gdi.scaleLength(128));
      gdi.dropLine(3);
      RECT rc;
      rc.top = gdi.getCY();
      rc.bottom = rc.top + 4;
      rc.left = gdi.scaleLength(30);
      rc.right = width - rc.left;
      gdi.addRectangle(rc, GDICOLOR::colorGreyBlue, false);

      gdi.addString("", gdi.getCY(), width/2, gdiFonts::boldHuge | textCenter, "Boka starttid");
    }
    else {
      gdi.addString("", fontLarge, "Boka starttid");

      if (settings) {
        gdi.dropLine();
        gdi.pushY();
        gdi.setRestorePoint("RequestSettings");
        gdi.addListBox("Classes", 220, 400, nullptr, L"Klasser:", L"", true);
        gdi.setTabStops("Classes", 170);
        gEvent->fillClasses(gdi, "Classes", {}, oEvent::extraNone, oEvent::filterNone);
        gdi.setSelection("Classes", h->selectedClasses);
        gdi.dropLine();

        auto classesBox = gdi.getDimensionSince("RequestSettings");

        gdi.fillRight();
        gdi.addButton("SelectDefault", "Återställ").setHandler(h);
        gdi.addButton("SelectAll", "Välj alla").setHandler(h);
        gdi.addButton("SelectNone", "Välj ingen").setHandler(h);

        gdi.setCX(classesBox.right + gdi.scaleLength(20));
        gdi.popY();
        gdi.pushX();

        gdi.fillRight();
        gdi.addInput("FirstStart", oe->getAbsTime(h->firstStart), 10, nullptr, L"Första starttid:");
        gdi.addInput("LastStart", oe->getAbsTime(h->lastStart), 10, nullptr, L"Sista starttid:");
        gdi.fillDown();
        gdi.addInput("Interval", formatTimeMS(h->interval, true), 10, nullptr, L"Startintervall (minuter):");
        gdi.popX();
        gdi.dropLine(1);

        gdi.fillDown();
        gdi.addInput("TimeToFirstStart", formatTimeMS(h->timeToSuggestion, true), 10, nullptr, L"Minsta tid till start (minuter):");
        
        gdi.dropLine();
        gdi.addCheckbox("SuggestTimes", "Välj från flera förslag", nullptr, h->provideSuggestions).setHandler(h);
        
        gdi.addInput("TimeInterval", formatTimeMS(h->suggestionDistance, true), 10, nullptr, L"Avstånd mellan förslag (minuter):");
        gdi.setInputStatus("TimeInterval", h->provideSuggestions);

        gdi.popX();
        gdi.dropLine(0.5);
        gdi.addCheckbox("AllowSameCourse", "Tillåt klass med samma bana på samma starttid", nullptr, h->allowSameCourse);
        gdi.addCheckbox("AllowSameCourseNext", "Tillåt klass med samma bana på stattid före/efter", nullptr, h->allowSameCourseNeighbour);
        gdi.addCheckbox("AllowSameControl", "Tillåt klass med samma första kontroll vid samma starttid", nullptr, h->allowSameFirstControl);
        gdi.addCheckbox("AllowSameClub", "Tillåt deltagare med samma klubb och klass på närliggande starttid", nullptr, h->allowClubNeighbour);
        gdi.dropLine(0.5);
        gdi.addInput("MaxParallel", itow(h->maxParallel), 10, 0, L"Max parallellt startande:");
       
        gdi.dropLine(2);
        gdi.fillRight();
        gdi.addButton("Save", "Aktivera").setDefault().setHandler(h);
        
        auto settingsBox = gdi.getDimensionSince("RequestSettings");
        gdi.setCX(settingsBox.right + gdi.scaleLength(20));
        gdi.popY();
        gdi.pushX();

        gdi.setOnClearCb("request", h);
        gdi.addString("", 10, "help:requeststart");
        return;
      }
      else {
        int now = oe->getRelativeTime(oe->getCurrentTimeS());
        if (now > timeConstHour * 20)
          now = timeConstMinute;

        now -= (now % timeConstMinute) - timeConstMinute;        
        wstring t = oe->getAbsTimeHM(max(firstStart, now + timeToSuggestion));
               
        gdi.pushX();
        gdi.dropLine(1);

        gdi.fillRight();
        gdi.addButton("Settings", "Inställningar...").setHandler(h);
        gdi.addButton("Simulate", "Simulering...").setHandler(h);
       
        if (tsi.anyActivePort())
          gdi.addButton("SaveKiosk", "Aktivera kioskläge").setHandler(h);

        gdi.popX();
        gdi.fillDown();

        gdi.dropLine(3);

        RECT rc;
        rc.top = gdi.getCY();
        rc.left = gdi.getCX();
        gdi.setCX(gdi.getCX() + gdi.getLineHeight());
        gdi.dropLine();
        gdi.pushX();

        gdi.addString("", fontMediumPlus, "Manuellt startönskemål");
        gdi.fillRight();
        gdi.addSelection("Runners", 200, 400, nullptr, L"Deltagare:");
        vector<pRunner> rList;
        oe->getRunners(selectedClasses, rList, true);
                
        sort(rList.begin(), rList.end(), [](const pRunner& a, const pRunner& b) -> bool {
          return CompareString(LOCALE_USER_DEFAULT, 0,
                               a->getName().c_str(), a->getName().length(),
                               b->getName().c_str(), b->getName().length()) == CSTR_LESS_THAN; });

        vector<pair<wstring, size_t>> rItem;
        for (pRunner r : rList) {
          if (r->getStartTime() == 0) {
            rItem.emplace_back(r->getCompleteIdentification(oRunner::IDType::OnlyThis) + L", " + r->getClass(true), r->getId());
          }
        }
                
        gdi.setItems("Runners", rItem);
        gdi.autoGrow("Runners");
        gdi.addInput("StartTime", t, 10, nullptr, L"Önskad starttid:");
        gdi.dropLine();
        gdi.addButton("GetStart", "Tilldela").setHandler(h);

        gdi.dropLine(3);
        rc.bottom = gdi.getCY();
        rc.right = gdi.getCX();
        gdi.addRectangle(rc, GDICOLOR::colorLightCyan);

        gdi.fillDown();
        gdi.setCX(rc.left);
        gdi.pushX();
      }
    }
    
    gdi.setData("RequestStart", 1);
    gdi.setRestorePoint("RequestStart");
 
    if (!settings) {
      punchInfo(gdi, !tsi.anyActivePort());
      gdi.dropLine(6);
    }
  }

  void punchInfo(gdioutput& gdi, bool showPortInfo) {
    gdi.dropLine(3);
    if (showPortInfo) {
      gdi.fillRight();
      gdi.addString("", textImage, itow(IDI_MEOSWARN));
      gdi.fillDown();
      gdi.addString("", 0, "Anslut en SI-enhet och aktivera den.");
      gdi.popX();
    }
    else {
      if (provideSuggestions)
        gdi.addString("", boldHuge, L"Stämpla för välja starttid");
      else
        gdi.addString("", boldHuge, L"Stämpla för att boka första lediga tid om X minuter#" + itow(timeToSuggestion / timeConstMinute));
    }
    gdi.dropLine(3);
  }

  void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) final {
    if (type == GuiEventType::GUI_TIMER) {
      TimerInfo& ti = dynamic_cast<TimerInfo &>(info);

      if (ti.id == "RequestedStart") {
        gdi.restore("RequestStart", false);
        punchInfo(gdi, false);
        gdi.refreshFast();
      }
    }
    else if (type == GuiEventType::GUI_BUTTON) {
      const ButtonInfo& bi = dynamic_cast<ButtonInfo&>(info);

      if (bi.id == "Settings") {
        active = false;
        si->loadPage(gdi);
      }
      else if (bi.id == "Save") {
        save(gdi);
        active = true;
        si->loadPage(gdi);
      }
      else if (bi.id == "SaveKiosk") {
        if (gdi.ask(L"ask:startkiosk")) {
          //save(gdi);
          active = true;
          oe->setKiosk();
          si->loadPage(gdi);
        }
      }
      else if (bi.id == "SelectAll") {
        vector<pClass> cls;
        oe->getClasses(cls, false);
        for (auto& c : cls)
          selectedClasses.insert(c->getId());
        gdi.setSelection("Classes", selectedClasses);
      }
      else if (bi.id == "SelectNone") {
        selectedClasses.clear();
        gdi.setSelection("Classes", selectedClasses);
      }
      else if (bi.id == "SelectDefault") {
        vector<pClass> cls;
        oe->getClasses(cls, false);
        selectedClasses.clear();
        for (pClass c : cls) {
          if (c->hasRequestStart())
            selectedClasses.insert(c->getId());
        }
        gdi.setSelection("Classes", selectedClasses);
      }
      else if (bi.id == "SuggestTimes") {
        gdi.setInputStatus("TimeInterval", gdi.isChecked(bi.id));
      }
      else if (bi.id == "SelectStart") {
        int t = bi.getExtraInt();
        pRunner r = oe->getRunner(storedRunner, 0);
        if (storedRunner <= 0)
          throw meosException("Internal error");

        gdi.restore("SelectStart", false);
        gdi.fillDown();
        requestStartTime(oe, gdi, r, t);
        gdi.refresh();
      }
      else if (bi.id == "Simulate") {
        if (gdi.ask(L"ask:simulatestart"))
          simulation(gdi);
      }
      else if (bi.id == "GetStart") {
        ListBoxInfo lbi;
        int t = oe->getRelativeTime(gdi.getText("StartTime"));
        if (t > 0 && gdi.getSelectedItem("Runners", lbi)) {
          pRunner r = oe->getRunner(lbi.data, 0);
          if (r) {
            requestStartTime(oe, gdi, r, t);
          }
        }
      }
    }
    else if (type == GuiEventType::GUI_CLEAR) {
      save(gdi);
    }
  }

  void handleCard(gdioutput& gdi, int cardNo) {
    if (!gdi.hasData("RequestStart"))
      return;

    oe->synchronizeList(oListId::oLRunnerId);
    pRunner r = oe->getRunnerByCardNo(cardNo, 0, oEvent::CardLookupProperty::ForReadout);

    gdi.restore("RequestStart", false);
    gdi.setRestorePoint("RequestStart");
    gdi.dropLine(4);

    if (!r) {
      gdi.addString("", gdiFonts::fontLarge, "Bricka X#" + itos(cardNo)).setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, "Okänd bricka").setColor(GDICOLOR::colorRed);
      gdi.refresh();
    }
    else {
      handleRunner(gdi, r, cardNo);
    }
    gdi.scrollToBottom();
    gdi.addTimeoutMilli(30*1000, "RequestedStart", SportIdentCB);
  }

  void handleRunner(gdioutput& gdi, pRunner r, int cardNo) {
    bool fail = false;

    if (!selectedClasses.count(r->getClassId(true)) || r->getClassRef(false) == nullptr) {
      gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis) + L" / " + r->getClass(true)).setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, "Klassen tillåter ej val av starttid").setColor(GDICOLOR::colorRed);
      if (r->getStartTime() > 0)
        gdi.addString("", gdiFonts::boldHuge, oe->getAbsTime(r->getStartTime()));
      fail = true;
    }
    else if (r->getStartTime() > 0) {
      gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis)).setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, "Starttiden är redan tilldelad").setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, oe->getAbsTime(r->getStartTime()));

      si->generateStartInfo(gdi, *r, false);
      fail = true;
    }
    else if (r->getCard() || r->getFinishTime() > 0 || r->getStatus() == StatusNotCompetiting) {
      gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis)).setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, "Starttiden är  låst").setColor(GDICOLOR::colorRed);
      fail = true;
    }
    else {
      int now = oe->getRelativeTime(oe->getCurrentTimeS());

      if (now >= lastStart) {
        gdi.addString("", gdiFonts::boldHuge, "Tiden har passerat sista tillåtna starttid").setColor(GDICOLOR::colorRed);
        fail = true;
      }
      else {
        now -= (now % timeConstMinute) - timeConstMinute;

        if (provideSuggestions) {
          makeSuggestions(gdi, r, cardNo, max(firstStart, now + timeToSuggestion));
        }
        else {
          requestStartTime(oe, gdi, r, max(firstStart, now + timeToSuggestion));
        }
      }
    }

    gdi.dropLine(4);
    gdi.refresh();
  }

  bool requestStartTime(oEvent *oeLocal, gdioutput& gdi, pRunner r, int time) {
    if (time == firstStart) {
      int xx = max(2, interval / timeConstMinute);
      time += timeConstMinute * GetRandomNumber(xx); // To avoid using just even minutes in the beginning
    }
    bool fail = false;
    int iter = 0;
    const int maxIter = 3;
    while (++iter <= maxIter) {
      int st = oeLocal->requestStartTime(r->getId(), time, interval, lastStart, maxParallel,
        allowSameCourse, allowSameCourseNeighbour, allowSameFirstControl, allowClubNeighbour);

      if (st <= 0) {
        gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis)).setColor(GDICOLOR::colorRed);
        gdi.addString("", gdiFonts::boldHuge, "Ingen ledig starttid kunde hittas.").setColor(GDICOLOR::colorRed);
        fail = true;
      }
      else {
        pClass cls = r->getClassRef(true);
        r->setStartTime(st, true, oBase::ChangeType::Update);
        r->synchronize();

        // There is no locking. Check that time is OK (at least in class)
        oeLocal->synchronizeList(oListId::oLRunnerId);
        vector<pRunner> rl;
        oeLocal->getRunners(r->getClassId(true), 0, rl, false);
        int nDup = 0;
        for (pRunner rr : rl) {
          if (!rr->isRemoved() && rr->getStatus() != StatusNotCompetiting && rr->getStartTime() > 0 && 
               rr->getStartTime() > st - interval && rr->getStartTime() < st + interval) {
            nDup++;
          }
        }
        if (nDup > 1) {
          r->setStartTime(0, true, oBase::ChangeType::Update);
          r->synchronize();
          int randomMS = 50 + 10 * GetRandomNumber(50);
          Sleep(randomMS); // Wait some random time
          continue; // Try again
        }
        wstring start, cname;
        if (cls) {
          cname = cls->getName();
          start = cls->getStart();
          if (!start.empty())
            cname += L" (" + start + L")";
        }
        gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis));
        gdi.addStringUT(gdiFonts::fontLarge, cname);
        gdi.addString("", gdiFonts::boldHuge, r->getStartTimeS()).setColor(GDICOLOR::colorGreen);

        si->generateStartInfo(gdi, *r, false);
        break;
      }
    }
    if (iter > maxIter) {
      gdi.addStringUT(gdiFonts::fontLarge, r->getCompleteIdentification(oRunner::IDType::OnlyThis)).setColor(GDICOLOR::colorRed);
      gdi.addString("", gdiFonts::boldHuge, "Ingen ledig starttid kunde hittas.").setColor(GDICOLOR::colorRed);
      fail = true;
    }

    return !fail;
  }


  void simulation(gdioutput& gdi) {
    wstring tmp = getTempFile();
    oe->save(tmp, false);
    oEvent tmpOE(gdi);
    tmpOE.open(tmp, true, false, true);

    vector<pRunner> rList;
    tmpOE.getRunners(selectedClasses, rList);
    vector<pRunner> rListToUse;
    for (pRunner r : rList) {
      if (r->getStartTime() == 0 && r->getStatus() != StatusNotCompetiting)
        rListToUse.push_back(r);
    }

    gdi.addString("", fontMediumPlus, "Simulerar starttidstilldelning för X deltagare#" + itos(rListToUse.size()));
    
    gdi.fillRight();
    gdi.addString("", 0, "Första starttid:");
    gdi.fillDown();
    gdi.addStringUT(0, oe->getAbsTime(firstStart));
    gdi.popX();

    gdi.fillRight();
    gdi.addString("", 0, "Sista starttid:");
    gdi.fillDown();
    gdi.addStringUT(0, oe->getAbsTime(lastStart));
    gdi.popX();

    gdi.fillRight();
    gdi.addString("", 0, "Startintervall (minuter):");
    gdi.fillDown();
    gdi.addStringUT(0, formatTimeMS(interval, true));
    gdi.popX();

    if (allowSameCourse)
      gdi.addString("", 0, "Tillåt klass med samma bana på samma starttid");
    if (allowSameCourseNeighbour)
      gdi.addString("", 0, "Tillåt klass med samma bana på stattid före/efter");
    if (allowSameFirstControl)
      gdi.addString("", 0, "Tillåt klass med samma första kontroll vid samma starttid");
    if (allowClubNeighbour)
      gdi.addString("", 0, "Tillåt deltagare med samma klubb och klass på närliggande starttid");

    gdi.fillRight();
    gdi.addString("", 0, L"Max parallellt startande:");
    gdi.fillDown();
    gdi.addStringUT(0, itow(maxParallel));
    gdi.popX();

    map<int, tuple<int, int, int>> classToMaxDelay;
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(rListToUse.begin(), rListToUse.end(), g);

    vector<int> requestTimes(rListToUse.size());
    int first = firstStart - timeToSuggestion;
    int last = lastStart - timeToSuggestion;
    for (int i = 0; i < requestTimes.size(); i++) {
      int t = ((i+1) * (last-first)) / requestTimes.size() + first;
      t = max(firstStart, (t / timeConstMinute) * timeConstMinute);
      requestTimes[i] = t;
    }
    bool fail = false;
    for (int i = 0; i < requestTimes.size(); i++) {
      gdi.addString("", 0, L"X: Startid för Y kl Z#" + itow(i+1) + L"#" + rListToUse[i]->getClass(true) + L"#" + tmpOE.getAbsTime(requestTimes[i]));
      if (!requestStartTime(&tmpOE, gdi, rListToUse[i], requestTimes[i])) {
        gdi.addString("", 0, "FAILED");
        fail = true;
        break;
      }
      int st = rListToUse[i]->getStartTime();
      int after = st - requestTimes[i];
      auto& stat = classToMaxDelay[rListToUse[i]->getClassId(true)];
      ++get<0>(stat);// Count
      get<1>(stat) += after;
      get<2>(stat) = max(get<2>(stat), after);

      if (i % 10 == 9) {
        gdi.scrollToBottom();
        gdi.refreshFast();
      }
    }

    if (!fail) {
      gdi.addString("", 1, "Statistik genomsnitt (max) extra väntetid per klass");

      gdi.dropLine(2);
      for (auto &res : classToMaxDelay) {
        //gdi.fillRight();
        auto& stat = res.second;
        gdi.addStringUT(0, tmpOE.getClass(res.first)->getName()+ L": " + formatTimeMS(get<1>(stat)/get<0>(stat), true, SubSecond::Off) +
          L" (" + formatTimeMS(get<2>(stat), true, SubSecond::Off) + L")");

        //gdi.fillDown();
        //gdi.popX();
      }

      gdi.scrollToBottom();
      gdi.refresh();

      if (gdi.ask(L"Vill du spara en kopia av tävlingen med starttider för ytterligare analys?")) {
        vector< pair<wstring, wstring> > ext;
        ext.push_back(make_pair(L"MeOS-data", L"*.meosxml"));
        int ix = 0;
        wstring fileName = gdi.browseForSave(ext, L"meosxml", ix);
        if (!fileName.empty()) {
          tmpOE.setAnnotation(L"***ANALYSIS***");
          tmpOE.save(fileName, false);
        }
      }
    }

    gdi.addButton("Cancel", "Återgå", SportIdentCB).setCancel();

    gdi.scrollToBottom();
    gdi.refresh();


    /*for (pRunner r : rListToUse) {
      r->setStartTime(0, true, oBase::ChangeType::Update, false);
      r->synchronize();
    }*/

  }

};

void TabSI::showRequestStartTime(gdioutput& gdi) {
  if (!oe->isKiosk())
    checkBoxToolBar(gdi, {CheckBox::PrintStart });

  if (!requestStartTimeHandler) {
    requestStartTimeHandler = make_shared<RequestStart>(this);
  }
 
  shared_ptr<RequestStart> h = dynamic_pointer_cast<RequestStart, GuiHandler>(requestStartTimeHandler);

  h->loadPage(gdi, h);
}

void TabSI::requestStartTime(gdioutput& gdi, const SICard& sic) {
  shared_ptr<RequestStart> h = dynamic_pointer_cast<RequestStart, GuiHandler>(requestStartTimeHandler);
  if (!h)
    throw std::exception("Internal error");

  h->handleCard(gdi, sic.CardNumber);
}
