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

#include "stdafx.h"
#include <vector>
#include <math.h>
#include "meos_util.h"
#include "localizer.h"
#include "oFreeImport.h"
#include "meosexception.h"
#include <WinInet.h>

using namespace std;

StringCache globalStringCache;

namespace MeOSUtil {
  int useHourFormat = true;
}

string convertSystemTimeN(const SYSTEMTIME &st);
string convertSystemDateN(const SYSTEMTIME &st);
string convertSystemTimeOnlyN(const SYSTEMTIME &st);
extern int defaultCodePage;

DWORD mainThreadId = -1;
StringCache &StringCache::getInstance() {
  DWORD id = GetCurrentThreadId();
  if (mainThreadId == -1)
    mainThreadId = id;
  else if (mainThreadId != id)
    throw std::exception("Thread access error");
  return globalStringCache;
}

string getLocalTimeN() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemTimeN(st);
}

string getLocalDateN()
{
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemDateN(st);
}

wstring getLocalTime() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemTime(st);
}

wstring getLocalDate() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemDate(st);
}

int getLocalAbsTime() {
  return convertAbsoluteTimeHMS(getLocalTimeOnly(), -1);
}

int getThisYear() {
  static int thisYear = 0;
  if (thisYear == 0) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    thisYear = st.wYear;
  }
  return thisYear;
}


/** Extend a year from 03 -> 2003, 97 -> 1997 etc */
int extendYear(int year) {
  if (year<0)
    return year;

  if (year>=100)
    return year;

  int thisYear = getThisYear();

  int cLast = thisYear%100;

  if (cLast == 0 && year == 0)
    return thisYear;

  if (year > thisYear%100)
    return (thisYear - cLast) - 100 + year;
  else
    return (thisYear - cLast) + year;
}

wstring getLocalTimeFileName()
{
  SYSTEMTIME st;
  GetLocalTime(&st);

  wchar_t bf[32];
  swprintf_s(bf, L"%d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay,
    st.wHour, st.wMinute, st.wSecond);

  return bf;
}

string getLocalTimeOnlyN()
{
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemTimeOnlyN(st);
}

wstring getLocalTimeOnly()
{
  SYSTEMTIME st;
  GetLocalTime(&st);
  return convertSystemTimeOnly(st);
}

int getRelativeDay() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);

  ULARGE_INTEGER u;
  u.HighPart = ft.dwHighDateTime;
  u.LowPart = ft.dwLowDateTime;
  __int64 qp = u.QuadPart;
  qp /= __int64(10) * 1000 * 1000 * 3600 * 24;
  qp -=  400*365;
  return int(qp);
}

__int64 SystemTimeToInt64TenthSecond(const SYSTEMTIME &st) {
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);

  ULARGE_INTEGER u;
  u.HighPart = ft.dwHighDateTime;
  u.LowPart = ft.dwLowDateTime;
  __int64 qp = u.QuadPart; // Time resolution 100 ns
  qp /= __int64(1000 * 1000 * 10 / timeUnitsPerSecond);
  return qp;
}

SYSTEMTIME Int64TenthSecondToSystemTime(__int64 time) {
  SYSTEMTIME st;
  FILETIME ft;

  ULARGE_INTEGER u; // Time resolution 100 ns
  u.QuadPart = time * __int64(1000 * 1000 * 10 / timeUnitsPerSecond);
  ft.dwHighDateTime = u.HighPart;
  ft.dwLowDateTime = u.LowPart;

  FileTimeToSystemTime(&ft, &st);

  return st;
}
//2014-11-03 07:02:00
string convertSystemTimeN(const SYSTEMTIME &st)
{
  char bf[64];
  sprintf_s(bf, "%d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay,
    st.wHour, st.wMinute, st.wSecond);

  return bf;
}

//2014-11-03 07:02:00
wstring convertSystemTime(const SYSTEMTIME &st)
{
  wchar_t bf[64];
  swprintf_s(bf, L"%d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay,
    st.wHour, st.wMinute, st.wSecond);

  return bf;
}

string convertSystemTimeOnlyN(const SYSTEMTIME &st)
{
  char bf[32];
  sprintf_s(bf, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

  return bf;
}

wstring convertSystemTimeOnly(const SYSTEMTIME &st) {
  wchar_t bf[32];
  swprintf_s(bf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

  return bf;
}

string convertSystemDateN(const SYSTEMTIME &st)
{
  char bf[32];
  sprintf_s(bf, "%d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

  return bf;
}

wstring convertSystemDate(const SYSTEMTIME &st)
{
  wchar_t bf[32];
  swprintf_s(bf, L"%d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

  return bf;
}

string formatDateN(int m, bool useIsoFormat) {
  char bf[24];
  if (m > 0 && m < 30000101) {
    sprintf_s(bf, 24, "%d-%02d-%02d", m/(100*100), (m/100)%100, m%100);
  }
  else {
    bf[0] = '-';
    bf[1] = 0;
  }
  return bf;
}

wstring formatDate(int m, bool useIsoFormat) {
  wchar_t bf[24];
  if (m > 0 && m < 30000101) {
    swprintf_s(bf, 24, L"%d-%02d-%02d", m/(100*100), (m/100)%100, m%100);
  }
  else {
    bf[0] = '-';
    bf[1] = 0;
  }
  return bf;
}

int convertDateYMD(const wstring &m, SYSTEMTIME &st, bool checkValid) {
  string ms(m.begin(), m.end());
  return convertDateYMD(ms, st, checkValid);
}
//Absolute time string to SYSTEM TIME
int convertDateYMD(const string& m, SYSTEMTIME& st, bool checkValid) {
  memset(&st, 0, sizeof(st));

  if (m.length() == 0)
    return -1;

  int len = m.length();
  int dashCount = 0;
  for (int k = 0; k < len; k++) {
    BYTE b = m[k];
    if (b == 'T')
      break;
    if (!(b == '-' || b == ' ' || (b >= '0' && b <= '9')))
      return -1;

    if (b == '-')
      dashCount++;
  }

  int year = atoi(m.c_str());

  if (dashCount == 0) {
    int day = year % 100;
    year /= 100;
    int month = year % 100;
    year /= 100;

    if ((year > 0 && year < 100) || (year == 0 && m.size() > 2 && m[0] == '0' && m[1] == '0'))
      year = extendYear(year);

    if (year < 1900 || year>3000)
      return -1;

    if (month < 1 || month>12) {
      if (checkValid)
        return -1;
      month = 1;
    }

    if (day < 1 || day>31) {
      if (checkValid)
        return -1;
      day = 1;
    }

    st.wYear = year;
    st.wMonth = month;
    st.wDay = day;

    int t = year * 100 * 100 + month * 100 + day;
    if (t < 0)
      return -1;

    return t;
  }

  if ((year > 0 && year < 100) || (year == 0 && m.size() > 2 && m[0] == '0' && m[1] == '0'))
    year = extendYear(year);

  if (year < 1900 || year>3000)
    return -1;

  int month = 0;
  int day = 0;
  int kp = m.find_first_of('-');

  if (kp != string::npos) {
    string mtext = m.substr(kp + 1);
    month = atoi(mtext.c_str());

    if (month < 1 || month>12) {
      if (checkValid)
        return -1;
      month = 1;
    }

    kp = mtext.find_last_of('-');

    if (kp != string::npos) {
      day = atoi(mtext.substr(kp + 1).c_str());
      if (day < 1 || day>31) {
        if (checkValid)
          return -1;
        day = 1;
      }
    }
  }
  st.wYear = year;
  st.wMonth = month;
  st.wDay = day;


  int t = year * 100 * 100 + month * 100 + day;
  if (t < 0) return -1;

  return t;
}

//Absolute time string to absolute time int
int convertDateYMD(const string &m, bool checkValid)
{
  SYSTEMTIME st;
  return convertDateYMD(m, st, checkValid);
}

int convertDateYMD(const wstring &m, bool checkValid)
{
  SYSTEMTIME st;
  return convertDateYMD(m, st, checkValid);
}

bool myIsSpace(wchar_t b) {
  return iswspace(b) != 0 || b == 0x00A0 || b == 0x2007 || b == 0x202F;
}


//Absolute time string to absolute time int
int convertAbsoluteTimeHMS(const string &m, int daysZeroTime) {
  int len = m.length();

  if (len==0 || m[0]=='-')
    return -1;

  // Support for notation 2D 14:30:00 or 2T14:30:00 for several days
  int tix = -1;
  for (size_t k = 0; k < m.length(); k++) {
    int c = m[k];
    if (c =='D' || c =='d' || c =='T' || c =='t') {
      tix = k;
      break;
    }
  }
  if (tix != -1) {
    if (daysZeroTime < 0)
      return -1; // Not supported
    int tpart = convertAbsoluteTimeHMS(m.substr(tix+1), -1);
    if (tpart != -1) {
      int days = atoi(m.c_str());
      if (days <= 0)
        return -1;
      if (tpart < daysZeroTime)
        days--;
      return days * timeConstHour * 24 + tpart;
    }
    return -1;
  }

  int plusIndex = -1;
  for (int k=0;k<len;k++) {
    BYTE b=m[k];
    if ( !(isspace(b) || b==':' || (b>='0' && b<='9') || b == '.' || b == ',') ) {
      if (b=='+' && plusIndex ==-1 && k>0)
        plusIndex = k;
      else
        return -1;
    }
  }

  if (plusIndex>0) {
    int t = convertAbsoluteTimeHMS(m.substr(plusIndex+1), -1);
    int d = atoi(m.c_str());

    if (d>0 && t>=0)
      return d*24* timeConstHour + t;
    else
      return -1;
  }

  int hour=atoi(m.c_str());

  if (hour<0 || hour>23)
    return -1;

  int minute=0;
  int second=0;
  int tenth = 0;
  int kp=m.find_first_of(':');

  if (kp!=string::npos) {
    string mtext=m.substr(kp+1);
    minute=atoi(mtext.c_str());

    if (minute<0 || minute>60)
      minute=0;

    kp=mtext.find_last_of(':');

    if (kp!=string::npos) {
      second=atoi(mtext.c_str() + kp+1);
      if (second<0 || second>60)
        second=0;

      if (timeConstSecond > 1) {
        kp = mtext.find_last_of('.');
        if (kp == string::npos)
          kp = mtext.find_last_of(',');
        if (kp != string::npos) {
          tenth = atoi(mtext.c_str() + kp + 1);
          if (tenth < 0 || tenth >= 10)
            tenth = 0;
        }
      }
    }
  }

  int t = hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond + tenth;
  
  if (t<0) 
    return 0;

  return t;
}

//Absolute time string to absolute time int
int convertAbsoluteTimeHMS(const wstring &m, int daysZeroTime) {
  string sm(m.begin(), m.end());
  return convertAbsoluteTimeHMS(sm, daysZeroTime);
}

//Absolute time string to absolute time int
int convertAbsoluteTimeISO(const string &m)
{
  int len = m.length();

  if (len==0 || m[0]=='-')
    return -1;

  string hStr, mStr, sStr;

  string tmp =  trim(m);

  if (tmp.length() < 3)
    return -1;

  hStr = tmp.substr(0, 2);
  if (!(tmp[2] >= '0' && tmp[2]<='9'))
    tmp = tmp.substr(3);
  else
    tmp = tmp.substr(2);

  if (tmp.length() < 3)
    return -1;

  mStr = tmp.substr(0, 2);

  if (!(tmp[2] >= '0' && tmp[2]<='9'))
    tmp = tmp.substr(3);
  else
    tmp = tmp.substr(2);

  if (tmp.length() < 2)
    return -1;

  sStr = tmp.substr(0, 2);

  for (int i = 0; i < 2; i++) {
    if (hStr[i] < '0' || hStr[i] > '9')
      return -1;
    if (mStr[i] < '0' || mStr[i] > '9')
      return -1;
    if (sStr[i] < '0' || sStr[i] > '9')
      return -1;
  }

  int hour = atoi(hStr.c_str());
  if (hour<0 || hour>23)
    return -1;

  int minute = atoi(mStr.c_str());

  if (minute<0 || minute>60)
    return -1;

  int second = atoi(sStr.c_str());

  if (second<0 || second>60)
    return -1;

  int t = hour * timeConstHour + minute * timeConstMinute + second;

  return t;
}

int convertAbsoluteTimeISO(const wstring &m) {
  string mn(m.begin(), m.end());
  return convertAbsoluteTimeISO(mn);
}

// Parse +-MM:SS or +-HH:MM:SS
int convertAbsoluteTimeMS(const string &m)
{
  if (m.length()==0)
    return NOTIME;

  int minute=0;
  int second=0;

  int signpos=m.find_first_of('-');
  string mtext;

  int sign=1;

  if (signpos!=string::npos) {
    sign=-1;
    mtext=m.substr(signpos+1);
  }
  else
    mtext=m;

  minute=atoi(mtext.c_str());
  int hour = 0;
  if (minute<0 || minute>60*24)
    minute=0;

  int kp=mtext.find_first_of(':');
  bool gotSecond = false;
  if (kp!=string::npos) {
    mtext = mtext.substr(kp+1);
    second = atoi(mtext.c_str());
    gotSecond = true;
    if (second<0 || second>60)
      second=0;
  }

  int t;
  kp = mtext.find_first_of(':');
  if (kp != string::npos) {
    //Allow also for format +-HH:MM:SS
    hour = minute;
    minute = second;
    
    mtext = mtext.substr(kp + 1);
    second = atoi(mtext.c_str());
    if (second < 0 || second>60)
      second = 0;  
  }
  
  int tenth = 0;
  if (timeConstSecond > 1) {
    kp = mtext.find_first_of('.');
    if (kp == string::npos)
      kp = mtext.find_last_of(',');
    if (kp != string::npos) {
      tenth = atoi(mtext.c_str() + kp + 1);
      if (!gotSecond) { // Reinterpret minute as second (no minute was specified)
        second = minute;
        minute = 0;
      }
      if (tenth < 0 || tenth >= 10)
        tenth = 0;
    }
  }
  t = hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond + tenth;

  return sign*t;
}

int convertAbsoluteTimeMS(const wstring &m) {
  string mn(m.begin(), m.end());
  return convertAbsoluteTimeMS(mn);
}

//Generate +-MM:SS or +-HH:MM:SS
const wstring &formatTimeMS(int m, bool force2digit, SubSecond mode) {
  wchar_t bf[32];
  int am = abs(m);
  if (am < timeConstHour || !MeOSUtil::useHourFormat) {
    if (force2digit) {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf_s(bf, L"-%02d:%02d", am / timeConstMinute, (am / timeConstSecond) % 60);
      else
        swprintf_s(bf, L"-%02d:%02d.%d", am / timeConstMinute, (am / timeConstSecond) % 60, am % timeConstSecond);
    }
    else {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf_s(bf, L"-%d:%02d", am / timeConstMinute, (am / timeConstSecond) % 60);
      else
        swprintf_s(bf, L"-%d:%02d.%d", am / timeConstMinute, (am / timeConstSecond) % 60, am % timeConstSecond);
    }
  }
  else if (am < timeConstHour * 48) {
    if (force2digit) {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf_s(bf, L"-%02d:%02d:%02d", am / timeConstHour, (am / timeConstMinute) % 60, (am / timeConstSecond) % 60);
      else
        swprintf_s(bf, L"-%02d:%02d:%02d.%d", am / timeConstHour, (am / timeConstMinute) % 60, (am / timeConstSecond) % 60, am % timeConstSecond);
    }
    else {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf_s(bf, L"-%d:%02d:%02d", am / timeConstHour, (am / timeConstMinute) % 60, (am / timeConstSecond) % 60);
      else
        swprintf_s(bf, L"-%d:%02d:%02d.%d", am / timeConstHour, (am / timeConstMinute) % 60, (am / timeConstSecond) % 60, am % timeConstSecond);
    }
  }
  else {
    m = 0;
    bf[0] = 0x2013;
    bf[1] = 0;
  }
  wstring &res = StringCache::getInstance().wget();
  if (m<0)
    res = bf; // with minus
  else
    res = bf + 1;

  return res;
}

const wstring &formatTime(int rt, SubSecond mode) {
  wstring &res = StringCache::getInstance().wget();
  if (rt>0 && rt<timeConstHour*999) {
    wchar_t bf[40];
    if (mode == SubSecond::Off || (mode == SubSecond::Auto && rt % 10 == 0)) {
      if (rt >= timeConstHour && MeOSUtil::useHourFormat)
        swprintf_s(bf, L"%d:%02d:%02d", rt / timeConstHour, (rt / timeConstMinute) % 60, (rt / timeConstSecond) % 60);
      else
        swprintf_s(bf, L"%d:%02d", (rt / timeConstMinute), (rt / timeConstSecond) % 60);
    }
    else {
      if (rt >= timeConstHour && MeOSUtil::useHourFormat)
        swprintf_s(bf, L"%d:%02d:%02d.%d", rt / timeConstHour, (rt / timeConstMinute) % 60, (rt / timeConstSecond) % 60, rt%timeConstSecond);
      else
        swprintf_s(bf, L"%d:%02d.%d", (rt / timeConstMinute), (rt / timeConstSecond) % 60, rt%timeConstSecond);
    }
    res = bf;
    return res;
  }
  wchar_t ret[2] = {0x2013, 0};
  res = ret;
  return res;
}

const string &formatTimeN(int rt) {
  string &res = StringCache::getInstance().get();
  if (rt>0 && rt<timeConstHour *999) {
    char bf[16];
    if (rt>= timeConstHour && MeOSUtil::useHourFormat)
      sprintf_s(bf, 16, "%d:%02d:%02d", rt/ timeConstHour,(rt/timeConstMinute)%60, (rt/timeConstSecond)%60);
    else
      sprintf_s(bf, 16, "%d:%02d", (rt/timeConstMinute), (rt/timeConstSecond)%60);

    res = bf;
    return res;
  }
  res = "-";
  return res;
}

const wstring &formatTimeHMS(int rt, SubSecond mode) {
  wstring &res = StringCache::getInstance().wget();
  if (rt>=0) {
    wchar_t bf[40];
    if (mode == SubSecond::Off || (mode == SubSecond::Auto && rt%10 == 0))
      swprintf_s(bf, 16, L"%02d:%02d:%02d", rt/timeConstHour,(rt/timeConstMinute)%60, (rt/timeConstSecond)%60);
    else
      swprintf_s(bf, 16, L"%02d:%02d:%02d.%d", rt / timeConstHour, (rt / timeConstMinute) % 60, (rt / timeConstSecond) % 60, rt % timeConstSecond);

    res = bf;
    return res;
  }
  wchar_t ret[2] = {0x2013, 0};
  res = ret;
  return res;
}

wstring formatTimeIOF(int rt, int zeroTime)
{
  if (rt > 0) {
    rt += zeroTime;
    wchar_t bf[16];
    swprintf_s(bf, 16, L"%02d:%02d:%02d", (rt / timeConstHour) % 24, (rt / timeConstMinute) % 60, (rt / timeConstSecond) % 60);

    return bf;
  }
  return L"--:--:--";
}

size_t find(const wstring &str, const wstring &separator, size_t startpos)
{
  size_t seplen = separator.length();

  for (size_t m = startpos; m<str.length(); m++) {
    for (size_t n = 0; n<seplen; n++)
      if (str[m] == separator[n])
        return m;
  }
  return str.npos;
}

size_t find(const string &str, const string &separator, size_t startpos)
{
  size_t seplen = separator.length();

  for (size_t m = startpos; m<str.length(); m++) {
    for (size_t n = 0; n<seplen; n++)
      if (str[m] == separator[n])
        return m;
  }
  return str.npos;
}

const vector<string> & split(const string &line, const string &separators, vector<string> &split_vector) {
  split_vector.clear();

  if (line.empty())
    return split_vector;

  size_t startpos=0;
  size_t nextp=find(line, separators, startpos);
  split_vector.push_back(line.substr(startpos, nextp-startpos));

  while(nextp!=line.npos) {
    startpos=nextp+1;
    nextp=find(line, separators, startpos);
    split_vector.push_back(line.substr(startpos, nextp-startpos));
  }

  return split_vector;
}

const vector<wstring> & split(const wstring &line, const wstring &separators, vector<wstring> &split_vector) {
  split_vector.clear();

  if (line.empty())
    return split_vector;

  size_t startpos=0;
  size_t nextp=find(line, separators, startpos);
  split_vector.push_back(line.substr(startpos, nextp-startpos));

  while(nextp!=line.npos) {
    startpos=nextp+1;
    nextp=find(line, separators, startpos);
    split_vector.push_back(line.substr(startpos, nextp-startpos));
  }

  return split_vector;
}


const string &unsplit(const vector<string> &split_vector, const string &separators, string &line) {
  size_t s = split_vector.size() * separators.size();
  for (size_t k = 0; k < split_vector.size(); k++) {
    s += split_vector[k].size();
  }
  
  line.clear();
  line.reserve(s);
  for (size_t k = 0; k < split_vector.size(); k++) {
    if (k != 0)
      line += separators;
    line += split_vector[k];
  }
  return line;
}

const wstring &unsplit(const vector<wstring> &split_vector, const wstring &separators, wstring &line) {
  size_t s = split_vector.size() * separators.size();
  for (size_t k = 0; k < split_vector.size(); k++) {
    s += split_vector[k].size();
  }
  
  line.clear();
  line.reserve(s);
  for (size_t k = 0; k < split_vector.size(); k++) {
    if (k != 0)
      line += separators;
    line += split_vector[k];
  }
  return line;
}

const wstring &limitText(const wstring& tIn, size_t numChar) {
  wstring& out = StringCache::getInstance().wget();
  out.clear();
  const auto L = tIn.length();
  //if (L < numChar)
  //  out = tIn;

  int spacePos = -1;
  int outP = 0;
  int i = 0;
  for (; i < L && outP < numChar-1; i++) {
    wchar_t c = tIn[i];
    if (c == '\n' || c == '\r' || c == '\t')
      c = ' ';

    if (c==' ' && spacePos == outP)
      continue;  // Skip duplicate space

    out.push_back(c);
    outP++;
    if (iswspace(c))
      spacePos = outP;
  }

  if (i < L) {
    if (spacePos <= 2 || spacePos < signed(numChar) - 10)
      spacePos = numChar - 4;
    out = out.substr(0, spacePos-1) + L"\u2026";
  }

  return out;
}

wstring ensureEndingColon(const wstring& text) {
  if (text.empty() || text[text.length() - 1] == ':')
    return text;
  
  return text + L":";
}


const wstring &makeDash(const wstring &t) {
  return makeDash(t.c_str());
}

const wstring &makeDash(const wchar_t *t) {
  wstring &out = StringCache::getInstance().wget();
  out = t;
  for (size_t i=0;i<out.length(); i++) {
    if (t[i]=='-')
      out[i]= 0x2013;
  }
  return out;
}

wstring formatRank(int rank) {
  wchar_t r[16];
  swprintf_s(r, L"(%04d)", rank);
  return r;
}

const wstring &itow(int i) {
  wchar_t bf[32];
  _itow_s(i, bf, 10);
  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

wstring itow(unsigned long i) {
  wchar_t bf[32];
  _ultow_s(i, bf, 10);
  return bf;
}


wstring itow(unsigned int i) {
  wchar_t bf[32];
  _ultow_s(i, bf, 10);
  return bf;
}

wstring itow(int64_t i) {
  wchar_t bf[32];
  _i64tow_s(i, bf, 32, 10);
  return bf;
}

wstring itow(uint64_t i) {
  wchar_t bf[32];
  _ui64tow_s(i, bf, 32, 10);
  return bf;
}

const string &itos(int i)
{
  char bf[32];
  _itoa_s(i, bf, 10);
  string &res = StringCache::getInstance().get();
  res = bf;
  return res;
}

string itos(unsigned int i)
{
  char bf[32];
  _ultoa_s(i, bf, 10);
  return bf;
}

string itos(unsigned long i)
{
  char bf[32];
  _ultoa_s(i, bf, 10);
  return bf;
}

string itos(int64_t i)
{
  char bf[32];
  _i64toa_s(i, bf, 32, 10);
  return bf;
}

string itos(uint64_t i)
{
  char bf[32];
  _ui64toa_s(i, bf, 32, 10);
  return bf;
}

void prepareMatchString(wchar_t* data_c, int size) {
  CharLowerBuff(data_c, size);
  for (int j = 0; j < size; j++)
    data_c[j] = toLowerStripped(data_c[j]);
}

bool filterMatchString(const wstring &c, const wchar_t *filt_lc, int &score) {
  score = 0;
  if (filt_lc[0] == 0)
    return true;
  wchar_t key[2048];
  wcscpy_s(key, c.c_str());
  int cl = c.length();
  CharLowerBuff(key, cl);
  for (int j = 0; j < cl; j++)
    key[j] = toLowerStripped(key[j]);

  bool match = wcsstr(key, filt_lc) != 0;
  if (match) {
    while (filt_lc[score] && key[score] && filt_lc[score] == key[score])
      score++;
  }
  return match;
}


int countWords(const wchar_t *p) {
  int nwords=0;
  const wchar_t *ep=p;
  while (*ep) {
    if (!myIsSpace(*ep)) {
      nwords++;
      while ( *ep && !myIsSpace(*ep) )
        ep++;
    }
    while (*ep && myIsSpace(*ep))
        ep++;
  }
  return nwords;
}

string trim(const string &s)
{
  const char *ptr=s.c_str();
  int len=s.length();

  int i=0;
  while(i<len && (isspace(BYTE(ptr[i])) || BYTE(ptr[i])==BYTE(160))) i++;

  int k=len-1;

  while(k>=0 && (isspace(BYTE(ptr[k])) || BYTE(ptr[i])==BYTE(160))) k--;

  if (i == 0 && k == len-1)
    return s;
  else if (k>=i && i<len)
    return s.substr(i, k-i+1);
  else return "";
}

wstring trim(const wstring &s) {
  const wchar_t *ptr = s.c_str();
  int len = s.length();

  int i=0;
  while(i<len && myIsSpace(ptr[i])) i++;

  int k=len-1;

  while(k>=0 && myIsSpace(ptr[k])) k--;

  if (i == 0 && k == len-1)
    return s;
  else if (k>=i && i<len)
    return s.substr(i, k-i+1);
  else return L"";
}

bool fileExists(const wstring &file)
{
  return GetFileAttributes(file.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool stringMatch(const wstring &a, const wstring &b) {
  wstring aa = trim(a);
  wstring bb = trim(b);

  return CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, aa.c_str(), aa.length(), bb.c_str(), bb.length())==2;
}

const string &encodeXML(const string &in)
{
  static string out;
  const char *bf = in.c_str();
  int len = in.length();
  bool needEncode = false;
  for (int k=0;k<len ;k++)
    needEncode |=  (bf[k]=='&') | (bf[k]=='>') | (bf[k]=='<') | (bf[k]=='"') | (bf[k]==0) | (bf[k]=='\n') | (bf[k]=='\r');

  if (!needEncode)
    return in;
  out.clear();
  for (int k=0;k<len ;k++) {
    if (bf[k]=='&')
      out+="&amp;";
    else if (bf[k]=='<')
      out+="&lt;";
    else if (bf[k]=='>')
      out+="&gt;";
    else if (bf[k]=='"')
      out+="&quot;";
    else if (bf[k]=='\n')
      out+="&#10;";
    else if (bf[k]=='\r')
      out+="&#13;";
    else if (bf[k] == 0)
      out+=' ';
    else
      out+=bf[k];
  }
  return out;
}


const wstring &encodeXML(const wstring &in)
{
  static wstring out;//WCS
  const wchar_t *bf = in.c_str();
  int len = in.length();
  bool needEncode = false;
  for (int k=0;k<len ;k++)
    needEncode |=  (bf[k]=='&') | (bf[k]=='>') | (bf[k]=='<') | (bf[k]=='"') | (bf[k]==0) | (bf[k]=='\n') | (bf[k]=='\r');

  if (!needEncode)
    return in;
  out.clear();
  for (int k=0;k<len ;k++) {
    if (bf[k]=='&')
      out+=L"&amp;";
    else if (bf[k]=='<')
      out+=L"&lt;";
    else if (bf[k]=='>')
      out+=L"&gt;";
    else if (bf[k]=='"')
      out+=L"&quot;";
    else if (bf[k]=='\n')
      out+=L"&#10;";
    else if (bf[k]=='\r')
      out+=L"&#13;";
    else if (bf[k] == 0)
      out+=' ';
    else
      out+=bf[k];
  }
  return out;
}

const wstring &encodeHTML(const wstring &in)
{
  static wstring out;//WCS
  const wchar_t *bf = in.c_str();
  int len = in.length();
  bool needEncode = false;
  for (int k=0;k<len ;k++)
    needEncode |=  (bf[k]==' ') | (bf[k]=='&') | (bf[k]=='>') | (bf[k]=='<') | (bf[k]=='"') | (bf[k]==0) | (bf[k]=='\n') | (bf[k]=='\r') | (bf[k]==0x2013);

  if (!needEncode)
    return in;
  out.clear();
  for (int k=0;k<len ;k++) {
    if (bf[k]=='&')
      out+=L"&amp;";
    else if (bf[k]=='<')
      out+=L"&lt;";
    else if (bf[k]=='>')
      out+=L"&gt;";
    else if (bf[k]=='"')
      out+=L"&quot;";
    else if (bf[k]=='\n')
      out+=L"&#10;";
    else if (bf[k]=='\r')
      out+=L"&#13;";
    else if (bf[k] == 0)
      out+=' ';
    else if (bf[k] == 0x2013) {
      out+=L"&ndash;";
    }
    else if (bf[k] == ' ') {
      out+=L"&nbsp;";
    }
    else
      out+=bf[k];
  }
  return out;
}

const string &decodeXML(const string &in)
{
  const char *bf = in.c_str();
  int len = in.length();
  bool needDecode = false;
  for (int k=0;k<len ;k++)
    needDecode |=  (bf[k]=='&');

  if (!needDecode)
    return in;

  static string out;
  out.clear();

  if (len < 50) {
    for (int k=0;k<len ;k++) {
      if (bf[k]=='&') {
        if ( memcmp(&bf[k], "&amp;", 5)==0 )
          out+="&", k+=4;
        else if  ( memcmp(&bf[k], "&lt;", 4)==0 )
          out+="<", k+=3;
        else if  ( memcmp(&bf[k], "&gt;", 4)==0 )
          out+=">", k+=3;
        else if  ( memcmp(&bf[k], "&quot;", 6)==0 )
          out+="\"", k+=5;
        else if  ( memcmp(&bf[k], "&#10;", 5)==0 )
          out+="\n", k+=4;
        else if  ( memcmp(&bf[k], "&#13;", 5)==0 )
          out+="\r", k+=4;
        else
          out+=bf[k];
      }
      else
        out+=bf[k];
    }
  }
  else {
    ostringstream str;
    for (int k=0;k<len ;k++) {
      if (bf[k]=='&') {
        if ( memcmp(&bf[k], "&amp;", 5)==0 )
          str << '&', k+=4;
        else if  ( memcmp(&bf[k], "&lt;", 4)==0 )
          str << '<', k+=3;
        else if  ( memcmp(&bf[k], "&gt;", 4)==0 )
          str << '>', k+=3;
        else if  ( memcmp(&bf[k], "&quot;", 6)==0 )
          str << '\"', k+=5;
        else if  ( memcmp(&bf[k], "&nbsp;", 6)==0 )
          str << ' ', k+=5;
        else if  ( memcmp(&bf[k], "&#10;", 5)==0 )
          str << '\n', k+=4;
        else if  ( memcmp(&bf[k], "&#13;", 5)==0 )
          str << '\r', k+=4;
        else
          str << bf[k];
      }
      else
        str << bf[k];
    }
    out = str.str();
  }
  return out;
}


void inplaceDecodeXML(char *in)
{
  char *bf = in;
  int outp = 0;

  for (int k=0;bf[k] ;k++) {
    if (bf[k] != '&')
      bf[outp++] = bf[k];
    else {
      if ( memcmp(&bf[k], "&amp;", 5)==0 )
        bf[outp++] = '&', k+=4;
      else if  ( memcmp(&bf[k], "&lt;", 4)==0 )
        bf[outp++] = '<', k+=3;
      else if  ( memcmp(&bf[k], "&gt;", 4)==0 )
        bf[outp++] = '>', k+=3;
      else if  ( memcmp(&bf[k], "&quot;", 6)==0 )
        bf[outp++] = '"', k+=5;
      else if  ( memcmp(&bf[k], "&#10;", 5)==0 )
        bf[outp++] = '\n', k+=4;
      else if  ( memcmp(&bf[k], "&#13;", 5)==0 )
        bf[outp++] = '\r', k+=4;
      else
        bf[outp++] = bf[k];
    }
  }
  bf[outp] = 0;
}

const char *decodeXML(const char *in)
{
  const char *bf = in;
  bool needDecode = false;
  for (int k=0; bf[k] ;k++)
    needDecode |=  (bf[k]=='&');

  if (!needDecode)
    return in;

  static string out;
  out.clear();
  for (int k=0;bf[k] ;k++) {
    if (bf[k]=='&') {
      if ( memcmp(&bf[k], "&amp;", 5)==0 )
        out+="&", k+=4;
      else if  ( memcmp(&bf[k], "&lt;", 4)==0 )
        out+="<", k+=3;
      else if  ( memcmp(&bf[k], "&gt;", 4)==0 )
        out+=">", k+=3;
      else if  ( memcmp(&bf[k], "&quot;", 6)==0 )
        out+="\"", k+=5;
      else if  ( memcmp(&bf[k], "&#10;", 5)==0 )
        out+="\n", k+=4;
      else if  ( memcmp(&bf[k], "&#13;", 5)==0 )
        out+="\r", k+=4;
      else
        out+=bf[k];
    }
    else
      out+=bf[k];
  }

  return out.c_str();
}

void static setChar(wchar_t *map, wchar_t pos, wchar_t value)
{
  map[pos] = value;
}

int toLowerStripped(wchar_t c) {
  if (c>='A' && c<='Z')
    return c + ('a' - 'A');
  else if (c<128)
    return c;

  static wchar_t *map = 0;
  if (map == nullptr) {
    map = new wchar_t[65536];
    for (int i = 0; i < 65536; i++)
      map[i] = i;

    setChar(map, L'Å', L'a');
    setChar(map, L'Ä', L'a');
    setChar(map, L'Ö', L'o');

    setChar(map, L'É', L'e');
    setChar(map, L'é', L'e');
    setChar(map, L'è', L'e');
    setChar(map, L'È', L'e');
    setChar(map, L'ë', L'e');
    setChar(map, L'Ë', L'e');
    setChar(map, L'ê', L'e');
    setChar(map, L'Ê', L'e');

    setChar(map, L'û', L'u');
    setChar(map, L'Û', L'u');
    setChar(map, L'ü', L'u');
    setChar(map, L'Ü', L'u');
    setChar(map, L'ú', L'u');
    setChar(map, L'Ú', L'u');
    setChar(map, L'ù', L'u');
    setChar(map, L'Ù', L'u');

    setChar(map, L'ñ', L'n');
    setChar(map, L'Ñ', L'n');

    setChar(map, L'ä', L'a');
    setChar(map, L'å', L'a');
    setChar(map, L'á', L'a');
    setChar(map, L'Á', L'a');
    setChar(map, L'à', L'a');
    setChar(map, L'À', L'a');
    setChar(map, L'â', L'a');
    setChar(map, L'Â', L'a');
    setChar(map, L'ã', L'a');
    setChar(map, L'Ã', L'a');

    setChar(map, L'ï', L'i');
    setChar(map, L'Ï', L'i');
    setChar(map, L'î', L'i');
    setChar(map, L'Î', L'i');
    setChar(map, L'í', L'i');
    setChar(map, L'Í', L'i');
    setChar(map, L'ì', L'i');
    setChar(map, L'Ì', L'i');

    setChar(map, L'ó', L'o');
    setChar(map, L'Ó', L'o');
    setChar(map, L'ò', L'o');
    setChar(map, L'Ò', L'o');
    setChar(map, L'õ', L'o');
    setChar(map, L'Õ', L'o');
    setChar(map, L'ô', L'o');
    setChar(map, L'Ô', L'o');
    setChar(map, L'ö', L'o');

    setChar(map, L'ý', L'y');
    setChar(map, L'Ý', L'Y');
    setChar(map, L'ÿ', L'y');
    
    setChar(map, L'Æ', L'a');
    setChar(map, L'æ', L'a');

    setChar(map, L'Ø', L'o');
    setChar(map, L'ø', L'o');

    setChar(map, L'Ç', L'c');
    setChar(map, L'ç', L'c');

    wstring srcEx = L"ĂăĄąĆćĈĉĊċČčĎďĐđĒēĔĕĖėĘęĚěĜĝĞğĠġĢģĤĥĦħĨĩĪīĬĭĮįİıĲĳĴĵĶķĸĹĺĻĽľĿŀŁł"
                    L"ŃńŅņŇňŉŊŋŌōŎŏŐőŒœŔŕŖŗŘřŚśŜŝŞşŠšŢţŤťŦŧŨũŪūŬŭŮůŰűŲųŴŵŶŷŸŹźŻżŽž";
    wstring dstEx = L"aaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiijjjjkkklllllllll"
                    L"nnnnnnnnnooooooaarrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzz";

    assert(srcEx.size() == dstEx.size());
    for (int j = 0; j < srcEx.size(); j++)
      setChar(map, srcEx[j], dstEx[j]);
  }
  int a = map[c];
  return a;
}

const wchar_t *canonizeName(const wchar_t *name)
{
  static wchar_t out[70];
  static wchar_t tbf[70];
  
  for (int i = 0; i<63 && name[i]; i++) {
    if (name[i] == ',') {
      int tout = 0;
      for (int j = i+1; j < 63 && name[j]; j++) {
        tbf[tout++] = name[j];
      }      
      tbf[tout++] = ' ';
      for (int j = 0; j < i; j++) {
        tbf[tout++] = name[j];
      }
      tbf[tout] = 0;
      name = tbf;
      break;
    }
  }

  int outp = 0;
  int k = 0;
  for (k=0; k<63 && name[k]; k++) {
    if (name[k] != ' ')
      break;
  }

  bool first = true;
  while (k<63 && name[k]) {
    if (!first)
      out[outp++] = ' ';

    while(name[k]!= ' ' && k<63 && name[k]) {
      if (name[k] == '-')
        out[outp++] = ' ';
      else
        out[outp++] = toLowerStripped(name[k]);
      k++;
    }

    first = false;
    while(name[k] == ' ' && k<64)
      k++;
  }

  out[outp] = 0;
  return out;
}

const int notFound = 1000000;

int charDist(const wchar_t *b, int len, int origin, wchar_t c)
{
  int i;
  int bound = max(1, min(len/2, 4));
  for (int k = 0;k<bound; k++) {
    i = origin - k;
    if (i>0 && b[i] == c)
      return -k;
    i = origin + k;
    if (i<len && b[i] == c)
      return k;
  }
  return notFound;
}

static double stringDistance(const wchar_t *a, int al, const wchar_t *b, int bl)
{
  al = min (al, 256);
  int d1[256];
  int avg = 0;
  int missing = 0;
  int ndiff = 1; // Do not allow zero
  for (int k=0; k<al; k++) {
    int d = charDist(b, bl, k, a[k]);
    if (d == notFound) {
      missing++;
      d1[k] = 0;
    }
    else {
      d1[k] = d;
      avg += d;
      ndiff ++;
    }
  }
  if (missing>min(3, max(1, al/3)))
    return 1;

  double mfactor = double(missing*0.8);
  double center = double(avg)/double(ndiff);

  double dist = 0;
  for (int k=0; k<al; k++) {
    double ld = min<double>(fabs(d1[k] - center), abs(d1[k]));
    dist += ld*ld;
  }

  return (sqrt(dist)+mfactor*mfactor)/double(al);
}

double stringDistanceAssymetric(const wstring &target, const wstring &sample) {
  double d = stringDistance(target.c_str(), target.length(), sample.c_str(), sample.length());
  return min(1.0, d);
}


double stringDistance(const wchar_t *a, const wchar_t *b)
{
  int al = wcslen(a);
  int bl = wcslen(b);

  double d1 = stringDistance(a, al, b, bl);
  if (d1 >= 1)
    return 1.0;

  double d2 = stringDistance(b, bl, a, al);
  if (d2 >= 1)
    return 1.0;

  return (max(d1, d2) + d1 + d2)/3.0;
}

int getNumberSuffix(const string &str)
{
  int pos = str.length();

  while (pos>1 && (str[pos-1] & (~127)) == 0  && (isspace(str[pos-1]) || isdigit(str[pos-1]))) {
    pos--;
  }

  if (pos == str.length())
    return 0;
  return atoi(str.c_str() + pos);
}

int getNumberSuffix(const wstring &str)
{
  int pos = str.length();

  while (pos>1 && (str[pos-1] & (~127)) == 0  && (isspace(str[pos-1]) || isdigit(str[pos-1]))) {
    pos--;
  }

  if (pos == str.length())
    return 0;
  return _wtoi(str.c_str() + pos);
}

int extractAnyNumber(const wstring &str, wstring &prefix, wstring &suffix)
{
  const wchar_t *ptr = (const wchar_t*)str.c_str();
  for (size_t k = 0; k<str.length(); k++) {
    if (isdigit(ptr[k])) {
      prefix = str.substr(0, k);
      int num = _wtoi(str.c_str() + k);
      while(k<str.length() && (str[++k] & ~0x7F) == 0 && isdigit(str[k]));
        suffix = str.substr(k);

      return num;
    }
  }
  return -1;
}

static void decomposeClassName(const wstring &name, vector<wstring> &dec) {
  if (name.empty())
    return;

  dec.push_back(wstring());
  
  for (size_t i = 0; i < name.size(); i++) {
    int bchar = toLowerStripped(name[i]);
    if (myIsSpace(bchar) || bchar == '-' || bchar == 160) {
      if (!dec.back().empty())
        dec.push_back(wstring());
      continue;
    }
    if (!dec.back().empty()) {
      int last = *dec.back().rbegin();
      bool lastNum = last >= '0' && last <= '9';
      bool isNum = bchar >= '0' && bchar <= '9';

      if (lastNum^isNum)
        dec.push_back(wstring()); // Change num/ non-num
    }
    dec.back().push_back(bchar);
  }
  if (dec.back().empty())
    dec.pop_back();

  sort(dec.begin(), dec.end());
}

/** Matches H21 L with H21 Lång and H21L 
 but not Violet with Violet Court, which is obviously wrong.
 */
bool compareClassName(const wstring &a, const wstring &b)
{
  if (a == b)
    return true;

	vector<wstring> acanon;
	vector<wstring> bcanon;

  decomposeClassName(a, acanon);
  decomposeClassName(b, bcanon);

  if (acanon.size() != bcanon.size())
    return false;

  for (size_t k = 0; k < acanon.size(); k++) {
    if (acanon[k] == bcanon[k])
      continue;

    int sample = acanon[k][0];
    if (sample >= '0' && sample <= '9')
      return false; // Numbers must match

    if (acanon[k][0] == bcanon[k][0] && (acanon[k].length() == 1 || bcanon[k].length() == 1))
      continue; // Abbrevation W <-> Women etc

    return false; // No match
  }

  return true; // All parts matched
}

wstring getErrorMessage(int code) {
  LPVOID msg;

  if (code == ERROR_INTERNET_TIMEOUT) {
    return L"Timeout";
  }

  int s = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &msg, 0, NULL);
  if (s==0 || !msg) {
    if (code != 0) {
      wchar_t ch[128];
      swprintf_s(ch, L"Error code: %d", code);
      return ch;
    }
    return L"";
  }

  wstring str = LPCTSTR(msg);
  if (str.empty() && code>0) {
    wchar_t ch[128];
    swprintf_s(ch, L"Error code: %d", code);
    str = ch;
  }

  LocalFree(msg);
  return str;
}

#define  HLSMAX   252 /* H,L, and S vary over 0-HLSMAX */
#define  RGBMAX   255   /* R,G, and B vary over 0-RGBMAX */
                        /* HLSMAX BEST IF DIVISIBLE BY 6 */
                        /* RGBMAX, HLSMAX must each fit in a byte. */

/* Hue is undefined if Saturation is 0 (grey-scale) */
#define UNDEFINED (HLSMAX*2/3)

HLS &HLS::RGBtoHLS(DWORD lRGBColor)
{
  WORD R,G,B;          /* input RGB values */
  BYTE cMax,cMin;      /* max and min RGB values */
  WORD  Rdelta,Gdelta,Bdelta; /* intermediate value: % of spread from max
                              */
  /* get R, G, and B out of DWORD */
  R = GetRValue(lRGBColor);
  G = GetGValue(lRGBColor);
  B = GetBValue(lRGBColor);
  short &L = lightness;
  short &H = hue;
  short &S = saturation;

  /* calculate lightness */
  cMax = (BYTE)max( max(R,G), B);
  cMin = (BYTE)min( min(R,G), B);
  L = ( ((cMax+cMin)*HLSMAX) + RGBMAX )/(2*RGBMAX);

  if (cMax == cMin) {           /* r=g=b --> achromatic case */
    S = 0;                     /* saturation */
    H = UNDEFINED;             /* hue */
  }
  else {                        /* chromatic case */
    /* saturation */
    if (L <= (HLSMAX/2))
      S = ( ((cMax-cMin)*HLSMAX) + ((cMax+cMin)/2) ) / (cMax+cMin);
    else
      S = ( ((cMax-cMin)*HLSMAX) + ((2*RGBMAX-cMax-cMin)/2) )
      / (2*RGBMAX-cMax-cMin);

    /* hue */
    Rdelta = ( ((cMax-R)*(HLSMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin);
    Gdelta = ( ((cMax-G)*(HLSMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin);
    Bdelta = ( ((cMax-B)*(HLSMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin);

    if (R == cMax)
      H = Bdelta - Gdelta;
    else if (G == cMax)
      H = (HLSMAX/3) + Rdelta - Bdelta;
    else /* B == cMax */
      H = ((2*HLSMAX)/3) + Gdelta - Rdelta;

    if (H < 0)
      H += HLSMAX;
    if (H > HLSMAX)
      H -= HLSMAX;
  }
  return *this;
}


/* utility routine for HLStoRGB */
WORD HLS::HueToRGB(WORD n1, WORD n2, WORD hue) const
{
  /* range check: note values passed add/subtract thirds of range */
  if (hue < 0)
    hue += HLSMAX;

  if (hue > HLSMAX)
    hue -= HLSMAX;

  /* return r,g, or b value from this tridrant */
  if (hue < (HLSMAX/6))
    return ( n1 + (((n2-n1)*hue+(HLSMAX/12))/(HLSMAX/6)) );
  if (hue < (HLSMAX/2))
    return ( n2 );
  if (hue < ((HLSMAX*2)/3))
    return ( n1 +    (((n2-n1)*(((HLSMAX*2)/3)-hue)+(HLSMAX/12))/(HLSMAX/6))
    );
  else
    return ( n1 );
}

DWORD HLS::HLStoRGB() const
{
  const WORD &lum = lightness;
  const WORD &sat = saturation;

  WORD R,G,B;                /* RGB component values */
  WORD  Magic1,Magic2;       /* calculated magic numbers (really!) */

  if (sat == 0) {            /* achromatic case */
    R=G=B=(lum*RGBMAX)/HLSMAX;
    if (hue != UNDEFINED) {
      /* ERROR */
    }
  }
  else  {                    /* chromatic case */
    /* set up magic numbers */
    if (lum <= (HLSMAX/2))
      Magic2 = (lum*(HLSMAX + sat) + (HLSMAX/2))/HLSMAX;
    else
      Magic2 = lum + sat - ((lum*sat) + (HLSMAX/2))/HLSMAX;
    Magic1 = 2*lum-Magic2;

    /* get RGB, change units from HLSMAX to RGBMAX */
    R = (HueToRGB(Magic1,Magic2,hue+(HLSMAX/3))*RGBMAX +
      (HLSMAX/2))/HLSMAX;
    G = (HueToRGB(Magic1,Magic2,hue)*RGBMAX + (HLSMAX/2)) / HLSMAX;
    B = (HueToRGB(Magic1,Magic2,hue-(HLSMAX/3))*RGBMAX +
      (HLSMAX/2))/HLSMAX;
  }
  return(RGB(R,G,B));
}

void HLS::lighten(double f) {
  lightness = min<WORD>(HLSMAX, WORD(f*lightness));
}

void HLS::saturate(double s) {
  saturation = min<WORD>(HLSMAX, WORD(s*saturation));
}

void HLS::colorDegree(double d) {

}

bool isAscii(const string &s) {
  for (size_t k = 0; k<s.length(); k++)
    if (!isascii(s[k]))
      return false;
  return true;
}

bool isNumber(const string &s) {
  int len = s.length();
  for (int k = 0; k < len; k++) {
    if (!isdigit(s[k]))
      return false;
  }
  return len > 0;
}

bool isAscii(const wstring &s) {
  for (size_t k = 0; k<s.length(); k++)
    if (!iswascii(s[k]))
      return false;
  return true;
}

bool isNumber(const wstring &s) {
  int len = s.length();
  for (int k = 0; k < len; k++) {
    if ( (s[k]&127)!=s[k] || !isdigit(s[k]))
      return false;
  }
  return len > 0;
}

int convertDynamicBase(const wstring &s, long long &out) {
  out = 0;
  if (s.empty())
    return 0;

  bool alpha = false;
  bool general = false;
  int len = s.length();
  for (int k = 0; k < len; k++) {
    unsigned c = s[k];
    if (c >= '0' && c <= '9')
      continue;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      alpha = true;
      continue;
    }
    general = true;
    if (c<32)
      return 0; // Not a supported character
  }

  int base = general ? 256-32 : (alpha ? 36 : 10);
  long long factor = 1;
  for (int k = len-1; k >= 0; k--) {
    unsigned c = s[k]&0xFF;
    if (general)
      c -= 32;
    else {
      if (c >= '0' && c <= '9')
        c -= '0';
      else if (c >= 'A' && c <= 'Z')
        c -= 'A'-10;
      else if (c >= 'a' && c <= 'z')
        c -= 'a'-10;
    }
    out += factor * c;
    factor *= base;
  }

  return base;
}

void convertDynamicBase(long long val, int base, wchar_t out[16]) {
  int len = 0;
  while (val != 0) {
    unsigned int c = val % base;
    val = val / base;
    char cc;
    if (base == 10)
      cc = '0' + c;
    else if (base == 36) {
      if (c < 10)
        cc = '0' + c;
      else
        cc = 'A' + c - 10;
    }
    else {
      cc = c+32;
    }
    out[len++] = cc;
  }
  out[len] = 0;
  reverse(out, out+len);
}

bool expandDirectory(const wchar_t *file, const wchar_t *filetype, vector<wstring> &res)
{
  WIN32_FIND_DATA fd;

  wchar_t dir[MAX_PATH];
  wchar_t fullPath[MAX_PATH];

  if (file[0] == '.') {
    GetCurrentDirectory(MAX_PATH, dir);
    wcscat_s(dir, file+1);
  }
  else
    wcscpy_s(dir, MAX_PATH, file);

  if (dir[wcslen(dir)-1]!='\\')
    wcscat_s(dir, MAX_PATH, L"\\");

  wcscpy_s(fullPath, MAX_PATH, dir);
  wcscat_s(dir, MAX_PATH, filetype);

  HANDLE h=FindFirstFile(dir, &fd);

  if (h == INVALID_HANDLE_VALUE)
    return false;

  bool more = true;

  while (more) {
    if (fd.cFileName[0] != '.') {
      //Avoid .. and .
      wchar_t fullPathFile[MAX_PATH];
      wcscpy_s(fullPathFile, MAX_PATH, fullPath);
      wcscat_s(fullPathFile, MAX_PATH, fd.cFileName);
      res.push_back(fullPathFile);
    }
    more=FindNextFile(h, &fd)!=0;
  }

  FindClose(h);
  return true;
}

wstring encodeSex(PersonSex sex) {
  if (sex == sFemale)
    return L"F";
  else if (sex == sMale)
    return L"M";
  else if (sex == sBoth)
    return L"B";
  else
    return L"";
}

PersonSex interpretSex(const wstring &sex) {
  int sexC = sex.empty() ? 0 : sex[0];
  if (sexC == 'F' || sexC == 'K' || sexC == 'W' || sexC == 'f' || sexC == 'k' || sexC == 'w')
    return sFemale;
  else if (sexC == 'M' || sexC == 'H' || sexC == 'm' || sexC == 'h')
    return sMale;
  else if (sexC == 'B' || sexC == 'b')
    return sBoth;
  else
    return sUnknown;
}

bool matchNumber(int a, const wchar_t *b) {
  if (a == 0 && b[0])
    return false;

  wchar_t bf[32];
  _itow_s(a, bf, 10);

  // Check matching substring
  for (int k = 0; k < 12; k++) {
    if (b[k] == 0)
      return true;
    if (bf[k] != b[k])
      return false;
  }

  return false;
}

wstring makeValidFileName(const wstring &input, bool strict) {
  wstring out;
  out.reserve(input.size());

  if (strict) {
    for (size_t k = 0; k < input.length(); k++) {
      wchar_t b = input[k];
      if ( (b>='0' && b<='9') || (b>='a' && b<='z') || (b>='A' && b<='Z') || b == '_' || b=='.' )
        out.push_back(b);
      else if (b == ' ' ||  b == ',')
        out.push_back('_');
      else {
        b = toLowerStripped(b);
        
        if (b >= 'a' && b <= 'z')
          b = b;
        else if ( b == L'ö')
          b = 'o';
        else if (b == L'ä' || b == L'å' || b== L'à' || b == L'á' || b == L'â' || b == L'ã' || b == L'æ')
          b = 'a';
        else if (b == L'ç')
          b = 'c';
        else if (b == L'è' || b == L'é' || b == L'ê' || b == L'ë')
          b = 'e';
        else if (b == L'ð')
          b = 't';
        else if (b == L'ï' || b == L'ì' || b == L'ï' || b == L'î' || b == L'í')
          b = 'i';
        else if (b == L'ò' || b == L'ó' || b == L'ô' || b == L'õ' || b == L'ø')
          b = 'o';
        else if (b == L'ù' || b == L'ú' || b == L'û' || b == L'ü')
          b = 'u';
        else if (b == L'ý')
          b = 'y';
        else
          b = '-';

        out.push_back(b);
      }
    }
  }
  else {
     for (size_t k = 0; k < input.length(); k++) {
      wchar_t b = input[k];
      if (b < 32 || b == '*' || b == '?' || b==':' || b=='/' || b == '\\')
        b = '_';
      out.push_back(b);
     }
  }
  return out;
}


string makeValidFileName(const string& input, bool strict) {
  string out;
  out.reserve(input.size());

  if (strict) {
    for (size_t k = 0; k < input.length(); k++) {
      char b = input[k];
      if ((b >= '0' && b <= '9') || (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || b == '_' || b == '.')
        out.push_back(b);
      else if (b == ' ' || b == ',')
        out.push_back('_');
      else {
        b = toLowerStripped(b);

        if (b >= 'a' && b <= 'z')
          b = b;
        else if (b == 'ö')
          b = 'o';
        else if (b == 'ä' || b == 'å' || b == 'à' || b == 'á' || b == 'â' || b == 'ã' || b == 'æ')
          b = 'a';
        else if (b == 'ç')
          b = 'c';
        else if (b == 'è' || b == 'é' || b == 'ê' || b == 'ë')
          b = 'e';
        else if (b == 'ð')
          b = 't';
        else if (b == 'ï' || b == 'ì' || b == 'ï' || b == 'î' || b == 'í')
          b = 'i';
        else if (b == 'ò' || b == 'ó' || b == 'ô' || b == 'õ' || b == 'ø')
          b = 'o';
        else if (b == 'ù' || b == 'ú' || b == 'û' || b == 'ü')
          b = 'u';
        else if (b == 'ý')
          b = 'y';
        else
          b = '-';

        out.push_back(b);
      }
    }
  }
  else {
    for (size_t k = 0; k < input.length(); k++) {
      char b = input[k];
      if (b < 32 || b == '*' || b == '?' || b == ':' || b == '/' || b == '\\')
        b = '_';
      out.push_back(b);
    }
  }
  return out;
}


void capitalize(wstring &str) {
  if (str.length() > 0) {
    auto bf = str.c_str();
    CharUpperBuff(const_cast<LPWSTR>(bf), 1);
  }
}

bool checkValidDate(const wstring &date) {
  SYSTEMTIME st;
  if (convertDateYMD(date, st, false) <= 0)
    return false;

  st.wHour = 12;
  SYSTEMTIME utc;
  if (!TzSpecificLocalTimeToSystemTime(0, &st, &utc)) {
    return false;
  }

  return true;
}

/** Return bias in seconds. UTC = local time + bias. */
int getTimeZoneInfo(const wstring &date) {
  static wchar_t lastDate[16] = {0};
  static int lastValue = -1;
  // Local caching
  if (lastValue != -1 && lastDate == date) {
    return lastValue;
  }
  wcscpy_s(lastDate, 16, date.c_str());
//  TIME_ZONE_INFORMATION tzi;
  SYSTEMTIME st;
  convertDateYMD(date, st, false);
  st.wHour = 12;
  SYSTEMTIME utc;
  if (!TzSpecificLocalTimeToSystemTime(0, &st, &utc)) {
    lastValue = 0;
    return 0;
  }

  int datecode = ((st.wYear * 12 + st.wMonth) * 31) + st.wDay;
  int datecodeUTC = ((utc.wYear * 12 + utc.wMonth) * 31) + utc.wDay;

  int daydiff = 0;
  if (datecodeUTC > datecode)
    daydiff = 1;
  else if (datecodeUTC < datecode)
    daydiff = -1;

  int t = st.wHour * timeConstSecPerHour;
  int tUTC = daydiff * 24 * timeConstSecPerHour + utc.wHour * timeConstSecPerHour + utc.wMinute * timeConstSecPerMin + utc.wSecond;

  lastValue = tUTC - t;
  return lastValue;
}

wstring getTimeZoneString(const wstring &date) {
  int a = getTimeZoneInfo(date);
  if (a == 0)
    return L"+00:00";
  else if (a>0) {
    wchar_t bf[12];
    swprintf_s(bf, L"-%02d:%02d", a/timeConstSecPerHour, (a/timeConstMinPerHour)%60);
    return bf;
  }
  else {
    wchar_t bf[12];
    swprintf_s(bf, L"+%02d:%02d", a/-timeConstSecPerHour, (a/-timeConstMinPerHour)%60);
    return bf;
  }
}

bool compareBib(const wstring &b1, const wstring &b2) {
  int l1 = b1.length();
  int l2 = b2.length();
  if (l1 != l2)
    return l1 < l2;

  wchar_t maxc = 0, minc = numeric_limits<wchar_t>::max(); 
  for (int k = 0; k < l1; k++) {
    wchar_t b = b1[k];
    maxc = max(maxc, b);
    minc = min(minc, b);
  }
  for (int k = 0; k < l2; k++) {
    wchar_t b = b2[k];
    maxc = max(maxc, b);
    minc = min(minc, b);
  }

  unsigned coeff = maxc-minc + 1;

  unsigned z1 = 0;
  for (int k = 0; k < l1; k++) {
    wchar_t b = b1[k]-minc;
    z1 = coeff * z1 + b;
  }

  unsigned z2 = 0;
  for (int k = 0; k < l2; k++) {
    wchar_t b = b2[k]-minc;
    z2 = coeff * z2 + b;
  }

  return z1 < z2;
}

/// Split a name into first name and last name
int getNameCommaSplitPoint(const wstring &name) {
  int commaSplit = -1;

  for (unsigned k = 1; k + 1 < name.size(); k++) {
    if (name[k] == ',') {
      commaSplit = k;
      break;
    }
  }

  if (commaSplit >= 0) {
    commaSplit += 2;
  }
  
  return commaSplit;
}

/// Split a name into first name and last name
int getNameSplitPoint(const wstring &name) {
  int split[10];
  int nSplit = 0;

  for (unsigned k = 1; k + 1<name.size(); k++) {
    if (iswspace(name[k])) {
      split[nSplit++] = k;
      if ( nSplit>=9 )
        break;
    }
  }
  if (nSplit == 1)
    return split[0] + 1;
  else if (nSplit == 0)
    return -1;
  else {
    const oWordList &givenDB = lang.get().getGivenNames();
    int sp_ix = 0;
    for (int k = 1; k<nSplit; k++) {
      wstring sn = name.substr(split[k-1]+1, split[k] - split[k-1]-1);
      if (!givenDB.lookup(sn.c_str()))
        break;
      sp_ix = k;
    }
    return split[sp_ix]+1;
  }
}

wstring getGivenName(const wstring &name) {
  int sp = getNameCommaSplitPoint(name);
  if (sp != -1) {
    return trim(name.substr(sp));
  }

  sp = getNameSplitPoint(name);
  if (sp == -1)
    return trim(name);
  else
    return trim(name.substr(0, sp));
}

wstring getFamilyName(const wstring &name) {
  int sp = getNameCommaSplitPoint(name);
  if (sp != -1) {
    return trim(name.substr(0, sp - 2));
  }

  sp = getNameSplitPoint(name);
  if (sp == -1)
    return _EmptyWString;
  else
    return trim(name.substr(sp));
}

static bool noCapitalize(const wstring &str, size_t pos) {
  string word;
  while (pos < str.length() && !myIsSpace(str[pos])) {
    word.push_back(char(str[pos++]));
  }

  if (word == "of" || word == "for" || word == "at" || word == "by" || word == "on")
    return true;

  if (word == "and" || word == "or" || word == "from" || word == "as" || word == "in")
    return true;

  if (word == "with")
    return true;

  if (word == "to" || word == "next" || word == "a" || word == "an" || word == "the" || word == "but")
    return true;

  return false;
}

void capitalizeWords(wstring &str) {
  bool init = true;
  for (size_t i = 0; i < str.length(); i++) {
    wchar_t c = str[i];
    if (init && c >= 'a' && c <= 'z' && !noCapitalize(str, i))
      str[i] = c + ('A' - 'a');
    init = iswspace(c) != 0 || c == '/' || c == '-';
  }
}

void MeOSFileLock::unlockFile() {
  if (lockedFile != INVALID_HANDLE_VALUE)
    CloseHandle(lockedFile);

  lockedFile = INVALID_HANDLE_VALUE;
}

void MeOSFileLock::lockFile(const wstring &file) {
  unlockFile();
  lockedFile = CreateFile(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 
                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  //lockedFile = _open(file.c_str(), _O_RDWR);
  if (lockedFile == INVALID_HANDLE_VALUE) {
    int err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION)
      throw meosException("open_error_locked");
    else {
      TCHAR buff[256];
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buff, sizeof(buff), 0);
      throw meosException(L"open_error#" + file + L"#" + buff);
    }
  }
}

void processGeneralTime(const wstring &generalTime, wstring &meosTime, wstring &meosDate) {
  meosTime = L"";
  meosDate = L"";
  vector<wstring> parts;
  split(generalTime, L":-,. /\t", parts);
  
  // Indices into parts
  int year = -2;
  int month = -2;
  int day = -2;

  int hour = -2;
  int minute = -2;
  int second = -2;
  int subsecond = -2;
  
  int found = 0, base = -1, iter = 0;
  bool pm = wcsstr(generalTime.c_str(), L"PM") != 0 || 
            wcsstr(generalTime.c_str(), L"pm") != 0;
          
  while (iter < 2 && second==-2) {
    if (base == found)
      iter++;

    base = found;
    for (size_t k = 0; k < parts.size(); k++) {
      if (parts[k].empty())
        continue;
      int number = _wtoi(parts[k].c_str());
      if (number == 0 && parts[k][0] != '0')
        number = -1; // Not a number

      if (iter == 0) {
        // Date
        if (number > 1900 && number < 3000 && year < 0) {
          found++;
          year = k;
        }
        else if (number >= 1 && number <= 12 && month < 0 && (year == k+1 || year == k-1) ) {
          month = k;
          found++;
        }
        else if (number >= 1 && number <=31 && day < 0 && (month == k+1 || month == k-1)) {
          day = k;
          found++;
          iter++;
          break;
        }
        else if (number > 1011900 && number < 30000101 && year < 0 && month < 0 && day < 0) {
          day = k; //Date with format 20160906 or 06092016
          month = k;
          year = k;
          found++;
          break;
        }
      }
      else if (iter == 1) {
        
        // Time
        if (number >= 0 && number <= 24 && year != k && day != k && month != k && hour < 0) {
          hour = k;
          found++;
        }
        else if (number >= 0 && number <= 59 && minute < 0 && hour == k-1) {
          minute = k;
          found++;
        }
        else if (number >= 0 && number <= 59 && second < 0 && minute == k-1) {
          second = k;
          found++;
        }
        else if (number >= 0 && number < 1000 && subsecond < 0 && second == k-1 && k != year) {
          subsecond = k;
          found++;
          iter++;
          break;
        }
      }
    }
  }

  if (second >= 0 && minute >= 0 && hour>= 0) {
    if (!pm)
      meosTime = parts[hour] + L":" + parts[minute] + L":" + parts[second];
    else {
      int rawHour = _wtoi(parts[hour].c_str());
      if (rawHour < 12)
        rawHour+=12;
      meosTime = itow(rawHour) + L":" + parts[minute] + L":" + parts[second];
    }
  }

  if (year >= 0 && month >= 0 && day >= 0) {
    int y = -1, m = -1, d = -1;
    if (year != month) {
      y = _wtoi(parts[year].c_str());
      m = _wtoi(parts[month].c_str());
      d = _wtoi(parts[day].c_str());

      //meosDate = parts[year] + "-" + parts[month] + "-" + parts[day];
    } 
    else {
      int td = _wtoi(parts[year].c_str());
      int y1 = td / 10000;
      int m1 = (td / 100) % 100;
      int d1 = td % 100;
      bool ok = y1 > 2000 && y1 < 3000 && m1>=1 && m1<=12 && d1 >= 1 && d1 <= 31;
      if (!ok) {
        y1 = td % 10000;
        m1 = (td / 10000) % 100;
        d1 = (td / 1000000);

        ok = y1 > 2000 && y1 < 3000 && m1>=1 && m1<=12 && d1 >= 1 && d1 <= 31;
      }
      if (ok) {
        y = y1;
        m = m1;
        d = d1;
      }
        meosDate = itow(y1) + L"-" + itow(m1) + L"-" + itow(d1);
    }
    if (y > 0) {
      wchar_t bf[24];
      swprintf_s(bf, 24, L"%d-%02d-%02d", y, m, d);
      meosDate = bf;
    }
  }

}

void string2Wide(const string &in, wstring &out) {
  int cp = defaultCodePage;
  if (in.empty()) {
    out = L"";
    return;
  }
  out.reserve(in.size() + 1);
  out.resize(in.size(), 0);
  MultiByteToWideChar(cp, MB_PRECOMPOSED, in.c_str(), in.size(), &out[0], out.size() * sizeof(wchar_t));
}

void wide2String(const wstring &in, string &out) {
  out.clear();
  out.insert(out.begin(), in.begin(), in.end());// XXX Simple extend
}

void checkWriteAccess(const wstring &file) {
  int flag = CREATE_NEW;
  if (_waccess(file.c_str(), 4) == 0) {
    flag = OPEN_EXISTING;
  }

  auto h = CreateFile(file.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, flag, FILE_ATTRIBUTE_NORMAL, 0);
  if (h == INVALID_HANDLE_VALUE) {
    wchar_t absPath[260];
    _wfullpath(absPath, file.c_str(), 260);

    DWORD err = GetLastError();
    
    if (err == ERROR_ACCESS_DENIED)
      throw meosException(wstring(L"Behörighet saknas för att skriva till 'X'.#") + absPath);
    else
      throw meosException(wstring(L"Kunde inte skriva till 'X'.#") + absPath);

  }
  CloseHandle(h);
}

void moveFile(const wstring& src, const wstring& dst) {
  DeleteFile(dst.c_str());
  if (!MoveFile(src.c_str(), dst.c_str())) {
    throw meosException(L"Kunde inte skriva till 'X'.#" + dst);
  }
}

int compareStringIgnoreCase(const wstring &a, const wstring &b) {
  return CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, a.c_str(), a.length(),
                       b.c_str(), b.length()) - CSTR_EQUAL;
}

const char* meosException::narrow(const wstring& msg) {
  static string nmsg(msg.begin(), msg.end());
  return nmsg.c_str();
}

int parseRelativeTime(const char *data) {
  if (data) {
    int ret = atoi(data);
    if (timeConstSecond > 1) {
      int j = 0;
      while (data[j]) {
        if (data[j] == '.') {
          int t = data[j + 1] - '0';
          if (t > 0 && t < 10) {
            if (ret < 0 || data[0] == '-')
              return ret * timeConstSecond - t;
            else
              return ret * timeConstSecond + t;
          }
          break;
        }
        j++;
      }
    }
    if (ret == -1)
      return ret; // Special value

    return ret * timeConstSecond;
  }
  return 0;
}

int parseRelativeTime(const wchar_t *data) {
  if (data) {
    int ret = _wtoi(data);
    if (timeConstSecond > 1) {
      int j = 0;
      while (data[j]) {
        if (data[j] == '.') {
          int t = data[j + 1] - '0';
          if (t > 0 && t < 10) {
            if (ret < 0 || data[0] == '-')
              return ret * timeConstSecond - t;
            else
              return ret * timeConstSecond + t;
          }
          break;
        }
        j++;
      }
    }
    if (ret == -1)
      return ret; // Special value

    return ret * timeConstSecond;
  }
  return 0;
}

const wstring &codeRelativeTimeW(int rt) {
  wchar_t bf[32];
  int subSec = timeConstSecond == 1 ? 0 : rt % timeConstSecond;

  if (timeConstSecond == 1 || rt == -1)
    return itow(rt);
  else if (subSec == 0 && rt != -10)
    return itow(rt / timeConstSecond);
  else if (rt > 0) {
    swprintf_s(bf, L"%d.%d", rt / timeConstSecond, rt % timeConstSecond);
  }
  else {
    rt = -rt;
    swprintf_s(bf, L"-%d.%d", rt / timeConstSecond, rt % timeConstSecond);
  }
  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

const string &codeRelativeTime(int rt) {
  char bf[32];
  int subSec = timeConstSecond == 1 ? 0 : rt % timeConstSecond;

  if (timeConstSecond == 1 || rt == -1)
    return itos(rt);
  else if (subSec == 0 && rt != -10)
    return itos(rt / timeConstSecond);
  else if (rt > 0) {
    sprintf_s(bf, "%d.%d", rt / timeConstSecond, rt % timeConstSecond);
  }
  else {
    rt = -rt;
    sprintf_s(bf, "-%d.%d", rt / timeConstSecond, rt % timeConstSecond);
  }
  string &res = StringCache::getInstance().get();
  res = bf;
  return res;
}

wstring addOrSubtractDays(const wstring& m, int days) {
  // Convert wstring date to SYSTEMTIME
  SYSTEMTIME st;
  convertDateYMD(m, st, false);

  // Convert SYSTEMTIME to FILETIME
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);

  // Convert FILETIME to ULARGE_INTEGER for arithmetic
  ULARGE_INTEGER ui;
  ui.LowPart = ft.dwLowDateTime;
  ui.HighPart = ft.dwHighDateTime;

  // Add/subtract the number of days in 100-nanosecond intervals
  const LONGLONG intervals_per_day = 24 * timeConstSecPerHour * static_cast<LONGLONG>(10000000);
  ui.QuadPart += days * intervals_per_day;

  // Convert back to FILETIME
  ft.dwLowDateTime = ui.LowPart;
  ft.dwHighDateTime = ui.HighPart;

  // Convert FILETIME back to SYSTEMTIME
  SYSTEMTIME new_st;
  FileTimeToSystemTime(&ft, &new_st);

  return convertSystemDate(new_st);
}
