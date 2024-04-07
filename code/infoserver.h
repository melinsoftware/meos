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

/** Class for keeping an external information server (online results) up-to-date */

#include <vector>
#include <map>
#include <set>

class oBase;
class oEvent;
class oClass;
class oAbstractRunner;
class oRunner;
class oTeam;
class oClub;
class oControl;
class xmlparser;
class gdioutput;


class xmlbuffer {
private:
  struct block {
    string tag;
    vector< pair<string, wstring> > prop;
    wstring value;
    vector<xmlbuffer> subValues;
  };

  list<block> blocks;
  bool complete;
public:
  void setComplete(bool c) {complete = c;}
  xmlbuffer &startTag(const char *tag, const vector< pair<string, wstring> > &prop);
  void endTag();
  void write(const char *tag,
             const vector< pair<string, string> > &prop,
             const string &value);

  void write(const char *tag,
             const vector< pair<string, wstring> > &prop,
             const wstring &value);


  size_t size() const {return blocks.size();}
  bool commit(xmlparser &xml, int count);
  void commitCopy(xmlparser &xml);

  bool isComplete() const { return complete; }
  void startXML(xmlparser &xml, const wstring &dest);
};

class InfoBase
{
private:
	bool committed;
	const int id;
protected:
  void modified() {committed = false;}
  virtual void serialize(xmlbuffer &xml, bool diffOnly) const = 0;

  // Converts a relative time to absolute time:
  // number of tenths of a second since 00:00:00 on the day of
  // the zero time of the competition
  int convertRelativeTime(const oBase &elem, int t);
public:

	int getId() const {return id;}
	bool isCommitted() const {return committed;}

  InfoBase(int id);
  InfoBase(const InfoBase &in);
  void operator=(const InfoBase &in);
	virtual ~InfoBase();

  friend class InfoCompetition;
};

typedef InfoBase * pInfoBase;

class InfoRadioControl : public InfoBase {
  protected:
    wstring name;
    bool synchronize(oControl &c, int number);
    void serialize(xmlbuffer &xml, bool diffOnly) const;
  public:
    InfoRadioControl(int id);
    virtual ~InfoRadioControl() {}

    friend class InfoCompetition;
};

class InfoClass : public InfoBase {
  protected:
    wstring name;
    int sortOrder;
    vector< vector<int> > radioControls;
    vector<int> linearLegNumberToActual;
    vector<int> courses;
  public:
    bool synchronize(bool includeCourses, oClass &c, const set<int> &ctrls);
    void serialize(xmlbuffer &xml, bool diffOnly) const;

    InfoClass(int id);
    virtual ~InfoClass() {}

    friend class InfoCompetition;
};

class InfoMeosStatus : public InfoBase {
  protected:
    wstring eventNameId; // event Name Id, actual name of the database, can also be matched in oevent table of meosmain
    bool onDatabase; // true if currently on database
  public:
    void serialize(xmlbuffer &xml, bool diffOnly) const;
    InfoMeosStatus();
    virtual ~InfoMeosStatus() {}
    void setEventNameId(const wstring &);
    void setOnDatabase(const bool);
};

class InfoOrganization : public InfoBase {
  protected:
    wstring name;
    wstring nationality;
  public:
    InfoOrganization(int id);
    virtual ~InfoOrganization() {}

    bool synchronize(oClub &c);
    void serialize(xmlbuffer &xml, bool diffOnly) const;

    friend class InfoCompetition;
};

struct RadioTime {
  int radioId;
  int runningTime;

  bool operator==(const RadioTime &t) const {
    return radioId == t.radioId && runningTime == t.runningTime;
  }
};

class InfoBaseCompetitor : public InfoBase {
  protected:
    wstring name;
    int organizationId;
    int classId;

    int status;
    int startTime;
    int runningTime;
    wstring bib;
    wstring nationality;
    void serialize(xmlbuffer &xml, bool diffOnly, int course) const;
    bool synchronizeBase(oAbstractRunner &bc);
  public:
    InfoBaseCompetitor(int id);
    virtual ~InfoBaseCompetitor() {}
};

class InfoCompetitor : public InfoBaseCompetitor {
  protected:
    vector<RadioTime> radioTimes;
    int inputTime;
    int totalStatus;
    int course;
    int cardNo = 0;
    bool isRunning = false;
    bool synchronize(const InfoCompetition &cmp, oRunner &c);
    bool changeTotalSt;
    bool changeRadio;
    mutable bool changeCard = false;
  public:
    bool synchronize(bool useTotalResults, bool useCourse, oRunner &c);
    void serialize(xmlbuffer &xml, bool diffOnly) const;

    InfoCompetitor(int id);
    virtual ~InfoCompetitor() {}

    friend class InfoCompetition;
};

class InfoTeam : public InfoBaseCompetitor {
  protected:
    // The outer level holds legs, the inner level holds (parallel/patrol) runners on each leg.
    vector<vector<int>> competitors;
    public:
    bool synchronize(oTeam &t);
    void serialize(xmlbuffer &xml, bool diffOnly) const;
  
    InfoTeam(int id);
    virtual ~InfoTeam() {}
    friend class InfoCompetition;
};

class InfoCompetition : public InfoBase {
private:
    wstring name;
    wstring date;
    wstring organizer;
    wstring homepage;
    int zerotime;
protected:
    bool forceComplete;

    bool includeTotal = false;
    bool withCourse = false;

    list<InfoBase *> toCommit;

    map<int, InfoRadioControl> controls;
    map<int, InfoClass> classes;
    map<int, InfoOrganization> organizations;
    map<int, InfoCompetitor> competitors;
    map<int, InfoTeam> teams;
    vector<pair<string, int>> deleteMap;

    void needCommit(InfoBase &obj);
   
  public:
    void serialize(xmlbuffer &xml, bool diffOnly) const;

    bool includeTotalResults() const {return includeTotal;}
    void includeTotalResults(bool inc) {includeTotal = inc;}

    bool includeCourse() const { return withCourse; }
    void includeCourse(bool inc) { withCourse = inc; }

    const vector<int> &getControls(int classId, int legNumber) const;
    bool synchronize(oEvent &oe, bool onlyCmp, const set<int> &classes, const set<int> &ctrls, bool allowDeletion);
    bool synchronize(oEvent &oe) {
      set<int> dmy;
      return synchronize(oe, true, dmy, dmy, false);
    }
    void getCompleteXML(xmlbuffer &xml);
    void getDiffXML(xmlbuffer &xml);

    void commitComplete();

    InfoCompetition(int id);
    virtual ~InfoCompetition() {}
};
