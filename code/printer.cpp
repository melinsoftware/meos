/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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

#include <commdlg.h>
#include "gdioutput.h"
#include "oEvent.h"
#include "meos_util.h"
#include "Table.h"
#include "gdifonts.h"
#include "Printer.h"
#include "gdiimpl.h"
#include <algorithm>
#include "meosexception.h"

extern gdioutput *gdi_main;
static bool bPrint;
const double inchmmk=2.54;

BOOL CALLBACK AbortProc(HDC,	int  iError)
{
  if (iError==0)
    return bPrint;
  else
  {
    bPrint=false;
    return false;
  }
}

#define WM_SETPAGE WM_USER+423

BOOL CALLBACK  AbortPrintJob(HWND hDlg, UINT message, WPARAM wParam, LPARAM) {
  return false;
}

PrinterObject::PrinterObject() {
  hDC=0;
  hDevMode=0;
  hDevNames=0;

  nPagesPrinted=0;
  nPagesPrintedTotal=0;
  onlyChanged=true;

  memset(&ds, 0, sizeof(ds));
  memset(&DevMode, 0, sizeof(DevMode));
}

PrinterObject::PrinterObject(const PrinterObject &po) {
  hDC=0;
  hDevMode=po.hDevMode;
  hDevNames=po.hDevNames;
  if (hDevMode)
    GlobalLock(hDevMode);
  if (hDevNames)
    GlobalLock(hDevNames);
  nPagesPrinted=0;
  nPagesPrintedTotal=0;
  onlyChanged=true;

  memcpy(&ds, &po.ds, sizeof(ds));
  memcpy(&DevMode, &po.DevMode, sizeof(DevMode));
}

void PrinterObject::operator=(const PrinterObject &po)
{
  hDC = 0;///???po.hDC;
  hDevMode=po.hDevMode;
  hDevNames=po.hDevNames;
  if (hDevMode)
    GlobalLock(hDevMode);
  if (hDevNames)
    GlobalLock(hDevNames);

  nPagesPrinted = po.nPagesPrinted;
  nPagesPrintedTotal = po.nPagesPrintedTotal;
  onlyChanged = po.onlyChanged;

  memcpy(&ds, &po.ds, sizeof(ds));
  memcpy(&DevMode, &po.DevMode, sizeof(DevMode));
}

PrinterObject::~PrinterObject()
{
  if (hDC)
    DeleteDC(hDC);

  freePrinter();
}

void gdioutput::printPage(PrinterObject &po, const PageInfo &pageInfo, RenderedPage &page)
{
  if (po.hDC)
    SelectObject(po.hDC, GetStockObject(DC_BRUSH));

  for (size_t k = 0; k < page.text.size(); k++) {
    // Use transformed coordinates
    page.text[k].ti.xp = (int)page.text[k].xp;
    page.text[k].ti.yp = (int)page.text[k].yp;
    RenderString(page.text[k].ti, po.hDC);
  }

  for (size_t k = 0; k < page.rectangles.size(); k++) {
    renderRectangle(po.hDC, 0, page.rectangles[k]);
  }

  if (pageInfo.printHeader) {
    TextInfo t;
    t.yp = po.ds.MarginY;
    t.xp = po.ds.PageX - po.ds.MarginX;
    t.text = pageInfo.pageInfo(page);
    t.format=textRight|fontSmall;
    RenderString(t, po.hDC);
  }
}

void gdioutput::printSetup(PrinterObject &po)
{
  destroyPrinterDC(po);

  PRINTDLG pd;
  memset(&pd, 0, sizeof(pd));

  pd.lStructSize = sizeof(PRINTDLG);
  pd.hDevMode = po.hDevMode;
  pd.hDevNames = po.hDevNames;

  pd.Flags = PD_RETURNDC|PD_USEDEVMODECOPIESANDCOLLATE|PD_PRINTSETUP;
  pd.hwndOwner = hWndAppMain;
  pd.hDC = (HDC) po.hDC;
  pd.nFromPage = 1;
  pd.nToPage = 1;
  pd.nMinPage = 1;
  pd.nMaxPage = 1;
  pd.nCopies = 1;
  pd.hInstance = (HINSTANCE) NULL;
  pd.lCustData = 0L;
  pd.lpfnPrintHook = (LPPRINTHOOKPROC) NULL;
  pd.lpfnSetupHook = (LPSETUPHOOKPROC) NULL;
  pd.lpPrintTemplateName = (LPCWSTR) NULL;
  pd.lpSetupTemplateName = (LPCWSTR)  NULL;
  pd.hPrintTemplate = (HANDLE) NULL;
  pd.hSetupTemplate = (HANDLE) NULL;

  int iret=PrintDlg(&pd);

  if (iret==false) {
    int error = CommDlgExtendedError();
    if (error!=0) {
      char sb[127];
      sprintf_s(sb, "Printing Error Code=%d", error);
      //MessageBox(hWnd, sb, NULL, MB_OK);
      alert(sb);
      po.freePrinter();
      po.hDC = 0;
    }
    return;
  }
  else {
    //Save settings!
    po.hDevMode = pd.hDevMode;
    po.hDevNames = pd.hDevNames;
    po.hDC=pd.hDC;

    DEVNAMES *dn=(LPDEVNAMES)GlobalLock(pd.hDevNames);
    if (dn) {
      DEVMODE *dm=(LPDEVMODE)GlobalLock(pd.hDevMode);
      if (dm) {
        po.DevMode=*dm;
        po.DevMode.dmSize=sizeof(po.DevMode);
      }
      po.Driver=(wchar_t *)(dn)+dn->wDriverOffset;//XXX WCS
      po.Device=(wchar_t *)(dn)+dn->wDeviceOffset;
      //GlobalUnlock(pd.hDevMode);
    }
    //GlobalUnlock(pd.hDevNames);
  }
}

void gdioutput::print(pEvent oe, Table *t, bool printMeOSHeader, bool noMargin, bool respectPageBreak) {
  PageInfo pageInfo;
  pageInfo.printHeader = printMeOSHeader;
  pageInfo.noPrintMargin = noMargin;

  setWaitCursor(true);
  PRINTDLG pd;

  PrinterObject &po = *po_default;
  po.printedPages.clear();//Don't remember

  pd.lStructSize = sizeof(PRINTDLG);
  pd.hDevMode = po.hDevMode;
  pd.hDevNames = po.hDevNames;
  pd.Flags = PD_RETURNDC;
  pd.hwndOwner = hWndAppMain;
  pd.hDC = (HDC)NULL;
  pd.nFromPage = 1;
  pd.nToPage = 1;
  pd.nMinPage = 1;
  pd.nMaxPage = 1;
  pd.nCopies = 1;
  pd.hInstance = (HINSTANCE)NULL;
  pd.lCustData = 0L;
  pd.lpfnPrintHook = (LPPRINTHOOKPROC)NULL;
  pd.lpfnSetupHook = (LPSETUPHOOKPROC)NULL;
  pd.lpPrintTemplateName = (LPCWSTR)NULL;
  pd.lpSetupTemplateName = (LPCWSTR)NULL;
  pd.hPrintTemplate = (HANDLE)NULL;
  pd.hSetupTemplate = (HANDLE)NULL;

  int iret = PrintDlg(&pd);

  if (iret == false) {
    int error = CommDlgExtendedError();
    if (error != 0) {
      char sb[128];
      sprintf_s(sb, "Printing Error Code=%d", error);
      alert(sb);
      po.freePrinter();
      po.hDC = 0;
    }
    return;
  }
  setWaitCursor(true);
  //Save settings!
  po.freePrinter();

  po.hDevMode = pd.hDevMode;
  po.hDevNames = pd.hDevNames;
  po.hDC = pd.hDC;

  if (t)
    t->print(*this, po.hDC, 20, 0);

  DEVNAMES *dn = (LPDEVNAMES)GlobalLock(pd.hDevNames);
  if (dn) {
    DEVMODE *dm = (LPDEVMODE)GlobalLock(pd.hDevMode);
    if (dm) {
      po.DevMode = *dm;
      po.DevMode.dmSize = sizeof(po.DevMode);
    }
    po.Driver = (wchar_t *)(dn)+dn->wDriverOffset;//XXX WCS
    po.Device = (wchar_t *)(dn)+dn->wDeviceOffset;
    //GlobalUnlock(pd.hDevMode);
    //GlobalUnlock(pd.hDevNames);
  }

  doPrint(po, pageInfo, oe, respectPageBreak);

  // Delete the printer DC.
  DeleteDC(pd.hDC);
  po.hDC = 0;
}

void gdioutput::print(PrinterObject &po, pEvent oe, bool printMeOSHeader, bool noMargin, bool respectPageBreak)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "print")
        return;
    }
    throw std::exception("Printing error");
  }
  PageInfo pageInfo;
  pageInfo.printHeader = printMeOSHeader;
  pageInfo.noPrintMargin = noMargin;

  if (po.hDevMode==0) {
  //if (po.Driver.empty()) {
    PRINTDLG pd;

    pd.lStructSize = sizeof(PRINTDLG);
    pd.hDevMode = 0;
    pd.hDevNames = 0;
    pd.Flags = PD_RETURNDEFAULT;
    pd.hwndOwner = hWndAppMain;
    pd.hDC = (HDC) NULL;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nMinPage = 1;
    pd.nMaxPage = 1;
    pd.nCopies = 1;
    pd.hInstance = (HINSTANCE) NULL;
    pd.lCustData = 0L;
    pd.lpfnPrintHook = (LPPRINTHOOKPROC) NULL;
    pd.lpfnSetupHook = (LPSETUPHOOKPROC) NULL;
    pd.lpPrintTemplateName = (LPCWSTR) NULL;
    pd.lpSetupTemplateName = (LPCWSTR)  NULL;
    pd.hPrintTemplate = (HANDLE) NULL;
    pd.hSetupTemplate = (HANDLE) NULL;

    int iret=PrintDlg(&pd);

    if (iret==false) {
      int error=CommDlgExtendedError();
      if (error!=0) {
        char sb[128];
        sprintf_s(sb, "Printing Error Code=%d", error);
        alert(sb);
        po.hDC = 0;
      }
      return;
    }
    po.freePrinter();
    po.hDevMode = pd.hDevMode;
    po.hDevNames = pd.hDevNames;

    DEVNAMES *dn=(LPDEVNAMES)GlobalLock(pd.hDevNames);
    if (dn) {
      DEVMODE *dm=(LPDEVMODE)GlobalLock(pd.hDevMode);
      if (dm) {
        po.DevMode=*dm;
        po.DevMode.dmSize=sizeof(po.DevMode);
      }

      po.Driver=(wchar_t *)(dn)+dn->wDriverOffset; //XXX WCS
      po.Device=(wchar_t *)(dn)+dn->wDeviceOffset;
      po.hDC=CreateDC(po.Driver.c_str(), po.Device.c_str(), NULL, dm);
      GlobalUnlock(pd.hDevMode);
    }
    GlobalUnlock(pd.hDevNames);
  }
  else if (po.hDC==0) {
    po.hDC = CreateDC(po.Driver.c_str(), po.Device.c_str(), NULL, &po.DevMode);
  }
  doPrint(po, pageInfo, oe, respectPageBreak);
}

void gdioutput::destroyPrinterDC(PrinterObject &po)
{
  if (po.hDC) {
    // Delete the printer DC.
    DeleteDC(po.hDC);
    po.hDC=0;
  }
}

bool gdioutput::startDoc(PrinterObject &po)
{
  // Initialize the members of a DOCINFO structure.
  DOCINFO di;
  int nError;
  di.cbSize = sizeof(DOCINFO);

  wchar_t sb[256];
  swprintf_s(sb, L"MeOS");

  di.lpszDocName = sb;
  di.lpszOutput = (LPTSTR) NULL;
  di.lpszDatatype = (LPTSTR) NULL;
  di.fwType = 0;      // Begin a print job by calling the StartDoc function.

  nError = StartDoc(po.hDC, &di);

  if (nError <= 0) {
    nError=GetLastError();
    DeleteDC(po.hDC);
    po.hDC=0;

    if (nError == ERROR_CANCELLED)
      return false;

    wstring err = L"Printing failed (X: Y) Z#StartDoc#"+ itow(nError) + L"#" + getErrorMessage(nError);
    throw meosException(err);
    //sprintf_s(sb, "Window's StartDoc API returned with error code %d,", nError);
    //alert("StartDoc error: " + getErrorMessage(nError));
    //return false;
  }
  return true;
}

bool gdioutput::doPrint(PrinterObject &po, PageInfo &pageInfo, pEvent oe, bool respectPageBreak)
{
  setWaitCursor(true);

  if (!po.hDC)
    return false;
  set<__int64> myPages;

  po.nPagesPrinted=0;
  PrinterObject::DATASET &ds=po.ds;

  //Do the printing
  int xsize = GetDeviceCaps(po.hDC, HORZSIZE);
  int ysize = GetDeviceCaps(po.hDC, VERTSIZE);

  int physX = GetDeviceCaps(po.hDC, PHYSICALWIDTH);
  int physY = GetDeviceCaps(po.hDC, PHYSICALHEIGHT);

  int physOffsetX = GetDeviceCaps(po.hDC, PHYSICALOFFSETX);
  int physOffsetY = GetDeviceCaps(po.hDC, PHYSICALOFFSETY);

  // Retrieve the number of pixels-per-logical-inch in the
  // horizontal and vertical directions for the printer upon which
  // the bitmap will be printed.
  int xtot = GetDeviceCaps(po.hDC, HORZRES);
  int ytot = GetDeviceCaps(po.hDC, VERTRES);

  SetMapMode(po.hDC, MM_ISOTROPIC);

  const bool limitSize = xsize > 100;

  int PageXMax=limitSize ? max(512, MaxX) : MaxX;
  int PageYMax=(ysize*PageXMax)/xsize;
  SetWindowExtEx(po.hDC, int(PageXMax*1.05), int(PageYMax*1.05), 0);
  SetViewportExtEx(po.hDC, xtot, ytot, NULL);
  // xPrint = ((mm / xsize) * physX - physOff) / xtot * PageXMax*1.05
  // xPrint =  mm * (physX * PageXMax * 1.05) / (xsize*xtot) - physOff * (PageXMax * 1.05/xtot)
  pageInfo.xMM2PrintC = double(physX * PageXMax * 1.05) / double(xsize*xtot);
  pageInfo.xMM2PrintK = double(-physOffsetX) * (PageXMax * 1.05/xtot);
  pageInfo.yMM2PrintC = double(physY * PageYMax * 1.05) / double(ysize*ytot);
  pageInfo.yMM2PrintK = double(-physOffsetY) * (PageYMax * 1.05/ytot);

  ds.PageX = PageXMax;
  ds.PageY = PageYMax;
  ds.MarginX = pageInfo.noPrintMargin ? (limitSize ? PageXMax/30: 5) : PageXMax/25;
  ds.MarginY = pageInfo.noPrintMargin ? 5 : 20;
  ds.Scale=1;
  ds.LastPage=false;

  int sOffsetY = OffsetY;
  int sOffsetX = OffsetX;
  OffsetY = 0;
  OffsetX = 0;

  pageInfo.topMargin = float(ds.MarginY * 2);
  pageInfo.scaleX = 1.0f;
  pageInfo.scaleY = 1.0f;
  
  pageInfo.leftMargin = float(ds.MarginX);
  pageInfo.bottomMargin = float(ds.MarginY);
  pageInfo.pageY = float(PageYMax);

  vector<RenderedPage> pages;
  pageInfo.renderPages(TL, Rectangles, false, respectPageBreak, pages);

  vector<int> toPrint;
  for (size_t k = 0; k < pages.size(); k++) {
    if (!po.onlyChanged || po.printedPages.count(pages[k].checkSum)==0) {
      toPrint.push_back(k);
      po.nPagesPrinted++;
      po.nPagesPrintedTotal++;
    }
    myPages.insert(pages[k].checkSum);
  }
  int nPagesToPrint=toPrint.size();

  if (nPagesToPrint>0) {

    if (!startDoc(po)) {
      return false;
    }

    for (size_t k = 0; k < toPrint.size(); k++) {

      int nError = StartPage(po.hDC);
      if (nError <= 0) {
        nError=GetLastError();

        EndDoc(po.hDC);
        DeleteDC(po.hDC);
        po.hDC=0;
        po.freePrinter();
        OffsetY = sOffsetY;
        OffsetX = sOffsetX;
        alert(L"StartPage error: " + getErrorMessage(nError));
        return false;
      }

      printPage(po, pageInfo, pages[toPrint[k]]);
      EndPage(po.hDC);
    }

    int nError = EndDoc(po.hDC);
    OffsetY = sOffsetY;
    OffsetX = sOffsetX;

    if (nError <= 0) {
      nError=GetLastError();
      DeleteDC(po.hDC);
      po.hDC=0;
      alert(L"EndDoc error: " + getErrorMessage(nError));
      return false;
    }
  }

  po.printedPages.swap(myPages);
  return true;
}

UINT CALLBACK PagePaintHook(HWND, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
  return false;
}

void PageSetup(HWND hWnd, PrinterObject &po)
{
  PrinterObject::DATASET &ds=po.ds;
  PAGESETUPDLG pd;

  memset(&pd, 0, sizeof(pd));

  pd.lStructSize=sizeof(pd);
  pd.hwndOwner=hWnd;
//	pd.hDevMode=po.hDevMode;
//	pd.hDevNames=po.hDevNames;
  pd.Flags=PSD_MARGINS|PSD_ENABLEPAGEPAINTHOOK|PSD_INHUNDREDTHSOFMILLIMETERS;
  pd.lpfnPagePaintHook=PagePaintHook;


  pd.rtMargin.left=int(ds.pMgLeft*float(ds.pWidth_mm)+0.5)*100;
  pd.rtMargin.right=int(ds.pMgRight*float(ds.pWidth_mm)+0.5)*100;
  pd.rtMargin.top=int(ds.pMgTop*float(ds.pHeight_mm)+0.5)*100;
  pd.rtMargin.bottom=int(ds.pMgBottom*float(ds.pHeight_mm)+0.5)*100;


  if (PageSetupDlg(&pd))
  {
    RECT rtMargin=pd.rtMargin;

    if (pd.Flags & PSD_INHUNDREDTHSOFMILLIMETERS)
    {
      rtMargin.top=long(rtMargin.top/inchmmk);
      rtMargin.bottom=long(rtMargin.bottom/inchmmk);
      rtMargin.right=long(rtMargin.right/inchmmk);
      rtMargin.left=long(rtMargin.left/inchmmk);
    }

    po.hDevMode=pd.hDevMode;
    po.hDevNames=pd.hDevNames;


    DEVMODE *dm=(LPDEVMODE)GlobalLock(pd.hDevMode);

    if (dm)
    {/*
      if (dm->dmFields&DM_COLOR)
      {
        /*if (dm->dmColor==DMCOLOR_MONOCHROME)
          ds.bPrintColour=false;
        else
          ds.bPrintColour=true;
      }*/
    }

    DEVNAMES *dn=(LPDEVNAMES)GlobalLock(pd.hDevNames);

    if (dn)
    {
      wchar_t *driver=(wchar_t *)(dn)+dn->wDriverOffset;//WCS
      wchar_t *device=(wchar_t *)(dn)+dn->wDeviceOffset;

      HDC hDC=CreateDC(driver, device, NULL, dm);

      if (hDC)
      {
        ds.pWidth_mm=GetDeviceCaps(hDC, HORZSIZE);
        ds.pHeight_mm=GetDeviceCaps(hDC, VERTSIZE);

        ds.pMgLeft=inchmmk*rtMargin.left/float(ds.pWidth_mm)/100.;
        ds.pMgRight=inchmmk*rtMargin.right/float(ds.pWidth_mm)/100.;
        ds.pMgTop=inchmmk*rtMargin.top/float(ds.pHeight_mm)/100.;
        ds.pMgBottom=inchmmk*rtMargin.bottom/float(ds.pHeight_mm)/100.;

        DeleteDC(hDC);
      }
    }

  }
}

void PrinterObject::freePrinter() {
  if (hDevNames)
    GlobalUnlock(hDevNames);
  hDevNames = 0;

  if (hDevMode)
    GlobalUnlock(hDevMode);
  hDevMode = 0;
}


//Uses format, text, xp and yp
void RenderedPage::calculateCS(const TextInfo &text)
{
  if (gdioutput::skipTextRender(text.format) ||
     text.text.empty())
    return;
  DWORD localCS=0;
  DWORD localCS2=0;
  for(DWORD i=0; i<text.text.size(); i++){
    localCS+=(BYTE(text.text[i])<<(i%24))*(i*i+text.yp);
    localCS2+=(BYTE(text.text[i])<<(i%23))*(i+17)*(i+text.xp);
  }
  localCS+=(text.format*10000000+text.xp+text.yp*1000);
  checkSum += __int64(localCS) + (__int64(localCS2)<<32);
}

struct PrintItemInfo {
  PrintItemInfo(int yp, const BaseInfo *obj) : yp(yp), obj(obj) {}
  int yp;
  const BaseInfo *obj;

  bool operator<(const PrintItemInfo &other) const {
    return yp < other.yp;
  }

  bool isNewPage() const {
    const TextInfo *ti = dynamic_cast<const TextInfo *>(obj);
    return ti && (ti->format == pageNewPage || ti->format == pageNewChapter);
  }

  bool isNewChapter() const {
    const TextInfo *ti = dynamic_cast<const TextInfo *>(obj);
    return ti && ti->format == pageNewChapter;
  }

  bool isNoPrint() const {
    const TextInfo *ti = dynamic_cast<const TextInfo *>(obj);
    return ti && gdioutput::skipTextRender(ti->format);
  }
};

void PageInfo::renderPages(const list<TextInfo> &tl,
                           const list<RectangleInfo> &rects,
                           bool invertHeightY,
                           bool respectPageBreak,
                           vector<RenderedPage> &pages) {
  const PageInfo &pi = *this;
  TIList::const_iterator it;
  pages.clear();
  if (tl.empty())
    return;
  int currentYP = 0;
  vector<PrintItemInfo> indexedTL;
  indexedTL.reserve(tl.size() + rects.size());
  int top = 1000;
  float minX = 1000;
  currentYP = tl.front().yp;
  bool needSort = false;
  for (it=tl.begin();it!=tl.end(); ++it) {
    const TextInfo &text = *it;
    if (text.format == 10)
      continue;
    if (!text.isFormatInfo()) {
      if (currentYP > text.yp) {
        needSort = true;
      }

      minX = min(minX, (float)it->textRect.left);
      top = min(top, text.yp);
    }
    currentYP = text.yp;
    
    indexedTL.push_back(PrintItemInfo(currentYP, &text));
  }

  for (list<RectangleInfo>::const_iterator rit = rects.begin(); rit != rects.end(); ++rit) {
    needSort = true;
    top = min<int>(top, rit->getRect().top);
    indexedTL.push_back(PrintItemInfo(rit->getRect().top, &*rit));
  }

  if (needSort)
    stable_sort(indexedTL.begin(), indexedTL.end());

  bool startChapter = true;
  bool addPage = true;
  bool wasOrphan = false;
  int offsetY = 0;
  int desiredChapterStartY = 0;
  wstring infoText;
  int extraLimit = 0;
  for (size_t k = 0; k < indexedTL.size(); k++) {
    const TextInfo *tlp = dynamic_cast<const TextInfo *>(indexedTL[k].obj);

    if (tlp == 0) {
      const RectangleInfo *ri = dynamic_cast<const RectangleInfo *>(indexedTL[k].obj);
      assert(ri && !pages.empty());
      if (!ri || pages.empty())
        throw std::exception("Unexpected type");

      pages.back().rectangles.push_back(*ri);
      RectangleInfo &r = pages.back().rectangles.back();

      ///xxxx
      r.rc.left = LONG((r.rc.left - minX) * pi.scaleX + pi.leftMargin);
      r.rc.right = LONG((r.rc.right - minX) * pi.scaleX + pi.leftMargin);
      
      int off = invertHeightY ? (r.rc.bottom - r.rc.top) : 0;
      r.rc.top = LONG((r.rc.top + offsetY + off) * pi.scaleY + pi.topMargin);
      r.rc.bottom = LONG((r.rc.bottom + offsetY + off) * pi.scaleY + pi.topMargin);
      continue;
    }

    if (tlp->format == pagePageInfo) {
      infoText = tlp->text;
      if (!pages.empty() && pages.back().info.empty())
        pages.back().info = infoText;
    }

    if (addPage) {
      wasOrphan = false;
      addPage = false;
      pages.push_back(RenderedPage());
      pages.back().nPage = pages.size();
      pages.back().info = infoText;
      pages.back().startChapter = startChapter;
      
      if (k == 0) {
        offsetY = 0;
        desiredChapterStartY = tlp->yp;
      }
      else if (startChapter) {
        offsetY = desiredChapterStartY - tlp->yp + extraLimit;
      }
      else
        offsetY =  -tlp->yp + extraLimit;

      extraLimit = 0;
      startChapter = false;
    }

    if (gdioutput::skipTextRender(tlp->format))
        continue;

    pages.back().text.push_back(PrintTextInfo(*tlp));
    PrintTextInfo &text = pages.back().text.back();

    text.ti.yp +=  offsetY;
    text.ti.highlight=0;
    text.ti.hasCapture=0;
    text.ti.active=0;
    text.ti.callBack=0;
    text.ti.hasTimer=false;

    if (text.ti.absPrintX > 0) {
      text.xp = float(text.ti.absPrintX * pi.xMM2PrintC + pi.xMM2PrintK);
      text.yp = float(text.ti.absPrintY * pi.yMM2PrintC + pi.yMM2PrintK);
    }
    else {
      text.xp = (text.ti.xp - minX) * pi.scaleX + pi.leftMargin;
      int off = invertHeightY ? (text.ti.textRect.bottom - text.ti.textRect.top) : 0;
      text.yp = (text.ti.yp + off) * pi.scaleY + pi.topMargin;
    }

    pages.back().calculateCS(text.ti);
#ifdef _DEBUG
    static wchar_t breakbuff[8] = L"1429586";
    if (text.ti.text == breakbuff) {
      text.ti.text.empty(); // Break when hitting symbol
    }
#endif

    if (k + 1 < indexedTL.size() && tlp->yp != indexedTL[k+1].yp) {
      size_t j = k + 1;
      while (j + 1 < indexedTL.size() && indexedTL[j].isNoPrint() && !indexedTL[j].isNewPage())
        j++;

      // Required new page
      if (indexedTL[j].isNewPage()) {
        k++;
        if (respectPageBreak) {
          addPage = true;
          startChapter = indexedTL[j].isNewChapter();
          extraLimit = indexedTL[j].obj->getExtraInt();
          infoText.clear();
          continue;
        }
      }

      map<int, int> forwardyp;
      while ( j < indexedTL.size() && forwardyp.size() < 3) {
        if (!indexedTL[j].isNewPage() && !indexedTL[j].isNoPrint()) {
          if (forwardyp.count(indexedTL[j].yp) == 0 || indexedTL[j].isNewPage())
            forwardyp[indexedTL[j].yp] = j;
        }
        j++;
      }

      int ix = 0;
      float lastSize = GDIImplFontSet::baseSize(tlp->format, 1.0);
      bool nextIsHead = false;
      bool firstCanBreak = true;
      for (map<int,int>::iterator it = forwardyp.begin(); it != forwardyp.end(); ++it, ++ix) {
        const TextInfo *tlpItSecond = dynamic_cast<const TextInfo *>(indexedTL[it->second].obj);
        if (!tlpItSecond)
          continue;

        float y = (max<int>(tlpItSecond->textRect.bottom, indexedTL[it->second].yp) + offsetY) * pi.scaleY + pi.topMargin;
        float size = GDIImplFontSet::baseSize(tlpItSecond->format, 1.0);

        bool over = y > pi.pageY - pi.bottomMargin;
        bool canBreak = tlpItSecond->lineBreakPrioity >= 0;

        if (ix == 0 && lastSize < size)
          nextIsHead = true;

        if (ix == 0 && !canBreak)
          firstCanBreak = false; // First can break;

        if (ix > 0 && firstCanBreak && canBreak)
          firstCanBreak = false; // There is a more suitable break later

        if (over) {
          if (addPage && ix == 1 && lastSize < size && !wasOrphan) {
            wasOrphan = true;
            addPage = false; // Keep this line on this page. Orphan, next is head.
            break;
          }
          if (ix == 0 && forwardyp.size()>1) { // forwardyp.size()>1 -> more than one lines left
            if (!canBreak) {
              if (!wasOrphan) {
                wasOrphan = true;
                break; // Skip breaking here.
              }
              wasOrphan = false;
            }
            addPage = true;
          }
          else if (ix > 0 && !canBreak && firstCanBreak) {
            addPage = true; // Use this as a suitable break
          }
          else if (nextIsHead) {
            addPage = true;
          }
        }

        lastSize = size;
      }
    }
  }
  nPagesTotal = pages.size();
}

wstring PageInfo::pageInfo(const RenderedPage &page) const {
  if (printHeader) {
    wchar_t bf[256];
    if (nPagesTotal > 1) {
      if (!page.info.empty())
        swprintf_s(bf, L"MeOS %s, %s, (%d/%d)", getLocalTime().c_str(),
                  page.info.c_str(), page.nPage, nPagesTotal);
      else
        swprintf_s(bf, L"MeOS %s, (%d/%d)", getLocalTime().c_str(), page.nPage, nPagesTotal);
    }
    else {
      if (!page.info.empty())
        swprintf_s(bf, L"MeOS %s, %s", getLocalTime().c_str(), page.info.c_str());
      else
        swprintf_s(bf, L"MeOS %s", getLocalTime().c_str());
    }
    return bf;
  }
  else return L"";
}
