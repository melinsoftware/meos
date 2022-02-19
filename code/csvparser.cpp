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

// csvparser.cpp: implementation of the csvparser class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "meos.h"
#include "csvparser.h"
#include "oEvent.h"
#include "SportIdent.h"
#include "meos_util.h"
#include "localizer.h"
#include "importformats.h"

#include "meosexception.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#include <vector>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const int externalSourceId = 17000017;
extern gdioutput *gdi_main;

static int wtoi(const wstring &sp) {
  return _wtoi(sp.c_str());
}
csvparser::csvparser()
{
  LineNumber=0;
}

csvparser::~csvparser()
{

}

csvparser::CSV csvparser::iscsv(const wstring &file) {
  ifstream fin(file);

  if (!fin.good())
    return CSV::NoCSV;

  char bf[2048];
  bool isCSVType = false;
  while (fin.good() && !isCSVType) {
    fin.getline(bf, 2048);
    isCSVType = strlen(bf) >= 3;
  }

  fin.close();

  vector<char *> sp;
  split(bf, sp);
  if (sp.size() > 0) {
    string sp0 = sp[0];
    if (sp0.find("<?xml") != string::npos)
      return CSV::NoCSV;
  }

  if (sp.size()==1 && strcmp(sp[0], "RAIDDATA")==0)
    return CSV::RAID;

  if (sp.size()<2)//No csv
    return CSV::NoCSV;

  if (_stricmp(sp[1], "Descr")==0 || _stricmp(sp[1], "Namn")==0
      || _stricmp(sp[1], "Descr.")==0 || _stricmp(sp[1], "Navn")==0) //OS-fil (SWE/ENG)??
    return CSV::OS;
  else return CSV::OE; //OE?!
  }

RunnerStatus ConvertOEStatus(int i)
{
  switch(i)
  {
      case 0:
      return StatusOK;
      case 1:  // Ej start
      return StatusDNS;
      case 2:  // Utg.
      return StatusDNF;
      case 3:  // Felst.
      return StatusMP;
      case 4: //Disk
      return StatusDQ;
      case 5: //Maxtid
      return StatusMAX;
  }
  return StatusUnknown;
}


//Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented;Database Id;Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented;Database Id;Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented;Database Id;(may be more) ...

bool csvparser::importOS_CSV(oEvent &event, const wstring &file)
{
  enum {OSstno=0, OSdesc=1, OSstart=4, OStime=5, OSstatus=6, OSclubno=7, OSclub=9,
    OSnat=10, OSclassno=11, OSclass=12, OSlegs=14, OSfee=21, OSpaid=22};

  const int Offset=23;
  const int PostSize=11;

  enum {OSRsname=0, OSRfname=1, OSRyb=2, OSRsex=3, OSRstart=4,
    OSRfinish=5, OSRstatus=7, OSRcard=8, OSRrentcard=9};
  
  nimport=0;
  list< vector<wstring> > allLines;
  parse(file, allLines);
  list< vector<wstring> >::iterator it = allLines.begin();

  set<wstring> matchedClasses;
  // Skip first line
  while (++it != allLines.end()) {
    //fin.getline(bf, 1024);
    //split(bf, sp);
    const vector<wstring> &sp = *it;

    if (sp.size()>20 && sp[OSclub].size()>0)
    {
      nimport++;

      //Create club with this club number...
      int ClubId=wtoi(sp[OSclubno]);
      pClub pclub=event.getClubCreate(ClubId, sp[OSclub]);

      if (pclub){
        pclub->getDI().setString("Nationality", sp[OSnat]);
        pclub->synchronize(true);
      }

      //Create class with this class number...
      int ClassId=wtoi(sp[OSclassno]);
      event.getClassCreate(ClassId, sp[OSclass], matchedClasses);

      //Club is autocreated...
      pTeam team=event.addTeam(sp[OSclub] + L" " +  sp[OSdesc], ClubId,  ClassId);
      team->setEntrySource(externalSourceId);

      team->setStartNo(wtoi(sp[OSstno]), oBase::ChangeType::Update);

      if (sp[12].length()>0)
        team->setStatus( ConvertOEStatus( wtoi(sp[OSstatus]) ), true, oBase::ChangeType::Update);

      team->setStartTime(event.convertAbsoluteTime(sp[OSstart]), true, oBase::ChangeType::Update);

      if (sp[OStime].length()>0)
        team->setFinishTime( event.convertAbsoluteTime(sp[OSstart])+event.convertAbsoluteTime(sp[OStime])-event.getZeroTimeNum() );

      if (team->getStatus()==StatusOK && team->getFinishTime()==0)
        team->setStatus(StatusUnknown, true, oBase::ChangeType::Update);

      unsigned rindex=Offset;

      oDataInterface teamDI=team->getDI();

      teamDI.setInt("Fee", wtoi(sp[OSfee]));
      teamDI.setInt("Paid", wtoi(sp[OSpaid]));
      teamDI.setString("Nationality", sp[OSnat]);

      //Import runners!
      int runner=0;
      while( (rindex+OSRrentcard)<sp.size() && sp[rindex+OSRfname].length()>0 ){
        int year = extendYear(wtoi(sp[rindex+OSRyb]));
        int cardNo = wtoi(sp[rindex+OSRcard]);
        wstring sname = sp[rindex+OSRsname] + L", "  + sp[rindex+OSRfname];
        pRunner r = event.addRunner(sname, ClubId,
                                    ClassId, cardNo, year, false);
        
        r->setEntrySource(externalSourceId);
        oDataInterface DI=r->getDI();
        r->setSex(interpretSex(sp[rindex + OSRsex]));
        DI.setString("Nationality", sp[OSnat]);

        if (sp[rindex+OSRrentcard].length() > 0)
          DI.setInt("CardFee", event.getDCI().getInt("CardFee"));

        //r->setCardNo(atoi(sp[rindex+OSRcard]), false);
        r->setStartTime(event.convertAbsoluteTime(sp[rindex+OSRstart]), true, oBase::ChangeType::Update);
        r->setFinishTime( event.convertAbsoluteTime(sp[rindex+OSRfinish]) );

        if (sp[rindex+OSRstatus].length()>0)
          r->setStatus( ConvertOEStatus( wtoi(sp[rindex+OSRstatus]) ), true, oBase::ChangeType::Update, false);

        if (r->getStatus()==StatusOK && r->getRunningTime(false)==0)
          r->setStatus(StatusUnknown, true, oBase::ChangeType::Update, false);

        r->addClassDefaultFee(false);

        team->setRunner(runner++, r, true);

        rindex+=PostSize;
      }
      //int nrunners=team->GetNumRunners();
      pClass pc=event.getClass(ClassId);

      if (pc && runner>(int)pc->getNumStages())
        pc->setNumStages(runner);

      team->evaluate(oBase::ChangeType::Update);
    }
  }
  fin.close();

  return true;
}


bool csvparser::importOE_CSV(oEvent &event, const wstring &file) {

  enum {OEstno=0, OEcard=1, OEid=2, OEsurname=3, OEfirstname=4,
      OEbirth=5, OEsex=6, OEstart=9,  OEfinish=10, OEstatus=12,
      OEclubno=13, OEclub=14, OEclubcity=15, OEnat=16, OEclassno=17, OEclass=18, OEbib=23,
      OErent=35, OEfee=36, OEpaid=37, OEcourseno=38, OEcourse=39,
      OElength=40};

  list< vector<wstring> > allLines;
  parse(file, allLines);
  list< vector<wstring> >::iterator it = allLines.begin();

  set<wstring> matchedClasses;
  // Skip first line
  nimport=0;
  while (++it != allLines.end()) {
    const vector<wstring> &sp = *it;
    if (sp.size()>20) {
      nimport++;

      int clubId = wtoi(sp[OEclubno]);
      wstring clubName;
      wstring shortClubName;
      //string clubCity;

      clubName = sp[OEclubcity];
      shortClubName = sp[OEclub];

      if (clubName.empty() && !shortClubName.empty())
        swap(clubName, shortClubName);
    
      pClub pclub = event.getClubCreate(clubId, clubName);

      if (pclub) {
        if (sp[OEnat].length()>0)
          pclub->getDI().setString("Nationality", sp[OEnat]);

        pclub->getDI().setString("ShortName", shortClubName.substr(0, 8));
        pclub->setExtIdentifier(clubId);
        pclub->synchronize(true);
      }

      __int64 extId = oBase::converExtIdentifierString(sp[OEid]);
      int id = oBase::idFromExtId(extId);
      pRunner pr = 0;

      if (id>0)
        pr = event.getRunner(id, 0);

      while (pr) { // Check that the exact match is OK
        if (extId == pr->getExtIdentifier())
          break;
        id++;
        pr = event.getRunner(id, 0);
      }

      if (pr) {
        if (pr->getEntrySource() != externalSourceId) {
          // If not same source, do not accept match (will not work with older versions of MeOS files)
          pr = 0;
          id = 0;
        }
      }

      const bool newEntry = (pr == 0);

      if (pr == 0) {
        if (id==0) {
          oRunner r(&event);
          pr = event.addRunner(r, true);
        }
        else {
          oRunner r(&event, id);
          pr = event.addRunner(r, true);
        }
      }

      if (pr==0)
        continue;

      pr->setExtIdentifier(extId);
      pr->setEntrySource(externalSourceId);

      if (!pr->hasFlag(oAbstractRunner::FlagUpdateName)) {
        wstring name = sp[OEsurname] + L", " + sp[OEfirstname];
        pr->setName(name, false);
      }
      pr->setClubId(pclub ? pclub->getId():0);
      pr->setCardNo( wtoi(sp[OEcard]), false );

      pr->setStartTime(event.convertAbsoluteTime(sp[OEstart]), true, oBase::ChangeType::Update);
      pr->setFinishTime(event.convertAbsoluteTime(sp[OEfinish]));

      if (sp[OEstatus].length()>0)
        pr->setStatus( ConvertOEStatus( wtoi(sp[OEstatus]) ), true, oBase::ChangeType::Update);

      if (pr->getStatus()==StatusOK && pr->getRunningTime(false)==0)
        pr->setStatus(StatusUnknown, true, oBase::ChangeType::Update);

      //Autocreate class if it does not exist...
      int classId=wtoi(sp[OEclassno]);
      if (classId>0 && !pr->hasFlag(oAbstractRunner::FlagUpdateClass)) {
        pClass pc=event.getClassCreate(classId, sp[OEclass], matchedClasses);

        if (pc) {
          pc->synchronize();
          if (pr->getClassId(false) == 0 || !pr->hasFlag(oAbstractRunner::FlagUpdateClass))
            pr->setClassId(pc->getId(), false);
        }
      }
      int stno=wtoi(sp[OEstno]);
      bool needSno = pr->getStartNo() == 0 || newEntry;
      bool needBib = pr->getBib().empty();
      
      if (needSno || newEntry) {
        if (stno>0)
          pr->setStartNo(stno, oBase::ChangeType::Update);
        else
          pr->setStartNo(nimport, oBase::ChangeType::Update);
      }
      oDataInterface DI=pr->getDI();

      pr->setSex(interpretSex(sp[OEsex]));
      DI.setInt("BirthYear", extendYear(wtoi(sp[OEbirth])));
      DI.setString("Nationality", sp[OEnat]);

      if (sp.size()>OEbib && needBib)
        pr->setBib(sp[OEbib], 0, false);

      if (sp.size()>=38) {//ECO
        DI.setInt("Fee", wtoi(sp[OEfee]));
        DI.setInt("CardFee", wtoi(sp[OErent]));
        DI.setInt("Paid", wtoi(sp[OEpaid]));
      }

      if (sp.size()>=40) {//Course
        if (pr->getCourse(false) == 0) {
          const wstring &cid=sp[OEcourseno];
          const int courseid=wtoi(cid);
          if (courseid>0) {
            pCourse course=event.getCourse(courseid);

            if (!course) {
              oCourse oc(&event, courseid);
              oc.setLength(int(_wtof(sp[OElength].c_str())*1000));
              oc.setName(sp[OEcourse]);
              course = event.addCourse(oc);
              if (course)
                course->synchronize();
            }
            if (course) {
              if (pr->getClassId(false) != 0)
                event.getClass(pr->getClassId(false))->setCourse(course);
              else
                pr->setCourseId(course->getId());
            }
          }
        }
      }
      if (pr)
        pr->synchronize();
    }
  }
  fin.close();

  return true;
}

bool csvparser::openOutput(const wstring &filename, bool writeUTF)
{
  checkWriteAccess(filename);
  fout.open(filename);

  if (fout.bad())
    return false;

  if (writeUTF) {
    fout.put(-17);
    fout.put(-69);
    fout.put(-65);
  }
  return true;
}

bool csvparser::outputRow(const string &row)
{
  fout << row << endl;
  return true;
}
bool csvparser::outputRow(const vector<string> &out)
{
  int size=out.size();

  for(int i=0;i<size;i++){
    string p=out[i];

    //Replace " with '
    int found=p.find_first_of("\"");
    while (found!=string::npos){
      p[found]='\'';
      found=p.find_first_of("\"",found+1);
    }

    if (i>0) fout << ";";

    if (p.find_first_of("; ,\t.")!=string::npos)
      fout << "\"" << p << "\"";
    else fout << p;
  }
  fout << endl;
  fout.flush();
  return true;
}

bool csvparser::closeOutput()
{
  fout.close();
  return true;
}


int csvparser::split(char *line, vector<char *> &split_vector)
{
  split_vector.clear();
  int len=strlen(line);
  bool cite=false;

  for(int m=0;m<len;m++)
  {
    char *ptr=&line[m];

    if (*ptr=='"')
      ptr++;

    while(line[m])
    {
      if (!cite && line[m]==';')
        line[m]=0;
      else
      {
        if (line[m]=='"')
        {
          cite=!cite;
          line[m]=0;
          if (cite)
            ptr=&line[m+1];
        }
        m++;
      }
    }
    line[m]=0;

    split_vector.push_back(ptr);

    //m++;
  }

  return 0;
}

int csvparser::split(wchar_t *line, vector<wchar_t *> &split_vector)
{
  split_vector.clear();
  int len=wcslen(line);
  bool cite=false;

  for(int m=0;m<len;m++)
  {
    wchar_t *ptr=&line[m];

    if (*ptr=='"')
      ptr++;

    while(line[m])
    {
      if (!cite && line[m]==';')
        line[m]=0;
      else
      {
        if (line[m]=='"')
        {
          cite=!cite;
          line[m]=0;
          if (cite)
            ptr=&line[m+1];
        }
        m++;
      }
    }
    line[m]=0;

    split_vector.push_back(ptr);
  }

  return 0;
}



bool csvparser::importOCAD_CSV(oEvent &event, const wstring &file, bool addClasses) {
  list< vector<wstring> > allLines;
  parse(file, allLines);
  list< vector<wstring> >::iterator it = allLines.begin();

  while(it != allLines.end()) {
    const vector<wstring> &sp = *it;
    ++it;

    if (sp.size()>7) {
      size_t firstIndex = 7;
      bool hasLengths = true;
      int offset = 0;
      if (wtoi(sp[firstIndex]) < 30) {
        firstIndex = 6;
        offset = -1;
      }

      if (wtoi(sp[firstIndex])<30 || wtoi(sp[firstIndex])>1000) {
        wstring str = L"Ogiltig banfil. Kontroll förväntad på position X, men hittade 'Y'.#"
                      + itow(firstIndex+1) + L"#" + sp[firstIndex];
        throw meosException(str.c_str());
      }

      while (firstIndex > 0 && wtoi(sp[firstIndex])>30 && wtoi(sp[firstIndex])>1000) {
        firstIndex--;
      }
      wstring Start = lang.tl(L"Start ") + L"1";
      double Length = 0;
      wstring Course = event.getAutoCourseName();
      wstring Class = L"";

      if (firstIndex>=6) {
        Class = sp[0];
        Course = sp[1+offset];
        Start = sp[5+offset];
        Length = _wtof(sp[3+offset].c_str());

        if (Start[0]=='S') {
          int num = wtoi(Start.substr(1));
          if (num>0)
            Start = lang.tl(L"Start ") + Start.substr(1);
        }
      }
      else
        hasLengths = false;

      //Get controls
      pCourse pc=event.getCourse(Course);

      if (!pc) {
        pc = event.addCourse(Course, int(Length*1000));
      }
      else {
        // Reset control
        pc->importControls("", true, false);
        pc->setLength(int(Length*1000));
      }


      vector<int> legLengths;

      if (hasLengths) {
        double legLen = _wtof(sp[firstIndex-1].c_str()); // Length in km
        if (legLen > 0.001 && legLen < 30)
          legLengths.push_back(int(legLen*1000));
      }

      if (pc) {
        for (size_t k=firstIndex; k<sp.size()-1;k+=(hasLengths ? 2 : 1)) {
          wstring ctrlStr = trim(sp[k]);
          if (!ctrlStr.empty()) {
            if (ctrlStr[0] == 'S')
              continue;

            int ctrl = wtoi(ctrlStr.c_str());

            if (ctrl == 0 && trim(sp[k+1]).length() == 0)
              break; // Done

            if (ctrl >= 30 && ctrl < 1000)
              pc->addControl(wtoi(sp[k]));
            else {
              wstring str = L"Oväntad kontroll 'X' i bana Y.#" + ctrlStr + L"#" + pc->getName();
              throw meosException(str);
            }
          }
          if (hasLengths) {
            double legLen = _wtof(sp[k+1].c_str()); // Length in km
            if (legLen > 0.001 && legLen < 30)
              legLengths.push_back(int(legLen*1000));
          }
        }
        pc->setLength(int(Length*1000));
        pc->setStart(Start, true);

        if (legLengths.size() == pc->getNumControls()+1)
          pc->setLegLengths(legLengths);

        if (!Class.empty() && addClasses) {
          pClass cls = event.getBestClassMatch(Class);
          if (!cls)
            cls = event.addClass(Class);

          if (cls->getNumStages()==0) {
            cls->setCourse(pc);
          }
          else {
            for (size_t i = 0; i<cls->getNumStages(); i++)
              cls->addStageCourse(i, pc->getId(), -1);
          }

          cls->synchronize();
        }

        pc->synchronize();
      }
    }
  }
  return true;
}


bool csvparser::importRAID(oEvent &event, const wstring &file)
{
  enum {RAIDid=0, RAIDteam=1, RAIDcity=2, RAIDedate=3, RAIDclass=4,
        RAIDclassid=5, RAIDrunner1=6, RAIDrunner2=7, RAIDcanoe=8};

  list< vector<wstring> > allLines;
  parse(file, allLines);

  set<wstring> matchedClasses;
  list< vector<wstring> >::iterator it = allLines.begin();
  nimport=0;
  while (++it != allLines.end()) {
    const vector<wstring> &sp = *it;

    if (sp.size()>7) {
      nimport++;

      int ClubId=0;
      //Create class with this class number...
      int ClassId=wtoi(sp[RAIDclassid]);
      pClass pc = event.getClassCreate(ClassId, sp[RAIDclass], matchedClasses);
      ClassId = pc->getId();

      //Club is autocreated...
      pTeam team=event.addTeam(sp[RAIDteam], ClubId,  ClassId);

      team->setStartNo(wtoi(sp[RAIDid]), oBase::ChangeType::Update);
      if (sp.size()>8)
        team->getDI().setInt("SortIndex", wtoi(sp[RAIDcanoe]));
      oDataInterface teamDI=team->getDI();
      teamDI.setDate("EntryDate", sp[RAIDedate]);
      
      if (pc) {
        if (pc->getNumStages()<2)
          pc->setNumStages(2);

        pc->setLegType(0, LTNormal);
        pc->setLegType(1, LTIgnore);
      }

      //Import runners!
      pRunner r1=event.addRunner(sp[RAIDrunner1], ClubId, ClassId, 0, 0, false);
      team->setRunner(0, r1, false);

      pRunner r2=event.addRunner(sp[RAIDrunner2], ClubId, ClassId, 0, 0, false);
      team->setRunner(1, r2, false);

      team->evaluate(oBase::ChangeType::Update);
    }
  }
  fin.close();

  return true;
}

int csvparser::selectPunchIndex(const wstring &competitionDate, const vector<wstring> &sp, 
                                int &cardIndex, int &timeIndex, int &dateIndex,
                                wstring &processedTime, wstring &processedDate) {
  int ci = -1;
  int ti = -1;
  int di = -1;
  wstring pt, date;
  int maxCardNo = 0;
  processedDate.clear();
  for (size_t k = 0; k < sp.size(); k++) {
    processGeneralTime(sp[k], pt, date);
    if (!pt.empty()) {
      //if (ti == -1) {
        ti = k;
        pt.swap(processedTime);
     // }
     // else {
    //    return -1; // Not a unique time
    //  }
      if (!date.empty()) {
        date.swap(processedDate);
        di = k;
      }
    }
    else if (k == 2 && sp[k].length() == 2 && processedDate.empty()) {
      processedDate = sp[k]; // Old weekday format
      dateIndex = 2;
    }
    else {
      int cno = wtoi(sp[k]);
      if (cno > maxCardNo) {
        maxCardNo = cno;
        ci = k;
      }  
    }
    
  }

  if (ti == -1)
    return 0; // No time found
  if (ci == -1)
    return 0; // No card number found

  if (timeIndex >= 0 && timeIndex != ti)
    return -1; // Inconsistent

  if (cardIndex >= 0 && cardIndex != ci)
    return -1; // Inconsistent

  timeIndex = ti;
  cardIndex = ci;
  dateIndex = di;
  return 1;
}

bool csvparser::importPunches(const oEvent &oe, const wstring &file, vector<PunchInfo> &punches)
{
  punches.clear();
/*  fin.clear();
  fin.open(file);
  if (!fin.good())
    return false;
    */
  list< vector<wstring> > allLines;
  parse(file, allLines);
  list< vector<wstring> >::iterator it = allLines.begin();


  //const size_t siz = 1024 * 1;
  //char bf[siz];
  //string bfs;

  //fin.getline(bf, siz);
  //std::getline(fin, bfs);
  
  nimport=0;
  int cardIndex = -1;
  int timeIndex = -1;
  int dateIndex = -1;

  wstring processedTime, processedDate;
  const wstring date = oe.getDate();
  //vector<char *> sp;
  while (++it != allLines.end()) {
    const vector<wstring> &sp = *it;
    //if (fin.fail())
    //  throw meosException("Reading file failed.");

    //fin.getline(bf, siz);
    //std::getline(fin, bfs);
    /*sp.clear();
    char *bf = (char *)bfs.c_str();
    split(bf, sp);
    */


    int ret = selectPunchIndex(date, sp, cardIndex, timeIndex, dateIndex,
                               processedTime, processedDate); 
    if (ret == -1)
      return false; // Invalid file
    if (ret > 0) {
      int card = wtoi(sp[cardIndex]);
      int time = oe.getRelativeTime(processedTime);

      if (card>0) {
        PunchInfo pi;
        pi.card = card;
        pi.time = time;
        string pd(processedDate.begin(), processedDate.end());
        strncpy_s(pi.date, sizeof(pi.date), pd.c_str(), 26);
        pi.date[26] = 0;
        punches.push_back(pi);
        nimport++;
      }
    }
  }
  fin.close();

  return true;
}

int analyseSITime(const wchar_t *dow, const wchar_t *time, bool &is12Hour)
{
  int t = oEvent::convertAbsoluteTime(time);
  if (t >= 0 && trim(dow).empty())
    is12Hour = true;
  return t;
}

void csvparser::checkSIConfigHeader(const vector<wstring> &sp) {
  siconfigmap.clear();
  if (sp.size() < 200)
    return;

  //No;Read on;SIID;Start no;Clear CN;Clear DOW;Clear time;Clear_r CN;Clear_r DOW;Clear_r time;Check CN;Check DOW;Check time;Start CN;Start DOW;Start time;Start_r CN;Start_r DOW;Start_r time;Finish CN;Finish DOW;Finish time;Finish_r CN;Finish_r DOW;Finish_r time;Class;First name;Last name;Club;Country;Email;Date of birth;Sex;Phone;Street;ZIP;City;Hardware version;Software version;Battery date;Battery voltage;Clear count;Character set;SEL_FEEDBACK;No. of records;Record 1 CN;Record 1 DOW;Record 1 time;Record 2 CN;Record 2 DOW;Record 2 time;Record 3 CN;Record 3 DOW;Record 3 time;Record 4 CN;Record 4 DOW;Record 4 time;Record 5 CN;Record 5 DOW;Record 5 time;Record 6 CN;Record 6 DOW;Record 6 time;Record 7 CN;Record 7 DOW;Record 7 time;Record 8 CN;Record 8 DOW;Record 8 time;Record 9 CN;Record 9 DOW;Record 9 time;Record 10 CN;Record 10 DOW;Record 10 time;Record 11 CN;Record 11 DOW;Record 11 time;Record 12 CN;Record 12 DOW;Record 12 time;Record 13 CN;Record 13 DOW;Record 13 time;Record 14 CN;Record 14 DOW;Record 14 time;Record 15 CN;Record 15 DOW;Record 15 time;Record 16 CN;Record 16 DOW;Record 16 time;Record 17 CN;Record 17 DOW;Record 17 time;Record 18 CN;Record 18 DOW;Record 18 time;Record 19 CN;Record 19 DOW;Record 19 time;Record 20 CN;Record 20 DOW;Record 20 time;Record 21 CN;Record 21 DOW;Record 21 time;Record 22 CN;Record 22 DOW;Record 22 time;Record 23 CN;Record 23 DOW;Record 23 time;Record 24 CN;Record 24 DOW;Record 24 time;Record 25 CN;Record 25 DOW;Record 25 time;Record 26 CN;Record 26 DOW;Record 26 time;Record 27 CN;Record 27 DOW;Record 27 time;Record 28 CN;Record 28 DOW;Record 28 time;Record 29 CN;Record 29 DOW;Record 29 time;Record 30 CN;Record 30 DOW;Record 30 time;Record 31 CN;Record 31 DOW;Record 31 time;Record 32 CN;Record 32 DOW;Record 32 time;Record 33 CN;Record 33 DOW;Record 33 time;Record 34 CN;Record 34 DOW;Record 34 time;Record 35 CN;Record 35 DOW;Record 35 time;Record 36 CN;Record 36 DOW;Record 36 time;Record 37 CN;Record 37 DOW;Record 37 time;Record 38 CN;Record 38 DOW;Record 38 time;Record 39 CN;Record 39 DOW;Record 39 time;Record 40 CN;Record 40 DOW;Record 40 time;Record 41 CN;Record 41 DOW;Record 41 time;Record 42 CN;Record 42 DOW;Record 42 time;Record 43 CN;Record 43 DOW;Record 43 time;Record 44 CN;Record 44 DOW;Record 44 time;Record 45 CN;Record 45 DOW;Record 45 time;Record 46 CN;Record 46 DOW;Record 46 time;Record 47 CN;Record 47 DOW;Record 47 time;Record 48 CN;Record 48 DOW;Record 48 time;Record 49 CN;Record 49 DOW;Record 49 time;Record 50 CN;Record 50 DOW;Record 50 time;Record 51 CN;Record 51 DOW;Record 51 time;Record 52 CN;Record 52 DOW;Record 52 time;Record 53 CN;Record 53 DOW;Record 53 time;Record 54 CN;Record 54 DOW;Record 54 time;Record 55 CN;Record 55 DOW;Record 55 time;Record 56 CN;Record 56 DOW;Record 56 time;Record 57 CN;Record 57 DOW;Record 57 time;Record 58 CN;Record 58 DOW;Record 58 time;Record 59 CN;Record 59 DOW;Record 59 time;Record 60 CN;Record 60 DOW;Record 60 time;Record 61 CN;Record 61 DOW;Record 61 time;Record 62 CN;Record 62 DOW;Record 62 time;Record 63 CN;Record 63 DOW;Record 63 time;Record 64 CN;Record 64 DOW;Record 64 time;Record 65 CN;Record 65 DOW;Record 65 time;Record 66 CN;Record 66 DOW;Record 66 time;Record 67 CN;Record 67 DOW;Record 67 time;Record 68 CN;Record 68 DOW;Record 68 time;Record 69 CN;Record 69 DOW;Record 69 time;Record 70 CN;Record 70 DOW;Record 70 time;Record 71 CN;Record 71 DOW;Record 71 time;Record 72 CN;Record 72 DOW;Record 72 time;Record 73 CN;Record 73 DOW;Record 73 time;Record 74 CN;Record 74 DOW;Record 74 time;Record 75 CN;Record 75 DOW;Record 75 time;Record 76 CN;Record 76 DOW;Record 76 time;Record 77 CN;Record 77 DOW;Record 77 time;Record 78 CN;Record 78 DOW;Record 78 time;Record 79 CN;Record 79 DOW;Record 79 time;Record 80 CN;Record 80 DOW;Record 80 time;Record 81 CN;Record 81 DOW;Record 81 time;Record 82 CN;Record 82 DOW;Record 82 time;Record 83 CN;Record 83 DOW;Record 83 time;Record 84 CN;Record 84 DOW;Record 84 time;Record 85 CN;Record 85 DOW;Record 85 time;Record 86 CN;Record 86 DOW;Record 86 time;Record 87 CN;Record 87 DOW;Record 87 time;Record 88 CN;Record 88 DOW;Record 88 time;Record 89 CN;Record 89 DOW;Record 89 time;Record 90 CN;Record 90 DOW;Record 90 time;Record 91 CN;Record 91 DOW;Record 91 time;Record 92 CN;Record 92 DOW;Record 92 time;Record 93 CN;Record 93 DOW;Record 93 time;Record 94 CN;Record 94 DOW;Record 94 time;Record 95 CN;Record 95 DOW;Record 95 time;Record 96 CN;Record 96 DOW;Record 96 time;Record 97 CN;Record 97 DOW;Record 97 time;Record 98 CN;Record 98 DOW;Record 98 time;Record 99 CN;Record 99 DOW;Record 99 time;Record 100 CN;Record 100 DOW;Record 100 time;Record 101 CN;Record 101 DOW;Record 101 time;Record 102 CN;Record 102 DOW;Record 102 time;Record 103 CN;Record 103 DOW;Record 103 time;Record 104 CN;Record 104 DOW;Record 104 time;Record 105 CN;Record 105 DOW;Record 105 time;Record 106 CN;Record 106 DOW;Record 106 time;Record 107 CN;Record 107 DOW;Record 107 time;Record 108 CN;Record 108 DOW;Record 108 time;Record 109 CN;Record 109 DOW;Record 109 time;Record 110 CN;Record 110 DOW;Record 110 time;Record 111 CN;Record 111 DOW;Record 111 time;Record 112 CN;Record 112 DOW;Record 112 time;Record 113 CN;Record 113 DOW;Record 113 time;Record 114 CN;Record 114 DOW;Record 114 time;Record 115 CN;Record 115 DOW;Record 115 time;Record 116 CN;Record 116 DOW;Record 116 time;Record 117 CN;Record 117 DOW;Record 117 time;Record 118 CN;Record 118 DOW;Record 118 time;Record 119 CN;Record 119 DOW;Record 119 time;Record 120 CN;Record 120 DOW;Record 120 time;Record 121 CN;Record 121 DOW;Record 121 time;Record 122 CN;Record 122 DOW;Record 122 time;Record 123 CN;Record 123 DOW;Record 123 time;Record 124 CN;Record 124 DOW;Record 124 time;Record 125 CN;Record 125 DOW;Record 125 time;Record 126 CN;Record 126 DOW;Record 126 time;Record 127 CN;Record 127 DOW;Record 127 time;Record 128 CN;Record 128 DOW;Record 128 time;Record 129 CN;Record 129 DOW;Record 129 time;Record 130 CN;Record 130 DOW;Record 130 time;Record 131 CN;Record 131 DOW;Record 131 time;Record 132 CN;Record 132 DOW;Record 132 time;Record 133 CN;Record 133 DOW;Record 133 time;Record 134 CN;Record 134 DOW;Record 134 time;Record 135 CN;Record 135 DOW;Record 135 time;Record 136 CN;Record 136 DOW;Record 136 time;Record 137 CN;Record 137 DOW;Record 137 time;Record 138 CN;Record 138 DOW;Record 138 time;Record 139 CN;Record 139 DOW;Record 139 time;Record 140 CN;Record 140 DOW;Record 140 time;Record 141 CN;Record 141 DOW;Record 141 time;Record 142 CN;Record 142 DOW;Record 142 time;Record 143 CN;Record 143 DOW;Record 143 time;Record 144 CN;Record 144 DOW;Record 144 time;Record 145 CN;Record 145 DOW;Record 145 time;Record 146 CN;Record 146 DOW;Record 146 time;Record 147 CN;Record 147 DOW;Record 147 time;Record 148 CN;Record 148 DOW;Record 148 time;Record 149 CN;Record 149 DOW;Record 149 time;Record 150 CN;Record 150 DOW;Record 150 time;Record 151 CN;Record 151 DOW;Record 151 time;Record 152 CN;Record 152 DOW;Record 152 time;Record 153 CN;Record 153 DOW;Record 153 time;Record 154 CN;Record 154 DOW;Record 154 time;Record 155 CN;Record 155 DOW;Record 155 time;Record 156 CN;Record 156 DOW;Record 156 time;Record 157 CN;Record 157 DOW;Record 157 time;Record 158 CN;Record 158 DOW;Record 158 time;Record 159 CN;Record 159 DOW;Record 159 time;Record 160 CN;Record 160 DOW;Record 160 time;Record 161 CN;Record 161 DOW;Record 161 time;Record 162 CN;Record 162 DOW;Record 162 time;Record 163 CN;Record 163 DOW;Record 163 time;Record 164 CN;Record 164 DOW;Record 164 time;Record 165 CN;Record 165 DOW;Record 165 time;Record 166 CN;Record 166 DOW;Record 166 time;Record 167 CN;Record 167 DOW;Record 167 time;Record 168 CN;Record 168 DOW;Record 168 time;Record 169 CN;Record 169 DOW;Record 169 time;Record 170 CN;Record 170 DOW;Record 170 time;Record 171 CN;Record 171 DOW;Record 171 time;Record 172 CN;Record 172 DOW;Record 172 time;Record 173 CN;Record 173 DOW;Record 173 time;Record 174 CN;Record 174 DOW;Record 174 time;Record 175 CN;Record 175 DOW;Record 175 time;Record 176 CN;Record 176 DOW;Record 176 time;Record 177 CN;Record 177 DOW;Record 177 time;Record 178 CN;Record 178 DOW;Record 178 time;Record 179 CN;Record 179 DOW;Record 179 time;Record 180 CN;Record 180 DOW;Record 180 time;Record 181 CN;Record 181 DOW;Record 181 time;Record 182 CN;Record 182 DOW;Record 182 time;Record 183 CN;Record 183 DOW;Record 183 time;Record 184 CN;Record 184 DOW;Record 184 time;Record 185 CN;Record 185 DOW;Record 185 time;Record 186 CN;Record 186 DOW;Record 186 time;Record 187 CN;Record 187 DOW;Record 187 time;Record 188 CN;Record 188 DOW;Record 188 time;Record 189 CN;Record 189 DOW;Record 189 time;Record 190 CN;Record 190 DOW;Record 190 time;Record 191 CN;Record 191 DOW;Record 191 time;Record 192 CN;Record 192 DOW;Record 192 time;
  wstring key;
  for (size_t k = 0; k < sp.size(); k++) {
    key = sp[k];
    if (key == L"SIID" || key == L"SI-Card")
      siconfigmap[sicSIID] = k;
    else if (key == L"Check CN" || key == L"CHK_CN") {
      siconfigmap[sicCheck] = k;
      siconfigmap[sicCheckDOW] = k + 1;
      siconfigmap[sicCheckTime] = k + 2;
    }
    else if (key == L"Check time") {
      siconfigmap[sicCheckTime] = k;
    }
    else if (key == L"Start CN" || key == L"ST_CN") {
      siconfigmap[sicStart] = k;
      siconfigmap[sicStartDOW] = k + 1;
      siconfigmap[sicStartTime] = k + 2;
    }
    else if (key == L"Start time") {
      siconfigmap[sicStartTime] = k;
    }
    else if (key == L"Finish CN" || key == L"FI_CN") {
      siconfigmap[sicFinish] = k;
      siconfigmap[sicFinishDOW] = k + 1;
      siconfigmap[sicFinishTime] = k + 2;
    }
    else if (key == L"Finish time") {
      siconfigmap[sicFinishTime] = k;
    }
    else if (key == L"First name") {
      siconfigmap[sicFirstName] = k;
    }
    else if (key == L"Last name" || key == L"name") {
      siconfigmap[sicLastName] = k;
    }
    else if (key == L"No. of records" || key == L"No. of punches") {
      siconfigmap[sicNumPunch] = k;
    }
    else if (siconfigmap.count(sicRecordStart) == 0) {
      size_t pos = key.find(L"Record 1");
      if (pos == string::npos) {
        pos = key.find(L"1.CN");
      }
      if (pos != string::npos) {
        siconfigmap[sicRecordStart] = k;
        if (siconfigmap.count(sicRecordStart) == 0 && k > 0)
          siconfigmap[sicNumPunch] = k-1;
      }
    }
  }

  if (!siconfigmap.empty()) {

  }
}

const wchar_t *csvparser::getSIC(SIConfigFields sic, const vector<wstring> &sp) const {
  map<SIConfigFields, int>::const_iterator res = siconfigmap.find(sic);
  if (res == siconfigmap.end() || size_t(res->second) >= sp.size())
    return L"";

  return sp[res->second].c_str();
}

bool csvparser::checkSIConfigLine(const oEvent &oe, const vector<wstring> &sp, SICard &card) {
  if (siconfigmap.empty())
    return false;

  int startIx = siconfigmap[sicRecordStart];
  if (startIx == 0)
    return false;

  int cardNo = wtoi(getSIC(sicSIID, sp));

  if (cardNo < 1000 || cardNo > 99999999)
    return false;
  bool is12Hour = false;
  int check = analyseSITime(getSIC(sicCheckDOW, sp), getSIC(sicCheckTime, sp), is12Hour);
  int start = analyseSITime(getSIC(sicStartDOW, sp), getSIC(sicStartTime, sp), is12Hour);
  int finish = analyseSITime(getSIC(sicFinishDOW, sp), getSIC(sicFinishTime, sp), is12Hour);
  int startCode = wtoi(getSIC(sicStart, sp));
  int finishCode = wtoi(getSIC(sicFinish, sp));
  int checkCode = wtoi(getSIC(sicCheck, sp));
  wstring fname = getSIC(sicFirstName, sp);
  wstring lname = getSIC(sicLastName, sp);
  
  vector< pair<int, int> > punches;
  int np = wtoi(getSIC(sicNumPunch, sp));


  for (int k = 0; k < np; k++) {
    size_t ix = startIx + k * 3;
    if (ix + 2 >= sp.size())
      return false;
    int code = wtoi(sp[ix]);
    int time = analyseSITime(sp[ix + 1].c_str(), sp[ix + 2].c_str(), is12Hour);
    if (code > 0) {
      punches.push_back(make_pair(code, time));
    }
    else {
      return false;
    }
  }
 
  if ( (finish > 0 && finish != NOTIME) || punches.size() > 2) {
    card.clear(0);
    card.CardNumber = cardNo;
    if (start > 0 && start != NOTIME) {
      card.StartPunch.Code = startCode;
      card.StartPunch.Time = start;
    }
    else {
      card.StartPunch.Time = 0;
      card.StartPunch.Code = -1;
    }

    if (finish > 0 && finish != NOTIME) {
      card.FinishPunch.Code = finishCode;
      card.FinishPunch.Time = finish;
    }
    else {
      card.FinishPunch.Time = 0;
      card.FinishPunch.Code = -1;
    }

    if (check > 0 && check != NOTIME) {
      card.CheckPunch.Code = checkCode;
      card.CheckPunch.Time = check;
    }
    else {
      card.CheckPunch.Time = 0;
      card.CheckPunch.Code = -1;
    }

    for (size_t k = 0; k<punches.size(); k++) {
      card.Punch[k].Code = punches[k].first;
      card.Punch[k].Time = punches[k].second;
    }
    
    wcsncpy_s(card.firstName, fname.c_str(), 20);
    card.firstName[20] = 0;
    wcsncpy_s(card.lastName, lname.c_str(), 20);
    card.lastName[20] = 0;
    card.nPunch = punches.size();
    card.convertedTime = is12Hour ? ConvertedTimeStatus::Hour12 :  ConvertedTimeStatus::Hour24;
    return true;
  }

  return false;
}


bool csvparser::checkSimanLine(const oEvent &oe, const vector<wstring> &sp, SICard &card) {
  if (sp.size() <= 11)
    return false;

  int cardNo = wtoi(sp[1]);
  
  if (wcschr(sp[1].c_str(), '-') != 0)
    cardNo = 0; // Ensure not a date 2017-02-14

  if (cardNo < 1000 || cardNo > 99999999)
    return false;

  int start = convertAbsoluteTimeMS(sp[5]);
  int finish = convertAbsoluteTimeMS(sp[6]);
  vector< pair<int, int> > punches;
  for (size_t k=10; k + 1<sp.size(); k+=2) {
    int code = wtoi(sp[k]);
    int time = convertAbsoluteTimeMS(sp[k+1]);
    if (code > 0) {
      punches.push_back(make_pair(code, time));
    }
    else {
      return false;
    }
  }

  if (punches.size() > 2) {
    card.clear(0);
    card.CardNumber = cardNo;
    if (start > 0) {
      card.StartPunch.Code = 0;
      card.StartPunch.Time = start;
    }
    else {
      card.StartPunch.Code = -1;
    }

    if (finish > 0) {
      card.FinishPunch.Code = 0;
      card.FinishPunch.Time = finish;
    }
    else {
      card.FinishPunch.Code = -1;
    }

    for (size_t k = 0; k<punches.size(); k++) {
      card.Punch[k].Code = punches[k].first;
      card.Punch[k].Time = punches[k].second;
    }
    card.nPunch = punches.size();
    card.convertedTime = ConvertedTimeStatus::Hour24; //XXX Not correct in general
    return true;
  }

  return false;
}

bool csvparser::importCards(const oEvent &oe, const wstring &file, vector<SICard> &cards)
{
  cards.clear();
  list< vector<wstring> > allLines;
  parse(file, allLines);
  //vector<wchar_t *> sp;
  list< vector<wstring> >::iterator it = allLines.begin();
  if (it == allLines.end())
    return false;
//  fin.clear();
//  fin.open(file);

//  if (!fin.good())
//    return false;

  //[1024*16];
  //int s = 1024*256;
  //vector<char> bbf(s);
  //char *bf = &bbf[0];
  //fin.getline(bf, s);
  //vector<char *> sp;
  //split(bf, sp);
  checkSIConfigHeader(*it);
  nimport=0;
  
  while (++it != allLines.end()) {
    //fin.getline(bf, s);
    //split(bf, sp);
    const vector<wstring> &sp = *it;
    
    SICard card(ConvertedTimeStatus::Unknown);

    if (checkSimanLine(oe, sp, card)) {
      cards.push_back(card);
      nimport++;
    }
    else if (checkSIConfigLine(oe,sp, card)) {
      cards.push_back(card);
      nimport++;
    }
    else if (sp.size()>28) {
      int no = wtoi(sp[0]);
      card.CardNumber = wtoi(sp[2]);
      wcsncpy_s(card.firstName, sp[5].c_str(), 20);
      wcsncpy_s(card.lastName, sp[6].c_str(), 20);
      wcsncpy_s(card.club, sp[7].c_str(), 40);
      bool hour12 = false;
      if (trim(sp[21]).length()>1) {
        card.CheckPunch.Code = wtoi(sp[19]);
        card.CheckPunch.Time = analyseSITime(sp[20].c_str(), sp[21].c_str(), hour12);
      }
      else {
        card.CheckPunch.Code = -1;
        card.CheckPunch.Time = 0;
      }

      if (trim(sp[24]).length()>1) {
        card.StartPunch.Code = wtoi(sp[22]);
        card.StartPunch.Time = analyseSITime(sp[23].c_str(), sp[24].c_str(), hour12);
      }
      else {
        card.StartPunch.Code = -1;
        card.StartPunch.Time = 0;
      }

      if (trim(sp[27]).length()>1) {
        card.FinishPunch.Code = wtoi(sp[25]);
        card.FinishPunch.Time = analyseSITime(sp[26].c_str(), sp[27].c_str(), hour12);
      }
      else  {
        card.FinishPunch.Code = -1;
        card.FinishPunch.Time = 0;
      }

      card.nPunch = wtoi(sp[28]);
      if (no>0 && card.CardNumber>0 && card.nPunch>0 && card.nPunch < 200) {
        if (sp.size()>28+3*card.nPunch) {
          for (unsigned k=0;k<card.nPunch;k++) {
            card.Punch[k].Code = wtoi(sp[29+k*3]);
            card.Punch[k].Time = analyseSITime(sp[30+k*3].c_str(), sp[31+k*3].c_str(), hour12);
          }
          card.punchOnly = false;
          nimport++;
          card.convertedTime = hour12 ? ConvertedTimeStatus::Hour12 : ConvertedTimeStatus::Hour24;
          cards.push_back(card);
        }
      }
    }
  }
  fin.close();

  return true;
}

void csvparser::parseUnicode(const wstring &file, list< vector<wstring> > &data) {
  fin.open(file, ifstream::in | ifstream::binary);
  fin.seekg(0, ios_base::end);
  int len = int(fin.tellg())-2;
  if (len <= 0)
    return;
  fin.seekg(2); // BOM
  assert(len % 2 == 0);
  vector<wchar_t> bf(len / 2 + 1);
  fin.read((char *)&bf[0], len);
  vector<wstring> rows;
  int spp = 0;
  for (int k = 0; k < len / 2; k++) {
    if (bf[spp] == '\r')
      spp++;
    if (bf[k] == '\n') {
      bf[k] = 0;
      if (k > 0 && bf[k - 1] == '\r')
        bf[k - 1] = 0;
      wstring r = &bf[spp];
      spp = k + 1;
      rows.push_back(r);
    }
  }
  if (spp + 1 < len / 2) {
    wstring r = &bf[spp];
    rows.push_back(r);
  }
  vector<wchar_t *> sp;

  for (size_t k = 0; k < rows.size(); k++) {
    split(const_cast<wchar_t*>(rows[k].c_str()), sp);
  
    if (!sp.empty()) {
      data.push_back(vector<wstring>());
      data.back().resize(sp.size());
      for (size_t k = 0; k < sp.size(); k++) {
        data.back()[k] = sp[k];
      }
    }
  }
}

void csvparser::parse(const wstring &file, list< vector<wstring> > &data) {
  data.clear();

  fin.open(file);
  
  fin.seekg(0, ios_base::end);
  auto flen = fin.tellg();
  fin.seekg(0);
  
  string rbf;

  if (!fin.good())
    throw meosException("Failed to read file");

  bool isUTF8 = false;
  bool firstLine = true;
  vector<wchar_t *> sp;
  vector<wchar_t> wbf_a;
  wbf_a.resize(size_t(flen)+1);
  wchar_t *wbf = &wbf_a[0];
  wstring w;
  while(std::getline(fin, rbf)) {
    const char *bf = rbf.c_str();
    if (firstLine) {
      if (rbf.length() > 3 && rbf[0] == -17 && rbf[1] == -69 && rbf[2] == -65) {
        isUTF8 = true;
        bf += 3;
      }
      else if (bf[0] == -1 && bf[1] == -2) {
        fin.close();
        vector<wchar_t>().swap(wbf_a);
        parseUnicode(file, data);
        return;
      }
    }
   
    if (isUTF8) {
      int len = strlen(bf);
      int wlen = MultiByteToWideChar(CP_UTF8, 0, bf, len, wbf, buff_pre_alloc);
      wbf[wlen] = 0;
      split(&wbf[0], sp);
    }
    else {
      w = gdi_main->recodeToWide(bf);
      wchar_t *wbfL = const_cast<wchar_t *>(w.c_str());
      split(wbfL, sp);
    }
    firstLine = false;
    
    if (!sp.empty()) {
      data.push_back(vector<wstring>());
      data.back().resize(sp.size());
      for (size_t k = 0; k < sp.size(); k++) {
        data.back()[k] = sp[k];
      }
    }
  }

  fin.close();
}

void csvparser::convertUTF(const wstring &file) {
  ifstream fin;
  fin.open(file);
  string rbf;

  if (!fin.good())
    throw meosException("Failed to read file");

  bool isUTF8 = false;
  bool firstLine = true;
  wstring w;
  vector<string> out;
  while (std::getline(fin, rbf)) {
    const char *bf = rbf.c_str();
    if (firstLine) {
      if (rbf.length() > 3 && rbf[0] == -17 && rbf[1] == -69 && rbf[2] == -65) {
        isUTF8 = true;
        bf += 3;
        return; // Done
      }
    }
    w = gdi_main->recodeToWide(bf);
    out.push_back(gdi_main->toUTF8(w));
  }
  
  fin.close();
  _wrename(file.c_str(), (file + L"_").c_str());

  ofstream fout;
  fout.open(file);
  fout.put(-17);
  fout.put(-69);
  fout.put(-65);
  for (string &line : out)
    fout << line << endl;
}

void csvparser::importTeamLineup(const wstring &file,
                                 const map<wstring, int> &classNameToNumber,
                                 vector<TeamLineup> &teams) {
  list< vector<wstring> > data;
  parse(file, data);
  teams.clear();
  teams.reserve(data.size()/3);
  int membersToRead = 0;
  int lineNo = 1;
  while (!data.empty()) {
    vector<wstring> &line = data.front();
    if (!line.empty()) {
      if (membersToRead == 0) {
        if (line.size() < 2 || line.size() > 3)
          throw meosException(L"Ogiltigt lag på rad X.#" + itow(lineNo) + L": " + line[0]);
        const wstring cls = trim(line[0]);
        map<wstring, int>::const_iterator res = classNameToNumber.find(cls);
        if (res == classNameToNumber.end())
          throw meosException(L"Okänd klass på rad X.#" + itow(lineNo) + L": " + cls);
        if (res->second <= 1)
          throw meosException(L"Klassen X är individuell.#" + cls);

        membersToRead = res->second;
        teams.push_back(TeamLineup());
        teams.back().teamClass = cls;
        teams.back().teamName = trim(line[1]);
        if (line.size() >= 3)
          teams.back().teamClub = trim(line[2]);
      }
      else {
        membersToRead--;
        teams.back().members.push_back(TeamLineup::TeamMember());
        TeamLineup::TeamMember &member = teams.back().members.back();
        member.name = trim(line[0]);
        if (line.size()>1)
          member.cardNo = _wtoi(line[1].c_str());
        else
          member.cardNo = 0;

        if (line.size()>2)
          member.club = trim(line[2]);

        if (line.size() > 3)
          member.course = trim(line[3]);

        if (line.size() > 4)
          member.cls = trim(line[4]);
      }
    }
    else if (membersToRead>0) {
      membersToRead--;
      teams.back().members.push_back(TeamLineup::TeamMember());
    }
    lineNo++;
    data.pop_front();
  }
}

int csvparser::importRanking(oEvent &oe, const wstring &file, vector<wstring> &problems) {
  list< vector<wstring> > data;
  parse(file, data);

  size_t idIx = -1;
  size_t nameIx = 1;
  size_t lastNameIx = -1;
  size_t rankIx = -1;
  map<int64_t, pair<wstring, int> > id2Rank;
  map<wstring, pair<int, bool> > name2RankDup;
  bool first = true;
  for (auto &rank : data) {
    if (first) {
      first = false;
      bool any = false;

      for (size_t i = 0; i < rank.size(); i++) {
        wstring s = canonizeName(rank[i].c_str());
        
        if (s.find(L"name")  != wstring::npos && (s.find(L"last") != wstring::npos || s.find(L"family") != wstring::npos) && lastNameIx == -1) {
          lastNameIx = i;
          any = true;
        }
        else if (s.find(L"name") != wstring::npos && (s.find(L"first") != wstring::npos || s.find(L"given") != wstring::npos) && nameIx == -1) {
          nameIx = i;
          any = true;
        }
        else if (s.find(L" id") != wstring::npos && idIx == -1) {
          idIx = i;
          any = true;
        }
        else if (s.find(L"position") != wstring::npos && rankIx == -1) {
          rankIx = i;
          any = true;
        }
      }

      if (idIx == -1)
        idIx = 0;

      if (nameIx == -1)
        nameIx = 1;

      if (lastNameIx == -1)
        lastNameIx = 2;

      if (rankIx == -1)
        rankIx = 4;

      if (any)
        continue;
    }
    
    if (rank.size() <= rankIx)
      continue;

    int rpos = _wtoi(rank[rankIx].c_str());
    if (rpos <= 0)
      continue;

    wstring name;
    if (nameIx < rank.size()) {
      name = rank[nameIx];

      if (lastNameIx < rank.size()) {
        name += L" " + rank[lastNameIx];
      }
    }
    if (name.empty())
      continue;

    auto res = name2RankDup.emplace(name, make_pair(rpos, false));

    if (!res.second)
      res.first->second.second = true; // Duplicate names

    if (idIx < rank.size()) {
      int64_t id = oBase::converExtIdentifierString(rank[idIx]);
      if (id != 0)
        id2Rank[id] = make_pair(name, rpos);
    }
  }

  if ((name2RankDup.size() < data.size() / 2 || name2RankDup.empty()) &&
    (id2Rank.size() < data.size() / 2 || id2Rank.empty())) {
    throw meosException(L"Felaktigt rankingformat i X. Förväntat: Y#" + file + L"#ID; First name; Last name; Rank");
  }

  vector<pRunner> runners;
  oe.getRunners(-1, -1, runners);
  
  int count = 0;
  vector<pRunner> remRunners;
  for (pRunner r : runners) {
    int64_t id = r->getExtIdentifier();
    auto res = id2Rank.find(id);
    if (res != id2Rank.end() && r->matchName(res->second.first)) {
      r->getDI().setInt("Rank", res->second.second);
      r->synchronize(true);
      count++;
    }
    else
      remRunners.push_back(r);
  }

  for (pRunner r : remRunners) {
    auto res = name2RankDup.find(r->getName());
    if (r->getRaceNo() > 0)
      continue;
    if (res != name2RankDup.end()) {
      if (res->second.second)
        problems.push_back(r->getCompleteIdentification());
      else {
        res->second.second = true;
        r->getDI().setInt("Rank", res->second.first);
        r->synchronize(true);
        count++;
      }
    }
  }

  return count;
}
