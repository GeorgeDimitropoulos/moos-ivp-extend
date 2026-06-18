/************************************************************/
/*    NAME: George Dimitropoulos                            */
/*    ORGN: MIT / 2.680                                     */
/*    FILE: GenPath.h                                       */
/*    DATE: June 2026                                       */
/************************************************************/

#ifndef GenPath_HEADER
#define GenPath_HEADER

#include <string>
#include <vector>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

struct VisitPointGP {
  double x;
  double y;
  std::string id;
  bool visited;
};

class GenPath : public AppCastingMOOSApp
{
 public:
   GenPath();
   ~GenPath();

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
   bool parseVisitPoint(std::string sval, VisitPointGP& point);
   void generatePath();
   double distToPoint(double x1, double y1, double x2, double y2);

 private:
   std::vector<VisitPointGP> m_points;

   bool   m_first_point_received;
   bool   m_last_point_received;
   bool   m_nav_received;
   bool   m_path_generated;

   double m_nav_x;
   double m_nav_y;
   double m_visit_radius;

   unsigned int m_total_received;
   unsigned int m_invalid_received;

   std::string m_updates_var;
};

#endif