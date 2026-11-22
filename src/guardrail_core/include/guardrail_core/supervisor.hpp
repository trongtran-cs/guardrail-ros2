// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// Safety supervisor state machine. Pure C++17, ROS-independent, no exceptions,
// no dynamic allocation. Requirements: see docs/requirements.md (GR-SUP-*).

#ifndef GUARDRAIL_CORE_SUPERVISOR_HPP_
#define GUARDRAIL_CORE_SUPERVISOR_HPP_

#include <cstdint>

namespace guardrail
{

/// Supervisor output state, ordered by severity (higher = more restrictive).
enum class SafetyState : std::uint8_t
{
  kNominal = 0U,
  kDegraded = 1U,
  kSafeStop = 2U
};

/// Bit flags explaining why the supervisor left kNominal (GR-SUP-006).
enum ReasonFlag : std::uint32_t
{
  kReasonNone = 0U,
  kReasonStaleData = 1U << 0U,
  kReasonInvalidInput = 1U << 1U,
  kReasonLatencyBudget = 1U << 2U,
  kReasonLowConfidence = 1U << 3U,
  kReasonPersonNear = 1U << 4U,
  kReasonPersonVeryNear = 1U << 5U,
  kReasonStaleDataCritical = 1U << 6U
};

/// One perception cycle's health inputs. Distances < 0 mean "no person seen".
struct Inputs
{
  std::int64_t now_ns{0};
  std::int64_t last_frame_ns{0};      ///< stamp of newest perception data
  float confidence{1.0F};             ///< detector confidence in [0, 1]
  float nearest_person_m{-1.0F};      ///< metres, negative if none
  std::int64_t pipeline_latency_ns{0};  ///< last end-to-end latency
};

/// Tunable thresholds. Defaults are illustrative, not validated (see HARA).
struct Config
{
  std::int64_t stale_degraded_ns{200'000'000};   ///< 200 ms
  std::int64_t stale_stop_ns{500'000'000};       ///< 500 ms
  std::int64_t latency_budget_ns{100'000'000};   ///< 100 ms
  float min_confidence{0.40F};
  float slow_distance_m{2.0F};
  float stop_distance_m{0.8F};
  float degraded_speed_mps{0.3F};
  float nominal_speed_mps{1.0F};
  std::uint32_t recovery_cycles{5U};  ///< consecutive healthy cycles per level
};

struct Decision
{
  SafetyState state{SafetyState::kSafeStop};
  float speed_limit_mps{0.0F};
  std::uint32_t reasons{kReasonNone};
};

/// Fail-safe: a freshly constructed supervisor starts in kSafeStop (GR-SUP-001).
class Supervisor
{
public:
  explicit Supervisor(const Config & config) noexcept;

  /// Evaluate one cycle. Deterministic and O(1). Never throws.
  [[nodiscard]] Decision update(const Inputs & in) noexcept;

  [[nodiscard]] SafetyState state() const noexcept {return state_;}

private:
  [[nodiscard]] float speedFor(SafetyState s) const noexcept;

  Config config_;
  SafetyState state_{SafetyState::kSafeStop};
  std::uint32_t healthy_streak_{0U};
};

}  // namespace guardrail

#endif  // GUARDRAIL_CORE_SUPERVISOR_HPP_
