cmake_minimum_required(VERSION 3.21)
project(PS2NativeAndroidPackage LANGUAGES C CXX)

set(PS2X_SOURCE_ROOT "" CACHE PATH "Path to the PS2Recomp source checkout")
set(PS2X_GENERATED_CODE_DIR "" CACHE PATH "Directory containing generated PS2 C++ sources")
if(NOT PS2X_SOURCE_ROOT)
    get_filename_component(PS2X_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT IS_DIRECTORY "${PS2X_SOURCE_ROOT}")
    message(FATAL_ERROR "PS2X_SOURCE_ROOT must point to the PS2Recomp checkout")
endif()

set(PS2X_BUILD_ISO_INSPECTOR OFF CACHE BOOL "Do not build the host-only ISO inspector in the Android app" FORCE)
add_subdirectory("${PS2X_SOURCE_ROOT}" ps2x-source)

if(NOT TARGET ps2EntryRunner)
    message(FATAL_ERROR "The source checkout did not define Android target ps2EntryRunner")
endif()

if(NOT PS2X_GENERATED_CODE_DIR)
    message(WARNING "PS2X_GENERATED_CODE_DIR is empty; building the generic runtime placeholder only")
else()
    message(STATUS "PS2 runtime CMake owns generated-source handoff from ${PS2X_GENERATED_CODE_DIR}")
endif()
