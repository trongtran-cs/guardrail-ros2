// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include <gtest/gtest.h>

#include "guardrail_core/fault_injector.hpp"
#include "guardrail_core/latency_histogram.hpp"

namespace
{
using guardrail::FaultConfig;
using guardrail::FaultInjector;
using guardrail::LatencyHistogram;
using guardrail::Sample;
constexpr std::int64_t kMs = 1'000'000;
}  // namespace

// GR-LAT-001
TEST(LatencyHistogram, EmptyReturnsZero)
{
  LatencyHistogram h(100'000);
  EXPECT_EQ(h.count(), 0U);
  EXPECT_EQ(h.percentile_ns(99.0), 0);
  EXPECT_EQ(h.jitter_ns(), 0);
}

TEST(LatencyHistogram, PercentileIsConservativeUpperBound)
{
  LatencyHistogram h(1 * kMs);
  for (int i = 1; i <= 100; ++i) {
    h.record(static_cast<std::int64_t>(i) * kMs - 1);  // 0.999 ms .. 99.999 ms
  }
  // True p50 sample is ~50 ms; bucket upper bound must be >= it.
  EXPECT_GE(h.percentile_ns(50.0), 50 * kMs - 1);
  EXPECT_LE(h.percentile_ns(50.0), 51 * kMs);
  EXPECT_GE(h.percentile_ns(99.0), 99 * kMs - 1);
  EXPECT_EQ(h.max_ns(), 100 * kMs - 1);
}

TEST(LatencyHistogram, RejectsNegativeSamples)
{
  LatencyHistogram h(kMs);
  h.record(-5);
  EXPECT_EQ(h.count(), 0U);
  EXPECT_EQ(h.rejected(), 1U);
}

// GR-LAT-002: overflow bucket must not under-report.
TEST(LatencyHistogram, OverflowReportsTrueMax)
{
  LatencyHistogram h(1 * kMs);  // range 256 ms
  h.record(10 * kMs);
  h.record(900 * kMs);
  EXPECT_EQ(h.percentile_ns(100.0), 900 * kMs);
}

TEST(LatencyHistogram, InvalidPercentileReturnsZero)
{
  LatencyHistogram h(kMs);
  h.record(kMs);
  EXPECT_EQ(h.percentile_ns(0.0), 0);
  EXPECT_EQ(h.percentile_ns(101.0), 0);
}

// GR-FLT-001
TEST(FaultInjector, DeterministicForSameSeed)
{
  FaultConfig c;
  c.drop_probability = 0.3;
  c.noise_amplitude = 0.5F;
  FaultInjector a(c, 42U);
  FaultInjector b(c, 42U);
  for (int i = 0; i < 200; ++i) {
    Sample sa{i * kMs, 1.0F};
    Sample sb{i * kMs, 1.0F};
    const bool da = a.apply(sa);
    const bool db = b.apply(sb);
    ASSERT_EQ(da, db);
    if (da) {
      ASSERT_EQ(sa.stamp_ns, sb.stamp_ns);
      ASSERT_FLOAT_EQ(sa.value, sb.value);
    }
  }
}

TEST(FaultInjector, DropAllAndDropNone)
{
  FaultConfig all;
  all.drop_probability = 1.0;
  FaultInjector fa(all, 1U);
  Sample s{0, 0.0F};
  EXPECT_FALSE(fa.apply(s));

  FaultInjector fn(FaultConfig{}, 1U);
  for (int i = 0; i < 100; ++i) {
    Sample t{i, 0.0F};
    EXPECT_TRUE(fn.apply(t));
  }
}

TEST(FaultInjector, DelayAndSkewShiftStamp)
{
  FaultConfig c;
  c.added_delay_ns = 5 * kMs;
  c.clock_skew_ns = -2 * kMs;
  FaultInjector f(c, 7U);
  Sample s{100 * kMs, 0.0F};
  ASSERT_TRUE(f.apply(s));
  EXPECT_EQ(s.stamp_ns, 103 * kMs);
}

TEST(FaultInjector, FreezeReplaysLastSample)
{
  FaultConfig c;
  FaultInjector f(c, 9U);
  Sample first{10 * kMs, 3.0F};
  ASSERT_TRUE(f.apply(first));

  FaultConfig frozen;
  frozen.freeze = true;
  FaultInjector g(frozen, 9U);
  Sample a{10 * kMs, 3.0F};
  ASSERT_TRUE(g.apply(a));  // nothing to replay yet: passes through
  Sample b{20 * kMs, 9.0F};
  ASSERT_TRUE(g.apply(b));
  EXPECT_EQ(b.stamp_ns, 10 * kMs);
  EXPECT_FLOAT_EQ(b.value, 3.0F);
}

TEST(FaultInjector, ZeroSeedIsHandled)
{
  FaultConfig c;
  c.drop_probability = 0.5;
  FaultInjector f(c, 0U);
  int delivered = 0;
  for (int i = 0; i < 1000; ++i) {
    Sample s{i, 0.0F};
    if (f.apply(s)) {++delivered;}
  }
  EXPECT_GT(delivered, 300);
  EXPECT_LT(delivered, 700);
}
