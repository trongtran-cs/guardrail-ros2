// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "guardrail_core/geometry.hpp"

namespace
{
using guardrail::Detection;
using guardrail::DetectionList;
using guardrail::estimateDistance;
using guardrail::summarise;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kFocal = 600.0F;
constexpr float kHeight = 1.7F;

DetectionList listOf(std::initializer_list<Detection> dets)
{
  DetectionList l;
  for (const Detection & d : dets) {
    l.items[l.count++] = d;
  }
  return l;
}
}  // namespace

// GR-PER-001
TEST(Geometry, PinholeDistance)
{
  const auto d = estimateDistance(510.0F, kFocal, kHeight);  // 600*1.7/510 = 2.0 m
  ASSERT_TRUE(d.valid);
  EXPECT_NEAR(d.metres, 2.0F, 1e-4F);
}

TEST(Geometry, InvalidArgumentsAreRejected)
{
  EXPECT_FALSE(estimateDistance(0.0F, kFocal, kHeight).valid);
  EXPECT_FALSE(estimateDistance(-5.0F, kFocal, kHeight).valid);
  EXPECT_FALSE(estimateDistance(kNaN, kFocal, kHeight).valid);
  EXPECT_FALSE(estimateDistance(100.0F, 0.0F, kHeight).valid);
  EXPECT_FALSE(estimateDistance(100.0F, kFocal, kNaN).valid);
  EXPECT_FALSE(estimateDistance(std::numeric_limits<float>::infinity(), kFocal, kHeight).valid);
}

TEST(Geometry, TinyBoxOverflowIsInvalidNotInfinite)
{
  // 1e-30 px box would give an enormous (possibly inf) distance.
  const auto d = estimateDistance(1e-30F, 1e30F, 1e10F);
  EXPECT_FALSE(d.valid);
}

// GR-PER-002
TEST(Summary, NoDetectionsMeansNoPersonAndFullConfidence)
{
  const auto s = summarise(DetectionList{}, kFocal, kHeight);
  EXPECT_LT(s.nearest_person_m, 0.0F);
  EXPECT_FLOAT_EQ(s.confidence, 1.0F);
  EXPECT_EQ(s.invalid_count, 0U);
}

TEST(Summary, NearestPersonWinsAndCarriesItsConfidence)
{
  const auto s = summarise(
    listOf({{200.0F, 0.9F}, {510.0F, 0.6F}, {300.0F, 0.8F}}), kFocal, kHeight);
  EXPECT_NEAR(s.nearest_person_m, 2.0F, 1e-4F);
  EXPECT_FLOAT_EQ(s.confidence, 0.6F);
}

// GR-PER-003
TEST(Summary, AnyInvalidDetectionForcesZeroConfidence)
{
  const auto s = summarise(listOf({{510.0F, 0.9F}, {kNaN, 0.9F}}), kFocal, kHeight);
  EXPECT_EQ(s.invalid_count, 1U);
  EXPECT_FLOAT_EQ(s.confidence, 0.0F);
  EXPECT_NEAR(s.nearest_person_m, 2.0F, 1e-4F);  // valid one is still reported
}

TEST(Summary, OutOfRangeConfidenceIsInvalid)
{
  const auto s = summarise(listOf({{510.0F, 1.5F}}), kFocal, kHeight);
  EXPECT_EQ(s.invalid_count, 1U);
  EXPECT_FLOAT_EQ(s.confidence, 0.0F);
  EXPECT_LT(s.nearest_person_m, 0.0F);
}

TEST(Summary, MalformedCountIsFailSafe)
{
  DetectionList l;
  l.count = guardrail::kMaxDetections + 1U;
  const auto s = summarise(l, kFocal, kHeight);
  EXPECT_FLOAT_EQ(s.confidence, 0.0F);
  EXPECT_EQ(s.invalid_count, 1U);
}
