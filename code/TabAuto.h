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

#include "tabbase.h"
#include "gdioutput.h"
#include <string>
#include "oListInfo.h"
#include "importformats.h"

using namespace std;

class TabAuto;
class gdioutput;
class oEvent;

enum AutoSyncType {SyncNone, SyncTimer, SyncDataUp};

enum Machines {
  mPunchMachine,
  mPrintResultsMachine,
  mSplitsMachine,
  mPrewarningMachine,
  mOnlineResults,
  mOnlineInput,
  mSaveBackup,
  mInfoService,

  mMySQLReconnect,
  Unknown = -1,
};

class AutoMachine
{
public:
  enum class Status {
    Good,
    Error,
    Warning,
  };

  enum class State {
    Edit,
    Create,
    Load
  };

private:
  int myid;
  static int uniqueId;
  const Machines type;
  bool isSaved = false;

protected:
  Status lastRunStatus = Status::Good;
  wstring lastStatusMsg;

  bool editMode;
  
  void settingsTitle(gdioutput &gdi, const char *title);
  enum class IntervalType {IntervalNone, IntervalMinute, IntervalSecond};
  void startCancelInterval(gdioutput &gdi, oEvent &oe, const char *startCommand, State state, IntervalType type, const wstring &interval);
  
  virtual bool hasSaveMachine() const {
    return false;
  }
  
  template<typename OP>
  void processProtected(gdioutput& gdi, AutoSyncType ast, OP op) {
    lastRunStatus = Status::Good;
    lastStatusMsg.clear();

    try {
      op();
    }
    catch (const meosException& ex) {
      lastRunStatus = Status::Error;
      lastStatusMsg = ex.wwhat();
      if (ast == AutoSyncType::SyncNone)
        throw;
    }
    catch (const std::exception& ex) {
      lastRunStatus = Status::Error;
      lastStatusMsg = gdioutput::widen(ex.what());
      if (ast == AutoSyncType::SyncNone)
        throw;
    }
    catch (...) {
      lastRunStatus = Status::Error;
      throw;
    }

    if (!lastStatusMsg.empty()) {
      string id = getTypeString() + "_warning";
      gdi.removeFirstInfoBox(id);
      gdi.addInfoBox(id, lastStatusMsg, getDescription(), BoxStyle::HeaderWarning, 10000);
    }
  }

public:

  Status getStatus() const {
    return lastRunStatus;
  }

  const wstring &getStatusMsg() const {
    return lastStatusMsg;
  }

  virtual bool requireList(EStdListType type) const {
    return false;
  }

  virtual void saveMachine(oEvent &oe, const wstring &guiInterval) {
    isSaved = true;
  }

  virtual void loadMachine(oEvent &oe, const wstring &name) {
    if (name != L"default")
      machineName = name;

    isSaved = true;
  }

  bool wasSaved() {
    return isSaved;
  }

  // Return true to auto-remove
  virtual bool removeMe() const {
    return false;
  }

  static AutoMachine *getMachine(int id);
  static void resetGlobalId() {uniqueId = 1;}
  int getId() const {return myid;}
  static shared_ptr<AutoMachine> construct(Machines);
  static Machines getType(const string &typeStr);
  static wstring getDescription(Machines type);
  static string getTypeString(Machines type);
  string getTypeString() const { return getTypeString(type); }
  wstring getMachineName() const {
    return machineName.empty() ? L"default" : machineName;
  }
  wstring getDescription() const {
    return getDescription(type);
  }


  void setEditMode(bool em) {editMode = em;}
  string name;
  wstring machineName;
  DWORD interval; //Interval seconds
  uint64_t timeout; //Timeout (TickCount)
  bool synchronize;
  bool synchronizePunches;

  virtual void settings(gdioutput &gdi, oEvent &oe, State state) = 0;
  virtual void save(oEvent &oe, gdioutput &gdi, bool doProcess) = 0;
  virtual void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) = 0;
  virtual bool isEditMode() const {return editMode;}
  virtual void status(gdioutput &gdi) = 0;
  virtual bool stop() {return true;}
  virtual shared_ptr<AutoMachine> clone() const = 0;
  virtual void cancelEdit() {}

  Machines getType() {
    return type;
  };

  AutoMachine(const string &s, Machines type) : myid(uniqueId++), type(type), name(s), interval(0), timeout(0),
            synchronize(false), synchronizePunches(false), editMode(false) {}
  virtual ~AutoMachine() = 0 {}
};

class SaveMachine :
  public AutoMachine
{
private:
  wstring baseFile;
  int saveIter = 0;

protected:
  bool hasSaveMachine() const final {
    return true;
  }
  void saveMachine(oEvent& oe, const wstring& guiInterval) final;
  void loadMachine(oEvent& oe, const wstring& name) final;

public:

  shared_ptr<AutoMachine> clone() const final {
    auto prm = make_shared<SaveMachine>(*this);
    return prm;
  }

  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  SaveMachine():AutoMachine("Säkerhetskopiera", Machines::mSaveBackup) {
  }
};


class PrewarningMachine :
  public AutoMachine
{
protected:
  wstring waveFolder;
  set<int> controls;
  set<int> controlsSI;
public:
  void settings(gdioutput &gdi, oEvent &oe, State state);
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<PrewarningMachine>(*this);
  }
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  PrewarningMachine():AutoMachine("Förvarningsröst", Machines::mPrewarningMachine) {}
  friend class TabAuto;
};

class MySQLReconnect :
  public AutoMachine
{
protected:
  wstring error;
  wstring timeError;
  wstring timeReconnect;
  HANDLE hThread;
  bool toRemove = false;

public:
  void settings(gdioutput &gdi, oEvent &oe, State state);
  shared_ptr<AutoMachine> clone() const final { 
    return make_shared<MySQLReconnect>(*this);
  }

  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  bool stop();
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final {
  }

  bool removeMe() const final {
    return toRemove;
  }

  MySQLReconnect(const wstring &error);
  virtual ~MySQLReconnect();
  friend class TabAuto;
};

bool isThreadReconnecting();

class PunchMachine :
  public AutoMachine
{
protected:
  int radio;
public:
  shared_ptr<AutoMachine> clone() const final { 
    return make_shared<PunchMachine>(*this);
  }

  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  PunchMachine():AutoMachine("Stämplingsautomat", Machines::mPunchMachine), radio(0) {}
  friend class TabAuto;
};

class SplitsMachine :
  public AutoMachine
{
protected:
  wstring file;
  set<int> classes;
  int leg;
  ExportSplitsData data;
public:
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<SplitsMachine>(*this);
  }

  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  SplitsMachine() : AutoMachine("Export av resultat/sträcktider", Machines::mSplitsMachine), leg(-1) {}
  friend class TabAuto;
};



class TabAuto :
  public TabBase
{
private:
  bool editMode = false;
  int currentMachineEditId = -1;
  bool wasCreated = false;
  bool wasSaved = false;
  bool synchronize = false;
  bool synchronizePunches = false;
  void updateSyncInfo();

  list<shared_ptr<AutoMachine>> machines;
  void setTimer(AutoMachine *am);

  void timerCallback(gdioutput &gdi);
  void syncCallback(gdioutput &gdi);

  void settings(gdioutput &gdi, AutoMachine *sm, AutoMachine::State state, Machines type);

protected:
  void clearCompetitionData();
  bool hasActiveReconnection() const;

public:

  void removedList(EStdListType type);

  AutoMachine *getMachine(int id);
  bool stopMachine(AutoMachine *am);
  void killMachines();
  bool clearPage(gdioutput &gdi, bool postClear);

  AutoMachine &addMachine(const AutoMachine &am) {
    machines.push_back(am.clone());
    setTimer(machines.back().get());
    return *machines.back();
  }

  int processButton(gdioutput &gdi, const ButtonInfo &bu);
  int processListBox(gdioutput &gdi, const ListBoxInfo &bu);

  bool loadPage(gdioutput &gdi, bool showSettingsLast);
  bool loadPage(gdioutput &gdi) {
    return loadPage(gdi, false);
  }

  const char * getTypeStr() const {return "TAutoTab";}
  TabType getType() const {return TAutoTab;}

  TabAuto(oEvent *poe);
  ~TabAuto(void);

  friend class AutoTask;
  friend void tabForceSync(gdioutput &gdi, pEvent oe);

  static void tabAutoKillMachines();
  static void tabAutoRegister(TabAuto* ta);
  static AutoMachine& tabAutoAddMachinge(const AutoMachine& am);
  static bool hasActiveReconnectionMachine();
};

