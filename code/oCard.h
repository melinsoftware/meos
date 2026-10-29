// oCard.h: interface for the oCard class.
//
//////////////////////////////////////////////////////////////////////
#pragma once

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

#include "oBase.h"
#include "oPunch.h"

typedef list<oPunch> oPunchList;

class gdioutput;
class oCard;
typedef oCard *pCard;
class xmlparser;
class xmlobject;

class oCourse;
class oRunner;
typedef oRunner *pRunner;

class oControl;
typedef oControl* pControl;

struct DeviceCard;
class Table;

class oCard : public oBase {
protected:
  oPunchList punches;
  int cardNo;
  int miliVolt = 0; // Measured voltage of SIAC, if not zero.
  int batteryDate = 0; // Battery replace date (for SIAC)
  unsigned int readId; //Identify a specific read-out

  const static int ConstructedFromPunches = 1;

  pRunner tOwner;
  oPunch *getPunch(const pPunch punch);

  int getDISize() const final {return -1;}

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

  mutable string punchString;

  /** Raw merge of punches (type/time) */
  vector<pPunch> merge(const oCard &secondaryCard) const;

  bool deletePunch(pPunch p);

  pPunch findPunch(int type, int time) const;

public:
  /** Get start time from a punch (or -1)*/
  int getStartTime(int ptype) const;

  /** Construct a common punch list from a secondary card*/
  void constructUnitedPL(const oCard &secondaryCard, oPunchList &pl) const;
  
  vector<pPunch> getPunchList(oCard *secondaryCard);

  vector<const oPunch *> getPunchList(const oCard *secondaryCard) const;


  int getStartPunchCode() const;
  int getFinishPunchCode() const;

  void setStartPunchCode(int code);
  void setFinishPunchCode(int code);

  void setMeasuredVoltage(int miliVolt) { this->miliVolt = miliVolt; }
  wstring getCardVoltage() const;
  static wstring getCardVoltage(int miliVolt);

  enum class BatteryStatus {
    OK,
    Warning,
    Bad
  };
  BatteryStatus isCriticalCardVoltage() const;
  static BatteryStatus isCriticalCardVoltage(int miliVolt);

  wstring getBatteryDate() const;

  static const shared_ptr<Table> &getTable(oEvent *oe);

  /** Returns true if the order of punches is unexpected (wrt time)*/
  bool unexpectedOrder(int startTIme) const;

  // Returns true if the card was constructed from punches.
  bool isConstructedFromPunches() {return ConstructedFromPunches == readId;}

  // Setup a card from the runner's punches
  void setupFromRadioPunches(oRunner &r);

  void remove();
  bool canRemove() const;

  pair<int, int> getTimeRange() const;

  wstring getInfo() const;

  void addTableRow(Table &table) const;

  pair<int, bool> inputData(int id, const wstring& input,
                            int inputId, wstring& output, bool noUpdate) override;

  /// Returns the split time from the last used punch
  /// to the current punch, as indicated by evaluateCard
  static int getSplitTime(const vector<oPunch *> &punches, int startTime, const oPunch *punch);

  pRunner getOwner() const;
  int getNumPunches() const {return punches.size();}

  /** Returns the number of real control punches on the course. */
  int getNumControlPunches(int startPunchType, int finishPunchType) const;
  
  bool setPunchTime(const pPunch punch, const wstring &time);
  bool isCardRead(const DeviceCard &card) const;
  void setReadId(const DeviceCard &card);
  // Get DeviceCard from oCard (just punches)
  void getDeviceCard(const pCard secondaryCard, DeviceCard &card) const;

  void deletePunch(pCard card2, int mergedPunchIndex);
  
  bool fillPunches(const oCard *card2, gdioutput &gdi, const string &name, const oCourse *crs) const;

  /** Get a (candidate) incorrect punch for the specified missing control on the course */
  pair<int, pControl> getWrongPunch(const oCourse &crs, const oControl &ctrl);

  static int getUnmatchedPunchId(int punchIx) {
    return -(100 + punchIx);
  }

  static int getUnmatchedPunchIndex(int punchId) {
    return -punchId - 100;
  }
  enum class PunchOrigin {
    Unknown,
    Original,
    Manual,
  };

  PunchOrigin isOriginalCard() const;

  void addPunch(int type, int time, int matchControlId, int unit, PunchOrigin origin);
  oPunch *getPunchByType(int type) const;

  void insert(const oPunch &position, const oPunch toInsert);

  //Get punch by (matched) control punch id.
  oPunch *getPunchById(int courseControlId) const;
  oPunch *getPunchByIndex(const oCard *card2, int ix) const;

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

  void set(const xmlobject &xo);
  bool write(xmlparser &xml, bool secondary);

  oCard(oEvent *poe);
  oCard(oEvent *poe, int id);

  virtual ~oCard();

  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
  friend class oListInfo;
};
