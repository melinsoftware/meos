/************************************************************************
MeOS - Orienteering Software
Copyright (C) 2009-2021 Melin Software HB

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
#include <WindowsX.h>
#include <algorithm>

#include "autocomplete.h"
#include "autocompletehandler.h"
extern HINSTANCE hInst;

AutoCompleteInfo::AutoCompleteInfo(HWND hWnd, const string &widgetId, gdioutput &gdi) : hWnd(hWnd), 
                                   widgetId(widgetId), lock(false), gdi(gdi), 
                                   currentIx(0), handler(0), lastVisible(0), modifedAutoComplete(false) {
  SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
}

AutoCompleteInfo::~AutoCompleteInfo() {
  if (hWnd) {
    SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
    destoy();
  }
}

LRESULT CALLBACK completeProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  PAINTSTRUCT ps;
  HDC hDC;

  void *ptr = (void *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  AutoCompleteInfo *aci = (AutoCompleteInfo *)ptr;

  switch (message)
  {
  case WM_CREATE:
    break;

  case WM_PAINT:
    hDC = BeginPaint(hWnd, &ps);
    if (aci)
      aci->paint(hDC);
    //TextOut(hDC, 5, 5, L"BAE", 3);
    EndPaint(hWnd, &ps);
    break;

  case WM_LBUTTONUP: {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    if (aci)
      aci->click(x,y);
  }

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

void AutoCompleteInfo::registerAutoClass() {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = (WNDPROC)completeProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInst;
  wcex.hIcon = 0;// LoadIcon(hInstance, (LPCTSTR)IDI_MEOS);
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = 0;
  wcex.lpszClassName = L"AUTOCOMPLETE";
  wcex.hIconSm = 0;// LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

  RegisterClassEx(&wcex);
}

void AutoCompleteInfo::show() {
  lock = true;
  SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  ShowWindow(hWnd, SW_SHOW);
  InvalidateRect(hWnd, nullptr, true);
  UpdateWindow(hWnd);
  lock = false;
}

void AutoCompleteInfo::paint(HDC hDC) {
  RECT rc;
  GetClientRect(hWnd, &rc);

  SetDCBrushColor(hDC, RGB(247, 245, 250));
  SelectObject(hDC, GetStockObject(DC_BRUSH));
  Rectangle(hDC, -1, -1, rc.right+1, rc.bottom+1);

  int h = int(gdi.getLineHeight()*1.2);
  int rows = min<int>((rc.bottom -10) / h, data.size());
  TextInfo ti;
  ti.format = 0;
  ti.format = absolutePosition;
  lastVisible = rows;

  rendered.resize(rows);
  for (int i = 0; i < rows; i++) {
    ti.xp = 10;
    ti.yp = 10 + i * h;
    
    rendered[i].first = i;
    auto &rend = rendered[i].second;
    rend.top = ti.yp-5;
    rend.bottom = ti.yp + h;
    rend.left = 5;
    rend.right = rc.right - 5;

    if (i == currentIx) {
      ti.color = colorBlack;
      SetDCBrushColor(hDC, RGB(237, 235, 242));
      SelectObject(hDC, GetStockObject(DC_PEN));
      SetDCPenColor(hDC, RGB(165, 160, 180));
      Rectangle(hDC, rend.left, rend.top+2, rend.right, rend.bottom-2);
    }
    else
      ti.color = colorBlack;

    gdi.RenderString(ti, data[i].display, hDC); 
  }
}

void AutoCompleteInfo::click(int x, int y) {
  POINT pt = { x,y };
  for (auto &r : rendered) {
    if (PtInRect(&r.second, pt)) {
      currentIx = r.first;
      if (handler) {
        handler->handleAutoComplete(gdi, *this);
        return;
      }
    }
  }
}

void AutoCompleteInfo::upDown(int direction) {
  currentIx -= direction;
  if (currentIx < 0)
    currentIx = 0;
  else if (currentIx >= lastVisible)
    currentIx = lastVisible - 1;

  modifedAutoComplete = true;
  HDC hDC = GetDC(hWnd);
  paint(hDC);
  ReleaseDC(hWnd, hDC);
}

void  AutoCompleteInfo::enter() {
  if (handler && currentIx >= 0 && size_t(currentIx) < data.size()) {
    handler->handleAutoComplete(gdi, *this);
    return;
  }
}

void AutoCompleteInfo::setData(vector<AutoCompleteRecord> &items) {
  int newDataIx = -1;
  if (modifedAutoComplete && size_t(currentIx) < data.size()) {
    for (size_t k = 0; k < items.size(); k++) {
      if (data[currentIx].id == items[k].id)
        newDataIx = k;
    }
  }
  if (newDataIx == -1 && !items.empty()) {
    newDataIx = 0;
    modifedAutoComplete = false;
  }
  data = items;
  currentIx = newDataIx;
}
