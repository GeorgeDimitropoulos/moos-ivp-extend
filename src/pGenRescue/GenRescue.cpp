/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: Summer 2026                                     */
/************************************************************/

#include <cmath>
#include <cstdlib>
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
  m_planner_mode = "cluster";

  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = false;
  m_nav_y_set = false;

  m_home_x = 0;
  m_home_y = 0;
  m_home_set = false;

  m_contact.name = "";
  m_contact.x = 0;
  m_contact.y = 0;
  m_contact.heading = 0;
  m_contact.speed = 0;
  m_contact.set = false;

  m_total_alerts = 0;
  m_duplicate_alerts = 0;
  m_paths_posted = 0;
  m_node_reports = 0;
  m_last_path_repost_time = 0;

  // Approximate Athens field center. Used only to pull unsafe swimmer
  // targets inward by a few meters.
  m_field_cx = -96.5;
  m_field_cy = -19.5;

  // Keep this small. A large margin can make boundary swimmers unreachable.
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

    if((key == "SWIMMER_ALERT") || (key.find("SWIMMER_ALERT_") == 0))
      handled = handleMailNewSwimmer(sval);

    else if((key == "FOUND_SWIMMER") || (key == "RESCUED_SWIMMER"))
      handled = handleMailFoundSwimmer(sval);

    else if(key == "NODE_REPORT")
      handled = handleMailNodeReport(sval);

    else if(key == "RESCUE_REGION")
      handled = handleMailRescueRegion(sval);

    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;

      if(m_nav_x_set && m_nav_y_set && !m_home_set) {
        m_home_x = m_nav_x;
        m_home_y = m_nav_y;
        m_home_set = true;
      }
    }

    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;

      if(m_nav_x_set && m_nav_y_set && !m_home_set) {
        m_home_x = m_nav_x;
        m_home_y = m_nav_y;
        m_home_set = true;
      }
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
    else if(param == "planner_mode")
      m_planner_mode = tolower(value);
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
  Register("SWIMMER_ALERT_ABE", 0);
  Register("SWIMMER_ALERT_BEN", 0);
  Register("SWIMMER_ALERT_ASHA", 0);
  Register("SWIMMER_ALERT_BRAVO", 0);

  Register("FOUND_SWIMMER", 0);
  Register("RESCUED_SWIMMER", 0);

  Register("NODE_REPORT", 0);
  Register("RESCUE_REGION", 0);

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

  reportEvent("Dropped swimmer from plan: id=" + id + ", msg=" + str);

  postShortestPath();

  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailNodeReport()

bool GenRescue::handleMailNodeReport(string str)
{
  ContactInfo contact;
  bool ok = parseNodeReport(str, contact);

  if(!ok)
    return(true);

  if(contact.name == "")
    return(true);

  if((m_vname != "") && (contact.name == m_vname))
    return(true);

  m_contact = contact;
  m_contact.set = true;
  m_node_reports++;

  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailRescueRegion()

bool GenRescue::handleMailRescueRegion(string str)
{
  bool ok = parseRescueRegion(str);

  if(ok)
    reportEvent("Updated dynamic RESCUE_REGION with " +
                intToString((int)(m_field_poly.size())) + " vertices.");
  else
    reportRunWarning("Could not parse RESCUE_REGION: " + str);

  return(true);
}

//---------------------------------------------------------
// Procedure: parseRescueRegion()
// Expected examples:
//   pts={-215,-2:-76,-86:-16,6:-79,4}
//   label=...,pts={-215,-2:-76,-86:-16,6:-79,4},...

bool GenRescue::parseRescueRegion(string str)
{
  string pts = str;

  size_t p0 = pts.find("pts={");
  if(p0 != string::npos) {
    p0 += 5;
    size_t p1 = pts.find("}", p0);
    if(p1 == string::npos)
      return(false);
    pts = pts.substr(p0, p1 - p0);
  }
  else {
    size_t b0 = pts.find("{");
    size_t b1 = pts.find("}");
    if((b0 != string::npos) && (b1 != string::npos) && (b1 > b0))
      pts = pts.substr(b0 + 1, b1 - b0 - 1);
  }

  vector<Point2D> new_poly;

  while(pts != "") {
    string pair = biteStringX(pts, ':');
    pair = stripBlankEnds(pair);

    if(pair == "")
      continue;

    string x_str = biteStringX(pair, ',');
    string y_str = pair;

    x_str = stripBlankEnds(x_str);
    y_str = stripBlankEnds(y_str);

    if(!isNumber(x_str) || !isNumber(y_str))
      return(false);

    Point2D point;
    point.x = atof(x_str.c_str());
    point.y = atof(y_str.c_str());
    new_poly.push_back(point);
  }

  if(new_poly.size() < 3)
    return(false);

  m_field_poly = new_poly;
  updateFieldCentroid();

  return(true);
}

//---------------------------------------------------------
// Procedure: updateFieldCentroid()

void GenRescue::updateFieldCentroid()
{
  if(m_field_poly.size() == 0)
    return;

  double sx = 0;
  double sy = 0;

  for(unsigned int i = 0; i < m_field_poly.size(); i++) {
    sx += m_field_poly[i].x;
    sy += m_field_poly[i].y;
  }

  m_field_cx = sx / (double)(m_field_poly.size());
  m_field_cy = sy / (double)(m_field_poly.size());
}


//---------------------------------------------------------
// Procedure: parseSwimmerAlert()

bool GenRescue::parseSwimmerAlert(string str, Swimmer& swimmer)
{
  string x_str  = tokStringParse(str, "x", ',', '=');
  string y_str  = tokStringParse(str, "y", ',', '=');
  string id_str = tokStringParse(str, "id", ',', '=');

  if(id_str == "")
    id_str = tokStringParse(str, "name", ',', '=');

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

string GenRescue::parseID(string str)
{
  string id = tokStringParse(str, "id", ',', '=');
  id = stripBlankEnds(id);
  return(id);
}

//---------------------------------------------------------
// Procedure: parseNodeReport()

bool GenRescue::parseNodeReport(string str, ContactInfo& contact) const
{
  string name_str = tokStringParse(str, "NAME", ',', '=');
  string x_str    = tokStringParse(str, "X", ',', '=');
  string y_str    = tokStringParse(str, "Y", ',', '=');
  string hdg_str  = tokStringParse(str, "HDG", ',', '=');
  string spd_str  = tokStringParse(str, "SPD", ',', '=');

  name_str = stripBlankEnds(name_str);
  x_str    = stripBlankEnds(x_str);
  y_str    = stripBlankEnds(y_str);
  hdg_str  = stripBlankEnds(hdg_str);
  spd_str  = stripBlankEnds(spd_str);

  if((name_str == "") || (x_str == "") || (y_str == ""))
    return(false);

  if(!isNumber(x_str) || !isNumber(y_str))
    return(false);

  contact.name = name_str;
  contact.x = atof(x_str.c_str());
  contact.y = atof(y_str.c_str());

  if(isNumber(hdg_str))
    contact.heading = atof(hdg_str.c_str());
  else
    contact.heading = 0;

  if(isNumber(spd_str))
    contact.speed = atof(spd_str.c_str());
  else
    contact.speed = 0;

  contact.set = true;

  return(true);
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
// Procedure: bearingTo()
// Compass bearing: 0 north, 90 east.

double GenRescue::bearingTo(double x1, double y1, double x2, double y2) const
{
  double dx = x2 - x1;
  double dy = y2 - y1;

  double ang = atan2(dx, dy) * 180.0 / 3.14159265358979323846;

  if(ang < 0)
    ang += 360.0;

  return(ang);
}

//---------------------------------------------------------
// Procedure: angleDiff()

double GenRescue::angleDiff(double a1, double a2) const
{
  double diff = fabs(a1 - a2);

  while(diff > 360.0)
    diff -= 360.0;

  if(diff > 180.0)
    diff = 360.0 - diff;

  return(diff);
}

//---------------------------------------------------------
// Procedure: initMap()

void GenRescue::initMap()
{
  m_field_poly.clear();
  m_obstacles.clear();

  // Fixed Athens core region from meta_vehicle.bhv:
  // core_poly = pts={-215,-2:-76,-86:-16,6:-79,4}
  m_field_poly.push_back(Point2D{-215, -2});
  m_field_poly.push_back(Point2D{ -76,-86});
  m_field_poly.push_back(Point2D{ -16,  6});
  m_field_poly.push_back(Point2D{ -79,  4});

  // Fixed Athens buoy obstacles.
  // target_radius avoids posting an actual waypoint inside/near a buoy.
  // segment_radius is only a scoring penalty; the helm still performs
  // local obstacle avoidance.
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

    double d = pointSegDist(x, y,
                            m_field_poly[i].x, m_field_poly[i].y,
                            m_field_poly[j].x, m_field_poly[j].y);

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
// Used as a scoring penalty, not as a hard routing system.
// Do not add detour points here: local obstacle avoidance handles that.

bool GenRescue::segmentIsSafe(double x1, double y1,
                              double x2, double y2) const
{
  if(!pointIsSafe(x2, y2))
    return(false);

  for(unsigned int i = 1; i <= 20; i++) {
    double t = (double)(i) / 20.0;
    double x = x1 + t * (x2 - x1);
    double y = y1 + t * (y2 - y1);

    if(!pointInField(x, y))
      return(false);

    if(fieldBoundaryDist(x, y) < m_field_margin)
      return(false);
  }

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

  if(pointIsSafe(sx, sy))
    return;

  double vx = m_field_cx - x;
  double vy = m_field_cy - y;
  double vd = sqrt((vx * vx) + (vy * vy));

  // Try moving toward center, while staying within rescue range.
  if(vd > 0.001) {
    double cx = x + 4.0 * vx / vd;
    double cy = y + 4.0 * vy / vd;

    if(pointIsSafe(cx, cy)) {
      sx = cx;
      sy = cy;
      return;
    }
  }

  // Search within approximately rescue distance around the swimmer.
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

  // Final fallback. May be tight, but usually avoids the worst cases.
  if(vd > 0.001) {
    sx = x + 4.0 * vx / vd;
    sy = y + 4.0 * vy / vd;
  }
}

//---------------------------------------------------------
// Procedure: nearestActiveNeighborDist()
// Uses safe points so the lookahead matches the real posted path.

double GenRescue::nearestActiveNeighborDist(unsigned int ix,
                                            const vector<bool>& used) const
{
  double sx1, sy1;
  getSafePoint(m_swimmers[ix].x, m_swimmers[ix].y, sx1, sy1);

  double best = numeric_limits<double>::max();

  for(unsigned int j = 0; j < m_swimmers.size(); j++) {
    if(j == ix)
      continue;

    if(j < used.size() && used[j])
      continue;

    if(m_swimmers[j].found)
      continue;

    double sx2, sy2;
    getSafePoint(m_swimmers[j].x, m_swimmers[j].y, sx2, sy2);

    double d = dist(sx1, sy1, sx2, sy2);

    if(d < best)
      best = d;
  }

  if(best == numeric_limits<double>::max())
    return(0);

  return(best);
}

//---------------------------------------------------------
// Procedure: countNearbyActive()

unsigned int GenRescue::countNearbyActive(unsigned int ix,
                                          const vector<bool>& used,
                                          double radius) const
{
  unsigned int count = 0;

  for(unsigned int j = 0; j < m_swimmers.size(); j++) {
    if(j == ix)
      continue;

    if(j < used.size() && used[j])
      continue;

    if(m_swimmers[j].found)
      continue;

    double d = dist(m_swimmers[ix].x, m_swimmers[ix].y,
                    m_swimmers[j].x, m_swimmers[j].y);

    if(d <= radius)
      count++;
  }

  return(count);
}

//---------------------------------------------------------
// Procedure: opponentRank()

unsigned int GenRescue::opponentRank(unsigned int ix,
                                     const vector<bool>& used) const
{
  if(!m_contact.set)
    return(999);

  unsigned int rank = 1;

  double candidate_dist = dist(m_contact.x, m_contact.y,
                               m_swimmers[ix].x, m_swimmers[ix].y);

  for(unsigned int j = 0; j < m_swimmers.size(); j++) {
    if(j == ix)
      continue;

    if(j < used.size() && used[j])
      continue;

    if(m_swimmers[j].found)
      continue;

    double d = dist(m_contact.x, m_contact.y,
                    m_swimmers[j].x, m_swimmers[j].y);

    if(d < candidate_dist)
      rank++;
  }

  return(rank);
}

//---------------------------------------------------------
// Procedure: candidateScore()
// Lower is better.
// Combines safe travel distance, cluster lookahead, segment safety,
// and opponent-aware concession.

double GenRescue::candidateScore(unsigned int ix,
                                 double curr_x,
                                 double curr_y,
                                 const vector<bool>& used) const
{
  double target_x, target_y;
  getSafePoint(m_swimmers[ix].x, m_swimmers[ix].y, target_x, target_y);

  double self_dist = dist(curr_x, curr_y, target_x, target_y);
  double next_dist = nearestActiveNeighborDist(ix, used);

  unsigned int cluster_count = countNearbyActive(ix, used, 20.0);

  double score = self_dist + (0.65 * next_dist);

  score -= 4.0 * (double)(cluster_count);
  bool segment_safe = segmentIsSafe(curr_x, curr_y, target_x, target_y);

  if(!segment_safe)
    score += 5000.0;

  // Baseline comparison mode:
  // one-leg greedy, but still with safety penalties.
  if((m_planner_mode == "greedy") || (m_planner_mode == "baseline")) {
    double greedy_score = self_dist;

    if(!segment_safe)
      greedy_score += 500.0;

    return(greedy_score);
  }

  // Cluster-only comparison mode:
  // use two-leg lookahead and safety, but no opponent penalties.
  if(m_planner_mode == "cluster")
    return(score);

  if(m_contact.set) {
    double opp_dist = dist(m_contact.x, m_contact.y,
                           m_swimmers[ix].x, m_swimmers[ix].y);

    unsigned int rank = opponentRank(ix, used);

    if(opp_dist + 5.0 < self_dist)
      score += 15.0 + (0.25 * (self_dist - opp_dist));

    if(rank == 1)
      score += 20.0;
    else if(rank == 2)
      score += 5.0;

    if(opp_dist < 15.0)
      score += 15.0;
    else if(opp_dist < 25.0)
      score += 5.0;

    double brg = bearingTo(m_contact.x, m_contact.y,
                           m_swimmers[ix].x, m_swimmers[ix].y);

    double hdiff = angleDiff(m_contact.heading, brg);

    if((opp_dist < 40.0) && (hdiff < 45.0)) {
      double factor = (45.0 - hdiff) / 45.0;
      score += 10.0 * factor;
    }

    if(self_dist + 8.0 < opp_dist)
      score -= 25.0;
  }

  return(score);
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

  // Do not add current vehicle position as a survey waypoint.
  // Planning still starts from curr_x/curr_y, but the posted waypoint
  // list contains only rescue targets inside the active region.

  for(unsigned int step = 0; step < unvisited_count; step++) {
    double best_score = numeric_limits<double>::max();
    int best_ix = -1;

    for(unsigned int i = 0; i < m_swimmers.size(); i++) {
      if(used[i])
        continue;

      if(m_swimmers[i].found)
        continue;

      double score = candidateScore(i, curr_x, curr_y, used);

      if(score < best_score) {
        best_score = score;
        best_ix = (int)(i);
      }
    }

    if(best_ix < 0)
      break;

    double safe_x, safe_y;
    getSafePoint(m_swimmers[best_ix].x, m_swimmers[best_ix].y,
                 safe_x, safe_y);

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

  double home_x = m_nav_x;
  double home_y = m_nav_y;

  if(m_home_set) {
    home_x = m_home_x;
    home_y = m_home_y;
  }

  double safe_home_x, safe_home_y;
  getSafePoint(home_x, home_y, safe_home_x, safe_home_y);

  XYSegList segl;
  segl.add_vertex(safe_home_x, safe_home_y);

  string label = "rescue_home_path";
  if(m_vname != "")
    label += "_" + m_vname;

  segl.set_label(label);
  segl.set_color("edge", "orange");
  segl.set_color("vertex", "orange");
  segl.set_param("edge_size", "2");
  segl.set_param("vertex_size", "5");

  Notify("VIEW_SEGLIST", segl.get_spec());

  string update_str = "points = " + segl.get_spec_pts();

  Notify("SURVEY_UPDATE", update_str);
  m_last_update_str = update_str;
  reportEvent("Final home SURVEY_UPDATE=" + update_str);

  // Return home using the survey behavior, not waypoint_return.
  // waypoint_return may leave the active rescue OpRegion.
  Notify("RETURN", "false");
  Notify("DEPLOY", "true");
  Notify("STATION_KEEP", "false");
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "pGenRescue safe adversarial cluster planner" << endl;
  m_msgs << "------------------------------------------" << endl;
  m_msgs << "Vehicle name: " << m_vname << endl;
  m_msgs << "Planner mode: " << m_planner_mode << endl;
  m_msgs << "NAV_X/Y set:  " << boolToString(m_nav_x_set && m_nav_y_set) << endl;
  m_msgs << "NAV_X:        " << doubleToStringX(m_nav_x, 2) << endl;
  m_msgs << "NAV_Y:        " << doubleToStringX(m_nav_y, 2) << endl;
  m_msgs << endl;

  m_msgs << "Total alerts received: " << m_total_alerts << endl;
  m_msgs << "Duplicate alerts:      " << m_duplicate_alerts << endl;
  m_msgs << "Known swimmers:        " << m_swimmers.size() << endl;
  m_msgs << "Paths posted:          " << m_paths_posted << endl;
  m_msgs << "Node reports:          " << m_node_reports << endl;
  m_msgs << endl;

  if(m_contact.set) {
    m_msgs << "Adversary:             " << m_contact.name << endl;
    m_msgs << "Adv X/Y:               "
           << doubleToStringX(m_contact.x, 1) << ", "
           << doubleToStringX(m_contact.y, 1) << endl;
    m_msgs << "Adv heading/speed:     "
           << doubleToStringX(m_contact.heading, 1) << " / "
           << doubleToStringX(m_contact.speed, 2) << endl;
  } else {
    m_msgs << "Adversary:             none yet" << endl;
  }

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

  vector<bool> used;
  used.resize(m_swimmers.size(), false);

  for(unsigned int i = 0; i < m_swimmers.size(); i++) {
    double safe_x, safe_y;
    getSafePoint(m_swimmers[i].x, m_swimmers[i].y, safe_x, safe_y);

    m_msgs << "  id=" << m_swimmers[i].id
           << ", raw=(" << doubleToStringX(m_swimmers[i].x, 1)
           << "," << doubleToStringX(m_swimmers[i].y, 1) << ")"
           << ", safe=(" << doubleToStringX(safe_x, 1)
           << "," << doubleToStringX(safe_y, 1) << ")"
           << ", found=" << boolToString(m_swimmers[i].found);

    if(!m_swimmers[i].found && m_nav_x_set && m_nav_y_set)
      m_msgs << ", score=" << doubleToStringX(candidateScore(i, m_nav_x, m_nav_y, used), 1);

    if(m_contact.set && !m_swimmers[i].found)
      m_msgs << ", opp_rank=" << opponentRank(i, used);

    m_msgs << endl;
  }

  return(true);
}
