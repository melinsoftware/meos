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
 

  static vector<uint8_t> loadResourceToMemory(LPCTSTR lpName, LPCTSTR lpType);
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

  int getWidth(int resource);
  int getHeight(int resource);
  void drawImage(int resource, ImageMethod method, HDC hDC, int x, int y, int width, int height);

  Image();
  ~Image();
};

