# Requirements and traceability

IDs are referenced from source comments and tests (`grep -r GR-SUP-`). This table is the
traceability matrix in the spirit of ASPICE SWE.1 → SWE.4/SWE.6. It is *aligned with*, not
assessed against, ASPICE.

| ID | Requirement | Implementation | Verification |
|----|-------------|----------------|--------------|
| GR-ARCH-001 | Decision logic is ROS-independent and unit-testable | `guardrail_core` | CI builds core without ROS |
| GR-ARCH-002 | Core compiles with `-fno-exceptions -fno-rtti` and no heap use after init | `CMakeLists.txt`, `std::array` only | compile flags + review |
| GR-SUP-001 | Supervisor starts in SAFE_STOP (fail-safe) | `Supervisor::state_` | `Supervisor.StartsInSafeStop` |
| GR-SUP-002 | Non-finite / out-of-range / future-stamped inputs force SAFE_STOP | `Supervisor::update` | `NonFiniteInputsAreFaults`, `FutureFrameStampIsInvalid` |
| GR-SUP-003 | Data age > 200 ms → DEGRADED; > 500 ms → SAFE_STOP | `Supervisor::update` | `StaleDataDegradesThenStops` |
| GR-SUP-004 | Pipeline latency over budget → DEGRADED | `Supervisor::update` | `LatencyOverBudgetDegrades` |
| GR-SUP-005 | Person < 2.0 m → DEGRADED; < 0.8 m → SAFE_STOP | `Supervisor::update` | `PersonProximityRules` |
| GR-SUP-006 | Decision carries reason flags | `Decision::reasons` | asserted in tests above |
| GR-SUP-007 | Escalate immediately; de-escalate one level per healthy streak | `Supervisor::update` | `RecoversOneLevelPerStreak` |
| GR-LAT-001 | Histogram percentile never under-reports | `LatencyHistogram` | `PercentileIsConservativeUpperBound` |
| GR-LAT-002 | Overflow samples report the true maximum | `LatencyHistogram` | `OverflowReportsTrueMax` |
| GR-LAT-003 | CI gate exits 1 on p99/jitter/max violation, 2 on bad input | `tools/latency_gate.cpp` | ctest `gate_pass/fail/bad_input` |
| GR-LAT-005 | Gate bucket width adapts to the sample range so p99 is meaningful above 25.6 ms (found with real data) | `tools/latency_gate.cpp` | ctest `gate_wide_range` |
| GR-LAT-004 | Trace events are converted to per-message latencies; negative/unmatched data is an error | `guardrail_tools/trace_to_latency.py` | `test_trace_to_latency.py` (12 tests) |
| GR-FLT-001 | Fault campaigns are reproducible from a seed | `FaultInjector` | `DeterministicForSameSeed` |
| GR-FLT-002 | Each fault scenario meets explicit detection-time / severity / recovery expectations with zero pre-fault false alarms | `tools/fault_campaign.cpp` | ctest `fault_campaign_all`; mutation-checked |
| GR-PER-001 | Monocular distance estimate d = f·H/h; invalid/non-finite/overflow inputs rejected | `geometry.cpp` | `Geometry.*` |
| GR-PER-002 | Detections reduce to nearest-person distance + its confidence; none → no person, confidence 1.0 | `summarise` | `Summary.*` |
| GR-PER-003 | Any invalid detection or malformed list forces confidence 0 (supervisor degrades) | `summarise` | `AnyInvalidDetectionForcesZeroConfidence`, `MalformedCountIsFailSafe` |
| GR-PER-004 | Detector never publishes a frame with a future stamp or an undecodable image (supervisor then goes stale → SAFE_STOP) | `detector_node.cpp` | live run + review (no automated test yet) |
| GR-DET-001 | YOLOX row count follows strides 8/16/32 for the input size; sizes not divisible by 32 are rejected | `yolox_postprocess.cpp` | `ExpectedRowsMatchesYoloxNanoLayout` |
| GR-DET-002 | Raw YOLOX output is decoded with grid + stride; score = objectness × person-class | `decodeYoloxPersons` | `DecodesGridAndStrideForFirstRowOfEachScale`, `ScoreIsObjectnessTimesPersonAndThresholdApplies` |
| GR-DET-003 | Malformed tensors / NaN / overflow are rejected or dropped, never turned into boxes | `decodeYoloxPersons` | `MalformedInputsAreRejectedOrDropped` |
| GR-DET-004 | NMS keeps the highest score and suppresses overlaps | `nonMaxSuppression` | `YoloxNms.*` |
| GR-DET-005 | Missing/invalid model makes the node fail to start (no silent fallback) | `OnnxYoloxDetector` ctor | `MissingModelThrows` |
| GR-DET-006 | C++ letterbox + decode matches an independent numpy/onnxruntime reference (IoU ≥ 0.9, Δconf ≤ 0.02) | `OnnxYoloxDetector` | `scripts/yolox_reference.py --compare` (manual; 6/6 boxes, IoU 0.95–0.98); gtest `DetectsPedestriansInRealFrame…` (needs model) |
| GR-SYS-001 | The pipeline's input is camera frames (image/video); detections are derived from pixels, never from hand-set numbers. With no frames the data goes stale and the supervisor ends in SAFE_STOP | `detector_node` + `Supervisor`; web console tab "Ảnh / Video" | `fault_campaign` `camera_freeze`/`total_dropout`; web demo: "Ngắt camera" → SAFE_STOP in < 1 s (manual, headless-Chrome run) |
| GR-DET-007 | Browser YOLOX (`ui/yolox.js`) reproduces the reference letterbox/decode/NMS | `ui/yolox.js` | manual cross-check vs `scripts/yolox_reference.py` on `pedestrians.jpg`: 6/6 boxes, IoU 0.93–0.98, Δconf ≤ 0.008 (no automated test yet) |
| GR-PRIV-001 | No raw image/biometric payload leaves the robot | `visitor_events.py` | `test_rejects_raw_image` |
| GR-PRIV-002 | Visitor IDs pseudonymised with keyed HMAC-SHA256 | `visitor_events.py` | `test_pseudonymises_and_drops_raw_id` |
| GR-PRIV-003 | Events expire after the retention TTL | `purge_expired` | `test_retention_purge` |
| GR-PRIV-004 | Audit log contains no personal data | `AuditRecord` | `test_audit_log_has_no_personal_data` |

Default thresholds are **illustrative**; they have not been validated against a real robot or
hazard analysis result. See [hara.md](hara.md).
