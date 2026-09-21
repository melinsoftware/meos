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

#include "cardmanager.h"
#include "carddevices.h"
#include "gdioutput.h"

CardManager *CardManager::cardManager = nullptr;

CardManager::CardManager(HWND hWndNotify) : hWndNotify(hWndNotify) {
  resetPunchMap();
}

void CardManager::setSubSecondMode(bool useSubSecond) {
  this->useSubSeconds = useSubSecond;
  if (CardDevices::isLoaded())
    CardDevices::instance().setSubSecondMode(useSubSeconds);
}

void CardManager::processQueue(gdioutput &gdi) {
  DeviceCard sic(ConvertedTimeStatus::Unknown);
  while (getCard(sic))
    insertCard(gdi, sic);
}

void CardManager::insertCard(gdioutput &gdi, DeviceCard &card) {
  for (auto &[tag, cs] : subscribers)
    cs->insertCard(gdi, card);
}

bool CardManager::getCard(DeviceCard &sic) {
  bool ret = false;

  std::lock_guard lock(cardMutex);
  
  if (!readCards.empty()) {
    sic = readCards.front();
    readCards.pop_front();
    ret = true;
  }

  return ret;
}

void CardManager::addCard(const DeviceCard &sic) {
  {
    std::lock_guard lock(cardMutex);
    readCards.push_front(sic);
  }

  PostMessage(hWndNotify, WM_USER, 0, 0);
}

void CardManager::init(HWND hWnd) {
  if (cardManager == nullptr)
    cardManager = new CardManager(hWnd);
  else if (hWnd == nullptr) {
    delete cardManager;
    cardManager = nullptr;
  }
  else
    throw std::exception("Already initialized");
}

CardManager &CardManager::instance() {
  if (cardManager == nullptr)
    throw std::exception("Not initialized");

  return *cardManager;
}

void CardManager::resetPunchMap() {
  punchMap.resize(1024);
  fill(punchMap.begin(), punchMap.end(), 0);
  punchMap[oPunch::SpecialPunch::PunchCheck] = oPunch::SpecialPunch::PunchCheck;
  punchMap[oPunch::SpecialPunch::PunchFinish] = oPunch::SpecialPunch::PunchFinish;
}

map<int, oPunch::SpecialPunch> CardManager::getSpecialMappings() const {
  map<int, oPunch::SpecialPunch> res;
  for (int j = 1; j < punchMap.size(); j++) {
    if (punchMap[j] > 0)
      res[j] = oPunch::SpecialPunch(punchMap[j]);
  }
  return res;
}

void CardManager::clearSpecialMappings() {
  fill(punchMap.begin(), punchMap.end(), 0);
}

void CardManager::addSpecialMapping(int code, oPunch::SpecialPunch p) {
  if (code > 0 && code < punchMap.size())
    punchMap[code] = p;
  else
    throw std::exception("Not supported");
}

void CardManager::removeSpecialMapping(int code) {
  if (code > 0 && code < punchMap.size())
    punchMap[code] = 0;
  else
    throw std::exception("Not supported");
}

int CardManager::mapPunch(int code) const {
  if (code > 0 && code < punchMap.size() && punchMap[code] > 0)
    return int(punchMap[code]);
  else if (code == oPunch::SpecialPunch::PunchStart)
    return int(oPunch::SpecialPunch::PunchCheck); // Do not allow start unless explicitly set
  else
    return code;
}

namespace {
  bool analysePunch(BYTE *data,int &time, int &control, bool subSecond) {
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

  void analyseTPunch(BYTE *data, int &time, int &control) {
    if (*LPDWORD(data) != 0xEEEEEEEE) {
      BYTE cn = data[0];
      BYTE dt0 = data[4];
      BYTE pth = data[5];
      BYTE ptl = data[6];
      control = cn;
      time = MAKEWORD(ptl, pth) * timeConstSecond + timeConstHour * 12 * (dt0 & 0x1);
    }
    else {
      control = -1;
      time = 0;
    }
  }

  DWORD getExtCardNumber(const BYTE *data) {
    DWORD cnr = 0;
    BYTE *p = (BYTE *)&cnr;
    p[0] = data[27];
    p[1] = data[26];
    p[2] = data[25];

    return cnr;
  }

  bool getCard9Data(BYTE *data, SICard &card) {
    
    memset(&card, 0, sizeof(card));


    card.CardNumber = getExtCardNumber(data);

    int series = data[24] & 15;

    card.convertedTime = ConvertedTimeStatus::Hour24;
    analysePunch(data + 12, card.StartPunch.Time, card.StartPunch.Code, false);
    analysePunch(data + 16, card.FinishPunch.Time, card.FinishPunch.Code, false);
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

}

void CardManager::readRawData(const wstring &file) {
  std::ifstream fin(file);

  BYTE b[128 * 5];
  memset(b, 0, 128 * 5);

  int blocks_8_9_p_t[2] = { 0,1 };
  int blocks_10_11_SIAC[6] = { 0,1,4,5,6,7 }; // Block 1 added,to handle SIACs using Beacon
  int limit = 1;
  int *blocks = blocks_8_9_p_t;

  auto readBlock = [&](BYTE *dest) {
    string line;
    bool ok = false;
    while (fin.good()) {
      std::getline(fin, line);
      size_t bl = line.find("Block");
      if (bl != string::npos) {
        ok = true;
        std::getline(fin, line);
        if (line[0] == '(')
          memset(dest, 0xEE, 128);
        for (int i = 0; i < 8; i++) {
          if (line.size() < 60)
            return false;

          size_t ix = line.find_first_of(':');
          if (ix != string::npos) {
            ix++;
            for (int j = 0; j < 16; j++) {
              int a = line.at(ix + 3 * j + 1);
              if (a >= '0' && a <= '9')
                a -= '0';
              else
                a -= 'A' - 10;

              int b = line.at(ix + 3 * j + 2);
              if (b >= '0' && b <= '9')
                b -= '0';
              else
                b -= 'A' - 10;

              dest[i * 16 + j] = 16 * a + b;
            }
          }
          else
            return false;

          std::getline(fin, line);
        }

        return true;
      }
    }
    return false;
  };

  int rBlock = 0;
  for (int k = 0; k < limit; k++) {
    if (k == 0) {
      readBlock(b);
      rBlock++;
    }
    else {
      while (rBlock < blocks[k]) {
        BYTE dmy[128];
        readBlock(dmy);
        rBlock++;
      }
      readBlock(b + k * 128);
      rBlock++;
    }
    if (k == 0) {
      int series = b[24] & 15;
      if (series == 15) {
        int nPunch = min(int(b[22]), 128);
        blocks = blocks_10_11_SIAC;
        limit = 2 + (nPunch + 31) / 32;   // Read Block 0, Block 1 + punches
        int cardNo = getExtCardNumber(b);
      }
      else {
        limit = 2; // Card 8, 9, p, t
      }
    }
  }

  DeviceCard card(ConvertedTimeStatus::Hour24);
  if (getCard9Data(b, card)) {
    addCard(card);
  }
}

unsigned int DeviceCard::calculateHash() const {
  unsigned h = nPunch * 100000 + FinishPunch.Time;
  for (unsigned i = 0; i < nPunch; i++) {
    h = h * 31 + Punch[i].Code;
    h = h * 31 + Punch[i].Time;
  }
  h += StartPunch.Time;
  return h;
}

string DeviceCard::serializePunches() const {
  string ser = "*";// Mark of time factor
  if (CheckPunch.Code != -1)
    ser += "C-" + itos(CheckPunch.Time);

  if (StartPunch.Code != -1) {
    if (!ser.empty()) ser += ";";
    ser += "S-" + itos(StartPunch.Time);
  }
  for (DWORD i = 0; i < nPunch; i++) {
    if (!ser.empty()) ser += ";";
    ser += itos(Punch[i].Code) + "-" + itos(Punch[i].Time);
  }

  if (FinishPunch.Code != -1) {
    if (!ser.empty()) ser += ";";
    ser += "F-" + itos(FinishPunch.Time);
  }
  return ser;
}

void DeviceCard::deserializePunches(const string &arg) {
  convertedTime = ConvertedTimeStatus::Hour24;
  FinishPunch.Code = -1;
  StartPunch.Code = -1;
  CheckPunch.Code = -1;
  vector<string> out;
  int timeFactor = 10;
  if (arg.length() > 1 && arg[0] == '*') {
    split(arg.c_str() + 2, ";", out);
    timeFactor = 1;
  }
  else {
    split(arg, ";", out);
  }
  nPunch = 0;
  for (size_t k = 0; k < out.size(); k++) {
    vector<string> mark;
    split(out[k], "-", mark);
    if (mark.size() != 2 || mark[0].empty())
      throw std::exception("Invalid string");
    int *tp = nullptr;
    if (mark[0][0] == 'F') {
      FinishPunch.Code = atoi(mark[0].c_str() + 1);
      tp = &FinishPunch.Time;
    }
    else if (mark[0][0] == 'S') {
      StartPunch.Code = atoi(mark[0].c_str() + 1);
      tp = &StartPunch.Time;
    }
    else if (mark[0][0] == 'C') {
      CheckPunch.Code = atoi(mark[0].c_str() + 1);
      tp = &CheckPunch.Time;
    }
    else {
      Punch[nPunch].Code = atoi(mark[0].c_str());
      tp = &Punch[nPunch++].Time;
    }

    *tp = atoi(mark[1].c_str()) * timeFactor;
  }
  if (out.size() == 1)
    punchOnly = true;
}

int DeviceCard::getFirstTime() const {
  if (StartPunch.Time > 0)
    return StartPunch.Time;

  for (int i = 0; i < nPunch; i++) {
    if (Punch[i].Time > 0)
      return Punch[i].Time;
  }

  if (FinishPunch.Time > 0)
    return FinishPunch.Time;

  return 0;
}

void DeviceCard::analyseHour12Time(int zeroTime) {
  if (convertedTime != ConvertedTimeStatus::Hour12)
    return;

  StartPunch.analyseHour12Time(zeroTime);
  CheckPunch.analyseHour12Time(zeroTime);
  FinishPunch.analyseHour12Time(zeroTime);
  for (DWORD k = 0; k < nPunch; k++)
    Punch[k].analyseHour12Time(zeroTime);

  convertedTime = ConvertedTimeStatus::Hour24;
}

void SIPunch::analyseHour12Time(int zeroTime) {
  if (Time == -1)
    return;
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

namespace {
  string formatTimeN(int t) {
    const wstring &wt = formatTime(t, SubSecond::Auto);
    string nt(wt.begin(), wt.end());
    return nt;
  }
}
vector<string> DeviceCard::codeLogData(int row) const {
  vector<string> log;

  log.push_back(itos(row));
  if (readOutTime[0] == 0)
    log.push_back(getLocalTimeN());
  else
    log.push_back(readOutTime);
  log.push_back(itos(CardNumber));
  log.push_back("");
  log.push_back("");

  log.push_back(gdioutput::recodeToNarrow(firstName));
  log.push_back(gdioutput::recodeToNarrow(lastName));
  log.push_back(gdioutput::recodeToNarrow(club));
  log.push_back("");
  log.push_back("");
  log.push_back("");
  log.push_back(""); //email
  log.push_back("");
  log.push_back("");
  log.push_back("");
  log.push_back(""); //zip

  log.push_back(""); //CLR_NO
  log.push_back("");
  log.push_back("");

  string indicator24 = convertedTime == ConvertedTimeStatus::Hour12 ? "" : "MO";

  //We set monday on every punch for 24-hour clock, since this indicates 24-hour support. Empty for 12-hour punch
  if (signed(CheckPunch.Code) >= 0) {
    log.push_back(itos(CheckPunch.Code));
    log.push_back(indicator24);
    log.push_back(formatTimeN(CheckPunch.Time));
  }
  else {
    log.push_back(""); //CHCK_NO
    log.push_back("");
    log.push_back("");
  }

  if (signed(StartPunch.Code) >= 0) {
    log.push_back(itos(StartPunch.Code));
    log.push_back(indicator24);
    log.push_back(formatTimeN(StartPunch.Time));
  }
  else {
    log.push_back(""); //START_NO
    log.push_back("");
    log.push_back("");
  }

  if (signed(FinishPunch.Code) >= 0) {
    log.push_back(itos(FinishPunch.Code));
    log.push_back(indicator24);
    log.push_back(formatTimeN(FinishPunch.Time));
  }
  else {
    log.push_back(""); //FINISH_NO
    log.push_back("");
    log.push_back("");
  }
  log.push_back(itos(nPunch));

  for (int k = 0; k < 192; k++) {
    if (k<int(nPunch)) {
      log.push_back(itos(Punch[k].Code));
      if (Punch[k].Time > 0) {
        log.push_back(indicator24);
        log.push_back(formatTimeN(Punch[k].Time));
      }
      else {
        log.push_back("");
        log.push_back("");
      }
    }
    else {
      log.push_back("");
      log.push_back("");
      log.push_back("");
    }
  }
  return log;
}

vector<string> SICard::logHeader() {
  vector<string> log;

  log.push_back("No.");
  log.push_back("read at");
  log.push_back("SI-Card");
  log.push_back("St no");
  log.push_back("cat");
  log.push_back("First name");
  log.push_back("name");
  log.push_back("club");
  log.push_back("country");
  log.push_back("sex");
  log.push_back("year-op");
  log.push_back("EMail");
  log.push_back("mobile");
  log.push_back("city");
  log.push_back("street");
  log.push_back("zip");

  log.push_back("CLR_CN");
  log.push_back("CLR_DOW");
  log.push_back("clear time");

  log.push_back("CHK_CN");
  log.push_back("CHK_DOW");
  log.push_back("check time");

  log.push_back("ST_CN");
  log.push_back("ST_DOW");
  log.push_back("start time");

  log.push_back("FI_CN");
  log.push_back("FO_DOW");
  log.push_back("Finish time");

  log.push_back("No. of punches");

  for (int k = 0; k < 192; k++) {
    string pf = itos(k + 1);
    log.push_back(pf + ".CN");
    log.push_back(pf + ".DOW");
    log.push_back(pf + ".Time");
  }
  return log;
}

