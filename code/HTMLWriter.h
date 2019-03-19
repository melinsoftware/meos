/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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

struct oListParam;

class HTMLWriter {
  string info;
  string description;
  string head;
  string outerpage;
  string innerpage;
  string separator;
  string end;

  string page;

  static map <string, shared_ptr<HTMLWriter>> tCache;

  static string localize(const string &in);

public:

  static void reset() {
    tCache.clear();
  }
  
  enum class TemplateType {
    List,
    Page
  };

  static const HTMLWriter &getWriter(TemplateType type, const string &tag);
  
  struct TemplateInfo {
    string tag;
    wstring name;
    wstring desc;
    wstring file;
  };

  static void enumTemplates(TemplateType type, vector<TemplateInfo> &descriptionFile);

  void read(const wstring &fileName);

  void generate(gdioutput &gdi,
                ostream &fout,
                const wstring &title,
                const wstring &contentDescription,
                bool respectPageBreak,
                const int nRows,
                const int numCol,
                const int interval,
                const int marginPercent,
                double scal) const;

  void getPage(const oEvent &oe, string &out) const;

  static void writeHTML(gdioutput &gdi, ostream &dout, const wstring &title, int refreshTimeOut, double scale);

  static void writeTableHTML(gdioutput &gdi, ostream &fout,
                             const wstring &title,
                             bool simpleFormat,
                             int refreshTimeOut,
                             double scale);

  static void writeTableHTML(gdioutput &gdi, const wstring &file,
                             const wstring &title,
                             int refreshTimeOut,
                             double scale);

  static void writeHTML(gdioutput &gdi, const wstring &file, 
                        const wstring &title, int refreshTimeOut, double scale);

  static void write(gdioutput &gdi, const wstring &file, const wstring &title, const wstring &contentsDescription,
                    bool respectPageBreak, const string &typeTag, int refresh,
                    int rows, int cols, int time_ms, int margin, double scale);

  static void write(gdioutput &gdi, ostream &fout, const wstring &title, const wstring &contentsDescription,
                    bool respectPageBreak, const string &typeTag, int refresh,
                    int rows, int cols, int time_ms, int margin, double scale);

  static void write(gdioutput &gdi, const wstring &file, const wstring &title, int refresh, oListParam &param, const oEvent &oe);

  template<typename T, typename TI>
  static void formatTL(ostream &fout,
                       const map< pair<gdiFonts, string>, pair<string, string> > &styles,
                       const T &tl, 
                       double &yscale,
                       double &xscale,
                       int &offsetY,
                       int &offsetX);

};