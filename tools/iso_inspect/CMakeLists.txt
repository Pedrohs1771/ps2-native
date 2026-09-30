cmake_minimum_required(VERSION 3.16)
project(ps2_iso_inspect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(iso_inspector src/iso_inspector.cpp)
target_include_directories(iso_inspector PUBLIC include)

add_executable(ps2iso-inspect src/main.cpp)
target_link_libraries(ps2iso-inspect PRIVATE iso_inspector)

function(iso_inspect_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
  endif()
endfunction()

iso_inspect_enable_warnings(iso_inspector)
iso_inspect_enable_warnings(ps2iso-inspect)

include(CTest)
if(BUILD_TESTING)
  add_executable(iso_inspector_tests tests/iso_inspector_tests.cpp)
  target_link_libraries(iso_inspector_tests PRIVATE iso_inspector)
  iso_inspect_enable_warnings(iso_inspector_tests)
  add_test(NAME iso_inspector_tests COMMAND iso_inspector_tests)
  add_test(
    NAME cli_json_schema
    COMMAND ${CMAKE_COMMAND}
      -DINSPECTOR=$<TARGET_FILE:ps2iso-inspect>
      -DTEST_HELPER=$<TARGET_FILE:iso_inspector_tests>
      -DFIXTURE=${CMAKE_CURRENT_BINARY_DIR}/cli_json_fixture.iso
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli_json.cmake
  )
endif()
