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

// gdioutput.cpp: implementation of the gdioutput class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meosException.h"
#include "resource.h"

#include "process.h"

#include "meos.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <cassert>
#include <cmath>
#include <sstream>

#include "meos_util.h"
#include "Table.h"

#define _USE_MATH_DEFINES
#include "math.h"

#include "Localizer.h"

#include "TabBase.h"
#include "toolbar.h"
#include "gdiimpl.h"
#include "Printer.h"
#include "recorder.h"
#include "animationdata.h"
#include "image.h"
#include "autocomplete.h"

extern Image image;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


//Fulhack...
#ifndef IDC_HAND
  #define IDC_HAND MAKEINTRESOURCE(32649)
#endif

//#define DEBUGRENDER

#ifdef DEBUGRENDER
  static int counterRender = 0;
  static bool breakRender = false;
  static int debugDrawColor = 0;
#endif

extern int defaultCodePage;

GuiHandler &BaseInfo::getHandler() const {
  if (managedHandler)
    return *managedHandler;
  if (handler == 0)
    throw meosException("Handler not definied.");
  return *handler;
}

void GuiHandler::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  throw meosException("Handler not definied.");
}

InputInfo::InputInfo() : hWnd(0), callBack(0), ignoreCheck(false),
                isEditControl(true), bgColor(colorDefault), fgColor(colorDefault),
                writeLock(false), updateLastData(0) {}


EventInfo::EventInfo() : callBack(0), keyEvent(KC_NONE) {}

/** Return true if rendering text should be skipped for
    this format. */
bool gdioutput::skipTextRender(int format) {
  format &= 0xFF | hiddenText;
  return format == pageNewPage ||
         format == pagePageInfo ||
         format == pageNewChapter ||
         (format & hiddenText) == hiddenText;
}

gdioutput::gdioutput(const string &_tag, double _scale) :
  recorder((Recorder *)0, false) {
  tag = _tag;
  po_default = new PrinterObject();
  tabs = 0;
  hasAnyTimer = false;
  constructor(_scale);

  isTestMode = false;
}
extern gdioutput *gdi_main;

gdioutput::gdioutput(double _scale, HWND hWnd, const PrinterObject &prndef) :
  recorder((Recorder *)0, false) {
  hasAnyTimer = false;
  po_default = new PrinterObject(prndef);
  tabs = 0;
  setWindow(hWnd);
  constructor(_scale);
  if (gdi_main) {
    isTestMode = gdi_main->isTestMode;
    if (isTestMode)
      cmdAnswers.swap(gdi_main->cmdAnswers);
  }
  else isTestMode = false;
}

void gdioutput::constructor(double _scale)
{
  currentFontSet = 0;
  commandLock = false;
  commandUnlockTime = 0;
  lockUpDown = false;

  Background = 0;
  backgroundColor1 = -1;
  backgroundColor2 = -1;
  foregroundColor = -1;
  backgroundImage = -1;

  toolbar = 0;
  initCommon(_scale, L"Arial");

  OffsetY=0;
  OffsetX=0;

  manualUpdate = false;

  itTL = TL.end();

  hWndTarget = 0;
  hWndToolTip = 0;
  hWndAppMain = 0;
  onClear.clear();
  postClear.clear();
  clearPage(true);
  hasCleared = false;
  highContrast = false;
  hideBG = false;
  fullScreen = false;
  lockRefresh = 0;
  autoSpeed = 0;
  autoPos = 0;
  lastSpeed = 0;
  autoCounter = 0;
}

void gdioutput::setFont(int size, const wstring &font)
{
  double ss = size * sqrt(size);
  double s = 1 + double(ss)*0.25;
  initCommon(s, font);
}

void gdioutput::setFontCtrl(HWND hWnd) {
  SendMessage(hWnd, WM_SETFONT, (WPARAM) getGUIFont(), MAKELPARAM(TRUE, 0));
}

static void scaleWindow(HWND hWnd, double scale, int &w, int &h) {
  RECT rc;
  GetWindowRect(hWnd, &rc);
  w = rc.right - rc.left;
  h = rc.bottom - rc.top;
  w = int(w * scale + 0.5);
  h = int(h * scale + 0.5);
}

int transformX(int x, double scale) {
  if (x<40)
    return int(x * scale + 0.5);
  else
    return int((x-40) * scale + 0.5) + 40;
}

void gdioutput::scaleSize(double scale_, bool allowSmallScale, ScaleOperation op) {
  if (fabs(scale_ - 1.0) < 1e-4)
    return; // No scaling
  double ns = scale*scale_;

  if (!allowSmallScale && ns + 1e-6 < 1.0 ) {
    ns = 1.0;
    scale_ = 1.0;
  }
  initCommon(ns, currentFont);

  if (op == ScaleOperation::NoUpdate)
    return;

  for (list<TextInfo>::iterator it = TL.begin(); it!=TL.end(); ++it) {
    it->xlimit = int(it->xlimit * scale_ + 0.5);
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
  }
  int w, h;
  OffsetY = int (OffsetY * scale_ + 0.5);
  OffsetX = int (OffsetX * scale_ + 0.5);

  for (list<ButtonInfo>::iterator it = BI.begin(); it!=BI.end(); ++it) {
    if (it->fixedRightTop)
      it->xp = int(scale_ * it->xp + 0.5);
    else
      it->xp = transformX(it->xp, scale_);

    it->yp = int(it->yp * scale_ + 0.5);

    if (it->isCheckbox)
      scaleWindow(it->hWnd, 1.0, w, h);
    else
      scaleWindow(it->hWnd, scale_, w, h);
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, w, h, true);
  }

  for (list<InputInfo>::iterator it = II.begin(); it!=II.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
    it->height *= scale_;
    it->width *= scale_;
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, int(it->width+0.5), int(it->height+0.5), true);
  }

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it!=LBI.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
    it->height *= scale_;
    it->width *= scale_;
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, int(it->width+0.5), int(it->height+0.5), true);
  }

  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it!=Rectangles.end(); ++it) {
    it->rc.bottom = int(it->rc.bottom * scale_ + 0.5);
    it->rc.top = int(it->rc.top * scale_ + 0.5);
    it->rc.right = transformX(it->rc.right, scale_);
    it->rc.left = transformX(it->rc.left, scale_);
  }

  for (list<TableInfo>::iterator it = Tables.begin(); it != Tables.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
  }

  MaxX = transformX(MaxX, scale_);
  MaxY = int (MaxY * scale_ + 0.5);
  CurrentX = transformX(CurrentX, scale_);
  CurrentY = int (CurrentY * scale_ + 0.5);
  SX = transformX(SX, scale_);
  SY = int (SY * scale_ + 0.5);

  for (map<string, RestoreInfo>::iterator it = restorePoints.begin(); it != restorePoints.end(); ++it) {
    RestoreInfo &r = it->second;
    r.sMX = transformX(r.sMX, scale_);
    r.sMY = int (r.sMY * scale_ + 0.5);
    r.sCX = transformX(r.sCX, scale_);
    r.sCY = int (r.sCY * scale_ + 0.5);
    r.sOX = transformX(r.sOX, scale_);
    r.sOY = int (r.sOY * scale_ + 0.5);

  }
  if (op == ScaleOperation::Refresh) {
    refresh();
  }
  else {
    HDC hDC = GetDC(hWndTarget);
    for (auto &ti : TL) {
      calcStringSize(ti, hDC);
    }
    ReleaseDC(hWndTarget, hDC);
  }
}

void gdioutput::initCommon(double _scale, const wstring &font)
{
  guiMeasure.reset();
  dbErrorState = false;
  currentFontSet = 0;
  scale = _scale;
  currentFont = font;
  deleteFonts();
  enableTables();
  lineHeight = int(scale*14);

  Background=CreateSolidBrush(GetSysColor(COLOR_WINDOW));

  fontHeightCache.clear();
  fonts[currentFont].init(scale, currentFont, L"");
  updateTabFont();
}

void gdioutput::updateTabFont() {
  if (this == gdi_main && hWndTab) {
    HFONT gui = fonts[currentFont].getGUIFont();
    SendMessage(hWndTab, WM_SETFONT, WPARAM(gui), TRUE);

    RECT rc;
    GetClientRect(hWndAppMain, &rc);
    SendMessage(hWndAppMain, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
  }
}

double getLocalScale(const wstring &fontName, wstring &faceName) {
  double locScale = 1.0;
  vector<wstring> res;
  split(fontName, L";", res);

  if (res.empty() || res.size() > 2)
    throw meosException(L"Cannot load font: " + fontName);
  if (res.size() == 2) {
    locScale = _wtof(res[1].c_str());
    if (!(locScale>0.001 && locScale < 100))
      throw meosException(L"Cannot scale font with factor: " + res[1]);
  }
  faceName = res[0];
  return locScale;
}

const GDIImplFontSet & gdioutput::loadFont(const wstring &font) {
  currentFontSet = 0;
  vector< pair<wstring, size_t> > fontIx;
  getEnumeratedFonts(fontIx);
  double relScale = 1.0;
  for (size_t k = 0; k < fontIx.size(); k++) {
    if (stringMatch(fontIx[k].first, font)) {
      relScale = enumeratedFonts[fontIx[k].second].getRelScale();
    }
  }

  wstring faceName;
  double locScale = getLocalScale(font, faceName);

  if (faceName.empty())
    faceName = currentFont;
  fonts[font].init(scale * relScale * locScale, faceName, font);
  return fonts[font];
}

void gdioutput::deleteFonts() {
  if (Background)
    DeleteObject(Background);
  Background = 0;

  currentFontSet = 0;
  fonts.clear();
}

#ifndef MEOSDB

gdioutput::~gdioutput()
{
  while(!timers.empty()) {
    KillTimer(hWndTarget, (UINT_PTR)&timers.back());
    timers.back().setWnd = 0;
    timers.back().parent = 0;
    timers.pop_back();
  }
  animationData.reset();

  deleteFonts();

  if (toolbar)
    delete toolbar;
  toolbar = 0;
  
  Tables.clear();

  if (tabs) {
    delete tabs;
    tabs = 0;
  }

  initRecorder(0);
  
  delete po_default;
  po_default = 0;
}
#endif


FixedTabs &gdioutput::getTabs() {
#ifndef MEOSDB
  if (!tabs)
    tabs = new FixedTabs();
#endif

  return *tabs;
}



void gdioutput::fetchPrinterSettings(PrinterObject &po) const {
  po = *po_default;
}


void gdioutput::drawBackground(HDC hDC, RECT &rc)
{
  if (backgroundColor1 != -1) {
    SelectObject(hDC, GetStockObject(NULL_PEN));
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, backgroundColor1);
    Rectangle(hDC, -1, -1, rc.right + 1, rc.bottom + 1);
    return;
  }
  else if (!backgroundImage.empty()) {
    // TODO

  }

  GRADIENT_RECT gr[1];

  SelectObject(hDC, GetStockObject(NULL_PEN));
  SelectObject(hDC, Background);

  if (highContrast) {
    Rectangle(hDC, -1, -1, rc.right + 1, rc.bottom + 1);

    HFONT hInfo = CreateFont(min(30, int(scale*22)), 0, 900, 900, FW_LIGHT, false,  false, false, DEFAULT_CHARSET,
                             OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH|FF_ROMAN, L"Arial");

    SelectObject(hDC, hInfo);
    RECT mrc;
    mrc.left = 0;
    mrc.right = 0;
    mrc.top = 0;
    mrc.bottom = 0;
    DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
    int height = mrc.right + mrc.right / 3;
    if (height > 0) {
      SetBkMode(hDC, TRANSPARENT);

      for (int k = height; k < MaxY; k += height) {
        mrc.left = 5 - OffsetX;
        mrc.right = 1000;
        mrc.top = k - OffsetY;
        mrc.bottom = MaxY;
        SetTextColor(hDC, RGB(192, 192, 192));

        DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
        mrc.top -= 1;
        mrc.left -= 1;
        SetTextColor(hDC, RGB(92, 32, 32));

        DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);

      }
    }
    SelectObject(hDC, GetStockObject(ANSI_FIXED_FONT));
    DeleteObject(hInfo);
    return;
  }
  if (!hideBG) {
    Rectangle(hDC, -1, -1, rc.right-OffsetX+1, 10-OffsetY+1);
    Rectangle(hDC, -1, -1, 11-OffsetX, rc.bottom+1);
    Rectangle(hDC, MaxX+10-OffsetX, 0, rc.right+1, rc.bottom+1);
    Rectangle(hDC, 10-OffsetX, MaxY+13-OffsetY, MaxX+11-OffsetX, rc.bottom+1);
  }
  if (dbErrorState) {
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, RGB(255, 100, 100));
    Rectangle(hDC, -1, -1, rc.right+1, rc.bottom+1);

    HFONT hInfo = CreateFont(30, 0, 900, 900, FW_BOLD, false,  false, false, DEFAULT_CHARSET,
                             OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH|FF_ROMAN, L"Arial");

    wstring err = lang.tl(L"DATABASE ERROR");
    SelectObject(hDC, hInfo);
    RECT mrc;
    mrc.left = 0;
    mrc.right = 0;
    mrc.top = 0;
    mrc.bottom = 0;
    DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
    int width = mrc.bottom + mrc.bottom / 4;
    int height = mrc.right + mrc.right / 4;
    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(64, 0, 0));

    for (int k = height; k < max<int>(MaxY, rc.bottom + height); k += height) {
      mrc.left = rc.right - 50 - OffsetX;
      mrc.right = mrc.left + 1000;
      mrc.top = k - OffsetY;
      mrc.bottom = MaxY;
      DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT|DT_NOCLIP|DT_NOPREFIX);
      mrc.left -= width;
      mrc.top -= height / 2;
      DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT|DT_NOCLIP|DT_NOPREFIX);
    }
    SelectObject(hDC, GetStockObject(ANSI_FIXED_FONT));
    DeleteObject(hInfo);
  }
/*
  DWORD c=GetSysColor(COLOR_3DFACE);
  double red = double(GetRValue(c)) *0.9;
  double green = double(GetGValue(c)) * 0.85;
  double blue = min(255.0, double(GetBValue(c)) * 1.05);

  if (blue<100) {
    //Invert
    red = 255-red;
    green = 255-green;
    blue = 255-blue;
  }

  double blue1=min(255., blue*1.3);
  double green1=min(255., green*1.3);
  double red1=min(255., red*1.3);
  */

  double red = 242.0;
  double green = 247.0;
  double blue = 254.0;

  double blue1 = 250.0;
  double green1 = 232.0;
  double red1 = 223.0;

  TRIVERTEX vert[2];
  if (hideBG) {
    vert [0] .x      = 0;
    vert [0] .y      = 0;
  }
  else {
    vert [0] .x      = 10-OffsetX;
    vert [0] .y      = 10-OffsetY;
  }
  vert [0] .Red    = 0xff00&DWORD(red1*256);
  vert [0] .Green  = 0xff00&DWORD(green1*256);
  vert [0] .Blue   = 0xff00&DWORD(blue1*256);
  vert [0] .Alpha  = 0x0000;

  if (hideBG) {
    vert [1] .x      = rc.right + 1;
    vert [1] .y      = rc.bottom + 1;
  }
  else {
    vert [1] .x      = MaxX+10-OffsetX;
    vert [1] .y      = MaxY+13-OffsetY;
  }
  vert [1] .Red    = 0xff00&DWORD(red*256);
  vert [1] .Green  = 0xff00&DWORD(green*256);
  vert [1] .Blue   = 0xff00&DWORD(blue*256);
  vert [1] .Alpha  = 0x0000;

  gr[0].UpperLeft=0;
  gr[0].LowerRight=1;


  if (MaxY>max(800, MaxX) || hideBG)
    GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_H);
  else
    GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_V);

  if (!hideBG) {
    SelectObject(hDC, GetSysColorBrush(COLOR_3DSHADOW));

    Rectangle(hDC, vert[0].x+3, vert[1].y, vert[1].x+1, vert[1].y+3);
    Rectangle(hDC, vert[1].x, vert[0].y+3, vert[1].x+3, vert[1].y+3);

    SelectObject(hDC, GetStockObject(NULL_BRUSH));
    SelectObject(hDC, GetStockObject(DC_PEN));
    SetDCPenColor(hDC, RGB(DWORD(red*0.4), DWORD(green*0.4), DWORD(blue*0.4)));
    Rectangle(hDC, vert[0].x, vert[0].y, vert[1].x, vert[1].y);
  }
}

void gdioutput::setDBErrorState(bool state) {
  if (dbErrorState != state) {
    dbErrorState = state;
    refresh();
  }
}

void gdioutput::draw(HDC hDC, RECT& rc, RECT& drawArea) {
#ifdef DEBUGRENDER
  if (debugDrawColor) {
    string ds = "DebugDraw" + itos(drawArea.left) + "-" + itos(drawArea.right) + ", " + itos(drawArea.top) + "-" + itos(drawArea.bottom) + "\n";
    OutputDebugString(ds.c_str());
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, debugDrawColor);
    Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
    return;
  }
#endif
  if (highContrast)
    drawBackground(hDC, drawArea);
  else
    drawBackground(hDC, rc);

  if (drawArea.left > MaxX - OffsetX + 15) {
    drawBoxes(hDC, rc);
    return;
  }

  if (animationData) {
    int page = 0;
    animationData->renderPage(hDC, *this, GetTickCount64());
    return;
  }

  SelectObject(hDC, GetStockObject(DC_BRUSH));

  for (auto& rit : Rectangles)
    renderRectangle(hDC, 0, rit);

  if (useTables)
    for (list<TableInfo>::iterator tit = Tables.begin(); tit != Tables.end(); ++tit) {
      tit->table->draw(*this, hDC, tit->xp, tit->yp, rc);
    }

  resetLast();
  TIList::iterator it;

  int BoundYup = OffsetY - maxTextBlockHeight - 2 + drawArea.top;
  int BoundYupTight = OffsetY - 2 + drawArea.top;
  int BoundYdown = OffsetY + drawArea.bottom + 2;

  for (auto imgTL : imageReferences) {
    RenderString(*imgTL, hDC);
  }

  if (!renderOptimize || itTL == TL.end()) {
#ifdef DEBUGRENDER
    //if (breakRender)
    //  DebugBreak();
    OutputDebugString(("Raw render" + itos(size_t(this)) + "\n").c_str());
#endif
    for (it = TL.begin(); it != TL.end(); ++it) {
      TextInfo& ti = *it;
      if ((ti.format & 0xFF) == textImage)
        continue;
      if ((ti.yp > BoundYup || ti.textRect.bottom > BoundYupTight) && ti.yp < BoundYdown)
        RenderString(*it, hDC);
    }
  }
  else {
#ifdef DEBUGRENDER
    OutputDebugString((itos(++counterRender) + " opt render " + itos(size_t(this)) + "\n").c_str());
#endif

    while (itTL != TL.end() && itTL->yp < BoundYup)
      ++itTL;

    if (itTL != TL.end())
      while (itTL != TL.begin() && itTL->yp > BoundYup)
        --itTL;

    it = itTL;
    while (it != TL.end() && it->yp < BoundYdown) {
      if ((it->format & 0xFF) != textImage)
        RenderString(*it, hDC);
      ++it;
    }
  }

  updateStringPosCache();
  drawBoxes(hDC, rc);
}

void gdioutput::renderRectangle(HDC hDC, RECT *clipRegion, const RectangleInfo &ri) {
  if (ri.drawBorder) {
    SelectObject(hDC, GetStockObject(DC_PEN));
    SetDCPenColor(hDC, RGB(40,40,60));
  }
  else
    SelectObject(hDC, GetStockObject(NULL_PEN));
  
  if (ri.color == colorTransparent) 
    SelectObject(hDC, GetStockObject(NULL_BRUSH));
  else {
    SetDCBrushColor(hDC, ri.color);
  }
  RECT rect_rc=ri.rc;
  OffsetRect(&rect_rc, -OffsetX, -OffsetY);
  Rectangle(hDC, rect_rc.left, rect_rc.top, rect_rc.right, rect_rc.bottom);
  if (ri.color == colorTransparent)
    SelectObject(hDC, GetStockObject(DC_BRUSH));
}

void gdioutput::updateStringPosCache() {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY-100;
  int BoundYdown = OffsetY+rc.bottom+10;
  shownStrings.clear();
  TIList::iterator it;

  if (!renderOptimize || itTL == TL.end()) {
    for (it=TL.begin();it!=TL.end(); ++it) {
      TextInfo &ti=*it;
      if ( ti.yp > BoundYup && ti.yp < BoundYdown) {
        if (ti.textRect.top != ti.yp - OffsetY) {
          int diff = it->textRect.top - (ti.yp - OffsetY);
          ti.textRect.top -= diff;
          ti.textRect.bottom -= diff;
        }
        shownStrings.push_back(&ti);
      }
    }
  }
  else {
    TIList::iterator itC = itTL;

    while( itC != TL.end() && itC->yp < BoundYup)
      ++itC;

    if (itC!=TL.end())
      while( itC != TL.begin() && itC->yp > BoundYup)
        --itC;

    it=itC;
    while( it != TL.end() && it->yp < BoundYdown) {
      shownStrings.push_back(&*it);
      if (it->textRect.top != it->yp - OffsetY) {
        int diff = it->textRect.top - (it->yp - OffsetY);
        it->textRect.top -= diff;
        it->textRect.bottom -= diff;
      }
      ++it;
    }
  }
}

TextInfo& gdioutput::addTimer(int yp, int xp, int format, int zeroTime, const wstring &textFormat, 
                              int xlimit, GUICALLBACK cb, int timeOut, const wchar_t* fontFace) {
  hasAnyTimer = true;
  int64_t signedTime = 1000 * zeroTime;
  uint64_t zt = GetTickCount64() - signedTime;
  wstring text = getTimerText(zeroTime, format, true, textFormat);

  addStringUT(yp, xp, format, text, xlimit, cb, fontFace);
  TextInfo& ti = TL.back();
  ti.hasTimer = true;
  ti.zeroTime = zt;
  ti.timerFormat = textFormat;
  if (timeOut != NOTIMEOUT)
    ti.timeOut = ti.zeroTime + timeOut * 1000;

  return ti;
}

TextInfo& gdioutput::addTimeout(int TimeOut, GUICALLBACK cb) {
  addStringUT(0, 0, 0, "", 0, cb);
  TextInfo& ti = TL.back();
  ti.hasTimer = true;
  ti.zeroTime = GetTickCount64();
  if (TimeOut != NOTIMEOUT)
    ti.timeOut = ti.zeroTime + TimeOut * 1000;
  return ti;
}

void CALLBACK gdiTimerProc(HWND hWnd, UINT a, UINT_PTR ptr, DWORD b) {
  wstring msg;
  KillTimer(hWnd, ptr);
  TimerInfo *it = (TimerInfo *)ptr;
  it->setWnd = 0;
  try {
    if (it->parent) {
      it->parent->timerProc(*it, b);
    }
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    string2Wide(ex.what(), msg);
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Unexpected error";
  }

  if (!msg.empty()) {
    MessageBox(hWnd, msg.c_str(), L"MeOS", MB_OK|MB_ICONEXCLAMATION);
  }
}

int TimerInfo::globalTimerId = 0;

void gdioutput::timerProc(TimerInfo &timer, DWORD timeout) {
  int timerId = timer.timerId;
  if (timer.handler)
    timer.handler->handle(*this, timer, GUI_TIMER);
  if (timer.managedHandler)
    timer.managedHandler->handle(*this, timer, GUI_TIMER);
  else if (timer.callBack)
    timer.callBack(this, GUI_TIMER, &timer);

  for (auto it = timers.begin(); it != timers.end(); ++it) {
    if (it->getId() == timerId) {
      timers.erase(it);
      break;
    }
  }  

  //timers.erase(remove_if(timers.begin(), timers.end(), [timerId](TimerInfo &x) {return x.getId() == timerId; }), timers.end());
}

void gdioutput::removeHandler(GuiHandler *h) {
  for (auto &it : timers) {
    if (it.handler == h)
      it.handler = 0;
  }

  for (auto &it : BI) {
    if (it.handler == h)
      it.handler = 0;
  }


  for (auto &it : II) {
    if (it.handler == h)
      it.handler = 0;
  }
  
  for (auto &it : TL) {
    if (it.handler == h)
      it.handler = 0;
  }

  for (auto &it : LBI) {
    if (it.handler == h)
      it.handler = 0;
  }
}

void gdioutput::removeTimeoutMilli(const string &id) {
  for (list<TimerInfo>::iterator it = timers.begin(); it != timers.end(); ++it) {
    if (it->id == id) {
      timers.erase(it);
      return;
    }
  }
}

TimerInfo &gdioutput::addTimeoutMilli(int timeOut, const string &id, GUICALLBACK cb)
{
  removeTimeoutMilli(id);
  timers.emplace_back(this, cb);
  timers.back().id = id;
  SetTimer(hWndTarget, (UINT_PTR)&timers.back(), timeOut, gdiTimerProc);
  timers.back().setWnd = hWndTarget;
  return timers.back();
}

TimerInfo:: ~TimerInfo() {
  handler = 0;
  callBack = 0;
  if (setWnd)
    KillTimer(setWnd, (UINT_PTR)this);
}

TextInfo& gdioutput::addImage(const string& id, int yp, int xp, int format, 
  const wstring& imageId, int width, int height, GUICALLBACK cb) {
  bool skipBBCalc = (format & skipBoundingBox) == skipBoundingBox;
  format &= ~skipBoundingBox;

  int oldYP = TL.empty() ? -1 : TL.back().yp;
  TL.emplace_back();
  TextInfo& TI = TL.back();
  itTL = TL.begin();

  imageReferences.push_back(&TI);

  TI.id = id;
  TI.format = format | textImage;
  TI.xp = xp;
  TI.yp = yp;
  TI.text = L"L" + imageId;
  TI.callBack = cb;
  
  if (width == 0 || height == 0) {
    uint64_t imgId = _wcstoui64(imageId.c_str(), nullptr, 10);
    int rwidth = image.getWidth(imgId);
    int rheight = image.getHeight(imgId);

    if (width == 0 && height == 0) {
      width = rwidth;
      height = rheight;
    }
    else if (height == 0) 
      height = (width * rheight) / rwidth;
    else
      width = (height * rwidth) / rheight;
  }

    //if (skipBBCalc) {
  TI.textRect.left = xp;
  TI.textRect.top = yp;
  TI.textRect.right = xp + width;
  TI.textRect.bottom = yp + height;
  TI.realWidth = width;
  
  FlowDirection oldDir = flowDirection;

  if (format & imageNoUpdatePos)
    flowDirection = FlowDirection::None;

  updatePos(TI.xp, TI.yp, width + scaleLength(10),
            height + scaleLength(2));
  
  flowDirection = oldDir;

  if (oldYP > TI.yp)
    renderOptimize = false;
  
  return TL.back();
}

TextInfo* gdioutput::setImage(const string& id, int imgId, bool update) {
  return (TextInfo *)setText(id.c_str(), L"L" + itow(imgId), update);
}

TextInfo &gdioutput::addStringUT(int yp, int xp, int format, const string &text,
                                 int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addStringUT(yp, xp, format, widen(text), xlimit, cb, fontFace);
}

int gdioutput::getFontHeight(int format, const wstring &fontFace) const {
  format = format & 0xFF;
  auto res = fontHeightCache.find(make_pair(format, fontFace));

  if (res != fontHeightCache.end())
    return res->second;

  TextInfo TI;
  TI.format = format;
  TI.xp = 0;
  TI.yp = 0;
  TI.text = L"M1y|";
  TI.xlimit = 100;
  TI.callBack = 0;
  TI.font = fontFace;
  calcStringSize(TI);
  int h = TI.textRect.bottom - TI.textRect.top;
  fontHeightCache.emplace(make_pair(format, fontFace), h);
  return h;
}

TextInfo& gdioutput::addStringUT(int yp, int xp, int format, const wstring& text,
  int xlimit, GUICALLBACK cb, const wchar_t* fontFace)
{
  bool skipBBCalc = (format & skipBoundingBox) == skipBoundingBox;
  format &= ~skipBoundingBox;
  int oldYP = TL.empty() ? -1 : TL.back().yp;

  TL.emplace_back();
  TextInfo& TI = TL.back();
  itTL = TL.begin();

  if ((format & 0xFF) == textImage)
    imageReferences.push_back(&TI);

  TI.format = format;
  TI.xp = xp;
  TI.yp = yp;
  TI.text = text;
  TI.xlimit = xlimit;
  TI.callBack = cb;
  if (fontFace)
    TI.font = fontFace;
  if (!skipTextRender(format)) {

    if (skipBBCalc) {
      assert(xlimit > 0);
      int h = getFontHeight(format, fontFace);
      TI.textRect.left = xp;
      TI.textRect.top = yp;
      TI.textRect.right = xp + xlimit;
      TI.textRect.bottom = yp + h;
      TI.realWidth = xlimit;

      updatePos(TI.xp, TI.yp, TI.realWidth + scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));

      maxTextBlockHeight = max(maxTextBlockHeight, h + 1);
    }
    else {
      HDC hDC = GetDC(hWndTarget);

      if (hWndTarget && !manualUpdate)
        RenderString(TI, hDC);
      else
        calcStringSize(TI, hDC);

      if (xlimit == 0 || (format & (textRight | textCenter)) == 0) {
        updatePosTight(TI.textRect.left, TI.yp,
          TI.realWidth, TI.textRect.bottom - TI.textRect.top,
          scaleLength(10), scaleLength(2));
      }
      else {
        updatePosTight(TI.xp, TI.yp,
          TI.realWidth, TI.textRect.bottom - TI.textRect.top,
          scaleLength(10), scaleLength(2));
      }
      ReleaseDC(hWndTarget, hDC);
      maxTextBlockHeight = max<int>(maxTextBlockHeight, 1 + TI.textRect.bottom - TI.textRect.top);
    }

    if (oldYP > TI.yp)
      renderOptimize = false;
  }
  else {
    TI.textRect.left = xp;
    TI.textRect.right = xp;
    TI.textRect.bottom = yp;
    TI.textRect.top = yp;
  }

  return TL.back();
}

TextInfo &gdioutput::addString(const char *id, int yp, int xp, int format, const string &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace)
{
  return addString(id, yp, xp, format, widen(text), xlimit, cb, fontFace);
}

TextInfo& gdioutput::addString(const char* id, int yp, int xp, int format, const wstring& text,
  int xlimit, GUICALLBACK cb, const wchar_t* fontFace)
{
  int oldYP = TL.empty() ? -1 : TL.back().yp;

  TL.emplace_back();
  itTL = TL.begin();
  TextInfo& TI = TL.back();

  if ((format & 0xFF) == textImage)
    imageReferences.push_back(&TI);

  TI.format = format;
  TI.xp = xp;
  TI.yp = yp;
  if ((format & 0xFF) != textImage) {
    TI.text = lang.tl(text);
    if ((format & Capitalize) == Capitalize && lang.capitalizeWords())
      capitalizeWords(TI.text);
  }
  else {
    TI.text = text;
  }
  TI.id = id;
  TI.xlimit = xlimit;
  TI.callBack = cb;
  if (fontFace)
    TI.font = fontFace;

  if (!skipTextRender(format)) {
    HDC hDC = GetDC(hWndTarget);

    if (hWndTarget && !manualUpdate)
      RenderString(TI, hDC);
    else
      calcStringSize(TI, hDC);

    if (xlimit == 0 || (format & (textRight | textCenter)) == 0) {
      updatePos(TI.textRect.right + OffsetX, yp, scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));
    }
    else {
      updatePos(TI.xp, TI.yp, TI.realWidth + scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));
    }
    ReleaseDC(hWndTarget, hDC);

    maxTextBlockHeight = max<int>(maxTextBlockHeight, TI.textRect.bottom - TI.textRect.top + 1);

    if (oldYP > TI.yp)
      renderOptimize = false;
  }
  else {
    TI.textRect.left = xp;
    TI.textRect.right = xp;
    TI.textRect.bottom = yp;
    TI.textRect.top = yp;
  }

  return TL.back();
}

TextInfo &gdioutput::addString(const string &id, int format, const string &text, GUICALLBACK cb) {
  return addString(id.c_str(), CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const string &id, int format, const wstring &text, GUICALLBACK cb) {
  return addString(id.c_str(), CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const string &id, int yp, int xp, int format, const string &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addString(id.c_str(), yp, xp, format, text, xlimit, cb, fontFace);
}

TextInfo &gdioutput::addString(const string &id, int yp, int xp, int format, const wstring &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addString(id.c_str(), yp, xp, format, text, xlimit, cb, fontFace);
}

TextInfo &gdioutput::addString(const char *id, int format, const string &text, GUICALLBACK cb)
{
  return addString(id, CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const char *id, int format, const wstring &text, GUICALLBACK cb)
{
  return addString(id, CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addStringUT(int format, const string &text, GUICALLBACK cb)
{
  return addStringUT(CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addStringUT(int format, const wstring &text, GUICALLBACK cb)
{
  return addStringUT(CurrentY, CurrentX, format, text, 0, cb);
}

ButtonInfo &gdioutput::addButton(const string &id, const string &text, GUICALLBACK cb,
                                 const string &tooltip)
{
  return addButton(CurrentX,  CurrentY, id, text, cb, tooltip);
}

ButtonInfo &gdioutput::addButton(const string &id, const wstring &text, GUICALLBACK cb,
                                 const wstring &tooltip)
{
  return addButton(CurrentX,  CurrentY, id, text, cb, tooltip);
}

ButtonInfo &gdioutput::addButton(int x, int y, const string &id, const string &text, GUICALLBACK cb,
                                 const string &tooltip)
{
  return addButton(x,y, id, widen(text), cb, widen(tooltip));
}

ButtonInfo &gdioutput::addButton(int x, int y, const string &id, const wstring &text, GUICALLBACK cb,
  const wstring &tooltip)
{
  HANDLE bm = 0;
  int width = 0;
  if (text[0] == '@') {
    HINSTANCE hInst = GetModuleHandle(0);    int ir = _wtoi(text.c_str() + 1);
    bm = LoadBitmap(hInst, MAKEINTRESOURCE(ir));

    SIZE size;
    size.cx = 24;
    width = size.cx+4;
  }
  else {
    SIZE size;
    HDC hDC = GetDC(hWndTarget);
    SelectObject(hDC, getGUIFont());
    wstring ttext = lang.tl(text);
    int tts = ttext.size();
    if (tts > 2 && ttext[0] == '<' && ttext[1] == '<') {
      ttext = L"◀" + ttext.substr(2);
    }
    else if (tts > 2 && ttext[tts-1] == '>' && ttext[tts-2] == '>') {
      ttext = ttext.substr(0, tts-2) + L"▶";
    }
    if (lang.capitalizeWords())
      capitalizeWords(ttext);
    GetTextExtentPoint32(hDC, ttext.c_str(), ttext.length(), &size);
    ReleaseDC(hWndTarget, hDC);
    width = size.cx + scaleLength(30);
    if (text != L"...")
      width = max<int>(width, scaleLength(75));
  }

  ButtonInfo &bi=addButton(x, y, width, id, text, cb, tooltip, false, false);

  if (bm != 0) {
    SendMessage(bi.hWnd, BM_SETIMAGE, IMAGE_BITMAP, LPARAM(bm));
  }

  return bi;
}

ButtonInfo &ButtonInfo::setDefault()
{
  flags |= 1;
  storedFlags |= 1;
  //SetWindowLong(hWnd, i, GetWindowLong(hWnd, i)|BS_DEFPUSHBUTTON);
  return *this;
}

void ButtonInfo::moveButton(gdioutput &gdi, int nxp, int nyp) {
  xp = nxp;
  yp = nyp;
  int w, h;
  getDimension(gdi, w, h);
  MoveWindow(hWnd, xp, yp, w, h, true);
  gdi.updatePos(xp, yp, w, h);
}

void ButtonInfo::getDimension(const gdioutput &gdi, int &w, int &h) const {
  RECT rc;
  GetWindowRect(hWnd, &rc);
  w = rc.right - rc.left + gdi.scaleLength(GDI_BUTTON_SPACING);
  h = rc.bottom - rc.top;
}

ButtonInfo &gdioutput::addButton(int x, int y, int w, const string &id,
                                 const string &text, GUICALLBACK cb, const string &tooltip,
                                 bool AbsPos, bool hasState) {
  return addButton(x, y, w, id, widen(text), cb, widen(tooltip), AbsPos, hasState);
}

ButtonInfo& gdioutput::addButton(int x, int y, int w, const string& id,
  const wstring& text, GUICALLBACK cb, const wstring& toolTip,
  bool absPos, bool hasState) {
  return addButton(x, y, w, getButtonHeight(), id, text,
    gdiFonts::normalText, cb, toolTip, absPos, hasState);
  }


ButtonInfo& gdioutput::addButton(int x, int y, int width, int height,
  const string& id, const wstring& text,
  gdiFonts font, GUICALLBACK cb,
  const wstring& tooltip,
  bool absPos, bool hasState) {
  int style = hasState ? BS_CHECKBOX | BS_PUSHLIKE : BS_PUSHBUTTON;

  if (text[0] == '@')
    style |= BS_BITMAP;

  ButtonInfo bi;
  wstring ttext = lang.tl(text);
  int tts = ttext.size();
  if (tts > 2 && ttext[0] == '<' && ttext[1] == '<') {
    ttext = L"◀" + ttext.substr(2);
  }
  else if (tts > 2 && ttext[tts - 1] == '>' && ttext[tts - 2] == '>') {
    ttext = ttext.substr(0, tts - 2) + L"▶";
  }
  if (lang.capitalizeWords())
    capitalizeWords(ttext);
  if (absPos) {
    if (ttext.find_first_of('\n') != string::npos) { 
      style |= BS_MULTILINE;
      height *= 2;
    }
    bi.hWnd = CreateWindow(L"BUTTON", ttext.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }
  else {
    bi.hWnd = CreateWindow(L"BUTTON", ttext.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y - OffsetY - 1, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }

  if (font == gdiFonts::normalText)
    SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);
  else
    SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getCurrentFont().getFont(font), 0);

  if (!absPos)
    updatePos(x, y, width + scaleLength(GDI_BUTTON_SPACING), height + 5);

  bi.xp = x;
  bi.yp = y - 1;
  bi.width = width;
  bi.text = ttext;
  bi.id = id;
  bi.callBack = cb;
  bi.AbsPos = absPos;

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, bi.hWnd);

  BI.push_back(bi);
  biByHwnd[bi.hWnd] = &BI.back();

  FocusList.push_back(bi.hWnd);
  return BI.back();
}

static int checkBoxCallback(gdioutput *gdi, GuiEventType type, BaseInfo *data) {
  if (type == GUI_LINK) {
    TextInfo *ti = (TextInfo *)data;
    string cid = ti->id.substr(1);
    gdi->check(cid, !gdi->isChecked(cid), true);
    ButtonInfo &bi = ((ButtonInfo &)gdi->getBaseInfo(cid.c_str()));
    if (bi.callBack || bi.hasEventHandler())
      gdi->sendCtrlMessage(cid);
    //gdi->getBaseInfo(cid);
  }
  return 0;
}

void gdioutput::enableCheckBoxLink(TextInfo &ti, bool enable) {
  bool needRefresh = false;
  if (enable) {
    needRefresh = ti.callBack == 0;
    ti.callBack = checkBoxCallback;
    ti.setColor(colorDefault);
  }
  else {
    needRefresh = ti.callBack != 0;
    ti.callBack = 0;
    DWORD c = GetSysColor(COLOR_GRAYTEXT);
    ti.setColor(GDICOLOR(c));
  }
  if (needRefresh)
    InvalidateRect(hWndTarget, &ti.textRect, true);
}

ButtonInfo &gdioutput::addCheckbox(const string &id, const string &text,
                                   GUICALLBACK cb, bool Checked, const string &tooltip)
{
  return addCheckbox(CurrentX,  CurrentY,  id, text, cb, Checked, tooltip);
}

ButtonInfo &gdioutput::addCheckbox(const string &id, const wstring &text,
                                   GUICALLBACK cb, bool Checked, const wstring &tooltip)
{
  return addCheckbox(CurrentX,  CurrentY,  id, text, cb, Checked, tooltip);
}

ButtonInfo &gdioutput::addCheckbox(int x, int y, const string &id, const string &text,
                                   GUICALLBACK cb, bool Checked, const string &tooltip, bool AbsPos)
{
  return addCheckbox(x,y,id, widen(text), cb, Checked, widen(tooltip), AbsPos);
}

ButtonInfo& gdioutput::addCheckbox(int x, int y, const string& id, const wstring& text,
  GUICALLBACK cb, bool Checked, const wstring& tooltip, bool AbsPos)
{
  ButtonInfo bi;
  SIZE size;

  wstring ttext = lang.tl(text);
  HDC hDC = GetDC(hWndTarget);
  SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  GetTextExtentPoint32(hDC, L"M", 1, &size);

  int ox = OffsetX;
  int oy = OffsetY;

  if (AbsPos) {
    ox = 0;
    oy = 0;
  }

  int h = size.cy;
  SelectObject(hDC, getGUIFont());
  GetTextExtentPoint32(hDC, ttext.c_str(), ttext.length(), &size);
  ReleaseDC(hWndTarget, hDC);

  int cbY = y + (size.cy - h) / 2;
  bi.hWnd = CreateWindowEx(0, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE |
    WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX | BS_NOTIFY,
    x - ox, cbY - oy, h, h, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  TextInfo& desc = addStringUT(y, x + (3 * h) / 2, 0, ttext, 0, checkBoxCallback);
  desc.id = "T" + id;

  SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);

  if (Checked)
    SendMessage(bi.hWnd, BM_SETCHECK, BST_CHECKED, 0);

  bi.checked = Checked;

  if (!AbsPos) {
    if (ttext.empty())
      updatePos(x, y, size.cx + int(30 * scale), size.cy + int(scale * 12) + 3);
    else
      updatePos(x, y, size.cx + int(30 * scale), desc.textRect.bottom - desc.textRect.top + scaleLength(4));
  }
  if (tooltip.length() > 0) {
    addToolTip(id, tooltip, bi.hWnd);
    addToolTip(desc.id, tooltip, 0, &desc.textRect);
  }
  bi.isCheckbox = true;
  bi.xp = x;
  bi.yp = cbY;
  bi.width = desc.textRect.right - (x - ox);
  bi.text = ttext;
  bi.id = id;
  bi.callBack = cb;
  bi.AbsPos = AbsPos;
  bi.originalState = Checked;
  bi.isEdit(true);
  BI.push_back(bi);
  biByHwnd[bi.hWnd] = &BI.back();

  FocusList.push_back(bi.hWnd);
  return BI.back();
}

bool gdioutput::isChecked(const string &id)
{
  list<ButtonInfo>::iterator it;
  for(it=BI.begin(); it != BI.end(); ++it)
    if (it->id==id)
      return SendMessage(it->hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;

  return false;
}

void gdioutput::check(const string &id, bool state, bool keepOriginalState){
  list<ButtonInfo>::iterator it;
  for(it=BI.begin(); it != BI.end(); ++it) {
    if (it->id==id){
      SendMessage(it->hWnd, BM_SETCHECK, state ? BST_CHECKED:BST_UNCHECKED, 0);
      it->checked = state;
      it->synchData();
      if (!keepOriginalState)
        it->originalState = state;
      return;
    }
  }

  #ifdef _DEBUG
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  #endif
}

InputInfo &gdioutput::addInput(const string &id, const wstring &text, int length, 
                               GUICALLBACK cb, const wstring &explanation, const wstring &help)
{
  return addInput(CurrentX, CurrentY, id, text, length, cb, explanation, help);
}

HFONT gdioutput::getGUIFont() const
{
  if (scale==1)
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  else
    return getCurrentFont().getGUIFont();
}

pair<int, int> gdioutput::getInputDimension(int length) const {
  if (!guiMeasure) {
    HDC hDC = GetDC(hWndTarget);
    SelectObject(hDC, getGUIFont());
    SIZE size;
    GetTextExtentPoint32(hDC, L"M", 1, &size);

    SIZE sizeAvg;
    wstring avgText = L"123456789ABCDEFGHIJHKLMNOPQRSTUVXYZ abcdefghijklmnopqrstuvxyz";
    GetTextExtentPoint32(hDC, avgText.c_str(), avgText.length(), &sizeAvg);
    ReleaseDC(hWndTarget, hDC);

    int dy = GetSystemMetrics(SM_CYEDGE);
    int dx = GetSystemMetrics(SM_CXEDGE);
    guiMeasure = make_shared<GuiMeasure>();
    guiMeasure->letterWidth = size.cx;
    guiMeasure->extraX = 2 * dx;
    guiMeasure->height = 4 + dy * 2 + size.cy;
    guiMeasure->avgCharWidth = float(sizeAvg.cx) / float(avgText.length());
  }

  return make_pair(length * guiMeasure->letterWidth + guiMeasure->extraX, guiMeasure->height);
}

int gdioutput::getButtonHeight() const {
  return int(getInputDimension(0).second * 1.2);//int(scale * 24) + 0;
}


InputInfo &gdioutput::addInput(int x, int y, const string &id, const wstring &text,
                               int length, GUICALLBACK cb,
                               const wstring &explanation, const wstring &help) {
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  InputInfo ii;
  
  auto dim = getInputDimension(length);
  int ox=OffsetX;
  int oy=OffsetY;

  ii.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
    WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL | WS_BORDER,
    x-ox, y-oy, dim.first, dim.second,
    hWndTarget, NULL, (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  int mrg = scaleLength(4);
  updatePos(x, y, dim.first+mrg, dim.second+mrg);

  SendMessage(ii.hWnd, WM_SETFONT,
              (WPARAM) getGUIFont(), 0);

  ii.xp=x;
  ii.yp=y;
  ii.width = dim.first;
  ii.height = dim.second;
  ii.text = text;
  ii.original = text;
  ii.focusText = text;
  ii.id=id;
  ii.callBack=cb;

  II.push_back(ii);
  iiByHwnd[ii.hWnd] = &II.back();
  if (help.length() > 0)
    addToolTip(id, help, ii.hWnd);

  FocusList.push_back(ii.hWnd);

  if (II.size() == 1) {
    SetFocus(ii.hWnd);
    currentFocus = ii.hWnd;
  }

  return II.back();
}

InputInfo &gdioutput::addInputBox(const string &id, int width, int height, const wstring &text,
                                  GUICALLBACK cb, const wstring &explanation)
{
  return addInputBox(id, CurrentX, CurrentY, width, height, text, cb, explanation);
}

InputInfo &gdioutput::addInputBox(const string &id, int x, int y, int widthIn, int heightIn,
                                  const wstring &text, GUICALLBACK cb, const wstring &explanation)
{
  if (explanation.length()>0) {
    addString("", y, x, 0, explanation);
    y+=lineHeight;
  }
  int width = scaleLength(widthIn);
  int height = scaleLength(heightIn);
  InputInfo ii;

  int ox=OffsetX;
  int oy=OffsetY;

  ii.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(), WS_HSCROLL|WS_VSCROLL|
    WS_TABSTOP|WS_VISIBLE|WS_CHILD | WS_CLIPSIBLINGS |ES_AUTOHSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|WS_BORDER,
    x-ox, y-oy, width, height, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, width, height + scaleLength(5));

  SendMessage(ii.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  ii.xp=x;
  ii.yp=y;
  ii.width = width;
  ii.height = height;
  ii.text = text;
  ii.original = text;
  ii.focusText = text;
  ii.id=id;
  ii.callBack=cb;
  II.push_back(ii);

  iiByHwnd[ii.hWnd] = &II.back();
  
  FocusList.push_back(ii.hWnd);
  return II.back();
}

ListBoxInfo &gdioutput::addListBox(const string &id, int width, int height, GUICALLBACK cb, const wstring &explanation, const wstring &tooltip, bool multiple)
{
  return addListBox(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip, multiple);
}

LRESULT CALLBACK GetMsgProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo *lbi = (ListBoxInfo *)(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  if (!lbi) {
    throw std::exception("Internal GDI error");
  }

  LPARAM res = CallWindowProc(lbi->originalProc, hWnd, iMsg, wParam, lParam);
  if (iMsg == WM_VSCROLL || iMsg == WM_MOUSEWHEEL || iMsg == WM_KEYDOWN) {
    LRESULT topIndex = CallWindowProc(lbi->originalProc, hWnd, LB_GETTOPINDEX, 0, 0);
    if (lbi->lbiSync) {
      ListBoxInfo *other = lbi->lbiSync;
      CallWindowProc(other->originalProc, other->hWnd, LB_SETTOPINDEX, topIndex, 0);
    }
  }
  return res;
}

void gdioutput::synchronizeListScroll(const string &id1, const string &id2)
{
  ListBoxInfo *a = 0, *b = 0;
  list<ListBoxInfo>::iterator it;
  for (it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id1)
      a = &*it;
    else if (it->id == id2)
      b = &*it;
  }
  if (!a || !b)
    throw std::exception("Not found");

  a->lbiSync = b;
  b->lbiSync = a;
  SetWindowLongPtr(a->hWnd, GWLP_USERDATA, LONG_PTR(a));
  SetWindowLongPtr(b->hWnd, GWLP_USERDATA, LONG_PTR(b));

  a->originalProc = WNDPROC(GetWindowLongPtr(a->hWnd, GWLP_WNDPROC));
  b->originalProc = WNDPROC(GetWindowLongPtr(b->hWnd, GWLP_WNDPROC));

  SetWindowLongPtr(a->hWnd, GWLP_WNDPROC, LONG_PTR(GetMsgProc));
  SetWindowLongPtr(b->hWnd, GWLP_WNDPROC, LONG_PTR(GetMsgProc));
}

ListBoxInfo &gdioutput::addListBox(int x, int y, const string &id, int width, int height, GUICALLBACK cb, 
                                   const wstring &explanation, const wstring &tooltip, bool multiple) {
  if (explanation.length()>0) {
    addString(id+"_label", y, x, 0, explanation);
    y+=lineHeight;
  }
  ListBoxInfo lbi;
  int ox=OffsetX;
  int oy=OffsetY;

  DWORD style=WS_TABSTOP|WS_VISIBLE|WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|LBS_USETABSTOPS|LBS_NOTIFY|WS_VSCROLL;

  if (multiple)
    style|=LBS_MULTIPLESEL;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",  style,
    x-ox, y-oy, int(width*scale), int(height*scale), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale*(width+5)), int(scale * (height+2)));
  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=false;
  lbi.multipleSelection = multiple;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*height;
  lbi.id=id;
  lbi.callBack=cb;
  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();
  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

void gdioutput::setSelection(const string &id, const set<int> &selection)
{
  list<ListBoxInfo>::iterator it;
  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id && !it->IsCombo) {
      list<int>::const_iterator cit;

      if (selection.count(-1)==1)
        SendMessage(it->hWnd, LB_SETSEL, 1, -1);
      else {
        LRESULT count=SendMessage(it->hWnd, LB_GETCOUNT, 0,0);
        SendMessage(it->hWnd, LB_SETSEL, 0, -1);
        for(int i=0;i<count;i++){
          LRESULT d=SendMessage(it->hWnd, LB_GETITEMDATA, i, 0);

          if (selection.count(int(d))==1)
            SendMessage(it->hWnd, LB_SETSEL, 1, i);
        }
        return;
      }
    }
  }
}

void gdioutput::getSelection(const string &id, set<int> &selection) {
  list<ListBoxInfo>::iterator it;
  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id && !it->IsCombo) {
      selection.clear();
      LRESULT count=SendMessage(it->hWnd, LB_GETCOUNT, 0,0);
      for(int i=0;i<count;i++){
        LRESULT s=SendMessage(it->hWnd, LB_GETSEL, i, 0);
        if (s) {
          LRESULT d=SendMessage(it->hWnd, LB_GETITEMDATA, i, 0);
          selection.insert(int(d));
        }
      }
      return;
    }
  }

  #ifdef _DEBUG
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  #endif
}

ListBoxInfo &gdioutput::addSelection(const string &id, int width, int height, GUICALLBACK cb, const wstring &explanation, const wstring &tooltip)
{
  return addSelection(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip);
}

ListBoxInfo &gdioutput::addSelection(int x, int y, const string &id, int width, int height,
                                     GUICALLBACK cb, const wstring &explanation, const wstring &tooltip)
{
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  ListBoxInfo lbi;

  int ox = OffsetX;
  int oy = OffsetY;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",  WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|CBS_DROPDOWNLIST|WS_VSCROLL ,
    x-ox, y-oy, int(scale*width), int(scale*height), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale*(width+5)), int(scale*30));

  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=true;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*30;
  lbi.id=id;
  lbi.callBack=cb;

  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

ListBoxInfo &gdioutput::addCombo(const string &id, int width, int height, GUICALLBACK cb,
                                 const wstring &explanation, const wstring &tooltip) {
  return addCombo(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip);
}

ListBoxInfo &gdioutput::addCombo(int x, int y, const string &id, int width, int height, GUICALLBACK cb, 
                                 const wstring &explanation, const wstring &tooltip) {
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  ListBoxInfo lbi;
  int ox=OffsetX;
  int oy=OffsetY;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",  WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|CBS_DROPDOWN |CBS_AUTOHSCROLL,
    x-ox, y-oy, int(scale*width), int(scale*height), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale * (width+5)), getButtonHeight()+scaleLength(5));

  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=true;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*height;
  lbi.id=id;
  lbi.callBack=cb;

  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

bool gdioutput::addItem(const string &id, const wstring &text, size_t data) {
  list<ListBoxInfo>::reverse_iterator it;
  for (it=LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT index=SendMessage(it->hWnd, CB_ADDSTRING, 0, LPARAM(text.c_str()));
        SendMessage(it->hWnd, CB_SETITEMDATA, index, data);
        it->data2Index[data] = int(index);
        it->computed_hash = 0;
      }
      else {
        LRESULT index=SendMessage(it->hWnd, LB_INSERTSTRING, -1, LPARAM(text.c_str()));
        SendMessage(it->hWnd, LB_SETITEMDATA, index, data);
        it->data2Index[data] = int(index);
        it->computed_hash = 0;
      }
      return true;
    }
  }
  return false;
}

bool gdioutput::modifyItemDescription(const string& id, size_t itemData, const wstring &description) {
  for (auto it = LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id == id) {
      int ix = it->data2Index[itemData];
      // It is intentioal that the hash is not modified. This method allows "customization" of
      // some description without reloading a complete listbox
      if (it->IsCombo) {
        SendMessage(it->hWnd, CB_DELETESTRING, ix, 0);
        SendMessage(it->hWnd, CB_INSERTSTRING, ix, LPARAM(description.c_str()));
        SendMessage(it->hWnd, CB_SETITEMDATA, ix, itemData);
      }
      else {
        SendMessage(it->hWnd, LB_DELETESTRING, ix, 0);
        SendMessage(it->hWnd, LB_INSERTSTRING, ix, LPARAM(description.c_str()));
        SendMessage(it->hWnd, LB_SETITEMDATA, ix, itemData);
      }
      return true;
    }
  }

  return false;
}

bool gdioutput::setItems(const string& id, const vector<pair<wstring, size_t>>& items) {
  auto hash = ListBoxInfo::computeItemHash(items);
  for (auto it = LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id == id) {
      if (it->IsCombo) {
        if (it->computed_hash == 0 || it->computed_hash != hash) {
          SendMessage(it->hWnd, CB_RESETCONTENT, 0, 0);
          SendMessage(it->hWnd, CB_INITSTORAGE, items.size(), 48);
          SendMessage(it->hWnd, WM_SETREDRAW, FALSE, 0);
          it->data2Index.clear();

          for (size_t k = 0; k < items.size(); k++) {
            LRESULT index = SendMessage(it->hWnd, CB_ADDSTRING, 0, LPARAM(items[k].first.c_str()));
            SendMessage(it->hWnd, CB_SETITEMDATA, index, items[k].second);
            it->data2Index[items[k].second] = int(index);
          }
          SendMessage(it->hWnd, WM_SETREDRAW, TRUE, 0);
          RedrawWindow(it->hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
          it->computed_hash = hash;
        }
        else {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
        }
      }
      else {
        if (it->computed_hash == 0 || it->computed_hash != hash) {
          SendMessage(it->hWnd, LB_RESETCONTENT, 0, 0);
          SendMessage(it->hWnd, LB_INITSTORAGE, items.size(), 48);
          SendMessage(it->hWnd, WM_SETREDRAW, FALSE, 0);

          it->data2Index.clear();
          for (size_t k = 0; k < items.size(); k++) {
            LRESULT index = SendMessage(it->hWnd, LB_INSERTSTRING, -1, LPARAM(items[k].first.c_str()));
            SendMessage(it->hWnd, LB_SETITEMDATA, index, items[k].second);
            it->data2Index[items[k].second] = int(index);
          }

          SendMessage(it->hWnd, WM_SETREDRAW, TRUE, 0);
          RedrawWindow(it->hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
          it->computed_hash = hash;
        }
        else {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
        }
      }
      return true;
    }
  }
  return false;
}

void gdioutput::filterOnData(const string &id, const unordered_set<int> &filter) {
  list<ListBoxInfo>::iterator it;
  for (it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->id==id) {
      if (it->IsCombo) {
      }
      else {
        it->computed_hash = 0;
        const HWND &hWnd = it->hWnd;
        LRESULT count = SendMessage(hWnd, LB_GETCOUNT, 0, 0);
        for (intptr_t ix = count - 1; ix>=0; ix--) {
          LRESULT ret = SendMessage(hWnd, LB_GETITEMDATA, ix, 0);
          if (ret != LB_ERR && filter.count(int(ret)) == 0)
            SendMessage(hWnd, LB_DELETESTRING, ix, 0);
        }
        return;
      }
    }
  }
  assert(false);
}

bool gdioutput::clearList(const string& id) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      it->original = L"";
      it->originalIdx = -1;
      it->computed_hash = 0;
      it->data2Index.clear();

      if (it->IsCombo)
        SendMessage(it->hWnd, CB_RESETCONTENT, 0, 0);
      else
        SendMessage(it->hWnd, LB_RESETCONTENT, 0, 0);
      return true;
    }
  }

  return false;
}

bool gdioutput::getSelectedItem(const string &id, ListBoxInfo &lbi) {
  lbi = ListBoxInfo();
  list<ListBoxInfo>::iterator it;
  for (it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->id==id) {
      bool ret = getSelectedItem(*it);
      it->copyUserData(lbi);
      return ret;
    }
  }
  return false;
}

pair<int, bool> gdioutput::getSelectedItem(const string &id) {
  ListBoxInfo lbi;
  bool ret = getSelectedItem(id, lbi);
  return make_pair(lbi.getDataInt(), ret);
}

pair<int, bool> gdioutput::getSelectedItem(const char *id) {
  string ids = id;
  return getSelectedItem(ids);
}

void ListBoxInfo::copyUserData(ListBoxInfo &dest) const {
  dest.data = data;
  dest.text = text;
  dest.id = id;
  dest.extra = extra;
  dest.index = index;
  dest.IsCombo = IsCombo;
}

uint64_t ListBoxInfo::computeItemHash(const vector<pair<wstring, size_t>>& items) {
  uint64_t res = 1;
  for (auto& it : items) {
    res = res * 997 + it.second;
    for (auto ch : it.first)
      res = res * 2003 + ch;
  }

  return res;
}

bool gdioutput::getSelectedItem(ListBoxInfo &lbi) {
  if (lbi.IsCombo) {
    LRESULT index=SendMessage(lbi.hWnd, CB_GETCURSEL, 0, 0);

    if (index == CB_ERR) {
      wchar_t bf[256];
      GetWindowText(lbi.hWnd, bf, 256);
      lbi.text=bf;
      lbi.data=-1;
      lbi.index=int(index);
      return false;
    }
    lbi.data=SendMessage(lbi.hWnd, CB_GETITEMDATA, index, 0);
    wchar_t bf[1024];
    if (SendMessage(lbi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR)
      lbi.text=bf;
  }
  else {
    LRESULT index=SendMessage(lbi.hWnd, LB_GETCURSEL, 0, 0);

    if (index==LB_ERR)
      return false;

    lbi.data=SendMessage(lbi.hWnd, LB_GETITEMDATA, index, 0);
    lbi.index=int(index);

    TCHAR bf[1024];
    if (SendMessage(lbi.hWnd, LB_GETTEXT, index, LPARAM(bf))!=LB_ERR)
      lbi.text=bf;
  }
  return true;
}

int gdioutput::getNumItems(const char *id) {
  for (auto &lbi : LBI) {
    if (lbi.id == id) {
      if (lbi.IsCombo) {
        return (int)SendMessage(lbi.hWnd, CB_GETCOUNT, 0, 0);
      }
      else {
        return (int)SendMessage(lbi.hWnd, LB_GETCOUNT, 0, 0);
      }
    }
  }

#ifdef _DEBUG
  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
#endif

  return 0;
}

int gdioutput::getItemDataByName(const char *id, const char *name) const{
  wstring wname = recodeToWide(name);
  list<ListBoxInfo>::const_iterator it;
  for(it = LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT ix = SendMessage(it->hWnd, CB_FINDSTRING, -1, LPARAM(wname.c_str()));
        if (ix >= 0) {
          return (int)SendMessage(it->hWnd, CB_GETITEMDATA, ix, 0);
        }
        return -1;
      }
      else {
        LRESULT ix = SendMessage(it->hWnd, LB_FINDSTRING, -1, LPARAM(wname.c_str()));
        if (ix >= 0) {
          return (int)SendMessage(it->hWnd, LB_GETITEMDATA, ix, 0);
        }
        return -1;
      }
    }
  }
  return -1;
}

bool gdioutput::selectItemByData(const char *id, int data)
{
  list<ListBoxInfo>::iterator it;
  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id) {
      if (it->IsCombo) {
        
        if (data==-1) {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          LRESULT count = SendMessage(it->hWnd, CB_GETCOUNT, 0, 0);
          
          for (int m = 0; m < count; m++) {
            LRESULT ret = SendMessage(it->hWnd, CB_GETITEMDATA, m, 0);
            if (ret == data) {
              SendMessage(it->hWnd, CB_SETCURSEL, m, 0);
              it->data = data;
              it->originalIdx = data;
              TCHAR bf[1024];
              if (SendMessage(it->hWnd, CB_GETLBTEXT, m, LPARAM(bf))!=CB_ERR) {
                it->text = bf;
                it->original = bf;
              }
              return true;
            }
          }
        }
        return false;
      }
      else {
        if (data==-1) {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
          it->data=0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          LRESULT count = SendMessage(it->hWnd, LB_GETCOUNT, 0, 0);
          for (int m = 0; m < count; m++) {
            LRESULT ret = SendMessage(it->hWnd, LB_GETITEMDATA, m, 0);

            if (ret == data) {
              SendMessage(it->hWnd, LB_SETCURSEL, m, 0);
              it->data = data;
              it->originalIdx = data;
              TCHAR bf[1024];
              if (SendMessage(it->hWnd, LB_GETTEXT, m, LPARAM(bf)) != LB_ERR) {
                it->text = bf;
                it->original = bf;
              }
              return true;
            }
          }
        }
        return false;
      }
    }
  }
  return false;
}

bool gdioutput::selectItemByIndex(const char *id, int index) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      if (it->IsCombo) {
        if (index == -1) {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          SendMessage(it->hWnd, CB_SETCURSEL, index, 0);
          LRESULT data = SendMessage(it->hWnd, CB_GETITEMDATA, index, 0);
          it->data = data;
          it->originalIdx = data;
          TCHAR bf[1024];
          if (SendMessage(it->hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR) {
            it->text = bf;
            it->original = bf;
          }
          return true;
        }
        return false;
      }
      else {
        if (index == -1) {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          SendMessage(it->hWnd, LB_SETCURSEL, index, 0);
          LRESULT data = SendMessage(it->hWnd, LB_GETITEMDATA, index, 0);

          it->data = data;
          it->originalIdx = data;
          TCHAR bf[1024];
          if (SendMessage(it->hWnd, LB_GETTEXT, index, LPARAM(bf)) != LB_ERR) {
            it->text = bf;
            it->original = bf;
          }
          return true;
        }
        return false;
      }
    }
  }
  return false;
}

bool gdioutput::autoGrow(const char *id) {
  list<ListBoxInfo>::iterator it;
  int size = 0;
  TextInfo TI;
  TI.format=0;
  TI.xp=0;
  TI.yp=0;
  TI.id="";
  TI.xlimit=0;
  TI.callBack=0;
  HDC hDC=GetDC(hWndTarget);

  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT count = SendMessage(it->hWnd, CB_GETCOUNT, 0, 0);
        for (int m = 0; m < count; m++) {
          wchar_t bf[1024];
          if (SendMessage(it->hWnd, CB_GETLBTEXT, m, LPARAM(bf))!=CB_ERR) {
            TI.text = bf;
            calcStringSize(TI, hDC);
            size = max<int>(size, TI.textRect.right - TI.textRect.left);
          }
        }
        
        ReleaseDC(hWndTarget, hDC);

        size += scaleLength(30);
        if (size > it->width) {
          it->width = size;
          SetWindowPos(it->hWnd, 0, 0, 0, (int)it->width, (int)it->height, SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOMOVE);
          updatePos(it->xp, it->yp, (int)it->width + int(scale*5), (int)it->height);
          return true;
        }
        return false;
      }
      else {
        LRESULT count = SendMessage(it->hWnd, LB_GETCOUNT, 0, 0);
        for (int m = 0; m < count; m++) {
          wchar_t bf[1024];
          LRESULT len = SendMessage(it->hWnd, LB_GETTEXT, m, LPARAM(bf));
          if (len!=LB_ERR) {
            if (it->lastTabStop == 0)
              TI.text = bf;
            else {
              auto pos = len;
              while(pos > 0) {
                if (bf[pos-1] == '\t') {
                  break;
                }
                pos--;
              }
              TI.text = &bf[pos];
            }
            calcStringSize(TI, hDC);
            size = max<int>(size, TI.realWidth + it->lastTabStop);
          }
        }
        
        ReleaseDC(hWndTarget, hDC);
        size += scaleLength(30);
        if (size > it->width) {
          it->width = size;
          SetWindowPos(it->hWnd, 0, 0, 0, (int)it->width, (int)it->height, SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOMOVE);
          updatePos(it->xp, it->yp, (int)it->width+int(scale*5), (int)it->height);
          return true;
        }
        return false;
      }
    }
  }

  ReleaseDC(hWndTarget, hDC);
  return false;
}

void gdioutput::removeSelected(const char *id)
{
}

LRESULT gdioutput::ProcessMsg(UINT iMessage, LPARAM lParam, WPARAM wParam)
{
  wstring msg;
  try {
    return ProcessMsgWrp(iMessage, lParam, wParam);
  }
  catch (const meosCancel&) {
    return false;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg=widen(ex.what());
    if (msg.empty())
      msg=L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg=L"Ett okänt fel inträffade.";
  }
  
  if (!msg.empty()) {
    alert(msg);
    setWaitCursor(false);
  }
  return 0;
}

void gdioutput::processButtonMessage(ButtonInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);

  switch (hwParam) {
    case BN_CLICKED: {
      string cmd;
      if (getRecorder().recording()) {
        if (bi.isExtraString()) {
          cmd = "press(\"" + bi.id + "\", \""  + narrow(bi.getExtra()) + "\"); //" + toUTF8(bi.text);
        }
        else {
          int arg = int((size_t)bi.extra);
          if (arg > 1000000 || arg < -1000000 || arg == 0)
            cmd = "press(\"" + bi.id + "\"); //" + toUTF8(bi.text);
          else
            cmd = "press(\"" + bi.id + "\", "  + itos(bi.getExtraInt()) + "); //" + toUTF8(bi.text);
        }
      }
      if (bi.isCheckbox)
        bi.checked = SendMessage(bi.hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;
      bi.synchData();
      if (bi.callBack || bi.hasEventHandler()) {
        setWaitCursor(true);
        if (!bi.handleEvent(*this, GUI_BUTTON) && bi.callBack)
          bi.callBack(this, GUI_BUTTON, &bi); //it may be destroyed here...

        setWaitCursor(false);
      }
      getRecorder().record(cmd);
      break;
    }
    case BN_SETFOCUS:
      if (currentFocus.hWnd != bi.hWnd) {
//        if (currentF      ocus.wasTabbed)
//          Button_SetState(currentFocus.hWnd, false);
        currentFocus = bi.hWnd;
      }
      break;
    case BN_KILLFOCUS:
      if (currentFocus.hWnd == bi.hWnd) {
//        if (currentFocus.wasTabbed)
//          Button_SetState(currentFocus.hWnd, false);
      }
      break;
  }
}

void gdioutput::processEditMessage(InputInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);

  switch (hwParam) {
    case EN_CHANGE:
      if (bi.writeLock)
        return;
      getWindowText(bi.hWnd, bi.text);
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_INPUTCHANGE);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_INPUTCHANGE);
      else if (bi.callBack)
        bi.callBack(this, GUI_INPUTCHANGE, &bi); //it may be destroyed here...
     
      break;

    case EN_KILLFOCUS: {
      autoCompleteInfo.reset();
      wstring old = bi.focusText;
      getWindowText(bi.hWnd, bi.text);
      bi.synchData();
      bool equal = old == bi.text;
      string cmd = "input(\"" + bi.id + "\", \"" + toUTF8(bi.text) + "\");";
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_INPUT);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_INPUT);
      else if (bi.callBack)
        bi.callBack(this, GUI_INPUT, &bi);
      if (!equal)
        getRecorder().record(cmd);
      break;
    }
    case EN_SETFOCUS:
      currentFocus = bi.hWnd;
      getWindowText(bi.hWnd, bi.text);
      bi.synchData();
      bi.focusText = bi.text;
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_FOCUS);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_FOCUS);
      else if (bi.callBack)
        bi.callBack(this, GUI_FOCUS, &bi);
      break;
  }
}

void gdioutput::processComboMessage(ListBoxInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);
  LRESULT index;
  switch (hwParam) {
    case CBN_SETFOCUS:
      currentFocus = bi.hWnd;
      lockUpDown = true;
      break;
    case CBN_KILLFOCUS: {
      if (autoCompleteInfo && !autoCompleteInfo->locked())
        autoCompleteInfo.reset();
      lockUpDown = false;

      TCHAR bf[1024];
      index=SendMessage(bi.hWnd, CB_GETCURSEL, 0, 0);

      if (index != CB_ERR) {
        if (SendMessage(bi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR) {
          bi.text = bf;
          bi.data=SendMessage(bi.hWnd, CB_GETITEMDATA, index, 0);
          if (bi.handler)
            bi.handler->handle(*this, bi, GUI_COMBO);
          else if (bi.callBack)
            bi.callBack(this, GUI_COMBO, &bi); //it may be destroyed here...
        }
      }
      else {
        GetWindowText(bi.hWnd, bf, sizeof(bf)-1);
        bi.data = -1;
        bi.text = bf;
        string cmd = "input(\"" + bi.id + "\", \"" + toUTF8(bi.text) + "\");";
        if (bi.handler)
          bi.handler->handle(*this, bi, GUI_COMBO);
        else if (bi.callBack)
          bi.callBack(this, GUI_COMBO, &bi); //it may be destroyed here...
        getRecorder().record(cmd);
      }
    }
    break;

    case CBN_EDITCHANGE: {
      if (bi.writeLock)
        return;
      getWindowText(bi.hWnd, bi.text);
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_COMBOCHANGE);
      else if (bi.callBack)
        bi.callBack(this, GUI_COMBOCHANGE, &bi); //it may be destroyed here...
      break;
    }
    case CBN_SELCHANGE:
      index=SendMessage(bi.hWnd, CB_GETCURSEL, 0, 0);

      if (index != CB_ERR) {
        bi.data=SendMessage(bi.hWnd, CB_GETITEMDATA, index, 0);

        TCHAR bf[1024];
        if (SendMessage(bi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR)
          bi.text=bf;
        string cmd = "select(\"" + bi.id + "\", " + itos(bi.data) + ");";
        internalSelect(bi);
        getRecorder().record(cmd);
      }
      break;
  }
}

void gdioutput::keyCommand(KeyCommandCode code) {
  if (hasCommandLock())
    return;

  if (code == KC_SLOWDOWN)
    autoSpeed *= 0.9;
  else if (code == KC_SPEEDUP)
    autoSpeed *= 1.0/0.9;

  wstring msg;
  try {
    list<TableInfo>::iterator tit;
    if (useTables) {
      for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
        if (tit->table->keyCommand(*this, code))
          return;
    }

    for (list<EventInfo>::iterator it = Events.begin(); it != Events.end(); ++it) {
      if (it->getKeyCommand() == code) {
        it->setData("", 0);
        it->setExtra(0);
        if (!it->handleEvent(*this, GUI_EVENT) && it->callBack) {
          it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
        }
        return;
      }
    }
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
}

void gdioutput::processListMessage(ListBoxInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);
  LRESULT index;

  switch (hwParam) {
    case LBN_SETFOCUS:
      currentFocus = bi.hWnd;
      lockUpDown = true;
      break;
    case LBN_KILLFOCUS:
      autoCompleteInfo.reset();
      lockUpDown = false;
      break;
    case LBN_SELCHANGE:
    case LBN_DBLCLK:

      index=SendMessage(bi.hWnd, LB_GETCURSEL, 0, 0);

      if (index!=LB_ERR) {
        bi.data = SendMessage(bi.hWnd, LB_GETITEMDATA, index, 0);

        TCHAR bf[1024];
        if (SendMessage(bi.hWnd, LB_GETTEXT, index, LPARAM(bf)) != LB_ERR)
          bi.text = bf;
        
        string cmd;
        if (hwParam == LBN_SELCHANGE)
          cmd = "select(\"" + bi.id + "\", " + itos(bi.data) + ");";
        else
          cmd = "dblclick(\"" + bi.id + "\", " + itos(bi.data) + ");";

        if (bi.callBack || bi.handler) {
          setWaitCursor(true);
          if (hwParam == LBN_SELCHANGE) {
            if (bi.handler)
              bi.handler->handle(*this, bi, GUI_LISTBOX);
            else
              bi.callBack(this, GUI_LISTBOX, &bi); //it may be destroyed here...
          }
          else {
            if (bi.handler)
              bi.handler->handle(*this, bi, GUI_LISTBOXSELECT);
            else
              bi.callBack(this, GUI_LISTBOXSELECT, &bi); //it may be destroyed here...
          }
          setWaitCursor(false);
        }
        getRecorder().record(cmd); 
      }
      break;
  }
}


LRESULT gdioutput::ProcessMsgWrp(UINT iMessage, LPARAM lParam, WPARAM wParam)
{
  if (iMessage == WM_COMMAND) {
    WORD hwParam = HIWORD(wParam);
    HWND hWnd = (HWND)lParam;
    if (hwParam == EN_CHANGE) {
      list<TableInfo>::iterator tit;
      if (useTables)
        for (tit = Tables.begin(); tit != Tables.end(); ++tit)
          if (tit->table->inputChange(*this, hWnd))
            return 0;
    }

    {
      //list<ButtonInfo>::iterator it;
      //for (it=BI.begin(); it != BI.end(); ++it) {
      unordered_map<HWND, ButtonInfo*>::iterator it = biByHwnd.find(HWND(lParam));

      //    if (it->hWnd==hWnd) {
      if (it != biByHwnd.end()) {
        ButtonInfo &bi = *it->second;
        processButtonMessage(bi, wParam);
        return 0;
      }
      //}
    }

    {
      unordered_map<HWND, InputInfo*>::iterator it = iiByHwnd.find(HWND(lParam));
      if (it != iiByHwnd.end()) {
        InputInfo &ii = *it->second;
        processEditMessage(ii, wParam);
        return 0;
      }
      //list<InputInfo>::iterator it;
      /*for (it=II.begin(); it != II.end(); ++it) {
        if (it->hWnd==hWnd) {
          processEditMessage(*it, wParam);
          return 0;
        }
      }*/
    }

    {
      //list<ListBoxInfo>::iterator it;
      //for(it=LBI.begin(); it != LBI.end(); ++it) {
      unordered_map<HWND, ListBoxInfo*>::iterator it = lbiByHwnd.find(HWND(lParam));
      if (it != lbiByHwnd.end()) {
        ListBoxInfo &lbi = *it->second;
        if (lbi.IsCombo)
          processComboMessage(lbi, wParam);
        else
          processListMessage(lbi, wParam);
        return 0;
      }
    }
  }
  else if (iMessage == WM_MOUSEMOVE) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    list<TableInfo>::iterator tit;

    bool GotCapture = false;

    if (useTables)
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        GotCapture = tit->table->mouseMove(*this, pt.x, pt.y) || GotCapture;

    if (GotCapture)
      return 0;

    list<InfoBox>::iterator it = IBox.begin();


    while (it != IBox.end()) {
      if (PtInRect(&it->textRect, pt) && (it->callBack || it->hasEventHandler())) {
        SetCursor(LoadCursor(NULL, IDC_HAND));

        HDC hDC = GetDC(hWndTarget);
        //drawBoxText(hDC, *it, true);
        drawBoxBg(hDC, *it);
        drawCloseBox(hDC, it->close, false);
        drawBoxText(hDC, *it, true);

        ReleaseDC(hWndTarget, hDC);
        SetCapture(hWndTarget);
        GotCapture = true;
        it->hasTCapture = true;
      }
      else {
        if (it->hasTCapture) {
          HDC hDC = GetDC(hWndTarget);
          //drawBoxText(hDC, *it, false);
          drawBoxBg(hDC, *it);
          drawCloseBox(hDC, it->close, false);
          drawBoxText(hDC, *it, false);
          
          ReleaseDC(hWndTarget, hDC);
          if (!GotCapture)
            ReleaseCapture();
          it->hasTCapture = false;
        }
      }

      if (it->hasCapture) {
        if (GetCapture() != hWndTarget) {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, false);
          ReleaseDC(hWndTarget, hDC);
          if (!GotCapture) ReleaseCapture();
          it->hasCapture = false;
        }
        else if (!PtInRect(&it->close, pt)) {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, false);
          ReleaseDC(hWndTarget, hDC);
        }
        else {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, true);
          ReleaseDC(hWndTarget, hDC);
        }
      }
      ++it;
    }

    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if ((!ti.callBack && !ti.hasEventHandler()) || ti.hasTimer)
        continue;

      if (PtInRect(&ti.textRect, pt)) {
        if (!ti.highlight) {
          ti.highlight = true;
          InvalidateRect(hWndTarget, &ti.textRect, true);
        }

        SetCapture(hWndTarget);
        GotCapture = true;
        ti.hasCapture = true;
        SetCursor(LoadCursor(NULL, IDC_HAND));
      }
      else {
        if (ti.highlight) {
          ti.highlight = false;
          InvalidateRect(hWndTarget, &ti.textRect, true);
        }

        if (ti.hasCapture) {
          if (!GotCapture)
            ReleaseCapture();

          ti.hasCapture = false;
        }
      }
    }
  }
  else if (iMessage == WM_LBUTTONDOWN) {
    if (autoCompleteInfo) {
      autoCompleteInfo.reset();
      return 0;
    }

    list<InfoBox>::iterator it = IBox.begin();

    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    list<TableInfo>::iterator tit;

    if (useTables) {
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftDown(*this, pt.x, pt.y))
          return 0;
    }

    while (it != IBox.end()) {
      if (PtInRect(&it->close, pt)) {
        HDC hDC = GetDC(hWndTarget);
        drawCloseBox(hDC, it->close, true);
        ReleaseDC(hWndTarget, hDC);
        SetCapture(hWndTarget);
        it->hasCapture = true;
      }
      ++it;
    }

    //Handle links
    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if (!ti.callBack && !ti.hasEventHandler())
        continue;

      if (ti.hasCapture) {
        HDC hDC = GetDC(hWndTarget);
        if (PtInRect(&ti.textRect, pt)) {
          ti.active = true;
          RenderString(ti, hDC);
        }
        ReleaseDC(hWndTarget, hDC);
      }
    }
  }
  else if (iMessage == WM_LBUTTONUP) {
    list<TableInfo>::iterator tit;

    list<InfoBox>::iterator it = IBox.begin();

    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftUp(*this, pt.x, pt.y))
          return 0;
    }
    while (it != IBox.end()) {
      if (it->hasCapture) {
        HDC hDC = GetDC(hWndTarget);
        drawCloseBox(hDC, it->close, false);
        ReleaseDC(hWndTarget, hDC);
        ReleaseCapture();
        it->hasCapture = false;

        if (PtInRect(&it->close, pt)) {
          RECT rc;
          computeBoxesBoundingBox(rc);
          IBox.erase(it);
          InvalidateRect(hWndTarget, &rc, true);
          //refresh();
          return 0;
        }
      }
      else if (it->hasTCapture) {
        ReleaseCapture();
        it->hasTCapture = false;

        if (PtInRect(&it->textRect, pt)) {
          if (!it->handleEvent(*this, GUI_INFOBOX) && it->callBack)
            it->callBack(this, GUI_INFOBOX, &*it); //it may be destroyed here...
          return 0;
        }
      }
      ++it;
    }

    //Handle links
    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if (!ti.callBack && !ti.hasEventHandler())
        continue;

      if (ti.hasCapture) {
        ReleaseCapture();
        ti.hasCapture = false;

        if (PtInRect(&ti.textRect, pt)) {
          if (ti.active) {
            string cmd;
            if (ti.getExtraInt() != 0)
              cmd = "click(\"" + ti.id + "\", " + itos(ti.getExtraInt()) + "); //" + toUTF8(ti.text);
            else
              cmd = "click(\"" + ti.id + "\"); //" + toUTF8(ti.text);
            ti.active = false;
            RenderString(ti);
            if (!ti.handleEvent(*this, GUI_LINK))
              ti.callBack(this, GUI_LINK, &ti);
            getRecorder().record(cmd);
            return 0;
          }
        }
      }
      else if (ti.active) {
        ti.active = false;
        RenderString(ti);
      }
    }
  }
  else if (iMessage == WM_LBUTTONDBLCLK) {
    list<TableInfo>::iterator tit;
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables)
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftDblClick(*this, pt.x, pt.y))
          return 0;

  }
  else if (iMessage == WM_RBUTTONDOWN) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseRightDown(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_RBUTTONUP) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseRightUp(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_MBUTTONDOWN) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseMidDown(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_MBUTTONUP) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseMidUp(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_CHAR) {
    /*list<TableInfo>::iterator tit;
    if (useTables)
      for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
        if (tit->table->character(*this, int(wParam), lParam & 0xFFFF))
          return 0;*/
  }
  else if (iMessage == WM_CTLCOLOREDIT) {
    unordered_map<HWND, InputInfo*>::iterator it = iiByHwnd.find(HWND(lParam));
    if (it != iiByHwnd.end()) {
      InputInfo &ii = *it->second;
        if (ii.bgColor != colorDefault || ii.fgColor != colorDefault) {
          if (ii.bgColor != colorDefault) {
            SetDCBrushColor(HDC(wParam), ii.bgColor);
            SetBkColor(HDC(wParam), ii.bgColor);
          }
          else {
            SetDCBrushColor(HDC(wParam), GetSysColor(COLOR_WINDOW));
            SetBkColor(HDC(wParam), GetSysColor(COLOR_WINDOW));
          }
          if (ii.fgColor != colorDefault)
            SetTextColor(HDC(wParam), ii.fgColor);
          return LRESULT(GetStockObject(DC_BRUSH));
        }
    }
    return 0;
  }
  else if (iMessage == WM_DESTROY) {
    canClear();// Ignore return value
  }

  return 0;
}

void gdioutput::TabFocus(int direction)
{
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->tabFocus(*this, direction))
        return;

  if (FocusList.empty())
    return;

  list<HWND>::iterator it=FocusList.begin();

  while(it!=FocusList.end() && *it != currentFocus.hWnd)
    ++it;

  //if (*it==CurrentFocus)
  if (it!=FocusList.end()) {
    if (direction==1){
      ++it;
      if (it==FocusList.end()) it=FocusList.begin();
      while(!IsWindowEnabled(*it) && *it != currentFocus.hWnd){
      ++it;
        if (it==FocusList.end()) it=FocusList.begin();
      }
    }
    else{
      if (it==FocusList.begin()) it=FocusList.end();

      it--;
      while(!IsWindowEnabled(*it) && *it != currentFocus.hWnd){
        if (it==FocusList.begin()) it=FocusList.end();
        it--;
      }

    }

  //  if (currentFocus.wasTabbed)
  //    Button_SetState(currentFocus.hWnd, false);

    HWND hWT = *it;
    //SetFocus(0);
    SetFocus(hWT);
    currentFocus = hWT;
    //currentFocus = *it;
    /*if (biByHwnd.find(currentFocus.hWnd) != biByHwnd.end()) {
      currentFocus.wasTabbed = true;
      Button_SetState(currentFocus.hWnd, true);
    }*/
  }
  else{
    SetFocus(currentFocus.hWnd);
    currentFocus=*FocusList.begin();

  }
}

bool gdioutput::isInputChanged(const string &exclude)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it) {
    if (it->id!=exclude) {
      if (it->changed()  && !it->ignoreCheck)
        return true;
    }
  }

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    getSelectedItem(*it);
    if (it->changed() && !it->ignoreCheck)
      return true;
  }

  for (list<ButtonInfo>::iterator it = BI.begin(); it != BI.end(); ++it) {
    bool checked = SendMessage(it->hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;
    if (it->originalState != checked)
      return true;
  }

  return false;
}

InputInfo *gdioutput::replaceSelection(const char *id, const wstring &text)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id) {
      SendMessage(it->hWnd, EM_REPLACESEL, TRUE, LPARAM(text.c_str()));
      return &*it;
    }

  return 0;
}

BaseInfo *gdioutput::setInputFocus(const string &id, bool select)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      BaseInfo *bi = SetFocus(it->hWnd)!=NULL ? &*it: 0;
      if (bi) {
        if (select)
          PostMessage(it->hWnd, EM_SETSEL, it->text.length(), 0);
      }
      return bi;
    }

  for(list<ListBoxInfo>::iterator it=LBI.begin(); it!=LBI.end();++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      return SetFocus(it->hWnd)!=NULL ? &*it: 0;
  }

  for(list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end();++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      return SetFocus(it->hWnd)!=NULL ? &*it: 0;
  }

  return 0;
}

InputInfo *gdioutput::getInputFocus()
{
  HWND hF=GetFocus();

  if (hF) {
    list<InputInfo>::iterator it;

    for(it=II.begin(); it != II.end(); ++it)
      if (it->hWnd==hF)
        return &*it;
  }
  return 0;
}

void gdioutput::enter()
{
  if (hasCommandLock())
    return;

  wstring msg;
  try {
    doEnter();
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
}

void gdioutput::doEnter() {
  if (autoCompleteInfo) {
    autoCompleteInfo->enter();
    return;
  }
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->enter(*this))
        return;

  HWND hWnd=GetFocus();

  for (list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end(); ++it)
    if (it->isDefaultButton()) {
      if (!it->handleEvent(*this, GUI_BUTTON) && it->callBack)
        it->callBack(this, GUI_BUTTON, &*it);
      return;
    }

  list<InputInfo>::iterator it;

  for(it=II.begin(); it != II.end(); ++it)
    if (it->hWnd==hWnd && (it->hasEventHandler() || it->callBack)){
      TCHAR bf[1024];
      GetWindowText(hWnd, bf, 1024);
      it->text = bf;
      if (!it->handleEvent(*this, GUI_INPUT))
        it->callBack(this, GUI_INPUT, &*it);
      return;
    }
}

bool gdioutput::upDown(int direction)
{
  wstring msg;
  try {
    return doUpDown(direction);
  }
  catch (const meosCancel&) {
    return false;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
  return false;
}


bool gdioutput::doUpDown(int direction)
{
  if (autoCompleteInfo) {
    autoCompleteInfo->upDown(direction);
    return true;
  }
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->upDown(*this, direction))
        return true;

  return false;
}

void gdioutput::escape()
{
  if (hasCommandLock())
    return;
  wstring msg;
  try {
    doEscape();
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
}


void gdioutput::doEscape()
{
  if (fullScreen) {
    PostMessage(hWndTarget, WM_CLOSE, 0,0);
  }

  if (autoCompleteInfo) {
    autoCompleteInfo.reset();
    return;
  }

  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      tit->table->escape(*this);

  for (list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end(); ++it) {
    if (it->isCancelButton() && (it->callBack || it->hasEventHandler()) ) {
      if (!it->handleEvent(*this, GUI_BUTTON))
        it->callBack(this, GUI_BUTTON, &*it);
      return;
    }
  }
}

void gdioutput::clearPage(bool autoRefresh, bool keepToolbar) {
  maxTextBlockHeight = getLineHeight();
  animationData.reset();
  lockUpDown = false;
  hasAnyTimer = false;
  enableTables();
#ifndef MEOSDB
  if (toolbar && !keepToolbar)
    toolbar->hide();
#endif

  while (!timers.empty()) {
    KillTimer(hWndTarget, (UINT_PTR)&timers.back());
    timers.back().setWnd = 0;
    timers.back().parent = 0;
    timers.pop_back();
  }

  restorePoints.clear();
  shownStrings.clear();
  onClear.clear();
  FocusList.clear();
  currentFocus = 0;
  TL.clear();
  itTL = TL.end();
  updateImageReferences();

  listDescription.clear();

  if (hWndTarget && autoRefresh)
    InvalidateRect(hWndTarget, NULL, true);

  fillDown();

  hasCleared = true;

  for (ToolList::iterator it = toolTips.begin(); it != toolTips.end(); ++it) {
    if (hWndToolTip) {
      SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM)&it->ti);
    }
  }
  toolTips.clear();

  {
    list<ButtonInfo>::iterator it;
    for (it = BI.begin(); it != BI.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
    }
    biByHwnd.clear();
    BI.clear();
  }
  {
    list<InputInfo>::iterator it;
    for (it = II.begin(); it != II.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
    }
    iiByHwnd.clear();
    II.clear();
  }

  {
    list<ListBoxInfo>::iterator it;
    for (it = LBI.begin(); it != LBI.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
      if (it->writeLock)
        hasCleared = true;
    }
    lbiByHwnd.clear();
    LBI.clear();
  }

  while (!Tables.empty()) {
    auto t = Tables.front().table;
    Tables.pop_front();
    t->hide(*this);
  }

  DataInfo.clear();
  FocusList.clear();
  Events.clear();

  Rectangles.clear();

  MaxX = scaleLength(60);
  MaxY = scaleLength(100);

  CurrentX = scaleLength(40);
  CurrentY = scaleLength(START_YP);
  SX = CurrentX;
  SY = CurrentY;
  OffsetX = 0;
  OffsetY = 0;

  renderOptimize = true;

  backgroundColor1 = -1;
  backgroundColor2 = -1;
  foregroundColor = -1;
  backgroundImage = -1;


  setRestorePoint();

  if (autoRefresh)
    updateScrollbars();

  auto clsCopy = postClear;
  for (auto& clr : clsCopy) {
    try {
      clr.makeEvent(*this, GUI_POSTCLEAR);
    }
    catch (const meosCancel&) {
    }
    catch (meosException& ex) {
      if (isTestMode)
        throw ex;
      wstring msg = ex.wwhat();
      alert(msg);
    }
    catch (const std::exception& ex) {
      if (isTestMode)
        throw ex;
      string msg(ex.what());
      alert(msg);
    }
  }
  postClear.clear();
  manualUpdate = !autoRefresh;
}

void gdioutput::updateImageReferences() {
  if (imageReferences.size() > 0) {
    imageReferences.clear();
    for (auto& ti : TL) {
      if ((ti.format & 0xFF) == textImage) {
        imageReferences.push_back(&ti);
      }
    }
  }
}

void gdioutput::getWindowText(HWND hWnd, wstring &text)
{
  TCHAR bf[1024];
  TCHAR *bptr=bf;

  int len=GetWindowTextLength(hWnd);

  if (len>1023)
    bptr=new TCHAR[len+1];

  GetWindowText(hWnd, bptr, len+1);
  text=bptr;

  if (len>1023)
    delete[] bptr;
}

BaseInfo& gdioutput::getBaseInfo(const char* id, int requireExtraMatch) const {
  for (auto& ii : II) {
    if (ii.id == id && ii.matchExtra(requireExtraMatch)) {
      return const_cast<InputInfo&>(ii);
    }
  }

  for (auto& lbi : LBI) {
    if (lbi.id == id && lbi.matchExtra(requireExtraMatch)) {
      return const_cast<ListBoxInfo&>(lbi);
    }
  }

  for (auto& bi : BI) {
    if (bi.id == id && bi.matchExtra(requireExtraMatch)) {
      return const_cast<ButtonInfo&>(bi);
    }
  }

  for (auto& tl : TL) {
    if (tl.id == id && tl.matchExtra(requireExtraMatch)) {
      return const_cast<TextInfo&>(tl);
    }
  }

  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
}

const wstring &gdioutput::getText(const char *id, bool acceptMissing, int requireExtraMatch) const {
  TCHAR bf[1024];
  TCHAR *bptr=bf;

  for(list<InputInfo>::const_iterator it=II.begin();
                                  it != II.end(); ++it){
    if (it->id==id && it->matchExtra(requireExtraMatch)){
      int len=GetWindowTextLength(it->hWnd);

      if (len>1023)
        bptr=new TCHAR[len+1];

      GetWindowText(it->hWnd, bptr, len+1);
      const_cast<wstring&>(it->text)=bptr;

      if (len>1023)
        delete[] bptr;

      return it->text;
    }
  }

  for(list<ListBoxInfo>::const_iterator it=LBI.begin();
                                  it != LBI.end(); ++it){
    if (it->id==id && it->IsCombo && it->matchExtra(requireExtraMatch)){
      if (!it->writeLock) {
        GetWindowText(it->hWnd, bf, 1024);
        const_cast<wstring&>(it->text)=bf;
      }
      return it->text;
    }
  }

  for(list<TextInfo>::const_iterator it=TL.begin();
                                  it != TL.end(); ++it){
    if (it->id==id && it->matchExtra(requireExtraMatch)) {
      return it->text;
    }
  }

#ifdef _DEBUG
  if (!acceptMissing) {
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  }
#endif
  return _EmptyWString;
}

bool gdioutput::hasWidget(const string &id) const
{
  for(list<InputInfo>::const_iterator it=II.begin();
                                  it != II.end(); ++it){
    if (it->id==id)
      return true;
  }

  for(list<ListBoxInfo>::const_iterator it=LBI.begin();
                                  it != LBI.end(); ++it){
    if (it->id==id)
      return true;
  }

  for(list<ButtonInfo>::const_iterator it=BI.begin();
                                  it != BI.end(); ++it){
    if (it->id==id)
      return true;
  }

  for (auto &tl : TL) {
    if (tl.id == id)
      return true;
  }

  return false;
}

int gdioutput::getTextNo(const char *id, bool acceptMissing) const
{
  const wstring &t = getText(id, acceptMissing);
  return _wtoi(t.c_str());
}

BaseInfo *gdioutput::setTextTranslate(const char *id,
                                      const wstring &text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}

BaseInfo *gdioutput::setTextTranslate(const string &id,
                                      const wstring &text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}

BaseInfo *gdioutput::setTextTranslate(const char *id,
                                      const wchar_t *text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}



BaseInfo *gdioutput::setText(const char *id, int number, bool Update)
{
  return setText(id, itow(number), Update);
}

BaseInfo *gdioutput::setTextZeroBlank(const char *id, int number, bool Update)
{
  if (number!=0)
    return setText(id, number, Update);
  else
    return setText(id, L"", Update);
}


BaseInfo *gdioutput::setText(const char *id, const wstring &text, bool update, int requireExtraMatch, bool updateOriginal)
{
  for (auto it = II.begin(); it != II.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      bool oldWR = it->writeLock;
      it->writeLock = true;
      SetWindowText(it->hWnd, text.c_str());
      it->writeLock = oldWR;
      it->text = text;
      it->synchData();
      if (updateOriginal)
        it->original = text;
      it->focusText = text;
      return &*it;
    }
  }

  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id && it->IsCombo && it->matchExtra(requireExtraMatch)) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;
      if (updateOriginal)
        it->original = text;
      return &*it;
    }
  }

  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;
      return &*it;
    }
  }

  for (auto it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      RECT rc = it->textRect;

      it->text = text;
      calcStringSize(*it);

      rc.right = max(it->textRect.right, rc.right);
      rc.bottom = max(it->textRect.bottom, rc.bottom);

      bool changed = updatePos(0, 0, it->textRect.right, it->textRect.bottom);

      if (update && hWndTarget) {
        if (changed)
          InvalidateRect(hWndTarget, 0, true);
        else
          InvalidateRect(hWndTarget, &rc, true);
      }
      return &*it;
    }
  }
  return nullptr;
}

bool gdioutput::insertText(const string &id, const wstring &text)
{
  for (list<InputInfo>::iterator it = II.begin();
    it != II.end(); ++it) {
    if (it->id == id) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;

      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_INPUT);
      else if (it->callBack)
        it->callBack(this, GUI_INPUT, &*it);

      return true;
    }
  }
  return false;
}

void gdioutput::setData(const string &id, DWORD data)
{
  void *pd = (void *)(size_t(data));
  setData(id, pd);
}

void gdioutput::setData(const string &id, void *data)
{
  list<DataStore>::iterator it;
  for(it=DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id==id){
      it->data = data;
      return;
    }
  }

  DataStore ds;
  ds.id=id;
  ds.data=data;

  DataInfo.push_front(ds);
  return;
}

bool gdioutput::getData(const string &id, DWORD &data) const
{
  list<DataStore>::const_iterator it;
  for(it=DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id==id){
      data=DWORD(size_t(it->data));
      return true;
    }
  }

  data=0;
  return false;
}

void gdioutput::setData(const string &id, const string &data) {
  for (auto &it : DataInfo) {
    if (it.id == id) {
      it.sdata = data;
      return;
    }
  }

  DataStore ds;
  ds.id = id;
  ds.sdata = data;
  DataInfo.push_front(ds);
  return;
}

bool gdioutput::getData(const string &id, string &out) const {
  for (auto &it : DataInfo) {
    if (it.id == id) {
      out = it.sdata;
      return true;
    }
  }
  out.clear();
  return false;
}

void *gdioutput::getData(const string &id) const {
  list<DataStore>::const_iterator it;
  for (it = DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id == id){
      return it->data;
    }
  }

  throw meosException("Data X not found#" + id);
}

bool gdioutput::hasData(const char *id) const {
  DWORD dummy;
  return getData(id, dummy);
}

bool gdioutput::updatePosTight(int x, int y, int width, int height, int marginx, int marginy) {
  int ox = MaxX;
  int oy = MaxY;

  MaxX = max(x + width, MaxX);
  MaxY = max(y + height, MaxY);
  bool changed = (ox != MaxX || oy != MaxY);

  if (changed && hWndTarget && !manualUpdate) {
    RECT rc;
    if (ox == MaxX) {
      rc.top = oy - CurrentY - 5;
      rc.bottom = MaxY - CurrentY + scaleLength(50);
      rc.right = 10000;
      rc.left = 0;
      InvalidateRect(hWndTarget, &rc, true);
    }
    else {
      InvalidateRect(hWndTarget, 0, true);
    }
    GetClientRect(hWndTarget, &rc);

    if (MaxX > rc.right || MaxY > rc.bottom) //Update scrollbars
      SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
  }

  if (flowDirection == FlowDirection::Down) {
    CurrentY = max(y + height + marginy, CurrentY);
  }
  else if (flowDirection == FlowDirection::Right) {
    CurrentX = max(x + width + marginx, CurrentX);
  }
  return changed;
}

bool gdioutput::updatePos(int x, int y, int width, int height) {
  return updatePosTight(x, y, width, height, 0, 0);
}

void gdioutput::adjustDimension(int width, int height)
{
  int ox = MaxX;
  int oy = MaxY;

  MaxX = width;
  MaxY = height;

  if  ((ox!=MaxX || oy!=MaxY) && hWndTarget && !manualUpdate) {
    RECT rc;
    if (ox == MaxX) {
      rc.top = oy - CurrentY - 5;
      rc.bottom = MaxY - CurrentY + scaleLength(50);
      rc.right = 10000;
      rc.left = 0;
      InvalidateRect(hWndTarget, &rc, true);
    }
    else {
      InvalidateRect(hWndTarget, 0, true);
    }
    GetClientRect(hWndTarget, &rc);

    if (MaxX>rc.right || MaxY>rc.bottom) //Update scrollbars
      SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));

  }
}

// Alert from main thread (via callback)
void gdioutput::delayAlert(const wstring& msg) {
  if (!delayedAlert.empty())
    delayedAlert += L", ";
  if (delayedAlert.length() > 1000)
    delayedAlert = L"";

  delayedAlert += lang.tl(msg);
  PostMessage(hWndAppMain, WM_USER + 6, 0, LPARAM(this));
}

wstring gdioutput::getDelayedAlert() {
  wstring out = L"#" + delayedAlert;
  delayedAlert.clear();
  return out;
}

void gdioutput::alert(const string &msg) const
{
  alert(widen(msg));
}

void gdioutput::alert(const wstring &msg) const
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "ok")
        return;
    }
    throw meosException(msg + L"-- ok");
  }

  HWND hFlt = getToolbarWindow();
  if (hasToolbar()) {
    EnableWindow(hFlt, false);
  }
  refreshFast();
  SetForegroundWindow(hWndAppMain);
  setCommandLock();
  try {
    MessageBoxW(hWndAppMain, lang.tl(msg).c_str(), L"MeOS", MB_OK|MB_ICONINFORMATION);
    if (hasToolbar()) {
      EnableWindow(hFlt, true);
    }
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }
}

struct AskDialogInfo {
  wstring btnYes;
  wstring btnNo;
  wstring btnCancel;
  wstring message;
  wstring title;
  int icon = 0;
};

static AskDialogInfo* askDlgPtr = nullptr;

/*INT_PTR CALLBACK askDialogCB(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG: {
      const AskDialogInfo& info = *reinterpret_cast<AskDialogInfo*>(lParam);
      SetWindowText(GetDlgItem(hDlg, IDOK), info.btnYes.c_str());
      SetWindowText(GetDlgItem(hDlg, IDCANCEL), info.btnNo.c_str());
      SetWindowText(GetDlgItem(hDlg, IDC_MESSAGETEXT), info.message.c_str());

      return 1;
    }
    break;

    case WM_COMMAND:
      switch (wParam) {
      case IDOK:
        EndDialog(hDlg, true);
        return 0;
      case IDCANCEL:
        EndDialog(hDlg, false);
        return 0;
      }
     
    break; 
  }
  return 0;// DefDlgProc(hDlg, iMsg, wParam, lParam);
}
*/
LRESULT WINAPI hookFn(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HCBT_ACTIVATE) {
    if (askDlgPtr) {
      int movDiff = 0;
      // Modify standard MessageBox button texts    
      auto updateBtnTextPosSize = [&movDiff, wParam](int id, const wstring &text) {
        HWND btn = GetDlgItem((HWND)wParam, id);
        if (text != L"@")
          SetWindowText(btn, text.c_str());
        SIZE sz;
        RECT rc;
        GetWindowRect(btn, &rc);
        
        if (text != L"@")
          Button_GetIdealSize(btn, &sz);
        else {
          sz.cx = rc.right - rc.left;
          sz.cy = rc.bottom - rc.top;
        }

        POINT pt = { rc.left, rc.top };
        ScreenToClient((HWND)wParam, &pt);
        int wdActual = rc.right - rc.left;
        if (wdActual < sz.cx) {
          movDiff += sz.cx - wdActual;
          SetWindowPos(btn, nullptr, pt.x - movDiff, pt.y, sz.cx, rc.bottom - rc.top, SWP_NOZORDER);
        }
        else if (movDiff > 0) {
          SetWindowPos(btn, nullptr, pt.x - movDiff, pt.y, sz.cx, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOSIZE);
        }
      };

      if (!askDlgPtr->btnCancel.empty()) 
        updateBtnTextPosSize(IDCANCEL, askDlgPtr->btnCancel);

      if (!askDlgPtr->btnNo.empty()) 
        updateBtnTextPosSize(IDNO, askDlgPtr->btnNo);
      
      if (!askDlgPtr->btnYes.empty())
        updateBtnTextPosSize(IDYES, askDlgPtr->btnYes);

      askDlgPtr = nullptr;
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool gdioutput::ask(const wstring &s, const char* yesButton, const char* noButton) {
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "yes")
        return true;
      else if (ans == "no")
        return false;
    }
    throw meosException(s + L"--yes/no");
  }

  setCommandLock();
  SetForegroundWindow(hWndAppMain);
  bool yes;
  HHOOK hook = nullptr;
  try {
    if (yesButton != nullptr || noButton != nullptr) {
      HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
      AskDialogInfo info;
      if (!yesButton)
        yesButton = "#@";
      if (!noButton)
        noButton = "#@";
      info.btnYes = lang.tl(yesButton);
      info.btnNo = lang.tl(noButton);
      
      askDlgPtr = &info;
      hook = SetWindowsHookEx(WH_CBT, hookFn, hInst, GetCurrentThreadId());
      yes = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNO | MB_ICONQUESTION) == IDYES;
      askDlgPtr = nullptr;
      UnhookWindowsHookEx(hook);
      hook = nullptr;
      //yes = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_ASK), hWndAppMain, askDialogCB, LPARAM((void *) &info));
    }
    else {
      yes = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNO | MB_ICONQUESTION) == IDYES;
    }
    liftCommandLock();
  }
  catch (...) {
    if (hook)
      UnhookWindowsHookEx(hook);

    liftCommandLock();
    throw;
  }

  return yes;
}

gdioutput::AskAnswer gdioutput::askCancel(const wstring &s, const char* yesButton, const char* noButton) {
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "cancel")
        return AskAnswer::AnswerCancel;
      else if (ans == "yes")
        return AskAnswer::AnswerYes;
      else if (ans == "no")
        return AskAnswer::AnswerNo;
    }
    throw meosException(s + L"--yes/no/cancel");
  }

  int a;
  HHOOK hook = nullptr;
  setCommandLock();
  try {
    SetForegroundWindow(hWndAppMain);
    if (yesButton != nullptr || noButton != nullptr) {
      HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
      AskDialogInfo info;
      if (!yesButton)
        yesButton = "#@";
      if (!noButton)
        noButton = "#@";
      info.btnYes = lang.tl(yesButton);
      info.btnNo = lang.tl(noButton);

      askDlgPtr = &info;
      hook = SetWindowsHookEx(WH_CBT, hookFn, hInst, GetCurrentThreadId());
      a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNOCANCEL | MB_ICONQUESTION);
      askDlgPtr = nullptr;
      UnhookWindowsHookEx(hook);
      hook = nullptr;
    }
    else {
      a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNOCANCEL | MB_ICONQUESTION);
    }
    liftCommandLock();
  }
  catch (...) {
    if (hook)
      UnhookWindowsHookEx(hook);

    liftCommandLock();
    throw;
  }

  if (a == IDYES)
    return AskAnswer::AnswerYes;
  else if (a == IDNO)
    return AskAnswer::AnswerNo;
  else
    return AskAnswer::AnswerCancel;
}

gdioutput::AskAnswer gdioutput::askOkCancel(const wstring& s)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "cancel")
        return AskAnswer::AnswerCancel;
      else if (ans == "ok")
        return AskAnswer::AnswerOK;
    }
    throw meosException(s + L"--ok/cancel");
  }

  setCommandLock();
  SetForegroundWindow(hWndAppMain);
  int a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_OKCANCEL | MB_ICONINFORMATION);
  liftCommandLock();
  if (a == IDOK)
    return AskAnswer::AnswerOK;
  else
    return AskAnswer::AnswerCancel;
}

void gdioutput::setTabStops(const string& name, int t1, int t2) {
  getInputDimension(0);
  double relTextScale = scale / guiMeasure->avgCharWidth;

  DWORD ptr[2];
  int n = 1;
  //LONG bu=GetDialogBaseUnits();
  //int baseunitX=LOWORD(bu);
  //array[0]=int(t1 * 4.2 * scale) / baseunitX ;
  //array[1]=int(t2 * 4.2 * scale) / baseunitX ;
  ptr[0] = int(t1 * relTextScale * 6.4 * 4.2 / 8.0);
  ptr[1] = int(t2 * relTextScale * 6.4 * 4.2 / 8.0);

  int lastTabStop = 0;
  if (t2 > 0) {
    n = 2;
    lastTabStop = t2;
  }
  else {
    lastTabStop = t1;
  }

  list<ListBoxInfo>::iterator it;
  for (it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == name) {
      if (!it->IsCombo) {
        SendMessage(it->hWnd, LB_SETTABSTOPS, n, LPARAM(ptr));
        it->lastTabStop = lastTabStop;
      }
      return;
    }
  }
}

void gdioutput::setInputStatus(const char *id, bool status, bool acceptMissing, int matchExtra) {
  bool hit = false;
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      hit = true;
    }
  for(list<ListBoxInfo>::iterator it=LBI.begin(); it != LBI.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      hit = true;
    }
  for(list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      if (it->isCheckbox) {
        string tid = "T" + it->id;
        for(list<TextInfo>::iterator tit=TL.begin(); tit != TL.end(); ++tit){
          if (tit->id == tid) {
            enableCheckBoxLink(*tit, status);
            break;
          }
        }
      }

      hit = true;
      if (status==false) {
        it->storedFlags |= it->flags;
        it->flags = 0; //Remove default status etc.
      }
      else {
        // Restore flags
        it->flags |= it->storedFlags;
      }
    }

  if (acceptMissing)
    return;
#ifdef _DEBUG
  if (!hit) {
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  }
#endif
}

void gdioutput::refresh() const {
#ifdef DEBUGRENDER
  OutputDebugString("### Full refresh\n");
#endif
  if (hWndTarget) {
    updateScrollbars();
    InvalidateRect(hWndTarget, NULL, true);
    UpdateWindow(hWndTarget);
  }
  screenXYToString.clear();
  stringToScreenXY.clear();
}

void gdioutput::refreshFast() const {
#ifdef DEBUGRENDER
  OutputDebugString("Fast refresh\n");
#endif
  if (hWndTarget) {
    InvalidateRect(hWndTarget, NULL, true);
    UpdateWindow(hWndTarget);
  }
  screenXYToString.clear();
  stringToScreenXY.clear();
}


void gdioutput::takeShownStringsSnapshot() {
#ifdef DEBUGRENDER
  OutputDebugString("** Take snapshot\n");
#endif

  screenXYToString.clear();
  stringToScreenXY.clear();
  snapshotMaxXY.first = MaxX;
  snapshotMaxXY.second = MaxY - OffsetY;
#ifdef DEBUGRENDER
  OutputDebugString(("ymax:" + itos(MaxY-OffsetY) + "\n").c_str());
#endif
  for (size_t k = 0; k < shownStrings.size(); k++) {
    if (shownStrings[k]->hasTimer)
      continue; //Ignore
    int x = shownStrings[k]->xp - OffsetX;
    int y = shownStrings[k]->yp - OffsetY;
    const wstring &str = shownStrings[k]->text;
#ifdef DEBUGRENDER
    //OutputDebugString((itos(k) + ":" + itos(shownStrings[k]->xp) + "," + itos(shownStrings[k]->yp) + "," + str + "\n").c_str());
#endif
    screenXYToString.insert(make_pair(make_pair(x, y), ScreenStringInfo(shownStrings[k]->textRect, str)));
    if (stringToScreenXY.count(str) == 0)
      stringToScreenXY.insert(make_pair(str,make_pair(x, y)));
  }

  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY;
  int BoundYdown = OffsetY+rc.bottom;
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    if (it->rc.top <= BoundYdown && it->rc.bottom >= BoundYup) {
      wstring r = L"[R]";
      RECT rect_rc = it->rc;
      OffsetRect(&rect_rc, -OffsetX, -OffsetY);
      screenXYToString.insert(make_pair(make_pair(rect_rc.left, rect_rc.top), ScreenStringInfo(rect_rc, r)));
    }
  }
}

void updateScrollInfo(HWND hWnd, gdioutput &gdi, int nHeight, int nWidth);

void gdioutput::refreshSmartFromSnapshot(bool allowMoveOffset) {
#ifdef DEBUGRENDER
  OutputDebugString("Smart refresh\n");
#endif

  RECT clientRC;
  GetClientRect(hWndTarget, &clientRC);

  updateStringPosCache();

  vector<int> changedStrings;
  bool updateScroll = false;
  if (allowMoveOffset) {
    map< pair<int, int>, int> offsetCount;
    int misses = 0, hits = 0;
    for (size_t k = 0; k < shownStrings.size(); k++) {
      if (shownStrings[k]->hasTimer)
        continue; //Ignore
      int x = shownStrings[k]->xp - OffsetX;
      int y = shownStrings[k]->yp - OffsetY;
      const wstring &str = shownStrings[k]->text;
      map<wstring, pair<int,int> >::const_iterator found = stringToScreenXY.find(str);
      if (found != stringToScreenXY.end()) {
        hits++;
        int ox = found->second.first - x;
        int oy = found->second.second - y;
        ++offsetCount[make_pair(ox, oy)];
        if (hits > 30)
          break;
      }
      else {
        misses++;
        if (misses > 20)
          break;
      }
    }

    // Choose dominating offset, if dominating enough
    pair<int, int> offset(0,0);
    int maxVal = 10; // Require at least 10 hits
    for(map< pair<int, int>, int>::iterator it = offsetCount.begin(); it != offsetCount.end(); ++it) {
      if (it->second > maxVal) {
        maxVal = it->second;
        offset = it->first;
      }
    }

    int maxOffsetY=max<int>(getPageY()-clientRC.bottom, 0);
    int maxOffsetX=max<int>(getPageX()-clientRC.right, 0);
    int noy = OffsetY - offset.second;
    int nox = OffsetX - offset.first;
    if ((offset.first != 0 && nox>0 && nox<maxOffsetX) || (offset.second != 0 && noy>0 && noy<maxOffsetY) ) {
      #ifdef DEBUGRENDER
        OutputDebugString(("Change offset: " + itos(offset.first) + "," + itos(offset.second) + "\n").c_str());
      #endif
      OffsetX -= offset.first;
      OffsetY -= offset.second;
      autoPos -= offset.second;

      if (offset.second != 0) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_POS;
        si.nPos   = OffsetY;
        SetScrollInfo(hWndTarget, SB_VERT, &si, false);
        updateScroll = true;
      }

      if (offset.first != 0) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_POS;
        si.nPos   = OffsetX;
        SetScrollInfo(hWndTarget, SB_HORZ, &si, false);
        updateScroll = true;
      }

      updateStringPosCache();
    }
  }
#ifdef DEBUGRENDER
  OutputDebugString(("* ymax:" + itos(MaxY-OffsetY) + "\n").c_str());
#endif

  RECT invalidRect;
  invalidRect.top = 1000000;
  invalidRect.left = 1000000;
  invalidRect.right = -1;
  invalidRect.bottom = -1;
  bool invalid = false;

  for (size_t k = 0; k < shownStrings.size(); k++) {
    if (shownStrings[k]->hasTimer)
      continue; //Ignore

    int x = shownStrings[k]->xp - OffsetX;
    int y = shownStrings[k]->yp - OffsetY;
    const wstring &str = shownStrings[k]->text;
#ifdef DEBUGRENDER
    //OutputDebugString((itos(k) + ":" + itos(shownStrings[k]->xp) + "," + itos(shownStrings[k]->yp) + "," + str + "\n").c_str());
#endif
    map<pair<int, int>, ScreenStringInfo>::iterator res = screenXYToString.find(make_pair(x,y));
    if (res != screenXYToString.end()) {
      res->second.reached = true;
      if (str != res->second.str) {
        if (res->second.rc.bottom >= 0 && res->second.rc.top <= clientRC.bottom) {
          changedStrings.push_back(k);
          invalidRect.top = min(invalidRect.top, res->second.rc.top);
          invalidRect.bottom = max(invalidRect.bottom, res->second.rc.bottom);
          invalidRect.left = min(invalidRect.left, res->second.rc.left);
          invalidRect.right = max(invalidRect.right, res->second.rc.right);
        }
      }
    }
    else
      changedStrings.push_back(k);
  }

  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY;
  int BoundYdown = OffsetY+rc.bottom;
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    if (it->rc.top <= BoundYdown && it->rc.bottom >= BoundYup) {
      RECT rect_rc = it->rc;
      OffsetRect(&rect_rc, -OffsetX, -OffsetY);

      map<pair<int, int>, ScreenStringInfo>::iterator res = screenXYToString.find(make_pair(rect_rc.left, rect_rc.top));
      bool add = false;
      if (res != screenXYToString.end()) {
        res->second.reached = true;
        if (!EqualRect(&rect_rc, &res->second.rc)) {
          add = true;
          invalidRect.top = min(invalidRect.top, res->second.rc.top);
          invalidRect.bottom = max(invalidRect.bottom, res->second.rc.bottom);
          invalidRect.left = min(invalidRect.left, res->second.rc.left);
          invalidRect.right = max(invalidRect.right, res->second.rc.right);
        }
      }
      else
        add = true;

      if (add) {
        invalid = true;
        invalidRect.top = min(invalidRect.top, rect_rc.top);
        invalidRect.bottom = max(invalidRect.bottom, rect_rc.bottom);
        invalidRect.left = min(invalidRect.left, rect_rc.left);
        invalidRect.right = max(invalidRect.right, rect_rc.right);
      }
    }
  }

  for (map<pair<int, int>, ScreenStringInfo>::iterator it = screenXYToString.begin(); it != screenXYToString.end(); ++it) {
    if (!it->second.reached) {
      invalid = true;
      invalidRect.top = min(invalidRect.top, it->second.rc.top);
      invalidRect.bottom = max(invalidRect.bottom, it->second.rc.bottom);
      invalidRect.left = min(invalidRect.left, it->second.rc.left);
      invalidRect.right = max(invalidRect.right, it->second.rc.right);
    }
  }

  screenXYToString.clear();
  stringToScreenXY.clear();

  if (snapshotMaxXY.second != MaxY - OffsetY) {
    // We added (or removed) a row. Add result to list is typical case.
    int currentMaxP = (MaxY - OffsetY);
    int oldMaxP = snapshotMaxXY.second;
    bool bottomVisible = ((currentMaxP < clientRC.bottom + 15) && currentMaxP > -15) ||
                           ((oldMaxP < clientRC.bottom + 15) && oldMaxP > -15);
    if (bottomVisible && !highContrast && oldMaxP != currentMaxP) {
      invalid = true;
      invalidRect.top = min<int>(invalidRect.top, oldMaxP-15);
      invalidRect.top = min<int>(invalidRect.top, currentMaxP-15);

      invalidRect.bottom = max<int>(invalidRect.bottom, oldMaxP+15);
      invalidRect.bottom = max<int>(invalidRect.bottom, currentMaxP+15);

      invalidRect.left = 0;
      invalidRect.right = clientRC.right;
      #ifdef DEBUGRENDER
        OutputDebugString("Extend Y\n");
      #endif
    }
    updateScroll = true;
  }

  if (snapshotMaxXY.first != MaxX) {
    // This almost never happens
    invalidRect = clientRC;
    invalid = true;
    updateScroll;
  }

  if (updateScroll) {
    bool hc = highContrast;
    highContrast = false;
    updateScrollInfo(hWndTarget, *this, clientRC.bottom, clientRC.right); // No throw
    highContrast = hc;
  }
  if (changedStrings.empty() && !invalid) {
    #ifdef DEBUGRENDER
      //breakRender = true;
      OutputDebugString("*** NO CHANGE\n");
    #endif

    return;
  }

  for (size_t k = 0; k< changedStrings.size(); k++) {
    TextInfo &ti = *shownStrings[changedStrings[k]];
    invalidRect.top = min(invalidRect.top, ti.textRect.top);
    invalidRect.bottom = max(invalidRect.bottom, ti.textRect.bottom);
    invalidRect.left = min(invalidRect.left, ti.textRect.left);
    invalidRect.right = max(invalidRect.right, ti.textRect.right);
  }


  if (invalidRect.bottom<0 || invalidRect.right < 0
    || invalidRect.top > clientRC.bottom || invalidRect.left > clientRC.right) {

    #ifdef DEBUGRENDER
      //breakRender = true;
      OutputDebugString("*** EMPTY CHANGE\n");
    #endif

    return;
  }

  if (hWndTarget) {
    //InvalidateRect(hWndTarget, &invalidRect, true);
    //UpdateWindow(hWndTarget);
    HDC hDC = GetDC(hWndTarget);
    IntersectClipRect(hDC, invalidRect.left, invalidRect.top, invalidRect.right, invalidRect.bottom);
    //debugDrawColor = RGB((30*counterRender)%256,0,0);
    draw(hDC, clientRC, invalidRect);
    //debugDrawColor = 0;

    ReleaseDC(hWndTarget, hDC);
  }
}

void gdioutput::removeString(string id) {
  int cnt = 0;
  for (auto it = TL.begin(); it != TL.end(); ++it, ++cnt) {
    if (it->id == id) {
      InvalidateRect(hWndTarget, &it->textRect, true);
      TL.erase(it);
      itTL = TL.end();
      shownStrings.clear();

      updateImageReferences();

      // Update restorepoints
      for (auto& rp : restorePoints) {
        if (rp.second.nTL > cnt)
          rp.second.nTL--;
      }
      return;
    }
  }
}

bool gdioutput::selectFirstItem(const string& id) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it)
    if (it->id == id) {
      bool ret;
      if (it->IsCombo)
        ret = SendMessage(it->hWnd, CB_SETCURSEL, 0, 0) >= 0;
      else
        ret = SendMessage(it->hWnd, LB_SETCURSEL, 0, 0) >= 0;
      getSelectedItem(*it);
      it->original = it->text;
      it->originalIdx = it->data;
    }

  return false;
}

void gdioutput::setWindowTitle(const wstring &title)
{
  if (title.length()>0) {
    wstring titlew = title + makeDash(L" - MeOS");
    SetWindowText(hWndAppMain, titlew.c_str());
  }
  else SetWindowText(hWndAppMain, L"MeOS");
}

void gdioutput::setWaitCursor(bool wait)
{
  if (wait)
    SetCursor(LoadCursor(NULL, IDC_WAIT));
  else
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

struct FadeInfo
{
  TextInfo ti;
  uint64_t Start;
  uint64_t End;
  HWND hWnd;
  COLORREF StartC;
  COLORREF EndC;
};

void TextFader(void *f)
{
  FadeInfo *fi=(FadeInfo *)f;
  HDC hDC=GetDC(fi->hWnd);

  SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hDC, TRANSPARENT);

  double p=0;

  double r1=GetRValue(fi->StartC);
  double g1=GetGValue(fi->StartC);
  double b1=GetBValue(fi->StartC);

  double r2=GetRValue(fi->EndC);
  double g2=GetGValue(fi->EndC);
  double b2=GetBValue(fi->EndC);

  while(p<1)
  {
    p=double(GetTickCount64()-fi->Start)/double(fi->End-fi->Start);

    if (p>1) p=1;

    p=1-(p-1)*(p-1);

    int red=int((1-p)*r1+(p)*r2);
    int green=int((1-p)*g1+(p)*g2);
    int blue=int((1-p)*b1+(p)*b2);
    //int green=int((p-1)*GetGValue(fi->StartC)+(p)*GetGValue(fi->EndC));
    //int blue=int((p-1)*GetBValue(fi->StartC)+(p)*GetBValue(fi->EndC));

    SetTextColor(hDC, RGB(red, green, blue));
    TextOut(hDC, fi->ti.xp, fi->ti.yp, fi->ti.text.c_str(), fi->ti.text.length());
    Sleep(30);
    //char bf[10];
    //fi->ti.text=fi->ti.text+itoa(red, bf, 16);
  }

  ReleaseDC(fi->hWnd, hDC);
  delete fi;
}

void gdioutput::fadeOut(string Id, int ms)
{
  list<TextInfo>::iterator it;
  for(it=TL.begin(); it != TL.end(); ++it){
    if (it->id==Id){
      FadeInfo *fi=new FadeInfo;
      fi->Start=GetTickCount64();
      fi->End=fi->Start+ms;
      fi->ti=*it;
      fi->StartC=RGB(0, 0, 0);
      fi->EndC=GetSysColor(COLOR_WINDOW);
      fi->hWnd=hWndTarget;
      _beginthread(TextFader, 0, fi);
      TL.erase(it);
      return;
    }
  }
}

void gdioutput::RenderString(TextInfo &ti, HDC hDC) {
  if (skipTextRender(ti.format))
    return;

  if (ti.hasTimer && ti.xp == 0)
    return;

  HDC hThis=0;

  if (!hDC){
    assert(hWndTarget!=0);
    hDC=hThis=GetDC(hWndTarget);
  }
  RECT rc;
  if ((ti.format & absolutePosition) == 0) {
    rc.left = ti.xp - OffsetX;
    rc.top = ti.yp - OffsetY;
  }
  else {
    rc.left = ti.xp;
    rc.top = ti.yp;
  }
  rc.right = rc.left;
  rc.bottom = rc.top;

  formatString(ti, hDC);
  int format=ti.format&0xFF;
  if (format == textImage) {
    // Image
    int id = _wtoi(ti.text.c_str());
    bool fixedRect = false;
    int h = 16, w = 16;
    bool setWH = false;
    if (id > 0) {
      image.loadImage(id, Image::ImageMethod::Default);
      w = image.getWidth(id);
      h = image.getHeight(id);
      setWH = true;
      image.drawImage(id, Image::ImageMethod::Default, hDC, rc.left, rc.top, w, h);
    }
    else if (ti.text.size()>1) {      
      if (ti.text[0] == 'S') { // Icon
        setWH = true;
        w = getLineHeight();
        h = getLineHeight();
      }
      else if (ti.text[0] == 'L') {
        fixedRect = true;
        uint64_t imgId = _wcstoui64(ti.text.c_str() + 1, nullptr, 10);
        if (imgId > 0) {
          w = ti.textRect.right - ti.textRect.left;
          h = ti.textRect.bottom - ti.textRect.top;
          image.drawImage(imgId, Image::ImageMethod::Default, hDC, rc.left, rc.top, w, h);
        }
      }
    }
    if (!fixedRect) {
      ti.textRect.left = rc.left;
      ti.textRect.top = rc.top;
      if (setWH) {
        ti.textRect.right = rc.left + w + 5;
        ti.textRect.bottom = rc.bottom + h + 5;
      }
    }
  }
  else if (format != 10 && (breakLines&ti.format) == 0) {
    if (ti.xlimit == 0) {
      if (ti.format&textRight) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT | DT_NOPREFIX);
        int dx = rc.right - rc.left;
        ti.realWidth = dx;
        rc.right -= dx;
        rc.left -= dx;
        ti.textRect = rc;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_RIGHT | DT_NOCLIP | DT_NOPREFIX);
      }
      else if (ti.format&textCenter) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | DT_CALCRECT | DT_NOPREFIX);
        int dx = rc.right - rc.left;
        ti.realWidth = dx;
        rc.right -= dx / 2;
        rc.left -= dx / 2;
        ti.textRect = rc;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | DT_NOCLIP | DT_NOPREFIX);
      }
      else {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);
        ti.textRect = rc;
        ti.realWidth = rc.right - rc.left;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
      }
    }
    else {
      int flags = DT_NOPREFIX;
      if (ti.format & textLimitEllipsis)
        flags = DT_END_ELLIPSIS;

      DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT | flags);
      ti.realWidth = rc.right - rc.left;
      if (ti.format&textRight) {
        rc.right = rc.left + ti.xlimit - (rc.bottom - rc.top) / 2;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_RIGHT | flags);
      }
      else if (ti.format&textCenter) {
        rc.right = rc.left + ti.xlimit - (rc.bottom - rc.top) / 2;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | flags);
      }
      else {
        rc.right = rc.left + ti.xlimit;
        DrawText(hDC, ti.text.c_str(), -1, &rc, DT_LEFT | flags);
      }
      ti.textRect = rc;
    }
  }
  else {
    memset(&rc, 0, sizeof(rc));
    int width =  scaleLength( (breakLines&ti.format) ? ti.xlimit : 450 );
    rc.right = width;
    int dx = format != 10 ? 0 : scaleLength(20);
    ti.realWidth = width + dx;
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    ti.textRect=rc;
    ti.textRect.right+=ti.xp+dx;
    ti.textRect.left+=ti.xp;
    ti.textRect.top+=ti.yp;
    ti.textRect.bottom+=ti.yp+dx;

    if (format == 10) {
      DWORD c = colorLightYellow;// GetSysColor(COLOR_INFOBK);
      double red=GetRValue(c);
      double green=GetGValue(c);
      double blue=GetBValue(c);

      double blue1=min(255., blue*1.05);
      double green1=min(255., green*1.05);
      double red1=min(255., red*1.05);

      TRIVERTEX vert[2];
      vert [0] .x      = ti.xp-OffsetX;
      vert [0] .y      = ti.yp-OffsetY;
      vert [0] .Red    = 0xff00&DWORD(red1*256);
      vert [0] .Green  = 0xff00&DWORD(green1*256);
      vert [0] .Blue   = 0xff00&DWORD(blue1*256);
      vert [0] .Alpha  = 0x0000;

      vert [1] .x      = ti.xp+rc.right+dx-OffsetX;
      vert [1] .y      = ti.yp+rc.bottom+dx-OffsetY;
      vert [1] .Red    = 0xff00&DWORD(red*256);
      vert [1] .Green  = 0xff00&DWORD(green*256);
      vert [1] .Blue   = 0xff00&DWORD(blue*256);
      vert [1] .Alpha  = 0x0000;

      GRADIENT_RECT gr[1];
      gr[0].UpperLeft=0;
      gr[0].LowerRight=1;

      GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_H);
      SelectObject(hDC, GetStockObject(NULL_BRUSH));
      SelectObject(hDC, GetStockObject(DC_PEN));
      SetDCPenColor(hDC, RGB(DWORD(red*0.5),
                             DWORD(green*0.5),
                             DWORD(blue*0.5)));

      Rectangle(hDC, vert[0].x, vert[0].y, vert[1].x, vert[1].y);

      SetDCPenColor(hDC, RGB(DWORD(min(255., red*1.1)),
                             DWORD(min(255., green*1.2)),
                             DWORD(min(255., blue))));
      POINT pt;
      MoveToEx(hDC, vert[0].x-1, vert[1].y, &pt);
      LineTo(hDC, vert[0].x-1, vert[0].y-1);
      LineTo(hDC, vert[1].x, vert[0].y-1);

      SetDCPenColor(hDC, RGB(DWORD(min(255., red*0.4)),
                       DWORD(min(255., green*0.4)),
                       DWORD(min(255., blue*0.4))));

      MoveToEx(hDC, vert[1].x+0, vert[0].y, &pt);
      LineTo(hDC, vert[1].x+0, vert[1].y+0);
      LineTo(hDC, vert[0].x, vert[1].y+0);

    }
    dx/=2;
    rc.top=ti.yp+dx-OffsetY;
    rc.left=ti.xp+dx-OffsetX;
    rc.bottom+=ti.yp+dx-OffsetY;
    rc.right=ti.xp+dx+width-OffsetX;

    SetTextColor(hDC, 0);
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
  }

  if (hThis)
    ReleaseDC(hWndTarget, hDC);
}

void gdioutput::RenderString(TextInfo &ti, const wstring &text, HDC hDC)
{
  if (skipTextRender(ti.format))
    return;

  RECT rc;
  if ((ti.format & absolutePosition) == 0) {
    rc.left = ti.xp - OffsetX;
    rc.top = ti.yp - OffsetY;
  }
  else {
    rc.left = ti.xp;
    rc.top = ti.yp;
  }
  rc.right = rc.left;
  rc.bottom = rc.top;

  int format=ti.format&0xFF;
  assert(format!=10);
  formatString(ti, hDC);

  if (ti.xlimit==0){
    if (ti.format&textRight) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CALCRECT|DT_NOPREFIX);
      int dx=rc.right-rc.left;
      rc.right-=dx;
      rc.left-=dx;
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_RIGHT|DT_NOCLIP|DT_NOPREFIX);
    }
    else if (ti.format&textCenter) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CENTER|DT_CALCRECT|DT_NOPREFIX);
      int dx=rc.right-rc.left;
      rc.right-=dx/2;
      rc.left-=dx/2;
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CENTER|DT_NOCLIP|DT_NOPREFIX);
    }
    else{
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_NOCLIP|DT_NOPREFIX);
    }
  }
  else{
    if (ti.format&textRight) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      rc.right = rc.left + ti.xlimit;
      ti.textRect = rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_RIGHT|DT_NOPREFIX);
    }
    else {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      rc.right=rc.left+ti.xlimit;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_NOPREFIX);
      ti.textRect=rc;
    }
  }
}

void gdioutput::resetLast() const {
  lastFormet = -1;
  lastActive = false;
  lastHighlight = false;
  lastColor = -1;
  lastFont.clear();
}

void gdioutput::getFontInfo(const TextInfo &ti, FontInfo &fi) const {
  if (ti.font.empty()) {
    fi.name = 0;
    fi.bold = fi.normal = fi.italic = 0;
  }
  else {
    fi.name = &ti.font;
    getFont(ti.font).getInfo(fi);
  }
}


void gdioutput::formatString(const TextInfo &ti, HDC hDC) const
{
  int format=ti.format&0xFF;

  if (lastFormet == format &&
      lastActive == ti.active &&
      lastHighlight == ti.highlight &&
      lastColor == ti.color &&
      ti.font == lastFont)
    return;

  if (ti.font.empty()) {
    getCurrentFont().selectFont(hDC, format);
    lastFont.clear();
  }
  else {
    getFont(ti.font).selectFont(hDC, format);
    lastFont = ti.font;
  }

  SetBkMode(hDC, TRANSPARENT);

  if (ti.active)
    SetTextColor(hDC, RGB(255,0,0));
  else if (ti.highlight)
    SetTextColor(hDC, RGB(64,64,128));
  else if (ti.color == 0 && foregroundColor != -1)
    SetTextColor(hDC, foregroundColor);
  else
    SetTextColor(hDC, ti.color);
}

void gdioutput::calcStringSize(TextInfo &ti, HDC hDC_in) const {

  RECT rc;
  rc.left=ti.xp-OffsetX;
  rc.top=ti.yp-OffsetY;
  rc.right = rc.left;
  rc.bottom = rc.top;

  if ((ti.format & 0xFF) == textImage) {
    // Image
    int id = _wtoi(ti.text.c_str());
    int w = 16, h = 16;
    if (id > 0) {
      w = image.getWidth(id);
      h = image.getHeight(id);
    }
    else if (ti.text.size()>1 && ti.text[0] == 'S') { // Icon
      w = _wtoi(ti.text.c_str() + 1);
      h = getLineHeight();
    }
    else if (ti.text[0] == 'L') {
      return;
    }

    ti.textRect.left = rc.left;
    ti.textRect.right = rc.left + w + 5;
    ti.textRect.top = rc.top;
    ti.textRect.bottom = rc.bottom + h + 5;
    return;
  }

  HDC hDC = hDC_in;

  if (!hDC) {
    //    assert(hWndTarget!=0);
    hDC = GetDC(hWndTarget);
  }
  resetLast();
  formatString(ti, hDC);
  int format=ti.format&0xFF;

  if (format != 10 && (breakLines&ti.format) == 0) {
    if (ti.xlimit==0){
      if (ti.format&textRight) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_NOPREFIX);
        int dx=rc.right-rc.left;
        ti.realWidth = dx;
        rc.right-=dx;
        rc.left-=dx;
        ti.textRect=rc;
      }
      else if (ti.format&textCenter) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER|DT_CALCRECT|DT_NOPREFIX);
        int dx=rc.right-rc.left;
        ti.realWidth = dx;
        rc.right-=dx/2;
        rc.left-=dx/2;
        ti.textRect=rc;
      }
      else{
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
        ti.realWidth = rc.right - rc.left;
        ti.textRect=rc;
      }
    }
    else {
      DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      ti.realWidth = rc.right - rc.left;
      rc.right=rc.left+ti.xlimit;
      ti.textRect=rc;
    }
  }
  else {
    memset(&rc, 0, sizeof(rc));
    rc.right = scaleLength( (breakLines&ti.format) ? ti.xlimit : 450 );
    int dx = format != 10 ? 0 : scaleLength(20);
    ti.realWidth = rc.right + dx;
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    ti.textRect=rc;
    ti.textRect.right+=ti.xp+dx;
    ti.textRect.left+=ti.xp;
    ti.textRect.top+=ti.yp;
    ti.textRect.bottom+=ti.yp+dx;
  }

  if (!hDC_in)
    ReleaseDC(hWndTarget, hDC);
}


void gdioutput::updateScrollbars() const {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
}


void gdioutput::setOffsetY(int oy) { 
  bool changed = OffsetY != oy;
  OffsetY = oy; 
  if (changed)
    updateToolTips();
}

void gdioutput::setOffsetX(int ox) {
  bool changed = OffsetX != ox;
  OffsetX = ox; 
  if (changed) 
    updateToolTips();
}

void gdioutput::updateToolTips() {
  for (auto& tt : toolTips) {
    if (tt.hasRect) {
      tt.ti.rect.top = tt.rc.top - OffsetY;
      tt.ti.rect.bottom = tt.rc.bottom - OffsetY;
      tt.ti.rect.left = tt.rc.left - OffsetX;
      tt.ti.rect.right = tt.rc.right - OffsetX;
      SendMessage(hWndToolTip, TTM_NEWTOOLRECTW, 0, (LPARAM)&tt.ti);
    }
  }
}

void gdioutput::updateObjectPositions() {
  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (!it->AbsPos)
      SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
  }
  for (auto it = II.begin(); it != II.end(); ++it)
    SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
  updateToolTips();
}

constexpr int BoxWidthLimit = 250;
constexpr int NumBoxLimit = 10;

InfoBox &gdioutput::addInfoBox(const string &id, const wstring &text,
                               const wstring& extraLine, BoxStyle style, 
                               int timeOut, GUICALLBACK cb, bool autoRefresh) {
  InfoBox box;

  box.id = id;
  box.callBack = cb;
  box.text = lang.tl(text);
  box.underLine = lang.tl(extraLine);
  box.style = style;

  if (timeOut > 0)
    box.timeOut = GetTickCount64() + timeOut;

  IBox.push_back(box);

  if (autoRefresh && IBox.size() <= NumBoxLimit) {
    RECT rc;
    computeBoxesBoundingBox(rc);
    InvalidateRect(hWndTarget, &rc, true);
  }
  return IBox.back();
}

void gdioutput::drawBox(HDC hDC, InfoBox& box, RECT& pos) {
  getCurrentFont().selectFont(hDC, 0);
  lastFont.clear();

  SetBkMode(hDC, TRANSPARENT);

  //Calculate size.
  RECT testrect = { 0,0,0,0 };
  
  DrawText(hDC, box.text.c_str(), box.text.length(), &testrect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);

  int limit = scaleLength(BoxWidthLimit);
  int ulimit = scaleLength(80);

  if (testrect.right > limit || box.text.find_first_of('\n') != string::npos) {
    testrect.right = limit;
    DrawText(hDC, box.text.c_str(), box.text.length(), &testrect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
  }
  else if (testrect.right < ulimit)
    testrect.right = ulimit;


  RECT extraRect = { 0,0,0,0 };
  if (!box.underLine.empty()) {
    getCurrentFont().selectFont(hDC, 1);
    DrawText(hDC, box.underLine.c_str(), box.underLine.length(), &extraRect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
    extraRect.bottom += scaleLength(4);
  }

  int width = max(testrect.right, extraRect.right);
  int height = testrect.bottom + extraRect.bottom;

  pos.left = pos.right - (width + scaleLength(22));
  pos.top = pos.bottom - (height + scaleLength(20));
  
 
  box.boundingBox = pos;

  //Close Box
  RECT Close;
  Close.top = pos.top + 3;
  Close.bottom = Close.top + scaleLength(11);
  Close.right = pos.right - 3;
  Close.left = Close.right - scaleLength(11);

  box.close = Close;
  
  RECT tr = pos;

  tr.left += scaleLength(10);
  tr.right -= scaleLength(10);
  tr.top += scaleLength(15);
  tr.bottom -= scaleLength(5);
  box.textRect = tr;
  int extraYP = tr.top + testrect.bottom + scaleLength(4);

  box.underlineY = extraYP;
  
  drawBoxBg(hDC, box);
  drawCloseBox(hDC, box.close, false);
  drawBoxText(hDC, box, false);
}

void gdioutput::drawBoxBg(HDC hDC, const InfoBox& box) const {
  DWORD c;
  if (box.style == BoxStyle::HeaderWarning)
   c = colorLightRed;
  else
   c = GetSysColor(COLOR_INFOBK);

  double red = GetRValue(c);
  double green = GetGValue(c);
  double blue = GetBValue(c);

  double blue1 = min(255., blue * 1.1);
  double green1 = min(255., green * 1.1);
  double red1 = min(255., red * 1.1);

  TRIVERTEX vert[2];
  vert[0].x = box.boundingBox.left;
  vert[0].y = box.boundingBox.top;
  vert[0].Red = 0xff00 & DWORD(red * 256);
  vert[0].Green = 0xff00 & DWORD(green * 256);
  vert[0].Blue = 0xff00 & DWORD(blue * 256);
  vert[0].Alpha = 0x0000;

  vert[1].x = box.boundingBox.right;
  vert[1].y = box.boundingBox.bottom;
  vert[1].Red = 0xff00 & DWORD(red1 * 256);
  vert[1].Green = 0xff00 & DWORD(green1 * 256);
  vert[1].Blue = 0xff00 & DWORD(blue1 * 256);
  vert[1].Alpha = 0x0000;

  GRADIENT_RECT gr[1];

  gr[0].UpperLeft = 0;
  gr[0].LowerRight = 1;

  //if (MaxY>500)
  GradientFill(hDC, vert, 2, gr, 1, GRADIENT_FILL_RECT_V);


  //HPEN pen = CreatePen(PS_SOLID, scaleLength(2), RGB(0, 0, 0));
  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  SelectObject(hDC, GetStockObject(BLACK_PEN));
  Rectangle(hDC, box.boundingBox.left, box.boundingBox.top, box.boundingBox.right, box.boundingBox.bottom);


  SelectObject(hDC, GetStockObject(DC_PEN));

  SetDCPenColor(hDC, RGB(DWORD(min(255., red * 1.1)),
    DWORD(min(255., green * 1.2)),
    DWORD(min(255., blue))));
  POINT pt;
  MoveToEx(hDC, vert[0].x - 1, vert[1].y, &pt);
  LineTo(hDC, vert[0].x - 1, vert[0].y - 1);
  LineTo(hDC, vert[1].x, vert[0].y - 1);

  SetDCPenColor(hDC, RGB(DWORD(min(255., red * 0.4)),
    DWORD(min(255., green * 0.4)),
    DWORD(min(255., blue * 0.4))));

  MoveToEx(hDC, vert[1].x + 0, vert[0].y, &pt);
  LineTo(hDC, vert[1].x + 0, vert[1].y + 0);
  LineTo(hDC, vert[0].x, vert[1].y + 0);
}

void gdioutput::computeBoxesBoundingBox(RECT& rc) const {
  RECT clientRC;
  GetClientRect(hWndTarget, &clientRC);

  rc.left = clientRC.right;
  rc.right = clientRC.right;
  rc.bottom = clientRC.bottom;
  rc.top = clientRC.bottom;
  
  auto it = IBox.begin();
  int maxNumBox = NumBoxLimit;
  while (it != IBox.end() && --maxNumBox > 0) {
    if (it->boundingBox.right > 0) {
      rc.left = min(rc.left, it->boundingBox.left);
      rc.top = it->boundingBox.top;
    }
    else {
      rc.left = min(rc.left, clientRC.right - scaleLength(BoxWidthLimit+30));
      rc.top -= scaleLength(BoxWidthLimit / 2);
    } 
    ++it;
  }
}

void gdioutput::drawBoxes(HDC hDC, RECT &rc) {
  RECT pos;
  pos.right=rc.right;
  pos.bottom=rc.bottom;

  auto it=IBox.begin();
  int maxNumBox = NumBoxLimit;
  while (it != IBox.end() && --maxNumBox > 0) {
    drawBox(hDC, *it, pos);
    pos.bottom = pos.top;
    ++it;
  }
}

void gdioutput::drawCloseBox(HDC hDC, RECT &Close, bool pressed)
{
  HPEN hPen = CreatePen(PS_SOLID, int(scale * 1.5), 0);
  if (!pressed) 
    SelectObject(hDC, GetStockObject(WHITE_BRUSH));
  else 
    SelectObject(hDC, GetStockObject(LTGRAY_BRUSH));
  
  SelectObject(hDC, hPen);
    
  //Close Box
  Rectangle(hDC, Close.left, Close.top, Close.right, Close.bottom);

  MoveToEx(hDC, Close.left+1, Close.top+1, 0);
  LineTo(hDC, Close.right-2, Close.bottom-2);

  MoveToEx(hDC, Close.right-2, Close.top+1, 0);
  LineTo(hDC, Close.left+1, Close.bottom-2);

  SelectObject(hDC, GetStockObject(BLACK_PEN));
  DeleteObject(hPen);
}

void gdioutput::drawBoxText(HDC hDC, const InfoBox &box, bool highlight) {
  getCurrentFont().selectFont(hDC, 0);
  SetBkMode(hDC, TRANSPARENT);

  if (highlight) {
    SetTextColor(hDC, colorGreyBlue);
  }
  else {
    SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));
  }
  bool asHead = !box.underLine.empty() && box.style != BoxStyle::SubLine;

  RECT rc = box.textRect;
  
  if (asHead) {
    // Swap header/underline
    int diff = getLineHeight()+scaleLength(2);
    rc.top += diff;
    rc.bottom += diff;
  }
  
  DrawText(hDC, box.text.c_str(), box.text.length(), &rc, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);

  if (!box.underLine.empty()) {
    getCurrentFont().selectFont(hDC, 1);
    SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));

    RECT tr2 = box.textRect;
    if (!asHead)
      tr2.top = box.underlineY;

    DrawText(hDC, box.underLine.c_str(), box.underLine.length(), &tr2, DT_LEFT | DT_NOPREFIX);
  }
}

bool gdioutput::removeFirstInfoBox(const string& id) {
  auto it = IBox.begin();

  while (it != IBox.end()) {
    if (it->id == id) {
      IBox.erase(it);
      return true;
    }
    ++it;
  }
  return false;
}

wstring gdioutput::getTimerText(int zeroTime, int format, bool timeInSeconds, const wstring& textFormat) {
  TextInfo temp;
  temp.zeroTime=0;
  temp.format=format;
  temp.timerFormat = textFormat;
  if (timeInSeconds)
    return getTimerText(temp, 1000*zeroTime);
  else
    return getTimerText(temp, (1000/timeUnitsPerSecond) * zeroTime);
}

wstring gdioutput::getTimerText(const TextInfo &tit, uint64_t T) {
  int rt = int(T - tit.zeroTime) / 1000;
  int tenth = (abs(int(T - tit.zeroTime)) / 100) % 10;
  wstring text;

  int t=abs(rt);
  wchar_t bf[16];
  if ((tit.format & time24HourClock) != 0 && t > 0)
    t = t % (24 * timeConstSecPerHour);

  if (tit.format & timeHHMM) {
    swprintf_s(bf, 16, L"%d:%02d", (t / timeConstSecPerHour), (t / timeConstSecPerMin) % timeConstSecPerMin);
  }
  else if (tit.format & timeSeconds) {
    if (tit.format & timeWithTenth) 
      swprintf_s(bf, 16, L"%d.%d", t, tenth);
    else
      swprintf_s(bf, 16, L"%d", t);
  }
  else if ((tit.format & timeWithTenth) && rt < timeConstSecPerHour) {
    swprintf_s(bf, 16, L"%02d:%02d.%d", t/ timeConstSecPerMin, t%timeConstSecPerMin, tenth);
  }
  else if (rt>=timeConstSecPerHour  || (tit.format&fullTimeHMS))
    swprintf_s(bf, 16, L"%02d:%02d:%02d", t/ timeConstSecPerHour, (t/ timeConstSecPerMin)% timeConstSecPerMin, t%timeConstSecPerMin);
  else
    swprintf_s(bf, 16, L"%d:%02d", (t/ timeConstMinPerHour), t%timeConstMinPerHour);

  if (rt>0 || ((tit.format&fullTimeHMS) && rt>=0) )
    if (tit.format&timerCanBeNegative) 
      text = wstring(L"+") + bf;
    else				
      text = bf;
  else if (rt<0)
    if (tit.format&timerCanBeNegative) 
      text = wstring(L"-")+bf;
    else if (tit.format&timerIgnoreSign) 
      text = bf;
    else
      text = L"-";

  if (tit.timerFormat.empty())
   return text;
  else {
    return lang.tl(tit.timerFormat + L"#" + text);
  }
}

void gdioutput::CheckInterfaceTimeouts(uint64_t T)
{
  list<InfoBox>::iterator it=IBox.begin();

  while (it!=IBox.end()) {
    if (it->timeOut && it->timeOut<T) {
      if (it->hasCapture || it->hasTCapture)
        ReleaseCapture();

      InvalidateRect(hWndTarget, &(it->boundingBox), true);
      IBox.erase(it);
      it=IBox.begin();
    }
    else ++it;
  }

  list<TextInfo>::iterator tit = TL.begin();
  vector<TextInfo> timeout;
  if (hasAnyTimer) {
    bool anyChange = false;
    while(tit!=TL.end()){
      if (tit->hasTimer){
        wstring text = tit->xp > 0 ? getTimerText(*tit, T) : L"";
        if (tit->timeOut && T > tit->timeOut){
          tit->timeOut = 0;
          if (tit->callBack || tit->hasEventHandler())
            timeout.push_back(*tit);
        }
        if (text != tit->text) {
          RECT rc=tit->textRect;
          tit->text=text;
          calcStringSize(*tit);

          rc.right=max(tit->textRect.right, rc.right);
          rc.bottom=max(tit->textRect.bottom, rc.bottom);

          anyChange = true;
          //InvalidateRecthWndTarget, &rc, true);
        }
      }
      ++tit;
    }

    if (anyChange) {
      int w, h;
      getTargetDimension(w, h);
      HDC hDC = GetDC(hWndTarget);
      HBITMAP btm = CreateCompatibleBitmap(hDC, w, h);
      HDC memDC = CreateCompatibleDC (hDC);
      HGDIOBJ hOld = SelectObject(memDC, btm);
      RECT rc;
      rc.top = 0;
      rc.left = 0;
      rc.bottom = h;
      rc.right = w;
      RECT area = rc;
      drawBackground(memDC, rc);
      draw(memDC, rc, area);
      BitBlt(hDC, 0, 0, w, h, memDC, 0,0, SRCCOPY);
      SelectObject(memDC, hOld);
      DeleteObject(btm);
      DeleteDC(memDC);
      ReleaseDC(hWndTarget, hDC);
    }
  }

  for (size_t k = 0; k < timeout.size(); k++) {
    if (!timeout[k].handleEvent(*this, GUI_TIMEOUT))
      timeout[k].callBack(this, GUI_TIMEOUT, &timeout[k]);
  }
}

bool gdioutput::removeWidget(const string &id)
{
  {
    auto it = BI.begin();
    int cnt = 0;
    while (it != BI.end()) {
      if (it->id == id) {
        DestroyWindow(it->hWnd);
        biByHwnd.erase(it->hWnd);

        if (it->isCheckbox)
          removeString("T" + id);
        BI.erase(it);
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nBI > cnt)
            rp.second.nBI--;
        }
        return true;
      }
      ++it;
      ++cnt;
    }
  }
  {
    auto lit = LBI.begin();
    int cnt = 0;
    while (lit != LBI.end()) {
      if (lit->id == id) {
        DestroyWindow(lit->hWnd);
        lbiByHwnd.erase(lit->hWnd);
        removeString(id + "_label");
        if (lit->writeLock)
          hasCleared = true;
        LBI.erase(lit);
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nLBI > cnt)
            rp.second.nLBI--;
        }
        return true;
      }
      ++lit;
      cnt++;
    }
  }

  {
    auto iit = II.begin();
    int cnt = 0;
    while (iit != II.end()) {
      if (iit->id == id) {
        DestroyWindow(iit->hWnd);
        iiByHwnd.erase(iit->hWnd);
        II.erase(iit);
        removeString(id + "_label");
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nII > cnt)
            rp.second.nII--;
        }
        return true;
      }
      ++iit;
      cnt++;
    }
  }
  removeString(id);
  return false;
}

bool gdioutput::hideWidget(const string &id, bool hide) {
  list<ButtonInfo>::iterator it=BI.begin();

  while (it!=BI.end()) {
    if (it->id == id) {
      ShowWindow(it->hWnd, hide ? SW_HIDE : SW_SHOW);
      if (it->isCheckbox) {
        hideWidget("T" + id, hide);
      }
      return true;
    }
    ++it;
  }

  list<ListBoxInfo>::iterator lit=LBI.begin();

  while (lit!=LBI.end()) {
    if (lit->id==id) {
      ShowWindow(lit->hWnd, hide ? SW_HIDE : SW_SHOW);
      return true;
    }
    ++lit;
  }

  list<InputInfo>::iterator iit=II.begin();

  while (iit!=II.end()) {
    if (iit->id==id) {
      ShowWindow(iit->hWnd, hide ? SW_HIDE : SW_SHOW);
      return true;
    }
    ++iit;
  }

  for (auto &ti : TL) {
    if (ti.id == id) {
      if (hide)
        ti.format |= hiddenText;
      else
        ti.format &= ~hiddenText;

      return true;
    }
  }
  return false;
}

void gdioutput::setRestorePoint() {
  setRestorePoint("");
}


void gdioutput::setRestorePoint(const string &id) {
  RestoreInfo ri;

  ri.id = id;

  ri.nLBI = LBI.size();
  ri.nBI = BI.size();
  ri.nII = II.size();
  ri.nTL = TL.size();
  ri.nRect = Rectangles.size();
  ri.nTooltip = toolTips.size();
  ri.nTables = Tables.size();
  ri.nHWND = FocusList.size();
  ri.nData = DataInfo.size();
  
  for (auto& rp : restorePoints)
    ri.restorePoints.insert(rp.first);

  ri.sCX=CurrentX;
  ri.sCY=CurrentY;
  ri.sMX=MaxX;
  ri.sMY=MaxY;
  ri.sOX=OffsetX;
  ri.sOY=OffsetY;

  ri.onClear = onClear;
  ri.postClear = postClear;
  restorePoints[id]=ri;
}


bool gdioutput::getWidgetRestorePoint(const string& id, string& restorePoint) const {
  int count;

  count = 0;
  for (auto& lbi : LBI) {
    if (lbi.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nLBI && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }

  count = 0;
  for (auto& ii : II) {
    if (ii.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nII && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }


  count = 0;
  for (auto& bi : BI) {
    if (bi.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nBI && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }

  count = 0;
  for (auto& ti : TL) {
    if (ti.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nTL && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }


  return false;
}

void gdioutput::setWidgetRestorePoint(const string& id, const string& restorePoint) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = LBI.begin();
      int numNew = rpTarget.nLBI;
      advance(itNew, numNew);
      LBI.splice(itNew, LBI, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nLBI >= numNew) {
          rp.second.nLBI++;
        }
      }
      return;
    }
  }

  for (auto it = II.begin(); it != II.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = II.begin();
      int numNew = rpTarget.nII;
      advance(itNew, numNew);
      II.splice(itNew, II, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nII >= numNew) {
          rp.second.nII++;
        }
      }
      return;
    }
  }

  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = BI.begin();
      int numNew = rpTarget.nBI;
      advance(itNew, numNew);
      BI.splice(itNew, BI, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nBI >= numNew) {
          rp.second.nBI++;
        }
      }
      return;
    }
  }

  for (auto it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = TL.begin();
      int numNew = rpTarget.nTL;
      advance(itNew, numNew);
      TL.splice(itNew, TL, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nTL >= numNew) {
          rp.second.nTL++;
        }
      }
      return;
    }
  }
}

void gdioutput::restoreInternal(const RestoreInfo &ri)
{
  int toolRemove=toolTips.size()-ri.nTooltip;
  while (toolRemove>0 && toolTips.size()>0) {
    ToolInfo &info=toolTips.back();
    if (hWndToolTip) {
      SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM) &info.ti);
    }
    toolTips.pop_back();
    toolRemove--;
  }

  int lbiRemove=LBI.size()-ri.nLBI;
  while (lbiRemove>0 && LBI.size()>0) {
    ListBoxInfo &lbi=LBI.back();
    lbi.callBack = nullptr; // Avoid kill focus event here
    lbi.clearHandler();
    DestroyWindow(lbi.hWnd);
    if (lbi.writeLock)
      hasCleared = true;
    lbiByHwnd.erase(lbi.hWnd);
    LBI.pop_back();
    lbiRemove--;
  }
  int tlRemove=TL.size()-ri.nTL;

  while (tlRemove>0 && TL.size()>0) {
    TL.pop_back();
    tlRemove--;
  }
  itTL=TL.begin();
  updateImageReferences();

  // Clear cache of shown strings
  shownStrings.clear();

  int biRemove=BI.size()-ri.nBI;
  while (biRemove>0 && BI.size()>0) {
    ButtonInfo &bi=BI.back();
    bi.callBack = nullptr;
    bi.clearHandler();
    DestroyWindow(bi.hWnd);
    biByHwnd.erase(bi.hWnd);
    BI.pop_back();
    biRemove--;
  }

  int iiRemove=II.size()-ri.nII;

  while (iiRemove>0 && II.size()>0) {
    InputInfo &ii=II.back();
    ii.callBack = nullptr; // Avoid kill focus event here
    ii.clearHandler();
    DestroyWindow(ii.hWnd);
    iiByHwnd.erase(ii.hWnd);
    II.pop_back();
    iiRemove--;
  }

  int rectRemove=Rectangles.size()-ri.nRect;

  while (rectRemove>0 && Rectangles.size()>0) {
    Rectangles.pop_back();
    rectRemove--;
  }

  int hwndRemove=FocusList.size()-ri.nHWND;
  while(hwndRemove>0 && FocusList.size()>0) {
    FocusList.pop_back();
    hwndRemove--;
  }

  while(Tables.size() > unsigned(ri.nTables)){
    auto t=Tables.back().table;
    Tables.pop_back();
    t->hide(*this);
  }

  int dataRemove=DataInfo.size()-ri.nData;
  while(dataRemove>0 && DataInfo.size()>0) {
    DataInfo.pop_front();
    dataRemove--;
  }

  CurrentX=ri.sCX;
  CurrentY=ri.sCY;
  onClear = ri.onClear;
  postClear = ri.postClear;

  for (auto it = restorePoints.begin(); it != restorePoints.end(); ) {
    if (!ri.restorePoints.count(it->first) && (& it->second != &ri))
      it = restorePoints.erase(it);
    else
      ++it;
  }
}

void gdioutput::restore(const string &restorePointId, bool doRefresh) {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    return;
  const RestoreInfo& ri = rp->second;

  restoreInternal(ri);

  MaxX=ri.sMX;
  MaxY=ri.sMY;

  if (doRefresh)
    refresh();

  setOffset(ri.sOY, ri.sOY, false);
}

RECT gdioutput::getDimensionSince(const string& restorePointId) const {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    throw meosException("Internal error: " + restorePointId);
  
  const RestoreInfo& ri = rp->second;
  RECT out = {numeric_limits<int>::max(), numeric_limits<int>::max(), 0, 0};
  
  auto grow = [&out](int x, int y, int w, int h) {
    out.left = min<int>(out.left, x);
    out.right = max<int>(out.right, x + w);
    out.top = min<int>(out.top, y);
    out.bottom = max<int>(out.bottom, y + h);
  };

  int lbiRemove = LBI.size() - ri.nLBI;
  for (auto it = LBI.rbegin(); lbiRemove > 0; lbiRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }
  
  int tlRemove = TL.size() - ri.nTL;
  for (auto it = TL.rbegin(); tlRemove > 0; tlRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }

  int biRemove = BI.size() - ri.nBI;
  for (auto it = BI.rbegin(); biRemove > 0; biRemove--, ++it) {
    int w, h;
    it->getDimension(*this, w, h);
    grow(it->getX(), it->getY(), w, h);
  }

  int iiRemove = II.size() - ri.nII;
  for (auto it = II.rbegin(); iiRemove > 0; iiRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }

  int rectRemove = Rectangles.size() - ri.nRect;
  for (auto it = Rectangles.rbegin(); rectRemove > 0; rectRemove--, ++it) {
    auto& rc = it->getRect();
    grow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
  }
  
  return out;
}

void gdioutput::restoreNoUpdate(const string &restorePointId) {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    return;

  const RestoreInfo& ri = rp->second;

  MaxX=ri.sMX;
  MaxY=ri.sMY;

  restoreInternal(ri);
}

bool gdioutput::canClear() {
  bool ok = true;
  auto clsCopy = onClear;
  for (auto& clr : clsCopy) {
    try {
      if (clr.makeEvent(*this, GUI_CLEAR) == 0)
        ok = false;
    }
    catch (const meosCancel&) {
      return false;
    }
    catch (meosException& ex) {
      if (isTestMode)
        throw ex;
      wstring msg = ex.wwhat();
      alert(msg);
      return true;
    }
    catch (const std::exception& ex) {
      if (isTestMode)
        throw ex;
      string msg(ex.what());
      alert(msg);
      return true;
    }
  }
  return ok;
}

int gdioutput::sendCtrlMessage(const string &id)
{
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id) {
      if (it->hasEventHandler())
        return it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack) 
        return it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
    }
  }
  for(list<EventInfo>::iterator it=Events.begin(); it != Events.end(); ++it){
    if (id==it->id) {
      if (it->hasEventHandler())
        return it->handleEvent(*this, GUI_EVENT);
      else if (it->callBack) 
        return it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
    }
  }
#ifdef _DEBUG
  throw meosException("Unknown command " +id);
#endif
  return 0;
}

void gdioutput::unregisterEvent(const string &id)
{
  list<EventInfo>::iterator it;
  for (it = Events.begin(); it != Events.end(); ++it) {
    if ( id == it->id) {
      Events.erase(it);
      return;
    }
  }
}

EventInfo &gdioutput::registerEvent(const string &id, GUICALLBACK cb)
{
  list<EventInfo>::iterator it;
  for (it = Events.begin(); it != Events.end(); ++it) {
    if ( id == it->id) {
      Events.erase(it);
      break;
    }
  }

  EventInfo ei;
  ei.id=id;
  ei.callBack=cb;

  Events.push_front(ei);
  return Events.front();
}

void flushEvent(const string &id, const string &origin, DWORD data, int extraData);

DWORD gdioutput::makeEvent(const string &id, const string &origin,
                           DWORD data, int extraData, bool doflush)
{
  if (doflush) {
#ifndef MEOSDB
    ::flushEvent(id, origin, data, extraData);
#else
    throw std::exception("internal gdi/database error");
#endif
  }
  else {
    list<EventInfo>::iterator it;

    for(it=Events.begin(); it != Events.end(); ++it){
      if (id==it->id && (it->callBack || it->hasEventHandler()) ) {
        it->setData(origin, data);
        if (extraData) {
          it->setExtra(extraData);
        }
        if (it->handleEvent(*this, GUI_EVENT)) {
          return 1;
        }
        else
          return it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
      }
    }
  }
  return -1;
}


RectangleInfo &RectangleInfo::changeDimension(gdioutput &gdi, int dx, int dy) {
  rc.right += dx;
  rc.bottom += dy;
  int ex = gdi.scaleLength(5);
  gdi.updatePos(rc.left, rc.top, rc.right-rc.left+ex, rc.bottom-rc.top+ex);
  return *this;
}

RectangleInfo &gdioutput::addRectangle(const RECT &rc, GDICOLOR color, bool drawBorder, bool addFirst) {
  RectangleInfo ri;

  ri.rc.left = min<int>(rc.left, rc.right);
  ri.rc.right = max<int>(rc.left, rc.right);
  ri.rc.top = min<int>(rc.top, rc.bottom);
  ri.rc.bottom = max<int>(rc.top, rc.bottom);

  if (color==colorDefault)
    ri.color = GetSysColor(COLOR_INFOBK);
  else if (color == colorWindowBar) {
    ri.color = GetSysColor(COLOR_3DFACE);
  }
  else ri.color = color;

  ri.color2 = ri.color;
  ri.drawBorder = drawBorder;

  if (hWndTarget && !manualUpdate) {
    HDC hDC=GetDC(hWndTarget);
    renderRectangle(hDC, 0, ri);
    ReleaseDC(hWndTarget, hDC);
  }

  int ex = scaleLength(5);
  updatePos(ri.rc.left, ri.rc.top, ri.rc.right-ri.rc.left+ex, ri.rc.bottom-ri.rc.top+ex);
  if (addFirst) {
    Rectangles.push_front(ri);
    return Rectangles.front();
  }
  else {
    Rectangles.push_back(ri);
    return Rectangles.back();
  }
}

RectangleInfo &gdioutput::getRectangle(const char *id) {
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    return *it;
  }
  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
}
  
void gdioutput::setOffset(int x, int y, bool update)
{
  int h, w;
  getTargetDimension(w, h);

  int cdy = 0;
  int cdx = 0;

  if (y != OffsetY) {
    int oldY = OffsetY;
    OffsetY = y;
    if (OffsetY < 0)
      OffsetY = 0;
    else if (OffsetY > MaxY)
      OffsetY = MaxY;
    //cdy=(oldY!=OffsetY);
    cdy = oldY - OffsetY;
  }

  if (x != OffsetX) {
    int oldX = OffsetX;
    OffsetX = x;
    if (OffsetX < 0)
      OffsetX = 0;
    else if (OffsetX > MaxX)
      OffsetX = MaxX;

    //cdx=(oldX!=OffsetX);
    cdx = oldX - OffsetX;
  }

  if (cdx || cdy) {
    updateScrollbars();
    updateObjectPositions();
    if (cdy) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetY;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_VERT, &si, true);
    }

    if (cdx) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetX;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_HORZ, &si, true);
    }

    if (update) {
      //RECT ScrollArea, ClipArea;
      //GetClientRect(hWndTarget, &ScrollArea);
      //ClipArea = ScrollArea;

    /*  ScrollArea.top=-gdi->getHeight()-100;
      ScrollArea.bottom+=gdi->getHeight();
      ScrollArea.right=gdi->getWidth()-gdi->GetOffsetX()+15;
      ScrollArea.left = -2000;
  */
      ScrollWindowEx(hWndTarget, -cdx, cdy,
        NULL, NULL,
        (HRGN)NULL, (LPRECT)NULL, 0/*SW_INVALIDATE|SW_SMOOTHSCROLL|(1000*65536 )*/);
      UpdateWindow(hWndTarget);

    }
  }
}

void gdioutput::scrollTo(int x, int y) {
  int cx = x - OffsetX;
  int cy = y - OffsetY;

  int h, w;
  getTargetDimension(w, h);

  bool cdy = false;
  bool cdx = false;

  if (cy <= (h / 15) || cy >= (h - h / 10)) {
    int oldY = OffsetY;
    OffsetY = y - h / 2;
    if (OffsetY < 0)
      OffsetY = 0;
    else if (OffsetY > MaxY)
      OffsetY = MaxY;

    cdy = (oldY != OffsetY);
  }

  if (cx <= (w / 15) || cx >= (w - w / 8)) {
    int oldX = OffsetX;
    OffsetX = x - w / 2;
    if (OffsetX < 0)
      OffsetX = 0;
    else if (OffsetX > MaxX)
      OffsetX = MaxX;

    cdx = (oldX != OffsetX);
  }

  if (cdx || cdy) {
    updateScrollbars();
    updateObjectPositions();
    if (cdy) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetY;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_VERT, &si, true);
    }

    if (cdx) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetX;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_HORZ, &si, true);
    }
  }
}

void gdioutput::scrollToBottom() {
  OffsetY = MaxY;
  SCROLLINFO si;
  memset(&si, 0, sizeof(si));

  updateScrollbars();
  updateObjectPositions();
  si.nPos = OffsetY;
  si.fMask = SIF_POS;
  SetScrollInfo(hWndTarget, SB_VERT, &si, true);
}

bool gdioutput::clipOffset(int PageX, int PageY, int& MaxOffsetX, int& MaxOffsetY)
{
  if (animationData) {
    MaxOffsetX = 0;
    MaxOffsetY = 0;
    return false;
  }

  if (highContrast)
    setHighContrastMaxWidth();

  int oy = OffsetY;
  int ox = OffsetX;

  MaxOffsetY = max(getPageY() - PageY, 0);
  MaxOffsetX = max(getPageX() - PageX, 0);

  if (OffsetY < 0) OffsetY = 0;
  else if (OffsetY > MaxOffsetY)
    OffsetY = MaxOffsetY;

  if (OffsetX < 0) OffsetX = 0;
  else if (OffsetX > MaxOffsetX)
    OffsetX = MaxOffsetX;

  if (ox != OffsetX || oy != OffsetY) {
    updateObjectPositions();
    return true;

  }
  return false;
}

//bool ::GetSaveFile(string &file, char *filter)
wstring gdioutput::browseForSave(const vector< pair<wstring, wstring> > &filter,
                                const wstring &defext, int &filterIndex)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for file");
  }

  InitCommonControls();

  TCHAR FileName[260];
  FileName[0]=0;
  OPENFILENAME of;
  wstring sFilter;
  for (size_t k = 0; k< filter.size(); k++) {
    sFilter.append(lang.tl(filter[k].first)).push_back(0);
    sFilter.append(filter[k].second).push_back(0);
  }
  sFilter.push_back(0);

  of.lStructSize       = sizeof(of);
  of.hwndOwner         = hWndTarget;
  of.hInstance         = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  of.lpstrFilter       = sFilter.c_str();
  of.lpstrCustomFilter = NULL;
  of.nMaxCustFilter    = 0;
  of.nFilterIndex      = filterIndex;
  of.lpstrFile         = FileName;
  of.nMaxFile          = 260;
  of.lpstrFileTitle    = NULL;
  of.nMaxFileTitle     = 0;
  of.lpstrInitialDir   = NULL;
  of.lpstrTitle        = NULL;
  of.Flags             = OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
  of.lpstrDefExt   	   = defext.c_str();
  of.lpfnHook		       = NULL;

  bool res;
  setCommandLock();
  try {
    res = GetSaveFileName(&of) != false;
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (res==false)
    return L"";

  filterIndex=of.nFilterIndex;

  return FileName;
}

wstring gdioutput::browseForOpen(const vector< pair<wstring, wstring> > &filter,
                                const wstring &defext)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for file");
  }

  InitCommonControls();

  wchar_t FileName[260];
  FileName[0]=0;
  OPENFILENAME of;

  wstring sFilter;
  for (size_t k = 0; k< filter.size(); k++) {
    sFilter.append(lang.tl(filter[k].first)).push_back(0);
    sFilter.append(filter[k].second).push_back(0);
  }
  sFilter.push_back(0);
  
  of.lStructSize       = sizeof(of);
  of.hwndOwner         = hWndTarget;
  of.hInstance         = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  of.lpstrFilter       = sFilter.c_str();
  of.lpstrCustomFilter = NULL;
  of.nMaxCustFilter    = 0;
  of.nFilterIndex      = 1;
  of.lpstrFile         = FileName;
  of.nMaxFile          = 260;
  of.lpstrFileTitle    = NULL;
  of.nMaxFileTitle     = 0;
  of.lpstrInitialDir   = NULL;
  of.lpstrTitle        = NULL;
  of.Flags             = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
  of.lpstrDefExt   	   = defext.c_str();
  of.lpfnHook		       = NULL;

  bool res;
  setCommandLock();
  try {
    res = GetOpenFileName(&of) != false;
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }


  if (res == false)
    return L"";

  return FileName;
}

wstring gdioutput::browseForFolder(const wstring &folderStart, const wchar_t *descr)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for folder");
  }

  CoInitializeEx(0, COINIT_APARTMENTTHREADED);
  BROWSEINFO bi;

  wchar_t InstPath[260];
  wcscpy_s(InstPath, folderStart.c_str());

  memset(&bi, 0, sizeof(bi) );

  bi.hwndOwner=hWndAppMain;
  bi.pszDisplayName=InstPath;
  wstring title = descr ? lang.tl(descr) : L"";
  bi.lpszTitle = title.c_str();
  bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_EDITBOX|BIF_NEWDIALOGSTYLE|BIF_EDITBOX;

  LPITEMIDLIST  pidl_new;

  setCommandLock();
  try {
    pidl_new = SHBrowseForFolder(&bi);
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (pidl_new==NULL)
    return L"";

  // Convert the item ID list's binary
  // representation into a file system path
  //char szPath[_MAX_PATH];
  SHGetPathFromIDList(pidl_new, InstPath);

  // Allocate a pointer to an IMalloc interface
  LPMALLOC pMalloc;

  // Get the address of our task allocator's IMalloc interface
  SHGetMalloc(&pMalloc);

  // Free the item ID list allocated by SHGetSpecialFolderLocation
  pMalloc->Free(pidl_new);

  // Free our task allocator
  pMalloc->Release();

  return InstPath;
}


bool gdioutput::openDoc(const wstring &doc) {
  return (intptr_t)ShellExecute(hWndTarget, L"open", doc.c_str(), NULL, L"", SW_SHOWNORMAL) >32;
}

void gdioutput::init(HWND hWnd, HWND hMain, HWND hTab) {
  setWindow(hWnd);
  hWndAppMain=hMain;
  hWndTab=hTab;

  InitCommonControls();

  hWndToolTip = CreateWindow(TOOLTIPS_CLASS, (LPWSTR) NULL, TTS_ALWAYSTIP,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      NULL, (HMENU) NULL, (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
}

ToolInfo &gdioutput::addToolTip(const string &tipId, const wstring &tip, HWND hWnd, RECT *rc) {
  static ToolInfo dummy;
  if (!hWndToolTip)
    return dummy;

  toolTips.emplace_back();
  ToolInfo &info = toolTips.back();
  TOOLINFOW &ti = info.ti;
  info.tip = lang.tl(tip);

  memset(&ti, 0, sizeof(ti));
  ti.cbSize = sizeof(TOOLINFO);

  if (hWnd != 0) {
    ti.uFlags = TTF_IDISHWND;
    info.id = uintptr_t(hWnd);
    ti.uId = (UINT_PTR) hWnd;
  }
  else {
    ti.uFlags = TTF_SUBCLASS;
    info.id = toolTips.size();
    ti.uId = info.id;
  }

  ti.hwnd = hWndTarget;
  ti.hinst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  info.name = tipId;
  ti.lpszText = (LPWSTR)toolTips.back().tip.c_str();

  if (rc != nullptr) {
    ti.rect = *rc;
    info.rc = *rc;
    info.hasRect = true;
    ti.rect.top -= OffsetY;
    ti.rect.bottom -= OffsetY;
    ti.rect.right -= OffsetX;
    ti.rect.left -= OffsetX;
  }
  SendMessage(hWndToolTip, TTM_ADDTOOLW, 0, (LPARAM) &ti);

  if (tip.find('\n') != string::npos || tip.length()>40)
    SendMessage(hWndToolTip, TTM_SETMAXTIPWIDTH, 0, scaleLength(250));

  return info;
}

void gdioutput::removeToolTip(const string& id) {
  for (auto tt = toolTips.begin(); tt != toolTips.end(); ++tt) {
    if (tt->name == id) {
      if (hWndToolTip) {
        SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM)&tt->ti);
      }
      toolTips.erase(tt);
      return;
    }
  }
}


ToolInfo *gdioutput::getToolTip(const string &id) {
  for (ToolList::reverse_iterator it = toolTips.rbegin(); it != toolTips.rend(); ++it) {
    if (it->name == id)
      return &*it;
  }
  return 0;
}

ToolInfo &gdioutput::updateToolTip(const string &id, const wstring &tip) {
  for (ToolList::reverse_iterator it = toolTips.rbegin(); it != toolTips.rend(); ++it) {
    if (it->name == id && hWndToolTip) {
      it->tip = lang.tl(tip);
      it->ti.lpszText = (LPWSTR)it->tip.c_str();
      SendMessage(hWndToolTip, TTM_UPDATETIPTEXTW, 0, (LPARAM) &it->ti);
      return *it;
    }
  }
  BaseInfo &bi = getBaseInfo(id.c_str());
  return addToolTip(id, tip, bi.getControlWindow());
}

void gdioutput::selectTab(int Id)
{
  if (hWndTab)
    TabCtrl_SetCurSel(hWndTab, Id);
}

void gdioutput::getTargetDimension(int &x, int &y) const
{
  if (hWndTarget){
    RECT rc;
    GetClientRect(hWndTarget, &rc);
    x=rc.right;
    y=rc.bottom;
  }
  else {
    x=0;
    y=0;
  }
}

Table &gdioutput::getTable() const {
  if (Tables.empty())
    throw std::exception("No table defined");

  return *const_cast<Table *>(Tables.back().table.get());
}

static int gdiTableCB(gdioutput *gdi, GuiEventType type, BaseInfo *data)
{
  if (type == GUI_BUTTON) {
    ButtonInfo bi = *static_cast<ButtonInfo *>(data);
    gdi->tableCB(bi, &gdi->getTable());
  }
  return 0;
}

void gdioutput::tableCB(ButtonInfo &bu, Table *t)
{
  if (bu.id=="tblPrint") {
    t->keyCommand(*this, KC_PRINT);
  }
  else if (bu.id=="tblColumns") {
    disableTables();
    if (Tables.empty())
      return;

    restore("tblRestore");
    int ybase =  Tables.back().yp;
    addString("", ybase, 20, boldLarge, "Välj kolumner");
    ybase += scaleLength(30);
    addString("", ybase, 20, 0, L"Välj kolumner för tabellen X.#"+ t->getTableName());
    ybase += getLineHeight()*2;

    addListBox(20, ybase, "tblColSel", 180, 450, 0, L"", L"", true);
    const int btnHeight = getButtonHeight()+scaleLength(5);
    vector<Table::ColSelection> cols = t->getColumns();
    set<int> sel;

    for (size_t k=0; k<cols.size(); k++) {
      addItem("tblColSel", cols[k].name, cols[k].index);
      if (cols[k].selected)
        sel.insert(cols[k].index);
    }
    setSelection("tblColSel", sel);
    int xp = scaleLength(220);
    addButton(xp, ybase+btnHeight*0, "tblAll", "Välj allt", gdiTableCB);
    addButton(xp, ybase+btnHeight*1, "tblNone", "Välj inget", gdiTableCB);
    addButton(xp, ybase+btnHeight*2, "tblAuto", "Välj automatiskt", gdiTableCB).setExtra(t->getTableId());

    addButton(xp, ybase+btnHeight*4, "tblOK", "OK", gdiTableCB).setExtra(t->getTableId());
    addButton(xp, ybase+btnHeight*5, "tblCancel", "Avbryt", gdiTableCB);

    if (toolbar)
      toolbar->hide();

    refresh();
  }
  else if (bu.id=="tblAll") {
    set<int> sel;
    sel.insert(-1);
    setSelection("tblColSel", sel);
  }
  else if (bu.id=="tblNone") {
    set<int> sel;
    setSelection("tblColSel", sel);
  }
  else if (bu.id=="tblAuto") {
    restore("tblRestore", false);
    t->autoSelectColumns();
    t->autoAdjust(*this);
    enableTables();
    refresh();
  }
  else if (bu.id=="tblOK") {
    set<int> sel;
    getSelection("tblColSel", sel);
    restore("tblRestore", false);
    t->clearCellSelection(this);
    t->selectColumns(sel);
    t->autoAdjust(*this);
    enableTables();
    refresh();
  }
  else if (bu.id=="tblReset") {
    t->clearCellSelection(this);
    t->resetColumns();
    t->autoAdjust(*this);
    t->updateDimension(*this);
    refresh();
  }
  else if (bu.id == "tblMarkAll") {
    t->keyCommand(*this, KC_MARKALL);
  }
  else if (bu.id == "tblClearAll") {
    t->keyCommand(*this, KC_CLEARALL);
  }
  else if (bu.id=="tblUpdate") {
    t->keyCommand(*this, KC_REFRESH);
  }
  else if (bu.id=="tblCancel") {
    restore("tblRestore", true);
    enableTables();
    refresh();
  }
  else if (bu.id == "tblCopy") {
    t->keyCommand(*this, KC_COPY);
  }
  else if (bu.id == "tblPaste") {
    t->keyCommand(*this, KC_PASTE);
  }
  else if (bu.id == "tblRemove") {
    t->keyCommand(*this, KC_DELETE);
  }
  else if (bu.id == "tblInsert") {
    t->keyCommand(*this, KC_INSERT);
  }
}

void gdioutput::enableTables()
{
  useTables=true;
  if (!Tables.empty()) {
    auto &t = Tables.front().table;
    if (toolbar == 0)
      toolbar = new Toolbar(*this);

    toolbar->setData(t);

    string tname = string("table") + itos(t->canDelete()) + itos(t->canInsert()) + itos(t->canPaste());
    if (!toolbar->isLoaded(tname)) {
      toolbar->reset();
      toolbar->addButton("tblColumns", 1, 2, "Välj vilka kolumner du vill visa");
      toolbar->addButton("tblPrint", 0, STD_PRINT, "Skriv ut tabellen (X)#Ctrl+P");
      toolbar->addButton("tblUpdate", 1, 0, "Uppdatera alla värden i tabellen (X)#F5");
      toolbar->addButton("tblReset", 1, 4, "Återställ tabeldesignen och visa allt");
      toolbar->addButton("tblMarkAll", 1, 5, "Markera allt (X)#Ctrl+A");
      toolbar->addButton("tblClearAll", 1, 6, "Markera inget (X)#Ctrl+D");
      toolbar->addButton("tblCopy", 0, STD_COPY, "Kopiera selektionen till urklipp (X)#Ctrl+C");
      if (t->canPaste())
        toolbar->addButton("tblPaste", 0, STD_PASTE, "Klistra in data från urklipp (X)#Ctrl+V");
      if (t->canDelete())
       toolbar->addButton("tblRemove", 1, 1, "Ta bort valda rader från tabellen (X)#Del");
      if (t->canInsert())
       toolbar->addButton("tblInsert", 1, 3, "Lägg till en ny rad i tabellen (X)#Ctrl+I");
      toolbar->createToolbar(tname, L"Tabellverktyg");
    }
    else {
      toolbar->show();
    }
  }
}

void gdioutput::processToolbarMessage(const string &id, Table *tbl) {
  if (hasCommandLock())
    return;
  wstring msg;
  string cmd;
  if (getRecorder().recording()) { 
    cmd = "tableCmd(\"" + id + "\"); //" + toUTF8(tbl->getTableName());
  }
  try {
    ButtonInfo bi;
    bi.id = id;
    tableCB(bi, tbl);
    getRecorder().record(cmd);
  }
  catch (const meosCancel&) {
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
}

HWND gdioutput::getToolbarWindow() const {
  if (!toolbar)
    return 0;
  return toolbar->getFloater();
}

bool gdioutput::hasToolbar() const {
  if (!toolbar)
    return false;
  return toolbar->isVisible();
}

void gdioutput::activateToolbar(bool active) {
  if (!toolbar)
    return;
  toolbar->activate(active);
}

void gdioutput::disableTables()
{
  useTables=false;

  for(list<ButtonInfo>::iterator bit=BI.begin(); bit != BI.end();) {
    if (bit->id.substr(0, 3)=="tbl" && bit->getExtra()!=0) {
      string id = bit->id;
      ++bit;
      removeWidget(id);
    }
    else
      ++bit;
  }

}

void gdioutput::addTable(const shared_ptr<Table> &t, int x, int y)
{
  TableInfo ti;
  ti.table = t;
  ti.xp = x;
  ti.yp = y;
  t->setPosition(x,y, MaxX, MaxY);

  if (t->hasAutoSelect())
    t->autoSelectColumns();
  t->autoAdjust(*this);

  Tables.push_back(ti);

  //updatePos(x, y, dx + TableXMargin, dy + TableYMargin);
  setRestorePoint("tblRestore");

  enableTables();
  updateScrollbars();
}

void gdioutput::pasteText(const char *id)
{
  list<InputInfo>::iterator it;
  for (it=II.begin(); it != II.end(); ++it) {
    if (it->id==id) {
      SendMessage(it->hWnd, WM_PASTE, 0,0);
      return;
    }
  }
}

wchar_t *gdioutput::getExtra(const char *id) const {
  return getBaseInfo(id).getExtra();
}

int gdioutput::getExtraInt(const char *id) const {
  return getBaseInfo(id).getExtraInt();
}

bool gdioutput::hasEditControl() const
{
  return !II.empty() || (Tables.size()>0 && Tables.front().table->hasEditControl());
}

void gdioutput::enableEditControls(bool enable, bool processAll)
{
  set<string> TCheckControls;
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (it->isEditControl || processAll) {
      EnableWindow(it->hWnd, enable);
      if (it->isCheckbox) {
        TCheckControls.insert("T" + it->id);
      }
    }
  }

  for (list<TextInfo>::iterator it=TL.begin(); it != TL.end(); ++it) {
    if (TCheckControls.count(it->id)) {
      enableCheckBoxLink(*it, enable);
    }
  }


  for (list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it) {
    if (it->isEditControl)
      EnableWindow(it->hWnd, enable);
  }

  for(  list<ListBoxInfo>::iterator it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->isEditControl)
      EnableWindow(it->hWnd, enable);
  }
}

void gdioutput::closeWindow() {
  PostMessage(hWndTarget, WM_CLOSE, 0, 0);
}

InputInfo &InputInfo::setPassword(bool pwd) {
  LONG style = GetWindowLong(hWnd, GWL_STYLE);
  if (pwd)
    style |= ES_PASSWORD;
  else
    style &= ~ES_PASSWORD;
  SetWindowLong(hWnd, GWL_STYLE, style);
  SendMessage(hWnd, EM_SETPASSWORDCHAR, 183, 0);
  return *this;
}

int gdioutput::setHighContrastMaxWidth() {

  RECT rc;
  GetClientRect(hWndTarget, &rc);

  if (lockRefresh)
    return rc.bottom;

#ifdef DEBUGRENDER
  OutputDebugString("Set high contrast\n");
#endif

  double w = getPageX();
  double s = rc.right / w;
  if (!highContrast || (fabs(s-1.0) > 1e-3 && (s * scale) >= 1.0) ) {
    lockRefresh = true;
    try {
      highContrast = true;
      scaleSize(s);
      refresh();
      lockRefresh = false;
    }
    catch (...) {
      lockRefresh = false;
      throw;
    }
  }
  return rc.bottom;
}

double static acc = 0;

void gdioutput::setAutoScroll(double speed) {
  if (autoSpeed == 0 && speed != 0) {
    SetTimer(hWndTarget, 1001, 20, 0);
    autoPos = OffsetY;
  }
  else if (speed == 0 && autoSpeed != 0) {
    KillTimer(hWndTarget, 1001);
  }

  if (speed == -1)
    autoSpeed = -autoSpeed;
  else
    autoSpeed = speed;

  autoCounter = - M_PI_2;
  acc = 0;
}

void gdioutput::getAutoScroll(double &speed, double &pos) const {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  double height = rc.bottom;

  double s = autoSpeed * (1 + height/1000 + sin(autoCounter)/max(1.0, 500/height));

  autoCounter += M_PI/75.0;
  if (autoCounter > M_PI)
    autoCounter -= 2*M_PI;

  acc += 0.3/30;
  if (acc>0.8)
    acc = 0.8;

  speed = (lastSpeed * (1.0-acc) + s * acc);
  lastSpeed = speed;
  pos = autoPos;
}

void gdioutput::storeAutoPos(double pos) {
  autoPos = pos;
}

void gdioutput::setFullScreen(bool useFullScreen) {
  if (useFullScreen && !fullScreen) {
    SetWindowLong(hWndTarget, GWL_STYLE, WS_POPUP | WS_BORDER);
    ShowWindow(hWndTarget, SW_MAXIMIZE);
    UpdateWindow(hWndTarget);
  }
  else if (fullScreen) {
    SetWindowLong(hWndTarget, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    ShowWindow(hWndTarget, SW_NORMAL);
    UpdateWindow(hWndTarget);
  }
  fullScreen = useFullScreen;
}

void gdioutput::setColorMode(DWORD bgColor1, DWORD bgColor2,
                             DWORD fgColor, const wstring &bgImage) {
  backgroundColor1 = bgColor1;
  backgroundColor2 = bgColor2;
  foregroundColor = fgColor;
  backgroundImage = bgImage;
}


bool gdioutput::hasFGColor() const {
  return foregroundColor != -1;
}
bool gdioutput::hasBGColor() const {
  return backgroundColor1 != -1;
}
bool gdioutput::hasBGColor2() const {
  return backgroundColor2 != -1;
}

DWORD gdioutput::getFGColor() const {
  return foregroundColor != -1 ? foregroundColor : 0;
}
DWORD gdioutput::getBGColor() const {
  return backgroundColor1 != -1 ? backgroundColor1 : RGB(255,255,255);
}
DWORD gdioutput::getBGColor2() const {
  return backgroundColor2;
}
const wstring &gdioutput::getBGImage() const {
  return backgroundImage;
}

bool gdioutput::hasCommandLock() const {
  if (commandLock)
    return true;

  if (commandUnlockTime > 0) {
    uint64_t t = GetTickCount64();
    if (commandUnlockTime < (commandUnlockTime + 500) &&
        t < (commandUnlockTime+500)) {
      commandUnlockTime = 0;
      return true;
    }
  }

  return false;
}

void gdioutput::setCommandLock() const {
  commandLock = true;
}

void gdioutput::liftCommandLock() const {
  commandUnlockTime = GetTickCount64();
  commandLock = false;
}

int gdioutput::getLineHeight(gdiFonts font, const wchar_t *face) const {
  int h;
  if (face == nullptr)
    h = getFontHeight(font, _EmptyWString);
  else
    h = getFontHeight(font, face);

  return (11*h)/10;
}

GDIImplFontSet::GDIImplFontSet() {
  Huge = 0;
  Large = 0;
  Medium = 0;
  Small = 0;
  pfLarge = 0;
  pfMedium = 0;
  pfMediumPlus = 0;
  pfSmall = 0;
  pfSmallItalic = 0;
  pfItalic = 0;
  pfItalicMediumPlus = 0;
  pfMono = 0;
}

GDIImplFontSet::~GDIImplFontSet() {
  deleteFonts();
}

void GDIImplFontSet::deleteFonts()
{
  if (Huge)
    DeleteObject(Huge);
  Huge = 0;

  if (Large)
    DeleteObject(Large);
  Large = 0;

  if (Medium)
    DeleteObject(Medium);
  Medium = 0;

  if (Small)
    DeleteObject(Small);
  Small = 0;

  if (pfLarge)
    DeleteObject(pfLarge);
  pfLarge = 0;

  if (pfMedium)
    DeleteObject(pfMedium);
  pfMedium = 0;

  if (pfMediumPlus)
    DeleteObject(pfMediumPlus);
  pfMediumPlus = 0;

  if (pfSmall)
    DeleteObject(pfSmall);
  pfSmall = 0;

  if (pfMono)
    DeleteObject(pfMono);
  pfMono = 0;


  if (pfSmallItalic)
    DeleteObject(pfSmallItalic);
  pfSmallItalic = 0;

  if (pfItalicMediumPlus)
    DeleteObject(pfItalicMediumPlus);
  pfItalicMediumPlus = 0;

  if (pfItalic)
    DeleteObject(pfItalic);
  pfItalic = 0;
}

float GDIImplFontSet::baseSize(int format, float scale)  {
  format &= 0xFF;
  if (format==0 || format==10) {
    return 14 * scale;
  }
  else if (format==fontMedium){
    return 14 * scale;
  }
  else if (format==1) {
    return 14.0001f * scale; //Bold
  }
  else if (format==boldLarge){
    return 24.0001f * scale;
  }
  else if (format==boldHuge){
    return 34.0001f * scale;
  }
  else if (format==boldSmall){
    return 11.0001f * scale;
  }
  else if (format==fontLarge){
    return 24 * scale;
  }
  else if (format==fontMediumPlus){
    return 18 * scale;
  }
  else if (format==fontSmall){
    return 11 * scale;
  }
  else if (format==italicSmall){
    return 11 * scale;
  }
  else if (format==italicText){
    return 14 * scale;
  }
  else if (format == monoText){
    return 14 * scale;
  }
  else if (format==italicMediumPlus){
    return 18 * scale;
  }
  else {
    return 10 * scale;
  }
}

void GDIImplFontSet::init(double scale, const wstring &font, const wstring &gdiName_)
{
  int charSet = DEFAULT_CHARSET;
  deleteFonts();
  gdiName = gdiName_;

  Huge=CreateFont(int(scale*34), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Large=CreateFont(int(scale*24), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Medium=CreateFont(int(scale*14), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Small=CreateFont(int(scale*11), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfLarge=CreateFont(int(scale*24), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMedium=CreateFont(int(scale*14), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMediumPlus=CreateFont(int(scale*18), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfSmall=CreateFont(int(scale*11), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfSmallItalic = CreateFont(int(scale*11), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfItalic = CreateFont(int(scale*14), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMono = CreateFont(int(scale*12), 0, 0, 0, FW_NORMAL, false, false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_MODERN, L"Lucida Console");

  pfItalicMediumPlus = CreateFont(int(scale*18), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());
}

void GDIImplFontSet::getInfo(FontInfo &fi) const {
  fi.normal = pfMedium;
  fi.bold = Medium;
  fi.italic = pfItalic;
}

void GDIImplFontSet::selectFont(HDC hDC, int format) const {
  if (format==0 || format==10) {
    SelectObject(hDC, pfMedium);
  }
  else if (format==fontMedium){
    SelectObject(hDC, pfMedium);
  }
  else if (format==1){
    SelectObject(hDC, Medium);
  }
  else if (format==boldLarge){
    SelectObject(hDC, Large);
  }
  else if (format==boldHuge){
    SelectObject(hDC, Huge);
  }
  else if (format==boldSmall){
    SelectObject(hDC, Small);
  }
  else if (format==fontLarge){
    SelectObject(hDC, pfLarge);
  }
  else if (format==fontMediumPlus){
    SelectObject(hDC, pfMediumPlus);
  }
  else if (format==fontSmall){
    SelectObject(hDC, pfSmall);
  }
  else if (format==italicSmall){
    SelectObject(hDC, pfSmallItalic);
  }
  else if (format==italicText){
    SelectObject(hDC, pfItalic);
  }
  else if (format==italicMediumPlus){
    SelectObject(hDC, pfItalicMediumPlus);
  }
  else if (format == monoText) {
    SelectObject(hDC, pfMono);
  }
  else {
    SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  }
}


HFONT GDIImplFontSet::getFont(int format) const {
  format = format & 31;
  if (format==0 || format==10) {
    return pfMedium;
  }
  else if (format==fontMedium){
    return pfMedium;
  }
  else if (format==1){
    return Medium;
  }
  else if (format==boldLarge){
    return Large;
  }
  else if (format==boldHuge){
    return Huge;
  }
  else if (format==boldSmall){
    return Small;
  }
  else if (format==fontLarge){
    return pfLarge;
  }
  else if (format==fontMediumPlus){
    return pfMediumPlus;
  }
  else if (format==fontSmall){
    return pfSmall;
  }
  else if (format==italicSmall){
    return pfSmallItalic;
  }
  else if (format==italicText){
    return pfItalic;
  }
  else if (format==italicMediumPlus){
    return pfItalicMediumPlus;
  }
  else if (format == monoText) {
    return pfMono;
  }
  else {
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  }
}



const GDIImplFontSet &gdioutput::getCurrentFont() const {
  if (currentFontSet == 0) {
    map<wstring, GDIImplFontSet>::const_iterator res = fonts.find(currentFont);
    if (res == fonts.end())
      throw meosException(L"Font not defined: " + currentFont);
    currentFontSet = &res->second;
  }

  return *currentFontSet;
}

const GDIImplFontSet &gdioutput::getFont(const wstring &font) const {
  map<wstring, GDIImplFontSet>::const_iterator res = fonts.find(font);
  if (res == fonts.end()) {
    return const_cast<gdioutput *>(this)->loadFont(font);
    throw meosException(L"Font not defined: " + currentFont);
  }
  return res->second;
}

int CALLBACK enumFontProc(const LOGFONT* logFont, const TEXTMETRIC *metric, DWORD id, LPARAM lParam) {
  if (logFont->lfFaceName[0] == '@')
    return 1;

  if (metric->tmAveCharWidth <= 0)
    return 1;

  vector<GDIImplFontEnum> &enumFonts = *(vector<GDIImplFontEnum> *)(lParam);
  /*string we = "we: " + itos(logFont->lfWeight);
  string wi = "wi: " + itos(metric->tmAveCharWidth);
  string he = "he: " + itos(metric->tmHeight);
  string info = string(logFont->lfFaceName) + ", " + we + ", " + wi + ", " + he;*/
  enumFonts.push_back(GDIImplFontEnum());
  GDIImplFontEnum &f = enumFonts.back();
  f.face = logFont->lfFaceName;
  f.height = metric->tmHeight;
  f.width = metric->tmAveCharWidth;
  f.relScale = ((double(metric->tmHeight) / double(metric->tmAveCharWidth)) * 14.0/36.0);
  return 1;
}

void gdioutput::getEnumeratedFonts(vector< pair<wstring, size_t> > &output) const {
  if (enumeratedFonts.empty()) {
    HDC hDC = GetDC(hWndTarget);
//    EnumFontFamilies(hDC, NULL, enumFontProc, LPARAM(&enumeratedFonts));
    LOGFONT logFont;
    memset(&logFont, 0, sizeof(LOGFONT));
    logFont.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesEx(hDC, &logFont, enumFontProc, LPARAM(&enumeratedFonts), 0);
    ReleaseDC(hWndTarget, hDC);
  }
  output.resize(enumeratedFonts.size());
  for (size_t k = 0; k<output.size(); k++) {
    output[k].first = enumeratedFonts[k].getFace();
    output[k].second = k;
  }
}

double gdioutput::getRelativeFontScale(gdiFonts font, const wchar_t *fontFace) const {
  double sw = scale * 5.2381; //MeOS default assums this//getCurrentFont().getAvgFontWidth(*this, normalText);
  double other;
  if (fontFace == 0 || fontFace[0] == 0)
    other = getCurrentFont().getAvgFontWidth(*this, font);
  else
    other = getFont(fontFace).getAvgFontWidth(*this, font);
  return other/sw;
}

double GDIImplFontSet::getAvgFontWidth(const gdioutput &gdi, gdiFonts font) const {
  if (avgWidthCache.empty())
    avgWidthCache.resize(16, 0);

  if (size_t(font) > avgWidthCache.size())
    throw meosException("Internal font error");

  if (avgWidthCache[font] == 0) {
    TextInfo ti;
    ti.xp = 0;
    ti.yp = 0;
    ti.format = font;
    ti.text = L"Goliat Meze 1234:5678";
    ti.font = gdiName;
    gdi.calcStringSize(ti);
    avgWidthCache[font] = double(ti.textRect.right) / double(ti.text.length());

  }
  return avgWidthCache[font];
}

const wstring &gdioutput::getFontName(int id) {

  return _EmptyWString;
}

GDIImplFontEnum::GDIImplFontEnum() {
  relScale = 1.0;
}

GDIImplFontEnum::~GDIImplFontEnum() {
}

/*
FontEncoding interpetEncoding(const string &enc) {
  if (enc == "RUSSIAN")
    return Russian;
  else if (enc == "EASTEUROPE")
    return EastEurope;
  else if (enc == "HEBREW")
    return Hebrew;
  else
    return ANSI;
}*/

const string &gdioutput::narrow(const wstring &input) {
  string &output = StringCache::getInstance().get();
  output.clear();
  output.insert(output.begin(), input.begin(), input.end());
  return output; 
}

const wstring &gdioutput::widen(const string &input) {
  wstring &output = StringCache::getInstance().wget();
  int cp = 1252;
  if (input.empty()) {
    output = L"";
    return output;
  }
  output.reserve(input.size()+1);
  output.resize(input.size(), 0);
  MultiByteToWideChar(cp, MB_PRECOMPOSED, input.c_str(), input.size(), &output[0], output.size() * sizeof(wchar_t));
  return output;
}

const wstring &gdioutput::recodeToWide(const string &input) {
  wstring &output = StringCache::getInstance().wget();
  int cp = defaultCodePage;
 // if (defaultCodePage > 0)
 //   cp = defaultCodePage;

  /*switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/

  if (input.empty()) {
    output = L"";
    return output;
  }
  output.reserve(input.size()+1);
  output.resize(input.size(), 0);
  MultiByteToWideChar(cp, MB_PRECOMPOSED, input.c_str(), input.size(), &output[0], output.size() * sizeof(wchar_t));
  return output;
}

const string &gdioutput::recodeToNarrow(const wstring &input) {
  string &output = StringCache::getInstance().get();
  int cp = defaultCodePage;
 // if (defaultCodePage > 0)
 //   cp = defaultCodePage;

  /*switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/

  if (input.empty()) {
    output = "";
    return output;
  }
  int res = input.size() * 3 + 2;
  output.reserve(res);
  output.resize(input.size(), 0);
  BOOL usedDef = false;
  int ok = WideCharToMultiByte(cp, 0, input.c_str(), input.size(), &output[0], res, "?", &usedDef);

  return output;
}

const wstring &gdioutput::fromUTF8(const string &input) {
  wstring &output = StringCache::getInstance().wget();
  size_t alloc = input.length() + 1;
  output.resize(alloc);
  wchar_t *ptr = &output[0];
  int wlen = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), ptr, alloc);
  ptr[wlen] = 0;
  output.resize(wlen);
  return output;
}
const string &gdioutput::toUTF8(const wstring &winput)  {
  string &output = StringCache::getInstance().get();
  size_t alloc = winput.length()*4+32;
  output.resize(alloc);
  WideCharToMultiByte(CP_UTF8, 0, winput.c_str(), winput.length()+1, (char *)output.c_str(), alloc, 0, 0);
  output.resize(strlen(output.c_str()));
  return output;
}

void gdioutput::setListDescription(const wstring &desc) {
  listDescription = desc;
}

InputInfo &InputInfo::setFont(gdioutput &gdi, gdiFonts font) {
  SendMessage(hWnd, WM_SETFONT, (WPARAM) gdi.getCurrentFont().getFont(font), 0);
  return *this;
}

void gdioutput::copyToClipboard(const string &html, const wstring &txt) const {

  if (OpenClipboard(getHWNDMain()) != false) {
    EmptyClipboard();

    size_t len = html.length() + 1;
    const char *output = html.c_str();
    
    const char cbd[]=
      "Version:0.9\n"
      "StartHTML:%08u\n"
      "EndHTML:%08u\n"
      "StartFragment:%08u\n"
      "EndFragment:%08u\n";

    char head[256];
    sprintf_s(head, cbd, 1,0,0,0);

    int offset=strlen(head);
 
    //Fill header with relevant information
    int ho_start = offset;
    int ho_end = offset + len;
    sprintf_s(head, cbd, offset,offset+len,ho_start,ho_end);

    HANDLE hMem=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, offset+len);
    LPVOID data=GlobalLock(hMem);

    memcpy(LPSTR(data), head, offset);
    memcpy(LPSTR(data)+offset, output, len);
    
    GlobalUnlock(hMem);

    // Text format
    //HANDLE hMemText = 0;
    HANDLE hMemTextWide = 0;

    if (txt.length() > 0) {
      size_t siz = txt.length();
      hMemTextWide = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, siz * sizeof(wchar_t));
      LPVOID dataText = GlobalLock(hMemTextWide);
      memcpy(LPSTR(dataText), txt.c_str(), siz * sizeof(wchar_t));
      GlobalUnlock(hMemTextWide);


      /*
      hMemText = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, txt.length()+1);
      LPVOID dataText=GlobalLock(hMemText);
      memcpy(LPSTR(dataText), txt.c_str() , txt.length()+1);
      GlobalUnlock(hMemText);*/
    }
    else {
      // HTML table to text
      std::ostringstream result;
      bool started = false;
      bool newline = false;
      bool dowrite = false;
      for (size_t k = 0; k + 3 < html.size(); k++) {
        if (html[k] == '<') {
          if (html[k+1] == 't') {
            if (html[k+2] == 'r') {
              newline = true;
              if (started)
               result << "\r\n";
            }
            else if (html[k+2] == 'd') {
              if (!newline)
                result << "\t";
              started = true;
              newline = false;
              dowrite = true;
            }
          }
          else if (html[k+1] == '/') {
            if (html[k+2] == 't' && html[k+3] == 'd') {
              dowrite = false;
            }
          }
          while (k < html.size() && html[k] != '>')
            k++;
        }
        else {
          if (dowrite)
            result << html[k];
        }
      }

      string atext = decodeXML(result.str());
/*      result.flush();

      for (size_t k = 0; k < atext.size(); k++) {
        if (atext[k] == '&') {
          size_t m = 0;
          while ((k+m) < atext.size() && atext[k+m] != ';')
            m++;

          if ((k+m) < atext.size() && atext[k+m] == ';') {
            string cmd = atext.substr(k, m-k);
            if (cmd == "nbsp")
              result << " ";
            else if (cmd == "amp")
              result << " ";
            else if (cmd == "lt")
              result << "<";
            else if (cmd == "gt")
              result << ">";
            else if (cmd == "quot")
              result << "\"";
            
            k += m;
          }
        }
        else 
          result << atext[k];

      }

      atext = result.str();
*/
      if (atext.size() > 0) {
        wstring atextw;
        int osize = atext.size();
        atextw.resize(osize + 1, 0);
        size_t siz = atextw.size();
        MultiByteToWideChar(CP_UTF8, 0, atext.c_str(), -1, &atextw[0], siz * sizeof(wchar_t));
        hMemTextWide = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, siz * sizeof(wchar_t));
        LPVOID dataText = GlobalLock(hMemTextWide);
        memcpy(LPSTR(dataText), atextw.c_str(), siz * sizeof(wchar_t));
        GlobalUnlock(hMemTextWide);
      }
    }
    UINT CF_HTML = RegisterClipboardFormat(L"HTML format");
    SetClipboardData(CF_HTML, hMem);
    
    if (hMemTextWide != 0) {
      SetClipboardData(CF_UNICODETEXT, hMemTextWide);
    }
    CloseClipboard();
  }
}

Recorder &gdioutput::getRecorder() {
  if (recorder.first == 0) {
    recorder.first = new Recorder();
    recorder.second = true;
  }
  return *recorder.first;
}

void gdioutput::initRecorder(Recorder *rec) {
  if (recorder.second)
    delete recorder.first;

  recorder.first = rec;
  recorder.second = false;
}

string gdioutput::dbPress(const string &id, int extra) {
  bool notEnabled = false;
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id && (extra == -65536 || extra == it->getExtraInt())) {
      
      if (!IsWindowEnabled(it->hWnd)) {
        notEnabled = true;
        continue;
      }
        
      if (it->isCheckbox) {
        check(id, !isChecked(id));
      }
      else if(!it->callBack && !it->hasEventHandler())
        throw meosException("Button " + id + " is not active.");

      wstring val = it->text;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack)
        it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
      return toUTF8(val);
    }
  }
  if (notEnabled)
    throw meosException("Button " + id + " is not active.");
      
  throw meosException("Unknown command " + id + ".");
}

string gdioutput::dbPress(const string &id, const char *extra) {
  wstring eid = widen(extra ? extra : "");
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id && (!extra || (it->isExtraString() && eid == it->getExtra()))) {
      
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Button " + id + " is not active.");
      
      if (it->isCheckbox) {
        check(id, !isChecked(id));
      }
      else if(!it->callBack && !it->hasEventHandler())
        throw meosException("Button " + id + " is not active.");

      wstring val = it->text;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack)
        it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
      return toUTF8(val);
    }
  }
  throw meosException(L"Unknown command " + widen(id) + L"/" + eid + L".");
}


string gdioutput::dbSelect(const string &id, int data) {

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Selection " + id + " is not active.");
      if (it->multipleSelection) {
        auto res = it->data2Index.find(data);
        if (res != it->data2Index.end())
          SendMessage(it->hWnd, LB_SETSEL, true, res->second);
        else
          throw meosException("List " + id + " does not contain value " + itos(data) + ".");
      }
      else {
        size_t origIdx = it->originalIdx;
        wstring orig = it->original;
        if (!selectItemByData(id, data))
          throw meosException("List " + id + " does not contain value " + itos(data) + ".");
        it->original = orig;
        it->originalIdx = origIdx;
      }
      UpdateWindow(it->hWnd);
      wstring res = it->text;
      internalSelect(*it);
      return toUTF8(res);
    }
  }
  throw meosException("Unknown selection " + id + ".");
}

void gdioutput::internalSelect(ListBoxInfo &bi) {
  bi.syncData();
  if (bi.callBack || bi.handler || bi.managedHandler) {
    setWaitCursor(true);
    hasCleared = false;
    try {
      bi.writeLock = true;
      if (bi.hasEventHandler())
        bi.handleEvent(*this, GUI_LISTBOX);
      else
        bi.callBack(this, GUI_LISTBOX, &bi); //it may be destroyed here... Then hasCleared is set.
    }
    catch(...) {
      if (!hasCleared)
        bi.writeLock = false;
      setWaitCursor(false);
      throw;
    }
    if (!hasCleared)
      bi.writeLock = false;
    setWaitCursor(false);
  }
}

void gdioutput::dbInput(const string &id, const string &text) {
  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd) || !it->IsCombo)
        throw meosException("Selection " + id + " is not active.");

      SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
      SetWindowText(it->hWnd, widen(text).c_str());
      it->text = widen(text);
      it->data = -1;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_COMBO);
      else if (it->callBack)
        it->callBack(this, GUI_COMBO, &*it); //it may be destroyed here...
      return;
    }
  }

  for (list<InputInfo>::iterator it = II.begin(); it != II.end(); ++it) {
    if (id == it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Input " + id + " is not active.");

      it->text = widen(text);
      SetWindowText(it->hWnd, widen(text).c_str());
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_INPUT);
      else if (it->callBack)
        it->callBack(this, GUI_INPUT, &*it);
      return;
    }
  }

  throw meosException("Unknown input " + id + ".");
}

void gdioutput::dbCheck(const string &id, bool state) {

}

string gdioutput::dbClick(const string &id, int extra) {
  for (list<TextInfo>::iterator it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id && (extra == -65536 || it->getExtraInt() == extra)) {
      if (it->callBack || it->hasEventHandler()) {
        string res = toUTF8(it->text);
        if (!it->handleEvent(*this, GUI_LINK))
          it->callBack(this, GUI_LINK, &*it);
        return res;
      }
      else
        throw meosException("Link " + id + " is not active.");
    }
  }
  
  throw meosException("Unknown link " + id + ".");
}

void gdioutput::dbDblClick(const string &id, int data) {
  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Selection " + id + " is not active.");
      selectItemByData(id, data);
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_LISTBOXSELECT);
      else if (it->callBack)
        it->callBack(this, GUI_LISTBOXSELECT, &*it); //it may be destroyed here...
      return;
    }
  }
  throw meosException("Unknown selection " + id + ".");
}

// Add the next answer for a dialog popup
void gdioutput::dbPushDialogAnswer(const string &answer) {
  cmdAnswers.push_back(answer);
}

void gdioutput::clearDialogAnswers(bool checkEmpty) {
  if (!cmdAnswers.empty()) {
    string front = cmdAnswers.front();
    cmdAnswers.clear();
    if (checkEmpty)
      throw meosException("Pending answer: X#" + front);
  }
}

int gdioutput::dbGetStringCount(const string &str, bool subString) const {
  int count = 0;
  wstring wstr = widen(str);
  for (list<TextInfo>::const_iterator it = TL.begin(); it != TL.end(); ++it) {
    if (subString == false) {
      if (it->text == wstr)
        count++;
    }
    else {
      size_t off = 0;
      while(off < it->text.size()) {
        off = it->text.find(wstr, off);
        if (off != string::npos) {
          count++;
          off++;
        }
        else
          break;
      }
    }
  }
  return count;
}

void gdioutput::dbRegisterSubCommand(const SubCommand *cmd, const string &action) {
  if (cmd == 0)
    subCommands.clear();
  else
    subCommands.push_back(make_pair(cmd, action));
}

void gdioutput::runSubCommand() {
  if (!subCommands.empty()) {
    auto cmd = subCommands.back();
    subCommands.pop_back();
    cmd.first->subCommand(cmd.second);
  }
}

void gdioutput::getWindowsPosition(RECT &rc) const {
  WINDOWPLACEMENT wpl;
  memset(&wpl, 0, sizeof(WINDOWPLACEMENT));
  wpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hWndAppMain, &wpl);
  rc = wpl.rcNormalPosition;
}

void gdioutput::setWindowsPosition(const RECT &rc) {
  WINDOWPLACEMENT wpl;
  memset(&wpl, 0, sizeof(WINDOWPLACEMENT));
  wpl.length = sizeof(WINDOWPLACEMENT);
  wpl.rcNormalPosition = rc;
  wpl.showCmd = SW_SHOWNORMAL;
  SetWindowPlacement(hWndAppMain, &wpl);
}

void gdioutput::getVirtualScreenSize(RECT &rc) {
  int px = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  if (px < 10 || px > 100000)
    px = GetSystemMetrics(SM_CXSCREEN);

  int py = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (py < 10 || py > 100000)
    py = GetSystemMetrics(SM_CYSCREEN);

  rc.left = 0;
  rc.right = px;
  rc.top = 0;
  rc.bottom = py;
}

DWORD gdioutput::selectColor(wstring &def, DWORD input) {
  CHOOSECOLOR cc;
  memset(&cc, 0, sizeof(cc));
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = getHWNDMain();
  cc.rgbResult = COLORREF(input);
  if (GDICOLOR(input) != colorDefault)
    cc.Flags |= CC_RGBINIT;

  COLORREF staticColor[16];
  memset(staticColor, 0, 16 * sizeof(COLORREF));

  const wchar_t *end = def.c_str() + def.length();
  const wchar_t * pEnd = def.c_str();
  int pix = 0;
  while (pEnd < end && pix < 16) {
    staticColor[pix++] = wcstol(pEnd, (wchar_t **)&pEnd, 16);
  }

  cc.lpCustColors = staticColor;
  int res = 0;
  setCommandLock();
  try {
    res = ChooseColor(&cc);
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (res) {
    wstring co;
    for (int ix = 0; ix < 16; ix++) {
      wchar_t bf[16];
      swprintf_s(bf, L"%x ", staticColor[ix]);
      co += bf;
    }
    swap(def,co);
    return cc.rgbResult;
  }
  return -1;
}

void gdioutput::setAnimationMode(const shared_ptr<AnimationData> &data) {
  if (animationData && animationData->takeOver(data))
    return;
  animationData = data;
}

namespace {
  BOOL CALLBACK enumMonitors(HMONITOR hMonitor, HDC hDC, LPRECT rect, LPARAM gdiObj) {
    gdioutput* gdi = reinterpret_cast<gdioutput*>(gdiObj);
    gdi->addMonitorRect(*rect);
    return true;
  }
}

void gdioutput::updateMonitorConfiguration() {
  monitorConfiguration.clear();
  EnumDisplayMonitors(NULL, NULL, enumMonitors, LPARAM(this));

  if (monitorConfiguration.size() > 0) {
    RECT rc;
    GetWindowRect(hWndAppMain, &rc);
    double showArea = 0;
    for (auto& mRC : monitorConfiguration) {
      RECT dst;
      IntersectRect(&dst, &mRC, &rc);
      double area = fabs(dst.right - dst.left) * fabs(dst.bottom - dst.top);
      showArea += area;
    }

    double totArea = fabs(rc.right - rc.left) * fabs(rc.bottom - rc.top);

    if (showArea < 0.33 * totArea) {
      HWND hDskTop = GetDesktopWindow();
      GetClientRect(hDskTop, &rc);

      // Out of bounds, just use default position and size
      int xp = 50;
      int yp = 20;
      int xs = max(850, min<int>(int(rc.right) - yp, (rc.right * 9) / 10));
      int ys = max(650, min<int>(int(rc.bottom) - yp - 40, (rc.bottom * 8) / 10));
      SetWindowPos(hWndAppMain, NULL, xp, yp, xs, ys, SWP_NOZORDER);
    }

  }
}

AutoCompleteInfo &gdioutput::addAutoComplete(const string &key) {
  BaseInfo &bi = getBaseInfo(key.c_str());
  RECT rc, rcMain;
  GetWindowRect(bi.getControlWindow(), &rc);
  GetWindowRect(hWndTarget, &rcMain);
  POINT pt;
  int height = scaleLength(200);
  pt.x = rc.right;
  //pt.y = min(rc.top, rcMain.bottom-height);
  pt.y = rc.bottom;
  if (pt.y + height > rcMain.bottom)
    pt.y = rc.top - height;
  
  ScreenToClient(hWndTarget, &pt);
  if (pt.y < 0) { //Fallback
    pt.x = rc.right;
    pt.y = min(rc.top, rcMain.bottom - height);
    ScreenToClient(hWndTarget, &pt);
  }

  if (autoCompleteInfo && autoCompleteInfo->matchKey(key)) {
    return *autoCompleteInfo;
  }

  autoCompleteInfo.reset();

  HWND hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"AUTOCOMPLETE", L"", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS| WS_BORDER ,
    pt.x, pt.y, scaleLength(350), height, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  autoCompleteInfo.reset(new AutoCompleteInfo(hWnd, key, *this));
  
  //SendMessage(hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);

  return *autoCompleteInfo;
}

void gdioutput::clearAutoComplete(const string &key) {
  autoCompleteInfo.reset();
}

int gdioutput::getPageY() const {
  if (hideBG || backgroundColor1 != -1)
    return max(MaxY, 100);
  else
    return max(MaxY, 100) + scaleLength(60); 
}

int gdioutput::getPageX() const { 
  int xlimit = 100;
  for (auto &b : BI)
    xlimit = max(b.xp + b.width, xlimit);

  if (hideBG || backgroundColor1 != -1 || xlimit >= MaxX)
    return max(MaxX, xlimit);
  else
    return max(MaxX, xlimit) + scaleLength(60); 
}

int gdioutput::popupMenu(int x, int y, const vector<pair<wstring, int>> &menuItems) const {
  POINT pt;
  pt.x = x;
  pt.y = y;
  ClientToScreen(getHWNDTarget(), &pt);
  HMENU hm = CreatePopupMenu();
  for (auto &me : menuItems) {
    if (me.first.empty())
      AppendMenu(hm, MF_SEPARATOR, me.second, L"");
    else
      AppendMenu(hm, MF_STRING, me.second, lang.tl(me.first).c_str());
  }
  int res = TrackPopupMenuEx(hm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, getHWNDTarget(), nullptr);

  DestroyMenu(hm);
  return res;
}
