/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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
#include <cassert>

#include "oEvent.h"
#include "autotask.h"
#include "TabAuto.h"
#include "TabSI.h"
#include "meos_util.h"
#include "socket.h"
#include "meosexception.h"

const int SYNC_FACTOR = 4; 

//#define DEBUGPRINT
//#define NODELAY

AutoTask::AutoTask(HWND hWnd, oEvent &oeIn, gdioutput &gdiIn) : hWndMain(hWnd), oe(oeIn), gdi(gdiIn) {
  currentRevision = 0;
  lock = false;
  lastSynchTime = 0;
  lastTriedSynchTime = 0;

  autoSaveTimeBase = autoSaveTime = oe.getPropertyInt("AutoSaveTimeOut", (3*60+15)*1000);
  synchBaseTime = oe.getPropertyInt("SynchronizationTimeOut", 2500);
  maxDelay = oe.getPropertyInt("MaximumSpeakerDelay", 10000)/SYNC_FACTOR;
}

void AutoTask::autoSave() {
  if (!oe.empty()) {
    wstring msg;
    try {
      if (oe.getNumRunners() > 500)
        gdi.setWaitCursor(true);

      DWORD tic = GetTickCount();
      oe.save();
      DWORD toc = GetTickCount();

      if (toc > tic) {
        int timeToSave = toc - tic;
        int interval = max(autoSaveTimeBase, timeToSave * 10);
        if (abs(interval - autoSaveTime) > 4000) {
          autoSaveTime = interval;
          resetSaveTimer();
        }
      }
    }
    catch (meosException &ex) {
      msg = ex.wwhat();
    }
    catch(std::exception &ex) {
      msg = gdi.widen(ex.what());
    }
    catch(...) {
      msg = L"Ett okänt fel inträffade.";
    }

    if (!msg.empty()) {
      gdi.alert(msg);
    }
    else
      gdi.addInfoBox("", L"Tävlingsdata har sparats.", 10);

    gdi.setWaitCursor(false);

  }
}

void AutoTask::resetSaveTimer() {
  KillTimer(hWndMain, 1);
  SetTimer(hWndMain, 1, autoSaveTime, 0); //Autosave
}

void AutoTask::setTimers() {
  SetTimer(hWndMain, 2, 100, 0); //Interface timeout
  SetTimer(hWndMain, 3, synchBaseTime, 0); //DataSync
}

void AutoTask::interfaceTimeout(const vector<gdioutput *> &windows) {
  TabAuto *tabAuto = dynamic_cast<TabAuto *>(gdi.getTabs().get(TAutoTab));
  TabSI *tabSI = dynamic_cast<TabSI *>(gdi.getTabs().get(TSITab));

  if (lock)
    return;
  lock = true;

  wstring msg;
  try {
    DWORD tick = GetTickCount();
    for (size_t k = 0; k<windows.size(); k++) {
      if (windows[k])
        windows[k]->CheckInterfaceTimeouts(tick);
    }

    if (tabAuto)
      tabAuto->timerCallback(gdi);

    if (tabSI)
      while(tabSI->checkpPrintQueue(gdi));
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = gdi.widen(ex.what());
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }
  if (!msg.empty()) {
    gdi.alert(msg);
    gdi.setWaitCursor(false);
  }
  lock = false;
}

void AutoTask::addSynchTime(DWORD tick) {
  if (tick > 1000 * 60)
    return; // Ignore extreme times

  if (synchQueue.size () > 8)
    synchQueue.pop_front();

  synchQueue.push_back(tick);
}

DWORD AutoTask::getAvgSynchTime() {
#ifdef NODELAY
  return 0;
#else
  double res = 0;
  for (size_t k = 0; k < synchQueue.size(); k++) {
    double sq = synchQueue[k];
    res += sq*sq;
  }

  if (res > 0)
    res = sqrt(res) / double(synchQueue.size());

  return min(int(res), maxDelay);
#endif
}

void AutoTask::synchronize(const vector<gdioutput *> &windows) {
  DWORD tic = GetTickCount();
  DWORD avg = getAvgSynchTime();
  //OutputDebugString(("AVG Update Time: " + itos(avg)).c_str());
  if (tic > lastSynchTime) {
    DWORD since = tic - lastSynchTime;
    if (since < avg * SYNC_FACTOR) {
      //OutputDebugString((" skipped: " + itos(since) + "\n").c_str());
      return;
    }
    //else
      //OutputDebugString((" check: tsl = " + itos(since) + "\n").c_str());
  }
  else
    lastSynchTime = tic;

  if (oe.hasDirectSocket()) {
    // Clear any incomming messages (already in db)
    vector<SocketPunchInfo> pi;
    oe.getDirectSocket().getPunchQueue(pi);
  }

  // Store last time we tried to synch
  lastTriedSynchTime = tic;

  if (synchronizeImpl(windows)) {
    DWORD toc = GetTickCount();
    if (toc > tic)
      addSynchTime(toc-tic);

    lastSynchTime = toc;
#ifdef DEBUGPRINT
    OutputDebugString((" updated: " + itos(toc-tic) + "\n").c_str());
#endif
  }

  //OutputDebugString(" no update\n");
}

void AutoTask::advancePunchInformation(const vector<gdioutput *> &windows) {
  DWORD tic = GetTickCount();
  DWORD avg = getAvgSynchTime();
  //OutputDebugString(("Direct Update Time: " + itos(avg)).c_str());
  if (tic > lastSynchTime) {
    DWORD since = tic-lastSynchTime;
    if (since < avg * SYNC_FACTOR) {
      //OutputDebugString((" skipped: " + itos(since) + "\n").c_str());
      return;
    }
  }
  else
    lastSynchTime = tic;

  DWORD since = tic - lastTriedSynchTime;
  if (since > DWORD(synchBaseTime*4)) { // Synchronize all instead.
    synchronize(windows);
    return;
  }

  if (advancePunchInformationImpl(windows)) {
    DWORD toc = GetTickCount();
    if (toc > tic)
      addSynchTime(toc-tic);
    lastSynchTime = toc;
#ifdef DEBUGPRINT
    OutputDebugString((" direct update: " + itos(toc-tic) + "\n").c_str());
#endif
  }
}

bool AutoTask::synchronizeImpl(const vector<gdioutput *> &windows) {
  if (lock)
    return false;
  lock = true;

  DWORD d=0;
  bool doSync = false;
  bool doSyncPunch = false;
  TabAuto *tabAuto = dynamic_cast<TabAuto *>(gdi.getTabs().get(TAutoTab));

  for (size_t k = 0; k<windows.size(); k++) {
    if (windows[k] && windows[k]->getData("DataSync", d)) {
      doSync = true;
    }
    if (windows[k] && windows[k]->getData("PunchSync", d)) {
      doSyncPunch = true;
      doSync = true;
    }
  }

  wstring msg;
  bool ret = false;
  try {
    if (doSync || (tabAuto && tabAuto->synchronize)) {

      if (tabAuto && tabAuto->synchronizePunches)
        doSyncPunch = true;

      if ( oe.autoSynchronizeLists(doSyncPunch) || oe.getRevision() != currentRevision) {
        ret = true;
        if (getAvgSynchTime() > 1000)
          gdi.setWaitCursor(true);

        if (doSync) {
          for (size_t k = 0; k<windows.size(); k++) {
            if (windows[k]) {
              try {
                windows[k]->makeEvent("DataUpdate", "autosync", 0, 0, false);
              }
              catch (meosException &ex) {
                msg = ex.wwhat();
              }
              catch(std::exception &ex) {
                msg = gdi.widen(ex.what());
              }
              catch(...) {
                msg = L"Ett okänt fel inträffade.";
              }
            }
          }
        }

        if (tabAuto)
          tabAuto->syncCallback(gdi);
      }
    }
    oe.resetSQLChanged(false, true);
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch (std::exception &ex) {
    msg = gdi.widen(ex.what());
  }
  catch (...) {
    msg = L"Ett okänt fel inträffade.";
  }

  currentRevision = oe.getRevision();

  if (!msg.empty()) {
    gdi.alert(msg);
  }
  lock = false;
  gdi.setWaitCursor(false);
  return ret;
}

bool AutoTask::advancePunchInformationImpl(const vector<gdioutput *> &windows) {
  DWORD d=0;
  bool doSync = false;
  bool doSyncPunch = false;

  for (size_t k = 0; k<windows.size(); k++) {
    if (windows[k] && windows[k]->getData("DataSync", d)) {
      doSync = true;
    }
    if (windows[k] && windows[k]->getData("PunchSync", d)) {
      doSyncPunch = true;
      doSync = true;
    }
  }

  //string msg;
  try {
    if (oe.hasDirectSocket()) {
      //OutputDebugString("Advance punch info\n");
      vector<SocketPunchInfo> pi;
      oe.getDirectSocket().getPunchQueue(pi);
      bool ret = oe.advancePunchInformation(windows, pi, doSyncPunch, doSync);
      if (ret)
        oe.resetSQLChanged(false, true);

      return ret;
    }
  }
  catch (meosException &ex) {
    wstring msg = ex.wwhat();
    OutputDebugString(msg.c_str());
  }
  catch (std::exception &ex) {
    wstring str;
    string2Wide(ex.what(), str);
    OutputDebugString(str.c_str());
  }
  catch (...) {
  }

  return false;
}
