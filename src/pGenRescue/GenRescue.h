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

 private: // Config variables
  std::string m_vname;
  
 private: // State variables
  std::vector<Swimmer> m_swimmers;

  XYSegList  m_path;
  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;

  unsigned int m_total_alerts;
  unsigned int m_duplicate_alerts;
  unsigned int m_paths_posted;
};

#endif 