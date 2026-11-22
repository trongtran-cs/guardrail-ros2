// Copyright 2026 GuardRail-ROS contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "guardrail_detector/onnx_yolox_detector.hpp"
#include "opencv2/imgcodecs.hpp"

using guardrail_detector::OnnxYoloxDetector;
using guardrail_detector::YoloxConfig;

namespace
{
// The model is not stored in the repo (see scripts/fetch_model.sh). Tests that need it skip.
std::string modelPath()
{
  const char * p = std::getenv("GUARDRAIL_YOLOX_MODEL");
  return (p == nullptr) ? std::string() : std::string(p);
}
}  // namespace

// GR-DET-005
TEST(OnnxYolox, MissingModelThrows)
{
  YoloxConfig cfg;
  cfg.model_path = "/nonexistent/model.onnx";
  EXPECT_THROW(OnnxYoloxDetector{cfg}, std::runtime_error);
}

// GR-DET-006
TEST(OnnxYolox, DetectsPedestriansInRealFrameInOriginalCoordinates)
{
  if (modelPath().empty()) {
    GTEST_SKIP() << "set GUARDRAIL_YOLOX_MODEL to a YOLOX .onnx file";
  }
  const cv::Mat frame = cv::imread(std::string(FIXTURE_DIR) + "/pedestrians.jpg");
  ASSERT_FALSE(frame.empty());
  YoloxConfig cfg;
  cfg.model_path = modelPath();
  OnnxYoloxDetector d{cfg};
  EXPECT_EQ(d.inputWidth(), 416);
  const auto people = d.detect(frame);
  ASSERT_GE(people.size(), 1U);
  for (const auto & p : people) {
    EXPECT_GT(p.confidence, 0.0F);
    EXPECT_LE(p.confidence, 1.0F);
    EXPECT_GE(p.box.x, 0);
    EXPECT_GE(p.box.y, 0);
    EXPECT_LE(p.box.x + p.box.width, frame.cols);
    EXPECT_LE(p.box.y + p.box.height, frame.rows);
  }
}

TEST(OnnxYolox, BlankImageGivesNoPersons)
{
  if (modelPath().empty()) {
    GTEST_SKIP() << "set GUARDRAIL_YOLOX_MODEL to a YOLOX .onnx file";
  }
  YoloxConfig cfg;
  cfg.model_path = modelPath();
  OnnxYoloxDetector d{cfg};
  const cv::Mat blank(480, 640, CV_8UC3, cv::Scalar(30, 30, 30));
  EXPECT_TRUE(d.detect(blank).empty());
  EXPECT_TRUE(d.detect(cv::Mat()).empty());
}
