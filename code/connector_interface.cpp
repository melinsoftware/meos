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

#include <Windows.h>
#include "connector_interface.h"
#include "meosexception.h"
#include "carddevices.h"

DynamicConnection::DynamicConnection(const wstring& library) {
  hModule = LoadLibraryEx(library.c_str(), 0, 0);
  if (hModule == nullptr) {
    auto err = GetLastError();

    int lst = library.find_last_of(L"\\/");
    
    wstring fn = library;
    if (lst +1 < library.size())
      fn = library.substr(lst + 1);

    if (err == ERROR_BAD_EXE_FORMAT)
      throw meosException(L"X är inte ett giltigt programbibliotek. "
                          L"Kontrollera att du använder en version för Y-bitars Windows#"
                          + fn + L"#" + itow(CardDevices::buildversion));

    throw meosException(L"Cannot load: " + fn + L"\n\n" + getErrorMessage(err));
  }
  try {
    FARPROC ff;

    ff = loadProc("getName");
    MConWString(__cdecl * getName)() = (MConWString(__cdecl*)())ff;
    name = getName();

    ff = loadProc("getShortName");
    MConWString(__cdecl * getShortName)() = (MConWString(__cdecl*)())ff;
    shortName = getShortName();

    ff = loadProc("init");
    enumerateDevicesPtr = (int(__cdecl*)(int, MConWString*, bool*))ff;
    initPtr = (void(__cdecl*)(MConDeviceProps*, MConWString *, MConWString *, MConAddCard, MConAddPunch))ff;

    ff = loadProc("enumerateDevices");
    enumerateDevicesPtr =  (int(__cdecl*)(int, MConWString *, bool* ))ff;

    ff = loadProc("status");
    statusPtr = (void(__cdecl*)(int, MConDeviceStatus*))ff;

    ff = loadProc("openConnection");
    openConnectionPtr = (int(__cdecl*)(int, bool, MConWString, MConLogMessage))ff;

    ff = loadProc("autoDetect", true);
    autoDetectPtr = (int(__cdecl *)(int, int*))ff;

    ff = loadProc("closeConnection");
    closeConnectionPtr = (void(__cdecl*)(int))ff;

    ff = loadProc("unload");
    unloadPtr = (void(__cdecl*)())ff;

    ff = loadProc("monitorThread");
    monitorThreadPtr = (MConWString(__cdecl*)(int))ff;
  }
  catch (...) {
    FreeLibrary(hModule);
    hModule = 0;
    throw;
  }
}

DynamicConnection::~DynamicConnection() {
  if (unloadPtr)
    unloadPtr();
}

void DynamicConnection::unload() {
  if (unloadPtr)
    unloadPtr();
}

FARPROC DynamicConnection::loadProc(const char* name, bool allowMissing) const {
  FARPROC pa = GetProcAddress(hModule, name);

  if (!pa && !allowMissing)
    throw meosException("Connector error, missing " + string(name));
    
  return pa;
}

void DynamicConnection::init(bool useSubSeconds, MConAddCard addCard, MConAddPunch addPunch) {
  MConWString startParameterName, deviceHelpPtr;
  MConDeviceProps props;
  props.useSubSecond = useSubSeconds;
  initPtr(&props, &deviceHelpPtr, &startParameterName, addCard, addPunch);
  supportAuto = props.supportAutoDetect;
  if (startParameterName != nullptr) {
    parameterName = startParameterName;
    size_t sep = parameterName.find_first_of('|');
    if (sep != wstring::npos) {
      parameterDefault = parameterName.substr(sep+1);
      parameterName = parameterName.substr(0, sep);
      sep = parameterDefault.find_first_of('|');
      if (sep != wstring::npos) {
        parameterHelp = parameterDefault.substr(sep + 1);
        parameterDefault = parameterDefault.substr(0, sep);
      }
    }
  }
  if (deviceHelpPtr)
    deviceHelpStr = deviceHelpPtr;
}

bool DynamicConnection::needInputParameter(wstring &name, wstring &parDef,  wstring &help) const {
  if (parameterName.empty())
    return false;
  name = parameterName;
  help = parameterHelp;
  parDef = parameterDefault;
  return true;
}

vector<pair<wstring, bool>> DynamicConnection::enumerateDevices() {
  int maxNum = 32;
  vector<MConWString> stringArray(maxNum);
  vector<uint8_t> boolArray(maxNum);
  numDevices = enumerateDevicesPtr(maxNum, stringArray.data(), (bool*)boolArray.data());
  if (numDevices < 0 || numDevices > maxNum)
    throw meosException("Error in connector call enumerateDevices");

  vector<pair<wstring, bool>> out(numDevices);
  for (int i = 0; i < numDevices; i++) {
    out[i].first = stringArray[i];
    out[i].second = boolArray[i];
  }
  return out;
}

vector<int> DynamicConnection::autoDetect() {
  vector<int> intArray;
  if (autoDetectPtr) {
    int maxNum = 32;
    intArray.resize(maxNum);
    numDevices = autoDetectPtr(maxNum, intArray.data());
    if (numDevices < 0 || numDevices > maxNum)
      throw meosException("Error in connector call autoDetect");

    intArray.resize(numDevices);
  }
  return intArray;
}

void DynamicConnection::status(int deviceIndex, DeviceStatus& status) const {
  MConDeviceStatus conStatus;
  statusPtr(deviceIndex, &conStatus);
  status.name = conStatus.name;
  status.connected = conStatus.connected;
  status.requireFunctionMapping = conStatus.requireFunctionMapping;
  status.messages.clear();
  
  if (conStatus.numStatusMessage < 0 || conStatus.numStatusMessage > (sizeof(conStatus.message) / sizeof(conStatus.message[0])))
    throw meosException("Error in connector call status");

  for (int i = 0; i < conStatus.numStatusMessage; i++) {
    wstring msg = conStatus.message[i];
    bool warn = conStatus.isWarningMessage[i];
    status.messages.emplace_back(msg, warn);
  }
}

int DynamicConnection::openConnection(int deviceIndex, bool listenOnly, const wstring &parameter, MConLogMessage logger) {
  return openConnectionPtr(deviceIndex, listenOnly, parameter.c_str(), logger);
}

void DynamicConnection::closeConnection(int deviceIndex) {
  closeConnectionPtr(deviceIndex);
}

wstring DynamicConnection::monitorThread(int deviceIndex) {
  wstring res;
  auto resPtr = monitorThreadPtr(deviceIndex);
  if (resPtr)
    res = resPtr;
  return res;
}

wstring DynamicConnection::getDeviceName(int deviceIndex) const {
  DeviceStatus s;
  status(deviceIndex, s);
  return s.name;
}


bool DynamicConnection::hasAnyOpenUnkownUnit() const {
  DeviceStatus st;
  for (int i = 0; i < numDevices; i++) {
    status(i, st);
    if (st.connected && st.requireFunctionMapping)
      return true;
  }
  return false;
}