#pragma once
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

#include "connector_interface.h"
#include "gdioutput.h"
#include <mutex>

class CardDevices {
public:
#ifdef _WIN64
  static constexpr int buildversion = 64;
#else
  static constexpr int buildversion = 32;
#endif
private:
  map<string, shared_ptr<DynamicConnection>> connections;
  map<int, pair<string, int>> idToConnectorIndex;
  bool useSubSeconds = false;
  pair<shared_ptr<DynamicConnection>, int> findDevice(int deviceId) const;
  static wstring translate(const wstring& in);

  wstring warningMessage;
  wstring warningOrigin;

  std::mutex warningMutex;

  struct ExternalConnector {
    wstring path;
    wstring md5;
  };

  map<pair<string, int>, ExternalConnector> extConnectors;

  void loadExt();
  void saveExt() const;

  void initExternal();

  wstring getConfigFile() const;

  static CardDevices *cardDevices;

public:
  CardDevices();

  const DynamicConnection &connection(int deviceId) const;

  void loadDevice(const string& tag, const wstring &library);
  
  void installDevice(const wstring &library);

  void unload();

  vector<tuple<wstring, int, bool>> enumerateDevices(const string &system);
  bool accept(const shared_ptr<DynamicConnection> &con, const wstring &wsystem) const;
  void status(int deviceId, DynamicConnection::DeviceStatus& status) const;
  bool isConnected(int deviceId) const;

  vector<int> autoDetect(const string &system);
  int openConnection(int deviceId, bool listenOnly, const wstring &parameter, gdioutput &out);
  void closeConnection(int deviceId);
  
  wstring deviceName(int deviceId) const;
  
  bool setWarning(const wstring &warningMessage, const wstring &warningOrigin);

  void showRunStatus(HWND hWnd);

  bool hasAnyOpenUnknownUnit() const;
  bool anySupportAutoDetect(const string &system) const;
  void setSubSecondMode(bool useSubSecond);

  int getDeviceId(const string &tag, int index) const;
  vector<tuple<string, wstring, wstring>> getConnectors() const;

  static CardDevices &instance();

  static bool isLoaded() {
    return cardDevices != nullptr;
  }
};
