# Architecture

```
 Gazebo (OpenSCENARIO 2)                      ┌─────────────────────────┐
   camera/depth ─► detector node (C++) ──────►│ guardrail_supervisor     │──► safety_state ─► motion gate
        ▲            │ PerceptionStatus       │  (thin node)             │
        │            ▼                        │   └ guardrail_core       │
   FaultInjector   ros2_tracing (LTTng)       │      Supervisor FSM      │
   (drop/delay/    └► trace_to_latency ─► latency_gate (CI p99/jitter)   │
    noise/skew)                               └─────────────────────────┘
                                   events ─► guardrail_bridge ─► enterprise mock (OIDC REST)
                                              (pseudonymise, TTL, audit)
```

Implemented now: `guardrail_core`, `latency_gate`, `guardrail_bridge`, `guardrail_msgs`,
`guardrail_supervisor_node`. See [verification-status.md](verification-status.md).

Design decisions are recorded as ADRs in [adr/](adr/).
