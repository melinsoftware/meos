// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define NOMINMAX
// Windows Header Files:
#include <windows.h>
#include <commctrl.h>

// C RunTime Header Files

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <memory.h>
#include <tchar.h>

#include <string>
#include <fstream>
#include <list>
#include <crtdbg.h>

using namespace std;
bool getUserFile(char *fileNamePath, const char *fileName);
bool getDesktopFile(char *fileNamePath, const char *fileName, const char *subFolder = 0);
bool getMeOSFile(char *FileNamePath, const char *FileName);

class gdioutput;
gdioutput *createExtraWindow(const string &tag, const string &title, int max_x = 0, int max_y = 0);
gdioutput *getExtraWindow(const string &tag, bool toForeGround);
string uniqueTag(const char *base);

void LoadPage(const string &name);

string getTempFile();
string getTempPath();
void removeTempFile(const string &file); // Delete a temporyary
void registerTempFile(const string &tempFile); //Register a file/folder as temporary => autmatic removal on exit.

const extern string _EmptyString;
const extern string _VacantName;
const extern string _UnkownName;

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
