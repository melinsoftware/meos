#pragma once

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

#include "oBase.h"
#include "parser.h"
#include "oListInfo.h"

class oAbstractRunner;
class oTeam;
class oRunner;
struct oListParam;
class xmlparser;
class xmlobject;

class GeneralResult
{
private:
  const oListParam *context;

protected:

  enum PrincipalSort {None, ClassWise, CourseWise};

  virtual PrincipalSort getPrincipalSort() const {return ClassWise;}

  virtual int score(oTeam &team, RunnerStatus st, int time, int points) const;
  virtual RunnerStatus deduceStatus(oTeam &team) const;
  virtual int deduceTime(oTeam &team) const;
  virtual int deducePoints(oTeam &team) const;

  virtual int score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const;
  virtual RunnerStatus deduceStatus(oRunner &runner) const;
  virtual int deduceTime(oRunner &runner, int startTime) const;
  virtual int deducePoints(oRunner &runner) const;

  virtual void prepareCalculations(oEvent &oe, bool prepareForTeam, int inputNumber) const;
  virtual void prepareCalculations(oTeam &team) const;
  virtual void prepareCalculations(oRunner &runner) const;
  virtual void storeOutput(vector<int> &times, vector<int> &numbers) const;

  int getListParamTimeToControl() const;
  int getListParamTimeFromControl() const;

public:

  void setContext(const oListParam *context);
  void clearContext();

  void calculateTeamResults(vector<oTeam *> &teams, oListInfo::ResultType resType, bool sortTeams, int inputNumber) const;
  void calculateIndividualResults(vector<oRunner *> &runners, oListInfo::ResultType resType, bool sortRunners, int inputNumber) const;
  void sortTeamMembers(vector<oRunner *> &runners) const;

  template<class T> void sort(vector<T*> &rt, SortOrder so) const;

  GeneralResult(void);
  virtual ~GeneralResult(void);
};

class ResultAtControl : public GeneralResult {
protected:
  int score(oTeam &team, RunnerStatus st, int time, int points) const;
  RunnerStatus deduceStatus(oTeam &team) const;
  int deduceTime(oTeam &team) const;
  int deducePoints(oTeam &team) const;

  int score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const;
  RunnerStatus deduceStatus(oRunner &runner) const;
  int deduceTime(oRunner &runner, int startTime) const;
  int deducePoints(oRunner &runner) const;
};

class TotalResultAtControl : public ResultAtControl {
protected:
  int deduceTime(oRunner &runner, int startTime) const;
  RunnerStatus deduceStatus(oRunner &runner) const;
  int score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const;
};

class DynamicResult : public GeneralResult {
public:

  enum DynamicMethods {
    MTScore,
    MDeduceTStatus,
    MDeduceTTime,
    MDeduceTPoints,

    MRScore,
    MDeduceRStatus,
    MDeduceRTime,
    MDeduceRPoints,
    _Mlast
  };

private:

  static map<string, DynamicMethods> symb2Method;
  static map<DynamicMethods, pair<string, string> > method2SymbName;
  static int instanceCount;

  class MethodInfo {
    string source;
    mutable ParseNode *pn;
    string description;
  public:
    friend class DynamicResult;
    MethodInfo();
    ~MethodInfo();
  };

  vector<MethodInfo> methods;
  mutable bool isCompiled;
  mutable Parser parser;
  string name;
  string tag;
  string description;
  string annotation;
  mutable string origin;
  string timeStamp;
  bool builtIn;
  mutable bool readOnly;


  const ParseNode *getMethod(DynamicMethods method) const;
  void addSymbol(DynamicMethods method, const char *symb, const char *name);
  RunnerStatus toStatus(int status) const;
  
  void prepareCommon(oAbstractRunner &runner) const;
  
  static string getInternalPath(const string &tag);
public:

  void setReadOnly() const {readOnly = true;}

  bool isReadOnly() const {return readOnly;}

  const string &getTimeStamp() const {return timeStamp;}

  static string undecorateTag(const string &inputTag);

  long long getHashCode() const;

  void getSymbols(vector< pair<string, size_t> > &symb) const;
  void getSymbolInfo(int ix, string &name, string &desc) const;

  void declareSymbols(DynamicMethods m, bool clear) const;

  void prepareCalculations(oEvent &oe, bool prepareForTeam, int inputNumber) const;
  void prepareCalculations(oTeam &team) const;
  void prepareCalculations(oRunner &runner) const;
  void storeOutput(vector<int> &times, vector<int> &numbers) const;

  int score(oTeam &team, RunnerStatus st, int time, int points) const;
  RunnerStatus deduceStatus(oTeam &team) const;
  int deduceTime(oTeam &team) const;
  int deducePoints(oTeam &team) const;

  int score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const;
  RunnerStatus deduceStatus(oRunner &runner) const;
  int deduceTime(oRunner &runner, int startTime) const;
  int deducePoints(oRunner &runner) const;

  DynamicResult();
  DynamicResult(const DynamicResult &resIn);
  void operator=(const DynamicResult &ctr);

  ~DynamicResult();

  bool hasMethod(DynamicMethods method) const {return getMethod(method) != 0;}
  

  const string &getMethodSource(DynamicMethods method) const;
  void setMethodSource(DynamicMethods method, const string &source);

  void getMethodTypes(vector< pair<DynamicMethods, string> > &mt) const;
  //const string &getMethodName(DynamicMethods method) const;

  const string &getTag() const {return tag;}
  void setTag(const string &t) {tag = t;}
  void setBuiltIn() {builtIn = true;}
  bool isBuiltIn() const {return builtIn;}
  string getName(bool withAnnotation) const;
  void setName(const string &n) {name = n;}
  void setAnnotation(const string &a) {annotation = a;}
  const string &getDescription() const {return description;}
  void setDescription(const string &n) {description = n;}

  void save(const string &file) const;
  void save(xmlparser &xml) const;

  void load(const string &file);
  void load(const xmlobject &xDef);

  void compile(bool forceRecompile) const;

  void debugDumpVariables(gdioutput &gdi, bool includeSymbols) const;
   
  void clear();
};

