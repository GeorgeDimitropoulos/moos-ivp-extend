#--------------------------------------------------------
# The CMakeLists.txt for:                       pGenPath
# Author(s):                              George Dimitropoulos
#--------------------------------------------------------

SET(SRC
  GenPath.cpp
  GenPath_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pGenPath ${SRC})

TARGET_LINK_LIBRARIES(pGenPath
   ${MOOS_LIBRARIES}
   apputil
   geometry
   mbutil
   m
   pthread)

