﻿/************************************************************************
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

#include "stdafx.h"
#include "gdioutput.h"
#include <vector>
#include <map>
#include <cassert>
#include <algorithm>
#include <cmath>
#include "meos_util.h"
#include "Localizer.h"
#include "gdiconstants.h"
#include "HTMLWriter.h"
#include "Printer.h"
#include "oListInfo.h"
#include "meosexception.h"
#include "image.h"

extern Image image;

#include <sstream>
#include <iomanip>

using namespace std;

double getLocalScale(const wstring &fontName, wstring &faceName);
wstring getMeosCompectVersion();

map <string, shared_ptr<HTMLWriter>> HTMLWriter::tCache;
extern wchar_t exePath[MAX_PATH];

static string getColor(int color) {
  int r = color & 0xFF;
  int g = (color >> 8) & 0xFF;
  int b = (color >> 16) & 0xFF;
  char bf[32];
  sprintf_s(bf, "%02x%02x%02x", r, g, b);
  return bf;
}

static void generateStyles(const gdioutput &gdi, std::ostream &fout, double scale, bool withTbl, const list<TextInfo> &TL,
                           map<pair<gdiFonts, wstring>, pair<string, string>> &styles) {
  fout << "<style type=\"text/css\">\n";
  if (gdi.hasBGColor())
    fout << "body {background-color: #" << getColor(gdi.getBGColor()) << "}\n";
  else
    fout << "body {background-coloro: rgb(250,250,255)}\n";

  string tcolor;
  if (gdi.hasFGColor())
    tcolor = ";color: #" + getColor(gdi.getFGColor());

  fout << "h1 {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 24 * scale << 
           "px;font-weight:normal;white-space:nowrap" << tcolor <<  "}\n";
  fout << "h2 {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 20 * scale <<
           "px;font-weight:normal;white-space:nowrap" << tcolor <<  "}\n";
  fout << "h3 {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 16 * scale << 
           "px;font-weight:normal;white-space:nowrap" << tcolor << "}\n";
  fout << "p {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 12 * scale << 
           "px;font-weight:normal" << tcolor << "}\n";
  fout << "div {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 12 * scale << 
           "px;font-weight:normal;white-space:nowrap" << tcolor << "}\n";

  if (withTbl) {
    fout << "td {font-family:arial,sans-serif;font-size:" << std::fixed << std::setprecision(2) << 12 * scale <<
            "px;font-weight:normal;white-space:nowrap" << tcolor << "}\n";
    if (gdi.hasBGColor()) {
      int bgRGB = gdi.getBGColor();
      HLS bgHLS;
      bgHLS.RGBtoHLS(bgRGB);
      HLS e1(bgHLS), e2(bgHLS);

      if (bgHLS.lightness > 180) {
        e1.lighten(0.98);
        e2.lighten(1.02);
      }
      if (bgHLS.lightness > 80) {
        e1.lighten(0.9);
        e2.lighten(1.1);
      }
      else {
        e1.lightness += 30;
        e2.lightness += 45;
      }

      fout << "td.e0 {background-color: #" << getColor(e1.HLStoRGB()) << "}\n";
      fout << "td.e1 {background-color: #" << getColor(e2.HLStoRGB()) << "}\n";
    }
    else {
      fout << "td.e0 {background-color: rgb(238,238,255)}\n";
      fout << "td.e1 {background-color: rgb(245,245,255)}\n";
    }

    fout << "td.header {line-height:" << std::fixed << std::setprecision(2) << 1.8 * scale << ";height:" 
      << std::fixed << std::setprecision(2) << 40 * scale << "px}\n";
    fout << "td.freeheader {line-height:" << std::fixed << std::setprecision(2) << 1.2 * scale << "}\n";
  }
  list<TextInfo>::const_iterator it=TL.begin();
  int styleList = 1;
  while (it != TL.end()) {
    gdiFonts font = it->getGdiFont();

    if (!it->font.empty() || (font == italicMediumPlus)
                          || (font == fontMediumPlus)) {

      if (styles.find(make_pair(font, it->font)) != styles.end()) {
        ++it;
        continue;
      }

      string style = "sty" + itos(styleList++);
      string element = "div";
      double baseSize = 12;
      switch (font) {
        case boldHuge:
          element = "h1";
          baseSize = 24;
          break;
        case boldLarge:
        case fontLarge:
          element = "h2";
          baseSize = 20;
          break;
        case fontMedium:
          element = "h3";
          baseSize = 16;
          break;
        case fontMediumPlus:
        case italicMediumPlus:
          baseSize = 18;
          break;
        case fontSmall:
        case italicSmall:
        case boldSmall:
          baseSize = 10;
         break;
      }

      wstring faceName;
      double iscale = 1.0;
      if (it->font.empty()) {
        faceName = L"arial,sans-serif";
      }
      else
        iscale = getLocalScale(it->font, faceName);

      fout << element << "." <<  style
           << "{font-family:" << gdi.narrow(faceName) << ";font-size:"
           << fixed << std::setprecision(2) << (scale * iscale * baseSize) 
           << "px;font-weight:normal;white-space:nowrap" << tcolor << "}\n";

      styles[make_pair(font, it->font)] = make_pair(element, style);
    }
    ++it;
  }
  fout << "</style>\n";
}

class InterpTextInfo {
public:
    static int x(const TextInfo &ti) {
      return ti.xp;
    }

    static int y(const TextInfo &ti) {
      return ti.yp;
    }

    static const TextInfo &ti(const TextInfo &in_ti) {
      return in_ti;
    }
};

template<typename T, typename TI>
void HTMLWriter::formatTL(ostream &fout,
                          ImageWriter& imageWriter,
                          const map<pair<gdiFonts, wstring>, pair<string, string>> &styles,
                          const T &tl,
                          double &yscale, double &xscale,
                          int &offsetY, int &offsetX) {


  auto itt = tl.begin();
  while (itt != tl.end()) {
    auto &ctr_it = *itt;
    const TextInfo &it = TI::ti(ctr_it);

    if (gdioutput::skipTextRender(it.format)) {
      ++itt;
      continue;
    }

    string yp = itos(int(yscale*TI::y(ctr_it)) + offsetY);
    string xp = itos(int(xscale*TI::x(ctr_it)) + offsetX);

    if ((it.format & 0xFF) == textImage) {
      int imgW = int((it.textRect.right - it.textRect.left) * xscale);
      int imgH = int((it.textRect.bottom - it.textRect.top) * xscale);
      imageWriter.write(fout, xp, yp, it.text, imgW, imgH);
      ++itt;
      continue;
    }

    string estyle;
    if (it.format != 1 && it.format != boldSmall) {
      if (it.format & textRight)
        estyle = " style=\"position:absolute;left:" +
        xp + "px;top:" + yp + "px\"";
      else
        estyle = " style=\"position:absolute;left:" +
        xp + "px;top:" + yp + "px\"";

    }
    else {
      if (it.format & textRight)
        estyle = " style=\"font-weight:bold;position:absolute;left:" +
        xp + "px;top:" + yp + "px\"";
      else
        estyle = " style=\"font-weight:bold;position:absolute;left:" +
        xp + "px;top:" + yp + "px\"";
    }
    string starttag, endtag;
    getStyle(styles, it, estyle, starttag, endtag);

    if (!it.text.empty())
      fout << starttag << gdioutput::toUTF8(encodeXML(it.text)) << endtag << endl;

    if (it.format == boldLarge) {
      auto next = itt;
      ++next;
      if (next == tl.end() || next->yp != it.yp)
        offsetY += 7;
    }
    ++itt;
  }
}

static void getStyle(const map< pair<gdiFonts, wstring>, pair<string, string>> &styles,
                     const TextInfo &ti, const string &extraStyle, string &starttag, string &endtag) {
  starttag.clear();
  endtag.clear();
  string extra;
  switch (ti.getGdiFont()) {
    case boldText:
    case boldSmall:
      extra = "b";
      break;
    case italicSmall:
    case italicText:
    case italicMediumPlus:
      extra = "i";
      break;
  }

  string colorStyle;
  if (ti.color != 0) {
    string color = getColor(ti.color);
    if (!extraStyle.empty()) {
      colorStyle = extraStyle;
      colorStyle.back() = ';';
    }
    else
      colorStyle = " style=\"";

    colorStyle += string("color: #") + color + "\"";
  }
  pair<gdiFonts, wstring> key(ti.getGdiFont(), ti.font);
  auto res = styles.find(key);

  if (res != styles.end()) {
    auto &stylePair = res->second;

    if (!stylePair.first.empty()) {
      starttag = "<" + stylePair.first;
      if (!stylePair.second.empty())
        starttag += " class=\""  + stylePair.second + "\"";
      
      if (!colorStyle.empty())
        starttag += colorStyle;
      else
        starttag += extraStyle;
      starttag += ">";
    }

    if (!extra.empty()) {
      starttag += "<" + extra + ">";
      endtag = "</" + extra + ">";
    }

    if (!stylePair.first.empty()) {
      endtag += "</" + stylePair.first + ">";
    }
  }
  else {
    string element;
    switch(ti.getGdiFont()) {
      case boldHuge:
        element="h1";
        break;
      case boldLarge:
        element="h2";
        break;
      case fontLarge:
        element="h2";
        break;
      case fontMedium:
        element="h3";
        break;
    }

    if ((!extraStyle.empty() || !colorStyle.empty()) && element.empty() )
      element = "div";

    if (element.size()>0) {
      starttag = "<" + element;
      if (!colorStyle.empty())
        starttag += colorStyle;
      else
        starttag += extraStyle;
      starttag += ">";
    }

    if (!extra.empty()) {
      starttag += "<" + extra + ">";
      endtag = "</" + extra + ">";
    }

    if (element.size()>0) {
      endtag += "</" + element + ">";
    }
  }
}

void HTMLWriter::writeHTML(gdioutput &gdi, const wstring &file, 
                           const wstring &title, int refreshTimeOut, double scale){
  checkWriteAccess(file);
  ofstream fout(file.c_str());
  if (fout.bad())
    throw std::exception("Bad output stream");
  
  wchar_t drive[20];
  wchar_t dir[MAX_PATH];
  wchar_t name[MAX_PATH];
  wchar_t ext[MAX_PATH];
  _wsplitpath_s(file.c_str(), drive, dir, name, ext);
  wstring path = wstring(drive) + dir;
  
  writeHTML(gdi, fout, title, true, path, refreshTimeOut, scale);
}

void HTMLWriter::writeHTML(gdioutput& gdi, ostream& fout,
                           const wstring& title, 
                           bool includeImages,
                           const wstring& imageDirectoryDestination, 
                           int refreshTimeOut, double scale) {

  ImageWriter imgWriter(imageDirectoryDestination, includeImages);

  if (scale <= 0)
    scale = 1.0;

  fout << "<!DOCTYPE html>" << endl;
  fout << "<html>\n<head>\n";
  fout << "<meta charset=\"UTF-8\"/>\n";

  if (refreshTimeOut > 0)
    fout << "<meta http-equiv=\"refresh\" content=\"" << refreshTimeOut << "\">\n";

  fout << "<title>" << gdioutput::toUTF8(title) << "</title>\n";

  map<pair<gdiFonts, wstring>, pair<string, string>> styles;
  generateStyles(gdi, fout, scale, false, gdi.getTL(), styles);

  fout << "</head>\n";

  fout << "<body>\n";
  double yscale = 1.3 * scale;
  double xscale = 1.2 * scale;
  int offsetY = 0, offsetX = 0;
  HTMLWriter::formatTL<list<TextInfo>, InterpTextInfo>(fout, imgWriter, styles, gdi.getTL(), yscale, xscale, offsetY, offsetX);

  fout << "<p style=\"position:absolute;left:10px;top:" << int(yscale * (gdi.getPageY() - 45)) + offsetY << "px\">";

  char bf1[256];
  char bf2[256];
  GetTimeFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf2, 256);
  GetDateFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf1, 256);
  //fout << "Skapad av <i>MeOS</i>: " << bf1 << " "<< bf2 << "\n";
  fout << gdioutput::toUTF8(lang.tl("Skapad av ")) + "<a href=\"https://www.melin.nu/meos\" target=\"_blank\"><i>MeOS</i></a>: " << bf1 << " " << bf2 << "\n";
  fout << "</p>\n";

  fout << "</body>\n";
  fout << "</html>\n";
}

wstring html_table_code(const wstring &in)
{
  if (in.size()==0)
    return L"&nbsp;";
  else {
    return encodeXML(in);
  }
}

bool sortTL_X(const TextInfo *a, const TextInfo *b)
{
  return a->xp < b->xp;
}


void HTMLWriter::writeTableHTML(gdioutput &gdi, 
                                const wstring &file,
                                const wstring &title, 
                                int refreshTimeOut, 
                                double scale) {
  checkWriteAccess(file);
  ofstream fout(file.c_str());

  if (fout.bad())
    return throw std::exception("Bad output stream");

  wchar_t drive[20];
  wchar_t dir[MAX_PATH];
  wchar_t name[MAX_PATH];
  wchar_t ext[MAX_PATH];
  _wsplitpath_s(file.c_str(), drive, dir, name, ext);
  wstring path = wstring(drive) + dir;

  writeTableHTML(gdi, fout, title, true, path, false, refreshTimeOut, scale);
}

void HTMLWriter::writeTableHTML(gdioutput& gdi,
                                ostream& fout,
                                const wstring& title,
                                bool includeImages,
                                const wstring& imageDirectoryDestination,
                                bool simpleFormat,
                                int refreshTimeOut,
                                double scale) {
  if (scale <= 0)
    scale = 1.0;

  ImageWriter imgWriter(imageDirectoryDestination, includeImages);

  fout << "<!DOCTYPE html>" << endl;
  fout << "<html>\n<head>\n";
  fout << "<meta charset=\"UTF-8\"/>\n";
  if (refreshTimeOut > 0)
    fout << "<meta http-equiv=\"refresh\" content=\"" << refreshTimeOut << "\">\n";
  fout << "<title>" << gdioutput::toUTF8(title) << "</title>\n";

  map< pair<gdiFonts, wstring>, pair<string, string> > styles;
  generateStyles(gdi, fout, scale, true, gdi.getTL(), styles);

  fout << "</head>\n";

  fout << "<body>\n";
  auto& TL = gdi.getTL();
  auto it = TL.begin();
  int MaxX = gdi.getPageX() - 100;
  map<int, int> tableCoordinates;

  //Get x-coordinates
  while (it != TL.end()) {
    tableCoordinates[it->xp] = 0;
    ++it;
  }

  int kr = 0;
  map<int, int>::iterator mit = tableCoordinates.begin();
  while (mit != tableCoordinates.end()) {
    mit->second = kr++;
    ++mit;
  }
  tableCoordinates[MaxX] = kr;

  vector<bool> sizeSet(kr + 1, false);

  fout << "<table cellspacing=\"0\" border=\"0\">\n";

  int linecounter = 0;
  it = TL.begin();

  vector< pair<int, vector<const TextInfo*> > > rows;
  rows.reserve(TL.size() / 3);
  vector<int> ypRow;
  int minHeight = 100000;

  while (it != TL.end()) {
    int y = it->yp;
    vector<const TextInfo*> row;

    int subnormal = 0;
    int normal = 0;
    int header = 0;
    int mainheader = 0;
    while (it != TL.end() && it->yp == y) {
      if (!gdioutput::skipTextRender(it->format)) {
        row.push_back(&*it);
        switch (it->getGdiFont()) {
        case fontLarge:
        case boldLarge:
        case boldHuge:
          mainheader++;
          break;
        case boldText:
        case italicMediumPlus:
        case fontMediumPlus:
          header++;
          break;
        case fontSmall:
        case italicSmall:
          subnormal++;
          break;
        default:
          normal++;
        }
      }
      ++it;
    }

    if (row.empty())
      continue;

    bool isMainHeader = mainheader > normal;
    bool isHeader = (header + mainheader) > normal;
    bool isSub = subnormal > normal;

    sort(row.begin(), row.end(), sortTL_X);
    rows.resize(rows.size() + 1);
    rows.back().first = isMainHeader ? 1 : (isHeader ? 2 : (isSub ? 3 : 0));
    rows.back().second.swap(row);
    int last = ypRow.size();
    ypRow.push_back(y);
    if (last > 0) {
      minHeight = min(minHeight, ypRow[last] - ypRow[last - 1]);
    }
  }
  int numMin = 0;
  for (size_t gCount = 1; gCount < rows.size(); gCount++) {
    int h = ypRow[gCount] - ypRow[gCount - 1];
    if (h == minHeight)
      numMin++;
  }
  if (numMin == 0)
    numMin = 1;

  int hdrLimit = (rows.size() / numMin) <= 4 ? int(minHeight * 1.2) : int(minHeight * 1.5);
  for (size_t gCount = 1; gCount + 1 < rows.size(); gCount++) {
    int type = rows[gCount].first;
    int lastType = gCount > 0 ? rows[gCount - 1].first : 0;
    int nextType = gCount + 1 < rows.size() ? rows[gCount + 1].first : 0;
    if (type == 0 && (lastType == 1 || lastType == 2) && (nextType == 1 || nextType == 2))
      continue; // No reclassify

    int h = ypRow[gCount] - ypRow[gCount - 1];
    if (h > hdrLimit && rows[gCount].first == 0)
      rows[gCount].first = 2;
  }

  ypRow.clear();
  string lineclass;
  for (size_t gCount = 0; gCount < rows.size(); gCount++) {
    vector<const TextInfo*>& row = rows[gCount].second;
    int type = rows[gCount].first;
    int lastType = gCount > 0 ? rows[gCount - 1].first : 0;
    int nextType = gCount + 1 < rows.size() ? rows[gCount + 1].first : 0;

    vector<const TextInfo*>::iterator rit;
    fout << "<tr>" << endl;

    if (simpleFormat) {
    }
    else if (type == 1) {
      lineclass = " class=\"freeheader\"";
      linecounter = 0;
    }
    else if (type == 2) {
      linecounter = 0;
      lineclass = " valign=\"bottom\" class=\"header\"";
    }
    else {
      if (type == 3)
        linecounter = 1;

      if ((lastType == 1 || lastType == 2) && (nextType == 1 || nextType == 2) && row.size() < 3) {
        lineclass = "";
      }
      else
        lineclass = (linecounter & 1) ? " class=\"e1\"" : " class=\"e0\"";

      linecounter++;
    }

    for (size_t k = 0; k < row.size(); k++) {
      int thisCol = tableCoordinates[row[k]->xp];

      if (k == 0 && thisCol != 0)
        fout << "<td" << lineclass << " colspan=\"" << thisCol << "\">&nbsp;</td>";

      int nextCol;
      if (row.size() == k + 1)
        nextCol = tableCoordinates.rbegin()->second;
      else
        nextCol = tableCoordinates[row[k + 1]->xp];

      int colspan = nextCol - thisCol;

      assert(colspan > 0);

      string style;

      if (row[k]->format & textRight)
        style = " style=\"text-align:right\"";

      if (colspan == 1 && !sizeSet[thisCol]) {
        fout << "  <td" << lineclass << style << " width=\"" << int((k + 1 < row.size()) ?
          (row[k + 1]->xp - row[k]->xp) : (MaxX - row[k]->xp)) << "\">";
        sizeSet[thisCol] = true;
      }
      else if (colspan > 1)
        fout << "  <td" << lineclass << style << " colspan=\"" << colspan << "\">";
      else
        fout << "  <td" << lineclass << style << ">";

      if ((row[k]->format & 0xFF) == textImage) {
        int imgW = int((row[k]->textRect.right - row[k]->textRect.left) * scale);
        int imgH = int((row[k]->textRect.bottom - row[k]->textRect.top) * scale);
        imgWriter.write(fout, "", "", row[k]->text, imgW, imgH);
        fout << "</td>" << endl;
      }
      else {
        gdiFonts font = row[k]->getGdiFont();
        string starttag, endtag;
        getStyle(styles, *row[k], "", starttag, endtag);

        fout << starttag << gdioutput::toUTF8(html_table_code(row[k]->text)) << endtag << "</td>" << endl;
      }
    }
    fout << "</tr>\n";

    row.clear();
  }

  fout << "</table>\n";

  if (!simpleFormat) {
    fout << "<br><p>";
    char bf1[256];
    char bf2[256];
    GetTimeFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf2, 256);
    GetDateFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf1, 256);
    wstring meos = getMeosCompectVersion();
    fout << gdioutput::toUTF8(lang.tl("Skapad av ")) + "<a href=\"https://www.melin.nu/meos\" target=\"_blank\"><i>MeOS "
      << gdioutput::toUTF8(meos) << "</i></a>: " << bf1 << " " << bf2 << "\n";
    fout << "</p><br>\n";
  }
  fout << "</body>" << endl;
  fout << "</html>" << endl;
}

extern wchar_t programPath[MAX_PATH];

namespace {
  bool isWord(const string& str, const string& word, size_t off) {
    int wlen = word.length();
    if (str.length() < off + wlen)
      return true;

    char nextC = str[off + wlen];
    if (!isalnum(nextC))
      return true;

    return false;
  }

  void replaceAll(string& str, const vector<string>& fromV, const string& to, bool forceAppend = false) {
    bool didApply = false;
    for (const string& from : fromV) {
      size_t start_pos = 0;
      while ((start_pos = str.find(from, start_pos)) != string::npos) {
        if (isWord(str, from, start_pos)) {
          didApply = true;
          str.replace(start_pos, from.length(), to);
          start_pos += to.length();
        }
        else {
          start_pos += from.length();
        }
      }
    }
    if (!didApply && forceAppend)
      str += to; // Append if not set
  }

  bool findAny(const string& str, const vector<string>& fromV) {
    for (const string& from : fromV) {
      size_t start_pos = 0;
      while ((start_pos = str.find(from, start_pos)) != string::npos) {
        if (isWord(str, from, start_pos))
          return true;
        start_pos += from.length();
      }
      
    }
    return false;
  }
}

void HTMLWriter::parseTagName(const string &str, string &tag, wstring &name) {
  vector<string> tagName;
  split(str, "@", tagName);
  if (tagName.size() == 2) {
    tag = tagName[0]; 
    string2Wide(tagName[1], name);
  }
  else {
    throw meosException("Bad template format: " + str);
  }
}

void HTMLWriter::enumTemplates(TemplateType type, vector<TemplateInfo> &descriptionFile) {
  vector<wstring> res;

  wchar_t listpath[MAX_PATH];
  getUserFile(listpath, L"");
  expandDirectory(listpath, L"*.meostmpl", res);
  expandDirectory(listpath, L"*.template", res);
  
  int userCounter = res.size();

#ifdef _DEBUG
  expandDirectory((wstring(programPath) + L".\\..\\Lists\\").c_str(), L"*.meostmpl", res);
  expandDirectory((wstring(programPath) + L".\\..\\Lists\\").c_str(), L"*.template", res);
#endif

  if (exePath[0]) {
    expandDirectory(exePath, L"*.meostmpl", res);
    expandDirectory(exePath, L"*.template", res);
  }

  set<string> tags;
  auto fillInUsedFunctions = [](ifstream &file, TemplateInfo& ti) {
    string str;
    while (getline(file, str)) {
      bool isComment = false;
      int ix = 0;
      while (ix < str.size()) {
        if (str[ix] == '%')
          isComment = true;
        else if (str[ix] != ' ' && str[ix] != '\t')
          break;
        ix++;
      }
      if (!isComment) {
        ti.hasInnerPage = ti.hasInnerPage || findAny(str, { "@INNERPAGE" });
        ti.hasOuterPage = ti.hasOuterPage || findAny(str, { "@OUTERPAGE" });
        ti.hasTimer = ti.hasTimer || findAny(str, { "@T", "@TIME" });
      }
    }
  };

  tCache.clear();
  for (wstring &fn : res) {
    TemplateInfo ti;
    bool userDefined = --userCounter >= 0;
    ifstream file(fn);
    string str;
    if (getline(file, str)) {
      if (str == "@MEOS EXPORT TEMPLATE" && getline(file, str)) {
        parseTagName(str, ti.tag, ti.name);
        if (!tags.insert(ti.tag).second)
          continue; // Already included

        if (getline(file, str)) 
          string2Wide(str, ti.desc);

        ti.file = fn;
        ti.userInstalled = userDefined;
        if (type == TemplateType::List) {
          fillInUsedFunctions(file, ti);
          descriptionFile.push_back(ti);
        }
      }
      else if (str == "@MEOS PAGE" && getline(file, str)) {
        parseTagName(str, ti.tag, ti.name);
        if (!tags.insert(ti.tag).second)
          continue; // Already included

        if (getline(file, str)) {
          string2Wide(str, ti.desc);
        }
        ti.file = fn;

        ti.userInstalled = userDefined;
        if (type == TemplateType::Page)
          descriptionFile.push_back(ti); 
      }
    }
  }
}

HTMLWriter::TemplateInfo HTMLWriter::getTemplateInfo(TemplateType type, const string& tag, bool acceptMissing) {
  vector<TemplateInfo> descriptionFile;
  enumTemplates(type, descriptionFile);
  int ix = -1;
  for (size_t k = 0; k < descriptionFile.size(); k++) {
    if (descriptionFile[k].tag == tag) {
      ix = k;
      break;
    }
  }
  if (ix == -1) {
    if (acceptMissing)
      return TemplateInfo();

    throw meosException("Unknown template X#" + tag);
  }
  return descriptionFile[ix];
}

const HTMLWriter& HTMLWriter::getWriter(TemplateType type, const string &tag, 
                                        const vector<pair<string, wstring>> &options) {
  string cacheTag = tag;
  for (auto& op : options)
    cacheTag += "|" + op.first + gdioutput::narrow(op.second);

  if (auto res = tCache.find(cacheTag); res != tCache.end()) {
    return *res->second;
  }
  else {

    shared_ptr<HTMLWriter> tmpl = make_shared<HTMLWriter>();
    tmpl->setConditions(options);
    tmpl->read(getTemplateFile(type, tag));

    tCache[cacheTag] = tmpl;
    return *tmpl;
  }
}

string HTMLWriter::localize(const string &in) {
  string out;
  size_t offset = 0;
  size_t pos = in.find_first_of('$', offset);

  auto specialTranslate = [this](const string& key, wstring& out) -> bool {
    if (auto res = conditions.find(key); res != conditions.end()) {
      out = res->second;
      return true;
    }
    return false;
  };

  while (pos != string::npos) {
    if (out.empty())
      out.reserve(in.length() * 2);

    size_t end = in.find_first_of('$', pos+1);
    if (end != string::npos && end > pos + 2) {
      string key = in.substr(pos + 1, end - pos - 1);
      out += in.substr(offset, pos - offset);

      wstring wkey;
      if (!specialTranslate(key, wkey)) {
        wkey = gdioutput::fromUTF8(key);

        if (wkey[0] != '!')
          wkey = lang.tl(wkey);
        else
          wkey = lang.tl(wkey.substr(1), true);
      }

      out += gdioutput::toUTF8(wkey);
      
      //wstring key = gdioutput::fromUTF8(in.substr(pos + 1, end - pos - 1));
      
      
      offset = end + 1;
      pos = in.find_first_of('$', offset);
    }
    else
      break;
  }
  if (offset == 0)
    return trim(in);
  
  out += in.substr(offset);
  return trim(out);
}

void HTMLWriter::read(const wstring &fileName) {
  ifstream file(fileName);
  string dmy;
  string *acc = &dmy;
  string str;
  int ok = 0;
  vector<int> ifBlock;

  const string comment = "//";
  while (getline(file, str)) {
    bool skipLine = count(ifBlock.begin(), ifBlock.end(), 0) > 0;
    string trimLine = trim(str);
    if (ok == 0 && str == "@MEOS EXPORT TEMPLATE") {
      ok = 1;
      continue;
    }
    else if (ok == 1) {
      ok = 2;
      parseTagName(str, tag, name);
      if (getline(file, str)) {
        info = str;
      }
      skipLine = true;
    }
    else if (ok == 0 && str == "@MEOS PAGE") {
      ok = 3;
      continue;
    }
    else if (ok == 3) {
      ok = 4;
      info = str;
      acc = &page;
      continue;
    }
    else if (trimLine.length() > 1 && trimLine[0] == '%')
      continue; // Ignore comment
    else if (trimLine == "@HEAD")
      acc = &head, skipLine = true;
    else if (trimLine == "@DESCRIPRION")
      acc = &description, skipLine = true;
    else if (trimLine == "@OUTERPAGE")
      acc = &outerpage, skipLine = true;
    else if (trimLine == "@INNERPAGE")
      acc = &innerpage, skipLine = true;
    else if (trimLine == "@SEPARATOR")
      acc = &separator, skipLine = true;
    else if (trimLine == "@END")
      acc = &end, skipLine = true;
    else if (trimLine.length() > 1 && trimLine[0] == '@') {
      string arg;
      auto res = parseFunc(str.substr(1), arg);
      if (res == Function::ENDIF) {
        if (ifBlock.empty())
          throw meosException("IF/END mismatch");
        else
          ifBlock.pop_back();
      }
      else if (res == Function::IF) {
        ifBlock.push_back(hasCondition(arg));
      }

      if (res != Function::TEXTCOMMAND)
        continue;
    }
     
    if (!skipLine) {
      size_t cix = str.rfind(comment);
      if (cix != string::npos) {
        if (cix == 0)
          continue; // Comment line
        else if (cix > 0 && str[cix - 1] != ':')
          str = str.substr(0, cix);
      }

      *acc += localize(str) + "\n";
      if (!(str.empty() || acc->back() == '>' || acc->back() == ';' || acc->back() == '}' || acc->back() == '{'))
        *acc += " ";
    }
  }

  if (!ifBlock.empty())
    throw meosException("EOF: IF/END mismatch");
}

HTMLWriter::Function HTMLWriter::parseFunc(const string& str, string& arg) const {
  if (textCommands.empty()) {
    for (auto& s : {"STYLE", "TITLE", "DESCRIPTION", "CONTENTS", "PAGE", "NUMPAGE", "NUMCOL", "PERCENTCOMPLETE", "MEOS", "TIME"}) {
      textCommands.insert(s);
    }
  }
  if (str.substr(0, 3) == "IF ") {
    arg = trim(str.substr(3));
    return Function::IF;
  }
  else if (str == "ENDIF")
    return Function::ENDIF;
  else if (str == "END" || str == "INNERPAGE" || str == "OUTERPAGE" || str == "HEAD" || str == "SEPARATOR")
    return Function::NONE;
  else if (str.length() == 1 || textCommands.count(str))
    return Function::TEXTCOMMAND;
  
  throw meosException("Unknown function: " + str);
}

bool HTMLWriter::hasCondition(const string& arg) const {
  return conditions.count(arg) > 0;
}

class InterpPrintTextInfo {
public:
  static int x(const PrintTextInfo &ti) {
    return int(ti.xp);
  }

  static int y(const PrintTextInfo &ti) {
    return int(ti.yp);
  }

  static const TextInfo &ti(const PrintTextInfo &in_ti) {
    return in_ti.ti;
  }
};

void HTMLWriter::generate(gdioutput &gdi,
                          ostream &fout,
                          const wstring &title, 
                          const wstring &contentDescription,
                          bool respectPageBreak,
                          const int nRows,
                          const int numCol,
                          const int interval,
                          const int marginPercent, 
                          double scale) const {
  int w, h;
  gdi.getTargetDimension(w, h);


  ImageWriter imgWriter(L"", false);
  
  string meos = "<a href=\"https://www.melin.nu/meos\" target=\"_blank\"><i>MeOS</i></a>: "  + gdioutput::toUTF8(getMeosCompectVersion());

  int margin = (w * marginPercent) / 100;
  int height = nRows * gdi.getLineHeight();
  bool infPage = false;
  if (nRows == 0) {
    infPage = true;
    height = gdi.getPageY()/numCol;
  }
  
  PageInfo pageInfo;
  pageInfo.topMargin = 20;
  pageInfo.scaleX = 1.0f;
  pageInfo.scaleY = 1.0f;
  pageInfo.leftMargin = 20;
  pageInfo.bottomMargin = 30;
  pageInfo.pageY = float(height - margin);
  pageInfo.printHeader = false;
  pageInfo.yMM2PrintC = pageInfo.xMM2PrintC = 1;
  pageInfo.xMM2PrintK = 0;
  pageInfo.yMM2PrintK = 0;

  list<RectangleInfo> rectangles;
  vector<RenderedPage> pages;
  pageInfo.renderPages(gdi.getTL(), rectangles, false, respectPageBreak || infPage, pages);

  int numChapter = 0;
  for (auto &pi : pages)
    if (pi.startChapter)
      numChapter++;

  if (infPage && pages.size() > size_t(numCol)) {
    bool respectChapter = numChapter == numCol; // If the number of chapters (linked lists) equals number of columns, respec these.
    vector<RenderedPage> pagesOut;
    bool startPage = true;
    int ydiff = 0;
    for (auto &p : pages) {
      if (p.text.empty())
        continue;

      if (respectChapter)
        startPage = p.startChapter;
      else if (!pagesOut.empty() && pagesOut.back().text.back().yp + p.text.back().yp / 4 > height)
        startPage = true;


      if (startPage && pagesOut.size() < size_t(numCol)) {
        pagesOut.push_back(move(p));
        startPage = false;
        ydiff = int(pagesOut.back().text.back().yp);
      }
      else {
        for (auto &t : p.text) {
          pagesOut.back().text.push_back(move(t));
          pagesOut.back().text.back().yp += ydiff;
        }
        ydiff = int(pagesOut.back().text.back().yp);
      }
    }
    pages.swap(pagesOut);
  }

  string output = head;

  if (findAny(output, { "@C", "@CONTENTS" }))
    throw meosException("Cannot place @CONTENTS in @HEAD section");

  replaceAll(output, { "@D", "@DESCRIPTION"}, gdioutput::toUTF8(encodeXML(contentDescription)));
  replaceAll(output, { "@T", "@TITLE"}, gdioutput::toUTF8(encodeXML(title)));
  replaceAll(output, { "@M", "@MEOSVERSION"}, meos);
  map<pair<gdiFonts, wstring>, pair<string, string>> styles;

  {
    stringstream sout;
    generateStyles(gdi, sout, scale, false, gdi.getTL(), styles);
    replaceAll(output, { "@S", "@STYLE"}, sout.str());
  }

  int nPage = (pages.size() + numCol - 1) / numCol;
  replaceAll(output, { "@N", "@NUMPAGE" }, itos(nPage));

  fout << output;

  int ipCounter = 1;
  int opCounter = 1;
  
  for (size_t pix = 0; pix < pages.size();) {
    string innerpageoutput;

    for (int ip = 0; ip < numCol; ip++) {
      if (pages.size() == pix)
        break;

      if (ip > 0 || (innerpage.empty() && numCol == 1 && pix > 0)) {
        // Separator
        output = separator;
        replaceAll(output, { "@P", "@PAGE" }, itos(ipCounter));
        replaceAll(output, { "@L", "@PERCENTCOMPLETE" }, itos((ip * 100) / numCol));
        
        innerpageoutput += output;
      }

      auto &p = pages[pix++];
      stringstream sout;
      double yscale = 1.3 * scale;
      double xscale = 1.2 * scale;
      int offsetY = 0, offsetX = 0;

      formatTL<vector<PrintTextInfo>, InterpPrintTextInfo>(sout, imgWriter, styles, p.text, yscale, xscale, offsetY, offsetX);

      output = innerpage;
      replaceAll(output, { "@P", "@PAGE" }, itos(ipCounter++));
      replaceAll(output, { "@L", "@PERCENTCOMPLETE" }, itos((ip * 100) / numCol));
      replaceAll(output, { "@M", "@MEOSVERSION" }, meos);
      replaceAll(output, { "@C", "@CONTENTS" }, sout.str(), true);

      innerpageoutput += output;
    }

    string outeroutput = outerpage;
    replaceAll(outeroutput, { "@L", "@PERCENTCOMPLETE" }, itos((opCounter * 100) / nPage));
    replaceAll(outeroutput, { "@P", "@PAGE" }, itos(opCounter++));
    replaceAll(outeroutput, { "@M", "@MEOSVERSION" }, meos);
    replaceAll(outeroutput, { "@N", "@NUMPAGE" }, itos(nPage));
    replaceAll(outeroutput, { "@C", "@CONTENTS" }, innerpageoutput, true);

    fout << outeroutput << endl;
  }

  assert(opCounter - 1 == nPage);
  output = end;

  if (findAny(output, { "@C", "@CONTENTS" } ))
    throw meosException("Cannot place @CONTENTS in @END section");

  replaceAll(output, { "@N", "@NUMPAGE" }, itos(opCounter - 1));
  replaceAll(output, { "@I", "@NUMCOL"}, itos(numCol));
  replaceAll(output, { "@T", "@TIME" }, itos(interval));
  replaceAll(output, { "@M", "@MEOSVERSION" }, meos);
  fout << output;
}

void HTMLWriter::write(gdioutput &gdi, const wstring &file, const wstring &title, const wstring &contentsDescription,
                       bool respectPageBreak,
                       const string &typeTag, int refresh,
                       int rows, int cols, int time_ms, int margin, double scale) {

  checkWriteAccess(file);
  ofstream fout(file.c_str());

  wchar_t drive[20];
  wchar_t dir[MAX_PATH];
  wchar_t name[MAX_PATH];
  wchar_t ext[MAX_PATH];
  _wsplitpath_s(file.c_str(), drive, dir, name, ext);
  wstring path = wstring(drive) + dir;

  write(gdi, fout, title, true, path, contentsDescription, respectPageBreak, typeTag, refresh,
        rows, cols, time_ms, margin, scale);
}

void HTMLWriter::write(gdioutput &gdi, ostream &fout, const wstring &title, 
                       bool includeImages,
                       const wstring& imageDirectoryDestination,
                       const wstring &contentsDescription,
                       bool respectPageBreak, const string &typeTag, int refresh,
                       int rows, int cols, int time_ms, int margin, double scale) {
  if (typeTag == "table")
    writeTableHTML(gdi, fout, title, includeImages, imageDirectoryDestination, false, refresh, scale);
  else if (typeTag == "free") {
    writeHTML(gdi, fout, title, includeImages, imageDirectoryDestination, refresh, scale);
  }
  else {
   /* auto res = tCache.find(typeTag);
    if (res == tCache.end()) {
      vector<HTMLWriter::TemplateInfo> htmlTmpl;
      HTMLWriter::enumTemplates(TemplateType::List, htmlTmpl);
      int ix = -1;
      for (size_t k = 0; k < htmlTmpl.size(); k++) {
        if (htmlTmpl[k].tag == typeTag) {
          ix = k;
          break;
        }
      }

      if (ix == -1)
        throw std::exception("Internal error");

      shared_ptr<HTMLWriter> tmpl = make_shared<HTMLWriter>();
      tmpl->read(htmlTmpl[ix].file);

      vector<string> tagName;
      split(tmpl->info, "@", tagName);
      if (tagName.size() == 2)
        tCache[tagName[0]] = tmpl;
      tmpl->generate(gdi, fout, title, contentsDescription, respectPageBreak, rows, cols, time_ms, margin, scale);
    }
    else {
      res->second->generate(gdi, fout, title, contentsDescription, respectPageBreak, rows, cols, time_ms, margin, scale);
    }*/

    getWriter(TemplateType::List, typeTag, {}).generate(gdi, fout, title, contentsDescription, respectPageBreak, rows, cols, time_ms, margin, scale);
  }
}

void HTMLWriter::write(gdioutput &gdi, const wstring &file, const wstring &title, int refresh, oListParam &param, const oEvent &oe) {
  write(gdi, file, title, param.getContentsDescriptor(oe), param.pageBreak, param.htmlTypeTag, 
        refresh != 0 ? refresh : param.timePerPage / 1000, param.htmlRows, param.nColumns,
        param.timePerPage, param.margin, param.htmlScale);
}

void HTMLWriter::write(gdioutput& gdi, ostream& fout, const wstring& title, int refresh, oListParam& param, const oEvent& oe) {
  write(gdi, fout, title, true, L"", param.getContentsDescriptor(oe), param.pageBreak, param.htmlTypeTag,
    refresh != 0 ? refresh : param.timePerPage / 1000, param.htmlRows, param.nColumns,
    param.timePerPage, param.margin, param.htmlScale);
}

void HTMLWriter::getPage(const oEvent &oe, string &out) const {
  out = page;
}

void HTMLWriter::ImageWriter::write(ostream& fout, const string& xp, const string& yp, const wstring& img, int width, int height) {
  if (!writeImages) {
    if (xp.empty())
      fout << "&nbsp;";
  }
  else {
    if (img.empty() || img[0] != 'L')
      throw meosException("Unsupported image");
    uint64_t imgId = _wcstoui64(img.c_str() + 1, nullptr, 10);

    if (!savedFiles.count(imgId)) {
      if (!destination.empty()) {
        auto& data = image.getRawData(imgId);
        wstring d = destination + img + L".png";
        ofstream out(d, ofstream::out | ofstream::binary);
        out.write((const char *)data.data(), data.size());
        savedFiles[imgId] = "L" + itos(imgId) + ".png";
      }
      else {
        savedFiles[imgId] = "/meos?image=ID" + itos(imgId) + ".png";
      }      
    }
    string style;
    if (xp.size() > 0) 
      style = " style=\"position:absolute;left:" + xp + "px;top:" + yp + "px\"";
        
    fout << "<img src=\"" << savedFiles[imgId] << "\" width=\"" <<
             width << "\" height=\"" << height << "\"" << style << ">";
    
  }
}
