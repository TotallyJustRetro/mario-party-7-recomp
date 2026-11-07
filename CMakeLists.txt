cmake_minimum_required(VERSION 3.20)
project(CruiseNavigator C CXX)

# Patch the upstream runtime's desktop entry point so it invokes Cruise Navigator
# before trying to boot the game. The patch is idempotent and fails clearly if the
# upstream entry point changes rather than silently producing an unlaunched executable.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GCN_RUNTIME_MAIN "${CMAKE_SOURCE_DIR}/gcn-recomp/runtime/main.cpp")
if(NOT EXISTS "${GCN_RUNTIME_MAIN}")
  message(FATAL_ERROR "gcn-recomp/runtime/main.cpp was not found. Clone the gcn-recomp submodule before configuring.")
endif()
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/patch_gcn_launcher.py" "${GCN_RUNTIME_MAIN}"
  RESULT_VARIABLE GCN_LAUNCHER_PATCH_RESULT)
if(NOT GCN_LAUNCHER_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not connect Cruise Navigator to the gcn-recomp runtime entry point.")
endif()

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

# Rename the output binary to CruiseNavigator(.exe).
set_target_properties(CruiseNavigator PROPERTIES OUTPUT_NAME "CruiseNavigator")

# The native launcher uses GetOpenFileNameA. Hide the console and use WinMain.
if(WIN32)
  set_target_properties(CruiseNavigator PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_libraries(CruiseNavigator PRIVATE comdlg32 shell32 ole32)
endif()
