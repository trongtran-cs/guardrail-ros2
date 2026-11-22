// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include "guardrail_core/geometry.hpp"

#include <cmath>

namespace guardrail
{

namespace
{
bool positiveFinite(float v) noexcept
{
  return std::isfinite(v) && (v > 0.0F);
}
}  // namespace

Distance estimateDistance(float bbox_height_px, float focal_px, float person_height_m) noexcept
{
  Distance d;
  if (!positiveFinite(bbox_height_px) || !positiveFinite(focal_px) ||
    !positiveFinite(person_height_m))
  {
    return d;
  }
  const float metres = (focal_px * person_height_m) / bbox_height_px;
  if (!std::isfinite(metres)) {
    return d;
  }
  d.valid = true;
  d.metres = metres;
  return d;
}

Summary summarise(const DetectionList & list, float focal_px, float person_height_m) noexcept
{
  Summary s;
  if (list.count > kMaxDetections) {
    s.confidence = 0.0F;
    s.invalid_count = 1U;
    return s;
  }

  bool have_nearest = false;
  for (std::size_t i = 0U; i < list.count; ++i) {
    const Detection & det = list.items[i];
    const Distance dist = estimateDistance(det.bbox_height_px, focal_px, person_height_m);
    const bool conf_ok = std::isfinite(det.confidence) && (det.confidence >= 0.0F) &&
      (det.confidence <= 1.0F);
    if (!dist.valid || !conf_ok) {
      ++s.invalid_count;
      continue;
    }
    if (!have_nearest || (dist.metres < s.nearest_person_m)) {
      have_nearest = true;
      s.nearest_person_m = dist.metres;
      s.confidence = det.confidence;
    }
  }

  if (s.invalid_count != 0U) {
    s.confidence = 0.0F;  // GR-PER-003
  }
  return s;
}

}  // namespace guardrail
