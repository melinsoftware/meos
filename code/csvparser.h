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

// csvparser.h: interface for the csvparser class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CSVPARSER_H__FD04656A_1D2A_4E6C_BE23_BD66052E276E__INCLUDED_)
#define AFX_CSVPARSER_H__FD04656A_1D2A_4E6C_BE23_BD66052E276E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <vector>
#include <map>

class oEvent;
struct SICard;
class  ImportFormats;

struct PunchInfo {
  int code;
  int card;
  int time;
  char date[28];
};


struct TeamLineup {
  struct TeamMember {
    string name;
    string club;
    int cardNo;
    string course;
    string cls;
  };

  string teamName;
  string teamClass;
  string teamClub;
  vector<TeamMember> members;
};

class csvparser
{
protected:
  ofstream fout;
  ifstream fin;

  int LineNumber;
  string ErrorMessage;

  // Returns true if a SI-manager line is identified
  bool checkSimanLine(const oEvent &oe, const vector<char *> &sp, SICard &cards);

  // Check and setup header for SIConfig import
  void checkSIConfigHeader(const vector<char *> &sp);

  // Return true if SIConfig line was detected 
  bool checkSIConfigLine(const oEvent &oe, const vector<char *> &sp, SICard &card);

  enum SIConfigFields {
    sicSIID,
    sicCheck,
    sicCheckTime,
    sicCheckDOW,
    sicStart,
    sicStartTime,
    sicStartDOW,
    sicFinish,
    sicFinishTime,
    sicFinishDOW,
    sicNumPunch,
    sicRecordStart,
    sicFirstName,
    sicLastName,
  };

  map<SIConfigFields, int> siconfigmap;
  const char *getSIC(SIConfigFields sic, const vector<char *> &sp) const;

  // Check and process a punch line
  static int selectPunchIndex(const string &competitionDate, const vector<char *> &sp, 
                              int &cardIndex, int &timeIndex, int &dateIndex,
                              string &processedTime, string &date);

public:
  void parse(const string &file, list< vector<string> > &dataOutput);

  void importTeamLineup(const string &file,
                        const map<string, int> &classNameToNumber,
                        vector<TeamLineup> &teams);

  bool openOutput(const char *file);
  bool closeOutput();
  bool OutputRow(vector<string> &out);
  bool OutputRow(const string &row);

  int nimport;
  bool ImportOCAD_CSV(oEvent &event, const char *file, bool addClasses);
  bool ImportOS_CSV(oEvent &event, const char *file);
  bool ImportRAID(oEvent &event, const char *file);

  bool importPunches(const oEvent &oe, const char *file,
                     vector<PunchInfo> &punches);

  bool importCards(const oEvent &oe, const char *file,
                   vector<SICard> &punches);

  int split(char *line, vector<char *> &split);

  bool ImportOE_CSV(oEvent &event, const char *file);
  int iscsv(const char *file);
  csvparser();
  virtual ~csvparser();

};

#endif // !defined(AFX_CSVPARSER_H__FD04656A_1D2A_4E6C_BE23_BD66052E276E__INCLUDED_)
