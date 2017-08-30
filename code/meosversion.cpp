/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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
//V35: abc
int getMeosBuild() {
  string revision("$Rev: 611 $");
  return 174 + atoi(revision.substr(5, string::npos).c_str());
}

//ABCDEFGHIJKILMNOPQRSTUVXYZabcdefghijklmnopqrstuvxyz
//V2: abcdefgh
//V3: abcdefghijklmnopqrstuvxyz
//V31: abcde
//V32: abcdefgh
//V33: abcdefghij
//V34: abcdfg
wstring getMeosDate() {
  wstring date(L"$Date: 2017-08-28 20:32:01 +0200 (mÃ¥, 28 aug 2017) $");
  return date.substr(7,10);
}

wstring getBuildType() {
  return L"Snapshot"; // No parantheses (...)
}

wstring getMajorVersion() {
  return L"3.5";
}

wstring getMeosFullVersion() {
  wchar_t bf[256];
  wstring maj = getMajorVersion();
  if (getBuildType().empty())
    swprintf_s(bf, L"Version X#%s.%d, %s", maj.c_str(), getMeosBuild(), getMeosDate().c_str());
  else
    swprintf_s(bf, L"Version X#%s.%d, %s %s", maj.c_str(), getMeosBuild(), getBuildType().c_str(), getMeosDate().c_str());
  return bf;
}

wstring getMeosCompectVersion() {
  if (getBuildType().empty())
    return getMajorVersion() + L"." + itow(getMeosBuild());
  else
    return getMajorVersion() + L"." + itow(getMeosBuild()) + L" (" + getBuildType() + L")";
}

void getSupporters(vector<string> &supp)
{
  supp.push_back("Centrum OK");
  supp.push_back("Ove Persson, Piteå IF");
  supp.push_back("OK Rodhen");
  supp.push_back("Täby Extreme Challenge");
  supp.push_back("Thomas Engberg, VK Uvarna");
  supp.push_back("Eilert Edin, Sidensjö IK");
  supp.push_back("Göran Nordh, Trollhättans SK");
  supp.push_back("Roger Gustavsson, OK Tisaren");
  supp.push_back("Sundsvalls OK");
  supp.push_back("OK Gipens OL-skytte");
  supp.push_back("Helsingborgs SOK");
  supp.push_back("OK Gipens OL-skytte");
  supp.push_back("Rune Thurén, Vallentuna-Össeby OL");
  supp.push_back("Roland Persson, Kalmar OK");
  supp.push_back("Robert Jessen, Främmestads IK");
  supp.push_back("Anders Platt, Järla Orientering");
  supp.push_back("Almby IK, Örebro");
  supp.push_back("Peter Rydesäter, Rehns BK");
  supp.push_back("IK Hakarpspojkarna");
  supp.push_back("Rydboholms SK");
  supp.push_back("IFK Kiruna");
  supp.push_back("Peter Andersson, Söders SOL");
  supp.push_back("Björkfors GoIF");
  supp.push_back("OK Ziemelkurzeme");
  supp.push_back("Big Foot Orienteers");
  supp.push_back("FIF Hillerød");
  supp.push_back("Anne Udd");
  supp.push_back("OK Orinto");
  supp.push_back("SOK Träff");
  supp.push_back("Gamleby OK");
  supp.push_back("Vänersborgs SK");
  supp.push_back("Henrik Ortman, Västerås SOK");
  supp.push_back("Leif Olofsson, Sjuntorp");
  supp.push_back("Vallentuna/Össeby OL");
  supp.push_back("Oskarström OK");
  supp.push_back("Skogslöparna");
  supp.push_back("OK Milan");
  supp.push_back("GoIF Tjalve");
  supp.push_back("OK Skärmen");
  supp.push_back("Østkredsen");
  supp.push_back("OK Roskilde");
  supp.push_back("Holbæk Orienteringsklub");
  supp.push_back("Bodens BK");
  supp.push_back("OK Tyr, Karlstad");
  supp.push_back("Göteborg-Majorna OK");
  supp.push_back("OK Järnbärarna, Kopparberg");
  supp.push_back("FK Åsen");
  supp.push_back("Ballerup OK");
  supp.push_back("Olivier Benevello, Valbonne SAO");
  supp.push_back("Tommy Wåhlin, OK Enen");
  supp.push_back("Hjobygdens OK");
  supp.push_back("Tisvilde Hegn OK");
  supp.push_back("Lindebygdens OK");
  supp.push_back("OK Flundrehof");
  supp.push_back("Vittjärvs IK");
  supp.push_back("Annebergs GIF");
  supp.push_back("Lars-Eric Gahlin, Östersunds OK");
  supp.push_back("Sundsvalls OK:s Veteraner");
  supp.push_back("OK Skogshjortarna");
  supp.push_back("Kinnaströms SK");
  supp.push_back("OK Pan Århus");
  supp.push_back("Jan Ernberg, Täby OK");
  supp.push_back("Stjärnorps SK");
  supp.push_back("Mölndal Outdoor IF");
  supp.push_back("Roland Elg, Fjärås AIK");
  supp.push_back("Tenhults SOK");
  supp.push_back("Järfälla OK");
  supp.push_back("Lars Jonasson");
  supp.push_back("Anders Larsson, OK Nackhe");
  supp.push_back("Hans Wilhelmsson");
  supp.push_back("Patrice Lavallee, Noyon Course d'Orientation");
  supp.push_back("IFK Linköpings OS");
  supp.push_back("Lars Ove Karlsson, Västerås SOK");
  supp.push_back("OK Djerf");
  supp.push_back("OK Vivill");
  supp.push_back("IFK Mora OK");
  supp.push_back("Sonny Andersson, Huskvarna");
  supp.push_back("Hässleholms OK Skolorientering");
  supp.push_back("IBM-klubben Orientering");
  supp.push_back("OK Øst, Birkerød");
  supp.push_back("OK Klemmingen");
  supp.push_back("Hans Johansson");
  supp.push_back("KOB Kysak");  
  supp.push_back("Per Ivarsson, Trollhättans SOK");
  supp.push_back("Sergio Yañez, ABC TRAIL");
  supp.push_back("Western Race Services");
  supp.push_back("IK Gandvik, Skara");
  supp.push_back("IK Stern");
  supp.push_back("OK Roslagen");
  supp.push_back("TSV Malente");
  supp.push_back("Emmaboda Verda OK");
  supp.push_back("KOB ATU Košice");
  supp.push_back("Gävle OK");
  supp.push_back("Kenneth Gattmalm, Jönköpings OK");
  supp.push_back("Søllerød OK");
  supp.push_back("O-travel");
  supp.push_back("Bengt Bengtsson");
  supp.push_back("OK Landehof");
  supp.push_back("OK Orinto");
  supp.push_back("Bredaryds SOK");
  supp.push_back("Thore Nilsson, Uddevalla OK");
  supp.push_back("Timrå SOK");
  supp.push_back("Åke Larsson, OK Hedströmmen");
  supp.push_back("Avesta OK");
}
