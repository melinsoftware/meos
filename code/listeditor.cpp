/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2023 Melin Software HB

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

#include <algorithm>

#include "listeditor.h"
#include "metalist.h"
#include "gdioutput.h"
#include "meosexception.h"
#include "gdistructures.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "oEvent.h"
#include "tabbase.h"
#include "CommDlg.h"
#include "generalresult.h"
#include "gdiconstants.h"
#include "autocomplete.h"
#include "image.h"

extern Image image;

ListEditor::ListEditor(oEvent *oe_) {
  oe = oe_;
  currentList = 0;
  currentIndex = -1;
  dirtyExt = false;
  dirtyInt = false;
  lastSaved = NotSaved;
  oe->loadGeneralResults(false, true);
}

namespace {
  const wstring &getSearchString() {
    return lang.tl(L"Sök (X)#Ctrl+F");
  }
}

ListEditor::~ListEditor() {
  setCurrentList(0);
}

void ListEditor::setCurrentList(MetaList *lst) {
  delete currentList;
  currentList = lst;
}

void ListEditor::load(const MetaListContainer &mlc, int index) {
  const MetaList &mc = mlc.getList(index);
  setCurrentList(new MetaList());
  *currentList = mc;
  
  if (mlc.isInternal(index)) {
    currentIndex = -1;
    currentList->clearTag();
  }
  else
    currentIndex = index;
  
  dirtyExt = true;
  dirtyInt = false;
  savedFileName.clear();
}

void ListEditor::show(TabBase *dst, gdioutput &gdi) {

  gdi.clearPage(false);
  origin = dst;
  show(gdi);
}

int editListCB(gdioutput* gdi, int type, void* data);

void ListEditor::show(gdioutput &gdi) {

  gdi.setRestorePoint("BeginListEdit");

  gdi.pushX();
  gdi.setCX(gdi.getCX() + gdi.scaleLength(6));

  int bx = gdi.getCX();
  int by = gdi.getCY();

  if (currentList)
    gdi.addString("", boldLarge, makeDash(L"Listredigerare - X#") + currentList->getListName());
  else
    gdi.addString("", boldLarge, "Listredigerare");

  gdi.setOnClearCb(editListCB);
  gdi.setData("ListEditorClz", this);
  gdi.dropLine(0.5);
  gdi.fillRight();

  gdi.addButton("EditList", "Egenskaper", editListCB);
  gdi.addButton("SplitPrint", "Sträcktidslista", editListCB);

  gdi.setCX(gdi.getCX() + gdi.scaleLength(32));
  gdi.addButton("OpenFile", "Öppna fil", editListCB);
  gdi.addButton("OpenInside", "Öppna från aktuell tävling", editListCB);

  if (savedFileName.empty())
    gdi.addButton("SaveFile", "Spara som fil", editListCB);
  else {
    gdi.addButton("SaveFile", L"Spara fil", editListCB, L"#" + savedFileName);
    gdi.addButton("SaveFileCopy", "Spara som...", editListCB);
  }

  gdi.addButton("SaveInside", "Spara i aktuell tävling", editListCB);
  gdi.addButton("NewList", "Ny lista", editListCB);
  gdi.addButton("RemoveInside", "Radera", editListCB, "Radera listan från aktuell tävling");
  gdi.setInputStatus("RemoveInside", currentIndex != -1);
  gdi.addButton("Close", "Stäng", editListCB);

  gdi.dropLine(2);

  int dx = gdi.getCX();
  
  if (currentList && currentList->isSplitPrintList()) {
    gdi.setCX(bx);
    gdi.addString("", 0, "Välj deltagare för förhandsgranskning:");
    gdi.addSelection("Runner", 300, 300, editListCB);
    oe->fillRunners(gdi, "Runner", false, 0);
    if (currentRunnerId > 0 && oe->getRunner(currentRunnerId, 0))
      gdi.selectItemByData("Runner", currentRunnerId);
    else {
      gdi.selectFirstItem("Runner");
      currentRunnerId = gdi.getSelectedItem("Runner").first;
    }
    gdi.dropLine(2);
  }
  
  int dy = gdi.getCY();

  RECT rc;
  int off = gdi.scaleLength(6);
  rc.left = bx - 2 * off;
  rc.right = dx + 2 * off;

  rc.top = by - off;
  rc.bottom = dy + off;

  gdi.addRectangle(rc, colorWindowBar);

  gdi.dropLine();
  gdi.popX();


  makeDirty(gdi, NoTouch, NoTouch);
  if (!currentList) {
    gdi.disableInput("EditList");
    gdi.disableInput("SplitPrint");
    gdi.disableInput("SaveFile");
    gdi.disableInput("SaveFileCopy", true);
    gdi.disableInput("SaveInside");
    gdi.refresh();
    return;
  }

  MetaList &list = *currentList;

  const vector< vector<MetaListPost> > &head = list.getHead();
  gdi.fillDown();
  gdi.addString("", 1, "Rubrik");
  gdi.pushX();
  gdi.fillRight();
  const double buttonDrop = 2.2;
  int lineIx = 100;
  for (size_t k = 0; k < head.size(); k++) {
    showLine(gdi, head[k], lineIx++);
    gdi.popX();
    gdi.dropLine(buttonDrop);
  }
  gdi.fillDown();
  gdi.addButton("AddLine0", "Lägg till rad", editListCB);

  gdi.dropLine(0.5);
  gdi.addString("", 1, "Underrubrik");
  gdi.pushX();
  gdi.fillRight();
  const vector< vector<MetaListPost> > &subHead = list.getSubHead();
  lineIx = 200;
  for (size_t k = 0; k < subHead.size(); k++) {
    showLine(gdi, subHead[k], lineIx++);
    gdi.popX();
    gdi.dropLine(buttonDrop);
  }
  gdi.fillDown();
  gdi.addButton("AddLine1", "Lägg till rad", editListCB);

  gdi.dropLine(0.5);
  gdi.addString("", 1, "Huvudlista");
  gdi.pushX();
  gdi.fillRight();
  const vector< vector<MetaListPost> > &mainList = list.getList();
  lineIx = 300;
  for (size_t k = 0; k < mainList.size(); k++) {
    showLine(gdi, mainList[k], lineIx++);
    gdi.popX();
    gdi.dropLine(buttonDrop);
  }
  gdi.fillDown();
  gdi.addButton("AddLine2", "Lägg till rad", editListCB);

  gdi.dropLine(0.5);
  gdi.addString("", 1, "Underlista");
  gdi.pushX();
  gdi.fillRight();
  const vector< vector<MetaListPost> > &subList = list.getSubList();
  lineIx = 400;
  for (size_t k = 0; k < subList.size(); k++) {
    showLine(gdi, subList[k], lineIx++);
    gdi.popX();
    gdi.dropLine(buttonDrop);
  }
  gdi.fillDown();
  gdi.addButton("AddLine3", "Lägg till rad", editListCB);

  gdi.setRestorePoint("EditList");
  renderListPreview(gdi);
  gdi.refresh();
}

void ListEditor::renderListPreview(gdioutput &gdi) {
  gdi.dropLine(2);
  gdi.fillDown();
  RECT rc;
  pRunner splitPrintR = nullptr;
  if (currentList->isSplitPrintList() && currentRunnerId)
    splitPrintR = oe->getRunner(currentRunnerId, 0);

  oListInfo li;
  oListParam par;
  par.pageBreak = false;
  par.splitAnalysis = true;
  par.setLegNumberCoded(-1);
  par.inputNumber = 0;

  if (splitPrintR) {
    par.selection.insert(splitPrintR->getClassId(true));
    par.showInterTimes = false;
    par.setLegNumberCoded(splitPrintR->getLegNumber());
    par.filterMaxPer = 3;
    par.alwaysInclude = splitPrintR;
    par.showHeader = false;
  }
  double originalScale = 0;
  try {
    rc.left = gdi.getCX();
    rc.right = gdi.getCX() + gdi.getWidth() - gdi.scaleLength(20);
    rc.top = gdi.getCY();
    rc.bottom = rc.top + 4;
    gdi.addRectangle(rc, colorDarkGreen, false, false);
    gdi.dropLine();

    currentList->interpret(oe, gdi, par, li);
    
    if (splitPrintR) {
      auto& sp = *li.getSplitPrintInfo();
      li.getParam().filterMaxPer = sp.numClassResults;

      const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
      if (!wideFormat)
        li.shrinkSize();

      rc.left = gdi.getCX();
      rc.top = gdi.getCY();
      gdi.setCX(gdi.getCX() + gdi.scaleLength(10));
      gdi.dropLine();

      splitPrintR->printSplits(gdi, &li);

      gdi.dropLine();
      rc.right = rc.left + gdi.scaleLength(250);
      rc.bottom = gdi.getHeight();
      gdi.addRectangle(rc, GDICOLOR::colorLightYellow);
      gdi.refresh();
    }
    else {
      oe->generateList(gdi, false, li, true);
    }
  }
  catch (meosException& ex) {
    gdi.addString("", 1, "Listan kan inte visas").setColor(colorRed);
    gdi.addString("", 0, ex.wwhat());
  }
  catch (std::exception& ex) {
    gdi.addString("", 1, "Listan kan inte visas").setColor(colorRed);
    gdi.addString("", 0, ex.what());
  }
}

int editListCB(gdioutput *gdi, int type, void *data)
{
  void *clz = gdi->getData("ListEditorClz");
  ListEditor *le = (ListEditor *)clz;
  BaseInfo *bi = (BaseInfo *)data;
  if (le)
    return le->editList(*gdi, type, *bi);

  throw meosException("Unexpected error");
}

void ListEditor::showLine(gdioutput &gdi, const vector<MetaListPost> &line, int ix) const {
  for (size_t k = 0; k < line.size(); k++) {
    addButton(gdi, line[k], gdi.getCX(), gdi.getCY(), ix, k);
  }

  gdi.addButton("AddPost", "Lägg till ny", editListCB).setExtra(ix);
  gdi.addButton("AddImage", "Lägg till bild", editListCB).setExtra(ix);
}

ButtonInfo &ListEditor::addButton(gdioutput &gdi, const MetaListPost &mlp, int x, int y, int lineIx, int ix) const {
  wstring cap;
  if (mlp.getType() == L"String") {
    cap = L"Text: X#" + mlp.getText();
  }
  else if (mlp.getType() == L"Image") {
    if (mlp.getText().empty())
      cap = L"Image";
    else {
      uint64_t imgId = mlp.getImageId();
      if (!image.hasImage(imgId))
        cap = L"Error";
      else
        cap = image.getFileName(imgId);
    }
  }
  else {
    const wstring &text = mlp.getText();
    if (text.length() > 0) {
      if (text[0] == '@') {
        vector<wstring> part; 
        split(text.substr(1), L";", part);
        for (int j = 0; j < part.size(); j++) {
          if (part[j].empty())
            continue;
          else if (part[j][0] == '@')
            part[j] = part[j].substr(1);
          if (!cap.empty())
            cap += L"|";
          else
            cap += L"#";

          cap += lang.tl(part[j] + L"#" + itow(j));
        }
      }
      else
        cap = text + L"#" + lang.tl(mlp.getType());
    }
    else {
      cap = mlp.getType();
    }
  }

  ButtonInfo &bi = gdi.addButton(x, y, "EditPost" + itos(lineIx * 100 + ix), cap, editListCB);
  return bi;
}

static void getPosFromId(int id, int &groupIx, int &lineIx, int &ix) {
  lineIx = id / 100;
  ix = id % 100;
  groupIx = (lineIx / 100) - 1;
  lineIx = lineIx % 100;
}

int ListEditor::editList(gdioutput &gdi, int type, BaseInfo &data) {
  int lineIx, groupIx, ix;
  
  if (type == GUI_EVENT) {
    EventInfo &ev = *(EventInfo *)(&data);
    if (ev.getKeyCommand() == KC_FIND) {
      gdi.setInputFocus("SearchText", true);
    }
    else if (ev.getKeyCommand() == KC_FINDBACK) {
      gdi.setInputFocus("SearchText", false);
    }
  }
  else if (type == GUI_FOCUS) {
    InputInfo &ii = *(InputInfo *)(&data);

    if (ii.text == getSearchString()) {
      ((InputInfo *)gdi.setText("SearchText", L""))->setFgColor(colorDefault);
    }
  }
  else if (type == GUI_INPUTCHANGE) {
    InputInfo &ii = *(InputInfo *)(&data);
    if (ii.id == "SearchText") {
      bool show = false;
      if (ii.text.length() > 1) {
        vector<AutoCompleteRecord> rec;
        MetaList::getAutoComplete(ii.text, rec);
        if (!rec.empty()) {
          auto &ac = gdi.addAutoComplete(ii.id);
          ac.setAutoCompleteHandler(this);
          ac.setData(rec);
          ac.show();
          show = true;
        }
      }
      if (!show) {
        gdi.clearAutoComplete(ii.id);
      }
    }
  }
  else if (type == GUI_INPUT) {
    InputInfo &ii = *(InputInfo *)(&data);
    if (ii.id == "Text" && ii.text != lastShownExampleText) {
      showExample(gdi);
    }
    else if (ii.id == "ImgWidth" || ii.id == "ImgHeight") {
      if (gdi.isChecked("PreserveAspectRatio")) {
        auto sel = gdi.getSelectedItem("Image");
        if (sel.second && sel.first >= 0) {
          auto imgId = image.getIdFromEnumeration(sel.first);
          int h = image.getHeight(imgId);
          int w = image.getWidth(imgId);
          int ww, hh;
          if (ii.id == "ImgWidth") {
            ww = _wtoi(ii.text.c_str());
            hh = (ww * h + w/2) / w;
            gdi.setText("ImgHeight", hh);
          } else {
            hh = _wtoi(ii.text.c_str());
            ww = (hh * w + h / 2) / h;
            gdi.setText("ImgWidth", ww);
          }
        }
      }
    }
  }
  else if (type == GUI_BUTTON) {
    ButtonInfo bi = dynamic_cast<ButtonInfo &>(data);
    ButtonInfo &biSrc = dynamic_cast<ButtonInfo &>(data);

    if (bi.id == "Color") {
      wstring c = oe->getPropertyString("Colors", L"");
      int res = gdi.selectColor(c, bi.getExtraInt());
      if (res >= -1) {
        biSrc.setExtra(res);
        oe->setProperty("Colors", c);
      }
    }
    else if (bi.id == "NewImage") {
      vector<pair<wstring, wstring>> ext = { make_pair(L">Bilder", L"*.png") };
      wstring fn = gdi.browseForOpen(ext, L"png");
      if (!fn.empty()) {
        bool transparent = gdi.isChecked("TransparentWhite");

        uint64_t imgId = image.loadFromFile(fn, transparent ? Image::ImageMethod::WhiteTransparent : Image::ImageMethod::Default);
        int selIx = selectImage(gdi, imgId);
        previewImage(gdi, selIx);
        updateImageStatus(gdi, selIx);

      }
    }
    else if (bi.id == "TransparentWhite") {
      bool transparent = gdi.isChecked("TransparentWhite");
      int data = gdi.getSelectedItem("Image").first;

      if (data >= 0) {
        uint64_t imgId = image.getIdFromEnumeration(data);
        image.reloadImage(imgId, transparent ? Image::ImageMethod::WhiteTransparent : Image::ImageMethod::Default);
        gdi.refresh();
      }
    }
    else if ( bi.id.substr(0, 8) == "EditPost" ) {
      if (gdi.hasData("CurrentId")) {
        checkUnsaved(gdi);
      }
      int id = atoi(bi.id.substr(8).c_str());
      getPosFromId(id, groupIx, lineIx, ix);
      MetaListPost &mlp = currentList->getMLP(groupIx, lineIx, ix);
      if (mlp.getTypeRaw() == EPostType::lImage)
        editImage(gdi, mlp, id);
      else
        editListPost(gdi, mlp, id);
    }
    else if (bi.id == "AddPost" || bi.id == "AddImage") {
      checkUnsaved(gdi);
      gdi.restore("EditList", true);
      gdi.pushX();
      lineIx = bi.getExtraInt();
      groupIx = (lineIx / 100) - 1;
      int ixOutput = 0;
      MetaListPost &mlp = currentList->addNew(groupIx, lineIx % 100, ixOutput);
      if (bi.id == "AddImage")
        mlp.setType(EPostType::lImage);
      
      auto& post = dynamic_cast<ButtonInfo&>(gdi.getBaseInfo("AddPost", lineIx));

      int xp = post.xp;
      int yp = post.yp;
      ButtonInfo &nb = addButton(gdi, mlp, xp, yp, lineIx, ixOutput);
      int w, h;
      nb.getDimension(gdi, w, h);
      post.moveButton(gdi, xp + w, yp);
      int w2, h2;
      post.getDimension(gdi, w2, h2);
      dynamic_cast<ButtonInfo&>(gdi.getBaseInfo("AddImage", lineIx)).moveButton(gdi, xp + w + w2, yp);
      gdi.popX();
      gdi.setRestorePoint("EditList");
      makeDirty(gdi, MakeDirty, MakeDirty);
      gdi.sendCtrlMessage(nb.id);
    }
    else if ( bi.id.substr(0, 7) == "AddLine" ) {
      checkUnsaved(gdi);
      groupIx = atoi(bi.id.substr(7).c_str());
      int ixOutput = 0;
      currentList->addNew(groupIx, -1, ixOutput);

      gdi.restore("BeginListEdit", false);
      makeDirty(gdi, MakeDirty, MakeDirty);
      show(gdi);
    }
    else if (bi.id == "Remove") {
      DWORD id;
      gdi.getData("CurrentId", id);
      getPosFromId(id, groupIx, lineIx, ix);
      currentList->removeMLP(groupIx, lineIx, ix);
      gdi.restore("BeginListEdit", false);
      makeDirty(gdi, MakeDirty, MakeDirty);
      show(gdi);
    }
    else if (bi.id == "UseLeg") {
      gdi.setInputStatus("Leg", gdi.isChecked(bi.id));
    }
    else if (bi.id == "UseForSplit") {
      statusSplitPrint(gdi, gdi.isChecked(bi.id));
    }
    else if (bi.id == "Cancel") {
      gdi.restore("EditList");
      gdi.enableInput("EditList");
      gdi.enableInput("SplitPrint");
    }
    else if (bi.id == "CancelNew") {
      gdi.clearPage(false);
      currentList = 0;
      show(gdi);
    }
    else if (bi.id == "Apply" || bi.id == "MoveLeft" || bi.id == "MoveRight") {
      bool image = gdi.hasData("IsEditingImage");
      DWORD id;
      gdi.getData("CurrentId", id);
      getPosFromId(id, groupIx, lineIx, ix);

      if (bi.id == "MoveLeft") {
        currentList->moveOnRow(groupIx, lineIx, ix, -1);
        id--;
      }
      else if (bi.id == "MoveRight") {
        currentList->moveOnRow(groupIx, lineIx, ix, 1);
        id++;
      }
      gdi.setData("CurrentId", id);
      MetaListPost &mlp = currentList->getMLP(groupIx, lineIx, ix);

      bool force = checkUnsaved(gdi);//saveListPost(gdi, mlp);

      if (!gdi.hasData("NoRedraw") || force) {
        gdi.restore("BeginListEdit", false);
        show(gdi);
      }

      if (bi.id != "Apply") {
        if (image)
          editImage(gdi, mlp, bi.getExtraInt());
        else
          editListPost(gdi, mlp, bi.getExtraInt());
      }
    }
    else if (bi.id == "ApplyListProp") {
      wstring name = gdi.getText("Name");

      if (name.empty())
        throw meosException("Namnet kan inte vara tomt");

      MetaList &list = *currentList;
      list.setListName(name);
      ListBoxInfo lbi;

      if (gdi.getSelectedItem("SortOrder", lbi))
        list.setSortOrder(SortOrder(lbi.data));

      if (gdi.getSelectedItem("BaseType", lbi))
        list.setListType(oListInfo::EBaseType(lbi.data));

      if (gdi.getSelectedItem("ResultType", lbi))
        list.setResultModule(*oe, lbi.data);

      if (gdi.getSelectedItem("SubType", lbi))
        list.setSubListType(oListInfo::EBaseType(lbi.data));

      vector< pair<wstring, bool> > filtersIn;
      vector< bool > filtersOut;
      list.getFilters(filtersIn);
      for (size_t k = 0; k < filtersIn.size(); k++)
        filtersOut.push_back(gdi.isChecked("filter" + itos(k)));

      list.setFilters(filtersOut);

      vector< pair<wstring, bool> > subFiltersIn;
      vector< bool > subFiltersOut;
      list.getSubFilters(subFiltersIn);
      for (size_t k = 0; k < subFiltersIn.size(); k++)
        subFiltersOut.push_back(gdi.isChecked("subfilter" + itos(k)));

      list.setSubFilters(subFiltersOut);

      for (int k = 0; k < 4; k++) {
        list.setFontFace(k, gdi.getText("Font" + itos(k)),
                            gdi.getTextNo("FontFactor" + itos(k)));

        int f = gdi.getTextNo("ExtraSpace" + itos(k));
        list.setExtraSpace(k, f);
      }

      list.setSupportFromTo(gdi.isChecked("SupportFrom"), gdi.isChecked("SupportTo"));
      list.setSupportLegSelection(gdi.isChecked("SupportLegSelection"));

      makeDirty(gdi, MakeDirty, MakeDirty);

      if (!gdi.hasData("NoRedraw")) {
        gdi.clearPage(false);
        show(gdi);
      }
    }
    else if (bi.id == "ApplySplitList") {
      MetaList& list = *currentList;
      if (!gdi.isChecked("UseForSplit"))
        list.setSplitPrintInfo(nullptr);
      else {
        auto spInfo = make_shared<SplitPrintListInfo>();
        spInfo->includeSplitTimes = gdi.isChecked("Split");
        spInfo->withSpeed = gdi.isChecked("Speed");
        spInfo->withResult = gdi.isChecked("Result");
        spInfo->withAnalysis = gdi.isChecked("Analysis");
        list.setSplitPrintInfo(spInfo);
      }

      makeDirty(gdi, MakeDirty, MakeDirty);

      if (!gdi.hasData("NoRedraw")) {
        gdi.clearPage(false);
        show(gdi);
      }
    }
    else if (bi.id == "EditList") {
      editListProp(gdi, false);
    }
    else if (bi.id == "SplitPrint") {
      splitPrintList(gdi);
    }
    else if (bi.id == "NewList") {
      if (!checkSave(gdi))
        return 0;
      gdi.clearPage(false);
      gdi.setData("ListEditorClz", this);
      gdi.addString("", boldLarge, "Ny lista");

      setCurrentList(new MetaList());
      currentIndex = -1;
      lastSaved = NotSaved;
      makeDirty(gdi,  ClearDirty, ClearDirty);

      editListProp(gdi, true);
    }
    else if (bi.id == "MakeNewList") {
      /*
      currentList->setListName(lang.tl("Ny lista"));
      currentIndex = -1;

      lastSaved = NotSaved;
      makeDirty(gdi,  ClearDirty, ClearDirty);

      gdi.clearPage(false);
      show(gdi);*/
    }
    else if (bi.id == "SaveFile" || bi.id == "SaveFileCopy") {
      if (!currentList)
        return 0;

      bool copy = bi.id == "SaveFileCopy";

      wstring fileName = copy ? L"" : savedFileName;

      if (fileName.empty()) {
        int ix = 0;
        vector< pair<wstring, wstring> > ext;
        ext.push_back(make_pair(L"List definition", L"*.meoslist"));
        fileName = gdi.browseForSave(ext, L"meoslist", ix);
        if (fileName.empty())
          return 0;
      }

      currentList->save(fileName, oe);

      lastSaved = SavedFile;
      makeDirty(gdi, NoTouch, ClearDirty);
      savedFileName = fileName;
      return 1;
    }
    else if (bi.id == "OpenFile") {
      if (!checkSave(gdi))
        return 0;

      vector< pair<wstring, wstring> > ext;
      ext.push_back(make_pair(L"List definition", L"*.meoslist;*.xml"));
      wstring fileName = gdi.browseForOpen(ext, L"meoslist");
      if (fileName.empty())
        return 0;

      MetaList *tmp = new MetaList();
      try {
        tmp->setListName(lang.tl(L"Ny lista"));
        tmp->load(fileName);
      }
      catch(...) {
        delete tmp;
        throw;
      }

      setCurrentList(tmp);
      currentIndex = -1;
      gdi.clearPage(false);
      lastSaved = SavedFile;

      savedFileName = fileName;
      oe->loadGeneralResults(true, true);
      makeDirty(gdi, ClearDirty, ClearDirty);
      show(gdi);
    }
    else if (bi.id == "OpenInside") {
      if (!checkSave(gdi))
        return 0;

      savedFileName.clear();
      gdi.clearPage(true);
      gdi.setOnClearCb(editListCB);
      gdi.setData("ListEditorClz", this);

      gdi.pushX();
      vector< pair<wstring, size_t> > lists;
      oe->getListContainer().getLists(lists, true, false, false, false);
      
      gdi.fillRight();
      gdi.addSelection("OpenList", 250, 400, editListCB, L"Välj lista:");
      gdi.addItem("OpenList", lists);
      gdi.selectFirstItem("OpenList");


      gdi.dropLine();
      gdi.addButton("DoOpen", "Öppna", editListCB);
      gdi.addButton("DoOpenCopy", "Open a Copy", editListCB);
      enableOpen(gdi);

      gdi.addButton("CancelReload", "Avbryt", editListCB).setCancel();
      gdi.dropLine(4);
      gdi.popX();
    }
    else if (bi.id == "DoOpen" || bi.id == "DoOpenCopy" ) {
      ListBoxInfo lbi;
      if (gdi.getSelectedItem("OpenList", lbi)) {
        load(oe->getListContainer(), lbi.data);
      }

      if (bi.id == "DoOpenCopy") {
        currentIndex = -1;
        currentList->clearTag();
        lastSaved = NotSaved;
        makeDirty(gdi, MakeDirty, MakeDirty);
      }
      else {
        lastSaved = SavedInside;
        makeDirty(gdi, ClearDirty, ClearDirty);
      }
      gdi.clearPage(false);
      show(gdi);
    }
    else if (bi.id == "CancelReload") {
      gdi.clearPage(false);
      show(gdi);
    }
    else if (bi.id == "SaveInside") {
      if (currentList == 0)
        return 0;
      savedFileName.clear();
      oe->synchronize(false);

      set<uint64_t> imgUsed;
      currentList->getUsedImages(imgUsed);
      for (uint64_t id : imgUsed)
        oe->saveImage(id);

      if (currentIndex != -1) {
        oe->getListContainer().saveList(currentIndex, *currentList);
      }
      else {
        oe->getListContainer().addExternal(*currentList);
        currentIndex = oe->getListContainer().getNumLists() - 1;
        oe->getListContainer().saveList(currentIndex, *currentList);
      }

      oe->synchronize(true);
      lastSaved = SavedInside;
      makeDirty(gdi, ClearDirty, ClearDirty);
    }
    else if (bi.id == "RemoveInside") {
      if (currentIndex != -1) {
        oe->getListContainer().removeList(currentIndex);
        currentIndex = -1;
        savedFileName.clear();
        if (lastSaved == SavedInside)
          lastSaved = NotSaved;

        gdi.alert("Listan togs bort från tävlingen.");
        makeDirty(gdi, MakeDirty, NoTouch);
        gdi.setInputStatus("RemoveInside", false);
      }
    }
    else if (bi.id == "Close") {
      if (!checkSave(gdi))
        return 0;

      setCurrentList(0);
      makeDirty(gdi, ClearDirty, ClearDirty);
      currentIndex = -1;
      savedFileName.clear();

      if (origin) {
        auto oc = origin;
        origin = nullptr;
        oc->loadPage(gdi);
      }
      return 0;
    }
  }
  else if (type == GUI_LISTBOX) {
    ListBoxInfo &lbi = dynamic_cast<ListBoxInfo &>(data);
    if (lbi.id == "RelPrevious") {
      updateAlign(gdi, lbi.data);
    }
    else if (lbi.id == "AlignType") {
      gdi.setInputStatus("AlignText", lbi.data == lString);
      if (lbi.data == lString) {
        int ix = lbi.text.find_first_of(L":");
        if (ix != lbi.text.npos)
          gdi.setText("AlignText", lbi.text.substr(ix+1));
      }
      else
        gdi.setText("AlignText", L"");
    }
    else if (lbi.id == "Image") {
      previewImage(gdi, lbi.data);
      updateImageStatus(gdi, lbi.data);
    }
    else if (lbi.id == "Type") {
      updateType(lbi.data, gdi);
    }
    else if (lbi.id == "SubType") {
      oListInfo::EBaseType subType = oListInfo::EBaseType(lbi.data);
      vector< pair<wstring, bool> > subfilters;
      currentList->getSubFilters(subfilters);
      for (size_t k = 0; k < subfilters.size(); k++) {
        gdi.setInputStatus("subfilter" + itos(k), subType != oListInfo::EBaseTypeNone);
      }
    }
    else if (lbi.id == "ResultType") {
      vector< pair<wstring, size_t> > types;
      int currentType = 0;
      currentList->getSortOrder(lbi.data != 0, types, currentType);
      if (lbi.data == 0) {
        ListBoxInfo mlbi;
        gdi.getSelectedItem("SortOrder", mlbi);
        currentType = mlbi.data;
      }
      gdi.addItem("SortOrder", types);
      gdi.selectItemByData("SortOrder", currentType);
    }
    else if (lbi.id == "OpenList") {
      enableOpen(gdi);
    }
    else if (lbi.id == "Runner") {
      currentRunnerId = lbi.getDataInt();
      gdi.restore("EditList", false);
      renderListPreview(gdi);
      gdi.refresh();
    }
  }
  else if (type==GUI_CLEAR) {
    return checkSave(gdi);
  }
  return 0;
}

bool ListEditor::saveListPost(gdioutput &gdi, MetaListPost &mlp) {
  ListBoxInfo lbi;
  bool force = false;
  gdi.getSelectedItem("Type", lbi);

  EPostType ptype = EPostType(lbi.data);

  wstring str = gdi.getText("Text");
  if (ptype != lString) {
    if (!str.empty() && str.find_first_of('X') == string::npos && str[0] != '@') {
      throw meosException("Texten ska innehålla tecknet X, som byts ut mot tävlingsspecifik data");
    }
  }

  wstring t1 = mlp.getType();
  EPostType newType = EPostType(lbi.data);
  mlp.setType(newType);
  if (t1 != mlp.getType())
    force = true;
  mlp.setText(str);

  gdi.getSelectedItem("AlignType", lbi);
  mlp.align(EPostType(lbi.data));

  mlp.limitBlockWidth(gdi.isChecked("LimitBlockWidth"));
  mlp.alignText(gdi.getText("AlignText"));

  auto relPrev = gdi.getSelectedItem("RelPrevious");
  mlp.packWithPrevious(relPrev.first == 2);
  mlp.mergePrevious(relPrev.first == 1);

  gdi.getSelectedItem("TextAdjust", lbi);
  mlp.setTextAdjust(lbi.data);

  mlp.setColor(GDICOLOR(gdi.getExtraInt("Color")));

  int leg = readLeg(gdi, newType, true);
  mlp.setLeg(leg);

  if (gdi.hasWidget("UseResultModule") && gdi.isChecked("UseResultModule"))
    mlp.setResultModule(currentList->getResultModule());
  else
    mlp.setResultModule("");

  mlp.setBlock(gdi.getTextNo("BlockSize"));
  mlp.indent(gdi.getTextNo("MinIndent"));

  gdi.getSelectedItem("Fonts", lbi);
  mlp.setFont(gdiFonts(lbi.data));
  makeDirty(gdi, MakeDirty, MakeDirty);

  return force;
}

bool ListEditor::saveImage(gdioutput& gdi, MetaListPost& mlp) {
  ListBoxInfo lbi;
  int selIx = gdi.getSelectedItem("Image").first;

  if (selIx >= 0) {
    auto imgId = image.getIdFromEnumeration(selIx);
    mlp.setText(itow(imgId));
    mlp.setImageDimension(gdi.getTextNo("ImgWidth"), gdi.getTextNo("ImgHeight"));
    mlp.setImageOffset(gdi.getTextNo("ImgOffsetX"), gdi.getTextNo("ImgOffsetY"));
    mlp.setImageStyle(gdi.isChecked("TransparentWhite") ? 1 : 0);
    mlp.imageUnderText(gdi.isChecked("ImageUnderText"));
  }
  else {
    mlp.setText(L"");
  }
  makeDirty(gdi, MakeDirty, MakeDirty);

  return false;
}

int ListEditor::readLeg(gdioutput &gdi, EPostType newType, bool checkError) const {
  if (MetaList::isResultModuleOutput(newType)) {
    int leg = gdi.getTextNo("Leg");
    if (leg < 0 || leg > 1000) {
      if (checkError)
        throw meosException("X är inget giltigt index#" + itos(leg));
      else
        leg = -1;
    }
    return leg;
  }
  else if (MetaList::isAllStageType(newType)) {
    int leg = gdi.getSelectedItem("LegSel").first;
    if (leg >= 1)
      return leg - 1;
    else
      return -1;
  }
  else if (MetaList::isAllLegType(newType)) {
    int leg = gdi.getSelectedItem("LegSel").first;
    if (leg >= 0)
      return leg - 1; // -1 -> automatic
    else 
      return -2; // All legs
  }
  else {
    if (gdi.isChecked("UseLeg")) {
      int leg = gdi.getTextNo("Leg");
      if (leg < 1 || leg > 1000) {
        if (checkError)
          throw meosException("X är inget giltigt sträcknummer#" + itos(leg));
        else
          leg = -1;
      }
      return leg - 1;
    }
    else
      return -1;
  }
}

void ListEditor::updateType(int iType, gdioutput & gdi) {
  EPostType type = EPostType(iType);
  int leg = -1;
  if (gdi.hasWidget("leg"))
    leg = gdi.getTextNo("Leg");

  gdi.restore("Example", false);
  
  if (legStageTypeIndex(gdi, type, leg)) {
    gdi.setRestorePoint("Example");
  }
  if (MetaList::isResultModuleOutput(type)) {
    if (gdi.hasWidget("UseResultModule")) {
      gdi.check("UseResultModule", true);
      gdi.disableInput("UseResultModule");
    }
    gdi.enableInput("Leg");
    if (gdi.getText("Leg").empty())
      gdi.setText("Leg", L"0");
  }
  else if (MetaList::isAllStageType(type) || MetaList::isAllLegType(type)) {

  }
  else {
    if (leg == 0) {
      gdi.setText("Leg", L"");
      gdi.enableInput("UseLeg");
      gdi.enableInput("UseResultModule", true);
      gdi.check("UseLeg", false);
      gdi.disableInput("Leg");
    }
  }
  
  showExample(gdi, type);
}

bool ListEditor::checkUnsaved(gdioutput& gdi) {
  if (gdi.hasData("IsEditing")) {
    DWORD id;
    gdi.getData("CurrentId", id);
    int groupIx, lineIx, ix;
    getPosFromId(id, groupIx, lineIx, ix);
    MetaListPost& mlp = currentList->getMLP(groupIx, lineIx, ix);
    return saveListPost(gdi, mlp);
  }
  else if (gdi.hasData("IsEditingImage")) {
    DWORD id;
    gdi.getData("CurrentId", id);
    int groupIx, lineIx, ix;
    getPosFromId(id, groupIx, lineIx, ix);
    MetaListPost& mlp = currentList->getMLP(groupIx, lineIx, ix);
    return saveImage(gdi, mlp);
  }
  else if (gdi.hasData("IsEditingList")) {
    if (gdi.isInputChanged("")) {
      gdi.setData("NoRedraw", 1);
      gdi.sendCtrlMessage("ApplyListProp");
    }
  }
  else if (gdi.hasData("IsSplitListEdit")) {
    if (gdi.isInputChanged("")) {
      gdi.setData("NoRedraw", 1);
      gdi.sendCtrlMessage("ApplySplitList");
    }
  }
  return false;
}

void ListEditor::updateAlign(gdioutput &gdi, int val) {
  gdi.setInputStatus("AlignType", val != 1);
  gdi.setInputStatus("AlignText", val != 1);

  gdi.setInputStatus("BlockSize", val != 1);
  gdi.setInputStatus("LimitBlockWidth", val != 1);

  gdi.setInputStatus("TextAdjust", val != 1);
  gdi.setInputStatus("MinIndent", val != 1);

  gdi.setInputStatus("Color", val != 1);
  gdi.setInputStatus("Fonts", val != 1);
}

void ListEditor::editListPost(gdioutput &gdi, const MetaListPost &mlp, int id) {
  int groupIx, lineIx, ix;
  getPosFromId(id, groupIx, lineIx, ix);

  int x1, y1, boxY;
  editDlgStart(gdi, id, "Listpost", x1, y1, boxY);

  int maxX = gdi.getCX();

  gdi.popX();
  const bool hasResultModule = currentList && !currentList->getResultModule().empty();

  vector< pair<wstring, size_t> > types;
  int currentType;
  mlp.getTypes(types, currentType);
  EPostType storedType = EPostType(currentType);

  if (!hasResultModule) {
    for (size_t k = 0; k < types.size(); k++) {
      if ( (storedType != lResultModuleNumber && types[k].second == lResultModuleNumber) ||
           (storedType != lResultModuleTime && types[k].second == lResultModuleTime) ||
           (storedType != lResultModuleNumberTeam && types[k].second == lResultModuleNumberTeam) ||
           (storedType != lResultModuleTimeTeam && types[k].second == lResultModuleTimeTeam)) {
        swap(types[k], types.back());
        types.pop_back();
        k--;
      }     
    }
  }

  sort(types.begin(), types.end());
  gdi.pushX();
  gdi.fillRight();
  gdi.addString("", 0, L"Typ:");
  gdi.fillDown();
  gdi.registerEvent("SearchRunner", editListCB).setKeyCommand(KC_FIND);
  gdi.registerEvent("SearchRunnerBack", editListCB).setKeyCommand(KC_FINDBACK);

  gdi.addInput("SearchText", getSearchString(), 26, editListCB, L"",
               L"Sök symbol.").isEdit(false)
              .setBgColor(colorLightCyan).ignore(true);
  
  gdi.dropLine(-0.1);
  gdi.popX();
  gdi.fillRight();
  gdi.addSelection("Type", 290, 500, editListCB);
  gdi.dropLine(-1);
  gdi.addItem("Type", types);
  gdi.selectItemByData("Type", currentType);
  gdi.addInput("Text", mlp.getText(), 16, editListCB,
               L"Egen text:", L"Använd symbolen X där MeOS ska fylla i typens data.");
  gdi.setInputFocus("Text", true);
  ((InputInfo *)gdi.setText("SearchText", getSearchString()))->setFgColor(colorGreyBlue);
  int boxX = gdi.getCX();
  gdi.popX();
  gdi.fillRight();
  gdi.dropLine(3);
  
  if (hasResultModule) {
    gdi.addCheckbox("UseResultModule", "Data from result module (X)#" + currentList->getResultModule(), 0, !mlp.getResultModule().empty());
    gdi.dropLine(1.5);
    gdi.popX();
  }

  xpUseLeg = gdi.getCX();
  ypUseLeg = gdi.getCY();

  int leg = mlp.getLeg();

  legStageTypeIndex(gdi, storedType, leg);
  if (MetaList::isResultModuleOutput(storedType)) {
    if (gdi.hasWidget("UseResultModule")) {
      gdi.check("UseResultModule", true);
      gdi.disableInput("UseResultModule");
    }
  }

  gdi.popX();
  gdi.dropLine(2.5);
  currentList->getAlignTypes(mlp, types, currentType);
  sort(types.begin(), types.end());
  gdi.addSelection("AlignType", 290, 500, editListCB, L"Justera mot:");
  gdi.addItem("AlignType", types);
  gdi.selectItemByData("AlignType", currentType);

  gdi.addInput("AlignText", mlp.getAlignText(), 16, 0, L"Text:");
  if (currentType != lString)
    gdi.disableInput("AlignText");
  gdi.popX();
  gdi.dropLine(3);
  gdi.fillRight();
  gdi.addString("", 0, "Minsta blockbredd:");
  gdi.dropLine(-0.2);
  gdi.addInput("BlockSize", itow(mlp.getBlockWidth()), 5, 0, L"", L"Blockbredd");
  gdi.dropLine(0.2);
  gdi.addCheckbox("LimitBlockWidth", "Begränsa bredd (klipp text)", 0, mlp.getLimitBlockWidth());
  gdi.dropLine(1.9);
  gdi.popX();
  gdi.fillRight();
  gdi.dropLine(2);
  
  int maxY = gdi.getCY();
  gdi.popX();
  gdi.fillDown();
  int innerBoxUpperCX = boxX + gdi.scaleLength(18);
  int innerBoxUpperCY = boxY;

  gdi.setCX(boxX + gdi.scaleLength(24));
  gdi.setCY(boxY + gdi.scaleLength(6));
  gdi.pushX();
  gdi.addString("", fontMediumPlus, "Formateringsregler");
  gdi.dropLine(0.5);
  gdi.fillRight();
  int val = 0;
  if (ix>0) {
    gdi.popX();
    gdi.addSelection("RelPrevious", 100, 100, editListCB, L"Relation till föregående:");
    gdi.addItem("RelPrevious", lang.tl("Ingen"), 0);
    gdi.addItem("RelPrevious", lang.tl("Slå ihop text"), 1);
    gdi.addItem("RelPrevious", lang.tl("Håll ihop med"), 2);
    gdi.autoGrow("RelPrevious");

    val = mlp.isMergePrevious() ? 1 : (mlp.getPackWithPrevious() ? 2 : 0);
    gdi.selectItemByData("RelPrevious", val);
  }
  

  gdi.addInput("MinIndent", itow(mlp.getMinimalIndent()), 7, 0, L"Justering i sidled:");
  maxX = max(maxX, gdi.getCX());
  gdi.popX();
  gdi.dropLine(3);
  vector< pair<wstring, size_t> > fonts;
  int currentFont;
  mlp.getFonts(fonts, currentFont);

  gdi.addSelection("Fonts", 200, 500, 0, L"Format:");
  gdi.addItem("Fonts", fonts);
  gdi.selectItemByData("Fonts", currentFont);
  maxX = max(maxX, gdi.getCX());

  gdi.popX();
  gdi.dropLine(3);

  gdi.addSelection("TextAdjust", 130, 100, 0, L"Textjustering:");
  gdi.addItem("TextAdjust", lang.tl("Vänster"), 0);
  gdi.addItem("TextAdjust", lang.tl("Höger"), textRight);
  gdi.addItem("TextAdjust", lang.tl("Centrera"), textCenter);
  gdi.selectItemByData("TextAdjust", mlp.getTextAdjustNum());

  gdi.dropLine();
  gdi.addButton("Color", "Färg...", editListCB).setExtra(mlp.getColorValue());

  maxX = max(maxX, gdi.getCX());
  int innerBoxLowerCX = maxX + gdi.scaleLength(6);
  maxX += gdi.scaleLength(12);

  gdi.setCX(boxX - gdi.scaleLength(6));
  gdi.dropLine(3);
  int innerBoxLowerCY = gdi.getCY();

  gdi.dropLine();

  gdi.setData("CurrentId", id);
  gdi.addButton("Remove", "Radera", editListCB, "Ta bort listposten");
  gdi.addButton("Cancel", "Avbryt", editListCB).setCancel();

  gdi.updatePos(gdi.getCX(), gdi.getCY(), gdi.scaleLength(20), 0);
  gdi.addButton("Apply", "OK", editListCB).setDefault();

  gdi.dropLine(3);
  maxY = max(maxY, gdi.getCY());
  maxX = max(gdi.getCX(), maxX);

  RECT rc;
 
  rc.top = y1;
  rc.left = x1;
  rc.right = maxX + gdi.scaleLength(6);
  rc.bottom = maxY + gdi.scaleLength(6) + gdi.getLineHeight()*4;

  gdi.addRectangle(rc, colorLightBlue, true, false);

  rc.top = innerBoxUpperCY;
  rc.left = innerBoxUpperCX;
  rc.right = innerBoxLowerCX;
  rc.bottom = innerBoxLowerCY;

  gdi.addRectangle(rc, colorLightYellow, true, false);

  gdi.setData("IsEditing", 1);
  gdi.setCX(x1);
  gdi.setCY(maxY);

  gdi.scrollToBottom();

  showExample(gdi, mlp);

  updateAlign(gdi, val);
  gdi.refresh();
}

void ListEditor::editDlgStart(gdioutput& gdi, int id, const char *title, int &x1, int &y1, int &boxY) {
  int groupIx, lineIx, ix;
  getPosFromId(id, groupIx, lineIx, ix);

  checkUnsaved(gdi);
  gdi.restore("EditList", false);
  gdi.dropLine();
  gdi.enableInput("EditList");
  gdi.enableInput("SplitPrint");
  
  x1 = gdi.getCX();
  y1 = gdi.getCY();
  int margin = gdi.scaleLength(10);
  gdi.setCX(x1 + margin);

  gdi.dropLine();
  gdi.pushX();
  gdi.fillRight();
  gdi.addString("", boldLarge, title).setColor(colorDarkGrey);
  gdi.setCX(gdi.getCX() + gdi.scaleLength(20));

  gdi.addButton("MoveLeft", "<< Flytta vänster", editListCB).setExtra(id - 1);
  if (ix == 0)
    gdi.setInputStatus("MoveLeft", false);

  gdi.addButton("MoveRight", "Flytta höger >>", editListCB).setExtra(id + 1);
  if (ix + 1 == currentList->getNumPostsOnLine(groupIx, lineIx))
    gdi.setInputStatus("MoveRight", false);

  gdi.dropLine(1);
  boxY = gdi.getCY();
  gdi.dropLine(2);
}

void ListEditor::editImage(gdioutput& gdi, const MetaListPost& mlp, int id) {
  int groupIx, lineIx, ix;
  getPosFromId(id, groupIx, lineIx, ix);

  uint64_t imgId = mlp.getImageId();

  int x1, y1, boxY;
  editDlgStart(gdi, id, "Bild", x1, y1, boxY);

  int maxX = gdi.getCX();
  gdi.popX();
  gdi.fillRight();
  gdi.dropLine(1);
  
  gdi.addSelection("Image", 200, 200, editListCB, L"Välj bild:", L"Välj bland befintliga bilder");

  int selIx = selectImage(gdi, imgId);

  gdi.dropLine(1);
  gdi.addButton("NewImage", "Ny bild...", editListCB);

  int maxY = 0;
  maxX = max(maxX, gdi.getCX());
  int innerBoxLowerCX = maxX + gdi.scaleLength(6);
  maxX += gdi.scaleLength(12);

  gdi.popX();
  gdi.dropLine(3);

  wstring wh, ww, xoff, yoff;
  bool keepRatio = true;
  if (imgId) {
    int h = mlp.getImageHeight();
    int w = mlp.getImageWidth();
    wh = itow(h);
    ww = itow(w);

    int hImg = image.getHeight(imgId);
    int wImg = image.getWidth(imgId);

    int hComputed = (w * hImg + wImg / 2) / wImg;
    int wComputed = (h * wImg + hImg / 2) / hImg;

    keepRatio = std::abs(h - hComputed) <= 1 || std::abs(w - wComputed) <= 1;

    xoff = itow(mlp.getImageOffsetX());
    yoff = itow(mlp.getImageOffsetY());
  }

  gdi.addInput("ImgWidth", ww, 5, editListCB, L"Bredd:");
  gdi.addInput("ImgHeight", wh, 5, editListCB, L"Höjd:");
  gdi.dropLine();
  gdi.addCheckbox("PreserveAspectRatio", "Bevara höjd/bredd-relationen", nullptr, keepRatio);
  gdi.popX();
  gdi.dropLine(2);

  gdi.dropLine(1);
  gdi.addString("", 0, "Förskjutning:");
  gdi.dropLine(-1);

  gdi.addInput("ImgOffsetX", xoff, 5, editListCB, L"Horizontell:");
  gdi.addInput("ImgOffsetY", yoff, 5, editListCB, L"Vertikal:");

  gdi.popX();
  gdi.dropLine(3);

  gdi.addCheckbox("TransparentWhite", "Tolka vitt som genomskinligt", editListCB, mlp.getImageStyle() == 1);
  gdi.addCheckbox("ImageUnderText", "Bild under text", editListCB, mlp.imageUnderText());

  bool hasImg = imgId != 0;
  gdi.setInputStatus("ImgWidth", hasImg);
  gdi.setInputStatus("ImgHeight", hasImg);
  gdi.setInputStatus("ImgOffsetX", hasImg);
  gdi.setInputStatus("ImgOffsetY", hasImg);

  gdi.setInputStatus("PreserveAspectRatio", hasImg);

  gdi.dropLine(3);
  gdi.setData("CurrentId", id);
  gdi.addButton("Remove", "Radera", editListCB, "Ta bort listposten");
  gdi.addButton("Cancel", "Avbryt", editListCB).setCancel();

  gdi.updatePos(gdi.getCX(), gdi.getCY(), gdi.scaleLength(20), 0);
  gdi.addButton("Apply", "OK", editListCB).setDefault();

  gdi.dropLine(1);
  maxY = max(maxY, gdi.getCY());
  maxX = max(gdi.getCX(), maxX);

  RECT rc;

  rc.top = y1;
  rc.left = x1;
  rc.right = maxX + gdi.scaleLength(6);
  rc.bottom = maxY + gdi.scaleLength(6) + gdi.getLineHeight();

  gdi.addRectangle(rc, colorLightBlue, true, false);

  gdi.setData("IsEditingImage", 1);
  
  gdi.scrollToBottom();
  gdi.setCX(rc.right + gdi.scaleLength(10));
  gdi.setCY(boxY);

  if (imgId != 0) {
    previewImage(gdi, selIx);
  }
  else
    gdi.refresh();
}

int ListEditor::selectImage(gdioutput &gdi, uint64_t imgId) {
  vector<pair<wstring, size_t>> img;
  image.enumerateImages(img);
  img.emplace(img.begin(), lang.tl("Ingen[bild]"), -2);
  gdi.addItem("Image", img);

  int ix = image.getEnumerationIxFromId(imgId);
  if (ix >= 0)
    gdi.selectItemByData("Image", ix);
  else
    gdi.selectFirstItem("Image");

  return ix;
}

void ListEditor::showExample(gdioutput &gdi, EPostType type) {
  if (type == EPostType::lLastItem) {
    type = EPostType(gdi.getSelectedItem("Type").first);
  }
  
  gdi.restore("Example", false);

  MetaListPost mlp(type);
  // Has not effect
  //mlp.setLeg(readLeg(gdi, type, false));
  mlp.setText(gdi.getText("Text", false));
  showExample(gdi, mlp);
  gdi.refreshFast();
}

void ListEditor::showExample(gdioutput &gdi, const MetaListPost &mlp) {
  int x1 = gdi.getCX();
  int margin = gdi.scaleLength(10);

  RECT rrInner;
  rrInner.left = x1 + margin;
  rrInner.top = gdi.getCY();
  bool hasSymbol, hasStringMap = false;
  lastShownExampleText = mlp.getText();
  GDICOLOR color = GDICOLOR::colorLightGreen;
  wstring text = MetaList::encode(mlp.getTypeRaw(), lastShownExampleText, hasSymbol);

  if (mlp.getTypeRaw() == lResultModuleNumber || mlp.getTypeRaw() == lResultModuleNumberTeam) {
    if (text.length() > 0 && text[0]=='@') {
      hasStringMap = true;
      text = text.substr(1);
    }
  }

  if (!hasSymbol && !hasStringMap) {
    text = lang.tl("Fel: Använd X i texten där värdet (Y) ska sättas in.#X#%s");
    color = GDICOLOR::colorLightRed;
  }

  gdi.setRestorePoint("Example");
  gdi.fillDown();
  gdi.setCX(x1 + margin * 2);
  gdi.dropLine(0.5);
  gdi.addString("", 0, "Exempel:");
  gdi.fillRight();
  int maxX = gdi.getWidth();
  gdi.dropLine(0.5);
  
  vector<pRunner> rr;
  oe->getRunners(0, 0, rr, false);
  set<wstring> used;
  for (size_t i = 0; i < rr.size(); i++) {
    int ix = (997 * i) % rr.size();
    wstring s;
    if (!hasStringMap)
      s = oe->formatListString(mlp.getTypeRaw(), rr[i]);
    else 
      MetaList::fromResultModuleNumber(text, i, s);
    
    if (used.insert(s).second) {
      int xb = gdi.getCX();
      if (!text.empty() && !hasStringMap) {
        wchar_t st[300];
        swprintf_s(st, text.c_str(), s.c_str());
        s = st;
      }
      
      gdi.addStringUT(italicText, s + L"  ");
      int xa = gdi.getCX();
      int delta = xa - xb;

      int dist = maxX - xa;
      if (dist < delta * 3 || used.size() == 5)
        break; // Enough examples
    }
  }
  gdi.dropLine(1.5);
  rrInner.right = max(gdi.getCX(), gdi.scaleLength(120)) + margin;
  rrInner.bottom = gdi.getCY();
  gdi.fillDown();
  gdi.popX();

  gdi.addRectangle(rrInner, color, true);
}

void ListEditor::previewImage(gdioutput& gdi, int data) const {
  gdi.restoreNoUpdate("image_preview");
  gdi.setRestorePoint("image_preview");
  if (data >= 0) {
    auto imgId = image.getIdFromEnumeration(data);
    oe->loadImage(imgId);
    bool transparent = gdi.isChecked("TransparentWhite");
    image.reloadImage(imgId, transparent ? Image::ImageMethod::WhiteTransparent : Image::ImageMethod::Default);
    gdi.addImage("", gdi.getCY(), gdi.getCX(), 0, itow(imgId));
  }
  gdi.refreshFast();
}

void ListEditor::updateImageStatus(gdioutput& gdi, int data) {
  bool hasImg = int(data) >= 0;
  gdi.setInputStatus("ImgWidth", hasImg);
  gdi.setInputStatus("ImgHeight", hasImg);
  gdi.setInputStatus("PreserveAspectRatio", hasImg);
  gdi.setInputStatus("ImgOffsetX", hasImg);
  gdi.setInputStatus("ImgOffsetY", hasImg);

  if (hasImg) {
    auto imgId = image.getIdFromEnumeration(data);
    int h = image.getHeight(imgId);
    int w = image.getWidth(imgId);
    gdi.setText("ImgWidth", w);
    gdi.setText("ImgHeight", h);
    
    if (gdi.getTextNo("ImgOffsetX") == 0 && gdi.getTextNo("ImgOffsetY") == 0) {
      // Change from blank to "0"
      gdi.setText("ImgOffsetX", 0);
      gdi.setText("ImgOffsetY", 0);
    }
  }
  else {
    gdi.setText("ImgWidth", L"");
    gdi.setText("ImgHeight", L"");
  }
}

const wchar_t *ListEditor::getIndexDescription(EPostType type) {
  if (type == lResultModuleTime || type == lResultModuleTimeTeam)
    return L"Index in X[index]#OutputTimes";
  else if (type == lResultModuleNumber || type == lResultModuleNumberTeam)
    return L"Index in X[index]#OutputNumbers";
  else if (MetaList::isAllStageType(type))
    return L"Applicera för specifik etapp:";
  else
    return L"Applicera för specifik sträcka:";
}

bool ListEditor::legStageTypeIndex(gdioutput &gdi, EPostType type, int leg) {
  int dx = gdi.scaleLength(250);
  int dy = -gdi.getLineHeight() / 5;
  
  if (MetaList::isResultModuleOutput(type)) {
    if (gdi.hasWidget("UseLeg"))
      gdi.removeWidget("UseLeg");

    if (gdi.hasWidget("LegSel"))
      gdi.removeWidget("LegSel");

    if (gdi.hasWidget("TUseLeg"))
      gdi.setTextTranslate("TUseLeg", getIndexDescription(type), true);
    else
      gdi.addString("TUseLeg", ypUseLeg, xpUseLeg, 0, getIndexDescription(type));

    wstring legW = leg >= 0 ? itow(leg) : L"0";
    if (!gdi.hasWidget("Leg"))
      gdi.addInput(xpUseLeg + dx, ypUseLeg + dy, "Leg", legW, 4, editListCB);
    else {
      if (gdi.getText("Leg").empty())
        gdi.setText("Leg", legW);
    }
  }
  else if (MetaList::isAllStageType(type) || MetaList::isAllLegType(type)) {
    if (gdi.hasWidget("UseLeg"))
      gdi.removeWidget("UseLeg");

    if (gdi.hasWidget("Leg"))
      gdi.removeWidget("Leg");

    if (gdi.hasWidget("TUseLeg"))
      gdi.setTextTranslate("TUseLeg", getIndexDescription(type), true);
    else
      gdi.addString("TUseLeg", ypUseLeg, xpUseLeg, 0, getIndexDescription(type));

    if (!gdi.hasWidget("LegSel")) 
      gdi.addSelection(xpUseLeg + dx, ypUseLeg + dy, "LegSel", 160, gdi.scaleLength(300), editListCB);
    vector<pair<wstring, size_t>> items;
    if (MetaList::isAllLegType(type)) {
      items.emplace_back(lang.tl("Automatisk"), 0);
      items.emplace_back(lang.tl("Alla sträckor"), -2);
      for (int j = 1; j <= 50; j++) {
        items.emplace_back(lang.tl("Sträcka X#" + itos(j)), j);
      }
      gdi.addItem("LegSel", items);
      if (leg >= -1)
        gdi.selectItemByData("LegSel", leg + 1);
      else if (leg == -2)
        gdi.selectItemByData("LegSel", -2);
    }
    else {
      items.emplace_back(lang.tl("Alla tidigare etapper"), -2);
      for (int j = 1; j < 20; j++) {
        items.emplace_back(lang.tl("Etapp X#" + itos(j)), j);
      }
      gdi.addItem("LegSel", items);
      gdi.selectItemByData("LegSel", leg >= 0 ? leg + 1 : -2);
    }
  } 
  else {
    if (gdi.hasWidget("LegSel"))
      gdi.removeWidget("LegSel");
    
    if (!gdi.hasWidget("UseLeg")) {
      if (gdi.hasWidget("TUseLeg"))
        gdi.removeWidget("TUseLeg");

      gdi.addCheckbox(xpUseLeg, ypUseLeg, "UseLeg", getIndexDescription(type), editListCB, leg != -1);

      wstring ix = leg >= 0 ? itow(leg + 1) : L"";
      if (!gdi.hasWidget("Leg"))
        gdi.addInput(xpUseLeg + dx, ypUseLeg + dy, "Leg", ix, 4, editListCB);
      else {
        int leg = gdi.getTextNo("Leg");
        gdi.setText("Leg", ix);
      }
      gdi.setInputStatus("Leg", leg != -1);
    }
  }
  
  /*
  if (MetaList::isResultModuleOutput(type)) {
    gdi.check("UseLeg", true);
    gdi.disableInput("UseLeg");
  }*/
  return true;
}

void ListEditor::editListProp(gdioutput &gdi, bool newList) {
  checkUnsaved(gdi);

  if (!currentList)
    return;

  MetaList &list = *currentList;

  if (!newList) {
    gdi.restore("EditList", false);
    gdi.disableInput("EditList");
    gdi.disableInput("SplitPrint");
  }

  gdi.dropLine(0.8);

  int x1 = gdi.getCX();
  int y1 = gdi.getCY();
  int margin = gdi.scaleLength(10);
  gdi.setCX(x1+margin);

  if (!newList) {
    gdi.dropLine();
    gdi.fillDown();
    gdi.addString("", boldLarge, "Listegenskaper").setColor(colorDarkGrey);
    gdi.dropLine();
  }

  gdi.fillRight();
  gdi.pushX();

  gdi.addInput("Name", list.getListName(), 20, 0, L"Listnamn:");

  if (newList) {
    gdi.dropLine(3.5);
    gdi.popX();
  }

  vector< pair<wstring, size_t> > types;
  int currentType = 0;

  int maxX = gdi.getCX();

  list.getBaseType(types, currentType);
  gdi.addSelection("BaseType", 150, 400, 0, L"Listtyp:");
  gdi.addItem("BaseType", types);
  gdi.selectItemByData("BaseType", currentType);
  gdi.autoGrow("BaseType");
  
  list.getResultModule(*oe, types, currentType);
  gdi.addSelection("ResultType", 150, 400, editListCB, L"Resultatuträkning:");
  gdi.addItem("ResultType", types);
  gdi.autoGrow("ResultType");
  gdi.selectItemByData("ResultType", currentType);

  list.getSortOrder(false, types, currentType);
  gdi.addSelection("SortOrder", 170, 400, 0, L"Global sorteringsordning:");
  gdi.addItem("SortOrder", types);
  gdi.autoGrow("SortOrder");
  
  gdi.selectItemByData("SortOrder", currentType);

  list.getSubType(types, currentType);
  gdi.addSelection("SubType", 150, 400, editListCB, L"Sekundär typ:");
  gdi.addItem("SubType", types);
  gdi.selectItemByData("SubType", currentType);
  oListInfo::EBaseType subType = oListInfo::EBaseType(currentType);

  maxX = max(maxX, gdi.getCX());
  gdi.popX();
  gdi.dropLine(3);

  gdi.fillRight();
  gdi.addCheckbox("SupportFrom", "Support time from control", 0, list.supportFrom());
  gdi.addCheckbox("SupportTo", "Support time to control", 0,  list.supportTo());
  gdi.addCheckbox("SupportLegSelection", "Support intermediate legs", 0, list.supportLegSelection());

  gdi.dropLine(2);
  gdi.popX();

  gdi.fillDown();
  gdi.addString("", 1, "Filter");
  gdi.dropLine(0.5);
  vector< pair<wstring, bool> > filters;
  list.getFilters(filters);
  gdi.fillRight();
  int xp = gdi.getCX();
  int yp = gdi.getCY();
  //const int w = gdi.scaleLength(130);
  for (size_t k = 0; k < filters.size(); k++) {
    gdi.addCheckbox(xp, yp, "filter" + itos(k), filters[k].first, 0, filters[k].second);
    xp = gdi.getCX();
    maxX = max(maxX, xp);
    if (k % 10 == 9) {
      xp = x1 + margin;
      gdi.setCX(xp);
      yp += int(1.3 * gdi.getLineHeight());
      gdi.dropLine(1.3);
    }
  }

  gdi.popX();
  gdi.dropLine(2);
  gdi.fillDown();
  gdi.addString("", 1, "Underfilter");
  gdi.dropLine(0.5);
  vector< pair<wstring, bool> > subfilters;
  list.getSubFilters(subfilters);
  gdi.fillRight();
  xp = gdi.getCX();
  yp = gdi.getCY();
  for (size_t k = 0; k < subfilters.size(); k++) {
    gdi.addCheckbox(xp, yp, "subfilter" + itos(k), subfilters[k].first, 0, subfilters[k].second);
    if (subType == oListInfo::EBaseTypeNone)
      gdi.disableInput(("subfilter" + itos(k)).c_str());
    //xp += w;
    xp = gdi.getCX();
    maxX = max(maxX, xp);
    if (k % 10 == 9) {
      xp = x1 + margin;
      gdi.setCX(xp);
      yp += int(1.3 * gdi.getLineHeight());
    }
  }

  gdi.popX();
  gdi.dropLine(2);

  gdi.fillDown();
  gdi.addString("", 1, "Typsnitt");
  gdi.dropLine(0.5);
  gdi.fillRight();
  const wchar_t *expl[4] = {L"Rubrik", L"Underrubrik", L"Lista", L"Underlista"};
  vector< pair<wstring, size_t> > fonts;
  gdi.getEnumeratedFonts(fonts);
  sort(fonts.begin(), fonts.end());

  for (int k = 0; k < 4; k++) {
    string id("Font" + itos(k));
    gdi.addCombo(id, 200, 300, 0, expl[k]);
    gdi.addItem(id, fonts);

    gdi.setText(id, list.getFontFace(k));
    gdi.setCX(gdi.getCX()+20);
    int f = list.getFontFaceFactor(k);
    wstring ff = f == 0 ? L"100 %" : itow(f) + L" %";
    gdi.addInput("FontFactor" + itos(k), ff, 4, 0, L"Skalfaktor", L"Relativ skalfaktor för typsnittets storlek i procent");
    f = list.getExtraSpace(k);
    gdi.addInput("ExtraSpace" + itos(k), itow(f), 4, 0, L"Avstånd", L"Extra avstånd ovanför textblock");
    if (k == 1) {
      gdi.dropLine(3);
      gdi.popX();
    }
  }

  if (!newList) {
    gdi.dropLine(0.8);
    gdi.setCX(gdi.getCX()+20);
    gdi.addButton("ApplyListProp", "OK", editListCB);
    gdi.addButton("Cancel", "Avbryt", editListCB);
  }
  else {
    gdi.setCX(x1);
    gdi.setCY(gdi.getHeight());
    gdi.dropLine();
    gdi.addButton("ApplyListProp", "Skapa", editListCB);
    gdi.addButton("CancelNew", "Avbryt", editListCB);
  }

  gdi.dropLine(3);
  int maxY = gdi.getCY();

  gdi.fillDown();
  gdi.popX();
  gdi.setData("IsEditingList", 1);

  RECT rc;
  rc.top = y1;
  rc.left = x1;
  rc.right = maxX + gdi.scaleLength(6);
  rc.bottom = maxY;

  if (!newList) {
    gdi.addRectangle(rc, colorLightBlue, true);
  }

  gdi.scrollToBottom();
  gdi.refresh();
  gdi.setInputFocus("Name");
}

void ListEditor::statusSplitPrint(gdioutput& gdi, bool status) {
  gdi.setInputStatus("Split", status);
  gdi.setInputStatus("Speed", status);
  gdi.setInputStatus("Result", status);
  gdi.setInputStatus("Analysis", status);
}

void ListEditor::splitPrintList(gdioutput& gdi) {
  checkUnsaved(gdi);

  if (!currentList)
    return;

  MetaList& list = *currentList;

  gdi.restore("EditList", false);
  gdi.disableInput("EditList");
  gdi.disableInput("SplitPrint");
  
  gdi.dropLine(0.8);

  int x1 = gdi.getCX();
  int y1 = gdi.getCY();
  int margin = gdi.scaleLength(10);
  gdi.setCX(x1 + margin);

  gdi.dropLine();
  gdi.fillDown();
  gdi.addString("", boldLarge, "Sträcktidsutskrift").setColor(colorDarkGrey);
  gdi.dropLine();
  
  gdi.fillRight();
  gdi.pushX();

  bool isSP = list.isSplitPrintList();
  auto sp = list.getSplitPrintInfo();
  gdi.fillDown();

  gdi.addCheckbox("UseForSplit", "Använd listan för sträcktidsutskrift", editListCB, isSP);

  gdi.dropLine(0.5);
  gdi.fillRight();
  gdi.addCheckbox("Split", "Inkludera sträcktider", nullptr, isSP ? sp->includeSplitTimes : true);
  gdi.addCheckbox("Speed", "Inkludera tempo", nullptr, isSP ? sp->withSpeed : true);
  gdi.addCheckbox("Result", "Inkludera individuellt resultat", nullptr, isSP ? sp->withResult : true);
  gdi.addCheckbox("Analysis", "Inkludera bomanalys", nullptr, isSP ? sp->withAnalysis : true);

  statusSplitPrint(gdi, isSP);

  gdi.dropLine(0.8);
  gdi.setCX(gdi.getCX() + 20);
  gdi.addButton("ApplySplitList", "OK", editListCB);
  gdi.addButton("Cancel", "Avbryt", editListCB);

  gdi.dropLine(3);
  int maxY = gdi.getCY();
  int maxX = gdi.getCX();

  gdi.fillDown();
  gdi.popX();
  gdi.setData("IsSplitListEdit", 1);

  RECT rc;
  rc.top = y1;
  rc.left = x1;
  rc.right = maxX + gdi.scaleLength(6);
  rc.bottom = maxY;

  gdi.addRectangle(rc, colorLightBlue, true);
  
  gdi.scrollToBottom();
  gdi.refresh();
}

void ListEditor::makeDirty(gdioutput &gdi, DirtyFlag inside, DirtyFlag outside) {
  if (inside == MakeDirty)
    dirtyInt = true;
  else if (inside == ClearDirty)
    dirtyInt = false;

  if (outside == MakeDirty)
    dirtyExt = true;
  else if (outside == ClearDirty)
    dirtyExt = false;

  if (gdi.hasWidget("SaveInside")) {
    gdi.setInputStatus("SaveInside", dirtyInt || lastSaved != SavedInside);
  }

  if (gdi.hasWidget("SaveFile")) {
    gdi.setInputStatus("SaveFile", dirtyExt || lastSaved != SavedFile);
  }
}

bool ListEditor::checkSave(gdioutput &gdi) {
  if (dirtyInt || dirtyExt) {
    gdioutput::AskAnswer answer = gdi.askCancel(L"Vill du spara ändringar?");
    if (answer == gdioutput::AskAnswer::AnswerCancel)
      return false;

    if (answer == gdioutput::AskAnswer::AnswerYes) {
      if (currentIndex >= 0)
        gdi.sendCtrlMessage("SaveInside");
      else if (gdi.sendCtrlMessage("SaveFile") == 0)
        return false;
    }
    makeDirty(gdi, ClearDirty, ClearDirty);
  }

  return true;
}

void ListEditor::enableOpen(gdioutput &gdi) {
  ListBoxInfo lbi;
  bool enabled = true;
  if (gdi.getSelectedItem("OpenList", lbi)) {
    if (oe->getListContainer().isInternal(lbi.data))
      enabled = false;
  }

  gdi.setInputStatus("DoOpen", enabled);  
}

void ListEditor::handleAutoComplete(gdioutput &gdi, AutoCompleteInfo &info) {
  gdi.selectItemByData("Type", info.getCurrentInt());
  updateType(info.getCurrentInt(), gdi);
  gdi.clearAutoComplete("");
  gdi.setText("SearchText", getSearchString());
  gdi.TabFocus(1);
}
