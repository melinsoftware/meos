#pragma once
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

#include "TabAuto.h"

class InfoCompetition;

class OnlineResults :
  public AutoMachine
{
protected:
  wstring file;
  wstring url;
  wstring passwd;
  wstring prefix;
  int cmpId;
  set<int> classes;
  set<int> controls;

  enum class DataType {
    MOP20 = 1,
    MOP10 = 2,
    IOF3 = 10,
    IOF2 = 11
  };

  DataType dataType;
  bool zipFile;
  bool includeTotal;
  bool includeCourse;
  bool sendToURL;
  bool sendToFile;
  mutable InfoCompetition *infoServer;
  wstring exportScript;
  int exportCounter;
  void enableURL(gdioutput &gdi, bool state);
  void enableFile(gdioutput &gdi, bool state);

  wstring getExportFileName() const;
  int bytesExported;
  DWORD lastSync;

  vector<string> errorLines;
  void formatError(gdioutput &gdi);

public:

  int processButton(gdioutput &gdi, ButtonInfo &bi);
  InfoCompetition &getInfoServer() const;

  void save(oEvent &oe, gdioutput &gdi);
  void settings(gdioutput &gdi, oEvent &oe, bool created);
  OnlineResults *clone() const {return new OnlineResults(*this);}
  void status(gdioutput &gdi);
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast);
  OnlineResults();
  ~OnlineResults();
  friend class TabAuto;
};
