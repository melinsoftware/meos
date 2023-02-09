#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define NOMINMAX

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

using namespace std;
bool getUserFile(wchar_t *fileNamePath, const wchar_t *fileName);
bool getDesktopFile(wchar_t *fileNamePath, const wchar_t *fileName, const wchar_t *subFolder = 0);
bool getMeOSFile(wchar_t *FileNamePath, const wchar_t *FileName);

class gdioutput;
gdioutput *createExtraWindow(const string &tag, const wstring &title, int max_x = 0, int max_y = 0, bool fixedSize = false);
gdioutput *getExtraWindow(const string &tag, bool toForeGround);
string uniqueTag(const char *base);

void LoadPage(const string &name);

wstring getTempFile();
wstring getTempPath();
void removeTempFile(const wstring &file); // Delete a temporyary
void registerTempFile(const wstring &tempFile); //Register a file/folder as temporary => autmatic removal on exit.

const extern string _EmptyString;
const extern string _VacantName;
const extern wstring _EmptyWString;

