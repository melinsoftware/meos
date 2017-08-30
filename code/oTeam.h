#pragma once

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
#include "oBase.h"
#include "oRunner.h"
#include <set>

class oTeam;//:public oBase {};
typedef oTeam* pTeam;
typedef const oTeam* cTeam;

const unsigned int maxRunnersTeam=32;

class oTeam : public oAbstractRunner
{
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
  int getLegRunningTimeUnadjusted(int leg, bool multidayTotal) const;
  /** Return the total time the team has been resting (pursuit start etc.) up to the specified leg */
  int getLegRestingTime(int leg) const;
  
  void speakerLegInfo(int leg, int specifiedLeg, int courseControlId,
                      int &missingLeg, int &totalLeg,
                      RunnerStatus &status, int &runningTime) const;

protected:
  //pRunner Runners[maxRunnersTeam];
  vector<pRunner> Runners;
  void setRunnerInternal(int k, pRunner r);

  static const int dataSize = 160;
  int getDISize() const {return dataSize;}
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  // Remove runner r by force and mark as need correction
  void correctRemove(pRunner r);

  // Update hash
  void changeId(int newId);

  struct TeamPlace {
    int p; // Day result
    int totalP; // Total result
  };

  TeamPlace _places[maxRunnersTeam];
  
  int _sortTime;
  int _sortStatus;
  RunnerStatus _cachedStatus;

  mutable vector< vector< vector<int> > > resultCalculationCache;

  string getRunners() const;
  bool matchTeam(int number, const wchar_t *s_lc) const;
  int tNumRestarts; //Number of restarts for team

  int getLegToUse(int leg) const; // Get the number of the actual
                                  // runner to consider for a given leg
                                  // Maps -1 to last runner

  void addTableRow(Table &table) const;

  bool inputData(int id, const wstring &input,
                 int inputId, wstring &output, bool noUpdate);

  void fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected);

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

public:
  /** Check the the main leg is set if any parallel is set. Returns true if corrections where made.*/
  bool checkValdParSetup();

  int getRanking() const;

  void resetResultCalcCache() const;
  vector< vector<int> > &getResultCache(ResultCalcCacheSymbol symb) const;
  void setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int> &data) const;

  void markClassChanged(int controlId);

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

  int getRunningTime() const;

  /// Input data for multiday event
  void setInputData(const oTeam &t);

  /// Get total status for multiday event
  RunnerStatus getTotalStatus() const;

  void remove();
  bool canRemove() const;

  void prepareRemove();

  bool skip() const {return isRemoved();}
  void setTeamNoStart(bool dns);
  // If apply is triggered by a runner, don't go further than that runner.
  bool apply(bool sync, pRunner source, bool setTmpOnly);

  void quickApply();

  void evaluate(bool sync);

  bool adjustMultiRunners(bool sync);

  int getRogainingPoints(bool multidayTotal) const;
  int getRogainingReduction() const;
  int getRogainingOvertime() const;
  int getRogainingPointsGross() const;
  
  void fillSpeakerObject(int leg, int courseControlId, int previousControlCourseId,
                          bool totalResult, oSpeakerObject &spk) const;

  bool isRunnerUsed(int Id) const;
  void setRunner(unsigned i, pRunner r, bool syncRunner);

  pRunner getRunner(unsigned leg) const;
  int getNumRunners() const {return Runners.size();}

  void decodeRunners(const string &rns, vector<int> &rid);
  void importRunners(const vector<int> &rns);
  void importRunners(const vector<pRunner> &rns);

  int getPlace() const {return getLegPlace(-1, false);}
  int getTotalPlace() const  {return getLegPlace(-1, true);}

  int getNumShortening() const;
  // Number of shortenings up to and including a leg
  int getNumShortening(int leg) const;
  
  wstring getDisplayName() const;
  wstring getDisplayClub() const;

  void setBib(const wstring &bib, int numericalBib, bool updateStartNo, bool setTmpOnly);

  int getLegStartTime(int leg) const;
  wstring getLegStartTimeS(int leg) const;
  wstring getLegStartTimeCompact(int leg) const;

  wstring getLegFinishTimeS(int leg) const;
  int getLegFinishTime(int leg) const;

  int getTimeAfter(int leg) const;

  //Get total running time after leg
  wstring getLegRunningTimeS(int leg, bool multidayTotal) const;

  int getLegRunningTime(int leg, bool multidayTotal) const;
  int getLegPrelRunningTime(int leg) const;
  wstring getLegPrelRunningTimeS(int leg) const;

  RunnerStatus getLegStatus(int leg, bool multidayTotal) const;
  const wstring &getLegStatusS(int leg, bool multidayTotal) const;

  wstring getLegPlaceS(int leg, bool multidayTotal) const;
  wstring getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const;
  int getLegPlace(int leg, bool multidayTotal) const;

  static bool compareSNO(const oTeam &a, const oTeam &b);
  static bool compareName(const oTeam &a, const oTeam &b) {return a.sName<b.sName;}
  static bool compareResult(const oTeam &a, const oTeam &b);
  static bool compareStartTime(const oTeam &a, const oTeam &b);

  static void checkClassesWithReferences(oEvent &oe, set<int> &clsWithRef);
  static void convertClassWithReferenceToPatrol(oEvent &oe, const set<int> &clsWithRef);

  void set(const xmlobject &xo);
  bool write(xmlparser &xml);

  oTeam(oEvent *poe, int id);
  oTeam(oEvent *poe);
  virtual ~oTeam(void);

  friend class oClass;
  friend class oRunner;
  friend class MeosSQL;
  friend class oEvent;
};
