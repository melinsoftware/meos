#pragma once
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


template<class T, class KEY = int> class intkeymap {
private:
  const static KEY NoKey = -1013;

  struct keypair {
    KEY key;
    T value;
  };
  T dummy;
  T tmp;
  keypair *keys;
  unsigned siz;
  unsigned used;
  intkeymap *next;
  intkeymap *parent;
  double allocFactor;
  T noValue;
  unsigned hash1;
  unsigned hash2;
  int level;
  static int optsize(int arg);

  T &rehash(int size, KEY key, const T &value);
  T &get(const KEY key);

  void *lookup(KEY key) const;
public:
  virtual ~intkeymap();
  intkeymap(int size);
  intkeymap();
  intkeymap(const intkeymap &co);
  const intkeymap &operator=(const intkeymap &co);

  bool empty() const;
  int size() const;
  int getAlloc() const {return siz;}
  void clear();

  void resize(int size);
  int count(KEY key) {
    return lookup(key, dummy) ? 1:0;
  }
  bool lookup(KEY key, T &value) const;

  void insert(KEY key, const T &value);
  void remove(KEY key);
  void erase(KEY key) {remove(key);}
  const T operator[](KEY key) const
    {if (lookup(key,tmp)) return tmp; else return T();}

  T &operator[](KEY key) {
    return get(key);
  }
};
