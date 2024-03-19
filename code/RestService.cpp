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

#include "stdafx.h"
#include "RestService.h"
#include "meos_util.h"
#include "restserver.h"
#include "meosexception.h"

#include <ShellAPI.h>

int AutomaticCB(gdioutput *gdi, GuiEventType type, BaseInfo* data);

RestService::RestService() : AutoMachine("Informationsserver", Machines::mInfoService), port(-1) {
}

RestService::~RestService() {
  if (server) {
    server->stop();
    RestServer::remove(server);
  }
}

void RestService::save(oEvent &oe, gdioutput &gdi, bool doProcess) {
  AutoMachine::save(oe, gdi, doProcess);

  if (!server) {
    server = RestServer::construct();

    int xport = gdi.getTextNo("Port");
    if (xport > 0 && xport < 65536) {
      oe.setProperty("ServicePort", xport);
      
      port = xport;
      if (doProcess)
        server->startService(port);
    }
    else
      throw meosException("Invalid port number");
  }

  if (gdi.isChecked("MapRoot")) {
    rootMap = gdi.recodeToNarrow(gdi.getText("RootMap"));
    server->setRootMap(rootMap);
    oe.setProperty("ServiceRootMap", gdi.getText("RootMap"));
  }

  if (gdi.isChecked("AllowEntry")) {
    RestServer::EntryPermissionType pt = (RestServer::EntryPermissionType)gdi.getSelectedItem("PermissionPerson").first;
    RestServer::EntryPermissionClass pc = (RestServer::EntryPermissionClass)gdi.getSelectedItem("PermissionClass").first;
    server->setEntryPermission(pc, pt);
  }
  else {
    server->setEntryPermission(RestServer::EntryPermissionClass::None,
                               RestServer::EntryPermissionType::None);
  }
}

void ListIpAddresses(vector<string>& ip);

void RestService::settings(gdioutput &gdi, oEvent &oe, State state) {
  if (port == -1) {
    port = oe.getPropertyInt("ServicePort", 2009);    
    rootMap = oe.getPropertyString("ServiceRootMap", "");
  }
  settingsTitle(gdi, "MeOS Informationsserver REST-API");

  //gdi.fillRight();
  gdi.pushX();
  gdi.addCheckbox("AllowEntry", "Tillåt anmälan", 0, false).setHandler(this);
  gdi.addSelection("PermissionPerson", 180, 200, 0, L"Vem får anmäla sig:");
  gdi.setItems("PermissionPerson", RestServer::getPermissionsPersons());
  gdi.autoGrow("PermissionPerson");
  gdi.selectFirstItem("PermissionPerson");
  gdi.fillDown();
  gdi.addSelection("PermissionClass", 180, 200, 0, L"Till vilka klasser:");
  gdi.setItems("PermissionClass", RestServer::getPermissionsClass());
  gdi.autoGrow("PermissionClass");
  gdi.selectFirstItem("PermissionClass");
  bool disablePermisson = true;
  gdi.popX();

  gdi.addCheckbox("MapRoot", "Mappa rootadressen (http:///localhost:port/) till funktion:", nullptr, !rootMap.empty()).setHandler(this);
  gdi.addInput("RootMap", gdi.recodeToWide(rootMap));
  gdi.setInputStatus("RootMap", !rootMap.empty());

  startCancelInterval(gdi, "Save", state, IntervalNone, L"");
  
  if (!server) {
    gdi.addInput("Port", itow(port), 10, 0, L"Port:", L"#http://localhost:[PORT]/meos");
  }
  else {
    gdi.addString("", 0, "Server startad på X#" + itos(port));
    auto per = server->getEntryPermission();
    if (get<RestServer::EntryPermissionType>(per) != RestServer::EntryPermissionType::None)
      disablePermisson = false;
    else {
      gdi.selectItemByData("PermissionPerson", size_t(get<RestServer::EntryPermissionType>(per)));
      gdi.selectItemByData("PermissionClass", size_t(get<RestServer::EntryPermissionClass>(per)));
    }
  }
  if (disablePermisson) {
    gdi.disableInput("PermissionPerson");
    gdi.disableInput("PermissionClass");
  }
  else {
    gdi.check("AllowEntry", true);
  }

  gdi.popX();
  gdi.addString("", 10, "help:rest");
}

void RestService::status(gdioutput &gdi) {
  AutoMachine::status(gdi);

  if (server) {
    gdi.addString("", 0, "Server startad på X#" + itos(port));

    RestServer::Statistics rs;
    server->getStatistics(rs);
    gdi.addString("", 0, "Antal förfrågningar: X.#" + itos(rs.numRequests));
    gdi.addString("", 0, "Genomsnittlig svarstid: X ms.#" + itos(rs.averageResponseTime));
    gdi.addString("", 0, "Längsta svarstid: X ms.#" + itos(rs.maxResponseTime));

    gdi.dropLine(0.6);
    gdi.addButton("Update", "Uppdatera").setHandler(this);
    gdi.dropLine(0.6);
    gdi.addString("", 1, "Testa servern:");

    string sport;
    if (port != 80) {
      sport = ":" + itos(port);
    }
    gdi.addString("link", 0, "#http://localhost" + sport + "/meos").setHandler(this);
    
    vector<string> adr;
    ListIpAddresses(adr);

    if (adr.size() > 0) {
      gdi.dropLine();
      gdi.addString("", 1, "Externa adresser:");
      for (string &ip : adr) {
        gdi.addString("link", 0, "#http://" + ip + sport + "/meos").setHandler(this);
      }
    }

    /*
    if (get<RestServer::EntryPermissionType>(server->getEntryPermission()) != RestServer::EntryPermissionType::None) {
      gdi.addString("", fontMediumPlus, "Anmälan");


    }*/

  }

  gdi.dropLine(2);
  gdi.fillRight();
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("InfoService", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void RestService::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) {

}


void RestService::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  if (type == GUI_BUTTON) {
    ButtonInfo &bi = static_cast<ButtonInfo&>(info);
    if (bi.id == "Update") {
      gdi.getTabs().get(TAutoTab)->loadPage(gdi);
    }
    else if (bi.id == "AllowEntry") {
      gdi.setInputStatus("PermissionPerson", gdi.isChecked(bi.id));
      gdi.setInputStatus("PermissionClass", gdi.isChecked(bi.id));
    }
    else if (bi.id == "MapRoot") {
      gdi.setInputStatus("RootMap", gdi.isChecked(bi.id));
    }
  }
  else if (type == GUI_LINK) {
    wstring url = ((TextInfo &)info).text;
    ShellExecute(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
  }
}
