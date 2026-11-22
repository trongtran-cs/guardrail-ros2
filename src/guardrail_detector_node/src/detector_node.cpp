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

// Person detector node: Image -> HOG -> guardrail_core::summarise -> PerceptionStatus.
//
// Latency semantics: pipeline_latency_ns = (publish time) - (image header stamp), i.e.
// capture-to-status as long as the camera stamps frames at capture. A negative value (clock
// mismatch) is never published: no message -> supervisor sees stale data -> SAFE_STOP.

#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cv_bridge/cv_bridge.hpp"
#include "guardrail_core/geometry.hpp"
#include "guardrail_detector/hog_detector.hpp"
#ifdef GUARDRAIL_WITH_ONNX
#include "guardrail_detector/onnx_yolox_detector.hpp"
#endif
#include "guardrail_msgs/msg/perception_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"

namespace guardrail_detector
{

class DetectorNode : public rclcpp::Node
{
public:
  DetectorNode()
  : rclcpp::Node("guardrail_detector")
  {
    focal_px_ = static_cast<float>(declare_parameter<double>("focal_px", 600.0));
    person_height_m_ = static_cast<float>(declare_parameter<double>("person_height_m", 1.7));
    const std::string backend = declare_parameter<std::string>("backend", "hog");
    if (backend == "hog") {
      HogConfig cfg;
      cfg.max_width = static_cast<int>(declare_parameter<int64_t>("max_width", cfg.max_width));
      cfg.hit_threshold = declare_parameter<double>("hit_threshold", cfg.hit_threshold);
      cfg.score_norm = declare_parameter<double>("score_norm", cfg.score_norm);
      detector_ = std::make_unique<HogDetector>(cfg);
    } else if (backend == "yolox") {
#ifdef GUARDRAIL_WITH_ONNX
      YoloxConfig cfg;
      cfg.model_path = declare_parameter<std::string>("model_path", "");
      cfg.score_thresh = static_cast<float>(
        declare_parameter<double>("score_thresh", static_cast<double>(cfg.score_thresh)));
      cfg.nms_iou = static_cast<float>(
        declare_parameter<double>("nms_iou", static_cast<double>(cfg.nms_iou)));
      cfg.intra_op_threads = static_cast<int>(declare_parameter<int64_t>("intra_op_threads", 0));
      detector_ = std::make_unique<OnnxYoloxDetector>(cfg);  // throws -> node fails to start
#else
      throw std::runtime_error("backend 'yolox' requested but built without ONNX Runtime");
#endif
    } else {
      throw std::invalid_argument("unknown backend '" + backend + "' (use 'hog' or 'yolox')");
    }
    RCLCPP_INFO(get_logger(), "person detector backend: %s", backend.c_str());

    const std::string log_path = declare_parameter<std::string>("latency_log_path", "");
    if (!log_path.empty()) {
      latency_log_.open(log_path, std::ios::out | std::ios::trunc);
      if (!latency_log_) {
        RCLCPP_ERROR(get_logger(), "cannot open latency_log_path '%s'", log_path.c_str());
      }
    }

    pub_ = create_publisher<guardrail_msgs::msg::PerceptionStatus>(
      "perception_status", rclcpp::SensorDataQoS());
    sub_ = create_subscription<sensor_msgs::msg::Image>(
      "image_raw", rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::Image::ConstSharedPtr msg) {onImage(msg);});
  }

  ~DetectorNode() override
  {
    if (latency_log_.is_open()) {
      latency_log_.flush();
    }
  }

private:
  void onImage(const sensor_msgs::msg::Image::ConstSharedPtr & msg)
  {
    cv_bridge::CvImageConstPtr img;
    try {
      img = cv_bridge::toCvShare(msg, "bgr8");
    } catch (const cv_bridge::Exception & e) {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 2000, "cv_bridge: %s", e.what());
      return;  // no output -> supervisor goes stale -> safe
    }
    if (img->image.empty()) {
      return;
    }

    std::vector<PersonBox> boxes = detector_->detect(img->image);
    // Keep the largest boxes (nearest people) if more than kMaxDetections are found.
    std::sort(
      boxes.begin(), boxes.end(),
      [](const PersonBox & a, const PersonBox & b) {return a.box.height > b.box.height;});

    guardrail::DetectionList list;
    for (const PersonBox & b : boxes) {
      if (list.count >= guardrail::kMaxDetections) {
        break;
      }
      list.items[list.count].bbox_height_px = static_cast<float>(b.box.height);
      list.items[list.count].confidence = b.confidence;
      ++list.count;
    }
    const guardrail::Summary s = guardrail::summarise(list, focal_px_, person_height_m_);

    const rclcpp::Time now = get_clock()->now();
    const rclcpp::Time stamp(msg->header.stamp, get_clock()->get_clock_type());
    const int64_t latency_ns = (now - stamp).nanoseconds();
    if (latency_ns < 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "image stamp is in the future; dropping frame");
      return;
    }

    guardrail_msgs::msg::PerceptionStatus out;
    out.header.stamp = msg->header.stamp;
    out.header.frame_id = msg->header.frame_id;
    out.confidence = s.confidence;
    out.nearest_person_m = s.nearest_person_m;
    out.pipeline_latency_ns = static_cast<uint64_t>(latency_ns);
    pub_->publish(out);

    if (latency_log_.is_open()) {
      latency_log_ << latency_ns << '\n';
      if ((++frames_ % 30U) == 0U) {
        latency_log_.flush();
      }
    }
  }

  float focal_px_{600.0F};
  float person_height_m_{1.7F};
  std::unique_ptr<PersonDetector> detector_;
  std::ofstream latency_log_;
  uint32_t frames_{0U};
  rclcpp::Publisher<guardrail_msgs::msg::PerceptionStatus>::SharedPtr pub_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
};

}  // namespace guardrail_detector

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<guardrail_detector::DetectorNode>());
  rclcpp::shutdown();
  return 0;
}
