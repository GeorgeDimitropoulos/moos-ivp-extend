#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>
#include <utility>
#include <limits>

#include "BHV_Scout.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "BuildUtils.h"
#include "GeomUtils.h"
#include "ZAIC_PEAK.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"

using namespace std;

//-----------------------------------------------------------
// Constructor()

BHV_Scout::BHV_Scout(IvPDomain gdomain) : 
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "scout");
 
  m_osx  = 0;
  m_osy  = 0;

  m_ptx = 0;
  m_pty = 0;
  m_pt_set = false;

  m_sweep_ix = 0;
  m_sweep_ready = false;

  m_tmate_x = 0;
  m_tmate_y = 0;
  m_tmate_set = false;

  m_desired_speed  = 1.0; 
  m_capture_radius = 10.0;

  m_sweep_spacing = 22.0;
  m_known_avoid_radius = 24.0;
  m_tmate_avoid_radius = 35.0;

  srand(time(NULL));
  
  addInfoVars("NAV_X, NAV_Y");
  addInfoVars("RESCUE_REGION");
  addInfoVars("SCOUTED_SWIMMER");
  addInfoVars("SWIMMER_ALERT");
  addInfoVars("NODE_REPORT");
}

//-----------------------------------------------------------
// Procedure: setParam()

bool BHV_Scout::setParam(string param, string val) 
{
  param = tolower(param);
  
  bool handled = true;

  if(param == "capture_radius")
    handled = setPosDoubleOnString(m_capture_radius, val);
  else if(param == "desired_speed")
    handled = setPosDoubleOnString(m_desired_speed, val);
  else if(param == "sweep_spacing")
    handled = setPosDoubleOnString(m_sweep_spacing, val);
  else if(param == "known_avoid_radius")
    handled = setPosDoubleOnString(m_known_avoid_radius, val);
  else if(param == "tmate_avoid_radius")
    handled = setPosDoubleOnString(m_tmate_avoid_radius, val);
  else if(param == "tmate")
    handled = setNonWhiteVarOnString(m_tmate, val);
  else
    handled = false;
  
  return(handled);
}

//-----------------------------------------------------------
// Procedure: onEveryState()

void BHV_Scout::onEveryState(string str) 
{
  if(getBufferVarUpdated("SWIMMER_ALERT")) {
    string report = getBufferStringVal("SWIMMER_ALERT");

    string id;
    double x = 0;
    double y = 0;

    if(parseSwimmerAlert(report, id, x, y))
      addKnownSwimmer(id, x, y);
  }

  if(getBufferVarUpdated("NODE_REPORT")) {
    string report = getBufferStringVal("NODE_REPORT");
    handleNodeReport(report);
  }

  if(!getBufferVarUpdated("SCOUTED_SWIMMER"))
    return;

  string report = getBufferStringVal("SCOUTED_SWIMMER");
  if(report == "")
    return;

  if(m_tmate == "") {
    postWMessage("Mandatory Teammate name is null");
    return;
  }

  string id;
  double x = 0;
  double y = 0;

  if(parseSwimmerAlert(report, id, x, y))
    addKnownSwimmer(id, x, y);

  postOffboardMessage(m_tmate, "SWIMMER_ALERT", report);

  postEventMessage("Forwarded scouted swimmer to " + m_tmate +
                   ": " + report);
}

//-----------------------------------------------------------
// Procedure: onIdleState()

void BHV_Scout::onIdleState() 
{
  m_curr_time = getBufferCurrTime();
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction *BHV_Scout::onRunState() 
{
  bool ok1, ok2;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);

  if(!ok1 || !ok2) {
    postWMessage("No ownship X/Y info in info_buffer.");
    return(0);
  }
  
  updateScoutPoint();

  if(!m_pt_set)
    return(0);

  double dist_to_point = hypot((m_ptx - m_osx), (m_pty - m_osy));

  if(dist_to_point <= m_capture_radius) {
    m_pt_set = false;
    postViewPoint(false);
    return(0);
  }

  postViewPoint(true);

  IvPFunction *ipf = buildFunction();

  if(ipf == 0) 
    postWMessage("Problem Creating the IvP Function");
  
  return(ipf);
}

//-----------------------------------------------------------
// Procedure: updateScoutPoint()

void BHV_Scout::updateScoutPoint()
{
  if(m_pt_set)
    return;

  string region_str = getBufferStringVal("RESCUE_REGION");

  if(region_str == "") {
    postWMessage("Unknown RESCUE_REGION");
    return;
  }

  postRetractWMessage("Unknown RESCUE_REGION");

  XYPolygon region = string2Poly(region_str);

  if(!region.is_convex()) {
    postWMessage("Badly formed RESCUE_REGION");
    return;
  }

  m_rescue_region = region;

  if(!m_sweep_ready) {
    bool ok = generateSweepPoints();

    if(!ok) {
      postWMessage("Unable to generate sweep points");
      return;
    }

    postEventMessage("Generated " + intToString((int)m_sweep_points.size()) +
                     " smart lawnmower scout points");
  }

  if(m_sweep_points.size() == 0) {
    postWMessage("No sweep points available");
    return;
  }

  chooseBestSweepPoint();
}

//-----------------------------------------------------------
// Procedure: generateSweepPoints()

bool BHV_Scout::generateSweepPoints()
{
  m_sweep_points.clear();

  unsigned int vcount = m_rescue_region.size();

  if(vcount < 3)
    return(false);

  double min_x = m_rescue_region.get_vx(0);
  double max_x = m_rescue_region.get_vx(0);
  double min_y = m_rescue_region.get_vy(0);
  double max_y = m_rescue_region.get_vy(0);

  for(unsigned int i = 1; i < vcount; i++) {
    double x = m_rescue_region.get_vx(i);
    double y = m_rescue_region.get_vy(i);

    if(x < min_x)
      min_x = x;
    if(x > max_x)
      max_x = x;
    if(y < min_y)
      min_y = y;
    if(y > max_y)
      max_y = y;
  }

  double spacing = m_sweep_spacing;

  if(spacing < 5.0)
    spacing = 5.0;

  bool left_to_right = true;

  for(double y = min_y + spacing / 2.0;
      y <= max_y - spacing / 2.0;
      y += spacing) {

    vector<pair<double,double> > sweep_row;

    for(double x = min_x + spacing / 2.0;
        x <= max_x - spacing / 2.0;
        x += spacing) {

      if(pointInRegion(x, y))
        sweep_row.push_back(pair<double,double>(x, y));
    }

    if(sweep_row.size() == 0)
      continue;

    if(left_to_right) {
      for(unsigned int i = 0; i < sweep_row.size(); i++)
        m_sweep_points.push_back(sweep_row[i]);
    }
    else {
      for(int i = (int)sweep_row.size() - 1; i >= 0; i--)
        m_sweep_points.push_back(sweep_row[i]);
    }

    left_to_right = !left_to_right;
  }

  if(m_sweep_points.size() == 0) {
    double ptx = 0;
    double pty = 0;

    bool ok = randPointInPoly(m_rescue_region, ptx, pty);

    if(!ok)
      return(false);

    m_sweep_points.push_back(pair<double,double>(ptx, pty));
  }

  m_sweep_ix = 0;
  m_sweep_ready = true;

  return(true);
}

//-----------------------------------------------------------
// Procedure: chooseBestSweepPoint()

void BHV_Scout::chooseBestSweepPoint()
{
  if(m_sweep_points.size() == 0)
    return;

  if(m_sweep_ix >= m_sweep_points.size())
    m_sweep_ix = 0;

  unsigned int best_ix = m_sweep_ix;
  double best_score = numeric_limits<double>::max();

  for(unsigned int k = 0; k < m_sweep_points.size(); k++) {
    unsigned int ix = (m_sweep_ix + k) % m_sweep_points.size();

    double x = m_sweep_points[ix].first;
    double y = m_sweep_points[ix].second;

    double score = candidatePointScore(x, y, k);

    if(score < best_score) {
      best_score = score;
      best_ix = ix;
    }
  }

  m_ptx = m_sweep_points[best_ix].first;
  m_pty = m_sweep_points[best_ix].second;
  m_pt_set = true;

  string msg = "New smart sweep pt[" + intToString((int)best_ix) + "]: " +
               doubleToStringX(m_ptx, 1) + "," +
               doubleToStringX(m_pty, 1) +
               ", score=" + doubleToStringX(best_score, 1);

  postEventMessage(msg);

  m_sweep_ix = best_ix + 1;

  if(m_sweep_ix >= m_sweep_points.size())
    m_sweep_ix = 0;
}

//-----------------------------------------------------------
// Procedure: candidatePointScore()

double BHV_Scout::candidatePointScore(double x, double y,
                                      unsigned int order_offset) const
{
  double score = (double)order_offset;

  for(unsigned int i = 0; i < m_known_swimmers.size(); i++) {
    double sx = m_known_swimmers[i].first;
    double sy = m_known_swimmers[i].second;

    double d = hypot(x - sx, y - sy);

    if(d < m_known_avoid_radius)
      score += 100.0 + ((m_known_avoid_radius - d) * 10.0);
  }

  if(m_tmate_set) {
    double d = hypot(x - m_tmate_x, y - m_tmate_y);

    if(d < m_tmate_avoid_radius)
      score += 80.0 + ((m_tmate_avoid_radius - d) * 5.0);
  }

  return(score);
}

//-----------------------------------------------------------
// Procedure: parseSwimmerAlert()

bool BHV_Scout::parseSwimmerAlert(string report,
                                  string& id,
                                  double& x,
                                  double& y) const
{
  string x_str  = tokStringParse(report, "x", ',', '=');
  string y_str  = tokStringParse(report, "y", ',', '=');
  string id_str = tokStringParse(report, "id", ',', '=');

  if(id_str == "")
    id_str = tokStringParse(report, "name", ',', '=');

  x_str  = stripBlankEnds(x_str);
  y_str  = stripBlankEnds(y_str);
  id_str = stripBlankEnds(id_str);

  if((x_str == "") || (y_str == "") || (id_str == ""))
    return(false);

  if(!isNumber(x_str) || !isNumber(y_str))
    return(false);

  x = atof(x_str.c_str());
  y = atof(y_str.c_str());
  id = id_str;

  return(true);
}

//-----------------------------------------------------------
// Procedure: addKnownSwimmer()

bool BHV_Scout::addKnownSwimmer(string id, double x, double y)
{
  if(id == "")
    return(false);

  for(unsigned int i = 0; i < m_known_ids.size(); i++) {
    if(m_known_ids[i] == id)
      return(false);
  }

  m_known_ids.push_back(id);
  m_known_swimmers.push_back(pair<double,double>(x, y));

  postEventMessage("Scout learned known swimmer id=" + id +
                   ", x=" + doubleToStringX(x, 1) +
                   ", y=" + doubleToStringX(y, 1));

  return(true);
}

//-----------------------------------------------------------
// Procedure: handleNodeReport()

void BHV_Scout::handleNodeReport(string report)
{
  if(m_tmate == "")
    return;

  string name = tokStringParse(report, "NAME", ',', '=');
  string xstr = tokStringParse(report, "X", ',', '=');
  string ystr = tokStringParse(report, "Y", ',', '=');

  name = stripBlankEnds(name);
  xstr = stripBlankEnds(xstr);
  ystr = stripBlankEnds(ystr);

  if(name != m_tmate)
    return;

  if(!isNumber(xstr) || !isNumber(ystr))
    return;

  m_tmate_x = atof(xstr.c_str());
  m_tmate_y = atof(ystr.c_str());
  m_tmate_set = true;
}

//-----------------------------------------------------------
// Procedure: pointInRegion()

bool BHV_Scout::pointInRegion(double x, double y) const
{
  return(m_rescue_region.contains(x, y));
}

//-----------------------------------------------------------
// Procedure: postViewPoint()

void BHV_Scout::postViewPoint(bool viewable) 
{
  XYPoint pt(m_ptx, m_pty);

  pt.set_vertex_size(5);
  pt.set_vertex_color("orange");
  pt.set_label(m_us_name + "'s next waypoint");
  
  string point_spec;

  if(viewable)
    point_spec = pt.get_spec("active=true");
  else
    point_spec = pt.get_spec("active=false");

  postMessage("VIEW_POINT", point_spec);
}

//-----------------------------------------------------------
// Procedure: buildFunction()

IvPFunction *BHV_Scout::buildFunction() 
{
  if(!m_pt_set)
    return(0);
  
  ZAIC_PEAK spd_zaic(m_domain, "speed");

  spd_zaic.setSummit(m_desired_speed);
  spd_zaic.setPeakWidth(0.5);
  spd_zaic.setBaseWidth(1.0);
  spd_zaic.setSummitDelta(0.8);  

  if(spd_zaic.stateOK() == false) {
    string warnings = "Speed ZAIC problems " + spd_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }
  
  double rel_ang_to_wpt = relAng(m_osx, m_osy, m_ptx, m_pty);

  ZAIC_PEAK crs_zaic(m_domain, "course");

  crs_zaic.setSummit(rel_ang_to_wpt);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180.0);
  crs_zaic.setSummitDelta(0);  
  crs_zaic.setValueWrap(true);

  if(crs_zaic.stateOK() == false) {
    string warnings = "Course ZAIC problems " + crs_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }

  IvPFunction *spd_ipf = spd_zaic.extractIvPFunction();
  IvPFunction *crs_ipf = crs_zaic.extractIvPFunction();

  OF_Coupler coupler;
  IvPFunction *ivp_function = coupler.couple(crs_ipf, spd_ipf, 50, 50);

  return(ivp_function);
}
