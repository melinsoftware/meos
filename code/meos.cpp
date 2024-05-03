/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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

// meos.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include <shlobj.h>

#include "oEvent.h"
#include "xmlparser.h"
#include "recorder.h"

#include "gdioutput.h"
#include "commctrl.h"
#include "SportIdent.h"
#include "TabBase.h"
#include "TabCompetition.h"
#include "TabAuto.h"
#include "TabClass.h"
#include "TabCourse.h"
#include "TabControl.h"
#include "TabSI.h"
#include "TabList.h"
#include "TabTeam.h"
#include "TabSpeaker.h"
#include "TabMulti.h"
#include "TabRunner.h"
#include "TabClub.h"
#include "progress.h"
#include "inthashmap.h"
#include <cassert>
#include "localizer.h"
#include "intkeymap.hpp"
#include "intkeymapimpl.hpp"
#include "download.h"
#include "meos_util.h"
#include <sys/stat.h>
#include "random.h"
#include "metalist.h"
#include "gdiconstants.h"
#include "socket.h"
#include "autotask.h"
#include "meosexception.h"
#include "parser.h"
#include "restserver.h"
#include "autocomplete.h"
#include "image.h"
#include "csvparser.h"
#include "generalresult.h"

int defaultCodePage = 1252;

Image image;
gdioutput *gdi_main=0;
oEvent *gEvent=0;
SportIdent *gSI=0;
Localizer lang;
AutoTask *autoTask = 0;
#ifdef _DEBUG
  bool enableTests = true;
#else
  bool enableTests = false;
#endif

vector<gdioutput *> gdi_extra;
gdioutput* getGDIForExtraWindow(HWND hWnd);

void initMySQLCriticalSection(bool init);

HWND hWndMain;
HWND hWndWorkspace;

#define MAX_LOADSTRING 100

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

void removeTempFiles();
void Setup(bool overwrite, bool overwriteAll);

// Global Variables:
HINSTANCE hInst; // current instance
TCHAR szTitle[MAX_LOADSTRING]; // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING]; // The title bar text
TCHAR szWorkSpaceClass[MAX_LOADSTRING]; // The title bar text

// Foward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WorkSpaceWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
void registerToolbar(HINSTANCE hInstance);
extern const wchar_t *szToolClass;

HHOOK g_hhk; //- handle to the hook procedure.

HWND hMainTab = nullptr;
HIMAGELIST imageList = nullptr;

list<TabObject> *tabList = nullptr;
void scrollVertical(gdioutput *gdi, int yInc, HWND hWnd);
static size_t currentFocusIx = 0;

void resetSaveTimer() {
  if (autoTask)
    autoTask->resetSaveTimer();
}

void dumpLeaks() {
  _CrtDumpMemoryLeaks();
}

void LoadPage(gdioutput &gdi, TabType type) {
  gdi.setWaitCursor(true);
  TabBase *t = gdi.getTabs().get(type);
  if (t)
    t->loadPage(gdi);
  gdi.setWaitCursor(false);
}

// Path to settings file
static wchar_t settings[260];
// Startup path
wchar_t programPath[MAX_PATH];
// Exe path
wchar_t exePath[MAX_PATH];


void mainMessageLoop(HACCEL hAccelTable, DWORD time) {
  MSG msg;
  BOOL bRet;
  
  if (time > 0) {
    time += GetTickCount();
  }
  // Main message loop:
  while ( (bRet = GetMessage(&msg, NULL, 0, 0)) != 0 ) {
    if (bRet == -1)
      return;
    if (gEvent != 0)
      RestServer::computeRequested(*gEvent);

    if (hAccelTable == 0 || !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (time != 0) {
      if (GetTickCount() > time)
        return;
    }
  }
}

INT_PTR CALLBACK splashDialogProc(
  _In_ HWND   hwndDlg,
  _In_ UINT   uMsg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
);

int APIENTRY WinMain(HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nCmdShow)
{
  hInst = hInstance; // Store instance handle in our global variable
  
  atexit(dumpLeaks);	//
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

  if (strstr(lpCmdLine, "-s") != 0) {
    Setup(true, false);
    exit(0);
  }
  else if (strstr(lpCmdLine, "-test") != 0) {
    enableTests = true;
  }

  HWND hSplash = nullptr;
  if (strstr(lpCmdLine, "-nosplash") == 0) {
    hSplash = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_SPLASH), nullptr, splashDialogProc);
    ShowWindow(hSplash, SW_SHOW);
    UpdateWindow(hSplash);
  }
  DWORD splashStart = GetTickCount();

  for (int k = 0; k < 100; k++) {
    RunnerStatusOrderMap[k] = 0;
  }
  RunnerStatusOrderMap[StatusOK] = 0;
  RunnerStatusOrderMap[StatusNoTiming] = 1;
  RunnerStatusOrderMap[StatusOutOfCompetition] = 2;

  RunnerStatusOrderMap[StatusMAX] = 3;
  RunnerStatusOrderMap[StatusMP] = 4;
  RunnerStatusOrderMap[StatusDNF] = 5;
  RunnerStatusOrderMap[StatusDQ] = 6;
  RunnerStatusOrderMap[StatusCANCEL] = 7;
  RunnerStatusOrderMap[StatusDNS] = 8;
  RunnerStatusOrderMap[StatusUnknown] = 9;
  RunnerStatusOrderMap[StatusNotCompetiting] = 10;

  lang.init();
  StringCache::getInstance().init();
  
  for (RunnerStatus st : getAllRunnerStatus()) {
    if (st != StatusOK)
      assert(RunnerStatusOrderMap[st] > 0);
    oAbstractRunner::encodeStatus(st);
  }

  GetCurrentDirectory(MAX_PATH, programPath);
  bool utfRecode = false;
  if (utfRecode) {
    vector<wstring> dyn;

    expandDirectory(L".", L"*.cpp", dyn);
    expandDirectory(L".", L"*.h", dyn);
    expandDirectory(L"./meosdb", L"*.cpp", dyn);
    expandDirectory(L"./meosdb", L"*.h", dyn);
    for (auto &f : dyn)
      csvparser::convertUTF(f);
  }
  
  GetModuleFileName(NULL, exePath, MAX_PATH);
  int lastDiv = -1;
  for (int i = 0; i < MAX_PATH; i++) {
    if (exePath[i] == 0)
      break;
    if (exePath[i] == '\\' || exePath[i] == '/')
      lastDiv = i;
  }
  if (lastDiv != -1)
    exePath[lastDiv] = 0;
  else
    exePath[0] = 0;

  getUserFile(settings, L"meoswpref.xml");
  Parser::test();

  int rInit = (GetTickCount() / 100);
  InitRanom(rInit, rInit/379);

  tabList=new list<TabObject>;

  HACCEL hAccelTable;

  gdi_main = new gdioutput("main", 1.0);
  gdi_extra.push_back(gdi_main);

  try {
    gEvent = new oEvent(*gdi_main);
    gEvent->setMainEvent();
  }
  catch (meosException &ex) {
    gdi_main->alert(wstring(L"Failed to create base event: ") + ex.wwhat());
    return 0;
  }
  catch (std::exception &ex) {
    gdi_main->alert(string("Failed to create base event: ") + ex.what());
    return 0;
  }

  if (fileExists(settings)) {
    gEvent->loadProperties(settings);
  }
  else {
    wchar_t oldSettings[260];  
    // Read from older version
    getUserFile(oldSettings, L"meospref.xml");
    gEvent->loadProperties(oldSettings);
  }
  gEvent->clear();

  lang.get().addLangResource(L"English", L"104");
  lang.get().addLangResource(L"Svenska", L"103");
  lang.get().addLangResource(L"Deutsch", L"105");
  lang.get().addLangResource(L"Dansk", L"106");
  lang.get().addLangResource(L"Český", L"108");
  lang.get().addLangResource(L"Français", L"110");
  lang.get().addLangResource(L"Español", L"111");
  lang.get().addLangResource(L"Russian", L"107");
  lang.get().addLangResource(L"Ukrainian", L"112");
  lang.get().addLangResource(L"Portuguese", L"113");

  if (fileExists(L"extra.lng")) {
    lang.get().addLangResource(L"Extraspråk", L"extra.lng");
  }
  else {
    wchar_t lpath[260];
    getUserFile(lpath, L"extra.lng");
    if (fileExists(lpath))
      lang.get().addLangResource(L"Extraspråk", lpath);
  }

  wstring defLang = gEvent->getPropertyString("Language", L"Svenska");

  defaultCodePage = gEvent->getPropertyInt("CodePage", 1252);

  // Backward compatibility
  if (defLang==L"103")
    defLang = L"Svenska";
  else if (defLang==L"104")
    defLang = L"English";

  gEvent->setProperty("Language", defLang);

  try {
    lang.get().loadLangResource(defLang);
  }
  catch (std::exception &) {
    lang.get().loadLangResource(L"Svenska");
  }

  try {
    vector<wstring> res;
#ifdef _DEBUG
    expandDirectory(L".\\..\\Lists\\", L"*.lxml", res);
    expandDirectory(L".\\..\\Lists\\", L"*.listdef", res);
#endif
    
    if (exePath[0]) {
      expandDirectory(exePath, L"*.lxml", res);
      expandDirectory(exePath, L"*.listdef", res);
    }

    expandDirectory(programPath, L"*.lxml", res);
    expandDirectory(programPath, L"*.listdef", res);

    wchar_t listpath[MAX_PATH];
    getUserFile(listpath, L"");
    expandDirectory(listpath, L"*.lxml", res);
    expandDirectory(listpath, L"*.listdef", res);

    wstring err;
    set<wstring> processed;
    for (size_t k = 0; k<res.size(); k++) {
      try {

        wchar_t filename[128];
        wchar_t ext[32];
        _wsplitpath_s(res[k].c_str(), NULL, 0, NULL, 0, filename, 128, ext, 32);
        wstring fullFile = wstring(filename) + ext;
        if (processed.count(fullFile))
          continue;
        processed.insert(fullFile);
        xmlparser xml;

        wcscpy_s(listpath, res[k].c_str());
        xml.read(listpath);

        xmlobject xlist = xml.getObject(0);
        gEvent->getListContainer().load(MetaListContainer::InternalList, xlist, true);
      }
      catch (meosException &ex) {
        wstring errLoc = L"Kunde inte ladda X\n\n(Y)#" + wstring(listpath) + L"#" + lang.tl(ex.wwhat());
        if (err.empty())
          err = lang.tl(errLoc);
        else
          err += L"\n" + lang.tl(errLoc);
      }
      catch (std::exception &ex) {
        wstring errLoc = L"Kunde inte ladda X\n\n(Y)#" + wstring(listpath) + L"#" + lang.tl(ex.what());
        if (err.empty())
          err = lang.tl(errLoc);
        else
          err += L"\n" + lang.tl(errLoc);
      }
    }
    if (!err.empty())
      gdi_main->alert(err);
  }
  catch (meosException &ex) {
    gdi_main->alert(ex.wwhat());
  }
  catch (std::exception &ex) {
    gdi_main->alert(ex.what());
  }

  gEvent->openRunnerDatabase(L"database");
  wcscpy_s(szTitle, L"MeOS");
  wcscpy_s(szWindowClass, L"MeosMainClass");
  wcscpy_s(szWorkSpaceClass, L"MeosWorkSpace");
  MyRegisterClass(hInstance);
  registerToolbar(hInstance);

  int defSize = 0;
  {
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    // Get the size of screen to the variable desktop
    GetWindowRect(hDesktop, &desktop);
    int d = max(desktop.right, desktop.bottom);
    if (d <= 1024)
      defSize = 0;
    else if (d <= 2000)
      defSize = 1;
    else if (d <= 2500)
      defSize = 2;
    else
      defSize = 3;
  }
  gdi_main->setFont(gEvent->getPropertyInt("TextSize", defSize),
                    gEvent->getPropertyString("TextFont", L"Arial"));

  if (hSplash != nullptr) {
    DWORD startupToc = GetTickCount() - splashStart;
    Sleep(min<int>(1000, max<int>(0, 700 - startupToc)));
  }

  // Perform application initialization:
  if (!InitInstance (hInstance, nCmdShow)) {
    return FALSE;
  }

  RECT rc;
  GetClientRect(hWndMain, &rc);
  SendMessage(hWndMain, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));

  gdi_main->init(hWndWorkspace, hWndMain, hMainTab);
  gdi_main->getTabs().get(TCmpTab)->loadPage(*gdi_main);

  image.loadImage(IDI_MEOSEDIT, Image::ImageMethod::Default);

  autoTask = new AutoTask(hWndMain, *gEvent, *gdi_main);

  autoTask->setTimers();

  // Install a hook procedure to monitor the message stream for mouse
  // messages intended for the controls in the dialog box.
  g_hhk = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc,
      (HINSTANCE) NULL, GetCurrentThreadId());

  hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_MEOS);
    
  DestroyWindow(hSplash);
  
  initMySQLCriticalSection(true);
  // Main message loop:
  mainMessageLoop(hAccelTable, 0);

  TabAuto::tabAutoRegister(nullptr);
  tabList->clear();
  delete tabList;
  tabList = nullptr;

  delete autoTask;
  autoTask = 0;

  for (size_t k = 0; k<gdi_extra.size(); k++) {
    if (gdi_extra[k]) {
      DestroyWindow(gdi_extra[k]->getHWNDMain());
      if (k < gdi_extra.size()) {
        delete gdi_extra[k];
        gdi_extra[k] = 0;
      }
    }
  }
  gdi_main = nullptr;
  gdi_extra.clear();

  if (gEvent)
    gEvent->saveProperties(settings);

  delete gEvent;
  gEvent = nullptr;

  initMySQLCriticalSection(false);

  removeTempFiles();

  #ifdef _DEBUG
    lang.get().debugDump(L"untranslated.txt", L"translated.txt");
  #endif

  StringCache::getInstance().clear();
  lang.unload();

  return 0;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style			= CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc	= (WNDPROC)WndProc;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= 0;
  wcex.hInstance		= hInstance;
  wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_MEOS);
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
  wcex.lpszMenuName	= 0;//(LPCSTR)IDC_MEOS;
  wcex.lpszClassName	= szWindowClass;
  wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

  RegisterClassEx(&wcex);

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wcex.lpfnWndProc	= (WNDPROC)WorkSpaceWndProc;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= 0;
  wcex.hInstance		= hInstance;
  wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_MEOS);
  wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground	= 0;
  wcex.lpszMenuName	= 0;
  wcex.lpszClassName	= szWorkSpaceClass;
  wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);
  RegisterClassEx(&wcex);

  AutoCompleteInfo::registerAutoClass();

  return true;
}


// GetMsgProc - monitors the message stream for mouse messages intended
//     for a control window in the dialog box.
// Returns a message-dependent value.
// nCode - hook code.
// wParam - message flag (not used).
// lParam - address of an MSG structure.
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam) {
  MSG* lpmsg = (MSG*)lParam;
  gdioutput* extra = nullptr;


  switch (lpmsg->message) {
  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    if (nCode >= 0) {
      if (IsChild(hWndWorkspace, lpmsg->hwnd))
        extra = gdi_main;
      else
        extra = getGDIForExtraWindow(lpmsg->hwnd); 
    }

    if (extra != nullptr && extra->getToolTip() != nullptr) {
      MSG msg;
      msg.lParam = lpmsg->lParam;
      msg.wParam = lpmsg->wParam;
      msg.message = lpmsg->message;
      msg.hwnd = lpmsg->hwnd;
      SendMessage(extra->getToolTip(), TTM_RELAYEVENT, 0,
        (LPARAM)(LPMSG)&msg);
    }
  }
  return CallNextHookEx(g_hhk, nCode, wParam, lParam);
}

void flushEvent(const string &id, const string &origin, DWORD data, int extraData)
{
  for (size_t k = 0; k<gdi_extra.size(); k++) {
    if (gdi_extra[k]) {
      gdi_extra[k]->makeEvent(id, origin, data, extraData, false);
    }
  }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (code<0)
    return CallNextHookEx(0, code, wParam, lParam);

  gdioutput *gdi = 0;

  if (size_t(currentFocusIx) < gdi_extra.size())
    gdi = gdi_extra[currentFocusIx];

  if (!gdi)
    gdi = gdi_main;


  HWND hWnd = gdi ? gdi->getHWNDTarget() : 0;

  bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) == 0x8000;
  bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) == 0x8000;

  //if (code<0) return CallNextHookEx(
  if (wParam==VK_TAB) {
    if ( (lParam& (1<<31))) {
      SHORT state=GetKeyState(VK_SHIFT);
      if (gdi) {
        if (state&(1<<16))
          gdi->TabFocus(-1);
        else
          gdi->TabFocus(1);
      }
    }
    return 1;
  }
  else if (wParam==VK_RETURN && (lParam & (1<<31))) {
    if (gdi)
      gdi->enter();
  }
  else if (wParam==VK_UP) {
    bool c = false;
    if (gdi  && (lParam & (1<<31)))
      c = gdi->upDown(1);

    if (gdi && gdi->hasAutoComplete())
      return 1;

    if (!c  && !(lParam & (1<<31)) && !(gdi && gdi->lockUpDown))
      SendMessage(hWnd, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0);
  }
  else if (wParam == VK_NEXT && !(lParam & (1<<31)) && !(gdi && gdi->lockUpDown)) {
    SendMessage(hWnd, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), 0);
  }
  else if (wParam == VK_PRIOR && !(lParam & (1<<31)) && !(gdi && gdi->lockUpDown)) {
    SendMessage(hWnd, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), 0);
  }
  else if (wParam==VK_DOWN) {
    bool c = false;
    if (gdi && (lParam & (1<<31)))
      c = gdi->upDown(-1);

    if (gdi && gdi->hasAutoComplete())
      return 1;

    if (!c && !(lParam & (1<<31)) && !(gdi && gdi->lockUpDown))
      SendMessage(hWnd, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0);
  }
  else if (wParam==VK_LEFT && !(lParam & (1<<31))) {
    if (!gdi || !gdi->hasEditControl())
      SendMessage(hWnd, WM_HSCROLL, MAKELONG(SB_LINEUP, 0), 0);
  }
  else if (wParam==VK_RIGHT && !(lParam & (1<<31))) {
    if (!gdi || !gdi->hasEditControl())
      SendMessage(hWnd, WM_HSCROLL, MAKELONG(SB_LINEDOWN, 0), 0);
  }
  else if (wParam==VK_ESCAPE && (lParam & (1<<31))) {
    if (gdi)
      gdi->escape();
  }
  else if (wParam==VK_F2) {
    ProgressWindow pw(hWnd);

    pw.init();
    for (int k=0;k<=20;k++) {
      pw.setProgress(k*50);
      Sleep(100);
    }
    //pw.draw();
  }
  else if (ctrlPressed && (wParam == VK_ADD || wParam == VK_SUBTRACT ||
           wParam == VK_F5 || wParam == VK_F6)) {
    if (gdi) {
      if (wParam == VK_ADD || wParam == VK_F5)
        gdi->scaleSize(1.1);
      else
        gdi->scaleSize(1.0/1.1);
    }
  }
  else if (wParam == 'C' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_COPY);
  }
  else if (wParam == 'V'  && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_PASTE);
  }
  else if (wParam == 'F'  && ctrlPressed) {
    if (gdi) {
      if (!shiftPressed)
        gdi->keyCommand(KC_FIND);
      else
        gdi->keyCommand(KC_FINDBACK);
    }
  }
  else if (wParam == 'A' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_MARKALL);
  }
  else if (wParam == 'D' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_CLEARALL);
  }
  else if (wParam == VK_DELETE) {
    if (gdi)
      gdi->keyCommand(KC_DELETE);
  }
  else if (wParam == 'I' &&  ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_INSERT);
  }
  else if (wParam == 'P' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_PRINT);
  }
  else if (wParam == VK_F5 && !ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_REFRESH);
  }
  else if (wParam == 'M' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_SPEEDUP);
  }
  else if (wParam == 'N' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_SLOWDOWN);
  }
  else if (wParam == ' ' && ctrlPressed) {
    if (gdi)
      gdi->keyCommand(KC_AUTOCOMPLETE);
  }

  return 0;
}
//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  HWND hWnd;

  HWND hDskTop=GetDesktopWindow();
  RECT rc;
  GetClientRect(hDskTop, &rc);

  int xp = gEvent->getPropertyInt("xpos", 50);
  int yp = gEvent->getPropertyInt("ypos", 20);

  int xs = gEvent->getPropertyInt("xsize", max(850, min<int>(int(rc.right)-yp, (rc.right*9)/10)));
  int ys = gEvent->getPropertyInt("ysize", max(650, min<int>(int(rc.bottom)-yp-40, (rc.bottom*8)/10)));

  if ((xp + xs > rc.right) || xp < rc.left || yp + ys > rc.bottom || yp < rc.top) {
    // out of bounds, just use default position and size
    xp = 50;
    yp = 20;
    xs = max(850, min<int>(int(rc.right) - yp, (rc.right * 9) / 10));
    ys = max(650, min<int>(int(rc.bottom) - yp - 40, (rc.bottom * 8) / 10));
  }

  gEvent->setProperty("ypos", yp + 16);
  gEvent->setProperty("xpos", xp + 32);
  gEvent->saveProperties(settings); // For other instance starting while running
  gEvent->setProperty("ypos", yp);
  gEvent->setProperty("xpos", xp);

  hWnd = CreateWindowEx(0, szWindowClass, szTitle,
          WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
          xp, yp, max(min(int(rc.right)-yp, xs), 200),
                  max(min(int(rc.bottom)-yp-40, ys), 100),
          NULL, NULL, hInstance, NULL);

  if (!hWnd)
    return FALSE;

  hWndMain = hWnd;

  SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, 0, GetCurrentThreadId());
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  hWnd = CreateWindowEx(0, szWorkSpaceClass, L"WorkSpace", WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
    50, 200, 200, 100, hWndMain, NULL, hInstance, NULL);

  if (!hWnd)
    return FALSE;

  hWndWorkspace=hWnd;
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return TRUE;
}

void destroyExtraWindows() {
  for (size_t k = 1; k<gdi_extra.size(); k++) {
    if (gdi_extra[k]) {
      DestroyWindow(gdi_extra[k]->getHWNDMain());
    }
  }
}

string uniqueTag(const char *base) {
  int j = 0;
  string b = base;
  while(true) {
    string tag = b + itos(j++);
    if (getExtraWindow(tag, false) == 0)
      return tag;
  }
}

vector<string> getExtraWindows() {
  vector<string> res;
  for (size_t k = 0; k < gdi_extra.size(); k++) {
    if (gdi_extra[k])
      res.push_back(gdi_extra[k]->getTag());
  }
  return res;
}

gdioutput *getExtraWindow(const string &tag, bool toForeGround) {
  for (size_t k = 0; k<gdi_extra.size(); k++) {
    if (gdi_extra[k] && gdi_extra[k]->hasTag(tag)) {
      if (toForeGround)
        SetForegroundWindow(gdi_extra[k]->getHWNDMain());
      return gdi_extra[k];
    }
  }
  return 0;
}

gdioutput* getGDIForExtraWindow(HWND hWnd) {
  for (size_t k = 1; k < gdi_extra.size(); k++) {
    if (gdi_extra[k] && IsChild(gdi_extra[k]->getHWNDTarget(), hWnd)) {
      return gdi_extra[k];
    }
  }
  return nullptr;
}

gdioutput *createExtraWindow(const string &tag, const wstring &title, int max_x, int max_y, bool fixedSize) {
  if (getExtraWindow(tag, false) != 0)
    throw meosException("Window already exists");

  HWND hWnd;


  HWND hDskTop=GetDesktopWindow();
  RECT rc;
  GetClientRect(hDskTop, &rc);

  int xp = gEvent->getPropertyInt("xpos", 50) + 16;
  int yp = gEvent->getPropertyInt("ypos", 20) + 32;

  for (size_t k = 0; k<gdi_extra.size(); k++) {
    if (gdi_extra[k]) {
      HWND hWnd = gdi_extra[k]->getHWNDTarget();
      RECT rcc;
      if (GetWindowRect(hWnd, &rcc)) {
        xp = max<int>(rcc.left + 16, xp);
        yp = max<int>(rcc.top + 32, yp);
      }
    }
  }

  if (xp > rc.right - 100)
    xp = rc.right - 100;

  if (yp > rc.bottom - 100)
    yp = rc.bottom - 100;

  int xs = max_x, ys = max_y;
  if (!fixedSize) {
    xs = gEvent->getPropertyInt("xsize", max(850, min(int(rc.right) - yp, 1124)));
    ys = gEvent->getPropertyInt("ysize", max(650, min(int(rc.bottom) - yp - 40, 800)));

    if (max_x > 0)
      xs = min(max_x, xs);
    if (max_y > 0)
      ys = min(max_y, ys);
  }
  
  hWnd = CreateWindowEx(0, szWorkSpaceClass, title.c_str(),
    WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
    xp, yp, max(xs, 200), max(ys, 100), 0, NULL, hInst, NULL);

  if (!hWnd)
    return 0;

  ShowWindow(hWnd, SW_SHOWNORMAL);
  UpdateWindow(hWnd);
  gdioutput *gdi = new gdioutput(tag, 1.0);
  gdi->setFont(gEvent->getPropertyInt("TextSize", 0),
               gEvent->getPropertyString("TextFont", L"Arial"));

  gdi->init(hWnd, hWnd, 0);
  gdi->isTestMode = gdi_main->isTestMode;
  if (gdi->isTestMode) {
    if (!gdi_main->cmdAnswers.empty()) {
      gdi->dbPushDialogAnswer(gdi_main->cmdAnswers.front());
      gdi_main->cmdAnswers.pop_front();
    }
  }
  else {
    gdi->initRecorder(&gdi_main->getRecorder());
  }
  SetWindowLongPtr(hWnd, GWLP_USERDATA, gdi_extra.size());
  currentFocusIx = gdi_extra.size();
  gdi_extra.push_back(gdi);

  return gdi;
}

/** Returns the tag of the last extra window. */
const string &getLastExtraWindow() {
  if (gdi_extra.empty())
    throw meosException("Empty");
  else
    return gdi_extra.back()->getTag();
}

void InsertSICard(gdioutput &gdi, SICard &sic);

void createTabs(bool force, bool onlyMain, bool skipTeam, bool skipSpeaker,
                bool skipEconomy, bool skipLists,
                bool skipRunners, bool skipCourses, bool skipControls)
{
  static bool onlyMainP = false;
  static bool skipTeamP = false;
  static bool skipSpeakerP = false;
  static bool skipEconomyP = false;
  static bool skipListsP = false;
  static bool skipRunnersP = false;
  static bool skipCoursesP = false;
  static bool skipControlsP = false;

  if (!force && onlyMain==onlyMainP && skipTeam==skipTeamP && skipSpeaker==skipSpeakerP &&
      skipEconomy==skipEconomyP && skipLists==skipListsP &&
      skipRunners==skipRunnersP && skipControls==skipControlsP && skipCourses == skipCoursesP)
    return;

  if (!tabList)
    return;

  onlyMainP = onlyMain;
  skipTeamP = skipTeam;
  skipSpeakerP = skipSpeaker;
  skipEconomyP = skipEconomy;
  skipListsP = skipLists;
  skipRunnersP = skipRunners;
  skipCoursesP = skipCourses;
  skipControlsP = skipControls;

  int oldid=TabCtrl_GetCurSel(hMainTab);
  TabObject *to = 0;
  for (list<TabObject>::iterator it=tabList->begin();it!=tabList->end();++it) {
    if (it->id==oldid) {
      to = &*it;
    }
  }
  if (gdi_main)
    gdi_main->updateTabFont();
  int id = 0;
  TabCtrl_DeleteAllItems(hMainTab);
  for (list<TabObject>::iterator it=tabList->begin();it!=tabList->end();++it) {
    it->setId(-1);

    if (onlyMain && it->getType() != typeid(TabCompetition) && it->getType() != typeid(TabSI))
      continue;

    if (skipTeam && it->getType() == typeid(TabTeam))
      continue;

    if (skipSpeaker && it->getType() == typeid(TabSpeaker))
      continue;

    if (skipEconomy && it->getType() == typeid(TabClub))
      continue;

    if (skipRunners && it->getType() == typeid(TabRunner))
      continue;

    if (skipCourses && it->getType() == typeid(TabCourse))
      continue;

    if (skipControls && it->getType() == typeid(TabControl))
      continue;

    if (skipLists && (it->getType() == typeid(TabList) || it->getType() == typeid(TabAuto)))
      continue;

    TCITEM ti;
    ti.pszText=(LPWSTR)lang.tl(it->name).c_str();
    ti.mask = TCIF_TEXT;
    if (it->imageId >= 0)
      ti.mask |= TCIF_IMAGE;

    ti.iImage = it->imageId;

    it->setId(id++);

    TabCtrl_InsertItem(hMainTab, it->id, &ti);
  }

  if (to && (to->id)>=0)
    TabCtrl_SetCurSel(hMainTab, to->id);
}

void hideTabs() {
  TabCtrl_DeleteAllItems(hMainTab);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;
  PAINTSTRUCT ps;
  HDC hdc;
  static bool connectionAlert = false;

  switch (message)
  {
    case WM_CREATE:

      tabList->emplace_back(gdi_main->getTabs().get(TCmpTab), L"Tävling", 10);
      tabList->emplace_back(gdi_main->getTabs().get(TRunnerTab), L"Deltagare", 6);
      tabList->emplace_back(gdi_main->getTabs().get(TTeamTab), L"Lag(flera)", 7);
      tabList->emplace_back(gdi_main->getTabs().get(TListTab), L"Listor", 5);
      {
        TabAuto *ta = (TabAuto *)gdi_main->getTabs().get(TAutoTab);
        tabList->emplace_back(ta, L"Automater", 4);
        TabAuto::tabAutoRegister(ta);
      }
      tabList->emplace_back(gdi_main->getTabs().get(TSpeakerTab), L"Speaker", 3);
      tabList->emplace_back(gdi_main->getTabs().get(TClassTab), L"Klasser", 0);
      tabList->emplace_back(gdi_main->getTabs().get(TCourseTab), L"Banor", 1);
      tabList->emplace_back(gdi_main->getTabs().get(TControlTab), L"Kontroller", 2);
      tabList->emplace_back(gdi_main->getTabs().get(TClubTab), L"Klubbar", 8);

      tabList->emplace_back(gdi_main->getTabs().get(TSITab), L"SportIdent", 9);

      INITCOMMONCONTROLSEX ic;

      ic.dwSize=sizeof(ic);
      ic.dwICC=ICC_TAB_CLASSES ;
      InitCommonControlsEx(&ic);
      hMainTab = CreateWindowEx(0, WC_TABCONTROL, L"tabs", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 300, 20, hWnd, 0, hInst, 0);
      imageList = ImageList_Create(24, 24, ILC_COLOR32, 1, 1);
      {
        
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCLASS24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCOURSE24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCONTROL24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSANNOUNCER24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSSERVICES24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSLIST24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSRUNNER24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSTEAM24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCLUBS24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCARD24, Image::ImageMethod::Default), nullptr);
        ImageList_Add(imageList, image.loadImage(IDI_MEOSCOMPETITION24, Image::ImageMethod::Default), nullptr);

        TabCtrl_SetImageList(hMainTab, imageList);
      }
      createTabs(true, true, false, false, false, false, false, false, false);
      SetTimer(hWnd, 4, 10000, 0); //Connection check
      break;

    case WM_MOUSEWHEEL: {
      int dz = GET_WHEEL_DELTA_WPARAM(wParam);
      scrollVertical(gdi_main, -dz, hWndWorkspace);
      }
      break;

    case WM_SIZE: {
      int h = 30;
      if (gdi_main) {
        h = max(h, gdi_main->getFontHeight(0, L"") + gdi_main->scaleLength(6));
      }

      MoveWindow(hMainTab, 0, 0, LOWORD(lParam), h, 1);
      MoveWindow(hWndWorkspace, 0, h, LOWORD(lParam), HIWORD(lParam) - h, 1);

      RECT rc;
      GetClientRect(hWndWorkspace, &rc);
      PostMessage(hWndWorkspace, WM_SIZE, wParam, MAKELONG(rc.right, rc.bottom));
    }
      break;

    case WM_WINDOWPOSCHANGED:
      if (gEvent) {
        LPWINDOWPOS wp = (LPWINDOWPOS) lParam; // points to size and position data

        if (wp->x>=0 && wp->y>=0 && wp->cx>300 && wp->cy>200) {
          gEvent->setProperty("xpos", wp->x);
          gEvent->setProperty("ypos", wp->y);
          gEvent->setProperty("xsize", wp->cx);
          gEvent->setProperty("ysize", wp->cy);
        }
        else {
          Sleep(0);
        }
      }
      return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_TIMER:

      if (!gdi_main) return 0;

      if (wParam==1) {
        if (autoTask)
          autoTask->autoSave();
      }
      else if (wParam==2) {
        // Interface timeouts (no synch)
        if (autoTask)
          autoTask->interfaceTimeout(gdi_extra);
      }
      else if (wParam==3) {
        if (autoTask)
          autoTask->synchronize(gdi_extra);
      }
      else if (wParam==4) {
        // Verify database link
        if (gEvent)
          gEvent->verifyConnection();
        //OutputDebugString("Verify link\n");
        //Sleep(0);
        if (gdi_main) {
          if (gEvent->hasClientChanged()) {
            gdi_main->makeEvent("Connections", "verify_connection", 0, 0, false);
            gEvent->validateClients();
          }
        }
      }
      break;

    case WM_ACTIVATE:
      if (LOWORD(wParam) != WA_INACTIVE)
        currentFocusIx = 0;
      return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_NCACTIVATE:
      if (gdi_main && gdi_main->hasToolbar())
        gdi_main->activateToolbar(wParam != 0);
      return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_NOTIFY:
    {
      LPNMHDR pnmh = (LPNMHDR) lParam;

      if (pnmh->hwndFrom==hMainTab && gdi_main && gEvent)
      {
        if (pnmh->code==TCN_SELCHANGE)
        {
          int id=TabCtrl_GetCurSel(hMainTab);

          for (list<TabObject>::iterator it=tabList->begin();it!=tabList->end();++it) {
            if (it->id==id) {
              try {
                gdi_main->setWaitCursor(true);
                string cmd = "showTab("+ string(it->getTab().getTypeStr()) + "); //" + gdi_main->toUTF8(it->name);
                it->loadPage(*gdi_main);
                gdi_main->getRecorder().record(cmd);
              }
              catch (meosException &ex) {
                gdi_main->alert(ex.wwhat());
              }
              catch(std::exception &ex) {
                gdi_main->alert(ex.what());
              }
              gdi_main->setWaitCursor(false);
            }
          }
        }
        else if (pnmh->code==TCN_SELCHANGING) {
          if (gdi_main == 0) {
            MessageBeep(-1);
            return true;
          }
          else {
            if (!gdi_main->canClear())
              return true;

            return false;
          }
        }
      }
      break;
    }

    case WM_USER:
      //The card has been read and posted to a synchronized
      //queue by different thread. Read and process this card.
      {
        SICard sic(ConvertedTimeStatus::Unknown);
        while (gSI && gSI->getCard(sic))
          InsertSICard(*gdi_main, sic);
        break;
      }
    case WM_USER+1:
      MessageBox(hWnd, lang.tl(L"Kommunikationen med en SI-enhet avbröts.").c_str(), L"SportIdent", MB_OK);
      break;

    case WM_USER + 3:
      //OutputDebugString("Get punch from queue\n");
      if (autoTask)
        autoTask->advancePunchInformation(gdi_extra);
      break;

    case WM_USER + 4:
      if (autoTask)
        autoTask->synchronize(gdi_extra);
      break;
    
    case WM_USER + 5:
      if (gdi_main)
        gdi_main->addInfoBox("ainfo", L"info:advanceinfo", 10000);
      break;

    case WM_USER + 6:
      if (gdi_main && lParam) {
        gdioutput* g = (gdioutput*)lParam;
        wstring msg = g->getDelayedAlert();
        if (!connectionAlert) {
          connectionAlert = true;
          gdi_main->alert(msg);
          connectionAlert = false;
        }
      }
      break;

    case WM_COMMAND:
      wmId    = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
        case IDM_EXIT:
           //DestroyWindow(hWnd);
          PostMessage(hWnd, WM_CLOSE, 0,0);
           break;
        default:
           return DefWindowProc(hWnd, message, wParam, lParam);
      }
      break;
    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      // TODO: Add any drawing code here...


      EndPaint(hWnd, &ps);
      break;

    case WM_CLOSE:
      if (!gEvent || gEvent->empty() || gdi_main->ask(L"Vill du verkligen stänga MeOS?"))
          DestroyWindow(hWnd);
      break;

    case WM_DESTROY:
      delete gSI;
      gSI=0;

      for (size_t k = 0; k < gdi_extra.size(); k++) {
        if (gdi_extra[k])
          gdi_extra[k]->clearPage(false, false);
      }

      if (gEvent) {
        try {
          gEvent->save();
        }
        catch (meosException &ex) {
          MessageBox(hWnd, lang.tl(ex.wwhat()).c_str(), L"Fel när tävlingen skulle sparas", MB_OK);
        }
        catch(std::exception &ex) {
          MessageBox(hWnd, lang.tl(ex.what()).c_str(), L"Fel när tävlingen skulle sparas", MB_OK);
        }

        try {
          gEvent->saveRunnerDatabase(L"database", true);
        }
        catch (meosException &ex) {
          MessageBox(hWnd, lang.tl(ex.wwhat()).c_str(), L"Fel när löpardatabas skulle sparas", MB_OK);
        }
        catch(std::exception &ex) {
          MessageBox(hWnd, lang.tl(ex.what()).c_str(), L"Fel när löpardatabas skulle sparas", MB_OK);
        }

        if (gEvent)
          gEvent->saveProperties(settings);

        delete gEvent;
        gEvent=0;
      }

      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

void scrollVertical(gdioutput *gdi, int yInc, HWND hWnd) {
  SCROLLINFO si;
  si.cbSize=sizeof(si);
  si.fMask=SIF_ALL;
  GetScrollInfo(hWnd, SB_VERT, &si);
  if (si.nPage==0)
    yInc = 0;

  int yPos=gdi->getOffsetY();
  int a=si.nMax-signed(si.nPage-1) - yPos;

  if ( (yInc = max( -yPos, min(yInc, a)))!=0 ) {
    yPos += yInc;
    RECT ScrollArea, ClipArea;
    GetClientRect(hWnd, &ScrollArea);
    ClipArea=ScrollArea;

    ScrollArea.top=-gdi->getHeight()-100;
    ScrollArea.bottom+=gdi->getHeight();
    ScrollArea.right=gdi->getWidth()-gdi->getOffsetX()+15;
    ScrollArea.left = -2000;
    gdi->setOffsetY(yPos);

    bool inv = true; //Inv = false works only for lists etc. where there are not controls in the scroll area.

    RECT invalidArea;
    ScrollWindowEx (hWnd, 0,  -yInc,
      &ScrollArea, &ClipArea,
      (HRGN) NULL, &invalidArea, SW_SCROLLCHILDREN | (inv ? SW_INVALIDATE : 0));

   //	gdi->UpdateObjectPositions();

    si.cbSize = sizeof(si);
    si.fMask  = SIF_POS;
    si.nPos   = yPos;

    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

    if (inv)
      UpdateWindow(hWnd);
    else {
      HDC hDC = GetDC(hWnd);
      IntersectClipRect(hDC, invalidArea.left, invalidArea.top, invalidArea.right, invalidArea.bottom);
      gdi->draw(hDC, ScrollArea, invalidArea);
      ReleaseDC(hWnd, hDC);
    }
  }
}

void updateScrollInfo(HWND hWnd, gdioutput &gdi, int nHeight, int nWidth) {
  SCROLLINFO si;
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE|SIF_RANGE;

  int maxx, maxy;
  gdi.clipOffset(nWidth, nHeight, maxx, maxy);

  si.nMin=0;

  if (maxy>0) {
    si.nMax=maxy+nHeight;
    si.nPos=gdi.getOffsetY();
    si.nPage=nHeight;
  }
  else {
    si.nMax=0;
    si.nPos=0;
    si.nPage=0;
  }
  SetScrollInfo(hWnd, SB_VERT, &si, true);

  si.nMin=0;
  if (maxx>0) {
    si.nMax=maxx+nWidth;
    si.nPos=gdi.getOffsetX();
    si.nPage=nWidth;
  }
  else {
    si.nMax=0;
    si.nPos=0;
    si.nPage=0;
  }

  SetScrollInfo(hWnd, SB_HORZ, &si, true);
}

LRESULT CALLBACK WorkSpaceWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;
  PAINTSTRUCT ps;
  HDC hdc;

  LONG_PTR ix = GetWindowLongPtr(hWnd, GWLP_USERDATA);
  gdioutput *gdi = 0;
  if (ix < LONG_PTR(gdi_extra.size()))
    gdi = gdi_extra[ix];

  if (gdi) {
    LRESULT res = gdi->ProcessMsg(message, lParam, wParam);
    if (res)
      return res;
  }
  switch (message)
  {
    case WM_CREATE:
      break;

    case WM_SIZE:
      //SCROLLINFO si;
      //si.cbSize=sizeof(si);
      //si.fMask=SIF_PAGE|SIF_RANGE;

      int nHeight;
      nHeight = HIWORD(lParam);
      int nWidth;
      nWidth = LOWORD(lParam);
      updateScrollInfo(hWnd, *gdi, nHeight, nWidth);
      /*

      int maxx, maxy;
      gdi->clipOffset(nWidth, nHeight, maxx, maxy);

      si.nMin=0;

      if (maxy>0) {
        si.nMax=maxy+nHeight;
        si.nPos=gdi->GetOffsetY();
        si.nPage=nHeight;
      }
      else {
        si.nMax=0;
        si.nPos=0;
        si.nPage=0;
      }
      SetScrollInfo(hWnd, SB_VERT, &si, true);

      si.nMin=0;
      if (maxx>0) {
        si.nMax=maxx+nWidth;
        si.nPos=gdi->GetOffsetX();
        si.nPage=nWidth;
      }
      else {
        si.nMax=0;
        si.nPos=0;
        si.nPage=0;
      }
      SetScrollInfo(hWnd, SB_HORZ, &si, true);
      */
      InvalidateRect(hWnd, NULL, true);
      break;
    case WM_KEYDOWN:
      //gdi->keyCommand(;
      break;

    case WM_VSCROLL:
    {
      int	nScrollCode = (int) LOWORD(wParam); // scroll bar value
      //int hwndScrollBar = (HWND) lParam;      // handle to scroll bar

      int yInc;
      int yPos=gdi->getOffsetY();
      RECT rc;
      GetClientRect(hWnd, &rc);
      int pagestep = max(50, int(0.9*rc.bottom));

      switch(nScrollCode)
      {
        // User clicked shaft left of the scroll box.
        case SB_PAGEUP:
           yInc = -pagestep;
           break;

        // User clicked shaft right of the scroll box.
        case SB_PAGEDOWN:
           yInc = pagestep;
           break;

        // User clicked the left arrow.
        case SB_LINEUP:
           yInc = -10;
           break;

        // User clicked the right arrow.
        case SB_LINEDOWN:
           yInc = 10;
           break;

        // User dragged the scroll box.
        case SB_THUMBTRACK: {
            // Initialize SCROLLINFO structure
            SCROLLINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;

            if (!GetScrollInfo(hWnd, SB_VERT, &si) )
                return 1; // GetScrollInfo failed

            yInc = si.nTrackPos - yPos;
          break;
        }

        default:
        yInc = 0;
      }

      scrollVertical(gdi, yInc, hWnd);
      gdi->storeAutoPos(gdi->getOffsetY());
      break;
    }

    case WM_HSCROLL:
    {
      int	nScrollCode = (int) LOWORD(wParam); // scroll bar value
      //int hwndScrollBar = (HWND) lParam;      // handle to scroll bar

      int xInc;
      int xPos=gdi->getOffsetX();

      switch(nScrollCode)
      {
        case SB_ENDSCROLL:
          InvalidateRect(hWnd, 0, false);
          return 0;
        
        // User clicked shaft left of the scroll box.
        case SB_PAGEUP:
           xInc = -80;
           break;

        // User clicked shaft right of the scroll box.
        case SB_PAGEDOWN:
           xInc = 80;
           break;

        // User clicked the left arrow.
        case SB_LINEUP:
           xInc = -10;
           break;

        // User clicked the right arrow.
        case SB_LINEDOWN:
           xInc = 10;
           break;

        // User dragged the scroll box.
        case SB_THUMBTRACK:  {
            // Initialize SCROLLINFO structure
            SCROLLINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;

            if (!GetScrollInfo(hWnd, SB_HORZ, &si) )
                return 1; // GetScrollInfo failed

            xInc = si.nTrackPos - xPos;
          break;
        }
          //xInc = HIWORD(wParam) - xPos;
          //break;
        default:
          xInc = 0;
      }

      SCROLLINFO si;
      si.cbSize=sizeof(si);
      si.fMask=SIF_ALL;
      GetScrollInfo(hWnd, SB_HORZ, &si);

      if (si.nPage==0)
        xInc = 0;

      int a=si.nMax-signed(si.nPage-1) - xPos;

      if ((xInc = max( -xPos, min(xInc, a)))!=0) {
        xPos += xInc;
        RECT ScrollArea, ClipArea;
        GetClientRect(hWnd, &ScrollArea);
        ClipArea=ScrollArea;

        gdi->setOffsetX(xPos);

        ScrollWindowEx (hWnd, -xInc,  0,
          0, &ClipArea,
          (HRGN) NULL, (LPRECT) NULL, SW_INVALIDATE|SW_SCROLLCHILDREN);

        si.cbSize = sizeof(si);
        si.fMask  = SIF_POS;
        si.nPos   = xPos;

        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
        UpdateWindow (hWnd);
      }
      break;
    }

    case WM_MOUSEWHEEL: {
      int dz = GET_WHEEL_DELTA_WPARAM(wParam);
      scrollVertical(gdi, -dz, hWnd);
      gdi->storeAutoPos(gdi->getOffsetY());
      }
      break;

    case WM_TIMER:
      if (wParam == 1001) {
        double autoScroll, pos;
        gdi->getAutoScroll(autoScroll, pos);

        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;

        GetScrollInfo(hWnd, SB_VERT, &si);
        int dir = gdi->getAutoScrollDir();
        int dy = 0;
        if ((dir<0 && si.nPos <= si.nMin) ||
            (dir>0 && (si.nPos + int(si.nPage)) >= si.nMax)) {
          autoScroll = -autoScroll;
          gdi->setAutoScroll(-1); // Mirror

          double nextPos = pos + autoScroll;
          dy = int(nextPos - si.nPos);
          gdi->storeAutoPos(nextPos);

          //gdi->setData("AutoScroll", -int(data));
        }
        else {
          double nextPos = pos + autoScroll;
          dy = int(nextPos - si.nPos);
          gdi->storeAutoPos(nextPos);
          //gdi->setData("Discrete", DWORD(nextPos*1e3));
        }

        scrollVertical(gdi, dy, hWnd);
      }
      else
        MessageBox(hWnd, L"Runtime exception", 0, MB_OK);
      break;

    case WM_ACTIVATE: {
      int fActive = LOWORD(wParam);
      if (fActive != WA_INACTIVE)
        currentFocusIx = ix;

      return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_USER + 2:
      if (gdi)
       LoadPage(*gdi, TabType(wParam));
      break;

    case WM_COMMAND:
      wmId    = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      // Parse the menu selections:
      switch (wmId)
      {
      case 0: break;
        default:
           return DefWindowProc(hWnd, message, wParam, lParam);
      }
      break;

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      RECT rt;
      GetClientRect(hWnd, &rt);

      if (gdi && (ps.rcPaint.right|ps.rcPaint.left|ps.rcPaint.top|ps.rcPaint.bottom) != 0 )
        gdi->draw(hdc, rt, ps.rcPaint);
      /*{
      HANDLE icon = LoadImage(hInst, (LPCTSTR)IDI_MEOS, IMAGE_ICON, 64, 64, LR_SHARED);
      DrawIconEx(hdc, 0,0, (HICON)icon, 64, 64, 0, NULL, DI_NORMAL | DI_COMPAT);
      }*/
      EndPaint(hWnd, &ps);
      break;

    case WM_ERASEBKGND:
      return 0;
      break;

    case WM_DESTROY:
      if (ix > 0) {
        gdi->makeEvent("CloseWindow", "meos", 0, 0, false);
        gdi_extra[ix] = 0;
        delete gdi;

        while(!gdi_extra.empty() && gdi_extra.back() == 0)
          gdi_extra.pop_back();
      }
      break;

    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
      }
      break;
  }
    return FALSE;
}


namespace setup {
const int nFiles=7;
const wchar_t *fileList[nFiles]={L"baseclass.xml",
                                 L"wfamily.mwd",
                                 L"wgiven.mwd",
                                 L"wclub.mwd",
                                 L"wclass.mwd",
                                 L"database.wclubs",
                                 L"database.wpersons"};
}

void Setup(bool overwrite, bool overwriteAll)
{
  static bool isSetup=false;
  if (isSetup && overwrite==false)
    return;
  isSetup=true; //Run at most once.

  vector<pair<wstring, bool> > toInstall;
  for(int k=0;k<setup::nFiles;k++) {
    toInstall.push_back(make_pair(wstring(setup::fileList[k]), overwriteAll));
  }

  wchar_t dir[260];
  GetCurrentDirectory(260, dir);
  vector<wstring> dyn;
  if (overwrite)
    expandDirectory(dir, L"*.meos", dyn);
  
  for (size_t k = 0; k < dyn.size(); k++)
    toInstall.push_back(make_pair(dyn[k], true));

  wchar_t bf[260];
  for(size_t k=0; k<toInstall.size(); k++) {
    const wstring src = toInstall[k].first;
    wchar_t filename[128];
    wchar_t ext[32];
    _wsplitpath_s(src.c_str(), NULL, 0, NULL,0, filename, 128, ext, 32);
    wstring fullFile = wstring(filename) + ext;
    
    getUserFile(bf, fullFile.c_str());
    bool canOverwrite = overwrite && toInstall[k].second;
    CopyFile(toInstall[k].first.c_str(), bf, !canOverwrite);
  }
}

void exportSetup()
{
  wchar_t bf[260];
  for(int k=0;k<setup::nFiles;k++) {
    getUserFile(bf, setup::fileList[k]);
    CopyFile(bf, setup::fileList[k], false);
  }
}

bool getMeOSFile(wchar_t *FileNamePath, const wchar_t *FileName) {
  wchar_t Path[MAX_PATH];

  wcscpy_s(Path, programPath);
  int i=wcslen(Path);
  if (Path[i-1]!='\\')
    wcscat_s(Path, MAX_PATH, L"\\");

  wcscat_s(Path, FileName);
  wcscpy_s(FileNamePath, MAX_PATH, Path);
  return true;
}

bool getUserFile(wchar_t* FileNamePath, const wchar_t* FileName) {
  wchar_t Path[MAX_PATH];
  wchar_t AppPath[MAX_PATH];

  if (SHGetSpecialFolderPath(hWndMain, Path, CSIDL_APPDATA, 1) != NOERROR) {
    int i = wcslen(Path);
    if (Path[i - 1] != '\\')
      wcscat_s(Path, MAX_PATH, L"\\");

    wcscpy_s(AppPath, MAX_PATH, Path);
    wcscat_s(AppPath, MAX_PATH, L"Meos\\");

    CreateDirectory(AppPath, NULL);

    Setup(false, false);

    wcscpy_s(FileNamePath, MAX_PATH, AppPath);
    wcscat_s(FileNamePath, MAX_PATH, FileName);

    //return true;
  }
  else wcscpy_s(FileNamePath, MAX_PATH, FileName);

  return true;
}


bool getDesktopFile(wchar_t *fileNamePath, const wchar_t *fileName, const wchar_t *subFolder)
{
  wchar_t Path[MAX_PATH];
  wchar_t AppPath[MAX_PATH];

  if (SHGetSpecialFolderPath(hWndMain, Path, CSIDL_DESKTOPDIRECTORY, 1)!=NOERROR) {
    int i=wcslen(Path);
    if (Path[i-1]!='\\')
      wcscat_s(Path, MAX_PATH, L"\\");

    wcscpy_s(AppPath, MAX_PATH, Path);
    wcscat_s(AppPath, MAX_PATH, L"Meos\\");

    CreateDirectory(AppPath, NULL);

    if (subFolder) {
      wcscat_s(AppPath, MAX_PATH, subFolder);
      wcscat_s(AppPath, MAX_PATH, L"\\");
      CreateDirectory(AppPath, NULL);
    }

    wcscpy_s(fileNamePath, MAX_PATH, AppPath);
    wcscat_s(fileNamePath, MAX_PATH, fileName);
  }
  else wcscpy_s(fileNamePath, MAX_PATH, fileName);

  return true;
}

static set<wstring> tempFiles;
static wstring tempPath;

wstring getTempPath() {
  wchar_t tempFile[MAX_PATH];
  if (tempPath.empty()) {
    wchar_t path[MAX_PATH];
    GetTempPath(MAX_PATH, path);
    GetTempFileName(path, L"meos", 0, tempFile);
    DeleteFile(tempFile);
    if (CreateDirectory(tempFile, NULL))
      tempPath = tempFile;
    else
      throw std::exception("Failed to create temporary file.");
  }
  return tempPath;
}

void registerTempFile(const wstring &tempFile) {
  tempFiles.insert(tempFile);
}

wstring getTempFile() {
  getTempPath();

  wchar_t tempFile[MAX_PATH];
  if (GetTempFileName(tempPath.c_str(), L"ix", 0, tempFile)) {
    tempFiles.insert(tempFile);
    return tempFile;
  }
  else
    throw std::exception("Failed to create temporary file.");
}

void removeTempFile(const wstring &file) {
  DeleteFile(file.c_str());
  tempFiles.erase(file);
}

void removeTempFiles() {
  vector<wstring> dir;
  for (set<wstring>::iterator it = tempFiles.begin(); it!= tempFiles.end(); ++it) {
    wchar_t c = *it->rbegin();
    if (c == '/' || c == '\\')
      dir.push_back(*it);
    else
      DeleteFile(it->c_str());
  }
  tempFiles.clear();
  bool removed = true;
  while (removed) {
    removed = false;
    for (size_t k = 0; k<dir.size(); k++) {
      if (!dir[k].empty() && RemoveDirectory(dir[k].c_str()) != 0) {
        removed = true;
        dir[k].clear();
      }
    }
  }

  if (!tempPath.empty()) {
    RemoveDirectory(tempPath.c_str());
    tempPath.clear();
  }
}

INT_PTR CALLBACK splashDialogProc(
  _In_ HWND   hwndDlg,
  _In_ UINT   uMsg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
) {
  PAINTSTRUCT ps;

  switch (uMsg) {
  case WM_INITDIALOG:
   // SetWindowLong(hwndDlg, GWL_EXSTYLE, GetWindowLong(hwndDlg, GWL_EXSTYLE) | WS_EX_LAYERED);
    // Make this window 40% alpha
    //SetLayeredWindowAttributes(hwndDlg, 0, (255 * 60) / 100, LWA_ALPHA);
    break;

  case WM_PAINT: {
    HDC hdc = BeginPaint(hwndDlg, &ps);
    RECT rt;
    GetClientRect(hwndDlg, &rt);
    image.loadImage(IDI_SPLASHIMAGE, Image::ImageMethod::MonoAlpha);
    int h = image.getHeight(IDI_SPLASHIMAGE);
    int w = image.getWidth(IDI_SPLASHIMAGE);
    image.drawImage(IDI_SPLASHIMAGE, Image::ImageMethod::MonoAlpha, hdc, (rt.right - w) / 2, (rt.bottom - h) / 2, w, h);
    EndPaint(hwndDlg, &ps);
    break;
  }
  case WM_ERASEBKGND:
    return 1;
  }

  return 0;
}