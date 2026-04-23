/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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
#include "restserver.h"
#include "resource.h"
#include "xmlparser.h"
#include "infoserver.h"

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
    case GUI_LINK: {
      TextInfo ti = dynamic_cast<TextInfo &>(*data);
      OnlineInput &ores = dynamic_cast<OnlineInput &>(*AutoMachine::getMachine(ti.getExtraInt()));      
      return ores.processLink(*gdi, ti);
    }
  }
  return 0;
}

int OnlineInput::processLink(gdioutput &gdi, TextInfo &bi) {
  if (!errorLogFile.empty())
    gdi.openDoc(errorLogFile);
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
    updateEntryStatus(gdi);
    gdi.setInputStatus("UseUnitId", serverType != Type::MIP);
  }
  return 0;
}

int OnlineInput::processButton(gdioutput &gdi, ButtonInfo &bi) {
  if (settingsOE == nullptr)
    throw std::exception("Internal error");

  oEvent &oe = *settingsOE;
  if (bi.id == "SaveMapping") {
    int ctrl = gdi.getTextNo("Code");
    if (ctrl<1 || ctrl>=1024)
      throw meosException("Ogiltig kontrollkod");
    ListBoxInfo lbi;
    if (!gdi.getSelectedItem("Function", lbi))
      throw meosException("Ogiltig funktion");
    oe.definePunchMapping(ctrl, oPunch::SpecialPunch(lbi.data));
    fillMappings(oe, gdi);
    gdi.setInputStatus("LoadLastMapping", false);
  }
  else if (bi.id == "RemoveMapping") {
    set<int> sel;
    gdi.getSelection("Mappings", sel);
    for (auto it = sel.begin(); it != sel.end(); ++it) {
      oe.definePunchMapping(*it, oPunch::PunchUnused);
    }
    fillMappings(oe, gdi);
    gdi.setInputStatus("LoadLastMapping", false);
  }
  else if (bi.id == "LoadLastMapping") {
    auto cm = oe.getPropertyString("ControlMap", L"");
    oe.getDI().setString("ControlMap", cm);
    fillMappings(oe, gdi);
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

void OnlineInput::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GUI_BUTTON) {
    if (info.id == "AllowEntry")
      updateEntryStatus(gdi);
  }
}

void OnlineInput::settings(gdioutput &gdi, oEvent &oe, State state) {
  settingsOE = &oe;

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
  gdi.fillRight();
  gdi.pushX();
  if (serverType != Type::MIP && useUnitId)
    gdi.addInput("CmpID", unitId, 20, 0, L"Enhetens ID-nummer (MAC):");
  else
    gdi.addInput("CmpID", sanitizeId(cmpId), 20, 0, L"Tävlingens ID-nummer:");

  gdi.fillDown();
  gdi.addInput("Password", passwd, 15, 0, L"Lösenord:").setPassword(true);

  gdi.dropLine(1);
  gdi.popX();

  bool allowEntry = serverType == Type::MIP && epClass != EntryPermissionClass::None
                    && epType != EntryPermissionType::None;

  gdi.fillRight();
  int cxE = gdi.getCX();
  int cyE = gdi.getCY();
  gdi.setRestorePoint("StartEntryRect");
  gdi.dropLine(0.5);
  gdi.setCX(cxE + gdi.getLineHeight());
  gdi.addCheckbox("AllowEntry", "Tillåt anmälan", 0, allowEntry).setHandler(this);
  gdi.dropLine(1.5);
  gdi.setCX(cxE + gdi.getLineHeight());
  gdi.addSelection("PermissionPerson", 180, 200, 0, L"Vem får anmäla sig:");
  gdi.setItems("PermissionPerson", RestServer::getPermissionsPersons());
  gdi.autoGrow("PermissionPerson");
  
  gdi.fillDown();
  gdi.addSelection("PermissionClass", 180, 200, 0, L"Till vilka klasser:");
  gdi.setItems("PermissionClass", RestServer::getPermissionsClass());
  gdi.autoGrow("PermissionClass");
  if (allowEntry) {
    gdi.selectItemByData("PermissionPerson", int(epType));
    gdi.selectItemByData("PermissionClass", int(epClass));
  }
  else {
    gdi.selectItemByData("PermissionPerson", int(EntryPermissionType::InDbAny));
    gdi.selectItemByData("PermissionClass", int(EntryPermissionClass::QuickEntry));
  }
  gdi.dropLine(0.5);
  RECT rcE = gdi.getDimensionSince("StartEntryRect");
  rcE.left = cxE;
  rcE.right += gdi.getLineHeight();
  rcE.top -= gdi.getLineHeight();
  rcE.bottom += gdi.getLineHeight()/2;

  gdi.addRectangle(rcE, GDICOLOR::colorLightCyan);
  gdi.popX();
  gdi.dropLine(0.5);
  updateEntryStatus(gdi);

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

void OnlineInput::updateEntryStatus(gdioutput &gdi) {
  bool mip = serverType == Type::MIP;
  bool allow = mip && gdi.isChecked("AllowEntry");

  gdi.setInputStatus("AllowEntry", mip);
  gdi.setInputStatus("PermissionPerson", allow);
  gdi.setInputStatus("PermissionClass", allow);
}

void OnlineInput::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);
  int iv=gdi.getTextNo("Interval");
  const wstring &xurl=gdi.getText("URL");

  if (!xurl.empty()) {
    oe.setProperty("MIPURL", xurl);
    oe.setProperty("InputServer", int(serverType));
  }

  cmpId = gdi.getText("CmpID");
  unitId = gdi.getText("CmpID");
  passwd = gdi.getText("Password");

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

  if (serverType == Type::MIP) {
    if (gdi.isChecked("AllowEntry")) {
      epType = (EntryPermissionType)gdi.getSelectedItem("PermissionPerson").first;
      epClass = (EntryPermissionClass)gdi.getSelectedItem("PermissionClass").first;
    }
    else {
      epClass = EntryPermissionClass::None;
      epType = EntryPermissionType::None;
    }
  }

  if (doProcess) {
    errorLogFile.clear();
    try {
      process(gdi, &oe, SyncNone);
    }
    catch (meosException &) {
      if (!errorLogFile.empty()) {
        gdi.openDoc(errorLogFile);
      }
      throw;
    }
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

  if (!errorLogFile.empty()) {
    gdi.fillRight();
    gdi.addImage("", gdi.getCY() - gdi.scaleLength(2), gdi.getCX(), 0, itow(IDI_MEOSERROR), 
                 gdi.scaleLength(16), gdi.scaleLength(16));

    gdi.addString("", 1, "Protocoll error  ");
    gdi.addString("ErrorLog", 0, L"#" + errorLogFile, OnlineCB).setExtra(getId());
  }

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
  cnt.set("EntryClass", int(epClass));
  cnt.set("EntryType", int(epType));

  string pwProp = "@inppwd" + gdioutput::narrow(getMachineName());
  oe.setPropertyEncrypt(pwProp.c_str(), gdioutput::toUTF8(passwd));

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
  url = cnt->getString("url");
  cmpId = cnt->getString("cmpId");
  unitId = cnt->getString("unitId");

  string pwProp = "@inppwd" + gdioutput::narrow(getMachineName());
  passwd = gdioutput::fromUTF8(oe.getPropertyStringDecrypt(pwProp.c_str(), ""));

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

  epClass = EntryPermissionClass(cnt->getInt("EntryClass"));
  epType = EntryPermissionType(cnt->getInt("EntryType"));

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
    vector<pair<wstring, wstring>> key;
    wstring q;
    if (serverType == Type::SICenter) {
      // SportIdent Center uses milliseconds since linux Epoch 1970-01-01
      if (!useUnitId)
        q = L"?eventId=" + sanitizeId(cmpId) + L"&afterId=" + itow(lastImportedId) + L"&after=" + std::to_wstring(getZeroTimeMSLinuxEpoch(*oe));
      else
        q = L"?modem=" + unitId + L"&afterId=" + itow(lastImportedId) + L"&after=" + std::to_wstring(getZeroTimeMSLinuxEpoch(*oe));

      pair<wstring, wstring> mk1(L"Accept", L"text/csv");
      key.push_back(mk1);
    }
    else if (serverType == Type::ROC) {
      if (!useUnitId)
        q = L"?unitId=" + sanitizeId(cmpId) + L"&lastId=" + itow(lastImportedId) + L"&date=" + oe->getDate() + L"&time=" + oe->getZeroTime();
      else
        q = L"?unitId=" + unitId + L"&lastId=" + itow(lastImportedId) + L"&date=" + oe->getDate() + L"&time=" + oe->getZeroTime();
    }
    else {
      key.emplace_back(L"competition", sanitizeId(cmpId));
      key.emplace_back(L"lastid", itow(lastImportedId)); // Assumes index 1 (see below for response)
      if (!passwd.empty())
        key.emplace_back(L"pwd", passwd);
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
        wstring responsePost;
        
        processMIP(*oe, result, responsePost);

        if (!responsePost.empty()) {
          assert(key.size() >= 2);
          key[1].first = L"response";
          key.emplace_back(L"Content-Type", L"text/plain");
          
          wstring result2 = getTempFile();
          dwl.postFile(url, responsePost, result2, key, pw);

          removeTempFile(responsePost);

          xmlparser xml2;
          try {
            xml2.read(result2);
            auto res2 = xml2.getObject("MIPStatus");
          }
          catch (std::exception &) {
            if (errorLogFile.empty()) {
              errorLogFile = getTempPath() + L"\\inputlog.xml";
              CopyFile(result2.c_str(), errorLogFile.c_str(), false);
            }
            removeTempFile(result2);
            throw meosException("Onlineservern svarade felaktigt.");
          }
          catch (...) {
            removeTempFile(result2);
            throw;
          }
          if (mipCmp)
            mipCmp->commitComplete(); // Mark commit as completed
          removeTempFile(result2);
        }
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

void OnlineInput::processMIP(oEvent &oe, const wstring &inputFile, wstring &responePost) {
  xmlobject res;
  xmlparser xml;
  try {
    xml.read(inputFile);
    res = xml.getObject("MIPData");
  }
  catch (std::exception &) {
    if (errorLogFile.empty()) {
      errorLogFile = getTempPath() + L"\\inputlog.xml";
      CopyFile(inputFile.c_str(), errorLogFile.c_str(), false);
    }

    throw meosException("Onlineservern svarade felaktigt.");
  }
  errorLogFile.clear();
  xmlList entries;
  res.getObjects("entry", entries);
  vector<MipEntryInfo> statusOut;
  processEntries(oe, entries, statusOut);

  xmlList cards;
  res.getObjects("card", cards);
  processCards(oe, cards);

  xmlList punches;
  res.getObjects("p", punches);
  processPunches(oe, punches);

  xmlList teamlineup;
  res.getObjects("team", teamlineup);
  processTeamLineups(oe, teamlineup);

  xmlList responseRequests;
  res.getObjects("response", responseRequests);
  bool entryStatus = false;
  bool entryConfig = false;
  bool entryEntries = false;

  for (auto &rr : responseRequests) {
    string rtype;
    rr.getObjectString("type", rtype);
    if (rtype == "entrystatus")
      entryStatus = true;
    if (rtype == "config")
      entryConfig = true;
    if (rtype == "entries")
      entryEntries = true;
  }
  xmlparser xmlOut;

  if (entryStatus || entryConfig || entryEntries) {
    responePost = getTempFile();
    xmlOut.openOutputT(responePost.c_str(), false, "MipStatusResponse");
  }

  if (entryStatus) {
    xmlOut.startTag("EntryStatus");
    vector<pair<string, wstring>> props;
    for (auto &s : statusOut) {
      props.clear();
      if (s.status == MipEntryStatus::EntryOK) 
        props.emplace_back("status", L"OK");
      else if (s.status == MipEntryStatus::Failed)
        props.emplace_back("status", L"ERROR");
      else {
        assert(s.status == MipEntryStatus::UpdatedOK);
        props.emplace_back("status", L"UPDATEOK");
      }
      props.emplace_back("id", itow(s.id));
      props.emplace_back("localId", itow(s.meosId));
      props.emplace_back("message", lang.tl(s.statusMessage));
      xmlOut.write("Entry", props, L"");
    }
    xmlOut.endTag();
  }

  auto getEntryClasses = [&oe, this]() {
    set<int> clsSet;
    vector<pClass> cls;
    oe.getClasses(cls, true);
    for (auto c : cls) {
      if (epClass == EntryPermissionClass::Any || (epClass == EntryPermissionClass::QuickEntry && c->getAllowQuickEntry()))
        clsSet.insert(c->getId());
    }
    return clsSet;
  };

  if (entryConfig) {
    InfoCompetition &cmp = getMipCmp(true);
    if (!entryEntries) {
      auto clsSet = getEntryClasses();
      cmp.synchronize(oe, L"", InfoCompetition::SynchType::CmpAndClass, clsSet, {}, {}, true);
      xmlbuffer xbuff;
      cmp.getCompleteXML(xbuff);
      xbuff.startTagXML(xmlOut);
      xbuff.commit(xmlOut, 100000);
      xmlOut.endTag();
    }
  }

  if (entryEntries) {
    InfoCompetition &cmp = getMipCmp(false);
    auto clsSet = getEntryClasses();
    cmp.synchronize(oe, L"", InfoCompetition::SynchType::Entries, clsSet, {}, {}, true);
    xmlbuffer xbuff;
    cmp.getDiffXML(xbuff);
    xbuff.startTagXML(xmlOut);
    xbuff.commit(xmlOut, 100000);
    xmlOut.endTag();
  }

  if (xmlOut.hasOpenOut())
    xmlOut.closeOut();

  lastImportedId = res.getAttrib("lastid").getInt();
}

InfoCompetition &OnlineInput::getMipCmp(bool forceReset) {
  int id = _wtoi(cmpId.c_str());
  if (!mipCmp || id != mipCmp->getId() || forceReset)
    mipCmp = make_shared<InfoCompetition>(id);

  return *mipCmp;
}

void OnlineInput::processPunches(oEvent &oe, const xmlList &punches) {
  auto transformTime = [](int in) {
    if (timeConstSecond <= 10)
      return in / (10 / timeConstSecond);
    else
      return in * (timeConstSecond / 10);
  };

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
    int time = transformTime(punches[k].getObjectInt("time"));

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

	time_t epoch_abs = getZeroTimeMSLinuxEpoch(oe);

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

void OnlineInput::processCards(oEvent &oe, const xmlList &cards) {

  auto transformTime = [](int in) {
    if (timeConstSecond <= 10)
      return in / (10 / timeConstSecond);
    else
      return in * (timeConstSecond/10);
  };

  for (size_t k = 0; k < cards.size(); k++) {
    SICard sic(ConvertedTimeStatus::Hour24);
    sic.CardNumber = cards[k].getObjectInt("number");

    if (xmlobject fin = cards[k].getObject("finish"); fin) {
      sic.FinishPunch.Time = transformTime(fin.getObjectInt("time"));
      sic.FinishPunch.Code = fin.getObjectInt("code");
    }

    if (xmlobject sta = cards[k].getObject("start"); sta) {
      sic.StartPunch.Time = transformTime(sta.getObjectInt("time"));
      sic.StartPunch.Code = sta.getObjectInt("code");
    }

    if (xmlobject chk = cards[k].getObject("check"); chk) {
      sic.CheckPunch.Time = transformTime(chk.getObjectInt("time"));
      sic.CheckPunch.Code = chk.getObjectInt("code");
    }

    xmlList punches;
    cards[k].getObjects("p", punches);
    for (size_t j = 0; j < punches.size(); j++) {
      sic.Punch[j].Code = punches[j].getObjectInt("code");
      sic.Punch[j].Time = transformTime(punches[j].getObjectInt("time"));
    }
    sic.nPunch = punches.size();
    TabSI::getSI().addCard(sic);
  }
}

void OnlineInput::processTeamLineups(oEvent &oe, const xmlList &updates) {
}

void OnlineInput::processEntries(oEvent &oe, const xmlList &entries, vector<MipEntryInfo> &status) {
  status.reserve(entries.size());
  map<int, pRunner> raceId2R;
  map<uint64_t, pRunner> extId2R;

  auto getRaceToId = [&oe]() {
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
    return raceId2R;
  };

  auto getExtIdToR = [&oe]() {
    map<uint64_t, pRunner> extId2R;
    vector<pRunner> runners;
    oe.getRunners(0, 0, runners);
    for (auto &r : runners) {
      auto id = r->getExtIdentifier();
      if (id != 0) {
        extId2R[id] = r;
      }
    }
    return extId2R;
  };

  wstring error;

  for (auto &entry : entries) {
    int id = entry.getObjectInt("id"); // External 
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
      wstring locError = L"#" + lang.tl("Okänd klass: ");
      if (clsName.empty())
        locError += L"?";
      else
        locError += clsName;

      if (classId > 0)
        locError += L", Id = " + itow(classId);
      
      status.emplace_back(id, 0, MipEntryStatus::Failed, locError);

      if (error.empty()) {
        error = locError;
      }
      continue;
    }

    int fee = entry.getObjectInt("fee");
    bool gotPaid = entry.got("paid");
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
      addInfo(L"Fel: X#" + lang.tl("Namnet kan inte vara tomt"));
      status.emplace_back(id, 0, MipEntryStatus::Failed, L"Namnet kan inte vara tomt");
      continue;
    }
    wstring club;
    entry.getObjectString("club", club);

    wstring bib, phone, rank, text;
    bool gotBib = entry.got("bib");
    entry.getObjectString("bib", bib);
    bool gotPhone = entry.got("phone");
    entry.getObjectString("phone", phone);
    entry.getObjectString("rank", rank);
    bool gotText = entry.got("text");
    entry.getObjectString("text", text);
    bool gotDataA = entry.got("dataA");
    int dataA = entry.getObjectInt("dataA");
    bool gotDataB = entry.got("dataB");
    int dataB = entry.getObjectInt("dataB");
    bool noTiming = entry.getObjectBool("notiming");

    wstring start;
    bool gotStart = entry.got("starttime");
    entry.getObjectString("starttime", start);
    wstring runnerStatus;
    entry.getObjectString("status", runnerStatus);

    int cardNo = 0;
    bool hiredCard = false;
    bool hasCard = false;
    xmlobject card = entry.getObject("card");
    if (card) {
      cardNo = card.getInt();
      hiredCard = card.getObjectBool("hired");
      hasCard = true;
    }

    if (!hiredCard && oe.hasHiredCardData())
      hiredCard = oe.isHiredCard(cardNo);

    pRunner r = nullptr;
    wstring extIdS;

    if (int mid = entry.getObjectInt("localId"); mid > 0) {
      r = oe.getRunner(mid, 0);
      if (r && !r->matchName(name))
        r = nullptr;
    }
    else if (id > 0) {
      if (raceId2R.empty())
        raceId2R = getRaceToId();

      auto res = raceId2R.find(id);
      if (res != raceId2R.end()) {
        r = res->second;
        if (r && !r->matchName(name))
          r = nullptr;
      }
    }

    uint64_t extId = 0;
    if (!entry.getObjectString("extId", extIdS).empty()) {
      extId = oBase::converExtIdentifierString(extIdS);
      if (!r) {
        int pid = oBase::idFromExtId(extId);
        r = oe.getRunner(pid, 0);
        if (r && r->getExtIdentifier() != extId)
          r = nullptr;

        if (!r) {
          if (extId2R.empty())
            extId2R = getExtIdToR();

          auto res = extId2R.find(extId);
          if (res != extId2R.end())
            r = res->second;
        }
        if (r && !r->matchName(name))
          r = nullptr;
      }
    }

    if (!r) {
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
      // Do not allow change of club or class with only name/card match

      if (r) {
        if (!stringMatch(r->getClub(), club))
          r = nullptr;
        else if (r->getClassId(false) != classId)
          r = nullptr;
      }
    }

    if (r == nullptr) {
      bool permissionDenied = false;
      wstring locError;
      RestServer::newEntryErrorCheck(oe, extId, name, club, classId,
                                     cardNo, epClass, epType,
                                     permissionDenied, locError);

      if (permissionDenied) {
        addInfo(L"Fel: X#" + name + L", Permission denied");
        status.emplace_back(id, 0, MipEntryStatus::Failed, L"Not allowed");
        continue;
      }
      else if (locError.size() > 0) {
        addInfo(L"Fel: X#" + name + L", " + lang.tl(locError));
        status.emplace_back(id, 0, MipEntryStatus::Failed, locError);
        continue;
      }

      r = oe.addRunner(name, club, cls->getId(), cardNo, birthyear, true);
      addInfo(L"#" + r->getCompleteIdentification(oRunner::IDType::OnlyThis) + L", " + cls->getName());
      status.emplace_back(id, r->getId(), MipEntryStatus::EntryOK, L"");
      if (fee == 0)
        fee = r->getDefaultFee();
    }
    else {
      addInfo(L"#" + r->getCompleteIdentification(oRunner::IDType::OnlyThis) + L", " + lang.tl("[Uppdaterad anmälan]"));
      
      r->setName(name, false);
      if (!club.empty())
        r->setClub(club);
      if (!birthyear.empty())
        r->setBirthDate(birthyear);
      if (cardNo != 0)
        r->setCardNo(cardNo, false, false);
      if (cls != nullptr)
        r->setClassId(cls->getId(), true);
      status.emplace_back(id, r->getId(), MipEntryStatus::UpdatedOK, L"Updated");
      if (fee == 0)
        fee = r->getEntryFee();
    }
    
    auto di = r->getDI();
    di.setInt("Fee", fee);
    int toPay = fee;
    int cf = 0;
    if (hasCard) {
      if (hiredCard) {
        cf = r->getEvent()->getBaseCardFee();
        if (cf > 0)
          toPay += cf;
      }
      r->setFlag(oRunner::FlagAddedViaAPI, true);
      di.setInt("CardFee", cf);
    }

    if (gotPaid)
      di.setInt("Paid", paid ? toPay : 0);
    
    if (gotStart) {
      r->setStartTimeS(start);
    }

    // Sex and rank is only updated if empty
    if (nat.empty())
      nat = r->getNationality();

    oDataConstInterface dci = r->getDCI();
    if (phone.empty())
      phone = dci.getString("Phone");
    if (!gotText)
      text = dci.getString("TextA");
    if (!gotDataA)
      dataA = dci.getInt("DataA");
    if (!gotDataB)
      dataB = dci.getInt("DataB");
    if (!gotBib)
      bib = r->getBib();
    
    r->setExtraPersonData(sex, nat, rank, phone, bib, text, dataA, dataB);

    if (noTiming)
      r->setStatus(StatusNoTiming, true, oBase::ChangeType::Update, false);

    if (!runnerStatus.empty()) {
      RunnerStatus st = oAbstractRunner::decodeStatus(runnerStatus);
      r->setStatus(st, true, oBase::ChangeType::Update, false);

      if (r->getCard()) {
        vector<pair<int, pControl>> mp;
        r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
      }
    }

    if (id > 0) {
      di.setInt("RaceId", id | (1 << 30));
      raceId2R[id] = r;
    }
    r->synchronize(true);
  }
  //if (!error.empty())
  //  throw meosException(error);
}

time_t OnlineInput::getZeroTimeMSLinuxEpoch(const oEvent &oe) const {
  SYSTEMTIME st;
  convertDateYMD(oe.getDate(), st, false);

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
  epoch += (time_t)oe.getZeroTimeNum() * 100;    // we use tenth of a second
  return epoch;
}

int OnlineInput::mapPunch(int code) const {
  if (auto res = specialPunches.find(code); res != specialPunches.end())
    code = res->second;
  else if (code == oPunch::SpecialPunch::PunchStart)
    code = oPunch::SpecialPunch::PunchCheck; // Do not allow unmatched start

  return code;
}

wstring OnlineInput::sanitizeId(const wstring &in) {
  wstring out;
  for (auto w : in) {
    if (iswspace(w))
      continue;
    if ((w >= '0' && w <= '9') || (w >= 'a' && w <= 'z') || (w >= 'A' && w <= 'Z'))
      out.push_back(w);
    else
      break;
  }
  if (out.empty())
    out = L"0";
  return out;
}
