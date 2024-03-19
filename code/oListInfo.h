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

#include <set>
#include <vector>
#include <map>
#include "oBase.h"
#include "gdifonts.h"
#include "gdioutput.h"
#include "oEvent.h"
class oClass;

typedef oEvent *pEvent;
typedef oClass *pClass;

enum EPostType {
  lAlignNext,
  lNone,
  lString,
  lResultDescription,
  lTimingFromName,
  lTimingToName,
  lCmpName,
  lCmpDate,
  lCurrentTime,
  lClubName,
  lClassName,
  lClassStartName,
  lClassStartTime,
  lClassStartTimeRange,
  lClassLength,
  lClassResultFraction,
  lClassAvailableMaps,
  lClassTotalMaps,
  lClassNumEntries,
  lClassDataA,
  lClassDataB,
  lClassTextA,

  lCourseLength,
  lCourseName,
  lCourseNumber,
  lCourseClimb,
  lCourseShortening,
  lCourseUsage,
  lCourseUsageNoVacant,
  lCourseClasses,
  lCourseNumControls,
  lRunnerName,
  lRunnerGivenName,
  lRunnerFamilyName,
  lRunnerCompleteName,
  lRunnerLegTeamLeaderName, // The runner on the (parallell) leg (in the team) with best result
  lPatrolNameNames, // Single runner's name or both names in a patrol
  lPatrolClubNameNames, // Single runner's club or combination of patrol clubs
  lRunnerFinish,
  lRunnerTime,
  lRunnerGrossTime,
  lRunnerTimeStatus,
  lRunnerTotalTime,
  lRunnerTimePerKM,
  lRunnerTotalTimeStatus,
  lRunnerTotalPlace,
  lRunnerPlaceDiff,
  lRunnerClassCoursePlace,
  lRunnerCoursePlace,
  lRunnerTotalTimeAfter,
  lRunnerClassCourseTimeAfter,
  lRunnerTimeAfterDiff,
  lRunnerTempTimeStatus,
  lRunnerTempTimeAfter,
  lRunnerGeneralTimeStatus,
  lRunnerGeneralPlace,
  lRunnerGeneralTimeAfter,
  lRunnerTimeAfter,
  lRunnerLostTime,
  lRunnerPlace,
  lRunnerStart,
  lRunnerCheck,
  lRunnerStartCond,
  lRunnerStartZero,
  lRunnerClub,
  lRunnerCard,
  lRunnerRentalCard,
  lRunnerBib,
  lRunnerStartNo,
  lRunnerRank,
  lRunnerCourse,
  lRunnerRogainingPoint,
  lRunnerRogainingPointTotal,
  lRunnerRogainingPointReduction,
  lRunnerRogainingPointOvertime,
  lRunnerRogainingPointGross,
  lRunnerTimeAdjustment,
  lRunnerPointAdjustment,
  lRunnerCardVoltage,

  lRunnerStageTime,
  lRunnerStageStatus,
  lRunnerStageTimeStatus,
  lRunnerStagePlace,
  lRunnerStagePoints,
  lRunnerStageNumber,

  lRunnerUMMasterPoint,
  lRunnerTimePlaceFixed,
  lRunnerLegNumberAlpha,
  lRunnerLegNumber,

  lRunnerBirthYear,
  lRunnerBirthDate,
  lRunnerAge,
  lRunnerSex,
  lRunnerNationality,
  lRunnerPhone,
  lRunnerFee,
  lRunnerExpectedFee,
  lRunnerPaid,
  lRunnerPayMethod,
  lRunnerEntryDate,
  lRunnerEntryTime,
  lRunnerId,
  lRunnerDataA,
  lRunnerDataB,
  lRunnerTextA,

  lTeamName,
  lTeamStart,
  lTeamStartCond,
  lTeamStartZero,
  lTeamTimeStatus,
  lTeamTimeAfter,
  lTeamPlace,
  lTeamCourseName,
  lTeamCourseNumber,
  lTeamLegName,
  lTeamLegTimeStatus,
  lTeamLegTimeAfter,
  lTeamRogainingPoint,
  lTeamRogainingPointTotal,
  lTeamRogainingPointReduction,
  lTeamRogainingPointOvertime,
  lTeamTimeAdjustment,
  lTeamPointAdjustment,

  lTeamTime,
  lTeamGrossTime,
  lTeamStatus,
  lTeamClub,
  lTeamRunner,
  lTeamRunnerCard,
  lTeamBib,
  lTeamStartNo,
  lTeamFee,

  lTeamTotalTime,
  lTeamTotalTimeStatus,
  lTeamTotalPlace,
  lTeamTotalTimeAfter,
  lTeamTotalTimeDiff,
  lTeamPlaceDiff,

  lTeamDataA,
  lTeamDataB,
  lTeamTextA,

  lPunchNamedTime,
  lPunchTeamTotalNamedTime,
  lPunchNamedSplit,
  
  lPunchName,

  lPunchTime,
  lPunchTeamTime,

  lPunchControlNumber,
  lPunchControlCode,
  lPunchLostTime,
  lPunchControlPlace,
  lPunchControlPlaceAcc,
  lPunchControlPlaceTeamAcc,

  lPunchSplitTime,
  lPunchTotalTime,
  lPunchTotalTimeAfter,

  lPunchTeamTotalTime,
  lPunchTeamTotalTimeAfter,

  lPunchAbsTime,
  lPunchTimeSinceLast,

  lResultModuleTime,
  lResultModuleNumber,
  lResultModuleTimeTeam,
  lResultModuleNumberTeam,
  
  lCountry,
  lNationality,

  lControlName,
  lControlCourses,
  lControlClasses,
  lControlVisitors,
  lControlPunches,
  lControlMedianLostTime,
  lControlMaxLostTime,
  lControlMistakeQuotient,
  lControlRunnersLeft,
  lControlCodes,

  lNumEntries,
  lNumStarts,
  lTotalRunLength,
  lTotalRunTime,

  lRogainingPunch,
  lTotalCounter,
  lSubCounter,
  lSubSubCounter,

  lImage,
  lLineBreak,
  lLastItem
};

enum EStdListType {
  EStdNone=-1,
  EStdStartList=1,
  EStdResultList,
  EGeneralResultList,
  ERogainingInd,
  EStdTeamResultListAll,
  unused_EStdTeamResultListLeg,//EStdTeamResultListLeg,
  EStdTeamResultList,
  EStdTeamStartList,
  EStdTeamStartListLeg,
  EStdIndMultiStartListLeg,
  EStdIndMultiResultListLeg,
  EStdIndMultiResultListAll,
  EStdPatrolStartList,
  EStdPatrolResultList,
  EStdRentedCard,
  EStdResultListLARGE,
  unused_EStdTeamResultListLegLARGE,//EStdTeamResultListLegLARGE,
  EStdPatrolResultListLARGE,
  EStdIndMultiResultListLegLARGE,
  unused_EStdRaidResultListLARGE,//EStdRaidResultListLARGE, //Obsolete
  ETeamCourseList,
  EIndCourseList,
  EStdClubStartList,
  EStdClubResultList,

  EIndPriceList,
  EStdUM_Master,

  EFixedPreReport,
  EFixedReport,
  EFixedInForest,
  EFixedInvoices,
  EFixedEconomy,
  unused_EFixedResultFinishPerClass,
  unused_EFixedResultFinish,
  EFixedMinuteStartlist,
  EFixedTimeLine,
  EFixedLiveResult,

  EStdTeamAllLegLARGE,
  
  EFirstLoadedList = 1000
};

enum EFilterList
{
  EFilterHasResult,
  EFilterHasPrelResult,
  EFilterRentCard,
  EFilterHasCard,
  EFilterHasNoCard,
  EFilterExcludeDNS,
  EFilterExcludeCANCEL,
  EFilterVacant,
  EFilterOnlyVacant,  
  EFilterAnyResult, // With any (radio) punch on a leg
  EFilterAPIEntry, // Entry via API
  EFilterWrongFee,
  EFilterIncludeNotParticipating,
  _EFilterMax
};

enum ESubFilterList
{
  ESubFilterHasResult,
  ESubFilterHasPrelResult,
  ESubFilterExcludeDNS,
  ESubFilterVacant,
  ESubFilterSameParallel,
  ESubFilterSameParallelNotFirst,
  ESubFilterNotFinish,
  ESubFilterNamedControl,
  _ESubFilterMax
};

enum gdiFonts;

struct oPrintPost {
  oPrintPost();
  oPrintPost(EPostType type, const wstring &format,
             int style, int dx, int dy, 
             pair<int, bool> legIndex = make_pair(0, true));
  oPrintPost(const wstring& image,
             int style, int dx, int dy,
             int width, int height);

  static string encodeFont(const string &face, int factor);
  static wstring encodeFont(const wstring &face, int factor);

  EPostType type;
  int format;

  wstring text;
  wstring fontFace;

  int resultModuleIndex = -1;
  GDICOLOR color = colorDefault;

  int dx;
  int dy;
  mutable int xlimit = 0;
  int legIndex;
  bool linearLegIndex;
  gdiFonts getFont() const {return gdiFonts(format & 0xFF);}
  oPrintPost &setFontFace(const wstring &font, int factor) {
    fontFace = encodeFont(font, factor);
    return *this;
  }
  int fixedWidth = 0;
  int fixedHeight = 0;
  bool useStrictWidth = false; // Crop text
  bool doMergeNext = false;
  bool imageNoUpdatePos = false;
  mutable const oPrintPost *mergeWithTmp = nullptr; // Merge text with this output
};

class gdioutput;
class BaseInfo;
enum gdiFonts;
class xmlparser;
class xmlobject;
class MetaListContainer;

struct SplitPrintListInfo {
  bool includeSplitTimes = true;
  bool withSpeed = true;
  bool withResult = true;
  bool withAnalysis = true;
  bool withStandardHeading = true;
  int numClassResults = 3;

  void serialize(xmlparser& xml) const;
  void deserialize(const xmlobject& xml);
  int64_t checkSum() const;
};

struct oListParam {
  oListParam();

  bool operator==(const oListParam& a) const {
    return a.listCode == listCode &&
      a.selection == selection &&
      a.ageFilter == ageFilter &&
      a.useControlIdResultFrom == useControlIdResultFrom &&
      a.useControlIdResultTo == useControlIdResultTo &&
      a.filterMaxPer == filterMaxPer &&
      a.alwaysInclude == alwaysInclude &&
      a.pageBreak == pageBreak &&
      a.showHeader == showHeader &&
      a.showInterTimes == showInterTimes &&
      a.showSplitTimes == showSplitTimes &&
      a.inputNumber == inputNumber &&
      a.nextList == nextList &&
      a.previousList == previousList &&
      a.bgColor == bgColor &&
      a.bgColor2 == bgColor2 &&
      a.bgImage == bgImage &&
      a.legNumber == legNumber &&
      a.nColumns == nColumns &&
      a.timePerPage == timePerPage &&
      a.screenMode == screenMode &&
      a.animate == animate &&
      a.htmlRows == htmlRows &&
      a.htmlScale == htmlScale &&
      a.htmlTypeTag == htmlTypeTag;
  }

  wstring getContentsDescriptor(const oEvent &oe) const;

  EStdListType listCode;
  GUICALLBACK cb;
  set<int> selection;
  
  bool lockUpdate; // Temporary prevent animation update

  int useControlIdResultTo;
  int useControlIdResultFrom;
  
  // Max number shown per class/class etc
  int filterMaxPer;
  const oAbstractRunner *alwaysInclude = nullptr;
  bool filterInclude(int count, const oAbstractRunner *r) const;

  bool pageBreak;
  bool showHeader = true;
  bool showInterTimes;
  bool showSplitTimes;
  bool splitAnalysis;
  bool showInterTitle;
  wstring title;
  wstring name;
  int inputNumber;
  int nextList; // 1-based index of next list (in the container, MetaListParam::listParam) for linked lists
  int previousList; // 1-based index of previous list (in the container, MetaListParam::listParam) for linked lists. Not serialized

  enum class AgeFilter {
    All,
    OnlyYouth,
    ExludeYouth,
  };

  AgeFilter ageFilter = AgeFilter::All;

  mutable bool lineBreakControlList = false;
  mutable int relayLegIndex; // Current index of leg (or -1 for entire team)
  mutable wstring defaultName; // Initialized when generating list
  // generate a large-size list (supported as input when supportLarge is true)
  bool useLargeSize;
  bool saved;

  int bgColor;
  int bgColor2;

  int fgColor;
  wstring bgImage;

  int nColumns;
  bool animate;
  int timePerPage;
  int margin;
  int screenMode;// 0 normal window, 1 = page by page, 2 = scroll

  int htmlRows;
  double htmlScale;
  string htmlTypeTag; // free, table, or template tag.

  void updateDefaultName(const wstring &pname) const {defaultName = pname;}
  void setCustomTitle(const wstring &t) {title = t;}
  const wstring &getCustomTitle(const wstring &t) const;
  const wstring &getDefaultName() const {return defaultName;}
  void setName(const wstring &n) {name = n;}
  const wstring &getName() const {return name;}

  int getInputNumber() const {return inputNumber;}
  void setInputNumber(int n) {inputNumber = n;}

  void serialize(xmlparser &xml, 
                 const MetaListContainer &container, 
                 const map<int, int> &idToIndex) const;
  void deserialize(const xmlobject &xml, const MetaListContainer &container);

  void setLegNumberCoded(int code) {
    if (code == 1000)
      legNumber = -1;
    else
      legNumber = code;
  }

  bool matchLegNumber(const pClass cls, int leg) const;
  int getLegNumber(const pClass cls) const;
  pair<int, bool> getLegInfo(const pClass cls) const;

  wstring getLegName() const;

  const int getLegNumberCoded() const {
    return legNumber >= 0 ? legNumber : 1000;
  }

  int sourceParam = -1;

  bool tightBoundingBox = false;

private:
   int legNumber;
};

class oListInfo {
public:
  enum EBaseType {EBaseTypeRunner,
                  EBaseTypeTeam,
                  EBaseTypeClubRunner,
                  EBaseTypeClubTeam,
                  EBaseTypeCoursePunches,
                  EBaseTypeAllPunches,
                  EBaseTypeNone,
                  EBaseTypeRunnerGlobal,  // Used only in metalist (meaning global, not classwise)
                  EBaseTypeRunnerLeg,  // Used only in metalist, meaning legwise
                  EBaseTypeTeamGlobal, // Used only in metalist (meaning global, not classwise)
                  EBaseTypeCourse,
                  EBaseTypeControl,
                  EBasedTypeLast_};

  bool isTeamList() const {return listType == EBaseTypeTeam;}
  bool isSplitPrintList() const { return splitPrintInfo != nullptr; }

  /** Return true, if includeHeader is set, also considers the header. */
  bool empty(bool includeHeader = true) const {
    if (includeHeader)
      return head.empty() && subHead.empty() && listPost.empty() && subListPost.empty();
    else
      return subHead.empty() && listPost.empty() && subListPost.empty();
  }

  enum ResultType {
    Global,
    Classwise,
    Legwise,
    Coursewise
  };

  enum class PunchMode {
    NoPunch,
    SpecificPunch,
    AnyPunch
  };
  static bool addRunners(EBaseType t) {return t == EBaseTypeRunner || t == EBaseTypeClubRunner;}
  static bool addTeams(EBaseType t) {return t == EBaseTypeTeam || t == EBaseTypeClubRunner || t == EBaseType::EBaseTypeClubTeam;}
  static bool addPatrols(EBaseType t) {return t == EBaseTypeTeam || t == EBaseTypeClubRunner || t == EBaseType::EBaseTypeClubTeam;}

  // Return true if the runner should be skipped
  bool filterRunner(const oRunner &r) const;
  bool filterRunnerResult(GeneralResult *gResult, const oRunner &r) const;

  GeneralResult *applyResultModule(oEvent &oe, vector<pRunner> &rlist) const;
  const wstring &getName() const {return Name;}

  void shrinkSize();

protected:
  wstring Name;
  EBaseType listType;
  EBaseType listSubType;
  SortOrder sortOrder;
     
  bool calcResults;
  bool calcCourseClassResults;
  bool calcCourseResults;

  bool calcTotalResults;
  bool rogainingResults;
  bool calculateLiveResults;

  oListParam lp;

  list<oPrintPost> head;
  list<oPrintPost> subHead;
  list<oPrintPost> listPost;
  list<oPrintPost> subListPost;
  
  vector<char> listPostFilter;
  vector<char> listPostSubFilter;
  bool fixedType;
  
  PunchMode needPunches;
  string resultModule;
  set<string> additionalModules;

  void setupLinks(const list<oPrintPost> &lst) const;
  void setupLinks() const;

  list<oListInfo> next;

  shared_ptr<SplitPrintListInfo> splitPrintInfo;

public:
  ResultType getResultType() const;

  bool supportClasses;
  bool supportLegs;
  bool supportParameter;
  // True if large (and non-large) is supported
  bool supportLarge;
  // True if a large-size list only
  bool largeSize;

  bool supportSplitAnalysis;
  bool supportInterResults;
  bool supportPageBreak;
  bool supportClassLimit;
  bool supportCustomTitle;

  // True if supports timing from control
  bool supportTo;
  // True if supports timing to control
  bool supportFrom;
  // Result type 
  ResultType resType;

  void replaceType(EPostType find, EPostType replace, bool onlyFirst);

  PunchMode needPunchCheck() const {return needPunches;}
  void setCallback(GUICALLBACK cb);
  int getLegNumberCoded() const {return lp.getLegNumberCoded();}

  bool supportUpdateClasses() const {
    return supportClasses && next.empty();
  }

  bool hasHead() const { return head.size() > 0; }
  
  const shared_ptr<SplitPrintListInfo>& getSplitPrintInfo() const {
    return splitPrintInfo;
  }

  void setSplitPrintInfo(const shared_ptr<SplitPrintListInfo>& info) {
    splitPrintInfo = info;
  }

  EStdListType getListCode() const {return lp.listCode;}
  oPrintPost &addHead(const oPrintPost &pp) {
    head.push_back(pp);
    return head.back();
  }
  oPrintPost &addSubHead(const oPrintPost &pp) {
    subHead.push_back(pp);
    return subHead.back();
  }
  oPrintPost &addListPost(const oPrintPost &pp) {
    listPost.push_back(pp);
    return listPost.back();
  }
  oPrintPost &addSubListPost(const oPrintPost &pp) {
    subListPost.push_back(pp);
    return subListPost.back();
  }
  inline bool filter(EFilterList i) const {return listPostFilter[i]!=0;}
  inline bool subFilter(ESubFilterList i) const {return listPostSubFilter[i]!=0;}

  void setFilter(EFilterList i) {listPostFilter[i]=1;}
  void setSubFilter(ESubFilterList i) {listPostSubFilter[i]=1;}

  void setResultModule(const string &rm) {resultModule = rm;}
  void additionalResultModule(const string &rm) {additionalModules.insert(rm);}
  const string &getResultModule() const {return resultModule;}
  oListInfo(void);
  ~oListInfo(void);

  friend class oEvent;
  friend class MetaList;
  friend class MetaListContainer;

  int getMaxCharWidth(const oEvent *oe,
                      const gdioutput &gdi,
                      const set<int> &clsSel,
                      const vector< pair<EPostType, wstring> > &typeFormats,
                      gdiFonts font,
                      const wchar_t *fontFace = nullptr,
                      bool large = false, 
                      int minSize = 0) const;


  int getMaxCharWidth(const oEvent *oe, 
                      const set<int> &clsSel,
                      EPostType type, 
                      wstring formats,
                      gdiFonts font,
                      const wchar_t *fontFace = nullptr,
                      bool large = false, 
                      int minSize = 0) const {
    vector< pair<EPostType, wstring> > typeFormats(1, make_pair(type, formats));
    return getMaxCharWidth(oe, oe->gdiBase(), clsSel, typeFormats, font, fontFace, largeSize, minSize);
  }


  const oListParam &getParam() const {return lp;}
  oListParam &getParam() {return lp;}

  // Returns true if the list needs to be regenerated due to competition changes
  bool needRegenerate(const oEvent &oe) const;

};
