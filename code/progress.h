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

#include <atomic>
#include <thread>
#include <mutex>

class ProgressWindow {
  HWND hWnd = nullptr;
  uint64_t lastTime = 0;
  uint64_t time = 0;
  double speed = 0;

  shared_ptr<std::thread> threadObj;
  std::atomic_bool terminate;
  std::atomic_bool running;

  int lastProgress = 0;
  int progress = 0;

  
  int lastPrg = 0;
  int subStart = 0;
  int subEnd = 1000;

  void draw(int count, int progress) const;
  void process();
  mutable std::mutex mtx;
  std::condition_variable exitCond;

  const int p_width;
  const int p_height;

public:
  // Start showing progress
  void init();

  ProgressWindow(HWND hWndParent, double scale);
  virtual ~ProgressWindow();

  int getProgress() const;
  int computeProgress(int dt) const;
  void setProgress(int prg);
  void setSubProgress(int prg);
  void initSubProgress(int start, int end);
};
