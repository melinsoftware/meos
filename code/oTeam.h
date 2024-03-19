#pragma once

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
#include "oBase.h"
#include "oRunner.h"
#include <set>

class oTeam;//:public oBase {};
typedef oTeam* pTeam;
typedef const oTeam* cTeam;

const unsigned int maxRunnersTeam=32;

class oTeam final : public oAbstractRunner {
public:
  enum ResultCalcCacheSymbol {
    RCCCourse,
    RCCSplitTime,
    RCCCardTimes,
    RCCCardPunches,
    RCCCardControls,
    RCCLast
  };

private:
  int getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputedRunnerTime) const;
  /** Return the total time the team has been resting (pursuit start etc.) up to the specified leg */
  int getLegRestingTime(int leg, bool useComputedRunnerTime) const;
  
  void speakerLegInfo(int leg, int specifiedLeg, int courseControlId,
                      int &missingLeg, int &totalLeg,
                      RunnerStatus &status, int &runningTime) const;
  void propagateClub();

protected:

  vector<pRunner> Runners;
  void setRunnerInternal(int k, pRunner r);

  static const int dataSize = 256;
  int getDISize() const final {return dataSize;}
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  // Remove runner r by force and mark as need correction
  void correctRemove(pRunner r);

  // Update hash
  void changeId(int newId);

  struct TeamPlace {
    DynamicValue p; // Day result
    DynamicValue totalP; // Total result
  };

  mutable vector<TeamPlace> tPlace;

  TeamPlace &getTeamPlace(int leg) const;

  struct ComputedLegResult {
    int version = -1;
    int time = 0;
    RunnerStatus status = StatusUnknown;

    ComputedLegResult() {};
  };

  mutable vector<ComputedLegResult> tComputedResults;
  
  void setTmpTime(int t) const { tmpSortTime = tmpDefinedTime = t; }
  mutable int tmpSortTime;
  mutable int tmpDefinedTime;
  mutable int tmpSortStatus;
  mutable RunnerStatus tmpCachedStatus;

  mutable vector< vector< vector<int> > > resultCalculationCache;
  
  struct RogainingResult {
    RogainingResult() { reset(); }

    int points;
    int reduction;
    int overtime;
    
    void reset() {
      points = 0;
      reduction = 0;
      overtime = 0;
    }
  };
  mutable pair<int, RogainingResult> tTeamPatrolRogainingAndVersion;

  const ComputedLegResult &getComputedResult(int leg) const;
  void setComputedResult(int leg, ComputedLegResult &comp) const;

  string getRunners() const;
  bool matchTeam(int number, const wchar_t *s_lc) const;
  int tNumRestarts; //Number of restarts for team

  int getLegToUse(int leg) const; // Get the number of the actual
                                  // runner to consider for a given leg
                                  // Maps -1 to last runner

  void addTableRow(Table &table) const;

  pair<int, bool> inputData(int id, const wstring &input,
                            int inputId, wstring &output, bool noUpdate);

  void fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected) override;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void fillInSortData(SortOrder so, 
                      int leg,
                      bool linearLeg, 
                      map<int, int> &classId2Linear, 
                      bool &hasRunner) const;

  void changedObject() final;

public:

  bool matchAbstractRunner(const oAbstractRunner* target) const override;

  /** Deduce from computed runner times.*/
  RunnerStatus deduceComputedStatus() const;
  int deduceComputedRunningTime() const;
  int deduceComputedPoints() const;

  const pair<wstring, int> getRaceInfo() override;

  static const shared_ptr<Table> &getTable(oEvent *oe);

  /** Check the the main leg is set if any parallel is set. Returns true if corrections where made.*/
  bool checkValdParSetup();

  int getRanking() const;

  int classInstance() const override {
    return 0; // Not supported
  }

  void resetResultCalcCache() const;
  vector< vector<int> > &getResultCache(ResultCalcCacheSymbol symb) const;
  void setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int> &data) const;
  void markClassChanged(int controlId);

  void setClub(const wstring &name) override;
  pClub setClubId(int clubId) override;

  /// Returns team fee (including participating runners fees)
  int getTeamFee() const;

  /** Remove runner from team (and from competition)
    @param askRemoveRunner ask if runner should be removed from cmp. Otherwise just do it.
    */
  void removeRunner(gdioutput &gdi, bool askRemoveRunner, int runnerIx);
  // Get entry date of team
  wstring getEntryDate(bool dummy) const;

  // Get the team itself
  cTeam getTeam() const {return this;}
  pTeam getTeam() {return this;}

  int getRunningTime(bool computedTime) const override;

  /// Input data for multiday event
  void setInputData(const oTeam &t);

  void remove();
  bool canRemove() const;

  void prepareRemove();

  bool skip() const {return isRemoved();}

  void setTeamMemberStatus(RunnerStatus memberStatus); // Set DNS or CANCEL, NotCompeting etc to team and its members
  // If apply is triggered by a runner, don't go further than that runner.
  void apply(ChangeType ct, pRunner source) override;

  // Set bibs on team members
  void applyBibs(); 

  void quickApply();

  // Evaluate card and times. Synchronize to server if changeType is update.
  void evaluate(ChangeType changeType);

  void adjustMultiRunners();

  int getRogainingPoints(bool computed, bool multidayTotal) const override;
  int getRogainingReduction(bool computed) const override;
  int getRogainingOvertime(bool computed) const override;
  int getRogainingPointsGross(bool computed) const override;

  int getRogainingPatrolPoints(bool multidayTotal) const;
  int getRogainingPatrolReduction() const;
  int getRogainingPatrolOvertime() const;

  void fillSpeakerObject(int leg, int courseControlId, int previousControlCourseId,
                          bool totalResult, oSpeakerObject &spk) const;

  bool isRunnerUsed(int Id) const;
  void setRunner(unsigned i, pRunner r, bool syncRunner);

  pRunner getRunner(unsigned leg) const;
  int getNumRunners() const {return Runners.size();}
  int getNumAssignedRunners() const { 
    int cnt = 0;
    for (auto &r : Runners) if (r) cnt++;
    return cnt;
  }

  /** For legs with many parallel / extra runner, get the runner with the 
      first finish time */
  pRunner getRunnerBestTimePar(int linearLegInput) const;

  void decodeRunners(const string &rns, vector<int> &rid);
  void importRunners(const vector<int> &rns);
  void importRunners(const vector<pRunner> &rns);


  RunnerStatus getStatusComputed(bool allowUpdate) const final;

  int getPlace(bool allowUpdate = true) const override {return getLegPlace(-1, false, allowUpdate);}
  int getTotalPlace(bool allowUpdate = true) const override {return getLegPlace(-1, true, allowUpdate);}

  int getNumShortening() const;
  // Number of shortenings up to and including a leg
  int getNumShortening(int leg) const;
  
  wstring getDisplayName() const;
  wstring getDisplayClub() const;

  void setBib(const wstring &bib, int numericalBib, bool updateStartNo) override;

  int getLegStartTime(int leg) const;
  wstring getLegStartTimeS(int leg) const;
  wstring getLegStartTimeCompact(int leg) const;

  wstring getLegFinishTimeS(int leg, SubSecond mode) const;
  int getLegFinishTime(int leg) const;

  int getTimeAfter(int leg, bool allowUpdate) const override;

  //Get total running time after leg
  wstring getLegRunningTimeS(int leg, bool computed, bool multidayTotal, SubSecond mode) const;

  int getLegRunningTime(int leg, bool computed, bool multidayTotal) const;
  
  // Get the team's total running time when starting specified leg
  int getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const;

  RunnerStatus getLegStatus(int leg, bool computed, bool multidayTotal) const;
  const wstring &getLegStatusS(int leg, bool computed, bool multidayTotal) const;

  wstring getLegPlaceS(int leg, bool multidayTotal) const;
  wstring getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const;
  int getLegPlace(int leg, bool multidayTotal, bool allowUpdate = true) const;

  bool isResultUpdated(bool totalResult) const override;

  static bool compareSNO(const oTeam &a, const oTeam &b);
  static bool compareName(const oTeam &a, const oTeam &b) {return a.sName<b.sName;}
  static bool compareResult(const oTeam &a, const oTeam &b);
  static bool compareResultNoSno(const oTeam &a, const oTeam &b);
  static bool compareResultClub(const oTeam& a, const oTeam& b);

  static void checkClassesWithReferences(oEvent &oe, set<int> &clsWithRef);
  static void convertClassWithReferenceToPatrol(oEvent &oe, const set<int> &clsWithRef);

  void set(const xmlobject &xo);
  bool write(xmlparser &xml);

  void merge(const oBase &input, const oBase *base) final;

  bool isTeam() const final { return true; }


  oTeam(oEvent *poe, int id);
  oTeam(oEvent *poe);
  virtual ~oTeam(void);

  friend class oClass;
  friend class oRunner;
  friend class MeosSQL;
  friend class oEvent;
};
