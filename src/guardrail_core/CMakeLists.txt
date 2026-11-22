# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(guardrail_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GUARDRAIL_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(GUARDRAIL_BUILD_TESTS "Build unit tests" ON)

# Safety core: no exceptions, no RTTI (GR-ARCH-002).
add_library(guardrail_core
  src/supervisor.cpp
  src/latency_histogram.cpp
  src/fault_injector.cpp
  src/geometry.cpp)
add_library(guardrail::core ALIAS guardrail_core)

target_include_directories(guardrail_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_options(guardrail_core PRIVATE
  -fno-exceptions -fno-rtti
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
  -Wold-style-cast -Wcast-align -Wnull-dereference -Wdouble-promotion
  -Wformat=2)
if(GUARDRAIL_WARNINGS_AS_ERRORS)
  target_compile_options(guardrail_core PRIVATE -Werror)
endif()

# CI gate tool (not part of the safety core; may use iostream).
add_executable(latency_gate tools/latency_gate.cpp)
target_link_libraries(latency_gate PRIVATE guardrail_core)
target_compile_options(latency_gate PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
if(GUARDRAIL_WARNINGS_AS_ERRORS)
  target_compile_options(latency_gate PRIVATE -Werror)
endif()

add_executable(fault_campaign tools/fault_campaign.cpp)
target_link_libraries(fault_campaign PRIVATE guardrail_core)
target_compile_options(fault_campaign PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
if(GUARDRAIL_WARNINGS_AS_ERRORS)
  target_compile_options(fault_campaign PRIVATE -Werror)
endif()

install(TARGETS guardrail_core EXPORT export_guardrail_core
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(TARGETS latency_gate fault_campaign RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)

# Standalone build works without ROS; under colcon the package is exported via ament.
find_package(ament_cmake QUIET)
if(ament_cmake_FOUND)
  ament_export_targets(export_guardrail_core HAS_LIBRARY_TARGET)
  ament_package()
endif()

if(GUARDRAIL_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(guardrail_core_tests
    test/test_supervisor.cpp
    test/test_latency_and_faults.cpp
    test/test_geometry.cpp)
  target_link_libraries(guardrail_core_tests PRIVATE guardrail_core GTest::gtest_main)
  add_test(NAME guardrail_core_tests COMMAND guardrail_core_tests)

  # Latency gate end-to-end: pass / fail / error exit codes (GR-LAT-003).
  set(FIX ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures)
  add_test(NAME gate_pass COMMAND latency_gate --input ${FIX}/latency_ok.txt --p99-ms 30 --jitter-ms 10)
  add_test(NAME gate_fail COMMAND latency_gate --input ${FIX}/latency_regressed.txt --p99-ms 30)
  set_tests_properties(gate_fail PROPERTIES WILL_FAIL TRUE)
  # Regression (GR-LAT-005): 1% slow outliers up to 121 ms must not inflate p99 of the other 99%.
  add_test(NAME gate_wide_range COMMAND latency_gate --input ${FIX}/latency_wide_range.txt --p99-ms 35)
  add_test(NAME gate_bad_input COMMAND latency_gate --input ${FIX}/latency_malformed.txt --p99-ms 30)
  set_tests_properties(gate_bad_input PROPERTIES WILL_FAIL TRUE)

  # Fault campaign: every scenario must meet its detection/recovery expectations (GR-FLT-002).
  add_test(NAME fault_campaign_all COMMAND fault_campaign --scenario all)
  add_test(NAME fault_campaign_unknown COMMAND fault_campaign --scenario nope)
  set_tests_properties(fault_campaign_unknown PROPERTIES WILL_FAIL TRUE)
endif()
