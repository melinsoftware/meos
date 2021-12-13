#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2021 Melin Software HB

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

class TabControl :
  public TabBase
{
  int controlCB(gdioutput &gdi, int type, void *data);

  bool tableMode;
  int controlId;
  void save(gdioutput &gdi);


protected:
  void clearCompetitionData();

public:
  void visitorTable(Table &table) const;
  void courseTable(Table &table) const;
  void selectControl(gdioutput &gdi,  pControl pc);
  
  const char * getTypeStr() const {return "TControlTab";}
  TabType getType() const {return TControlTab;}

  bool loadPage(gdioutput &gdi);
  TabControl(oEvent *oe);
  ~TabControl(void);

  friend int ControlsCB(gdioutput *gdi, int type, void *data);
};
