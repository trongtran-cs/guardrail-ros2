// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include "guardrail_core/fault_injector.hpp"

namespace guardrail
{

namespace
{
constexpr std::uint64_t kFallbackSeed = 0x9E3779B97F4A7C15ULL;
}  // namespace

FaultInjector::FaultInjector(const FaultConfig & config, std::uint64_t seed) noexcept
: config_(config), state_((seed != 0U) ? seed : kFallbackSeed)
{
}

double FaultInjector::nextUnit() noexcept
{
  // xorshift64* — small, deterministic, not for cryptographic use.
  state_ ^= state_ >> 12U;
  state_ ^= state_ << 25U;
  state_ ^= state_ >> 27U;
  const std::uint64_t r = state_ * 0x2545F4914F6CDD1DULL;
  // Top 53 bits -> [0, 1).
  return static_cast<double>(r >> 11U) * (1.0 / 9007199254740992.0);
}

bool FaultInjector::apply(Sample & sample) noexcept
{
  if (nextUnit() < config_.drop_probability) {
    return false;
  }

  if (config_.freeze && has_last_) {
    sample = last_delivered_;
    return true;
  }

  sample.stamp_ns += config_.added_delay_ns + config_.clock_skew_ns;
  if (config_.noise_amplitude > 0.0F) {
    const double u = (nextUnit() * 2.0) - 1.0;
    sample.value += static_cast<float>(u) * config_.noise_amplitude;
  }

  last_delivered_ = sample;
  has_last_ = true;
  return true;
}

}  // namespace guardrail
