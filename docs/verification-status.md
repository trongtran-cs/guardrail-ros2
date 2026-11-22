# Verification status (honest ledger)

Last updated: 2026-11-22. Environment: Docker Desktop, linux/aarch64, ROS 2 Jazzy.

| Component | Built | Tested | Notes |
|---|---|---|---|
| `guardrail_core` | ✅ AppleClang 21, `-Werror`, `-fno-exceptions -fno-rtti` | ✅ 26 gtest + 4 gate CLI + 2 campaign ctest | Linux/GCC + sanitizer jobs are defined in CI but have not run yet (repo not pushed) |
| `guardrail_bridge` | ✅ Python 3.14 | ✅ 7 unittest | ruff/mypy configured, not yet run |
| `fault_campaign` (9 scenarios) | ✅ -Werror | ✅ all pass; fails when supervisor is weakened (mutation check) | Decision-logic simulation, **not** robot/Gazebo measurements |
| `guardrail_tools` `trace_to_latency` | ✅ Python 3.14 | ✅ 13 unittest incl. an excerpt of a **real** Jazzy capture | One real capture (5,061 trace lines, 361/361 `rclcpp_publish`→`rmw_publish` pairs) parsed correctly |
| `scripts/trace_capture.sh` (LTTng→babeltrace2→converter→gate) | — | ✅ ran end-to-end once in the container: PASS | Measures **publish-path overhead** (µs), *not* perception pipeline latency; needs a detector node with a capture→detect stage pair |
| `guardrail_core` geometry (distance/summary) | ✅ -Werror | ✅ 8 gtest | Distance is a coarse pinhole estimate, uncalibrated |
| `guardrail_detector_node` (HOG backend, image source, launch) | ✅ Jazzy (Docker) | ✅ 3 gtest incl. real pedestrian frame; ament lint | HOG is a classic detector: low accuracy, no mAP measured |
| Live pipeline (video → detector → supervisor), 300 frames @10 fps, aarch64 container CPU | ✅ ran | n/a | capture→status latency p50 23.9 ms, p99 ≈ 35–36 ms, one 121 ms start-up outlier. **CPU HOG in a Docker VM on a laptop, not edge hardware** |
| YOLOX ONNX backend (`OnnxYoloxDetector`, ONNX Runtime 1.24 via `ros-jazzy-onnxruntime-vendor`) | ✅ Jazzy (Docker) | ✅ 7 postprocess gtest; 3 ONNX gtest with the real model (skip without `GUARDRAIL_YOLOX_MODEL`); ament lint | CPU execution provider only; TensorRT/CUDA on Jetson untested |
| YOLOX vs independent Python reference (`scripts/yolox_reference.py`) | — | ✅ on `pedestrians.jpg`: 6 boxes, IoU 0.95–0.98, identical confidences | One image; no accuracy (mAP/recall) measured on a labelled set |
| Live latency by backend (video → detector → status, 10 fps, ~30 s, one run each, Docker VM on a laptop, CPU) | ✅ ran | n/a | HOG: p50 24.3 / p99 46.1 / max 62 ms (n=240) · YOLOX-nano: p50 23.0 / p99 81.1 / max 98.9 ms (n=270) · YOLOX-tiny: p50 38.7 / p99 185 / max 507 ms (n=270). **Single short runs with noisy tails; not a benchmark.** Tail comes from a shared VM, not controlled |
| cppcheck static analysis (all C++ packages) | — | ✅ `scripts/run_cppcheck.sh`, run in the Docker `ci` target and the `static-analysis` CI job; exit 0 on current code, exit 1 when a bug is injected | `ament_cppcheck` **silently skips** on cppcheck 2.13.0 (Ubuntu 24.04: 13 skipped tests), so it is run directly. 6 style/performance findings reviewed and suppressed with reasons in `scripts/cppcheck-suppressions.txt`; no functional defects found |
| Web console image/video tab (`ui/`: YOLOX-nano via ONNX Runtime Web → distance → JS supervisor) | — | ✅ driven in headless Chrome 22 Nov 2026: sample image → 6 persons, NOMINAL; sample video ~10 fps, p50 ≈ 35 ms, "Ngắt camera" → SAFE_STOP < 1 s, restore → NOMINAL; JS pipeline vs Python reference on one image: 6/6 boxes, IoU 0.93–0.98 | Manual, no automated UI test; WebAssembly CPU latency on a laptop (first frame ≈ 260 ms JIT outlier is kept in the stats); needs network for the ORT CDN; Safari/Firefox untested |
| `guardrail_msgs` | ✅ Jazzy (Docker, aarch64) | ✅ ament lint | Verified in `Dockerfile` target `ci` |
| `guardrail_supervisor_node` | ✅ Jazzy (Docker, aarch64) | ✅ ament copyright/cpplint/uncrustify/xmllint; no runtime unit test yet | Ran live in the trace capture below; lint caught 2 real issues that were fixed |
| Gazebo scenarios, YOLO node | ⏳ Not implemented | — | Roadmap (see README) |
| Jetson INT8 benchmark | ⏳ Not implemented | — | Optional |

**No performance numbers are claimed** until the roadmap items above produce measurements.
