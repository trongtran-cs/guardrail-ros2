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

// YOLOX output decoding + NMS. Deliberately free of OpenCV/ONNX types so it is unit-testable
// with synthetic tensors (requirements GR-DET-*).

#ifndef GUARDRAIL_DETECTOR__YOLOX_POSTPROCESS_HPP_
#define GUARDRAIL_DETECTOR__YOLOX_POSTPROCESS_HPP_

#include <cstddef>
#include <vector>

namespace guardrail_detector
{

/// Corner box in network-input pixel coordinates.
struct ScoredBox
{
  float x1{0.0F};
  float y1{0.0F};
  float x2{0.0F};
  float y2{0.0F};
  float score{0.0F};
};

constexpr std::size_t kYoloxCols = 85U;  // 4 box + 1 objectness + 80 COCO classes
constexpr std::size_t kCocoPersonClass = 0U;

/// Number of output rows YOLOX produces for an input size (strides 8, 16, 32).
/// Returns 0 if the size is not divisible by 32.
std::size_t yoloxExpectedRows(int input_w, int input_h);

struct DecodeResult
{
  bool ok{false};  ///< false if the tensor shape does not match the input size
  std::vector<ScoredBox> boxes;
};

/// Decode a raw (non-decoded) YOLOX output [rows x 85] into person boxes above score_thresh.
/// score = objectness * person_class_score. Non-finite or degenerate boxes are dropped.
DecodeResult decodeYoloxPersons(
  const float * data, std::size_t rows, int input_w, int input_h, float score_thresh);

/// Greedy NMS (highest score first). Result is sorted by descending score.
std::vector<ScoredBox> nonMaxSuppression(std::vector<ScoredBox> boxes, float iou_thresh);

float iou(const ScoredBox & a, const ScoredBox & b);

}  // namespace guardrail_detector

#endif  // GUARDRAIL_DETECTOR__YOLOX_POSTPROCESS_HPP_
