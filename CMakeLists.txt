cmake_minimum_required(VERSION 3.20)
project(CruiseNavigator C CXX)

add_subdirectory(gcn-recomp)

gcn_add_game(
  NAME         CruiseNavigator
  TITLE        "Mario Party 7"
  LABEL        "Cruise Navigator"
  DISC_ID      GP7E01
  DOL_SHA1     d4e5a0f8b4b1a6b6ab67d0514b7696e64072167e
  SYMBOLS      ${CMAKE_SOURCE_DIR}/analysis/symbols.txt
  TABLES       ${CMAKE_SOURCE_DIR}/recomp
  SOURCES
    ${CMAKE_SOURCE_DIR}/src/mp7_shims.c
    ${CMAKE_SOURCE_DIR}/src/mp7_launcher.cpp
  FPS          60)

# Rename the output binary to CruiseNavigator(.exe)
set_target_properties(CruiseNavigator PROPERTIES OUTPUT_NAME "CruiseNavigator")

# The native disc picker uses GetOpenFileNameA on Windows.
if(WIN32)
  target_link_libraries(CruiseNavigator PRIVATE comdlg32)
endif()
