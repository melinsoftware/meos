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
#include "Table.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <exception>
#include <algorithm>
#include <set>
#include "oEvent.h"
#include "localizer.h"
#include <cassert>
#include "gdiconstants.h"
#include "meosexception.h"
#include "recorder.h"
#include "oDataContainer.h"

extern HINSTANCE hInst;
const char *tId="_TABLE_SEL";

const Table *TableSortIndex::table = 0;

int Table::uniqueId = 1;

Table::Table(oEvent *oe_, int rowH,
             const wstring &name, const string &iName)
{
  id = uniqueId++;
  commandLock = false;
  oe=oe_;
  tableName=name;
  internalName = iName;
  nTitles=0;
  PrevSort=-1;
  baseRowHeight=rowH;
  rowHeight = 0;
  highRow=-1;
  highCol=-1;
  colSelected=-1;

  editRow=-1;
  editCol=-1;
  drawFilterLabel=false;
  currentSortColumn=-1;
  hEdit=0;

  hdcCompatible=0;
  hbmStored=0;

  hdcCompatibleCell = 0;
  hbmStoredCell = 0;
  partialCell = false;

  startSelect = false;
  clearCellSelection(0);
  tableProp = -1;
  dataPointer = -1;

  clearOnHide = true;
  doAutoSelectColumns = true;

  generator = 0;
  generatorPtr = 0;
}

Table::~Table(void)
{
  if (hEdit)
    DestroyWindow(hEdit);
}

void Table::clearCellSelection(gdioutput *gdi) {
  upperRow = -1;
  lowerRow = -1;
  upperCol = -1;
  lowerCol = -1;
  if (gdi) {
    HDC hDC = GetDC(gdi->getHWNDTarget());
    clearSelectionBitmap(gdi, hDC);
    ReleaseDC(gdi->getHWNDTarget(), hDC);
  }
}

int Table::addColumn(const string &Title, int width, bool isnum, bool formatRight) {
  return addColumn(lang.tl(Title).c_str(), width, isnum, formatRight);
}

int Table::addColumn(const wstring &translatedTitle, int width, bool isnum, bool formatRight) {
  ColInfo ri;
  wcscpy_s(ri.name, translatedTitle.c_str());
  ri.baseWidth = width;
  ri.width = 0;
  ri.padWidthZeroSort = 0;
  ri.isnumeric = isnum;
  ri.formatRight = formatRight;
  Titles.push_back(ri);
  columns.push_back(nTitles);
  nTitles++;
  return Titles.size() - 1;
}

int Table::addColumnPaddedSort(const string &title, int width, int padding, bool formatRight) {
  ColInfo ri;
  wcscpy_s(ri.name, lang.tl(title).c_str());
  ri.baseWidth = width;
  ri.width = 0;
  ri.padWidthZeroSort = padding;
  ri.isnumeric = false;
  ri.formatRight = formatRight;
  Titles.push_back(ri);
  columns.push_back(nTitles);
  nTitles++;
  return Titles.size()-1;
}


void Table::moveColumn(int src, int target)
{
  if (src==target)
    return;

  vector<int>::iterator it_s=find(columns.begin(), columns.end(), src);
  vector<int>::iterator it_t=find(columns.begin(), columns.end(), target);

  if (it_s!=columns.end()) {

    if (it_s<it_t) {
      columns.erase(it_s);
      it_t=find(columns.begin(), columns.end(), target);

      if (it_t!=columns.end())
        ++it_t;

      columns.insert(it_t, src);
    }
    else {
      columns.erase(it_s);
      it_t=find(columns.begin(), columns.end(), target);
      columns.insert(it_t, src);
    }
  }
}

void Table::reserve(size_t siz) {
  Data.reserve(siz+3);
  sortIndex.reserve(siz+3);
  idToRow.resize(siz+3);
}

TableRow *Table::getRowById(int rowId) {
  int ix;
  if (idToRow.lookup(rowId, ix)) {
    return &Data[ix];
  }
  return 0;
}

void Table::addRow(int rowId, oBase *object)
{
  int ix;
  if (rowId>0 && idToRow.lookup(rowId, ix)) {
    dataPointer = ix;
    return;
  }

  TableRow tr(nTitles, object);
  tr.height=rowHeight;
  tr.id = rowId;
  TableSortIndex tsi;

  if (Data.empty()) {
    sortIndex.clear();
    tsi.index=0;
    sortIndex.push_back(tsi);
    tsi.index=1;
    sortIndex.push_back(tsi);

    tsi.index=2;
    Data.resize(2, tr);

    for (unsigned i=0;i<nTitles;i++) {
      Data[0].cells[i].contents=Titles[i].name;
      Data[1].cells[i].contents = L"...";

      Data[0].cells[i].canEdit=false;
      Data[0].cells[i].type=cellEdit;
      Data[0].cells[i].ownerRef.reset();

      Data[1].cells[i].canEdit=false;
      Data[1].cells[i].type=cellEdit;
      Data[1].cells[i].ownerRef.reset();
    }
  }
  else {
    //tsi.index=sortIndex.size();
    tsi.index = Data.size();
  }
  if (rowId>0)
    idToRow[rowId]=Data.size();

  dataPointer = Data.size();
  sortIndex.push_back(tsi);
  Data.push_back(tr);
}

void Table::set(int column, oBase &owner, int id, const wstring &data, bool canEdit, CellType type)
{
  if (dataPointer >= Data.size() || dataPointer<2)
    throw std::exception("Internal table error: wrong data pointer");

  TableRow &row=Data[dataPointer];
  TableCell &cell=row.cells[column];
  cell.contents=data;
  cell.ownerRef = owner.getReference();
  cell.id=id;
  cell.canEdit=canEdit;
  cell.type=type;
}

void Table::filter(int col, const wstring &filt, bool forceFilter)
{
  const wstring &oldFilter=Titles[col].filter;
  vector<TableSortIndex> baseIndex;

  if (filt==oldFilter && (!forceFilter || filt.empty()))
    return;
  else if (wcsncmp(oldFilter.c_str(), filt.c_str(), oldFilter.length())==0) {
    if (sortIndex.empty())
      return;
    //Filter more...
    baseIndex.resize(2);
    baseIndex[0]=sortIndex[0];
    baseIndex[1]=sortIndex[1];
    swap(baseIndex, sortIndex);
    Titles[col].filter=filt;
  }
  else {
    //Filter less -> refilter all!
    Titles[col].filter=filt;
    sortIndex.resize(Data.size());
    for (size_t k=0; k<Data.size();k++) {
      sortIndex[k].index = k;
    }

    for (unsigned k=0;k<nTitles;k++) {
      filter(k, Titles[k].filter, true);
    }

    PrevSort = -1;
    return;
  }

  wchar_t filt_lc[1024];
  wcscpy_s(filt_lc, filt.c_str());
  CharLowerBuff(filt_lc, filt.length());

  sortIndex.resize(2);
  for (size_t k=2;k<baseIndex.size();k++) {
    if (filterMatchString(Data[baseIndex[k].index].cells[col].contents, filt_lc))
      sortIndex.push_back(baseIndex[k]);
  }
}

bool Table::compareRow(int indexA, int indexB) const {
  const TableRow &a = Data[indexA];
  const TableRow &b = Data[indexB];
  if (a.intKey != b.intKey && (a.intKey != 0xFEFEFEFE && b.intKey != 0xFEFEFEFE))
    return a.intKey < b.intKey;
  else
    return CompareString( LOCALE_USER_DEFAULT, 0, Data[indexA].key.c_str(), Data[indexA].key.length(),
                                                  Data[indexB].key.c_str(), Data[indexB].key.length()) == CSTR_LESS_THAN;
    //return Data[indexA].key < Data[indexB].key;
    //return Data[indexA].key < Data[indexB].key;
}

void Table::sort(int col)
{
  bool reverse = col < 0;
  if (col < 0)
    col = -(10+col);

  if (sortIndex.size()<2)
    return;
  currentSortColumn=col;
  if (PrevSort!=col && PrevSort!=-(10+col)) {
    if (Titles[col].isnumeric) {
      bool hasDeci = false;
      for(size_t k=2; k<sortIndex.size(); k++){
        Data[sortIndex[k].index].key.clear();
        const wchar_t *str = Data[sortIndex[k].index].cells[col].contents.c_str();

        int i = 0;
        while (str[i] != 0 && str[i] != ':' && str[i] != ',' && str[i] != '.')
          i++;

        if (str[i]) {
          hasDeci = true;
          break;
        }

        i = 0;
        while (str[i] != 0 && (str[i] < '0' || str[i] > '9'))
          i++;

        int key = _wtoi(str + i);
        Data[sortIndex[k].index].intKey = key;
        if (key == 0)
          Data[sortIndex[k].index].key = Data[sortIndex[k].index].cells[col].contents; 
      }

      if (hasDeci) { // Times etc.
       for(size_t k=2; k<sortIndex.size(); k++){
          Data[sortIndex[k].index].key.clear();
          const wchar_t *str = Data[sortIndex[k].index].cells[col].contents.c_str();

          int i = 0;
          while (str[i] != 0 && (str[i] < '0' || str[i] > '9'))
            i++;

          int key = 0;

          while (str[i] >= '0' && str[i] <= '9') {
            key = key * 10 + (str[i] - '0');
            i++;
          }

          if (str[i] == ':' || str[i]==',' || str[i] == '.' || (str[i] == '-' && key != 0)) {
            bool valid = true;
            for (int j = 1; j <= 4; j++) {
              if (valid && str[i+j] >= '0' && str[i+j] <= '9')
                key = key * 10 + (str[i+j] - '0');
              else {
                key *= 10;
                valid = false;
              }
            }
          }
          else {
            key *= 10000;
          }

          Data[sortIndex[k].index].intKey = key;
          if (key == 0)
            Data[sortIndex[k].index].key = Data[sortIndex[k].index].cells[col].contents; 
        }
      }
    }
    else {
      if (Titles[col].padWidthZeroSort) {
        for (size_t k=2; k<sortIndex.size(); k++) {
          wstring &key = Data[sortIndex[k].index].key;
          const wstring &contents = Data[sortIndex[k].index].cells[col].contents;
          if (contents.length() < unsigned(Titles[col].padWidthZeroSort)) {
            key.resize(Titles[col].padWidthZeroSort+1);
            int cl = Titles[col].padWidthZeroSort-contents.length();
            for (int i = 0; i < cl; i++)
              key[i] = '0';
            for (int i = cl; i < Titles[col].padWidthZeroSort; i++)
              key[i] = contents[i-cl];
          }
          else
            key = contents;

          const wchar_t *strBuff = (const wchar_t *)key.c_str();
          CharUpperBuff(LPWSTR(strBuff), key.size());

          int &intKey = Data[sortIndex[k].index].intKey;
          intKey = unsigned(strBuff[0])<<16;
          if (key.length() > 1) {
            intKey |= unsigned(strBuff[1]);
          }
        }
      }
      else {
        for (size_t k=2; k<sortIndex.size(); k++) {
          wstring &key = Data[sortIndex[k].index].key;
          key = Data[sortIndex[k].index].cells[col].contents;
          const wchar_t *strBuff = (const wchar_t *)key.c_str();
          CharUpperBuff(LPWSTR(strBuff), key.size());

          int &intKey = Data[sortIndex[k].index].intKey;
          
          if ( ((strBuff[0]|strBuff[1]) & ~127) == 0) {
            intKey = unsigned(strBuff[0])<<16;
            if (key.length() > 1) {
              intKey |= unsigned(strBuff[1])<<8;
              //intKey |= unsigned(strBuff[2]);
            }
          }
          else {
            intKey = 0xFEFEFEFE;
          }
        }
      }
    }
    assert(TableSortIndex::table == 0);
    TableSortIndex::table = this;
    //DWORD sStart = GetTickCount();
    std::stable_sort(sortIndex.begin()+2, sortIndex.end());
    //DWORD sEnd = GetTickCount();
    //string st = itos(sEnd-sStart);
    TableSortIndex::table = 0;
    PrevSort=col;

    if (reverse)
      std::reverse(sortIndex.begin()+2, sortIndex.end());
  }
  else {
    std::reverse(sortIndex.begin()+2, sortIndex.end());

    if (PrevSort==col)
      PrevSort=-(10+col);
    else
      PrevSort=col;
  }
}

int TablesCB(gdioutput *gdi, int type, void *data) {
  if (type!=GUI_LINK || gdi->Tables.empty())
    return 0;

  TableInfo &tai=gdi->Tables.front();
  auto &t = tai.table;

  TextInfo *ti=(TextInfo *)data;

  if (ti->id.substr(0,4)=="sort"){
    int col=atoi(ti->id.substr(4).c_str());
    t->sort(col);
  }

  gdi->refresh();
  return 0;
}

void Table::getDimension(gdioutput &gdi, int &dx, int &dy, bool filteredResult) const
{
  rowHeight = gdi.scaleLength(baseRowHeight);

  if (filteredResult)
    dy = rowHeight * (1+sortIndex.size());
  else
    dy = rowHeight * (1+max<size_t>(2,Data.size()));

  dx=1;
  for(size_t i=0;i<columns.size();i++) {
    Titles[columns[i]].width = gdi.scaleLength(Titles[columns[i]].baseWidth);
    dx += Titles[columns[i]].width + 1;
  }
}

void Table::redrawCell(gdioutput &gdi, HDC hDC, int c, int r)
{
  if (unsigned(r)<Data.size() && unsigned(c)<unsigned(nTitles)) {
    const TableRow &row=Data[r];
    const TableCell &cell=row.cells[c];
    RECT rc=cell.absPos;
    draw(gdi, hDC, table_xp+gdi.OffsetX, table_yp+gdi.OffsetY, rc);
  }
}
void Table::startMoveCell(HDC hDC, const TableCell &cell)
{
  RECT rc=cell.absPos;
  int cx=rc.right-rc.left+1;
  int cy=rc.bottom-rc.top;

  hdcCompatible = CreateCompatibleDC(hDC);
  hbmStored = CreateCompatibleBitmap(hDC, cx, cy);

  // Select the bitmaps into the compatible DC.
  SelectObject(hdcCompatible, hbmStored);

  lastX=rc.left;
  lastY=rc.top;
  BitBlt(hdcCompatible, 0, 0, cx, cy, hDC, lastX, lastY, SRCCOPY);
}

void Table::restoreCell(HDC hDC, const TableCell &cell)
{
  const RECT &rc=cell.absPos;
  int cx=rc.right-rc.left+1;
  int cy=rc.bottom-rc.top;

  if (hdcCompatible) {
    //Restore bitmap
    BitBlt(hDC, lastX, lastY, cx, cy, hdcCompatible, 0, 0, SRCCOPY);
  }
}

void Table::moveCell(HDC hDC, gdioutput &gdi, const TableCell &cell, int dx, int dy)
{
  RECT rc=cell.absPos;
  rc.left+=dx; rc.right+=dx;
  rc.top+=dy;  rc.bottom+=dy;
  int cx=rc.right-rc.left+1;
  int cy=rc.bottom-rc.top;

  if (hdcCompatible) {
    lastX=rc.left;
    lastY=rc.top;
    //Take new
    BitBlt(hdcCompatible, 0, 0, cx, cy, hDC, lastX, lastY , SRCCOPY);
  }
  else {
    startMoveCell(hDC, cell);
    dx=0;
    dy=0;
  }
  highlightCell(hDC, gdi, cell, RGB(255, 0,0), dx, dy);
}

void Table::stopMoveCell(HDC hDC, const TableCell &cell, int dx, int dy)
{
  if (hdcCompatible) {
    restoreCell(hDC, cell);
/*    RECT rc=cell.absPos;
    rc.left+=dx; rc.right+=dx;
    rc.top+=dy;  rc.bottom+=dy;
    int cx=rc.right-rc.left+2;
    int cy=rc.bottom-rc.top+2;

    //Restore bitmap
    BitBlt(hDC, lastX, lastY, cx, cy, hdcCompatible, 0, 0, SRCCOPY);
*/
    DeleteDC(hdcCompatible);
    hdcCompatible=0;
  }

  if (hbmStored) {
    DeleteObject(hbmStored);
    hbmStored=0;
  }
}

bool Table::mouseMove(gdioutput &gdi, int x, int y)
{
  int row=getRow(y);
  int col=-1;

  if (row!=-1)
    col=getColumn(x);

  HWND hWnd=gdi.getHWNDTarget();

  if (colSelected!=-1) {
    TableCell &cell=Data[0].cells[colSelected];
    HDC hDC=GetDC(hWnd);

    restoreCell(hDC, cell);

    if (col!=highCol) {
      if (unsigned(highRow)<Data.size() && unsigned(highCol)<nTitles)
        redrawCell(gdi, hDC, highCol, highRow);

      DWORD c=RGB(240, 200, 140);
      if (unsigned(col)<nTitles)
        highlightCell(hDC, gdi, Data[0].cells[col], c, 0,0);
    }

    //highlightCell(hDC, cell, RGB(255,0,0), x-startX, y-startY);
    moveCell(hDC, gdi, cell,  x-startX, y-startY);
    ReleaseDC(hWnd, hDC);
    highRow=0;
    highCol=col;
    return false;
  }

  RECT rc;
  GetClientRect(hWnd, &rc);

  if (x<=rc.left || x>=rc.right || y<rc.top || y>rc.bottom)
    row=-1;

  bool ret = false;

  if (startSelect) {
    int c = getColumn(x, true);
    if (c != -1)
      upperCol = c;
    c = getRow(y, true);
    if (c != -1 && c>=0) {
      upperRow = max<int>(c, 2);
    }

    HDC hDC=GetDC(hWnd);
    if (unsigned(highRow)<Data.size() && unsigned(highCol)<Titles.size())
      redrawCell(gdi, hDC, highCol, highRow);
    highRow = -1;
    drawSelection(gdi, hDC, false);
    ReleaseDC(hWnd, hDC);
    scrollToCell(gdi, upperRow, upperCol);
    ret = true;
  }
  else if (row>=0 && col>=0) {
    POINT pt = {x, y};
    ClientToScreen(hWnd, &pt);
    HWND hUnder = WindowFromPoint(pt);

    if (hUnder == hWnd) {
      //int index=sortIndex[row].index;
      TableRow &trow=Data[row];
      TableCell &cell=trow.cells[col];

      if (highRow!=row || highCol!=col) {

        HDC hDC=GetDC(hWnd);

        if (unsigned(highRow)<Data.size() && unsigned(highCol)<Titles.size())
          redrawCell(gdi, hDC, highCol, highRow);

        if (row >= 2) {
          DWORD c;
          if (cell.canEdit)
            c=RGB(240, 240, 150);
          else
            c=RGB(240, 200, 140);

          highlightCell(hDC, gdi, cell, c, 0,0);
        }

        ReleaseDC(hWnd, hDC);
        SetCapture(hWnd);
        highCol=col;
        highRow=row;
      }
      ret = true;
    }
  }

  if (ret)
    return true;

  if (unsigned(highRow)<Data.size() && unsigned(highCol)<Titles.size()) {
    ReleaseCapture();
    HDC hDC=GetDC(hWnd);
    redrawCell(gdi, hDC, highCol, highRow);
    ReleaseDC(hWnd, hDC);
    highRow=-1;
  }
  return false;
}

bool Table::mouseLeftUp(gdioutput &gdi, int x, int y)
{
  if (colSelected!=-1) {

    if (hdcCompatible) {
      TableCell &cell=Data[0].cells[colSelected];
      HWND hWnd=gdi.getHWNDTarget();
      HDC hDC=GetDC(hWnd);
      stopMoveCell(hDC, cell, x-startX, y-startY);
      ReleaseDC(hWnd, hDC);
      //return true;
    }

    if (highRow==0 && colSelected==highCol) {
      colSelected=-1;
      gdi.setWaitCursor(true);
      sort(highCol);
      gdi.setWaitCursor(false);
      gdi.refresh();
      mouseMove(gdi, x, y);
      return true;
    }
    else {
      moveColumn(colSelected, highCol);
      InvalidateRect(gdi.getHWNDTarget(), 0, false);
      colSelected=-1;
      return true;
    }
  }
  else {
    upperCol = getColumn(x);
    upperRow = getRow(y);
    startSelect = false;
    ReleaseCapture();
  }
  colSelected=-1;
  return false;
}

int tblSelectionCB(gdioutput *gdi, int type, void *data)
{
  if (type == GUI_LISTBOX) {
    ListBoxInfo lbi = *static_cast<ListBoxInfo *>(data);
    Table &t = gdi->getTable();
    t.selection(*gdi, lbi.text, lbi.data);
  }
  return 0;
}

void Table::selection(gdioutput &gdi, const wstring &text, int data) {
  if (size_t(selectionRow) >= Data.size() || size_t(selectionCol) >= Titles.size())
    throw std::exception("Index out of bounds.");

  TableCell &cell = Data[selectionRow].cells[selectionCol];
  int id = Data[selectionRow].id;
  pair<int, bool> res;
  if (cell.hasOwner())
    res = cell.getOwner()->inputData(cell.id, text, data, cell.contents, false);
  if (res.second) {
    update();
    gdi.refresh();
  }
  else {
    reloadRow(id);
    if (res.first)
      reloadRow(res.first);
    //RECT rc;
    //getRowRect(selectionRow, rc);
    InvalidateRect(gdi.getHWNDTarget(), nullptr, false);
  }
}

#ifndef MEOSDB

bool Table::keyCommand(gdioutput &gdi, KeyCommandCode code) {
  if (commandLock)
    return false;
  commandLock = true;

  try {
    if (code == KC_COPY && hEdit == 0)
      exportClipboard(gdi);
    else if (code == KC_PASTE && hEdit == 0) {
      importClipboard(gdi);
    }else if (code == KC_DELETE && hEdit == 0) {
      deleteSelection(gdi);
    }
    else if (code == KC_REFRESH) {
      gdi.setWaitCursor(true);
      update();
      autoAdjust(gdi);
      gdi.refresh();
    }
    else if (code == KC_INSERT) {
      insertRow(gdi);
      gdi.refresh();
    }
    else if (code == KC_PRINT) {
      gdioutput gdiPrint("temp", gdi.getScale());
      gdiPrint.clearPage(false);
      gdiPrint.print(getEvent(), this);
    }
  }
  catch (...) {
    commandLock = false;
    throw;
  }

  commandLock = false;
  return false;
}

#endif

bool Table::deleteSelection(gdioutput &gdi) {
  int r1, r2;
  getRowRange(r1, r2);
  if (r1 != -1 && r2 != -1 && r1<=r2) {
    if (!gdi.ask(L"Vill du radera X rader från tabellen?#" + itow(r2-r1+1)))
      return false;
    gdi.setWaitCursor(true);
    int failed = deleteRows(r1, r2);
    gdi.refresh();
    gdi.setWaitCursor(false);
    if (failed > 0)
      gdi.alert("X rader kunde inte raderas.#" + itos(failed));
  }
  return true;
}

void Table::hide(gdioutput &gdi) {
  try {
    destroyEditControl(gdi);
  }
  catch (meosException &ex) {
    gdi.alert(ex.wwhat());
  }
  catch (std::exception &ex) {
    gdi.alert(ex.what());
  }

  clearCellSelection(0);
  ReleaseCapture();
  if (clearOnHide) {
    Data.clear();
    sortIndex.clear();
    idToRow.clear();
    clear();
  }
}

void Table::clear() {
  Data.clear();
  sortIndex.clear();
  idToRow.clear();
}

bool Table::destroyEditControl(gdioutput &gdi) {
  colSelected=-1;

  if (hEdit) {
    try {
      if (!enter(gdi))
        return false;
    }
    catch (std::exception &) {
      if (hEdit) {
        DestroyWindow(hEdit);
        hEdit=0;
      }
      throw;
    }
    if (hEdit) {
      DestroyWindow(hEdit);
      hEdit=0;
    }
  }
  gdi.removeWidget(tId);

  if (drawFilterLabel) {
    drawFilterLabel=false;
    gdi.refresh();
  }

  return true;
}

bool Table::mouseLeftDown(gdioutput &gdi, int x, int y) {
  partialCell = true;
  clearCellSelection(&gdi);

  if (!destroyEditControl(gdi))
    return false;

  if (highRow==0) {
    colSelected=highCol;
    startX=x;
    startY=y;
    //sort(highCol);
    //gdi.refresh();
    //mouseMove(gdi, x, y);
  }
  else if (highRow==1) {
    //filter(highCol, "lots");
    RECT rc=Data[1].cells[columns[0]].absPos;
    //rc.right=rc.left+tableWidth;
    editRow=highRow;
    editCol=highCol;

    hEdit=CreateWindowEx(0, L"EDIT", Titles[highCol].filter.c_str(),
        WS_TABSTOP|WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL|WS_BORDER,
        rc.left+105, rc.top, tableWidth-105, (rc.bottom-rc.top-1), gdi.getHWNDTarget(),
        0, hInst, 0);
    drawFilterLabel=true;
    SendMessage(hEdit, EM_SETSEL, 0, -1);
    SetFocus(hEdit);
    SendMessage(hEdit, WM_SETFONT, (WPARAM) gdi.getGUIFont(), 0);
    gdi.refresh();
  }
  else {
    SetFocus(gdi.getHWNDTarget());
    SetCapture(gdi.getHWNDTarget());
    lowerCol = getColumn(x);
    lowerRow = getRow(y);
    startSelect = true;
  }
  return false;
}

bool Table::mouseLeftDblClick(gdioutput &gdi, int x, int y)
{
  clearCellSelection(&gdi);

  if (!destroyEditControl(gdi))
    return false;

  if (unsigned(highRow)>=Data.size() || unsigned(highCol)>=Titles.size()) {
    return false;
  }


  if (highRow != 0 && highRow != 1) {
    if (editCell(gdi, highRow, highCol))
      return true;
  }
  return false;
}

bool Table::editCell(gdioutput &gdi, int row, int col) {
  TableCell &cell = Data[row].cells[col];

  if (cell.type == cellAction) {
    ReleaseCapture();
    gdi.makeEvent("CellAction", internalName, cell.id, cell.hasOwner() ? cell.getOwner()->getId() : 0, false);
    return true;
  }

  if (!cell.canEdit) {
    MessageBeep(-1);
    return true;
  }
  ReleaseCapture();

  editRow = row;
  editCol = col;

  RECT &rc=cell.absPos;

  if (cell.type == cellSelection || cell.type == cellCombo) {
    selectionRow = row;
    selectionCol = col;

    vector< pair<wstring, size_t> > out;
    size_t selected = 0;
    if (cell.hasOwner())
      cell.getOwner()->fillInput(cell.id, out, selected);

    int width = 40;
    for (size_t k = 0; k<out.size(); k++)
      width = max<int>(width, 8*out[k].first.length());

    if (cell.type == cellSelection) {
      gdi.addSelection(rc.left+gdi.OffsetX, rc.top+gdi.OffsetY, tId,
              max<int>(int((rc.right-rc.left+1)/gdi.scale), width), (rc.bottom-rc.top)*10,
              tblSelectionCB).setExtra(id);
    }
    else {
      gdi.addCombo(rc.left+gdi.OffsetX, rc.top+gdi.OffsetY, tId,
              max<int>(int((rc.right-rc.left+1)/gdi.scale), width), (rc.bottom-rc.top)*10,
              tblSelectionCB).setExtra(id);
    }
    gdi.addItem(tId, out);
    gdi.selectItemByData(tId, selected);
    return true;
  }
  else if (cell.type==cellEdit) {
    hEdit=CreateWindowEx(0, L"EDIT", cell.contents.c_str(),
      WS_TABSTOP|WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL|WS_BORDER,
      rc.left, rc.top, rc.right-rc.left+1, (rc.bottom-rc.top), gdi.getHWNDTarget(),
      0, hInst, 0);
    SendMessage(hEdit, EM_SETSEL, 0, -1);
    SetFocus(hEdit);
    SendMessage(hEdit, WM_SETFONT, (WPARAM) gdi.getGUIFont(), 0);
    return true;
  }

  return false;
}

RGBQUAD RGBA(BYTE r, BYTE g, BYTE b, BYTE a) {
  RGBQUAD q = {b,g,r,a};
  return q;
}

RGBQUAD RGBA(DWORD c) {
  RGBQUAD q = {GetBValue(c),GetGValue(c),GetRValue(c),0};
  return q;
}

RGBQUAD transform(RGBQUAD src, double scale) {
  src.rgbRed = BYTE(min(src.rgbRed * scale, 255.0));
  src.rgbGreen = BYTE(min(src.rgbGreen * scale, 255.0));
  src.rgbBlue = BYTE(min(src.rgbBlue * scale, 255.0));
  return src;
}

void gradRect(HDC hDC, int left, int top, int right, int bottom,
              bool horizontal, RGBQUAD c1, RGBQUAD c2) {

  TRIVERTEX vert[2];
  vert [0] .x      = left;
  vert [0] .y      = top;
  vert [0] .Red    = 0xff00 & (DWORD(c1.rgbRed)<<8);
  vert [0] .Green  = 0xff00 & (DWORD(c1.rgbGreen)<<8);
  vert [0] .Blue   = 0xff00 & (DWORD(c1.rgbBlue)<<8);
  vert [0] .Alpha  = 0xff00 & (DWORD(c1.rgbReserved)<<8);

  vert [1] .x      = right;
  vert [1] .y      = bottom;
  vert [1] .Red    = 0xff00 & (DWORD(c2.rgbRed)<<8);
  vert [1] .Green  = 0xff00 & (DWORD(c2.rgbGreen)<<8);
  vert [1] .Blue   = 0xff00 & (DWORD(c2.rgbBlue)<<8);
  vert [1] .Alpha  = 0xff00 & (DWORD(c2.rgbReserved)<<8);

  GRADIENT_RECT gr[1];
  gr[0].UpperLeft=0;
  gr[0].LowerRight=1;

  if (horizontal)
    GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_H);
  else
    GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_V);
}

void Table::initEmpty() {
  if (Data.empty()) {
    addRow(0,0);
    Data.resize(2, TableRow(nTitles,0));
    sortIndex.resize(2);
  }
}

void drawSymbol(gdioutput &gdi, HDC hDC, int height,
                const TextInfo &ti, const wstring &symbol, bool highLight) {
  int cx = ti.xp - gdi.getOffsetX() + ti.xlimit/2;
  int cy = ti.yp - gdi.getOffsetY() + height/2 - 2;
  int h = int(height * 0.4);
  int w = h/3;
  h-=2;

  RGBQUAD a,b;
  if (highLight) {
    a = RGBA(32, 114, 14, 0);
    b = RGBA(64, 211, 44, 0);
  }
  else {
    a = RGBA(32, 114, 114, 0);
    b = RGBA(84, 231, 64, 0);
  }
  gradRect(hDC, cx-w, cy-h, cx+w, cy+h, false, a, b);
  gradRect(hDC, cx-h, cy-w, cx+h, cy+w, false, a, b);

  POINT pt;
  MoveToEx(hDC, cx-w, cy-h, &pt);
  SelectObject(hDC, GetStockObject(DC_PEN));
  SetDCPenColor(hDC, RGB(14, 80, 7));
  LineTo(hDC, cx+w, cy-h);
  LineTo(hDC, cx+w, cy-w);
  LineTo(hDC, cx+h, cy-w);
  LineTo(hDC, cx+h, cy+w);
  LineTo(hDC, cx+w, cy+w);
  LineTo(hDC, cx+w, cy+h);
  LineTo(hDC, cx-w, cy+h);
  LineTo(hDC, cx-w, cy+w);
  LineTo(hDC, cx-h, cy+w);
  LineTo(hDC, cx-h, cy-w);
  LineTo(hDC, cx-w, cy-w);
  LineTo(hDC, cx-w, cy-h);
}


void Table::highlightCell(HDC hDC, gdioutput &gdi, const TableCell &cell, DWORD color, int dx, int dy)
{
  SelectObject(hDC, GetStockObject(DC_BRUSH));
  SelectObject(hDC, GetStockObject(NULL_PEN));

  SetDCBrushColor(hDC, color);

  RECT rc=cell.absPos;
  rc.left+=dx;
  rc.right+=dx;
  rc.top+=dy;
  rc.bottom+=dy;

  Rectangle(hDC, rc.left+1, rc.top,
                 rc.right+2, rc.bottom);

  TextInfo ti;

  if (cell.type == cellAction && cell.contents[0] == '@') {
    ti.xp = rc.left + gdi.OffsetX;
    ti.yp = rc.top + gdi.OffsetY;
    ti.xlimit = rc.right - rc.left;

    drawSymbol(gdi, hDC, rowHeight, ti, cell.contents, true);
    //SetDCPenColor(hDC, RGB(190,190,190));
  }
  else {
    gdi.formatString(ti, hDC);
    SetBkMode(hDC, TRANSPARENT);
    rc.left+=4;
    rc.top+=2;
    DrawText(hDC, cell.contents.c_str(), -1, &rc, DT_LEFT|DT_NOPREFIX);
  }
}

void Table::draw(gdioutput &gdi, HDC hDC, int dx, int dy, const RECT &screen)
{
  gdi.resetLast();
  initEmpty();
  updateDimension(gdi);

  table_xp=dx-gdi.OffsetX;
  table_yp=dy-gdi.OffsetY;

  if (currentSortColumn==-1)
    sort(0);

  int wi, he;
  getDimension(gdi, wi, he, true);

  tableWidth=wi;
  tableHeight=he;

  //Find first and last row
  int yreltop=(screen.top-(dy-gdi.OffsetY)-rowHeight+1)/rowHeight;
  int yrelbottom=(screen.bottom-(dy-gdi.OffsetY)-1)/rowHeight;

  int firstRow=max(yreltop,0);
  int lastRow=min(yrelbottom, int(sortIndex.size()));

  int firstCol=0;
  int lastCol=columns.size();

  xpos.resize(columns.size()+1);

  xpos[0]=dx-gdi.OffsetX;
  for (size_t i=1;i<=columns.size();i++)
    xpos[i]=xpos[i-1]+Titles[columns[i-1]].width+1;

  //Find first and last visible column
  while(firstCol<int(columns.size()) && xpos[firstCol+1]<=screen.left)
    firstCol++;

  while(lastCol>0 && xpos[lastCol-1]>screen.right)
    lastCol--;

  SelectObject(hDC, GetStockObject(DC_BRUSH));
  SelectObject(hDC, GetStockObject(DC_PEN));

  SetDCPenColor(hDC, RGB(190,190,190));
  SetDCBrushColor(hDC, RGB(60,25,150));

  //Title
  TextInfo ti;
  gdi.formatString(ti, hDC);
  HLS hls;
  hls.RGBtoHLS(GetSysColor(COLOR_ACTIVECAPTION));
  hls.lighten(2);
  hls.saturation = min<WORD>(90,hls.saturation);
  DWORD lightColor = hls.HLStoRGB();

  if (firstRow==0 && lastRow>=0) {
    //Rectangle(hDC, dx-gdi.OffsetX, dy-gdi.OffsetY,
    //          dx+tableWidth-gdi.OffsetX, dy+rowHeight-gdi.OffsetY);
    RGBQUAD c1 = RGBA(GetSysColor(COLOR_ACTIVECAPTION));
    gradRect(hDC, dx-gdi.OffsetX, dy-gdi.OffsetY,
            dx+tableWidth-gdi.OffsetX, dy+rowHeight-gdi.OffsetY, false,
            c1, transform(c1, 0.7));

    RECT rc;
    rc.left=dx+5-gdi.OffsetX;
    rc.right=rc.left+gdi.scaleLength(150);
    rc.top=dy+3-gdi.OffsetY;
    rc.bottom=rc.top+rowHeight;
    ti.format = 1;
    gdi.formatString(ti, hDC);
    SetTextColor(hDC, RGB(255,255,255));
    DrawText(hDC, lang.tl(tableName).c_str(), -1, &rc, DT_LEFT|DT_NOPREFIX);
    DrawText(hDC, lang.tl(tableName).c_str(), -1, &rc, DT_CALCRECT|DT_NOPREFIX);
    ti.format = 0;
    gdi.formatString(ti, hDC);
    SetTextColor(hDC, RGB(255,255,255));
    wchar_t bf[256];
    wstring info = lang.tl(wstring(L"sortering: X, antal rader: Y#") + Titles[currentSortColumn].name + L"#" + itow(sortIndex.size()-2));
    //sprintf_s(bf, .c_str(),
    //          Titles[currentSortColumn].name, int(sortIndex.size())-2);
    rc.left=rc.right+30;
    rc.right=dx + tableWidth - gdi.OffsetX - 10;
    DrawText(hDC, info.c_str(), info.length(), &rc, DT_LEFT|DT_NOPREFIX);

    SetTextColor(hDC, RGB(0,0,0));

    if (drawFilterLabel) {
      SetDCBrushColor(hDC, RGB(100,200,100));

      Rectangle(hDC, dx-gdi.OffsetX, dy+2*rowHeight-gdi.OffsetY,
              dx+tableWidth-gdi.OffsetX, dy+3*rowHeight-gdi.OffsetY);

      rc.left=dx+5-gdi.OffsetX;
      rc.right=rc.left+100;
      rc.top=dy+3+2*rowHeight-gdi.OffsetY;
      rc.bottom=rc.top+rowHeight;
      wchar_t tbf[2];
      tbf[0]=Titles[editCol].name[0];
      tbf[1]=0;
      CharLower(tbf);
      wstring filter = lang.tl("Urval %c%s: ");
      swprintf_s(bf, filter.c_str(), tbf[0],
              Titles[editCol].name+1, int(sortIndex.size())-2);

      DrawText(hDC, bf, -1, &rc, DT_RIGHT|DT_NOPREFIX);
    }
  }

  int yp;
  yp=dy+rowHeight;

  if (firstRow<=2 && lastRow>=1) {
    wstring filterText = lang.tl("Urval...");

    for (int k=firstCol;k<lastCol;k++) {
      int xp=xpos[k];
      int index=columns[k];
      RECT rc;
      rc.left=xp;
      rc.right=rc.left+Titles[index].width;
      rc.top=yp+1-gdi.OffsetY;
      rc.bottom=rc.top+rowHeight;
      SetDCBrushColor(hDC, RGB(200,200,200));
      Rectangle(hDC, rc.left, rc.top-1, rc.right+2, rc.bottom);

      if (index == PrevSort || PrevSort == -(10+index) ) {
        SetDCBrushColor(hDC, RGB(100,250,100));
        POINT pt[3];
        int r = gdi.scaleLength(4);
        int s = rc.bottom - rc.top - r;
        int px = (rc.right + rc.left + s)/2;
        int py = rc.top + r/2;

        pt[0].x = px - s;
        pt[1].x = px - s/2;
        pt[2].x = px;
        if (index == PrevSort) {
          pt[0].y = py;
          pt[1].y = py + s;
          pt[2].y = py;
        }
        else {
          pt[0].y = py + s;
          pt[1].y = py;
          pt[2].y = py + s;
        }

        Polygon(hDC, pt, 3);
      }

      Data[0].cells[index].absPos=rc;
      RECT tp = rc;
      tp.top += gdi.scaleLength(1);
      DrawText(hDC, Titles[index].name, -1, &tp, DT_CENTER|DT_NOPREFIX);
      Titles[index].title=rc;

      if (!drawFilterLabel) {
        rc.top+=rowHeight;
        rc.bottom+=rowHeight;
        Titles[index].condition=rc;
        Data[1].cells[index].absPos=rc;
        if (Titles[index].filter.empty()) {
          SetDCBrushColor(hDC, lightColor);
          Rectangle(hDC, rc.left, rc.top-1, rc.right+2, rc.bottom);
          rc.left+=3;
          DrawText(hDC, filterText.c_str(), -1, &rc, DT_LEFT|DT_NOPREFIX);
        }
        else {
          SetDCBrushColor(hDC, RGB(230,100,100));
          Rectangle(hDC, rc.left, rc.top-1, rc.right+2, rc.bottom);
          rc.left+=3;
          DrawText(hDC, Titles[index].filter.c_str(), -1, &rc, DT_LEFT|DT_NOPREFIX);
        }
      }
    }
  }

  SelectObject(hDC, GetStockObject(DC_BRUSH));

  SetDCPenColor(hDC, RGB(190,190, 190));

  RECT desktop, target;
  GetClientRect(GetDesktopWindow(), &desktop);
  GetClientRect(gdi.getHWNDTarget(), &target);
  int marginPixel = (desktop.bottom - desktop.top)  - (target.bottom - target.top);

  int margin = max(5, (marginPixel + rowHeight) / rowHeight);

  const int rStart = max(2, firstRow - margin);
  const int rEnd = min<int>(lastRow + margin, sortIndex.size());
  for (int k1 = rStart; k1 < rEnd; k1++){
    int yp = dy + rowHeight*(k1+1);
    TableRow &tr = Data[sortIndex[k1].index];
    for(size_t k=0;k<columns.size();k++){
      int xp = xpos[k];
      int i = columns[k];
      tr.cells[i].absPos.left = xp;
      tr.cells[i].absPos.right = xp+Titles[i].width;
      tr.cells[i].absPos.top = yp-gdi.OffsetY;
      tr.cells[i].absPos.bottom = yp-gdi.OffsetY+rowHeight;
    }
  }

  for (size_t k1=max(2, firstRow); int(k1)<lastRow; k1++){
    int yp=dy+rowHeight*(k1+1);
    TableRow &tr=Data[sortIndex[k1].index];
    int xp=xpos[0];

    if (k1&1)
      SetDCBrushColor(hDC, RGB(230,230, 240));
    else
      SetDCBrushColor(hDC, RGB(230,230, 250));

    SelectObject(hDC, GetStockObject(NULL_PEN));
    Rectangle(hDC, max(xpos[firstCol], int(screen.left)), yp-gdi.OffsetY,
              min(xpos[lastCol]+1, int(screen.right+2)), yp+rowHeight-gdi.OffsetY);
    SelectObject(hDC, GetStockObject(DC_PEN));
    const int cy=yp+rowHeight-gdi.OffsetY-1;
    MoveToEx(hDC, xp, cy, 0);
    LineTo(hDC, xp+wi, cy);

    for(int k=firstCol;k<lastCol;k++) {
      xp=xpos[k];
      int i=columns[k];
      if (!tr.cells[i].contents.empty()) {
        ti.xp = xp + 4 + gdi.OffsetX;
        ti.yp = yp + 2;
        ti.format = 0;
        ti.xlimit = Titles[i].width - 4;

        if (tr.cells[i].type == cellAction && tr.cells[i].contents[0] == '@') {
          drawSymbol(gdi, hDC, rowHeight, ti, tr.cells[i].contents, false);
          SetDCPenColor(hDC, RGB(190,190,190));
        }
        else {
          if (Titles[i].formatRight) {
            ti.xlimit -= 4;
            ti.format = textRight;
          }
          gdi.RenderString(ti, tr.cells[i].contents, hDC);
        }
      }
    }
  }

  const int miy=dy+rowHeight-gdi.OffsetY;
  const int may=dy+rowHeight*(sortIndex.size()+1)-gdi.OffsetY;

  for (size_t i=firstCol;i<=columns.size();i++) {
    int xp=xpos[i];
    MoveToEx(hDC, xp, miy, 0);
    LineTo(hDC, xp, may);
  }


  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  SelectObject(hDC, GetStockObject(DC_PEN));
  SetDCPenColor(hDC, RGB(64,64,64));
  Rectangle(hDC, dx-gdi.OffsetX, dy-gdi.OffsetY,
            dx+tableWidth-gdi.OffsetX, dy+tableHeight-gdi.OffsetY);
  SetDCPenColor(hDC, RGB(200,200,200));
  Rectangle(hDC, dx-gdi.OffsetX-1, dy-gdi.OffsetY-1,
            dx+tableWidth-gdi.OffsetX+1, dy+tableHeight-gdi.OffsetY+1);
  SetDCPenColor(hDC, RGB(64,64,64));
  Rectangle(hDC, dx-gdi.OffsetX-2, dy-gdi.OffsetY-2,
            dx+tableWidth-gdi.OffsetX+2, dy+tableHeight-gdi.OffsetY+2);
  drawSelection(gdi, hDC, true);
}


TableCell &Table::getCell(int row, int col) const {
  if (size_t(row) >= sortIndex.size())
    throw std::exception("Index out of range");
  const TableRow &tr = Data[sortIndex[row].index];

  if ( size_t(col) >= columns.size())
    throw std::exception("Index out of range");

  col = columns[col];
  return *((TableCell *)&tr.cells[col]);
}

void Table::clearSelectionBitmap(gdioutput *gdi, HDC hDC)
{
  if (gdi)
    restoreSelection(*gdi, hDC);

  if (hdcCompatibleCell) {
    DeleteDC(hdcCompatibleCell);
    hdcCompatibleCell = 0;
  }

  if (hbmStoredCell) {
    DeleteObject(hbmStoredCell);
    hbmStoredCell = 0;
  }
}

void Table::restoreSelection(gdioutput &gdi, HDC hDC) {
  if (partialCell) {
    RECT rc = lastCell;
    rc.left -= gdi.OffsetX;
    rc.right -= gdi.OffsetX;
    rc.top -= gdi.OffsetY;
    rc.bottom -= gdi.OffsetY;
    InvalidateRect(gdi.getHWNDTarget(), &rc, false);
    partialCell = false;
  }
  else if (hdcCompatibleCell) {
    //Restore bitmap
    int cx = lastCell.right - lastCell.left + 1;
    int cy = lastCell.bottom - lastCell.top + 1;
    int x = lastCell.left - 1 - gdi.OffsetX;
    int y = lastCell.top - 1 - gdi.OffsetY;
    BitBlt(hDC, x, y, cx, cy, hdcCompatibleCell, 0, 0, SRCCOPY);
  }
}

void Table::drawSelection(gdioutput &gdi, HDC hDC, bool forceDraw) {
  bool modified = false;
  if (lowerColOld != lowerCol || upperCol != upperColOld ||
      lowerRow != lowerRowOld || upperRow != upperRowOld) {
    modified = true;
    restoreSelection(gdi, hDC);
  }

  if (lowerCol != -1  && upperCol != -1 &&
       lowerRow != -1  && upperRow != -1 &&
       (forceDraw || modified)) {
    TableCell &c1 = Data[lowerRow].cells[lowerCol];
    TableCell &c2 = Data[upperRow].cells[upperCol];
    RECT rc;
    rc.top = min(c1.absPos.top, c2.absPos.top) + gdi.OffsetY;
    rc.left = min(c1.absPos.left, c2.absPos.left) + gdi.OffsetX;
    rc.right = max(c1.absPos.right, c2.absPos.right) + 1 + gdi.OffsetX;
    rc.bottom = max(c1.absPos.bottom, c2.absPos.bottom) + gdi.OffsetY;

    if (modified) {
      int cx=rc.right-rc.left + 1;
      int cy=rc.bottom-rc.top + 1;

      clearSelectionBitmap(&gdi, hDC);

      lastCell = rc;
      int x = lastCell.left - 1 - gdi.OffsetX;
      int y = lastCell.top - 1 - gdi.OffsetY;
      int maxX, maxY;
      gdi.getTargetDimension(maxX, maxY);

      if (x<=0 || y<=0 || (x+cx)>=maxX || (y+cy)>=maxY) {
        partialCell = true;
      }
      else {
        hdcCompatibleCell = CreateCompatibleDC(hDC);
        hbmStoredCell = CreateCompatibleBitmap(hDC, cx, cy);

        // Select the bitmaps into the compatible DC.
        SelectObject(hdcCompatibleCell, hbmStoredCell);
        BitBlt(hdcCompatibleCell, 0, 0, cx, cy, hDC, x, y, SRCCOPY);
        partialCell = false;
      }
    }

    SelectObject(hDC, GetStockObject(NULL_BRUSH));
    SelectObject(hDC, GetStockObject(DC_PEN));
    SetDCPenColor(hDC, RGB(0,0, 128));
    Rectangle(hDC, rc.left - gdi.OffsetX, rc.top - gdi.OffsetY,
                   rc.right - gdi.OffsetX, rc.bottom - gdi.OffsetY);

    lowerColOld = lowerCol;
    upperColOld = upperCol;
    lowerRowOld = lowerRow;
    upperRowOld = upperRow;
  }
}

void Table::scrollToCell(gdioutput &gdi, int row, int col) {
  if (size_t(row) >= Data.size() || size_t(col) >= Data[row].cells.size())
    return;
  const RECT &rc = Data[row].cells[col].absPos;
  int maxX, maxY;
  gdi.getTargetDimension(maxX, maxY);
  int xo = gdi.OffsetX;
  int yo = gdi.OffsetY;
  if (rc.right > maxX) {
    xo = gdi.OffsetX + (rc.right - maxX) + 10;
  }
  else if (rc.left < 0) {
    xo = gdi.OffsetX + rc.left - 10;
  }

  if (rc.bottom > maxY) {
    yo = gdi.OffsetY + (rc.bottom - maxY) + 10;
  }
  else if (rc.top < 0) {
    yo = gdi.OffsetY + rc.top - 10;
  }

  if (xo != gdi.OffsetX || yo != gdi.OffsetY) {
    gdi.setOffset(xo, yo, true);
    //gdi.refreshFast();
    //Sleep(300);
  }
}

void Table::print(gdioutput &gdi, HDC hDC, int dx, int dy)
{
  vector<int> widths(columns.size());
  vector<bool> skip(columns.size(), true);
  int rh = 0;

  for (size_t j=0;j<columns.size();j++) {
    int i=columns[j];
    TextInfo ti;
    ti.format = boldSmall;
    ti.text = Titles[i].name;
    gdi.calcStringSize(ti, hDC);
    widths[j] = max<int>(widths[j], ti.textRect.right-ti.textRect.left);
    rh = max<int>(rh, ti.textRect.bottom-ti.textRect.top);
  }
  const int extra = 10;

  for (size_t j=0;j<columns.size();j++) {
    for (size_t k=2; k<sortIndex.size(); k++) {
      TableRow &tr=Data[sortIndex[k].index];
      const wstring &ct = tr.cells[columns[j]].contents;
      if (!ct.empty()) {
        skip[j] = false;
        TextInfo ti;
        ti.format = fontSmall;
        ti.text = ct;
        gdi.calcStringSize(ti, hDC);
        widths[j] = max<int>(widths[j],
          int(1.1*(ti.textRect.right-ti.textRect.left)+extra));
        rh = max<int>(rh, ti.textRect.bottom-ti.textRect.top);
      }
    }
  }

  rh = int(rh*1.3);
  vector<int> adj_xp(columns.size());
  int w = widths[0];
  for (size_t j=1;j<columns.size();j++)
    if (!skip[j]) {
      adj_xp[j] = adj_xp[j-1] + w +5;
      w = widths[j];
    }
    else
      adj_xp[j] = adj_xp[j-1];

  for (size_t j=0;j<columns.size();j++) {
    int yp=dy+rh;
    int xp=dx+adj_xp[j];
    int i=columns[j];
    if (!skip[j])
      gdi.addString("", yp, xp, boldSmall, Titles[i].name);
  }

  for (size_t k=2; k<sortIndex.size(); k++) {
    TableRow &tr=Data[sortIndex[k].index];
    int yp=dy+rh*k;

    for (size_t j=0;j<columns.size();j++) {
      if (!skip[j]) {
        int xp=dx+adj_xp[j];
        int i=columns[j];

        if (!tr.cells[i].contents.empty())
          gdi.addString("", yp, xp, fontSmall, tr.cells[i].contents, widths[j]);
      }
    }
  }
}

bool Table::upDown(gdioutput &gdi, int direction)
{
  return false;
}

bool Table::tabFocus(gdioutput &gdi, int direction)
{
  if (hEdit) {
    DestroyWindow(hEdit);
    drawFilterLabel=false;
    gdi.refresh();
    return true;
  }

  return false;
}

void Table::setTableText(gdioutput &gdi, int editRow, int editCol, const wstring &bf) {
  if (size_t(editRow) >= Data.size() || size_t(editCol) >= Data[editRow].cells.size())
    throw std::exception("Index out of bounds");

  wstring output;
  TableCell &cell=Data[editRow].cells[editCol];
  if (cell.hasOwner())
    cell.getOwner()->inputData(cell.id, bf, 0, output, false);
  cell.contents = output;
  if (hEdit != 0)
    DestroyWindow(hEdit);
  hEdit=0;
  reloadRow(Data[editRow].id);
  RECT rc;
  getRowRect(editRow, rc);
  InvalidateRect(gdi.getHWNDTarget(), &rc, false);
}

const wstring &Table::getTableText(gdioutput &gdi, int editRow, int editCol) {
  if (size_t(editRow) >= Data.size() || size_t(editCol) >= Data[editRow].cells.size())
    throw std::exception("Index out of bounds");

  string output;
  TableCell &cell=Data[editRow].cells[editCol];

  return cell.contents;
}


bool Table::enter(gdioutput &gdi)
{
  if (hEdit) {
    if (unsigned(editRow)<Data.size() && unsigned(editCol)<Titles.size()) {
      wchar_t bf[1024];
      GetWindowText(hEdit, bf, 1024);

      if (editRow>=2) {
        string cmd;
        if (gdi.getRecorder().recording())
          cmd = "setTableText(" + itos(editRow) + ", " + itos(editCol) + ", \"" + gdi.narrow(bf) + "\");"; 
        setTableText(gdi, editRow, editCol, bf);
        gdi.getRecorder().record(cmd);
        return true;
      }
      else if (editRow==1) {//Filter
        filter(editCol, bf);
        DestroyWindow(hEdit);
        hEdit=0;
        drawFilterLabel = false;
        gdi.refresh();
        return true;
      }
    }
  }
  else if (gdi.hasWidget(tId)) {
    ListBoxInfo lbi;
    gdi.getSelectedItem(tId, lbi);

    if (lbi.isCombo()) {
      if (size_t(selectionRow) < Data.size() && size_t(selectionCol) < Titles.size()) {
        selection(gdi, lbi.text, lbi.data);
      }
    }
  }
  return false;
}

void Table::escape(gdioutput &gdi)
{
  if (hEdit) {
    DestroyWindow(hEdit);
    hEdit = 0;
  }
  gdi.removeWidget(tId);
  drawFilterLabel=false;
  gdi.refresh();
}

bool Table::inputChange(gdioutput &gdi, HWND hdt)
{
  if (hEdit==hdt) {

    if (drawFilterLabel) {
      wchar_t bf[256];
      GetWindowText(hEdit, bf, 256);
      filter(editCol, bf);
      updateDimension(gdi);
      gdi.refresh();
    }

    return true;
  }
  return false;
}

int Table::getColumn(int x, bool limit) const
{
  if (columns.empty() || xpos.empty())
    return -1;

  if (x<xpos[0]) {
    if (!limit)
      return -1;
    else
      return columns[0];
  }

  for(size_t k=0;k<columns.size();k++) {
    if (x<xpos[k+1])
      return columns[k];
  }
  if (!limit)
    return -1;
  else
    return columns.back();
}

int Table::getRow(int y, bool limit) const
{
  int r=(y-table_yp)/rowHeight-1;
  if (limit) {
    if (r<0)
      r=0;
    else if (r>=int(sortIndex.size()))
      r = sortIndex.size() - 1;
  }
  if (r>=0 && r<int(sortIndex.size()))
    return sortIndex[r].index;
  else return -1;
}

void Table::reloadRow(int rowId)
{
  int index;
  if (!idToRow.lookup(rowId, index))
    throw std::exception("Row not found.");

  if (index >= int(Data.size()))
    throw std::exception("Index out of bounds.");

  TableRow &row=Data[index];
  oBase *obj = row.getObject();
  if (obj) {
    TableUpdateInfo tui;
    tui.object = obj;
    tui.id = rowId;
    oe->generateTableData(internalName, *this, tui);
  }
}

vector<Table::ColSelection> Table::getColumns() const
{
  vector<Table::ColSelection> cols(Titles.size());

  // Get selected columns
  for (size_t k=0; k<columns.size(); k++) {
    cols[k].name = Titles[columns[k]].name;
    cols[k].selected = true;
    cols[k].index = columns[k];
  }
  size_t j = columns.size();
  // Get remaining columns
  for (size_t k=0; k<Titles.size(); k++) {
    if (count(columns.begin(), columns.end(), k) == 0) {
      cols[j].name = Titles[k].name;
      cols[j].selected = false;
      cols[j].index = k;
      j++;
    }
  }
  /*
  // Get all columns
  for (size_t k=0; k<cols.size(); k++)
    cols[k].first = Titles[k].name;

  // Mark shown columns as selected
  for (size_t k=0; k<columns.size(); k++)
    cols[columns[k]].second = true;
*/
  return cols;
}

void Table::selectColumns(const std::set<int> &sel)
{
  vector<int> newcols;

  for (size_t k=0; k<columns.size(); k++) {
    if (sel.count(columns[k])==1) {
      newcols.push_back(columns[k]);
    }
  }

  std::set<int>::const_iterator it = sel.begin();

  while(it != sel.end()) {
    if (count(newcols.begin(), newcols.end(), *it)==0)
      newcols.push_back(*it);
    ++it;
  }

  swap(columns, newcols);
  doAutoSelectColumns = false;
}

void Table::resetColumns()
{
  columns.resize(Titles.size());

  for (size_t k=0;k<columns.size();k++)
    columns[k] = k;

  for (size_t k=0;k<nTitles;k++)
    filter(k, L"", false);

  doAutoSelectColumns = false;
}

void Table::update()
{
  for (auto &dd : dataDefiners)
    dd.second->prepare(oe);
  int oldSort = PrevSort;
  Data.clear();
  sortIndex.clear();
  idToRow.clear();

  clearCellSelection(0);

  if (generator == 0) {
    TableUpdateInfo tui;
    oe->generateTableData(internalName, *this, tui);
  }
  else {
    generator(*this, generatorPtr);
  }

  //Refilter all
  for (size_t k=0;k<nTitles;k++)
    filter(k, Titles[k].filter, true);

  PrevSort = -1;
  if (oldSort != -1)
    sort(oldSort);
  commandLock = false; // Reset lock
}

void Table::getExportData(int col1, int col2, int row1, int row2, wstring &html, wstring &txt) const
{
  html = L"<html><table>";
  txt = L"";

  for (size_t k = row1; k<=size_t(row2); k++) {
    if ( k >= sortIndex.size())
      throw std::exception("Index out of range");
    const TableRow &tr = Data[sortIndex[k].index];
    html += L"<tr>";
    for (size_t j = col1; j<= size_t(col2); j++) {
      if ( j >= columns.size())
        throw std::exception("Index out of range");
      int col = columns[j];
      const TableCell &cell = tr.cells[col];
      html += L"<td>" + cell.contents + L"</td>";
      if (j == col1)
        txt += cell.contents;
      else
        txt += L"\t" + cell.contents;
    }
    txt += L"\r\n";
    html += L"</tr>";
  }

  html += L"</table></html>";
}

void Table::getRowRange(int &rowLo, int &rowHi) const {
  int row1 = -1, row2 = -1;
  for (size_t k = 0; k < sortIndex.size(); k++) {
    if (upperRow == sortIndex[k].index)
      row1 = k;
    if (lowerRow == sortIndex[k].index)
      row2 = k;
  }
  rowLo = min(row1, row2);
  rowHi = max(row1, row2);
}

void Table::getColRange(int &colLo, int &colHi) const {
  int col1 = -1, col2 = -1;
  for (size_t k = 0; k < columns.size(); k++) {
    if (upperCol == columns[k])
      col1 = k;
    if (lowerCol == columns[k])
      col2 = k;
  }
  colLo = min(col1, col2);
  colHi = max(col1, col2);
}

void Table::exportClipboard(gdioutput &gdi)
{
  wstring str;// = "<html><table><tr><td>a</td><td>b</td></tr></table></html>";
  wstring txt;

  int col1 = -1, col2 = -1;
  getColRange(col1, col2);
  /*for (size_t k = 0; k < columns.size(); k++) {
    if (upperCol == columns[k])
      col1 = k;
    if (lowerCol == columns[k])
      col2 = k;
  }*/

  int row1 = -1, row2 = -1;
  getRowRange(row1, row2);
  /*for (size_t k = 0; k < sortIndex.size(); k++) {
    if (upperRow == sortIndex[k].index)
      row1 = k;
    if (lowerRow == sortIndex[k].index)
      row2 = k;
  }*/

  if (col1 == -1 || col2 == -1 || row1 == -1 || row2 == -1)
    return;

  getExportData(min(col1, col2), max(col1, col2),
                min(row1, row2), max(row1, row2), str, txt);
  
  string htmlUTF = gdi.toUTF8(str);
  gdi.copyToClipboard(htmlUTF, txt);

  /*if (OpenClipboard(gdi.getHWND()) != false) {

    EmptyClipboard();

    const char *HTML = str.c_str();
    size_t len = str.length() + 1;

    size_t bsize = len*2;
    char *bf = new char[bsize];

    //Convert to UTF-8
    WORD *wide = new WORD[len];
    MultiByteToWideChar(CP_ACP, 0, HTML, -1, LPWSTR(wide), len);//To Wide
    memset(bf, 0, bsize);
    WideCharToMultiByte(CP_UTF8, 0, LPCWSTR(wide), -1, bf, bsize, NULL, NULL);
    delete[] wide;

    len = strlen(bf) + 1;

    const char cbd[]=
      "Version:0.9\n"
      "StartHTML:%08u\n"
      "EndHTML:%08u\n"
      "StartFragment:%08u\n"
      "EndFragment:%08u\n";

    char head[256];
    sprintf_s(head, cbd, 1,0,0,0);

    int offset=strlen(head);
    //Fill header with relevant information
    int ho_start = offset;
    int ho_end = offset + len;
    sprintf_s(head, cbd, offset,offset+len,ho_start,ho_end);


    HANDLE hMem=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, offset+len);
    LPVOID data=GlobalLock(hMem);

    memcpy(LPSTR(data), head, offset);
    memcpy(LPSTR(data)+offset, bf, len);

    GlobalUnlock(hMem);

    // Text format
    HANDLE hMemText = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, txt.length()+1);
    LPVOID dataText=GlobalLock(hMemText);
    memcpy(LPSTR(dataText), txt.c_str() , txt.length()+1);
    GlobalUnlock(hMemText);


    UINT CF_HTML = RegisterClipboardFormat("HTML format");

    SetClipboardData(CF_HTML, hMem);
    SetClipboardData(CF_TEXT, hMemText);

    delete[] bf;
    CloseClipboard();
  }*/
}


void Table::importClipboard(gdioutput &gdi)
{
  if (!canPaste())
    throw std::exception("Operationen stöds ej");

  wstring str;
  if (OpenClipboard(gdi.getHWNDMain()) != false) {
    
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
      LPVOID lptstr = GlobalLock(data);
      if (lptstr) {
        str = wstring(((wchar_t*)lptstr));
        GlobalUnlock(data);
      }
    }
    else {
      data = GetClipboardData(CF_TEXT);
      if (data) {
        LPVOID lptstr = GlobalLock(data);
        if (lptstr) {
          string strn = string(((char*)lptstr));
          str = gdi.recodeToWide(strn);
          GlobalUnlock(data);
        }
      }
    }
    CloseClipboard();
  }
  if (!str.empty()) {
    // Parse raw data
    vector< vector<wstring> > table(1);
    const wchar_t *ptr = str.c_str();
    wstring word;
    while (*ptr) {
      if (*ptr != '\t' && *ptr != '\r' && *ptr != '\n') {
        word.append(ptr, 1);
      }
      else if (*ptr == '\t') {
        table.back().push_back(word);
        word.clear();
      }
      else if (*ptr == '\n') {
        table.back().push_back(word);
        table.push_back(vector<wstring>());
        word.clear();
      }
      ++ptr;
    }
    if (!word.empty())
      table.back().push_back(word);
    else if (table.back().empty())
      table.pop_back();

    if (table.empty())
      return;

    int rowS = 2;
    int colS = 0;

    size_t tw = 0;
    for (size_t k = 0; k<table.size(); k++)
      tw = max(tw, table[k].size());

    if (tw > columns.size())
      throw meosException("Antalet kolumner i urklippet är större än antalet kolumner i tabellen.");

    if (upperRow == -1) {
      if (!gdi.ask(L"Vill du klistra in X nya rader i tabellen?#"+itow(table.size())))
        return;
      rowS = sortIndex.size(); // Add new rows
    }
    else {
      int col1 = -1, col2 = -1;
      getColRange(col1, col2);
      int row1 = -1, row2 = -1;
      getRowRange(row1, row2);

      if ( (row1 + table.size()) > sortIndex.size() )
        throw meosException("Antalet rader i urklipp får inte plats i selektionen.");

      if ( (col1 + tw) > columns.size() )
        throw meosException("Antalet kolumner i urklipp får inte plats i selektionen.");

      bool wrongSize = false;

      if (row1 != row2 && (row2 - row1 + 1) != table.size())
        wrongSize = true;

      if (col1 != col2 && (col2 - col1 +1 ) != tw)
        wrongSize = true;

      if (wrongSize && !gdi.ask(L"Selektionens storlek matchar inte urklippets storlek. Klistra in i alla fall?"))
        return;

      rowS = row1;
      colS = col1;
    }

    bool addedRow = false;

    for (size_t k = 0; k<table.size(); k++) {
      if (table[k].empty())
        continue;
      if ( (rowS + k) > sortIndex.size()) {
        throw std::exception("Index out of range");
      }
      else if ( (rowS + k) == sortIndex.size()) {
         // Add data
         TableUpdateInfo tui;
         tui.doAdd = true;
         oe->generateTableData(internalName, *this, tui);
         addedRow = true;
         //sortIndex.push_back(TableSortIndex(Data.size()-1, ""));
      }

      TableRow &tr = Data[sortIndex[rowS + k].index];
      for (size_t j = 0; j<table[k].size(); j++) {
        if ( (colS + j) >= columns.size())
          throw std::exception("Index out of range");
        int col = columns[colS + j];

        TableCell &cell=tr.cells[col];
        wstring output;

        size_t index = 0;

        if (cell.type==cellSelection || cell.type==cellCombo) {
          vector< pair<wstring, size_t> > out;
          size_t selected = 0;
          if (cell.hasOwner())
            cell.getOwner()->fillInput(cell.id, out, selected);
          index = -1;
          for (size_t i = 0; i<out.size() && index == -1; i++) {
            if (_wcsicmp(out[i].first.c_str(), table[k][j].c_str()) == 0)
              index = out[i].second;
          }
        }
        try {
          if (index != -1) {
            if (cell.hasOwner())
              cell.getOwner()->inputData(cell.id, table[k][j], index, output, false);
            cell.contents = output;
          }
          else if (cell.type == cellCombo) {
            if (cell.hasOwner())
             cell.getOwner()->inputData(cell.id, table[k][j], index, output, false);
            cell.contents = output;
          }
        }
        catch (const meosException &ex) {
          wstring msg(ex.wwhat());
        }
        catch(const std::exception &ex) {
          string msg(ex.what());
        }
      }
    }

    if (addedRow) {
      TableUpdateInfo tui;
      tui.doRefresh = true;
      oe->generateTableData(internalName, *this, tui);

      updateDimension(gdi);
      int dx,dy;
      getDimension(gdi, dx, dy, true);
      gdi.scrollTo(0, dy + table_yp + gdi.OffsetY);
    }
    gdi.refresh();
  }
}

int Table::deleteRows(int row1, int row2)
{
  if (!canDelete())
    throw std::exception("Operationen stöds ej");

  int failed = 0;
  for (size_t k = row1; k<=size_t(row2); k++) {
    if ( k >= sortIndex.size())
      throw std::exception("Index out of range");
    const TableRow &tr = Data[sortIndex[k].index];
    oBase *ob = tr.cells[0].getOwner();
    if (ob) {
      if (ob->canRemove())
        ob->remove();
      else
        failed++;
    }
  }

  clearCellSelection(0);
  clearSelectionBitmap(0,0);
  update();
  return failed;
}

void Table::insertRow(gdioutput &gdi) {
  if (!canInsert())
    throw std::exception("Operationen stöds ej");

  TableUpdateInfo tui;
  tui.doAdd = true;
  tui.doRefresh = true;
  oe->generateTableData(internalName, *this, tui);
  int dx,dy;
  updateDimension(gdi);
  getDimension(gdi, dx, dy, true);
  gdi.scrollTo(0, dy + table_yp + gdi.OffsetY);
}

void Table::autoAdjust(gdioutput &gdi) {
  initEmpty();
  if (Titles.empty())
    return;
  HDC hDC = GetDC(gdi.getHWNDTarget());
  RECT rc = {0,0,0,0};
  TextInfo ti;
  wstring filterText = lang.tl("Urval...");
  wstring filterName = lang.tl("Namn");
  ti.format = 0;
  gdi.formatString(ti, hDC);
  int sum = 0;
  for (size_t k = 0; k<Titles.size(); k++) {
    int w = 20;
    int minlen = 0;
    int diff = Titles[k].isnumeric ? 0 : 4;
    size_t dsize = Data.size();
    int sample = max<size_t>(1, dsize/1973);
    int sameCount = 0;
    for (size_t r = 0; r < dsize; r+=sample) {
      const TableCell &c = Data[r].cells[k];
      if (r==0 && c.contents == filterName)
        w = max(w, 100);

      const wstring &str = r != 1 ? c.contents : filterText;
      int len = str.length();
      if (len == minlen) {
        sameCount++;
        if (sameCount > 40)
          continue;
      }

      if (len > minlen - diff) {
        sameCount = 0;
        if (Titles[k].isnumeric && r>2)
          DrawText(hDC, (str + L"55").c_str(), len, &rc, DT_CALCRECT|DT_NOPREFIX);
        else
          DrawText(hDC, str.c_str(), len, &rc, DT_CALCRECT|DT_NOPREFIX);
        w = max<int>(w, rc.right - rc.left);
        if (r>2)
          minlen = max(len, minlen);
      }
    }
    Titles[k].baseWidth = int( (w + 25)/ gdi.getScale());
  }
  if (columns.empty())
    columns.push_back(0);

  for (size_t k = 0; k<columns.size(); k++) {
     sum += Titles[columns[k]].baseWidth;
  }

  if (sum<300) {
    int dx = (310-sum) / columns.size();
    for (size_t k = 0; k<columns.size(); k++) {
      Titles[columns[k]].baseWidth += dx;
    }
  }
  ReleaseDC(gdi.getHWNDTarget(), hDC);
  updateDimension(gdi);
}

void Table::autoSelectColumns() {
  initEmpty();
  vector<bool> empty(Titles.size(), true);
  int nonEmpty = 0;

  // Filter away empty and all-equal columns
  for (size_t k = 0; k<Titles.size(); k++) {

    if (Data.size() < 3) {
      if (Data.size() > 2 && Data[2].cells[k].type == cellAction)
        nonEmpty++;
      empty[k] = false;
    }
    else {
      if (Data[2].cells[k].type == cellAction) {
        nonEmpty++;
        empty[k] = false;
      }
      else {
        const wstring &first = Data.size() > 3 ?
          Data[2].cells[k].contents : _EmptyWString;

        for (size_t r = 2; r<Data.size(); r++) {
          const wstring &c = Data[r].cells[k].contents;
          if (c != first) {
            nonEmpty++;
            empty[k] = false;
            break;
          }
        }
      }
    }
  }

  if (nonEmpty > 1) {
    columns.clear();
    wstring id = lang.tl("Id");
    wstring mod = lang.tl("Ändrad");
    for (size_t k = 0; k<empty.size(); k++) {
      if (!empty[k] && wcscmp(Titles[k].name, id.c_str()) != 0 && wcscmp(Titles[k].name, mod.c_str()) != 0)
        columns.push_back(k);
    }
  }

  doAutoSelectColumns = true;
}

void Table::getRowRect(int row, RECT &rc) const {
  rc.left = 100000;
  rc.right = -100000;
  if (columns.empty())
    return;
  rc.top = Data[row].cells[columns[0]].absPos.top;
  rc.bottom = Data[row].cells[columns[0]].absPos.bottom;

  for (size_t k = 0; k<columns.size(); k++) {
    rc.left = min(rc.left, Data[row].cells[columns[k]].absPos.left);
    rc.right = max(rc.right, Data[row].cells[columns[k]].absPos.right);
  }
}

void Table::updateDimension(gdioutput &gdi) {
  int dx, dy;
  getDimension(gdi, dx, dy, true);
  gdi.adjustDimension(max(t_xpos + dx + TableXMargin, t_maxX), max(t_ypos + dy + TableYMargin, t_maxY));
}

int Table::getNumDataRows() const {
  return Data.size() - 2;
}

void TableRow::setObject(oBase &obj) {
  ob = &obj;
  for (size_t k = 0; k < cells.size(); k++) {
    if (cells[k].hasOwner())
      cells[k].ownerRef = obj.getReference();
  }
}

void Table::addDataDefiner(const string &key, const oDataDefiner *definer) {
  dataDefiners[key] = definer;
}
