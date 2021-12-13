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
#include <shlobj.h>

#include "oEvent.h"
#include "xmlparser.h"
#include <process.h>

#include "gdioutput.h"
#include "commctrl.h"

#include "localizer.h"
#include "progress.h"


void start_progress_thread(void *ptr)
{
  ProgressWindow *pw=(ProgressWindow *)ptr;
  pw->process();
}

const int p_width = 230;
const int p_height = 12;

ProgressWindow::ProgressWindow(HWND hWndParent)
{
  lastPrg = 0;
  initialized = false;
  subStart = 0;
  subEnd = 1000;
  hWnd = hWndParent;
  terminate = false;
  running = false;
}

void ProgressWindow::init()
{
  initialized = true;
  progress = 0;
  lastProgress = 0;
  time = 0;
  lastTime = 0;
  speed = 0;
  RECT rc;
  GetClientRect(hWnd, &rc);
  hWnd=CreateWindowEx(WS_EX_TOPMOST, L"STATIC", L"",  WS_VISIBLE|WS_CHILD,
    (rc.right-p_width)/2, rc.bottom/2, p_width, p_height+1, hWnd, NULL,
      (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

  ShowWindow(hWnd, SW_SHOW);
  SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE|SWP_NOMOVE);
  SetActiveWindow(hWnd);
  UpdateWindow(hWnd);
  InitializeCriticalSection(&syncObj);
  thread = (HANDLE)_beginthread(start_progress_thread, 0,  this);
}

ProgressWindow::~ProgressWindow()
{
  if (initialized) {
    setProgress(1000);
    EnterCriticalSection(&syncObj);
    terminate = true;
    LeaveCriticalSection(&syncObj);

    int maxCount = 100;
    while (maxCount-- > 0 && running) {
      Sleep(20);
    }
    EnterCriticalSection(&syncObj);
    if (running)
      TerminateThread(thread, 0);
    LeaveCriticalSection(&syncObj);

//	  CloseHandle(thread);
    DeleteCriticalSection(&syncObj);

    DestroyWindow(hWnd);
  }
}

void ProgressWindow::process()
{
  running = true;
  while (!terminate) {
    EnterCriticalSection(&syncObj);
    if (!terminate)
      draw(GetTickCount()/40);
    LeaveCriticalSection(&syncObj);
    if (!terminate)
      Sleep(33);
  }
  running = false;
}

void ProgressWindow::draw(int count)
{
  HDC hDC = GetDC(hWnd);
  int prgBase = getProgress();
  int prg = min((prgBase * p_width)/1000, p_width-1);
  int center = int(prg*((cos(count*0.1)+1)*0.8) / 2);

  DWORD c=GetSysColor(COLOR_ACTIVECAPTION);
  double red=GetRValue(c);
  double green=GetGValue(c);
  double blue=GetBValue(c);

  double blue1=min(255., blue*1.4);
  double green1=min(255., green*1.4);
  double red1=min(255., red*1.4);

  int blueD=int(blue/2);
  int redD=int(red/2);
  int greenD=int(green/2);

  SelectObject(hDC, GetStockObject(DC_PEN));
  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  SetDCPenColor(hDC, RGB(redD,greenD,blueD));

  Rectangle(hDC, 0, 0, p_width, p_height-1);
  SelectObject(hDC, GetStockObject(DC_BRUSH));
  SelectObject(hDC, GetStockObject(NULL_PEN));

  SetDCBrushColor(hDC, GetSysColor(COLOR_3DHIGHLIGHT));

  Rectangle(hDC, prg, 1, p_width-1, p_height-2);

  TRIVERTEX vert[4];
  vert [0] .x      = 1;
  vert [0] .y      = 1;
  vert [0] .Red    = 0xff00&DWORD(red*256);
  vert [0] .Green  = 0xff00&DWORD(green*256);
  vert [0] .Blue   = 0xff00&DWORD(blue*256);
  vert [0] .Alpha  = 0x0000;

  vert [1] .x      = center;
  vert [1] .y      = p_height-2;
  vert [1] .Red    = 0xff00&DWORD(red1*256);
  vert [1] .Green  = 0xff00&DWORD(green1*256);
  vert [1] .Blue   = 0xff00&DWORD(blue1*256);
  vert [1] .Alpha  = 0x0000;

  vert [2] .x      = center;
  vert [2] .y      = 1;
  vert [2] .Red    = 0xff00&DWORD(red1*256);
  vert [2] .Green  = 0xff00&DWORD(green1*256);
  vert [2] .Blue   = 0xff00&DWORD(blue1*256);
  vert [2] .Alpha  = 0x0000;

  vert [3] .x      = prg;
  vert [3] .y      = p_height-2;
  vert [3] .Red    = 0xff00&DWORD(red*256);
  vert [3] .Green  = 0xff00&DWORD(green*256);
  vert [3] .Blue   = 0xff00&DWORD(blue*256);
  vert [3] .Alpha  = 0x0000;

  GRADIENT_RECT gr[2];
  gr[0].UpperLeft=0;
  gr[0].LowerRight=1;
  gr[1].UpperLeft=2;
  gr[1].LowerRight=3;

  GradientFill(hDC,vert, 4, gr, 2, GRADIENT_FILL_RECT_H);

  ReleaseDC(hWnd, hDC);
}

int ProgressWindow::getProgress() const
{
  DWORD now = GetTickCount();
  int dt = int(now-time);
  EnterCriticalSection(&syncObj);
    int dp = int(dt * speed);
    dp = min<int>(dp, int(progress - lastProgress));
  LeaveCriticalSection(&syncObj);
  return min(progress + dp, 1000);
}

void ProgressWindow::setProgress(int prg)
{
  if (prg <= lastPrg + 1)
    return;
  lastPrg = prg;
  if (!initialized)
    return;
  EnterCriticalSection(&syncObj);
  lastProgress = progress;
  lastTime = time;
  time = GetTickCount();
  int newProgress = getProgress();
  newProgress = prg > newProgress ? prg : newProgress;
  if (lastTime>0)
    speed = double(prg - lastProgress) / double(time-lastTime);
  progress = max(newProgress, int(progress));
  LeaveCriticalSection(&syncObj);
}

void ProgressWindow::setSubProgress(int prg)
{
  prg = subStart + prg *(subEnd-subStart) / 1000;
  setProgress(prg);
}

void ProgressWindow::initSubProgress(int start, int end)
{
  subStart = start;
  subEnd = end;
}
