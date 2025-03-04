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
#include "meosexception.h"
#include "meos_util.h"
#include "MeosSQL.h"
#include "generalresult.h"
#include "metalist.h"
#include "image.h"

#include "meos.h"
#include <cassert>

extern Image image;

bool oEvent::connectToServer()
{
  if (isThreadReconnecting())
    return false;

  return true;
}

void oEvent::startReconnectDaemon()
{
  if (isThreadReconnecting() || TabAuto::hasActiveReconnectionMachine())
    return;

  string err;
  sqlConnection->getErrorMessage(err);

  MySQLReconnect msqlr(lang.tl("warning:dbproblem#" + err));
  msqlr.interval=5;
  hasPendingDBConnection = true;
  TabAuto::tabAutoAddMachinge(msqlr);

  gdibase.setDBErrorState(false);
  gdibase.setWindowTitle(oe->getTitleName());
  isConnectedToServer = false;
  if (!isReadOnly()) {
    // Do not show in read only-mode
    gdibase.delayAlert(L"warning:dbproblem#" + gdibase.widen(err));
  }
}

bool oEvent::msSynchronize(oBase *ob)
{
  if (!hasDBConnection() && !hasPendingDBConnection)
    return true;

  int ret = sqlConnection->syncRead(false, ob);

  string err;
  if (sqlConnection->getErrorMessage(err))
    gdibase.addInfoBox("sqlerror", gdibase.widen(err), L"Databasvarning", BoxStyle::HeaderWarning, 15000);

  if (ret==0) {
    verifyConnection();
    return false;
  }

  if (typeid(*ob)==typeid(oTeam)) {
    static_cast<pTeam>(ob)->apply(ChangeType::Quiet, nullptr);
  }

  if (ret==1) {
    gdibase.removeFirstInfoBox("sqlwarning");
    gdibase.addInfoBox("sqlwarning", L"Varning: ändringar i X blev överskrivna#" + ob->getInfo(), L"Databasvarning", BoxStyle::HeaderWarning, 5000);
  }
  return ret!=0;
}

bool oEvent::synchronizeList(initializer_list<oListId> types) {
  if (!hasDBConnection())
    return true;

  unsigned int ct = GetTickCount();
  if (ct < lastTimeConsistencyCheck || (ct - lastTimeConsistencyCheck) > 1000 * 60) {
    // Make autoSynch instead
    autoSynchronizeLists(true);
    return true;
  }

  sqlConnection->clearReadTimes();
  msSynchronize(this);
  resetSQLChanged(true, false);

  vector<oListId> toSync;
  bool hasCard = false;
  for (oListId t : types) {
    if (t == oListId::oLRunnerId && !hasCard) {
      hasCard = true;
      toSync.push_back(oListId::oLCardId); // Make always card sync before runners
    }
    else if (t == oListId::oLCardId) {
      if (hasCard)
        continue;
      else
        hasCard = true;
    }
    toSync.push_back(t);
  }

  for (oListId t : toSync) {
    if (!sqlConnection->synchronizeList(this, t)) {
      verifyConnection();
      return false;
    }

    if (t == oListId::oLPunchId)
      advanceInformationPunches.clear();
  }

  reinitializeClasses();
  reEvaluateChanged();
  return true;
}

bool oEvent::synchronizeList(oListId id, bool preSyncEvent, bool postSyncEvent) {
  if (!hasDBConnection())
    return true;

  if (postSyncEvent) {
    unsigned int ct = GetTickCount();
    if (ct < lastTimeConsistencyCheck || (ct - lastTimeConsistencyCheck) > 1000 * 60) {
      // Make autoSynch instead
      autoSynchronizeLists(true);
      return true;
    }
  }

  if (preSyncEvent && postSyncEvent && id == oListId::oLRunnerId) {
    sqlConnection->clearReadTimes();
    synchronizeList(oListId::oLCardId, true, false);
    preSyncEvent = false;
  }

  if (preSyncEvent) {
    msSynchronize(this);
    resetSQLChanged(true, false);
  }

  if ( !sqlConnection->synchronizeList(this, id) ) {
    verifyConnection();
    return false;
  }

  if (id == oListId::oLPunchId)
    advanceInformationPunches.clear();

  if (postSyncEvent) {
    reinitializeClasses();
    reEvaluateChanged();
    return true;
  }

  return true;
}

bool oEvent::checkDatabaseConsistency(bool force) {
  if (!hasDBConnection())
    return false;

  if (!force) {
    unsigned int ct = GetTickCount();
    if (ct < lastTimeConsistencyCheck || (ct - lastTimeConsistencyCheck) > 1000 * 60) {
      lastTimeConsistencyCheck = ct;
    }
    else
      return false; // Skip check
  }

  sqlConnection->checkConsistency(this, force);
  return true; // Did check
}


bool oEvent::needReEvaluate() {
  return sqlRunners.changed |
         sqlClasses.changed |
         sqlCourses.changed |
         sqlControls.changed |
         sqlCards.changed |
         sqlTeams.changed;
}

void oEvent::resetSQLChanged(bool resetAllTeamsRunners, bool cleanClasses) {
  if (empty())
    return;
  sqlRunners.changed = false;
  sqlClasses.changed = false;
  sqlCourses.changed = false;
  sqlControls.changed = false;
  sqlClubs.changed = false;
  sqlCards.changed = false;
  sqlPunches.changed = false;
  sqlTeams.changed = false;

  if (resetAllTeamsRunners) {
    for (auto &r : Runners) 
      r.sqlChanged = false;
    for (auto &t : Teams) 
      t.sqlChanged = false;
  }
  if (cleanClasses) {
    // This data is used to redraw lists/speaker etc.
    for (list<oClass>::iterator it=oe->Classes.begin();
      it!=oe->Classes.end(); ++it) {
      it->sqlChangedControlLeg.clear();
      it->sqlChangedLegControl.clear();
    }
    globalModification = false;
  } 
}

bool BaseIsRemoved(const oBase &ob){return ob.isRemoved();}

namespace {
  bool isSet(int mask, oListId id) {
    return (mask & int(id)) != 0;
  }
}
//Returns true if data is changed.
bool oEvent::autoSynchronizeLists(bool synchPunches)
{
  if (!hasDBConnection())
    return false;

  bool changed=false;
  string ot;

  int mask = sqlConnection->getModifiedMask(*this);
  if (mask != 0)
    sqlConnection->clearReadTimes();

  // Reset change data and store update status on objects
  // (which might be incorrectly changed during sql update)
  resetSQLChanged(true, false);

  //Synchronize ourself
  if (isSet(mask, oListId::oLEventId)) {
    ot=sqlUpdated;
    msSynchronize(this);
    if (sqlUpdated!=ot) {
      changed=true;
      gdibase.setWindowTitle(getTitleName());
    }
  }

  int dr = dataRevision;

  //Controls
  if (isSet(mask, oListId::oLControlId)) 
    synchronizeList(oListId::oLControlId, false, false);
  
  //Courses
  if (isSet(mask, oListId::oLCourseId)) 
    synchronizeList(oListId::oLCourseId, false, false);

  //Classes
  if (isSet(mask, oListId::oLClassId)) 
    synchronizeList(oListId::oLClassId, false, false);

  //Clubs
  if (isSet(mask, oListId::oLClubId)) 
    synchronizeList(oListId::oLClubId, false, false);

  //Cards
  if (isSet(mask, oListId::oLCardId)) 
    synchronizeList(oListId::oLCardId, false, false);

  //Runners
  if (isSet(mask, oListId::oLRunnerId)) 
    synchronizeList(oListId::oLRunnerId, false, false);

  //Teams
  if (isSet(mask, oListId::oLTeamId)) 
    synchronizeList(oListId::oLTeamId, false, false);

  if (isSet(mask, oListId::oLPunchId)) 
    synchronizeList(oListId::oLPunchId, false, false);

  checkDatabaseConsistency(false);

  if (changed || dr != dataRevision) {
    if (needReEvaluate())
      reEvaluateChanged();

    reCalculateLeaderTimes(0);
    //Restore changed staus on object that might have been changed
    //during sql update, due to partial updates
    return true;
  }

  return false;
}


bool oEvent::connectToMySQL(const string &server, const string &user, const string &pwd, int port)
{
  if (!connectToServer())
    return false;

  MySQLServer=server;
  MySQLPassword=pwd;
  MySQLPort=port;
  MySQLUser=user;

  sqlConnection = make_shared<MeosSQL>();

  //Delete non-server competitions.
  list<CompetitionInfo> saved;
  list<CompetitionInfo>::iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty())
      saved.push_back(*it);
  }
  cinfo = saved;

  if (!sqlConnection->listCompetitions(this, false)) {
    string err;
    sqlConnection->getErrorMessage(err);
    gdibase.alert(err);
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

      if (ans == gdioutput::AskAnswer::AnswerCancel)
        return false;
      else if (ans == gdioutput::AskAnswer::AnswerNo) {
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
            CurrentNameId[63] = 0;
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

  isConnectedToServer = false;

  if ( !sqlConnection->openDB(this) ){
    string err;
    sqlConnection->getErrorMessage(err);
    string error = string("Kunde inte öppna databasen (X).#") + err;
    throw std::exception(error.c_str());
  }

  if ( !sqlConnection->synchronizeUpdate(this) ) {
    string err;
    sqlConnection->getErrorMessage(err);
    string error = string("Kunde inte ladda upp tävlingen (X).#") + err;
    throw std::exception(error.c_str());
  }

  OpFailStatus stat = (OpFailStatus)sqlConnection->uploadRunnerDB(this);

  if (stat == opStatusFail) {
    string err;
    sqlConnection->getErrorMessage(err);
    string error = string("Kunde inte ladda upp löpardatabasen (X).#") + err;
    throw meosException(error);
  }
  else if (stat == opStatusWarning) {
    string err;
    sqlConnection->getErrorMessage(err);
    gdibase.addInfoBox("", wstring(L"Kunde inte ladda upp löpardatabasen (X).#") + lang.tl(err), L"", BoxStyle::Header, 5000);
  }

  set<uint64_t> img;
  listContainer->getUsedImages(img);
  if (!img.empty()) {
    for (auto imgId : img) {
      wstring fileName = image.getFileName(imgId);
      auto rawData = image.getRawData(imgId);
      sqlConnection->storeImage(imgId, fileName, rawData);
    }
  }

  isConnectedToServer = true;

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

  isConnectedToServer = false;

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

  if (!sqlConnection->listCompetitions(this, false)) {
    string err;
    sqlConnection->getErrorMessage(err);
    throw meosException(err);
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
  if ( !sqlConnection->openDB(this) ) {
    string err;
    sqlConnection->getErrorMessage(err);
    throw meosException(err);
  }

  updateFreeId();

  image.clearLoaded();

  // Publish list of images (no loading)
  vector<pair<wstring, uint64_t>> img;
  sqlConnection->enumerateImages(img);
  for (auto& i : img) {
    image.addImage(i.second, i.first);
  }

  isConnectedToServer = false;

  openRunnerDatabase(currentNameId.c_str());

  int ret = sqlConnection->syncRead(false, this);
  if (ret == 0) {
    string err;
    sqlConnection->getErrorMessage(err);

    err = string("Kunde inte öppna tävlingen (X)#") + err;
    throw meosException(err);
  }
  else if (ret == 1) {
    // Warning
    string err;
    sqlConnection->getErrorMessage(err);
    wstring info = L"Databasvarning: X#" + lang.tl(err);
    gdibase.addInfoBox("sqlerror", info, L"Databasvarning", BoxStyle::HeaderWarning, 15000);
  }

  // Cache database locally
  saveRunnerDatabase(currentNameId.c_str(), false);

  isConnectedToServer = true;

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
          it->Runners[i] = nullptr; // Reset duplicate runners
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
      if (!it->isRemoved()) {
        it->apply(oBase::ChangeType::Update, nullptr);
        it->synchronize(true);
      }
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

void oEvent::loadImage(uint64_t id) const {
  if (image.hasImage(id))
    return;
  if (sqlConnection && isConnectedToServer) {
    wstring fn;
    vector<uint8_t> data;
    if (sqlConnection->getImage(id, fn, data) == OpFailStatus::opStatusOK)
      image.provideFromMemory(id, fn, data);
  }
}

void oEvent::saveImage(uint64_t id) const {
  if (sqlConnection && isConnectedToServer) {
    wstring fn = image.getFileName(id);
    auto &data = image.getRawData(id);
    sqlConnection->storeImage(id, fn, data);
  }
}

bool oEvent::reConnectRaw() {
  if (!sqlConnection)
    return false;
  return sqlConnection->reConnect();
}

bool oEvent::sqlRemove(oBase *obj) {
  if (!sqlConnection)
    return false;
  return sqlConnection->remove(obj);
}

MeosSQL &oEvent::sql() {
  if (!sqlConnection)
    throw meosException("Internal SQL error");

  return *sqlConnection;
}

bool oEvent::reConnect(string &err)
{
  if (hasDBConnection())
    return true;

  if (isThreadReconnecting()){
    err = "Synkroniseringsfel.";
    return false;
  }

  if (sqlConnection->reConnect()) {
    isConnectedToServer = true;
    hasPendingDBConnection = false;
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

  sqlConnection->getErrorMessage(err);
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
      swprintf_s(bf, L"Punch SI=%d, %d", it->CardNo, it->type);
      out.push_back(bf);
      it->synchronize();
    }

  return changed;
}

bool oEvent::verifyConnection()
{
  if (!hasDBConnection())
    return false;

  if (isThreadReconnecting())
    return false;

  if (!sqlConnection->checkConnection(this)) {
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
  bool hadDB=hasDBConnection();

  if (isThreadReconnecting()) {
    //Don't know what to do?!
  }
  gdibase.setWaitCursor(true);
  if (hasDBConnection()) {
    autoSynchronizeLists(true);
  }
  isConnectedToServer = false;

  
  sqlConnection->closeDB();
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
  for (int k=0;k<connectedClients.size();k++) {
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
  if (hasDBConnection()) {
    isConnectedToServer = false;

    dropped = sqlConnection->dropDatabase(this)!=0;
  }
  else throw std::exception("Not connected");

  if (!dropped) {
    string err;
    sqlConnection->getErrorMessage(err);
    if (!err.empty())
      throw meosException(err);

    throw meosException("Operation failed. Unknown reason");
  }
  clear();
}

