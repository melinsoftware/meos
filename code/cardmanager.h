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

using std::map;
using std::vector;
using std::string;
using std::list;
class gdioutput;

struct SIPunch {
  int Code;
  int Time;
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
    clear(nullptr);
    convertedTime = status;
  }
  // Clears the card if this == condition or condition is 0
  void clear(const DeviceCard *condition) {
    if (this == condition || !condition)
      memset(this, 0, sizeof(DeviceCard));
  }

  void analyseHour12Time(int zeroTime);

  bool empty() const { return CardNumber == 0; }
  int CardNumber;
  SIPunch StartPunch;
  SIPunch FinishPunch;
  SIPunch CheckPunch;
  int nPunch;
  SIPunch Punch[192];
  wchar_t firstName[21];
  wchar_t lastName[21];
  wchar_t club[41];
  int miliVolt; // SIAC voltage
  char readOutTime[32];
  bool punchOnly;
  ConvertedTimeStatus convertedTime;
  // Used for manual time input
  int runnerId;
  int relativeFinishTime;
  bool statusOK;
  bool statusDNF;

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
