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
  char FirstName[21];
  char LastName[21];
  char Club[41];
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
  string ComPort;
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
  bool ReadSI6Block(HANDLE hComm, BYTE *data);
  bool ReadSystemData(SI_StationInfo *si, int retry=2);
  bool ReadSystemDataV2(SI_StationInfo &si);
  CRITICAL_SECTION SyncObj;

  DWORD ZeroTime; //Used to analyse times. Seconds 0-24h (0-24*3600)
  int ReadByte_delay(BYTE &byte,  HANDLE hComm);
  int ReadBytes_delay(BYTE *byte, DWORD buffSize, DWORD len,  HANDLE hComm);
  int ReadBytesDLE_delay(BYTE *byte, DWORD buffSize, DWORD len,  HANDLE hComm);

  int ReadByte(BYTE &byte,  HANDLE hComm);
  int ReadBytes(BYTE *byte, DWORD len,  HANDLE hComm);
  int ReadBytesDLE(BYTE *byte, DWORD len,  HANDLE hComm);

  // Returns zero on failure, number of bytes used on success. 
  int analyzeStation(BYTE *db, SI_StationData &si);

  SI_StationInfo SI_Info[32];
  int n_SI_Info; //Number of structures..
  SI_StationInfo *Current_SI_Info; //Current SI_Info in use (for thread startup);

  WORD CalcCRC(BYTE *data, DWORD length);
  bool CheckCRC(BYTE *bf);
  void SetCRC(BYTE *bf);

  bool GetCard5Data(BYTE *data, SICard &card);
  bool GetCard6Data(BYTE *data, SICard &card);
  bool GetCard9Data(BYTE *data, SICard &card);

  DWORD GetExtCardNumber(BYTE *data) const;

  void GetSI5Data(HANDLE hComm);
  void GetSI5DataExt(HANDLE hComm);

  void GetSI6Data(HANDLE hComm);
  void GetSI6DataExt(HANDLE hComm);
  void GetSI9DataExt(HANDLE hComm);

  void AnalyseSI5Time(BYTE *data, DWORD &time, DWORD &control);
  bool AnalysePunch(BYTE *data, DWORD &time, DWORD &control);
  void AnalyseTPunch(BYTE *data, DWORD &time, DWORD &control);

  //Card read waiting to be processed.
  list<SICard> ReadCards;
  HWND hWndNotify;
  DWORD ClassId;

  volatile int tcpPortOpen;
  volatile unsigned int serverSocket;

  bool MonitorSI(SI_StationInfo &si);
  int MonitorTCPSI(WORD port, int localZeroTime);

public:
  SI_StationInfo *findStation(const string &com);

  void getInfoString(const string &com, vector<string> &info);
  bool IsPortOpen(const string &com);
  void SetZeroTime(DWORD zt);
  bool AutoDetect(list<int> &ComPorts);
  void StopMonitorThread();

  void StartMonitorThread(const char *com);
  bool GetCard(SICard &sic);
  void addCard(const SICard &sic);
  void AddPunch(DWORD Time, int Station, int Card, int Mode=0);


  void EnumrateSerialPorts(list<int> &ports);

  void CloseCom(const char *com);
  bool OpenCom(const char *com);
  bool tcpAddPort(int port, DWORD zeroTime);

  bool OpenComListen(const char *com, DWORD BaudRate);

  SportIdent(HWND hWnd, DWORD Id);
  virtual ~SportIdent();
  friend void start_si_thread(void *ptr);

};

#endif // !defined(AFX_SPORTIDENT_H__F13F5795_8FA9_4CE6_8497_7407CD590139__INCLUDED_)
