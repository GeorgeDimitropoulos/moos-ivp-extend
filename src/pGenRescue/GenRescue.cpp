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

  for(unsigned int step = 0; step < unvisited_count; step++) {
    double best_dist = numeric_limits<double>::max();
    int best_ix = -1;

    for(unsigned int i = 0; i < m_swimmers.size(); i++) {
      if(used[i])
        continue;

      if(m_swimmers[i].found)
        continue;

      double d = dist(curr_x, curr_y, m_swimmers[i].x, m_swimmers[i].y);

      if(d < best_dist) {
        best_dist = d;
        best_ix = (int)(i);
      }
    }

    if(best_ix < 0)
      break;

    segl.add_vertex(m_swimmers[best_ix].x, m_swimmers[best_ix].y);
    used[best_ix] = true;

    curr_x = m_swimmers[best_ix].x;
    curr_y = m_swimmers[best_ix].y;
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

  string update_str = "points = " + m_path.get_spec_pts();

  Notify("SURVEY_UPDATE", update_str);

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

  Notify("SURVEY_UPDATE", update_str);
  reportEvent("SURVEY_UPDATE=" + update_str);
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