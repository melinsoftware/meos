#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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

class DynamicResult;
class gdioutput;
class BaseInfo;
class ButtonInfo;
class oEvent;

#include <vector>

class TabBase;

class MethodEditor {
private:
  enum SaveType {NotSaved, SavedInside, SavedFile};
  oEvent *oe;
  DynamicResult *currentResult;
  wstring fileNameSource;
  void setCurrentResult(DynamicResult *lst, const wstring &src);
  int currentIndex;
  bool dirtyInt;
  bool wasLoadedBuiltIn;
  enum DirtyFlag {MakeDirty, ClearDirty, NoTouch};
  TabBase *origin = nullptr;
  /// Check (and autosave) if there are unsaved changes in a dialog box
  void checkUnsaved(gdioutput &gdi);

  void checkChangedSave(gdioutput &gdi);

  /// Check and ask if there are changes to save
  bool checkSave(gdioutput &gdi);

  int methodCb(gdioutput &gdi, int type, BaseInfo &data);

  void makeDirty(gdioutput &gdi, DirtyFlag inside);

  bool checkTag(const string &tag, bool throwError) const;
  string uniqueTag(const string &tag) const;

  static wstring getInternalPath(const string &tag);

  void saveSettings(gdioutput &gdi);

  bool resultIsInstalled() const;

  int inputNumber;
  void debug(gdioutput &gdi, int id, bool isTeam);
  void show(gdioutput &gdi);

public:
  MethodEditor(oEvent *oe);
  virtual ~MethodEditor();

  void show(TabBase *dst, gdioutput &gdi);

  bool isShown(TabBase *tab) const { return origin == tab; }

  DynamicResult *load(gdioutput &gdi, const string &tag, bool forceLoadCopy);

  friend int methodCB(gdioutput*, int, void *);
};
