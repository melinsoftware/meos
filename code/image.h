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

#pragma once

#include <map>
#include <vector>

class Image {
public:
  enum class ImageMethod {
    Default,
    MonoAlpha
  };

private:
 
  static HBITMAP read_png_file(const wstring &filename, int &width, int &height, ImageMethod method);
  static HBITMAP read_png_resource(LPCTSTR lpName, LPCTSTR lpType, int &width, int &height, ImageMethod method);
  static HBITMAP read_png(vector<uint8_t> &data, int &width, int &height, ImageMethod method);



  struct Bmp {
    HBITMAP image;
    int width;
    int height;
  };

  map<int, Bmp> images;
public:

  HBITMAP loadImage(int resource, ImageMethod method);
  
  static vector<uint8_t> loadResourceToMemory(LPCTSTR lpName, LPCTSTR lpType);

  int getWidth(int resource);
  int getHeight(int resource);
  void drawImage(int resource, ImageMethod method, HDC hDC, int x, int y, int width, int height);

  Image();
  ~Image();
};

