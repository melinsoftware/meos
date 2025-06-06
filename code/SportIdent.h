﻿// SportIdent.h: interface for the SportIdent class.
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <set>
#include <vector>
#include "oPunch.h"

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
const BYTE STX=0x02;
const BYTE ETX=0x03;
const BYTE ACK=0x06;
const BYTE DLE=0x10;
const BYTE WAKEUP=0xFF;
const BYTE NAK=0x15;

// This is taken from r56 and checked in on r63
#include <vector>

class gdioutput;

struct SICard5Detect
{
  BYTE code;//Code;
  BYTE len;
  SHORT station;
  DWORD number;
  WORD crc;
};

struct SIPunch {
  DWORD Code;
  DWORD Time;

  void analyseHour12Time(DWORD zeroTime);
};

enum class ConvertedTimeStatus {
  Unknown = 0,
  Hour12,
  Hour24,
  Done,
};

struct SICard
{
  SICard(ConvertedTimeStatus status) {
    clear(0);
    convertedTime = status;
  }
  // Clears the card if this == condition or condition is 0
  void clear(const SICard *condition) {
    if (this==condition || condition==0)
      memset(this, 0, sizeof(SICard));
  }

  void analyseHour12Time(DWORD zeroTime);

  bool empty() const {return CardNumber==0;}
  DWORD CardNumber;
  SIPunch StartPunch;
  SIPunch FinishPunch;
  SIPunch CheckPunch;
  DWORD nPunch;
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
  vector<string> codeLogData(gdioutput &converter, int row) const;
  static vector<string> logHeader();

  unsigned int calculateHash() const;
  bool isManualInput() const {return runnerId != 0;}

  string serializePunches() const;
  void deserializePunches(const string &arg);

  int getFirstTime() const;
};

struct SI_StationData {
  SI_StationData();
  
  int stationNumber;
  int stationMode;
  bool extended;
  bool handShake;
  bool autoSend;
  int radioChannel;
};

struct SI_StationInfo
{
  SI_StationInfo();
  HANDLE ThreadHandle;
  wstring ComPort;
  HANDLE hComm;
  COMMTIMEOUTS TimeOuts;

  vector<SI_StationData> data;

  int stationMode() const {
    if (data.empty())
      return 0;
    else
      return data[0].stationMode;
  }

  bool extended() const {
    if (data.empty())
      return false;
    bool ext = true;
    for (size_t k = 0; k < data.size(); k++) {
      if (!data[k].extended)
        ext = false;
    }
    return ext;
  }
  //Used for TCP ports
  WORD tcpPort;
  int localZeroTime;
};


class SportIdent {
protected:

  bool useSubsecondMode = false;
  
  bool readSI6Block(HANDLE hComm, BYTE *data);
  bool readSystemData(SI_StationInfo *si, int retry=2);
  bool readSystemDataV2(SI_StationInfo &si);
  CRITICAL_SECTION SyncObj;

  int readByte_delay(BYTE &byte,  HANDLE hComm);
  int readBytes_delay(BYTE *byte, DWORD buffSize, DWORD len,  HANDLE hComm);
  int readBytesDLE_delay(BYTE *byte, DWORD buffSize, DWORD len,  HANDLE hComm);

  int readByte(BYTE &byte,  HANDLE hComm);
  int readBytes(BYTE *byte, DWORD len,  HANDLE hComm);
  int readBytesDLE(BYTE *byte, DWORD len,  HANDLE hComm);

  // Returns zero on failure, number of bytes used on success. 
  int analyzeStation(BYTE *db, SI_StationData &si);

  SI_StationInfo SI_Info[32];
  int n_SI_Info; //Number of structures..
  SI_StationInfo *Current_SI_Info; //Current SI_Info in use (for thread startup);

  WORD calcCRC(BYTE *data, DWORD length);
  bool checkCRC(BYTE *bf, DWORD maxLen);
  void setCRC(BYTE *bf);

  bool getCard5Data(BYTE *data, SICard &card);
  bool getCard6Data(BYTE *data, SICard &card);
  bool getCard9Data(BYTE *data, SICard &card);

  DWORD GetExtCardNumber(const BYTE *data) const;

  void getSI5Data(HANDLE hComm);
  void getSI5DataExt(HANDLE hComm);

  void getSI6Data(HANDLE hComm);
  void getSI6DataExt(HANDLE hComm);
  void getSI9DataExt(HANDLE hComm);

  void analyseSI5Time(BYTE *data, DWORD &time, DWORD &control);
  bool analysePunch(BYTE *data, DWORD &time, DWORD &control, bool subSecond);
  void analyseTPunch(BYTE *data, DWORD &time, DWORD &control);

  //Card read waiting to be processed.
  list<SICard> ReadCards;
  HWND hWndNotify;
  DWORD ClassId;

  volatile int tcpPortOpen;
  volatile size_t serverSocket;

  bool MonitorTEST(SI_StationInfo &si);
  bool MonitorSI(SI_StationInfo &si);
  int MonitorTCPSI(WORD port, int localZeroTime);

  struct TestCard {
    int cardNo;
    vector<int> punches;

    bool operator<(const TestCard &c) const {
      return cardNo < c.cardNo;
    }

    TestCard(int cardNo, const vector<int> &punches) : cardNo(cardNo), 
                                                 punches(punches) {
    }
  };

  set<TestCard> testCards;

  bool readVoltage;

  vector<uint8_t> punchMap;
  SI_StationInfo* findStationInt(const wstring& com);
  void addTestStation(const wstring& com);

public:

  map<int, oPunch::SpecialPunch> getSpecialMappings() const;
  void clearSpecialMappings();
  void addSpecialMapping(int code, oPunch::SpecialPunch);
  void removeSpecialMapping(int code);

  const SI_StationInfo *findStation(const wstring &com) const;

  /** Log debug data. */
  void debugLog(const wchar_t *ptr);

  void getInfoString(const wstring &com, vector<pair<bool, wstring>> &info) const;
  
  bool isAnyOpenUnkownUnit() const;
  
  bool isPortOpen(const wstring &com);
  bool autoDetect(list<int> &ComPorts);
  void stopMonitorThread();

  void startMonitorThread(const wchar_t *com);
  bool getCard(SICard &sic);
  void addCard(const SICard &sic);
  void addPunch(DWORD Time, int Station, int Card, int Mode=0);

  void addTestCard(int cardNo, const vector<int> &punches);

  void EnumrateSerialPorts(list<int> &ports);

  void closeCom(const wchar_t *com);
  bool openCom(const wchar_t *com);
  bool tcpAddPort(int port, DWORD zeroTime);
  bool openComListen(const wchar_t *com, DWORD BaudRate);

  SportIdent(HWND hWnd, DWORD Id, bool readVoltage);

  void readRawData(const wstring& file);

  void setSubSecondMode(bool subSec) { useSubsecondMode = subSec; }
  void resetPunchMap();

  virtual ~SportIdent();
  friend void start_si_thread(void *ptr);

};
