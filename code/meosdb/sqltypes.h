#pragma once
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

enum OpFailStatus {
  opStatusOK = 2,
  opStatusFail = 0,
  opStatusWarning = 1,
  opUnreachable = -1,
};

#ifndef BUILD_DB_DLL

class oEvent;
class oBase;

extern "C"{

#define MEOSDB_API
  int MEOSDB_API getMeosVersion();
  bool MEOSDB_API msSynchronizeList(oEvent *, int lid);
  int MEOSDB_API msSynchronizeUpdate(oBase *);
  int MEOSDB_API msSynchronizeRead(oBase *obj);
  int MEOSDB_API msRemove(oBase *obj);
  int MEOSDB_API msMonitor(oEvent *oe);
  int MEOSDB_API msUploadRunnerDB(oEvent *oe);
  int MEOSDB_API msOpenDatabase(oEvent *oe);
  int MEOSDB_API msDropDatabase(oEvent *oe);
  int MEOSDB_API msConnectToServer(oEvent *oe);
  bool MEOSDB_API msGetErrorState(char *msgBuff);
  bool MEOSDB_API msResetConnection();
  bool MEOSDB_API msReConnect();
  int MEOSDB_API msListCompetitions(oEvent *oe);

  int getListMask(oEvent &oe);
}

bool repairTables(const string &db, vector<string> &output);

#endif
