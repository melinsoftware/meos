#pragma once

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

#include <map>
#include <vector>
#include <set>

#include "oBase.h"
#include "inthashmap.h"
#include "TableType.h"

class InputInfo;

constexpr int MaxVarNameLength = 28;

class oDataDefiner {
public:
  virtual ~oDataDefiner() {}
  virtual const wstring &formatData(const oBase *obj, int index) const = 0;
  virtual pair<int, bool> setData(oBase *obj, int index, const wstring &input, wstring &output, int inputId) const = 0;
  virtual void fillInput(const oBase *obj, int index, vector<pair<wstring, size_t>> &out, size_t &selected) const {}

  /** Used to define/add the table column in the table*/
  virtual TableColSpec addTableColumn(Table *table, const string &description, int minWidth) const = 0;
  virtual void prepare(oEvent *oe) const {}

  // Return false to be cell read-only
  virtual bool canEdit(int index) const { return true; }

  // Return the desired cell type
  virtual CellType getCellType(int index) const;
};

/** Listen and act on data change*/
class oDataNotifier {
public:
  virtual ~oDataNotifier() = default;
  /** Notified when integer data changes */
  virtual void notify(oBase* ob, int oldValue, int newValue) = 0;
};

struct oDataInfo {
  char Name[MaxVarNameLength];
  int Index;
  int Size;
  int Type;
  int SubType;
  TableColSpec tableIndex;
  char Description[48];
  int decimalSize;
  int decimalScale;
  vector<pair<wstring, wstring>> enumDescription;
  shared_ptr<oDataDefiner> dataDefiner;
  shared_ptr<oDataNotifier> dataNotifier;
  int zeroSortPadding;
  oDataInfo();
  ~oDataInfo();
};

struct oVariableInt {
  char name[MaxVarNameLength];
  int *data32;
  __int64 *data64;
  oVariableInt() : data32(0), data64(0) {name[0] = 0;}
};

class oVariableString {
  public:
    oVariableString(wchar_t *buff, int size) : data(buff), maxSize(size), strData(0), strIndex(-2) { name[0] = 0; }
    oVariableString(vector<wstring> &vec) : data(0), maxSize(0), strData(&vec), strIndex(-1) { name[0] = 0; }
    oVariableString(vector<wstring> &vec, int position) : data(0), maxSize(0), strData(&vec), strIndex(position) { name[0] = 0; }
    char name[MaxVarNameLength];
    bool store(const wchar_t *str);
  private:
    wchar_t *data;
    int maxSize;
    vector<wstring> *strData;
    int strIndex; //-1 means array, otherwise string in fixed position
};

class oBase;

class xmlparser;
class xmlobject;
class gdioutput;
class oDataInterface;
class oDataConstInterface;

class oDataContainer {
protected:
  enum oDataType{oDTInt=1, oDTString=2, oDTStringDynamic=3, oDTStringArray=4};
  int dataMaxSize;
  int dataPointer;
  size_t stringIndexPointer;
  size_t stringArrayIndexPointer;
  inthashmap index;
  vector<oDataInfo> ordered;

  static int hash(const char *name);

  oDataInfo *findVariable(const char *name);
  const oDataInfo *findVariable(const char *Name) const;
  bool formatNumber(int nr, const oDataInfo &di, wchar_t bf[64]) const;

  static wstring encodeArray(const vector<wstring> &input);
  static void decodeArray(const string &winput, vector<wstring> &output);

  bool isModified(const oDataInfo &di,
                  const void *data,
                  const void *oldData,
                  vector< vector<wstring> > *strptr) const;

  oDataInfo &addVariable(oDataInfo &odi);
  static string C_INT(const string & name);
  static string C_INT64(const string & name);
  static string C_SMALLINT(const string & name);
  static string C_TINYINT(const string & name);
  static string C_SMALLINTU(const string & name);
  static string C_TINYINTU(const string & name);
  static string C_STRING(const string & name, int len);
  static string SQL_quote(const wchar_t *in);
public:
  enum oIntSize {
    oISDecimal = 28, oISTime = 29, oISTimeAdjust = 26, oISCurrency = 30,
    oISDate = 31, oISDateOrYear = 27, oIS64 = 64,
    oIS32 = 32, oIS16 = 16, oIS8 = 8, oIS16U = 17, oIS8U = 9
  };

  enum oStringSubType {oSSString = 0, oSSEnum = 1};
  string generateSQLDefinition(const std::set<string> &exclude) const;
  string generateSQLDefinition() const {
    return generateSQLDefinition(std::set<string>());
  }

  bool merge(oBase &destination, const oBase &source, const oBase *base) const;

  string generateSQLSet(const oBase *ob, bool forceSetAll) const;

  void allDataStored(const oBase *ob);
  void getVariableInt(const void *data, list<oVariableInt> &var) const;
  void getVariableString(const oBase *data, list<oVariableString> &var) const;

  oDataInterface getInterface(void *data, int datasize, oBase *ob);
  oDataConstInterface getConstInterface(const void *data, int datasize,
                                        const oBase *ob) const;

  oDataInfo &addVariableInt(const char *name, oIntSize isize, const char *descr, const shared_ptr<oDataDefiner> &dataDef = nullptr);
  oDataInfo &addVariableDecimal(const char *name, const char *descr, int fixedDeci);
  oDataInfo &addVariableDate(const char *name,  const char *descr){return addVariableInt(name, oISDate, descr);}
  oDataInfo &addVariableCurrency(const char *name,  const char *descr){return addVariableInt(name, oISCurrency, descr);}
  oDataInfo &addVariableString(const char *name, int maxChar, const char *descr, const shared_ptr<oDataDefiner> &dataDef = nullptr);
  oDataInfo &addVariableString(const char *name, const char *descr, const shared_ptr<oDataDefiner> &dataDef = nullptr);

  oDataInfo &addVariableEnum(const char *name, int maxChar, const char *descr,
                                  const vector< pair<wstring, wstring> > enumValues);

  void initData(oBase *ob, int datasize);

  bool isInt(const char *name) const;
  bool isString(const char *name) const;

  bool setInt(oBase *ob, void *data, const char *Name, int V);
  int getInt(const void *data, const char *Name) const;

  bool setInt64(void *data, const char *Name, __int64 V);
  __int64 getInt64(const void *data, const char *Name) const;

  bool setString(oBase *ob, const char *name, const wstring &v);
  const wstring &getString(const oBase *ob, const char *name) const;
  const wstring &formatString(const oBase *ob, const char *name) const;

  bool setDate(void *data, const char *Name, const wstring &V);
  const wstring &getDate(const void *data, const char *name) const;
  int getYear(const void* data, const char* name) const;

  bool write(const oBase *ob, xmlparser &xml) const;
  void set(oBase *ob, const xmlobject &xo);

  // Get a measure of how much data is stored in this record.
  int getDataAmountMeasure(const void *data) const;

  vector<InputInfo *> buildDataFields(gdioutput &gdi, int maxFieldSize) const;
  vector<InputInfo *> buildDataFields(gdioutput &gdi, const vector<string> &fields, int maxFieldSize) const;

  void fillDataFields(const oBase *ob, gdioutput &gdi) const;
  bool saveDataFields(oBase *ob, gdioutput &gdi, std::set<string> &modified);

  int fillTableCol(const oBase &owner, Table &table, bool canEdit) const;
  void buildTableCol(Table *table);
  pair<int, bool> inputData(oBase *ob, int id, const wstring &input, int inputId, wstring &output, bool noUpdate);

  // Use id (table internal) or name
  void fillInput(const oBase *ob, int id, const char *name, vector< pair<wstring, size_t> > &out, size_t &selected) const;

  bool setEnum(oBase *ob, const char *name, int selectedIndex);

  oDataContainer(int maxsize);
  virtual ~oDataContainer(void);

  friend class oDataInterface;
};


class oDataInterface
{
private:
  void *Data;
  oDataContainer *oDC;
  oBase *oB;
public:

  bool merge(const oBase &source, const oBase *base) {
    return oDC->merge(*oB, source, base);
  }

  inline bool setInt(const char *name, int value) {
    if (oDC->setInt(oB, Data, name, value)) {
      oB->updateChanged();
      return true;
    }
    else return false;
  }

  inline bool setInt(const string &name, int value) {
    return setInt(name.c_str(), value);
  }

  inline bool setInt64(const char *Name, __int64 Value)
  {
    if (oDC->setInt64(Data, Name, Value)){
      oB->updateChanged();
      return true;
    }
    else return false;
  }

  bool isInt(const string &name) const {
    return oDC->isInt(name.c_str());
  }

  bool isString(const string &name) const {
    return oDC->isString(name.c_str());
  }

  inline int getInt(const char *Name) const {
    return oDC->getInt(Data, Name);
  }

  inline int getInt(const string &name) const {
    return oDC->getInt(Data, name.c_str());
  }

  inline __int64 getInt64(const char *Name) const
    {return oDC->getInt64(Data, Name);}

  inline bool setStringNoUpdate(const char *name, const wstring &value)
    {return oDC->setString(oB, name, value);}

  inline bool setString(const char *name, const wstring &value) {
    if (oDC->setString(oB, name, value)) {
      oB->updateChanged();
      return true;
    }
    else return false;
  }

  inline bool setString(const string &name, const wstring &value) {
    return setString(name.c_str(), value);
  }

  inline const wstring &getString(const char *name) const {
    return oDC->getString(oB, name);
  }

  inline const wstring &getString(const string &name) const {
    return oDC->getString(oB, name.c_str());
  }

  inline const wstring &formatString(const oBase *oB, const char *name) const {
    return oDC->formatString(oB, name);
  }

  inline bool setDate(const char *Name, const wstring &Value)
  {
    if (oDC->setDate(Data, Name, Value)){
      oB->updateChanged();
      return true;
    }
    else return false;
  }

  inline const wstring &getDate(const char *name) const
    {return oDC->getDate(Data, name);}

  inline int getYear(const char* name) const {
    return oDC->getYear(Data, name);
  }

  inline vector<InputInfo *> buildDataFields(gdioutput &gdi, int maxFieldSize) const
    {return oDC->buildDataFields(gdi, maxFieldSize);}

  inline vector<InputInfo *> buildDataFields(gdioutput &gdi, const vector<string> &fields, int maxFieldSize) const
    {return oDC->buildDataFields(gdi, fields, maxFieldSize);}

  inline void fillDataFields(gdioutput &gdi) const
    {oDC->fillDataFields(oB, gdi);}

  inline bool saveDataFields(gdioutput &gdi, std::set<string> &modified)
    {return oDC->saveDataFields(oB, gdi, modified);}

  inline string generateSQLDefinition() const
    {return oDC->generateSQLDefinition(std::set<string>());}

  inline string generateSQLDefinition(const std::set<string> &exclude) const
    {return oDC->generateSQLDefinition(exclude);}

  inline string generateSQLSet(bool forceSetAll) const
    {return oDC->generateSQLSet(oB, forceSetAll);}

  // Mark all data as stored in db
  inline void allDataStored()
    {return oDC->allDataStored(oB);}

  inline void getVariableInt(list<oVariableInt> &var) const
    {oDC->getVariableInt(Data, var);}

  inline void getVariableString(list<oVariableString> &var) const
    {oDC->getVariableString(oB, var);}

  inline void initData()
    {oDC->initData(oB, oDC->dataMaxSize);}

  inline bool write(xmlparser &xml) const
    {return oDC->write(oB, xml);}

  inline void set(const xmlobject &xo)
    {oDC->set(oB, xo);}

  void fillInput(const char *name, vector< pair<wstring, size_t> > &out, size_t &selected) const {
    oDC->fillInput(oB, -1, name, out, selected);
  }

  bool setEnum(const char *name, int selectedIndex) {
    if (oDC->setEnum(oB, name, selectedIndex) ) {
      oB->updateChanged();
      return true;
    }
    else return false;
  }

  int getDataAmountMeasure() const
    {return oDC->getDataAmountMeasure(Data);}

  oDataInterface(oDataContainer *odc, void *data, oBase *ob);
  ~oDataInterface(void);
};

class oDataConstInterface
{
private:
  const void *Data;
  const oDataContainer *oDC;
  const oBase *oB;
public:

  bool isInt(const string &name) const {
    return oDC->isInt(name.c_str());
  }

  bool isString(const string &name) const {
    return oDC->isString(name.c_str());
  }

  inline int getInt(const char *Name) const {
    return oDC->getInt(Data, Name);
  }

  inline int getInt(const string &name) const {
    return oDC->getInt(Data, name.c_str());
  }

  inline __int64 getInt64(const char *Name) const
    {return oDC->getInt64(Data, Name);}

  inline const wstring &getString(const char *Name) const
    {return oDC->getString(oB, Name);}

  inline const wstring &getString(const string &name) const {
    return oDC->getString(oB, name.c_str());
  }

  inline const wstring &formatString(const oBase *oB, const char *name) const {
    return oDC->formatString(oB, name);
  }

  inline const wstring &getDate(const char *name) const
    {return oDC->getDate(Data, name);}
  
  inline int getYear(const char* name) const {
    return oDC->getYear(Data, name);
  }
  inline __int64 getInt64(const string &name) const
    {return oDC->getInt64(Data, name.c_str());}

  inline const wstring &getDate(const string &name) const
    {return oDC->getDate(Data, name.c_str());}

  inline void buildDataFields(gdioutput &gdi, int maxFieldSize) const
    {oDC->buildDataFields(gdi, maxFieldSize);}

  inline void fillDataFields(gdioutput &gdi, int maxFieldSize) const
    {oDC->fillDataFields(oB, gdi);}

  inline string generateSQLDefinition() const
    {return oDC->generateSQLDefinition(set<string>());}

  inline string generateSQLDefinition(const set<string> &exclude) const
    {return oDC->generateSQLDefinition(exclude);}

  inline string generateSQLSet(bool forceSetAll) const
    {return oDC->generateSQLSet(oB, forceSetAll);}

  inline void getVariableInt(list<oVariableInt> &var) const
    {oDC->getVariableInt(Data, var);}

  inline void getVariableString(list<oVariableString> &var) const
    {oDC->getVariableString(oB, var);}

  inline bool write(xmlparser &xml) const
    {return oDC->write(oB, xml);}

  int getDataAmountMeasure() const
    {return oDC->getDataAmountMeasure(Data);}

  void fillInput(const char *name, vector< pair<wstring, size_t> > &out, size_t &selected) const {
    oDC->fillInput(oB, -1, name, out, selected);
  }

  oDataConstInterface(const oDataContainer *odc, const void *data, const oBase *ob);
  ~oDataConstInterface(void);
};
