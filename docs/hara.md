# HARA-lite (ISO 26262-inspired, not an assessed analysis)

Scope: indoor guide/reception robot, speed ≤ 1.0 m/s, shared space with visitors.
Severity/exposure/controllability ratings below are the author's **engineering judgement for
a portfolio exercise**, not a validated ASIL determination.

| # | Hazard | Malfunction | Safety goal | Mitigation (requirement) |
|---|--------|-------------|-------------|--------------------------|
| H1 | Collision with a person | Detector misses person / stale detections | Stop or slow before contact | GR-SUP-003, GR-SUP-005 |
| H2 | Late reaction | Pipeline latency spike (executor starvation, GC, CPU throttling) | Detect budget violation and slow down | GR-SUP-004, GR-LAT-003 (regression caught in CI) |
| H3 | Acting on garbage | NaN / negative / future timestamp | Treat invalid data as a fault | GR-SUP-002 |
| H4 | Silent frozen sensor | Camera freeze re-publishes old frame | Detect via data age | GR-SUP-003, `FaultInjector.freeze` |
| H5 | Unsafe restart | System starts and drives before data arrives | Start in safe state | GR-SUP-001 |
| H6 | Privacy breach | Raw faces/images leave the robot | No biometric export | GR-PRIV-001..004 |

## SOTIF note (ISO 21448-inspired)
Detector performance limits (occlusion, lighting, unusual poses) are *not* a hardware fault.
The supervisor mitigates them only indirectly (confidence threshold, proximity rules). A real
programme would add scenario coverage and field-data triggers.
