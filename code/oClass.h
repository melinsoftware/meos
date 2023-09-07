#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include "oCourse.h"
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "inthashmap.h"
class oClass;
typedef oClass* pClass;
class oDataInterface;
class GeneralResult;
class oRunner;

const int MaxClassId = 1000000;

enum PersonSex;

enum StartTypes {
  STTime=0,
  STChange,
  STDrawn,
  STPursuit,
  ST_max
};
enum { nStartTypes = ST_max };

enum LegTypes {
  LTNormal=0,
  LTParallel,
  LTExtra,
  LTSum,
  LTIgnore,
  LTParallelOptional,
  LTGroup,
  LT_max,
};
enum { nLegTypes = LT_max };


enum BibMode {
  BibUndefined = -1,
  BibSame = 0,
  BibAdd = 1,
  BibFree = 2,
  BibLeg = 3,
};

enum AutoBibType {
  AutoBibManual = 0,
  AutoBibConsecutive = 1,
  AutoBibNone = 2,
  AutoBibExplicit = 3
};

enum ClassSplitMethod {
  SplitRandom,
  SplitClub,
  SplitRank,
  SplitRankEven,
  SplitResult,
  SplitTime,
  SplitResultEven,
  SplitTimeEven,
};

enum ClassSeedMethod {
  SeedRank,
  SeedResult,
  SeedTime,
  SeedPoints
};

#ifdef DODECLARETYPESYMBOLS
  const char *StartTypeNames[4]={"ST", "CH", "DR", "HU"};
  const char *LegTypeNames[7]={"NO", "PA", "EX", "SM", "IG", "PO", "GP"};
#endif

struct oLegInfo {
  StartTypes startMethod;
  LegTypes legMethod;
  bool isParallel() const {return legMethod == LTParallel || legMethod == LTParallelOptional;}
  bool isOptional() const {return legMethod == LTParallelOptional || legMethod == LTExtra || legMethod == LTIgnore;}
  //Interpreteation depends. Can be starttime/first start (if styp==STTime || styp==STPursuit)
  // or number of earlier legs to consider.
  int legStartData;
  int legRestartTime;
  int legRopeTime;
  int duplicateRunner;

  /** Return true if start data should be interpreted as a time.*/
  bool isStartDataTime() const { return startMethod == STTime || startMethod == STPursuit; }

  // Transient, deducable data
  int trueSubLeg;
  int trueLeg;
  string displayLeg;

  oLegInfo():startMethod(STTime), legMethod(LTNormal), legStartData(0),
             legRestartTime(0), legRopeTime(0), duplicateRunner(-1) {}
  string codeLegMethod() const;
  void importLegMethod(const string &courses);
};

struct ClassResultInfo {
  ClassResultInfo() : nUnknown(0), nFinished(0), lastStartTime(0) {}

  int nUnknown;
  int nFinished;
  int lastStartTime;
};

class QualificationFinal;

enum ClassType {
  oClassIndividual = 1, 
  oClassPatrol = 2,
  oClassRelay = 3, 
  oClassIndividRelay = 4, 
  oClassKnockout = 5
};

enum ClassMetaType {ctElite, ctNormal, ctYouth, ctTraining,
                    ctExercise, ctOpen, ctUnknown};

class Table;
class oClass : public oBase
{
public:
  enum class ClassStatus {Normal, Invalid, InvalidRefund};
  enum class AllowRecompute {Yes, No, NoUseOld };

  static void getSplitMethods(vector< pair<wstring, size_t> > &methods);
  static void getSeedingMethods(vector< pair<wstring, size_t> > &methods);

protected:
  wstring Name;
  pCourse Course;

  vector< vector<pCourse> > MultiCourse;
  vector< oLegInfo > legInfo;

  //First: best time on leg
  //Second: Total leader time (total leader)
  struct LeaderInfo {
  private:
    int bestTimeOnLeg;
    int bestTimeOnLegComputed; // Computed be default result module
    
    int totalLeaderTime;
    int totalLeaderTimeComputed; // Computed be default result module

    int totalLeaderTimeInput; //Team total including input
    int totalLeaderTimeInputComputed; //Team total including input

    int inputTime;

    bool complete = false;
  public:
    LeaderInfo() {
      reset();
    }

    void reset() {
      bestTimeOnLeg = -1;
      bestTimeOnLegComputed = -1;

      totalLeaderTime = -1;
      totalLeaderTimeComputed = -1;

      inputTime = -1;
      totalLeaderTimeInput = -1;
      totalLeaderTimeInputComputed = -1;
      complete = false;
    }

    void updateFrom(const LeaderInfo& i) {
      if (i.complete) {
        if (i.bestTimeOnLeg != -1)
          bestTimeOnLeg = i.bestTimeOnLeg;
        if (i.bestTimeOnLegComputed != -1)
          bestTimeOnLegComputed = i.bestTimeOnLegComputed;
        if (i.totalLeaderTime != -1)
          totalLeaderTime = i.totalLeaderTime;
        if (i.totalLeaderTimeComputed != -1)
          totalLeaderTimeComputed = i.totalLeaderTimeComputed;
        if (i.inputTime != -1)
          inputTime = i.inputTime;
        if (i.totalLeaderTimeInput != -1)
          totalLeaderTimeInput = i.totalLeaderTimeInput;
        if (i.totalLeaderTimeInputComputed != -1)
          totalLeaderTimeInputComputed = i.totalLeaderTimeInputComputed;
      }
    }
    
    enum class Type {
      Leg,
      Total,
      TotalInput,
      Input,
    };

    int getInputTime() const {
      return inputTime;
    }
    
    void resetComputed(Type t);    
    bool update(int rt, Type t);
    bool updateComputed(int rt, Type t);
    int getLeader(Type t, bool computed) const;

    void setComplete() {
      complete = true;
    }

    // For non-team classes, input is the same as total input and computed total input
    void copyInputToTotalInput();
  };

  void updateLeaderTimes() const;

  LeaderInfo &getLeaderInfo(AllowRecompute recompute, int leg) const;

  mutable int leaderTimeVersion = -1;
  mutable vector<LeaderInfo> tLeaderTime;
  mutable vector<LeaderInfo> tLeaderTimeOld;
  mutable map<int, int> tBestTimePerCourse;

  int tSplitRevision;
  map<int, vector<int> > tSplitAnalysisData;
  map<int, vector<int> > tCourseLegLeaderTime;
  map<int, vector<int> > tCourseAccLegLeaderTime;
  mutable vector<int> tFirstStart;
  mutable vector<int> tLastStart;

  mutable ClassStatus tStatus;
  mutable int tStatusRevision;

  // A map with places for given times on given legs
  inthashmap *tLegTimeToPlace;
  inthashmap *tLegAccTimeToPlace;

  struct PlaceTime {
    int leader = -1;
    map<int, int> timeToPlace;
  };

  vector<unordered_map<int, PlaceTime>> teamLegCourseControlToLeaderPlace;
  
  void insertLegPlace(int from, int to, int time, int place);
  void insertAccLegPlace(int courseId, int controlNo, int time, int place);

  /** Get relay/team accumulated leader time/place at control. */
  int getAccLegControlLeader(int teamLeg, int courseControlId) const;
  int getAccLegControlPlace(int teamLeg, int courseControlId, int time) const;
     
  // For sub split times
  int tLegLeaderTime;
  mutable int tNoTiming;
  mutable int tIgnoreStartPunch;

  // Sort classes for this index
  mutable int tSortIndex;
  mutable int tMaxTime;

  mutable bool isInitialized = false;

  // True when courses was changed on this client. Used to update course pool  bindings
  bool tCoursesChanged;

  // Used to force show of full multi course dialog
  bool tShowMultiDialog;

  static const int dataSize = 512;
  int getDISize() const {return dataSize;}

  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];
  vector< vector<wstring> > oDataStr;
  //Multicourse data
  string codeMultiCourse() const;
  //Fill courseId with id:s of used courses.
  void importCourses(const vector< vector<int> > &multi);
  static void parseCourses(const string &courses, vector< vector<int> > &multi, set<int> &courseId);
  set<int> &getMCourseIdSet(set<int> &in) const;

  //Multicourse leg methods
  string codeLegMethod() const;
  void importLegMethod(const string &courses);

  //bool sqlChanged;
  /** Pairs of changed entities. (Leg number, control id (or PunchFinish))
    (-1,-1) means all (leg, -1) means all on leg.
  */
  map< int, set<int> > sqlChangedControlLeg;
  map< int, set<int> > sqlChangedLegControl;

  void markSQLChanged(int leg, int control);

  void addTableRow(Table &table) const;
  pair<int, bool> inputData(int id, const wstring &input, int inputId,
                        wstring &output, bool noUpdate) override;

  void fillInput(int id, vector<pair<wstring, size_t>> &elements, size_t &selected) override;

  void exportIOFStart(xmlparser &xml);

  /** Setup transient data */
  void reinitialize(bool force) const;

  /** Recalculate derived data */
  void apply();

  void calculateSplits();
  void clearSplitAnalysis();

  /** Map to correct leg number for diff class/runner class (for example qual/final)*/
  int mapLeg(int inputLeg) const {
    if (inputLeg > 0 && legInfo.size() <= 1)
      return 0; // The case with different class for team/runner. Leg is an index in another class.
    return inputLeg;
  }

  /** Info about the result in the class for each leg.
      Use oEvent::analyseClassResultStatus to setup */
  mutable vector<ClassResultInfo> tResultInfo;

  /** Get/calculate sort index from candidate */
  int getSortIndex(int candidate) const;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

  static long long setupForkKey(const vector<int> indices, const vector< vector< vector<int> > > &courseKeys, vector<int> &ws);

  mutable vector<pClass> virtualClasses;
  pClass parentClass;

  mutable shared_ptr<QualificationFinal> qualificatonFinal;

  int tMapsRemaining;
  mutable int tMapsUsed;
  mutable int tMapsUsedNoVacant;

  // First is data revision, second is key
  mutable pair<int, map<string, int>> tTypeKeyToRunnerCount;

  enum CountKeyType {
    AllCompeting,
    Finished,
    ExpectedStarting,
    DNS,
    IncludeNotCompeting
  };

  static string getCountTypeKey(int leg, CountKeyType type, bool countVacant);

  void configureInstance(int instance, bool allowCreation) const;
public:

  static const shared_ptr<Table> &getTable(oEvent *oe);

  enum TransferFlags {
    FlagManualName = 1,
    FlagManualFees = 2,
  };

  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);

  /** The master class in a qualification/final scheme. */
  const pClass getParentClass() const { return parentClass; }

  const QualificationFinal *getQualificationFinal() const {
    reinitialize(false);
    return qualificatonFinal.get();
  }

  void clearQualificationFinal() const;

  bool isQualificationFinalClass() const {
    return parentClass && parentClass->isQualificationFinalBaseClass();
  }

  bool isQualificationFinalBaseClass() const {
    return qualificatonFinal != nullptr;
  }

  bool isTeamClass() const {
    int ns = getNumStages();
    return ns > 0 && getNumDistinctRunners() == 1;
  }

  /** Returns the number of possible final classes.*/
  int getNumQualificationFinalClasses() const;
  void loadQualificationFinalScheme(const wstring &fileName);

  void updateFinalClasses(oRunner *causingResult, bool updateStartNumbers);

  static void initClassId(oEvent &oe, const set<int>& classes);

  // Return true if forking in the class is locked
  bool lockedForking() const;
  void lockedForking(bool locked);

  bool lockedClassAssignment() const;
  void lockedClassAssignment(bool locked);

  // Draw data
  int getDrawFirstStart() const;
  void setDrawFirstStart(int st);
  int getDrawInterval() const;
  void setDrawInterval(int st);
  int getDrawVacant() const;
  void setDrawVacant(int st);
  int getDrawNumReserved() const;
  void setDrawNumReserved(int st);

  enum class DrawSpecified {
    FixedTime = 1, Vacant = 2, Extra = 4
  };
  
  void setDrawSpecification(const vector<DrawSpecified> &ds);
  set<DrawSpecified> getDrawSpecification() const;

  /** Return an actual linear index for this class. */
  int getLinearIndex(int index, bool isLinear) const;

  /** Split class into subclasses. */
  void splitClass(ClassSplitMethod method, const vector<int> &parts, vector<int> &outClassId);
  void mergeClass(int classIdSec);
  
  void drawSeeded(ClassSeedMethod seed, int leg, int firstStart, int interval, const vector<int> &groups,
                  bool noClubNb, bool reverse, int pairSize);
  /** Returns true if the class is setup so that changeing one runner can effect all others. (Pursuit)*/
  bool hasClassGlobalDependence() const;
  // Autoassign new bibs
  static void extractBibPatterns(oEvent &oe, map<int, pair<wstring, int> > &patterns);
  pair<int, wstring> getNextBib(map<int, pair<wstring, int> > &patterns); // Version that calculates next free bib from cached data (fast, no gap usage)
  pair<int, wstring> getNextBib(); // Version that calculates next free bib (slow, but reuses gaps)

  bool usesCourse(const oCourse &crs) const;
  
  /** Returns an (overestimate) of the actual number of forks.*/
  int getNumForks() const;

  bool checkStartMethod();

  static int extractBibPattern(const wstring &bibInfo, wchar_t pattern[32]);

  bool isParallel(size_t leg) const {
    if (leg < legInfo.size())
      return legInfo[leg].isParallel();
    else
      return false;
  }

  bool isOptional(size_t leg) const {
    if (leg < legInfo.size())
      return legInfo[leg].isOptional();
    else
      return false;
  }

  pClass getVirtualClass(int instance, bool allowCreation);
  const pClass getVirtualClass(int instance) const;

  ClassStatus getClassStatus() const;
  static void fillClassStatus(vector<pair<wstring, wstring>> &statusClass);

  ClassMetaType interpretClassType() const;

  int getMaximumRunnerTime() const;

  void remove();
  bool canRemove() const;

  void forceShowMultiDialog(bool force) {tShowMultiDialog = force;}

  /// Return first and last start of runners in class
  void getStartRange(int leg, int &firstStart, int &lastStart) const;

  /** Return true if pure rogaining class, with time limit (sort results by points) */
  bool isRogaining() const;

  /** Get the place for the specified leg (CommonControl of course == start/finish) and time. 0 if not found */
  int getLegPlace(int from, int to, int time) const;

  /** Get accumulated leg place */
  int getAccLegPlace(int courseId, int controlNo, int time) const;

  /** Get cached sort index */
  int getSortIndex() const {return tSortIndex;};

  /// Guess type from class name
  void assignTypeFromName();

  bool isSingleRunnerMultiStage() const;

  bool wasSQLChanged(int leg, int control) const;// {return sqlChanged;}

  void getStatistics(const set<int> &feeLock, int &entries, int &started) const;

  int getBestInputTime(AllowRecompute recompute, int leg) const;
  int getBestLegTime(AllowRecompute recompute, int leg, bool computedTime) const;
  int getBestTimeCourse(AllowRecompute recompute, int courseId) const;

  int getTotalLegLeaderTime(AllowRecompute recompute, int leg, bool computedTime, bool includeInput) const;

  wstring getInfo() const;
  // Returns true if the class has a pool of courses
  bool hasCoursePool() const;
  // Set whether to use a pool or not
  void setCoursePool(bool p);
  // Get the best matching course from a pool
  pCourse selectCourseFromPool(int leg, const SICard &card) const;
  // Update changed course pool
  void updateChangedCoursePool();

  /** Reset cache of leader times */
  void resetLeaderTime() const;

  ClassType getClassType() const;

  bool startdataIgnored(int i) const;
  bool restartIgnored(int i) const;

  StartTypes getStartType(int leg) const;
  LegTypes getLegType(int leg) const;
  int getStartData(int leg) const;
  int getRestartTime(int leg) const;
  int getRopeTime(int leg) const;
  wstring getStartDataS(int leg) const;
  wstring getRestartTimeS(int leg) const;
  wstring getRopeTimeS(int leg) const;

  // Get the index of the base leg for this leg (=first race of leg's runner)
  int getLegRunner(int leg) const;
  // Get the index of this leg for its runner.
  int getLegRunnerIndex(int leg) const;
  // Set the runner index for the specified leg. (Used when several legs are run be the same person)
  void setLegRunner(int leg, int runnerNo);

  // Get number of races run by the runner of given leg
  int getNumMultiRunners(int leg) const;

  // Get number of legs, not counting parallel legs
  int getNumLegNoParallel() const;

  // Split a linear leg index into non-parallel leg number and order
  // number on the leg (zero-indexed). Returns true if the legNumber is parallel
  bool splitLegNumberParallel(int leg, int &legNumber, int &legOrder) const;

  // The inverse of splitLegNumberParallel. Return -1 on invalid input.
  int getLegNumberLinear(int legNumber, int legOrder) const;

  //Get the number of parallel runners on a given leg (before and after)
  int getNumParallel(int leg) const;

  // Get the linear leg number of the next (non-parallel with this) leg
  int getNextBaseLeg(int leg) const;

  // Get the linear leg number of the preceeding leg
  int getPreceedingLeg(int leg) const;

  // Get result defining leg (for parallel legs, the last leg in the currrent parallel set)
  int getResultDefining(int leg) const;


  /// Get a string 1, 2a, etc describing the number of the leg
  wstring getLegNumber(int leg) const;

  // Return the number of distinct runners for one
  // "team" in this class.
  int getNumDistinctRunners() const;

  // Return the minimal number of runners in team
  int getNumDistinctRunnersMinimal() const;
  void setStartType(int leg, StartTypes st, bool noThrow);
  void setLegType(int leg, LegTypes lt);

  bool setStartData(int leg, const wstring &s);
  bool setStartData(int leg, int value);
  
  void setRestartTime(int leg, const wstring &t);
  void setRopeTime(int leg, const wstring &t);

  void setNoTiming(bool noResult);
  bool getNoTiming() const;

  void setIgnoreStartPunch(bool ignoreStartPunch);
  bool ignoreStartPunch() const;

  void setFreeStart(bool freeStart);
  bool hasFreeStart() const;

  void setDirectResult(bool directResult);
  bool hasDirectResult() const;
  bool isValidLeg(int legIndex) const {
    return legIndex == -1 || legIndex == 0 || (legIndex > 0 && legIndex<int(MultiCourse.size()));
  }
  bool isCourseUsed(int Id) const;
  wstring getLength(int leg) const;

  // True if the multicourse structure is in use
  bool hasMultiCourse() const {return MultiCourse.size()>0;}

  // True if there is a true multicourse usage.
  bool hasTrueMultiCourse() const;

  unsigned getNumStages() const {return MultiCourse.size();}
  /** Get the set of true legs, identifying parallell legs etc. Returns indices into
   legInfo of the last leg of the true leg (first), and true leg (second).*/
  struct TrueLegInfo {
  protected:
    TrueLegInfo(int first_, int second_) : first(first_), second(second_) {}
    friend class oClass;
  public:
    int first;
    int second;
    int nonOptional; // Index of a leg with a non-optional runner of that leg (which e.g. defines the course)
  };

  void getTrueStages(vector<TrueLegInfo> &stages) const;

  unsigned getLastStageIndex() const {return max<signed>(MultiCourse.size(), 1)-1;}

  void setNumStages(int no);

  bool operator<(const oClass &b) {return tSortIndex<b.tSortIndex || (tSortIndex == b.tSortIndex && Id < b.Id);}

  // Get total number of runners running this class.
  // Use checkFirstLeg to only check the number of runners running leg 1.
  int getNumRunners(bool checkFirstLeg, bool noCountVacant, bool noCountNotCompeting) const;
  void getNumResults(int leg, int &total, int &finished, int &dns) const;

  //Get remaining maps for class (or int::minvalue)
  int getNumRemainingMaps(bool forceRecalculate) const;

  void setNumberMaps(int nm);
  int getNumberMaps(bool rawAttribute = false) const;

  const wstring &getName() const {return Name;}
  void setName(const wstring &name, bool manualSet);

  void Set(const xmlobject *xo);
  bool Write(xmlparser &xml);

  bool fillStageCourses(gdioutput &gdi, int stage,
                        const string &name) const;

  static void fillStartTypes(gdioutput &gdi, const string &name, bool firstLeg);
  static void fillLegTypes(gdioutput &gdi, const string &name);

  pCourse getCourse(bool getSampleFromRunner = false) const;

  void getCourses(int leg, vector<pCourse> &courses) const;

  pCourse getCourse(int leg, unsigned fork=0, bool getSampleFromRunner = false) const;
  int getCourseId() const {if (Course) return Course->getId(); else return 0;}
  void setCourse(pCourse c);

  bool addStageCourse(int stage, int courseId, int index);
  bool addStageCourse(int stage, pCourse pc,  int index);
  void clearStageCourses(int stage);
  bool moveStageCourse(int stage, int index, int offset);

  bool removeStageCourse(int stage, int courseId, int position);

  void getAgeLimit(int &low, int &high) const;
  void setAgeLimit(int low, int high);

  int getExpectedAge() const;

  PersonSex getSex() const;
  void setSex(PersonSex sex);

  wstring getStart() const;
  void setStart(const wstring &start);

  int getBlock() const;
  void setBlock(int block);

  bool getAllowQuickEntry() const;
  void setAllowQuickEntry(bool quick);

  AutoBibType getAutoBibType() const;
  BibMode getBibMode() const;
  void setBibMode(BibMode bibMode);

  wstring getType() const;
  void setType(const wstring &type);

  // Get class default fee from competition, depending on type(?)
  // a non-zero fee is changed only if resetFee is true
  void addClassDefaultFee(bool resetFee);

  // Get entry fee depending on date and age
  int getEntryFee(const wstring &date, int age) const;

  /// Get all class fees
  vector<pair<wstring, size_t>> getAllFees() const;

  // Clear cached data
  void clearCache(bool recalculate);

  // Check if forking is fair
  bool checkForking(vector< vector<int> > &legOrder,
                    vector< vector<int> > &forks,
                    set< pair<int, int> > &unfairLegs) const;

  // Automatically setup forkings using the specified courses.
  // Returns <number of forkings created, number of courses used>
  pair<int, int> autoForking(const vector< vector<int> > &inputCourses, int numToGenerateMax);

  bool hasUnorderedLegs() const;
  void setUnorderedLegs(bool order);
  void getParallelCourseGroup(int leg, int startNo, vector< pair<int, pCourse> > &group) const;
  // Returns 0 for no parallel selection (= normal mode)
  pCourse selectParallelCourse(const oRunner &r, const SICard &sic);
  void getParallelRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const;
  void getParallelOptionalRange(int leg, int& parLegRangeMin, int& parLegRangeMax) const;

  bool hasAnyCourse(const set<int> &crsId) const;

  GeneralResult *getResultModule() const;
  void setResultModule(const string &tag);
  const string &getResultModuleTag() const;

  void merge(const oBase &input, const oBase *base) final;

  oClass(oEvent *poe);
  oClass(oEvent *poe, int id);
  virtual ~oClass();
  void clearDuplicate();

  friend class oAbstractRunner;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
  friend class TabSpeaker;
};

static const oClass::DrawSpecified DrawKeys[4] = { oClass::DrawSpecified::FixedTime,
                                                   oClass::DrawSpecified::Vacant, 
                                                   oClass::DrawSpecified::Extra };
