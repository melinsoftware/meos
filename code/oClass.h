// oClass.h: interface for the oClass class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OCLASS_H__63E948E3_3C06_4404_8E72_2185582FF30F__INCLUDED_)
#define AFX_OCLASS_H__63E948E3_3C06_4404_8E72_2185582FF30F__INCLUDED_

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

#include "oCourse.h"
#include <vector>
#include <set>
#include <map>
#include "inthashmap.h"
class oClass;
typedef oClass* pClass;
class oDataInterface;

enum PersonSex;

enum StartTypes {
  STTime=0,
  STChange,
  STDrawn,
  STHunting,
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
  BibSame,
  BibAdd,
  BibFree,
  BibLeg,
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
  //Interpreteation depends. Can be starttime/first start
  //or number of earlier legs to consider.
  int legStartData;
  int legRestartTime;
  int legRopeTime;
  int duplicateRunner;

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


enum ClassType {oClassIndividual=1, oClassPatrol=2,
                oClassRelay=3, oClassIndividRelay=4};

enum ClassMetaType {ctElite, ctNormal, ctYouth, ctTraining,
                    ctExercise, ctOpen, ctUnknown};

class Table;
class oClass : public oBase
{
public:
  enum ClassStatus {Normal, Invalid, InvalidRefund};

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
    LeaderInfo() {bestTimeOnLeg = 0; totalLeaderTime = 0; inputTime = 0; totalLeaderTimeInput = 0;}
    void reset() {bestTimeOnLeg = 0; totalLeaderTime = 0; inputTime = 0; totalLeaderTimeInput = 0;}
    int bestTimeOnLeg;
    int totalLeaderTime;
    int totalLeaderTimeInput; //Team total including input
    int inputTime;
  };

  vector<LeaderInfo> tLeaderTime;
  map<int, int> tBestTimePerCourse;

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

  void insertLegPlace(int from, int to, int time, int place);
  void insertAccLegPlace(int courseId, int controlNo, int time, int place);

  // For sub split times
  int tLegLeaderTime;
  mutable int tNoTiming;
  mutable int tIgnoreStartPunch;

  // Sort classes for this index
  int tSortIndex;
  int tMaxTime;

  // True when courses was changed on this client. Used to update course pool  bindings
  bool tCoursesChanged;

  // Used to force show of full multi course dialog
  bool tShowMultiDialog;

  static const int dataSize = 512;
  int getDISize() const {return dataSize;}

  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

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
  bool inputData(int id, const wstring &input, int inputId,
                        wstring &output, bool noUpdate);

  void fillInput(int id, vector< pair<wstring, size_t> > &elements, size_t &selected);

  void exportIOFStart(xmlparser &xml);

  /** Setup transient data */
  void reinitialize();

  /** Recalculate derived data */
  void apply();

  void calculateSplits();
  void clearSplitAnalysis();

  /** Info about the result in the class for each leg.
      Use oEvent::analyseClassResultStatus to setup */
  mutable vector<ClassResultInfo> tResultInfo;

  /** Get/calculate sort index from candidate */
  int getSortIndex(int candidate);

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

  static long long setupForkKey(const vector<int> indices, const vector< vector< vector<int> > > &courseKeys, vector<int> &ws);

public:

  static void initClassId(oEvent &oe);

  // Return true if forking in the class is locked
  bool lockedForking() const;
  void lockedForking(bool locked);

  // Draw data
  int getDrawFirstStart() const;
  void setDrawFirstStart(int st);
  int getDrawInterval() const;
  void setDrawInterval(int st);
  int getDrawVacant() const;
  void setDrawVacant(int st);
  int getDrawNumReserved() const;
  void setDrawNumReserved(int st);

  /** Return an actual linear index for this class. */
  int getLinearIndex(int index, bool isLinear) const;

  /** Split class into subclasses. */
  void splitClass(ClassSplitMethod method, const vector<int> &parts, vector<int> &outClassId);
  void mergeClass(int classIdSec);
  
  void drawSeeded(ClassSeedMethod seed, int leg, int firstStart, int interval, const vector<int> &groups,
                  bool noClubNb, bool reverse, int pairSize);
  /** Returns true if the class is setup so that changeing one runner can effect all others. (Pursuit)*/
  bool hasClassGlobalDependance() const;
  // Autoassign new bibs
  static void extractBibPatterns(oEvent &oe, map<int, pair<wstring, int> > &patterns);
  pair<int, wstring> getNextBib(map<int, pair<wstring, int> > &patterns); // Version that calculates next free bib from cached data (fast, no gap usage)
  pair<int, wstring> oClass::getNextBib(); // Version that calculates next free bib (slow, but reuses gaps)

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

  ClassStatus getClassStatus() const;

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

  int getBestInputTime(int leg) const;
  int getBestLegTime(int leg) const;
  int getBestTimeCourse(int courseId) const;

  int getTotalLegLeaderTime(int leg, bool includeInput) const;

  wstring getInfo() const;
  // Returns true if the class has a pool of courses
  bool hasCoursePool() const;
  // Set whether to use a pool or not
  void setCoursePool(bool p);
  // Get the best matching course from a pool
  pCourse selectCourseFromPool(int leg, const SICard &card) const;
  // Update changed course pool
  void updateChangedCoursePool();

  void resetLeaderTime();

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


  string getClassResultStatus() const;

  bool isCourseUsed(int Id) const;
  wstring getLength(int leg) const;

  // True if the multicourse structure is in use
  bool hasMultiCourse() const {return MultiCourse.size()>0;}

  // True if there is a true multicourse usage.
  bool hasTrueMultiCourse() const;

  unsigned getNumStages() const {return MultiCourse.size();}
  /** Get the set of true legs, identifying parallell legs etc. Returns indecs into
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

  bool operator<(const oClass &b){return tSortIndex<b.tSortIndex;}

  // Get total number of runners running this class.
  // Use checkFirstLeg to only check the number of runners running leg 1.
  int getNumRunners(bool checkFirstLeg, bool noCountVacant, bool noCountNotCompeting) const;

  //Get remaining maps for class (or int::minvalue)
  int getNumRemainingMaps(bool recalculate) const;

  const wstring &getName() const {return Name;}
  void setName(const wstring &name);

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

  bool addStageCourse(int stage, int courseId);
  bool addStageCourse(int stage, pCourse pc);
  void clearStageCourses(int stage);

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

  // Clear cached data
  void clearCache(bool recalculate);

  // Check if forking is fair
  bool checkForking(vector< vector<int> > &legOrder,
                    vector< vector<int> > &forks,
                    set< pair<int, int> > &unfairLegs) const;

  // Automatically setup forkings using the specified courses.
  // Returns <number of forkings created, number of courses used>
  pair<int, int> autoForking(const vector< vector<int> > &inputCourses);

  bool hasUnorderedLegs() const;
  void setUnorderedLegs(bool order);
  void getParallelCourseGroup(int leg, int startNo, vector< pair<int, pCourse> > &group) const;
  // Returns 0 for no parallel selection (= normal mode)
  pCourse selectParallelCourse(const oRunner &r, const SICard &sic);

  void getParallelRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const;

  bool hasAnyCourse(const set<int> &crsId) const;

  oClass(oEvent *poe);
  oClass(oEvent *poe, int id);
  virtual ~oClass();

  friend class oAbstractRunner;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
  friend class TabSpeaker;
};

#endif // !defined(AFX_OCLASS_H__63E948E3_3C06_4404_8E72_2185582FF30F__INCLUDED_)
