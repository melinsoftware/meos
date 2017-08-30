#pragma once

#include <vector>
#include <map>
#include "inthashmap.h"
#include "oclub.h"
#include <hash_set>
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

const int baseNameLength=32;

//Has 0-clearing constructor. Must not contain any
//dynamic data etc.
struct RunnerDBEntryV1 {
  RunnerDBEntryV1();

  char name[baseNameLength];
  int cardNo;
  int clubNo;
  char national[3];
  char sex;
  short int birthYear;
  short int reserved;
};

struct RunnerDBEntry {
  // Init from old struct
  void init(const RunnerDBEntryV1 &dbe);
  RunnerDBEntry();

  /** Binary compatible with V1*/
  char name[baseNameLength];
  int cardNo;
  int clubNo;
  char national[3];
  char sex;
  short int birthYear;
  short int reserved;
  /** End of V1*/

  __int64 extId;

  void getName(string &name) const;
  void setName(const char *name);

  string getGivenName() const;
  string getFamilyName() const;

  string getNationality() const;
  int getBirthYear() const {return birthYear;}
  string getSex() const;

  __int64 getExtId() const;
  void setExtId(__int64 id);

  bool isRemoved() const {return (reserved & 1) == 1;}
  void remove() {reserved |= 1;}
};

typedef vector<RunnerDBEntry> RunnerDBVector;

class oDBRunnerEntry;
class oClass;
class oDBClubEntry;

class RunnerDB {
private:
  oEvent *oe;

  Table *runnerTable;
  Table *clubTable;

  bool check(const RunnerDBEntry &rde) const;

  intkeymap<oClass *, __int64> runnerInEvent;

  /** Init name hash lazy */
  void setupNameHash() const;
  void setupIdHash() const;
  void setupCNHash() const;

  vector<RunnerDBEntry> rdb;
  vector<oDBClubEntry> cdb;
  vector<oDBRunnerEntry> oRDB;

  // Runner card hash
  inthashmap rhash;

  // Runner id hash
  mutable intkeymap<int, __int64> idhash;

  // Club id hash
  inthashmap chash;

  // Last known free index
  int freeCIx;

  // Name hash
  mutable multimap<string, int> nhash;

  // Club name hash
  mutable multimap<string, int> cnhash;

  static void canonizeSplitName(const string &name, vector<string> &split);

  bool loadedFromServer;

  /** Date when database was updated. The format is YYYYMMDD */
  int dataDate;

  /** Time when database was updated. The format is HH:MM:SS */
  int dataTime;

  void fillClubs(vector< pair<string, size_t> > &out) const;

public:

  void generateRunnerTableData(Table &table, oDBRunnerEntry *addEntry);
  void generateClubTableData(Table &table, oClub *addEntry);

  void refreshRunnerTableData(Table &table);
  void refreshClubTableData(Table &table);
  void refreshTables();

  Table *getRunnerTB();
  Table *getClubTB();

  void hasEnteredCompetition(__int64 extId);

  void releaseTables();

  /** Get the date, YYYY-MM-DD HH:MM:SS when database was updated */
  string getDataDate() const;
  /** Set the date YYYY-MM-DD HH:MM:SS when database was updated */
  void setDataDate(const string &date);


  /** Returns true if the database was loaded from server */
  bool isFromServer() const {return loadedFromServer;}

  /** Prepare for loading runner from server*/
  void prepareLoadFromServer(int nrunner, int nclub);

  const vector<RunnerDBEntry>& getRunnerDB() const;
  const vector<oDBClubEntry>& getClubDB() const;

  void clearRunners();
  void clearClubs();

  /** Add a club. Create a new Id if necessary*/
  int addClub(oClub &c, bool createNewId);
  RunnerDBEntry *addRunner(const char *name, __int64 extId,
                           int club, int card);

  oDBRunnerEntry *addRunner();
  oClub *addClub();

  RunnerDBEntry *getRunnerByIndex(size_t index) const;
  RunnerDBEntry *getRunnerById(__int64 extId) const;
  RunnerDBEntry *getRunnerByCard(int card) const;
  RunnerDBEntry *getRunnerByName(const string &name, int clubId,
                                 int expectedBirthYear) const;

  bool getClub(int clubId, string &club) const;
  oClub *getClub(int clubId) const;

  oClub *getClub(const string &name) const;

  void saveClubs(const char *file);
  void saveRunners(const char *file);
  void loadRunners(const char *file);
  void loadClubs(const char *file);

  void updateAdd(const oRunner &r, map<int, int> &clubIdMap);

  void importClub(oClub &club, bool matchName);
  void compactifyClubs();

  void getAllNames(vector<string> &givenName, vector<string> &familyName);
  RunnerDB(oEvent *);
  ~RunnerDB(void);
  friend class oDBRunnerEntry;
  friend class oDBClubEntry;
};

class oDBRunnerEntry : public oBase {
private:
  RunnerDB *db;
  int index;
protected:
  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;
  int getDISize() const {return 0;}
  void changedObject() {}
public:

  int getIndex() const {return index;}
  void init(RunnerDB *db_, int index_) {db=db_, index=index_; Id = index;}

  const RunnerDBEntry &getRunner() const;

  void addTableRow(Table &table) const;
  bool inputData(int id, const string &input,
                 int inputId, string &output, bool noUpdate);
  void fillInput(int id, vector< pair<string, size_t> > &out, size_t &selected);

  oDBRunnerEntry(oEvent *oe);
  virtual ~oDBRunnerEntry();

  void remove();
  bool canRemove() const;

  string getInfo() const {return "Database Runner";}
};


class oDBClubEntry : public oClub {
private:
  int index;
  RunnerDB *db;
public:
  oDBClubEntry(oEvent *oe, int id, int index, RunnerDB *db);
  oDBClubEntry(const oClub &c, int index, RunnerDB *db);

  int getTableId() const;
  virtual ~oDBClubEntry();
  void remove();
  bool canRemove() const;
};
