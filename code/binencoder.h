#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include <cstdint>

class Encoder92 {
  int8_t table[92];
  int8_t reverse_table[128];
  bool used[128];
public:
  Encoder92();

  void encode92(const uint8_t datain[13], uint8_t dataout[16]);
  void decode92(const uint8_t datain[16], uint8_t dataout[13]);

  void encode92(const vector<uint8_t>& bytesIn, string& encodedString);
  void decode92(const string& encodedString, vector<uint8_t>& bytesOut);
};