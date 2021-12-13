#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>

class xmlparser;
class xmlobject;
using namespace std;

class MachineContainer {

public:

  class AbstractMachine {
    map<string, wstring> props;
  public:
    void clear() {
      props.clear();
    }

    int getInt(const string &v) const;
    const wstring &getString(const string &v) const;
    vector<int> getVectorInt(const string &v) const;
    set<int> getSetInt(const string &v) const;

    void set(const string &name, int v);
    void set(const string &name, const vector<int> &v);
    void set(const string &name, const wstring &v);
    void set(const string &name, bool v) {
      set(name, int(v));
    }

    template<typename T>
    void set(const string &name, const T &v) {
      vector<int> vv;
      for (auto x : v)
        vv.push_back(x);
      set(name, vv);
    }

  protected:
    void load(const xmlobject &data);
    void load(const string &data);
    void save(xmlparser &data) const;
    string save() const;
    friend class MachineContainer;
  };

private:
  map<pair<string, wstring>, AbstractMachine> machines;

public:
  const AbstractMachine *get(const string &type, const wstring &name) const {
    auto res = machines.find(make_pair(type, name));
    if (res != machines.end())
      return &res->second;

    return nullptr;
  }

  void erase(const string &type, const wstring &name) {
    machines.erase(make_pair(type, name));
  }

  AbstractMachine &set(const string &type, const wstring &name) {
    auto &m = machines[make_pair(type, name)];
    m.clear();
    return m;
  }

  vector<pair<string, wstring>> enumerate() const;

  void load(const xmlobject &data);
  void save(xmlparser &data);
  void load(const string &data);
  string save();
};
