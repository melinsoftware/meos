// TimeStamp.h: interface for the TimeStamp class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TIMESTAMP_H__CC16BFC5_ECD9_4D76_AC98_79F802314B65__INCLUDED_)
#define AFX_TIMESTAMP_H__CC16BFC5_ECD9_4D76_AC98_79F802314B65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
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

class TimeStamp {
  unsigned int Time;
  mutable string stampCode;
  mutable int stampCodeTime = 0;
public:
  void setStamp(const string &s);
  const string &getStamp() const;
  const string &getStamp(const string &sqlStampIn) const;

  const wstring getUpdateTime() const;

  wstring getStampString() const;
  string getStampStringN() const;
  int getAge() const;
  unsigned int getModificationTime() const {return Time;}

  void update();
  void update(TimeStamp &ts);
  TimeStamp();
  virtual ~TimeStamp();
};

#endif // !defined(AFX_TIMESTAMP_H__CC16BFC5_ECD9_4D76_AC98_79F802314B65__INCLUDED_)
