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

#ifndef GUARDRAIL_DETECTOR__ONNX_YOLOX_DETECTOR_HPP_
#define GUARDRAIL_DETECTOR__ONNX_YOLOX_DETECTOR_HPP_

#include <memory>
#include <string>
#include <vector>

#include "guardrail_detector/hog_detector.hpp"

namespace guardrail_detector
{

struct YoloxConfig
{
  std::string model_path;
  float score_thresh{0.35F};
  float nms_iou{0.45F};
  int intra_op_threads{0};  ///< 0 = ONNX Runtime default
};

/// YOLOX (Apache-2.0, Megvii) via ONNX Runtime; person class only. CPU execution provider by
/// default; other providers (TensorRT/CUDA on Jetson) are a deployment option, not tested here.
/// The constructor throws std::runtime_error if the model is missing or has an unexpected shape.
class OnnxYoloxDetector : public PersonDetector
{
public:
  explicit OnnxYoloxDetector(const YoloxConfig & config);
  ~OnnxYoloxDetector() override;
  OnnxYoloxDetector(const OnnxYoloxDetector &) = delete;
  OnnxYoloxDetector & operator=(const OnnxYoloxDetector &) = delete;

  std::vector<PersonBox> detect(const cv::Mat & bgr) override;

  int inputWidth() const;
  int inputHeight() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace guardrail_detector

#endif  // GUARDRAIL_DETECTOR__ONNX_YOLOX_DETECTOR_HPP_
