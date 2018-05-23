/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2012 Melin Software HB

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

#pragma once

enum gdiFonts {
  normalText=0,
  boldText=1,
  boldLarge=2,
  boldHuge=3,
  boldSmall=5,

  italicText = 6,
  italicMediumPlus = 7,
  monoText = 8,

  fontLarge=11,
  fontMedium=12,
  fontSmall=13,
  fontMediumPlus=14,

  italicSmall = 15,
  textImage = 99,
  formatIgnore = 1000,
};

const int pageNewPage=100;
//const int pageReserveHeight=101;
const int pagePageInfo=102;

const int textRight=256;
const int textCenter=512;
const int timerCanBeNegative=1024;
const int breakLines=2048;
const int fullTimeHMS = 4096;
const int timeWithTenth = 1<<13;
const int timeSeconds = 1<<14;
const int timerIgnoreSign = 1<<15;
const int Capitalize = 1<<16;
const int absolutePosition = 1 << 17;

enum GDICOLOR {colorBlack = RGB(0,0,0),
              colorRed = RGB(128,0,0),
              colorGreen = RGB(0,128,0),
              colorDarkGrey = RGB(40,40,40),
              colorDarkRed = RGB(64,0,0),
              colorGreyBlue = RGB(92,92,128),
              colorDarkBlue = RGB(0,0,92),
              colorDarkGreen = RGB(0,64,0),
              colorYellow = RGB(255, 230, 0),
              colorLightBlue = RGB(240,240,255),
              colorLightRed = RGB(255,230,230),
              colorLightGreen = RGB(180, 255, 180),
              colorLightYellow = RGB(255, 255, 200),
              colorLightCyan = RGB(200, 255, 255),
              colorLightMagenta = RGB(255, 200, 255),
              colorMediumRed = RGB(255,200,200),
              colorMediumDarkRed = RGB(240,120,120),
              colorWindowBar = -2,
              colorDefault = -1,
              colorTransparent = -3};

