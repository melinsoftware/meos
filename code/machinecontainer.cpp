#include "StdAfx.h"

#include "machinecontainer.h"
#include "meos_util.h"
#include "xmlparser.h"
#include "gdioutput.h"
#include "TabAuto.h"

int MachineContainer::AbstractMachine::getInt(const string &v) const {
  return _wtoi(getString(v).c_str());
}

const wstring &MachineContainer::AbstractMachine::getString(const string &v) const {
  auto res = props.find(v);
  if (res != props.end())
    return res->second;
  return _EmptyWString;
}

vector<int> MachineContainer::AbstractMachine::getVectorInt(const string &v) const {
  auto &s = getString(v);
  vector<wstring> sp;
  split(s, L",", sp);
  vector<int> res(sp.size());
  for (size_t j = 0; j < sp.size(); j++)
    res[j] = _wtoi(sp[j].c_str());
  return res;
}

set<int> MachineContainer::AbstractMachine::getSetInt(const string &v) const {
  std::set<int> res;
  for (int i : getVectorInt(v)) {
    res.insert(i);
  }
  return res;
}

void MachineContainer::AbstractMachine::set(const string &name, int v) {
  if (v != 0)
    props[name] = itow(v);
}

void MachineContainer::AbstractMachine::set(const string &name, const vector<int> &v) {
  wstring &r = props[name];
  for (size_t j = 0; j < v.size(); j++) {
    if (j == 0)
      r = itow(v[j]);
    else
      r += L"," + itow(v[j]);
  }
}

void MachineContainer::AbstractMachine::load(const xmlobject &data) {
  xmlList out;
  data.getObjects(out);
  for (auto &x : out) {
    props[x.getName()] = x.getw();
  }
}

void MachineContainer::AbstractMachine::save(xmlparser &data) const {
  for (auto &p : props) {
    data.write(p.first.c_str(), p.second);
  }
}

namespace {
  string encode(const string &in) {
    string out;
    out.reserve(in.length());
    for (size_t j = 0; j < in.length(); j++) {
      if (in[j] == '|' || in[j] == '$' || in[j] == '%') {
        out.push_back('%');
        if (in[j] == '|')
          out.push_back('1');
        else if (in[j] == '$')
          out.push_back('2');
        else if (in[j] == '%')
          out.push_back('3');
      }
      else
        out.push_back(in[j]);
    }
    return out;
  }

  string decode(const string &in) {
    string out;
    out.reserve(in.length());
    for (size_t j = 0; j < in.length(); j++) {
      if (in[j] == '%') {
        j++;
        if (j < in.length()) {
          if (in[j] == '1')
            out.push_back('|');
          else if (in[j] == '2')
            out.push_back('$');
          else if (in[j] == '3')
            out.push_back('%');
        }
      }
      else
        out.push_back(in[j]);
    }
    return out;
  }
}

void MachineContainer::AbstractMachine::load(const string &data) {
  vector<string> parts;
  split(data, "|", parts);
  for (int j = 0; j < parts.size(); j+=2) {
    const wstring &w = gdioutput::fromUTF8(decode(parts[j + 1]));
    props[parts[j]] = w;
  }
}

string  MachineContainer::AbstractMachine::save() const {
  string out;
  for (auto &p : props) {
    if (!out.empty())
      out += "|";
    out.append(p.first);
    out += "|";
    out.append(encode(gdioutput::toUTF8(p.second)));
  }
  return out;
}

void MachineContainer::AbstractMachine::set(const string &name, const wstring &v) {
  if (!v.empty())
    props[name] = v;
}

vector<pair<string, wstring>> MachineContainer::enumerate() const {
  vector<pair<string, wstring>> res;
  for (auto v : machines)
    res.push_back(v.first);

  return res;
}

void MachineContainer::load(const xmlobject &data) {
  xmlList out;
  data.getObjects("Machine", out);

  for (auto &m : out) {
    string type;
    wstring tag;
    m.getObjectString("type", type);
    m.getObjectString("name", tag);
    if (!type.empty() && !tag.empty()) {
      auto res = machines.emplace(make_pair(type, tag), MachineContainer::AbstractMachine());
      if (res.second) 
        res.first->second.load(m);
    }
  }
}

void MachineContainer::save(xmlparser &data) {
  for (auto &m : machines) {
    vector<wstring> p({ wstring(L"type"), gdioutput::widen(m.first.first), 
                        wstring(L"name"), m.first.second });
    data.startTag("Machine", p);
    m.second.save(data);
    data.endTag();
  }
}

void MachineContainer::load(const string &data) {
  vector<string> parts;
  split(data, "$", parts);
  if ((parts.size() % 3) != 0) {
    // Data is corrupt. Repair by delete...
    if (parts.size() > 3)
      parts.resize(3);
  }

  machines.clear();
  for (size_t j = 0; j + 2 < parts.size(); j+=3) {
    const string &type = parts[j];

    if (AutoMachine::getType(type) == Machines::Unknown)
      continue;

    wstring tag = gdioutput::fromUTF8(decode(parts[j + 1]));
    auto res = machines.emplace(make_pair(type, tag), MachineContainer::AbstractMachine());
    if (res.second)
      res.first->second.load(parts[j+2]);
  }
}

string MachineContainer::save() {
  vector<string> ml;
  ml.reserve(machines.size() * 3);
  size_t tSize = 0;
  for (auto &m : machines) {
    ml.push_back(m.first.first);
    tSize += ml.back().length() + 2;
    ml.push_back(encode(gdioutput::toUTF8(m.first.second)));
    tSize += ml.back().length() + 2;
    ml.push_back(m.second.save());
    tSize += ml.back().length() + 2;
  }

  string out;
  out.reserve(tSize);

  for (const string & m : ml) {
    if (!out.empty())
      out.append("$");
    out.append(m);
  }
  return out;
}

void MachineContainer::rename(const string& type, const wstring& oldName, const wstring& newName) {
  if (newName != oldName) {
    auto res = machines.find(make_pair(type, oldName));
    if (res != machines.end()) {
      machines.emplace(make_pair(type, newName), res->second);
      machines.erase(res);
    }
  }
}
