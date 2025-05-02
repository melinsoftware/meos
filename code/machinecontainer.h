#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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
    bool has(const string& prop) const;

    void set(const string &name, int v);
    void set(const string &name, const vector<int> &v);
    void set(const string &name, const wstring &v);
    void set(const string& name, const string& v) = delete;

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

  void rename(const string& type, const wstring& oldName, const wstring& newName);

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
