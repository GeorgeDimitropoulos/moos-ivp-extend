/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: BHV_ZigLeg.cpp                                  */
/*    DATE: Summer 2026                                     */
/************************************************************/

#include <cstdlib>
#include "BHV_ZigLeg.h"
#include "MBUtils.h"
#include "XYRangePulse.h"
#include "ZAIC_PEAK.h"

using namespace std;

//-----------------------------------------------------------
// Constructor

BHV_ZigLeg::BHV_ZigLeg(IvPDomain domain) : IvPBehavior(domain)
{
  this->setParam("descriptor", "zigleg");

  addInfoVars("NAV_X, NAV_Y, NAV_HEADING, WPT_INDEX");

  // Pulse defaults
  m_pulse_range    = 25.0;
  m_pulse_duration = 4.0;
  m_delay          = 5.0;

  // Zig defaults
  m_zig_angle      = 45.0;
  m_zig_duration   = 10.0;

  // State
  m_osx = 0;
  m_osy = 0;
  m_osh = 0;
  m_curr_time = 0;

  m_wpt_index = 0;
  m_prev_wpt_index = 0;
  m_wpt_index_set = false;

  m_pending_zig = false;
  m_zig_active = false;
  m_wpt_change_time = 0;
  m_zig_start_time = 0;
  m_zig_heading = 0;
}

//-----------------------------------------------------------
// Procedure: setParam()

bool BHV_ZigLeg::setParam(string param, string val)
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

  if(param == "zig_angle") {
    if(!isNumber(val))
      return(false);
    double dval = atof(val.c_str());
    if((dval < -180) || (dval > 180))
      return(false);
    m_zig_angle = dval;
    return(true);
  }

  if(param == "zig_duration") {
    if(!isNumber(val))
      return(false);
    double dval = atof(val.c_str());
    if(dval <= 0)
      return(false);
    m_zig_duration = dval;
    return(true);
  }

  return(false);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()

bool BHV_ZigLeg::updateInfoIn()
{
  bool ok_x = false;
  bool ok_y = false;
  bool ok_h = false;
  bool ok_wpt = false;

  m_osx = getBufferDoubleVal("NAV_X", ok_x);
  m_osy = getBufferDoubleVal("NAV_Y", ok_y);
  m_osh = getBufferDoubleVal("NAV_HEADING", ok_h);

  double wpt_dval = getBufferDoubleVal("WPT_INDEX", ok_wpt);

  m_curr_time = getBufferCurrTime();

  if(!ok_x || !ok_y || !ok_h || !ok_wpt) {
    postWMessage("Missing NAV_X, NAV_Y, NAV_HEADING, or WPT_INDEX from InfoBuffer.");
    return(false);
  }

  m_wpt_index = (int)(wpt_dval);

  return(true);
}

//-----------------------------------------------------------
// Procedure: postPulse()

void BHV_ZigLeg::postPulse()
{
  XYRangePulse pulse;

  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label("bhv_zigleg");
  pulse.set_rad(m_pulse_range);
  pulse.set_duration(m_pulse_duration);
  pulse.set_time(m_curr_time);
  pulse.set_color("edge", "orange");
  pulse.set_color("fill", "orange");
  pulse.set_param("edge_size", "1");

  string spec = pulse.get_spec();

  postMessage("VIEW_RANGE_PULSE", spec);
}

//-----------------------------------------------------------
// Procedure: buildFunction()

IvPFunction* BHV_ZigLeg::buildFunction()
{
  ZAIC_PEAK zaic(m_domain, "course");

  zaic.setSummit(m_zig_heading);
  zaic.setPeakWidth(0);
  zaic.setBaseWidth(180);
  zaic.setSummitDelta(0);
  zaic.setValueWrap(true);

  IvPFunction *ipf = zaic.extractIvPFunction();

  if(ipf)
    ipf->setPWT(m_priority_wt);

  return(ipf);
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction* BHV_ZigLeg::onRunState()
{
  bool ok = updateInfoIn();

  if(!ok)
    return(0);

  // First time seeing WPT_INDEX: initialize but do not zig.
  if(!m_wpt_index_set) {
    m_prev_wpt_index = m_wpt_index;
    m_wpt_index_set = true;
    return(0);
  }

  // Detect waypoint index change.
  if(m_wpt_index != m_prev_wpt_index) {
    m_prev_wpt_index = m_wpt_index;
    m_wpt_change_time = m_curr_time;
    m_pending_zig = true;
    m_zig_active = false;
  }

  // Start zig m_delay seconds after waypoint index change.
  if(m_pending_zig && ((m_curr_time - m_wpt_change_time) >= m_delay)) {
    postPulse();

    m_zig_heading = m_osh + m_zig_angle;

    while(m_zig_heading >= 360)
      m_zig_heading -= 360;
    while(m_zig_heading < 0)
      m_zig_heading += 360;

    m_zig_start_time = m_curr_time;
    m_pending_zig = false;
    m_zig_active = true;
  }

  // Stop zig after duration.
  if(m_zig_active && ((m_curr_time - m_zig_start_time) > m_zig_duration)) {
    m_zig_active = false;
    return(0);
  }

  if(m_zig_active)
    return(buildFunction());

  return(0);
}
