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

#include "guardrail_detector/hog_detector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "opencv2/imgproc.hpp"

namespace guardrail_detector
{

HogDetector::HogDetector(const HogConfig & config)
: config_(config)
{
  hog_.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
}

std::vector<PersonBox> HogDetector::detect(const cv::Mat & bgr)
{
  std::vector<PersonBox> out;
  if (bgr.empty() || (bgr.cols <= 0) || (bgr.rows <= 0)) {
    return out;
  }

  cv::Mat work = bgr;
  double scale = 1.0;
  if ((config_.max_width > 0) && (bgr.cols > config_.max_width)) {
    scale = static_cast<double>(config_.max_width) / static_cast<double>(bgr.cols);
    cv::resize(bgr, work, cv::Size(), scale, scale, cv::INTER_AREA);
  }

  std::vector<cv::Rect> rects;
  std::vector<double> weights;
  hog_.detectMultiScale(
    work, rects, weights, config_.hit_threshold, cv::Size(8, 8), cv::Size(), 1.05, 2.0, false);

  out.reserve(rects.size());
  for (std::size_t i = 0U; i < rects.size(); ++i) {
    const double w = (i < weights.size()) ? weights[i] : 0.0;
    const double conf = std::clamp(w / config_.score_norm, 0.0, 1.0);
    const cv::Rect & r = rects[i];
    PersonBox p;
    p.box = cv::Rect(
      static_cast<int>(std::lround(r.x / scale)), static_cast<int>(std::lround(r.y / scale)),
      static_cast<int>(std::lround(r.width / scale)),
      static_cast<int>(std::lround(r.height / scale)));
    p.confidence = static_cast<float>(conf);
    out.push_back(p);
  }
  return out;
}

}  // namespace guardrail_detector
