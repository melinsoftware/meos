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

// csvparser.h: interface for the csvparser class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <map>

class oEvent;
struct SICard;
class  ImportFormats;

class CSVLineWrapper;

struct PunchInfo {
  int code;
  int card;
  int time;
  char date[28];
};


struct TeamLineup {
  struct TeamMember {
    wstring name;
    wstring club;
    int cardNo;
    wstring course;
    wstring cls;
  };

  wstring teamName;
  wstring teamClass;
  wstring teamClub;
  vector<TeamMember> members;
};

class csvparser
{
protected:
  std::ofstream fout;
  std::ifstream fin;

  int LineNumber;
  string ErrorMessage;

  // Returns true if a SI-manager line is identified
  bool checkSimanLine(const oEvent &oe, const CSVLineWrapper &sp, SICard &cards);

  // Check and setup header for SIConfig import
  void checkSIConfigHeader(const CSVLineWrapper &sp);

  // Return true if SIConfig line was detected 
  bool checkSIConfigLine(const oEvent &oe, const CSVLineWrapper &sp, SICard &card);

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
  const wchar_t *getSIC(SIConfigFields sic, const CSVLineWrapper&sp) const;

  void parseUnicode(const wstring &file, list<vector<wstring>> &data);

  // Check and process a punch line
  static int selectPunchIndex(const wstring &competitionDate, const CSVLineWrapper &sp,
                              int &cardIndex, int &timeIndex, int &dateIndex,
                              wstring &processedTime, wstring &date);

public:

  static void convertUTF(const wstring &file);

  void parse(const wstring &file, list< vector<wstring> > &dataOutput);

  void importTeamLineup(const wstring &file,
                        const map<wstring, int> &classNameToNumber,
                        vector<TeamLineup> &teams);

  bool openOutput(const wstring &file, bool writeUTF = false);
  bool closeOutput();

  bool outputRow(const vector<string> &out);
  bool outputRow(const string &row);

  int nimport;
  bool importOCAD_CSV(oEvent &oe, const wstring &file, bool addClasses);
  bool importOS_CSV(oEvent &oe, const wstring &file);
  bool importRAID(oEvent &oe, const wstring &file);
  bool importOE_CSV(oEvent &oe, const wstring &file);

  bool importPunches(const oEvent &oe, const wstring &file,
                     vector<PunchInfo> &punches);

  bool importCards(const oEvent &oe, const wstring &file,
                   vector<SICard> &punches);

  int importRanking(oEvent &oe, const wstring &file, vector<wstring> & problems);

  static int split(char *line, vector<char *> &split);
  static int split(wchar_t *line, vector<wchar_t *> &split);

  enum class CSV {
    NoCSV,
    Unknown,
    RAID,
    OE,
    OS,
  };

  static CSV iscsv(const wstring &file);

  csvparser();
  virtual ~csvparser();

};
