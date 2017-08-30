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
#include <sys/stat.h>

#include "oEvent.h"
#include "gdioutput.h"

#include "onlineinput.h"
#include "meos_util.h"
#include <shellapi.h>

#include "gdiconstants.h"
#include "meosException.h"
#include "Download.h"
#include "xmlparser.h"
#include "progress.h"
#include "csvparser.h"

#include "SportIdent.h"
#include "TabSI.h"

int AutomaticCB(gdioutput *gdi, int type, void *data);

static int OnlineCB(gdioutput *gdi, int type, void *data) {
  switch (type) {
    case GUI_BUTTON: {
      //Make a copy
      ButtonInfo bu=*static_cast<ButtonInfo *>(data);
      OnlineInput &ores = dynamic_cast<OnlineInput &>(*AutoMachine::getMachine(bu.getExtraInt()));
      return ores.processButton(*gdi, bu);
    }
    case GUI_LISTBOX:{
    }
  }
  return 0;
}

OnlineInput::~OnlineInput() {
}

int OnlineInput::processButton(gdioutput &gdi, ButtonInfo &bi) {
  if (bi.id == "SaveMapping") {
    int ctrl = gdi.getTextNo("Code");
    if (ctrl<10)
      throw meosException("Ogiltig kontrollkod");
    ListBoxInfo lbi;
    if (!gdi.getSelectedItem("Function", lbi))
      throw meosException("Ogiltig funktion");
    specialPunches[ctrl] = (oPunch::SpecialPunch)lbi.data;
    fillMappings(gdi);
  }
  else if (bi.id == "RemoveMapping") {
    set<int> sel;
    gdi.getSelection("Mappings", sel);
    for (set<int>::iterator it = sel.begin(); it != sel.end(); ++it) {
      specialPunches.erase(*it);
    }
    fillMappings(gdi);
  }
  else if (bi.id == "UseROC") {
    useROCProtocol = gdi.isChecked(bi.id);
    if (useROCProtocol) {
      gdi.setText("URL", "http://roc.olresultat.se/getpunches.asp");      
    }
    else {
      gdi.check("UseUnitId", false);
      gdi.setTextTranslate("CmpID_label", "Tävlingens ID-nummer:", true);
      useUnitId = false;
    }
    gdi.setInputStatus("UseUnitId", useROCProtocol);
  }
  else if (bi.id == "UseUnitId") {
    useUnitId = gdi.isChecked(bi.id);
    if (useUnitId)
      gdi.setTextTranslate("CmpID_label", "Enhetens ID-nummer (MAC):", true);
    else
      gdi.setTextTranslate("CmpID_label", "Tävlingens ID-nummer:", true);
  }

  return 0;
}

void OnlineInput::fillMappings(gdioutput &gdi) const{
  gdi.clearList("Mappings");
  for (map<int, oPunch::SpecialPunch>::const_iterator it = specialPunches.begin(); it != specialPunches.end(); ++it) {
    gdi.addItem("Mappings", itos(it->first) + " -> " + oPunch::getType(it->second), it->first);
  }
}


void OnlineInput::settings(gdioutput &gdi, oEvent &oe, bool created) {
  int iv = interval;
  if (created) {
    iv = 10;
    url = oe.getPropertyString("MIPURL", "");
  }

  string time;
  if (iv>0)
    time = itos(iv);

  settingsTitle(gdi, "Inmatning online");
  startCancelInterval(gdi, "Save", created, IntervalSecond, time);

  gdi.addInput("URL", url, 40, 0, "URL:", "Till exempel X#http://www.input.org/online.php");
  gdi.addCheckbox("UseROC", "Använd ROC-protokoll", OnlineCB, useROCProtocol).setExtra(getId());
  gdi.addCheckbox("UseUnitId", "Använd enhets-id istället för tävlings-id", OnlineCB, useROCProtocol & useUnitId).setExtra(getId());
  gdi.setInputStatus("UseUnitId", useROCProtocol);
  gdi.addInput("CmpID", itos(cmpId), 10, 0, "Tävlingens ID-nummer:");

  gdi.dropLine(1);

  gdi.addString("", boldText, "Kontrollmappning");
  gdi.dropLine(0.5);
  gdi.fillRight();
  gdi.addInput("Code", "", 4, 0, "Kod:");
  gdi.addSelection("Function", 80, 200, 0, "Funktion:");
  gdi.addItem("Function", lang.tl("Mål"), oPunch::PunchFinish);
  gdi.addItem("Function", lang.tl("Start"), oPunch::PunchStart);
  gdi.addItem("Function", lang.tl("Check"), oPunch::PunchCheck);
  gdi.dropLine();
  gdi.addButton("SaveMapping", "Lägg till", OnlineCB).setExtra(getId());
  gdi.popX();
  gdi.dropLine(2);
  gdi.addListBox("Mappings", 150, 100, 0, "Definierade mappningar:", "", true);
  gdi.dropLine();
  gdi.addButton("RemoveMapping", "Ta bort", OnlineCB).setExtra(getId());
  fillMappings(gdi);

  gdi.setCY(gdi.getHeight());
  gdi.popX();
  gdi.addString("", 10, "help:onlineinput");
}

void OnlineInput::save(oEvent &oe, gdioutput &gdi) {
  int iv=gdi.getTextNo("Interval");
  const string &xurl=gdi.getText("URL");

  if (!xurl.empty())
    oe.setProperty("MIPURL", xurl);

  cmpId = gdi.getTextNo("CmpID");
  unitId = gdi.getText("CmpID");

  if (xurl.empty()) {
    throw meosException("URL måste anges.");
  }
  url = xurl;

  process(gdi, &oe, SyncNone);
  interval = iv;
}

void OnlineInput::status(gdioutput &gdi)
{
  gdi.addString("", 1, name);
  gdi.fillRight();
  gdi.pushX();

  gdi.addString("", 0, "URL:");
  gdi.addStringUT(0, url);
  gdi.popX();
  gdi.dropLine(1);

  gdi.addString("", 0, "Antal hämtade uppdateringar X (Y kb)#" +
                        itos(importCounter-1) + "#" + itos(bytesImported/1024));
  gdi.popX();
  gdi.fillDown();
  gdi.dropLine(2);

  for (size_t k = 0; k < info.size(); k++) {
    gdi.addString("", 0, info[k]);
  }

  gdi.fillRight();
  gdi.dropLine(1);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("OnlineInput", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void OnlineInput::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  oe->autoSynchronizeLists(true);

  try {
    Download dwl;
    dwl.initInternet();
    ProgressWindow pw(0);
    vector<pair<string,string> > key;
    string q;
    if (useROCProtocol) {
      if (!useUnitId)
        q = "?unitId=" + itos(cmpId) + "&lastId=" + itos(lastImportedId) + "&date=" + oe->getDate() +"&time=" + oe->getZeroTime();
      else
        q = "?unitId=" + unitId + "&lastId=" + itos(lastImportedId) + "&date=" + oe->getDate() +"&time=" + oe->getZeroTime();
    }
    else {
      pair<string, string> mk1("competition", itos(cmpId));
      key.push_back(mk1);
	  pair<string, string> mk2("lastid", itos(lastImportedId));
      key.push_back(mk2);
    }
    string result = getTempFile();
    dwl.downloadFile(url + q, result, key);
    dwl.downLoadNoThread();

    if (!useROCProtocol) {
      xmlobject res;
      xmlparser xml(0);
      try {
        xml.read(result);
        res = xml.getObject("MIPData");
      }
      catch(std::exception &) {
        throw meosException("Onlineservern svarade felaktigt.");
      }

      xmlList entries;
      res.getObjects("entry", entries);
      processEntries(*oe, entries);

      xmlList updates;
      res.getObjects("update", updates);
      processUpdates(*oe, updates);

      xmlList cards;
      res.getObjects("card", cards);
      processCards(gdi, *oe, cards);

      xmlList punches;
      res.getObjects("p", punches);
      processPunches(*oe, punches);

      lastImportedId = res.getAttrib("lastid").getInt();
    }
    else {
      csvparser csv;
      list< vector<string> > rocData;
      csv.parse(result, rocData);
      processPunches(*oe, rocData);
    }

    struct stat st;
    stat(result.c_str(), &st);
    bytesImported += st.st_size;
    removeTempFile(result);
  }
  catch(std::exception &ex) {
    if (ast == SyncNone)
      throw;
    else
      gdi.addInfoBox("", string("Online Input Error X#")+ex.what(), 5000);
  }
  importCounter++;
}

void OnlineInput::processPunches(oEvent &oe, const xmlList &punches) {
  for (size_t k = 0; k < punches.size(); k++) {
    int code = punches[k].getObjectInt("code");
    string startno;
    punches[k].getObjectString("sno", startno);

    if (specialPunches.count(code))
      code = specialPunches[code];

    pRunner r = 0;

    int card = punches[k].getObjectInt("card");
    int time = punches[k].getObjectInt("time") / 10;
    time = oe.getRelativeTime(formatTimeHMS(time));

    if (startno.length() > 0)
      r = oe.getRunnerByBibOrStartNo(startno, false);
    else
      r = oe.getRunnerByCardNo(card, time);

    string rname;
    if (r) {
      rname = r->getName();
      card = r->getCardNo();
    }
    else {
      rname=lang.tl("Okänd");
    }
    if (time < 0) {
      time = 0;
      addInfo("Ogiltig tid");
    }
    oe.addFreePunch(time, code, card, true);

    addInfo("Löpare: X, kontroll: Y, kl Z#" + rname + "#" + oPunch::getType(code) + "#" +  oe.getAbsTime(time));
  }
}

void OnlineInput::processPunches(oEvent &oe, list< vector<string> > &rocData) {
  for (list< vector<string> >::iterator it = rocData.begin(); it != rocData.end(); ++it) {
    vector<string> &line = *it;
    if (line.size() == 4) {
      int punchId = atoi(line[0].c_str());
      int code = atoi(line[1].c_str());
      int card = atoi(line[2].c_str());
      string timeS = line[3].substr(11);
      int time = oe.getRelativeTime(timeS);

      if (specialPunches.count(code))
        code = specialPunches[code];

      pRunner r = oe.getRunnerByCardNo(card, time);

      string rname;
      if (r) {
        rname = r->getName();
        card = r->getCardNo();
      }
      else {
        rname=lang.tl("Okänd");
      }

      if (time < 0) {
        time = 0;
        addInfo("Ogiltig tid");
      }
      oe.addFreePunch(time, code, card, true);

      lastImportedId = max(lastImportedId, punchId);

      addInfo("Löpare: X, kontroll: Y, kl Z#" + rname + "#" + oPunch::getType(code) + "#" +  oe.getAbsTime(time));
    }
    else
      throw meosException("Onlineservern svarade felaktigt.");
  }
}

void OnlineInput::processCards(gdioutput &gdi, oEvent &oe, const xmlList &cards) {
  for (size_t k = 0; k < cards.size(); k++) {
    SICard sic;
    sic.clear(0);
    sic.CardNumber = cards[k].getObjectInt("number");
    if (cards[k].getObject("finish"))
      sic.FinishPunch.Time = cards[k].getObject("finish").getObjectInt("time") / 10;
    if (cards[k].getObject("start"))
      sic.StartPunch.Time = cards[k].getObject("start").getObjectInt("time") / 10;
    xmlList punches;
    cards[k].getObjects("p", punches);
    for (size_t j = 0; j < punches.size(); j++) {
      sic.Punch[j].Code = punches[j].getObjectInt("code");
      sic.Punch[j].Time = punches[j].getObjectInt("time") / 10;
    }
    sic.nPunch = punches.size();
    TabSI::getSI(gdi).addCard(sic);
  }
}

void OnlineInput::processUpdates(oEvent &oe, const xmlList &updates) {
}

void OnlineInput::processEntries(oEvent &oe, const xmlList &entries) {
}

