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
#include "oEvent.h"
#include "xmlparser.h"
#include <thread>

#include "restbed/restbed"
#include "meosexception.h"
#include "restserver.h"
#include "infoserver.h"
#include <chrono>

using namespace restbed;

vector< shared_ptr<RestServer> > RestServer::startedServers;


shared_ptr<RestServer> RestServer::construct() {
  shared_ptr<RestServer> obj(new RestServer());
  startedServers.push_back(obj);
  return obj;
}

void RestServer::remove(shared_ptr<RestServer> server) {
// xxx  std::remove(startedServers.begin(), startedServers.end(), server);
}

RestServer::RestServer() : hasAnyRequest(false) {
}

RestServer::~RestServer() {
  stop();
}

class MeOSResource : public restbed::Resource {
  RestServer *server;

public:
  MeOSResource(RestServer *server) : server(server) {}
  
  ~MeOSResource() {
  }

  RestServer &getServer() const { return *server; }
};

static void method_handler(const shared_ptr< restbed::Session > session) {
  RestServer &server = dynamic_cast<const MeOSResource &>(*session->get_resource()).getServer();
  server.handleRequest(session);
}

void RestServer::handleRequest(const shared_ptr<restbed::Session> &session) {
  const auto request = session->get_request();
  size_t content_length = request->get_header("Content-Length", 0);

  chrono::time_point<chrono::system_clock> start, end;
  start = chrono::system_clock::now();
  
  auto param = request->get_query_parameters();
  auto answer = RestServer::addRequest(param);
  {
    unique_lock<mutex> mlock(lock);
    if (!waitForCompletion.wait_for(mlock, 10s, [answer] {return answer->isCompleted(); })) {
      answer->answer = "Error (MeOS): Internal timeout";
    }

    end = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    responseTimes.push_back(int(1000 * elapsed_seconds.count()));
  }
 // lock.unlock();

  session->fetch(content_length, [request, answer](const shared_ptr< Session > session, const Bytes & body)
  {
    //fprintf(stdout, "%.*s\n", (int)body.size(), body.data());
    /*while (!answer->state) {
      std::this_thread::yield();
    }*/
    session->close(restbed::OK, answer->answer, { { "Content-Length", itos(answer->answer.length()) },{ "Connection", "close" } });
  });
}

void RestServer::startThread(int port) {
  auto settings = make_shared<Settings>();
  settings->set_port(port);
  auto resource = make_shared<MeOSResource>(this);
  resource->set_path("/meos");
  
  resource->set_method_handler("GET", method_handler);
  restService->publish(resource);
  
  restService->start(settings);
}

void RestServer::startService(int port) {
  if (service)
    throw meosException("Server started");
  
  restService.reset(new restbed::Service());
  service = make_shared<thread>(&RestServer::startThread, this, port);
}

void RestServer::stop() {
  if (restService)
    restService->stop();
   
  if (service && service->joinable())
    service->join();

  restService.reset();
  service.reset();
}

void RestServer::computeRequested(oEvent &ref) {
  for (auto &server : startedServers) {
    server->compute(ref);
  }
}

void RestServer::compute(oEvent &ref) {
  auto rq = getRequest();
  if (!rq)
    return;
  if (rq->parameters.empty()) {
    rq->answer = "<html><head>"
      "<title>MeOS Information Service</title>"
      "</head>"
      "<body>"
      "<h2>MeOS</h2>"
      "</body>"
      "</html>";
  }
  else if (rq->parameters.count("get") > 0) {
    string what = rq->parameters.find("get")->second;
    getData(ref, what, rq->parameters, rq->answer);    
  }
  else {
    rq->answer = "Error (MeOS): Unknown request";
  }

  {
    lock_guard<mutex> lg(lock);
    rq->state = true;
  }
  waitForCompletion.notify_all();
}

void RestServer::getData(oEvent &oe, const string &what, const multimap<string, string> &param, string &answer) {
  xmlbuffer out;
  out.setComplete(true);

  if (what == "competition") {
    InfoCompetition cmp(0);
    cmp.synchronize(oe);
    cmp.serialize(out, false);
  }
  else if (what == "class") {
    vector<pClass> cls;
    oe.getClasses(cls, true);
    
    for (auto c : cls) {
      InfoClass iCls(c->getId());
      set<int> ctrl;
      iCls.synchronize(*c, ctrl);
      iCls.serialize(out, false);
    }
  }
  else if (what == "competitor") {
    vector<pRunner> r;
    set<int> selection;
    if (param.count("id") > 0)
      getSelection(param.find("id")->second, selection);

    oe.getRunners(selection.size() == 1 ? *selection.begin() : 0, -1, r, true);

    for (auto c : r) {
      InfoCompetitor iR(c->getId());
      iR.synchronize(false, *c);
      iR.serialize(out, false);
    }
  }
  if (out.size() > 0) {
    xmlparser mem;
    mem.openMemoryOutput(false);
    mem.startTag("MOPComplete", "xmlns", "http://www.melin.nu/mop");
    out.commit(mem, 100000);
    mem.endTag();
    mem.getMemoryOutput(answer);
  }
  else {
    answer = "Error (MeOS): Unknown command '" + what + "'";
  }
}

void RestServer::getSelection(const string &param, set<int> &sel) {
  vector<string> sw;
  split(param, ";,", sw);
  for (auto &s : sw) {
    int id = atoi(s.c_str());
    if (id > 0)
      sel.insert(id);
  }
}

shared_ptr<RestServer::EventRequest> RestServer::getRequest() {
  shared_ptr<EventRequest> res;
  if (hasAnyRequest) {
    lock_guard<mutex> lg(lock);
    if (!requests.empty()) {
      res = requests.front();
      requests.pop_front();
    }
    if (requests.empty())
      hasAnyRequest = false;
  }
  return res;
}

shared_ptr<RestServer::EventRequest> RestServer::addRequest(multimap<string, string> &param) {
  auto rq = make_shared<EventRequest>();
  rq->parameters.swap(param);
  lock_guard<mutex> lg(lock);
  requests.push_back(rq);
  hasAnyRequest = true;
  return rq;
}

void RestServer::getStatistics(Statistics &s) {
  lock_guard<mutex> lg(lock);
  s.numRequests = responseTimes.size();
  s.maxResponseTime = 0;
  s.averageResponseTime = 0;
  for (int t : responseTimes) {
    s.maxResponseTime = max(s.maxResponseTime, t);
    s.averageResponseTime += t;
  }
  if (s.numRequests > 0) {
    s.averageResponseTime /= s.numRequests;
  }
}
