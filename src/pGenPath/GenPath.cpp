/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT / 2.680                                     */
/*    FILE: GenPath.cpp                                     */
/*    DATE: June 2026                                       */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include <cmath>
#include <limits>
#include "MBUtils.h"
#include "ACTable.h"
#include "XYSegList.h"
#include "GenPath.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenPath::GenPath()
{
  m_first_point_received = false;
  m_last_point_received  = false;
  m_nav_received         = false;
  m_path_generated       = false;

  m_nav_x = 0;
  m_nav_y = 0;

  m_visit_radius = 3.0;

  m_total_received   = 0;
  m_invalid_received = 0;

  m_updates_var = "WPT_UPDATE";
}

//---------------------------------------------------------
// Destructor()

GenPath::~GenPath()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

    if(key == "VISIT_POINT") {
      handleVisitPoint(msg.GetString());
    }
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      if(m_nav_received == false)
        m_nav_received = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      if(m_nav_received == false)
        m_nav_received = true;
    }
    else if(key != "APPCAST_REQ") {
      reportRunWarning("Unhandled Mail: " + key);
    }
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenPath::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if(m_last_point_received && m_nav_received && !m_path_generated)
    generatePath();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenPath::OnStartUp()
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

    if(param == "updates_var") {
      m_updates_var = value;
      handled = true;
    }
    else if(param == "visit_radius") {
      m_visit_radius = atof(value.c_str());
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

//---------------------------------------------------------
// Procedure: handleVisitPoint()

void GenPath::handleVisitPoint(string sval)
{
  sval = stripBlankEnds(sval);

  if(sval == "firstpoint") {
    m_points.clear();

    m_first_point_received = true;
    m_last_point_received  = false;
    m_path_generated       = false;

    m_total_received   = 0;
    m_invalid_received = 0;

    reportEvent("Received firstpoint. Starting new path set.");
    return;
  }

  if(sval == "lastpoint") {
    m_last_point_received = true;
    reportEvent("Received lastpoint. Ready to generate path.");
    return;
  }

  VisitPointGP point;
  bool ok = parseVisitPoint(sval, point);

  if(ok) {
    m_points.push_back(point);
    m_total_received++;
  }
  else {
    m_invalid_received++;
    reportRunWarning("Bad VISIT_POINT format: " + sval);
  }
}

//---------------------------------------------------------
// Procedure: parseVisitPoint()
// Expected format: x=8,y=9,id=1 OR x=8, y=9, id=1

bool GenPath::parseVisitPoint(string sval, VisitPointGP& point)
{
  string xstr  = tokStringParse(sval, "x", ',', '=');
  string ystr  = tokStringParse(sval, "y", ',', '=');
  string idstr = tokStringParse(sval, "id", ',', '=');

  if((xstr == "") || (ystr == "") || (idstr == ""))
    return(false);

  point.x = atof(xstr.c_str());
  point.y = atof(ystr.c_str());
  point.id = stripBlankEnds(idstr);
  point.visited = false;

  return(true);
}

//---------------------------------------------------------
// Procedure: generatePath()
// Greedy shortest path from current NAV_X/NAV_Y.

void GenPath::generatePath()
{
  if(m_points.size() == 0) {
    reportRunWarning("No visit points received. Cannot generate path.");
    return;
  }

  vector<VisitPointGP> remaining = m_points;
  XYSegList seglist;

  double curr_x = m_nav_x;
  double curr_y = m_nav_y;

  while(remaining.size() > 0) {
    double best_dist = numeric_limits<double>::max();
    unsigned int best_ix = 0;

    for(unsigned int i = 0; i < remaining.size(); i++) {
      double dist = distToPoint(curr_x, curr_y, remaining[i].x, remaining[i].y);

      if(dist < best_dist) {
        best_dist = dist;
        best_ix = i;
      }
    }

    seglist.add_vertex(remaining[best_ix].x, remaining[best_ix].y);

    curr_x = remaining[best_ix].x;
    curr_y = remaining[best_ix].y;

    remaining.erase(remaining.begin() + best_ix);
  }

  string update_str = "points = ";
  update_str += seglist.get_spec();

  Notify(m_updates_var, update_str);
  Notify("GENPATH_PATH", seglist.get_spec());

  seglist.set_label("genpath_" + m_host_community);
  seglist.set_color("edge", "white");
  seglist.set_color("vertex", "white");
  seglist.set_param("edge_size", "2");
  seglist.set_param("vertex_size", "3");

  Notify("VIEW_SEGLIST", seglist.get_spec());

  m_path_generated = true;

  reportEvent("Generated greedy path with " + uintToString(m_points.size()) + " points.");
}

//---------------------------------------------------------
// Procedure: distToPoint()

double GenPath::distToPoint(double x1, double y1, double x2, double y2)
{
  double dx = x1 - x2;
  double dy = y1 - y2;
  return(sqrt((dx * dx) + (dy * dy)));
}

//------------------------------------------------------------
// Procedure: buildReport()

bool GenPath::buildReport()
{
  unsigned int visited = 0;
  unsigned int unvisited = 0;

  for(unsigned int i = 0; i < m_points.size(); i++) {
    if(m_points[i].visited)
      visited++;
    else
      unvisited++;
  }

  m_msgs << "============================================" << endl;
  m_msgs << "pGenPath Report                             " << endl;
  m_msgs << "============================================" << endl;
  m_msgs << "Updates Var:             " << m_updates_var << endl;
  m_msgs << "Visit Radius:            " << m_visit_radius << endl;
  m_msgs << "Total Points Received:   " << m_total_received << endl;
  m_msgs << "Invalid Points Received: " << m_invalid_received << endl;
  m_msgs << "First Point Received:    " << (m_first_point_received ? "true" : "false") << endl;
  m_msgs << "Last Point Received:     " << (m_last_point_received ? "true" : "false") << endl;
  m_msgs << "NAV_X/Y Received:        " << (m_nav_received ? "true" : "false") << endl;
  m_msgs << "Path Generated:          " << (m_path_generated ? "true" : "false") << endl;
  m_msgs << endl;
  m_msgs << "Tour Status" << endl;
  m_msgs << "------------------------" << endl;
  m_msgs << "   Points Visited:       " << visited << endl;
  m_msgs << "   Points Unvisited:     " << unvisited << endl;

  return(true);
}