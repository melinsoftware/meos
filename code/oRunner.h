// oRunner.h: interface for the oRunner class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ORUNNER_H__D3B8D6C8_C90A_4F86_B776_7D77E5C76F42__INCLUDED_)
#define AFX_ORUNNER_H__D3B8D6C8_C90A_4F86_B776_7D77E5C76F42__INCLUDED_

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

#include <map>

#include "oBase.h"
#include "oClub.h"
#include "oClass.h"
#include "oCard.h"
#include "oDataContainer.h"

enum RunnerStatus {
  StatusOK = 1, StatusDNS = 20, StatusCANCEL = 21, StatusOutOfCompetition = 15, StatusMP = 3,
  StatusDNF = 4, StatusDQ = 5, StatusMAX = 6, StatusNoTiming = 2,
  StatusUnknown = 0, StatusNotCompetiting = 99
};

/** Returns true for a status that might or might not indicate a result. */
template<int dummy=0>
bool isPossibleResultStatus(RunnerStatus st) {
  return st == StatusNoTiming || st == StatusOutOfCompetition;
}

template<int dummy=0>
vector<RunnerStatus> getAllRunnerStatus() {
  return { StatusOK, StatusDNS, StatusCANCEL, StatusOutOfCompetition, StatusMP,
           StatusDNF, StatusDQ, StatusMAX,
           StatusUnknown, StatusNotCompetiting , StatusNoTiming};
}

#include "oSpeaker.h"

extern char RunnerStatusOrderMap[100];

enum SortOrder {
  ClassStartTime,
  ClassTeamLeg,
  ClassResult,
  ClassDefaultResult,
  ClassCourseResult,
  ClassTotalResult,
  ClassTeamLegResult,
  ClassFinishTime,
  ClassStartTimeClub,
  ClassPoints,
  ClassLiveResult,
  ClassKnockoutTotalResult,
  SortByName,
  SortByLastName,
  SortByFinishTime,
  SortByFinishTimeReverse,
  SortByStartTime,
  SortByStartTimeClass,
  CourseResult,
  CourseStartTime,
  SortByEntryTime,
  Custom,
  SortEnumLastItem
};

class oRunner;
typedef oRunner* pRunner;
typedef const oRunner* cRunner;

class oTeam;
typedef oTeam* pTeam;
typedef const oTeam* cTeam;

struct SICard;

const int MaxRankingConstant = 99999999;

class oAbstractRunner : public oBase {
protected:
  wstring sName;
  pClub Club;
  pClass Class;

  int startTime;
  int tStartTime;

  int FinishTime;
  mutable int tComputedTime = 0;

  RunnerStatus status;
  RunnerStatus tStatus;
  mutable RunnerStatus tComputedStatus = RunnerStatus::StatusUnknown;
  mutable int tComputedPoints = -1;

  vector<vector<wstring>> dynamicData;
public:
  /** Encode status as a two-letter code, non-translated*/
  static const wstring &encodeStatus(RunnerStatus st, bool allowError = false);

  /** Decode the two-letter code of above. Returns unknown if not understood*/
  static RunnerStatus decodeStatus(const wstring &stat);

  /** Return true if the status is of type indicating a result. */
  static bool isResultStatus(RunnerStatus rs);

  /** Returns true if the result in the current state is up-to-date. */
  virtual bool isResultUpdated(bool totalResult) const = 0;

  struct TempResult {
  private:
    int startTime;
    int runningTime;
    int timeAfter;
    int points;
    int place;
    pair<int, int> internalScore;
    vector<int> outputTimes;
    vector<int> outputNumbers;
    RunnerStatus status;
    void reset();
    TempResult();
  public:
    int getRunningTime() const {return runningTime;}
    int getFinishTime() const {return runningTime > 0 ? startTime + runningTime : 0;}
    int getStartTime() const {return startTime;}
    int getTimeAfter() const {return timeAfter;}
    int getPoints() const {return points;}
    int getPlace() const {return place;}
    RunnerStatus getStatus() const {return status;}
    bool isStatusOK() const {
      return status == StatusOK || 
        ((status == StatusOutOfCompetition || status == StatusNoTiming) && runningTime > 0);
    }
    const wstring &getStatusS(RunnerStatus inputStatus) const;
    const wstring &getPrintPlaceS(bool withDot) const;
    const wstring &getRunningTimeS(int inputTime) const;
    const wstring &getFinishTimeS(const oEvent *oe) const;
    const wstring &getStartTimeS(const oEvent *oe) const;

    const wstring &getOutputTime(int ix) const;
    int getOutputNumber(int ix) const;

    friend class GeneralResult;
    friend class oAbstractRunner;
    friend class oEvent;
    friend class DynamicResult;
    TempResult(RunnerStatus statusIn, int startTime, 
               int runningTime, int points);
  };

protected:
  TempResult tmpResult;

  mutable int tTimeAdjustment;
  mutable int tPointAdjustment;
  mutable int tAdjustDataRevision;
  //Used for automatically assigning courses form class.
  //Set when drawn or by team or...
  int StartNo;

  // Data used for multi-days events
  int inputTime;
  RunnerStatus inputStatus;
  int inputPoints;
  int inputPlace;

  bool sqlChanged;
  bool tEntryTouched;

  void changedObject();

  mutable pair<bool, int> tPreventRestartCache = { false, -1 };
public:

  /** Return true if the runner/team should not be part av restart in a relay etc.*/
  bool preventRestart() const;
  void preventRestart(bool state);

  /** Call this method after doing something to just this
      runner/team that changed the time/status etc, that effects
      the result. May make a global evaluation of the class.
      Never call "for each" runner. */
  void hasManuallyUpdatedTimeStatus();

  /** Returs true if the class is a patrol class */
  bool isPatrolMember() const {
    return Class && Class->getClassType() == oClassPatrol;
  }

  /** Returns true if the team / runner has a start time available*/
  virtual bool startTimeAvailable() const;

  int getEntrySource() const;
  void setEntrySource(int src);
  void flagEntryTouched(bool flag);
  bool isEntryTouched() const;

  /** Returns number of shortenings taken. */
  virtual int getNumShortening() const = 0;

  int getPaymentMode() const;
  void setPaymentMode(int mode);

  enum TransferFlags {
    FlagTransferNew = 1,
    FlagUpdateCard = 2,
    FlagTransferSpecified = 4,
    FlagFeeSpecified = 8,
    FlagUpdateClass = 16,
    FlagUpdateName = 32,
    FlagAutoDNS = 64, // The competitor was set to DNS by the in-forest algorithm
    FlagAddedViaAPI = 128, // Added by the REST api entry.
    FlagOutsideCompetition = 256,
    FlagNoTiming = 512, // No timing requested
  };

  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);

  // Get the runners team or the team itself
  virtual cTeam getTeam() const = 0;
  virtual pTeam getTeam() = 0;

  virtual wstring getEntryDate(bool useTeamEntryDate = true) const = 0;

  // Set default fee, from class
  // a non-zero fee is changed only if resetFee is true
  void addClassDefaultFee(bool resetFees);

  /** Returns fee from the class. */
  int getDefaultFee() const;

  /** Returns the currently assigned fee. */
  int getEntryFee() const;

  /** Returns true if the entry fee is a late fee. */
  bool hasLateEntryFee() const;

  bool hasInputData() const {return inputTime > 0 || inputStatus != StatusOK || inputPoints > 0;}

  /** Reset input data to no input and the input status to NotCompeting. */
  void resetInputData();

  /** Return results for a specific result module. */
  const TempResult &getTempResult(int tempResultIndex) const;
  TempResult &getTempResult();
 
  void setTempResultZero(const TempResult &tr);

  // Time
  void setInputTime(const wstring &time);
  wstring getInputTimeS() const;
  int getInputTime() const {return inputTime;}

  // Status
  void setInputStatus(RunnerStatus s);
  wstring getInputStatusS() const;
  RunnerStatus getInputStatus() const {return inputStatus;}

  // Points
  void setInputPoints(int p);
  int getInputPoints() const {return inputPoints;}

  // Place
  void setInputPlace(int p);
  int getInputPlace() const {return inputPlace;}

  bool isVacant() const;
  
  bool wasSQLChanged() const {return sqlChanged;}

  /** Use -1 for all, PunchFinish or controlId */
  virtual void markClassChanged(int controlId) = 0;

  wstring getInfo() const;

  virtual void apply(ChangeType ct, pRunner src) = 0;

  //Get time after on leg/for race
  virtual int getTimeAfter(int leg) const = 0;


  virtual void fillSpeakerObject(int leg, int controlCourseId, int previousControlCourseId, bool totalResult,
                                 oSpeakerObject &spk) const = 0;

  virtual int getBirthAge() const;

  virtual void setName(const wstring &n, bool manualChange);
  virtual const wstring &getName() const {return sName;}

  void setFinishTimeS(const wstring &t);
  virtual	void setFinishTime(int t);

  /** Sets start time, if updatePermanent is true, the stored start time is updated,
  otherwise the value is considered deduced. */
  bool setStartTime(int t, bool updatePermanent, ChangeType changeType, bool recalculate = true);
  void setStartTimeS(const wstring &t);

  const pClub getClubRef() const {return Club;}
  pClub getClubRef() {return Club;}
  virtual int classInstance() const = 0;

  const pClass getClassRef(bool virtualClass) const {
    return (virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class;
  }
  
  pClass getClassRef(bool virtualClass) {
    return pClass((virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class);
  }

  virtual const wstring &getClub() const {if (Club) return Club->name; else return _EmptyWString;}
  virtual int getClubId() const {if (Club) return Club->Id; else return 0;}
  virtual void setClub(const wstring &clubName);
  virtual pClub setClubId(int clubId);

  const wstring &getClass(bool virtualClass) const;
  int getClassId(bool virtualClass) const {
    if (Class)
      return virtualClass ? Class->getVirtualClass(classInstance())->Id : Class->Id;
    return 0;
  }
      
  virtual void setClassId(int id, bool isManualUpdate);
  virtual int getStartNo() const {return StartNo;}
  virtual void setStartNo(int no, ChangeType changeType);

  // Do not assume start number is equal to bib-no, Bib
  // is only set when it should be shown in lists etc.
  const wstring &getBib() const;
  virtual void setBib(const wstring &bib, int numericalBib, bool updateStartNo) = 0;
  int getEncodedBib() const;

  virtual int getStartTime() const {return tStartTime;}
  virtual int getFinishTime() const {return FinishTime;}

  int getFinishTimeAdjusted() const {return getFinishTime() + getTimeAdjustment();}

  virtual int getRogainingPoints(bool computed, bool multidayTotal) const = 0;
  virtual int getRogainingReduction(bool computed) const = 0;
  virtual int getRogainingOvertime(bool computed) const = 0;
  virtual int getRogainingPointsGross(bool computed) const = 0;
  
  virtual const wstring &getStartTimeS() const;
  virtual const wstring &getStartTimeCompact() const;
  virtual const wstring &getFinishTimeS() const;

  const wstring &getTotalRunningTimeS() const;
 	const wstring &getRunningTimeS(bool computedTime) const;
  virtual int getRunningTime(bool computedTime) const;

  /// Get total running time (including earlier stages / races)
  virtual int getTotalRunningTime() const;

  virtual int getPrelRunningTime() const;
  
  wstring getPlaceS() const;
  wstring getPrintPlaceS(bool withDot) const;

  wstring getTotalPlaceS() const;
  wstring getPrintTotalPlaceS(bool withDot) const;

  virtual int getPlace() const = 0;
  virtual int getTotalPlace() const = 0;

  RunnerStatus getStatusComputed() const { return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus; }
  virtual RunnerStatus getStatus() const { return tStatus;}
  inline bool statusOK(bool computed) const {return (computed ? getStatusComputed() : tStatus) == StatusOK;}
  inline bool prelStatusOK(bool computed, bool includeOutsideCompetition) const {
    bool ok = statusOK(computed) || (tStatus == StatusUnknown && getRunningTime(false) > 0);
    if (!ok && includeOutsideCompetition) {
      RunnerStatus st = (computed ? getStatusComputed() : tStatus);
      ok = (st == StatusOutOfCompetition || st == StatusNoTiming) && getRunningTime(false) > 0;
    }
    return ok;
  }
  // Returns true if the competitor has a definite result
  bool hasResult() const {
    RunnerStatus st = this->getStatusComputed();
    if (st == StatusUnknown || st == StatusNotCompetiting)
      return false;
    if (isPossibleResultStatus(st))
      return getRunningTime(false) > 0;
    else
      return true;
  }

  /** Sets the status. If updatePermanent is true, the stored start
    time is updated, otherwise the value is considered deduced.
    */
  bool setStatus(RunnerStatus st, bool updatePermanent, ChangeType changeType, bool recalculate = true);
   
  /** Returns the ranking of the runner or the team (first runner in it?) */
  virtual int getRanking() const = 0;

  /// Get total status for this running (including team/earlier races)
  virtual RunnerStatus getTotalStatus() const;

  // Get results from all previous stages
  void getInputResults(vector<RunnerStatus> &st, vector<int> &times, vector<int> &points, vector<int> &places);
  // Add current result to input result. Only use when transferring to next stage. ThisStageNumber is zero indexed.
  void addToInputResult(int thisStageNo, const oAbstractRunner *src);

  const wstring &getStatusS(bool formatForPrint, bool computedStatus) const;
  wstring getIOFStatusS() const;

  const wstring &getTotalStatusS(bool formatForPrint) const;
  wstring getIOFTotalStatusS() const;

  void setSpeakerPriority(int pri);
  virtual int getSpeakerPriority() const;

  int getTimeAdjustment() const;
  int getPointAdjustment() const;

  void setTimeAdjustment(int adjust);
  void setPointAdjustment(int adjust);

  oAbstractRunner(oEvent *poe, bool loading);
  virtual ~oAbstractRunner() {};

  struct DynamicValue {
    int dataRevision;
    int value;
    bool isOld(const oEvent &oe) const;
    void update(const oEvent &oe, int v);
  };

  friend class oListInfo;
  friend class GeneralResult;
};

struct RunnerWDBEntry;

struct SplitData {
  enum SplitStatus {OK, Missing, NoTime};
  int time;
  SplitStatus status;
  SplitData() {};
  SplitData(int t, SplitStatus s) : time(t), status(s) {};

  void setPunchTime(int t) {
    time = t;
    status = OK;
  }

  void setPunched() {
    time = -1;
    status = NoTime;
  }

  void setNotPunched() {
    time = -1;
    status = Missing;
  }

  bool hasTime() const {
    return time > 0 && status == OK;
  }

  bool isMissing() const {
    return status == Missing;
  }
};

class oRunner : public oAbstractRunner
{
protected:
  pCourse Course;

  int cardNumber;
  pCard Card;

  vector<pRunner> multiRunner;
  vector<int> multiRunnerId;

  wstring tRealName;

  //Can be changed by apply
  mutable DynamicValue tPlace;
  mutable DynamicValue tCoursePlace;
  mutable DynamicValue tCourseClassPlace;
  mutable DynamicValue tTotalPlace;
  mutable int tLeg;
  mutable int tLegEquClass;
  mutable pTeam tInTeam;
  mutable pRunner tParentRunner;
  mutable bool tNeedNoCard;
  mutable bool tUseStartPunch;
  mutable int tDuplicateLeg;
  mutable int tNumShortening;
  mutable int tShortenDataRevision;

  //Temporary status and running time
  RunnerStatus tempStatus;
  int tempRT;

  bool isTemporaryObject;
  int tTimeAfter; // Used in time line calculations, time after "last radio".
  int tInitialTimeAfter; // Used in time line calculations, time after when started.
  //Speaker data
  map<int, int> priority;
  int cPriority;

  static const int dataSize = 256;
  int getDISize() const {return dataSize;}

  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  bool storeTimes(); // Returns true if best times were updated
  
  bool storeTimesAux(pClass targetClass); // Returns true if best times were updated
  
  // Adjust times for fixed time controls
  void doAdjustTimes(pCourse course);

  vector<int> adjustTimes;
  vector<SplitData> splitTimes;
  mutable vector<SplitData> normalizedSplitTimes; //Loop courses

  vector<int> tLegTimes;

  string codeMultiR() const;
  void decodeMultiR(const string &r);

  pRunner getPredecessor() const;

  void markForCorrection() {correctionNeeded = true;}
  //Remove by force the runner from a multirunner definition
  void correctRemove(pRunner r);

  vector<pRunner> getRunnersOrdered() const;
  int getMultiIndex(); //Returns the index into multi runners, 0 - n, -1 on error.

  void exportIOFRunner(xmlparser &xml, bool compact);
  void exportIOFStart(xmlparser &xml);

  // Revision number of runners statistics cache
  mutable int tSplitRevision;
  // Running time as calculated by evalute. Used to detect changes.
  int tCachedRunningTime;

  mutable pair<int, int> classInstanceRev;

  void clearOnChangedRunningTime();

  // Cached runner statistics
  mutable vector<int> tMissedTime;
  mutable vector<int> tPlaceLeg;
  mutable vector<int> tAfterLeg;
  mutable vector<int> tPlaceLegAcc;
  mutable vector<int> tAfterLegAcc;
    
  // Used to calculate temporary split time results
  struct OnCourseResult {
    OnCourseResult(int courseControlId,
                   int controlIx,
                   int time) : courseControlId(courseControlId), 
                               controlIx(controlIx), time(time) {}
    int courseControlId;
    int controlIx;
    int time;
    int place;
    int after;
  };
  mutable pair<int, int> currentControlTime;

  struct OnCourseResultCollection {
    bool hasAnyRes = false;
    vector<OnCourseResult> res;
    void clear() { hasAnyRes = false; res.clear(); }
    void emplace_back(int courseControlId,
                      int controlIx,
                      int time) {
      res.emplace_back(courseControlId, controlIx, time);
      hasAnyRes = true;
    }
    bool empty() const { return hasAnyRes == false; }
  };

  mutable OnCourseResultCollection tOnCourseResults;

  // Rogainig results. Control and punch time
  vector< pair<pControl, int> > tRogaining;
  int tRogainingPoints;
  int tRogainingPointsGross;
  int tReduction;
  int tRogainingOvertime;
  wstring tProblemDescription;
  // Sets up mutable data above
  void setupRunnerStatistics() const;

  // Update hash
  void changeId(int newId);

  class RaceIdFormatter : public oDataDefiner {
    public:
      const wstring &formatData(const oBase *obj) const override;
      pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override;
      int addTableColumn(Table *table, const string &description, int minWidth) const override;
  };

  class RunnerReference : public oDataDefiner {
  public:
    const wstring &formatData(const oBase *obj) const override;
    pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override;
    void fillInput(const oBase *obj, vector<pair<wstring, size_t>> &out, size_t &selected) const override;
    int addTableColumn(Table *table, const string &description, int minWidth) const override;
    CellType getCellType() const override;
  };
 
  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  // Course adapted to loops
  mutable pCourse tAdaptedCourse;
  mutable int tAdaptedCourseRevision;

  /** Internal propagate club.*/
  void propagateClub();

  bool isHiredCard(int card) const;

public:
  static const shared_ptr<Table> &getTable(oEvent *oe);

  // Get the leg defineing parallel results for this runner (in a team)
  int getParResultLeg() const;

  // Returns true if there are radio control results, provided result calculation oEvent::ResultType::PreliminarySplitResults was invoked.
  bool hasOnCourseResult() const { return !tOnCourseResults.empty() || getFinishTime() > 0 || hasResult(); }
  
  /** Return true if the race is completed (or definitely never will be started), e.g., not in forest*/
  bool hasFinished() const {
    if (tStatus == StatusUnknown)
      return false;
    else if (isPossibleResultStatus(tStatus)) {
      return Card || FinishTime > 0;
    }
    else
      return true;
  }

  /** Returns a check time (or zero for no time). */
  int getCheckTime() const;

  /** Get a runner reference (drawing) */
  pRunner getReference() const;
  
  int classInstance() const override;

  /**Set a runner reference*/
  void setReference(int runnerId);

  const wstring &getUIName() const;
  const wstring &getNameRaw() const {return sName;}
  virtual const wstring &getName() const;
  const wstring &getNameLastFirst() const;
  void getRealName(const wstring &input, wstring &output) const;

  /** Returns true if this runner can use the specified card, 
   or false if it conflicts with the card of the other runner. */
  bool canShareCard(const pRunner other, int newCardNumber) const;

  void markClassChanged(int controlId);

  int getRanking() const;

  bool isResultUpdated(bool totalResult) const override;

  /** Returns true if the team / runner has a valid start time available*/
  bool startTimeAvailable() const override;

  /** Get a total input time from previous legs and stages*/
  int getTotalTimeInput() const;

  /** Get a total input time from previous legs and stages*/
  RunnerStatus getTotalStatusInput() const;

  // Returns public unqiue identifier of runner's race (for binding card numbers etc.)
  int getRaceIdentifier() const;

  bool isAnnonumousTeamMember() const;

  // Get entry date of runner (or its team)
  wstring getEntryDate(bool useTeamEntryDate = true) const;

  // Get date of birth
  int getBirthAge() const;

  // Multi day data input
  void setInputData(const oRunner &source);

  // Returns true if the card number is suppossed to be transferred to the next stage
  bool isTransferCardNoNextStage() const;

  // Set wheather the card number should be transferred to the next stage
  void setTransferCardNoNextStage(bool state);

  int getLegNumber() const {return tLeg;}
  int getSpeakerPriority() const;

  RunnerStatus getTempStatus() const { return tempStatus; }
  int getTempTime() const { return tempRT; }

  void remove();
  bool canRemove() const;

  cTeam getTeam() const {return tInTeam;}
  pTeam getTeam() {return tInTeam;}

  /// Get total running time for multi/team runner at the given time
  int getTotalRunningTime(int time, bool computedTime, bool includeInput) const;

  // Get total running time at finish time 
  int getTotalRunningTime() const override;

  //Get total running time after leg
  int getRaceRunningTime(int leg) const;

  // Get the complete name, including team and club.
  wstring getCompleteIdentification(bool includeExtra = true) const;

  /// Get total status for this running (including team/earlier races)
  RunnerStatus getTotalStatus() const override;

  // Return the runner in a multi-runner set matching the card, if course type is extra
  pRunner getMatchedRunner(const SICard &sic) const;

  int getRogainingPoints(bool computed, bool multidayTotal) const override;
  int getRogainingPointsGross(bool computed) const override;
  int getRogainingReduction(bool computed) const override;
  int getRogainingOvertime(bool computed) const override;

  const wstring &getProblemDescription() const {return tProblemDescription;}
  const pair<wstring, int> getRaceInfo();
  // Leg statistics access methods
  wstring getMissedTimeS() const;
  wstring getMissedTimeS(int ctrlNo) const;

  int getMissedTime(int ctrlNo) const;
  int getLegPlace(int ctrlNo) const;
  int getLegTimeAfter(int ctrlNo) const;
  int getLegPlaceAcc(int ctrlNo) const;
  int getLegTimeAfterAcc(int ctrlNo) const;

  /** Calculate the time when the runners place is fixed, i.e,
      when no other runner can threaten the place.
      Returns -1 if undeterminable.
      Return 0 if place is fixed. */
  int getTimeWhenPlaceFixed() const;

  enum class BibAssignResult {
    Assigned,
    NoBib,
    Failed,
  };
  /** Automatically assign a bib. Returns true if bib is assigned. */
  BibAssignResult autoAssignBib();

  /** Flag as temporary */
  void setTemporary() {isTemporaryObject=true;}

  /** Init from dbrunner */
  void init(const RunnerWDBEntry &entry, bool updateOnlyExt);

  /** Use db to pdate runner */
  bool updateFromDB(const wstring &name, int clubId, int classId,
                    int cardNo, int birthYear, bool forceUpdate);

  void printSplits(gdioutput &gdi) const;

  void printStartInfo(gdioutput &gdi) const;

  /** Take the start time from runner r*/
  void cloneStartTime(const pRunner r);

  /** Clone data from other runner */
  void cloneData(const pRunner r);

  // Leg to run for this runner. Maps into oClass.MultiCourse.
  // Need to check index in bounds.
  int legToRun() const {return tInTeam ? tLeg : tDuplicateLeg;}
  void setName(const wstring &n, bool manualUpdate);
  void setClassId(int id, bool isManualUpdate);
  void setClub(const wstring &name) override;
  pClub setClubId(int clubId) override;

  // Start number is equal to bib-no, but bib
  // is only set when it should be shown in lists etc.
  // Need not be so for teams. Course depends on start number,
  // which should be more stable.
  void setBib(const wstring &bib, int bibNumerical, bool updateStartNo) override;
  void setStartNo(int no, ChangeType changeType) override;
  // Update and synch start number for runner and team.
  void updateStartNo(int no);

  pRunner nextNeedReadout() const;

  // Synchronize this runner and parents/sibllings and team
  bool synchronizeAll(bool writeOnly = false);

  void setFinishTime(int t) override;
  int getTimeAfter(int leg) const;
  int getTimeAfter() const;
  int getTimeAfterCourse() const;

  bool skip() const {return isRemoved() || tDuplicateLeg!=0;}

  pRunner getMultiRunner(int race) const;
  int getNumMulti() const {return multiRunner.size();} //Returns number of  multi runners (zero=no multi)
  void createMultiRunner(bool createMaster, bool sync);
  int getRaceNo() const {return tDuplicateLeg;}
  wstring getNameAndRace(bool useUIName) const;

  void fillSpeakerObject(int leg, int courseControlId, int previousControlCourseId, bool totalResult,
                         oSpeakerObject &spk) const;

  bool needNoCard() const;
  int getPlace() const;
  int getCoursePlace(bool perClass) const;
  int getTotalPlace() const;

  // Normalized = true means permuted to the unlooped version of the course
  const vector<SplitData> &getSplitTimes(bool normalized) const;

  void getSplitAnalysis(vector<int> &deltaTimes) const;
  void getLegPlaces(vector<int> &places) const;
  void getLegTimeAfter(vector<int> &deltaTimes) const;

  void getLegPlacesAcc(vector<int> &places) const;
  void getLegTimeAfterAcc(vector<int> &deltaTimes) const;

  // Normalized = true means permuted to the unlooped version of the course
  int getSplitTime(int controlNumber, bool normalized) const;
  int getTimeAdjust(int controlNumber) const;

  int getNamedSplit(int controlNumber) const;
  // Normalized = true means permuted to the unlooped version of the course
  int getPunchTime(int controlNumber, bool normalized) const;
  // Normalized = true means permuted to the unlooped version of the course
  wstring getSplitTimeS(int controlNumber, bool normalized) const;
  // Normalized = true means permuted to the unlooped version of the course
  wstring getPunchTimeS(int controlNumber, bool normalized) const;
  wstring getNamedSplitS(int controlNumber) const;

  void addTableRow(Table &table) const;
  pair<int, bool> inputData(int id, const wstring &input,
                            int inputId, wstring &output, bool noUpdate) override;
  void fillInput(int id, vector< pair<wstring, size_t> > &elements, size_t &selected) override;

  void apply(ChangeType changeType, pRunner src) override;
  void resetPersonalData();

  //Local user data. No Update.
  void setPriority(int courseControlId, int p){priority[courseControlId]=p;}

  wstring getGivenName() const;
  wstring getFamilyName() const;

  pCourse getCourse(bool getAdaptedCourse) const;
  const wstring &getCourseName() const;

  int getNumShortening() const;
  void setNumShortening(int numShorten);

  pCard getCard() const {return Card;}
  int getCardId(){if (Card) return Card->Id; else return 0;}

  bool operator<(const oRunner &c) const;
  bool static CompareCardNumber(const oRunner &a, const oRunner &b) { return a.cardNumber < b.cardNumber; }

  bool evaluateCard(bool applyTeam, vector<int> &missingPunches, int addPunch, ChangeType changeType);
  void addPunches(pCard card, vector<int> &missingPunches);

  /** Get split time for a controlId and optionally controlIndex on course (-1 means unknown, uses the first occurance on course)*/
  void getSplitTime(int courseControlId, RunnerStatus &stat, int &rt) const;

  //Returns only Id of a runner-specific course, not classcourse
  int getCourseId() const {if (Course) return Course->Id; else return 0;}
  void setCourseId(int id);

  bool isHiredCard() const;

  int getCardNo() const { return tParentRunner && cardNumber == 0 ? tParentRunner->cardNumber : cardNumber; }
  void setCardNo(int card, bool matchCard, bool updateFromDatabase = false);
  /** Sets the card to a given card. An existing card is marked as unpaired.
      CardNo is updated. Returns id of old card (or 0).
  */
  int setCard(int cardId);
  void Set(const xmlobject &xo);
  bool Write(xmlparser &xml);
  
  oRunner(oEvent *poe);
  oRunner(oEvent *poe, int id);

  void setSex(PersonSex sex);
  PersonSex getSex() const;

  void setBirthYear(int year);
  int getBirthYear() const;
  void setNationality(const wstring &nat);
  wstring getNationality() const;

  // Return true if the input name is considered equal to output name
  bool matchName(const wstring &pname) const;

  /** Formats extra line for runner []-syntax, or if r is null, checks validity and throws on error.*/
  static wstring formatExtraLine(pRunner r, const wstring &input);

  virtual ~oRunner();

  friend class MeosSQL;
  friend class oEvent;
  friend class oTeam;
  friend class oClass;
  friend bool compareClubClassTeamName(const oRunner &a, const oRunner &b);
  friend class RunnerDB;
  friend class oListInfo;
  static bool sortSplit(const oRunner &a, const oRunner &b);

};

#endif // !defined(AFX_ORUNNER_H__D3B8D6C8_C90A_4F86_B776_7D77E5C76F42__INCLUDED_)
