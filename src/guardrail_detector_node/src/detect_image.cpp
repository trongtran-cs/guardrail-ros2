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

// CLI: detect persons in one image with a YOLOX ONNX model; prints "x y w h conf" per line.
// Used to cross-check the C++ pre/post-processing against scripts/yolox_reference.py.

#include <cstdio>
#include <exception>
#include <string>

#include "guardrail_detector/onnx_yolox_detector.hpp"
#include "opencv2/imgcodecs.hpp"

int main(int argc, char ** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: detect_image MODEL.onnx IMAGE\n");
    return 2;
  }
  try {
    guardrail_detector::YoloxConfig cfg;
    cfg.model_path = argv[1];
    guardrail_detector::OnnxYoloxDetector detector(cfg);
    const cv::Mat image = cv::imread(argv[2]);
    if (image.empty()) {
      std::fprintf(stderr, "cannot read image %s\n", argv[2]);
      return 2;
    }
    for (const auto & p : detector.detect(image)) {
      std::printf(
        "%d %d %d %d %.4f\n", p.box.x, p.box.y, p.box.width, p.box.height,
        static_cast<double>(p.confidence));
    }
  } catch (const std::exception & e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 2;
  }
  return 0;
}
