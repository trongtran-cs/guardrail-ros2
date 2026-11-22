// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// Deterministic fault injector (drop / delay / noise / clock skew / freeze).
// Seeded PRNG => reproducible fault campaigns in CI. Requirements GR-FLT-*.

#ifndef GUARDRAIL_CORE_FAULT_INJECTOR_HPP_
#define GUARDRAIL_CORE_FAULT_INJECTOR_HPP_

#include <cstdint>

namespace guardrail
{

struct FaultConfig
{
  double drop_probability{0.0};          ///< [0, 1]
  std::int64_t added_delay_ns{0};        ///< >= 0
  float noise_amplitude{0.0F};           ///< uniform noise in [-a, +a], >= 0
  std::int64_t clock_skew_ns{0};         ///< added to the stamp
  bool freeze{false};                    ///< re-emit the last delivered sample
};

struct Sample
{
  std::int64_t stamp_ns{0};
  float value{0.0F};
};

class FaultInjector
{
public:
  FaultInjector(const FaultConfig & config, std::uint64_t seed) noexcept;

  /// Apply faults to `sample` in place.
  /// @return false if the sample was dropped (caller must not forward it).
  [[nodiscard]] bool apply(Sample & sample) noexcept;

private:
  [[nodiscard]] double nextUnit() noexcept;  ///< uniform in [0, 1)

  FaultConfig config_;
  std::uint64_t state_;
  Sample last_delivered_{};
  bool has_last_{false};
};

}  // namespace guardrail

#endif  // GUARDRAIL_CORE_FAULT_INJECTOR_HPP_
