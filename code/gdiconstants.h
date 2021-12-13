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

#ifndef GDI_CONSTANTS
#define GDI_CONSTANTS

#include "gdifonts.h"

enum KeyCommandCode {
  KC_NONE,
  KC_COPY,
  KC_PASTE,
  KC_DELETE,
  KC_INSERT,
  KC_PRINT,
  KC_FIND,
  KC_FINDBACK,
  KC_REFRESH,
  KC_SPEEDUP,
  KC_SLOWDOWN,
  KC_AUTOCOMPLETE,
  KC_MARKALL,
  KC_CLEARALL
};

/** Enum used to stack GUI command control, "command line wizard" */
enum FlowOperation {
  FlowContinue,
  FlowCancel,
  FlowAborted
};

#endif
