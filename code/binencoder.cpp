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

#include "stdafx.h"
#include "meos_util.h"
#include <vector>
#include <cassert>
#include "binencoder.h"

Encoder92::Encoder92() {
  int d = 32;
  fill(reverse_table, reverse_table + 128, 0);
  fill(used, used + 128, false);
  for (int j = 0; j < 92; j++) {
    table[j] = d++;
    while (d == '"' || d == '<' || d == '&' || d=='>')
      d++;
  }
  table[91] = '\t';
  swap(table[0], table[1]);

  for (int j = 0; j < 92; j++) {
    reverse_table[table[j]] = j;
    used[table[j]] = true;
  }
}

void Encoder92::encode92(const uint8_t datain[13], uint8_t dataout[16]) {
  uint64_t w64bitA = *(uint64_t*)&datain[0];
  uint32_t w32bitB = *(uint32_t*)&datain[8];
  uint32_t w8bitC = datain[12];

  // Consume 60 bits of data from w64bitA (4 bits will remain)
  for (int i = 0; i < 10; i++) {
    dataout[i] = w64bitA & 0b00111111;
    w64bitA = w64bitA >> 6;
  }

  // Consume 30 bits of data from w32bitB (2 bits will remain)
  for (int i = 10; i < 15; i++) {
    dataout[i] = w32bitB & 0b00111111;
    w32bitB = w32bitB >> 6;
  }

  // Consume remaining 2 + 4 bits
  dataout[15] = int8_t(w32bitB | (w64bitA << 2));

  for (int i = 0; i < 16; i+=2) {
    int extraData = (w8bitC & 0x1);
    w8bitC = w8bitC >> 1;
    int combined = (extraData << 12) | dataout[i] | (dataout[i+1] << 6); // 13 bits 0 -- 8192
    dataout[i] = combined % 92;
    dataout[i+1] = combined / 92;
  }
}

void Encoder92::decode92(const uint8_t datain[16], uint8_t dataout[13]) {
  uint8_t datain_64[16];
  uint32_t w8bitC = 0;

  for (int i = 0; i < 16; i += 2) {
    int combined = datain[i] + datain[i + 1] * 92;
    datain_64[i] = combined & 0b00111111;
    combined = combined >> 6;
    datain_64[i+1] = combined & 0b00111111;
    combined = combined >> 6;
    w8bitC |= (combined << (i/2));
  }

  uint64_t w64bitA = 0;
  uint32_t w32bitB = 0;
  
  // Reconstruct 60 bits of data to w64bitA
  for (int i = 9; i >= 0; i--) {
    w64bitA <<= 6;
    w64bitA |= datain_64[i];
  }

  // Reconstruct 30 bits of data to w32bitB
  for (int i = 14; i >= 10; i--) {
    w32bitB <<= 6;
    w32bitB |= datain_64[i];    
  }

  // Add remaining bits
  w32bitB = w32bitB | ((datain_64[15] & 0b11) << 30);
  uint64_t highBits = (datain_64[15] >> 2) & 0b1111;
  w64bitA = w64bitA | (highBits << 60ll);

  *((uint64_t*)&dataout[0]) = w64bitA;
  *((uint32_t*)&dataout[8]) = w32bitB;
  dataout[12] = int8_t(w8bitC);
}


void Encoder92::encode92(const vector<uint8_t>& bytesIn, string& encodedString) {
  int m13 = bytesIn.size()%13;
  int extra = m13 > 0 ? 13 - m13 : 0;
  int blocks = (bytesIn.size() + extra) / 13;

  encodedString = itos(bytesIn.size()) + ":";
  int outLen = encodedString.length();
  encodedString.resize(encodedString.size() + blocks * 16, ' ');
  const uint8_t* inPtr = bytesIn.data();
  //uint8_t* outPtr = encodedString.data() + outLen;
  uint8_t datain[13];
  fill(datain, datain + 13, 0);
  uint8_t dataout[16];
  int fullBlocks = blocks;
  if (extra > 0)
    fullBlocks--;

  for (int i = 0; i < fullBlocks; i++) {
    //for (int j = 0; j < 13; j++)
    encode92(inPtr, dataout);
    inPtr += 13;
    for (int j = 0; j < 16; j++)
      encodedString[outLen++] = table[dataout[j]];
  }

  if (extra > 0) {
    for (int j = 0; j < m13; j++) {
      datain[j] = inPtr[j];
    }
    encode92(datain, dataout);
    for (int j = 0; j < 16; j++)
      encodedString[outLen++] = table[dataout[j]];
  }
}

void Encoder92::decode92(const string& encodedString, vector<uint8_t>& bytesOut) {
  bytesOut.clear();
  if (encodedString.empty())
    return;

  unsigned size = atoi(encodedString.c_str());
  string start = itos(size);
  int len = start.length();
  if (encodedString.size() < len || encodedString[len] != ':' || size<0)
    throw std::exception("Invalid data");

  int m13 = size % 13;
  int extra = m13 > 0 ? 13 - m13 : 0;
  int blocks = (size + extra) / 13;

  int inDataSize = blocks * 16;
  if (encodedString.size() < len + 1 + inDataSize)
    throw std::exception("Invalid data");

  bytesOut.resize(size);
  auto outPtr = bytesOut.data();

  uint8_t datain[16];
  uint8_t dataout[13];
  int fullBlocks = blocks;
  if (extra > 0)
    fullBlocks--;

  const char *inPtr = encodedString.c_str() + len + 1;

  for (int i = 0; i < fullBlocks; i++) {
    for (int j = 0; j < 16; j++) {
      int v = inPtr[j];
      if (v < 0 || v > 127 || !used[v])
        throw std::exception("Invalid data");
      datain[j] = reverse_table[inPtr[j]];
    }
    decode92(datain, outPtr);
    inPtr += 16;
    outPtr += 13;
  }

  if (extra > 0) {
    for (int j = 0; j < 16; j++) {
      int v = inPtr[j];
      if (v < 0 || v > 127 || !used[v])
        throw std::exception("Invalid data");
      datain[j] = reverse_table[inPtr[j]];
    }
    decode92(datain, dataout);
    for (int j = 0; j < m13; j++)
      outPtr[j] = dataout[j];
  }
}

