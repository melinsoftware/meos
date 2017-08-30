/********************i****************************************************
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

#include "stdafx.h"
#include "oListInfo.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <cassert>
#include <cmath>
#include "Localizer.h"
#include "metalist.h"
#include <algorithm>
#include "gdifonts.h"
#include "generalresult.h"
#include "meosexception.h"

struct PrintPostInfo {
  PrintPostInfo(gdioutput &gdi, const oListParam &par) : 
              gdi(gdi), par(par), keepToghether(false) {}

  gdioutput &gdi;
  const oListParam &par;
  oCounter counter;
  bool keepToghether;
  void reset() {keepToghether = false;}
private:
  PrintPostInfo &operator=(const PrintPostInfo &a) {}
};

oListInfo::oListInfo() {
  listType=EBaseTypeRunner;
  listSubType=EBaseTypeRunner;
  calcResults=false;
  calcTotalResults = false;
  rogainingResults = false;
  calcCourseClassResults = false;
  listPostFilter.resize(_EFilterMax+1, 0);
  listPostSubFilter.resize(_ESubFilterMax+1, 0);
  fixedType = false;
  largeSize = false;
  supportClasses = true;
  supportLegs = false;
  supportParameter = false;
  supportLarge = false;
  supportTo = false;
  supportFrom = false;
  supportCustomTitle = true;
  
  resType = Classwise;

  supportSplitAnalysis = true;
  supportInterResults = true;
  supportPageBreak = true;
  supportClassLimit = true;
  
  needPunches = false;
}

oListInfo::~oListInfo(void)
{
}

oPrintPost::oPrintPost()
{
  dx=0;
  dy=0;
  format=0;
  type=lString;
  legIndex= - 1;
  linearLegIndex = true;
  fixedWidth = 0;
  mergeWithTmp = 0;
  doMergeNext = false;
  color = colorDefault;
  resultModuleIndex = -1;
}

oPrintPost::oPrintPost(EPostType type_, const string &text_,
                       int format_, int dx_, int dy_, pair<int, bool> index) {
  dx=dx_;
  dy=dy_;
  text=text_;
  format=format_;
  type=type_;
  legIndex=index.first;
  linearLegIndex = index.second;
  fixedWidth = 0;
  mergeWithTmp = 0;
  doMergeNext = false;
  color = colorDefault;
  resultModuleIndex = -1;
}

bool oListInfo::needRegenerate(const oEvent &oe) const {
  for(oClassList::const_iterator it = oe.Classes.begin(); it != oe.Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!lp.selection.empty() && lp.selection.count(it->getId()) == 0 )
      continue; // Not our class

    int legToCheck = -1;
    if (needPunches) {
      int to = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultTo).first;
      int from = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultFrom).first;

      if (it->wasSQLChanged(legToCheck, to) ||
          it->wasSQLChanged(legToCheck, from) )
        return true;
    }
    else {
      if (it->wasSQLChanged(legToCheck, -1))
        return true;
    }
  }

  return false;
}

static void generateNBestHead(const oListParam &par, oListInfo &li, int ypos) {
  if (par.filterMaxPer > 0)
    li.addHead(oPrintPost(lString, lang.tl("Visar de X bästa#" + itos(par.filterMaxPer)), normalText, 0, ypos));
}

static pair<string, bool> getControlName(const oEvent &oe, int courseContolId) {
  pair<int, int> idt = oControl::getIdIndexFromCourseControlId(courseContolId);
  pControl to = oe.getControl(idt.first);
  string toS;
  bool name = false;
  if (to) {
    if (to->hasName()) {
      toS = to->getName();
      name = true;
    }
    else if (to->getFirstNumber()>0)
      toS = itos(to->getFirstNumber());
    else
      toS = itos(idt.first);

    if (to->getNumberDuplicates() > 0)
      toS += "-" + itos(idt.second + 1);
  }
  else
    toS = itos(idt.first);

  return make_pair(toS, name);
}

static string getFullControlName(const oEvent &oe, int ctrl) {
  pair<string, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl("Kontroll X#" + toS.first);
}

static void getResultTitle(const oEvent &oe, const oListParam &lp, string &title) {
  if (lp.useControlIdResultTo <= 0 && lp.useControlIdResultFrom <= 0)
    title = lang.tl("Resultat - %s");
  else if (lp.useControlIdResultTo>0 && lp.useControlIdResultFrom<=0){
    pair<string, bool> toS = getControlName(oe, lp.useControlIdResultTo);
    if (toS.second)
      title = lang.tl("Resultat - %s") + ", " + toS.first;
    else
      title = lang.tl("Resultat - %s") + ", " + lang.tl("vid kontroll X#" + toS.first);
  }
  else {
    string fromS = lang.tl("Start"), toS = lang.tl("Mål");
    if (lp.useControlIdResultTo>0) {
      toS = getControlName(oe, lp.useControlIdResultTo).first;
    }
    if (lp.useControlIdResultFrom>0) {
      fromS = getControlName(oe, lp.useControlIdResultFrom).first;
    }
    title = lang.tl("Resultat mellan X och Y#" + fromS + "#" + toS);
  }
}

static double adjustmentFactor(double par, double target) {
  double k = (1.0 - target)/10;
  double val = max(target, 1.0 - (par * k));
  return val;
}

int oListInfo::getMaxCharWidth(const oEvent *oe,
                               const set<int> &clsSel,
                               const vector< pair<EPostType, string> > &typeFormats,
                               gdiFonts font,
                               const char *fontFace,
                               bool large, int minSize) {
  vector<oPrintPost> pps;
  for (size_t k = 0; k < typeFormats.size(); k++) {
    pps.push_back(oPrintPost());
    pps.back().text = typeFormats[k].second;
    pps.back().type = typeFormats[k].first;
  }

  oListParam par;
  par.setLegNumberCoded(0);
  oCounter c;
  vector<int> extras(pps.size(), 0);

  for (size_t k = 0; k < pps.size(); k++) {
    string extra;
    switch (pps[k].type) {
      case lRunnerTotalTimeAfter:
      case lRunnerClassCourseTimeAfter:
      case lRunnerTimeAfterDiff:
      case lRunnerTempTimeAfter:
      case lRunnerTimeAfter:
      case lRunnerMissedTime:
      case lTeamTimeAfter:
      case lTeamLegTimeAfter:
      case lTeamTotalTimeAfter:
      case lTeamTimeAdjustment:
      case lRunnerTimeAdjustment:
      case lRunnerGeneralTimeAfter:
 
        extra = "+10:00";
        break;
      case lTeamRogainingPointOvertime:
      case lRunnerRogainingPointOvertime:
      case lResultModuleTime:
      case lResultModuleTimeTeam:  
      case lTeamTime:
      case lTeamTotalTime:
      case lTeamTotalTimeStatus:
      case lTeamLegTimeStatus:
      case lTeamTimeStatus:
      case lRunnerTempTimeStatus:
      case lRunnerTotalTimeStatus:
      case lRunnerTotalTime:
      case lClassStartTime:
      case lRunnerFinish:
      case lRunnerTime:
      case lRunnerTimeStatus:
      case lRunnerTimePlaceFixed:
        extra = "50:50";
        break;
      case lRunnerGeneralTimeStatus:
      case lClassStartTimeRange:
        extra = "50:50 (50:50)";
        break;
      case lTeamRogainingPointReduction:
      case lRunnerRogainingPointReduction:
      case lTeamPointAdjustment:
      case lRunnerPointAdjustment:
      case lRunnerRogainingPointGross:
      case lRunnerPlace:
      case lRunnerPlaceDiff:
      case lTeamPlaceDiff:
      case lRunnerTotalPlace:
      case lRunnerClassCoursePlace:
      case lTeamPlace:
      case lTeamTotalPlace:
      case lPunchControlPlace:
      case lPunchControlPlaceAcc:
      case lResultModuleNumber:
      case lResultModuleNumberTeam:
        extra = "199.";
        break;
      case lRunnerGeneralPlace:
        extra = "199. (99.)";
        break;
    }

    int width = 0;
    EPostType type = pps[k].type;

    if (type == lClubName || type == lRunnerName || type == lTeamName
                          || type == lTeamRunner || type == lTeamClub)
      width = max(15, width);

    if (type == lRunnerGivenName || type == lRunnerFamilyName || type == lRunnerNationality)
      width = max(7, width);

    if (type == lRunnerCompleteName || type == lPatrolNameNames || type == lPatrolClubNameNames)
      width = max(20, width);

    width = max<int>(extra.size(), width);

    extras[k] = width;
  }

  for (size_t k = 0; k < pps.size(); k++) {
    const oPrintPost &pp = pps[k];
    int width = 0;

    for (oClassList::const_iterator it = oe->Classes.begin(); it != oe->Classes.end(); ++it) {
      if (it->isRemoved())
        continue;
      if (!clsSel.empty() && clsSel.count(it->getId()) == 0)
        continue;

      const string &out = oe->formatListString(pp, par, 0, 0, 0, pClass(&*it), c);
      width = max(width, int(out.length()));
    }

    for (oCourseList::const_iterator it = oe->Courses.begin(); it != oe->Courses.end(); ++it) {
      if (it->isRemoved())
        continue;

      const string &out = oe->formatSpecialString(pp, par, 0, 0, pCourse(&*it), 0,  c);
      width = max(width, int(out.length()));
    }

    for (oControlList::const_iterator it = oe->Controls.begin(); it != oe->Controls.end(); ++it) {
      if (it->isRemoved())
        continue;

      const string &out = oe->formatSpecialString(pp, par, 0, 0, 0, pControl(&*it),  c);
      width = max(width, int(out.length()));
    }
    extras[k] = max(extras[k], width);
  }

  int width = minSize;
  vector<int> row(pps.size(), 0);

  for (oRunnerList::const_iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->isRemoved())
      continue;

    // Case when runner/team has different class
    bool teamOK = it->getTeam() && clsSel.count(it->getTeam()->getClassId());

    if (!clsSel.empty() && (!teamOK && clsSel.count(it->getClassId()) == 0))
        continue;

    copy(extras.begin(), extras.end(), row.begin());
    
    for (size_t k = 0; k < pps.size(); k++) {
      oPrintPost &pp = pps[k];
      
      pp.legIndex = it->tLeg;
      pp.linearLegIndex = true;
      int numIter = 1;

      if (pp.type == lPunchNamedTime || pp.type == lPunchTime) {
        row[k] = max(row[k], 10);
        pRunner r = pRunner(&*it);
        numIter = (r && r->getCard()) ? r->getCard()->getNumPunches() + 1 : 1;
      }

      while (numIter-- > 0) {
        const string &out = oe->formatListString(pp, par, it->tInTeam, pRunner(&*it), it->Club, it->Class, c);
        row[k] = max(row[k], int(out.length()));
        if (numIter>0)
          c.level3++;
      }
    }

    int sum = 0; 
    for (size_t k = 0; k < row.size(); k++)
      sum += row[k];
    width = max(width, sum); 
  }

  if (width == 0) {
    for (size_t k = 0; k < row.size(); k++)
      width += extras[k];
  }

  double w = width;
  double s = 1.0;

  s = oe->gdibase.getRelativeFontScale(font, fontFace);

  if (w > 15) {
    if (large)
      w *= adjustmentFactor(w-15, 0.7);
    else
      w *= adjustmentFactor(w-15, 0.85);

    if (w > 40)
      w *= adjustmentFactor(w-40, 0.85);
  }
  w = max<double>(w, minSize);

  if (width>0 && !large)
    return int(s*(w*6.0+20.0));
  else if (width>0 && large)
    return int(s*(w*7.0+10.0));
  else
    return 0;
}

const string & oEvent::formatListString(EPostType type, const pRunner r) const
{
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->Class, ctr);
}

const string & oEvent::formatListString(EPostType type, const pRunner r, 
                                        const string &format) const {
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  pp.text = format;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->Class, ctr);
}


const string &oEvent::formatListString(const oPrintPost &pp, const oListParam &par,
                                       const pTeam t, const pRunner r, const pClub c,
                                       const pClass pc, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const string *tmp = 0;
  string *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().get();
        *out = "";
      }
      out->append(*tmp);
    }
    tmp = &formatListStringAux(*cpp, par, t, r, c, pc, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const string &oEvent::formatSpecialString(const oPrintPost &pp, const oListParam &par, const pTeam t, int legIndex,
                                          const pCourse crs, const pControl ctrl, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const string *tmp = 0;
  string *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().get();
        *out = "";
      }
      out->append(*tmp);
    }
    tmp = &formatSpecialStringAux(*cpp, par, t, legIndex, crs, ctrl, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const string &oEvent::formatSpecialStringAux(const oPrintPost &pp, const oListParam &par,
                                             const pTeam t, int legIndex,
                                             const pCourse pc, const pControl ctrl, 
                                             oCounter &counter) const {

  char bf[512];
  const string *sptr=0;
  bf[0]=0;
  static bool reentrantLock = false;
  if (reentrantLock == true) {
    reentrantLock = false;
    throw meosException("Internal list error");
  }

  switch (pp.type) {
    case lCourseLength:
      if (pc) {
        int len = pc->getLength();
        if (len > 0)
          sprintf_s(bf, "%d", len);
      }
    break;

    case lCourseName:
    case lRunnerCourse:
      if (pc) {
        sptr = &pc->getName();
      }
    break;

    case lRunnerLegNumberAlpha:
      if (t && t->getClassRef() && legIndex >= 0) {
        string legStr = t->getClassRef()->getLegNumber(legIndex);
        strcpy_s(bf, legStr.c_str());
      }
      break;

    case lRunnerLegNumber:
      if (t && t->getClassRef() && legIndex >= 0) {
         int legNumber, legOrder;
         t->getClassRef()->splitLegNumberParallel(legIndex, legNumber, legOrder);
         sptr = &itos(legNumber+1);
      }
      break;

    case lCourseClimb: {
      int len = pc ? pc->getDCI().getInt("Climb") : 0;
      if (len > 0)
        sprintf_s(bf, "%d", len);      
    }
    break;

    case lCourseUsage:
      if (pc) {
        sptr = &itos(pc->getNumUsedMaps(false));
      }
    break;

    case lCourseUsageNoVacant:
      if (pc) {
        sptr = &itos(pc->getNumUsedMaps(true));
      }
    break;

    case lCourseClasses:
      if (pc) {
        vector<pClass> cls;
        pc->getClasses(cls);
        string tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += ", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += ", ...";
            break;
          }
        }
        strncpy_s(bf, tmp.c_str(), 256);
      }
    break;
    

    case lControlName:
      if (ctrl)
        strncpy_s(bf, ctrl->getName().c_str(), 128);
    break;

    case lControlCourses:
      if (ctrl) {
        vector<pCourse> crs;
        ctrl->getCourses(crs);
        if (crs.size() == Courses.size()) {
          sptr = &lang.tl("Alla");
          break;
        }

        string tmp;
        for (size_t k = 0; k < crs.size(); k++) {
          if (k > 0)
            tmp += ", ";
          tmp += crs[k]->getName();
          if (tmp.length() > 100) {
            tmp += ", ...";
            break;
          }
        }
        strncpy_s(bf, tmp.c_str(), 256);
      }
    break;
  
    case lControlClasses:
      if (ctrl) {
        vector<pClass> cls;
        ctrl->getClasses(cls);
        if (cls.size() == getNumClasses()) {
          sptr = &lang.tl("Alla");
          break;
        }

        string tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += ", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += ", ...";
            break;
          }
        }
        strncpy_s(bf, tmp.c_str(), 256);
      }
    break;

    case lControlVisitors:
      if (ctrl)
        sptr = &itos(ctrl->getNumVisitors(false));
    break;

    case lControlPunches:
      if (ctrl)
        sptr = &itos(ctrl->getNumVisitors(true));
      break;

    case lControlMedianLostTime:
      if (ctrl) 
        sptr = &formatTime(ctrl->getMissedTimeMedian()); 
    break;

    case lControlMaxLostTime:
      if (ctrl) 
        sptr = &formatTime(ctrl->getMissedTimeMax()); 
      break;
    
    case lControlMistakeQuotient:
      if (ctrl) {
        string tmp = itos(ctrl->getMistakeQuotient()); 
        tmp += "%";
        strncpy_s(bf, tmp.c_str(), 20);
      }
      break;

    case lControlRunnersLeft:
      if (ctrl)
        sptr = &itos(ctrl->getNumRunnersRemaining());
    break;

    case lControlCodes: 
      if (ctrl) {
        vector<int> numbers;
        ctrl->getNumbers(numbers);
        string tmp;
        for (size_t j = 0; j < numbers.size(); j++) {
          if (j > 0)
            tmp += ", ";
          tmp += itos(numbers[j]);
        }
        strncpy_s(bf, tmp.c_str(), 256);
      }
      break;

    default: {
      reentrantLock = true;
      try {
        const string &res = formatListStringAux(pp, par, t, t ? t->getRunner(legIndex) : 0, 
                                                                t ? t->getClubRef() : 0,
                                                                t ? t->getClassRef() : 0, counter);
        reentrantLock = false;
        return res;
      }
      catch(...) {
        reentrantLock = false;
        throw;
      }
    }
  }

  if (pp.type!=lString && (sptr==0 || sptr->empty()) && bf[0]==0)
    return _EmptyString;
  else if (sptr) {
    if (pp.text.empty())
      return *sptr;
    else {
      sprintf_s(bf, pp.text.c_str(), sptr->c_str());
      string &res = StringCache::getInstance().get();
      res = bf;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      string &res = StringCache::getInstance().get();
      res = bf;
      return res;
    }
    else {
      char bf2[512];
      sprintf_s(bf2, pp.text.c_str(), bf);
      string &res = StringCache::getInstance().get();
      res = bf2;
      return res;
    }
  }
}

const string &oEvent::formatListStringAux(const oPrintPost &pp, const oListParam &par,
                                          const pTeam t, const pRunner r, const pClub c,
                                          const pClass pc, oCounter &counter) const {

  char bf[512];
  const string *sptr=0;
  bf[0]=0;
  bool invalidClass = pc && pc->getClassStatus() != oClass::Normal;
  int legIndex = pp.legIndex;
  if(pc && pp.type != lResultModuleNumber && pp.type != lResultModuleNumberTeam
        && pp.type != lResultModuleTime && pp.type != lResultModuleTimeTeam)
   legIndex = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
        
  switch ( pp.type ) {
    case lClassName:
      if (invalidClass)
        sprintf_s(bf, "%s (%s)", pc->getName().c_str(), lang.tl("Struken").c_str());
      else
        sptr=pc ? &pc->getName() : 0;
      break;
    case lResultDescription: {
        string title;
        getResultTitle(*this, par, title);
        strcpy_s(bf, title.c_str());
      }
      break;
    case lTimingFromName:
      if (par.useControlIdResultFrom > 0)
        strcpy_s(bf, getFullControlName(*this, par.useControlIdResultFrom).c_str());
      else
        sptr = &lang.tl("Start");
      break;
    case lTimingToName:
      if (par.useControlIdResultTo > 0)
        strcpy_s(bf, getFullControlName(*this, par.useControlIdResultTo).c_str());
      else
        sptr = &lang.tl("Mål");
      break;
    case lClassLength:
      if (pc) {
        strcpy_s(bf, pc->getLength(par.relayLegIndex).c_str());
      }
      break;
    case lClassStartName:
      if (pc) strcpy_s(bf, pc->getDI().getString("StartName").c_str());
      break;
    case lClassStartTime:
    case lClassStartTimeRange:
      if (pc) {
        int first, last;
        pc->getStartRange(legIndex, first, last);
        if (pc->hasFreeStart())
          strcpy_s(bf, lang.tl("Fri starttid").c_str());
        else if (first > 0 && first == last) {
          if (oe->useStartSeconds())
            strcpy_s(bf, oe->getAbsTime(first).c_str());
          else
            strcpy_s(bf, oe->getAbsTimeHM(first).c_str());
        }
        else if (pp.type == lClassStartTimeRange) {
          string range =  oe->getAbsTimeHM(first) + MakeDash(" - ") + oe->getAbsTimeHM(last);
          strcpy_s(bf, range.c_str());
        }
      }
      break;
    case lClassResultFraction:
      if (pc && !invalidClass) {
        int total, finished,  dns;
        oe->getNumClassRunners(pc->getId(), par.getLegNumber(pc), total, finished, dns);
        sprintf_s(bf, "(%d / %d)", finished, total);
      }
      break;
    case lCourseLength:
      if (r) {
        pCourse crs = r->getCourse(false);
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
    break;
    case lCourseName:
    case lRunnerCourse:
      if (r) {
        pCourse crs = r->getCourse(false);
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
    break;
    case lCourseClimb:
      if (r) {
        pCourse crs = r->getCourse(false);
         return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
    break;
    case lCourseShortening:
      if (r) {
        int sh = r->getNumShortening();
        if (sh > 0)
          sprintf_s(bf, "%d", sh);
      }
      break;
    case lCmpName:
      sptr = &getName();
      break;
    case lCmpDate:
      strcpy_s(bf, this->getDate().c_str());
      break;
    case lCurrentTime:
      strcpy_s(bf, getCurrentTimeS().c_str());
      break;
    case lRunnerClub:
      sptr=(r && r->Club) ? &r->Club->getDisplayName() : 0;
      break;
    case lRunnerFinish:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          sptr = &r->getFinishTimeS();
        else
          sptr = &r->getTempResult(pp.resultModuleIndex).getFinishTimeS(this);
      }
      break;
    case lRunnerStart:
    case lRunnerStartCond:
    case lRunnerStartZero:
      if (r) {
        if ((pp.type == lRunnerStartCond || pp.type == lRunnerStartZero) && pc && !pc->hasFreeStart()) {
          int fs, ls;
          pc->getStartRange(legIndex, fs, ls);
          if (fs>0 && fs == ls) {
            break; // Common start time, skip
          }
        }
        if (r->startTimeAvailable()) {
          if (pp.type != lRunnerStartZero) 
            sptr = &r->getStartTimeCompact();
          else {
            int st = r->getStartTime();
            sptr = &getTimeMS(st-3600);
          }
        }
        else
          sptr = &MakeDash("-");
      }
      break;
    
    case lRunnerName:
      sptr = r ? &r->getName() : 0;
      break;
    case lRunnerGivenName:
      if (r) strcpy_s(bf, r->getGivenName().c_str());
      break;
    case lRunnerFamilyName:
      if (r) strcpy_s(bf, r->getFamilyName().c_str());
      break;
    case lRunnerCompleteName:
      if (r) strcpy_s(bf, r->getCompleteIdentification().c_str());
      break;
    case lPatrolNameNames:
      if (t) {
        pRunner r1 = t->getRunner(0);
        pRunner r2 = t->getRunner(1);
        if (r1 && r2) {
          sprintf_s(bf, "%s / %s", r1->getName().c_str(),r2->getName().c_str());
        }
        else if (r1) {
          sptr = &r1->getName();
        }
        else if (r2) {
          sptr = &r2->getName();
        }
      }
      else {
        sptr = r ? &r->getName() : 0;
      }
      break;
    case lPatrolClubNameNames:
      if (t) {
        pRunner r1 = t->getRunner(0);
        pRunner r2 = t->getRunner(1);
        pClub c1 = r1 ? r1->Club : 0;
        pClub c2 = r2 ? r2->Club : 0;
        if (c1 == c2)
          c2 = 0;
        if (c1 && c2) {
          sprintf_s(bf, "%s / %s", c1->getDisplayName().c_str(),c2->getDisplayName().c_str());
        }
        else if (c1) {
          sptr = &c1->getDisplayName();
        }
        else if (c2) {
          sptr = &c2->getDisplayName();
        }
      }
      else {
        sptr = r && r->Club ? &r->Club->getDisplayName() : 0;
      }
      break;
    case lRunnerTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          sptr = &r->getRunningTimeS();
        else
          sptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);

        if (r->getNumShortening() > 0) {
          sprintf_s(bf, "*%s", sptr->c_str());
          sptr = 0;
        }

      }
    case lRunnerTimeStatus:
      if (r) {
        if (invalidClass)
          sptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          bool ok = r->prelStatusOK();
          if (ok && pc && !pc->getNoTiming()) {
            sptr = &r->getRunningTimeS();
            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else {
            if (ok)
              sptr = &formatStatus(StatusOK);
            else
              sptr = &r->getStatusS();
          }
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.getStatus() == StatusOK && pc && !pc->getNoTiming()) {
            sptr = &res.getRunningTimeS(0);
            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &res.getStatusS(StatusOK);
        }
      }
      break;

    case lRunnerGeneralTimeStatus:
      if (r) {
        if (invalidClass)
          sptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          if (r->prelStatusOK() && pc && !pc->getNoTiming()) {
            string timeStatus = r->getRunningTimeS();
            
            if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
              RunnerStatus ts = r->getTotalStatus();
              int rt = r->getTotalRunningTime();
              if (ts == StatusOK || (ts == StatusUnknown && rt > 0)) {
                string vts = formatTime(rt) + " (" + timeStatus + ")";
                swap(vts, timeStatus);
              }
              else {
                string vts = formatStatus(ts) + " (" + timeStatus + ")";
                swap(vts, timeStatus);
              }
            }

            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", timeStatus.c_str());
            }
            else {
              strcpy_s(bf, timeStatus.c_str());
            }
          }
          else
            sptr = &r->getStatusS();
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.getStatus() == StatusOK && pc && !pc->getNoTiming()) {
            sptr = &res.getRunningTimeS(0);
            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &res.getStatusS(StatusOK);
        }
      }
      break;

      break;

    case lRunnerGeneralTimeAfter:
      if (r && pc && !invalidClass && !pc->getNoTiming()) {
        if (pp.resultModuleIndex == -1) {

          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            int tleg = r->tLeg >= 0 ? r->tLeg:0;
            if (r->getTotalStatus()==StatusOK) {
              if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
                int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(tleg, true);
                if (after > 0)
                  sprintf_s(bf, "+%d:%02d", after/60, after%60);
              }
              else {
                sptr = &MakeDash("-");
              }
            }
          }
          else {
            int tleg=r->tLeg>=0 ? r->tLeg:0;
            if (r->tStatus==StatusOK &&  pc && !pc->getNoTiming() ) {
              if (r->getNumShortening() == 0) {
                int after = r->getRunningTime() - pc->getBestLegTime(tleg);
                if (after > 0)
                  sprintf_s(bf, "+%d:%02d", after/60, after%60);
              }
              else {
                sptr = &MakeDash("-");
              }
            }
          }
        }
        else {
          int after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
          if (after > 0)
            sprintf_s(bf, "+%d:%02d", after/60, after%60);
        }
      }
      break;


    case lRunnerTimePerKM:
      if (r && !invalidClass && r->prelStatusOK()) {
        const pCourse pc = r->getCourse(false);
        if (pc) {
          int t = r->getRunningTime();
          int len = pc->getLength();
          if (len > 0 && t > 0) {
            int sperkm = (1000 * t) / len;
            strcpy_s(bf, formatTime(sperkm).c_str());
          }
        }
      }
      break;
    case lRunnerTotalTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          sptr = &r->getTotalRunningTimeS();
        else
          sptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(r->getTotalTimeInput());
      }
      break;
    case lRunnerTotalTimeStatus:
      if (invalidClass)
        sptr = &lang.tl("Struken");
      else if (r) {
        if (pp.resultModuleIndex == -1) {
          if ((r->getTotalStatus()==StatusOK || (r->getTotalStatus()==StatusUnknown 
            && r->prelStatusOK() && r->getInputStatus() == StatusOK) )&& pc && !pc->getNoTiming()) {
            sptr = &r->getTotalRunningTimeS();
            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &r->getTotalStatusS();
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          RunnerStatus input = r->getTotalStatusInput();
          if (input == StatusOK && res.getStatus() == StatusOK && pc && !pc->getNoTiming()) {
            sptr = &res.getRunningTimeS(r->getTotalTimeInput());
            if (r->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &res.getStatusS(input);
        }
      }
      break;
    case lRunnerTempTimeStatus:
      if (invalidClass)
          sptr = &lang.tl("Struken");
      else if (r) {
        if (r->tempStatus==StatusOK && pc && !pc->getNoTiming())
          strcpy_s(bf, formatTime(r->tempRT).c_str());
        else
          strcpy_s(bf, formatStatus(r->tempStatus).c_str() );
      }
      break;
    case lRunnerPlace:
      if (r && !invalidClass  && pc && !pc->getNoTiming()) {
        if (pp.resultModuleIndex == -1)
          strcpy_s(bf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        else
          sptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;
    case lRunnerTotalPlace:
      if (r && !invalidClass && pc && !pc->getNoTiming())
        strcpy_s(bf, r->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;

    case lRunnerGeneralPlace:
      if (r && !invalidClass && pc && !pc->getNoTiming()) {
        if (pp.resultModuleIndex == -1) {
          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            string iPlace;
            if (pc->getClassType() != oClassPatrol)
              iPlace = r->getPrintPlaceS(true);

            string tPlace = r->getPrintTotalPlaceS(true); 
            string v;
            if (iPlace.empty())
              v = tPlace;
            else {
              if (tPlace.empty())
                tPlace = MakeDash("-");
              v = tPlace + " (" + iPlace + ")";
            }
            strcpy_s(bf, v.c_str());
          }
          else
            strcpy_s(bf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        }
        else
          sptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lRunnerClassCoursePlace:
      if (r && !invalidClass && pc && !pc->getNoTiming()) {
        int p = r->getCoursePlace();
        if (p>0 && p<10000)
          sprintf_s(bf, "%d.", p);
      }
      break;
    case lRunnerPlaceDiff:
      if (r && !invalidClass && pc && !pc->getNoTiming()) {
        int p = r->getTotalPlace();
        if (r->getTotalStatus() == StatusOK && p > 0 && r->inputPlace>0) {
          int pd = p - r->inputPlace;
          if (pd > 0)
            sprintf_s(bf, "+%d", pd);
          else if (pd < 0)
            sprintf_s(bf, "-%d", -pd);
        }
      }
      break;
    case lRunnerTimeAfterDiff:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus()==StatusOK &&  pc && !pc->getNoTiming()) {
          int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(tleg, true);
          int afterOld = r->inputTime - pc->getBestInputTime(tleg);
          int ad = after - afterOld;
          if (ad > 0)
            sprintf_s(bf, "+%d:%02d", ad/60, ad%60);
          if (ad < 0)
            sprintf_s(bf, "-%d:%02d", (-ad)/60, (-ad)%60);
        }
      }
      break;
    case lRunnerRogainingPoint:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          sprintf_s(bf, "%d", r->getRogainingPoints(false));
        else
          sprintf_s(bf, "%d", r->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;

    case lRunnerRogainingPointTotal:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          sprintf_s(bf, "%d", r->getRogainingPoints(true));
        else
          sprintf_s(bf, "%d", r->getTempResult(pp.resultModuleIndex).getPoints() + r->getInputPoints());
      }
      break;

    case lRunnerRogainingPointReduction:
      if (r && !invalidClass) {
        int red = r->getRogainingReduction();
        if (red > 0)
          sprintf_s(bf, "-%d", red);
      }
      break;
    
    case lRunnerRogainingPointGross:
      if (r && !invalidClass) {
        int p = r->getRogainingPointsGross();
        sptr = &itos(p);
      }
      break;
    
    case lRunnerPointAdjustment:
      if (r && !invalidClass) {
        int a = r->getPointAdjustment();
        if (a<0)
          sptr = &itos(a);
        else if (a>0)
          sprintf_s(bf, "+%d", a);
      }
      break;
    case lRunnerTimeAdjustment:
      if (r && !invalidClass) {
        int a = r->getTimeAdjustment();
        if (a != 0)
          sptr = &getTimeMS(a);
      }
      break;
    case lRunnerRogainingPointOvertime:
      if (r && !invalidClass) {
        int over = r->getRogainingOvertime();
        if (over > 0)
          sptr = &formatTime(over);
      }
      break;

    case lRunnerTimeAfter:
      if (r && pc && !invalidClass && !pc->getNoTiming()) {
        int after = 0;
        if (pp.resultModuleIndex == -1) {
          int tleg=r->tLeg>=0 ? r->tLeg:0;
          int brt = pc->getBestLegTime(tleg);
          if (r->prelStatusOK() && brt > 0) {
            after=r->getRunningTime() - brt;
          }
        }
        else {
          after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
        }
  
        if (r->getNumShortening() == 0) {
          if (after > 0)
            sprintf_s(bf, "+%d:%02d", after/60, after%60);
        }
        else {
          sptr = &MakeDash("-");
        }
      }
      break;
    case lRunnerTotalTimeAfter:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus()==StatusOK &&  pc && !pc->getNoTiming()) {
          if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
            int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(tleg, true);
            if (after > 0)
              sprintf_s(bf, "+%d:%02d", after/60, after%60);
          }
          else {
            sptr = &MakeDash("-");
          }
        }
      }
      break;
    case lRunnerClassCourseTimeAfter:
      if (r && pc && !invalidClass) {
        pCourse crs = r->getCourse(false);
        if (crs && r->tStatus==StatusOK && !pc->getNoTiming()) {
          int after = r->getRunningTime() - pc->getBestTimeCourse(crs->getId());
          if (after > 0)
            sprintf_s(bf, "+%d:%02d", after/60, after%60);
        }
      }
      break;
    case lRunnerTimePlaceFixed:
      if (r && !invalidClass) {
        int t = r->getTimeWhenPlaceFixed();
        if (t == 0 || (t>0 && t < getComputerTime())) {
          strcpy_s(bf, lang.tl("klar").c_str());
        }
        else if (t == -1)
          strcpy_s(bf, "-");
        else
          strcpy_s(bf, oe->getAbsTime(t).c_str());
      }
      break;
    case lRunnerLegNumberAlpha:
    case lRunnerLegNumber:
      if (r)
        return formatSpecialStringAux(pp, par, t, r->getLegNumber(), 0, 0, counter);
      else
        strcpy_s(bf, par.getLegName().c_str());
      break;
    case lRunnerMissedTime:
      if (r && r->tStatus == StatusOK && pc && !pc->getNoTiming() && !invalidClass) {
        strcpy_s(bf, r->getMissedTimeS().c_str());
      }
      break;
    case lRunnerTempTimeAfter:
      if (r && pc) {
        if (r->tempStatus==StatusOK &&  pc && !pc->getNoTiming()
              && r->tempRT>pc->tLegLeaderTime) {
          int after=r->tempRT-pc->tLegLeaderTime;
          sprintf_s(bf, "+%d:%02d", after/60, after%60);
        }
      }
      break;

    case lRunnerCard:
      if (r && r->CardNo > 0)
        sprintf_s(bf, "%d", r->getCardNo());
      break;
    case lRunnerRank:
      if (r) {
        int rank=r->getDI().getInt("Rank");
        if (rank>0)
          strcpy_s(bf, FormatRank(rank).c_str());
      }
      break;
    case lRunnerBib:
      if (r) {
        string bib = r->getBib();
        if (!bib.empty())
          sprintf_s(bf, "%s", bib.c_str());
      }
      break;
    case lRunnerUMMasterPoint:
      if (r) {
        int total, finished, dns;
        oe->getNumClassRunners(pc->getId(), par.getLegNumber(pc), total, finished, dns);
        int percent = int(floor(0.5+double((100*(total-dns-r->getPlace()))/double(total-dns))));
        if (r->getStatus()==StatusOK)
          sprintf_s(bf, "%d",  percent);
        else
          sprintf_s(bf, "0");
      }
      break;
    case lRunnerAge:
      if (r) {
        int y = r->getBirthAge();
        if (y > 0)
          sprintf_s(bf, "%d", y);
      }
    break;
    case lRunnerBirthYear:
      if (r) {
        int y = r->getBirthYear();
        if (y > 0)
          sprintf_s(bf, "%d", y);
      }
    break;
    case lRunnerSex:
      if (r) {
        PersonSex s = r->getSex();
        if (s == sFemale)
          sptr = &lang.tl("Kvinna");
        else if (s == sMale)
          sptr = &lang.tl("Man");
      }
    break;
    case lRunnerPhone:
      if (r) {
        string s = r->getDCI().getString("Phone");
        strcpy_s(bf, s.c_str());
      }
    break;
    case lRunnerFee:
      if (r) {
        string s = formatCurrency(r->getDCI().getInt("Fee"));
        strcpy_s(bf, s.c_str());
      }
    break;
    case lTeamFee:
      if (t) {
        string s = formatCurrency(t->getTeamFee());
        strcpy_s(bf, s.c_str());
      }
    break;
    case lRunnerNationality:
      if (r) {
        string nat = r->getNationality();
        strcpy_s(bf, nat.c_str());
      }
    break;
    case lTeamName:
      //sptr = t ? &t->Name : 0;
      if (t) {
        strcpy_s(bf, t->getDisplayName().c_str());
      }
      break;
    case lTeamStart:
    case lTeamStartCond:
    case lTeamStartZero:
      if (t) {
        if ((pp.type == lTeamStartCond || pp.type == lTeamStartZero) && pc && !pc->hasFreeStart()) {
          int fs, ls;
          pc->getStartRange(legIndex, fs, ls);
          if (fs>0 && fs == ls) {
            break; // Common start time, skip
          }
        }

        if (unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex] && t->Runners[legIndex]->startTimeAvailable()) {
          if (pp.type != lTeamStartZero) 
            sptr = &t->Runners[legIndex]->getStartTimeCompact();
          else {
            int st = t->Runners[legIndex]->getStartTime();
            sptr = &getTimeMS(st-3600);
          }
        }
      }
      break;
    case lTeamStatus:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          sptr = &t->getLegStatusS(legIndex, false);
        else
          sptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
      }
      break;
    case lTeamTime:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          strcpy_s(bf, t->getLegRunningTimeS(legIndex, false).c_str() );
        else
          sptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
      }
      break;
    case lTeamTimeStatus:
      if (invalidClass)
          sptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          RunnerStatus st = t->getLegStatus(legIndex, false);
          if (st==StatusOK || (st==StatusUnknown && t->getLegRunningTime(legIndex, false)>0))
            strcpy_s(bf, t->getLegRunningTimeS(legIndex, false).c_str());
          else
            sptr = &t->getLegStatusS(legIndex, false);
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          if (st == StatusOK || st == StatusUnknown) {
            sptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
            if (t->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamRogainingPoint:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          sprintf_s(bf, "%d", t->getRogainingPoints(false));
        else
          sprintf_s(bf, "%d", t->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;
    case lTeamRogainingPointTotal:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          sprintf_s(bf, "%d", t->getRogainingPoints(true));
        else
          sprintf_s(bf, "%d", t->getTempResult(pp.resultModuleIndex).getPoints() + t->getInputPoints());
      }
      break;

    case lTeamRogainingPointReduction:
      if (t && !invalidClass) {
        int red = t->getRogainingReduction();
        if (red > 0)
          sprintf_s(bf, "-%d", red);
      }
      break;

    case lTeamRogainingPointOvertime:
      if (t && !invalidClass) {
        int over = t->getRogainingOvertime();
        if (over > 0)
          sptr = &formatTime(over);
      }
      break;

   case lTeamPointAdjustment:
      if (t && !invalidClass) {
        int a = t->getPointAdjustment();
        if (a<0)
          sptr = &itos(a);
        else if (a>0)
          sprintf_s(bf, "+%d", a);
      }
      break;

    case lTeamTimeAdjustment:
      if (t && !invalidClass) {
        int a = t->getTimeAdjustment();
        if (a != 0)
          sptr = &getTimeMS(a);
      }
      break;

    case lTeamTimeAfter:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, false)==StatusOK) {
            if (t->getNumShortening(legIndex) == 0) {
              int ta=t->getTimeAfter(legIndex);
              if (ta>0)
                sprintf_s(bf, "+%d:%02d", ta/60, ta%60);
            }
            else {
              sptr = &MakeDash("-");
            }
          }
        }
        else {
          if (t->getTempResult().getStatus() == StatusOK) {
            int after = t->getTempResult(pp.resultModuleIndex).getTimeAfter();
            if (after > 0)
              sprintf_s(bf, "+%d:%02d", after/60, after%60);
          }
        }
      }
      break;
    case lTeamPlace:
      if (t && !invalidClass && pc && !pc->getNoTiming()) {
        if (pp.resultModuleIndex == -1) {
          strcpy_s(bf, t->getLegPrintPlaceS(legIndex, false, pp.text.empty()).c_str());
        }
        else
          sptr = &t->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lTeamLegTimeStatus:
      if (invalidClass)
        sptr = &lang.tl("Struken");
      else if (t) {
        int ix = r ? r->getLegNumber() : counter.level3;
        if (t->getLegStatus(ix, false)==StatusOK)
          strcpy_s(bf, t->getLegRunningTimeS(ix, false).c_str() );
        else
          strcpy_s(bf, t->getLegStatusS(ix, false).c_str() );
      }
      break;
    case lTeamLegTimeAfter:
      if (t) {
        int ix = r ? r->getLegNumber() : counter.level3;
        if (t->getLegStatus(ix, false)==StatusOK && !invalidClass) {
          if (t->getNumShortening(ix) == 0) {
            int ta=t->getTimeAfter(ix);
            if (ta>0)
              sprintf_s(bf, "+%d:%02d", ta/60, ta%60);
          }
          else {
            sptr = &MakeDash("-");
          }
        }
      }
      break;
    case lTeamClub:
      if (t) {
        strcpy_s(bf, t->getDisplayClub().c_str());
      }
      break;
    case lTeamRunner:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex])
        sptr=&t->Runners[legIndex]->getName();
      break;
    case lTeamRunnerCard:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex]
      && t->Runners[legIndex]->getCardNo()>0)
        sprintf_s(bf, "%d", t->Runners[legIndex]->getCardNo());
      break;
    case lTeamBib:
      if (t) {
        sptr = &t->getBib();
      }
      break;
    case lTeamTotalTime:
      if (t && !invalidClass) strcpy_s(bf, t->getLegRunningTimeS(legIndex, true).c_str() );
      break;
    case lTeamTotalTimeStatus:
      if (invalidClass)
          sptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, true)==StatusOK)
            strcpy_s(bf, t->getLegRunningTimeS(legIndex, true).c_str() );
          else
            strcpy_s(bf, t->getLegStatusS(legIndex, true).c_str() );
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          RunnerStatus inp = t->getInputStatus();
          if (st == StatusOK && inp == StatusOK) {
            sptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
            if (t->getNumShortening() > 0) {
              sprintf_s(bf, "*%s", sptr->c_str());
              sptr = 0;
            }
          }
          else
            sptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamPlaceDiff:
      if (t && !invalidClass) {
        int p = t->getTotalPlace();
        if (t->getTotalStatus() == StatusOK && p > 0 && t->inputPlace>0) {
          int pd = p - t->inputPlace;
          if (pd > 0)
            sprintf_s(bf, "+%d", pd);
          else if (pd < 0)
            sprintf_s(bf, "-%d", -pd);
        }
      }
      break;
    case lTeamTotalPlace:
      if (t && !invalidClass && pc && !pc->getNoTiming()) strcpy_s(bf, t->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;

      break;
    case lTeamTotalTimeAfter:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !pc->getNoTiming()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(tleg, true);
          if (after > 0)
            sprintf_s(bf, "+%d:%02d", after/60, after%60);
        }
      }
      break;
    case lTeamTotalTimeDiff:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !pc->getNoTiming()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(tleg, true);
          int afterOld = t->inputTime - pc->getBestInputTime(tleg);
          int ad = after - afterOld;
          if (ad > 0)
            sprintf_s(bf, "+%d:%02d", ad/60, ad%60);
          if (ad < 0)
            sprintf_s(bf, "-%d:%02d", (-ad)/60, (-ad)%60);
        }
      }
      break;
    case lTeamStartNo:
      if (t)
        sprintf_s(bf, "%d", t->getStartNo());
      break;
    case lRunnerStartNo:
      if (r)
        sprintf_s(bf, "%d", r->getStartNo());
      break;
    case lNationality:
      if (r && !(sptr = &r->getDCI().getString("Nationality"))->empty())
        break;
      else if (t && !(sptr = &t->getDCI().getString("Nationality"))->empty())
        break;
      else if (c && !(sptr = &c->getDCI().getString("Nationality"))->empty())
        break;

      break;

    case lCountry:
      if (r && !(sptr = &r->getDCI().getString("Country"))->empty())
        break;
      else if (t && !(sptr = &t->getDCI().getString("Country"))->empty())
        break;
      else if (c && !(sptr = &c->getDCI().getString("Country"))->empty())
        break;

      break;
    case lPunchNamedTime:
      if (r && r->Card && r->getCourse(true) && !invalidClass) {
        const pCourse crs = r->getCourse(true);
        const oControl *ctrl=crs->getControl(counter.level3);
        if (!ctrl || ctrl->isRogaining(crs->hasRogaining()))
          break;
        if (r->getPunchTime(counter.level3, false)>0 && ctrl->hasName()) {
          sprintf_s(bf, "%s: %s (%s)", ctrl->getName().c_str(),
            r->getNamedSplitS(counter.level3).c_str(),
            r->getPunchTimeS(counter.level3, false).c_str());
        }
      }
      break;
    case lPunchTime:
    case lPunchControlNumber:
    case lPunchControlCode:
    case lPunchLostTime:
    case lPunchControlPlace:
    case lPunchControlPlaceAcc:

      if (r && r->Card && r->getCourse(true) && !invalidClass) {
        const pCourse crs=r->getCourse(true);
        int nCtrl = crs->getNumControls();
        if (counter.level3 != nCtrl) { // Always allow finish
          const oControl *ctrl=crs->getControl(counter.level3);
          if (!ctrl || ctrl->isRogaining(crs->hasRogaining()))
            break;
        }
        if (pp.type == lPunchTime) {
          if (r->getPunchTime(counter.level3, false)>0) {
            sprintf_s(bf, "%s (%s)",
              r->getSplitTimeS(counter.level3, false).c_str(),
              r->getPunchTimeS(counter.level3, false).c_str());
          }
          else {
            strcpy_s(bf, MakeDash("- (-)").c_str());
          }
        }
        else if (pp.type == lPunchControlNumber) {
          strcpy_s(bf, crs->getControlOrdinal(counter.level3).c_str());
        }
        else if (pp.type == lPunchControlCode) {
          const oControl *ctrl=crs->getControl(counter.level3);
          if (ctrl) {
            if (ctrl->getStatus() == oControl::StatusMultiple) {
              string str = ctrl->getStatusS();
              sprintf_s(bf, "%s.", str.substr(0, 1).c_str());
            }
            else
              sprintf_s(bf, "%d", ctrl->getFirstNumber());
          }
        }
        else if (pp.type == lPunchControlPlace) {
          int p = r->getLegPlace(counter.level3);
          if (p>0)
            sprintf_s(bf, "%d", p);
        }
        else if (pp.type == lPunchControlPlaceAcc) {
          int p = r->getLegPlaceAcc(counter.level3);
          if (p>0)
            sprintf_s(bf, "%d", p);
        }
        else if (pp.type == lPunchLostTime) {
          strcpy_s(bf, r->getMissedTimeS(counter.level3).c_str());
        }
      }
      break;
    case lSubSubCounter:
      if (pp.text.empty())
        sprintf_s(bf, "%d.", counter.level3+1);
      else
        sprintf_s(bf, "%d", counter.level3+1);
      break;
    case lSubCounter:
      if (pp.text.empty())
        sprintf_s(bf, "%d.", counter.level2+1);
      else
        sprintf_s(bf, "%d", counter.level2+1);
      break;
    case lTotalCounter:
      if (pp.text.empty())
        sprintf_s(bf, "%d.", counter.level1+1);
      else
        sprintf_s(bf, "%d", counter.level1+1);
      break;
    case lClubName:
      sptr = c != 0 ? &c->getDisplayName() : 0;
      break;
    case lResultModuleTime:
    case lResultModuleTimeTeam:

      if (pp.resultModuleIndex != -1) {
        if (t && pp.type == lResultModuleTimeTeam)
          sptr = &t->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
        else if (r)
          sptr = &r->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
      }
      break;

    case lResultModuleNumber:
    case lResultModuleNumberTeam:
    
      if (pp.resultModuleIndex != -1) {
        int nr = 0;
        if (t && pp.type == lResultModuleNumberTeam)
          nr = t->getTempResult(pp.resultModuleIndex).getOutputNumber(legIndex);
        else if (r)
          nr = r->getTempResult(pp.resultModuleIndex).getOutputNumber(legIndex);

        if (pp.text.empty() || pp.text[0]!='@')
          sptr = &itos(nr);
        else {
          vector<string> out;
          split(pp.text.substr(1), ";", out);
          string &res = StringCache::getInstance().get();
          size_t ix = nr;
          if (!out.empty() && ix >= out.size() && out.back().find_first_of('%') != out.back().npos) {
            ix = out.size() - 1;
          }
          if (ix < out.size()) { 
            res.swap(out[ix]);
            if (res.find_first_of('%') != res.npos) {
              char bf2[256];
              sprintf_s(bf2, res.c_str(), itos(nr).c_str());
              res = bf2;
            }
          }
          else
            res = "";

          return res;
        }
      }
      break;

    case lRogainingPunch:
      if (r && r->Card && r->getCourse(false)) {
        const pCourse crs = r->getCourse(false);
        const pPunch punch = r->Card->getPunchByIndex(counter.level3);
        if (punch && punch->tRogainingIndex>=0) {
          const pControl ctrl = crs->getControl(punch->tRogainingIndex);
          if (ctrl) {
            sprintf_s(bf, "%d, %dp, %s (%s)",
                      punch->Type, ctrl->getRogainingPoints(),
                      r->Card->getRogainingSplit(counter.level3, r->tStartTime).c_str(),
                      punch->getRunningTime(r->tStartTime).c_str());
          }
        }
      }
      break;
  }

  if (pp.type!=lString && (sptr==0 || sptr->empty()) && bf[0]==0)
    return _EmptyString;
  else if (sptr) {
    if (pp.text.empty())
      return *sptr;
    else {
      sprintf_s(bf, pp.text.c_str(), sptr->c_str());
      string &res = StringCache::getInstance().get();
      res = bf;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      string &res = StringCache::getInstance().get();
      res = bf;
      return res;
    }
    else {
      char bf2[512];
      sprintf_s(bf2, pp.text.c_str(), bf);
      string &res = StringCache::getInstance().get();
      res = bf2;
      return res;
    }
  }
}



bool oEvent::formatPrintPost(const list<oPrintPost> &ppli, PrintPostInfo &ppi,
                             const pTeam t, const pRunner r, const pClub c,
                             const pClass pc, const pCourse crs, const pControl ctrl, int legIndex)
{
  list<oPrintPost>::const_iterator ppit;
  int y=ppi.gdi.getCY();
  int x=ppi.gdi.getCX();
  bool updated=false;
  for (ppit=ppli.begin();ppit!=ppli.end();) {
    const oPrintPost &pp=*ppit;
    int limit = 0;

    bool keepNext = false;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;

    // Main increment below
    if ( ++ppit != ppli.end() && ppit->dy==pp.dy)
      limit = ppit->dx - pp.dx;
    else
      keepNext = true;

    limit=max(pp.fixedWidth, limit);

    assert(limit>=0);
    pRunner rr = r;
    if (!rr && t) {
      if (pp.legIndex >= 0) {
        int lg = pc ? pc->getLinearIndex(pp.legIndex, pp.linearLegIndex) : pp.legIndex;
        rr=t->getRunner(lg);
      }
      else if (legIndex >= 0)
        rr=t->getRunner(legIndex);
      else {
        int lg = ppi.par.getLegNumber(pc);
        rr = t->getRunner(lg);
      }
    }
    const string &text = (legIndex == -1) ? formatListString(pp, ppi.par, t, rr, c, pc, ppi.counter) :
                                            formatSpecialString(pp, ppi.par, t, legIndex, crs, ctrl, ppi.counter);
    updated |= !text.empty();
    TextInfo *ti = 0;
    if (!text.empty()) {
      if ( (pp.type == lRunnerName || pp.type == lRunnerCompleteName ||
            pp.type == lRunnerFamilyName || pp.type == lRunnerGivenName ||
            pp.type == lTeamRunner || (pp.type == lPatrolNameNames && !t)) && rr) {
        ti = &ppi.gdi.addStringUT(y+ppi.gdi.scaleLength(pp.dy), x+ppi.gdi.scaleLength(pp.dx), pp.format, text,
        ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(rr->getId());
        ti->id = "R";
      }
      else if ((pp.type == lTeamName || pp.type == lPatrolNameNames) && t) {
        ti = &ppi.gdi.addStringUT(y+ppi.gdi.scaleLength(pp.dy), x+ppi.gdi.scaleLength(pp.dx), pp.format, text,
                              ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(t->getId());
        ti->id = "T";
      }
      else {
        ti = &ppi.gdi.addStringUT(y + ppi.gdi.scaleLength(pp.dy), x + ppi.gdi.scaleLength(pp.dx),
                              pp.format, text, ppi.gdi.scaleLength(limit), 0, pp.fontFace.c_str());
      }
      if (ti && ppi.keepToghether)
        ti->lineBreakPrioity = -1;

      if (pp.color != colorDefault)
        ti->setColor(pp.color);
    }
    ppi.keepToghether |= keepNext;

  }

  return updated;
}

void oEvent::calculatePrintPostKey(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                                   const pTeam t, const pRunner r, const pClub c,
                                   const pClass pc, oCounter &counter, string &key)
{
  key.clear();
  list<oPrintPost>::const_iterator ppit;
  for (ppit=ppli.begin();ppit!=ppli.end(); ++ppit) {
    const oPrintPost &pp=*ppit;
    pRunner rr = r;
    if (!rr && t) {
      int linLeg = pp.legIndex;
      if (pc)
        linLeg = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
      rr=t->getRunner(linLeg);
    }
    const string &text = formatListString(pp, par, t, rr, c, pc, counter);
    key += text;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;
  }
}

void oEvent::listGeneratePunches(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                                 pTeam t, pRunner r, pClub club, pClass cls)
{
  if (!r || ppli.empty())
    return;

  pCourse crs=r->getCourse(true);

  if (!crs)
    return;

  if (cls && cls->getNoTiming())
    return;

  bool filterNamed = false;
  int h = gdi.getLineHeight();
  int w=0;

  for (list<oPrintPost>::const_iterator it = ppli.begin(); it != ppli.end(); ++it) {
    if (it->type == lPunchNamedTime)
      filterNamed = true;
    h = max(h, gdi.getLineHeight(it->getFont(), it->fontFace.c_str()) + gdi.scaleLength(it->dy));
    w = max(w, gdi.scaleLength(it->fixedWidth + it->dx));
  }

  int xlimit=gdi.getCX()+ gdi.scaleLength(600);

  if (w>0) {
    gdi.pushX();
    gdi.fillNone();
  }

  bool neednewline = false;
  bool updated=false;

  int limit = crs->nControls + 1;

  if (r->Card && r->Card->getNumPunches()>limit)
    limit = r->Card->getNumPunches();

  vector<char> skip(limit, false);
  if (filterNamed) {
    for (int k = 0; k < crs->nControls; k++) {
      if (crs->getControl(k) && !crs->getControl(k)->hasName())
        skip[k] = true;
    }
    for (int k = crs->nControls; k < limit; k++) {
      skip[k] = true;
    }
  }

  PrintPostInfo ppi(gdi, par);

  for (int k=0; k<limit; k++) {
    if (w>0 && updated) {
      updated=false;
      if ( gdi.getCX() + w > xlimit) {
        neednewline = false;
        gdi.popX();
        gdi.setCY(gdi.getCY() + h);
      }
      else
       gdi.setCX(gdi.getCX()+w);
    }

    if (!skip[k]) {
      updated |= formatPrintPost(ppli, ppi, t, r, club, cls, 0, 0, -1);
      neednewline |= updated;
    }

    ppi.counter.level3++;
  }

  if (w>0) {
    gdi.popX();
    gdi.fillDown();
    if (neednewline)
      gdi.setCY(gdi.getCY() + h);
  }
}

void oEvent::generateList(gdioutput &gdi, bool reEvaluate, const oListInfo &li, bool updateScrollBars) {
  if (reEvaluate)
    reEvaluateAll(set<int>(), false);

  oe->calcUseStartSeconds();
  oe->calculateNumRemainingMaps();
  oe->updateComputerTime();
  vector< pair<int, pair<string, string> > > tagNameList;
  oe->getGeneralResults(false, tagNameList, false);
  string src;
  for (size_t k = 0; k < tagNameList.size(); k++)
    oe->getGeneralResult(tagNameList[k].second.first, src).setContext(&li.lp);

  string listname;
  if (!li.Head.empty()) {
    oCounter counter;
    const string &name = formatListString(li.Head.front(), li.lp, 0, 0, 0, 0, counter);
    listname = name;
    li.lp.updateDefaultName(name);
  }
  bool addHead = !li.lp.pageBreak && !li.lp.useLargeSize;
  size_t nClassesSelected = li.lp.selection.size();
  if (nClassesSelected!=0 && nClassesSelected < min(Classes.size(), Classes.size()/2+5) ) {
    // Non-trivial class selection:
    Classes.sort();
    string cls;
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (li.lp.selection.count(it->getId())) {
        cls += MakeDash(" - ");
        cls += it->getName();
      }
    }
    listname += cls;
  }

  if (li.lp.getLegNumberCoded() != -1) {
    listname += lang.tl(" Sträcka X#" + li.lp.getLegName());
  }

  generateListInternal(gdi, li, addHead);
  
  for (list<oListInfo>::const_iterator it = li.next.begin(); it != li.next.end(); ++it) {
    bool interHead = addHead && it->getParam().showInterTitle;
    if (li.lp.pageBreak || it->lp.pageBreak) {
      gdi.dropLine(1.0);
      gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
    }
    else if (interHead)
      gdi.dropLine(1.5);

    generateListInternal(gdi, *it, interHead);
  }
  
  for (size_t k = 0; k < tagNameList.size(); k++)
    oe->getGeneralResult(tagNameList[k].second.first, src).clearContext();

  gdi.setListDescription(listname);
  if (updateScrollBars)
    gdi.updateScrollbars();
}

void oEvent::generateListInternal(gdioutput &gdi, const oListInfo &li, bool formatHead) {
  li.setupLinks();
  pClass sampleClass = 0;

  if (!li.lp.selection.empty())
    sampleClass = getClass(*li.lp.selection.begin());
  if (!sampleClass && !Classes.empty())
    sampleClass = &*Classes.begin();

  PrintPostInfo printPostInfo(gdi, li.lp);
  //oCounter counter;
  //Render header

  if (formatHead)
    formatPrintPost(li.Head, printPostInfo, 0,0,0,0,0,0, -1);

  if (li.fixedType) {
    generateFixedList(gdi, li);
    return;
  }

  // Apply for all teams (calculate start times etc.)
  for (oTeamList::iterator it=Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved() || it->tStatus == StatusNotCompetiting)
      continue;

    if (!li.lp.selection.empty() && li.lp.selection.count(it->getClassId())==0)
      continue;
    it->apply(false, 0, true);
  }

  string oldKey;
  if ( li.listType==li.EBaseTypeRunner ) {

    if (li.calcCourseClassResults)
      calculateResults(RTClassCourseResult);

    if (li.calcTotalResults) {
      if (li.calcResults) {
        calculateResults(RTClassResult);
        calculateTeamResults(false);
      }

      calculateTeamResults(true);
      calculateResults(RTTotalResult);

      if (li.sortOrder != ClassTotalResult)
        sortRunners(li.sortOrder);
    }
    else if (li.calcResults) {
      if (li.rogainingResults) {
        calculateRogainingResults();
        if (li.sortOrder != ClassPoints)
          sortRunners(li.sortOrder);
      }
      else if (li.lp.useControlIdResultTo>0 || li.lp.useControlIdResultFrom>0)
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
      else if (li.sortOrder == CourseResult) {
        calculateResults(RTCourseResult);
      }
      else {
        calculateTeamResults(false);
        calculateResults(RTClassResult);
        if (li.sortOrder != ClassResult)
          sortRunners(li.sortOrder);
      }
    }
    else
      sortRunners(li.sortOrder);

    vector<pRunner> rlist;
    rlist.reserve(Runners.size());

    for (oRunnerList::iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved() || it->tStatus == StatusNotCompetiting)
        continue;

      if (!li.lp.selection.empty() && li.lp.selection.count(it->getClassId())==0)
        continue;

      //if (it->legToRun() != li.lp.legNumber && li.lp.legNumber!=-1)
      if (!li.lp.matchLegNumber(it->getClassRef(), it->legToRun()))
        continue;

      if (li.filter(EFilterExcludeDNS))
        if (it->tStatus==StatusDNS)
          continue;

      if (li.filter(EFilterVacant)) {
        if (it->isVacant())
          continue;
      }
      if (li.filter(EFilterOnlyVacant)) {
        if (!it->isVacant())
          continue;
      }

      if (li.filter(EFilterRentCard) && it->getDI().getInt("CardFee")==0)
        continue;

      if (li.filter(EFilterHasCard) && it->getCardNo()==0)
        continue;

      if (li.filter(EFilterHasNoCard) && it->getCardNo()>0)
        continue;

      rlist.push_back(&*it);
    }

    GeneralResult *gResult = 0;
    if (!li.resultModule.empty()) {
      string src;
      gResult = &getGeneralResult(li.resultModule, src);
      oListInfo::ResultType resType = li.getResultType();
      gResult->calculateIndividualResults(rlist, resType, li.sortOrder == Custom, li.getParam().getInputNumber());

      if (li.sortOrder == SortByFinishTime || li.sortOrder == SortByFinishTimeReverse
          || li.sortOrder == SortByStartTime)
        gResult->sort(rlist, li.sortOrder);
    }

    for (size_t k = 0; k < rlist.size(); k++) {
      pRunner it = rlist[k];

      if (gResult && it->getTempResult(0).getStatus() == StatusNotCompetiting)
         continue;

      if (li.filter(EFilterHasResult)) {
        if (gResult == 0) {
          if (li.lp.useControlIdResultTo<=0 && it->tStatus==StatusUnknown)
            continue;
          else if ( (li.lp.useControlIdResultTo>0 || li.lp.useControlIdResultFrom>0) && it->tempStatus!=StatusOK)
            continue;
          else if (li.calcTotalResults && it->getTotalStatus() == StatusUnknown)
            continue;
        }
        else {
          const oAbstractRunner::TempResult &res = it->getTempResult(0);
          RunnerStatus st = res.getStatus();
          if (st==StatusUnknown)
            continue;
        }
      }
      else if (li.filter(EFilterHasPrelResult)) {
        if (gResult == 0) {
          if (li.lp.useControlIdResultTo<=0 && it->tStatus==StatusUnknown && it->getRunningTime()<=0)
            continue;
          else if ( (li.lp.useControlIdResultTo>0 || li.lp.useControlIdResultFrom>0) && it->tempStatus!=StatusOK)
            continue;
          else if (li.calcTotalResults && it->getTotalStatus() == StatusUnknown && it->getTotalRunningTime()<=0)
            continue;
        }
        else {
          const oAbstractRunner::TempResult &res = it->getTempResult(0);
          int rt = res.getRunningTime();
          RunnerStatus st = res.getStatus();
          if (st==StatusUnknown && rt<=0)
            continue;
        }
      }

      string newKey;
      printPostInfo.par.relayLegIndex = -1;
      calculatePrintPostKey(li.subHead, gdi, li.lp, it->tInTeam, &*it, it->Club, it->Class, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (li.lp.pageBreak) {
          if (!oldKey.empty())
            gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
        }
        gdi.addStringUT(pagePageInfo, it->getClass());

        oldKey.swap(newKey);
        printPostInfo.counter.level2=0;
        printPostInfo.counter.level3=0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = -1;
        formatPrintPost(li.subHead, printPostInfo, it->tInTeam, &*it, it->Club, it->Class, 0, 0, -1);
      }
      if (li.lp.filterMaxPer==0 || printPostInfo.counter.level2<li.lp.filterMaxPer) {
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = it->tLeg;
        formatPrintPost(li.listPost, printPostInfo, it->tInTeam, &*it, it->Club, it->Class, 0, 0, -1);

        if (li.listSubType==li.EBaseTypePunches) {
          listGeneratePunches(li.subListPost, gdi, li.lp, it->tInTeam, &*it, it->Club, it->Class);
        }
      }
      ++printPostInfo.counter;
    }

  }
  else if ( li.listType==li.EBaseTypeTeam ) {
    if (li.calcResults)
      calculateTeamResults(false);
    if (li.calcTotalResults)
      calculateTeamResults(true);
    if (li.rogainingResults && li.resultModule.empty())
      throw std::exception("Not implemented");
    if (li.calcCourseClassResults)
      calculateResults(RTClassCourseResult);

    if (li.resultModule.empty()) {
      pair<int, bool> legInfo = li.lp.getLegInfo(sampleClass);
      sortTeams(li.sortOrder, legInfo.first, legInfo.second);
    }
    vector<pTeam> tlist;
    tlist.reserve(Teams.size());
    for (oTeamList::iterator it=Teams.begin(); it != Teams.end(); ++it) {
      if (it->isRemoved() || it->tStatus == StatusNotCompetiting)
        continue;

      if (!li.lp.selection.empty() && li.lp.selection.count(it->getClassId())==0)
        continue;
      tlist.push_back(&*it);
    }
    GeneralResult *gResult = 0;
    if (!li.resultModule.empty()) {
      string src;
      gResult = &getGeneralResult(li.resultModule, src);
      oListInfo::ResultType resType = li.getResultType();
      gResult->calculateTeamResults(tlist, resType, li.sortOrder == Custom, li.getParam().getInputNumber());
    }
    // Range of runners to include
    int parLegRangeMin = 0, parLegRangeMax = 1000;
    pClass parLegRangeClass = 0;
    const bool needParRange = li.subFilter(ESubFilterSameParallel) 
                              || li.subFilter(ESubFilterSameParallelNotFirst);

    for (size_t k = 0; k < tlist.size(); k++) {
      pTeam it = tlist[k];
      int linearLegSpec = li.lp.getLegNumber(it->getClassRef());

      if (gResult && it->getTempResult(0).getStatus() == StatusNotCompetiting)
         continue;
 
      if (li.filter(EFilterExcludeDNS))
        if (it->tStatus==StatusDNS)
          continue;

      if (li.filter(EFilterVacant))
        if (it->isVacant())
          continue;

      if (li.filter(EFilterOnlyVacant)) {
        if (!it->isVacant())
          continue;
      }

      if ( li.filter(EFilterHasResult) ) {
        if (gResult) {
          if (it->getTempResult(0).getStatus() == StatusUnknown)
            continue;
        }
        else {
          if (it->getLegStatus(linearLegSpec, false)==StatusUnknown)
            continue;
          else if (li.calcTotalResults && it->getLegStatus(linearLegSpec, true) == StatusUnknown)
            continue;
        }
      }
      else if ( li.filter(EFilterHasPrelResult) ) {
        if (gResult) {
          if (it->getTempResult(0).getStatus() == StatusUnknown && it->getTempResult(0).getRunningTime() <= 0)
            continue;
        }
        else {
          if (it->getLegStatus(linearLegSpec, false)==StatusUnknown && it->getLegRunningTime(linearLegSpec, false)<=0)
            continue;
          else if (li.calcTotalResults && it->getLegStatus(linearLegSpec, true) == StatusUnknown && it->getTotalRunningTime()<=0)
            continue;
        }
      }

      if (needParRange && it->Class != parLegRangeClass && it->Class) {
        parLegRangeClass = it->Class;
        parLegRangeClass->getParallelRange(linearLegSpec < 0 ? 0 : linearLegSpec, parLegRangeMin, parLegRangeMax);
        if (li.subFilter(ESubFilterSameParallelNotFirst))
          parLegRangeMin++;
      }

      string newKey;
      printPostInfo.par.relayLegIndex = linearLegSpec;
      calculatePrintPostKey(li.subHead, gdi, li.lp, &*it, 0, it->Club, it->Class, printPostInfo.counter, newKey);
      if (newKey != oldKey) {
        if (li.lp.pageBreak) {
          if (!oldKey.empty())
            gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
        }
        string legInfo;
        if (linearLegSpec >= 0 && it->getClassRef()) {
          // Specified leg
          legInfo = lang.tl(", Str. X#" + li.lp.getLegName());
        }

        gdi.addStringUT(pagePageInfo, it->getClass() + legInfo); // Teamlist

        oldKey.swap(newKey);
        printPostInfo.counter.level2=0;
        printPostInfo.counter.level3=0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = linearLegSpec;
        formatPrintPost(li.subHead, printPostInfo, &*it, 0, it->Club, it->Class, 0,0,-1);
      }
      ++printPostInfo.counter;
      if (li.lp.filterMaxPer==0 || printPostInfo.counter.level2<=li.lp.filterMaxPer) {
        printPostInfo.counter.level3=0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = linearLegSpec;
        formatPrintPost(li.listPost, printPostInfo, &*it, 0, it->Club, it->Class, 0, 0, -1);

        if (li.subListPost.empty())
          continue;

        if (li.listSubType==li.EBaseTypeRunner) {
          int nr = int(it->Runners.size());
          vector<pRunner> tr;
          tr.reserve(nr);
          vector<int> usedIx(nr, -1);

          for (int k=0;k<nr;k++) {
            if (!it->Runners[k]) {

               if (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult) || 
                   li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult) ||
                   li.filter(EFilterExcludeDNS) || li.subFilter(ESubFilterExcludeDNS) ||
                   li.subFilter(ESubFilterVacant)) {
                 usedIx[k] = -2; // Skip totally
               }
              continue;
            }
            else
              usedIx[k] = -2; // Skip unless allowed after filters
            bool noResult = false;
            bool noPrelResult = false;
            bool noStart = false;
            if (gResult == 0) {
              noResult = it->Runners[k]->tStatus == StatusUnknown;
              noPrelResult = it->Runners[k]->tStatus == StatusUnknown && it->Runners[k]->getRunningTime() <= 0;
              noStart = it->Runners[k]->tStatus == StatusDNS;
              //XXX TODO Multiday
            }
            else {
              noResult = it->Runners[k]->tmpResult.status == StatusUnknown;
              noPrelResult = it->Runners[k]->tmpResult.status == StatusUnknown &&  it->Runners[k]->tmpResult.runningTime <= 0;
              noStart =  it->Runners[k]->tmpResult.status == StatusDNS;
            }

            if (noResult && (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult)))
              continue;

            if (noPrelResult && (li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult)))
              continue;

            if (noStart && (li.filter(EFilterExcludeDNS) || li.subFilter(ESubFilterExcludeDNS)))
              continue;

            if (it->Runners[k]->isVacant() && li.subFilter(ESubFilterVacant))
              continue;

            if ( (it->Runners[k]->tLeg < parLegRangeMin || it->Runners[k]->tLeg > parLegRangeMax) 
              && needParRange)
              continue;

            usedIx[k] = tr.size();
            tr.push_back(it->Runners[k]);
          }

          if (gResult) {
            gResult->sortTeamMembers(tr);

            for (size_t k = 0; k < tr.size(); k++) {
              bool suitableBreak = k<2 || (k+2)>=tr.size();
              printPostInfo.keepToghether = suitableBreak;
              printPostInfo.par.relayLegIndex = tr[k] ? tr[k]->tLeg : -1;
              formatPrintPost(li.subListPost, printPostInfo, &*it, tr[k],
                              it->Club, tr[k]->Class, 0, 0, -1);
              printPostInfo.counter.level3++;
            }
          }
          else {
            for (size_t k = 0; k < usedIx.size(); k++) {
              if (usedIx[k] == -2)
                continue; // Skip
              bool suitableBreak = k<2 || (k+2)>=usedIx.size();
              printPostInfo.keepToghether = suitableBreak;
              printPostInfo.par.relayLegIndex = k;
              if (usedIx[k] == -1) {
                pCourse crs = it->Class ? it->Class->getCourse(k, it->StartNo) :  0;
                formatPrintPost(li.subListPost, printPostInfo, &*it, 0,
                                it->Club, it->Class, crs, 0, k);
              }
              else {
                formatPrintPost(li.subListPost, printPostInfo, &*it, tr[usedIx[k]],
                              it->Club, tr[usedIx[k]]->Class, 0, 0, -1);
              }
              printPostInfo.counter.level3++;
            }
          }
        }
        else if (li.listSubType==li.EBaseTypePunches) {
          pRunner r=it->Runners.empty() ? 0:it->Runners[0];
          if (!r) continue;

          listGeneratePunches(li.subListPost, gdi, li.lp, &*it, r, it->Club, it->Class);
        }
      }
    }
  }
  else if ( li.listType==li.EBaseTypeClub ) {
    if (li.calcResults) {
      calculateTeamResults(true);
      calculateTeamResults(false);
    }
    if (li.calcCourseClassResults)
      calculateResults(RTClassCourseResult);

    pair<int, bool> info = li.lp.getLegInfo(sampleClass);
    sortTeams(li.sortOrder, info.first, info.second);
    if ( li.calcResults ) {
      if (li.lp.useControlIdResultTo>0 || li.lp.useControlIdResultFrom>0)
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
      else
        calculateResults(RTClassResult);
    }
    else sortRunners(li.sortOrder);

    Clubs.sort();
    oClubList::iterator it;

    oRunnerList::iterator rit;
    bool first = true;
    for (it = Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (li.filter(EFilterVacant)) {
        if (it->getId() == getVacantClub())
          continue;
      }

      if (li.filter(EFilterOnlyVacant)) {
        if (it->getId() != getVacantClub())
          continue;
      }

      bool startClub = false;
      pRunner pLeader = 0;

      for (rit = Runners.begin(); rit != Runners.end(); ++rit) {
        if (rit->isRemoved() || rit->tStatus == StatusNotCompetiting)
          continue;

        if (!li.lp.selection.empty() && li.lp.selection.count(rit->getClassId())==0)
          continue;

        if (!li.lp.matchLegNumber(rit->getClassRef(), rit->legToRun()))
          continue;

        if (li.filter(EFilterExcludeDNS))
          if (rit->tStatus==StatusDNS)
            continue;

        if (li.filter(EFilterHasResult)) {
          if (li.lp.useControlIdResultTo<=0 && rit->tStatus==StatusUnknown)
            continue;
          else if ((li.lp.useControlIdResultTo>0 || li.lp.useControlIdResultFrom>0) && rit->tempStatus!=StatusOK)
            continue;
          else if (li.calcTotalResults && rit->getTotalStatus() == StatusUnknown)
            continue;
        }

        if (!pLeader || pLeader->Class != rit->Class)
          pLeader = &*rit;
        if (rit->Club == &*it) {
          if (!startClub) {
            if (li.lp.pageBreak) {
              if (!first)
                gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
              else
                first = false;
            }
            gdi.addStringUT(pagePageInfo, it->getName());
            printPostInfo.counter.level2=0;
            printPostInfo.counter.level3=0;
            printPostInfo.reset();
            printPostInfo.par.relayLegIndex = -1;
            formatPrintPost(li.subHead, printPostInfo, 0, 0, &*it, 0, 0, 0, -1);
            startClub = true;
          }
          ++printPostInfo.counter;
          if (li.lp.filterMaxPer==0 || printPostInfo.counter.level2<=li.lp.filterMaxPer) {
            printPostInfo.counter.level3=0;
            printPostInfo.reset();
            printPostInfo.par.relayLegIndex = rit->tLeg;
            formatPrintPost(li.listPost, printPostInfo, 0, &*rit, &*it, rit->Class, 0, 0, -1);
            if (li.subListPost.empty())
              continue;
          }
        }
      }//Runners
    }//Clubs
  }
  else if (li.listType == oListInfo::EBaseTypeCourse) {
    Courses.sort();
    oCourseList::iterator it;
    for (it = Courses.begin(); it != Courses.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (!li.lp.selection.empty()) {
        vector<pClass> usageClass;
        it->getClasses(usageClass);

        bool used = false;
        while (!used && !usageClass.empty()) {
          used = li.lp.selection.count(usageClass.back()->getId()) > 0;
          usageClass.pop_back();
        }
        if (!used)
          continue;
      }

      string newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (li.lp.pageBreak) {
          if (!oldKey.empty())
            gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
        }
      
        oldKey.swap(newKey);
        printPostInfo.counter.level2=0;
        printPostInfo.counter.level3=0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, 0, 0, 0, 0, &*it, 0, 0);
      }
      if (li.lp.filterMaxPer==0 || printPostInfo.counter.level2<li.lp.filterMaxPer) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, 0, 0,0, 0, &*it, 0, 0);

        if (li.listSubType==li.EBaseTypeControl) {
          //TODO: listGeneratePunches(li.subListPost, gdi, li.lp, it->tInTeam, &*it, it->Club, it->Class);
        }
      }
      ++printPostInfo.counter;

    }
  }
  else if (li.listType == oListInfo::EBaseTypeControl) {
    Controls.sort();
    oControlList::iterator it;
    for (it = Controls.begin(); it != Controls.end(); ++it) {
      if (it->isRemoved())
        continue;

      string newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (li.lp.pageBreak) {
          if (!oldKey.empty())
            gdi.addStringUT(gdi.getCY()-1, 0, pageNewPage, "");
        }
      
        oldKey.swap(newKey);
        printPostInfo.counter.level2=0;
        printPostInfo.counter.level3=0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, 0, 0, 0, 0, 0, &*it, 0);
      }
      if (li.lp.filterMaxPer==0 || printPostInfo.counter.level2<li.lp.filterMaxPer) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, 0, 0,0, 0, 0, &*it, 0);
      }
      ++printPostInfo.counter;
    }
  }
}

void oEvent::fillListTypes(gdioutput &gdi, const string &name, int filter)
{
  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, filter);

  //gdi.clearList(name);
  map<EStdListType, oListInfo>::iterator it;

  vector< pair<string, size_t> > v;
  for (it=listMap.begin(); it!=listMap.end(); ++it) {
    v.push_back(make_pair(it->second.Name, it->first));
    //gdi.addItem(name, it->second.Name, it->first);
  }
  sort(v.begin(), v.end());
  gdi.addItem(name, v);
}

void oEvent::getListType(EStdListType type, oListInfo &li)
{
  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, false);
  li = listMap[type];
}


void oEvent::getListTypes(map<EStdListType, oListInfo> &listMap, int filterResults)
{
  listMap.clear();

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl("Startlista, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Resultat, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportFrom = true;
    li.supportTo = true;
    listMap[EStdResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Resultat, generell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportLarge = true;
    li.supportFrom = true;
    li.supportTo = true;
    li.calcTotalResults = true;
    listMap[EGeneralResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Rogaining, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[ERogainingInd]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Avgjorda klasser (prisutdelningslista)");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EIndPriceList]=li;
  }
  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl("Startlista, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Resultat, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolResultList]=li;
  }

  {
    oListInfo li;
    li.Name="Patrullresultat (STOR)";
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    li.largeSize = true;
    listMap[EStdPatrolResultListLARGE]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Resultat (STOR)");
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportFrom = true;
    li.supportTo = true;
    li.largeSize = true;
    li.listType=li.EBaseTypeRunner;
    listMap[EStdResultListLARGE]=li;
  }

  /*{
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, sträcka (STOR)");
    li.supportClasses = true;
    li.supportLegs = true;
    li.largeSize = true;
    li.listType=li.EBaseTypeTeam;
    listMap[EStdTeamResultListLegLARGE]=li;
  }*/

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl("Hyrbricksrapport");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = false;
    li.supportLegs = false;
    listMap[EStdRentedCard]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, delsträckor");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdTeamResultListAll]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, lag");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdTeamResultList]=li;
  }
  /*
  {
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, sträcka");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdTeamResultListLeg]=li;
  }*/

  if (!filterResults) {
    {
      oListInfo li;
      li.Name=lang.tl("Startlista, stafett (lag)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EStdTeamStartList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl("Startlista, stafett (sträcka)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdTeamStartListLeg]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl("Bantilldelning, stafett");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[ETeamCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl("Bantilldelning, individuell");
      li.listType=li.EBaseTypeRunner;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EIndCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl("Individuell startlista, visst lopp");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdIndMultiStartListLeg]=li;
    }
  }

  {
    oListInfo li;
    li.Name=lang.tl("Individuell resultatlista, visst lopp");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdIndMultiResultListLeg]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Individuell resultatlista, visst lopp (STOR)");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    li.largeSize = true;
    listMap[EStdIndMultiResultListLegLARGE]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl("Individuell resultatlista, alla lopp");
    li.listType = li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdIndMultiResultListAll]=li;
  }

  if (!filterResults) {
    oListInfo li;
    li.Name = lang.tl("Klubbstartlista");
    li.listType = li.EBaseTypeClub;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubStartList]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl("Klubbresultatlista");
    li.listType = li.EBaseTypeClub;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubResultList]=li;
  }

  if (!filterResults) {
    {
      oListInfo li;
      li.Name=lang.tl("Tävlingsrapport");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl("Kontroll inför tävlingen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedPreReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl("Kvar-i-skogen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInForest]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl("Fakturor");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInvoices]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl("Ekonomisk sammanställning");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedEconomy]=li;
    }
  }
  /*
  {
    oListInfo li;
    li.Name=lang.tl("Först-i-mål, klassvis");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedResultFinishPerClass]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Först-i-mål, gemensam");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedResultFinish]=li;
  }*/

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl("Minutstartlista");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedMinuteStartlist]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Händelser - tidslinje");
    li.supportClasses = true;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedTimeLine]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Liveresultat, deltagare");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = false;
    li.supportFrom = true;
    li.supportTo = true;
    li.supportSplitAnalysis = false;
    li.supportInterResults = false;
    li.supportPageBreak = false;
    li.supportClassLimit = false;
    li.supportCustomTitle = false;
  
    listMap[EFixedLiveResult]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, sträcka (STOR)");
    li.supportClasses = true;
    li.supportLegs = false;
    li.largeSize = true;
    li.listType=li.EBaseTypeTeam;
    listMap[EStdTeamAllLegLARGE]=li;
  }

  getListContainer().setupListInfo(EFirstLoadedList, listMap, filterResults != 0);
}


void oEvent::generateListInfo(EStdListType lt, const gdioutput &gdi, int classId, oListInfo &li)
{
  oListParam par;

  if (classId!=0)
    par.selection.insert(classId);

  par.listCode=lt;

  generateListInfo(par, gdi.getLineHeight(), li);
}

int openRunnerTeamCB(gdioutput *gdi, int type, void *data);

void oEvent::generateFixedList(gdioutput &gdi, const oListInfo &li)
{
  string dmy;
  switch (li.lp.listCode) {
    case EFixedPreReport:
      generatePreReport(gdi);
    break;

    case EFixedReport:
      generateCompetitionReport(gdi);
    break;

    case EFixedInForest:
      generateInForestList(gdi, openRunnerTeamCB, 0);
    break;

    case EFixedEconomy:
      printInvoices(gdi, IPTAllPrint, dmy, true);
    break;

    case EFixedInvoices:
      printInvoices(gdi, IPTAllPrint, dmy, false);
    break;

    case EFixedMinuteStartlist:
      generateMinuteStartlist(gdi);
    break;

    case EFixedLiveResult:


    break;

    case EFixedTimeLine:
      gdi.clearPage(false);
      gdi.addString("", boldLarge, MakeDash("Tidslinje - X#" + getName()));

      gdi.dropLine();
      set<__int64> stored;
      vector<oTimeLine> events;

      map<int, string> cName;
      for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
        if (!it->isRemoved())
          cName[it->getId()] = it->getName();
      }

      oe->getTimeLineEvents(li.lp.selection, events, stored, 3600*24*7);
      gdi.fillDown();
      int yp = gdi.getCY();
      int xp = gdi.getCX();

      int w1 = gdi.scaleLength(60);
      int w = gdi.scaleLength(110);
      int w2 = w1+w;

      for (size_t k = 0; k<events.size(); k++) {
        const oTimeLine &ev = events[k];

        pRunner r = dynamic_cast<pRunner>(ev.getSource(*this));
        if (!r)
          continue;

        if (ev.getType() == oTimeLine::TLTFinish && r->getStatus() != StatusOK)
          continue;

        string name = "";
        if (r)
          name = r->getCompleteIdentification() + " ";

        gdi.addStringUT(yp, xp, 0, oe->getAbsTime(ev.getTime()));
        gdi.addStringUT(yp, xp + w1, 0, cName[ev.getClassId()], w-10);
        gdi.addStringUT(yp, xp + w2, 0, name + lang.tl(ev.getMessage()));

        yp += gdi.getLineHeight();


        /*string detail = ev.getDetail();

        if (detail.size() > 0) {
          gdi.addStringUT(yp, xp + w, 0, detail);
          yp += gdi.getLineHeight();
        }*/

      }
      gdi.refresh();

    break;
  }
}

void oListInfo::setCallback(GUICALLBACK cb) {
  lp.cb=cb;
  for (list<oListInfo>::iterator it = next.begin(); it != next.end(); ++it) {
    it->setCallback(cb);
  }
}

void oEvent::generateListInfo(oListParam &par, int lineHeight, oListInfo &li) {
  vector<oListParam> parV(1, par);
  generateListInfo(parV, lineHeight, li);
}

void oEvent::generateListInfo(vector<oListParam> &par, int lineHeight, oListInfo &li) {
  loadGeneralResults(false);
  lineHeight = 14;
  for (size_t k = 0; k < par.size(); k++) {
    par[k].cb = 0;
  }

  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, false);

  if (par.size() == 1) {
    generateListInfoAux(par[0], lineHeight, li, listMap[par[0].listCode].Name);
    set<int> used;
    // Add linked lists
    oListParam *cPar = &par[0];
    while (cPar->nextList>0) {
      if (used.count(cPar->nextList))
        break; // Circular definition
      used.insert(cPar->nextList);
      used.insert(cPar->previousList);
      oListParam &nextPar = oe->getListContainer().getParam(cPar->nextList-1);
      li.next.push_back(oListInfo());
      nextPar.cb = 0;
      generateListInfoAux(nextPar, lineHeight, li.next.back(), "");
      cPar = &nextPar;
    }
  }
  else {
    for (size_t k = 0; k < par.size(); k++) {
      if (k > 0) {
        li.next.push_back(oListInfo());
      }
      generateListInfoAux(par[k], lineHeight, k == 0 ? li : li.next.back(), 
                          li.Name = listMap[par[0].listCode].Name);
    }
  }
}

void oEvent::generateListInfoAux(oListParam &par, int lineHeight, oListInfo &li, const string &name) {
  const int lh=lineHeight;
  const int vspace=lh/2;
  int bib;
  pair<int, bool> ln;
  const double scale = 1.8;

  string ttl;
  Position pos;

  pClass sampleClass = 0;
  if (!par.selection.empty())
    sampleClass = getClass(*par.selection.begin());
  if (!sampleClass && !Classes.empty())
    sampleClass = &*Classes.begin();

  EStdListType lt=par.listCode;
  li=oListInfo();
  li.lp = par;
  li.Name = name;
  li.lp.defaultName = li.Name;
  if (par.defaultName.empty())
    par.defaultName = li.Name;
  if (par.name.empty())
    par.name = li.Name;
  if (li.lp.name.empty())
    li.lp.name = li.Name;

  switch (lt) {
    case EStdStartList: {
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Startlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      int bib = 0;
      int rank = 0;
      if (hasBib(true, false)) {
        li.addListPost(oPrintPost(lRunnerBib, "", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lRunnerStart, "", normalText, 0+bib, 0));
      li.addListPost(oPrintPost(lPatrolNameNames, "", normalText, 70+bib, 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, "", normalText, 300+bib, 0));
      if (hasRank()) {
        li.addListPost(oPrintPost(lRunnerRank, "", normalText, 470+bib, 0));
        rank = 50;
      }
      li.addListPost(oPrintPost(lRunnerCard, "", normalText, 470+bib+rank, 0));

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 12));
      li.addSubHead(oPrintPost(lClassLength, lang.tl("%s meter"), boldText, 300+bib, 12));
      li.addSubHead(oPrintPost(lClassStartName, "", boldText, 470+bib+rank, 12));
      li.addSubHead(oPrintPost(lString, "", boldText, 470+bib, 16));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;
    }

    case EStdClubStartList: {
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Klubbstartlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      if (hasBib(true, true)) {
        pos.add("bib", 40);
        li.addListPost(oPrintPost(lRunnerBib, "", normalText, 0, 0));
      }

      pos.add("start", li.getMaxCharWidth(this, par.selection, lRunnerStart, "", normalText));
      li.addListPost(oPrintPost(lRunnerStart, "", normalText, pos.get("start"), 0));

      if (hasRank()) {
        pos.add("rank", 50);
        li.addListPost(oPrintPost(lRunnerRank, "", normalText, pos.get("rank"), 0));
      }
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, "", normalText));
      li.addListPost(oPrintPost(lPatrolNameNames, "", normalText, pos.get("name"), 0));
      pos.add("class", li.getMaxCharWidth(this, par.selection, lClassName, "", normalText));
      li.addListPost(oPrintPost(lClassName, "", normalText, pos.get("class"), 0));
      pos.add("length", li.getMaxCharWidth(this, par.selection, lClassLength, "%s m", normalText));
      li.addListPost(oPrintPost(lClassLength, lang.tl("%s m"), normalText, pos.get("length"), 0));
      pos.add("sname", li.getMaxCharWidth(this, par.selection, lClassStartName, "", normalText));
      li.addListPost(oPrintPost(lClassStartName, "", normalText, pos.get("sname"), 0));

      pos.add("card", 70);
      li.addListPost(oPrintPost(lRunnerCard, "", normalText, pos.get("card"), 0));

      li.addSubHead(oPrintPost(lClubName, "", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, "", boldText, 100, 16));

      li.listType=li.EBaseTypeClub;
      li.sortOrder=ClassTeamLeg;
      li.setFilter(EFilterExcludeDNS);
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdClubResultList: {
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Klubbresultatlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      pos.add("class", li.getMaxCharWidth(this, par.selection, lClassName, "", normalText));
      li.addListPost(oPrintPost(lClassName, "", normalText, pos.get("class"), 0));
       
      pos.add("place", 40);
      li.addListPost(oPrintPost(lRunnerPlace, "", normalText, pos.get("place"), 0));

      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, "", normalText));
      li.addListPost(oPrintPost(lRunnerName, "", normalText, pos.get("name"), 0));

      pos.add("time", li.getMaxCharWidth(this, par.selection, lRunnerTimeStatus, "", normalText));
      li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, pos.get("time"), 0));

      pos.add("after", li.getMaxCharWidth(this, par.selection, lRunnerTimeAfter, "", normalText));
      li.addListPost(oPrintPost(lRunnerTimeAfter, "", normalText, pos.get("after"), 0));

      li.addSubHead(oPrintPost(lClubName, "", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, "", boldText, 100, 16));

      li.listType=li.EBaseTypeClub;
      li.sortOrder=ClassResult;
      li.calcResults = true;
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdRentedCard:
    {
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Hyrbricksrapport - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      li.addListPost(oPrintPost(lTotalCounter, "%s", normalText, 0, 0));
      li.addListPost(oPrintPost(lRunnerCard, "", normalText, 30, 0));
      li.addListPost(oPrintPost(lRunnerName, "", normalText, 130, 0));
      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));

      li.setFilter(EFilterHasCard);
      li.setFilter(EFilterRentCard);
      li.setFilter(EFilterExcludeDNS);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      break;
    }

    case EStdResultList: {
      string stitle;
      getResultTitle(*this, li.lp, stitle);
      stitle = par.getCustomTitle(stitle);

      li.addHead(oPrintPost(lCmpName, MakeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, "", normalText, 0, false, 25));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, "", normalText, 0, false, 25));
      pos.add("status", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));

      li.addListPost(oPrintPost(lRunnerPlace, "", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, "", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, "", normalText, pos.get("club"), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, pos.get("club"), 10));

        li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTimeAfter, "", normalText, pos.get("after"), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, "", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, "", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypePunches;
        }
      }
      else {
        li.needPunches = true;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, "", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, "", normalText, pos.get("after"), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldText, pos.get("after"), 10));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", normalText, pos.get("missed"), 0));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldText, pos.get("missed"), 10));
      }

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.supportFrom = true;
      li.supportTo = true;
      li.setFilter(EFilterHasPrelResult);
      break;
    }
    case EGeneralResultList: {
      string stitle;
      getResultTitle(*this, li.lp, stitle);
      stitle = par.getCustomTitle(stitle);

      gdiFonts normal, header, small;
      double s;

      if (par.useLargeSize) {
        s = scale;
        normal = fontLarge;
        header = boldLarge;
        small = normalText;
      }
      else {
        s = 1.0;
        normal = normalText;
        header = boldText;
        small = italicSmall;
      }

      li.addHead(oPrintPost(lCmpName, MakeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", li.getMaxCharWidth(this, par.selection, lRunnerGeneralPlace, "", normalText));
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerCompleteName, "", normalText));
      pos.add("status", li.getMaxCharWidth(this, par.selection, lRunnerGeneralTimeStatus, "", normalText));
      pos.add("after", li.getMaxCharWidth(this, par.selection, lRunnerGeneralTimeAfter, "", normalText));
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, "", header, 0, 10));

      li.addListPost(oPrintPost(lRunnerGeneralPlace, "", normal, pos.get("place", s), 0));
      li.addListPost(oPrintPost(lRunnerCompleteName, "", normal, pos.get("name", s), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addListPost(oPrintPost(lRunnerGeneralTimeStatus, "", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerGeneralTimeAfter, "", normal, pos.get("after", s), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, "", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, "", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypePunches;
        }
      }
      else {
        li.needPunches = true;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, "", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, "", normal, pos.get("after", s), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), header, pos.get("status", s), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), header, pos.get("after", s), 10));

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasPrelResult);
      li.supportFrom = true;
      li.supportTo = true;
      li.calcTotalResults = true;
      break;
    }
    case EIndPriceList:

      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Avgjorda placeringar - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCurrentTime, lang.tl("Genererad: ") + "%s", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, "", normalText));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, "", normalText));
      pos.add("status", 80);
      pos.add("info", 80);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, pos.get("club"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Avgörs kl"), boldText, pos.get("info"), 10));

      li.addListPost(oPrintPost(lRunnerPlace, "", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, "", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, "", normalText, pos.get("club"), 0));
      li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, pos.get("status"), 0));
      li.addListPost(oPrintPost(lRunnerTimePlaceFixed, "", normalText, pos.get("info"), 0));

      li.calcResults = true;
      li.listType = li.EBaseTypeRunner;
      li.sortOrder = ClassResult;
      li.setFilter(EFilterHasResult);
      break;

    case EStdTeamResultList:
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Resultatsammanställning - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", normalText, 280, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, "", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, "", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, 280, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, 340, 5, make_pair(-1, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;

    case EStdTeamResultListAll:
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, 280, 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, 400, 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldText, 460, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, "", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, "", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, 400, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, 460, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, "%s.", normalText, 25, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerName, "", normalText, 45, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, "", normalText, 280, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, "", normalText, 400, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, "", normalText, 460, 0, make_pair(0, true)));

      if (li.lp.splitAnalysis)  {
        li.addSubListPost(oPrintPost(lRunnerMissedTime, "", normalText, 510, 0));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldText, 510, 14));
      }

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;

    case unused_EStdTeamResultListLeg: {
      char title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        sprintf_s(title, (lang.tl("Resultat efter sträcka X#" + li.lp.getLegName())+" - %%s").c_str());
      else
        sprintf_s(title, (lang.tl("Resultat efter sträckan")+" - %%s").c_str());

      pos.add("place", 25);
      pos.add("team", li.getMaxCharWidth(this, par.selection, lTeamName, "", normalText));
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, "", normalText));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addHead(oPrintPost(lCmpName, MakeDash(title), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, "", boldText, pos.get("place"), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, pos.get("name"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, pos.get("status"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Totalt"), boldText, pos.get("teamstatus"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldText, pos.get("after"), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, "", normalText, pos.get("place"), 2, ln));
      li.addListPost(oPrintPost(lTeamName, "", normalText, pos.get("team"), 2, ln));
      li.addListPost(oPrintPost(lRunnerName, "", normalText, pos.get("name"), 2, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, pos.get("status"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, pos.get("teamstatus"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, pos.get("after"), 2, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", normalText, pos.get("missed"), 2, ln));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldText, pos.get("missed"), 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    }
    case unused_EStdTeamResultListLegLARGE: {
      char title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        sprintf_s(title, ("%%s - "+lang.tl("sträcka X#" + li.lp.getLegName())).c_str());
      else
        sprintf_s(title, ("%%s - "+lang.tl("slutsträckan")).c_str());

      pos.add("place", 25);
      pos.add("team", min(120, li.getMaxCharWidth(this, par.selection, lTeamName, "", normalText)));
      pos.add("name", min(120, li.getMaxCharWidth(this, par.selection, lRunnerName, "", normalText)));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, MakeDash(title), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldLarge, pos.get("name", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldLarge, pos.get("after", scale), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, "", fontLarge, pos.get("place", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamName, "", fontLarge, pos.get("team", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerName, "", fontLarge, pos.get("name", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, "", fontLarge, pos.get("status", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", fontLarge, pos.get("teamstatus", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", fontLarge, pos.get("after", scale), 5, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", fontLarge, pos.get("missed", scale), 5, ln));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldLarge, pos.get("missed", scale), 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    }
    case EStdTeamStartList:
      {
        MetaList mList;
        mList.setListName("Startlista, stafett");
        mList.addToHead(lCmpName).setText("Startlista - X");;
        mList.newHead();
        mList.addToHead(lCmpDate);

        mList.addToSubHead(lClassName);
        mList.addToSubHead(MetaListPost(lClassStartTime, lNone));
        mList.addToSubHead(MetaListPost(lClassLength, lNone)).setText("X meter");
        mList.addToSubHead(MetaListPost(lClassStartName, lNone));

        mList.addToList(lTeamBib);
        mList.addToList(lTeamStartCond);
        mList.addToList(lTeamName);

        mList.addToSubList(lRunnerLegNumberAlpha).align(lTeamName, false).setText("X.");
        mList.addToSubList(lRunnerName);
        mList.addToSubList(lRunnerCard).align(lClassStartName);

        mList.interpret(this, gdibase, par, lh, li);
      }
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      //li.setFilter(EFilterExcludeDNS);
      li.setSubFilter(ESubFilterVacant);
      li.sortOrder = ClassStartTime;
      break;

    case ETeamCourseList:
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      li.listType=li.EBaseTypeTeam;

      bib=0;
      li.addListPost(oPrintPost(lTeamBib, "", normalText, 0+bib, 4));
      li.addListPost(oPrintPost(lTeamName, "", normalText, 50+bib, 4));
      li.addListPost(oPrintPost(lTeamClub, "", normalText, 300+bib, 4));

      li.listSubType=li.EBaseTypeRunner;
      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, "%s.", normalText, 25+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerName, "", normalText, 50+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerCard, "", normalText, 300+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerCourse, "", normalText, 400+bib, 0, make_pair(0, true)));

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;

    case EIndCourseList: {
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      li.listType=li.EBaseTypeRunner;

      bib=0;
      li.addListPost(oPrintPost(lRunnerBib, "", normalText, 0+bib, 0));
      oPrintPost &rn = li.addListPost(oPrintPost(lRunnerName, "", normalText, 50+bib, 0));
      rn.doMergeNext = true;
      li.addListPost(oPrintPost(lRunnerClub, " (%s)", normalText, 50+bib, 0));
      li.addListPost(oPrintPost(lRunnerCard, "", normalText, 300+bib, 0));
      li.addListPost(oPrintPost(lRunnerCourse, "", normalText, 400+bib, 0));

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;
    }
    case EStdTeamStartListLeg: {
      char title[256];
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      sprintf_s(title, lang.tl("Startlista %%s - sträcka X#" + li.lp.getLegName()).c_str());

      li.addHead(oPrintPost(lCmpName, MakeDash(title), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      ln=li.lp.getLegInfo(sampleClass);
      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, "", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, "", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamName, "", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, "", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, "", normalText, 520+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassStartName, "", normalText, 300+bib, 10));

      li.sortOrder=ClassStartTime;
      //li.setFilter(EFilterExcludeDNS);
      break;
    }
    case EStdIndMultiStartListLeg:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      //sprintf_s(title, lang.tl("Startlista lopp %d - %%s").c_str(), li.lp.legNumber+1);
      ln=li.lp.getLegInfo(sampleClass);

      ttl = MakeDash(lang.tl("Startlista lopp X - Y#" + li.lp.getLegName() + "#%s"));
      li.addHead(oPrintPost(lCmpName, ttl, boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));

      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, "", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, "", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, "", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamClub, "", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, "", normalText, 500+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassLength, lang.tl("%s meter"), boldText, 300+bib, 10, ln));
      li.addSubHead(oPrintPost(lClassStartName, "", boldText, 500+bib, 10, ln));

      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;

    case EStdIndMultiResultListLeg:
      ln=li.lp.getLegInfo(sampleClass);

      if (li.lp.getLegNumberCoded() != 1000)
        ttl = lang.tl("Resultat lopp X - Y#" + li.lp.getLegName() + "#%s");
      else
        ttl = lang.tl("Resultat - X#%s");

      li.addHead(oPrintPost(lCmpName, MakeDash(ttl), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, 260, 14));

      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, 460, 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Totalt"), boldText, 510, 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldText, 560, 14));

      li.addListPost(oPrintPost(lTeamPlace, "", normalText, 0, 0, ln));
      li.addListPost(oPrintPost(lRunnerName, "", normalText, 40, 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, "", normalText, 260, 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, 460, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, 510, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, 560, 0, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", normalText, 620, 0, ln));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldText, 620, 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;

    case EStdIndMultiResultListLegLARGE:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      ln=li.lp.getLegInfo(sampleClass);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, "", normalText));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lRunnerClub, "", normalText));

      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);

      ttl = "%s - " + lang.tl("Lopp ") + li.lp.getLegName();
      li.addSubHead(oPrintPost(lClassName, MakeDash(ttl), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldLarge, pos.get("club", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldLarge, pos.get("after", scale), 14));

      li.addListPost(oPrintPost(lTeamPlace, "", fontLarge, pos.get("place", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerName, "", fontLarge, pos.get("name", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, "", fontLarge, pos.get("club", scale), 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, "", fontLarge, pos.get("status", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", fontLarge, pos.get("teamstatus", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", fontLarge, pos.get("after", scale), 0, ln));

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;

    case EStdIndMultiResultListAll:
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, "", boldText, 280, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl("Tid"), boldText, 480, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl("Efter"), boldText, 540, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, "", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerName, "", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerClub, "", normalText, 280, 5, make_pair(-1, true)));

      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, 480, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, 540, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lSubSubCounter, lang.tl("Lopp %s"), normalText, 25, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, "", normalText, 90, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, "", normalText, 150, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, "", normalText, 210, 0, make_pair(0, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    case EStdPatrolStartList:
    {
      MetaList mList;
      mList.setListName("Startlista, patrull");

      mList.addToHead(lCmpName).setText("Startlista - X").align(false);
      mList.newHead();
      mList.addToHead(lCmpDate).align(false);

      mList.addToSubHead(lClassName);
      mList.addToSubHead(lClassStartTime);
      mList.addToSubHead(lClassLength).setText("X meter");
      mList.addToSubHead(lClassStartName);

      mList.addToList(lTeamBib);
      mList.addToList(lTeamStartCond).setLeg(0);
      mList.addToList(lTeamName);

      mList.newListRow();

      mList.addToList(MetaListPost(lTeamRunner, lTeamName, 0));
      mList.addToList(MetaListPost(lTeamRunnerCard, lAlignNext, 0));
      mList.addToList(MetaListPost(lTeamRunner, lAlignNext, 1));
      mList.addToList(MetaListPost(lTeamRunnerCard, lAlignNext, 1));

      mList.setListType(li.EBaseTypeTeam);
      mList.setSortOrder(ClassStartTime);
      mList.addFilter(EFilterExcludeDNS);

/*      xmlparser xfoo, xbar;
      xfoo.openMemoryOutput(true);

      mList.save(xfoo);

      string res;
      xfoo.getMemoryOutput(res);
      xbar.readMemory(res, 0);

      MetaList mList2;
      mList2.load(xbar.getObject("MeOSListDefinition"));
*/
      mList.interpret(this, gdibase, par, lh, li);
      break;
    }
    case EStdPatrolResultList:
      li.addHead(oPrintPost(lCmpName, MakeDash(lang.tl("Resultatlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lTeamPlace, "", normalText, 0, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, "", normalText, 70, vspace));
      //li.addListPost(oPrintPost(lTeamClub, "", normalText, 250, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", normalText, 400, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", normalText, 460, vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lTeamRunner, "", normalText, 70, lh+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, "", normalText, 250, lh+vspace, make_pair(1, true)));
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, "", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, 400, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldText, 460, 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, "", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=160;
        li.listSubType=li.EBaseTypePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", normalText, 520, vspace, make_pair(1, true)));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldText, 520, 10));
      }

      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      break;

    case EStdPatrolResultListLARGE:
      pos.add("place", 25);
      pos.add("team", int(0.7*max(li.getMaxCharWidth(this, par.selection, lTeamName, "", normalText),
                        li.getMaxCharWidth(this, par.selection, lPatrolNameNames, "", normalText))));

      pos.add("status", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lTeamPlace, "", fontLarge, pos.get("place", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, "", fontLarge, pos.get("team", scale), vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", fontLarge, pos.get("status", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, "", fontLarge, pos.get("after", scale), vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lPatrolNameNames, "", fontLarge, pos.get("team", scale), 25+vspace, make_pair(0, true)));
      //li.addListPost(oPrintPost(lTeamRunner, "", fontLarge, pos.get("status", scale), 25+vspace, 1));

      li.addSubHead(oPrintPost(lClassName, "", boldLarge, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldLarge, pos.get("status", scale), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Efter"), boldLarge, pos.get("after", scale), 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, "", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=200;
        li.listSubType=li.EBaseTypePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerMissedTime, "", fontLarge, pos.get("missed", scale), vspace, make_pair(0, true)));
        li.addSubHead(oPrintPost(lString, lang.tl("Bomtid"), boldLarge, pos.get("missed", scale), 10));
      }

      li.setFilter(EFilterHasResult);
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      break;

    case unused_EStdRaidResultListLARGE:
      li.addListPost(oPrintPost(lTeamPlace, "", fontLarge, 0, vspace));
      li.addListPost(oPrintPost(lTeamName, "", fontLarge, 40, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, "", fontLarge, 490, vspace));

      li.addListPost(oPrintPost(lTeamRunner, "", fontLarge, 40, 25+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, "", fontLarge, 300, 25+vspace, make_pair(1, true)));

      li.addSubListPost(oPrintPost(lPunchNamedTime, "", fontMedium, 0, 2, make_pair(1, true)));
      li.subListPost.back().fixedWidth=200;
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, "", boldLarge, 0, 10));

      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypePunches;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case EStdResultListLARGE:
      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, "", normalText, 0, true));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, "", normalText, 0, true));
      pos.add("status", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lRunnerPlace, "", fontLarge, pos.get("place", scale), vspace));
      li.addListPost(oPrintPost(lPatrolNameNames, "", fontLarge, pos.get("name", scale), vspace));
      li.addListPost(oPrintPost(lPatrolClubNameNames, "", fontLarge, pos.get("club", scale), vspace));

      li.addSubHead(oPrintPost(lClassName, "", boldLarge, pos.get("place", scale), 10));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, "", boldLarge, pos.get("club", scale), 10));
        li.addListPost(oPrintPost(lRunnerTimeStatus, "", fontLarge, pos.get("status", scale), vspace));
      }
      else {
        li.needPunches = true;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, "", normalText, pos.get("status", scale), vspace));
      }
      if (li.lp.splitAnalysis) {
        li.addSubHead(oPrintPost(lString, "Bomtid", boldLarge, pos.get("missed", scale), 10));
        li.addListPost(oPrintPost(lRunnerMissedTime, "", fontLarge, pos.get("missed", scale), vspace));
      }

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, "", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 160;
        li.listSubType = li.EBaseTypePunches;
      }
      else if (li.lp.showSplitTimes) {
        li.addSubListPost(oPrintPost(lPunchTime, "", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 95;
        li.listSubType = li.EBaseTypePunches;
      }

      li.setFilter(EFilterHasResult);
      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;

      li.supportFrom = true;
      li.supportTo = true;
      break;

   case EStdUM_Master:
      li.addListPost(oPrintPost(lRunnerPlace, "", fontMedium, 0, 0));
      li.addListPost(oPrintPost(lRunnerName, "", fontMedium, 40, 0));
      li.addListPost(oPrintPost(lRunnerClub, "", fontMedium, 250, 0));
      li.addListPost(oPrintPost(lClassName, "", fontMedium, 490, 0));
      li.addListPost(oPrintPost(lRunnerUMMasterPoint, "", fontMedium, 580, 0));

      li.setFilter(EFilterHasResult);

      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case ERogainingInd:
      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerCompleteName, "", normalText, 0, true));
      pos.add("points", 50);
      pos.add("status", 50);
      li.addHead(oPrintPost(lCmpName, MakeDash(par.getCustomTitle(lang.tl("Rogainingresultat - %s"))), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, "", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lRunnerPlace, "", normalText, pos.get("place"), vspace));
      li.addListPost(oPrintPost(lRunnerCompleteName, "", normalText, pos.get("name"), vspace));
      li.addListPost(oPrintPost(lRunnerRogainingPoint, "%sp", normalText, pos.get("points"), vspace));
      li.addListPost(oPrintPost(lRunnerTimeStatus, "", normalText, pos.get("status"), vspace));

      li.setFilter(EFilterHasResult);

      if (li.lp.splitAnalysis || li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lRogainingPunch, "", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=130;
        li.listSubType=li.EBaseTypePunches;
      }

      li.addSubHead(oPrintPost(lClassName, "", boldText, pos.get("place"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Poäng"), boldText, pos.get("points"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl("Tid"), boldText, pos.get("status"), 10));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder = ClassPoints;
      li.lp.setLegNumberCoded(-1);
      li.calcResults = true;
      li.rogainingResults = true;
    break;

    case EStdTeamAllLegLARGE: {
      vector< pair<string, size_t> > out;
      fillLegNumbers(par.selection, false, false, out);
      par.listCode = oe->getListContainer().getType("legresult");
      par.useLargeSize = true;
      for (size_t k = 0; k < out.size(); k++) {
        if (out[k].second >= 1000)
          continue;
        par.setLegNumberCoded(out[k].second);
        if (k == 0)
          generateListInfo(par, lineHeight, li);
        else {
          li.next.push_back(oListInfo());
          generateListInfo(par, lineHeight, li.next.back());
        }
      }
    }
    break;

    case EFixedPreReport:
    case EFixedReport:
    case EFixedInForest:
    case EFixedEconomy:
    case EFixedInvoices:
    case EFixedMinuteStartlist:
    case EFixedTimeLine:
    case EFixedLiveResult:
      li.fixedType = true;
    break;

    default:
      if (!getListContainer().interpret(this, gdibase, par, lineHeight, li))
        throw std::exception("Not implemented");
  }
}

string oPrintPost::encodeFont(const string &face, int factor) {
  string out(face);
  if (factor > 0 && factor != 100) {
    out += ";" + itos(factor/100) + "." + itos(factor%100);
  }
  return out;
}
void oListInfo::setupLinks(const list<oPrintPost> &lst) const {
  for (list<oPrintPost>::const_iterator it = lst.begin(); it != lst.end(); ++it) {
    list<oPrintPost>::const_iterator itNext = it;
    ++itNext;
    if (itNext != lst.end() && it->doMergeNext)
      it->mergeWithTmp = &*itNext;
    else
      it->mergeWithTmp = 0;
  }
}

void oListInfo::setupLinks() const {
  setupLinks(Head);
  setupLinks(subHead);
  setupLinks(listPost);
  setupLinks(subListPost);
}

oListInfo::ResultType oListInfo::getResultType() const {
  return resType;
}

bool oListParam::matchLegNumber(const pClass cls, int leg)  const {
  if (cls == 0 || legNumber == -1 || leg < 0)
    return true;
  int number, order;
  cls->splitLegNumberParallel(leg, number, order);
  if (number == legNumber)
    return true;
  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  return maj == number + 1 && sub == order;
}

int oListParam::getLegNumber(const pClass cls) const {
  if (legNumber < 0)
    return legNumber;

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  if (cls) {
    if (legNumber < 10000)
      return cls->getLegNumberLinear(legNumber, 0);
    else
      return cls->getLegNumberLinear(maj-1, sub);
  }
  return sub;
}

pair<int, bool> oListParam::getLegInfo(const pClass cls) const {
  if (legNumber == -1)
    return make_pair(-1, true);
  else if (legNumber < 10000)
    return make_pair(legNumber, false);

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  int lin = cls ? cls->getLegNumberLinear(maj-1, sub) : sub+maj;
  
  return make_pair(lin, true);
}

string oListParam::getLegName() const {
  if (legNumber == -1)
    return "";

  if (legNumber < 1000)
    return itos(legNumber + 1);

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  char bf[64];
  char symb = 'a' + sub;
  sprintf_s(bf, "%d%c", maj, symb);
  return bf;
}
