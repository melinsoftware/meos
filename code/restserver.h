#pragma once

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

/** Class for providing a MeOS REST service */

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <tuple>
#include <random>

class InfoCompetition;
class xmlbuffer;

namespace restbed {
  class Service;
  class Session;
};

class RestServer {
public:
  
  enum class EntryPermissionClass {
    None,
    DirectEntry,
    Any
  };

  enum class EntryPermissionType {
    None,
    InDbExistingClub,
    InDbAny,
    Any
  };

  static vector<pair<wstring, size_t>> getPermissionsPersons();
  static vector<pair<wstring, size_t>> getPermissionsClass();

private:
  struct EventRequest {
    EventRequest() : state(false) {}
    multimap<string, string> parameters;
    string answer;
    vector<uint8_t> image;
    std::atomic_bool state; //false - asked, true - answerd

    bool isCompleted() { 
      return state; 
    }
  };

  EntryPermissionClass  epClass = EntryPermissionClass::None;
  EntryPermissionType epType = EntryPermissionType::None;
  
  std::mutex lock;
  std::atomic_bool hasAnyRequest;
  shared_ptr<std::thread> service;
  shared_ptr<restbed::Service> restService;
  std::condition_variable waitForCompletion;

  deque<shared_ptr<EventRequest>> requests;
  void getData(oEvent &ref, const string &what, const multimap<string, string> &param, string &answer);
  void lookup(oEvent &ref, const string &what, const multimap<string, string> &param, string &answer);
  
  void newEntry(oEvent &ref, const multimap<string, string> &param, string &answer);

  void compute(oEvent &ref);
  void startThread(int port);

  static void getSelection(const string &param, set<int> &sel);

  void handleRequest(const shared_ptr<restbed::Session> &session);
  friend void method_handler(const shared_ptr< restbed::Session > session);

  shared_ptr<EventRequest> addRequest(multimap<string, string> &param);
  shared_ptr<EventRequest> getRequest();

  vector<int> responseTimes;
  
  map<string, vector<uint8_t>> imageCache;

  RestServer();
  static vector< shared_ptr<RestServer> > startedServers;
 
  RestServer(const RestServer &);
  RestServer & operator=(const RestServer &) const;
  
  void computeInternal(oEvent &ref, shared_ptr<RestServer::EventRequest> &rq);

  map<int, pair<oListParam, shared_ptr<oListInfo> > > listCache;

  string root;
  multimap<string, string> rootMap;

  struct InfoServerContainer {
    //static int currentInstanceId;
    int getNextInstanceId();

    const int instanceIncrementor;
    int thisInstanceId = -1;
    int nextInstanceId;
    shared_ptr<InfoCompetition> cmpModel;
    shared_ptr<xmlbuffer> lastData;
    set<int> classes;
    set<int> controls;

    InfoServerContainer(int s, int e) : nextInstanceId(s), instanceIncrementor(e) {}
  };

  shared_ptr<std::default_random_engine> randGen;
  list<InfoServerContainer> isContainers;
  int getNewInstanceId();
  xmlbuffer *getMOPXML(oEvent &oe, int id, int &nextId);

  void difference(oEvent &oe, int id, string &answer);

public:

  ~RestServer();

  void startService(int port);
  void stop();
  
  static shared_ptr<RestServer> construct();
  static void remove(shared_ptr<RestServer> server);
  static void computeRequested(oEvent &ref);

  void setEntryPermission(EntryPermissionClass epClass, EntryPermissionType epType);

  void setRootMap(const string &rootMap);
  const string &getRootMap() const { return root; }

  std::tuple<EntryPermissionClass, EntryPermissionType> getEntryPermission() const {
    return std::make_tuple(epClass, epType);
  }

  struct Statistics {
    int numRequests;
    int averageResponseTime;
    int maxResponseTime;
  };

  void getStatistics(Statistics &s);
};


