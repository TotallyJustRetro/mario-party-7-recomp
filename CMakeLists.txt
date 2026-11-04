cmake_minimum_required(VERSION 3.20)
project(mp7 C CXX)

add_subdirectory(gcn-recomp)

gcn_add_game(
  NAME mp7
  TITLE "Mario Party 7"
  LABEL "MP7 Recomp"
  DISC_ID GP7E01
  DOL_SHA1 d4e5a0f8b4b1a6b6ab67d0514b7696e64072167e
  SYMBOLS ${CMAKE_SOURCE_DIR}/analysis/symbols.txt
  TABLES  ${CMAKE_SOURCE_DIR}/recomp
  SOURCES ${CMAKE_SOURCE_DIR}/src/mp7_shims.c
  FPS 60)
