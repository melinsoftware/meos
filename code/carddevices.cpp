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

#include "StdAfx.h"

#include <thread>

#include <filesystem>
#include <iostream>
#include "carddevices.h"
#include "cardmanager.h"
#include "md5.h"
#include "xmlparser.h"
#include "localizer.h"
#include "meosexception.h"

namespace fs = std::filesystem;

CardDevices *CardDevices::cardDevices = nullptr;

CardDevices::CardDevices() = default;

CardDevices &CardDevices::instance() {
  if (cardDevices == nullptr) {
    cardDevices = new CardDevices();

    cardDevices->setSubSecondMode(CardManager::instance().subSecondMode());

    wstring error;
    try {
      cardDevices->loadDevice("SI", L"siconnector.dll");
    }
    catch (const std::exception &ex) {
      error = gdioutput::widen(ex.what());
    }

    try {
      cardDevices->loadDevice("SIBT", L"sibtconnector.dll");
    }
    catch (const std::exception& ex) {
      error = gdioutput::widen(ex.what());
    }

    try {
      cardDevices->loadDevice("tcp", L"ipconnector.dll");
    }
    catch (const std::exception &ex) {
      error = gdioutput::widen(ex.what());
    }

    cardDevices->loadExt();
    cardDevices->initExternal();

    if (!error.empty())
      throw meosException(error);
  }
  return *cardDevices;
}

namespace {
  gdioutput* logOutput = nullptr;
}

void __cdecl mconLogMessage(MConWString message, bool isWarning) {
  if (!logOutput)
    return;

  auto &s = logOutput->addString("", isWarning ? boldText : 0, message);
  if (isWarning)
    s.setColor(GDICOLOR::colorDarkRed);
}

void __cdecl mconAddCard(const MConCard* card) {
  DeviceCard sic(card->use12HourClock ? ConvertedTimeStatus::Hour12 : ConvertedTimeStatus::Hour24);

  sic.CardNumber = card->cardNo;
  sic.miliVolt = card->miliVolt;
  memcpy(sic.firstName, card->firstName, sizeof(sic.firstName));  
  memcpy(sic.lastName, card->lastName, sizeof(sic.lastName));
  memcpy(sic.club, card->club, sizeof(sic.club));

  auto assign = [](CardPunch& out, const MConPunch& in) {
    if (in.time >= 0)
      out.Time = in.time / (1000 / timeConstSecond);
    else
      out.Time = -1;

    out.Code = in.code;
  };

  assign(sic.CheckPunch, card->check);
  assign(sic.StartPunch, card->start);
  assign(sic.FinishPunch, card->finish);

  sic.nPunch = card->nPunch;
  for (int i = 0; i < card->nPunch; i++) {
    assign(sic.Punch[i], card->punch[i]);
  }

  CardManager::instance().addCard(sic);
}

void __cdecl mconAddPunch(const MConFreePunch* punch) {
  SICard sic(ConvertedTimeStatus::Hour24);
  
  sic.CardNumber = punch->cardNo;
  auto setTime = [](int code, int time, CardPunch& dst) {
    dst.Code = code;
    if (time >= 0)
      dst.Time = time / (1000 / timeConstSecond);
    else
      dst.Time = -1;
  };

  if (punch->type == MConPunchType::Unknown) { 
    int mappedCode = CardManager::instance().mapPunch(punch->code);
    int unit = 0;
    if (mappedCode != punch->code)
      unit = punch->code;
    else {
      unit = punch->unit;
    }

    if (mappedCode > 30) {
      setTime(punch->code, punch->time, sic.Punch[0]);
      sic.nPunch = 1;
    }
    else if (mappedCode == oPunch::PunchStart) {
      setTime(unit, punch->time, sic.StartPunch);
    }
    else if (mappedCode == oPunch::PunchCheck) {
      setTime(unit, punch->time, sic.CheckPunch);
    }
    else {
      setTime(unit, punch->time, sic.FinishPunch);
    }
  }
  else if (punch->type == MConPunchType::PunchControl) {
    setTime(punch->code, punch->time, sic.Punch[0]);
    sic.nPunch = 1;
  }
  else if (punch->type == MConPunchType::PunchStart) {
    setTime(punch->unit, punch->time, sic.StartPunch);
  }
  else if (punch->type == MConPunchType::PunchCheck) {
    setTime(punch->unit, punch->time, sic.CheckPunch);
  }
  else {
    setTime(punch->unit, punch->time, sic.FinishPunch);
  }
  
  sic.punchOnly = true;
  CardManager::instance().addCard(sic);
}

wstring CardDevices::getConfigFile() const {
  return getUserFile(L"devices.config");
}

void CardDevices::loadExt() {
  extConnectors.clear();
  if (!fileExists(getConfigFile()))
    return;
  xmlparser xml;
  xml.read(getConfigFile());
  xmlobject obj = xml.getObject("deviceconfig");
  xmlList x32, x64;
  obj.getObjects("Device32", x32);
  for (auto &x : x32) {
    string tag = x.getAttrib("tag").getStr();
    ExternalConnector con;
    x.getObjectString("Connector", con.path);
    x.getObjectString("MD5", con.md5);
    extConnectors[make_pair(tag, 32)] = std::move(con);
  }

  obj.getObjects("Device64", x64);
  for (auto &x : x64) {
    string tag = x.getAttrib("tag").getStr();
    ExternalConnector con;
    x.getObjectString("Connector", con.path);
    x.getObjectString("MD5", con.md5);
    extConnectors[make_pair(tag, 64)] = std::move(con);
  }
}

void CardDevices::saveExt() const {
  xmlparser xml;
  xml.openOutput(getConfigFile().c_str(), true);
  xml.startTag("deviceconfig");

  for (auto &[tag, data] : extConnectors) {
    if (tag.second == 32)
      xml.startTag("Device32", "tag", tag.first);
    else if (tag.second == 64)
      xml.startTag("Device64", "tag", tag.first);

    xml.write("Connector", data.path);
    xml.write("MD5", data.md5);
    xml.endTag();
  }

  xml.endTag();
  xml.closeOut();
}

extern gdioutput *gdi_main;

void CardDevices::initExternal() {
  set<pair<string, int>> toRemove;
  bool modify = false;
  wstring error;

  for (auto &[tag, data] : extConnectors) {
    if (tag.second == buildversion) {
      if (!fileExists(data.path)) {
        toRemove.insert(tag);
        continue;
      }
      wstring hash = md5::hash(data.path);
      if (hash != data.md5) {
        modify = true;
        if (!gdi_main->ask(L"help:warnmodified#" + data.path)) {
          toRemove.insert(tag);
          continue;
        }
        data.md5 = hash;
      }

      try {
        loadDevice(tag.first, data.path);
      }
      catch (const std::exception &ex) {
        error = gdioutput::widen(ex.what());
      }
    }
  }

  if (!toRemove.empty() || modify) {
    for (auto &tag : toRemove)
      extConnectors.erase(tag);

    saveExt();
  }

  if (!error.empty())
    throw meosException(error);
}


void CardDevices::loadDevice(const string& tag, const wstring& library) {
  auto &c = connections[tag] = make_shared<DynamicConnection>(library);
  c->init(useSubSeconds, mconAddCard, mconAddPunch);
}

void CardDevices::installDevice(const wstring &library) {
  wstring hash = md5::hash(library);
  auto c = make_shared<DynamicConnection>(library);
  wstring wtag = fs::path(library).filename().generic_wstring();
  string tag(wtag.begin(), wtag.end());

  loadExt();
  ExternalConnector ec;
  ec.path = library;
  ec.md5 = hash;
  extConnectors[make_pair(tag, buildversion)] = ec;
  saveExt();

  connections[tag] = c;
  c->init(useSubSeconds, mconAddCard, mconAddPunch);
}

void CardDevices::setSubSecondMode(bool useSubSecondIn) {
  if (useSubSecondIn != useSubSeconds) {
    for (auto &[tag, con] : connections) {
      con->init(useSubSecondIn, mconAddCard, mconAddPunch);
    }
    useSubSeconds = useSubSecondIn;
  }
}

void CardDevices::unload() {
  for (auto &[tag, con] : connections) {
    con->unload();
  }
  connections.clear();
}

const DynamicConnection &CardDevices::connection(int deviceId) const {
  return *findDevice(deviceId).first;
}

vector<tuple<wstring, int, bool>> CardDevices::enumerateDevices(const string &system) {
  idToConnectorIndex.clear();
  vector<tuple<wstring, int, bool>> out;
  wstring wsystem(system.begin(), system.end());
  int id = 1;
  for (auto &[tag, c] : connections) {
    if (!accept(c, wsystem))
      continue;
    auto res = c->enumerateDevices();
    for (int ix = 0; ix < res.size(); ix++) {
      auto& d = res[ix];
      out.emplace_back(d.first, id, d.second);
      idToConnectorIndex[id] = make_pair(tag, ix);
      id++;
    }
  }
  return out;
}

bool CardDevices::accept(const shared_ptr<DynamicConnection> &c, const wstring &wsystem) const {
  wstring tag = c->getShortName();
  return tag.empty() || wsystem.empty() || tag.front() == '*' || tag == wsystem;
}

vector<int> CardDevices::autoDetect(const string &system) {
  enumerateDevices(system);
  map<pair<string, int>, int> connectorIndex2id;
  wstring wsystem(system.begin(), system.end());

  for (auto &cons : idToConnectorIndex) 
    connectorIndex2id.emplace(cons.second, cons.first);
  
  vector<int> out;
  for (auto &[tag, c] : connections) {
    if (!accept(c, wsystem))
      continue;
    auto res = c->autoDetect();
    for (int i : res) {
      auto r2 = connectorIndex2id.find(make_pair(tag, i));
      if (r2 != connectorIndex2id.end())
        out.push_back(r2->second);
    }
  }
  return out;
}

void CardDevices::status(int deviceId, DynamicConnection::DeviceStatus& status) const {
  auto [con, ix] = findDevice(deviceId);
  con->status(ix, status);

  for (auto& [msg, warn] : status.messages) {
    msg = translate(msg);
  }
}

bool CardDevices::isConnected(int deviceId) const {
  auto [con, ix] = findDevice(deviceId);
  DynamicConnection::DeviceStatus status;
  con->status(ix, status);
  return status.connected;
}

pair<shared_ptr<DynamicConnection>, int> CardDevices::findDevice(int deviceId) const {
  auto res = idToConnectorIndex.find(deviceId);
  if (res == idToConnectorIndex.end())
    throw std::exception("Device not present");
  auto dev = connections.find(res->second.first);
  if (dev == connections.end())
    throw std::exception("Device not present");

  return make_pair(dev->second, res->second.second);
}

namespace {
  void runThread(shared_ptr<DynamicConnection> con, int ix, HWND mainWindow) {
    wstring res = con->monitorThread(ix);
    CardDevices::instance().setWarning(res, con->getDeviceName(ix));
    if (!res.empty()) {
      PostMessage(mainWindow, WM_USER + 1, 0, 0);
    }
  }
}

int CardDevices::openConnection(int deviceId, bool listenOnly, const wstring &parameter, gdioutput& out) {
  auto [con, ix] = findDevice(deviceId);
  logOutput = &out;
  int res = con->openConnection(ix, listenOnly, parameter, mconLogMessage);
  logOutput = nullptr;

  if (res != 1)
    return res;

  std::thread runner(runThread, con, ix, out.getHWNDMain());
  runner.detach();

  DynamicConnection::DeviceStatus st;
  int maxIter = 5;
  while (--maxIter >= 0 && !st.connected) {
    Sleep(100);
    con->status(ix, st);
  }

  return 1;
}

void CardDevices::closeConnection(int deviceId) {
  auto [con, ix] = findDevice(deviceId);
  con->closeConnection(ix);
}

wstring CardDevices::deviceName(int deviceId) const {
  auto [con, ix] = findDevice(deviceId);

  wstring name = con->getName();  
  wstring subName = con->getDeviceName(ix);

  if (!subName.empty())
    return name + L" / " + subName;

  return name;
}

wstring CardDevices::translate(const wstring& in) {
  wstring out;
  wstring toTranslate;
  out.reserve(in.length());
  bool doTranslate = false;
  for (int i = 0; i < in.length(); i++) {
    if (in[i] == '$') {
      if (doTranslate) {
        out += lang.tl(toTranslate);
        toTranslate.clear();
        doTranslate = false;
      }
      else {
        doTranslate = true;
      }
    }
    else {
      if (doTranslate)
        toTranslate.push_back(in[i]);
      else
        out.push_back(in[i]);
    }
  }
  return out;
}

bool CardDevices::setWarning(const wstring &warningMessageIn, const wstring &warningOriginIn) {
  lock_guard<mutex> lock(warningMutex);
  if (warningMessage.empty()) {
    warningMessage = warningMessageIn;
    warningOrigin = warningOriginIn;
    return true;
  }

  return false;
}

void CardDevices::showRunStatus(HWND hWnd) {
  wstring msg, origin;
  {
    lock_guard<mutex> lock(warningMutex);
    msg.swap(warningMessage);
    origin.swap(warningOrigin);
  }
  
  if (!msg.empty()) {
    MessageBox(hWnd, lang.tl(msg).c_str(), origin.c_str(), MB_ICONWARNING | MB_OK);
  }
}

bool CardDevices::hasAnyOpenUnknownUnit() const {
  for (auto &con : connections) {
    if (con.second->hasAnyOpenUnkownUnit())
      return true;
  }
  return false;
}

bool CardDevices::anySupportAutoDetect(const string &system) const {
  wstring wsystem(system.begin(), system.end());
  for (auto &[tag, c] : connections) {
    if (!c->supportAutoDetect() || !accept(c, wsystem))
      continue;
    return true;
  }
  return false;
}

int CardDevices::getDeviceId(const string &tag, int index) const {
  for (auto &[id, conIx] : idToConnectorIndex) {
    if (conIx.first == tag && conIx.second == index)
      return id;
  }
  return -1;
}

vector<tuple<string, wstring, wstring>> CardDevices::getConnectors() const {
  map<string, pair<wstring, wstring>> con;
  for (auto &[itag, c] : connections) {
    wstring tag = c->getShortName();
    if (tag.empty() || tag.front() == '*')
      continue;
    con.emplace(itag, make_pair(c->getName(), c->deviceHelp()));
  }
  vector<tuple<string, wstring, wstring>> out;
  for (auto &[tag, name] : con)
    out.emplace_back(tag, name.first, name.second);
  return out;
}
