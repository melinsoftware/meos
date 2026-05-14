/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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

tuple<HBITMAP, uint8_t*, bool> Image::read_png(vector<uint8_t> &&inData, int &width, int &height, ImageMethod method) {
  PngData inputStream;
  inputStream.memory = std::move(inData);
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) 
    return tuple(nullptr, nullptr, false);

  png_infop info = png_create_info_struct(png);
  if (!info) 
    return tuple(nullptr, nullptr, false);

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
  bool hasAlpha = false;
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
        if (row[x + 3] < 255u)
          hasAlpha = true;
      }
    }
    else if (method == ImageMethod::WhiteTransparent) {
      for (size_t x = 0; x < cbStride; x += 4) {
        row[x + 2] = src[x + 0]; // Red
        row[x + 1] = src[x + 1]; // Green 
        row[x + 0] = src[x + 2]; // Blue
        row[x + 3] = src[x + 3];
        if (src[x + 0] == 0xFF && src[x + 1] == 0xFF && src[x + 2] == 0xFF) {
          row[x + 3] = 0;
          row[x + 2] = 0;
          row[x + 1] = 0;
          row[x + 0] = 0;
        }
      }
    }
  }
  return tuple(hbmp, dst, hasAlpha);
}

uint64_t Image::computeHash(const vector<uint8_t>& data) {
  uint64_t h = data.size();
  size_t siz4 = data.size() / 4;
  const uint32_t* ptr = (const uint32_t *)data.data();
  size_t lim = siz4 * 4;
  for (size_t e = siz4 * 4; e < data.size(); e++)
    h = h * 256 + data[e];

  for (size_t e = 0; e < siz4; e++) {
    h = 997 * h + ptr[e];
  }

  return h;
}

void Image::read_file(const wstring& filename, vector<uint8_t>& data) {
  std::ifstream fin;
  fin.open(filename, std::ios::binary);
  fin.seekg(0, std::ios::end);
  int p2 = (int)fin.tellg();
  fin.seekg(0, std::ios::beg);
  data.resize(p2);
  fin.read((char*)data.data(), data.size());
  fin.close();
}

tuple<HBITMAP, uint8_t*, bool> Image::read_png_file(const wstring &filename, int &width, int &height, uint64_t &hash, ImageMethod method) {
  width = 0;
  height = 0;
  PngData inputStream;
  read_file(filename, inputStream.memory);
  hash = computeHash(inputStream.memory);
  return read_png(std::move(inputStream.memory), width, height, method);
}

tuple<HBITMAP, uint8_t*, bool> Image::read_png_resource(LPCTSTR lpName, LPCTSTR lpType, int &width, int &height, ImageMethod method) {
  width = 0;
  height = 0;
  PngData inputStream;
  inputStream.memory = loadResourceToMemory(lpName, lpType);
  if (inputStream.memory.empty())
    return tuple(nullptr, nullptr, false);
  return read_png(std::move(inputStream.memory), width, height, method);
}

Image::Image() = default;

Image::~Image() = default;

// Loads image (or use alreadly loaded version
HBITMAP Image::loadImage(uint64_t resource, ImageMethod method) {
  if (images.count(resource)) {
    if (!images[resource].image)
      reloadImage(resource, method);

    return images[resource].image;
  }
  int width, height;
  auto [hImage, pixels, alpha] = read_png_resource(MAKEINTRESOURCE(resource), _T("PNG"), width, height, method);
  if (hImage != nullptr) {
    images[resource].image = hImage;
    images[resource].pixels = pixels;
    images[resource].hasTrueAlpha = alpha;
    images[resource].width = width;
    images[resource].height = height;
  }
  return hImage;
}

int Image::getWidth(uint64_t resource) {
  if (!resource)
    return -1;
  loadImage(resource, ImageMethod::Default);
  return images[resource].width;
}

int Image::getHeight(uint64_t resource) {
  if (!resource)
    return -1;
  loadImage(resource, ImageMethod::Default);
  return images[resource].height;
}

HBITMAP Image::getVersion(uint64_t resource, int sx, int sy) {
  auto res = images.find(resource);
  if (res == images.end())
    return nullptr;
  int wSrc = sx;
  int hSrc = sy;

  if (sx < 0)
    wSrc = (res->second.width * sy) / res->second.height;
 
  if (sy < 0)
    hSrc = (res->second.height * sx) / res->second.width;

  return res->second.getVersion(wSrc, hSrc);
}

void Image::drawImage(uint64_t resource, ImageMethod method, HDC hDC,
                      int x, int y, int width, int height,
                      int srcOffX, int srcOffY, int srcWidth, int srcHeight) {
  loadImage(resource, method);
  auto res = images.find(resource);
  if (res == images.end())
    return;

  int wSrc = width;
  int hSrc = height;

  bool cmpW = false;
  bool cmpH = false;

  if (srcWidth <= 0 || (srcWidth == res->second.width && srcOffX == 0))
    srcWidth = width;
  else {
    wSrc = (res->second.width * width) / srcWidth;
    cmpW = true;
  }

  if (srcHeight <= 0 || (srcHeight == res->second.height && srcOffY == 0))
    srcHeight = height;
  else {
    hSrc =  (res->second.height * height) / srcHeight;
    cmpH = true;
  }

  HBITMAP bmp = res->second.getVersion(wSrc, hSrc);

  if (cmpW) {
    srcOffX = (wSrc * srcOffX) / res->second.width;
    srcWidth = (wSrc * srcWidth) / res->second.width;
  }

  if (cmpH) {
    srcOffY = (hSrc * srcOffY) / res->second.height;
    srcHeight = (hSrc * srcHeight) / res->second.height;
  }

  HDC memdc = CreateCompatibleDC(hDC);
  SelectObject(memdc, bmp);
  
  BLENDFUNCTION bf;
  bf.BlendOp = AC_SRC_OVER;
  bf.BlendFlags = 0;
  bf.SourceConstantAlpha = 0xFF;  
  bf.AlphaFormat = AC_SRC_ALPHA;
 
  if (srcOffX > 0) {
    if (srcOffX + width > wSrc)
      width = max<int>(0, wSrc - srcOffX);
  }
  else if (srcOffX < 0) {
    x -= srcOffX;
    width += srcOffX;
    srcOffX = 0;
  }
  if (srcOffX + width > wSrc) {
    width = wSrc - srcOffX;
  }

  if (srcOffY > 0) {
    if (srcOffY + height > hSrc)
      height = max<int>(0, hSrc - srcOffY);
  }
  else if (srcOffY < 0) {
    y -= srcOffY;
    height += srcOffY;
    srcOffY = 0;
  }
  if (srcOffY + height > hSrc) {
    height = hSrc - srcOffY;
  }

  AlphaBlend(hDC, x, y, width, height, memdc, srcOffX, srcOffY, width, height, bf);

  DeleteDC(memdc);
}

uint64_t Image::loadFromFile(const wstring& path, ImageMethod method) {
  vector<uint8_t> bytes;
  read_file(path, bytes);

  uint64_t hash = computeHash(bytes);

  auto res = images.emplace(hash, Bmp());
  if (res.second) {
    wchar_t drive[20];
    wchar_t dir[MAX_PATH];
    wchar_t name[MAX_PATH];
    wchar_t ext[MAX_PATH];
    _wsplitpath_s(path.c_str(), drive, dir, name, ext);
    Bmp &out = res.first->second;
    out.fileName = wstring(name) + ext;
    out.rawData = bytes;
    auto [hImage, pixels, alpha] = read_png(std::move(bytes), out.width, out.height, method);
    out.image = hImage;
    out.pixels = pixels;
    out.hasTrueAlpha = alpha;
  }
  return hash;
}

uint64_t Image::loadFromMemory(const wstring& fileName, const vector<uint8_t>& bytes, ImageMethod method) {
  uint64_t hash = computeHash(bytes);
  return hash;
}

void Image::provideFromMemory(uint64_t id, const wstring& fileName, const vector<uint8_t>& bytes) {
  uint64_t hash = computeHash(bytes);
  if (id != hash) 
    throw meosException(L"Corrupted image: " + fileName);

  images[id].fileName = fileName;
  images[id].rawData = bytes;
}

void Image::addImage(uint64_t id, const wstring& fileName) {
  images[id].fileName = fileName;
}

void Image::reloadImage(uint64_t imgId, ImageMethod method) {
  auto res = images.find(imgId);
  if (res != images.end() && res->second.rawData.size() > 0) {
    auto copy = res->second.rawData;
    res->second.destroy();
    auto [hImage, pixels, alpha] = read_png(std::move(copy), res->second.width, res->second.height, method);
    res->second.image = hImage;
    res->second.pixels = pixels;
    res->second.hasTrueAlpha = alpha;
    return;
  }
  throw meosException("Unknown image " + itos(imgId));
}

void Image::clearLoaded() {
  for (auto iter = images.begin(); iter != images.end(); ) {
    if (iter->second.fileName.empty())
      ++iter;
    else {
      iter = images.erase(iter);
    }
  }
}

Image::Bmp::~Bmp() {
  destroy();
}

void Image::Bmp::destroy() {
  if (image) {
    pixels = nullptr;
    DeleteObject(image);
    image = nullptr;    
  }
  for (auto [key, hImg] : resamples) {
    DeleteObject(hImg);
  }
  resamples.clear();
}

HBITMAP Image::Bmp::getVersion(int &width, int &height) {
  if (image == nullptr)
    return nullptr;

  if (width == this->width && height == this->height && !hasTrueAlpha)
    return image;

  for (auto &[wh, img] : resamples) {
    double diffx = std::abs(wh.first - width);
    double diffy = std::abs(wh.second - height);

    if ((diffx / width < 0.01 || diffx <= 1) && (diffy / height < 0.01 || diffy <= 1)) {
      width = wh.first;
      height = wh.second;
      return img;
    }
  }


  HBITMAP version = resample(width, height);
  resamples[make_pair(width, height)] = version;

  return version;
}

HBITMAP Image::Bmp::resample(int w, int h) {
  double limit = min<double>(6e9, numeric_limits<size_t>::max() / 2);
  if (w <= 0 || h <= 0 || double(h) * double(w) * 9 * 16 > limit)
    throw std::exception("Error in image handling");

  // initialize return value
  HBITMAP hbmp = NULL;

  // prepare structure giving bitmap information (negative height indicates a top-down DIB)
  BITMAPINFO bminfo;
  ZeroMemory(&bminfo, sizeof(bminfo));
  bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bminfo.bmiHeader.biWidth = w;
  bminfo.bmiHeader.biHeight = ((LONG)-h);
  bminfo.bmiHeader.biPlanes = 1;
  bminfo.bmiHeader.biBitCount = 32;
  bminfo.bmiHeader.biCompression = BI_RGB;

  // create a DIB section that can hold the image
  void* pvImageBits = NULL;
  HDC hdcScreen = GetDC(NULL);
  hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
  ReleaseDC(NULL, hdcScreen);

  // extract the image into the HBITMAP
  const size_t cbStride = w * 4;
  const size_t cbStrideSrc = width * 4;
  const size_t cbImage = cbStride * h;
  byte* dst = static_cast<byte*>(pvImageBits);

  struct fRGBA {
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;

    uint8_t getR() const {
      return uint8_t(min(255, int(r * a / 255.f)));
    }
    uint8_t getG() const {
      return uint8_t(min(255, int(g * a / 255.f)));
    }
    uint8_t getB() const {
      return uint8_t(min(255, int(b * a / 255.f)));
    }
    uint8_t getA() const {
      return uint8_t(min(255, int(a)));
    }

    void add(const fRGBA& nb, float factor) {
      r += nb.r * factor;
      g += nb.g * factor;
      b += nb.b * factor;
      a += nb.a * factor;
    }

    void clear() {
      r = 0;
      g = 0;
      b = 0;
      a = 0;
    }
  };

  class UpSample {
    vector<vector<fRGBA>> data;
    float xScale;
    float yScale;
    int w;
    int h;
  public:

    UpSample(int width, int height, const uint8_t* pixels, int dstX, int dstY) {
      float upFactor = 1;
      if (dstX > width) {
        upFactor = 3.f;
        xScale = float(3 * width - 2) / float(dstX);
        yScale = float(3 * height - 2) / float(dstY);
        w = width * 3;
        h = height * 3;
        data.resize(height * 3);
        for (auto& line : data)
          line.resize(width * 3);
        const size_t cbStride = width * 4;
        constexpr float factor = 0.8f;
        for (int y = 0, ydst = 0; y < height; y++, ydst += 3) {
          const byte* src = pixels + cbStride * y;
          auto& dstRow = data[ydst + 1];
          int last = dstRow.size() - 1;
          for (int x = 0, xdst = 0; x < width; x++, xdst += 3) {
            int xs = x * 4;
            dstRow[xdst + 1].g = src[xs + 1]; // Green 
            dstRow[xdst + 1].b = src[xs + 0]; // Blue
            dstRow[xdst + 1].r = src[xs + 2]; // Red
            dstRow[xdst + 1].a = src[xs + 3]; //Alpha

            dstRow[xdst].add(dstRow[xdst + 1], factor);
            dstRow[xdst + 2].add(dstRow[xdst + 1], factor);
            if (x > 0)
              dstRow[xdst - 1].add(dstRow[xdst + 1], (1 - factor));
            if (x + 1 < width)
              dstRow[xdst + 3].add(dstRow[xdst + 1], (1 - factor));
          }
          dstRow[0].add(dstRow[1], (1 - factor));
          dstRow[last].add(dstRow[last - 1], (1 - factor));


          if (y == 0)
            data[0] = data[1];
          else {
            auto& pRow = data[ydst];
            for (int j = 0; j < pRow.size(); j++)
              pRow[j].add(dstRow[j], factor);

            auto& pRow2 = data[ydst - 1];
            for (int j = 0; j < pRow.size(); j++)
              pRow2[j].add(dstRow[j], (1 - factor));
          }

          if (y + 1 == height)
            data[ydst + 2] = data[ydst + 1];
          else {
            auto& pRow = data[ydst + 2];
            for (int j = 0; j < pRow.size(); j++)
              pRow[j].add(dstRow[j], factor);

            auto& pRow2 = data[ydst + 3];
            for (int j = 0; j < pRow.size(); j++)
              pRow2[j].add(dstRow[j], (1 - factor));
          }
        }
      }
      else {
        xScale = float(width - 2) / float(dstX);
        yScale = float(height - 2) / float(dstY);
        w = width;
        h = height;
        data.resize(height);
        for (auto& line : data)
          line.resize(width);

        const size_t cbStride = width * 4;
        for (int y = 0; y < height; y++) {
          const byte* src = pixels + cbStride * y;
          auto& dstRow = data[y];
          int last = dstRow.size() - 1;
          for (int x = 0; x < width; x++) {
            int xs = x * 4;
            dstRow[x].g = src[xs + 1]; // Green 
            dstRow[x].b = src[xs + 0]; // Blue
            dstRow[x].r = src[xs + 2]; // Red
            dstRow[x].a = src[xs + 3]; //Alpha
          }
        }
      }

      if (dstX < width || dstY < height) {
        float ratX = float(width) / float(dstX);
        float ratY = float(height) / float(dstY);
        int xkern = int(2 * upFactor * ratX) | 1;
        int ykern = int(2 * upFactor * ratY) | 1;
        vector<vector<float>> kernel(ykern);
        for (auto& row : kernel)
          row.resize(xkern);

        int centerx = (xkern - 1) / 2;
        int centery = (ykern - 1) / 2;
        double sum = 0;
        for (int x = 0; x < xkern; x++) {
          float xs = float(x - centerx) / float(centerx);
          for (int y = 0; y < ykern; y++) {
            float ys = float(y - centery) / float(centery);
            float d = sqrt(xs * xs + ys * ys)*0.9;
            if (d < 1.0001) {
              float base = (1.0 - d * d);
              sum += kernel[y][x] = base;
            }
          }
        }
        for (auto& row : kernel)
          for (float& f : row)
            f /= sum;

        vector<vector<fRGBA>> copy = data;

        for (int yy = 0; yy < h; yy++){
          for (int xx = 0; xx < w; xx++) {
            auto& t = data[yy][xx];
            t.clear();

            for (int y = 0; y < ykern; y++) {
              for (int x = 0; x < xkern; x++) {
                float f = kernel[y][x];
                if (f > 0) {
                  int xp = xx + x - centerx;
                  xp = min(max(0, xp), w - 1);

                  int yp = yy + y - centery;
                  yp = min(max(0, yp), h - 1);
                  t.add(copy[yp][xp], f);
                }
              }
            }
          }
        }
      }
    }

    fRGBA getPixel(int x, int y) const {
      float cx = x * xScale + 1;
      float cy = y * yScale + 1;

      int tx = min(max(1, int(round(cx))), w - 2);
      int ty = min(max(1, int(round(cy))), h - 2);

      //return data[ty][tx];

      float fracX = cx - tx;
      float fracY = cy - ty;
      float sx = std::abs(fracX);
      float sy = std::abs(fracY);

      fRGBA ret;
      if (fracX > 0) {
        ret.add(data[ty][tx], 0.5 - sx);
        ret.add(data[ty][tx+1], sx);
      }
      else {
        ret.add(data[ty][tx], 0.5 - sx);
        ret.add(data[ty][tx - 1], sx);
      }
    

      if (fracY > 0) {
        ret.add(data[ty][tx], 0.5 - sy);
        ret.add(data[ty+1][tx], sy);
      }
      else {
        ret.add(data[ty][tx], 0.5 - sy);
        ret.add(data[ty-1][tx], sy);
      }
    

      return ret;
    }
  };
  
  UpSample sampler(width, height, pixels, w, h);

  for (int y = 0; y < h; y++) {
    byte* row = dst + cbStride * y;
    for (size_t x = 0; x < w; x++) {
      auto p = sampler.getPixel(x, y);
      int xs = x * 4;
      row[xs + 1] = p.getG(); // Green 
      row[xs + 0] = p.getB(); // Blue
      row[xs + 2] = p.getR(); // Red
      row[xs + 3] = p.getA();
    }
  }

  return hbmp;
}

void Image::enumerateImages(vector<pair<wstring, size_t>>& img) const {
  img.clear();
  for (auto& bmp : images) {
    if (bmp.second.fileName.size() > 0)
      img.emplace_back(bmp.second.fileName, img.size());
  }
  sort(img.begin(), img.end());
}

uint64_t Image::getIdFromEnumeration(int enumerationIx) const {
  int ix = 0;
  for (auto& bmp : images) {
    if (bmp.second.fileName.size() > 0) {
      if (enumerationIx == ix++) {
        return bmp.first;
      }
    }
  }
  throw meosException("Internal error");
}

int Image::getEnumerationIxFromId(uint64_t imgId) const {
  int ix = 0;
  for (auto& bmp : images) {
    if (bmp.second.fileName.size() > 0) {
      if (imgId == bmp.first)
        return ix;
      ix++;
    }
  }
  return -1;
}

const wstring& Image::getFileName(uint64_t imgId) const {
  if (!hasImage(imgId))
    throw meosException("Missing image: " + itos(imgId));
  return images.at(imgId).fileName;
}

const vector<uint8_t>& Image::getRawData(uint64_t imgId) const {
  if (!hasImage(imgId))
    throw meosException("Missing image: " + itos(imgId));
  return images.at(imgId).rawData;
}


bool Image::hasImage(uint64_t imgId) const {
  auto res = images.find(imgId);
  return res != images.end() && res->second.rawData.size() > 0;
}
