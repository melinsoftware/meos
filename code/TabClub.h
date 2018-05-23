#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2018 Melin Software HB

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

#include "tabbase.h"

class TabClub :
  public TabBase
{
  int clubCB(gdioutput &gdi, int type, void *data);

  wstring firstDate;
  wstring lastDate;
  bool filterAge;
  bool onlyNoFee;
  bool useManualFee;
  int highAge;
  int lowAge;
  int baseFee;
  wstring typeS;

  int firstInvoice;

  int ClubId;

  void readFeeFilter(gdioutput &gdi);

protected:
  void clearCompetitionData();

public:
  void selectClub(gdioutput &gdi,  pClub pc);

  void importAcceptedInvoice(gdioutput &gdi, const wstring &file);

  const char * getTypeStr() const {return "TClubTab";}
  TabType getType() const {return TClubTab;}

  bool loadPage(gdioutput &gdi);
  TabClub(oEvent *oe);
  ~TabClub(void);

  friend int ClubsCB(gdioutput *gdi, int type, void *data);
};
