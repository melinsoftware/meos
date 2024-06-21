#pragma once

#include <cassert>

class TableColSpec {
  int firstCol = -1;
  int numCol = 0;

public:
  TableColSpec() = default;
  TableColSpec(int firstCol, int numCol) : firstCol(firstCol), numCol(numCol) {}
  
  int operator[](int ix) const {
    assert(ix < numCol && ix >= 0);
    return firstCol + ix;
  }

  bool hasColumn(int colIx) const {
    return colIx >= firstCol && colIx < firstCol + numCol;
  }

  int getIndex(int colIx) const {
    return colIx - firstCol;
  }

  int numColumns() const {
    return numCol;
  }

  int nextColumn() const {
    return firstCol + numCol;
  }

  int firstColumn() const {
    return firstCol;
  }
};

enum CellType { cellEdit, cellSelection, cellAction, cellCombo };
enum KeyCommandCode;

class Table;
typedef void (*GENERATETABLEDATA)(Table& table, void* ptr);
