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
//
// Thin ROS 2 wrapper around guardrail::Supervisor. All decision logic lives in
// guardrail_core (ROS-independent, unit-tested). Builds and passes ament lint on
// ROS 2 Jazzy (see docs/verification-status.md).

#include <chrono>
#include <memory>

#include "guardrail_core/supervisor.hpp"
#include "guardrail_msgs/msg/perception_status.hpp"
#include "guardrail_msgs/msg/safety_state.hpp"
#include "rclcpp/rclcpp.hpp"

namespace guardrail_ros
{

class SupervisorNode : public rclcpp::Node
{
public:
  SupervisorNode()
  : rclcpp::Node("guardrail_supervisor"), supervisor_(loadConfig())
  {
    const int period_ms = declare_parameter<int>("period_ms", 20);
    pub_ = create_publisher<guardrail_msgs::msg::SafetyState>(
      "safety_state", rclcpp::QoS(1).reliable());
    sub_ = create_subscription<guardrail_msgs::msg::PerceptionStatus>(
      "perception_status", rclcpp::SensorDataQoS(),
      [this](guardrail_msgs::msg::PerceptionStatus::ConstSharedPtr msg) {
        last_ = *msg;
        have_msg_ = true;
      });
    timer_ = create_wall_timer(std::chrono::milliseconds(period_ms), [this]() {tick();});
  }

private:
  guardrail::Config loadConfig()
  {
    guardrail::Config c;
    c.stale_degraded_ns = declare_parameter<int64_t>("stale_degraded_ns", c.stale_degraded_ns);
    c.stale_stop_ns = declare_parameter<int64_t>("stale_stop_ns", c.stale_stop_ns);
    c.latency_budget_ns = declare_parameter<int64_t>("latency_budget_ns", c.latency_budget_ns);
    c.min_confidence = static_cast<float>(declare_parameter<double>("min_confidence",
        c.min_confidence));
    c.slow_distance_m = static_cast<float>(declare_parameter<double>("slow_distance_m",
        c.slow_distance_m));
    c.stop_distance_m = static_cast<float>(declare_parameter<double>("stop_distance_m",
        c.stop_distance_m));
    return c;
  }

  void tick()
  {
    guardrail::Inputs in;
    const rclcpp::Time now = get_clock()->now();
    in.now_ns = now.nanoseconds();
    if (have_msg_) {
      in.last_frame_ns = rclcpp::Time(last_.header.stamp).nanoseconds();
      in.confidence = last_.confidence;
      in.nearest_person_m = last_.nearest_person_m;
      in.pipeline_latency_ns = static_cast<std::int64_t>(last_.pipeline_latency_ns);
    } else {
      // No data yet: age is "infinite" -> supervisor stays in SAFE_STOP (fail-safe).
      in.last_frame_ns = 0;
    }
    const guardrail::Decision d = supervisor_.update(in);

    guardrail_msgs::msg::SafetyState out;
    out.header.stamp = now;
    out.state = static_cast<std::uint8_t>(d.state);
    out.speed_limit_mps = d.speed_limit_mps;
    out.reasons = d.reasons;
    pub_->publish(out);
  }

  guardrail::Supervisor supervisor_;
  guardrail_msgs::msg::PerceptionStatus last_;
  bool have_msg_{false};
  rclcpp::Publisher<guardrail_msgs::msg::SafetyState>::SharedPtr pub_;
  rclcpp::Subscription<guardrail_msgs::msg::PerceptionStatus>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace guardrail_ros

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<guardrail_ros::SupervisorNode>());
  rclcpp::shutdown();
  return 0;
}
