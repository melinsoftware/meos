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

#include <vector>
#include <deque>

class oEvent;
class gdioutput;

class AutoTask {
private:
  oEvent &oe;
  gdioutput &gdi;
  AutoTask &operator=(const AutoTask &);

  bool synchronizeImpl(const vector<gdioutput *> &gdi);
  bool advancePunchInformationImpl(const vector<gdioutput *> &windows);

  long currentRevision;
  bool lock;

  deque<DWORD> synchQueue;
  deque<DWORD> directQueue;

  DWORD lastSynchTime;
  DWORD lastTriedSynchTime;
  void addSynchTime(DWORD tick);
  DWORD getAvgSynchTime();

  HWND hWndMain;

  int autoSaveTime;
  int autoSaveTimeBase;

  int synchBaseTime;
  int maxDelay; // The maximal delay between syncs
public:

  void setTimers();

  void resetSaveTimer();

  AutoTask(HWND hWnd, oEvent &oe, gdioutput &gdi);
  void autoSave();
  /** Trigger timed text updates and gdi timeouts, service timeouts. */
  void interfaceTimeout(const vector<gdioutput *> &windows);

  /** Read updates from SQL (if connected) and update windows due to changed competition data.*/
  void synchronize(const vector<gdioutput *> &windows);

  /** Fetch fast advance information.*/
  void advancePunchInformation(const vector<gdioutput *> &windows);
};
