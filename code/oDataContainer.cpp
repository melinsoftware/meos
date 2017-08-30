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

#include "stdafx.h"
#include <set>
#include "oDataContainer.h"
#include "oEvent.h"

#include "gdioutput.h"
#include "xmlparser.h"
#include "Table.h"
#include "meos_util.h"
#include "Localizer.h"
#include "meosException.h"

oDataContainer::oDataContainer(int maxsize) {
  dataPointer = 0;
  dataMaxSize = maxsize;
  stringIndexPointer = 0;
  stringArrayIndexPointer = 2;
}

oDataContainer::~oDataContainer(void) {
}

oDataInfo::oDataInfo() {
  memset(Name, 0, sizeof(Name));
  Index = 0;
  Size = 0;
  Type = 0;
  SubType = 0;
  tableIndex = 0;
  decimalSize = 0;
  decimalScale = 1;
  dataDefiner = 0;
  zeroSortPadding = 0;
  memset(Description, 0, sizeof(Description));
}

oDataInfo::~oDataInfo() {
}

oDataInfo &oDataContainer::addVariableInt(const char *name,
                                    oIntSize isize, const char *description,
                                    const oDataDefiner *dataDef) {
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  odi.Index=dataPointer;
  strcpy_s(odi.Name, name);
  strcpy_s(odi.Description, description);

  if (isize == oIS64)
    odi.Size=sizeof(__int64);
  else
    odi.Size=sizeof(int);

  odi.Type=oDTInt;
  odi.SubType=isize;

  if (dataPointer+odi.Size<=dataMaxSize){
    dataPointer+=odi.Size;
    return addVariable(odi);
  }
  else
    throw std::exception("oDataContainer: Out of bounds.");
}

oDataInfo &oDataContainer::addVariableDecimal(const char *name, const char *descr, int fixedDeci) {
  oDataInfo &odi = addVariableInt(name, oISDecimal, descr);
  odi.decimalSize = fixedDeci;
  int &s = odi.decimalScale;
  s = 1;
  for (int k = 0; k < fixedDeci; k++)
    s*=10;

  return odi;
}

oDataInfo &oDataContainer::addVariableString(const char *name, const char *descr, const oDataDefiner *dataDef) {
  return addVariableString(name, -1, descr, dataDef);
}

oDataInfo &oDataContainer::addVariableString(const char *name, int maxChar,
                                       const char *descr, const oDataDefiner *dataDef)
{
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  strcpy_s(odi.Name,name);
  strcpy_s(odi.Description,descr);
  if (maxChar > 0) {
    odi.Index = dataPointer;
    odi.Size = maxChar+1;
    odi.Type = oDTString;
    odi.SubType = oSSString;

    if (dataPointer+odi.Size<=dataMaxSize){
      dataPointer+=odi.Size;
      return addVariable(odi);
    }
    else
      throw std::exception("oDataContainer: Out of bounds.");
  }
  else {
    odi.Index = stringIndexPointer++;
    odi.Size = 0;
    odi.Type = oDTStringDynamic;
    odi.SubType = oSSString;
    return addVariable(odi);
  }
}



oDataInfo &oDataContainer::addVariableEnum(const char *name, int maxChar, const char *descr,
                     const vector< pair<string, string> > enumValues) {
  oDataInfo &odi = addVariableString(name, maxChar, descr);
  odi.SubType = oSSEnum;
  for (size_t k = 0; k<enumValues.size(); k++)
    odi.enumDescription.push_back(enumValues[k]);
  return odi;
}

oDataInfo &oDataContainer::addVariable(oDataInfo &odi) {
  if (findVariable(odi.Name))
    throw std::exception("oDataContainer: Variable already exist.");

  //index[odi.Name]=odi;
  index.insert(hash(odi.Name), ordered.size());
  ordered.push_back(odi);
  return ordered.back();
}

int oDataContainer::hash(const char *name) {
  int res = 0;
  while(*name != 0) {
    res = 31 * res + *name;
    name++;
  }
  return res;
}

oDataInfo *oDataContainer::findVariable(const char *name) {
/*  map<string, oDataInfo>::iterator it=index.find(Name);

  if (it == index.end())
    return 0;
  else return &(it->second);

  return 0;*/
  int res;
  if (index.lookup(hash(name), res)) {
    return &ordered[res];
  }
  return 0;

}



const oDataInfo *oDataContainer::findVariable(const char *name) const
{
  if (name == 0)
    return 0;
  /*map<string, oDataInfo>::const_iterator it=index.find(Name);

  if (it == index.end())
    return 0;
  else return &(it->second);
  */
  int res;
  if (index.lookup(hash(name), res)) {
    return &ordered[res];
  }
  return 0;
}

void oDataContainer::initData(oBase *ob, int datasize) {
  if (datasize<dataPointer)
    throw std::exception("oDataContainer: Buffer too small.");

  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memset(data, 0, dataPointer);
  memset(oldData, 0, dataPointer);

  if (stringIndexPointer > 0 || stringArrayIndexPointer>2) {
    vector< vector<string> > &str = *strptr;
    str.clear();
    str.resize(stringArrayIndexPointer);
    str[0].resize(stringIndexPointer);
    str[1].resize(stringIndexPointer);
  }
}

bool oDataContainer::setInt(void *data, const char *Name, int V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType == oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  if (*((int *)vd)!=V){
    *((int *)vd)=V;
    return true;
  }
  else return false;//Not modified
}

bool oDataContainer::setInt64(void *data, const char *Name, __int64 V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType != oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  if (*((__int64 *)vd)!=V){
    *((__int64 *)vd)=V;
    return true;
  }
  else return false;//Not modified
}

int oDataContainer::getInt(const void *data, const char *Name) const
{
  const oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  if (odi->SubType == oIS64)
    throw std::exception("oDataContainer: Variable to large.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  return *((int *)vd);
}

__int64 oDataContainer::getInt64(const void *data, const char *Name) const
{
  const oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  LPBYTE vd=LPBYTE(data)+odi->Index;

  if (odi->SubType == oIS64)
    return *((__int64 *)vd);
  else {
    int tmp = *((int *)vd);
    return tmp;
  }
}


bool oDataContainer::setString(oBase *ob, const char *name, const string &V)
{
  oDataInfo *odi=findVariable(name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (odi->Type == oDTString) {
    LPBYTE vd=LPBYTE(data)+odi->Index;

    if (strcmp((char *)vd, V.c_str())!=0){
      strncpy_s((char *)vd, odi->Size, V.c_str(), odi->Size-1);
      return true;
    }
    else return false;//Not modified
  }
  else if (odi->Type == oDTStringDynamic) {
    string &str = (*strptr)[0][odi->Index];
    if (str == V)
      return false; // Same string

    str = V;
    return true;
  }
  else
    throw std::exception("oDataContainer: Variable of wrong type.");

}

const string &oDataContainer::getString(const oBase *ob, const char *Name) const {
  const oDataInfo *odi=findVariable(Name);

  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type == oDTString) {
    LPBYTE vd=LPBYTE(data)+odi->Index;
    string &res = StringCache::getInstance().get();
    res = (char *) vd;
    return res;
  }
  else if (odi->Type == oDTStringDynamic) {
    string &str = (*strptr)[0][odi->Index];
    return str;
  }
  else
    throw std::exception("oDataContainer: Variable of wrong type.");
}


bool oDataContainer::setDate(void *data, const char *Name, const string &V)
{
  oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  int year=0,month=0,day=0;

  sscanf_s(V.c_str(), "%d-%d-%d", &year, &month, &day);

  int C=year*10000+month*100+day;


  LPBYTE vd=LPBYTE(data)+odi->Index;
  if (*((int *)vd)!=C){
    *((int *)vd)=C;
    return true;
  }
  else return false;//Not modified
}

const string &oDataContainer::getDate(const void *data,
                               const char *Name) const
{
  const oDataInfo *odi=findVariable(Name);

  if (!odi)
    throw std::exception("oDataContainer: Variable not found.");

  if (odi->Type!=oDTInt)
    throw std::exception("oDataContainer: Variable of wrong type.");

  LPBYTE vd=LPBYTE(data)+odi->Index;
  int C=*((int *)vd);

  char bf[24];
  if (C%10000!=0 || C==0)
    sprintf_s(bf, "%04d-%02d-%02d", C/10000, (C/100)%100, C%100);
  else
    sprintf_s(bf, "%04d", C/10000);

  string &res = StringCache::getInstance().get();
  res = bf;
  return res;
}

bool oDataContainer::write(const oBase *ob, xmlparser &xml) const {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  xml.startTag("oData");


  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.Type==oDTInt){
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        xml.write(di.Name, nr);
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        xml.write64(di.Name, nr);
      }
    }
    else if (di.Type == oDTString){
      LPBYTE vd=LPBYTE(data)+di.Index;
      xml.write(di.Name, (char *)vd);
    }
    else if (di.Type == oDTStringDynamic) {
      const string &str = (*strptr)[0][di.Index];
      xml.write(di.Name, str);
    }
  }

  xml.endTag();

  return true;
}

void oDataContainer::set(oBase *ob, const xmlobject &xo) {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  xmlList xl;
  xo.getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){

    oDataInfo *odi=findVariable(it->getName());

    if (odi) {
      if (odi->Type == oDTInt){
        LPBYTE vd=LPBYTE(data)+odi->Index;
        if (odi->SubType != oIS64)
          *((int *)vd) = it->getInt();
        else
          *((__int64 *)vd) = it->getInt64();
      }
      else if (odi->Type == oDTString) {
        LPBYTE vd=LPBYTE(data)+odi->Index;
        strncpy_s((char *)vd, odi->Size, it->get(), odi->Size-1);
      }
      else if (odi->Type == oDTStringDynamic) {
        string &str = (*strptr)[0][odi->Index];
        str = it->get();
      }
    }
  }

  allDataStored(ob);
}

void oDataContainer::buildDataFields(gdioutput &gdi) const
{
  vector<string> fields;
  for (size_t k = 0; k < ordered.size(); k++)
    fields.push_back(ordered[k].Name);

  buildDataFields(gdi, fields);
}

void oDataContainer::buildDataFields(gdioutput &gdi, const vector<string> &fields) const
{
  for (size_t k=0;k<fields.size();k++) {
    //map<string, oDataInfo>::const_iterator it=index.find(fields[k]);
    const oDataInfo *odi = findVariable(fields[k].c_str());

    //if (it==index.end())
    if (odi == 0)
      throw std::exception( ("Bad key: " + fields[k]).c_str());

    const oDataInfo &di=*odi;
    string Id=di.Name+string("_odc");

    if (di.Type==oDTInt){
      if (di.SubType == oISDate || di.SubType == oISTime)
        gdi.addInput(Id, "", 10, 0, string(di.Description) + ":");
      else
        gdi.addInput(Id, "", 6, 0, string(di.Description) + ":");
    }
    else if (di.Type==oDTString){
      gdi.addInput(Id, "", min(di.Size+2, 30), 0, string(di.Description) +":");
    }
    else if (di.Type==oDTStringDynamic){
      gdi.addInput(Id, "", 30, 0, string(di.Description) +":");
    }
  }
}

int oDataContainer::getDataAmountMeasure(const void *data) const
{
  int amount = 0;
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      int nr;
      memcpy(&nr, vd, sizeof(int));
      if (nr != 0)
        amount++;
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      amount += strlen((char *)vd);
    }

  }
  return amount;
}

void oDataContainer::fillDataFields(const oBase *ob, gdioutput &gdi) const
{
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    string Id=di.Name+string("_odc");
    if (di.Type==oDTInt){
      LPBYTE vd=LPBYTE(data)+di.Index;

      if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        if (di.SubType == oISCurrency) {
          if (strcmp(di.Name, "CardFee") == 0 && nr == -1)
            nr = 0; //XXX CardFee hack. CardFee = 0 is coded as -1
          gdi.setText(Id.c_str(), ob->getEvent()->formatCurrency(nr));
        }
        else {
          char bf[64];
          formatNumber(nr, di, bf);
          gdi.setText(Id.c_str(), bf);
        }
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        char bf[16];
        oBase::converExtIdentifierString(nr, bf);
        gdi.setText(Id.c_str(), bf);
      }
    }
    else if (di.Type==oDTString){
      LPBYTE vd=LPBYTE(data)+di.Index;
      gdi.setText(Id.c_str(), (char *)vd);
    }
    else if (di.Type==oDTStringDynamic){
      const string &str = (*strptr)[0][di.Index];
      gdi.setText(Id.c_str(), str);
    }
  }
}

bool oDataContainer::saveDataFields(oBase *ob, gdioutput &gdi) {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    string Id=di.Name+string("_odc");

    if (!gdi.hasField(Id)) {
      continue;
    }
    if (di.Type==oDTInt){
      int no = 0;
      if (di.SubType == oISCurrency) {
        no = ob->getEvent()->interpretCurrency(gdi.getText(Id.c_str()));
      }
      else if (di.SubType == oISDate) {
        no = convertDateYMS(gdi.getText(Id.c_str()), true);
      }
      else if (di.SubType == oISTime) {
        no = convertAbsoluteTimeHMS(gdi.getText(Id.c_str()), -1);
      }
      else if (di.SubType == oISDecimal) {
        string str = gdi.getText(Id.c_str());
        for (size_t k = 0; k < str.length(); k++) {
          if (str[k] == ',') {
            str[k] = '.';
            break;
          }
        }
        double val = atof(str.c_str());
        no = int(di.decimalScale * val);
      }
      else {
        no = gdi.getTextNo(Id.c_str());
      }

      LPBYTE vd=LPBYTE(data)+di.Index;
      int oldNo = *((int *)vd);
      if (oldNo != no) {
        *((int *)vd)=no;
        ob->updateChanged();
      }
    }
    else if (di.Type == oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      string oldS = (char *)vd;
      string newS = gdi.getText(Id.c_str());
      if (oldS != newS) {
        strncpy_s((char *)vd, di.Size, newS.c_str(), di.Size-1);
        ob->updateChanged();
      }
    }
    else if (di.Type == oDTStringDynamic) {
      string &oldS = (*strptr)[0][di.Index];
      string newS = gdi.getText(Id.c_str());
      if (oldS != newS) {
        oldS = newS;
        ob->updateChanged();
      }
    }
  }

  return true;
}

string oDataContainer::C_INT64(const string &name)
{
  return " "+name+" BIGINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_INT(const string &name)
{
  return " "+name+" INT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_SMALLINT(const string &name)
{
  return " "+name+" SMALLINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_TINYINT(const string &name)
{
  return " "+name+" TINYINT NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_SMALLINTU(const string &name)
{
  return " "+name+" SMALLINT UNSIGNED NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_TINYINTU(const string &name)
{
  return " "+name+" TINYINT UNSIGNED NOT NULL DEFAULT 0, ";
}

string oDataContainer::C_STRING(const string &name, int len)
{
  if (len>0) {
    char bf[16];
    sprintf_s(bf, "%d", len);
    return " "+name+" VARCHAR("+ bf +") NOT NULL DEFAULT '', ";
  }
  else {
    return " "+name+" MEDIUMTEXT NOT NULL, ";
  }
}

string oDataContainer::SQL_quote(const char *in)
{
  char out[256];
  int o=0;

  while(*in && o<250){
    if (*in=='\'')
      out[o++]='\'';
    if (*in=='\\')
      out[o++]='\\';
    out[o++]=*in;

    in++;
  }
  out[o]=0;

  return out;
}

string oDataContainer::generateSQLDefinition(const std::set<string> &exclude) const {
  string sql;
  bool addSyntx = !exclude.empty();

  for (size_t k = 0; k < ordered.size(); k++) {
    if (exclude.count(ordered[k].Name) == 0) {
      if (addSyntx)
         sql += "ADD COLUMN ";

      const oDataInfo &di = ordered[k];
      string name = di.Name;
      if (di.Type==oDTInt){
        if (di.SubType==oIS32 || di.SubType==oISDate || di.SubType==oISCurrency ||
           di.SubType==oISTime || di.SubType==oISDecimal)
          sql+=C_INT(name);
        else if (di.SubType==oIS16)
          sql+=C_SMALLINT(name);
        else if (di.SubType==oIS8)
          sql+=C_TINYINT(name);
        else if (di.SubType==oIS64)
          sql+=C_INT64(name);
        else if (di.SubType==oIS16U)
          sql+=C_SMALLINTU(name);
        else if (di.SubType==oIS8U)
          sql+=C_TINYINTU(name);
      }
      else if (di.Type==oDTString){
        sql+=C_STRING(name, di.Size-1);
      }
      else if (di.Type==oDTStringDynamic || di.Type==oDTStringArray){
        sql+=C_STRING(name, -1);
      }
    }
  }

  if (addSyntx && !sql.empty())
    return sql.substr(0, sql.length() - 2); //Remove trailing comma-space
  else
    return sql;
}

bool oDataContainer::isModified(const oDataInfo &di,
                                const void *data,
                                const void *oldData,
                                vector< vector<string> > *strptr) const {

  if (di.Type == oDTInt) {
    LPBYTE vd=LPBYTE(data)+di.Index;
    LPBYTE vdOld=LPBYTE(oldData)+di.Index;
    if (di.SubType != oIS64) {
      return memcmp(vd, vdOld, 4) != 0;
    }
    else {
      return memcmp(vd, vdOld, 8) != 0;
    }
  }
  else if (di.Type == oDTString){
    char * vd=(char *)(data)+di.Index;
    char * vdOld=(char *)(oldData)+di.Index;
    return strcmp(vd, vdOld) != 0;
  }
  else if (di.Type == oDTStringDynamic){
    const string &newS = (*strptr)[0][di.Index];
    const string &oldS = (*strptr)[1][di.Index];
    return newS != oldS;
  }
  else if (di.Type == oDTStringArray){
    const vector<string> &newS = (*strptr)[di.Index];
    const vector<string> &oldS = (*strptr)[di.Index+1];
    return newS != oldS;
  }
  else
    return true;
}

void oDataContainer::allDataStored(const oBase *ob) {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memcpy(oldData, data, ob->getDISize());
  if (stringIndexPointer > 0 || stringArrayIndexPointer > 2) {
    for (size_t k = 0; k < stringArrayIndexPointer; k+=2) {
      (*strptr)[k+1] = (*strptr)[k];
    }
  }
}

string oDataContainer::generateSQLSet(const oBase *ob, bool forceSetAll) const {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  string sql;
  char bf[256];

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (!forceSetAll && !isModified(di, data, oldData, strptr)) {
      continue;
    }

    if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType == oIS8U) {
        sprintf_s(bf, ", %s=%u", di.Name, (*((int *)vd))&0xFF);
        sql+=bf;
      }
      else if (di.SubType == oIS16U) {
        sprintf_s(bf, ", %s=%u", di.Name, (*((int *)vd))&0xFFFF);
        sql+=bf;
      }
      else if (di.SubType == oIS8) {
        char r = (*((int *)vd))&0xFF;
        sprintf_s(bf, ", %s=%d", di.Name, (int)r);
        sql+=bf;
      }
      else if (di.SubType == oIS16) {
        short r = (*((int *)vd))&0xFFFF;
        sprintf_s(bf, ", %s=%d", di.Name, (int)r);
        sql+=bf;
      }
      else if (di.SubType != oIS64) {
        sprintf_s(bf, ", %s=%d", di.Name, *((int *)vd));
        sql+=bf;
      }
      else {
        char tmp[32];
        _i64toa_s(*((__int64 *)vd), tmp, 32, 10);
        sprintf_s(bf, ", %s=%s", di.Name, tmp);
        sql+=bf;
      }
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      sprintf_s(bf, ", %s='%s'", di.Name, SQL_quote((char *)vd).c_str());
      sql+=bf;
    }
    else if (di.Type==oDTStringDynamic) {
      const string &str = (*strptr)[0][di.Index];
      sprintf_s(bf, ", %s='%s'", di.Name, SQL_quote(str.c_str()).c_str());
      sql+=bf;
    }
    else if (di.Type==oDTStringArray) {
      const string str = encodeArray((*strptr)[di.Index]);
      sprintf_s(bf, ", %s='%s'", di.Name, SQL_quote(str.c_str()).c_str());
      sql+=bf;
    }
  }
  return sql;
}


void oDataContainer::getVariableInt(const void *data,
                                    list<oVariableInt> &var) const {
  var.clear();

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (di.Type == oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;

      oVariableInt vi;
      memcpy(vi.name, di.Name, sizeof(vi.name));
      if (di.SubType != oIS64)
        vi.data32 = (int *)vd;
      else
        vi.data64 = (__int64 *)vd;
      var.push_back(vi);
    }
  }
}


void oDataContainer::getVariableString(const oBase *ob,
                                       list<oVariableString> &var) const
{
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  var.clear();

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (di.Type == oDTString){
      char * vd=(char *)(data)+di.Index;

      oVariableString vs(vd, di.Size);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
    else if (di.Type == oDTStringDynamic) {
      oVariableString vs((*strptr)[0], di.Index);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
    else if (di.Type == oDTStringArray) {
      oVariableString vs((*strptr)[di.Index]);
      memcpy(vs.name, di.Name, sizeof(vs.name));
      var.push_back(vs);
    }
  }
}

oDataInterface oDataContainer::getInterface(void *data, int datasize, oBase *ob)
{
  if (datasize<dataPointer) throw std::exception("Out Of Bounds.");

  return oDataInterface(this, data, ob);
}

oDataConstInterface oDataContainer::getConstInterface(const void *data, int datasize,
                                                      const oBase *ob) const
{
  if (datasize<dataPointer) throw std::exception("Out Of Bounds.");

  return oDataConstInterface(this, data, ob);
}


oDataInterface::oDataInterface(oDataContainer *odc, void *data, oBase *ob)
{
  oB=ob;
  oDC=odc;
  Data=data;
}

oDataConstInterface::~oDataConstInterface(void)
{
}

oDataConstInterface::oDataConstInterface(const oDataContainer *odc, const void *data, const oBase *ob)
{
  oB=ob;
  oDC=odc;
  Data=data;
}

oDataInterface::~oDataInterface(void)
{
}


void oDataContainer::buildTableCol(Table *table)
{
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    oDataInfo &di=ordered[kk];

    if (di.dataDefiner)  {
      int w = strlen(di.Description)*6;
      di.tableIndex = di.dataDefiner->addTableColumn(table, di.Description, w);
    }
    else if (di.Type == oDTInt) {
      bool right = di.SubType == oISCurrency;
      bool numeric = di.SubType != oISDate && di.SubType != oISTime;
      int w;
      if (di.SubType == oISDecimal)
        w = max( (di.decimalSize+4)*10, 70);
      else
        w = 70;

      w = max(int(strlen(di.Description))*6, w);
      di.tableIndex = table->addColumn(di.Description, w, numeric, right);
    }
    else if (di.Type == oDTString) {
      int w = max(max(di.Size+1, int(strlen(di.Description)))*6, 70);

      for (size_t k = 0; k < di.enumDescription.size(); k++)
        w = max<int>(w, lang.tl(di.enumDescription[k].second).length() * 6);

      if (di.zeroSortPadding)
        di.tableIndex = table->addColumnPaddedSort(di.Description, w, di.zeroSortPadding, true);

      else
        di.tableIndex = table->addColumn(di.Description, w, false);

    }
    else if (di.Type == oDTStringDynamic) {
      int w = 64*6;
      di.tableIndex = table->addColumn(di.Description, w, false);
    }
  }
}

bool oDataContainer::formatNumber(int nr, const oDataInfo &di, char bf[64]) const {
  if (di.SubType == oISDate) {
    if (nr>0) {
      sprintf_s(bf, 64, "%d-%02d-%02d", nr/(100*100), (nr/100)%100, nr%100);
    }
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  }
  else if (di.SubType == oISTime) {
    if (nr>0 && nr<(30*24*3600)) {
      if (nr < 24*3600)
        sprintf_s(bf, 64, "%02d:%02d:%02d", nr/3600, (nr/60)%60, nr%60);
      else {
        int days = nr / (24*3600);
        nr = nr % (24*3600);
        sprintf_s(bf, 64, "%d+%02d:%02d:%02d", days, nr/3600, (nr/60)%60, nr%60);
      }
    }
    else {
      bf[0] = '-';
      bf[1] = 0;
    }
    return true;
  }
  else if (di.SubType == oISDecimal) {
    if (nr) {
      int whole = nr / di.decimalScale;
      int part = nr - whole * di.decimalScale;
      string deci = ",";
      string ptrn = "%d" + deci + "%0" + itos(di.decimalSize) + "d";
      sprintf_s(bf, 64, ptrn.c_str(), whole, abs(part));
    }
    else
      bf[0] = 0;

    return true;
  }
  else {
    if (nr)
      sprintf_s(bf, 64, "%d", nr);
    else
      bf[0] = 0;
    return true;
  }
}

int oDataContainer::fillTableCol(const oBase &owner, Table &table, bool canEdit) const {
  void *data, *oldData;
  vector< vector<string> > *strptr;
  owner.getDataBuffers(data, oldData, strptr);

  int nextIndex = 0;
  char bf[64];
  oBase &ob = *(oBase *)&owner;
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];
    if (di.dataDefiner != 0) {
      table.set(di.tableIndex, ob, 1000+di.tableIndex, di.dataDefiner->formatData(&ob), canEdit);
    }
    else if (di.Type==oDTInt) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType != oIS64) {
        int nr;
        memcpy(&nr, vd, sizeof(int));
        if (di.SubType == oISCurrency) {
          table.set(di.tableIndex, ob, 1000+di.tableIndex, ob.getEvent()->formatCurrency(nr), canEdit);
        }
        else {
          formatNumber(nr, di, bf);
          table.set(di.tableIndex, ob, 1000+di.tableIndex, bf, canEdit);
        }
      }
      else {
        __int64 nr;
        memcpy(&nr, vd, sizeof(__int64));
        char bf[16];
        oBase::converExtIdentifierString(nr, bf);
        table.set(di.tableIndex, ob, 1000+di.tableIndex, bf, canEdit);
      }
    }
    else if (di.Type==oDTString) {
      LPBYTE vd=LPBYTE(data)+di.Index;
      if (di.SubType == oSSString || !canEdit) {
        table.set(di.tableIndex, *((oBase*)&owner), 1000+di.tableIndex, (char *)vd, canEdit, cellEdit);
      }
      else {
        string str((char *)vd);
        for (size_t k = 0; k<di.enumDescription.size(); k++) {
          if ( str == di.enumDescription[k].first ) {
            str = lang.tl(di.enumDescription[k].second);
            break;
          }
        }
        table.set(di.tableIndex, *((oBase*)&owner), 1000+di.tableIndex, str, true, cellSelection);
      }
    }
    else if (di.Type == oDTStringDynamic) {
      const string &str = (*strptr)[0][di.Index];
      table.set(di.tableIndex, *((oBase*)&owner), 1000+di.tableIndex, str, canEdit, cellEdit);
    }
    nextIndex = di.tableIndex + 1;
  }
  return nextIndex;
}

bool oDataContainer::inputData(oBase *ob, int id,
                               const string &input, int inputId,
                               string &output, bool noUpdate)
{
  void *data, *oldData;
  vector< vector<string> > *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di=ordered[kk];

    if (di.tableIndex+1000==id) {
      if (di.dataDefiner) {
        const string &src = di.dataDefiner->formatData(ob);
        output = di.dataDefiner->setData(ob, input);
        bool ch = output != src;
        if (ch && noUpdate == false)
          ob->synchronize(true);
        return ch;
      }
      else if (di.Type==oDTInt) {
        LPBYTE vd=LPBYTE(data)+di.Index;

        int no = 0;
        if (di.SubType == oISCurrency) {
          no = ob->getEvent()->interpretCurrency(input);
        }
        else if (di.SubType == oISDate) {
          no = convertDateYMS(input, true);
        }
        else if (di.SubType == oISTime) {
          no = convertAbsoluteTimeHMS(input, -1);
        }
        else if (di.SubType == oISDecimal) {
          string str = input;
          for (size_t k = 0; k < str.length(); k++) {
            if (str[k] == ',') {
              str[k] = '.';
              break;
            }
          }
          double val = atof(str.c_str());
          no = int(di.decimalScale * val);
        }
        else if (di.SubType == oIS64) {
          //__int64 no64 = _atoi64(input.c_str());
          __int64 k64;
          memcpy(&k64, vd, sizeof(__int64));
          __int64 no64 = oBase::converExtIdentifierString(input);

          memcpy(vd, &no64, sizeof(__int64));
          __int64 out64 = no64;
          if (k64 != no64) {
            ob->updateChanged();
            if (noUpdate == false)
              ob->synchronize(true);

            memcpy(&out64, vd, sizeof(__int64));
          }
          //output = itos(out64);
          char outbf[16];
          oBase::converExtIdentifierString(out64, outbf);
          output = outbf;

          return k64 != no64;
        }
        else
          no = atoi(input.c_str());

        int k;
        memcpy(&k, vd, sizeof(int));
        memcpy(vd, &no, sizeof(int));
        char bf[128];
        int outN = no;

        if (k != no) {
          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          memcpy(&outN, vd, sizeof(int));
        }

        formatNumber(outN, di, bf);
        output = bf;
        return k != no;
      }
      else if (di.Type==oDTString) {
        LPBYTE vd=LPBYTE(data)+di.Index;
        const char *str = input.c_str();

        if (di.SubType == oSSEnum) {
          size_t ix = inputId-1;
          if (ix < di.enumDescription.size()) {
            str = di.enumDescription[ix].first.c_str();
          }
        }

        if (strcmp((char *)vd, str)!=0) {
          strncpy_s((char *)vd, di.Size, str, di.Size-1);

          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          if (di.SubType == oSSEnum) {
            size_t ix = inputId-1;
            if (ix < di.enumDescription.size()) {
              output = lang.tl(di.enumDescription[ix].second);
              // This might be incorrect if data was changed on server,
              // but this issue is minor, I think: conflicts have no resolution
              // Anyway, the row will typically be reloaded
            }
            else
              output=(char *)vd;
          }
          else
            output=(char *)vd;
          return true;
        }
        else
          output=input;

        return false;
      }
      else if (di.Type == oDTStringDynamic) {
        string &vd = (*strptr)[0][di.Index];

        if (vd != input) {
          vd = input;
          ob->updateChanged();
          if (noUpdate == false)
            ob->synchronize(true);

          output = (*strptr)[0][di.Index];
          return true;
        }
        else
          output=input;

        return false;
      }
    }
  }
  return true;
}

void oDataContainer::fillInput(const void *data, int id, const char *name,
                               vector< pair<string, size_t> > &out, size_t &selected) const {


  const oDataInfo * info = findVariable(name);

  if (!info) {
    for (size_t kk = 0; kk < ordered.size(); kk++) {
      const oDataInfo &di=ordered[kk];
      if (di.tableIndex+1000==id && di.Type == oDTString && di.SubType == oSSEnum) {
        info = &di;
        break;
      }
    }
  }

  if (info && info->Type == oDTString && info->SubType == oSSEnum) {
    char *vd = (char *)(LPBYTE(data)+info->Index);

    selected = -1;
    for (size_t k = 0; k < info->enumDescription.size(); ++k) {
      out.push_back(make_pair(lang.tl(info->enumDescription[k].second), k+1));
      if (info->enumDescription[k].first == string(vd))
        selected = k+1;
    }
  }
  else
    throw meosException("Invalid enum");
}

bool oDataContainer::setEnum(oBase *ob, const char *name, int selectedIndex) {
  const oDataInfo * info = findVariable(name);

  if (info  && info->Type == oDTString && info->SubType == oSSEnum) {
    if (size_t(selectedIndex - 1) < info->enumDescription.size()) {
      return setString(ob, name, info->enumDescription[selectedIndex-1].first);
    }
  }
  throw meosException("Invalid enum");
}

bool oVariableString::store(const char *in) {
  if (data) {
    char cb[1024];
    strncpy_s(cb, maxSize, in, maxSize-1);
    if (strcmp(cb, data) != 0) {
      strncpy_s(data, maxSize, cb, maxSize-1);
      return true;
    }
    return false;
  }
  else {
    vector<string> &str = *strData;
    if (strIndex>=0) {
      if (str[strIndex] != in) {
        str[strIndex] = in;
        return true;
      }
      else
        return false;
    }
  }

  return false;
}

string oDataContainer::encodeArray(const vector<string> &input) {
  return "";//XXX
}

void oDataContainer::decodeArray(const string &input, vector<string> &output) {
}
