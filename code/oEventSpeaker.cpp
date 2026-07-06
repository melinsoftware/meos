/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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

#include <vector>
#include <io.h>
#include <algorithm>

#include "oEvent.h"
#include "oSpeaker.h"
#include "gdioutput.h"

#include "mmsystem.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "meosexception.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern gdioutput *gdi_main;
extern oEvent* gEvent;
constexpr int constNoLeaderTime = 10000000;
constexpr int highlightNewResultTime = 15;

oTimeLine::oTimeLine(int time_, TimeLineType type_, Priority priority_, int classId_, int ID_, oRunner *source) :
  time(time_), type(type_), priority(priority_), classId(classId_), ID(ID_)
{
  if (source) {
    typeId.second = source->getId();
    typeId.first = false; // typeid(*source) == typeid(oTeam);
  }
  else {
    typeId.first = false;
    typeId.second = 0;
  }
}

pCourse deduceSampleCourse(pClass pc, vector<oClass::TrueLegInfo > &stages, int leg) {
  int courseLeg = leg;
  for (size_t k = 0; k <stages.size(); k++) {
    if (stages[k].first == leg) {
      courseLeg = stages[k].nonOptional;
      break;
    }
  }
  if (pc->hasMultiCourse())
    return pc->getCourse(courseLeg, 0, true);
  else
    return pc->getCourse(true);
}

oRunner *oTimeLine::getSource(const oEvent &oe) const {
  //assert(typeId.second != 0);
  if (typeId.second > 0) {
    //if (typeId.first)
    //  return oe.getTeam(typeId.second);
    //else
      return oe.getRunner(typeId.second, 0);
  }
  return 0;
}

__int64 oTimeLine::getTag() const {
  __int64 cs = type != TLTExpected ? time : 0;
  cs = 997 * cs + type;
  cs = 997 * cs + priority;
  cs = 997 * cs + classId;
  cs = 997 * cs + ID;
  cs = 997 * cs + typeId.second;

  return cs;
}

//Order by preliminary times for speaker list
static bool compareSpkSPList(const oSpeakerObject& a, const oSpeakerObject& b, int ix) {
  bool startTime = false;
  if (ix == -1) {
    ix = 0;
    startTime = true;
  }

  if (a.finishStatus > 1 && b.finishStatus <= 1)
    return false;
  else if (b.finishStatus > 1 && a.finishStatus <= 1)
    return true;

  // A runner without any result is after
  if (a.compareResultIndex == -1 && b.compareResultIndex != -1)
    return false;
  if (b.compareResultIndex == -1 && a.compareResultIndex != -1)
    return true;

  if (a.result[ix].status <= 1 && b.result[ix].status <= 1) {
    int at = a.result[ix].runningTime.preliminary;
    int bt = b.result[ix].runningTime.preliminary;

    bool aHasResult = a.result[ix].runningTime.time > 0;
    bool bHasResult = b.result[ix].runningTime.time > 0;

    if (at == bt) { //Compare leg times instead
      at = a.result[ix].runningTimeLeg.preliminary;
      bt = b.result[ix].runningTimeLeg.preliminary;
    }

    if (at == bt && aHasResult == bHasResult) {
      if (a.result[ix].runningTimeSinceLast.preliminary > 0 && b.result[ix].runningTimeSinceLast.preliminary > 0) {
        if (a.result[ix].runningTimeSinceLast.preliminary != b.result[ix].runningTimeSinceLast.preliminary)
          return a.result[ix].runningTimeSinceLast.preliminary > b.result[ix].runningTimeSinceLast.preliminary;
      }
      else if (a.result[ix].runningTimeSinceLast.preliminary > 0)
        return true;
      else if (b.result[ix].runningTimeSinceLast.preliminary > 0)
        return false;
    }

    if (a.missingStartTime != b.missingStartTime)
      return a.missingStartTime < b.missingStartTime;

    if (at == bt) {
      if (aHasResult != bHasResult)
        return aHasResult > bHasResult;
      if (a.result[ix].parallelScore != b.result[ix].parallelScore)
        return a.result[ix].parallelScore > b.result[ix].parallelScore;

      if (a.bib != b.bib) {
        return compareBib(a.bib, b.bib);
      }
      return a.names < b.names;
    }
    else if (at >= 0 && bt >= 0) {
      if (startTime) //(a.priority == 0)
        return bt < at;
      else
        return at < bt;
    }
    else if (at >= 0 && bt < 0)
      return true;
    else if (at < 0 && bt >= 0)
      return false;
    else return at > bt;
  }
  else if (a.result[ix].status != b.result[ix].status)
    return a.result[ix].status < b.result[ix].status;

  if (a.result[ix].parallelScore != b.result[ix].parallelScore)
    return a.result[ix].parallelScore > b.result[ix].parallelScore;

  if (a.bib != b.bib) {
    return compareBib(a.bib, b.bib);
  }

  return a.names < b.names;
}

static bool compareSpkSPListPart(const oSpeakerObject& a, const oSpeakerObject& b, int ix) {
  if (a.finishStatus > 1 && b.finishStatus <= 1)
    return false;
  else if (b.finishStatus > 1 && a.finishStatus <= 1)
    return true;

  if (ix == -1) {
    if (a.compareResultIndex == -1 && b.compareResultIndex == -1) {
      // No result comparison
      return compareSpkSPList(a, b, compareSpkSPList(a, b, 0));
    }
    return false; // No comparison
  }

  if (a.compareResultIndex == b.compareResultIndex) {
    if (ix == a.compareResultIndex)
      return compareSpkSPList(a, b, ix);
    else
      return false; // No comparison
  }

  if (a.compareResultIndex != ix && b.compareResultIndex != ix)
    return false; // No comparison

  if (a.compareResultIndex == -1)
    return false;
  else if (b.compareResultIndex == -1)
    return true;

  auto getCompareTimes = [&a, &b](int ix) {
    int at = a.result[ix].runningTime.preliminary;
    int bt = b.result[ix].runningTime.preliminary;

    if (at == bt) { //Compare leg times instead
      at = a.result[ix].runningTimeLeg.preliminary;
      bt = b.result[ix].runningTimeLeg.preliminary;
    }
    return make_pair(at, bt);
  };

  if (a.compareResultIndex < b.compareResultIndex) {
    auto [at, bt] = getCompareTimes(a.compareResultIndex);
    if (bt < at)
      return false;
    else {
      if (a.nextPreliminaryTime != -1) {
        auto [at2, bt2] = getCompareTimes(a.nextPreliminaryTime);
        if (at2 != bt2)
          return at2 < bt2;
      }
      else {
        if (at != bt)
          return at < bt;
      }
    }
  }
  else {
    auto [at, bt] = getCompareTimes(b.compareResultIndex);
    if (bt < at)
      return false;
    else {
      if (b.nextPreliminaryTime != -1) {
        auto [at2, bt2] = getCompareTimes(b.nextPreliminaryTime);
        if (at2 != bt2)
          return at2 < bt2;
      }
      else {
        if (at != bt)
          return at < bt;
      }
    }
  }

  /*
  if (a.compareResultIndex < b.compareResultIndex) {
    if (a.compareResultIndex == -1)
      return false;

    if ()
  }
  else  if (b.compareResultIndex == -1)
    return true;

  if (a.com)
    if (a.priority != b.priority)
      return a.priority > b.priority;
    else if (a.result[ix].status <= 1 && b.result[ix].status <= 1) {
      int at = a.result[ix].runningTime.preliminary;
      int bt = b.result[ix].runningTime.preliminary;

      bool aHasResult = a.result[ix].runningTime.time > 0;
      bool bHasResult = b.result[ix].runningTime.time > 0;

      if (at == bt) { //Compare leg times instead
        at = a.result[ix].runningTimeLeg.preliminary;
        bt = b.result[ix].runningTimeLeg.preliminary;
      }

      if (at == bt && aHasResult == bHasResult) {
        if (a.result[ix].runningTimeSinceLast.preliminary > 0 && b.result[ix].runningTimeSinceLast.preliminary > 0) {
          if (a.result[ix].runningTimeSinceLast.preliminary != b.result[ix].runningTimeSinceLast.preliminary)
            return a.result[ix].runningTimeSinceLast.preliminary > b.result[ix].runningTimeSinceLast.preliminary;
        }
        else if (a.result[ix].runningTimeSinceLast.preliminary > 0)
          return true;
        else if (b.result[ix].runningTimeSinceLast.preliminary > 0)
          return false;
      }

      if (a.missingStartTime != b.missingStartTime)
        return a.missingStartTime < b.missingStartTime;

      if (at == bt) {
        if (aHasResult != bHasResult)
          return aHasResult > bHasResult;
        if (a.result[ix].parallelScore != b.result[ix].parallelScore)
          return a.result[ix].parallelScore > b.result[ix].parallelScore;

        if (a.bib != b.bib) {
          return compareBib(a.bib, b.bib);
        }
        return a.names < b.names;
      }
      else if (at >= 0 && bt >= 0) {
        if (a.priority == 0)
          return bt < at;
        else
          return at < bt;
      }
      else if (at >= 0 && bt < 0)
        return true;
      else if (at < 0 && bt >= 0)
        return false;
      else return at > bt;
    }
    else if (a.result[ix].status != b.result[ix].status)
      return a.result[ix].status < b.result[ix].status;
  */
  if (a.result[ix].parallelScore != b.result[ix].parallelScore)
    return a.result[ix].parallelScore > b.result[ix].parallelScore;

  if (a.bib != b.bib) {
    return compareBib(a.bib, b.bib);
  }

  return a.names < b.names;
}


/*
static bool compareSpkSPListAll(const oSpeakerObject& a, const oSpeakerObject& b) {
  if (a.missingStartTime != b.missingStartTime)
    return a.missingStartTime < b.missingStartTime;

  if (a.compareResultIndex == b.compareResultIndex)
    return compareSpkSPList(a, b, compareSpkSPList(a, b, a.compareResultIndex));

  auto getCompareTimes = [&a, &b](int ix) {
    int at = a.result[ix].runningTime.preliminary;
    int bt = b.result[ix].runningTime.preliminary;

    if (at == bt) { //Compare leg times instead
      at = a.result[ix].runningTimeLeg.preliminary;
      bt = b.result[ix].runningTimeLeg.preliminary;
    }
    return make_pair(at, bt);
  };

  if (a.compareResultIndex < b.compareResultIndex) {
    if (a.compareResultIndex == -1)
      return false;

    if ()
  }
  else  if (b.compareResultIndex == -1)
    return true;

  if (a.com)
  if (a.priority != b.priority)
    return a.priority > b.priority;
  else if (a.result[ix].status <= 1 && b.result[ix].status <= 1) {
    int at = a.result[ix].runningTime.preliminary;
    int bt = b.result[ix].runningTime.preliminary;

    bool aHasResult = a.result[ix].runningTime.time > 0;
    bool bHasResult = b.result[ix].runningTime.time > 0;

    if (at == bt) { //Compare leg times instead
      at = a.result[ix].runningTimeLeg.preliminary;
      bt = b.result[ix].runningTimeLeg.preliminary;
    }

    if (at == bt && aHasResult == bHasResult) {
      if (a.result[ix].runningTimeSinceLast.preliminary > 0 && b.result[ix].runningTimeSinceLast.preliminary > 0) {
        if (a.result[ix].runningTimeSinceLast.preliminary != b.result[ix].runningTimeSinceLast.preliminary)
          return a.result[ix].runningTimeSinceLast.preliminary > b.result[ix].runningTimeSinceLast.preliminary;
      }
      else if (a.result[ix].runningTimeSinceLast.preliminary > 0)
        return true;
      else if (b.result[ix].runningTimeSinceLast.preliminary > 0)
        return false;
    }

    if (a.missingStartTime != b.missingStartTime)
      return a.missingStartTime < b.missingStartTime;

    if (at == bt) {
      if (aHasResult != bHasResult)
        return aHasResult > bHasResult;
      if (a.result[ix].parallelScore != b.result[ix].parallelScore)
        return a.result[ix].parallelScore > b.result[ix].parallelScore;

      if (a.bib != b.bib) {
        return compareBib(a.bib, b.bib);
      }
      return a.names < b.names;
    }
    else if (at >= 0 && bt >= 0) {
      if (a.priority == 0)
        return bt < at;
      else
        return at < bt;
    }
    else if (at >= 0 && bt < 0)
      return true;
    else if (at < 0 && bt >= 0)
      return false;
    else return at > bt;
  }
  else if (a.result[ix].status != b.result[ix].status)
    return a.result[ix].status < b.result[ix].status;

  if (a.result[ix].parallelScore != b.result[ix].parallelScore)
    return a.result[ix].parallelScore > b.result[ix].parallelScore;

  if (a.bib != b.bib) {
    return compareBib(a.bib, b.bib);
  }

  return a.names < b.names;
}

*/
//Order by known time for calculating results
static bool compareSOResult(const oSpeakerObject& a, const oSpeakerObject& b, int ix) {
  if (a.result[ix].status != b.result[ix].status) {
    if (a.result[ix].status == StatusOK)
      return true;
    else if (b.result[ix].status == StatusOK)
      return false;
    else return a.result[ix].status < b.result[ix].status;
  }
  else if (a.result[ix].status == StatusOK) {
    int at = a.result[ix].runningTime.time;
    int bt = b.result[ix].runningTime.time;
    if (at != bt)
      return at < bt;

    at = a.result[ix].runningTimeLeg.time;
    bt = b.result[ix].runningTimeLeg.time;
    if (at != bt)
      return at < bt;

    return a.names < b.names;
  }
  return a.names < b.names;
}

int SpeakerCB(gdioutput *gdi, GuiEventType type, BaseInfo* data) {
  oEvent *oe=gEvent;
  if (!oe)
    return false;

  DWORD classId=0, previousControlId, leg=0, totalResult=0, shortNames = 0, compactView = 0, classLimit = 0;
  string controlId;
  gdi->getData("ClassId", classId);
  gdi->getData("ControlId", controlId);
  gdi->getData("PreviousControlId", previousControlId);
  gdi->getData("LegNo", leg);
  gdi->getData("TotalResult", totalResult);
  gdi->getData("ShortNames", shortNames);
  gdi->getData("CompactView", compactView);
  gdi->getData("ClassLimit", classLimit);

  vector<string> ctrls;
  split(controlId, ",", ctrls);
  vector<int> ctrlIds;
  for (const string& s : ctrls)
    ctrlIds.push_back(atoi(s.c_str()));

  if (classId>0 && controlId.size()>0) {
    pClass pc = oe->getClass(classId);
    bool update = false;
    if (type == GUI_TIMEOUT)
      update = true;
    else if (pc) {
      vector<oClass::TrueLegInfo> stages;
      pc->getTrueStages(stages);
      int trueIndex = -1;
      int start = 0;
      for (size_t k = 0; k < stages.size(); k++) {
        if (leg <= DWORD(stages[k].first)) {
          trueIndex = k;
          if (k > 0)
            start = stages[k - 1].first + 1;
          break;
        }
      }
      for (int ctrlId : ctrlIds) {
        int cid = oControl::getIdIndexFromCourseControlId(ctrlId).first;
        if (trueIndex != -1) {
          /// Check all legs corresponding to the true leg.
          for (int k = start; k <= stages[trueIndex].first; k++) {
            if (pc->wasSQLChanged(k, cid)) {
              update = true;
              break;
            }
          }
        }
        else if (pc->wasSQLChanged(-1, cid))
          update = true;
      }
    }

    if (update) { 
      oe->speakerList(*gdi, classId, leg, ctrlIds, previousControlId,
        totalResult != 0, shortNames != 0, compactView != 0, classLimit);
    }
  }

  return true;
}

int MovePriorityCB(gdioutput *gdi, GuiEventType type, BaseInfo* data) {
  if (type==GUI_LINK){
    oEvent *oe = gEvent;
    if (!oe)
      return 0;

    TextInfo *ti=(TextInfo *)data;
    //gdi->alert(ti->id);
    if (ti->id.size()>1){
      //int rid=atoi(ti->id.substr(1).c_str());
      int rid = ti->getExtraInt();
      oRunner *r= oe->getRunner(rid, 0);
      if (!r) return false;

      DWORD classId=0, previousControlId = 0;
      string controlId;
      if (gdi->getData("ClassId", classId) && gdi->getData("ControlId", controlId) && gdi->getData("PreviousControlId", previousControlId)){
        DWORD leg = 0, totalResult = 0, shortNames = 0, compactView = 0, classLimit;
        gdi->getData("LegNo", leg);
        gdi->getData("TotalResult", totalResult);
        gdi->getData("ShortNames", shortNames);
        gdi->getData("CompactView", compactView);
        gdi->getData("ClassLimit", classLimit);

        if (ti->id[0]=='D'){
          r->setPriority(-1);
        }
        else if (ti->id[0]=='U'){
          r->setPriority(1);
        }
        else if (ti->id[0]=='M'){
          r->setPriority(0);
        }

        vector<string> ctrls;
        split(controlId, ",", ctrls);
        vector<int> ctrlIds;
        for (const string& s : ctrls)
          ctrlIds.push_back(atoi(s.c_str()));

        oe->speakerList(*gdi, classId, leg, ctrlIds, previousControlId, 
                        totalResult != 0, shortNames != 0, compactView != 0, classLimit);
      }
    }
  }
  return true;
}

int renderRowSpeakerList(const oSpeakerObject& r, const oSpeakerObject* next_r,
                         const vector<int> &leaderTimes, int type,
                         vector<vector<SpeakerString>>& rows,
                         bool shortName, bool useSubSecond, bool hasBib) {
  rows.emplace_back();
  int firstControlColumn = 0; // Return value
  bool twoRowsFormat = true;

  if (r.size() == 1) {
    if (r[0].place > 0)
      rows[0].emplace_back(textRight, itow(r[0].place));
    else
      rows[0].emplace_back();
  }
  wstring names;
  for (size_t k = 0; k < r.names.size(); k++) {
    if (!names.empty()) {
      names += L"/";
    }
    if (!shortName) {
      names += r.names[k];
    }
    else {
      vector<wstring> splt;
      split(r.names[k], L" ", splt);
      for (size_t j = 0; j < splt.size(); j++) {
        if (j == 0) {
          if (splt.size() > 1 && splt[j].length() > 1)
            names += splt[j].substr(0, 1) + L".";
          else
            names += splt[j];
        }
        else
          names += L" " + splt[j];
      }
    }
  }

  if (r.size() == 1 && r[0].runnersFinishedLeg < r.runnersTotalLeg && r[0].runnersFinishedLeg>0) {
    // Fraction of runners has finished on leg
    names += L" (" + itow(r[0].runnersFinishedLeg) + L"/" + itow(r.runnersTotalLeg) + L")";
  }

  for (size_t k = 0; k < r.outgoingnames.size() && r.size() == 1; k++) {
    if (k == 0) {
      names += L" \u21D2 ";
    }
    else
      names += L"/";

    if (!shortName) {
      names += r.outgoingnames[k];
    }
    else {
      vector<wstring> splt;
      split(r.outgoingnames[k], L" ", splt);
      for (size_t j = 0; j < splt.size(); j++) {
        if (j == 0) {
          if (splt.size() > 1 && splt[j].length() > 1)
            names += splt[j].substr(0, 1) + L".";
          else
            names += splt[j];
        }
        else
          names += L" " + splt[j];
      }
    }
  }

  int nameIx = 0;
  int clubTeamIx = 0;

  bool isTeam = r.owner->getTeam();
  if (isTeam && r.owner->getClassRef(false)) {
    ClassType ct = r.owner->getClassRef(true)->getClassType();
    isTeam = ct != ClassType::oClassIndividual && ct != ClassType::oClassKnockout;
  }

  if (hasBib) {
    rows[0].emplace_back(normalText, r.bib);
  }

  if (r.size() > 1 && twoRowsFormat) {
    rows.emplace_back();
    rows[1].resize(rows[0].size()); 
    if (isTeam)
      nameIx = 1; // Team first, then name
    else
      clubTeamIx = 1; // Name first, then club
  }

  if (r.finishStatus <= 1 || r.finishStatus == r[0].status || r.size()>1)
    rows[nameIx].emplace_back(normalText, names);
  else
    rows[nameIx].emplace_back(normalText, names + L" (" + oEvent::formatStatus(r.finishStatus, true) + L")");

  rows[clubTeamIx].emplace_back(normalText, r.club);
  auto ssMode = useSubSecond ? SubSecond::On : SubSecond::Auto; 
  firstControlColumn = rows[0].size();
  if (r.size() == 1) {
    // Single result mode
    if (r[0].status == StatusOK || (r[0].status == StatusUnknown && r[0].runningTime.time > 0)) {
      wstring timeStr = formatTime(r[0].runningTime.preliminary, ssMode);
      rows[0].emplace_back(textRight, timeStr);

      if (r[0].runningTime.time != r[0].runningTimeLeg.time)
        rows[0].emplace_back(textRight, formatTime(r[0].runningTimeLeg.time, ssMode));
      else
        rows[0].emplace_back();

      if (leaderTimes[0] != constNoLeaderTime && leaderTimes[0] > 0) {
        int flag = timerCanBeNegative;
        if (useSubSecond)
          flag |= timeWithTenth;

        wstring afterStr = gdioutput::getTimerText(r[0].runningTime.time - leaderTimes[0], flag, false, L"");
        rows[0].emplace_back(textRight, afterStr);
      }
      else
        rows[0].emplace_back();
    }
    else if (r[0].status == StatusUnknown) {
      DWORD timeOut = NOTIMEOUT;

      if (r[0].runningTimeLeg.preliminary >= -timeConstSecond && !r.missingStartTime) {

        if (next_r && (*next_r)[0].status == StatusOK && (*next_r)[0].runningTime.preliminary > r[0].runningTime.preliminary)
          timeOut = (*next_r)[0].runningTime.preliminary / timeConstSecond;

        rows[0].emplace_back(textRight, r[0].runningTime.preliminary, timeOut);

        if (r[0].runningTime.preliminary != r[0].runningTimeLeg.preliminary)
          rows[0].emplace_back(textRight, r[0].runningTimeLeg.preliminary);
        else
          rows[0].emplace_back();

        if (leaderTimes[0] != constNoLeaderTime)
          rows[0].emplace_back(timerCanBeNegative | textRight, r[0].runningTime.preliminary - leaderTimes[0]);
        else
          rows[0].emplace_back();
      }
      else {
        if (!r.startTimeS.empty())
          rows[0].emplace_back(textRight, L"[" + r.startTimeS + L"]");
        else
          rows[0].emplace_back();

        if (!r.missingStartTime && r[0].runningTimeLeg.preliminary < 0) {
          rows[0].emplace_back(timerCanBeNegative | textRight,
                           r[0].runningTimeLeg.preliminary, 0); // Timeout on start
        }
        else
          rows[0].emplace_back();

        rows[0].emplace_back();
      }
    }
    else {
      rows[0].emplace_back();
      if (r.finishStatus == StatusOK || r.finishStatus == StatusUnknown)
        rows[0].emplace_back(textRight, L"\u2013"); // Punch missing, but OK anyway
      else {
        rows[0].emplace_back(textRight, oEvent::formatStatus(r[0].status, true));
        rows[0].back().color = colorDarkRed;
      }
      rows[0].emplace_back();
    }
  }
  else {
    bool didReportBad = false;
    // Multiple results
    int lastWithResult = -1;
    for (int j = 0; j < r.size(); j++) {
      if (r[j].status == StatusOK || (r[j].status == StatusUnknown && r[j].runningTime.time > 0))
        lastWithResult = j;
    }
    bool hasShownTimer = false;

    for (int j = 0; j < r.size(); j++) {
      if (r[j].status == StatusOK || (r[j].status == StatusUnknown && r[j].runningTime.time > 0)) {
        // With result here
        wstring timeStr = formatTime(r[j].runningTime.preliminary, ssMode);
        if (r[j].place > 0)
          timeStr += L" (" + itow(r[j].place) + L")";
        rows[0].emplace_back(textLeft, timeStr);

        int lineIx = 0;
        if (twoRowsFormat) {
          lineIx = 1;
        }

        if (leaderTimes[j] != constNoLeaderTime && leaderTimes[j] > 0) {
          int flag = timerCanBeNegative;
          if (useSubSecond)
            flag |= timeWithTenth;

          wstring afterStr = gdioutput::getTimerText(r[j].runningTime.time - leaderTimes[j], flag, false, L"");
          
          rows[0].emplace_back(textLeft, afterStr);
        }
        else
          rows[0].emplace_back();

        // Leg running time
        if (r[j].runningTime.time != r[j].runningTimeLeg.time)
          rows[lineIx].emplace_back(textLeft | italicText, L"  " + formatTime(r[j].runningTimeLeg.time, ssMode));
        else
          rows[lineIx].emplace_back();

        if (twoRowsFormat)
          rows[lineIx].emplace_back(); // Unused, lower right corner
      }
      else if (r[j].status == StatusUnknown) {
        // No result yet

        if (j < lastWithResult || hasShownTimer) {
          if (twoRowsFormat) {
            rows[0].resize(rows[1].size() + 2);
            rows[1].resize(rows[1].size() + 2);
          }
          else {
            rows[0].resize(rows[1].size() + 3);
          }
        }
        else {
          hasShownTimer = true;
#          // No result yet
          DWORD timeOut = NOTIMEOUT;          
          if (r[j].runningTimeLeg.preliminary >= -timeConstSecond && !r.missingStartTime) {
            // Waiting for competitor
            if (next_r && (*next_r)[j].status == StatusOK && (*next_r)[j].runningTime.preliminary > r[j].runningTime.preliminary)
              timeOut = (*next_r)[j].runningTime.preliminary / timeConstSecond;

            rows[0].emplace_back(textLeft, r[j].runningTime.preliminary, timeOut);

            int lineIx = 0;
            if (twoRowsFormat) {
              lineIx = 1;
            }

            if (leaderTimes[j] != constNoLeaderTime)
              rows[0].emplace_back(timerCanBeNegative | textLeft, r[j].runningTime.preliminary - leaderTimes[j]);
            else
              rows[0].emplace_back();

            if (r[j].runningTime.preliminary != r[j].runningTimeLeg.preliminary)
              rows[lineIx].emplace_back(textLeft | italicText, r[j].runningTimeLeg.preliminary);
            else
              rows[lineIx].emplace_back();

            if (twoRowsFormat)
              rows[lineIx].emplace_back(); // Unused, lower right corner
          }
          else {
            // Not yet started

            if (j == 0 && !r.startTimeS.empty())
              rows[0].emplace_back(textRight, L"[" + r.startTimeS + L"]");
            else
              rows[0].emplace_back();

            int lineIx = 0;
            if (twoRowsFormat) {
              lineIx = 1;
              rows[0].emplace_back();
            }

            if (!r.missingStartTime && r[j].runningTimeLeg.preliminary < 0) {
              rows[lineIx].emplace_back(timerCanBeNegative | textRight,
                r[j].runningTimeLeg.preliminary, 0); // Timeout on start
            }
            else
              rows[lineIx].emplace_back();

            rows[lineIx].emplace_back();
          }
        }
      }
      else {
        // Status BAD
        if (r.finishStatus == StatusOK || r.finishStatus == StatusUnknown)
          rows[0].emplace_back(textRight, L"\u2013"); // Punch missing, but OK anyway
        else if (!didReportBad) { // Only report here
          rows[0].emplace_back(textRight, oEvent::formatStatus(r[j].status, true));
          rows[0].back().color = colorDarkRed;
          didReportBad = true;
        }
        
        if (twoRowsFormat) {
          rows[0].resize(rows[1].size() + 2);
          rows[1].resize(rows[1].size() + 2);
        }
        else {
          rows[0].resize(rows[1].size() + 3);
        }
      }
    }
  }

  int ownerId = r.owner ? r.owner->getId() : 0;
  if (type == 1) {
    if (!r[0].hasResult()) {
      rows[0].emplace_back(normalText, lang.tl(L"[Bort]"));
      rows[0].back().color = colorRed;
      rows[0].back().moveKey = "D" + itos(ownerId);
    }
  }
  else if (type == 2) {
    rows[0].emplace_back(normalText, lang.tl(L"[Bevaka]"));
    rows[0].back().color = colorGreen;
    rows[0].back().moveKey = "U" + itos(ownerId);

    rows[0].emplace_back(normalText, lang.tl(L"[Bort]"));
    rows[0].back().color = colorRed;
    rows[0].back().moveKey = "D" + itos(ownerId);
  }
  else if (type == 3) {
    if (!r[0].hasResult() && r[0].status == StatusUnknown) {
      if (r.priority < 0) {
        rows[0].emplace_back(normalText, lang.tl(L"[Återställ]"));
        rows[0].back().moveKey = "M" + itos(ownerId);
      }
      else {
        rows[0].emplace_back(normalText, lang.tl(L"[Bevaka]"));
        rows[0].back().moveKey = "U" + itos(ownerId);
      }
    }
  }

  return firstControlColumn;
}

void renderRowSpeakerList(gdioutput &gdi, int type, const oSpeakerObject *r, int x, int y, int rowHeight,
                          const vector<vector<SpeakerString>> &rows, const vector<int> &pos) {
  int lh=gdi.getLineHeight();
  bool highlight = false;
  if (r && r->highlight(highlightNewResultTime)) {
    if (type == 1) {
      RECT rc;
      rc.left = x + pos[1] - 4;
      rc.right = x + pos.back() + gdi.scaleLength(60);
      rc.top = y - 1;
      rc.bottom = y + lh + 1;
      gdi.addRectangle(rc, colorLightGreen, false);
    }
    highlight = true;
    gdi.addTimeout(highlightNewResultTime + 5, SpeakerCB);
  }
  
  for (auto& row : rows) {
    for (size_t k = 0; k < row.size(); k++) {
      int limit = pos[k + 1] - pos[k] - 3;
      if (limit < 0)
        limit = 0;
      if (!row[k].hasTimer) {
        if (!row[k].str.empty()) {
          if (row[k].moveKey.empty()) {

            TextInfo& ti = gdi.addStringUT(y, x + pos[k], row[k].format, row[k].str, limit);
            if (row[k].color != colorDefault)
              ti.setColor(row[k].color);
          }
          else {
            TextInfo& ti = gdi.addStringUT(y, x + pos[k], row[k].format, row[k].str, limit, MovePriorityCB);
            ti.id = row[k].moveKey;
            if (r)
              ti.setExtra(r->owner->getId());
            if (!highlight && row[k].color != colorDefault)
              ti.setColor(row[k].color);
          }
        }
      }
      else {
        gdi.addTimer(y, x + pos[k], row[k].format, row[k].timer / timeConstSecond, L"", limit,
          row[k].timeout != NOTIMEOUT ? SpeakerCB : nullptr, row[k].timeout);
      }
    }
    y += rowHeight;
  }
}

static void calculateSpeakerResults(list<oSpeakerObject>& rl) {
  
  if (rl.empty())
    return;

  int numResult = rl.front().result.size();

  for (auto& so : rl) {
    if (numResult != so.result.size())
      throw meosException("Internal error");
  }


  for (int resultIx = 0; resultIx < numResult; resultIx ++) {

    rl.sort([resultIx](const oSpeakerObject& a, const oSpeakerObject& b) {
      return compareSOResult(a, b, resultIx);
    });

    int cPlace = 0;
    int vPlace = 0;
    int cTime = 0;

    for (auto& so : rl) {
      if (so.result[resultIx].status == StatusOK) {
        cPlace++;
        int rt = so.result[resultIx].runningTime.time;
        if (rt > cTime)
          vPlace = cPlace;
        cTime = rt;
        so.result[resultIx].place = vPlace;
      }
      else
        so.result[resultIx].place = 0;
    }
  }
}

void oEvent::speakerList(gdioutput& gdi, 
                         int classId, int leg,
                         const vector<int> &controlId,
                         int previousControlId, 
                         bool totalResults, 
                         bool shortNames,
                         bool compactView,
                         int classLimit) {
#ifdef _DEBUG
  OutputDebugString(L"SpeakerListUpdate\n");
#endif

  DWORD clsIds = 0, cLegs = 0, cTotal = 0, cShort = 0, cCompact = 0, cClassLimit;
  string ctrlIds;
  gdi.getData("ClassId", clsIds);
  gdi.getData("ControlId", ctrlIds);
  gdi.getData("LegNo", cLegs);
  gdi.getData("TotalResult", cTotal);
  gdi.getData("ShortNames", cShort);
  gdi.getData("CompactView", cCompact);
  gdi.getData("ClassLimit", cClassLimit);

  string ctrlCoded;
  vector<string> srep;
  for (int c : controlId)
    srep.push_back(itos(c));
  unsplit(srep, string(","), ctrlCoded);

  bool refresh = clsIds == classId && ctrlIds == ctrlCoded && leg == cLegs &&
    (totalResults ? 1 : 0) == cTotal && (shortNames ? 1 : 0) == cShort && cClassLimit == classLimit;

  if (refresh)
    gdi.takeShownStringsSnapshot();

  int storedY = gdi.getOffsetY();
  int storedHeight = gdi.getHeight();

  gdi.restoreNoUpdate("SpeakerList");
  gdi.setRestorePoint("SpeakerList");

  gdi.pushX(); gdi.pushY();
  gdi.updatePos(0, 0, 0, storedHeight);
  gdi.popX(); gdi.popY();
  gdi.setOffsetY(storedY);

  gdi.setData("ClassId", classId);
  gdi.setData("ControlId", ctrlCoded);
  gdi.setData("PreviousControlId", previousControlId);
  gdi.setData("LegNo", leg);
  gdi.setData("TotalResult", totalResults ? 1 : 0);
  gdi.setData("ShortNames", shortNames ? 1 : 0);
  gdi.setData("CompactView", compactView ? 1 : 0);
  gdi.setData("ClassLimit", classLimit);

  gdi.registerEvent("DataUpdate", SpeakerCB);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);

  list<oSpeakerObject> speakerList;

  //For preliminary times
  updateComputerTime(false);

  speakerList.clear();
  
  if (classHasTeams(classId)) {
    oTeamList::iterator it = Teams.begin();
    while (it != Teams.end()) {
      if (it->getClassId(true) == classId && !it->skip()) {
        auto s = it->getStatus();
        if (s == StatusCANCEL || s == StatusNotCompeting) // Allow DNS for incomplete teams
          continue;
        oSpeakerObject& so = speakerList.emplace_back();
        it->fillSpeakerObject(leg, previousControlId, controlId, totalResults, so);
        if (!so.owner)
          speakerList.pop_back();
      }
      ++it;
    }
  }
  else {
    pClass pc = getClass(classId);
    bool qfBaseClass = pc && pc->getQualificationFinal();
    bool qfFinalClass = pc && pc->getParentClass();

    if (qfBaseClass || qfFinalClass)
      leg = 0;

    for (auto& it : Runners) {
      if (it.getClassId(true) == classId && !it.isRemoved()) {
        if (qfBaseClass && it.getLegNumber() > 0)
          continue;

        auto s = it.getStatus();
        if (s == StatusCANCEL || s == StatusNotCompeting)
          continue;
        oSpeakerObject& so = speakerList.emplace_back();
        it.fillSpeakerObject(leg, previousControlId, controlId, totalResults, so);
        if (!so.owner)
          speakerList.pop_back();
      }
    }
  }
  if (speakerList.empty()) {
    gdi.dropLine();
    gdi.addString("", fontMediumPlus, "Inga deltagare");
    gdi.refresh();
    return;
  }

  if (controlId.size() == 1) {
    for (auto& sl : speakerList) {
      if (sl.hasResult(0) && sl.priority >= 0)
        sl.priority = 1;
      else if (sl.result[0].status > StatusOK && sl.priority <= 0)
        sl.priority = -1;
    }
  }
  else {
    for (auto& sl : speakerList) {
      sl.priority = 2;
    }
  }

  //Calculate place...
  calculateSpeakerResults(speakerList);

  list<oSpeakerObject*> sortedList;
  if (controlId.size() == 1) {
    //Calculate preliminary times and sort by this and prio.
    speakerList.sort([](const oSpeakerObject& a, const oSpeakerObject& b) {
       
      if (!a.result[0].hasResult() && !b.result[0].hasResult() && a.priority != b.priority)
        return a.priority > b.priority;
      return compareSpkSPList(a, b, 0);
    });

    for (auto& so : speakerList)
      sortedList.push_back(&so);
  }
  else {
    for (auto& so : speakerList) {
      so.compareResultIndex = -1;
      so.nextPreliminaryTime = -1;
      bool hasLastResult = false;
      for (int ix = 0; ix < so.size(); ix++) {
        if (so[ix].hasResult()) {
          hasLastResult = true;
          so.compareResultIndex = ix;
        }
        else if (hasLastResult) {
          so.nextPreliminaryTime = ix;
          hasLastResult = false;
        }
      }
    }
    vector<oSpeakerObject*> sortSelection;
    for (int ix = 0; ix <= controlId.size(); ix++) {
      //OutputDebugString((L"\n**** Sorting: " + itow(ix - 1) + L"\n").c_str());
      sortSelection.clear();
      for (auto& so : speakerList) {
        if (so.compareResultIndex == ix - 1)
          sortSelection.push_back(&so);
      }

      // Sort current control
      int compareIx = ix - 1;
      sort(sortSelection.begin(), sortSelection.end(), [compareIx](const oSpeakerObject* a, const oSpeakerObject * b) {
        return compareSpkSPList(*a, *b, compareIx);
      });

      //for (auto& ss : sortSelection) {
      //  OutputDebugString((ss->names[0] + L"\n").c_str());
      //}

      //OutputDebugString((L"\n**** Merging: " + itow(ix - 1) + L"\n").c_str());

      // Merge with sorted list
      auto sit = sortedList.begin();

      for (auto& ss : sortSelection) {
        while (sit != sortedList.end()) {
          if (compareSpkSPListPart(**sit, *ss, compareIx))
            ++sit;
          else
            break;
        }

        if (sit == sortedList.end()) {
          sortedList.push_back(ss);
          sit = sortedList.end();
        }
        else {
          //OutputDebugString((ss->names[0] + L" < " + (*sit)->names[0] + L"\n").c_str());
          sortedList.insert(sit, ss);
        }
      }   
    }
  }

  pClass pCls = oe->getClass(classId);
  if (!pCls)
    return;

  vector<oClass::TrueLegInfo > stages;
  pCls->getTrueStages(stages);

  pCourse crs = deduceSampleCourse(pCls, stages, leg);
  wstring legName, cname = pCls->getName();
  size_t istage = -1;
  bool useSS = useSubSecond(); // Only for finish time??
  for (size_t k = 0; k < stages.size(); k++) {
    if (stages[k].first >= leg) {
      istage = k;
      break;
    }
  }
  if (stages.size() > 1 && istage < stages.size())
    cname += lang.tl(L", Sträcka X#" + itow(stages[istage].second));

  if (controlId.size() == 1) {
    if (controlId[0] != oPunch::PunchFinish && crs) {
      cname += L", " + crs->getRadioName(controlId[0]);
    }
    else {
      cname += lang.tl(L", Mål");
    }
  }

  int y = gdi.getCY();
  int x = gdi.scaleLength(30);
  int lh = gdi.getLineHeight();

  if (compactView) {
    gdi.setWindowTitle(makeDash(L"Speakerstöd - " + cname));
    y -= lh / 2;
  }
  else {
    y += gdi.scaleLength(5);
    gdi.addStringUT(y, x, fontMediumPlus, cname).setColor(colorGreyBlue);
    y += lh * 2;
  }

  int numResult = speakerList.front().result.size();
  vector<int> leaderTime(numResult, constNoLeaderTime);

  //Calculate leader-time
  for (const auto& sl : speakerList) {
    for (int j = 0; j < numResult; j++) {
      int rt = sl.result[j].runningTime.time;
      if ((sl.result[j].status == StatusOK || sl.result[j].status == StatusUnknown) && rt > 0)
        leaderTime[j] = min(leaderTime[j], rt);
    }
  }

  vector<pair<oSpeakerObject*, vector<vector<SpeakerString>>>> toRender;

  size_t maxRow = 0;

  if (numResult > 1) {
    toRender.emplace_back();
    toRender.back().second.emplace_back();
    auto& row = toRender.back().second.back();
  }

  int firstColumn = -1;

  // Determine if bibs are needed
  bool classHasBib = false;
  for (auto &r : sortedList) {
    if (!r->bib.empty()) {
      classHasBib = true;
      break;
    }
  }

  auto sit = sortedList.begin();
  int count = 0;
  int lastPlace = 0;
  while (sit != sortedList.end()) {
    if (classLimit > 0 && ++count > classLimit) {
      if (lastPlace == 0 || (*sit)->result.back().place != lastPlace)
        break;
    }
    toRender.emplace_back();
    auto& [so, textLines] = toRender.back();
    so = *sit; 
    ++sit;
    lastPlace = so->result.back().place;
    const oSpeakerObject* next = nullptr;
    if (sit != sortedList.end())
      next = *sit;

    int type = 0;
    if (numResult == 1) {
      type = 3;
      if (so->priority > 0 || so->hasResult(0))
        type = 1;
      else if (so->result[0].status == StatusUnknown && so->priority == 0)
        type = 2;
    }

    firstColumn = renderRowSpeakerList(*so, next, leaderTime,
                                       type, textLines, 
                                       shortNames, useSS, classHasBib);

    for (auto& row : textLines)
      maxRow = max(maxRow, row.size());
  }

  constexpr int colsPerResult = 2;

  if (numResult > 1 && firstColumn > 0) {
    toRender.emplace_back();
    auto& [so, textLines] = toRender.back();
    textLines.resize(1);
    textLines[0].resize(maxRow);
    wstring cn;
    for (int c = 0; c < numResult; c++) {
      int ix = firstColumn + c * colsPerResult;

      if (controlId[c] != oPunch::PunchFinish && crs) {
        cn = crs->getRadioName(controlId[c]);
      }
      else {
        cn = lang.tl(L"Mål");
      }
      textLines[0][ix].str = cn;
      textLines[0][ix].format = boldText;
      if (colsPerResult > 1) {
        if (textLines[0].size() <= ix + 1)
          textLines[0].emplace_back(); 
        textLines[0][ix + 1].str = L" "; // Needed to ensure enough width
      }

    }
  }

  vector<int> dx_new(maxRow);
  int s60 = gdi.scaleLength(60);
  int s28 = gdi.scaleLength(35);
  int s7 = gdi.scaleLength(7);
  TextInfo ti;

  HDC hDC = GetDC(gdi.getHWNDTarget());

  ti.xp = 0;
  ti.yp = 0;
  ti.format = 0;
  ti.text = L"59:59 (8)";
  gdi.calcStringSize(ti, hDC);
  int maxSCOL = ti.textRect.right + s7;

  for (size_t k = 0; k < toRender.size(); k++) {
    const auto& rows = toRender[k].second;
    for (auto& row : rows) {
      for (size_t j = 0; j < row.size(); j++) {
        if (!row[j].str.empty()) {
          ti.xp = 0;
          ti.yp = 0;
          ti.format = 0;
          ti.text = row[j].str;
          gdi.calcStringSize(ti, hDC);
          if (j > 0)
            dx_new[j] = max(maxSCOL, max<int>(dx_new[j], ti.textRect.right + s7));
          else
            dx_new[j] = max(s28, max<int>(dx_new[j], ti.textRect.right + s7));
        }
        else if (row[j].hasTimer) {
          dx_new[j] = max<int>(dx_new[j], s60);
        }
      }
    }
  }
  ReleaseDC(gdi.getHWNDTarget(), hDC);

  int limit = gdi.scaleLength(280);
  vector<int> dx(maxRow + 1);
  dx[0] = gdi.scaleLength(4);
  for (size_t k = 1; k < dx.size(); k++) {
    dx[k] = dx[k - 1] + min(limit, dx_new[k - 1] + gdi.scaleLength(4));
  }

  if (numResult == 1) {
    auto doRenderType = [&](int type, const string& header, auto filter) {
      bool rendered = false;
      for (auto &[so, rows] : toRender) {
        if (so && filter(*so)) {
          if (rendered == false) {
            gdi.addString("", y + gdi.scaleLength(4), x, boldSmall, header);
            y += lh + gdi.scaleLength(5), rendered = true;
          }
          renderRowSpeakerList(gdi, 1, so, x, y, lh, rows, dx);
          y += lh;
          so = nullptr; // Mark as rendered
        }
      }
    };

    doRenderType(1, "Resultat", [](const oSpeakerObject& so) -> bool {return so.priority > 0 || so.hasResult(0); });
    doRenderType(2, "Inkommande", [](const oSpeakerObject& so) -> bool {return so.isIncomming(0) && so.priority == 0; });
    doRenderType(3, "Övriga", [](const oSpeakerObject& so) -> bool {return true; });
  }
  else {
    renderRowSpeakerList(gdi, 0, nullptr, x, y, lh, toRender.back().second, dx);
    y += (lh * 3) / 2;

    GDICOLOR bColor = GDICOLOR(RGB(150, 170, 150));
    int topY = y;
    int marginY = gdi.scaleLength(4);
    int count = 0;
    for (auto& [so, rows] : toRender) {
      if (so) {
        bool high = so->highlight(highlightNewResultTime);
        if (count % 2 == 0 || high) {
          RECT rc;
          rc.top = y - marginY/2;
          rc.bottom = y + lh * rows.size() + marginY / 2;
          rc.left = x;
          rc.right = x + dx.back();
          if (high)
            gdi.addRectangle(rc, colorYellow, true, false, bColor);
          else
            gdi.addRectangle(rc, colorLightGreen, true, false, bColor);
        }
        count++;
        renderRowSpeakerList(gdi, 2, so, x, y, lh, rows, dx);
        y += lh * rows.size() + marginY;
      }
    }

    int bottomY = y;

    RECT rc;
    rc.left = x;
    rc.right = x + dx.back();
    rc.top = topY - marginY/2;
    rc.bottom = bottomY;
    gdi.addRectangle(rc, GDICOLOR::colorTransparent, true, false, GDICOLOR::colorBlack);

    for (int c = 0; c < numResult; c++) {
      int ix = firstColumn + c * colsPerResult;
      rc.left = x + dx[ix] - marginY / 2;
      rc.right = x + dx[ix] - marginY / 2;
      gdi.addRectangle(rc, GDICOLOR::colorTransparent, true, false,  bColor);
    }
  }

  if (refresh)
    gdi.refreshSmartFromSnapshot(false);
  else
    gdi.refresh();
}

void oEvent::updateComputerTime(bool considerDate) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  if (!useLongTimes() && !considerDate)
    computerTime = (((24+2+st.wHour)*timeConstHour+st.wMinute*timeConstMinute+st.wSecond*timeConstSecond - ZeroTime)%(24*timeConstHour)-2*timeConstHour) * (1000/timeConstSecond) + st.wMilliseconds;
  else {
    SYSTEMTIME stDate;
    if (convertDateYMD(getDate(), stDate, true) == -1) {
      stDate = st;
      stDate.wHour = 0;
      stDate.wMinute = 0;
      stDate.wSecond = 0;
      stDate.wMilliseconds = 0;
    }

    stDate.wHour = (ZeroTime / timeConstHour)%24;
    stDate.wMinute = (ZeroTime / timeConstMinute) % 60;
    stDate.wSecond = (ZeroTime / timeConstSecond) % 60;
    st.wMilliseconds = (ZeroTime % timeConstSecond) * 1000;

    int64_t zero = SystemTimeToInt64TenthSecond(stDate);
    int64_t now = SystemTimeToInt64TenthSecond(st);
    computerTime = (now - zero) * (1000 / timeConstSecond) + st.wMilliseconds % (1000 / timeConstSecond);
  }
}

void oEvent::clearPrewarningSounds() {
  oFreePunchList::reverse_iterator it;
  for (it=punches.rbegin(); it!=punches.rend(); ++it)
    it->hasBeenPlayed=true;
}

void oEvent::tryPrewarningSounds(const wstring &basedir, int number) {
  wchar_t wave[20];
  swprintf_s(wave, L"%d.wav", number);

  wstring file=basedir+L"\\"+wave;

  if (_waccess(file.c_str(), 0)==-1)
    gdibase.alert(L"Fel: hittar inte filen X.#" + file);

  PlaySound(file.c_str(), 0, SND_SYNC|SND_FILENAME );
}

void oEvent::playPrewarningSounds(const wstring &basedir, set<int> &controls) {
  oFreePunchList::reverse_iterator it;
  for (it=punches.rbegin(); it!=punches.rend() && !it->hasBeenPlayed; ++it) {

    if (controls.count(it->type)==1 || controls.empty()) {
      pRunner r = getRunnerByCardNo(it->CardNo, it->getAdjustedTime(), oEvent::CardLookupProperty::ForReadout);

      if (r){
        wchar_t wave[20];
        swprintf_s(wave, L"%d.wav", r->getStartNo());

        wstring file=basedir+L"\\"+ r->getDI().getString("Nationality") +L"\\"+wave;

        if (_waccess(file.c_str(), 0)==-1)
          file=basedir+L"\\"+wave;

        PlaySound(file.c_str(), 0, SND_SYNC|SND_FILENAME );
        it->hasBeenPlayed=true;
      }
    }
  }
}

static bool compareFinishTime(pRunner a, pRunner b) {
  return a->getFinishTime() < b->getFinishTime();
}

struct TimeRunner {
  TimeRunner(int t, pRunner r) : time(t), runner(r) {}
  int time;
  pRunner runner;
  bool operator<(const TimeRunner &b) const {return time<b.time;}
};

wstring getOrder(int k) {
  wstring str;
  if (k==1)
    str = L"första";
  else if (k==2)
    str = L"andra";
  else if (k==3)
    str = L"tredje";
  else if (k==4)
    str = L"fjärde";
  else if (k==5)
    str = L"femte";
  else if (k==6)
    str = L"sjätte";
  else if (k==7)
    str = L"sjunde";
  else if (k==8)
    str = L"åttonde";
  else if (k==9)
    str = L"nionde";
  else if (k==10)
    str = L"tionde";
  else if (k==11)
    str = L"elfte";
  else if (k==12)
    str = L"tolfte";
  else
    return lang.tl(L"X:e#" + itow(k));

  return lang.tl(str);
}

wstring getNumber(int k) {
  wstring str;
  if (k==1)
    str = L"etta";
  else if (k==2)
    str = L"tvåa";
  else if (k==3)
    str = L"trea";
  else if (k==4)
    str = L"fyra";
  else if (k==5)
    str = L"femma";
  else if (k==6)
    str = L"sexa";
  else if (k==7)
    str = L"sjua";
  else if (k==8)
    str = L"åtta";
  else if (k==9)
    str = L"nia";
  else if (k==10)
    str = L"tia";
  else if (k==11)
    str = L"elva";
  else if (k==12)
    str = L"tolva";
  else
    return lang.tl(L"X:e#" + itow(k));

  return lang.tl(str);
}

struct BestTime {
  static const int nt = 4;
  static const int maxtime = timeConstHour*24*7;
  int times[nt];

  BestTime() {
    for (int k = 0; k<nt; k++)
      times[k] = maxtime;
  }

  void addTime(int t) {
    for (int k = 0; k<nt; k++) {
      if (t<times[k]) {
        int a = times[k];
        times[k] = t;
        t = a;
      }
    }
  }

  // Get the best time, but filter away unrealistic good times
  int getBestTime() {
    int avg = 0;
    int navg = 0;
    for (int k = 0; k<nt; k++) {
      if (times[k] < maxtime) {
        avg += times[k];
        navg++;
      }
    }

    if (navg == 0)
      return 0;
    else if (navg == 1)
      return avg;
    int limit = int(0.8 * double(avg) / double(navg));
    for (int k = 0; k<nt; k++) {
      if (limit < times[k] && times[k] < maxtime) {
        return times[k];
      }
    }
    return 0; // Cannot happen
  }

  // Get the second best time
  int getSecondBestTime() {
    int t = getBestTime();
    if (t > 0) {
      for (int k = 0; k < nt-1; k++) {
        if (t == times[k] && times[k+1] < maxtime)
          return times[k+1];
        else if (t < times[k] && times[k] < maxtime)
          return times[k]; // Will not happen with current implementation of besttime
      }
    }
    return t;
  }
};

struct PlaceRunner {
  PlaceRunner(int p, pRunner r) : place(p), runner(r) {}
  int place;
  pRunner runner;
};

// ClassId -> (Time -> Place, Runner)
typedef multimap<int, PlaceRunner> TempResultMap;

void insertResult(TempResultMap &rm, oRunner &r, int time,
                  int &place, bool &sharedPlace, vector<pRunner> &preRunners) {
  TempResultMap::iterator it = rm.insert(make_pair(time, PlaceRunner(0, &r)));
  place = 1;
  sharedPlace = false;
  preRunners.clear();

  if (it != rm.begin()) {
    TempResultMap::iterator p_it = it;
    --p_it;
    if (p_it->first < it->first)
      place = p_it->second.place + 1;
    else {
      place = p_it->second.place;
      sharedPlace = true;
    }
    int pretime = p_it->first;
    while(p_it->first == pretime) {
      preRunners.push_back(p_it->second.runner);
      if (p_it != rm.begin())
        --p_it;
      else
        break;
    }
  }

  TempResultMap::iterator p_it = it;
  if ( (++p_it) != rm.end() && p_it->first == time)
    sharedPlace = true;


  int lastplace = place;
  int cplace = place;

  // Update remaining
  while(it != rm.end()) {
    if (time == it->first)
      it->second.place = lastplace;
    else {
      it->second.place = cplace;
      lastplace = cplace;
      time = it->first;
    }
    ++cplace;
    ++it;
  }
}

int oEvent::setupTimeLineEvents(int currentTime)
{
  if (currentTime == 0) {
    updateComputerTime(false);
    currentTime = getComputerTime();
  }

  int nextKnownEvent = timeConstHour*48;
  vector<pRunner> started;
  started.reserve(Runners.size());
  timeLineEvents.clear();

  for(set<int>::iterator it = timelineClasses.begin(); it != timelineClasses.end(); ++it) {
    int ne = setupTimeLineEvents(*it, currentTime);
    nextKnownEvent = min(ne, nextKnownEvent);
    modifiedClasses.erase(*it);
  }
  return nextKnownEvent;
}


int oEvent::setupTimeLineEvents(int classId, int currentTime)
{
  // leg -> started on leg
  vector< vector<pRunner> > started;
  started.reserve(32);
  int nextKnownEvent = timeConstHour*48;
  int classSize = 0;

  pClass pc = getClass(classId);
  if (!pc)
    return nextKnownEvent;

  vector<char> skipLegs;
  skipLegs.resize(max<int>(1, pc->getNumStages()));

  for (size_t k = 1; k < skipLegs.size(); k++) {
    LegTypes lt = pc->getLegType(k);
    if (lt == LTIgnore) {
      skipLegs[k] = true;
    }
    else if (lt == LTParallel || lt == LTParallelOptional) {
      StartTypes st = pc->getStartType(k-1);
      if (st != STChange) {
        // Check that there is no forking
        vector<pCourse> &cc1 = pc->MultiCourse[k-1];
        vector<pCourse> &cc2 = pc->MultiCourse[k];
        bool equal = cc1.size() == cc2.size();
        for (size_t j = 0; j<cc1.size() && equal; j++)
          equal = cc1[j] == cc2[j];

        if (equal) { // Don't watch both runners if "patrol" style
          skipLegs[k-1] = true;
        }
      }
    }
  }
  // Count the number of starters at the same time
  inthashmap startTimes;

  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    oRunner &r = *it;
    if (r.isRemoved() || r.isVacant())
      continue;
    if (!r.Class ||r.Class->Id != classId)
      continue;
    if (r.tStatus == StatusDNS || r.tStatus == StatusCANCEL || r.tStatus == StatusNotCompeting)
      continue;
//    if (r.CardNo == 0)
//      continue;
    if (r.tLeg == 0)
      classSize++; // Count number of starts on first leg
    if (size_t(r.tLeg) < skipLegs.size() && skipLegs[r.tLeg])
      continue;
    if (r.tStartTime > 0 && r.tStartTime <= currentTime) {
      if (started.size() <= size_t(r.tLeg)) {
        started.resize(r.tLeg+1);
        started.reserve(Runners.size() / (r.tLeg + 1));
      }
      r.tTimeAfter = 0; //Reset time after
      r.tInitialTimeAfter = 0;
      started[r.tLeg].push_back(&r);
      int id = r.tLeg + 100 * r.tStartTime;
      ++startTimes[id];
    }
    else if (r.tStartTime > currentTime) {
      nextKnownEvent = min(r.tStartTime, nextKnownEvent);
    }
  }

  if (started.empty())
    return nextKnownEvent;

  size_t firstNonEmpty = 0;
  while (started[firstNonEmpty].empty()) // Note -- started cannot be empty.
    firstNonEmpty++;

  int sLimit = min(4, classSize);

  if (false && startTimes.size() == 1) {
    oRunner &r = *started[firstNonEmpty][0];

    oTimeLine tl(r.tStartTime, oTimeLine::TLTStart, oTimeLine::PHigh, r.getClassId(true), 0, 0);
    TimeLineIterator it = timeLineEvents.insert(pair<int, oTimeLine>(r.tStartTime, tl));
    it->second.setMessage(L"X har startat.#" + r.getClass(true));
  }
  else {
    for (size_t j = 0; j<started.size(); j++) {
      bool startedClass = false;
      for (size_t k = 0; k<started[j].size(); k++) {
        oRunner &r = *started[j][k];
        int id = r.tLeg + 100 * r.tStartTime;
        if (startTimes[id] < sLimit) {
          oTimeLine::Priority prio = oTimeLine::PLow;
          int p = r.getSpeakerPriority();
          if (p > 1)
            prio = oTimeLine::PHigh;
          else if (p == 1)
            prio = oTimeLine::PMedium;
          oTimeLine tl(r.tStartTime, oTimeLine::TLTStart, prio, r.getClassId(true), r.getId(), &r);
          TimeLineIterator it = timeLineEvents.insert(pair<int, oTimeLine>(r.tStartTime + 1, tl));
          it->second.setMessage(L"har startat.");
        }
        else if (!startedClass) {
          // The entire class started
          oTimeLine tl(r.tStartTime, oTimeLine::TLTStart, oTimeLine::PHigh, r.getClassId(true), 0, 0);
          TimeLineIterator it = timeLineEvents.insert(pair<int, oTimeLine>(r.tStartTime, tl));
          it->second.setMessage(L"X har startat.#" + r.getClass(true));
          startedClass = true;
        }
      }
    }
  }
  set<int> controlId;
  getFreeControls(controlId);

  // Radio controls for each leg, pair is (courseControlId, control)
  map<int, vector<pair<int, pControl> > > radioControls;

  for (size_t leg = 0; leg<started.size(); leg++) {
    for (size_t k = 0; k < started[leg].size(); k++) {
      if (radioControls.count(leg) == 0) {
        pCourse pc = started[leg][k]->getCourse(false);
        if (pc) {
          vector< pair<int, pControl> > &rc = radioControls[leg];
          for (int j = 0; j < pc->nControls(); j++) {
            int id = pc->controls[j]->Id;
            if (controlId.count(id) > 0) {
              rc.push_back(make_pair(pc->getCourseControlId(j), pc->controls[j]));
            }
          }
        }
      }
    }
  }
  for (size_t leg = 0; leg<started.size(); leg++) {
    const vector< pair<int, pControl> > &rc = radioControls[leg];
    int nv = setupTimeLineEvents(started[leg], rc, currentTime, leg + 1 == started.size());
    nextKnownEvent = min(nv, nextKnownEvent);
  }
  return nextKnownEvent;
}

wstring getTimeDesc(int t1, int t2);


void getTimeAfterDetail(wstring &detail, int timeAfter, int deltaTime, bool wasAfter) {
  wstring aTimeS = getTimeDesc(timeAfter, 0);
  if (timeAfter > 0) {
    if (!wasAfter || deltaTime == 0)
      detail = L"är X efter#" + aTimeS;
    else {
      wstring deltaS = getTimeDesc(deltaTime, 0);
      if (deltaTime > 0)
        detail = L"är X efter; har tappat Y#" + aTimeS + L"#" + deltaS;
      else
        detail = L"är X efter; har tagit in Y#" + aTimeS + L"#" + deltaS;
    }
  }
  else if (timeAfter < 0) {

    if (wasAfter && deltaTime != 0) {
      wstring deltaS = getTimeDesc(deltaTime, 0);
      if (deltaTime > 0)
        detail = L"leder med X; har tappat Y.#" + aTimeS + L"#" + deltaS;
      else if (deltaTime < 0)
        detail = L"leder med X; sprang Y snabbare än de jagande.#" + aTimeS + L"#" + deltaS;
      else
        detail = L"leder med X#" + aTimeS;
    }
  }
}

void oEvent::timeLinePrognose(TempResultMap &results, TimeRunner &tr, int prelT,
                              int radioNumber, const wstring &rname, int radioId) {
  TempResultMap::iterator place_it = results.lower_bound(prelT);
  int p = place_it != results.end() ? place_it->second.place : results.size() + 1;
  int prio = tr.runner->getSpeakerPriority();

  if ((radioNumber == 0 && prio > 0) || (radioNumber > 0 && p <= 10) || (radioNumber > 0 && prio > 0 && p <= 20)) {
    wstring msg;
    if (radioNumber > 0) {
      if (p == 1)
        msg = L"väntas till X om någon minut, och kan i så fall ta ledningen.#" + rname;
      else
        msg = L"väntas till X om någon minut, och kan i så fall ta en Y plats.#" + rname + getOrder(p);
    }
    else
      msg = L"väntas till X om någon minut.#" + rname;

    oTimeLine::Priority mp = oTimeLine::PMedium;
    if (p <= (3 + prio * 3))
      mp = oTimeLine::PHigh;
    else if (p>6 + prio * 5)
      mp = oTimeLine::PLow;

    oTimeLine tl(tr.time, oTimeLine::TLTExpected, mp, tr.runner->getClassId(true), radioId, tr.runner);
    TimeLineIterator tlit = timeLineEvents.insert(pair<int, oTimeLine>(tl.getTime(), tl));
    tlit->second.setMessage(msg);
  }
}

int oEvent::setupTimeLineEvents(vector<pRunner> &started, const vector< pair<int, pControl> > &rc, int currentTime, bool finish)
{
  int nextKnownEvent = 48*timeConstHour;
  vector< vector<TimeRunner> > radioResults(rc.size());
  vector<BestTime> bestLegTime(rc.size() + 1);
  vector<BestTime> bestTotalTime(rc.size() + 1);
  vector<BestTime> bestRaceTime(rc.size());

  for (size_t j = 0; j < rc.size(); j++) {
    int id = rc[j].first;
    vector<TimeRunner> &radio = radioResults[j];
    radio.reserve(started.size());
    for (size_t k = 0; k < started.size(); k++) {
      int rt;
      oRunner &r = *started[k];
      RunnerStatus rs;
      r.getSplitTime(id, rs, rt);
      if (rs == StatusOK)
        radio.push_back(TimeRunner(rt + r.tStartTime, &r));
      else
        radio.push_back(TimeRunner(0, &r));

      if (rt > 0) {
        bestTotalTime[j].addTime(r.getTotalRunningTime(rt + r.tStartTime, true, true));
        bestRaceTime[j].addTime(rt);
        // Calculate leg time since last radio (or start)
        int lt = 0;
        if (j == 0)
          lt = rt;
        else if (radioResults[j-1][k].time>0)
          lt = rt + r.tStartTime - radioResults[j-1][k].time;

        if (lt>0)
          bestLegTime[j].addTime(lt);

        if (j == rc.size()-1 && r.FinishTime>0 && r.tStatus == StatusOK) {
          // Get best total time
          bestTotalTime[j+1].addTime(r.getTotalRunningTime(r.FinishTime, true, true));

          // Calculate best time from last radio to finish
          int ft = r.FinishTime - (rt + r.tStartTime);
          if (ft > 0)
            bestLegTime[j+1].addTime(ft);
        }
      }
    }
  }

  // Relative speed of a runner
  vector<double> relSpeed(started.size(), 1.0);

  // Calculate relative speeds
  for (size_t k = 0; k < started.size(); k++) {
    size_t j = 0;
    int j_radio = -1;
    int time = 0;
    while(j < rc.size() && (radioResults[j][k].time > 0 || j_radio == -1)) {
      if (radioResults[j][k].time > 0) {
        j_radio = j;
        time = radioResults[j][k].time - radioResults[j][k].runner->tStartTime;
      }
      j++;
    }

    if (j_radio >= 0) {
      int reltime = bestRaceTime[j_radio].getBestTime();
      if (reltime == time)
        reltime = bestRaceTime[j_radio].getSecondBestTime();
      relSpeed[k] = double(time) / double(reltime);
    }
  }

  vector< vector<TimeRunner> > expectedAtNext(rc.size() + 1);

  // Time before expection to "prewarn"
  const int pwTime = 60;

  // First radio
  int bestLeg = bestLegTime[0].getBestTime();
  if (bestLeg > 0 && bestLeg != BestTime::maxtime){
    vector<TimeRunner> &radio = radioResults[0];
    vector<TimeRunner> &expectedRadio = expectedAtNext[0];
    expectedRadio.reserve(radio.size());
    for (size_t k = 0; k < radio.size(); k++) {
      oRunner &r = *radio[k].runner;
      int expected = r.tStartTime + bestLeg;
      int actual = radio[k].time;
      if ( (actual == 0 && (expected - pwTime) < currentTime) || (actual > (expected - pwTime)) ) {
        expectedRadio.push_back(TimeRunner(expected-pwTime, &r));
      }
    }
  }

  // Remaining radios and finish
  for (size_t j = 0; j < rc.size(); j++) {
    bestLeg = bestLegTime[j+1].getBestTime();
    if (bestLeg == 0 || bestLeg == BestTime::maxtime)
      continue;

    vector<TimeRunner> &radio = radioResults[j];
    vector<TimeRunner> &expectedRadio = expectedAtNext[j+1];
    expectedRadio.reserve(radio.size());
    for (size_t k = 0; k < radio.size(); k++) {
      oRunner &r = *radio[k].runner;
      if (radio[k].time > 0) {
        int expected = radio[k].time + int(bestLeg * relSpeed[k]);
        int actual = 0;
        if (j + 1 < rc.size())
          actual = radioResults[j+1][k].time;
        else
          actual = r.FinishTime;

        if ( (actual == 0 && (expected - pwTime) <= currentTime) || (actual > (expected - pwTime)) ) {
          expectedRadio.push_back(TimeRunner(expected-pwTime, &r));
        }
        else if (actual == 0 && (expected - pwTime) > currentTime) {
          nextKnownEvent = min(nextKnownEvent, expected - pwTime);
        }
      }
    }
  }

  vector<TempResultMap> timeAtRadio(rc.size());
  int numbered = 1;
  for (size_t j = 0; j < rc.size(); j++) {

    TempResultMap &results = timeAtRadio[j];
    vector<TimeRunner> &radio = radioResults[j];
    vector<TimeRunner> &expectedRadio = expectedAtNext[j];

    wstring rname;// = ->getRadioName(rc[j].first);
    if (rc[j].second->Name.empty())
      rname = lang.tl("radio X#" + itos(numbered++)) + L"#";
    else {
      if (rc[j].second->tNumberDuplicates > 1)
        rname = rc[j].second->Name + L"-" + itow(numbered++) + L"#";
      else
        rname = rc[j].second->Name + L"#";
    }
    // Sort according to pass time
    sort(radio.begin(), radio.end());
    sort(expectedRadio.begin(), expectedRadio.end());
    size_t expIndex = 0;

    for (size_t k = 0; k < radio.size(); k++) {
      RunnerStatus ts = radio[k].runner->getTotalStatus();
      if (ts == StatusMP || ts == StatusDQ)
        continue;
      if (radio[k].time > 0) {
        while (expIndex < expectedRadio.size() && expectedRadio[expIndex].time < radio[k].time) {
          TimeRunner &tr = expectedRadio[expIndex];
          int prelT = tr.runner->getTotalRunningTime(tr.time + pwTime, true, true);

          timeLinePrognose(results, tr, prelT, j, rname, rc[j].first);
          expIndex++;
        }

        oRunner &r = *radio[k].runner;
        int place = 1;
        bool sharedPlace = false;
        vector<pRunner> preRunners;
        int time = radio[k].time - r.tStartTime;
        int totTime = r.getTotalRunningTime(radio[k].time, true, true);
        insertResult(results, r, totTime, place, sharedPlace, preRunners);
        int leaderTime = results.begin()->first;
        int timeAfter = totTime - leaderTime;
        int deltaTime = timeAfter - r.tTimeAfter;

        wstring timeS = formatTime(time);
        wstring msg, detail;
        getTimeAfterDetail(detail, timeAfter, deltaTime, r.tTimeAfter > 0);
        r.tTimeAfter = timeAfter;

        if (place == 1) {
          if (results.size() == 1)
            msg = L"är först vid X med tiden Y.#" + rname + timeS;
          else if (!sharedPlace)
            msg = L"tar ledningen vid X med tiden Y.#" + rname + timeS;
          else
            msg = L"går upp i delad ledning vid X med tiden Y.#" + rname + timeS;
        }
        else {
          if (!sharedPlace) {
            msg = L"stämplar vid X som Y, på tiden Z.#" +
                          rname + getNumber(place) +
                          L"#" + timeS;

          }
          else {
            msg = L"stämplar vid X som delad Y med tiden Z.#" + rname + getNumber(place) +
                                                         L"#" + timeS;
          }
        }

        oTimeLine::Priority mp = oTimeLine::PLow;
        if (place <= 2)
          mp = oTimeLine::PTop;
        else if (place <= 5)
          mp = oTimeLine::PHigh;
        else if (place <= 10)
          mp = oTimeLine::PMedium;

        oTimeLine tl(radio[k].time, oTimeLine::TLTRadio, mp, r.getClassId(true),  rc[j].first, &r);
        TimeLineIterator tlit = timeLineEvents.insert(pair<int, oTimeLine>(tl.getTime(), tl));
        tlit->second.setMessage(msg).setDetail(detail);
      }
    }
  }

  TempResultMap results;
  sort(started.begin(), started.end(), compareFinishTime);

  wstring location, locverb, thelocation;
  if (finish) {
    location = L"i mål";
    locverb = L"går i mål";
    thelocation = lang.tl(L"målet") + L"#";
  }
  else {
    location = L"vid växeln";
    locverb = L"växlar";
    thelocation = lang.tl(L"växeln") + L"#";
  }

  vector<TimeRunner> &expectedFinish = expectedAtNext.back();
    // Sort according to pass time
  sort(expectedFinish.begin(), expectedFinish.end());
  size_t expIndex = 0;


  for (size_t k = 0; k<started.size(); k++) {
    oRunner &r = *started[k];
    if (r.getTotalStatus() == StatusOK) {

      while (expIndex < expectedFinish.size() && expectedFinish[expIndex].time < r.FinishTime) {
        TimeRunner &tr = expectedFinish[expIndex];
        int prelT = tr.runner->getTotalRunningTime(tr.time + pwTime, true, true);
        timeLinePrognose(results, tr, prelT, 1, thelocation, 0);
        expIndex++;
      }

      int place = 1;
      bool sharedPlace = false;
      vector<pRunner> preRunners;
      int time = r.getTotalRunningTime(r.FinishTime, true, true);

      insertResult(results, r, time, place, sharedPlace, preRunners);

      int leaderTime = results.begin()->first;
      int timeAfter = time - leaderTime;
      int deltaTime = timeAfter - r.tInitialTimeAfter;

      wstring timeS = formatTime(time);

      wstring msg, detail;
      getTimeAfterDetail(detail, timeAfter, deltaTime, r.tInitialTimeAfter > 0);

      // Transfer time after to next runner
      if (r.tInTeam) {
        pRunner next = r.tInTeam->getRunner(r.tLeg + 1);
        if (next) {
          next->tTimeAfter = timeAfter;
          next->tInitialTimeAfter = timeAfter;
        }
      }

      if (place == 1) {
        if (results.size() == 1)
          msg = L"är först " + location + L" med tiden X.#" + timeS;
        else if (!sharedPlace)
          msg = L"tar ledningen med tiden X.#" + timeS;
        else
          msg = L"går upp i delad ledning med tiden X.#" + timeS;
      }
      else {
        if (!sharedPlace) {
          if (preRunners.size() == 1 && place < 10)
            msg = locverb + L" på X plats, efter Y, på tiden Z.#" +
                        getOrder(place) +
                        L"#" + preRunners[0]->getCompleteIdentification(oRunner::IDType::ParallelLegExtra) +
                        L"#" + timeS;
          else
             msg = locverb + L" på X plats med tiden Y.#" +
                        getOrder(place) + L"#" + timeS;

        }
        else {
          msg = locverb + L" på delad X plats med tiden Y.#" + getOrder(place) +
                                                       L"#" + timeS;
        }
      }

      oTimeLine::Priority mp = oTimeLine::PLow;
      if (place <= 5)
        mp = oTimeLine::PTop;
      else if (place <= 10)
        mp = oTimeLine::PHigh;
      else if (place <= 20)
        mp = oTimeLine::PMedium;

      oTimeLine tl(r.FinishTime, oTimeLine::TLTFinish, mp, r.getClassId(true), r.getId(), &r);
      TimeLineIterator tlit = timeLineEvents.insert(pair<int, oTimeLine>(tl.getTime(), tl));
      tlit->second.setMessage(msg).setDetail(detail);
    }
    else if (r.getStatus() != StatusUnknown && r.getStatus() != StatusOK) {
      int t = r.FinishTime;
      if ( t == 0)
        t = r.tStartTime;

      int place = 1000;
      if (r.FinishTime > 0) {
        place = results.size() + 1;
        int rt = r.getTotalRunningTime(r.FinishTime, true,  true);
        if (rt > 0) {
          TempResultMap::iterator place_it = results.lower_bound(rt);
          place = place_it != results.end() ? place_it->second.place : results.size() + 1;
        }
      }
      oTimeLine::Priority mp = r.getSpeakerPriority() > 0 ? oTimeLine::PMedium : oTimeLine::PLow;
      if (place <= 3)
        mp = oTimeLine::PTop;
      else if (place <= 10)
        mp = oTimeLine::PHigh;
      else if (place <= 20)
        mp = oTimeLine::PMedium;

      oTimeLine tl(r.FinishTime, oTimeLine::TLTFinish, mp, r.getClassId(true), r.getId(), &r);
      TimeLineIterator tlit = timeLineEvents.insert(pair<int, oTimeLine>(t, tl));
      wstring msg;
      if (r.getStatus() != StatusDQ)
        msg = L"är inte godkänd.";
      else
        msg = L"är diskvalificerad.";

      tlit->second.setMessage(msg);
    }
  }
  return nextKnownEvent;
}


void oEvent::renderTimeLineEvents(gdioutput &gdi) const
{
  gdi.fillDown();
  for (TimeLineMap::const_iterator it = timeLineEvents.begin(); it != timeLineEvents.end(); ++it) {
    int t = it->second.getTime();
    wstring msg = t>0 ? getAbsTime(t) : makeDash(L"--:--:--");
    oRunner *r = it->second.getSource(*this);
    if (r) {
      pClass cls = r->getClassRef(true);
      if (cls) {
        auto type = cls->getClassType();
        if (type == ClassType::oClassRelay || type == ClassType::oClassIndividRelay)
          msg += L" (" + cls->getName() + L"/" + cls->getLegNumber(r->getLegNumber()) + L") ";
        else
          msg += L" (" + cls->getName() + L") ";
      }
      msg += r->getName() + L", " + r->getClub();
    }
    msg += L", " + lang.tl(it->second.getMessage());
    gdi.addStringUT(0, msg);
  }
}

int oEvent::getTimeLineEvents(const set<int> &classes, vector<oTimeLine> &events,
                              set<__int64> &stored, int currentTime) {
  if (currentTime == 0) {
    updateComputerTime(false);
    currentTime = getComputerTime();
  }
  //OutputDebugString(("GetTimeLine at: " + getAbsTime(getComputerTime()) + "\n").c_str());

  const int timeWindowSize = 10*60;
  int eval = nextTimeLineEvent <= getComputerTime() + 1;
  for (set<int>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
    if (timelineClasses.count(*it) == 0) {
      timelineClasses.insert(*it);
      eval = true;
    }
    if (modifiedClasses.count(*it) != 0)
      eval = true;
  }
  if (eval) {
    //OutputDebugStringA("SetupTimeLine\n");
    nextTimeLineEvent = setupTimeLineEvents(currentTime);
  }
//  else
//    OutputDebugString("No change\n");

  int time = 0;
  for (int k = events.size()-1; k>=0; k--) {
    int t = events[k].getTime();
    if (t > 0) {
      time = t;
      break;
    }
  }

  TimeLineMap::const_iterator it_start = timeLineEvents.lower_bound(time - timeWindowSize);

  bool filter = !events.empty();

  if (filter) {
    for (TimeLineMap::const_iterator it = it_start; it != timeLineEvents.end(); ++it) {


    }
  }

  for (TimeLineMap::const_iterator it = it_start; it != timeLineEvents.end(); ++it) {
    if (it->first <= time && it->second.getType() == oTimeLine::TLTExpected)
      continue; // Never get old expectations
    if (classes.count(it->second.getClassId()) == 0)
      continue;

    __int64 tag = it->second.getTag();
    if (stored.count(tag) == 0) {
      events.push_back(it->second);
      stored.insert(tag);
    }
  }

  return nextTimeLineEvent;
}

void oEvent::classChanged(pClass cls, bool punchOnly) {
  if (timelineClasses.count(cls->getId()) == 1) {
    modifiedClasses.insert(cls->getId());
    timeLineEvents.clear();
  }
}

static bool orderByTime(const oEvent::ResultEvent &a, const oEvent::ResultEvent &b) {
  if (a.classId() != b.classId())
    return a.classId() < b.classId(); // Sort first by class.
  return a.time < b.time;
}

struct LegSetupInfo {
  int baseLeg;
  int parCount;
  bool optional;

  LegSetupInfo():baseLeg(-1), parCount(0), optional(false) {}
  LegSetupInfo(int bl, bool opt):baseLeg(bl), parCount(0), optional(opt) {}

};

struct TeamLegControl {
  int teamId;
  int leg;
  int control;

  TeamLegControl() : teamId(0), leg(0), control(0) {}
  
  TeamLegControl(int id, int leg, int ctrl) : teamId(id), leg(leg), control(ctrl) {}

  bool operator<(const TeamLegControl &tlc) const {
    if (teamId != tlc.teamId)
      return teamId < tlc.teamId;
    else if (leg != tlc.leg)
      return leg < tlc.leg;
    else
      return control < tlc.control;
  }
};

void oEvent::getResultEvents(const set<int> &classFilter, const set<int> &punchFilter, vector<ResultEvent> &results) const {
  results.clear();

  vector<RunnerStatus> teamLegStatusOK;
  teamLegStatusOK.reserve(Teams.size() * 5);
  map<int, int> teamStatusPos;
  for (oTeamList::const_iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (!classFilter.count(it->getClassId(false)))
      continue;

    int base = teamLegStatusOK.size();
    teamStatusPos[it->getId()] = base;
    int nr = it->getNumRunners();
    bool ok = it->getStatus() == StatusOK || it->getStatus() == StatusUnknown;
    for(int k = 0; k < nr; k++) {
      pRunner r = it->getRunner(k);
      if (r && r->getStatus() != StatusUnknown && r->getStatus() != StatusOK)
        ok = false;

      teamLegStatusOK.push_back(ok ? StatusOK : StatusUnknown);
    }
    if (!ok) { // A more careful analysis
      for(int k = 0; k < nr; k++) {
        teamLegStatusOK[base + k] = it->getLegStatus(k, true, true);
      }
    }
  }

  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    const oRunner &r = *it;
    if (r.isRemoved() || !classFilter.count(r.getClassId(true)))
      continue;

    if (r.getStatusComputed(true) == StatusOutOfCompetition || r.getStatusComputed(true) == StatusNoTiming)
      continue;

    bool wroteResult = false;
    if (r.prelStatusOK(true, false, true) || r.getStatusComputed(true) != StatusUnknown) {
      RunnerStatus stat = r.prelStatusOK(true, false, true) ? StatusOK : r.getStatusComputed(true);
      wroteResult = true;
      results.push_back(ResultEvent(pRunner(&r), r.getFinishTime(), oPunch::PunchFinish, stat));
    }
    pCard card = r.getCard();
    
    RunnerStatus punchStatus = StatusOK;
    if (r.tInTeam && r.tLeg > 0) {
      map<int, int>::iterator res = teamStatusPos.find(r.tInTeam->getId());
      if (res != teamStatusPos.end()) {
        RunnerStatus prevStat = teamLegStatusOK[res->second + r.tLeg - 1];
        if (prevStat != StatusOK && prevStat != StatusUnknown) {
          if (wroteResult)
            results.back().status = StatusNotCompeting;
          punchStatus = StatusNotCompeting;
        }
      }
    }

    if (card) {
      oPunchList::const_iterator it;
      map<int,int> dupCount;
      for (it = card->punches.begin(); it != card->punches.end(); ++it) {
        if  (punchFilter.count(it->tMatchControlId)) {
          int dupC = ++dupCount[it->tMatchControlId];
          int courseControlId = oControl::getCourseControlIdFromIdIndex(it->tMatchControlId, dupC-1);
          results.push_back(ResultEvent(pRunner(&r), it->getAdjustedTime(), courseControlId, punchStatus));
        }
      }
    }
  }

  for (oFreePunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    const oFreePunch &fp = *it;
    if (fp.isRemoved() || fp.tRunnerId == 0 || fp.type == oPunch::PunchCheck || fp.type == oPunch::PunchStart || fp.type == oPunch::HiredCard)
      continue;

    pRunner r = getRunner(fp.tRunnerId, 0);
    if (r == 0 || !classFilter.count(r->getClassId(true)) || r->getCard())
      continue;

    int courseControlId = oFreePunch::getControlIdFromHash(fp.iHashType, true);
    int ctrl = oControl::getIdIndexFromCourseControlId(courseControlId).first;

    if (!punchFilter.count(ctrl))
      continue;

    results.push_back(ResultEvent(r, fp.getTimeInt(), courseControlId, StatusOK));

    if (r->tInTeam && r->tLeg > 0) {
      map<int, int>::iterator res = teamStatusPos.find(r->tInTeam->getId());
      if (res != teamStatusPos.end()) {
        RunnerStatus prevStat = teamLegStatusOK[res->second + r->tLeg - 1];
        if (prevStat != StatusOK && prevStat != StatusUnknown) {
          results.back().status = StatusNotCompeting; 
        }
      }
    }
  }

  for (map<pair<int,int>, oFreePunch>::const_iterator it = advanceInformationPunches.begin(); 
                                                      it != advanceInformationPunches.end(); ++it) {
    const oFreePunch &fp = it->second;
    if (fp.isRemoved() || fp.tRunnerId == 0 || fp.type == oPunch::PunchCheck || fp.type == oPunch::PunchStart)
      continue;
    pRunner r = getRunner(fp.tRunnerId, 0);
    if (r == 0 || !classFilter.count(r->getClassId(true)))
      continue;
    int courseControlId = oFreePunch::getControlIdFromHash(fp.iHashType, true);
    int ctrl = oControl::getIdIndexFromCourseControlId(courseControlId).first;
    if (!punchFilter.count(ctrl))
      continue;

    results.push_back(ResultEvent(r, fp.getTimeInt(), courseControlId, StatusOK));

    if (r->tInTeam && r->tLeg > 0) {
      map<int, int>::iterator res = teamStatusPos.find(r->tInTeam->getId());
      if (res != teamStatusPos.end()) {
        RunnerStatus prevStat = teamLegStatusOK[res->second + r->tLeg - 1];
        if (prevStat != StatusOK && prevStat != StatusUnknown) {
          results.back().status = StatusNotCompeting; 
        }
      }
    }
  }

  map< int, vector<LegSetupInfo> > parLegSetup;
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (!classFilter.count(it->getId()))
      continue;

    int ns = it->getNumStages();
    if (ns == 1)
      continue;
    
    int baseLeg = 0;
    for (int i = 0; i < ns; i++) {
      bool optional = it->legInfo[i].isOptional();
      bool par = it->legInfo[i].isParallel(); 
      if (optional || par) {
        vector<LegSetupInfo> &ls = parLegSetup[it->getId()];
        ls.resize(ns, LegSetupInfo());
        ls[i] = LegSetupInfo(baseLeg, optional);
        if (i > 0)
          ls[i-1] = ls[i];
      }
      else {
        baseLeg = i;
      }
    }
  }

  if (parLegSetup.empty()) 
    return;

  for (map< int, vector<LegSetupInfo> >::iterator it = parLegSetup.begin();
       it != parLegSetup.end(); ++it) {
    vector<LegSetupInfo> &setup = it->second;
    size_t sx = -1;
    for (size_t k = 0; k < setup.size(); k++) {
      if (setup[k].baseLeg == -1) {
        if (sx != -1) {
          int count = k - sx;
          while (sx < k) {
            setup[sx++].parCount = count;
          }
        }
        sx = -1;
      }
      else if (sx == -1) {
        sx = k;
      }
    }

    if (sx != -1) {
      int count = setup.size() - sx;
      while (sx < setup.size()) {
        setup[sx++].parCount = count;
      }
    }
  }

  sort(results.begin(), results.end(), orderByTime);
  map<TeamLegControl, int> countTeamLeg;

  for (size_t k = 0; k < results.size(); k++) {
    int clsId = results[k].classId();
    map< int, vector<LegSetupInfo> >::iterator res = parLegSetup.find(clsId);
    if (res == parLegSetup.end())
      continue;
    const vector<LegSetupInfo> &setup = res->second;
    int leg = results[k].r->getLegNumber();
    if (setup[leg].baseLeg == -1) {
      results[k].legNumber = leg;
      continue;
    }
    if (results[k].status != StatusOK)
      continue;
    results[k].legNumber = setup[leg].baseLeg;
    int teamId = results[k].r->getTeam()->getId();
    int val = ++countTeamLeg[TeamLegControl(teamId, setup[leg].baseLeg, results[k].control)];

    if (setup[leg].optional) {
      results[k].partialCount = val - 1;
    }
    else {
      const int numpar = setup[leg].parCount; // XXX Count legs
      results[k].partialCount = numpar - val;
    }
  }
}
