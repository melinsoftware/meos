/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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

oPunch::oPunch(oEvent *poe): oBase(poe)
{
  Type=0;
  Time=0;
  tTimeAdjust=0;
  isUsed=false;
  hasBeenPlayed=false;
  tMatchControlId = -1;
  tRogainingIndex = 0;
  anyRogainingMatchControlId = -1;
  tIndex = -1;
}

oPunch::~oPunch()
{
}

wstring oPunch::getInfo() const
{
  return L"Stämpling "+oe->gdiBase().widen(codeString());
}

string oPunch::codeString() const
{
  char bf[32];
  sprintf_s(bf, 32, "%d-%d;", Type, Time);
  return bf;
}

void oPunch::appendCodeString(string &dst) const {
  char bf[32];
  sprintf_s(bf, 32, "%d-%d;", Type, Time);
  dst.append(bf);
}

void oPunch::decodeString(const string &s)
{
  Type=atoi(s.c_str());
  Time=atoi(s.substr(s.find_first_of('-')+1).c_str());
}

wstring oPunch::getString() const {
  wchar_t bf[32];

  const wchar_t *ct;
  wstring time(getTime());
  ct=time.c_str();

  wstring typeS = getType();
  const wchar_t *tp = typeS.c_str();

  if (Type==oPunch::PunchStart)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else if (Type==oPunch::PunchFinish)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else if (Type==oPunch::PunchCheck)
    swprintf_s(bf, L"%s\t%s", tp, ct);
  else
  {
    if (isUsed)
      swprintf_s(bf, L"%d\t%s", Type, ct);
    else
      swprintf_s(bf, L"  %d*\t%s", Type, ct);
  }

  return bf;
}

wstring oPunch::getSimpleString() const
{
  wstring time(getTime());

  if (Type==oPunch::PunchStart)
    return lang.tl(L"starten (X)#" + time);
  else if (Type==oPunch::PunchFinish)
    return lang.tl(L"målet (X)#" + time);
  else if (Type==oPunch::PunchCheck)
    return lang.tl(L"check (X)#" + time);
  else
    return lang.tl(L"kontroll X (Y)#" + itow(Type) + L"#" + time);
}

wstring oPunch::getTime() const
{
  if (Time>=0)
    return oe->getAbsTime(Time+tTimeAdjust);
  else return makeDash(L"-");
}

int oPunch::getTimeInt() const {
  return Time;
}


int oPunch::getAdjustedTime() const
{
  if (Time>=0)
    return Time+tTimeAdjust;
  else return -1;
}
void oPunch::setTime(const wstring &t)
{
  if (convertAbsoluteTimeHMS(t, -1) <= 0) {
    setTimeInt(-1, false);
    return;
  }
 
  int tt = oe->getRelativeTime(t)-tTimeAdjust;
  if (tt < 0)
    tt = 0;
  setTimeInt(tt, false);
}

void oPunch::setTimeInt(int tt, bool databaseUpdate) {
  if (tt != Time) {
    Time = tt;
    if (!databaseUpdate)
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
  return getType(Type);
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
