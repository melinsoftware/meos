/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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

#pragma once
#include <map>
#include <set>
#include <vector>
#include <tuple>
#include <unordered_map>

class oEvent;
class xmlobject;
typedef vector<xmlobject> xmlList;
class xmlparser;
class gdioutput;
class oRunner;
class oClub;
class oTeam;
class oCourse;
class oControl;
class oClass;
class oDataInterface;
class oDataConstInterface;
class oAbstractRunner;
struct RunnerWDBEntry;
class RunnerDB;

typedef oRunner * pRunner;
typedef oClass * pClass;
typedef oClub * pClub;
typedef oTeam * pTeam;
typedef oCourse *pCourse;

struct XMLService {
  int id;
  wstring name;

  XMLService(int id, const wstring &name) : id(id), name(name) {}
  XMLService() {}
};

class IOF30Interface {
  oEvent &oe;

  int cachedStageNumber;
  int entrySourceId;

  bool splitLateFee;
  bool useGMT; // Use GMT when exporting

  // Export teams as individual
  bool teamsAsIndividual;
  // Unroll course loops
  bool unrollLoops;
  // Include data on stage number
  bool includeStageRaceInfo;

  bool preferShortName;

  int classIdOffset = 0;
  int courseIdOffset = 0;

  const IOF30Interface &operator=(const IOF30Interface &) = delete;

  set<wstring> matchedClasses;

  list<XMLService> services;

  struct LegInfo {
    int maxRunners;
    int minRunners;
    LegInfo() : maxRunners(1), minRunners(1) {}
    void setMinRunners(int nr) {minRunners = max(minRunners, nr); maxRunners = max(maxRunners, nr);}
    void setMaxRunners(int nr) {maxRunners = max(maxRunners, nr);}
  };

  struct FeeInfo {
    double fee;
    double taxable;
    double percentage; // Eventor / OLA stupidity

    wstring currency;

    wstring fromTime;
    wstring toTime;

    wstring fromBirthDate;
    wstring toBirthDate;

    bool includes(const FeeInfo &fo) const {
      if (toBirthDate != fo.toBirthDate || fromBirthDate != fo.fromBirthDate)
        return false;
      if (!includeFrom(fromTime, fo.fromTime))
        return false;
      if (!includeTo(toTime, fo.toTime))
        return false;
      /*if (!includeFrom(fromBirthDate, fo.fromBirthDate))
        return false;
      if (!includeTo(toBirthDate, fo.toBirthDate))
        return false;*/
      return true;
    }

    void add(FeeInfo &fi);

    wstring getDateKey() const {return fromTime + L" - " + toTime;}
    FeeInfo() : fee(0), taxable(0), percentage(0) {}

    const bool operator<(const FeeInfo &fi) const {
      return fee < fi.fee || (fee == fi.fee && taxable < fi.taxable);
    }
  private:
    bool includeFrom(const wstring &a, const wstring &b) const {
      if ( a > b || (b.empty() && !a.empty()) )
        return false;
      return true;
    }

    bool includeTo(const wstring &a, const wstring &b) const {
      if ( (!a.empty() && a < b) || (b.empty() && !a.empty()) )
        return false;
      return true;
    }
  };

  struct FeeStatistics {
    double fee;
    double lateFactor;
  };

  vector<FeeStatistics> feeStatistics;

  map<int, vector<tuple<int, int, pCourse>>> classToBibLegCourse;

  static void getAgeLevels(const vector<FeeInfo> &fees, const vector<int> &ix,
                           int &normalIx, int &redIx, wstring &youthLimit, wstring &seniorLimit);

  bool matchStageFilter(const set<int> &stageFilter, const xmlList &races);

  set<string> idProviders;
  pair<string, string> preferredIdProvider;
  vector<vector<pair<string, wstring>>> externalIdTypes;

  void readEvent(gdioutput &gdi, const xmlobject &xo,
                 map<int, vector<LegInfo> > &teamClassConfig);
  pRunner readPersonEntry(gdioutput &gdi, xmlobject &xo, pTeam team,
                          const map<int, vector<LegInfo> > &teamClassConfig,
                          const set<int> &stageFilter,
                          map<int, vector< pair<int, int> > > &personId2TeamLeg);
  pRunner readPerson(gdioutput &gdi, const xmlobject &xo);
  pClub readOrganization(gdioutput &gdi, const xmlobject &xo, bool saveToDB);
  pClass readClass(const xmlobject &xo,
                   map<int, vector<LegInfo> > &teamClassConfig);

  pTeam readTeamEntry(gdioutput &gdi, xmlobject &xTeam,
                      const set<int> &stageFilter,
                      map<int, pair<wstring, int> > &bibPatterns,
                      const map<int, vector<LegInfo> > &teamClassConfig,
                      map<int, vector< pair<int, int> > > &personId2TeamLeg);

  pRunner readPersonStart(gdioutput &gdi, pClass pc, xmlobject &xo, pTeam team,
                          const map<int, vector<LegInfo> > &teamClassConfig);

  pTeam readTeamStart(gdioutput &gdi, pClass pc, xmlobject &xTeam,
                      map<int, pair<wstring, int> > &bibPatterns,
                      const map<int, vector<LegInfo> > &teamClassConfig);


  pRunner readPersonResult(gdioutput &gdi, pClass pc, xmlobject &xo, pTeam team,
                          const map<int, vector<LegInfo> > &teamClassConfig);

  pTeam getCreateTeam(gdioutput &gdi, const xmlobject &xTeam, int expectedClassId, bool &newTeam);

  static int getIndexFromLegPos(int leg, int legorder, const vector<LegInfo> &setup);
  
  void prescanEntry(xmlobject & xo, set<int>& stages, xmlList &work);
  void readIdProviders(xmlobject &person, xmlList &ids, std::string &type);
  void setupClassConfig(int classId, const xmlobject &xTeam, map<int, vector<LegInfo> > &teamClassConfig);

  void setupRelayClasses(const map<int, vector<LegInfo> > &teamClassConfig);
  void setupRelayClass(pClass pc, const vector<LegInfo> &teamClassConfig);

  int parseISO8601Time(const xmlobject &xo);
  wstring getCurrentTime() const;
  wstring formatRelTime(int rt);

  static void getNationality(const xmlobject &xCountry, oDataInterface &di);

  static void getAmount(const xmlobject &xAmount, double &amount, wstring &currency);
  static void getAssignedFee(const xmlobject &xFee, double &fee, double &paid, double &taxable, double &percentage, wstring &currency);
  static void getFee(const xmlobject &xFee, FeeInfo &fee);
  static void getFeeAmounts(const xmlobject &xFee, double &fee, double &taxable, double &percentage, wstring &currency);

  void writeFees(xmlparser &xml, const oRunner &r) const;

  void writeAmount(xmlparser &xml, const char *tag, int amount) const;
  void writeAssignedFee(xmlparser &xml, const oAbstractRunner &tr, int paidForCard) const;
  void writeRentalCardService(xmlparser &xml, int cardFee, bool paid) const;

  void writeTeamForkings(xmlparser& xml) const;
  void writeTeamForking(xmlparser& xml, const oTeam& t) const;

  void getProps(vector<wstring> &props) const;

  void writeClassResult(xmlparser &xml, const oClass &c, const vector<pRunner> &r,
                        const vector<pTeam> &t);

  void writeClass(xmlparser &xml, const oClass &c);
  void writeCourse(xmlparser &xml, const oCourse &c);
  void writePersonResult(xmlparser &xml, const oRunner &r, bool includeCourse,
                         bool teamMember, bool hasInputTime);


  void writeTeamResult(xmlparser &xml, const oTeam &t, bool hasInputTime);

  void writeTeamEntryId(const oTeam& t, xmlparser& xml);

  void writeResult(xmlparser &xml, const oRunner &rPerson, const oRunner &rResultCarrier,
                   bool includeCourse, bool includeRaceNumber, bool teamMember, bool hasInputTime);

  void writePerson(xmlparser &xml, const oRunner &r);
  void writeClub(xmlparser &xml, const oClub &c, bool writeExtended) const;


  void getRunnersToUse(const pClass cls, vector<pRunner> &rToUse,
                       vector<pTeam> &tToUse, int leg, 
                       bool includeUnknown, bool skipVacant) const;

  void writeClassStartList(xmlparser &xml, const oClass &c, const vector<pRunner> &r,
                           const vector<pTeam> &t);

  void writePersonStart(xmlparser &xml, const oRunner &r, bool includeCourse, bool teamMember);
  
  void writeTeamNoPersonStart(xmlparser &xml, const oTeam &t, int leg, bool includeRaceNumber);

  void writeTeamStart(xmlparser &xml, const oTeam &t);

  void writeStart(xmlparser &xml, const oRunner &r, bool includeCourse,
                  bool includeRaceNumber, bool teamMember);


  pCourse haveSameCourse(const vector<pRunner> &r) const;
  void writeLegOrder(xmlparser &xml, const oClass *pc, int legNo) const;

  // Returns zero if no stage number
  int getStageNumber();

  bool readXMLCompetitorDB(const xmlobject &xCompetitor,
                           bool onlyWithClub, 
                           std::unordered_multimap<size_t, int> &duplicateCheck,
                           int &duplicateCount);
  void writeXMLCompetitorDB(xmlparser &xml, const RunnerDB &db, const RunnerWDBEntry &rde) const;

  int getStartIndex(const wstring &startId);

  bool readControl(const xmlobject &xControl);
  pCourse readCourse(const xmlobject &xcrs);

  void readCourseGroups(xmlobject xClassCourse, vector< vector<pCourse> > &crs);
  void bindClassCourse(oClass &pc, const vector<vector<pCourse>> &crs);

  static wstring constructCourseName(const xmlobject &xcrs);
  static wstring constructCourseName(const wstring &family, const wstring &name);

  void classAssignmentObsolete(gdioutput &gdi, xmlList &xAssignment, const map<wstring, pCourse> &courses,
                               const map<wstring, vector<pCourse> > &coursesFamilies);
  void classCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                             const map<wstring, pCourse> &courses,
                             const map<wstring, vector<pCourse> > &coursesFamilies);
  void personCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                              const map<wstring, pCourse> &courses);
  void teamCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                            const map<wstring, pCourse> &courses);

  void assignTeamCourse(gdioutput &gdi, oTeam *t, int iClass, int iBib, xmlList &xAssignment,
                        const map<wstring, pCourse> &courses);

  pCourse findCourse(gdioutput &gdi, const map<wstring, pCourse> &courses,
                     xmlobject &xPAssignment);

  wstring writeControl(xmlparser &xml, const oControl &c, set<wstring> &writtenId);

  void writeCourseInfo(xmlparser &xml, const oCourse &c);

  void writeFullCourse(xmlparser &xml, const oCourse &c,
                         const map<int, wstring> &ctrlId2ExportId);

  void readId(const xmlobject &person, int &pid, int64_t &extId, int64_t& extId2) const;

  set<int> readCrsIds;

  bool useEventorQuirks;

public:
  IOF30Interface(oEvent *oe, bool forceSplitFee, bool useEventorQuirks);
  virtual ~IOF30Interface() = default;

  void setIdOffset(int classIdOffsetIn, int courseIdOffsetIn) {
    classIdOffset = classIdOffsetIn;
    assert(courseIdOffsetIn == 0);
    courseIdOffset = courseIdOffsetIn;
  }


  static void getLocalDateTime(const wstring &datetime, wstring &dateOut, wstring &timeOut);

  static void getLocalDateTime(const wstring &date, const wstring &time,
                               wstring &dateOut, wstring &timeOut);
  static void getLocalDateTime(const string &date, const string &time,
                               string &dateOut, string &timeOut);

  void getIdTypes(vector<string> &types);
  void setPreferredIdType(const pair<string, string>&type);

  void readEventList(gdioutput &gdi, xmlobject &xo);

  /** Scan the entry list to find specification of stage numbers*/
  void prescanEntryList(xmlobject & xo, set<int>& definedStages);

  void readEntryList(gdioutput &gdi, xmlobject &xo, bool removeNonexisting, 
                     const set<int> &stageFilter, int &entRead, int &entFail, int &entRemoved);

  void readStartList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void readResultList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void readServiceRequestList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void readClassList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail);

  void prescanCompetitorList(xmlobject &xo);
  void readCompetitorList(gdioutput &gdi, const xmlobject &xo,
                          bool onlyWithClub, int &personCount, int& duplicateCount);

  void readClubList(gdioutput &gdi, const xmlobject &xo, int &clubCount);

  void readCourseData(gdioutput &gdi, const xmlobject &xo, bool updateClasses, int &courseCount, int &entFail);

  void writeResultList(xmlparser &xml, const set<int> &classes, int leg,
                       bool useUTC, bool teamsAsIndividual, 
                       bool unrollLoops, bool includeStageInfo,
                       bool withPartialResult);

  void writeStartList(xmlparser &xml, const set<int> &classes, bool useUTC, 
                      bool teamsAsIndividual, bool includeStageInfo);

  void writeEvent(xmlparser &xml);

  void writeCourses(xmlparser &xml);

  void writeRunnerDB(const RunnerDB &db, xmlparser &xml) const;

  void writeClubDB(const RunnerDB &db, xmlparser &xml) const;

  void writeForkings(xmlparser& xml) const;

};
