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

class ProgressWindow {
  HWND hWnd;
  int lastProgress;
  DWORD lastTime;
  volatile int progress;
  DWORD time;
  double speed;

  HANDLE thread;
  mutable CRITICAL_SECTION syncObj;
  volatile bool terminate;
  volatile bool running;

  bool initialized;
  int lastPrg;
  int subStart;
  int subEnd;
public:
  // Start showing progress
  void init();

  ProgressWindow(HWND hWndParent);
  virtual ~ProgressWindow();

  void process();

  int getProgress() const;
  void setProgress(int prg);
  void setSubProgress(int prg);
  void initSubProgress(int start, int end);
  void draw(int count);
};
