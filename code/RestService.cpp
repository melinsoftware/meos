#include "stdafx.h"
#include "RestService.h"
#include "meos_util.h"
#include "restserver.h"
#include "meosexception.h"

#include <ShellAPI.h>

int AutomaticCB(gdioutput *gdi, int type, void *data);

RestService::RestService() : AutoMachine("Informationsserver"), port(-1) {
}

RestService::~RestService() {
  if (server) {
    server->stop();
    RestServer::remove(server);
  }
}

void RestService::save(oEvent &oe, gdioutput &gdi) {
  if (!server) {
    server = RestServer::construct();

    int xport = gdi.getTextNo("Port");
    if (xport > 0 && xport < 65536) {
      port = xport;
      server->startService(port);
    }
    else
      throw meosException("Invalid port number");
  }
}

void RestService::settings(gdioutput &gdi, oEvent &oe, bool created) {  
  if (port == -1)
    port = oe.getPropertyInt("ServicePort", 2009);

  settingsTitle(gdi, "MeOS Informationsserver REST-API");
  startCancelInterval(gdi, "Save", created, IntervalNone, L"");

  if (!server)
    gdi.addInput("Port", itow(port), 10, 0, L"Port:", L"#http://localhost:[PORT]/meos");
  else 
    gdi.addString("", 0, "Server startad på X#" + itos(port));

  gdi.popX();
  gdi.addString("", 10, "help:rest");
}

void RestService::status(gdioutput &gdi) {
  gdi.pushX();
  gdi.addString("", 1, name);

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
    gdi.addString("link", 0, "#http://localhost:" + itos(port) + "/meos").setHandler(this);
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
  }
  else if (type == GUI_LINK) {
    wstring url = L"http://localhost:" + itow(port) + L"/meos";
    ShellExecute(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
  }
}
