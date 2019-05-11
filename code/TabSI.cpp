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
#include "meos_util.h"
#include <cassert>
#include "TabRunner.h"
#include "meosexception.h"
#include "MeOSFeatures.h"
#include "RunnerDB.h"
#include "recorder.h"
#include "autocomplete.h"

TabSI::TabSI(oEvent *poe):TabBase(poe), activeSIC(ConvertedTimeStatus::Unknown) {
  editCardData.tabSI = this;
  directEntryGUI.tabSI = this;

  interactiveReadout=poe->getPropertyInt("Interactive", 1)!=0;
  useDatabase = poe->getPropertyInt("Database", 1)!=0;
  printSplits = false;
  printStartInfo = false;
  savedCardUniqueId = 1;  
  
  manualInput = poe->getPropertyInt("ManualInput", 0) == 1;

  mode=ModeReadOut;
  currentAssignIndex=0;

  lastClubId=0;
  lastClassId=0;
  logger = 0;

  minRunnerId = 0;
  inputId = 0;
  printErrorShown = false;
  NC = 8;
}

TabSI::~TabSI(void)
{
  if (logger!=0)
    delete logger;
  logger = 0;
}


static void entryTips(gdioutput &gdi) {
  gdi.fillDown();
  gdi.addString("", 10, "help:21576");
  gdi.dropLine(1);
  gdi.setRestorePoint("EntryLine");
}


void TabSI::logCard(gdioutput &gdi, const SICard &card)
{
  if (logger == 0) {
    logger = new csvparser;
    wstring readlog = L"sireadlog_" + getLocalTimeFileName() + L".csv";
    wchar_t file[260];
    wstring subfolder = makeValidFileName(oe->getName(), true);
    const wchar_t *sf = subfolder.empty() ? 0 : subfolder.c_str();
    getDesktopFile(file, readlog.c_str(), sf);
    logger->openOutput(file);
    vector<string> head = SICard::logHeader();
    logger->outputRow(head);
    logcounter = 0;
  }

  vector<string> log = card.codeLogData(gdi, ++logcounter);
  logger->outputRow(log);
}

extern SportIdent *gSI;
extern pEvent gEvent;

int SportIdentCB(gdioutput *gdi, int type, void *data) {
  TabSI &tsi = dynamic_cast<TabSI &>(*gdi->getTabs().get(TSITab));

  return tsi.siCB(*gdi, type, data);
}

int TabSI::siCB(gdioutput &gdi, int type, void *data)
{
  if (type == GUI_BUTTON) {
    ButtonInfo bi = *(ButtonInfo *)data;

    if (bi.id == "ClearMemory") {
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
        loadPage(gdi);
        gdi.addString("", 1, L"Lyssnar på X.#" + port).setColor(colorDarkGreen);
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
      gdi.refresh();
    }
    else if (bi.id == "StartSI") {
      wchar_t bf[64];
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("ComPort", lbi)) {

        swprintf_s(bf, 64, L"COM%d", lbi.data);
        wstring port = bf;

        if (lbi.text.substr(0, 3) == L"TCP")
          port = L"TCP";

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

          gdi.addStringUT(0, lang.tl(L"Startar SI på ") + port + L"...");
          gdi.refresh();
          if (gSI->openCom(port.c_str())) {
            gSI->startMonitorThread(port.c_str());
            gdi.addStringUT(0, lang.tl(L"SI på ") + port + L": " + lang.tl(L"OK"));
            printSIInfo(gdi, port);

            SI_StationInfo *si = gSI->findStation(port);
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

              SI_StationInfo *si = gSI->findStation(port);
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
              }
            }
          }
          gdi.popX();
          gdi.dropLine();
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
          swprintf_s(bf, 64, L"COM%d", lbi.data);
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

          SI_StationInfo *si = gSI->findStation(bf);
          if (si && !si->extended())
            gdi.addString("", boldText, "warn:notextended").setColor(colorDarkRed);
        }
        else if (gSI->openCom(bf)) {
          gSI->startMonitorThread(bf);
          gdi.addStringUT(0, lang.tl(L"SI på ") + wstring(bf) + L": " + lang.tl(L"OK"));
          printSIInfo(gdi, bf);

          SI_StationInfo *si = gSI->findStation(bf);
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
      if (mode == ModeEntry) {
        printStartInfo = true;
        TabList::splitPrintSettings(*oe, gdi, true, TSITab, TabList::StartInfo);
      }
      else {
        printSplits = true;
        TabList::splitPrintSettings(*oe, gdi, true, TSITab, TabList::Splits);
      }
    }
    else if (bi.id == "AutoTie") {
      gEvent->setProperty("AutoTie", gdi.isChecked("AutoTie"));
    }
    else if (bi.id == "RentCardTie") {
      gEvent->setProperty("RentCard", gdi.isChecked(bi.id));
    }
    else if (bi.id == "TieOK") {
      tieCard(gdi);
    }
    else if (bi.id == "Interactive") {
      interactiveReadout = gdi.isChecked(bi.id);
      gEvent->setProperty("Interactive", interactiveReadout);

      if (mode == ModeAssignCards) {
        gdi.restore("ManualTie", false);
        showAssignCard(gdi, false);
      }
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
            gdi.addItem("ControlType", d);
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
        oe->addFreePunch(punches[k].time, type, punches[k].card, true);
      }
      punches.clear();
      if (origin == 1) {
        TabRunner &tc = dynamic_cast<TabRunner &>(*gdi.getTabs().get(TRunnerTab));
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
        TabRunner &tc = dynamic_cast<TabRunner &>(*gdi.getTabs().get(TRunnerTab));
        tc.showInForestList(gdi);
      }
    }
    else if (bi.id == "Save") {
      SICard sic(ConvertedTimeStatus::Hour24);
      sic.CheckPunch.Code = -1;
      sic.CardNumber = gdi.getTextNo("SI");
      int f = convertAbsoluteTimeHMS(gdi.getText("Finish"), oe->getZeroTimeNum());
      int s = convertAbsoluteTimeHMS(gdi.getText("Start"), oe->getZeroTimeNum());
      if (f < s) {
        f += 24 * 3600;
      }
      sic.FinishPunch.Time = f % (24 * 3600);
      sic.StartPunch.Time = s % (24 * 3600);
      if (!gdi.isChecked("HasFinish")) {
        sic.FinishPunch.Code = -1;
        sic.FinishPunch.Time = 0;
      }

      if (!gdi.isChecked("HasStart")) {
        sic.StartPunch.Code = -1;
        sic.StartPunch.Time = 0;
      }

      double t = 0.1;
      for (sic.nPunch = 0; sic.nPunch<unsigned(NC); sic.nPunch++) {
        sic.Punch[sic.nPunch].Code = gdi.getTextNo("C" + itos(sic.nPunch + 1));
        sic.Punch[sic.nPunch].Time = int(f*t + s*(1.0 - t)) % (24 * 3600);
        t += ((1.0 - t) * (sic.nPunch + 1) / 10.0) * ((rand() % 100) + 400.0) / 500.0;
        if ((sic.nPunch % 11) == 1 || 5 == (sic.nPunch % 8))
          t += min(0.2, 0.9 - t);
      }

      gdi.getRecorder().record("insertCard(" + itos(sic.CardNumber) + ", \"" + sic.serializePunches() + "\"); //Readout card");

      if (false) {
        sic.convertedTime = ConvertedTimeStatus::Hour12;
        sic.StartPunch.Time %= (12 * 3600);
        sic.FinishPunch.Time %= (12 * 3600);
        sic.CheckPunch.Time %= (12 * 3600);
        for (unsigned i = 0; i < sic.nPunch; i++) {
          sic.Punch[i].Time %= (12 * 3600);
        }
      }

      gSI->addCard(sic);
    }
    else if (bi.id == "SaveP") {
      SICard sic(ConvertedTimeStatus::Hour24);
      sic.clear(0);
      sic.FinishPunch.Code = -1;
      sic.CheckPunch.Code = -1;
      sic.StartPunch.Code = -1;

      sic.CardNumber = gdi.getTextNo("SI");
      int f = convertAbsoluteTimeHMS(gdi.getText("Finish"), oe->getZeroTimeNum());
      if (f > 0) {
        sic.FinishPunch.Time = f;
        sic.FinishPunch.Code = 1;
        sic.punchOnly = true;
        gSI->addCard(sic);
        return 0;
      }

      int s = convertAbsoluteTimeHMS(gdi.getText("Start"), oe->getZeroTimeNum());
      if (s > 0) {
        sic.StartPunch.Time = s;
        sic.StartPunch.Code = 1;
        sic.punchOnly = true;
        gSI->addCard(sic);
        return 0;
      }

      sic.Punch[sic.nPunch].Code = gdi.getTextNo("C1");
      sic.Punch[sic.nPunch].Time = convertAbsoluteTimeHMS(gdi.getText("C2"), oe->getZeroTimeNum());
      sic.nPunch = 1;
      sic.punchOnly = true;
      gSI->addCard(sic);
    }
    else if (bi.id == "Cancel") {
      int origin = bi.getExtraInt();
      activeSIC.clear(0);
      punches.clear();
      if (origin == 1) {
        TabRunner &tc = dynamic_cast<TabRunner &>(*gdi.getTabs().get(TRunnerTab));
        tc.showInForestList(gdi);
        return 0;
      }
      loadPage(gdi);

      checkMoreCardsInQueue(gdi);
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

        int year = 0;
        pRunner r = gEvent->addRunner(gdi.getText("Runners"), club,
                                      classes[0]->getId(), activeSIC.CardNumber, year, true);

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
      gEvent->fillClasses(gdi, "Classes", oEvent::extraNone, oEvent::filterNone);
      gdi.setInputFocus("Classes");

      if (classes.size() > 0)
        gdi.selectItemByData("Classes", classes[0]->getId());

      gdi.dropLine();

      gdi.setRestorePoint("restOK2");

      gdi.addButton("Cancel", "Avbryt", SportIdentCB).setCancel();
      if (oe->getNumClasses() > 0)
        gdi.addButton("OK2", "OK", SportIdentCB).setDefault();
      gdi.fillDown();

      gdi.addButton("NewClass", "Skapa ny klass", SportIdentCB);

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

      int year = 0;
      pRunner r = gEvent->addRunner(gdi.getText("Runners"), club,
                                    lbi.data, activeSIC.CardNumber, year, true);

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
      gdi.addButton("Cancel", "Avbryt", SportIdentCB).setCancel();
      gdi.fillDown();
      gdi.addButton("OK3", "OK", SportIdentCB).setDefault();
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
        pclass->setName(gdi.getText("ClassName"));
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
        pclass = gEvent->addClass(gdi.getText("ClassName"), pc ? pc->getId(): 0);
      }
      else if (pc)
        pclass->setCourse(pc);

      int year = 0;
      pRunner r = gEvent->addRunner(gdi.getText("Runners"), gdi.getText("Club", true),
                                    pclass->getId(), activeSIC.CardNumber, year, true);

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
      else r = gEvent->addRunner(lang.tl(L"Oparad bricka"), lang.tl("Okänd"), 0, 0, 0, false);

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
      int year = 0;
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
      
      int cardFee = gdi.isChecked("RentCard") ? oe->getBaseCardFee() : 0;

      di.setInt("CardFee", cardFee);
      di.setInt("Fee", lastFeeNum);
      r->setFlag(oRunner::FlagFeeSpecified, true);

      int totFee = lastFeeNum + (cardFee > 0 ? cardFee : 0);
      writePayMode(gdi, totFee, *r);

      di.setString("Phone", gdi.getText("Phone"));

      r->setFlag(oRunner::FlagTransferSpecified, gdi.hasField("AllStages"));
      r->setFlag(oRunner::FlagTransferNew, gdi.isChecked("AllStages"));

      r->setStartTimeS(gdi.getText("StartTime"));

      wstring bibIn = gdi.getText("Bib");
      wstring bib;
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
        r->setBib(bibIn, 0, false, false);
        bib = L", " + lang.tl(L"Nummerlapp: ") + r->getBib();
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
      if (r->getDI().getInt("CardFee") != 0)
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

      generateStartInfo(gdi, *r);

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
        if (!gdi.ask(L"X har redan ett resultat. Vi du fortsätta?#" + r->getCompleteIdentification()))
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
        for (auto &c : data) {
          for (wstring wc : c) {
            int cn = _wtoi(wc.c_str());
            if (cn > 0) {
              oe->setHiredCard(cn, true);
              gdi.addStringUT(0, itos(cn)).setHandler(getResetHiredCardHandler());
            }
          }
        }
        gdi.scrollToBottom();
        gdi.refresh();
        vector<pRunner> runners;
        oe->getRunners(0, 0, runners);
        if (!runners.empty() && gdi.ask(L"Vill du sätta hyrbricka på befintliga löpare med dessa brickor?")) {
          int bcf = oe->getBaseCardFee();
          for (pRunner r : runners) {
            if (rentCards.count(r->getCardNo()) && r->getDCI().getInt("CardFee") == 0) {
              gdi.addStringUT(0, r->getCompleteIdentification());
              r->getDI().setInt("CardFee", bcf);
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
  else if (type==GUI_LISTBOX) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Runners") {
      pRunner r = gEvent->getRunner(bi.data, 0);
      if (r) {
        gdi.setData("RunnerId", bi.data);
        if (gdi.hasField("Club"))
          gdi.setText("Club", r->getClub());
        gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);
      }
    }
    else if (bi.id == "PayMode") {
      updateEntryInfo(gdi);
    }
    else if (bi.id=="ComPort") {
      wchar_t bf[64];

      if (bi.text.substr(0,3)!=L"TCP")
        swprintf_s(bf, 64, L"COM%d", bi.data);
      else
        wcscpy_s(bf, L"TCP");

      if (gSI->isPortOpen(bf))
        gdi.setText("StartSI", lang.tl("Koppla ifrån"));
      else
        gdi.setText("StartSI", lang.tl("Aktivera"));
    }
    else if (bi.id=="ReadType") {
      gdi.restore("SIPageLoaded");
      mode = SIMode(bi.data);
      gdi.setInputStatus("StartInfo", mode == ModeEntry);
    
      if (mode==ModeAssignCards || mode==ModeEntry) {
        if (mode==ModeAssignCards) {
          gdi.dropLine(1);
          showAssignCard(gdi, true);
        }
        else {
          entryTips(gdi);
          generateEntryLine(gdi, 0);
        }
        gdi.setInputStatus("Interactive", mode == ModeAssignCards);
        gdi.setInputStatus("Database", mode != ModeAssignCards, true);
        gdi.disableInput("PrintSplits");
        
        gdi.disableInput("UseManualInput");
      }
      else if (mode==ModeReadOut) {
        gdi.enableInput("Interactive");
        gdi.enableInput("Database", true);
        gdi.enableInput("PrintSplits");
        gdi.enableInput("UseManualInput");
        gdi.fillDown();
        gdi.addButton("Import", "Importera från fil...", SportIdentCB);

        if (gdi.isChecked("UseManualInput"))
          showManualInput(gdi);
      }
      else if (mode == ModeCardData) {
        showModeCardData(gdi);
      }
      else if (mode == ModeCheckCards) {
        showCheckCardStatus(gdi, "init");
      }
      else if (mode == ModeRegisterCards) {
        showRegisterHiredCards(gdi);
      }
      gdi.refresh();
    }
    else if (bi.id=="Fee") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "NC") {
      NC = bi.data;
      PostMessage(gdi.getHWNDTarget(), WM_USER + 2, TSITab, 0);
    }
  }
  else if (type == GUI_LINK) {
    TextInfo ti = *(TextInfo *)data;
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
        gdi.setText("FindMatch", r->getCompleteIdentification(), true);
        runnerMatchedId = r->getId();
      }
    }
  }
  else if (type == GUI_COMBO) {
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Fee") {
      updateEntryInfo(gdi);
    }
    else if (bi.id == "Runners") {
      DWORD rid;
      if ((gdi.getData("RunnerId", rid) && rid>0) || !gdi.getText("Club", true).empty())
        return 0; // Selected from list

      if (!bi.text.empty() && showDatabase()) {
        pRunner db_r = oe->dbLookUpByName(bi.text, 0, 0, 0);
        if (!db_r && lastClubId)
          db_r = oe->dbLookUpByName(bi.text, lastClubId, 0, 0);

        if (db_r && gdi.hasField("Club")) {
          gdi.setText("Club", db_r->getClub());
        }
      }
      gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);

    }
  }
  else if (type == GUI_COMBOCHANGE) {
    ListBoxInfo bi=*(ListBoxInfo *)data;
    if (bi.id == "Runners") {
      
      if (!showDatabase()) {
        inputId++;
        gdi.addTimeoutMilli(300, "AddRunnerInteractive", SportIdentCB).setExtra(inputId);
      }

      bool show = false;
      if (showDatabase() && bi.text.length() > 1) {
        auto rw = oe->getRunnerDatabase().getRunnerSuggestions(bi.text, 0, 20);
        if (!rw.empty()) {
          auto &ac = gdi.addAutoComplete(bi.id);
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
    EventInfo ev = *(EventInfo *)data;
    if (ev.id == "AutoComplete") {
      pRunner r = oe->getRunner(runnerMatchedId, 0);
      if (r) {
        gdi.clearAutoComplete("");
        gdi.setInputFocus("OK1");
        gdi.setText("Runners", r->getName());
        gdi.setData("RunnerId", runnerMatchedId);
        if (gdi.hasField("Club"))
          gdi.setText("Club", r->getClub());
        inputId = -1;
        gdi.setText("FindMatch", lang.tl("Press Enter to continue"), true);

      }
    }
  }
  else if (type == GUI_FOCUS) {
    InputInfo &ii=*(InputInfo *)data;

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
    TimerInfo &ti = *(TimerInfo *)(data);

    if (ti.id == "TieCard") {
      runnerMatchedId = ti.getExtraInt();
      tieCard(gdi);
      return 0;
    }

    if (inputId != ti.getExtraInt())
      return 0;

    if (ti.id == "RunnerId") {
      const wstring &text = gdi.getText(ti.id);
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
        gdi.setText("FindMatch", r->getCompleteIdentification(), true);
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
      const wstring &text = gdi.getText(ti.id);
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
        gdi.setText("FindMatch", r->getCompleteIdentification(), true);
        runnerMatchedId = r->getId();
      }
      else {
        gdi.setText("FindMatch", L"", true);
        runnerMatchedId = -1;
      }
    }
    else if (ti.id == "AddRunnerInteractive") {
      const wstring &text = gdi.getText("Runners");
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
        gdi.setText("FindMatch", lang.tl(L"X (press Ctrl+Space to confirm)#" + r->getCompleteIdentification()), true);
        runnerMatchedId = r->getId();
      }
      else {
        gdi.setText("FindMatch", L"", true);
        runnerMatchedId = -1;
      }
    }
  }
  else if (type==GUI_INPUTCHANGE) {

    InputInfo ii=*(InputInfo *)data;
    if (ii.id == "RunnerId") {
      inputId++;
      gdi.addTimeoutMilli(300, ii.id, SportIdentCB).setExtra(inputId);
    }
    else if (ii.id == "Manual") {
      inputId++;
      gdi.addTimeoutMilli(300, ii.id, SportIdentCB).setExtra(inputId);
    }
    else if (ii.id == "CardNo" && mode == ModeAssignCards) {
      gdi.setInputStatus("TieOK", runnerMatchedId != -1);
    }
    else if (ii.id == "SI") {
      pRunner r = oe->getRunnerByCardNo(_wtoi(ii.text.c_str()), 0, oEvent::CardLookupProperty::ForReadout);
      if (r && r->getStartTime() > 0) {
        gdi.setText("Start", r->getStartTimeS());
        gdi.check("HasStart", false);
        int f = r->getStartTime() + 2800 + rand()%1200;
        gdi.setText("Finish", oe->getAbsTime(f));
        pCourse pc = r->getCourse(false);
        if (pc) {
          for (int n = 0; n < pc->getNumControls(); n++) {
            if (pc->getControl(n) && n < NC) {
              gdi.setText("C" + itos(n+1), pc->getControl(n)->getFirstNumber());
            }
          }
        }
      }
    }
  }
  else if (type==GUI_INPUT) {
    InputInfo &ii=*(InputInfo *)data;
    if (ii.id == "FinishTime") {
      if (ii.text.empty()) {
        ii.setExtra(1);
        ii.setFgColor(colorGreyBlue);
        gdi.setText(ii.id, lang.tl("Aktuell tid"), true);
      }
    }
    else if (ii.id=="CardNo") {
      int cardNo = gdi.getTextNo("CardNo");

      if (mode == ModeAssignCards) {
        if (runnerMatchedId != -1 && gdi.isChecked("AutoTie") && cardNo>0)
          gdi.addTimeoutMilli(50, "TieCard", SportIdentCB).setExtra(runnerMatchedId);
      }
      else if (cardNo>0) {
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
    else if (ii.id[0]=='*') {
      int si=_wtoi(ii.text.c_str());

      pRunner r=oe->getRunner(ii.getExtraInt(), 0);
      r->synchronize();

      if (r && r->getCardNo() != si) {
        if (si == 0 || !oe->checkCardUsed(gdi, *r, si)) {
          r->setCardNo(si, false);

          r->getDI().setInt("CardFee", oe->getBaseCardFee());
          r->synchronize();
        }

        if (r->getCardNo())
          gdi.setText(ii.id, r->getCardNo());
        else
          gdi.setText(ii.id, L"");
      }
    }
  }
  else if (type==GUI_INFOBOX) {
    DWORD loaded;
    if (!gdi.getData("SIPageLoaded", loaded))
      loadPage(gdi);
  }
  else if (type == GUI_CLEAR) {
    if (mode == ModeEntry) {
      storedInfo.clear();
      storedInfo.storedName = gdi.getText("Name");
      storedInfo.storedCardNo = gdi.getText("CardNo");
      storedInfo.storedClub = gdi.hasField("Club") ? gdi.getText("Club") : L"";
      storedInfo.storedFee = gdi.getText("Fee", true);

      ListBoxInfo lbi;
      gdi.getSelectedItem("Class", lbi);
      storedInfo.storedClassId = lbi.data;
      storedInfo.storedPhone = gdi.getText("Phone");
      storedInfo.storedStartTime = gdi.getText("StartTime");
      
      storedInfo.allStages = gdi.isChecked("AllStages");
      storedInfo.rentState = gdi.isChecked("RentCard");
      storedInfo.hasPaid = gdi.isChecked("Paid");
      storedInfo.payMode = gdi.hasField("PayMode") ? gdi.getSelectedItem("PayMode").first : 0;
    }
    return 1;
  }

  return 0;
}


void TabSI::refillComPorts(gdioutput &gdi)
{
  if (!gSI) return;

  list<int> ports;
  gSI->EnumrateSerialPorts(ports);

  gdi.clearList("ComPort");
  ports.sort();
  wchar_t bf[256];
  int active=0;
  int inactive=0;
  while(!ports.empty())
  {
    int p=ports.front();
    swprintf_s(bf, 256, L"COM%d", p);

    if (gSI->isPortOpen(bf)){
      gdi.addItem("ComPort", wstring(bf)+L" [OK]", p);
      active=p;
    }
    else{
      gdi.addItem("ComPort", bf, p);
      inactive=p;
    }

    ports.pop_front();
  }

  if (gSI->isPortOpen(L"TCP"))
    gdi.addItem("ComPort", L"TCP [OK]");
  else
    gdi.addItem("ComPort", L"TCP");

  if (active){
    gdi.selectItemByData("ComPort", active);
    gdi.setText("StartSI", lang.tl("Koppla ifrån"));
  }
  else{
    gdi.selectItemByData("ComPort", inactive);
    gdi.setText("StartSI", lang.tl("Aktivera"));
  }
}

void TabSI::showReadPunches(gdioutput &gdi, vector<PunchInfo> &punches, set<string> &dates)
{
  char bf[64];
  int yp = gdi.getCY();
  int xp = gdi.getCX();
  dates.clear();
  for (size_t k=0;k<punches.size(); k++) {
    sprintf_s(bf, "%d.", k+1);
    gdi.addStringUT(yp, xp, 0, bf);

    pRunner r = oe->getRunnerByCardNo(punches[k].card, punches[k].time, oEvent::CardLookupProperty::Any);
    sprintf_s(bf, "%d", punches[k].card);
    gdi.addStringUT(yp, xp+40, 0, bf, 240);

    if (r!=0)
      gdi.addStringUT(yp, xp+100, 0, r->getName(), 170);

    if (punches[k].date[0] != 0) {
      gdi.addStringUT(yp, xp+280, 0, punches[k].date, 75);
      dates.insert(punches[k].date);
    }
    if (punches[k].time>0)
      gdi.addStringUT(yp, xp+360, 0, oe->getAbsTime(punches[k].time));
    else
      gdi.addStringUT(yp, xp+360, 0, makeDash(L"-"));

    yp += gdi.getLineHeight();
  }
}

void TabSI::showReadCards(gdioutput &gdi, vector<SICard> &cards)
{
  char bf[64];
  int yp = gdi.getCY();
  int xp = gdi.getCX();
  for (size_t k=0;k<cards.size(); k++) {
    sprintf_s(bf, "%d.", k+1);
    gdi.addStringUT(yp, xp, 0, bf);

    pRunner r = oe->getRunnerByCardNo(cards[k].CardNumber, 0, oEvent::CardLookupProperty::Any);
    sprintf_s(bf, "%d", cards[k].CardNumber);
    gdi.addStringUT(yp, xp+40, 0, bf, 240);

    if (r!=0)
      gdi.addStringUT(yp, xp+100, 0, r->getName(), 240);

    gdi.addStringUT(yp, xp+300, 0, oe->getAbsTime(cards[k].FinishPunch.Time));
    yp += gdi.getLineHeight();
  }
}

SportIdent &TabSI::getSI(const gdioutput &gdi) {
  if (!gSI) {
    HWND hWnd=gdi.getHWNDMain();
    gSI = new SportIdent(hWnd, 0);
  }
  return *gSI;
}

bool TabSI::loadPage(gdioutput &gdi) {
  gdi.clearPage(true);
  printErrorShown = false;
  gdi.pushX();
  gdi.selectTab(tabId);
  oe->checkDB();
  gdi.setData("SIPageLoaded", 1);

  if (!gSI) {
    getSI(gdi);
    if (oe->isClient())
      interactiveReadout = false;
  }
#ifdef _DEBUG
  gdi.fillRight();
  gdi.pushX();
  gdi.addInput("SI", L"", 10, SportIdentCB, L"SI");
  int s = 3600+(rand()%60)*60;
  int f = s + 1800 + rand()%900;
  
  gdi.setCX(gdi.getCX()+gdi.getLineHeight());
  
  gdi.dropLine(1.4);
  gdi.addCheckbox("HasStart", "");
  gdi.dropLine(-1.4);
  gdi.setCX(gdi.getCX()-gdi.getLineHeight());
  gdi.addInput("Start", oe->getAbsTime(s), 6, 0, L"Start");
  
  gdi.dropLine(1.4);
  gdi.addCheckbox("HasFinish", "");
  gdi.dropLine(-1.4);
  gdi.setCX(gdi.getCX()-gdi.getLineHeight());

  gdi.addInput("Finish", oe->getAbsTime(f), 6, 0, L"Mål");
  gdi.addSelection("NC", 45, 200, SportIdentCB, L"NC");
  const int src[11] = {33, 34, 45, 50, 36, 38, 59, 61, 62, 67, 100};
  
  for (int i = 0; i < 32; i++)
    gdi.addItem("NC", itow(i), i);

  gdi.selectItemByData("NC", NC);

  for (int i = 0; i < NC; i++) {
    int level = min(i, NC-i)/5;
    int c;
    if (i < NC /2) {
      int ix = i%6;
      c = src[ix] + level * 10;
      if (c == 100)
        c = 183;
    }
    else {
      int ix = 10-(NC-i-1)%5;
      c = src[ix] + level * 10;
    }

    gdi.addInput("C" + itos(i+1), itow(c), 3, 0, L"#C" + itow(i+1));
  }
  
  gdi.dropLine();
  gdi.addButton("Save", "Bricka", SportIdentCB);
  gdi.fillDown();

  gdi.addButton("SaveP", "Stämpling", SportIdentCB);
  gdi.popX();
#endif
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

  int xb = gdi.getCX();
  int yb = gdi.getCY();

  gdi.fillRight();
  if (!oe->empty()) {
    gdi.setCX(xb + gdi.scaleLength(10));
    gdi.setCY(yb + gdi.scaleLength(10));
    gdi.addString("", fontMediumPlus, "Funktion:");
    gdi.addSelection("ReadType", 200, 200, SportIdentCB);
    gdi.addItem("ReadType", lang.tl("Avläsning/radiotider"), ModeReadOut);
    gdi.addItem("ReadType", lang.tl("Tilldela hyrbrickor"), ModeAssignCards);
    gdi.addItem("ReadType", lang.tl("Avstämning hyrbrickor"), ModeCheckCards);
    gdi.addItem("ReadType", lang.tl("Registrera hyrbrickor"), ModeRegisterCards);
    gdi.addItem("ReadType", lang.tl("Anmälningsläge"), ModeEntry);
    gdi.addItem("ReadType", lang.tl("Print card data"), ModeCardData);

    gdi.selectItemByData("ReadType", mode);
    gdi.dropLine(2.5);
    gdi.setCX(xb + gdi.scaleLength(10));
  }
  else {
    mode = ModeCardData;
  }

  if (!oe->empty())
    gdi.addCheckbox("Interactive", "Interaktiv inläsning", SportIdentCB, interactiveReadout);

  if (oe->empty() || oe->useRunnerDb())
    gdi.addCheckbox("Database", "Använd löpardatabasen", SportIdentCB, useDatabase);

  gdi.addCheckbox("PrintSplits", "Sträcktidsutskrift[check]", SportIdentCB, printSplits);
  
  if (!oe->empty()) {
    gdi.addCheckbox("StartInfo", "Startbevis", SportIdentCB, printStartInfo, "Skriv ut startbevis för deltagaren");
    if (mode != ModeEntry)
      gdi.disableInput("StartInfo");
  }
  if (!oe->empty())
    gdi.addCheckbox("UseManualInput", "Manuell inmatning", SportIdentCB, manualInput);

  gdi.fillDown();

  if (!oe->empty()) {
    RECT rc = {xb, yb, gdi.getWidth(), gdi.getHeight()};
    gdi.addRectangle(rc, colorLightBlue);
  }
  gdi.popX();
  gdi.dropLine(2);
  gdi.setRestorePoint("SIPageLoaded");

  if (mode == ModeReadOut) {
    gdi.addButton("Import", "Importera från fil...", SportIdentCB);

    gdi.setRestorePoint("Help");
    gdi.addString("", 10, "help:471101");

    if (gdi.isChecked("UseManualInput"))
      showManualInput(gdi);

    gdi.dropLine();
  }
  else if (mode == ModeAssignCards) {
    gdi.dropLine(1);
    showAssignCard(gdi, true);
  }
  else if (mode == ModeEntry) {
    entryTips(gdi);
    generateEntryLine(gdi, 0);
    gdi.disableInput("Interactive");
    gdi.disableInput("PrintSplits");
    gdi.disableInput("UseManualInput");
  }
  else if (mode == ModeCardData) {
    showModeCardData(gdi);
  }
  else if (mode == ModeCheckCards) {
    showCheckCardStatus(gdi, "init");
  }
  else if (mode == ModeRegisterCards) {
    showRegisterHiredCards(gdi);
  }

  // Unconditional clear
  activeSIC.clear(0);

  checkMoreCardsInQueue(gdi);
  gdi.refresh();
  return true;
}

void InsertSICard(gdioutput &gdi, SICard &sic)
{
  TabSI &tsi = dynamic_cast<TabSI &>(*gdi.getTabs().get(TSITab));
  tsi.insertSICard(gdi, sic);
}

pRunner TabSI::autoMatch(const SICard &sic, pRunner db_r)
{
  assert(useDatabase);
  //Look up in database.
  if (!db_r)
    db_r = gEvent->dbLookUpByCard(sic.CardNumber);

  pRunner r=0;

  if (db_r) {
    r = gEvent->getRunnerByName(db_r->getName(), db_r->getClub());

    if ( !r ) {
      vector<pClass> classes;
      int dist = gEvent->findBestClass(sic, classes);

      if (classes.size()==1 && dist>=-1 && dist<=1) { //Almost perfect match found. Assume it is it!
        r = gEvent->addRunnerFromDB(db_r, classes[0]->getId(), true);
        r->setCardNo(sic.CardNumber, false);
      }
      else r=0; //Do not assume too much...
    }
  }
  if (r && r->getCard()==0)
    return r;
  else return 0;
}

void TabSI::insertSICard(gdioutput &gdi, SICard &sic)
{
  wstring msg;
  try {
    insertSICardAux(gdi, sic);
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

  if (!msg.empty())
    gdi.alert(msg);
}

void TabSI::insertSICardAux(gdioutput &gdi, SICard &sic)
{
  if (oe->isReadOnly()) {
    gdi.makeEvent("ReadCard", "insertSICard", sic.CardNumber, 0, true);
    return;
  }

  DWORD loaded;
  bool pageLoaded=gdi.getData("SIPageLoaded", loaded);

  if (pageLoaded && manualInput && mode == ModeReadOut)
    gdi.restore("ManualInput");

  if (!pageLoaded && !insertCardNumberField.empty()) {
    if (gdi.insertText(insertCardNumberField, itow(sic.CardNumber)))
      return;
  }

  if (mode==ModeAssignCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö");
    }
    else assignCard(gdi, sic);
    return;
  }
  else if (mode==ModeEntry) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö");
    }
    else entryCard(gdi, sic);
    return;
  }
  if (mode==ModeCheckCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö");
    }
    else 
      checkCard(gdi, sic, true);
    return;
  }
  else if (mode == ModeRegisterCards) {
    if (!pageLoaded) {
      CardQueue.push_back(sic);
      gdi.addInfoBox("SIREAD", L"Inläst bricka ställd i kö");
    }
    else {
      registerHiredCard(gdi, sic);
    }
    return;
  }
  else if (mode == ModeCardData) {
    if (sic.convertedTime == ConvertedTimeStatus::Hour12) {
      int locTime = getLocalAbsTime();
      int st = -1;
      if (sic.StartPunch.Code != -1)
        st = sic.StartPunch.Time;
      else if (sic.nPunch > 0 && sic.Punch[0].Code != -1)
        st = sic.Punch[0].Time;
      
      if (st == -1)
        st = (locTime + 3600 * 20) % (12 * 3600);
      else {
        // We got a start time. Calculate running time
        if (sic.FinishPunch.Code != -1) {
          int rt = (sic.FinishPunch.Time - st + 12 * 3600) % (12 * 3600);
          
          // Adjust to local time at start;
          locTime = (locTime - rt + (24 * 3600)) % (24 * 3600);
        }
      }
      int zt1 = (st + 23 * 3600) % (24 * 3600);
      int zt2 = st + 11 * 3600;
      int d1 = min(abs(locTime - zt1), abs(locTime - zt1 + 3600 * 24));
      int d2 = min(abs(locTime - zt2), abs(locTime - zt2 + 3600 * 24));

      if (d1 < d2)
        sic.analyseHour12Time(zt1);
      else
        sic.analyseHour12Time(zt2);
    }
    savedCards.push_back(make_pair(savedCardUniqueId++, sic));
    
    if (printSplits) {
      generateSplits(savedCards.back().first, gdi);
    }
    if (savedCards.size() > 1 && pageLoaded) {
      RECT rc = {30, gdi.getCY(), gdi.scaleLength(250), gdi.getCY() + 3};
      gdi.addRectangle(rc);
    }

    if (pageLoaded) {
      gdi.enableInput("CreateCompetition", true);
      printCard(gdi, savedCards.back().first, false);
      gdi.dropLine();
      gdi.refreshFast();
      gdi.scrollToBottom();
    }
    return;
  }
  gEvent->synchronizeList({ oListId::oLCardId, oListId::oLRunnerId });

  if (sic.punchOnly) {
    processPunchOnly(gdi, sic);
    return;
  }
  pRunner r;
  if (sic.runnerId == 0)
    r = gEvent->getRunnerByCardNo(sic.CardNumber, 0, oEvent::CardLookupProperty::ForReadout);
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
      r=autoMatch(sic, 0);

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
        gdi.addInfoBox("SIREAD", L"info:readout_action#" + gEvent->getCurrentTimeS()+L"#"+itow(sic.CardNumber), 0, SportIdentCB);
        return;
      }
    }
    else {
      if (!readBefore) {
        if (r && r->getClassId(false) && !sameCardNewRace)
          processCard(gdi, r, sic, true);
        else
          processUnmatched(gdi, sic, true);
      }
      else
        gdi.addInfoBox("SIREAD", L"Brickan redan inläst.", 0, SportIdentCB);
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
    gdi.addInfoBox("SIREAD", L"info:readout_queue#" + gEvent->getCurrentTimeS() + L"#" + name);
    return;
  }

  if (readBefore) {
    //We stop processing of new cards, while working...
    // Thus cannot be in interactive mode
    activeSIC=sic;
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

pRunner TabSI::getRunnerForCardSplitPrint(const SICard &sic) const {
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

void TabSI::startInteractive(gdioutput &gdi, const SICard &sic, pRunner r, pRunner db_r)
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

    if (db_r){
      gdi.setText("Runners", db_r->getName()); //Data from DB
    }
    else if (sic.firstName[0] || sic.lastName[0]){ //Data from SI-card
      gdi.setText("Runners", wstring(sic.lastName) + L", "  + sic.firstName);
    }
    if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
      gdi.addCombo("Club", 200, 300, 0, L"Klubb:").setHandler(&directEntryGUI);
      gEvent->fillClubs(gdi, "Club");

      if (db_r)
        gdi.setText("Club", db_r->getClub()); //Data from DB
    }
    if (gdi.getText("Runners").empty() || !gdi.hasField("Club"))
      gdi.setInputFocus("Runners");
    else
      gdi.setInputFocus("Club");

    //Process this card.
    activeSIC=sic;
    gdi.dropLine();
    gdi.setRestorePoint("restOK1");
    gdi.addButton("OK1", "OK", SportIdentCB).setDefault();
    gdi.fillDown();
    gdi.addButton("Cancel", "Avbryt", SportIdentCB).setCancel();
    gdi.popX();
    gdi.addString("FindMatch", 0, "").setColor(colorGreen);
    gdi.registerEvent("AutoComplete", SportIdentCB).setKeyCommand(KC_AUTOCOMPLETE);
    gdi.dropLine();
    gdi.scrollToBottom();
    gdi.refresh();
  }
  else {
    //Process this card.
    activeSIC=sic;

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
    gEvent->fillClasses(gdi, "Classes", oEvent::extraNone, oEvent::filterNone);
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
    gdi.refresh();
  }
}

// Insert card without converting times and with/without runner
void TabSI::processInsertCard(const SICard &sic)
{
  if (oe->isCardRead(sic))
    return;

  pRunner runner = oe->getRunnerByCardNo(sic.CardNumber, 0, oEvent::CardLookupProperty::ForReadout);
  pCard card = oe->allocateCard(runner);
  card->setReadId(sic);
  card->setCardNo(sic.CardNumber);

  if (sic.CheckPunch.Code!=-1)
    card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time, 0);

  if (sic.StartPunch.Code!=-1)
    card->addPunch(oPunch::PunchStart, sic.StartPunch.Time, 0);

  for(unsigned i=0;i<sic.nPunch;i++)
    card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time, 0);

  if (sic.FinishPunch.Code!=-1)
    card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time,0 );

  //Update to SQL-source
  card->synchronize();

  if (runner) {
    vector<int> mp;
    runner->addPunches(card, mp);
  }
}

bool TabSI::processUnmatched(gdioutput &gdi, const SICard &csic, bool silent)
{
  SICard sic(csic);
  pCard card=gEvent->allocateCard(0);

  card->setReadId(csic);
  card->setCardNo(csic.CardNumber);

  
  wstring info=lang.tl(L"Okänd bricka ") + itow(sic.CardNumber) + L".";
  wstring warnings;

  // Write read card to log
  logCard(gdi, sic);

  // Convert punch times to relative times.
  gEvent->convertTimes(nullptr, sic);

  if (sic.CheckPunch.Code!=-1)
    card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time, 0);

  if (sic.StartPunch.Code!=-1)
    card->addPunch(oPunch::PunchStart, sic.StartPunch.Time, 0);

  for(unsigned i=0;i<sic.nPunch;i++)
    card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time, 0);

  if (sic.FinishPunch.Code!=-1)
    card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time, 0);
  else
    warnings+=lang.tl("Målstämpling saknas.");

  //Update to SQL-source
  card->synchronize();

  RECT rc;
  rc.left=15;
  rc.right=gdi.getWidth()-10;
  rc.top=gdi.getCY()+gdi.getLineHeight()-5;
  rc.bottom=rc.top+gdi.getLineHeight()*2+14;

  if (!silent) {
    gdi.fillDown();
    //gdi.dropLine();
    gdi.addRectangle(rc, colorLightRed, true);
    gdi.addStringUT(rc.top+6, rc.left+20, 1, info);
    //gdi.dropLine();
    if (gdi.isChecked("UseManualInput"))
      showManualInput(gdi);

    gdi.scrollToBottom();
  }
  else {
    gdi.addInfoBox("SIINFO", L"#" + info, 10000);
  }
  gdi.makeEvent("DataUpdate", "sireadout", 0, 0, true);

  checkMoreCardsInQueue(gdi);
  return true;
}

void TabSI::rentCardInfo(gdioutput &gdi, int width)
{
  RECT rc;
  rc.left=15;
  rc.right=rc.left+width;
  rc.top=gdi.getCY()-7;
  rc.bottom=rc.top+gdi.getLineHeight()+5;

  gdi.addRectangle(rc, colorYellow, true);
  gdi.addString("", rc.top+2, rc.left+width/2, 1|textCenter, "Vänligen återlämna hyrbrickan.");
}

bool TabSI::processCard(gdioutput &gdi, pRunner runner, const SICard &csic, bool silent)
{
  if (!runner)
    return false;
  if (runner->getClubId())
    lastClubId = runner->getClubId();

  runner = runner->getMatchedRunner(csic);

  int lh=gdi.getLineHeight();
  //Update from SQL-source
  runner->synchronize();

  if (!runner->getClassId(false))
    runner->setClassId(gEvent->addClass(lang.tl(L"Okänd klass"))->getId(), true);

  // Choose course from pool
  pClass cls = runner->getClassRef(false);
  if (cls && cls->hasCoursePool()) {
    unsigned leg=runner->legToRun();

    if (leg<cls->getNumStages()) {
      pCourse c = cls->selectCourseFromPool(leg, csic);
      if (c)
        runner->setCourseId(c->getId());
    }
  }

  if (cls && cls->hasUnorderedLegs()) {
    pCourse crs = cls->selectParallelCourse(*runner, csic);
    if (crs) {
      runner->setCourseId(crs->getId());
      runner->synchronize(true);
    }
  }

  pClass pclass = runner->getClassRef(true);
  if (!runner->getCourse(false) && !csic.isManualInput() && !oe->getMeOSFeatures().hasFeature(MeOSFeatures::NoCourses)) {

    if (pclass && !pclass->hasMultiCourse() && !pclass->hasDirectResult()) {
      pCourse pcourse=gEvent->addCourse(pclass->getName());
      pclass->setCourse(pcourse);

      for(unsigned i=0;i<csic.nPunch; i++)
        pcourse->addControl(csic.Punch[i].Code);

      wchar_t msg[256];

      swprintf_s(msg, lang.tl(L"Skapade en bana för klassen %s med %d kontroller från brickdata (SI-%d)").c_str(),
                              pclass->getName().c_str(), csic.nPunch, csic.CardNumber);

      if (silent)
        gdi.addInfoBox("SIINFO", wstring(L"#") + msg, 15000);
      else
        gdi.addStringUT(0, msg);
    }
    else {
      if (!(pclass && pclass->hasDirectResult())) {
        const wchar_t *msg=L"Löpare saknar klass eller bana";

        if (silent)
          gdi.addInfoBox("SIINFO", msg, 15000);
        else
          gdi.addString("", 0, msg);
      }
    }
  }

  pCourse pcourse=runner->getCourse(false);

  if (pcourse)
    pcourse->synchronize();
  else if (pclass && pclass->hasDirectResult())
    runner->setStatus(StatusOK, true, false, false);
  //silent=true;
  SICard sic(csic);
  wstring info, warnings, cardno;
  vector<int> MP;

  if (!csic.isManualInput()) {
    pCard card=gEvent->allocateCard(runner);

    card->setReadId(csic);
    card->setCardNo(sic.CardNumber);

    cardno = itow(sic.CardNumber);

    info = runner->getName() + L" (" + cardno + L"),   ";
    if (!runner->getClub().empty())
      info += runner->getClub() + +L",   ";
    info += runner->getClass(true);

    // Write read card to log
    logCard(gdi, sic);

    // Convert punch times to relative times.
    oe->convertTimes(runner, sic);
    pCourse prelCourse = runner->getCourse(false);
    const int finishPT = prelCourse ? prelCourse->getFinishPunchType() : oPunch::PunchFinish;
    bool hasFinish = false;

    if (sic.CheckPunch.Code!=-1)
      card->addPunch(oPunch::PunchCheck, sic.CheckPunch.Time,0);

    if (sic.StartPunch.Code!=-1)
      card->addPunch(oPunch::PunchStart, sic.StartPunch.Time,0);

    for(unsigned i=0;i<sic.nPunch;i++) {
      if (sic.Punch[i].Code == finishPT)
        hasFinish = true;
      card->addPunch(sic.Punch[i].Code, sic.Punch[i].Time,0);
    }
    if (sic.FinishPunch.Code!=-1) {
      card->addPunch(oPunch::PunchFinish, sic.FinishPunch.Time,0);
      if (finishPT == oPunch::PunchFinish)
        hasFinish = true;
    }

    if (!hasFinish)
      warnings+=lang.tl(L"Målstämpling saknas.");

    card->synchronize();
    runner->addPunches(card, MP);
    runner->hasManuallyUpdatedTimeStatus();
  }
  else {
    //Manual input
    info = runner->getName() + L",   " + runner->getClub() + L",   " + runner->getClass(true);
    runner->setCard(0);

    if (csic.statusOK) {
      runner->setStatus(StatusOK, true, false);
      runner->setFinishTime(csic.relativeFinishTime);
    }
    else if (csic.statusDNF) {
      runner->setStatus(StatusDNF, true, false);
      runner->setFinishTime(0);
    }
    else {
      runner->setStatus(StatusMP, true, false);
      runner->setFinishTime(csic.relativeFinishTime);
    }

    cardno = makeDash(L"-");
    runner->evaluateCard(true, MP, false, false);
    runner->hasManuallyUpdatedTimeStatus();
  }

  //Update to SQL-source
  runner->synchronize();

  RECT rc;
  rc.left=15;
  rc.right=gdi.getWidth()-10;
  rc.top=gdi.getCY()+gdi.getLineHeight()-5;
  rc.bottom=rc.top+gdi.getLineHeight()*2+14;

  if (!warnings.empty())
    rc.bottom+=gdi.getLineHeight();

  if (runner->getStatus()==StatusOK) {
    set<int> clsSet;
    if (runner->getClassId(false))
      clsSet.insert(runner->getClassId(true));
    gEvent->calculateResults(clsSet, oEvent::ResultType::ClassResult);
    if (runner->getTeam())
      gEvent->calculateTeamResults(runner->getLegNumber(), false);
    bool qfClass = runner->getClassId(false) != runner->getClassId(true);
    wstring placeS = (runner->getTeam() && !qfClass) ? 
                   runner->getTeam()->getLegPlaceS(runner->getLegNumber(), false) :
                   runner->getPlaceS();

    if (!silent) {
      gdi.fillDown();
      //gdi.dropLine();
      gdi.addRectangle(rc, colorLightGreen, true);

      gdi.addStringUT(rc.top+6, rc.left+20, 1, info);
      if (!warnings.empty())
        gdi.addStringUT(rc.top+6+2*lh, rc.left+20, 0, warnings);

      wstring statusline = lang.tl(L"Status OK,    ") +
                           lang.tl(L"Tid: ") + runner->getRunningTimeS() +
                           lang.tl(L",      Prel. placering: ") + placeS;


      statusline += lang.tl(L",     Prel. bomtid: ") + runner->getMissedTimeS();
      gdi.addStringUT(rc.top+6+lh, rc.left+20, 0, statusline);

      if (runner->isHiredCard())
        rentCardInfo(gdi, rc.right-rc.left);
      gdi.scrollToBottom();
    }
    else {
      wstring msg = L"#" + runner->getName()  + L" (" + cardno + L")\n"+
          runner->getClub() + L". " + runner->getClass(true) +
          L"\n" + lang.tl("Tid:  ") + runner->getRunningTimeS() + lang.tl(L", Plats  ") + placeS;

      gdi.addInfoBox("SIINFO", msg, 10000);
    }
  }
  else {
    wstring msg=lang.tl(L"Status: ") + runner->getStatusS(true);

    if (!MP.empty()) {
      msg=msg + L", (";
      vector<int>::iterator it;
      
      for(it=MP.begin(); it!=MP.end(); ++it) {
        msg = msg + itow(*it)+ L" ";
      }
      msg += lang.tl(L" saknas") + L".)";
    }

    if (!silent) {
      gdi.fillDown();
      gdi.dropLine();
      gdi.addRectangle(rc, colorLightRed, true);

      gdi.addStringUT(rc.top+6, rc.left+20, 1, info);
      if (!warnings.empty())
        gdi.addStringUT(rc.top+6+lh*2, rc.left+20, 1, warnings);

      gdi.addStringUT(rc.top+6+lh, rc.left+20, 0, msg);

      if (runner->isHiredCard())
        rentCardInfo(gdi, rc.right-rc.left);

      gdi.scrollToBottom();
    }
    else {
      wstring statusmsg = L"#" + runner->getName()  + L" (" + cardno + L")\n"+
          runner->getClub() + L". "+ runner->getClass(true) +
          L"\n" + msg;

      gdi.addInfoBox("SIINFO", statusmsg, 10000);
    }
  }

  tabForceSync(gdi, gEvent);
  gdi.makeEvent("DataUpdate", "sireadout", runner ? runner->getId() : 0, 0, true);

  // Print splits
  if (printSplits)
    generateSplits(runner, gdi);

  activeSIC.clear(&csic);

  checkMoreCardsInQueue(gdi);
  return true;
}

void TabSI::processPunchOnly(gdioutput &gdi, const SICard &csic)
{
  SICard sic=csic;
  DWORD loaded;
  gEvent->convertTimes(nullptr, sic);
  oFreePunch *ofp=0;

  if (sic.nPunch==1)
    ofp=gEvent->addFreePunch(sic.Punch[0].Time, sic.Punch[0].Code, sic.CardNumber, true);
  else if (sic.FinishPunch.Time > 0)
    ofp=gEvent->addFreePunch(sic.FinishPunch.Time, oPunch::PunchFinish, sic.CardNumber, true);
  else if (sic.StartPunch.Time > 0)
    ofp=gEvent->addFreePunch(sic.StartPunch.Time, oPunch::PunchStart, sic.CardNumber, true);
  else
    ofp=gEvent->addFreePunch(sic.CheckPunch.Time, oPunch::PunchCheck, sic.CardNumber, true);

  if (ofp) {
    pRunner r = ofp->getTiedRunner();
    if (gdi.getData("SIPageLoaded", loaded)){
      //gEvent->getRunnerByCard(sic.CardNumber);

      if (r) {
        wstring str = r->getName() + lang.tl(" stämplade vid ") + ofp->getSimpleString();
        gdi.addStringUT(0, str);
        gdi.dropLine();
      }
      else {
        wstring str=L"SI " + itow(sic.CardNumber) + lang.tl(" (okänd) stämplade vid ") + ofp->getSimpleString();
        gdi.addStringUT(0, str);
        gdi.dropLine(0.3);
      }
      gdi.scrollToBottom();
    }

    tabForceSync(gdi, gEvent);
    gdi.makeEvent("DataUpdate", "sireadout", r ? r->getId() : 0, 0, true);

  }

  checkMoreCardsInQueue(gdi);
  return;
}


void TabSI::entryCard(gdioutput &gdi, const SICard &sic)
{
  gdi.setText("CardNo", sic.CardNumber);

  if (oe->hasHiredCardData())
    gdi.check("RentCard", oe->isHiredCard(sic.CardNumber));

  wstring name;
  wstring club;
  int age = 0;
  if (showDatabase()) {
    pRunner db_r=oe->dbLookUpByCard(sic.CardNumber);

    if (db_r) {
      name=db_r->getNameRaw();
      club=db_r->getClub();
      age = db_r->getBirthAge();
    }
  }

  //Else get name from card
  if (name.empty() && (sic.firstName[0] || sic.lastName[0]))
    name=wstring(sic.lastName) + L", " + wstring(sic.firstName);

  gdi.setText("Name", name);
  if (gdi.hasField("Club") && !club.empty())
    gdi.setText("Club", club);

  if (club.empty() && gdi.hasField("Club"))
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

void TabSI::assignCard(gdioutput &gdi, const SICard &sic)
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
  BaseInfo *ii=gdi.getInputFocus();
  wstring sicode = itow(sic.CardNumber);

  if (ii && ii->id[0]=='*') {
    currentAssignIndex=atoi(ii->id.c_str()+1);
  }
  else { //If not correct focus, use internal counter
    char id[32];
    sprintf_s(id, "*%d", currentAssignIndex++);

    ii=gdi.setInputFocus(id);

    if (!ii) {
      currentAssignIndex=0;
      sprintf_s(id, "*%d", currentAssignIndex++);
      ii=gdi.setInputFocus(id);
    }
  }

  if (ii && ii->getExtraInt()) {
    pRunner r=oe->getRunner(ii->getExtraInt(), 0);
    if (r) {
      if (oe->checkCardUsed(gdi, *r, sic.CardNumber)) {
        currentAssignIndex = storedAssigneIndex;
        return;
      }
      if (r->getCardNo() == 0 ||
          gdi.ask(L"Skriv över existerande bricknummer?")) {

        r->setCardNo(sic.CardNumber, false);
        r->getDI().setInt("CardFee", oe->getBaseCardFee());
        r->synchronize();
        gdi.setText(ii->id, sicode);
      }
    }
    gdi.TabFocus();
  }

  checkMoreCardsInQueue(gdi);
}

void TabSI::generateEntryLine(gdioutput &gdi, pRunner r) {
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
    gdi.addItem("Class", d);
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

    gdi.dropLine(1.2);
    generatePayModeWidget(gdi);
    gdi.dropLine(-1.2);
  }

  gdi.popX();
  gdi.dropLine(3.1);

  gdi.addString("",0, "Starttid:");
  gdi.dropLine(-0.2);
  gdi.addInput("StartTime", storedInfo.storedStartTime, 5, 0, L"");

  gdi.setCX(gdi.getCX() + gdi.scaleLength(20));
  gdi.dropLine(0.2);

  gdi.addString("", 0, "Nummerlapp:");
  gdi.dropLine(-0.2);
  gdi.addInput("Bib", L"", 5, 0, L"");

  gdi.setCX(gdi.getCX()+gdi.scaleLength(20));
  gdi.dropLine(0.2);
  
  gdi.addString("", 0, "Telefon:");
  gdi.dropLine(-0.2);
  gdi.addInput("Phone", storedInfo.storedPhone, 12, 0, L"");
  gdi.dropLine(0.2);

  gdi.setCX(gdi.getCX()+gdi.scaleLength(20));

  gdi.addCheckbox("RentCard", "Hyrbricka", SportIdentCB, storedInfo.rentState);
  if (oe->hasNextStage())
    gdi.addCheckbox("AllStages", "Anmäl till efterföljande etapper", SportIdentCB, storedInfo.allStages);
      
  if (r!=0) {
    if (r->getCardNo()>0)
      gdi.setText("CardNo", r->getCardNo());

    gdi.setText("Name", r->getNameRaw());
    if (gdi.hasField("Club")) {
      gdi.selectItemByData("Club", r->getClubId());
    }
    gdi.selectItemByData("Class", r->getClassId(true));

    oDataConstInterface dci = r->getDCI();
    if (gdi.hasField("Fee"))
      gdi.setText("Fee", oe->formatCurrency(dci.getInt("Fee")));

    gdi.setText("Phone", dci.getString("Phone"));
    gdi.setText("Bib", r->getBib());

    gdi.check("RentCard", dci.getInt("CardFee") != 0);
    if (gdi.hasField("Paid"))
      gdi.check("Paid", dci.getInt("Paid")>0);
    else if (gdi.hasField("PayMode")) {
      int paidId = dci.getInt("Paid") > 0 ? r->getPaymentMode() : 1000;
      gdi.selectItemByData("PayMode", paidId);
    }

    if (gdi.hasField("AllStages")) {
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
  
  RECT rc = {xb, yb, gdi.getWidth(), gdi.getHeight()};
  gdi.addRectangle(rc, colorLightCyan);
  gdi.scrollToBottom();
  gdi.popX();
  gdi.setOnClearCb(SportIdentCB);
}

void TabSI::updateEntryInfo(gdioutput &gdi)
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
    if (gdi.hasField("PayMode")) {
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

void TabSI::generateSplits(const pRunner r, gdioutput &gdi)
{
  const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
  if (wideFormat) {
    addToPrintQueue(r);
    while(checkpPrintQueue(gdi));
  }
  else {
    gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
    vector<int> mp;
    r->evaluateCard(true, mp);
    r->printSplits(gdiprint);
    printProtected(gdi, gdiprint);
    //gdiprint.print(splitPrinter, oe, false, true);
  }
}

void TabSI::generateStartInfo(gdioutput &gdi, const oRunner &r) {
  if (printStartInfo) {
    gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
    r.printStartInfo(gdiprint);
    printProtected(gdi, gdiprint);
    //gdiprint.print(splitPrinter, oe, false, true);
  }
}

void TabSI::printerSetup(gdioutput &gdi)
{
  gdi.printSetup(splitPrinter);
}

void TabSI::checkMoreCardsInQueue(gdioutput &gdi) {
  // Create a local list to avoid stack overflow
  list<SICard> cards = CardQueue;
  CardQueue.clear();
  std::exception storedEx;
  bool fail = false;

  while (!cards.empty()) {
    SICard c = cards.front();
    cards.pop_front();
    try {
      gdi.RemoveFirstInfoBox("SIREAD");
      insertSICard(gdi, c);
    }
    catch (std::exception &ex) {
      fail = true;
      storedEx = ex;
    }
  }

  if (fail)
    throw storedEx;
}

bool TabSI::autoAssignClass(pRunner r, const SICard &sic) {
  if (r && r->getClassId(false)==0) {
    vector<pClass> classes;
    int dist = oe->findBestClass(sic, classes);

    if (classes.size() == 1 && dist>=-1 && dist<=1) // Allow at most one wrong punch
      r->setClassId(classes[0]->getId(), true);
  }

  return r && r->getClassId(false) != 0;
}

void TabSI::showManualInput(gdioutput &gdi) {
  runnerMatchedId = -1;
  gdi.setRestorePoint("ManualInput");
  gdi.fillDown();
  gdi.dropLine(0.7);

  int x = gdi.getCX();
  int y = gdi.getCY();

  gdi.setCX(x+gdi.scaleLength(15));
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
  rc.left=x;
  rc.right=gdi.getWidth()-10;
  rc.top=y;
  rc.bottom=gdi.getCY()+gdi.scaleLength(5);
  gdi.dropLine();
  gdi.addRectangle(rc, colorLightBlue);
  //gdi.refresh();
  gdi.scrollToBottom();
}

void TabSI::tieCard(gdioutput &gdi) {
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

  bool rent = gdi.isChecked("RentCardTie");
  r->setCardNo(card, true, false);
  
  r->getDI().setInt("CardFee", rent ? oe->getBaseCardFee() : 0);
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
  gdi.addString("EditAssign", 0, "Ändra", SportIdentCB).setExtra(r->getId());
  gdi.dropLine(1.5);
  gdi.popX();

  showAssignCard(gdi, false);
}

void TabSI::showAssignCard(gdioutput &gdi, bool showHelp) {
  gdi.enableInput("Interactive");
  gdi.disableInput("Database", true);
  gdi.disableInput("PrintSplits");
  gdi.disableInput("StartInfo");
  gdi.disableInput("UseManualInput");
  gdi.setRestorePoint("ManualTie");
  gdi.fillDown();
  if (interactiveReadout) {
    if (showHelp)
      gdi.addString("", 10, L"Avmarkera 'X' för att hantera alla bricktildelningar samtidigt.#" + lang.tl("Interaktiv inläsning"));
  }
  else {
    if (showHelp)
      gdi.addString("", 10, L"Markera 'X' för att hantera deltagarna en och en.#" + lang.tl("Interaktiv inläsning"));
    gEvent->assignCardInteractive(gdi, SportIdentCB);
    gdi.refresh();
    return;
  }

  runnerMatchedId = -1;
  gdi.fillDown();
  gdi.dropLine(0.7);

  int x = gdi.getCX();
  int y = gdi.getCY();

  gdi.setCX(x+gdi.scaleLength(15));
  gdi.dropLine();
  gdi.addString("", 1, "Knyt bricka / deltagare");
  gdi.fillRight();
  gdi.pushX();
  gdi.dropLine();
  gdi.addInput("RunnerId", L"", 20, SportIdentCB, L"Nummerlapp, lopp-id eller namn:");
  gdi.addInput("CardNo", L"", 8, SportIdentCB, L"Bricknr:");
  gdi.dropLine(1.2);
  gdi.addCheckbox("AutoTie", "Knyt automatiskt efter inläsning", SportIdentCB, oe->getPropertyInt("AutoTie", 1) != 0);
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
  rc.left=x;
  rc.right=gdi.getWidth()+gdi.scaleLength(5);
  rc.top=y;
  rc.bottom=gdi.getCY()+gdi.scaleLength(5);
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
  for ( size_t k = 0; k< runners.size(); k++) {
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

bool TabSI::askOverwriteCard(gdioutput &gdi, pRunner r) const {
  return gdi.ask(L"ask:overwriteresult#" + r->getCompleteIdentification());
}

void TabSI::showModeCardData(gdioutput &gdi) {
  gdi.disableInput("Interactive", true);
  gdi.enableInput("Database", true);
  gdi.enableInput("PrintSplits");
  gdi.disableInput("StartInfo", true);
  gdi.disableInput("UseManualInput", true);

  gdi.dropLine();
  gdi.fillDown();
  gdi.pushX();
  gdi.addString("", boldLarge,  "Print Card Data");
  gdi.addString("", 10, "help:analyzecard");
  gdi.dropLine();
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
  gdi.dropLine(3);
  gdi.popX();
  bool first = true;
  for (list<pair<int, SICard> >::iterator it = savedCards.begin(); it != savedCards.end(); ++it) {
    gdi.dropLine(0.5);
    if (!first) {
      RECT rc = {30, gdi.getCY(), gdi.scaleLength(250), gdi.getCY() + 3};
      gdi.addRectangle(rc);
    }
    first = false;

    printCard(gdi, it->first, false);
  }
}

void TabSI::EditCardData::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GUI_LINK) {
    TextInfo &ti = dynamic_cast<TextInfo &>(info);
    int cardId = ti.getExtraInt();
    SICard &card = tabSI->getCard(cardId);
    ti.id = "card" + itos(cardId);
    gdi.removeControl("CardName");
    gdi.removeControl("ClubName");
    gdi.removeControl("OKCard");
    gdi.removeControl("CancelCard");

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

    InputInfo &ii = gdi.addInput(ti.xp-2, ti.yp-2, "CardName", name, 18, 0);
    ii.setHandler(this);
    InputInfo &ii2 = gdi.addInput(ti.xp + ii.getWidth(), ti.yp-2, "ClubName", club, 22, 0);
    ii2.setExtra(noClub).setHandler(this);
    ButtonInfo &bi = gdi.addButton(ii2.getX() + 2 + ii2.getWidth(), ti.yp-4, "OKCard", "OK", 0);
    bi.setExtra(cardId).setHandler(this);
    bi.setDefault();
    int w, h;
    bi.getDimension(gdi, w, h);
    gdi.addButton(bi.xp + w + 4, ti.yp-4, "CancelCard", "Avbryt", 0).setCancel().setHandler(this);
    gdi.setInputFocus(ii.id, noName);
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo bi = dynamic_cast<ButtonInfo &>(info);
    //OKCard or CancelCard
    if (bi.id == "OKCard") {
      int cardId = bi.getExtraInt();
      SICard &card = tabSI->getCard(cardId);
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

    gdi.removeControl("CardName");
    gdi.removeControl("ClubName");
    gdi.removeControl("OKCard");
    gdi.removeControl("CancelCard");
  }
  else if (type == GUI_FOCUS) {
    InputInfo &ii = dynamic_cast<InputInfo &>(info);
    if (ii.getExtraInt()) {
      ii.setExtra(0);
      gdi.setInputFocus(ii.id, true);
    }
  }
}

void TabSI::printCard(gdioutput &gdi, int cardId, bool forPrinter) const {
  SICard &c = getCard(cardId);
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
    const RunnerWDBEntry *r = oe->getRunnerDatabase().getRunnerByCard(c.CardNumber);
    if (r) {
      r->getName(name);
      const oClub *club = oe->getRunnerDatabase().getClub(r->dbe().clubNo);
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
      name += L", "  + clubName;
    gdi.fillDown();
    gdi.addStringUT(0, name).setExtra(cardId).setHandler(&editCardData);
    gdi.popX();
  }
  gdi.fillDown();
  gdi.addStringUT(0, c.readOutTime);
  gdi.popX();

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

  int accTime = 0;
  int days = 0;
  for (unsigned k = 0; k < c.nPunch; k++) {
    int cy = gdi.getCY();
    gdi.addStringUT(cy, xp, 0, itos(k+1) + ".");
    gdi.addStringUT(cy, xp2, 0, itos(c.Punch[k].Code));
    gdi.addStringUT(cy, xp3, 0, formatTimeHMS(c.Punch[k].Time % (24*3600)));
    if (start != NOTIME) {
      int legTime = analyzePunch(c.Punch[k], start, accTime, days);
      if (legTime > 0)
        gdi.addStringUT(cy, xp5-gdi.scaleLength(10), textRight, formatTime(legTime));

      gdi.addStringUT(cy, xp5 + gdi.scaleLength(40), textRight, formatTime(days*3600*24 + accTime));
    }
    else {
      start = c.Punch[k].Time;
    }
  }
  if (c.FinishPunch.Code != -1) {
    int cy = gdi.getCY();
    gdi.addString("", cy, xp, 0, "Mål");
    gdi.addStringUT(cy, xp3, 0, formatTimeHMS(c.FinishPunch.Time % (24*3600)));

    if (start != NOTIME) {
      int legTime = analyzePunch(c.FinishPunch, start, accTime, days);
      if (legTime > 0)
        gdi.addStringUT(cy, xp5-gdi.scaleLength(10), textRight, formatTime(legTime));

      gdi.addStringUT(cy, xp5 + gdi.scaleLength(40), textRight, formatTime(days*3600*24 + accTime));
    }
    gdi.addString("", 1, L"Time: X#" + formatTime(days*3600*24 + accTime));
  }

  if (forPrinter) {
    gdi.dropLine(1);

    vector< pair<wstring, int> > lines;
    oe->getExtraLines("SPExtra", lines);

    for (size_t k = 0; k < lines.size(); k++) {
      gdi.addStringUT(lines[k].second, lines[k].first);
    }
    if (lines.size()>0)
      gdi.dropLine(0.5);

    gdi.addString("", fontSmall, "Av MeOS: www.melin.nu/meos");
  }
}

int TabSI::analyzePunch(SIPunch &p, int &start, int &accTime, int &days) {
  int newAccTime = p.Time - start;
  if (newAccTime < 0) {
    newAccTime += 3600 * 24;
    if (accTime > 12 * 3600)
      days++;
  }
  else if (newAccTime < accTime - 12 * 3600) {
    days++;
  }
  int legTime = newAccTime - accTime;
  accTime = newAccTime;
  return legTime;
}

void TabSI::generateSplits(int cardId, gdioutput &gdi) {
  gdioutput gdiprint(2.0, gdi.getHWNDTarget(), splitPrinter);
  printCard(gdiprint, cardId, true);
  printProtected(gdi, gdiprint);
}

void TabSI::printProtected(gdioutput &gdi, gdioutput &gdiprint) {
  try {
    gdiprint.print(splitPrinter, oe, false, true);
  }
  catch (meosException &ex) {
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

void TabSI::createCompetitionFromCards(gdioutput &gdi) {
  oe->newCompetition(lang.tl(L"Ny tävling"));
  gdi.setWindowTitle(L"");
  map<size_t, int> hashCount;
  vector< pair<size_t, SICard *> > cards;
  int zeroTime = 3600 * 24;
  for (list<pair<int, SICard> >::iterator it = savedCards.begin(); it != savedCards.end(); ++it) {
    size_t hash = 0;
    if (it->second.StartPunch.Code != -1 && it->second.StartPunch.Time > 0)
      zeroTime = min<int>(zeroTime, it->second.StartPunch.Time);

    for (unsigned k = 0; k < it->second.nPunch; k++) {
      hash = 997 * hash + (it->second.Punch[k].Code-30);
      if (it->second.Punch[k].Code != -1 && it->second.Punch[k].Time > 0)
        zeroTime = min<int>(zeroTime, it->second.Punch[k].Time);
    }
    pair<int, SICard *> p(hash, &it->second);
    ++hashCount[hash];
    cards.push_back(p);
  }

  int course = 0;
  for (size_t k = 0; k < cards.size(); k++) {
    if (!hashCount.count(cards[k].first))
      continue;
    int count = hashCount[cards[k].first];
    if (count < 5 && count < int(cards.size()) /2)
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
  zeroTime -= 3600;
  if (zeroTime < 0)
    zeroTime += 3600 * 24;
  zeroTime -= zeroTime % 1800;
  oe->setZeroTime(formatTime(zeroTime));

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
                          cards[k].second->CardNumber, 0, true);

      processInsertCard(*cards[k].second);
    }
  }

  TabList &tc = dynamic_cast<TabList &>(*gdi.getTabs().get(TListTab));
  tc.loadPage(gdi, "ResultIndividual");
}

void TabSI::StoredStartInfo::checkAge() {
  DWORD t = GetTickCount();
  const int minuteLimit = 3;
  if (t > age && (t - age) > (1000*60*minuteLimit)) {
    clear();
  }
  age = t;
}

void TabSI::StoredStartInfo::clear() {
  age = GetTickCount();
  storedName.clear();
  storedCardNo.clear();
  storedClub.clear();
  storedFee.clear();
  storedPhone.clear();
  rentState = false;
  storedStartTime.clear();
  hasPaid = false;
  payMode = 1000;
  //allStages = lastAllStages; // Always use last setting
  storedClassId = 0;
}

void TabSI::clearCompetitionData() {
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
}

SICard &TabSI::getCard(int id) const {
  if (id < int(savedCards.size() / 2)) {
    for (list< pair<int, SICard> >::const_iterator it = savedCards.begin(); it != savedCards.end(); ++it){
      if (it->first==id)
        return const_cast<SICard &>(it->second);
    }
  }
  else {
    for (list< pair<int, SICard> >::const_reverse_iterator it = savedCards.rbegin(); it != savedCards.rend(); ++it){
      if (it->first==id)
        return const_cast<SICard &>(it->second);
    }
  }
  throw meosException("Interal error");
}

bool compareCardNo(const pRunner &r1, const pRunner &r2) {
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

wstring TabSI::getCardInfo(bool param, vector<int> &count) const {
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

void TabSI::showRegisterHiredCards(gdioutput &gdi) {
  gdi.disableInput("Interactive");
  gdi.disableInput("Database");
  gdi.disableInput("PrintSplits");
  gdi.disableInput("UseManualInput");

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
  auto &hiredCards = oe->getHiredCards();
  for (int i : hiredCards) {
    gdi.addStringUT(0, itos(i)).setExtra(i).setHandler(getResetHiredCardHandler());
  }

  gdi.refresh();
}

void TabSI::showCheckCardStatus(gdioutput &gdi, const string &cmd) {
  vector<pRunner> r;
  const int cx = gdi.getCX();
  const int col1 = gdi.scaleLength(50);
  const int col2 = gdi.scaleLength(200);
 
  if (cmd == "init") {
    gdi.disableInput("Interactive");
    gdi.disableInput("Database");
    gdi.disableInput("PrintSplits");
    gdi.disableInput("UseManualInput");
    
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
          wstring cp = r[k]->getCompleteIdentification();
          bool hire = r[k]->isHiredCard();
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
        wstring cp = r[k]->getCompleteIdentification();

        if (r[k]->getStatus() != StatusUnknown)
          cp += L" " + r[k]->getStatusS(true);
        else
          cp += makeDash(L" -");

        int s = r[k]->getStartTime();
        int f = r[k]->getFinishTime();
        if (s> 0 || f>0) {
          cp += L", " + (s>0 ? r[k]->getStartTimeS() : wstring(L"?")) + makeDash(L" - ") 
                 + (f>0 ? r[k]->getFinishTimeS() : wstring(L"?"));  
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
  oEvent *oe;
  
public:
  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
    if (type == GuiEventType::GUI_LINK) {
      TextInfo &ti = dynamic_cast<TextInfo &>(info);
      int c = _wtoi(ti.text.c_str());
      if (gdi.ask(L"Vill du ta bort brickan från hyrbrickslistan?")) {
        oe->setHiredCard(c, false);
        ti.text = L"-";
        ti.setHandler(nullptr);
        gdi.refreshFast();
      }
    }
  }

  ResetHiredCard(oEvent *oe) : oe(oe) {}
};

GuiHandler *TabSI::getResetHiredCardHandler() {
  if (!resetHiredCardHandler)
    resetHiredCardHandler = make_shared<ResetHiredCard>(oe);
  
  return resetHiredCardHandler.get();  
}

void TabSI::registerHiredCard(gdioutput &gdi, const SICard &sic) {
  if (!oe->isHiredCard(sic.CardNumber))
    oe->setHiredCard(sic.CardNumber, true);
  gdi.addStringUT(0, itos(sic.CardNumber)).setHandler(getResetHiredCardHandler());
  gdi.scrollToBottom();
  gdi.refresh();
}

void TabSI::checkCard(gdioutput &gdi, const SICard &card, bool updateAll) {
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
  TextInfo &ti = gdi.addStringUT(cardPosY, cardPosX + cardCurrentCol * cardOffsetX, 0, itos(card.CardNumber));
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

void TabSI::generatePayModeWidget(gdioutput &gdi) const {
  vector< pair<wstring, size_t> > pm;
  oe->getPayModes(pm);
  assert(pm.size() > 0);
  if (pm.size() == 1) {
    assert(pm[0].second == 0);
    gdi.addCheckbox("Paid", L"#" + pm[0].first, SportIdentCB, storedInfo.hasPaid);
  }
  else {
    pm.insert(pm.begin(), make_pair(lang.tl(L"Faktureras"), 1000));
    gdi.addSelection("PayMode", 110, 100, SportIdentCB);
    gdi.addItem("PayMode", pm);
    gdi.selectItemByData("PayMode", storedInfo.payMode);
    gdi.autoGrow("PayMode");
  }
}

bool TabSI::writePayMode(gdioutput &gdi, int amount, oRunner &r) {
  int paid = 0;
  bool hasPaid = false;
      
  if (gdi.hasField("PayMode"))
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
  unsigned t = GetTickCount();
  printPunchRunnerIdQueue.push_back(make_pair(t, r->getId()));
}

bool TabSI::checkpPrintQueue(gdioutput &gdi) {
  if (printPunchRunnerIdQueue.empty())
    return false;
  size_t printLen = oe->getPropertyInt("NumSplitsOnePage", 3);
  if (printPunchRunnerIdQueue.size() < printLen) {
    unsigned t = GetTickCount();
    unsigned diff = abs(int(t - printPunchRunnerIdQueue.front().first))/1000;

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
      r->evaluateCard(true, mp);
      r->printSplits(gdiprint);
    }
    gdiprint.dropLine(4);
  }
  
  printProtected(gdi, gdiprint);
  //gdiprint.print(splitPrinter, oe, false, true);
  return true;
}

void TabSI::printSIInfo(gdioutput &gdi, const wstring &port) const {
  vector<wstring> info;
  gdi.fillDown();
  gSI->getInfoString(port, info);
  for (size_t j = 0; j < info.size(); j++)
    gdi.addStringUT(0, info[j]);      
}

oClub *TabSI::extractClub(gdioutput &gdi) const {
  auto &db = oe->getRunnerDatabase();
  oClub *dbClub = nullptr;
  if (gdi.hasField("Club")) {
    int clubId = gdi.getExtraInt("Club");
    if (clubId > 0) {
      dbClub = db.getClub(clubId-1);
      if (dbClub && !stringMatch(dbClub->getName(), gdi.getText("Club")))
        dbClub = nullptr;
    }
    if (dbClub == nullptr) {
      dbClub = db.getClub(gdi.getText("Club"));
    }
  }
  return dbClub;
}

RunnerWDBEntry *TabSI::extractRunner(gdioutput &gdi) const {
  auto &db = oe->getRunnerDatabase();

  int rId = gdi.getExtraInt("Name");
  wstring name = gdi.getText("Name");
  RunnerWDBEntry *dbR = nullptr;
  if (rId > 0) {
    dbR = db.getRunnerByIndex(rId-1);
    if (dbR) {
      wstring fname = dbR->getFamilyName();
      wstring gname = dbR->getGivenName();

      if (wcsstr(name.c_str(), fname.c_str()) == nullptr || wcsstr(name.c_str(), gname.c_str()) == nullptr)
        dbR = nullptr;
    }
  }
  if (dbR == nullptr) {
    oClub * dbClub = extractClub(gdi);
    dbR = db.getRunnerByName(name, dbClub ? dbClub->getId() : 0, 0);
  }
  return dbR;
}

void TabSI::DirectEntryGUI::updateFees(gdioutput &gdi, const pClass cls, int age) {
  int fee = cls->getEntryFee(getLocalDate(), age);
  auto fees = cls->getAllFees();
  gdi.addItem("Fee", fees);
  if (fee > 0) {
    gdi.selectItemByData("Fee", fee);
    gdi.setText("Fee", tabSI->oe->formatCurrency(fee));
  }
  else if (!fees.empty())
    gdi.selectFirstItem("Fee");

  tabSI->updateEntryInfo(gdi);
}

void TabSI::DirectEntryGUI::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GUI_FOCUS) {
    InputInfo &ii = dynamic_cast<InputInfo &>(info);
    /*if (ii.getExtraInt()) {
      ii.setExtra(0);
      gdi.setInputFocus(ii.id, true);
    }*/
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo &lbi = dynamic_cast<ListBoxInfo &>(info);
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
    ListBoxInfo &combo = dynamic_cast<ListBoxInfo &>(info);
    bool show = false;
    if (tabSI->useDatabase && combo.id == "Club" && combo.text.length() > 1) {
      auto clubs = tabSI->oe->getRunnerDatabase().getClubSuggestions(combo.text, 20);
      if (!clubs.empty()) {
        auto &ac = gdi.addAutoComplete(combo.id);
        ac.setAutoCompleteHandler(this->tabSI);
        vector<AutoCompleteRecord> items;
        for (auto club : clubs)
          items.emplace_back(club->getDisplayName(), club->getName(), club->getId());

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
    InputInfo &ii = dynamic_cast<InputInfo &>(info);
    bool show = false;
    if (tabSI->showDatabase() && ii.id == "Name") {
      auto &db = tabSI->oe->getRunnerDatabase();
      if (ii.text.length() > 1) {
        auto dbClub = tabSI->extractClub(gdi);
        auto rw = db.getRunnerSuggestions(ii.text, dbClub ? dbClub->getId() : 0, 10);
        if (!rw.empty()) {
          auto &ac = gdi.addAutoComplete(ii.id);
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

vector<AutoCompleteRecord> TabSI::getRunnerAutoCompelete(RunnerDB &db, const vector< pair<RunnerWDBEntry *, int>> &rw, pClub dbClub) {
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

      items.emplace_back(ns, r.first->getNameCstr(), r.second);
    }
    if (!needRerun)
      break;
    else if (i == 0) {
      items.clear();
    }
  }
  return items;
}

void TabSI::handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) {
  auto bi = gdi.setText(info.getTarget(), info.getCurrent().c_str());
  if (bi) {
    int ix = info.getCurrentInt();
    
    bi->setExtra(ix+1);
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
        if (gdi.hasField("Club") && r->dbe().clubNo) {
          if (gdi.getText("Club").empty()) {
            auto pclub = oe->getRunnerDatabase().getClub(r->dbe().clubNo);
            if (pclub)
              gdi.setText("Club", pclub->getName());
          }
        }
        if (gdi.hasField("CardNo") && r->dbe().cardNo) {
          if (gdi.getText("CardNo").empty())
            gdi.setText("CardNo", r->dbe().cardNo);
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
