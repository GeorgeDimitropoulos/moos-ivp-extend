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

struct ContactInfo {
  std::string name;
  double x;
  double y;
  double heading;
  double speed;
  bool set;
};

struct Point2D {
  double x;
  double y;
};

struct Obstacle {
  std::string name;
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
  bool handleMailNodeReport(std::string);
  bool handleMailRescueRegion(std::string);
  bool parseRescueRegion(std::string);
  void updateFieldCentroid();

  void postShortestPath();
  void postNullPath();

  bool parseSwimmerAlert(std::string, Swimmer&);
  std::string parseID(std::string);
  bool parseNodeReport(std::string, ContactInfo&) const;

  int getSwimmerIndexByID(std::string) const;

  double dist(double, double, double, double) const;
  double pointSegDist(double, double, double, double, double, double) const;
  double bearingTo(double, double, double, double) const;
  double angleDiff(double, double) const;

  void initMap();
  bool pointInField(double, double) const;
  double fieldBoundaryDist(double, double) const;
  bool pointIsSafe(double, double) const;
  bool segmentIsSafe(double, double, double, double) const;
  void getSafePoint(double, double, double&, double&) const;

  double nearestActiveNeighborDist(unsigned int, const std::vector<bool>&) const;
  unsigned int countNearbyActive(unsigned int, const std::vector<bool>&, double) const;
  unsigned int opponentRank(unsigned int, const std::vector<bool>&) const;
  double candidateScore(unsigned int, double, double, const std::vector<bool>&) const;

 private:
  std::string m_vname;
  std::string m_planner_mode;

  std::vector<Swimmer> m_swimmers;
  std::vector<Point2D> m_field_poly;
  std::vector<Obstacle> m_obstacles;

  ContactInfo m_contact;

  XYSegList  m_path;

  double m_nav_x;
  double m_nav_y;
  bool   m_nav_x_set;
  bool   m_nav_y_set;

  double m_field_cx;
  double m_field_cy;
  double m_field_margin;

  unsigned int m_total_alerts;
  unsigned int m_duplicate_alerts;
  unsigned int m_paths_posted;
  unsigned int m_node_reports;

  std::string m_last_update_str;
  double      m_last_path_repost_time;
};

#endif
