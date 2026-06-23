/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: BHV_Pulse.cpp                                   */
/*    DATE: Summer 2026                                     */
/************************************************************/

#include <cstdlib>
#include "BHV_Pulse.h"
#include "MBUtils.h"
#include "XYRangePulse.h"

using namespace std;

//-----------------------------------------------------------
// Constructor

BHV_Pulse::BHV_Pulse(IvPDomain domain) : IvPBehavior(domain)
{
  this->setParam("descriptor", "pulse");

  // The behavior needs these variables from the Helm InfoBuffer.
  addInfoVars("NAV_X, NAV_Y, WPT_INDEX");

  // Configuration defaults
  m_pulse_range    = 40.0;
  m_pulse_duration = 4.0;
  m_delay          = 5.0;

  // State variables
  m_osx = 0;
  m_osy = 0;
  m_curr_time = 0;

  m_wpt_index = 0;
  m_prev_wpt_index = 0;
  m_wpt_index_set = false;

  m_pending_pulse = false;
  m_wpt_change_time = 0;
}

//-----------------------------------------------------------
// Procedure: setParam()

bool BHV_Pulse::setParam(string param, string val)
{
  param = tolower(param);
  val   = stripBlankEnds(val);

  if(param == "pulse_range") {
    if(!isNumber(val))
      return(false);

    double dval = atof(val.c_str());
    if(dval <= 0)
      return(false);

    m_pulse_range = dval;
    return(true);
  }

  if(param == "pulse_duration") {
    if(!isNumber(val))
      return(false);

    double dval = atof(val.c_str());
    if(dval <= 0)
      return(false);

    m_pulse_duration = dval;
    return(true);
  }

  if(param == "delay") {
    if(!isNumber(val))
      return(false);

    double dval = atof(val.c_str());
    if(dval < 0)
      return(false);

    m_delay = dval;
    return(true);
  }

  return(false);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()

bool BHV_Pulse::updateInfoIn()
{
  bool ok_x = false;
  bool ok_y = false;
  bool ok_wpt = false;

  m_osx = getBufferDoubleVal("NAV_X", ok_x);
  m_osy = getBufferDoubleVal("NAV_Y", ok_y);

  double wpt_dval = getBufferDoubleVal("WPT_INDEX", ok_wpt);

  m_curr_time = getBufferCurrTime();

  if(!ok_x || !ok_y || !ok_wpt) {
    postWMessage("Missing NAV_X, NAV_Y, or WPT_INDEX from InfoBuffer.");
    return(false);
  }

  m_wpt_index = (int)(wpt_dval);

  return(true);
}

//-----------------------------------------------------------
// Procedure: postPulse()

void BHV_Pulse::postPulse()
{
  XYRangePulse pulse;

  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label("bhv_pulse");
  pulse.set_rad(m_pulse_range);
  pulse.set_duration(m_pulse_duration);
  pulse.set_time(m_curr_time);
  pulse.set_color("edge", "yellow");
  pulse.set_color("fill", "yellow");
  pulse.set_param("edge_size", "1");

  string spec = pulse.get_spec();

  postMessage("VIEW_RANGE_PULSE", spec);
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction* BHV_Pulse::onRunState()
{
  bool ok = updateInfoIn();

  if(!ok)
    return(0);

  // First time seeing WPT_INDEX: initialize but do not pulse.
  if(!m_wpt_index_set) {
    m_prev_wpt_index = m_wpt_index;
    m_wpt_index_set = true;
    return(0);
  }

  // Detect that the waypoint behavior has advanced to the next waypoint.
  if(m_wpt_index != m_prev_wpt_index) {
    m_prev_wpt_index = m_wpt_index;
    m_wpt_change_time = m_curr_time;
    m_pending_pulse = true;
  }

  // Post pulse m_delay seconds after the waypoint index change.
  if(m_pending_pulse && ((m_curr_time - m_wpt_change_time) >= m_delay)) {
    postPulse();
    m_pending_pulse = false;
  }

  // This behavior posts a visual artifact only. It does not influence heading/speed.
  return(0);
}
