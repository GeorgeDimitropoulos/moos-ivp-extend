#ifndef BHV_SCOUT_HEADER
#define BHV_SCOUT_HEADER

#include <string>
#include <vector>
#include <utility>

#include "IvPBehavior.h"
#include "XYPoint.h"
#include "XYPolygon.h"

class BHV_Scout : public IvPBehavior {
public:
  BHV_Scout(IvPDomain);
  ~BHV_Scout() {};
  
  bool          setParam(std::string, std::string);
  void          onIdleState();
  IvPFunction* onRunState();
  void          onEveryState(std::string);
  
protected:
  IvPFunction* buildFunction();
  void         updateScoutPoint();
  void         postViewPoint(bool viewable=true);

  bool         generateSweepPoints();
  bool         pointInRegion(double x, double y) const;

  bool         parseSwimmerAlert(std::string report,
                                 std::string& id,
                                 double& x,
                                 double& y) const;
  bool         addKnownSwimmer(std::string id, double x, double y);
  void         handleNodeReport(std::string report);
  double       candidatePointScore(double x, double y,
                                   unsigned int order_offset) const;
  void         chooseBestSweepPoint();

protected:
  double   m_osx;
  double   m_osy;
  double   m_curr_time;

  double   m_ptx;
  double   m_pty;
  bool     m_pt_set;

  XYPolygon m_rescue_region;

  std::vector<std::pair<double,double> > m_sweep_points;
  unsigned int m_sweep_ix;
  bool     m_sweep_ready;

  std::vector<std::string> m_known_ids;
  std::vector<std::pair<double,double> > m_known_swimmers;

  double   m_tmate_x;
  double   m_tmate_y;
  bool     m_tmate_set;

protected:
  double m_capture_radius;
  double m_desired_speed;
  double m_sweep_spacing;
  double m_known_avoid_radius;
  double m_tmate_avoid_radius;

  std::string m_tmate;
};

#define IVP_EXPORT_FUNCTION
extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain) 
  {return new BHV_Scout(domain);}
}

#endif
