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

#include "stdafx.h"
#include "gdioutput.h"


#pragma warning( disable : 4512 )
class Toolbar {
  HWND hwndFloater;
  HWND hwndToolbar;

  HIMAGELIST hImageListDef;
  HIMAGELIST hImageListMeOS;

  gdioutput &gdi;

  vector<TBBUTTON> btn;
  vector<string> btn_id;
  list<wstring> tooltips;
  void *data;

  string toolbar_id;

  void processCommand(int id, int code);
  bool isactivating;
public:

  void show();
  void hide();
  void activate(bool active);

  HWND getFloater() const;
  bool isVisible() const;

  void setData(void *d) {data = d;}

  void reset();
  void addButton(const string &id, int imgList, int icon, const string &tooltip);

  void createToolbar(const string &id, const wstring &title);
  bool isLoaded(const string &id) const {return toolbar_id == id;}
  Toolbar(gdioutput &gdi_par);
  virtual ~Toolbar();

  friend LRESULT CALLBACK ToolProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

