// oCard.h: interface for the oCard class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OCARD_H__674EAB76_A232_4E44_A9B4_C52F6A04D7CF__INCLUDED_)
#define AFX_OCARD_H__674EAB76_A232_4E44_A9B4_C52F6A04D7CF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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

#include "oBase.h"
#include "oPunch.h"

#include "xmlparser.h"

typedef list<oPunch> oPunchList;

class gdioutput;
class oCard;
typedef oCard *pCard;

class oCourse;
class oRunner;
typedef oRunner *pRunner;

struct SICard;
class Table;

class oCard : public oBase {
protected:
  oPunchList punches;
  int cardNo;
  DWORD readId; //Identify a specific read-out

  const static DWORD ConstructedFromPunches = 1;

  pRunner tOwner;
  oPunch *getPunch(const pPunch punch);

  int getDISize() const {return -1;}

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

  mutable string punchString;

public:

  static const shared_ptr<Table> &getTable(oEvent *oe);

  // Returns true if the card was constructed from punches.
  bool isConstructedFromPunches() {return ConstructedFromPunches == readId;}

  // Setup a card from the runner's punches
  void setupFromRadioPunches(oRunner &r);

  void remove();
  bool canRemove() const;

  pair<int, int> getTimeRange() const;

  wstring getInfo() const;

  void addTableRow(Table &table) const;

  /// Returns the split time from the last used punch
  /// to the current punch, as indicated by evaluateCard
  int getSplitTime(int startTime, const pPunch punch) const;

  pRunner getOwner() const;
  int getNumPunches() const {return punches.size();}

  /** Returns the number of real control punches on the course. */
  int getNumControlPunches(int startPunchType, int finishPunchType) const;
  
  bool setPunchTime(const pPunch punch, const wstring &time);
  bool isCardRead(const SICard &card) const;
  void setReadId(const SICard &card);
  // Get SI-Card from oCard (just punches)
  void getSICard(SICard &card) const;

  void deletePunch(pPunch pp);
  void insertPunchAfter(int pos, int type, int time);

  bool fillPunches(gdioutput &gdi, const string &name, oCourse *crs);

  void addPunch(int type, int time, int matchControlId);
  oPunch *getPunchByType(int type) const;

  //Get punch by (matched) control punch id.
  oPunch *getPunchById(int courseControlId) const;
  oPunch *getPunchByIndex(int ix) const;

  // Get all punches
  void getPunches(vector<pPunch> &punches) const;
  // Return split time to previous matched control
  wstring getRogainingSplit(int ix, int startTime) const;

  /** Adapt the 24-hours based time to a start time that may br larger that 24-hour after zero time. */
  void adaptTimes(int startTime);

  int getCardNo() const {return cardNo;}
  const wstring &getCardNoString() const;
  void setCardNo(int c);
  void importPunches(const string &s);
  const string &getPunchString() const;

  void merge(const oBase &input, const oBase *base) final;
  pair<int, int> getCardHash() const;

  void Set(const xmlobject &xo);
  bool Write(xmlparser &xml);

  oCard(oEvent *poe);
  oCard(oEvent *poe, int id);

  virtual ~oCard();

  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
  friend class oListInfo;
};

#endif // !defined(AFX_OCARD_H__674EAB76_A232_4E44_A9B4_C52F6A04D7CF__INCLUDED_)
