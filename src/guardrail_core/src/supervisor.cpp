// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include "guardrail_core/supervisor.hpp"

#include <cmath>

namespace guardrail
{

namespace
{

/// Most restrictive of two states.
SafetyState worse(SafetyState a, SafetyState b) noexcept
{
  return (static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b)) ? a : b;
}

}  // namespace

Supervisor::Supervisor(const Config & config) noexcept
: config_(config)
{
}

float Supervisor::speedFor(SafetyState s) const noexcept
{
  switch (s) {
    case SafetyState::kNominal:
      return config_.nominal_speed_mps;
    case SafetyState::kDegraded:
      return config_.degraded_speed_mps;
    case SafetyState::kSafeStop:
    default:
      return 0.0F;
  }
}

Decision Supervisor::update(const Inputs & in) noexcept
{
  SafetyState required = SafetyState::kNominal;
  std::uint32_t reasons = kReasonNone;

  // GR-SUP-002: non-finite or out-of-range inputs are a fault, never ignored.
  const bool inputs_valid =
    std::isfinite(in.confidence) && (in.confidence >= 0.0F) && (in.confidence <= 1.0F) &&
    std::isfinite(in.nearest_person_m) && (in.pipeline_latency_ns >= 0) &&
    (in.now_ns >= in.last_frame_ns);

  if (!inputs_valid) {
    required = SafetyState::kSafeStop;
    reasons |= kReasonInvalidInput;
  } else {
    // GR-SUP-003: data age.
    const std::int64_t age_ns = in.now_ns - in.last_frame_ns;
    if (age_ns > config_.stale_stop_ns) {
      required = worse(required, SafetyState::kSafeStop);
      reasons |= kReasonStaleDataCritical;
    } else if (age_ns > config_.stale_degraded_ns) {
      required = worse(required, SafetyState::kDegraded);
      reasons |= kReasonStaleData;
    }

    // GR-SUP-004: latency budget.
    if (in.pipeline_latency_ns > config_.latency_budget_ns) {
      required = worse(required, SafetyState::kDegraded);
      reasons |= kReasonLatencyBudget;
    }

    // Low detector confidence.
    if (in.confidence < config_.min_confidence) {
      required = worse(required, SafetyState::kDegraded);
      reasons |= kReasonLowConfidence;
    }

    // GR-SUP-005: person proximity (negative distance = none detected).
    if (in.nearest_person_m >= 0.0F) {
      if (in.nearest_person_m < config_.stop_distance_m) {
        required = worse(required, SafetyState::kSafeStop);
        reasons |= kReasonPersonVeryNear;
      } else if (in.nearest_person_m < config_.slow_distance_m) {
        required = worse(required, SafetyState::kDegraded);
        reasons |= kReasonPersonNear;
      }
    }
  }

  // GR-SUP-007: escalate immediately, de-escalate one level after a streak.
  if (static_cast<std::uint8_t>(required) >= static_cast<std::uint8_t>(state_)) {
    state_ = required;
    healthy_streak_ = 0U;
  } else {
    ++healthy_streak_;
    if (healthy_streak_ >= config_.recovery_cycles) {
      state_ = static_cast<SafetyState>(static_cast<std::uint8_t>(state_) - 1U);
      healthy_streak_ = 0U;
    }
  }

  Decision d;
  d.state = state_;
  d.speed_limit_mps = speedFor(state_);
  d.reasons = reasons;
  return d;
}

}  // namespace guardrail
