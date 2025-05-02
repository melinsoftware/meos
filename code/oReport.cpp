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

#include "stdafx.h"

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <deque>

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"

#include "oDataContainer.h"

#include "random.h"
#include "SportIdent.h"

#include "oFreeImport.h"

#include "meos.h"
#include "meos_util.h"
#include "RunnerDB.h"
#include "MeOSFeatures.h"
#include "cardsystem.h"

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <deque>

#include "Localizer.h"


void oEvent::generateCompetitionReport(gdioutput &gdi)
{
  gdi.fillDown();
  gdi.addString("", boldLarge, "Tävlingsstatistik");


  int lh = gdi.getLineHeight();

  oClassList::iterator it;

  reinitializeClasses();
  Classes.sort();

  gdi.dropLine();
 //  int xp = gdi.getCX();
//  int yp = gdi.getCY();
  int entries=0;
  int started=0;
  int fee=0;
//  int dx[]={0, 150, 200, 250, 350, 450};

  int entries_sum=0;
  int started_sum=0;
  int fee_sum=0;

  int entries_sum_y=0;
  int started_sum_y=0;
  int fee_sum_y=0;

  int cfee;
  gdi.addString("", boldText, "Elitklasser");
  vector<ClassMetaType> types;
  types.push_back(ctElite);
  cfee = getDCI().getInt("EliteFee");
  generateStatisticsPart(gdi, types, set<int>(), cfee, false, 90, entries, started, fee);
  entries_sum += entries;
  started_sum +=  started;
  fee_sum += fee;

  gdi.addString("", boldText, "Vuxenklasser");
  types.clear();
  types.push_back(ctNormal);
  types.push_back(ctExercise);
  cfee = getDCI().getInt("EntryFee");
  generateStatisticsPart(gdi, types, set<int>(), cfee, false, 90, entries, started, fee);
  entries_sum += entries;
  started_sum +=  started;
  fee_sum += fee;

  gdi.addString("", boldText, "Ungdomsklasser");
  types.clear();
  types.push_back(ctYouth);
  cfee = getDCI().getInt("YouthFee");
  generateStatisticsPart(gdi, types, set<int>(), cfee, true, 50, entries, started, fee);
  entries_sum_y += entries;
  started_sum_y +=  started;
  fee_sum_y += fee;

  types.clear();
  types.push_back(ctOpen);

  gdi.addString("", boldText, "Öppna klasser, vuxna");
  set<int> adultFee;
  set<int> youthFee;

  cfee = getDCI().getInt("EntryFee");
  if (cfee > 0)
    adultFee.insert(cfee);

  for(it=Classes.begin(); it!=Classes.end(); ++it) {
    if (!it->isRemoved() && it->interpretClassType() == ctOpen) {
      int af = it->getDCI().getInt("ClassFee");
      if (af > 0)
        adultFee.insert(af);
      int yf = it->getDCI().getInt("ClassFeeRed");
      if (yf > 0)
        youthFee.insert(yf);
    }
  }

  generateStatisticsPart(gdi, types, adultFee, cfee, false, 90, entries, started, fee);
  entries_sum += entries;
  started_sum +=  started;
  fee_sum += fee;

  gdi.addString("", boldText, "Öppna klasser, ungdom");

  cfee = getDCI().getInt("YouthFee");
  if (cfee > 0)
    youthFee.insert(cfee);

  generateStatisticsPart(gdi, types, youthFee, cfee, true, 50, entries, started, fee);
  entries_sum_y += entries;
  started_sum_y +=  started;
  fee_sum_y += fee;

  gdi.addString("", boldText, "Sammanställning");
  gdi.dropLine();
  int xp = gdi.getCX();
  int yp = gdi.getCY();

  gdi.addString("", yp, xp+200, textRight|fontMedium, "Vuxna");
  gdi.addString("", yp, xp+300, textRight|fontMedium, "Ungdom");
  gdi.addString("", yp, xp+400, textRight|fontMedium, "Totalt");

  yp+=lh;
  gdi.addString("", yp, xp+0, fontMedium, "Anmälda");
  gdi.addStringUT(yp, xp+200, textRight|fontMedium, itos(entries_sum));
  gdi.addStringUT(yp, xp+300, textRight|fontMedium, itos(entries_sum_y));
  gdi.addStringUT(yp, xp+400, textRight|boldText, itos(entries_sum+entries_sum_y));

  yp+=lh;
  gdi.addString("", yp, xp+0, fontMedium, "Startande");
  gdi.addStringUT(yp, xp+200, textRight|fontMedium, itos(started_sum));
  gdi.addStringUT(yp, xp+300, textRight|fontMedium, itos(started_sum_y));
  gdi.addStringUT( yp, xp+400, textRight|boldText, itos(started_sum+started_sum_y));

  yp+=lh;
  gdi.addString("", yp, xp+0, fontMedium, "Grundavgift");
  gdi.addStringUT(yp, xp+200, textRight|fontMedium, itos(fee_sum));
  gdi.addStringUT(yp, xp+300, textRight|fontMedium, itos(fee_sum_y));
  gdi.addStringUT(yp, xp+400, textRight|boldText, itos(fee_sum+fee_sum_y));

  yp+=lh;
  gdi.addString("", yp, xp+0, fontMedium, "SOFT-avgift");
  gdi.addStringUT(yp, xp+200, textRight|fontMedium, itos(entries_sum*15));
  gdi.addStringUT(yp, xp+300, textRight|fontMedium, itos(entries_sum_y*5));
  gdi.addStringUT(yp, xp+400, textRight|boldText, itos(entries_sum*15+entries_sum_y*5));

  yp+=lh*2;
  gdi.addString("", yp, xp+0,fontMedium, "Underlag för tävlingsavgift:");
  int baseFee =  (fee_sum+fee_sum_y) - (entries_sum*15+5*entries_sum_y);
  gdi.addStringUT(yp, xp+200,fontMedium, itos(baseFee));

  yp+=lh;
  gdi.addString("", yp, xp+0,fontMedium, "Total tävlingsavgift:");
  int cmpFee =  int((baseFee * .34 * (baseFee - 5800)) / (200000));
  gdi.addStringUT(yp, xp+200, fontMedium, itos(cmpFee));

  yp+=lh;
  gdi.addString("", yp, xp, fontMedium, "Avrundad tävlingsavgift:");
  gdi.addStringUT(yp, xp+200, boldText, itos(((cmpFee+50))/100)+"00");

  gdi.dropLine();
  gdi.addString("", boldText, "Geografisk fördelning");
  gdi.addString("", fontSmall, "Anmälda per distrikt");
  gdi.dropLine(0.2);
  yp = gdi.getCY();
  vector<int> runners;
  vector<string> districts;
  getRunnersPerDistrict(runners);
  getDistricts(districts);
  int nd = min(runners.size(), districts.size());

  int ybase = yp;
  for (int k=1;k<=nd;k++) {
    gdi.addStringUT(yp, xp, fontMedium, itos(k) + " " + districts[k%nd]);
    gdi.addStringUT(yp, xp+200, textRight|fontMedium, itos(runners[k%nd]));
    yp+=lh;
    if (k%8==0){
      yp = ybase;
      xp += 250;
    }
  }
}

void oEvent::generateStatisticsPart(gdioutput &gdi, const vector<ClassMetaType> &type,
                                    const set<int> &feeLock, int actualFee, bool useReducedFee,
                                    int baseFee, int &entries_sum, int &started_sum, int &fee_sum) const
{
  entries_sum=0;
  started_sum=0;
  fee_sum=0;
  int xp = gdi.getCX();
  int yp = gdi.getCY();
  int entries;
  int started;
  int dx[]={0, 150, 210, 270, 350, 450};
  int lh = gdi.getLineHeight();
  oClassList::const_iterator it;

  gdi.addString("", yp, xp+dx[0], fontSmall, "Klass");
  gdi.addString("", yp, xp+dx[1], textRight|fontSmall, "Anm. avg.");
  gdi.addString("", yp, xp+dx[2], textRight|fontSmall, "Grund avg.");
  gdi.addString("", yp, xp+dx[3], textRight|fontSmall, "Anmälda");
  gdi.addString("", yp, xp+dx[4], textRight|fontSmall, "Avgift");
  gdi.addString("", yp, xp+dx[5], textRight|fontSmall, "Startande");
  yp+=lh;
  for(it=Classes.begin(); it!=Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    //int lowAge = it->getDCI().getInt("LowAge");
/*    int highAge = it->getDCI().getInt("HighAge");

    if (ageSpan.second > 0 && (highAge == 0 || highAge > ageSpan.second))
      continue;

    if (ageSpan.first > 0 && (highAge != 0 && highAge < ageSpan.first))
      continue;
*/
    if (count(type.begin(), type.end(), it->interpretClassType())==1) {
      it->getStatistics(feeLock, entries, started);
      gdi.addStringUT(yp, xp+dx[0], fontMedium, it->getName());

      int afee = it->getDCI().getInt("ClassFee");
      int redfee = it->getDCI().getInt("ClassFeeRed");

      int f = actualFee;

      if (afee > 0)
        f = afee;

      if (useReducedFee && redfee > 0)
        f = redfee;

      gdi.addStringUT(yp, xp+dx[1], textRight|fontMedium, itos(f));
      gdi.addStringUT(yp, xp+dx[2], textRight|fontMedium, itos(baseFee));
      gdi.addStringUT(yp, xp+dx[3], textRight|fontMedium, itos(entries));
      gdi.addStringUT(yp, xp+dx[4], textRight|fontMedium, itos(baseFee*entries));
      gdi.addStringUT(yp, xp+dx[5], textRight|fontMedium, itos(started));
      entries_sum += entries;
      started_sum += started;
      fee_sum += entries*baseFee;
      yp+=lh;
    }
  }
  yp+=lh/2;
  gdi.addStringUT(yp, xp+dx[3], textRight|boldText, itos(entries_sum));
  gdi.addStringUT(yp, xp+dx[4], textRight|boldText, itos(fee_sum));
  gdi.addStringUT(yp, xp+dx[5], textRight|boldText, itos(started_sum));
  gdi.dropLine();
}

void oEvent::getRunnersPerDistrict(vector<int> &runners) const
{
  runners.clear();
  runners.resize(24);

  oRunnerList::const_iterator it;

  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (!it->skip()) {
      int code = 0;
      if (it->Club)
        code = it->Club->getDCI().getInt("District");

      if (code>0 && code<24)
        ++runners[code];
      else
        ++runners[0];
    }
  }
}

void oEvent::getDistricts(vector<string> &districts)
{
  districts.resize(24);
  int i=0;
  districts[i++]="Övriga";
  districts[i++]="Blekinge";
  districts[i++]="Bohuslän-Dal";
  districts[i++]="Dalarna";
  districts[i++]="Gotland";
  districts[i++]="Gästrikland";
  districts[i++]="Göteborg";
  districts[i++]="Halland";
  districts[i++]="Hälsingland";
  districts[i++]="Jämtland-Härjedalan";
  districts[i++]="Medelpad";
  districts[i++]="Norrbotten";
  districts[i++]="Örebro län";
  districts[i++]="Skåne";
  districts[i++]="Småland";
  districts[i++]="Stockholm";
  districts[i++]="Södermanland";
  districts[i++]="Uppland";
  districts[i++]="Värmland";
  districts[i++]="Västerbotten";
  districts[i++]="Västergötland";
  districts[i++]="Västmanland";
  districts[i++]="Ångermanland";
  districts[i++]="Östergötland";
}


void oEvent::generatePreReport(gdioutput& gdi) {
  CurrentSortOrder = SortByName;
  Runners.sort();

  int lVacId = getVacantClub(false);
  int y = gdi.getCY();
  int x = gdi.getCX();
  int lh = gdi.getLineHeight();
  bool compactClub = getPropertyBool("CompactClubName", false);

  auto getClub = [compactClub](oAbstractRunner& rt) -> const wstring & {
    pClub clb = rt.getClubRef();
    if (clb) {
      if (compactClub)
        return clb->getCompactName();
      else
        return clb->getDisplayName();
    }
    else
      return _EmptyWString;
  };

  gdi.addStringUT(2, lang.tl(L"Rapport inför: ") + getName());
  gdi.addStringUT(1, getDate());
  gdi.dropLine();
  
  vector<pRunner> no_card;
  vector<pRunner> no_start;
  vector<pRunner> no_class;
  vector<pRunner> no_course;
  vector<pRunner> no_club;
  vector<pRunner> too_many_controls;

  for (auto& r : Runners) {
    if (r.isRemoved())
      continue;

    bool needStartTime = true;
    bool needCourse = true;

    pClass pc = r.Class;
    if (pc) {
      LegTypes lt = pc->getLegType(r.tLeg);
      if (lt == LTIgnore) {
        needStartTime = false;
        needCourse = false;
      }
      if (pc->hasDirectResult())
        needCourse = false;

      StartTypes st = pc->getStartType(r.tLeg);

      if (st != STTime && st != STDrawn)
        needStartTime = false;

      if (pc->hasFreeStart() || pc->hasRequestStart())
        needStartTime = false;
    }
    if (r.getClubId() != lVacId) {
      if (needCourse && r.getCardNo() == 0)
        no_card.push_back(&r);
      if (needStartTime && r.getStartTime() == 0)
        no_start.push_back(&r);
      if (r.getClubId() == 0)
        no_club.push_back(&r);
    }

    pCourse crs = r.getCourse(false);
    int cNo = r.getCardNo();

    if (r.getClassId(false) == 0)
      no_class.push_back(&r);
    else if (needCourse && !crs)
      no_course.push_back(&r);

    if (cNo > 0 && crs) {
      int maxP = getCardSystem().getMaxNumPunch(cNo);
      if (maxP > 0 && maxP < crs->getNumControls())
        too_many_controls.push_back(&r);
    }
  }

  deque<pRunner> si_duplicate;

  if (Runners.size() > 1) {
    Runners.sort(oRunner::CompareCardNumber);
    map<int, vector<pRunner> > initDup;

    auto r_it = Runners.begin();
    while (++r_it != Runners.end()) {
      oRunnerList::iterator r_it2 = r_it;
      r_it2--;
      int cno = r_it->getCardNo();
      if (cno && r_it2->getCardNo() == cno) {
        vector<pRunner>& sid = initDup[cno];
        if (sid.empty() || sid.back()->getId() != r_it2->getId())
          sid.push_back(&*r_it2);

        sid.push_back(&*r_it);
      }
    }

    for (map<int, vector<pRunner> >::const_iterator it = initDup.begin(); it != initDup.end(); ++it) {
      const vector<pRunner>& eq = it->second;
      vector<char> added(eq.size());
      for (size_t k = 0; k < eq.size(); k++) {
        if (added[k])
          continue;

        for (size_t j = 0; j < eq.size(); j++) {
          if (j == k)
            continue;
          if (!eq[k]->canShareCard(eq[j], eq[k]->getCardNo())) {
            if (!added[k]) {
              si_duplicate.push_back(eq[k]);
              added[k] = 1;
            }
            if (!added[j]) {
              si_duplicate.push_back(eq[j]);
              added[j] = 1;
            }
          }
        }
      }
    }

  }

  const string Ellipsis = "[ ... ]";

  int tab[5] = { 0, 100, 350, 420, 550 };
  for (int& t : tab)
    t = gdi.scaleLength(t);


  auto showRunner = [&gdi, tab](int x, pRunner r, bool showCard, bool showStartTime) {
    wstring name;
    int y = gdi.getCY();
    if (r->Class)
      gdi.addStringUT(y, x + tab[0], 0, r->getClass(true));

    name += r->getCompleteIdentification(oRunner::IDType::ParallelLegExtra);

    if (r->getCardNo() > 0 && showCard)
      name += L" (" + itow(r->getCardNo()) + L")";

    gdi.addStringUT(y, x + tab[1], 0, name);

    if (showStartTime) 
      gdi.addStringUT(y, x + tab[3], 0, r->getStartTimeS());
  };

  auto showHeader = [&gdi](const string& detail, int count) {
    gdi.pushX();
    gdi.fillRight();
    if (count == 0) {
      gdi.addImage("", gdi.getCY() - gdi.scaleLength(2), gdi.getCX(), 0, itow(IDI_MEOSCHECK), gdi.scaleLength(16), gdi.scaleLength(16));
      gdi.fillDown();
      gdi.addString("", 1, detail + "#" + itos(count)).setColor(colorDarkGreen);
    }
    else {
      gdi.addImage("", gdi.getCY() - gdi.scaleLength(2), gdi.getCX(), 0, itow(IDI_MEOSERROR), gdi.scaleLength(16), gdi.scaleLength(16));
      gdi.fillDown();
      gdi.addString("", 1, detail + "#" + itos(count)).setColor(colorDarkRed);
    }
    gdi.popX();
    gdi.dropLine(0.2);
  };

  auto showReport = [&gdi, tab, &showRunner, &showHeader](const auto& problemRunners, const string &detail) {
    showHeader(detail, problemRunners.size());
    int maxShow = 0;
    const int x = gdi.getCX();
    for (auto& r : problemRunners) {
      if (++maxShow > 20) {
        gdi.addStringUT(1, "[ ... ]");
        break;
      }
      showRunner(x, r, true, false);
    }
  };

  showReport(no_class, "Löpare utan klass: X");

  if (getMeOSFeatures().withCourses(this)) {
    gdi.dropLine();
    showReport(no_course, "Löpare utan bana: X");
  }

  if (oe->getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    gdi.dropLine();
    showReport(no_club, "Löpare utan klubb: X");
  }

  gdi.dropLine();
  showReport(no_start, "Löpare utan starttid: X");

  gdi.dropLine();
  showReport(no_card, "Löpare utan SI-bricka: X");

  gdi.dropLine();
  showReport(si_duplicate, "SI-dubbletter: X");


  if (oe->deprecateOldCards()) {
    vector<pRunner> oldCard;
    auto& cardSystem = oe->getCardSystem();
    for (auto& r : Runners) {
      if (r.isRemoved())
        continue;
      if (r.getCardNo() > 0 && cardSystem.isDeprecated(r.getCardNo())) {
        oldCard.push_back(&r);
      }
    }

    gdi.dropLine();
    showReport(oldCard, "Löparbrickor av äldre typ: X.");
  }
  else if (useLongTimes()) { // Warn SICard5 + long times
    vector<pRunner> si5Card;
    for (auto &r : Runners) {
      if (r.isRemoved())
        continue;
      if (r.getCardNo() > 0 && r.getCardNo() < 300000) {
        si5Card.push_back(&r);
      }
    }
    gdi.dropLine();
    showReport(si5Card, "Gamla brickor utan stöd för långa tider: X.");
  }

  if (getMeOSFeatures().withCourses(this)) {
    gdi.dropLine();
    showReport(too_many_controls, "Löpare med fler kontroller än brickan kan innehålla: X.");
    
    map<pair<int, int>, int> counterCourseStartTime;
    map<pair<int, int>, int> counterFirstControlStartTime;
    vector<pRunner> filterR;
    for (auto& r : Runners) {
      if (r.isRemoved() || !r.Class)
        continue;
      pClass cls = r.getClassRef(true);
      if (cls->hasCoursePool() || cls->hasFreeStart())
        continue;
      if (cls->isForked(r.getLegNumber()))
        continue;
      if (cls->getStartType(r.getLegNumber()) != StartTypes::STDrawn)
        continue;
      if (cls->getLegType(r.getLegNumber()) == LTIgnore ||
            cls->getLegType(r.getLegNumber()) == LTExtra)
        continue;
      int st = r.getStartTime();
      if (st <= 0)
        continue;

      pCourse crs = r.getCourse(false);
      if (!crs || crs->getNumControls() == 0)
        continue;
      ++counterCourseStartTime[make_pair(crs->getId(), st)];
      ++counterFirstControlStartTime[make_pair(crs->getControl(0)->getId(), st)];
      filterR.push_back(&r);
    }

    map<pair<int, int>, vector<pRunner>> counterCourseStartTimeR;
    map<pair<int, int>, vector<pRunner>> counterFirstControlStartTimeR;

    int cntDupCrs = 0, cntDupCtr = 0;
    for (pRunner r : filterR) {
      pCourse crs = r->getCourse(false);
      int st = r->getStartTime();
      if (counterCourseStartTime[make_pair(crs->getId(), st)] > 1) {
        counterCourseStartTimeR[make_pair(crs->getId(), st)].push_back(r);
        cntDupCrs++;
      }
      else if (counterFirstControlStartTime[make_pair(crs->getControl(0)->getId(), st)] > 1) {
        counterFirstControlStartTimeR[make_pair(crs->getControl(0)->getId(), st)].push_back(r);
        cntDupCtr++;
      }
    }

    gdi.dropLine();
    showHeader("Deltagare med samma bana och samma starttid X.", cntDupCtr);

    //gdi.addString("", 1, "Deltagare med samma bana och samma starttid X.#" + itos(cntDupCrs));
    int crsId = -1;
    int xc = gdi.getCX();
    for (auto& dup : counterCourseStartTimeR) {
      if (crsId != dup.first.first) {
        crsId = dup.first.first;
        gdi.dropLine(0.4);
        gdi.addStringUT(0, getCourse(crsId)->getName());
      }
      for (pRunner r : dup.second) {
        showRunner(xc, r, false, true);
      }
    }

    gdi.dropLine();
    showHeader("Deltagare med samma första kontroll och samma starttid X.", cntDupCtr);
    //gdi.addString("", 1, "Deltagare med samma första kontroll och samma starttid X.#" + itos(cntDupCtr));
    int ctrlId = -1;
    for (auto& dup : counterFirstControlStartTimeR) {
      if (ctrlId != dup.first.first) {
        ctrlId = dup.first.first;
        gdi.dropLine(0.4);
        gdi.addStringUT(0, getControl(ctrlId)->getName());
      }
      for (pRunner r : dup.second) {
        showRunner(xc, r, false, true);
      }
    }
  }

  map<int, int> objectMarkers;

  //List all competitors not in a team.
  if (oe->hasTeam()) {
    for (auto t_it = Teams.begin(); t_it != Teams.end(); ++t_it) {
      if (t_it->isRemoved())
        continue;
      pClass pc = getClass(t_it->getClassId(true));

      if (pc) {
        for (unsigned i = 0; i < pc->getNumStages(); i++) {
          pRunner r = t_it->getRunner(i);
          if (r) {
            ++objectMarkers[r->getId()];
          }
        }
      }
    }

    gdi.dropLine();
    gdi.addString("", 1, "Löpare som förekommer i mer än ett lag:");
    bool any = false;
    for (auto r_it = Runners.begin(); r_it != Runners.end(); ++r_it) {
      if (objectMarkers[r_it->getId()] > 1) {
        wstring name = r_it->getClass(true) + L": " + r_it->getName();
        if (!r_it->getClub().empty())
          name += L" (" + getClub(*r_it) + L")";

        gdi.addStringUT(0, name);
        any = true;
      }
    }
    if (!any)
      gdi.addStringUT(1, "0");
  }

  sortRunners(ClassStartTime);

  gdi.dropLine();
  gdi.addString("", 1, "Individuella deltagare");

  y = gdi.getCY();
  for (auto r_it = Runners.begin(); r_it != Runners.end(); ++r_it) {
    if (r_it->isRemoved())
      continue;
    if (objectMarkers.count(r_it->getId()) == 0) { //Only consider runners not in a team.
      gdi.addStringUT(y, x + tab[0], 0, r_it->getClass(true), tab[1] - tab[0]);
      wstring name = r_it->getName();
      if (!r_it->getClub().empty())
        name += L" (" + getClub(*r_it) + L")";
      gdi.addStringUT(y, x + tab[1], 0, name, tab[2] - tab[1]);
      gdi.addStringUT(y, x + tab[2], 0, itos(r_it->getCardNo()), tab[3] - tab[2]);
      gdi.addStringUT(y, x + tab[3], 0, r_it->getCourseName(), tab[4] - tab[3]);
      y += lh;
      pCourse pc = r_it->getCourse(true);

      if (pc) {
        vector<wstring> res = pc->getCourseReadable(101);
        for (size_t k = 0; k < res.size(); k++) {
          gdi.addStringUT(y, x + tab[1], 0, res[k]);
          y += lh;
        }
      }
    }
  }

  gdi.dropLine();
  gdi.addString("", 1, "Lag(flera)");

  for (auto t_it = Teams.begin(); t_it != Teams.end(); ++t_it) {
    pClass pc = getClass(t_it->getClassId(false));

    gdi.addStringUT(0, t_it->getClass(false) + L": " + t_it->getName() + L"  " + t_it->getStartTimeS());

    if (pc) {
      for (unsigned i = 0; i < pc->getNumStages(); i++) {
        pRunner r = t_it->getRunner(i);
        if (r) {
          gdi.addStringUT(0, r->getName() + L" SI: " + itow(r->getCardNo()));

          pCourse pcourse = r->getCourse(true);

          if (pcourse) {
            y = gdi.getCY();
            vector<wstring> res = pcourse->getCourseReadable(101);
            for (size_t k = 0; k < res.size(); k++) {
              gdi.addStringUT(y, x + tab[1], 0, res[k]);
              y += lh;
            }
          }
        }
        else
          gdi.addString("", 0, "Löpare saknas");
      }
    }
    gdi.dropLine();
  }

  gdi.updateScrollbars();
}

