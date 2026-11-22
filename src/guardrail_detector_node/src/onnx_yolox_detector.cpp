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

#include "guardrail_detector/onnx_yolox_detector.hpp"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "guardrail_detector/yolox_postprocess.hpp"
#include "opencv2/imgproc.hpp"

namespace guardrail_detector
{

namespace
{
constexpr float kPadValue = 114.0F;  // YOLOX letterbox padding
}  // namespace

struct OnnxYoloxDetector::Impl
{
  explicit Impl(const YoloxConfig & cfg)
  : config(cfg), env(ORT_LOGGING_LEVEL_WARNING, "guardrail_yolox"),
    memory(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
  {
    Ort::SessionOptions opts;
    if (cfg.intra_op_threads > 0) {
      opts.SetIntraOpNumThreads(cfg.intra_op_threads);
    }
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    try {
      session = std::make_unique<Ort::Session>(env, cfg.model_path.c_str(), opts);
    } catch (const Ort::Exception & e) {
      throw std::runtime_error(
        std::string("cannot load model '") + cfg.model_path + "': " + e.what());
    }

    Ort::AllocatorWithDefaultOptions alloc;
    if ((session->GetInputCount() != 1U) || (session->GetOutputCount() != 1U)) {
      throw std::runtime_error("YOLOX model must have exactly one input and one output");
    }
    input_name = session->GetInputNameAllocated(0, alloc).get();
    output_name = session->GetOutputNameAllocated(0, alloc).get();

    const auto in_shape =
      session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if ((in_shape.size() != 4U) || (in_shape[0] != 1) || (in_shape[1] != 3) || (in_shape[2] <= 0) ||
      (in_shape[3] <= 0))
    {
      throw std::runtime_error("expected static input shape [1,3,H,W]");
    }
    input_h = static_cast<int>(in_shape[2]);
    input_w = static_cast<int>(in_shape[3]);

    const auto out_shape =
      session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    const std::size_t rows = yoloxExpectedRows(input_w, input_h);
    if ((rows == 0U) || (out_shape.size() != 3U) || (out_shape[0] != 1) ||
      (static_cast<std::size_t>(out_shape[1]) != rows) ||
      (static_cast<std::size_t>(out_shape[2]) != kYoloxCols))
    {
      throw std::runtime_error("unexpected YOLOX output shape (need [1,N,85] raw, undecoded)");
    }
    expected_rows = rows;
    tensor.assign(static_cast<std::size_t>(3 * input_h * input_w), 0.0F);
  }

  YoloxConfig config;
  Ort::Env env;
  Ort::MemoryInfo memory;
  std::unique_ptr<Ort::Session> session;
  std::string input_name;
  std::string output_name;
  int input_w{0};
  int input_h{0};
  std::size_t expected_rows{0U};
  std::vector<float> tensor;  // reused CHW float buffer (BGR, 0..255)
};

OnnxYoloxDetector::OnnxYoloxDetector(const YoloxConfig & config)
: impl_(std::make_unique<Impl>(config))
{
}

OnnxYoloxDetector::~OnnxYoloxDetector() = default;

int OnnxYoloxDetector::inputWidth() const {return impl_->input_w;}
int OnnxYoloxDetector::inputHeight() const {return impl_->input_h;}

std::vector<PersonBox> OnnxYoloxDetector::detect(const cv::Mat & bgr)
{
  std::vector<PersonBox> out;
  if (bgr.empty() || (bgr.type() != CV_8UC3)) {
    return out;
  }
  Impl & m = *impl_;

  // Letterbox: scale to fit, keep aspect, image at top-left, pad bottom/right with 114.
  const float r = std::min(
    static_cast<float>(m.input_w) / static_cast<float>(bgr.cols),
    static_cast<float>(m.input_h) / static_cast<float>(bgr.rows));
  const int scaled_w = static_cast<int>(std::lround(static_cast<float>(bgr.cols) * r));
  const int scaled_h = static_cast<int>(std::lround(static_cast<float>(bgr.rows) * r));
  const int new_w = std::max(1, std::min(m.input_w, scaled_w));
  const int new_h = std::max(1, std::min(m.input_h, scaled_h));
  cv::Mat resized;
  cv::resize(bgr, resized, cv::Size(new_w, new_h), 0.0, 0.0, cv::INTER_LINEAR);
  cv::Mat canvas(m.input_h, m.input_w, CV_8UC3, cv::Scalar(kPadValue, kPadValue, kPadValue));
  resized.copyTo(canvas(cv::Rect(0, 0, new_w, new_h)));

  // HWC uint8 BGR -> CHW float32 (no mean/std: YOLOX expects raw 0..255 BGR).
  const std::size_t in_h = static_cast<std::size_t>(m.input_h);
  const std::size_t in_w = static_cast<std::size_t>(m.input_w);
  const std::size_t plane = in_h * in_w;
  for (int y = 0; y < m.input_h; ++y) {
    const uchar * row = canvas.ptr<uchar>(y);
    for (int x = 0; x < m.input_w; ++x) {
      const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(m.input_w)) +
        static_cast<std::size_t>(x);
      for (std::size_t c = 0U; c < 3U; ++c) {
        m.tensor[(c * plane) + idx] = static_cast<float>(row[(3 * x) + static_cast<int>(c)]);
      }
    }
  }

  const std::array<int64_t, 4> shape = {1, 3, m.input_h, m.input_w};
  Ort::Value input = Ort::Value::CreateTensor<float>(
    m.memory, m.tensor.data(), m.tensor.size(), shape.data(), shape.size());
  const std::array<const char *, 1> in_names = {m.input_name.c_str()};
  const std::array<const char *, 1> out_names = {m.output_name.c_str()};
  std::vector<Ort::Value> outputs =
    m.session->Run(Ort::RunOptions{nullptr}, in_names.data(), &input, 1, out_names.data(), 1);
  if (outputs.size() != 1U) {
    return out;
  }
  const float * raw = outputs[0].GetTensorData<float>();
  const DecodeResult decoded =
    decodeYoloxPersons(raw, m.expected_rows, m.input_w, m.input_h, m.config.score_thresh);
  if (!decoded.ok) {
    return out;
  }
  const std::vector<ScoredBox> kept = nonMaxSuppression(decoded.boxes, m.config.nms_iou);

  const float fw = static_cast<float>(bgr.cols);
  const float fh = static_cast<float>(bgr.rows);
  out.reserve(kept.size());
  for (const ScoredBox & b : kept) {
    const float x1 = std::clamp(b.x1 / r, 0.0F, fw);
    const float y1 = std::clamp(b.y1 / r, 0.0F, fh);
    const float x2 = std::clamp(b.x2 / r, 0.0F, fw);
    const float y2 = std::clamp(b.y2 / r, 0.0F, fh);
    if ((x2 <= x1) || (y2 <= y1)) {
      continue;
    }
    PersonBox p;
    p.box = cv::Rect(
      static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)),
      static_cast<int>(std::lround(x2 - x1)), static_cast<int>(std::lround(y2 - y1)));
    p.confidence = std::clamp(b.score, 0.0F, 1.0F);
    out.push_back(p);
  }
  return out;
}

}  // namespace guardrail_detector
