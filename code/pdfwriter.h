/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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

#ifndef _HPDF_H
typedef void         *HPDF_HANDLE;
typedef HPDF_HANDLE   HPDF_Doc;
typedef HPDF_HANDLE   HPDF_Page;
typedef HPDF_HANDLE   HPDF_Font;
#endif

class pdfwriter  {

  struct PDFFontSet {
    HPDF_Font font;
    float fontScale;
    HPDF_Font fontBold;
    float fontScaleBold;
    HPDF_Font fontItalic;
    float fontScaleItalic;
    PDFFontSet() : font(0), fontScale(1), fontBold(0), fontScaleBold(1), fontItalic(0), fontScaleItalic(1) {}
  };

  protected:

    bool getFontData(const HFONT fontHandle, std::vector<char>& data, int &width);
    HPDF_Font getPDFFont(HFONT font, float hFontScale, wstring &tmp, float &fontScale);
    void selectFont(HPDF_Page page, const PDFFontSet &fs, int format, float scale);
    vector<wstring> tmpFiles;
    HPDF_Doc pdf;
  public:
    pdfwriter();
    ~pdfwriter();

    void generatePDF(const gdioutput &gdi,
                     const wstring &file,
                     const wstring &pageTitle,
                     const wstring &author,
                     const list<TextInfo> &tl,
                     bool respectPageBreak);
};
