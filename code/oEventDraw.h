#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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

#include "oEvent.h"

struct ClassDrawSpecification {
  int classID = -1;
  int startGroup = 0;
  int leg = -1;
  mutable int firstStart = 0;
  mutable int interval = 0;
  int vacances = 0;
  mutable int ntimes = 0;
  oEvent::VacantPosition vacantPosition = oEvent::VacantPosition::Mixed;
  ClassDrawSpecification() = default;
  ClassDrawSpecification(int classID, int leg, int firstStart, int interval, int vacances, oEvent::VacantPosition vp) :
                         classID(classID), leg(leg), firstStart(firstStart), 
                         interval(interval), vacances(vacances), vacantPosition(vp) {}
};

/** Struct with info to draw a class */
struct ClassInfo {
  enum class Hint {
    Default = 0,
    Early = 1,
    Late = 2,
    Fixed = 3
  };

  int classId = 0;
  int startGroupId = 0;
  pClass pc = nullptr;

  int firstStart = 0;
  int firstStartComputed = -1;

  int interval = 0;
  int fixedInterval = 0;

  int unique = 0;

  enum class Speed {
    Normal, 
    Slow,
    Fast
  };
  Speed speed = Speed::Normal;
  int courseId = 0;
  int nRunners = 0;
  int nRunnersGroup = 0; // Number of runners in group
  int nRunnersCourse = 0; // Number of runners on this course

  bool nVacantSpecified = 0;
  int nVacant = -1;

  bool nExtraSpecified = 0;
  int nExtra = 0;

  int sortFactor = 0;

  // Algorithm status. Extra time needed to start this class.
  int overShoot = 0;

  Hint hint = Hint::Default;

  // Position for warnig information (in GUI)
  mutable int warnX = 0;
  mutable int warnY = 0;
  mutable uint64_t warnHash = 0;

  /** Mark specified warning as shown. Returns true if already shown before. */
  bool showWarning(const wstring&) const;

  bool hasShownWarning() const { return warnHash != 0; }

  void resetWarning() const { warnHash = 0; }

  ClassInfo() = default;

  ClassInfo(pClass pc) : pc(pc), classId(pc->getId()) {
  }

  // Selection of sorting method
  static int sSortOrder;
  bool operator<(const ClassInfo &ci) const;
};


/**Structure for optimizing start order */
struct DrawInfo {
  DrawInfo();

  double vacancyFactor = 0.05;
  double extraFactor = 0.1;
  int minVacancy = 1;
  int maxVacancy = 10;
  int baseInterval = timeConstMinute;
  int minClassInterval = 2 * timeConstMinute;
  int maxClassInterval = 4 * timeConstMinute;
  int nFieldsMax = 10;
  int firstStart = timeConstHour;
  int maxCommonControl = 3;
  bool allowNeighbourSameCourse = true;
  bool coursesTogether = false;
  // Statistics output from optimize start order
  int numDistinctInit = -1;
  int numRunnerSameInitMax = -1;
  int numRunnerSameCourseMax = -1;
  int minimalStartDepth = -1;


  bool changedVacancyInfo = true;
  bool changedExtraInfo = true;

  map<int, ClassInfo> classes;
  wstring startName;
};

class DrawOptimAlgo {
private:
  oEvent* oe;

  struct DrawId {
  private:
    uint32_t unique = 0;
    static constexpr uint32_t FlagSlow = 0x80000000;
    static constexpr uint32_t FlagFast = 0x40000000;
    static constexpr uint32_t HashMask = ~(FlagSlow | FlagFast);
    static constexpr uint32_t FlagMask = (FlagSlow | FlagFast);

    uint32_t flag(ClassInfo::Speed speed) const {
      if (speed == ClassInfo::Speed::Fast)
        return FlagFast;
      else if (speed == ClassInfo::Speed::Slow)
        return FlagSlow;
      return 0;
    }

  public:
    int courseId = 0;
    int classId = 0;
    bool isFree() const { return unique == 0; }

    bool slowAndFast(ClassInfo::Speed speed) const {
      if (speed == ClassInfo::Speed::Slow)
        return (unique & FlagFast) != 0;
      else if (speed == ClassInfo::Speed::Fast)
        return (unique & FlagSlow) != 0;
      return false;
    }

    bool occupied(int uni, ClassInfo::Speed speed, int crs) const {
      return crs == courseId || ((unique & HashMask) == (uni & HashMask) && !slowAndFast(speed));
    }

    void setUnique(int uni, ClassInfo::Speed speed) {
      unique = (uni & HashMask) | flag(speed);
    }

    bool sameUnique(int uni) {
      return (unique & HashMask) == (uni & HashMask);
    }

    int getUnique() const {
      return (unique & HashMask);
    }
  };

  vector<vector<DrawId>> startField;

  vector<pClass> classes;
  vector<pRunner> runners;

  map<int, int> runnerPerGroup;
  map<int, int> runnerPerCourse;

  int maxNRunner = 0;
  int maxGroup = 0;
  int maxCourse = 0;
  int bestEndPos = 0;

  // Classes not drawn now. Is filled in by computeBestStartDepth(...) viz initData(...)
  map<int, ClassInfo> otherClasses;

  int bestEndPosGlobal = 0;

  static int optimalLayout(int interval, vector<pair<int, int>>& classes);
  bool isFree(const DrawInfo& di, int nFields, int firstPos, int posInterval, ClassInfo& cInfo) const;
  void insertStart(const ClassInfo& cInfo);
  int initData(DrawInfo& di, vector<ClassInfo>& cInfo, int useNControls);

  void computeBestStartDepth(DrawInfo& di, vector<ClassInfo>& cInfo, int nFields, int useNControls, int alteration);
  int countStarts(int pos) const;

public:

  DrawOptimAlgo(oEvent* oe);

  void optimizeStartOrder(DrawInfo& di,
    vector<ClassInfo>& cInfo,
    int nFields,
    int useNControls,
    int seed);

  void insertExisting(const DrawInfo& di, int startGroup);

  /** Return the relative position of the last start assigned */
  int getLastStart() const;

  vector<int> getNumStartPerInterval();

  enum class InfoType {
    Warning,
    Info
  };

  /** Returns list of (classId, problem description to be localized) */
  vector<tuple<int, wstring, InfoType>> checkDrawSettings(const DrawInfo& di, vector<ClassInfo>& cInfo);
};
