/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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

#include <algorithm>
#include <cassert>
#include <tuple>

#include "oEvent.h"

#include "listeditor.h"
#include "generalresult.h"
#include "metalist.h"
#include "gdioutput.h"
#include "meosexception.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "autocomplete.h"

extern oEvent *gEvent;

const int MAXLISTPARAMID = 10000000;

class PositionVer2 {
  struct Block {
    Block(int position, int indent, int width, bool packPrevious) : position(position),
                                                                    indent(indent), width(width),
                                                                    packPrevious(packPrevious), originalPos(position) {
    
    }
    int position; // Actual position
    int indent; // Indent inside block
    bool packPrevious; // Do not move from 
    const int originalPos; // Original position
    int width;    // Original block width
    int end() const { return position + width; }
    int getPos() const { return position + indent; }
    int getOriginalPos() const { return originalPos + indent; }

    vector<pair<int, int>> alignWith;

  };
  map<string, pair<int, int>> pmap; // Map from label into block
  vector<vector<Block>> blocks; // Pair of position, specified (minimal) width
  
  void move(int row, int ix, int newPos) {
    auto &b = blocks[row][ix];
    int delta = newPos - b.position;
    if (delta == 0)
      return;
    assert(delta > 0);
        
    //b.position += delta;
    if (ix > 0 && b.packPrevious)
      move(row, ix - 1, blocks[row][ix-1].position + delta);
    else {
      // Move rest of row
      for (size_t j = ix; j < blocks[row].size(); j++)
        blocks[row][j].position += delta;

      // Move aligned blocks
      for (auto &a : b.alignWith)
        move(a.first, a.second, newPos);
    }
  }

public:
  PositionVer2() : blocks(1) {
  }

  int getWidth() const {
    int w = 0;
    for (auto &r : blocks) {
      if (!r.empty()) {        
        w = max(w, r.back().end());
      }
    }
    return w;
  }

  void postAdjust() {

  }

  bool canMoveTo(int row, int ix, int newPos) const {
    auto &r = blocks[row];
    int delta = newPos - r[ix].position;
    if (delta == 0)
      return true;

    if (std::abs(delta) > r[ix].width / 3 )
      return false;

    if (ix > 0 && r[ix].packPrevious && !canMoveTo(row, ix - 1, r[ix - 1].position + delta))
      return false;
  }

  /*
  int getAlignPos(int row, int pos) {
    auto &r = blocks[row];
    
    vector<double> score(r.size());
    for (size_t j = 0; j < r.size(); j++) {
      for (r[j].packPrevious)
        continue; // No align

    }


  }*/

  void add(const string &name, int width, int indent, bool packPrevious) {
    pmap[name] = make_pair(blocks.size() - 1, blocks.back().size());
    int pos = 0;
    if (blocks.back().size() > 0)
      pos = blocks.back().back().end();
    blocks.back().emplace_back(pos, indent, width + indent, packPrevious);
  }

  void add(const string &name, const int width) {
    add(name, width, 0, false);
  }

  void addAlignWith(const string &alignWith, const string &newName,
                    int width, int indent, bool packPrevious) {
    auto align = pmap.find(alignWith);
    if (align == pmap.end())
      throw meosException("Internal error");
    int row = align->second.first;
    int ix = align->second.second;

    int pos = 0;
    if (blocks.back().size() > 0)
      pos = blocks.back().back().end();

    int apos = blocks[row][ix].position;
    if (apos > pos)
      pos = apos;
    else 
      move(row, ix, pos);

    pair<int, int> q(blocks.size() - 1, blocks.back().size());
    pmap[newName] = q;
    blocks[row][ix].alignWith.emplace_back(q);

    blocks.back().emplace_back(pos, indent, width, packPrevious);
    blocks.back().back().alignWith.emplace_back(row, ix);    
  }
  
  bool addAlignNext(const string &newName, int width, int indent) {
    int best = 10000;
    string bestName;
    
    int pos = 0;
    if (blocks.back().size() > 0)
      pos = blocks.back().back().end();

    for (auto &a : pmap) {
      auto &b = blocks[a.second.first][a.second.second];
      if (b.position >= pos) {
        int delta = b.originalPos - pos;
        if (delta <= best) {
          best = delta;
          bestName = a.first;
        }
      }
    }
    if (bestName.empty())
      return false;
    addAlignWith(bestName, newName, width, indent, false);
    return true;
  }


  void newRow() {
    if (!blocks.back().empty())
      blocks.emplace_back();
  }

  int get(const string &name) const  {
    return get(name, 1.0);
  }

  int get(const string &name, double scale) const {
    auto r = pmap.find(name);
    if (r != pmap.end())
      return int(blocks[r->second.first][r->second.second].getPos() * scale);
    return 0;
  }

  int getOriginalPos(const string &name) {
    return getOriginalPos(name, 1.0);
  }

  int getOriginalPos(const string &name, double scale) const {
    auto r = pmap.find(name);
    if (r != pmap.end())
      return int(blocks[r->second.first][r->second.second].getOriginalPos() * scale);
    return 0;
  }

};



oListParam::oListParam() {
  lockUpdate = false;
  listCode = EStdResultList; //Just need a default
  cb = 0;
  legNumber = 0;
  useControlIdResultTo = 0;
  useControlIdResultFrom = 0;
  filterMaxPer = 0;
  pageBreak = false;
  showInterTimes = false;
  showSplitTimes = false;
  splitAnalysis = false;
  useLargeSize = false;
  saved = false;
  inputNumber = 0;
  nextList = 0; // No linked list
  previousList = 0;
  relayLegIndex = -1;
  bgColor = -1;
  fgColor = -1;
  bgColor2 = -1;

  nColumns = 0;
  animate = true;
  timePerPage = 8000;
  margin = 5;
  screenMode = 0;

  htmlScale = 1.0;
  htmlRows = 60;
  htmlTypeTag = "free";
}

void oListParam::serialize(xmlparser &xml, 
                           const MetaListContainer &container, 
                           const map<int, int> &idToIndex) const {
  xml.startTag("ListParam", "Name", name);
  xml.write("ListId", container.getUniqueId(listCode));
  string sel;
  for (int s : selection) {
    if (!sel.empty())
      sel += ";";
    sel += itos(s);
  }
  xml.write("ClassId", sel);
  xml.write("LegNumber", legNumber);
  xml.write("FromControl", useControlIdResultFrom);
  xml.write("ToControl", useControlIdResultTo);
  xml.write("MaxFilter", filterMaxPer);
  xml.write("Title", title);
  xml.writeBool("Large", useLargeSize);
  xml.writeBool("PageBreak", pageBreak);
  xml.writeBool("HideHeader", !showHeader);
  xml.write("AgeFilter", int(ageFilter));
  xml.writeBool("ShowNamedSplits", showInterTimes);
  xml.writeBool("ShowInterTitle", showInterTitle);
  xml.writeBool("ShowSplits", showSplitTimes);
  xml.writeBool("ShowAnalysis", splitAnalysis);
  xml.write("InputNumber", inputNumber);
  if (nextList != 0) {
    auto res = idToIndex.find(nextList);
    if (res != idToIndex.end())
      xml.write("NextList", res->second);
  }
  if (bgColor != -1)
    xml.write("BGColor", itos(bgColor));
  if (bgColor2 != -1)
    xml.write("BGColor2", itos(bgColor2));

  if (fgColor != -1)
    xml.write("FGColor", itos(fgColor));
  xml.write("Image", bgImage);

  xml.write("ScreenMode", screenMode);

  if (nColumns != 0 || htmlTypeTag != "free") {
    xml.write("NumColumns", itos(nColumns));
    xml.writeBool("Animate", animate);
    xml.write("TimePerPage", timePerPage);
    xml.write("Margin", margin);
  }

  if (htmlTypeTag != "free") {
    xml.write("HTML", htmlTypeTag);
    xml.write("PageRows", htmlRows);
    xml.write("Scale", to_string(htmlScale));
  }

  xml.endTag();
}

void oListParam::deserialize(const xmlobject &xml, const MetaListContainer &container) {
  xml.getObjectString("Name", name);
  string id;
  xml.getObjectString("ListId", id);
  listCode = container.getCodeFromUnqiueId(id);

  string sel;
  xml.getObjectString("ClassId", sel);
  vector<string> selVec;
  split(sel, ";", selVec);
  for (size_t k = 0; k < selVec.size(); k++) {
    selection.insert(atoi(selVec[k].c_str()));
  }
  legNumber = xml.getObjectInt("LegNumber");
  useControlIdResultFrom = xml.getObjectInt("FromControl");
  useControlIdResultTo = xml.getObjectInt("ToControl");
  filterMaxPer = xml.getObjectInt("MaxFilter");
  xml.getObjectString("Title", title);

  useLargeSize = xml.getObjectBool("Large");
  pageBreak = xml.getObjectBool("PageBreak");
  showHeader = !xml.getObjectBool("HideHeader");

  showInterTimes = xml.getObjectBool("ShowNamedSplits");
  showSplitTimes = xml.getObjectBool("ShowSplits");
  splitAnalysis = xml.getObjectBool("ShowAnalysis");
  showInterTitle = xml.getObjectBool("ShowInterTitle");
  inputNumber = xml.getObjectInt("InputNumber");
  nextList = xml.getObjectInt("NextList");
  ageFilter = AgeFilter(xml.getObjectInt("AgeFilter"));
  xmlobject bg = xml.getObject("BGColor");
  if (bg)
    bgColor = bg.getInt();

  xmlobject bg2 = xml.getObject("BGColor2");
  if (bg2)
    bgColor2 = bg2.getInt();

  xmlobject fg = xml.getObject("FGColor");
  if (fg)
    fgColor = fg.getInt();

  xml.getObjectString("Image", bgImage);

  nColumns = xml.getObjectInt("NumColumns");

  screenMode = xml.getObjectInt("ScreenMode");
  animate = xml.getObjectBool("Animate");

  if (xml.got("TimePerPage"))
    timePerPage = xml.getObjectInt("TimePerPage");

  if (xml.got("Margin"))
    margin = xml.getObjectInt("Margin");

  if (xml.got("HTML"))
    xml.getObjectString("HTML", htmlTypeTag);

  if (xml.got("Scale")) {
    string tmp;
    xml.getObjectString("Scale", tmp);
    htmlScale = atof(tmp.c_str());
    if (!(htmlScale > 0.1 && htmlScale < 1000))
      htmlScale = 1.0;
  }

  if (xml.got("PageRows")) {
    htmlRows = xml.getObjectInt("PageRows");
  }

  saved = true;
}

void oListParam::getCustomTitle(wchar_t *t) const
{
  if (!title.empty())
    wcscpy_s(t, 256, makeDash(title).c_str());
}

const wstring &oListParam::getCustomTitle(const wstring &t) const
{
  if (!title.empty())
    return title;
  else
    return t;
}

MetaList::MetaList() {
  data.resize(4);
  fontFaces.resize(4);
  hasResults_ = false;
  initSymbols();
  listType = oListInfo::EBaseTypeRunner;
  listSubType = oListInfo::EBaseTypeNone;
  sortOrder = SortByName;
  supportFromControl = false;
  supportToControl = false;
  hideLegSelection = false;
}

MetaListPost::MetaListPost(EPostType type_, EPostType align_, int leg_) : type(type_),
    alignType(align_), leg(leg_), minimalIndent(0), alignBlock(true), blockWidth(0), font(formatIgnore),
    mergeWithPrevious(false), textAdjust(0), color(colorDefault)
{}

int checksum(const string &str) {
  int ret = 0;
  for (size_t k = 0; k<str.length(); k++)
    ret = ret * 19 + str[k];
  return ret;
}

int checksum(const wstring &str) {
  int ret = 0;
  for (size_t k = 0; k<str.length(); k++)
    ret = ret * 19 + str[k];
  return ret;
}


void MetaList::initUniqueIndex() const {
  __int64 ix = 0;

  for (int i = 0; i<4; i++) {
    const vector< vector<MetaListPost> > &lines = data[i];
    for (size_t j = 0; j<lines.size(); j++) {
      const vector<MetaListPost> &cline = lines[j];
      for (size_t k = 0; k<cline.size(); k++) {
        const MetaListPost &mp = cline[k];
        int value = mp.alignBlock;
        value = value * 31 + mp.type;
        value = value * 31 + mp.leg;
        value = value * 31 + checksum(mp.alignWithText);
        value = value * 31 + checksum(DynamicResult::undecorateTag(resultModule));
        value = value * 31 + mp.alignType;
        value = value * 31 + mp.blockWidth;
        value = value * 31 + mp.font;
        value = value * 31 + checksum(mp.text);
        value = value * 31 + mp.minimalIndent;
        value = value * 31 + mp.mergeWithPrevious;
        value = value * 31 + mp.color;
        value = value * 31 + mp.textAdjust;
        ix = ix * 997 + value;
      }
    }
  }

  unsigned yx = 0;
  yx = yx * 31 + checksum(listName);
  yx = yx * 31 + listType;
  yx = yx * 31 + listSubType;
  yx = yx * 31 + sortOrder;
  for (int i = 0; i < 4; i++) {
    yx = yx * 31 + checksum(fontFaces[i].font);
    yx = yx * 31 + fontFaces[i].scale;
    if (fontFaces[i].extraSpaceAbove != 0)
      yx = yx * 39 + fontFaces[i].extraSpaceAbove;
  }
  yx = yx * 31 + checksum(DynamicResult::undecorateTag(resultModule));
  yx = yx * 31 + supportFromControl;
  yx = yx * 31 + supportToControl;
  yx = yx * 37 + hideLegSelection;

  for (set<EFilterList>::const_iterator it = filter.begin(); it != filter.end(); ++it)
    yx = yx * 31 + *it;
  for (set<ESubFilterList>::const_iterator it = subFilter.begin(); it != subFilter.end(); ++it)
    yx = yx * 29 + *it;

  uniqueIndex = "A" + itos(ix) + "B" + itos(yx);
}

bool MetaList::isBreak(int x) {
  return isspace(x) || x == '.' || x == ',' ||
          x == '-'  || x == ':' || x == ';' || x == '('
          || x == ')' || x=='/' || (x>30 && x < 127 && !isalnum(x));
}

wstring MetaList::encode(EPostType type, const wstring &inputS, bool &foundSymbol) {
  if (inputS.empty()) {
    foundSymbol = true; // No symbol needed
    return inputS;
  }
  wstring out;
  wstring input = lang.tl(inputS);
  
  out.reserve(input.length() + 5);
  int sCount = 0;
  if (type == EPostType::lString)
    sCount = 1; // No symbols expected in string
  
  for (size_t k = 0; k<input.length(); k++) {
    int c = input[k];
    int p = k > 0 ? input[k-1] : ' ';
    int n = k+1 < input.length() ? input[k+1] : ' ';

    if (c == '%') {
      out.push_back('%');
      out.push_back('%');
    }
    else if (c == 'X' &&  isBreak(n) && isBreak(p) && sCount == 0) {
      out.push_back('%');
      out.push_back('s');
      sCount++;
    }
    else
      out.push_back(c);
  }

  foundSymbol = sCount > 0;
  return out;
}

MetaListPost &MetaList::add(ListIndex ix, const MetaListPost &post) {
  if (data[ix].empty())
    addRow(ix);

  vector<MetaListPost> &d = data[ix].back();
  d.push_back(post);
  return d.back();
}

void MetaList::addRow(int ix) {
  data[ix].push_back(vector<MetaListPost>());
}

static void setFixedWidth(oPrintPost &added, 
                          const map<tuple<int,int,int>, int> &indexPosToWidth, 
                          int type, int j, int k) {
  map<tuple<int,int,int>, int>::const_iterator res = indexPosToWidth.find(tuple<int,int,int>(type, j, k));
  if (res != indexPosToWidth.end())
    added.fixedWidth = res->second;
  else
    added.fixedWidth = 0;
}

void MetaList::interpret(oEvent *oe, const gdioutput &gdi, const oListParam &par, oListInfo &li) const {
  const MetaList &mList = *this;
  PositionVer2 pos;
  const bool large = par.useLargeSize;
  li.lp = par;
  gdiFonts normal, header, small, italic;
  double s_factor;
  oe->calculateResults({}, oEvent::ResultType::ClassResult, false);
  map<pair<gdiFonts, int>, int> fontHeight;

  for (size_t k = 0; k < fontFaces.size(); k++) {
    for (map<gdiFonts, string>::const_iterator it = fontToSymbol.begin();
      it != fontToSymbol.end(); ++it) {
        wstring face = fontFaces[k].font;
        if (fontFaces[k].scale > 0 && fontFaces[k].scale != 100) {
          face += L";" + itow(fontFaces[k].scale/100) + L"." + itow(fontFaces[k].scale%100);
        }
        fontHeight[make_pair(it->first, int(k))] = int(gdi.getLineHeight(it->first, face.c_str()) / gdi.getScale());
    }
  }
  int lineHeight;
  if (large) {
    s_factor = 0.9;
    normal = fontLarge;
    header = boldLarge;
    small = normalText;
    lineHeight = 22;
    italic = italicMediumPlus;
  }
  else {
    lineHeight = 14;
    s_factor = 1.0;
    normal = normalText;
    header = boldText;
    small = italicSmall;
    italic = italicText;
  }

  map<pair<EPostType, int>, string> labelMap; // From <type, leg> to label
  map<wstring, string> stringLabelMap;

  set<EPostType> skip;
  li.calcResults = false;
  li.calcTotalResults = false;
  li.rogainingResults = false;
  li.calculateLiveResults = false;
  li.calcCourseClassResults = false;
  li.calcCourseResults = false;

  if (par.useControlIdResultFrom > 0 || par.useControlIdResultTo > 0)
    li.needPunches = oListInfo::PunchMode::SpecificPunch;
  const bool isPunchList = mList.listSubType == oListInfo::EBaseTypeCoursePunches ||
                           mList.listSubType == oListInfo::EBaseTypeAllPunches;
  if (isPunchList) {
    if ((!li.filter(EFilterList::EFilterHasResult) && !li.filter(EFilterList::EFilterHasPrelResult)) 
        || li.sortOrder == SortOrder::ClassLiveResult)
      li.needPunches = oListInfo::PunchMode::AnyPunch;
  }
  
  map<tuple<int, int, int>, int> indexPosToWidth;
  map<pair<int, int>, int> linePostCount;

  vector<vector<vector<MetaListPost>>> dataCopy(4);

  auto getHead = [&dataCopy]() -> const vector<vector<MetaListPost>>& {
    return dataCopy[MLHead];
  };

  auto getSubHead = [&dataCopy]() -> const vector<vector<MetaListPost>>& {
    return dataCopy[MLSubHead];
  };

  auto getList = [&dataCopy]() -> const vector<vector<MetaListPost>>&{
    return dataCopy[MLList];
  };

  auto subListIx = [&dataCopy]() -> const vector<vector<MetaListPost>>& {
    return dataCopy[MLSubList];
  };

  for (int i = 0; i<4; i++) {
    const vector< vector<MetaListPost> > &lines = mList.data[i];
    gdiFonts defaultFont = normal;
    switch (i) {
      case 0:
      case 1:
        defaultFont = header;
      break;
      case 3: {
        if (isPunchList)
          defaultFont = small;
        else
          defaultFont = italic;
      }
    }
    
    auto isAllStageType = [](const MetaListPost &mp) {
      return MetaList::isAllStageType(mp.type) && mp.leg == -1;
    };

    for (size_t j = 0; j<lines.size(); j++) {
      if (i == 0 && j == 0)
        defaultFont = boldLarge;
     
      dataCopy[i].emplace_back();
      vector<MetaListPost> &lineCopy = dataCopy[i].back();

      int firstStage = -1;
      for (size_t k = 0; k < lines[j].size(); k++) {
        if (isAllStageType(lines[j][k])) {
          if (firstStage == -1)
            firstStage = k;

          if (k + 1 == lines[j].size() || !isAllStageType(lines[j][k + 1])) {
            int ns = min(oe->getStageNumber(), 30);
            for (int s = 1; s < ns; s++) {
              for (size_t f = firstStage; f <= k; f++) {
                lineCopy.push_back(lines[j][f]);
                lineCopy.back().leg = s - 1;
              }
            }
            firstStage = -1;
          }
        }
        else {
          lineCopy.push_back(lines[j][k]);
        }
      }

      const vector<MetaListPost> &cline = lineCopy;
      for (size_t k = 0; k<cline.size(); k++) {
        const MetaListPost &mp = cline[k];
        int extraMinWidth = 0;
        if (mp.type == lCmpName) {
          extraMinWidth = par.getCustomTitle(mp.getText()).length();
        }

        // Automatically determine what needs to be calculated
        if (mp.type == lTeamPlace || mp.type == lRunnerPlace || mp.type == lRunnerGeneralPlace) {
          if (!li.calcResults) {
            oe->calculateResults(set<int>(), oEvent::ResultType::ClassResult);
            oe->calculateTeamResults(set<int>(), oEvent::ResultType::ClassResult);
          }
          li.calcResults = true;
        }
        if (mp.type == lRunnerTotalPlace || mp.type == lRunnerPlaceDiff
                || mp.type == lTeamTotalPlace || mp.type == lTeamPlaceDiff 
                || mp.type == lRunnerGeneralPlace) {
          if (!li.calcTotalResults) {
            oe->calculateResults(set<int>(), oEvent::ResultType::TotalResult);
            oe->calculateTeamResults(set<int>(), oEvent::ResultType::TotalResult);
          }
          li.calcTotalResults = true;
        }
        else if (mp.type == lRunnerRogainingPoint || mp.type == lRunnerRogainingPointTotal
              || mp.type == lTeamRogainingPoint || mp.type == lTeamRogainingPointTotal) {
          li.rogainingResults = true;
        }
        else if (mp.type == lRunnerClassCoursePlace || mp.type == lRunnerClassCourseTimeAfter) {
          if (!li.calcCourseClassResults)
            oe->calculateResults(set<int>(), oEvent::ResultType::ClassCourseResult);

          li.calcCourseClassResults = true;
        }
        else if (mp.type == lRunnerCoursePlace) {
          if (!li.calcCourseResults)
            oe->calculateResults(set<int>(), oEvent::ResultType::ClassCourseResult);

          li.calcCourseResults = true;
        }
        else if (mp.type == lRunnerTempTimeAfter || mp.type == lRunnerTempTimeStatus) {
          if (li.needPunches == oListInfo::PunchMode::NoPunch)
          li.needPunches = oListInfo::PunchMode::SpecificPunch;
        }

        string label = "P" + itos(i*1000 + j*100 + k);

        if (mp.type == lRunnerBib || mp.type == lTeamBib) {
          if (!oe->hasBib(mp.type == lRunnerBib, mp.type == lTeamBib))
            skip.insert(mp.type);
        }

        int width = 0;
        if (skip.count(mp.type) == 0 && (k==0 || !mp.mergeWithPrevious)) {
          gdiFonts font = defaultFont;
          if (mp.font != formatIgnore)
            font = mp.font;

          bool dmy;
          vector<pair<EPostType, wstring>> typeFormats;
          typeFormats.push_back(make_pair(mp.type, encode(mp.type, mp.text, dmy)));
          size_t kk = k+1;
          //Add merged entities
          while (kk < cline.size() && cline[kk].mergeWithPrevious) {
            typeFormats.push_back(make_pair(cline[kk].type, encode(cline[kk].type, cline[kk].text, dmy)));
            kk++;
          }
          if (mp.limitWidth && mp.blockWidth > 0)
            width = gdi.scaleLength(mp.blockWidth);
          else
            width = li.getMaxCharWidth(oe, gdi, par.selection, typeFormats, font,
                                     oPrintPost::encodeFont(fontFaces[i].font, 
                                     fontFaces[i].scale).c_str(), 
                                     large, max(mp.blockWidth, extraMinWidth));

          ++linePostCount[make_pair(i, j)]; // Count how many positions on this line
          indexPosToWidth[tuple<int,int,int>(i, j, k)] = width;
        }

        if (mp.alignType == lNone) {
          //pos.add(label, width, width);
          //pos.indent(mp.minimalIndent);
          pos.add(label, width, mp.minimalIndent, mp.packPrevious);
          
        }
        else {
          if (mp.alignType == lAlignNext) {
            if (!pos.addAlignNext(label, width, mp.minimalIndent))
              pos.add(label, width, mp.minimalIndent, mp.packPrevious);
            //pos.alignNext(label, width, mp.alignBlock);
          }
          else if (mp.alignType == lString) {
            if (stringLabelMap.count(mp.alignWithText) == 0) {
              throw meosException(L"Don't know how to align with 'X'#" + mp.alignWithText);
            }
            //pos.update(stringLabelMap[mp.alignWithText], label, width, mp.alignBlock, true);
            pos.addAlignWith(stringLabelMap[mp.alignWithText], label, width, mp.minimalIndent, mp.packPrevious);
          }
          else {
            auto res = labelMap.find(make_pair(mp.alignType, mp.leg));
            if (res == labelMap.end())
              res = labelMap.find(make_pair(mp.alignType, -1));

            if (res == labelMap.end()) {
              throw meosException(L"Don't know how to align with 'X'#" + lang.tl(typeToSymbol[mp.alignType]));
            }

            //pos.update(res->second, label, width, mp.alignBlock, true);
            pos.addAlignWith(res->second, label, width, mp.minimalIndent, mp.packPrevious);
          }

          //pos.indent(mp.minimalIndent);
        }
        labelMap[make_pair(mp.type, mp.leg)] = label;
        labelMap.emplace(make_pair(mp.type, -1), label);

        if (!mp.text.empty())
          stringLabelMap[mp.text] = label;

      }
      pos.newRow();
    }
    pos.newRow();
  }
  /*
  bool c = true;
  while (c) {
    c = pos.postAdjust();
  }*/

  pos.postAdjust();
  int dy = 0, next_dy = 0;
  oPrintPost *last = 0;
  oPrintPost *base = 0;

  auto indexPosToWidthSrc = indexPosToWidth;

  int totalWidth = pos.getWidth();
  for (map<pair<int, int>, int>::iterator it = linePostCount.begin(); it != linePostCount.end(); ++it) {
    if (it->second == 1) {
      int base = it->first.first;
      if (base == MLSubList && listSubType == oListInfo::EBaseTypeCoursePunches)
        continue; // This type of list requires actual width
      indexPosToWidth[tuple<int,int,int>(base, it->first.second, 0)] = totalWidth; 
    }
  }

  pClass sampleClass = 0;

  if (!par.selection.empty())
    sampleClass = oe->getClass(*par.selection.begin());
  if (!sampleClass) {
    vector<pClass> cls;
    oe->getClasses(cls, false);
    if (!cls.empty())
      sampleClass = cls[0];
  }
  pair<int, bool> parLegNumber = par.getLegInfo(sampleClass);
  bool capitalizeTitle = lang.capitalizeWords();
  resultToIndex.clear();
  /*if (large == false && par.pageBreak == false) {*/
  {
    int head_dy = mList.getExtraSpace(MLHead);
    for (size_t j = 0; j < getHead().size(); j++) {
      const vector<MetaListPost> &cline = getHead()[j];
      next_dy = 0;
      for (size_t k = 0; k<cline.size(); k++) {
        const MetaListPost &mp = cline[k];
        if (skip.count(mp.type) == 1)
          continue;

        string label = "P" + itos(0*1000 + j*100 + k);
        bool dmy;
        wstring text = makeDash(encode(cline[k].type, cline[k].text, dmy));
        if (capitalizeTitle)
          capitalizeWords(text);

        gdiFonts font = normalText;
        if (j == 0)
          font = boldLarge;

        if (mp.type == lCmpName) {
          text = makeDash(par.getCustomTitle(text));
        }

        if (mp.font != formatIgnore)
          font = mp.font;

        oPrintPost &added = li.addHead(oPrintPost(mp.type, text, font|mp.textAdjust,
                                       pos.get(label), dy + head_dy, mp.leg == -1 ? parLegNumber : make_pair(mp.leg, true))).
                                       setFontFace(fontFaces[MLHead].font,
                                          fontFaces[MLHead].scale);

        added.resultModuleIndex = getResultModuleIndex(oe, li, mp);
        setFixedWidth(added, indexPosToWidth, MLHead, j, k);
        added.xlimit = indexPosToWidthSrc[tuple<int, int, int>(MLHead, j, k)];
        added.color = mp.color;
        if (!mp.mergeWithPrevious)
          base = &added;
        added.useStrictWidth = mp.getLimitBlockWidth();
        if (added.useStrictWidth)
          added.format |= textLimitEllipsis;

        if (last && mp.mergeWithPrevious) {
          //last->mergeWith = &added;
          last->doMergeNext = true;
          if (base) {
            base->fixedWidth += added.fixedWidth;
            added.fixedWidth = 0;
          }
        }
        last = &added;

        next_dy = max(next_dy, fontHeight[make_pair(font, MLHead)]);
      }
      dy += next_dy;
      next_dy = lineHeight;
    }
  }

  dy = lineHeight;
  int subhead_dy = mList.getExtraSpace(MLSubHead);
    
  last = 0;
  base = 0;
  for (size_t j = 0; j<getSubHead().size(); j++) {
    next_dy = 0;
    const vector<MetaListPost> &cline = getSubHead()[j];
    for (size_t k = 0; k<cline.size(); k++) {
      const MetaListPost &mp = cline[k];
      if (skip.count(mp.type) == 1)
        continue;

      string label = "P" + itos(1*1000 + j*100 + k);
      gdiFonts font = header;
      if (mp.font != formatIgnore)
        font = mp.font;
      bool dmy;
      wstring text = encode(mp.type, mp.text, dmy);
      if (capitalizeTitle)
        capitalizeWords(text);

      oPrintPost &added = li.addSubHead(oPrintPost(mp.type, text, font|mp.textAdjust,
                                        pos.get(label, s_factor), dy + subhead_dy, cline[k].leg == -1 ? parLegNumber : make_pair(cline[k].leg, true))).
                                        setFontFace(fontFaces[MLSubHead].font,
                                                    fontFaces[MLSubHead].scale);

      added.resultModuleIndex = getResultModuleIndex(oe, li, mp);
      setFixedWidth(added, indexPosToWidth, MLSubHead, j, k);
      added.xlimit = indexPosToWidthSrc[tuple<int, int, int>(MLSubHead, j, k)];
      added.color = mp.color;
      if (!mp.mergeWithPrevious)
        base = &added;

      added.useStrictWidth = mp.getLimitBlockWidth();
      if (added.useStrictWidth)
        added.format |= textLimitEllipsis;

      if (last && mp.mergeWithPrevious) {
        //last->mergeWith = &added;
        last->doMergeNext = true;
        if (base) {
          base->fixedWidth += added.fixedWidth;
          added.fixedWidth = 0;
        }
      }
      last = &added;

      next_dy = max(next_dy, fontHeight[make_pair(font, MLSubHead)]);
    }
    dy += next_dy;
  }

  last = 0;
  base = 0;
  int list_dy = mList.getExtraSpace(MLList);
  dy = 0;
  for (size_t j = 0; j < getList().size(); j++) {
    const vector<MetaListPost> &cline = getList()[j];
    next_dy = 0;
    for (size_t k = 0; k<cline.size(); k++) {
      const MetaListPost &mp = cline[k];
      if (skip.count(mp.type) == 1)
        continue;

      string label = "P" + itos(2*1000 + j*100 + k);
      gdiFonts font = normal;
      if (mp.font != formatIgnore)
        font = mp.font;

      next_dy = max(next_dy, fontHeight[make_pair(font, MLList)]);
      bool dmy;
      oPrintPost &added = li.addListPost(oPrintPost(mp.type, encode(mp.type, mp.text, dmy), font|mp.textAdjust,
                                         pos.get(label, s_factor),
                                         dy + list_dy, cline[k].leg == -1 ? parLegNumber : make_pair(cline[k].leg, true))).
                                         setFontFace(fontFaces[MLList].font,
                                                     fontFaces[MLList].scale);

      added.resultModuleIndex = getResultModuleIndex(oe, li, mp);
      setFixedWidth(added, indexPosToWidth, MLList, j, k);
      added.xlimit = indexPosToWidthSrc[tuple<int, int, int>(MLList, j, k)];
      added.useStrictWidth = mp.getLimitBlockWidth();
      if (added.useStrictWidth)
        added.format |= textLimitEllipsis;

      added.color = mp.color;
      if (!mp.mergeWithPrevious)
        base = &added;

      if (last && mp.mergeWithPrevious) {
        last->doMergeNext = true;
        if (base) {
          base->fixedWidth += added.fixedWidth;
          added.fixedWidth = 0;
        }
      }
      last = &added;
    }
    dy += next_dy;
  }

  int sublist_dy = mList.getExtraSpace(MLSubList);
  last = 0;
  base = 0;
  dy = 0;
  for (size_t j = 0; j<getSubList().size(); j++) {
    const vector<MetaListPost> &cline = getSubList()[j];
    next_dy = 0;
    for (size_t k = 0; k<cline.size(); k++) {
      const MetaListPost &mp = cline[k];
      if (skip.count(mp.type) == 1)
        continue;
      gdiFonts font = small;
      if (mp.font != formatIgnore)
        font = mp.font;
      if (mp.font == normalText && large)
        font = normal;

      string label = "P" + itos(3*1000 + j*100 + k);
      int xp;
      if (isPunchList)
        xp = pos.getOriginalPos(label, s_factor);
      else
        xp = pos.get(label, s_factor);

      next_dy = max(next_dy, fontHeight[make_pair(font, MLSubList)]);
      bool dmy;
      oPrintPost &added = li.addSubListPost(oPrintPost(mp.type, encode(mp.type, mp.text, dmy), font|mp.textAdjust,
                                            xp, dy+sublist_dy, mp.leg == -1 ? parLegNumber : make_pair(mp.leg, true))).
                                            setFontFace(fontFaces[MLSubList].font,
                                                        fontFaces[MLSubList].scale);

      added.color = mp.color;
      if (!mp.mergeWithPrevious)
        base = &added;

      added.resultModuleIndex = getResultModuleIndex(oe, li, mp);
      setFixedWidth(added, indexPosToWidth, MLSubList, j, k);
      added.xlimit = indexPosToWidthSrc[tuple<int, int, int>(MLSubList, j, k)];
      added.useStrictWidth = mp.getLimitBlockWidth();
      if (added.useStrictWidth)
        added.format |= textLimitEllipsis;

      if (last && mp.mergeWithPrevious) {
        last->doMergeNext = true;
        if (base) {
          base->fixedWidth += added.fixedWidth;
          added.fixedWidth = 0;
        }
      }
      last = &added;
    }
    dy += next_dy;
  }

  li.listType = getListType();
  li.listSubType = listSubType;
  li.sortOrder = sortOrder;
  for (set<EFilterList>::const_iterator it = filter.begin();
                                      it != filter.end(); ++it) {
    li.setFilter(*it);
  }
  if (li.filter(EFilterList::EFilterAnyResult) || sortOrder == SortOrder::ClassLiveResult)
    li.calculateLiveResults = true;

  for (set<ESubFilterList>::const_iterator it = subFilter.begin();
                                      it != subFilter.end(); ++it) {
    li.setSubFilter(*it);
  }
  li.setResultModule(resultModule);
  li.supportFrom = supportFromControl;
  li.supportTo = supportToControl;
  li.resType = getResultType();
  if (!resultModule.empty() || li.calcResults || li.calcCourseClassResults 
      || li.calcTotalResults || li.calcCourseResults)
    hasResults_ = true;

  if (li.sortOrder == SortOrder::CourseResult) {
    li.replaceType(EPostType::lRunnerPlace, EPostType::lRunnerCoursePlace, true);
  }

  if (li.sortOrder == SortOrder::ClassCourseResult) {
    li.replaceType(EPostType::lRunnerPlace, EPostType::lRunnerClassCoursePlace, true);
  }

}

void Position::indent(int ind) {
  int end = pos.size() - 1;
  if (end < 1)
    return;
  if (pos[end-1].first < ind) {
    int dx = ind - pos[end-1].first;
    pos[end-1].first += dx;
    pos[end].first += dx;
  }
}

void Position::newRow() {
  if (!pos.empty())
    pos.pop_back();
  pos.push_back(PosInfo(0, 0));
}

void Position::alignNext(const string &newname, const int width, bool alignBlock) {
  if (pos.empty())
    return;
  int p = pos.empty() ? 0 : pos.back().first;
  const int backAlign = 20;
  const int fwdAlign = max(width/2, 40);

  if (p==0)
    p = backAlign;

  int next = 0, prev = 0;
  int next_p = 100000, prev_p = -100000;

  int last = pos.size()-1;
  for (int k = pos.size()-2; k >= 0; k--) {
    last = k;
    if (pos[k+1].first < pos[k].first)
      break;
  }

  for (int k = 0; k <= last; k++) {
    if ( pos[k].first >= p && pos[k].first < next_p ) {
      next = k;
      next_p = pos[k].first;
    }

    if ( pos[k].first < p && pos[k].first > prev_p ) {
      prev = k;
      prev_p = pos[k].first;
    }
  }

  if ( p - prev_p < backAlign) {
    int delta = p - prev_p;
    for (size_t k = 0; k + 1 < pos.size(); k++) {
      if (pos[k].first >= prev_p)
        pos[k].first += delta;
    }
    update(prev, newname, width, alignBlock, false);
  }
  else {
    if (next > 0 && (next_p - p) < fwdAlign)
      update(next, newname, width, alignBlock, false);
    else
      add(newname, width, width);
  }
}

void Position::update(const string &oldname, const string &newname,
                      const int width, bool alignBlock, bool alignLock) {
  if (pmap.count(oldname) == 0)
    throw std::exception("Invalid position");

  int ix = pmap[oldname];

  update(ix, newname, width, alignBlock, alignLock);
}

void Position::update(int ix, const string &newname, const int width,
                      bool alignBlock, bool alignLock) {

  int last = pos.size()-1;

  if (alignLock) {
    pos[last].aligned = true;
    pos[ix].aligned = true;
  }
  int xlimit = pos[ix].first;
  if (xlimit < pos[last].first) {
    int delta = pos[last].first - xlimit;

    // Find last entry to update (higher row)
    int lastud = last;
    while (lastud>1 && pos[lastud].first >= pos[lastud-1].first)
      lastud--;

    for (int k = 0; k<lastud; k++) {
      if (pos[k].first >= xlimit)
        pos[k].first += delta;
    }
  }
  else
    pos[last].first = pos[ix].first;

  int ow = pos[ix+1].first - pos[ix].first;
  int nw = width;
  if (alignBlock && ow>0) {
    nw = max(ow, nw);
    if (nw > ow) {
      int delta = nw - ow;
      for (size_t k = 0; k<pos.size(); k++) {
        if (pos[k].first > pos[ix].first)
          pos[k].first += delta;
      }
    }
  }
  add(newname, nw, width);
}

int Position::getWidth() const {
  int w = 0;
  for (size_t k = 0; k < pos.size(); k++) {
    w = max(w, pos[k].first + pos[k].width);
  }
  return w;
}

void Position::add(const string &name, int width, int blockWidth) {

  if (pos.empty()) {
    pos.push_back(PosInfo(0, blockWidth));
    pmap[name] = 0;
    pos.push_back(PosInfo(width, 0));
  }
  else {
    pmap[name] = pos.size() - 1;
    pos.back().width = blockWidth;
    pos.push_back(PosInfo(width + pos.back().first, 0));
  }
}

int Position::get(const string &name) {
  return pos[pmap[name]].first;
}

int Position::get(const string &name, double scale) {
  return int(pos[pmap[name]].first * scale);
}

int Position::getOriginalPos(const string &name) {
  return pos[pmap[name]].originalPos;
}

int Position::getOriginalPos(const string &name, double scale) {
  return int(pos[pmap[name]].originalPos * scale);
}


struct PosOrderIndex {
  int pos;
  int index;
  int width;
  int row;
  bool aligned;
  bool operator<(const PosOrderIndex &x) const {return pos < x.pos;}
};

bool Position::postAdjust() {

  vector<PosOrderIndex> x;
  int row = 0;
  vector<int> aligned(1, -1);

  for (size_t k = 0; k < pos.size(); k++) {
    PosOrderIndex poi;
    poi.pos = pos[k].first;
    poi.index = k;
    poi.row = row;
    if (pos[k].aligned)
      aligned[row] = k;
    if (k + 1 == pos.size() || pos[k+1].first == 0) {
      poi.width = 100000;
      row++;
      aligned.push_back(-1);
    }
    else
      poi.width = pos[k].width;//pos[k+1].first - pos[k].first;//XXX//

    x.push_back(poi);
  }

  sort(x.begin(), x.end());

  // Transfer aligned blocks to x
  for (size_t k = 0; k<x.size(); k++) {
    int r = x[k].row;
    if (r!= -1 && aligned[r]>=x[k].index)
      x[k].aligned = true;
    else
      x[k].aligned = false;
  }

  pair<int, int> smallDiff(100000, -1);
  for (size_t k = 0; k<x.size(); k++) {
    if (k>0 && x[k-1].pos == x[k].pos || x[k].pos == 0)
      continue;
    int diff = 0;
    for (size_t j = k + 1; j < x.size(); j++) {
      if (x[j].pos > x[k].pos) {
        if (x[j].row == x[k].row)
          break;
        else {
          diff = x[j].pos - x[k].pos;


          bool skipRow = x[j].aligned || x[k].aligned;

          if (skipRow)
            break;

          for (size_t i = 0; i<x.size(); i++) {
            if (x[i].pos == x[k].pos && x[i].row == x[j].row) {
              skipRow = true;
              break;
            }
          }

          if (skipRow || diff > x[k].width / 2)
            break;

          if (diff < smallDiff.first) {
            smallDiff.first = diff;
            smallDiff.second = k;
          }
        }
      }
    }
  }

  if (smallDiff.second != -1) {
    int minK = smallDiff.second;
    int diff = smallDiff.first;
    int basePos = x[minK].pos;
    set<int> keepRow;
    for (size_t k = 0; k<x.size(); k++) {
      if (x[k].pos == basePos + diff) {
        keepRow.insert(x[k].row);
      }
    }
    bool changed = false;
    assert(keepRow.count(x[minK].row) == 0);
    for (size_t k = 0; k<x.size(); k++) {
      if (x[k].pos >= basePos && keepRow.count(x[k].row) == 0) {
        pos[x[k].index].first += diff;
        changed = true;
      }
    }

    return changed;
  }
  return false;
}

void MetaList::save(const wstring &file, const oEvent *oe) const {
  xmlparser xml;
  xml.openOutput(file.c_str(), true);
  save(xml, oe);
  xml.closeOut();
}

void MetaList::save(xmlparser &xml, const oEvent *oe) const {
  initSymbols();
  xml.startTag("MeOSListDefinition", "version", getMajorVersion());
//  xml.write("Title", defaultTitle);
  xml.write("ListName", listName);
  if (listOrigin.empty())
    listOrigin = gEvent->getName() + L" (" + getLocalDate() + L")";
  xml.write("ListOrigin", listOrigin);
  xml.write("Tag", tag);
  xml.write("UID", getUniqueId());
  xml.write("SortOrder", orderToSymbol[sortOrder]);
  xml.write("ListType", baseTypeToSymbol[listType]);
  xml.write("SubListType", baseTypeToSymbol[listSubType]);
  if (!resultModule.empty()) {
    string ttw = DynamicResult::undecorateTag(resultModule);
    xml.write("ResultModule", ttw);
    try {
      wstring srcFile;
      GeneralResult &gr = *oe->getGeneralResult(resultModule, srcFile);
      DynamicResult &dr = dynamic_cast<DynamicResult &>(gr);
      if (!dr.isBuiltIn()) {
        string ot = dr.getTag();
        dr.setTag(ttw);
        try {
          dr.save(xml);
        }
        catch (...) {
          dr.setTag(ot);
          throw;
        }
        dr.setTag(ot);
          
//        string db = "Saved res mod: " + ttw + " (" + resultModule +  ") h=" + itos(dr.getHashCode())+ "\n";
//        OutputDebugString(db.c_str());
      }
    }
    catch (std::exception &) { // Not available
    }
  }
  if (supportFromControl)
    xml.write("SupportFrom", supportFromControl);
  if (supportToControl)
    xml.write("SupportTo", supportToControl);
  if (hideLegSelection)
    xml.write("HideLegSelection", hideLegSelection);

  for (set<EFilterList>::const_iterator it = filter.begin(); it != filter.end(); ++it)
    xml.write("Filter", "name", filterToSymbol[*it]);

  for (set<ESubFilterList>::const_iterator it = subFilter.begin(); it != subFilter.end(); ++it)
    xml.write("SubFilter", "name", subFilterToSymbol[*it]);

  vector< pair<string, wstring> > props(2);
  xml.write("HeadFont", fontFaces[MLHead].serialize(props), fontFaces[MLHead].font);
  xml.write("SubHeadFont", fontFaces[MLSubHead].serialize(props), fontFaces[MLSubHead].font);
  xml.write("ListFont", fontFaces[MLList].serialize(props), fontFaces[MLList].font);
  xml.write("SubListFont", fontFaces[MLSubList].serialize(props), fontFaces[MLSubList].font);

  serialize(xml, "Head", getHead());
  serialize(xml, "SubHead", getSubHead());
  serialize(xml, "List", getList());
  serialize(xml, "SubList", getSubList());

  xml.endTag();
}

void MetaList::load(const wstring &file) {
  xmlparser xml;
  xml.read(file);
  filter.clear();
  xmlobject xDef = xml.getObject("MeOSListDefinition");
  load(xDef);
}

void MetaList::load(const xmlobject &xDef) {
  if (!xDef)
    throw meosException("Ogiltigt filformat");

  xDef.getObjectString("ListName", listName);
  xDef.getObjectString("ListOrigin", listOrigin);
  xDef.getObjectString("Tag", tag);
  xDef.getObjectString("UID", uniqueIndex);
  xDef.getObjectString("ResultModule", resultModule);
  
//  string db = "Specified res mod: " + resultModule + "\n";
//  OutputDebugString(db.c_str());

  vector<xmlobject> rs;
  xDef.getObjects("MeOSResultCalculationSet", rs);
  dynamicResults.resize(rs.size());
  for (size_t k = 0; k < rs.size(); k++) {
    DynamicResult dr;
    dr.load(rs[k]);
//    string db = "Loaded res mod: " + dr.getTag() + ", h=" + itos(dr.getHashCode())+ "\n";
//    OutputDebugString(db.c_str());
    wstring file = L"*";
    dynamicResults[k] = GeneralResultCtr(file, make_shared<DynamicResult>(dr));
  }
  supportFromControl = xDef.getObjectBool("SupportFrom");
  supportToControl = xDef.getObjectBool("SupportTo");
  hideLegSelection =xDef.getObjectBool("HideLegSelection");

  string tmp;
  xDef.getObjectString("SortOrder", tmp);
  if (symbolToOrder.count(tmp) == 0) {
    string err = "Invalid sort order X#" + tmp;
    throw std::exception(err.c_str());
  }
  sortOrder = symbolToOrder[tmp];

  xDef.getObjectString("ListType", tmp);
  if (symbolToBaseType.count(tmp) == 0) {
    string err = "Invalid type X#" + tmp;
    throw std::exception(err.c_str());
  }
  listType = symbolToBaseType[tmp];

  xDef.getObjectString("SubListType", tmp);
  if (!tmp.empty()) {
    if (symbolToBaseType.count(tmp) == 0) {
      string err = "Invalid type X#" + tmp;
      throw std::exception(err.c_str());
    }

    listSubType = symbolToBaseType[tmp];
  }
  xmlobject xHeadFont = xDef.getObject("HeadFont");
  xmlobject xSubHeadFont = xDef.getObject("SubHeadFont");
  xmlobject xListFont = xDef.getObject("ListFont");
  xmlobject xSubListFont = xDef.getObject("SubListFont");

  if (xHeadFont) {
    const wchar_t *f = xHeadFont.getw();
    fontFaces[MLHead].font = f != 0 ? f : L"arial";
    fontFaces[MLHead].scale = xHeadFont.getObjectInt("scale");
    fontFaces[MLHead].extraSpaceAbove = xHeadFont.getObjectInt("above");
  }

  if (xSubHeadFont) {
    const wchar_t *f = xSubHeadFont.getw();
    fontFaces[MLSubHead].font = f != 0 ? f : L"arial";
    fontFaces[MLSubHead].scale = xSubHeadFont.getObjectInt("scale");
    fontFaces[MLSubHead].extraSpaceAbove = xSubHeadFont.getObjectInt("above");
  }

  if (xListFont) {
    const wchar_t *f = xListFont.getw();
    fontFaces[MLList].font = f != 0 ? f : L"arial";
    fontFaces[MLList].scale = xListFont.getObjectInt("scale");
    fontFaces[MLList].extraSpaceAbove = xListFont.getObjectInt("above");
  }

  if (xSubListFont) {
    const wchar_t *f = xSubListFont.getw();
    fontFaces[MLSubList].font = f != 0 ? f : L"arial";
    fontFaces[MLSubList].scale = xSubListFont.getObjectInt("scale");
    fontFaces[MLSubList].extraSpaceAbove = xSubListFont.getObjectInt("above");
  }

  xmlobject xHead = xDef.getObject("Head");
  xmlobject xSubHead = xDef.getObject("SubHead");
  xmlobject xList = xDef.getObject("List");
  xmlobject xSubList = xDef.getObject("SubList");

  deserialize(xHead, data[MLHead]);
  deserialize(xSubHead, data[MLSubHead]);
  deserialize(xList, data[MLList]);
  deserialize(xSubList, data[MLSubList]);

  // Check if result list
  for (int i = 0; i<4; i++) {
    const vector< vector<MetaListPost> > &lines = data[i];
    for (size_t j = 0; j<lines.size(); j++) {
      const vector<MetaListPost> &cline = lines[j];
      for (size_t k = 0; k<cline.size(); k++) {
        const MetaListPost &mp = cline[k];

        if (mp.type == lTeamPlace || mp.type == lRunnerPlace || mp.type == lRunnerFinish 
          || mp.type == lRunnerTempTimeStatus || mp.type == lRunnerTimeAfter ||
          mp.type == lRunnerTime || mp.type == lRunnerTimeStatus || mp.type == lRunnerTimeAfter
          || mp.type == lRunnerTimePlaceFixed || mp.type == lRunnerStageTime || mp.type == lRunnerStagePlace
           || mp.type == lRunnerStageTimeStatus) {
          hasResults_ = true;
          break;
        }

        if (mp.type == lRunnerTotalPlace || mp.type == lRunnerTotalTimeStatus ||
          mp.type == lRunnerClassCourseTimeAfter || mp.type == lTeamTimeStatus || 
          mp.type == lTeamLegTimeStatus || mp.type == lTeamLegTimeAfter) {
          hasResults_ = true;
          break;
        }

        if (mp.type == lRunnerGeneralPlace || mp.type == lRunnerGeneralTimeStatus ||
          mp.type == lRunnerGeneralTimeAfter) {
          hasResults_ = true;
          break;
        }

        if (mp.type == lRunnerRogainingPoint || mp.type == lRunnerRogainingPointTotal ||
                mp.type == lTeamRogainingPoint || mp.type == lTeamRogainingPointTotal ||
                mp.type == lTeamTime || mp.type == lTeamStatus || mp.type == lTeamTotalTime ||
                mp.type == lTeamTotalTimeStatus || mp.type == lTeamTotalPlace) {
          hasResults_ = true;
          break;
        }
      }
    }
  }

  xmlList f;
  xDef.getObjects("Filter", f);

  for (size_t k = 0; k<f.size(); k++) {
    string attrib = f[k].getAttrib("name").get();
    if (symbolToFilter.count(attrib) == 0) {
      string err = "Invalid filter X#" + attrib;
      throw std::exception(err.c_str());
    }
    EFilterList f = symbolToFilter[attrib];
    if (f == EFilterHasResult || f == EFilterHasPrelResult)
      hasResults_ = true;
    
    addFilter(f);
  }

  xDef.getObjects("SubFilter", f);

  for (size_t k = 0; k<f.size(); k++) {
    string attrib = f[k].getAttrib("name").get();
    if (symbolToSubFilter.count(attrib) == 0) {
      string err = "Invalid filter X#" + attrib;
      throw std::exception(err.c_str());
    }
    addSubFilter(symbolToSubFilter[attrib]);
  }
}

void MetaList::getDynamicResults(vector<DynamicResultRef> &resultModules) const {
  resultModules.resize(dynamicResults.size());
  for (size_t k = 0; k < dynamicResults.size(); k++) {
    resultModules[k].res = dynamic_pointer_cast<DynamicResult>(dynamicResults[k].ptr);
    resultModules[k].ctr = const_cast<MetaList *>(this);
  }
}
extern gdioutput *gdi_main;

const wstring &MetaList::getListInfo(const oEvent &oe) const {
  vector<DynamicResultRef> resultModules;
  getDynamicResults(resultModules);

  for (size_t k = 0; k < resultModules.size(); k++) {
    if (resultModules[k].res) {
      return resultModules[k].res->getDescription();
    }
  }
  if (!resultModule.empty()) {
    wstring f;
    try {
      GeneralResult &res = *oe.getGeneralResult(resultModule, f);
      DynamicResult &dres = dynamic_cast<DynamicResult &>(res);
      return dres.getDescription();
    }
    catch (...) {

    }
  }
  return _EmptyWString;
}


void MetaList::retagResultModule(const string &newTag, bool retagStoredModule) {
  if (newTag == resultModule)
    return;
  string oldTag = resultModule;
  resultModule = newTag;
  for (size_t k = 0; k < data.size(); k++) {
    for (size_t i = 0; i < data[k].size(); i++) {
      for (size_t j = 0; j < data[k][i].size(); j++) {
        MetaListPost &mlp = data[k][i][j];
        if (mlp.getResultModule() == oldTag)
          mlp.setResultModule(newTag);
      }
    }
  }

  if (retagStoredModule) {
    for (size_t k = 0; k < dynamicResults.size(); k++) {
      DynamicResult *res = dynamic_cast<DynamicResult *>(dynamicResults[k].ptr.get());
      if (res && res->getTag() == oldTag) {
        res->setTag(newTag);
      }
    }
  }
}

bool MetaList::updateResultModule(const DynamicResult &dr, bool updateSimilar) {
  for (size_t k = 0; k < dynamicResults.size(); k++) {
    DynamicResult *res = dynamic_cast<DynamicResult *>(dynamicResults[k].ptr.get());
    if (res) {
      const string &tag1 = res->getTag();
      const string &tag2 = dr.getTag();
      if (tag1 == tag2 || (updateSimilar && DynamicResult::undecorateTag(tag1) == DynamicResult::undecorateTag(tag2)) ) {
        long long oldCS = res->getHashCode();
        *res = dr;
        retagResultModule(res->getTag(), false);
        return res->getHashCode() != oldCS;
      }
    }

    if (dr.getTag() == resultModule) {
      return true;
    }
  }
  return false;
}

bool MetaListContainer::updateResultModule(const DynamicResult &dr, bool updateSimilar) {
  bool changed = false;
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i].first == ExternalList) {
      if (data[i].second.updateResultModule(dr, updateSimilar))
        changed = true;
    }
  }

  auto f = freeResultModules.find(dr.getTag());
  if (f != freeResultModules.end()) {
    f->second.ptr = make_shared<DynamicResult>(dr);
    changed = true;
  }

  if (changed && owner)
    owner->updateChanged();

  return changed;
}

void MetaList::serialize(xmlparser &xml, const string &tagp,
                         const vector< vector<MetaListPost> > &lp) const {
  xml.startTag(tagp.c_str());

  for (size_t k = 0; k<lp.size(); k++) {
    xml.startTag("Line");
    for (size_t j = 0; j<lp[k].size(); j++) {
      lp[k][j].serialize(xml);
    }
    xml.endTag();
  }

  xml.endTag();
}

void MetaList::deserialize(const xmlobject &xml, vector< vector<MetaListPost> > &lp) {
  if (!xml)
    throw meosException("Ogiltigt filformat");


  xmlList xLines;
  xml.getObjects(xLines);

  for (size_t k = 0; k<xLines.size(); k++) {
    lp.push_back(vector<MetaListPost>());
    xmlList xBlocks;
    xLines[k].getObjects(xBlocks);
    for (size_t j = 0; j<xBlocks.size(); j++) {
      lp[k].push_back(MetaListPost(lNone));
      lp[k].back().deserialize(xBlocks[j]);
    }
  }
}

const wstring &MetaListPost::getType() const {
  return MetaList::typeToSymbol[type];
}

const string &MetaListPost::getTextAdjust() const {
  string &res = StringCache::getInstance().get();
  if (textAdjust == textRight)
    res = "Right";
  else if (textAdjust == textCenter)
    res = "Center";
  else
    res = "Left";
  return res;
}

void MetaListPost::setTextAdjust(int align) {
  if (align != 0 && align != textRight && align != textCenter)
    throw meosException("Invalid argument");
  textAdjust = align;
}

const string &MetaListPost::getColor() const {
  return itos(color);
}

void MetaListPost::setColor(GDICOLOR c) {
  color = c;
}

void MetaListPost::getTypes(vector< pair<wstring, size_t> > &types, int &currentType) const {
  currentType = type;
  types.clear();
  types.reserve(MetaList::typeToSymbol.size());
  for (map<EPostType, wstring>::const_iterator it =
    MetaList::typeToSymbol.begin(); it != MetaList::typeToSymbol.end(); ++it) {
      if (it->first == lNone)
        continue;
      if (it->first == lAlignNext)
        continue;

      types.push_back(make_pair(lang.tl(it->second), it->first));
  }
}

const string &MetaListPost::getFont() const {
  return MetaList::fontToSymbol[font];
}

void MetaListPost::getFonts(vector< pair<wstring, size_t> > &fonts, int &currentFont) const {
  currentFont = font;
  fonts.clear();
  fonts.reserve(MetaList::fontToSymbol.size());
  for (map<gdiFonts, string>::const_iterator it =
    MetaList::fontToSymbol.begin(); it != MetaList::fontToSymbol.end(); ++it) {
      fonts.push_back(make_pair(lang.tl(it->second), it->first));
  }
}

void MetaListPost::getAllFonts(vector< pair<wstring, size_t> > &fonts) {
 fonts.clear();
  fonts.reserve(MetaList::fontToSymbol.size());
  for (map<gdiFonts, string>::const_iterator it =
    MetaList::fontToSymbol.begin(); it != MetaList::fontToSymbol.end(); ++it) {
      fonts.push_back(make_pair(lang.tl(it->second), it->first));
  }
}

void MetaList::getAlignTypes(const MetaListPost &mlp, vector< pair<wstring, size_t> > &types, int &currentType) const {
  currentType = mlp.alignType;
  types.clear();
  int gix, lix, ix;
  getIndex(mlp, gix, lix, ix);
  set< pair<EPostType, wstring> > atypes;
  bool q = false;
  for (size_t k = 0; k < data.size(); k++) {
    for (size_t j = 0; j < data[k].size(); j++) {
      if ( k == gix && j == lix) {
        q = true;
        break;
      }
      for (size_t i = 0; i < data[k][j].size(); i++) {
        if (data[k][j][i].type != lString)
          atypes.insert(make_pair(data[k][j][i].type, L""));
        else
          atypes.insert(make_pair(data[k][j][i].type, data[k][j][i].text));
      }
    }

    if (q)
      break;
  }
  if (currentType != lString)
    atypes.insert(make_pair(EPostType(currentType), L""));
  atypes.insert(make_pair(lNone, L""));

  for (set< pair<EPostType, wstring> >::iterator it = atypes.begin(); it != atypes.end(); ++it) {
    wstring type = lang.tl(typeToSymbol[it->first]);
    if (it->first == lString)
      type += L":" + it->second;
    types.push_back(make_pair(type, it->first));
  }
}

void MetaList::getIndex(const MetaListPost &mlp, int &gix, int &lix, int &ix) const {
  for (size_t k = 0; k < data.size(); k++) {
    for (size_t j = 0; j < data[k].size(); j++) {
      for (size_t i = 0; i < data[k][j].size(); i++) {
        if (&data[k][j][i] == &mlp) {
          gix = k, lix = j, ix = i;
          return;
        }
      }
    }
  }
  throw meosException("Invalid object");
}
void MetaListPost::serialize(xmlparser &xml) const {
  xml.startTag("Block", "Type", MetaList::typeToSymbol[type]);
  xml.write("Text", text);
  if (!resultModule.empty())
    xml.write("ResultModule", DynamicResult::undecorateTag(resultModule));
  if (leg != -1)
    xml.write("Leg", itos(leg));
  if (alignType == lString)
    xml.writeBool("Align", "BlockAlign", alignBlock, alignWithText);
  else
    xml.writeBool("Align", "BlockAlign", alignBlock, MetaList::typeToSymbol[alignType]);
  xml.write("BlockWidth", blockWidth);
  xml.write("IndentMin", minimalIndent);
  xml.write("PackPrevious", packPrevious);
  xml.write("LimitWidth", limitWidth);


  if (font != formatIgnore)
    xml.write("Font", getFont());
  if (mergeWithPrevious)
    xml.write("MergePrevious", "1");
  if (textAdjust != 0)
    xml.write("TextAdjust", getTextAdjust());
  if (color != colorDefault) {
    char bf[16];
    sprintf_s(bf, "%x", color);
    xml.write("Color", bf);
  }
  xml.endTag();
}

void MetaListPost::deserialize(const xmlobject &xml) {
  if (!xml)
    throw meosException("Ogiltigt filformat");

  wstring tp = xml.getAttrib("Type").wget();
  if (MetaList::symbolToType.count(tp) == 0) {
    wstring err = L"Invalid type X#" + tp;
    throw meosException(err);
  }

  type = MetaList::symbolToType[tp];
  xml.getObjectString("Text", text);
  xml.getObjectString("ResultModule", resultModule);
  if (xml.getObject("Leg"))
    leg = xml.getObjectInt("Leg");
  else
    leg = -1;
  xmlobject xAlignBlock = xml.getObject("Align");
  alignBlock = xAlignBlock && xAlignBlock.getObjectBool("BlockAlign");
  blockWidth = xml.getObjectInt("BlockWidth");
  minimalIndent = xml.getObjectInt("IndentMin");
  wstring at;
  xml.getObjectString("Align", at);

  if (!at.empty()) {
    if (MetaList::symbolToType.count(at) == 0) {
      alignType = lString;
      alignWithText = at;
    }
    else alignType = MetaList::symbolToType[at];
  }

  packPrevious = xml.getObjectBool("PackPrevious");
  limitWidth = xml.getObjectBool("LimitWidth");

  mergeWithPrevious = xml.getObjectInt("MergePrevious") != 0;
  xml.getObjectString("TextAdjust", at);

  if (at == L"Right")
    textAdjust = textRight;
  else if (at == L"Center")
    textAdjust = textCenter;

  xml.getObjectString("Color", at);
  if (!at.empty()) {
    color = (GDICOLOR)(wcstol(at.c_str(), 0, 16)&0xFFFFFF);
    //color = GDICOLOR(atoi(at.c_str()));
  }

  string f;
  xml.getObjectString("Font", f);
  if (!f.empty()) {
    if (MetaList::symbolToFont.count(f) == 0) {
      string err = "Invalid font X#" + f;
      throw meosException(err);
    }
    else font = MetaList::symbolToFont[f];
  }
}

map<EPostType, wstring> MetaList::typeToSymbol;
map<wstring, EPostType> MetaList::symbolToType;
map<oListInfo::EBaseType, string> MetaList::baseTypeToSymbol;
map<string, oListInfo::EBaseType> MetaList::symbolToBaseType;
map<SortOrder, string> MetaList::orderToSymbol;
map<string, SortOrder> MetaList::symbolToOrder;
map<EFilterList, string> MetaList::filterToSymbol;
map<string, EFilterList> MetaList::symbolToFilter;
map<gdiFonts, string> MetaList::fontToSymbol;
map<string, gdiFonts> MetaList::symbolToFont;
map<ESubFilterList, string> MetaList::subFilterToSymbol;
map<string, ESubFilterList> MetaList::symbolToSubFilter;

void MetaList::initSymbols() {
  if (typeToSymbol.empty()) {
    typeToSymbol[lAlignNext] = L"AlignNext";
    typeToSymbol[lNone] = L"None";
    typeToSymbol[lString] = L"String";
    typeToSymbol[lResultDescription] = L"ResultDescription";
    typeToSymbol[lTimingFromName] = L"TimingFrom";
    typeToSymbol[lTimingToName] = L"TimingTo";
    typeToSymbol[lCmpName] = L"CmpName";
    typeToSymbol[lCmpDate] = L"CmpDate";
    typeToSymbol[lCurrentTime] = L"CurrentTime";
    typeToSymbol[lClubName] = L"ClubName";
    typeToSymbol[lClassName] = L"ClassName";
    typeToSymbol[lClassStartName] = L"ClassStartName";
    typeToSymbol[lClassStartTime] = L"StartTimeForClass";
    typeToSymbol[lClassStartTimeRange] = L"StartTimeForClassRange";
    typeToSymbol[lClassLength] = L"ClassLength";
    typeToSymbol[lClassResultFraction] = L"ClassResultFraction";
    typeToSymbol[lClassAvailableMaps] = L"ClassAvailableMaps";
    typeToSymbol[lClassTotalMaps] = L"ClassTotalMaps";
    typeToSymbol[lClassNumEntries] = L"ClassNumEntries";
    typeToSymbol[lCourseLength] = L"CourseLength";
    typeToSymbol[lCourseName] = L"CourseName";
    typeToSymbol[lCourseClimb] = L"CourseClimb";
    typeToSymbol[lCourseUsage] = L"CourseUsage";
    typeToSymbol[lCourseUsageNoVacant] = L"CourseUsageNoVacant";
    typeToSymbol[lCourseClasses] = L"CourseClasses";
    typeToSymbol[lCourseShortening] = L"CourseShortening";
    typeToSymbol[lRunnerName] = L"RunnerName";
    typeToSymbol[lRunnerGivenName] = L"RunnerGivenName";
    typeToSymbol[lRunnerFamilyName] = L"RunnerFamilyName";
    typeToSymbol[lRunnerCompleteName] = L"RunnerCompleteName";
    typeToSymbol[lPatrolNameNames] = L"PatrolNameNames";
    typeToSymbol[lPatrolClubNameNames] = L"PatrolClubNameNames";
    typeToSymbol[lRunnerFinish] = L"RunnerFinish";
    typeToSymbol[lRunnerTime] = L"RunnerTime";
    typeToSymbol[lRunnerGrossTime] = L"RunnerGrossTime";
    typeToSymbol[lRunnerTimeStatus] = L"RunnerTimeStatus";
    typeToSymbol[lRunnerTempTimeStatus] = L"RunnerTempTimeStatus";
    typeToSymbol[lRunnerTempTimeAfter] = L"RunnerTempTimeAfter";
    typeToSymbol[lRunnerTimeAfter] = L"RunnerTimeAfter";
    typeToSymbol[lRunnerClassCourseTimeAfter] = L"RunnerClassCourseTimeAfter";
    typeToSymbol[lRunnerLostTime] = L"RunnerTimeLost";
    typeToSymbol[lRunnerPlace] = L"RunnerPlace";
    typeToSymbol[lRunnerClassCoursePlace] = L"RunnerClassCoursePlace";
    typeToSymbol[lRunnerCoursePlace] = L"RunnerCoursePlace";
    typeToSymbol[lRunnerStart] = L"RunnerStart";
    typeToSymbol[lRunnerCheck] = L"RunnerCheck";
    typeToSymbol[lRunnerStartCond] = L"RunnerStartCond";
    typeToSymbol[lRunnerStartZero] = L"RunnerStartZero";
    typeToSymbol[lRunnerClub] = L"RunnerClub";
    typeToSymbol[lRunnerCard] = L"RunnerCard";
    typeToSymbol[lRunnerBib] = L"RunnerBib";
    typeToSymbol[lRunnerStartNo] = L"RunnerStartNo";
    typeToSymbol[lRunnerRank] = L"RunnerRank";
    typeToSymbol[lRunnerCourse] = L"RunnerCourse";
    typeToSymbol[lRunnerRogainingPoint] = L"RunnerRogainingPoint";
    typeToSymbol[lRunnerRogainingPointTotal] = L"RunnerRogainingPointTotal";
    typeToSymbol[lRunnerRogainingPointReduction] = L"RunnerRogainingReduction";
    typeToSymbol[lRunnerRogainingPointOvertime] = L"RunnerRogainingOvertime";
    typeToSymbol[lRunnerTimeAdjustment] = L"RunnerTimeAdjustment";
    typeToSymbol[lRunnerPointAdjustment] = L"RunnerPointAdjustment";
    typeToSymbol[lRunnerRogainingPointGross] = L"RunnerRogainingPointGross";
    typeToSymbol[lRunnerCardVoltage] = L"RunnerCardVoltage";

    typeToSymbol[lRunnerStageTime] = L"RunnerStageTime";
    typeToSymbol[lRunnerStageStatus] = L"RunnerStageStatus";
    typeToSymbol[lRunnerStageTimeStatus] = L"RunnerStageTimeStatus";
    typeToSymbol[lRunnerStagePlace] = L"RunnerStagePlace";
    typeToSymbol[lRunnerStagePoints] = L"RunnerStagePoints";
    typeToSymbol[lRunnerStageNumber] = L"RunnerStageNumber";

    typeToSymbol[lRunnerUMMasterPoint] = L"RunnerUMMasterPoint";
    typeToSymbol[lRunnerTimePlaceFixed] = L"RunnerTimePlaceFixed";
    typeToSymbol[lRunnerLegNumberAlpha] = L"RunnerLegNumberAlpha";
    typeToSymbol[lRunnerLegNumber] = L"RunnerLegNumber";

    typeToSymbol[lResultModuleTime] = L"ResultModuleTime";
    typeToSymbol[lResultModuleNumber] = L"ResultModuleNumber";
    typeToSymbol[lResultModuleTimeTeam] = L"ResultModuleTimeTeam";
    typeToSymbol[lResultModuleNumberTeam] = L"ResultModuleNumberTeam";

    typeToSymbol[lRunnerBirthYear] = L"RunnerBirthYear";
    typeToSymbol[lRunnerAge] = L"RunnerAge";
    typeToSymbol[lRunnerSex] = L"RunnerSex";
    typeToSymbol[lRunnerNationality] = L"RunnerNationality";
    typeToSymbol[lRunnerPhone] = L"RunnerPhone";
    typeToSymbol[lRunnerFee] = L"RunnerFee";
    typeToSymbol[lRunnerExpectedFee] = L"RunnerExpectedFee";
    typeToSymbol[lRunnerPaid] = L"RunnerPaid";
    typeToSymbol[lRunnerPayMethod] = L"RunnerPayMethod";
    typeToSymbol[lRunnerEntryDate] = L"RunnerEntryDate";
    typeToSymbol[lRunnerEntryTime] = L"RunnerEntryTime";
    typeToSymbol[lRunnerId] = L"RunnerId";

    typeToSymbol[lTeamName] = L"TeamName";
    typeToSymbol[lTeamStart] = L"TeamStart";
    typeToSymbol[lTeamStartCond] = L"TeamStartCond";
    typeToSymbol[lTeamStartZero] = L"TeamStartZero";

    typeToSymbol[lTeamTimeStatus] = L"TeamTimeStatus";
    typeToSymbol[lTeamTimeAfter] = L"TeamTimeAfter";
    typeToSymbol[lTeamPlace] = L"TeamPlace";
    typeToSymbol[lTeamLegTimeStatus] = L"TeamLegTimeStatus";
    typeToSymbol[lTeamLegTimeAfter] = L"TeamLegTimeAfter";
    typeToSymbol[lTeamRogainingPoint] = L"TeamRogainingPoint";
    typeToSymbol[lTeamRogainingPointTotal] = L"TeamRogainingPointTotal";
    typeToSymbol[lTeamRogainingPointReduction] = L"TeamRogainingReduction";
    typeToSymbol[lTeamRogainingPointOvertime] = L"TeamRogainingOvertime";
    typeToSymbol[lTeamTimeAdjustment] = L"TeamTimeAdjustment";
    typeToSymbol[lTeamPointAdjustment] = L"TeamPointAdjustment";
  
    typeToSymbol[lTeamTime] = L"TeamTime";
    typeToSymbol[lTeamGrossTime] = L"TeamGrossTime";
    typeToSymbol[lTeamStatus] = L"TeamStatus";
    typeToSymbol[lTeamClub] = L"TeamClub";
    typeToSymbol[lTeamRunner] = L"TeamRunner";
    typeToSymbol[lTeamRunnerCard] = L"TeamRunnerCard";
    typeToSymbol[lTeamBib] = L"TeamBib";
    typeToSymbol[lTeamStartNo] = L"TeamStartNo";
    
    typeToSymbol[lPunchNamedTime] = L"PunchNamedTime";
    typeToSymbol[lPunchName] = L"PunchName";
    typeToSymbol[lPunchNamedSplit] = L"PunchNamedSplit";

    typeToSymbol[lPunchTime] = L"PunchTime";
    typeToSymbol[lPunchControlNumber] = L"PunchControlNumber";
    typeToSymbol[lPunchControlCode] = L"PunchControlCode";
    typeToSymbol[lPunchLostTime] = L"PunchLostTime";
    typeToSymbol[lPunchControlPlace] = L"PunchControlPlace";
    typeToSymbol[lPunchControlPlaceAcc] = L"PunchControlPlaceAcc";

    typeToSymbol[lPunchSplitTime] = L"PunchSplitTime";
    typeToSymbol[lPunchTotalTime] = L"PunchTotalTime";
    typeToSymbol[lPunchAbsTime] = L"PunchAbsTime";
    typeToSymbol[lPunchTotalTimeAfter] = L"PunchTotalTimeAfter";
    typeToSymbol[lPunchTimeSinceLast] = L"PunchTimeSinceLast";

    typeToSymbol[lRogainingPunch] = L"RogainingPunch";
    typeToSymbol[lTotalCounter] = L"TotalCounter";
    typeToSymbol[lSubCounter] = L"SubCounter";
    typeToSymbol[lSubSubCounter] = L"SubSubCounter";
    typeToSymbol[lTeamFee] = L"TeamFee";

    typeToSymbol[lRunnerTotalTime] = L"RunnerTotalTime";
    typeToSymbol[lRunnerTimePerKM] = L"RunnerTimePerKM";
    typeToSymbol[lRunnerTotalTimeStatus] = L"RunnerTotalTimeStatus";
    typeToSymbol[lRunnerTotalPlace] = L"RunnerTotalPlace";
    typeToSymbol[lRunnerTotalTimeAfter] = L"RunnerTotalTimeAfter";
    typeToSymbol[lRunnerTimeAfterDiff] = L"RunnerTimeAfterDiff";
    typeToSymbol[lRunnerPlaceDiff] = L"RunnerPlaceDiff";

    typeToSymbol[lRunnerGeneralTimeStatus] = L"RunnerGeneralTimeStatus";
    typeToSymbol[lRunnerGeneralPlace] = L"RunnerGeneralPlace";
    typeToSymbol[lRunnerGeneralTimeAfter] = L"RunnerGeneralTimeAfter";

    typeToSymbol[lTeamTotalTime] = L"TeamTotalTime";
    typeToSymbol[lTeamTotalTimeStatus] = L"TeamTotalTimeStatus";
    typeToSymbol[lTeamTotalPlace] = L"TeamTotalPlace";
    typeToSymbol[lTeamTotalTimeAfter] = L"TeamTotalTimeAfter";
    typeToSymbol[lTeamTotalTimeDiff] = L"TeamTotalTimeDiff";
    typeToSymbol[lTeamPlaceDiff] = L"TeamPlaceDiff";

    typeToSymbol[lCountry] = L"Country";
    typeToSymbol[lNationality] = L"Nationality";

    typeToSymbol[lControlName] = L"ControlName";
    typeToSymbol[lControlCourses] = L"ControlCourses";
    typeToSymbol[lControlClasses] = L"ControlClasses";
    typeToSymbol[lControlVisitors] = L"ControlVisitors";
    typeToSymbol[lControlPunches] = L"ControlPunches";
    typeToSymbol[lControlMedianLostTime] = L"ControlMedianLostTime";
    typeToSymbol[lControlMaxLostTime] = L"ControlMaxLostTime";
    typeToSymbol[lControlMistakeQuotient] = L"ControlMistakeQuotient";
    typeToSymbol[lControlRunnersLeft] = L"ControlRunnersLeft";
    typeToSymbol[lControlCodes] = L"ControlCodes";
    
    typeToSymbol[lLineBreak] = L"LineBreak";

    for (map<EPostType, wstring>::iterator it = typeToSymbol.begin();
      it != typeToSymbol.end(); ++it) {
      symbolToType[it->second] = it->first;
    }

    if (typeToSymbol.size() != lLastItem)
      throw std::exception("Bad symbol setup");

    if (symbolToType.size() != lLastItem)
      throw std::exception("Bad symbol setup");

    baseTypeToSymbol[oListInfo::EBaseTypeRunner] = "Runner";
    baseTypeToSymbol[oListInfo::EBaseTypeTeam] = "Team";
    baseTypeToSymbol[oListInfo::EBaseTypeClub] = "ClubRunner";
    baseTypeToSymbol[oListInfo::EBaseTypeCoursePunches] = "CoursePunches";
    baseTypeToSymbol[oListInfo::EBaseTypeAllPunches] = "AllPunches";
    baseTypeToSymbol[oListInfo::EBaseTypeNone] = "None";
    baseTypeToSymbol[oListInfo::EBaseTypeRunnerGlobal] = "RunnerGlobal";
    baseTypeToSymbol[oListInfo::EBaseTypeRunnerLeg] = "RunnerLeg";
    baseTypeToSymbol[oListInfo::EBaseTypeTeamGlobal] = "TeamGlobal";
    baseTypeToSymbol[oListInfo::EBaseTypeControl] = "Control";
    baseTypeToSymbol[oListInfo::EBaseTypeCourse] = "Course";

    for (map<oListInfo::EBaseType, string>::iterator it = baseTypeToSymbol.begin();
      it != baseTypeToSymbol.end(); ++it) {
      symbolToBaseType[it->second] = it->first;
    }

    if (baseTypeToSymbol.size() != oListInfo::EBasedTypeLast_)
      throw std::exception("Bad symbol setup");

    if (symbolToBaseType.size() != oListInfo::EBasedTypeLast_)
      throw std::exception("Bad symbol setup");
    symbolToBaseType["Punches"] = oListInfo::EBaseTypeCoursePunches; // Back comp MeOS --3.5

    orderToSymbol[ClassStartTime] = "ClassStartTime";
    orderToSymbol[ClassStartTimeClub] = "ClassStartTimeClub";
    orderToSymbol[ClassResult] = "ClassResult";
    orderToSymbol[ClassDefaultResult] = "ClassDefaultResult";
    orderToSymbol[ClassCourseResult] = "ClassCourseResult";
    orderToSymbol[SortByName] = "SortNameOnly";
    orderToSymbol[SortByLastName] = "SortLastNameOnly";
    orderToSymbol[SortByFinishTime] = "FinishTime";
    orderToSymbol[SortByFinishTimeReverse] = "FinishTimeReverse";
    orderToSymbol[ClassFinishTime] = "ClassFinishTime";
    orderToSymbol[SortByStartTime] = "StartTime";
    orderToSymbol[SortByStartTimeClass] = "StartTimeClass";
    orderToSymbol[SortByEntryTime] = "EntryTime";
    orderToSymbol[ClassPoints] = "ClassPoints";
    orderToSymbol[ClassTotalResult] = "ClassTotalResult";
    orderToSymbol[ClassTeamLegResult] = "ClassTeamLegResult";
    orderToSymbol[CourseResult] = "CourseResult";
    orderToSymbol[CourseStartTime] = "CourseStartTime";
    orderToSymbol[ClassTeamLeg] = "ClassTeamLeg";
    orderToSymbol[ClassLiveResult] = "ClassLiveResult";
  	orderToSymbol[ClassKnockoutTotalResult] = "ClassKnockoutTotalResult";
    orderToSymbol[Custom] = "CustomSort";

    for (map<SortOrder, string>::iterator it = orderToSymbol.begin();
      it != orderToSymbol.end(); ++it) {
      symbolToOrder[it->second] = it->first;
    }

    if (orderToSymbol.size() != SortEnumLastItem)
      throw std::exception("Bad symbol setup");

    if (symbolToOrder.size() != SortEnumLastItem)
      throw std::exception("Bad symbol setup");

    filterToSymbol[EFilterHasResult] = "FilterResult";
    filterToSymbol[EFilterHasPrelResult] = "FilterPrelResult";
    filterToSymbol[EFilterRentCard] = "FilterRentCard";
    filterToSymbol[EFilterHasCard] = "FilterHasCard";
    filterToSymbol[EFilterExcludeDNS] = "FilterStarted";
    filterToSymbol[EFilterExcludeCANCEL] = "FilterNoCancel";
    filterToSymbol[EFilterVacant] = "FilterNotVacant";
    filterToSymbol[EFilterOnlyVacant] = "FilterOnlyVacant";
    filterToSymbol[EFilterHasNoCard] = "FilterNoCard";
    filterToSymbol[EFilterAnyResult] = "FilterAnyResult";
    filterToSymbol[EFilterAPIEntry] = "EFilterAPIEntry";
    filterToSymbol[EFilterWrongFee] = "EFilterWrongFee";
    filterToSymbol[EFilterIncludeNotParticipating] = "EFilterIncludeNotParticipating";

    for (map<EFilterList, string>::iterator it = filterToSymbol.begin();
      it != filterToSymbol.end(); ++it) {
      symbolToFilter[it->second] = it->first;
    }

    if (filterToSymbol.size() != _EFilterMax)
      throw std::exception("Bad symbol setup");

    if (symbolToFilter.size() != _EFilterMax)
      throw std::exception("Bad symbol setup");

    subFilterToSymbol[ESubFilterHasResult] = "FilterResult";
    subFilterToSymbol[ESubFilterHasPrelResult] = "FilterPrelResult";
    subFilterToSymbol[ESubFilterExcludeDNS] = "FilterStarted";
    subFilterToSymbol[ESubFilterVacant] = "FilterNotVacant";
    subFilterToSymbol[ESubFilterSameParallel] = "FilterSameParallel";
    subFilterToSymbol[ESubFilterSameParallelNotFirst] = "FilterSameParallelNotFirst";
    subFilterToSymbol[ESubFilterNotFinish] = "FilterNotFinish";
    subFilterToSymbol[ESubFilterNamedControl] = "FilterNamedControl";
    
    for (map<ESubFilterList, string>::iterator it = subFilterToSymbol.begin();
      it != subFilterToSymbol.end(); ++it) {
      symbolToSubFilter[it->second] = it->first;
    }

    if (subFilterToSymbol.size() != _ESubFilterMax)
      throw std::exception("Bad symbol setup");

    if (symbolToSubFilter.size() != _ESubFilterMax)
      throw std::exception("Bad symbol setup");


    fontToSymbol[normalText] = "NormalFont";
    fontToSymbol[boldText] = "Bold";
    fontToSymbol[boldLarge] = "BoldLarge";
    fontToSymbol[boldHuge] = "BoldHuge";
    fontToSymbol[boldSmall] = "BoldSmall";
    fontToSymbol[fontLarge] = "LargeFont";
    fontToSymbol[fontMedium] = "MediumFont";
    fontToSymbol[fontMediumPlus] = "MediumPlus";
    fontToSymbol[fontSmall] = "SmallFont";
    fontToSymbol[italicSmall] = "SmallItalic";
    fontToSymbol[italicText] = "Italic";
    fontToSymbol[italicMediumPlus] = "ItalicMediumPlus";

    fontToSymbol[formatIgnore] = "DefaultFont";

    for (map<gdiFonts, string>::iterator it = fontToSymbol.begin();
      it != fontToSymbol.end(); ++it) {
      symbolToFont[it->second] = it->first;
    }

    if (fontToSymbol.size() != symbolToFont.size())
      throw std::exception("Bad symbol setup");

  }
}

MetaListContainer::MetaListContainer(oEvent *owner): owner(owner) {}


MetaListContainer::MetaListContainer(oEvent *owner, const MetaListContainer &src) {
  *this = src;
  this->owner = owner;
}


MetaListContainer::~MetaListContainer() {}


const MetaList &MetaListContainer::getList(int index) const {
  return data[index].second;
}

MetaList &MetaListContainer::getList(int index) {
  return data[index].second;
}

MetaList &MetaListContainer::addExternal(const MetaList &ml) {
  data.push_back(make_pair(ExternalList, ml));
  if (owner)
    owner->updateChanged();
  return data.back().second;
}

void MetaListContainer::clearExternal() {
  globalIndex.clear();
  uniqueIndex.clear();
  while(!data.empty() && (data.back().first == ExternalList || data.back().first == RemovedList) )
    data.pop_back();

  listParam.clear();
}

void MetaListContainer::save(MetaListType type, xmlparser &xml, const oEvent *oe) const {

  for (size_t k = 0; k<data.size(); k++) {
    if (data[k].first == type)
      data[k].second.save(xml, oe);
  }
  if (type == ExternalList) {
    setupIndex(EFirstLoadedList);
    
    // Setup map with new param numbering
    map<int, int> id2Ix;
    int count = 0;
    for (map<int, oListParam>::const_iterator it = listParam.begin(); it != listParam.end(); ++it) {
      id2Ix[it->first+1] = ++count; // One indexed
    }

    for (map<int, oListParam>::const_iterator it = listParam.begin(); it != listParam.end(); ++it) {
      it->second.serialize(xml, *this, id2Ix);
    }
  }

  for (auto res : freeResultModules) {
    DynamicResult *dr = dynamic_cast<DynamicResult *>(res.second.ptr.get());
    if (dr) {
      dr->save(xml);
    }
  }
}

bool MetaListContainer::load(MetaListType type, const xmlobject &xDef, bool ignoreOld) {
  if (!xDef)
    return true;
  xmlList xList;
  xDef.getObjects("MeOSListDefinition", xList);
  wstring majVer = getMajorVersion();
  bool hasSkipped = false;
  bool pushLevel = false;
  if (xList.empty() && strcmp(xDef.getName(), "MeOSListDefinition") == 0) {
    xList.push_back(xDef);
    pushLevel = true;
  }
  wstring err;
  for (size_t k = 0; k<xList.size(); k++) {
    xmlattrib ver = xList[k].getAttrib("version");
    bool newVersion = false;
    if (ver) {
      wstring vers = ver.wget();
      if (vers > majVer) {
        newVersion = true;
      }
    }

    data.push_back(make_pair(type, MetaList()));
    try {
      data.back().second.load(xList[k]);
    }
    catch (const meosException &ex) {
      if (newVersion && ignoreOld)
        hasSkipped = true;
      else if (err.empty())
        err = ex.wwhat();

      data.pop_back();
    }
    catch (const std::exception &ex) {
      if (newVersion && ignoreOld)
        hasSkipped = true;
      else if (err.empty()) {
        string nw = ex.what();
        err.insert(err.begin(), nw.begin(), nw.end());
      }
      data.pop_back();
    }
  }

  setupIndex(EFirstLoadedList);

  xmlList xParam;
  xDef.getObjects("ListParam", xParam);

  for (size_t k = 0; k<xParam.size(); k++) {
    try {
      listParam[k].deserialize(xParam[k], *this);
    }
    catch (const meosException &ex) {
      if (err.empty())
        err = ex.wwhat();
      listParam.erase(k);
    }
    catch (const std::exception &ex) {
      if (err.empty()) {
        string ne = ex.what();
        err.insert(err.begin(), ne.begin(), ne.end());
      }
      listParam.erase(k);
    }
  }

  for (map<int, oListParam>::iterator it = listParam.begin(); it != listParam.end(); ++it) {
    int next = it->second.nextList;
    if (next>0) {
      int ix = next - 1;
      if (listParam.count(ix)) {
        if (listParam[ix].previousList == 0)
          listParam[ix].previousList = it->first + 1;
        else
          it->second.nextList = 0; // Clear relation
      }
      else
        it->second.nextList = 0; // Clear relation
    }
  }
  if (!pushLevel) {
    xDef.getObjects("MeOSResultCalculationSet", xList);
    decltype(freeResultModules) copy;
    freeResultModules.swap(copy);
    for (auto res : xList) {
      GeneralResultCtr ctr;
      auto dr = make_shared<DynamicResult>();
      dr->load(res);
      if (copy.count(dr->getTag())) {
        DynamicResult &oldResult = dynamic_cast<DynamicResult&>(*copy[dr->getTag()].ptr);
        oldResult = *dr;
        dr = dynamic_pointer_cast<DynamicResult>(copy[dr->getTag()].ptr);
      }

      freeResultModules.emplace(dr->getTag(), GeneralResultCtr(dr->getTag().c_str(), dr->getName(false), dr));
    }

    if (owner)
      owner->updateChanged();
  }

  if (!err.empty())
    throw meosException(err);

  return !hasSkipped;
}

bool MetaList::isValidIx(size_t gIx, size_t lIx, size_t ix) const {
  return gIx < data.size() && lIx < data[gIx].size() && (ix == -1 || ix < data[gIx][lIx].size());
}

MetaListPost &MetaList::addNew(int groupIx, int lineIx, int &ix) {
  if (isValidIx(groupIx, lineIx, -1)) {
    ix = data[groupIx][lineIx].size();
    data[groupIx][lineIx].push_back(MetaListPost(lString));
    data[groupIx][lineIx].back().setResultModule(resultModule);
    return data[groupIx][lineIx].back();
  }
  else if (lineIx == -1 && size_t(groupIx) < data.size()) {
    lineIx = data[groupIx].size();
    addRow(groupIx);
    return addNew(groupIx, lineIx, ix);
  }
  throw meosException("Invalid index");
}

MetaListPost &MetaList::getMLP(int groupIx, int lineIx, int ix) {
  if (isValidIx(groupIx, lineIx, ix)) {
    return data[groupIx][lineIx][ix];
  }
  else
    throw meosException("Invalid index");
}

void MetaList::removeMLP(int groupIx, int lineIx, int ix) {
  if (isValidIx(groupIx, lineIx, ix)) {
    data[groupIx][lineIx].erase(data[groupIx][lineIx].begin() + ix);
    if (data[groupIx][lineIx].empty() && data[groupIx].size() == lineIx + 1)
      data[groupIx].pop_back();
  }
  else
    throw meosException("Invalid index");
}

void MetaList::moveOnRow(int groupIx, int lineIx, int &ix, int delta) {
  if (isValidIx(groupIx, lineIx, ix)) {
    if (ix > 0 && delta == -1) {
      ix--;
      swap(data[groupIx][lineIx][ix], data[groupIx][lineIx][ix+1]);
    }
    else if (delta == 1 && size_t(ix + 1) < data[groupIx][lineIx].size()) {
      ix++;
      swap(data[groupIx][lineIx][ix], data[groupIx][lineIx][ix-1]);
    }
  }
  else
    throw meosException("Invalid index");

}

string MetaListContainer::getUniqueId(EStdListType code) const {
  if (int(code) < int(EFirstLoadedList))
    return "C" + itos(code);
  else {
    size_t ix = int(code) - int(EFirstLoadedList);
    if (ix < data.size()) {
      if (!data[ix].second.getTag().empty())
        return "T" + data[ix].second.getTag();
      else
        return data[ix].second.getUniqueId();
    }
    else
      return "C-1";
  }
}

EStdListType MetaListContainer::getCodeFromUnqiueId(const string &id) const {
  if (id[0] == 'C')
    return EStdListType(atoi(id.substr(1).c_str()));
  else if (id[0] == 'T') {
    string tag = id.substr(1);
    map<string, EStdListType>::const_iterator res = tagIndex.find(tag);
    if (res != tagIndex.end())
      return res->second;
    else
      return EStdNone;
  }
  else {
    map<string, EStdListType>::const_iterator res = uniqueIndex.find(id);
    if (res != uniqueIndex.end())
      return res->second;
    else
      return EStdNone;
  }
}

void MetaListContainer::setupIndex(int firstIndex) const {
  globalIndex.clear();
  uniqueIndex.clear();
  for (size_t k = 0; k<data.size(); k++) {
    if (data[k].first == RemovedList)
      continue;
    const MetaList &ml = data[k].second;
    EStdListType listIx = EStdListType(k + firstIndex);
    if (data[k].first == InternalList) {
      const string &tag = data[k].second.getTag();
      if (!tag.empty())
        tagIndex[tag] = listIx;
    }

    globalIndex[listIx] = k;

    if (!ml.getUniqueId().empty())
      uniqueIndex[ml.getUniqueId()] = listIx;
  }
}

void MetaListContainer::setupListInfo(int firstIndex,
                                      map<EStdListType, oListInfo> &listMap,
                                      bool resultsOnly) const {
  setupIndex(firstIndex);

  for (size_t k = 0; k<data.size(); k++) {
    if (data[k].first == RemovedList)
      continue;
    const MetaList &ml = data[k].second;
    EStdListType listIx = EStdListType(k + firstIndex);

    if (!resultsOnly || ml.hasResults()) {
      oListInfo &li = listMap[listIx];
      li.Name = lang.tl(ml.getListName());
      li.listType = ml.getListType();
      li.supportClasses = ml.supportClasses();
      li.supportLegs = (ml.getListType() == oListInfo::EBaseTypeTeam) && ml.supportLegSelection();
      li.supportParameter = !ml.getResultModule().empty();
      li.supportLarge = true;
      li.supportFrom = ml.supportFrom();
      li.supportTo = ml.supportTo();
      li.resType = ml.getResultType();
    }
  }
}

wstring MetaListContainer::makeUniqueParamName(const wstring &nameIn) const {
  int maxValue = -1;
  size_t len = nameIn.length();
  for (map<int, oListParam>::const_iterator it = listParam.begin(); it != listParam.end(); ++it) {
    if (it->second.name.length() >= len) {
      if (nameIn == it->second.name)
        maxValue = max(1, maxValue);
      else {
        if (it->second.name.substr(0, len) == nameIn) {
          int v = _wtoi(it->second.name.substr(len + 1).c_str());
          if (v > 0)
            maxValue = max(v, maxValue);
        }
      }
    }
  }
  if (maxValue == -1)
    return nameIn;
  else
    return nameIn + L" " + itow(maxValue + 1);
}


bool MetaListContainer::interpret(oEvent *oe, const gdioutput &gdi, const oListParam &par, oListInfo &li) const {

  map<EStdListType, int>::const_iterator it = globalIndex.find(par.listCode);
  if (it != globalIndex.end()) {
    data[it->second].second.interpret(oe, gdi, par, li);
    return true;
  }
  return false;
}

EStdListType MetaListContainer::getType(const std::string &tag) const {
  map<string, EStdListType>::iterator it = tagIndex.find(tag);

  if (it == tagIndex.end())
    throw meosException("Could not load list 'X'.#" + tag);

  return it->second;
}

EStdListType MetaListContainer::getType(const int index) const {
  return EStdListType(index + EFirstLoadedList);
}

void MetaListContainer::getLists(vector<pair<wstring, size_t> > &lists, bool markBuiltIn, 
                                 bool resultListOnly, bool noTeamList) const {
  lists.clear();
  size_t currentIx = 0;
  for (bool i : {false, true}) {
    for (size_t k = 0; k < data.size(); k++) {
      if (data[k].first == RemovedList)
        continue;
      if (resultListOnly && !data[k].second.hasResults())
        continue;

      if (noTeamList && data[k].second.getListType() == oListInfo::EBaseTypeTeam)
        continue;

      if ((data[k].first == InternalList) == (i != markBuiltIn))
        continue;

      if (data[k].first == InternalList) {
        if (markBuiltIn)
          lists.push_back(make_pair(L"[" + lang.tl(data[k].second.getListName()) + L"]", k));
        else
          lists.push_back(make_pair(lang.tl(data[k].second.getListName()), k));
      }
      else
        lists.push_back(make_pair(data[k].second.getListName(), k));
    }

    if (markBuiltIn) {
      sort(lists.begin() + currentIx, lists.end());
      currentIx = lists.size();
    }
  }

  if (!markBuiltIn) {
    sort(lists.begin(), lists.end());
  }
}

void MetaListContainer::mergeParam(int toInsertAfter, int toMerge, bool showTitleBetween) {
  if (toInsertAfter >= MAXLISTPARAMID) {
    toInsertAfter -= MAXLISTPARAMID;
    swap(toMerge, toInsertAfter);
  }

  oListParam &after = getParam(toInsertAfter);
  oListParam &merge = getParam(toMerge);
  merge.showInterTitle = showTitleBetween;

  int oldNext = after.nextList;
  after.nextList = toMerge + 1;
  merge.previousList = toInsertAfter + 1;

  if (oldNext > 0) {
    oListParam &am = getParam(oldNext - 1);
    am.previousList = toMerge + 1;
    merge.nextList = oldNext;
  }

  if (owner)
    owner->updateChanged();
}

void MetaListContainer::getMergeCandidates(int toMerge, vector< pair<wstring, size_t> > &param) const {
  param.clear();
  for (map<int, oListParam>::const_iterator it = listParam.begin(); it != listParam.end(); ++it) {
    if (it->first == toMerge)
      continue;

    if (it->second.previousList == 0) {
      wstring desc = L"Före X#" + it->second.getName();
      param.push_back(make_pair(lang.tl(desc), MAXLISTPARAMID + it->first));
    }

    if (it->second.nextList == 0) {
      wstring desc = L"Efter X#" + it->second.getName();
      param.push_back(make_pair(lang.tl(desc), it->first));
    }
    else {
      const oListParam &next = getParam(it->second.nextList - 1);
      wstring desc = L"Mellan X och Y#" + it->second.getName() + L"#" + next.getName();
      param.push_back(make_pair(lang.tl(desc), it->first));
    }
  }
}

bool MetaListContainer::canSplit(int index) const {
  return getParam(index).nextList > 0;
}

void MetaListContainer::split(int index) {
  oListParam &par = getParam(index);
  int n = par.nextList;
  par.nextList = 0;
  par.previousList = 0;
  if (n > 0) {
    split(n-1);
    if (owner)
      owner->updateChanged();
  }
}

const string &MetaListContainer::getTag(int index) const {
  if (size_t(index) >= data.size())
    throw meosException("Invalid index");

  if (data[index].first != InternalList)
    throw meosException("Invalid list type");

  return data[index].second.getTag();
}

void MetaListContainer::removeList(int index) {
  if (size_t(index) >= data.size())
    throw meosException("Invalid index");

  if (data[index].first != ExternalList)
    throw meosException("Invalid list type");

  data[index].first = RemovedList;
  if (owner)
    owner->updateChanged();
}

void MetaListContainer::saveList(int index, const MetaList &ml) {
  if (size_t(index) >= data.size())
    throw meosException("Invalid index");

  if (data[index].first == InternalList)
    throw meosException("Invalid list type");

  data[index].first = ExternalList;
  data[index].second = ml;
  data[index].second.initUniqueIndex();
  if (owner)
    owner->updateChanged();
}

void MetaList::getFilters(vector< pair<wstring, bool> > &filters) const {
  filters.clear();
  for (map<EFilterList, string>::const_iterator it = filterToSymbol.begin();
                             it != filterToSymbol.end(); ++it) {
    bool has = this->filter.count(it->first) == 1;
    filters.push_back(make_pair(gdi_main->widen(it->second), has));
  }
}

void MetaList::setFilters(const vector<bool> &filters) {
  int k = 0;
  filter.clear();
  assert(filters.size() == filterToSymbol.size());
  for (map<EFilterList, string>::const_iterator it = filterToSymbol.begin();
                             it != filterToSymbol.end(); ++it) {
    bool has = filters[k++];
    if (has)
      filter.insert(it->first);
  }
}

void MetaList::getSubFilters(vector< pair<wstring, bool> > &filters) const {
  filters.clear();
  for (map<ESubFilterList, string>::const_iterator it = subFilterToSymbol.begin();
                             it != subFilterToSymbol.end(); ++it) {
    bool has = this->subFilter.count(it->first) == 1;
    filters.push_back(make_pair(gdi_main->widen(it->second), has));
  }
}

void MetaList::setSubFilters(const vector<bool> &filters) {
  int k = 0;
  subFilter.clear();
  assert(filters.size() == subFilterToSymbol.size());
  for (map<ESubFilterList, string>::const_iterator it = subFilterToSymbol.begin();
                             it != subFilterToSymbol.end(); ++it) {
    bool has = filters[k++];
    if (has)
      subFilter.insert(it->first);
  }
}

void MetaList::getResultModule(const oEvent &oe, vector< pair<wstring, size_t> > &modules, int &currentModule) const {
  modules.clear();
  vector< pair<int, pair<string, wstring> > > mol;
  oe.getGeneralResults(false, mol, true);
  modules.push_back(make_pair(lang.tl("Standard"), 0));
  currentModule = 0;

  for (size_t k = 0; k < mol.size(); k++) {
    modules.push_back(make_pair(mol[k].second.second, mol[k].first));
    if (resultModule == mol[k].second.first)
      currentModule = mol[k].first;
  }
}

MetaList &MetaList::setResultModule(const oEvent &oe, int moduleIx) {
  vector< pair<int, pair<string, wstring> > > mol;
  oe.getGeneralResults(false, mol, false);
  if (moduleIx == 0) {
    //resultModule = "";
    retagResultModule("", false);
    return *this;
  }
  else {
    for (size_t k = 0; k < mol.size(); k++) {
      if (moduleIx == mol[k].first) {
        retagResultModule(mol[k].second.first, false);
        return *this;
      }
    }
  }
  throw meosException("Unknown result module");
}

MetaList &MetaList::setSupportLegSelection(bool state) {
  hideLegSelection = !state;
  return *this;
}

bool MetaList::supportLegSelection() const {
  return !hideLegSelection;
}

MetaList &MetaList::setSupportFromTo(bool from, bool to) {
  supportFromControl = from;
  supportToControl = to;
  return *this;
}

void MetaList::getSortOrder(bool forceIncludeCustom, vector< pair<wstring, size_t> > &orders, int &currentOrder) const {
  orders.clear();
  for(map<SortOrder, string>::const_iterator it = orderToSymbol.begin();
    it != orderToSymbol.end(); ++it) {
      if (it->first != Custom || forceIncludeCustom || !resultModule.empty() || currentOrder == Custom)
        orders.push_back(make_pair(lang.tl(it->second), it->first));
  }
  currentOrder = sortOrder;
}

void MetaList::getBaseType(vector< pair<wstring, size_t> > &types, int &currentType) const {
  types.clear();
  for(map<oListInfo::EBaseType, string>::const_iterator it = baseTypeToSymbol.begin();
    it != baseTypeToSymbol.end(); ++it) {
      if (it->first == oListInfo::EBaseTypeNone || it->first == oListInfo::EBaseTypeCoursePunches
        || it->first == oListInfo::EBaseTypeAllPunches)
        continue;
      types.push_back(make_pair(lang.tl(it->second), it->first));
  }

  currentType = listType;
}

void MetaList::getSubType(vector< pair<wstring, size_t> > &types, int &currentType) const {
  types.clear();

  oListInfo::EBaseType t;
  set<oListInfo::EBaseType> tt;

  t = oListInfo::EBaseTypeNone;
  tt.insert(t);
  types.push_back(make_pair(lang.tl(baseTypeToSymbol[t]), t));

  t = oListInfo::EBaseTypeCoursePunches;
  tt.insert(t);
  types.push_back(make_pair(lang.tl(baseTypeToSymbol[t]), t));

  t = oListInfo::EBaseTypeAllPunches;
  tt.insert(t);
  types.push_back(make_pair(lang.tl(baseTypeToSymbol[t]), t));

  t = oListInfo::EBaseTypeRunner;
  tt.insert(t);
  types.push_back(make_pair(lang.tl(baseTypeToSymbol[t]), t));

  if (tt.count(listSubType) == 0) {
    t = listSubType;
    types.push_back(make_pair(lang.tl(baseTypeToSymbol[t]), t));
  }

  currentType = listSubType;
}

int MetaListContainer::getNumLists(MetaListType t) const {
  int num = 0;
  for (size_t k = 0; k<data.size(); k++) {
    if (data[k].first == t)
      num++;
  }
  return num;
}

void MetaListContainer::getListParam( vector< pair<wstring, size_t> > &param) const {
  for (map<int, oListParam>::const_iterator it = listParam.begin(); it != listParam.end(); ++it) {
    if (it->second.previousList > 0)
      continue;
    param.push_back(make_pair(it->second.getName(), it->first));
  }
}

void MetaListContainer::removeParam(int index) {
  if (listParam.count(index)) {
    listParam[index].previousList = 0;
    if (listParam[index].nextList > 0) {
      oListParam &next = getParam(listParam[index].nextList - 1);
      if (next.previousList == index + 1)
        removeParam(listParam[index].nextList-1);
    }
    listParam.erase(index);
    if (owner)
      owner->updateChanged();
  }
  else
    throw meosException("No such parameters exist");
}

int MetaListContainer::addListParam(oListParam &param) {
  param.saved = true;
  int ix = 0;
  if (!listParam.empty())
    ix = listParam.rbegin()->first + 1;

  listParam[ix] = param;
  if (owner)
    owner->updateChanged();
  return ix;
}

const oListParam &MetaListContainer::getParam(int index) const {
  if (!listParam.count(index))
    throw meosException("Internal error");
  return listParam.find(index)->second;
}

oListParam &MetaListContainer::getParam(int index) {
  if (!listParam.count(index))
    throw meosException("Internal error");

  return listParam.find(index)->second;
}

extern gdioutput *gdi_main;

void MetaListContainer::enumerateLists(vector< pair<wstring, pair<string, wstring> > > &out) const {
  out.clear();
  wchar_t bf[260];
  getUserFile(bf, L"");
  vector<wstring> res;
  expandDirectory(bf, L"*.meoslist", res);
  for (size_t k = 0; k < res.size(); k++) {
    xmlparser xml;
    try {
      xml.read(res[k], 6);
      xmlobject xDef = xml.getObject("MeOSListDefinition");
      wstring name;
      xDef.getObjectString("ListName", name);
      wstring origin;
      xDef.getObjectString("ListOrigin", origin);
      string uid;
      xDef.getObjectString("UID", uid);

      if (!origin.empty())
        name += makeDash(L" - ") + origin;
      out.push_back(make_pair(name, make_pair(uid, res[k])));
    }
    catch (std::exception &) { // Ignore log?!
      out.push_back(make_pair(L"Error? " + res[k], make_pair("?", res[k])));
    }
  }
  sort(out.begin(), out.end());

}

int MetaList::getResultModuleIndex(oEvent *oe, oListInfo &li, const MetaListPost &lp) const {
  if (resultToIndex.empty()) {
    vector< pair<int, pair<string, wstring> > > tagNameList;
    oe->getGeneralResults(false, tagNameList, false);
    resultToIndex[""] = -1;
    for (size_t k = 0; k < tagNameList.size(); k++) {
      resultToIndex[tagNameList[k].second.first] = k;
    }
  }

  if (resultToIndex.count(lp.resultModule) == 0)
    throw meosException("Unknown result module: " + lp.resultModule);

  if (!lp.resultModule.empty())
    li.additionalResultModule(lp.resultModule);

  return resultToIndex[lp.resultModule];
}

void MetaListContainer::getListsByResultModule(const string &tag, vector<int> &listIx) const {
  listIx.clear();
  for (size_t k = 0; k<data.size(); k++) {
    if (data[k].first == RemovedList)
      continue;

    if (data[k].second.getResultModule() == tag) {
      listIx.push_back(k);
    }
  }
}

oListInfo::EBaseType MetaList::getListType() const {
  if (listType == oListInfo::EBaseTypeRunnerGlobal || listType == oListInfo::EBaseTypeRunnerLeg)
    return oListInfo::EBaseTypeRunner;
  if (listType == oListInfo::EBaseTypeTeamGlobal)
    return oListInfo::EBaseTypeTeam;

  return listType;
}

oListInfo::ResultType MetaList::getResultType() const {
  if (listType == oListInfo::EBaseTypeRunnerGlobal)
    return oListInfo::Global;
  if (listType == oListInfo::EBaseTypeTeamGlobal)
    return oListInfo::Global;
  if (listType == oListInfo::EBaseTypeRunnerLeg)
    return oListInfo::Legwise;

  return oListInfo::Classwise;
}

bool MetaList::supportClasses() const {
  if (listType == oListInfo::EBaseTypeControl)
    return false;
  else
    return true;
}

EPostType MetaList::getTypeFromSymbol(wstring &symb) noexcept {
  auto res = symbolToType.find(symb);
  if (res == symbolToType.end())
    return EPostType::lNone; // Error code

  return res->second;
}

void MetaList::fillSymbols(vector < pair<wstring, size_t>> &symb) {
  for (auto s : symbolToType) {
    if (s.second == lAlignNext || s.second == lNone  || s.second == lLineBreak)
      continue;
    wstring desc = L"[" + s.first + L"]\t" + lang.tl(s.first);
    symb.emplace_back(desc, size_t(s.second));
  }
}

void MetaListContainer::synchronizeTo(MetaListContainer &dst) const {
  auto &dstData = dst.data;
  map<wstring, int> dstLst;
  map<string, int> dstLstId;

  for (size_t k = 0; k < dstData.size(); k++) {
    auto &d = dstData[k];
    if (d.first == MetaListType::ExternalList) {
      wstring n = d.second.getListName();
      dstLst[n] = k;

      string id = d.second.getUniqueId();
      dstLstId[id] = k;
    }
  }

  for (auto &d : data) {
    if (d.first == MetaListType::ExternalList) {
      wstring n = d.second.getListName();
      string id = d.second.getUniqueId();

      if (dstLst.count(n))
        dstData[dstLst[n]].second = d.second;
      else if (!dstLstId.count(id)) {
        dstData.push_back(d);
      }
    }
    else if (d.first != MetaListType::RemovedList) {
      dstData.push_back(d); // All internal lists
    }
  }

  dst.setupIndex(EFirstLoadedList);

  map<wstring, int> dstLstParam;

  for (auto &lp : dst.listParam) {
    const wstring &n = lp.second.getName();
    dstLstParam[n] = lp.first;
  }

  for (auto &lp : listParam) {
    EStdListType dstCode = dst.getCodeFromUnqiueId(getUniqueId(lp.second.listCode));
    const wstring &n = lp.second.getName();
    if (dstLstParam.count(n)) {
      dst.listParam[dstLstParam[n]] = lp.second;
      dst.listParam[dstLstParam[n]].listCode = dstCode;
    }
    else {
      int ix = 0;
      if (!listParam.empty())
        ix = listParam.rbegin()->first + 1;

      dst.listParam[ix] = lp.second;
      dst.listParam[ix].listCode = dstCode;
    }
  }
}

void MetaListContainer::getGeneralResults(vector<DynamicResultRef> &rmAll) {
  for (int k = 0; k < getNumLists(); k++) {
    vector<DynamicResultRef> rm;
    getList(k).getDynamicResults(rm);

    if (!isExternal(k)) {
      for (size_t j = 0; j < rm.size(); j++) {
        if (rm[j].res)
          rm[j].res->setReadOnly();
      }
    }
    rmAll.insert(rmAll.end(), rm.begin(), rm.end());
  }

  for (auto &res : freeResultModules) {
    auto dynRes = dynamic_pointer_cast<DynamicResult, GeneralResult>(res.second.ptr);
    dynRes->retaggable(false);
    if (dynRes) {
      rmAll.emplace_back(dynRes, nullptr);
    }
  }
}


void MetaListContainer::getFreeResultModules(vector<pair<string, shared_ptr<DynamicResult>>> &res) const {
  for (auto &r : freeResultModules) {
    auto dynRes = dynamic_pointer_cast<DynamicResult, GeneralResult>(r.second.ptr);
    if (dynRes) {
      res.emplace_back(r.first, dynRes);
    }
  }
}

wstring DynamicResultRef::getAnnotation() const {
  if (ctr)
    return ctr->getListName();
  else
    return L"";
}

void MetaListContainer::updateGeneralResult(string tag, const shared_ptr<DynamicResult> &res) {
  bool changed = false;
  if (!res) {
    changed = freeResultModules.count(tag) > 0;
    freeResultModules.erase(tag);
  }
  else {
    auto rm = freeResultModules.find(tag);
    if (rm == freeResultModules.end())
      changed = true;
    else
      changed = rm->second.ptr.get() != res.get();
    assert(tag == res->getTag());
    freeResultModules.emplace(tag, GeneralResultCtr(tag.c_str(), res->getName(false), res));
  }

  if (changed && owner)
    owner->updateChanged();
}

void MetaList::getAutoComplete(const wstring &w, vector<AutoCompleteRecord> &records) {
  records.clear();
  wchar_t s_lc[1024];
  wcscpy_s(s_lc, w.c_str());
  CharLowerBuff(s_lc, w.length());
  wstring tl;
  for (auto &ts : typeToSymbol) {
    if (ts.first == lNone || ts.first == lAlignNext)
      continue;
    
    tl = lang.tl(ts.second);
    if (filterMatchString(ts.second, s_lc) || filterMatchString(tl, s_lc)) {
      records.emplace_back(tl, ts.second, ts.first);
    }
  }
}
