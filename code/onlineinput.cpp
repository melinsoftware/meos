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

#include <sys/stat.h>
#include "oEvent.h"
#include "gdioutput.h"

#include "onlineinput.h"
#include "meos_util.h"

#include "meosException.h"
#include "Download.h"
#include "xmlparser.h"
#include "progress.h"
#include "csvparser.h"
#include "machinecontainer.h"

#include "SportIdent.h"
#include "TabSI.h"

int AutomaticCB(gdioutput *gdi, GuiEventType type, BaseInfo* data);

static int OnlineCB(gdioutput *gdi, GuiEventType type, BaseInfo* data) {
  switch (type) {
    case GUI_BUTTON: {
      //Make a copy
      ButtonInfo bu = dynamic_cast<ButtonInfo &>(*data);
      OnlineInput &ores = dynamic_cast<OnlineInput &>(*AutoMachine::getMachine(bu.getExtraInt()));
      return ores.processButton(*gdi, bu);
    }
    case GUI_LISTBOX:{
      ListBoxInfo lbi = dynamic_cast<ListBoxInfo&>(*data);
      OnlineInput& ores = dynamic_cast<OnlineInput&>(*AutoMachine::getMachine(lbi.getExtraInt()));
      
      return ores.processListBox(*gdi, lbi);
    }
  }
  return 0;
}

int OnlineInput::processListBox(gdioutput& gdi, ListBoxInfo& lbi) {
  if (lbi.id == "ServerType") {
    serverType = Type(lbi.data);
    if (serverType == Type::ROC)
      gdi.setText("URL", L"http://roc.olresultat.se/getpunches.asp");
    else if (serverType == Type::SICenter)
      gdi.setText("URL", L"https://center-origin.sportident.com/api/rest/v1/punches");
    else {
      gdi.check("UseUnitId", false);
      useUnitId = false;
      updateLabel(gdi);
    }

    gdi.setInputStatus("UseUnitId", serverType != Type::MIP);
  }
  return 0;
}

int OnlineInput::processButton(gdioutput &gdi, ButtonInfo &bi) {
  if (oe == nullptr)
    throw std::exception("Internal error");

  if (bi.id == "SaveMapping") {
    int ctrl = gdi.getTextNo("Code");
    if (ctrl<1 || ctrl>=1024)
      throw meosException("Ogiltig kontrollkod");
    ListBoxInfo lbi;
    if (!gdi.getSelectedItem("Function", lbi))
      throw meosException("Ogiltig funktion");
    oe->definePunchMapping(ctrl, oPunch::SpecialPunch(lbi.data));
    fillMappings(*oe, gdi);
    gdi.setInputStatus("LoadLastMapping", false);
  }
  else if (bi.id == "RemoveMapping") {
    set<int> sel;
    gdi.getSelection("Mappings", sel);
    for (auto it = sel.begin(); it != sel.end(); ++it) {
      oe->definePunchMapping(*it, oPunch::PunchUnused);
    }
    fillMappings(*oe, gdi);
    gdi.setInputStatus("LoadLastMapping", false);
  }
  else if (bi.id == "LoadLastMapping") {
    auto cm = oe->getPropertyString("ControlMap", L"");
    oe->getDI().setString("ControlMap", cm);
    fillMappings(*oe, gdi);
    gdi.setInputStatus(bi.id, false);
  } 
  else if (bi.id == "UseUnitId") {
    useUnitId = gdi.isChecked(bi.id);
    updateLabel(gdi);
  }

  return 0;
}

void OnlineInput::updateLabel(gdioutput& gdi) {
  if (useUnitId)
    gdi.setTextTranslate("CmpID_label", L"Enhetens ID-nummer (MAC):", true);
  else
    gdi.setTextTranslate("CmpID_label", L"Tävlingens ID-nummer:", true);
}

void OnlineInput::fillMappings(oEvent& oe, gdioutput &gdi) {
  specialPunches = oe.getPunchMapping();
  gdi.clearList("Mappings");
  for (auto &[code, val] : specialPunches) {
    gdi.addItem("Mappings", itow(code) + L" \u21A6 " + oPunch::getType(val, nullptr), code);
  }
}

void OnlineInput::settings(gdioutput &gdi, oEvent &oe, State state) {
  this->oe = &oe;

  int iv = interval;
  if (state == State::Create) {
    iv = 10;
    url = oe.getPropertyString("MIPURL", L"");
    int st = oe.getPropertyInt("InputServer", 0);
    switch (st) {
    case 0:
      serverType = Type::MIP;
      break;
    case 1:
      serverType = Type::ROC;
      break;
    case 2:
      serverType = Type::SICenter;
      break;
    }
  }

  wstring time;
  if (iv>0)
    time = itow(iv);

  settingsTitle(gdi, "Inmatning online");
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalSecond, time);

  gdi.addInput("URL", url, 40, 0, L"URL:", L"Till exempel X#https://www.input.org/online.php");

  gdi.addSelection("ServerType", 200, 100, OnlineCB, L"Typ:").setExtra(getId());
  gdi.addItem("ServerType", L"MeOS Input Protocol (MIP)", int(Type::MIP));
  gdi.addItem("ServerType", L"Radio Online Control (ROC)", int(Type::ROC));
  gdi.addItem("ServerType", L"SportIdent Center (SI)", int(Type::SICenter));
  gdi.selectItemByData("ServerType", int(serverType));
  gdi.dropLine(0.5);
  gdi.addCheckbox("UseUnitId", "Använd enhets-id istället för tävlings-id", OnlineCB, serverType != Type::MIP && useUnitId).setExtra(getId());
  gdi.setInputStatus("UseUnitId", serverType != Type::MIP);

  if (serverType != Type::MIP && useUnitId)
    gdi.addInput("CmpID", unitId, 20, 0, L"Enhetens ID-nummer (MAC):");
  else
    gdi.addInput("CmpID", itow(cmpId), 20, 0, L"Tävlingens ID-nummer:");

  gdi.dropLine(1);

  int dateNow = convertDateYMD(getLocalDate(), false);
  int cmpDate = convertDateYMD(oe.getDate(), false);
  int zt = oe.getZeroTimeNum() / timeConstMinute;
  int localTime = convertAbsoluteTimeHMS(getLocalTimeOnly(), 0) / timeConstMinute;

  uint64_t tNow = uint64_t(dateNow) * 10000ul + localTime;
  uint64_t tZero = uint64_t(cmpDate) * 10000ul + zt;
  if (tNow < tZero) {
    gdi.pushX();
    gdi.fillRight();
    gdi.addString("warningicon", textImage, "516");
    gdi.dropLine(-0.2);
    gdi.fillDown();
    gdi.addString("cmpwarning", 0, "Observera att stämplingar före tävlingens nolltid inte kan hämtas.");
    gdi.addString("", italicText, "För kommunikationstest kan man använda en separat testtävling");
    gdi.dropLine(0.5);
    gdi.popX();
  }

  controlMappingView(gdi, &oe, OnlineCB, getId());
  fillMappings(oe, gdi);

  gdi.setCY(gdi.getHeight());
  gdi.popX();
  gdi.addString("", 10, "help:onlineinput");
}

void OnlineInput::controlMappingView(gdioutput& gdi, oEvent *oe, GUICALLBACK cb, int widgetId) {
  gdi.setRestorePoint("ControlMapping");
  gdi.addString("", fontMediumPlus, "Kontrollmappning");
  gdi.dropLine(0.5);

  gdi.pushX();

  gdi.fillRight();
  gdi.addInput("Code", L"", 4, 0, L"Kod:");
  gdi.addSelection("Function", 80, 200, 0, L"Funktion:");
  gdi.addItem("Function", lang.tl("Mål"), oPunch::PunchFinish);
  gdi.addItem("Function", lang.tl("Start"), oPunch::PunchStart);
  gdi.addItem("Function", lang.tl("Check"), oPunch::PunchCheck);
  gdi.dropLine();
  gdi.addButton("SaveMapping", "Lägg till", cb).setExtra(widgetId);
  
  class HelpHandler : public GuiHandler {
  public:
    int xpos = 0;
    void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
      ButtonInfo bi = dynamic_cast<ButtonInfo&>(info);
      gdi.removeWidget("Help");
      gdi.addString("", bi.getY(), xpos, 10, "info:mapcontrol");
      gdi.refresh();
    };
  };

  auto hh = make_shared<HelpHandler>();

  gdi.addButton("Help", "Hjälp").setHandler(hh);
  
  gdi.popX();
  gdi.dropLine(2);
  gdi.addListBox("Mappings", 150, 150, 0, L"Definierade mappningar:", L"", true);
  gdi.dropLine();
  gdi.fillDown();
  gdi.addButton("RemoveMapping", "Ta bort", cb).setExtra(widgetId);

  auto cm = oe->getPropertyString("ControlMap", L"");
  gdi.addButton("LoadLastMapping", "Hämta föregående", cb, "Ladda de inställningar som senast gjordes på den här datorn.").setExtra(widgetId);
  if (cm.empty())
    gdi.disableInput("LoadLastMapping");

  RECT rc = gdi.getDimensionSince("ControlMapping");

  hh->xpos = rc.right + gdi.scaleLength(20);
}

void OnlineInput::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  int iv=gdi.getTextNo("Interval");
  const wstring &xurl=gdi.getText("URL");

  if (!xurl.empty()) {
    oe.setProperty("MIPURL", xurl);
    oe.setProperty("InputServer", int(serverType));
  }

  cmpId = gdi.getTextNo("CmpID");
  unitId = gdi.getText("CmpID");

  if (useUnitId && serverType == Type::SICenter) {
    vector<wstring> out;
    split(unitId, L" ,;", out);
    wstring modems;
    for (auto& m : out) {
      int mId = _wtoi(m.c_str());
      if (mId > 0) {
        if (!modems.empty())
          modems += L",";
        modems += itow(mId);
      }
    } 
    if (modems.empty())
      throw meosException("Invalid unit ID");
    unitId = modems;
  }

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
  AutoMachine::saveMachine(oe, guiInterval);
  auto &cnt = oe.getMachineContainer().set(getTypeString(), getMachineName());
 
  cnt.set("url", url);
  cnt.set("cmpId", cmpId);
  cnt.set("unitId", unitId);
  cnt.set("ServerType", int(serverType));
  
  cnt.set("useId", useUnitId);

  int iv = _wtoi(guiInterval.c_str());
  cnt.set("interval", iv);

  /*vector<int> pm;
  for (auto &v : specialPunches) {
    pm.push_back(v.first);
    pm.push_back(v.second);
  }
  cnt.set("map", pm);*/
}

void OnlineInput::loadMachine(oEvent& oe, const wstring& name) {
  auto* cnt = oe.getMachineContainer().get(getTypeString(), name);
  if (!cnt)
    return;
  AutoMachine::loadMachine(oe, name);
  this->oe = &oe;
  url = cnt->getString("url");
  cmpId = cnt->getInt("cmpId");
  unitId = cnt->getString("unitId");

  serverType = Type::MIP;
  if (cnt->getInt("ROC") != 0)
    serverType = Type::ROC; // MeOS 4.0
  else {
    switch (cnt->getInt("ServerType")) {
    case 0:
      serverType = Type::MIP;
      break;
    case 1:
      serverType = Type::ROC;
      break;
    case 2:
      serverType = Type::SICenter;
      break;
    }
  }
  
  useUnitId = cnt->getInt("useId") != 0;
  interval = cnt->getInt("interval");

  if (cnt->has("map")) {
    vector<int> pm = cnt->getVectorInt("map");
    for (size_t j = 0; j + 1 < pm.size(); j += 2) {
      oe.definePunchMapping(pm[j], oPunch::SpecialPunch(pm[j + 1])); // Load from v. 4.0 and earlier
    }
  }
}

void OnlineInput::process(gdioutput& gdi, oEvent* oe, AutoSyncType ast) {
  processProtected(gdi, ast, [&]() {
    oe->autoSynchronizeLists(true);

    Download dwl;
    dwl.initInternet();
    ProgressWindow pw(nullptr, gdi.getScale());
    vector<pair<wstring, wstring> > key;
    wstring q;
    if (serverType == Type::SICenter) {
      // SportIdent Center uses milliseconds since linux Epoch 1970-01-01
      if (!useUnitId)
        q = L"?eventId=" + itow(cmpId) + L"&afterId=" + itow(lastImportedId) + L"&after=" + std::to_wstring(getZeroTimeMSLinuxEpoch());
      else
        q = L"?modem=" + unitId + L"&afterId=" + itow(lastImportedId) + L"&after=" + std::to_wstring(getZeroTimeMSLinuxEpoch());

      pair<wstring, wstring> mk1(L"Accept", L"text/csv");
      key.push_back(mk1);
    }
    else if (serverType == Type::ROC) {
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
    try {
      dwl.downloadFile(url + q, result, key);
      dwl.downLoadNoThread();

      if (serverType == Type::ROC) {
        csvparser csv;
        list<vector<wstring>> rocData;
        csv.parse(result, rocData);
        processPunches(*oe, rocData);
      }
      else if (serverType == Type::SICenter) {
        // we can't use csv.parse as it expects semi-colon as separator
        processPunchesSICenter(*oe, result);
      }
      else {
        xmlobject res;
        xmlparser xml;
        try {
          xml.read(result);
          res = xml.getObject("MIPData");
        }
        catch (std::exception&) {
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

      struct _stat st;
      _wstat(result.c_str(), &st);
      bytesImported += st.st_size;
      removeTempFile(result);
    }
    catch (...) {
      removeTempFile(result);
      throw;
    }

    importCounter++;
  });
}

void OnlineInput::processPunches(oEvent &oe, const xmlList &punches) {
  for (size_t k = 0; k < punches.size(); k++) {
    int code = punches[k].getObjectInt("code");
    wstring startno, type;
    punches[k].getObjectString("sno", startno);
    punches[k].getObjectString("type", type);

    int originalCode = code;
    if (type == L"start") {
      code = oPunch::SpecialPunch::PunchStart;
    }
    else if (type == L"finish") {
      code = oPunch::SpecialPunch::PunchFinish;
    }
    else if (type == L"check") {
      code = oPunch::SpecialPunch::PunchCheck;
    }
    else {
      code = mapPunch(code);
    }

    pRunner r = 0;

    int card = punches[k].getObjectInt("card");
    int time = punches[k].getObjectInt("time") / (10 / timeConstSecond);

    if (!oe.supportSubSeconds())
      time -= (time % timeConstSecond);

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

    if (code <= 0) {
      addInfo(L"Ogiltig kontrollkod");
      continue; 
    }

    if (card <= 0) {
      addInfo(L"Ogiltigt bricknummer");
      continue;
    }

    oe.addFreePunch(time, code, originalCode, card, true, true);

    addInfo(L"Löpare: X, kontroll: Y, kl Z#" + rname + L"#" + oPunch::getType(code, r ? r->getCourse(false) : nullptr) + L"#" +  oe.getAbsTime(time));
  }
}

void OnlineInput::processPunches(oEvent &oe, list<vector<wstring>> &rocData) {
  for (list< vector<wstring> >::iterator it = rocData.begin(); it != rocData.end(); ++it) {
    vector<wstring> &line = *it;
    if (line.size() == 4) {
      int punchId = _wtoi(line[0].c_str());
      int code = _wtoi(line[1].c_str());
      int card = _wtoi(line[2].c_str());
      wstring timeS = line[3].substr(11);
      int time = oe.getRelativeTime(timeS);
      if (!oe.supportSubSeconds())
        time -= (time % timeConstSecond);

      int originalCode = code;
      code = mapPunch(code);
      
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
      oe.addFreePunch(time, code, originalCode, card, true, true);

      lastImportedId = max(lastImportedId, punchId);

      addInfo(L"Löpare: X, kontroll: Y, kl Z#" + rname + L"#" + oPunch::getType(code, r ? r->getCourse(false) : nullptr) + L"#" + oe.getAbsTime(time));
    }
    else
      throw meosException("Onlineservern svarade felaktigt.");
  }
}

void OnlineInput::processPunchesSICenter(oEvent &oe, const wstring& filename) {

	time_t epoch_abs = getZeroTimeMSLinuxEpoch();

	std::wifstream file(filename);
	if (!file.is_open())
		return;

	wstring line;

	// Skip the header
	std::getline(file, line);

	while (std::getline(file, line)) {
		wstringstream ss(line);
		wstring field, type, cardstr;

		int punchId, code, card, time;

		std::getline(ss, field, L',');
	    punchId = std::stoi(field);

		std::getline(ss, cardstr, L',');
		card = std::stoi(cardstr);

		std::getline(ss, field, L',');
		time_t epoch = std::stoll(field);
        epoch -= epoch_abs; // in ms
        time = (int)(epoch / 100); // in tenth of seconds

		std::getline(ss, field, L',');
		code = std::stoi(field);

    int originalCode = code;
    std::getline(ss, type, L',');
    if (type == L"Start")
      code = oPunch::SpecialPunch::PunchStart;
    else if (type == L"Finish")
      code = oPunch::SpecialPunch::PunchFinish;
    else if (type == L"Check" || type == L"Clear")
      code = oPunch::SpecialPunch::PunchCheck;
    else
      code = mapPunch(code);

    if (!oe.supportSubSeconds())
      time -= (time % timeConstSecond);

		pRunner r = oe.getRunnerByCardNo(card, time, oEvent::CardLookupProperty::Any);

		wstring rname;
		if (r) {
			rname = r->getName();
			card = r->getCardNo();
		}
		else {
			rname = lang.tl("Okänd") + L" (" + cardstr + L")";
		}

		if (time < 0) {
			time = 0;
			addInfo(L"Ogiltig tid");
		}
		oe.addFreePunch(time, code, originalCode, card, true, true);

		lastImportedId = max(lastImportedId, punchId);

		addInfo(L"Löpare: X, kontroll: Y, kl Z#" + rname + L"#" + oPunch::getType(code, r ? r->getCourse(false) : nullptr) + L"#" + oe.getAbsTime(time));
	}

	file.close();
}

void OnlineInput::processCards(gdioutput &gdi, oEvent &oe, const xmlList &cards) {
  for (size_t k = 0; k < cards.size(); k++) {
    SICard sic(ConvertedTimeStatus::Hour24);
    sic.CardNumber = cards[k].getObjectInt("number");

    if (xmlobject fin = cards[k].getObject("finish"); fin) {
      sic.FinishPunch.Time = fin.getObjectInt("time") / (10 / timeConstSecond);
      sic.FinishPunch.Code = fin.getObjectInt("code");
    }

    if (xmlobject sta = cards[k].getObject("start"); sta) {
      sic.StartPunch.Time = sta.getObjectInt("time") / (10 / timeConstSecond);
      sic.StartPunch.Code = sta.getObjectInt("code");
    }

    if (xmlobject chk = cards[k].getObject("check"); chk) {
      sic.CheckPunch.Time = chk.getObjectInt("time") / (10 / timeConstSecond);
      sic.CheckPunch.Code = chk.getObjectInt("code");
    }

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

  wstring error;

  for (auto &entry : entries) {
    pClass cls = nullptr;

    int classId = entry.getObjectInt("classid");
    if (classId > 0)
      cls = oe.getClass(classId);

    wstring clsName;
    if (!cls) {
      entry.getObjectString("classname", clsName);
      cls = oe.getClass(clsName);
    }
    if (!cls) {
      if (error.empty()) {
        error = L"#" + lang.tl("Okänd klass: ");
        if (clsName.empty())
          error += L"?";
        else
          error += clsName;

        if (classId > 0)
          error += L", Id = " + itow(classId);   
      }
      continue;
    }

    int fee = entry.getObjectInt("fee");
    bool paid = entry.getObjectBool("paid");

    xmlobject xname = entry.getObject("name");
    wstring birthyear, sex, nat;
    if (xname) {
      xname.getObjectString("birthdate", birthyear);
      if (birthyear.empty()) {
        xname.getObjectString("birthyear", birthyear);
      }

      xname.getObjectString("sex", sex);
      xname.getObjectString("nationality", nat);
    }
    
    wstring name;
    entry.getObjectString("name", name);
    if (name.empty()) {
      name = oe.getAutoRunnerName();
    }

    wstring club;
    entry.getObjectString("club", club);

    wstring bib, phone, rank, text;
    entry.getObjectString("bib", bib);
    entry.getObjectString("phone", phone);
    entry.getObjectString("rank", rank);
    entry.getObjectString("text", text);
    int dataA = entry.getObjectInt("dataA");
    int dataB = entry.getObjectInt("dataB");
    bool noTiming = entry.getObjectBool("notiming");

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

    auto di = r->getDI();
    di.setInt("Fee", fee);
    int toPay = fee;
    int cf = 0;
    if (hiredCard) {
      cf = r->getEvent()->getBaseCardFee();
      if (cf > 0)
        toPay += cf;
    }
    r->setFlag(oRunner::FlagAddedViaAPI, true);
    di.setInt("CardFee", cf);
    di.setInt("Paid", paid ? toPay : 0);
    r->setExtraPersonData(sex, nat, rank, phone, bib, text, dataA, dataB);

    if (noTiming)
      r->setStatus(StatusNoTiming, true, oBase::ChangeType::Update, false);

    if (id > 0) {
      di.setInt("RaceId", id | (1 << 30));
      raceId2R[id] = r;
    }
    r->synchronize(true);
  }
  if (!error.empty())
    throw meosException(error);
}

time_t OnlineInput::getZeroTimeMSLinuxEpoch() const {
  SYSTEMTIME st;
  convertDateYMD(oe->getDate(), st, false);

  // Convert SYSTEMTIME to struct tm
  tm tm{};
  tm.tm_year = st.wYear - 1900;     // tm_year is years since 1900
  tm.tm_mon = st.wMonth - 1;        // tm_mon is 0-based
  tm.tm_mday = st.wDay;
  tm.tm_hour = st.wHour;
  tm.tm_min = st.wMinute;
  tm.tm_sec = st.wSecond;
  tm.tm_isdst = -1;

  // Convert to epoch (UTC)
  time_t epoch = _mkgmtime64(&tm) * 1000;  // mktime is in seconds
  epoch += (time_t)oe->getZeroTimeNum() * 100;    // we use tenth of a second
  return epoch;
}

int OnlineInput::mapPunch(int code) const {
  if (auto res = specialPunches.find(code); res != specialPunches.end())
    code = res->second;
  else if (code == oPunch::SpecialPunch::PunchStart)
    code = oPunch::SpecialPunch::PunchCheck; // Do not allow unmatched start

  return code;
}
