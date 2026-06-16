/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT / 2.680                                     */
/*    FILE: Odometry.cpp                                    */
/*    DATE: June 15, 2026                                   */
/************************************************************/

#include <iterator>
#include <cmath>
#include "MBUtils.h"
#include "ACTable.h"
#include "Odometry.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

Odometry::Odometry()
{
  m_iteration = 0;
  m_time_warp = 1;

  m_first_reading = true;
  m_current_x = 0;
  m_current_y = 0;
  m_previous_x = 0;
  m_previous_y = 0;
  m_total_distance = 0;

  m_current_depth = 0;
  m_depth_thresh = 0;
  m_total_distance_at_depth = 0;
}

//---------------------------------------------------------
// Destructor()

Odometry::~Odometry()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool Odometry::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

#if 0 // Keep these around just for template
    string comm = msg.GetCommunity();
    double dval = msg.GetDouble();
    string sval = msg.GetString();
    string msrc = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl = msg.IsDouble();
    bool   mstr = msg.IsString();
#endif

    if(key == "NAV_X") {
      m_current_x = msg.GetDouble();
    }
    else if(key == "NAV_Y") {
      m_current_y = msg.GetDouble();
    }
    else if(key == "NAV_DEPTH") {
      m_current_depth = msg.GetDouble();
    }
    else if(key != "APPCAST_REQ") {
      reportRunWarning("Unhandled Mail: " + key);
    }
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool Odometry::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool Odometry::Iterate()
{
  AppCastingMOOSApp::Iterate();
  m_iteration++;

  if(m_first_reading) {
    m_previous_x = m_current_x;
    m_previous_y = m_current_y;
    m_first_reading = false;
  }
  else {
    double dx = m_current_x - m_previous_x;
    double dy = m_current_y - m_previous_y;
    double dist = sqrt((dx * dx) + (dy * dy));

    m_total_distance += dist;

    if(m_current_depth > m_depth_thresh)
      m_total_distance_at_depth += dist;

    m_previous_x = m_current_x;
    m_previous_y = m_current_y;
  }

  Notify("ODOMETRY_DIST", m_total_distance);
  Notify("ODOMETRY_DIST_AT_DEPTH", m_total_distance_at_depth);

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Odometry::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);

  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    string orig = *p;
    string line = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;

    if(param == "depth_thresh") {
      m_depth_thresh = atof(value.c_str());
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

void Odometry::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();

  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("NAV_DEPTH", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport()
{
  m_msgs << "============================================" << endl;
  m_msgs << "pOdometry Report                            " << endl;
  m_msgs << "============================================" << endl;
  m_msgs << "Current X:              " << m_current_x << endl;
  m_msgs << "Current Y:              " << m_current_y << endl;
  m_msgs << "Current Depth:          " << m_current_depth << endl;
  m_msgs << "Depth Threshold:        " << m_depth_thresh << endl;
  m_msgs << "Total Distance:         " << m_total_distance << endl;
  m_msgs << "Distance At Depth:      " << m_total_distance_at_depth << endl;

  return(true);
}