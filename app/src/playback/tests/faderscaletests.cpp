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

#include "playback/internal/faderscale.h"

using namespace mu::playback;

//! The points VolumeSlider.qml puts on its travel: the meter must use the same ones
TEST(Playback_FaderScaleTests, KnownPoints)
{
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(-60.0), 0.0);
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(-12.0), 0.5);
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(0.0), 0.75);
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(12.0), 1.0);
}

//! Outside the scale the position is clamped: silence at the bottom, more than +12 dB at the top
TEST(Playback_FaderScaleTests, Clamped)
{
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(-100.0), 0.0);
    EXPECT_DOUBLE_EQ(FaderScale::positionFromDb(30.0), 1.0);
}

TEST(Playback_FaderScaleTests, MonotonicAndContinuous)
{
    double previous = FaderScale::positionFromDb(FaderScale::MIN_DB);
    for (double db = FaderScale::MIN_DB + 0.1; db <= FaderScale::MAX_DB; db += 0.1) {
        const double position = FaderScale::positionFromDb(db);
        EXPECT_GT(position, previous) << db;
        EXPECT_LT(position - previous, 0.01) << db; // no jump where the two halves meet
        previous = position;
    }
}
