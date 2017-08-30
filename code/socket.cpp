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


#include "stdafx.h"
#include <fstream>
#include <process.h>
#include "socket.h"
#include "meosexception.h"
#include <iostream>

//#define MEOS_DIRECT_PORT 21338


DirectSocket::DirectSocket(int cmpId, int p) {
  competitionId = cmpId;
  port = p;
  InitializeCriticalSection(&syncObj);
  shutDown = false;
  sendSocket = -1;
  hDestinationWindow = 0;
  clearQueue = false;
}

DirectSocket::~DirectSocket() {
  EnterCriticalSection(&syncObj);
  shutDown = true;
  LeaveCriticalSection(&syncObj);

  if (sendSocket != -1) {
    closesocket(sendSocket);
    sendSocket = -1;
  }

  Sleep(1000);
  DeleteCriticalSection(&syncObj);
  shutDown = true;
}

void DirectSocket::addPunchInfo(const SocketPunchInfo &pi) {
  //OutputDebugString("Enter punch in queue\n");
  EnterCriticalSection(&syncObj);
  if (clearQueue)
    messageQueue.clear();
  clearQueue = false;
  messageQueue.push_back(pi);
  LeaveCriticalSection(&syncObj);
  PostMessage(hDestinationWindow, WM_USER + 3, 0,0);
}

void DirectSocket::getPunchQueue(vector<SocketPunchInfo> &pq) {
  pq.clear();

  EnterCriticalSection(&syncObj);
  if (!clearQueue)
    pq.insert(pq.begin(), messageQueue.begin(), messageQueue.end());

  clearQueue = true;
  LeaveCriticalSection(&syncObj);
  return;
}

void DirectSocket::listenDirectSocket() {

  SOCKET clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (clientSocket == -1) {
    throw meosException("Socket error");
  }

  SOCKADDR_IN UDPserveraddr;
  memset(&UDPserveraddr,0, sizeof(UDPserveraddr));
  UDPserveraddr.sin_family = AF_INET;
  UDPserveraddr.sin_port = htons(port);
  UDPserveraddr.sin_addr.s_addr = INADDR_ANY;

  if (bind(clientSocket, (SOCKADDR*)&UDPserveraddr,sizeof(SOCKADDR_IN)) < 0) {
    throw meosException("Socket error");
  }

  fd_set fds;
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 200000;

  while (!shutDown) {
    FD_ZERO(&fds);
    FD_SET(clientSocket, &fds);

    int rc = select(0, &fds, NULL, NULL, &timeout);

    if (shutDown) {
      closesocket(clientSocket);
      return;
    }

    if (rc > 0) {
      ExtPunchInfo pi;
      SOCKADDR_IN clientaddr;
      int len = sizeof(clientaddr);
      if (recvfrom(clientSocket, (char*)&pi, sizeof(pi), 0, (sockaddr*)&clientaddr, &len) > 0) {
        if (pi.cmpId == competitionId)
          addPunchInfo(pi.punch);
      }
    }
  }
  closesocket(clientSocket);
}

void startListeningDirectSocket(void *p) {
  wstring error;
  try {
    ((DirectSocket*)p)->listenDirectSocket();
  }
  catch (const meosException &ex) {
    error = ex.wwhat();
  }
  catch (std::exception &ex) {
    string ne = ex.what();
    error.insert(error.begin(), ne.begin(), ne.end());
  }
  catch (...) {
    error = L"Unknown error";
  }
  if (!error.empty()) {
    error = L"Setting up advance information service for punches failed. Punches will be recieved with some seconds delay. Is the network port blocked by an other MeOS session?\n\n" + error;
    MessageBox(NULL, error.c_str(), L"MeOS", MB_OK|MB_ICONSTOP);
  }
}

void DirectSocket::startUDPSocketThread(HWND targetWindow) {
  hDestinationWindow = targetWindow;
  _beginthread(startListeningDirectSocket, 0, this);
}

void DirectSocket::sendPunch(SocketPunchInfo &pi) {

  if (sendSocket == -1) {
    WORD w = MAKEWORD(1,1);
    WSADATA wsadata;
    WSAStartup(w, &wsadata);

    sendSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSocket == -1) {
      throw meosException("Socket error");
    }
    char opt = 1;
    setsockopt(sendSocket, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(char));
  }

  SOCKADDR_IN brdcastaddr;
  memset(&brdcastaddr,0, sizeof(brdcastaddr));
  brdcastaddr.sin_family = AF_INET;
  brdcastaddr.sin_port = htons(port);
  brdcastaddr.sin_addr.s_addr = INADDR_BROADCAST;

  int len = sizeof(brdcastaddr);

  ExtPunchInfo epi;
  epi.cmpId = competitionId;
  epi.punch = pi;

  int ret = sendto(sendSocket, (char*)&epi, sizeof(epi), 0, (sockaddr*)&brdcastaddr, len);

  if (ret < 0) {
    OutputDebugStringA("Error broadcasting to the clients");
  }
}
