#pragma once
#include <vector>
#include "gdioutput.h"

class AutoCompleteHandler;

struct AutoCompleteRecord {
  AutoCompleteRecord() : id(-1) {}
  AutoCompleteRecord(const wstring &display, const wstring &name, int id) : display(display), name(name), id(id) {}
  wstring display;
  wstring name;
  int id;
};

class AutoCompleteInfo {
public: 
  
private:

  AutoCompleteHandler *handler;
  HWND hWnd;
  
  string widgetId;
  bool lock;
  gdioutput &gdi;

  vector<AutoCompleteRecord> data;
  vector<pair<int, RECT>> rendered;
  
  bool modifedAutoComplete; // True if the user has made a selection
  int lastVisible;
  int currentIx;
public:
  AutoCompleteInfo(HWND hWnd, const string &widgetId, gdioutput &gdi);
  ~AutoCompleteInfo();
  bool matchKey(const string &k) const { return widgetId == k; }

  void destoy() {
    if (hWnd)
      DestroyWindow(hWnd);
    hWnd = nullptr;
  }

  void paint(HDC hDC);
  void setData(vector<AutoCompleteRecord> &items);
  void click(int x, int y);
  void show();

  void upDown(int direction);
  void enter();
  
  bool locked() const { return lock; }
  const string &getTarget() const { return widgetId; }
  wstring getCurrent() const { if (size_t(currentIx) < data.size()) return data[currentIx].name; else return L""; }
  int getCurrentInt() const { if (size_t(currentIx) < data.size()) return data[currentIx].id; else return 0; }

  void setAutoCompleteHandler(AutoCompleteHandler *h) { handler = h; };
  static void registerAutoClass();
};

