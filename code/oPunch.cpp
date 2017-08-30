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
  tIndex = -1;
}

oPunch::~oPunch()
{
}

string oPunch::getInfo() const
{
  return "Stämpling "+codeString();
}

string oPunch::codeString() const
{
  char bf[32];
  sprintf_s(bf, 32, "%d-%d;", Type, Time);
  return bf;
}

void oPunch::decodeString(const string &s)
{
  Type=atoi(s.c_str());
  Time=atoi(s.substr(s.find_first_of('-')+1).c_str());
}

string oPunch::getString() const
{
  char bf[32];

  const char *ct;
  string time(getTime());
  ct=time.c_str();

  string typeS = getType();
  const char *tp = typeS.c_str();

  if (Type==oPunch::PunchStart)
    sprintf_s(bf, "%s\t%s", tp, ct);
  else if (Type==oPunch::PunchFinish)
    sprintf_s(bf, "%s\t%s", tp, ct);
  else if (Type==oPunch::PunchCheck)
    sprintf_s(bf, "%s\t%s", tp, ct);
  else
  {
    if (isUsed)
      sprintf_s(bf, "%d\t%s", Type, ct);
    else
      sprintf_s(bf, "  %d*\t%s", Type, ct);
  }

  return bf;
}

string oPunch::getSimpleString() const
{
  string time(getTime());

  if (Type==oPunch::PunchStart)
    return lang.tl("starten (X)#" + time);
  else if (Type==oPunch::PunchFinish)
    return lang.tl("målet (X)#" + time);
  else if (Type==oPunch::PunchCheck)
    return lang.tl("check (X)#" + time);
  else
    return lang.tl("kontroll X (Y)#" + itos(Type) + "#" + time);
}

string oPunch::getTime() const
{
  if (Time>=0)
    return oe->getAbsTime(Time+tTimeAdjust);
  else return "-";
}

int oPunch::getAdjustedTime() const
{
  if (Time>=0)
    return Time+tTimeAdjust;
  else return -1;
}
void oPunch::setTime(const string &t)
{
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

string oPunch::getRunningTime(int startTime) const
{
  int t = getAdjustedTime();
  if (startTime>0 && t>0 && t>startTime)
    return formatTime(t-startTime);
  else
    return "-";
}

void oPunch::remove()
{
  // Not implemented
}

bool oPunch::canRemove() const
{
  return true;
}

const string &oPunch::getType() const {
  return getType(Type);
}

const string &oPunch::getType(int t) {
  if (t==oPunch::PunchStart)
    return lang.tl("Start");
  else if (t==oPunch::PunchFinish)
    return lang.tl("Mål");
  else if (t==oPunch::PunchCheck)
    return lang.tl("Check");
  else if (t>10 && t<10000) {
    return itos(t);
  }
  return _EmptyString;
}

void oPunch::changedObject() {
  // Does nothing
}
