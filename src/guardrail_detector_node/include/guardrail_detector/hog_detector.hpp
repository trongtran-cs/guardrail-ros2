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
// SPDX-License-Identifier: Apache-2.0

#ifndef GUARDRAIL_DETECTOR__HOG_DETECTOR_HPP_
#define GUARDRAIL_DETECTOR__HOG_DETECTOR_HPP_

#include <vector>

#include "opencv2/core.hpp"
#include "opencv2/objdetect.hpp"

namespace guardrail_detector
{

/// One person detection in the coordinates of the image passed to detect().
struct PersonBox
{
  cv::Rect box;
  float confidence{0.0F};  ///< normalised to [0, 1]
};

/// Backend interface so an ONNX/TensorRT detector can replace HOG without touching the node.
class PersonDetector
{
public:
  virtual ~PersonDetector() = default;
  virtual std::vector<PersonBox> detect(const cv::Mat & bgr) = 0;
};

struct HogConfig
{
  int max_width{640};          ///< larger frames are downscaled before detection (speed)
  double hit_threshold{0.0};   ///< SVM margin threshold
  double score_norm{2.0};      ///< SVM weight mapped to confidence 1.0
};

/// OpenCV HOG + default linear-SVM people detector. CPU only, no model download.
class HogDetector : public PersonDetector
{
public:
  explicit HogDetector(const HogConfig & config);
  std::vector<PersonBox> detect(const cv::Mat & bgr) override;

private:
  HogConfig config_;
  cv::HOGDescriptor hog_;
};

}  // namespace guardrail_detector

#endif  // GUARDRAIL_DETECTOR__HOG_DETECTOR_HPP_
