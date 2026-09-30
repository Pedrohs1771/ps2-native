cmake_minimum_required(VERSION 3.21)
project(PS2NativeDesktopPackage LANGUAGES C CXX)

set(PS2X_SOURCE_ROOT "" CACHE PATH "Path to the PS2Recomp source checkout")
set(PS2X_GENERATED_CODE_DIR "" CACHE PATH "Directory containing generated PS2 C++ sources")

if(NOT IS_DIRECTORY "${PS2X_SOURCE_ROOT}")
    message(FATAL_ERROR "PS2X_SOURCE_ROOT must point to the PS2Recomp checkout")
endif()
if(NOT IS_DIRECTORY "${PS2X_GENERATED_CODE_DIR}")
    message(FATAL_ERROR "PS2X_GENERATED_CODE_DIR must point to the recompiler output directory")
endif()

# add_subdirectory keeps runtime objects within this disposable title-specific
# project. Never configure the checkout's root build in place.
add_subdirectory("${PS2X_SOURCE_ROOT}" ps2x-source)

if(NOT TARGET ps2EntryRunner)
    message(FATAL_ERROR "The source checkout did not define target ps2EntryRunner")
endif()
message(STATUS "PS2 runtime CMake owns generated-source handoff from ${PS2X_GENERATED_CODE_DIR}")
