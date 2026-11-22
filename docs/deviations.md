# Documented deviations (MISRA-style)

| Rule area | Deviation | Rationale |
|---|---|---|
| Digit separators / literals | `200'000'000` used in `Config` defaults | Readability of nanosecond constants (C++14) |
| `<iostream>` / `<cstdio>` | Used only in `tools/latency_gate.cpp` | CI tool, not part of the safety core |
| `std::int64_t` ↔ `double` conversions | Explicit `static_cast` in percentile math | Needed for ceil(p·n); inputs validated first |
| `rclcpp` usage | Only in `guardrail_supervisor_node` | ROS API is exception-based; kept out of the core |
