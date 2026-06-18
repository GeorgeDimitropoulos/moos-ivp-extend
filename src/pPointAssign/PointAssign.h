/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT / 2.680                                     */
/*    FILE: PointAssign.h                                   */
/*    DATE: June 2026                                       */
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include <string>
#include <vector>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include <utility>

struct VisitPoint {
  double x;
  double y;
  std::string id;
  std::string spec;
};

class PointAssign : public AppCastingMOOSApp
{
 public:
   PointAssign();
   ~PointAssign();

 protected:
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected:
   bool buildReport();

 protected:
   void registerVariables();

 private:
   void handleVisitPoint(std::string sval);
   bool parseVisitPoint(std::string sval, VisitPoint& point);
   void queueDistribution();
   void postNextQueuedPoint();
   void distributePoints();
   void postViewPoint(double x, double y, std::string label, std::string color);
   std::string toupperString(std::string str);

 private:
   std::vector<std::string> m_vnames;
   std::vector<VisitPoint> m_points;

   bool   m_assign_by_region;
   double m_region_mid_x;

   unsigned int m_total_received;
   unsigned int m_total_assigned;

   std::vector<std::pair<std::string, std::string> > m_outgoing_posts;
   unsigned int m_outgoing_ix;
   bool m_distributing;
};

#endif