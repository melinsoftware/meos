#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2019 Melin Software HB

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

class oEvent;

/** Support for context specific interpretation of import/export data*/
class ImportFormats {
public:
  enum ImportFormatOptions {
    Default
  };

  //static void getImportFormats(vector< pair<string, size_t> > &formats);

  //static int getDefault(oEvent &oe);

  enum ExportFormats {
    IOF30 = 1,
    IOF203 = 2,
    OE = 3,
    HTML = 5
  };

  static void getExportFormats(vector< pair<wstring, size_t> > &types, bool exportFilter);

  static void getExportFilters(bool exportFilters, vector< pair<wstring, wstring> > &ext);

  static ExportFormats getDefaultExportFormat(oEvent &oe);

  static ExportFormats setExportFormat(oEvent &oe, int raw);

  ImportFormats(int opt) : option((ImportFormatOptions)opt) {}

  ImportFormatOptions getOption() const {
    return option;
  }

  static void getOECSVLanguage(vector< pair<wstring, size_t> > &typeLanguages); 
  
  static int getDefaultCSVLanguage(oEvent &oe);

  static wstring getExtension(ExportFormats fm);

  private:
    ImportFormatOptions option;
};

struct ExportSplitsData {
  int cSVLanguageHeaderIndex = 0;
  ImportFormats::ExportFormats filterIndex = ImportFormats::ExportFormats::IOF30;
  bool includeStage = true;
  bool unroll = false;
  bool includeSplits = true;
  bool withPartialResults = false;
  pair<string, string> preferredIdTypes;

  int legType = -1;
};