// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// Fixed-capacity latency histogram for percentile / jitter budgets.
// No heap allocation; O(1) record, O(buckets) percentile. Requirements GR-LAT-*.

#ifndef GUARDRAIL_CORE_LATENCY_HISTOGRAM_HPP_
#define GUARDRAIL_CORE_LATENCY_HISTOGRAM_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

namespace guardrail
{

class LatencyHistogram
{
public:
  static constexpr std::size_t kBuckets = 256U;

  /// @param bucket_width_ns width of each linear bucket; must be > 0.
  ///        Values >= kBuckets * width land in the overflow (last) bucket.
  explicit LatencyHistogram(std::int64_t bucket_width_ns) noexcept;

  /// Record one sample. Negative samples are rejected and counted separately.
  void record(std::int64_t latency_ns) noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept {return count_;}
  [[nodiscard]] std::uint64_t rejected() const noexcept {return rejected_;}
  [[nodiscard]] std::int64_t max_ns() const noexcept {return max_ns_;}
  [[nodiscard]] std::int64_t min_ns() const noexcept {return (count_ == 0U) ? 0 : min_ns_;}

  /// Upper bound of the bucket containing percentile p in (0, 100].
  /// Conservative (never under-reports). Returns 0 if empty.
  [[nodiscard]] std::int64_t percentile_ns(double p) const noexcept;

  /// Jitter = p99 - p50 (upper-bound estimate).
  [[nodiscard]] std::int64_t jitter_ns() const noexcept;

private:
  std::int64_t width_ns_;
  std::array<std::uint64_t, kBuckets> buckets_{};
  std::uint64_t count_{0U};
  std::uint64_t rejected_{0U};
  std::int64_t max_ns_{0};
  std::int64_t min_ns_{0};
};

}  // namespace guardrail

#endif  // GUARDRAIL_CORE_LATENCY_HISTOGRAM_HPP_
