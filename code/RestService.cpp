#include "stdafx.h"
#include "RestService.h"
#include "meos_util.h"
#include "restserver.h"

int AutomaticCB(gdioutput *gdi, int type, void *data);

RestService::RestService() : AutoMachine("RestService"), port(-1) {
}


RestService::~RestService() {
  if (server) {
    server->stop();
    RestServer::remove(server);
  }
}

void RestService::save(oEvent &oe, gdioutput &gdi) {
  if (!server)
    server = RestServer::construct();
  
  int port = gdi.getTextNo("Port");
  if (port > 0 && port < 65536)
    server->startService(port);
  else
    throw std::exception("Invalid port number");
}

void RestService::settings(gdioutput &gdi, oEvent &oe, bool created) {  
  if (port == -1)
    port = oe.getPropertyInt("ServicePort", 2009);

  settingsTitle(gdi, "MeOS Informationsserver REST-API");
  startCancelInterval(gdi, "Save", created, IntervalNone, L"");

  if (!server)
    gdi.addInput("Port", itow(port), 10, 0, L"Port:", L"Testa genom http://localhost:[PORT]/meos");
  else 
    gdi.addString("", 0, "Server startad på X#" + itos(port));

  gdi.popX();
  gdi.addString("", 10, "help:rest");
}

void RestService::status(gdioutput &gdi) {
  gdi.pushX();
  gdi.addString("", 1, name);
  /*if (!baseFile.empty()) {
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", 0, L"Destination: X#" + baseFile);

    if (interval>0) {
      gdi.popX();
      gdi.dropLine(1);
      gdi.addString("", 0, "Säkerhetskopierar om: ");
      gdi.addTimer(gdi.getCY(), gdi.getCX(), timerIgnoreSign, (GetTickCount() - timeout) / 1000);
    }

    gdi.popX();
  }*/

  if (server) {
    gdi.addString("", 0, "Server startad på port X#" + itos(port));

    RestServer::Statistics rs;
    server->getStatistics(rs);
    gdi.addString("", 0, "Antal förfrågningar: X#" + itos(rs.numRequests));
    gdi.addString("", 0, "Genomsnittlig svarstid: X ms#" + itos(rs.averageResponseTime));
    gdi.addString("", 0, "Längsta svarstid: X ms#" + itos(rs.maxResponseTime));
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

  }
}