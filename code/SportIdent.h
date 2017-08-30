// SportIdent.h: interface for the SportIdent class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SPORTIDENT_H__F13F5795_8FA9_4CE6_8497_7407CD590139__INCLUDED_)
#define AFX_SPORTIDENT_H__F13F5795_8FA9_4CE6_8497_7407CD590139__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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
const BYTE STX=0x02;
const BYTE ETX=0x03;
const BYTE ACK=0x06;
const BYTE DLE=0x10;
const BYTE WAKEUP=0xFF;
const BYTE NAK=0x15;

// This is taken from r56 and checked in on r63
#include <vector>

struct SICard5Detect
{
  BYTE code;//Code;
  BYTE len;
  SHORT station;
  DWORD number;
  WORD crc;
};

struct SIPunch
{
  DWORD Code;
  DWORD Time;
};

struct SICard
{
  SICard() {
    clear(0);
    convertedTime = false;
  }
  // Clears the card if this == condition or condition is 0
  void clear(const SICard *condition) {
    if (this==condition || condition==0)
      memset(this, 0, sizeof(SICard));
  }
  bool empty() const {return CardNumber==0;}
  DWORD CardNumber;
  SIPunch StartPunch;
  SIPunch FinishPunch;
  SIPunch CheckPunch;
  DWORD nPunch;
  SIPunch Punch[192];
  wchar_t FirstName[21];
  wchar_t LastName[21];
  wchar_t Club[41];
  char readOutTime[32];
  bool PunchOnly;
  bool convertedTime;
  // Used for manual time input
  int runnerId;
  int relativeFinishTime;
  bool statusOK;
  bool statusDNF;

  vector<string> codeLogData(int row) const;
  static vector<string> logHeader();

  unsigned calculateHash() const;
  bool isManualInput() const {return runnerId != 0;}

  string serializePunches() const;
  void deserializePunches(const string &arg);
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


class SportIdent
{
protected:
  bool readSI6Block(HANDLE hComm, BYTE *data);
  bool readSystemData(SI_StationInfo *si, int retry=2);
  bool readSystemDataV2(SI_StationInfo &si);
  CRITICAL_SECTION SyncObj;

  DWORD ZeroTime; //Used to analyse times. Seconds 0-24h (0-24*3600)
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
  bool checkCRC(BYTE *bf);
  void setCRC(BYTE *bf);

  bool getCard5Data(BYTE *data, SICard &card);
  bool getCard6Data(BYTE *data, SICard &card);
  bool getCard9Data(BYTE *data, SICard &card);

  DWORD GetExtCardNumber(BYTE *data) const;

  void getSI5Data(HANDLE hComm);
  void getSI5DataExt(HANDLE hComm);

  void getSI6Data(HANDLE hComm);
  void getSI6DataExt(HANDLE hComm);
  void getSI9DataExt(HANDLE hComm);

  void analyseSI5Time(BYTE *data, DWORD &time, DWORD &control);
  bool analysePunch(BYTE *data, DWORD &time, DWORD &control);
  void analyseTPunch(BYTE *data, DWORD &time, DWORD &control);

  //Card read waiting to be processed.
  list<SICard> ReadCards;
  HWND hWndNotify;
  DWORD ClassId;

  volatile int tcpPortOpen;
  volatile unsigned int serverSocket;

  bool MonitorSI(SI_StationInfo &si);
  int MonitorTCPSI(WORD port, int localZeroTime);

public:
  SI_StationInfo *findStation(const wstring &com);

  void getInfoString(const wstring &com, vector<wstring> &info);
  bool isPortOpen(const wstring &com);
  void setZeroTime(DWORD zt);
  bool autoDetect(list<int> &ComPorts);
  void stopMonitorThread();

  void startMonitorThread(const wchar_t *com);
  bool getCard(SICard &sic);
  void addCard(const SICard &sic);
  void addPunch(DWORD Time, int Station, int Card, int Mode=0);


  void EnumrateSerialPorts(list<int> &ports);

  void closeCom(const wchar_t *com);
  bool openCom(const wchar_t *com);
  bool tcpAddPort(int port, DWORD zeroTime);

  bool openComListen(const wchar_t *com, DWORD BaudRate);

  SportIdent(HWND hWnd, DWORD Id);
  virtual ~SportIdent();
  friend void start_si_thread(void *ptr);

};

#endif // !defined(AFX_SPORTIDENT_H__F13F5795_8FA9_4CE6_8497_7407CD590139__INCLUDED_)
