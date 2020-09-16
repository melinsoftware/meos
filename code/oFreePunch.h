#pragma once

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

#include "oBase.h"
#include "oPunch.h"
#include "xmlparser.h"

class oFreePunch;
typedef oFreePunch* pFreePunch;
class oRunner;
typedef oRunner *pRunner;
class Table;

class oFreePunch : public oPunch
{
protected:
  int CardNo;
  int iHashType; //Index type used for lookup
  int tRunnerId; // Id of runner the punch is classified to.

  /** Class used to sort punches by time. */
  class FreePunchComp {
    public:
    bool operator()(pFreePunch a, pFreePunch b) {
      return a->Time < b->Time;
    }
  };

  void changedObject();

public:

  static const shared_ptr<Table> &getTable(oEvent *oe);

  // Get control hash (itype) from course controld and race number
  static int getControlHash(int courseControlId, int race);

  // Get controlId or courseControlId from hash (itype)
  static int getControlIdFromHash(int hash, bool courseControlId);

  // Get the id of the control currently tied to this punch
  int getControlId() const override { return getControlIdFromHash(iHashType, false); };

  // Get the id of the course control currently tied to this punch
  int getCourseControlId() const {return getControlIdFromHash(iHashType, true);}

  // Get the id hash
  int getIHashType() const {return iHashType;}


  // Get the runner currently tied to this punch
  pRunner getTiedRunner() const;
  void addTableRow(Table &table) const;
  void fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected) override;
  pair<int, bool> inputData(int id, const wstring &input, int inputId, wstring &output, bool noUpdate) override;

  void remove();
  bool canRemove() const;

  int getCardNo() const {return CardNo;}
  bool setCardNo(int cardNo, bool databaseUpdate = false);
  bool setType(const wstring &t, bool databaseUpdate = false);
  void setTimeInt(int newTime, bool databaseUpdate);

  static void rehashPunches(oEvent &oe, int cardNo, pFreePunch newPunch);
  static bool disableHashing;

  void merge(const oBase &input, const oBase *base) final;

  oFreePunch(oEvent *poe, int card, int time, int type);
  oFreePunch(oEvent *poe, int id);
  virtual ~oFreePunch(void);

  void Set(const xmlobject *xo);
  bool Write(xmlparser &xml);

  friend class oEvent;
  friend class oRunner;
  friend class MeosSQL;
};
