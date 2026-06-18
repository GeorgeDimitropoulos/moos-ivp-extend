/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT / 2.680                                     */
/*    FILE: PointAssign.cpp                                 */
/*    DATE: June 2026                                       */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include <cctype>
#include "MBUtils.h"
#include "ACTable.h"
#include "XYPoint.h"
#include "PointAssign.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

PointAssign::PointAssign()
{
  m_assign_by_region = false;
  m_distributing = false;
  m_region_mid_x = 87.5;

  m_total_received = 0;
  m_total_assigned = 0;
  m_outgoing_ix = 0;
}

//---------------------------------------------------------
// Destructor()

PointAssign::~PointAssign()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool PointAssign::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

    if(key == "VISIT_POINT") {
      handleVisitPoint(msg.GetString());
    }
    else if(key != "APPCAST_REQ") {
      reportRunWarning("Unhandled Mail: " + key);
    }
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool PointAssign::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool PointAssign::Iterate()
{
  AppCastingMOOSApp::Iterate();

  postNextQueuedPoint();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool PointAssign::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);

  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = tolower(biteStringX(line, '='));
    string value = stripBlankEnds(line);

    bool handled = false;

    if(param == "vname") {
      value = tolower(value);
      m_vnames.push_back(value);
      handled = true;
    }
    else if(param == "assign_by_region") {
      value = tolower(value);
      m_assign_by_region = (value == "true");
      handled = true;
    }
    else if(param == "region_mid_x") {
      m_region_mid_x = atof(value.c_str());
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  if(m_vnames.size() == 0)
    reportConfigWarning("No vname parameters configured.");

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void PointAssign::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);

}

//---------------------------------------------------------
// Procedure: handleVisitPoint()

//---------------------------------------------------------
// Procedure: handleVisitPoint()

void PointAssign::handleVisitPoint(string sval)
{
  sval = stripBlankEnds(sval);

  if(sval == "firstpoint") {
    m_points.clear();
    m_outgoing_posts.clear();
    m_outgoing_ix = 0;
    m_distributing = false;

    m_total_received = 0;
    m_total_assigned = 0;

    reportEvent("Received firstpoint. Starting new point set.");
    return;
  }

  if(sval == "lastpoint") {
    queueDistribution();
    reportEvent("Received lastpoint. Queued point distribution.");
    return;
  }

  VisitPoint point;
  bool ok = parseVisitPoint(sval, point);

  if(!ok) {
    reportRunWarning("Bad VISIT_POINT format: " + sval);
    return;
  }

  m_points.push_back(point);

  unsigned int vehicle_ix = 0;

  if(m_assign_by_region && (m_vnames.size() >= 2)) {
    if(point.x < m_region_mid_x)
      vehicle_ix = 0;
    else
      vehicle_ix = 1;
  }
  else {
    vehicle_ix = m_total_received % m_vnames.size();
  }

  string vname = m_vnames[vehicle_ix];

  string color = "yellow";
  if(vehicle_ix == 1)
    color = "cyan";
  else if(vehicle_ix == 2)
    color = "green";
  else if(vehicle_ix == 3)
    color = "magenta";

  string label = vname + "_" + point.id;
  postViewPoint(point.x, point.y, label, color);

  m_total_received++;
}

//---------------------------------------------------------
// Procedure: parseVisitPoint()
// Expected format: x=8, y=9, id=1

bool PointAssign::parseVisitPoint(string sval, VisitPoint& point)
{
  string xstr  = tokStringParse(sval, "x", ',', '=');
  string ystr  = tokStringParse(sval, "y", ',', '=');
  string idstr = tokStringParse(sval, "id", ',', '=');

  if((xstr == "") || (ystr == "") || (idstr == ""))
    return(false);

  point.x = atof(xstr.c_str());
  point.y = atof(ystr.c_str());
  point.id = stripBlankEnds(idstr);
  point.spec = sval;

  return(true);
}

//---------------------------------------------------------
// Procedure: distributePoints()

void PointAssign::distributePoints()
{
  if(m_vnames.size() == 0) {
    reportRunWarning("No vehicles configured. Cannot distribute points.");
    return;
  }

  // Send firstpoint to each vehicle.
  for(unsigned int i = 0; i < m_vnames.size(); i++) {
    string var = "VISIT_POINT_" + toupperString(m_vnames[i]);
    Notify(var, "firstpoint");
  }

  for(unsigned int i = 0; i < m_points.size(); i++) {
    unsigned int vehicle_ix = 0;

    if(m_assign_by_region && (m_vnames.size() >= 2)) {
      if(m_points[i].x < m_region_mid_x)
        vehicle_ix = 0;
      else
        vehicle_ix = 1;
    }
    else {
      vehicle_ix = i % m_vnames.size();
    }

    string vname = m_vnames[vehicle_ix];
    string var = "VISIT_POINT_" + toupperString(vname);

    Notify(var, m_points[i].spec);

    string color = "yellow";
    if(vehicle_ix == 1)
      color = "cyan";
    else if(vehicle_ix == 2)
      color = "green";
    else if(vehicle_ix == 3)
      color = "magenta";

    string label = vname + "_" + m_points[i].id;
    postViewPoint(m_points[i].x, m_points[i].y, label, color);

    m_total_assigned++;
  }

  // Send lastpoint to each vehicle.
  for(unsigned int i = 0; i < m_vnames.size(); i++) {
    string var = "VISIT_POINT_" + toupperString(m_vnames[i]);
    Notify(var, "lastpoint");
  }
}

//---------------------------------------------------------
// Procedure: queueDistribution()

void PointAssign::queueDistribution()
{
  if(m_vnames.size() == 0) {
    reportRunWarning("No vehicles configured. Cannot distribute points.");
    return;
  }

  m_outgoing_posts.clear();
  m_outgoing_ix = 0;
  m_total_assigned = 0;

  // Send firstpoint to each vehicle.
  for(unsigned int i = 0; i < m_vnames.size(); i++) {
    string var = "VISIT_POINT_" + toupperString(m_vnames[i]);
    m_outgoing_posts.push_back(make_pair(var, string("firstpoint")));
  }

  // Queue all assigned points.
  for(unsigned int i = 0; i < m_points.size(); i++) {
    unsigned int vehicle_ix = 0;

    if(m_assign_by_region && (m_vnames.size() >= 2)) {
      if(m_points[i].x < m_region_mid_x)
        vehicle_ix = 0;
      else
        vehicle_ix = 1;
    }
    else {
      vehicle_ix = i % m_vnames.size();
    }

    string vname = m_vnames[vehicle_ix];
    string var = "VISIT_POINT_" + toupperString(vname);

    m_outgoing_posts.push_back(make_pair(var, m_points[i].spec));
  }

  // Send lastpoint to each vehicle.
  for(unsigned int i = 0; i < m_vnames.size(); i++) {
    string var = "VISIT_POINT_" + toupperString(m_vnames[i]);
    m_outgoing_posts.push_back(make_pair(var, string("lastpoint")));
  }

  m_distributing = true;

  reportEvent("Queued " + uintToString(m_outgoing_posts.size()) + " outgoing point messages.");
}

//---------------------------------------------------------
// Procedure: postNextQueuedPoint()

void PointAssign::postNextQueuedPoint()
{
  if(!m_distributing)
    return;

  if(m_outgoing_ix >= m_outgoing_posts.size()) {
    m_distributing = false;
    reportEvent("Finished streaming all point assignments.");
    return;
  }

  string var = m_outgoing_posts[m_outgoing_ix].first;
  string val = m_outgoing_posts[m_outgoing_ix].second;

  Notify(var, val);

  if(strContains(val, "x="))
    m_total_assigned++;

  m_outgoing_ix++;
}

//---------------------------------------------------------
// Procedure: postViewPoint()

void PointAssign::postViewPoint(double x, double y, string label, string color)
{
  XYPoint point(x, y);
  point.set_label(label);
  point.set_color("vertex", color);
  point.set_param("vertex_size", "4");

  string spec = point.get_spec();
  Notify("VIEW_POINT", spec);
}

//---------------------------------------------------------
// Procedure: toupperString()

string PointAssign::toupperString(string str)
{
  for(unsigned int i = 0; i < str.length(); i++)
    str[i] = toupper(str[i]);

  return(str);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool PointAssign::buildReport()
{
  m_msgs << "============================================" << endl;
  m_msgs << "pPointAssign Report                         " << endl;
  m_msgs << "============================================" << endl;
  m_msgs << "Vehicles configured: " << m_vnames.size() << endl;

  for(unsigned int i = 0; i < m_vnames.size(); i++)
    m_msgs << "  vehicle[" << i << "]: " << m_vnames[i] << endl;

  m_msgs << "Assign by region:    " << (m_assign_by_region ? "true" : "false") << endl;
  m_msgs << "Region mid x:        " << m_region_mid_x << endl;
  m_msgs << "Points received:     " << m_total_received << endl;
  m_msgs << "Points assigned:     " << m_total_assigned << endl;

  return(true);
}