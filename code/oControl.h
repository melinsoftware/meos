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

// oControl.h: interface for the oControl class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OCONTROL_H__E86192B9_78D2_4EEF_AAE1_3BD4A8EB16F0__INCLUDED_)
#define AFX_OCONTROL_H__E86192B9_78D2_4EEF_AAE1_3BD4A8EB16F0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "xmlparser.h"
#include "oBase.h"
#include <limits>
#include <algorithm>

class oControl;

typedef oControl* pControl;
class oDataInterface;
class oDataConstInterface;
class Table;

class oCourse;
class oClass;
typedef oCourse* pCourse;
typedef oClass* pClass;

class oControl : public oBase
{
public:
  /** Returns the number of duplicates of this control in any course. Valid after a call to oEvent::getControls with
      the calculate flags set to true. */
  int getNumberDuplicates() const;

  void getCourseControls(vector<int> &cc) const;

  static pair<int, int> getIdIndexFromCourseControlId(int courseControlId);
  static int getCourseControlIdFromIdIndex(int controlId, int index);

  enum ControlStatus {StatusOK=0, StatusBad=1, StatusMultiple=2,
                      StatusStart = 4, StatusFinish = 5, StatusRogaining = 6, StatusNoTiming = 7, StatusOptional = 8};
  bool operator<(const oControl &b) const {return minNumber()<b.minNumber();}

protected:
  int nNumbers;
  int Numbers[32];
  bool checkedNumbers[32];

  ControlStatus Status;
  wstring Name;
  bool decodeNumbers(string s);

  static const int dataSize = 32;
  int getDISize() const {return dataSize;}
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  /// Table methods
  void addTableRow(Table &table) const;
  pair<int, bool> inputData(int id, const wstring &input,
                            int inputId, wstring &output, bool noUpdate) override;

  /// Table methods
  void fillInput(int id, vector< pair<wstring, size_t> > &elements, size_t &selected) override;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  struct TCache {
    TCache() : minTime(0), timeAdjust(0), dataRevision(-1) {}
    int minTime;
    int timeAdjust;
    int dataRevision;
  };

  // Is set to true if there is a free punch tied to the control
  bool tHasFreePunchLabel;
  mutable int tNumberDuplicates;
  mutable TCache tCache;
  void setupCache() const;

  mutable int tStatDataRevision;
  
  mutable int tMissedTimeTotal;
  mutable int tMissedTimeMax;
  mutable int tMistakeQuotient;
  mutable int tNumVisitorsActual; // Count actual punches
  mutable int tNumVisitorsExpected; // Count expected visitors
  mutable int tNumRunnersRemaining; // Number of remaining runners expected

  mutable int tMissedTimeMedian;

  void changedObject();

public:
  static const shared_ptr<Table> &getTable(oEvent *oe);

  static int getControlIdByName(const oEvent &oe, const string &name);

  // Returns true if controls is considered a radio control.
  bool isValidRadio() const;
  // Specify true to mark the controls as a radio control, otherwise no radio
  void setRadio(bool r);

  void remove();
  bool canRemove() const;

  wstring getInfo() const;

  bool isSingleStatusOK() const {return Status == StatusOK || Status == StatusNoTiming;}

  int getMissedTimeTotal() const;
  int getMissedTimeMax() const;
  int getMistakeQuotient() const;
  int getMissedTimeMedian() const;
  int getNumVisitors(bool actulaVisits) const;
  int getNumRunnersRemaining() const;

  void getCourses(vector<pCourse> &crs) const;
  void getClasses(vector<pClass> &cls) const;

  inline int minNumber() const {
    int m = numeric_limits<int>::max();
    for (int k=0;k<nNumbers;k++)
      m = min(Numbers[k], m);
    return m;
  }

  inline int maxNumber() const {
    int m = 0;
    for (int k=0;k<nNumbers;k++)
      m = max(Numbers[k], m);
    return m;
  }

  //Add unchecked controls to the list
  void addUncheckedPunches(vector<int> &mp, bool supportRogaining) const;
  //Start checking if all punches needed for this control exist
  void startCheckControl();
  //Get the number of a missing punch
  int getMissingNumber() const;
  /** Returns true if the check of this control is completed
   @param supportRogaining true if rogaining controls are supported
  */
  bool controlCompleted(bool supportRogaiing) const;

  wstring codeNumbers(char sep=';') const;
  bool setNumbers(const wstring &numbers);

  ControlStatus getStatus() const {return Status;}
  const wstring getStatusS() const;

  bool hasName() const {return !Name.empty();}
  /// Get name or [id]
  wstring getName() const;
  /// Get name or id
  wstring getIdS() const;

  bool isRogaining(bool useRogaining) const {return useRogaining && (Status == StatusRogaining);}

  void setStatus(ControlStatus st);
  void setName(wstring name);

  //Returns true if control has number and checks it.
  bool hasNumber(int i);
  //Return true if it has number i and it is unchecked.
  //Checks the number
  bool hasNumberUnchecked(int i);
  // Uncheck a given number
  bool uncheckNumber(int i);

  wstring getString();
  wstring getLongString();

  //For a control that requires several punches,
  //return the number of required punches.
  int getNumMulti();

  int getTimeAdjust() const;
  wstring getTimeAdjustS() const;
  void setTimeAdjust(int v);
  void setTimeAdjust(const wstring &t);

  int getMinTime() const;
  wstring getMinTimeS() const;
  void setMinTime(int v);
  void setMinTime(const wstring &t);

  int getRogainingPoints() const;
  wstring getRogainingPointsS() const;
  void setRogainingPoints(int v);
  void setRogainingPoints(const string &t);

  /// Return first code number (or zero)
  int getFirstNumber() const;
  void getNumbers(vector<int> &numbers) const;

  void merge(const oBase &input, const oBase *base) final;

  void set(const xmlobject *xo);
  void set(int pId, int pNumber, wstring pName);
  bool write(xmlparser &xml);
  oControl(oEvent *poe);
  oControl(oEvent *poe, int id);

  virtual ~oControl();

  friend class oRunner;
  friend class oCourse;
  friend class oEvent;
  friend class oClass;
  friend class MeosSQL;
  friend class TabAuto;
  friend class TabSpeaker;
};

#endif // !defined(AFX_OCONTROL_H__E86192B9_78D2_4EEF_AAE1_3BD4A8EB16F0__INCLUDED_)
