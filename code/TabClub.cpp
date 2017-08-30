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
    Stigbergsvägen 11, SE-75242 UPPSALA, Sweden

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
#include <cassert>
#include "meos_util.h"
#include "Table.h"
#include "gdifonts.h"
#include "meosexception.h"
#include "MeOSFeatures.h"

#include "TabCompetition.h"
#include "TabClub.h"
#include "TabList.h"

#include "csvparser.h"
#include "pdfwriter.h"


TabClub::TabClub(oEvent *poe):TabBase(poe)
{
  baseFee = 0;
  lowAge = 0;
  highAge = 0;
  filterAge = false;
  onlyNoFee = false;
  useManualFee = false;
}

TabClub::~TabClub(void)
{
}

void TabClub::readFeeFilter(gdioutput &gdi) {
  baseFee = oe->interpretCurrency(gdi.getText("BaseFee"));
  firstDate = gdi.getText("FirstDate");
  lastDate = gdi.getText("LastDate");
  filterAge = gdi.isChecked("FilterAge");
  useManualFee = gdi.isChecked("DefaultFees");
  if (filterAge) {
    highAge = gdi.getTextNo("HighLimit");
    lowAge = gdi.getTextNo("LowLimit");
  }

  onlyNoFee = gdi.isChecked("OnlyNoFee");

  ListBoxInfo lbi;
  gdi.getSelectedItem("ClassType", lbi);

  if (lbi.data == -5)
    typeS = L"*";
  else if (lbi.data > 0)
    typeS = L"::" + itow(lbi.data);
  else
    typeS = lbi.text;
}

void TabClub::selectClub(gdioutput &gdi,  pClub pc)
{
  if (pc) {
    pc->synchronize();
    ClubId = pc->getId();
  }
  else{
    ClubId = 0;
  }
}

void manualFees(gdioutput &gdi, bool on) {
  gdi.setInputStatus("BaseFee", on);
  gdi.setInputStatus("FirstDate", on);
  gdi.setInputStatus("LastDate", on);
}

void ageFilter(gdioutput &gdi, bool on, bool use) {
  gdi.setInputStatus("HighLimit", on & use);
  gdi.setInputStatus("LowLimit", on & use);
  gdi.setInputStatus("FilterAge", use);
}

int ClubsCB(gdioutput *gdi, int type, void *data)
{
  TabClub &tc = dynamic_cast<TabClub &>(*gdi->getTabs().get(TClubTab));
  return tc.clubCB(*gdi, type, data);
}

int TabClub::clubCB(gdioutput &gdi, int type, void *data)
{
  if (type==GUI_BUTTON) {
    ButtonInfo bi=*(ButtonInfo *)data;

    if (bi.id=="Save") {
    }
    else if (bi.id == "EraseClubs") {
      if (gdi.ask(L"Vill du ta bort alla klubbar från tävlingen? Alla deltagare blir klubblösa.")) {
        oClub::clearClubs(*oe);
      }
    }
    else if (bi.id=="Invoice") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Clubs", lbi);
      pClub pc=oe->getClub(lbi.data);
      if (pc) {
        gdi.clearPage(true);
        oe->calculateTeamResults(false);
        oe->sortTeams(ClassStartTime, 0, true);
        oe->calculateResults(oEvent::RTClassResult);
        oe->sortRunners(ClassStartTime);
        int pay, paid;
        {
          map<int, int> ppm;
          map<int, wstring> dpm;
          oClub::definedPayModes(*oe, dpm);
          pc->generateInvoice(gdi, pay, paid, dpm, ppm);
        }
        gdi.addButton(gdi.getWidth()+20, 15, gdi.scaleLength(120),
                      "Cancel", "Återgå", ClubsCB, "", true, false);
        gdi.addButton(gdi.getWidth()+20, 45,  gdi.scaleLength(120),
                      "Print", "Skriv ut...", ClubsCB,
                      "Skriv ut fakturan", true, false);
        gdi.addButton(gdi.getWidth()+20, 75,  gdi.scaleLength(120),
                      "PDF", "PDF...", ClubsCB,
                      "Spara som PDF.", true, false);
        gdi.refresh();
      }
    }
    else if (bi.id=="AllInvoice") {
      gdi.clearPage(false);
      gdi.addString("", boldLarge, "Skapa fakturor");

      gdi.addSelection("Type", 300, 100, 0, L"Val av export:");

      gdi.addItem("Type", lang.tl("Skriv ut alla"), oEvent::IPTAllPrint);
      gdi.addItem("Type", lang.tl("Exportera alla till HTML"), oEvent::IPTAllHTML);
      gdi.addItem("Type", lang.tl("Exportera alla till PDF"), oEvent::IPTAllPDF);

#ifdef _DEBUG
      gdi.addItem("Type", lang.tl("Skriv ut dem utan e-post"), oEvent::IPTNoMailPrint);
      gdi.addItem("Type", lang.tl("Skriv ut ej accepterade elektroniska"), oEvent::IPTNonAcceptedPrint);
      gdi.addItem("Type", lang.tl("Exportera elektroniska fakturor"), oEvent::IPTElectronincHTML);
#endif
      gdi.selectFirstItem("Type");
      gdi.fillRight();
      gdi.pushX();
      gdi.addButton("DoAllInvoice", "Skapa fakturor", ClubsCB);
#ifdef _DEBUG
      gdi.addButton("ImportAnswer", "Hämta svar om elektroniska fakturor", ClubsCB);
#endif
      gdi.addButton("Cancel", "Avbryt", ClubsCB);
      gdi.refresh();
    }
    else if (bi.id=="DoAllInvoice") {
      ListBoxInfo lbi;
      gdi.getSelectedItem("Type", lbi);
      wstring path;
      if (lbi.data > 10)
        path = gdi.browseForFolder(path, 0);
      gdi.clearPage(false);

      oe->printInvoices(gdi, oEvent::InvoicePrintType(lbi.data), path, false);

      gdi.addButton(gdi.getWidth()+20, 15, gdi.scaleLength(120),
                    "Cancel", "Återgå", ClubsCB, "", true, false);

      if (lbi.data>10) { // To file
        gdi.addButton(gdi.getWidth()+20, 45,  gdi.scaleLength(120),
                      "Print", "Skriv ut...", ClubsCB,
                      "", true, false);
        gdi.addButton(gdi.getWidth()+20, 75,  gdi.scaleLength(120),
                      "PDF", "PDF...", ClubsCB,
                      "Spara som PDF.", true, false);
        gdi.refresh();
      }
      else {
        gdi.refresh();
        gdi.print(oe, 0, false, false);
      }
    }
    else if (bi.id=="ImportAnswer") {
      vector< pair<wstring, wstring> > ft;
      ft.push_back(make_pair(L"Textfiler", L"*.txt"));
      wstring file = gdi.browseForOpen(ft, L"txt");
      if (!file.empty()) {
        gdi.clearPage(true);
        try {
          importAcceptedInvoice(gdi, file);
        }
        catch (meosException &ex) {
          gdi.addString("", 0, ex.wwhat()).setColor(colorRed);
        }
        catch (std::exception &ex) {
          gdi.addString("", 0, ex.what()).setColor(colorRed);
        }
        gdi.addButton("Cancel", "OK", ClubsCB);
      }
    }
    else if (bi.id=="UpdateAll") {
      oe->updateClubsFromDB();
      gdi.getTable().update();
      gdi.refresh();
    }
    else if (bi.id=="UpdateAllRunners") {
      oe->updateClubsFromDB();
      oe->updateRunnersFromDB();
      gdi.getTable().update();
      gdi.refresh();
    }
    else if (bi.id=="Update") {
      pClub pc=oe->getClub(gdi.getSelectedItem("Clubs").first);
      if (pc) {
        pc->updateFromDB();
        pc->synchronize();
        gdi.getTable().update();
        gdi.refresh();
      }
    }
    else if (bi.id=="Summary") {
      gdi.clearPage(false);
      wstring nn;
      oe->printInvoices(gdi, oEvent::IPTAllPrint, nn, true);
      gdi.addButton(gdi.getWidth()+20, 15,  gdi.scaleLength(120), "Cancel",
                    "Återgå", ClubsCB, "", true, false);
      gdi.addButton(gdi.getWidth()+20, 45,  gdi.scaleLength(120), "Print",
                    "Skriv ut...", ClubsCB, "Skriv ut fakturan", true, false);

      gdi.refresh();
    }
    else if (bi.id=="Merge") {
      ClubId = gdi.getSelectedItem("Clubs").first;
      pClub pc = oe->getClub(ClubId);
      if (pc) {
        gdi.clearPage(false);
        gdi.addString("", boldText, "Slå ihop klubb");

        wchar_t bf[256];
        swprintf_s(bf, lang.tl("help:12352").c_str(), pc->getName().c_str(), pc->getId());

        gdi.addStringUT(10, bf);

        gdi.addSelection("NewClub", 200, 300, 0, L"Ny klubb:");
        oe->fillClubs(gdi, "NewClub");
        gdi.selectItemByData("NewClub", pc->getId());
        gdi.removeSelected("NewClub");

        gdi.pushX();
        gdi.fillRight();
        gdi.addButton("DoMerge", "Slå ihop", ClubsCB);
        gdi.addButton("Cancel", "Avbryt", ClubsCB);
        gdi.fillDown();
        gdi.popX();
        gdi.dropLine(2);
        gdi.addStringUT(boldText, lang.tl("Klubb att ta bort: ") + pc->getName());
        oe->viewClubMembers(gdi, pc->getId());

        gdi.refresh();
      }
    }
    else if (bi.id=="DoMerge") {
      pClub pc1 = oe->getClub(ClubId);
      pClub pc2 = oe->getClub(gdi.getSelectedItem("NewClub").first);

      if (pc1==pc2)
        throw std::exception("En klubb kan inte slås ihop med sig själv.");

      if (pc1 && pc2)
        oe->mergeClub(pc2->getId(), pc1->getId());
      loadPage(gdi);
    }
    else if (bi.id == "InvoiceSettings") {
      gdi.clearPage(true);
      gdi.addString("", boldLarge, "Fakturainställningar");
      gdi.dropLine();
      firstInvoice = oClub::getFirstInvoiceNumber(*oe);
      if (firstInvoice == 0)
        firstInvoice = oe->getPropertyInt("FirstInvoice", 1000);

      gdi.addInput("FirstInvoice", itow(firstInvoice), 5, 0, L"Första fakturanummer:");

      gdi.dropLine();
      gdi.addString("", boldText, "Organisatör");

      vector<string> fields;
      gdi.pushY();
      fields.push_back("Organizer");
      fields.push_back("CareOf");
      fields.push_back("Street");
      fields.push_back("Address");
      fields.push_back("EMail");
      oe->getDI().buildDataFields(gdi, fields);

      gdi.dropLine();
      gdi.addString("", boldText, "Betalningsinformation");
      fields.clear();
      fields.push_back("Account");
      fields.push_back("PaymentDue");
      oe->getDI().buildDataFields(gdi, fields);

      gdi.pushX();
      gdi.fillRight();
      gdi.addString("", normalText, "Avgifter och valuta ställer du in under");
      gdi.addString("CmpSettings", normalText, "Tävlingsinställningar.", ClubsCB);
      gdi.fillDown();
      gdi.dropLine(2);
      gdi.popX();

      gdi.addString("", boldText, "Formatering");

      gdi.fillRight();
      gdi.addString("", 0, "Koordinater (mm) för adressfält:");
      wstring xc = oe->getPropertyString("addressxpos", L"125");
      wstring yc = oe->getPropertyString("addressypos", L"50");
      gdi.addStringUT(0, "x:");
      gdi.addInput("XC", xc + L" [mm]", 6);
      gdi.addStringUT(0, "y:");
      gdi.addInput("YC", yc + L" [mm]", 6);

      gdi.fillDown();
      gdi.popX();

      TabList::customTextLines(*oe, "IVExtra", gdi);

      gdi.dropLine(1);

      gdi.fillRight();
      gdi.addButton("SaveSettings", "Spara", ClubsCB);
      gdi.addButton("Cancel", "Avbryt", ClubsCB);
      gdi.dropLine(2);
      gdi.setOnClearCb(ClubsCB);
      oe->getDI().fillDataFields(gdi);

    }
    else if (bi.id == "SaveSettings") {
      oe->getDI().saveDataFields(gdi);

      TabList::saveExtraLines(*oe, "IVExtra", gdi);

      int fn = gdi.getTextNo("FirstInvoice");

      if (fn != firstInvoice && oClub::getFirstInvoiceNumber(*oe) > 0) {
        if (gdi.ask(L"Tilldela nya fakturanummer till alla klubbar?")) {
          oe->setProperty("FirstInvoice", fn);
          oClub::assignInvoiceNumber(*oe, true);
        }
      }
      else
        oe->setProperty("FirstInvoice", fn);

      int xc = gdi.getTextNo("XC");
      int yc = gdi.getTextNo("YC");

      if (xc<=0 || yc<=0)
        throw meosException("Invalid coordinate (x,y)");

      oe->setProperty("addressxpos", xc);
      oe->setProperty("addressypos", yc);
      loadPage(gdi);
    }
    else if (bi.id == "Fees") {
      gdi.clearPage(true);

      gdi.addString("", boldLarge, "Tilldela avgifter");

      gdi.dropLine();
      gdi.addString("", 10, "help:assignfee");
      gdi.dropLine();
      gdi.pushX();

      gdi.addSelection("ClassType", 150, 300, 0, L"Klass / klasstyp:");
      vector< pair<wstring, size_t> > types;
      vector< pair<wstring, size_t> > classes;

      oe->fillClassTypes(types);
      oe->fillClasses(classes, oEvent::extraNone, oEvent::filterNone);
      types.insert(types.end(), classes.begin(), classes.end());
      gdi.addItem("ClassType", types);
      gdi.addItem("ClassType", lang.tl("Alla typer"), -5);

      gdi.selectItemByData("ClassType", -5);

      gdi.fillRight();
      gdi.dropLine(2);
      gdi.addCheckbox("DefaultFees", "Manuella avgifter:", ClubsCB, useManualFee);

      gdi.dropLine(-1);

      int px = gdi.getCX();
      gdi.addInput("BaseFee", oe->formatCurrency(baseFee), 8, 0, L"Avgift:");
      gdi.addInput("FirstDate", firstDate, 10, 0, L"Undre datumgräns:", L"ÅÅÅÅ-MM-DD");
      gdi.addInput("LastDate", lastDate, 10, 0, L"Övre datumgräns:", L"ÅÅÅÅ-MM-DD");

      manualFees(gdi, useManualFee);

      gdi.setCX(px);
      gdi.dropLine(4);
      gdi.fillRight();
      gdi.addCheckbox("FilterAge", "Åldersfilter:", ClubsCB, filterAge);

      gdi.dropLine(-1);
      gdi.addInput("LowLimit", lowAge > 0 ? itow(lowAge) : L"", 5, 0, L"Undre gräns (år):");
      gdi.addInput("HighLimit", highAge > 0 ? itow(highAge) : L"", 5, 0, L"Övre gräns (år):");
      ageFilter(gdi, filterAge, useManualFee);

      gdi.popX();
      gdi.fillDown();
      gdi.dropLine(3);

      gdi.addCheckbox("OnlyNoFee", "Tilldela endast avgift till deltagare utan avgift", ClubsCB, onlyNoFee);

      gdi.pushX();
      gdi.fillRight();
      gdi.addButton("ShowFiltered", "Visa valda deltagare", ClubsCB);

      gdi.addButton("DoFees", "Tilldela avgifter", ClubsCB);
      gdi.addButton("ClearFees", "Nollställ avgifter", ClubsCB);

      gdi.addButton("Cancel", "Återgå", ClubsCB);
      gdi.popX();
      gdi.fillDown();
      gdi.dropLine(2);
      gdi.refresh();
    }
    else if (bi.id == "FilterAge") {
      ageFilter(gdi, gdi.isChecked(bi.id), gdi.isChecked("DefaultFees"));
    }
    else if (bi.id == "DefaultFees") {
      manualFees(gdi, gdi.isChecked(bi.id));
      ageFilter(gdi, gdi.isChecked("FilterAge"), gdi.isChecked(bi.id));
    }
    else if (bi.id == "DoFees" || bi.id == "ClearFees" ||
             bi.id == "ShowFiltered" || bi.id == "ResetFees") {

      readFeeFilter(gdi);
      int op;

      if (bi.id == "DoFees") {
        if (useManualFee)
          op = 0;
        else
          op = 2;
      }
      else if (bi.id == "ClearFees")
        op = 1;
      else if (bi.id == "ResetFees")
        op = 2;
      else
        op = 3;

      gdi.restore("FeeList", false);
      gdi.setRestorePoint("FeeList");
      gdi.fillDown();

      vector<pRunner> filtered;

      oe->sortRunners(ClassStartTimeClub);
      wstring fdate, ldate;
      int lage = 0, hage = 0;

      if (useManualFee) {
        fdate = firstDate;
        ldate = lastDate;

        if (filterAge) {
          lage = lowAge;
          hage = highAge;
        }
      }

      oe->selectRunners(typeS, lage, hage, fdate, ldate, !onlyNoFee, filtered);

      gdi.dropLine(2);
      int modified = 0;
      int count = 0;
      for (size_t k = 0; k<filtered.size(); k++) {
        if (op != 1 && filtered[k]->isVacant())
          continue;
        count++;

        oDataInterface di = filtered[k]->getDI();
        int fee = 0;

        if (op == 0 || op == 1) {
          if (op == 0)
            fee = baseFee;

          if (di.getInt("Fee") != fee) {
            di.setInt("Fee", fee);
            modified++;
            filtered[k]->synchronize(true);
          }
        }
        else if (op == 2) {
          filtered[k]->addClassDefaultFee(true);
          if (filtered[k]->isChanged())
            modified++;
          filtered[k]->synchronize(true);
          fee = di.getInt("Fee");
        }
        else
          fee = di.getInt("Fee");

        wstring info = filtered[k]->getClass() + L", " + filtered[k]->getCompleteIdentification();

        gdi.addStringUT(0,  info + L" (" + oe->formatCurrency(fee) + L")");
        if (count % 5 == 0)
          gdi.dropLine();
      }

      gdi.dropLine();

      if (count == 0)
        gdi.addString("", 1, "Ingen deltagare matchar sökkriteriet").setColor(colorRed);
      else if (op == 0 || op == 2)
        gdi.addString("", 1, "Ändrade avgift för X deltagare#" + itos(modified)).setColor(colorGreen);
      else if (op == 1)
        gdi.addString("", 1, "Nollställde avgift för X deltagare#" + itos(modified)).setColor(colorGreen);

      gdi.refresh();
    }
    else if (bi.id=="Cancel") {
      loadPage(gdi);
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
        pdf.generatePDF(gdi, file, lang.tl("Faktura"), oe->getDCI().getString("Organizer"), gdi.getTL());
        gdi.openDoc(file.c_str());
      }
    }

  }
  else if (type==GUI_LISTBOX){
    ListBoxInfo bi=*(ListBoxInfo *)data;

    if (bi.id=="Clubs"){
      pClub pc=oe->getClub(bi.data);
      if (!pc)
        throw std::exception("Internal error");

      selectClub(gdi, pc);
    }
  }
  else if (type == GUI_LINK) {
    TextInfo *ti = static_cast<TextInfo*>(data);
    if (ti->id == "CmpSettings") {
      if (gdi.hasField("SaveSettings"))
        gdi.sendCtrlMessage("SaveSettings");
      TabCompetition &tc = dynamic_cast<TabCompetition &>(*gdi.getTabs().get(TCmpTab));
      tc.loadPage(gdi);
      gdi.selectTab(tc.getTabId());
      gdi.sendCtrlMessage("Settings");
      return 0;
    }
  }
  else if (type == GUI_CLEAR) {
    if (gdi.isInputChanged("")) {
      if (gdi.hasField("SaveSettings")) {
        gdi.sendCtrlMessage("SaveSettings");
      }
    }
    return 1;
  }

  return 0;
}


bool TabClub::loadPage(gdioutput &gdi)
{
  oe->checkDB();
  gdi.selectTab(tabId);

  if (baseFee == 0) {
    if (oe->getDCI().getInt("OrdinaryEntry") > 0)
      lastDate = oe->getDCI().getDate("OrdinaryEntry");
    baseFee = oe->getDCI().getInt("EntryFee");

    lowAge = 0;
    highAge = oe->getDCI().getInt("YouthAge");
  }

  gdi.clearPage(false);
  gdi.fillDown();
  gdi.addString("", boldLarge, "Klubbar");
  gdi.dropLine(0.5);
  gdi.pushX();
  gdi.fillRight();
  gdi.addSelection("Clubs", 200, 300, ClubsCB);
  oe->fillClubs(gdi, "Clubs");
  gdi.selectItemByData("Clubs", ClubId);
  gdi.addButton("Merge", "Ta bort / slå ihop...", ClubsCB);
  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy))
    gdi.addButton("Invoice", "Faktura", ClubsCB);
  if (oe->useRunnerDb())
    gdi.addButton("Update", "Uppdatera", ClubsCB, "Uppdatera klubbens uppgifter med data från löpardatabasen/distriktsregistret");

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy)) {
    gdi.popX();
    gdi.dropLine(3);

    gdi.addString("", boldText, "Ekonomi");
    gdi.popX();
    gdi.dropLine(1.5);
    gdi.addButton("Fees", "Avgifter...", ClubsCB);
    gdi.addButton("InvoiceSettings", "Fakturainställningar...", ClubsCB);

    gdi.addButton("AllInvoice", "Skapa fakturor...", ClubsCB);
    gdi.addButton("Summary", "Sammanställning", ClubsCB);
  }

  gdi.popX();
  gdi.dropLine(3);

  gdi.addString("", boldText, "Hantera klubbar");

  gdi.popX();
  gdi.dropLine(1.5);
  if (oe->useRunnerDb()) {
    gdi.addButton("UpdateAll", "Uppdatera alla klubbar", ClubsCB, "Uppdatera klubbarnas uppgifter med data från löpardatabasen/distriktsregistret");
    gdi.addButton("UpdateAllRunners", "Uppdatera klubbar && löpare", ClubsCB, "Uppdatera klubbarnas och löparnas uppgifter med data från löpardatabasen/distriktsregistret");
  }
  gdi.addButton("EraseClubs", "Radera alla klubbar", ClubsCB, "Radera alla klubbar och ta bort klubbtillhörighet");

  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(2);
  gdi.addString("", 10, "help:29758");
  gdi.dropLine(1);
  Table *tbl=oe->getClubsTB();
  gdi.addTable(tbl, gdi.getCX(), gdi.getCY());
  gdi.refresh();
  return true;
}

void TabClub::importAcceptedInvoice(gdioutput &gdi, const wstring &file) {

  gdi.addString("", boldLarge, "Hämta svar om elektroniska fakturor");

  gdi.fillDown();
  gdi.dropLine(2);
  csvparser csv;
  list< vector<wstring> > data;
  csv.parse(file, data);
  list< vector<wstring> >::iterator it;
  map<int, pair<bool, wstring> > hasAccepted;
  for (it = data.begin(); it != data.end(); ++it) {
    if (it->size() == 3) {
      int id = _wtoi((*it)[0].c_str());
      bool accepted = trim((*it)[1]) == L"OK";
      pClub pc = oe->getClub(id);
      if (pc) {
        hasAccepted[id].first = accepted;
        if ( hasAccepted[id].second.empty())
          hasAccepted[id].second = (*it)[2];
        else
          hasAccepted[id].second += L", " + (*it)[2];
      }
      else
        gdi.addString("", 0, "Okänd klubb med id X#" + itos(id)).setColor(colorRed);
    }
    else
      throw meosException("Bad file format.");
  }

  gdi.pushX();
  gdi.fillNone();

  int margin = gdi.getCX() + gdi.scaleLength(30);
  vector<pClub> clubs;
  oe->getClubs(clubs, true);
  bool anyAccepted = false;
  int count = 0;
  for (size_t k = 0; k < clubs.size(); k++) {
    map<int, pair<bool, wstring> >::iterator res = hasAccepted.find(clubs[k]->getId());

    if (res != hasAccepted.end() && res->second.first) {
      if (!anyAccepted) {
        gdi.dropLine();
        gdi.addString("", 1, "Accepterade elektroniska fakturor");
        gdi.dropLine();
        gdi.popX();
        anyAccepted = true;
      }
      clubs[k]->getDI().setString("Invoice", L"A");
      gdi.addStringUT(0, itos(++count) + ".");
      gdi.setCX(margin);
      gdi.addStringUT(0, clubs[k]->getName() + L", " + res->second.second);
      gdi.dropLine();
      gdi.popX();
    }
  }

  bool anyNotAccepted = false;
  count = 0;
  for (size_t k = 0; k < clubs.size(); k++) {
    map<int, pair<bool, wstring> >::iterator res = hasAccepted.find(clubs[k]->getId());

    if (res != hasAccepted.end() && !res->second.first) {
      if (!anyNotAccepted) {
        gdi.dropLine();
        gdi.addString("", 1, "Ej accepterade elektroniska fakturor");
        gdi.dropLine();
        gdi.popX();
        anyNotAccepted = true;
      }
      clubs[k]->getDI().setString("Invoice", L"P");
      gdi.addStringUT(0, itos(++count) + ".");
      gdi.setCX(margin);
      gdi.addStringUT(0, clubs[k]->getName() + L", " + res->second.second);
      gdi.dropLine();
      gdi.popX();

    }
  }

  bool anyNoAnswer = false;
  count = 0;
  for (size_t k = 0; k < clubs.size(); k++) {
    wstring email = clubs[k]->getDCI().getString("EMail");
    bool hasMail = !email.empty() && email.find_first_of('@') != email.npos;

    map<int, pair<bool, wstring> >::iterator res = hasAccepted.find(clubs[k]->getId());

    if (res == hasAccepted.end() ) {
      if (!anyNoAnswer) {
        gdi.dropLine();
        gdi.addString("", 1, "Klubbar som inte svarat");
        gdi.dropLine();
        gdi.popX();

        anyNoAnswer = true;
      }
      gdi.addStringUT(0, itos(++count) + ".");
      gdi.setCX(margin);

      if (hasMail)
        gdi.addStringUT(0, clubs[k]->getName());
      else
        gdi.addString("", 0, L"X (Saknar e-post)#" + clubs[k]->getName());

      gdi.dropLine();
      gdi.popX();
    }
  }
  gdi.fillDown();
  gdi.dropLine();
}

void TabClub::clearCompetitionData() {
}
