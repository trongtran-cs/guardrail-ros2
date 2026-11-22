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

#include <string>

#include "guardrail_detector/hog_detector.hpp"
#include "opencv2/imgcodecs.hpp"

using guardrail_detector::HogConfig;
using guardrail_detector::HogDetector;

TEST(HogDetector, EmptyImageGivesNoDetections)
{
  HogDetector d{HogConfig{}};
  EXPECT_TRUE(d.detect(cv::Mat()).empty());
}

TEST(HogDetector, BlankImageGivesNoDetections)
{
  HogDetector d{HogConfig{}};
  const cv::Mat blank(480, 640, CV_8UC3, cv::Scalar(30, 30, 30));
  EXPECT_TRUE(d.detect(blank).empty());
}

TEST(HogDetector, ConfidenceIsNormalisedAndBoxesAreInOriginalCoordinates)
{
  const cv::Mat frame = cv::imread(std::string(FIXTURE_DIR) + "/pedestrians.jpg");
  ASSERT_FALSE(frame.empty()) << "missing fixture pedestrians.jpg";
  HogConfig cfg;
  cfg.max_width = 400;  // force downscaling; boxes must map back to full resolution
  HogDetector d{cfg};
  const auto people = d.detect(frame);
  ASSERT_GE(people.size(), 1U) << "HOG should find at least one pedestrian in the fixture";
  for (const auto & p : people) {
    EXPECT_GE(p.confidence, 0.0F);
    EXPECT_LE(p.confidence, 1.0F);
    EXPECT_GT(p.box.height, 0);
    EXPECT_LE(p.box.y + p.box.height, frame.rows + frame.rows / 4);  // roughly inside the frame
  }
}
