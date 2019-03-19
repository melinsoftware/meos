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

#include <vector>
#include "gdioutput.h"

class AutoCompleteHandler;

struct AutoCompleteRecord {
  AutoCompleteRecord() : id(-1) {}
  AutoCompleteRecord(const wstring &display, const wstring &name, int id) : display(display), name(name), id(id) {}
  wstring display;
  wstring name;
  int id;
};

class AutoCompleteInfo {
public: 
  
private:

  AutoCompleteHandler *handler;
  HWND hWnd;
  
  string widgetId;
  bool lock;
  gdioutput &gdi;

  vector<AutoCompleteRecord> data;
  vector<pair<int, RECT>> rendered;
  
  bool modifedAutoComplete; // True if the user has made a selection
  int lastVisible;
  int currentIx;
public:
  AutoCompleteInfo(HWND hWnd, const string &widgetId, gdioutput &gdi);
  ~AutoCompleteInfo();
  bool matchKey(const string &k) const { return widgetId == k; }

  void destoy() {
    if (hWnd)
      DestroyWindow(hWnd);
    hWnd = nullptr;
  }

  void paint(HDC hDC);
  void setData(vector<AutoCompleteRecord> &items);
  void click(int x, int y);
  void show();

  void upDown(int direction);
  void enter();
  
  bool locked() const { return lock; }
  const string &getTarget() const { return widgetId; }
  wstring getCurrent() const { if (size_t(currentIx) < data.size()) return data[currentIx].name; else return L""; }
  int getCurrentInt() const { if (size_t(currentIx) < data.size()) return data[currentIx].id; else return 0; }

  void setAutoCompleteHandler(AutoCompleteHandler *h) { handler = h; };
  static void registerAutoClass();
};

