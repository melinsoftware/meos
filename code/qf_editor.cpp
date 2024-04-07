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

#include "stdafx.h"
#include "gdioutput.h"
#include "qf_editor.h"
#include "qualification_final.h"
#include "localizer.h"
#include "oClass.h"
#include "meosexception.h"
#include "TabBase.h"

QFEditor::QFEditor() {
  emptyScheme();
}

QFEditor::~QFEditor() = default;

void QFEditor::emptyScheme() {
  currentQF = make_shared<QualificationFinal>(0, 0);
  data.clear();
  data.resize(2, { 1, {} });
  data[0].second.emplace_back(lang.tl("Kval"));
  data[1].second.emplace_back(lang.tl("Final"));
  numLevels = 2;
  makeDirty(DirtyFlag::ClearDirty, DirtyFlag::ClearDirty);
}

void QFEditor::load(const shared_ptr<QualificationFinal>& qf) {
  currentQF = make_shared<QualificationFinal>(*qf);
  data.clear();
  data.resize(currentQF->getNumLevels());
  for (int j = 0; j < currentQF->getNumClasses(); j++) {
    int level = currentQF->getLevel(j+1);
    data.at(level).second.push_back(currentQF->getInstance(j));
    data[level].first++;
  }
  numLevels = data.size();
}

void QFEditor::makeQF() {
  vector<QFClass> linear;
  for (int j = 0; j < numLevels; j++) {
    for (int i = 0; i < data[j].first; i++) {
      linear.push_back(data[j].second[i]);
      linear.back().level = j;
    }
  }
  
  currentQF->setClasses(linear);
}


void QFEditor::show(gdioutput& gdi) {
  gdi.clearPage(false);
  gdi.pushX();
  gdi.fillRight();
  gdi.addButton("CloseEditor", "Stäng").setHandler(this);
  gdi.addButton("New", "Nytt").setHandler(this);
  gdi.addButton("Load", "Ladda").setHandler(this);
  gdi.addButton("Save", "Exportera").setHandler(this);
  if (cls)
    gdi.addButton("Use", L"Use in X#" + cls->getName()).setHandler(this);
  
  gdi.popX();

  updateActions(gdi);

  gdi.fillDown();
  gdi.dropLine(3);
  gdi.addSelection("Levels", 100, 100, nullptr, L"Antal nivåer:").setHandler(this);
  
  vector<pair<wstring, size_t>> levels;
  for (int i = 2; i <= 10; i++) 
    levels.emplace_back(itow(i), i);

  gdi.setItems("Levels", levels);
  gdi.selectItemByData("Levels", numLevels);
  gdi.autoGrow("Levels");
  gdi.setRestorePoint("LevelView");

  showLevels(gdi);
  gdi.refresh();
}

void QFEditor::showLevels(gdioutput& gdi) const {
  for (int level = 0; level < numLevels; level++) {
    showLevel(gdi, level, data[level].first, data[level].second);
  }
}

void QFEditor::showLevel(gdioutput& gdi, int level, size_t numRaces, const vector<QFClass>& classes) const {
  const int cxBase = gdi.getCX();
  int cy = gdi.getCY();

  gdi.setRestorePoint("Level" + itos(level));
  gdi.fillDown(); 

  RECT rcR = { cxBase, cy, cxBase + gdi.scaleLength(400), cy + gdi.scaleLength(2) };
  gdi.addRectangle(rcR, colorDarkBlue);

  gdi.addString("", fontMediumPlus, "Nivå X#" + itos(level + 1));
  string cKey = "NumCls" + itos(level);
  gdi.addSelection(cKey, 100, 100, nullptr, L"Antal klasser:").setHandler(this).setExtra(level);

  vector<pair<wstring, size_t>> levels;
  
  for (int i = 1; i <= 32; i++)
    levels.emplace_back(itow(i), i);

  gdi.setItems(cKey, levels);
  gdi.autoGrow(cKey.c_str());
  gdi.selectItemByData(cKey, numRaces);
  bool rankBased = classes.size() > 0 && classes[0].rankLevel;

  if (level > 0) {
    gdi.addCheckbox("Rank" + itos(level), "Klass efter ranking", nullptr, rankBased,
      "Använd ranking istället för placering i kval för att placera kvalificerade löpare i klasser").setExtra(level).setHandler(this);
  }
//  auto rc = gdi.getDimensionSince("Level" + itos(level));

  gdi.setCX(gdi.scaleLength(230));
  
  const int cxCls = gdi.getCX();
  int cx = cxCls;
  gdi.fillDown();
  cy += gdi.scaleLength(10);
  const int w = gdi.scaleLength(24 * 10);
  int ww, hh;
  gdi.getTargetDimension(ww, hh);
  ww = max(ww, gdi.scaleLength(500));
  for (int i = 0; i < numRaces; i++) {
    gdi.setCY(cy);
    gdi.setCX(cx);
    string CK = getKeyTag(level, i);
    auto& ref = gdi.addInput("Name" + CK, classes[i].name.empty() ? wstring(L"???") : classes[i].name, 24, nullptr,
      L"Klass X (namnsuffix):#" + itow(i + 1));
    ref.setHandler(this).setExtra(code(level, i));
    
    if (level > 0) {
      gdi.addString("QInfo" + CK, 0, classes[i].getQualInfo());
      gdi.addButton("Edit", "Redigera...").setHandler(this).setExtra(code(level, i));
    }
    cx += w;
    if (cx + w > ww) {
      cx = cxCls;
      gdi.dropLine(2);
      cy = gdi.getCY();
    }
  }

  gdi.dropLine(3);
  gdi.setCX(cxBase);
}

void QFEditor::loadQualificationView(gdioutput& gdi, const QFClass& cls, int cCode) {
  gdi.clearPage(false);
  gdi.fillDown();
  gdi.addString("", boldLarge, L"Kvalificeringsregler för X#" + cls.name);

  gdi.pushY();
  gdi.setRestorePoint("X");
  gdi.addListBox("Rules", 120, 150, nullptr, L"Regler").setExtra(cCode).setHandler(this);
  gdi.setItems("Rules", getRules(cls));
  gdi.addButton("RemoveRule", "Ta bort").setHandler(this).setExtra(cCode);
  gdi.disableInput("RemoveRule");


  gdi.popY();
  gdi.dropLine();
  auto rc = gdi.getDimensionSince("X");
  gdi.setCX(rc.right + gdi.scaleLength(10));

  rcRuleForm.left = gdi.getCX();
  rcRuleForm.top = gdi.getCY();
  gdi.dropLine(0.8);
  gdi.setCX(rcRuleForm.left + gdi.scaleLength(10));
  gdi.pushX();
  gdi.fillDown();
  gdi.addString("", fontMediumPlus, "Lägg till regler");
  gdi.dropLine();

  gdi.addSelection("RuleType", 200, 100).setHandler(this).setExtra(cCode);

  vector<pair<wstring, size_t>> ruleType;
  for (auto* type : { "Klass / placering", "Bästa tid", "Övriga okvalificerade"}) {
    ruleType.emplace_back(lang.tl(type), ruleType.size());
  }
  gdi.setItems("RuleType", ruleType);
  gdi.selectFirstItem("RuleType");
  gdi.setRestorePoint("RuleType");
  //other.emplace_back(lang.tl("Inga"), 0);
  
  //gdi.addString("", gdiFonts::boldText, "Klass / placering");
  addRuleClassPlace(cCode, gdi);

  /*gdi.dropLine(2.5);
  rcE.bottom = gdi.getCY();
  rcE.right = gdi.getWidth();
  gdi.addRectangle(rcE, GDICOLOR::colorLightCyan);
  gdi.dropLine();

  gdi.setCX(rcE.right - gdi.scaleLength(75));
  gdi.setCY(gdi.getHeight());
  gdi.addButton("Close", "Stäng").setHandler(this);
  gdi.refresh();*/
}

void QFEditor::addRuleClassPlace(int cCode, gdioutput& gdi) {
  pair<int, int> levelClass = decode(cCode);
  gdi.addSelection("Classes", 150, 200, nullptr, L"Kvalklass:");
  if (levelClass.first > 0 && levelClass.first < data.size()) {
    vector<pair<wstring, size_t>> prevLevel;
    for (int i = levelClass.first - 1; i >= 0; i--) {
      auto& dLine = data[i];
      for (int j = 0; j < dLine.first; j++)
        prevLevel.emplace_back(dLine.second[j].name, code(i, j));
    }
    gdi.setItems("Classes", prevLevel);
    gdi.autoGrow("Classes");
    gdi.selectFirstItem("Classes");
  }
  gdi.fillRight();
  gdi.addInput("Places", L"", 10, nullptr, L"Placeringar");
  gdi.fillDown();
  gdi.dropLine(0.9);
  gdi.addButton("AddClassPlace", "Lägg till").setHandler(this).setExtra(cCode);
  gdi.popX();
  endAddRuleForm(gdi);
}

void QFEditor::addRuleTime(int cCode, gdioutput& gdi) {
  gdi.dropLine();
  gdi.addString("", gdiFonts::boldText, "Bästa tid");
  gdi.addButton("AddBestTime", "Lägg till").setHandler(this).setExtra(cCode);
  endAddRuleForm(gdi);
}

void QFEditor::addRuleOther(int cCode, gdioutput &gdi) {
//  gdi.addString("", gdiFonts::boldText, "Övriga okvalificerade:");

  gdi.pushX();
  gdi.fillRight();
  gdi.addSelection("Other", 100, 200).setExtra(cCode).setHandler(this);
  vector<pair<wstring, size_t>> other;
  other.emplace_back(lang.tl("Inga"), 0);
  other.emplace_back(lang.tl("Alla"), 1);
  other.emplace_back(lang.tl("N bästa"), 2);
  other.emplace_back(lang.tl("Tidskval"), 3);
  gdi.setItems("Other", other);
  gdi.autoGrow("Other");

  gdi.addButton("AddOther", "Lägg till").setHandler(this).setExtra(cCode);

  gdi.popX();
  gdi.dropLine(3);
  QFClass& cls = getQFClass(cCode);

  int toSel = 1;
  if (cls.extraQualification == QFClass::ExtraQualType::All)
    toSel = 1;
  else if (cls.extraQualification == QFClass::ExtraQualType::NBest)
    toSel = 2;
  else if (cls.extraQualification == QFClass::ExtraQualType::TimeLimit)
    toSel = 3;

  gdi.selectItemByData("Other", toSel);
  extraSelPosX = gdi.getCX();
  extraSelPosY = gdi.getCY();
  updateExtraField(gdi, cls, cCode);
  gdi.setCY(extraSelPosY);
  gdi.dropLine(3);
  endAddRuleForm(gdi);
}

void QFEditor::endAddRuleForm(gdioutput &gdi) {
  gdi.dropLine(2.5);
  rcRuleForm.bottom = gdi.getCY();
  rcRuleForm.right = gdi.getWidth();
  gdi.addRectangle(rcRuleForm, GDICOLOR::colorLightCyan);
  gdi.dropLine();

  gdi.setCX(rcRuleForm.right - gdi.scaleLength(75));
  gdi.setCY(gdi.getHeight());
  gdi.addButton("Close", "Stäng").setHandler(this);
  gdi.refresh();
}

void QFEditor::updateExtraField(gdioutput& gdi, const QFClass& cls, int code) {
  int type = gdi.getSelectedItem("Other").first;
  if (type == 2) {
    wstring def = cls.extraQualification == QFClass::ExtraQualType::NBest ? itow(cls.extraQualData) : L"0";
    if (!gdi.hasWidget("NBest"))
      gdi.addInput(extraSelPosX, extraSelPosY, "NBest", def, 6).setHandler(this).setExtra(code);
    else
      gdi.setText("NBest", def)->setExtra(code);
  }
  else if (gdi.hasWidget("NBest"))
    gdi.removeWidget("NBest");

  if (type == 3) {
    wstring def = cls.extraQualification == QFClass::ExtraQualType::TimeLimit ?
      formatTimeMS(cls.extraQualData, false) : L"0:00";

    if (!gdi.hasWidget("TimeLimit"))
      gdi.addInput(extraSelPosX, extraSelPosY, "TimeLimit", def , 6).setHandler(this).setExtra(code);
    else
      gdi.setText("TimeLimit", def)->setExtra(code);
  }
  else if (gdi.hasWidget("TimeLimit"))
    gdi.removeWidget("TimeLimit");
}

vector<pair<wstring, size_t>> QFEditor::getRules(const QFClass& cls) const {
  vector<pair<wstring, size_t>> qf;
  for (int j = 0; j < cls.qualificationMap.size(); j++) {
    auto& q = cls.qualificationMap[j];
    int qIx = q.first;
    pair<int, int> ix = getLevelIndexFromLinear(qIx);
    if (ix.first == -1)
      qf.emplace_back(L"???", -1);
    else {
      qf.emplace_back(itow(j+1) + L": " + data[ix.first].second[ix.second].name + L" / " + itow(q.second), j);
    }
  }

  int base = cls.qualificationMap.size() + 1;
  for (int j = 0; j < cls.numTimeQualifications; j++) {
    qf.emplace_back(itow(base++) + lang.tl(" Bästa tid"), 1000 + j);
  }

  if (cls.extraQualification == QFClass::ExtraQualType::All)
    qf.emplace_back(itow(base++) + lang.tl(" Alla övriga"), 10000);
  else if(cls.extraQualification == QFClass::ExtraQualType::NBest)
    qf.emplace_back(itow(base++) + lang.tl(" X bästa#" + itos(cls.extraQualData) ), 10000);
  else if (cls.extraQualification == QFClass::ExtraQualType::TimeLimit)
    qf.emplace_back(itow(base++) + lang.tl(L" Tidskval: X#" + formatTimeMS(cls.extraQualData, false)), 10000);

  return qf;
}

QFClass& QFEditor::getQFClass(int id) {
  auto key = decode(id);
  if (key.first < data.size() && key.second < data[key.first].second.size()) {
    return data[key.first].second[key.second];
  }
  throw meosException("Internal error");
}

const QFClass& QFEditor::getQFClass(int id) const {
  return const_cast<QFEditor*>(this)->getQFClass(id);
}

int QFEditor::getLinearIndex(int level, int ix) const {
  int lx = 0;
  for (int j = 0; j <= level; j++) {
    if (j < level)
      lx += data[j].first;
    else
      return lx + ix + 1; // One indexed
  }
  return -1;
}

pair<int, int> QFEditor::getLevelIndexFromLinear(int linearIndex) const {
  int lx = 0;
  for (int j = 0; j <= numLevels; j++) {
    if (lx + data[j].first < linearIndex)
      lx += data[j].first;
    else {
      return make_pair(j, linearIndex - lx - 1); // linearIndex is one indexed
    }
  }
  return make_pair(-1, -1);
}

vector<pair<int, int>> QFEditor::getLevelIndex() const {
  vector<pair<int, int>> out;
  out.emplace_back(-1, -1);
  for (int j = 0; j < numLevels; j++) {
    for (int i = 0; i < data[j].first; i++)
      out.emplace_back(j+1, i+1);
  }
  return out;
}

void QFEditor::updateLevelMap(const vector<pair<int, int>>& oldIndex) {
  auto newIndex = getLevelIndex();
  map<pair<int, int>, int> levelMap;
  for (int j = 1; j < newIndex.size(); j++) 
    levelMap[newIndex[j]] = j;
  
  for (auto& d : data) {
    for (auto &dc : d.second) {
      for (auto& clpPlace : dc.qualificationMap) {
        if (clpPlace.first < oldIndex.size()) {
          auto res = levelMap.find(oldIndex[clpPlace.first]);
          if (res == levelMap.end())
            clpPlace.first = -1;
          else
            clpPlace.first = res->second;
        }
        else {
          clpPlace.first = -1;
        }
      }
      auto it = remove_if(dc.qualificationMap.begin(), dc.qualificationMap.end(), [](auto v)->bool {return v.first == -1; });
      dc.qualificationMap.erase(it, dc.qualificationMap.end());
    }
  }
}

QFClass& QFEditor::getQFClassFromIndex(int linearIndex) {
  auto v = getLevelIndexFromLinear(linearIndex);
  return data[v.first].second[v.second];
}

void QFEditor::handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) {
  if (type == GUI_TIMER) {
    if (info.id == "Reload") {
      gdi.restore("LevelView", false);
      showLevels(gdi);
      gdi.refresh();
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo& ii = dynamic_cast<InputInfo&>(info);
    if (ii.id.substr(0, 4) == "Name") {
      if (trim(ii.text).empty())
        ii.setBgColor(GDICOLOR::colorLightRed);
      else {
        ii.setBgColor(GDICOLOR::colorDefault);
      }
      ii.refresh();
    }
  }
  else if (type == GUI_INPUT) {
    InputInfo& ii = dynamic_cast<InputInfo&>(info);
    if (ii.id.substr(0, 4) == "Name") {
      QFClass& cls = getQFClass(ii.getExtraInt());
      wstring tt = trim(ii.text);
      if (!tt.empty()) {
        if (tt != cls.name && !(cls.name.empty() && tt == ii.getPreviousText())) {
          cls.name = tt;
          edit(gdi);
        }
      }
      else {
        gdi.setText(ii.id, ii.getPreviousText());
        ii.setBgColor(GDICOLOR::colorDefault);
      }
    }
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo& bi = dynamic_cast<ButtonInfo&>(info);
    if (bi.id == "AddClassPlace") {
      wstring place = gdi.getText("Places");
      vector<wstring> sPlace;
      split(place, L" ;,", sPlace);
      vector<int> iPlace;
      for (auto& sp : sPlace) {
        int p = _wtoi(sp.c_str());
        if (p > 0)
          iPlace.push_back(p);
      }
      if (iPlace.empty())
        throw meosException("Ange vilka placeringar i kalssen som kvalificerar hit, t.ex. '1,3'");
      
      pair<int, int> levelCls = decode(bi.getExtraInt());
      QFClass& cls = getQFClass(bi.getExtraInt());

      int codedTarget = gdi.getSelectedItem("Classes").first;
      pair<int, int> levelClsTgt = decode(codedTarget);
      
      int linearCode = getLinearIndex(levelClsTgt.first, levelClsTgt.second);
      int level = levelCls.first;
      wstring errInfo;
      for (int place : iPlace) {
        pair<int, int> codedClassPlace(linearCode, place);

        bool add = true;
        for (int j = 0; j < data.size(); j++) {
          for (int i = 0; i < data[j].first; i++) {
            auto& c = data[j].second[i];
            if (count(c.qualificationMap.begin(), c.qualificationMap.end(), codedClassPlace)) {
              wstring info = lang.tl(L"Placering X kvalificerar till Y#" + itow(place) + L"#" + c.name);
              if (errInfo.empty())
                errInfo = info;
              else
                errInfo = L"\n" + info;
              add = false;
              break;
            }
          }
        }
        if (add)
          cls.qualificationMap.push_back(codedClassPlace);

        // Clear from unused data
        for (int j = 0; j < data.size(); j++) {
          for (int i = data[j].first; i < data[j].second.size(); i++) {
            auto& c = data[j].second[i];
            if (!c.qualificationMap.empty())
              c.qualificationMap.erase(remove(c.qualificationMap.begin(), c.qualificationMap.end(), codedClassPlace));
          }
        }
      }
      sort(cls.qualificationMap.begin(), cls.qualificationMap.end());
      gdi.setItems("Rules", getRules(cls));
      updateQInfo(cls);
      gdi.setText("Places", L"", true);
      if (!errInfo.empty()) {
        gdi.alert(errInfo);
      }
    }
    else if (bi.id == "AddBestTime") {
      QFClass& cls = getQFClass(bi.getExtraInt());
      cls.numTimeQualifications++;
      gdi.setItems("Rules", getRules(cls));
      updateQInfo(cls);
      gdi.setText("Places", L"", true);
    }
    else if (bi.id == "AddOther") {
      QFClass& cls = getQFClass(bi.getExtraInt());
      ListBoxInfo lbi;
      gdi.getSelectedItem("Other", lbi);
      int tp = lbi.data;
      if (tp == 0)
        cls.extraQualification = QFClass::ExtraQualType::None;
      else if (tp == 1)
        cls.extraQualification = QFClass::ExtraQualType::All;
      else if (tp == 2) {       
        cls.extraQualification = QFClass::ExtraQualType::NBest;
        cls.extraQualData = max(_wtoi(gdi.getText("NBest").c_str()), 0);
      }
      else if (tp == 3) {       
        cls.extraQualification = QFClass::ExtraQualType::TimeLimit;
        cls.extraQualData = convertAbsoluteTimeMS(gdi.getText("TimeLimit").c_str());
        if (cls.extraQualData == NOTIME || cls.extraQualData < 0)
          cls.extraQualData = 0;
      }

      gdi.setItems("Rules", getRules(cls));
      updateQInfo(cls);
    }
    else if (bi.id == "RemoveRule") {
      auto sel = gdi.getSelectedItem("Rules");
      QFClass& cls = getQFClass(bi.getExtraInt());
      if (sel.second) {
        int id = sel.first;
        if (id < 1000) {
          cls.qualificationMap.erase(cls.qualificationMap.begin() + id);
        }
        else if (id < 10000 && cls.numTimeQualifications > 0)
          cls.numTimeQualifications--;
        else if (id == 10000)
          cls.extraQualification = QFClass::ExtraQualType::None;

        gdi.setItems("Rules", getRules(cls));
        updateQInfo(getQFClass(bi.getExtraInt()));
        gdi.disableInput("RemoveRule");
      }
    }
    else if (bi.id.substr(0, 4) == "Rank") {
      int level = bi.getExtraInt();
      if (level < data.size()) {
        edit(gdi);
        bool rnk = gdi.isChecked(bi.id);
        for (auto& d : data[level].second)
          d.rankLevel = rnk;
      }
      else
        throw meosException("Internal error");
    }
    else if (bi.id == "Edit") {
      QFClass& qfc = getQFClass(bi.getExtraInt());
      pair<int, int> LIX = decode(bi.getExtraInt());
      infoTextTag = "QInfo" + getKeyTag(LIX.first, LIX.second);
      infoTextGdi = &gdi;

      gdioutput* qual_settings = getExtraWindow("qualification", true);
      if (!qual_settings) {
        qual_settings = createExtraWindow("qualification", L"Qualification", gdi.scaleLength(700), gdi.scaleLength(500), true);
        loadQualificationView(*qual_settings, qfc, bi.getExtraInt());
      }
    }
    else if (bi.id == "Close") {
      gdioutput* qual_settings = getExtraWindow("qualification", false);
      if (qual_settings) {
        qual_settings->closeWindow();
        infoTextGdi = nullptr;
      }
    }
    else if (bi.id == "New" || bi.id == "CloseEditor") {
      if (checkSave(gdi)) {
        gdioutput* qual_settings = getExtraWindow("qualification", false);
        if (qual_settings) {
          qual_settings->closeWindow();
          infoTextGdi = nullptr;
        }
        if (bi.id == "New") {
          emptyScheme();
          if (cls  && cls->getQualificationFinal())
            cls = nullptr; // Clear connection to class
          show(gdi);
        }
        else {
          gdi.getTabs().get(TClassTab)->loadPage(gdi);
          return;
        }
      }
    }
    else if (bi.id == "Use") {
      if (cls) {
        use();
        updateActions(gdi);
      }
    }
    else if (bi.id == "Load") {
      vector<pair<wstring, wstring>> ext;
      ext.push_back(make_pair(L"Qualfication/Final", L"*.xml"));
      wstring fileName = gdi.browseForOpen(ext, L"xml");
      if (!fileName.empty()) {
        auto newQF = make_shared<QualificationFinal>(0, 0);
        newQF->importXML(fileName);
        load(newQF);
        makeDirty(QFEditor::DirtyFlag::MakeDirty, QFEditor::DirtyFlag::ClearDirty);
        show(gdi);
      }
    }
    else if (bi.id == "Save") {
      if (save(gdi))
        updateActions(gdi);
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo& lbi = dynamic_cast<ListBoxInfo&>(info);

    if (lbi.id == "Levels") {
      auto origiIndex = getLevelIndex();
      numLevels = lbi.data;
      if (numLevels > data.size()) {
        data.resize(numLevels);
        for (auto& d : data) {
          if (d.first == 0) {
            d.first = 1;
            d.second.emplace_back(lang.tl("Klass"));
          }
        }
      }
      edit(gdi);
      updateLevelMap(origiIndex);
      gdi.addTimeoutMilli(25, "Reload", nullptr).setHandler(this);
      gdioutput* qual_settings = getExtraWindow("qualification", false);
      if (qual_settings)
        qual_settings->closeWindow();
    }
    else if (lbi.id.substr(0, 6) == "NumCls") {
      int numCls = lbi.data;
      int level = lbi.getExtraInt();
      if (level < data.size()) {
        auto origiIndex = getLevelIndex();
        wstring bn = lang.tl("Class");
        bool rnk = false;
        if (!data[level].second.empty()) {
          bn = data[level].second.back().name;
          rnk = data[level].second.front().rankLevel;
        }
        while (numCls >= data[level].second.size()) {
          data[level].second.emplace_back(bn);
          data[level].second.back().rankLevel = rnk;
        }
        data[level].first = numCls;
        updateLevelMap(origiIndex);
      }
      else
        throw meosException("Internal error");

      edit(gdi);
      gdi.addTimeoutMilli(25, "Reload", nullptr).setHandler(this);
      gdioutput* qual_settings = getExtraWindow("qualification", false);
      if (qual_settings)
        qual_settings->closeWindow();
    }
    else if (lbi.id == "Rules") {
      gdi.setInputStatus("RemoveRule", lbi.data != -1);
    }
    else if (lbi.id == "RuleType") {
      int cCode = lbi.getExtraInt();
      gdi.restoreNoUpdate("RuleType");
      if (lbi.data == 0)
        addRuleClassPlace(cCode, gdi);
      else if (lbi.data == 1)
        addRuleTime(cCode, gdi);
      else if (lbi.data == 2)
        addRuleOther(cCode, gdi);
    }
    else if (lbi.id == "Other") {
      QFClass& cls = getQFClass(lbi.getExtraInt());
      updateExtraField(gdi, cls, lbi.getExtraInt());
    }
  }
}

string QFEditor::getKeyTag(int level, int ix) {
  return "L" + itos(level) + "C" + itos(ix);
}

void QFEditor::updateQInfo(const QFClass& cls) {
  if (infoTextGdi) {
    edit(*infoTextGdi);
    infoTextGdi->setText(infoTextTag, lang.tl(cls.getQualInfo()), true);
  }
}

bool QFEditor::checkSave(gdioutput& gdi) {
  if (cls != nullptr && dirtyInt) {
    auto ans = gdi.askCancel(L"Vill du uppdatera X med ändringarna?#" + cls->getName());
    if (ans == gdioutput::AskAnswer::AnswerCancel)
      return false;
    else if (ans == gdioutput::AskAnswer::AnswerYes)
      use();
  }
  else if (cls == nullptr && dirtyExt) {
    auto ans = gdi.askCancel(L"Vill du spara ändringar?");
    if (ans == gdioutput::AskAnswer::AnswerCancel)
      return false;
    else if (ans == gdioutput::AskAnswer::AnswerYes) {
      if (!save(gdi))
        return false;
    } 
  }
  return true;
}

void QFEditor::makeDirty(DirtyFlag inside, DirtyFlag outside) {
  if (inside == DirtyFlag::ClearDirty)
    dirtyInt = false;
  else if (inside == DirtyFlag::MakeDirty)
    dirtyInt = true;

  if (outside == DirtyFlag::ClearDirty)
    dirtyExt = false;
  else if (outside == DirtyFlag::MakeDirty)
    dirtyExt = true;
}

void QFEditor::edit(gdioutput& gdi) {
  makeDirty(DirtyFlag::MakeDirty, DirtyFlag::MakeDirty);
  updateActions(gdi);
}

void QFEditor::updateActions(gdioutput& gdi) {
  gdi.setInputStatus("Use", dirtyInt, true);
  gdi.setInputStatus("Save", dirtyExt);
}

void QFEditor::use() {
  makeQF();
  cls->loadQualificationFinalScheme(*currentQF);
  cls->updateFinalClasses(0, true);  
  makeDirty(QFEditor::DirtyFlag::ClearDirty, QFEditor::DirtyFlag::NoTouch);
}

bool QFEditor::save(gdioutput& gdi) {
  vector<pair<wstring, wstring>> ext;
  ext.push_back(make_pair(L"Qualfication/Final", L"*.xml"));
  int fx = 0;
  wstring fileName = gdi.browseForSave(ext, L"xml", fx);
  if (!fileName.empty()) {
    makeQF();
    currentQF->exportXML(fileName);
    makeDirty(QFEditor::DirtyFlag::NoTouch, QFEditor::DirtyFlag::ClearDirty);
    return true;
  }
  return false;
}
