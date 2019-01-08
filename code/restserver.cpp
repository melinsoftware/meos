/************************************************************************
MeOS - Orienteering Software
Copyright (C) 2009-2018 Melin Software HB

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

#include "oListInfo.h"
#include "TabList.h"
#include "generalresult.h"

using namespace restbed;

vector< shared_ptr<RestServer> > RestServer::startedServers;

shared_ptr<RestServer> RestServer::construct() {
  shared_ptr<RestServer> obj(new RestServer());
  startedServers.push_back(obj);
  return obj;
}

void RestServer::remove(shared_ptr<RestServer> server) {
  //std::remove(startedServers.begin(), startedServers.end(), server);
  for (size_t k = 0; k < startedServers.size(); k++) {
    if (startedServers[k] == server) {
      startedServers.erase(startedServers.begin() + k);
      break;
    }
  }
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

  session->fetch(content_length, [request, answer](const shared_ptr< Session > session, const Bytes & body)
  {
    session->close(restbed::OK, answer->answer, { { "Content-Length", itos(answer->answer.length()) },
                                                  { "Connection", "close" },
                                                  { "Access-Control-Allow-Origin", "*" }  });
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

  try {
    computeInternal(ref, rq);
  }
  catch (meosException &ex) {
    rq->answer = "Error (MeOS): Error: " + ref.gdiBase().toUTF8(lang.tl(ex.wwhat()));
  }
  catch (std::exception &ex) {
    rq->answer = "Error (MeOS): General Error: " + string(ex.what());
  }
  catch (...) {
    rq->answer = "Error (MeOS): Unknown internal error.";
  }

  {
    lock_guard<mutex> lg(lock);
    rq->state = true;
  }
  waitForCompletion.notify_all();
}


void RestServer::computeInternal(oEvent &ref, shared_ptr<RestServer::EventRequest> &rq) {
  if (rq->parameters.empty()) {
    rq->answer = "<html><head>"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset = UTF-8\">"
      "<title>MeOS Information Service</title>"
      "</head>"
      "<body>"
      "<h2>MeOS</h2><p>" + ref.gdiBase().toUTF8(lang.tl(getMeosFullVersion())) + "<p>"
      "<ul>\n";

    vector<oListParam> lists;
    TabList::getPublicLists(ref, lists);
    map<EStdListType, oListInfo> listMap;
    ref.getListTypes(listMap, false);
    for (auto &lp : lists) {
      wstring n = lp.getName();
      if (n.empty()) {
        n = listMap[lp.listCode].getName();
      }
      lp.setName(n);
      
      int keyCand = lp.listCode * 100;
      bool done = false;
      for (int i = 0; i < 100; i++) {
        if (!listCache.count(keyCand + i)) {
          keyCand += i;
          listCache[keyCand].first = lp;
          done = true;
          break;
        }
        else if(listCache[keyCand + i].first == lp) {
          keyCand = keyCand + i;
          done = true;
          break;
        }
      }
      if (!done) {
        listCache[keyCand].first = lp;
        listCache[keyCand].second.reset();
      }

      rq->answer += "<li><a href=\"?html=1&type=" + itos(keyCand) + "\">" + ref.gdiBase().toUTF8(n) + "</a></li>\n";
    }
    //  "<li><a href=\"?html=1&result=1\">Resultat</a></li>"
    //  "<li><a href=\"?html=1&startlist=1\">Startlista</a></li>"
    rq->answer += "</ul>\n";

    HINSTANCE hInst = GetModuleHandle(0);
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(132), RT_HTML);
    HGLOBAL res = LoadResource(hInst, hRes);
    char *html = (char *)LockResource(res);
    int resSize = SizeofResource(hInst, hRes);

    if (html) {
      string htmlS;
      while (*html != 0 && resSize > 0) {
        if (*html == '*')
          htmlS += "&lt;";
        else if (*html == '#')
          htmlS += "&amp;";
        else
          htmlS += *html;
        ++html;
        resSize--;
      }
      
      rq->answer += htmlS;
    }

    rq->answer += "\n</body></html>\n";
  }
  else if (rq->parameters.count("get") > 0) {
    string what = rq->parameters.find("get")->second;
    getData(ref, what, rq->parameters, rq->answer);    
  }
  else if (rq->parameters.count("html") > 0) {
    string stype = rq->parameters.count("type") ? rq->parameters.find("type")->second : _EmptyString;
    int type = atoi(stype.c_str());
    auto res = listCache.find(type);
      
    if (res != listCache.end()) {
      gdioutput gdiPrint("print", ref.gdiBase().getScale());
      gdiPrint.clearPage(false);

      if (!res->second.second) {
        res->second.second = make_shared<oListInfo>();
        ref.generateListInfo(res->second.first, gdiPrint.getLineHeight(), *res->second.second);
      }
      ref.generateList(gdiPrint, true, *res->second.second, false);
      wstring exportFile = getTempFile();
      gdiPrint.writeHTML(exportFile, ref.getName(), 30);
      ifstream fin(exportFile.c_str());
      string rbf;
      while (std::getline(fin, rbf)) {
        rq->answer += rbf;
      }
      removeTempFile(exportFile);
    }
    else {
      rq->answer = "Error (MeOS): Unknown list";
    }
  }
  else {
    rq->answer = "Error (MeOS): Unknown request";
  }
}

void writePerson(const GeneralResult::GeneralResultInfo &res,
                 xmlbuffer &reslist,
                 bool includeTeam,
                 bool includeCourse,
                 vector< pair<string, wstring> > &rProp) {
  int clsId = res.src->getClassId(true);
  rProp.push_back(make_pair("cls", itow(clsId)));
  if (includeCourse) {
    pRunner pr = dynamic_cast<pRunner>(res.src);
    if (pr && pr->getCourse(false))
      rProp.push_back(make_pair("course", itow(pr->getCourse(false)->getId())));
  }
  bool hasTeam = res.src->getTeam() != 0 && 
                    clsId == res.src->getClassId(false) && // No Qualification/Final
                  (clsId == 0 || res.src->getClassRef(false)->getQualificationFinal() == 0); // // No Qualification/Final

  if (includeTeam && hasTeam)
    rProp.push_back(make_pair("team", itow(res.src->getTeam()->getId())));

  if (hasTeam)
    rProp.push_back(make_pair("leg", itow(1+pRunner(res.src)->getLegNumber())));

  rProp.push_back(make_pair("stat", itow(res.status)));

  if (res.score > 0)
    rProp.push_back(make_pair("score", itow(res.score)));

  rProp.push_back(make_pair("st", itow(10 * (res.src->getStartTime() + res.src->getEvent()->getZeroTimeNum()))));

  if (res.src->getClassRef(false) == 0 || !res.src->getClassRef(true)->getNoTiming()) {
    if (res.time > 0)
      rProp.push_back(make_pair("rt", itow(10 * res.time)));
    if (res.place > 0)
      rProp.push_back(make_pair("place", itow(res.place)));
  }

  auto &subTag = reslist.startTag("person", rProp);
  rProp.clear();
  rProp.push_back(make_pair("id", itow(res.src->getId())));
  subTag.write("name", rProp, res.src->getName());
  rProp.clear();

  if (res.src->getClubRef()) {
    rProp.push_back(make_pair("id", itow(res.src->getClubId())));
    subTag.write("org", rProp, res.src->getClub());
    rProp.clear();
  }
  
  subTag.endTag();
}

void RestServer::getData(oEvent &oe, const string &what, const multimap<string, string> &param, string &answer) {
  xmlbuffer out;
  out.setComplete(true);
  bool okRequest = false;
  if (what == "iofresult") {
    wstring exportFile = getTempFile();
    bool useUTC = false;
    set<int> cls;
    if (param.count("class") > 0)
      getSelection(param.find("class")->second, cls);

    oe.exportIOFSplits(oEvent::IOF30, exportFile.c_str(), false, useUTC, cls, -1, false, false, true, false);
    ifstream fin(exportFile.c_str());
    string rbf;
    while (std::getline(fin, rbf)) {
      answer += rbf;
    }
    removeTempFile(exportFile);
    okRequest = true;
  }
  else if (what == "iofstart") {
    wstring exportFile = getTempFile();
    bool useUTC = false;
    set<int> cls;
    if (param.count("class") > 0)
      getSelection(param.find("class")->second, cls);

    oe.exportIOFStartlist(oEvent::IOF30, exportFile.c_str(), useUTC, cls, false, true, false);
    ifstream fin(exportFile.c_str());
    string rbf;
    while (std::getline(fin, rbf)) {
      answer += rbf;
    }
    removeTempFile(exportFile);
    okRequest = true;
  }
  else if (what == "competition") {
    InfoCompetition cmp(0);
    cmp.synchronize(oe);
    cmp.serialize(out, false);
    okRequest = true;
  }
  else if (what == "class") {
    vector<pClass> cls;
    oe.getClasses(cls, true);
    set<int> ctrlW;
    vector<pControl> ctrl;
    oe.getControls(ctrl, true);
    for (size_t k = 0; k < ctrl.size(); k++) {
      if (ctrl[k]->isValidRadio()) {
        vector<int> cc;
        ctrl[k]->getCourseControls(cc);
        ctrlW.insert(cc.begin(), cc.end());
      }
    }
    for (auto c : cls) {
      InfoClass iCls(c->getId());
      iCls.synchronize(false, *c, ctrlW);
      iCls.serialize(out, false);
    }
    okRequest = true;
  }
  else if (what == "organization") {
    vector<pClub> clb;
    oe.getClubs(clb, true);
    for (auto c : clb) {
      InfoOrganization iClb(c->getId());
      iClb.synchronize(*c);
      iClb.serialize(out, false);
    }
    okRequest = true;
  }
  else if (what == "competitor") {
    vector<pRunner> r;
    set<int> selection;
    if (param.count("class") > 0)
      getSelection(param.find("class")->second, selection);

    oe.getRunners(selection.size() == 1 ? *selection.begin() : 0, -1, r, true);

    {
      vector<pRunner> r2;
      r2.reserve(r.size());
      for (pRunner tr : r) {
        pClass cls = tr->getClassRef(true);
        if (cls && cls->getQualificationFinal() && tr->getLegNumber() != 0)
          continue;
        if (selection.empty() || (cls && selection.count(cls->getId())))
          r2.push_back(tr);
      }
      r.swap(r2);
    }

    for (auto c : r) {
      InfoCompetitor iR(c->getId());
      iR.synchronize(false, false, *c);
      iR.serialize(out, false);
    }
    okRequest = true;
  }
  else if (what == "team") {
    vector<pTeam> teams;
    set<int> selection;
    if (param.count("class") > 0)
      getSelection(param.find("class")->second, selection);

    oe.getTeams(selection.size() == 1 ? *selection.begin() : 0, teams, true);

    if (selection.size() > 1) {
      vector<pTeam> r2;
      r2.reserve(teams.size());
      for (pTeam tr : teams) {
        if (selection.count(tr->getClassId(false)))
          r2.push_back(tr);
      }
      teams.swap(r2);
    }

    for (auto c : teams) {
      InfoTeam iT(c->getId());
      iT.synchronize(*c);
      iT.serialize(out, false);
    }
    okRequest = true;
  }
  else if (what == "control") {
    vector<pControl> ctrl;
    oe.getControls(ctrl, true);
    vector< pair<string, wstring> > prop(1);
    prop[0].first = "id";
    for (pControl c : ctrl) {
      int nd = c->getNumberDuplicates();
      for (int k = 0; k < nd; k++) {
        prop[0].second = itow(oControl::getCourseControlIdFromIdIndex(c->getId(), k));
        if (nd > 1)
          out.write("control", prop, c->getName() + L"-" + itow(k+1));
        else
          out.write("control", prop, c->getName());
      }
    }
    okRequest = true;
  }
  else if (what == "result") {
    okRequest = true;
    vector<pRunner> r;
    set<int> selection;
    pClass sampleClass = 0;
    if (param.count("class") > 0) {
      getSelection(param.find("class")->second, selection);
      if (!selection.empty())
        sampleClass = oe.getClass(*selection.begin());
    }

    if (sampleClass == 0) {
      vector<pClass> tt;
      oe.getClasses(tt, false);
      if (!tt.empty())
        sampleClass = tt[0];
    }
      
    string resTag;
    if (param.count("module") > 0)
      resTag = param.find("module")->second;

    pair<int,int> controlId(oPunch::PunchStart, oPunch::PunchFinish);
    bool totalResult = false;
    if (param.count("total")) {
      const string &tot = param.find("total")->second;
      totalResult = atoi(tot.c_str()) > 0 || _stricmp(tot.c_str(), "true") == 0;
    }
    
    if (param.count("to")) {
      const string &cid = param.find("to")->second;
      
      controlId.second = atoi(cid.c_str());
      if (controlId.second == 0)
        controlId.second = oControl::getControlIdByName(oe, cid);

      if (controlId.second == 0)
        throw meosException("Unknown control: " + cid);
    }

    if (param.count("from")) {
      const string &cid = param.find("from")->second;

      controlId.first = atoi(cid.c_str());
      if (controlId.first == 0)
        controlId.first = oControl::getControlIdByName(oe, cid);

      if (controlId.first == 0)
        throw meosException("Unknown control: " + cid);
    }

    oListInfo::ResultType resType = oListInfo::Classwise;
    wstring resTypeStr = L"classindividual";

    ClassType ct = sampleClass ? sampleClass->getClassType() : ClassType::oClassIndividual;
    bool team = ct == oClassPatrol || ct == oClassRelay;
    int limit = 100000;
    if (param.count("limit")) {
      limit = atoi(param.find("limit")->second.c_str());
      if (limit <= 0)
        throw meosException("Invalid limit: " + param.find("limit")->second);
    }

    bool inclRunnersInForest = param.count("allrunners")>0;
    if (param.count("type")) {
      string type = param.find("type")->second;

      if (type == "GlobalIndividual") {
        resType = oListInfo::Global;
        team = false;
      }
      else if (type == "ClassIndividual") {
        resType = oListInfo::Classwise;
        team = false;
      }
      else if (type == "CourseIndividual") {
        resType = oListInfo::Coursewise;
        team = false;
      }
      else if (type == "LegIndividual") {
        resType = oListInfo::Legwise;
        team = false;
      }
      else if (type == "GlobalTeam") {
        resType = oListInfo::Global;
        team = true;
      }
      else if (type == "ClassTeam") {
        resType = oListInfo::Classwise;
        team = true;
      }
      else
        throw meosException("Unknown type: " + type);

      string2Wide(type, resTypeStr);
      resTypeStr = canonizeName(resTypeStr.c_str());
    }
    else if (team) {
      resTypeStr = L"classteam";
    }

    int inputNumber = 0;


    if (param.count("argument")) {
      const string &arg = param.find("argument")->second;
      inputNumber = atoi(arg.c_str());
    }

    vector<GeneralResult::GeneralResultInfo> results;
    vector< pair<string, wstring> > prop, noProp, rProp;
  
    prop.push_back(make_pair("type", resTypeStr));

    int leg = -1;
    if (param.count("leg") > 0) {
      string legs = param.find("leg")->second;
      leg = atoi(legs.c_str())-1;
      if (leg < 0 || leg > 32)
        throw meosException("Invalid leg: " + legs);
    }

    if (ct == oClassRelay) {
      if (leg == -1)
        prop.push_back(make_pair("leg", L"Last"));
      else
        prop.push_back(make_pair("leg", itow(leg + 1)));
    }

    if (!resTag.empty())
      prop.push_back(make_pair("module", oe.gdiBase().widen(resTag) + L"(" + itow(inputNumber) + L")"));

    if (controlId.first != oPunch::PunchStart) {
      pair<int, int> idIx = oControl::getIdIndexFromCourseControlId(controlId.first);
      pControl ctrl = oe.getControl(idIx.first);
      if (ctrl == 0)
        throw meosException("Unknown control: " + itos(idIx.first));

      wstring loc = ctrl->getName();
      if (idIx.second > 0)
        loc += L"-" + itow(idIx.second + 1);

      prop.push_back(make_pair("from", loc));
    }

    if (controlId.second == oPunch::PunchFinish)
      prop.push_back(make_pair("to", L"Finish"));
    else {
      pair<int, int> idIx = oControl::getIdIndexFromCourseControlId(controlId.second);
      pControl ctrl = oe.getControl(idIx.first);
      if (ctrl == 0)
        throw meosException("Unknown control: " + itos(idIx.first));

      wstring loc = ctrl->getName();
      if (idIx.second > 0)
        loc += L"-" + itow(idIx.second+1);

      prop.push_back(make_pair("to", loc));
    }

    if (!team) {
      oe.getRunners(selection.size() == 1 ? *selection.begin() : 0, -1, r, false);

      {
        vector<pRunner> r2;
        r2.reserve(r.size());
        for (pRunner tr : r) {
          pClass cls = tr->getClassRef(true);
          if (cls && cls->getQualificationFinal() && tr->getLegNumber() != 0)
            continue;
          if (selection.empty() || (cls && selection.count(cls->getId())))
            r2.push_back(tr);
        }
        r.swap(r2);
      }

      GeneralResult::calculateIndividualResults(r, controlId, totalResult, inclRunnersInForest, resTag, resType, inputNumber, oe, results);


      if (resType ==  oListInfo::Classwise)
        sort(results.begin(), results.end());
      /*else if (resType == oListInfo::Coursewise) {
        sort(results.begin(), results.end(),
           [](const GeneralResult::GeneralResultInfo &a, const GeneralResult::GeneralResultInfo &b)-> 
         bool {
           pCourse ac = dynamic_cast<const oRunner &>(*a.src).getCourse(false);
           pCourse bc = dynamic_cast<const oRunner &>(*b.src).getCourse(false);
           if (ac != bc)
             return ac->getId() < bc->getId();

           return a.compareResult(b);
         }
      );
      }*/
      auto &reslist = out.startTag("results", prop);

      int place = -1;
      int cClass = -1;
      int counter = 0;
      for (const auto &res : results) {
        if (res.src->getClassId(true) != cClass) {
          counter = 0;
          place = 1;
          cClass = res.src->getClassId(true);
        }
        if (++counter > limit && (place != res.place || res.status != StatusOK))
          continue;
        place = res.place;
        writePerson(res, reslist, true, resType == oListInfo::Coursewise, rProp);
      }
      reslist.endTag();
    }
    else {
      //Teams 
      vector<pTeam> teams;
      oe.getTeams(selection.size() == 1 ? *selection.begin() : 0, teams, true);

      if (selection.size() > 1) {
        vector<pTeam> r2;
        r2.reserve(teams.size());
        for (pTeam tr : teams) {
          if (selection.count(tr->getClassId(true)))
            r2.push_back(tr);
        }
        teams.swap(r2);
      }
      auto context = GeneralResult::calculateTeamResults(teams, leg, controlId, totalResult, resTag, resType, inputNumber, oe, results);

      sort(results.begin(), results.end());
      auto &reslist = out.startTag("results", prop);

      int place = -1;
      int cClass = -1;
      int counter = 0;
      for (const auto &res : results) {
        pTeam team = pTeam(res.src);
        if (leg >= team->getNumRunners())
          continue;

        if (res.src->getClassId(true) != cClass) {
          counter = 0;
          place = 1;
          cClass = res.src->getClassId(true);
        }
        if (++counter > limit && (place != res.place || res.status != StatusOK))
          continue;
        place = res.place;

        rProp.push_back(make_pair("cls", itow(res.src->getClassId(true))));
        rProp.push_back(make_pair("stat", itow(res.status)));
        if (res.score > 0)
          rProp.push_back(make_pair("score", itow(res.score)));

        if (res.src->getStartTime() > 0)
          rProp.push_back(make_pair("st", itow(10 * (res.src->getStartTime() + oe.getZeroTimeNum()))));

        if (res.src->getClassRef(false) == 0 || !res.src->getClassRef(true)->getNoTiming()) {
          if (res.time > 0)
            rProp.push_back(make_pair("rt", itow(10 * res.time)));
          if (res.place > 0)
            rProp.push_back(make_pair("place", itow(res.place)));
        }

        auto &subTag = reslist.startTag("team", rProp);
        rProp.clear();
        rProp.push_back(make_pair("id", itow(res.src->getId())));
        subTag.write("name", rProp, res.src->getName());
        rProp.clear();

        if (res.src->getClubRef()) {
          rProp.push_back(make_pair("id", itow(res.src->getClubId())));
          subTag.write("org", rProp, res.src->getClub());
          rProp.clear();
        }
        
        int subRes = res.getNumSubresult(*context);
        GeneralResult::GeneralResultInfo out;

        for (int k = 0; k < subRes; k++) {
          if (res.getSubResult(*context, k, out)) {
            writePerson(out, subTag, false, false, rProp);
          }
        }
        
        subTag.endTag();
      }
      reslist.endTag();

    }
  }
  else if (what == "status") {
    InfoMeosStatus iStatus;
    if (oe.empty()) {
      iStatus.setEventNameId(L"");	// no event
      iStatus.setOnDatabase(false);
    }
    else {
      iStatus.setEventNameId(oe.getNameId(0));	// id of event
      iStatus.setOnDatabase(oe.isClient());	// onDatabase
    }

    iStatus.serialize(out, false);
    okRequest = true;
  }
  if (out.size() > 0) {
    xmlparser mem;
    mem.openMemoryOutput(false);
    mem.startTag("MOPComplete", "xmlns", "http://www.melin.nu/mop");
    out.commit(mem, 100000);
    mem.endTag();
    mem.getMemoryOutput(answer);
  }
  else if (!okRequest) {
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
