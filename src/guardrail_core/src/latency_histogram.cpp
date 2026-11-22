// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include "guardrail_core/latency_histogram.hpp"

#include <cmath>

namespace guardrail
{

LatencyHistogram::LatencyHistogram(std::int64_t bucket_width_ns) noexcept
: width_ns_((bucket_width_ns > 0) ? bucket_width_ns : 1)
{
}

void LatencyHistogram::record(std::int64_t latency_ns) noexcept
{
  if (latency_ns < 0) {
    ++rejected_;
    return;
  }
  std::size_t idx = static_cast<std::size_t>(latency_ns / width_ns_);
  if (idx >= kBuckets) {
    idx = kBuckets - 1U;
  }
  ++buckets_[idx];
  if ((count_ == 0U) || (latency_ns < min_ns_)) {
    min_ns_ = latency_ns;
  }
  if (latency_ns > max_ns_) {
    max_ns_ = latency_ns;
  }
  ++count_;
}

std::int64_t LatencyHistogram::percentile_ns(double p) const noexcept
{
  if ((count_ == 0U) || !std::isfinite(p) || (p <= 0.0) || (p > 100.0)) {
    return 0;
  }
  const double target_real = std::ceil((p / 100.0) * static_cast<double>(count_));
  std::uint64_t target = static_cast<std::uint64_t>(target_real);
  if (target == 0U) {
    target = 1U;
  }
  std::uint64_t cumulative = 0U;
  for (std::size_t i = 0U; i < kBuckets; ++i) {
    cumulative += buckets_[i];
    if (cumulative >= target) {
      // Overflow bucket has no finite upper bound: report the true maximum.
      if (i == (kBuckets - 1U)) {
        return max_ns_;
      }
      return static_cast<std::int64_t>(i + 1U) * width_ns_;
    }
  }
  return max_ns_;
}

std::int64_t LatencyHistogram::jitter_ns() const noexcept
{
  const std::int64_t spread = percentile_ns(99.0) - percentile_ns(50.0);
  return (spread > 0) ? spread : 0;
}

}  // namespace guardrail
