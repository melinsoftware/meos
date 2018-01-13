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

double getLocalScale(const wstring &fontName, wstring &faceName);
wstring getMeosCompectVersion();

static void generateStyles(const gdioutput &gdi, ostream &fout, bool withTbl, const list<TextInfo> &TL,
                           map< pair<gdiFonts, string>, pair<string, string> > &styles) {
  fout << "<style type=\"text/css\">\n";
  fout << "body {background-color: rgb(250,250,255)}\n";
  fout << "h1 {font-family:arial,sans-serif;font-size:24px;font-weight:normal;white-space:nowrap}\n";
  fout << "h2 {font-family:arial,sans-serif;font-size:20px;font-weight:normal;white-space:nowrap}\n";
  fout << "h3 {font-family:arial,sans-serif;font-size:16px;font-weight:normal;white-space:nowrap}\n";
  fout << "p {font-family:arial,sans-serif;font-size:12px;font-weight:normal}\n";
  fout << "div {font-family:arial,sans-serif;font-size:12px;font-weight:normal}\n";

  if (withTbl) {
    fout << "td {font-family:arial,sans-serif;font-size:12px;font-weight:normal;white-space:nowrap}\n";
    fout << "td.e0 {background-color: rgb(238,238,255)}\n";
    fout << "td.e1 {background-color: rgb(245,245,255)}\n";
    fout << "td.header {line-height:1.8;height:40px}\n";
    fout << "td.freeheader {line-height:1.2}\n";
  }
  list<TextInfo>::const_iterator it=TL.begin();
  int styleList = 1;
  while (it != TL.end()) {
    gdiFonts font = it->getGdiFont();

    if (!it->font.empty() || (font == italicMediumPlus)
                          || (font == fontMediumPlus)) {

      if (styles.find(make_pair(font, gdi.narrow(it->font))) != styles.end()) {
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
      double scale = 1.0;
      if (it->font.empty()) {
        faceName = L"arial,sans-serif";
      }
      else
        scale = getLocalScale(it->font, faceName);

      fout << element << "." <<  style
           << "{font-family:" << gdi.narrow(faceName) << ";font-size:"
           << itos(int(floor(scale * baseSize + 0.5)))
           << "px;font-weight:normal;white-space:nowrap}\n";

      styles[make_pair(font, gdi.narrow(it->font))] = make_pair(element, style);
    }
    ++it;
  }
  fout << "</style>\n";
}

static void getStyle(const map< pair<gdiFonts, string>, pair<string, string> > &styles,
                     gdiFonts font, const string &face, const string &extraStyle, string &starttag, string &endtag) {
  starttag.clear();
  endtag.clear();
  string extra;
  switch (font) {
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

  pair<gdiFonts, string> key(font, face);
  map< pair<gdiFonts, string>, pair<string, string> >::const_iterator res = styles.find(key);

  if (res != styles.end()) {
    const pair<string, string> &stylePair = res->second;

    if (!stylePair.first.empty()) {
      starttag = "<" + stylePair.first;
      if (!stylePair.second.empty())
        starttag += " class=\""  + stylePair.second + "\"";
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
    switch(font) {
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

    if (!extraStyle.empty() && element.empty())
      element = "div";

    if (element.size()>0) {
      starttag = "<" + element + extraStyle + ">";
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

bool gdioutput::writeHTML(const wstring &file, const wstring &title, int refreshTimeOut, int autoscroll) const
{
  checkWriteAccess(file);
  ofstream fout(file.c_str());

  if (fout.bad())
    return false;


  fout << "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n" <<
          "          \"http://www.w3.org/TR/html4/loose.dtd\">\n\n";

  fout << "<html>\n<head>\n";
  fout << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
  if (refreshTimeOut > 0)
    fout << "<meta http-equiv=\"refresh\" content=\"" << refreshTimeOut << "\">\n";


  fout << "<title>" << toUTF8(title) << "</title>\n";

  map< pair<gdiFonts, string>, pair<string, string> > styles;
  generateStyles(*this, fout, false, TL, styles);

  fout << "</head>\n";

  fout << "<body>\n";

  list<TextInfo>::const_iterator it = TL.begin();

  double yscale = 1.3;
  double xscale = 1.2;
  int offsetY = 0;
  while (it!=TL.end()) {
    if (skipTextRender(it->format)) {
      ++it;
      continue;
    }

    string yp = itos(int(yscale*it->yp) + offsetY); 
    string xp = itos(int(xscale *it->xp));

    string estyle;
    if (it->format!=1 && it->format!=boldSmall) {
      if (it->format & textRight)
        estyle = " style=\"position:absolute;left:" +
            xp + "px;top:"  + yp + "px\"";
      else
        estyle = " style=\"position:absolute;left:" +
            xp + "px;top:" + yp + "px\"";

    }
    else {
      if (it->format & textRight)
         estyle = " style=\"font-weight:bold;position:absolute;left:" +
              xp + "px;top:" + yp +  "px\"";
      else
         estyle = " style=\"font-weight:bold;position:absolute;left:" +
              xp + "px;top:" + yp + "px\"";
    }
    string starttag, endtag;
    getStyle(styles, it->getGdiFont(), narrow(it->font), estyle, starttag, endtag);

    if (!it->text.empty())
      fout << starttag << toUTF8(encodeXML(it->text)) << endtag << endl;

    if (it->format == boldLarge) {
      list<TextInfo>::const_iterator next = it;
      ++next;
      if (next == TL.end() || next->yp != it->yp)
        offsetY += 7;
    }
    ++it;
  }

  fout << "<p style=\"position:absolute;left:10px;top:" <<  int(yscale*MaxY)+15 + offsetY << "px\">";

  char bf1[256];
  char bf2[256];
  GetTimeFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf2, 256);
  GetDateFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, bf1, 256);
  //fout << "Skapad av <i>MeOS</i>: " << bf1 << " "<< bf2 << "\n";
  fout << toUTF8(lang.tl("Skapad av ")) + "<a href=\"http://www.melin.nu/meos\" target=\"_blank\"><i>MeOS</i></a>: " << bf1 << " "<< bf2 << "\n";
  fout << "</p>\n";

  if (autoscroll) {
    fout << "<script>\n";
    fout << "	 var dir = 1\n";
    fout << "	 var extremity_wait_sec = 2\n";
    fout << "	 var scroll_sec = .05\n";
    fout << "	 var scroll_amount = 2\n";
    fout << "	 function scrollMore() {\n";
    fout << "    var current = window.pageYOffset\n";
    fout << "    window.scrollBy(0, scroll_amount * dir)\n";
    fout << "    if (dir != 0 && Math.abs(current - window.pageYOffset) < scroll_amount / 2.0) {\n";
    fout << "      var nextDir = dir * -1\n";
    fout << "      dir = 0\n";
    fout << "      setTimeout(function() { dir = nextDir }, 1000 * extremity_wait_sec)\n";
    fout << "    }\n";
    fout << "  }\n";
    fout << "  setInterval(scrollMore, 1000 * scroll_sec)\n";
    fout << "</script>\n";
  }

  fout << "</body>\n";
  fout << "</html>\n";

  return false;
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


bool gdioutput::writeTableHTML(const wstring &file, 
                               const wstring &title, int refreshTimeOut, int autoScroll) const
{
  checkWriteAccess(file);
  ofstream fout(file.c_str());

  if (fout.bad())
    return false;

  return writeTableHTML(fout, title, false, refreshTimeOut,autoScroll);
}

bool gdioutput::writeTableHTML(ostream &fout, 
                               const wstring &title,
                               bool simpleFormat,
                               int refreshTimeOut,
	                           int autoscroll) const {

  fout << "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n" <<
          "          \"http://www.w3.org/TR/html4/loose.dtd\">\n\n";

  fout << "<html>\n<head>\n";
  fout << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
  if (refreshTimeOut > 0)
    fout << "<meta http-equiv=\"refresh\" content=\"" << refreshTimeOut << "\">\n";
  fout << "<title>" << toUTF8(title) << "</title>\n";

  map< pair<gdiFonts, string>, pair<string, string> > styles;
  generateStyles(*this, fout, true, TL, styles);

  fout << "</head>\n";

  fout << "<body>\n";

  list<TextInfo>::const_iterator it = TL.begin();
  map<int,int> tableCoordinates;

  //Get x-coordinates
  while (it!=TL.end()) {
      tableCoordinates[it->xp]=0;
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

  int linecounter=0;
  it=TL.begin();

  vector< pair<int, vector<const TextInfo *> > > rows;
  rows.reserve(TL.size() / 3);
  vector<int> ypRow;
  int minHeight = 100000;

  while (it!=TL.end()) {
    int y=it->yp;
    vector<const TextInfo *> row;

    int subnormal = 0;
    int normal = 0;
    int header = 0;
    int mainheader = 0;
    while (it!=TL.end() && it->yp==y) {
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
      minHeight = min(minHeight, ypRow[last] - ypRow[last-1]);
    }
  }
  int numMin = 0;
  for (size_t gCount = 1; gCount < rows.size(); gCount++) {
    int h = ypRow[gCount] - ypRow[gCount-1];
    if (h == minHeight)
      numMin++;
  }
  if (numMin == 0)
    numMin = 1;

  int hdrLimit = (rows.size() / numMin) <= 4 ?  int(minHeight * 1.2) : int(minHeight * 1.5);
  for (size_t gCount = 1; gCount + 1 < rows.size(); gCount++) {
    int type = rows[gCount].first;
    int lastType = gCount > 0 ? rows[gCount-1].first : 0;
    int nextType = gCount + 1 < rows.size() ? rows[gCount + 1].first : 0;
    if (type == 0 && (lastType == 1 || lastType == 2) && (nextType == 1 || nextType == 2))
      continue; // No reclassify

    int h = ypRow[gCount] - ypRow[gCount-1];
    if (h > hdrLimit && rows[gCount].first == 0)
      rows[gCount].first = 2;
  }

  ypRow.clear();
  string lineclass;
  for (size_t gCount = 0; gCount < rows.size(); gCount++) {
    vector<const TextInfo *> &row = rows[gCount].second;
    int type = rows[gCount].first;
    int lastType = gCount > 0 ? rows[gCount-1].first : 0;
    int nextType = gCount + 1 < rows.size() ? rows[gCount + 1].first : 0;

    vector<const TextInfo *>::iterator rit;
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
        lineclass = (linecounter&1) ? " class=\"e1\"" : " class=\"e0\"";

      linecounter++;
    }

    for (size_t k=0;k<row.size();k++) {
      int thisCol=tableCoordinates[row[k]->xp];

      if (k==0 && thisCol!=0)
        fout << "<td" << lineclass << " colspan=\"" << thisCol << "\">&nbsp;</td>";

      int nextCol;
      if (row.size()==k+1)
        nextCol=tableCoordinates.rbegin()->second;
      else
        nextCol=tableCoordinates[row[k+1]->xp];

      int colspan=nextCol-thisCol;

      assert(colspan>0);

      string style;

      if (row[k]->format&textRight)
        style=" style=\"text-align:right\"";

      if (colspan==1 && !sizeSet[thisCol]) {
        fout << "  <td" << lineclass << style << " width=\"" << int( (k+1<row.size()) ?
                        (row[k+1]->xp - row[k]->xp) : (MaxX-row[k]->xp)) << "\">";
        sizeSet[thisCol]=true;
      }
      else if (colspan>1)
        fout << "  <td" << lineclass << style << " colspan=\"" << colspan << "\">";
      else
        fout << "  <td" << lineclass << style << ">";

      gdiFonts font = row[k]->getGdiFont();
      string starttag, endtag;
      getStyle(styles, font, narrow(row[k]->font), "", starttag, endtag);

      fout << starttag << toUTF8(html_table_code(row[k]->text)) << endtag << "</td>" << endl;

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
    fout << toUTF8(lang.tl("Skapad av ")) + "<a href=\"http://www.melin.nu/meos\" target=\"_blank\"><i>MeOS "
         << toUTF8(meos) << "</i></a>: " << bf1 << " "<< bf2 << "\n";
    fout << "</p><br>\n";
  }
  if (autoscroll) {
	  fout << "<script>\n";
	  fout << "	 var dir = 1\n";
	  fout << "	 var extremity_wait_sec = 2\n";
	  fout << "	 var scroll_sec = .05\n";
	  fout << "	 var scroll_amount = 2\n";
	  fout << "	 function scrollMore() {\n";
	  fout << "    var current = window.pageYOffset\n";
	  fout << "    window.scrollBy(0, scroll_amount * dir)\n";
	  fout << "    if (dir != 0 && Math.abs(current - window.pageYOffset) < scroll_amount / 2.0) {\n";
	  fout << "      var nextDir = dir * -1\n";
	  fout << "      dir = 0\n";
	  fout << "      setTimeout(function() { dir = nextDir }, 1000 * extremity_wait_sec)\n";
	  fout << "    }\n";
	  fout << "  }\n";
	  fout << "  setInterval(scrollMore, 1000 * scroll_sec)\n";
	  fout << "</script>\n";
  }

  fout << "</body>\n";
  fout << "</html>\n";

  return true;
}