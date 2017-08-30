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

#include <map>

#include "oBase.h"
#include "oClub.h"
#include "oClass.h"
#include "oCard.h"
#include "oSpeaker.h"
#include "oDataContainer.h"

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
  string sName;
  pClub Club;
  pClass Class;

  int startTime;
  int tStartTime;

  int FinishTime;
  RunnerStatus status;
  RunnerStatus tStatus;

  /** Temporary storage for set operations to be applied later*/
  struct TempSetStore {
    int startTime;
    RunnerStatus status;
    int startNo;
    string bib;
  };

  TempSetStore tmpStore;

  void resetTmpStore();

public:

  struct TempResult {
  private:
    int startTime;
    int runningTime;
    int timeAfter;
    int points;
    int place;
    int internalScore;
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

    const string &getStatusS(RunnerStatus inputStatus) const;
    const string &getPrintPlaceS(bool withDot) const;
    const string &getRunningTimeS(int inputTime) const;
    const string &getFinishTimeS(const oEvent *oe) const;
    const string &getStartTimeS(const oEvent *oe) const;

    const string &getOutputTime(int ix) const;
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

  // Sets result to object. Returns true if status/time changed
  bool setTmpStore();

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
public:
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
  };

  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);

  // Get the runners team or the team itself
  virtual cTeam getTeam() const = 0;
  virtual pTeam getTeam() = 0;

  virtual string getEntryDate(bool useTeamEntryDate = true) const = 0;

  // Set default fee, from class
  // a non-zero fee is changed only if resetFee is true
  void addClassDefaultFee(bool resetFees);

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
  void setInputTime(const string &time);
  string getInputTimeS() const;
  int getInputTime() const {return inputTime;}

  // Status
  void setInputStatus(RunnerStatus s);
  string getInputStatusS() const;
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

  string getInfo() const;

  virtual bool apply(bool sync, pRunner src, bool setTmpOnly) = 0;

  //Get time after on leg/for race
  virtual int getTimeAfter(int leg) const = 0;


  virtual void fillSpeakerObject(int leg, int controlCourseId, int previousControlCourseId, bool totalResult,
                                 oSpeakerObject &spk) const = 0;

  virtual int getBirthAge() const;

  virtual void setName(const string &n, bool manualChange);
  virtual const string &getName() const {return sName;}

  virtual void setFinishTimeS(const string &t);
  virtual	void setFinishTime(int t);

  /** Sets start time, if updatePermanent is true, the stored start time is updated,
  otherwise the value is considered deduced. */
  bool setStartTime(int t, bool updatePermanent, bool setTmpOnly, bool recalculate = true);
  void setStartTimeS(const string &t);

  const pClub getClubRef() const {return Club;}
  pClub getClubRef() {return Club;}

  const pClass getClassRef() const {return Class;}
  pClass getClassRef() {return Class;}

  virtual const string &getClub() const {if (Club) return Club->name; else return _EmptyString;}
  virtual int getClubId() const {if (Club) return Club->Id; else return 0;}
  virtual void setClub(const string &Name);
  virtual pClub setClubId(int clubId);

  virtual const string &getClass() const {if (Class) return Class->Name; else return _EmptyString;}
  virtual int getClassId() const {if (Class) return Class->Id; else return 0;}
  virtual void setClassId(int id, bool isManualUpdate);
  virtual int getStartNo() const {return StartNo;}
  virtual void setStartNo(int no, bool storeTmpOnly);

  // Start number is equal to bib-no, but bib
  // is only set when it should be shown in lists etc.
  const string &getBib() const;
  virtual void setBib(const string &bib, int numericalBib, bool updateStartNo, bool setTmpOnly) = 0;
  int getEncodedBib() const;

  virtual int getStartTime() const {return tStartTime;}
  virtual int getFinishTime() const {return FinishTime;}

  int getFinishTimeAdjusted() const {return getFinishTime() + getTimeAdjustment();}

  virtual int getRogainingPoints(bool multidayTotal) const = 0;
  virtual int getRogainingReduction() const = 0;
  virtual int getRogainingOvertime() const = 0;
  virtual int getRogainingPointsGross() const = 0;
  
  virtual const string &getStartTimeS() const;
  virtual const string &getStartTimeCompact() const;
  virtual const string &getFinishTimeS() const;

  virtual	const string &getTotalRunningTimeS() const;
  virtual	const string &getRunningTimeS() const;
  virtual int getRunningTime() const;

  /// Get total running time (including earlier stages / races)
  virtual int getTotalRunningTime() const;

  virtual int getPrelRunningTime() const;
  virtual string getPrelRunningTimeS() const;

  virtual string getPlaceS() const;
  virtual string getPrintPlaceS(bool withDot) const;

  virtual string getTotalPlaceS() const;
  virtual string getPrintTotalPlaceS(bool withDot) const;

  virtual int getPlace() const = 0;
  virtual int getTotalPlace() const = 0;

  virtual RunnerStatus getStatus() const {return tStatus;}
  inline bool statusOK() const {return tStatus==StatusOK;}
  inline bool prelStatusOK() const {return tStatus==StatusOK || (tStatus == StatusUnknown && getRunningTime() > 0);}

  /** Sets the status. If updatePermanent is true, the stored start
    time is updated, otherwise the value is considered deduced.
    if setTmp is true, the status is only stored in a temporary container.
    */
  virtual bool setStatus(RunnerStatus st, bool updatePermanent, bool setTmp, bool recalculate = true);
   
  /** Returns the ranking of the runner or the team (first runner in it?) */
  virtual int getRanking() const = 0;

  /// Get total status for this running (including team/earlier races)
  virtual RunnerStatus getTotalStatus() const;

  virtual const string &getStatusS() const;
  virtual string getIOFStatusS() const;

  virtual const string &getTotalStatusS() const;
  virtual string getIOFTotalStatusS() const;

  void setSpeakerPriority(int pri);
  virtual int getSpeakerPriority() const;

  int getTimeAdjustment() const;
  int getPointAdjustment() const;

  void setTimeAdjustment(int adjust);
  void setPointAdjustment(int adjust);

  oAbstractRunner(oEvent *poe, bool loading);
  virtual ~oAbstractRunner() {};

  friend class oListInfo;
  friend class GeneralResult;
};

struct RunnerDBEntry;

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

  int CardNo;
  pCard Card;

  vector<pRunner> multiRunner;
  vector<int> multiRunnerId;

  string tRealName;

  //Can be changed by apply
  mutable int tPlace;
  mutable int tCoursePlace;
  mutable int tTotalPlace;
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

  static const int dataSize = 192;
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

  void clearOnChangedRunningTime();

  // Cached runner statistics
  mutable vector<int> tMissedTime;
  mutable vector<int> tPlaceLeg;
  mutable vector<int> tAfterLeg;
  mutable vector<int> tPlaceLegAcc;
  mutable vector<int> tAfterLegAcc;

  // Rogainig results. Control and punch time
  vector< pair<pControl, int> > tRogaining;
  int tRogainingPoints;
  int tRogainingPointsGross;
  int tReduction;
  int tRogainingOvertime;
  string tProblemDescription;
  // Sets up mutable data above
  void setupRunnerStatistics() const;

  // Update hash
  void changeId(int newId);

  class RaceIdFormatter : public oDataDefiner {
    public:
      const string &formatData(oBase *obj) const;
      string setData(oBase *obj, const string &input) const;
      int addTableColumn(Table *table, const string &description, int minWidth) const;
  };

  static RaceIdFormatter raceIdFormatter;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  // Course adapted to loops
  mutable pCourse tAdaptedCourse;
  mutable int tAdaptedCourseRevision;

public:
  const string &getUIName() const;
  const string &getNameRaw() const {return sName;}
  virtual const string &getName() const;
  const string &getNameLastFirst() const;
  static void getRealName(const string &input, string &output);

  /** Returns true if this runner can use the specified card, 
   or false if it conflicts with the card of the other runner. */
  bool canShareCard(const pRunner other, int newCardNumber) const;

  void markClassChanged(int controlId);

  int getRanking() const;

  /** Returns true if the team / runner has a valid start time available*/
  virtual bool startTimeAvailable() const;

  /** Get a total input time from previous legs and stages*/
  int getTotalTimeInput() const;

  /** Get a total input time from previous legs and stages*/
  RunnerStatus getTotalStatusInput() const;

  // Returns public unqiue identifier of runner's race (for binding card numbers etc.)
  int getRaceIdentifier() const;

  // Get entry date of runner (or its team)
  string getEntryDate(bool useTeamEntryDate = true) const;

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

  void remove();
  bool canRemove() const;

  cTeam getTeam() const {return tInTeam;}
  pTeam getTeam() {return tInTeam;}

  /// Get total running time for multi/team runner at the given time
  int getTotalRunningTime(int time) const;

  // Get total running time at finish time (@override)
  int getTotalRunningTime() const;

  //Get total running time after leg
  int getRaceRunningTime(int leg) const;

  // Get the complete name, including team and club.
  string getCompleteIdentification(bool includeExtra = true) const;

  /// Get total status for this running (including team/earlier races)
  RunnerStatus getTotalStatus() const;

  // Return the runner in a multi-runner set matching the card, if course type is extra
  pRunner getMatchedRunner(const SICard &sic) const;

  int getRogainingPoints(bool multidayTotal) const;
  int getRogainingPointsGross() const;
  int getRogainingReduction() const;
  int getRogainingOvertime() const;

  const string &getProblemDescription() const {return tProblemDescription;}

  // Leg statistics access methods
  string getMissedTimeS() const;
  string getMissedTimeS(int ctrlNo) const;

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

  /** Automatically assign a bib. Returns true if bib is assigned. */
  bool autoAssignBib();

  /** Flag as temporary */
  void setTemporary() {isTemporaryObject=true;}

  /** Init from dbrunner */
  void init(const RunnerDBEntry &entry);

  /** Use db to pdate runner */
  bool updateFromDB(const string &name, int clubId, int classId,
                    int cardNo, int birthYear);

  void printSplits(gdioutput &gdi) const;

  void printStartInfo(gdioutput &gdi) const;

  /** Take the start time from runner r*/
  void cloneStartTime(const pRunner r);

  /** Clone data from other runner */
  void cloneData(const pRunner r);

  // Leg to run for this runner. Maps into oClass.MultiCourse.
  // Need to check index in bounds.
  int legToRun() const {return tInTeam ? tLeg : tDuplicateLeg;}
  void setName(const string &n, bool manualUpdate);
  void setClassId(int id, bool isManualUpdate);
  void setClub(const string &Name);
  pClub setClubId(int clubId);

  // Start number is equal to bib-no, but bib
  // is only set when it should be shown in lists etc.
  // Need not be so for teams. Course depends on start number,
  // which should be more stable.
  void setBib(const string &bib, int bibNumerical, bool updateStartNo, bool setTmpOnly);
  void setStartNo(int no, bool setTmpOnly);

  pRunner nextNeedReadout() const;

  // Synchronize this runner and parents/sibllings and team
  bool synchronizeAll();

  void setFinishTime(int t);
  int getTimeAfter(int leg) const;
  int getTimeAfter() const;
  int getTimeAfterCourse() const;

  bool skip() const {return isRemoved() || tDuplicateLeg!=0;}

  pRunner getMultiRunner(int race) const;
  int getNumMulti() const {return multiRunner.size();} //Returns number of  multi runners (zero=no multi)
  void createMultiRunner(bool createMaster, bool sync);
  int getRaceNo() const {return tDuplicateLeg;}
  string getNameAndRace(bool useUIName) const;

  void fillSpeakerObject(int leg, int courseControlId, int previousControlCourseId, bool totalResult,
                         oSpeakerObject &spk) const;

  bool needNoCard() const;
  int getPlace() const;
  int getCoursePlace() const;
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
  string getSplitTimeS(int controlNumber, bool normalized) const;
  // Normalized = true means permuted to the unlooped version of the course
  string getPunchTimeS(int controlNumber, bool normalized) const;
  string getNamedSplitS(int controlNumber) const;

  void addTableRow(Table &table) const;
  bool inputData(int id, const string &input,
                  int inputId, string &output, bool noUpdate);
  void fillInput(int id, vector< pair<string, size_t> > &elements, size_t &selected);

  bool apply(bool sync, pRunner src, bool setTmpOnly);
  void resetPersonalData();

  //Local user data. No Update.
  void setPriority(int courseControlId, int p){priority[courseControlId]=p;}

  string getGivenName() const;
  string getFamilyName() const;

  pCourse getCourse(bool getAdaptedCourse) const;
  const string &getCourseName() const;

  int getNumShortening() const;
  void setNumShortening(int numShorten);

  pCard getCard() const {return Card;}
  int getCardId(){if (Card) return Card->Id; else return 0;}

  bool operator<(const oRunner &c);
  bool static CompareSINumber(const oRunner &a, const oRunner &b){return a.CardNo<b.CardNo;}

  bool evaluateCard(bool applyTeam, vector<int> &missingPunches, int addpunch=0, bool synchronize=false);
  void addPunches(pCard card, vector<int> &missingPunches);

  /** Get split time for a controlId and optionally controlIndex on course (-1 means unknown, uses the first occurance on course)*/
  void getSplitTime(int courseControlId, RunnerStatus &stat, int &rt) const;

  //const string &getClubNameDB() const {return ClubName;}

  //Returns only Id of a runner-specific course, not classcourse
  int getCourseId() const {if (Course) return Course->Id; else return 0;}
  void setCourseId(int id);

  int getCardNo() const {return tParentRunner ? tParentRunner->CardNo : CardNo;}
  void setCardNo(int card, bool matchCard, bool updateFromDatabase = false);
  /** Sets the card to a given card. An existing card is marked as unpaired.
      CardNo is updated. Returns id of old card (or 0).
  */
  int setCard(int cardId);

  void Set(const xmlobject &xo);

  bool Write(xmlparser &xml);
  bool WriteSmall(xmlparser &xml);

  oRunner(oEvent *poe);
  oRunner(oEvent *poe, int id);

  void setSex(PersonSex sex);
  PersonSex getSex() const;

  void setBirthYear(int year);
  int getBirthYear() const;
  void setNationality(const string &nat);
  string getNationality() const;

  // Return true if the input name is considered equal to output name
  bool matchName(const string &pname) const;

  virtual ~oRunner();

  friend class MeosSQL;
  friend class oEvent;
  friend class oTeam;
  friend class oClass;
  friend bool CompareRunnerSPList(const oRunner &a, const oRunner &b);
  friend bool CompareRunnerResult(const oRunner &a, const oRunner &b);
  friend bool compareClubClassTeamName(const oRunner &a, const oRunner &b);
  friend class RunnerDB;
  friend class oListInfo;
  static bool sortSplit(const oRunner &a, const oRunner &b);

};

#endif // !defined(AFX_ORUNNER_H__D3B8D6C8_C90A_4F86_B776_7D77E5C76F42__INCLUDED_)
