# SPDX-License-Identifier: Apache-2.0
# GuardRail-ROS development / CI image: ROS 2 Jazzy (Ubuntu 24.04) + LTTng tracing tools.
#
#   docker build --target dev -t guardrail-ros:dev .       # interactive dev shell
#   docker build --target ci  -t guardrail-ros:ci  .       # builds + tests, fails on error
#
# Reproducibility: for releases pin the base by digest, e.g.
#   --build-arg BASE_IMAGE=ros:jazzy-ros-base@sha256:<digest>
# (resolve with: docker buildx imagetools inspect ros:jazzy-ros-base)

ARG ROS_DISTRO=jazzy
ARG BASE_IMAGE=ros:${ROS_DISTRO}-ros-base

# ---------------------------------------------------------------- base: tools
FROM ${BASE_IMAGE} AS base
ARG ROS_DISTRO
ENV ROS_DISTRO=${ROS_DISTRO} \
    DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1

# Tracing: ros2trace + tracetools (ros2_tracing), LTTng, babeltrace2 for text export.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git \
      python3-colcon-common-extensions python3-rosdep python3-pip \
      libgtest-dev clang-tidy cppcheck \
      lttng-tools babeltrace2 \
      ros-${ROS_DISTRO}-ros2trace \
      ros-${ROS_DISTRO}-tracetools \
      ros-${ROS_DISTRO}-tracetools-trace \
      ros-${ROS_DISTRO}-ament-lint-auto \
      ros-${ROS_DISTRO}-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------ workspace deps
FROM base AS deps
WORKDIR /ws
# Copy only manifests first so dependency layers are cached across source edits.
COPY src/guardrail_core/package.xml src/guardrail_core/package.xml
COPY src/guardrail_msgs/package.xml src/guardrail_msgs/package.xml
COPY src/guardrail_supervisor_node/package.xml src/guardrail_supervisor_node/package.xml
COPY src/guardrail_detector_node/package.xml src/guardrail_detector_node/package.xml
RUN apt-get update \
    && (rosdep update --rosdistro ${ROS_DISTRO} || true) \
    && rosdep install --from-paths src --ignore-src -y --rosdistro ${ROS_DISTRO} \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------------ dev shell
FROM deps AS dev
# Non-root: the ubuntu:24.04 based ROS image ships user "ubuntu" (uid 1000).
RUN chown -R ubuntu:ubuntu /ws
USER ubuntu
ENV HOME=/home/ubuntu
RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash" >> ~/.bashrc \
    && echo "[ -f /ws/install/setup.bash ] && source /ws/install/setup.bash" >> ~/.bashrc
WORKDIR /ws
CMD ["bash"]

# ------------------------------------------------------------------- CI build
FROM deps AS ci
COPY src/ /ws/src/
COPY scripts/run_cppcheck.sh scripts/cppcheck-suppressions.txt /ws/scripts/
SHELL ["/bin/bash", "-c"]
RUN /ws/scripts/run_cppcheck.sh \
    && source /opt/ros/${ROS_DISTRO}/setup.bash \
    && colcon build --event-handlers console_cohesion+ \
    && colcon test --return-code-on-test-failure \
    && colcon test-result --verbose
