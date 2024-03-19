// oPunch.h: interface for the oPunch class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "oBase.h"

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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

#include "meos_util.h"
class oEvent;

class oPunch : public oBase
{
protected:

  int type = 0;
  int punchTime = 0;
  int punchUnit = 0;
  int origin = 0;
  bool isUsed = false; //Is used in the course...

  // Index into course (-1 if unused)
  int tRogainingIndex;

  // Index into course (-1 if unused) for a rogaining control, even if it did not give any points
  int anyRogainingMatchControlId;
  // Number of rogaining points given
  int tRogainingPoints;

  //Adjustment of this punch, loaded from control. First is a fixed time adjustment (from control, "wrong time set")
  // second is dynamic adjustment (minimum time between controls etc)
  pair<int, int> tTimeAdjust;

  int tCardIndex = -1; // Index into card
  int tIndex; // Control match index in course
  int tMatchControlId;
  bool hasBeenPlayed;

  /** Get internal data buffers for DI */
  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const;
  int getDISize() const final { return -1; }

  void changedObject();
  mutable int previousPunchTime; /// Note that this is not valid in general

public:

  int getPunchUnit() const { return punchUnit; }
  void setPunchUnit(int unit);

  virtual int getControlId() const { return tMatchControlId; }

  bool isUsedInCourse() const { return isUsed; }
  void remove();
  bool canRemove() const;

  static int computeOrigin(int time, int code);
  bool isOriginal() const;
  int getOriginalTime() const;

  wstring getInfo() const;

  bool isHiredCard() const { return type == HiredCard; }
  bool isStart() const { return type == PunchStart; }
  bool isStart(int startType) const { return type == PunchStart || type == startType; }
  bool isFinish() const { return type == PunchFinish; }
  bool isFinish(int finishType) const { return type == PunchFinish || type == finishType; }
  bool isCheck() const { return type == PunchCheck; }
  int getControlNumber() const { return type >= 30 ? type : 0; }
  const wstring& getType() const;
  static const wstring& getType(int t);
  int getTypeCode() const { return type; }
  wstring getString() const;
  wstring getSimpleString() const;

  wstring getTime(bool adjusted, SubSecond mode) const;
  
  // Return time adjusted after punch unit adjustment (but not including course/control adjustments)
  int getTimeInt() const;
  
  /** Return true if time is set */
  bool hasTime() const { return punchTime > 0; }

  /** Return time after unit adjustment AND control/course adjustments. */
  int getAdjustedTime() const;
  void setTime(const wstring& t);
  virtual void setTimeInt(int newTime, bool databaseUpdate);

  void clearTimeAdjust() { tTimeAdjust = make_pair(0, 0); }
  void setTimeAdjust(int t) { tTimeAdjust.first = t; }
  void adjustTimeAdjust(int t) { tTimeAdjust.second += t; }

  wstring getRunningTime(int startTime) const;

  enum SpecialPunch { PunchStart = 1, PunchFinish = 2, PunchCheck = 3, HiredCard = 11111 };
  void decodeString(const char* s);
  string codeString() const;
  void appendCodeString(string& dst) const;

  void merge(const oBase& input, const oBase* base) override;

  oPunch(oEvent* poe);
  virtual ~oPunch();

  friend class oCard;
  friend class oRunner;
  friend class oTeam;
  friend class oEvent;
  friend class oListInfo;
};

typedef oPunch * pPunch;
