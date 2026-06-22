/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.h                                     */
/*    DATE: Summer 2026                                     */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYSegList.h"

struct Swimmer {
  std::string id;
  double x;
  double y;
  bool found;
};

struct Point2D {
  double x;
  double y;
};

struct Obstacle {
  std::string label;
  double x;
  double y;
  double target_radius;
  double segment_radius;
};

class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {};

 protected:
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();
  void RegisterVariables();
  
 protected:
  bool handleMailNewSwimmer(std::string);
  bool handleMailFoundSwimmer(std::string);
  bool handleMailRescueRegion(std::string);

  void postShortestPath();
  void postNullPath();

  bool parseSwimmerAlert(std::string, Swimmer&);
  std::string parseID(std::string);
  int getSwimmerIndexByID(std::string) const;
  double dist(double, double, double, double) const;
  double pointSegDist(double, double, double, double, double, double) const;

  void initMap();
  bool pointInField(double, double) const;
  double fieldBoundaryDist(double, double) const;
  bool pointIsSafe(double, double) const;
  bool segmentIsSafe(double, double, double, double) const;
  void getSafePoint(double, double, double&, double&) const;

 private: // Config variables
  std::string m_vname;
  
 private: // State variables
  std::vector<Swimmer> m_swimmers;
  std::vector<Point2D>  m_field_poly;
  std::vector<Obstacle> m_obstacles;

  XYSegList  m_path;
  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;

  double     m_field_cx;
  double     m_field_cy;
  double     m_field_margin;

  unsigned int m_total_alerts;
  unsigned int m_duplicate_alerts;
  unsigned int m_paths_posted;
  std::string m_last_update_str;
  double m_last_path_repost_time;
};

#endif 