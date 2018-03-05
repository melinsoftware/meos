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
#include "meos_util.h"

class meosException : public std::exception {
  wstring wideMessage;
  static const char *narrow(const wstring &msg) {
    static string nmsg(msg.begin(), msg.end());
    return nmsg.c_str();
  }
public:
  meosException(const wstring &wmsg) : std::exception(narrow(wmsg)), wideMessage(wmsg) {
    
  }
  meosException(const string &msg) : std::exception(msg.c_str()) {
    string2Wide(msg, wideMessage);
  }
  meosException(const char *msg) : std::exception(msg) {
    string2Wide(string(msg), wideMessage);
  }
  meosException() : std::exception() {}

  wstring wwhat() const{
    return wideMessage;
  }
};

