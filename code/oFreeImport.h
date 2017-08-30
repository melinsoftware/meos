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

#include <set>
#include <map>
#include <vector>
#include <string>

#include "oEvent.h"

class oWordDatabase;
typedef oWordDatabase* pWordDatabase;
typedef map<char, pWordDatabase> MapTable;

const char indexMapStart='a';
const char indexMapEnd='z';
const int hashSplitSize=16;
const int hashTableSize=indexMapEnd-indexMapStart+1;


class oFreeImport;

class oWordDatabase {
public:
  virtual char getType() const = 0;
  virtual const char *deserialize(const char *bf, const char *end) = 0;
  virtual char *serialize(char *bf) const = 0;
  virtual int serialSize() const = 0;
  virtual pWordDatabase split() {return this;}
  virtual void insert(const char *s) = 0;
  virtual bool lookup(const char *s) const = 0;
  virtual ~oWordDatabase() = 0 {}
};

class oWordDB : public oWordDatabase {
protected:
  char getType() const {return 1;}
  set<string> str;
public:
  const char *deserialize(const char *bf, const char *end);
  char *serialize(char *bf) const;
  int serialSize() const;
  pWordDatabase split();
  void insert(const char *s);
  bool lookup(const char *s) const;
  size_t size();
};


class oWordIndexHash : public oWordDatabase {
protected:
  char getType() const {return 2;}
  pWordDatabase hashTable[hashTableSize];
  MapTable unMapped;
  bool hashAll;
public:
  const char *deserialize(const char *bf, const char *end);
  char *serialize(char *bf) const;
  int serialSize() const;
  void insert(const char *s);
  bool lookup(const char *s) const;
  void clear();
  ~oWordIndexHash();
  oWordIndexHash(bool hashAll_);
};

class oWordList {
protected:
  oWordIndexHash wh;
public:
  void serialize(vector<char> &serial) const;
  void deserialize(const vector<char> &serial);

  void save(const char *file) const;
  void load(const char *file);

  void insert(const char *s);
  bool lookup(const char *s) const;
  ~oWordList();
  oWordList();
};

struct oEntryPerson {
  string name1;
  string name2;
  string club;
  int cardNo;
  void swap();
  int nameCount() const;
  oEntryPerson(const string &clb);
};

struct oEntryBlock {
  // Remember clear for additional data
  vector<oEntryPerson> ePersons;
  string eClub;
  string eClass;
  bool isClassSet;
  string eStartTime;
  bool canInsertName;
  int nClubsSet;
  const oFreeImport &freeImporter;

  int nameCount();

  void setClub(const char *s);
  void setClass(const char *s);
  void setCardNo(int c);
  void setStartTime(const char *s);
  void addPerson(const char *s, bool complete);
  vector<string> getPersons() const;
  void clear(const string &rulingClub, const string &rulingClass);

  // Return true if more clubs can be accepted
  bool acceptMoreClubs(int expectedNumRunners) const;

  bool expectMoreNames(int expectedNumRunners) const;
  bool needCard() const;
  bool hasClub() const {return !eClub.empty();}
  /** Return true if class explicitly set. May have a ruling class anyway*/
  bool hasClass() const {return isClassSet;}
  bool hasStartTime() const {return !eStartTime.empty();}
  bool hasName() {return !ePersons.empty() &&
                  !ePersons.front().name1.empty();}
  void cleanEntry(); //Remove bad data.
  oEntryBlock(const oFreeImport &importer);
  oEntryBlock(const oEntryBlock &eb);
  void operator=(const oEntryBlock &eb);
  int getNumPersons() const {return ePersons.size();}

  string getTeamName() const;
  string getName(int k) const;
  string getClub(int k) const;
  int getCard(int k) const;

  /** Make name complete */
  void completeName();
};

struct MatchPattern {
  short nName;
  short singleName;
  short nClass;
  short nClub;
  short nTime;
  short nCard;

  bool isClass(int level=2) const {return nClass>(nClub+nTime+nCard)+level;}
  bool isClub(int level=2) const {return nClub>(nClass+nTime+nCard)+level;}
  bool isTime(int level=2) const {return nTime>(nClass+nClub+nCard)+level;}
  bool isCard(int level=2) const {return nCard>(nClass+nTime+nTime)+level;}
  bool isName(int level=2) const {return nName>(nCard+nClass+nTime+nClub)+level;}
  bool isCompleteName() const {return singleName<int(nName*0.8);}
  MatchPattern():nClass(0), nClub(0), nTime(0),
    nCard(0), nName(0), singleName(0) {}
};


class oFreeImport {
protected:
  BYTE separator[256];
  oWordList givenDB;
  oWordList familyDB;
  oWordList clubDB;
  oWordList classDB;

  enum Types {
    None = -2,
    Unknown = -1,
    Club = 0,
    Class = 1,
    Time = 2,
    Card = 3,
    Name = 4
  };

  void analyzePart(char *part, const MatchPattern &ptrn, int nNamesPerPart,
                   oEntryBlock &entry, vector<oEntryBlock> &entries, bool allowNames);

  //bool analyze(vector<bool> &b, int &offset, int &delta) const;
  char *extractWord(char *&str, int &count) const;
  char *extractPart(char *&str, int &wordCount) const;
  char *extractLine(char *&str, int &count) const;
  bool isNumber(const char *str, int &number) const;

  bool isTime(const string &m) const;
  bool isName(const char *p) const;
  bool isCard(const char *p) const;
  bool isCrap(const char *p) const;

  bool loaded;

  int preAnalyzeRow(vector<char *> &p,
                    const vector<MatchPattern> &ptrn,
                    vector<int> &classified);

  // Runners per class in defined classes
  map<string, int> runnersPerClass;

  /** Must not return 0 */
  int getExpectedNumRunners(const string &cls) const;
  friend struct oEntryBlock;

  /** Check if a line is a header and remove header words,
      name: , class: etc. Returns true if the entire line
      is header (->ignore) */
  bool analyzeHeaders(vector<char *> &line) const;

  /** Returns true if the given word is a header word */
  bool isHeaderWord(const string &word) const;

  mutable set<string> headerWords;

  // Expected club for all entries
  string rulingClub;
  string rulingClass;
  Types lastInsertedType;
public:
  void load();
  bool isLoaded() const {return loaded;}
  void save() const;

  void init(const oRunnerList &r, const oClubList &clb, const oClassList &cls);

  void extractEntries(char *bf, vector<oEntryBlock> &entries);
  void showEntries(gdioutput &gdi, const vector<oEntryBlock> &entries);
  void addEntries(pEvent oe, const vector<oEntryBlock> &entries);

  void test(const oRunnerList &li);
  oFreeImport(void);
  ~oFreeImport(void);

  friend class oEvent;
};
