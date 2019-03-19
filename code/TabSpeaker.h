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
#include "tabbase.h"

class SpeakerMonitor;

class spkClassSelection {
  // The currently selected leg
  int selectedLeg;
  // True if total results, otherwise stage results
  bool total;
  // Given a leg, get the corresponding (control, previous control) to watch
  map<int, pair<int, int> > legToControl;
public:
  spkClassSelection() : selectedLeg(0), total(false) {}
  void setLeg(bool totalIn, int leg) {total = totalIn, selectedLeg=leg;}
  int getLeg() const {return selectedLeg;}
  bool isTotal() const {return total;}

  void setControl(int controlId, int previousControl) {
    legToControl[selectedLeg] = make_pair(controlId, previousControl);
  }
  int getControl()  {
    if (legToControl.count(selectedLeg)==1)
      return legToControl[selectedLeg].first;
    else return -1;
  }
  int getPreviousControl()  {
    if (legToControl.count(selectedLeg)==1)
      return legToControl[selectedLeg].second;
    else return -1;
  }
};

class TabSpeaker :
  public TabBase {
private:
  set<int> controlsToWatch;
  set<int> classesToWatch;
  
  // For runner report
  int runnerId = -1;

  int lastControlToWatch;
  int lastClassToWatch;

  set<__int64> shownEvents;
  vector<oTimeLine> events;
  oTimeLine::Priority watchLevel;
  int watchNumber;

  void generateControlList(gdioutput &gdi, int classId);
  void generateControlListForLeg(gdioutput &gdi, int classId, int leg);

  wstring lastControl;

  void manualTimePage(gdioutput &gdi) const;
  void storeManualTime(gdioutput &gdi);

  //Curren class-
  int classId;
  //Map CourseNo -> selected Control.
  //map<int, int> selectedControl;
  map<int, spkClassSelection> selectedControl;
  int deducePreviousControl(int classId, int leg, int control);

  bool ownWindow;

  void drawTimeLine(gdioutput &gdi);
  void splitAnalysis(gdioutput &gdi, int xp, int yp, pRunner r);

  // Runner Id:s to set priority for
  vector<int> runnersToSet;

  SpeakerMonitor *speakerMonitor;

  SpeakerMonitor *getSpeakerMonitor();

  void getSettings(gdioutput &gdi, multimap<string, wstring> &settings);
  void importSettings(gdioutput &gdi, multimap<string, wstring> &settings);
  static void loadSettings(vector< multimap<string, wstring> > &settings);
  static void saveSettings(const vector< multimap<string, wstring> > &settings);
  static wstring getSpeakerSettingsFile();
public:

  void setSelectedRunner(const oRunner &r) { runnerId = r.getId(); }

  bool onClear(gdioutput &gdi);
  void loadPriorityClass(gdioutput &gdi, int classId);
  void savePriorityClass(gdioutput &gdi);

  void updateTimeLine(gdioutput &gdi);

  //Clear selection data
  void clearCompetitionData();
  int processButton(gdioutput &gdi, const ButtonInfo &bu);
  int processListBox(gdioutput &gdi, const ListBoxInfo &bu);
  int handleEvent(gdioutput &gdi, const EventInfo &ei);
  
  const char * getTypeStr() const {return "TSpeakerTab";}
  TabType getType() const {return TSpeakerTab;}

  bool loadPage(gdioutput &gdi);
  TabSpeaker(oEvent *oe);
  ~TabSpeaker(void);
};
