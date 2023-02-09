#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include <vector>

class ClassConfigInfo {
  friend class oEvent;
private:
  bool results;
  bool starttimes;
  int maximumLegNumber;
public:
  vector < vector<int> > timeStart;
  vector<int> individual;
  vector<int> relay;
  vector<int> patrol;
  vector<int> rogainingTeam;

  vector< vector<int> > legNStart;
  vector< vector<int> > raceNStart;

  map<int, vector<int> > legResult; // main leg number -> class selection
  vector< vector<int> > raceNRes;

  vector<int> rogainingClasses;

  vector<int> knockout;

  vector<int> lapcountsingle;
  vector<int> lapcountextra;

  // True if predefined forking
  bool hasMultiCourse;

  bool hasMultiEvent;

  // True if there are rented cards
  bool hasRentedCard;

  vector<wstring> classWithoutCourse;

  void clear();

  bool hasIndividual() const {return individual.size()>0;}
  bool hasRelay() const {return relay.size()>0;}
  bool hasPatrol() const {return patrol.size()>0;}
  bool hasRogaining() const {return rogainingClasses.size()>0;}
  bool hasRogainingTeam() const { return rogainingTeam.size()>0; }

  bool empty() const;

  // Return true of this is an event in a sequence of events.
  bool isMultiStageEvent() const {return hasMultiEvent;}
  void getIndividual(set<int> &sel, bool forStartList) const;
  void getRelay(set<int> &sel) const;
  void getPatrol(set<int> &sel) const;
  void getTeamClass(set<int> &sel) const;
  void getRogaining(set<int> &sel) const;
  void getRogainingTeam(set<int> &sel) const;

  bool hasTeamClass() const;
  bool hasQualificationFinal() const;

  void getRaceNStart(int race, set<int> &sel) const;
  void getLegNStart(int leg, set<int> &sel) const;

  void getRaceNRes(int race, set<int> &sel) const;
  void getLegNRes(int leg, set<int> &sel) const;

  void getTimeStart(int leg, set<int> &sel) const;

  // Return true if the competiont has any results
  bool hasResults() const {return results;}

  // Return true if the competition defines any start times;
  bool hasStartTimes() const {return starttimes;}

  int getNumLegsTotal() const {return maximumLegNumber;}
};
