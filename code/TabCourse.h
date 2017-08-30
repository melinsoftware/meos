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

#include "tabbase.h"

struct ClassDrawSpecification;
enum DrawMethod;

class TabCourse :
  public TabBase
{
  int courseId;
  /** canSwitchViewMode: 0 = no, 1 = yes, 2 = switching to legs */
  void save(gdioutput &gdi, int canSwitchViewMode);
  int courseCB(gdioutput &gdi, int type, void *data);
  bool addedCourse;

  string time_limit;
  string point_limit;
  string point_reduction;

  void fillCourseControls(gdioutput &gdi, const string &ctrl);
  void fillOtherCourses(gdioutput &gdi, oCourse &crs);

  void saveLegLengths(gdioutput &gdi);

  vector<ClassDrawSpecification> courseDrawClasses;

  DrawMethod getDefaultMethod() const;

  string encodeCourse(const string &in, bool firstStart, bool lastFinish);
  void refreshCourse(const string &text, gdioutput &gdi);
  
  const string &formatControl(int id, string &bf) const;

protected:
  void clearCompetitionData();


public:
  void selectCourse(gdioutput &gdi, pCourse pc);

  bool loadPage(gdioutput &gdi);

  const char * getTypeStr() const {return "TCourseTab";}
  TabType getType() const {return TCourseTab;}

  TabCourse(oEvent *oe);
  ~TabCourse(void);

  static void runCourseImport(gdioutput& gdi, const string &filename,
                              oEvent *oe, bool addClasses);

  static void setupCourseImport(gdioutput& gdi, GUICALLBACK cb);

  friend int CourseCB(gdioutput *gdi, int type, void *data);
};
