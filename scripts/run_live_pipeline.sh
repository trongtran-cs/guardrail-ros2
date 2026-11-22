#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run camera -> detector -> supervisor on a video, log capture->status latency, gate it.
# Run inside the dev/ci container after `colcon build`:
#   [BACKEND=hog|yolox] [MODEL_PATH=/path/yolox_nano.onnx] \
#     ./scripts/run_live_pipeline.sh /path/to/video.avi [seconds] [p99_budget_ms]
# Example video: https://github.com/opencv/opencv/blob/4.x/samples/data/vtest.avi (Apache-2.0)
#
# Measures CPU HOG latency on whatever machine runs it (a Docker VM on a laptop is NOT edge
# hardware). Report the machine alongside any number.
set -euo pipefail

VIDEO="${1:?usage: run_live_pipeline.sh VIDEO [seconds] [p99_budget_ms]}"
SECONDS_RUN="${2:-30}"
P99_MS="${3:-100}"
OUT="${LIVE_OUT:-/ws/live_out}"
mkdir -p "$OUT"

set +u
# shellcheck disable=SC1091
source "/opt/ros/${ROS_DISTRO:-jazzy}/setup.bash"
# shellcheck disable=SC1091
source /ws/install/setup.bash
set -u

ARGS=(video_path:="$VIDEO" latency_log_path:="$OUT/latency.txt" fps:=10.0 backend:="${BACKEND:-hog}")
# An empty value makes `ros2 launch` reject the argument, so only pass model_path when set.
[ -n "${MODEL_PATH:-}" ] && ARGS+=(model_path:="$MODEL_PATH")
ros2 launch guardrail_detector_node pipeline.launch.py "${ARGS[@]}" >"$OUT/launch.log" 2>&1 &
LAUNCH_PID=$!
trap 'kill -INT "$LAUNCH_PID" 2>/dev/null || true' EXIT
sleep "$SECONDS_RUN"
# Stop the launch cleanly; escalate if it does not exit (a blocking `wait` hung CI-like runs).
kill -INT "$LAUNCH_PID" 2>/dev/null || true
for _ in 1 2 3 4 5; do
  kill -0 "$LAUNCH_PID" 2>/dev/null || break
  sleep 1
done
kill -TERM "$LAUNCH_PID" 2>/dev/null || true
sleep 1
kill -KILL "$LAUNCH_PID" 2>/dev/null || true
trap - EXIT

echo "frames logged: $(wc -l <"$OUT/latency.txt")"
GATE="$(ros2 pkg prefix guardrail_core)/lib/guardrail_core/latency_gate"
"$GATE" --input "$OUT/latency.txt" --p99-ms "$P99_MS"
