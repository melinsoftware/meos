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

#include "StdAfx.h"
#include "animationdata.h"
#include "meos_util.h"
#include "Printer.h"

AnimationData::AnimationData(gdioutput &gdi, int timePerPage, int nCol, 
                             int marginPercent, bool animate, bool respectPageBreak) :
  nCol(nCol), animate(animate), page(-1), gdiRef(0) {
  
  lastTime = 0;
  nextTime = 0;
  timeOut = timePerPage;
  doAnimation = true;
  PageInfo pageInfo;
  errorState = false;

  gdi.getTargetDimension(width, height);

  margin = (width * marginPercent) / 100;
  double w = (gdi.getWidth() + 20) *nCol + margin;
  double s = width / w;
  if ((fabs(s - 1.0) > 1e-3)) {
    gdi.scaleSize(s, true, false);
  }
  pageInfo.topMargin = 20;
  pageInfo.scaleX = 1.0f;
  pageInfo.scaleY = 1.0f;
  pageInfo.leftMargin = 20;
  pageInfo.bottomMargin = 30;
  pageInfo.pageY = float(height-margin);
  pageInfo.printHeader = false;
  pageInfo.yMM2PrintC = pageInfo.xMM2PrintC = 1;
  pageInfo.xMM2PrintK = 0;
  pageInfo.yMM2PrintK = 0;

  list<RectangleInfo> rectangles;
  pageInfo.renderPages(gdi.getTL(), rectangles, false, respectPageBreak, pages);
}

AnimationData::~AnimationData() {
  if (animationThread && animationThread->joinable()) {
    animationThread->join();
    animationThread.reset();
  }

  if (gdiRef) {
    gdiRef->removeHandler(this);
  }
}

bool AnimationData::takeOver(shared_ptr<AnimationData> &other) {
  delayedTakeOver = other;
  return true;
}

void AnimationData::takeOverInternal(shared_ptr<AnimationData> &other) {
  pages.swap(other->pages);
  width = other->width;
  height = other->height;
  nCol = other->nCol;
  margin = other->margin;
  animate = other->animate;
}

void AnimationData::renderPage(HDC hDC, gdioutput &gdi, DWORD time) {
 
  bool addTextAnimation = false;
  if (doAnimation) {
    if (page == -1)
      page = 0;
    else
      page++;

    addTextAnimation = true;
    nextTime = time + timeOut;
    lastTime = time;
    gdiRef = &gdi;
    gdi.addTimeoutMilli(timeOut, "AnimationData", 0).setHandler(this);
    doAnimation = false;
  }

  if (animationThread && animationThread->joinable()) {
    if (!addTextAnimation)
      return; // Ignore repaint
    
    if (animationThread) {
      animationThread->join();
      animationThread.reset();
    }
  }

  if (delayedTakeOver && addTextAnimation) {
    takeOverInternal(delayedTakeOver);
    delayedTakeOver.reset();
  }

  size_t sp = nCol * page;
  if (sp >= pages.size()) {
    sp = 0;
    page = 0;
  }

  int count = 1;
  for (size_t i = sp; i < sp + nCol && i < pages.size(); i++) {
    int currentRow = 0;
    for (auto &text : pages[i].text) {
      if (text.ti.yp != currentRow) {
        currentRow = text.ti.yp;
        count++;
      }
    }
  }

  int atime = 400;
  if (count < 30)
    atime = 800;
  else if (count < 50)
    atime = 500;

  int delay = addTextAnimation && animate ? atime / count : 0;
  if (delay == 0 || errorState == true) {
    doRender(hDC, gdi, sp, delay);
    errorState = false;
  }
  else {
    animationThread = make_shared<std::thread>(&AnimationData::threadRender, this, &gdi, sp, delay);
  }
}

void AnimationData::threadRender(gdioutput *gdi, size_t sp, int delay) {
  HWND hWnd = gdi->getHWNDTarget();
  HDC hDC = GetDC(hWnd);
  int x, y;
  gdi->getTargetDimension(x, y);
  RECT rc;
  rc.left = 0;
  rc.right = x;
  rc.top = 0;
  rc.bottom = y;
  gdi->drawBackground(hDC, rc);
  try {
    doRender(hDC, *gdi, sp, delay);
  }
  catch (...) {
    errorState = true;
  }
  ReleaseDC(hWnd, hDC);
  // End thread and notify that it has ended
}

 void AnimationData::doRender(HDC hDC, gdioutput &gdi, size_t sp, int delay) {
  for (size_t i = sp; i < sp + nCol && i < pages.size(); i++) {
    renderSubPage(hDC, gdi, pages[i], margin / 2 + ((i - sp) * width) / nCol, 0, delay);

    if (i + 1 < sp + nCol) {
      int x = margin / 2 + ((i + 1 - sp) * width) / nCol - 10;

      SelectObject(hDC, GetStockObject(DC_BRUSH));
      HLS fg, bg;
      fg.RGBtoHLS(gdi.getFGColor());
      bg.RGBtoHLS(gdi.getBGColor());

      if (bg.lightness > fg.lightness)
        fg.lighten(1.3);
      else
        fg.lighten(1.0 / 1.3);

      SetDCBrushColor(hDC, fg.HLStoRGB());
      Rectangle(hDC, x - 1, 20, x + 1, height - 20);
    }
  }
}

void AnimationData::renderSubPage(HDC hDC, gdioutput &gdi, RenderedPage &page, int x, int y, int animateDelay) {
  int ox = gdi.getOffsetX();
  int oy = gdi.getOffsetY();
  
  int top = 10000;
  for (const auto &text : page.text) {
    if (!text.ti.isFormatInfo()) {
      top = min<int>(top, text.ti.yp);
    }
  }
  gdi.setOffsetY(-y+top-margin/2);
  gdi.setOffsetX(-x);

  int currentRow = 0;
  for (auto &text : page.text) {
    if (animateDelay>0 && text.ti.yp != currentRow) {
      Sleep(animateDelay);
      currentRow = text.ti.yp;
    }
    gdi.RenderString(text.ti, hDC);
  }
  gdi.setOffsetY(ox);
  gdi.setOffsetX(oy);
}

void AnimationData::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (pages.size() > size_t(nCol) || delayedTakeOver) {
    doAnimation = true;
    gdi.refreshFast();
  }
  gdiRef = &gdi;
  gdi.addTimeoutMilli(timeOut, "AnimationData", 0).setHandler(this);
}
