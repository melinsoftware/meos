/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#include "meosexception.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "gdiimpl.h"
#include "Printer.h"

#define HPDF_DLL
#include "hpdf.h"

#include "pdfwriter.h"

wstring getMeosCompectVersion();

void  __stdcall pdfErrorhandler(HPDF_STATUS errorNo,
                                HPDF_STATUS detailNo,
                                void *user_data) {
  char bf[128];
  sprintf_s(bf, "PDF ERROR: error_no=%04X, detail_no=%u\n", (HPDF_UINT)errorNo,
                (HPDF_UINT)detailNo);
  throw meosException(bf);
}

const char* stdText = "Mr. Fantom of 43 Years";

bool pdfwriter::getFontData(const HFONT fontHandle, std::vector<char>& data, int &width) {
  bool result = false;
  HDC hdc = CreateCompatibleDC(NULL);
  if (hdc != NULL) {
    SelectObject(hdc, fontHandle);
    SIZE s;
    GetTextExtentPoint32A(hdc, stdText, strlen(stdText), &s);
    width = s.cx;
    const size_t size = GetFontData(hdc, 0, 0, NULL, 0);
    if (size > 0) {
      char* buffer = new char[size];
      if (GetFontData(hdc, 0, 0, buffer, size) == size) {
        data.resize(size);
        memcpy(&data[0], buffer, size);
        result = true;
      }
      delete[] buffer;
    }
    DeleteDC(hdc);
  }
  return result;
}

HPDF_Font pdfwriter::getPDFFont(HFONT font, float hFontScale, wstring &tmp, float &fontScale) {
  fontScale = 1.0;
  vector<char> data;
  int stdSize;
  if (getFontData(font, data, stdSize)) {
    tmp = getTempFile();
    string ntmp(tmp.begin(), tmp.end()); //XXX WCS
    ofstream out(tmp.c_str(), ios::binary|ios::out|ios::trunc);
    out.write(&data[0], data.size());
    out.close();
    const char *detailName = HPDF_LoadTTFontFromFile(pdf, ntmp.c_str(), HPDF_TRUE);
    HPDF_Font font = HPDF_GetFont (pdf, detailName, "UTF-8");

    HPDF_TextWidth res = HPDF_Font_TextWidth(font, (HPDF_BYTE *)stdText, strlen(stdText));
    if (res.width > 0)
      fontScale = float(stdSize) / float(hFontScale * res.width * 0.012f);
    else
      return 0;

    return font;
  }
  return 0;
}

void pdfwriter::selectFont(HPDF_Page page, const PDFFontSet &fs, int format, float scale)  {
  format &= 0xFF;
  if (format==0 || format==10) {
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==fontMedium){
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==1){
    HPDF_Page_SetFontAndSize (page, fs.fontBold, fs.fontScaleBold*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==boldLarge){
    HPDF_Page_SetFontAndSize (page, fs.fontBold, fs.fontScaleBold*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==boldHuge){
    HPDF_Page_SetFontAndSize (page, fs.fontBold, fs.fontScaleBold*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==boldSmall){
    HPDF_Page_SetFontAndSize (page, fs.fontBold, fs.fontScaleBold*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==fontLarge){
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==fontMediumPlus){
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==fontSmall){
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==italicSmall){
    HPDF_Page_SetFontAndSize (page, fs.fontItalic, fs.fontScaleItalic*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==italicText){
    HPDF_Page_SetFontAndSize (page, fs.fontItalic, fs.fontScaleItalic*GDIImplFontSet::baseSize(format, scale));
  }
  else if (format==italicMediumPlus){
    HPDF_Page_SetFontAndSize (page, fs.fontItalic, fs.fontScaleItalic*GDIImplFontSet::baseSize(format, scale));
  }
  else {
    HPDF_Page_SetFontAndSize (page, fs.font, fs.fontScale*GDIImplFontSet::baseSize(format, scale));
  }
}

const float fontFromGdiScale(double gdiScale) {
  double f = max(1.0, min(gdiScale, 3.0))-1.0;
  double s = 1.05 + 0.75*f;
  return float(s);
}

void pdfwriter::generatePDF(const gdioutput &gdi,
                            const wstring &file,
                            const wstring &pageTitleW,
                            const wstring &authorW,
                            const list<TextInfo> &tl,
                            bool respectPageBreak) {
  checkWriteAccess(file);
  string pageTitle = gdi.narrow(pageTitleW); // XXX WCS
  string author = gdi.narrow(authorW);

  pdf = HPDF_New(pdfErrorhandler, 0);
  if (!pdf)
    pdfErrorhandler(-1, -1, 0);
  HPDF_UseUTFEncodings(pdf);

  // Set compression mode
  HPDF_SetCompressionMode (pdf, HPDF_COMP_ALL);
  string creator = "MeOS " + gdi.toUTF8(getMeosCompectVersion());
  HPDF_SetInfoAttr(pdf, HPDF_INFO_CREATOR, creator.c_str());
  HPDF_SetInfoAttr(pdf, HPDF_INFO_TITLE, pageTitle.c_str());

  // Map font name to pdf font sets
  map<wstring, PDFFontSet> fonts;

  // Create default-font
  PDFFontSet &fs = fonts[L""];
  {
    FontInfo fi;
    TextInfo ti;
    ti.format = 0;
    ti.font = L"Arial";
    gdi.getFontInfo(ti, fi);
    float scale;
    wstring tmp;
    fs.font = getPDFFont(fi.normal, 1.0, tmp, scale);
    fs.fontScale = scale;
    fs.fontBold = getPDFFont(fi.bold, 1.0, tmp, scale);
    fs.fontScaleBold = scale;
    fs.fontItalic = getPDFFont(fi.italic, 1.0, tmp, scale);
    fs.fontScaleItalic = scale;  
  }

  fs.fontScale = 0.9f;
  fs.fontScaleBold = 0.9f;
  fs.fontScaleItalic = 0.9f;

  // Add a new page object.
  HPDF_Page page = HPDF_AddPage (pdf);

  // Set page size
  HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

  float maxX = 0;
  for (list<TextInfo>::const_iterator it = tl.begin(); it != tl.end(); ++it) {
    if (gdioutput::skipTextRender(it->format))
      continue;

    maxX = max(maxX, (float)it->textRect.right);
  }
  const float scaleXFactor = 1.2f;
  maxX *= scaleXFactor;
  float w = HPDF_Page_GetWidth(page);
  float h = HPDF_Page_GetHeight(page);
  float scale = (w / maxX) * 0.95f;
  
  double gdiScale = gdi.getScale();
  const float fontScale = fontFromGdiScale(gdiScale);

  vector<RenderedPage> pages;
  PageInfo pageInfo;
  pageInfo.topMargin = h * 0.03f;
  pageInfo.scaleX = scale * scaleXFactor;
  pageInfo.scaleY = scale * 1.1f;
  pageInfo.leftMargin = w * 0.03f;
  pageInfo.bottomMargin = pageInfo.topMargin * 1.0f;
  pageInfo.pageY = h;
  pageInfo.printHeader = true;
  pageInfo.yMM2PrintC = pageInfo.xMM2PrintC = 1199.551f / 420.f;
  pageInfo.yMM2PrintC *= fontScale;
  pageInfo.xMM2PrintK = 0;
  pageInfo.yMM2PrintK = 0;
  
  list<RectangleInfo> rectangles;
  pageInfo.renderPages(tl, rectangles, true, respectPageBreak, pages);
  for (size_t j = 0; j< pages.size(); j++) {
    wstring pinfo = pageInfo.pageInfo(pages[j]);
    if (!pinfo.empty()) {
      selectFont(page, fs, fontSmall, scale);
      HPDF_Page_BeginText (page);
      float df = min(w, h) * 0.04f;
      float sw = HPDF_Page_TextWidth(page, gdi.toUTF8(pinfo).c_str());
      float sy = HPDF_Page_TextWidth(page, "MMM");

      HPDF_Page_TextOut (page, w - sw - df , h - sy, gdi.toUTF8(pinfo).c_str());
      HPDF_Page_EndText(page);
    }

    vector<PrintTextInfo> &info = pages[j].text;

    for (size_t k = 0; k < info.size(); k++) {
      if (fonts.count(info[k].ti.font) == 0) {
        FontInfo fi;
        gdi.getFontInfo(info[k].ti, fi);
        float fontScaleLoc;
        wstring tmpFile;
        fonts[info[k].ti.font] = fs; //Default fallback
        PDFFontSet &f = fonts[info[k].ti.font];

        HPDF_Font font = getPDFFont(fi.normal, float(gdi.getScale()), tmpFile, fontScaleLoc);
        if (!tmpFile.empty())
          tmpFiles.push_back(tmpFile);
        if (font) {
          f.font = font;
          f.fontScale = fontScaleLoc;
        }

        font = getPDFFont(fi.italic, float(gdi.getScale()), tmpFile, fontScaleLoc);
        if (!tmpFile.empty())
          tmpFiles.push_back(tmpFile);
        if (font) {
          f.fontItalic = font;
          f.fontScaleItalic = fontScaleLoc;
        }

        font = getPDFFont(fi.bold, (float)gdi.getScale(), tmpFile, fontScaleLoc);
        if (!tmpFile.empty())
          tmpFiles.push_back(tmpFile);
        if (font) {
          f.fontBold = font;
          f.fontScaleBold = fontScaleLoc;
        }
      }

      selectFont(page, fonts[info[k].ti.font], info[k].ti.format, scale*fontScale);
      HPDF_Page_BeginText (page);
      float r = GetRValue(info[k].ti.color);
      float g = GetGValue(info[k].ti.color);
      float b = GetBValue(info[k].ti.color);
      HPDF_Page_SetRGBFill (page, r/255.0f, g/255.0f, b/255.0f);
      string nt = gdi.toUTF8(info[k].ti.text);
        
      if (info[k].ti.format & textRight) {
        float w = float(info[k].ti.xlimit) * scale*fontScale;
        float sw = HPDF_Page_TextWidth(page, nt.c_str());
        float space = info[k].ti.xlimit > 0 ? 2 * HPDF_Page_GetCharSpace(page) : 0;
        HPDF_Page_TextOut (page, info[k].xp + w - sw - space, h - info[k].yp,
                            nt.c_str());
      }
      else if (info[k].ti.format & textCenter) {
        float w = float(info[k].ti.xlimit) * scale*fontScale;
        float sw = HPDF_Page_TextWidth(page, nt.c_str());
        HPDF_Page_TextOut (page, info[k].xp + w - sw/2, h - info[k].yp,
                            nt.c_str());
      }
      else {
        HPDF_Page_TextOut (page, info[k].xp, h - info[k].yp, nt.c_str());
      }
      HPDF_Page_EndText (page);
    }
    if (j+1 < pages.size()) {
      // Add a new page object.
      page = HPDF_AddPage (pdf);

      // Set page size
      HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
      HPDF_Page_SetFontAndSize (page, fs.font, 16);
    }
  }

  // Save the document to a file
  wstring tmpResW = getTempFile();
  string tmpRes(tmpResW.begin(), tmpResW.end());
  HPDF_SaveToFile (pdf, tmpRes.c_str());
  DeleteFileW(file.c_str());
  BOOL res = MoveFile(tmpResW.c_str(), file.c_str());
  removeTempFile(tmpResW);

  if (!res) {
    throw meosException("Failed to save pdf the specified file.");
  }
  return;
}

pdfwriter::pdfwriter() {
  pdf = 0;
}

pdfwriter::~pdfwriter() {
  // Clean up
  if (pdf)
    HPDF_Free(pdf);
  while(!tmpFiles.empty()) {
    removeTempFile(tmpFiles.back());
    tmpFiles.pop_back();
  }
}
