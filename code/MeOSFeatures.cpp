/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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
#include "oEvent.h"
#include "MeOSFeatures.h"
#include "meosexception.h"
#include "meos_util.h"
#include <cassert>
#include "classconfiginfo.h"

MeOSFeatures::MeOSFeatures(void)
{
  addHead("General");
  add(Feature::DrawStartList, L"SL", "Prepare start lists");
  add(Feature::Bib, L"BB", "Bibs");
  add(Feature::Clubs, L"CL", "Clubs");
  add(Feature::EditClub, L"CC", "Edit Clubs").require(Feature::Clubs);

  add(Feature::InForest, L"RF", "Track runners in forest");
  add(Feature::Network, L"NW", "Several MeOS Clients in a network");

  addHead("MeOS Features");
  add(Feature::Speaker, L"SP", "Använd speakerstöd");
  add(Feature::SeveralStages, L"ST", "Several stages");
  add(Feature::Economy, L"EC", "Economy and fees").require(Feature::EditClub).require(Feature::Clubs);
  add(Feature::Vacancy, L"VA", "Vacancies and entry cancellations").require(Feature::DrawStartList);
  add(Feature::TimeAdjust, L"TA", "Manual time penalties and adjustments");
  add(Feature::RunnerDb, L"RD", "Club and runner database").require(Feature::Clubs);
  add(Feature::DualCards, L"DC", "Använd extra stämplingsbricka");

  addHead("Teams and forking");
  add(Feature::ForkedIndividual, L"FO", "Forked individual courses");
  add(Feature::Patrol, L"PT", "Patrols");
  add(Feature::Relay, L"RL", "Relays");
  add(Feature::MultipleRaces, L"MR", "Several races for a runner").require(Feature::Relay);

  addHead("Rogaining");
  add(Feature::Rogaining, L"RO", "Rogaining");
  add(Feature::PointAdjust, L"PA", "Manual point reductions and adjustments").require(Feature::Rogaining);

  addHead("Timekeeping");
  add(Feature::NoCourses, L"NC", "Without courses");
}

MeOSFeatures::FeatureDescriptor &MeOSFeatures::add(Feature feat, const wchar_t *code, const char *descr) {
  assert(codeToIx.count(code) == 0);
  assert(featureToIx.count(feat) == 0);

  featureToIx[feat] = desc.size();
  wstring codeS = code;
  codeToIx[codeS] = desc.size();
  desc.push_back(FeatureDescriptor(feat, codeS, descr));
  return desc.back();
}

MeOSFeatures::FeatureDescriptor &MeOSFeatures::addHead(const char *descr) {
  desc.push_back(FeatureDescriptor(Feature::_Head, L"-", descr));
  return desc.back();
}

bool MeOSFeatures::isHead(int featureIx) const {
  return getFeature(featureIx) == Feature::_Head;
}

const string &MeOSFeatures::getHead(int featureIx) const {
  if (isHead(featureIx))
    return desc[featureIx].desc;
  else
    isHead(-1);//Throws
  return _EmptyString;
}

MeOSFeatures::FeatureDescriptor::FeatureDescriptor(Feature featIn,
                                                   wstring codeIn,
                                                   string descIn) :
  feat(featIn), code(codeIn), desc(descIn)
{}

MeOSFeatures::~MeOSFeatures() = default;

bool MeOSFeatures::hasFeature(Feature f) const {
  return features.count(f) != 0;
}

void MeOSFeatures::useFeature(Feature f, bool use, oEvent &oe) {
  if (use) {
    const set<Feature> &dep = desc[getIndex(f)].dependsOn;
    features.insert(f);
    features.insert(dep.begin(), dep.end());
  }
  else {
    if (!isRequiredInternal(f))
      features.erase(f);
  }
  oe.getDI().setString("Features", serialize());    
}

bool MeOSFeatures::isRequiredInternal(Feature f) const {
  for (set<Feature>::const_iterator it = features.begin(); it != features.end(); ++it) {
    if (desc[getIndex(*it)].dependsOn.count(f))
      return true;
  }
  return false;
}

bool MeOSFeatures::isRequired(Feature f, const oEvent &oe) const {
  if (isRequiredInternal(f))
    return true;

  if (f == Feature::Rogaining && oe.hasRogaining() && hasFeature(Feature::Rogaining))
    return true;

  return false;
}

int MeOSFeatures::getNumFeatures() const {
  return desc.size();
}

MeOSFeatures::Feature MeOSFeatures::getFeature(int featureIx) const {
  if (size_t(featureIx) < desc.size())
    return desc[featureIx].feat;
  else
    throw meosException("Index out of bounds");
}

const string &MeOSFeatures::getDescription(Feature f) const  {
  return desc[getIndex(f)].desc;
}

const wstring &MeOSFeatures::getCode(Feature f) const {
  return desc[getIndex(f)].code;
}

int MeOSFeatures::getIndex(Feature f) const {
    map<Feature, int >::const_iterator res = featureToIx.find(f);
  if (res == featureToIx.end())
    throw meosException("Index out of bounds");
  return  res->second;
}

wstring MeOSFeatures::serialize() const {
  if (features.empty())
    return L"NONE";

  wstring st;
  for (set<Feature>::const_iterator it = features.begin(); it != features.end(); ++it) {
    if (!st.empty())
      st += L"+";
    st += getCode(*it);
  }
  return st;
}

void MeOSFeatures::deserialize(const wstring &input, oEvent &oe) {
  features.clear();

  if (input == L"NONE")
    return;
  else if (input.empty()) {
    loadDefaults(oe);
  }

  vector<wstring> ff;
  split(input, L"+", ff);
  for (size_t k = 0; k < ff.size(); k++) {
    map<wstring, int>::iterator res = codeToIx.find(ff[k]);
    if (res != codeToIx.end())
      features.insert(desc[res->second].feat);
  }

  set<Feature> iF;
  for (set<Feature>::iterator it = features.begin(); it != features.end(); ++it) {
    int ix = getIndex(*it);
    iF.insert(desc[ix].dependsOn.begin(), desc[ix].dependsOn.end());
  }

  features.insert(iF.begin(), iF.end());
}

void MeOSFeatures::loadDefaults(oEvent &oe) {
  if (oe.getDCI().getInt("UseEconomy") != 0) {
    features.insert(Feature::Economy);
    features.insert(Feature::EditClub);
  }

  if (oe.getDCI().getInt("UseSpeaker") != 0)
    features.insert(Feature::Speaker);

  if (oe.hasRogaining())
    features.insert(Feature::Rogaining);

  if (oe.getDCI().getInt("SkipRunnerDb") == 0 )
    features.insert(Feature::RunnerDb);

  ClassConfigInfo cnf;
  oe.getClassConfigurationInfo(cnf);
  if (cnf.hasPatrol())
    features.insert(Feature::Patrol);

  if (cnf.hasRelay())
    features.insert(Feature::Relay);

  if (cnf.raceNStart.size() > 0) {
    features.insert(Feature::Relay);
    features.insert(Feature::MultipleRaces);
  }
  
  if (cnf.isMultiStageEvent())
    features.insert(Feature::SeveralStages);

  features.insert(Feature::Clubs);
  features.insert(Feature::Network);
  features.insert(Feature::ForkedIndividual);

  features.insert(Feature::Vacancy);
  features.insert(Feature::InForest);
  features.insert(Feature::DrawStartList);
  features.insert(Feature::Bib);
}

void MeOSFeatures::useAll(oEvent &oe) {
  for (size_t k = 0; k < desc.size(); k++) {
    if (desc[k].feat == Feature::NoCourses)
      continue;
    if (desc[k].feat != Feature::_Head)
      features.insert(desc[k].feat);
  }
  oe.getDI().setString("Features", serialize());
}

void MeOSFeatures::clear(oEvent &oe) {
  features.clear();
  oe.getDI().setString("Features", serialize());
}
