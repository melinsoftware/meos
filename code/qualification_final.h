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

#include "stdafx.h"

#include <vector>
#include <map>
#include <set>


class QualificationFinal {
private:

  int maxClassId;
  int baseId;

  struct Class {
    wstring name;
    vector< pair<int, int> > qualificationMap;
    vector< vector<int> > timeQualifications;
  };

  vector<Class> classDefinition;

  map<pair<int, int>, pair<int, int>> sourcePlaceToFinalOrder;

  void initgmap(bool check);

  // Recursive implementation
  int getNumStages(int stage) const;
public:

  QualificationFinal(int maxClassId, int baseId) : maxClassId(maxClassId), baseId(baseId) {}
  
  int getNumClasses() const {
    return classDefinition.size();
  }

  const wstring getInstanceName(int inst) {
    return classDefinition.at(inst-1).name;
  }

  // Count number of stages
  int getNumStages() const {
    return getNumStages(classDefinition.size());
  }

  int getHeatFromClass(int finalClassId, int baseClassId) const;
  
  void import(const wstring &file);

  void init(const wstring &def);
  void encode(wstring &output) const;

  /** Retuns the final class and the order within that class. */
  pair<int,int> getNextFinal(int instance, int orderPlace, int numSharedPlaceNext) const;

  /** Returns true if all competitors are automatically qualified*/
  bool noQualification(int instance) const;

  /** Fills in the set of no-qualification classes*/
  void getBaseClassInstances(set<int> &base) const;
};
