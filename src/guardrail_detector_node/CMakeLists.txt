# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(guardrail_detector_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect videoio imgcodecs)
find_package(guardrail_msgs REQUIRED)
find_package(guardrail_core REQUIRED)

# ONNX Runtime is optional: without it the YOLOX backend is simply not built.
find_package(onnxruntime_vendor QUIET)
if(onnxruntime_vendor_FOUND)
  find_package(onnxruntime QUIET)
endif()

add_library(guardrail_detector_lib STATIC src/hog_detector.cpp src/yolox_postprocess.cpp)
target_include_directories(guardrail_detector_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  ${OpenCV_INCLUDE_DIRS})
target_link_libraries(guardrail_detector_lib ${OpenCV_LIBS})
target_compile_options(guardrail_detector_lib PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
if(TARGET onnxruntime::onnxruntime)
  message(STATUS "ONNX Runtime found: building YOLOX backend")
  target_sources(guardrail_detector_lib PRIVATE src/onnx_yolox_detector.cpp)
  target_link_libraries(guardrail_detector_lib onnxruntime::onnxruntime)
  target_compile_definitions(guardrail_detector_lib PUBLIC GUARDRAIL_WITH_ONNX=1)
else()
  message(STATUS "ONNX Runtime not found: YOLOX backend disabled (HOG only)")
endif()

add_executable(detector_node src/detector_node.cpp)
target_link_libraries(detector_node guardrail_detector_lib guardrail_core::guardrail_core)
ament_target_dependencies(detector_node rclcpp sensor_msgs cv_bridge guardrail_msgs)
target_compile_options(detector_node PRIVATE -Wall -Wextra -Wpedantic)

add_executable(image_source_node src/image_source_node.cpp)
target_link_libraries(image_source_node ${OpenCV_LIBS})
ament_target_dependencies(image_source_node rclcpp sensor_msgs cv_bridge)
target_compile_options(image_source_node PRIVATE -Wall -Wextra -Wpedantic)

if(TARGET onnxruntime::onnxruntime)
  add_executable(detect_image src/detect_image.cpp)
  target_link_libraries(detect_image guardrail_detector_lib)
  install(TARGETS detect_image DESTINATION lib/${PROJECT_NAME})
endif()

install(TARGETS detector_node image_source_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_yolox_postprocess test/test_yolox_postprocess.cpp)
  target_link_libraries(test_yolox_postprocess guardrail_detector_lib)
  if(TARGET onnxruntime::onnxruntime)
    ament_add_gtest(test_onnx_yolox test/test_onnx_yolox.cpp)
    target_link_libraries(test_onnx_yolox guardrail_detector_lib)
    target_compile_definitions(test_onnx_yolox PRIVATE
      FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
  endif()
  ament_add_gtest(test_hog_detector test/test_hog_detector.cpp)
  target_link_libraries(test_hog_detector guardrail_detector_lib)
  target_compile_definitions(test_hog_detector PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
endif()

ament_package()
