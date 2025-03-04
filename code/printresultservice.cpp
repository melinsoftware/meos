#include "stdafx.h"

#include <shellapi.h>

#include "printresultservice.h"
#include "classconfiginfo.h"
#include "meosexception.h"
#include "HTMLWriter.h"
#include "machinecontainer.h"
#include "TabList.h"

int AutomaticCB(gdioutput* gdi, GuiEventType type, BaseInfo* data);


PrintResultMachine::PrintResultMachine(int v) :
  AutoMachine("Resultatutskrift", Machines::mPrintResultsMachine) {
  interval = v;
  readOnly = false;
  structuredExport = true;
  htmlRefresh = v;
}

PrintResultMachine::PrintResultMachine(int v, const oListInfo& li) :
  AutoMachine("Utskrift / export", Machines::mPrintResultsMachine), listInfo(li) {
  interval = v;
  readOnly = true;
  structuredExport = false;
  htmlRefresh = v;
  if (!li.getParam().getName().empty())
    machineName = li.getParam().getName();
  else
    machineName = li.getName();
}

PrintResultMachine::~PrintResultMachine() {
  if (!gdiListSettings.empty()) {
    gdioutput *w = getExtraWindow(gdiListSettings, false);
    if (w) {
      w->unregisterEvent("CloseWindow");
      w->closeWindow();
    }
    gdiListSettings = "";
  }
}

void PrintResultMachine::settings(gdioutput& gdi, oEvent& oe, State state) {
  settingsTitle(gdi, "Resultatutskrift / export");
  wstring time = (state == State::Create && interval <= 0) ? L"10:00" : formatTimeMS(interval * timeConstSecond, false, SubSecond::Off);
  startCancelInterval(gdi, oe, "Save", state, IntervalType::IntervalMinute, time);

  if (state == State::Create) {
    oe.getAllClasses(classesToPrint);
  }

  gdi.pushX();
  gdi.fillRight();
  gdi.addCheckbox("DoPrint", "Skriv ut", AutomaticCB, doPrint);
  gdi.dropLine(-0.5);
  gdi.addButton("PrinterSetup", "Skrivare...", AutomaticCB, "Välj skrivare...").setExtra(getId());

  gdi.dropLine(4);
  gdi.popX();
  gdi.addCheckbox("DoExport", "Exportera", AutomaticCB, doExport);
  gdi.dropLine(-1);
  int cx = gdi.getCX();
  gdi.addInput("ExportFile", exportFile, 32, 0, L"Fil att exportera till:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseFile", "Bläddra...", AutomaticCB);
  gdi.setCX(cx);
  gdi.dropLine(2.3);
  if (!readOnly) {
    gdi.addCheckbox("StructuredExport", "Strukturerat exportformat", 0, structuredExport);
    gdi.addCheckbox("HTMLRefresh", "HTML med AutoRefresh", 0, htmlRefresh != 0);
  }
  else {
    gdi.addString("", 0, "HTML formaterad genom listinställningar");
  }

  gdi.dropLine(1.8);
  gdi.setCX(cx);
  gdi.addInput("ExportScript", exportScript, 32, 0, L"Skript att köra efter export:");
  gdi.dropLine(0.7);
  gdi.addButton("BrowseScript", "Bläddra...", AutomaticCB);
  gdi.dropLine(3);
  gdi.popX();

  gdi.setInputStatus("ExportFile", doExport);
  gdi.setInputStatus("ExportScript", doExport);
  gdi.setInputStatus("BrowseFile", doExport);
  gdi.setInputStatus("BrowseScript", doExport);
  gdi.setInputStatus("PrinterSetup", doPrint);

  if (!readOnly) {
    gdi.setInputStatus("StructuredExport", doExport);
    gdi.setInputStatus("HTMLRefresh", doExport);

    gdi.fillDown();
    gdi.addString("", fontMediumPlus, "Listval");
    gdi.dropLine();
    gdi.addString("", 10, "info:autolist");
    gdi.dropLine(0.5);
    gdi.fillRight();
    gdi.addListBox("Classes", 150, 300, 0, L"", L"", true);
    gdi.pushX();
    gdi.fillDown();
    vector< pair<wstring, size_t> > d;
    gdi.setItems("Classes", oe.fillClasses(d, oEvent::extraNone, oEvent::filterNone));
    gdi.setSelection("Classes", classesToPrint);

    gdi.addSelection("ListType", 200, 100, 0, L"Lista");
    oe.fillListTypes(gdi, "ListType", 1);
    if (notShown) {
      notShown = false;
      ClassConfigInfo cnf;
      oe.getClassConfigurationInfo(cnf);
      int type = EStdResultListLARGE;
      if (cnf.hasRelay())
        type = EStdTeamAllLegLARGE;
      else if (cnf.hasPatrol())
        type = EStdPatrolResultListLARGE;
      gdi.selectItemByData("ListType", type);
    }
    else
      gdi.selectItemByData("ListType", listInfo.getListCode());

    gdi.addSelection("LegNumber", 140, 300, 0, L"Sträcka:");
    set<int> clsUnused;
    vector< pair<wstring, size_t> > out;
    oe.fillLegNumbers(clsUnused, listInfo.isTeamList(), true, out);
    gdi.setItems("LegNumber", out);
    gdi.selectItemByData("LegNumber", listInfo.getLegNumberCoded());

    gdi.addCheckbox("PageBreak", "Sidbrytning mellan klasser", 0, pageBreak);
    gdi.addCheckbox("ShowHeader", "Visa rubrik", 0, showHeader);

    gdi.addCheckbox("ShowInterResults", "Visa mellantider", 0, showInterResult,
      "Mellantider visas för namngivna kontroller.");
    gdi.addCheckbox("SplitAnalysis", "Med sträcktidsanalys", 0, splitAnalysis);

    gdi.addCheckbox("OnlyChanged", "Skriv endast ut ändade sidor", 0, po.onlyChanged);

    gdi.dropLine();
    gdi.popX();
    gdi.addButton("SelectAll", "Välj allt", AutomaticCB, "").setExtra(L"Classes");
    gdi.popX();
    gdi.addButton("SelectNone", "Välj inget", AutomaticCB, "").setExtra(L"Classes");
  }
  else {
    gdi.fillDown();
    gdi.addString("", fontMediumPlus, L"Lista av typ 'X'#" + listInfo.getName());

    gdi.addButton("Edit", "Visa och redigera").setHandler(this);

    gdi.dropLine();
    gdi.addCheckbox("OnlyChanged", "Skriv endast ut ändade sidor", 0, po.onlyChanged);
  }
}

bool PrintResultMachine::requireList(EStdListType type) const {
  if (listInfo.getParam().listCode == type)
    return true;

  for (auto& ll : listInfo.linkedLists()) {
    if (ll.getParam().listCode == type)
      return true;
  }

  return false;
}

int PrintResultMachine::getInterval(const wstring& mmss) {
  int t = convertAbsoluteTimeMS(mmss) / timeConstSecond;
  if (t < 2 || t > 7200)
    throw meosException("Intervallet måste anges på formen MM:SS.");

  return t;
}

void PrintResultMachine::cancelEdit() {
  if (!gdiListSettings.empty()) {
    gdioutput* w = getExtraWindow(gdiListSettings, false);
    if (w) {
      w->unregisterEvent("CloseWindow");
      w->closeWindow();
    }
    gdiListSettings = "";
  }
}

void PrintResultMachine::save(oEvent& oe, gdioutput& gdi, bool doProcess) {
  cancelEdit();
  AutoMachine::save(oe, gdi, doProcess);
  wstring minute = gdi.getText("Interval");
  int t = getInterval(minute);

  doExport = gdi.isChecked("DoExport");
  doPrint = gdi.isChecked("DoPrint");
  exportFile = gdi.getText("ExportFile");
  exportScript = gdi.getText("ExportScript");

  if (!readOnly) {
    structuredExport = gdi.isChecked("StructuredExport");
    htmlRefresh = gdi.isChecked("HTMLRefresh") ? t : 0;

    gdi.getSelection("Classes", classesToPrint);

    ListBoxInfo lbi;
    if (gdi.getSelectedItem("ListType", lbi)) {
      oListParam par;
      par.selection = classesToPrint;
      par.listCode = EStdListType(lbi.data);
      par.pageBreak = gdi.isChecked("PageBreak");
      par.showHeader = gdi.isChecked("ShowHeader");
      par.showInterTimes = gdi.isChecked("ShowInterResults");
      par.splitAnalysis = gdi.isChecked("SplitAnalysis");
      int legNr = gdi.getSelectedItem("LegNumber").first;
      if (legNr >= 0)
        par.setLegNumberCoded(legNr);
      else
        par.setLegNumberCoded(0);

      oe.generateListInfo(gdi, par, listInfo);
    }
  }
  po.onlyChanged = gdi.isChecked("OnlyChanged");
  pageBreak = gdi.isChecked("PageBreak");
  showHeader = gdi.isChecked("ShowHeader");

  showInterResult = gdi.isChecked("ShowInterResults");
  splitAnalysis = gdi.isChecked("SplitAnalysis");
  if (doProcess) {
    interval = t;
    synchronize = true; //To force continuos data sync.
  }
}

void PrintResultMachine::process(gdioutput& gdi, oEvent* oe, AutoSyncType ast) {
  if (lock)
    return;

  if (ast != SyncDataUp) {
    processProtected(gdi, ast, [&]() {
      lock = true;
      try {
        gdioutput gdiPrint("print", gdi.getScale());
        gdiPrint.clearPage(false);
        oe->generateList(gdiPrint, true, listInfo, false);
        if (doPrint) {
          gdiPrint.refresh();
          gdiPrint.print(po, oe, true, false, listInfo.getParam().pageBreak);
        }

        if (doExport) {
          if (!exportFile.empty()) {
            checkWriteAccess(exportFile);
            wstring tExport = getTempFile();
            if (!readOnly) {
              if (structuredExport)
                HTMLWriter::writeTableHTML(gdiPrint, tExport, oe->getName(), htmlRefresh, 1.0);
              else
                HTMLWriter::writeHTML(gdiPrint, tExport, oe->getName(), htmlRefresh, 1.0);
            }
            else {
              HTMLWriter::write(gdiPrint, tExport, oe->getName(), 0, listInfo.getParam(), *oe);
            }

            try {
              moveFile(tExport, exportFile);
            }
            catch (...) {
              removeTempFile(tExport);
              throw;
            }


            if (!exportScript.empty()) {
              ShellExecute(NULL, NULL, exportScript.c_str(), exportFile.c_str(), NULL, SW_HIDE);
            }
          }
        }
      }
      catch (...) {
        lock = false;
        throw;
      }
      lock = false;
      });
  }
}

void PrintResultMachine::status(gdioutput& gdi)
{
  gdi.fillRight();
  gdi.pushX();
  AutoMachine::status(gdi);
  gdi.addString("", 0, listInfo.getName());
  gdi.dropLine();
  if (doExport) {
    gdi.popX();
    gdi.addString("", 0, "Målfil: ");
    gdi.addStringUT(0, exportFile).setColor(colorRed);
    gdi.dropLine();
  }
  gdi.fillRight();
  gdi.popX();
  if (interval > 0) {
    gdi.dropLine(0.2);
    gdi.addString("", 0, "Automatisk utskrift / export: ");
    gdi.addTimer(gdi.getCY(), gdi.getCX(), timerIgnoreSign, (GetTickCount64() - timeout) / 1000);
  }
  else {

  }
  gdi.popX();
  gdi.dropLine(2);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
  gdi.addButton("PrintNow", "Exportera nu", AutomaticCB).setExtra(getId());
  gdi.fillDown();
  gdi.addButton("Result", "Inställningar...", AutomaticCB).setExtra(getId());
  gdi.popX();
}

void PrintResultMachine::saveMachine(oEvent& oe, const wstring& guiInterval) {
  int t = getInterval(guiInterval);
  AutoMachine::saveMachine(oe, guiInterval);
  auto& cnt = oe.getMachineContainer().set(getTypeString(), getMachineName());

  cnt.set("file", exportFile);
  cnt.set("script", exportScript);
  cnt.set("classes", classesToPrint);
  cnt.set("export", doExport);
  cnt.set("print", doPrint);
  cnt.set("structured", structuredExport);
  cnt.set("intertime", showInterResult);
  cnt.set("split", splitAnalysis);
  cnt.set("rd", readOnly);
  cnt.set("refresh", htmlRefresh);

  xmlparser xml;
  xml.openMemoryOutput(true);
  xml.startTag("Lists");
  map<int, int> idToIx;
  listInfo.getParam().serialize(xml, oe.getListContainer(), idToIx);
  for (auto& linkedList : listInfo.linkedLists()) {
    linkedList.getParam().serialize(xml, oe.getListContainer(), idToIx);
  }
  xml.endTag();

  string res;
  xml.getMemoryOutput(res);
  cnt.set("list", gdioutput::widen(res));
  cnt.set("interval", t);
}

void PrintResultMachine::loadMachine(oEvent& oe, const wstring& name) {
  auto* cnt = oe.getMachineContainer().get(getTypeString(), name);
  if (!cnt)
    return;

  AutoMachine::loadMachine(oe, name);
  exportFile = cnt->getString("file");
  exportScript = cnt->getString("script");
  auto clsV = cnt->getVectorInt("classes");
  classesToPrint.insert(clsV.begin(), clsV.end());

  doExport = cnt->getInt("export") != 0;
  doPrint = cnt->getInt("print") != 0;
  structuredExport = cnt->getInt("structured") != 0;
  showInterResult = cnt->getInt("intertime") != 0;
  splitAnalysis = cnt->getInt("split") != 0;
  readOnly = cnt->getInt("rd") != 0;
  htmlRefresh = cnt->getInt("refresh");

  wstring wStrList;
  wStrList = cnt->getString("list");
  string strList = gdioutput::narrow(wStrList);

  xmlparser xml;
  xml.readMemory(strList, 0);

  xmlobject xLists = xml.getObject("Lists");

  xmlList xParams;
  xLists.getObjects("ListParam", xParams);
  vector<oListParam> params;
  for (xmlobject xPar : xParams) {
    params.emplace_back();
    params.back().deserialize(xPar, oe.getListContainer());
  }
  try {
    oe.generateListInfo(oe.gdiBase(), params, listInfo);
  }
  catch (const meosException&) {
  }

  interval = cnt->getInt("interval");
}

void PrintResultMachine::handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
  if (type == GuiEventType::GUI_BUTTON) {
    if (info.id == "Edit") {
      TabList *tl = dynamic_cast<TabList *>(gdi.getTabs().get(TabType::TListTab));
      gdioutput *gdiList = tl->showList(gdi, listInfo, this);
      gdiListSettings = gdiList->getTag();
      gdi.disableInput("Edit");
      mainGdi = &gdi;
      gdiList->registerEvent("CloseWindow", nullptr).setHandler(this);
    }
  }
  else if (type == GuiEventType::GUI_EVENT) {
    if (info.id == "CloseWindow") {
      if (mainGdi && mainGdi->hasWidget("Edit"))
        mainGdi->enableInput("Edit");

      mainGdi = nullptr;
      gdiListSettings = "";
    }
  }
}

void PrintResultMachine::updateListParam(int index, oListParam& listParam){
  listInfo.getParam() = listParam;
}
