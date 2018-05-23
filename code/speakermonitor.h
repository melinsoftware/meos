#pragma once
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
#include "oListInfo.h"
#include <deque>

class gdioutput;

class SpeakerMonitor : public GuiHandler {
private:

  struct ResultKey {
    int classId;
    int radio;
    int leg;

    ResultKey(int classId, int radio, int leg) : 
               classId(classId), radio(radio), leg(leg) {}

    bool operator<(const ResultKey &key) const {
      if (classId != key.classId)
        return classId < key.classId;
      else if (radio != key.radio)
        return radio < key.radio;
      else
        return leg < key.leg;
    }

    bool operator==(const ResultKey &key) const {
      return radio == key.radio && classId == key.classId && leg == key.leg;
    }
  };

  struct ResultInfo : public ResultKey {
    int time;
    int totTime;

    ResultInfo(const ResultKey &key, int time, int totalTime) : 
    
    ResultKey(key), time(time), totTime(totalTime) {}

    ResultInfo(int classId, int radio, int leg, int time, int totalTime) : 
               ResultKey(classId, radio, leg), time(time), totTime(totalTime) {}
  };

  struct SimpleResult {
    SimpleResult() : r(0), eventIx(-1) {}
    int totalTime;
    pRunner r;
    int eventIx;
    bool isValid() const {return r != 0;}
  };

  oEvent &oe;
  SpeakerMonitor &operator=(const SpeakerMonitor&); // Not used

  set<int> classFilter;
  set<int> controlIdFilter;
  vector<oEvent::ResultEvent> results;
  bool totalResults;

  int placeLimit;
  int numLimit;

  int maxClassNameWidth;

  int classWidth;
  int  timeWidth;
  int totWidth;
  int extraWidth;
  wstring dash;
  
  void renderResult(gdioutput &gdi, 
                    const oEvent::ResultEvent *ahead,
                    const oEvent::ResultEvent &res, 
                    const oEvent::ResultEvent *behind,
                    int &order, bool firstResults);

  static bool sameResultPoint(const oEvent::ResultEvent &a,
                              const oEvent::ResultEvent &b);
                    
  void getSharedResult(const oEvent::ResultEvent &res, vector<pRunner> &shared) const;
  void getSharedResult(const oEvent::ResultEvent &res, wstring &detail) const;

  void splitAnalysis(gdioutput &gdi, int xp, int yp, pRunner r);

  void getMessage(const oEvent::ResultEvent &res, 
                  wstring &message, deque<wstring> &details);

  map<ResultKey, int> firstTimes;
  map<ResultKey, int> totalLeaderTimes;
  map<ResultKey, map<int, int> > dynamicTotalLeaderTimes;
  multimap<int, int> timeToResultIx;

  int getResultIx(const ResultInfo &rinfo);

  map<int,  vector<ResultInfo> > runnerToTimeKey;
  vector<SimpleResult> orderedResults;

  static bool timeSort(ResultInfo &a, ResultInfo &b) {
    return a.time < b.time;
  }

  const oEvent::ResultEvent *getAdjacentResult(const oEvent::ResultEvent &res, int delta) const;

  int getLeaderTime(const oEvent::ResultEvent &res);
  int getDynamicLeaderTime(const oEvent::ResultEvent &res, int time);
  int getDynamicLeaderTime(const ResultKey &res, int time);

public:
  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);

  SpeakerMonitor(oEvent &oe);
  virtual ~SpeakerMonitor();

  void setClassFilter(const set<int> &filter, const set<int> &controlIdFilter);
  
  void useTotalResults(bool total) {
    totalResults = total;
  }
  
  bool useTotalResults() const {
    return totalResults;
  }

  void setLimits(int placeLimit, int numLimit);
  void show(gdioutput &gdi);
  
  void calculateResults();
};
