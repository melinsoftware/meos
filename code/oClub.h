// oClub.h: interface for the oClub class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OCLUB_H__8B2917E2_6A48_4E7F_82AD_4F8C64167439__INCLUDED_)
#define AFX_OCLUB_H__8B2917E2_6A48_4E7F_82AD_4F8C64167439__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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

#include <map>
#include "xmlparser.h"
#include "oBase.h"
class oEvent;

class oClub;
class oRunner;
typedef oRunner* pRunner;
class oTeam;
typedef oTeam* pTeam;

typedef oClub* pClub;
class oDataInterface;
class oDataConstInterface;
class Table;

class oClub : public oBase
{
protected:

  struct InvoiceLine {
    InvoiceLine() : fee(0), rent(0), paid(0), payMode(0) {}
    vector< pair<int, pair<bool, wstring> > > xposAndString;
    int fee;
    int rent;
    int paid;
    int payMode;
    void addString(int xpos, const wstring &str, bool right = false) {
      xposAndString.push_back(make_pair(xpos, make_pair(right, str)));
    }
  };

  wstring name;
  vector<wstring> altNames;
  wstring tPrettyName;

  static const int dataSize = 768;
  int getDISize() const {return dataSize;}
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  int tNumRunners;
  int tFee;
  int tPaid;

  virtual int getTableId() const;

  bool inputData(int id, const wstring &input, int inputId,
                        wstring &output, bool noUpdate);

  void fillInput(int id, vector< pair<wstring, size_t> > &elements, size_t &selected);

  void exportIOFClub(xmlparser &xml, bool compact) const;

  void exportClubOrId(xmlparser &xml) const;

  // Set name internally, and update pretty name
  void internalSetName(const wstring &n);

  void changeId(int newId);

  struct InvoiceData {
    int yp;
    int xs;
    int adrPos;
    int clsPos;
    bool multiDay;
    int cardPos;
    int feePos;
    int paidPos;
    int resPos;
    int total_fee_amount;
    int total_rent_amount;
    int total_paid_amount;
    map<int, int> paidPerMode;
    int lh;//lineheight
    InvoiceData(int lh_) {
      yp = 0;
      xs = 0;
      adrPos = 0;
      clsPos = 0;
      multiDay = 0;
      cardPos = 0;
      feePos = 0;
      paidPos = 0;
      resPos = 0;
      total_fee_amount = 0;
      total_rent_amount = 0;
      total_paid_amount = 0;
      paidPerMode.clear();
      lh = lh_;
    }
  };

  void addInvoiceLine(gdioutput &gdi, const InvoiceLine &lines, InvoiceData &data) const;

  void addRunnerInvoiceLine(const pRunner r, bool inTeam, 
                            const map<int, wstring> &definedPayModes,
                            const InvoiceData &data, list<InvoiceLine> &lines) const;
  void addTeamInvoiceLine(const pTeam r, 
                          const map<int, wstring> &definedPayModes, 
                          const InvoiceData &data, list<InvoiceLine> &lines) const;

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

public:

  /** Assign invoice numbers to all clubs. */
  static void assignInvoiceNumber(oEvent &oe, bool reset);

  static int getFirstInvoiceNumber(oEvent &oe);

  static void definedPayModes(oEvent &oe, map<int, wstring> &definedPayModes);


  /** Remove all clubs from a competion (and all belong to club relations)*/
  static void clearClubs(oEvent &oe);

  static void buildTableCol(oEvent *oe, Table *t);
  void addTableRow(Table &table) const;

  void remove();
  bool canRemove() const;

  void updateFromDB();

  bool operator<(const oClub &c) const;
  void generateInvoice(gdioutput &gdi, int &toPay, int &hasPaid, 
                       const map<int, wstring> &definedPayModes, 
                       map<int, int> &paidPerMode);

  wstring getInfo() const;
  bool sameClub(const oClub &c);

  const wstring &getName() const {return name;}

  const wstring &getDisplayName() const {return tPrettyName.empty() ?  name : tPrettyName;}

  void setName(const wstring &n);

  void set(const xmlobject &xo);
  bool write(xmlparser &xml);

  bool isVacant() const;
  oClub(oEvent *poe);
  oClub(oEvent *poe, int id);
  virtual ~oClub();

  friend class oAbstractRunner;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class oClass;
  friend class MeosSQL;
};

#endif // !defined(AFX_OCLUB_H__8B2917E2_6A48_4E7F_82AD_4F8C64167439__INCLUDED_)
