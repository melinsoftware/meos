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

class gdioutput;
class oEvent;
class BaseInfo;

#include <list>

class Recorder {
private:
  list<string> records;

  // True if commands should be recorded
  bool isRecording;

public:

  bool recording() const {return isRecording;}
  void saveRecordings(const string &file);
  void clearRecordings() {records.clear();}
  void record(const string &cmd);
  
  Recorder();
  ~Recorder();
};
