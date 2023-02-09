/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include "stdafx.h"
#include "Download.h"
#include <Wininet.h>
#include "Localizer.h"
#include "meos_util.h"
#include "progress.h"
#include "meosexception.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <cassert>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>

#include <process.h>

#pragma comment(lib, "IPHLPAPI.lib")
#define INET_ADDRSTRLEN 16

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Download::Download()
{
  hThread = 0;
  doExit = false;
//hProgress = NULL;

  fileno = 0;
  hInternet = NULL;
  hURL = NULL;

  bytesLoaded = 0;
  bytesToLoad = 1024;

  success = false;
}

Download::~Download()
{
  shutDown();
  endDownload();

  if (hInternet)
    InternetCloseHandle(hInternet);
}

void __cdecl SUThread(void *ptr)
{
  Download *dwl=(Download *)ptr;
  dwl->initThread();
}

bool Download::createDownloadThread() {
  doExit=false;
  hThread=_beginthread(SUThread, 0, this);

  if (hThread==-1) {
    hThread=0;
    return false;
  }

  return true;
}

void Download::shutDown()
{
  if (hThread) {
    doExit=true;

    int m=0;
    while(m<100 && hThread) {
      Sleep(0);
      Sleep(10);
      m++;
    }
    //If unsuccessful ending thread, do it violently
    if (hThread) {
      OutputDebugString(L"Terminate thread...\n");
      TerminateThread(HANDLE(hThread), 0);
      CloseHandle(HANDLE(hThread));
    }
    hThread=0;
  }

}

void Download::initThread()
{
  int status = true;
  while(!doExit && status) {
    status = doDownload();
  }
  hThread=0;
}

void Download::initInternet() {
  hInternet = InternetOpen(L"MeOS", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

  if (hInternet==NULL) {
    DWORD ec = GetLastError();
    wstring error = lang.tl(L"Error: X#" + getErrorMessage(ec));
    throw meosException(error);
  }

  DWORD dwTimeOut = 180 * 1000;
  InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeOut, sizeof(DWORD));
  InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTimeOut, sizeof(DWORD));
}

void Download::downloadFile(const wstring &url, const wstring &file, const vector< pair<wstring, wstring> > &headers)
{
  if (hURL || !hInternet)
    throw std::exception("Not inititialized");
  
  success = false;

  wstring hdr;
  wstring row;
  for (size_t k = 0; k<headers.size(); k++) {
    hdr += headers[k].first + L": " + headers[k].second + L"\r\n";
  }
  wstring url2 = url;
  hURL = InternetOpenUrl(hInternet, url2.c_str(), hdr.empty() ? 0 : hdr.c_str(), hdr.length(), INTERNET_FLAG_DONT_CACHE, 0);

  if (!hURL) {
    int err = GetLastError();
    wstring msg2 = getErrorMessage(err);
    DWORD em = 0, blen = 256;
    wchar_t bf2[256];
    InternetGetLastResponseInfo(&em, bf2, &blen);
    wstring msg = L"Failed to connect to: " + url2;
    msg += L" " + msg2;
    if (bf2[0] != 0)
      msg += L" (" + wstring(bf2) + L")";
    throw meosException(msg.c_str());
  }


  DWORD dwContentLen = 0;
  DWORD dwBufLen = sizeof(dwContentLen);
  BOOL vsuccess = HttpQueryInfo(hURL,
                          HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                          (LPVOID)&dwContentLen, &dwBufLen, 0);

  if (vsuccess)
    setBytesToDownload(dwContentLen);
  else
    setBytesToDownload(0);

  DWORD dwStatus = 0;
  dwBufLen = sizeof(dwStatus);
  vsuccess = HttpQueryInfo(hURL, HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,
                          (LPVOID)&dwStatus, &dwBufLen, 0);

  if (vsuccess) {
    if (dwStatus >= 400) {
      char bf[256];
      switch (dwStatus) {
        case HTTP_STATUS_BAD_REQUEST:
          sprintf_s(bf, "HTTP Error 400: The request could not be processed by the server due to invalid syntax.");
          break;
        case HTTP_STATUS_DENIED:
          sprintf_s(bf, "HTTP Error 401: The requested resource requires user authentication.");
          break;
        case HTTP_STATUS_FORBIDDEN:
          sprintf_s(bf, "HTTP Error 403: Åtkomst nekad (access is denied).");
          break;
        case HTTP_STATUS_NOT_FOUND:
          sprintf_s(bf, "HTTP Error 404: Resursen kunde ej hittas (not found).");
          break;
        case HTTP_STATUS_NOT_SUPPORTED:
          sprintf_s(bf, "HTTP Error 501: Förfrågan stöds ej (not supported).");
          break;
        case HTTP_STATUS_SERVER_ERROR:
          sprintf_s(bf, "HTTP Error 500: Internt serverfel (server error).");
          break;
        default:
          sprintf_s(bf, "HTTP Status Error %d", dwStatus);
      }
      throw dwException(bf, dwStatus);
    }
  }

  fileno=_wopen(file.c_str(), O_BINARY|O_CREAT|O_WRONLY|O_TRUNC, S_IREAD|S_IWRITE);

  if (fileno==-1) {
    fileno=0;
    endDownload();
    wchar_t bf[256];
    swprintf_s(bf, L"Error opening '%s' for writing", file.c_str());
    throw meosException(bf);
  }

  bytesLoaded = 0;
  return;
}

void Download::endDownload()
{
  if (hURL && hInternet) {
    InternetCloseHandle(hURL);
    hURL=NULL;
  }

  if (fileno) {
    _close(fileno);
    fileno=0;
  }
}

bool Download::doDownload()
{
  if (hURL && hInternet) {
    char buffer[512];

    DWORD bRead;
    if (InternetReadFile(hURL, buffer, 512, &bRead)) {
      //Success!
      if (bRead==0) {
        //EOF
        success=true;
        endDownload();
      }
      else{
        if (_write(fileno, buffer, bRead) != int(bRead)) {
          endDownload();
          return false;
        }
        bytesLoaded+=bRead;
        return true;
      }
    }
  }
  return false;
}

void Download::setBytesToDownload(DWORD btd)
{
  bytesToLoad = btd;
}

bool Download::isWorking()
{
  return hThread!=0;
}

bool Download::successful()
{
  return success;
}

void Download::postFile(const wstring &url, const wstring &file, const wstring &fileOut,
                        const vector< pair<wstring, wstring> > &headers, ProgressWindow &pw) {
  SetLastError(0);
  DWORD_PTR dw = 0;
  URL_COMPONENTS uc;
  memset(&uc, 0, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host[128];
  wchar_t path[128];
  wchar_t extra[256];
  uc.lpszExtraInfo = extra;
  uc.dwExtraInfoLength = sizeof(extra);
  uc.lpszHostName = host;
  uc.dwHostNameLength = sizeof(host);
  uc.lpszUrlPath = path;
  uc.dwUrlPathLength = sizeof(path);

  InternetCrackUrl(url.c_str(), url.length(), ICU_ESCAPE, &uc);
  int port = INTERNET_DEFAULT_HTTP_PORT;
  bool https = uc.nScheme == INTERNET_SCHEME_HTTPS;
  if (https)
    port = INTERNET_DEFAULT_HTTPS_PORT;
  else if (uc.nPort>0)
    port = uc.nPort;
  HINTERNET hConnect = InternetConnect(hInternet, host, port,
                                       NULL, NULL, INTERNET_SERVICE_HTTP, 0, dw);
  bool vsuccess = false;
  int errorCode = 0;
  try {
    vsuccess = httpSendReqEx(hConnect, https, path, headers, file, fileOut, pw, errorCode);
  }
  catch (std::exception &) {
    InternetCloseHandle(hConnect);
    throw;
  }
  InternetCloseHandle(hConnect);

  if (!vsuccess) {
    if (errorCode != 0)
      errorCode = GetLastError();

    wstring error = errorCode != 0 ? getErrorMessage(errorCode) : L"";
    if (error.empty())
      error = L"Ett okänt fel inträffade.";
    throw meosException(error);
  }
 }

bool Download::httpSendReqEx(HINTERNET hConnect, bool https, const wstring &dest,
                             const vector< pair<wstring, wstring> > &headers,
                             const wstring &upFile, const wstring &outFile, 
                             ProgressWindow &pw, 
                             int &errorCode) const {
  errorCode = 0;
  INTERNET_BUFFERS BufferIn;
  memset(&BufferIn, 0, sizeof(BufferIn));
  BufferIn.dwStructSize = sizeof( INTERNET_BUFFERS );
  TCHAR szAccept[] = L"*/*";
  LPCTSTR AcceptTypes[2] = { 0, 0 };
  AcceptTypes[0] = szAccept;
  DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE |
    INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
    INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
    INTERNET_FLAG_KEEP_CONNECTION;

  if (https)
    flags |= INTERNET_FLAG_SECURE;

  HINTERNET hRequest = HttpOpenRequest (hConnect, L"POST", dest.c_str(), HTTP_VERSION, NULL, AcceptTypes,
                                        flags, 0);

  DWORD dwBytesRead = 0;
  DWORD dwBytesWritten = 0;
  BYTE pBuffer[4*1024]; // Read from file in 4K chunks

  wstring hdr;
  for (size_t k = 0; k<headers.size(); k++) {
    if (!trim(headers[k].second).empty()) {
      hdr += headers[k].first + L": " + headers[k].second + L"\r\n";
    }
  }

  int retry = 5;
  while (retry>0) {

    HANDLE hFile = CreateFile(upFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == HANDLE(-1))
      return false;


    BufferIn.dwBufferTotal = GetFileSize (hFile, NULL);
    BufferIn.dwHeadersLength = hdr.length();
    BufferIn.lpcszHeader = hdr.c_str();

    double totSize = BufferIn.dwBufferTotal;

    if (!HttpSendRequestEx( hRequest, &BufferIn, NULL, 0, 0)) {
      CloseHandle(hFile);
      InternetCloseHandle(hRequest);
      return false;
    }

    DWORD sum = 0;
    do {
      if (!ReadFile (hFile, pBuffer, sizeof(pBuffer), &dwBytesRead, NULL)) {
        errorCode = GetLastError();
        CloseHandle(hFile);
        InternetCloseHandle(hRequest);
        return false;
      }

      if (dwBytesRead > 0) {
        if (!InternetWriteFile(hRequest, pBuffer, dwBytesRead, &dwBytesWritten)) {
          errorCode = GetLastError();
          CloseHandle(hFile);
          InternetCloseHandle(hRequest);
          return false;
        }
      }
      sum += dwBytesWritten;

      try {
        pw.setProgress(int(1000 * double(sum) / totSize));
      }
      catch (std::exception &) {
        CloseHandle(hFile);
        InternetCloseHandle(hRequest);
        throw;
      }
    }
    while (dwBytesRead == sizeof(pBuffer)) ;

    CloseHandle(hFile);

    if (!HttpEndRequest(hRequest, NULL, 0, 0)) {
      DWORD error = GetLastError();
      errorCode = error;
      if (error == ERROR_INTERNET_FORCE_RETRY)
        retry--;      
      else {
        InternetCloseHandle(hRequest);
        if (error == ERROR_INTERNET_TIMEOUT) {
          throw std::exception("Fick inget svar i tid (ERROR_INTERNET_TIMEOUT)");
        }
        else if (error == ERROR_INTERNET_CONNECTION_RESET) {
          throw std::exception("Inget svar (ERROR_INTERNET_CONNECTION_RESET)");
        }
        return false;
      }
    }
    else
      retry = 0; // Done
  }

  DWORD dwStatus = 0;
  DWORD dwBufLen = sizeof(dwStatus);
  int vsuccess = HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,
                                        (LPVOID)&dwStatus, &dwBufLen, 0);

  if (vsuccess) {
    if (dwStatus >= 400) {
      char bf[256];
      switch (dwStatus) {
        case HTTP_STATUS_BAD_REQUEST:
          sprintf_s(bf, "HTTP Error 400: The request could not be processed by the server due to invalid syntax.");
          break;
        case HTTP_STATUS_DENIED:
          sprintf_s(bf, "HTTP Error 401: The requested resource requires user authentication.");
          break;
        case HTTP_STATUS_FORBIDDEN:
          sprintf_s(bf, "HTTP Error 403: Åtkomst nekad (access is denied).");
          break;
        case HTTP_STATUS_NOT_FOUND:
          sprintf_s(bf, "HTTP Error 404: Resursen kunde ej hittas (not found).");
          break;
        case HTTP_STATUS_NOT_SUPPORTED:
          sprintf_s(bf, "HTTP Error 501: Förfrågan stöds ej (not supported).");
          break;
        case HTTP_STATUS_SERVER_ERROR:
          sprintf_s(bf, "HTTP Error 500: Internt serverfel (server error).");
          break;
        default:
          sprintf_s(bf, "HTTP Status Error %d", dwStatus);
      }
      throw dwException(bf, dwStatus);
    }
  }

  int rfileno = _wopen(outFile.c_str(), O_BINARY|O_CREAT|O_WRONLY|O_TRUNC, S_IREAD|S_IWRITE);

  do {
    dwBytesRead=0;
    if (InternetReadFile(hRequest, pBuffer, sizeof(pBuffer)-1, &dwBytesRead)) {
      _write(rfileno, pBuffer, dwBytesRead);
    }
  } while(dwBytesRead>0);

  _close(rfileno);

  InternetCloseHandle(hRequest);
  return true;
}


void ListIpAddresses(vector<string>& ipAddrs)
{
  ipAddrs.clear();
  IP_ADAPTER_ADDRESSES* adapter_addresses(NULL);
  IP_ADAPTER_ADDRESSES* adapter(NULL);
  const int KB = 1024;
  // Start with a 16 KB buffer and resize if needed -
  // multiple attempts in case interfaces change while
  // we are in the middle of querying them.
  DWORD adapter_addresses_buffer_size = 16 * 1024;
  for (int attempts = 0; attempts != 3; ++attempts)
  {
    adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(adapter_addresses_buffer_size);
    assert(adapter_addresses);

    DWORD error = ::GetAdaptersAddresses(
      AF_UNSPEC,
      GAA_FLAG_SKIP_ANYCAST |
      GAA_FLAG_SKIP_MULTICAST |
      GAA_FLAG_SKIP_DNS_SERVER |
      GAA_FLAG_SKIP_FRIENDLY_NAME,
      NULL,
      adapter_addresses,
      &adapter_addresses_buffer_size);

    if (ERROR_SUCCESS == error) {
      // We're done here, people!
      break;
    }
    else if (ERROR_BUFFER_OVERFLOW == error)
    {
      // Try again with the new size
      free(adapter_addresses);
      adapter_addresses = NULL;

      continue;
    }
    else {
      // Unexpected error code - log and throw
      free(adapter_addresses);
      adapter_addresses = NULL;

      return;
    }
  }

  // Iterate through all of the adapters
  for (adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next)
  {
    // Skip loopback adapters
    if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType)
    {
      continue;
    }

    // Parse all IPv4 and IPv6 addresses
    for (
      IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
      NULL != address;
      address = address->Next)
    {
      auto family = address->Address.lpSockaddr->sa_family;
      if (AF_INET == family)
      {
        // IPv4
        SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);

        char str_buffer[INET_ADDRSTRLEN] = { 0 };
        //inet_ntop(AF_INET, &(ipv4->sin_addr), str_buffer, INET_ADDRSTRLEN);
        auto &id = ipv4->sin_addr.S_un.S_un_b;
        if (id.s_b1 == 169 && id.s_b2 == 254)
          continue; // Not usable
        sprintf_s(str_buffer, "%u.%u.%u.%u", id.s_b1, id.s_b2, id.s_b3, id.s_b4);
        ipAddrs.emplace_back(str_buffer);
      }
      else if (AF_INET6 == family)
      {/*
        // IPv6
        SOCKADDR_IN6* ipv6 = reinterpret_cast<SOCKADDR_IN6*>(address->Address.lpSockaddr);

        char str_buffer[INET6_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), str_buffer, INET6_ADDRSTRLEN);

        std::string ipv6_str(str_buffer);

        // Detect and skip non-external addresses
        bool is_link_local(false);
        bool is_special_use(false);

        if (0 == ipv6_str.find("fe"))
        {
          char c = ipv6_str[2];
          if (c == '8' || c == '9' || c == 'a' || c == 'b')
          {
            is_link_local = true;
          }
        }
        else if (0 == ipv6_str.find("2001:0:"))
        {
          is_special_use = true;
        }

        if (!(is_link_local || is_special_use))
        {
          ipAddrs.mIpv6.push_back(ipv6_str);
        }*/
      }
      else
      {
        // Skip all other types of addresses
        continue;
      }
    }
  }

  // Cleanup
  free(adapter_addresses);
  adapter_addresses = NULL;
}
