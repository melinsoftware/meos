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
#include "TabBase.h"

#include "oEvent.h"

#include "TabRunner.h"
#include "TabTeam.h"
#include "TabList.h"
#include "TabSpeaker.h"
#include "TabClass.h"
#include "TabCourse.h"
#include "TabControl.h"
#include "TabClub.h"
#include "TabSI.h"
#include "TabCompetition.h"
#include "TabAuto.h"

extern oEvent *gEvent;

FixedTabs::FixedTabs() {
  runnerTab = 0;
  teamTab = 0;
  classTab = 0;
  courseTab = 0;
  controlTab = 0;
  siTab = 0;
  listTab = 0;
  cmpTab = 0;
  speakerTab = 0;
  clubTab = 0;
  autoTab = 0;
 }

FixedTabs::~FixedTabs() {
  tabs.clear();

  delete runnerTab;
  runnerTab = 0;

  delete teamTab;
  teamTab = 0;

  delete classTab;
  classTab = 0;

  delete courseTab;
  courseTab = 0;

  delete controlTab;
  controlTab = 0;

  delete siTab;
  siTab = 0;

  delete listTab;
  listTab = 0;

  delete cmpTab;
  cmpTab = 0;

  delete speakerTab;
  speakerTab = 0;

  delete clubTab;
  clubTab = 0;

  delete autoTab;
  autoTab = 0;
}

TabBase *FixedTabs::get(const TabType tab) {
  switch(tab) {
    case TCmpTab:
      if (!cmpTab) {
        cmpTab = new TabCompetition(gEvent);
        tabs.push_back(cmpTab);
      }
      return cmpTab;
    break;
    case TRunnerTab:
      if (!runnerTab) {
        runnerTab = new TabRunner(gEvent);
        tabs.push_back(runnerTab);
      }
      return runnerTab;
    break;
    case TTeamTab:
      if (!teamTab) {
        teamTab = new TabTeam(gEvent);
        tabs.push_back(teamTab);
      }
      return teamTab;
    break;

    case TListTab:
      if (!listTab) {
        listTab = new TabList(gEvent);
        tabs.push_back(listTab);
      }
      return listTab;
    break;

    case TClassTab:
      if (!classTab) {
        classTab = new TabClass(gEvent);
        tabs.push_back(classTab);
      }
      return classTab;
    break;

    case TCourseTab:
      if (!courseTab) {
        courseTab = new TabCourse(gEvent);
        tabs.push_back(courseTab);
      }
      return courseTab;
    break;

    case TControlTab:
      if (!controlTab) {
        controlTab = new TabControl(gEvent);
        tabs.push_back(controlTab);
      }
      return controlTab;
    break;

    case TClubTab:
      if (!clubTab) {
        clubTab = new TabClub(gEvent);
        tabs.push_back(clubTab);
      }
      return clubTab;
    break;

    case TSpeakerTab:
      if (!speakerTab) {
        speakerTab = new TabSpeaker(gEvent);
        tabs.push_back(speakerTab);
      }
      return speakerTab;
    break;

    case TSITab:
      if (!siTab) {
        siTab = new TabSI(gEvent);
        tabs.push_back(siTab);
      }
      return siTab;
    break;

    case TAutoTab:
      if (!autoTab) {
        autoTab = new TabAuto(gEvent);
        tabs.push_back(autoTab);
      }
      return autoTab;
    break;

    default:
      throw new std::exception("Bad tab type");
  }

  return 0;
}

void FixedTabs::clearCompetitionData() {
  for (size_t k = 0; k < tabs.size(); k++)
    tabs[k]->clearCompetitionData();
}

bool TabObject::loadPage(gdioutput &gdi)
{
  if (tab)
    return tab->loadPage(gdi);
  else return false;
}
