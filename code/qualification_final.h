#pragma once

/************************************************************************
MeOS - Orienteering Software
Copyright (C) 2009-2019 Melin Software HB

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
typedef oRunner * pRunner;
class oClass;

class QualificationFinal {
private:
  mutable wstring serializedFrom;
  int maxClassId;
  int baseId;

  struct Class {
    wstring name;
    vector< pair<int, int> > qualificationMap;
    vector< vector<int> > timeQualifications;
    bool rankLevel = false;
    mutable int level = -1;
  };

  enum LevelInfo {
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

  vector<Class> classDefinition;

  map<pair<int, int>, pair<int, int>> sourcePlaceToFinalOrder;

  void initgmap(bool check);

  // Recursive implementation
  int getNumStages(int stage) const;
public:

  QualificationFinal(int maxClassId, int baseId) : maxClassId(maxClassId), baseId(baseId) {}
  
  bool matchSerialization(const wstring &ser) const {
    return serializedFrom == ser;
  }

  int getNumClasses() const {
    return classDefinition.size();
  }

  const wstring getInstanceName(int inst) {
    return classDefinition.at(inst-1).name;
  }

  /** Return true if this is a final class*/
  bool isFinalClass(int instance) const;

  // Count number of stages
  int getNumStages() const {
    return getNumStages(classDefinition.size());
  }

  int getHeatFromClass(int finalClassId, int baseClassId) const;
  
  void import(const wstring &file);

  void init(const wstring &def);
  void encode(wstring &output) const;

  /** Calculate the level of a particular instance. */
  int getLevel(int instance) const;

  /** Calculate the number of levels. */
  int getNumLevels() const;

  /** Get the minimum number instance of a specified level. */
  int getMinInstance(int level) const;

  /** Retuns the final class and the order within that class. */
  pair<int,int> getNextFinal(int instance, int orderPlace, int numSharedPlaceNext) const;

  /** Clear previous staus data*/
  void prepareCalculations();

  /** Return the final class and the order within that class. */
  void setupNextFinal(pRunner r, int instance, int orderPlace, int numSharedPlaceNext);

  /** Do internal calculations. */
  void computeFinals();

  /** Returns the final class and the order within that class. */
  pair<int, int> getNextFinal(int runnerId) const;

  /** Returns true if all competitors are automatically qualified*/
  bool noQualification(int instance) const;

  /** Fills in the set of no-qualification classes*/
  void getBaseClassInstances(set<int> &base) const;

  void printScheme(oClass * cls, gdioutput &gdi) const;
};
