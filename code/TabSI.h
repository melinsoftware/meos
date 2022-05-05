#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2022 Melin Software HB

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
#include "SportIdent.h"
#include "Printer.h"
#include "inthashmap.h"
#include "autocompletehandler.h"

struct PunchInfo;
class csvparser;
struct AutoCompleteRecord;

class TabSI :  public TabBase, AutoCompleteHandler {
public:
  enum SIMode {
    ModeReadOut,
    ModeAssignCards,
    ModeCheckCards,    
    ModeEntry,
    ModeCardData,
    ModeRegisterCards,
  };
 
  void setMode(SIMode m) { mode = m; }
private:
  /** Try to automatcally assign a class to runner (if none is given)
      Return true if runner has a class on exist */
  bool autoAssignClass(pRunner r, const SICard &sic);

  void checkMoreCardsInQueue(gdioutput &gdi);

  pRunner autoMatch(const SICard &sic, pRunner db_r);
  void processPunchOnly(gdioutput &gdi, const SICard &sic);
  void startInteractive(gdioutput &gdi, const SICard &sic,
                        pRunner r, pRunner db_r);
  bool processCard(gdioutput &gdi, pRunner runner, const SICard &csic,
                   bool silent=false);
  bool processUnmatched(gdioutput &gdi, const SICard &csic, bool silent);

  void rentCardInfo(gdioutput &gdi, int width);

  bool interactiveReadout;
  bool useDatabase;
  bool printSplits;
  bool printStartInfo;
  bool manualInput;
  PrinterObject splitPrinter;
  list< pair<unsigned, int> > printPunchRunnerIdQueue;
  void addToPrintQueue(pRunner r);
  
  enum class SND {
    OK,
    Leader,
    NotOK,
    ActionNeeded
  };

  string typeFromSndType(SND s);

  set<wstring> checkedSound;

  void playReadoutSound(SND type);

  vector<PunchInfo> punches;
  vector<SICard> cards;
  vector<wstring> filterDate;

  set<int> warnedClassOutOfMaps;
  
  shared_ptr<GuiHandler> resetHiredCardHandler;
  GuiHandler *getResetHiredCardHandler();

  int runnerMatchedId;
  bool printErrorShown;
  void printProtected(gdioutput &gdi, gdioutput &gdiprint);

  //Operation mode
  SIMode mode;
  bool lockedFunction = false;
  bool allowControl = true;
  bool allowFinish = true;
  bool allowStart = false;

  int currentAssignIndex;

  void printSIInfo(gdioutput &gdi, const wstring &port) const;

  void assignCard(gdioutput &gdi, const SICard &sic);
  void entryCard(gdioutput &gdi, const SICard &sic);

  void updateEntryInfo(gdioutput &gdi);
  void generateEntryLine(gdioutput &gdi, pRunner r);
  int lastClassId;
  int lastClubId;
  wstring lastFee;
  int inputId;
  int numSavedCardsOnCmpOpen = 0;
  void showCheckCardStatus(gdioutput &gdi, const string &cmd);
  void showRegisterHiredCards(gdioutput &gdi);

  wstring getCardInfo(bool param, vector<int> &count) const;
  // Formatting for card tick off
  bool checkHeader;
  int cardPosX;
  int cardPosY;
  int cardOffsetX;
  int cardNumCol;
  int cardCurrentCol;

  enum CardNumberFlags {
    // Basic flags
    CNFChecked = 1,
    CNFUsed = 2,
    CNFNotRented = 4,

    // Combinations
    CNFCheckedAndUsed = 3,
    CNFCheckedNotRented = 5,
    CNFRentAndNotRent = 6,
    CNFCheckedRentAndNotRent = 7,
  };

  map<int, CardNumberFlags> checkedCardFlags;
  void checkCard(gdioutput &gdi, const SICard &sic, bool updateAll);
  void registerHiredCard(gdioutput &gdi, const SICard &sic);

  void showReadPunches(gdioutput &gdi, vector<PunchInfo> &punches, set<string> &dates);
  void showReadCards(gdioutput &gdi, vector<SICard> &cards);

  void showManualInput(gdioutput &gdi);
  void showAssignCard(gdioutput &gdi, bool showHelp);

  pRunner getRunnerByIdentifier(int id) const;
  mutable inthashmap identifierToRunnerId;
  mutable int minRunnerId;
  void tieCard(gdioutput &gdi);

  // Insert card without converting times and with/without runner
  void processInsertCard(const SICard &csic);

  void generateSplits(const pRunner r, gdioutput &gdi);
  int logcounter;
  shared_ptr<csvparser> logger;

  string insertCardNumberField;

  void insertSICardAux(gdioutput &gdi, SICard &sic);

  pRunner getRunnerForCardSplitPrint(const SICard &sic) const;

  // Ask if card is to be overwritten
  bool askOverwriteCard(gdioutput &gdi, pRunner r) const;

  list< pair<int, SICard> > savedCards;
  int savedCardUniqueId;
  SICard &getCard(int id) const;

  void showModeCardData(gdioutput &gdi);

  void printCard(gdioutput &gdi, int lineBreak, int cardId, SICard *crdRef, bool forPrinter) const;
  void generateSplits(int cardId, gdioutput &gdi);

  static int analyzePunch(SIPunch &p, int &start, int &accTime, int &days);


  void createCompetitionFromCards(gdioutput &gdi);

  int NC;

  class EditCardData : public GuiHandler {
    TabSI *tabSI;
    EditCardData(const EditCardData&);
    EditCardData &operator=(const EditCardData&);
  public:
    EditCardData() : tabSI(0) {}
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    friend class TabSI;
  };

  class DirectEntryGUI : public GuiHandler {
    TabSI *tabSI;
    DirectEntryGUI(const DirectEntryGUI&);
    DirectEntryGUI &operator=(const DirectEntryGUI&);
  public:

    void updateFees(gdioutput &gdi, const pClass cls, int age);
    DirectEntryGUI() : tabSI(0) {}
    void handle(gdioutput &gdi, BaseInfo &info, GuiEventType type);
    friend class TabSI;
  };

  EditCardData editCardData;
  DirectEntryGUI directEntryGUI;

  oClub *extractClub(gdioutput &gdi) const;
  RunnerWDBEntry *extractRunner(gdioutput &gdi) const;

  void updateReadoutFunction(gdioutput &gdi);
  int readoutFunctionX = 0;
  int readoutFunctionY = 0;

  void playSoundResource(int res) const;
  void playSoundFile(const wstring& file) const;
protected:
  void clearCompetitionData();

  static wstring getPlace(const oRunner *r);
  static wstring getTimeString(const oRunner *r);
  static wstring getTimeAfterString(const oRunner *r);

public:

  bool showDatabase() const;

  static vector<AutoCompleteRecord> getRunnerAutoCompelete(RunnerDB &db, const vector< pair<RunnerWDBEntry *, int>> &rw, pClub dbClub);

  void handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) override;

    // Returns true if a repeated check should be done (there is more to print)
  bool checkpPrintQueue(gdioutput &gdi);

  struct StoredStartInfo {
    wstring storedName;
    wstring storedCardNo;
    wstring storedClub;
    wstring storedFee;
    wstring storedPhone;
    wstring storedStartTime;
    bool allStages;
    bool rentState;
    bool hasPaid;
    int payMode;
    DWORD age;
    int storedClassId;

    void clear();
    void checkAge();
    StoredStartInfo() : rentState(false), age(0), storedClassId(0), hasPaid(0), payMode(0), allStages(false) {}
  };

  StoredStartInfo storedInfo;
  void generatePayModeWidget(gdioutput &gdi) const;
  static bool writePayMode(gdioutput &gdi, int amount, oRunner &r);

  static SportIdent &getSI(const gdioutput &gdi);
  void printerSetup(gdioutput &gdi);

  void generateStartInfo(gdioutput &gdi, const oRunner &r);
  bool hasPrintStartInfo() const {return printStartInfo;}
  void setPrintStartInfo(bool info) {printStartInfo = info;}

  int siCB(gdioutput &gdi, int type, void *data);

  void logCard(gdioutput &gdi, const SICard &card);

  void setCardNumberField(const string &fieldId) {insertCardNumberField=fieldId;}

  //SICard CSIC;
  SICard activeSIC;
  list<SICard> CardQueue;

  const char * getTypeStr() const {return "TSITab";}
  TabType getType() const {return TSITab;}

  void insertSICard(gdioutput &gdi, SICard &sic);
  void clearQueue() { CardQueue.clear(); }
  void refillComPorts(gdioutput &gdi);

  bool loadPage(gdioutput &gdi);
  void showReadoutMode(gdioutput & gdi);

  void showReadoutStatus(gdioutput &gdi, const oRunner *r, 
                         const oCard *crd, SICard *card,
                         const wstring &missingPunchList);

  TabSI(oEvent *oe);
  ~TabSI(void);
};
