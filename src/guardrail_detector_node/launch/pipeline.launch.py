# Copyright 2026 GuardRail-ROS contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
"""Camera source -> detector -> supervisor. Optional: video_path, latency_log_path."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('video_path', default_value=''),
        DeclareLaunchArgument('latency_log_path', default_value=''),
        DeclareLaunchArgument('fps', default_value='10.0'),
        DeclareLaunchArgument('backend', default_value='hog'),
        DeclareLaunchArgument('model_path', default_value=''),
        Node(package='guardrail_detector_node', executable='image_source_node',
             parameters=[{'video_path': LaunchConfiguration('video_path'),
                          'fps': LaunchConfiguration('fps')}]),
        Node(package='guardrail_detector_node', executable='detector_node',
             parameters=[{'latency_log_path': LaunchConfiguration('latency_log_path'),
                          'backend': LaunchConfiguration('backend'),
                          'model_path': LaunchConfiguration('model_path')}]),
        Node(package='guardrail_supervisor_node', executable='supervisor_node'),
    ])
