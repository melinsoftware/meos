#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2025 Melin Software HB

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

#include "Printer.h"
#include "TabAuto.h"
#include "guihandler.h"
#include "metalist.h"

class PrintResultMachine final : public AutoMachine, public GuiHandler, public ListUpdater {
private:
  wstring exportFile;
  wstring exportScript;
  set<int> classesToPrint;
  oListInfo listInfo;

  bool doExport = false;
  bool doPrint = true;
  bool structuredExport;
  bool pageBreak = true;
  bool showHeader = false;
  bool showInterResult = true;
  bool splitAnalysis = true;
  bool readOnly;
  int htmlRefresh;

  bool notShown = true; // True first time settings are shown
  PrinterObject po;
  bool lock = false; // true while printing
  bool errorLock = false; // true while showing error dialog

  gdioutput* mainGdi = nullptr;
  string gdiListSettings;
protected:
  bool hasSaveMachine() const final {
    return true;
  }
  void saveMachine(oEvent& oe, const wstring& guiInterval) final;
  void loadMachine(oEvent& oe, const wstring& name) final;

public:

  void printSetup(gdioutput& gdi) {
    gdi.printSetup(po);
  }

  shared_ptr<AutoMachine> clone() const final {
    auto prm = make_shared<PrintResultMachine>(*this);
    prm->lock = false;
    prm->errorLock = false;
    return prm;
  }
  void status(gdioutput& gdi) final;
  void process(gdioutput& gdi, oEvent* oe, AutoSyncType ast) final;
  void settings(gdioutput& gdi, oEvent& oe, State state) final;
  void save(oEvent& oe, gdioutput& gdi, bool doProcess) final;
  void cancelEdit() final;

  bool requireList(EStdListType type) const final;

  static int getInterval(const wstring& mmss);

  void setHTML(const wstring& file, int timeout) {
    exportFile = file;
    doExport = true;
    doPrint = false;
    if (timeout > 0)
      interval = timeout;
  }

  PrintResultMachine(int v);

  PrintResultMachine(int v, const oListInfo& li);
  
  ~PrintResultMachine();

  // Inherited via GuiHandler
  void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) override;

  // Inherited via ListUpdater
  void updateListParam(int index, oListParam& listParam) override;
};
