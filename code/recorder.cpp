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
#include "stdafx.h"

#include <cassert>
#include "recorder.h"
#include "meosexception.h"
#include "meos_util.h"

Recorder::Recorder() {
#ifdef _DEBUG
  isRecording = true;
#else
  isRecording = false;
#endif
}

Recorder::~Recorder() {

}

void Recorder::record(const string &cmd) {
  if (isRecording)
    records.push_back(cmd);
}

void Recorder::saveRecordings(const string &file) {
  ofstream fout(file.c_str(), ios::trunc|ios::out);
  fout << "void run() {" << endl;
  for (list<string>::iterator it = records.begin(); it != records.end(); ++it) {
    if (it->find_first_of("\n") == string::npos)
      fout << "  " << *it << endl;
    else {
      vector<string> splt;
      split(*it, "\n", splt);
      int ls = splt[0].find_last_of('\"');
      if (splt.size() > 1 && ls != string::npos) {
        fout << "  " << splt[0] << "\"" << endl;      
        int ind = splt[0].length() + 2 - ls;
        for (size_t j = 1; j < splt.size(); j++) {
          for (int k = 0; k < ind; k++)
            fout << " ";
          fout << "\"";
          fout << splt[j];
          if (j + 1 == splt.size())
            fout << endl;
          else
            fout << "\"" << endl;
        }
      }
    }
  }
  fout << "}" << endl;
}

