/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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

#include <vector>
#include <map>

class StringCache {
private:
  vector<std::string> cache;
  size_t ix;

  vector<std::wstring> wcache;
  size_t wix;
public:
  static StringCache &getInstance();

  void init() {cache.resize(256); wcache.resize(256);}
  void clear() {cache.clear(); wcache.clear();}

  std::string &get() {
    if ( (++ix) >= cache.size() )
      ix = 0;
    int lx = ix;
    return cache[lx];
  }

  std::wstring &wget() {
    if ( (++wix) >= wcache.size() )
      wix = 0;
    int lx = wix;
    return wcache[lx];
  }
};

string convertSystemTime(const SYSTEMTIME &st);
string convertSystemTimeOnly(const SYSTEMTIME &st);
string convertSystemDate(const SYSTEMTIME &st);
string getLocalTime();
string getLocalDate();
string getLocalTimeOnly();

// Get a day number after a fixed day some time ago...
int getRelativeDay();

/// Get time and date in a format that forms a part of a filename
string getLocalTimeFileName();

const string &getTimeMS(int m);
const string &formatTime(int rt);
const string &formatTimeHMS(int rt);
string formatTimeIOF(int rt, int zeroTime);

int convertDateYMS(const string &m, bool checkValid);
int convertDateYMS(const string &m, SYSTEMTIME &st, bool checkValid);

// Convert a "general" time string to a MeOS compatible time string
void processGeneralTime(const string &generalTime, string &meosTime, string &meosDate);

// Format number date 20160421 -> 2016-04-21 (if iso) or according to a custom format otherwise
string formatDate(int m, bool useIsoFormat);

__int64 SystemTimeToInt64Second(const SYSTEMTIME &st);
SYSTEMTIME Int64SecondToSystemTime(__int64 time);

#define NOTIME 0x7FFFFFFF

//Returns a time converted from +/-MM:SS or NOTIME, in seconds
int convertAbsoluteTimeMS(const string &m);
// Parses a time on format HH:MM:SS+01:00Z or HHMMSS+0100Z (but ignores time zone)
int convertAbsoluteTimeISO(const string &m);

/** Returns a time converted from HH:MM:SS or -1, in seconds
   @param m time to convert
   @param daysZeroTime -1 do not support days syntax, positive interpret days w.r.t the specified zero time.
*/
int convertAbsoluteTimeHMS(const string &m, int daysZeroTime);

const vector<string> &split(const string &line, const string &separators, vector<string> &split_vector);
const string &unsplit(const vector<string> &split_vector, const string &separators, string &line);

const string &MakeDash(const string &t);
const string &MakeDash(const char *t);
string FormatRank(int rank);
const string &itos(int i);
string itos(unsigned long i);
string itos(unsigned int i);
string itos(__int64 i);

///Lower case match (filt_lc must be lc)
bool filterMatchString(const string &c, const char *filt_lc);
bool matchNumber(int number, const char *key);

int getMeosBuild();
string getMeosDate();
string getMeosFullVersion();
string getMajorVersion();
string getMeosCompectVersion();

void getSupporters(vector<string> &supp);

int countWords(const char *p);

string trim(const string &s);

bool fileExist(const char *file);

bool stringMatch(const string &a, const string &b);

const char *decodeXML(const char *in);
const string &decodeXML(const string &in);
const string &encodeXML(const string &in);

/** Extend a year from 03 -> 2003, 97 -> 1997 etc */
int extendYear(int year);

/** Get current year, e.g., 2010 */
int getThisYear();

/** Translate a char to lower/stripped of accents etc.*/
int toLowerStripped(int c);

/** Canonize a person/club name */
const char *canonizeName(const char *name);

/** String distance between 0 and 1. 0 is equal*/
double stringDistance(const char *a, const char *b);


/** Get a number suffix, Start 1 -> 1. Zero for none*/
int getNumberSuffix(const string &str);

/// Extract any number from a string and return the number, prefix and suffix
int extractAnyNumber(const string &str, string &prefix, string &suffix);


/** Compare classnames, match H21 Elit with H21E and H21 E */
bool compareClassName(const string &a, const string &b);

/** Get WinAPI error from code */
string getErrorMessage(int code);

class HLS {
private:
  WORD HueToRGB(WORD n1, WORD n2, WORD hue) const;
public:

  HLS(WORD H, WORD L, WORD S) : hue(H), lightness(L), saturation(S) {}
  HLS() : hue(0), lightness(0), saturation(1) {}
  WORD hue;
  WORD lightness;
  WORD saturation;
  void lighten(double f);
  void saturate(double s);
  void colorDegree(double d);
  HLS &RGBtoHLS(DWORD lRGBColor);
  DWORD HLStoRGB() const;
};

#ifndef MEOSDB
  void unzip(const char *zipfilename, const char *password, vector<string> &extractedFiles);
  int zip(const char *zipfilename, const char *password, const vector<string> &files);
#endif

bool isAscii(const string &s);
bool isNumber(const string &s);
int convertDynamicBase(const string &s, long long &out);
void convertDynamicBase(long long val, int base, char out[16]);

/// Find all files in dir matching given file pattern
bool expandDirectory(const char *dir, const char *pattern, vector<string> &res);

enum PersonSex {sFemale = 1, sMale, sBoth, sUnknown};

PersonSex interpretSex(const string &sex);

string encodeSex(PersonSex sex);

string makeValidFileName(const string &input, bool strict);

/** Initial capital letter. */
void capitalize(string &str);

/** Initial capital letter for each word. */
void capitalizeWords(string &str);

string getTimeZoneString(const string &date);

/** Return bias in seconds. UTC = local time + bias. */
int getTimeZoneInfo(const string &date);

/** Compare bib numbers (which may contain non-digits, e.g. A-203, or 301a, 301b)*/
bool compareBib(const string &b1, const string &b2);

/** Split a name into Given, Family, and return Given.*/
string getGivenName(const string &name);

/** Split a name into Given, Family, and return Family.*/
string getFamilyName(const string &name);

/** Simple file locking class to prevent opening in different MeOS session. */
class MeOSFileLock {
  HANDLE lockedFile;
  // Not supported
  MeOSFileLock(const MeOSFileLock &);
  const MeOSFileLock &operator=(const MeOSFileLock &);

public:
  MeOSFileLock() {lockedFile = INVALID_HANDLE_VALUE;}
  ~MeOSFileLock() {unlockFile();}

  void unlockFile();
  void lockFile(const string &file);
};

namespace MeOSUtil {
  extern int useHourFormat;
}
