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

#pragma once

class gdioutput;
struct FontInfo;

class GDIImplFontSet {
  HFONT Huge;
  HFONT Large;
  HFONT Medium;
  HFONT Small;

  HFONT pfLarge;
  HFONT pfMedium;
  HFONT pfSmall;
  HFONT pfMediumPlus;
  HFONT pfMono;

  HFONT pfSmallItalic;
  HFONT pfItalicMediumPlus;
  HFONT pfItalic;
  void deleteFonts();

  wstring gdiName;
  mutable vector<double> avgWidthCache;
public:
  static float baseSize(int format, float scale);
  void getInfo(FontInfo &fi) const;

  GDIImplFontSet();
  virtual ~GDIImplFontSet();
  void init(double scale, const wstring &font, const wstring &gdiName);
  void selectFont(HDC hDC, int format) const;
  HFONT getGUIFont() const {return pfMedium;}
  HFONT getFont(int format) const;
  double getAvgFontWidth(const gdioutput &gdi, gdiFonts font) const;
};

class GDIImplFontEnum {
private:
  int width;
  int height;
  double relScale;
  wstring face;
public:
  GDIImplFontEnum();
  virtual ~GDIImplFontEnum();

  const wstring &getFace() const {return face;}
  double getRelScale() const {return relScale;}

  friend int CALLBACK enumFontProc(const LOGFONT* logFont, const TEXTMETRIC *metric, DWORD id, LPARAM lParam);
};

