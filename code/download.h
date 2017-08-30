// Download.h: interface for the Download class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DOWNLOAD_H__DEBC6296_9CAE_4FF6_867B_DD896D0B6A7A__INCLUDED_)
#define AFX_DOWNLOAD_H__DEBC6296_9CAE_4FF6_867B_DD896D0B6A7A__INCLUDED_

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2017 Melin Software HB

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


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <vector>
typedef HANDLE HINTERNET;

class dwException : public std::exception {
public:
  int code;
  dwException(const char *msg, int id) : std::exception(msg), code(id) {}
  virtual ~dwException() {}
};

class ProgressWindow;

class Download {
private:
  volatile unsigned hThread;
  volatile bool doExit;
  //HWND hProgress;

  HINTERNET hInternet;
  HINTERNET hURL;

  int fileno;

  DWORD bytesLoaded;
  DWORD bytesToLoad;
  bool success;
  void initThread();

  bool httpSendReqEx(HINTERNET hConnect, const string &dest, const vector< pair<string, string> > &headers,
                     const string &upFile, const string &outFile, ProgressWindow &pw, int &errroCode) const;

public:

  void postFile(const string &url, const string &file, const string &fileOut,
                const vector< pair<string, string> > &headers, ProgressWindow &pw);
  int processMessages();
  bool successful();
  bool isWorking();
  void setBytesToDownload(DWORD btd);
  void endDownload();
  void downloadFile(const string &url, const string &file, const vector< pair<string, string> > &headers);
  void initInternet();
  void shutDown();
  bool createDownloadThread();
  void downLoadNoThread() {initThread();}

  Download();
  virtual ~Download();

protected:
  bool doDownload();

  friend void SUThread(void *ptr);

};

#endif // !defined(AFX_DOWNLOAD_H__DEBC6296_9CAE_4FF6_867B_DD896D0B6A7A__INCLUDED_)
