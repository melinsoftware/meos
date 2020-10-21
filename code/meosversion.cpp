﻿/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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
#include <vector>
#include "meos_util.h"

//ABCDEFGHIJKLMNO
//V2: ABCDEFGHIHJKMN
//V31: a
//V33: abcde
//V35: abcdef
//V36: abcdef
int getMeosBuild() {
  string revision("$Rev: 1047 $");
  return 174 + atoi(revision.substr(5, string::npos).c_str());
}

//V37: ab
wstring getMeosDate() {
  wstring date(L"$Date: 2020-09-06 08:57:56 +0200 (sö, 06 sep 2020) $");
  return date.substr(7,10);
}

wstring getBuildType() {
  return L"ET"; // No parantheses (...)
}

wstring getMajorVersion() {
  return L"3.7SD";
}

wstring getMeosFullVersion() {
  wchar_t bf[256];
  wstring maj = getMajorVersion();
  if (getBuildType().empty())
    swprintf_s(bf, L"Version X#%s.%d, %s", maj.c_str(), getMeosBuild(), getMeosDate().c_str());
  else
    swprintf_s(bf, L"Version X#%s.%d, %s, %s", maj.c_str(), getMeosBuild(), getBuildType().c_str(), getMeosDate().c_str());
  return bf;
}

wstring getMeosCompectVersion() {
  if (getBuildType().empty())
    return getMajorVersion() + L"." + itow(getMeosBuild());
  else
    return getMajorVersion() + L"." + itow(getMeosBuild()) + L" (" + getBuildType() + L")";
}

void getSupporters(vector<wstring> &supp, vector<wstring> &developSupp)
{
  supp.emplace_back(L"Tjalve IF");
  supp.emplace_back(L"Nyköpings Orienteringsklubb");
  supp.emplace_back(L"Motionsorientering Göteborg");
  supp.emplace_back(L"OK Måsen");
  supp.emplace_back(L"IF Thor");
  supp.emplace_back(L"SOS Jindřichův Hradec");
  supp.emplace_back(L"KOB ATU Košice");
  supp.emplace_back(L"Mats Holmberg, OK Gränsen");
  supp.emplace_back(L"Christoffer Ohlsson, Uddevalla OK");
  supp.emplace_back(L"KOB ATU Košice");
  supp.emplace_back(L"O-Ringen AB");
  supp.emplace_back(L"Hans Carlstedt, Sävedalens AIK");
  supp.emplace_back(L"IFK Mora OK");
  supp.emplace_back(L"Attunda OK");
  supp.emplace_back(L"Siguldas Takas, Latvia");
  supp.emplace_back(L"Eric Teutsch, Ottawa Orienteering Club, Canada");
  supp.emplace_back(L"Silkeborg OK, Denmark");
  supp.emplace_back(L"Erik Ivarsson Sandberg");
  supp.emplace_back(L"Stenungsunds OK");
  supp.emplace_back(L"OK Leipzig");
  supp.emplace_back(L"Degerfors OK");
  supp.emplace_back(L"OK Tjärnen");
  supp.emplace_back(L"Leksands OK");  
  supp.emplace_back(L"O-Travel");
  supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
  developSupp.emplace_back(L"KOB Kysak");
  supp.emplace_back(L"Richard HEYRIES");
  supp.emplace_back(L"Ingemar Carlsson");
  supp.emplace_back(L"Tolereds AIK");
  supp.emplace_back(L"OK Snab");
  supp.emplace_back(L"OK 73");
  supp.emplace_back(L"Herlufsholm OK");
  supp.emplace_back(L"Helsingborgs SOK");
  supp.emplace_back(L"Sala OK");
  supp.emplace_back(L"OK Roskilde");
  developSupp.emplace_back(L"Almby IK, Örebro");
  supp.emplace_back(L"Ligue PACA");
  supp.emplace_back(L"SC vebr-sport");
  supp.emplace_back(L"IP Skogen Göteborg");
  supp.emplace_back(L"Smedjebackens Orientering");
  supp.emplace_back(L"Gudhems IF");
  supp.emplace_back(L"Kexholm SK");
  supp.emplace_back(L"Utby IK");
  supp.emplace_back(L"JWOC 2019");
  developSupp.emplace_back(L"OK Nackhe");
  supp.emplace_back(L"OK Rodhen");
  supp.emplace_back(L"HEYRIES");
  developSupp.emplace_back(L"SongTao Wang / Henan Zhixing Exploration Sports Culture Co., Ltd.");
  developSupp.emplace_back(L"Australian and Oceania Orienteering Championships 2019");
  supp.emplace_back(L"Järfälla OK");
  supp.emplace_back(L"TJ Slávia Farmaceut Bratislava");
  supp.emplace_back(L"OK Tyr, Karlstad");
  supp.emplace_back(L"Magnus Thornell, Surahammars SOK");
  supp.emplace_back(L"Mariager Fjord OK");
  supp.emplace_back(L"Nässjö OK");
  supp.emplace_back(L"Ringsjö OK");
  supp.emplace_back(L"Big Foot Orienteers");
  supp.emplace_back(L"Erik Hulthen, Mölndal Outdoor IF");
  supp.emplace_back(L"Bay Area Orienteering Club");
  supp.emplace_back(L"Finspångs SOK");
  supp.emplace_back(L"OK Gorm, Denmark");
  supp.emplace_back(L"Nyköpings OK");
  supp.emplace_back(L"Thomas Engberg, VK Uvarna");
  supp.emplace_back(L"LG Axmalm, Sävedalens AIK");
  supp.emplace_back(L"Martin Ivarsson");
  supp.emplace_back(L"Falköpings AIK OK");
  developSupp.push_back(L"Karlskrona SOK");
  supp.emplace_back(L"Kristian Toustrup, OK Syd");
  supp.emplace_back(L"Patrick NG, HKAYP");
  supp.emplace_back(L"Lars Ove Karlsson, Västerås SOK");
  supp.emplace_back(L"OK Milan");
  reverse(supp.begin(), supp.end());
}
