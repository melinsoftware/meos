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

class gdioutput;
class oEvent;
class QualificationFinal;

#include <vector>
#include <memory>
#include "qualification_final.h"

#include "autocompletehandler.h"

class oClass;

class QFEditor : public GuiHandler {
private:
  enum SaveType {NotSaved, SavedInside, SavedFile};
  shared_ptr<QualificationFinal> currentQF;
  
  wstring savedFileName;
  bool dirtyExt = false;
  bool dirtyInt = false;
  SaveType lastSaved = NotSaved;
  oClass* cls;
  
  /// Check and ask if there are changes to save
  bool checkSave(gdioutput &gdi);

  void edit(gdioutput& gdi);
  void updateActions(gdioutput& gdi);

  vector<pair<size_t, vector<QFClass>>> data; // First is number of classes (for convenient undo)
  size_t numLevels = 0; // Allow larger data than levels (for convenient "undo")

  void showLevels(gdioutput& gdi) const;
  void showLevel(gdioutput& gdi, int level, size_t numRaces, const vector<QFClass>& classes) const;

  static int code(int level, int cls) {
    return level * 256 + cls;
  }

  static pair<int, int> decode(int c) {
    return make_pair(c / 256, c % 256);
  }

  QFClass& getQFClass(int id);
  const QFClass& getQFClass(int id) const;

  void loadQualificationView(gdioutput &gdi, const QFClass &cls, int code);

  int getLinearIndex(int level, int ix) const;
  QFClass &getQFClassFromIndex(int linearIndex);
  pair<int, int> getLevelIndexFromLinear(int linearIndex) const;
  vector<pair<int, int>> getLevelIndex() const;

  void updateLevelMap(const vector<pair<int, int>>& oldIndex);

  vector<pair<wstring, size_t>> getRules(const QFClass& cls) const;
  void emptyScheme();

  static string getKeyTag(int level, int ix);
  string infoTextTag;
  gdioutput *infoTextGdi = nullptr;
  void updateQInfo(const QFClass& cls);
  
  void makeQF();

  int extraSelPosX = 0;
  int extraSelPosY = 0;
  void updateExtraField(gdioutput &gdi, const QFClass &cls, int code);

  RECT rcRuleForm;
  void endAddRuleForm(gdioutput& gdi);
  void addRuleOther(int cCcode, gdioutput& gdi);
  void addRuleClassPlace(int cCode, gdioutput& gdi);
  void addRuleTime(int cCode, gdioutput& gdi);

  void use();
  bool save(gdioutput &gdi);
public:
  QFEditor();
  virtual ~QFEditor();

  enum class DirtyFlag { MakeDirty, ClearDirty, NoTouch };
  void makeDirty(DirtyFlag inside, DirtyFlag outside);

  void load(const shared_ptr<QualificationFinal>& qf);
  void show(gdioutput &gdi);

  void setClass(oClass* cls) { this->cls = cls; }

  // Inherited via GuiHandler
  virtual void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type);
};
