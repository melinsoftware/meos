// printer.h: printing utilities.

#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include "gdistructures.h"

/** Data structure describing text to print.*/
struct PrintTextInfo {
  float xp;
  float yp;
  float width;
  TextInfo ti;
  PrintTextInfo(const TextInfo &ti_) : xp(0), yp(0), width(0), ti(ti_) {};
  PrintTextInfo() : xp(0), yp(0), width(0) {};
};

/** Data structure describing page to print*/
struct PageInfo {
  float topMargin;
  float bottomMargin;
  float pageY;
  float leftMargin;
  float scaleX;
  float scaleY;

  bool printHeader;
  bool noPrintMargin;
  int nPagesTotal; //Total number of pages to print

  // Transfer mm to local printing coordinates: cLocalX = cx + m, cLocalX = cy + m.
  double xMM2PrintC;
  double xMM2PrintK;

  double yMM2PrintC;
  double yMM2PrintK;

  void renderPages(const list<TextInfo> &tl,
                   const list<RectangleInfo> &rects,
                   bool invertHeightY,
                   bool respectPageBreak,
                   vector<RenderedPage> &pages);

  wstring pageInfo(const RenderedPage &page) const;
};

/** A rendered page ready to print. */
struct RenderedPage {
  int nPage; // This page number
  wstring info;
  bool startChapter = false;
  vector<PrintTextInfo> text;
  vector<RectangleInfo> rectangles;
  __int64 checkSum;

  RenderedPage() : checkSum(0) {}
  void calculateCS(const TextInfo &text);
};

struct PrinterObject {
  //Printing
  HDC hDC;
  HGLOBAL hDevMode;
  HGLOBAL hDevNames;

  void freePrinter();

  wstring Device;
  wstring Driver;
  DEVMODE DevMode;
  set<__int64> printedPages;
  int nPagesPrinted;
  int nPagesPrintedTotal;
  bool onlyChanged;

  struct DATASET {
    int pWidth_mm = 0;
    int pHeight_mm = 0;
    double pMgBottom = 0.0;
    double pMgTop = 0.0;
    double pMgRight = 0.0;
    double pMgLeft = 0.0;

    int MarginX = 0;
    int MarginY = 0;
    int PageX = 0;
    int PageY = 0;
    double Scale = 0.0;
    bool LastPage = false;
  } ds;

  void operator=(const PrinterObject &po);

  PrinterObject();
  ~PrinterObject();
  PrinterObject(const PrinterObject &po);
};
