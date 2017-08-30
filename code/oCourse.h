// oCourse.h: interface for the oCourse class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OCOURSE_H__936E61C9_CDAC_490D_A475_E58190A2910C__INCLUDED_)
#define AFX_OCOURSE_H__936E61C9_CDAC_490D_A475_E58190A2910C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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

#include "oControl.h"
#include <map>
class oEvent;
class oCourse;
class oClass;
typedef oCourse * pCourse;
typedef oClass * pClass;
class oCard;

class gdioutput;
class oDataInterface;

struct SICard;

const int NControlsMax = 128;

class oCourse : public oBase
{
private:
  // Return 1000 on no match. Lower return value means better match
  static int matchLoopKey(const vector<int> &punches, const vector<pControl> &key);
protected:
  pControl Controls[NControlsMax];

  int nControls;
  string Name;
  int Length;
  static const int dataSize = 128;
  int getDISize() const {return dataSize;}

  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  // Length of each leg, Start-1, 1-2,... N-Finish.
  vector<int> legLengths;

  int tMapsRemaining;
  mutable int tMapsUsed;
  mutable int tMapsUsedNoVacant;

  // Get an identity sum based on controls
  int getIdSum(int nControls);

  /// Add an control without update
  pControl doAddControl(int Id);

  void changeId(int newId);

  // Caching.
  mutable vector<string> cachedControlOrdinal;
  mutable int cachedHasRogaining;
  mutable int cacheDataRevision;
  void clearCache() const;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  // For adapted courses;
  vector<int> tMapToOriginalOrder;

  void changedObject();

public:

  void getClasses(vector<pClass> &usageClass) const;
  
  void remove();
  bool canRemove() const;

  string getRadioName(int courseControlId) const;

  bool hasControl(const oControl *ctrl) const;

  /// Returns course specific id for specified control (taking duplicats of the control into account)
  int getCourseControlId(int controlIx) const;

  bool useFirstAsStart() const;
  bool useLastAsFinish() const;

  void firstAsStart(bool f);
  void lastAsFinish(bool f);

  int getFinishPunchType() const;
  int getStartPunchType() const;

  int getCommonControl() const;
  void setCommonControl(int ctrlId);

  bool operator<(const oCourse &b) const {return Name<b.Name;}

  void setNumberMaps(int nm);
  int getNumberMaps() const;

  int getNumUsedMaps(bool noVacant) const;

  //Get a loop course adapted to a card.
  pCourse getAdapetedCourse(const oCard &card, oCourse &tmpCourse) const;

  // Returns true if this course is adapted to specific punches
  bool isAdapted() const;

  // Returns the next shorter course, if any, null otherwise
  pCourse getShorterVersion() const;

  // Returns the next longer course, if any, null otherwise. Note that this method is slow.
  pCourse getLongerVersion() const;

  // Set a shorter version of the course.
  void setShorterVersion(pCourse shorter);

  // Returns a map for an adapted course to the original control order
  const vector<int> &getMapToOriginalOrder() const {return tMapToOriginalOrder;}

  // Returns a unique key for this variant
  int getAdaptionId() const;

  // Constuct loop keys of controls. CC is the common control
  bool constructLoopKeys(int commonControls, vector< vector<pControl> > &loopKeys, vector<int> &commonControlIndex) const;

  /// Check if course has problems
  string getCourseProblems() const;

  int getNumControls() const {return nControls;}
  void setLegLengths(const vector<int> &legLengths);

  // Get/set the minimal number of rogaining points to pass
  int getMinimumRogainingPoints() const;
  void setMinimumRogainingPoints(int p);

  // Get/set the maximal time allowed for rogaining
  int getMaximumRogainingTime() const;
  void setMaximumRogainingTime(int t);

  // Rogaining: point lost per minute over maximal time
  int getRogainingPointsPerMinute() const;
  void setRogainingPointsPerMinute(int t);

  // Calculate point reduction given a over time (in seconds)
  int calculateReduction(int overTime) const;

  /// Return true if the course has rogaining
  bool hasRogaining() const;

  // Get the control number as "printed on map". Do not count
  // rogaining controls
  const string &getControlOrdinal(int controlIndex) const;

  /** Get the part of the course between the start and end. Use start = 0 for the
      start of the course, and end = 0 for the finish. Returns 0 if fraction
      cannot be determined */
  double getPartOfCourse(int start, int end) const;

  string getInfo() const;

  oControl *getControl(int index) const;

  /** Return the distance between the course and the card.
      Positive return = extra controls
      Negative return = missing controls
      Zero return = exact match */
  int distance(const SICard &card);

  bool fillCourse(gdioutput &gdi, const string &name);

  /** Returns true if changed. */
  bool importControls(const string &cstring, bool updateLegLengths);
  void importLegLengths(const string &legs, bool setChanged);

  /** Returns the length of the i:th leg (or 0 if unknown)*/
  int getLegLength(int i) const;

  static void splitControls(const string &ctrls, vector<int> &nr);

  pControl addControl(int Id);
  void Set(const xmlobject *xo);

  void getControls(vector<pControl> &pc);
  string getControls() const;
  string getLegLengths() const;

  string getControlsUI() const;
  vector<string> getCourseReadable(int limit) const;

  const string &getName() const {return Name;}
  int getLength() const {return Length;}
  string getLengthS() const;

  void setName(const string &n);
  void setLength(int l);

  string getStart() const;
  void setStart(const string &start, bool sync);

  bool Write(xmlparser &xml);

  oCourse(oEvent *poe, int id);
  oCourse(oEvent *poe);
  virtual ~oCourse();

  friend class oEvent;
  friend class oClass;
  friend class oRunner;
  friend class MeosSQL;
};

#endif // !defined(AFX_OCOURSE_H__936E61C9_CDAC_490D_A475_E58190A2910C__INCLUDED_)
