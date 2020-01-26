/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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


// meosdb.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#ifdef c
  #include "meosdb.h"
#else
  #define MEOSDB_API
#endif

#include <iostream>
#include <iomanip>

#include <string>
#include <list>
#include <fstream>

#include "MeosSQL.h"
#include "../meos_util.h"

using namespace std;

#include "../oRunner.h"
#include "../oEvent.h"
#include "../Localizer.h"

#include <typeinfo.h>

#ifdef BUILD_DB_DLL
  HINSTANCE hInst=0;
  Localizer lang;
#endif


int MEOSDB_API getMeosVersion()
{
  return getMeosBuild();
}

MeosSQL msql;
static int nSynchList = 0;
static int nSynchEnt = 0;

int getListMask(oEvent &oe) {
  return msql.getModifiedMask(oe);
}

void resetSynchTimes() {
  msql.clearReadTimes();
}

bool MEOSDB_API msSynchronizeList(oEvent *oe, oListId lid)
{
  nSynchList++;
  if (nSynchList % 100 == 99)
    OutputDebugString(L"Synchronized 100 lists\n");
  bool ret = false;
  switch (lid) {
  case oListId::oLRunnerId:
    ret = msql.syncListRunner(oe);
    break;
  case oListId::oLClassId:
    ret = msql.syncListClass(oe);
    break;
  case oListId::oLCourseId:
    ret = msql.syncListCourse(oe);
    break;
  case oListId::oLControlId:
    ret = msql.syncListControl(oe);
    break;
  case oListId::oLClubId:
    ret = msql.syncListClub(oe);
    break;
  case oListId::oLCardId:
    ret = msql.syncListCard(oe);
    break;
  case oListId::oLPunchId:
    ret = msql.syncListPunch(oe);
    break;
  case oListId::oLTeamId:
    ret = msql.syncListTeam(oe);
    break;
  }

  msql.processMissingObjects();
  return ret;
}

int MEOSDB_API msSynchronizeUpdate(oBase *obj)
{
  if (typeid(*obj)==typeid(oRunner)){
    return msql.syncUpdate((oRunner *) obj, false);
  }
  else if (typeid(*obj)==typeid(oClass)){
    return msql.syncUpdate((oClass *) obj, false);
  }
  else if (typeid(*obj)==typeid(oCourse)){
    return msql.syncUpdate((oCourse *) obj, false);
  }
  else if (typeid(*obj)==typeid(oControl)){
    return msql.syncUpdate((oControl *) obj, false);
  }
  else if (typeid(*obj)==typeid(oClub)){
    return msql.syncUpdate((oClub *) obj, false);
  }
  else if (typeid(*obj)==typeid(oCard)){
    return msql.syncUpdate((oCard *) obj, false);
  }
  else if (typeid(*obj)==typeid(oFreePunch)){
    return msql.syncUpdate((oFreePunch *) obj, false);
  }
  else if (typeid(*obj)==typeid(oEvent)){

    return msql.SyncUpdate((oEvent *) obj);
  }
  else if (typeid(*obj)==typeid(oTeam)){
    return msql.syncUpdate((oTeam *) obj, false);
  }
  return 0;
}

int MEOSDB_API msSynchronizeRead(oBase *obj)
{
  nSynchEnt++;
  if (nSynchEnt % 100 == 99)
    OutputDebugString(L"Synchronized 100 entities\n");

  return msql.syncRead(false, obj);
}

// Removes (marks it as removed) an entry from the database.
int MEOSDB_API msRemove(oBase *obj)
{
  return msql.Remove(obj);
}

// Checks the database connection, lists other connected components
// and register ourself in the database. The value oe=0 unregister us.
int MEOSDB_API msMonitor(oEvent *oe)
{
  return msql.checkConnection(oe);
}

// Tries to open the database defined by oe.
int MEOSDB_API msUploadRunnerDB(oEvent *oe)
{
  return msql.uploadRunnerDB(oe);
}

// Tries to open the database defined by oe.
int MEOSDB_API msOpenDatabase(oEvent *oe)
{
  return msql.openDB(oe);
}

// Tries to remove the database defined by oe.
int MEOSDB_API msDropDatabase(oEvent *oe)
{
  return msql.dropDatabase(oe);
}

// Tries to connect to the server defined by oe.
int MEOSDB_API msConnectToServer(oEvent *oe)
{
  return msql.listCompetitions(oe, false);
}

// Reloads competitions. Assumes a connection.
int MEOSDB_API msListCompetitions(oEvent *oe)
{
  return msql.listCompetitions(oe, true);
}

// Fills string msgBuff with the current error stage
bool MEOSDB_API msGetErrorState(char *msgBuff)
{
  return msql.getErrorMessage(msgBuff);
}

// Close database connection.
bool MEOSDB_API msResetConnection()
{
  return msql.closeDB();
}

// Try to reconnect to the database. Returns true if successful.
bool MEOSDB_API msReConnect()
{
  return msql.reConnect();
}



bool repairTables(const string &db, vector<string> &output) {
  return msql.repairTables(db, output);
}


#ifdef BUILD_DB_DLL

bool getUserFile(char *file, const char *in)
{
  throw 0;
  strcpy_s(file, 256, in);
  return true;
}

string MakeDash(string)
{
  throw 0;
  return "";
}

bool __cdecl GetRandomBit()
{
  throw 0;
  return true;
}

int __cdecl GetRandomNumber(int)
{
  throw 0;
  return 0;
}

#endif
