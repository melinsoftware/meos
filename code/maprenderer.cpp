/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

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
#define _USE_MATH_DEFINES

#include "stdafx.h"

#include <algorithm>
#include <string>

#include "gdioutput.h"
#include "meosexception.h"
#include "gdistructures.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "oEvent.h"
#include "gdiconstants.h"
#include "image.h"
#include "maprenderer.h"
#include "xmlparser.h"
#include "csvparser.h"
#include "utm_interface.h"

using namespace std;
extern Image image;

void MapData::load(const xmlobject& data) {
  imageId = data.getObjectInt64u("mapid");
  top = data.getObjectDouble("top");
  bottom = data.getObjectDouble("bottom");
  left = data.getObjectDouble("left");
  right = data.getObjectDouble("right");

  latCenter = data.getObjectDouble("latc");
  lngCenter = data.getObjectDouble("lngc");

  string wdata;
  data.getObjectString("world", wdata);
  vector<string> wcoord;
  split(wdata, ",", wcoord);
  
  world.clear();
  for (auto& w : wcoord)
    world.push_back(atof(w.c_str()));
}

void MapData::save(xmlparser& data) const {
  data.startTag("Map");
  data.write64u("mapid", imageId);
  data.write("top", top);
  data.write("bottom", bottom);
  data.write("left", left);
  data.write("right", right);

  data.write("latc", latCenter);
  data.write("lngc", lngCenter);
  string wdata;
  for (double d : world) {
    if (!wdata.empty())
      wdata += ",";
    wdata += oDataContainer::formatDouble(d);
  }
  data.write("world", wdata);
  data.endTag();
}

void MapData::readWorld(const wstring& worldFile) {
  csvparser csv;
  list<vector<wstring>> out;
  csv.parse(worldFile, out);
  world.clear();
  if (out.size() >= 6) {
    for (auto& line : out) {
      if (line.size() == 1 && !line.begin()->empty()) {
        double val = _wtof(line[0].data());
        if (val != 0 || line.begin()->at(0) == '0')
          world.push_back(val);
      }
    }
  }
  if (world.size() >= 6)
    world.resize(6);
  else
    world.clear();
}

void MapData::setMapPos(double top, double left, double bottom, double right) {
  this->top = top;
  this->left = left;
  this->right = right;
  this->bottom = bottom;
}

void MapData::getDimensions(int& h, int& w) const {
  h = image.getHeight(imageId);
  w = image.getWidth(imageId);
}

void MapData::render(gdioutput& gdi, int xp, int yp) const {
  int h = image.getHeight(imageId);
  int w = image.getWidth(imageId);

  gdi.addImage("", xp, yp, 0, itow(imageId));
  gdi.refreshFast();
}

pair<int, int>  MapData::render(oEvent& oe, gdioutput& gdi, int xp, int yp, 
                                const vector<tuple<oControl*, wstring, RenderCType>>& ctrlList,
                                bool fullMap, int maxWidth, int maxHeight) const {
  shared_ptr<const MapData> md = shared_from_this();

  
  int margin = metersToPixels(100);
  auto rdr = make_shared<MapDataRenderer>(gdi, xp, yp, 75.0/double(margin), md);

  oe.loadImage(imageId);
  int xmax = 0, xmin = numeric_limits<int>::max();
  int ymax = 0, ymin = numeric_limits<int>::max();
  bool any = false;
  for (auto &[ctrl, label, type] : ctrlList) {
    if (ctrl) {
      double xpos = ctrl->getDCI().getDouble("xpos");
      double ypos = ctrl->getDCI().getDouble("ypos");

      double lat = ctrl->getDCI().getDouble("latcrd");
      double lon = ctrl->getDCI().getDouble("longcrd");
      int xc, yc;
      if (mapCoordinate(lon, lat, xc, yc)) {
        any = true;
        rdr->addControl(type, xc, yc, label);
        xmin = min(xmin, xc);
        xmax = max(xmax, xc);

        ymin = min(ymin, yc);
        ymax = max(ymax, yc);
      }
      else {
        rdr->addControl(RenderCType::NotMappableError, 0, 0, label);        
      }
    }
  }

  if (fullMap) {
    int h, w;
    getDimensions(h, w);
    xmin = 0;
    xmax = w;
    ymin = 0;
    ymax = h;
    margin = 0;
  }
  else if (!any) {
    throw meosException("Kan inte lokalisera kontrollerna på kartan.");
  }
 
  int wm = xmax - xmin + 2 * margin;
  int hm = ymax - ymin + 2 * margin;

  int renderW = gdi.scaleLength(wm * rdr->getScale());
  int renderH = gdi.scaleLength(hm * rdr->getScale());

  double sx = maxWidth > 0 ? double(maxWidth) / double(renderW) : 1.0;
  double sy = maxHeight> 0 ? double(maxHeight) / double(renderH) : 1.0;


  if (min(sx, sy) < 1.0) {
    
    // Snap to resonable nearby scale
    double s = scaleToUse(min(sx, sy));
    
    rdr->scaleScale(gdi, s);
    renderH = int(renderH * s);
    renderW = int(renderW * s);
  }

  gdi.addImage("", yp, xp, 0, itow(imageId), renderW, renderH, xmin-margin, ymin-margin, wm, hm);
  RECT rc;
  rc.left = xp - 1;
  rc.top = yp - 1;
  rc.right = xp + renderW + 1;
  rc.bottom = yp + renderH + 1;
  gdi.addRectangle(rc, GDICOLOR::colorTransparent, true);

  rdr->setView(xmin - margin, xmax + margin, ymin - margin, ymax + margin);

  gdi.setMapRenderer(rdr);
  gdi.refreshFast();
  return make_pair(xp + renderW, yp + renderH);
}

double MapData::scaleToUse(double sIn) const {
  if (sIn > 0.99 && sIn < 1.2)
    return 1.0;

  if (sIn < 1) {
    double s = 1.0;
    for (int i = 0; i < 100; i++) {
      s *= 0.8;
      if (sIn > s)
        return s;
    }
  }
  else {
    double s = 1.2;
    for (int i = 0; i < 100; i++) {
      double sOld = s;
      s *= 1.25;
      if (sIn < s)
        return sOld;
    }
  }
  return sIn;
}

void MapData::setImage(uint64_t imgId, bool clearWorld) {
  imageId = imgId;
  if (clearWorld)
    world.clear();
}

void MapData::geoReference(const vector<array<double, 4>> &cpt) {
  auto &[long1, lat1, x1, y1] = cpt[0];
  auto &[long2, lat2, x2, y2] = cpt[1];

  int h, w;
  getDimensions(h, w);
  double utmx1, utmy1, utmx2, utmy2;

  int zone = utm::LatLonToUTMXY(lat1, long1, 0, utmx1, utmy1);
  utm::LatLonToUTMXY(lat2, long2, zone, utmx2, utmy2);
  double xp1 = x1 * w;
  double xp2 = x2 * w;

  double yp1 = y1 * h;
  double yp2 = y2 * h;

  double XU = utmx1 - utmx2;
  double YU = utmy1 - utmy2;

  double X = xp1 - xp2;
  double Y = yp1 - yp2;

  double A = (XU * X - Y * YU) / (X * X + Y * Y);
  double B = (X * YU + XU * Y) / (X * X + Y * Y);

  double C = utmx1 - A * xp1 - B * yp1;
  double F = utmy1 - B * xp1 + A * yp1;
  
  world.resize(6);
  world[0] = A;
  world[1] = B;
  world[2] = B;
  world[3] = -A;
  world[4] = C;
  world[5] = F;
}

pair<double, double> MapData::mapPoint(double x, double y) const {
  x = (x - left) / (right - left);
  y = (y - top) / (bottom - top);
  return make_pair(x, y);
}

pair<int, int> MapDataContainer::render(oEvent& oe, gdioutput& gdi, 
                                        int xp, int yp,
                                        const vector<tuple<oControl*, wstring, RenderCType>> &ctrl,
                                        bool fullMap,
                                        int maxWidth,
                                        int maxHeight) const {
  if (!maps.empty())
    return maps[0]->render(oe, gdi, xp, yp, ctrl, fullMap, maxWidth, maxHeight);
  else
    return make_pair(-1, -1);
}

int MapDataContainer::add(shared_ptr<MapData>& newMap) {
  maps.push_back(newMap);
  return maps.size() - 1;
}

void MapDataContainer::clear() {
  while (!maps.empty()) {
    // auto id = maps.back()->getImage();
    // Images are not reference counted, so in general we cannot remove the (GC pass needed)

    maps.pop_back();
  }
}

void MapDataContainer::geoReference(int ix, const vector<array<double, 4>> &cpt) {
  if (ix < maps.size())
    return maps[ix]->geoReference(cpt);
}


bool MapDataContainer::validCoordinates(const oControl& ctrl) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      if (map->validCoordinates(ctrl))
        return true;
  }
  return false;
}

bool MapDataContainer::getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      if (map->getCoordinatePosition(ctrl, dimx, dimy, xp, yp))
        return true;
  }
  return false;
}

void  MapDataContainer::getUsedImage(set<uint64_t>& img) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      img.insert(map->getImage());
  }
}

void MapDataContainer::serialize(xmlparser& xml) const {
  if (!maps.empty()) {
    xml.startTag("Maps");
    for (auto& map : maps) {
      map->save(xml);
    }
    xml.endTag();
  }
}

string MapDataContainer::save() const {
  string out;
  xmlparser xml;
  xml.openMemoryOutput(true);
  
  serialize(xml);

  xml.getMemoryOutput(out);  
  return out;
}

void MapDataContainer::load(const string& raw) {
  xmlparser xml;
  xml.readMemory(raw, 0);
  deserialize(xml.getObject("Maps"));
}

bool MapDataContainer::deserialize(const xmlobject& xmaps) {
  maps.clear();
  if (xmaps) {
    xmlList mapLst;
    xmaps.getObjects("Map", mapLst);
    for (auto& m : mapLst) {
      auto md = make_shared<MapData>();
      try {
        md->load(m);
        maps.emplace_back(md);
      }
      catch (const std::exception& ) {
      }
    }
  }
  return !maps.empty();
}

void MapDataRenderer::renderDecoration(HDC hDC, gdioutput& gdi) const {
  //HPEN pen = CreatePen(PS_SOLID, scaleLength(2), RGB(0, 0, 0));
  SelectObject(hDC, hPen);
  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  int h, w;
  data->getDimensions(h, w);
  int rad = gdi.scaleLength(data->metersToPixels(40)*scale);
  int lastCX = -1;
  int lastCY = -1;
  bool usedTop = false;

  TextInfo ti;
  ti.setColor(GDICOLOR(RGB(250, 50, 215)));
  ti.format = boldLarge;
  ti.font = L"Calibri";
  ti.text = L"222";
  gdi.calcStringSize(ti, hDC);
  
  gdi.formatString(ti, hDC);
  int rf = rad / 8;
  int texth = ti.getHeight();

  int lastcx = -1;
  int lastcy = -1;

  for (int j = 0; j < controls.size(); j++) {
    auto& c = controls[j];

    int cx = x + gdi.scaleLength((c.x - xmin)*scale) - gdi.getOffsetX();
    int cy = y + gdi.scaleLength((c.y - ymin)*scale) - gdi.getOffsetY();

    if (c.type == RenderCType::NotMappableError) {
      cx = -1;
      cy = -1;
      lastcx = -1;
      lastcy = -1;
    }
    if (c.type == RenderCType::Finish) {
      Ellipse(hDC, cx - rad - rf, cy - rad - rf, cx + rad + rf, cy + rad + rf);
      Ellipse(hDC, cx - rad + rf, cy - rad + rf, cx + rad - rf, cy + rad - rf);
    }
    else if (c.type == RenderCType::Start) {
      double ang = 0;
      if (j + 1 < controls.size()) {
        double dx = controls[j + 1].x - c.x;
        double dy = controls[j + 1].y - c.y;
        if (dx != 0 || dy != 0)
          ang = atan2(dy, dx);
      }
      
      constexpr double d120 = 2.0 * M_PI / 3.0;
      int p1x = cx + int(cos(ang) * rad);
      int p1y = cy + int(sin(ang) * rad);

      int p2x = cx + int(cos(ang + d120) * rad);
      int p2y = cy + int(sin(ang + d120) * rad);

      int p3x = cx + int(cos(ang - d120) * rad);
      int p3y = cy + int(sin(ang - d120) * rad);

      MoveToEx(hDC, p1x, p1y, nullptr);
      LineTo(hDC, p2x, p2y);
      LineTo(hDC, p3x, p3y);
      LineTo(hDC, p1x, p1y);
    }
    else {
      Ellipse(hDC, cx - rad, cy - rad, cx + rad, cy + rad);
    }

    bool isCourse = true;
    if (c.type == RenderCType::CorrectControl || c.type == RenderCType::WrongControl) {
      isCourse = false;
      int cmin = 1000000, cmax = -10000000;
      for (auto& c2 : controls) {
        cmin = min(cmin, c2.y);
        cmax = min(cmax, c2.y);
      }

      if (c.y == cmin && !usedTop) {
        usedTop = true;
        TextOut(hDC, cx - rad, cy - rad - rf - texth, c.label.c_str(), c.label.size());
      }
      else {
        TextOut(hDC, cx - rad, cy + rad + rf, c.label.c_str(), c.label.size());
      }
    }
  
    if (lastcx != -1 && lastcy != -1) {
      double dx = cx - lastcx;
      double dy = cy - lastcy;
      double dist = sqrt(dx * dx + dy * dy);
      if (dist > 2*(rad + 2*rf) && isCourse) {
        int xoff = int(dx * (rad + 2*rf) / dist);
        int yoff = int(dy * (rad + 2*rf) / dist);

        MoveToEx(hDC, lastcx + xoff, lastcy + yoff, nullptr);
        LineTo(hDC, cx - xoff, cy - yoff);
      }
    }

    lastcx = cx;
    lastcy = cy;
  }

  SelectObject(hDC, GetStockObject(BLACK_PEN));
}

MapDataRenderer::MapDataRenderer(const gdioutput &gdi, int x, int y, double scale, const shared_ptr<const MapData>& src) :
 x(x), y(y), scale(scale), data(src), hPen(nullptr) {
  scaleScale(gdi, 1.0);
}

MapDataRenderer::~MapDataRenderer() {
  DeleteObject(hPen);
}

void MapDataRenderer::scaleScale(const gdioutput &gdi, double s) { 
  if (hPen) {
    DeleteObject(hPen);
    hPen = nullptr;
  }

  scale *= s; 
  int w = std::max(2, gdi.scaleLength(scale * data->metersToPixels(3.0)));
  hPen = CreatePen(PS_SOLID, w, RGB(250, 50, 215));
}

void MapDataRenderer::setView(int xmin, int xmax, int ymin, int ymax) {
  this->xmax = xmax;
  this->xmin = xmin;

  this->ymax = ymax;
  this->ymin = ymin;
}

void MapDataRenderer::addNamedControl(const string &tag, double relX, double relY, const wstring &label) {
  removeNamedControl(tag);
  int h, w;
  data->getDimensions(h, w);
  int x = relX * w;
  int y = relY * h;
  controls.emplace_back(RenderCType::CorrectControl, x, y, label);
  controls.back().tag = tag;
}

void MapDataRenderer::removeNamedControl(const string &tag) {
  for (auto &c : controls) {
    if (c.tag == tag) {
      c = controls.back();
      controls.pop_back();
      break;
    }
  }
}

int MapData::metersToPixels(int meter) const {
  return (int)metersToPixels(double(meter));
}

double MapData::metersToPixels(double meter) const {
  if (world.size() == 6) {
    if (isUTM) {
      double px = (2.0 * meter) / (std::abs(world[0]) + std::abs(world[3])); // XX
      return px;
    }
    else {
      double xx, yy, xxPix, yyPix;
      int zone = utm::LatLonToUTMXY(latCenter, lngCenter, 0, xx, yy);

      utm::LatLonToUTMXY(world[5], world[4], zone, xx, yy);
      utm::LatLonToUTMXY(world[5] + world[3], world[4] + world[0], zone, xxPix, yyPix);

      double px = (2.0 * meter) / (std::abs(xx-xxPix) + std::abs(yy-yyPix)); // XX
      return px;
    }
  }
  else
    return meter;
}

bool MapData::mapCoordinate(double lng, double lat, int& x, int& y) const {
  x = -1, y = -1;

  double xx, yy;
  int zone = utm::LatLonToUTMXY(latCenter, lngCenter, 0, xx, yy);

  if (world.size() == 6) {
    const double &offX = world[4];
    const double &offY = world[5];

    int w, h;
    getDimensions(h, w);
    double relX;
    double relY;

    const double E = world[3];
    const double C = world[4];
    const double F = world[5];
    const double A = world[0];
    const double D = world[1];
    const double B = world[2];

    bool ok = false;
    int iter = 0;
    while (!ok && iter <= 6) {
      int off = 0;
      if (iter > 0) {
        off = (iter + 1) / 2;
        if (iter % 2 == 0)
          off = -off;
      }
      int mzone = (zone + 59 + off) % 60 + 1;
      utm::LatLonToUTMXY(lat, lng, mzone, xx, yy);

      double xp = (xx * E - yy * B + B * F - E * C) / (A * E - D * B);
      double yp = (-xx * D + yy * A + D * C - F * A) / (A * E - D * B);

      relX = xp / w;
      relY = yp / h;

      ok = true;

      if (!(relX >= 0.0 && relX <= 1.0))
        ok = false;
      else if (!(relY >= 0.0 && relY <= 1.0))
        ok = false;

      iter++;
    }

    if (ok) {
      x = min(w, int(round(w * relX)));
      y = min(h, int(round(h * relY)));
      isUTM = true;
    }
    else {
      double c1x = F;
      double c1y = C;

      double c2x = F + w * A;
      double c2y = C + h * E;

      //xp = (lat * E - lng * B + B * F - E * C) / (A * E - D * B);
      //yp = (-lat * D + lng * A + D * C - F * A) / (A * E - D * B);
      //xp = A * lat + B * lng + C;
      //yp = D * lat + E * lng + F;
      
      
      //xp = (F - lat) / A;
      //yp = (C - lng) / E;

      double xp = (lng * E - lat * B + B * F - E * C) / (A * E - D * B);
      double yp = (-lng * D + lat * A + D * C - F * A) / (A * E - D * B);

      relX = xp / w;
      relY = yp / h;
      ok = true;

      if (!(relX >= 0.0 && relX <= 1.0))
        ok = false;
      else if (!(relY >= 0.0 && relY <= 1.0))
        ok = false;

      if (ok) {
        isUTM = false;
        x = min(w, int(round(w * relX)));
        y = min(h, int(round(h * relY)));
      }
    }

    return ok;
  }

  return false;
}

bool MapData::validCoordinates(const oControl& ctrl) const {
  int dimx, dimy, xp, yp;
  return getCoordinatePosition(ctrl, dimx, dimy, xp, yp);
}

bool MapData::getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const {
  double xpos = ctrl.getDCI().getDouble("xpos");
  double ypos = ctrl.getDCI().getDouble("ypos");
  double lat = ctrl.getDCI().getDouble("latcrd");
  double lon = ctrl.getDCI().getDouble("longcrd");

  getDimensions(dimy, dimx);

  if (mapCoordinate(lon, lat, xp, yp))
    return true;

  if (xpos == 0.0 && ypos == 0.0)
    return false; // Unsupported (default, unset) coordinate

  if (ypos <= top || ypos >= bottom)
    return false;

  if (xpos <= left || xpos >= right)
    return false;

  return true;
}
