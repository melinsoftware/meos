/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

// oPunch.cpp: implementation of the oPunch class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "oPunch.h"
#include "oEvent.h"
#include "meos_util.h"
#include "localizer.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oPunch::oPunch(oEvent *poe): oBase(poe) {
  type=0;
  punchTime=0;
  tTimeAdjust=0;
  isUsed=false;
  hasBeenPlayed=false;
  tMatchControlId = -1;
  tRogainingIndex = 0;
  anyRogainingMatchControlId = -1;
  tIndex = -1;
}

oPunch::~oPunch() = default;

wstring oPunch::getInfo() const
{
  return L"Stämpling "+oe->gdiBase().widen(codeString());
}

string oPunch::codeString() const
{
  char bf[32];
  sprintf_s(bf, 32, "%d-%d;", type, punchTime);
  return bf;
}

void oPunch::appendCodeString(string &dst) const {
  char ubf[16];
  if (punchUnit > 0)
    sprintf_s(ubf, "@%d", punchUnit);
  else
    ubf[0] = 0;

  char bf[48];
  if (timeConstSecond > 1 && punchTime != -1) {
    if (punchTime >= 0)
      sprintf_s(bf, 32, "%d-%d.%d%s;", type, punchTime / timeConstSecond,
        punchTime % timeConstSecond, ubf);
    else {
      sprintf_s(bf, 32, "%d--%d.%d%s;", type, (-punchTime) / timeConstSecond, 
        (-punchTime) % timeConstSecond, ubf);
    }
  }
  else
    sprintf_s(bf, 32, "%d-%d%s;", type, punchTime, ubf);

  dst.append(bf);
}

void oPunch::decodeString(const char *s) {
  const char *typeS = s;
  while (*s >= '0' && *s <= '9') 
    ++s;
  
  type = atoi(typeS);

  if (*s == '-') {
    ++s;
    const char *timeS = s;
    while ((*s >= '0' && *s <= '9') || *s == '-')
      ++s;
  
    int t = atoi(timeS);
    if (timeConstSecond > 1 && *s == '.') {
      ++s;
      int tenth = *s - '0';
      while ((*s >= '0' && *s <= '9')) // Eat more decimal digits (unused)
        ++s;

      if (tenth > 0 && tenth < 10) {
        if (t >= 0 && *timeS != '-')
          punchTime = timeConstSecond * t + tenth;
        else
          punchTime = timeConstSecond * t - tenth;
      }
      else
        punchTime = timeConstSecond * t;
    }
    else if (t == -1)
      punchTime = t;
    else
      punchTime = timeConstSecond * t;
  }
  else
    punchTime = 0;

  if (*s == '@') {
    ++s;
    punchUnit = atoi(s);
  }
  else {
    punchUnit = 0;
  }
}

wstring oPunch::getString() const {
  wchar_t bf[32];

  const wchar_t *ct;
  wstring time(getTime(false, SubSecond::Auto));
  ct=time.c_str();

  wstring typeS = getType();
  const wchar_t *tp = typeS.c_str();

  if (type==oPunch::PunchStart)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else if (type==oPunch::PunchFinish)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else if (type==oPunch::PunchCheck)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else
  {
    if (isUsed)
      swprintf_s(bf, L"%d\t%s", type, ct);
    else
      swprintf_s(bf, L"  %d*\t%s", type, ct);
  }

  return bf;
}

wstring oPunch::getSimpleString() const
{
  wstring time(getTime(false, SubSecond::Auto));

  if (type==oPunch::PunchStart)
    return lang.tl(L"starten (X)#" + time);
  else if (type==oPunch::PunchFinish)
    return lang.tl(L"målet (X)#" + time);
  else if (type==oPunch::PunchCheck)
    return lang.tl(L"check (X)#" + time);
  else
    return lang.tl(L"kontroll X (Y)#" + itow(type) + L"#" + time);
}

wstring oPunch::getTime(bool adjust, SubSecond mode) const
{
  if (punchTime >= 0) {
    if (adjust)
      return oe->getAbsTime(getTimeInt() + tTimeAdjust, mode);
    else
      return oe->getAbsTime(getTimeInt(), mode);
  }
  else return makeDash(L"-");
}

int oPunch::getTimeInt() const {
  if (punchUnit > 0)
    return punchTime + oe->getUnitAdjustment(oPunch::SpecialPunch(type), punchUnit);
  return punchTime;
}

int oPunch::getAdjustedTime() const
{
  if (punchTime>=0)
    return getTimeInt() +tTimeAdjust;
  else return -1;
}

void oPunch::setTime(const wstring &t)
{
  if (convertAbsoluteTimeHMS(t, -1) <= 0) {
    setTimeInt(-1, false);
    return;
  }
 
  int tt = oe->getRelativeTime(t);
  if (tt < 0)
    tt = 0;
  else if (punchUnit > 0) {
    tt -= oe->getUnitAdjustment(oPunch::SpecialPunch(type), punchUnit);

  }
  setTimeInt(tt, false);
}

void oPunch::setTimeInt(int tt, bool databaseUpdate) {
  if (tt != punchTime) {
    punchTime = tt;
    if (!databaseUpdate)
      updateChanged();
  }
}

void oPunch::setPunchUnit(int unit) {
  if (unit != punchUnit) {
    punchUnit = unit;
    updateChanged();
  }
}

oDataContainer &oPunch::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  throw std::exception("Unsupported");
}

wstring oPunch::getRunningTime(int startTime) const
{
  int t = getAdjustedTime();
  if (startTime>0 && t>0 && t>startTime)
    return formatTime(t-startTime);
  else
    return makeDash(L"-");
}

void oPunch::remove()
{
  // Not implemented
}

bool oPunch::canRemove() const
{
  return true;
}

const wstring &oPunch::getType() const {
  return getType(type);
}

const wstring &oPunch::getType(int t) {
  if (t==oPunch::PunchStart)
    return lang.tl("Start");
  else if (t==oPunch::PunchFinish)
    return lang.tl("Mål");
  else if (t==oPunch::PunchCheck)
    return lang.tl("Check");
  else if (t>10 && t<10000) {
    return itow(t);
  }
  return _EmptyWString;
}

void oPunch::changedObject() {
  // Does nothing
}
