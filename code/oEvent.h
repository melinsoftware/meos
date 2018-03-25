// oEvent.h: interface for the oEvent class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OEVENT_H__CDA15578_CB62_4EAD_96B9_3037355F5D48__INCLUDED_)
#define AFX_OEVENT_H__CDA15578_CB62_4EAD_96B9_3037355F5D48__INCLUDED_

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


#include "oDataContainer.h"

#include "oControl.h"
#include "oCourse.h"
#include "oClass.h"
#include "oClub.h"
#include "oRunner.h"
#include "oCard.h"
#include "oFreePunch.h"
#include "oTeam.h"

#include "intkeymap.hpp"
#include <set>
#include <map>

#ifdef OLD
  #include <hash_set>
  #include <hash_map>

#define unordered_multimap stdext::hash_multimap 
#define unordered_map stdext::hash_map 
#define unordered_set stdext::hash_set 

#else
  #include <unordered_map>
  #include <unordered_set>
#endif

#define cVacantId 888888888
#define cNoClubId 999999999

class MeOSFileLock;
class RunnerDB;
class gdioutput;
class oDataContainer;
class oListInfo;
struct oListParam;
struct oPrintPost;
enum EStdListType;
class oFreeImport;
class oWordList;
class ClassConfigInfo;
enum EPostType;
struct SocketPunchInfo;
class DirectSocket;
template<class T, class KEY> class intkeymap;
class MeOSFeatures;
class GeneralResult;
class DynamicResult;
struct SpeakerString;
struct ClassDrawSpecification;
class ImportFormats;

struct oCounter {
  int level1;
  int level2;
  int level3;
  oCounter() : level1(0), level2(0), level3(0) {}
  void operator++() {level1++, level2++, level3++;}
};


struct GeneralResultCtr {
  wstring name;
  string tag;
  wstring fileSource;

  bool isDynamic() const;

  mutable GeneralResult *ptr;

  GeneralResultCtr(const char *tag, const wstring &name, GeneralResult *ptr);
  GeneralResultCtr(wstring &file, DynamicResult *ptr);
  GeneralResultCtr() : ptr(0) {}

  ~GeneralResultCtr();
  GeneralResultCtr(const GeneralResultCtr &ctr);
  void operator=(const GeneralResultCtr &ctr);
};

class oTimeLine {
public:
  enum TimeLineType {TLTStart, TLTFinish, TLTRadio, TLTExpected};
  enum Priority {PTop = 6, PHigh = 5, PMedium = 4, PLow = 3};
private:
  int time;
  wstring msg;
  wstring detail;
  TimeLineType type;
  Priority priority;
  pair<bool, int> typeId; //True if teamId, otherwise runnerId
  int classId;
  int ID;
public:

  oTimeLine &setMessage(const wstring &msg_) {msg = msg_; return *this;}
  oTimeLine &setDetail(const wstring &detail_) {detail = detail_; return *this;}

  const wstring &getMessage() const {return msg;}
  const wstring &getDetail() const {return detail;}

  int getTime() const {return time;}
  TimeLineType getType() const {return type;}
  Priority getPriority() const {return priority;}
  int getClassId() const {return classId;}
  pair<bool, int> getSource() const {return typeId;}
  oAbstractRunner *getSource(const oEvent &oe) const;

  __int64 getTag() const;
  oTimeLine(int time, TimeLineType type, Priority priority, int classId, int id, oAbstractRunner *source);
  virtual ~oTimeLine();
};

typedef multimap<int, oTimeLine> TimeLineMap;
typedef TimeLineMap::iterator TimeLineIterator;

typedef list<oControl> oControlList;
typedef list<oCourse> oCourseList;
typedef list<oClass> oClassList;
typedef list<oClub> oClubList;
typedef list<oRunner> oRunnerList;
typedef list<oCard> oCardList;
typedef list<oTeam> oTeamList;

typedef list<oFreePunch> oFreePunchList;

typedef int (*GUICALLBACK)(gdioutput *gdi, int type, void *data);

struct ClassInfo;
struct DrawInfo;

struct CompetitionInfo {
  int Id;
  wstring Name;
  wstring Annotation;
  wstring Date;
  wstring NameId;
  wstring FullPath;
  string Server;
  string ServerUser;
  string ServerPassword;
  string Modified;

  wstring url;
  wstring firstStart;
  wstring account;
  wstring lastNormalEntryDate;
  int ServerPort;
  int numConnected; // Number of connected entities
  int backupId; // Used to identify backups
  bool operator<(const CompetitionInfo &ci)
  {
    if (Date != ci.Date)
      return Date<ci.Date;
    else
      return Modified < ci.Modified;
  }
};

struct BackupInfo : public CompetitionInfo {
  int type;
  wstring fileName;
  size_t fileSize;
  bool operator<(const BackupInfo &ci);
};

class oListInfo;
class MetaListContainer;

typedef bool (__cdecl* ERRORMESG_FCN)(char *bf256);
typedef bool (__cdecl* OPENDB_FCN)(void);
typedef int  (__cdecl* SYNCHRONIZE_FCN)(oBase *obj);
typedef bool (__cdecl* SYNCHRONIZELIST_FCN)(oBase *obj, int lid);

enum oListId {oLRunnerId=1, oLClassId=2, oLCourseId=4,
              oLControlId=8, oLClubId=16, oLCardId=32,
              oLPunchId=64, oLTeamId=128, oLEventId=256};


class Table;
class oEvent;
typedef oEvent * pEvent;
struct TableUpdateInfo;

struct oListParam;
class ProgressWindow;
struct PlaceRunner;
typedef multimap<int, PlaceRunner> TempResultMap;
struct TimeRunner;

struct PrintPostInfo;

enum PropertyType {
  String,
  Integer,
  Boolean
};

class oEvent : public oBase
{
private:
  oDataDefiner *firstStartDefiner;
  oDataDefiner *intervalDefiner;

protected:
  // Revision number for data modified on this client.
  unsigned long dataRevision;

  // Set to true if a global modification is made that should case all lists etc to regenerate.
  bool globalModification;

  gdioutput &gdibase;
  HMODULE hMod;//meosdb.dll

  void generateFixedList(gdioutput &gdi, const oListInfo &li);

  void startReconnectDaemon();
  
  mutable int vacantId; //Cached vacant id
  mutable int noClubId; //Cached no club id

  wstring Name;
  wstring Annotation;
  wstring Date;
  DWORD ZeroTime;

  mutable map<wstring, wstring> date2LocalTZ;
  const wstring &getTimeZoneString() const;

  int tCurrencyFactor;
  wstring tCurrencySymbol;
  wstring tCurrencySeparator;
  bool tCurrencyPreSymbol;

  int tMaxTime;

  bool writeControls(xmlparser &xml);
  bool writeCourses(xmlparser &xml);
  bool writeClasses(xmlparser &xml);
  bool writeClubs(xmlparser &xml);
  bool writeRunners(xmlparser &xml, ProgressWindow &pw);
  //Write free cards not bound to runner
  bool writeCards(xmlparser &xml);
  bool writePunches(xmlparser &xml, ProgressWindow &pw);
  bool writeTeams(xmlparser &xml);

  oControlList Controls;
  oCourseList Courses;
  intkeymap<pCourse> courseIdIndex;
  oClassList Classes;
  oClubList Clubs;
  intkeymap<pClub> clubIdIndex;

  oRunnerList Runners;
  intkeymap<pRunner> runnerById;

  RunnerDB *runnerDB;
  MeOSFeatures *meosFeatures;

  oCardList Cards;

  oFreePunchList punches;
  typedef unordered_multimap<int, pFreePunch> PunchIndexType;
  typedef PunchIndexType::iterator PunchIterator;
  typedef PunchIndexType::const_iterator PunchConstIterator;
  /** First level maps a constant based on control number
      and index on course to a second maps, that maps cardNo to punches. */
  map<int, PunchIndexType> punchIndex;

  oTeamList Teams;
  intkeymap<pTeam> teamById;

  oDataContainer *oEventData;
  oDataContainer *oControlData;
  oDataContainer *oCourseData;
  oDataContainer *oClassData;
  oDataContainer *oClubData;
  oDataContainer *oRunnerData;
  oDataContainer *oTeamData;

  string sqlUpdateRunners;
  string sqlUpdateClasses;
  string sqlUpdateCourses;
  string sqlUpdateControls;
  string sqlUpdateClubs;
  string sqlUpdateCards;
  string sqlUpdatePunches;
  string sqlUpdateTeams;

  int sqlCounterRunners;
  int sqlCounterClasses;
  int sqlCounterCourses;
  int sqlCounterControls;
  int sqlCounterClubs;
  int sqlCounterCards;
  int sqlCounterPunches;
  int sqlCounterTeams;

  bool sqlChangedRunners;
  bool sqlChangedClasses;
  bool sqlChangedCourses;
  bool sqlChangedControls;
  bool sqlChangedClubs;
  bool sqlChangedCards;
  bool sqlChangedPunches;
  bool sqlChangedTeams;


  bool needReEvaluate();

  DirectSocket *directSocket;

  int getFreeRunnerId();
  int getFreeClassId();
  int getFreeCourseId();
  int getFreeControlId();
  int getFreeClubId();
  int getFreeCardId();
  int getFreePunchId();
  int getFreeTeamId();

  int qFreeRunnerId;
  int qFreeClassId;
  int qFreeCourseId;
  int qFreeControlId;
  int qFreeClubId;
  int qFreeCardId;
  int qFreePunchId;
  int qFreeTeamId;

  int nextFreeStartNo;
  void updateFreeId();
  void updateFreeId(oBase *ob);

  SortOrder CurrentSortOrder;

  list<CompetitionInfo> cinfo;
  list<BackupInfo> backupInfo;
  mutable map<wstring, ClassMetaType> classTypeNameToType;

  MetaListContainer *listContainer;
  wchar_t CurrentFile[260];
  wstring currentNameId;

  static int dbVersion;
  string MySQLServer;
  string MySQLUser;
  string MySQLPassword;
  int MySQLPort;

  string serverName;//Verified (connected) server name.

  MeOSFileLock *openFileLock;

  bool HasDBConnection;
  bool HasPendingDBConnection;
  bool msSynchronize(oBase *ob);
  void resetChangeStatus(bool onlyChangable=true);
  void storeChangeStatus(bool onlyChangable=true);

  wstring clientName;
  vector<string> connectedClients;
  DWORD clientCheckSum() const; //Calculate a check sum for current clients
  DWORD currentClientCS; //The current, stored check sum.

  //Protected speaker functions.
  int computerTime;

  multimap<int, oTimeLine> timeLineEvents;
  int timeLineRevision;
  set<int> timelineClasses;
  set<int> modifiedClasses;

  static const int dataSize = 1024;
  int getDISize() const {return dataSize;}
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];
  vector< vector<string> > dynamicData;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  //Precalculated. Used in list processing.
  vector<int> currentSplitTimes;

  void initProperties();

  map<string, wstring> eventProperties;
  map<string, wstring> savedProperties;
  
  bool tUseStartSeconds;

  set< pair<int,int> > readPunchHash;
  void insertIntoPunchHash(int card, int code, int time);
  void removeFromPunchHash(int card, int code, int time);
  bool isInPunchHash(int card, int code, int time);

  void generateStatisticsPart(gdioutput &gdi, const vector<ClassMetaType> &type,
                              const set<int> &feeLimit, int actualFee, bool useReducedFee,
                              int baseFee, int &entries_sum, int &started_sum, int &fee_sum) const;
  void getRunnersPerDistrict(vector<int> &runners) const;
  void getDistricts(vector<string> &district);

  void autoAddTeam(pRunner pr);
  void autoRemoveTeam(pRunner pr);

  void exportIOFEventList(xmlparser &xml);
  void exportIOFEvent(xmlparser &xml);
  void exportIOFClass(xmlparser &xml);
  void exportIOFStartlist(xmlparser &xml);
  void exportIOFClublist(xmlparser &xml);

  void exportIOFResults(xmlparser &xml, bool selfContained, const set<int> &classes, int leg, bool oldStylePatrol);
  void exportTeamSplits(xmlparser &xml, const set<int> &classes, bool oldStylePatrol);

  /** Set up transient data in classes */
  void reinitializeClasses();

  /** Analyze the result status of each class*/
  void analyzeClassResultStatus() const;

  /// Implementation versions
  int setupTimeLineEvents(int classId, int currentTime);
  int setupTimeLineEvents(vector<pRunner> &started, const vector< pair<int, pControl> > &rc, int currentTime, bool finish);
  void timeLinePrognose(TempResultMap &result, TimeRunner &tr, int prelT,
                        int radioNumber, const wstring &rname, int radioId);
  int nextTimeLineEvent; // Time when next known event will occur.

  // Tables
  map<string, Table *> tables;

  // Internal list method
  void generateListInternal(gdioutput &gdi, const oListInfo &li, bool formatHead);

  /** Format a string for a list. */
  const wstring &formatListStringAux(const oPrintPost &pp, const oListParam &par,
                                    const pTeam t, const pRunner r, const pClub c,
                                    const pClass pc, oCounter &counter) const;

  /** Format a string that does not depend on team or runner*/
  const wstring &formatSpecialStringAux(const oPrintPost &pp, const oListParam &par,
                                        const pTeam t, int legIndex,                                   
                                        const pCourse pc, const pControl ctrl,  
                                        oCounter &counter) const;
  void changedObject();

  mutable vector<GeneralResultCtr> generalResults;

  // Temporarily disable recaluclate leader times
  bool disableRecalculate;
public:
  
  int getVacantClub(bool returnNoClubClub); // Create vacant club if it does not exist
  int getVacantClubIfExist(bool returnNoClubClub) const;

  enum NameMode {
    FirstLast,
    LastFirst,
    Raw,
  };

private:
  NameMode currentNameMode;

public:
  NameMode getNameMode() const {return currentNameMode;};
  NameMode setNameMode(NameMode newNameMode);

  /// Get new punches since firstTime
  void getLatestPunches(int firstTime, vector<const oFreePunch *> &punches) const;

  void resetSQLChanged(bool resetAllTeamsRunners, bool cleanClasses);

  void pushDirectChange();

  void getPayModes(vector< pair<wstring, size_t> > &modes);
  void setPayMode(int id, const wstring &mode);

  bool hasDirectSocket() const {return directSocket != 0;}
  DirectSocket &getDirectSocket();

  bool advancePunchInformation(const vector<gdioutput *> &gdi, vector<SocketPunchInfo> &pi,
                               bool getPunch, bool getFinish);

  // Sets and returns extra lines (string, style) to be printed on the split print, invoice, ...
  void setExtraLines(const char *attrib, const vector<pair<wstring, int> > &lines);
  void getExtraLines(const char *attrib, vector<pair<wstring, int> > &lines) const;

  RunnerDB &getRunnerDatabase() const {return *runnerDB;}

  MeOSFeatures &getMeOSFeatures() const {return *meosFeatures;}
  void getDBRunnersInEvent(intkeymap<pClass, __int64> &runners) const;
  MetaListContainer &getListContainer() const;
  wstring getNameId(int id) const;
  const wstring &getFileNameFromId(int id) const;

  // Adjust team size to class size and create multi runners.
  void adjustTeamMultiRunners(pClass cls);

  //Get list of runners in a class
  void getRunners(int classId, int courseId, vector<pRunner> &r, bool sortRunners = true);
  void getRunnersByCard(int cardNo, vector<pRunner> &r);

  void getTeams(int classId, vector<pTeam> &t, bool sortTeams = true);

  bool hasRank() const;
  bool hasBib(bool runnerBib, bool teamBib) const;
  bool hasTeam() const;

  /// Speaker timeline
  int setupTimeLineEvents(int currentTime);
  void renderTimeLineEvents(gdioutput &gdi) const;
  int getTimeLineEvents(const set<int> &classes, vector<oTimeLine> &events,
                        set<__int64> &stored, int currentTime);
  /// Notification that a class has been changed. If only a punch changed
  void classChanged(pClass cls, bool punchOnly);

  // Rogaining
  bool hasRogaining() const;

  // Maximal time
  wstring getMaximalTimeS() const;
  int getMaximalTime() const;
  void setMaximalTime(const wstring &time);

  void saveProperties(const wchar_t *file);
  void loadProperties(const wchar_t *file);

  /** Get number of classes*/
  int getNumClasses() const {return Classes.size();}

  /** Get number of runners */
  int getNumRunners() const {return runnerById.size();}


  // Show an warning dialog if database is not sane
  void sanityCheck(gdioutput &gdi, bool expectResult, int checkOnlyClass = -1);

  // Automatic draw of all classes
  void automaticDrawAll(gdioutput &gdi, const wstring &firstStart,
                        const wstring &minIntervall, const wstring &vacances,
                        bool lateBefore, bool softMethod, int pairSize);

  // Restore a backup by renamning the file to .meos
  void restoreBackup();

  void generateVacancyList(gdioutput &gdi, GUICALLBACK callBack);

  // Returns true if there is a multi runner class.
  bool hasMultiRunner() const;

  void updateTabs(bool force = false, bool hide = false) const;
  bool useRunnerDb() const;
  void useRunnerDb(bool use);

  int getFirstClassId(bool teamClass) const;

  void generateCompetitionReport(gdioutput &gdi);


  // To file if n > 10.
  enum InvoicePrintType {IPTAllPrint=1, IPTAllHTML=11, IPTNoMailPrint=2,
                         IPTNonAcceptedPrint=3, IPTElectronincHTML=12, IPTAllPDF=13};

  void printInvoices(gdioutput &gdi, InvoicePrintType type,
                     const wstring &basePath, bool onlySummary);
  void selectRunners(const wstring &classType, int lowAge,
                     int highAge, const wstring &firstDate,
                     const wstring &lastDate, bool includeWithFee,
                     vector<pRunner> &output) const;

  void applyEventFees(bool updateClassFromEvent,
                      bool updateFees, bool updateCardFees,
                      const set<int> &classFilter);

  void listConnectedClients(gdioutput &gdi);
  void validateClients();
  bool hasClientChanged() const;

  enum PredefinedTypes {PNoSettings, PPool, PForking, PPoolDrawn, PHunting,
                 PPatrol, PPatrolOptional, PPatrolOneSI, PRelay, PTwinRelay,
                 PYouthRelay, PNoMulti};

  void fillPredefinedCmp(gdioutput &gdi, const string &name) const;

  // Sets up class and ajust multirunner in teams and synchronizes.
  void setupRelay(oClass &cls, PredefinedTypes type,
                  int nleg, const wstring &start);
  void setupRelayInfo(PredefinedTypes type,
                      bool &useNLeg, bool &useNStart);

  void fillLegNumbers(const set<int> &cls, bool isTeamList, 
                      bool includeSubLegs, vector< pair<wstring, size_t> > &out);

  void reCalculateLeaderTimes(int classId);

  void testFreeImport(gdioutput &gdi);
  void getFreeImporter(oFreeImport &fi);
  void init(oFreeImport &fi);


  void calculateSplitResults(int controlIdFrom, int controlIdTo);
  // Get total number of completed runner for given class and leg.
  void getNumClassRunners(int id, int leg, int &total, int &finished, int &dns) const;

  pTeam findTeam(const wstring &s, int lastId, unordered_set<int> &filter) const;
  pRunner findRunner(const wstring &s, int lastId, const unordered_set<int> &inputFilter, unordered_set<int> &filter) const;

  static const wstring &formatStatus(RunnerStatus status);

  inline bool useStartSeconds() const {return tUseStartSeconds;}
  void calcUseStartSeconds();

  void assignCardInteractive(gdioutput &gdi, GUICALLBACK cb);

  int getPropertyInt(const char *name, int def);
  const string &getPropertyString(const char *name, const string &def);
  const wstring &getPropertyString(const char *name, const wstring &def);
  
  wstring getPropertyStringDecrypt(const char *name, const string &def);

  void setProperty(const char *name, int prop);
  //void setProperty(const char *name, const string &prop);
  void setProperty(const char *name, const wstring &prop);
  
  void setPropertyEncrypt(const char *name, const string &prop);

  void listProperties(bool userProps, vector< pair<string, PropertyType> > &propNames) const;

  // Get classes that have not yet been drawn.
  // someMissing (true = all classes where some runner has no start time)
  //            (false = all classeds where no runner has a start time)
  void getNotDrawnClasses(set<int> &classes, bool someMissing);
  void getAllClasses(set<int> &classes);
  bool deleteCompetition();

  void clear();

  // Drop the open database.
  void dropDatabase();
  bool connectToMySQL(const string &server, const string &user,
                      const string &pwd, int port);
  bool connectToServer();
  bool reConnect(char *errorMsg256);
  void closeDBConnection();

  const string &getServerName() const;

  // Upload competition to server
  bool uploadSynchronize();
  // Download competition from server
  bool readSynchronize(const CompetitionInfo &ci);

  void playPrewarningSounds(const wstring &basedir, set<int> &controls);
  void clearPrewarningSounds();
  void tryPrewarningSounds(const wstring &basedir, int number);

  int getFreeStartNo() const;
  void generatePreReport(gdioutput &gdi);

  void generateList(gdioutput &gdi, bool reEvaluate, const oListInfo &li, bool updateScrollBars);
  
  void generateListInfo(oListParam &par, int lineHeight, oListInfo &li);
  void generateListInfo(vector<oListParam> &par, int lineHeight, oListInfo &li);
  void generateListInfo(EStdListType lt, const gdioutput &gdi, int classId, oListInfo &li);
  void generateListInfoAux(oListParam &par, int lineHeight, oListInfo &li, const wstring &name);

  /** Format a string for a list. Returns true of output is not empty*/
  const wstring &formatListString(const oPrintPost &pp, const oListParam &par,
                                 const pTeam t, const pRunner r, const pClub c,
                                 const pClass pc, oCounter &counter) const;

  const wstring &formatSpecialString(const oPrintPost &pp, const oListParam &par,
                                    const pTeam t, int legIndex,
                                    const pCourse crs, const pControl ctrl, oCounter &counter) const;

  void calculatePrintPostKey(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                             const pTeam t, const pRunner r, const pClub c,
                             const pClass pc, oCounter &counter, wstring &key);
  const wstring &formatListString(EPostType type, const pRunner r) const;
  const wstring &formatListString(EPostType type, const pRunner r, const wstring &format) const;

  

 /** Format a print post. Returns true of output is not empty*/
  bool formatPrintPost(const list<oPrintPost> &ppli, PrintPostInfo &ppi, 
                       const pTeam t, const pRunner r, const pClub c,
                       const pClass pc, const pCourse crs, const pControl ctrl, int legIndex);
  void listGeneratePunches(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                           pTeam t, pRunner r, pClub club, pClass cls);
  void getListTypes(map<EStdListType, oListInfo> &listMap, int filter);
  void getListType(EStdListType type, oListInfo &li);

  void fillListTypes(gdioutput &gdi, const string &name, int filter);


  void checkOrderIdMultipleCourses(int ClassId);

  void addBib(int ClassId, int leg, const wstring &firstNumber);
  void addAutoBib();

  //Speaker functions.
  void speakerList(gdioutput &gdi, int classId, int leg, int controlId,
                   int previousControlId, bool totalResults, bool shortNames);
  int getComputerTime() const {return (computerTime+500)/1000;}
  int getComputerTimeMS() const {return computerTime;}

  void updateComputerTime();

  // Get set of controls with registered punches
  void getFreeControls(set<int> &controlId) const;
  // Returns the added punch, of null of already added.
  pFreePunch addFreePunch(int time, int type, int card, bool updateRunner);
  pFreePunch addFreePunch(oFreePunch &fp);

  bool useLongTimes() const;
  void useLongTimes(bool use);

  /** Use the current computer time to convert the specified time to a long time, if long times are used. */
  int convertToFullTime(int inTime);

  /** Internal version of start order optimizer */
  void optimizeStartOrder(vector< vector<pair<int, int> > > &StartField, DrawInfo &drawInfo,
                          vector<ClassInfo> &cInfo, int useNControls, int alteration);

  struct ResultEvent {
    ResultEvent() {}
    ResultEvent(pRunner r, int time, int control, RunnerStatus status):
        r(r), time(time), control(control), status(status), 
        resultScore(0), place(-1), runTime(0), partialCount(0), legNumber(short(r->tLeg)) {}
    
    pRunner r;
    int time;
    int control;
    RunnerStatus status;

    int localIndex;
    int resultScore;
    int runTime;
    unsigned short place;
    /* By default zero. Used for parallel results etc.
      -1 : Ignore
      1,2,3 (how many runners are missing on the leg)
    */
    short partialCount;
    short legNumber;

    inline int classId() const {return r->getClassId(true);}
    inline int leg() const {return legNumber;}
  };

  void getResultEvents(const set<int> &classFilter, const set<int> &controlFilter, vector<ResultEvent> &results) const;
protected:
  // Returns hash key for punch based on control id, and leg. Class is marked as changed if oldHashKey != newHashKey.
  int getControlIdFromPunch(int time, int type, int card,
                            bool markClassChanged, oFreePunch &punch);

  static void drawSOFTMethod(vector<pRunner> &runners, bool handleBlanks=true);
  bool enumerateBackups(const wstring &file, const wstring &filetype, int type);
  unordered_multimap<int, pRunner> cardHash;
  mutable multimap<int, oAbstractRunner*> bibStartNoToRunnerTeam;
  int tClubDataRevision;
  bool readOnly;
  mutable int tLongTimesCached;
  mutable map<int, pair<int, int> > cachedFirstStart; //First start per classid.
  map<pair<int, int>, oFreePunch> advanceInformationPunches;

public:
  void updateStartTimes(int delta);

  void useDefaultProperties(bool useDefault);

  bool isReadOnly() const {return readOnly;}
  void setReadOnly() {readOnly = true;}

  enum IOFVersion {IOF20, IOF30};

  void setCurrency(int factor, const wstring &symbol, const wstring &separator, bool preSymbol);
  wstring formatCurrency(int c, bool includeSymbol = true) const;
  int interpretCurrency(const wstring &c) const;
  int interpretCurrency(double val, const wstring &cur);

  void setupClubInfoData(); //Precalculate temporary data in club object

  // Clears map if clear is true. A cleared map is not used. Map must be cleared after use, is not updated.
  void setupCardHash(bool clear = false);

  void remove();
  bool canRemove() const;

  /// Return revision number for current data
  long getRevision() const {return dataRevision;}

  /// Calculate total missed time and other statistics for each control
  void setupControlStatistics() const;

  // Get information on classes
  void getClassConfigurationInfo(ClassConfigInfo &cnf) const;

  /** Add numbers to make team names unique */
  void makeUniqueTeamNames();

  void removeVacanies(int classId);

  wstring getInfo() const {return Name;}
  bool verifyConnection();
  bool isClient() const {return HasDBConnection;}
  const wstring &getClientName() const {return clientName;}
  void setClientName(const wstring &n) {clientName=n;}

  void removeFreePunch(int id);
  pFreePunch getPunch(int id) const;
  pFreePunch getPunch(int runnerId, int courseControlId, int card) const;
  void getPunchesForRunner(int runnerId, vector<pFreePunch> &punches) const;

  //Returns true if data is changed.
  bool autoSynchronizeLists(bool syncPunches);
  bool synchronizeList(oListId id, bool preSyncEvent = true, bool postSyncEvent = true);

  void generateInForestList(gdioutput &gdi, GUICALLBACK cb,
                            GUICALLBACK cb_nostart);

  pRunner dbLookUpById(__int64 extId) const;
  pRunner dbLookUpByCard(int CardNo) const;
  pRunner dbLookUpByName(const wstring &name, int clubId,
                         int classId, int birthYear) const;

  void updateRunnerDatabase();
  void updateRunnerDatabase(pRunner r, map<int, int> &clubIdMap);

  /** Returns the first start in a class */
  int getFirstStart(int classId = 0) const;
  void convertTimes(pRunner runner, SICard &sic) const;

  pCard getCard(int Id) const;
  pCard getCardByNumber(int cno) const;
  bool isCardRead(const SICard &card) const;
  void getCards(vector<pCard> &cards);

  /** Try to find the class that best matches the card.
      Negative return = missing controls
      Positve return = extra controls
      Zero = exact match */
  int findBestClass(const SICard &card, vector<pClass> &classes) const;
  wstring getCurrentTimeS() const;

  //void reEvaluateClass(const set<int> &classId, bool doSync);
  void reEvaluateCourse(int courseId, bool doSync);
  void reEvaluateAll(const set<int> &classId, bool doSync);
  void reEvaluateChanged();

  void exportIOFSplits(IOFVersion version, const wchar_t *file, bool oldStylePatrolExport,
                       bool useUTC,
                       const set<int> &classes,
                       int leg,
                       bool teamsAsIndividual,
                       bool unrollLoops,
                       bool includeStageData,
                       bool forceSplitFee);

  void exportIOFStartlist(IOFVersion version, const wchar_t *file,
                          bool useUTC, const set<int> &classes,
                          bool teamsAsIndividual,
                          bool includeStageInfo,
                          bool forceSplitFee);

  bool exportOECSV(const wchar_t *file, int LanguageTypeIndex, bool includeSplits);
  bool save();
  void duplicate();
  void newCompetition(const wstring &Name);
  void clearListedCmp();
  bool enumerateCompetitions(const wchar_t *path, const wchar_t *extension);

  bool fillCompetitions(gdioutput &gdi, const string &name,
                        int type, const wstring &select = L"");

  bool enumerateBackups(const wstring &path);
  bool listBackups(gdioutput &gdi, GUICALLBACK cb);
  const BackupInfo &getBackup(int id) const;
  void deleteBackups(const BackupInfo &bu);

  // Check if competition is empty
  bool empty() const;

  void generateMinuteStartlist(gdioutput &gdi);
  
  bool classHasTeams(int Id) const;
  bool classHasResults(int Id) const;
  bool isCourseUsed(int Id) const;
  bool isClassUsed(int Id) const;
  bool isControlUsed(int Id) const;
  bool isRunnerUsed(int Id) const;
  bool isClubUsed(int Id) const;

  void removeRunner(const vector<int> &Ids);
  void removeCourse(int Id);
  void removeClass(int Id);
  void removeControl(int Id);
  void removeTeam(int Id);
  void removeClub(int Id);
  void removeCard(int Id);

  /// Convert a clock time string to time relative zero time
  int getRelativeTime(const wstring &absoluteTime) const;
  //int getRelativeTime(const string &absoluteTime) const {return getRelativeTime(toWide(absoluteTime));}

  /// Convert a clock time string to time relative zero time
  int getRelativeTime(const string &date, const string &absoluteTime, const string &timeZone) const;

  // Convert a clock time string (SI5 12 Hour clock) to time relative zero time
  //int getRelativeTimeFrom12Hour(const wstring &absoluteTime) const;

  /// Convert c clock time string to absolute time (after 00:00:00)
  static int convertAbsoluteTime(const string &m);
  static int convertAbsoluteTime(const wstring &m);

  /// Get clock time from relative time
  const wstring &getAbsTime(DWORD relativeTime) const;
  
  wstring getAbsDateTimeISO(DWORD relativeTime, bool includeDate, bool useGMT) const;

  const wstring &getAbsTimeHM(DWORD relativeTime) const;

  const wstring &getName() const;
  wstring getTitleName() const;
  void setName(const wstring &m);

  const wstring &getAnnotation() const {return Annotation;}
  void setAnnotation(const wstring &m);

  const wstring &getDate() const {return Date;}
    
  void setDate(const wstring &m);

  int getZeroTimeNum() const {return ZeroTime;}
  wstring getZeroTime() const;
  
  void setZeroTime(wstring m);

  /** Get the automatic bib gap between classes. */
  int getBibClassGap() const;
  
  /** Set the automatic bib gap between classes. */
  void setBibClassGap(int numStages);

  bool openRunnerDatabase(const wchar_t *file);
  bool saveRunnerDatabase(const wchar_t *file, bool onlyLocal);

  enum ResultType {RTClassResult, RTTotalResult, RTCourseResult, RTClassCourseResult};
  void calculateResults(ResultType result, bool includePreliminary = false);
  void calculateRogainingResults(const set<int> &classSelection);

  void calculateResults(list<oSpeakerObject> &rl);
  void calculateTeamResults(bool totalMultiday);
  bool calculateTeamResults(int leg, bool totalMultiday);
  // Set results for specified classes to tempResult
  void calculateTeamResultAtControl(const set<int> &classId, int leg, int controlId, bool totalResults);

  bool sortRunners(SortOrder so);
  /** If linear leg is true, leg is interpreted as actual leg numer, otherwise w.r.t to parallel legs. */
  bool sortTeams(SortOrder so, int leg, bool linearLeg);

  pCard allocateCard(pRunner owner);

  /** Optimize the start order based on drawInfo. Result in cInfo */
  void optimizeStartOrder(gdioutput &gdi, DrawInfo &drawInfo, vector<ClassInfo> &cInfo);

  void loadDrawSettings(const set<int> &classes, DrawInfo &drawInfo, vector<ClassInfo> &cInfo) const;

  enum DrawType {
    drawAll, remainingBefore, remainingAfter,
  };

  void drawRemaining(bool useSOFTMethod, bool placeAfter);
  void drawList(const vector<ClassDrawSpecification> &spec,
                bool useSOFTMethod, int pairSize, DrawType drawType);
  void drawListClumped(int classID, int firstStart, int interval, int vacances);
  void drawPersuitList(int classId, int firstTime, int restartTime,
                       int ropeTime, int interval, int pairSize,
                       bool reverse, double scale);

  wstring getAutoTeamName() const;
  pTeam addTeam(const oTeam &t, bool autoAssignStartNo);
  pTeam addTeam(const wstring &pname, int clubId=0, int classId=0);
  pTeam getTeam(int Id) const;
  pTeam getTeamByName(const wstring &pname) const;
  const vector< pair<wstring, size_t> > &fillTeams(vector< pair<wstring, size_t> > &out, int classId=0);
  const vector< pair<wstring, size_t> > &fillStatus(vector< pair<wstring, size_t> > &out);
  const vector< pair<wstring, size_t> > &fillControlStatus(vector< pair<wstring, size_t> > &out) const;

  void fillTeams(gdioutput &gdi, const string &id, int ClassId=0);
  void fillStatus(gdioutput &gdi, const string &id);
  void fillControlStatus(gdioutput &gdi, const string &id) const;


  wstring getAutoRunnerName() const;
  
  pRunner addRunner(const wstring &pname, int clubId, int classId,
                    int cardNo, int birthYear, bool autoAdd);

  pRunner addRunner(const wstring &pname, const wstring &pclub, int classId,
                    int cardNo, int birthYear, bool autoAdd);

  pRunner addRunnerFromDB(const pRunner db_r, int classId, bool autoAdd);
  pRunner addRunner(const oRunner &r, bool updateStartNo);
  pRunner addRunnerVacant(int classId);

  pRunner getRunner(int Id, int stage) const;
  /** Get a competitor by cardNo.
      @param cardNo card number to look for.
      @param time if non-zero, try to find a runner actually running on the specified time, if there are multiple runners using the same card.
      @param onlyRunnerWithNoCard returns only a runner that has no card tied.
      @param ignoreRunnersWithNoStart If true, never return a runner with status NoStart
      @return runner of null.
   */
  pRunner getRunnerByCardNo(int cardNo, int time,
                            bool onlyRunnerWithNoCard = false,
                            bool ignoreRunnersWithNoStart = false) const;
  /** Get all competitors for a cardNo.
      @param cardNo card number to look for.
      @param ignoreRunnersWithNoStart If true, skip runners with status DNS
      @param skipDuplicates if true, only return the main instance of each runner (if several races)
      @param out runners using the card
   */
  void getRunnersByCardNo(int cardNo, bool ignoreRunnersWithNoStart, 
                          bool skipDuplicates, vector<pRunner> &out) const;
  /** Finds a runner by start number (linear search). If several runners has same bib/number try to get the right one:
       findWithoutCardNo false : find first that has not finished
       findWithoutCardNo true : find first with no card.
  */
  pRunner getRunnerByBibOrStartNo(const wstring &bib, bool findWithoutCardNo) const;

  pRunner getRunnerByName(const wstring &pname, const wstring &pclub = L"") const;

  enum FillRunnerFilter {RunnerFilterShowAll = 1,
                         RunnerFilterOnlyNoResult = 2,
                         RunnerFilterWithResult = 4,
                         RunnerCompactMode = 8};

  const vector< pair<wstring, size_t> > &fillRunners(vector< pair<wstring, size_t> > &out,
                                                    bool longName, int filter,
                                                    const unordered_set<int> &personFilter);
  void fillRunners(gdioutput &gdi, const string &id, bool longName = false, int filter = 0);

  Table *getRunnersTB();//Table mode
  Table *getClubsTB();
  Table *getPunchesTB();
  Table *getClassTB();
  Table *getControlTB();
  Table *getCardsTB();
  Table *getTeamsTB();
  Table *getCoursesTB();


  void generateTableData(const string &tname, Table &table, TableUpdateInfo &ui);

  void generateControlTableData(Table &table, oControl *addControl);
  void generateRunnerTableData(Table &table, oRunner *addRunner);
  void generateClassTableData(Table &table, oClass *addClass);
  void generateCardTableData(Table &table, oCard *addCard);
  void generateClubTableData(Table &table, oClub *club);
  void generatePunchTableData(Table &table, oFreePunch *punch);

  void generateCourseTableData(Table &table, oCourse *course);
  void generateTeamTableData(Table &table, oTeam *team);

  pClub addClub(const wstring &pname, int createId=0);
  pClub addClub(const oClub &oc);

  void getClubRunners(int clubId, vector<pRunner> &runners) const;
  void getClubTeams(int clubId, vector<pTeam> &teams) const;

  //Get club, first by id then by name, and create if it does not exist
  pClub getClubCreate(int Id, const wstring &createName);

  void mergeClub(int clubIdPri, int clubIdSec);
  pClub getClub(int Id) const;
  pClub getClub(const wstring &pname) const;
  
  const vector< pair<wstring, size_t> > &fillClubs(vector< pair<wstring, size_t> > &out);
  void fillClubs(gdioutput &gdi, const string &id);
  void getClubs(vector<pClub> &c, bool sort);

  void viewClubMembers(gdioutput &gdi, int clubId);

  void updateClubsFromDB();
  void updateRunnersFromDB();

  void fillFees(gdioutput &gdi, const string &name, bool withAuto) const;
  wstring getAutoClassName() const;
  pClass addClass(const wstring &pname, int CourseId = 0, int classId = 0);
  pClass addClass(oClass &c);
  pClass getClassCreate(int Id, const wstring &createName);
  pClass getClass(const wstring &name) const;
  void getClasses(vector<pClass> &classes, bool sync) const;
  pClass getBestClassMatch(const wstring &name) const;
  bool getClassesFromBirthYear(int year, PersonSex sex, vector<int> &classes) const;
  pClass getClass(int Id) const;
  
  void getStartBlocks(vector<int> &blocks, vector<wstring> &starts) const;

  enum ClassFilter {
    filterNone,
    filterOnlyMulti,
    filterOnlySingle,
    filterOnlyDirect,
  };

  enum ClassExtra {
    extraNone,
    extraDrawn,
    extraNumMaps,
  };

  const vector< pair<wstring, size_t> > &fillClasses(vector< pair<wstring, size_t> > &out,
                    ClassExtra extended, ClassFilter filter);
  void fillClasses(gdioutput &gdi, const string &id, ClassExtra extended, ClassFilter filter);

  bool fillClassesTB(gdioutput &gdi);
  const vector< pair<wstring, size_t> > &fillStarts(vector< pair<wstring, size_t> > &out);
  const vector< pair<wstring, size_t> > &fillClassTypes(vector< pair<wstring, size_t> > &out);
  void fillStarts(gdioutput &gdi, const string &id);
  void fillClassTypes(gdioutput &gdi, const string &id);

  wstring getAutoCourseName() const;
  pCourse addCourse(const wstring &pname, int plength = 0, int id = 0);
  pCourse addCourse(const oCourse &oc);

  pCourse getCourseCreate(int Id);
  pCourse getCourse(const wstring &name) const;
  pCourse getCourse(int Id) const;

  void getCourses(vector<pCourse> &courses) const;
  /** Get controls. If calculateCourseControls, duplicate numbers are calculated for each control and course. */
  void getControls(vector<pControl> &controls, bool calculateCourseControls) const;

  void fillCourses(gdioutput &gdi, const string &id, bool simple = false);
  const vector< pair<wstring, size_t> > &fillCourses(vector< pair<wstring, size_t> > &out, bool simple = false);

  void calculateNumRemainingMaps();

  pControl getControl(int Id) const;
  pControl getControl(int Id, bool create);
  enum ControlType {CTAll, CTRealControl, CTCourseControl};

  const vector< pair<wstring, size_t> > &fillControls(vector< pair<wstring, size_t> > &out, ControlType type);
  const vector< pair<wstring, size_t> > &fillControlTypes(vector< pair<wstring, size_t> > &out);

  bool open(int id);
  bool open(const wstring &file, bool import=false);
  bool open(const xmlparser &xml);

  bool save(const wstring &file);
  pControl addControl(int id, int number, const wstring &name);
  pControl addControl(const oControl &oc);
  int getNextControlNumber() const;

  pCard addCard(const oCard &oc);

  /** Import entry data */
  void importXML_EntryData(gdioutput &gdi, const wstring &file, 
                           bool updateClass, bool removeNonexisting,
                           const set<int> &filter, const string &preferredIdType);

protected:
  pClass getXMLClass(const xmlobject &xentry);
  pClub getClubCreate(int clubId);

  bool addXMLCompetitorDB(const xmlobject &xentry, int ClubId);
  bool addOECSVCompetitorDB(const vector<wstring> &row);
  pRunner addXMLPerson(const xmlobject &person);
  pRunner addXMLStart(const xmlobject &xstart, pClass cls);
  pRunner addXMLEntry(const xmlobject &xentry, int ClubId, bool setClass);
  bool addXMLTeamEntry(const xmlobject &xentry, int ClubId);
  bool addXMLClass(const xmlobject &xclub);
  bool addXMLClub(const xmlobject &xclub, bool importToDB);
  bool addXMLRank(const xmlobject &xrank, map<__int64, int> &externIdToRunnerId);
  bool addXMLEvent(const xmlobject &xevent);

  bool addXMLCourse(const xmlobject &xcourse, bool addClasses);
  /** type: 0 control, 1 start, 2 finish*/
  bool addXMLControl(const xmlobject &xcontrol, int type);

public:

  GeneralResult &getGeneralResult(const string &tag, wstring &sourceFileOut) const;
  void getGeneralResults(bool onlyEditable, vector< pair<int, pair<string, wstring> > > &tagNameList, bool includeDateInName) const;
  void loadGeneralResults(bool forceReload) const;

  void getPredefinedClassTypes(map<wstring, ClassMetaType> &types) const;

  wstring cloneCompetition(bool cloneRunners, bool cloneTimes,
                          bool cloneCourses, bool cloneResult, bool addToDate);

  enum ChangedClassMethod {
    ChangeClassVacant,
    ChangeClass,
    TransferNoResult,
    TransferAnyway,
  };

  void transferResult(oEvent &ce,
                      const set<int> &allowNewEntries,
                      ChangedClassMethod changeClassMethod,
                      bool transferAllNoCompete,
                      vector<pRunner> &changedClass,
                      vector<pRunner> &changedClassNoResult,
                      vector<pRunner> &assignedVacant,
                      vector<pRunner> &newEntries,
                      vector<pRunner> &notTransfered,
                      vector<pRunner> &noAssignmentTarget);

  void transferResult(oEvent &ce,
                      ChangedClassMethod changeClassMethod,
                      vector<pTeam> &newEntries,
                      vector<pTeam> &notTransfered,
                      vector<pTeam> &noAssignmentTarget);

  enum MultiStageType {
    MultiStageNone = 0,
    MultiStageSeparateEntry = 1,
    MultiStageSameEntry = 2,
  };

  MultiStageType getMultiStageType() const;
  bool hasNextStage() const;
  bool hasPrevStage() const;

  int getNumStages() const;
  void setNumStages(int numStages);

  int getStageNumber() const;
  void setStageNumber(int num);

  /** Check that all necessary features are present, (fix by adding features)*/
  void checkNecessaryFeatures();

  /** Show dialog and return false if card is not used. */
  bool checkCardUsed(gdioutput &gdi, oRunner &runnerToAssignCard, int CardNo);

  void analyseDNS(vector<pRunner> &unknown_dns, vector<pRunner> &known_dns,
                  vector<pRunner> &known, vector<pRunner> &unknown, bool &hasSetDNS);

  void importOECSV_Data(const wstring &oecsvfile, bool clear);
  void importXML_IOF_Data(const wstring &clubfile, const wstring &competitorfile, bool clear);

  void generateTestCard(SICard &sic) const;
  pClass generateTestClass(int nlegs, int nrunners,
                           wchar_t *name, const wstring &start);
  pCourse generateTestCourse(int nCtrl);
  void generateTestCompetition(int nClasses, int nRunners, bool generateTeams);
  //Returns number of changed, non-saved elements.
  int checkChanged(vector<wstring> &out) const;
  void checkDB(); //Check database for consistancy...
  oEvent(gdioutput &gdi);
  oEvent &operator=(const oEvent &oe);
  virtual ~oEvent();
  friend class oAbstractRunner;
  friend class oCourse;
  friend class oClass;
  friend class oClub;
  friend class oRunner;
  friend class oBase;
  friend class oControl;
  friend class oTeam;
  friend class oCard;
  friend class oFreePunch;

  friend class oListInfo;
  friend class MeosSQL;
  friend class MySQLReconnect;

  friend class TestMeOS;

  //const string &toNarrow(const wstring &in) const {return gdibase.toNarrow(in);}
  //const wstring &toWide(const string &in) const {return gdibase.toWide(in);}
  const gdioutput &gdiBase() const {return gdibase;}
};

#endif // !defined(AFX_OEVENT_H__CDA15578_CB62_4EAD_96B9_3037355F5D48__INCLUDED_)