#include "stdafx.h"

#include "cardsystem.h"
#include "csvparser.h"
#include "meosexception.h"

void CardSystem::load(const wstring& fn) {
  unknown = L"?";
  // Format first-last;numpunch
  csvparser cards;
  list<vector<wstring>> rows;
  cards.parse(fn, rows);
  if (rows.empty() || rows.begin()->size() != 1 || (*rows.begin())[0].empty())
    throw meosException(L"Invalid file format: " + fn);

  systemName = (*rows.begin())[0];
  auto it = rows.begin();
  vector<wstring> sp;
  while (++it != rows.end()) {
    if (it->empty())
      continue;

    if (it->size() < 3 || it->size() > 4)
      throw meosException(L"Invalid file format: " + fn);
    wstring name;
    if (it->size() == 4)
      name = (*it)[3];

    split((*it)[0], L"-", sp);
    if (sp.size() != 2)
      throw meosException(L"Invalid file format: " + fn);

    int a = _wtoi(sp[0].c_str());
    int b = _wtoi(sp[1].c_str());
    int numPunch = _wtoi((*it)[1].c_str());
    if (b<a || a<=0 || b <= 0 || numPunch<1 || numPunch>=1000)
      throw meosException(L"Invalid card specification: " + fn);

    if (getMaxNumPunch(a) != -1 || getMaxNumPunch(b) != -1)
      throw meosException(L"Overlapping card number intervals: " + fn + L" " + itow(a) + L"-" + itow(b));

    bool deprecated = (*it)[2] == L"deprecated";

    cardNoToMaxNumPunch.emplace(a, tuple(numPunch, false, deprecated, name));
    cardNoToMaxNumPunch.emplace(b, tuple(numPunch, true, deprecated, name));
  }
}

const tuple<int, bool, bool, wstring> *CardSystem::getTuple(int cardNo) const {
  auto lb = cardNoToMaxNumPunch.lower_bound(cardNo);

  if (lb == cardNoToMaxNumPunch.end())
    return nullptr;

  if (cardNo == lb->first)
    return &lb->second;

  if (lb == cardNoToMaxNumPunch.begin())
    return nullptr;
  auto down = std::prev(lb);
  if (cardNo < down->first || std::get<0>(down->second) != std::get<0>(lb->second) || std::get<1>(down->second))
    return nullptr;

  return &down->second;
}

int CardSystem::getMaxNumPunch(int cardNo) const {
  auto tp = getTuple(cardNo);
  if (tp)
    return std::get<0>(*tp);
  else
    return -1;
}

bool CardSystem::isDeprecated(int cardNo) const {
  auto tp = getTuple(cardNo);
  if (tp)
    return std::get<2>(*tp);
  else
    return false;
}

const wstring &CardSystem::getType(int cardNo) const {
  auto tp = getTuple(cardNo);
  if (tp)
    return std::get<3>(*tp);
  else
    return unknown;
}
