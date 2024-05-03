#pragma once

#include "localizer.h"
#include "meosexception.h"
#include "oDataContainer.h"
#include "oEvent.h"
#include "Table.h"
#include "metalist.h"

class RelativeTimeFormatter : public oDataDefiner {
  string name;
public:
  RelativeTimeFormatter(const char* n) : name(n) {}

  const wstring& formatData(const oBase* obj) const override {
    int t = obj->getDCI().getInt(name);
    if (t <= 0)
      return makeDash(L"-");
    return obj->getEvent()->getAbsTime(t);
  }
  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    int t = obj->getEvent()->getRelativeTime(input);
    obj->getDI().setInt(name.c_str(), t);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class AbsoluteTimeFormatter : public oDataDefiner {
  string name;
  const SubSecond mode;
public:
  AbsoluteTimeFormatter(const char* n, SubSecond mode) : name(n), mode(mode) {}

  const wstring& formatData(const oBase* obj) const override {
    int t = obj->getDCI().getInt(name);
    return formatTime(t, mode);
  }
  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    int t = convertAbsoluteTimeMS(input);
    if (t == NOTIME)
      t = 0;
    obj->getDI().setInt(name.c_str(), t);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class PayMethodFormatter : public oDataDefiner {
  mutable vector< pair<wstring, size_t> > modes;
  mutable map<wstring, int> setCodes;
  mutable long rev;
public:
  PayMethodFormatter() : rev(-1) {}

  void prepare(oEvent* oe) const override {
    oe->getPayModes(modes);
    for (size_t i = 0; i < modes.size(); i++) {
      setCodes[canonizeName(modes[i].first.c_str())] = modes[i].second;
    }
  }

  const wstring& formatData(const oBase* ob) const override {
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

  pair<int, bool> setData(oBase* ob, const wstring& input, wstring& output, int inputId) const override {
    auto res = setCodes.find(canonizeName(input.c_str()));
    if (res != setCodes.end()) {
      ob->getDI().setInt("PayMode", res->second);
    }
    output = formatData(ob);
    return make_pair(0, false);
  }

  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, true);
  }
};

class RankScoreFormatter : public oDataDefiner {
  mutable int type;
public:
  RankScoreFormatter() : type(0) {}

  const wstring& formatData(const oBase* ob) const override {
    const oRunner* r = (const oRunner*)ob;
    int rank = ob->getDCI().getInt("Rank");
    if (rank == 0)
      return _EmptyWString;
    else {
      if (rank < MaxOrderRank) {
        type = 1;
        return itow(rank);
      }
      else {
        type = 2;
        wstring& res = StringCache::getInstance().wget();
        res = r->getRankingScore();
        return res;
      }
      return _EmptyWString;
    }
  }

  pair<int, bool> setData(oBase* ob, const wstring& input, wstring& output, int inputId) const override {
    if (input.empty())
      type = 0;
    if (type == 2 || input.find_first_of('.') != wstring::npos) {
      pRunner r = pRunner(ob);
      r->setRankingScore(_wtof(input.c_str()));
    }
    else {
      int rank = _wtoi(input.c_str());
      if (rank >= 0 && rank < MaxOrderRank)
        ob->getDI().setInt("Rank", rank);
    }
    return make_pair(0, false);
  }

  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, true);
  }
};

class StartGroupFormatter : public oDataDefiner {
  mutable long rev = -1;
  mutable map<int, wstring> sgmap;
  mutable wstring out;

  int static getGroup(const oBase* ob) {
    const oRunner* r = dynamic_cast<const oRunner*>(ob);
    int sg = 0;
    if (r)
      sg = r->getStartGroup(false);
    else {
      const oClub* c = dynamic_cast<const oClub*>(ob);
      if (c)
        sg = c->getStartGroup();
    }
    return sg;
  }

public:
  StartGroupFormatter() {}

  void prepare(oEvent* oe) const override {
    auto& sg = oe->getStartGroups(true);
    for (auto& g : sg) {
      int t = g.second.firstStart;
      sgmap[g.first] = oe->getAbsTimeHM(t);
    }
  }

  const wstring& formatData(const oBase* ob) const override {
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

  pair<int, bool> setData(oBase* ob, const wstring& input, wstring& output, int inputId) const override {
    int g = inputId;
    if (inputId <= 0 && !input.empty()) {
      vector<wstring> sIn;
      split(input, L" ", sIn);
      for (wstring& in : sIn) {
        int num = _wtoi(in.c_str());
        if (in.find_first_of(':') != input.npos) {
          int t = ob->getEvent()->convertAbsoluteTime(input);
          if (t > 0) {
            for (auto& sg : ob->getEvent()->getStartGroups(false)) {
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
    oRunner* r = dynamic_cast<oRunner*>(ob);
    if (r) {
      r->setStartGroup(g);
    }
    else {
      oClub* c = dynamic_cast<oClub*>(ob);
      if (c)
        c->setStartGroup(g);
    }
    output = formatData(ob);
    return make_pair(0, false);
  }

  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, false);
  }

  // Return the desired cell type
  CellType getCellType() const {
    return CellType::cellSelection;
  }

  void fillInput(const oBase* obj, vector<pair<wstring, size_t>>& out, size_t& selected) const final {
    if (obj->getEvent()->getRevision() != rev)
      prepare(obj->getEvent());

    int sg = getGroup(obj);

    out.emplace_back(_EmptyWString, 0);
    selected = 0;
    for (auto& v : sgmap) {
      out.emplace_back(v.second, v.first);

      if (sg == v.first)
        selected = sg;
    }
  }
};


class DataHider : public oDataDefiner {
public:

  const wstring& formatData(const oBase* obj) const override {
    return _EmptyWString;
  }
  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    return make_pair(0, false);
  }
  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return -1;
  }
};

class DataBoolean : public oDataDefiner {
  string attrib;
public:
  DataBoolean(const string& attrib) : attrib(attrib) {}

  const wstring& formatData(const oBase* obj) const override {
    int v = obj->getDCI().getInt(attrib);
    return lang.tl(v ? "true[boolean]" : "false[boolean]");
  }
  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    bool v = compareStringIgnoreCase(L"true", input) == 0 || _wtoi64(input.c_str()) > 0;
    if (!v) {
      const wstring& T = lang.tl("true[boolean]");
      v = compareStringIgnoreCase(T, input) == 0;
    }
    obj->getDI().setInt(attrib.c_str(), v);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), true, true);
  }
};

class ResultModuleFormatter : public oDataDefiner {
public:

  const wstring& formatData(const oBase* obj) const override {
    return obj->getDCI().getString("Result");
  }
  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    string tag(input.begin(), input.end());
    dynamic_cast<oClass&>(*obj).setResultModule(tag);
    output = formatData(obj);
    return make_pair(0, false);
  }
  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class SplitPrintListFormatter : public oDataDefiner {
public:

  const wstring& formatData(const oBase* obj) const override {
    wstring listId = obj->getDCI().getString("SplitPrint");
    if (listId.empty()) {
      return lang.tl("Standard");
    }
    try {
      const MetaListContainer& lc = obj->getEvent()->getListContainer();
      EStdListType type = lc.getCodeFromUnqiueId(gdioutput::narrow(listId));
      const MetaList& ml = lc.getList(type);
      return ml.getListName();
    }
    catch (meosException&) {
      return _EmptyWString;
    }
  }

  void fillInput(const oBase* obj, vector<pair<wstring, size_t>>& out, size_t& selected) const {
    oEvent* oe = obj->getEvent();
    oe->getListContainer().getLists(out, false, false, false, true);
    out.insert(out.begin(), make_pair(lang.tl("Standard"), -10));
    wstring listId = obj->getDCI().getString("SplitPrint");
    EStdListType type = oe->getListContainer().getCodeFromUnqiueId(gdioutput::narrow(listId));
    if (type == EStdListType::EStdNone)
      selected = -10;
    else {
      for (auto& t : out) {
        if (type == oe->getListContainer().getType(t.second)) {
          selected = t.second;
          break;
        }
      }
    }
  }

  CellType getCellType() const final {
    return CellType::cellSelection;
  }

  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    if (inputId == -10)
      obj->getDI().setString("SplitPrint", L"");
    else {
      EStdListType type = obj->getEvent()->getListContainer().getType(inputId);
      string id = obj->getEvent()->getListContainer().getUniqueId(type);
      obj->getDI().setString("SplitPrint", gdioutput::widen(id));
    }

    output = formatData(obj);
    return make_pair(0, false);
  }

  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    oEvent* oe = table->getEvent();
    vector<pair<wstring, size_t>> out;
    oe->getListContainer().getLists(out, false, false, false, true);

    for (auto& t : out) {
      minWidth = max<int>(minWidth, t.first.size() * 6);
    }

    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class AnnotationFormatter : public oDataDefiner {
  mutable int numChar = 12;
public:

  const wstring& formatData(const oBase* obj) const override {
    const wstring& ws = obj->getDCI().getString("Annotation");
    if (ws.empty())
      return ws;
    int pos = ws.find_first_of('@');
    if (pos != wstring::npos)
      return limitText(ws.substr(pos + 1), numChar);
    return limitText(ws, numChar);
  }

  bool canEdit() const override {
    return false;
  }

  pair<int, bool> setData(oBase* obj, const wstring& input, wstring& output, int inputId) const override {
    return make_pair(0, false);
  }

  int addTableColumn(Table* table, const string& description, int minWidth) const override {
    numChar = minWidth / 5;
    return table->addColumn(description, max(minWidth, 90), false, true);
  }
};

class PaymentChangedNf : public oDataNotifier {
  void notify(oBase* ob, int oldValue, int newValue) final {
    oRunner* r = (oRunner *)ob;
    if (r->payBeforeResult(true)) {
      bool wasOK = oldValue >= r->getEntryFee();
      if (!wasOK && r->getStatus() == StatusDQ)
        r->setStatus(RunnerStatus::StatusUnknown, true, oBase::ChangeType::Update, false);
      vector<int> mp;
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    }
  }
};

class FeeChangedNf : public oDataNotifier {
  void notify(oBase* ob, int oldValue, int newValue) final {
    oRunner* r = (oRunner*)ob;
    if (r->payBeforeResult(true)) {
      bool wasOK = r->getDCI().getInt("Paid") >= oldValue;
      if (!wasOK && r->getStatus() == StatusDQ)
        r->setStatus(RunnerStatus::StatusUnknown, true, oBase::ChangeType::Update, false);
      vector<int> mp;
      r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    }
  }
};
