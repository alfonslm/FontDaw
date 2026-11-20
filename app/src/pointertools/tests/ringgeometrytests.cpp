/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * FontDaw
 * The notation-first DAW
 *
 * Copyright (C) 2026 FontDaw contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "internal/ringgeometry.h"

using namespace mu::pointertools;

namespace {
constexpr double HOLE = 24.0;
constexpr double MAXR = 108.0;
constexpr double PI = 3.14159265358979323846;

// a point at `degrees` clockwise from straight up and `radius` from the centre
int at(double degrees, int count, double radius = 60.0)
{
    const double r = degrees * PI / 180.0;
    return ringSegmentAt(radius * std::sin(r), -radius * std::cos(r), count, HOLE, MAXR);
}
}

class Pointertools_RingGeometryTests : public ::testing::Test
{
};

TEST_F(Pointertools_RingGeometryTests, EightSlots_MoreIsStraightDown)
{
    EXPECT_EQ(ringSegmentAt(0, 60, 8, HOLE, MAXR), 7);
}

TEST_F(Pointertools_RingGeometryTests, EightSlots_StraightUpIsIndexThree)
{
    EXPECT_EQ(ringSegmentAt(0, -60, 8, HOLE, MAXR), 3);
}

TEST_F(Pointertools_RingGeometryTests, EightSlots_AfterMoreTowardsTheWestIsIndexZero)
{
    // slot 0 is centred at 225 degrees: down and to the left
    EXPECT_EQ(at(225, 8), 0);
    EXPECT_EQ(ringSegmentAt(-40, 40, 8, HOLE, MAXR), 0);
    // and just left of straight down is still "More…"
    EXPECT_EQ(ringSegmentAt(-5, 60, 8, HOLE, MAXR), 7);
}

TEST_F(Pointertools_RingGeometryTests, EightSlots_ClockwiseOrder)
{
    // 270 = west is slot 1, 315 = up-left slot 2, 0 = up slot 3, 45 slot 4, 90 east slot 5, 135 slot 6
    EXPECT_EQ(at(270, 8), 1);
    EXPECT_EQ(at(315, 8), 2);
    EXPECT_EQ(at(0, 8), 3);
    EXPECT_EQ(at(45, 8), 4);
    EXPECT_EQ(at(90, 8), 5);
    EXPECT_EQ(at(135, 8), 6);
    EXPECT_EQ(at(180, 8), 7);
}

TEST_F(Pointertools_RingGeometryTests, EightSlots_SegmentBoundaries)
{
    // "More…" spans 157.5 to 202.5 degrees
    EXPECT_EQ(at(157.0, 8), 6);
    EXPECT_EQ(at(158.0, 8), 7);
    EXPECT_EQ(at(202.0, 8), 7);
    EXPECT_EQ(at(203.0, 8), 0);
    // the wrap through straight up: slot 3 spans -22.5 to 22.5
    EXPECT_EQ(at(-21.0, 8), 3);
    EXPECT_EQ(at(21.0, 8), 3);
    EXPECT_EQ(at(24.0, 8), 4);
    EXPECT_EQ(at(-24.0, 8), 2);
}

TEST_F(Pointertools_RingGeometryTests, InsideTheHole_IsNone)
{
    EXPECT_EQ(ringSegmentAt(0, 0, 8, HOLE, MAXR), -1);
    EXPECT_EQ(ringSegmentAt(0, 23.0, 8, HOLE, MAXR), -1);
    EXPECT_EQ(ringSegmentAt(0, 25.0, 8, HOLE, MAXR), 7);
}

TEST_F(Pointertools_RingGeometryTests, BeyondMaxRadius_IsNone)
{
    EXPECT_EQ(ringSegmentAt(0, 109.0, 8, HOLE, MAXR), -1);
    EXPECT_EQ(ringSegmentAt(0, 107.0, 8, HOLE, MAXR), 7);
    EXPECT_EQ(ringSegmentAt(-500, 0, 8, HOLE, MAXR), -1);
}

TEST_F(Pointertools_RingGeometryTests, TwoSlots_TopHalfAndBottomHalf)
{
    // "More…" is the bottom half, slot 0 the top half
    EXPECT_EQ(ringSegmentAt(0, 60, 2, HOLE, MAXR), 1);
    EXPECT_EQ(ringSegmentAt(50, 20, 2, HOLE, MAXR), 1);
    EXPECT_EQ(ringSegmentAt(-50, 20, 2, HOLE, MAXR), 1);
    EXPECT_EQ(ringSegmentAt(0, -60, 2, HOLE, MAXR), 0);
    EXPECT_EQ(ringSegmentAt(50, -20, 2, HOLE, MAXR), 0);
    EXPECT_EQ(ringSegmentAt(-50, -20, 2, HOLE, MAXR), 0);
}

TEST_F(Pointertools_RingGeometryTests, OneSlot_EverythingOutsideTheHoleIsZero)
{
    EXPECT_EQ(ringSegmentAt(0, 60, 1, HOLE, MAXR), 0);
    EXPECT_EQ(ringSegmentAt(0, -60, 1, HOLE, MAXR), 0);
    EXPECT_EQ(ringSegmentAt(-60, 3, 1, HOLE, MAXR), 0);
    EXPECT_EQ(ringSegmentAt(0, 0, 1, HOLE, MAXR), -1);
}

TEST_F(Pointertools_RingGeometryTests, NoSlots_IsNone)
{
    EXPECT_EQ(ringSegmentAt(0, 60, 0, HOLE, MAXR), -1);
}

TEST_F(Pointertools_RingGeometryTests, SegmentCentre_MatchesSegmentAt)
{
    for (int count = 1; count <= 8; ++count) {
        for (int i = 0; i < count; ++i) {
            const QPointF c = ringSegmentCentre(i, count, 60.0);
            EXPECT_EQ(ringSegmentAt(c.x(), c.y(), count, HOLE, MAXR), i) << "count " << count << " index " << i;
            EXPECT_NEAR(std::hypot(c.x(), c.y()), 60.0, 1e-9);
        }
    }
}

TEST_F(Pointertools_RingGeometryTests, SegmentCentre_MoreIsStraightDown)
{
    const QPointF c = ringSegmentCentre(7, 8, 60.0);
    EXPECT_NEAR(c.x(), 0.0, 1e-9);
    EXPECT_NEAR(c.y(), 60.0, 1e-9);
}

// Review of #195, finding 1: the ring's centre is moved to keep the ring inside the view while the
// pointer stays put, so near an edge the pointer can be over a segment at once. A release where the
// ring was opened must never run anything, wherever the ring had to be moved.
TEST_F(Pointertools_RingGeometryTests, OpeningReleaseAtTheOpeningPointNeverChooses)
{
    const QPointF openPos(10.0, 790.0); // near the bottom-left corner of a view
    for (int highlighted : { -1, 0, 3, 7 }) {
        EXPECT_FALSE(ringOpeningReleaseChooses(openPos, openPos, 24.0, highlighted));
        EXPECT_FALSE(ringOpeningReleaseChooses(openPos, openPos + QPointF(10.0, -10.0), 24.0, highlighted));
    }
}

TEST_F(Pointertools_RingGeometryTests, OpeningReleaseChoosesOnlyTheHighlightedSegmentAfterAMove)
{
    const QPointF openPos(400.0, 300.0);
    EXPECT_TRUE(ringOpeningReleaseChooses(openPos, openPos + QPointF(-40.0, 0.0), 24.0, 2));
    EXPECT_TRUE(ringOpeningReleaseChooses(openPos, openPos + QPointF(0.0, 24.0), 24.0, 0));
    // moved out, but nothing highlighted (the pointer left the view): nothing runs
    EXPECT_FALSE(ringOpeningReleaseChooses(openPos, openPos + QPointF(-400.0, 0.0), 24.0, -1));
}


// FD-156: the width a slot's name may have, as in the approved pictures' renderer (ring()).
// There, index 0 is "More…" at 6 o'clock and the others follow clockwise, so a slot's `index`
// here is the renderer's index minus one (More… is the last slot).
namespace {
constexpr double R_IN = 24.0;
constexpr double R_OUT = 92.0;
constexpr double CAP = 84.0;

// the renderer's formula, with its own index
double rendererWidth(int i, int count)
{
    const double step = 2.0 * PI / count;
    const double mid = (R_IN + R_OUT) / 2.0;
    const double chord = 2.0 * mid * std::sin(step / 2.0) - 6.0;
    const double depth = R_OUT - R_IN - 8.0;
    const double side = std::abs(std::sin(i * step));
    return std::min(chord + side * (depth - chord), CAP);
}
}

TEST_F(Pointertools_RingGeometryTests, LabelWidth_MatchesTheRenderer)
{
    for (int count = 1; count <= 8; ++count) {
        for (int index = 0; index < count; ++index) {
            EXPECT_NEAR(ringLabelWidth(index, count, R_IN, R_OUT, CAP), rendererWidth((index + 1) % count, count), 1e-9)
                << "count " << count << " index " << index;
        }
    }
}

TEST_F(Pointertools_RingGeometryTests, LabelWidth_EightSegments_TopBottomAndSides)
{
    // mid = 58, step = 45 degrees: chord = 116 * sin(22.5 deg) - 6 = 38.39, depth = 60
    const double chord = 116.0 * std::sin(PI / 8.0) - 6.0;
    EXPECT_NEAR(chord, 38.3905, 1e-3);
    // "More…" (last) is straight down, slot 3 straight up: the chord
    EXPECT_NEAR(ringLabelWidth(7, 8, R_IN, R_OUT, CAP), chord, 1e-9);
    EXPECT_NEAR(ringLabelWidth(3, 8, R_IN, R_OUT, CAP), chord, 1e-9);
    // slot 1 is west and slot 5 east: the depth
    EXPECT_NEAR(ringLabelWidth(1, 8, R_IN, R_OUT, CAP), 60.0, 1e-9);
    EXPECT_NEAR(ringLabelWidth(5, 8, R_IN, R_OUT, CAP), 60.0, 1e-9);
    // the diagonals lie in between, and the ring is mirror-symmetric
    const double diagonal = ringLabelWidth(0, 8, R_IN, R_OUT, CAP);
    EXPECT_GT(diagonal, chord);
    EXPECT_LT(diagonal, 60.0);
    EXPECT_NEAR(ringLabelWidth(2, 8, R_IN, R_OUT, CAP), diagonal, 1e-9);
    EXPECT_NEAR(ringLabelWidth(4, 8, R_IN, R_OUT, CAP), diagonal, 1e-9);
    EXPECT_NEAR(ringLabelWidth(6, 8, R_IN, R_OUT, CAP), diagonal, 1e-9);
}

TEST_F(Pointertools_RingGeometryTests, LabelWidth_FourSegments)
{
    // step = 90 degrees: chord = 116 * sin(45 deg) - 6 = 76.02, depth = 60
    const double chord = 116.0 * std::sin(PI / 4.0) - 6.0;
    // "More…" (3) down and slot 1 up: the chord; slots 0 and 2 are west and east: the depth
    EXPECT_NEAR(ringLabelWidth(3, 4, R_IN, R_OUT, CAP), chord, 1e-9);
    EXPECT_NEAR(ringLabelWidth(1, 4, R_IN, R_OUT, CAP), chord, 1e-9);
    EXPECT_NEAR(ringLabelWidth(0, 4, R_IN, R_OUT, CAP), 60.0, 1e-9);
    EXPECT_NEAR(ringLabelWidth(2, 4, R_IN, R_OUT, CAP), 60.0, 1e-9);
}

TEST_F(Pointertools_RingGeometryTests, LabelWidth_IsCappedAndHandlesFewSlots)
{
    // two slots: the chord is 110, over the cap at top and bottom
    EXPECT_NEAR(ringLabelWidth(1, 2, R_IN, R_OUT, CAP), CAP, 1e-9);
    EXPECT_NEAR(ringLabelWidth(0, 2, R_IN, R_OUT, CAP), CAP, 1e-9);
    // a smaller cap wins anywhere
    EXPECT_NEAR(ringLabelWidth(3, 8, R_IN, R_OUT, 30.0), 30.0, 1e-9);
    EXPECT_EQ(ringLabelWidth(0, 0, R_IN, R_OUT, CAP), 0.0);
}
