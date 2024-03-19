/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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

#pragma once

#include <map>
#include <vector>

class Image {
public:
  enum class ImageMethod {
    Default,
    MonoAlpha,
    WhiteTransparent
  };

private:
 
  static uint64_t computeHash(const vector<uint8_t>& data);

  static void read_file(const wstring& filename, vector<uint8_t>& data);

  static HBITMAP read_png_file(const wstring &filename, int &width, int &height, uint64_t &hash, ImageMethod method);
  static HBITMAP read_png_resource(LPCTSTR lpName, LPCTSTR lpType, int &width, int &height, ImageMethod method);
  static HBITMAP read_png(vector<uint8_t> &&data, int &width, int &height, ImageMethod method);

  struct Bmp {
    HBITMAP image = nullptr;
    int width = -1;
    int height = -1;
    wstring fileName;
    vector<uint8_t> rawData;
    ~Bmp();
    void destroy();
  };

  map<uint64_t, Bmp> images;
public:

  HBITMAP loadImage(uint64_t resource, ImageMethod method);
  
  static vector<uint8_t> loadResourceToMemory(LPCTSTR lpName, LPCTSTR lpType);

  int getWidth(uint64_t resource);
  int getHeight(uint64_t resource);
  void drawImage(uint64_t resource, ImageMethod method, HDC hDC, int x, int y, int width, int height);

  uint64_t loadFromFile(const wstring& path, ImageMethod method);
  uint64_t loadFromMemory(const wstring& fileName, const vector<uint8_t> &bytes, ImageMethod method);
  void provideFromMemory(uint64_t id, const wstring& fileName, const vector<uint8_t>& bytes);
  void addImage(uint64_t id, const wstring& fileName);

  void clearLoaded();

  void enumerateImages(vector<pair<wstring, size_t>>& img) const;
  uint64_t getIdFromEnumeration(int enumerationIx) const;
  int getEnumerationIxFromId(uint64_t imgId) const;

  bool hasImage(uint64_t imgId) const;
  void reloadImage(uint64_t imgId, ImageMethod method);

  const wstring& getFileName(uint64_t imgId) const;
  const vector<uint8_t> &getRawData(uint64_t imgId) const;
  Image();
  ~Image();
};

