/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: BHV_Pulse.h                                     */
/*    DATE: Summer 2026                                     */
/************************************************************/

#ifndef Pulse_HEADER
#define Pulse_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_Pulse : public IvPBehavior {
public:
  BHV_Pulse(IvPDomain);
  ~BHV_Pulse() {}

  bool         setParam(std::string, std::string);
  IvPFunction* onRunState();

protected:
  bool         updateInfoIn();
  void         postPulse();

protected: // Configuration parameters
  double       m_pulse_range;
  double       m_pulse_duration;
  double       m_delay;

protected: // State variables
  double       m_osx;
  double       m_osy;
  double       m_curr_time;

  int          m_wpt_index;
  int          m_prev_wpt_index;
  bool         m_wpt_index_set;

  bool         m_pending_pulse;
  double       m_wpt_change_time;
};

#define IVP_EXPORT_FUNCTION

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_Pulse(domain);}
}
#endif
