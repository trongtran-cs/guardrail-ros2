# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(guardrail_supervisor_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(guardrail_msgs REQUIRED)
find_package(guardrail_core REQUIRED)

add_executable(supervisor_node src/supervisor_node.cpp)
# ament_export_targets namespaces the imported target as <package>::<target>.
# ament_target_dependencies uses the plain signature, so keep this plain too (no PRIVATE).
target_link_libraries(supervisor_node guardrail_core::guardrail_core)
ament_target_dependencies(supervisor_node rclcpp guardrail_msgs)
target_compile_options(supervisor_node PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS supervisor_node DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
