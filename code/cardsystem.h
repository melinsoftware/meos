#pragma once

#include <map>
#include <string>
#include <tuple>
using std::map;
using std::string;
using std::wstring;
using std::tuple;

class CardSystem {
  wstring systemName;
  wstring unknown;
  // Target tuple is [numPunch, (upper/lower bound, true/false), deprecated, name of card]
  map<int, tuple<int, bool, bool, wstring>> cardNoToMaxNumPunch;

  const tuple<int, bool, bool, wstring>* getTuple(int cardNo) const;

public:
  void load(const wstring& fn);
  int getMaxNumPunch(int cardNo) const;
  bool isDeprecated(int cardNo) const;
  const wstring& getType(int cardNo) const;
};

