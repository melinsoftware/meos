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

#include "stdafx.h"

#include <vector>
#include <io.h>
#include <algorithm>

#include "oEvent.h"
#include "oSpeaker.h"
#include "gdioutput.h"

#include "SportIdent.h"
#include "mmsystem.h"
#include "meos_util.h"
#include "localizer.h"
#include "meos.h"
#include "gdifonts.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern gdioutput *gdi_main;
extern oEvent* gEvent;
constexpr int constNoLeaderTime = 10000000;
constexpr int highlightNewResultTime = 20;

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

//Order by preliminary times and priotity for speaker list
bool CompareSpkSPList(const oSpeakerObject &a, const oSpeakerObject &b)
{
  if (a.priority!=b.priority)
    return a.priority>b.priority;
  else if (a.status<=1 && b.status<=1){
    int at=a.runningTime.preliminary;
    int bt=b.runningTime.preliminary;

    if (at==bt) { //Compare leg times instead
      at=a.runningTimeLeg.preliminary;
      bt=b.runningTimeLeg.preliminary;
    }

    if (at == bt) {
      if (a.runningTimeSinceLast.preliminary > 0 && b.runningTimeSinceLast.preliminary > 0) {
        if (a.runningTimeSinceLast.preliminary != b.runningTimeSinceLast.preliminary)
          return a.runningTimeSinceLast.preliminary > b.runningTimeSinceLast.preliminary;
      }
      else if (a.runningTimeSinceLast.preliminary>0)
        return true;
      else if (b.runningTimeSinceLast.preliminary>0)
        return false;
    }

    if (a.missingStartTime != b.missingStartTime)
      return a.missingStartTime < b.missingStartTime;

    if (at==bt) {
      if (a.parallelScore != b.parallelScore)
        return a.parallelScore > b.parallelScore;

      if (a.bib != b.bib) {
        return compareBib(a.bib, b.bib);
      }
      return a.names<b.names;
    }
    else if (at>=0 && bt>=0) {
      if (a.priority == 0)
        return bt<at;
      else
        return at<bt;
    }
    else if (at>=0 && bt<0)
      return true;
    else if (at<0 && bt>=0)
      return false;
    else return at>bt;
  }
  else if (a.status!=b.status)
    return a.status<b.status;

  if (a.parallelScore != b.parallelScore)
    return a.parallelScore > b.parallelScore;

  if (a.bib != b.bib) {
    return compareBib(a.bib, b.bib);
  }

  return a.names<b.names;
}

//Order by known time for calculating results
bool CompareSOResult(const oSpeakerObject &a, const oSpeakerObject &b)
{
  if (a.status!=b.status){
    if (a.status==StatusOK)
      return true;
    else if (b.status==StatusOK)
      return false;
    else return a.status<b.status;
  }
  else if (a.status==StatusOK){
    int at=a.runningTime.time;
    int bt=b.runningTime.time;
    if (at!=bt)
      return at<bt;

    at=a.runningTimeLeg.time;
    bt=b.runningTimeLeg.time;
    if (at!=bt)
      return at<bt;

    return a.names<b.names;
  }
  return a.names<b.names;
}

int SpeakerCB (gdioutput *gdi, GuiEventType type, BaseInfo* data) {
  oEvent *oe=gEvent;
  if (!oe)
    return false;

  DWORD classId=0, controlId=0, previousControlId, leg=0, totalResult=0, shortNames = 0;
  gdi->getData("ClassId", classId);
  gdi->getData("ControlId", controlId);
  gdi->getData("PreviousControlId", previousControlId);
  gdi->getData("LegNo", leg);
  gdi->getData("TotalResult", totalResult);
  gdi->getData("ShortNames", shortNames);

  if (classId>0 && controlId>0) {
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
            start = stages[k-1].first + 1;
          break;
        }
      }
      int cid = oControl::getIdIndexFromCourseControlId(controlId).first;
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

    if (update)
      oe->speakerList(*gdi, classId, leg, controlId, previousControlId,
                      totalResult != 0, shortNames != 0);
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

      DWORD classId=0, controlId=0, previousControlId = 0;
      if (gdi->getData("ClassId", classId) && gdi->getData("ControlId", controlId) && gdi->getData("PreviousControlId", previousControlId)){
        DWORD leg = 0, totalResult = 0, shortNames = 0;
        gdi->getData("LegNo", leg);
        gdi->getData("TotalResult", totalResult);
        gdi->getData("ShortNames", shortNames);

        if (ti->id[0]=='D'){
          r->setPriority(controlId, -1);
        }
        else if (ti->id[0]=='U'){
          r->setPriority(controlId, 1);
        }
        else if (ti->id[0]=='M'){
          r->setPriority(controlId, 0);
        }
        oe->speakerList(*gdi, classId, leg, controlId, previousControlId, totalResult != 0, shortNames != 0);
      }
    }
  }
  return true;
}

void renderRowSpeakerList(const oSpeakerObject& r, const oSpeakerObject* next_r,
  int leaderTime, int type, vector<SpeakerString>& row,
  bool shortName, bool useSubSecond) {

  if (r.place > 0)
    row.push_back(SpeakerString(textRight, itow(r.place)));
  else
    row.push_back(SpeakerString());

  wstring names;
  for (size_t k = 0; k < r.names.size(); k++) {
    if (names.empty()) {
      if (!r.bib.empty())
        names += r.bib + L": ";
    }
    else {
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

  if (r.runnersFinishedLeg < r.runnersTotalLeg && r.runnersFinishedLeg>0) {
    // Fraction of runners has finished on leg
    names += L" (" + itow(r.runnersFinishedLeg) + L"/" + itow(r.runnersTotalLeg) + L")";
  }

  for (size_t k = 0; k < r.outgoingnames.size(); k++) {
    if (k == 0) {
      names += L" > ";
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

  if (r.finishStatus <= 1 || r.finishStatus == r.status)
    row.push_back(SpeakerString(normalText, names));
  else
    row.push_back(SpeakerString(normalText, names + L" (" + oEvent::formatStatus(r.finishStatus, true) + L")"));

  row.push_back(SpeakerString(normalText, r.club));
  auto ssMode = useSubSecond ? SubSecond::On : SubSecond::Auto;

  if (r.status == StatusOK || (r.status == StatusUnknown && r.runningTime.time > 0)) {
    row.push_back(SpeakerString(textRight, formatTime(r.runningTime.preliminary, ssMode)));

    if (r.runningTime.time != r.runningTimeLeg.time)
      row.push_back(SpeakerString(textRight, formatTime(r.runningTimeLeg.time, ssMode)));
    else
      row.push_back(SpeakerString());

    if (leaderTime != constNoLeaderTime && leaderTime > 0) {
      int flag = timerCanBeNegative;
      if (useSubSecond)
        flag |= timeWithTenth;

      row.push_back(SpeakerString(textRight, gdioutput::getTimerText(r.runningTime.time - leaderTime,
        flag, false, L"")));
    }
    else
      row.push_back(SpeakerString());
    /*
    if (r.timeSinceChange < 10 && r.timeSinceChange>=0) {
      RECT rc;
      rc.left=x+dx[1]-4;
      rc.right=x+dx[7]+gdi.scaleLength(60);
      rc.top=y-1;
      rc.bottom=y+lh+1;
      gdi.addRectangle(rc, colorLightGreen, false);
    }*/
  }
  else if (r.status == StatusUnknown) {
    DWORD timeOut = NOTIMEOUT;

    if (r.runningTimeLeg.preliminary > 0 && !r.missingStartTime) {

      if (next_r && next_r->status == StatusOK && next_r->runningTime.preliminary > r.runningTime.preliminary)
        timeOut = next_r->runningTime.preliminary / timeConstSecond;

      row.push_back(SpeakerString(textRight, r.runningTime.preliminary, timeOut));

      if (r.runningTime.preliminary != r.runningTimeLeg.preliminary)
        row.push_back(SpeakerString(textRight, r.runningTimeLeg.preliminary));
      else
        row.push_back(SpeakerString());

      if (leaderTime != constNoLeaderTime)
        row.push_back(SpeakerString(timerCanBeNegative | textRight, r.runningTime.preliminary - leaderTime));
      else
        row.push_back(SpeakerString());
    }
    else {
      row.push_back(SpeakerString(textRight, L"[" + r.startTimeS + L"]"));

      if (!r.missingStartTime && r.runningTimeLeg.preliminary < 0) {
        row.push_back(SpeakerString(timerCanBeNegative | textRight,
          r.runningTimeLeg.preliminary, 0)); // Timeout on start
      }
      else
        row.push_back(SpeakerString());

      row.push_back(SpeakerString());
    }
  }
  else {
    row.push_back(SpeakerString());
    row.push_back(SpeakerString(textRight, oEvent::formatStatus(r.status, true)));
    row.back().color = colorDarkRed;
    row.push_back(SpeakerString());
  }

  int ownerId = r.owner ? r.owner->getId() : 0;
  if (type == 1) {
    row.push_back(SpeakerString(normalText, lang.tl(L"[Bort]")));
    row.back().color = colorRed;
    row.back().moveKey = "D" + itos(ownerId);
  }
  else if (type == 2) {
    row.push_back(SpeakerString(normalText, lang.tl(L"[Bevaka]")));
    row.back().color = colorGreen;
    row.back().moveKey = "U" + itos(ownerId);

    row.push_back(SpeakerString(normalText, lang.tl(L"[Bort]")));
    row.back().color = colorRed;
    row.back().moveKey = "D" + itos(ownerId);
  }
  else if (type == 3) {
    if (r.status <= StatusOK) {
      if (r.priority < 0) {
        row.push_back(SpeakerString(normalText, lang.tl(L"[Återställ]")));
        row.back().moveKey = "M" + itos(ownerId);
      }
      else {
        row.push_back(SpeakerString(normalText, lang.tl(L"[Bevaka]")));
        row.back().moveKey = "U" + itos(ownerId);
      }
    }
  }
}

void renderRowSpeakerList(gdioutput &gdi, int type, const oSpeakerObject &r, int x, int y,
                          const vector<SpeakerString> &row, const vector<int> &pos) {
  int lh=gdi.getLineHeight();
  bool highlight = false;
  if (r.timeSinceChange < highlightNewResultTime * timeConstSecond && r.timeSinceChange>=0) {
    RECT rc;
    rc.left = x+pos[1] - 4;
    rc.right=x+pos.back()+gdi.scaleLength(60);
    rc.top=y-1;
    rc.bottom=y+lh+1;
    gdi.addRectangle(rc, colorLightGreen, false);
    highlight = true;
    gdi.addTimeout(highlightNewResultTime + 5, SpeakerCB);
  }

  for (size_t k = 0; k < row.size(); k++) {
    int limit = pos[k+1]-pos[k]-3;
    if (limit < 0)
      limit = 0;
    if (!row[k].hasTimer) {
      if (!row[k].str.empty()) {
        if (row[k].moveKey.empty()) {

          TextInfo &ti = gdi.addStringUT(y, x+pos[k], row[k].format, row[k].str, limit);
          if (row[k].color != colorDefault)
            ti.setColor(row[k].color);
        }
        else {
          TextInfo &ti = gdi.addStringUT(y, x+pos[k], row[k].format, row[k].str, limit, MovePriorityCB);
          ti.id = row[k].moveKey;
          ti.setExtra(r.owner->getId());
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
}

void oEvent::calculateResults(list<oSpeakerObject> &rl)
{
  rl.sort(CompareSOResult);
  list<oSpeakerObject>::iterator it;

  int cPlace=0;
  int vPlace=0;
  int cTime=0;

  for (it=rl.begin(); it != rl.end(); ++it) {
    if (it->status==StatusOK) {
      cPlace++;
      int rt = it->runningTime.time;
      if (rt>cTime)
        vPlace=cPlace;
      cTime=rt;
      it->place=vPlace;
    }
    else
      it->place = 0;
  }
}

void oEvent::speakerList(gdioutput &gdi, int ClassId, int leg, int ControlId,
                         int PreviousControlId, bool totalResults, bool shortNames) {
#ifdef _DEBUG
  OutputDebugString(L"SpeakerListUpdate\n");
#endif

  DWORD clsIds = 0, ctrlIds = 0, cLegs = 0, cTotal = 0, cShort = 0;
  gdi.getData("ClassId", clsIds);
  gdi.getData("ControlId", ctrlIds);
  gdi.getData("LegNo", cLegs);
  gdi.getData("TotalResult", cTotal);
  gdi.getData("ShortNames", cShort);

  bool refresh = clsIds == ClassId &&  ctrlIds == ControlId && leg == cLegs &&
                           (totalResults ? 1 : 0) == cTotal && (shortNames ? 1 : 0) == cShort;

  if (refresh)
    gdi.takeShownStringsSnapshot();

  int storedY = gdi.getOffsetY();
  int storedHeight = gdi.getHeight();

  gdi.restoreNoUpdate("SpeakerList");
  gdi.setRestorePoint("SpeakerList");

  gdi.pushX(); gdi.pushY();
  gdi.updatePos(0,0,0, storedHeight);
  gdi.popX(); gdi.popY();
  gdi.setOffsetY(storedY);

  gdi.setData("ClassId", ClassId);
  gdi.setData("ControlId", ControlId);
  gdi.setData("PreviousControlId", PreviousControlId);
  gdi.setData("LegNo", leg);
  gdi.setData("TotalResult", totalResults ? 1 : 0);
  gdi.setData("ShortNames", shortNames ? 1 : 0);

  gdi.registerEvent("DataUpdate", SpeakerCB);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);

  list<oSpeakerObject> speakerList;

  //For preliminary times
  updateComputerTime(false);

  speakerList.clear();
  if (classHasTeams(ClassId)) {
    oTeamList::iterator it=Teams.begin();
    while(it!=Teams.end()){
      if (it->getClassId(true)==ClassId && !it->skip()) {
        oSpeakerObject so;
        it->fillSpeakerObject(leg, ControlId, PreviousControlId, totalResults, so);
        if (so.owner)
          speakerList.push_back(so);
      }
      ++it;
    }
  }
  else {
    pClass pc = getClass(ClassId);
    bool qfBaseClass = pc && pc->getQualificationFinal();
    bool qfFinalClass = pc && pc->getParentClass();

    if (qfBaseClass || qfFinalClass)
      leg = 0;

    for(auto &it : Runners){
      if (it.getClassId(true) == ClassId && !it.isRemoved()) {
        if (qfBaseClass && it.getLegNumber() > 0)
          continue;

        oSpeakerObject so;
        it.fillSpeakerObject(leg, ControlId, PreviousControlId,  totalResults, so);
        if (so.owner)
          speakerList.push_back(so);
      }
    }
  }
  if (speakerList.empty()) {
    gdi.dropLine();
    gdi.addString("", fontMediumPlus, "Inga deltagare");
    gdi.refresh();
    return;
  }

  list<oSpeakerObject>::iterator sit;
  for (sit=speakerList.begin(); sit != speakerList.end(); ++sit) {
    if (sit->hasResult() && sit->priority>=0)
      sit->priority=1;
    else if (sit->status > StatusOK  && sit->priority<=0)
      sit->priority=-1;
  }

  //Calculate place...
  calculateResults(speakerList);

  //Calculate preliminary times and sort by this and prio.
  speakerList.sort(CompareSpkSPList);

  //char bf[256];
  pClass pCls=oe->getClass(ClassId);

  if (!pCls)
    return;

  vector<oClass::TrueLegInfo > stages;
  pCls->getTrueStages(stages);

  pCourse crs = deduceSampleCourse(pCls, stages, leg);
  
  //char bf2[64]="";
  wstring legName, cname = pCls->getName();
  size_t istage = -1;
  bool useSS = useSubSecond(); // Only for finish time??
  for (size_t k = 0; k < stages.size(); k++) {
    if (stages[k].first >= leg) {
      istage = k;
      break;
    }
  }
  if (stages.size()>1 && istage < stages.size())
    cname += lang.tl(L", Sträcka X#" + itow(stages[istage].second));

  if (ControlId != oPunch::PunchFinish  && crs) {
    cname += L", " + crs->getRadioName(ControlId);
  }
  else {
    cname += lang.tl(L", Mål");
  }

  int y=gdi.getCY()+5;
  int x=30;

  gdi.addStringUT(y, x, fontMediumPlus, cname).setColor(colorGreyBlue);
  int lh=gdi.getLineHeight();

  y+=lh*2;
  int leaderTime = constNoLeaderTime;

  //Calculate leader-time
  for (sit = speakerList.begin(); sit != speakerList.end(); ++sit) {
    int rt = sit->runningTime.time;
    if ((sit->status == StatusOK || sit->status == StatusUnknown) && rt > 0)
      leaderTime = min(leaderTime, rt);
  }

  vector< pair<oSpeakerObject *, vector<SpeakerString> > > toRender(speakerList.size());

  int ix = 0;
  sit = speakerList.begin();
  size_t maxRow = 0;
  while (sit != speakerList.end()) {
    oSpeakerObject *so = toRender[ix].first = &*sit;
    ++sit;
    oSpeakerObject *next = sit != speakerList.end() ? &*sit : 0;
    int type = 3;
    if (so->priority > 0 || (so->hasResult() && so->priority>=0))
      type = 1;
    else if (so->status == StatusUnknown && so->priority==0)
      type = 2;

    renderRowSpeakerList(*toRender[ix].first, next, leaderTime, 
                          type, toRender[ix].second, shortNames, useSS);
    maxRow = max(maxRow, toRender[ix].second.size());
    ix++;
  }

  vector<int> dx_new(maxRow);
  int s60 = gdi.scaleLength(60);
  int s28 = gdi.scaleLength(35);
  int s7 = gdi.scaleLength(7);
  TextInfo ti;
  HDC hDC = GetDC(gdi.getHWNDTarget());
  for (size_t k = 0; k < toRender.size(); k++) {
    const vector<SpeakerString> &row = toRender[k].second;
    for (size_t j = 0; j < row.size(); j++) {
      if (!row[j].str.empty()) {
        ti.xp = 0;
        ti.yp = 0;
        ti.format = 0;
        ti.text = row[j].str;
        gdi.calcStringSize(ti, hDC);
        dx_new[j] = max(s28, max<int>(dx_new[j], ti.textRect.right+s7));
        /*double len = row[j].str.length();
        double factor = 5.6;
        if (len < 4)
          factor = 7;
        else if (len <10)
          factor = 5.8;
        dx_new[j] = max(28, max<int>(dx_new[j], int(len * factor)+15));*/
      }
      else if (row[j].hasTimer) {
        dx_new[j] = max<int>(dx_new[j], s60);
      }
    }
  }
  ReleaseDC(gdi.getHWNDTarget(), hDC);
  bool rendered;
  int limit = gdi.scaleLength(280);
  vector<int> dx(maxRow+1);
  for (size_t k = 1; k < dx.size(); k++) {
    dx[k] = dx[k-1] + min(limit, dx_new[k-1] + gdi.scaleLength(4));
  }

  rendered = false;
  for (size_t k = 0; k < toRender.size(); k++) {
    oSpeakerObject *so = toRender[k].first;
    if (so && (so->priority > 0 || (so->hasResult() && so->priority>=0))) {
      if (rendered == false) {
        gdi.addString("", y, x, boldSmall, "Resultat");
        y+=lh+5, rendered=true;
      }
      renderRowSpeakerList(gdi, 1, *so, x, y, toRender[k].second, dx);
      y+=lh;
      toRender[k].first = 0;
    }
  }

  rendered = false;
  for (size_t k = 0; k < toRender.size(); k++) {
    oSpeakerObject *so = toRender[k].first;
    if (so && so->isIncomming() && so->priority==0) {
      if (rendered == false) {
        gdi.addString("", y+4, x, boldSmall, "Inkommande");
        y+=lh+5, rendered=true;
      }
      renderRowSpeakerList(gdi, 2, *so, x, y, toRender[k].second, dx);
      y+=lh;
      toRender[k].first = 0;
    }
  }

  rendered = false;
  for (size_t k = 0; k < toRender.size(); k++) {
    oSpeakerObject *so = toRender[k].first;
    if (so) {
      if (rendered == false) {
        gdi.addString("", y+4, x, boldSmall, "Övriga");
        y+=lh+5, rendered=true;
      }
      renderRowSpeakerList(gdi, 3, *so, x, y, toRender[k].second, dx);
      y+=lh;
      toRender[k].first = 0;
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

void oEvent::clearPrewarningSounds()
{
  oFreePunchList::reverse_iterator it;
  for (it=punches.rbegin(); it!=punches.rend(); ++it)
    it->hasBeenPlayed=true;
}

void oEvent::tryPrewarningSounds(const wstring &basedir, int number)
{
  wchar_t wave[20];
  swprintf_s(wave, L"%d.wav", number);

  wstring file=basedir+L"\\"+wave;

  if (_waccess(file.c_str(), 0)==-1)
    gdibase.alert(L"Fel: hittar inte filen X.#" + file);

  PlaySound(file.c_str(), 0, SND_SYNC|SND_FILENAME );
}

void oEvent::playPrewarningSounds(const wstring &basedir, set<int> &controls)
{
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
    if (r.tStatus == StatusDNS || r.tStatus == StatusCANCEL || r.tStatus == StatusNotCompetiting)
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
          for (int j = 0; j < pc->nControls; j++) {
            int id = pc->Controls[j]->Id;
            if (controlId.count(id) > 0) {
              rc.push_back(make_pair(pc->getCourseControlId(j), pc->Controls[j]));
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
    OutputDebugStringA("SetupTimeLine\n");
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
            results.back().status = StatusNotCompetiting;
          punchStatus = StatusNotCompetiting;
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
          results.back().status = StatusNotCompetiting; 
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
          results.back().status = StatusNotCompetiting; 
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
