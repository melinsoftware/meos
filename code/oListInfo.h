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

enum EPostType
{
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
  lCourseLength,
  lCourseName,
  lCourseClimb,
  lCourseShortening,
  lCourseUsage,
  lCourseUsageNoVacant,
  lCourseClasses,
  lRunnerName,
  lRunnerGivenName,
  lRunnerFamilyName,
  lRunnerCompleteName,
  lPatrolNameNames, // Single runner's name or both names in a patrol
  lPatrolClubNameNames, // Single runner's club or combination of patrol clubs
  lRunnerFinish,
  lRunnerTime,
  lRunnerTimeStatus,
  lRunnerTotalTime,
  lRunnerTimePerKM,
  lRunnerTotalTimeStatus,
  lRunnerTotalPlace,
  lRunnerPlaceDiff,
  lRunnerClassCoursePlace,
  lRunnerTotalTimeAfter,
  lRunnerClassCourseTimeAfter,
  lRunnerTimeAfterDiff,
  lRunnerTempTimeStatus,
  lRunnerTempTimeAfter,
  lRunnerGeneralTimeStatus,
  lRunnerGeneralPlace,
  lRunnerGeneralTimeAfter,
  lRunnerTimeAfter,
  lRunnerMissedTime,
  lRunnerPlace,
  lRunnerStart,
  lRunnerStartCond,
  lRunnerStartZero,
  lRunnerClub,
  lRunnerCard,
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

  lRunnerUMMasterPoint,
  lRunnerTimePlaceFixed,
  lRunnerLegNumberAlpha,
  lRunnerLegNumber,

  lRunnerBirthYear,
  lRunnerAge,
  lRunnerSex,
  lRunnerNationality,
  lRunnerPhone,
  lRunnerFee,

  lTeamName,
  lTeamStart,
  lTeamStartCond,
  lTeamStartZero,
  lTeamTimeStatus,
  lTeamTimeAfter,
  lTeamPlace,
  lTeamLegTimeStatus,
  lTeamLegTimeAfter,
  lTeamRogainingPoint,
  lTeamRogainingPointTotal,
  lTeamRogainingPointReduction,
  lTeamRogainingPointOvertime,
  lTeamTimeAdjustment,
  lTeamPointAdjustment,

  lTeamTime,
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

  lPunchNamedTime,
  lPunchTime,
  lPunchControlNumber,
  lPunchControlCode,
  lPunchLostTime,
  lPunchControlPlace,
  lPunchControlPlaceAcc,

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

  lRogainingPunch,
  lTotalCounter,
  lSubCounter,
  lSubSubCounter,
  lLastItem
};

enum EStdListType
{
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
  EFilterVacant,
  EFilterOnlyVacant,
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
  _ESubFilterMax
};

enum gdiFonts;

struct oPrintPost {
  oPrintPost();
  oPrintPost(EPostType type_, const wstring &format_,
             int style_, int dx_, int dy_, 
             pair<int, bool> legIndex_=make_pair(0, true));

  static string encodeFont(const string &face, int factor);
  static wstring encodeFont(const wstring &face, int factor);

  EPostType type;
  wstring text;
  wstring fontFace;
  int resultModuleIndex;
  int format;
  GDICOLOR color;
  int dx;
  int dy;
  int legIndex;
  bool linearLegIndex;
  gdiFonts getFont() const {return gdiFonts(format & 0xFF);}
  oPrintPost &setFontFace(const wstring &font, int factor) {
    fontFace = encodeFont(font, factor);
    return *this;
  }
  int fixedWidth;
  bool doMergeNext;
  mutable const oPrintPost *mergeWithTmp; // Merge text with this output
};

class gdioutput;
enum gdiFonts;
typedef int (*GUICALLBACK)(gdioutput *gdi, int type, void *data);
class xmlparser;
class xmlobject;
class MetaListContainer;

struct oListParam {
  oListParam();
  EStdListType listCode;
  GUICALLBACK cb;
  set<int> selection;
  
  int useControlIdResultTo;
  int useControlIdResultFrom;
  int filterMaxPer;
  bool pageBreak;
  bool showInterTimes;
  bool showSplitTimes;
  bool splitAnalysis;
  bool showInterTitle;
  wstring title;
  wstring name;
  int inputNumber;
  int nextList; // 1-based index of next list (in the container, MetaListParam::listParam) for linked lists
  int previousList; // 1-based index of previous list (in the container, MetaListParam::listParam) for linked lists. Not serialized

  mutable int relayLegIndex; // Current index of leg (or -1 for entire team)
  mutable wstring defaultName; // Initialized when generating list
  // Generate a large-size list (supported as input when supportLarge is true)
  bool useLargeSize;
  bool saved;

  void updateDefaultName(const wstring &pname) const {defaultName = pname;}
  void setCustomTitle(const wstring &t) {title = t;}
  void getCustomTitle(wchar_t *t) const; // 256 size buffer required. Get title if set
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



private:
   int legNumber;
};

class oListInfo {
public:
  enum EBaseType {EBaseTypeRunner,
                  EBaseTypeTeam,
                  EBaseTypeClub,
                  EBaseTypePunches,
                  EBaseTypeNone,
                  EBaseTypeRunnerGlobal,  // Used only in metalist (meaning global, not classwise)
                  EBaseTypeRunnerLeg,  // Used only in metalist, meaning legwise
                  EBaseTypeTeamGlobal, // Used only in metalist (meaning global, not classwise)
                  EBaseTypeCourse,
                  EBaseTypeControl,
                  EBasedTypeLast_};

  bool isTeamList() const {return listType == EBaseTypeTeam;}
  
  enum ResultType {
    Global,
    Classwise,
    Legwise,
  };

  static bool addRunners(EBaseType t) {return t == EBaseTypeRunner || t == EBaseTypeClub;}
  static bool addTeams(EBaseType t) {return t == EBaseTypeTeam || t == EBaseTypeClub;}
  static bool addPatrols(EBaseType t) {return t == EBaseTypeTeam || t == EBaseTypeClub;}

  const wstring &getName() const {return Name;}
protected:
  wstring Name;
  EBaseType listType;
  EBaseType listSubType;
  SortOrder sortOrder;
     
  bool calcResults;
  bool calcCourseClassResults;
  bool calcTotalResults;
  bool rogainingResults;

  oListParam lp;

  list<oPrintPost> Head;
  list<oPrintPost> subHead;
  list<oPrintPost> listPost;
  vector<char> listPostFilter;
  vector<char> listPostSubFilter;
  list<oPrintPost> subListPost;
  bool fixedType;
  bool needPunches;
  string resultModule;
  set<string> additionalModules;

  void setupLinks(const list<oPrintPost> &lst) const;
  void setupLinks() const;

  list<oListInfo> next;
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


  bool needPunchCheck() const {return needPunches;}
  void setCallback(GUICALLBACK cb);
  int getLegNumberCoded() const {return lp.getLegNumberCoded();}


  EStdListType getListCode() const {return lp.listCode;}
  oPrintPost &addHead(const oPrintPost &pp) {
    Head.push_back(pp);
    return Head.back();
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
                      const wchar_t *fontFace = 0,
                      bool large = false, 
                      int minSize = 0);


  int getMaxCharWidth(const oEvent *oe, 
                      const set<int> &clsSel,
                      EPostType type, 
                      wstring formats,
                      gdiFonts font,
                      const wchar_t *fontFace = 0,
                      bool large = false, 
                      int minSize = 0) {
    vector< pair<EPostType, wstring> > typeFormats(1, make_pair(type, formats));
    return getMaxCharWidth(oe, oe->gdiBase(), clsSel, typeFormats, font, fontFace, largeSize, minSize);
  }


  const oListParam &getParam() const {return lp;}
  oListParam &getParam() {return lp;}

  // Returns true if the list needs to be regenerated due to competition changes
  bool needRegenerate(const oEvent &oe) const;

};
