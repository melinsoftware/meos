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

// gdioutput.h: interface for the gdioutput class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <set>
#include <map>
#include <vector>

#include <unordered_map>
#include <unordered_set>

#include <algorithm>
#include "subcommand.h"

class Toolbar;

class gdioutput;
class oEvent;
typedef oEvent *pEvent;

struct PrinterObject;

class GDIImplFontEnum;
class GDIImplFontSet;

class Table;
class FixedTabs;

struct PageInfo;
struct RenderedPage;
class AnimationData;

enum GDICOLOR;
enum KeyCommandCode;
enum gdiFonts;
#include "gdistructures.h"
#include <memory>

#define START_YP 30
#define NOTIMEOUT 0x0AAAAAAA

constexpr int baseButtonWidth = 150;

constexpr int GDI_BUTTON_SPACING = 8;

typedef list<ToolInfo> ToolList;

struct FontInfo {
  const wstring *name;
  HFONT normal;
  HFONT bold;
  HFONT italic;
};

class AutoCompleteInfo;

class Recorder;

class gdioutput {
protected:
  string tag;
  // Database error state warning
  bool dbErrorState;
  // Flag set to true when clearPage is called.
  bool hasCleared;
  bool useTables;
  // Set to true when in test mode
  bool isTestMode;

  bool highContrast;

  void deleteFonts();
  void constructor(double _scale);

  void updateStringPosCache();
  vector<TextInfo*> shownStrings;

  void enableCheckBoxLink(TextInfo& ti, bool enable);

  //void CalculateCS(TextInfo &text);
  //void printPage(PrinterObject &po, int StartY, int &EndY, bool calculate);
  void printPage(PrinterObject& po, const PageInfo& pageInfo, RenderedPage& page);
  bool startDoc(PrinterObject& po);

  bool getSelectedItem(ListBoxInfo& lbi);
  bool doPrint(PrinterObject& po, PageInfo& pageInfo, pEvent oe, bool respectPageBreak);

  PrinterObject* po_default;

  void restoreInternal(const RestoreInfo& ri);

  void setFontCtrl(HWND hWnd);

  list<TextInfo> TL;

  //True if textlist has increasing y-values so
  //that we can optimize rendering.
  bool renderOptimize;
  //Stored iterator used to optimize rendering
  //by avoiding to loop through complete TL.
  list<TextInfo>::iterator itTL;

  // References into TL for all images
  vector<TextInfo*> imageReferences;

  // Needed after removing images to clear unused references
  void updateImageReferences();

  list<ButtonInfo> BI;
  unordered_map<HWND, ButtonInfo*> biByHwnd;

  list<InputInfo> II;
  unordered_map<HWND, InputInfo*> iiByHwnd;

  list<ListBoxInfo> LBI;
  unordered_map<HWND, ListBoxInfo*> lbiByHwnd;

  list<DataStore> DataInfo;
  list<EventInfo> Events;
  list<RectangleInfo> Rectangles;
  list<TableInfo> Tables;
  list<TimerInfo> timers;

  Toolbar* toolbar;
  ToolList toolTips;

  map<string, RestoreInfo> restorePoints;

  list<GuiEvent> onClear;
  list<GuiEvent> postClear;

  list<InfoBox> IBox;

  list<HWND> FocusList;
  struct FucusInfo {
    bool wasTabbed;
    HWND hWnd;
    FucusInfo() : wasTabbed(false), hWnd(0) {}
    FucusInfo(HWND wnd) : wasTabbed(false), hWnd(wnd) {}
  };

  FucusInfo currentFocus;

  int lineHeight;
  HWND hWndTarget;
  HWND hWndAppMain;
  HWND hWndToolTip;
  HWND hWndTab;
  list<RECT> monitorConfiguration;

  HBRUSH Background;

  mutable map<pair<int, wstring>, int> fontHeightCache;

  map<wstring, GDIImplFontSet> fonts;
  const GDIImplFontSet& getCurrentFont() const;
  const GDIImplFontSet& getFont(const wstring& font) const;
  const GDIImplFontSet& loadFont(const wstring& font);
  mutable const GDIImplFontSet* currentFontSet;

  int MaxX;
  int MaxY;
  int CurrentX;
  int CurrentY;
  int SX;
  int SY;

  enum class FlowDirection {
    Down,
    Right,
    None,
  };

  FlowDirection flowDirection;

  int OffsetY; //Range 0 -- MaxY
  int OffsetX; //Range 0 -- MaxX

  // Maximum height of a text block of the current view.
  int maxTextBlockHeight = 0;

  //Set to true if we should not update window during "addText" operations
  bool manualUpdate;

  LRESULT ProcessMsgWrp(UINT iMessage, LPARAM lParam, WPARAM wParam);
  void getWindowText(HWND hWnd, wstring& text);
  double scale;
  HFONT getGUIFont() const;

  void resetLast() const;
  mutable int lastFormet;
  mutable bool lastActive;
  mutable bool lastHighlight;
  mutable DWORD lastColor;
  mutable wstring lastFont;

  void initCommon(double scale, const wstring& font);

  void processButtonMessage(ButtonInfo& bi, WPARAM wParam);
  void processEditMessage(InputInfo& bi, WPARAM wParam);
  void processComboMessage(ListBoxInfo& bi, WPARAM wParam);
  void processListMessage(ListBoxInfo& bi, WPARAM wParam);

  void doEnter();
  void doEscape();
  bool doUpDown(int direction);

  void drawBoxText(HDC hDC, const InfoBox& Box, bool highligh);
  void drawBoxes(HDC hDC, RECT& rc);
  void drawBox(HDC hDC, InfoBox& box, RECT& pos);
  void drawBoxBg(HDC hDC, const InfoBox& box) const;
  void computeBoxesBoundingBox(RECT& rc) const;
  void drawCloseBox(HDC hDC, RECT& Close, bool pressed);

  FixedTabs* tabs;

  wstring currentFont;
  vector< GDIImplFontEnum > enumeratedFonts;

  double autoSpeed;
  double autoPos;
  mutable double lastSpeed;
  mutable double autoCounter;

  bool lockRefresh;
  bool fullScreen;
  bool hideBG;

  DWORD backgroundColor1;
  DWORD backgroundColor2;
  wstring backgroundImage;
  DWORD foregroundColor;

  mutable bool commandLock;
  mutable uint64_t commandUnlockTime;

  bool hasCommandLock() const;
  void setCommandLock() const;
  void liftCommandLock() const;

  struct ScreenStringInfo {
    RECT rc;
    wstring str;
    bool reached;

    ScreenStringInfo(const RECT& r, const wstring& s) {
      rc = r;
      str = s;
      reached = false;
    }
  };

  wstring listDescription;

  mutable map<pair<int, int>, ScreenStringInfo> screenXYToString;
  mutable map<wstring, pair<int, int> > stringToScreenXY;
  mutable pair<int, int> snapshotMaxXY;
  bool hasAnyTimer;

  friend class InputInfo;
  friend class TestMeOS;

  // Recorder, the second member is true if the recorder is owned and should be deleted
  pair<Recorder*, bool> recorder;
  list< pair<const SubCommand*, string> > subCommands;

  shared_ptr<AnimationData> animationData;

  shared_ptr<AutoCompleteInfo> autoCompleteInfo;

  wstring delayedAlert;

  struct GuiMeasure {
    int height = 0;
    int extraX = 0;
    int letterWidth = 0;
    float avgCharWidth = 0;
  };

  mutable shared_ptr<GuiMeasure> guiMeasure;

public:

  void updateMonitorConfiguration();

  // For enumeration
  void addMonitorRect(const RECT& rc) {
    monitorConfiguration.push_back(rc);
  }

  AutoCompleteInfo& addAutoComplete(const string& key);
  void clearAutoComplete(const string& key);
  bool hasAutoComplete() const { return autoCompleteInfo != nullptr; }
  // Return the bounding dimension of the desktop
  void getVirtualScreenSize(RECT& rc);

  void getWindowsPosition(RECT& rc) const;
  void setWindowsPosition(const RECT& rc);


  void initRecorder(Recorder* rec);
  Recorder& getRecorder();
  string dbPress(const string& id, int extra);
  string dbPress(const string& id, const char* extra);

  string dbSelect(const string& id, int data);
  void dbInput(const string& id, const string& test);
  void dbCheck(const string& id, bool state);
  string dbClick(const string& id, int extra);
  void dbDblClick(const string& id, int data);

  void dbRegisterSubCommand(const SubCommand* cmd, const string& action);
  void runSubCommand();

  // Add the next answer for a dialog popup
  void dbPushDialogAnswer(const string& answer);
  mutable list<string> cmdAnswers;

  int dbGetStringCount(const string& str, bool subString) const;

  // Ensure list of stored answers is empty
  void clearDialogAnswers(bool checkEmpty);

  void internalSelect(ListBoxInfo& bi);

  bool isTest() const { return isTestMode; }
  const string& getTag() const { return tag; }
  bool hasTag(const string& t) const { return tag == t; }
  static const wstring& recodeToWide(const string& input);
  static const string& recodeToNarrow(const wstring& input);

  static const wstring& widen(const string& input);
  static const string& narrow(const wstring& input);

  static const string& toUTF8(const wstring& input);
  static const wstring& fromUTF8(const string& input);

  
  void updateTabFont();

  void getFontInfo(const TextInfo& ti, FontInfo& fi) const;

  /** Return true if rendering text should be skipped for
    this format. */
  static bool skipTextRender(int format);

  const list<TextInfo>& getTL() const { return TL; }

  void getEnumeratedFonts(vector< pair<wstring, size_t> >& output) const;
  const wstring& getFontName(int id);
  double getRelativeFontScale(gdiFonts font, const wchar_t* fontFace) const;

  bool isFullScreen() const { return fullScreen; }
  void setFullScreen(bool useFullScreen);
  void setColorMode(DWORD bgColor1, DWORD bgColor2 = -1,
    DWORD fgColor = -1, const wstring& bgImage = L"");

  bool hasFGColor() const;
  bool hasBGColor() const;
  bool hasBGColor2() const;

  DWORD getFGColor() const;
  
  
  DWORD getBGColor() const;
  DWORD getBGColor2() const;
  const wstring& getBGImage() const;

  void setAnimationMode(const shared_ptr<AnimationData>& mode);

  void setAutoScroll(double speed);
  void getAutoScroll(double& speed, double& pos) const;
  void storeAutoPos(double pos);
  int getAutoScrollDir() const { return (autoSpeed > 0 ? 1 : -1); }
  int setHighContrastMaxWidth();
  void hideBackground(bool hide) { hideBG = hide; }
  HWND getToolbarWindow() const;
  bool hasToolbar() const;
  void activateToolbar(bool active);

  void processToolbarMessage(const string& id, Table* data);

  void synchronizeListScroll(const string& id1, const string& id2);

  FixedTabs& getTabs();

  // True if up/down is locked, i.e, don't move page
  bool lockUpDown;


  double getScale() const { return scale; }
  void enableEditControls(bool enable, bool processAll = false);

  bool hasEditControl() const;

  void setFont(int size, const wstring& font);

  int getButtonHeight() const;
  int scaleLength(int input) const { return int(scale * input + 0.5); }

  // Fill in current printer settings
  void fetchPrinterSettings(PrinterObject& po) const;

  void tableCB(ButtonInfo& bu, Table* t);

  wchar_t* getExtra(const char* id) const;
  int getExtraInt(const char* id) const;

  void enableTables();
  void disableTables();

  void pasteText(const char* id);
  void print(pEvent oe, Table* t = nullptr, bool printMeOSHeader = true, bool noMargin = false, bool respectPageBreak = true);
  void print(PrinterObject& po, pEvent oe, bool printMeOSHeader = true, bool noMargin = false, bool respectPageBreak = true);
  void printSetup(PrinterObject& po);
  void destroyPrinterDC(PrinterObject& po);

  void setSelection(const string& id, const set<int>& selection);
  void setSelection(const wstring& id, const set<int>& selection) {
    setSelection(narrow(id), selection);
  }

  void getSelection(const string& id, set<int>& selection);

  HWND getHWNDTarget() const { return hWndTarget; }
  HWND getHWNDMain() const { return hWndAppMain; }

  void scrollToBottom();
  void scrollTo(int x, int y);
  void setOffset(int x, int y, bool update);

  void selectTab(int Id);

  void addTable(const shared_ptr<Table>& table, int x, int y);
  Table& getTable() const; //Get the (last) table. If needed, add support for named tables...

  ToolInfo& addToolTip(const string& id, const wstring& tip, HWND hWnd, RECT* rc = nullptr);
  void removeToolTip(const string& id);

  ToolInfo* getToolTip(const string& id);
  ToolInfo& updateToolTip(const string& id, const wstring& tip);

  HWND getToolTip() { return hWndToolTip; }

  void init(HWND hWnd, HWND hMainApp, HWND hTab);
  bool openDoc(const wstring &doc);
  wstring browseForSave(const vector<pair<wstring, wstring>>& filter,
    const wstring& defext, int& FilterIndex);
  wstring browseForOpen(const vector<pair<wstring, wstring>>& filter,
    const wstring& defext);
  wstring browseForFolder(const wstring& folderStart, const wchar_t* descr);

  bool clipOffset(int PageX, int PageY, int& MaxOffsetX, int& MaxOffsetY);
  RectangleInfo& addRectangle(const RECT& rc, GDICOLOR Color = GDICOLOR(-1),
    bool DrawBorder = true, bool addFirst = false);

  RectangleInfo& getRectangle(const char* id);

  DWORD makeEvent(const string& id, const string& origin,
    DWORD data, int extraData, bool flushEvent);

  void unregisterEvent(const string& id);
  EventInfo& registerEvent(const string& id, GUICALLBACK cb);

  int sendCtrlMessage(const string& id);
  bool canClear();
  
  template<typename H>
  void setPostClearCb(const char *id, H cb) {
    if (find_if(postClear.begin(), postClear.end(), [&id](auto& c) {return c.id == id; }) == postClear.end())
      postClear.emplace_back(cb);
  }

  template<typename H>
  void setOnClearCb(const char* id, H cb) {
    if (find_if(onClear.begin(), onClear.end(), [&id](auto& c) {return c.id == id; }) == onClear.end())
      onClear.emplace_back(cb);
  }

  void clearPostClearCb() {
    postClear.clear();
  }

  void clearOnClearCb() {
    onClear.clear();
  }

  void restore(const string& restorePointId = "", bool doRefresh = true);

  /// Restore, but do not update client area size,
  /// position, zoom, scrollbars, and do not refresh
  void restoreNoUpdate(const string& restorePointId);

  RECT getDimensionSince(const string& restorePointId) const;

  void setRestorePoint();
  void setRestorePoint(const string& id);

  bool removeWidget(const string& id);
  bool hideWidget(const string& id, bool hide = true);

  // Get and set restore point presence for a widget
  bool getWidgetRestorePoint(const string& id, string& restorePoint) const;
  void setWidgetRestorePoint(const string& id, const string& restorePoint);

  void CheckInterfaceTimeouts(uint64_t T);
  
  bool removeFirstInfoBox(const string& id);

  InfoBox &addInfoBox(const string &id, 
                      const wstring &text, 
                      const wstring &extraLine, 
                      BoxStyle style = BoxStyle::Header,
                      int timeOut = 0, 
                      GUICALLBACK cb = nullptr, 
                      bool autoRefresh = true);
  
  
  void updateObjectPositions();
  void drawBackground(HDC hDC, RECT& rc);
  void renderRectangle(HDC hDC, RECT* clipRegion, const RectangleInfo& ri);

  void updateScrollbars() const;
  void updateToolTips();

  void setOffsetY(int oy);
  void setOffsetX(int ox);
  int getPageY() const;
  int getPageX() const;
  int getOffsetY() const { return OffsetY; }
  int getOffsetX() const { return OffsetX; }

  void RenderString(TextInfo& ti, const wstring& text, HDC hDC);
  void RenderString(TextInfo& ti, HDC hDC = 0);
  void calcStringSize(TextInfo& ti, HDC hDC = 0) const;
  void formatString(const TextInfo& ti, HDC hDC) const;

  static wstring getTimerText(const TextInfo &tit, uint64_t T);
  static wstring getTimerText(int ZeroTime, int format, bool timeInSeconds, const wstring &textFormat);

  void fadeOut(string Id, int ms);
  void setWaitCursor(bool wait);
  void setWindowTitle(const wstring& title);
  bool selectFirstItem(const string& name);
  void removeString(string Id);
  void refresh() const;
  void refreshFast() const;

  void takeShownStringsSnapshot();
  void refreshSmartFromSnapshot(bool allowMoveOffset);

  void dropLine(double lines = 1) { CurrentY += int(lineHeight * lines); MaxY = max(MaxY, CurrentY); }
  int getCX() const { return CurrentX; }
  int getCY() const { return CurrentY; }
  int getWidth() const { return MaxX; }
  int getHeight() const { return MaxY; }
  void getTargetDimension(int& x, int& y) const;

  pair<int, int> getPos() const { return make_pair(CurrentX, CurrentY); }
  void setPos(const pair<int, int>& p) { CurrentX = p.first; CurrentY = p.second; }

  void setCX(int cx) { CurrentX = cx; }
  void setCY(int cy) { CurrentY = cy; }
  int getLineHeight() const { return lineHeight; }
  int getLineHeight(gdiFonts font, const wchar_t* face) const;

  BaseInfo* setInputFocus(const string& id, bool select = false);
  InputInfo* getInputFocus();

  void enableInput(const char* id, bool acceptMissing = false,
    int requireExtraMatch = -1) {
    setInputStatus(id, true, acceptMissing, requireExtraMatch);
  }
  void disableInput(const char* id, bool acceptMissing = false,
    int requireExtraMatch = -1) {
    setInputStatus(id, false, acceptMissing, requireExtraMatch);
  }
  void setInputStatus(const char* id, bool status, bool acceptMissing = false, int requireExtraMatch = -1);
  void setInputStatus(const string& id, bool status, bool acceptMissing = false, int requireExtraMatch = -1)
  {
    setInputStatus(id.c_str(), status, acceptMissing, requireExtraMatch);
  }

  void setTabStops(const string& name, int t1, int t2 = -1);
  void setData(const string& id, DWORD data);
  void setData(const string& id, void* data);
  void setData(const string& id, const string& data);

  void* getData(const string& id) const;
  int getDataInt(const string& id) const { return int(size_t(getData(id))); }

  bool getData(const string& id, string& out) const;


  DWORD selectColor(wstring& def, DWORD input);

  void autoRefresh(bool flag) { manualUpdate = !flag; }

  bool getData(const string& id, DWORD& data) const;
  bool hasData(const char* id) const;

  int getItemDataByName(const char* id, const char* name) const;
  bool selectItemByData(const char* id, int data);
  bool selectItemByIndex(const char* id, int index);

  void removeSelected(const char* id);

  bool selectItemByData(const string& id, int data) {
    return selectItemByData(id.c_str(), data);
  }

  enum class AskAnswer { AnswerNo = 0, AnswerYes = 1, AnswerCancel = 2, AnswerOK = 3 };
  bool ask(const wstring& s, const char *yesButton = nullptr, const char *noButton = nullptr);
  AskAnswer askOkCancel(const wstring& s);
  AskAnswer askCancel(const wstring& s, const char* yesButton = nullptr, const char* noButton = nullptr);

  void alert(const string& msg) const;
  void alert(const wstring& msg) const;
  // Alert from main thread (via callback)
  void delayAlert(const wstring& msg);
  // Get and clear any delayed alert
  wstring getDelayedAlert();

  void fillDown() { flowDirection = FlowDirection::Down; }
  void fillRight() { flowDirection = FlowDirection::Right; }
  void fillNone() { flowDirection = FlowDirection::None; }

  void newColumn() { CurrentY = START_YP; CurrentX = MaxX + 10; }
  void newRow() { CurrentY = MaxY; CurrentX = 10; }

  void pushX() { SX = CurrentX; }
  void pushY() { SY = CurrentY; }
  void popX() { CurrentX = SX; }
  void popY() { CurrentY = SY; }

  bool updatePos(int x, int y, int width, int height);
  bool updatePosTight(int x, int y, int width, int height, int marginx, int marginy);

  void adjustDimension(int width, int height);

  /** Return a selected item*/
  bool getSelectedItem(const string& id, ListBoxInfo& lbi);

  /** Get number of items in a list box.'*/
  int getNumItems(const char* id);

  /** Return the selected data in first, second indicates if data was available*/
  pair<int, bool> getSelectedItem(const string& id);
  pair<int, bool> getSelectedItem(const char* id);

  bool addItem(const string& id, const wstring& text, size_t data = 0);
  bool setItems(const string& id, const vector< pair<wstring, size_t> >& items);
  bool modifyItemDescription(const string& id, size_t itemData, const wstring &description);

  void filterOnData(const string& id, const unordered_set<int>& filter);

  bool clearList(const string& id);

  bool hasWidget(const string& id) const;

  const wstring& getText(const char* id, bool acceptMissing = false, int requireExtraMatch = -1) const;

  BaseInfo& getBaseInfo(const string& id, int requireExtraMatch = -1) const {
    return getBaseInfo(id.c_str(), requireExtraMatch);
  }
  BaseInfo& getBaseInfo(const char* id, int requireExtraMatch = -1) const;
  BaseInfo& getBaseInfo(const wchar_t* id, int requireExtraMatch = -1) const {
    return getBaseInfo(narrow(id).c_str(), requireExtraMatch);
  }

  int getTextNo(const char* id, bool acceptMissing = false) const;
  int getTextNo(const string& id, bool acceptMissing = false) const
  {
    return getTextNo(id.c_str(), acceptMissing);
  }

  const wstring& getText(const string& id, bool acceptMissing = false) const
  {
    return getText(id.c_str(), acceptMissing);
  }

  // Insert text and notify "focusList"
  bool insertText(const string& id, const wstring& text);

  int getFontHeight(int format, const wstring& fontFace) const;

  // The html version should be UTF-8.
  void copyToClipboard(const string& html,
    const wstring& txt) const;

  BaseInfo* setTextTranslate(const char* id, const wstring& text, bool update = false);
  BaseInfo* setTextTranslate(const char* id, const wchar_t* text, bool update = false);
  BaseInfo* setTextTranslate(const string& id, const wstring& text, bool update = false);

  BaseInfo* setText(const char* id, const wstring& text, bool update = false, int requireExtraMatch = -1, bool updateOriginal = true);
  BaseInfo* setText(const wchar_t* id, const wstring& text, bool update = false) {
    return setText(narrow(id), text, update);
  }
  BaseInfo* setText(const char* id, int number, bool update = false);
  BaseInfo* setText(const string& id, const wstring& text, bool update = false) {
    return setText(id.c_str(), text, update);
  }
  BaseInfo* setTextZeroBlank(const char* id, int number, bool update = false);
  BaseInfo* setText(const string& id, int number, bool update = false) {
    return setText(id.c_str(), number, update);
  }
  TextInfo* setImage(const string& id, int imgId, bool update = false);

  void clearPage(bool autoRefresh, bool keepToolbar = false);

  void TabFocus(int direction = 1);
  void enter();
  void escape();
  bool upDown(int direction);
  void keyCommand(KeyCommandCode code);

  LRESULT ProcessMsg(UINT iMessage, LPARAM lParam, WPARAM wParam);
  void setWindow(HWND hWnd) { hWndTarget = hWnd; }

  enum class ScaleOperation {
    NoRefresh,
    Refresh,
    NoUpdate
  };

  void scaleSize(double scale, bool allowSmallScale = false, ScaleOperation type = ScaleOperation::Refresh);

  ButtonInfo& addButton(const string& id, const wstring& text, GUICALLBACK cb = nullptr,
    const wstring& tooltip = L"");

  ButtonInfo& addButton(int x, int y, const string& id, const wstring& text,
    GUICALLBACK cb = nullptr, const wstring& tooltop = L"");

  ButtonInfo& addButton(int x, int y, int w, const string& id, const wstring& text,
    GUICALLBACK cb, const wstring& tooltip, bool absPos, bool hasState);

  ButtonInfo& addButton(int x, int y, int width, int height, 
                        const string& id, const wstring& text,
                        gdiFonts font, GUICALLBACK cb, 
                        const wstring& tooltip, 
                        bool absPos, bool hasState);

  ButtonInfo& addCheckbox(const string& id, const wstring& text, GUICALLBACK cb = nullptr,
    bool Checked = true, const wstring& tooltip = L"");
  ButtonInfo& addCheckbox(int x, int y, const string& id,
    const wstring& text, GUICALLBACK cb = nullptr,
    bool checked = true, const wstring& tooltop = L"", bool absPos = false);


  /// XXX Temporary
  ButtonInfo& addButton(const string& id, const string& text, GUICALLBACK cb = nullptr, const string& tooltip = "");

  ButtonInfo& addButton(int x, int y, const string& id, const string& text,
    GUICALLBACK cb = nullptr, const string& tooltop = "");
  ButtonInfo& addButton(int x, int y, int w, const string& id, const string& text,
    GUICALLBACK cb, const string& tooltop, bool AbsPos, bool hasState);

  ButtonInfo& addCheckbox(const string& id, const string& text, GUICALLBACK cb = nullptr, bool Checked = true, const string& Help = "");
  ButtonInfo& addCheckbox(int x, int y, const string& id, const string& text, GUICALLBACK cb = nullptr, bool Checked = true, const string& Help = "", bool AbsPos = false);
  /// XXX

  bool isChecked(const string& id);
  void check(const string& id, bool state, bool keepOriginalState = false);

  bool isInputChanged(const string& exclude);

  /** Get width of input widget with specified length (chars)*/
  pair<int, int> getInputDimension(int length) const;

  InputInfo& addInput(const string& id, const wstring& text = L"", int length = 16, GUICALLBACK cb = nullptr,
    const wstring& Explanation = L"", const wstring& tooltip = L"");
  InputInfo& addInput(int x, int y, const string& id, const wstring& text, int length,
    GUICALLBACK cb = nullptr, const wstring& Explanation = L"", const wstring& tooltip = L"");

  InputInfo* replaceSelection(const char* id, const wstring& text);

  InputInfo& addInputBox(const string& id, int width, int height, const wstring& text,
    GUICALLBACK cb, const wstring& explanation);

  InputInfo& addInputBox(const string& id, int x, int y, int width, int height,
    const wstring& text, GUICALLBACK cb, const wstring& explanation);

  ListBoxInfo& addListBox(const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"", bool multiple = false);
  ListBoxInfo& addListBox(int x, int y, const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"", bool multiple = false);

  ListBoxInfo& addSelection(const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"");
  ListBoxInfo& addSelection(int x, int y, const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"");

  ListBoxInfo& addCombo(const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"");
  ListBoxInfo& addCombo(int x, int y, const string& id, int width, int height, GUICALLBACK cb = nullptr, const wstring& explanation = L"", const wstring& tooltip = L"");

  // Grows a listbox, selection, combo in X-direction to fit current contents. Returns true if changed.
  bool autoGrow(const char* id);

  void setListDescription(const wstring& desc);

  // Wide versions
  TextInfo& addString(const string& id, int format, const wstring& text, GUICALLBACK cb = nullptr);
  TextInfo& addString(const string& id, int yp, int xp, int format, const wstring& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  TextInfo& addString(const char* id, int format, const wstring& text, GUICALLBACK cb = nullptr);
  TextInfo& addString(const char* id, int yp, int xp, int format, const wstring& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  // Untranslated versions
  TextInfo& addStringUT(int yp, int xp, int format, const wstring& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  TextInfo& addStringUT(int format, const wstring& text, GUICALLBACK cb = nullptr);

  // Temporary XXX
  TextInfo& addString(const string& id, int format, const string& text, GUICALLBACK cb = nullptr);
  TextInfo& addString(const string& id, int yp, int xp, int format, const string& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  TextInfo& addString(const char* id, int format, const string& text, GUICALLBACK cb = nullptr);
  TextInfo& addString(const char* id, int yp, int xp, int format, const string& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  // Untranslated versions
  TextInfo& addStringUT(int yp, int xp, int format, const string& text,
    int xlimit = 0, GUICALLBACK cb = nullptr, const wchar_t* fontFace = nullptr);
  TextInfo& addStringUT(int format, const string& text, GUICALLBACK cb = nullptr);
  // XXX Temporary

  TextInfo& addImage(const string& id, int format, const wstring& imageId,
    int width = 0, int height = 0, GUICALLBACK cb = nullptr) {
    return addImage(id, getCY(), getCX(), format, imageId, width, height, cb);
  }

  TextInfo& addImage(const string& id, int yp, int xp, int format, const wstring& imageId,
    int width = 0, int height = 0, GUICALLBACK cb = nullptr);

  TextInfo& addTimer(int yp, int xp, int format, int zeroTime, const wstring &textSrc = L"",
                     int xlimit = 0, GUICALLBACK cb = nullptr, 
                     int TimeOut = NOTIMEOUT, const wchar_t* fontFace = nullptr);

  TextInfo& addTimeout(int TimeOut, GUICALLBACK cb);

  void removeTimeoutMilli(const string& id);
  TimerInfo& addTimeoutMilli(int timeOut, const string& id, GUICALLBACK cb);
  void timerProc(TimerInfo& timer, DWORD timeout);

  void removeHandler(GuiHandler* h);

  void draw(HDC hDC, RECT& windowArea, RECT& drawArea);

  void closeWindow();

  int popupMenu(int x, int y, const vector<pair<wstring, int>>& menuItems) const;

  void setDBErrorState(bool state);
  friend class Table;
  friend gdioutput* createExtraWindow(const string& tag, const wstring& title, int max_x, int max_y, bool fixedSize);

  gdioutput(const string& tag, double _scale);
  gdioutput(double _scale, HWND hWndTarget, const PrinterObject& defprn);
  virtual ~gdioutput();
};

