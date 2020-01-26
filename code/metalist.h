#pragma once

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

class oListInfo;
enum EPostType;
#include "oListInfo.h"
#include "oEvent.h"
#include <map>
class xmlparser;
class xmlobject;
enum gdiFonts;
class oEvent;

const string &itos(int);
const wstring &itow(int i);

class Position
{
  struct PosInfo {
    PosInfo(int f, int wid) : first(f), width(wid), aligned(false), originalPos(f) {}
    int first;   // Actual position
    int width;   // Original block width
    bool aligned;// True if aligned
    const int originalPos; // Original position
    void operator=(const PosInfo &) {throw std::exception("Unsupported");}
  };
  map<string, int> pmap;
  vector< PosInfo > pos; // Pair of position, specified (minimal) width
  void update(int ix, const string &newname, int width, bool alignBlock, bool alignLock);

public:

  int getWidth() const;

  bool postAdjust();

  void add(const string &name, const int width, int blockWidth);
  void add(const string &name, const int width) {
    add(name, width, width);
  }

  void update(const string &oldname, const string &newname,
              const int width, bool alignBlock, bool alignLock);
  void alignNext(const string &newname, const int width, bool alignBlock);

  void newRow();

  int get(const string &name);
  int get(const string &name, double scale);
  int getOriginalPos(const string &name);
  int getOriginalPos(const string &name, double scale);

  void indent(int ind);
};

class MetaListPost {
private:
  void serialize(xmlparser &xml) const;
  void deserialize(const xmlobject &xml);

  EPostType type;
  wstring text;
  wstring alignWithText;
  string resultModule;
  EPostType alignType;
  int leg;
  int minimalIndent;
  bool alignBlock; // True if the next item should also be align (table style block)
  int blockWidth;
  bool mergeWithPrevious;
  gdiFonts font;
  int textAdjust; // 0, textRight, textCenter
  GDICOLOR color;
public:

  MetaListPost(EPostType type_, EPostType align_ = lNone, int leg_ = -1);

  MetaListPost &setBlock(int width) {blockWidth = width; return *this;}
  MetaListPost &setText(const wstring &text_) {text = text_; return *this;}
  MetaListPost &setResultModule(const string &resMod) {resultModule = resMod; return *this;}

  MetaListPost &align(EPostType align_, bool alignBlock_ = true) {alignType = align_; alignBlock = alignBlock_; return *this;}
  MetaListPost &align(bool alignBlock_ = true) {return align(lAlignNext, alignBlock_);}
  MetaListPost &alignText(const wstring &t) {alignWithText = t; return *this;}
  MetaListPost &mergePrevious(bool m_=true) {mergeWithPrevious = m_; return *this;}

  MetaListPost &indent(int ind) {minimalIndent = ind; return *this;}

  void getTypes(vector< pair<wstring, size_t> > &types, int &currentType) const;

  const wstring &getType() const;
  MetaListPost &setType(EPostType type_) {type = type_; return *this;}

  const wstring &getText() const {return text;}
  const string &getResultModule() const {return resultModule;}
  const wstring &getAlignText() const {return alignWithText;}

  int getLeg() const {return leg;}
  void setLeg(int leg_) {leg = leg_;}

  int getMinimalIndent() const {return minimalIndent;}
  bool getAlignBlock() const {return alignBlock;}
  bool isMergePrevious() const {return mergeWithPrevious;}

  int getBlockWidth() const {return blockWidth;}

  const string &getFont() const;
  void setFont(gdiFonts font_) {font = font_;}

  void getFonts(vector< pair<wstring, size_t> > &fonts, int &currentFont) const;

  const string &getTextAdjust() const;
  int getTextAdjustNum() const {return textAdjust;}
  void setTextAdjust(int align);
  const string &getColor() const;
  void setColor(GDICOLOR color);
  GDICOLOR getColorValue() const {return color;}

  static void getAllFonts(vector< pair<wstring, size_t> > &fonts);

  friend class MetaList;
};

struct DynamicResultRef {
  DynamicResultRef(const shared_ptr<DynamicResult> &resIn, MetaList *ctrIn) : res(resIn), ctr(ctrIn) {}
  DynamicResultRef() : ctr(0) {}
  shared_ptr<DynamicResult> res;
  MetaList *ctr;

  wstring getAnnotation() const;
};

class MetaList {
private:

  struct FontInfo {
    wstring font;
    int scale;
    int extraSpaceAbove;

    FontInfo() : scale(0), extraSpaceAbove(0) {}

    const vector< pair<string, wstring> > &serialize(vector< pair<string, wstring> > &props) const {
      props[0].first = "scale";
      props[0].second = itow(scale);
      props[1].first = "above";
      props[1].second = itow(extraSpaceAbove);
      return props;
    }
  };

  vector< vector< vector<MetaListPost> > > data;
  vector<FontInfo> fontFaces;

  wstring listName;
  mutable wstring listOrigin;
  string tag;
  mutable string uniqueIndex;

  mutable bool hasResults_;

  oListInfo::EBaseType listType;
  oListInfo::EBaseType listSubType;
  SortOrder sortOrder;

  set<EFilterList> filter;
  set<ESubFilterList> subFilter;
  vector<GeneralResultCtr> dynamicResults;

  string resultModule;
  bool supportFromControl;
  bool supportToControl;
  bool hideLegSelection;

  enum ListIndex {MLHead = 0, MLSubHead = 1, MLList = 2, MLSubList=3};
  MetaListPost &add(ListIndex ix, const MetaListPost &post);
  void addRow(int ix);
  wstring encode(const wstring &input) const;
  bool isBreak(int x) const;

  static map<EPostType, wstring> typeToSymbol;
  static map<wstring, EPostType> symbolToType;

  static map<oListInfo::EBaseType, string> baseTypeToSymbol;
  static map<string, oListInfo::EBaseType> symbolToBaseType;

  static map<SortOrder, string> orderToSymbol;
  static map<string, SortOrder> symbolToOrder;

  static map<EFilterList, string> filterToSymbol;
  static map<string, EFilterList> symbolToFilter;

  static map<ESubFilterList, string> subFilterToSymbol;
  static map<string, ESubFilterList> symbolToSubFilter;

  static map<gdiFonts, string> fontToSymbol;
  static map<string, gdiFonts> symbolToFont;

  static void initSymbols();

  void serialize(xmlparser &xml, const string &tag,
                 const vector< vector<MetaListPost> > &lp) const;
  void deserialize(const xmlobject &xml, vector< vector<MetaListPost> > &lp);

  int getResultModuleIndex(oEvent *oe, oListInfo &li, const MetaListPost &lp) const;
  mutable map<string, int> resultToIndex;

public:
  MetaList();
  virtual ~MetaList() {}

  bool supportClasses() const;

  const wstring &getListInfo(const oEvent &oe) const;
  void clearTag() {tag.clear();}

  void initUniqueIndex() const;
  const string &getUniqueId() const {
    if (uniqueIndex.empty())
      initUniqueIndex();
    return uniqueIndex;
  }

  void retagResultModule(const string &newTag, bool retagStoredModule);
  bool updateResultModule(const DynamicResult &dr, bool updateSimilar);

  void getDynamicResults(vector<DynamicResultRef> &resultModules) const;
  void getFilters(vector< pair<wstring, bool> > &filters) const;
  void setFilters(const vector<bool> &filters);
  void getSubFilters(vector< pair<wstring, bool> > &filters) const;
  void setSubFilters(const vector<bool> &filters);

  void getResultModule(const oEvent &oe, vector< pair<wstring, size_t> > &modules, int &currentModule) const;
  const string &getResultModule() const {return resultModule;}

  MetaList &setSupportLegSelection(bool state);
  bool supportLegSelection() const;

  MetaList &setSupportFromTo(bool from, bool to);
  bool supportFrom() const {return supportFromControl;}
  bool supportTo() const {return supportToControl;}
  void getSortOrder(bool forceIncludeCustom, vector< pair<wstring, size_t> > &orders, int &currentOrder) const;
  void getBaseType(vector< pair<wstring, size_t> > &types, int &currentType) const;
  void getSubType(vector< pair<wstring, size_t> > &types, int &currentType) const;

  const wstring &getFontFace(int type) const {return fontFaces[type].font;}
  int getFontFaceFactor(int type) const {return fontFaces[type].scale;}
  int getExtraSpace(int type) const {return fontFaces[type].extraSpaceAbove;}

  MetaList &setFontFace(int type, const wstring &face, int factor) {
    fontFaces[type].font = face;
    fontFaces[type].scale = factor;
    return *this;
  }

  MetaList &setExtraSpace(int type, int space) {
    fontFaces[type].extraSpaceAbove = space;
    return *this;
  }

  const wstring &getListName() const {return listName;}
  oListInfo::EBaseType getListType() const;

  oListInfo::ResultType getResultType() const; // Classwise or global

  bool hasResults() const {return hasResults_;}
  const string &getTag() const {return tag;}

  void getAlignTypes(const MetaListPost &mlp, vector< pair<wstring, size_t> > &types, int &currentType) const;
  void getIndex(const MetaListPost &mlp, int &gix, int &lix, int &ix) const;

  MetaList &setResultModule(const oEvent &oe, int moduleIx);

  MetaList &setListType(oListInfo::EBaseType t) {listType = t; return *this;}
  MetaList &setSubListType(oListInfo::EBaseType t) {listSubType = t; return *this;}
  MetaList &setSortOrder(SortOrder so) {sortOrder = so; return *this;}

  MetaList &addFilter(EFilterList f) {filter.insert(f); return *this;}
  MetaList &addSubFilter(ESubFilterList f) {subFilter.insert(f); return *this;}

  void save(const wstring &file, const oEvent *oe) const;
  void load(const wstring &file);

  bool isValidIx(size_t gIx, size_t lIx, size_t ix) const;

  void save(xmlparser &xml, const oEvent *oe) const;
  void load(const xmlobject &xDef);

  void interpret(oEvent *oe, const gdioutput &gdi, const oListParam &par,
                 int lineHeight, oListInfo &li) const;

  MetaList &setListName(const wstring &title) {listName = title; return *this;}

  MetaListPost &addNew(int groupIx, int lineIx, int &ix);
  MetaListPost &getMLP(int groupIx, int lineIx, int ix);
  void removeMLP(int groupIx, int lineIx, int ix);
  void moveOnRow(int groupIx, int lineIx, int &ix, int delta);

  MetaListPost &addToHead(const MetaListPost &post) {return add(MLHead, post);}
  MetaListPost &addToSubHead(const MetaListPost &post) {return add(MLSubHead, post).setBlock(10);}
  MetaListPost &addToList(const MetaListPost &post) {return add(MLList, post);}
  MetaListPost &addToSubList(const MetaListPost &post) {return add(MLSubList, post);}

  const vector< vector<MetaListPost> > &getList() const {return data[MLList];}
  const vector< vector<MetaListPost> > &getSubList() const {return data[MLSubList];}
  const vector< vector<MetaListPost> > &getHead() const {return data[MLHead];}
  const vector< vector<MetaListPost> > &getSubHead() const {return data[MLSubHead];}

  vector< vector<MetaListPost> > &getList() {return data[MLList];}
  vector< vector<MetaListPost> > &getSubList() {return data[MLSubList];}
  vector< vector<MetaListPost> > &getHead() {return data[MLHead];}
  vector< vector<MetaListPost> > &getSubHead() {return data[MLSubHead];}

  void newListRow() {addRow(MLList);}
  void newSubListRow() {addRow(MLSubList);}
  void newHead() {addRow(MLHead);}
  void newSubHead() {addRow(MLSubHead);}

  /** Lookup type from symbol. Return lNone if not found, no exception*/
  static EPostType getTypeFromSymbol(wstring &symb) noexcept;

  static void fillSymbols(vector < pair<wstring, size_t>> &symb);

  friend class MetaListPost;
};

class MetaListContainer {
public:
  enum MetaListType {InternalList, ExternalList, RemovedList};
private:
  vector< pair<MetaListType, MetaList> > data;
  mutable map<EStdListType, int> globalIndex;
  mutable map<string, EStdListType> tagIndex;
  mutable map<string, EStdListType> uniqueIndex;
  map<int, oListParam> listParam;

  map<string, GeneralResultCtr> freeResultModules;

  oEvent *owner;
public:

  MetaListContainer(oEvent *owner);
  MetaListContainer(oEvent *owner, const MetaListContainer &src);

  virtual ~MetaListContainer();

  string getUniqueId(EStdListType code) const;
  EStdListType getCodeFromUnqiueId(const string &id) const;
  wstring makeUniqueParamName(const wstring &nameIn) const;

  bool updateResultModule(const DynamicResult &res, bool updateSimilar);
 
  int getNumParam() const {return listParam.size();}
  int getNumLists() const {return data.size();}
  int getNumLists(MetaListType) const;

  EStdListType getType(const string &tag) const;
  EStdListType getType(const int index) const;

  const MetaList &getList(int index) const;
  MetaList &getList(int index);

  const oListParam &getParam(int index) const;
  oListParam &getParam(int index);

  void getListsByResultModule(const string &tag, vector<int> &listIx) const;

  MetaList &addExternal(const MetaList &ml);
  void clearExternal();

  void getLists(vector< pair<wstring, size_t> > &lists, 
                bool markBuiltIn, 
                bool resultListOnly, 
                bool noTeamList) const;

  const string &getTag(int index) const;

  void removeList(int index);
  void saveList(int index, const MetaList &ml);
  bool isInternal(int index) const {return data[index].first == InternalList;}
  bool isExternal(int index) const {return data[index].first == ExternalList;}

  void updateGeneralResult(string tag, const shared_ptr<DynamicResult> &res);
  void getGeneralResults(vector<DynamicResultRef> &resMod);

  void save(MetaListType type, xmlparser &xml, const oEvent *oe) const;
  /** Returns true if all lists where loaded, false if some list was in a unnsupported version and ignoreOld was set.
     Throws if some list was incorrect. */
  bool load(MetaListType type, const xmlobject &xDef, bool ignoreOld);

  void setupListInfo(int firstIndex, map<EStdListType, oListInfo> &listMap, bool resultsOnly) const;
  void setupIndex(int firstIndex) const;

  void getListParam( vector< pair<wstring, size_t> > &param) const;
  void removeParam(int index);
  /** Return the list index.*/
  int addListParam(oListParam &listParam);

  void mergeParam(int toInsertAfter, int toMerge, bool showTitleBetween);
  void getMergeCandidates(int toMerge, vector< pair<wstring, size_t> > &param) const;
  bool canSplit(int index) const;
  void split(int index);

  void synchronizeTo(MetaListContainer &dst) const;

  bool interpret(oEvent *oe, const gdioutput &gdi, const oListParam &par,
                 int lineHeight, oListInfo &li) const;

  void enumerateLists(vector< pair<wstring, pair<string, wstring> > > &out) const;
};
