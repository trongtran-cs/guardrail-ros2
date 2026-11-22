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

// Test camera: publishes frames from a video file (loops) or a synthetic pattern.
// Frames are stamped with the publish time (stands in for capture time).

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

#include "cv_bridge/cv_bridge.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"

namespace guardrail_detector
{

class ImageSourceNode : public rclcpp::Node
{
public:
  ImageSourceNode()
  : rclcpp::Node("guardrail_image_source")
  {
    const double fps = declare_parameter<double>("fps", 30.0);
    const std::string video = declare_parameter<std::string>("video_path", "");
    width_ = static_cast<int>(declare_parameter<int64_t>("width", 640));
    height_ = static_cast<int>(declare_parameter<int64_t>("height", 480));
    frame_id_ = declare_parameter<std::string>("frame_id", "camera_optical_frame");
    if (!(fps > 0.0) || (width_ <= 0) || (height_ <= 0)) {
      throw std::invalid_argument("fps, width and height must be positive");
    }

    if (!video.empty()) {
      capture_.open(video);
      if (!capture_.isOpened()) {
        throw std::runtime_error("cannot open video_path: " + video);
      }
    }
    pub_ = create_publisher<sensor_msgs::msg::Image>("image_raw", rclcpp::SensorDataQoS());
    const auto period = std::chrono::duration<double>(1.0 / fps);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period), [this]() {tick();});
  }

private:
  cv::Mat nextFrame()
  {
    cv::Mat frame;
    if (capture_.isOpened()) {
      capture_ >> frame;
      if (frame.empty()) {
        capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        capture_ >> frame;
      }
      return frame;
    }
    frame = cv::Mat(height_, width_, CV_8UC3, cv::Scalar(40, 40, 40));
    const int x = static_cast<int>((counter_ * 8U) % static_cast<unsigned>(width_));
    const cv::Rect bar(x, height_ / 3, width_ / 10, height_ / 3);
    cv::rectangle(frame, bar, cv::Scalar(200, 200, 200), -1);
    return frame;
  }

  void tick()
  {
    cv::Mat frame = nextFrame();
    ++counter_;
    if (frame.empty()) {
      return;
    }
    std_msgs::msg::Header header;
    header.stamp = get_clock()->now();
    header.frame_id = frame_id_;
    pub_->publish(*cv_bridge::CvImage(header, "bgr8", frame).toImageMsg());
  }

  int width_{640};
  int height_{480};
  std::string frame_id_;
  unsigned counter_{0U};
  cv::VideoCapture capture_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace guardrail_detector

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<guardrail_detector::ImageSourceNode>());
  rclcpp::shutdown();
  return 0;
}
