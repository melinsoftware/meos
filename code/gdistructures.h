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

#ifndef GDI_STRUCTURES
#define GDI_STRUCTURES

#include <cassert>
#include "guihandler.h"
#include "gdifonts.h"

class BaseInfo
{
protected:
  void *extra;
  GuiHandler *handler;
  bool dataString;
public:

  bool hasEventHandler() const {
    return handler != 0;
  }

  bool handleEvent(gdioutput &gdi, GuiEventType type) {
    if (handler) {
      handler->handle(gdi, *this, type);
      return true;
    }
    return false;
  }

  BaseInfo():extra(0), dataString(false), handler(0) {}
  virtual ~BaseInfo() {}
  string id;

  virtual HWND getControlWindow() const = 0;

  virtual void refresh() {
    InvalidateRect(getControlWindow(), 0, true);
  }

  BaseInfo &setExtra(const wchar_t *e) {extra=(void *)e; dataString = true; return *this;}

  BaseInfo &setExtra(int e) {extra = (void *)(e); return *this;}
  BaseInfo &setExtra(size_t e) {extra = (void *)(e); return *this;}

  bool isExtraString() const {return dataString;}
  wchar_t *getExtra() const {assert(extra == 0 || dataString); return (wchar_t *)extra;}
  int getExtraInt() const {return int(extra);}
  size_t getExtraSize() const {return size_t(extra);}

  GuiHandler &getHandler() const;
  BaseInfo &setHandler(const GuiHandler *h) {handler = const_cast<GuiHandler *>(h); return *this;}

};

class RestoreInfo : public BaseInfo
{
public:
  int nLBI;
  int nBI;
  int nII;
  int nTL;
  int nRect;
  int nHWND;
  int nData;

  int sCX;
  int sCY;
  int sMX;
  int sMY;
  int sOX;
  int sOY;

  int nTooltip;
  int nTables;

  GUICALLBACK onClear;
  GUICALLBACK postClear;

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};

class RectangleInfo : public BaseInfo
{
private:
  DWORD color;
  DWORD color2;
  bool drawBorder;
  DWORD borderColor;
  RECT rc;
 
  bool border3D;
public:
  const RECT &getRect()  const {return rc;}
  RectangleInfo(): color(0), color2(0), borderColor(0), border3D(false), drawBorder(false) {memset(&rc, 0, sizeof(RECT));}
  RectangleInfo &setColor(GDICOLOR c) {color = c; return *this;}
  RectangleInfo &setColor2(GDICOLOR c) {color2 = c; return *this;}
  RectangleInfo &set3D(bool is3d) {border3D = is3d; return *this;}
  RectangleInfo &setBorderColor(GDICOLOR c) {borderColor = c; return *this;}
  friend class gdioutput;
  friend struct PageInfo;

  RectangleInfo &changeDimension(gdioutput &gdi, int dx, int dy); 

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};


class TableInfo : public BaseInfo
{
public:
  TableInfo():xp(0), yp(0), table(0) {}
  int xp;
  int yp;
  Table *table;

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};


class TextInfo : public BaseInfo
{
public:

  TextInfo():format(0), color(0), xlimit(0), hasTimer(false),
    hasCapture(false), callBack(0), highlight(false),
    active(false), lineBreakPrioity(0),
    absPrintX(0), absPrintY(0), realWidth(0) {
      textRect.left = 0; textRect.right = 0;
      textRect.top = 0; textRect.bottom = 0;
  }

  TextInfo &setColor(GDICOLOR c) {color = c; return *this;}
  GDICOLOR getColor() const { return GDICOLOR(color); }

  TextInfo &changeFont(const wstring &fnt) {font = fnt; return *this;} //Note: size not updated

  bool isFormatInfo() const { return format == pageNewPage || format == pagePageInfo || format == pageNewChapter; }

  int getHeight() {return int(textRect.bottom-textRect.top);}
  gdiFonts getGdiFont() const {return gdiFonts(format & 0xFF);}
  // Sets absolute print coordinates in [mm]
  TextInfo &setAbsPrintPos(int x, int y) {
    absPrintX = x; absPrintY = y; return *this;
  }
  wstring text;
  wstring font;

  int xp;
  int yp;

  int format;
  DWORD color;
  int xlimit;
  int lineBreakPrioity;
  int absPrintX;
  int absPrintY;

  bool hasTimer;
  DWORD zeroTime;
  DWORD timeOut;

  bool hasCapture;
  GUICALLBACK callBack;
  RECT textRect;
  int realWidth; // The calculated actual width of the string in pixels
  bool highlight;
  bool active;


  HWND getControlWindow() const {throw std::exception("Unsupported");}

  friend class gdioutput;
};

class ButtonInfo : public BaseInfo
{
private:
  bool originalState;
  bool isEditControl;
  bool checked;
  bool *updateLastData;
  void synchData() const {if (updateLastData) *updateLastData = checked;}
  
public:
  ButtonInfo(): callBack(0), hWnd(0), AbsPos(false), fixedRightTop(false),
            flags(0), storedFlags(0), originalState(false), isEditControl(false),
            isCheckbox(false), checked(false), updateLastData(0) {}

  ButtonInfo &isEdit(bool e) {isEditControl=e; return *this;}

  int xp;
  int yp;
  int width;
  wstring text;
  HWND hWnd;
  bool AbsPos;
  bool fixedRightTop;
  int flags;
  int storedFlags;
  bool isCheckbox;
  bool isDefaultButton() const {return (flags&1)==1;}
  bool isCancelButton() const {return (flags&2)==2;}

  ButtonInfo &setSynchData(bool *variable) {updateLastData = variable; return *this;}


  void moveButton(gdioutput &gdi, int xp, int yp);
  void getDimension(gdioutput &gdi, int &w, int &h);

  ButtonInfo &setDefault();
  ButtonInfo &setCancel() {flags|=2, storedFlags|=2; return *this;}
  ButtonInfo &fixedCorner() {fixedRightTop = true; return *this;}
  GUICALLBACK callBack;
  friend class gdioutput;

  HWND getControlWindow() const {return hWnd;}
};

enum gdiFonts;
class InputInfo : public BaseInfo
{
public:
  InputInfo();
  wstring text;

  bool changed() const {return text!=original;}
  void ignore(bool ig) {ignoreCheck=ig;}
  InputInfo &isEdit(bool e) {isEditControl=e; return *this;}
  InputInfo &setBgColor(GDICOLOR c) {bgColor = c; return *this;}
  InputInfo &setFgColor(GDICOLOR c) {fgColor = c; return *this;}
  InputInfo &setFont(gdioutput &gdi, gdiFonts font);
  GDICOLOR getBgColor() const {return bgColor;}
  GDICOLOR getFgColor() const {return fgColor;}

  InputInfo &setPassword(bool pwd);
  
  HWND getControlWindow() const {return hWnd;}
  
  InputInfo &setSynchData(wstring *variable) {updateLastData = variable; return *this;}

  int getX() const {return xp;}
  int getY() const {return yp;}
  int getWidth() const {return int(width);}
private:
  HWND hWnd;
  GUICALLBACK callBack;
  void synchData() const {if (updateLastData) *updateLastData = text;}
  wstring *updateLastData;
  int xp;
  int yp;
  double width;
  double height;

  GDICOLOR bgColor;
  GDICOLOR fgColor;
  bool isEditControl;
  bool writeLock;
  wstring original;
  wstring focusText; // Test when got focus
  bool ignoreCheck; // True if changed-state should be ignored
  friend class gdioutput;
};

class ListBoxInfo : public BaseInfo
{
public:
  ListBoxInfo() : hWnd(0), callBack(0), IsCombo(false), index(-1),
              writeLock(false), ignoreCheck(false), isEditControl(true),
              originalProc(0), lbiSync(0), multipleSelection(false), 
              xp(0), yp(0), width(0), height(0), data(0), lastTabStop(0),
              updateLastData(0) {}
  wstring text;
  size_t data;
  int index;
  bool changed() const {return text!=original;}
  void ignore(bool ig) {ignoreCheck=ig;}
  ListBoxInfo &isEdit(bool e) {isEditControl=e; return *this;}
  HWND getControlWindow() const {return hWnd;}

  void copyUserData(ListBoxInfo &userLBI) const;
  ListBoxInfo &setSynchData(int *variable) {updateLastData = variable; return *this;}
  int getWidth() const {return int(width);}
  int getX() const {return xp;}
  int getY() const {return yp;}
  bool isCombo() const {return IsCombo;}
private:
  void syncData() const {if (updateLastData) *updateLastData = data;}
  bool IsCombo;
  int *updateLastData;
  
  GUICALLBACK callBack;
  
  int xp;
  int yp;
  double width;
  double height;
  HWND hWnd;
  int lastTabStop;

  bool multipleSelection;
  bool isEditControl;
  bool writeLock;
  wstring original;
  int originalIdx;
  bool ignoreCheck; // True if changed-state should be ignored

  map<int, int> data2Index;

  // Synchronize with other list box
  WNDPROC originalProc;
  ListBoxInfo *lbiSync;

  friend LRESULT CALLBACK GetMsgProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
  friend class gdioutput;
};

class DataStore
{
public:
  DataStore() {
    data = 0;
  }
  string id;
  void *data;
  string sdata;
};

class EventInfo : public BaseInfo
{
private:
  string origin;
  DWORD data;
  KeyCommandCode keyEvent;
public:
  KeyCommandCode getKeyCommand() const {return keyEvent;}
  DWORD getData() const {return data;}
  void setKeyCommand(KeyCommandCode kc) {keyEvent = kc;}
  void setData(const string &origin_, DWORD d) {origin = origin_, data = d;}
  const string &getOrigin() {return origin;}
  EventInfo();
  GUICALLBACK callBack;

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};

class TimerInfo : public BaseInfo
{
private:
  static int globalTimerId;
  int timerId;
  DWORD dataInt;
  wstring dataString;
  gdioutput *parent;
  TimerInfo(gdioutput *gdi, GUICALLBACK cb) : parent(gdi), callBack(cb), setWnd(0), timerId(++globalTimerId) {}
  HWND setWnd;
public:
  ~TimerInfo();

  int getId() const { return timerId; }
  BaseInfo &setExtra(const wchar_t *e) {return BaseInfo::setExtra(e);}
  BaseInfo &setExtra(int e) {return BaseInfo::setExtra(e);}

  void setData(DWORD d, const wstring &s) {dataInt = d; dataString = s;}
  const wstring &getDataString() {
    return dataString;
  }
  DWORD getData() {
    return dataInt;
  }

  GUICALLBACK callBack;
  friend class gdioutput;
  friend void CALLBACK gdiTimerProc(HWND hWnd, UINT a, UINT_PTR ptr, DWORD b);

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};


class InfoBox : public BaseInfo
{
public:
  InfoBox() : callBack(0), HasCapture(0), HasTCapture(0), TimeOut(0) {}
  wstring text;
  GUICALLBACK callBack;

  RECT TextRect;
  RECT Close;
  RECT BoundingBox;

  bool HasCapture;
  bool HasTCapture;

  DWORD TimeOut;

  HWND getControlWindow() const {throw std::exception("Unsupported");}
};

typedef list<TextInfo> TIList;

struct ToolInfo {
  string name;
  TOOLINFOW ti;
  wstring tip;
  int id;
};


#endif
