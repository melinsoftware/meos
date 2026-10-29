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

#include <vector>
#include <mutex>
#include <string>
#include <list>
#include <map>

#include "oPunch.h"
#include "timeconstants.hpp"

using std::map;
using std::vector;
using std::string;
using std::list;
class gdioutput;

struct CardPunch {
  int Code = -1;
  int Time = 0;
  void clearSubSeconds() {
    if (Code != -1 && Time >= 0)
      Time -= (Time % timeConstSecond);
  }
  void analyseHour12Time(int zeroTime);
};

enum class ConvertedTimeStatus {
  Unknown = 0,
  Hour12,
  Hour24,
  Done,
};

struct DeviceCard {
  DeviceCard(ConvertedTimeStatus status) {
    convertedTime = status;
  }
  // Clears the card if this == condition or condition is 0
  void clear(const DeviceCard *condition) {
    if (this == condition || !condition)
      *this = DeviceCard(ConvertedTimeStatus::Unknown);
  }

  void analyseHour12Time(int zeroTime);

  bool empty() const { return CardNumber == 0; }
  int CardNumber = 0;
  CardPunch StartPunch;
  CardPunch FinishPunch;
  CardPunch CheckPunch;
  int nPunch = 0;
  CardPunch Punch[192];

  void clearSubSeconds() {
    StartPunch.clearSubSeconds();
    FinishPunch.clearSubSeconds();
    CheckPunch.clearSubSeconds();
    for (int i = 0; i < nPunch; i++)
      Punch[i].clearSubSeconds();
  }

  wchar_t firstName[21] = { 0 };
  wchar_t lastName[21] = { 0 };
  wchar_t club[41] = { 0 };
  int miliVolt = 0; // SIAC voltage
  char readOutTime[32] = { 0 };
  bool punchOnly = false;
  ConvertedTimeStatus convertedTime = ConvertedTimeStatus::Unknown;
  // Used for manual time input
  int runnerId = 0;
  int relativeFinishTime = 0;
  bool statusOK = false;
  bool statusDNF = false;

  // 
  bool isDebugCard = false;
  vector<string> codeLogData(int row) const;
  static vector<string> logHeader();

  unsigned int calculateHash() const;
  bool isManualInput() const { return runnerId != 0; }

  string serializePunches() const;
  void deserializePunches(const string &arg);

  int getFirstTime() const;
};


typedef DeviceCard SICard;

class ICardSubscriber {
public:
  virtual void insertCard(gdioutput &gdi, DeviceCard &card) = 0;
  virtual ~ICardSubscriber() = 0 {};
};

class CardManager {
private:

  const HWND hWndNotify;
  bool useSubSeconds = false;

  static CardManager *cardManager;
  CardManager(HWND hWndNotify);

  list<DeviceCard> readCards;

  std::mutex cardMutex;

  map<string, ICardSubscriber *> subscribers;

  
  vector<uint8_t> punchMap;
  void insertCard(gdioutput &gdi, DeviceCard &card);

public:

  void subscribe(const string tag, ICardSubscriber *cs) {
    subscribers[tag] = cs;
  }

  void unsubscribe(const string tag) {
    subscribers.erase(tag);
  }

  void readRawData(const wstring &file);
  void addCard(const DeviceCard &sic);
  bool getCard(DeviceCard &sic);
  void processQueue(gdioutput &gdi);

  void resetPunchMap();
  map<int, oPunch::SpecialPunch> getSpecialMappings() const;
  void clearSpecialMappings();
  void addSpecialMapping(int code, oPunch::SpecialPunch);
  void removeSpecialMapping(int code);
  int mapPunch(int code) const;

  void setSubSecondMode(bool useSubSecond);
  bool subSecondMode() const {
    return useSubSeconds;
  }

  static void init(HWND hWnd);
  static CardManager &instance();
};
