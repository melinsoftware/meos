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

#ifndef MEOS_GUI_HANDLER
#define MEOS_GUI_HANDLER

enum GuiEventType {GUI_BUTTON=1, GUI_INPUT=2, GUI_LISTBOX=3,
  GUI_INFOBOX=4, GUI_CLEAR=5, GUI_INPUTCHANGE=6,
  GUI_COMBO, GUI_COMBOCHANGE, GUI_EVENT, GUI_LINK,
  GUI_TIMEOUT, GUI_POSTCLEAR, GUI_FOCUS, GUI_TIMER,
  GUI_LISTBOXSELECT //DBL-click
};

class gdioutput;
class BaseInfo;

class GuiHandler {
public:
  GuiHandler() {}
  virtual ~GuiHandler() = 0 {}
  virtual void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) = 0;
};

#endif
