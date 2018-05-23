// oBase.h: interface for the oBase class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OBASE_H__C950D806_EEC7_4C56_B298_132C67FCF719__INCLUDED_)
#define AFX_OBASE_H__C950D806_EEC7_4C56_B298_132C67FCF719__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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

#include "TimeStamp.h"
#include "stdafx.h"
#include <vector>

class oEvent;
class gdioutput;
class oDataInterface;
class oDataConstInterface;
class oDataContainer;
typedef void * pvoid;
typedef vector< vector<wstring> > * pvectorstr;

enum RunnerStatus {StatusOK=1, StatusDNS=20, StatusCANCEL = 21, StatusMP=3,
                   StatusDNF=4, StatusDQ=5, StatusMAX=6,
                   StatusUnknown=0, StatusNotCompetiting=99};

extern char RunnerStatusOrderMap[100];

enum SortOrder {ClassStartTime,
                ClassTeamLeg,
                ClassResult,
                ClassCourseResult,
                ClassTotalResult,
                ClassTeamLegResult,
                ClassFinishTime,
                ClassStartTimeClub,
                ClassPoints,
                SortByName,
                SortByLastName,
                SortByFinishTime,
                SortByFinishTimeReverse,
                SortByStartTime,
                CourseResult,
                CourseStartTime,
                SortByEntryTime,
                Custom,
                SortEnumLastItem};

class oBase
{
private:
  void storeChangeStatus() {reChanged = changed;}
  void resetChangeStatus() {changed &= reChanged;}
  bool reChanged;
  bool changed;
  bool localObject;
  const static unsigned long long BaseGenStringFlag = 1ull << 63;
  const static unsigned long long Base36StringFlag = 1ull << 62;
  const static unsigned long long ExtStringMask = ~(BaseGenStringFlag|Base36StringFlag);

protected:
  int Id;
  TimeStamp Modified;
  string sqlUpdated; //SQL TIMESTAMP
  int counter;

  // True if the object is incorrect and needs correction
  // An example is if id changed as we wrote. Then owner
  // needs to be updated.
  bool correctionNeeded;

  oEvent *oe;
  bool Removed;

  //Used for selections, etc.
  int _objectmarker;

  /// Mark the object as changed (on client) and that it needs synchronize to server
  virtual void updateChanged();

  /// Mark the object as "changed" (locally or remotely), eg lists and other views may need update
  virtual void changedObject() = 0;

  /** Change the id of the object */
  virtual void changeId(int newId);

  /** Get internal data buffers for DI */
  virtual oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const = 0;
  virtual int getDISize() const = 0;

  void setLocalObject() { localObject = true; }

public:

  // Returns true if the object is local, not stored in DB/On disc
  bool isLocalObject() { return localObject; }

  /// Returns textual information on the object
  virtual wstring getInfo() const = 0;

  //Called (by a table) when user enters data in a cell
  virtual bool inputData(int id, const wstring &input, int inputId,
                         wstring &output, bool noUpdate) {output=L""; return false;}

  //Called (by a table) to fill a list box with contents in a table
  virtual void fillInput(int id, vector< pair<wstring, size_t> > &elements, size_t &selected)
    {throw std::exception("Not implemented");}

  oEvent *getEvent() const {return oe;}
  int getId() const {return Id;}
  bool isChanged() const {return changed;}
  bool isRemoved() const {return Removed;}
  int getAge() const {return Modified.getAge();}
  unsigned int getModificationTime() const {return Modified.getModificationTime();}
  
  bool synchronize(bool writeOnly=false);
  wstring getTimeStamp() const;

  bool existInDB() const { return !sqlUpdated.empty(); }

  oDataInterface getDI();

  oDataConstInterface getDCI() const;

  // Remove object from the competition
  virtual void remove() = 0;

  // Check if object can be remove (is not used by someone else)
  virtual bool canRemove() const = 0;

  /// Set an external identifier (0 if none)
  void setExtIdentifier(__int64 id);

  /// Get an external identifier (or 0) if none
  __int64 getExtIdentifier() const;

  wstring getExtIdentifierString() const;
  void setExtIdentifier(const wstring &str);
  bool isStringIdentifier() const;

  // Convert an external to a int id. The result
  // need not be unique, of course.
  static int idFromExtId(__int64 extId);
  static void converExtIdentifierString(__int64 raw, wchar_t bf[16]);
  static __int64 converExtIdentifierString(const wstring &str);

  oBase(oEvent *poe);
  virtual ~oBase();

  friend class RunnerDB;
  friend class MeosSQL;
  friend class oEvent;
  friend class oDataInterface;
  friend class oDataContainer;
  friend class MetaListContainer;
};

typedef oBase * pBase;

#endif // !defined(AFX_OBASE_H__C950D806_EEC7_4C56_B298_132C67FCF719__INCLUDED_)
