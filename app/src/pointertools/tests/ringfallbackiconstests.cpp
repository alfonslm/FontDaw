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

#include "internal/ringfallbackicons.h"

using namespace mu::pointertools;
using namespace muse::ui;

TEST(PointerTools_RingFallbackIconsTests, KnownCommandsGetTheirIcon)
{
    EXPECT_EQ(ringFallbackIcon("command://notation/open-edit-style"), IconCode::Code::BRUSH);
    EXPECT_EQ(ringFallbackIcon("command://notation/open-page-settings"), IconCode::Code::PAGE);
    EXPECT_EQ(ringFallbackIcon("command://notation/load-style"), IconCode::Code::OPEN_FILE);
}

TEST(PointerTools_RingFallbackIconsTests, ParametersAreIgnored)
{
    EXPECT_EQ(ringFallbackIcon("command://notation/load-style?x=1"), IconCode::Code::OPEN_FILE);
}

TEST(PointerTools_RingFallbackIconsTests, UnknownCommandsAndEmptyIdsGetNone)
{
    EXPECT_EQ(ringFallbackIcon("command://notation/something-unknown"), IconCode::Code::NONE);
    EXPECT_EQ(ringFallbackIcon(""), IconCode::Code::NONE);
    EXPECT_EQ(ringFallbackIcon("command://notation/open-edit-style-more"), IconCode::Code::NONE);
}
