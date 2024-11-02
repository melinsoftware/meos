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

// oEvent.cpp: implementation of the oEvent class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <vector>
#include <algorithm>

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "meosexception.h"
#include "inthashmap.h"

#include "oDataContainer.h"
#include "csvparser.h"
#include "oFreeImport.h"

#include "random.h"
#include "SportIdent.h"
#include "RunnerDB.h"
#include "meos_util.h"
#include "meos.h"
#include "importformats.h"

#include <io.h>
#include <fcntl.h>
#include "localizer.h"
#include "iof30interface.h"
#include "gdiconstants.h"

#include "MeosSQL.h"

FlowOperation importFilterGUI(oEvent *oe,
                              gdioutput & gdi,
                              const set<int>& stages,
                              const vector<string> &idProviders,
                              set<int> & filter,
                              pair<string, string> &preferredIdProvider);

string conv_is(int i) {
  char bf[256];
  if (_itoa_s(i, bf, 10)==0)
    return bf;
  return "";
}

int ConvertStatusToOE(int i)
{
  switch(i)
  {
      case StatusOK:
      case StatusNoTiming:
      return 0;
      case StatusDNS:  // Ej start
      case StatusCANCEL:
      case StatusNotCompetiting:
      case StatusOutOfCompetition:
      return 1;
      case StatusDNF:  // Utg.
      return 2;
      case StatusMP:  // Felst.
      return 3;
      case StatusDQ: //Disk
      return 4;
      case StatusMAX: //Maxtid
      return 5;
  }
  return 1;//Ej start...?!
}

wstring &getFirst(wstring &inout, int maxNames) {
  int s = inout.size();
  for (int k = 0;k<s; k++) {
    if (inout[k] == ' ' && k>3 && maxNames<=1) {
      inout[k] = 0;
      maxNames--;
      return inout;
    }
  }
  return inout;
}

bool oEvent::exportOECSV(const wchar_t *file, const set<int>& classes, int languageTypeIndex, bool includeSplits)
{
  enum {
    OEstno = 0, OEcard = 1, OEid = 2, OEsurname = 3, OEfirstname = 4,
    OEbirth = 5, OEsex = 6, OEnc = 8, OEstart = 9, OEfinish = 10, OEtime = 11, OEstatus = 12,
    OEclubno = 13, OEclub = 14, OEclubcity = 15, OEnat = 16, OEclassno = 17,
    OEclassshortname = 18, OEclassname = 19, OErent = 35, OEfee = 36, OEpaid = 37, OEcourseno = 38, OEcourse = 39,
    OElength = 40, OEclimb = 41, OEcoursecontrols = 42, OEpl = 43, OEstartpunch = 44, OEfinishpunch = 45
  };

  csvparser csv;

  oClass::initClassId(*this, classes);

  if (!csv.openOutput(file))
    return false;

  calculateResults(classes, ResultType::ClassResult);
  sortRunners(SortOrder::ClassResult);
  oRunnerList::iterator it;
  string maleString;
  string femaleString;

  switch (languageTypeIndex)
  {
  case 1: // English
    csv.outputRow("Stno;Chip;Database Id;Surname;First name;YB;S;Block;nc;Start;Finish;Time;Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;Num1;Num2;Num3;Text1;Text2;Text3;Adr. name;Street;Line2;Zip;City;Phone;Fax;EMail;Id/Club;Rented;Start fee;Paid;Course no.;Course;km;m;Course controls;Pl;Start punch;Finish punch;Control1;Punch1;Control2;Punch2;Control3;Punch3;Control4;Punch4;Control5;Punch5;Control6;Punch6;Control7;Punch7;Control8;Punch8;Control9;Punch9;Control10;Punch10;(may be more) ...");
    maleString = "M";
    femaleString = "F";
    break;
  case 2: // Svenska
    csv.outputRow("Startnr;Bricka;Databas nr.;Efternamn;Förnamn;År;K;Block;ut;Start;Mål;Tid;Status;Klubb nr.;Namn;Ort;Land;Klass nr.;Kort;Lång;Num1;Num2;Num3;Text1;Text2;Text3;Adr. namn;Gata;Rad 2;Post nr.;Ort;Tel;Fax;E-post;Id/Club;Hyrd;Startavgift;Betalt;Bana nr.;Bana;km;Hm;Bana kontroller;Pl;Startstämpling;Målstämpling;Kontroll1;Stämplar1;Kontroll2;Stämplar2;Kontroll3;Stämplar3;Kontroll4;Stämplar4;Kontroll5;Stämplar5;Kontroll6;Stämplar6;Kontroll7;Stämplar7;Kontroll8;Stämplar8;Kontroll9;Stämplar9;Kontroll10;Stämplar10;(kan fortsätta)..");
    maleString = "M"; 
    femaleString = "K"; 
    break;
  case 3: // Deutsch
    csv.outputRow("Stnr;Chip;Datenbank Id;Nachname;Vorname;Jg;G;Block;AK;Start;Ziel;Zeit;Wertung;Club-Nr.;Abk;Ort;Nat;Katnr;Kurz;Lang;Num1;Num2;Num3;Text1;Text2;Text3;Adr. Name;Straße;Zeile2;PLZ;Ort;Tel;Fax;EMail;Id/Verein;Gemietet;Startgeld;Bezahlt;Bahnnummer;Bahn;km;Hm;Bahn Posten;Pl;Startstempel;Zielstempel;Posten1;Stempel1;Posten2;Stempel2;Posten3;Stempel3;Posten4;Stempel4;Posten5;Stempel5;Posten6;Stempel6;Posten7;Stempel7;Posten8;Stempel8;Posten9;Stempel9;Posten10;Stempel10;(und weitere)...");
    maleString = "M";
    femaleString = "W";
    break;
  case 4: // Dansk
    csv.outputRow("Stnr;Brik;Database ID;Efternavn;Fornavn;År;K;Blok;UFK;Start;Mål;Tid;Status;Klub nr.;Navn;Klub;Land;Klasse nr.;kort;Lang;Num1;Num2;Num3;Text1;Text2;Text3;Adr. navn;Gade;Linie2;Post nr.;Klub;Tlf.;Fax.;Email;Id/klub;Lejet;Startafgift;Betalt;Bane nr.;Bane;km;Hm;Poster på bane;Pl;Start-stempling;Mål-stempling;Post1;Klip1;Post2;Klip2;Post3;Klip3;Post4;Klip4;Post5;Klip5;Post6;Klip6;Post7;Klip7;Post8;Klip8;Post9;Klip9;Post10;Klip10;(måske mere)...");
    maleString = "M";
    femaleString = "K";
    break;
  case 5: // Français
    csv.outputRow("N° dép.;Puce;Ident. base de données;Nom;Prénom;Né;S;Plage;nc;Départ;Arrivée;Temps;Evaluation;N° club;Nom;Ville;Nat;N° cat.;Court;Long;Num1;Num2;Num3;Text1;Text2;Text3;Adr. nom;Rue;Ligne2;Code Post.;Ville;Tél.;Fax;E-mail;Id/Club;Louée;Engagement;Payé;Circuit N°;Circuit;km;m;Postes du circuit;Pl;Poinçon de départ;Arrivée (P);Poste1;Poinçon1;Poste2;Poinçon2;Poste3;Poinçon3;Poste4;Poinçon4;Poste5;Poinçon5;Poste6;Poinçon6;Poste7;Poinçon7;Poste8;Poinçon8;Poste9;Poinçon9;Poste10;Poinçon10;(peut être plus) ...");
    maleString = "H";
    femaleString = "F";
    break;
  case 6: // Russian
    csv.outputRow("Stnr;Chip;Datenbank Id;Nachname;Vorname;Jg;G_Sex;Block;AK_notclass;Start;Ziel;Zeit;Wertung;Club-Nr.;Abk;Ort;Nat;Katnr;Kurz;Lang;Num1;Num2;Num3;Text1;Text2;Text3;Adr. Name;Strasse;Zeile2;PLZ;Ort;Tel;Fax;EMail;Club_TIdNr;Gemietet;Startgeld;Bezahlt;Bahnnummer;Bahn;km_Kilometer;Hm_Climbmeter;Bahn Posten;Pl_Place;Startstempel;Zielstempel;Posten1;Stempel1;Posten2;Stempel2;Posten3;Stempel3;Posten4;Stempel4;Posten5;Stempel5;Posten6;Stempel6;Posten7;Stempel7;Posten8;Stempel8;Posten9;Stempel9;Posten10;Stempel10;(und weitere)...");
    maleString = "M";
    femaleString = "W";
    break;
  default:
    csv.outputRow("Stno;Chip;Database Id;Surname;First name;YB;S;Block;nc;Start;Finish;Time;Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;Num1;Num2;Num3;Text1;Text2;Text3;Adr. name;Street;Line2;Zip;City;Phone;Fax;EMail;Id/Club;Rented;Start fee;Paid;Course no.;Course;km;m;Course controls;Pl;Start punch;Finish punch;Control1;Punch1;Control2;Punch2;Control3;Punch3;Control4;Punch4;Control5;Punch5;Control6;Punch6;Control7;Punch7;Control8;Punch8;Control9;Punch9;Control10;Punch10;(may be more) ...");
    maleString = "M";
    femaleString = "F";
  }

  char bf[256];
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (!classes.empty() && !classes.count(it->getClassId(true)))
      continue;

    vector<string> row;
    row.resize(46);
    oDataInterface di = it->getDI();

    row[OEstno] = conv_is(it->getId());
    row[OEcard] = conv_is(it->getCardNo());
    if (it->getExtIdentifier() != 0)
      row[OEid] = gdibase.recodeToNarrow(it->getExtIdentifierString());
    row[OEsurname] = gdibase.recodeToNarrow(it->getFamilyName());
    row[OEfirstname] = gdibase.recodeToNarrow(it->getGivenName());
    row[OEbirth] = conv_is(di.getInt("BirthYear") % 100);

    // Specialized per language
    PersonSex s = it->getSex();
    switch (s) {
    case sFemale:
      row[OEsex] = femaleString;
      break;
    case sMale:
      row[OEsex] = maleString;
      break;
    case sBoth:
    case sUnknown:
    default:
      row[OEsex] = gdibase.recodeToNarrow(di.getString("Sex"));
      break;
    }

    // nc / Runner shall not / doesn't want to be ranked
    if (it->getStatus() == StatusNotCompetiting)
      row[OEnc] = "X";
    else
      row[OEnc] = "0";

    // Excel format HH:MM:SS
    if (it->getStartTime() > 0)
      row[OEstart] = gdibase.recodeToNarrow(it->getStartTimeS());
    
    // Excel format HH:MM:SS
    if (it->getFinishTime() > 0)
      row[OEfinish] = gdibase.recodeToNarrow(it->getFinishTimeS(false, SubSecond::Auto));
    
    // Excel format HH:MM:SS
    
    if (it->getRunningTime(true) > 0)
      row[OEtime] = gdibase.recodeToNarrow(formatTimeHMS(it->getRunningTime(true)));

    row[OEstatus] = conv_is(ConvertStatusToOE(it->getStatusComputed(true)));
    row[OEclubno] = conv_is(it->getClubId());

    if (it->getClubRef()) {
      row[OEclub] = gdibase.recodeToNarrow(it->getClubRef()->getDI().getString("ShortName"));
      row[OEclubcity] = gdibase.recodeToNarrow(it->getClub());
    }
    row[OEnat] = gdibase.recodeToNarrow(di.getString("Nationality"));
    {
      pClass pc = it->getClassRef(true);
      row[OEclassno] = pc ? gdibase.recodeToNarrow(pc->getExtIdentifierString()) : "0";
    }
    row[OEclassshortname] = gdibase.recodeToNarrow(it->getClass(true));
    row[OEclassname] = gdibase.recodeToNarrow(it->getClass(true));

    row[OErent] = conv_is(di.getInt("CardFee"));
    row[OEfee] = conv_is(di.getInt("Fee"));
    row[OEpaid] = conv_is(di.getInt("Paid"));

    pCourse pc = it->getCourse(true);
    if (pc) {
      row[OEcourseno] = conv_is(pc->getId());
      row[OEcourse] = gdibase.recodeToNarrow(pc->getName());
      if (pc->getLength()>0) {
        sprintf_s(bf, "%d.%d", pc->getLength() / 1000, pc->getLength() % 1000);
        row[OElength] = bf;
      }
      row[OEclimb] = conv_is(pc->getDI().getInt("Climb"));

      row[OEcoursecontrols] = conv_is(pc->nControls);
    }
    row[OEpl] = gdibase.recodeToNarrow(it->getPlaceS());

    if (includeSplits && pc != NULL)
    {
      // Add here split times

      // row[45]: finish time
      row[OEfinishpunch] = row[OEfinish];

      // row[46; 48; 50; ..]: control id
      // row[47; 49; 51; ..]: punch time of control id row[i-1]

      const vector<SplitData> &sp = it->getSplitTimes(true);

      bool hasRogaining = pc->hasRogaining();
      int startIx = pc->useFirstAsStart() ? 1 : 0;
      int endIx = pc->useLastAsFinish() ? pc->nControls - 1 : pc->nControls;

      for (int k = startIx, m = 0; k < endIx; k++, m += 2) {
        if (pc->getControl(k)->isRogaining(hasRogaining))
          continue;
        row.push_back(gdibase.recodeToNarrow(pc->getControl(k)->getIdS()));
        if (unsigned(k) < sp.size() && sp[k].getTime(false) > 0)
          row.push_back(gdibase.recodeToNarrow(formatTimeHMS(sp[k].getTime(false) - it->tStartTime)));
        else
          row.push_back("-----");
      }

      // Extra punches
      vector<pFreePunch> punches;

      oe->getPunchesForRunner(it->getId(), true, punches);
      for (vector<pFreePunch>::iterator punchIt = punches.begin(); punchIt != punches.end(); ++punchIt) {
        pPunch punch = *punchIt;
        if (!punch->isUsed && !(punch->isFinish() && !pc->useLastAsFinish()) && !(punch->isStart() && !pc->useFirstAsStart()) && !punch->isCheck())
        {
          row.push_back(gdibase.recodeToNarrow(punch->getType()));

          int t = punch->getAdjustedTime();
          if (it->tStartTime > 0 && t > 0 && t > it->tStartTime)
            row.push_back(gdibase.recodeToNarrow(formatTimeHMS(t - it->tStartTime)));
          else
            row.push_back("-----");
        }
      }

    }

    csv.outputRow(row);
  }

  csv.closeOutput();

  return true;
}

void oEvent::importXML_EntryData(gdioutput &gdi, const wstring &file, 
                                 bool updateClass, bool removeNonexisting,
                                 const set<int> &filter,
                                 int classIdOffset,
                                 int courseIdOffset,
                                 const pair<string, string> &preferredIdType) {
  vector<pair<int, int>> runnersInTeam;
  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved() && it->tInTeam) {
      runnersInTeam.push_back(make_pair(it->getId(), it->getClassId(false)) );
    }
  }

  xmlparser xml;
  xml.read(file);

  xmlobject xo = xml.getObject("EntryList");
  set<wstring> matchedClasses;

  if (xo) {

    gdi.addString("", 0, "Importerar anmälningar (IOF, xml)");
    gdi.refreshFast();
    int ent = 0, fail = 0, removed = 0;

    if (xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      reader.setIdOffset(classIdOffset, courseIdOffset);
      reader.setPreferredIdType(preferredIdType);
      reader.readEntryList(gdi, xo, removeNonexisting, filter, ent, fail, removed);

      for (auto &c : Clubs) {
        c.updateFromDB();
      }
    }
    else {
      xmlList xl;
      xo.getObjects(xl);
      xmlList::const_iterator it;

      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("ClubEntry")){
          xmlList entries;
          //xmlobject xentry=it->getObject("Entry");
          int ClubId = 0;

          xmlobject club = it->getObject("Club");

          if (club) {
            addXMLClub(club, false);
            ClubId = club.getObjectInt("ClubId");
          }
          else
            ClubId = it->getObjectInt("ClubId");

          it->getObjects("Entry", entries);
          for (size_t k = 0; k<entries.size(); k++) {
            bool team = entries[k] && entries[k].getObject("TeamName");
            if (team) {
              if (addXMLTeamEntry(entries[k], ClubId))
                ent++;
              else
                fail++;
            }
            else {
              if (addXMLEntry(entries[k], ClubId, true))
                ent++;
              else
                fail++;
            }
          }
        }
      }
    }
    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(ent));
    if (fail>0)
      gdi.addString("", 0, "Antal som inte importerades: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();
  }


  xo = xml.getObject("StartList");

  if (xo) {

    gdi.addString("", 0, "Importerar anmälningar (IOF, xml)");
    gdi.refreshFast();

    int ent = 0, fail = 0;
    
    if (xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      reader.setIdOffset(classIdOffset, courseIdOffset);
      reader.readStartList(gdi, xo, ent, fail);
    }
    else {
      xmlList xl;
      xo.getObjects(xl);
      xmlList::const_iterator it;
      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("ClassStart")){
          xmlList entries;
          int clsId = it->getObjectInt("ClassId");

          pClass cls = 0;
          if (clsId == 0) {
            wstring clsName;
            it->getObjectString("ClassShortName", clsName);
            if (!clsName.empty())
              cls = getClassCreate(0, clsName, matchedClasses);
          }
          else {
            cls = getClassCreate(clsId, lang.tl(L"Klass ") + itow(clsId), matchedClasses);
          }
          it->getObjects("PersonStart", entries);
          for (size_t k = 0; k<entries.size(); k++) {
            {
              if (addXMLStart(entries[k], cls))
                ent++;
              else
                fail++;
            }
          }
        }
      }
    }
    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(ent));
    if (fail>0)
      gdi.addString("", 0, "Antal som inte importerades: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();
  }

  xo = xml.getObject("ResultList");

  if (xo) {

    int ent = 0, fail = 0;
    if (xo.getAttrib("iofVersion")) {
      gdi.addString("", 0, "Importerar resultat (IOF, xml)");
      gdi.refreshFast();
      IOF30Interface reader(this, false, false);
      reader.setIdOffset(classIdOffset, courseIdOffset);
      reader.readResultList(gdi, xo, ent, fail);
    }
    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(ent));
    if (fail>0)
      gdi.addString("", 0, "Antal som inte importerades: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();

  }

  xo = xml.getObject("ClassData");

  if (!xo)
    xo = xml.getObject("ClassList");

  if (xo) {
    gdi.addString("", 0, "Importerar klasser (IOF, xml)");
    gdi.refreshFast();
    int imp = 0, fail = 0;

    if (xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      reader.setIdOffset(classIdOffset, courseIdOffset);
      reader.readClassList(gdi, xo, imp, fail);
    }
    else {
      xmlList xl;
      xo.getObjects(xl);

      xmlList::const_iterator it;

      for (it=xl.begin(); it != xl.end(); ++it) {
        if (it->is("Class")) {
          if (addXMLClass(*it))
            imp++;
          else fail++;
        }
      }
    }
    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(imp));
    if (fail>0)
      gdi.addString("", 0, "Antal misslyckade: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();
  }

  xo=xml.getObject("ClubList");

  if (xo) {
    gdi.addString("", 0, "Importerar klubbar (IOF, xml)");
    gdi.refreshFast();
    int imp = 0, fail = 0;

    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Club")){
        if (addXMLClub(*it, false))
          imp++;
        else
          fail++;
      }
    }
    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(imp));
    if (fail>0)
      gdi.addString("", 0, "Antal misslyckade: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();
  }

  xo=xml.getObject("RankList");

  if (xo) {
    gdi.addString("", 0, "Importerar ranking...");
    gdi.refreshFast();
    int imp = 0, fail = 0;

    xmlList xl;
    xo.getObjects(xl);
    
    map<__int64, int> ext2Id;
    for (auto it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;
      __int64 ext = it->getExtIdentifier();
      if (ext != 0)
        ext2Id[ext] = it->getId();
    }
    map<int, pair<int, RankStatus>> rankList;
    for (auto it = xl.begin(); it != xl.end(); ++it) {
      if (it->is("Competitor")) {
        addXMLRank(*it, ext2Id, rankList);
      }
    }
    for (auto it : rankList) {
      if (it.second.second == RankStatus::Ambivalent)
        fail++;
      else {
        pRunner r = getRunner(it.first, 0);
        if (r) {
          r->getDI().setInt("Rank", it.second.first);
          r->synchronize();
          imp++;
        }
        else {
          fail++;
        }
      }
    }
    
    gdi.addString("", 0, "Klart. X värden tilldelade.#" + itos(imp));
    if (fail>0)
      gdi.addString("", 0, "Antal ignorerade: X#" + itos(fail));
    gdi.dropLine();
    gdi.refreshFast();
  }

  xo=xml.getObject("CourseData");

  if (xo) {
    gdi.addString("", 0, "Importerar banor (IOF, xml)");
    gdi.refreshFast();
    int imp = 0, fail = 0;

    if (xo && xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      reader.setIdOffset(classIdOffset, courseIdOffset);
      reader.readCourseData(gdi, xo, updateClass, imp, fail);
    }
    else {
      xmlList xl;
      xo.getObjects(xl);

      xmlList::const_iterator it;

      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("Course")){
          if (addXMLCourse(*it, updateClass, matchedClasses))
            imp++;
          else
            fail++;
        }
        else if (it->is("Control")){
          addXMLControl(*it, 0);
        }
        else if (it->is("StartPoint")){
          addXMLControl(*it, 1);
        }
        else if (it->is("FinishPoint")){
          addXMLControl(*it, 2);
        }
      }
    }

    gdi.addString("", 0, "Klart. Antal importerade: X#" + itos(imp));
    if (fail>0)
      gdi.addString("", 0, "Antal misslyckade: X#" + itos(fail)).setColor(colorRed);
    gdi.dropLine();
    gdi.refreshFast();
  }


  xo=xml.getObject("EventList");

  if (xo) {
    gdi.addString("", 0, "Importerar tävlingsdata (IOF, xml)");
    gdi.refreshFast();

    if (xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      reader.readEventList(gdi, xo);
      gdi.addString("", 0, L"Tävlingens namn: X#" + getName());
      gdi.dropLine();
      gdi.refreshFast();
    }
    else {
      xmlList xl;
      xo.getObjects(xl);

      xmlList::const_iterator it;

      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("Event")){
          addXMLEvent(*it);
          gdi.addString("", 0, L"Tävlingens namn: X#" + getName());
          gdi.dropLine();
          gdi.refreshFast();
          break;
        }
      }
    }
  }


  xo = xml.getObject("ServiceRequestList");

  if (xo) {
    gdi.addString("", 0, "Importerar tävlingsdata (IOF, xml)");
    gdi.refreshFast();

    if (xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);
      int imp = 0, fail = 0;

      reader.readServiceRequestList(gdi, xo, imp, fail);      
      gdi.dropLine();
      gdi.refreshFast();
    }
    
  }

  vector<int> toRemove;
  for (size_t k = 0; k < runnersInTeam.size(); k++) {
    int id = runnersInTeam[k].first;
    int classId = runnersInTeam[k].second;
    pRunner r = getRunner(id, 0);
    if (r && !r->tInTeam && r->getClassId(false) == classId) {
      toRemove.push_back(r->getId());
    }
  }
  removeRunner(toRemove);
}

bool oEvent::addXMLCompetitorDB(const xmlobject &xentry, int clubId)
{
  if (!xentry) return false;

  xmlobject person = xentry.getObject("Person");
  if (!person) return false;

  wstring pids;
  person.getObjectString("PersonId", pids);
  __int64 extId = oBase::converExtIdentifierString(pids);
  int pid = oBase::idFromExtId(extId);

  xmlobject pname = person.getObject("PersonName");
  if (!pname) return false;

  int cardno = 0;
  string tmp;

  xmlList cards;
  xentry.getObjects("CCard", cards);

  for (size_t k = 0; k<cards.size(); k++) {
    xmlobject &card = cards[k];
    if (card) {
      xmlobject psys = card.getObject("PunchingUnitType");
      if (!psys || psys.getObjectString("value", tmp) == "SI") {
        cardno = card.getObjectInt("CCardId");
        break;
      }
    }
  }

  //if (!cardno)
  //  return false;

  wstring given;
  pname.getObjectString("Given", given);
  getFirst(given, 2);
  wstring family;
  pname.getObjectString("Family", family);

  if (given.empty() || family.empty())
    return false;

  wstring name(family + L", " + given);

  char sex[2];
  person.getObjectString("sex", sex, 2);

  xmlobject bd = person.getObject("BirthDate");
  wstring birth;
  if (bd)
    bd.getObjectString("Date", birth);

  xmlobject nat = person.getObject("Nationality");

  char national[4] = { 0,0,0,0 };
  if (nat) {
    xmlobject natId = nat.getObject("CountryId");
    if (natId)
      natId.getObjectString("value", national, 4);
  }

  RunnerWDBEntry *rde = runnerDB->getRunnerById(extId);

  if (!rde) {
    rde = runnerDB->getRunnerByCard(cardno);

    if (rde && rde->getExtId() != 0)
      rde = 0; //Other runner, same card

    if (!rde)
      rde = runnerDB->addRunner(name.c_str(), pid, clubId, cardno);
  }

  if (rde) {
    rde->setExtId(extId);
    rde->setName(name.c_str());
    rde->dbe().clubNo = clubId;
    rde->dbe().setBirthDate(birth);
    rde->dbe().sex = sex[0];
    memcpy(rde->dbe().national, national, 3);
  }
  return true;
}

bool oEvent::addOECSVCompetitorDB(const vector<wstring> &row)
{
  // Ident. base de données;Puce;Nom;Prénom;Né;S;N° club;Nom;Ville;Nat;N° cat.;Court;Long;Num1;Num2;Num3;E_Mail;Texte1;Texte2;Texte3;Adr. nom;Rue;Ligne2;Code Post.;Ville;Tél.;Fax;E-mail;Id/Club;Louée
  enum { OEid = 0, OEcard = 1, OEsurname = 2, OEfirstname = 3, OEbirth = 4, OEsex = 5,
    OEclubno = 6, OEclub = 7, OEclubcity = 8, OEnat = 9, OEclassno = 10, OEclassshort = 11, OEclasslong = 12
  };

  int pid = _wtoi(row[OEid].c_str());

  wstring given = row[OEfirstname];
  wstring family = row[OEsurname];

  if (given.empty() && family.empty())
    return false;

  wstring name = family + L", " + given;
  
  // Depending on the OE language, man = "H" (French) or "M" (English, Svenska, Dansk, Russian, Deutsch)
  // woman = "F" (English, French) or "W" (Deutsch, Russian) or "K" (Svenska, Dansk)
  char sex[2];
  if (row[OEsex] == L"H" || row[OEsex] == L"M")
    strcpy_s(sex, "M");
  else if (row[OEsex] == L"F" || row[OEsex] == L"K" || row[OEsex] == L"W")
    strcpy_s(sex, "W");
  else
    strcpy_s(sex, "");

  const wstring &birth = row[OEbirth];

  // Hack to take care of inconsistency between FFCO licensees archive (France) and event registrations from FFCO (FR)
  char national[4] = { 0,0,0,0 };
  if (row[OEnat] == L"France") {
    strcpy_s(national, "FRA");
  }

  // Extract club data

  int clubId = _wtoi(row[OEclubno].c_str());
  wstring clubName;
  wstring shortClubName;

  clubName = row[OEclubcity];
  shortClubName = row[OEclub];

  if (clubName.length() > 0 && IsCharAlphaNumeric(clubName[0])) {

    pClub pc = new oClub(this);
    pc->Id = clubId;

    pc->setName(clubName);
    pc->setExtIdentifier(clubId);

    oDataInterface DI = pc->getDI();
    DI.setString("ShortName", shortClubName.substr(0, 8));
    // Nationality?

    runnerDB->importClub(*pc, false);
    delete pc;
  }

  RunnerWDBEntry *rde = runnerDB->getRunnerById(pid);

  int cardno = _wtoi(row[OEcard].c_str());
  if (!rde) {
    rde = runnerDB->getRunnerByCard(cardno);

    if (rde && rde->getExtId() != 0)
      rde = NULL; //Other runner, same card

    if (!rde)
      rde = runnerDB->addRunner(name.c_str(), pid, clubId, cardno);
  }

  if (rde) {
    rde->setExtId(pid);
    rde->setName(name.c_str());
    rde->dbe().clubNo = clubId;
    rde->dbe().setBirthDate(birth);
    rde->dbe().sex = sex[0];
    memcpy(rde->dbe().national, national, 3);
  }
  return true;
}

bool oEvent::addXMLTeamEntry(const xmlobject &xentry, int clubId)
{
  if (!xentry) return false;

  wstring name;
  xentry.getObjectString("TeamName", name);
  int id = xentry.getObjectInt("EntryId");

  if (name.empty())
    name = lang.tl("Lag X#" + itos(id));

  xmlList teamCmp;
  xentry.getObjects("TeamCompetitor", teamCmp);

  xmlobject cls = xentry.getObject("EntryClass");
  xmlobject edate = xentry.getObject("EntryDate");

  pClass pc = getXMLClass(xentry);

  if (!pc)
    return false;

  pClub club = getClubCreate(clubId);

  pTeam t = getTeam(id);

  if (t == 0) {
    if ( id > 0) {
      oTeam oR(this, id);
      t = addTeam(oR, true);
    }
    else {
      oTeam oR(this);
      t = addTeam(oR, true);
    }
    t->setStartNo(Teams.size(), oBase::ChangeType::Update);
  }

  if (!t->hasFlag(oAbstractRunner::FlagUpdateName))
    t->setName(name, false);
  if (!t->Class || !t->hasFlag(oAbstractRunner::FlagUpdateClass))
    t->setClassId(pc->getId(), false);

  t->Club = club;
  oDataInterface DI = t->getDI();

  wstring date;
  if (edate) DI.setDate("EntryDate", edate.getObjectString("Date", date));

  int maxleg = teamCmp.size();
  for (size_t k = 0; k < teamCmp.size(); k++) {
    maxleg = max(maxleg, teamCmp[k].getObjectInt("TeamSequence"));
  }

  if (pc->getNumStages() < unsigned(maxleg)) {
    setupRelay(*pc, PRelay, maxleg, getAbsTime(timeConstHour));
  }

  for (size_t k = 0; k < teamCmp.size(); k++) {
    int leg = teamCmp[k].getObjectInt("TeamSequence") - 1;
    if (leg>=0) {
      pRunner r = addXMLEntry(teamCmp[k], clubId, false);
      t->setRunner(leg, r, true);
    }
  }

  return true;
}

pClass oEvent::getXMLClass(const xmlobject &xentry) {

  xmlobject eclass = xentry.getObject("EntryClass");
  if (eclass) {
    int cid = eclass.getObjectInt("ClassId");
    pClass pc = getClass(cid);
    if ( pc == 0 && cid>0) {
      oClass cls(this, cid);
      cls.Name = lang.tl(L"Klass X#" + itow(cid));
      pc = addClass(cls);//Create class if not found
    }
    return pc;
  }
  return 0;
}

pClub oEvent::getClubCreate(int clubId) {
  if (clubId) {
    pClub club = getClub(clubId);
    if (!club) {
      pClub dbClub = runnerDB->getClub(clubId);
      if (dbClub) {
        club = addClub(*dbClub);
      }
      if (!club) {
        club = addClub(lang.tl("Klubb X#" + itos(clubId)));
      }
    }
    return club;
  }
  return 0;
}

pRunner oEvent::addXMLPerson(const xmlobject &person) {
  xmlobject pname = person.getObject("PersonName");
  if (!pname) return 0;

  wstring pids;
  person.getObjectString("PersonId", pids);
  __int64 extId = oBase::converExtIdentifierString(pids);
  int pid = oBase::idFromExtId(extId);
  pRunner r = 0;

  if (pid)
    r = getRunner(pid, 0);

  if (!r) {
    if ( pid > 0) {
      oRunner oR(this, pid);
      r = addRunner(oR, true);
    }
    else {
      oRunner oR(this);
      r = addRunner(oR, true);
    }
  }

  wstring given, family;
  pname.getObjectString("Given", given);
  pname.getObjectString("Family", family);

  r->setName(family + L", " + getFirst(given, 2), false);
  r->setExtIdentifier(extId);

  oDataInterface DI=r->getDI();
  wstring tmp;

  r->setSex(interpretSex(person.getObjectString("sex", tmp)));
  xmlobject bd=person.getObject("BirthDate");

  if (bd) {
    bd.getObjectString("Date", tmp);
    r->setBirthDate(tmp);
  }
  xmlobject nat=person.getObject("Nationality");

  if (nat) {
    wchar_t national[4];
    xmlobject natId = nat.getObject("CountryId");
    if (natId)
      r->setNationality(natId.getObjectString("value", national, 4));
  }

  return r;
}

pRunner oEvent::addXMLEntry(const xmlobject &xentry, int clubId, bool setClass) {
  if (!xentry) return 0;

  xmlobject person = xentry.getObject("Person");
  if (!person) return 0;

  pRunner r = addXMLPerson(person);

  int cmpClubId = xentry.getObjectInt("ClubId");
  if (cmpClubId != 0)
    clubId = cmpClubId;

  oDataInterface DI=r->getDI();
  wstring given = r->getGivenName();
  xmlList cards;
  xentry.getObjects("CCard", cards);
  wstring tmp;
  for (size_t k= 0; k<cards.size(); k++) {
    xmlobject &card = cards[k];
    if (card) {
      xmlobject psys = card.getObject("PunchingUnitType");
      if (!psys || psys.getObjectString("value", tmp) == L"SI") {
        int cardno = card.getObjectInt("CCardId");
        r->setCardNo(cardno, false);
        break;
      }
    }
  }

  pClass oldClass = r->Class;
  pClub oldClub = r->Club;

  if (setClass && !r->hasFlag(oAbstractRunner::FlagUpdateClass) )
    r->Class = getXMLClass(xentry);
  classIdToRunnerHash.reset();

  r->Club = getClubCreate(clubId);

  if (oldClass != r->Class || oldClub != r->Club)
    r->updateChanged();

  xmlobject edate=xentry.getObject("EntryDate");
  if (edate) DI.setDate("EntryDate", edate.getObjectString("Date", tmp));

  r->addClassDefaultFee(false);
  r->synchronize();

  xmlobject adjRunner = xentry.getObject("AllocationControl");

  if (adjRunner) {
    xmlobject person2 = adjRunner.getObject("Person");
    if (person2) {
      xmlobject pname2 = person2.getObject("PersonName");

      wstring pids2;
      person.getObjectString("PersonId", pids2);
      const __int64 extId2 = oBase::converExtIdentifierString(pids2);
      int pid2 = oBase::idFromExtId(extId2);
      pRunner r2 = getRunner(pid2, 0);

      if (!r2) {
        if ( pid2 > 0) {
          oRunner oR(this, pid2);
          r2 = addRunner(oR, true);
        }
        else {
          oRunner oR(this);
          r2 = addRunner(oR, true);
        }
      }

      wstring given2, family2;
      if (pname2) {
        pname2.getObjectString("Given", given2);
        pname2.getObjectString("Family", family2);
        r2->setName(family2 + L", " + getFirst(given2, 2), false);
      }

      r2->setExtIdentifier(pid2);
      // Create patrol

      if (r->Class) {

        bool createTeam = false;
        pClass pc = r->Class;
        if (pc->getNumStages() <= 1) {
          setupRelay(*pc, PPatrolOptional, 2, getAbsTime(timeConstHour));
          createTeam = true;
        }

        pTeam t = r->tInTeam;
        if (t == 0)
          t = r2->tInTeam;

        if (t == 0) {
          autoAddTeam(r);
          t = r2->tInTeam;
          createTeam = true;
        }

        if (t != 0) {
          if (t->getRunner(0) == r2) {
            t->setRunner(0, r2, true);
            t->setRunner(1, r, true);
          }
          else {
            t->setRunner(0, r, true);
            t->setRunner(1, r2, true);
          }
        }

        if (createTeam && t && !t->hasFlag(oAbstractRunner::FlagUpdateName)) {
          t->setName(given + L" / " + given2, false);
        }
      }
    }
  }
  else {
    if (r->Class && r->Class->getNumStages()>=2) {
      autoAddTeam(r);
    }
  }
  return r;
}


pRunner oEvent::addXMLStart(const xmlobject &xstart, pClass cls) {

  if (!xstart) return 0;

  xmlobject person_ = xstart.getObject("Person");
  if (!person_) return 0;

  pRunner r = addXMLPerson(person_);
  pClass oldClass = r->Class;
  pClub oldClub = r->Club;
  r->Class = cls;
  classIdToRunnerHash.reset();

  xmlobject xclub = xstart.getObject("Club");
  int clubId = xstart.getObjectInt("ClubId");
  wstring cname;
  if (xclub) {
    clubId = xclub.getObjectInt("ClubId");
    xclub.getObjectString("ShortName", cname);
  }

  if (clubId > 0) {
    r->Club = getClubCreate(clubId, cname);
  }

  xmlobject xstrt = xstart.getObject("Start");

  if (!xstrt)
    return r;

  oDataInterface DI=r->getDI();

  int cardno = xstrt.getObjectInt("CCardId");

  if (cardno == 0) {
    xmlList cards;
    xstrt.getObjects("CCard", cards);
    string tmp;
    for (size_t k= 0; k<cards.size(); k++) {
      xmlobject &card = cards[k];
      if (card) {
        xmlobject psys = card.getObject("PunchingUnitType");
        if (!psys || psys.getObjectString("value", tmp) == "SI") {
          int cardno = card.getObjectInt("CCardId");
          r->setCardNo(cardno, false);
          break;
        }
      }
    }
  }
  else
    r->setCardNo(cardno, false);

  wstring tmp;
  xmlobject xstarttime = xstrt.getObject("StartTime");
  if (xstarttime)
    r->setStartTimeS(xstarttime.getObjectString("Clock", tmp));

  if (oldClass != r->Class || oldClub != r->Club)
    r->updateChanged();

  r->addClassDefaultFee(false);
  r->synchronize();

  if (r->Class && r->Class->getNumStages()>=2) {
    autoAddTeam(r);
  }
  return r;
}

void oEvent::importOECSV_Data(const wstring &oecsvfile, bool clear) {
  // Clear DB if needed
  if (clear) {
    runnerDB->clearClubs();
    runnerDB->clearRunners();
  }

  csvparser cp;
  list< vector<wstring> > data;

  gdibase.addString("",0,"Läser löpare...");
  gdibase.refresh();

  cp.parse(oecsvfile, data);

  gdibase.addString("", 0, "Behandlar löpardatabasen").setColor(colorGreen);
  
  gdibase.refresh();

  list<vector<wstring>>::iterator it;

  for (it = ++(data.begin()); it != data.end(); ++it) {
    addOECSVCompetitorDB(*it);
  }
    
  gdibase.addString("", 0, "Klart. Antal importerade: X#" + itos(data.size()-1));
  gdibase.refresh();

  setProperty("DatabaseUpdate", getRelativeDay());

  // Save DB
  saveRunnerDatabase(L"database", true);

  if (hasDBConnection()) {
    gdibase.addString("", 0, "Uppdaterar serverns databas...");
    gdibase.refresh();

    OpFailStatus stat = sqlConnection->uploadRunnerDB(this);

    if (stat == opStatusFail) {
      string err;
      sqlConnection->getErrorMessage(err);
      string error = string("Kunde inte ladda upp löpardatabasen (X).#") + err;
      throw meosException(error);
    }
    else if (stat == opStatusWarning) {
      string err;
      sqlConnection->getErrorMessage(err);
      gdibase.addInfoBox("", wstring(L"Kunde inte ladda upp löpardatabasen (X).#") + lang.tl(err), 5000);
    }

    gdibase.addString("", 0, "Klart");
    gdibase.refresh();
  }
}

void oEvent::importXML_IOF_Data(const wstring &clubfile,
                                const wstring &competitorfile, 
                                bool onlyWithClub, bool clear)
{
  if (!clubfile.empty()) {
    xmlparser xml_club;
    xml_club.setProgress(gdibase.getHWNDTarget());

    if (clear && !competitorfile.empty())
      runnerDB->clearClubs();

    gdibase.addString("",0,"Läser klubbar...");
    gdibase.refresh();

    xml_club.read(clubfile);

    gdibase.addString("",0,"Lägger till klubbar...");
    gdibase.refresh();

    xmlobject xo = xml_club.getObject("ClubList");
    int clubCount = 0;

    if (!xo) {
      xo = xml_club.getObject("OrganisationList");
      if (xo) {
        IOF30Interface reader(this, false, false);
        reader.readClubList(gdibase, xo, clubCount);
      }
    }
    else {
      xmlList xl;
      if (xo)
        xo.getObjects(xl);

      xmlList::const_iterator it;
      for (it=xl.begin(); it != xl.end(); ++it)
        if (it->is("Club")) {
          if (addXMLClub(*it, true))
            clubCount++;
        }
    }
    gdibase.addStringUT(0, lang.tl("Antal importerade: ") + itow(clubCount));
  }

  if (!competitorfile.empty()) {
    xmlparser xml_cmp;
    xml_cmp.setProgress(gdibase.getHWNDTarget());
    gdibase.dropLine();
    gdibase.addString("",0,"Läser löpare...");
    gdibase.refresh();

    xml_cmp.read(competitorfile);

    if (clear) {
      runnerDB->clearRunners();
    }

    gdibase.addString("",0,"Lägger till löpare...");
    gdibase.refresh();

    int personCount = 0;
    int duplicateCount = 0;
    xmlobject xo=xml_cmp.getObject("CompetitorList");

    if (xo && xo.getAttrib("iofVersion")) {
      IOF30Interface reader(this, false, false);

      vector<string> idProviders;
      reader.prescanCompetitorList(xo);
      reader.getIdTypes(idProviders);

      if (idProviders.size() > 1) {
        pair<string, string> preferredIdProvider;
        set<int> dmy;
        FlowOperation op = importFilterGUI(oe, gdibase, 
                                           {}, idProviders,
                                           dmy, preferredIdProvider);
        if (op != FlowContinue)
          return;

        reader.setPreferredIdType(preferredIdProvider);
      }
      reader.readCompetitorList(gdibase, xo, onlyWithClub, personCount, duplicateCount);
    }
    else {
      xmlList xl;
      if (xo)
        xo.getObjects(xl);

      xmlList::const_iterator it;

      for (it=xl.begin(); it != xl.end(); ++it) {
        if (it->is("Competitor")){
          int ClubId=it->getObjectInt("ClubId");
          if (addXMLCompetitorDB(*it, ClubId))
            personCount++;
        }
      }
    }

    gdibase.addStringUT(0, lang.tl("Antal importerade: ") + itow(personCount));

    if (duplicateCount > 0)
      gdibase.addString("", 0, "Ignorerade X duplikat.#" + itos(duplicateCount));

    gdibase.refresh();

    setProperty("DatabaseUpdate", getRelativeDay());
  }

  saveRunnerDatabase(L"database", true);

  if (hasDBConnection()) {
    gdibase.addString("", 0, "Uppdaterar serverns databas...");
    gdibase.refresh();

    OpFailStatus stat = (OpFailStatus)sqlConnection->uploadRunnerDB(this);

    if (stat == opStatusFail) {
      string err;
      sqlConnection->getErrorMessage(err);
      string error = string("Kunde inte ladda upp löpardatabasen (X).#") + err;
      throw meosException(error);
    }
    else if (stat == opStatusWarning) {
      string err;
      sqlConnection->getErrorMessage(err);
      gdibase.addInfoBox("", wstring(L"Kunde inte ladda upp löpardatabasen (X).#") + lang.tl(err), 5000);
    }

    gdibase.addString("", 0, "Klart");
    gdibase.refresh();
  }
}


/*
 <Class lowAge="1" highAge="18" sex="K" actualForRanking="Y">
    <ClassId type="nat" idManager="SWE">6</ClassId>
    <Name>D18 Elit</Name>
    <ClassShortName>D18 Elit</ClassShortName>
    <ClassTypeId>E</ClassTypeId>
    <SubstituteClass>
      <ClassId> 600 </ClassId>
    </SubstituteClass>
    <NotQualifiedSubstituteClass>
      <ClassId> 600 </ClassId>
    </NotQualifiedSubstituteClass>
    <EntryFee>
      <EntryFeeId> A </EntryFeeId>
      <Name> Adult </Name>
      <Amount currency="SEK"> 65 </Amount>
    </EntryFee>
    <ModifyDate>
      <Date> 2001-10-18 </Date>
    </ModifyDate>
  </Class>
*/

bool oEvent::addXMLEvent(const xmlobject &xevent)
{
  if (!xevent)
    return false;

  int id = xevent.getObjectInt("EventId");
  wstring name;
  xevent.getObjectString("Name", name);

  xmlobject date = xevent.getObject("StartDate");

  if (id>0)
    setExtIdentifier(id);

  if (!hasFlag(TransferFlags::FlagManualName))
    setName(name, false);

  if (date) {
    wstring dateStr;
    date.getObjectString("Date", dateStr);
    if (!hasFlag(TransferFlags::FlagManualDateTime))
      setDate(dateStr, false);
  }

  synchronize();
  return true;
}

/*
<Course>
  <CourseName> Bana 02 </CourseName>
  <CourseId> 1 </CourseId>
  <ClassShortName> D21E </ClassShortName>
  <CourseVariation>
    <CourseVariationId> 0 </CourseVariationId>
    <CourseLength> 8500 </CourseLength>
    <CourseClimb> 0 </CourseClimb>
    <StartPointCode> S1 </StartPointCode>

    <CourseControl>
    <Sequence> 1 </Sequence>
    <ControlCode> 104 </ControlCode>
    <LegLength> 310 </LegLength>
    </CourseControl>

    <CourseControl>
    <Sequence> 2 </Sequence>
    <ControlCode> 102 </ControlCode>
    <LegLength> 360 </LegLength>
    </CourseControl>

    <FinishPointCode> M1 </FinishPointCode>
    <DistanceToFinish> 157 </DistanceToFinish>
  </CourseVariation>
</Course>

*/
int getLength(const xmlobject &xlen) {
  if (xlen.isnull())
    return 0;
  return xlen.getInt();
}

int getStartIndex(int sn) {
  return sn + 211100;
}

int getFinishIndex(int sn) {
  return sn + 311100;
}

wstring getStartName(const wstring &start) {
  int num = getNumberSuffix(start);
  if (num == 0 && start.length()>0)
    num = int(start[start.length()-1])-'0';
  if (num > 0 && num < 10)
    return lang.tl(L"Start ") + itow(num);
  else if (start.length() == 1)
    return lang.tl(L"Start");
  else
    return start;
}
/*
<Control>
 <ControlCode> 120 </ControlCode>
 <MapPosition x="-1349.6" y="862.7"/>
</Control>
*/
bool oEvent::addXMLControl(const xmlobject &xcontrol, int type)
{
  // type: 0 control, 1 start, 2 finish
  if (!xcontrol)
    return false;

  xmlobject pos = xcontrol.getObject("MapPosition");

  int xp = 0, yp = 0;
  if (pos) {
    string x,y;
    pos.getObjectString("x", x);
    pos.getObjectString("y", y);
    xp = int(10.0 * atof(x.c_str()));
    yp = int(10.0 * atof(y.c_str()));
  }

  if (type == 0) {
    int code = xcontrol.getObjectInt("ControlCode");
    if (code>=30 && code<1024) {
      pControl pc = getControl(code, true, false);
      pc->getDI().setInt("xpos", xp);
      pc->getDI().setInt("ypos", yp);
      pc->synchronize();
    }
  }
  else if (type == 1) {
    wstring start;
    xcontrol.getObjectString("StartPointCode", start);
    start = getStartName(trim(start));
    int num = getNumberSuffix(start);
    if (num == 0 && start.length()>0)
      num = int(start[start.length()-1])-'0';
    pControl pc = getControl(getStartIndex(num), true, false);
    pc->setNumbers(L"");
    pc->setName(start);
    pc->setStatus(oControl::ControlStatus::StatusStart);
    pc->getDI().setInt("xpos", xp);
    pc->getDI().setInt("ypos", yp);
  }
  else if (type == 2) {
    wstring finish;
    xcontrol.getObjectString("FinishPointCode", finish);
    finish = trim(finish);
    int num = getNumberSuffix(finish);
    if (num == 0 && finish.length()>0)
      num = int(finish[finish.length()-1])-'0';
    if (num > 0)
      finish = lang.tl("Mål ") + itow(num);
    pControl pc = getControl(getFinishIndex(num), true, false);
    pc->setNumbers(L"");
    pc->setName(finish);
    pc->setStatus(oControl::ControlStatus::StatusFinish);
    pc->getDI().setInt("xpos", xp);
    pc->getDI().setInt("ypos", yp);
  }

  return true;
}

bool oEvent::addXMLCourse(const xmlobject &xcrs, bool addClasses, set<wstring> &matchedClasses) {
  if (!xcrs)
    return false;

  int cid = xcrs.getObjectInt("CourseId");

  wstring name;
  xcrs.getObjectString("CourseName", name);
  name = trim(name);
  vector<wstring> cls;
  xmlList obj;
  xcrs.getObjects("ClassShortName", obj);
  for (size_t k = 0; k<obj.size(); k++) {
    wstring c;
    obj[k].getObjectString(0, c);
    cls.push_back(trim(c));
  }

  //int clsId = xevent.getObjectInt("ClassId");
  vector<pCourse> courses;
  xcrs.getObjects("CourseVariation", obj);
  for (size_t k = 0; k<obj.size(); k++) {
    int variationId = obj[k].getObjectInt("CourseVariationId");
    wstring varName;
    obj[k].getObjectString("Name", varName);
    //int len = obj[k].getObjectInt("CourseLength");
    xmlobject lenObj = obj[k].getObject("CourseLength");
    int len = getLength(lenObj);
    int climb = getLength(obj[k].getObject("CourseClimb"));
    wstring start;
    obj[k].getObjectString("StartPointCode", start);
    start = getStartName(trim(start));

    xmlList ctrl;
    obj[k].getObjects("CourseControl", ctrl);
    vector<int> ctrlCode(ctrl.size());
    vector<int> legLen(ctrl.size());
    for (size_t i = 0; i<ctrl.size(); i++) {
      unsigned seq = ctrl[i].getObjectInt("Sequence");
      int index = i;
      if (seq>0 && seq<=ctrl.size())
        index = seq-1;

      ctrlCode[index] = ctrl[i].getObjectInt("ControlCode");
      legLen[index] = getLength(ctrl[i].getObject("LegLength"));
    }

    legLen.push_back(getLength(obj[k].getObject("DistanceToFinish")));
    int actualId = ((cid+1)*100) + variationId;
    wstring cname=name;
    if (!varName.empty())
      cname += L" " + varName;
    else if (obj.size() > 1)
      cname += L" " + itow(k+1);

    pCourse pc = 0;
    if (cid > 0)
      pc = getCourseCreate(actualId);
    else {
      pc = getCourse(cname);
      if (pc == 0)
        pc = addCourse(cname);
    }

    pc->setName(cname);
    pc->setLength(len);
    pc->importControls("", true, false);
    for (size_t i = 0; i<ctrlCode.size(); i++) {
      if (ctrlCode[i]>=30 && ctrlCode[i]<1024)
        pc->addControl(ctrlCode[i]);
    }
    if (pc->getNumControls() + 1 == legLen.size())
      pc->setLegLengths(legLen);
    pc->getDI().setInt("Climb", climb);
    pc->setStart(start, true);
    pc->synchronize();

    string finish;
    obj[k].getObjectString("FinishPointCode", finish);
    finish = trim(finish);

    pc->synchronize();

    courses.push_back(pc);
  }
  if (addClasses) {
    for (size_t k = 0; k<cls.size(); k++) {
      pClass pCls = getClassCreate(0, cls[k], matchedClasses);
      if (pCls) {
        if (courses.size()==1) {
          if (pCls->getNumStages()==0) {
            pCls->setCourse(courses[0]);
          }
          else {
            for (size_t i = 0; i<pCls->getNumStages(); i++)
              pCls->addStageCourse(i, courses[0]->getId(), -1);
          }
        }
        else {
          if (courses.size() == pCls->getNumStages()) {
            for (size_t i = 0; i<courses.size(); i++)
              pCls->addStageCourse(i, courses[i]->getId(), -1);
          }
          else {
            for (size_t i = 0; i<courses.size(); i++)
              pCls->addStageCourse(0, courses[i]->getId(), -1);
          }
        }
      }
    }
  }

  return true;
}


bool oEvent::addXMLClass(const xmlobject &xclass)
{
  if (!xclass)
    return false;

  int classid=xclass.getObjectInt("ClassId");
  wstring name, shortName;
  xclass.getObjectString("Name", name);
  xclass.getObjectString("ClassShortName", shortName);

  if (!shortName.empty())
    name = shortName;

  pClass pc=0;

  if (classid) {
    pc = getClass(classid);

    if (!pc) {
      oClass c(this, classid);
      pc = addClass(c);
    }
  }
  else
    pc = addClass(name);

  if (!pc->hasFlag(oClass::TransferFlags::FlagManualName))
    pc->setName(name, false);
  oDataInterface DI=pc->getDI();

  wstring tmp;
  DI.setInt("LowAge", xclass.getObjectInt("lowAge"));
  DI.setInt("HighAge", xclass.getObjectInt("highAge"));
  DI.setString("Sex", xclass.getObjectString("sex", tmp));
  DI.setString("ClassType", xclass.getObjectString("ClassTypeId", tmp));

  xmlList xFee;
  xclass.getObjects("EntryFee", xFee);
  vector<int> fee;
  for (size_t k = 0; k<xFee.size(); k++) {
    xmlobject xamount = xFee[k].getObject("Amount");
    if (xamount) {
      const char* ptr = xamount.getRawPtr();    
      double f = ptr ? atof(ptr) : 0.0;
      wstring cur = xamount.getAttrib("currency").getWStr();
      if (f>0) {
        fee.push_back(oe->interpretCurrency(f, cur));
      }
    }
  }

  // XXX Eventor studpid hack
  if (fee.size() == 2 && fee[1]<fee[0] && fee[1]==50)
    fee[1] = ( 3 * fee[0] ) / 2;

  if (!fee.empty()) {
    sort(fee.begin(), fee.end());
    if (fee.size() == 1) {
      DI.setInt("ClassFee", fee[0]);
      DI.setInt("HighClassFee", fee[0]);
    }
    else {
      DI.setInt("ClassFee", fee[0]);
      DI.setInt("HighClassFee", fee[1]);
    }
  }

  pc->synchronize();
  return true;
}


bool oEvent::addXMLClub(const xmlobject &xclub, bool savetoDB)
{
  if (!xclub)
    return false;

  int clubid=xclub.getObjectInt("ClubId");
  wstring Name, shortName;
  xclub.getObjectString("Name", Name);
  xclub.getObjectString("ShortName", shortName);

  if (!shortName.empty() && shortName.length() < Name.length())
    swap(Name, shortName);

  int district = xclub.getObjectInt("OrganisationId");

  if (Name.length()==0 || !IsCharAlphaNumeric(Name[0]))
    return false;

  xmlobject address=xclub.getObject("Address");

  wstring str;
  wstring co;

  if (address) {
    address.getObjectString("street", str);
    address.getObjectString("careOf", co);
  }

  pClub pc=0;

  if ( !savetoDB ) {
    if (clubid)
      pc = getClubCreate(clubid, Name);

    if (!pc) return false;
  }
  else {
    pc = new oClub(this);
    pc->Id = clubid;
  }

  pc->setName(Name);

  pc->setExtIdentifier(clubid);

  oDataInterface DI=pc->getDI();

  wstring tmp;
  DI.setString("CareOf", co);
  DI.setString("Street", str);
  if (address) {
    DI.setString("City", address.getObjectString("city", tmp));
    DI.setString("ZIP", address.getObjectString("zipCode", tmp));
  }
  DI.setInt("District", district);

  xmlobject tele=xclub.getObject("Tele");

  if (tele){
    DI.setString("EMail", tele.getObjectString("mailAddress", tmp));
    DI.setString("Phone", tele.getObjectString("phoneNumber", tmp));
  }

  xmlobject country=xclub.getObject("Country");

  if (country) {
    xmlobject natId = country.getObject("CountryId");
    wchar_t national[4];
    if (natId)
      DI.setString("Nationality", natId.getObjectString("value", national, 4));
  }

  if (savetoDB) {
    runnerDB->importClub(*pc, false);
    delete pc;
  }
  else {
    pc->synchronize();
  }

  return true;
}


bool oEvent::addXMLRank(const xmlobject &xrank, const map<__int64, int> &externIdToRunnerId,
                        map<int, pair<int, oEvent::RankStatus>> &output)
{
  if (!xrank)
    return false;

  xmlobject person;//xrank->getObject("Person");
  xmlobject club;//xrank->getObject("Club");
  xmlobject rank;
  xmlobject vrank;

  xmlList x;
  xrank.getObjects(x);

  xmlList::const_iterator cit=x.begin();

  string tmp;
  while(cit!=x.end()){

    if (cit->is("Person"))
      person=*cit;
    else if (cit->is("Club"))
      club=*cit;
    else if (cit->is("Rank")){
      if (cit->getObjectString("Name", tmp).find("Vacancy") != string::npos)
        vrank=*cit;
      else
        rank = *cit;
    }
    ++cit;
  }

  if (!person) return false;

  wstring pid;
  person.getObjectString("PersonId", pid);
  const __int64 extId = oBase::converExtIdentifierString(pid);
  int id = oBase::idFromExtId(extId);
  auto res = externIdToRunnerId.find(extId);
  if (res != externIdToRunnerId.end())
    id = res->second;

  pRunner r = getRunner(id, 0);
  bool idMatch = r != nullptr;

  if (r == nullptr){
    xmlobject pname = person.getObject("PersonName");

    if (!pname) return false;

    wstring given, family;
    wstring name=pname.getObjectString("Family", family) + L", "  + getFirst(pname.getObjectString("Given", given), 2);

    if (!club)
      r=getRunnerByName(name);
    else {
      wstring cn, cns;
      club.getObjectString("ShortName", cns);
      club.getObjectString("Name", cn);

      if (cns.empty())
        cns = cn;

      if (!cn.empty() && cn.length()<cns.length())
        swap(cn, cns);

      r=getRunnerByName(name, cns);

      if (r == nullptr)
        r = getRunnerByName(name);
    }
  }

  if (!r) return false; //No runner here!

  if (rank) {
    int pos =  rank.getObjectInt("RankPosition");
    if (idMatch)
      output[r->getId()] = make_pair(pos, RankStatus::IdMatch);
    else {
      auto ores = output.find(r->getId());
      if (ores == output.end())
        output[r->getId()] = make_pair(pos, RankStatus::NameMatch);
      else if (ores->second.second == RankStatus::NameMatch)
        output[r->getId()] = make_pair(pos, RankStatus::Ambivalent); // Not clear. Do not match.
    }
  }

  
 /* oDataInterface DI=r->getDI();

  if (rank)
    DI.setInt("Rank", rank.getObjectInt("RankPosition"));

  r->synchronize();*/

  return true;
}

void oEvent::exportIOFEventList(xmlparser &xml)
{
  xml.startTag("EventList");
  xml.write("IOFVersion", "version", L"2.0.3");

  exportIOFEvent(xml);

  xml.endTag(); //EventList
}

void oEvent::exportIOFEvent(xmlparser &xml)
{
  // (IndSingleDay|IndMultiDay|teamSingleDay|teamMultiDay|relay)
  xml.startTag("Event", "eventForm", "IndSingleDay");

  xml.write("EventId", getExtIdentifierString());
  xml.write("Name", getName());

  xml.write("EventClassificationId", "type", L"other", L"MeOS");

  {
    xml.startTag("StartDate");
    xml.write("Date", "dateFormat", L"YYYY-MM-DD", getDate());
    xml.write("Clock", "clockFormat", L"HH:MM:SS", getZeroTime());
    xml.endTag(); // StartDate
  }

  wstring url = getDCI().getString("Homepage");
  if (!url.empty())
    xml.write("WebURL", url);

  wstring account = getDCI().getString("Account");
  if (!account.empty())
    xml.write("Account", "type", L"other", account);

  xml.endTag(); //Event
}

void oEvent::exportIOFClass(xmlparser &xml)
{
  xml.startTag("ClassData");

  set<wstring> cls;
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (!it->getType().empty())
      cls.insert(it->getType());
  }

  int id = 1;
  map<wstring, int> idMap;
  for (set<wstring>::iterator it = cls.begin(); it != cls.end(); ++it) {
    xml.startTag("ClassType");
    idMap[*it] = id;
    xml.write("ClassTypeId", id++);
    xml.write("Name", *it);
    xml.endTag();
  }

  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    vector<wstring> pv;

    pClass pc = &*it;
    int low = 0;
    int high = 0;
    pc->getAgeLimit(low, high);

    if (low>0) {
      pv.push_back(L"lowAge");
      pv.push_back(itow(low));
    }
    if (high>0) {
      pv.push_back(L"highAge");
      pv.push_back(itow(high));
    }

    wstring sex = encodeSex(pc->getSex());
    if (sex.empty())
      sex = L"B";

    pv.push_back(L"sex");
    pv.push_back(sex);

    if (pc->getNumStages()>1) {
      pv.push_back(L"numberInTeam");
      pv.push_back(itow(pc->getNumStages()));
    }

    if (pc->getClassType() == oClassRelay) {
      pv.push_back(L"teamEntry");
      pv.push_back(L"Y");
    }

    if (pc->getNoTiming()) {
      pv.push_back(L"timePresentation");
      pv.push_back(L"N");
    }

    xml.startTag("Class", pv);

    xml.write("ClassId", itow(pc->getId()));

    xml.write("ClassShortName", pc->getName());

    if (!pc->getType().empty())
      xml.write("ClassTypeId", itow(idMap[pc->getType()]));

    xml.endTag();
  }
  xml.endTag();
}

void oEvent::exportIOFClublist(xmlparser &xml)
{
  xml.startTag("ClubList");
  xml.write("IOFVersion", "version", L"2.0.3");

  for (oClubList::iterator it = Clubs.begin(); it!=Clubs.end(); ++it) {
    it->exportIOFClub(xml, true);
  }

  xml.endTag();
}

void cWrite(xmlparser &xml, const char *tag, const wstring &value) {
  if (!value.empty()) {
    xml.write(tag, value);
  }
}

void oClub::exportClubOrId(xmlparser &xml) const
{
  if (getExtIdentifier() != 0)
    xml.write("ClubId", getExtIdentifierString());
  else {
    exportIOFClub(xml, true);
  }
}

void oClub::exportIOFClub(xmlparser &xml, bool compact) const
{
  xml.startTag("Club");
  if (getExtIdentifier() != 0)
    xml.write("ClubId", getExtIdentifierString());
  else
    xml.write("ClubId");

  xml.write("ShortName", getName());

  if (compact) {
    xml.endTag();
    return;
  }

  wstring country = getDCI().getString("Nationality");
  if (!country.empty())
    xml.write("CountryId", "value", country);

  int district = getDCI().getInt("District");
  if (district>0)
    xml.write("OrganisationId", district);

  vector<wstring> pv;

  // Address
  wstring co = getDCI().getString("CareOf");
  wstring street = getDCI().getString("Street");
  wstring city = getDCI().getString("City");
  wstring zip = getDCI().getString("ZIP");

  if (!co.empty()) {
    pv.push_back(L"careOf");
    pv.push_back(co);
  }
  if (!street.empty()) {
    pv.push_back(L"street");
    pv.push_back(street);
  }
  if (!city.empty()) {
    pv.push_back(L"city");
    pv.push_back(city);
  }
  if (!zip.empty()) {
    pv.push_back(L"zipCode");
    pv.push_back(zip);
  }
  if (!pv.empty()) {
    xml.startTag("Address", pv);
    xml.endTag();
  }
  pv.clear();

  //Tele
  wstring mail = getDCI().getString("EMail");
  wstring phone = getDCI().getString("Phone");

  if (!mail.empty()) {
    pv.push_back(L"mailAddress");
    pv.push_back(mail);
  }
  if (!phone.empty()) {
    pv.push_back(L"phoneNumber");
    pv.push_back(phone);
  }
  if (!pv.empty()) {
    xml.startTag("Tele", pv);
    xml.endTag();
  }

  //Club
  xml.endTag();
}

void oEvent::exportIOFStartlist(xmlparser &xml)
{
  xml.startTag("StartList");
  xml.write("IOFVersion", "version", L"2.0.3");

  exportIOFEvent(xml);

  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    xml.startTag("ClassStart");
    //xml.write("ClassId", itos(it->getId()));
    xml.write("ClassShortName", it->getName());
    it->exportIOFStart(xml);
    xml.endTag();
  }
  xml.endTag();
}

void oClass::exportIOFStart(xmlparser &xml) {
  bool useEventor = oe->getPropertyInt("UseEventor", 0) == 1;

  if (getClassType() == oClassIndividual || getClassType() == oClassIndividRelay) {
    for (oRunnerList::iterator it = oe->Runners.begin(); it!=oe->Runners.end(); ++it) {
      if (it->getClassId(true) != getId() || it->isRemoved())
        continue;

      xml.startTag("PersonStart");

      it->exportIOFRunner(xml, true);

      if (it->getClubId()>0)
        it->Club->exportClubOrId(xml);

      int rank = it->getDCI().getInt("Rank");
      if (rank>0) {
        //Ranking
        xml.startTag("Rank");
        xml.write("Name", L"MeOS");
        xml.write("RankPosition", rank);
        xml.write("RankValue", rank);
        xml.endTag();
      }

      int multi = it->getNumMulti();
      if (multi==0)
        it->exportIOFStart(xml);
      else {
        xml.startTag("RaceStart");
        xml.write("EventRaceId", L"1");
        it->exportIOFStart(xml);
        xml.endTag();
        for (int k = 0; k < multi; k++) {
          pRunner r = it->getMultiRunner(k+1);
          if (r) {
            xml.startTag("RaceStart");
            xml.write("EventRaceId", k+2);
            r->exportIOFStart(xml);
            xml.endTag();
          }
        }
      }
      xml.endTag();
    }
  }
  else if (getClassType() == oClassRelay || getClassType() == oClassPatrol) {

    // A bug in Eventor / OLA results in an internal error if a patrol has a team name.
    // Set writeTeamName to true (or remove) when this bug is fixed.
    bool writeTeamName = !useEventor || getClassType() != oClassPatrol;

    for (oTeamList::iterator it = oe->Teams.begin(); it!=oe->Teams.end(); ++it) {
      if (it->getClassId(true) != getId() || it->isRemoved())
        continue;

      xml.startTag("TeamStart");

      if (writeTeamName)
        xml.write("TeamName", it->getName());

      wstring nat = it->getDCI().getString("Nationality");
      if (!nat.empty())
        xml.write("CountryId", "value", nat);

      for (size_t k=0; k<it->Runners.size(); k++) {
        if (it->Runners[k]) {
          xml.startTag("PersonStart");

          pRunner parent = it->Runners[k]->getMultiRunner(0);
          if (parent != 0)
            parent->exportIOFRunner(xml, true);

          if (it->Runners[k]->getClubId()>0) {
            it->Runners[k]->Club->exportClubOrId(xml);
          }
          else if (it->getClubId()>0) {
            it->Club->exportClubOrId(xml);
          }

          it->Runners[k]->exportIOFStart(xml);
          xml.endTag();
        }
      }

      xml.endTag();
    }
  }
}

void oRunner::exportIOFStart(xmlparser &xml)
{
  xml.startTag("Start");
  int sno = getStartNo();
  if (sno>0)
    xml.write("StartNumber", sno);

  xml.startTag("StartTime");
  xml.write("Clock", "clockFormat", L"HH:MM:SS",
              formatTimeIOF(getStartTime(), oe->ZeroTime));
  xml.endTag();

  wstring bib = getBib();
  if (!bib.empty())
    xml.write("BibNumber", bib);

  if (getCardNo() > 0)
    xml.write("CCardId", getCardNo());

  int len = 0;
  if (getCourse(false)) {
    len = getCourse(false)->getLength();
    if (len>0)
      xml.write("CourseLength", "unit", L"m", itow(len));

    wstring start = getCourse(false)->getStart();
    if (!start.empty())
      xml.write("StartId", max(1, getNumberSuffix(start)));
  }

  if (tInTeam) {
    xml.write("TeamSequence", tLeg+1);
  }

  xml.endTag();
}

void oRunner::exportIOFRunner(xmlparser &xml, bool compact)
{
  wstring sex = encodeSex(getSex());

  if (sex.length()==1)
    xml.startTag("Person", "sex", sex);
  else
    xml.startTag("Person");

  if (getExtIdentifier() != 0)
    xml.write("PersonId", getExtIdentifierString());
  else
    xml.write("PersonId");

  xml.startTag("PersonName");
  xml.write("Family", getFamilyName());
  xml.write("Given", "sequence", L"1", getGivenName());
  xml.endTag();

  int year = getBirthYear();

  if (year>0 && !compact) {
    xml.startTag("BirthDate");
    xml.write("Date", "dateFormat", L"YYYY", itow(extendYear(year)));
    xml.endTag();
  }

  xml.endTag();
}

void oEvent::exportIOFResults(xmlparser &xml, bool selfContained, const set<int> &classes, int leg, bool oldStylePatol)
{
  vector<SplitData> dummy;
  xml.startTag("ResultList");

  xml.write("IOFVersion", "version", L"2.0.3");
  wstring hhmmss = L"HH:MM:SS";
  exportIOFEvent(xml);

  bool ClassStarted=false;
  int Id=-1;
  bool skipClass=false;
  if (oldStylePatol) {
    // OLD STYLE PATROL EXPORT
    for (oTeamList::iterator it=Teams.begin(); it != Teams.end(); ++it) {
      if (it->isRemoved())
        continue;
      if (it->Runners.size()>=2 && it->Runners[0] && it->Runners[1]) {
        if (it->getClassId(true)!=Id) {
          if (ClassStarted) xml.endTag();

          if (!it->Class || it->Class->getClassType()!=oClassPatrol) {
            skipClass=true;
            ClassStarted=false;
            continue;
          }

          if ((!classes.empty() && classes.count(it->getClassId(true)) == 0) || leg != -1) {
            skipClass=true;
            ClassStarted=false;
            continue;
          }

          skipClass=false;
          xml.startTag("ClassResult");
          ClassStarted=true;
          Id=it->getClassId(true);

          xml.write("ClassShortName", it->getClass(true));
        }

        if (skipClass)
          continue;

        xml.startTag("PersonResult");
          it->Runners[0]->exportIOFRunner(xml, true);

          xml.startTag("Club");
            xml.write("ClubId", 0);
            xml.write("ShortName", it->Runners[1]->getName());
            xml.write("CountryId", "value", it->getDI().getString("Nationality"));
          xml.endTag();

          xml.startTag("Result");
            xml.startTag("StartTime");
              xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getStartTime(), ZeroTime));
            xml.endTag();
            xml.startTag("FinishTime");
              xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getLegFinishTime(-1), ZeroTime));
            xml.endTag();

            xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(it->getLegRunningTime(-1, true, false), 0));
            xml.write("ResultPosition", it->getLegPlaceS(-1, false));

            xml.write("CompetitorStatus", "value", it->Runners[0]->getIOFStatusS());

            const vector<SplitData> &sp=it->Runners[0]->getSplitTimes(true);

            pCourse pc=it->Runners[0]->getCourse(false);
            if (pc) xml.write("CourseLength", "unit", L"m", pc->getLengthS());

            pCourse pcourse=pc;
            auto legStatus = it->getLegStatus(-1, true, false);
            if (pcourse && legStatus>0 && legStatus!=StatusDNS && legStatus!=StatusCANCEL) {
              int no = 1;
              bool hasRogaining = pcourse->hasRogaining();
              int startIx = pcourse->useFirstAsStart() ? 1 : 0;
              int endIx = pcourse->useLastAsFinish() ? pcourse->nControls - 1 : pcourse->nControls;
              for (int k=startIx;k<endIx;k++) {
                if (pcourse->Controls[k]->isRogaining(hasRogaining))
                  continue;
                xml.startTag("SplitTime", "sequence", itos(no++));
                xml.write("ControlCode", pcourse->Controls[k]->getFirstNumber());
                if (unsigned(k)<sp.size() && sp[k].getTime(false)>0)
                  xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(sp[k].getTime(false) -it->tStartTime, 0));
                else
                  xml.write("Time", L"--:--:--");

                xml.endTag();
              }
            }
          xml.endTag();
        xml.endTag();
      }
    }

    if (ClassStarted) {
      xml.endTag();
      ClassStarted = false;
    }
  }
  // OldStylePatrol

  if (leg == -1)
    exportTeamSplits(xml, classes, oldStylePatol);

  skipClass=false;
  Id=-1;

  for (oRunnerList::iterator it=Runners.begin();
        it != Runners.end(); ++it) {

    if (it->isRemoved() || (leg != -1 && it->tLeg != leg) || it->isVacant())
      continue;

    if (it->getClassId(true)!=Id) {
      if (ClassStarted) xml.endTag();

      if (!it->Class) {
        skipClass=true;
        ClassStarted=false;
        continue;
      }

      ClassType ct = it->Class->getClassType();

      if (leg == -1 && (ct == oClassPatrol || ct ==oClassRelay || ct == oClassIndividRelay) ) {
        skipClass=true;
        ClassStarted=false;
        continue;
      }

      if ( (!classes.empty() && classes.count(it->getClassId(true)) == 0) ) {
        skipClass=true;
        ClassStarted=false;
        continue;
      }

      xml.startTag("ClassResult");
      ClassStarted=true;
      skipClass=false;
      Id=it->getClassId(true);

      xml.write("ClassShortName", it->getClass(true));
    }

    if (skipClass)
      continue;

    xml.startTag("PersonResult");

      it->exportIOFRunner(xml, true);

      if (it->Club)
        it->Club->exportIOFClub(xml, true);

      xml.startTag("Result");
        xml.startTag("CCard");
          xml.write("CCardId", it->getCardNo());
        xml.endTag();
        xml.startTag("StartTime");
        xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getStartTime(), ZeroTime));
        xml.endTag();
        xml.startTag("FinishTime");
        xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getFinishTimeAdjusted(false), ZeroTime));
        xml.endTag();

        xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(it->getRunningTime(true),0));
        xml.write("ResultPosition", it->getPlaceS());

        xml.write("CompetitorStatus", "value", it->getIOFStatusS());

        const vector<SplitData> &sp=it->getSplitTimes(true);
        pCourse pc=it->getCourse(false);
        if (pc) xml.write("CourseLength", "unit", L"m", pc->getLengthS());

        pCourse pcourse=it->getCourse(true);
        if (pcourse && it->getStatus()>0 && it->getStatus()!=StatusDNS
          && it->getStatus()!=StatusNotCompetiting && it->getStatus() != StatusCANCEL) {
          bool hasRogaining = pcourse->hasRogaining();
          int no = 1;
          int startIx = pcourse->useFirstAsStart() ? 1 : 0;
          int endIx = pcourse->useLastAsFinish() ? pcourse->nControls - 1 : pcourse->nControls;
          for (int k=startIx;k<endIx;k++) {
            if (pcourse->Controls[k]->isRogaining(hasRogaining))
              continue;
            xml.startTag("SplitTime", "sequence", itos(no++));
            xml.write("ControlCode", pcourse->Controls[k]->getFirstNumber());
            if (unsigned(k)<sp.size() && sp[k].getTime(false)>0)
              xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(sp[k].getTime(false) - it->tStartTime, 0));
            else
              xml.write("Time", L"--:--:--");

            xml.endTag();
          }
        }
      xml.endTag();
    xml.endTag();
  }

  if (ClassStarted) {
    xml.endTag();
    ClassStarted = false;
  }

  xml.endTag();
}

void oEvent::exportTeamSplits(xmlparser &xml, const set<int> &classes, bool oldStylePatrol)
{
  wstring hhmmss = L"HH:MM:SS";
  vector<SplitData> dummy;
  bool ClassStarted=false;
  int Id=-1;
  bool skipClass=false;

  sortTeams(ClassResult, -1, true);
  for(oTeamList::iterator it=Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassId(true)!=Id) {
      if (ClassStarted) {
        xml.endTag();
        ClassStarted = false;
      }

      if (!it->Class) {
        skipClass=true;
        continue;
      }

      ClassType ct = it->Class->getClassType();

      if (oldStylePatrol && ct == oClassPatrol) {
        skipClass=true;
        continue;
      }

      if (ct != oClassRelay && ct != oClassIndividRelay && ct != oClassPatrol) {
        skipClass=true;
        continue;
      }

      if (!classes.empty() && classes.count(it->getClassId(true)) == 0) {
        skipClass=true;
        continue;
      }

      skipClass=false;
      xml.startTag("ClassResult");
      ClassStarted=true;
      Id=it->getClassId(true);

      xml.write("ClassShortName", it->getClass(true));
    }

    if (skipClass)
      continue;

    xml.startTag("TeamResult"); {
      /*
    <TeamName>Sundsvalls OK</TeamName>
    <BibNumber></BibNumber>
    <StartTime>
     <Clock clockFormat="HH:MM:SS">10:00:00</Clock>
    </StartTime>
    <FinishTime>
     <Clock clockFormat="HH:MM:SS">11:45:52</Clock>
    </FinishTime>
    <Time>
     <Time timeFormat="HH:MM:SS">01:45:52</Time>
    </Time>
    <ResultPosition>1</ResultPosition>
    <TeamStatus value="OK"></TeamStatus>
      */
      wstring nat = it->getDCI().getString("Nationality");
      if (!nat.empty())
        xml.write("CountryId", "value", nat);

      xml.write("TeamName", it->getName());
      xml.write("BibNumber", it->getStartNo());

      xml.startTag("StartTime");
        xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getStartTime(), ZeroTime));
      xml.endTag();
      xml.startTag("FinishTime");
      xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(it->getFinishTimeAdjusted(false), ZeroTime));
      xml.endTag();

      xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(it->getRunningTime(true), 0));
      xml.write("ResultPosition", it->getPlaceS());
      xml.write("TeamStatus", "value", it->getIOFStatusS());

      for (size_t k=0;k<it->Runners.size();k++) {
        if (!it->Runners[k])
          continue;
        pRunner r=it->Runners[k];

        xml.startTag("PersonResult"); {

          r->exportIOFRunner(xml, true);

          if (r->Club)
            r->Club->exportIOFClub(xml, true);

          xml.startTag("Result"); {
            xml.write("TeamSequence", k+1);
            xml.startTag("StartTime");
              xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(r->getStartTime(), ZeroTime));
            xml.endTag();
            xml.startTag("FinishTime");
            xml.write("Clock", "clockFormat", hhmmss, formatTimeIOF(r->getFinishTimeAdjusted(false), ZeroTime));
            xml.endTag();

            xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(r->getRunningTime(true), 0));
            xml.write("ResultPosition", r->getPlaceS());

            xml.write("CompetitorStatus", "value", r->getIOFStatusS());

            const vector<SplitData> &sp = r->getSplitTimes(true);

            pCourse pc=r->getCourse(false);

            if (pc) {
              xml.startTag("CourseVariation");
              xml.write("CourseVariationId", pc->getId());
              xml.write("CourseLength", "unit", L"m", pc->getLengthS());
              xml.endTag();
            }
            pCourse pcourse=pc;
            if (pcourse && r->getStatus()>0 && r->getStatus()!=StatusDNS
                  && r->getStatus()!=StatusNotCompetiting && r->getStatus() != StatusCANCEL) {
              int no = 1;
              bool hasRogaining = pcourse->hasRogaining();
              int startIx = pcourse->useFirstAsStart() ? 1 : 0;
              int endIx = pcourse->useLastAsFinish() ? pcourse->nControls - 1 : pcourse->nControls;
              for (int k=startIx;k<endIx;k++) {
                if (pcourse->Controls[k]->isRogaining(hasRogaining))
                  continue;
                xml.startTag("SplitTime", "sequence", itos(no++));
                xml.write("ControlCode", pcourse->Controls[k]->getFirstNumber());
                if (unsigned(k)<sp.size() && sp[k].getTime(false)>0)
                  xml.write("Time", "timeFormat", hhmmss, formatTimeIOF(sp[k].getTime(false) - it->tStartTime, 0));
                else
                  xml.write("Time", L"--:--:--");

                xml.endTag();
              } //Loop over splits
            }
          } xml.endTag();
        } xml.endTag();
      } //Loop over team members

    }  xml.endTag();    // Team result
  }

  if (ClassStarted) {
    xml.endTag();
    ClassStarted = false;
  }
}

void oEvent::exportIOFSplits(IOFVersion version, const wchar_t *file,
                             bool oldStylePatrolExport, bool useUTC,
                             const set<int> &classes,
                             const pair<string, string>& preferredIdTypes, int leg,
                             bool teamsAsIndividual, bool unrollLoops,
                             bool includeStageInfo, bool forceSplitFee,
                             bool useEventorQuirks) {
  xmlparser xml;

  xml.openOutput(file, false);
  oClass::initClassId(*this, classes);
  reEvaluateAll(classes, true);
  if (version != IOF20)
    calculateResults(classes, ResultType::ClassCourseResult);
  calculateResults(classes, ResultType::TotalResult);
  calculateResults(classes, ResultType::ClassResult);
  calculateTeamResults(classes, ResultType::TotalResult);
  calculateTeamResults(classes, ResultType::ClassResult);

  sortRunners(SortOrder::ClassResult);
  sortTeams(SortOrder::ClassResult, -1, false);

  if (version == IOF20)
    exportIOFResults(xml, true, classes, leg, oldStylePatrolExport);
  else {
    IOF30Interface writer(this, forceSplitFee, useEventorQuirks);
    writer.setPreferredIdType(preferredIdTypes);
    writer.writeResultList(xml, classes, leg, useUTC, 
                           teamsAsIndividual, unrollLoops, includeStageInfo);
  }

  xml.closeOut();
}

void oEvent::exportIOFStartlist(IOFVersion version, const wchar_t *file, bool useUTC,
                                const set<int> &classes, 
                                const pair<string, string>& preferredIdTypes,
                                bool teamsAsIndividual,
                                bool includeStageInfo, 
                                bool forceSplitFee,
                                bool useEventorQuirks) {
  xmlparser xml;
  
  oClass::initClassId(*this, classes);
  xml.openOutput(file, false);

  if (version == IOF20)
    exportIOFStartlist(xml);
  else {
    IOF30Interface writer(this, forceSplitFee, useEventorQuirks);
    writer.setPreferredIdType(preferredIdTypes);
    writer.writeStartList(xml, classes, useUTC, teamsAsIndividual, includeStageInfo);
  }
  xml.closeOut();
}
