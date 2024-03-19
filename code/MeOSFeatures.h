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

#include <set>
#include <vector>
#include <map>

class oEvent;

class MeOSFeatures
{
public:
  enum Feature {
    _Head = -1,
    Speaker = 0,
    Economy,
    Clubs,
    EditClub,
    SeveralStages,
    Network,
    TimeAdjust,
    PointAdjust,
    ForkedIndividual,
    Patrol,
    Relay,
    MultipleRaces,
    Rogaining,
    Vacancy,
    InForest,
    DrawStartList,
    Bib,
    RunnerDb,
    NoCourses,
  };

private:
  struct FeatureDescriptor {
    Feature feat;
    wstring code;
    string desc;
    set<Feature> dependsOn;
    FeatureDescriptor &require(Feature f) {
      dependsOn.insert(f);
      return *this;
    }
    FeatureDescriptor(Feature feat, wstring code, string desc);
  };

  vector<FeatureDescriptor> desc;
  map<Feature, int> featureToIx;
  map<wstring, int> codeToIx;
  FeatureDescriptor &add(Feature feat, const wchar_t *code, const char *desc);
  FeatureDescriptor &addHead(const char *desc);

  set<Feature> features;

  int getIndex(Feature f) const;

  void loadDefaults(oEvent &oe);

  bool isRequiredInternal(Feature f) const;

public:
  MeOSFeatures(void);
  ~MeOSFeatures(void);

  bool hasFeature(Feature f) const;
  void useFeature(Feature f, bool use, oEvent &oe);
  bool isRequired(Feature f, const oEvent &oe) const;

  void useAll(oEvent &oe);
  void clear(oEvent &oe);

  int getNumFeatures() const;
  Feature getFeature(int featureIx) const;
  bool isHead(int featureIx) const;
  const string &getHead(int featureIx) const;
  const string &getDescription(Feature f) const;
  const wstring &getCode(Feature f) const;

  bool withoutCourses(const oEvent &oe) const { return hasFeature(NoCourses) && oe.getNumCourses() == 0; }
  bool withCourses(const oEvent *oe) const { return !withoutCourses(*oe); }

  wstring serialize() const;
  void deserialize(const wstring &input, oEvent &oe);
};

