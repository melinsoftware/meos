#pragma once

class AutoCompleteInfo;
class gdioutput;

class AutoCompleteHandler {
public:
  virtual ~AutoCompleteHandler() = 0 {}
  virtual void handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) = 0;
};
