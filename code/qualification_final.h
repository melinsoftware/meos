#pragma once

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

#include "stdafx.h"

#include <vector>
#include <map>
#include <set>

class oRunner;
typedef oRunner *pRunner;
class oClass;

struct QFClass {
  QFClass() = default;
  QFClass(const wstring& name) : name(name) {}

  wstring name;
  vector<pair<int, int>> qualificationMap;
  int numTimeQualifications = 0;

  enum class ExtraQualType {
    None,
    All,
    NBest,
    TimeLimit,
  };

  ExtraQualType extraQualification = ExtraQualType::None;
  int extraQualData = 0;

  static ExtraQualType deserialType(wchar_t dc) {
    switch (dc) {
    case 'A':
      return ExtraQualType::All;
    case 'B':
      return ExtraQualType::NBest;
    case 'L':
      return ExtraQualType::TimeLimit;

    }
    return ExtraQualType::None;
  }

  wstring serialExtra() const;
  // Get the minimum qualification instance
  int getMinQualInst() const;
  bool rankLevel = false;
  mutable int level = -1;
  wstring getQualInfo() const;
};

class QualificationFinal {
private:
  mutable wstring serializedFrom;
  int maxClassId;
  int baseId;

  enum class LevelInfo {
    Normal,
    RankSort,
  };

  vector<LevelInfo> levels;
  struct ResultInfo {
    ResultInfo(pRunner r, int inst, int order) : r(r), instance(inst), order(order) {}
    pRunner r;
    int instance;
    int order;
  };
 
  vector<vector<ResultInfo>> storedInfo;
  map<int, ResultInfo *> storedInfoLookup;
  map<int, int> numExtraAssigned; // Number of extra assigned competitors (per race/instance)

  vector<QFClass> classDefinition;
  map<pair<int, int>, pair<int, int>> sourcePlaceToFinalOrder;
  mutable map<int, int> level2DefningInstance;

  void initgmap(bool check);

  /** Retuns the final class and the order within that class. */
  pair<int, int> getPrelFinalFromPlace(int instance, int orderPlace, int numSharedPlaceNext);

  static wstring validName(const wstring& name);

public:

  void setClasses(const vector<QFClass>& def);
  const vector<QFClass>& getClasses() const {
    return classDefinition;
  }

  QualificationFinal(int maxClassId, int baseId) : maxClassId(maxClassId), baseId(baseId) {}
  
  static bool isValidNameChar(wchar_t c);

  bool matchSerialization(const wstring &ser) const {
    return serializedFrom == ser;
  }

  int getNumClasses() const {
    return classDefinition.size();
  }

  const QFClass& getInstance(int instance) const {
    return classDefinition[instance];
  }

  const wstring getInstanceName(int inst) {
    return classDefinition.at(inst-1).name;
  }

  /** Return true if this is a final class*/
  bool isFinalClass(int instance) const;

  // Count number of stages
  int getNumStages() const {
    return getNumLevels();
    //return getNumStages(classDefinition.size());
  }

  int getHeatFromClass(int finalClassId, int baseClassId) const;
  
  void importXML(const wstring &file);
  void exportXML(const wstring& file) const;

  void init(const wstring &def);
  void encode(wstring &output) const;

  /** Return true of any class is of type remaining */
  bool hasRemainingClass() const;

  /** Calculate the level of a particular instance. */
  int getLevel(int instance) const;

  /** Calculate the number of levels. */
  int getNumLevels() const;

  /** Get the minimum number instance of a specified level
      affecting later level qualifications. */
  int getMinInstance(const int level) const;

  /** Clear previous staus data*/
  void prepareCalculations();

  /** Prove result to compute final classes. */
  void provideQualificationResult(pRunner r, int instance, int orderPlace, int numSharedPlace);

  /** Provide data for non-qualified competitors */
  void provideUnqualified(int level, pRunner r);

  /** Do internal calculations. */
  void computeFinals();

  /** Returns the final class and the order within that class. */
  pair<int, int> getNextFinal(int runnerId) const;

  /** Returns true if all competitors are automatically qualified*/
  bool noQualification(int instance) const;

  /** Fills in the set of no-qualification classes*/
  void getBaseClassInstances(set<int> &base) const;

  void printScheme(const oClass &cls, gdioutput &gdi) const;
};
