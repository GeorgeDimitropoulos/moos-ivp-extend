/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: Summer 2026                                     */
/************************************************************/

#include <cmath>
#include <limits>
#include <sstream>
#include "GenRescue.h"
#include "MBUtils.h"
#include "XYPoint.h"
#include "XYSegList.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  m_vname = "";

  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = false;
  m_nav_y_set = false;

  m_total_alerts = 0;
  m_duplicate_alerts = 0;
  m_paths_posted = 0;
  m_last_path_repost_time = 0;

  // Athens rescue field centroid and safety margin.
  // The swimmer positions are random, but the field and buoys are fixed.
  m_field_cx = -96.5;
  m_field_cy = -19.5;
  m_field_margin = 4.0;

  initMap();
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    string sval = msg.GetString();

    bool handled = true;

    if(key == "SWIMMER_ALERT") 
      handled = handleMailNewSwimmer(sval);

    else if((key == "FOUND_SWIMMER") || (key == "RESCUED_SWIMMER")) 
      handled = handleMailFoundSwimmer(sval);

    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }

    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }

    else if(key != "APPCAST_REQ")
      handled = false;
    
    if(!handled)
      reportRunWarning("Unhandled Mail: " + key + "=" + sval);
  }

  return(true);
}
 
//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();

  unsigned int active = 0;
  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    if(!m_swimmers[i].found)
      active++;
  }

  double now = MOOSTime();

  if((active > 0) && ((now - m_last_path_repost_time) >= 5.0)) {
    Notify("DEPLOY", "true");
    Notify("RETURN", "false");
    Notify("STATION_KEEP", "false");

    postShortestPath();

    m_last_path_repost_time = now;
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp(); 

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  
  STRING_LIST::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(biteStringX(sLine, '='));
    string value  = stripBlankEnds(sLine);

    if(param == "vname")
      m_vname = value;
  }

  RegisterVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void GenRescue::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();

  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("RESCUED_SWIMMER", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()

bool GenRescue::handleMailNewSwimmer(string str)
{
  m_total_alerts++;

  Swimmer swimmer;
  bool ok = parseSwimmerAlert(str, swimmer);

  if(!ok) {
    reportRunWarning("Bad SWIMMER_ALERT: " + str);
    return(true);
  }

  int ix = getSwimmerIndexByID(swimmer.id);

  if(ix >= 0) {
    m_duplicate_alerts++;
    postShortestPath();
    return(true);
  }

  m_swimmers.push_back(swimmer);

  string msg = "New swimmer: id=" + swimmer.id +
               ", x=" + doubleToStringX(swimmer.x, 1) +
               ", y=" + doubleToStringX(swimmer.y, 1);

  reportEvent(msg);

  postShortestPath();

  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()

bool GenRescue::handleMailFoundSwimmer(string str)
{
  string id = parseID(str);

  if(id == "") {
    reportRunWarning("Bad FOUND/RESCUED_SWIMMER: " + str);
    return(true);
  }

  int ix = getSwimmerIndexByID(id);

  if(ix < 0)
    return(true);

  if(m_swimmers[ix].found)
    return(true);

  m_swimmers[ix].found = true;

  reportEvent("Marked swimmer found: id=" + id);

  postShortestPath();

  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailRescueRegion()

bool GenRescue::handleMailRescueRegion(string str)
{
  return(true);
}

//---------------------------------------------------------
// Procedure: parseSwimmerAlert()
// Example:
//   x=-142, y=-10, id=56

bool GenRescue::parseSwimmerAlert(string str, Swimmer& swimmer)
{
  string x_str  = tokStringParse(str, "x", ',', '=');
  string y_str  = tokStringParse(str, "y", ',', '=');
  string id_str = tokStringParse(str, "id", ',', '=');

  x_str  = stripBlankEnds(x_str);
  y_str  = stripBlankEnds(y_str);
  id_str = stripBlankEnds(id_str);

  if((x_str == "") || (y_str == "") || (id_str == ""))
    return(false);

  if(!isNumber(x_str) || !isNumber(y_str))
    return(false);

  swimmer.x = atof(x_str.c_str());
  swimmer.y = atof(y_str.c_str());
  swimmer.id = id_str;
  swimmer.found = false;

  return(true);
}

//---------------------------------------------------------
// Procedure: parseID()
// Example:
//   id=48, finder=abe

string GenRescue::parseID(string str)
{
  string id = tokStringParse(str, "id", ',', '=');
  id = stripBlankEnds(id);
  return(id);
}

//---------------------------------------------------------
// Procedure: getSwimmerIndexByID()

int GenRescue::getSwimmerIndexByID(string id) const
{
  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    if(m_swimmers[i].id == id)
      return((int)(i));
  }

  return(-1);
}

//---------------------------------------------------------
// Procedure: dist()

double GenRescue::dist(double x1, double y1, double x2, double y2) const
{
  double dx = x1 - x2;
  double dy = y1 - y2;
  return(sqrt((dx * dx) + (dy * dy)));
}


//---------------------------------------------------------
// Procedure: pointSegDist()

double GenRescue::pointSegDist(double px, double py,
                               double x1, double y1,
                               double x2, double y2) const
{
  double vx = x2 - x1;
  double vy = y2 - y1;
  double wx = px - x1;
  double wy = py - y1;

  double len2 = (vx * vx) + (vy * vy);
  if(len2 <= 0.0001)
    return(dist(px, py, x1, y1));

  double t = ((wx * vx) + (wy * vy)) / len2;

  if(t < 0)
    t = 0;
  if(t > 1)
    t = 1;

  double cx = x1 + (t * vx);
  double cy = y1 + (t * vy);

  return(dist(px, py, cx, cy));
}

//---------------------------------------------------------
// Procedure: initMap()

void GenRescue::initMap()
{
  m_field_poly.clear();
  m_obstacles.clear();

  // Fixed Athens operating region from meta_vehicle.bhv:
  // core_poly = pts={-215,-2:-76,-86:-16,6:-79,4}
  m_field_poly.push_back(Point2D{-215, -2});
  m_field_poly.push_back(Point2D{ -76,-86});
  m_field_poly.push_back(Point2D{ -16,  6});
  m_field_poly.push_back(Point2D{ -79,  4});

  // Fixed Athens buoy obstacles from meta_vehicle.bhv.
  // target_radius keeps generated waypoints out of the buoy polygon.
  // segment_radius is only used as an ordering penalty. We do not
  // insert detour points, so the planner stays adaptive.
  m_obstacles.push_back(Obstacle{"buoy_1", -35.0,  -6.0, 5.0, 10.0});
  m_obstacles.push_back(Obstacle{"buoy_2", -62.5, -17.9, 5.0, 10.0});
  m_obstacles.push_back(Obstacle{"buoy_3", -95.0, -28.0, 5.0, 10.0});
}

//---------------------------------------------------------
// Procedure: pointInField()

bool GenRescue::pointInField(double x, double y) const
{
  bool inside = false;
  unsigned int n = m_field_poly.size();

  if(n < 3)
    return(true);

  for(unsigned int i = 0, j = n - 1; i < n; j = i++) {
    double xi = m_field_poly[i].x;
    double yi = m_field_poly[i].y;
    double xj = m_field_poly[j].x;
    double yj = m_field_poly[j].y;

    bool intersect = ((yi > y) != (yj > y)) &&
      (x < (xj - xi) * (y - yi) / ((yj - yi) + 0.000001) + xi);

    if(intersect)
      inside = !inside;
  }

  return(inside);
}

//---------------------------------------------------------
// Procedure: fieldBoundaryDist()

double GenRescue::fieldBoundaryDist(double x, double y) const
{
  if(m_field_poly.size() < 2)
    return(9999);

  double best = numeric_limits<double>::max();
  unsigned int n = m_field_poly.size();

  for(unsigned int i = 0; i < n; i++) {
    unsigned int j = (i + 1) % n;

    double x1 = m_field_poly[i].x;
    double y1 = m_field_poly[i].y;
    double x2 = m_field_poly[j].x;
    double y2 = m_field_poly[j].y;

    double d = pointSegDist(x, y, x1, y1, x2, y2);

    if(d < best)
      best = d;
  }

  return(best);
}

//---------------------------------------------------------
// Procedure: pointIsSafe()

bool GenRescue::pointIsSafe(double x, double y) const
{
  if(!pointInField(x, y))
    return(false);

  if(fieldBoundaryDist(x, y) < m_field_margin)
    return(false);

  for(unsigned int i = 0; i < m_obstacles.size(); i++) {
    const Obstacle& obs = m_obstacles[i];

    if(dist(x, y, obs.x, obs.y) < obs.target_radius)
      return(false);
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: segmentIsSafe()
// Used only to prefer safer next swimmers. We do not add relay
// or detour points here because that made the path less adaptive.

bool GenRescue::segmentIsSafe(double x1, double y1,
                              double x2, double y2) const
{
  if(!pointIsSafe(x2, y2))
    return(false);

  // Sample along the segment to avoid ordering choices that cut
  // outside the operation region.
  for(unsigned int i = 1; i <= 20; i++) {
    double t = (double)(i) / 20.0;
    double x = x1 + t * (x2 - x1);
    double y = y1 + t * (y2 - y1);

    if(!pointInField(x, y))
      return(false);

    if(fieldBoundaryDist(x, y) < 1.0)
      return(false);
  }

  // Penalize direct segments that cut through buoy buffers.
  // BHV_AvoidObstacleV24 will still do local avoidance.
  for(unsigned int i = 0; i < m_obstacles.size(); i++) {
    const Obstacle& obs = m_obstacles[i];

    double d = pointSegDist(obs.x, obs.y, x1, y1, x2, y2);

    if(d < obs.segment_radius)
      return(false);
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: getSafePoint()

void GenRescue::getSafePoint(double x, double y, double& sx, double& sy) const
{
  sx = x;
  sy = y;

  // If the raw swimmer point is safely inside the field and away from
  // buoys, use it directly. This preserves the adaptive path shape.
  if(pointIsSafe(sx, sy))
    return;

  // Candidate 1: move up to 4m toward the field center.
  // Rescue range is 3-5m, so this should usually still rescue.
  double vx = m_field_cx - x;
  double vy = m_field_cy - y;
  double vd = sqrt((vx * vx) + (vy * vy));

  if(vd > 0.001) {
    double cx = x + 4.0 * vx / vd;
    double cy = y + 4.0 * vy / vd;

    if(pointIsSafe(cx, cy)) {
      sx = cx;
      sy = cy;
      return;
    }
  }

  // Candidate 2: search within rescue range around the swimmer.
  // This handles random swimmers close to the boundary or near buoys.
  double best_x = sx;
  double best_y = sy;
  double best_d = numeric_limits<double>::max();

  const double pi_val = 3.14159265358979323846;

  for(double rad = 1.0; rad <= 5.0; rad += 0.5) {
    for(unsigned int k = 0; k < 72; k++) {
      double ang = (2.0 * pi_val * (double)(k)) / 72.0;
      double cx = x + rad * cos(ang);
      double cy = y + rad * sin(ang);

      if(!pointIsSafe(cx, cy))
        continue;

      double cd = dist(x, y, cx, cy);

      if(cd < best_d) {
        best_d = cd;
        best_x = cx;
        best_y = cy;
      }
    }
  }

  if(best_d < numeric_limits<double>::max()) {
    sx = best_x;
    sy = best_y;
    return;
  }

  // Last fallback: use the point 4m toward center even if margin is tight.
  if(vd > 0.001) {
    sx = x + 4.0 * vx / vd;
    sy = y + 4.0 * vy / vd;
  }
}

//---------------------------------------------------------
// Procedure: postShortestPath()

void GenRescue::postShortestPath()
{
  if(!m_nav_x_set || !m_nav_y_set) {
    reportRunWarning("Cannot post path yet: NAV_X/NAV_Y not received.");
    return;
  }

  unsigned int unvisited_count = 0;
  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    if(!m_swimmers[i].found)
      unvisited_count++;
  }

  if(unvisited_count == 0) {
    postNullPath();
    return;
  }

  vector<bool> used;
  used.resize(m_swimmers.size(), false);

  XYSegList segl;

  double curr_x = m_nav_x;
  double curr_y = m_nav_y;

  // Start the freshly posted path at Abe's current position.
  // This makes each periodic update visibly adaptive.
  segl.add_vertex(curr_x, curr_y);

  for(unsigned int step = 0; step < unvisited_count; step++) {
    double best_dist = numeric_limits<double>::max();
    int best_ix = -1;

    for(unsigned int i = 0; i < m_swimmers.size(); i++) {
      if(used[i])
        continue;

      if(m_swimmers[i].found)
        continue;

      double safe_x, safe_y;
      getSafePoint(m_swimmers[i].x, m_swimmers[i].y, safe_x, safe_y);

      double d = dist(curr_x, curr_y, safe_x, safe_y);

      if(!segmentIsSafe(curr_x, curr_y, safe_x, safe_y))
        d += 500.0;

      if(d < best_dist) {
        best_dist = d;
        best_ix = (int)(i);
      }
    }

    if(best_ix < 0)
      break;

    double safe_x, safe_y;
    getSafePoint(m_swimmers[best_ix].x, m_swimmers[best_ix].y, safe_x, safe_y);

    segl.add_vertex(safe_x, safe_y);
    used[best_ix] = true;

    curr_x = safe_x;
    curr_y = safe_y;
  }

  string label = "rescue_path";
  if(m_vname != "")
    label += "_" + m_vname;

  segl.set_label(label);
  segl.set_color("edge", "yellow");
  segl.set_color("vertex", "dodger_blue");
  segl.set_param("edge_size", "2");
  segl.set_param("vertex_size", "4");

  m_path = segl;

  Notify("VIEW_SEGLIST", m_path.get_spec());

  Notify("DEPLOY", "true");
  Notify("RETURN", "false");
  Notify("STATION_KEEP", "false");

  string update_str = "points = " + m_path.get_spec_pts();

  Notify("SURVEY_UPDATE", update_str);
  m_last_update_str = update_str;
  m_paths_posted++;
  reportEvent("SURVEY_UPDATE=" + update_str);
}

//---------------------------------------------------------
// Procedure: postNullPath()

void GenRescue::postNullPath()
{
  if(!m_nav_x_set || !m_nav_y_set)
    return;

  XYSegList segl;
  segl.add_vertex(m_nav_x, m_nav_y);

  string label = "rescue_path";
  if(m_vname != "")
    label += "_" + m_vname;

  segl.set_label(label);

  Notify("VIEW_SEGLIST", segl.get_spec());

  string update_str = "points = " + segl.get_spec_pts();

  if(update_str != m_last_update_str) {
    Notify("SURVEY_UPDATE", update_str);
    m_last_update_str = update_str;
    reportEvent("SURVEY_UPDATE=" + update_str);
}
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "pGenRescue swimmer-driven planner" << endl;
  m_msgs << "---------------------------------" << endl;
  m_msgs << "Vehicle name: " << m_vname << endl;
  m_msgs << "NAV_X/Y set:  " << boolToString(m_nav_x_set && m_nav_y_set) << endl;
  m_msgs << "NAV_X:        " << doubleToStringX(m_nav_x, 2) << endl;
  m_msgs << "NAV_Y:        " << doubleToStringX(m_nav_y, 2) << endl;
  m_msgs << endl;

  m_msgs << "Total alerts received: " << m_total_alerts << endl;
  m_msgs << "Duplicate alerts:      " << m_duplicate_alerts << endl;
  m_msgs << "Known swimmers:        " << m_swimmers.size() << endl;
  m_msgs << "Paths posted:          " << m_paths_posted << endl;
  m_msgs << endl;

  unsigned int active = 0;
  unsigned int found = 0;

  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    if(m_swimmers[i].found)
      found++;
    else
      active++;
  }

  m_msgs << "Active swimmers:       " << active << endl;
  m_msgs << "Found swimmers:        " << found << endl;
  m_msgs << endl;

  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    m_msgs << "  id=" << m_swimmers[i].id
           << ", x=" << doubleToStringX(m_swimmers[i].x, 1)
           << ", y=" << doubleToStringX(m_swimmers[i].y, 1)
           << ", found=" << boolToString(m_swimmers[i].found)
           << endl;
  }

  return(true);
}