/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2021 Melin Software HB

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

// xmlparser.cpp: implementation of the xmlparser class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "xmlparser.h"
#include "meos_util.h"
#include "progress.h"
#include "meosexception.h"
#include "gdioutput.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern gdioutput *gdi_main;

xmlparser::xmlparser() : utfConverter(gdi_main)
{
  progress = 0;
  lastIndex = 0;
  tagStackPointer=0;
  isUTF = false;
  cutMode = false;
  toString = false;
}

xmlparser::~xmlparser()
{
  delete progress;
  fin.close();
  foutFile.close();
}

inline bool isBlankSpace(char b) {
  return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

void xmlparser::setProgress(HWND hWnd)
{
  progress = new ProgressWindow(hWnd);
}

void xmlparser::access(int index) {
  if (progress && (index-lastIndex)>1000 ) {
    lastIndex = index;
    progress->setProgress(500 + int(500.0 * index/xmlinfo.size()));
  }
}


xmlobject::xmlobject()
{
  parser = 0;
}

xmlobject::~xmlobject()
{
  //MessageBox(NULL, name.c_str(), "Destroying: ", MB_OK);

//	if (objects) delete objects;
}


const string &xmlparser::encodeXML(const wstring &input) {
  return ::encodeXML(gdi_main->toUTF8(input));
}

const string &xmlparser::encodeXML(const string &input) {
  if (utfConverter)
    return ::encodeXML(utfConverter->toUTF8(gdi_main->widen(input))); //XXX WCS
  else
    return ::encodeXML(input);
}

void xmlparser::write(const char *tag, const wstring &Value)
{
  if (!cutMode || !Value.empty()) {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      sprintf_s(bf, "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
    /*fOut() << "<" << tag << ">"
           << encodeXML(Value)
           << "</" << tag << ">" << endl;*/
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const string &Value)
{
  if (!cutMode || Value!="") {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      sprintf_s(bf, "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Value)
{
  if (!cutMode || Value != "") {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      sprintf_s(bf, "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}


void xmlparser::write(const char *tag)
{
  char bf[128];
  sprintf_s(bf, "<%s/>\n", tag);
  fOut() << bf;

  //fOut() << "<" << tag << "/>" << endl;
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const string &Value)
{
  if (!cutMode || Value!="") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const wstring &Value)
{
  if (!cutMode || !Value.empty()) {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *prop, const wchar_t *value)
{
  encodeString = value;
  write(tag, prop, encodeString);
}

void xmlparser::writeBool(const char *tag, const char *prop, bool value)
{
  if (!cutMode || value)
    write(tag, prop, value ? L"true" : L"false");
}

/*
void xmlparser::write(const char *tag, const char *Property, const string &PropValue, const string &Value)
{
  if (!cutMode || Value != "" || PropValue != "") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(PropValue) << "\">" << encodeXML(Value)
           << "</" << tag << ">" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}
*/

void xmlparser::write(const char *tag, const char *Property, const wstring &PropValue, const wstring &Value)
{
  if (!cutMode || Value != L"" || PropValue != L"") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(PropValue) << "\">" << encodeXML(Value)
           << "</" << tag << ">\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

/*
void xmlparser::write(const char *tag, const vector< pair<string, string> > &propValue, const string &value) {
  if (!cutMode || value != "" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">" << endl;
    }
    else
      fOut() << "/>" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const vector< pair<string, string> > &propValue, const wstring &value) {
  if (!cutMode || value != L"" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">" << endl;
    }
    else
      fOut() << "/>" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}
*/
void xmlparser::write(const char *tag, const vector< pair<string, wstring> > &propValue, const wstring &value) {
  if (!cutMode || value != L"" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">\n";
    }
    else
      fOut() << "/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

/*
void xmlparser::write(const char *tag, const char *prop,
                      bool propValue, const string &value) {
  write(tag, prop, propValue ? "true" : "false", value);
}*/

void xmlparser::writeBool(const char *tag, const char *prop,
                      bool propValue, const wstring &value) {
  static const wstring wTrue = L"true";
  static const wstring wFalse = L"false";

  write(tag, prop, propValue ? wTrue : wFalse, value);
}
/*
void xmlparser::write(const char *tag, const char *prop,
                      const char *propValue, const string &value) {
  write(tag, prop, string(propValue), value);
}*/

void xmlparser::write(const char *tag, const char *prop,
                      const wchar_t *propValue, const wstring &value) {
  write(tag, prop, wstring(propValue), value);
}

void xmlparser::write(const char *tag, int Value)
{
  if (!cutMode || Value!=0) {
    char bf[256];
    sprintf_s(bf, "<%s>%d</%s>\n", tag, Value, tag);
    fOut() << bf;
    //fOut() << "<" << tag << ">"
    //       << Value
    //       << "</" << tag << ">" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::writeBool(const char *tag, bool value)
{
  if (!cutMode || value) {
    if (value) {
      char bf[256];
      sprintf_s(bf, "<%s>true</%s>\n", tag, tag);
      fOut() << bf;
    }
    else {
      char bf[256];
      sprintf_s(bf, "<%s>false</%s>\n", tag, tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}


void xmlparser::write64(const char *tag, __int64 Value)
{
  if (!cutMode || Value!=0) {
    fOut() << "<" << tag << ">"
           << Value
           << "</" << tag << ">\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::startTag(const char *tag, const char *prop, const wstring &Value)
{
  if (tagStackPointer<32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      sprintf_s(bf, "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    }
    else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">" << endl;
    }

    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}


void xmlparser::startTag(const char *tag, const char *prop, const string &Value)
{
  if (tagStackPointer<32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      sprintf_s(bf, "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    }
    else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">" << endl;
    }

    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}
/*
void xmlparser::startTag(const char *tag, const vector<string> &propvalue)
{
  if (tagStackPointer<32) {
    fOut() << "<" << tag << " ";
    for (size_t k=0;k<propvalue.size(); k+=2) {
      fOut() << propvalue[k] << "=\"" << encodeXML(propvalue[k+1]) << "\" ";
    }
    fOut() << ">\n";
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}
*/
void xmlparser::startTag(const char *tag, const vector<wstring> &propvalue)
{
  if (tagStackPointer<32) {
    fOut() << "<" << tag ;
    for (size_t k=0;k<propvalue.size(); k+=2) {
      fOut() << " " << encodeXML(propvalue[k]) << "=\"" << encodeXML(propvalue[k+1]) << "\"";
    }
    fOut() << ">\n";
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}

void xmlparser::startTag(const char *tag)
{
  if (tagStackPointer<32) {
    char bf[128];
    sprintf_s(bf, "<%s>\n", tag);
    fOut() << bf;
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}

void xmlparser::endTag()
{
  if (tagStackPointer>0) {
    char bf[128];
    const char *tag = tagStack[--tagStackPointer].c_str();
    sprintf_s(bf, "</%s>\n", tag);
    fOut() << bf;

    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else throw std::exception("BAD XML CODE");
}

void xmlparser::openMemoryOutput(bool useCutMode) {
  cutMode = useCutMode;
  toString = true;
  foutString.clear();
  if (utfConverter)
    fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";
  else
    fOut() << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n\n\n";

  string out = foutString.str();
}

void xmlparser::getMemoryOutput(string &res) {
  res = foutString.str();
  foutString.clear();
}

void xmlparser::openOutput(const wchar_t *file, bool useCutMode)
{
  openOutputT(file, useCutMode, "");
}

void xmlparser::openOutputT(const wchar_t *file, bool useCutMode, const string &type)
{
  toString = false;
  cutMode = useCutMode;
  foutFile.open(file);
  checkWriteAccess(file);
  tagStackPointer=0;

  if (foutFile.bad())
    throw meosException(wstring(L"Writing to XML file failed: ") + wstring(file));

  if (utfConverter)
    fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";
  else
    fOut() << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n\n\n";

  if (!type.empty()) {
    startTag(type.c_str());
  }
  return;
}

int xmlparser::closeOut()
{
  while(tagStackPointer>0)
    endTag();

  int len = (int)foutFile.tellp();
  foutFile.close();

  return len;
}

xmldata::xmldata(const char *t, char *d) : tag(t), data(d)
{
  parent = -1;
  next = 0;
}

xmlattrib::xmlattrib(const char *t, char *d, const xmlparser *p) : tag(t), data(d), parser(p) {}

void xmlparser::read(const wstring &file, int maxobj)
{
  fin.open(file.c_str(), ios::binary);

  if (!fin.good())
    throw meosException(L"Failed to open 'X' for reading.#" + file);

  char bf[1024];
  bf[0]=0;

  do {
    fin.getline(bf, 1024, '>');
    lineNumber++;
  }
  while(fin.good() && bf[0]==0);

  char *ptr=ltrim(bf);
  isUTF = checkUTF(ptr);
  int p1 = (int)fin.tellg();

  fin.seekg(0, ios::end);
  int p2 = (int)fin.tellg();
  fin.seekg(p1, ios::beg);

  int asize = p2-p1;
  if (maxobj>0)
    asize = min(asize, maxobj*256);

  if (progress && asize>80000)
    progress->init();

  xbf.resize(asize+1);
  xmlinfo.clear();
  xmlinfo.reserve(xbf.size() / 30); // Guess number of tags

  parseStack.clear();

  fin.read(&xbf[0], xbf.size());
  xbf[asize] = 0;

  fin.close();

  parse(maxobj);
}

void xmlparser::readMemory(const string &mem, int maxobj)
{
  if (mem.empty())
    return;

  char bf[1024];
  bf[0] = mem[0];
  int i = 1;
  int stop = min<int>(1020, mem.length());
  while (i < stop && mem[i-1] != '>'){
    bf[i] = mem[i];
    i++;
  }
  bf[i] = 0;

  char *ptr=ltrim(bf);
  isUTF = checkUTF(ptr);
  int p1 = i;
  int p2 = mem.size();

  int asize = p2-p1;
  if (maxobj>0)
    asize = min(asize, maxobj*256);

  if (progress && asize>80000)
    progress->init();

  xbf.resize(asize+1);
  xmlinfo.clear();
  xmlinfo.reserve(xbf.size() / 30); // Guess number of tags

  parseStack.clear();

  memcpy(&xbf[0], mem.c_str() + p1, xbf.size());
  xbf[asize] = 0;

  parse(maxobj);
}

bool xmlparser::checkUTF(const char *ptr) const {
  bool utf = false;

  if (ptr[0] == -17 && ptr[1]==-69 && ptr[2]==-65) {
    utf = true;
    ptr+=3; //Windows UTF attribute
  }

  if (memcmp(ptr, "<?xml", 5) == 0) {
    int i = 5;
    bool hasEncode = false;
    while (ptr[i]) {
      if ((ptr[i] == 'U' || ptr[i] == 'u') && _memicmp(ptr+i, "UTF-8", 5)==0) {
        utf = true;
        break;
      }
      if (ptr[i] == 'e' && memcmp(ptr+i, "encoding", 8)==0) {
        hasEncode = true;
      }
      i++;
    }
    if (!hasEncode)
      utf = true; // Assume UTF
  }
  else if (ptr[0] == '<' && ptr[1] == '?') {
    // Assume UTF XML if not specified
    utf = true;
  }
  else {
    throw std::exception("Invalid XML file.");
  }
  return utf;
}

bool xmlparser::parse(int maxobj) {
  lineNumber=0;
  int oldPrg = -50001;
  int pp = 0;
  const int size = xbf.size()-2;
  while (pp < size) {
    while (pp < size && xbf[pp] != '<') pp++;

    // Update progress while parsing
    if (progress && (pp - oldPrg)> 50000) {
      progress->setProgress(int(500.0*pp/size));
      oldPrg = pp;
    }

    // Found tag
    if (xbf[pp] == '<') {
      xbf[pp] = 0;
      char *start = &xbf[pp+1];
      while (pp < size && xbf[pp] != '>') pp++;

      if (xbf[pp] == '>') {
        xbf[pp] = 0;
      }
      if (*start=='!')
        continue; //Comment

      processTag(start, &xbf[pp-1]);
    }

    if (maxobj>0 && int(xmlinfo.size()) >= maxobj) {
      xbf[pp+1] = 0;
      return true;
    }
    pp++;
  }

  lastIndex = 0;
  return true;
}

void inplaceDecodeXML(char *in);

bool xmlparser::processTag(char *start, char *end) {
  static char err[64];
  bool onlyAttrib = *end == '/';
  bool endTag = *start == '/';

  char *tag = start;

  if (endTag)
    tag++;

  while (start<=end && !isBlankSpace(*start))
    start++;

  *start = 0;

  if (!endTag && !onlyAttrib) {
    parseStack.push_back(xmlinfo.size());
    xmlinfo.push_back(xmldata(tag, end+2));
    int p = parseStack.size()-2;
    xmlinfo.back().parent = p>=0 ? parseStack[p] : -1;
  }
  else if (endTag) {
    if (!parseStack.empty()){
      xmldata &xd = xmlinfo[parseStack.back()];
      inplaceDecodeXML(xd.data);
      if (strcmp(tag, xd.tag)== 0) {
        parseStack.pop_back();
        xd.next = xmlinfo.size();
      }
      else {
        sprintf_s(err, "Unmatched tag '%s', expected '%s'.", tag, xd.tag);
        throw std::exception(err);
      }
    }
    else
    {
      sprintf_s(err, "Unmatched tag '%s'.", tag);
      throw std::exception(err);
    }
  }
  else if (onlyAttrib) {
    *end = 0;
    xmlinfo.push_back(xmldata(tag, 0));
    int p = parseStack.size() - 1;
    xmlinfo.back().parent = p>=0 ? parseStack[p] : -1;
    xmlinfo.back().next = xmlinfo.size();
  }
  return true;
}

char * xmlparser::ltrim(char *s)
{
  while(*s && isspace(BYTE(*s)))
    s++;

  return s;
}

const char * xmlparser::ltrim(const char *s)
{
  while(*s && isspace(BYTE(*s)))
    s++;

  return s;
}
/*
const char * xmlparser::getError()
{
  return errorMessage.c_str();
}*/

xmlobject xmlobject::getObject(const char *pname) const
{
  if (pname == 0)
    return *this;
  if (isnull())
    throw std::exception("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;

  parser->access(index);

  unsigned child = index+1;
  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    if (strcmp(xmlinfo[child].tag, pname)==0)
      return xmlobject(parser, child);
    else
      child = xmlinfo[child].next;
  }
  return xmlobject(0);
}


void xmlobject::getObjects(xmlList &obj) const
{
  obj.clear();

  if (isnull())
    throw std::exception("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;
  unsigned child = index+1;
  parser->access(index);

  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    obj.push_back(xmlobject(parser, child));
    child = xmlinfo[child].next;
  }
}

void xmlobject::getObjects(const char *tag, xmlList &obj) const
{
  obj.clear();

  if (isnull())
    throw std::exception("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;
  unsigned child = index+1;
  parser->access(index);

  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    if (strcmp(tag, xmlinfo[child].tag) == 0)
      obj.push_back(xmlobject(parser, child));
    child = xmlinfo[child].next;
  }
}


const xmlobject xmlparser::getObject(const char *pname) const
{
  if (xmlinfo.size()>0){
    if (pname == 0 || strcmp(xmlinfo[0].tag, pname) == 0)
      return xmlobject(const_cast<xmlparser *>(this), 0);
    else return xmlobject(const_cast<xmlparser *>(this), 0).getObject(pname);
  }
  else return xmlobject(0);
}

xmlattrib xmlobject::getAttrib(const char *pname) const
{
  if (pname != 0) {
    char *start = const_cast<char *>(parser->xmlinfo[index].tag);
    const char *end = parser->xmlinfo[index].data;

    if (end)
      end-=2;
    else {
      if (size_t(index + 1) < parser->xmlinfo.size())
        end = parser->xmlinfo[index+1].tag - 1;
      else
        end = &parser->xbf.back();
    }

    // Scan past tag.
    while (start<end && *start != 0)
      start++;
    start++;

    char *oldStart = start;
    while (start<end) {
      while(start<end && isBlankSpace(*start))
        start++;

      char *tag = start;

      while(start<end && *start!='=' && *start!=0)
        start++;

      if (start<end && (start[1]=='"' || start[1] == 0)) {
        *start = 0;
        ++start;
        char *value = ++start;

        while(start<end && (*start!='"' && *start != 0))
          start++;

        if (start<=end) {
          *start = 0;
          if (strcmp(pname, tag) == 0)
            return xmlattrib(tag, value, parser);
          start++;
        }
        else {//Error
        }
      }

      if (oldStart == start)
        break;
      else
        oldStart = start;
    }
  }
  return xmlattrib(0,0, parser);
}

static int unconverted = 0;




const wchar_t *xmlobject::getw() const
{
  const char *ptr = getRaw();
  if (ptr == 0)
    return 0;
  static wchar_t buff[buff_pre_alloc];
  int len = strlen(ptr);
  len = min(len+1, buff_pre_alloc-10);
  if (parser->isUTF) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, ptr, len, buff, buff_pre_alloc);
    buff[wlen-1] = 0;
  }
  else {
    int cp = 1252;
  /*XXX TODO: switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/
    int wlen = MultiByteToWideChar(cp, MB_PRECOMPOSED, ptr, len, buff, buff_pre_alloc);
    buff[wlen-1] = 0;
  }
  return buff;
}


const char *xmlobject::get() const
{
  const char *ptr = getRaw();
  if (ptr == 0)
    return 0;
  static char buff[buff_pre_alloc];
  if (parser->isUTF) {
    int len = strlen(ptr);
    len = min(len+1, buff_pre_alloc-10);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, ptr, len, parser->strbuffw, buff_pre_alloc);
    parser->strbuffw[wlen-1] = 0;
    for (int k = 0; k< wlen; k++) {
      buff[k] = parser->strbuffw[k] & 0xFF; 
    }
  }
  else {
    return ptr;
  }
  return buff;
}


void xmlparser::convertString(const char *in, char *out, int maxlen) const
{
  if (in==0)
    throw std::exception("Null pointer exception");

  if (!isUTF) {
    strncpy_s(out, maxlen, in, maxlen-1);
    out[maxlen-1] = 0;
    return;
  }

  wchar_t buff[buff_pre_alloc];
  int len = strlen(in);
  len = min(min(len+1, maxlen), buff_pre_alloc-10);

  int wlen = MultiByteToWideChar(CP_UTF8, 0, in, len, buff, buff_pre_alloc);
  buff[wlen-1] = 0;

  BOOL untranslated = false;
  WideCharToMultiByte(CP_ACP, 0, buff, wlen, out, buff_pre_alloc, "?", &untranslated);
  out[wlen-1] = 0;

  if (untranslated)
    unconverted++;
}

void xmlparser::convertString(const char *in, wchar_t *out, int maxlen) const
{
  if (in==0)
    throw std::exception("Null pointer exception");

  int len = strlen(in);
  len = min(min(len+1, maxlen), buff_pre_alloc-10);

  if (!isUTF) {
    int cp = 1252;
  /*XXX TODO: switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/
    int wlen = MultiByteToWideChar(cp, MB_PRECOMPOSED, in, len, out, maxlen);
    out[wlen-1] = 0;
    return;
  }

  int wlen = MultiByteToWideChar(CP_UTF8, 0, in, len, out, maxlen);
  out[wlen-1] = 0;
}


bool xmlobject::getObjectBool(const char *pname) const
{
  string tmp;
  getObjectString(pname, tmp);

  return tmp=="true" ||
         atoi(tmp.c_str()) > 0 ||
         _strcmpi(trim(tmp).c_str(), "true") == 0;
}

string &xmlobject::getObjectString(const char *pname, string &out) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const char *bf = x.getRaw();
    if (bf) {
      parser->convertString(x.getRaw(), parser->strbuff, buff_pre_alloc);
      out = parser->strbuff;
      return out;
    }
  }

  xmlattrib xa(getAttrib(pname));
  if (xa && xa.data) {
    parser->convertString(xa.get(), parser->strbuff, buff_pre_alloc);
    out = parser->strbuff;
  }
  else
    out = "";

  return out;
}

wstring &xmlobject::getObjectString(const char *pname, wstring &out) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const wchar_t *bf = x.getw();
    if (bf) {
      out = bf;
      return out;
    }
  }

  xmlattrib xa(getAttrib(pname));
  if (xa && xa.data) {
    parser->convertString(xa.get(), parser->strbuffw, buff_pre_alloc);
    out = parser->strbuffw;
  }
  else
    out = L"";

  return out;
}


char *xmlobject::getObjectString(const char *pname, char *out, int maxlen) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const char *bf = x.getRaw();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    else
      out[0] = 0;
  }
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      parser->convertString(xa.data, out, maxlen);
      inplaceDecodeXML(out);
    } else
       out[0] = 0;
  }
  return out;
}

wchar_t *xmlobject::getObjectString(const char *pname, wchar_t *out, int maxlen) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const char *bf = x.getRaw();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    else 
      out[0] = 0;
  }
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      inplaceDecodeXML(xa.data); //WCS XXX  Only once!?
      parser->convertString(xa.data, out, maxlen);
    } else
       out[0] = 0;
  }
  return out;
}


const char *xmlattrib::get() const
{
  if (data)
    return decodeXML(data);
  else
    return 0;
}

const wchar_t *xmlattrib::wget() const
{
  if (data) {
    const char *dec = decodeXML(data);
    static wchar_t xbf[buff_pre_alloc];
    parser->convertString(dec, xbf, buff_pre_alloc);
    return xbf;
  }
  else
    return 0;
}
