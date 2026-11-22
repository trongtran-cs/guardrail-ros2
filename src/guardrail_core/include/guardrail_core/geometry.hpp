// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// Detector-independent perception maths: monocular distance estimate and the reduction of a
// detection list to the (nearest_person_m, confidence) pair the supervisor consumes.
// Pure C++17, no heap, no exceptions. Requirements: GR-PER-*.

#ifndef GUARDRAIL_CORE_GEOMETRY_HPP_
#define GUARDRAIL_CORE_GEOMETRY_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

namespace guardrail
{

struct Distance
{
  bool valid{false};
  float metres{-1.0F};
};

/// Pinhole estimate d = f_px * H / h_px. Rough (assumes an upright person of known height,
/// roughly fronto-parallel, no lens distortion); suitable for coarse slow/stop zoning only.
/// Invalid (valid == false) for any non-finite or non-positive argument.
[[nodiscard]] Distance estimateDistance(
  float bbox_height_px, float focal_px, float person_height_m) noexcept;

struct Detection
{
  float bbox_height_px{0.0F};  ///< pixels, in the same image the focal length refers to
  float confidence{0.0F};      ///< normalised to [0, 1] by the detector backend
};

constexpr std::size_t kMaxDetections = 32U;

struct DetectionList
{
  std::array<Detection, kMaxDetections> items{};
  std::size_t count{0U};  ///< valid entries in `items`; values > kMaxDetections are rejected
};

struct Summary
{
  float nearest_person_m{-1.0F};  ///< < 0 when no valid person detection
  float confidence{1.0F};         ///< confidence of the nearest person; 1.0 if none seen
  std::uint32_t invalid_count{0U};
};

/// Reduce detections to the supervisor inputs.
/// Fail-safe rule (GR-PER-003): if any detection is invalid, or the list is malformed,
/// confidence is forced to 0 so the supervisor degrades rather than trusting partial data.
[[nodiscard]] Summary summarise(
  const DetectionList & list, float focal_px, float person_height_m) noexcept;

}  // namespace guardrail

#endif  // GUARDRAIL_CORE_GEOMETRY_HPP_
