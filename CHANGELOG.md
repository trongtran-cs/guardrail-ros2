# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/); versioning: [SemVer](https://semver.org/).

## [Unreleased]

### Fixed
- `latency_gate`: bucket width now adapts to the sample range (a fixed 25.6 ms range made p99 equal the max for slower pipelines).

### Added
- `scripts/run_cppcheck.sh` + reviewed suppressions: cppcheck now runs directly over all C++ packages in the Docker `ci` target and CI, because `ament_cppcheck` skips itself on cppcheck 2.13.0.
- YOLOX ONNX Runtime backend (`backend:=yolox`), pure decode/NMS library, `detect_image` CLI, `scripts/fetch_model.sh`, `scripts/yolox_reference.py` cross-check.
- `guardrail_detector_node` (HOG person detector, image source, launch), `geometry` (distance + summary) in core, `scripts/run_live_pipeline.sh`.
- Dockerfile (ROS 2 Jazzy, targets `dev`/`ci`), docker-compose, `scripts/trace_capture.sh` (LTTng → babeltrace2 → gate).
- `fault_campaign` (9 scenarios) and `guardrail_tools/trace_to_latency`.
- `guardrail_core`: safety `Supervisor`, `LatencyHistogram`, `FaultInjector`, `latency_gate` CLI.
- `guardrail_bridge`: privacy-by-design visitor event store.
- `guardrail_msgs`, `guardrail_supervisor_node`.
- Docs: requirements traceability, HARA-lite, threat model, standards alignment, ADR-0001.
- CI: Linux GCC/Clang+ASan/UBSan, static analysis, Python lint/type/test, ROS 2 Jazzy/Lyrical, SBOM.
