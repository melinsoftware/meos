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

  string localize(const string &in);

  enum class Function {
    NONE,
    IF,
    ENDIF,
    TEXTCOMMAND,
  };
  mutable set<string> textCommands;
  map<string, wstring> conditions;

  Function parseFunc(const string& str, string& arg) const;
  bool hasCondition(const string& arg) const;

  class ImageWriter {
    wstring destination;
    const bool writeImages;
    const wstring imageDirectoryDestination;
    map<uint64_t, string> savedFiles;
  public:
    ImageWriter(const wstring& dst, bool writeImages) : destination(dst), writeImages(writeImages) {}


    void write(std::ostream &fout, const string &xp, const string &yp, const wstring &img, int width, int height);
  };

  template<typename T, typename TI>
  static void formatTL(std::ostream& fout,
    ImageWriter& imageWriter,
    const map< pair<gdiFonts, wstring>, pair<string, string> >& styles,
    const T& tl,
    double& yscale,
    double& xscale,
    int& offsetY,
    int& offsetX);


  static void parseTagName(const string& str, string& tag, wstring& name);

public:

  static void reset() {
    tCache.clear();
  }
  
  template<typename T>
  void setConditions(const T& cond) {
    conditions.clear();
    for (auto &[key, value] : cond)
      conditions[key] = value;
  }

  enum class TemplateType {
    List,
    Page,
    Unknown
  };

  TemplateType type = TemplateType::Unknown;
  string tag;
  wstring name;

  static const HTMLWriter &getWriter(TemplateType type, const string &tag, const vector<pair<string, wstring>> &options);
  
  struct TemplateInfo {
    string tag;
    wstring name;
    wstring desc;
    wstring file;
    bool userInstalled = false;
    bool hasInnerPage = false;
    bool hasOuterPage = false;
    bool hasTimer = false;
    bool empty() const { return tag.empty(); };
  };

  static void enumTemplates(TemplateType type, vector<TemplateInfo> &descriptionFile);

  static TemplateInfo getTemplateInfo(TemplateType type, const string& tag, bool acceptMissing);
  
  static wstring getTemplateFile(TemplateType type, const string& tag) {
    return getTemplateInfo(type, tag, false).file;
  }

  void read(const wstring &fileName);

  void generate(gdioutput &gdi,
                std::ostream &fout,
                const wstring &title,
                const wstring &contentDescription,
                bool respectPageBreak,
                const int nRows,
                const int numCol,
                const int interval,
                const int marginPercent,
                double scal) const;

  void getPage(const oEvent &oe, string &out) const;

  static void writeHTML(gdioutput &gdi, std::ostream &dout, const wstring &title, 
                        bool includeImages,
                        const wstring& imageDirectoryDestination,
                        int refreshTimeOut, double scale);

  static void writeTableHTML(gdioutput &gdi, std::ostream &fout,
                             const wstring &title,
                             bool includeImages,
                             const wstring &imageDirectoryDestination,
                             bool simpleFormat,
                             int refreshTimeOut,
                             double scale);

  static void writeTableHTML(gdioutput &gdi, const wstring &file,
                             const wstring &title,
                             int refreshTimeOut,
                             double scale);

  static void writeHTML(gdioutput &gdi, const wstring &file, 
                        const wstring &title, int refreshTimeOut, double scale);

  static void write(gdioutput& gdi, const wstring& file, const wstring& title, int refresh, oListParam& param, const oEvent& oe);
  static void write(gdioutput& gdi, std::ostream& fout, const wstring& title, int refresh, oListParam& param, const oEvent& oe);

  static void write(gdioutput& gdi, const wstring& file, const wstring& title, const wstring& contentsDescription,
    bool respectPageBreak, const string& typeTag, int refresh,
    int rows, int cols, int time_ms, int margin, double scale);

  static void write(gdioutput& gdi, std::ostream& fout, const wstring& title, 
    bool includeImages,
    const wstring& imageDirectoryDestination,
    const wstring& contentsDescription,
    bool respectPageBreak, const string& typeTag, int refresh,
    int rows, int cols, int time_ms, int margin, double scale);
};