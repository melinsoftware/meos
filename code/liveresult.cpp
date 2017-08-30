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
#include "oEvent.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <cassert>
#include "Localizer.h"
#include <algorithm>
#include "gdifonts.h"
#include "meosexception.h"
#include "liveresult.h"

LiveResult::LiveResult(oEvent *oe) : oe(oe), active(false), lastTime(0), rToWatch(0) {
  baseFont = oe->getPropertyString("LiveResultFont", "Consolas");
  showResultList = -1;
  timerScale = 1.0;
}


string LiveResult::getFont(const gdioutput &gdi, double relScale) const {
  int h,w;
  gdi.getTargetDimension(w, h);
  if (!gdi.isFullScreen())
    w -=  gdi.scaleLength(160);

  double fact = min(h/180.0, w/300.0);

  double size = relScale * fact;
  char ss[32];
  sprintf_s(ss, "%f", size);
  string font = baseFont + ";" + ss;
  return font;
}

void LiveResult::showDefaultView(gdioutput &gdi) {
  int h,w;
  gdi.getTargetDimension(w, h);
  if (!gdi.isFullScreen())
    w -=  gdi.scaleLength(160);

  RECT rc;
  rc.top = h-24;
  rc.left = 20;
  rc.right = w - 30;
  rc.bottom = h - 22;
  
  string font = getFont(gdi, 1.0);
  gdi.addRectangle(rc, colorLightYellow, true);
  gdi.addString("timing", 50, w / 2, textCenter|boldHuge, "MeOS Timing", 0, 0, font.c_str());

  TextInfo &ti = gdi.addString("measure", 0, 0, boldHuge,  "55:55:55", 0, 0, font.c_str());
  int tw = ti.textRect.right - ti.textRect.left;
  timerScale = double(w) * 0.8 / double(tw); 
  gdi.removeString("measure");
}

void LiveResult::showTimer(gdioutput &gdi, const oListInfo &liIn) {
  li = liIn;
  active = true;
  
  int h,w;
  gdi.getTargetDimension(w, h);
  gdi.clearPage(false);
  showDefaultView(gdi);

  gdi.registerEvent("DataUpdate", 0).setHandler(this);
  gdi.setData("DataSync", 1);
  gdi.setData("PunchSync", 1);
  gdi.setRestorePoint("LiveResult");

  lastTime = 0;
  vector<const oFreePunch *> pp;
  oe->synchronizeList(oLRunnerId, true, false);
  oe->synchronizeList(oLPunchId, false, true);
  
  oe->getLatestPunches(lastTime, pp);
  processedPunches.clear();

  map<int, pair<vector<int>, vector<int> > > storedPunches;
  int fromPunch = li.getParam().useControlIdResultFrom;
  int toPunch = li.getParam().useControlIdResultTo;
  if (fromPunch == 0)
    fromPunch = oPunch::PunchStart;
  if (toPunch == 0)
    toPunch = oPunch::PunchFinish;
    
  for (size_t k = 0; k < pp.size(); k++) {
    lastTime = max(pp[k]->getModificationTime(), lastTime);
    pRunner r = pp[k]->getTiedRunner();
    if (r) {
      pair<int, int> key = make_pair(r->getId(), pp[k]->getControlId());
      processedPunches[key] = max(processedPunches[key], pp[k]->getAdjustedTime());
      
      if (!li.getParam().selection.empty() && !li.getParam().selection.count(r->getClassId()))
        continue; // Filter class

      if (pp[k]->getTypeCode() == fromPunch) {
        storedPunches[r->getId()].first.push_back(k);
      }
      else if (pp[k]->getTypeCode() == toPunch) {
        storedPunches[r->getId()].second.push_back(k);
      }
    }
  }
  startFinishTime.clear();
  results.clear();
  for (map<int, pair<vector<int>, vector<int> > >::iterator it = storedPunches.begin();
       it != storedPunches.end(); ++it) {
    vector<int> &froms = it->second.first;
    vector<int> &tos = it->second.second;
    pRunner r = oe->getRunner(it->first, 0);
    for (size_t j = 0; j < tos.size(); j++) {
      int fin = pp[tos[j]]->getAdjustedTime();
      int time = 100000000;
      int sta = 0;
      for (size_t k = 0; k < froms.size(); k++) {
        int t = fin - pp[froms[k]]->getAdjustedTime();
        if (t > 0 && t < time) {
          time = t;
          sta = pp[froms[k]]->getAdjustedTime();
        }
      }
      if (time < 100000000 && r->getStatus() <= StatusOK) {
//        results.push_back(Result());
//        results.back().r = r;
//        results.back().time = time;
        startFinishTime[r->getId()].first = sta;
        startFinishTime[r->getId()].second = fin;

      }
    }
  }

  resYPos = h/3;

  calculateResults();
  showResultList = 0;
  gdi.addTimeoutMilli(1000, "res", 0).setHandler(this);
  gdi.refreshFast();
}


void LiveResult::handle(gdioutput &gdi, BaseInfo &bu, GuiEventType type) {
  if (type == GUI_EVENT) {
    vector<const oFreePunch *> pp;
    oe->getLatestPunches(lastTime, pp);
    
    int fromPunch = li.getParam().useControlIdResultFrom;
    int toPunch = li.getParam().useControlIdResultTo;
    if (fromPunch == 0)
      fromPunch = oPunch::PunchStart;
    if (toPunch == 0)
      toPunch = oPunch::PunchFinish;
    
    bool hasCheckedOld = false;
    vector< pair<int, const oFreePunch*> > enter, exit, backExit;

    for (size_t k = 0; k < pp.size(); k++) {
      lastTime = max(pp[k]->getModificationTime(), lastTime);
      pRunner r = pp[k]->getTiedRunner();
      if (!r)
        continue;

      if (!li.getParam().selection.empty() && !li.getParam().selection.count(r->getClassId()))
        continue; // Filter class

      pair<int, int> key = make_pair(r->getId(), pp[k]->getControlId());
      
      bool accept = !processedPunches.count(key) || abs(processedPunches[key] - pp[k]->getAdjustedTime()) > 5;
      
      if (accept && !hasCheckedOld && pp[k]->getTypeCode() == fromPunch) {
        hasCheckedOld = true;
        while (rToWatch.size() > 1) {
          watchedR.push_back(rToWatch.front());
          rToWatch.erase(rToWatch.begin()); // TODO: Better algorithm for forgetting?
        }
      }

      processedPunches[key] = pp[k]->getAdjustedTime();

      if (accept) {
        if (pp[k]->getTypeCode() == fromPunch) {
          enter.push_back(make_pair(pp[k]->getAdjustedTime(), pp[k]));
        }
        else if (pp[k]->getTypeCode() == toPunch) {
          if (count(rToWatch.begin(), rToWatch.end(), r->getId()) > 0) {
            exit.push_back(make_pair(pp[k]->getAdjustedTime(), pp[k]));
          }
          else if (count(watchedR.begin(), watchedR.end(), r->getId()) > 0) {
            backExit.push_back(make_pair(pp[k]->getAdjustedTime(), pp[k]));
          }
        }
      }
    }
    
    sort(enter.begin(), enter.end());
    sort(exit.begin(), exit.end());

    int h,w;
    gdi.getTargetDimension(w, h);
    bool doRefresh = false;

    for (size_t k = 0; k < enter.size(); k++) {
      showResultList = -1;
      const oFreePunch *fp = enter[k].second;
      pRunner newRToWatch = fp->getTiedRunner();

      if (count(rToWatch.begin(), rToWatch.end(), newRToWatch->getId()))
        continue;

      rToWatch.push_back(newRToWatch->getId());
      gdi.restore("LiveResult", false);
      startFinishTime[newRToWatch->getId()].first = fp->getAdjustedTime();
      isDuel = false;
      if (rToWatch.size() == 1) {
        string font = getFont(gdi, timerScale);
        BaseInfo *bi = gdi.setText("timing", newRToWatch->getName(), false);
        dynamic_cast<TextInfo &>(*bi).changeFont(getFont(gdi, 0.7));
        gdi.addTimer(h/2, w/2, boldHuge|textCenter|timeWithTenth, 0, 0, 0, NOTIMEOUT, font.c_str());
        screenSize = 1;
      }
      else if (rToWatch.size() == 2) {
        string font = getFont(gdi, timerScale * 0.6);
  
        pRunner r0 = oe->getRunner(rToWatch[0], 0);
        pRunner r1 = oe->getRunner(rToWatch[1], 0);

        string n = (r0 ? r0->getName(): "-") + " / " + (r1 ? r1->getName() : "-");
        bool duel = r0 && r1 && fromPunch == oPunch::PunchStart && 
                          r0->getTeam() != 0 && 
                          r0->getTeam() == r1->getTeam();
        isDuel = duel;
        if (n.length() < 30) {
          BaseInfo *bi = gdi.setText("timing", n, false);
          TextInfo &ti = dynamic_cast<TextInfo &>(*bi);
          ti.changeFont(getFont(gdi, 0.5));
        }
        else {
          BaseInfo *bi = gdi.setText("timing", "", false);
          TextInfo &ti = dynamic_cast<TextInfo &>(*bi);
          string sfont = getFont(gdi, 0.5);
          TextInfo &ti2 = gdi.addString("n1", ti.yp, gdi.scaleLength(20), boldHuge, 
                                        "#" + (r0 ? r0->getName() : string("")), 0, 0, sfont.c_str());
          gdi.addString("n2", ti.yp + ti2.getHeight() + 4, gdi.getWidth(), boldHuge | textRight, 
                        "#" + (r1 ? r1->getName() : string("")), 0, 0, sfont.c_str());
        }
        int id1 = rToWatch[0];
        int id2 = rToWatch[1];

        int t1 = startFinishTime[id1].first;
        int diff = abs(fp->getAdjustedTime() - t1);
        runner2ScreenPos[id1] = 1;
        runner2ScreenPos[id2] = 2;
        screenSize = 2;
        int startTimeR2 = 0;
        if (duel) {
          // Ensure same start time
          int t2 = startFinishTime[id2].first;
          int st = min(t1,t2);
          startFinishTime[id1].first = st;
          startFinishTime[id2].first = st;

          for (size_t i = 0; i < rToWatch.size(); i++) {
            pRunner r = oe->getRunner(rToWatch[i], 0);
            if (r) {
              r->synchronize();
              r->setStartTime(st, true, false, true);
              r->synchronize(false);
            }
          }
          startTimeR2 = diff;
        }

        gdi.addTimer(h/2, w/2-w/4, boldHuge|textCenter|timeWithTenth, diff, 0, 0, NOTIMEOUT, font.c_str()).id = "timer1";
        gdi.addTimer(h/2, w/2+w/4, boldHuge|textCenter|timeWithTenth, startTimeR2, 0, 0, NOTIMEOUT, font.c_str()).id = "timer2";
      }

      doRefresh = true;
    }

    for (size_t k = 0; k < exit.size(); k++) {
      const oFreePunch *fp = exit[k].second;
      pRunner rToFinish = fp->getTiedRunner();

      if (count(rToWatch.begin(), rToWatch.end(), rToFinish->getId()) > 0) {
        showResultList = -1;
        pair<int,int> &se = startFinishTime[rToFinish->getId()];
        se.second = fp->getAdjustedTime();
        int rt = se.second - se.first;
        size_t ix = find(rToWatch.begin(), rToWatch.end(), rToFinish->getId()) - rToWatch.begin();
      
        if (screenSize == 1) {
          gdi.restore("LiveResult", false);
          string font = getFont(gdi, timerScale);
          gdi.addString("", h/2, w/2, boldHuge|textCenter, formatTime(rt), 0, 0, font.c_str()).setColor(colorGreen);
          gdi.addTimeout(5, 0).setHandler(this);
        }
        else if (screenSize == 2) {
          string id = "timer" + itos(runner2ScreenPos[rToFinish->getId()]);
          BaseInfo *bi = gdi.setText(id, formatTime(rt), false);
          string font = getFont(gdi, timerScale * 0.6);
  
          if (bi) {
            TextInfo &ti = dynamic_cast<TextInfo &>(*bi);
            ti.format = boldHuge|textCenter;
            ti.hasTimer = false;
            
           if (rToWatch.size() == 2 || !isDuel) 
              ti.setColor(colorGreen);
          }

          if (rToWatch.size() == 1) {
            gdi.addTimeout(5, 0).setHandler(this);
          }
        }

        rToWatch.erase(rToWatch.begin() + ix);
        doRefresh = true;
      }
    }

    for (size_t k = 0; k < backExit.size(); k++) {
      const oFreePunch *fp = backExit[k].second;
      pRunner rToFinish = fp->getTiedRunner();
      if (count(watchedR.begin(), watchedR.end(), rToFinish->getId()) > 0) {
        pair<int,int> &se = startFinishTime[rToFinish->getId()];
        se.second = fp->getAdjustedTime();
        size_t ix = find(watchedR.begin(), watchedR.end(), rToFinish->getId()) - watchedR.begin();
        watchedR.erase(watchedR.begin() + ix);
      }
    }

    if (doRefresh)
      gdi.refreshFast();
  }
  else if (type == GUI_TIMEOUT) {
    gdi.restore("LiveResult", false);
    int h,w;
    gdi.getTargetDimension(w, h);
    gdi.fillDown();
    BaseInfo *bi = gdi.setTextTranslate("timing", "MeOS Timing", false);
    TextInfo &ti = dynamic_cast<TextInfo &>(*bi);
    ti.changeFont(getFont(gdi, 0.7));
    gdi.refreshFast();
    resYPos = ti.textRect.bottom + gdi.scaleLength(20);
    calculateResults();
    showResultList = 0;
    gdi.addTimeoutMilli(300, "res", 0).setHandler(this);
  }
  else if (type == GUI_TIMER) {
    if (size_t(showResultList) >= results.size())
      return;
    Result &res = results[showResultList];
    string font = getFont(gdi, 0.7);
    int y = resYPos;
    pRunner r = oe->getRunner(res.runnerId, 0);
    if (!r) {
      showResultList++;
      gdi.addTimeoutMilli(10, "res" + itos(showResultList), 0).setHandler(this);
    }
    else if (res.place > 0) {
      int h,w;
      gdi.getTargetDimension(w, h);
   
      gdi.takeShownStringsSnapshot();
      TextInfo &ti = gdi.addStringUT(y, 30, fontLarge, itos(res.place) + ".", 0, 0, font.c_str());
      int ht = ti.textRect.bottom - ti.textRect.top;
      gdi.addStringUT(y, 30 + ht * 2 , fontLarge, r->getName(), 0, 0, font.c_str());
      //int w = gdi.getWidth();
      gdi.addStringUT(y, w - 4 * ht, fontLarge, formatTime(res.time), 0, 0, font.c_str());
      gdi.refreshSmartFromSnapshot(false);
      resYPos += int (ht * 1.1);
      showResultList++;
      
      int limit = h - ht * 2;
      //OutputDebugString(("w:" + itos(resYPos) + " " + itos(limit) + "\n").c_str());
      if ( resYPos < limit )
        gdi.addTimeoutMilli(300, "res" + itos(showResultList), 0).setHandler(this);
    }
  }
}

void LiveResult::calculateResults() {
  rToWatch.clear();
  results.clear();
  results.reserve(startFinishTime.size());
  const int highTime = 10000000;
  for (map<int, pair<int, int> >::iterator it = startFinishTime.begin();
       it != startFinishTime.end(); ++it) {
    pRunner r = oe->getRunner(it->first, 0);
    if (!r)
      continue;
    results.push_back(Result());
    results.back().runnerId = it->first;
    results.back().time = it->second.second - it->second.first;
    if (results.back().time <= 0 || r->getStatus() > StatusOK)
      results.back().time = highTime;
  }

  sort(results.begin(), results.end());

  int place = 1;
  for (size_t k = 0; k< results.size(); k++) {
    if (results[k].time < highTime) {
      if (k>0 && results[k-1].time < results[k].time)
        place = k + 1;

      results[k].place = place;
    }
    else {
      results[k].place = 0;
    }
  }
}
