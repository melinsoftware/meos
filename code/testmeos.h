#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2020 Melin Software HB

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

class gdioutput;
class oEvent;
class BaseInfo;

#include <vector>
#include "TabBase.h"
#include "subcommand.h"

enum GDICOLOR;
enum PropertyType;

enum TestStatus {
  PASSED,
  FAILED,
  NORUN,
  RUNNING,
};

struct meosAssertionFailure {
  meosAssertionFailure() {message = L"MeOS assertion failure";};
  meosAssertionFailure(const string &err) : message(err.begin(), err.end()) {}
  meosAssertionFailure(const wstring &err) : message(err) {}
  wstring message;
};

class TestMeOS : public SubCommand {
private:
  oEvent *oe_main;
  gdioutput *gdi_main;
  string test;
  vector<TestMeOS *> subTests;
  void runProtected(bool protect) const;
  mutable list<TestMeOS> subWindows;
  mutable vector<wstring> tmpFiles;

  mutable TestStatus status;
  mutable wstring message;
  int testId;
  int *testIdMain; // Pointer to main test id

  // Subwindow constructor
  TestMeOS(const TestMeOS &tmIn, gdioutput &newWindow);
  
protected:
  oEvent &oe() {return *oe_main;}
  const oEvent &oe() const {return *oe_main;}
  
  TestMeOS &registerTest(const TestMeOS &test);

  void registerSubCommand(const string &cmd) const;

  void showTab(TabType type) const;
  
  void insertCard(int cardNo, const char *ser) const;

  void assertEquals(int expected, int value) const;
  void assertEquals(const string &expected, const string &value) const;
  void assertEquals(const char *message, const char *expected, const string &value) const;
  void assertEquals(const string &message, const string &expected, const string &value) const;

  void assertEquals(const wstring &expected, const wstring &value) const;
  void assertEquals(const wstring &message, const wstring &expected, const wstring &value) const;


  void assertTrue(const char *message, bool condition) const;

  int getResultModuleIndex(const char *tag) const;
  int getListIndex(const char *name) const;
  int getResultListIndex(const char *name) const {
    return 10 + getListIndex(name);
  }

  virtual TestMeOS *newInstance() const;

  void publishChildren(gdioutput &gdi, pair<int,int> &passFail) const;


  const string getLastExtraWindow() const;
  const TestMeOS &getExtraWindow(const string &tag) const;

  void runInternal(bool protect) const;
  virtual void cleanup() const;
public:
  gdioutput &gdi() const {return *gdi_main;}

  void pressEscape() const;
  void closeWindow() const;

  void press(const char *btn) const;
  void press(const char *btn, int extra) const;
  void press(const char *btn, const char *extra) const;

  string selectString(const char *btn, const char *data) const;
  string select(const char *btn, size_t data) const;
  void input(const char *id, const char *data) const;
  void click(const char *id) const;
  void click(const char *id, int extra) const;

  void dblclick(const char *id, size_t data) const;

  void tableCmd(const char *id) const;
  void setTableText(int editRow, int editCol, const string &text) const;
  string getTableText(int editRow, int editCol) const;

  void setAnswer(const char *ans) const;
  void setFile(const string &file) const;

  void checkString(const char *str, int count = 1) const;
  void checkStringRes(const char *str, int count = 1) const;
  void checkSubString(const char *str, int count = 1) const;
  
  string getText(const char *ctrl) const;
  bool isChecked(const char *ctrl) const;

  void runAll() const;
  bool runSpecific(int id) const;

  void publish(gdioutput &gdi) const;
  void getTests(vector< pair<wstring, size_t> > &tl) const;

  string getTestFile(const char *relPath) const;
  wstring getTempFile() const;

  virtual void run() const;

  // Run a test sub command
  virtual void subCommand(const string &cmd) const override {}

  TestMeOS(oEvent *oe, const string &test);
  TestMeOS(TestMeOS &tmIn, const char *test);
  virtual ~TestMeOS();

  friend void registerTests(TestMeOS &tm);
};
