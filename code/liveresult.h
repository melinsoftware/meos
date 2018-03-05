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
#include "guihandler.h"

class gdioutput;

class LiveResult : public GuiHandler {
  oEvent *oe;
  oListInfo li;
  bool active;
  unsigned int lastTime;
  map< pair<int, int>, int > processedPunches;
  vector<int> rToWatch;
  vector<int> watchedR;// Backlog
  
  map<int, int> runner2ScreenPos;
  int screenSize;
  bool isDuel;

  wstring baseFont;
  void showDefaultView(gdioutput &gdi);
  map<int, pair<int, int> > startFinishTime;
  int showResultList;
  int resYPos;  
  wstring getFont(const gdioutput &gdi, double relScale) const;
  double timerScale;
  struct Result {
    int place;
    int runnerId;
    int time;
    bool operator<(const Result &b) const {
      return time < b.time;
    }
  };

  vector< Result > results;

  void calculateResults();

public:
  LiveResult(oEvent *oe);
  ~LiveResult() {}

  void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    
  void showTimer(gdioutput &gdi, const oListInfo &li);
};
