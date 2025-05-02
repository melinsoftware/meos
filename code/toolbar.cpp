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

#include "stdafx.h"
#include "resource.h"
#include <shlobj.h>

#include "oEvent.h"
#include "xmlparser.h"

#include "gdioutput.h"
#include "commctrl.h"
#include "SportIdent.h"
#include "TabBase.h"
#include "TabCompetition.h"
#include "TabAuto.h"
#include "TabClass.h"
#include "TabCourse.h"
#include "TabControl.h"
#include "TabSI.h"
#include "TabList.h"
#include "TabTeam.h"
#include "TabSpeaker.h"
#include "TabMulti.h"
#include "TabRunner.h"
#include "TabClub.h"
#include "progress.h"
#include "inthashmap.h"
#include <cassert>
#include "localizer.h"
#include "intkeymap.hpp"
#include "intkeymapimpl.hpp"
#include "download.h"
#include "meos_util.h"
#include "toolbar.h"

const wchar_t *szToolClass = L"MeOSToolClass";

LRESULT CALLBACK ToolProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#define BASE_ID 1013


const DWORD buttonStyles = BTNS_AUTOSIZE;
const int bitmapSize = 24;

Toolbar::Toolbar(gdioutput &gdi_par) : gdi(gdi_par)
{
  hwndFloater = 0;
  hwndToolbar = 0;

  isactivating = false;
  // Create the imagelist.
  hImageListDef = ImageList_Create(bitmapSize, bitmapSize, ILC_COLOR24 | ILC_MASK, 1, 15);
  //hImageListMeOS = ImageList_Create(bitmapSize, bitmapSize, ILC_COLOR16 | ILC_MASK, 1, 10);
  hImageListMeOS = ImageList_LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(BMP_TEST),
                                        bitmapSize, 17, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION);

}


Toolbar::~Toolbar()
{
  ImageList_Destroy(hImageListDef);
  ImageList_Destroy(hImageListMeOS);
}

void Toolbar::reset()
{
  btn.clear();
  tooltips.clear();
  btn_id.clear();
  toolbar_id.clear();
  DestroyWindow(hwndToolbar);
  hwndToolbar = 0;
}

void Toolbar::show() {
  if (hwndFloater)
    ShowWindow(hwndFloater, SW_NORMAL);
}

void Toolbar::hide() {
  if (hwndFloater)
    ShowWindow(hwndFloater, SW_HIDE);
}

void Toolbar::activate(bool active) {
  if (!isactivating && isVisible()) {
    isactivating = true;
    //SendMessage(hwndFloater, WM_NCACTIVATE, active ? 1:0, 0);
    DefWindowProc(hwndFloater, WM_NCACTIVATE, active ? 1:0, 0);
    isactivating = false;
  }
}

void Toolbar::addButton(const string &id, int imgList, int icon, const string &tooltip)
{
  tooltips.push_back(lang.tl(tooltip));
  TBBUTTON tbButton = { MAKELONG(icon, imgList), int(btn_id.size()) + BASE_ID, TBSTATE_ENABLED,
                        buttonStyles, {0}, 0, (INT_PTR)tooltips.back().c_str() };
  btn.push_back(tbButton);
  btn_id.push_back(id);
}


void Toolbar::processCommand(int id, int code)
{
  size_t ix = id - BASE_ID;
  if (ix < btn_id.size()) {
    gdi.processToolbarMessage(btn_id[ix], table.get());
  }
}

void registerToolbar(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style			= CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc	= (WNDPROC)ToolProc;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= 0;
  wcex.hInstance		= hInstance;
  wcex.hIcon			= 0;
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
  wcex.lpszMenuName	= 0;
  wcex.lpszClassName	= szToolClass;
  wcex.hIconSm = 0;

  RegisterClassEx(&wcex);
}

void Toolbar::createToolbar(const string &id, const wstring &title)
{
  if (id == toolbar_id) {
    show();
    return;
  }

  wstring t = lang.tl(title);
  HWND hParent = gdi.getHWNDTarget();
  RECT rc;
  GetWindowRect(hParent, &rc);
  if (hwndFloater == 0) {
    hwndFloater = CreateWindowEx(WS_EX_TOOLWINDOW, szToolClass, t.c_str(),
                  WS_POPUP | WS_THICKFRAME | WS_CAPTION,
                  rc.right-300, rc.top+10, 600, 64, hParent, NULL, GetModuleHandle(0), NULL);

    SetWindowLongPtr(hwndFloater, GWLP_USERDATA, LONG_PTR(this));
  }
  else  {
    SetWindowText(hwndFloater, t.c_str());
  }

  if (hwndToolbar != 0)
    DestroyWindow(hwndToolbar);

  // Create the toolbar.
  hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
    WS_CHILD | TBSTYLE_TOOLTIPS,
    0, 0, 0, 0, hwndFloater, NULL, GetModuleHandle(0), NULL);

  if (hwndToolbar == NULL)
    return;

  toolbar_id = id;

  int ImageListID = 0;

  SendMessage(hwndToolbar, CCM_SETVERSION, 5, 0);

  // Set the image list.
  SendMessage(hwndToolbar, TB_SETIMAGELIST, (WPARAM)ImageListID,
    (LPARAM)hImageListDef);

  // Load the button images.
  SendMessage(hwndToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_LARGE_COLOR,
    (LPARAM)HINST_COMMCTRL);

  ImageListID = 1;
  // Set the image list.
  SendMessage(hwndToolbar, TB_SETIMAGELIST, (WPARAM)ImageListID,
    (LPARAM)hImageListMeOS);

  // Add buttons.
  SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE,
    (WPARAM)sizeof(TBBUTTON), 0);
  TBBUTTON *bt_ptr = &btn.at(0);
  SendMessage(hwndToolbar, TB_ADDBUTTONS, (WPARAM)btn.size(),
    (LPARAM)bt_ptr);

  SendMessage(hwndToolbar, TB_SETMAXTEXTROWS, 0, 0);
  // Tell the toolbar to resize itself, and show it.
  SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);

  LRESULT bsize = SendMessage(hwndToolbar, TB_GETBUTTONSIZE, 0,0);
  int bw = LOWORD(bsize);
  int bh = HIWORD(bsize);

  int tw = bw * btn.size();

  // Resize floater
  GetClientRect(hwndFloater, &rc);

  int dx = rc.right - rc.left  - tw;
  int dy = rc.bottom - rc.top -  bh - 5;

  WINDOWPLACEMENT  wpl;
  wpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hwndFloater, &wpl);

  wpl.rcNormalPosition.right -= dx;
  wpl.rcNormalPosition.bottom -= dy;

  HWND desktop = GetDesktopWindow();
  GetClientRect(desktop, &rc);
  dx = 0;
  dy = 0;
  if (wpl.rcNormalPosition.right > rc.right)
    dx = wpl.rcNormalPosition.right - rc.right + 10;
  else if (wpl.rcNormalPosition.left < 0)
    dx = wpl.rcNormalPosition.left - 10;

  if (wpl.rcNormalPosition.bottom > rc.bottom)
    dy = wpl.rcNormalPosition.bottom - rc.bottom + 10;
  else if (wpl.rcNormalPosition.top < 0)
    dy = wpl.rcNormalPosition.top - 30;

  if (dx != 0 || dy != 0) {
    OffsetRect(&wpl.rcNormalPosition, -dx, -dy);
  }

  SetWindowPlacement(hwndFloater, &wpl);

  SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);

  ShowWindow(hwndFloater, SW_SHOW);
  ShowWindow(hwndToolbar, TRUE);
}

LRESULT CALLBACK ToolProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PAINTSTRUCT ps;
  HDC hdc;

  switch (message)
  {
    case WM_CREATE:
      break;

    case WM_SIZE:
      break;

    case WM_WINDOWPOSCHANGED:
      return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_ACTIVATE:
      return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_NCACTIVATE: {
      Toolbar *tb = (Toolbar *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
      if (tb) {
        //DefWindowProc(tb->gdi.getHWND(), message, wParam, lParam);
        SendMessage(tb->gdi.getHWNDMain(), message, wParam, lParam);
      }
      return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      break;

    case WM_DESTROY:
      break;

    case WM_COMMAND: {
      Toolbar *tb = (Toolbar *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      if (tb) {
        tb->processCommand(id, code);
      }
    }
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


HWND Toolbar::getFloater() const {
  return hwndFloater;
}

bool Toolbar::isVisible() const {
  if (!hwndFloater)
    return false;
  return IsWindowVisible(hwndFloater) != 0;
}
