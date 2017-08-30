#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2015 Melin Software HB

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
    Stigbergsvägen 7, SE-75242 UPPSALA, Sweden

************************************************************************/

#include "stdafx.h"
#include "intkeymap.hpp"



template <class T, class KEY> intkeymap<T, KEY>::intkeymap()  {
  siz = 17;
  allocFactor = 1.5;
  keys = new keypair[siz];
  hash1 = siz / 2 + 3;
  hash2 = siz / 3 + 2;
  used = 0;
  next = 0;
  level = 0;
  parent = 0;
  dummy = 0;
  noValue = 0;
  clear();
}

template <class T, class KEY> intkeymap<T, KEY>::intkeymap(int _size)
{
  allocFactor = 1.3;
  siz = optsize(_size);
  keys = new keypair[siz];
  hash1 = siz / 2 + 3;
  hash2 = siz / 3 + 2;
  used = 0;
  next = 0;
  level = 0;
  parent = 0;
  dummy = T();
  noValue = T();
  clear();
}

template <class T, class KEY> intkeymap<T, KEY>::intkeymap(const intkeymap &co)
{
  allocFactor =  co.allocFactor;
  siz = co.siz;
  keys = new keypair[siz];
  hash1 = co.hash1;
  hash2 = co.hash2;
  used = co.used;
  level = co.level;
  dummy = co.dummy;
  noValue = co.noValue;

  for (unsigned k=0; k<siz; k++)
    keys[k] = co.keys[k];

  parent = 0;
  next = 0;

  if (co.next) {
    next = new intkeymap<T, KEY>(*co.next);
    next->parent = this;
  }
}

template <class T, class KEY> intkeymap<T, KEY>::~intkeymap()
{
  delete[] keys;
  if (next)
    delete next;
}

template <class T, class KEY> void intkeymap<T, KEY>::clear()
{
  for (unsigned k=0;k<siz;k++)
    keys[k].key = NoKey;

  used = 0;
  delete next;
  next = 0;
}

template <class T, class KEY> void intkeymap<T, KEY>::insert(KEY key, const T &value)
{
  if (key == NoKey) {
    noValue = value;
    return;
  }

  keypair *ptr = (keypair *)lookup(key);
  if (ptr) {
    ptr->key = key;
    ptr->value = value;
    return;
  }

  unsigned hk = unsigned(key) % siz;

  if (keys[hk].key == NoKey)
    used++;
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    keys[hk].value = value;
    return;
  }

  hk = unsigned(key + hash1) % siz;

  if (keys[hk].key == NoKey)
    used++;
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    keys[hk].value = value;
    return;
  }

  hk = unsigned(key + hash2) % siz;

  if (keys[hk].key == NoKey)
    used++;
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    keys[hk].value = value;
    return;
  }

  if (next) {
    next->insert(key, value);
    return;
  }
  else if (level < 3) {
    next = new intkeymap<T, KEY>(siz/2);
    next->level = level + 1;
    next->parent = this;
    next->insert(key, value);
    return;
  }

  rehash(0, key, value);
}

template <class T, class KEY> T &intkeymap<T, KEY>::get(KEY key)
{
  keypair *ptr = (keypair *)lookup(key);
  if (ptr)
    return ptr->value;

  if (key == NoKey)
    return noValue;

  unsigned hk = unsigned(key) % siz;

  if (keys[hk].key == NoKey) {
    used++;
    keys[hk].value = T();
  }
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    return keys[hk].value;
  }

  hk = unsigned(key + hash1) % siz;

  if (keys[hk].key == NoKey) {
    used++;
    keys[hk].value = T();
  }
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    return keys[hk].value;
  }

  hk = unsigned(key + hash2) % siz;

  if (keys[hk].key == NoKey) {
    keys[hk].value = T();
    used++;
  }
  if (keys[hk].key == NoKey || keys[hk].key == key) {
    keys[hk].key = key;
    return keys[hk].value;
  }

  if (next) {
    return next->get(key);
  }
  else if (level < 3) {
    next = new intkeymap<T, KEY>(siz/2);
    next->level = level + 1;
    next->parent = this;
    return next->get(key);
  }

  return rehash(0, key, T());
}


template <class T, class KEY> void intkeymap<T, KEY>::remove(KEY key)
{
  unsigned hk = unsigned(key) % siz;
  if (keys[hk].key == key) {
    keys[hk].key = NoKey;
    used--;
    return;
  }

  hk = unsigned(key + hash1) % siz;
  if (keys[hk].key == key) {
    keys[hk].key = NoKey;
    used--;
    return;
  }

  hk = unsigned(key + hash2) % siz;
  if (keys[hk].key == key) {
    keys[hk].key = NoKey;
    used--;
    return;
  }

  if (next) {
    next->remove(key);
    return;
  }
}

template <class T, class KEY> T &intkeymap<T, KEY>::rehash(int _siz, KEY key, const T &value)
{
  if (parent)
    return parent->rehash(_siz+used, key, value);
  else {
    intkeymap<T, KEY> nm(int((_siz+used)*allocFactor));
    if (key != NoKey)
      nm.insert(key, value);

    intkeymap<T, KEY> *tmap = this;
    while (tmap) {
      int tsize = tmap->siz;
      keypair *tkeys = tmap->keys;
      for (int k=0; k<tsize; k++) {
        if (tkeys[k].key != NoKey)
          nm.insert(tkeys[k].key, tkeys[k].value);
      }
      tmap = tmap->next;
    }

    // Swap
    keypair *oldkeys = keys;

    //Take next
    delete next;
    next = nm.next;
    nm.next = 0;
    if (next)
      next->parent = this;

    //Take keys
    keys = nm.keys;
    nm.keys = 0;
    delete[] oldkeys;

    // Copy relevant data
    siz = nm.siz;
    used = nm.used;
    hash1 = nm.hash1;
    hash2 = nm.hash2;

    if (key!=NoKey)
      return get(key);
    return dummy;
  }
}

template <class T, class KEY> bool intkeymap<T, KEY>::lookup(KEY key, T &value) const
{
  keypair *ptr = (keypair *)lookup(key);
  if (ptr) {
    value = ptr->value;
    return true;
  }
  else {
    value = T();
    return false;
  }
}

template <class T, class KEY> void *intkeymap<T, KEY>::lookup(KEY key) const
{
  if (key == NoKey) {
    return 0;
  }

  unsigned hk = unsigned(key) % siz;
  if (keys[hk].key == key) {
    return (void *)&keys[hk];
  }

  hk = unsigned(key + hash1) % siz;
  if (keys[hk].key == key) {
    return (void *)&keys[hk];
  }

  hk = unsigned(key + hash2) % siz;
  if (keys[hk].key == key) {
    return (void *)&keys[hk];
  }

  if (next)
    return next->lookup(key);
  else
    return 0;
}

template <class T, class KEY> int intkeymap<T, KEY>::optsize(int a) {
  if (a<5)
    a = 5;

  if ((a&1) == 0)
    a++;

  while (true)  {
    if (a%3 == 0)
      a+=2;
    else if (a%5 == 0)
      a+=2;
    else if (a%7 == 0)
      a+=2;
    else if (a%11 == 0)
      a+=2;
    else if (a%13 == 0)
      a+=2;
    else
      return a;
  }
}

template <class T, class KEY> int intkeymap<T, KEY>::size() const
{
  if (next)
    return used + next->size();
  else
    return used;
}

template <class T, class KEY> bool intkeymap<T, KEY>::empty() const
{
  return used==0 && (next==0 || next->empty());
}

template <class T, class KEY> void intkeymap<T, KEY>::resize(int size)
{
  allocFactor = 1.0;
  rehash(size, NoKey, 0);
  allocFactor = 1.3;
}
