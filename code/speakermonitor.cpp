/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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
#include "oEvent.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <cassert>
#include "Localizer.h"
#include <algorithm>
#include "gdifonts.h"
#include "meosexception.h"
#include "speakermonitor.h"
#include "oListInfo.h"

extern gdioutput *gdi_main;

SpeakerMonitor::SpeakerMonitor(oEvent &oe) : oe(oe) {
  placeLimit = 0;
  numLimit = 0;

  maxClassNameWidth = 0;
  totalResults = false;
  classWidth = 0;
  timeWidth = 0;
  totWidth = 0;
  extraWidth = 0;
}

SpeakerMonitor::~SpeakerMonitor() {
}

void SpeakerMonitor::setClassFilter(const set<int> &filter, const set<int> &cfilter) {
  classFilter = filter;
  controlIdFilter = cfilter;
  oListInfo li;
  maxClassNameWidth = li.getMaxCharWidth(&oe, classFilter, lClassName, L"", normalText, false);
}

void SpeakerMonitor::setLimits(int place, int num) {
  placeLimit = place;
  numLimit = num;
}

bool orderResultsInTime(const oEvent::ResultEvent &a,
                        const oEvent::ResultEvent &b) {
  if (a.time != b.time)
    return a.time > b.time;
 
  if (a.time > 0) {
    const wstring &na = a.r->getName();
    const wstring &nb = b.r->getName();
  
    return CompareString(LOCALE_USER_DEFAULT, 0,
                          na.c_str(), na.length(),
                          nb.c_str(), nb.length()) == CSTR_LESS_THAN;
  }
  return a.r->getId() < b.r->getId();
}

void SpeakerMonitor::show(gdioutput &gdi) {
  classWidth = gdi.scaleLength(int(1.5*maxClassNameWidth));
  timeWidth = gdi.scaleLength(60);
  totWidth = gdi.scaleLength(600);
  extraWidth = gdi.scaleLength(200);
  dash = makeDash(L"- ");

  oe.getResultEvents(classFilter, controlIdFilter, results);
  calculateResults();

  orderedResults.resize(results.size());
  for (size_t k = 0; k < results.size(); k++) {
    // Initialize forward map
    results[k].localIndex = k;
    orderedResults[k].r = results[k].r;
    orderedResults[k].totalTime = results[k].runTime;
  }

  // Sort
  sort(results.begin(), results.end(), orderResultsInTime);
  
  // Initialize map back
  for (size_t k = 0; k < results.size(); k++) {
    orderedResults[results[k].localIndex].eventIx = k;
  }

  for (size_t k = 0; k < results.size(); k++) {
    oEvent::ResultEvent &re(results[results.size()-(1+k)]);
    if (re.status == StatusOK) {
      map<int,int> &dynLead = dynamicTotalLeaderTimes[ResultKey(re.classId(), re.control, re.leg())];
      int totTime = re.runTime;
      if (dynLead.empty() || totTime < dynLead.rbegin()->second)
        dynLead[re.time] = totTime;
    }
  }
  
  int order = 0;
  for (size_t k = 0; k < results.size() && (order < numLimit || numLimit == 0); k++) {
    if (results[k].time > 0) {
      oEvent::ResultEvent *ahead = 0, *behind = 0;

      if (results[k].status == StatusOK)
        ahead = k > 0 && sameResultPoint(results[k], results[k-1]) ? &results[k-1] : 0;
      if (results[k].status == StatusOK)
        behind = (k + 1) < results.size() && sameResultPoint(results[k], results[k+1]) ? &results[k+1] : 0;

      renderResult(gdi, ahead, results[k], behind, order, true);
    }
  }

  for (size_t k = 0; k < results.size() && (order < numLimit || numLimit == 0); k++) {
    if (results[k].time == 0)
      renderResult(gdi, 0, results[k], 0, order, false);
    else
      break;
  }
}

bool SpeakerMonitor::sameResultPoint(const oEvent::ResultEvent &a,
                                     const oEvent::ResultEvent &b) {
  return a.control == b.control && a.classId() == b.classId() && a.leg() == b.leg();
}

void SpeakerMonitor::renderResult(gdioutput &gdi, 
                                  const oEvent::ResultEvent *ahead,
                                  const oEvent::ResultEvent &res, 
                                  const oEvent::ResultEvent *behind,
                                  int &order,
                                  bool firstResults) {
  

  /*string s = itos(res.localIndex) + ": " + res.r->getName() + " control: " + 
                   itos(res.control) + " tid: " + formatTime(res.runTime);
  gdi.addStringUT(0, s);
  */
  int finishLargePlaceLimit = 5;
  int radioLargePlaceLimit = 3;
  const bool showClass = classFilter.size() > 1;

  if (res.status == StatusOK) {
    if (res.place > placeLimit && res.r->getSpeakerPriority() == 0 ) {
      return;
    }
  }
  else if (order < max(2, numLimit/20)) {
    return;
  }
  else if (res.status == StatusNotCompetiting) {
    return; // DQ on ealier leg, for example
  }

  order++;
  const int extra = 5;

  int t = res.time;
  wstring msg = t>0 ? oe.getAbsTime(t) : makeDash(L"--:--:--");
  pRunner r = res.r;

  RECT rc;
  rc.top = gdi.getCY();
  rc.left = gdi.getCX();

  int xp = rc.left + extra;
  int yp = rc.top + extra;

  bool largeFormat = res.status == StatusOK &&
    ((res.control == oPunch::PunchFinish && res.place <= finishLargePlaceLimit) || 
                                            res.place <= radioLargePlaceLimit);
  wstring message;
  deque<wstring> details;
  getMessage(res,message, details);

  if (largeFormat) {
    gdi.addStringUT(yp, xp, 0, msg);

    int dx = 0;
    if (showClass) {
      pClass pc = res.r->getClassRef(true);
      if (pc) {
        gdi.addStringUT(yp, xp + timeWidth, fontMediumPlus, pc->getName());
        dx = classWidth;
      }
    }

    wstring bib = r->getBib();
    if (!bib.empty())
      msg = bib + L", ";
    else
      msg = L"";
    
    msg += r->getCompleteIdentification(false);
    int xlimit = totWidth + extraWidth - (timeWidth + dx);
    gdi.addStringUT(yp, xp + timeWidth + dx, fontMediumPlus, msg, xlimit);
    
    yp += int(gdi.getLineHeight() * 1.5);

    msg = dash + lang.tl(message);
    gdi.addStringUT(yp, xp + 10, breakLines, msg, totWidth);

    for (size_t k = 0; k < details.size(); k++) {
      gdi.addStringUT(gdi.getCY(), xp + 20, 0, dash + lang.tl(details[k])).setColor(colorDarkGrey);
    }

    if (res.control == oPunch::PunchFinish && r->getStatus() == StatusOK) {
      splitAnalysis(gdi, xp + 20, gdi.getCY(), r);
    }

    GDICOLOR color = colorLightRed;

    if (res.control == oPunch::PunchFinish) {
      if (r && r->statusOK(true))
        color = colorLightGreen;
      else
        color = colorLightRed;
    }
    else {
      color = colorLightCyan;
    }
       
    rc.bottom = gdi.getCY() + extra;
    rc.right = rc.left + totWidth + extraWidth + 2 * extra;
    gdi.addRectangle(rc, color, true);
    gdi.dropLine(0.5);
  }
  else {
    if (showClass) {
      pClass pc = res.r->getClassRef(true);
      if (pc)
        msg += L" (" + pc->getName() + L") ";
      else
        msg += L" ";
    }
    else
      msg += L" ";

    msg += r->getCompleteIdentification(false) + L" ";
    
    msg += lang.tl(message);
    gdi.addStringUT(gdi.getCY(), gdi.getCX(), breakLines, msg, totWidth);
    
    for (size_t k = 0; k < details.size(); k++) {
      gdi.addString("", gdi.getCY(), gdi.getCX()+50, 0, details[k]).setColor(colorDarkGrey);
    }

    if (res.control == oPunch::PunchFinish && r->getStatus() == StatusOK) {
      splitAnalysis(gdi, xp, gdi.getCY(), r);
    }
    gdi.dropLine(0.5);
  }


}

bool compareResult(const oEvent::ResultEvent &a,
                   const oEvent::ResultEvent &b) {

  if (a.classId() != b.classId())
    return a.classId() < b.classId();
  if (a.control != b.control)
    return a.control < b.control;
  int lega = a.leg();
  int legb = b.leg();
  if (lega != legb)
    return lega<legb;
  if (a.resultScore != b.resultScore)
    return a.resultScore < b.resultScore;

  return a.r->getId() < b.r->getId();
}

void SpeakerMonitor::calculateResults() {
  // TODO Result modules

  for (size_t k = 0; k < results.size(); k++) {
    results[k].runTime = results[k].r->getTotalRunningTime(results[k].time, true, totalResults);
    if (results[k].status == StatusOK && totalResults)
      results[k].status = results[k].r->getTotalStatus();
    
    if (results[k].status == StatusOK)
      results[k].resultScore = results[k].runTime;
    else
      results[k].resultScore = RunnerStatusOrderMap[results[k].status] + 3600*24*7;
  }

  totalLeaderTimes.clear();
  firstTimes.clear();
  dynamicTotalLeaderTimes.clear();
  runnerToTimeKey.clear();
  timeToResultIx.clear();
  sort(results.begin(), results.end(), compareResult);

  int clsId = -1;
  int ctrlId = -1;
  int legId = -1;
  int lastScore = 0;
  int place = 0;

  for (size_t k = 0; k < results.size(); k++) {
    if (results[k].partialCount > 0)
      continue; // Skip in result calculation
    int totTime = results[k].r->getTotalRunningTime(results[k].time, true, totalResults);
    assert(totTime == results[k].runTime);
    int leg = results[k].leg();
    
    if (clsId != results[k].classId() || ctrlId != results[k].control || legId != leg) {
      clsId = results[k].classId();
      ctrlId = results[k].control;
      legId = leg;
      place = 1;
      ResultKey key(clsId, ctrlId, leg);
    
      totalLeaderTimes[key] = totTime;
    }
    else if (lastScore != results[k].resultScore)
      place++;

    ResultKey key(clsId, ctrlId, leg);
    lastScore = results[k].resultScore;

    if (results[k].status == StatusOK) {
      results[k].place = place;
      runnerToTimeKey[results[k].r->getId()].push_back(ResultInfo(key, results[k].time, totTime));

      if (results[k].time > 0) {
        int val = firstTimes[key];
        firstTimes[key] = (val > 0 && val < results[k].time) ? val : results[k].time;
      }
    }
    else
      results[k].place = 0;
  }

  // Sort times for each runner
  for (map<int, vector<ResultInfo> >::iterator it = runnerToTimeKey.begin();
        it != runnerToTimeKey.end(); ++it) {
    sort(it->second.begin(), it->second.end(), timeSort);
  }
}

void SpeakerMonitor::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {

}

void SpeakerMonitor::splitAnalysis(gdioutput &gdi, int xp, int yp, pRunner r) {
  vector<int> delta;
  r->getSplitAnalysis(delta);
  wstring timeloss = lang.tl("Bommade kontroller: ");
  pCourse pc = 0;
  bool first = true;
  const int charlimit = 90;
  for (size_t j = 0; j<delta.size(); ++j) {
    if (delta[j] > 0) {
      if (pc == 0) {
        pc = r->getCourse(true);
        if (pc == 0)
          break;
      }

      if (!first)
        timeloss += L" | ";
      else
        first = false;

      timeloss += pc->getControlOrdinal(j) + L". " + formatTime(delta[j]);
    }
    if (timeloss.length() > charlimit || (!timeloss.empty() && !first && j+1 == delta.size())) {
      gdi.addStringUT(yp, xp, 0, timeloss).setColor(colorDarkRed);
      yp += gdi.getLineHeight();
      timeloss = L"";
    }
  }
  if (first) {
    gdi.addString("", yp, xp, 0, "Inga bommar registrerade").setColor(colorDarkGreen);
  }
}

wstring getOrder(int k);
wstring getNumber(int k);
void getTimeAfterDetail(wstring &detail, int timeAfter, int deltaTime, bool wasAfter);

wstring getTimeDesc(int t1, int t2) {
  int tb = abs(t1 - t2);
  wstring stime;
  if (tb == 1)
    stime = L"1" + lang.tl(L" sekund");
  else if (tb <= 60)
    stime = itow(tb) + lang.tl(L" sekunder");
  else
    stime = formatTime(tb);

  return stime;
}

void SpeakerMonitor::getMessage(const oEvent::ResultEvent &res, 
                                wstring &message, deque<wstring> &details) {
  pClass cls = res.r->getClassRef(true);
  if (!cls)
    return;

  wstring &msg = message;

  int totTime = res.runTime;
  int leaderTime = getLeaderTime(res);
  int timeAfter = totTime - leaderTime;

  ResultKey key(res.classId(), res.control, res.leg());

  vector<ResultInfo> &rResults = runnerToTimeKey[res.r->getId()];
  ResultInfo *preRes = 0;
  pRunner preRunner = 0;
 
  for (size_t k = 0; k < rResults.size(); k++) {
    if (rResults[k] == key) {
      if (k > 0) {
        preRes = &rResults[k-1];
        preRunner = res.r;
      }
      else {
        int leg = res.r->getLegNumber();
        if (leg > 0 && res.r->getTeam()) {
          while (leg > 0) {
            pRunner pr = res.r->getTeam()->getRunner(--leg);
            // TODO: Pursuit
            if (pr && pr->prelStatusOK(false, false) && pr->getFinishTime() == res.r->getStartTime()) {
              vector<ResultInfo> &rResultsP = runnerToTimeKey[pr->getId()];
              if (!rResultsP.empty()) {
                preRes = &rResultsP.back();
                preRunner = pr;
              }
              break;
            }
          }
        }
      }

    }
  }

  if (res.status != StatusOK) {
    if (res.status != StatusDQ)
      msg = L"är inte godkänd.";
    else
      msg = L"är diskvalificerad.";

    return;
  }

  bool hasPrevRes = false;
  int deltaTime = 0;

  const oEvent::ResultEvent *ahead = getAdjacentResult(res, -1);
  const oEvent::ResultEvent *behind = getAdjacentResult(res, +1);

  if (preRes != 0) {
    int prevLeader = getDynamicLeaderTime(*preRes, preRes->time);
    int prevAfter = preRes->totTime - prevLeader; // May be negative (for leader)
    int after = totTime - getDynamicLeaderTime(res, res.time);

    hasPrevRes = totTime > 0 && preRes->totTime > 0;

    if (after == 0) { // Takes (and holds) the lead in a relay (for example)
      if (!behind) {
        hasPrevRes = false;
      }
      else {
        after = totTime - behind->runTime; // A negative number. Time ahead.
      }
    }

    if (prevAfter == 0) { // Took (and holds) the lead
      int rix = getResultIx(*preRes);
      const oEvent::ResultEvent *prevBehind = 0;
      if (rix != -1)
        prevBehind = getAdjacentResult(results[rix], +1);
      
      if (!prevBehind) {
        hasPrevRes = false;
      }
      else {
        prevAfter = preRes->totTime - prevBehind->runTime; // A negative number. Time ahead
      }

    }

    deltaTime = after - prevAfter; // Positive -> more after.
  }

  wstring timeS = formatTime(res.runTime);

  wstring detail;
  const wstring *cname = 0;
  if (timeAfter > 0 && ahead)
    cname = &ahead->r->getName();
  else if (timeAfter < 0 && behind)
    cname = &behind->r->getName();

  int ta = timeAfter;
  if (res.place == 1 && behind && behind->status == StatusOK)
    ta = res.runTime - behind->runTime;

  getTimeAfterDetail(detail, ta, deltaTime, hasPrevRes);
  if (!detail.empty())
    details.push_back(detail);

  pCourse crs = res.r->getCourse(false);

  wstring location, locverb, thelocation;
  
  bool finish = res.control == oPunch::PunchFinish;
  bool changeover = false;
  int leg = res.r->getLegNumber();
  int numstage = cls->getNumStages();
  if (finish && res.r->getTeam() != 0 && (leg + 1) < numstage) {
    for (int k = leg + 1; k < numstage; k++)
      if (!cls->isParallel(k) && !cls->isOptional(k)) {
        finish = false;
        changeover = true;
        break;
      }
  }

  if (finish) {
    location = L"i mål";
    locverb = L"går i mål";
    thelocation = lang.tl(L"målet") + L"#";
  }
  else if (changeover) {
    location = L"vid växeln";
    locverb = L"växlar";
    thelocation = lang.tl(L"växeln") + L"#";
  }
  else {
    thelocation = crs ? (crs->getRadioName(res.control) + L"#") : L"#";
  }

  
  bool sharedPlace = (ahead && ahead->place == res.place) || (behind && behind->place == res.place);

  if (finish || changeover) {
    if (res.place == 1) {
      if ((ahead == 0 && behind == 0) || firstTimes[key] == res.time)
        msg = L"är först " + location + L" med tiden X.#" + timeS;
      else if (!sharedPlace) {
        msg = L"tar ledningen med tiden X.#" + timeS;
        if (behind  && res.place>2) {
          wstring stime(getTimeDesc(behind->runTime, totTime));
          details.push_back(L"är X före Y#" + stime + L"#" + behind->r->getCompleteIdentification(false));
        }
      }
      else
        msg = L"går upp i delad ledning med tiden X.#" + timeS;
    }
    else {
      if (firstTimes[key] == res.time) {
        msg = L"var först " + location + L" med tiden X.#" + timeS;

        if (!sharedPlace) {
          details.push_front(L"är nu på X plats med tiden Y.#" +
                      getOrder(res.place) + L"#" + timeS);
          if (ahead && res.place != 2) {
            wstring stime(getTimeDesc(ahead->runTime, totTime));
            details.push_back(L"är X efter Y#" + stime + L"#" + ahead->r->getCompleteIdentification(false));
          }
        }
        else {
          details.push_back(L"är nu på delad X plats med tiden Y.#" + getOrder(res.place) +
                                                        L"#" + timeS);
          wstring share;
          getSharedResult(res, share);
          details.push_back(share);
        }
      }
      else if (!sharedPlace) {
        msg = locverb + L" på X plats med tiden Y.#" +
                  getOrder(res.place) + L"#" + timeS;
        if (ahead && res.place != 2) {
          wstring stime(getTimeDesc(ahead->runTime, totTime));
          details.push_back(L"är X efter Y#" + stime + L"#" + ahead->r->getCompleteIdentification(false));
        }
      }
      else {
        msg = locverb + L" på delad X plats med tiden Y.#" + getOrder(res.place) +
                                                      L"#" + timeS;
        wstring share;
        getSharedResult(res, share);
        details.push_back(share);
      }
    }
    if (changeover && res.r->getTeam() && res.r->getTeam()->getClassRef(true) != 0) {
      wstring vxl = L"skickar ut X.#";
      pTeam t = res.r->getTeam();
      pClass cls = t->getClassRef(true);
      bool second = false;
      int nextLeg = cls->getNextBaseLeg(res.r->getLegNumber());
      if (nextLeg > 0) {
        for (int k = nextLeg; k < t->getNumRunners(); k++) {
          pRunner r = t->getRunner(k);
          if (!r && !vxl.empty())
            continue;
          if (second)
            vxl += L", ";
          second = true;
          vxl += r ? r->getName() : L"?";
          if (!(cls->isParallel(k) || cls->isOptional(k)))
            break;
        }
      }
      details.push_front(vxl);
    }
  }
  else {
    if (res.place == 1) {
      if ((ahead == 0 && behind == 0) || res.time == firstTimes[key])
        msg = L"är först vid X med tiden Y.#" + thelocation + timeS;
      else if (!sharedPlace) {
        msg = L"tar ledningen vid X med tiden Y.#" + thelocation + timeS;
        if (behind) {
          wstring stime(getTimeDesc(behind->runTime, totTime));
          details.push_back(L"är X före Y#" + stime + L"#" + behind->r->getCompleteIdentification(false));
        }
      }
      else
        msg = L"går upp i delad ledning vid X med tiden Y.#" + thelocation + timeS;
    }
    else {
      if (firstTimes[key] == res.time) {
        msg = L"var först vid X med tiden Y.#" + thelocation + timeS;

        if (!sharedPlace) {
          details.push_front(L"är nu på X plats med tiden Y.#" +
                        getOrder(res.place) + L"#" + timeS);
          if (ahead) {
            wstring stime(getTimeDesc(ahead->runTime, totTime));
            details.push_back(L"är X efter Y#" + stime + L"#" + ahead->r->getCompleteIdentification(false));
          }
        }
        else {
          details.push_back(L"är nu på delad X plats med tiden Y.#" + getOrder(res.place) +
                                                        L"#" + timeS);
          wstring share;
          getSharedResult(res, share);
          details.push_back(share);
        }
      }
      else if (!sharedPlace) {
        msg = L"stämplar vid X som Y, på tiden Z.#" +
                      thelocation + getNumber(res.place) +
                      L"#" + timeS;
        if (ahead && res.place>2) {
          wstring stime(getTimeDesc(ahead->runTime, totTime));
          details.push_back(L"är X efter Y#" + stime + L"#" + ahead->r->getCompleteIdentification(false));
        }
      }
      else {
        msg = L"stämplar vid X som delad Y med tiden Z.#" + thelocation + getNumber(res.place) +
                                                      L"#" + timeS;
        wstring share;
        getSharedResult(res, share);
        details.push_back(share);
      }
    }                                    
  }
  return;
}

void SpeakerMonitor::getSharedResult(const oEvent::ResultEvent &res, wstring &detail) const {
  vector<pRunner> shared;
  getSharedResult(res, shared);
  if (!shared.empty())
    detail = L"delar placering med X.#";
  else 
    detail = L"";

  for (size_t k = 0; k < shared.size(); k++) {
    if (k > 0)
      detail += L", ";
    
    detail += shared[k]->getCompleteIdentification(false);
  }
}

void SpeakerMonitor::getSharedResult(const oEvent::ResultEvent &res, vector<pRunner> &shared) const {
  shared.clear();
  if (res.status != StatusOK)
    return;
  int p = 1;
  const oEvent::ResultEvent * ar;;
  while ( (ar  = getAdjacentResult(res, p++)) != 0 && ar->place == res.place) {
    shared.push_back(ar->r);
  }
  p = 1;
  while ( (ar  = getAdjacentResult(res, -(p++))) != 0 && ar->place == res.place) {
    shared.push_back(ar->r);
  }
}

const oEvent::ResultEvent *SpeakerMonitor::getAdjacentResult(const oEvent::ResultEvent &res, int delta) const {
  while (true) {
    int orderIx = res.localIndex + delta;
    if (orderIx < 0 || orderIx >= int(orderedResults.size()) )
      return 0;

    int aIx = orderedResults[orderIx].eventIx;

    if (!sameResultPoint(res, results[aIx]))
      return 0;

    if (results[aIx].partialCount == 0)
      return &results[aIx];

    if (delta < 0)
      delta--;
    else
      delta++;
  }
}

int SpeakerMonitor::getLeaderTime(const oEvent::ResultEvent &res) {
  return totalLeaderTimes[ResultKey(res.classId(), res.control, res.leg())];
}

int SpeakerMonitor::getDynamicLeaderTime(const oEvent::ResultEvent &res, int time) {
  return getDynamicLeaderTime(ResultKey(res.classId(), res.control, res.leg()), time);
}

int SpeakerMonitor::getDynamicLeaderTime(const ResultKey &res, int time) {
  map<int, int> &leaderMap = dynamicTotalLeaderTimes[res];
  map<int, int>::iterator g = leaderMap.lower_bound(time);
  if (leaderMap.empty())
    return 0;

  if (g != leaderMap.end()) {
    if (g != leaderMap.begin())
      --g;
    return g->second;
  }
  else 
    return leaderMap.rbegin()->second;
}


int SpeakerMonitor::getResultIx(const ResultInfo &rinfo) {
  if (timeToResultIx.empty()) {
    for (size_t k = 0; k < results.size(); k++) {
      timeToResultIx.insert(make_pair(results[k].time, k));
    }
  }

  pair<multimap<int,int>::iterator, multimap<int,int>::iterator> range = timeToResultIx.equal_range(rinfo.time);
  while (range.first != range.second) {
    int ix = range.first->second;
    if (results[ix].classId() == rinfo.classId && 
        results[ix].leg() == rinfo.leg &&
        results[ix].control == rinfo.radio) {
      return ix;
    }
    ++range.first;
  }

  return -1;
}
