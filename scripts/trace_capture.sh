#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# EXPERIMENTAL: capture an LTTng/ros2_tracing session around the supervisor node, export it with
# babeltrace2, and convert to latencies for latency_gate.
#
# Run inside the dev container after `colcon build`:
#   ./scripts/trace_capture.sh [seconds] [p99_budget_ms]
#
# Status: ran end-to-end once (Jazzy, aarch64 container). rclcpp_publish -> rmw_publish keyed by
# `message` pairs 1:1 for rclcpp publishers. NOTE this measures the publish path (microseconds),
# not perception latency; for that, trace a capture stage and a detect-published stage in the
# detector node and set START/END accordingly. Pointer keys can be reused by the allocator, so
# duplicates/unmatched events are reported (use --allow-unmatched only deliberately).
set -euo pipefail

DURATION="${1:-10}"
P99_MS="${2:-30}"
OUT="${TRACE_OUT:-/ws/trace_out}"
SESSION="guardrail-$(date +%Y%m%d-%H%M%S)"
START="ros2:rclcpp_publish"
END="ros2:rmw_publish"
KEY="message"

: "${ROS_DISTRO:?source /opt/ros/<distro>/setup.bash first}"
[ -f /ws/install/setup.bash ] || { echo "run colcon build first" >&2; exit 2; }
# ROS setup scripts reference unset variables, so relax -u while sourcing them.
set +u
# shellcheck disable=SC1091
source /ws/install/setup.bash
set -u
export PYTHONPATH="/ws/src/guardrail_tools${PYTHONPATH:+:$PYTHONPATH}"

mkdir -p "$OUT"
cleanup() { kill "${PUB_PID:-0}" "${NODE_PID:-0}" 2>/dev/null || true; ros2 trace stop "$SESSION" >/dev/null 2>&1 || true; }
trap cleanup EXIT

ros2 trace start "$SESSION" --path "$OUT"
ros2 run guardrail_supervisor_node supervisor_node &
NODE_PID=$!
sleep 1
# Synthetic perception stream at 30 Hz (until the detector node exists).
ros2 topic pub -r 30 /perception_status guardrail_msgs/msg/PerceptionStatus \
  "{confidence: 0.9, nearest_person_m: -1.0, pipeline_latency_ns: 20000000}" >/dev/null &
PUB_PID=$!
sleep "$DURATION"
cleanup
trap - EXIT

TRACE_DIR="$(find "$OUT" -maxdepth 2 -type d -name 'ust' -path "*$SESSION*" -print -quit | xargs -r dirname)"
[ -n "$TRACE_DIR" ] || { echo "no trace found under $OUT" >&2; exit 2; }

babeltrace2 "$TRACE_DIR" > "$OUT/$SESSION.txt"
python3 -m guardrail_tools.trace_to_latency --input "$OUT/$SESSION.txt" --format babeltrace2 \
  --start-stage "$START" --end-stage "$END" --key-field "$KEY" --allow-unmatched \
  --output "$OUT/$SESSION.latency.txt"

# latency_gate is installed by colcon under the guardrail_core package prefix.
GATE="$(ros2 pkg prefix guardrail_core)/lib/guardrail_core/latency_gate"
"$GATE" --input "$OUT/$SESSION.latency.txt" --p99-ms "$P99_MS"
