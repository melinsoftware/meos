/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2021 Melin Software HB

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

#include <cassert>
#include "prefseditor.h"

#include "gdioutput.h"
#include "meosexception.h"
#include "gdistructures.h"
#include "meos_util.h"
#include "localizer.h"
#include "oEvent.h"

PrefsEditor::PrefsEditor(oEvent *oe) : oe(oe) {
}

PrefsEditor::~PrefsEditor() {
}

void PrefsEditor::showPrefs(gdioutput &gdi) {
  vector< pair<string, PropertyType> > iprefs;

  oe->listProperties(true, iprefs);
  map<string, vector< pair<string, PropertyType> > > orderPrefs;
  for (size_t k = 0; k < iprefs.size(); k++) {
    char c = toupper(iprefs[k].first[0]);
    string key;
    key.push_back(c);
    orderPrefs[key].push_back(iprefs[k]);
  }

  const int bw = gdi.scaleLength(80);
  int basePos = gdi.getCX();
  int valuePos = basePos + gdi.scaleLength(160);
  int editPos = valuePos + gdi.scaleLength(280);
  int descPos = editPos + bw + gdi.scaleLength(10);

  gdi.fillDown();
  for (map<string, vector< pair<string, PropertyType> > >::const_iterator it = orderPrefs.begin();
          it != orderPrefs.end(); ++it) {
    const vector<pair< string, PropertyType> > &prefs = it->second;
    gdi.addStringUT(boldLarge, it->first);
    for (size_t k = 0; k < prefs.size(); k++) {
      int y = gdi.getCY();
      gdi.addStringUT(y, basePos, 0, prefs[k].first, 0, 0, L"Consolas;1.1");
      wstring rawVal = oe->getPropertyString(prefs[k].first.c_str(), L"");
      wstring val = codeValue(rawVal, prefs[k].second);
      
      gdi.addString("value" + prefs[k].first, y, valuePos, 0, L"#" +  val, editPos - valuePos, 0, L"Consolas;1").
        setColor(selectColor(rawVal, prefs[k].second)).setExtra(prefs[k].second);
    
      gdi.addButton(editPos, y - gdi.getLineHeight()/4, "Edit_" + prefs[k].first, "Ändra").setHandler(this);
    
      wstring dkey = L"prefs" + gdi.widen(prefs[k].first);
      wstring desc = lang.tl(dkey);
      if (desc != dkey) {
        gdi.addStringUT(y, descPos, 0, desc).setColor(colorDarkGreen);
      }
    }
    gdi.dropLine(0.5);
  }
}

void PrefsEditor::handle(gdioutput &gdi, BaseInfo &data, GuiEventType type) {
  if (type == GUI_BUTTON) {
    ButtonInfo bi(dynamic_cast<const ButtonInfo &>(data));

    if (bi.id.substr(0, 5) == "Edit_") {
      if (gdi.hasData("EditPrefs"))
        gdi.restore("BeforeEdit");
      
      gdi.setRestorePoint("BeforeEdit");
      gdi.fillDown();
      gdi.dropLine();
      int x = gdi.getCX();
      int y = gdi.getCY();
      gdi.setCX(x + gdi.getLineHeight());
      string pref = bi.id.substr(5);
      PropertyType type = (PropertyType)gdi.getBaseInfo(("value" + pref).c_str()).getExtraInt();
      gdi.setData("EditPrefs", type);
      gdi.dropLine();
      gdi.addString("", fontMediumPlus, "Ändra X#" + pref);
      wstring dkey = L"prefs" + gdi.widen(pref);
      wstring desc = lang.tl(dkey);
      gdi.dropLine();
      if (desc != dkey) {
        gdi.addStringUT(0, desc +L":");
      }
      wstring val = oe->getPropertyString(pref.c_str(), L"");
      if (type == String)
        gdi.addInput("Value", val, 48);
      else if (type == Integer)
        gdi.addInput("Value", val, 16);
      else {
        gdi.addSelection("ValueBoolean", 200, 50, 0);
        gdi.addItem("ValueBoolean", codeValue(L"0", Boolean), 0);
        gdi.addItem("ValueBoolean", codeValue(L"1", Boolean), 1);
        gdi.selectItemByData("ValueBoolean", val == L"0" ? 0 : 1);
      }
      gdi.dropLine();
      gdi.fillRight();
      gdi.addButton("Save_" + pref, "Ändra").setHandler(this); 
      gdi.addButton("Cancel", "Avbryt").setHandler(this); 
      gdi.dropLine(3);

      RECT rc = {x,y, gdi.getWidth(), gdi.getCY()};
      gdi.addRectangle(rc, colorLightCyan);
      gdi.scrollToBottom();
      gdi.setCX(x);
      gdi.refresh();
    } 
    else if (bi.id.substr(0, 5) == "Save_") {
      string pref = bi.id.substr(5);
      wstring value;
      if (gdi.hasWidget("ValueBoolean")) {
        ListBoxInfo lbi;
        gdi.getSelectedItem("ValueBoolean", lbi);
        value = itow(lbi.data);
      }
      else
        value = gdi.getText("Value");

      PropertyType type = (PropertyType)(gdi.getDataInt("EditPrefs"));
      oe->setProperty(pref.c_str(), value);
      dynamic_cast<TextInfo *>(gdi.setText("value" + pref, codeValue(value, type)))->
             setColor(selectColor(value, type));
      gdi.restore("BeforeEdit", false);
      gdi.refresh();
    }
    else if (bi.id == "Cancel") {
      gdi.restore("BeforeEdit");
    }
  }
  
  return;
}

wstring PrefsEditor::codeValue(const wstring &val, PropertyType p) const {
  if (p == Boolean) {
    if (_wtoi(val.c_str()) != 0) {
      return lang.tl(L"true[boolean]");
    }
    else {
      return lang.tl(L"false[boolean]");
    }
  }
  else if (p == Integer)
    return itow(_wtoi(val.c_str()));
  else
    return val;
}

GDICOLOR PrefsEditor::selectColor(const wstring &val, PropertyType p) const {
  if (p == Boolean) {
    if (_wtoi(val.c_str()) != 0) {
      return colorDarkGreen;
    }
    else {
      return colorDarkRed;
    }
  }
  if (p == Integer)
    return colorDarkBlue;
  return colorDefault;
}
