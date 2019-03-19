#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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
class oRunner;
#include "gdifonts.h"
#include "gdioutput.h"

struct SpeakerString {
  wstring str;
  int format;
  bool hasTimer;
  int timer;
  int timeout;
  string moveKey;
  GDICOLOR color;
  SpeakerString() : format(0), hasTimer(false), timer(0), timeout(NOTIMEOUT), color(colorDefault) {}
  SpeakerString(int formatIn, const wstring &in) : str(in), format(formatIn), hasTimer(false),
                                           timer(0), timeout(NOTIMEOUT),  color(colorDefault) {}
  SpeakerString(int formatIn, int timerIn, int timeoutIn = NOTIMEOUT) : format(formatIn),
                                           hasTimer(true), timer(timerIn),
                                           timeout(timeoutIn), color(colorDefault) {}
};

class oSpeakerObject
{
public:
  struct RunningTime {
    void reset() { time = 0; preliminary = 0; }
    int time;
    int preliminary;
    RunningTime() : time(0), preliminary(0) {}
  };

  void reset() {
    owner = 0;
    bib.clear();
    names.clear();
    outgoingnames.clear();
    resultRemark.clear();
    club.clear();
    startTimeS.clear();
    status = StatusUnknown;
    finishStatus = StatusUnknown;
    useSinceLast = 0;
    runningTime.reset();
    runningTimeLeg.reset();
    runningTimeSinceLast.reset();
  }
  oRunner *owner;
  wstring bib;
  vector<wstring> names;
  vector<wstring> outgoingnames;
  string resultRemark;
  wstring club;
  wstring startTimeS;

  bool useSinceLast;
  int place;
  int parallelScore;

  // For parallel legs
  int runnersFinishedLeg;
  int runnersTotalLeg;

  RunnerStatus status;
  RunnerStatus finishStatus;

  RunningTime runningTime;
  RunningTime runningTimeLeg;
  RunningTime runningTimeSinceLast;

  bool isRendered;
  int priority;
  bool missingStartTime;

  // In seconds. Negative if undefined.
  int timeSinceChange;

  oSpeakerObject() : owner(0), place(0), parallelScore(0), status(StatusUnknown),
                     finishStatus(StatusUnknown), isRendered(false),
                     priority(0), missingStartTime(false), timeSinceChange(-1), useSinceLast(false),
                     runnersFinishedLeg(0), runnersTotalLeg(0) {}

};

