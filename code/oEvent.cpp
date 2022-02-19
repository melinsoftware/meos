/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "oDataContainer.h"
#include "MetaList.h"

#include "random.h"
#include "SportIdent.h"

#include "meosException.h"
#include "oFreeImport.h"
#include "TabBase.h"
#include "meos.h"
#include "meos_util.h"
#include "RunnerDB.h"
#include "localizer.h"
#include "progress.h"
#include "intkeymapimpl.hpp"
#include "socket.h"

#include "machinecontainer.h"
#include "MeOSFeatures.h"
#include "generalresult.h"
#include "oEventDraw.h"
#include "MeosSQL.h"
#include "TabAuto.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include "Table.h"

//Version of database
int oEvent::dbVersion = 87;

class RelativeTimeFormatter : public oDataDefiner {
  string name;
public:
  RelativeTimeFormatter(const char *n) : name(n) {}

  const wstring &formatData(const oBase *obj) const override {
    int t = obj->getDCI().getInt(name);
    if (t <= 0)
      return makeDash(L"-");
    return obj->getEvent()->getAbsTime(t);
  }
  pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override {
    int t = obj->getEvent()->getRelativeTime(input);
    obj->getDI().setInt(name.c_str(), t);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class AbsoluteTimeFormatter : public oDataDefiner {
  string name;
public:
  AbsoluteTimeFormatter(const char *n) : name(n) {}

  const wstring &formatData(const oBase *obj) const override {
    int t = obj->getDCI().getInt(name);
    return formatTime(t);
  }
  pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override {
    int t = convertAbsoluteTimeMS(input);
    if (t == NOTIME)
      t = 0;
    obj->getDI().setInt(name.c_str(), t);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class PayMethodFormatter : public oDataDefiner {
  mutable vector< pair<wstring, size_t> > modes;
  mutable map<wstring, int> setCodes;
  mutable long rev;
public:
  PayMethodFormatter() : rev(-1) {}

  void prepare(oEvent *oe) const override {
    oe->getPayModes(modes);
    for (size_t i = 0; i < modes.size(); i++) {
      setCodes[canonizeName(modes[i].first.c_str())] = modes[i].second;
    }
  }

  const wstring &formatData(const oBase *ob) const override {
    if (ob->getEvent()->getRevision() != rev)
      prepare(ob->getEvent());
    int p = ob->getDCI().getInt("Paid");
    if (p == 0)
      return lang.tl("Faktura");
    else {
      int pm = ob->getDCI().getInt("PayMode");
      for (size_t i = 0; i < modes.size(); i++) {
        if (modes[i].second == pm)
          return modes[i].first;
      }
      return _EmptyWString;
    }
  }

  pair<int, bool> setData(oBase *ob, const wstring &input, wstring &output, int inputId) const override {
    auto res = setCodes.find(canonizeName(input.c_str()));
    if (res != setCodes.end()) {
      ob->getDI().setInt("PayMode", res->second);
    }
    output = formatData(ob);
    return make_pair(0, false);
  }

  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, true);
  }
};

class StartGroupFormatter : public oDataDefiner {
  mutable long rev = -1;
  mutable map<int, wstring> sgmap;
  mutable wstring out;

  int static getGroup(const oBase *ob) {
    const oRunner *r = dynamic_cast<const oRunner *>(ob);
    int sg = 0;
    if (r)
      sg = r->getStartGroup(false);
    else {
      const oClub *c = dynamic_cast<const oClub *>(ob);
      if (c)
        sg = c->getStartGroup();
    }
    return sg;
  }

public:
  StartGroupFormatter() {}

  void prepare(oEvent *oe) const override {
    auto &sg = oe->getStartGroups(true);
    for (auto &g : sg) {
      int t = g.second.firstStart;
      sgmap[g.first] = oe->getAbsTimeHM(t);
    }
  }
  
  const wstring &formatData(const oBase *ob) const override {
    if (ob->getEvent()->getRevision() != rev)
      prepare(ob->getEvent());
    int sg = getGroup(ob);
    if (sg > 0) {
      auto res = sgmap.find(sg);
      if (res != sgmap.end())
        out = itow(sg) + L" (" + res->second + L")";
      else
        out = itow(sg) + L" (??)";

      return out;
    }
    else
      return _EmptyWString;
  }

  pair<int, bool> setData(oBase *ob, const wstring &input, wstring &output, int inputId) const override {
    int g = inputId;
    if (inputId <= 0 && !input.empty()) {
      vector<wstring> sIn;
      split(input, L" ", sIn);
      for (wstring &in : sIn) {
        int num = _wtoi(in.c_str());
        if (in.find_first_of(':') != input.npos) {
          int t = ob->getEvent()->convertAbsoluteTime(input);
          if (t > 0) {
            for (auto &sg : ob->getEvent()->getStartGroups(false)) {
              if (sg.second.firstStart == t) {
                g = sg.first;
                break;
              }
            }
          }
        }
        else if (sgmap.count(num)) {
          g = num;
          break;
        }
      }
    }
    oRunner *r = dynamic_cast<oRunner *>(ob);
    if (r) {
      r->setStartGroup(g);
    }
    else {
      oClub *c = dynamic_cast<oClub *>(ob);
      if (c)
        c->setStartGroup(g);
    }
    output = formatData(ob);
    return make_pair(0, false);
  }

  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, false);
  }

  // Return the desired cell type
  CellType getCellType() const {
    return CellType::cellSelection;
  }

  void fillInput(const oBase *obj, vector<pair<wstring, size_t>> &out, size_t &selected) const final {
    if (obj->getEvent()->getRevision() != rev)
      prepare(obj->getEvent());

    int sg = getGroup(obj);

    out.emplace_back(_EmptyWString, 0);
    selected = 0;
    for (auto &v : sgmap) {
      out.emplace_back(v.second, v.first);

      if (sg == v.first)
        selected = sg;
    }
  }
};


class DataHider : public oDataDefiner {
public:

  const wstring &formatData(const oBase *obj) const override {
    return _EmptyWString;
  }
  pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override {
    return make_pair(0, false);
  }
  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return -1;
  }
};

class DataBoolean : public oDataDefiner {
  string attrib;
public:
  DataBoolean(const string &attrib) : attrib(attrib) {}

  const wstring &formatData(const oBase *obj) const override {
    int v = obj->getDCI().getInt(attrib);
    return lang.tl(v ? "true[boolean]" : "false[boolean]");
  }
  pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override {
    bool v = compareStringIgnoreCase(L"true", input) == 0 || _wtoi64(input.c_str())>0;
    if (!v) {
      const wstring &T = lang.tl("true[boolean]");
      v = compareStringIgnoreCase(T, input) == 0;
    }
    obj->getDI().setInt(attrib.c_str(), v);
    output =  formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, true);
  }
};

class ResultModuleFormatter : public oDataDefiner {
public:
  
  const wstring &formatData(const oBase *obj) const override {
    return obj->getDCI().getString("Result");
  }
  pair<int, bool> setData(oBase *obj, const wstring &input, wstring &output, int inputId) const override {
    string tag(input.begin(), input.end());
    dynamic_cast<oClass &>(*obj).setResultModule(tag);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table *table, const string &description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

oEvent::oEvent(gdioutput &gdi):oBase(0), gdibase(gdi)
{
  readOnly = false;
  tLongTimesCached = -1;
  directSocket = 0;
  ZeroTime=0;
  vacantId = 0;
  noClubId = 0;
  dataRevision = 0;
  
  disableRecalculate = false;

  initProperties();

#ifndef MEOSDB
  listContainer = new MetaListContainer(this);
#else
  throw std::exception();
#endif


  tCurrencyFactor = 1;
  tCurrencySymbol = L"kr";
  tCurrencySeparator = L",";
  tCurrencyPreSymbol = false;

  tClubDataRevision = -1;

  nextFreeStartNo = 0;

  SYSTEMTIME st;
  GetLocalTime(&st);

  wchar_t bf[64];
  swprintf_s(bf, 64, L"%d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

  Date=bf;
  ZeroTime=st.wHour*3600;
  oe=this;

  runnerDB = make_shared<RunnerDB>(this);
  meosFeatures = new MeOSFeatures();
  openFileLock = new MeOSFileLock();

  wchar_t cp[ MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(cp, &size);
  clientName = cp;

  isConnectedToServer = false;
  hasPendingDBConnection = false;
  currentNameMode = FirstLast;

  nextTimeLineEvent = 0;
  //These object must be initialized on creation of any oObject,
  //but we need to create (dummy) objects to get the sizeof their
  //oData[]-sets...

  // --- REMEMBER TO UPDATE dvVersion when these are changed.

  oEventData=new oDataContainer(dataSize);
  oEventData->addVariableCurrency("CardFee", "Brickhyra");
  oEventData->addVariableCurrency("EliteFee", "Elitavgift");
  oEventData->addVariableCurrency("EntryFee", "Normalavgift");
  oEventData->addVariableCurrency("YouthFee", "Ungdomsavgift");
  oEventData->addVariableInt("YouthAge", oDataContainer::oIS8U, "Åldersgräns ungdom");
  oEventData->addVariableInt("SeniorAge", oDataContainer::oIS8U, "Åldersgräns äldre");

  oEventData->addVariableString("Account", 30, "Konto");
  oEventData->addVariableDate("PaymentDue", "Sista betalningsdatum");
  oEventData->addVariableDate("OrdinaryEntry", "Ordinarie anmälningsdatum");
  oEventData->addVariableString("LateEntryFactor", 6, "Avgiftshöjning (procent)");

  oEventData->addVariableString("Organizer", "Arrangör");
  oEventData->addVariableString("CareOf", 31, "c/o");

  oEventData->addVariableString("Street", 32, "Adress");
  oEventData->addVariableString("Address", 32, "Postadress");
  oEventData->addVariableString("EMail", "E-post");
  oEventData->addVariableString("Homepage", "Hemsida");
  oEventData->addVariableString("Phone", 32, "Telefon");

  oEventData->addVariableInt("UseEconomy", oDataContainer::oIS8U, "Ekonomi");
  oEventData->addVariableInt("UseSpeaker", oDataContainer::oIS8U, "Speaker");
  oEventData->addVariableInt("SkipRunnerDb", oDataContainer::oIS8U, "Databas");
  oEventData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");

  oEventData->addVariableInt("MaxTime", oDataContainer::oISTime, "Gräns för maxtid");
  oEventData->addVariableInt("DiffTime", oDataContainer::oISTime, "Stämplingsintervall, rogaining-patrull");

  oEventData->addVariableString("PreEvent", 64, "");
  oEventData->addVariableString("PostEvent", 64, "");
  oEventData->addVariableString("ImportStamp", 14, "Stamp");

  // Positive number -> stage number, negative number -> no stage number. Zero = unknown
  oEventData->addVariableInt("EventNumber", oDataContainer::oIS8, "");

  oEventData->addVariableInt("CurrencyFactor", oDataContainer::oIS16, "Valutafaktor");
  oEventData->addVariableString("CurrencySymbol", 5, "Valutasymbol");
  oEventData->addVariableString("CurrencySeparator", 2, "Decimalseparator");
  oEventData->addVariableInt("CurrencyPreSymbol", oDataContainer::oIS8, "Symbolläge");
  oEventData->addVariableString("CurrencyCode", 5, "Valutakod");
  oEventData->addVariableInt("UTC", oDataContainer::oIS8, "UTC");
  
  oEventData->addVariableInt("Analysis", oDataContainer::oIS8, "Utan analys");
  // With split time analysis (0 = default, with analysis, with min/km)
  // bit 1: without analysis
  // bit 2: without min/km
  // bit 4: without result

  oEventData->addVariableString("SPExtra", "Extra rader");
  oEventData->addVariableString("IVExtra", "Fakturainfo");
  oEventData->addVariableString("Features", "Funktioner");
  oEventData->addVariableString("EntryExtra", "Extra rader");
  oEventData->addVariableInt("NumStages", oDataContainer::oIS8, "Antal etapper");
  oEventData->addVariableInt("BibGap", oDataContainer::oIS8U, "Nummerlappshopp");
  oEventData->addVariableInt("LongTimes", oDataContainer::oIS8U, "Långa tider");
  oEventData->addVariableString("PayModes", "Betalsätt");
  oEventData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring");
  oEventData->addVariableDate("InvoiceDate", "Fakturadatum");
  oEventData->addVariableString("StartGroups", "Startgrupper");
  oEventData->addVariableString("MergeTag", 12, "Tag");
  oEventData->addVariableString("MergeInfo", "MergeInfo");
  
  oEventData->initData(this, dataSize);

  oClubData=new oDataContainer(oClub::dataSize);
  oClubData->addVariableInt("District", oDataContainer::oIS32, "Organisation");

  oClubData->addVariableString("ShortName", 8, "Kortnamn");
  oClubData->addVariableString("CareOf", 31, "c/o");
  oClubData->addVariableString("Street", 41, "Gata");
  oClubData->addVariableString("City", 23, "Stad");
  oClubData->addVariableString("State", 23, "Region");
  oClubData->addVariableString("ZIP", 11, "Postkod");
  oClubData->addVariableString("EMail", 64, "E-post");
  oClubData->addVariableString("Phone", 32, "Telefon");
  oClubData->addVariableString("Nationality", 3, "Nationalitet");
  oClubData->addVariableString("Country", 23, "Land");
  oClubData->addVariableString("Type", 20, "Typ");
  oClubData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");

  vector< pair<wstring,wstring> > eInvoice;
  eInvoice.push_back(make_pair(L"E", L"Elektronisk"));
  eInvoice.push_back(make_pair(L"A", L"Elektronisk godkänd"));
  eInvoice.push_back(make_pair(L"P", L"Ej elektronisk"));
  eInvoice.push_back(make_pair(L"", makeDash(L"-")));
  oClubData->addVariableEnum("Invoice", 1, "Faktura", eInvoice);
  oClubData->addVariableInt("InvoiceNo", oDataContainer::oIS16U, "Fakturanummer");
  oClubData->addVariableInt("StartGroup", oDataContainer::oIS32, "Startgrupp", make_shared<StartGroupFormatter>());

  oRunnerData=new oDataContainer(oRunner::dataSize);
  oRunnerData->addVariableCurrency("Fee", "Anm. avgift");
  oRunnerData->addVariableCurrency("CardFee", "Brickhyra");
  oRunnerData->addVariableCurrency("Paid", "Betalat");
  oRunnerData->addVariableInt("PayMode", oDataContainer::oIS8U, "Betalsätt", make_shared<PayMethodFormatter>());
  oRunnerData->addVariableCurrency("Taxable", "Skattad avgift");
  oRunnerData->addVariableInt("BirthYear", oDataContainer::oIS32, "Födelseår");
  oRunnerData->addVariableString("Bib", 8, "Nummerlapp").zeroSortPadding = 5;
  oRunnerData->addVariableInt("Rank", oDataContainer::oIS16U, "Ranking");
  
  oRunnerData->addVariableDate("EntryDate", "Anm. datum");
  oRunnerData->addVariableInt("EntryTime", oDataContainer::oIS32, "Anm. tid",  make_shared<AbsoluteTimeFormatter>("EntryTime"));

  vector< pair<wstring,wstring> > sex;
  sex.push_back(make_pair(L"M", L"Man"));
  sex.push_back(make_pair(L"F", L"Kvinna"));
  sex.push_back(make_pair(L"", makeDash(L"-")));

  oRunnerData->addVariableEnum("Sex", 1, "Kön", sex);
  oRunnerData->addVariableString("Nationality", 3, "Nationalitet");
  oRunnerData->addVariableString("Country", 23, "Land");
  oRunnerData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oRunnerData->addVariableInt("Priority", oDataContainer::oIS8U, "Prioritering");
  oRunnerData->addVariableString("Phone", 20, "Telefon");

  oRunnerData->addVariableInt("RaceId", oDataContainer::oIS32, "Lopp-id", make_shared<oRunner::RaceIdFormatter>());

  oRunnerData->addVariableInt("TimeAdjust", oDataContainer::oIS16, "Tidsjustering");
  oRunnerData->addVariableInt("PointAdjust", oDataContainer::oIS32, "Poängjustering");
  oRunnerData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring");
  oRunnerData->addVariableInt("Shorten", oDataContainer::oIS8U, "Avkortning");
  oRunnerData->addVariableInt("EntrySource", oDataContainer::oIS32, "Källa");
  oRunnerData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oRunnerData->addVariableInt("Reference", oDataContainer::oIS32, "Referens", make_shared<oRunner::RunnerReference>());
  oRunnerData->addVariableInt("NoRestart", oDataContainer::oIS8U, "Ej omstart", make_shared<DataBoolean>("NoRestart"));
  oRunnerData->addVariableString("InputResult", "Tidigare resultat", make_shared<DataHider>());
  oRunnerData->addVariableInt("StartGroup", oDataContainer::oIS32, "Startgrupp", make_shared<StartGroupFormatter>());
  oRunnerData->addVariableInt("Family", oDataContainer::oIS32, "Familj");

  oControlData=new oDataContainer(oControl::dataSize);
  oControlData->addVariableInt("TimeAdjust", oDataContainer::oIS32, "Tidsjustering");
  oControlData->addVariableInt("MinTime", oDataContainer::oIS32, "Minitid");
  oControlData->addVariableDecimal("xpos", "x", 1);
  oControlData->addVariableDecimal("ypos", "y", 1);
  oControlData->addVariableDecimal("latcrd", "Latitud", 6);
  oControlData->addVariableDecimal("longcrd", "Longitud", 6);

  oControlData->addVariableInt("Rogaining", oDataContainer::oIS32, "Poäng");
  oControlData->addVariableInt("Radio", oDataContainer::oIS8U, "Radio");

  oCourseData=new oDataContainer(oCourse::dataSize);
  oCourseData->addVariableInt("NumberMaps", oDataContainer::oIS16, "Kartor");
  oCourseData->addVariableString("StartName", 16, "Start");
  oCourseData->addVariableInt("Climb", oDataContainer::oIS16, "Stigning");
  oCourseData->addVariableInt("RPointLimit", oDataContainer::oIS32, "Poänggräns");
  oCourseData->addVariableInt("RTimeLimit", oDataContainer::oIS32, "Tidsgräns");
  oCourseData->addVariableInt("RReduction", oDataContainer::oIS32, "Poängreduktion");
  oCourseData->addVariableInt("RReductionMethod", oDataContainer::oIS8U, "Reduktionsmetod");

  oCourseData->addVariableInt("FirstAsStart", oDataContainer::oIS8U, "Från första", make_shared<DataBoolean>("FirstAsStart"));
  oCourseData->addVariableInt("LastAsFinish", oDataContainer::oIS8U, "Till sista", make_shared<DataBoolean>("LastAsFinish"));

  oCourseData->addVariableInt("CControl", oDataContainer::oIS16U, "Varvningskontroll"); //Common control index
  oCourseData->addVariableInt("Shorten", oDataContainer::oIS32, "Avkortning"); 
 
  oClassData=new oDataContainer(oClass::dataSize);
  oClassData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oClassData->addVariableString("LongName", 32, "Långt namn");
  oClassData->addVariableInt("LowAge", oDataContainer::oIS8U, "Undre ålder");
  oClassData->addVariableInt("HighAge", oDataContainer::oIS8U, "Övre ålder");
  oClassData->addVariableInt("HasPool", oDataContainer::oIS8U, "Banpool", make_shared<DataBoolean>("HasPool"));
  oClassData->addVariableInt("AllowQuickEntry", oDataContainer::oIS8U, "Direktanmälan", make_shared<DataBoolean>("AllowQuickEntry"));

  oClassData->addVariableString("ClassType", 40, "Klasstyp");

  vector< pair<wstring,wstring> > sexClass;
  sexClass.push_back(make_pair(L"M", L"Män"));
  sexClass.push_back(make_pair(L"F", L"Kvinnor"));
  sexClass.push_back(make_pair(L"B", L"Alla"));
  sexClass.push_back(make_pair(L"", makeDash(L"-")));

  oClassData->addVariableEnum("Sex", 1, "Kön", sexClass);
  oClassData->addVariableString("StartName", 16, "Start");
  oClassData->addVariableInt("StartBlock", oDataContainer::oIS8U, "Block");
  oClassData->addVariableInt("NoTiming", oDataContainer::oIS8U, "Ej tidtagning", make_shared<DataBoolean>("NoTiming"));
  oClassData->addVariableInt("FreeStart", oDataContainer::oIS8U, "Fri starttid", make_shared<DataBoolean>("FreeStart"));
  oClassData->addVariableInt("IgnoreStart", oDataContainer::oIS8U, "Ej startstämpling", make_shared<DataBoolean>("IgnoreStart"));

  oClassData->addVariableInt("FirstStart", oDataContainer::oIS32, "Första start", make_shared<RelativeTimeFormatter>("FirstStart"));
  oClassData->addVariableInt("StartInterval", oDataContainer::oIS16, "Intervall", make_shared<AbsoluteTimeFormatter>("StartInterval"));
  oClassData->addVariableInt("Vacant", oDataContainer::oIS8U, "Vakanser");
  oClassData->addVariableInt("Reserved", oDataContainer::oIS16U, "Extraplatser");

  oClassData->addVariableCurrency("ClassFee", "Anm. avgift");
  oClassData->addVariableCurrency("HighClassFee", "Efteranm. avg.");
  oClassData->addVariableCurrency("ClassFeeRed", "Reducerad avg.");
  oClassData->addVariableCurrency("HighClassFeeRed", "Red. avg. efteranm.");

  oClassData->addVariableInt("SortIndex", oDataContainer::oIS32, "Sortering");
  oClassData->addVariableInt("MaxTime", oDataContainer::oISTime, "Maxtid");

  vector<pair<wstring, wstring>> statusClass;
  oClass::fillClassStatus(statusClass);
  oClassData->addVariableEnum("Status", 2, "Status", statusClass);
  oClassData->addVariableInt("DirectResult", oDataContainer::oIS8, "Resultat vid målstämpling", make_shared<DataBoolean>("DirectResult"));
  oClassData->addVariableString("Bib", 8, "Nummerlapp");

  vector< pair<wstring,wstring> > bibMode;
  bibMode.push_back(make_pair(L"", L"Från lag"));
  bibMode.push_back(make_pair(L"A", L"Lag + sträcka"));
  bibMode.push_back(make_pair(L"F", L"Fritt"));

  oClassData->addVariableEnum("BibMode", 1, "Nummerlappshantering", bibMode);
  oClassData->addVariableInt("Unordered", oDataContainer::oIS8U, "Oordnade parallella");
  oClassData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oClassData->addVariableInt("Locked", oDataContainer::oIS8U, "Låst gaffling", make_shared<DataBoolean>("Locked"));
  oClassData->addVariableString("Qualification", "Kvalschema", make_shared<DataHider>());
  oClassData->addVariableInt("NumberMaps", oDataContainer::oIS16, "Kartor");
  oClassData->addVariableString("Result", 24, "Result module", make_shared<ResultModuleFormatter>());
  oClassData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring", make_shared<DataHider>());

  oTeamData = new oDataContainer(oTeam::dataSize);
  oTeamData->addVariableCurrency("Fee", "Anm. avgift");
  oTeamData->addVariableCurrency("Paid", "Betalat");
  oTeamData->addVariableInt("PayMode", oDataContainer::oIS8U, "Betalsätt");
  oTeamData->addVariableCurrency("Taxable", "Skattad avgift");
  oTeamData->addVariableDate("EntryDate", "Anm. datum");
  oTeamData->addVariableInt("EntryTime", oDataContainer::oIS32, "Anm. tid", make_shared<AbsoluteTimeFormatter>("EntryTime"));
  oTeamData->addVariableString("Nationality", 3, "Nationalitet");
  oTeamData->addVariableString("Country", 23, "Land");
  oTeamData->addVariableString("Bib", 8, "Nummerlapp").zeroSortPadding = 5;
  oTeamData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oTeamData->addVariableInt("Priority", oDataContainer::oIS8U, "Prioritering");
  oTeamData->addVariableInt("SortIndex", oDataContainer::oIS16, "Sortering");
  oTeamData->addVariableInt("TimeAdjust", oDataContainer::oIS16, "Tidsjustering");
  oTeamData->addVariableInt("PointAdjust", oDataContainer::oIS32, "Poängjustering");
  oTeamData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring");
  oTeamData->addVariableInt("EntrySource", oDataContainer::oIS32, "Källa");
  oTeamData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oTeamData->addVariableInt("NoRestart", oDataContainer::oIS8U, "Ej omstart");
  oTeamData->addVariableString("InputResult", "Tidigare resultat", make_shared<DataHider>());

  generalResults.push_back(GeneralResultCtr("atcontrol", L"Result at a control", make_shared<ResultAtControl>()));
  generalResults.push_back(GeneralResultCtr("totatcontrol", L"Total/team result at a control", make_shared<TotalResultAtControl>()));

  currentClientCS = 0;
  memset(CurrentFile, 0, sizeof(CurrentFile));
}

oEvent::~oEvent()
{
  //Clean up things in the right order.
  clear();
  runnerDB.reset();
  delete meosFeatures;
  meosFeatures = 0;

  delete oEventData;
  delete oRunnerData;
  delete oClubData;
  delete oControlData;
  delete oCourseData;
  delete oClassData;
  delete oTeamData;

  delete openFileLock;
  delete listContainer;

  return;
}

void oEvent::initProperties() {
  setProperty("Language", getPropertyString("Language", L"103"));

  setProperty("Interactive", getPropertyString("Interactive", L"1"));
  setProperty("Database", getPropertyString("Database", L"1"));

  // Setup some defaults
  getPropertyInt("SplitLateFees", false);
  getPropertyInt("DirectPort", 21338);
  getPropertyInt("UseHourFormat", 1);
  getPropertyInt("UseDirectSocket", true);
  getPropertyInt("UseEventorUTC", 0);
  getPropertyInt("UseHourFormat", 1);
  getPropertyInt("NameMode", FirstLast);
}

void oEvent::listProperties(bool userProps, vector< pair<string, PropertyType> > &propNames) const {
  
  
  set<string> filter;
  if (userProps) {
    filter.insert("Language");
    filter.insert("apikey");
    filter.insert("Colors");
    filter.insert("xpos");
    filter.insert("ypos");
    filter.insert("xsize");
    filter.insert("ysize");
    filter.insert("ListType");
    filter.insert("LastCompetition");
    filter.insert("DrawTypeDefault");
    filter.insert("Email"); 
    filter.insert("TextSize");
    filter.insert("PayModes");
    filter.insert("ReadVoltageExp");
  }

  // Boolean and integer properties
  set<string> b, i;

  // Booleans
  b.insert("AdvancedClassSettings");
  b.insert("AutoTie");
  b.insert("CurrencyPreSymbol");
  b.insert("Database");
  b.insert("Interactive");
  b.insert("intertime");
  b.insert("ManualInput");
  b.insert("PageBreak");
  b.insert("RentCard");
  b.insert("SpeakerShortNames");
  b.insert("splitanalysis");
  b.insert("UseDirectSocket");
  b.insert("UseEventor");
  b.insert("UseEventorUTC");
  b.insert("UseHourFormat");
  b.insert("SplitLateFees");
  b.insert("WideSplitFormat");
  b.insert("pagebreak");
  b.insert("FirstTime");
  b.insert("ExportCSVSplits");
  b.insert("DrawInterlace");
  b.insert("PlaySound");
  b.insert("showheader");

  // Integers
  i.insert("YouthFee");
  i.insert("YouthAge");
  i.insert("TextSize");
  i.insert("SynchronizationTimeOut");
  i.insert("SeniorAge");
  i.insert("Port");
  i.insert("MaximumSpeakerDelay");
  i.insert("FirstInvoice");
  i.insert("EntryFee");
  i.insert("EliteFee");
  i.insert("DirectPort");
  i.insert("DatabaseUpdate");
  i.insert("ControlTo");
  i.insert("ControlFrom");
  i.insert("CardFee");
  i.insert("addressypos");
  i.insert("addressxpos");
  i.insert("AutoSaveTimeOut");
  i.insert("ServicePort");
  i.insert("CodePage");

  propNames.clear();
  for(map<string, wstring>::const_iterator it = eventProperties.begin(); 
      it != eventProperties.end(); ++it) {
    if (it->first.size() > 1 && it->first[0] == '@')
      continue;
    if (!filter.count(it->first)) {
      if (b.count(it->first)) {
        assert(!i.count(it->first));
        propNames.push_back(make_pair(it->first, Boolean));
      }
      else if (i.count(it->first)) {
        propNames.push_back(make_pair(it->first, Integer));
      }
      else
        propNames.push_back(make_pair(it->first, String));
    }
  }
}

pControl oEvent::addControl(int Id, int Number, const wstring &Name)
{
  if (Id<=0)
    Id=getFreeControlId();
  else
    qFreeControlId = max (qFreeControlId, Id);

  oControl c(this);
  c.set(Id, Number, Name);
  addControl(c);

  oe->updateTabs();
  return &Controls.back();
}

int oEvent::getNextControlNumber() const
{
  int c = 31;
  for (oControlList::const_iterator it = Controls.begin(); it!=Controls.end(); ++it)
    c = max(c, it->maxNumber()+1);

  return c;
}

pControl oEvent::addControl(const oControl &oc)
{
  if (oc.Id<=0)
    return 0;

  if (getControl(oc.Id, false))
    return 0;

  qFreeControlId = max (qFreeControlId, Id);

  Controls.push_back(oc);
  oe->Controls.back().addToEvent(this, &oc);

  return &Controls.back();
}

DirectSocket &oEvent::getDirectSocket() {
  if (directSocket == 0)
    directSocket = new DirectSocket(getId(), getPropertyInt("DirectPort", 21338));

  return *directSocket;
}

pControl oEvent::getControl(int Id) const {
  return const_cast<oEvent *>(this)->getControl(Id, false);
}

pControl oEvent::getControlByType(int type) const {
  for (auto &c : Controls) {
    if (!c.isRemoved() && c.getFirstNumber() == type)
      return pControl(&c);
  }
  return nullptr;
}

pControl oEvent::getControl(int Id, bool create) {
  oControlList::const_iterator it;

  for (it=Controls.begin(); it != Controls.end(); ++it) {
    if (it->Id==Id && !it->isRemoved())
      return pControl(&*it);
  }

  if (!create || Id<=0)
    return nullptr;

  //Not found. Auto add...
  return addControl(Id, Id, L"");
}

bool oEvent::writeControls(xmlparser &xml)
{
  oControlList::iterator it;

  xml.startTag("ControlList");

  for (it=Controls.begin(); it != Controls.end(); ++it)
    it->write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeCourses(xmlparser &xml)
{
  oCourseList::iterator it;

  xml.startTag("CourseList");

  for (it=Courses.begin(); it != Courses.end(); ++it)
    it->Write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeClasses(xmlparser &xml)
{
  oClassList::iterator it;

  xml.startTag("ClassList");

  for (it=Classes.begin(); it != Classes.end(); ++it)
    it->Write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeClubs(xmlparser &xml)
{
  oClubList::iterator it;

  xml.startTag("ClubList");

  for (it=Clubs.begin(); it != Clubs.end(); ++it)
    it->write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeRunners(xmlparser &xml, ProgressWindow &pw)
{
  oRunnerList::iterator it;

  xml.startTag("RunnerList");
  int k=0;
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->tDuplicateLeg) //Duplicates is written by the ruling runner.
      it->Write(xml);
    if (++k%300 == 200)
      pw.setSubProgress( (1000*k)/ Runners.size());
  }
  xml.endTag();

  return true;
}


bool oEvent::writePunches(xmlparser &xml, ProgressWindow &pw)
{
  oFreePunchList::iterator it;

  xml.startTag("PunchList");
  int k = 0;
  for (it=punches.begin(); it != punches.end(); ++it) {
    it->Write(xml);
    if (++k%300 == 200)
      pw.setSubProgress( (1000*k)/ (punches.size()));
  }
  xml.endTag();

  return true;
}

//Write free cards not owned by a runner.
bool oEvent::writeCards(xmlparser &xml)
{
  oCardList::iterator it;

  xml.startTag("CardList");

  for (it=Cards.begin(); it != Cards.end(); ++it) {
    if (it->getOwner() == 0)
      it->Write(xml);
  }

  xml.endTag();
  return true;
}

void oEvent::duplicate(const wstring &annotationIn, bool keepTags) {
  wchar_t file[260];
  wchar_t filename[64];
  wchar_t nameid[64];

  SYSTEMTIME st;
  GetLocalTime(&st);

  swprintf_s(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  getUserFile(file, filename);

  _wsplitpath_s(filename, NULL, 0, NULL,0, nameid, 64, NULL, 0);
  int i=0;
  while (nameid[i]) {
    if (nameid[i]=='.') {
      nameid[i]=0;
      break;
    }
    i++;
  }

  wchar_t oldFile[260];
  wstring oldId;
  wcscpy_s(oldFile, CurrentFile);
  oldId = currentNameId;
  wstring oldAnno = getAnnotation();

  wcscpy_s(CurrentFile, file);
  if (!keepTags)
    currentNameId = nameid;

  swprintf_s(filename, L"%d/%d %d:%02d",
                      st.wDay, st.wMonth, st.wHour, st.wMinute);

  if (annotationIn.empty()) {
    wstring anno = lang.tl(L"Kopia (X)#" + wstring(filename));
    anno = oldAnno.empty() ? anno : oldAnno + L" " + anno;
    setAnnotation(anno);
  }
  else {
    setAnnotation(annotationIn);
  }
  wstring oldTag = getMergeTag();
  try {
    if (!keepTags)
      getMergeTag(true);
    save();
  }
  catch(...) {
    getDI().setString("MergeTag", oldTag);
    // Restore in case of error
    wcscpy_s(CurrentFile, oldFile);
    currentNameId = oldId;
    setAnnotation(oldAnno);
    synchronize(true);
    throw;
  }

  // Restore
  wcscpy_s(CurrentFile, oldFile);
  currentNameId = oldId;
  setAnnotation(oldAnno);
  getDI().setString("MergeTag", oldTag);
  synchronize(true);
}

bool oEvent::save()
{
  if (empty() || gdibase.isTest())
    return true;

  autoSynchronizeLists(true);

  if (!CurrentFile[0])
    throw std::exception("Felaktigt filnamn");

  int f=0;
  _wsopen_s(&f, CurrentFile, _O_RDONLY, _SH_DENYNO, _S_IWRITE);

  wchar_t fn1[260];
  wchar_t fn2[260];
  wstring finalRenameTarget;

  if (f!=-1) {
    _close(f);
    time_t currentTime = time(0);
    const int baseAge = 3; // Three minutes
    time_t allowedAge = baseAge*60;
    time_t oldAge = allowedAge + 60;
    const int maxBackup = 8;
    int toDelete = maxBackup;

    for(int k = 0; k <= maxBackup; k++) {
      swprintf_s(fn1, MAX_PATH, L"%s.bu%d", CurrentFile, k);
      struct _stat st;
      int ret = _wstat(fn1, &st);
      if (ret==0) {
        time_t age = currentTime - st.st_mtime;
        // If file is too young or to old at its
        // position, it is possible to delete.
        // The oldest old file (or youngest young file if none is old)
        // possible to delete is deleted.
        // If no file is possible to delete, the oldest
        // file is deleted.
        if ( (age<allowedAge && toDelete==maxBackup) || age>oldAge)
          toDelete = k;
        allowedAge *= 2;
        oldAge*=2;

        if (k==maxBackup-3)
          oldAge = 24*3600; // Allow a few old copies
      }
      else {
        toDelete = k; // File does not exist. No file need be deleted
        break;
      }
    }

    swprintf_s(fn1, MAX_PATH, L"%s.bu%d", CurrentFile, toDelete);
    ::_wremove(fn1);

    for(int k=toDelete;k>0;k--) {
      swprintf_s(fn1, MAX_PATH, L"%s.bu%d", CurrentFile, k-1);
      swprintf_s(fn2, MAX_PATH, L"%s.bu%d", CurrentFile, k);
      _wrename(fn1, fn2);
    }

    finalRenameTarget = fn1;
    //rename(CurrentFile, fn1);
  }
  bool res;
  if (finalRenameTarget.empty()) {
    res = save(CurrentFile);
    if (!(hasDBConnection() || hasPendingDBConnection))
      openFileLock->lockFile(CurrentFile);
  }
  else {
    wstring tmpName = wstring(CurrentFile) + L".~tmp";
    res = save(tmpName);
    if (res) {
      openFileLock->unlockFile();
      _wrename(CurrentFile, finalRenameTarget.c_str());
      _wrename(tmpName.c_str(), CurrentFile);
  
      if (!(hasDBConnection() || hasPendingDBConnection))
        openFileLock->lockFile(CurrentFile);
    }
  }

  return res;
}

bool oEvent::save(const wstring &fileIn) {
  if (gdibase.isTest())
    return true;

  const wchar_t *file = fileIn.c_str();
  xmlparser xml;
  ProgressWindow pw(gdibase.getHWNDTarget());

  if (Runners.size()>200)
    pw.init();

  xml.openOutput(file, true);
  xml.startTag("meosdata", "version", getMajorVersion());
  xml.write("Name", Name);
  xml.write("Date", Date);
  xml.write("ZeroTime", itos(ZeroTime));
  xml.write("NameId", currentNameId);
  xml.write("Annotation", Annotation);
  xml.write("Id", Id);
  xml.write("Updated", getStamp());

  oEventData->write(this, xml);

  int i = 0;
  vector<int> p;
  p.resize(10);
  p[0] = 2; //= {2, 20, 50, 80, 180, 400,500,700,800,1000};
  p[1] = Controls.size();
  p[2] = Courses.size();
  p[3] = Classes.size();
  p[4] = Clubs.size();
  p[5] = Runners.size() + Cards.size();
  p[6] = Teams.size();
  p[7] = punches.size();
  p[8] = Cards.size();
  p[9] = Runners.size()/2;

  int sum = 0;
  for (int k = 0; k<10; k++)
    sum += p[k];

  for (int k = 1; k<10; k++)
    p[k] = p[k-1] + (1000 * p[k]) / sum;

  p[9] = 1000;

  pw.setProgress(p[i++]);
  writeControls(xml);
  pw.setProgress(p[i++]);
  writeCourses(xml);
  pw.setProgress(p[i++]);
  writeClasses(xml);
  pw.setProgress(p[i++]);
  writeClubs(xml);
  pw.initSubProgress(p[i], p[i+1]);
  pw.setProgress(p[i++]);
  writeRunners(xml, pw);
  pw.setProgress(p[i++]);
  writeTeams(xml);
  pw.initSubProgress(p[i], p[i+1]);
  pw.setProgress(p[i++]);
  writePunches(xml, pw);
  pw.setProgress(p[i++]);
  writeCards(xml);

  xml.startTag("Lists");
  listContainer->save(MetaListContainer::ExternalList, xml, this);
  xml.endTag();

  if (machineContainer) {
    xml.startTag("Machines");
    machineContainer->save(xml);
    xml.endTag();
  }

  xml.closeOut();
  pw.setProgress(p[i++]);
  updateRunnerDatabase();
  pw.setProgress(p[i++]);

  return true;
}

wstring oEvent::getNameId(int id) const {
  if (id == 0)
    return currentNameId;

  list<CompetitionInfo>::const_iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id)
        return it->NameId;
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        return it->NameId;
      }
    }
  }
  return _EmptyWString;
}

const wstring &oEvent::getFileNameFromId(int id) const {

  list<CompetitionInfo>::const_iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id)
        return it->FullPath;
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        return _EmptyWString;
      }
    }
  }
  return _EmptyWString;
}


bool oEvent::open(int id)
{
  list<CompetitionInfo>::iterator it;

  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id) {
        CompetitionInfo ci=*it; //Take copy
        return open(ci.FullPath.c_str(), false, false);
      }
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        CompetitionInfo ci=*it; //Take copy
        if (readSynchronize(ci)) {
          getMergeTag();
          return true;
        }
        return false;
      }
    }
  }

  return false;
}

static DWORD timer;
static string mlog;

static void tic() {
  timer = GetTickCount();
  mlog.clear();
}

static void toc(const string &str) {
  DWORD t = GetTickCount();
  if (!mlog.empty())
    mlog += ",\n";
  else
    mlog = "Tid (hundradels sekunder):\n";

  mlog += str + "=" + itos( (t-timer)/10 );
  timer = t;
}

namespace {
  void getNewFileName(wstring &fn, wstring &nameId) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t file[260];
    wchar_t filename[64];
    swprintf_s(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    //strcpy_s(CurrentNameId, filename);
    getUserFile(file, filename);

    wchar_t CurrentNameId[64];
    _wsplitpath_s(file, NULL, 0, NULL, 0, CurrentNameId, 64, NULL, 0);
    int i = 0;
    while (CurrentNameId[i]) {
      if (CurrentNameId[i] == '.') {
        CurrentNameId[i] = 0;
        break;
      }
      i++;
    }

    fn = file;
    nameId = CurrentNameId;
  }
}

bool oEvent::open(const wstring &file, bool Import, bool forMerge) {
  if (!Import)
    openFileLock->lockFile(file);

  xmlparser xml;
  xml.setProgress(gdibase.getHWNDTarget());
  tic();
  string log;
  xml.read(file);

  string tag = xml.getObject(0).getName();
  wstring iof;
  xml.getObject(0).getObjectString("iofVersion", iof);
  if (tag == "EntryList" || tag == "StartList" || iof.length() > 0)
    throw meosException(L"Filen (X) innehåller IOF-XML tävlingsdata och kan importeras i en existerande tävling#" + file);

  if (tag == "MeOSListDefinition")
    throw meosException(L"Filen (X) är en listdefinition#" + file);

  if (tag == "MeOSResultCalculationSet")
    throw meosException(L"Filen (X) är en resultatmodul#" + file);

  if (tag != "meosdata")
    throw meosException(L"Filen (X) är inte en MeOS-tävling#" + file);

  xmlattrib ver = xml.getObject(0).getAttrib("version");
  if (ver) {
    wstring vs = ver.wget();
    if (vs > getMajorVersion()) {
      // Tävlingen är skapad i MeOS X. Data kan gå förlorad om du öppnar tävlingen.\n\nVill du fortsätta?
      bool cont = gdibase.ask(L"warn:opennewversion#" + vs);
      if (!cont)
        return false;
    }
  }
  toc("parse");
  //This generates a new file name
  newCompetition(L"-");

  if (!Import) {
    wcscpy_s(CurrentFile, MAX_PATH, file.c_str()); //Keep new file name, if imported

    wchar_t CurrentNameId[64];
    _wsplitpath_s(CurrentFile, NULL, 0, NULL,0, CurrentNameId, 64, NULL, 0);
    int i=0;
    while (CurrentNameId[i]) {
      if (CurrentNameId[i]=='.') {
        CurrentNameId[i]=0;
        break;
      }
      i++;
    }
    currentNameId = CurrentNameId;
  }
  bool res = open(xml);
  if (res && !Import)
    openFileLock->lockFile(file);

  if (Import) {
    for (auto &cmp : cinfo) {
      if (cmp.NameId == currentNameId) {
        if (!gdibase.ask(L"ask:importcopy#" + cmp.Name + L", " + cmp.Date)) {
          wstring fn;
          getNewFileName(fn, currentNameId);
        }
        break;
      }
    }
  }

  getMergeTag(Import && !forMerge);

  if (Import && !forMerge) {
    getDI().setString("ImportStamp", gdibase.widen(getLastModified()));
  }

  return res;
}

void oEvent::restoreBackup()
{
  wstring cfile = wstring(CurrentFile) + L".meos";
  wcscpy_s(CurrentFile, cfile.c_str());
}

bool oEvent::open(const xmlparser &xml) {
  xmlobject xo;
  ZeroTime = 0;

  xo = xml.getObject("Date");
  if (xo) {
    wstring fDate = xo.getw();
    if (convertDateYMS(fDate, true) > 0)
      Date = fDate;
  }
  Name.clear();
  xo = xml.getObject("Name");
  if (xo)  Name=xo.getw();

  if (Name.empty()) {
    Name = lang.tl("Ny tävling");
  }

  xo = xml.getObject("Annotation");
  if (xo) Annotation = xo.getw();

  xo=xml.getObject("ZeroTime");
  if (xo) ZeroTime=xo.getInt();

  xo=xml.getObject("Id");
  if (xo) Id=xo.getInt();

  xo=xml.getObject("oData");

  if (xo)
    oEventData->set(this, xo);

  setCurrency(-1, L"", L",", false);

  xo = xml.getObject("NameId");
  if (xo)
    currentNameId = xo.getw();

  toc("event");
  //Get controls
  xo = xml.getObject("ControlList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownControls;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Control")){
        oControl c(this);
        c.set(&*it);

        if (c.Id>0 && knownControls.insert(c.Id).second) {
          addControl(c);
        }
      }
    }
  }

  toc("controls");

  //Get courses
  xo=xml.getObject("CourseList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownCourse;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Course")){
        oCourse c(this);
        c.Set(&*it);
        if (c.Id>0 && knownCourse.count(c.Id) == 0) {
          addCourse(c);
          knownCourse.insert(c.Id);
        }
      }
    }
  }

  toc("course");

  //Get classes
  xo=xml.getObject("ClassList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownClass;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Class")){
        oClass c(this);
        c.Set(&*it);
        if (c.Id>0 && knownClass.count(c.Id) == 0) {
          Classes.push_back(c);
          Classes.back().addToEvent(this, &c);
          knownClass.insert(c.Id);
        }
      }
    }
  }

  toc("class");
  reinitializeClasses();

  //Get clubs
  xo=xml.getObject("ClubList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Club")){
        oClub c(this);
        c.set(*it);
        if (c.Id>0)
          addClub(c);
      }
    }
  }

  toc("club");

  //Get runners
  xo=xml.getObject("RunnerList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Runner")){
        oRunner r(this, 0);
        r.Set(*it);
        if (r.Id>0)
          addRunner(r, false);
        else if (r.Card)
          r.Card->tOwner=0;
      }
    }
  }

  toc("runner");

  //Get teams
  xo=xml.getObject("TeamList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Team")){
        oTeam t(this, 0);
        t.set(*it);
        if (t.Id>0){
          //Teams.push_back(t);
          addTeam(t, false);
          Teams.back().apply(ChangeType::Quiet, nullptr);
        }
      }
    }
  }

  for (oRunner &r : Runners)
    r.apply(ChangeType::Quiet, nullptr);

  toc("team");

  xo=xml.getObject("PunchList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    oFreePunch::disableHashing = true;
    try {
      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("Punch")){
          oFreePunch p(this, 0, 0, 0);
          p.Set(&*it);
          addFreePunch(p);
        }
      }
    }
    catch(...) {
      oFreePunch::disableHashing = false;
      throw;
    }
    oFreePunch::disableHashing = false;
    oFreePunch::rehashPunches(*this, 0, 0);
  }

  toc("punch");

  xo=xml.getObject("CardList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);
    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Card")){
        oCard c(this);
        c.Set(*it);
        assert(c.Id>=0);
        addCard(c);
      }
    }
  }

  toc("card");

  xo=xml.getObject("Updated");
  if (xo) Modified.setStamp(xo.getRaw());

  adjustTeamMultiRunners(0);
  updateFreeId();
  reEvaluateAll(set<int>(), true); //True needed to update data for sure

  toc("update");
  wstring err;

  try {
    xmlobject xList = xml.getObject("Lists");
    if (xList) {
      if (!listContainer->load(MetaListContainer::ExternalList, xList, true)) {
        err = L"Visa listor är gjorda i en senare version av MeOS och kunde inte laddas.";
      }
    }
  }
  catch (const meosException &ex) {
    if (err.empty())
      err = ex.wwhat();
  }
  catch (const std::exception &ex) {
    if (err.empty())
      err = gdibase.widen(ex.what());
  }

  getMeOSFeatures().deserialize(getDCI().getString("Features"), *this);


  try {
    xmlobject xMachine = xml.getObject("Machines");
    if (xMachine) {
      getMachineContainer().load(xMachine);
    }
  }
  catch (const meosException &ex) {
    if (err.empty())
      err = ex.wwhat();
  }
  catch (const std::exception &ex) {
    if (err.empty())
      err = gdibase.widen(ex.what());
  }


  if (!err.empty())
    throw meosException(err);

  return true;
}

bool oEvent::openRunnerDatabase(const wchar_t* filename)
{
  wchar_t file[260];
  getUserFile(file, filename);

  wchar_t fclub[260];
  wchar_t fwclub[260];
  wchar_t frunner[260];
  wchar_t fwrunner[260];

  wcscpy_s(fclub, file);
  wcscat_s(fclub, L".clubs");

  wcscpy_s(fwclub, file);
  wcscat_s(fwclub, L".wclubs");

  wcscpy_s(frunner, file);
  wcscat_s(frunner, L".persons");

  wcscpy_s(fwrunner, file);
  wcscat_s(fwrunner, L".wpersons");

  try {
    if ((fileExists(fwclub) || fileExists(fclub)) && (fileExists(frunner) || fileExists(fwrunner)) ) {
      if (fileExists(fwclub))
        runnerDB->loadClubs(fwclub);
      else
        runnerDB->loadClubs(fclub);

      if (fileExists(fwrunner))
        runnerDB->loadRunners(fwrunner);
      else
        runnerDB->loadRunners(frunner);
    }
  }
  catch (meosException &ex) {
    gdibase.alert(ex.wwhat());
  }
  catch(std::exception &ex) {
    gdibase.alert(ex.what());
  }
  return true;
}

pRunner oEvent::dbLookUpById(__int64 extId) const
{
  if (!useRunnerDb())
    return 0;
  oEvent *toe = const_cast<oEvent *>(this);
  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  sRunner.setTemporary();
  RunnerWDBEntry *dbr = runnerDB->getRunnerById(int(extId));
  if (dbr != 0) {
    sRunner.init(*dbr, false);
    return &sRunner;
  }
  else
    return 0;
}

pRunner oEvent::dbLookUpByCard(int cardNo) const
{
  if (!useRunnerDb())
    return 0;

  oEvent *toe = const_cast<oEvent *>(this);
  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  RunnerWDBEntry *dbr = runnerDB->getRunnerByCard(cardNo);
  if (dbr != 0) {
    dbr->getName(sRunner.sName);
    sRunner.getRealName(sRunner.sName, sRunner.tRealName);
    sRunner.init(*dbr, false);
    sRunner.cardNumber = cardNo;
    return &sRunner;
  }
  else
    return 0;
}

pRunner oEvent::dbLookUpByName(const wstring &name, int clubId, int classId, int birthYear) const
{
  if (!useRunnerDb())
    return 0;

  oEvent *toe = const_cast<oEvent *>(this);

  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  sRunner.setTemporary();

  if (birthYear == 0) {
    pClass pc = getClass(classId);

    int expectedAge = pc ? pc->getExpectedAge() : 0;

    if (expectedAge>0)
      birthYear = getThisYear() - expectedAge;
  }

  pClub pc = getClub(clubId);

  if (pc && pc->getExtIdentifier()>0)
    clubId = (int)pc->getExtIdentifier();

  RunnerWDBEntry *dbr = runnerDB->getRunnerByName(name, clubId, birthYear);

  if (dbr) {
    sRunner.init(*dbr, false);
    return &sRunner;
  }

  return 0;
}

bool oEvent::saveRunnerDatabase(const wchar_t *filename, bool onlyLocal)
{
  wchar_t file[260];
  getUserFile(file, filename);

  wchar_t fclub[260];
  wchar_t frunner[260];
  wcscpy_s(fclub, file);
  wcscat_s(fclub, L".wclubs");

  wcscpy_s(frunner, file);
  wcscat_s(frunner, L".wpersons");

  if (!onlyLocal || !runnerDB->isFromServer()) {
    runnerDB->saveClubs(fclub);
    runnerDB->saveRunners(frunner);
  }
  return true;
}

void oEvent::updateRunnerDatabase()
{
  if (Name == L"!TESTTÄVLING")
    return;

  if (useRunnerDb()) {
    oRunnerList::iterator it;
    map<int, int> clubIdMap;
    for (it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->Card && it->Card->cardNo == it->cardNumber &&
          it->getDI().getInt("CardFee") == 0 && it->Card->getNumPunches() > 5)
        updateRunnerDatabase(&*it, clubIdMap);
    }
    runnerDB->refreshTables();
  }
  if (listContainer) {
    for (int k = 0; k < listContainer->getNumLists(); k++) {
      if (listContainer->isExternal(k)) {
        MetaList &ml = listContainer->getList(k);
        wstring uid = gdibase.widen(ml.getUniqueId()) + L".meoslist";
        wchar_t file[260];
        getUserFile(file, uid.c_str());
        if (!fileExists(file)) {
          ml.save(file, this);
        }
      }
    }vector<pair<string, shared_ptr<DynamicResult>>> freeMod;
    listContainer->getFreeResultModules(freeMod);

    for (size_t k = 0; k < freeMod.size(); k++) {
      wstring uid = gdibase.widen(freeMod[k].first) + L".rules";
      wchar_t file[260];
      getUserFile(file, uid.c_str());
      if (!fileExists(file)) {
        freeMod[k].second->save(file);
      }
    }
  }
}

void oEvent::updateRunnerDatabase(pRunner r, map<int, int> &clubIdMap)
{
  if (!r->cardNumber)
    return;
  runnerDB->updateAdd(*r, clubIdMap);
}

void oEvent::backupRunnerDatabase() {
  if (!runnerDBCopy)
    runnerDBCopy = make_shared<RunnerDB>(*runnerDB);
}

void oEvent::restoreRunnerDatabase() {
  if (runnerDBCopy && *runnerDB != *runnerDBCopy) {
    runnerDB = make_shared<RunnerDB>(*runnerDBCopy);
  }
}

pCourse oEvent::addCourse(const wstring &pname, int plengh, int id) {
  oCourse c(this, id);
  c.Length = plengh;
  c.Name = pname;
  return addCourse(c);
}

pCourse oEvent::addCourse(const oCourse &oc)
{
  if (oc.Id==0)
    return 0;
  else {
    pCourse pOld=getCourse(oc.getId());
    if (pOld)
      return 0;
  }
  Courses.push_back(oc);
  qFreeCourseId=max(qFreeCourseId, oc.getId());

  pCourse pc = &Courses.back();
  pc->addToEvent(this, &oc);

  if (hasDBConnection() && !pc->existInDB() && !pc->isImplicitlyCreated()) {
    pc->changed = true;
    pc->synchronize();
  }
  courseIdIndex[oc.Id] = pc;
  return pc;
}

void oEvent::autoAddTeam(pRunner pr)
{
  //Warning: make sure there is no team already in DB that has not yet been applied yet...
  if (pr && pr->Class) {
    pClass pc = pr->Class;
    if (pc->isSingleRunnerMultiStage()) {
      //Auto create corresponding team
      pTeam t = addTeam(pr->getName(), pr->getClubId(), pc->getId());
      if (pr->StartNo == 0)
        pr->StartNo = Teams.size();
      t->setStartNo(pr->StartNo, ChangeType::Update);
      t->setRunner(0, pr, true);
    }
  }
}

void oEvent::autoRemoveTeam(pRunner pr)
{
  if (pr && pr->Class) {
    pClass pc = pr->Class;
    if (pc->isSingleRunnerMultiStage()) {
      if (pr->tInTeam) {
        // A team may have more than this runner -> do not remove
        bool canRemove = true;
        const auto &runners = pr->tInTeam->Runners;
        for (size_t k = 0; k<runners.size(); k++) {
          if (runners[k] && runners[k]->sName != pr->sName)
            canRemove = false;
        }
        if (canRemove)
          removeTeam(pr->tInTeam->getId());
      }
    }
  }
}

pRunner oEvent::addRunner(const wstring &name, int clubId, int classId,
                          int cardNo, int birthYear, bool autoAdd)
{
  if (birthYear != 0)
    birthYear = extendYear(birthYear);

  pRunner db_r = oe->dbLookUpByCard(cardNo);

  if (db_r && !db_r->matchName(name))
    db_r = 0; // "Existing" card, but different runner


  if (db_r == 0 && getNumberSuffix(name) == 0)
    db_r = oe->dbLookUpByName(name, clubId, classId, birthYear);

  if (db_r) {
    // We got name from DB. Other parameters might have changed from DB.
    if (clubId>0)
      db_r->Club = getClub(clubId);
    db_r->Class = getClass(classId);
    if (cardNo>0)
      db_r->cardNumber = cardNo;
    if (birthYear>0)
      db_r->setBirthYear(birthYear);
    return addRunnerFromDB(db_r, classId, autoAdd);
  }
  oRunner r(this);
  //r.sName = name;
  r.setName(name, false);
  r.getRealName(r.sName, r.tRealName);
  r.Club = getClub(clubId);
  r.Class = getClass(classId);
  if (cardNo>0)
    r.cardNumber = cardNo;
  if (birthYear>0)
    r.setBirthYear(birthYear);
  pRunner pr = addRunner(r, true);
  
  if (pr->getDI().getInt("EntryDate") == 0 && !pr->isVacant()) {
    pr->getDI().setDate("EntryDate", getLocalDate());
    pr->getDI().setInt("EntryTime", getLocalAbsTime());
  }
  if (pr->Class) {
    int heat = pr->Class->getDCI().getInt("Heat");
    if (heat != 0)
      pr->getDI().setInt("Heat", heat);
  }

  pr->updateChanged();

  if (autoAdd)
    autoAddTeam(pr);
  return pr;
}

pRunner oEvent::addRunner(const wstring &pname, const wstring &pclub, int classId,
                          int cardNo, int birthYear, bool autoAdd)
{
  if (!pclub.empty() || getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    pClub club = getClubCreate(0, pclub);
    return addRunner(pname, club->getId(), classId, cardNo, birthYear, autoAdd);
  }
  else
    return addRunner(pname, 0, classId, cardNo, birthYear, autoAdd);
}

pRunner oEvent::addRunnerFromDB(const pRunner db_r,
                                int classId, bool autoAdd)
{
  oRunner r(this);
  r.sName = db_r->sName;
  r.getRealName(r.sName, r.tRealName);
  r.cardNumber = db_r->cardNumber;

  if (db_r->Club) {
    r.Club = getClub(db_r->getClubId());
    if (!r.Club)
      r.Club = addClub(*db_r->Club);
  }

  r.Class=classId ? getClass(classId) : 0;
  memcpy(r.oData, db_r->oData, sizeof(r.oData));

  pRunner pr = addRunner(r, true);
  if (pr->getDI().getInt("EntryDate") == 0 && !pr->isVacant()) {
    pr->getDI().setDate("EntryDate", getLocalDate());
    pr->getDI().setInt("EntryTime", getLocalAbsTime());
  }
  if (r.Class) {
    int heat = r.Class->getDCI().getInt("Heat");
    if (heat != 0)
      pr->getDI().setInt("Heat", heat);
  }

  pr->updateChanged();

  if (autoAdd)
    autoAddTeam(pr);
  return pr;
}

pRunner oEvent::addRunner(const oRunner &r, bool updateStartNo) {
  bool needUpdate = Runners.empty();
  
  Runners.push_back(r);
  pRunner pr=&Runners.back();
  pr->addToEvent(this, &r);

  for (size_t i = 0; i < pr->multiRunner.size(); i++) {
    if (pr->multiRunner[i]) {
      assert(pr->multiRunner[i]->tParentRunner == nullptr || pr->multiRunner[i]->tParentRunner == &r);
      pr->multiRunner[i]->tParentRunner = pr;
    }
  }

  //cardToRunnerHash.reset();
  if (cardToRunnerHash && r.getCardNo() != 0) {
    cardToRunnerHash->emplace(r.getCardNo(), pr);
  } 
  if (classIdToRunnerHash && r.getClassId(false)) {
    (*classIdToRunnerHash)[r.getClassId(true)].push_back(pr);
  }

  if (pr->StartNo == 0 && updateStartNo) {
    pr->StartNo = ++nextFreeStartNo; // Need not be unique
  }
  else {
    nextFreeStartNo = max(nextFreeStartNo, pr->StartNo);
  }

  if (pr->Card)
    pr->Card->tOwner = pr;

  if (hasDBConnection()) {
    if (!pr->existInDB() && !pr->isImplicitlyCreated())
      pr->synchronize();
  }
  if (needUpdate)
    oe->updateTabs();

  if (pr->Class)
    pr->Class->tResultInfo.clear();

  bibStartNoToRunnerTeam.clear();
  runnerById[pr->Id] = pr;

  // Notify runner database that runner has entered
  getRunnerDatabase().hasEnteredCompetition(r.getExtIdentifier());
  return pr;
}

pRunner oEvent::addRunnerVacant(int classId) {
  pRunner r=addRunner(lang.tl(L"Vakant"), getVacantClub(false), classId, 0,0, true);
  if (r) {
    r->apply(ChangeType::Update, nullptr);
    r->synchronize(true);
  }
  return r;
}

int oEvent::getFreeCourseId()
{
  qFreeCourseId++;
  return qFreeCourseId;
}

int oEvent::getFreeControlId()
{
  qFreeControlId++;
  return qFreeControlId;
}

wstring oEvent::getAutoCourseName() const
{
  wchar_t bf[32];
  swprintf_s(bf, lang.tl("Bana %d").c_str(), Courses.size()+1);
  return bf;
}

int oEvent::getFreeClassId()
{
  qFreeClassId++;
  return qFreeClassId;
}

int oEvent::getFirstClassId(bool teamClass) const {
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->getQualificationFinal())
      return it->Id; // Both team and single

    int ns = it->getNumStages();
    if (ns > 0 && it->getNumDistinctRunners() == 1)
      return it->Id; // Both team and single

    if (teamClass && ns > 0)
      return it->Id;
    else if (!teamClass && ns == 0)
      return it->Id;
  }
  return 0;
}

int oEvent::getFreeCardId()
{
  qFreeCardId++;
  return qFreeCardId;
}

int oEvent::getFreePunchId()
{
  qFreePunchId++;
  return qFreePunchId;
}

wstring oEvent::getAutoClassName() const
{
  wchar_t bf[32];
  swprintf_s(bf, 32, lang.tl(L"Klass %d").c_str(), Classes.size()+1);
  return bf;
}

wstring oEvent::getAutoTeamName() const
{
  wchar_t bf[32];
  swprintf_s(bf, 32, lang.tl("Lag %d").c_str(), Teams.size()+1);
  return bf;
}

wstring oEvent::getAutoRunnerName() const
{
  wchar_t bf[32];
  swprintf_s(bf, 32, lang.tl(L"Deltagare %d").c_str(), Runners.size()+1);
  return bf;
}

int oEvent::getFreeClubId()
{
  qFreeClubId++;
  return qFreeClubId;
}

int oEvent::getFreeRunnerId()
{
  qFreeRunnerId++;
  return qFreeRunnerId;
}

void oEvent::updateFreeId(oBase *obj)
{
  if (typeid(*obj)==typeid(oRunner)){
    qFreeRunnerId=max(obj->Id, qFreeRunnerId);
  }
  else if (typeid(*obj)==typeid(oClass)){
    qFreeClassId=max(obj->Id % MaxClassId, qFreeClassId);
  }
  else if (typeid(*obj)==typeid(oCourse)){
    qFreeCourseId=max(obj->Id, qFreeCourseId);
  }
  else if (typeid(*obj)==typeid(oControl)){
    qFreeControlId=max(obj->Id, qFreeControlId);
  }
  else if (typeid(*obj)==typeid(oClub)){
    if (obj->Id != cVacantId && obj->Id != cVacantId)
      qFreeClubId=max(obj->Id, qFreeClubId);
  }
  else if (typeid(*obj)==typeid(oCard)){
    qFreeCardId=max(obj->Id, qFreeCardId);
  }
  else if (typeid(*obj)==typeid(oFreePunch)){
    qFreePunchId=max(obj->Id, qFreePunchId);
  }
  else if (typeid(*obj)==typeid(oTeam)){
    qFreeTeamId=max(obj->Id, qFreeTeamId);
  }
  /*else if (typeid(*obj)==typeid(oEvent)){
    qFree
  }*/
}

void oEvent::updateFreeId()
{
  {
    oRunnerList::iterator it;
    qFreeRunnerId=0;
    nextFreeStartNo = 0;

    for (it=Runners.begin(); it != Runners.end(); ++it) {
      qFreeRunnerId = max(qFreeRunnerId, it->Id);
      nextFreeStartNo = max(nextFreeStartNo, it->StartNo);
    }
  }
  {
    oClassList::iterator it;
    qFreeClassId=0;
    for (it=Classes.begin(); it != Classes.end(); ++it)
      qFreeClassId=max(qFreeClassId, it->Id  % MaxClassId);
  }
  {
    oCourseList::iterator it;
    qFreeCourseId=0;
    for (it=Courses.begin(); it != Courses.end(); ++it)
      qFreeCourseId=max(qFreeCourseId, it->Id);
  }
  {
    oControlList::iterator it;
    qFreeControlId=0;
    for (it=Controls.begin(); it != Controls.end(); ++it)
      qFreeControlId=max(qFreeControlId, it->Id);
  }
  {
    oClubList::iterator it;
    qFreeClubId=0;
    for (it=Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->Id != cVacantId && it->Id != cNoClubId)
        qFreeClubId=max(qFreeClubId, it->Id);
    }
  }
  {
    oCardList::iterator it;
    qFreeCardId=0;
    for (it=Cards.begin(); it != Cards.end(); ++it)
      qFreeCardId=max(qFreeCardId, it->Id);
  }
  {
    oFreePunchList::iterator it;
    qFreePunchId=0;
    for (it=punches.begin(); it != punches.end(); ++it)
      qFreePunchId=max(qFreePunchId, it->Id);
  }

  {
    oTeamList::iterator it;
    qFreeTeamId=0;
    for (it=Teams.begin(); it != Teams.end(); ++it)
      qFreeTeamId=max(qFreeTeamId, it->Id);
  }
}

int oEvent::getVacantClub(bool returnNoClubClub) {
  if (returnNoClubClub) {
    if (noClubId > 0) {
      pClub pc = getClub(noClubId);
      if (pc != 0 && !pc->isRemoved())
        return noClubId;
    }
    pClub pc = getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(L"No club"); //eng
    if (pc == 0)
      pc = getClub(lang.tl("Klubblös")); //other lang?

    if (pc == 0)
      pc=getClubCreate(cNoClubId, lang.tl("Klubblös"));

    noClubId = pc->getId();
    return noClubId;
  }
  else {
    if (vacantId > 0) {
      pClub pc = getClub(vacantId);
      if (pc != 0 && !pc->isRemoved())
        return vacantId;
    }
    pClub pc = getClub(L"Vakant");
    if (pc == 0)
      pc = getClub(L"Vacant"); //eng
    if (pc == 0)
      pc = getClub(lang.tl("Vakant")); //other lang?

    if (pc == 0)
      pc=getClubCreate(cVacantId, lang.tl("Vakant"));

    vacantId = pc->getId();
    return vacantId;
  }
}

int oEvent::getVacantClubIfExist(bool returnNoClubClub) const
{
  if (returnNoClubClub) {
    if (noClubId > 0) {
      pClub pc = getClub(noClubId);
      if (pc != 0 && !pc->isRemoved())
        return noClubId;
    }
    if (noClubId == -1)
      return 0;
    pClub pc=getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(lang.tl(L"Klubblös")); //other lang?

    if (!pc) {
      noClubId = -1;
      return 0;
    }
    noClubId = pc->getId();
    return noClubId;
  }
  else {
    if (vacantId > 0) {
      pClub pc = getClub(vacantId);
      if (pc != 0 && !pc->isRemoved())
        return vacantId;
    }
    if (vacantId == -1)
      return 0;
    pClub pc=getClub(L"Vakant");
    if (pc == 0)
      pc = getClub(L"Vacant");
    if (pc == 0)
      pc = getClub(lang.tl("Vakant")); //other lang?

    if (!pc) {
      vacantId = -1;
      return 0;
    }
    vacantId = pc->getId();
    return vacantId;
  }
}

pCard oEvent::allocateCard(pRunner owner)
{
  oCard c(this);
  c.tOwner = owner;
  Cards.push_back(c);
  pCard newCard = &Cards.back();
  newCard->addToEvent(this, &c);
  return newCard;
}

bool oEvent::sortRunners(SortOrder so) {
  reinitializeClasses();
  if (so == Custom)
    return false;
  CurrentSortOrder=so;
  Runners.sort();
  return true;
}

bool oEvent::sortRunners(SortOrder so, vector<const oRunner *> &runners) const {
  reinitializeClasses();
  auto oldSortOrder = CurrentSortOrder;
  CurrentSortOrder = so;
  sort(runners.begin(), runners.end(), [](const oRunner * &a, const oRunner * &b)->bool {return *a < *b; });
  CurrentSortOrder = oldSortOrder;
  return true;
}

bool oEvent::sortRunners(SortOrder so, vector<pRunner> &runners) const {
  reinitializeClasses();
  auto oldSortOrder = CurrentSortOrder;
  CurrentSortOrder = so;
  sort(runners.begin(), runners.end(), [](pRunner &a, pRunner &b)->bool {return *a < *b; });
  CurrentSortOrder = oldSortOrder;
  return true;
}

wstring oEvent::getZeroTime() const
{
  return getAbsTime(0);
}

void oEvent::setZeroTime(wstring m, bool manualSet)
{
  unsigned nZeroTime = convertAbsoluteTime(m);
  if (nZeroTime!=ZeroTime && nZeroTime != -1) {
    if (manualSet)
      setFlag(TransferFlags::FlagManualDateTime, true);

    updateChanged();
    ZeroTime=nZeroTime;
  }
}

void oEvent::setName(const wstring &m, bool manualSet)
{ 
  wstring tn = trim(m);
  if (tn.empty())
    throw meosException("Tomt namn är inte tillåtet.");

  if (tn != getName()) {
    if (manualSet)
      setFlag(TransferFlags::FlagManualName, true);
    Name = tn;
    updateChanged();
  }
}

void oEvent::setAnnotation(const wstring &m)
{
  if (m!=Annotation) {
    Annotation=m;
    updateChanged();
  }
}

wstring oEvent::getTitleName() const {
  if (empty())
    return L"";
  if (hasPendingDBConnection)
    return getName() + lang.tl(L" (på server)") + lang.tl(L" DATABASE ERROR");
  else if (isClient())
    return getName() + lang.tl(L" (på server)");
  else
    return getName() + lang.tl(L" (lokalt)");
}

void oEvent::setDate(const wstring &m, bool manualSet)
{
  if (m!=Date) {
    int d = convertDateYMS(m, true);
    if (d <= 0)
      throw meosException(L"Felaktigt datumformat 'X' (Använd ÅÅÅÅ-MM-DD).#" + m);
    wstring nDate = formatDate(d, true);
    if (Date != nDate) {
      Date = nDate;
      if (manualSet)
        setFlag(TransferFlags::FlagManualDateTime, true);
      updateChanged();
    }
  }
}

const wstring &oEvent::getAbsTime(DWORD time) const {
  DWORD t = ZeroTime + time;
  if (int(t)<0)
    t = 0;
  int days = time/(3600*24);
  if (days <= 0)
    return formatTimeHMS(t % (24*3600));
  else {
     wstring &res = StringCache::getInstance().wget();
     res = itow(days) + L"D " + formatTimeHMS(t % (24*3600));
     return res;
  }
}

const wstring &oEvent::getTimeZoneString() const {
  if (!date2LocalTZ.count(Date))
    date2LocalTZ[Date] = ::getTimeZoneString(Date);
  return date2LocalTZ[Date];
}

wstring oEvent::getAbsDateTimeISO(DWORD time, bool includeDate, bool useGMT) const
{
  int t = ZeroTime + time;
  wstring dateS, timeS;
  if (int(t)<0) {
    dateS = L"2000-01-01";
    if (useGMT)
      timeS = L"00:00:00Z";
    else
      timeS = L"00:00:00" + getTimeZoneString();
  }
  else {
    int extraDay;

    if (useGMT) {
      int offset = ::getTimeZoneInfo(Date);
      t += offset;
      if (t < 0) {
        extraDay = -1;
        t += 3600 * 24;
      }
      else {
        extraDay = t / (3600*24);
      }
      wchar_t bf[64];
      swprintf_s(bf, L"%02d:%02d:%02dZ", (t/3600)%24, (t/60)%60, t%60);
      timeS = bf;
    }
    else {
      wchar_t bf[64];
      extraDay = t / (3600*24);
      swprintf_s(bf, L"%02d:%02d:%02d", (t/3600)%24, (t/60)%60, t%60);
      timeS = bf + getTimeZoneString();
    }

    if (extraDay == 0 ) {
      dateS = Date;
    }
    else {
      SYSTEMTIME st;
      convertDateYMS(Date, st, false);
      __int64 sec = SystemTimeToInt64Second(st);
      sec = sec + (extraDay * 3600 * 24);
      st = Int64SecondToSystemTime(sec);
      dateS = convertSystemDate(st);
    }
  }

  if (includeDate)
    return dateS + L"T" + timeS;
  else
    return timeS;
}

const wstring &oEvent::getAbsTimeHM(DWORD time) const
{
  DWORD t=ZeroTime+time;

  if (int(t)<0)
    return makeDash(L"-");

  wchar_t bf[32];
  swprintf_s(bf, L"%02d:%02d", (t/3600)%24, (t/60)%60);

  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

//Absolute time string to absolute time int (used by cvs-parser)
int oEvent::convertAbsoluteTime(const string &m)
{
  if (m.empty() || m[0]=='-')
    return -1;

  int len=m.length();
  bool firstComma = false;
  for (int k=0;k<len;k++) {
    BYTE b=m[k];
    if ( !(b==' ' || (b>='0' && b<='9')) ) {
      if (b==':' && firstComma == false)
        continue;
      else if ((b==',' || b=='.') && firstComma == false) {
        firstComma = true;
        continue;
      }
      return -1;
    }
  }

  int hour=atoi(m.c_str());

  if (hour<0 || hour>23)
    return -1;

  int minute=0;
  int second=0;

  int kp=m.find_first_of(':');

  if (kp>0)
  {
    string mtext=m.substr(kp+1);
    minute=atoi(mtext.c_str());

    if (minute<0 || minute>60)
      minute=0;

    kp=mtext.find_last_of(':');

    if (kp>0) {
      second=atoi(mtext.substr(kp+1).c_str());

      if (second<0 || second>60)
        second=0;
    }
  }
  int t=hour*3600+minute*60+second;

  if (t<0) return 0;

  return t;
}

int oEvent::convertAbsoluteTime(const wstring &m)
{
  if (m.empty() || m[0]=='-')
    return -1;

  int len=m.length();
  bool firstComma = false;
  bool anyColon = false;
  for (int k = 0; k < len; k++) {
    wchar_t b = m[k];
    if (!(b == ' ' || (b >= '0' && b <= '9'))) {
      if (b == ':' && firstComma == false) {
        anyColon = true;
        continue;
      }
      else if ((b == ',' || b == '.') && firstComma == false) {
        firstComma = true;
        continue;
      }
      return -1;
    }
  }

  int hour=_wtoi(m.c_str());

  if (!anyColon && hour>=0 && len>=5) {
    int second = hour % 100;
    hour /= 100;
    int minute = hour % 100;
    hour /= 100;
    if (hour > 23 || minute >=60 || second >= 60)
      return -1;
    return hour * 3600 + minute * 60 + second;
  }

  if (hour<0 || hour>23)
    return -1;

  int minute=0;
  int second=0;

  int kp=m.find_first_of(':');

  if (kp>0)
  {
    wstring mtext=m.substr(kp+1);
    minute=_wtoi(mtext.c_str());

    if (minute<0 || minute>60)
      minute=0;

    kp=mtext.find_last_of(':');

    if (kp>0) {
      second=_wtoi(mtext.substr(kp+1).c_str());

      if (second<0 || second>60)
        second=0;
    }
  }
  int t=hour*3600+minute*60+second;

  if (t<0) return 0;

  return t;
}

int oEvent::getRelativeTime(const string &date, const string &absoluteTime, const string &timeZone) const {

  int atime=convertAbsoluteTime(absoluteTime);

  if (timeZone == "Z" || timeZone == "z") {
    SYSTEMTIME st;
    convertDateYMS(date, st, false);

    st.wHour = atime / 3600;
    st.wMinute = (atime / 60) % 60;
    st.wSecond = atime % 60;

    SYSTEMTIME localTime;
    memset(&localTime, 0, sizeof(SYSTEMTIME));
    SystemTimeToTzSpecificLocalTime(0, &st, &localTime);

    atime = localTime.wHour*3600 + localTime.wMinute * 60 + localTime.wSecond;
  }

  if (atime>=0 && atime<3600*24){
    int rtime=atime-ZeroTime;

    if (rtime<=0)
      rtime+=3600*24;

    //Don't allow times just before zero time.
    if (rtime>3600*23)
      return -1;

    return rtime;
  }
  else return -1;
}

int oEvent::getRelativeTime(const wstring &m) const {
  int dayIndex = 0;
  for (size_t k = 0; k + 1 < m.length(); k++) {
    int c = m[k];
    if (c == 'D' || c == 'd' || c == 'T' || c == 't') {
      dayIndex = k + 1;
      break;
    }
  }

  int atime;
  int days = 0;
  if (dayIndex == 0)
    atime = convertAbsoluteTime(m);
  else {
    atime = convertAbsoluteTime(m.substr(dayIndex));
    days = _wtoi(m.c_str());
  }
  if (atime>=0 && atime <= 3600*24){
    int rtime = atime-ZeroTime;

    if (rtime < 0)
      rtime += 3600*24;

    rtime += days * 3600 * 24;
    //Don't allow times just before zero time.
    //if (rtime>3600*22)
    //  return -1;

    return rtime;
  }
  else return -1;
}

void oEvent::removeRunner(const vector<int> &ids)
{
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  oRunnerList::iterator it;

  set<int> toRemove;
  for (size_t k = 0; k < ids.size(); k++) {
    int Id = ids[k];
    pRunner r=getRunner(Id, 0);

    if (r==0)
      continue;
    
    if (r->tInTeam) // XXX
      r = r->tParentRunner ? r->tParentRunner : r;
    else if (r->tParentRunner) {      
      r->tParentRunner->createMultiRunner(true, true);
      r = getRunner(Id, 0);
      if (r == nullptr)
        continue;
      else {
        auto &mlr = r->tParentRunner->multiRunner;
        mlr.erase(std::remove(mlr.begin(), mlr.end(), r), mlr.end());
      }
    }
    if (toRemove.count(r->getId()))
      continue; //Already found.

    //Remove a singe runner team
    for (size_t k = 0; k < r->multiRunner.size(); k++) {
      if (r->multiRunner[k])
        toRemove.insert(r->multiRunner[k]->getId());
    }
    autoRemoveTeam(r);
    toRemove.insert(r->Id);
  }

  if (toRemove.empty())
    return;

  dataRevision++;
  set<pClass> affectedCls;
  for (it=Runners.begin(); it != Runners.end();){
    oRunner &cr = *it;
    if (toRemove.count(cr.getId())> 0) {
      if (cr.Class)
        affectedCls.insert(cr.Class);
      if (hasDBConnection())
        sqlRemove(&cr);
      toRemove.erase(cr.getId());
      runnerById.erase(cr.getId());
      if (cr.Card) {
        assert( cr.Card->tOwner == &cr );
        cr.Card->tOwner = 0;
      }
      // Reset team runner (this should not happen)
      if (it->tInTeam) {
        if (it->tInTeam->Runners[it->tLeg]==&*it)
          it->tInTeam->Runners[it->tLeg] = nullptr;
      }

      oRunnerList::iterator next = it;
      ++next;

      Runners.erase(it);
      if (toRemove.empty()) {
        break;
      }
      else
      it = next;
    }
    else
      ++it;
  }

  for (set<pClass>::iterator it = affectedCls.begin(); it != affectedCls.end(); ++it) {
    (*it)->clearCache(true);
    (*it)->markSQLChanged(-1,-1);
  }

  oe->updateTabs();
}

void oEvent::removeCourse(int Id)
{
  oCourseList::iterator it;

  for (it=Courses.begin(); it != Courses.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      dataRevision++;
      Courses.erase(it);
      courseIdIndex.erase(Id);
      return;
    }
  }
}

void oEvent::removeClass(int Id)
{
  oClassList::iterator it;
  vector<int> subRemove;
  for (it = Classes.begin(); it != Classes.end(); ++it){
    if (it->Id==Id){
      if (it->getQualificationFinal()) {
        for (int n = 0; n < it->getNumQualificationFinalClasses(); n++) {
          const oClass *pc = it->getVirtualClass(n);
          if (pc && pc != &*it)
            subRemove.push_back(pc->getId());
        }
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Classes.erase(it);
      dataRevision++;
      updateTabs();
      break;
    }
  }
  for (int id : subRemove) {
    removeClass(id);
  }
}

void oEvent::removeControl(int Id)
{
  oControlList::iterator it;

  for (it=Controls.begin(); it != Controls.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      Controls.erase(it);
      dataRevision++;
      return;
    }
  }
}

void oEvent::removeClub(int Id)
{
  oClubList::iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it){
    if (it->Id==Id) {
      if (hasDBConnection())
        sqlRemove(&*it);
      Clubs.erase(it);
      clubIdIndex.erase(Id);
      dataRevision++;
      return;
    }
  }
  if (vacantId == Id)
    vacantId = 0; // Clear vacant id

  if (noClubId == Id)
    noClubId = 0;
}

void oEvent::removeCard(int Id)
{
  oCardList::iterator it;

  for (it=Cards.begin(); it != Cards.end(); ++it) {
    if (it->getOwner() == 0 && it->Id == Id) {
      if (it->tOwner) {
        if (it->tOwner->Card == &*it)
          it->tOwner->Card = 0;
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Cards.erase(it);
      dataRevision++;
      return;
    }
  }
}

bool oEvent::isCourseUsed(int Id) const
{
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (it->isCourseUsed(Id))
      return true;
  }

  oRunnerList::const_iterator rit;

  for (rit=Runners.begin(); rit != Runners.end(); ++rit){
    pCourse pc=rit->getCourse(false);
    if (pc && pc->Id==Id)
      return true;
  }
  return false;
}

bool oEvent::isClassUsed(int Id) const
{
  pClass cl = getClass(Id);
  if (cl && cl->parentClass) {
    if (isClassUsed(cl->parentClass->Id))
      return true;
  }

  set<int> idToCheck;
  idToCheck.insert(Id);
  if (cl) {
    for (int i = 0; i < cl->getNumQualificationFinalClasses(); i++)
      idToCheck.insert(cl->getVirtualClass(i)->getId());
  }
  //Search runners
  oRunnerList::const_iterator it;
  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->isRemoved())
      continue;
    if (idToCheck.count(it->getClassId(false)))
      return true;
  }

  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (it->isRemoved())
      continue;
    if (idToCheck.count(tit->getClassId(false)))
      return true;
  }
  return false;
}

bool oEvent::isClubUsed(int Id) const
{
  //Search runners
  oRunnerList::const_iterator it;
  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getClubId()==Id)
      return true;
  }

  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->getClubId()==Id)
      return true;
  }

  return false;
}

bool oEvent::isRunnerUsed(int Id) const
{
  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->isRunnerUsed(Id)) {
      if (tit->Class && tit->Class->isSingleRunnerMultiStage())
        //Don't report single-runner-teams as blocking
        continue;
      return true;
    }
  }

  return false;
}

bool oEvent::isControlUsed(int Id) const
{
  oCourseList::const_iterator it;

  for (it=Courses.begin(); it != Courses.end(); ++it){

    for(int i=0;i<it->nControls;i++)
      if (it->Controls[i] && it->Controls[i]->Id==Id)
        return true;
  }
  return false;
}

bool oEvent::classHasResults(int Id) const
{
  oRunnerList::const_iterator it;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if ( (Id == 0 || it->getClassId(true) == Id) && (it->getCard() || it->FinishTime))
      return true;
  }

  return false;
}

bool oEvent::classHasTeams(int Id) const
{
  pClass pc = oe->getClass(Id);
  if (pc == 0)
    return false;

  if (pc->getQualificationFinal() != 0)
    return false;

  oTeamList::const_iterator it;
  for (it=Teams.begin(); it != Teams.end(); ++it)
    if (!it->isRemoved() && it->getClassId(false)==Id)
      return true;

  return false;
}

void oEvent::generateVacancyList(gdioutput &gdi, GUICALLBACK cb)
{
  sortRunners(ClassStartTime);
  oRunnerList::iterator it;

  // BIB, START, NAME, CLUB, SI
  int dx[5]={0, 0, gdi.scaleLength(70), gdi.scaleLength(150)};

  bool withbib=hasBib(true, false);
  int i;

  const int bibLen = gdi.scaleLength(40);
  if (withbib) for (i = 1; i < 4; i++) dx[i] += bibLen;

  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  const int yStart = y;
  int nVac = 0;
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip() || !it->isVacant())
      continue;
    nVac++;
  }

  int nCol = 1 + min(3, nVac/10);
  int RunnersPerCol = nVac / nCol;

  char bf[256];
  int nRunner = 0;
  y+=lh;

  int Id=0;
  for(it=Runners.begin(); it != Runners.end(); ++it){
    if (it->skip() || !it->isVacant())
      continue;

    if (it->getClassId(true) != Id) {
      Id=it->getClassId(true);
      y+=lh/2;

      if (nRunner>=RunnersPerCol) {
        y = yStart;
        x += dx[3]+gdi.scaleLength(5);
        nRunner = 0;
      }


      gdi.addStringUT(y, x+dx[0], 1, it->getClass(true));
      y+=lh+lh/3;
    }

    oDataInterface DI=it->getDI();

    if (withbib) {
      wstring bib=it->getBib();

      if (!bib.empty()) {
        gdi.addStringUT(y, x+dx[0], 0, bib);
      }
    }
    gdi.addStringUT(y, x+dx[1], 0, it->getStartTimeS(), 0,  cb).setExtra(it->getId());

    _itoa_s(it->Id, bf, 256, 10);
    gdi.addStringUT(y, x+dx[2], 0, it->getName(), dx[3]-dx[2]-4, cb).setExtra(it->getId());
    //gdi.addStringUT(y, x+dx[3], 0, it->getClub());

    y+=lh;
    nRunner++;
  }
  if (nVac==0)
    gdi.addString("", y, x, 0, "Inga vakanser tillgängliga. Vakanser skapas vanligen vid lottning.");
  gdi.updateScrollbars();
}

void oEvent::generateInForestList(gdioutput &gdi, GUICALLBACK cb, GUICALLBACK cb_nostart)
{
  //Lazy setup: tie runners and persons
  oFreePunch::rehashPunches(*oe, 0, 0);

  // Map cardNo -> punch
  multimap<int, pFreePunch> punchHash;
  map<int, int> cardCount;

  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved() && it->getCardNo() > 0)
      ++cardCount[it->getCardNo()];
  }

  typedef multimap<int, pFreePunch>::const_iterator TPunchIter;

  for (oFreePunchList::iterator it = punches.begin(); it != punches.end(); ++it) {
    if (it->isRemoved() || it->isHiredCard())
      continue;
    punchHash.insert(make_pair(it->getCardNo(), &*it));
  }

  sortTeams(ClassStartTime, 0, true);
  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  oTeamList::iterator it;
  gdi.addStringUT(2, lang.tl(L"Kvar-i-skogen") + makeDash(L" - ") + getName());
  y+=lh/2;

  gdi.addStringUT(1, getDate());

  gdi.dropLine();

  y+=3*lh;
  int id=0;
  int nr=0;

  // Get a set with unknown runner id:s
  set<int> statUnknown;
  for (oRunnerList::const_iterator itr=Runners.begin(); itr != Runners.end(); ++itr) {
    if (!itr->hasFinished() && !(itr->skip() || itr->needNoCard())) {
      statUnknown.insert(itr->getId());
    }
  }

  for(it=Teams.begin(); it!=Teams.end(); ++it) {
    if (it->isRemoved())
      continue;
    
    bool unknown = false;
    for (int j = 0; j < it->getNumRunners(); j++) {
      pRunner lr = it->getRunner(j);
      if (lr && statUnknown.count(lr->getId())) {
        unknown = true;
        break;
      }
    }

    if (unknown) {
      if (id != it->getClassId(false)) {
        if (nr>0) {
          gdi.addString("", y, x, 0, "Antal: X#"+itos(nr));
          y+=lh;
          nr=0;
        }
        else {
          gdi.addString("", y, x, fontMediumPlus, "Lag(flera)");
          y += lh;
        }
        y += lh;
        id = it->getClassId(false);
        gdi.addStringUT(y, x, 1, it->getClass(false));
        y += lh;
      }
      gdi.addStringUT(y, x, 0, it->getClass(false));
      nr++;
      gdi.addStringUT(y, x+100, 0, it->getName(), 0, cb).setExtra(it->getId()).id = "T";
      y+=lh;
    }
  }

  if (nr>0) {
    gdi.addString("", y, x, 0, "Antal: X#"+itos(nr));
    y+=lh*2;
    gdi.addString("", y, x, fontMediumPlus, "Deltagare");
    y+=lh/2;
  }

  {
    int tnr = 0;
    id=0;
    nr=0;
    sortRunners(ClassStartTime);

    oRunnerList::iterator it;

    int dx[4]={0, 70, 350, 470};
    int y=gdi.getCY();
    int x=gdi.getCX();
    int lh=gdi.getLineHeight();

    y+=lh;
    char bf[256];

    y=gdi.getCY();
    vector<pRunner> rr;

    for(it=Runners.begin(); it != Runners.end(); ++it){
      if (it->skip() || it->needNoCard())
        continue;

      if (!it->hasFinished()) {

        if (id != it->getClassId(true)) {
          if (nr>0) {
            gdi.addString("", y, x, 0, "Antal: X#"+itos(nr));
            y+=lh;
            nr=0;
          }
          y += lh;
          id = it->getClassId(true);
          gdi.addStringUT(y, x, 1, it->getClass(true));
          y += lh;
        }

        bool hasPunch = false;
        wstring punches;
        wstring otherRunners;
        pair<TPunchIter, TPunchIter> range = punchHash.equal_range(it->getCardNo());
        for (TPunchIter pit = range.first; pit != range.second; ++pit) {
          if (pit->second->tRunnerId == it->getId()) {
            if (hasPunch)
              punches.append(L", ");
            else
              hasPunch = true;

            punches.append(pit->second->getSimpleString());
          }
        }

        getRunnersByCardNo(it->getCardNo(), false, CardLookupProperty::SkipNoStart, rr);
        for (size_t k = 0; k < rr.size(); k++) {
          if (!rr[k]->skip() && rr[k]->getId() != it->getId()) {
            if (otherRunners.empty()) {
              otherRunners = lang.tl("Bricka X används också av: #" + itos(it->getCardNo()));
            }
            else {
              otherRunners += L", ";
            }
            otherRunners += rr[k]->getName();
          }
        }
        gdi.addStringUT(y, x+dx[0], 0, it->getStartTimeS());
        wstring club = it->getClub();
        if (!club.empty())
          club = L" (" + club + L")";

        gdi.addStringUT(y, x+dx[1], 0, it->getName() + club, dx[2]-dx[1]-4, cb).setExtra(it->getId()).id = "R";
        _itoa_s(it->Id, bf, 256, 10);
        nr++;
        tnr++;

        if (hasPunch) {
          if (otherRunners.empty()) {
            RECT rc = gdi.addString("", y, x+dx[2], 0, "(har stämplat)", dx[3]-dx[2]-4).textRect;
            capitalize(punches);
            gdi.addToolTip("", L"#" + punches, 0, &rc);
          }
          else {
            // Återanvänd bricka
            RECT rc = gdi.addString("", y, x+dx[2], 0, L"#(" + lang.tl("reused card") + L")", dx[3]-dx[2]-4).textRect;
            capitalize(punches);
            gdi.addToolTip("", L"#" + punches + L". " + otherRunners, 0, &rc);
          }
        }
        gdi.addStringUT(y, x+dx[3], 0, it->getClass(true));
        y+=lh;
      }
    }
    if (nr>0) {
      gdi.addString("", y, x, 0, "Antal: X#"+itos(nr));
      y+=lh;
    }

    if (tnr == 0 && Runners.size()>0) {
      gdi.addString("", 10, "inforestwarning");
    }
  }

  gdi.updateScrollbars();
}

void oEvent::generateMinuteStartlist(gdioutput &gdi) {
  sortRunners(SortByStartTime);

  int dx[4]={0, gdi.scaleLength(70), gdi.scaleLength(340), gdi.scaleLength(510)};
  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  vector<int> blocks;
  vector<wstring> starts;
  getStartBlocks(blocks, starts);

  wchar_t bf[256];
  for (size_t k=0;k<blocks.size();k++) {
    gdi.dropLine();
    if (k>0)
      gdi.addStringUT(gdi.getCY()-1, 0, pageNewChapter, "");

    gdi.addStringUT(boldLarge|Capitalize, lang.tl(L"Minutstartlista", true) +  makeDash(L" - ") + getName());
    if (!starts[k].empty()) {
      swprintf_s(bf, lang.tl("%s, block: %d").c_str(), starts[k].c_str(), blocks[k]);
      gdi.addStringUT(fontMedium, bf);
    }
    else if (blocks[k]!=0) {
      swprintf_s(bf, lang.tl("Startblock: %d").c_str(),  blocks[k]);
      gdi.addStringUT(fontMedium, bf);
    }

    vector< vector< vector<pRunner> > > sb;
    sb.reserve(Runners.size());
    int LastStartTime=-1;
    for (oRunnerList::iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->Class && it->Class->getBlock() != blocks[k])
        continue;
      if (it->Class && it->Class->getStart() != starts[k])
        continue;
      if (!it->Class && blocks[k]!=0)
        continue;
      if (it->getStatus() == StatusNotCompetiting || it->getStatus() == StatusCANCEL)
        continue;

      if (LastStartTime!=it->tStartTime) {
        sb.resize(sb.size() + 1);
        LastStartTime = it->tStartTime;
      }

      if (sb.empty())
        sb.resize(1);

      if (it->tInTeam == 0)
        sb.back().push_back(vector<pRunner>(1, &*it));
      else {
        if (it->legToRun() > 0 && it->getStartTime() == 0)
          continue;
        int minIx = 10000;
        for (int j = 0; j < it->tInTeam->getNumRunners(); j++) {
          if (j != it->tLeg &&
              it->tInTeam->Runners[j] &&
              it->tInTeam->Runners[j]->tStartTime == it->tStartTime)
            minIx = min(minIx, j);
        }
        if (minIx == 10000)
          sb.back().push_back(vector<pRunner>(1, &*it)); // Single runner on this start time
        else if (minIx > it->tLeg) {
          sb.back().push_back(vector<pRunner>());
          for (int j = 0; j < it->tInTeam->getNumRunners(); j++) {
          if (it->tInTeam->Runners[j] &&
              it->tInTeam->Runners[j]->tStartTime == it->tStartTime)
            sb.back().back().push_back(it->tInTeam->Runners[j]);
          }
        }
      }
    }

    y = gdi.getCY();
    for (size_t k = 0; k < sb.size(); k++) {
      if (sb[k].empty())
        continue;
      y+=lh/2;
      gdi.addStringUT(y, x+dx[0], boldText, sb[k][0][0]->getStartTimeS());
      y+=lh;

      for (size_t j = 0; j < sb[k].size(); j++) {
        const int src_y = y;
        int indent = 0;
        const vector<pRunner> &r = sb[k][j];
        if (r.size() == 1) {
          if (r[0]->getCardNo()>0)
            gdi.addStringUT(y, x+dx[0], fontMedium, itos(r[0]->getCardNo()));

          wstring name;
          if (r[0]->getBib().empty())
            name = r[0]->getName();
          else
            name = r[0]->getName() + L" (" + r[0]->getBib() + L")";
          gdi.addStringUT(y, x+dx[1], fontMedium, name, dx[2]-dx[1]-4);
        }
        else {
          wstring name;
          if (!r[0]->tInTeam->getBib().empty())
            name = r[0]->tInTeam->getBib() + L": ";

          int nnames = 0;
          for (size_t i = 0; i < r.size(); i++) {
            if (nnames>0)
              name += L", ";
            nnames++;

            if (nnames > 2) {
              gdi.addStringUT(y, x+dx[0]+indent, fontMedium, name, dx[2]-dx[0]-4-indent);
              name.clear();
              nnames = 1;
              y+=lh;
              indent = gdi.scaleLength(20);
            }

            name += r[i]->getName();
            if (r[i]->getCardNo()>0) {
              name += L" (" + itow(r[i]->getCardNo()) + L")";
            }

          }
          gdi.addStringUT(y, x+dx[0]+indent, fontMedium, name, dx[2]-dx[0]-4-indent);
        }

        gdi.addStringUT(src_y, x+dx[2], fontMedium, r[0]->getClub(), dx[3]-dx[2]-4);
        gdi.addStringUT(src_y, x+dx[3], fontMedium, r[0]->getClass(true));
        y+=lh;
      }
    }
  }
  gdi.refresh();
}

const wstring &oEvent::getName() const {
  if (Name.size() > 1 && Name.at(0) == '%') {
    return lang.tl(Name.substr(1));
  }
  else
    return Name;
}

bool oEvent::empty() const
{
  return Name.empty();
}

void oEvent::clearListedCmp()
{
  cinfo.clear();
}

bool oEvent::enumerateCompetitions(const wchar_t *file, const wchar_t *filetype)
{
  WIN32_FIND_DATA fd;

  wchar_t dir[MAX_PATH];
  wchar_t FullPath[MAX_PATH];

  wcscpy_s(dir, MAX_PATH, file);

  if (dir[wcslen(file)-1]!='\\')
    wcscat_s(dir, MAX_PATH, L"\\");

  wcscpy_s(FullPath, MAX_PATH, dir);

  wcscat_s(dir, MAX_PATH, filetype);

  HANDLE h=FindFirstFile(dir, &fd);

  if (h==INVALID_HANDLE_VALUE)
    return false;

  bool more=true;
  int id=1;
  cinfo.clear();

  while (more) {
    if (fd.cFileName[0]!='.') //Avoid .. and .
    {
      wchar_t FullPathFile[MAX_PATH];
      wcscpy_s(FullPathFile, MAX_PATH, FullPath);
      wcscat_s(FullPathFile, MAX_PATH, fd.cFileName);

      CompetitionInfo ci;

      ci.FullPath=FullPathFile;
      ci.Name=L"";
      ci.Date=L"2007-01-01";
      ci.Id=id++;

      SYSTEMTIME st;
      FileTimeToSystemTime(&fd.ftLastWriteTime, &st);
      ci.Modified=convertSystemTimeN(st);
      xmlparser xp;

      try {
        xp.read(FullPathFile, 30);

        const xmlobject date=xp.getObject("Date");

        if (date) ci.Date=date.getw();

        const xmlobject name=xp.getObject("Name");

        if (name) {
          ci.Name=name.getw();
          if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
            ci.Name = lang.tl(ci.Name.substr(1));
          }
        }
        const xmlobject annotation=xp.getObject("Annotation");

        if (annotation)
          ci.Annotation=annotation.getw();

        const xmlobject nameid = xp.getObject("NameId");
        if (nameid)
          ci.NameId = nameid.getw();

        auto oData = xp.getObject("oData");
        if (oData) {
          auto preEvent = oData.getObject("PreEvent");
          if (preEvent)
            ci.preEvent = preEvent.getw();

          auto postEvent = oData.getObject("PostEvent");
          if (postEvent)
            ci.postEvent = postEvent.getw();

          auto importStamp = oData.getObject("ImportStamp");
          if (importStamp)
            ci.importTimeStamp = importStamp.getw();
        }
        cinfo.push_front(ci);
      }
      catch (std::exception &) {
        // XXX Do what??
      }
    }
    more=FindNextFile(h, &fd)!=0;
  }

  FindClose(h);

  if (!getServerName().empty())
    sqlConnection->listCompetitions(this, true);

  for (list<CompetitionInfo>::iterator it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Name.size() > 1 && it->Name[0] == '%')
      it->Name = lang.tl(it->Name.substr(1));
  }

/*
  vector<pair<wstring, wstring>> cc;
  for (auto &c : cinfo) {
    cc.emplace_back(c.NameId, c.Date + L": " + c.Name);
  }
  sort(cc.begin(), cc.end());
  for (auto &c : cc) {
    OutputDebugString(c.first.c_str());
    OutputDebugString(L", ");
    OutputDebugString(c.second.c_str());
    OutputDebugString(L"\n");
  }
*/
  return true;
}

bool oEvent::enumerateBackups(const wstring &file) {
  backupInfo.clear();

  enumerateBackups(file, L"*.meos.bu?", 1);
  enumerateBackups(file, L"*.removed", 1);
  enumerateBackups(file, L"*.dbmeos*", 2);
  backupInfo.sort();

  int id = 1;
  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    it->backupId = id++;
  }
  return true;
}

const BackupInfo &oEvent::getBackup(int bid) const {
  for (list<BackupInfo>::const_iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (it->backupId == bid) {
      return *it;
    }
  }
  throw meosException("Internal error");
}

void oEvent::deleteBackups(const BackupInfo &bu) {
  wstring file = bu.fileName + bu.Name;
  list<wstring> toRemove;

  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (file == it->fileName + it->Name)
      toRemove.push_back(it->FullPath);
  }
  if (!toRemove.empty()) {
    wchar_t path[260];
    wchar_t drive[48];
    wchar_t filename[260];
    wchar_t ext[64];
    //_splitpath_s(toRemove.back().c_str(), drive, ds, path, dirs, filename, fns, ext, exts);
    _wsplitpath_s(toRemove.back().c_str(), drive, path, filename, ext);

    wstring dest = wstring(drive) + path;
    toRemove.push_back(dest + bu.fileName + L".persons");
    toRemove.push_back(dest + bu.fileName + L".clubs");
    toRemove.push_back(dest + bu.fileName + L".wclubs");
    toRemove.push_back(dest + bu.fileName + L".wpersons");

    for (list<wstring>::iterator it = toRemove.begin(); it != toRemove.end(); ++it) {
      DeleteFile(it->c_str());
    }
  }
}


bool oEvent::listBackups(gdioutput &gdi, GUICALLBACK cb)
{
  int y = gdi.getCY();
  int x = gdi.getCX();

  list<BackupInfo>::iterator it = backupInfo.begin();
  while (it != backupInfo.end()) {
    list<BackupInfo>::iterator sum_size = it;
    size_t s = 0;
    //string date = it->Modified;
    wstring file = it->fileName + it->Name;

    while(sum_size != backupInfo.end() && file == sum_size->fileName + sum_size->Name) {
      s += sum_size->fileSize;
      ++sum_size;
    }
    wstring type = lang.tl(it->type==1 ? L"backup" : L"serverbackup");
    string size;
    if (s < 1024) {
      size = itos(s) + " bytes";
    }
    else if (s < 1024*512) {
      size = itos(s/1024) + " kB";
    }
    else {
      size = itos(s/(1024*1024)) + "." + itos( ((10*(s/1024))/1024)%10) + " MB";
    }
    gdi.dropLine();
    gdi.addStringUT(gdi.getCY(), gdi.getCX(), boldText, it->Name + L" (" + it->Date + L") " + type, 400);
    
    gdi.pushX();
    gdi.fillRight();
    gdi.addString("", 0, "Utrymme: X#" + size);
    gdi.addString("EraseBackup", 0, "[Radera]", cb).setExtra(it->backupId);
    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(1.5);
    y = gdi.getCY();
    while(it != backupInfo.end() && file == it->fileName + it->Name) {
      gdi.addStringUT(y, x+30, 0, it->Modified, 400, cb).setExtra(it->backupId);
      ++it;
      y += gdi.getLineHeight();
    }
  }

  return true;
}

bool BackupInfo::operator<(const BackupInfo &ci)
{
  if (Date!=ci.Date)
    return Date>ci.Date;

  if (fileName!=ci.fileName)
    return fileName<ci.fileName;

  return Modified>ci.Modified;
}


bool oEvent::enumerateBackups(const wstring &file, const wstring &filetype, int type)
{
  WIN32_FIND_DATA fd;
  wchar_t dir[MAX_PATH];
  wchar_t FullPath[MAX_PATH];

  wcscpy_s(dir, MAX_PATH, file.c_str());

  if (dir[file.length()-1]!='\\')//WCS
    wcscat_s(dir, MAX_PATH, L"\\");

  wcscpy_s(FullPath, MAX_PATH, dir);
  wcscat_s(dir, MAX_PATH, filetype.c_str());
  HANDLE h=FindFirstFile(dir, &fd);

  if (h==INVALID_HANDLE_VALUE)
    return false;

  bool more=true;
  while (more) {
    if (fd.cFileName[0]!='.') {//Avoid .. and .
      wchar_t FullPathFile[MAX_PATH];
      wcscpy_s(FullPathFile, MAX_PATH, FullPath);
      wcscat_s(FullPathFile, MAX_PATH, fd.cFileName);

      BackupInfo ci;

      ci.type = type;
      ci.FullPath=FullPathFile;
      ci.Name=L"";
      ci.Date=L"2007-01-01";
      ci.fileName = fd.cFileName;
      ci.fileSize = fd.nFileSizeLow;
      size_t pIndex = ci.fileName.find_first_of(L".");
      if (pIndex>0 && pIndex<ci.fileName.size())
        ci.fileName = ci.fileName.substr(0, pIndex);

      SYSTEMTIME st;
      FILETIME localTime;
      FileTimeToLocalFileTime(&fd.ftLastWriteTime, &localTime);
      FileTimeToSystemTime(&localTime, &st);

      ci.Modified=convertSystemTimeN(st);
      xmlparser xp;

      try {
        xp.read(FullPathFile, 5);
        //xmlobject *xo=xp.getObject("meosdata");
        const xmlobject date=xp.getObject("Date");

        if (date) ci.Date=date.getw();

        const xmlobject name=xp.getObject("Name");

        if (name) {
          ci.Name=name.getw();
          if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
            ci.Name = lang.tl(ci.Name.substr(1));
          }
        }

        backupInfo.push_front(ci);
      }
      catch (std::exception &) {
        //XXX Do what?
      }
    }
    more=FindNextFile(h, &fd)!=0;
  }

  FindClose(h);

  return true;
}

bool oEvent::fillCompetitions(gdioutput &gdi,
                              const string &name, int type,
                              const wstring &select) {
  cinfo.sort();
  cinfo.reverse();
  list<CompetitionInfo>::iterator it;
  const CompetitionInfo *bestMatch = nullptr; 

  auto accept = [this, &bestMatch](const CompetitionInfo &ci) {
    if (bestMatch == nullptr)
      bestMatch = &ci;
    else {
      bool matchPrevNextId = bestMatch->preEvent == currentNameId || bestMatch->postEvent == currentNameId;
      bool ciMatchPrevNextId = ci.preEvent == currentNameId || ci.postEvent == currentNameId;
      if (matchPrevNextId != ciMatchPrevNextId) {
        if (ciMatchPrevNextId)
          bestMatch = &ci;
      }
      else {
        if (ci.Date > bestMatch->Date) {
          bestMatch = &ci;
        }
        else {
          if (ci.importTimeStamp > bestMatch->importTimeStamp)
            bestMatch = &ci;
        }
      }
    }
  };

  gdi.clearList(name);
  string b;
  //char bf[128];
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    wstring annotation;
    if (!it->Annotation.empty())
      annotation = L" (" + it->Annotation + L")";
    if (it->Server.length()==0) {
      if (type==0 || type==1) {
        if (it->NameId == select && !select.empty())
          accept(*it);
        wstring bf = L"[" + it->Date + L"] " + it->Name;
        gdi.addItem(name, bf + annotation, it->Id);
      }
    }
    else if (type==0 || type==2) {
      if (it->NameId == select && !select.empty())
        accept(*it);
      wstring bf;
      if (type==0)
        bf = lang.tl(L"Server: [X] Y#" + it->Date + L"#" + it->Name);
      else
         bf = L"[" + it->Date + L"] " + it->Name;

      gdi.addItem(name, bf + annotation, 10000000+it->Id);
    }
  }

  if (bestMatch)
    gdi.selectItemByData(name.c_str(), bestMatch->Id);

  return true;
}

void oEvent::checkDB()
{
  if (hasDBConnection()) {
    vector<wstring> err;
    int k=checkChanged(err);

#ifdef _DEBUG
    if (k>0) {
      wchar_t bf[256];
      swprintf_s(bf, L"Databasen innehåller %d osynkroniserade ändringar.", k);
      wstring msg(bf);
      for(int i=0;i < min<int>(err.size(), 10);i++)
        msg+=wstring(L"\n")+err[i];

      MessageBox(0, msg.c_str(), L"Varning/Fel", MB_OK);
    }
#endif
  }
  updateTabs();
  gdibase.setWindowTitle(getTitleName());
}

void destroyExtraWindows();

void oEvent::clear()
{
  checkDB();

  if (hasDBConnection())
    sqlConnection->checkConnection(0);

  isConnectedToServer = false;
  hasPendingDBConnection = false;

  destroyExtraWindows();

  tables.clear();
  Table::resetTableIds();

  getRunnerDatabase().releaseTables();
  getMeOSFeatures().clear(*this);
  Id=0;
  dataRevision = 0;
  tClubDataRevision = -1;
  tCalcNumMapsDataRevision = -1;

  ZeroTime=0;
  Name.clear();
  Annotation.clear();

  //Make sure no daemon is hunting us.
  TabAuto::tabAutoKillMachines();

  delete directSocket;
  directSocket = 0;

  tLongTimesCached = -1;

  //Order of destruction is extreamly important...
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  runnerById.clear();
  bibStartNoToRunnerTeam.clear();
  Runners.clear();
  Teams.clear();
  teamById.clear();

  Classes.clear();
  Courses.clear();
  courseIdIndex.clear();

  Controls.clear();

  Cards.clear();
  Clubs.clear();
  clubIdIndex.clear();

  punchIndex.clear();
  punches.clear();
  cachedFirstStart.clear();
  hiredCardHash.clear();

  updateFreeId();

  currentNameId.clear();
  wcscpy_s(CurrentFile, L"");

  sqlRunners.reset();
  sqlClasses.reset();
  sqlCourses.reset();
  sqlControls.reset();
  sqlClubs.reset();
  sqlCards.reset();
  sqlPunches.reset();
  sqlTeams.reset();
  
  vacantId = 0;
  noClubId = 0;
  oEventData->initData(this, sizeof(oData));
  timelineClasses.clear();
  timeLineEvents.clear();
  nextTimeLineEvent = 0;

  tCurrencyFactor = 1;
  tCurrencySymbol = L"kr";
  tCurrencySeparator = L",";
  tCurrencyPreSymbol = false;

  readPunchHash.clear();

  //Reset speaker data structures.
  listContainer->clearExternal();
  while(!generalResults.empty() && generalResults.back().isDynamic())
    generalResults.pop_back();

  // Cleanup user interface
  gdibase.getTabs().clearCompetitionData();
  
  machineContainer.release();

  MeOSUtil::useHourFormat = getPropertyInt("UseHourFormat", 1) != 0;

  currentNameMode = (NameMode) getPropertyInt("NameMode", FirstLast);
}

const shared_ptr<Table> &oEvent::getTable(const string &key) const {
  if (tables.count(key)) {
    tables.find(key)->second->update();
    return tables.find(key)->second;
  }
  throw meosException("Unknown table " + key);
}

void oEvent::setTable(const string &key, const shared_ptr<Table> &table) {
  tables[key] = table;
}

bool oEvent::deleteCompetition()
{
  if (!empty() && !hasDBConnection()) {
    wstring removed = wstring(CurrentFile)+L".removed";
    ::_wremove(removed.c_str()); //Delete old removed file
    openFileLock->unlockFile();
    ::_wrename(CurrentFile, removed.c_str());
    return true;
  }
  else return false;
}

void oEvent::newCompetition(const wstring &name)
{
  openFileLock->unlockFile();
  clear();

  SYSTEMTIME st;
  GetLocalTime(&st);

  Date = convertSystemDate(st);
  ZeroTime = st.wHour*3600;

  Name = name;
  oEventData->initData(this, sizeof(oData));

  if (!name.empty() && name != L"-")
    getMergeTag();

  getDI().setString("Organizer", getPropertyString("Organizer", L""));
  getDI().setString("Street", getPropertyString("Street", L""));
  getDI().setString("Address", getPropertyString("Address", L""));
  getDI().setString("EMail", getPropertyString("EMail", L""));
  getDI().setString("Homepage", getPropertyString("Homepage", L""));

  getDI().setInt("CardFee", getPropertyInt("CardFee", 25));
  getDI().setInt("EliteFee", getPropertyInt("EliteFee", 130));
  getDI().setInt("EntryFee", getPropertyInt("EntryFee", 90));
  getDI().setInt("YouthFee", getPropertyInt("YouthFee", 50));

  getDI().setInt("SeniorAge", getPropertyInt("SeniorAge", 0));
  getDI().setInt("YouthAge", getPropertyInt("YouthAge", 16));

  getDI().setString("Account", getPropertyString("Account", L""));
  getDI().setString("LateEntryFactor", getPropertyString("LateEntryFactor", L"50 %"));

  getDI().setString("CurrencySymbol", getPropertyString("CurrencySymbol", L"kr"));
  getDI().setString("CurrencySeparator", getPropertyString("CurrencySeparator", L"."));
  getDI().setInt("CurrencyFactor", getPropertyInt("CurrencyFactor", 1));
  getDI().setInt("CurrencyPreSymbol", getPropertyInt("CurrencyPreSymbol", 0));
  getDI().setString("PayModes", getPropertyString("PayModes", L""));

  setCurrency(-1, L"", L"", 0);

  wstring file;
  getNewFileName(file, currentNameId);
  wcscpy_s(CurrentFile, MAX_PATH, file.c_str());

  oe->updateTabs();
}

void oEvent::reEvaluateCourse(int CourseId, bool doSync)
{
  oRunnerList::iterator it;

  if (doSync)
    autoSynchronizeLists(false);

  vector<int> mp;
  set<int> classes;
  for(it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getCourse(false) && it->getCourse(false)->getId()==CourseId){
      classes.insert(it->getClassId(true));
    }
  }

  reEvaluateAll(classes, false);
}

void oEvent::reEvaluateAll(const set<int> &cls, bool doSync)
{
  if (disableRecalculate)
    return;

  if (doSync)
    autoSynchronizeLists(false);

  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it) {
    if (cls.empty() || cls.count(it->Id)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!cls.empty() && cls.count(tit->getClassId(false)) == 0)
      continue;

    if (!tit->isRemoved()) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  oRunnerList::iterator it;

  if (cls.size() < 5) {
    vector<pRunner> runners;
    getRunners(cls, runners);
    for (pRunner it : runners) {
      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }
  else {
    for (it = Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  vector<int> mp;
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
         continue;

      if (!it->isRemoved()) {
        if (it->tLeg == leg) {
          it->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
          it->storeTimes();
        }
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }

  // Mark info as complete
  for (auto& c : Classes) {
    if (!c.isRemoved() && (cls.empty() || cls.count(c.Id)))
      for (auto &i : c.tLeaderTime)
        i.setComplete();
  }

  // Update team start times etc.
  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!tit->isRemoved()) {
      if (!cls.empty() && cls.count(tit->getClassId(true)) == 0)
        continue;

      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved()) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && (it->Class->isQualificationFinalBaseClass())))
        it->apply(ChangeType::Quiet, nullptr);
      it->storeTimes();
      it->clearOnChangedRunningTime();
    }
  }
  //reCalculateLeaderTimes(0);
}

void oEvent::reEvaluateChanged()
{
  if (sqlClasses.changed || sqlCourses.changed || sqlControls.changed) {
    reEvaluateAll(set<int>(), false);
    globalModification = true;
    return;
  }

  if (sqlClubs.changed)
    globalModification = true;


  if (!sqlCards.changed && !sqlRunners.changed && !sqlTeams.changed)
    return; // Nothing to do

  map<int, bool> resetClasses;
  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it)  {
    if (it->wasSQLChanged(-1, oPunch::PunchFinish)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
      resetClasses[it->getId()] = it->hasClassGlobalDependence();
      it->updateLeaderTimes();
    }
  }

  unordered_set<int> addedTeams;

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (tit->isRemoved() || !tit->wasSQLChanged())
      continue;

    addedTeams.insert(tit->getId());
    
    tit->apply(ChangeType::Quiet, nullptr);
  }

  oRunnerList::iterator it;
  vector< vector<pRunner> > legRunners(maxRunnersTeam);

  if (Teams.size() > 0) {
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved())
        continue;
      int clz = it->getClassId(true);
      //if (resetClasses.count(clz))
      //  it->storeTimes();

      if (!it->wasSQLChanged() && !resetClasses[clz])
        continue;

      pTeam t = it->tInTeam;
      if (t && !addedTeams.count(t->getId())) {
        addedTeams.insert(t->getId());
        t->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    pRunner r = &*it;
    if (r->isRemoved())
      continue;

    if (r->wasSQLChanged() || (r->tInTeam && addedTeams.count(r->tInTeam->getId()))) {
      unsigned leg = r->tLeg;
      if (leg <0 || leg >= maxRunnersTeam)
        leg = 0;

      if (legRunners[leg].empty())
        legRunners[leg].reserve(Runners.size() / (leg+1));

      legRunners[leg].push_back(r);
      if (!r->tInTeam) {
        r->apply(ChangeType::Quiet, nullptr);
      }
    }
    else {
      if (r->Class && r->Class->wasSQLChanged(-1, oPunch::PunchFinish)) {
        it->storeTimes();
      }
    }
  }

  vector<int> mp;

  // Reevaluate
  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      lr[k]->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (addedTeams.count(tit->getId())) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }

  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      if (!lr[k]->tInTeam)
        lr[k]->apply(ChangeType::Quiet, nullptr);
      lr[k]->clearOnChangedRunningTime();
    }
  }
}

void oEvent::reCalculateLeaderTimes(int classId)
{
  if (disableRecalculate)
    return;

  if (classId) {
    pClass cls = getClass(classId);
    if (cls)
      cls->resetLeaderTime();
  }
  else {
    for (auto &c : Classes) {
      if (!c.isRemoved())
        c.resetLeaderTime();
    }
  }
  
  /*
#ifdef _DEBUG
  wchar_t bf[128];
  swprintf_s(bf, L"Calculate leader times %d\n", classId);
  OutputDebugString(bf);
#endif
  for (oClassList::iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->isRemoved() && (classId==it->getId() || classId==0))
      it->resetLeaderTime();
  }
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (oRunnerList::iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (!it->isRemoved() && (classId==0 || classId==it->getClassId(true))) {
        if (it->tLeg == leg)
          it->storeTimes();
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }*/
}


wstring oEvent::getCurrentTimeS() const
{
  SYSTEMTIME st;
  GetLocalTime(&st);

  wchar_t bf[64];
  swprintf_s(bf, 64, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
  return bf;
}

int oEvent::findBestClass(const SICard &card, vector<pClass> &classes) const
{
  classes.clear();
  int Distance=-1000;
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it) {
    vector<pCourse> courses;
    it->getCourses(0, courses);
    bool insertClass = false; // Make sure a class is only included once

    for (size_t k = 0; k<courses.size(); k++) {
      pCourse pc = courses[k];
      if (pc) {
        int d=pc->distance(card);

        if (d>=0) {
          if (Distance<0) Distance=1000;

          if (d<Distance) {
            Distance=d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (d == Distance) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
        else {
          if (Distance<0 && d>Distance) {
            Distance = d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (Distance == d) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
      }
    }
  }
  return Distance;
}

void oEvent::convertTimes(pRunner runner, SICard &sic) const
{
  assert(sic.convertedTime != ConvertedTimeStatus::Unknown);
  if (sic.convertedTime == ConvertedTimeStatus::Done)
    return;

  if (sic.convertedTime == ConvertedTimeStatus::Hour12) {

    int startTime = ZeroTime + 3600; //Add one hour. Subtracted below
    if (useLongTimes())
      startTime = 5 * 3600; // Avoid midnight as default. Prefer morning

    int st = -1;
    if (runner) {
      st = runner->getStartTime();
      if (st > 0) {
        startTime = (ZeroTime + st) % (3600 * 24);
      }
      else {
        st = -1;
      }
    }

    if (st <= -1) {
      // Fallback for no start time. Take from card. Will be wrong if more than 12 hour after ZeroTime
      if (sic.StartPunch.Code != -1) {
        st = sic.StartPunch.Time;
      }
      else if (sic.nPunch > 0 && sic.Punch[0].Time >= 0) {
        st = sic.Punch[0].Time;
      }

      if (st >= 0) { // Optimize local zero time w.r.t first punch
        int relT12 = (st - ZeroTime + 3600 * 24) % (3600 * 12);
        startTime = (ZeroTime + relT12) % (3600 * 24);
      }
    }
    int zt = (startTime + 23 * 3600) % (24 * 3600); // Subtract one hour
    sic.analyseHour12Time(zt);
  }
  sic.convertedTime = ConvertedTimeStatus::Done;

  if (sic.CheckPunch.Code!=-1){
    if (sic.CheckPunch.Time<unsigned(ZeroTime))
      sic.CheckPunch.Time+=(24*3600);

    sic.CheckPunch.Time-=ZeroTime;
  }

   // Support times longer than 24 hours
  int maxLegTime = useLongTimes() ? 22 * 3600 : 0;
  
  if (maxLegTime > 0) {

    const int START = 1000;
    const int FINISH = 1001;
    vector<pair<int, int> > times;
    
    if (sic.StartPunch.Code!=-1) {
      if (sic.StartPunch.Time != -1)
        times.push_back(make_pair(sic.StartPunch.Time, START));
    }

    for (unsigned k=0; k <sic.nPunch; k++){
      if (sic.Punch[k].Code!=-1 && sic.Punch[k].Time != -1) {
        times.push_back(make_pair(sic.Punch[k].Time, k));
      }
    }

    if (sic.FinishPunch.Code!=-1 && sic.FinishPunch.Time != 1) {
      times.push_back(make_pair(sic.FinishPunch.Time, FINISH));

    if (!times.empty()) {
      int dayOffset = 0;
      if (times.front().first < int(ZeroTime)) {
        dayOffset = 3600 * 24;
        times.front().first += dayOffset;
      }
      for (size_t k = 1; k < times.size(); k++) {
        int delta = times[k].first - (times[k-1].first - dayOffset);
        if (delta < (maxLegTime - 24 * 3600)) {
          dayOffset += 24 * 3600;
        }
        times[k].first += dayOffset;
      }

      // Update card times
      for (size_t k = 0; k < times.size(); k++) {
        if (times[k].second == START)
          sic.StartPunch.Time = times[k].first;
        else if (times[k].second == FINISH)
          sic.FinishPunch.Time = times[k].first;
        else 
          sic.Punch[times[k].second].Time = times[k].first;
        }
      }
    }
  }

  if (sic.StartPunch.Code != -1) {
    if (sic.StartPunch.Time<unsigned(ZeroTime))
      sic.StartPunch.Time+=(24*3600);

    sic.StartPunch.Time-=ZeroTime;
  }

  for (unsigned k = 0; k < sic.nPunch; k++){
    if (sic.Punch[k].Code!=-1){
      if (sic.Punch[k].Time<unsigned(ZeroTime))
        sic.Punch[k].Time+=(24*3600);

      sic.Punch[k].Time-=ZeroTime;
    }
  }

  if (sic.FinishPunch.Code!=-1){
    if (sic.FinishPunch.Time<unsigned(ZeroTime))
      sic.FinishPunch.Time+=(24*3600);

    sic.FinishPunch.Time-=ZeroTime;
  }
}

int oEvent::getFirstStart(int classId) const {
  auto &cf = cachedFirstStart[classId];
  if (dataRevision == cf.first)
    return cf.second;

  oRunnerList::const_iterator it=Runners.begin();
  int minTime=3600*24;

  while(it!=Runners.end()){
    if (!it->isRemoved() && (classId == 0 || it->getClassId(true) == classId)) {
      if (it->tStartTime < minTime && it->tStatus != StatusNotCompetiting && it->tStartTime>0)
        minTime = it->tStartTime;
    }
    ++it;
  }

  if (minTime==3600*24)
    minTime=0;

  cf.first = dataRevision;
  cf.second = minTime;

  return minTime;
}

bool oEvent::hasRank() const
{
  oRunnerList::const_iterator it;

  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getDCI().getInt("Rank")>0)
      return true;
  }
  return false;
}

void oEvent::setMaximalTime(const wstring &t)
{
  getDI().setInt("MaxTime", convertAbsoluteTime(t));
}

int oEvent::getMaximalTime() const
{
  return getDCI().getInt("MaxTime");
}

wstring oEvent::getMaximalTimeS() const
{
  return formatTime(getMaximalTime());
}


bool oEvent::hasBib(bool runnerBib, bool teamBib) const
{
  if (runnerBib) {
    oRunnerList::const_iterator it;
    for (it=Runners.begin(); it != Runners.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  if (teamBib) {
    oTeamList::const_iterator it;
    for (it=Teams.begin(); it != Teams.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  return false;
}

bool oEvent::hasTeam() const
{
  return Teams.size() > 0;
}

void oEvent::addBib(int ClassId, int leg, const wstring &firstNumber) {
  if ( !classHasTeams(ClassId) ) {
    sortRunners(ClassStartTimeClub);
    oRunnerList::iterator it;

    pClass cls = getClass(ClassId);
    if (cls == 0)
      throw meosException("Class not found");

    if (cls->getParentClass()) {
      cls->getParentClass()->setBibMode(BibFree);
      cls->getParentClass()->synchronize(true);
    }
    if (!firstNumber.empty()) {
      cls->setBibMode(BibFree);
      cls->synchronize(true);
      wchar_t pattern[32];
      int num =  oClass::extractBibPattern(firstNumber, pattern);
    
      for (it=Runners.begin(); it != Runners.end(); ++it) {
        if (it->isRemoved())
          continue;
        if ( (ClassId==0 || it->getClassId(true)==ClassId) && (it->legToRun()==leg || leg == -1)) {
          wchar_t bib[32];
          swprintf_s(bib, pattern, num);
          pClass pc = it->getClassRef(true);
          it->setBib(bib, num, pc ? !pc->lockedForking() : true);
          num++;
          it->synchronize();
        }
      }
    }
    else {
      for(it=Runners.begin(); it != Runners.end(); ++it){
        if (it->isRemoved())
          continue;
        if (ClassId==0 || it->getClassId(true)==ClassId) {
          it->getDI().setString("Bib", L"");//Update only bib
          it->synchronize();
        }
      }
    }
  }
  else {
    map<int, int> teamStartNo;
   
    if (!firstNumber.empty()) {
      // Clear out start number temporarily, to not use it for sorting
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          if (it->getClassRef(false) && it->getClassRef(false)->getBibMode() != BibFree) {
            for (size_t i = 0; i < it->Runners.size(); i++) {
              if (it->Runners[i]) {
                it->Runners[i]->setStartNo(0, ChangeType::Update);
                it->Runners[i]->setBib(L"", 0, false);
              }
            }
          }
          teamStartNo[it->getId()] = it->getStartNo();
          it->setStartNo(0, ChangeType::Update);
        }
      }
    }

    sortTeams(ClassStartTime, 0, true); // Sort on first leg starttime and sortindex

    if (!firstNumber.empty()) {
      wchar_t pattern[32];
      int num =  oClass::extractBibPattern(firstNumber, pattern);
    
      for (auto it=Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;

        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          wchar_t bib[32];
          swprintf_s(bib, pattern, num);
          bool lockedStartNo = it->Class && it->Class->lockedForking();
          if (lockedStartNo) {
            it->setBib(bib, num, false);
            it->setStartNo(teamStartNo[it->getId()], ChangeType::Update);
          }
          else {
            it->setBib(bib, num, true);
          }
          num++;
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
    else {
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          it->getDI().setString("Bib", L""); //Update only bib
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
  }
}

void oEvent::addAutoBib() {
  sortRunners(ClassStartTimeClub);
  oRunnerList::iterator it;
  int clsId = -1;
  int bibGap = oe->getBibClassGap();
  int interval = 1;
  set<int> isTeamCls;
  wchar_t pattern[32] = {0};
  wchar_t storedPattern[32];
  wcscpy_s(storedPattern, L"%d");

  int number = 0;

  map<int, int> teamStartNo;
  // Clear out start number temporarily, to not use it for sorting
  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    pClass cls = tit->getClassRef(false);
    if (cls == 0)
      continue;
    teamStartNo[tit->getId()] = tit->getStartNo();

    wstring bibInfo = cls->getDCI().getString("Bib");
  
    bool teamAssign = !bibInfo.empty() && cls->getNumStages() > 1;

    bool freeMode = cls->getBibMode()==BibFree;
    if (!teamAssign && freeMode)
      continue; // Manul or none
    isTeamCls.insert(cls->getId());

    bool addBib = bibInfo != L"-";

    if (addBib && teamAssign)
      tit->setStartNo(0, ChangeType::Update);

    if (tit->getClassRef(false) && tit->getClassRef(false)->getBibMode() != BibFree) {
      for (size_t i = 0; i < tit->Runners.size(); i++) {
        if (tit->Runners[i]) {
          if (addBib && teamAssign)
            tit->Runners[i]->setStartNo(0, ChangeType::Update);
          if (!freeMode)
            tit->Runners[i]->setBib(L"", 0, false);
        }
      }
    }
  }

  sortTeams(ClassStartTime, 0, true); // Sort on first leg starttime and sortindex
  map<int, vector<pTeam> > cls2TeamList;

  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    int clsId = tit->getClassId(false);
    cls2TeamList[clsId].push_back(&*tit);
  }

  map<int, vector<pRunner> > cls2RunnerList;
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved() || !it->getClassId(false))
      continue;
    int clsId = it->getClassId(true);
    cls2RunnerList[clsId].push_back(&*it);
  }

  Classes.sort();
  for (oClassList::iterator clsIt = Classes.begin(); clsIt != Classes.end(); ++clsIt) {
  
    const pClass cls = &*clsIt;
    clsId = cls->getId();

    wstring bibInfo = cls->getDCI().getString("Bib");
    if (bibInfo.empty()) {
      // Skip class
      continue;
    }
    else if (bibInfo == L"*") {
      if (number == 0)
        number = 1;
      else 
        number += bibGap;

      if (pattern[0] == 0) {
        wcscpy_s(pattern, storedPattern);
      }
    }
    else if (bibInfo == L"-") {
      if (pattern[0]) {
        wcscpy_s(storedPattern, pattern);
      }
      pattern[0] = 0; // Clear bibs in class

    }
    else {
      number = oClass::extractBibPattern(bibInfo, pattern); 
    }
      
    if (isTeamCls.count(clsId)) {
      vector<pTeam> &tl = cls2TeamList[clsId]; 

      if (cls->getBibMode() == BibAdd) {
        int ns = cls->getNumStages();
        if (ns <= 10)
          interval = 10;
        else
          interval = 100;

        if (bibInfo == L"*") {
          int add = interval - number % interval;
          number += add;
        }
      }
      else {
        interval = 1;
      }

      if (pattern[0] == 0) {
        // Remove bib
        for (size_t k = 0; k < tl.size(); k++) {
          tl[k]->getDI().setString("Bib", L""); //Update only bib
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }
      else  {
        bool lockedForking = cls->lockedForking();
        for (size_t k = 0; k < tl.size(); k++) {
          wchar_t buff[32];
          swprintf_s(buff, pattern, number);

          if (lockedForking) {
            tl[k]->setBib(buff, number, false);
            tl[k]->setStartNo(teamStartNo[tl[k]->getId()], ChangeType::Update);
          }
          else {
            tl[k]->setBib(buff, number, true);
          }
          number += interval;
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }

      continue;
    }
    else {
      interval = 1;
    
      vector<pRunner> &rl = cls2RunnerList[clsId]; 
      bool locked = cls->lockedForking();
      if (pattern[0] && cls->getParentClass()) {
        // Switch to free mode if bib set for subclass
        cls->getParentClass()->setBibMode(BibFree);
        cls->setBibMode(BibFree);
        cls->getParentClass()->synchronize(true);
        cls->synchronize(true);
      }
      for (size_t k = 0; k < rl.size(); k++) {
        if (pattern[0]) {
          wchar_t buff[32];
          swprintf_s(buff, pattern, number);
          rl[k]->setBib(buff, number, !locked);
          number += interval;
        }
        else {
          rl[k]->getDI().setString("Bib", L""); //Update only bib
        }
        rl[k]->synchronize(true);
      }
    }
  }
}

void oEvent::checkOrderIdMultipleCourses(int ClassId) {
  sortRunners(ClassStartTime);
  int order = 1;
  oRunnerList::iterator it;

  //Find first free order
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId) {
      it->synchronize();//Ensure we are up-to-date
      order = max(order, it->StartNo);
    }
  }

  //Assign orders
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassRef(true) && it->getClassRef(true)->lockedForking())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId)
      if (it->StartNo == 0) {
        if (it->getTeam()) {
          if (it->getTeam()->getStartNo() == 0) {
            it->updateStartNo(++order);
          }
          else {
            it->setStartNo(it->getTeam()->getStartNo(), ChangeType::Update);
            it->synchronize(true);
          }
        }
        else {
          it->updateStartNo(++order);
        }
      }
  }
}

void oEvent::fillStatus(gdioutput &gdi, const string& id)
{
  vector< pair<wstring, size_t> > d;
  fillStatus(d);
  gdi.addItem(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillStatus(vector< pair<wstring, size_t> > &out) {
  out.clear();
  out.push_back(make_pair(lang.tl(L"-"), StatusUnknown));
  out.push_back(make_pair(lang.tl(L"Godkänd"), StatusOK));
  out.push_back(make_pair(lang.tl(L"Ej start"), StatusDNS));
  out.push_back(make_pair(lang.tl(L"Återbud[status]"), StatusCANCEL));
  out.push_back(make_pair(lang.tl(L"Felst."), StatusMP));
  out.push_back(make_pair(lang.tl(L"Utg."), StatusDNF));
  out.push_back(make_pair(lang.tl(L"Disk."), StatusDQ));
  out.push_back(make_pair(lang.tl(L"Maxtid"), StatusMAX));
  out.push_back(make_pair(lang.tl(L"Utom tävlan"), StatusOutOfCompetition));
  out.push_back(make_pair(lang.tl(L"Utan tidtagning"), StatusNoTiming));
  out.push_back(make_pair(lang.tl(L"Deltar ej"), StatusNotCompetiting));
  return out;
}

int oEvent::getPropertyInt(const char *name, int def)
{
  if (eventProperties.count(name)==1)
    return _wtoi(eventProperties[name].c_str());
  else {
    setProperty(name, def);
    return def;
  }
}

const wstring &oEvent::getPropertyString(const char *name, const wstring &def)
{
  if (eventProperties.count(name)==1) {
    return eventProperties[name];
  }
  else {
    eventProperties[name] = def;
    return eventProperties[name];
  }
}

const string &oEvent::getPropertyString(const char *name, const string &def)
{
  if (eventProperties.count(name)==1) {
    string &out = StringCache::getInstance().get();
    wide2String(eventProperties[name], out);
    return out;
  }
  else {
    string &out = StringCache::getInstance().get();
    string2Wide(def, eventProperties[name]);
    out = def;
    return out;
  }
}

string oEvent::getPropertyStringDecrypt(const char *name, const string &def)
{
  wchar_t bf[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(bf, &len);
  string prop = getPropertyString(name, def);
  string prop2;
  int code = 0;
  const int s = 337;

  for (size_t j = 0; j<prop.length(); j+=2) {
    for (size_t k = 0; k<len; k++)
      code = code * 31 + bf[k];
    unsigned int b1 = ((unsigned char *)prop.c_str())[j] - 33;
    unsigned int b2 = ((unsigned char *)prop.c_str())[j+1] - 33;
    unsigned int b = b1 | (b2<<4);
    unsigned kk = abs(code) % s;
    b = (b + s - kk) % s;
    code += b%5;
    prop2.push_back((unsigned char)b);
  }
  return prop2;
}

void oEvent::setPropertyEncrypt(const char *name, const string &prop) {
  wchar_t bf[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(bf, &len);
  string prop2;
  int code = 0;
  const int s = 337;

  for (size_t j = 0; j<prop.length(); j++) {
    for (size_t k = 0; k<len; k++)
      code = code * 31 + bf[k];
    unsigned int b = ((unsigned char *)prop.c_str())[j];
    unsigned kk = abs(code) % s;
    code += b%5;
    b = (b + kk) % s;
    unsigned b1 = (b & 0x0F) + 33;
    unsigned b2 = (b>>4) + 33;
    prop2.push_back((unsigned char)b1);
    prop2.push_back((unsigned char)b2);
  }

  setProperty(name, gdibase.widen(prop2));
}

void oEvent::setProperty(const char *name, int prop) {
  eventProperties[name]=itow(prop);
}

void oEvent::setProperty(const char *name, const wstring &prop) {
  eventProperties[name] = prop;
}

void oEvent::saveProperties(const wchar_t *file) {
  map<string, wstring>::const_iterator it;
  xmlparser xml;
  xml.openOutputT(file, false, "MeOSPreference");

  for (it = eventProperties.begin(); it != eventProperties.end(); ++it) {
    xml.write(it->first.c_str(), it->second);
  }

  xml.closeOut();
}

void oEvent::loadProperties(const wchar_t *file) {
  eventProperties.clear();
  initProperties();
  try {
    xmlparser xml;
    xml.read(file);
    xmlobject xo = xml.getObject("MeOSPreference");
    if (xo) {
      xmlList list;
      xo.getObjects(list);
      for (size_t k = 0; k<list.size(); k++) {
        eventProperties[list[k].getName()] = list[k].getw();
      }
    }
  }
  catch (std::exception &) {
    // Failed to read. Continue.
  }
}

bool compareClubClassTeamName(const oRunner &a, const oRunner &b)
{
  if (a.Club==b.Club) {
    if (a.getClassId(true) == b.getClassId(true)) {
      if (a.tInTeam==b.tInTeam)
        return a.tRealName<b.tRealName;
      else if (a.tInTeam) {
        if (b.tInTeam)
          return a.tInTeam->getStartNo() < b.tInTeam->getStartNo();
        else return false;
      }
      return b.tInTeam!=0;
    }
    else
      return a.getClass(true)<b.getClass(true);
  }
  else
    return a.getClub()<b.getClub();
}

void oEvent::assignCardInteractive(gdioutput &gdi, GUICALLBACK cb)
{
  gdi.fillDown();
  gdi.dropLine(1);
  gdi.addString("", 2, "Tilldelning av hyrbrickor");

  Runners.sort(compareClubClassTeamName);

  oRunnerList::iterator it;
  pClub lastClub=0;

  int k=0;
  for (it=Runners.begin(); it != Runners.end(); ++it) {

    if (it->skip() || it->getCardNo() || it->isVacant() || it->needNoCard())
      continue;

    if (it->getStatus() == StatusDNS || it->getStatus() == StatusCANCEL || it->getStatus() == StatusNotCompetiting)
      continue;

    if (it->Club!=lastClub) {
      lastClub=it->Club;
      gdi.dropLine(0.5);
      gdi.addString("", 1, it->getClub());
    }

    wstring r;
    if (it->Class)
      r+=it->getClass(false)+L", ";

    if (it->tInTeam) {
      r+=itow(it->tInTeam->getStartNo()) + L" " + it->tInTeam->getName() + L", ";
    }

    r += it->getName() + L":";
    gdi.fillRight();
    gdi.pushX();
    gdi.addStringUT(0, r);
    char id[24];
    sprintf_s(id, "*%d", k++);

    gdi.addInput(max(gdi.getCX(), 450), gdi.getCY()-4,
                 id, L"", 10, cb).setExtra(it->getId());

    gdi.popX();
    gdi.dropLine(1.6);
    gdi.fillDown();
  }

  if (k==0)
    gdi.addString("", 0, "Ingen löpare saknar bricka");
}

void oEvent::calcUseStartSeconds()
{
  tUseStartSeconds=false;
  oRunnerList::iterator it;
  for (it=Runners.begin(); it != Runners.end(); ++it)
    if ( it->getStartTime()>0 &&
        (it->getStartTime()+ZeroTime)%60!=0 ) {
      tUseStartSeconds=true;
      return;
    }
}

const wstring &oEvent::formatStatus(RunnerStatus status, bool forPrint)
{
  const static wstring stats[12] = { L"?", L"Godkänd", L"Ej start", L"Felst.", L"Utg.", L"Disk.",
                                 L"Maxtid", L"Deltar ej", L"Återbud[status]", L"Utom tävlan",
                                 L"Utan tidtagning", L"\u2014" };
  switch (status) {
  case StatusOK:
    return lang.tl(stats[1]);
  case StatusDNS:
    return lang.tl(stats[2]);
  case StatusCANCEL:
    return lang.tl(stats[8]);
  case StatusMP:
    return lang.tl(stats[3]);
  case StatusDNF:
    return lang.tl(stats[4]);
  case StatusDQ:
    return lang.tl(stats[5]);
  case StatusMAX:
    return lang.tl(stats[6]);
  case StatusNotCompetiting:
    if (forPrint)
      return stats[11];
    else
      return lang.tl(stats[7]);
  case StatusOutOfCompetition:
    return lang.tl(stats[9]);
  case StatusUnknown: {
    if (forPrint)
      return formatTime(-1);
    else
      return stats[0];
  }
  case StatusNoTiming: {
    if (forPrint)
      return lang.tl(stats[1]);
    else
      return lang.tl(stats[10]);
  }
  default:
    return stats[0];
  }
}

#ifndef MEOSDB

void oEvent::analyzeClassResultStatus() const
{
  map<int, ClassResultInfo> res;
  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved() || !it->Class)
      continue;

    int id = it->Class->Id * 31 + it->tLeg;
    ClassResultInfo &cri = res[id];

    if (it->getStatus() == StatusUnknown) {
      cri.nUnknown++;
      if (it->tStartTime > 0) {
        if (!it->isVacant()) {
          if (cri.lastStartTime>=0)
            cri.lastStartTime = max(cri.lastStartTime, it->tStartTime);
        }
      }
      else
        cri.lastStartTime = -1; // Cannot determine
    }
    else
      cri.nFinished++;

  }

  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!it->legInfo.empty()) {
      it->tResultInfo.resize(it->legInfo.size());
      for (size_t k = 0; k<it->legInfo.size(); k++) {
        int id = it->Id * 31 + k;
        it->tResultInfo[k] = res[id];
      }
    }
    else {
      it->tResultInfo.resize(1);
      it->tResultInfo[0] = res[it->Id * 31];
    }
  }
}

void oEvent::generateTestCard(SICard &sic) const
{
  sic.clear(0);
  sic.convertedTime = ConvertedTimeStatus::Hour24;

  if (Runners.empty())
    return;

  analyzeClassResultStatus();

  oRunnerList::const_iterator it;

  int rNo = rand()%Runners.size();

  it=Runners.begin();

  while(rNo-->0)
    ++it;

  oRunner *r = 0;
  int cardNo = 0;
  while(r==0 && it!=Runners.end()) {
    cardNo = it->getCardNo();
    
    if (it->Class && it->tLeg>0) {
      StartTypes st = it->Class->getStartType(it->tLeg);
      if (st == STHunting) {
        if (it->Class->tResultInfo[it->tLeg-1].nUnknown > 0)
          cardNo = 0; // Wait with this leg
      }
    }

    // Make sure teams start in right order
    if (it->tInTeam && it->tLeg>0) {
      if (it->Class) {
        StartTypes st = it->Class->getStartType(it->tLeg);
        if (st != STDrawn && st != STTime) {
          pRunner prev = it->tInTeam->Runners[it->tLeg - 1];
          if (prev && prev->getStatus() == StatusUnknown)
            cardNo = 0; // Wait with this runner
        }
      }
    }

    if (cardNo && !it->Card) {
      // For team runners, we require start time to get right order
      if (!it->tInTeam || it->tStartTime>0)
        r=pRunner(&*it);
    }
    ++it;
  }
  --it;
  while(r==0 && it!=Runners.begin()) {
    cardNo = it->getCardNo();

    if (it->Class && it->tLeg>0) {
      StartTypes st = it->Class->getStartType(it->tLeg);
      if (st == STHunting) {
        if (it->Class->tResultInfo[it->tLeg-1].nUnknown > 0)
          cardNo = 0; // Wait with this leg
      }
    }

    // Make sure teams start in right order
    if (it->tInTeam && it->tLeg>0) {
      if (it->Class) {
        StartTypes st = it->Class->getStartType(it->tLeg);
        if (st != STDrawn && st != STTime) {
          pRunner prev = it->tInTeam->Runners[it->tLeg - 1];
          if (prev && prev->getStatus() == StatusUnknown)
            cardNo = 0; // Wait with this runner
        }
      }
    }

    if (cardNo && !it->Card) {
      // For team runners, we require start time to get right order
      if (!it->tInTeam || it->tStartTime>0) {
        r=pRunner(&*it);
      }
    }
    --it;
  }

  if (r) {
    r->synchronize();
    pCourse pc=r->getCourse(false);

    if (!pc) {
      pClass cls = r->Class;
      if (cls) {
        pc = const_cast<oEvent *>(this)->generateTestCourse(rand()%15+7);
        pc->synchronize();
        cls->setCourse(pc);
        cls->synchronize();
      }
    }

    if (pc) {
      sic.CardNumber = cardNo;

      if (rand()%5 == 3)
        sic.CardNumber = 100000;

      int s = sic.StartPunch.Time = r->tStartTime>0 ? r->tStartTime+ZeroTime : ZeroTime+3600+rand()%(3600*3);
      int tomiss = rand()%(60*10);
      if (tomiss>60*9)
        tomiss = rand()%30;
      else if (rand()%20 == 3)
        tomiss *= rand()%3;

      int f = sic.FinishPunch.Time = s+(30+pc->getLength()/200)*60+ rand()%(60*10) + tomiss;

      if (rand()%40==0 || r->tStartTime>0)
        sic.StartPunch.Code=-1;

      if (rand()%50==31)
        sic.FinishPunch.Code=-1;

      if (rand()%70==31)
        sic.CardNumber++;

      sic.nPunch=0;
      double dt=1./double(pc->nControls+1);

      int missed = 0;

      for(int k=0;k<pc->nControls;k++) {
        if (rand()%130!=50) {
          sic.Punch[sic.nPunch].Code=pc->getControl(k)->Numbers[0];
          double cc=(k+1)*dt;


          if (missed < tomiss) {
            int left = pc->nControls - k;
            if (rand() % left == 1)
              missed += ( (tomiss - missed) * (rand()%4 + 1))/6;
            else if (left == 1)
              missed = tomiss;
          }

          sic.Punch[sic.nPunch].Time=int((f-tomiss)*cc+s*(1.-cc)) + missed;
          sic.nPunch++;
        }
      }
    }
  }
}

pCourse oEvent::generateTestCourse(int nCtrl)
{
  wchar_t bf[64];
  static int sk=0;
  swprintf_s(bf, lang.tl("Bana %d").c_str(), ++sk);
  pCourse pc=addCourse(bf, 4000+(rand()%1000)*10);

  int i=0;
  for (;i<nCtrl/3;i++)
    pc->addControl(rand()%(99-32)+32);

  i++;
  pc->addControl(50)->setName(L"Radio 1");

  for (;i<(2*nCtrl)/3;i++)
    pc->addControl(rand()%(99-32)+32);

  i++;
  pc->addControl(150)->setName(L"Radio 2");

  for (;i<nCtrl-1;i++)
    pc->addControl(rand()%(99-32)+32);
  pc->addControl(100)->setName(L"Förvarning");

  return pc;
}


pClass oEvent::generateTestClass(int nlegs, int nrunners,
                                 wchar_t *name, const wstring &start)
{
  pClass cls=addClass(name);

  if (nlegs==1 && nrunners==1) {
    int nCtrl=rand()%15+5;
    if (rand()%10==1)
      nCtrl+=rand()%40;
    cls->setCourse(generateTestCourse(nCtrl));
  }
  else if (nlegs==1 && nrunners==2) {
    setupRelay(*cls, PPatrol, 2, start);
    int nCtrl=rand()%15+10;
    pCourse pc=generateTestCourse(nCtrl);
    cls->addStageCourse(0, pc->getId(), -1);
    cls->addStageCourse(1, pc->getId(), -1);
  }
  else if (nlegs>1 && nrunners==2) {
    setupRelay(*cls, PTwinRelay, nlegs, start);
    int nCtrl=rand()%8+10;
    int cid[64];
    for (int k=0;k<nlegs;k++)
      cid[k]=generateTestCourse(nCtrl)->getId();

    for (int k=0;k<nlegs;k++)
      for (int j=0;j<nlegs;j++)
        cls->addStageCourse(k, cid[(k+j)%nlegs], -1);
  }
  else if (nlegs>1 && nrunners==nlegs) {
    setupRelay(*cls, PRelay, nlegs, start);
    int nCtrl=rand()%8+10;
    int cid[64];
    for (int k=0;k<nlegs;k++)
      cid[k]=generateTestCourse(nCtrl)->getId();

    for (int k=0;k<nlegs;k++)
      for (int j=0;j<nlegs;j++)
        cls->addStageCourse(k, cid[(k+j)%nlegs], -1);
  }
  else if (nlegs>1 && nrunners==1) {
    setupRelay(*cls, PHunting, 2, start);
    cls->addStageCourse(0, generateTestCourse(rand()%8+10)->getId(), -1);
    cls->addStageCourse(1, generateTestCourse(rand()%8+10)->getId(), -1);
  }
  return cls;
}


void oEvent::generateTestCompetition(int nClasses, int nRunners,
                                     bool generateTeams) {
  if (nClasses > 0) {
    oe->newCompetition(L"!TESTTÄVLING");
    oe->setZeroTime(L"05:00:00", true);
    oe->getMeOSFeatures().useAll(*oe);
  }
  vector<wstring> gname;
  //gname.reserve(RunnerDatabase.size());
  vector<wstring> fname;
  //fname.reserve(RunnerDatabase.size());

  runnerDB->getAllNames(gname, fname);

  if (fname.empty())
    fname.push_back(L"Foo");

  if (gname.empty())
    gname.push_back(L"Bar");

/*  oRunnerList::iterator it;
  for(it=RunnerDatabase.begin(); it!=RunnerDatabase.end(); ++it){
    if (!it->getGivenName().empty())
      gname.push_back(it->getGivenName());

    if (!it->getFamilyName().empty())
      fname.push_back(it->getFamilyName());
  }
*/
  int nClubs=30;
  wchar_t bfw[128];
  
  int startno=1;
  const vector<oDBClubEntry> &oc = runnerDB->getClubDB(false);
  for(int k=0;k<nClubs;k++) {
    if (oc.empty()) {
      swprintf_s(bfw, L"Klubb %d", k);
      addClub(bfw, k+1);
    }
    else {
      addClub(oc[(k*13)%oc.size()].getName(), k+1);
    }
  }

  int now=getRelativeTime(getCurrentTimeS());
  wstring start=getAbsTime(now+60*3-(now%60));

  for (int k=0;k<nClasses;k++) {
    pClass cls=0;

    if (!generateTeams) {
      int age=0;
      if (k<7)
        age=k+10;
      else if (k==7)
        age=18;
      else if (k==8)
        age=20;
      else if (k==9)
        age=21;
      else
        age=30+(k-9)*5;

      swprintf_s(bfw, L"HD %d", age);
      cls=generateTestClass(1,1, bfw, L"");
    }
    else {
      swprintf_s(bfw, L"Klass %d", k);
      int nleg=k%5+1;
      int nrunner=k%3+1;
      nrunner = nrunner == 3 ? nleg:nrunner;

      nleg=3;
      nrunner=3;
      cls=generateTestClass(nleg, nrunner, bfw, start);
    }
  }

  nClasses = Classes.size();
  int k = 0;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it, ++k) {
    pClass cls = &*it;
    int classesLeft=(nClasses-k);
    int nRInClass=nRunners/classesLeft;

    if (classesLeft>2 && nRInClass>3)
      nRInClass+=int(nRInClass*0.7)-rand()%int(nRInClass*1.5);

    if (cls->getNumDistinctRunners()==1) {
      for (int i=0;i<nRInClass;i++) {
        pRunner r=addRunner(gname[rand()%gname.size()]+L" "+fname[rand()%fname.size()],
          rand()%nClubs+1, cls->getId(), 0, 0, true);

        r->setStartNo(startno++, ChangeType::Update);
        r->setCardNo(500001+Runners.size()*97+rand()%97, false);
        r->apply(ChangeType::Update, nullptr);
      }
      nRunners-=nRInClass;
      if (k%5!=5) {
        vector<ClassDrawSpecification> spec;
        spec.emplace_back(cls->getId(), 0, getRelativeTime(start), 10, 3, VacantPosition::Mixed);
        drawList(spec, DrawMethod::MeOS, 1, oEvent::DrawType::DrawAll);
      }
      else
        cls->Name += L" Öppen";
    }
    else {
      int dr=cls->getNumDistinctRunners();
      for (int i=0;i<nRInClass;i++) {
        pTeam t=addTeam(L"Lag " + fname[rand()%fname.size()], rand()%nClubs+1, cls->getId());
        t->setStartNo(startno++, ChangeType::Update);

        for (int j=0;j<dr;j++) {
          pRunner r=addRunner(gname[rand()%gname.size()]+L" "+fname[rand()%fname.size()], 0, 0, 0, 0, true);
          r->setCardNo(500001+Runners.size()*97+rand()%97, false);
          t->setRunner(j, r, false);
        }
      }
      nRunners-=nRInClass;

      if ( cls->getStartType(0)==STDrawn ) {
        vector<ClassDrawSpecification> spec;
        spec.emplace_back(cls->getId(), 0, getRelativeTime(start), 20, 3, VacantPosition::Mixed);
        drawList(spec, DrawMethod::MeOS, 1, DrawType::DrawAll);
      }
    }
  }
}

#endif

void oEvent::getFreeImporter(oFreeImport &fi)
{
  if (!fi.isLoaded())
    fi.load();

  fi.init(Runners, Clubs, Classes);
}


void oEvent::fillFees(gdioutput &gdi, const string &name, bool onlyDirect, bool withAuto) const {
  gdi.clearList(name);

  set<int> fees;

  int f;
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (onlyDirect && !it->getAllowQuickEntry())
      continue;

    f = it->getDCI().getInt("ClassFee");
    if (f > 0)
      fees.insert(f);

    f = it->getDCI().getInt("ClassFeeRed");
    if (f > 0)
      fees.insert(f);

    if (withAuto) {
      f = it->getDCI().getInt("HighClassFee");
      if (f > 0)
        fees.insert(f);

      f = it->getDCI().getInt("HighClassFeeRed");
      if (f > 0)
        fees.insert(f);
    }
  }
  
  if (fees.empty()) {
    if (!onlyDirect) {
      f = getDCI().getInt("EliteFee");
      if (f > 0)
        fees.insert(f);
    }

    f = getDCI().getInt("EntryFee");
    if (f > 0)
      fees.insert(f);

    f = getDCI().getInt("YouthFee");
    if (f > 0)
      fees.insert(f);
  }
  vector< pair<wstring, size_t> > ff;
  if (withAuto)
    ff.push_back(make_pair(lang.tl(L"Från klassen"), -1));
  for (set<int>::iterator it = fees.begin(); it != fees.end(); ++it)
    ff.push_back(make_pair(formatCurrency(*it), *it));

  gdi.addItem(name, ff);
}

void oEvent::fillLegNumbers(const set<int> &cls,
                            bool isTeamList, 
                            bool includeSubLegs, 
                            vector< pair<wstring, size_t> > &out) {
  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  out.clear();
  set< pair<int, int> > legs;

  for (it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->Removed && (cls.empty() || cls.count(it->getId()))) {
      if (it->getNumStages() == 0)
        continue;

      for (size_t j = 0; j < it->getNumStages(); j++) {
        int number, order;
        if (it->splitLegNumberParallel(j, number, order)) {
          if (order == 0)
            legs.insert( make_pair(number, 0) );
          else {
            if (it->isOptional(j))
              continue;

            if (order == 1)
              legs.insert( make_pair(number, 1000));
            legs.insert(make_pair(number, 1000+order));
          }
        }
        else {
          legs.insert( make_pair(number, 0) );
        }
      }
    }
  }

  out.reserve(legs.size() + 1);
  for (set< pair<int, int> >::const_iterator it = legs.begin(); it != legs.end(); ++it) {
    if (it->second == 0) {
      out.push_back( make_pair(lang.tl("Sträcka X#" + itos(it->first + 1)), it->first));
    }
  }
  if (includeSubLegs) {
    for (set< pair<int, int> >::const_iterator it = legs.begin(); it != legs.end(); ++it) {
      if (it->second >= 1000) {
        int leg = it->first;
        int sub = it->second - 1000;
        char bf[64];
        char symb = 'a' + sub;
        sprintf_s(bf, "Sträcka X#%d%c", leg+1, symb);
        out.push_back( make_pair(lang.tl(bf), (leg + 1) * 10000 + sub));
      }
    }
  }
  
  if (isTeamList)
    out.push_back(make_pair(lang.tl("Sista sträckan"), 1000));
  else
    out.push_back(make_pair(lang.tl("Alla sträckor"), 1000));
}

void oEvent::generateTableData(const string &tname, Table &table, TableUpdateInfo &tui)
{
  if (tname == "runners") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pRunner r = tui.doAdd ? addRunner(getAutoRunnerName(),0,0,0,0,false) : pRunner(tui.object);
    generateRunnerTableData(table, r);
    return;
  }
  else if (tname == "classes") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pClass c = tui.doAdd ? addClass(getAutoClassName()) : pClass(tui.object);
    generateClassTableData(table, c);
    return;
  }
  else if (tname == "clubs") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pClub c = tui.doAdd ? addClub(L"Club", 0) : pClub(tui.object);
    generateClubTableData(table, c);
    return;
  }
  else if (tname == "teams") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pTeam t = tui.doAdd ? addTeam(getAutoTeamName()) : pTeam(tui.object);
    generateTeamTableData(table, t);
    return;
  }
  else if (tname == "cards") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    generateCardTableData(table, pCard(tui.object));
    return;
  }
  else if (tname == "controls") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    generateControlTableData(table, pControl(tui.object));
    return;
  }
  else if (tname == "punches") {
    if (tui.doRefresh && !tui.doAdd)
      return;

    pFreePunch c = tui.doAdd ? addFreePunch(0,0,0, false) : pFreePunch(tui.object);
    generatePunchTableData(table, c);
    return;
  }
  else if (tname == "courses") {
    if (tui.doRefresh && !tui.doAdd)
      return;

    pCourse c = tui.doAdd ? addCourse(getAutoCourseName()) : pCourse(tui.object);
    oCourse::generateTableData(oe, table, c);
    return;
  }
  else if (tname == "runnerdb") {
    if (tui.doAdd || !tui.doRefresh) {
      oDBRunnerEntry *entry = tui.doAdd ? getRunnerDatabase().addRunner() : (oDBRunnerEntry *)(tui.object);
      getRunnerDatabase().generateRunnerTableData(table, entry);
    }

    if (tui.doRefresh)
      getRunnerDatabase().refreshRunnerTableData(table);

    return;
  }
  else if (tname == "clubdb") {
    if (tui.doAdd || !tui.doRefresh) {
      pClub c = tui.doAdd ? getRunnerDatabase().addClub() : pClub(tui.object);
      getRunnerDatabase().generateClubTableData(table, c);
    }

    if (tui.doRefresh) {
      getRunnerDatabase().refreshClubTableData(table);
    }
    return;
  }
  throw std::exception("Wrong table name");
}

void oEvent::applyEventFees(bool updateClassFromEvent,
                            bool updateFees, bool updateCardFees,
                            const set<int> &classFilter) {
  synchronizeList({ oListId::oLClassId, oListId::oLRunnerId });
  bool allClass = classFilter.empty();

  if (updateClassFromEvent) {
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (it->isRemoved())
        continue;
      if (allClass || classFilter.count(it->getId())) {
        it->addClassDefaultFee(true);
        it->synchronize(true);
      }
    }
  }

  if (updateFees) {
    for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;

      if (allClass || classFilter.count(it->getClassId(true))) {
        it->addClassDefaultFee(true);
        it->synchronize(true);
      }
    }
  }

  if (updateCardFees) {
    int cf = getDCI().getInt("CardFee");

    for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;

      if (it->getDI().getInt("CardFee") != 0) {
        it->getDI().setInt("CardFee", cf);
        it->synchronize(true);
      }
    }
  }
}

#ifndef MEOSDB
void hideTabs();
void createTabs(bool force, bool onlyMain, bool skipTeam, bool skipSpeaker,
                bool skipEconomy, bool skipLists, bool skipRunners, bool skipCourses, bool skipControls);

void oEvent::updateTabs(bool force, bool hide) const
{
  bool hasTeam = !Teams.empty();

  for (oClassList::const_iterator it = Classes.begin();
                  !hasTeam && it!=Classes.end(); ++it) {
    if (it->getNumStages()>1)
      hasTeam = true;
  }

  bool hasRunner = !Runners.empty() || !Classes.empty();
  bool hasLists = !empty();
  bool skipCourses = getMeOSFeatures().withoutCourses(*this);
  if (hide || isReadOnly())
    hideTabs();
  else
    createTabs(force, empty(), !hasTeam, !getMeOSFeatures().hasFeature(MeOSFeatures::Speaker),
               !(getMeOSFeatures().hasFeature(MeOSFeatures::Economy)
               || getMeOSFeatures().hasFeature(MeOSFeatures::EditClub)),
               !hasLists, !hasRunner, skipCourses, Controls.empty() && !skipCourses);
}

#else
void oEvent::updateTabs(bool force) const
{
}
#endif

bool oEvent::useRunnerDb() const {
  return getMeOSFeatures().hasFeature(MeOSFeatures::RunnerDb);
}

int oEvent::getBaseCardFee() const {
  int baseCardFee = oe->getDI().getInt("CardFee");
  if (baseCardFee == 0)
    baseCardFee = -1;
  return baseCardFee;
}

bool oEvent::hasMultiRunner() const {
  for (oClassList::const_iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (it->hasMultiCourse() && it->getNumDistinctRunners() != it->getNumStages())
      return true;
  }

  return false;
}

/** Return false if card is not used */
bool oEvent::checkCardUsed(gdioutput &gdi, oRunner &runnerToAssignCard, int cardNo) {
  pRunner pold = 0;
  if (cardNo != 0) {
    vector<pRunner> allR;
    getRunnersByCardNo(cardNo, true, CardLookupProperty::OnlyMainInstance, allR);
    for (pRunner it : allR) {
      if (!runnerToAssignCard.canShareCard(it, cardNo)) {
        pold = &*it;
        break;
      }
    }
  }
  wchar_t bf[1024];

  if (pold) {
    swprintf_s(bf, (L"#" + lang.tl("Bricka %d används redan av %s och kan inte tilldelas.")).c_str(),
                  cardNo, pold->getCompleteIdentification().c_str());
    gdi.alert(bf);
    return true;
  }
  return false;
}

void oEvent::removeVacanies(int classId) {
  oRunnerList::iterator it;
  vector<int> toRemove;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip() || !it->isVacant())
      continue;

    if (classId!=0 && it->getClassId(false)!=classId)
      continue;

    if (!isRunnerUsed(it->Id))
      toRemove.push_back(it->Id);
  }

  removeRunner(toRemove);
}

void oEvent::sanityCheck(gdioutput &gdi, bool expectResult, int onlyThisClass) {
  bool hasResult = false;
  bool warnNoName = false;
  bool warnNoClass = false;
  bool warnNoTeam = false;
  bool warnNoPatrol = false;
  bool warnIndividualTeam = false;

  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (onlyThisClass > 0 && it->getClassId(false) != onlyThisClass)
      continue;
    if (it->sName.empty()) {
      if (!warnNoName) {
        warnNoName = true;
        gdi.alert("Varning: deltagare med blankt namn påträffad. MeOS "
                  "kräver att alla deltagare har ett namn, och tilldelar namnet 'N.N.'");
      }
      it->setName(lang.tl("N.N."), false);
      it->synchronize();
    }

    if (!it->Class) {
      if (!warnNoClass) {
        gdi.alert(L"Deltagaren 'X' saknar klass.#" + it->getName());
        warnNoClass = true;
      }
      continue;
    }

    if (!it->tInTeam) {
      ClassType type = it->Class->getClassType();
      int cid = it->Class->getId();
      if (type == oClassIndividRelay) {
        it->setClassId(0, true);
        it->setClassId(cid, true);
        it->synchronizeAll();
      }
      else if (type == oClassRelay) {
        if (!warnNoTeam) {
          gdi.alert(L"Deltagaren 'X' deltar i stafettklassen 'Y' men saknar lag. Klassens start- "
                    L"och resultatlistor kan därmed bli felaktiga.#" + it->getName() +
                     L"#" + it->getClass(false));
          warnNoTeam = true;
        }
      }
      else if (type == oClassPatrol) {
        if (!warnNoPatrol) {
          gdi.alert(L"Deltagaren 'X' deltar i patrullklassen 'Y' men saknar patrull. Klassens start- "
                    L"och resultatlistor kan därmed bli felaktiga.#" + it->getName() +
                     + L"#" + it->getClass(false));
          warnNoPatrol = true;
        }
      }
    }

    if (it->getFinishTime()>0)
      hasResult = true;
  }

  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;
    
    if (onlyThisClass > 0 && it->getClassId(false) != onlyThisClass)
      continue;

    if (it->sName.empty()) {
      if (!warnNoName) {
        warnNoName = true;
        gdi.alert("Varning: lag utan namn påträffat. "
                  "MeOS kräver att alla lag har ett namn, och tilldelar namnet 'N.N.'");
      }
      it->setName(lang.tl("N.N."), false);
      it->synchronize();
    }

    if (!it->Class) {
      if (!warnNoClass) {
        gdi.alert(L"Laget 'X' saknar klass.#" + it->getName());
        warnNoClass = true;
      }
      continue;
    }

    ClassType type = it->Class->getClassType();
    if (type == oClassIndividual) {
      if (!warnIndividualTeam) {
        gdi.alert(L"Laget 'X' deltar i individuella klassen 'Y'. Klassens start- och resultatlistor "
                  L"kan därmed bli felaktiga.#" + it->getName() + L"#" + it->getClass(true));
        warnIndividualTeam = true;
      }
    }
  }


  if (expectResult && !hasResult)
    gdi.alert("Tävlingen innehåller inga resultat.");


  bool warnBadStart = false;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassStatus() != oClass::ClassStatus::Normal)
      continue;

    if (onlyThisClass > 0 && it->getId() != onlyThisClass)
      continue;

    if (it->getQualificationFinal())
      continue;

    if (it->hasMultiCourse()) {
      for (unsigned k=0;k<it->getNumStages(); k++) {
        StartTypes st = it->getStartType(k);
        LegTypes lt = it->getLegType(k);
        if (k==0 && (st == STChange || st == STHunting) && !warnBadStart) {
          warnBadStart = true;
          gdi.alert(L"Klassen 'X' har jaktstart/växling på första sträckan.#" + it->getName());
        }
        if (st == STTime && it->getStartData(k)<=0 && !warnBadStart &&
              (lt == LTNormal || lt == LTSum)) {
          warnBadStart = true;
          gdi.alert(L"Ogiltig starttid i 'X' på sträcka Y.#" + it->getName() + L"#" + itow(k+1));
        }
      }
    }
  }
}

oTimeLine::~oTimeLine() {

}

void oEvent::remove() {
  if (isClient())
   dropDatabase();
  else
   deleteCompetition();

  clearListedCmp();
  newCompetition(L"");
}

bool oEvent::canRemove() const {
  return true;
}

wstring oEvent::formatCurrency(int c, bool includeSymbol) const {
  if (tCurrencyFactor == 1)
    if (!includeSymbol)
      return itow(c);
    else if (tCurrencyPreSymbol)
      return tCurrencySymbol + itow(c);
    else
      return itow(c) + tCurrencySymbol;
  else {
    wchar_t bf[32];
    if (includeSymbol) {
      swprintf_s(bf, 32, L"%d%s%02d", c/tCurrencyFactor,
                 tCurrencySeparator.c_str(), c%tCurrencyFactor);

      if (tCurrencyPreSymbol)
        return tCurrencySymbol + bf;
      else
        return bf + tCurrencySymbol;
    }
    else {
      swprintf_s(bf, 32, L"%d.%02d", c/tCurrencyFactor, c%tCurrencyFactor);
      return bf;
    }
  }
}

int oEvent::interpretCurrency(const wstring &c) const {
  if (tCurrencyFactor == 1 && tCurrencyPreSymbol == false)
    return _wtoi(c.c_str());

  size_t s = 0;
  while (s < c.length() && (c[s]<'0' || c[s]>'9'))
    s++;

  wstring cc = c.substr(s);

  for (size_t k = 0; k<cc.length(); k++) {
    if (cc[k] == ',' || cc[k] == tCurrencySeparator[0])
      cc[k] = '.';
  }

  return int(_wtof(cc.c_str())*tCurrencyFactor);
}

int oEvent::interpretCurrency(double val, const wstring &cur)  {
  if (_wcsicmp(L"sek", cur.c_str()) == 0)
    setCurrency(1, L"kr", L",", false);
  else if (_wcsicmp(L"eur", cur.c_str()) == 0)
    setCurrency(100, L"€", L".", false);//WCS

  return int(floor(val * tCurrencyFactor+0.5));
}

void oEvent::setCurrency(int factor, const wstring &symbol, const wstring &separator, bool preSymbol) {
  if (factor == -1) {
    // Load from data
    int cf = getDCI().getInt("CurrencyFactor");
    if (cf != 0)
      tCurrencyFactor = cf;

    wstring cs = getDCI().getString("CurrencySymbol");
    if (!cs.empty())
      tCurrencySymbol = cs;

    cs = getDCI().getString("CurrencySeparator");
    if (!cs.empty())
      tCurrencySeparator = cs;

    int ps = getDCI().getInt("CurrencyPreSymbol");
    tCurrencyPreSymbol = (ps != 0);

    if (tCurrencySymbol.size() > 0) {
      if (tCurrencyPreSymbol) {
        wchar_t end = *tCurrencySymbol.rbegin();
        if ((end>='a' && end <='z') || end>='A' && end <='Z')
          tCurrencySymbol += L" ";
      }
      else {
        wchar_t end = *tCurrencySymbol.begin();
        if ((end>='a' && end <='z') || end>='A' && end <='Z')
          tCurrencySymbol = L" " + tCurrencySymbol;
      }
    }
  }
  else {
    tCurrencyFactor = factor;
    tCurrencySymbol = symbol;
    tCurrencySeparator = separator;
    tCurrencyPreSymbol = preSymbol;
    getDI().setString("CurrencySymbol", symbol);
    getDI().setInt("CurrencyFactor", factor);
    getDI().setString("CurrencySeparator", separator);
    getDI().setInt("CurrencyPreSymbol", preSymbol ? 1 : 0);
  }
}

MetaListContainer &oEvent::getListContainer() const {
  if (!listContainer)
    throw std::exception("Nullpointer exception");
  return *listContainer;
}

void oEvent::setExtraLines(const char *attrib, const vector< pair<wstring, int> > &lines) {
  wstring str;

  for(size_t k = 0; k < lines.size(); k++) {
    if (k>0)
      str.push_back('|');

    wstring msg = lines[k].first;
    for (size_t i = 0; i < msg.size(); i++) {
      if (msg[i] == '|')
        str.push_back(':'); // Encoding does not support |
      else
        str.push_back(msg[i]);
    }
    str.push_back('|');
    str.append(itow(lines[k].second));
  }
  getDI().setString(attrib, str);
}

void oEvent::getExtraLines(const char *attrib, vector< pair<wstring, int> > &lines) const {
  vector<wstring> splt;
  const wstring &splitPrintExtra = getDCI().getString(attrib);
  split(splitPrintExtra, L"|", splt);
  lines.clear();
  lines.reserve(splt.size() / 2);
  for (size_t k = 0; k + 1 < splt.size(); k+=2) {
    lines.push_back(make_pair(splt[k], _wtoi(splt[k+1].c_str())));
  }

  while(!lines.empty()) {
    if (lines.back().first.length() == 0)
      lines.pop_back();
    else break;
  }
}

oEvent::MultiStageType oEvent::getMultiStageType() const {
  if (getDCI().getString("PreEvent").empty())
    return MultiStageNone;
  else
    return MultiStageSameEntry;
}

bool oEvent::hasNextStage() const {
  return !getDCI().getString("PostEvent").empty();
}

bool oEvent::hasPrevStage() const {
  return !getDCI().getString("PreEvent").empty() || getStageNumber() > 1;
}

int oEvent::getNumStages() const {
  int ns = getDCI().getInt("NumStages");
  if (ns>0)
    return ns;
  else
    return 1;
}

void oEvent::setNumStages(int numStages) {
  getDI().setInt("NumStages", numStages);
}

int oEvent::getStageNumber() const {
  return getDCI().getInt("EventNumber");
}

void oEvent::setStageNumber(int num) {
  getDI().setInt("EventNumber", num);
}

oDataContainer &oEvent::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<pvectorstr>(&dynamicData);
  return *oEventData;
}

void oEvent::changedObject() {
  globalModification = true;
}

void oEvent::pushDirectChange() {
  PostMessage(gdibase.getHWNDMain(), WM_USER + 4, 0, 0);
}

int oEvent::getBibClassGap() const {
  int ns = getDCI().getInt("BibGap");
  return ns;
}

void oEvent::setBibClassGap(int numStages) {
  getDI().setInt("BibGap", numStages);
}

void oEvent::checkNecessaryFeatures() {
  bool hasMultiRace = false;
  bool hasRelay = false;
  bool hasPatrol = false;
  bool hasForkedIndividual = false;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    const oClass &c = *it;
    bool multiRace = false;
    bool relay = false;
    bool patrol = false;

    for (size_t j = 0; j < c.legInfo.size(); j++) {      

      if (c.legInfo[j].duplicateRunner != -1)
        multiRace = true;

      if (j > 0 && !c.legInfo[j].isParallel() && !c.legInfo[j].isOptional()) {
        relay = true;
        patrol = false;
      }

      if (j > 0 && (c.legInfo[j].isParallel() || c.legInfo[j].isOptional()) && !relay) {
        patrol = true;
      }
    }

    hasForkedIndividual |= c.legInfo.size() == 1;
    hasMultiRace |= multiRace;
    hasRelay |= relay;
    hasPatrol |= patrol;
  }

  if (hasForkedIndividual)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::ForkedIndividual, true, *this);

  if (hasRelay)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::Relay, true, *this);

  if (hasPatrol)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::Patrol, true, *this);

  if (hasMultiRace)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::MultipleRaces, true, *this);

  oe->synchronize(true);
}

bool oEvent::useLongTimes() const {
  if (tLongTimesCached != -1)
    return tLongTimesCached != 0;

  tLongTimesCached = getDCI().getInt("LongTimes");
  return tLongTimesCached != 0;
}

void oEvent::useLongTimes(bool use) {
  tLongTimesCached = use;
  getDI().setInt("LongTimes", use ? 1 : 0);
}

int oEvent::convertToFullTime(int inTime) {
  if (inTime < 0 || !useLongTimes() || inTime > 24*3600)
    return inTime;

  return inTime;
}


void oEvent::getPayModes(vector< pair<wstring, size_t> > &modes) {
  modes.clear();
  modes.reserve(10);
  vector< pair<wstring, int> > lines;
  getExtraLines("PayModes", lines);
  
  modes.push_back(make_pair(lang.tl(L"Kontant betalning"), 0));
  map<int,int> id2ix;
  id2ix[0] = 0;
  
  for (size_t k = 0; k < lines.size(); k++) {
    int id = lines[k].second;
    if (id2ix.count(id))
      modes[id2ix[id]].first = lines[k].first;
    else {
      id2ix[id] = k;
      modes.push_back(make_pair(lines[k].first, id));
    }
  }
}

void oEvent::setPayMode(int id, const wstring &mode) {
  vector< pair<wstring, int> > lines;
  getExtraLines("PayModes", lines);
  
  if (mode.empty()) {
    // Remove
    for (size_t k = 0; k < lines.size(); k++) {
      if (lines[k].second == id) {
        bool valid = id != 0;
        for (oRunnerList::const_iterator it = Runners.begin(); 
                               valid && it != Runners.end(); ++it) {
          if (it->getPaymentMode() == id)
            valid = false;
        }
        for (oTeamList::const_iterator it = Teams.begin(); 
                               valid && it != Teams.end(); ++it) {
          if (it->getPaymentMode() == id)
            valid = false;
        }

        if (!valid)
          throw meosException("Betalningsättet behövs och kan inte tas bort.");

        lines.erase(lines.begin() + k);
        k--;
      }
    }
  }
  else {
    // Add / update
    bool done = false;
    for (size_t k = 0; k < lines.size(); k++) {
      if (lines[k].second == id) {
        lines[k].first = mode;
        done = true;
        break;
      }
    }
    if (!done) {
      lines.push_back(make_pair(mode, id));
    }
  }

  setExtraLines("PayModes", lines);
}

void oEvent::useDefaultProperties(bool useDefault) {
  if (useDefault) {
    if (savedProperties.empty())
      savedProperties.swap(eventProperties);
  }
  else {
    if (!savedProperties.empty()) {
      savedProperties.swap(eventProperties);
      savedProperties.clear();
    }
  }
}

static void checkValid(oEvent &oe, int &time, int delta, const wstring &name) {
  int srcTime = time;
  time += delta;
  if (time <= 0)
    time += 24 * 3600;
  if (time > 24 * 3600)
    time -= 24 * 3600;
  if (time < 0 || time > 22 * 3600) {
    throw meosException(L"X har en tid (Y) som inte är kompatibel med förändringen.#" + name + L"#" + oe.getAbsTime(srcTime));
  }
}


void oEvent::updateStartTimes(int delta) {
  for (int pass = 0; pass <= 1; pass++) {
    for (oClass &c : Classes) {
      if (c.isRemoved())
        continue;
      for (unsigned i = 0; i < c.getNumStages(); i++) {
        int st = c.getStartData(i);
        if (st > 0) {
          checkValid(*oe, st, delta, c.getName());
          if (pass == 1) {
            c.setStartData(i, st);
            c.synchronize(true);
          }
        }
      }
    }

    if (pass == 1)
      reEvaluateAll(set<int>(), false);

    for (oRunner &r : Runners) {
      if (r.isRemoved())
        continue;
      if (r.Class  && r.Class->getStartType(r.getLegNumber()) == STDrawn) {
        int st = r.getStartTime();
        if (st > 0) {
          checkValid(*oe, st, delta, r.getName());
          if (pass == 1) {
            r.setStartTime(st, true, ChangeType::Update, false);
            r.synchronize(true);
          }
        }
      }
      int ft = r.getFinishTime();
      if (ft > 0) {
        checkValid(*oe, ft, delta, r.getName());
        if (pass == 1) {
          r.setFinishTime(ft);
          r.synchronize(true);
        }
      }
    }

    for (oCard &c : Cards) {
      if (c.isRemoved())
        continue;
      wstring desc = L"Bricka X#" + c.getCardNoString();
      for (oPunch &p : c.punches) {
        int t = p.Time;
        if (t > 0) {
          if (c.getOwner() != 0)
            checkValid(*oe, t, delta, desc);
          else {
            // Skip check
            t += delta;
            if (t <= 0)
              t += 24 * 3600;
          }

          if (pass == 1) {
            p.setTimeInt(t, false);
          }
        }
      }
    }

    for (oTeam &t : Teams) {
      if (t.isRemoved())
        continue;
      if (t.Class  && t.Class->getStartType(0) == STDrawn) {
        int st = t.getStartTime();
        if (st > 0) {
          checkValid(*oe, st, delta, t.getName());
          if (pass == 1) {
            t.setStartTime(st, true, ChangeType::Update, false);
            t.synchronize(true);
          }
        }
      }
      int ft = t.getFinishTime();
      if (ft > 0) {
        checkValid(*oe, ft, delta, t.getName());
        if (pass == 1) {
          t.setFinishTime(ft);
          t.synchronize(true);
        }
      }
    }

    for (oFreePunch &p : punches) {
      int t = p.Time;
      if (t > 0) {
        if (pass == 1) {
          t += delta;
          if (t <= 0)
            t += 24 * 3600;

          p.setTimeInt(t, false); // Skip check
        }
      }
    }
  }
}

bool oEvent::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oEvent::setFlag(TransferFlags flag, bool onoff) {
  int cf = getDCI().getInt("TransferFlags");
  cf = onoff ? (cf | flag) : (cf & (~flag));
  getDI().setInt("TransferFlags", cf);
}

string oEvent::encodeStartGroups() const {
  string ss;
  string tmp;
  for (auto &sg : startGroups) {
    tmp = itos(sg.first) + "," +
      itos(sg.second.firstStart) + "," + itos(sg.second.lastStart);
    if (!sg.second.name.empty()) {
      wstring name = sg.second.name;
      for (int j = 0; j < name.length(); j++) {
        if (name[j] == L',')
          name[j] = L'|';
        if (name[j] == L';')
          name[j] = L'^';
      }
      tmp += "," + gdioutput::toUTF8(name);
    }
    if (ss.empty())
      ss = tmp;
    else
      ss += ";" + tmp;
  }
  return ss;
}

void oEvent::decodeStartGroups(const string &enc) const {
  vector<string> g, sg;
  split(enc, ";", g);
  startGroups.clear();
  for (string &grp : g) {
    split(grp, ",", sg);
    if (sg.size() == 3 || sg.size() == 4) {
      int id = atoi(sg[0].c_str());
      int start = atoi(sg[1].c_str());
      int end = atoi(sg[2].c_str());
      wstring name;
      if (sg.size() == 4) {
        name = gdioutput::fromUTF8(sg[3]);
        for (int j = 0; j < name.length(); j++) {
          if (name[j] == L'|')
            name[j] = L',';
          if (name[j] == L'^')
            name[j] = L';';
        }
      }
      startGroups.emplace(id, StartGroupInfo(name, start, end));
    }
  }
}

void oEvent::setStartGroup(int id, int firstStart, int lastStart, const wstring &name) {
  if (firstStart < 0)
    startGroups.erase(id);
  else
    startGroups[id] = StartGroupInfo(name, firstStart, lastStart);
}

void oEvent::updateStartGroups() {
  getDI().setString("StartGroups", gdibase.widen(encodeStartGroups()));
}

void oEvent::readStartGroups() const {
  auto &sg = getDCI().getString("StartGroups");
  decodeStartGroups(gdibase.narrow(sg));
}

const map<int, StartGroupInfo> &oEvent::getStartGroups(bool reload) const {
  if (reload)
    readStartGroups();
  return startGroups;
}

StartGroupInfo oEvent::getStartGroup(int id) const {
  auto res = startGroups.find(id);
  if (res != startGroups.end())
    return res->second;
  else
    return StartGroupInfo(L"", -1, -1);
}

MachineContainer &oEvent::getMachineContainer() {
  if (!machineContainer)
    machineContainer = make_unique<MachineContainer>();

  return *machineContainer;
}
