/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

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

constexpr int pageNewPage=100;
constexpr int pagePageInfo=102;
constexpr int pageNewChapter = 103;

constexpr int textRight=256;
constexpr int textCenter=512;
constexpr int timerCanBeNegative=1024;
constexpr int breakLines=2048;
constexpr int fullTimeHMS = 4096;
constexpr int timeWithTenth = 1<<13;
constexpr int timeSeconds = 1<<14;
constexpr int timerIgnoreSign = 1<<15;
constexpr int Capitalize = 1<<16;
constexpr int absolutePosition = 1 << 17;
constexpr int skipBoundingBox = 1 << 18;
constexpr int hiddenText = 1 << 19;
constexpr int textLimitEllipsis = 1 << 20;
constexpr int imageNoUpdatePos = 1 << 21;
constexpr int time24HourClock = 1 << 22;

enum GDICOLOR {
  colorBlack = RGB(0, 0, 0),
  colorRed = RGB(128, 0, 0),
  colorGreen = RGB(0, 128, 0),
  colorDarkGrey = RGB(40, 40, 40),
  colorDarkRed = RGB(64, 0, 0),
  colorGreyBlue = RGB(92, 92, 128),
  colorDarkBlue = RGB(0, 0, 92),
  colorDarkGreen = RGB(0, 64, 0),
  colorYellow = RGB(249, 196, 72),
  colorLightBlue = RGB(235, 238, 254),
  colorLightRed = RGB(253, 219, 235),
  colorLightGreen = RGB(219, 247, 209),
  colorLightYellow = RGB(248, 248, 216),
  colorLightCyan = RGB(220, 249, 245),//RGB(200, 255, 255),
  colorLightMagenta = RGB(240, 207, 244),//RGB(255, 200, 255),
  
  colorMediumRed = RGB(251, 187, 218),
  colorMediumGreen = RGB(189, 240, 170),
  colorMediumYellow = RGB(240, 223, 153),

  colorMediumDarkRed = RGB(240, 120, 120),
  colorWindowBar = -2,
  colorDefault = -1,
  colorTransparent = -3
};

