/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2018 Melin Software HB

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

// oBase.cpp: implementation of the oBase class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "meos.h"
#include "oBase.h"
#include "oCard.h"
#include "meos_util.h"

#include "oEvent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

char RunnerStatusOrderMap[100];

oBase::oBase(oEvent *poe)
{
  Removed = false;
  oe = poe;
  Id = 0;
  changed = false;
  reChanged = false;
  counter = 0;
  Modified.update();
  correctionNeeded = true;
  localObject = false;
}

oBase::~oBase()
{
}

bool oBase::synchronize(bool writeOnly)
{
  if (oe && changed) {
    changedObject();
    oe->dataRevision++;
  }
  if (oe && oe->HasDBConnection && (changed || !writeOnly)) {
    correctionNeeded = false;
    if (localObject)
      return false;
    return oe->msSynchronize(this);
  }
  else {
    if (changed) {
      if (!oe->HasPendingDBConnection) // True if we are trying to reconnect to mysql
        changed = false;
    }
  }
  return true;
}

void oBase::setExtIdentifier(__int64 id)
{
  getDI().setInt64("ExtId", id);
}

__int64 oBase::getExtIdentifier() const
{
  return getDCI().getInt64("ExtId");
}

wstring oBase::getExtIdentifierString() const {
  __int64 raw = getExtIdentifier();
  wchar_t res[16];
  if (raw == 0)
    return L"";
  if (raw & BaseGenStringFlag)
    convertDynamicBase(raw & ExtStringMask, 256-32, res);
  else if (raw & Base36StringFlag)
    convertDynamicBase(raw & ExtStringMask, 36, res);
  else
    convertDynamicBase(raw, 10, res);
  return res;
}

void oBase::converExtIdentifierString(__int64 raw, wchar_t bf[16])  {
  if (raw & BaseGenStringFlag)
    convertDynamicBase(raw & ExtStringMask, 256-32, bf);
  else if (raw & Base36StringFlag)
    convertDynamicBase(raw & ExtStringMask, 36, bf);
  else
    convertDynamicBase(raw, 10, bf);
}

__int64 oBase::converExtIdentifierString(const wstring &str) {
  __int64 val;
    int base = convertDynamicBase(str, val);
  if (base == 36)
    val |= Base36StringFlag;
  else if (base > 36)
    val |= BaseGenStringFlag;
  return val;
}

void oBase::setExtIdentifier(const wstring &str) {
  __int64 val = converExtIdentifierString(str);
  setExtIdentifier(val);
}

int oBase::idFromExtId(__int64 val) {
  int basePart = int(val & 0x0FFFFFFF);
  if (basePart == val)
    return basePart;

  __int64 hash = (val&ExtStringMask) % 2000000011ul;
  
  int res = basePart + int(hash&0xFFFFFF);
  if (res == 0)
    res += int(hash);

  return res & 0x0FFFFFFF;
}

bool oBase::isStringIdentifier() const {
  __int64 raw = getExtIdentifier();
  return (raw & (BaseGenStringFlag|Base36StringFlag)) != 0;
}

wstring oBase::getTimeStamp() const {
  if (oe && oe->isClient() && !sqlUpdated.empty()) {
    wstring sqlW(sqlUpdated.begin(), sqlUpdated.end());
    return sqlW;
  }
  else return Modified.getStampString();
}

void oBase::changeId(int newId) {
  Id = newId;
}

oDataInterface oBase::getDI(void) {
  pvoid data;
  pvoid olddata;
  pvectorstr strData;
  oDataContainer &dc = getDataBuffers(data, olddata, strData);
  return dc.getInterface(data, getDISize(), this);
}

oDataConstInterface oBase::getDCI(void) const
{
  pvoid data;
  pvoid olddata;
  pvectorstr strData;
  oDataContainer &dc = getDataBuffers(data, olddata, strData);
  return dc.getConstInterface(data, getDISize(), this);
}

void oBase::updateChanged() {
  Modified.update();
  changed=true;
}
