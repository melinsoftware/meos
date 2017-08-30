/************************************************************************
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

// oEvent.cpp: implementation of the oEvent class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <vector>
#include "oEvent.h"
#include "gdioutput.h"
#include "oDataContainer.h"
#include "csvparser.h"

#include "TabAuto.h"

#include "random.h"
#include "SportIdent.h"
#include "meosdb/sqltypes.h"
#include "meosexception.h"
#include "meos_util.h"

#include "meos.h"
#include <cassert>
typedef bool (__cdecl* OPENDB_FCN)(void);
typedef int (__cdecl* SYNCHRONIZE_FCN)(oBase *obj);



bool oEvent::connectToServer()
{
  if (isThreadReconnecting())
    return false;

#ifdef BUILD_DB_DLL
  if (msOpenDatabase)
    return true;
#endif

#ifdef BUILD_DB_DLL
  hMod=LoadLibrary("meosdb.dll");

  msOpenDatabase = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msOpenDatabase");
  msConnectToServer = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msConnectToServer");
  msSynchronizeUpdate = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msSynchronizeUpdate");
  msSynchronizeRead = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msSynchronizeRead");
  msSynchronizeList = (SYNCHRONIZELIST_FCN)GetProcAddress(hMod, "msSynchronizeList");
  msRemove = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msRemove");
  msMonitor = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msMonitor");
  msDropDatabase = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msDropDatabase");
  msUploadRunnerDB = (SYNCHRONIZE_FCN)GetProcAddress(hMod, "msUploadRunnerDB");

  msGetErrorState = (ERRORMESG_FCN)GetProcAddress(hMod, "msGetErrorState");
  msResetConnection = (OPENDB_FCN)GetProcAddress(hMod, "msResetConnection");
  msReConnect = (OPENDB_FCN)GetProcAddress(hMod, "msReConnect");

  if (!msOpenDatabase || !msSynchronizeUpdate || !msSynchronizeRead
    || !msConnectToServer || !msReConnect || !msGetErrorState
    || !msMonitor || !msDropDatabase || !msUploadRunnerDB) {
    MessageBox(NULL, "Cannot load MySQL library.", NULL, MB_OK);
    // handle the error
    FreeLibrary(hMod);
    hMod=0;
    return false;//SOME_ERROR_CODE;
  }
#else/*
  msOpenDatabase = (SYNCHRONIZE_FCN)::msOpenDatabase;
  msConnectToServer = (SYNCHRONIZE_FCN)::msConnectToServer;
  msSynchronizeUpdate = (SYNCHRONIZE_FCN)::msSynchronizeUpdate;
  msSynchronizeRead = (SYNCHRONIZE_FCN)::msSynchronizeRead;
  msSynchronizeList = (SYNCHRONIZELIST_FCN)::msSynchronizeList;
  msRemove = (SYNCHRONIZE_FCN)::msRemove;
  msMonitor = (SYNCHRONIZE_FCN)::msMonitor;
  msDropDatabase = (SYNCHRONIZE_FCN)::msDropDatabase;
  msUploadRunnerDB = (SYNCHRONIZE_FCN)::msUploadRunnerDB;
  msGetErrorState = (ERRORMESG_FCN)::msGetErrorState;
  msResetConnection = (OPENDB_FCN)::msResetConnection;
  msReConnect = (OPENDB_FCN)::msReConnect;*/
#endif
  return true;
}

void oEvent::startReconnectDaemon()
{
  if (isThreadReconnecting())
    return;

  char bf[256];
  msGetErrorState(bf);

  MySQLReconnect msqlr(lang.tl("warning:dbproblem#" + string(bf)));
  msqlr.interval=5;
  HasDBConnection = false;
  HasPendingDBConnection = true;
  tabAutoAddMachinge(msqlr);

  gdibase.setDBErrorState(false);
  gdibase.setWindowTitle(oe->getTitleName());
  if (!isReadOnly()) {
    // Do not show in kiosk-mode
    gdibase.alert("warning:dbproblem#" + string(bf));
  }
}

bool oEvent::msSynchronize(oBase *ob)
{
  //if (!msSynchronizeRead) return true;
  if (!HasDBConnection && !HasPendingDBConnection)
    return true;

  int ret = msSynchronizeRead(ob);

  char err[256];
  if (msGetErrorState(err))
    gdibase.addInfoBox("sqlerror", gdibase.widen(err), 15000);

  if (ret==0) {
    verifyConnection();
    return false;
  }

  if (typeid(*ob)==typeid(oTeam)) {
    static_cast<pTeam>(ob)->apply(false, 0, false);
  }

  if (ret==1) {
    gdibase.RemoveFirstInfoBox("sqlwarning");
    gdibase.addInfoBox("sqlwarning", L"Varning: ändringar i X blev överskrivna#" + ob->getInfo(), 5000);
  }
  return ret!=0;
}

void oEvent::resetChangeStatus(bool onlyChangable)
{
  if (HasDBConnection) {
    if (!onlyChangable) {
       // The object here has no dependeces that makes it necessary to set/rest

      for (list<oFreePunch>::iterator it=oe->punches.begin();
          it!=oe->punches.end(); ++it)
        it->resetChangeStatus();

      //synchronize changed objects
      for (list<oCard>::iterator it=oe->Cards.begin();
            it!=oe->Cards.end(); ++it)
        it->resetChangeStatus();

      for (list<oClub>::iterator it=oe->Clubs.begin();
            it!=oe->Clubs.end(); ++it)
        it->resetChangeStatus();

      for (list<oControl>::iterator it=oe->Controls.begin();
            it!=oe->Controls.end(); ++it)
        it->resetChangeStatus();

      for (list<oCourse>::iterator it=oe->Courses.begin();
            it!=oe->Courses.end(); ++it)
        it->resetChangeStatus();

      for (list<oClass>::iterator it=oe->Classes.begin();
          it!=oe->Classes.end(); ++it)
        it->resetChangeStatus();
    }

    for (list<oRunner>::iterator it=oe->Runners.begin();
        it!=oe->Runners.end(); ++it)
      it->resetChangeStatus();

    for (list<oTeam>::iterator it=oe->Teams.begin();
        it!=oe->Teams.end(); ++it)
      it->resetChangeStatus();
  }
}

void oEvent::storeChangeStatus(bool onlyChangable)
{
  if (HasDBConnection) {
    if (!onlyChangable) {
       // The object here has no dependeces that makes it necessary to set/rest
      for (list<oFreePunch>::iterator it=oe->punches.begin();
            it!=oe->punches.end(); ++it)
        it->storeChangeStatus();

      for (list<oCard>::iterator it=oe->Cards.begin();
            it!=oe->Cards.end(); ++it)
        it->storeChangeStatus();

      for (list<oClub>::iterator it=oe->Clubs.begin();
            it!=oe->Clubs.end(); ++it)
        it->storeChangeStatus();

      for (list<oControl>::iterator it=oe->Controls.begin();
            it!=oe->Controls.end(); ++it)
        it->storeChangeStatus();

      for (list<oCourse>::iterator it=oe->Courses.begin();
            it!=oe->Courses.end(); ++it)
        it->storeChangeStatus();

      for (list<oClass>::iterator it=oe->Classes.begin();
          it!=oe->Classes.end(); ++it)
        it->storeChangeStatus();
    }

    for (list<oRunner>::iterator it=oe->Runners.begin();
        it!=oe->Runners.end(); ++it)
      it->storeChangeStatus();

    for (list<oTeam>::iterator it=oe->Teams.begin();
        it!=oe->Teams.end(); ++it)
      it->storeChangeStatus();
  }
}


bool oEvent::synchronizeList(oListId id, bool preSyncEvent, bool postSyncEvent) {
  if (!HasDBConnection)
    return true;

  if (preSyncEvent) {
    msSynchronize(this);
    resetSQLChanged(true, false);
  }

  if ( !msSynchronizeList(this, id) ) {
    verifyConnection();
    return false;
  }

  if (id == oLPunchId)
    advanceInformationPunches.clear();

  if (postSyncEvent) {
    reEvaluateChanged();
    resetChangeStatus();
    return true;
  }

  return true;
}

bool oEvent::needReEvaluate() {
  return sqlChangedRunners |
         sqlChangedClasses |
         sqlChangedCourses |
         sqlChangedControls |
         sqlChangedCards |
         sqlChangedTeams;
}

void oEvent::resetSQLChanged(bool resetAllTeamsRunners, bool cleanClasses) {
  sqlChangedRunners = false;
  sqlChangedClasses = false;
  sqlChangedCourses = false;
  sqlChangedControls = false;
  sqlChangedClubs = false;
  sqlChangedCards = false;
  sqlChangedPunches = false;
  sqlChangedTeams = false;

  if (resetAllTeamsRunners) {
    for (list<oRunner>::iterator it=oe->Runners.begin();
      it!=oe->Runners.end(); ++it) {
      it->storeChangeStatus();
      it->sqlChanged = false;
    }
    for (list<oTeam>::iterator it=oe->Teams.begin();
      it!=oe->Teams.end(); ++it) {
        it->storeChangeStatus();
      it->sqlChanged = false;
    }
  }
  if (cleanClasses) {
    // This data is used to redraw lists/speaker etc.
    for (list<oClass>::iterator it=oe->Classes.begin();
      it!=oe->Classes.end(); ++it) {
      it->storeChangeStatus();
      it->sqlChangedControlLeg.clear();
      it->sqlChangedLegControl.clear();
    }
    globalModification = false;
  }
}

bool BaseIsRemoved(const oBase &ob){return ob.isRemoved();}

//Returns true if data is changed.
bool oEvent::autoSynchronizeLists(bool SyncPunches)
{
  if (!HasDBConnection)
    return false;

  bool changed=false;
  string ot;

  int mask = getListMask(*this);
  if (mask == 0)
    return false;

  // Reset change data and store update status on objects
  // (which might be incorrectly changed during sql update)
  resetSQLChanged(true, false);

  //Synchronize ourself
  if (mask & oLEventId) {
    ot=sqlUpdated;
    msSynchronize(this);
    if (sqlUpdated!=ot) {
      changed=true;
      gdibase.setWindowTitle(getTitleName());
    }
  }

  //Controls
  if (mask & oLControlId) {
    int oc = sqlCounterControls;
    ot = sqlUpdateControls;
    synchronizeList(oLControlId, false, false);
    changed |= oc!=sqlCounterControls;
    changed |= ot!=sqlUpdateControls;
  }

  //Courses
  if (mask & oLCourseId) {
    int oc = sqlCounterCourses;
    ot = sqlUpdateCourses;
    synchronizeList(oLCourseId, false, false);
    changed |= oc!=sqlCounterCourses;
    changed |= ot!=sqlUpdateCourses;
  }

  //Classes
  if (mask & oLClassId) {
    int oc = sqlCounterClasses;
    ot = sqlUpdateClasses;
    synchronizeList(oLClassId, false, false);
    changed |= oc!=sqlCounterClasses;
    changed |= ot!=sqlUpdateClasses;
  }

  //Clubs
  if (mask & oLClubId) {
    int oc = sqlCounterClubs;
    ot = sqlUpdateClubs;
    synchronizeList(oLClubId, false, false);
    changed |= oc!=sqlCounterClubs;
    changed |= ot!=sqlUpdateClubs;
  }

  //Cards
  if (mask & oLCardId) {
    int oc = sqlCounterCards;
    ot = sqlUpdateCards;
    synchronizeList(oLCardId, false, false);
    changed |= oc!=sqlCounterCards;
    changed |= ot!=sqlUpdateCards;
  }

  //Runners
  if (mask & oLRunnerId) {
    int oc = sqlCounterRunners;
    ot = sqlUpdateRunners;
    synchronizeList(oLRunnerId, false, false);
    changed |= oc!=sqlCounterRunners;
    changed |= ot!=sqlUpdateRunners;
  }

  //Teams
  if (mask & oLTeamId) {
    int oc = sqlCounterTeams;
    ot = sqlUpdateTeams;
    synchronizeList(oLTeamId, false, false);
    changed |= oc!=sqlCounterTeams;
    changed |= ot!=sqlUpdateTeams;
  }

  if (SyncPunches && (mask & oLPunchId)) {
    //Punches
    int oc = sqlCounterPunches;
    ot = sqlUpdatePunches;
    synchronizeList(oLPunchId, false, false);
    changed |= oc!=sqlCounterPunches;
    changed |= ot!=sqlUpdatePunches;
  }

  if (changed) {
    if (needReEvaluate())
      reEvaluateChanged();

    reCalculateLeaderTimes(0);
    //Restore changed staus on object that might have been changed
    //during sql update, due to partial updates
    resetChangeStatus();
    return true;
  }

  return false;
}


bool oEvent::connectToMySQL(const string &server, const string &user, const string &pwd, int port)
{
  if (isThreadReconnecting())
    return false;

  if (!connectToServer())
    return false;

  MySQLServer=server;
  MySQLPassword=pwd;
  MySQLPort=port;
  MySQLUser=user;

  //Delete non-server competitions.
  list<CompetitionInfo> saved;
  list<CompetitionInfo>::iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty())
      saved.push_back(*it);
  }
  cinfo = saved;

  if (!msConnectToServer(this)) {
    char bf[256];
    msGetErrorState(bf);
    MessageBox(NULL, lang.tl(bf).c_str(), L"Error", MB_OK);
    return false;
  }

  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Name.size() > 1 && it->Name[0] == '%')
      it->Name = lang.tl(it->Name.substr(1));
  }

  return true;
}


bool oEvent::uploadSynchronize()
{
  if (isThreadReconnecting())
    throw std::exception("Internt fel i anslutningen. Starta om MeOS");
  wstring newId = makeValidFileName(currentNameId, true);
  currentNameId = newId;

  for (list<CompetitionInfo>::iterator it = cinfo.begin(); it != cinfo.end(); ++it) {
    if (it->FullPath == currentNameId && it->Server.length()>0) {
      gdioutput::AskAnswer ans = gdibase.askCancel(L"ask:overwrite_server");

      if (ans == gdioutput::AnswerCancel)
        return false;
      else if (ans == gdioutput::AnswerNo) {
        int len = currentNameId.length();
        wchar_t ex[10];
        swprintf_s(ex, L"_%05XZ", (GetTickCount()/97) & 0xFFFFF);
        if (len > 0) {
          if (len< 7 || currentNameId[len-1] != 'Z')
            currentNameId += ex;
          else {
            wchar_t CurrentNameId[64];
            wcscpy_s(CurrentNameId, currentNameId.c_str());
            wcscpy_s(CurrentNameId + len - 7, 64 - len + 7, ex);
            currentNameId = CurrentNameId;
          }
        }
      }
      else {
        if (!gdibase.ask(L"ask:overwriteconfirm"))
          return false;
      }
    }
  }

  HasDBConnection=false;

#ifdef BUILD_DB_DLL
  if (!msSynchronizeUpdate)
    throw std::exception("Internt fel. Starta om MeOS");
#endif

  if ( !msOpenDatabase(this) ){
    char bf[256];
    msGetErrorState(bf);
    string error = string("Kunde inte öppna databasen (X).#") + bf;
    throw std::exception(error.c_str());
  }

  if ( !msSynchronizeUpdate(this) ) {
    char bf[256];
    msGetErrorState(bf);
    string error = string("Kunde inte ladda upp tävlingen (X).#") + bf;
    throw std::exception(error.c_str());
  }

  OpFailStatus stat = (OpFailStatus)msUploadRunnerDB(this);

  if (stat == opStatusFail) {
    char bf[256];
    msGetErrorState(bf);
    string error = string("Kunde inte ladda upp löpardatabasen (X).#") + bf;
    throw meosException(error);
  }
  else if (stat == opStatusWarning) {
    char bf[256];
    msGetErrorState(bf);
    gdibase.addInfoBox("", wstring(L"Kunde inte ladda upp löpardatabasen (X).#") + gdibase.widen(bf), 5000);
  }

  HasDBConnection=true;

  // Save local version of database
  saveRunnerDatabase(currentNameId.c_str(), false);

  return true;
}

//Load a (new) competition from the server.
bool oEvent::readSynchronize(const CompetitionInfo &ci)
{
  if (ci.Id<=0)
    throw std::exception("help:12290");

  if (isThreadReconnecting())
    return false;

  HasDBConnection=false;

#ifdef BUILD_DB_DLL
  if (!msConnectToServer)
    return false;
#endif

  MySQLServer=ci.Server;
  MySQLPassword=ci.ServerPassword;
  MySQLPort=ci.ServerPort;
  MySQLUser=ci.ServerUser;

  //Delete non-server competitions.
  list<CompetitionInfo> saved;
  list<CompetitionInfo>::iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty())
      saved.push_back(*it);
  }
  cinfo=saved;

  if (!msConnectToServer(this)) {
    char bf[256];
    msGetErrorState(bf);
    throw std::exception(bf);
    return false;
  }

  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Name.size() > 1 && it->Name[0] == '%')
      it->Name = lang.tl(it->Name.substr(1));
  }

  newCompetition(L"");
  Id=ci.Id;
  currentNameId = ci.FullPath;

  wchar_t file[260];
  swprintf_s(file, L"%s.dbmeos", currentNameId.c_str());
  getUserFile(CurrentFile, file);
  if ( !msOpenDatabase(this) ) {
    char bf[256];
    msGetErrorState(bf);
    throw std::exception(bf);
  }

  updateFreeId();
  HasDBConnection=false;

  openRunnerDatabase(currentNameId.c_str());

  int ret = msSynchronizeRead(this);
  if (ret == 0) {
    char bf[256];
    msGetErrorState(bf);

    string err = string("Kunde inte öppna tävlingen (X)#") + bf;
    throw std::exception(err.c_str());
  }
  else if (ret == 1) {
    // Warning
    char bf[256];
    msGetErrorState(bf);
    wstring info = L"Databasvarning: X#" + lang.tl(bf);
    gdibase.addInfoBox("sqlerror", info, 15000);
  }

  // Cache database locally
  saveRunnerDatabase(currentNameId.c_str(), false);

  HasDBConnection=true;

  // Setup multirunner links
  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it)
    it->createMultiRunner(false,false);

  // Remove incorrect references
  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->multiRunner.size() > 0 ) {
      vector<pRunner> &pr = it->multiRunner;
      for (size_t k=0; k<pr.size(); k++) {
        if (pr[k]==0 || pr[k]->tParentRunner != &*it) {
          it->multiRunner.clear();
          it->updateChanged();
          it->synchronize();
          break;
        }
      }
    }
  }

  // Check duplicates
  vector<bool> usedInTeam(qFreeRunnerId+1);
  bool teamCorrected = false;

  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->correctionNeeded) {
      it->updateChanged();
      teamCorrected = true;
    }

    for (size_t i = 0; i < it->Runners.size(); i++) {
      pRunner r = it->Runners[i];

      if (r != 0) {
        int expectedIndex = -1;
        if (it->Class)
          expectedIndex = it->Class->getLegRunnerIndex(i);

        if (expectedIndex>=0 && expectedIndex != r->getMultiIndex()) {
          int baseLeg = it->Class->getLegRunner(i);
          it->setRunner(baseLeg, r->getMultiRunner(0), true);
          teamCorrected = true;
        }
      }
    }

    for (size_t i = 0; i < it->Runners.size(); i++) {
      pRunner r = it->Runners[i];
      if (r != 0) {
        if (usedInTeam[r->Id]) {
          it->Runners[i] = 0; // Reset duplicate runners
          it->updateChanged();
          teamCorrected = true;
          if (r->tInTeam == &*it)
            r->tInTeam = 0;
        }
        else
          usedInTeam[r->Id] = true;
      }
    }
  }
  usedInTeam.clear();

  if (teamCorrected) {
    for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
      it->apply(true, 0, false);
    }
  }

  reEvaluateAll(set<int>(), false);
  vector<wstring> out;
  checkChanged(out);
  assert(out.empty());

  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->correctionNeeded) {
      it->createMultiRunner(true, true);
      if (it->tInTeam && it->Class) {
        pTeam t = it->tInTeam;
        int nr = min(int(it->Class->getNumStages())-1, int(it->multiRunner.size()));
        t->setRunner(0, &*it, true);
        for (int k=0; k<nr; k++)
          t->setRunner(k+1, it->multiRunner[k], true);
        t->updateChanged();
        t->synchronize();
      }
      it->synchronizeAll();
    }
  }

  return true;
}

bool oEvent::reConnect(char *errorMsg256)
{
  if (HasDBConnection)
    return true;

  if (isThreadReconnecting()){
    strcpy_s(errorMsg256, 256, "Synkroniseringsfel.");
    return false;
  }

#ifdef BUILD_DB_DLL
  if (!msReConnect) {
    strcpy_s(errorMsg256, 256, "Inte ansluten mot meosdb.dll");
    return false;
  }
#endif

  if (msReConnect()) {
    HasDBConnection = true;
    HasPendingDBConnection = false;
    //synchronize changed objects
    for (list<oCard>::iterator it=oe->Cards.begin();
          it!=oe->Cards.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oClub>::iterator it=oe->Clubs.begin();
          it!=oe->Clubs.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oControl>::iterator it=oe->Controls.begin();
          it!=oe->Controls.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oCourse>::iterator it=oe->Courses.begin();
          it!=oe->Courses.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oClass>::iterator it=oe->Classes.begin();
        it!=oe->Classes.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oRunner>::iterator it=oe->Runners.begin();
        it!=oe->Runners.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oTeam>::iterator it=oe->Teams.begin();
        it!=oe->Teams.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    for (list<oFreePunch>::iterator it=oe->punches.begin();
        it!=oe->punches.end(); ++it)
      if (it->isChanged())
        it->synchronize(false);

    autoSynchronizeLists(true);

    return true;
  }

  msGetErrorState(errorMsg256);
  return false;
}

//Returns number of changed, non-saved elements.
int oEvent::checkChanged(vector<wstring> &out) const
{
  int changed=0;
  wchar_t bf[256];
  out.clear();

  for (list<oCard>::iterator it=oe->Cards.begin();
    it!=oe->Cards.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Card %d", it->cardNo);
      out.push_back(bf);
      it->synchronize();
    }

  for (list<oClub>::iterator it=oe->Clubs.begin();
    it!=oe->Clubs.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Club %ws", it->name.c_str());
      out.push_back(bf);
      it->synchronize();
    }

  for (list<oControl>::iterator it=oe->Controls.begin();
    it!=oe->Controls.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Control %d", it->Numbers[0]);
      out.push_back(bf);
      it->synchronize();
    }

  for (list<oCourse>::iterator it=oe->Courses.begin();
        it!=oe->Courses.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Course %s", it->Name.c_str());
      out.push_back(bf);
      it->synchronize();
    }

  for (list<oClass>::iterator it=oe->Classes.begin();
      it!=oe->Classes.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Class %s", it->Name.c_str());
      out.push_back(bf);
      it->synchronize();
    }

  for (list<oRunner>::iterator it=oe->Runners.begin();
      it!=oe->Runners.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Runner %s", it->getName().c_str());
      out.push_back(bf);
      it->synchronize();
    }
  for (list<oTeam>::iterator it=oe->Teams.begin();
      it!=oe->Teams.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Team %s", it->getName().c_str());
      out.push_back(bf);
      it->synchronize();
    }
  for (list<oFreePunch>::iterator it=oe->punches.begin();
      it!=oe->punches.end(); ++it)
    if (it->isChanged()) {
      changed++;
      swprintf_s(bf, L"Punch SI=%d, %d", it->CardNo, it->Type);
      out.push_back(bf);
      it->synchronize();
    }

  return changed;
}

bool oEvent::verifyConnection()
{
  if (!HasDBConnection)
    return false;

  if (isThreadReconnecting())
    return false;

#ifdef BUILD_DB_DLL
  if (!msMonitor)
    return false;
#endif

  if (!msMonitor(this)) {
    startReconnectDaemon();
    return false;
  }
  return true;
}

const string &oEvent::getServerName() const
{
  return serverName;
}

void oEvent::closeDBConnection()
{
  bool hadDB=HasDBConnection;

  if (isThreadReconnecting()) {
    //Don't know what to do?!
  }
  gdibase.setWaitCursor(true);
  if (HasDBConnection) {
    autoSynchronizeLists(true);
  }
  HasDBConnection=false;

  #ifdef BUILD_DB_DLL
    if (msResetConnection)
      msResetConnection();
  #else
    msResetConnection();
  #endif
  Id=0;

  if (!oe->empty() && hadDB) {
    save();
    Name+=L" (Lokal kopia från: " + gdibase.widen(serverName) + L")";
    wstring cn = currentNameId + L"." + gdibase.widen(serverName) + L".meos";
    getUserFile(CurrentFile, cn.c_str());
    serverName.clear();
    save();
    gdibase.setWindowTitle(Name);
  }
  else serverName.clear();

  gdibase.setWaitCursor(false);
}

void oEvent::listConnectedClients(gdioutput &gdi)
{
  gdi.addString("", 1, "Anslutna klienter:");
  char bf[256];
  gdi.fillRight();
  gdi.pushX();
  int x=gdi.getCX();
  for (size_t k=0;k<connectedClients.size();k++) {
    sprintf_s(bf, "%d.", k+1);
    gdi.addStringUT(0, bf);
    gdi.addStringUT(gdi.getCY(), x+30, 0, connectedClients[k]);
    gdi.popX();
    gdi.dropLine();
  }
  gdi.fillRight();
  gdi.dropLine(2);
}

DWORD oEvent::clientCheckSum() const
{
  DWORD cs=0;
  for (size_t k=0;k<connectedClients.size();k++)
    for (size_t i=0;i<connectedClients[k].length();i++)
      cs += BYTE(connectedClients[k][i]) << ((i*5+k*7)%24);

  return cs;
}

void oEvent::validateClients()
{
  currentClientCS = clientCheckSum();
}

bool oEvent::hasClientChanged() const
{
  return currentClientCS!=clientCheckSum();
}

void oEvent::dropDatabase()
{
  bool dropped = false;
  if (HasDBConnection) {
    HasDBConnection=false;

    dropped = msDropDatabase(this)!=0;
  }
  else throw std::exception("Inte ansluten");

  if (!dropped) {
    char bf[256];
    msGetErrorState(bf);
    if (strlen(bf)>0)
      throw std::exception(bf);

    throw std::exception("Operationen misslyckades. Orsak okänd.");
  }
  clear();
}

