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

#include "Localizer.h"
#include "gdioutput.h"


class oBase;
class oRunner;
typedef oRunner * pRunner;

class oTeam;
typedef oTeam * pTeam;

class gdioutput;
class oEvent;

enum TabType {
  TRunnerTab,
  TTeamTab,
  TClassTab,
  TCourseTab,
  TControlTab,
  TSITab,
  TListTab,
  TCmpTab,
  TSpeakerTab,
  TClubTab,
  TAutoTab,
};

class TabBase
{
protected:
  oEvent *oe;
  int tabId;
  virtual void clearCompetitionData() = 0;

public:
  oEvent *getEvent() const {return oe;}
  int getTabId() const {return tabId;}
  virtual bool loadPage(gdioutput &gdi) = 0;

  virtual TabType getType() const = 0;
  virtual const char *getTypeStr() const = 0;
  
  TabBase(oEvent *poe) : oe(poe), tabId(0) {}
  virtual ~TabBase()=0  {}
  friend class TabObject;
  friend class FixedTabs;
};


class TabObject
{
protected:
  mutable TabBase *tab;

public:
  string name;
  int id;

  TabObject(TabBase *t, string n):name(n),tab(t) {}

  void setId(int i){id=i; if (tab) tab->tabId=id;}
  void setPage(TabBase *tb){delete tab; tab=tb;}

  const type_info &getType() const {return typeid(*tab);}
  const TabBase &getTab() const {return *tab;}
  TabObject(const TabObject &t)
  {
    if (&t!=this) {
      name=t.name;
      id=t.id;
      tab=t.tab;
      //t.tab=0;
    }
  }

  TabObject &operator=(TabObject &t)
  {
    if (&t!=this) {
      delete tab;
      name=t.name;
      id=t.id;
      tab=t.tab;
      //t.tab=0;
    }
  }

  TabObject(TabBase *t);
  ~TabObject();

  bool loadPage(gdioutput &gdi);
};

class TabRunner;
class TabTeam;
class TabClass;
class TabCourse;
class TabControl;
class TabClub;
class TabSI;
class TabList;
class TabCompetition;
class TabSpeaker;
class TabAuto;

class FixedTabs {
  oEvent *oe;
  TabRunner *runnerTab;
  TabTeam *teamTab;
  TabClass *classTab;
  TabCourse *courseTab;
  TabControl *controlTab;
  TabSI *siTab;
  TabList *listTab;
  TabCompetition *cmpTab;
  TabSpeaker *speakerTab;
  TabClub *clubTab;
  TabAuto *autoTab;

  vector<TabBase *> tabs;
public:

  TabBase *get(TabType tag);
  const vector<TabBase *> &getActiveTabs() const {return tabs;}
  // Clean up competition specific data from user interface
  void clearCompetitionData();


  FixedTabs();
  ~FixedTabs();
};

