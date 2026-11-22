// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "guardrail_core/supervisor.hpp"

namespace
{

using guardrail::Config;
using guardrail::Decision;
using guardrail::Inputs;
using guardrail::SafetyState;
using guardrail::Supervisor;

constexpr std::int64_t kMs = 1'000'000;

Inputs healthy(std::int64_t now_ns)
{
  Inputs in;
  in.now_ns = now_ns;
  in.last_frame_ns = now_ns - (10 * kMs);
  in.confidence = 0.9F;
  in.nearest_person_m = -1.0F;
  in.pipeline_latency_ns = 20 * kMs;
  return in;
}

// Drive supervisor with healthy inputs until it reaches nominal.
void settle(Supervisor & s, std::int64_t & now)
{
  for (int i = 0; i < 20; ++i) {
    now += 33 * kMs;
    (void)s.update(healthy(now));
  }
}

}  // namespace

// GR-SUP-001
TEST(Supervisor, StartsInSafeStop)
{
  Supervisor s{Config{}};
  EXPECT_EQ(s.state(), SafetyState::kSafeStop);
}

// GR-SUP-007: recovery needs a streak, one level at a time.
TEST(Supervisor, RecoversOneLevelPerStreak)
{
  Config c;
  c.recovery_cycles = 3U;
  Supervisor s{c};
  std::int64_t now = 1'000 * kMs;

  for (int i = 0; i < 2; ++i) {
    now += 33 * kMs;
    EXPECT_EQ(s.update(healthy(now)).state, SafetyState::kSafeStop);
  }
  now += 33 * kMs;
  EXPECT_EQ(s.update(healthy(now)).state, SafetyState::kDegraded);
  for (int i = 0; i < 2; ++i) {
    now += 33 * kMs;
    EXPECT_EQ(s.update(healthy(now)).state, SafetyState::kDegraded);
  }
  now += 33 * kMs;
  EXPECT_EQ(s.update(healthy(now)).state, SafetyState::kNominal);
}

// GR-SUP-003
TEST(Supervisor, StaleDataDegradesThenStops)
{
  Supervisor s{Config{}};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);
  ASSERT_EQ(s.state(), SafetyState::kNominal);

  Inputs in = healthy(now);
  in.last_frame_ns = now - (300 * kMs);
  Decision d = s.update(in);
  EXPECT_EQ(d.state, SafetyState::kDegraded);
  EXPECT_NE(d.reasons & guardrail::kReasonStaleData, 0U);

  in.last_frame_ns = now - (600 * kMs);
  d = s.update(in);
  EXPECT_EQ(d.state, SafetyState::kSafeStop);
  EXPECT_FLOAT_EQ(d.speed_limit_mps, 0.0F);
  EXPECT_NE(d.reasons & guardrail::kReasonStaleDataCritical, 0U);
}

// GR-SUP-002
TEST(Supervisor, NonFiniteInputsAreFaults)
{
  Supervisor s{Config{}};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);
  ASSERT_EQ(s.state(), SafetyState::kNominal);

  Inputs in = healthy(now);
  in.confidence = std::numeric_limits<float>::quiet_NaN();
  Decision d = s.update(in);
  EXPECT_EQ(d.state, SafetyState::kSafeStop);
  EXPECT_NE(d.reasons & guardrail::kReasonInvalidInput, 0U);
}

TEST(Supervisor, FutureFrameStampIsInvalid)
{
  Supervisor s{Config{}};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);
  Inputs in = healthy(now);
  in.last_frame_ns = now + kMs;  // clock skew / bad stamp
  EXPECT_EQ(s.update(in).state, SafetyState::kSafeStop);
}

// GR-SUP-004
TEST(Supervisor, LatencyOverBudgetDegrades)
{
  Supervisor s{Config{}};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);
  Inputs in = healthy(now);
  in.pipeline_latency_ns = 150 * kMs;
  const Decision d = s.update(in);
  EXPECT_EQ(d.state, SafetyState::kDegraded);
  EXPECT_NE(d.reasons & guardrail::kReasonLatencyBudget, 0U);
}

// GR-SUP-005
TEST(Supervisor, PersonProximityRules)
{
  Supervisor s{Config{}};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);

  Inputs in = healthy(now);
  in.nearest_person_m = 1.5F;
  EXPECT_EQ(s.update(in).state, SafetyState::kDegraded);

  in.nearest_person_m = 0.5F;
  const Decision d = s.update(in);
  EXPECT_EQ(d.state, SafetyState::kSafeStop);
  EXPECT_NE(d.reasons & guardrail::kReasonPersonVeryNear, 0U);
}

TEST(Supervisor, SpeedLimitMatchesState)
{
  Config c;
  Supervisor s{c};
  std::int64_t now = 1'000 * kMs;
  settle(s, now);
  Decision d = s.update(healthy(now));
  EXPECT_FLOAT_EQ(d.speed_limit_mps, c.nominal_speed_mps);
}
