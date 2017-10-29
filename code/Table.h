#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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

#include <vector>
#include <map>
#include <set>
#include "oBase.h"
#include "inthashmap.h"

#define TableXMargin 40
#define TableYMargin 30

enum CellType {cellEdit, cellSelection, cellAction, cellCombo};
enum KeyCommandCode;

class Table;
typedef void (*GENERATETABLEDATA)(Table &table, void *ptr);


struct TableUpdateInfo {
  bool doAdd;
  bool doRefresh;
  oBase *object;
  int id;
  TableUpdateInfo() : doAdd(false), object(0), id(0), doRefresh(false) {}
};


class TableCell
{
  wstring contents;
  RECT absPos;

  DWORD id;
  oBase *owner;
  bool canEdit;
  CellType type;

  friend class TableRow;
  friend class Table;
  friend int tblSelectionCB(gdioutput *gdi, int type, void *data);
};

class TableRow
{
protected:
  wstring key;
  int intKey;

  vector<TableCell> cells;
  int id;

  wstring *SortString;
  int sInt;

  int ypos;
  int height;
  oBase *ob;

public:
  oBase *getObject() const {return ob;}
  void setObject(oBase &obj);
  bool operator<(const TableRow &r){return *SortString<*r.SortString;}
  static bool cmpint(const TableRow &r1, const TableRow &r2) {return r1.sInt<r2.sInt;}

  TableRow(int elem, oBase *object): sInt(0)
  {
    cells.resize(elem);
    SortString=&cells[0].contents;
    ob = object;
    id = -1;
  }

  TableRow(const TableRow &t)
  {
    cells=t.cells;
    SortString=&cells[0].contents;
    ob = t.ob;
    id = t.id;
  }
  friend class Table;
  friend struct TableSortIndex;

};

class gdioutput;
class oDataDefiner;

struct ColInfo
{
  wchar_t name[64];
  mutable int width;
  int baseWidth;
  bool isnumeric;
  int padWidthZeroSort;
  bool formatRight;
  RECT title;
  RECT condition;
  wstring filter;
};

struct TableSortIndex;

class Table
{
protected:
  GENERATETABLEDATA generator;
  void *generatorPtr;

  bool doAutoSelectColumns;

  int t_xpos;
  int t_ypos;
  int t_maxX;
  int t_maxY;

  static int uniqueId;
  int id;
  bool clearOnHide;
  bool commandLock;
  wstring tableName;
  string internalName;
  vector<ColInfo> Titles;
  vector<int> xpos;
  unsigned nTitles;
  int PrevSort;
  mutable int rowHeight;
  int baseRowHeight;
  vector<TableRow> Data;
  size_t dataPointer; // Insertation pointer
  vector<TableSortIndex> sortIndex;
  vector<int> columns;
  inthashmap idToRow;
  //highlight
  int highRow;
  int highCol;

  // Selected columns. For drag/drop and sort
  int colSelected;
  int startX;
  int startY;

  //Edit selection
  int editRow;
  int editCol;

  // Selected rectangle
  int upperRow;
  int lowerRow;
  int upperCol;
  int lowerCol;

  int upperRowOld;
  int lowerRowOld;
  int upperColOld;
  int lowerColOld;

  bool startSelect;

  HWND hEdit;

  //For moving objects
  HDC hdcCompatible;
  HBITMAP hbmStored;
  int lastX;
  int lastY;

  // For cell seletion
  HDC hdcCompatibleCell;
  HBITMAP hbmStoredCell;
  RECT lastCell;
  bool partialCell;

  //static bool filterMatchString(const string &c, const char *filt);
  void highlightCell(HDC hDC, gdioutput &gdi, const TableCell &cell, DWORD color, int dx, int dy);

  void moveCell(HDC hDC, gdioutput &gdi, const TableCell &cell, int dx, int dy);
  void startMoveCell(HDC hDC, const TableCell &cell);
  void stopMoveCell(HDC hDC, const TableCell &cell, int dx, int dy);
  void restoreCell(HDC hDC, const TableCell &cell);

  void moveColumn(int src, int target);

  int tableWidth;
  int tableHeight;
  bool drawFilterLabel;
  int currentSortColumn;

  void initEmpty();

  int getColumn(int x, bool limit = false) const;
  int getRow(int y, bool limit = false) const;

  void redrawCell(gdioutput &gdi, HDC hDC, int c, int r);

  //Draw coordinates
  int table_xp;
  int table_yp;

  oEvent *oe;

  void clearSelectionBitmap(gdioutput *gdi, HDC hDC);
  void restoreSelection(gdioutput &gdi, HDC hDC);

  void drawSelection(gdioutput &gdi, HDC hDC, bool forceDraw);
  TableCell &getCell(int row, int col) const; //Index as displayed
  void scrollToCell(gdioutput &gdi, int row, int col);

  bool destroyEditControl(gdioutput &gdi);

  void getExportData(int col1, int col2, int row1, int row2,
                     wstring &html, wstring &txt) const;

  // Delete rows in selected range. Return number of rows that could not be removed
  int deleteRows(int row1, int row2);

  void getRowRange(int &rowLo, int &rowHi) const;
  void getColRange(int &colLo, int &colHi) const;
  int ownerCounter;
  DWORD tableProp;

  int selectionRow;
  int selectionCol;

  void getRowRect(int row, RECT &rc) const;

  bool compareRow(int indexA, int indexB) const;

  map<string, const oDataDefiner *> dataDefiners;
public:
  void addDataDefiner(const string &key, const oDataDefiner *definer);

  void setTableText(gdioutput &gdi, int editRow, int editCol, const wstring &bf);
  const wstring &getTableText(gdioutput &gdi, int editRow, int editCol);

  int getTableId() const {return id;}
  static void resetTableIds() {uniqueId = 1;}

  void setGenerator(GENERATETABLEDATA gen, void *genPtr) {
    generatorPtr = genPtr;
    generator = gen;
  }

  void clear();
  void setClearOnHide(bool coh) {clearOnHide = coh;}
  int getNumDataRows() const;

  void clearCellSelection(gdioutput *gdi);

  /// Return translated table name
  const wstring& getTableName() const {return tableName;}
  /// Get the internal identifier of the table
  const string& getInternalName() const {return internalName;}

  bool hasAutoSelect() const {return  doAutoSelectColumns;}

  void updateDimension(gdioutput &gdi);
  void selection(gdioutput &gdi, const wstring &text, int data);

  enum {
    CAN_PASTE = 1,
    CAN_INSERT = 2,
    CAN_DELETE = 4,
  };

  bool canPaste() const {return (tableProp & CAN_PASTE) != 0;}
  bool canInsert() const {return (tableProp & CAN_INSERT) != 0;}
  bool canDelete() const {return (tableProp & CAN_DELETE) != 0;}
  void setTableProp(DWORD w) {tableProp = w;}

  void hide(gdioutput &gdi); //Ensure no edit contol is visible
  void addOwnership() {ownerCounter++;}
  void releaseOwnership();

  void autoAdjust(gdioutput &gdi); // Adjust column widths
  void autoSelectColumns();

  void insertRow(gdioutput &gdi); // Insert a new row in the table
  bool deleteSelection(gdioutput &gdi);
  void setPosition(int x, int y, int maxX, int maxY) {t_xpos = x, t_ypos = y; t_maxX = maxX, t_maxY = maxY;}
  void exportClipboard(gdioutput &gdi);
  void importClipboard(gdioutput &gdi);


  bool hasEditControl() {return hEdit!=0;}

  struct ColSelection {
    ColSelection() : selected(false), index(0) {}
    wstring name;
    bool selected;
    int index;
  };

  vector< ColSelection > getColumns() const;
  void selectColumns(const set<int> &sel);

  oEvent *getEvent() const {return oe;}
  void getDimension(gdioutput &gdi, int &dx, int &dy, bool filteredResult) const;
  void draw(gdioutput &gdi, HDC hDC, int dx, int dy,
            const RECT &screen);

  void print(gdioutput &gdi, HDC hDC, int dx, int dy);

  //Returns true if capture is taken
  bool mouseMove(gdioutput &gdi, int x, int y);
  bool mouseLeftDown(gdioutput &gdi, int x, int y);
  bool mouseLeftUp(gdioutput &gdi, int x, int y);
  bool mouseLeftDblClick(gdioutput &gdi, int x, int y);

  bool editCell(gdioutput &gdi, int row, int col);

  bool keyCommand(gdioutput &gdi, KeyCommandCode code);
  void sort(int col);
  void filter(int col, const wstring &filt, bool forceFilter=false);

  int addColumn(const string &Title, int width, bool isnum, bool formatRight = false);
  int addColumnPaddedSort(const string &title, int width, int padding, bool formatRight = false);

  void reserve(size_t siz);

  TableRow *getRowById(int rowId);
  void addRow(int rowId, oBase *object);
  void set(int column, oBase &owner, int id, const wstring &data,
           bool canEdit=true, CellType type=cellEdit);

  //Reload a row from data
  void reloadRow(int rowId);

  bool UpDown(gdioutput &gdi, int direction);
  bool tabFocus(gdioutput &gdi, int direction);
  bool enter(gdioutput &gdi);
  void escape(gdioutput &gdi);
  bool inputChange(gdioutput &gdi, HWND hEdit);
  void resetColumns();
  void update();

  Table(oEvent *oe_, int rowHeight,
        const wstring &name, const string &tname);
  ~Table(void);

  friend struct TableSortIndex;
};

struct TableSortIndex {
  //TableSortIndex(const Table &t) : table(&t) {}
  const static Table *table;
  int index;
  bool operator<(const TableSortIndex &t) const {return table->compareRow(index,t.index);}
  //{return table->Data[index].key < table->Data[t.index].key;}
  //bool operator<=(const TableSortIndex &t) const {return table->Data[index].key <= table->Data[t.index].key;}
};

enum {TID_CLASSNAME, TID_COURSE, TID_NUM, TID_ID, TID_MODIFIED,
TID_RUNNER, TID_CLUB, TID_START, TID_TIME,
TID_FINISH, TID_STATUS, TID_RUNNINGTIME, TID_PLACE,
TID_CARD, TID_TEAM, TID_LEG, TID_CONTROL, TID_CODES, TID_FEE, TID_PAID,
TID_INPUTTIME, TID_INPUTSTATUS, TID_INPUTPOINTS, TID_INPUTPLACE,
TID_NAME, TID_NATIONAL, TID_SEX, TID_YEAR, TID_INDEX, TID_ENTER, TID_STARTNO};
