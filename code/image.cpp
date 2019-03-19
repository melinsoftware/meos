/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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
#include "image.h"
#include "png/png.h"
#include <vector>
#include <algorithm>
#include <Wincodec.h>
#include <fstream>
#include "meosexception.h"

FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
  return _iob;
}

namespace {

  struct PngData {
    vector<uint8_t> memory;
    size_t ptr;
    PngData() : ptr(0) {}

    size_t read(uint8_t *dst, size_t count);
  };

  void readDataFromInputStream(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    png_voidp io_ptr = png_get_io_ptr(png_ptr);
    if (io_ptr == NULL)
      return;   // add custom error handling here

    PngData& inputStream = *(PngData*)io_ptr;
    const size_t bytesRead = inputStream.read((byte*)outBytes, (size_t)byteCountToRead);

    if ((png_size_t)bytesRead != byteCountToRead)
      return;   // add custom error handling here
  }  
}

size_t PngData::read(uint8_t *dst, size_t count) {
  count = min(size_t(memory.size() - ptr), count);
  memcpy(dst, &memory[ptr], count);
  ptr += count;
  return count;
}

// Creates a stream object initialized with the data from an executable resource.
vector<uint8_t> Image::loadResourceToMemory(LPCTSTR lpName, LPCTSTR lpType)  {
  vector<uint8_t> result;
  // find the resource
  HRSRC hrsrc = FindResource(NULL, lpName, lpType);
  if (hrsrc == NULL)
    return result;

  // load the resource
  DWORD dwResourceSize = SizeofResource(NULL, hrsrc);
  HGLOBAL hglbImage = LoadResource(NULL, hrsrc);
  if (hglbImage == NULL)
    return result;

  // lock the resource, getting a pointer to its data
  LPVOID pvSourceResourceData = LockResource(hglbImage);

  result.resize(dwResourceSize);
  memcpy(&result[0], pvSourceResourceData, dwResourceSize);

  return result;
}

HBITMAP Image::read_png(vector<uint8_t> &inData, int &width, int &height, ImageMethod method) {
  PngData inputStream;
  inputStream.memory.swap(inData);
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) 
    return nullptr;

  png_infop info = png_create_info_struct(png);
  if (!info) 
    return nullptr;

  png_set_read_fn(png, &inputStream, readDataFromInputStream);

  png_read_info(png, info);

  width = png_get_image_width(png, info);
  height = png_get_image_height(png, info);
  int color_type = png_get_color_type(png, info);
  int bit_depth = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if (color_type == PNG_COLOR_TYPE_RGB ||
    color_type == PNG_COLOR_TYPE_GRAY ||
    color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  int rowb = png_get_rowbytes(png, info);
  vector<vector<png_byte>> data(height, vector<png_byte>(rowb, 0));
  vector<png_bytep> row_pointers_vec(height);
  for (int y = 0; y < height; y++) {
    row_pointers_vec[y] = &data[y][0];
  }
  png_bytepp row_pointers = &row_pointers_vec[0];
  
  png_read_image(png, row_pointers);

  // initialize return value
  HBITMAP hbmp = NULL;

  // prepare structure giving bitmap information (negative height indicates a top-down DIB)
  BITMAPINFO bminfo;
  ZeroMemory(&bminfo, sizeof(bminfo));
  bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bminfo.bmiHeader.biWidth = width;
  bminfo.bmiHeader.biHeight = ((LONG)-height);
  bminfo.bmiHeader.biPlanes = 1;
  bminfo.bmiHeader.biBitCount = 32;
  bminfo.bmiHeader.biCompression = BI_RGB;

  // create a DIB section that can hold the image
  void * pvImageBits = NULL;
  HDC hdcScreen = GetDC(NULL);
  hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
  ReleaseDC(NULL, hdcScreen);

  // extract the image into the HBITMAP
  const size_t cbStride = width * 4;
  const size_t cbImage = cbStride * height;
  byte *dst = static_cast<byte *>(pvImageBits);
  for (int y = 0; y < height; y++) {
    byte *row = dst + cbStride * y;
    byte *src = row_pointers_vec[y];

    if (method == ImageMethod::MonoAlpha) {
      for (size_t x = 0; x < cbStride; x += 4) {
        row[x + 2] = 0;// src[x + 0]; // Red
        row[x + 1] = 0;// src[x + 1]; // Green 
        row[x + 0] = 16;// src[x + 2]; // Blue
        row[x + 3] = 255 - src[x + 0];// ((x/100)%8)*31+1;// 255 - src[x + 0]; // Alpha

        if (row[x + 3] == 0) {
          row[x + 1] = 0;
          row[x + 2] = 0;
          row[x + 0] = 0;
        }
      }
    }
    else  if (method == ImageMethod::Default) {
      for (size_t x = 0; x < cbStride; x += 4) {
        row[x + 2] = src[x + 0]; // Red
        row[x + 1] = src[x + 1]; // Green 
        row[x + 0] = src[x + 2]; // Blue
        row[x + 3] = src[x + 3];
      }
    }
  }
  return hbmp;
}

HBITMAP Image::read_png_file(const wstring &filename, int &width, int &height, ImageMethod method) {
  width = 0;
  height = 0;
  PngData inputStream;
  inputStream.memory;

  ifstream fin;
  fin.open(filename, ios::binary);
  fin.seekg(0, ios::end);
  int p2 = (int)fin.tellg();
  fin.seekg(0, ios::beg);
  inputStream.memory.resize(p2);
  fin.read((char *)&inputStream.memory[0], inputStream.memory.size());
  fin.close();
  return read_png(inputStream.memory, width, height, method);
}

HBITMAP Image::read_png_resource(LPCTSTR lpName, LPCTSTR lpType, int &width, int &height, ImageMethod method) {
  width = 0;
  height = 0;
  PngData inputStream;
  inputStream.memory = loadResourceToMemory(lpName, lpType);
  if (inputStream.memory.empty())
    return nullptr;
  return read_png(inputStream.memory, width, height, method);
}

Image::Image()
{
}

Image::~Image()
{
}

// Loads the PNG containing the splash image into a HBITMAP.
HBITMAP Image::loadImage(int resource, ImageMethod method) {
  if (images.count(resource))
    return images[resource].image;

  int width, height;
  HBITMAP hbmp = read_png_resource(MAKEINTRESOURCE(resource), _T("PNG"), width, height, method);
  if (hbmp != 0) {
    images[resource].image = hbmp;
    images[resource].width = width;
    images[resource].height = height;
  }
  return hbmp;
}

int Image::getWidth(int resource) {
  loadImage(resource, ImageMethod::Default);
  return images[resource].width;
}

int Image::getHeight(int resource) {
  loadImage(resource, ImageMethod::Default);
  return images[resource].height;
}

void Image::drawImage(int resource, ImageMethod method, HDC hDC, int x, int y, int width, int height) {
  HBITMAP bmp = loadImage(resource, method);
  HDC memdc = CreateCompatibleDC(hDC);
  SelectObject(memdc, bmp);
  
  BLENDFUNCTION bf;
  bf.BlendOp = AC_SRC_OVER;
  bf.BlendFlags = 0;
  bf.SourceConstantAlpha =0xFF;  
  bf.AlphaFormat = AC_SRC_ALPHA;    
  AlphaBlend(hDC, x, y, width, height, memdc, 0, 0, width, height, bf);

  DeleteDC(memdc);
}
