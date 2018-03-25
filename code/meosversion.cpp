/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2018 Melin Software HB

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
//V35: abcde
int getMeosBuild() {
  string revision("$Rev: 669 $");
  return 174 + atoi(revision.substr(5, string::npos).c_str());
}

//ABCDEFGHIJKILMNOPQRSTUVXYZabcdefghijklmnopqrstuvxyz
//V2: abcdefgh
//V3: abcdefghijklmnopqrstuvxyz
//V31: abcde
//V32: abcdefgh
//V33: abcdefghij
//V34: abcdfge
wstring getMeosDate() {
  wstring date(L"$Date: 2018-03-25 07:29:55 +0200 (sö, 25 mar 2018) $");
  return date.substr(7,10);
}

wstring getBuildType() {
  return L"RC2"; // No parantheses (...)
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

void getSupporters(vector<wstring> &supp)
{
  supp.push_back(L"Centrum OK");
  supp.push_back(L"Ove Persson, Piteå IF");
  supp.push_back(L"OK Rodhen");
  supp.push_back(L"Täby Extreme Challenge");
  supp.push_back(L"Thomas Engberg, VK Uvarna");
  supp.push_back(L"Eilert Edin, Sidensjö IK");
  supp.push_back(L"Göran Nordh, Trollhättans SK");
  supp.push_back(L"Roger Gustavsson, OK Tisaren");
  supp.push_back(L"Sundsvalls OK");
  supp.push_back(L"OK Gipens OL-skytte");
  supp.push_back(L"Helsingborgs SOK");
  supp.push_back(L"OK Gipens OL-skytte");
  supp.push_back(L"Rune Thurén, Vallentuna-Össeby OL");
  supp.push_back(L"Roland Persson, Kalmar OK");
  supp.push_back(L"Robert Jessen, Främmestads IK");
  supp.push_back(L"Anders Platt, Järla Orientering");
  supp.push_back(L"Almby IK, Örebro");
  supp.push_back(L"Peter Rydesäter, Rehns BK");
  supp.push_back(L"IK Hakarpspojkarna");
  supp.push_back(L"Rydboholms SK");
  supp.push_back(L"IFK Kiruna");
  supp.push_back(L"Peter Andersson, Söders SOL");
  supp.push_back(L"Björkfors GoIF");
  supp.push_back(L"OK Ziemelkurzeme");
  supp.push_back(L"Big Foot Orienteers");
  supp.push_back(L"FIF Hillerød");
  supp.push_back(L"Anne Udd");
  supp.push_back(L"OK Orinto");
  supp.push_back(L"SOK Träff");
  supp.push_back(L"Gamleby OK");
  supp.push_back(L"Vänersborgs SK");
  supp.push_back(L"Henrik Ortman, Västerås SOK");
  supp.push_back(L"Leif Olofsson, Sjuntorp");
  supp.push_back(L"Vallentuna/Össeby OL");
  supp.push_back(L"Oskarström OK");
  supp.push_back(L"Skogslöparna");
  supp.push_back(L"OK Milan");
  supp.push_back(L"Tjalve IF");
  supp.push_back(L"OK Skärmen");
  supp.push_back(L"Østkredsen");
  supp.push_back(L"OK Roskilde");
  supp.push_back(L"Holbæk Orienteringsklub");
  supp.push_back(L"Bodens BK");
  supp.push_back(L"OK Tyr, Karlstad");
  supp.push_back(L"Göteborg-Majorna OK");
  supp.push_back(L"OK Järnbärarna, Kopparberg");
  supp.push_back(L"FK Åsen");
  supp.push_back(L"Ballerup OK");
  supp.push_back(L"Olivier Benevello, Valbonne SAO");
  supp.push_back(L"Tommy Wåhlin, OK Enen");
  supp.push_back(L"Hjobygdens OK");
  supp.push_back(L"Tisvilde Hegn OK");
  supp.push_back(L"Lindebygdens OK");
  supp.push_back(L"OK Flundrehof");
  supp.push_back(L"Vittjärvs IK");
  supp.push_back(L"Annebergs GIF");
  supp.push_back(L"Lars-Eric Gahlin, Östersunds OK");
  supp.push_back(L"Sundsvalls OK:s Veteraner");
  supp.push_back(L"OK Skogshjortarna");
  supp.push_back(L"Kinnaströms SK");
  supp.push_back(L"OK Pan Århus");
  supp.push_back(L"Jan Ernberg, Täby OK");
  supp.push_back(L"Stjärnorps SK");
  supp.push_back(L"Mölndal Outdoor IF");
  supp.push_back(L"Roland Elg, Fjärås AIK");
  supp.push_back(L"Tenhults SOK");
  supp.push_back(L"Järfälla OK");
  supp.push_back(L"Lars Jonasson");
  supp.push_back(L"Anders Larsson, OK Nackhe");
  supp.push_back(L"Hans Wilhelmsson");
  supp.push_back(L"Patrice Lavallee, Noyon Course d'Orientation");
  supp.push_back(L"IFK Linköpings OS");
  supp.push_back(L"Lars Ove Karlsson, Västerås SOK");
  supp.push_back(L"OK Djerf");
  supp.push_back(L"OK Vivill");
  supp.push_back(L"IFK Mora OK");
  supp.push_back(L"Sonny Andersson, Huskvarna");
  supp.push_back(L"Hässleholms OK Skolorientering");
  supp.push_back(L"IBM-klubben Orientering");
  supp.push_back(L"OK Øst, Birkerød");
  supp.push_back(L"OK Klemmingen");
  supp.push_back(L"Hans Johansson");
  supp.push_back(L"KOB Kysak");  
  supp.push_back(L"Per Ivarsson, Trollhättans SOK");
  supp.push_back(L"Sergio Yañez, ABC TRAIL");
  supp.push_back(L"Western Race Services");
  supp.push_back(L"IK Gandvik, Skara");
  supp.push_back(L"IK Stern");
  supp.push_back(L"OK Roslagen");
  supp.push_back(L"TSV Malente");
  supp.push_back(L"Emmaboda Verda OK");
  supp.push_back(L"KOB ATU Košice");
  supp.push_back(L"Gävle OK");
  supp.push_back(L"Kenneth Gattmalm, Jönköpings OK");
  supp.push_back(L"Søllerød OK");
  supp.push_back(L"O-travel");
  supp.push_back(L"Bengt Bengtsson");
  supp.push_back(L"OK Landehof");
  supp.push_back(L"OK Orinto");
  supp.push_back(L"Bredaryds SOK");
  supp.push_back(L"Thore Nilsson, Uddevalla OK");
  supp.push_back(L"Timrå SOK");
  supp.push_back(L"Åke Larsson, OK Hedströmmen");
  supp.push_back(L"Avesta OK");
  supp.push_back(L"Motionsorientering Göteborg");
  supp.push_back(L"OK Måsen");
  supp.push_back(L"IF Thor");
  supp.push_back(L"SOS Jindřichův Hradec");
  supp.push_back(L"Mats Holmberg, OK Gränsen");
  supp.push_back(L"Christoffer Ohlsson, Uddevalla OK");
  supp.push_back(L"O-Ringen AB");
  supp.push_back(L"Hans Carlstedt, Sävedalens AIK");
  supp.push_back(L"Attunda OK");
  supp.push_back(L"Siguldas Takas, Latvia");
  supp.push_back(L"Eric Teutsch, Ottawa Orienteering Club, Canada");
  supp.push_back(L"Silkeborg OK, Denmark");
}
