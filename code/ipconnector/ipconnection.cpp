/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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

#include "pch.h"

#include <vector>
#include <string>
#include <tchar.h>
#include <algorithm>
#include <map>
#include <thread>
#include <chrono>
#include <cassert>
#include <winsock2.h>
#include <atomic>
#include <mutex>

using namespace std::chrono_literals;

#define EXPORT_CONNECTOR

#include "../connector_interface.h"

using namespace std;

EXPORT MConWString __cdecl getName() {
  static const wchar_t *name = L"TCP/IP";
  return name;
}

EXPORT MConWString __cdecl getShortName() {
  static const wchar_t *shortName = L"*tcp";
  return shortName;
}

class IPConnection {
  mutable wstring name;
  mutable wstring statusStr;
  MConAddCard addCardPtr = nullptr;
  MConAddPunch addPunchPtr = nullptr;

  atomic_int tcpPortOpen = 0;
  
  int zeroTime = 0;

  SOCKET server = 0;

  mutex closeMutex;

  
public:
  IPConnection() = default;
  ~IPConnection();

  void init(MConAddCard addCard, MConAddPunch addPunch) {
    addCardPtr = addCard;
    addPunchPtr = addPunch;
  }

  void status(MConDeviceStatus &status) const;

  int openConnection(WORD port, int localZeroTime, MConLogMessage logger);
  void closeConnection();

  void unload() {
    closeConnection();
  }

  const wstring &connectionName() const {
    if (tcpPortOpen)
      name = L"TCP/IP (" + to_wstring(tcpPortOpen) + L")";
    else
      name = L"TCP/IP";

    return name;
  }

  bool connected() const {
    return tcpPortOpen != 0;
  }

  MConWString monitorThread();
};

IPConnection::~IPConnection() {
  if (connected())
    closeConnection();
}

void IPConnection::status(MConDeviceStatus &status) const {
  status.name = connectionName().c_str();
  status.connected = connected();
  status.requireFunctionMapping = true;

  if (!status.connected) {
    statusStr = L"TCP: $ej aktiv.";
  }
  else {
    wchar_t bf[128];
    swprintf_s(bf, L"TCP: $Port$ %d, $Nolltid$: %s", tcpPortOpen.load(), L"00:00:00");
    statusStr = bf;
  }
  
  status.numStatusMessage = 1;
  status.message[0] = statusStr.c_str();
  status.isWarningMessage[0] = false;
}

int IPConnection::openConnection(WORD port, int localZeroTime, MConLogMessage logger) {
  tcpPortOpen = 0;
  //A SOCKET is simply a typedef for an unsigned int.
  //In Unix, socket handles were just about same as file
  //handles which were again unsigned ints.
  //Since this cannot be entirely true under Windows
  //a new data type called SOCKET was defined.

  //WSADATA is a struct that is filled up by the call
  //to WSAStartup
  WSADATA wsaData;

  //The sockaddr_in specifies the address of the socket
  //for TCP/IP sockets. Other protocols use similar structures.
  sockaddr_in local;

  //WSAStartup initializes the program for calling WinSock.
  //The first parameter specifies the highest version of the
  //WinSock specification, the program is allowed to use.
  int wsaret = WSAStartup(0x101, &wsaData);

  //WSAStartup returns zero on success.
  //If it fails we exit.
  if (wsaret != 0) {
    logger(L"Failed to start WSA", true);
    return 0;
  }

  //Now we populate the sockaddr_in structure
  local.sin_family = AF_INET; //Address family
  local.sin_addr.s_addr = INADDR_ANY; //Wild card IP address
  local.sin_port = htons((u_short)port); //port to use

  //the socket function creates our SOCKET
  server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  //If the socket() function fails we exit
  if (server == INVALID_SOCKET) {
    logger(L"Failed to start server", true);
    return 0;
  }

  //bind links the socket we just created with the sockaddr_in
  //structure. Basically it connects the socket with
  //the local address and a specified port.
  //If it returns non-zero quit, as this indicates error

  if (bind(server, (sockaddr *)&local, sizeof(local)) != 0) {
    logger(L"Failed to bind specified port", true);
    closesocket(server);
    return 0;
  }
  //listen instructs the socket to listen for incoming
  //connections from clients. The second arg is the backlog

  if (listen(server, 10) != 0) {
    logger(L"Failed to listen to port", true);
    closesocket(server);
    return 0;
  }

  tcpPortOpen = port;

  return 1;
}

void IPConnection::closeConnection() {
  if (tcpPortOpen) {
    {
      lock_guard<mutex> lock(closeMutex);
      if (server)
        shutdown(server, SD_BOTH);
    }

    Sleep(300);

    {
      lock_guard<mutex> lock(closeMutex);
      if (server)
        closesocket(server);
      tcpPortOpen = 0;
      server = 0;
    }
  }
}

struct SIOnlinePunch {
  BYTE Type;  //0=punch, 255=Triggered time
  WORD CodeNo;  //2 byte 0-65K
  DWORD SICardNo; //4 byte integer  -2GB until +2GB
  DWORD CodeDay; //Obsolete, not used anymore
  DWORD CodeTime;  //Time
};

MConWString IPConnection::monitorThread() {
    //we will need variables to hold the client socket.
  //thus we declare them here.

  struct SIPunch {
    DWORD Code;
    DWORD Time;
  };

  auto assign = [](MConPunch &out, const SIPunch &in) {
    out.time = in.Time * 1000 / 10;
    out.code = in.Code;
  };


  SOCKET client;
  sockaddr_in from;
  int fromlen = sizeof(from);

  SIOnlinePunch op;
  
  while (tcpPortOpen) {
    client = accept(server, (sockaddr *)&from, &fromlen);

    if (client != INVALID_SOCKET) {

      char temp[15];

      int r = 0;
      while (r != -1 && tcpPortOpen) {

        uint64_t timeout = GetTickCount64() + 1000;
        int iter = 0;
        while (r != SOCKET_ERROR && r < 15 && tcpPortOpen) {
          r = recv(client, temp, 15, MSG_PEEK);
          iter++;
          if (iter > 10) {
            if (GetTickCount64() > timeout) {
              break;
            }
            else
              iter = 0;
          }
          Sleep(0);
        }

        r = recv(client, temp, 15, 0);

        if (r == 15) {
          //BYTE c[15]={0, 0x64,0,0x48, 0xa4, 0x07, 00, 0x05, 00, 00, 00, 0x8d, 0x16, 0x06, 0x00};
          //memcpy(temp, c, 15);
          op.Type = temp[0];
          op.CodeNo = *(WORD *)(&temp[1]);
          op.SICardNo = *(DWORD *)(&temp[3]);
          op.CodeDay = *(DWORD *)(&temp[7]);
          op.CodeTime = *(DWORD *)(&temp[11]);

          if (op.Type == 64 && op.CodeNo > 1 && op.CodeNo <= 192 && op.CodeTime == 0) {
            // Recieved card
            int nPunch = op.CodeNo;
            vector<SIPunch> punches(nPunch);
            r = recv(client, (char *)&punches[0], 8 * nPunch, 0);
            if (r == 8 * nPunch) {
              MConCard card;
              card.cardNo = op.SICardNo;
              card.nPunch = nPunch;
              for (int k = 0; k < nPunch; k++) {
                if (punches[k].Code == int(MConPunchType::PunchStart))
                  assign(card.start, punches[k]);
                else if (punches[k].Code == int(MConPunchType::PunchFinish))
                  assign(card.finish, punches[k]);
                else if (punches[k].Code == int(MConPunchType::PunchCheck))
                  assign(card.check, punches[k]);
                else
                  assign(card.punch[k], punches[k]);
              }
              if (addCardPtr)
                addCardPtr(&card);
            }
          }
          else {
            MConFreePunch punch;
            punch.cardNo = op.SICardNo;
            punch.time = (op.CodeTime * 1000) / 10;
            punch.code = op.CodeNo;
            punch.unit = op.CodeNo;
            if (addPunchPtr)
              addPunchPtr(&punch);
          }
        }
        else r = -1;

      }
      //close the client socket
      closesocket(client);
    }
    else {
      int err = WSAGetLastError();

      if (err == WSAESHUTDOWN || err == WSAECONNABORTED) {
        tcpPortOpen = 0;
        {
          lock_guard<mutex> lock(closeMutex);
          if (server)
            closesocket(server);
          server = 0;
        }
        return nullptr;
      }
    }
  }

  //closesocket() closes the socket and releases the socket descriptor
  lock_guard<mutex> lock(closeMutex);
  closesocket(server);
  WSACleanup();
  return nullptr;
}

IPConnection ipcon;

EXPORT int __cdecl enumerateDevices(int maxNumEntries, MConWString *deviceVectorDescr, bool *deviceVectorConnected) { 
  const wstring &con = ipcon.connectionName();
  deviceVectorDescr[0] = con.c_str();
  deviceVectorConnected[0] = ipcon.connected();
  return 1;
}

EXPORT void __cdecl status(int deviceIndex, MConDeviceStatus *status) {
  ipcon.status(*status);
}

EXPORT int __cdecl openConnection(int deviceIndex, bool listenOnly, MConWString parameter, MConLogMessage logger) {
  return ipcon.openConnection(_wtoi(parameter), 0, logger);
}

EXPORT void __cdecl closeConnection(int deviceIndex) {
  ipcon.closeConnection();
}

EXPORT MConWString __cdecl monitorThread(int deviceIndex) {
  return ipcon.monitorThread();
}

EXPORT void __cdecl init(MConDeviceProps *deviceProps, MConWString *deviceHelp,
                         MConWString *startParameterName,
                         MConAddCard addCard, MConAddPunch addPunch) {
  deviceProps->supportAutoDetect = false;
  static auto par = L"Port för TCP|10000|help:14070";
  *startParameterName = par;

  static const wchar_t *help = L"help:tcpsystem";
  *deviceHelp = help;

  ipcon.init(addCard, addPunch);
}

EXPORT void __cdecl unload() {
  ipcon.unload();
}

