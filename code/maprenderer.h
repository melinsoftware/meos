#pragma once

#include <cstdint>
#include <memory>
#include <array>
#include <tuple>

class xmlobject;
class xmlparser;

class gdioutput;
class oControl;

enum class RenderCType {
  Start,
  Finish,
  CourseControl,
  CorrectControl,
  WrongControl,
  NotMappableError,
};

class MapData : public std::enable_shared_from_this<MapData> {
  uint64_t imageId;
  double top;
  double left;
  double bottom;
  double right;

  double latCenter;
  double lngCenter;
  vector<double> world;
  mutable bool isUTM = true;

  mutable set<double> usedScale;

  double scaleToUse(double sIn) const;
public:

  void getDimensions(int& h, int& w) const;

  void readWorld(const wstring &worldFile);

  void setCenter(double lng, double lat) {
    lngCenter = lng;
    latCenter = lat;
  }

  int metersToPixels(int meter) const;
  double metersToPixels(double meter) const;

  bool mapCoordinate(double lng, double lat, int& x, int& y) const;

  bool validCoordinates(const oControl& ctrl) const;
  bool getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const;

  void load(const xmlobject& data);
  void save(xmlparser& data) const;
  pair<double, double> mapPoint(double x, double y) const;

  void render(gdioutput& gdi, int xp, int yp) const;

  pair<int, int> render(oEvent &oe, gdioutput& gdi, int xp, int yp,
                        const vector<tuple<oControl *, 
                        wstring, RenderCType>> &ctrl,
                        bool fullMap,
                        int maxWidth, 
                        int maxHeight) const;
  
  void geoReference(const vector<array<double, 4>> &cpt);
  bool isGeoReferenced() const {
    return world.size() == 6;
  }

  void setImage(uint64_t imgId, bool clearWorld);
  uint64_t getImage() const {
    return imageId;
  }
  void setMapPos(double top, double left, double bottom, double right);
};

class MapDataContainer {
  vector<shared_ptr<MapData>> maps;
public:
  void getUsedImage(set<uint64_t>& img) const;

  void serialize(xmlparser& xml) const;
  bool deserialize(const xmlobject& xml);

  string save() const;
  void load(const string& raw);

  /** Start render a map at specified position (xp, yp). Returns lower right corner coordinates */
  pair<int, int> render(oEvent& oe, gdioutput& gdi, int xp, int yp, 
                        const vector<tuple<oControl*, wstring, RenderCType>> &ctrl,
                        bool fullMap = false, int maxWidth = -1, int maxHeight = -1) const;

  void clear();

  int add(shared_ptr<MapData>& newMap);

  void geoReference(int ix, const vector<array<double, 4>> &cpt);
  
  bool validCoordinates(const oControl& ctrl) const;

  bool getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const;
};

class MapDataRenderer {
  HPEN hPen;
  int x;
  int y;
  
  int xmin = 0;
  int xmax = 0;
  int ymin = 0;
  int ymax = 0;
  double scale;
  shared_ptr<const MapData> data;

  struct ControlData {
    ControlData() = default;
    ControlData(RenderCType type, int x, int y, wstring label) : type(type), x(x), y(y), label(label) {}

    RenderCType type = RenderCType::CourseControl;
    int x = 0;
    int y = 0;
    string tag;
    wstring label;
  };

  vector<ControlData> controls;

public:
  MapDataRenderer(const gdioutput& gdi, int x, int y, double scale, const shared_ptr<const MapData> &src);
  ~MapDataRenderer();

  int addControl(RenderCType type, int x, int y, const wstring &label) {
    controls.emplace_back(type, x, y, label);
    return controls.size() - 1;
  }
  double getScale() const { return scale; }
  void scaleScale(const gdioutput &gdi, double s);
  void addNamedControl(const string &tag, double relX, double relY, const wstring &label);
  void removeNamedControl(const string &tag);

  void setView(int xmin, int xmax,  int ymin, int ymax);
  void renderDecoration(HDC hDC, gdioutput& gdi) const;
};
