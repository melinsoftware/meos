// xmlparser.h: interface for the xmlparser class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_XMLPARSER_H__87834E6D_6AB1_471C_8E1C_E65D67A4F98A__INCLUDED_)
#define AFX_XMLPARSER_H__87834E6D_6AB1_471C_8E1C_E65D67A4F98A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
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

#include <vector>
#include <sstream>
class xmlobject;

typedef vector<xmlobject> xmlList;
class xmlparser;
class gdioutput;

const int buff_pre_alloc = 1024 * 10;

struct xmldata
{
  xmldata(const char *t, char *d);
  const char *tag;
  char *data;
  int parent;
  int next;
};

struct xmlattrib
{

  xmlattrib(const char *t, char *d, const xmlparser *parser);
  const char *tag;
  char *data;
  operator bool() const {return data!=0;}

  int getInt() const {if (data) return atoi(data); else return 0;}
  const char *get() const;
  const wchar_t *wget() const;
private:
  const xmlparser *parser;
};

class ProgressWindow;

class xmlparser
{
protected:
  static char *ltrim(char *s);
  static const char *ltrim(const char *s);

  string tagStack[32];
  int tagStackPointer;

  bool toString;
  ofstream foutFile;
  ostringstream foutString;

  ifstream fin;

  std::ostream &fOut() {
    if (toString)
      return foutString;
    else
      return foutFile;
  }

  int lineNumber;
  string doctype;

  vector<int> parseStack;
  vector<xmldata> xmlinfo;
  vector<char> xbf;

  bool processTag(char *start, char *end);

  bool checkUTF(const char *ptr) const;
  bool parse(int maxobj);

  void convertString(const char *in, char *out, int maxlen) const;
  void convertString(const char *in, wchar_t *out, int maxlen) const;

  // True if empty/zero values are excluded when writing
  bool cutMode;

  bool isUTF;
  char strbuff[buff_pre_alloc]; // Temporary buffer for processing (no threading allowed)
  wchar_t strbuffw[buff_pre_alloc]; // Temporary buffer for processing (no threading allowed)

  ProgressWindow *progress;
  int lastIndex;

  gdioutput *utfConverter;

public:
  void access(int index);

  void setProgress(HWND hWnd);

//	bool failed(){return errorMessage.length()>0;}

  const xmlobject getObject(const char *pname) const;
//	const char *getError();

  void read(const wstring &file, int maxobj = 0);
  void readMemory(const string &mem, int maxobj);

  void write(const char *tag, const char *prop,
              const string &value);
  void write(const char *tag, const char *prop,
              const wstring &value);
  void write(const char *tag); // Empty case
  void write(const char *tag, const char *prop,
             const wchar_t *value);
  void writeBool(const char *tag, const char *prop,
             const bool value);

  //void write(const char *tag, const char *prop,
  //           const string &propValue, const string &value);
  void write(const char *tag, const char *prop,
             const wstring &propValue, const wstring &value);

  //void write(const char *tag, const char *prop,
  //           bool propValue, const string &value);
  void writeBool(const char *tag, const char *prop,
                 bool propValue, const wstring &value);

  //void write(const char *tag, const char *prop,
  //           const char *propValue, const string &value);
  void write(const char *tag, const char *prop,
             const wchar_t *propValue, const wstring &value);

  //void write(const char *tag, const vector< pair<string, string> > &propValue, const string &value);
  //void write(const char *tag, const vector< pair<string, string> > &propValue, const wstring &value);
  void write(const char *tag, const vector< pair<string, wstring> > &propValue, const wstring &value);

  void write(const char *tag, const string &value);
  void write(const char *tag, const wstring &value);
  
  void write(const char *tag, int value);

  void writeBool(const char *tag, bool value);
  void write64(const char *tag, __int64);

  void startTag(const char *tag);
  void startTag(const char *tag, const char *Property,
                const string &Value);
  //void startTag(const char *tag, const vector<string> &propvalue);

  void startTag(const char *tag, const char *Property,
                const wstring &Value);
  void startTag(const char *tag, const vector<wstring> &propvalue);


  void endTag();
  int closeOut();
  void openOutput(const wchar_t *file, bool useCutMode);
  void openOutputT(const wchar_t *file, bool useCutMode, const string &type);

  void openMemoryOutput(bool useCutMode);
  void getMemoryOutput(string &res);


  const string &encodeXML(const string &input);
  const string &encodeXML(const wstring &input);

  xmlparser();
  virtual ~xmlparser();

  friend class xmlobject;
  friend struct xmlattrib;
};

class xmlobject
{
protected:
  xmlobject(xmlparser *p) {parser = p;}
  xmlobject(xmlparser *p, int i) {parser = p; index = i;}

  xmlparser *parser;
  int index;
public:
  const char *getName() const {return parser->xmlinfo[index].tag;}
  xmlobject getObject(const char *pname) const;
  xmlattrib getAttrib(const char *pname) const;

  int getObjectInt(const char *pname) const
  {
    xmlobject x(getObject(pname));
    if (x)
      return x.getInt();
    else {
      xmlattrib xa(getAttrib(pname));
      if (xa)
        return xa.getInt();
    }
    return 0;
  }

  bool got(const char *pname) const {
    xmlobject x(getObject(pname));
    if (x)
      return true;
    else {
      xmlattrib xa(getAttrib(pname));
      if (xa)
        return true;
    }
    return false;
  }

  bool getObjectBool(const char *pname) const;

  string &getObjectString(const char *pname, string &out) const;
  char *getObjectString(const char *pname, char *out, int maxlen) const;

  wstring &getObjectString(const char *pname, wstring &out) const;
  wchar_t *getObjectString(const char *pname, wchar_t *out, int maxlen) const;


  void getObjects(xmlList &objects) const;
  void getObjects(const char *tag, xmlList &objects) const;

  bool is(const char *pname) const {
    const char *n = getName();
    return n[0] == pname[0] && strcmp(n, pname)==0;
  }

  const char *getRaw() const {return parser->xmlinfo[index].data;}
  
  const char *get() const;
  const wchar_t *getw() const;

  int getInt() const {const char *d = parser->xmlinfo[index].data;
                      return d ? atoi(d) : 0;}
  __int64 getInt64() const {const char *d = parser->xmlinfo[index].data;
                           return d ? _atoi64(d) : 0;}

  bool isnull() const {return parser==0;}

  operator bool() const {return parser!=0;}

  xmlobject();
  virtual ~xmlobject();
  friend class xmlparser;
};


#endif // !defined(AFX_XMLPARSER_H__87834E6D_6AB1_471C_8E1C_E65D67A4F98A__INCLUDED_)
