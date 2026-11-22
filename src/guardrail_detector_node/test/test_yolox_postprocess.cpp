// Copyright 2026 GuardRail-ROS contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "guardrail_detector/yolox_postprocess.hpp"

using guardrail_detector::decodeYoloxPersons;
using guardrail_detector::iou;
using guardrail_detector::kYoloxCols;
using guardrail_detector::nonMaxSuppression;
using guardrail_detector::ScoredBox;
using guardrail_detector::yoloxExpectedRows;

namespace
{
// Tensor of the right size with everything below any threshold.
std::vector<float> emptyTensor(int w, int h)
{
  const std::size_t rows = yoloxExpectedRows(w, h);
  return std::vector<float>(rows * kYoloxCols, 0.0F);
}

void setRow(
  std::vector<float> & t, std::size_t row, float tx, float ty, float lw, float lh, float obj,
  float person)
{
  float * p = t.data() + (row * kYoloxCols);
  p[0] = tx;
  p[1] = ty;
  p[2] = lw;
  p[3] = lh;
  p[4] = obj;
  p[5] = person;
}
}  // namespace

// GR-DET-001
TEST(YoloxPostprocess, ExpectedRowsMatchesYoloxNanoLayout)
{
  EXPECT_EQ(yoloxExpectedRows(416, 416), 3549U);  // 52^2 + 26^2 + 13^2
  EXPECT_EQ(yoloxExpectedRows(640, 640), 8400U);
  EXPECT_EQ(yoloxExpectedRows(417, 416), 0U);
  EXPECT_EQ(yoloxExpectedRows(0, 416), 0U);
}

// GR-DET-002
TEST(YoloxPostprocess, DecodesGridAndStrideForFirstRowOfEachScale)
{
  auto t = emptyTensor(416, 416);
  // Row 0: stride 8, cell (0,0). tx=ty=0.5 -> centre (4,4); log w/h = ln(2) -> 16 px.
  setRow(t, 0, 0.5F, 0.5F, std::log(2.0F), std::log(2.0F), 1.0F, 0.9F);
  // First row of the stride-16 block: index 52*52 = 2704, cell (0,0).
  setRow(t, 2704, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.8F);  // centre (16,16), size 16
  // Stride 32, cell (x=2, y=1): index 2704+676 + 1*13 + 2.
  setRow(t, 2704 + 676 + 15, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.7F);  // centre (64,32), size 32

  const auto r = decodeYoloxPersons(t.data(), yoloxExpectedRows(416, 416), 416, 416, 0.3F);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ(r.boxes.size(), 3U);
  EXPECT_NEAR(r.boxes[0].x1, -4.0F, 1e-4F);
  EXPECT_NEAR(r.boxes[0].x2, 12.0F, 1e-4F);
  EXPECT_NEAR(r.boxes[0].score, 0.9F, 1e-6F);
  EXPECT_NEAR(r.boxes[1].x1, 8.0F, 1e-4F);
  EXPECT_NEAR(r.boxes[1].y2, 24.0F, 1e-4F);
  EXPECT_NEAR(r.boxes[2].x1, 48.0F, 1e-4F);   // centre x = (0+2)*32 = 64, half width 16
  EXPECT_NEAR(r.boxes[2].y1, 16.0F, 1e-4F);   // centre y = (0+1)*32 = 32
}

TEST(YoloxPostprocess, ScoreIsObjectnessTimesPersonAndThresholdApplies)
{
  auto t = emptyTensor(416, 416);
  setRow(t, 10, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 0.5F);  // 0.25 < 0.3
  setRow(t, 11, 0.0F, 0.0F, 0.0F, 0.0F, 0.9F, 0.5F);  // 0.45
  const auto r = decodeYoloxPersons(t.data(), yoloxExpectedRows(416, 416), 416, 416, 0.3F);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ(r.boxes.size(), 1U);
  EXPECT_NEAR(r.boxes[0].score, 0.45F, 1e-6F);
}

// GR-DET-003
TEST(YoloxPostprocess, MalformedInputsAreRejectedOrDropped)
{
  auto t = emptyTensor(416, 416);
  EXPECT_FALSE(decodeYoloxPersons(nullptr, 3549U, 416, 416, 0.3F).ok);
  EXPECT_FALSE(decodeYoloxPersons(t.data(), 100U, 416, 416, 0.3F).ok);   // wrong row count
  EXPECT_FALSE(decodeYoloxPersons(t.data(), 3549U, 417, 416, 0.3F).ok);  // size not /32

  setRow(t, 5, std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F, 0.0F, 1.0F, 1.0F);
  setRow(t, 6, 0.0F, 0.0F, 1000.0F, 0.0F, 1.0F, 1.0F);  // exp overflow -> inf
  setRow(t, 7, 0.0F, 0.0F, 0.0F, 0.0F, std::numeric_limits<float>::quiet_NaN(), 1.0F);
  const auto r = decodeYoloxPersons(t.data(), 3549U, 416, 416, 0.3F);
  ASSERT_TRUE(r.ok);
  EXPECT_TRUE(r.boxes.empty());
}

// GR-DET-004
TEST(YoloxNms, IouBasics)
{
  const ScoredBox a{0, 0, 10, 10, 0.9F};
  const ScoredBox b{5, 0, 15, 10, 0.8F};
  EXPECT_NEAR(iou(a, b), 50.0F / 150.0F, 1e-5F);
  EXPECT_FLOAT_EQ(iou(a, ScoredBox{20, 20, 30, 30, 0.5F}), 0.0F);
  EXPECT_FLOAT_EQ(iou(ScoredBox{0, 0, 0, 0, 0.1F}, ScoredBox{0, 0, 0, 0, 0.1F}), 0.0F);
}

TEST(YoloxNms, KeepsHighestScoreAndDropsOverlaps)
{
  const std::vector<ScoredBox> in = {
    {0, 0, 10, 10, 0.6F}, {1, 1, 11, 11, 0.9F}, {50, 50, 60, 60, 0.7F}, {0, 0, 10, 10, 0.5F}};
  const auto out = nonMaxSuppression(in, 0.45F);
  ASSERT_EQ(out.size(), 2U);
  EXPECT_FLOAT_EQ(out[0].score, 0.9F);
  EXPECT_FLOAT_EQ(out[1].score, 0.7F);
}

TEST(YoloxNms, EmptyInputIsFine)
{
  EXPECT_TRUE(nonMaxSuppression({}, 0.45F).empty());
}
