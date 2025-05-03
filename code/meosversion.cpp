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
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/
#include "stdafx.h"
#include <vector>
#include "meos_util.h"

//ABCDEFGHIJKLMNOPQRSTU
int getMeosBuild() {
  string revision("$Rev: 1489 $");
  return 174 + atoi(revision.substr(5, string::npos).c_str());
}

wstring getMeosDate() {
  wstring date(L"$Date: 2025-05-03 07:07:44 +0200 (lör, 03 maj 2025) $");
  return date.substr(7,10);
}

wstring getBuildType() {
  return L""; // No parantheses (...)
}

wstring getMajorVersion() {
  return L"4.1";
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

void getSupporters(vector<wstring>& supp, vector<wstring>& developSupp){
  supp.emplace_back(L"OK Tyr, Karlstad");
  supp.emplace_back(L"Zdenko Rohac, KOB ATU Košice");
  supp.emplace_back(L"Hans Carlstedt, Sävedalens AIK");
  supp.emplace_back(L"O-Liceo, Spain");
  developSupp.emplace_back(L"Västerviks OK");
  supp.emplace_back(L"Aarhus 1900 Orientering");
  supp.emplace_back(L"Ljusne Ala OK");
  supp.emplace_back(L"Sävedalens AIK");
  supp.emplace_back(L"Foothills Wanderers Orienteering Club");
  supp.emplace_back(L"OK Gripen");
  supp.emplace_back(L"Per Ågren, OK Enen");
  supp.emplace_back(L"OK Roslagen");
  supp.emplace_back(L"OK Kolmården");
  developSupp.emplace_back(L"Orienteering Queensland Inc.");
  supp.emplace_back(L"Eksjö SOK");
  supp.emplace_back(L"Kolding OK");
  developSupp.emplace_back(L"Alfta-Ösa OK");
  supp.emplace_back(L"Erik Almséus, IFK Hedemora OK");
  supp.emplace_back(L"IK Gandvik, Skara");
  supp.emplace_back(L"Mats Kågeson");
  supp.emplace_back(L"Lerums SOK");
  supp.emplace_back(L"OSC Hamburg");
  developSupp.emplace_back(L"IFK Mora OK");
  supp.emplace_back(L"OK Rodhen");
  supp.emplace_back(L"Big Foot Orienteers");
  developSupp.emplace_back(L"OK Måsen");
  supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
  supp.emplace_back(L"Foothills Wanderers Orienteering Club");
  supp.emplace_back(L"Per Eklöf / PE Design / PE Timing");
  supp.emplace_back(L"Kvarnsvedens GOIF OK");
  supp.emplace_back(L"Ingemar Lindström, OK Österåker");
  supp.emplace_back(L"OK Österåker");
  supp.emplace_back(L"Guntars Mankus, OK Saldus");
  supp.emplace_back(L"Orienteering NSW");
  developSupp.emplace_back(L"OK Enen");
  supp.emplace_back(L"Hästveda OK");
  supp.emplace_back(L"Ingemar Carlsson, Sävedalens AIK");
  supp.emplace_back(L"Lunds OK");
  supp.emplace_back(L"Ramblers Orienteering Club, Canada");
  supp.emplace_back(L"CROCO");
  supp.emplace_back(L"Nässjö OK");
  supp.emplace_back(L"Silkeborg OK");
  supp.emplace_back(L"IK Uven");
  supp.emplace_back(L"Attunda OK");
  supp.emplace_back(L"Gunnar Svanberg");
  supp.emplace_back(L"Forsa OK");
  supp.emplace_back(L"Långhundra IF");
  supp.emplace_back(L"Mariestads friluftsklubb");
  supp.emplace_back(L"Ligue PACA");
  supp.emplace_back(L"SV Robotron Dresden");
  supp.emplace_back(L"Mats Holmberg, OK Gränsen");
  supp.emplace_back(L"Milen Marinov");
  supp.emplace_back(L"Miroslav Kollar, KOB Kysak");
  developSupp.emplace_back(L"FIF Hillerød Orientering");
  supp.emplace_back(L"Järla Orientering");
  supp.emplace_back(L"Stein Östby, Malmö OK");
  supp.emplace_back(L"Eric Teutsch (o-store.ca)");
  supp.emplace_back(L"Sportegyesület Hód-Mentor");
  developSupp.emplace_back(L"Täby OK");
  developSupp.emplace_back(L"Skogsluffarnas OK");
  developSupp.emplace_back(L"FK Friskus-Varberg");
  supp.emplace_back(L"Hagaby GoIF");
  supp.emplace_back(L"Waxholms OK");
  supp.emplace_back(L"Mariager Fjord OK");
  supp.emplace_back(L"David Ek, FK Göingarna");
  supp.emplace_back(L"Silkeborg OK");
  supp.emplace_back(L"OK73");
  supp.emplace_back(L"Ligue PACA");
  developSupp.emplace_back(L"Sävedalens AIK");
  supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
  supp.emplace_back(L"Autidó");
  supp.emplace_back(L"Tjalve OK");
  supp.emplace_back(L"TV Jahn Wolfsburg");
  developSupp.emplace_back(L"Malmö OK");
  supp.emplace_back(L"Söderhamns OK");
  supp.emplace_back(L"Järla Orientering");
  supp.emplace_back(L"Enebybergs IF");
  supp.emplace_back(L"IK Vikings OK");
  supp.emplace_back(L"Naturfreunde Wien Orienteering");
  supp.emplace_back(L"HEYRIES / ACA Aix en Provence");
  supp.emplace_back(L"Allerød OK");

  reverse(supp.begin(), supp.end());
}
