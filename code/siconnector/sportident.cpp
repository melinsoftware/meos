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

#include "framework.h"

#include <vector>
#include <string>
#include <tchar.h>
#include <algorithm>
#include <map>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

using namespace std::chrono_literals;

#define EXPORT_CONNECTOR

#include "../connector_interface.h"

using namespace std;

constexpr bool addFakePort = false;

EXPORT MConWString __cdecl getName() {
  static const wchar_t* name = L"SportIdent";
  return name;
}

EXPORT MConWString __cdecl getShortName() {
  static const wchar_t* shortName = L"SI";
  return shortName;
}

namespace SI {
  constexpr int timeConstSecond = 100;
  constexpr int timeConstHour = timeConstSecond * 3600;

  const BYTE STX = 0x02;
  const BYTE ETX = 0x03;
  const BYTE ACK = 0x06;
  const BYTE DLE = 0x10;
  const BYTE WAKEUP = 0xFF;
  const BYTE NAK = 0x15;

  void string2Wide(const string& in, wstring& out) {
    if (in.empty()) {
      out = L"";
      return;
    }
    out.reserve(in.size() + 1);
    out.resize(in.size(), 0);
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, in.c_str(), in.size(), &out[0], out.size() * sizeof(wchar_t));
  }

  struct SICard5Detect {
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

  struct SICard {
    SICard(ConvertedTimeStatus status) {
      convertedTime = status;
    }
    // Clears the card if this == condition or condition is 0
    void clear(const SICard* condition) {
      memset(this, 0, sizeof(SICard));
    }

    void analyseHour12Time(DWORD zeroTime);

    bool empty() const { return CardNumber == 0; }
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
  };

  struct SI_StationData {
    int stationNumber = 0;
    int stationMode = 0;
    bool extended = false;
    bool handShake = false;
    bool autoSend = false;
    int radioChannel = 0;
  };

  struct SI_StationInfo {
    HANDLE threadHandle = 0;
        
    wstring comPort;
    HANDLE hComm = 0;
    COMMTIMEOUTS timeOuts = { 0 };
    OVERLAPPED osStatus = { 0 };
    
    atomic_int status = 0;
    wstring warning;

    vector<SI_StationData> data;
    std::mutex closeMutex;

    bool running() const {
      return status.load() == 1;      
    }

    void requestStop() {
      int run = 1;
      status.compare_exchange_strong(run, 2);      
    }

    void startRun() {
      status.store(1);
    }

    void didStop() {
      status.store(3);
    }

    bool isStopping() {
      return status.load() == 2;
    }

    bool isStopped() {
      return status.load() == 3;
    }

    void resetStatus() {
      status.store(0);
    }

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
  };

  class SportIdent {
  protected:
    vector<wstring> portNames;
    map<wstring, SI_StationInfo> siInfo;
    mutable vector<pair<bool, wstring>> deviceInfo;

    bool useSubsecondMode = false;
    bool readVoltage = true;

    MConAddCard addCardPtr = nullptr;
    MConAddPunch addPunchPtr = nullptr;

    static bool readSI6Block(SI_StationInfo& si, BYTE* data);
    static bool readSystemData(SI_StationInfo* si, int retry = 2);
    static bool readSystemDataV2(SI_StationInfo& si);

    static int readByte_delay(BYTE& byte, SI_StationInfo &si);
    static int readBytes_delay(BYTE* byte, DWORD buffSize, DWORD len, SI_StationInfo &si);
    static int readBytesDLE_delay(BYTE* byte, DWORD buffSize, DWORD len, SI_StationInfo &si);

    static int readByte(BYTE& byte, SI_StationInfo& si);
    static int readBytes(BYTE* byte, DWORD len, SI_StationInfo& si);
    static int readBytesDLE(BYTE* byte, DWORD len, SI_StationInfo& si);

    static int writeBytes(const BYTE* byte, DWORD len, SI_StationInfo& si);

    // Returns zero on failure, number of bytes used on success. 
    static int analyzeStation(BYTE* db, SI_StationData& si);

    static WORD calcCRC(BYTE* data, DWORD length);
    static bool checkCRC(BYTE* bf, DWORD maxLen);
    static void setCRC(BYTE* bf);

    bool getCard5Data(BYTE* data, SICard& card);
    bool getCard6Data(BYTE* data, SICard& card);
    bool getCard9Data(BYTE* data, SICard& card);

    DWORD GetExtCardNumber(const BYTE* data) const;

    void getSI5Data(SI_StationInfo& si);
    void getSI5DataExt(SI_StationInfo& si);

    void getSI6Data(SI_StationInfo& si);
    void getSI6DataExt(SI_StationInfo& si);
    void getSI9DataExt(SI_StationInfo& si);

    void analyseSI5Time(BYTE* data, DWORD& time, DWORD& control);
    bool analysePunch(BYTE* data, DWORD& time, DWORD& control, bool subSecond);
    void analyseTPunch(BYTE* data, DWORD& time, DWORD& control);

    //Card read waiting to be processed.
    bool monitorSI(SI_StationInfo& si, wstring &warningOut);

    bool readCom(SI_StationInfo& si, wstring& warningOut, DWORD dwCommEvent);


    const SI_StationInfo* findStation(const wstring& com) const {
      auto res = siInfo.find(com);
      if (res != siInfo.end())
        return &res->second;
      else
        return nullptr;
    }

    SI_StationInfo* findStation(const wstring& com) {
      auto res = siInfo.find(com);
      if (res != siInfo.end())
        return &res->second;
      else
        return nullptr;
    }

    /** Log debug data. */
    static void debugLog(const wchar_t* ptr) {
      // Do nothing
    }

    const vector<pair<bool, wstring>> &getInfoString(const wstring& com) const;

    bool isAnyOpenUnkownUnit() const;

    void closeCom(const wstring &com);

    static void closeComStat(SI_StationInfo *si);
    /** 0 = fail, 1 = OK, 2 = no response */
    int openCom(const wstring &com, bool listenOnly, int baudRate, MConLogMessage logger);

    static int openComStat(SI_StationInfo *si,
                           const wstring &com,
                           bool listenOnly,
                           int baudRate,
                           MConLogMessage logger);

    static void openDetectClose(SI_StationInfo *si) {
      int s = openComStat(si, si->comPort, false, 0, nullptr);
      if (s) {
        closeComStat(si);
      }
    }

    void readRawData(const wstring& file);
    
    void addCard(const SICard& sic);
    void addPunch(DWORD Time, int Station, int Card, int Mode = 0);

  public:
    SportIdent();
    ~SportIdent();

    void init(MConAddCard addCard, MConAddPunch addPunch) {
      addCardPtr = addCard;
      addPunchPtr = addPunch;
    }

    void setSubSecondMode(bool subSec) { useSubsecondMode = subSec; }
    int enumerateDevices(int maxNumEntries, MConWString* unitVectorDescr, bool* unitVectorConnected);
    void status(int deviceIndex, MConDeviceStatus* status) const;

    int autoDetectToOpen(int maxNumEntries, int *toOpen) const;
    int openConnection(int deviceIndex, bool listenOnly, int baudRate, MConLogMessage logger);
    void closeConnection(int deviceIndex);

    void unload();

    MConWString monitorThread(int deviceIndex);
  };
}

using namespace SI;

SportIdent::SportIdent() {
}

SportIdent::~SportIdent() = default;

int SportIdent::autoDetectToOpen(int maxNumEntries, int *toOpen) const {
  if (portNames.empty())
    return 0;

  vector<SI_StationInfo> tmpInfo(portNames.size());
  vector<std::thread> detectionThreads(portNames.size());

  for (int i = 0; i < portNames.size(); i++) {
    auto *ptr = findStation(portNames[i]);
    if (ptr && ptr->hComm)
      continue; // Already open

    tmpInfo[i].comPort = portNames[i];
    detectionThreads[i] = thread(openDetectClose, &tmpInfo[i]);
  }

  for (int i = 0; i < detectionThreads.size(); i++) {
    if (!tmpInfo[i].comPort.empty())
      detectionThreads[i].join();
  }

  int ix = 0;
  for (int i = 0; i < tmpInfo.size(); i++) {
    if (tmpInfo[i].data.empty() || tmpInfo[i].data[0].stationNumber >= 1024 || tmpInfo[i].data[0].stationMode > 15 ||
        !(tmpInfo[i].data[0].autoSend || tmpInfo[i].data[0].handShake))
      continue; // Invalid
    if (ix < maxNumEntries) {
      toOpen[ix++] = i;
    }
  }
  return ix;
}

// CRC algoritm used by SPORTIdent GmbH
// implemented for MeOS in C++
WORD SportIdent::calcCRC(BYTE* data, DWORD count)
{
  // Return 0 for no or one data byte
  if (count < 2)
    return 0;

  size_t index = 0;
  WORD crc = (WORD(data[index]) << 8) + WORD(data[index + 1]);
  index += 2;
  // Return crc for two data bytes
  if (count == 2)
    return crc;

  WORD value;
  for (size_t k = count >> 1; k > 0; k--) {
    if (k > 1) {
      value = (WORD(data[index]) << 8) + WORD(data[index + 1]);
      index += 2;
    }
    else  // If the number of bytes is odd, complete with 0.
      value = (count & 1) ? data[index] << 8 : 0;

    for (int j = 0; j < 16; j++) {
      if (crc & 0x8000) {
        crc <<= 1;
        if (value & 0x8000)
          crc++;
        crc ^= 0x8005;
      }
      else {
        crc <<= 1;
        if (value & 0x8000)
          crc++;
      }
      value <<= 1;
    }
  }
  return crc;
}

void SportIdent::setCRC(BYTE* bf)
{
  DWORD len = bf[1];
  WORD crc = calcCRC(bf, len + 2);
  bf[len + 2] = HIBYTE(crc);
  bf[len + 3] = LOBYTE(crc);
}

bool SportIdent::checkCRC(BYTE* bf, DWORD maxLen)
{
  DWORD len = min(DWORD(bf[1]), maxLen);
  WORD crc = calcCRC(bf, len + 2);

  return bf[len + 2] == HIBYTE(crc) && bf[len + 3] == LOBYTE(crc);
}

bool SportIdent::readSystemData(SI_StationInfo* si, int retry)
{
  BYTE c[16];
  BYTE buff[256];

  c[0] = STX;
  c[1] = 0x83;
  c[2] = 0x02;
  c[3] = 0x70; //Request address 0x70
  c[4] = 0x06; //And 6 bytes
  //Address 0x74 = protocoll settings
  //Address 0x71 = Programming ctrl/readout/finish etc.
  setCRC(c + 1);
  c[7] = ETX;

  DWORD written = 0;
  written = writeBytes(c, 8, *si);
  Sleep(100);
  memset((void*)buff, 0, 30);
  DWORD offset = 0;
  readBytes_delay(buff, sizeof(buff), 15, *si);
  if (buff[0] == 0xFF && buff[1] == STX)
    offset++;
  if (1) {
    if (checkCRC(LPBYTE(buff + 1 + offset), 100)) {
      si->data.resize(1);
      SI_StationData& da = si->data[0];
      da.stationNumber = 511 & MAKEWORD(buff[4 + offset], buff[3 + offset]);
      BYTE PR = buff[6 + 4 + offset];
      BYTE MO = buff[6 + 1 + offset];
      da.extended = (PR & 0x1) != 0;
      da.handShake = (PR & 0x4) != 0;
      da.autoSend = (PR & 0x2) != 0;
      da.stationMode = MO & 0xf;
    }
    else if (retry > 0)
      return readSystemData(si, retry - 1);
    else return false;
  }
  else if (retry > 0)
    return readSystemData(si, retry - 1);
  else
    return false;

  return true;
}

bool SportIdent::readSystemDataV2(SI_StationInfo& si)
{
  BYTE c[16];
  BYTE buff[4096];

  int maxbytes = 256;
  while (readByte(c[0], si) == 1 && maxbytes > 0) {
    maxbytes--;
  }

  // 02 83 01 00 80 bf 17 03

  c[0] = STX;
  c[1] = 0x83;
  c[2] = 0x02;
  c[3] = 0;
  c[4] = 0x80;
  c[5] = 0xbf;
  c[6] = 0x17;
  setCRC(c + 1);
  c[7] = ETX;

  DWORD written = writeBytes(c, 8, si);
  Sleep(100);
  memset((void*)buff, 0, sizeof(buff));
  //  DWORD offset = 0;
  int consumed = 0;
  int read = readBytes_delay(buff, sizeof(buff), -1, si);
  const int requested = 0x80;
  while ((read - consumed) >= requested) {
    while (consumed < read&& buff[consumed] != STX)
      consumed++;

    BYTE* db = buff + consumed;
    if ((read - consumed) >= requested && db[0] == STX) {
      si.data.push_back(SI_StationData());
      int used = analyzeStation(db, si.data.back());
      if (used == 0) {
        si.data.pop_back(); // Not valid
        break;
      }
      else consumed += used;

      // Test duplicate units: si.data.push_back(si.data.back());
    }
    else break;
  }

  return si.data.size() > 0;
}

int SportIdent::analyzeStation(BYTE* db, SI_StationData& si) {
  DWORD size = 0;
  DWORD addr = 0x70;
  if (checkCRC(LPBYTE(db + 1), 256)) {
    size = DWORD(db[2]) + 6;

    bool dongle = db[0x11] == 0x6f && db[0x12] == 0x21;

    if (dongle) {
      BYTE PR = db[69];
      BYTE MO = db[6 + 1 + addr];
      si.extended = (PR & 0x1) != 0;
      si.handShake = false;
      si.autoSend = db[68] == 1;
      si.stationMode = MO & 0xf;
      si.radioChannel = db[58] & 0x1;
      si.stationNumber = 0;
    }
    else {
      si.stationNumber = 511 & MAKEWORD(db[4], db[3]);
      BYTE PR = db[6 + 4 + addr];
      BYTE MO = db[6 + 1 + addr];
      si.extended = (PR & 0x1) != 0;
      si.handShake = (PR & 0x4) != 0;
      si.autoSend = (PR & 0x2) != 0;
      si.stationMode = MO & 0xf;
      si.radioChannel = 0;
    }
  }

  return size;
}

string decode(BYTE* bf, int read)
{
  string st;
  for (int k = 0; k <= read; k++) {
    if (bf[k] == STX)
      st += "STX ";
    else if (bf[k] == ETX)
      st += "ETX ";
    else if (bf[k] == ACK)
      st += "ACK ";
    else if (bf[k] == DLE)
      st += "DLE ";
    else {
      char d[10];
      sprintf_s(d, "%02X  ", bf[k]);
      st += d;
    }
  }
  return st;
}

int SportIdent::writeBytes(const BYTE* byte, DWORD len, SI_StationInfo& si) {
  DWORD dwWritten = 0;
  if (WriteFile(si.hComm, byte, len, &dwWritten, &si.osStatus)) {
    return dwWritten;
  }
  else {
    if (GetLastError() == ERROR_IO_PENDING) {
      DWORD dwWait = WaitForSingleObject(si.osStatus.hEvent, 2000);
      
      if (dwWait == WAIT_OBJECT_0) {
        if (GetOverlappedResult(si.hComm, &si.osStatus, &dwWritten, true)) {
          return dwWritten;
        }
      }
      else {
        int error = GetLastError();

      }
    }
  }
  return 0;
}

int SportIdent::readByte_delay(BYTE& byte, SI_StationInfo& si)
{
  byte = 0;
  return readBytes_delay(&byte, 1, 1, si);
}

int SportIdent::readByte(BYTE& byte, SI_StationInfo& si) {
  byte = 0;

  if (!si.hComm)
    return -1;

  DWORD dwRead;

  if (ReadFile(si.hComm, &byte, 1, &dwRead, &si.osStatus))
  {
#ifdef DEBUG_SI2
    char t[64];
    sprintf_s(t, 64, "read=%02X\n", (int)byte);
    debugLog(t);
#endif
    if (dwRead)
      return 1;
    else 
      return 0;
  }
  else {
    if (GetLastError() == ERROR_IO_PENDING) {
      DWORD dwWait = WaitForSingleObject(si.osStatus.hEvent, 1000);

      if (dwWait == WAIT_OBJECT_0) {
        if (GetOverlappedResult(si.hComm, &si.osStatus, &dwRead, true)) {
          if (dwRead)
            return 1;
          else
            return 0;
        }
      }
    }
  }
    
  return -1;
}

int SportIdent::readBytes_delay(BYTE* byte, DWORD buffSize, DWORD len, SI_StationInfo& si) {
  int read = 0;
  int d;
  bool autoLen = false;
  if (len == -1) {
    len = min<int>(buffSize, 10);
    autoLen = true;
  }
  int toread = len;

  for (d = 0; d < 7 && read < toread; d++) {
    int maxToRead = min<int>(buffSize - read, len);
    if (maxToRead <= 0)
      return read;

    int r = readBytes(byte + read, maxToRead, si);

    if (r == -1) {
      if (read > 0)
        return read;
      return -1;
    }
    read += r;
    if (read == 1 && byte[0] == NAK)
      return read;

    if (autoLen && r == len) {
      int rloop;
      Sleep(100);
      while (read < int(buffSize) && (rloop = readBytes(byte + read, min<int>(16, buffSize - read), si)) > 0) {
        read += rloop;
      }
    }

    if (read < toread) {
      len = toread - read;
      Sleep(100);
    }
  }
#ifdef DEBUG_SI2
  char t[64];
  sprintf_s(t, 64, "retry=%d\n", d);
  debugLog(t);

  for (int k = 0; k < read; k++) {
    char t[64];
    sprintf_s(t, 64, "mreadd=%02X\n", (int)byte[k]);
    debugLog(t);
  }
#endif

  return read;
}

int SportIdent::readBytes(BYTE* byte, DWORD len, SI_StationInfo& si)
{
  if (!si.hComm)
    return -1;

  DWORD dwRead;

  if (ReadFile(si.hComm, byte, len, &dwRead, &si.osStatus)) {
#ifdef DEBUG_SI2
    for (int k = 0; k < dwRead; k++) {
      char t[64];
      sprintf_s(t, 64, "mread=%02X\n", (int)byte[k]);
      debugLog(t);
    }
#endif
    return dwRead;
  }
  else {
    if (GetLastError() == ERROR_IO_PENDING) {
      DWORD dwWait = WaitForSingleObject(si.osStatus.hEvent, 1000);

      if (dwWait == WAIT_OBJECT_0) {
        if (GetOverlappedResult(si.hComm, &si.osStatus, &dwRead, true)) {
          return dwRead;
        }
      }
    }
  }

  return -1;
}


int SportIdent::readBytesDLE_delay(BYTE* byte, DWORD buffSize, DWORD len, SI_StationInfo& si)
{
  int read = 0;
  int toread = len;
  int d;

  for (d = 0; d < 15 && read < toread; d++) {
    int maxToRead = max(buffSize - read, len);
    int r = readBytes(byte + read, maxToRead, si);

    if (r == -1) return -1;

    read += r;

    if (read < toread)
    {
      len = toread - read;
      Sleep(100);
    }
  }
#ifdef DEBUG_SI2
  char t[64];
  sprintf_s(t, 64, "retry=%d\n", d);
  debugLog(t);
#endif

  return read;
}


int SportIdent::readBytesDLE(BYTE* byte, DWORD len, SI_StationInfo& si)
{
  if (!si.hComm)
    return -1;

  DWORD dwRead;


  dwRead = readBytes(byte, len, si);

  if (dwRead > 0) {
    DWORD ip = 0;
    DWORD op = 0;

    for (ip = 0; ip < dwRead - 1; ip++) {
      if (byte[ip] == DLE)
        byte[op++] = byte[++ip];
      else
        byte[op++] = byte[ip];
    }

    if (ip < dwRead) {
      if (byte[ip] == DLE)
        readByte(byte[op++], si);
      else
        byte[op++] = byte[ip];
    }

    if (op < len)
      return op + readBytesDLE(byte + op, len - op, si);
    else return len;
  }

  return dwRead;
}

bool SportIdent::monitorSI(SI_StationInfo& si, wstring &warningOut) {
  HANDLE hComm = si.hComm;

  if (!hComm) {
    warningOut = L"No communication handle";
    return false;
  }
   
  DWORD dwCommEvent = 0;

  if (!SetCommMask(hComm, EV_RXCHAR)) {
    warningOut = L"Failed to set CommMask";
    return false;
  }

  while(si.running()) {
    if (WaitCommEvent(hComm, &dwCommEvent, &si.osStatus)) {
      if (!readCom(si, warningOut, dwCommEvent))
        return false;
    }
    else {
      if (GetLastError() == ERROR_IO_PENDING) {
        bool gotData = false;
        while (si.running() && !gotData) {
          DWORD dwWait = WaitForSingleObject(si.osStatus.hEvent, 500);

          switch (dwWait) {
          case WAIT_OBJECT_0:
            gotData = true;
            // Event triggered, check result
            if (GetOverlappedResult(hComm, &si.osStatus, &dwCommEvent, FALSE)) {
              if (!readCom(si, warningOut, dwCommEvent))
                return false;
            }
            break;

          case WAIT_TIMEOUT:
            continue;

          default:
            warningOut = L"Error in WaitForSingleObject";
            return false;
          }
        }
      }
      else {
        warningOut = L"Kommunikationen med en SI-enhet avbröts.";
        return false;
      }
    }
  }
    
  return true;
}

bool SportIdent::readCom(SI_StationInfo& si, wstring& warningOut, DWORD dwCommEvent) {
  HANDLE hComm = si.hComm;
  DWORD dwRead;
  BYTE  chRead;

  do {
    if ((dwRead = readByte(chRead, si)) != -1)
    {
      // A byte has been read; process it.
      if (chRead == STX && readByte(chRead, si))
      {
        switch (chRead)
        {
        case 0xD3: {//Control Punch, extended mode.
          BYTE bf[32];
          bf[0] = chRead;
          readBytes(bf + 1, 17, si);
          if (checkCRC(LPBYTE(bf), 32))
          {
            WORD Station = MAKEWORD(bf[3], bf[2]);

            DWORD ShortCard = MAKEWORD(bf[7], bf[6]);
            DWORD Series = bf[5];

            /*for (int i = 0; i < 32; i++) {
              uint32_t c = bf[i];
              char xxx[20];
              sprintf_s(xxx, "%X ", c);
              OutputDebugStringA(xxx);
            }
            OutputDebugStringA("\n");*/

            DWORD Card = MAKELONG(MAKEWORD(bf[7], bf[6]), MAKEWORD(bf[5], bf[4]));

            if (Series <= 4 && Series >= 1)
              Card = ShortCard + 100000 * Series;

            DWORD Time = 0;
            if (bf[8] & 0x1) Time = timeConstHour * 12;
            Time += MAKEWORD(bf[10], bf[9]) * timeConstSecond;
            uint8_t tss = bf[11]; // Sub second 1/256 seconds
            
            int tenth = (((100 * tss) / 256) + 4) / (100 / timeConstSecond);
            Time += tenth;
            
            int mode = si.stationMode();
            if (mode == 11) {
              int rawMode = bf[12] & 0xF;
              switch (rawMode) {
              case 2: // CTRL
              case 4: // FIN
              case 3: // STA
              case 10: // CHK
              case 7: // CLR
                mode = rawMode;
              }
            }

#ifdef DEBUG_SI
            char str[128];
            sprintf_s(str, "EXTENDED: Card = %d, Station = %d, StationMode = %d", Card, Station, si.StationMode);
            MessageBox(NULL, str, NULL, MB_OK);
#endif              
            addPunch(Time, Station & 511, Card & 0x00FFFFFF, mode);
          }
          break;
        }
        case 0x53: { //Control Punch, old mode
          BYTE bf[32];
          bf[0] = chRead;
          //Sleep(200);
          readBytesDLE(bf + 1, 30, si);

          BYTE Station = bf[2];
          BYTE Series = bf[3];
          WORD Card = MAKEWORD(bf[5], bf[4]);


          DWORD DCard;

          if (Series == 1)
            DCard = Card;
          else if (Series <= 4)
            DCard = Card + 100000 * Series;
          else
            DCard = MAKELONG(Card, Series);

          //if (Series!=1)
          //	Card+=100000*Series;

          DWORD Time = MAKEWORD(bf[8], bf[7]) * timeConstSecond;
          BYTE p = bf[1];
          if (p & 0x1) Time += timeConstHour * 12;

#ifdef DEBUG_SI
          char str[128];
          sprintf_s(str, "OLD: Card = %d, Station = %d, StationMode = %d", DCard, Station, si.StationMode);
          MessageBox(NULL, str, NULL, MB_OK);
#endif
          addPunch(Time, Station, DCard, si.stationMode());
          break;
        }
        case 0xE7: {//SI5/6 removed
          BYTE bf[32];
          readBytes(bf + 1, 10, si);
          break;
        }

        case 0xE6: {
          BYTE bf[32];
          bf[0] = 0xE6;
          readBytes(bf + 1, 10, si);

          //ReadByte(chRead); //ETX!
          if (checkCRC(LPBYTE(bf), 32))
            getSI6DataExt(si);

          break;
        }
        case 0xE5: {
          BYTE bf[32];
          bf[0] = 0xE5;
          readBytes(bf + 1, 10, si);

          if (checkCRC(LPBYTE(bf), 32))
            getSI5DataExt(si);

          break;
        }
        case 0x46:
          readByte(chRead, si); //0x49?!
          readByte(chRead, si); //ETX!
          getSI5Data(si);
          break;
        case 0x66: { //STX, 66h, CSI, TI, TP, CN3, CN2, CN1, CN0, ETX
          BYTE bf[32];
          //SICard5Detect si5;
          //si5.code=0xE5;
          bf[0] = 0xE6;
          readBytesDLE(bf + 1, 8, si);

          getSI6Data(si);

          break;
        }
        case 0xB1: {
          BYTE bf[200];
          bf[0] = chRead;
          readBytes(bf + 1, 200, si);
          warningOut = L"Programmera stationen utan AUTOSEND";
          return false;
        }
                 break;

        case 0xE1: {
          BYTE bf[200];
          bf[0] = chRead;
          readBytes(bf + 1, 200, si);
          warningOut = L"Programmera stationen utan AUTOSEND";
          return false;
        }
                 break;
        case 0xE8: {
          BYTE bf[32];
          bf[0] = 0xE8;
          readBytes(bf + 1, 10, si);

          //ReadByte(chRead); //ETX!
          if (checkCRC(LPBYTE(bf), 32))
            getSI9DataExt(si);

          break;
        }
                 //	MessageBox(NULL, "SI-card not supported", NULL, MB_OK);
                 //	break;
        default:
          BYTE bf[128];
          bf[0] = chRead;
          int rb = readBytes(bf + 1, 120, si);

          if (chRead == 0xEF) {
            warningOut = L"Programmera stationen utan AUTOSEND";
            return false;
          }
          //MessageBox(NULL, st.c_str(), "Unknown SI response", MB_OK);
        }
      }
    }
    else {
      warningOut = L"Error in ReadFile";
      return false;
    }
  } while (dwRead);

  return true;
}

void SportIdent::getSI5DataExt(SI_StationInfo& si)
{
  BYTE c[128];

  c[0] = STX;
  c[1] = 0xB1;
  c[2] = 0x00;
  setCRC(c + 1);
  c[5] = ETX;

  DWORD written = writeBytes(c, 5, si);

  if (written == 5)
  {
    Sleep(150);
    BYTE bf[256];
    memset(bf, 0, 256);

    readBytes(bf, 128 + 8, si);

    if (bf[0] == STX && bf[1] == 0xB1)
    {
      if (checkCRC(bf + 1, 254))
      {
        c[0] = ACK;
        written = writeBytes(c, 1, si);

        BYTE* Card5Data = bf + 5;
        SICard card(ConvertedTimeStatus::Hour12);
        getCard5Data(Card5Data, card);
        addCard(card);
      }
    }
    //Sleep(1000);
  }
}


void SportIdent::getSI6DataExt(SI_StationInfo& si)
{
  BYTE b[128 * 8];
  memset(b, 0, 128 * 8);
  BYTE c[16];
  //	STX, 0xE1, 0x01, BN, CRC1,
  //CRC0, ETX
  
 debugLog(L"STARTREAD EXT-");

  int blocks[7] = { 0,6,7,2,3,4,5 };
  DWORD written = 0;

  for (int k = 0; k < 7; k++)
  {
    c[0] = STX;
    c[1] = 0xE1;
    c[2] = 0x01;
    c[3] = blocks[k];
    setCRC(c + 1);
    c[6] = ETX;

    written = writeBytes(c, 7, si);

    if (written == 7) {
      Sleep(50);
      BYTE bf[256];
      memset(bf, 0, 256);

      int read = readBytes(bf, 128 + 9, si);

      if (read == 0) {
        debugLog(L"TIMING");
        Sleep(300);
        read = readBytes(bf, 128 + 9, si);
      }

      if (bf[0] == STX && bf[1] == 0xE1) {
        if (checkCRC(bf + 1, 250)) {
          memcpy(b + k * 128, bf + 6, 128);

          LPDWORD ptr = LPDWORD(bf + 6);
          if (ptr[31] == 0xEEEEEEEE)
            break; //No need to read more
        }
        else {
          debugLog(L"-FAIL-");
          return;
        }
      }
      else {
        debugLog(L"-FAIL-");
        return;
      }
    }
  }

  c[0] = ACK;
  written = writeBytes(c, 1, si);

  debugLog(L"-ACK-");

  SICard card(ConvertedTimeStatus::Hour24);
  getCard6Data(b, card);
  addCard(card);
}

void SportIdent::getSI9DataExt(SI_StationInfo& si) {
  constexpr int maxblock = 6;
  BYTE b[128 * maxblock];
  memset(b, 0, 128 * maxblock);
  BYTE c[16];
  int miliVolt = 0;
  //	STX, 0xE1, 0x01, BN, CRC1,
  //CRC0, ETX
  debugLog(L"STARTREAD9 EXT-");

  int blocks_8_9_p_t[2] = { 0,1 };
  int blocks_10_11_SIAC[6] = { 0,1,4,5,6,7 }; // Block 1 added,to handle SIACs using Beacon
  int limit = 1;
  int* blocks = blocks_8_9_p_t;
  bool readBattery = false;
  DWORD written = 0;

  for (int k = 0; k < limit; k++) {
    c[0] = STX;
    c[1] = 0xEF;
    c[2] = 0x01;
    c[3] = blocks[k];
    setCRC(c + 1);
    c[6] = ETX;

    written = writeBytes(c, 7, si);

    if (written == 7) {
      Sleep(50);
      BYTE bf[256];
      memset(bf, 0, 256);

      int read = readBytes(bf, 128 + 9, si);

      if (read == 0) {
        debugLog(L"TIMING");
        Sleep(300);
        read = readBytes(bf, 128 + 9, si);
      }

      if (bf[0] == STX && bf[1] == 0xEf) {
        if (checkCRC(bf + 1, 200)) {
          memcpy(b + k * 128, bf + 6, 128);
          if (k == 0) {
            int series = b[24] & 15;
            if (series == 15) {
              int nPunch = min<uint32_t>(int(b[22]), 128u);
              blocks = blocks_10_11_SIAC;
              limit = 2 + (nPunch + 31) / 32;   // Read Block 0, Block 1 + punches
              if (limit > maxblock)
                limit = maxblock;
              int cardNo = GetExtCardNumber(b);
              if (cardNo > 8000000 && cardNo < 9000000) {
                readBattery = readVoltage;
              }
            }
            else {
              limit = 2; // Card 8, 9, p, t
            }
          }

        }
        else {
          debugLog(L"-FAIL-");
          return;
        }
      }
      else {
        debugLog(L"-FAIL-");
        return;
      }
    }
  }

  if (readBattery) {
    c[0] = STX;
    c[1] = 0xEA;
    c[2] = 0x05;
    c[3] = 0x7E;
    c[4] = 0x05;
    c[5] = 0x05;
    c[6] = 0x05;
    c[7] = 0x05;
    c[10] = ETX;
    setCRC(c + 1);

    written = writeBytes(c, 11, si); // Measure batt voltage

    if (written == 11) {
      BYTE bf[256];
      int read = readBytes(bf, 9, si);
      if (read == 0) {
        debugLog(L"TIMING");
        Sleep(300);
        read = readBytes(bf, 9, si);
      }

      if (read == 9) {
        /*for (int i = 0; i < read; i++) {
          char xx[20];
          sprintf_s(xx, "%02x ", bf[i]);
          OutputDebugStringA(xx);
        }
        OutputDebugStringA("\n\n");*/

        c[0] = STX;
        c[1] = 0xEF;
        c[2] = 0x01;
        c[3] = 3;
        setCRC(c + 1);
        c[6] = ETX;

        written = writeBytes(c, 7, si);

        memset(bf, 0, 256);
        int read = readBytes(bf, 128 + 9, si);
        if (read == 0) {
          debugLog(L"TIMING");
          Sleep(300);
          read = readBytes(bf, 128 + 9, si);
        }

        if (bf[0] == STX && bf[1] == 0xEf) {
          /*
          for (int i = 0; i < read; i++) {
            char xx[20];
            sprintf_s(xx, "%02x ", bf[i]);
            OutputDebugStringA(xx);
            if (i%20==19)
              OutputDebugStringA("\n");
          }
          OutputDebugStringA("\n\n");
          */
          if (checkCRC(bf + 1, 200)) {
            BYTE battVoltageRow = bf[77];
            double voltage = 1.9 + (battVoltageRow * 0.09);
            miliVolt = int(1000 * voltage);

            if (miliVolt > 5000)
              miliVolt = 900; // Not allowed
            /*char xx[30];
            sprintf_s(xx, "V = %f\n\n", voltage);
            OutputDebugStringA(xx);*/
          }
        }
      }

    }
    //02 EA 05 7E 05 05 05 05 B2 31 03

  }


  c[0] = ACK;
  written = writeBytes(c, 1, si);

  debugLog(L"-ACK-");

  SICard card(ConvertedTimeStatus::Hour24);
  if (getCard9Data(b, card)) {
    card.miliVolt = miliVolt;
    addCard(card);
  }
}

bool SportIdent::readSI6Block(SI_StationInfo& si, BYTE* data)
{
  BYTE bf[256];
  memset(bf, 0, 256);

  int read = readBytesDLE(bf, 4, si);

  if (read == 0) {
    debugLog(L"TIMING");
    Sleep(1000);
    read = readBytesDLE(bf, 4, si);
  }

  if (bf[0] == STX && bf[1] == 0x61)
  {
    BYTE sum = bf[2] + bf[3]; //Start calc checksum

    read = readBytesDLE(bf, 128, si);
    BYTE cs, etx;
    readBytesDLE(&cs, 1, si);
    readByte(etx, si);

    for (int i = 0; i < 128; i++)
      sum += bf[i];

    if (sum == cs)
    {
      memcpy(data, bf, 128);
      return true;
    }
    else return false;
  }

  return false;
}

void SportIdent::getSI6Data(SI_StationInfo& si)
{
  BYTE b[128 * 8];
  memset(b, 0, 128 * 8);
  BYTE c[16];
  //	STX, 0xE1, 0x01, BN, CRC1,
  //CRC0, ETX
  debugLog(L"STARTREAD-");

  //int blocks[3]={0,6,7};
  DWORD written = 0;


  c[0] = STX;
  c[1] = 0x61;
  c[2] = 0x08;
  c[3] = ETX;

  written = writeBytes(c, 4, si);
  bool compact = false;
  if (written == 4) {
    Sleep(500);

    for (int k = 0; k < 8; k++) {

      if (!readSI6Block(si, b + k * 128)) {
        if (k <= 2) {
          debugLog(L"-FAIL-");
          return;
        }
        else
          break;
      }
      if (k > 2)
        compact = true;
    }
  }

  if (compact) {
    BYTE b2[128 * 7];
    // 192 punches, sicard6 star
    for (int k = 0; k < 128 * 8; k++) {
      if (k < 128)
        b2[k] = b[k];
      else if (k >= 256 && k < 128 * 6)
        b2[k + 128] = b[k];
      else if (k >= 128 * 6)
        b2[k - 128 * 5] = b[k];
    }
    memcpy(b, b2, sizeof(b2));
  }

  c[0] = ACK;
  written = writeBytes(c, 1, si);

  debugLog(L"-ACK-");
  SICard card(ConvertedTimeStatus::Hour24);
  getCard6Data(b, card);

  addCard(card);
}

void SportIdent::getSI5Data(SI_StationInfo& si) {
  BYTE c[128];

  c[0] = STX;
  c[1] = 0x31;
  c[2] = ETX;

  DWORD written = writeBytes(c, 3, si);

  if (written == 3)
  {
    Sleep(900);
    BYTE bf[256];
    memset(bf, 0, 256);

    int read = readBytesDLE(bf, 3, si);

    if (read == 0)
    {
      Sleep(1000);
      read = readBytesDLE(bf, 3, si);
    }

    if (bf[0] == STX && bf[1] == 0x31)
    {

      BYTE sum = bf[2]; //Start calc checksum

      read = readBytesDLE(bf, 128, si);
      BYTE cs, etx;
      readBytesDLE(&cs, 1, si);
      readByte(etx, si);

      for (int i = 0; i < 128; i++)
        sum += bf[i];

      if (sum == cs)
      {
        c[0] = ACK;
        written = writeBytes(c, 1, si);

        SICard card(ConvertedTimeStatus::Hour12);
        getCard5Data(bf, card);

        addCard(card);
      }
    }
  }
}

bool SportIdent::getCard5Data(BYTE* data, SICard& card)
{
  /*	ofstream fout("si.txt");

    for(int m=0;m<128;m++)
    {
      if (m%16==0) fout << endl;

      char bf[16];
      sprintf(bf, "%02x ", (DWORD)data[m]);
      fout << bf;
    }

    fout << endl;
    return 0;
  */
  memset(&card, 0, sizeof(card));

  DWORD number = MAKEWORD(data[5], data[4]);

  if (data[6] == 1)
    card.CardNumber = number;
  else
    card.CardNumber = 100000 * data[6] + number;

  data += 16;

  card.convertedTime = ConvertedTimeStatus::Hour12;
  analyseSI5Time(data + 3, card.StartPunch.Time, card.StartPunch.Code);
  analyseSI5Time(data + 5, card.FinishPunch.Time, card.FinishPunch.Code);
  analyseSI5Time(data + 9, card.CheckPunch.Time, card.CheckPunch.Code);

  card.nPunch = data[7] - 1;

  data += 16;

  for (DWORD k = 0; k < card.nPunch; k++) {
    if (k < 30) {
      DWORD basepointer = 3 * (k % 5) + 1 + (k / 5) * 16;
      DWORD code = data[basepointer];
      DWORD time;
      DWORD slask;
      analyseSI5Time(data + basepointer + 1, time, slask);

      card.Punch[k].Code = code;
      card.Punch[k].Time = time;
    }
    else if (k < 36) {
      card.Punch[k].Code = data[(k - 30) * 16];
      card.Punch[k].Time = -1; //No time for extra punch
    }
  }

  return true;
}

DWORD SportIdent::GetExtCardNumber(const BYTE* data) const {
  DWORD cnr = 0;
  BYTE* p = (BYTE*)&cnr;
  p[0] = data[27];
  p[1] = data[26];
  p[2] = data[25];

  return cnr;
}

bool SportIdent::getCard9Data(BYTE* data, SICard& card)
{
  /*ofstream fout("si.txt");

  for(int m=0;m<128*2;m++)
  {
    if (m%16==0) fout << endl;

    char bf[16];
    sprintf_s(bf, 16, "%02x ", (DWORD)data[m]);
    fout << bf;
  }

  fout << endl;*/
  //return 0;

  memset(&card, 0, sizeof(card));


  card.CardNumber = GetExtCardNumber(data);

  int series = data[24] & 15;

  card.convertedTime = ConvertedTimeStatus::Hour24;
  analysePunch(data + 12, card.StartPunch.Time, card.StartPunch.Code, useSubsecondMode);
  analysePunch(data + 16, card.FinishPunch.Time, card.FinishPunch.Code, useSubsecondMode);
  analysePunch(data + 8, card.CheckPunch.Time, card.CheckPunch.Code, false);

  if (series == 1) {
    // SI Card 9
    card.nPunch = min(int(data[22]), 50);
    for (unsigned k = 0; k < card.nPunch; k++) {
      analysePunch(14 * 4 + data + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false);
    }
  }
  else if (series == 2) {
    // SI Card 8
    card.nPunch = min(int(data[22]), 30);
    for (unsigned k = 0; k < card.nPunch; k++) {
      analysePunch(34 * 4 + data + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false);
    }
  }
  else if (series == 4) {
    // pCard
    card.nPunch = min(int(data[22]), 20);
    for (unsigned k = 0; k < card.nPunch; k++) {
      analysePunch(44 * 4 + data + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false);
    }
  }
  else if (series == 6) {
    // tCard (Pavel Kazakov)
    card.nPunch = min(int(data[22]), 25);
    for (unsigned k = 0; k < card.nPunch; k++) {
      analyseTPunch(14 * 4 + data + 8 * k, card.Punch[k].Time, card.Punch[k].Code);
    }

    // Remove unused punches
    while (card.nPunch != 0 && card.Punch[card.nPunch - 1].Code == -1) {
      card.nPunch--;
    }
  }
  else if (series == 15) {
    // Card 10, 11, SIAC
    card.nPunch = min(int(data[22]), 128);
    for (unsigned k = 0; k < card.nPunch; k++) {
      analysePunch(data + 256 + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false);  // Modified since Block 1 is also read now
    }
  }
  else
    return false;

  return true;
}

// Method by Pavel Kazakov.
void SportIdent::analyseTPunch(BYTE* data, DWORD& time, DWORD& control) {
  if (*LPDWORD(data) != 0xEEEEEEEE) {
    BYTE cn = data[0];
    //    BYTE dt1=data[3];
    BYTE dt0 = data[4];
    BYTE pth = data[5];
    BYTE ptl = data[6];

    //    BYTE year = (dt1 >> 2) & 0x0F;
    //    BYTE month = ((dt1 & 0x03) << 2) + (dt0 >> 6);
    //    BYTE day = (dt0 >> 1) & 0x1F;

    control = cn;
    time = MAKEWORD(ptl, pth) * timeConstSecond + timeConstHour * 12 * (dt0 & 0x1);
  }
  else {
    control = -1;
    time = 0;
  }
}

bool SportIdent::getCard6Data(BYTE* data, SICard& card)
{
  /*ofstream fout("si.txt");

  for(int m=0;m<128*3;m++)
  {
    if (m%16==0) fout << endl;

    char bf[16];
    sprintf_s(bf, 16, "%02x ", (DWORD)data[m]);
    fout << bf;
  }

  fout << endl;*/
  //return 0;

  memset(&card, 0, sizeof(card));

  WORD hi = MAKEWORD(data[11], data[10]);
  WORD lo = MAKEWORD(data[13], data[12]);

  card.CardNumber = MAKELONG(lo, hi);

  data += 16;

  //	DWORD control;
  //	DWORD time;
  card.convertedTime = ConvertedTimeStatus::Hour24;
  analysePunch(data + 8, card.StartPunch.Time, card.StartPunch.Code, useSubsecondMode);
  analysePunch(data + 4, card.FinishPunch.Time, card.FinishPunch.Code, useSubsecondMode);
  analysePunch(data + 12, card.CheckPunch.Time, card.CheckPunch.Code, false);
  card.nPunch = min(int(data[2]), 192);

  int i;
  char lastNameByte[21], firstNameByte[21];
  wstring lastName, firstName;

  memcpy(lastNameByte, data + 32, 20);
  lastNameByte[20] = 0;
  for (i = 19; i >= 0 && lastNameByte[i] == 0x20; i--) {
    lastNameByte[i] = 0;
  }

  string2Wide(lastNameByte, lastName);
  wcsncpy_s(card.lastName, lastName.c_str(), 20);
  card.lastName[20] = 0;

  memcpy(firstNameByte, data + 32 + 20, 20);
  firstNameByte[20] = 0;

  for (i = 19; i >= 0 && firstNameByte[i] == 0x20; i--) {
    firstNameByte[i] = 0;
  }

  string2Wide(firstNameByte, firstName);
  wcsncpy_s(card.firstName, firstName.c_str(), 20);
  card.firstName[20] = 0;

  data += 128 - 16;

  for (unsigned k = 0; k < card.nPunch; k++) {
    analysePunch(data + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false);
  }

  // Check for extra punches, SI6-bug
  for (unsigned k = card.nPunch; k < 192; k++) {
    if (!analysePunch(data + 4 * k, card.Punch[k].Time, card.Punch[k].Code, false)) {
      break;
    }
    else {
      card.nPunch++; // Extra punch
      card.Punch[k].Code += 1000; // Mark as special
    }
  }

  return true;
}
int cn2;

bool SportIdent::analysePunch(BYTE* data, DWORD& time, DWORD& control, bool subSecond) {
  if (*LPDWORD(data) != 0xEEEEEEEE && *LPDWORD(data) != 0x0)
  {
    BYTE ptd = data[0];
    BYTE cn = data[1];
    BYTE pth = data[2];
    BYTE ptl = data[3];
    cn = data[1];
    time = timeConstSecond * MAKEWORD(ptl, pth) + timeConstHour * 12 * (ptd & 0x1);
    if (!subSecond) {
      if (ptd >> 7 & 1) { //Beacon Start or Finish punch: Code stored in Block 1
        cn = data[153];
      }
      control = cn + 256 * ((ptd >> 6) & 0x1);
    }
    else {
      control = 0;
      uint8_t tss = data[1]; // Sub second 1/256 seconds
      static_assert(timeConstSecond == 10 || timeConstSecond == 100);
      int tenth = (((100 * tss) / 256) + 4) / (100 / timeConstSecond);
      time += tenth;
    }
    return true;
  }
  else
  {
    control = -1;
    time = 0;
    return false;
  }
}

void SportIdent::analyseSI5Time(BYTE* data, DWORD& time, DWORD& control)
{
  if (*LPWORD(data) != 0xEEEE) {
    time = MAKEWORD(data[1], data[0]) * timeConstSecond;
    control = 0;
  }
  else {
    control = -1;
    time = 0;
  }
}

void SICard::analyseHour12Time(DWORD zeroTime) {
  if (convertedTime != ConvertedTimeStatus::Hour12)
    return;

  StartPunch.analyseHour12Time(zeroTime);
  CheckPunch.analyseHour12Time(zeroTime);
  FinishPunch.analyseHour12Time(zeroTime);
  for (DWORD k = 0; k < nPunch; k++)
    Punch[k].analyseHour12Time(zeroTime);

  convertedTime = ConvertedTimeStatus::Hour24;
}

void SIPunch::analyseHour12Time(DWORD zeroTime) {
  if (Code != -1 && Time >= 0 && Time <= 12 * timeConstHour) {
    if (zeroTime < 12 * timeConstHour) {
      //Förmiddag
      if (Time < zeroTime)
        Time += 12 * timeConstHour; //->Eftermiddag
    }
    else {
      //Eftermiddag
      if (Time >= zeroTime % (12 * timeConstHour)) {
        //Eftermiddag
        Time += 12 * timeConstHour;
      }
      // else Efter midnatt OK.
    }
  }
}

int SportIdent::enumerateDevices(int maxNumEntries, MConWString* unitVectorDescr, bool* unitVectorConnected) {
  vector<int> ports;

  //Use QueryDosDevice to look for all devices of the form COMx. This is a better
  //solution as it means that no ports have to be opened at all.
  vector<TCHAR> szDevices(65535);
  DWORD dwChars = QueryDosDevice(NULL, szDevices.data(), 65535);
  if (dwChars) {
    int i = 0;

    for (;;) {
      //Get the current device name
      TCHAR* pszCurrentDevice = &szDevices[i];

      //If it looks like "COMX" then
      //add it to the array which will be returned
      int nLen = _tcslen(pszCurrentDevice);
      if (nLen > 3 && _tcsnicmp(pszCurrentDevice, _T("COM"), 3) == 0)
      {
        //Work out the port number
        int nPort = _ttoi(&pszCurrentDevice[3]);
        ports.push_back(nPort);
      }

      // Go to next NULL character
      while (szDevices[i] != _T('\0'))
        i++;

      // Bump pointer to the next string
      i++;

      // The list is double-NULL terminated, so if the character is
      // now NULL, we're at the end
      if (szDevices[i] == _T('\0'))
        break;
    }
  }

  sort(ports.begin(), ports.end());
  if (ports.size() > maxNumEntries)
    ports.resize(maxNumEntries);
  
  if (addFakePort && ports.empty())
    ports.push_back(13);

  portNames.clear();
  portNames.reserve(ports.size());
  for (int i = 0; i < ports.size(); i++) {
    portNames.push_back(L"COM" + to_wstring(ports[i]));
    unitVectorDescr[i] = portNames[i].c_str();

    auto si = findStation(portNames[i]);
    unitVectorConnected[i] = si ? si->running() : false;
  }

  return ports.size();
}

void SportIdent::status(int deviceIndex, MConDeviceStatus* status) const {
  const wstring& port = portNames.at(deviceIndex);
  status->name = port.c_str();
  status->requireFunctionMapping = false;

  const SI_StationInfo *station = findStation(port);

  if (station == nullptr) {
    status->connected = false;
  }
  else {
    status->connected = station->running();

    if (station->data.empty())
      status->requireFunctionMapping = true; // Listen mode (no contact made)
    else {
      switch (station->data[0].stationMode) {
      case 2: // Known modes
      case 50:
      case 4:
      case 3:
      case 5:
      case 7:
      case 10:
        break;
      default:
        status->requireFunctionMapping = true;
      }
    }
  }

  auto& infov = getInfoString(port);
  status->numStatusMessage = min(MConDeviceStatus::maxMessage, infov.size());
  for (int i = 0; i < status->numStatusMessage; i++) {
    status->message[i] = infov[i].second.c_str();
    status->isWarningMessage[i] = infov[i].first;
  }
}

int SportIdent::openConnection(int deviceIndex, bool listenOnly, int baudRate,  MConLogMessage logger) {
  const wstring &port = portNames.at(deviceIndex);

  for (int iter = 0; iter < 2; iter++) {
    int res = openCom(port, listenOnly, baudRate, logger);
    if (res == 1) {
      const SI_StationInfo* si = findStation(port);
      if (si && !si->extended() && !listenOnly)
        logger(L"warn:notextended", true);
      return 1;
    }
    else if (res == 0)
      return 0;

    //Retry...
    std::this_thread::sleep_for(300ms);
  }
  return 2;
}

int SportIdent::openCom(const wstring &com, bool listenOnly, int baudRate, MConLogMessage logger) {
  closeCom(com);

  SI_StationInfo *si = findStation(com);

  if (!si) {
    si = &siInfo[com];
    si->comPort = com;
  }

  si->data.clear();

  return openComStat(si, com, listenOnly, baudRate, logger); 
}

int SportIdent::openComStat(SI_StationInfo *si, 
                            const wstring & com, 
                            bool listenOnly, 
                            int baudRate, 
                            MConLogMessage logger) {

  wstring comfile = wstring(L"//./") + com;
  si->hComm = CreateFile(comfile.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0,
    0,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    0);

  if (si->hComm == INVALID_HANDLE_VALUE) {
    if (logger)
      logger(L"Failed to open port", true);
    si->hComm = 0;
    return 0;   // error opening port; abort
  }

  si->osStatus.hEvent = CreateEvent(nullptr, false, false, nullptr);
  if (si->osStatus.hEvent == nullptr) {
    if (logger)
      logger(L"Failed to create event", true);
    CloseHandle(si->hComm);
    si->hComm = 0;
    return 0;
  }

  GetCommTimeouts(si->hComm, &si->timeOuts);
  COMMTIMEOUTS myTimeOuts = si->timeOuts;
  myTimeOuts.ReadIntervalTimeout = 50;
  myTimeOuts.ReadTotalTimeoutMultiplier = 10;
  myTimeOuts.ReadTotalTimeoutConstant = 300;
  myTimeOuts.WriteTotalTimeoutMultiplier = 10;
  myTimeOuts.WriteTotalTimeoutConstant = 300;

  SetCommTimeouts(si->hComm, &myTimeOuts);

  DCB dcb;
  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength = sizeof(dcb);
  dcb.BaudRate = listenOnly ? baudRate : CBR_38400;
  dcb.fBinary = TRUE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.ByteSize = 8;

  SetCommState(si->hComm, &dcb);

  if (listenOnly)
    return 1;

  BYTE c[128];

  c[0] = WAKEUP;
  c[1] = STX;
  c[2] = STX;
  c[3] = 0xF0;
  c[4] = 0x01;
  c[5] = 0x4D;
  setCRC(c + 3);

  c[8] = ETX;

  DWORD written;

  written = writeBytes(c, 9, *si);
  Sleep(700);
  
  DWORD read;
  BYTE buff[128];
  memset(buff, 0, sizeof(buff));
  read = readBytes(buff, 1, *si);

  if (read == 1 && buff[0] == 0xFF) {
    Sleep(100);
    read = readBytes(buff, 1, *si);
  }

  if (read == 1 && buff[0] == STX) {
    read = readBytes(buff, 8, *si);

    if (!readSystemDataV2(*si))
      readSystemData(si, 1);
  }
  else {
    dcb.BaudRate = CBR_4800;
    SetCommState(si->hComm, &dcb);
    
    written = writeBytes(c, 9, *si);    
    Sleep(600);
    //c[6]=
    DWORD read;
    BYTE buff[128];

    read = readByte(*buff, *si);

    if (read == 1 && buff[0] == STX) {
      read = readBytes(buff, 8, *si);
      readSystemData(si);
    }
    else {
      BYTE cold[8];
      cold[0] = STX;
      cold[1] = 0x70;
      cold[2] = 0x4D;
      cold[3] = ETX;

      written = writeBytes(cold, 4, *si);

      Sleep(500);

      read = readByte(*buff, *si);

      if (read != 1 || buff[0] != STX) {
        closeComStat(si);
        return 2; // No response
      }

      read = readBytesDLE_delay(buff, sizeof(buff), 4, *si);
      if (read == 4) {
        si->data.resize(1);
        if (buff[1] > 30)
          si->data[0].stationNumber = buff[1];
        else {
          si->data[0].stationNumber = buff[2];

        }
        if (buff[3] != ETX)
          readByte(buff[4], *si);
      }
    }
  }

  return 1;
}

void SportIdent::closeCom(const wstring &com) {
  if (com.empty()) {
    vector<wstring> allOpen;
    for (auto& [key, value] : this->siInfo)
      allOpen.push_back(key);
    
    for (auto &key : allOpen)
      closeCom(key);
  }
  else {
    SI_StationInfo* si = findStation(com);

    closeComStat(si);
  }
}

void SportIdent::closeComStat(SI_StationInfo *si) {
  if (si) {
    if (si->running()) {
      si->requestStop();
    }

    int maxIter = 0;
    while (si->isStopping() && ++maxIter < 20) {
      std::this_thread::sleep_for(100ms);
    }

    lock_guard lc(si->closeMutex);

    if (si->hComm) {
      SetCommTimeouts(si->hComm, &si->timeOuts); //Restore

      CloseHandle(si->osStatus.hEvent);
      si->osStatus.hEvent = 0;

      CloseHandle(si->hComm);
      si->hComm = 0;
    }

    si->resetStatus();
  }
}

const vector<pair<bool, wstring>>& SportIdent::getInfoString(const wstring& com) const {
  deviceInfo.clear();
  const SI_StationInfo* si = findStation(com);
    

  if (!si || !si->running()) {
    deviceInfo.emplace_back(false, L"$ej aktiv$.");
    return deviceInfo;
  }

  for (size_t k = 0; k < si->data.size(); k++) {
    wstring info = si->comPort;

    if (si->data.size() > 1)
      info += L"\u2013" + to_wstring(k + 1);

    const SI_StationData& da = si->data[k];
    if (da.extended) info += L": $Utökat protokoll$. ";
    else info += L": $Äldre protokoll$. ";

    switch (da.stationMode) {
    case 2:
    case 50:
      info += L"$Kontroll$";
      break;
    case 4:
      info += L"$Mål$";
      break;
    case 3:
      info += L"$Start$";
      break;
    case 5:
      info += L"$Läs brickor$";
      break;
    case 7:
      info += L"$Töm$";
      break;
    case 10:
      info += L"$Check$";
      break;
    case 11:
      info += L"$SRR Dongle$ " + wstring(da.radioChannel == 0 ? L"$red channel$." : L"$blue channel$.");
      break;
    default:
      info += L"$Okänd funktion$";
    }

    if (da.stationNumber) {
      wchar_t bf[16];
      swprintf_s(bf, L" (%d).", da.stationNumber);
      info += bf;
    }

    info += L" $Kommunikation$: ";
    if (da.autoSend) info += L"$skicka stämplar$.";
    else if (da.handShake) info += L"$handskakning$.";
    else info += L"$[VARNING] ingen/okänd$.";

    deviceInfo.emplace_back(false, info);

    if (da.autoSend && da.stationMode == 5)
      deviceInfo.emplace_back(true, L"$Programmera stationen utan AUTOSEND$");
  }

  return deviceInfo;
}

void SportIdent::closeConnection(int deviceIndex) {
  const wstring &port = portNames.at(deviceIndex);
  closeCom(port);
}

void SportIdent::unload() {
  for (auto& [key, si] : siInfo) {
    if (si.running())
      si.requestStop();
  }
  closeCom(L"");
}

MConWString SportIdent::monitorThread(int deviceIndex) {
  SI_StationInfo *si = nullptr;
  MConWString result = nullptr;
  try {
    const wstring& port = portNames.at(deviceIndex);
    si = findStation(port);
    si->warning = L"";
    si->startRun();
    monitorSI(*si, si->warning);
  }
  catch (const std::exception& ex) {
    if (si) {
      string s = ex.what();
      string2Wide(s, si->warning);      
    }
  }
  catch (...) {
    si->warning = L"Unkown error in SportIdent";
  }
    
  if (si) {
    si->didStop();
    closeCom(si->comPort);

    if (!si->warning.empty())
      result = si->warning.c_str();
  }

  return result;
}

void SportIdent::addCard(const SICard& sic) {
  if (!addCardPtr)
    return;
  MConCard card;
  card.cardNo = sic.CardNumber;
  card.miliVolt = sic.miliVolt;
  card.use12HourClock = sic.convertedTime == SI::ConvertedTimeStatus::Hour12;
  static_assert(sizeof(card.firstName) == sizeof(sic.firstName));
  memcpy(card.firstName, sic.firstName, sizeof(sic.firstName));

  static_assert(sizeof(card.lastName) == sizeof(sic.lastName));
  memcpy(card.lastName, sic.lastName, sizeof(sic.lastName));

  static_assert(sizeof(card.club) == sizeof(sic.club));
  memcpy(card.club, sic.club, sizeof(sic.club));

  auto assign = [](const SIPunch& in, MConPunch& out) {
    out.time = in.Time * (1000 / timeConstSecond);
    out.code = in.Code;
  };

  assign(sic.CheckPunch, card.check);
  assign(sic.StartPunch, card.start);
  assign(sic.FinishPunch, card.finish);

  card.nPunch = sic.nPunch;
  for (int i = 0; i < card.nPunch; i++) {
    assign(sic.Punch[i], card.punch[i]);
  }

  addCardPtr(&card);
}

void SportIdent::addPunch(DWORD Time, int Station, int Card, int Mode) {
  if (!useSubsecondMode)
    Time -= (Time % timeConstSecond);

  MConFreePunch punch;
  punch.cardNo = Card;
  punch.time = Time * (1000 / timeConstSecond);
  /*SICard sic(ConvertedTimeStatus::Hour24);
  sic.CardNumber = Card;
  sic.StartPunch.Code = -1;
  sic.CheckPunch.Code = -1;
  sic.FinishPunch.Code = -1;*/
  /*
  auto mapPunch = [this](int code) {
    if (code > 0 && code < punchMap.size() && punchMap[code] > 0)
      return int(punchMap[code]);
    else if (code == oPunch::SpecialPunch::PunchStart)
      return int(oPunch::SpecialPunch::PunchCheck); // Do not allow start unless explicitly set
    else
      return code;
    };*/

  if (Mode == 0 || Mode == 11) { // 11 is dongle
    int code = (Station & 0xFFFF);
    
    int unit = (Station >> 16) & 0xFFFF;
    if (unit == 0)
      unit = code;
    
    punch.code = code;
    punch.unit = unit;
  }
  else {
    if (Mode == 0x02 || Mode == 50) {
      punch.code = Station;
      punch.unit = Station;
      punch.type = MConPunchType::PunchControl;
    }
    else if (Mode == 3) {
      punch.code = Station;
      punch.unit = Station;
      punch.type = MConPunchType::PunchStart;
    }
    else if (Mode == 10 || Mode == 7) { // Treat clear as check
      punch.code = Station;
      punch.unit = Station;
      punch.type = MConPunchType::PunchCheck;

    }
    else {
      punch.code = Station;
      punch.unit = Station;
      punch.type = MConPunchType::PunchFinish;
    }
  }

  addPunchPtr(&punch);
}

SportIdent si;

EXPORT int __cdecl autoDetect(int maxNumEntries, int *deviceIx) {
  return si.autoDetectToOpen(maxNumEntries, deviceIx);
}

EXPORT int __cdecl enumerateDevices(int maxNumEntries, MConWString *unitVectorDescr, bool *unitVectorConnected) {
  return si.enumerateDevices(maxNumEntries, unitVectorDescr, unitVectorConnected);
}

EXPORT void __cdecl status(int deviceIndex, MConDeviceStatus* status) {
  si.status(deviceIndex, status);
}

EXPORT int __cdecl openConnection(int deviceIndex, bool listenOnly, MConWString parameter, MConLogMessage logger) {
  return si.openConnection(deviceIndex, listenOnly, _wtoi(parameter), logger);
}

EXPORT void __cdecl closeConnection(int deviceIndex) {
  si.closeConnection(deviceIndex);
}

EXPORT MConWString __cdecl monitorThread(int deviceIndex) {
  return si.monitorThread(deviceIndex);
}

EXPORT void __cdecl init(MConDeviceProps *deviceProps,
                         MConWString *deviceHelp, 
                         MConWString *startParameterName,
                         MConAddCard addCard, MConAddPunch addPunch) {
  deviceProps->supportAutoDetect = true;
  si.setSubSecondMode(deviceProps->useSubSecond);

  *startParameterName = nullptr; // No parameter
  static const wchar_t *sihelp = L"info:sisystem";
  *deviceHelp = sihelp;
  si.init(addCard, addPunch);  
}

EXPORT void __cdecl unload() {
  si.unload();
}
