/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: BHV_ZigLeg.h                                    */
/*    DATE: Summer 2026                                     */
/************************************************************/

#ifndef ZigLeg_HEADER
#define ZigLeg_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_ZigLeg : public IvPBehavior {
public:
  BHV_ZigLeg(IvPDomain);
  ~BHV_ZigLeg() {}

  bool         setParam(std::string, std::string);
  IvPFunction* onRunState();

protected:
  bool         updateInfoIn();
  void         postPulse();
  IvPFunction* buildFunction();

protected: // Configuration parameters
  double       m_pulse_range;
  double       m_pulse_duration;
  double       m_delay;

  double       m_zig_angle;
  double       m_zig_duration;

protected: // State variables
  double       m_osx;
  double       m_osy;
  double       m_osh;
  double       m_curr_time;

  int          m_wpt_index;
  int          m_prev_wpt_index;
  bool         m_wpt_index_set;

  bool         m_pending_zig;
  bool         m_zig_active;
  double       m_wpt_change_time;
  double       m_zig_start_time;
  double       m_zig_heading;
};

#define IVP_EXPORT_FUNCTION

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_ZigLeg(domain);}
}
#endif
