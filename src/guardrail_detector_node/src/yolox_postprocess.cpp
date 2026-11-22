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

#include "guardrail_detector/yolox_postprocess.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace guardrail_detector
{

namespace
{
constexpr std::array<int, 3> kStrides = {8, 16, 32};

bool sizeOk(int w, int h)
{
  return (w > 0) && (h > 0) && ((w % 32) == 0) && ((h % 32) == 0);
}
}  // namespace

std::size_t yoloxExpectedRows(int input_w, int input_h)
{
  if (!sizeOk(input_w, input_h)) {
    return 0U;
  }
  std::size_t rows = 0U;
  for (const int s : kStrides) {
    rows += static_cast<std::size_t>(input_w / s) * static_cast<std::size_t>(input_h / s);
  }
  return rows;
}

DecodeResult decodeYoloxPersons(
  const float * data, std::size_t rows, int input_w, int input_h, float score_thresh)
{
  DecodeResult result;
  if ((data == nullptr) || (rows == 0U) || (rows != yoloxExpectedRows(input_w, input_h))) {
    return result;
  }
  result.ok = true;

  std::size_t row = 0U;
  for (const int stride : kStrides) {
    const int grid_w = input_w / stride;
    const int grid_h = input_h / stride;
    const float s = static_cast<float>(stride);
    for (int gy = 0; gy < grid_h; ++gy) {
      for (int gx = 0; gx < grid_w; ++gx, ++row) {
        const float * p = data + (row * kYoloxCols);
        const float score = p[4] * p[5U + kCocoPersonClass];
        if (!(score >= score_thresh)) {  // also rejects NaN
          continue;
        }
        const float cx = (p[0] + static_cast<float>(gx)) * s;
        const float cy = (p[1] + static_cast<float>(gy)) * s;
        const float w = std::exp(p[2]) * s;
        const float h = std::exp(p[3]) * s;
        if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(w) || !std::isfinite(h) ||
          (w <= 0.0F) || (h <= 0.0F))
        {
          continue;
        }
        ScoredBox b;
        b.x1 = cx - (w / 2.0F);
        b.y1 = cy - (h / 2.0F);
        b.x2 = cx + (w / 2.0F);
        b.y2 = cy + (h / 2.0F);
        b.score = score;
        result.boxes.push_back(b);
      }
    }
  }
  return result;
}

float iou(const ScoredBox & a, const ScoredBox & b)
{
  const float ix1 = std::max(a.x1, b.x1);
  const float iy1 = std::max(a.y1, b.y1);
  const float ix2 = std::min(a.x2, b.x2);
  const float iy2 = std::min(a.y2, b.y2);
  const float iw = std::max(0.0F, ix2 - ix1);
  const float ih = std::max(0.0F, iy2 - iy1);
  const float inter = iw * ih;
  const float area_a = std::max(0.0F, a.x2 - a.x1) * std::max(0.0F, a.y2 - a.y1);
  const float area_b = std::max(0.0F, b.x2 - b.x1) * std::max(0.0F, b.y2 - b.y1);
  const float uni = area_a + area_b - inter;
  return (uni > 0.0F) ? (inter / uni) : 0.0F;
}

std::vector<ScoredBox> nonMaxSuppression(std::vector<ScoredBox> boxes, float iou_thresh)
{
  std::stable_sort(
    boxes.begin(), boxes.end(),
    [](const ScoredBox & a, const ScoredBox & b) {return a.score > b.score;});
  std::vector<ScoredBox> kept;
  for (const ScoredBox & candidate : boxes) {
    bool suppressed = false;
    for (const ScoredBox & k : kept) {
      if (iou(candidate, k) > iou_thresh) {
        suppressed = true;
        break;
      }
    }
    if (!suppressed) {
      kept.push_back(candidate);
    }
  }
  return kept;
}

}  // namespace guardrail_detector
