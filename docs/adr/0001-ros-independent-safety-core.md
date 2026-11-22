# ADR-0001: ROS-independent safety core

- Status: accepted
- Date: 2026-11-22

## Context
Safety decision logic must be deterministic, reviewable and testable without simulators.
`rclcpp` brings exceptions, dynamic allocation and a large dependency surface.

## Decision
Put all decision logic in `guardrail_core` (C++17, no exceptions/RTTI, no heap after init).
ROS nodes are thin adapters.

## Consequences
+ Fast unit tests, static analysis and sanitizers run on any machine.
+ Easier to port to other middleware or an RTOS.
− One extra translation layer between ROS messages and core types.
