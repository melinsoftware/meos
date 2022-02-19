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
int getMeosBuild() {
  string revision("$Rev: 1116 $");
  return 174 + atoi(revision.substr(5, string::npos).c_str());
}

wstring getMeosDate() {
  wstring date(L"$Date: 2022-02-18 14:51:32 +0100 (fre, 18 feb 2022) $");
  return date.substr(7,10);
}

wstring getBuildType() {
  return L"RC2"; // No parantheses (...)
}

wstring getMajorVersion() {
  return L"3.8";
}

wstring getMeosFullVersion() {
  wchar_t bf[256];

#ifdef _WIN64
  const wchar_t *bits = L"64-bit";
#else
  const wchar_t *bits = L"32-bit";
#endif

  wstring maj = getMajorVersion();
  if (getBuildType().empty())
    swprintf_s(bf, L"Version X#%s.%d (%s), %s", maj.c_str(), getMeosBuild(), bits, getMeosDate().c_str());
  else
    swprintf_s(bf, L"Version X#%s.%d (%s), %s, %s", maj.c_str(), getMeosBuild(), bits, getBuildType().c_str(), getMeosDate().c_str());
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
  developSupp.emplace_back(L"Almby IK, Örebro");//2019--
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
  developSupp.emplace_back(L"SongTao Wang / Henan Zhixing Exploration Sports Culture Co., Ltd.");
  developSupp.emplace_back(L"Australian and Oceania Orienteering Championships 2019");
  supp.emplace_back(L"Järfälla OK");
  supp.emplace_back(L"TJ Slávia Farmaceut Bratislava");
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
  supp.emplace_back(L"Åke Larsson, OK Hedströmmen");
  developSupp.push_back(L"Västmanlands OF");
  supp.emplace_back(L"OK Tyr, Karlstad");
  developSupp.push_back(L"OK Orion");
  supp.emplace_back(L"Mjölby OK");
  supp.emplace_back(L"Malmö OK");
  supp.emplace_back(L"OK Vilse 87");
  supp.emplace_back(L"Rehns BK");
  supp.emplace_back(L"Fredrik Magnusson, Laholms IF");
  supp.emplace_back(L"KOB ATU Košice");
  supp.emplace_back(L"Alfta-Ösa OK");
  supp.emplace_back(L"HEYRIES, ACA Aix en Provence");
  supp.emplace_back(L"IFK Kiruna");
  supp.emplace_back(L"Smedjebackens OK");
  supp.emplace_back(L"Gunnar Persson, Svanesunds GIF");
  supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
  supp.emplace_back(L"Køge Orienteringsklub");
  supp.emplace_back(L"Simrishamns OK");
  supp.emplace_back(L"OK Fryksdalen");
  supp.emplace_back(L"Magnus Asplund, Sundbybergs IK");
  supp.emplace_back(L"Frölunda OL");
  supp.emplace_back(L"Hjobygdens OK");
  supp.emplace_back(L"OK Malmia");
  supp.emplace_back(L"Säterbygdens OK");
  supp.emplace_back(L"OK Orinto");
  supp.emplace_back(L"Trosabygdens OK");
  supp.emplace_back(L"Järla Orientering");
  supp.emplace_back(L"Hans Wilhelmsson, Säffle OK");
  supp.emplace_back(L"Cent Vallées Orientation 12 (C.V.O. 12)");
  supp.emplace_back(L"OK Tyr, Karlstad");
  supp.emplace_back(L"Miroslav Kollar, KOB Kysak");
  supp.emplace_back(L"Zdenko Rohac, KOB ATU Košice");
  supp.emplace_back(L"Hans Carlstedt, Sävedalens AIK");
  supp.emplace_back(L"Javier Arufe Varela, O-Liceo");
  reverse(supp.begin(), supp.end());
}
