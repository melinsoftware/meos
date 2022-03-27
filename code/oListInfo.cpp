/********************i****************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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
  calculateLiveResults = false;
  calcCourseClassResults = false;
  calcCourseResults = false;
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
  
  needPunches = PunchMode::NoPunch;
}

oListInfo::~oListInfo(void) {
}

void oListInfo::replaceType(EPostType find, EPostType replace, bool onlyFirst) {
  for (auto blp : { &Head, &subHead, &listPost, &subListPost }) {
    for (auto &pp : *blp) {
      if (pp.type == replace && onlyFirst)
        return;
      if (pp.type == find) {
        pp.type = replace;
        if (onlyFirst)
          return;
      }
    }
  }
}

wstring oListParam::getContentsDescriptor(const oEvent &oe) const {
  wstring cls;
  vector<pClass> classes;
  oe.getClasses(classes, false);
  if (classes.size() == selection.size() || selection.empty())
    cls = oe.getName(); // All classes
  else {
    for (pClass c : classes) {
      if (selection.count(c->getId())) {
        if (!cls.empty())
          cls += L", ";
        cls += c->getName();
      }
    }
  }
  if (legNumber != -1)
    return lang.tl(L"Sträcka X#" + getLegName()) + L": " + cls;
  else
    return cls;
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

oPrintPost::oPrintPost(EPostType type_, const wstring &text_,
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
    if (needPunches == PunchMode::SpecificPunch) {
      int to = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultTo).first;
      int from = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultFrom).first;

      if (it->wasSQLChanged(legToCheck, to) ||
          it->wasSQLChanged(legToCheck, from) )
        return true;
    }
    else if (needPunches == PunchMode::AnyPunch) {
      if (it->wasSQLChanged(legToCheck, -2))
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
    li.addHead(oPrintPost(lString, lang.tl(L"Visar de X bästa#" + itow(par.filterMaxPer)), normalText, 0, ypos));
}
extern gdioutput *gdi_main;

static pair<wstring, bool> getControlName(const oEvent &oe, int courseContolId) {
  pair<int, int> idt = oControl::getIdIndexFromCourseControlId(courseContolId);
  pControl to = oe.getControl(idt.first);
  wstring toS;
  bool name = false;
  if (to) {
    if (to->hasName()) {
      toS = to->getName();
      name = true;
    }
    else if (to->getFirstNumber()>0)
      toS = itow(to->getFirstNumber());
    else
      toS = itow(idt.first);

    if (to->getNumberDuplicates() > 0)
      toS += L"-" + itow(idt.second + 1);
  }
  else
    toS = itow(idt.first);

  return make_pair(toS, name);
}

static wstring getFullControlName(const oEvent &oe, int ctrl) {
  pair<wstring, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl(L"Kontroll X#" + toS.first);
}

static void getResultTitle(const oEvent &oe, const oListParam &lp, wstring &title) {
  if (lp.useControlIdResultTo <= 0 && lp.useControlIdResultFrom <= 0)
    title = lang.tl(L"Resultat - %s");
  else if (lp.useControlIdResultTo>0 && lp.useControlIdResultFrom<=0){
    pair<wstring, bool> toS = getControlName(oe, lp.useControlIdResultTo);
    if (toS.second)
      title = lang.tl(L"Resultat - %s") + L", " + toS.first;
    else
      title = lang.tl(L"Resultat - %s") + L", " + lang.tl(L"vid kontroll X#" + toS.first);
  }
  else {
    wstring fromS = lang.tl(L"Start"), toS = lang.tl(L"Mål");
    if (lp.useControlIdResultTo>0) {
      toS = getControlName(oe, lp.useControlIdResultTo).first;
    }
    if (lp.useControlIdResultFrom>0) {
      fromS = getControlName(oe, lp.useControlIdResultFrom).first;
    }
    title = lang.tl(L"Resultat mellan X och Y#" + fromS + L"#" + toS);
  }
}

static double adjustmentFactor(double par, double target) {
  double k = (1.0 - target)/10;
  double val = max(target, 1.0 - (par * k));
  return val;
}


template<typename T, int size> class WordMeasure {
  multimap<int, T> words;
public:

  WordMeasure() {}

  void add(const T &word) {
    if (words.size() > size) {
      size_t blen = words.begin()->first;
      if (word.length() <= blen)
        return;
      else {
        words.erase(words.begin());
        words.insert(make_pair(word.length(), word));
      }
    }
    else {
      words.insert(make_pair(word.length(), word));
    }
  }

  int measure(const gdioutput &gdi, 
              gdiFonts font,
              const wchar_t *fontFace,
              T &longest) {   

    int w = 0;
    TextInfo ti;
    HDC hDC = GetDC(gdi.getHWNDTarget());
    
    for (auto it = words.begin(); it != words.end(); ++it) {
      ti.xp = 0;
      ti.yp = 0;
      ti.format = font;
      ti.text = it->second;
      ti.font = fontFace != 0 ? fontFace : L"";
      gdi.calcStringSize(ti, hDC);
      if (ti.textRect.right > w) {
        w = ti.textRect.right;
        longest = ti.text;
      }
    }

    ReleaseDC(gdi.getHWNDTarget(), hDC);
    return w;
  }

};

int oListInfo::getMaxCharWidth(const oEvent *oe,
                               const gdioutput &gdi,
                               const set<int> &clsSel,
                               const vector< pair<EPostType, wstring> > &typeFormats,
                               gdiFonts font,
                               const wchar_t *fontFace,
                               bool large, int minSize) const {
  vector<oPrintPost> pps;
  for (size_t k = 0; k < typeFormats.size(); k++) {
    pps.push_back(oPrintPost());
    pps.back().text = typeFormats[k].second;
    pps.back().type = typeFormats[k].first;
  }

  oListParam par;
  par.setLegNumberCoded(0);
  oCounter c;
  vector< WordMeasure<wstring, 32> > extras(pps.size());

  for (size_t k = 0; k < pps.size(); k++) {
    wstring extra;
    switch (pps[k].type) {
      case lResultModuleNumber:
      case lResultModuleNumberTeam:
        if (pps[k].text.length() > 1 && pps[k].text[0] == '@') {
          wstring tmp;
          int miLen = 0;
          for (int j = 0; j < 10; j++) {
            tmp = MetaList::fromResultModuleNumber(pps[k].text.substr(1), j, tmp);
            if (tmp.length() > miLen / 2) {
              miLen = tmp.length();
              extras[k].add(tmp);
            }
          }
        }
        else
          extra = L"999";
        break;
      case lRunnerCardVoltage:
        extra = L"3.00 V";
        break;
      case lPunchName:
      case lControlName:
      case lPunchNamedTime: {
        wstring maxcn = lang.tl("Mål");
        vector<pControl> ctrl;
        oe->getControls(ctrl, false);
        for (pControl c : ctrl) {
          wstring cn = c->getName();
          if (cn.length() > maxcn.length())
            maxcn.swap(cn);
        }
        if (pps[k].type == lPunchNamedTime)
          extra = maxcn + L": 50:50 (50:50)";
        else
          maxcn.swap(extra);
      }
       break;
      case lRunnerFinish:
      case lRunnerCheck:
      case lPunchAbsTime:
      case lRunnerStart:
      case lTeamStart:
        extra = L"10:10:00";
      break;
      case lRunnerTotalTimeAfter:
      case lRunnerClassCourseTimeAfter:
      case lRunnerTimeAfterDiff:
      case lRunnerTempTimeAfter:
      case lRunnerTimeAfter:
      case lRunnerLostTime:
      case lTeamTimeAfter:
      case lTeamLegTimeAfter:
      case lTeamTotalTimeAfter:
      case lTeamTimeAdjustment:
      case lRunnerTimeAdjustment:
      case lRunnerGeneralTimeAfter:
      case lPunchTotalTimeAfter:
        extra = L"+10:00";
        break;
      case lTeamRogainingPointOvertime:
      case lRunnerRogainingPointOvertime:
      case lResultModuleTime:
      case lResultModuleTimeTeam:  
      case lTeamTime:
      case lTeamGrossTime:
      case lTeamTotalTime:
      case lTeamTotalTimeStatus:
      case lTeamLegTimeStatus:
      case lTeamTimeStatus:
      case lRunnerTempTimeStatus:
      case lRunnerTotalTimeStatus:
      case lRunnerTotalTime:
      case lClassStartTime:
      case lRunnerTime:
      case lRunnerGrossTime:
      case lRunnerTimeStatus:
      case lRunnerStageTime:
      case lRunnerStageTimeStatus:
      case lRunnerStageStatus:
      case lRunnerTimePlaceFixed:
      case lPunchLostTime:
      case lPunchTotalTime:
      case lPunchTimeSinceLast:
      case lPunchSplitTime:
      case lPunchNamedSplit:
        extra = L"50:50";
        break;
      case lRunnerGeneralTimeStatus:
      case lClassStartTimeRange:
        extra = L"50:50 (50:50)";
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
      case lRunnerCoursePlace:
      case lTeamPlace:
      case lTeamTotalPlace:
      case lPunchControlPlace:
      case lPunchControlPlaceAcc:
      case lRunnerStagePlace:           
        extra = L"99.";
        break;
      case lRunnerGeneralPlace:
        extra = L"99. (99.)";
        break;
    }

    EPostType type = pps[k].type;

    if (type == lClubName || type == lRunnerName || type == lTeamName
                          || type == lTeamRunner || type == lTeamClub)
      extras[k].add(L"IK Friskus Varberg");

    if (type == lRunnerGivenName || type == lRunnerFamilyName || type == lRunnerNationality)
      extras[k].add(L"Karl-Gunnar");

    if (type == lRunnerCompleteName || type == lPatrolNameNames || type == lPatrolClubNameNames)
      extras[k].add(L"Karl-Gunnar Alexandersson");

    extras[k].add(extra);
  }
  for (size_t k = 0; k < pps.size(); k++) {
    const oPrintPost &pp = pps[k];
   
    int cardSkip = 0;
    int skip = 0;
    wstring last;
    for (auto crd = oe->Cards.begin(); crd != oe->Cards.end(); ++crd) {
      if (--skip > 0)
        continue;
      skip = cardSkip++;

      for (auto &p : crd->punches) {
        pRunner r = crd->tOwner;
        p.previousPunchTime = 0;
        const wstring &out = oe->formatPunchString(pp, par, r ? r->getTeam() : nullptr, r, &p, c);
        if (last == out)
          break;
        else
          last = out;
        extras[k].add(out);
      }
    }

    for (auto &cls : oe->Classes) {
      if (cls.isRemoved())
        continue;
      if (!clsSel.empty() && clsSel.count(cls.getId()) == 0)
        continue;

      const wstring &out = oe->formatListString(pp, par, 0, 0, 0, pClass(&cls), c);
      extras[k].add(out);
    }

    for (oCourseList::const_iterator it = oe->Courses.begin(); it != oe->Courses.end(); ++it) {
      if (it->isRemoved())
        continue;

      const wstring &out = oe->formatSpecialString(pp, par, 0, 0, pCourse(&*it), 0,  c);
      extras[k].add(out);
    }

    for (oControlList::const_iterator it = oe->Controls.begin(); it != oe->Controls.end(); ++it) {
      if (it->isRemoved())
        continue;

      const wstring &out = oe->formatSpecialString(pp, par, 0, 0, 0, pControl(&*it),  c);
      extras[k].add(out);
    }
  }

  vector<int> row(pps.size(), 0);
  vector<wstring> samples(pps.size());
  wstring totWord = L"";
  for (size_t k = 0; k < pps.size(); k++) {
    extras[k].measure(gdi, font, fontFace, samples[k]);
    totWord += samples[k];
  }
  
  WordMeasure<wstring, 64> totMeasure;
  totMeasure.add(totWord);
  for (oRunnerList::const_iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->isRemoved())
      continue;

    // Case when runner/team has different class
    bool teamOK = it->getTeam() && clsSel.count(it->getTeam()->getClassId(false));

    if (!clsSel.empty() && (!teamOK && clsSel.count(it->getClassId(true)) == 0))
        continue;

    totWord.clear();
    wstring rout;
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
      rout.clear();
      while (numIter-- > 0) {
        const wstring &out = oe->formatListString(pp, par, it->tInTeam, pRunner(&*it), it->Club, pClass(it->getClassRef(true)), c);
        //row[k] = max(row[k], int(out.length()));
        if (out.length() > rout.length())
          rout = out;
        if (numIter>0)
          c.level3++;
      }

      if (rout.length() > samples[k].length())
        totWord.append(rout);
      else
        totWord.append(samples[k]);

    }
    totMeasure.add(totWord);
  }

  wstring dummy;
  int w = totMeasure.measure(gdi, font, fontFace, dummy);
  w = max(w, gdi.scaleLength(minSize));
  return int(0.5 + (w + (large ? 5 : 15))/gdi.getScale());
}

const wstring & oEvent::formatListString(EPostType type, const pRunner r) const
{
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->getClassRef(true), ctr);
}

const wstring & oEvent::formatListString(EPostType type, const pRunner r, 
                                         const wstring &format) const {
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  pp.text = format;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->getClassRef(true), ctr);
}


const wstring &oEvent::formatListString(const oPrintPost &pp, const oListParam &par,
                                        const pTeam t, const pRunner r, const pClub c,
                                        const pClass pc, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
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

const wstring &oEvent::formatPunchString(const oPrintPost &pp, const oListParam &par,
                                         const pTeam t, const pRunner r,
                                         const oPunch *punch, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
      }
      out->append(*tmp);
    }
    tmp = &formatPunchStringAux(*cpp, par, t, r, punch, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const wstring &oEvent::formatSpecialString(const oPrintPost &pp, const oListParam &par, const pTeam t, int legIndex,
                                           const pCourse crs, const pControl ctrl, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
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

const wstring &oEvent::formatPunchStringAux(const oPrintPost &pp, const oListParam &par,
                                            const pTeam t, const pRunner r,
                                            const oPunch *punch, oCounter &counterIn) const {
  wchar_t bfw[128];
  const wstring *wsptr = 0;
  bfw[0] = 0;
  pClass pc = r ? r->getClassRef(true) : 0;
  bool invalidClass = pc && pc->getClassStatus() != oClass::ClassStatus::Normal;
  oCounter counter(counterIn);

  static bool reentrantLock = false;
  if (reentrantLock == true) {
    reentrantLock = false;
    throw meosException("Internal list error");
  }
  bool doDefault = false;

  switch (pp.type) {
      case lControlName:
      case lPunchName:
        if (punch) {
          pCourse pc = r ? r->getCourse(false) : nullptr;
          if (punch->isFinish(pc ? pc->getFinishPunchType() : oPunch::PunchFinish)) {
            wsptr = &lang.tl("Mål");
          }
          else {
            pControl ctrl = getControl(punch->getControlId());
            if (!ctrl)
              ctrl = getControlByType(punch->Type);

            if (ctrl && ctrl->hasName()) {
              swprintf_s(bfw, L"%s", ctrl->getName().c_str());
            }
          }
        }
        break;
  case lPunchTimeSinceLast:
    if (punch && punch->previousPunchTime && r && !invalidClass) {
      int time = punch->Time;
      int pTime = punch->previousPunchTime;
      if (pTime > 0 && time > pTime) {
        int t = time - pTime;
        wsptr = &formatTime(t);
      }
    }
    break;
  case lPunchTime:
  case lPunchControlNumber:
  case lPunchControlCode:
  case lPunchLostTime:
  case lPunchControlPlace:
  case lPunchControlPlaceAcc:
  
  case lPunchSplitTime:
  case lPunchTotalTime:
  case lPunchAbsTime:
    if (punch && r && !invalidClass) {
      if (punch->tIndex >= 0) {
        // Punch in course
        counter.level3 = punch->tIndex;
        doDefault = true;
        break;
      }
      switch (pp.type) {
        case lPunchTime: {
          if (punch->Time > 0) {
            swprintf_s(bfw, L"\u2013 (%s)", formatTime(punch->Time - r->getStartTime()).c_str());
          }
          else {
            wsptr = &makeDash(L"- (-)");
          }
          break;
        }
        case lPunchSplitTime: {
          swprintf_s(bfw, L"\u2013");
          break;
        }
        case lPunchControlNumber: {
          wcscpy_s(bfw, L"\u2013");
          break;
        }
        case lPunchControlCode: {
          swprintf_s(bfw, L"%d", punch->getTypeCode());
          break;
        }
        case lPunchAbsTime: {
          if (punch->Time > 0)
            wsptr = &getAbsTime(punch->Time);
          break;
        }
        case lPunchTotalTime: {
          if (punch->Time > 0)
            wsptr = &formatTime(punch->Time - r->getStartTime());
          break;
        }
      }
    }
    break;

  default:
    doDefault = true;
  }

  if (doDefault) {
    reentrantLock = true;
    try {
      const wstring &res = formatListStringAux(pp, par, t, r,
                                                r ? r->getClubRef() : 0,
                                                r ? r->getClassRef(true) : 0, counter);
      reentrantLock = false;
      return res;
    }
    catch (...) {
      reentrantLock = false;
      throw;
    }
  }

  if (pp.type != lString && (wsptr == 0 || wsptr->empty()) && bfw[0] == 0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf_s(bfw, pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf_s(bf2, pp.text.c_str(), bfw);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

const wstring &oEvent::formatSpecialStringAux(const oPrintPost &pp, const oListParam &par,
                                              const pTeam t, int legIndex,
                                              const pCourse pc, const pControl ctrl, 
                                              const oCounter &counter) const {

  wchar_t bfw[512];
  const wstring *wsptr=0;
  bfw[0] = 0;

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
          swprintf_s(bfw, L"%d", len);
      }
    break;

    case lCourseName:
    case lRunnerCourse:
      if (pc) {
        wsptr = &pc->getName();
      }
    break;

    case lRunnerLegNumberAlpha:
      if (t && t->getClassRef(false) && legIndex >= 0) {
        wstring legStr = t->getClassRef(false)->getLegNumber(legIndex);
        wcscpy_s(bfw, legStr.c_str());
      }
      break;

    case lRunnerLegNumber:
      if (t && t->getClassRef(false) && legIndex >= 0) {
         int legNumber, legOrder;
         t->getClassRef(false)->splitLegNumberParallel(legIndex, legNumber, legOrder);
         wsptr = &itow(legNumber+1);
      }
      break;

    case lCourseClimb: {
      int len = pc ? pc->getDCI().getInt("Climb") : 0;
      if (len > 0)
        swprintf_s(bfw, L"%d", len);      
    }
    break;

    case lCourseUsage:
      if (pc) {
        wsptr = &itow(pc->getNumUsedMaps(false));
      }
    break;

    case lCourseUsageNoVacant:
      if (pc) {
        wsptr = &itow(pc->getNumUsedMaps(true));
      }
    break;

    case lCourseClasses:
      if (pc) {
        vector<pClass> cls;
        pc->getClasses(cls);
        wstring tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;

    case lControlName:
      if (ctrl)
        wcsncpy_s(bfw, ctrl->getName().c_str(), 128);
    break;

    case lControlCourses:
      if (ctrl) {
        vector<pCourse> crs;
        ctrl->getCourses(crs);
        if (crs.size() == Courses.size()) {
          wsptr = &lang.tl("Alla");
          break;
        }

        wstring tmp;
        for (size_t k = 0; k < crs.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += crs[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;
  
    case lControlClasses:
      if (ctrl) {
        vector<pClass> cls;
        ctrl->getClasses(cls);
        if (cls.size() == getNumClasses()) {
          wsptr = &lang.tl("Alla");
          break;
        }

        wstring tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;

    case lControlVisitors:
      if (ctrl)
        wsptr = &itow(ctrl->getNumVisitors(false));
    break;

    case lControlPunches:
      if (ctrl)
        wsptr = &itow(ctrl->getNumVisitors(true));
      break;

    case lControlMedianLostTime:
      if (ctrl) 
        wsptr = &formatTime(ctrl->getMissedTimeMedian()); 
    break;

    case lControlMaxLostTime:
      if (ctrl) 
        wsptr = &formatTime(ctrl->getMissedTimeMax()); 
      break;
    
    case lControlMistakeQuotient:
      if (ctrl) {
        wstring tmp = itow(ctrl->getMistakeQuotient()); 
        tmp += L"%";
        wcsncpy_s(bfw, tmp.c_str(), 20);
      }
      break;

    case lControlRunnersLeft:
      if (ctrl)
        wsptr = &itow(ctrl->getNumRunnersRemaining());
    break;

    case lControlCodes: 
      if (ctrl) {
        vector<int> numbers;
        ctrl->getNumbers(numbers);
        wstring tmp;
        for (size_t j = 0; j < numbers.size(); j++) {
          if (j > 0)
            tmp += L", ";
          tmp += itow(numbers[j]);
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
      break;

    default: {
      reentrantLock = true;
      try {
        const wstring &res = formatListStringAux(pp, par, t, t ? t->getRunner(legIndex) : 0, 
                                                                 t ? t->getClubRef() : 0,
                                                                 t ? t->getClassRef(false) : 0, counter);
        reentrantLock = false;
        return res;
      }
      catch(...) {
        reentrantLock = false;
        throw;
      }
    }
  }

  if (pp.type!=lString && (wsptr==0 || wsptr->empty()) && bfw[0]==0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf_s(bfw, pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf_s(bf2, pp.text.c_str(), bfw);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

const wstring &oEvent::formatListStringAux(const oPrintPost &pp, const oListParam &par,
                                          const pTeam t, const pRunner r, const pClub c,
                                          const pClass pc, const oCounter &counter) const {

  wchar_t wbf[512];
  const wstring *wsptr=0;
  wbf[0]=0;

  auto noTimingRunner = [&]() {
    return (pc ? pc->getNoTiming() : false) || (r ? (r->getStatusComputed() == StatusNoTiming || r->noTiming()) : false);
  };
  auto noTimingTeam = [&]() {
    return (pc ? pc->getNoTiming() : false) || (t ? (t->getStatusComputed() == StatusNoTiming || t->noTiming()): false);
  };
  bool invalidClass = pc && pc->getClassStatus() != oClass::ClassStatus::Normal;
  int legIndex = pp.legIndex;
  if(pc && !MetaList::isResultModuleOutput(pp.type) && !MetaList::isAllStageType(pp.type))
    legIndex = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
        
  switch ( pp.type ) {
    case lClassName:
      if (invalidClass)
        swprintf_s(wbf, L"%s (%s)", pc->getName().c_str(), lang.tl("Struken").c_str());
      else
        wsptr=pc ? &pc->getName() : 0;
      break;
    case lResultDescription: {
        wstring title;
        getResultTitle(*this, par, title);
        wcscpy_s(wbf, title.c_str());
      }
      break;
    case lTimingFromName:
      if (par.useControlIdResultFrom > 0)
        wcscpy_s(wbf, getFullControlName(*this, par.useControlIdResultFrom).c_str());
      else
        wsptr = &lang.tl(L"Start");
      break;
    case lTimingToName:
      if (par.useControlIdResultTo > 0)
        wcscpy_s(wbf, getFullControlName(*this, par.useControlIdResultTo).c_str());
      else
        wsptr = &lang.tl(L"Mål");
      break;
    case lClassLength:
      if (pc) {
        wcscpy_s(wbf, pc->getLength(par.relayLegIndex).c_str());
      }
      break;
    case lClassStartName:
      if (pc) wcscpy_s(wbf, pc->getDI().getString("StartName").c_str());
      break;
    case lClassStartTime:
    case lClassStartTimeRange:
      if (pc) {
        int first, last;
        pc->getStartRange(legIndex, first, last);
        if (pc->hasFreeStart())
          wsptr = &lang.tl("Fri starttid");
        else if (first > 0 && first == last) {
          if (oe->useStartSeconds())
            wsptr = &oe->getAbsTime(first);
          else
            wsptr = &oe->getAbsTimeHM(first);
        }
        else if (pp.type == lClassStartTimeRange) {
          wstring range =  oe->getAbsTimeHM(first) + makeDash(L" - ") + oe->getAbsTimeHM(last);
          wcscpy_s(wbf, range.c_str());
        }
      }
      break;
    case lClassResultFraction:
      if (pc && !invalidClass) {
        int total, finished,  dns;
        pc->getNumResults(par.getLegNumber(pc), total, finished, dns);
        swprintf_s(wbf, L"(%d / %d)", finished, total);
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

    case lClassAvailableMaps:
      if (pc) {
        int n = pc->getNumRemainingMaps(false);
        if (n != numeric_limits<int>::min())
          wsptr = &itow(n);
      }
      break;

    case lClassTotalMaps:
      if (pc) {
        int n = pc->getNumberMaps();
        if (n > 0)
          wsptr = &itow(n);
      }
      break;
    
    case lClassNumEntries:
      if (pc) {
        int n = pc->getNumRunners(true, true, true);
        wsptr = &itow(n);
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
          swprintf_s(wbf, L"%d", sh);
      }
      break;
    case lCmpName:
      wsptr = &getName();
      break;
    case lCmpDate:
      wsptr = &getDate();
      break;
    case lCurrentTime:
      wcscpy_s(wbf, getCurrentTimeS().c_str());
      break;
    case lRunnerClub:
      wsptr = (r && r->Club) ? &r->Club->getDisplayName() : 0;
      break;
    case lRunnerFinish:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getFinishTimeS();
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getFinishTimeS(this);
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
        if (r->getStatus() == StatusCANCEL) {
          wsptr = &oEvent::formatStatus(StatusCANCEL, true);
        }
        else if (r->startTimeAvailable()) {
          if (pp.type != lRunnerStartZero) 
            wsptr = &r->getStartTimeCompact();
          else {
            int st = r->getStartTime();
            wsptr = &getTimeMS(st-oe->getFirstStart());
          }
        }
        else
          wsptr = &makeDash(L"-");
      }
      break;
    case lRunnerCheck:
      if (r && !invalidClass && r->Card) {
        oPunch *punch = r->Card->getPunchByType(oPunch::PunchCheck);
        if (punch && punch->Time > 0)
          wsptr = &getAbsTime(punch->Time);
        else
          wsptr = &makeDash(L"-"); 
      }
      break;

    case lRunnerName:
      wsptr = r ? &r->getName() : 0;
      break;
    case lRunnerGivenName:
      if (r) wcscpy_s(wbf, r->getGivenName().c_str());
      break;
    case lRunnerFamilyName:
      if (r) wcscpy_s(wbf, r->getFamilyName().c_str());
      break;
    case lRunnerCompleteName:
      if (r) wcscpy_s(wbf, r->getCompleteIdentification().c_str());
      break;
    case lPatrolNameNames:
      if (t) {
        pRunner r1 = t->getRunner(0);
        pRunner r2 = t->getRunner(1);
        if (r1 && r2 && r2->tParentRunner != r1) {
          swprintf_s(wbf, L"%s / %s", r1->getName().c_str(),r2->getName().c_str());
        }
        else if (r1) {
          wsptr = &r1->getName();
        }
        else if (r2) {
          wsptr = &r2->getName();
        }
      }
      else {
        wsptr = r ? &r->getName() : 0;
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
          swprintf_s(wbf, L"%s / %s", c1->getDisplayName().c_str(),c2->getDisplayName().c_str());
        }
        else if (c1) {
          wsptr = &c1->getDisplayName();
        }
        else if (c2) {
          wsptr = &c2->getDisplayName();
        }
      }
      else {
        wsptr = r && r->Club ? &r->Club->getDisplayName() : 0;
      }
      break;
    case lRunnerTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getRunningTimeS(true);
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);

        if (r->getNumShortening() > 0) {
          swprintf_s(wbf, L"*%s", wsptr->c_str());
          wsptr = 0;
        }
      }
      break;
    case lRunnerGrossTime:
      if (r && !invalidClass) {
        int tm = r->getRunningTime(true);
        if (tm > 0)
          tm -= r->getTimeAdjustment();

        wsptr = &formatTime(tm);
      }
      break;

    case lRunnerTimeStatus:
      if (r) {
        if (invalidClass)
          wsptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          bool ok = r->prelStatusOK(true, true);
          if (ok && !noTimingRunner()) {
            wsptr = &r->getRunningTimeS(true);
            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (r->getStatusComputed() == StatusOutOfCompetition) {
              swprintf_s(wbf, L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else {
            if (ok)
              wsptr = &formatStatus(StatusOK, true);
            else
              wsptr = &r->getStatusS(true, true);
          }
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.isStatusOK() && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(0);
            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (res.getStatus() == StatusOutOfCompetition) {
              swprintf_s(wbf, L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(StatusOK);
        }
      }
      break;

    case lRunnerGeneralTimeStatus:
      if (r) {
        if (invalidClass)
          wsptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          if (r->prelStatusOK(true, true) && !noTimingRunner()) {
            wstring timeStatus = r->getRunningTimeS(true);
            
            if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
              RunnerStatus ts = r->getTotalStatus();
              int rt = r->getTotalRunningTime();
              if (ts == StatusOK || (ts == StatusUnknown && rt > 0)) {
                wstring vts = formatTime(rt) + L" (" + timeStatus + L")";
                swap(vts, timeStatus);
              }
              else {
                wstring vts = formatStatus(ts, true) + L" (" + timeStatus + L")";
                swap(vts, timeStatus);
              }
            }

            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", timeStatus.c_str());
            }
            else {
              wcscpy_s(wbf, timeStatus.c_str());
            }
          }
          else
            wsptr = &r->getStatusS(true, true);
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.isStatusOK() && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(0);
            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (res.getStatus() == StatusOutOfCompetition) {
              swprintf_s(wbf, L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(StatusOK);
        }
      }
      break;

      break;

    case lRunnerGeneralTimeAfter:
      if (r && pc && !invalidClass && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1) {

          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            int tleg = r->tLeg >= 0 ? r->tLeg:0;
            if (r->getTotalStatus()==StatusOK) {
              if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
                int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
                if (after > 0)
                  swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
              }
              else {
                wsptr = &makeDash(L"-");
              }
            }
          }
          else {
            int tleg=r->tLeg>=0 ? r->tLeg:0;
            if (r->tStatus==StatusOK && pc && !noTimingRunner()) {
              if (r->getNumShortening() == 0) {
                int after = r->getRunningTime(true) - pc->getBestLegTime(oClass::AllowRecompute::Yes, tleg, true);
                if (after > 0)
                  swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
              }
              else {
                wsptr = &makeDash(L"-");
              }
            }
          }
        }
        else {
          int after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
          if (after > 0)
            swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
        }
      }
      break;


    case lRunnerTimePerKM:
      if (r && !invalidClass && r->prelStatusOK(true, true)) {
        const pCourse pc = r->getCourse(false);
        if (pc) {
          int t = r->getRunningTime(false);
          int len = pc->getLength();
          if (len > 0 && t > 0) {
            int sperkm = (1000 * t) / len;
            wsptr = &formatTime(sperkm);
          }
        }
      }
      break;
    case lRunnerTotalTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getTotalRunningTimeS();
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(r->getTotalTimeInput());
      }
      break;
    case lRunnerTotalTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        if (pp.resultModuleIndex == -1) {
          if ((r->getTotalStatus()==StatusOK || (r->getTotalStatus()==StatusUnknown 
            && r->prelStatusOK(true, true) && r->getInputStatus() == StatusOK) ) && !noTimingRunner()) {
            wsptr = &r->getTotalRunningTimeS();
            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &r->getTotalStatusS(true);
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          RunnerStatus input = r->getTotalStatusInput();
          if (input == StatusOK && res.getStatus() == StatusOK && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(r->getTotalTimeInput());
            if (r->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(input);
        }
      }
      break;
    case lRunnerTempTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (r) {
        if (showResultTime(r->tempStatus, r->tempRT) && !noTimingRunner())
          wcscpy_s(wbf, formatTime(r->tempRT).c_str());
        else
          wcscpy_s(wbf, formatStatus(r->tempStatus, true).c_str() );
      }
      break;

    case lRunnerCardVoltage:
      if (r && r->getCard()) {
        wcscpy_s(wbf, r->getCard()->getCardVoltage().c_str());
      }
      break;

    case lRunnerStageNumber:
      if (pp.legIndex >= 0)
        swprintf_s(wbf, L"%d", pp.legIndex + 1);
      break;

    case lRunnerStageTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        if (showResultTime(st, time) && !noTimingRunner())
          wcscpy_s(wbf, formatTime(time).c_str());
        else
          wcscpy_s(wbf, formatStatus(st, true).c_str());
      }
      break;

    case lRunnerStageTime:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        if (time > 0 && !noTimingRunner())
          wcscpy_s(wbf, formatTime(time).c_str());
      }
      break;

    case lRunnerStageStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        wcscpy_s(wbf, formatStatus(st, true).c_str());
      }
      break;

    case lRunnerStagePoints:
      if (!invalidClass && r) {
        int points, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, d, points, d);
        swprintf_s(wbf, L"%d", points);
      }
      break;

    case lRunnerPlace:
      if (r && !invalidClass && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1)
          wcscpy_s(wbf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;
    case lRunnerTotalPlace:
      if (r && !invalidClass && !noTimingRunner())
        wcscpy_s(wbf, r->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;
    case lRunnerStagePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int d, place;
        wstring tmp;
        r->getStageResult(pp.legIndex, d, d, place);
        if (place > 0) {
          tmp = itow(place);
          if (pp.text.empty())
            tmp += L".";
        }
        wcscpy_s(wbf, tmp.c_str());
      }
      break;
    case lRunnerGeneralPlace:
      if (r && !invalidClass && pc && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1) {
          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            wstring iPlace;
            if (pc->getClassType() != oClassPatrol)
              iPlace = r->getPrintPlaceS(true);

            wstring tPlace = r->getPrintTotalPlaceS(true); 
            wstring v;
            if (iPlace.empty())
              v = tPlace;
            else {
              if (tPlace.empty())
                tPlace = makeDash(L"-");
              v = tPlace + L" (" + iPlace + L")";
            }
            wcscpy_s(wbf, v.c_str());
          }
          else
            wcscpy_s(wbf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        }
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lRunnerClassCoursePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getCoursePlace(true);
        if (p>0 && p<10000)
          swprintf_s(wbf, L"%d.", p);
      }
      break;

    case lRunnerCoursePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getCoursePlace(false);
        if (p>0 && p<10000)
          swprintf_s(wbf, L"%d.", p);
      }
      break;
    case lRunnerPlaceDiff:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getTotalPlace();
        if (r->getTotalStatus() == StatusOK && p > 0 && r->inputPlace>0) {
          int pd = p - r->inputPlace;
          if (pd > 0)
            swprintf_s(wbf, L"+%d", pd);
          else if (pd < 0)
            swprintf_s(wbf, L"-%d", -pd);
        }
      }
      break;
    case lRunnerTimeAfterDiff:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus() == StatusOK && pc && !noTimingRunner()) {
          int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          int afterOld = r->inputTime - pc->getBestInputTime(oClass::AllowRecompute::Yes, tleg);
          int ad = after - afterOld;
          if (ad > 0)
            swprintf_s(wbf, L"+%d:%02d", ad / 60, ad % 60);
          if (ad < 0)
            swprintf_s(wbf, L"-%d:%02d", (-ad) / 60, (-ad) % 60);
        }
      }
      break;
    case lRunnerRogainingPoint:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          swprintf_s(wbf, L"%d", r->getRogainingPoints(true, false));
        else
          swprintf_s(wbf, L"%d", r->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;

    case lRunnerRogainingPointTotal:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          swprintf_s(wbf, L"%d", r->getRogainingPoints(true, true));
        else
          swprintf_s(wbf, L"%d", r->getTempResult(pp.resultModuleIndex).getPoints() + r->getInputPoints());
      }
      break;

    case lRunnerRogainingPointReduction:
      if (r && !invalidClass) {
        int red = r->getRogainingReduction(true);
        if (red > 0)
          swprintf_s(wbf, L"-%d", red);
      }
      break;
    
    case lRunnerRogainingPointGross:
      if (r && !invalidClass) {
        int p = r->getRogainingPointsGross(true);
        wsptr = &itow(p);
      }
      break;
    
    case lRunnerPointAdjustment:
      if (r && !invalidClass) {
        int a = r->getPointAdjustment();
        if (a<0)
          wsptr = &itow(a);
        else if (a>0)
          swprintf_s(wbf, L"+%d", a);
      }
      break;
    case lRunnerTimeAdjustment:
      if (r && !invalidClass) {
        int a = r->getTimeAdjustment();
        if (a != 0)
          wsptr = &getTimeMS(a);
      }
      break;
    case lRunnerRogainingPointOvertime:
      if (r && !invalidClass) {
        int over = r->getRogainingOvertime(true);
        if (over > 0)
          wsptr = &formatTime(over);
      }
      break;

    case lRunnerTimeAfter:
      if (r && pc && !invalidClass && !noTimingRunner()) {
        int after = 0;
        if (pp.resultModuleIndex == -1) {
          int tleg=r->tLeg>=0 ? r->tLeg:0;
          int brt = pc->getBestLegTime(oClass::AllowRecompute::Yes, tleg, true);
          if (r->prelStatusOK(true, true) && brt > 0) {
            after=r->getRunningTime(true) - brt;
          }
        }
        else {
          after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
        }
  
        if (r->getNumShortening() == 0) {
          if (after > 0)
            swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
        }
        else {
          wsptr = &makeDash(L"-");
        }
      }
      break;
    case lRunnerTotalTimeAfter:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus()==StatusOK &&  pc && !noTimingRunner()) {
          if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
            int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
            if (after > 0)
              swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
          }
          else {
            wsptr = &makeDash(L"-");
          }
        }
      }
      break;
    case lRunnerClassCourseTimeAfter:
      if (r && pc && !invalidClass) {
        pCourse crs = r->getCourse(false);
        if (crs && r->tStatus==StatusOK && !noTimingRunner()) {
          int after = r->getRunningTime(true) - pc->getBestTimeCourse(oClass::AllowRecompute::Yes, crs->getId());
          if (after > 0)
            swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
        }
      }
      break;
    case lRunnerTimePlaceFixed:
      if (r && !invalidClass) {
        int t = r->getTimeWhenPlaceFixed();
        if (t == 0 || (t>0 && t < getComputerTime())) {
          wcscpy_s(wbf, lang.tl("klar").c_str());
        }
        else if (t == -1)
          wcscpy_s(wbf, L"-");
        else
          wcscpy_s(wbf, oe->getAbsTime(t).c_str());
      }
      break;
    case lRunnerLegNumberAlpha:
    case lRunnerLegNumber:
      if (r)
        return formatSpecialStringAux(pp, par, t, r->getLegNumber(), 0, 0, counter);
      else
        wcscpy_s(wbf, par.getLegName().c_str());
      break;
    case lRunnerLostTime:
      if (r && r->prelStatusOK(true, true) && !noTimingRunner() && !invalidClass) {
        wcscpy_s(wbf, r->getMissedTimeS().c_str());
      }
      break;
    case lRunnerTempTimeAfter:
      if (r && pc) {
        if (r->tempStatus==StatusOK &&  pc && !noTimingRunner()
              && r->tempRT>pc->tLegLeaderTime) {
          int after=r->tempRT-pc->tLegLeaderTime;
          swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
        }
      }
      break;

    case lRunnerCard:
      if (r && r->getCardNo() > 0)
        swprintf_s(wbf, L"%d", r->getCardNo());
      break;
    case lRunnerRank:
      if (r) {
        int rank=r->getDCI().getInt("Rank");
        if (rank == 0) {
          if (r->tParentRunner)
            rank = r->tParentRunner->getDCI().getInt("Rank");
        }
        
        if (rank>0)
          wcscpy_s(wbf, formatRank(rank).c_str());
      }
      break;
    case lRunnerBib:
      if (r) {
        const wstring &bib = r->getBib();
        if (!bib.empty())
          wsptr = &bib;
      }
      break;
    case lRunnerUMMasterPoint:
      if (r) {
        int total, finished, dns;
        pc->getNumResults(par.getLegNumber(pc), total, finished, dns);
        int percent = int(floor(0.5+double((100*(total-dns-r->getPlace()))/double(total-dns))));
        if (r->getStatus()==StatusOK)
          swprintf_s(wbf, L"%d",  percent);
        else
          swprintf_s(wbf, L"0");
      }
      break;
    case lRunnerAge:
      if (r) {
        int y = r->getBirthAge();
        if (y > 0)
          swprintf_s(wbf, L"%d", y);
      }
    break;
    case lRunnerBirthYear:
      if (r) {
        int y = r->getBirthYear();
        if (y > 0)
          swprintf_s(wbf, L"%d", y);
      }
    break;
    case lRunnerSex:
      if (r) {
        PersonSex s = r->getSex();
        if (s == sFemale)
          wsptr = &lang.tl("Kvinna");
        else if (s == sMale)
          wsptr = &lang.tl("Man");
      }
    break;
    case lRunnerPhone:
      if (r) {
        wsptr = &r->getDCI().getString("Phone");
      }
    break;
    case lRunnerFee:
      if (r) {
        wstring s = formatCurrency(r->getDCI().getInt("Fee"));
        wcscpy_s(wbf, s.c_str());
      }
    break;
    case lRunnerExpectedFee:
      if (r) {
        wstring s = formatCurrency(r->getDefaultFee());
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerPaid:
      if (r) {
        wstring s = formatCurrency(r->getDCI().getInt("Paid"));
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerId:
      if (r) {
        wstring s = r->getExtIdentifierString();
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerPayMethod:
      if (r) {
        wsptr = &r->getDCI().formatString(r, "PayMode");
      }
      break;
    case lRunnerEntryDate:
      if (r && r->getDCI().getInt("EntryDate") > 0) {
        wsptr = &r->getDCI().getDate("EntryDate");
      }
      break;
    case lRunnerEntryTime:
      if (r) {
        wsptr = &formatTime(r->getDCI().getInt("EntryTime"));
      }
      break;
    case lTeamFee:
      if (t) {
        wstring s = formatCurrency(t->getTeamFee());
        wcscpy_s(wbf, s.c_str());
      }
    break;
    case lRunnerNationality:
      if (r) {
        wstring nat = r->getNationality();
        wcscpy_s(wbf, nat.c_str());
      }
    break;
    case lTeamName:
      //sptr = t ? &t->Name : 0;
      if (t) {
        wcscpy_s(wbf, t->getDisplayName().c_str());
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
            wsptr = &t->Runners[legIndex]->getStartTimeCompact();
          else {
            int st = t->Runners[legIndex]->getStartTime();
            wsptr = &getTimeMS(st-3600);
          }
        }
      }
      break;
    case lTeamStatus:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &t->getLegStatusS(legIndex, true, false);
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
      }
      break;
    case lTeamTime:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, false).c_str() );
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
      }
      break;
    case lTeamGrossTime:
      if (t && !invalidClass) {
        int tm = t->getLegRunningTimeUnadjusted(legIndex, false, false);
        wsptr = &formatTime(tm);
      }
      break;
    case lTeamTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          RunnerStatus st = t->getLegStatus(legIndex, true, false);
          if (st == StatusOK || ((st == StatusUnknown || st == StatusOutOfCompetition) && t->getLegRunningTime(legIndex, true, false) > 0)) {
            if (st != StatusOutOfCompetition)
              wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, false).c_str());
            else 
              swprintf_s(wbf, L"(%s)", t->getLegRunningTimeS(legIndex, true, false).c_str());
          }
          else
            wsptr = &t->getLegStatusS(legIndex, true, false);
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          if (st == StatusOK || st == StatusUnknown) {
            wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
            if (t->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamRogainingPoint:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          swprintf_s(wbf, L"%d", t->getRogainingPoints(true, false));
        else
          swprintf_s(wbf, L"%d", t->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;
    case lTeamRogainingPointTotal:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          swprintf_s(wbf, L"%d", t->getRogainingPoints(true, true));
        else
          swprintf_s(wbf, L"%d", t->getTempResult(pp.resultModuleIndex).getPoints() + t->getInputPoints());
      }
      break;

    case lTeamRogainingPointReduction:
      if (t && !invalidClass) {
        int red = t->getRogainingReduction(true);
        if (red > 0)
          swprintf_s(wbf, L"-%d", red);
      }
      break;

    case lTeamRogainingPointOvertime:
      if (t && !invalidClass) {
        int over = t->getRogainingOvertime(true);
        if (over > 0)
          wsptr = &formatTime(over);
      }
      break;

   case lTeamPointAdjustment:
      if (t && !invalidClass) {
        int a = t->getPointAdjustment();
        if (a<0)
          wsptr = &itow(a);
        else if (a>0)
          wprintf_s(wbf, "+%d", a);
      }
      break;

    case lTeamTimeAdjustment:
      if (t && !invalidClass) {
        int a = t->getTimeAdjustment();
        if (a != 0)
          wsptr = &getTimeMS(a);
      }
      break;

    case lTeamTimeAfter:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, true, false)==StatusOK) {
            if (t->getNumShortening(legIndex) == 0) {
              int ta=t->getTimeAfter(legIndex);
              if (ta>0)
                swprintf_s(wbf, L"+%d:%02d", ta/60, ta%60);
            }
            else {
              wsptr = &makeDash(L"-");
            }
          }
        }
        else {
          if (t->getTempResult().getStatus() == StatusOK) {
            int after = t->getTempResult(pp.resultModuleIndex).getTimeAfter();
            if (after > 0)
              swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
          }
        }
      }
      break;
    case lTeamPlace:
      if (t && !invalidClass && !noTimingTeam()) {
        if (pp.resultModuleIndex == -1) {
          wcscpy_s(wbf, t->getLegPrintPlaceS(legIndex, false, pp.text.empty()).c_str());
        }
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lTeamLegTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (t) {
        int ix = r ? r->getLegNumber() : counter.level3;
        RunnerStatus st = t->getLegStatus(ix, true, false);
        if (st == StatusOK)
          wcscpy_s(wbf, t->getLegRunningTimeS(ix, true, false).c_str() );
        else if (st == StatusOutOfCompetition && t->getLegRunningTime(ix, true, false) > 0)
          swprintf_s(wbf, L"(%s)", t->getLegRunningTimeS(ix, true, false).c_str());
        else
          wcscpy_s(wbf, t->getLegStatusS(ix, true, false).c_str() );
      }
      break;
    case lTeamLegTimeAfter:
      if (t) {
        int ix = r ? r->getLegNumber() : counter.level3;
        if (t->getLegStatus(ix, true, false)==StatusOK && !invalidClass) {
          if (t->getNumShortening(ix) == 0) {
            int ta=t->getTimeAfter(ix);
            if (ta>0)
              swprintf_s(wbf, L"+%d:%02d", ta/60, ta%60);
          }
          else {
            wsptr = &makeDash(L"-");
          }
        }
      }
      break;
    case lTeamClub:
      if (t) {
        wcscpy_s(wbf, t->getDisplayClub().c_str());
      }
      break;
    case lTeamRunner:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex])
        wsptr=&t->Runners[legIndex]->getName();
      break;
    case lTeamRunnerCard:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex]
      && t->Runners[legIndex]->getCardNo()>0)
        swprintf_s(wbf, L"%d", t->Runners[legIndex]->getCardNo());
      break;
    case lTeamBib:
      if (t) {
        wsptr = &t->getBib();
      }
      break;
    case lTeamTotalTime:
      if (t && !invalidClass) wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, true).c_str() );
      break;
    case lTeamTotalTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, true, true)==StatusOK)
            wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, true).c_str() );
          else
            wcscpy_s(wbf, t->getLegStatusS(legIndex, true, true).c_str() );
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          RunnerStatus inp = t->getInputStatus();
          if (st == StatusOK && inp == StatusOK) {
            wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0);
            if (t->getNumShortening() > 0) {
              swprintf_s(wbf, L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamPlaceDiff:
      if (t && !invalidClass) {
        int p = t->getTotalPlace();
        if (t->getTotalStatus() == StatusOK && p > 0 && t->inputPlace>0) {
          int pd = p - t->inputPlace;
          if (pd > 0)
            swprintf_s(wbf, L"+%d", pd);
          else if (pd < 0)
            swprintf_s(wbf, L"-%d", -pd);
        }
      }
      break;
    case lTeamTotalPlace:
      if (t && !invalidClass && !noTimingTeam()) wcscpy_s(wbf, t->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;

      break;
    case lTeamTotalTimeAfter:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !noTimingTeam()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          if (after > 0)
            swprintf_s(wbf, L"+%d:%02d", after/60, after%60);
        }
      }
      break;
    case lTeamTotalTimeDiff:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !noTimingTeam()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          int afterOld = t->inputTime - pc->getBestInputTime(oClass::AllowRecompute::Yes, tleg);
          int ad = after - afterOld;
          if (ad > 0)
            swprintf_s(wbf, L"+%d:%02d", ad/60, ad%60);
          if (ad < 0)
            swprintf_s(wbf, L"-%d:%02d", (-ad)/60, (-ad)%60);
        }
      }
      break;
    case lTeamStartNo:
      if (t)
        swprintf_s(wbf, L"%d", t->getStartNo());
      break;
    case lRunnerStartNo:
      if (r)
        swprintf_s(wbf, L"%d", r->getStartNo());
      break;
    case lNationality:
      if (r && !(wsptr = &r->getDCI().getString("Nationality"))->empty())
        break;
      else if (t && !(wsptr = &t->getDCI().getString("Nationality"))->empty())
        break;
      else if (c && !(wsptr = &c->getDCI().getString("Nationality"))->empty())
        break;

      break;

    case lCountry:
      if (r && !(wsptr = &r->getDCI().getString("Country"))->empty())
        break;
      else if (t && !(wsptr = &t->getDCI().getString("Country"))->empty())
        break;
      else if (c && !(wsptr = &c->getDCI().getString("Country"))->empty())
        break;

      break;
    case lControlName:
    case lPunchName:
    case lPunchNamedTime:
    case lPunchNamedSplit:
    case lPunchTime:
    case lPunchSplitTime:
    case lPunchTotalTime:
    case lPunchControlNumber:
    case lPunchControlCode:
    case lPunchLostTime:
    case lPunchControlPlace:
    case lPunchControlPlaceAcc:
    case lPunchAbsTime:
    case lPunchTotalTimeAfter:
      if (r && r->getCourse(false) && !invalidClass) {
        const pCourse crs=r->getCourse(true);
        const oControl *ctrl = nullptr;
        int nCtrl = crs->getNumControls();
        if (counter.level3 != nCtrl) { // Always allow finish
          ctrl=crs->getControl(counter.level3);
          if (!ctrl || ctrl->isRogaining(crs->hasRogaining()))
            break;
        }
        switch (pp.type) {
          case lPunchNamedSplit:
            if (ctrl && ctrl->hasName() && r->getPunchTime(counter.level3, false) > 0) {
              swprintf_s(wbf, L"%s", r->getNamedSplitS(counter.level3).c_str());
            }
          break;

          case lPunchNamedTime:
            if (ctrl && ctrl->hasName() && (!par.lineBreakControlList || r->getPunchTime(counter.level3, false) > 0)) {
              swprintf_s(wbf, L"%s: %s (%s)", ctrl->getName().c_str(),
                          r->getNamedSplitS(counter.level3).c_str(),
                          r->getPunchTimeS(counter.level3, false).c_str());
            }
          break;

          case lControlName:
          case lPunchName:
            if (ctrl && ctrl->hasName() && (!par.lineBreakControlList || r->getPunchTime(counter.level3, false) > 0)) {
              swprintf_s(wbf, L"%s", ctrl->getName().c_str());
            }
            else if (counter.level3 == nCtrl) {
              wsptr = &lang.tl(L"Mål");
            }
            break;

          case lPunchTime: {
            swprintf_s(wbf, L"%s (%s)",
                       r->getSplitTimeS(counter.level3, false).c_str(),
                       r->getPunchTimeS(counter.level3, false).c_str());
            break;
          }
          case lPunchSplitTime: {
            wcscpy_s(wbf, r->getSplitTimeS(counter.level3, false).c_str());
            break;
          }
          case lPunchTotalTime: {
            if (r->getPunchTime(counter.level3, false) > 0) {
              wcscpy_s(wbf, r->getPunchTimeS(counter.level3, false).c_str());
            }
            break;
          }
          case lPunchTotalTimeAfter: {
            if (r->getPunchTime(counter.level3, false) > 0) {
              int rt = r->getLegTimeAfterAcc(counter.level3);
              if (rt > 0)
                wcscpy_s(wbf, (L"+" + formatTime(rt)).c_str());
            }
            break;
          }
          case lPunchControlNumber: {
            wcscpy_s(wbf, crs->getControlOrdinal(counter.level3).c_str());
            break;
          }
          case lPunchControlCode: {
            const oControl *ctrl = crs->getControl(counter.level3);
            if (ctrl) {
              if (ctrl->getStatus() == oControl::StatusMultiple) {
                wstring str = ctrl->getStatusS();
                swprintf_s(wbf, L"%s.", str.substr(0, 1).c_str());
              }
              else
                swprintf_s(wbf, L"%d", ctrl->getFirstNumber());
            }
            break;
          }
          case lPunchControlPlace: {
            int p = r->getLegPlace(counter.level3);
            if (p > 0)
              swprintf_s(wbf, L"%d", p);
            break;
          }
          case lPunchControlPlaceAcc: {
            int p = r->getLegPlaceAcc(counter.level3);
            if (p > 0)
              swprintf_s(wbf, L"%d", p);
            break;
          }
          case lPunchLostTime: {
            wcscpy_s(wbf, r->getMissedTimeS(counter.level3).c_str());
            break;
          }
          case lPunchAbsTime: {
            int t = r->getPunchTime(counter.level3, false);
            if (t > 0)
              wsptr = &getAbsTime(r->tStartTime + t);
            break;
          }
        }
      }
      break;
    case lSubSubCounter:
      if (pp.text.empty())
        swprintf_s(wbf, L"%d.", counter.level3+1);
      else
        swprintf_s(wbf, L"%d", counter.level3+1);
      break;
    case lSubCounter:
      if (pp.text.empty())
        swprintf_s(wbf, L"%d.", counter.level2+1);
      else
        swprintf_s(wbf, L"%d", counter.level2+1);
      break;
    case lTotalCounter:
      if (pp.text.empty())
        swprintf_s(wbf, L"%d.", counter.level1+1);
      else
        swprintf_s(wbf, L"%d", counter.level1+1);
      break;
    case lClubName:
      wsptr = c != 0 ? &c->getDisplayName() : 0;
      break;
    case lResultModuleTime:
    case lResultModuleTimeTeam:

      if (pp.resultModuleIndex != -1) {
        if (t && pp.type == lResultModuleTimeTeam)
          wsptr = &t->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
        else if (r)
          wsptr = &r->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
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
          wsptr = &itow(nr);
        else {
          wstring &res = StringCache::getInstance().wget();
          MetaList::fromResultModuleNumber(pp.text.substr(1), nr, res);
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
            swprintf_s(wbf, L"%d, %dp, %s (%s)",
                      punch->Type, ctrl->getRogainingPoints(),
                      r->Card->getRogainingSplit(counter.level3, r->tStartTime).c_str(),
                      punch->getRunningTime(r->tStartTime).c_str());
          }
        }
      }
      break;

    case lLineBreak:
      break;
  }

  if (pp.type!=lString && (wsptr==0 || wsptr->empty()) && wbf[0]==0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf_s(wbf, pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = wbf;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = wbf;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf_s(bf2, pp.text.c_str(), wbf);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

bool oEvent::formatPrintPost(const list<oPrintPost> &ppli, PrintPostInfo &ppi,
                             const pTeam t, const pRunner r, const pClub c,
                             const pClass pc, const pCourse crs, const pControl ctrl,
                             const oPunch *punch, int legIndex)
{
  list<oPrintPost>::const_iterator ppit;
  int y=ppi.gdi.getCY();
  int x=ppi.gdi.getCX();
  bool updated=false;
  int lineHeight = 0;
  int pdx = 0,  pdy = 0;
  for (ppit = ppli.begin(); ppit != ppli.end();) {
    const oPrintPost &pp = *ppit;

    if (pp.type == lLineBreak) {
      x -= ppi.gdi.scaleLength(pp.dx) - pdx;
      pdx = ppi.gdi.scaleLength(pp.dx);
      y += lineHeight;
      ++ppit;
      continue;
    }

    int limit = ppit->xlimit;

    bool keepNext = false;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;

    // Main increment below
    if (++ppit != ppli.end() && ppit->dy == pp.dy)
      limit = ppit->dx - pp.dx;
    else
      keepNext = true;

    if (pp.useStrictWidth)
      limit = max(pp.fixedWidth - 5, 0); // Allow some space
    else
      limit = max(pp.fixedWidth, limit);

    assert(limit >= 0);
    pRunner rr = r;
    if (!rr && t) {
      if (pp.legIndex >= 0) {
        int lg = pc ? pc->getLinearIndex(pp.legIndex, pp.linearLegIndex) : pp.legIndex;
        rr = t->getRunner(lg);
      }
      else if (legIndex >= 0)
        rr = t->getRunner(legIndex);
      else {
        int lg = ppi.par.getLegNumber(pc);
        rr = t->getRunner(lg);
      }
    }
    const wstring &text = punch ? formatPunchString(pp, ppi.par, t, rr, punch, ppi.counter) :
      ((legIndex == -1) ? formatListString(pp, ppi.par, t, rr, c, pc, ppi.counter) :
       formatSpecialString(pp, ppi.par, t, legIndex, crs, ctrl, ppi.counter));
    updated |= !text.empty();

    TextInfo *ti = 0;
    if (!text.empty()) {
      pdy = ppi.gdi.scaleLength(pp.dy);
      pdx = ppi.gdi.scaleLength(pp.dx);
      if ((pp.type == lRunnerName || pp.type == lRunnerCompleteName ||
          pp.type == lRunnerFamilyName || pp.type == lRunnerGivenName ||
          pp.type == lTeamRunner || (pp.type == lPatrolNameNames && !t)) && rr) {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx, pp.format | skipBoundingBox, text,
                                  ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(rr->getId());
        ti->id = "R";
      }
      else if ((pp.type == lTeamName || pp.type == lPatrolNameNames) && t) {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx, pp.format | skipBoundingBox, text,
                                  ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(t->getId());
        ti->id = "T";
      }
      else {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx,
                                  pp.format | skipBoundingBox, text, ppi.gdi.scaleLength(limit), 0, pp.fontFace.c_str());
      }
      if (ti && ppi.keepToghether)
        ti->lineBreakPrioity = -1;

      if (ti) {
        lineHeight = ti->getHeight();

      }

      if (pp.color != colorDefault)
        ti->setColor(pp.color);
    }
    ppi.keepToghether |= keepNext;

  }

  return updated;
}

void oEvent::calculatePrintPostKey(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                                   const pTeam t, const pRunner r, const pClub c,
                                   const pClass pc, oCounter &counter, wstring &key)
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
    const wstring &text = formatListString(pp, par, t, rr, c, pc, counter);
    key += text;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;
  }
}

void oEvent::listGeneratePunches(const oListInfo &listInfo, gdioutput &gdi, 
                                 pTeam t, pRunner r, pClub club, pClass cls) {
  const list<oPrintPost> &ppli = listInfo.subListPost;
  const oListParam &par = listInfo.lp;
  auto type = listInfo.listSubType;
  if (!r || ppli.empty())
    return;
  bool filterNamed = listInfo.subFilter(ESubFilterNamedControl);
  const bool filterFinish = listInfo.subFilter(ESubFilterNotFinish);

  pCourse crs = r->getCourse(true);

  if (cls && cls->getNoTiming())
    return;
  if (r && (r->getStatusComputed() == StatusNoTiming || r->noTiming()))
    return;

  int h = gdi.getLineHeight();
  int w = 0;
  bool newLine = false;
  int haccum = 0;
  for (auto &pl : ppli) {
    if (pl.type == lPunchNamedTime)
      filterNamed = true;
    if (pl.type == lLineBreak) {
      haccum += h;
      h = 0;
      newLine = true;
      continue;
    }
    h = max(h, gdi.getLineHeight(pl.getFont(), pl.fontFace.c_str()) + gdi.scaleLength(pl.dy));
    w = max(w, gdi.scaleLength(pl.fixedWidth + pl.dx));
  }
  h += haccum;
  int xlimit = gdi.getCX() + gdi.scaleLength(600);
  par.lineBreakControlList = newLine; // Controls if controls names are printed even if the runner has not punched there yet.

  if (w > 0) {
    gdi.pushX();
    gdi.fillNone();
  }

  bool neednewline = false;
  bool updated = false;

  int limit = crs ? crs->nControls + 1 : 1;

  if (r->Card && r->Card->getNumPunches() > limit)
    limit = r->Card->getNumPunches();

  vector<char> skip(limit, false);
  if (filterNamed && crs) {
    for (int k = 0; k < crs->nControls; k++) {
      if (crs->getControl(k) && !crs->getControl(k)->hasName())
        skip[k] = true;
    }
    for (int k = crs->nControls + 1; k < limit; k++) {
      skip[k] = true;
    }
    if (filterFinish)
      skip[crs->nControls] = true;
  }
  bool filterRadioTimes = listInfo.sortOrder == SortOrder::ClassLiveResult || listInfo.filter(EFilterList::EFilterAnyResult);
  if (filterRadioTimes && crs) {
    for (int k = 0; k < crs->nControls; k++) {
      if (crs->getControl(k) && !crs->getControl(k)->isValidRadio())
        skip[k] = true;
    }
    for (int k = crs->nControls + 1; k < limit; k++) {
      skip[k] = true;
    }
    if (filterFinish)
      skip[crs->nControls] = true;
  }
  PrintPostInfo ppi(gdi, par);
  if (type == oListInfo::EBaseType::EBaseTypeCoursePunches) {
    for (int k = 0; k < limit; k++) {
      if (w > 0 && updated) {
        updated = false;
        if (gdi.getCX() + w > xlimit || newLine) {
          neednewline = false;
          gdi.popX();
          gdi.setCY(gdi.getCY() + h);
        }
        else
          gdi.setCX(gdi.getCX() + w);
      }

      if (!skip[k]) {
        updated |= formatPrintPost(ppli, ppi, t, r, club, cls,
                                   nullptr, nullptr, nullptr, -1);
        neednewline |= updated;
      }
      ppi.counter.level3++;
    }
  }
  else if(type == oListInfo::EBaseType::EBaseTypeAllPunches) {
    int startType = -1;
    int finishType = -1;
    const pCourse pcrs = r->getCourse(false);

    if (pcrs) {
      startType = pcrs->getStartPunchType();
      finishType = pcrs->getFinishPunchType();
    }
    int prevPunchTime = r->getStartTime();
    vector<pPunch> punches;
    if (r->Card) {
      for (auto &punch : r->Card->punches)
        punches.push_back(&punch);
    }
    else {
      vector<pFreePunch> fPunches;
      oe->getPunchesForRunner(r->getId(), true, fPunches);
      for (auto punch : fPunches)
        punches.push_back(punch);
    }

    for (auto &pPunch : punches) {
      const oPunch &punch = *pPunch;
      punch.previousPunchTime = prevPunchTime;

      if (punch.isCheck() || punch.isStart(startType))
        continue;
      if (filterFinish && punch.isFinish(finishType))
        continue;
      prevPunchTime = punch.Time;
      if (w > 0 && updated) {
        updated = false;
        if (gdi.getCX() + w > xlimit || newLine) {
          neednewline = false;
          gdi.popX();
          gdi.setCY(gdi.getCY() + h);
        }
        else
          gdi.setCX(gdi.getCX() + w);
      }

      updated |= formatPrintPost(ppli, ppi, t, r, club, cls,
                                  nullptr, nullptr, &punch, -1);
      neednewline |= updated;
      ppi.counter.level3++;
    }
  }
  if (w > 0) {
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
  oe->calculateNumRemainingMaps(false);
  oe->updateComputerTime();
  oe->setGeneralResultContext(&li.lp);
  
  wstring listname;
  if (!li.Head.empty()) {
    oCounter counter;
    const wstring &name = formatListString(li.Head.front(), li.lp, 0, 0, 0, 0, counter);
    listname = name;
    li.lp.updateDefaultName(name);
  }
  bool addHead = li.lp.showHeader && !li.lp.useLargeSize;
  size_t nClassesSelected = li.lp.selection.size();
  if (nClassesSelected!=0 && nClassesSelected < min(Classes.size(), Classes.size()/2+5) ) {
    // Non-trivial class selection:
    Classes.sort();
    wstring cls;
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (li.lp.selection.count(it->getId())) {
        cls += makeDash(L" - ");
        cls += it->getName();
      }
    }
    listname += cls;
  }

  if (li.lp.getLegNumberCoded() != -1) {
    listname += lang.tl(L" Sträcka X#" + li.lp.getLegName());
  }

  generateListInternal(gdi, li, addHead);
  
  for (list<oListInfo>::const_iterator it = li.next.begin(); it != li.next.end(); ++it) {
    bool interHead = addHead && it->getParam().showInterTitle;
    if (li.lp.pageBreak || it->lp.pageBreak) {
      gdi.dropLine(1.0);
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
    }
    else if (interHead) {
      gdi.dropLine(1.5);
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewChapter, "");
    }
    else {
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
    }

    generateListInternal(gdi, *it, interHead);
  }
  // Reset context
  oe->setGeneralResultContext(nullptr);

  gdi.setListDescription(listname);
  if (updateScrollBars)
    gdi.updateScrollbars();
}

bool oListInfo::filterRunner(const oRunner &r) const {
  if (r.isRemoved()) 
    return true;

  if (r.tStatus == StatusNotCompetiting && !filter(EFilterIncludeNotParticipating))
    return true;

  if (!lp.selection.empty() && lp.selection.count(r.getClassId(true)) == 0)
    return true;

  if (!lp.matchLegNumber(r.getClassRef(false), r.legToRun()))
    return true;

  if (lp.ageFilter != oListParam::AgeFilter::All) {
    int age = r.getBirthAge();
    if (age > 0) {
      oDataConstInterface odc = r.getEvent()->getDCI();
      int lowAgeLimit = odc.getInt("YouthAge");
      //int highAgeLimit = odc.getInt("SeniorAge");


      if (lp.ageFilter == oListParam::AgeFilter::ExludeYouth &&
          age <= lowAgeLimit)
        return true;
        
      if (lp.ageFilter == oListParam::AgeFilter::OnlyYouth &&
          age > lowAgeLimit)
        return true;
    }
    else {
      // Consider "normal"
      if (lp.ageFilter != oListParam::AgeFilter::ExludeYouth)
        return true;
    }
  }

  if (filter(EFilterExcludeDNS)) {
    if (r.tStatus == StatusDNS)
      return true;
    if (r.Class && r.Class->isQualificationFinalBaseClass()) {
      if (r.getLegNumber() > 0 && r.getClassRef(true) == r.Class)
        return true; //Not qualified -> out
    }
  }
  if (filter(EFilterExcludeCANCEL) && r.tStatus == StatusCANCEL)
    return true;

  if (filter(EFilterVacant)) {
    if (r.isVacant())
      return true;
  }
  if (filter(EFilterOnlyVacant)) {
    if (!r.isVacant())
      return true;
  }

  if (filter(EFilterAnyResult)) {
    if (!r.hasOnCourseResult())
      return true;
  }

  if (filter(EFilterAPIEntry)) {
    if (!r.hasFlag(oRunner::FlagAddedViaAPI))
      return true;
  }

  if (filter(EFilterWrongFee)) {
    if (r.getEntryFee() == r.getDefaultFee())
      return true;
  }

  if (filter(EFilterRentCard) && r.getDCI().getInt("CardFee") == 0)
    return true;

  if (filter(EFilterHasCard) && r.getCardNo() == 0)
    return true;

  if (filter(EFilterHasNoCard) && r.getCardNo() > 0)
    return true;

  return false;
}

bool oListInfo::filterRunnerResult(GeneralResult *gResult, const oRunner &r) const {
  if (gResult && r.getTempResult(0).getStatus() == StatusNotCompetiting)
    return true;

  if (filter(EFilterHasResult)) {
    if (gResult == 0) {
      if (lp.useControlIdResultTo <= 0 && !r.hasResult())
        return true;
      else if ((lp.useControlIdResultTo > 0 || lp.useControlIdResultFrom > 0) && r.tempStatus != StatusOK)
        return true;
      else if (calcTotalResults && r.getTotalStatus() == StatusUnknown)
        return true;
    }
    else {
      auto &res = r.getTempResult(0);
      RunnerStatus st = res.getStatus();
      if (st == StatusUnknown || isPossibleResultStatus(st) && r.getRunningTime(false) <= 0)
        return true;
    }
  }
  else if (filter(EFilterHasPrelResult)) {
    if (gResult == 0) {
      if (lp.useControlIdResultTo <= 0 && (r.tStatus == StatusUnknown || isPossibleResultStatus(r.getStatusComputed())) && r.getRunningTime(false) <= 0)
        return true;
      else if ((lp.useControlIdResultTo > 0 || lp.useControlIdResultFrom > 0) && r.tempStatus != StatusOK)
        return true;
      else if (calcTotalResults && r.getTotalStatus() == StatusUnknown && r.getTotalRunningTime() <= 0)
        return true;
    }
    else {
      auto &res = r.getTempResult(0);
      int rt = res.getRunningTime();
      RunnerStatus st = res.getStatus();
      if ((st == StatusUnknown || isPossibleResultStatus(st)) && rt <= 0)
        return true;
    }
  }
  return false;
}

GeneralResult *oListInfo::applyResultModule(oEvent &oe, vector<pRunner> &rlist) const {
  GeneralResult *gResult = nullptr;
  if (!resultModule.empty()) {
    wstring src;
    oListInfo::ResultType resType = getResultType();
    gResult = oe.getGeneralResult(resultModule, src).get();
    gResult->calculateIndividualResults(rlist, false, resType, sortOrder == Custom, getParam().getInputNumber());

    if (sortOrder == SortByFinishTime || sortOrder == SortByFinishTimeReverse || sortOrder == SortByStartTime)
      gResult->sort(rlist, sortOrder);
  }
  return gResult;
}

void oEvent::generateListInternal(gdioutput &gdi, const oListInfo &li, bool formatHead) {
  li.setupLinks();
  pClass sampleClass = 0;

  if (!li.lp.selection.empty())
    sampleClass = getClass(*li.lp.selection.begin());
  if (!sampleClass && !Classes.empty())
    sampleClass = &*Classes.begin();

  if (li.listType == li.EBaseTypeRunner) {
    if (li.calculateLiveResults || li.sortOrder == SortOrder::ClassLiveResult)
      calculateResults(li.lp.selection, ResultType::PreliminarySplitResults);

    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);

    if (li.calcCourseResults)
      calculateResults(li.lp.selection, ResultType::CourseResult);

    if (li.calcTotalResults) {
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
      calculateResults(li.lp.selection, ResultType::TotalResult);
    }

    if (li.calcResults) {
      if (li.lp.useControlIdResultTo > 0 || li.lp.useControlIdResultFrom > 0)
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
      else {
        calculateTeamResults(li.lp.selection, ResultType::ClassResult);
        calculateResults(li.lp.selection, ResultType::ClassResult);
      }
    }
  }
  else if (li.listType == li.EBaseTypeTeam) {
    if (li.calcResults)
      calculateTeamResults(li.lp.selection, ResultType::ClassResult);
    if (li.calcTotalResults)
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
    if (li.calcCourseResults)
      calculateTeamResults(li.lp.selection, ResultType::CourseResult);

    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);
  }
  else if (li.listType == li.EBaseTypeClub) {
    if (li.calcResults) {
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
      calculateTeamResults(li.lp.selection, ResultType::ClassResult);
    }
    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);
    if (li.calcCourseResults)
      calculateResults(li.lp.selection, ResultType::CourseResult);

    //pair<int, bool> info = li.lp.getLegInfo(sampleClass);
    //sortTeams(li.sortOrder, info.first, info.second);
    if (li.calcResults) {
      if (li.lp.useControlIdResultTo > 0 || li.lp.useControlIdResultFrom > 0)
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
      else {
        calculateResults(li.lp.selection, ResultType::ClassResult);
      }
    }
  }

  PrintPostInfo printPostInfo(gdi, li.lp);
  //oCounter counter;
  //Render header
  vector< pair<EPostType, wstring> > v;
  for (auto &listPostList : { &li.Head, &li.subHead, &li.listPost, &li.subListPost }) {
    for (auto &lp : *listPostList) {
      if (lp.xlimit == 0) {
        v.clear();
        v.emplace_back(lp.type, lp.text);
        gdiFonts font = lp.getFont();
        lp.xlimit = li.getMaxCharWidth(this, gdi, li.getParam().selection, v, font, lp.fontFace.c_str());
      }
    }
  }

  if (formatHead && li.getParam().showHeader) {
    for (auto &h : li.Head) {
      if (h.type == lCmpName || h.type == lString) {
        v.clear();
        const_cast<wstring&>(h.text) = li.lp.getCustomTitle(h.text);
        v.emplace_back(h.type, h.text);
        gdiFonts font = h.getFont();
        h.xlimit = li.getMaxCharWidth(this, gdi, li.getParam().selection, v, font, h.fontFace.c_str());
        break;
      }
    }
    formatPrintPost(li.Head, printPostInfo, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr, -1);
  }
  if (li.fixedType) {
    generateFixedList(gdi, li);
    return;
  }
     
  // Apply for all teams (calculate start times etc.)
  
  vector<pTeam> tlist;
  tlist.reserve(Teams.size());
  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved() || it->tStatus == StatusNotCompetiting)
      continue;

    if (!li.lp.selection.empty() && li.lp.selection.count(it->getClassId(true)) == 0)
      continue;
    it->apply(oBase::ChangeType::Quiet, nullptr);
    tlist.push_back(&*it);
  }

  wstring oldKey;
  if (li.listType == li.EBaseTypeRunner) {
    vector<pRunner> rlist, rlistInput;
    getRunners(li.lp.selection, rlistInput, false);
    rlist.reserve(rlistInput.size());
    for (auto r : rlistInput) {
      if (!li.filterRunner(*r))
        rlist.push_back(r);
    }

    if (li.sortOrder != Custom)
      sortRunners(li.sortOrder, rlist);

    GeneralResult *gResult = li.applyResultModule(*this, rlist);

    for (size_t k = 0; k < rlist.size(); k++) {
      pRunner it = rlist[k];
      if (li.filterRunnerResult(gResult, *it))
        continue;
 
      wstring newKey;
      printPostInfo.par.relayLegIndex = -1;
      calculatePrintPostKey(li.subHead, gdi, li.lp, it->tInTeam, &*it, it->Club, it->getClassRef(true), printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        gdi.addStringUT(pagePageInfo, it->getClass(true));

        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = -1;
        formatPrintPost(li.subHead, printPostInfo, it->tInTeam, &*it, it->Club, it->getClassRef(true),
                        nullptr, nullptr, nullptr, -1);
      }
      if (li.lp.filterMaxPer == 0 || printPostInfo.counter.level2 < li.lp.filterMaxPer) {
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = it->tLeg;
        formatPrintPost(li.listPost, printPostInfo, it->tInTeam, &*it, it->Club, it->getClassRef(true),
                        nullptr, nullptr, nullptr, -1);

        if (li.listSubType == li.EBaseTypeCoursePunches ||
            li.listSubType == li.EBaseTypeAllPunches) {
          listGeneratePunches(li, gdi, it->tInTeam, &*it, it->Club, it->getClassRef(true));
        }
      }
      ++printPostInfo.counter;
    }
  }
  else if (li.listType == li.EBaseTypeTeam) {
    if (li.sortOrder != SortOrder::Custom) {
      pair<int, bool> legInfo = li.lp.getLegInfo(sampleClass);
      sortTeams(li.sortOrder, legInfo.first, legInfo.second, tlist);
    }
    
    GeneralResult *gResult = 0;
    if (!li.resultModule.empty()) {
      wstring src;
      gResult = getGeneralResult(li.resultModule, src).get();
      oListInfo::ResultType resType = li.getResultType();
      gResult->calculateTeamResults(tlist, false, resType, li.sortOrder == Custom, li.getParam().getInputNumber());
    }
    // Range of runners to include
    int parLegRangeMin = 0, parLegRangeMax = 1000;
    pClass parLegRangeClass = 0;
    const bool needParRange = li.subFilter(ESubFilterSameParallel)
      || li.subFilter(ESubFilterSameParallelNotFirst);

    for (size_t k = 0; k < tlist.size(); k++) {
      pTeam it = tlist[k];
      int linearLegSpec = li.lp.getLegNumber(it->getClassRef(false));

      if (gResult && it->getTempResult(0).getStatus() == StatusNotCompetiting)
        continue;

      if (li.filter(EFilterExcludeDNS))
        if (it->tStatus == StatusDNS)
          continue;

      if (li.filter(EFilterVacant))
        if (it->isVacant())
          continue;

      if (li.filter(EFilterOnlyVacant)) {
        if (!it->isVacant())
          continue;
      }

      if (li.filter(EFilterHasResult)) {
        if (gResult) {
          RunnerStatus st = it->getTempResult(0).getStatus();
          if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getTempResult(0).getRunningTime()<=0))
            continue;
        }
        else {
          RunnerStatus st = it->getLegStatus(linearLegSpec, true, false);
          if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getLegRunningTime(linearLegSpec, true, false) <=0))
            continue;
          else if (li.calcTotalResults) {
            st = it->getLegStatus(linearLegSpec, true, true); 
            if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getLegRunningTime(linearLegSpec, true, true) <= 0))
              continue;
          }
        }
      }
      else if (li.filter(EFilterHasPrelResult)) {
        if (gResult) {
          RunnerStatus st = it->getTempResult(0).getStatus();
          if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getTempResult(0).getRunningTime() <= 0)
            continue;
        }
        else {
          RunnerStatus st = it->getLegStatus(linearLegSpec, true, false);
          if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getLegRunningTime(linearLegSpec, true, false) <= 0)
            continue;
          else if (li.calcTotalResults) {
            RunnerStatus st = it->getLegStatus(linearLegSpec, true, true);

            if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getLegRunningTime(linearLegSpec, true, true) <= 0)
              continue;
          }
        }
      }

      if (needParRange && it->Class != parLegRangeClass && it->Class) {
        parLegRangeClass = it->Class;
        parLegRangeClass->getParallelRange(linearLegSpec < 0 ? 0 : linearLegSpec, parLegRangeMin, parLegRangeMax);
        if (li.subFilter(ESubFilterSameParallelNotFirst))
          parLegRangeMin++;
      }

      wstring newKey;
      printPostInfo.par.relayLegIndex = linearLegSpec;
      calculatePrintPostKey(li.subHead, gdi, li.lp, &*it, 0, it->Club, it->Class, printPostInfo.counter, newKey);
      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        wstring legInfo;
        if (linearLegSpec >= 0 && it->getClassRef(false)) {
          // Specified leg
          legInfo = lang.tl(L", Str. X#" + li.lp.getLegName());
        }

        gdi.addStringUT(pagePageInfo, it->getClass(true) + legInfo); // Teamlist

        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = linearLegSpec;
        formatPrintPost(li.subHead, printPostInfo, &*it, 0, it->Club, it->Class,
                        nullptr, nullptr, nullptr, -1);
      }
      ++printPostInfo.counter;
      if (li.lp.filterMaxPer == 0 || printPostInfo.counter.level2 <= li.lp.filterMaxPer) {
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = linearLegSpec;
        formatPrintPost(li.listPost, printPostInfo, &*it, 0, it->Club, it->Class,
                        nullptr, nullptr, nullptr, -1);

        if (li.subListPost.empty())
          continue;

        if (li.listSubType == li.EBaseTypeRunner) {
          int nr = int(it->Runners.size());
          vector<pRunner> tr;
          tr.reserve(nr);
          vector<int> usedIx(nr, -1);

          for (int k = 0; k < nr; k++) {
            if (!it->Runners[k]) {
              if (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult) ||
                  li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult) ||
                  li.filter(EFilterExcludeDNS) || li.filter(EFilterExcludeCANCEL) || li.subFilter(ESubFilterExcludeDNS) ||
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
            bool cancelled = false;
            if (gResult == 0) {
              noResult = it->Runners[k]->tStatus == StatusUnknown;
              noPrelResult = it->Runners[k]->tStatus == StatusUnknown && it->Runners[k]->getRunningTime(false) <= 0;
              noStart = it->Runners[k]->tStatus == StatusDNS || it->Runners[k]->tStatus == StatusCANCEL;
              if (it->Runners[k]->Class && it->Runners[k]->Class->isQualificationFinalBaseClass()) {
                if (k > 0 && it->Runners[k]->getClassRef(true) == it->Runners[k]->Class)
                  noStart = true;
              }
              
              cancelled = it->Runners[k]->tStatus == StatusCANCEL;
              //XXX TODO Multiday
            }
            else {
              noResult = it->Runners[k]->tmpResult.status == StatusUnknown;
              noPrelResult = it->Runners[k]->tmpResult.status == StatusUnknown && it->Runners[k]->tmpResult.runningTime <= 0;
              noStart = it->Runners[k]->tmpResult.status == StatusDNS || it->Runners[k]->tmpResult.status == StatusCANCEL;
              cancelled = it->Runners[k]->tmpResult.status == StatusCANCEL;
            }

            if (noResult && (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult)))
              continue;

            if (noPrelResult && (li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult)))
              continue;

            if (noStart && (li.filter(EFilterExcludeDNS) || li.subFilter(ESubFilterExcludeDNS)))
              continue;

            if (cancelled && li.filter(EFilterExcludeCANCEL))
              continue;

            if (it->Runners[k]->isVacant() && li.subFilter(ESubFilterVacant))
              continue;

            if ((it->Runners[k]->tLeg < parLegRangeMin || it->Runners[k]->tLeg > parLegRangeMax)
                && needParRange)
              continue;

            usedIx[k] = tr.size();
            tr.push_back(it->Runners[k]);
          }

          if (gResult) {
            gResult->sortTeamMembers(tr);

            for (size_t k = 0; k < tr.size(); k++) {
              bool suitableBreak = k < 2 || (k + 2) >= tr.size();
              printPostInfo.keepToghether = suitableBreak;
              printPostInfo.par.relayLegIndex = tr[k] ? tr[k]->tLeg : -1;
              formatPrintPost(li.subListPost, printPostInfo, &*it, tr[k],
                              it->Club, tr[k]->getClassRef(true),
                              nullptr, nullptr, nullptr, -1);
              printPostInfo.counter.level3++;
            }
          }
          else {
            for (size_t k = 0; k < usedIx.size(); k++) {
              if (usedIx[k] == -2)
                continue; // Skip
              bool suitableBreak = k < 2 || (k + 2) >= usedIx.size();
              printPostInfo.keepToghether = suitableBreak;
              printPostInfo.par.relayLegIndex = k;
              if (usedIx[k] == -1) {
                pCourse crs = it->Class ? it->Class->getCourse(k, it->StartNo) : 0;
                formatPrintPost(li.subListPost, printPostInfo, &*it, 0,
                                it->Club, it->Class, crs, nullptr, nullptr, k);
              }
              else {
                formatPrintPost(li.subListPost, printPostInfo, &*it, tr[usedIx[k]],
                                it->Club, tr[usedIx[k]]->getClassRef(true),
                                nullptr, nullptr, nullptr, -1);
              }
              printPostInfo.counter.level3++;
            }
          }
        }
        else if (li.listSubType == li.EBaseTypeCoursePunches ||
                 li.listSubType == li.EBaseTypeAllPunches) {
          pRunner r = it->Runners.empty() ? 0 : it->Runners[0];
          if (!r) continue;

          listGeneratePunches(li, gdi, &*it, r, it->Club, it->Class);
        }
      }
    }
  }
  else if (li.listType == li.EBaseTypeClub) {
    Clubs.sort();
    oClubList::iterator it;
    oRunnerList::iterator rit;

    map<int, vector<pRunner>> clubToRunner;

    vector<pRunner> rlist;
    rlist.reserve(Runners.size());
    for (auto &r : Runners) {
      if (!li.filterRunner(r))
        rlist.push_back(&r);
    }
    if (li.sortOrder != Custom)
      sortRunners(li.sortOrder, rlist);
    GeneralResult *gResult = li.applyResultModule(*this, rlist);

    for (pRunner r : rlist) {
      int club = r->getClubId();
      if (club == 0)
        continue;

      clubToRunner[club].push_back(r);
    }

    bool first = true;
    for (it = Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (li.filter(EFilterVacant)) {
        if (it->getId() == getVacantClub(false))
          continue;
      }

      if (li.filter(EFilterOnlyVacant)) {
        if (it->getId() != getVacantClub(false))
          continue;
      }

      bool startClub = false;

      for (auto rit : clubToRunner[it->getId()]) {
        if (li.filterRunnerResult(gResult, *rit))
          continue;

        if (!startClub) {
          if (!first)
            gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
          else
            first = false;
          
          gdi.addStringUT(pagePageInfo, it->getName());
          printPostInfo.counter.level2 = 0;
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          printPostInfo.par.relayLegIndex = -1;
          formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, &*it,
                          nullptr, nullptr, nullptr, nullptr, -1);
          startClub = true;
        }
        ++printPostInfo.counter;
        if (li.lp.filterMaxPer == 0 || printPostInfo.counter.level2 <= li.lp.filterMaxPer) {
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          printPostInfo.par.relayLegIndex = rit->tLeg;
          formatPrintPost(li.listPost, printPostInfo, nullptr, &*rit, &*it, rit->getClassRef(true),
                          nullptr, nullptr, nullptr, -1);

          if (li.listSubType == li.EBaseTypeCoursePunches ||
              li.listSubType == li.EBaseTypeAllPunches) {
            listGeneratePunches(li, gdi, rit->tInTeam, &*rit, rit->Club, rit->getClassRef(true));
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

      wstring newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, nullptr,
                        nullptr, nullptr, nullptr, &*it, nullptr, nullptr, 0);
      }
      if (li.lp.filterMaxPer == 0 || printPostInfo.counter.level2 < li.lp.filterMaxPer) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, &*it, nullptr, nullptr, 0);

        if (li.listSubType == li.EBaseTypeControl) {
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

      wstring newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, nullptr, &*it, nullptr, 0);
      }
      if (li.lp.filterMaxPer == 0 || printPostInfo.counter.level2 < li.lp.filterMaxPer) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, nullptr, &*it, nullptr, 0);
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

  vector< pair<wstring, size_t> > v;
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
    li.Name=lang.tl(L"Startlista, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportFrom = true;
    li.supportTo = true;
    listMap[EStdResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, generell");
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
    li.Name=lang.tl(L"Rogaining, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[ERogainingInd]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Avgjorda klasser (prisutdelningslista)");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EIndPriceList]=li;
  }
  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl(L"Startlista, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Patrullresultat (STOR)");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    li.largeSize = true;
    listMap[EStdPatrolResultListLARGE]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat (STOR)");
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
    li.Name=lang.tl(L"Hyrbricksrapport");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = false;
    li.supportLegs = false;
    listMap[EStdRentedCard]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Stafettresultat, delsträckor");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdTeamResultListAll]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Stafettresultat, lag");
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
      li.Name=lang.tl(L"Startlista, stafett (lag)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EStdTeamStartList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Startlista, stafett (sträcka)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdTeamStartListLeg]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Bantilldelning, stafett");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[ETeamCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Bantilldelning, individuell");
      li.listType=li.EBaseTypeRunner;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EIndCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Individuell startlista, visst lopp");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdIndMultiStartListLeg]=li;
    }
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Individuell resultatlista, visst lopp");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdIndMultiResultListLeg]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Individuell resultatlista, visst lopp (STOR)");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    li.largeSize = true;
    listMap[EStdIndMultiResultListLegLARGE]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl(L"Individuell resultatlista, alla lopp");
    li.listType = li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdIndMultiResultListAll]=li;
  }

  if (!filterResults) {
    oListInfo li;
    li.Name = lang.tl(L"Klubbstartlista");
    li.listType = li.EBaseTypeClub;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubStartList]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl(L"Klubbresultatlista");
    li.listType = li.EBaseTypeClub;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubResultList]=li;
  }

  if (!filterResults) {
    {
      oListInfo li;
      li.Name=lang.tl(L"Tävlingsrapport");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Kontroll inför tävlingen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedPreReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Kvar-i-skogen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInForest]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Fakturor");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInvoices]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Ekonomisk sammanställning");
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
    li.Name=lang.tl(L"Minutstartlista");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedMinuteStartlist]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Händelser - tidslinje");
    li.supportClasses = true;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedTimeLine]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Liveresultat, deltagare");
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
    li.Name=lang.tl(L"Stafettresultat, sträcka (STOR)");
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

  generateListInfo(par, li);
}

int openRunnerTeamCB(gdioutput *gdi, int type, void *data);

void oEvent::generateFixedList(gdioutput &gdi, const oListInfo &li)
{
  wstring dmy;
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
      gdi.addString("", boldLarge, makeDash(L"Tidslinje - X#") + getName());

      gdi.dropLine();
      set<__int64> stored;
      vector<oTimeLine> events;

      map<int, wstring> cName;
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

        wstring name = L"";
        if (r)
          name = r->getCompleteIdentification() + L" ";

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

void oEvent::generateListInfo(oListParam &par, oListInfo &li) {
  vector<oListParam> parV(1, par);
  generateListInfo(parV, li);
}

void oEvent::generateListInfo(vector<oListParam> &par, oListInfo &li) {
  li.getParam().sourceParam = -1;// Reset source
  loadGeneralResults(false, false);
  for (size_t k = 0; k < par.size(); k++) {
    par[k].cb = 0;
  }

  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, false);

  if (par.size() == 1) {
    generateListInfoAux(par[0], li, listMap[par[0].listCode].Name);
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
      generateListInfoAux(nextPar, li.next.back(), L"");
      cPar = &nextPar;
    }
  }
  else {
    for (size_t k = 0; k < par.size(); k++) {
      if (k > 0) {
        li.next.push_back(oListInfo());
      }
      generateListInfoAux(par[k], k == 0 ? li : li.next.back(), 
                          li.Name = listMap[par[0].listCode].Name);
    }
  }
}

void oEvent::generateListInfoAux(oListParam &par, oListInfo &li, const wstring &name) {
  const int lh=14;
  const int vspace=lh/2;
  int bib;
  pair<int, bool> ln;
  const double scale = 1.8;

  wstring ttl;
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
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Startlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      int bib = 0;
      int rank = 0;
      if (hasBib(true, false)) {
        li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lRunnerStart, L"", normalText, 0+bib, 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, 70+bib, 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, 300+bib, 0));
      if (hasRank()) {
        li.addListPost(oPrintPost(lRunnerRank, L"", normalText, 470+bib, 0));
        rank = 50;
      }
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 470+bib+rank, 0));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lClassLength, lang.tl(L"%s meter"), boldText, 300+bib, 12));
      li.addSubHead(oPrintPost(lClassStartName, L"", boldText, 470+bib+rank, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 470+bib, 16));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;
    }

    case EStdClubStartList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Klubbstartlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      if (hasBib(true, true)) {
        pos.add("bib", 40);
        li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0, 0));
      }

      pos.add("start", li.getMaxCharWidth(this, par.selection, lRunnerStart, L"", normalText));
      li.addListPost(oPrintPost(lRunnerStart, L"", normalText, pos.get("start"), 0));

      if (hasRank()) {
        pos.add("rank", 50);
        li.addListPost(oPrintPost(lRunnerRank, L"", normalText, pos.get("rank"), 0));
      }
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, L"", normalText));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      pos.add("class", li.getMaxCharWidth(this, par.selection, lClassName, L"", normalText));
      li.addListPost(oPrintPost(lClassName, L"", normalText, pos.get("class"), 0));
      pos.add("length", li.getMaxCharWidth(this, par.selection, lClassLength, L"%s m", normalText));
      li.addListPost(oPrintPost(lClassLength, lang.tl(L"%s m"), normalText, pos.get("length"), 0));
      pos.add("sname", li.getMaxCharWidth(this, par.selection, lClassStartName, L"", normalText));
      li.addListPost(oPrintPost(lClassStartName, L"", normalText, pos.get("sname"), 0));

      pos.add("card", 70);
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, pos.get("card"), 0));

      li.addSubHead(oPrintPost(lClubName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 100, 16));

      li.listType=li.EBaseTypeClub;
      li.sortOrder=ClassTeamLeg;
      li.setFilter(EFilterExcludeDNS);
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdClubResultList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Klubbresultatlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      pos.add("class", li.getMaxCharWidth(this, par.selection, lClassName, L"", normalText));
      li.addListPost(oPrintPost(lClassName, L"", normalText, pos.get("class"), 0));
       
      pos.add("place", 40);
      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));

      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, L"", normalText));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, pos.get("name"), 0));

      pos.add("time", li.getMaxCharWidth(this, par.selection, lRunnerTimeStatus, L"", normalText));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("time"), 0));

      pos.add("after", li.getMaxCharWidth(this, par.selection, lRunnerTimeAfter, L"", normalText));
      li.addListPost(oPrintPost(lRunnerTimeAfter, L"", normalText, pos.get("after"), 0));

      li.addSubHead(oPrintPost(lClubName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 100, 16));

      li.listType=li.EBaseTypeClub;
      li.sortOrder=ClassResult;
      li.calcResults = true;
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdRentedCard:
    {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Hyrbricksrapport - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      li.addListPost(oPrintPost(lTotalCounter, L"%s", normalText, 0, 0));
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 30, 0));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 130, 0));
      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));

      li.setFilter(EFilterHasCard);
      li.setFilter(EFilterRentCard);
      li.setFilter(EFilterExcludeDNS);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      break;
    }

    case EStdResultList: {
      wstring stitle;
      getResultTitle(*this, li.lp, stitle);
      stitle = par.getCustomTitle(stitle);

      li.addHead(oPrintPost(lCmpName, makeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, L"", normalText, 0, false, 25));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, L"", normalText, 0, false, 25));
      pos.add("status", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, pos.get("club"), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("club"), 10));

        li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTimeAfter, L"", normalText, pos.get("after"), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, L"", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, L"", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, L"", normalText, pos.get("after"), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, pos.get("after"), 10));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, pos.get("missed"), 0));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, pos.get("missed"), 10));
      }

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.supportFrom = true;
      li.supportTo = true;
      li.setFilter(EFilterHasPrelResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;
    }
    case EGeneralResultList: {
      wstring stitle;
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

      li.addHead(oPrintPost(lCmpName, makeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", li.getMaxCharWidth(this, par.selection, lRunnerGeneralPlace, L"", normalText));
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerCompleteName, L"", normalText));
      pos.add("status", li.getMaxCharWidth(this, par.selection, lRunnerGeneralTimeStatus, L"", normalText));
      pos.add("after", li.getMaxCharWidth(this, par.selection, lRunnerGeneralTimeAfter, L"", normalText));
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, L"", header, 0, 10));

      li.addListPost(oPrintPost(lRunnerGeneralPlace, L"", normal, pos.get("place", s), 0));
      li.addListPost(oPrintPost(lRunnerCompleteName, L"", normal, pos.get("name", s), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addListPost(oPrintPost(lRunnerGeneralTimeStatus, L"", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerGeneralTimeAfter, L"", normal, pos.get("after", s), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, L"", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, L"", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, L"", normal, pos.get("after", s), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), header, pos.get("status", s), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), header, pos.get("after", s), 10));

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasPrelResult);
      li.setFilter(EFilterExcludeCANCEL);
      li.supportFrom = true;
      li.supportTo = true;
      li.calcTotalResults = true;
      break;
    }
    case EIndPriceList:

      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Avgjorda placeringar - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCurrentTime, lang.tl(L"Genererad: ") + L"%s", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, L"", normalText));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, L"", normalText));
      pos.add("status", 80);
      pos.add("info", 80);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("club"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Avgörs kl"), boldText, pos.get("info"), 10));

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, pos.get("club"), 0));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 0));
      li.addListPost(oPrintPost(lRunnerTimePlaceFixed, L"", normalText, pos.get("info"), 0));

      li.calcResults = true;
      li.listType = li.EBaseTypeRunner;
      li.sortOrder = ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdTeamResultList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultatsammanställning - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", normalText, 280, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 280, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 340, 5, make_pair(-1, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;

    case EStdTeamResultListAll:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 280, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 400, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 460, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 400, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 460, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, L"%s.", normalText, 25, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerName, L"", normalText, 45, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 280, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, L"", normalText, 400, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, L"", normalText, 460, 0, make_pair(0, true)));

      if (li.lp.splitAnalysis)  {
        li.addSubListPost(oPrintPost(lRunnerLostTime, L"", normalText, 510, 0));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 510, 14));
      }

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;

    case unused_EStdTeamResultListLeg: {
      wchar_t title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        swprintf_s(title, (lang.tl(L"Resultat efter sträcka X#" + li.lp.getLegName())+L" - %%s").c_str());
      else
        swprintf_s(title, (lang.tl(L"Resultat efter sträckan")+L" - %%s").c_str());

      pos.add("place", 25);
      pos.add("team", li.getMaxCharWidth(this, par.selection, lTeamName, L"", normalText));
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, L"", normalText));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addHead(oPrintPost(lCmpName, makeDash(title), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, pos.get("place"), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("name"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldText, pos.get("teamstatus"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, pos.get("after"), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, pos.get("place"), 2, ln));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, pos.get("team"), 2, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, pos.get("name"), 2, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, pos.get("teamstatus"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, pos.get("after"), 2, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, pos.get("missed"), 2, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, pos.get("missed"), 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    }
    case unused_EStdTeamResultListLegLARGE: {
      wchar_t title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        swprintf_s(title, (L"%%s - "+lang.tl(L"sträcka X#" + li.lp.getLegName())).c_str());
      else
        swprintf_s(title, (L"%%s - "+lang.tl(L"slutsträckan")).c_str());

      pos.add("place", 25);
      pos.add("team", min(120, li.getMaxCharWidth(this, par.selection, lTeamName, L"", normalText)));
      pos.add("name", min(120, li.getMaxCharWidth(this, par.selection, lRunnerName, L"", normalText)));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, makeDash(title), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("name", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, pos.get("team", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", fontLarge, pos.get("name", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("teamstatus", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), 5, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), 5, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 14));
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
        mList.setListName(L"Startlista, stafett");
        mList.addToHead(lCmpName).setText(L"Startlista - X");;
        mList.newHead();
        mList.addToHead(lCmpDate);

        mList.addToSubHead(lClassName);
        mList.addToSubHead(MetaListPost(lClassStartTime, lNone));
        mList.addToSubHead(MetaListPost(lClassLength, lNone)).setText(L"X meter");
        mList.addToSubHead(MetaListPost(lClassStartName, lNone));

        mList.addToList(lTeamBib);
        mList.addToList(lTeamStartCond);
        mList.addToList(lTeamName);

        mList.addToSubList(lRunnerLegNumberAlpha).align(lTeamName, false).setText(L"X.");
        mList.addToSubList(lRunnerName);
        mList.addToSubList(lRunnerCard).align(lClassStartName);

        mList.interpret(this, gdibase, par, li);
      }
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      //li.setFilter(EFilterExcludeDNS);
      li.setSubFilter(ESubFilterVacant);
      li.sortOrder = ClassStartTime;
      break;

    case ETeamCourseList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      li.listType=li.EBaseTypeTeam;

      bib=0;
      li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0+bib, 4));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 50+bib, 4));
      li.addListPost(oPrintPost(lTeamClub, L"", normalText, 300+bib, 4));

      li.listSubType=li.EBaseTypeRunner;
      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, L"%s.", normalText, 25+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerName, L"", normalText, 50+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerCard, L"", normalText, 300+bib, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerCourse, L"", normalText, 400+bib, 0, make_pair(0, true)));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;

    case EIndCourseList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      li.listType=li.EBaseTypeRunner;

      bib=0;
      li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0+bib, 0));
      oPrintPost &rn = li.addListPost(oPrintPost(lRunnerName, L"", normalText, 50+bib, 0));
      rn.doMergeNext = true;
      li.addListPost(oPrintPost(lRunnerClub, L" (%s)", normalText, 50+bib, 0));
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 300+bib, 0));
      li.addListPost(oPrintPost(lRunnerCourse, L"", normalText, 400+bib, 0));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;
    }
    case EStdTeamStartListLeg: {
      wchar_t title[256];
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      swprintf_s(title, lang.tl(L"Startlista %%s - sträcka X#" + li.lp.getLegName()).c_str());

      li.addHead(oPrintPost(lCmpName, makeDash(title), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      ln=li.lp.getLegInfo(sampleClass);
      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, L"", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, L"", normalText, 520+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassStartName, L"", normalText, 300+bib, 10));

      li.sortOrder=ClassStartTime;
      //li.setFilter(EFilterExcludeDNS);
      break;
    }
    case EStdIndMultiStartListLeg:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      //sprintf_s(title, lang.tl("Startlista lopp %d - %%s").c_str(), li.lp.legNumber+1);
      ln=li.lp.getLegInfo(sampleClass);

      ttl = makeDash(lang.tl(L"Startlista lopp X - Y#" + li.lp.getLegName() + L"#%s"));
      li.addHead(oPrintPost(lCmpName, ttl, boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, L"", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamClub, L"", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, L"", normalText, 500+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassLength, lang.tl(L"%s meter"), boldText, 300+bib, 10, ln));
      li.addSubHead(oPrintPost(lClassStartName, L"", boldText, 500+bib, 10, ln));

      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;

    case EStdIndMultiResultListLeg:
      ln=li.lp.getLegInfo(sampleClass);

      if (li.lp.getLegNumberCoded() != 1000)
        ttl = lang.tl(L"Resultat lopp X - Y#" + li.lp.getLegName() + L"#%s");
      else
        ttl = lang.tl(L"Resultat - X#%s");

      li.addHead(oPrintPost(lCmpName, makeDash(ttl), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 260, 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 460, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldText, 510, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 560, 14));

      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 0, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 40, 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, L"", normalText, 260, 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 460, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 510, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 560, 0, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, 620, 0, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 620, 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdIndMultiResultListLegLARGE:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      ln=li.lp.getLegInfo(sampleClass);

      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerName, L"", normalText));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lRunnerClub, L"", normalText));

      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);

      ttl = L"%s - " + lang.tl(L"Lopp ") + li.lp.getLegName();
      li.addSubHead(oPrintPost(lClassName, makeDash(ttl), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("club", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 14));

      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", fontLarge, pos.get("name", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, L"", fontLarge, pos.get("club", scale), 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("teamstatus", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), 0, ln));

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdIndMultiResultListAll:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 280, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl(L"Tid"), boldText, 480, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl(L"Efter"), boldText, 540, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerClub, L"", normalText, 280, 5, make_pair(-1, true)));

      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 480, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 540, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lSubSubCounter, lang.tl(L"Lopp %s"), normalText, 25, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 90, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, L"", normalText, 150, 0, make_pair(0, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, L"", normalText, 210, 0, make_pair(0, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;
    case EStdPatrolStartList:
    {
      MetaList mList;
      mList.setListName(L"Startlista, patrull");

      mList.addToHead(lCmpName).setText(L"Startlista - X").align(false);
      mList.newHead();
      mList.addToHead(lCmpDate).align(false);

      mList.addToSubHead(lClassName);
      mList.addToSubHead(lClassStartTime);
      mList.addToSubHead(lClassLength).setText(L"X meter");
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
      mList.interpret(this, gdibase, par, li);
      break;
    }
    case EStdPatrolResultList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultatlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 70, vspace));
      //li.addListPost(oPrintPost(lTeamClub, "", normalText, 250, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 400, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 460, vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 70, lh+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 250, lh+vspace, make_pair(1, true)));
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 400, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 460, 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=160;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, 520, vspace, make_pair(1, true)));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 520, 10));
      }

      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdPatrolResultListLARGE:
      pos.add("place", 25);
      pos.add("team", int(0.7*max(li.getMaxCharWidth(this, par.selection, lTeamName, L"", normalText),
                        li.getMaxCharWidth(this, par.selection, lPatrolNameNames, L"", normalText))));

      pos.add("status", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, pos.get("team", scale), vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("status", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lPatrolNameNames, L"", fontLarge, pos.get("team", scale), 25+vspace, make_pair(0, true)));
      //li.addListPost(oPrintPost(lTeamRunner, "", fontLarge, pos.get("status", scale), 25+vspace, 1));

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=200;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), vspace, make_pair(0, true)));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 10));
      }

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      break;

    case unused_EStdRaidResultListLARGE:
      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, 0, vspace));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, 40, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, 490, vspace));

      li.addListPost(oPrintPost(lTeamRunner, L"", fontLarge, 40, 25+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, L"", fontLarge, 300, 25+vspace, make_pair(1, true)));

      li.addSubListPost(oPrintPost(lPunchNamedTime, L"", fontMedium, 0, 2, make_pair(1, true)));
      li.subListPost.back().fixedWidth=200;
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, 0, 10));

      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeCoursePunches;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case EStdResultListLARGE:
      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lPatrolNameNames, L"", normalText, 0, true));
      pos.add("club", li.getMaxCharWidth(this, par.selection, lPatrolClubNameNames, L"", normalText, 0, true));
      pos.add("status", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lRunnerPlace, L"", fontLarge, pos.get("place", scale), vspace));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", fontLarge, pos.get("name", scale), vspace));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", fontLarge, pos.get("club", scale), vspace));

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, pos.get("place", scale), 10));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("club", scale), 10));
        li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), vspace));
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normalText, pos.get("status", scale), vspace));
      }
      if (li.lp.splitAnalysis) {
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 10));
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), vspace));
      }

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 160;
        li.listSubType = li.EBaseTypeCoursePunches;
      }
      else if (li.lp.showSplitTimes) {
        li.addSubListPost(oPrintPost(lPunchTime, L"", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 95;
        li.listSubType = li.EBaseTypeCoursePunches;
      }

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;

      li.supportFrom = true;
      li.supportTo = true;
      break;

   case EStdUM_Master:
      li.addListPost(oPrintPost(lRunnerPlace, L"", fontMedium, 0, 0));
      li.addListPost(oPrintPost(lRunnerName, L"", fontMedium, 40, 0));
      li.addListPost(oPrintPost(lRunnerClub, L"", fontMedium, 250, 0));
      li.addListPost(oPrintPost(lClassName, L"", fontMedium, 490, 0));
      li.addListPost(oPrintPost(lRunnerUMMasterPoint, L"", fontMedium, 580, 0));

      li.setFilter(EFilterHasResult);

      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case ERogainingInd:
      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(this, par.selection, lRunnerCompleteName, L"", normalText, 0, true));
      pos.add("points", 50);
      pos.add("status", 50);
      li.addHead(oPrintPost(lCmpName, makeDash(par.getCustomTitle(lang.tl(L"Rogainingresultat - %s"))), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), vspace));
      li.addListPost(oPrintPost(lRunnerCompleteName, L"", normalText, pos.get("name"), vspace));
      li.addListPost(oPrintPost(lRunnerRogainingPoint, L"%sp", normalText, pos.get("points"), vspace));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), vspace));

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      if (li.lp.splitAnalysis || li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lRogainingPunch, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=130;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      li.addSubHead(oPrintPost(lClassName, L"", boldText, pos.get("place"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Poäng"), boldText, pos.get("points"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder = ClassPoints;
      li.lp.setLegNumberCoded(-1);
      li.calcResults = true;
      li.rogainingResults = true;
    break;

    case EStdTeamAllLegLARGE: {
      vector< pair<wstring, size_t> > out;
      fillLegNumbers(par.selection, false, false, out);
      par.listCode = oe->getListContainer().getType("legresult");
      par.useLargeSize = true;
      for (size_t k = 0; k < out.size(); k++) {
        if (out[k].second >= 1000)
          continue;
        par.setLegNumberCoded(out[k].second);
        if (k == 0)
          generateListInfo(par, li);
        else {
          li.next.push_back(oListInfo());
          generateListInfo(par, li.next.back());
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
      if (!getListContainer().interpret(this, gdibase, par, li))
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

wstring oPrintPost::encodeFont(const wstring &face, int factor) {
  wstring out(face);
  if (factor > 0 && factor != 100) {
    out += L";" + itow(factor/100) + L"." + itow(factor%100);
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

wstring oListParam::getLegName() const {
  if (legNumber == -1)
    return L"";

  if (legNumber < 1000)
    return itow(legNumber + 1);

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  wchar_t bf[64];
  char symb = 'a' + sub;
  swprintf_s(bf, L"%d%c", maj, symb);
  return bf;
}
