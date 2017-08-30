// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#define NOMINMAX
// Windows Header Files:
#include <windows.h>
#include <commctrl.h>

#include <string>
#include <list>
#include <fstream>
using namespace std;

bool GetUserFile(char *file, const char *in);

// TODO: reference additional headers your program requires here

const extern string _EmptyString;
const extern string _VacantName;
const extern string _UnkownName;

