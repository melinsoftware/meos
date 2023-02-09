/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
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
#include "machinecontainer.h"

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
    if (ctrl<1)
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
      gdi.setText("URL", L"http://roc.olresultat.se/getpunches.asp");      
    }
    else {
      gdi.check("UseUnitId", false);
      gdi.setTextTranslate("CmpID_label", L"Tävlingens ID-nummer:", true);
      useUnitId = false;
    }
    gdi.setInputStatus("UseUnitId", useROCProtocol);
  }
  else if (bi.id == "UseUnitId") {
    useUnitId = gdi.isChecked(bi.id);
    if (useUnitId)
      gdi.setTextTranslate("CmpID_label", L"Enhetens ID-nummer (MAC):", true);
    else
      gdi.setTextTranslate("CmpID_label", L"Tävlingens ID-nummer:", true);
  }

  return 0;
}

void OnlineInput::fillMappings(gdioutput &gdi) const{
  gdi.clearList("Mappings");
  for (map<int, oPunch::SpecialPunch>::const_iterator it = specialPunches.begin(); it != specialPunches.end(); ++it) {
    gdi.addItem("Mappings", itow(it->first) + L" \u21A6 " + oPunch::getType(it->second), it->first);
  }
}

void OnlineInput::settings(gdioutput &gdi, oEvent &oe, State state) {
  int iv = interval;
  if (state == State::Create) {
    iv = 10;
    url = oe.getPropertyString("MIPURL", L"");
  }

  wstring time;
  if (iv>0)
    time = itow(iv);

  settingsTitle(gdi, "Inmatning online");
  startCancelInterval(gdi, "Save", state, IntervalSecond, time);

  gdi.addInput("URL", url, 40, 0, L"URL:", L"Till exempel X#http://www.input.org/online.php");
  gdi.addCheckbox("UseROC", "Använd ROC-protokoll", OnlineCB, useROCProtocol).setExtra(getId());
  gdi.addCheckbox("UseUnitId", "Använd enhets-id istället för tävlings-id", OnlineCB, useROCProtocol && useUnitId).setExtra(getId());
  gdi.setInputStatus("UseUnitId", useROCProtocol);
  gdi.addInput("CmpID", itow(cmpId), 10, 0, L"Tävlingens ID-nummer:");

  gdi.dropLine(1);
  gdi.addString("", boldText, "Kontrollmappning");
  gdi.dropLine(0.5);

  controlMappingView(gdi, OnlineCB, getId());
  fillMappings(gdi);

  gdi.setCY(gdi.getHeight());
  gdi.popX();
  gdi.addString("", 10, "help:onlineinput");
}

void OnlineInput::controlMappingView(gdioutput& gdi, GUICALLBACK cb, int widgetId)
{
  gdi.fillRight();
  gdi.addInput("Code", L"", 4, 0, L"Kod:");
  gdi.addSelection("Function", 80, 200, 0, L"Funktion:");
  gdi.addItem("Function", lang.tl("Mål"), oPunch::PunchFinish);
  gdi.addItem("Function", lang.tl("Start"), oPunch::PunchStart);
  gdi.addItem("Function", lang.tl("Check"), oPunch::PunchCheck);
  gdi.dropLine();
  gdi.addButton("SaveMapping", "Lägg till", cb).setExtra(widgetId);
  gdi.popX();
  gdi.dropLine(2);
  gdi.addListBox("Mappings", 150, 150, 0, L"Definierade mappningar:", L"", true);
  gdi.dropLine();
  gdi.addButton("RemoveMapping", "Ta bort", cb).setExtra(widgetId);
}

void OnlineInput::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  int iv=gdi.getTextNo("Interval");
  const wstring &xurl=gdi.getText("URL");

  if (!xurl.empty())
    oe.setProperty("MIPURL", xurl);

  cmpId = gdi.getTextNo("CmpID");
  unitId = gdi.getText("CmpID");

  if (xurl.empty()) {
    throw meosException("URL måste anges.");
  }
  url = xurl;

  if (doProcess) {
    process(gdi, &oe, SyncNone);
    interval = iv;
  }
}

void OnlineInput::status(gdioutput &gdi)
{
  AutoMachine::status(gdi);
  gdi.fillRight();
  
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

void OnlineInput::saveMachine(oEvent &oe, const wstring &guiInterval) {
  auto &cnt = oe.getMachineContainer().set(getTypeString(), getMachineName());
 
  cnt.set("url", url);
  cnt.set("cmpId", cmpId);
  cnt.set("unitId", unitId);
  cnt.set("ROC", useROCProtocol);
  cnt.set("useId", useUnitId);

  int iv = _wtoi(guiInterval.c_str());
  cnt.set("interval", iv);

  vector<int> pm;
  for (auto &v : specialPunches) {
    pm.push_back(v.first);
    pm.push_back(v.second);
  }
  cnt.set("map", pm);
}

void OnlineInput::loadMachine(oEvent &oe, const wstring &name) {
  auto *cnt = oe.getMachineContainer().get(getTypeString(), name);
  if (!cnt)
    return;
  AutoMachine::loadMachine(oe, name);
  url = cnt->getString("url");
  cmpId = cnt->getInt("cmpId");
  unitId = cnt->getString("unitId");
  
  useROCProtocol = cnt->getInt("ROC") != 0;
  useUnitId = cnt->getInt("useId") != 0;
  interval = cnt->getInt("interval");

  specialPunches.clear();
  vector<int> pm = cnt->getVectorInt("map");
  for (size_t j = 0; j + 1 < pm.size(); j+=2) {
    specialPunches[pm[j]] = oPunch::SpecialPunch(pm[j + 1]);
  }
}

void OnlineInput::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {
  oe->autoSynchronizeLists(true);

  try {
    Download dwl;
    dwl.initInternet();
    ProgressWindow pw(0);
    vector<pair<wstring,wstring> > key;
    wstring q;
    if (useROCProtocol) {
      if (!useUnitId)
        q = L"?unitId=" + itow(cmpId) + L"&lastId=" + itow(lastImportedId) + L"&date=" + oe->getDate() + L"&time=" + oe->getZeroTime();
      else
        q = L"?unitId=" + unitId + L"&lastId=" + itow(lastImportedId) + L"&date=" + oe->getDate() + L"&time=" + oe->getZeroTime();
    }
    else {
      pair<wstring, wstring> mk1(L"competition", itow(cmpId));
      key.push_back(mk1);
	    pair<wstring, wstring> mk2(L"lastid", itow(lastImportedId));
      key.push_back(mk2);
    }
    wstring result = getTempFile();
    dwl.downloadFile(url + q, result, key);
    dwl.downLoadNoThread();

      if (!useROCProtocol) {
      xmlobject res;
      xmlparser xml;
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

      xmlList cards;
      res.getObjects("card", cards);
      processCards(gdi, *oe, cards);

      xmlList punches;
      res.getObjects("p", punches);
      processPunches(*oe, punches);

      xmlList teamlineup;
      res.getObjects("team", teamlineup);
      processTeamLineups(*oe, teamlineup);

      lastImportedId = res.getAttrib("lastid").getInt();
    }
    else {
      csvparser csv;
      list< vector<wstring> > rocData;
      csv.parse(result, rocData);
      processPunches(*oe, rocData);
    }

    struct _stat st;
    _wstat(result.c_str(), &st);
    bytesImported += st.st_size;
    removeTempFile(result);
  }
  catch (meosException &ex) {
    if (ast == SyncNone)
      throw;
    else
      gdi.addInfoBox("", wstring(L"Online Input Error X#") + ex.wwhat(), 5000);
  }
  catch(std::exception &ex) {
    if (ast == SyncNone)
      throw;
    else
      gdi.addInfoBox("", wstring(L"Online Input Error X#")+gdi.widen(ex.what()), 5000);
  }
  importCounter++;
}

void OnlineInput::processPunches(oEvent &oe, const xmlList &punches) {
  for (size_t k = 0; k < punches.size(); k++) {
    int code = punches[k].getObjectInt("code");
    wstring startno;
    punches[k].getObjectString("sno", startno);

    int originalCode = code;
    if (specialPunches.count(code))
      code = specialPunches[code];

    pRunner r = 0;

    int card = punches[k].getObjectInt("card");
    int time = punches[k].getObjectInt("time") / (10 / timeConstSecond);
    time = oe.getRelativeTime(formatTimeHMS(time));

    if (startno.length() > 0)
      r = oe.getRunnerByBibOrStartNo(startno, false);
    else
      r = oe.getRunnerByCardNo(card, time, oEvent::CardLookupProperty::Any);

    wstring rname;
    if (r) {
      rname = r->getName();
      card = r->getCardNo();
    }
    else {
      rname=lang.tl("Okänd");
    }
    if (time < 0) {
      time = 0;
      addInfo(L"Ogiltig tid");
    }
    oe.addFreePunch(time, code, originalCode, card, true);

    addInfo(L"Löpare: X, kontroll: Y, kl Z#" + rname + L"#" + oPunch::getType(code) + L"#" +  oe.getAbsTime(time));
  }
}

void OnlineInput::processPunches(oEvent &oe, list< vector<wstring> > &rocData) {
  for (list< vector<wstring> >::iterator it = rocData.begin(); it != rocData.end(); ++it) {
    vector<wstring> &line = *it;
    if (line.size() == 4) {
      int punchId = _wtoi(line[0].c_str());
      int code = _wtoi(line[1].c_str());
      int card = _wtoi(line[2].c_str());
      wstring timeS = line[3].substr(11);
      int time = oe.getRelativeTime(timeS);

      int originalCode = code;
      if (specialPunches.count(code))
        code = specialPunches[code];

      pRunner r = oe.getRunnerByCardNo(card, time, oEvent::CardLookupProperty::Any);

      wstring rname;
      if (r) {
        rname = r->getName();
        card = r->getCardNo();
      }
      else {
        rname=lang.tl("Okänd");
      }

      if (time < 0) {
        time = 0;
        addInfo(L"Ogiltig tid");
      }
      oe.addFreePunch(time, code, originalCode, card, true);

      lastImportedId = max(lastImportedId, punchId);

      addInfo(L"Löpare: X, kontroll: Y, kl Z#" + rname + L"#" + oPunch::getType(code) + L"#" + oe.getAbsTime(time));
    }
    else
      throw meosException("Onlineservern svarade felaktigt.");
  }
}

void OnlineInput::processCards(gdioutput &gdi, oEvent &oe, const xmlList &cards) {
  for (size_t k = 0; k < cards.size(); k++) {
    SICard sic(ConvertedTimeStatus::Hour24);
    sic.CardNumber = cards[k].getObjectInt("number");
    if (cards[k].getObject("finish"))
      sic.FinishPunch.Time = cards[k].getObject("finish").getObjectInt("time") / (10 / timeConstSecond);
    if (cards[k].getObject("start"))
      sic.StartPunch.Time = cards[k].getObject("start").getObjectInt("time") / (10 / timeConstSecond);
    xmlList punches;
    cards[k].getObjects("p", punches);
    for (size_t j = 0; j < punches.size(); j++) {
      sic.Punch[j].Code = punches[j].getObjectInt("code");
      sic.Punch[j].Time = punches[j].getObjectInt("time") / (10 / timeConstSecond);
    }
    sic.nPunch = punches.size();
    TabSI::getSI(gdi).addCard(sic);
  }
}

void OnlineInput::processTeamLineups(oEvent &oe, const xmlList &updates) {
}

void OnlineInput::processEntries(oEvent &oe, const xmlList &entries) {
  
  map<int, pRunner> raceId2R;
  vector<pRunner> runners;
  oe.getRunners(0, 0, runners);
  for (auto &r : runners) {
    int stored = r->getDCI().getInt("RaceId");
    if (stored > 0 && (stored & (1 << 30))) {
      int v = (stored & ~(1 << 30));
      raceId2R[v] = r;
    }
  }

  for (auto &entry : entries) {
    pClass cls = nullptr;

    int classId = entry.getObjectInt("classid");
    if (classId > 0)
      cls = oe.getClass(classId);

    if (!cls) {
      wstring clsName;
      entry.getObjectString("classname", clsName);
      cls = oe.getClass(clsName);
    }
    if (!cls)
      continue;

    int fee = entry.getObjectInt("fee");
    bool paid = entry.getObjectBool("paid");

    xmlobject xname = entry.getObject("name");
    wstring birthyear = 0;
    if (xname) {
      xname.getObjectString("birthyear", birthyear);
    }

    wstring name;
    entry.getObjectString("name", name);
    if (name.empty())
      continue;

    wstring club;
    entry.getObjectString("club", club);

    int cardNo = 0;
    bool hiredCard = false;

    xmlobject card = entry.getObject("card");
    if (card) {
      cardNo = card.getInt();
      hiredCard = card.getObjectBool("hired");
    }

    if (!hiredCard && oe.hasHiredCardData())
      hiredCard = oe.isHiredCard(cardNo);

    pRunner r = nullptr;
    int id = entry.getObjectInt("id");
    if (id > 0) {
      auto res = raceId2R.find(id);
      if (res != raceId2R.end()) {
        r = res->second;
      }
    }
    else {
      if (cardNo != 0) {
        r = oe.getRunnerByCardNo(cardNo, 0, oEvent::CardLookupProperty::Any);
        if (r && !r->matchName(name))
          r = nullptr;
      }
      if (r == nullptr) {
        r = oe.getRunnerByName(name, club);
        if (r && r->getCardNo() != cardNo)
          r = nullptr;
      }
    }
    if (r == nullptr)
      r = oe.addRunner(name, club, cls->getId(), cardNo, birthyear, true);
    else {
      r->setName(name, false);
      r->setClub(club);
      r->setBirthDate(birthyear);
      r->setCardNo(cardNo, false, false);
      r->setClassId(cls->getId(), true);
    }

    if (fee == 0)
      fee = r->getDefaultFee();

    r->getDI().setInt("Fee", fee);
    int toPay = fee;
    int cf = 0;
    if (hiredCard) {
      cf = r->getEvent()->getBaseCardFee();
      if (cf > 0)
        toPay += cf;
    }
    r->setFlag(oRunner::FlagAddedViaAPI, true);
    r->getDI().setInt("CardFee", cf);
    r->getDI().setInt("Paid", paid ? toPay : 0);

    if (id > 0) {
      r->getDI().setInt("RaceId", id | (1 << 30));
      raceId2R[id] = r;
    }
    r->synchronize(true);
  }
}
