#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include <winsdkver.h>  
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0601 // Target Windows 7.
#include <sdkddkver.h>

#include <windows.h>
#include <commctrl.h>
#include "timeconstants.hpp"

// C RunTime Header Files

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <memory>
#include <tchar.h>

#include <string>
#include <fstream>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <algorithm>

using std::shared_ptr;
using std::string;
using std::wstring;
using std::vector;
using std::list;
using std::map;
using std::set;
using std::pair;
using std::make_pair;
using std::make_shared;
using std::unordered_set;
using std::unordered_map;
using std::min;
using std::max;
using std::numeric_limits;
using std::multimap;
using std::unique_ptr;
using std::deque;
using std::tuple;

bool getUserFile(wchar_t *fileNamePath, const wchar_t *fileName);
bool getDesktopFile(wchar_t *fileNamePath, const wchar_t *fileName, const wchar_t *subFolder = 0);
wstring getMeOSFile(const wchar_t *fileName);

class gdioutput;
gdioutput *createExtraWindow(const string &tag, const wstring &title, int max_x = 0, int max_y = 0, bool fixedSize = false);
gdioutput *getExtraWindow(const string &tag, bool toForeGround);
string uniqueTag(const char *base);

wstring getTempFile();
wstring getTempPath();
void removeTempFile(const wstring &file); // Delete a temporyary
void registerTempFile(const wstring &tempFile); //Register a file/folder as temporary => autmatic removal on exit.

const extern string _EmptyString;
const extern string _VacantName;
const extern wstring _EmptyWString;

