/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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

#include <algorithm>
#include <cassert>

#include "iof30interface.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "xmlparser.h"
#include "RunnerDB.h"
#include "meos_util.h"
#include "meosException.h"
#include "localizer.h"

wstring &getFirst(wstring &inout, int maxNames);
wstring getMeosCompectVersion();

vector<int> parseSGTimes(const oEvent &oe, const wstring &name) {
  vector<wstring> parts;
  vector<int> times;
  split(name, L" -‒–—‐", parts);
  for (auto &p : parts) {
    for (auto &c : p) {
      if (c == '.')
        c = ':';
    }
    int t = oe.getRelativeTime(p);
    if (t > 0)
      times.push_back(t);
  }
  return times;
}

IOF30Interface::IOF30Interface(oEvent *oe, bool forceSplitFee) : oe(*oe), useGMT(false), teamsAsIndividual(false), 
                                entrySourceId(1), unrollLoops(true), 
                                includeStageRaceInfo(true) {
  cachedStageNumber = -1;
  splitLateFee = forceSplitFee || oe->getPropertyInt("SplitLateFees", false) == 1;
}

void IOF30Interface::readCourseData(gdioutput &gdi, const xmlobject &xo, bool updateClass,
                                    int &courseCount, int &failed) {
  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);
  courseCount = 0;
  failed = 0;
  xmlList xl;
  xo.getObjects("RaceCourseData", xl);
  xmlList::const_iterator it;
  xmlobject xRaceCourses;
  if (xl.size() == 1) {
    xRaceCourses = xl[0];
  }
  else {
    int nr = getStageNumber();
    int ix = -1;
    for (size_t k = 0; k < xl.size(); k++) {
      if (xl[k].getObjectInt("raceNumber") == nr) {
        ix = k;
        break;
      }
    }
    if (ix == -1)
      throw meosException("Filen innehåller flera uppsättningar banor, men ingen har samma etappnummer som denna etapp (X).#" + itos(nr));
    else
      xRaceCourses = xl[ix];
  }

  xmlList xControls, xCourse, x;
  xRaceCourses.getObjects("Control", xControls);
  xRaceCourses.getObjects("Course", xCourse);

  for (size_t k = 0; k < xControls.size(); k++) {
    readControl(xControls[k]);
  }

  map<wstring, pCourse> courses;
  map<wstring, vector<pCourse> > coursesFamilies;

  for (size_t k = 0; k < xCourse.size(); k++) {
    pCourse pc = readCourse(xCourse[k]);
    if (pc) {
      courseCount++;
      if (courses.count(pc->getName()))
        gdi.addString("", 0, L"Varning: Banan 'X' förekommer flera gånger#" + pc->getName());

      courses[pc->getName()] = pc;

      wstring family;
      xCourse[k].getObjectString("CourseFamily", family);

      if (!family.empty()) {
        coursesFamilies[family].push_back(pc);
      }
    }
    else
      failed++;
  }

  vector<pCourse> allC;
  oe.getCourses(allC);
  for (pCourse pc : allC) {
    if (!courses.count(pc->getName()))
      courses[pc->getName()] = pc;
  }

  if (!updateClass)
    return;


  xmlList xClassAssignment, xTeamAssignment, xPersonAssignment;
  xRaceCourses.getObjects("ClassCourseAssignment", xClassAssignment);
  if (xClassAssignment.size() > 0)
    classCourseAssignment(gdi, xClassAssignment, courses, coursesFamilies);

  xRaceCourses.getObjects("PersonCourseAssignment", xPersonAssignment);
  if (xPersonAssignment.size() > 0)
    personCourseAssignment(gdi, xPersonAssignment, courses);

  xRaceCourses.getObjects("TeamCourseAssignment", xTeamAssignment);
  if (xTeamAssignment.size() > 0)
    teamCourseAssignment(gdi, xTeamAssignment, courses);

  xmlList xAssignment;
  xRaceCourses.getObjects("CourseAssignment", xAssignment);
  if (xAssignment.size() > 0) {
    classAssignmentObsolete(gdi, xAssignment, courses, coursesFamilies);
  }

  auto matchCoursePattern = [](const vector<int> &p1, const vector<int> &p2) {
    for (int j = 0; j < p1.size(); j++) {
      if (p1[j] != p2[j] && p1[j] != -1 && p2[j] != -1)
        return false;
    }
    return true;
  };

  // Try to reconstruct 
  for (auto &bibLegCourse : classToBibLegCourse) {
    pClass pc = oe.getClass(bibLegCourse.first);
    if (!pc || bibLegCourse.second.empty())
      continue;

    // Get collection of courses
    set<int> classCourses;
    for (auto &blc : bibLegCourse.second)
      classCourses.insert(get<pCourse>(blc)->getId());

    vector<pCourse> presentCrs;
    pc->getCourses(-1, presentCrs);

    // Check if we have the same set of courses
    bool sameSet = presentCrs.size() == classCourses.size();
    for (pCourse crs : presentCrs) {
      if (!classCourses.count(crs->getId())) {
        sameSet = false;
        break;
      }
    }

    if (sameSet)
      continue; // Do not touch forking if same set

    int fallBackCrs = *classCourses.begin();
    map<int, vector<pair<int, int>>> bibToLegCourseId;
    for (auto &blc : bibLegCourse.second) {
      int bib = get<0>(blc);
      int leg = get<1>(blc);
      int crsId = get<2>(blc)->getId();
      bibToLegCourseId[bib].emplace_back(leg, crsId);
    }

    int width = 0;
    for (auto &blcid : bibToLegCourseId) {
      sort(blcid.second.begin(), blcid.second.end());
      width = max(width, blcid.second.back().first);
    }

    vector<vector<int>> coursePattern;
    int offset = bibToLegCourseId.begin()->first;
    for (auto &blcid : bibToLegCourseId) {
      int bib = blcid.first;
      while (coursePattern.size() <= bib - offset)
        coursePattern.emplace_back(width + 1, -1);
      for (auto &legCrsId : blcid.second) {
        coursePattern.back()[legCrsId.first] = legCrsId.second;
      }
    }

    int period = 1;
    while (period < coursePattern.size()) {
      if (matchCoursePattern(coursePattern[0], coursePattern[period])) {
        // Check if pattern is OK
        bool ok = true;
        for (int off = 0; off < period; off++) {
          for (int c = off + period; c < coursePattern.size(); c++) {
            if (!matchCoursePattern(coursePattern[off], coursePattern[c])) {
              ok = false;
              break;
            }
          }
          if (!ok)
            break;
        }

        if (ok) // Found OK pattern
          break;
      }
      period++;
    }
    
    // Add any missing courses for incomplete patterns. Need not result in a fair forking
    for (int leg = 0; leg < coursePattern[0].size(); leg++) {
      vector<int> crsLeg;
      for (int i = 0; i < period; i++) {
        int crs = coursePattern[i][leg];
        if (crs != -1)
          crsLeg.push_back(crs);
      }
      if (crsLeg.empty())
        crsLeg.push_back(fallBackCrs);
      int rot = 0;
      for (int i = 0; i < period; i++) {
        if (coursePattern[i][leg] == -1)
          coursePattern[i][leg] = crsLeg[(rot++) % crsLeg.size()]; // Take courses from this leg
      }
    }

    int patternStart = (offset - 1) % period;

    if (pc->getNumStages() == 0) {
      pc->setNumStages(coursePattern[0].size());
    }
    for (int leg = 0; leg < pc->getNumStages() && leg < coursePattern[0].size(); leg++) {
      pc->clearStageCourses(leg);
      for (int m = 0; m < period; m++)
        pc->addStageCourse(leg, coursePattern[(patternStart + m)%period][leg], -1);
    }
  }
}

void IOF30Interface::classCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                                           const map<wstring, pCourse> &courses,
                                           const map<wstring, vector<pCourse> > &coursesFamilies) {

  map< pair<int, int>, vector<wstring> > classIdLegToCourse;

  for (size_t k = 0; k < xAssignment.size(); k++) {
    xmlobject &xClsAssignment = xAssignment[k];
    map<int, vector<int> > cls2Stages;

    xmlList xClsId;
    xClsAssignment.getObjects("ClassId", xClsId);
    for (size_t j = 0; j <xClsId.size(); j++) {
      int id = xClsId[j].getInt();
      if (oe.getClass(id) == 0) {
        gdi.addString("", 0, "Klass saknad").setColor(colorRed);
      }
      else
        cls2Stages.insert(make_pair(id, vector<int>()));
    }

    if (cls2Stages.empty()) {
      wstring cname;
      xClsAssignment.getObjectString("ClassName", cname);
      if (cname.length() > 0) {
        pClass pc = oe.getClassCreate(0, cname, matchedClasses);
        if (pc)
          cls2Stages.insert(make_pair(pc->getId(), vector<int>()) );
      }
    }

    if (cls2Stages.empty()) {
      gdi.addString("", 0, "Klass saknad").setColor(colorRed);
      continue;
    }

    // Allowed on leg
    xmlList xLeg;
    xClsAssignment.getObjects("AllowedOnLeg", xLeg);

    for (map<int, vector<int> >::iterator it = cls2Stages.begin(); it != cls2Stages.end(); ++it) {
      pClass defClass = oe.getClass(it->first);
      vector<int> &legs = it->second;

      // Convert from leg/legorder to real leg number
      for (size_t j = 0; j <xLeg.size(); j++) {
        int leg = xLeg[j].getInt()-1;
        if (defClass && defClass->getNumStages() > 0) {
          for (unsigned i = 0; i < defClass->getNumStages(); i++) {
            int realLeg, legIx;
            defClass->splitLegNumberParallel(i, realLeg, legIx);
            if (realLeg == leg)
              legs.push_back(i);
          }
        }
        else
          legs.push_back(leg);
      }
      if (legs.empty())
        legs.push_back(-1); // All legs
    }
    // Extract courses / families
    xmlList xCourse;
    xClsAssignment.getObjects("CourseName", xCourse);

    xmlList xFamily;
    wstring t, t1, t2;
    xClsAssignment.getObjects("CourseFamily", xFamily);

    for (map<int, vector<int> >::iterator it = cls2Stages.begin(); it != cls2Stages.end(); ++it) {
      const vector<int> &legs = it->second;
      for (size_t m = 0; m < legs.size(); m++) {
        int leg = legs[m];
        for (size_t j = 0; j < xFamily.size(); j++) {
          for (size_t i = 0; i < xCourse.size(); i++) {
            wstring crs = constructCourseName(xFamily[j].getObjectString(0, t1),
                                              xCourse[i].getObjectString(0, t2));
            classIdLegToCourse[make_pair(it->first, leg)].push_back(crs);
          }
        }
        if (xFamily.empty()) {
          for (size_t i = 0; i < xCourse.size(); i++) {
            wstring crs = constructCourseName(L"", xCourse[i].getObjectString(0, t));
            classIdLegToCourse[make_pair(it->first, leg)].push_back(crs);
          }
        }
        if (xCourse.empty()) {
          for (size_t j = 0; j < xFamily.size(); j++) {
            map<wstring, vector<pCourse> >::const_iterator res  =
                         coursesFamilies.find(xFamily[j].getObjectString(0, t));


            if (res != coursesFamilies.end()) {
              const vector<pCourse> &family = res->second;
              for (size_t i = 0; i < family.size(); i++) {
                classIdLegToCourse[make_pair(it->first, leg)].push_back(family[i]->getName());
              }
            }
          }
        }
      }
    }
  }

  map< pair<int, int>, vector<wstring> >::iterator it;
  for (it = classIdLegToCourse.begin(); it != classIdLegToCourse.end(); ++it) {
    pClass pc = oe.getClass(it->first.first);
    if (pc) {
      pc->setCourse(0);
      for (size_t k = 0; k < pc->getNumStages(); k++)
        pc->clearStageCourses(k);
    }
  }
  for (it = classIdLegToCourse.begin(); it != classIdLegToCourse.end(); ++it) {
    pClass pc = oe.getClass(it->first.first);
    unsigned leg = it->first.second;
    const vector<wstring> &crs = it->second;
    vector<pCourse> pCrs;
    for (size_t k = 0; k < crs.size(); k++) {
      map<wstring, pCourse>::const_iterator res = courses.find(crs[k]);
      pCourse c = res != courses.end() ? res->second : 0;
      if (c == 0)
        gdi.addString("", 0, L"Varning: Banan 'X' finns inte#" + crs[k]).setColor(colorRed);
      pCrs.push_back(c);
    }
    if (pCrs.empty())
      continue;

    if (leg == -1) {
      if (pCrs.size() > 1) {
        if (!pc->hasMultiCourse()) {
          pc->setNumStages(1);
        }
      }

      if (pc->hasMultiCourse()) {
        for (size_t k = 0; k < pc->getNumStages(); k++) {
          for (size_t j = 0; j < pCrs.size(); j++)
            pc->addStageCourse(k, pCrs[j], -1);
        }
      }
      else
        pc->setCourse(pCrs[0]);
    }
    else if (leg == 0 && pCrs.size() == 1) {
      if (pc->hasMultiCourse())
        pc->addStageCourse(0, pCrs[0], -1);
      else
        pc->setCourse(pCrs[0]);
    }
    else {
      if (leg >= pc->getNumStages())
        pc->setNumStages(leg+1);

      for (size_t j = 0; j < pCrs.size(); j++)
        pc->addStageCourse(leg, pCrs[j], -1);
    }
  }
}

void IOF30Interface::personCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                                            const map<wstring, pCourse> &courses) {
  vector<pRunner> allR;
  oe.getRunners(0, 0, allR, false);
  map<wstring, pRunner> bib2Runner;
  multimap<wstring, pRunner> name2Runner;
  for (size_t k = 0; k < allR.size(); k++) {
    wstring bib = allR[k]->getBib();
    if (!bib.empty())
      bib2Runner[bib] = allR[k];

    name2Runner.insert(make_pair(allR[k]->getName(), allR[k]));
  }

  for (size_t k = 0; k < xAssignment.size(); k++) {
    xmlobject &xPAssignment = xAssignment[k];
    pRunner r = 0;

    wstring runnerText;
    wstring bib;
    xPAssignment.getObjectString("BibNumber", bib);

    if (!bib.empty()) {
      runnerText = bib;
      r = bib2Runner[bib];
    }

    if (r == 0) {
      int id = xPAssignment.getObjectInt("EntryId"); // This assumes entryId = personId, which may or may not be the case.
      if (id != 0) {
        runnerText = L"Id = " + itow(id);
        r = oe.getRunner(id, 0);
      }
    }

    if (r == 0) {
      wstring person;
      xPAssignment.getObjectString("PersonName", person);
      if (!person.empty()) {
        runnerText = person;
        wstring cls;
        xPAssignment.getObjectString("ClassName", cls);
        multimap<wstring, pRunner>::const_iterator res = name2Runner.find(person);
        while (res != name2Runner.end() && person == res->first) {
          if (cls.empty() || res->second->getClass(false) == cls) {
            r = res->second;
            break;
          }
          ++res;
        }
      }
    }

    if (r == 0) {
      gdi.addString("", 0, L"Varning: Deltagaren 'X' finns inte.#" + runnerText).setColor(colorRed);
      continue;
    }

    pCourse c = findCourse(gdi, courses, xPAssignment);
    if (c == 0)
      continue;

    r->setCourseId(c->getId());
  }
}

pCourse IOF30Interface::findCourse(gdioutput &gdi,
                                   const map<wstring, pCourse> &courses,
                                   xmlobject &xPAssignment) {
  wstring course;
  xPAssignment.getObjectString("CourseName", course);
  wstring family;
  xPAssignment.getObjectString("CourseFamily", family);
  wstring fullCrs = constructCourseName(family, course);

  map<wstring, pCourse>::const_iterator res = courses.find(fullCrs);
  pCourse c = res != courses.end() ? res->second : 0;
  if (c == 0) {
    gdi.addString("", 0, L"Varning: Banan 'X' finns inte.#" + fullCrs).setColor(colorRed);
  }
  return c;
}

void IOF30Interface::teamCourseAssignment(gdioutput &gdi, xmlList &xAssignment,
                                            const map<wstring, pCourse> &courses) {
  vector<pTeam> allT;
  oe.getTeams(0, allT, false);

  map<int, int> firstBib2Class;

  map<wstring, pTeam> bib2Team;
  map<pair<wstring, wstring>, pTeam> nameClass2Team;
  for (size_t k = 0; k < allT.size(); k++) {
    wstring bib = allT[k]->getBib();
    if (!bib.empty())
      bib2Team[bib] = allT[k];

    nameClass2Team[make_pair(allT[k]->getName(), allT[k]->getClass(false))] = allT[k];
  }

  for (size_t k = 0; k < xAssignment.size(); k++) {
    xmlobject &xTAssignment = xAssignment[k];
    pTeam t = 0;
    wstring teamText;
    wstring bib;
    int iBib = -1;
    int iClass = -1;
    xTAssignment.getObjectString("BibNumber", bib);

    if (!bib.empty()) {
      teamText = bib;
      iBib = _wtoi(bib.c_str());
      t = bib2Team[bib];
      if (t == nullptr) {
        if (iBib > 0) {
          wstring bib2 = itow(iBib);
          t = bib2Team[bib2];
        }
      }
      if (t != nullptr)
        iClass = t->getClassId(false);
    }

    if (t == 0) {
      wstring team;
      xTAssignment.getObjectString("TeamName", team);
      if (!team.empty()) {
        wstring cls;
        xTAssignment.getObjectString("ClassName", cls);
        auto pcls = oe.getClass(cls);
        if (pcls)
          iClass = pcls->getId();

        t = nameClass2Team[make_pair(team, cls)];
        teamText = team + L" / " + cls;
      }
    }

    if (iBib > 0 && iClass <= 0) {
      if (firstBib2Class.empty()) {
        map<int, int> classId2FirstBib;

        auto insertClsBib = [&](int cls, int b) {
          auto res = classId2FirstBib.find(cls);
          if (res == classId2FirstBib.end())
            classId2FirstBib.emplace(cls, b);
          else
            res->second = min(res->second, b);
        };

        for (pTeam t : allT) {
          int b = _wtoi(t->getBib().c_str());
          if (b <= 0)
            continue;
          int cls = t->getClassId(false);
          if (cls > 0) 
            insertClsBib(cls, b);
        }
        vector<pClass> allC;
        oe.getClasses(allC, false);
        for (pClass c : allC) {
          int b = _wtoi(c->getDCI().getString("Bib").c_str());
          if (b > 0) 
            insertClsBib(c->getId(), b);
        }

        // No check for overlapping classes
        for (auto cfb : classId2FirstBib) {
          firstBib2Class[cfb.second] = cfb.first;
        }
      }

      auto res = firstBib2Class.upper_bound(iBib);
      if (res != firstBib2Class.begin()) {
        --res;
        iClass = res->second;
      }
    }

    if (t == 0 && (iBib<=0 || iClass<=0)) {
      gdi.addString("", 0, L"Varning: Laget 'X' finns inte.#" + teamText).setColor(colorRed);
      continue;
    }

    xmlList teamMemberAssignment;
    xTAssignment.getObjects("TeamMemberCourseAssignment", teamMemberAssignment);
    assignTeamCourse(gdi, t, iClass, iBib, teamMemberAssignment, courses);
  }
}

void IOF30Interface::assignTeamCourse(gdioutput &gdi, oTeam *team, int iClass, int iBib, xmlList &xAssignment,
                                      const map<wstring, pCourse> &courses) {

  pClass cls = oe.getClass(iClass);
  if (!cls)
    return;

  for (size_t k = 0; k <xAssignment.size(); k++) {

    // Extract courses / families
    pCourse c = findCourse(gdi, courses, xAssignment[k]);
    if (c == 0)
      continue;

    xmlobject xLeg = xAssignment[k].getObject("Leg");
    if (xLeg) {
      int leg = xLeg.getInt() - 1;
      int legorder = 0;
      xmlobject xLegOrder = xAssignment[k].getObject("LegOrder");
      if (xLegOrder)
        legorder = xLegOrder.getInt() - 1;

      int legId = cls->getLegNumberLinear(leg, legorder);
      if (legId>=0) {

        classToBibLegCourse[iClass].emplace_back(iBib, legId, c);

        if (team) {
          pRunner r = team->getRunner(legId);
          if (r == 0) {
            r = oe.addRunner(lang.tl(L"N.N."), team->getClubId(), team->getClassId(false), 0, 0, false);
            if (r) {
              r->setEntrySource(entrySourceId);
              r->flagEntryTouched(true);
            }
            team->setRunner(legId, r, false);
            r = team->getRunner(legId);
          }
          if (r) {
            r->setCourseId(c->getId());
          }
        }
      }
      else
        gdi.addString("", 0, L"Bantilldelning för 'X' hänvisar till en sträcka som inte finns#" + cls->getName()).setColor(colorRed);
    }
    else {
      wstring name;
      xAssignment[k].getObjectString("TeamMemberName", name);
      bool done = false;
      if (team) {
        if (!name.empty()) {
          for (int j = 0; j < team->getNumRunners(); j++) {
            pRunner r = team->getRunner(j);
            if (r && r->getName() == name) {
              r->setCourseId(c->getId());
              done = true;
              break;
            }
          }
        }
      }
      if (!done) {
        gdi.addString("", 0, L"Bantilldelning hänvisar till en löpare (X) som saknas i laget (Y)#" + 
                      name + L"#" + team->getName()).setColor(colorRed);
      }
    }
  }
}

void IOF30Interface::classAssignmentObsolete(gdioutput &gdi, xmlList &xAssignment,
                                             const map<wstring, pCourse> &courses,
                                             const map<wstring, vector<pCourse> > &coursesFamilies) {
  map<int, vector<pCourse> > class2Courses;
  map<int, set<wstring> > class2Families;

  multimap<wstring, pRunner> bib2Runners;
  typedef multimap<wstring, pRunner>::iterator bibIterT;
  bool b2RInit = false;

  map<pair<wstring, wstring>, pTeam> clsName2Team;
  typedef map<pair<wstring, wstring>, pTeam>::iterator teamIterT;
  bool c2TeamInit = false;

  for (size_t k = 0; k < xAssignment.size(); k++) {
    wstring name = constructCourseName(xAssignment[k]);
    wstring family;
    xAssignment[k].getObjectString("CourseFamily", family);

    if ( courses.find(name) == courses.end() )
      gdi.addString("", 0, L"Varning: Banan 'X' finns inte#" + name);
    else {
      pCourse pc = courses.find(name)->second;
      xmlList xCls, xPrs;
      xAssignment[k].getObjects("Class", xCls);
      xAssignment[k].getObjects("Person", xPrs);

      for (size_t j = 0; j < xCls.size(); j++) {
        wstring cName;
        xCls[j].getObjectString("Name", cName);
        int id = xCls[j].getObjectInt("Id");
        pClass cls = oe.getClassCreate(id, cName, matchedClasses);
        if (cls) {
          class2Courses[cls->getId()].push_back(pc);

          if (!family.empty()) {
            class2Families[cls->getId()].insert(family);
          }
        }
      }

      for (size_t j = 0; j < xPrs.size(); j++) {
        wstring bib;
        int leg = xPrs[j].getObjectInt("Leg");
        int legOrder = xPrs[j].getObjectInt("LegOrder");

        xPrs[j].getObjectString("BibNumber", bib);
        if (!bib.empty()) {
          if (!b2RInit) {
            // Setup bib2runner map
            vector<pRunner> r;
            oe.getRunners(0, 0, r);
            for (size_t i = 0; i < r.size(); i++) {
              wstring b = r[i]->getBib();
              if (!b.empty())
                bib2Runners.insert(make_pair(b, r[i]));
            }
            b2RInit = true;
          }

          pair<bibIterT, bibIterT> range = bib2Runners.equal_range(bib);
          for (bibIterT it = range.first; it != range.second; ++it) {
            int ln = it->second->getLegNumber();
            int rLegNumber = 0, rLegOrder = 0;
            if (it->second->getClassRef(false))
              it->second->getClassRef(false)->splitLegNumberParallel(ln, rLegNumber, rLegOrder);
            bool match = true;
            if (leg != 0 && leg != rLegNumber+1)
              match = false;
            if (legOrder != 0 && legOrder != rLegOrder+1)
              match = false;

            if (match) {
              it->second->setCourseId(pc->getId());
              it->second->synchronize();
            }
          }
          continue;
        }

        wstring className;
        wstring teamName;
        xPrs[j].getObjectString("ClassName", className);
        xPrs[j].getObjectString("TeamName", teamName);

        if (!teamName.empty()) {
          if (!c2TeamInit) {
            vector<pTeam> t;
            oe.getTeams(0, t);
            for (size_t i = 0; i < t.size(); i++)
              clsName2Team[make_pair(t[i]->getClass(false), t[i]->getName())] = t[i];
            c2TeamInit = true;
          }

          teamIterT res = clsName2Team.find(make_pair(className, teamName));

          if (res != clsName2Team.end()) {
            pClass cls = res->second->getClassRef(false);
            if (cls) {
              int ln = cls->getLegNumberLinear(leg, legOrder);
              pRunner r = res->second->getRunner(ln);
              if (r) {
                r->setCourseId(pc->getId());
                r->synchronize();
              }
            }
          }
          continue;
        }

        // Note: entryId is assumed to be equal to personId,
        // which is the only we have. This might not be true.
        int entryId = xPrs[j].getObjectInt("EntryId");
        pRunner r = oe.getRunner(entryId, 0);
        if (r) {
          r->setCourseId(pc->getId());
          r->synchronize();
        }
      }
    }
  }

  if (!class2Families.empty()) {
    vector<pClass> c;
    oe.getClasses(c, false);
    for (size_t k = 0; k < c.size(); k++) {
      bool assigned = false;

      if (class2Families.count(c[k]->getId())) {
        const set<wstring> &families = class2Families[c[k]->getId()];

        if (families.size() == 1) {
          int nl = c[k]->getNumStages();
          const vector<pCourse> &crsFam = coursesFamilies.find(*families.begin())->second;
          if (nl == 0) {
            if (crsFam.size() == 1)
              c[k]->setCourse(crsFam[0]);
            else {
              c[k]->setNumStages(1);
              c[k]->clearStageCourses(0);
              for (size_t j = 0; j < crsFam.size(); j++)
                c[k]->addStageCourse(0, crsFam[j]->getId(), -1);
            }
          }
          else {
            int nFam = crsFam.size();
            for (int i = 0; i < nl; i++) {
              c[k]->clearStageCourses(i);
              for (int j = 0; j < nFam; j++)
                c[k]->addStageCourse(i, crsFam[(j + i)%nFam]->getId(), -1);
            }
          }
          assigned = true;
        }
        else if (families.size() > 1) {
          int nl = c[k]->getNumStages();
          if (nl == 0) {
            c[k]->setNumStages(families.size());
            nl = families.size();
          }

          set<wstring>::const_iterator fit = families.begin();
          for (int i = 0; i < nl; i++, ++fit) {
            if (fit == families.end())
              fit = families.begin();
            c[k]->clearStageCourses(i);
            const vector<pCourse> &crsFam = coursesFamilies.find(*fit)->second;
            int nFam = crsFam.size();
            for (int j = 0; j < nFam; j++)
              c[k]->addStageCourse(i, crsFam[j]->getId(), -1);
          }

          assigned = true;
        }
      }

      if (!assigned && class2Courses.count(c[k]->getId())) {
        const vector<pCourse> &crs = class2Courses[c[k]->getId()];
        int nl = c[k]->getNumStages();

        if (crs.size() == 1 && nl == 0) {
          c[k]->setCourse(crs[0]);
        }
        else if (crs.size() > 1) {
          int nCrs = crs.size();
          for (int i = 0; i < nl; i++) {
            c[k]->clearStageCourses(i);
            for (int j = 0; j < nCrs; j++)
              c[k]->addStageCourse(i, crs[(j + i)%nCrs]->getId(), -1);
          }
        }
      }
      c[k]->synchronize();
    }
  }
}

void IOF30Interface::prescanCompetitorList(xmlobject &xo) {
  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;
  xmlList work;
  string wstring;
  for (it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Competitor")) {
      xmlobject person = it->getObject("Person");
      if (person) {
        readIdProviders(person, work, wstring);
      }
    }
  }
}

void IOF30Interface::readCompetitorList(gdioutput &gdi, const xmlobject &xo, int &personCount) {
  if (!xo)
    return;

  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;

  for (it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Competitor")) {
      if (readXMLCompetitorDB(*it))
        personCount++;
    }
  }
}

void IOF30Interface::readClubList(gdioutput &gdi, const xmlobject &xo, int &clubCount) {
  if (!xo)
    return;

  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;
  for (it=xl.begin(); it != xl.end(); ++it) {
    if (it->is("Organisation")) {
      if (readOrganization(gdi, *it, true))
        clubCount++;
    }
  }
}

void IOF30Interface::prescanEntryList(xmlobject &xo, set<int> &definedStages) {
  definedStages.clear();

  xmlList pEntries, work;
  xo.getObjects("PersonEntry", pEntries);
  for (size_t k = 0; k < pEntries.size(); k++) {
    prescanEntry(pEntries[k], definedStages, work);
  }

  xo.getObjects("TeamEntry", pEntries);
  for (size_t k = 0; k < pEntries.size(); k++) {
    prescanEntry(pEntries[k], definedStages, work);
  }
}

void IOF30Interface::readEntryList(gdioutput &gdi, xmlobject &xo, bool removeNonexiting, 
                                   const set<int> &stageFilter,
                                   int &entRead, int &entFail, int &entRemoved) {
  string ver;
  entRemoved = 0;
  bool wasEmpty = oe.getNumRunners() == 0;

  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  xmlobject xEvent = xo.getObject("Event");
  map<int, vector<LegInfo> > teamClassConfig;
  map<int, pair<wstring, int> > bibPatterns;
  oClass::extractBibPatterns(oe, bibPatterns);

  if (xEvent) {
    readEvent(gdi, xEvent, teamClassConfig);
  }

  vector<pRunner> allR;
  vector<pTeam> allT;
  oe.getRunners(0, 0, allR, false);
  oe.getStartGroups(true); // Setup transient data for start groups

  for (size_t k = 0; k < allR.size(); k++) {
    if (allR[k]->getEntrySource() == entrySourceId)
      allR[k]->flagEntryTouched(false);
  }

  oe.getTeams(0, allT, false);
  for (size_t k = 0; k < allT.size(); k++) {
    if (allT[k]->getEntrySource() == entrySourceId)
      allT[k]->flagEntryTouched(false);
  }

  xmlList pEntries;
  xo.getObjects("PersonEntry", pEntries);
  map<int, vector< pair<int, int> > > personId2TeamLeg;
  for (size_t k = 0; k < pEntries.size(); k++) {
    if (readPersonEntry(gdi, pEntries[k], 0, teamClassConfig, stageFilter, personId2TeamLeg))
      entRead++;
    else
      entFail++;
  }
  
  xo.getObjects("TeamEntry", pEntries);
  for (size_t k = 0; k < pEntries.size(); k++) {
    xmlList races;
    pEntries[k].getObjects("Race", races);
    if (!matchStageFilter(stageFilter, races))
      continue; // Skip teams belonging to other stage

    setupClassConfig(0, pEntries[k], teamClassConfig);
  }

  // Get all classes, and use existing leg info  
  vector<pClass> allCls;
  oe.getClasses(allCls, false);
  for (size_t k = 0; k < allCls.size(); k++) {
    if (allCls[k]->getNumStages() > 1) {
      for (size_t j = 0; j < allCls[k]->getNumStages(); j++) {
        int number;
        int order;
        allCls[k]->splitLegNumberParallel(j, number, order);
        vector<LegInfo> &li = teamClassConfig[allCls[k]->getId()];
   
        if (size_t(number) >= li.size())
          li.resize(number+1);

        if (allCls[k]->getLegType(j) == LTExtra || allCls[k]->getLegType(j) == LTIgnore || allCls[k]->getLegType(j) == LTParallelOptional)
          li[number].setMaxRunners(order+1);
        else
          li[number].setMinRunners(order+1);
      }
    }
  }

  setupRelayClasses(teamClassConfig);

  for (size_t k = 0; k < pEntries.size(); k++) {
    if (readTeamEntry(gdi, pEntries[k], stageFilter, bibPatterns, teamClassConfig, personId2TeamLeg))
      entRead++;
    else
      entFail++;
  }

  oe.updateStartGroups(); // Store any updated start groups

  bool hasMulti = false;
  for (map<int, vector< pair<int, int> > >::iterator it = personId2TeamLeg.begin();
                  it != personId2TeamLeg.end(); ++it) {
    if (it->second.size() > 1) {
      hasMulti = true;
      break;
    }
  }

  // Analyze equivalences of legs
  map<int, set< vector<int> > > classLegEqClasses;

  for (map<int, vector< pair<int, int> > >::iterator it = personId2TeamLeg.begin();
                  it != personId2TeamLeg.end(); ++it) {
    const vector< pair<int, int> > &teamLeg = it->second;
    if (teamLeg.empty())
      continue; // Should not happen
    int minLeg = teamLeg.front().second;
    int teamId = teamLeg.front().first;
    bool inconsistentTeam = false;

    for (size_t i = 1; i < teamLeg.size(); i++) {
      if (teamLeg[i].first != teamId) {
        inconsistentTeam = true;
        break;
      }
      if (teamLeg[i].second < minLeg)
        minLeg = teamLeg[i].second;
    }

    if (!inconsistentTeam) {
      pTeam t = oe.getTeam(teamId);
      if (t) {
        if (minLeg != teamLeg.front().second) {
          pRunner r = t->getRunner(teamLeg.front().second);
          t->setRunner(minLeg, r, true);
          t->synchronize(true);
        }

        // If multi, for each class, store how the legs was multiplied
        if (hasMulti) {
          vector<int> key(it->second.size());
          for (size_t j = 0; j < key.size(); j++) 
            key[j] = it->second[j].second;

          sort(key.begin(), key.end());
          classLegEqClasses[t->getClassId(false)].insert(key);
        }
      }
    }
  }

  for (map<int, set< vector<int> > >::const_iterator it = classLegEqClasses.begin();
       it != classLegEqClasses.end(); ++it) {
    const set< vector<int> > &legEq = it->second;
    pClass cls = oe.getClass(it->first);
    if (!cls)
      continue;
    bool invalid = false;
    vector<int> specification(cls->getNumStages(), -2);
    for (set< vector<int> >::const_iterator eqit = legEq.begin(); eqit != legEq.end(); ++eqit) {
      const vector<int> &eq = *eqit;
      for (size_t j = 0; j < eq.size(); j++) {
        size_t ix = eq[j];
        if (ix >= specification.size()) {
          invalid = true;
          break; // Internal error?
        }
        if (j == 0) { // Base leg
          if (specification[ix] >= 0) {
            invalid = true;
            break; // Inconsistent specification
          }
          else {
            specification[ix] = -1;
          }
        }
        else { // Duplicated leg
          if (specification[ix] == -1 || (specification[ix] >= 0 && specification[ix] != eq[0])) {
            invalid = true;
            break; // Inconsistent specification
          }
          else {
            specification[ix] = eq[0]; // Specify duplication of base leg
          }
        }
      }
    }
    if (invalid)
      continue;

    vector<pTeam> teams;
    oe.getTeams(it->first, teams);

    // Check that the guessed specification is compatible with all current teams
    for (size_t j = 0; j < specification.size(); j++) {
      if (specification[j] >= 0) {
        // Check that leg is not occupied
        for (size_t i = 0; i < teams.size(); i++) {
          if (teams[i]->getRunner(j) && teams[i]->getRunner(j)->getRaceNo() == 0) {
            invalid = true;
            break;
          }
        }
      }
    }

    if (invalid)
      continue;

    for (size_t j = 0; j < specification.size(); j++) {
      if (specification[j] >= 0) {
        cls->setLegRunner(j, specification[j]);
      }
    }
    oe.adjustTeamMultiRunners(cls);
  }

  if (removeNonexiting && entRead > 0) {
    for (size_t k = 0; k < allT.size(); k++) {
      if (allT[k]->getEntrySource() == entrySourceId && !allT[k]->isEntryTouched()) {
        gdi.addString("", 0, L"Tar bort X#" + allT[k]->getName());
        oe.removeTeam(allT[k]->getId());
        entRemoved++;
      }
      /*else {
        for (int i = 0; i < allT[k]->getNumRunners(); i++) {
          pRunner r = allT[k]->getRunner(i);
          if (r)
            r->flagEntryTouched(true);
        }
      }*/
    }

    vector<int> rids;
    for (size_t k = 0; k < allR.size(); k++) {
      if (allR[k]->getEntrySource() == entrySourceId && !allR[k]->isEntryTouched() && !allR[k]->getTeam()) {
        entRemoved++;
        gdi.addString("", 0, L"Tar bort X#" + allR[k]->getCompleteIdentification());
        rids.push_back(allR[k]->getId());
      }
    }
    if (!rids.empty())
      oe.removeRunner(rids);
  }

  if (wasEmpty) {
    vector<pClass> allCls;
    oe.getClasses(allCls, false);
    set<int> fees;
    set<int> redFees;
    set<double> factor;    
    for (pClass cls : allCls) {      
      int cf = cls->getDCI().getInt("ClassFee");
      int cfRed = cls->getDCI().getInt("ClassFeeRed");
      if (cf > 0)
        fees.insert(cf);

      if (cfRed != 0 && cfRed != cf)
        redFees.insert(cfRed);

      int cfLate = cls->getDCI().getInt("HighClassFee");

      if (cfLate > cf && cf > 0) {
        factor.insert(double(cfLate) / double(cf));
      }
    }

    int youthFee = numeric_limits<int>::max();
    if (!fees.empty())
      youthFee = min(youthFee, *fees.begin());
    if (!redFees.empty())
      youthFee = min(youthFee, *redFees.begin());

    int eliteFee = numeric_limits<int>::max();
    if (!fees.empty())
      eliteFee = *fees.rbegin();

    int normalFee = eliteFee;
    for (int f : fees) {
      if (f != youthFee && f != eliteFee) {
        normalFee = f;
      }
    }
   
    if (youthFee != numeric_limits<int>::max()) {
      oe.getDI().setInt("YouthFee", youthFee);
    }

    if (eliteFee != numeric_limits<int>::max()) {
      oe.getDI().setInt("EliteFee", eliteFee);
    }

    if (normalFee != numeric_limits<int>::max()) {
      oe.getDI().setInt("EntryFee", normalFee);
    }

    if (factor.size() > 0) {
      double f = *factor.rbegin();
      wstring fs = std::to_wstring(int((f - 1.0) * 100.0)) + L" %";
      oe.getDI().setString("LateEntryFactor", fs);
    }
  }
}

void IOF30Interface::readServiceRequestList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail) {
  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  xmlList req;
  xo.getObjects("PersonServiceRequest", req);
  entrySourceId = 0;

  auto &sg = oe.getStartGroups(true);

  bool importStartGroups = sg.size() > 0;

  for (auto &rx : req) {
    xmlobject xPers = rx.getObject("Person");
    pRunner r = 0;
    if (xPers)
      r = readPerson(gdi, xPers);

    if (r) {
      auto xreq = rx.getObject("ServiceRequest");
      if (xreq) {
        auto xServ = xreq.getObject("Service");
        string type;
        if (xServ && (xServ.getObjectString("type", type)=="StartGroup" || importStartGroups)) {
          int id = xServ.getObjectInt("Id");
          if (!importStartGroups)
            r->getDI().setInt("Heat", id);

          if (sg.count(id))
            r->setStartGroup(id);
        }
      }
    }
  }
}

void IOF30Interface::readStartList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail) {
  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  map<int, vector<LegInfo> > teamClassConfig;

  xmlobject xEvent = xo.getObject("Event");
  if (xEvent) {
    readEvent(gdi, xEvent, teamClassConfig);
  }

  xmlList cStarts;
  xo.getObjects("ClassStart", cStarts);

  struct RaceInfo {
    int courseId;
    int length;
    int climb;
    wstring startName;
  };

  for (size_t k = 0; k < cStarts.size(); k++) {
    xmlobject &xClassStart = cStarts[k];

    pClass pc = readClass(xClassStart.getObject("Class"),
                          teamClassConfig);
    int classId = pc ? pc->getId() : 0;


    map<int, RaceInfo> raceToInfo;

    xmlList courses;
    xClassStart.getObjects("Course", courses);
    for (size_t k = 0; k < courses.size(); k++) {
      int raceNo = courses[k].getObjectInt("raceNumber");
      if (raceNo > 0)
        raceNo--;
      RaceInfo &raceInfo = raceToInfo[raceNo];

      raceInfo.courseId = courses[k].getObjectInt("Id");
      raceInfo.length = courses[k].getObjectInt("Length");
      raceInfo.climb = courses[k].getObjectInt("Climb");
    }

    xmlList startNames;
    xClassStart.getObjects("StartName", startNames);
    for (size_t k = 0; k < startNames.size(); k++) {
      int raceNo = startNames[k].getObjectInt("raceNumber");
      if (raceNo > 0)
        raceNo--;
      RaceInfo &raceInfo = raceToInfo[raceNo];
      startNames[k].getObjectString(0, raceInfo.startName);
      pc->setStart(raceInfo.startName);
    }

    if (raceToInfo.size() == 1) {
      RaceInfo &raceInfo = raceToInfo.begin()->second;
      if (raceInfo.courseId > 0) {
        if (pc->getCourse() == 0) {
          pCourse crs = oe.addCourse(pc->getName(), raceInfo.length, raceInfo.courseId);
          crs->setStart(raceInfo.startName, false);
          crs->getDI().setInt("Climb", raceInfo.climb);
          pc->setCourse(crs);
          crs->synchronize();
        }
      }
    }
    else if (raceToInfo.size() > 1) {
    }

    xmlList xPStarts;
    xClassStart.getObjects("PersonStart", xPStarts);
    map<int, pair<wstring, int> > bibPatterns;
    oClass::extractBibPatterns(oe, bibPatterns);

    for (size_t k = 0; k < xPStarts.size(); k++) {
      if (readPersonStart(gdi, pc, xPStarts[k], 0, teamClassConfig))
        entRead++;
      else
        entFail++;
    }

    xmlList tEntries;
    xClassStart.getObjects("TeamStart", tEntries);
    for (size_t k = 0; k < tEntries.size(); k++) {
      setupClassConfig(classId, tEntries[k], teamClassConfig);
    }

    //setupRelayClasses(teamClassConfig);
    if (pc && teamClassConfig.count(pc->getId()) && !teamClassConfig[pc->getId()].empty()) {
      setupRelayClass(pc, teamClassConfig[pc->getId()]);
    }

    for (size_t k = 0; k < tEntries.size(); k++) {
      if (readTeamStart(gdi, pc, tEntries[k], bibPatterns, teamClassConfig))
        entRead++;
      else
        entFail++;
    }

    pc->synchronize();
  }
}

void IOF30Interface::readClassList(gdioutput &gdi, xmlobject &xo, int &entRead, int &entFail) {
  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  map<int, vector<LegInfo> > teamClassConfig;

  xmlobject xEvent = xo.getObject("Event");
  if (xEvent) {
    readEvent(gdi, xEvent, teamClassConfig);
  }

  xmlList cClass;
  xo.getObjects("Class", cClass);


  for (size_t k = 0; k < cClass.size(); k++) {
    xmlobject &xClass = cClass[k];

    pClass pc = readClass(xClass, teamClassConfig);

    if (pc)
      entRead++;
    else
      entFail++;

    if (pc && teamClassConfig.count(pc->getId()) && !teamClassConfig[pc->getId()].empty()) {
      setupRelayClass(pc, teamClassConfig[pc->getId()]);
    }

    pc->synchronize();
  }
}

void IOF30Interface::readEventList(gdioutput &gdi, xmlobject &xo) {
  if (!xo)
    return;

  string ver;
  xo.getObjectString("iofVersion", ver);
  if (!ver.empty() && ver > "3.0")
    gdi.addString("", 0, "Varning, okänd XML-version X#" + ver);

  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;
  map<int, vector<LegInfo> > teamClassConfig;
  for (it=xl.begin(); it != xl.end(); ++it) {
    if (it->is("Event")) {
      readEvent(gdi, *it, teamClassConfig);
      return;
    }
  }
}

void IOF30Interface::readEvent(gdioutput &gdi, const xmlobject &xo,
                               map<int, vector<LegInfo> > &teamClassConfig) {

  wstring name;
  xo.getObjectString("Name", name);
  if (!oe.hasFlag(oEvent::TransferFlags::FlagManualName))
    oe.setName(name, false);

  int id = xo.getObjectInt("Id");
  if (id>0) {
    oe.setExtIdentifier(id);
    entrySourceId = id;
  }
  else {
    entrySourceId = 1; // Use this as a default number for "imported entries"
  }

  xmlobject date = xo.getObject("StartTime");

  if (date) {
    wstring dateStr;
    date.getObjectString("Date", dateStr);
    wstring timeStr;
    date.getObjectString("Time", timeStr);
    if (!timeStr.empty()) {
      wstring tDate, tTime;
      getLocalDateTime(dateStr, timeStr, tDate, tTime);
      dateStr.swap(tDate);
      timeStr.swap(tTime);
      int t = convertAbsoluteTimeISO(timeStr);
      if (t >= 0 && oe.getNumRunners() == 0) {
        int zt = t - 3600;
        if (zt < 0)
          zt += 3600*24;

        if (!oe.hasFlag(oEvent::TransferFlags::FlagManualDateTime))
          oe.setZeroTime(formatTimeHMS(zt), false);
      }
    }
    if (!oe.hasFlag(oEvent::TransferFlags::FlagManualDateTime))
      oe.setDate(dateStr, false);

    //oe.setZeroTime(...);
  }

  xmlobject xOrg = xo.getObject("Organiser");
  oDataInterface DI = oe.getDI();

  if (xOrg) {
    wstring name;
    xOrg.getObjectString("Name", name);
    if (name.length() > 0)
      DI.setString("Organizer", name);

    xmlobject address = xOrg.getObject("Address");

    wstring tmp;

    if (address) {
      DI.setString("CareOf", address.getObjectString("CareOf", tmp));
      DI.setString("Street", address.getObjectString("Street", tmp));
      wstring city, zip, state;
      address.getObjectString("City", city);
      address.getObjectString("ZipCode", zip);
      address.getObjectString("State", state);
      if (state.empty())
        DI.setString("Address", zip + L" " + city);
      else
        DI.setString("Address", state + L", " + zip + L" " + city);
    }

    xmlList xContact;
    xOrg.getObjects("Contact", xContact);

    wstring phone;
    for (size_t k = 0; k < xContact.size(); k++) {
      string type;
      xContact[k].getObjectString("type", type);
      wstring c;
      xContact[k].getObjectString(0, c);

      if (type == "PhoneNumber" || "MobilePhoneNumber")
        phone += phone.empty() ? c : L", " + c;
      else if (type == "EmailAddress")
        DI.setString("EMail", c);
      else if (type == "WebAddress")
        DI.setString("Homepage", c);
    }
    if (!phone.empty())
      DI.setString("Phone", phone);
  }

  wstring account;
  xo.getObjectString("Account", account);
  if (!account.empty())
    DI.setString("Account", account);

  xmlList xClass;
  xo.getObjects("Class", xClass);
  for (size_t k = 0; k < xClass.size(); k++)
    readClass(xClass[k], teamClassConfig);

  if (!feeStatistics.empty()) {
    set<int> fees;
    set<int> factors;
    for (size_t i = 0; i < feeStatistics.size(); i++) {
      int fee = int(100 * feeStatistics[i].fee);
      int factor = int(100 * feeStatistics[i].lateFactor) - 100;
      fees.insert(fee);
      if (factor > 0)
        factors.insert(factor);
    }
    int n = 0, y = 0, e = 0;

    if (fees.size() >= 3) {
      y = *fees.begin();
      fees.erase(fees.begin());
      n = *fees.begin();
      fees.erase(fees.begin());
      e = *fees.rbegin();
    }
    else if (fees.size() == 2) {
      y = *fees.begin();
      fees.erase(fees.begin());
      e = n = *fees.begin();
    }
    else if (fees.size() == 1) {
      e = n = y = *fees.begin();
    }

    if (n > 0) {
      DI.setInt("EliteFee", oe.interpretCurrency(double(e) * 0.01, L""));
      DI.setInt("EntryFee", oe.interpretCurrency(double(n) * 0.01, L""));
      DI.setInt("YouthFee", oe.interpretCurrency(double(y) * 0.01, L""));
    }

    if (!factors.empty()) {
      wchar_t lf[16];
      swprintf_s(lf, L"%d %%", *factors.rbegin());
      DI.setString("LateEntryFactor", lf);
    }
  }

  oe.synchronize();
  xmlList xService;
  xo.getObjects("Service", xService);
  services.clear();

  for (auto &s : xService) {
    int id = s.getObjectInt("Id");
    if (id > 0) {      
      xmlList nameList;
      s.getObjects("Name", nameList);
      for (auto s : nameList) {        
        services.emplace_back(id, s.getw());
      }
    }
  }
  bool anySG = false;
  // This is a "hack" to interpret services of the from "XXXX 14:00 - 15:00 XXXX" as a start group.
  for (auto &srv : services) {
    vector<int> times = parseSGTimes(oe, srv.name);
    int ts = times.size();
    if (ts >= 2 && times[ts - 2] < times[ts - 1]) {
      oe.setStartGroup(srv.id, times[ts - 2], times[ts - 1], L"");
      anySG = true;
    }
  }

  if (anySG)
    oe.updateStartGroups();
}

void IOF30Interface::setupClassConfig(int classId, const xmlobject &xTeam, map<int, vector<LegInfo> > &teamClassConfig) {

  // Get class
  xmlobject xClass = xTeam.getObject("Class");
  if (xClass) {
    pClass pc = readClass(xClass, teamClassConfig);
    classId = pc->getId();
  }
  vector<LegInfo> &teamClass = teamClassConfig[classId];

  // Get team entriess
  xmlList xEntries;
  xTeam.getObjects("TeamEntryPerson", xEntries);
  for (size_t k = 0; k < xEntries.size(); k++) {
    int leg = xEntries[k].getObjectInt("Leg");
    int legorder = xEntries[k].getObjectInt("LegOrder");
    leg = max(0, leg - 1);
    legorder = max(1, legorder);
    if (int(teamClass.size()) <= leg)
      teamClass.resize(leg + 1);
    teamClass[leg].setMaxRunners(legorder);
  }

  // Get team starts
  xmlList xMemberStarts;
  xTeam.getObjects("TeamMemberStart", xMemberStarts);
  for (size_t k = 0; k < xMemberStarts.size(); k++) {
    xmlList xStarts;
    xMemberStarts[k].getObjects("Start", xStarts);
    for (size_t j = 0; j < xStarts.size(); j++) {
      int leg = xStarts[j].getObjectInt("Leg");
      int legorder = xStarts[j].getObjectInt("LegOrder");
      leg = max(0, leg - 1);
      legorder = max(1, legorder);
      if (int(teamClass.size()) <= leg)
        teamClass.resize(leg + 1);
      teamClass[leg].setMaxRunners(legorder);
    }
  }
}

pTeam IOF30Interface::readTeamEntry(gdioutput &gdi, xmlobject &xTeam,
                                    const set<int> &stageFilter,
                                    map<int, pair<wstring, int> > &bibPatterns,
                                    const map<int, vector<LegInfo> > &teamClassConfig,
                                    map<int, vector< pair<int, int> > > &personId2TeamLeg) {

  xmlList races;
  xTeam.getObjects("Race", races);
  if (!matchStageFilter(stageFilter, races))
    return 0;
  
  // Class
  map<int, vector<LegInfo> > localTeamClassConfig;
  pClass pc = readClass(xTeam.getObject("Class"), localTeamClassConfig);

  bool newTeam;
  pTeam t = getCreateTeam(gdi, xTeam, pc ? pc->getId() : 0, newTeam);

  if (!t)
    return 0;

  if (pc && (t->getClassId(false) == 0 || !t->hasFlag(oAbstractRunner::FlagUpdateClass)) ) { 
    t->setClassId(pc->getId(), false);
  }
  wstring bib;
  xTeam.getObjectString("BibNumber", bib);
  wchar_t pat[32];
  int no = oClass::extractBibPattern(bib, pat);
  if (no > 0 && t->getBib().empty())
    t->setBib(bib, no, true);
  else if (newTeam) {
    pair<int, wstring> autoBib = pc->getNextBib(bibPatterns);
    if (autoBib.first > 0) {
      t->setBib(autoBib.second, autoBib.first, true);
    }
  }

  oDataInterface di = t->getDI();
  if (newTeam) {
    wstring entryTime;
    xTeam.getObjectString("EntryTime", entryTime);

    wstring date, time;
    getLocalDateTime(entryTime, date, time);
    di.setDate("EntryDate", date);
    if (time.length() > 0) {
      int t = convertAbsoluteTimeISO(time);
      if (t >= 0)
        di.setInt("EntryTime", t);
    }
  }

  double fee = 0, paid = 0, taxable = 0, percentage = 0;
  wstring currency;
  xmlList xAssigned;
  xTeam.getObjects("AssignedFee", xAssigned);
  for (size_t j = 0; j < xAssigned.size(); j++) {
    getAssignedFee(xAssigned[j], fee, paid, taxable, percentage, currency);
  }
  fee += fee * percentage; // OLA / Eventor stupidity

  di.setInt("Fee", oe.interpretCurrency(fee, currency));
  di.setInt("Paid", oe.interpretCurrency(paid, currency));
  di.setInt("Taxable", oe.interpretCurrency(fee, currency));

  xmlList xEntries;
  xTeam.getObjects("TeamEntryPerson", xEntries);

  set<int> noFilter;
  for (size_t k = 0; k<xEntries.size(); k++) {
    readPersonEntry(gdi, xEntries[k], t, teamClassConfig, noFilter, personId2TeamLeg);
  }
  t->applyBibs();
  t->evaluate(oBase::ChangeType::Update);
  return t;
}

pTeam IOF30Interface::readTeamStart(gdioutput &gdi, pClass pc, xmlobject &xTeam,
                                    map<int, pair<wstring, int> > &bibPatterns,
                                    const map<int, vector<LegInfo> > &teamClassConfig) {
  bool newTeam;
  pTeam t = getCreateTeam(gdi, xTeam, pc ? pc->getId() : 0, newTeam);

  if (!t)
    return 0;

  // Class
  if (pc && (t->getClassId(false) == 0 || !t->hasFlag(oAbstractRunner::FlagUpdateClass)) )
    t->setClassId(pc->getId(), false);

  wstring bib;
  xTeam.getObjectString("BibNumber", bib);
  wchar_t pat[32];
  int no = oClass::extractBibPattern(bib, pat);
  if (no > 0 && t->getBib().empty())
    t->setBib(bib, no, true);
  else if (newTeam){
    pair<int, wstring> autoBib = pc->getNextBib(bibPatterns);
    if (autoBib.first > 0) {
      t->setBib(autoBib.second, autoBib.first, true);
    }
  }
  xmlList xEntries;
  xTeam.getObjects("TeamMemberStart", xEntries);
  
  for (size_t k = 0; k<xEntries.size(); k++) {
    readPersonStart(gdi, pc, xEntries[k], t, teamClassConfig);
  }
  t->applyBibs();
  t->evaluate(oBase::ChangeType::Update);
  return t;
}

pTeam IOF30Interface::getCreateTeam(gdioutput &gdi, const xmlobject &xTeam, int expectedClassId, bool &newTeam) {
  newTeam = false;
  wstring name;
  xTeam.getObjectString("Name", name);

  if (name.empty())
    return 0;

  int id = xTeam.getObjectInt("Id");
  pTeam t = 0;

  if (id)
    t = oe.getTeam(id);
  else {
    t = oe.getTeamByName(name);
    if (t && expectedClassId > 0 && t->getClassId(false) != expectedClassId)
      t = nullptr;
  }
  if (!t) {
    if (id > 0) {
      oTeam tr(&oe, id);
      t = oe.addTeam(tr, true);
    }
    else {
      oTeam tr(&oe);
      t = oe.addTeam(tr, true);
    }
    newTeam = true;
  }

  if (!t)
    return 0;

  if (entrySourceId > 0)
    t->setEntrySource(entrySourceId);
  t->flagEntryTouched(true);  
  if (t->getName().empty() || !t->hasFlag(oAbstractRunner::FlagUpdateName))
    t->setName(name, false);

  // Club
  pClub c = 0;
  xmlList xOrgs;
  xTeam.getObjects("Organisation", xOrgs);
  if (xOrgs.empty())
    xTeam.getObjects("Organization", xOrgs);

  for (size_t k = 0; k < xOrgs.size(); k++) {
    if (c == 0)
      c = readOrganization(gdi, xOrgs[k], false);
    else
      readOrganization(gdi, xOrgs[k], false);// Just include in competition
  }

  if (c)
    t->setClubId(c->getId());

  return t;
}

int IOF30Interface::getIndexFromLegPos(int leg, int legorder, const vector<LegInfo> &setup) {
  int ix = 0;
  for (int k = 0; k < leg - 1; k++)
    ix += k < int(setup.size()) ? max(setup[k].maxRunners, 1) : 1;
  if (legorder > 0)
    ix += legorder - 1;
  return ix;
}

void IOF30Interface::prescanEntry(xmlobject &xo, set<int> &stages, xmlList &work) {
  xmlList &races = work;
  xo.getObjects("RaceNumber", races);
  if (races.empty())
    xo.getObjects("Race", races); // For unclear reason the attribute is called Race for teams and RaceNumber for persons.

  if (races.empty())
    stages.insert(-1);// All
  else {
    for (auto &race : races) {
      int r = race.getInt();
      if (r > 0)
        stages.insert(r);
    }
  }

  xmlobject person = xo.getObject("Person");
  if (!person) {
    xmlobject teamPerson = xo.getObject("TeamEntryPerson");
    if (teamPerson)
      person = teamPerson.getObject("Person");
  }
  
  if (person) {
    xmlList &ids = work;
    string type;
    readIdProviders(person, ids, type);
  }
}

void IOF30Interface::readIdProviders(xmlobject &person, xmlList &ids, std::string &type)
{
  person.getObjects("Id", ids);
  if (ids.size() > 1) {
    for (auto &id : ids) {
      id.getObjectString("type", type);
      if (!type.empty()) {
        idProviders.insert(type);
      }
    }
  }
}

bool IOF30Interface::matchStageFilter(const set<int> &stageFilter, const xmlList &races) {
  if (stageFilter.empty() || races.empty())
    return true;

  for (auto &r : races) {
    if (stageFilter.count(r.getInt()))
      return true;
  }

  return false;
}

pRunner IOF30Interface::readPersonEntry(gdioutput &gdi, xmlobject &xo, pTeam team,
                                        const map<int, vector<LegInfo> > &teamClassConfig,
                                        const set<int> &stageFilter, 
                                        map<int, vector< pair<int, int> > > &personId2TeamLeg) {
  
  xmlList races;
  xo.getObjects("RaceNumber", races);
  if (!matchStageFilter(stageFilter, races))
    return 0;
  
  xmlobject xPers = xo.getObject("Person");
  // Card
  const int cardNo = xo.getObjectInt("ControlCard");

  pRunner r = 0;

  if (xPers)
    r = readPerson(gdi, xPers);

  if (cardNo > 0 && r == 0 && team) {
    // We got no person, but a card number. Add the runner anonymously.
    r = oe.addRunner(lang.tl(L"N.N."), team->getClubId(), team->getClassId(false), cardNo, 0, false);
    r->flagEntryTouched(true);
    r->setEntrySource(entrySourceId);
    r->synchronize();
  }

  if (r == 0)
    return 0;

  // Club
  pClub c = readOrganization(gdi, xo.getObject("Organisation"), false);
  if (!c)
    c = readOrganization(gdi, xo.getObject("Organization"), false);

  if (c)
    r->setClubId(c->getId());

  // Class
  map<int, vector<LegInfo> > localTeamClassConfig;
  pClass pc = readClass(xo.getObject("Class"), localTeamClassConfig);

  if (pc && (r->getClassId(false) == 0 || !r->hasFlag(oAbstractRunner::FlagUpdateClass)) )
    r->setClassId(pc->getId(), false);

  if (team) {
    int leg = xo.getObjectInt("Leg");
    int legorder = xo.getObjectInt("LegOrder");
    int legindex = max(0, leg - 1);
    map<int, vector<LegInfo> >::const_iterator res = teamClassConfig.find(team->getClassId(false));
    if (res != teamClassConfig.end()) {
      legindex = getIndexFromLegPos(leg, legorder, res->second);
    }

    if (personId2TeamLeg.find(r->getId()) == personId2TeamLeg.end()) {
      if (team->getClassRef(false))
        legindex = team->getClassRef(false)->getLegRunner(legindex);

      // Ensure unique
      team->setRunner(legindex, r, false);
      if (r->getClubId() == 0)
        r->setClubId(team->getClubId());
    }
    personId2TeamLeg[r->getId()].push_back(make_pair(team->getId(), legindex));
  }

  // Card
  if (cardNo > 0)
    r->setCardNo(cardNo, false);

  oDataInterface di = r->getDI();

  wstring entryTime;
  xo.getObjectString("EntryTime", entryTime);
  
  wstring date, time;
  getLocalDateTime(entryTime, date, time);
  di.setDate("EntryDate", date);
  if (time.length() > 0) {
    int t = convertAbsoluteTimeISO(time);
    if (t >= 0)
      di.setInt("EntryTime", t);
  }
 
  double fee = 0, paid = 0, taxable = 0, percentage = 0;
  wstring currency;
  xmlList xAssigned;
  xo.getObjects("AssignedFee", xAssigned);
  for (size_t j = 0; j < xAssigned.size(); j++) {
    getAssignedFee(xAssigned[j], fee, paid, taxable, percentage, currency);
  }
  fee += fee * percentage; // OLA / Eventor stupidity

  di.setInt("Fee", oe.interpretCurrency(fee, currency));
  di.setInt("Paid", oe.interpretCurrency(paid, currency));
  di.setInt("Taxable", oe.interpretCurrency(fee, currency));


  // StartTimeAllocationRequest
  xmlobject sar = xo.getObject("StartTimeAllocationRequest");
  if (sar) {
    string type;
    sar.getObjectString("type", type);
    if (type == "groupedWithRef" || type == "GroupedWith") {
      xmlobject pRef = sar.getObject("Person");
      if (pRef) {
        wstring sid;
        pRef.getObjectString("Id", sid);
        __int64 extId = oBase::converExtIdentifierString(sid);
        int pid = oBase::idFromExtId(extId);
        pRunner rRef = oe.getRunner(pid, 0);
        if (rRef && rRef->getExtIdentifier() == extId) {
          rRef->setReference(r->getId());
        }
        r->setReference(pid);
      }
    }
  }

  bool hasTime = true;
  xmlobject ext = xo.getObject("Extensions");
  if (ext) {
    xmlList exts;
    ext.getObjects(exts);
    for (xmlobject &xx : exts) {
      if (xx.is("TimePresentation")) {
        hasTime = xx.getObjectBool(nullptr);
      }
      else if (xx.is("StartGroup")) {
        int groupId = xx.getObjectInt("Id");
        if (groupId > 0) {
          wstring groupName;
          xx.getObjectString("Name", groupName);
          if (oe.getStartGroup(groupId).firstStart == -1) {
            vector<int> times = parseSGTimes(oe, groupName);
            int ts = times.size();
            if (ts >= 2 && times[ts - 2] < times[ts - 1]) 
              oe.setStartGroup(groupId, times[ts - 2], times[ts - 1], groupName);
            else
              oe.setStartGroup(groupId, 3600, 3600 * 2, groupName);
          }
          r->setStartGroup(groupId);
        }
      }
    }
  }
  if (!hasTime)
    r->setStatus(StatusNoTiming, true, oBase::ChangeType::Update);
  else if (r->getStatus() == StatusNoTiming)
    r->setStatus(StatusUnknown, true, oBase::ChangeType::Update);

  r->synchronize();
  return r;
}

pRunner IOF30Interface::readPersonStart(gdioutput &gdi, pClass pc, xmlobject &xo, pTeam team,
                                        const map<int, vector<LegInfo> > &teamClassConfig) {
  xmlobject xPers = xo.getObject("Person");
  pRunner r = 0;
  if (xPers)
    r = readPerson(gdi, xPers);
  if (r == 0)
    return 0;

  // Club
  pClub c = readOrganization(gdi, xo.getObject("Organisation"), false);
  if (!c)
    c = readOrganization(gdi, xo.getObject("Organization"), false);

  if (c)
    r->setClubId(c->getId());

  xmlList starts;
  xo.getObjects("Start", starts);

  for (size_t k = 0; k < starts.size(); k++) {
    int race = starts[k].getObjectInt("raceNumber");
    pRunner rRace = r;
    if (race > 1 && r->getNumMulti() > 0) {
      pRunner rr = r->getMultiRunner(race - 1);
      if (rr)
        rRace = rr;
    }
    if (rRace) {
      // Card
      int cardNo = starts[k].getObjectInt("ControlCard");
      if (cardNo > 0)
        rRace->setCardNo(cardNo, false);

      xmlobject startTime = starts[k].getObject("StartTime");

      if (team) {
        int leg = starts[k].getObjectInt("Leg");
        int legorder = starts[k].getObjectInt("LegOrder");
        int legindex = max(0, leg - 1);
        map<int, vector<LegInfo> >::const_iterator res = teamClassConfig.find(team->getClassId(false));
        if (res != teamClassConfig.end()) {
          legindex = getIndexFromLegPos(leg, legorder, res->second);
        }
        team->setRunner(legindex, rRace, false);
        if (rRace->getClubId() == 0)
          rRace->setClubId(team->getClubId());

        if (startTime && pc) {
          pc->setStartType(legindex, STDrawn, false);

        }
      }

      wstring bib;
      starts[k].getObjectString("BibNumber", bib);
      rRace->getDI().setString("Bib", bib);

      rRace->setStartTime(parseISO8601Time(startTime), true, oBase::ChangeType::Update);
    }
  }

  if (pc && (r->getClassId(false) == 0 || !r->hasFlag(oAbstractRunner::FlagUpdateClass)) )
    r->setClassId(pc->getId(), true);

  r->synchronize();
  return r;
}

void IOF30Interface::readId(const xmlobject &person, int &pid, __int64 &extId) const {
  wstring sid;
  pid = 0;
  extId = 0;
  if (preferredIdProvider.empty()) {
    person.getObjectString("Id", sid);
  }
  else {
    xmlList sids;
    wstring bsid;
    person.getObjects("Id", sids);
    for (auto &x : sids) {
      auto type = x.getAttrib("type");
      if (type && type.get() == preferredIdProvider) {
        sid = x.getw();
      }
      else if (bsid.empty())
        bsid = x.getw();
    }
    if (sid.empty())
      pid = oBase::idFromExtId(oBase::converExtIdentifierString(bsid));
  }
  if (!sid.empty()) {
    extId = oBase::converExtIdentifierString(sid);
    pid = oBase::idFromExtId(extId);
  }
}

pRunner IOF30Interface::readPerson(gdioutput &gdi, const xmlobject &person) {

  xmlobject pname = person.getObject("Name");

  wstring name;
  
  if (pname) {
    wstring given, family;
    //name = getFirst(pname.getObjectString("Given", given), 2)+ " " +pname.getObjectString("Family", family);
    name = pname.getObjectString("Family", family) + L", " + getFirst(pname.getObjectString("Given", given), 2);
  }
  else {
    name = lang.tl("N.N.");
  }
  int pid = 0;
  __int64 extId = 0;
  readId(person, pid, extId);
  /*
  wstring sid;
  int pid = 0;
  __int64 extId = 0;
  if (preferredIdProvider.empty()) {
    person.getObjectString("Id", sid);
  }
  else {
    xmlList sids;
    wstring bsid;
    person.getObjects("Id", sids);
    for (auto &x : sids) {
      auto type = x.getAttrib("type");
      if (type && type.get() == preferredIdProvider) {
        sid = x.getw();
      }
      else if (bsid.empty())
        bsid = x.getw();
    }
    if (sid.empty())
      pid = oBase::idFromExtId(oBase::converExtIdentifierString(bsid));
  }
  if (!sid.empty()) {
    extId = oBase::converExtIdentifierString(sid);
    pid = oBase::idFromExtId(extId);
  }*/
  pRunner r = 0;

  if (pid) {
    r = oe.getRunner(pid, 0);
    while (r) { // Check that the exact match is OK
      if (extId == r->getExtIdentifier())
        break;
      pid++;
      r = oe.getRunner(pid, 0);
    }

    if (r) {
      // Check that a with this id runner does not happen to exist with a different source
      if (entrySourceId>0 && r->getEntrySource() != entrySourceId) {
        r = 0;
        pid = 0;
      }
      else if (entrySourceId == 0) {
        wstring canName = canonizeName(name.c_str());
        wstring canOldName = canonizeName(r->getName().c_str());
        if (canName != canOldName) {
          r = 0;
          pid = 0;
        }
      }
    }
  }

  if (!r) {
    if ( pid > 0) {
      oRunner or(&oe, pid);
      r = oe.addRunner(or, true);
    }
    else {
      oRunner or(&oe);
      r = oe.addRunner(or, true);
    }
  }

  if (entrySourceId > 0)
    r->setEntrySource(entrySourceId);
  r->flagEntryTouched(true);
 
  if (!r->hasFlag(oAbstractRunner::FlagUpdateName)) {
    r->setName(name, false);
  }

  r->setExtIdentifier(extId);

  oDataInterface DI=r->getDI();
  wstring tmp;

  PersonSex s = interpretSex(person.getObjectString("sex", tmp));
  if (s != sUnknown)
    r->setSex(s);
  person.getObjectString("BirthDate", tmp);
  if (tmp.length()>=4) {
    tmp = tmp.substr(0, 4);
    r->setBirthYear(_wtoi(tmp.c_str()));
  }

  getNationality(person.getObject("Nationality"), DI);

  return r;
}

pClub IOF30Interface::readOrganization(gdioutput &gdi, const xmlobject &xclub, bool saveToDB) {
  if (!xclub)
    return 0;
  wstring clubIdS;
  xclub.getObjectString("Id", clubIdS);
  __int64 extId = oBase::converExtIdentifierString(clubIdS);
  int clubId = oBase::idFromExtId(extId);
  wstring name, shortName;
  xclub.getObjectString("Name", name);
  xclub.getObjectString("ShortName", shortName);

  if (shortName.length() > 4 && shortName.length() < name.length())
    swap(name, shortName);

  if (name.length()==0 || !IsCharAlphaNumeric(name[0]))
    return 0;

  pClub pc=0;

  if ( !saveToDB ) {
    if (clubId)
      pc = oe.getClubCreate(clubId, name);

    if (!pc) return false;
  }
  else {
    pc = new oClub(&oe, clubId);
    //pc->setID->Id = clubId;
  }

  pc->setName(name);

  pc->setExtIdentifier(extId);

  oDataInterface DI=pc->getDI();

  wstring tmp;

  int district = xclub.getObjectInt("ParentOrganisationId");
  if (district > 0)
    DI.setInt("District", district);

  xmlobject address = xclub.getObject("Address");

  if (shortName.length() <= 4)
    DI.setString("ShortName", shortName);

  wstring str;

  if (address) {
    DI.setString("CareOf", address.getObjectString("CareOf", tmp));
    DI.setString("Street", address.getObjectString("Street", tmp));
    DI.setString("City", address.getObjectString("City", tmp));
    DI.setString("ZIP", address.getObjectString("ZipCode", tmp));
    DI.setString("State", address.getObjectString("State", tmp));
    getNationality(address.getObject("Country"), DI);
  }

  xmlList xContact;
  xclub.getObjects("Contact", xContact);

  wstring phone;
  for (size_t k = 0; k < xContact.size(); k++) {
    string type;
    xContact[k].getObjectString("type", type);
    wstring c;
    xContact[k].getObjectString(0, c);

    if (type == "PhoneNumber" || type == "MobilePhoneNumber")
      phone += phone.empty() ? c : L", " + c;
    else if (type == "EmailAddress")
      DI.setString("EMail", c);
  }
  DI.setString("Phone", phone);

  getNationality(xclub.getObject("Country"), DI);

  xclub.getObjectString("type", str);
  if (!str.empty())
    DI.setString("Type", str);

  if (saveToDB) {
    oe.getRunnerDatabase().importClub(*pc, false);
    delete pc;
  }
  else {
    pc->synchronize();
  }

  return pc;
}

void IOF30Interface::getNationality(const xmlobject &xCountry, oDataInterface &di) {
  if (xCountry) {
    wstring code, country;

    xCountry.getObjectString("code", code);
    xCountry.getObjectString(0, country);

    if (!code.empty())
        di.setString("Nationality", code);

    if (!country.empty())
        di.setString("Country", country);
  }
}

void IOF30Interface::getAmount(const xmlobject &xAmount, double &amount, wstring &currency) {
  amount = 0; // Do no clear currency. It is filled in where found (and assumed to be constant)
  if (xAmount) {
    string tmp;
    xAmount.getObjectString(0, tmp);
    amount = atof(tmp.c_str());
    xAmount.getObjectString("currency", currency);
  }
}

void IOF30Interface::getFeeAmounts(const xmlobject &xFee, double &fee, double &taxable, double &percentage, wstring &currency) {
  xmlobject xAmount = xFee.getObject("Amount");
  xmlobject xPercentage = xFee.getObject("Percentage"); // Eventor / OLA stupidity
  if (xPercentage) {
    string tmp;
    xPercentage.getObjectString(0, tmp);
    percentage = atof(tmp.c_str()) * 0.01;
  }
  else
    getAmount(xAmount, fee, currency);
  getAmount(xFee.getObject("TaxableAmount"), taxable, currency);
}

void IOF30Interface::getAssignedFee(const xmlobject &xFee, double &fee, double &paid, double &taxable, double &percentage, wstring &currency) {
  currency.clear();
  if (xFee) {
    getFeeAmounts(xFee.getObject("Fee"), fee, taxable, percentage, currency);
    getAmount(xFee.getObject("PaidAmount"), paid, currency);
  }
}

void IOF30Interface::getFee(const xmlobject &xFee, FeeInfo &fee) {
  getFeeAmounts(xFee, fee.fee, fee.taxable, fee.percentage, fee.currency);

  xFee.getObjectString("ValidFromTime", fee.fromTime);
  xFee.getObjectString("ValidToTime", fee.toTime);

  xFee.getObjectString("FromDateOfBirth", fee.fromBirthDate);
  xFee.getObjectString("ToDateOfBirth", fee.toBirthDate);
}

void IOF30Interface::writeAmount(xmlparser &xml, const char *tag, int amount) const {
  if (amount > 0) {
    wstring code = oe.getDCI().getString("CurrencyCode");
    if (code.empty())
      xml.write(tag, oe.formatCurrency(amount, false));
    else
      xml.write(tag, "currency", code, oe.formatCurrency(amount, false));
  }
}

void IOF30Interface::writeAssignedFee(xmlparser &xml, const oAbstractRunner &tr, int paidForCard) const {
  const oDataConstInterface dci = tr.getDCI();
  int fee = dci.getInt("Fee");
  int taxable = dci.getInt("Taxable");
  int paid = dci.getInt("Paid");

  if (fee == 0 && taxable == 0 && paid == 0)
    return;

  if (paid >= paidForCard) {
    paid -= paidForCard; // Included in card service fee
  }
  const oClass *pc = tr.getClassRef(false);
  if (!splitLateFee || !pc || !tr.hasLateEntryFee()) {
    xml.startTag("AssignedFee");
     string type = tr.hasLateEntryFee() ? "Late" : "Normal";
     xml.startTag("Fee", "type", type);
      xml.write("Name", L"Entry fee");
      writeAmount(xml, "Amount", fee);
      writeAmount(xml, "TaxableAmount", taxable);
     xml.endTag();

    writeAmount(xml, "PaidAmount", paid);
    xml.endTag();
  }
  else {
    int normalFee = pc->getDCI().getInt("ClassFee");
  
    int feeSplit[2] = {fee, 0};
    int paidSplit[2] = {paid, 0};
    if (normalFee > 0) {
      feeSplit[0] = min<int>(normalFee, fee);
      feeSplit[1] = max<int>(0, fee - feeSplit[0]);

      paidSplit[0] = min<int>(paid, feeSplit[0]);
      paidSplit[1] = max<int>(0, paid - paidSplit[0]);
    }

    for (int ft = 0; ft < 2; ft++) {
      xml.startTag("AssignedFee");
       string type = ft == 1 ? "Late" : "Normal";
       xml.startTag("Fee", "type", type);
        xml.write("Name", L"Entry fee");
        writeAmount(xml, "Amount", feeSplit[ft]);
        if (ft == 0)
          writeAmount(xml, "TaxableAmount", taxable);
       xml.endTag();

      writeAmount(xml, "PaidAmount", paidSplit[ft]);
      xml.endTag();
    }
  }
}

void IOF30Interface::writeRentalCardService(xmlparser &xml, int cardFee, bool paid) const {
  xml.startTag("ServiceRequest"); {

    xml.startTag("Service", "type", "RentalCard"); {
      xml.write("Name", L"Card Rental");
    }
    xml.endTag();

    xml.write("RequestedQuantity", L"1");

    xml.startTag("AssignedFee"); {
      xml.startTag("Fee"); {
        xml.write("Name", L"Card Rental Fee");
        writeAmount(xml, "Amount", cardFee);
      }
      xml.endTag();

      if (paid) {
        writeAmount(xml, "PaidAmount", cardFee);
      }
    }
    xml.endTag();
  }
  xml.endTag();
}

void IOF30Interface::getAgeLevels(const vector<FeeInfo> &fees, const vector<int> &ix,
                                  int &normalIx, int &redIx, wstring &youthLimit, wstring &seniorLimit) {
  assert(!ix.empty());
  if (ix.size() == 1) {
    normalIx = ix[0];
    redIx = ix[0];
    return;
  }
  else {
    normalIx = redIx = ix[0];
    for (size_t k = 0; k < ix.size(); k++) {
      if (fees[ix[k]] < fees[redIx])
        redIx = ix[k];
      if (fees[normalIx] < fees[ix[k]])
        normalIx = ix[k];
    }

    for (size_t k = 0; k < ix.size(); k++) {
      const wstring &to = fees[ix[k]].toBirthDate;
      const wstring &from = fees[ix[k]].fromBirthDate;

      if (!from.empty() && (youthLimit.empty() || youthLimit > from) && fees[ix[k]].fee == fees[redIx].fee)
        youthLimit = from;

      if (!to.empty() && (seniorLimit.empty() || seniorLimit > to) && fees[ix[k]].fee == fees[redIx].fee)
        seniorLimit = to;
    }

    
  }
}

int getAgeFromDate(const wstring &date) {
  int y = getThisYear();
  SYSTEMTIME st;
  convertDateYMS(date, st, false);
  if (st.wYear > 1900)
    return y - st.wYear;
  else
    return 0;
}

void IOF30Interface::FeeInfo::add(IOF30Interface::FeeInfo &fi) {
  fee += fi.fee;
  fee += fee*percentage;

  taxable += fi.taxable;

  if (fi.toTime.empty() || (fi.toTime > fromTime && !fromTime.empty())) {
    fi.toTime = fromTime;
    if (!fi.toTime.empty()) {
      SYSTEMTIME st;
      convertDateYMS(fi.toTime, st, false);
      __int64 sec = SystemTimeToInt64Second(st);
      sec -= 3600;
      fi.toTime = convertSystemDate(Int64SecondToSystemTime(sec));
    }
  }
  //if (fi.fromTime.empty() || (fi.fromTime < toTime && !toTime.empty()))
  //  fi.fromTime = toTime;
}

pClass IOF30Interface::readClass(const xmlobject &xclass,
                                 map<int, vector<LegInfo> > &teamClassConfig) {
  if (!xclass)
    return 0;
  int classId = xclass.getObjectInt("Id");
  wstring name, shortName, longName;
  xclass.getObjectString("Name", name);
  xclass.getObjectString("ShortName", shortName);

  if (!shortName.empty()) {
    longName = name;
    name = shortName;
  }

  pClass pc = 0;

  if (classId) {
    pc = oe.getClass(classId);

    if (!pc) {
      oClass c(&oe, classId);
      pc = oe.addClass(c);
    }
  }
  else
    pc = oe.addClass(name);

  oDataInterface DI = pc->getDI();
  if (!pc->hasFlag(oClass::TransferFlags::FlagManualName)) {
    if (!longName.empty()) {
      pc->setName(name, false);
      DI.setString("LongName", longName);
    }
    else {
      if (pc->getName() != name && DI.getString("LongName") != name)
        pc->setName(name, false);
    }
  }
  xmlList legs;
  xclass.getObjects("Leg", legs);
  if (!legs.empty()) {
    vector<LegInfo> &legInfo = teamClassConfig[pc->getId()];
    if (legInfo.size() < legs.size())
      legInfo.resize(legs.size());

    for (size_t k = 0; k < legs.size(); k++) {
      legInfo[k].setMaxRunners(legs[k].getObjectInt("maxNumberOfCompetitors"));
      legInfo[k].setMinRunners(legs[k].getObjectInt("minNumberOfCompetitors"));
    }
  }

  wstring tmp;
  // Status
  xclass.getObjectString("Status", tmp);

  if (tmp == L"Invalidated")
    DI.setString("Status", L"I"); // No refund
  else if (tmp == L"InvalidatedNoFee")
    DI.setString("Status", L"IR"); // Refund

  // No timing
  xclass.getObjectString("resultListMode", tmp);
  if (tmp == L"UnorderedNoTimes")
    pc->setNoTiming(true);

  int minAge = xclass.getObjectInt("minAge");
  if (minAge > 0)
    DI.setInt("LowAge", minAge);

  int highAge = xclass.getObjectInt("maxAge");
  if (highAge > 0)
    DI.setInt("HighAge", highAge);

  xclass.getObjectString("sex", tmp);
  if (!tmp.empty())
    DI.setString("Sex", tmp);

  xmlobject type = xclass.getObject("ClassType");
  if (type) {
    DI.setString("ClassType", type.getObjectString("Id", tmp));
  }

  // XXX we only care about the existance of one race class
  xmlobject raceClass = xclass.getObject("RaceClass");

  if (raceClass) {
    xmlList xFees;
    raceClass.getObjects("Fee", xFees);
    if (xFees.size() > 0) {
      vector<FeeInfo> fees(xFees.size());
      int feeIx = 0;
      int feeLateIx = 0;
      int feeRedIx = 0;
      int feeRedLateIx = 0;

      map<wstring, vector<int> > feePeriods;
      for (size_t k = 0; k < xFees.size(); k++) {
        getFee(xFees[k], fees[k]);
      }

      for (size_t k = 0; k < fees.size(); k++) {
        for (size_t j = k+1; j < fees.size(); j++) {
          if (fees[k].includes(fees[j]))
            fees[j].add(fees[k]);
          if (fees[j].includes(fees[k]))
            fees[k].add(fees[j]);
        }
        feePeriods[fees[k].getDateKey()].push_back(k);
      }

      wstring youthLimit;
      wstring seniorLimit;

      vector<int> &earlyEntry = feePeriods.begin()->second;
      getAgeLevels(fees, earlyEntry, feeIx, feeRedIx, youthLimit, seniorLimit);
      const wstring &lastODate = fees[earlyEntry[0]].toTime;
      if (!lastODate.empty()) {
        oe.getDI().setDate("OrdinaryEntry", lastODate);
      }
      vector<int> &lateEntry = feePeriods.rbegin()->second;
      getAgeLevels(fees, lateEntry, feeLateIx, feeRedLateIx, youthLimit, seniorLimit);

      if (!youthLimit.empty())
        oe.getDI().setInt("YouthAge", getAgeFromDate(youthLimit));

      if (!seniorLimit.empty())
        oe.getDI().setInt("SeniorAge", getAgeFromDate(seniorLimit));

      DI.setInt("ClassFee", oe.interpretCurrency(fees[feeIx].fee, fees[feeIx].currency));
      DI.setInt("HighClassFee", oe.interpretCurrency(fees[feeLateIx].fee, fees[feeLateIx].currency));

      DI.setInt("ClassFeeRed", oe.interpretCurrency(fees[feeRedIx].fee, fees[feeRedIx].currency));
      DI.setInt("HighClassFeeRed", oe.interpretCurrency(fees[feeRedLateIx].fee, fees[feeRedLateIx].currency));

      FeeStatistics feeStat;
      feeStat.fee = fees[feeIx].fee;
      if (feeStat.fee > 0) {
        feeStat.lateFactor = fees[feeLateIx].fee / feeStat.fee;
        feeStatistics.push_back(feeStat);
      }
    }
  }
  pc->synchronize();

  return pc;
}

void IOF30Interface::setupRelayClasses(const map<int, vector<LegInfo> > &teamClassConfig) {
  for (map<int, vector<LegInfo> >::const_iterator it = teamClassConfig.begin();
       it != teamClassConfig.end(); ++it) {
    int classId = it->first;
    const vector<LegInfo> &legs = it->second;
    if (legs.empty())
      continue;
    if (classId > 0) {
      pClass pc = oe.getClass(classId);
      if (!pc) {
        set<wstring> dmy;
        pc = oe.getClassCreate(classId, L"tmp" + itow(classId), dmy);
      }
      setupRelayClass(pc, legs);
    }
  }
}

void IOF30Interface::setupRelayClass(pClass pc, const vector<LegInfo> &legs) {
  if (pc) {
    int nStage = 0;
    for (size_t k = 0; k < legs.size(); k++) {
      nStage += legs[k].maxRunners;
    }
    if (int(pc->getNumStages())>=nStage)
      return; // Do nothing
    
    pc->setNumStages(nStage);
    pc->setStartType(0, STTime, false);
    pc->setStartData(0, oe.getAbsTime(3600));

    int ix = 0;
    for (size_t k = 0; k < legs.size(); k++) {
      for (int j = 0; j < legs[k].maxRunners; j++) {
        if (j>0) {
          if (j < legs[k].minRunners)
            pc->setLegType(ix, LTParallel);
          else
            pc->setLegType(ix, LTExtra);

          pc->setStartType(ix, STChange, false);
        }
        else if (k>0) {
          pc->setLegType(ix, LTNormal);
          pc->setStartType(ix, STChange, false);
        }
        ix++;
      }
    }
  }
}

wstring IOF30Interface::getCurrentTime() const {
  // Don't call this method at midnight!
  return getLocalDate() + L"T" + getLocalTimeOnly();
}

int IOF30Interface::parseISO8601Time(const xmlobject &xo) {
  if (!xo)
    return 0;
  const char *t = xo.getRaw();
  int tIx = -1;
  int zIx = -1;
  for (int k = 0; t[k] != 0; k++) {
    if (t[k] == 'T' || t[k] == 't') {
      if (tIx == -1)
        tIx = k;
      else ;
        // Bad format
    }
    else if (t[k] == '+' || t[k] == '-' || t[k] == 'Z') {
      if (zIx == -1 && tIx != -1)
        zIx = k;
      else ;
        // Bad format
    }
  }
  string date = t;
  string time = tIx >= 0 ? date.substr(tIx+1) : date;
  string zone = (tIx >= 0 && zIx > 0) ? time.substr(zIx - tIx - 1) : "";

  if (tIx > 0) {
    date = date.substr(0, tIx);

    if (zIx > 0)
      time = time.substr(0, zIx - tIx - 1);
  }

  return oe.getRelativeTime(date, time, zone);
}

void IOF30Interface::getLocalDateTime(const string &date, const string &time,
                                      string &dateOut, string &timeOut) {
  int zIx = -1;
  for (size_t k = 0; k < time.length(); k++) {
    if (time[k] == '+' || time[k] == '-' || time[k] == 'Z') {
      if (zIx == -1)
        zIx = k;
      else;
      // Bad format
    }
  }

  if (zIx == -1) {
    dateOut = date;
    timeOut = time;
    return;
  }

  string timePart = time.substr(0, zIx);
  string zone =  time.substr(zIx);
  wstring wTime(timePart.begin(), timePart.end());

  SYSTEMTIME st;
  memset(&st, 0, sizeof(SYSTEMTIME));

  int atime = convertAbsoluteTimeISO(wTime);
  int idate = convertDateYMS(date, st, true);
  if (idate != -1) {
    if (zone == "Z" || zone == "z") {
      st.wHour = atime / 3600;
      st.wMinute = (atime / 60) % 60;
      st.wSecond = atime % 60;

      SYSTEMTIME localTime;
      memset(&localTime, 0, sizeof(SYSTEMTIME));
      SystemTimeToTzSpecificLocalTime(0, &st, &localTime);

      char bf[64];
      sprintf_s(bf, "%02d:%02d:%02d", localTime.wHour, localTime.wMinute, localTime.wSecond);
      timeOut = bf;
      sprintf_s(bf, "%d-%02d-%02d", localTime.wYear, localTime.wMonth, localTime.wDay);
      dateOut = bf;
    }
    else {
      dateOut = date;
      timeOut = time;
    }
  }
}

void IOF30Interface::getLocalDateTime(const wstring &datetime, wstring &dateOut, wstring &timeOut) {
  size_t t = datetime.find_first_of('T');
  if (t != dateOut.npos) {
    wstring date = datetime.substr(0, t);
    wstring time = datetime.substr(t + 1);
    getLocalDateTime(date, time, dateOut, timeOut);
  }
  else {
    dateOut = datetime;
    timeOut = L"";
  }
}

void IOF30Interface::getLocalDateTime(const wstring &date, const wstring &time,
                                      wstring &dateOut, wstring &timeOut) {
  int zIx = -1;
  for (size_t k = 0; k < time.length(); k++) {
    if (time[k] == '+' || time[k] == '-' || time[k] == 'Z') {
      if (zIx == -1)
        zIx = k;
      else;
      // Bad format
    }
  }

  if (zIx == -1) {
    dateOut = date;
    timeOut = time;
    return;
  }

  wstring timePart = time.substr(0, zIx);
  wstring zone = time.substr(zIx);
  wstring wTime(timePart.begin(), timePart.end());

  SYSTEMTIME st;
  memset(&st, 0, sizeof(SYSTEMTIME));

  int atime = convertAbsoluteTimeISO(wTime);
  int idate = convertDateYMS(date, st, true);
  if (idate != -1) {
    if (zone == L"Z" || zone == L"z") {
      st.wHour = atime / 3600;
      st.wMinute = (atime / 60) % 60;
      st.wSecond = atime % 60;

      SYSTEMTIME localTime;
      memset(&localTime, 0, sizeof(SYSTEMTIME));
      SystemTimeToTzSpecificLocalTime(0, &st, &localTime);

      atime = localTime.wHour * 3600 + localTime.wMinute * 60 + localTime.wSecond;
      wchar_t bf[64];
      wsprintf(bf, L"%02d:%02d:%02d", localTime.wHour, localTime.wMinute, localTime.wSecond);
      timeOut = bf;
      wsprintf(bf, L"%d-%02d-%02d", localTime.wYear, localTime.wMonth, localTime.wDay);
      dateOut = bf;
      //dateOut = itow(localTime.wYear) + L"-" + itow(localTime.wMonth) + L"-" + itow(localTime.wDay);
      //timeOut = itow(localTime.wHour) + L":" + itow(localTime.wMinute) + L":" + itow(localTime.wSecond);
    }
    else {
      dateOut = date;
      timeOut = time;
    }
  }
}


void IOF30Interface::getProps(vector<wstring> &props) const {
  props.push_back(L"xmlns");
  props.push_back(L"http://www.orienteering.org/datastandard/3.0");

  props.push_back(L"xmlns:xsi");
  props.push_back(L"http://www.w3.org/2001/XMLSchema-instance");

  props.push_back(L"iofVersion");
  props.push_back(L"3.0");

  props.push_back(L"createTime");
  props.push_back(getCurrentTime());

  props.push_back(L"creator");
  props.push_back(L"MeOS " + getMeosCompectVersion());
}

void IOF30Interface::writeResultList(xmlparser &xml, const set<int> &classes,
                                     int leg,  bool useUTC_, 
                                     bool teamsAsIndividual_, bool unrollLoops_,
                                     bool includeStageInfo_) {
  useGMT = useUTC_;
  includeStageRaceInfo = includeStageInfo_;
  teamsAsIndividual = teamsAsIndividual_;
  unrollLoops = unrollLoops_;
  vector<wstring> props;
  getProps(props);

  props.push_back(L"status");
  props.push_back(L"Complete");

  xml.startTag("ResultList", props);

  writeEvent(xml);

  vector<pClass> c;
  oe.getClasses(c, false);
  vector<pRunner> rToUse;
  vector<pTeam> tToUse;

  for (size_t k = 0; k < c.size(); k++) {
    if (classes.empty() || classes.count(c[k]->getId())) {
      getRunnersToUse(c[k], rToUse, tToUse, leg, false);
      oe.sortRunners(SortOrder::ClassResult, rToUse);
      oe.sortTeams(SortOrder::ClassResult, -1, false, tToUse);

      if (!rToUse.empty() || !tToUse.empty()) {
        writeClassResult(xml, *c[k], rToUse, tToUse);
      }
    }
  }

  xml.endTag();
}

void IOF30Interface::writeClassResult(xmlparser &xml,
                                      const oClass &c,
                                      const vector<pRunner> &r,
                                      const vector<pTeam> &t) {
  pCourse stdCourse = haveSameCourse(r);

  xml.startTag("ClassResult");
  writeClass(xml, c);
  if (stdCourse)
    writeCourse(xml, *stdCourse);

  bool hasInputTime = false;
  for (size_t k = 0; !hasInputTime && k < r.size(); k++) {
    if (r[k]->hasInputData())
      hasInputTime = true;
  }

  for (size_t k = 0; !hasInputTime && k < t.size(); k++) {
    if (t[k]->hasInputData())
      hasInputTime = true;
  }

  for (size_t k = 0; k < r.size(); k++) {
    writePersonResult(xml, *r[k], stdCourse == 0, false, hasInputTime);
  }

  for (size_t k = 0; k < t.size(); k++) {
    writeTeamResult(xml, *t[k], hasInputTime);
  }

  xml.endTag();
}

pCourse IOF30Interface::haveSameCourse(const vector<pRunner> &r) const {
  bool sameCourse = true;
  pCourse stdCourse = r.size() > 0 ? r[0]->getCourse(false) : 0;
  for (size_t k = 1; sameCourse && k < r.size(); k++) {
    int nr = r[k]->getNumMulti();
    for (int j = 0; j <= nr; j++) {
      pRunner tr = r[k]->getMultiRunner(j);
      if (tr && stdCourse != tr->getCourse(true)) {
        sameCourse = false;
        return 0;
      }
    }
  }
  return stdCourse;
}

void IOF30Interface::writeClass(xmlparser &xml, const oClass &c) {
  xml.startTag("Class");
  xml.write("Id", c.getExtIdentifierString()); // Need to call initClassId first
  xml.write("Name", c.getName());

  oClass::ClassStatus stat = c.getClassStatus();
  if (stat == oClass::ClassStatus::Invalid)
    xml.write("Status", L"Invalidated");
  else if (stat == oClass::ClassStatus::InvalidRefund)
    xml.write("Status", L"InvalidatedNoFee");

  xml.endTag();
}

void IOF30Interface::writeCourse(xmlparser &xml, const oCourse &c) {
  xml.startTag("Course");
  writeCourseInfo(xml, c);
  xml.endTag();
}

void IOF30Interface::writeCourseInfo(xmlparser &xml, const oCourse &c) {
  xml.write("Id", c.getId());
  xml.write("Name", c.getName());
  int len = c.getLength();
  if (len > 0)
    xml.write("Length", len);
  int climb = c.getDCI().getInt("Climb");
  if (climb > 0)
    xml.write("Climb", climb);
}

wstring formatStatus(RunnerStatus st, bool hasTime) {
  switch (st) {
    case StatusNoTiming:
      if (!hasTime)
        break;
    case StatusOK:
      return L"OK";
    case StatusDNS:
      return L"DidNotStart";
    case StatusCANCEL:
      return L"Cancelled";
    case StatusMP:
      return L"MissingPunch";
    case StatusDNF:
      return L"DidNotFinish";
    case StatusDQ:
      return L"Disqualified";
    case StatusMAX:
      return L"OverTime";
    case StatusOutOfCompetition:
      if (!hasTime)
        break;
    case StatusNotCompetiting:
      return L"NotCompeting";
  }
  return L"Inactive";
}

void IOF30Interface::writePersonResult(xmlparser &xml, const oRunner &r,
                                       bool includeCourse, bool teamMember, bool hasInputTime) {
  if (!teamMember)
    xml.startTag("PersonResult");
  else
    xml.startTag("TeamMemberResult");

  writePerson(xml, r);
  const pClub pc = r.getClubRef();

  if (pc && !r.isVacant())
    writeClub(xml, *pc, false);

  if (teamMember) {
    oRunner const *resultHolder = &r;
    cTeam t = r.getTeam();
    const oClass *cls = r.getClassRef(false);

    if (t && cls) {
      int leg = r.getLegNumber();
      const int legOrg = leg;
      while (cls->getLegType(leg) == LTIgnore && leg > 0) {
        leg--;
      }
      if (leg < legOrg && t->getRunner(leg))
        resultHolder = t->getRunner(leg);
    }

    writeResult(xml, r, *resultHolder, includeCourse, 
                includeStageRaceInfo && (r.getNumMulti() > 0 || r.getRaceNo() > 0), teamMember, hasInputTime);
  }
  else {
    if (r.getNumMulti() > 0) {
      for (int k = 0; k <= r.getNumMulti(); k++) {
        const pRunner tr = r.getMultiRunner(k);
        if (tr)
          writeResult(xml, *tr, *tr, includeCourse, includeStageRaceInfo, teamMember, hasInputTime);
      }
    }
    else
      writeResult(xml, r, r, includeCourse, false, teamMember, hasInputTime);
  }


  xml.endTag();
}

void IOF30Interface::writeResult(xmlparser &xml, const oRunner &rPerson, const oRunner &r,
                                 bool includeCourse, bool includeRaceNumber,
                                 bool teamMember, bool hasInputTime) {

  vector<SplitData> dummy;
  if (!includeRaceNumber && getStageNumber() == 0)
    xml.startTag("Result");
  else {
    int rn = getStageNumber();
    if (rn == 0)
      rn = 1;
    if (includeRaceNumber)
      rn += rPerson.getRaceNo();
    xml.startTag("Result", "raceNumber", itos(rn));
  }

  if (teamMember)
    writeLegOrder(xml, rPerson.getClassRef(false), rPerson.getLegNumber());

  bool patrolResult = r.getTeam() && r.getClassRef(false)->getClassType() == oClassPatrol && !teamsAsIndividual;

  wstring bib = rPerson.getBib();
  if (!bib.empty())
    xml.write("BibNumber", bib);

  if (r.getStartTime() > 0)
    xml.write("StartTime", oe.getAbsDateTimeISO(r.getStartTime(), true, useGMT));

  bool hasTiming = (!r.getClassRef(false) || r.getClassRef(true)->getNoTiming() == false) &&
                    r.getStatusComputed() != RunnerStatus::StatusNoTiming && !r.noTiming();

  int finishTime, runningTime, place, after;
  RunnerStatus status;
  if (!patrolResult) {
    place = r.getPlace();
    finishTime = r.getFinishTimeAdjusted();
    runningTime = r.getRunningTime(true);
    after = r.getTimeAfter();   
    status = r.getStatusComputed();
  }
  else {
    int pl = r.getParResultLeg();
    place = r.getTeam()->getLegPlace(pl, false);
    runningTime = r.getTeam()->getLegRunningTime(pl, true, false);
    if (runningTime > 0)
      finishTime = r.getStartTime() + runningTime;
    else
      finishTime = 0;
    
    after = r.getTeam()->getTimeAfter(pl);
    status = r.getTeam()->getLegStatus(pl, true, false);
  }

  if (!hasTiming) {
    after = -1;
    runningTime = 0;
    finishTime = 0;
  }

  if (finishTime > 0)
    xml.write("FinishTime", oe.getAbsDateTimeISO(finishTime, true, useGMT));

  if (runningTime > 0)
    xml.write("Time", runningTime);

  if (after >= 0) {
    if (teamMember) {
      xml.write("TimeBehind", "type", L"Leg", itow(after));

      int afterCourse = r.getTimeAfterCourse();
      if (afterCourse >= 0)
        xml.write("TimeBehind", "type", L"Course", itow(afterCourse));
    }
    else
      xml.write("TimeBehind", after);
  }

  if (r.getClassRef(false)) {

    if (r.statusOK(true) && hasTiming) {
      if (!teamMember && place > 0 && place < 50000) {
        xml.write("Position", place);
      }
      else if (teamMember) {
        if (place > 0 && place < 50000)
          xml.write("Position", "type", L"Leg", itow(place));

        int placeCourse = r.getCoursePlace(true);
        if (placeCourse > 0)
          xml.write("Position", "type", L"Course", itow(placeCourse));
      }
    }

    xml.write("Status", formatStatus(status, r.getFinishTime()>0));

    int rg = r.getRogainingPoints(true, false);
    if (rg > 0) {
      xml.write("Score", "type", L"Score", itow(rg));
      xml.write("Score", "type", L"Penalty", itow(r.getRogainingReduction(true)));
    }
    if ( (r.getTeam() && r.getClassRef(false)->getClassType() != oClassPatrol && !teamsAsIndividual) || hasInputTime) {
      xml.startTag("OverallResult");

      int rt = r.getTotalRunningTime();
      if (rt > 0 && hasTiming)
        xml.write("Time", rt);

      RunnerStatus stat = r.getTotalStatus();

      int tleg = r.getLegNumber() >= 0 ? r.getLegNumber() : 0;

      if (stat == StatusOK && hasTiming) {
        int after = r.getTotalRunningTime() - 
          r.getClassRef(true)->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
        if (after >= 0)
          xml.write("TimeBehind", after);
      }

      if (stat == StatusOK && hasTiming)
        xml.write("Position", r.getTotalPlace());

      xml.write("Status", formatStatus(stat, r.getFinishTime() > 0));

      xml.endTag();
    }
    bool doUnroll = unrollLoops && r.getNumShortening() == 0;
    pCourse crs = r.getCourse(!doUnroll);
    if (crs) {
      if (includeCourse)
        writeCourse(xml, *crs);

      const vector<SplitData> &sp = r.getSplitTimes(doUnroll);
      RunnerStatus st = r.getStatusComputed();
      if (r.getStatus()>0 && st != StatusDNS && 
                             st != StatusCANCEL && 
                             st != StatusNotCompetiting) {
        int nc = crs->getNumControls();
        bool hasRogaining = crs->hasRogaining();
        int firstControl = crs->useFirstAsStart() ? 1 : 0;
        if (crs->useLastAsFinish()) {
          nc--;
        }
        set< pair<unsigned, int> > rogaining;
        for (int k = firstControl; k<nc; k++) {
          if (size_t(k) >= sp.size())
            break;
          if (crs->getControl(k)->isRogaining(hasRogaining)) {
            if (sp[k].hasTime()) {
              int time = sp[k].time - r.getStartTime();
              int control = crs->getControl(k)->getFirstNumber();
              rogaining.insert(make_pair(time, control));
            }
            else if (!sp[k].isMissing()) {
              int control = crs->getControl(k)->getFirstNumber();
              rogaining.insert(make_pair(-1, control));
            }
            continue;
          }

          if (sp[k].isMissing())
            xml.startTag("SplitTime", "status", "Missing");
          else
            xml.startTag("SplitTime");
          xml.write("ControlCode", crs->getControl(k)->getFirstNumber());
          if (sp[k].hasTime() && hasTiming)
            xml.write("Time", sp[k].time - r.getStartTime());
          xml.endTag();
        }

        for (set< pair<unsigned, int> >::iterator it = rogaining.begin(); it != rogaining.end(); ++it) {
          xml.startTag("SplitTime", "status", "Additional");
          xml.write("ControlCode", it->second);
          if (it->first != -1)
            xml.write("Time", it->first);
          xml.endTag();
        }

        oCard *card = r.getCard();
        if (card) { // Write additional punches
          vector<pPunch> plist;
          card->getPunches(plist);
          for (pPunch p : plist) {
            if (p->getTypeCode() >= 30 && !p->isUsedInCourse()) {
              xml.startTag("SplitTime", "status", "Additional");
              xml.write("ControlCode", p->getTypeCode());
              if (p->getTimeInt() > r.getStartTime())
                xml.write("Time", p->getTimeInt() - r.getStartTime());
              xml.endTag();
            }
          }
        }
      }
    }
  }

  if (rPerson.getCardNo() > 0)
    xml.write("ControlCard", rPerson.getCardNo());

  writeFees(xml, rPerson);

  if (!hasTiming) {
    xml.startTag("Extensions");
    xml.write("TimePresentation", L"false");
    xml.endTag();
  }

  xml.endTag();
}

void IOF30Interface::writeFees(xmlparser &xml, const oRunner &r) const {
  int cardFee = r.getDCI().getInt("CardFee");
  bool paidCard = r.getDCI().getInt("Paid") >= cardFee;
  
  writeAssignedFee(xml, r, paidCard ? cardFee : 0);

  if (cardFee > 0) 
    writeRentalCardService(xml, cardFee, paidCard);
}

void IOF30Interface::writeTeamResult(xmlparser &xml, const oTeam &t, bool hasInputTime) {
  xml.startTag("TeamResult");

  xml.write("EntryId", t.getId());
  xml.write("Name", t.getName());

  if (t.getClubRef())
    writeClub(xml, *t.getClubRef(), false);

  wstring bib = t.getBib();
  if (!bib.empty())
    xml.write("BibNumber", bib);

  for (int k = 0; k < t.getNumRunners(); k++) {
    if (t.getRunner(k))
      writePersonResult(xml, *t.getRunner(k), true, true, hasInputTime);
  }

  writeAssignedFee(xml, t, 0);
  xml.endTag();
}

int IOF30Interface::getStageNumber() {
  if (cachedStageNumber >= 0)
    return cachedStageNumber;
  int sn = oe.getStageNumber();
  if (sn != 0) {
    if (sn < 0)
      sn = 0;
    cachedStageNumber = sn;
    return sn;
  }
  bool pre = oe.hasPrevStage();
  bool post = oe.hasNextStage();

  if (!pre && !post) {
    cachedStageNumber = 0;
  }
  else if (!pre && post) {
    cachedStageNumber = 1;
  }
  else {
    cachedStageNumber = 1;
    // Guess from stage name
    wstring name = oe.getName();
    if (!name.empty()) {
      wchar_t d = name[name.length() -1];
      if (d>='1' && d<='9')
        cachedStageNumber = d - '0';
    }
  }

  return cachedStageNumber;
}

void IOF30Interface::writeEvent(xmlparser &xml) {
  xml.startTag("Event");
  xml.write("Id", oe.getExtIdentifierString());
  xml.write("Name", oe.getName());
  xml.startTag("StartTime");
  xml.write("Date", oe.getDate());
  xml.write("Time", oe.getAbsDateTimeISO(0, false, useGMT));
  xml.endTag();

  if (getStageNumber()) {
    xml.startTag("Race");
    xml.write("RaceNumber", getStageNumber());

    xml.write("Name", oe.getName());

    xml.startTag("StartTime");
    xml.write("Date", oe.getDate());
    xml.write("Time", oe.getAbsDateTimeISO(0, false, useGMT));
    xml.endTag();

    xml.endTag();
  }

  xml.endTag();
}

void IOF30Interface::writePerson(xmlparser &xml, const oRunner &r) {
  xml.startTag("Person");

  __int64 id = r.getExtIdentifier();
  if (id != 0)
    xml.write("Id", r.getExtIdentifierString());

  xml.startTag("Name");
  xml.write("Family", r.getFamilyName());
  xml.write("Given", r.getGivenName());
  xml.endTag();

  xml.endTag();
}

void IOF30Interface::writeClub(xmlparser &xml, const oClub &c, bool writeExtended) const {
  if (c.isVacant() || c.getName().empty())
    return;

  if (writeExtended) {
    const wstring &type = c.getDCI().getString("Type");
    if (type.empty())
      xml.startTag("Organisation");
    else
      xml.startTag("Organisation", "type", type);
  }
  else {
    xml.startTag("Organisation");
  }
  __int64 id = c.getExtIdentifier();
  if (id != 0)
    xml.write("Id", c.getExtIdentifierString());

  xml.write("Name", c.getName());
  wstring sname = c.getDCI().getString("ShortName");
  if (!sname.empty())
    xml.write("ShortName", sname);

  wstring ctry = c.getDCI().getString("Country");
  wstring nat = c.getDCI().getString("Nationality");

  if (!ctry.empty() || !nat.empty()) {
    if (ctry.empty()) {
      if (nat == L"SWE")
        ctry = L"Sweden";
      else if (nat == L"FR" || nat == L"FRA")
        ctry = L"France";
      else
        ctry = nat;
    }
    xml.write("Country", "code", nat, ctry);
  }

  if (writeExtended) {
    oDataConstInterface di = c.getDCI();
    const wstring &street = di.getString("Street");
    const wstring &co = di.getString("CareOf");
    const wstring &city = di.getString("City");
    const wstring &state = di.getString("State");
    const wstring &zip = di.getString("ZIP");
    const wstring &email = di.getString("EMail");
    const wstring &phone = di.getString("Phone");

    if (!street.empty()) {
      xml.startTag("Address");
      xml.write("Street", street);
      if (!co.empty())
        xml.write("CareOf", co);
      if (!zip.empty())
        xml.write("ZipCode", zip);
      if (!city.empty())
        xml.write("City", city);
      if (!state.empty())
        xml.write("State", state);
      if (!ctry.empty())
        xml.write("Country", ctry);
      xml.endTag();
    }

    if (!email.empty())
      xml.write("Contact", "type", L"EmailAddress", email);

    if (!phone.empty())
      xml.write("Contact", "type", L"PhoneNumber", phone);

    int dist = di.getInt("District");
    if (dist > 0)
      xml.write("ParentOrganisationId", dist);
  }

  xml.endTag();
}

void IOF30Interface::writeStartList(xmlparser &xml, const set<int> &classes, bool useUTC_, 
                                    bool teamsAsIndividual_, bool includeStageInfo_) {
  useGMT = useUTC_;
  teamsAsIndividual = teamsAsIndividual_;
  includeStageRaceInfo = includeStageInfo_;
  vector<wstring> props;
  getProps(props);

  xml.startTag("StartList", props);

  writeEvent(xml);

  vector<pClass> c;
  oe.getClasses(c, false);
  vector<pRunner> rToUse;
  vector<pTeam> tToUse;

  for (size_t k = 0; k < c.size(); k++) {
    if (classes.empty() || classes.count(c[k]->getId())) {
      getRunnersToUse(c[k], rToUse, tToUse, -1, true);
      oe.sortRunners(SortOrder::ClassStartTime, rToUse);
      oe.sortTeams(SortOrder::ClassStartTime, 0, false, tToUse);
      if (!rToUse.empty() || !tToUse.empty()) {
        writeClassStartList(xml, *c[k], rToUse, tToUse);
      }
    }
  }
  xml.endTag();
}

void IOF30Interface::getRunnersToUse(const pClass cls, vector<pRunner> &rToUse,
                                     vector<pTeam> &tToUse, int leg, bool includeUnknown) const {

  rToUse.clear();
  tToUse.clear();
  vector<pRunner> r;
  vector<pTeam> t;

  int classId = cls->getId();
  bool indRel = cls->getClassType() == oClassIndividRelay;

  oe.getRunners(classId, 0, r, false);
  rToUse.reserve(r.size());

  for (size_t j = 0; j < r.size(); j++) {
    if (leg == -1 || leg == r[j]->getLegNumber()) {

      if (!teamsAsIndividual) {
        if (leg == -1 && indRel && r[j]->getLegNumber() != 0)
          continue; // Skip all but leg 0 for individual relay

        if (leg == -1 && !indRel && r[j]->getTeam())
          continue; // For teams, skip presonal results, unless individual relay

        if (!includeUnknown && !r[j]->hasResult())
          continue;
      }
      rToUse.push_back(r[j]);
    }
  }

  if (leg == -1 && !teamsAsIndividual) {
    oe.getTeams(classId, t, false);
    tToUse.reserve(t.size());

    for (size_t j = 0; j < t.size(); j++) {
      if (includeUnknown)
         tToUse.push_back(t[j]);
      else {
        for (int n = 0; n < t[j]->getNumRunners(); n++) {
          pRunner tr = t[j]->getRunner(n);
          if (tr && tr->hasResult()) {
            tToUse.push_back(t[j]);
            break;
          }
        }
      }
    }
  }
}

void IOF30Interface::writeClassStartList(xmlparser &xml, const oClass &c,
                                         const vector<pRunner> &r,
                                         const vector<pTeam> &t) {

  pCourse stdCourse = haveSameCourse(r);

  xml.startTag("ClassStart");
  writeClass(xml, c);
  if (stdCourse)
    writeCourse(xml, *stdCourse);

  wstring start = c.getStart();
  if (!start.empty())
    xml.write("StartName", start);

  for (size_t k = 0; k < r.size(); k++) {
    writePersonStart(xml, *r[k], stdCourse == 0, false);
  }

  for (size_t k = 0; k < t.size(); k++) {
    writeTeamStart(xml, *t[k]);
  }
  xml.endTag();
}

void IOF30Interface::writePersonStart(xmlparser &xml, const oRunner &r, bool includeCourse, bool teamMember) {
  if (!teamMember)
    xml.startTag("PersonStart");
  else
    xml.startTag("TeamMemberStart");

  writePerson(xml, r);
  const pClub pc = r.getClubRef();

  if (pc && !r.isVacant())
    writeClub(xml, *pc, false);

  if (teamMember) {
    writeStart(xml, r, includeCourse, includeStageRaceInfo && (r.getNumMulti() > 0 || r.getRaceNo() > 0), teamMember);
  }
  else {
    if (r.getNumMulti() > 0) {
      for (int k = 0; k <= r.getNumMulti(); k++) {
        const pRunner tr = r.getMultiRunner(k);
        if (tr)
          writeStart(xml, *tr, includeCourse, includeStageRaceInfo, teamMember);
      }
    }
    else
      writeStart(xml, r, includeCourse, false, teamMember);
  }

  xml.endTag();
}

void IOF30Interface::writeTeamNoPersonStart(xmlparser &xml, const oTeam &t, int leg, bool includeRaceNumber) {
  xml.startTag("TeamMemberStart");

  const pClub pc = t.getClubRef();
  pClass cls = t.getClassRef(false);
  if (pc && !pc->isVacant())
    writeClub(xml, *pc, false);

  {
    if (!includeRaceNumber || (getStageNumber() == 0 && (cls && cls->getLegRunnerIndex(leg)==0)))
      xml.startTag("Start");
    else {
      int rn = getStageNumber();
      if (rn == 0)
        rn = 1;
      if (includeStageRaceInfo)
        rn += cls->getLegRunnerIndex(leg);

      xml.startTag("Start", "raceNumber", itos(rn));
    }
    
    writeLegOrder(xml, cls, leg);

    wstring bib = t.getBib();
    if (!bib.empty())
      xml.write("BibNumber", bib);
    int startTime = 0;
    if (cls && cls->getStartType(leg) == StartTypes::STTime)
      startTime = cls->getStartData(leg);
    if (startTime > 0)
      xml.write("StartTime", oe.getAbsDateTimeISO(startTime, true, useGMT));

    xml.endTag();
  }

  xml.endTag();
}

void IOF30Interface::writeTeamStart(xmlparser &xml, const oTeam &t) {
  xml.startTag("TeamStart");

  xml.write("EntryId", t.getId());
  xml.write("Name", t.getName());

  if (t.getClubRef())
    writeClub(xml, *t.getClubRef(), false);

  wstring bib = t.getBib();
  if (!bib.empty())
    xml.write("BibNumber", bib);

  pClass cls = t.getClassRef(false);

  for (int k = 0; k < t.getNumRunners(); k++) {
    if (t.getRunner(k))
      writePersonStart(xml, *t.getRunner(k), true, true);
    else if (cls && !cls->isOptional(k))
      writeTeamNoPersonStart(xml, t, k, includeStageRaceInfo);
  }

  writeAssignedFee(xml, t, 0);
  xml.endTag();
}

void IOF30Interface::writeStart(xmlparser &xml, const oRunner &r,
                                bool includeCourse, bool includeRaceNumber,
                                bool teamMember) {
  if (!includeRaceNumber && getStageNumber() == 0)
    xml.startTag("Start");
  else {
    int rn = getStageNumber();
    if (rn == 0)
      rn = 1;
    if (includeStageRaceInfo)
      rn += r.getRaceNo();

    xml.startTag("Start", "raceNumber", itos(rn));
  }
  if (teamMember)
    writeLegOrder(xml, r.getClassRef(false), r.getLegNumber());

  wstring bib = r.getBib();
  if (!bib.empty())
    xml.write("BibNumber", bib);

  if (r.getStartTime() > 0)
    xml.write("StartTime", oe.getAbsDateTimeISO(r.getStartTime(), true, useGMT));

  pCourse crs = r.getCourse(true);
  if (crs && includeCourse)
    writeCourse(xml, *crs);

  if (r.getCardNo() > 0)
    xml.write("ControlCard", r.getCardNo());

  writeFees(xml, r);

  xml.endTag();
}

void IOF30Interface::writeLegOrder(xmlparser &xml, const oClass *pc, int legNo) const {
  // Team member race result
  int legNumber, legOrder;  
  if (pc) {
    bool par = pc->splitLegNumberParallel(legNo, legNumber, legOrder);
    xml.write("Leg", legNumber + 1);
    if (par)
      xml.write("LegOrder", legOrder + 1);
  }
}

bool IOF30Interface::readXMLCompetitorDB(const xmlobject &xCompetitor) {

  if (!xCompetitor) return false;

  xmlobject person = xCompetitor.getObject("Person");

  if (!person) return false;
  
  int pidI;
  long long pid;
  readId(person, pidI, pid);
  /*
  wstring pidS;
  person.getObjectString("Id", pidS);xxx
  long long pid = oBase::converExtIdentifierString(pidS);*/
  xmlobject pname = person.getObject("Name");
  if (!pname) return false;

  int cardno=0;
  string tmp;

  xmlList cards;
  xCompetitor.getObjects("ControlCard", cards);

  for (size_t k= 0; k<cards.size(); k++) {
    xmlobject &card = cards[k];
    if (card) {
      xmlattrib pSystem = card.getAttrib("punchingSystem");
      if (!pSystem || _stricmp(pSystem.get(), "SI") == 0) {
        cardno = card.getObjectInt(0);
        break;
      }
    }
  }

  wstring given;
  pname.getObjectString("Given", given);
  getFirst(given, 2);
  wstring family;
  pname.getObjectString("Family", family);

  if (given.empty() || family.empty())
    return false;

  //string name(given+" "+family);
  wstring name(family + L", " + given);

  char sex[2];
  person.getObjectString("sex", sex, 2);

  int birth = person.getObjectInt("BirthDate");

  xmlobject nat=person.getObject("Nationality");

  char national[4]={0,0,0,0};
  if (nat) {
    nat.getObjectString("code", national, 4);
  }

  int clubId = 0;
  xmlobject xClub = xCompetitor.getObject("Organisation");
  if (xClub) {
    clubId = xClub.getObjectInt("Id");
  }

  RunnerDB &runnerDB = oe.getRunnerDatabase();

  RunnerWDBEntry *rde = runnerDB.getRunnerById(pid);

  if (!rde) {
    rde = runnerDB.getRunnerByCard(cardno);

    if (rde && rde->getExtId()!=0)
      rde = 0; //Other runner, same card

    if (!rde)
      rde = runnerDB.addRunner(name.c_str(), pid, clubId, cardno);
  }

  if (rde) {
    rde->setExtId(pid);
    rde->setName(name.c_str());
    rde->dbe().clubNo = clubId;
    rde->dbe().birthYear = extendYear(birth);
    rde->dbe().sex = sex[0];
    memcpy(rde->dbe().national, national, 3);
  }
  return true;
}

void IOF30Interface::writeXMLCompetitorDB(xmlparser &xml, const RunnerDB &db, 
                                          const RunnerWDBEntry &rde) const {
  wstring s = rde.getSex();

  xml.startTag("Competitor");

  if (s.empty())
    xml.startTag("Person");
  else
    xml.startTag("Person", "sex", s);

  long long pid = rde.getExtId();
  if (pid > 0) {
    wchar_t bf[16];
    oBase::converExtIdentifierString(pid, bf);
    xml.write("Id", bf);
  }
  xml.startTag("Name");
  xml.write("Given", rde.getGivenName());
  xml.write("Family", rde.getFamilyName());
  xml.endTag();

  if (rde.getBirthYear() > 1900)
    xml.write("BirthDate", itow(rde.getBirthYear()) + L"-01-01");

  wstring nat = rde.getNationality();
  if (!nat.empty()) {
    xml.write("Nationality", "code", nat.c_str());
  }

  xml.endTag(); // Person

  if (rde.dbe().cardNo > 0) {
    xml.write("ControlCard", "punchingSystem", L"SI", itow(rde.dbe().cardNo));
  }


  if (rde.dbe().clubNo > 0) {
    pClub clb = db.getClub(rde.dbe().clubNo);
    if (clb) {
      uint64_t extId = clb->getExtIdentifier();
      if (extId != 0) {
        xml.startTag("Organisation");
        xml.write("Id", int(extId));
        xml.endTag();
      }
      else {
        writeClub(xml, *clb, false);
      }
    }
  }

  xml.endTag(); // Competitor
}

int getStartIndex(int sn);
int getFinishIndex(int sn);
wstring getStartName(const wstring &start);

int IOF30Interface::getStartIndex(const wstring &startId) {
  int num = getNumberSuffix(startId);
  if (num == 0 && startId.length()>0)
    num = int(startId[startId.length()-1])-'0';
  return ::getStartIndex(num);
}

bool IOF30Interface::readControl(const xmlobject &xControl) {
  if (!xControl)
    return false;

  wstring idStr;
  xControl.getObjectString("Id", idStr);

  if (idStr.empty())
    return false;

  int type = -1;
  int code = 0;
  if (idStr[0] == 'S')
    type = 1;
  else if (idStr[0] == 'F')
    type = 2;
  else {
    type = 0;
    code = _wtoi(idStr.c_str());
    if (code <= 0)
      return false;
  }

  xmlobject pos = xControl.getObject("MapPosition");

  int xp = 0, yp = 0;
  if (pos) {
    string x,y;
    pos.getObjectString("x", x);
    pos.getObjectString("y", y);
    xp = int(10.0 * atof(x.c_str()));
    yp = int(10.0 * atof(y.c_str()));
  }

  int longitude = 0, latitude = 0;

  xmlobject geopos = xControl.getObject("Position");

  if (geopos) {
    string lat,lng;
    geopos.getObjectString("lat", lat);
    geopos.getObjectString("lng", lng);
    latitude = int(1e6 * atof(lat.c_str()));
    longitude = int(1e6 * atof(lng.c_str()));
  }
  pControl pc = 0;

  if (type == 0) {
    pc = oe.getControl(code, true);
  }
  else if (type == 1) {
    wstring start = getStartName(trim(idStr));
    pc = oe.getControl(getStartIndex(idStr), true);
    pc->setNumbers(L"");
    pc->setName(start);
    pc->setStatus(oControl::StatusStart);
  }
  else if (type == 2) {
    wstring finish = trim(idStr);
    int num = getNumberSuffix(finish);
    if (num == 0 && finish.length()>0)
      num = int(finish[finish.length()-1])-'0';
    if (num > 0 && num<10)
      finish = lang.tl(L"Mål ") + itow(num);
    else
      finish = lang.tl(L"Mål");
    pc = oe.getControl(getFinishIndex(num), true);
    pc->setNumbers(L"");
    pc->setName(finish);
    pc->setStatus(oControl::StatusFinish);
  }

  if (pc) {
    pc->getDI().setInt("xpos", xp);
    pc->getDI().setInt("ypos", yp);
    pc->getDI().setInt("longcrd", longitude);
    pc->getDI().setInt("latcrd", latitude);
    pc->synchronize();
  }
  return true;
}

void IOF30Interface::readCourseGroups(xmlobject xClassCourse, vector< vector<pCourse> > &crs) {
  xmlList groups;
  xClassCourse.getObjects("CourseGroup", groups);

  for (size_t k = 0; k < groups.size(); k++) {
    xmlList courses;
    groups[k].getObjects("Course", courses);
    crs.push_back(vector<pCourse>());
    for (size_t j = 0; j < courses.size(); j++) {
      pCourse pc = readCourse(courses[j]);
      if (pc)
        crs.back().push_back(pc);
    }
  }
}

wstring IOF30Interface::constructCourseName(const wstring &family, const wstring &name) {
  if (family.empty())
    return trim(name);
  else
    return trim(family) + L":" + trim(name);
}

wstring IOF30Interface::constructCourseName(const xmlobject &xcrs) {
  wstring name, family;
  xcrs.getObjectString("Name", name);
  if (name.empty())
    // CourseAssignment case
    xcrs.getObjectString("CourseName", name);

  xcrs.getObjectString("CourseFamily", family);

  return constructCourseName(family, name);
}

pCourse IOF30Interface::readCourse(const xmlobject &xcrs) {
  if (!xcrs)
    return 0;

  string sId;
  xcrs.getObjectString("Id", sId);
  int cid = 0;
  if (sId.length() > 0) {
    sId = trim(sId);
    if (isNumber(sId))
      cid = atoi(sId.c_str());
    else {
      // Handle non-numeric id. Hash. Uniqeness ensured below.
      for (size_t j = 0; j < sId.length(); j++)
        cid = 31 * cid + sId.at(j);

      cid = cid & 0xFFFFFFF;
    }
  }
  
  if (!readCrsIds.insert(cid).second)
    cid = 0; // Ignore for duplicates

  wstring name = constructCourseName(xcrs);
  /*, family;
  xcrs.getObjectString("Name", name);
  xcrs.getObjectString("CourseFamily", family);

  if (family.empty())
    name = trim(name);
  else
    name = trim(family) + ":" + trim(name);
*/
  int len = xcrs.getObjectInt("Length");
  int climb = xcrs.getObjectInt("Climb");

  xmlList xControls;
  xcrs.getObjects("CourseControl", xControls);

  vector<pControl> ctrlCode;
  vector<int> legLen;
  wstring startName;
  bool hasRogaining = false;

  for (size_t k = 0; k < xControls.size(); k++) {
    string type;
    xControls[k].getObjectString("type", type);
    if (type == "Start") {
      wstring idStr;
      xControls[k].getObjectString("Control", idStr);
      pControl pStart = oe.getControl(getStartIndex(idStr), false);
      if (pStart)
        startName = pStart->getName();
    }
    else if (type == "Finish") {
      legLen.push_back(xControls[k].getObjectInt("LegLength"));
    }
    else {
      xmlList xPunchControls;
      xControls[k].getObjects("Control", xPunchControls);
      pControl pCtrl = 0;
      if (xPunchControls.size() == 1) {
        pCtrl = oe.getControl(xPunchControls[0].getInt(), true);
      }
      else if (xPunchControls.size()>1) {
        pCtrl = oe.addControl(1000*cid + xPunchControls[0].getInt(),xPunchControls[0].getInt(), L"");
        if (pCtrl) {
          wstring cc;
          for (size_t j = 0; j < xPunchControls.size(); j++)
            cc += wstring(xPunchControls[j].getw()) + L" ";

          pCtrl->setNumbers(cc);
        }
      }

      if (pCtrl) {
        legLen.push_back(xControls[k].getObjectInt("LegLength"));
        ctrlCode.push_back(pCtrl);
        int score = xControls[k].getObjectInt("Score");
        if (score > 0) {
          pCtrl->getDI().setInt("Rogaining", score);
          pCtrl->setStatus(oControl::StatusRogaining);
          hasRogaining = true;
        }
      }
    }
  }

  pCourse pc = 0;
  if (cid > 0)
    pc = oe.getCourseCreate(cid);
  else {
    pc = oe.getCourse(name);
    if (pc == 0)
      pc = oe.addCourse(name);

    readCrsIds.insert(pc->getId());
  }

  if (pc) {
    pc->setName(name);
    pc->setLength(len);
    pc->importControls("", true, false);
    for (size_t i = 0; i<ctrlCode.size(); i++) {
      pc->addControl(ctrlCode[i]->getId());
    }
    if (pc->getNumControls() + 1 == legLen.size())
      pc->setLegLengths(legLen);
    pc->getDI().setInt("Climb", climb);

    pc->setStart(startName, true);
    if (hasRogaining) {
      int mt = oe.getMaximalTime();
      if (mt == 0)
        mt = 3600;
      pc->setMaximumRogainingTime(mt);
    }

    pc->synchronize();
  }
  return pc;
}


void IOF30Interface::bindClassCourse(oClass &pc, const vector< vector<pCourse> > &crs) {
  if (crs.empty())
    return;
  if (crs.size() == 1 && crs[0].size() == 0)
    pc.setCourse(crs[0][0]);
  else {
    unsigned ns = pc.getNumStages();
    ns = max<unsigned>(ns, crs.size());
    pc.setNumStages(ns);
    for (size_t k = 0; k < crs.size(); k++) {
      pc.clearStageCourses(k);
      for (size_t j = 0; j < crs[k].size(); j++) {
        pc.addStageCourse(k, crs[k][j]->getId(), -1);
      }
    }
  }
}


void IOF30Interface::writeCourses(xmlparser &xml) {
  vector<wstring> props;
  getProps(props);
  xml.startTag("CourseData", props);

  writeEvent(xml);

  vector<pControl> ctrl;
  vector<pCourse> crs;
  oe.getControls(ctrl, false);

  xml.startTag("RaceCourseData");
  map<int, wstring> ctrlId2ExportId;

  // Start
  xml.startTag("Control");
  xml.write("Id", L"S");
  xml.endTag();
  set<wstring> ids;
  for (size_t k = 0; k < ctrl.size(); k++) {
    if (ctrl[k]->getStatus() != oControl::StatusFinish && ctrl[k]->getStatus() != oControl::StatusStart) {
      wstring id = writeControl(xml, *ctrl[k], ids);
      ctrlId2ExportId[ctrl[k]->getId()] = id;
    }
  }

  // Finish
  xml.startTag("Control");
  xml.write("Id", L"F");
  xml.endTag();

  oe.getCourses(crs);
  for (size_t k = 0; k < crs.size(); k++) {
    writeFullCourse(xml, *crs[k], ctrlId2ExportId);
  }

  xml.endTag();

  xml.endTag();
}

wstring IOF30Interface::writeControl(xmlparser &xml, const oControl &c, set<wstring> &writtenId) {
  int id = c.getFirstNumber();
  wstring ids = itow(id);
  if (writtenId.count(ids) == 0) {
    xml.startTag("Control");
    xml.write("Id", ids);
/*
    <!-- the position of the control given in latitude and longitude -->

<!-- coordinates west of the Greenwich meridian and south of the equator are expressed by negative numbers -->
 <Position lng="17.687623" lat="59.760069"/>
<!-- the position of the control on the printed map, relative to the map's lower left corner -->
 <MapPosition y="58" x="187" unit="mm"/>
 */

    xml.endTag();
    writtenId.insert(ids);
  }

  return ids;

}

void IOF30Interface::writeFullCourse(xmlparser &xml, const oCourse &c,
                                       const map<int, wstring> &ctrlId2ExportId) {

  xml.startTag("Course");
  writeCourseInfo(xml, c);

  xml.startTag("CourseControl", "type", "Start");
  xml.write("Control", L"S");
  xml.endTag();

  for (int i = 0; i < c.getNumControls(); i++) {
    int id = c.getControl(i)->getId();
    xml.startTag("CourseControl", "type", "Control");
    if (ctrlId2ExportId.count(id))
      xml.write("Control", ctrlId2ExportId.find(id)->second);
    else
      throw exception();
    xml.endTag();
  }

  xml.startTag("CourseControl", "type", L"Finish");
  xml.write("Control", L"F");
  xml.endTag();

  xml.endTag();
}

void IOF30Interface::writeRunnerDB(const RunnerDB &db, xmlparser &xml) const {
  vector<wstring> props;
  getProps(props);

  xml.startTag("CompetitorList", props);

  const vector<RunnerWDBEntry> &rdb = db.getRunnerDB();
  for (size_t k = 0; k < rdb.size(); k++) {
    if (!rdb[k].isRemoved())
      writeXMLCompetitorDB(xml, db, rdb[k]);
  }

  xml.endTag();
}

void IOF30Interface::writeClubDB(const RunnerDB &db, xmlparser &xml) const {
  vector<wstring> props;
  getProps(props);

  xml.startTag("OrganisationList", props);

  const vector<oDBClubEntry> &cdb = db.getClubDB(true);
  for (size_t k = 0; k < cdb.size(); k++) {
    if (!cdb[k].isRemoved())
      writeClub(xml, cdb[k], true);
  }

  xml.endTag();
}

void IOF30Interface::getIdTypes(vector<string> &types) {
  types.clear();
  types.insert(types.begin(), idProviders.begin(), idProviders.end());
}

void IOF30Interface::setPreferredIdType(const string &type) {
  preferredIdProvider = type;
}
