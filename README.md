# GuardRail-ROS

**Safety-supervised, latency-gated, fault-injectable perception for edge robots (ROS 2).**

A reference stack for a guide/reception robot that answers three questions most ROS demos skip:

1. *When perception degrades, does the robot slow down or stop — provably?* → deterministic
   C++17 safety supervisor, unit-tested, fail-safe by default.
2. *Did my last commit make the pipeline slower?* → `latency_gate`: a CI gate on p99 / jitter / max
   latency budgets (exit 0/1/2), fed from trace-derived latencies.
3. *Can this talk to enterprise systems without leaking visitors' data?* → privacy-by-design
   bridge: no raw images, keyed-HMAC pseudonyms, retention TTL, PII-free audit log.

> **Status: early alpha.** See [docs/verification-status.md](docs/verification-status.md) for exactly
> what is built and tested. No performance numbers are claimed yet. Nothing here is certified;
> standards are "aligned with / inspired by" — see [docs/standards-alignment.md](docs/standards-alignment.md).

## Quick start (core, no ROS needed)

```bash
cmake -S src/guardrail_core -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure

# Gate a latency file (one integer nanosecond value per line)
./build/latency_gate --input latencies.txt --p99-ms 30 --jitter-ms 10 --max-ms 80

# Fault campaign: 9 scenarios, closed loop FaultInjector -> Supervisor (logic-level simulation)
./build/fault_campaign --scenario all --csv campaign.csv

# Trace -> latency -> gate (run the converter from src/guardrail_tools)
python3 -m guardrail_tools.trace_to_latency --input trace.csv \
  --start-stage capture --end-stage detect_published --output lat.txt
./build/latency_gate --input lat.txt --p99-ms 30
```

If a system-wide GoogleTest is broken on your machine, add `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=ON`
to fetch a pinned googletest instead.

Python tests (stdlib `unittest`): `cd src/guardrail_tools && python3 -m unittest discover -s test -v`,
and the same in `src/guardrail_bridge`.

ROS 2 (Jazzy / Ubuntu 24.04): `colcon build && colcon test` (CI also runs an experimental Lyrical job).

### Docker (works on macOS/Windows)

```bash
docker build --target ci -t guardrail-ros:ci .      # build + all tests (ament lint included)
docker compose run --rm dev                           # interactive dev shell, repo mounted
# inside the dev shell, after `colcon build`:
./scripts/trace_capture.sh 10 30                      # LTTng trace -> latency -> gate (experimental)
./scripts/run_live_pipeline.sh video.avi 30 100       # camera -> HOG detector -> supervisor, gated latency
```

### YOLOX (ONNX Runtime) backend

```bash
./scripts/fetch_model.sh yolox_nano models            # downloads + verifies SHA-256 (not stored in git)
ros2 launch guardrail_detector_node pipeline.launch.py \
  backend:=yolox model_path:=$PWD/models/yolox_nano.onnx video_path:=video.avi
GUARDRAIL_YOLOX_MODEL=$PWD/models/yolox_nano.onnx colcon test   # enables the model-dependent tests
python3 scripts/yolox_reference.py models/yolox_nano.onnx img.jpg --compare cpp_out.txt  # cross-check
```

Model: YOLOX (Megvii, Apache-2.0), COCO-trained, person class only. See
[docs/third-party.md](docs/third-party.md).

For releases, pin the base image by digest (`--build-arg BASE_IMAGE=ros:jazzy-ros-base@sha256:...`).

## Input and output

The system's input is **camera frames (image/video)**; without frames there is no data, the data
goes stale and the supervisor fails safe to `SAFE_STOP`.

```
image / video ─▶ person detector (HOG | YOLOX) ─▶ nearest-person distance + confidence
                                                         │   (+ pipeline latency, data age)
                                                         ▼
                                          Supervisor ─▶ NOMINAL / DEGRADED / SAFE_STOP + speed limit
```

## Web console (YOLOX runs in the browser)

```bash
python3 -m http.server -d ui 8000      # then open http://localhost:8000
```

- **Ảnh / Video → Quyết định**: upload an image or video (or use the bundled samples); YOLOX-nano runs
  in the browser (ONNX Runtime Web, WebAssembly), boxes + estimated distance are drawn, and the
  supervisor decision, speed limit and per-frame latency are shown. "Ngắt camera" simulates a dead
  camera (→ stale → `SAFE_STOP`); "Gửi sang Latency Gate" feeds the measured latencies to the gate tab.
  Nothing is uploaded; the page only needs the network to fetch `onnxruntime-web` from jsDelivr.
- **Supervisor Simulator**: the same supervisor (JS port of `supervisor.cpp`) driven by sliders instead of
  a detector, with fault injection and tunable thresholds.
- **Latency Gate** / **Fault Campaign**: viewers for `latencies.txt` and the campaign CSV.

Opened as `file://` the page cannot fetch the bundled model/samples; upload your own files and pick
`ui/models/yolox_nano.onnx` with the model button, or serve it as above. Browser latency is WebAssembly on
your machine, not edge hardware; it is not connected to ROS.

### Where to get test images/videos

| Source | What | Licence / note |
|---|---|---|
| Bundled `ui/samples/` | OpenCV `vtest.avi` (12 s, 640×480) and one frame | Apache-2.0, see `ui/samples/NOTICE.md` |
| [MOT Challenge](https://motchallenge.net) (MOT17/20) | pedestrian-tracking video sequences | research use; read the terms |
| [Penn-Fudan Pedestrian](https://www.cis.upenn.edu/~jshi/ped_html/) | 170 street images with people | research use |
| [Caltech Pedestrian](https://data.caltech.edu/records/f6rph-90m20) / [CityPersons](https://www.cityscapes-dataset.com) | driving/street pedestrians | research use |
| [COCO val2017](https://cocodataset.org) | person images (the training domain of the model) | annotations CC BY 4.0; images keep their Flickr licences |
| [Pexels](https://www.pexels.com/search/videos/pedestrians/) / [Pixabay](https://pixabay.com/videos/search/pedestrians/) | stock videos of walking people | free licences; attribute per site rules |
| Your own phone video | best test of the distance estimate (set `f` and `H` in the page) | make sure people in it consent |

## Layout

| Path | Purpose |
|---|---|
| `src/guardrail_core` | ROS-independent C++17 core: `Supervisor`, `LatencyHistogram`, `FaultInjector`, `latency_gate` |
| `src/guardrail_msgs` | `PerceptionStatus`, `SafetyState` |
| `src/guardrail_supervisor_node` | Thin rclcpp wrapper (builds + lints on Jazzy in Docker) |
| `src/guardrail_tools` | `trace_to_latency` (CSV + babeltrace2 text adapter) |
| `src/guardrail_detector_node` | `detector_node` (OpenCV HOG behind a `PersonDetector` interface), `image_source_node`, launch file |
| `ui/` | Web console: image/video → YOLOX (browser) → supervisor; simulator, latency gate, campaign viewer |
| `src/guardrail_bridge` | Privacy-by-design visitor event bridge (Python, stdlib only) |
| `Dockerfile`, `docker-compose.yml`, `scripts/` | ROS 2 Jazzy dev/CI image, LTTng trace capture experiment |
| `docs/` | Requirements + traceability, HARA-lite, threat model, ADRs, standards alignment |

## Roadmap

- [ ] Gazebo world + OpenSCENARIO 2 scenarios (reuse `scenario_execution`)
- [x] C++ person-detector node publishing `PerceptionStatus` (HOG; plain node, not yet a lifecycle node) · [x] ONNX Runtime backend with YOLOX (Apache-2.0) · [ ] TensorRT/CUDA provider on Jetson · [ ] accuracy (mAP/recall) on a labelled set
- [x] `trace_to_latency` converter feeding `latency_gate` (babeltrace2 adapter still unvalidated on real traces)
- [x] Fault campaign (9 scenarios, logic-level) · [ ] repeat on Gazebo/robot and report real numbers
- [ ] Zenoh/MQTT → OIDC REST mock bridge
- [ ] Optional: Jetson FP16 vs INT8 benchmark

## Contributing / Security / License

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md). Licensed under Apache-2.0.
