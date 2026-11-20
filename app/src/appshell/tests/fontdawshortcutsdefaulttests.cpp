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

#include "appshell/internal/fontdawshortcutsdefaultrule.h"

using namespace mu::appshell;

// FD-153: the MuseScore 4.7 layout becomes the current one once, at first start, and only for a
// user who has not chosen anything.
namespace {
const std::string WANTED("shortcuts_musescore47");
const std::vector<std::string> AVAILABLE { "shortcuts_musescore47", "shortcuts_azerty" };
}

TEST(AppShell_FontDawShortcutsDefaultTests, FirstStart_SetsTheDefault)
{
    EXPECT_EQ(fontDawShortcutsPresetToApply(false, false, "", AVAILABLE, WANTED), WANTED);
}

TEST(AppShell_FontDawShortcutsDefaultTests, AppliedBefore_SetsNothing)
{
    // a user who chose "Default" (the 5.0 keys) after the first start keeps it
    EXPECT_EQ(fontDawShortcutsPresetToApply(true, false, "", AVAILABLE, WANTED), "");
}

TEST(AppShell_FontDawShortcutsDefaultTests, ChangedBaseKeys_SetsNothing)
{
    EXPECT_EQ(fontDawShortcutsPresetToApply(false, true, "", AVAILABLE, WANTED), "");
}

TEST(AppShell_FontDawShortcutsDefaultTests, PresetAlreadySet_SetsNothing)
{
    EXPECT_EQ(fontDawShortcutsPresetToApply(false, false, "shortcuts_azerty", AVAILABLE, WANTED), "");
}

TEST(AppShell_FontDawShortcutsDefaultTests, NoDefaultConfigured_SetsNothing)
{
    EXPECT_EQ(fontDawShortcutsPresetToApply(false, false, "", AVAILABLE, ""), "");
}

TEST(AppShell_FontDawShortcutsDefaultTests, DefaultNotListed_SetsNothing)
{
    EXPECT_EQ(fontDawShortcutsPresetToApply(false, false, "", { "shortcuts_azerty" }, WANTED), "");
}
