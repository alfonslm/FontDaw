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

#include "internal/commandsections.h"

using namespace mu::pointertools;
using muse::rcommand::Command;

class PointerTools_CommandSectionsTests : public ::testing::Test
{
protected:
    static Section notation(const char* name)
    {
        return sectionOf(Command(std::string("command://notation/") + name), "notation");
    }
};

TEST_F(PointerTools_CommandSectionsTests, Articulations)
{
    for (const char* name : { "toggle-sforzato", "toggle-staccato", "toggle-tenuto", "toggle-marcato",
                              "add-up-bow", "add-down-bow", "toggle-laissez-vibrer" }) {
        EXPECT_EQ(notation(name), Section::Articulations) << name;
    }
}

TEST_F(PointerTools_CommandSectionsTests, CommandPathIsWhatTheRulesLookAt)
{
    EXPECT_EQ(Command("command://notation/toggle-staccato").path(), "notation/toggle-staccato");
}

TEST_F(PointerTools_CommandSectionsTests, SmallFamilies)
{
    EXPECT_EQ(notation("add-fermata"), Section::Fermatas);
    EXPECT_EQ(notation("add-breath"), Section::BreathsAndPauses);
    EXPECT_EQ(notation("add-caesura"), Section::BreathsAndPauses);
    EXPECT_EQ(notation("add-clef-bass"), Section::Clefs);
    EXPECT_EQ(notation("add-clef-violin"), Section::Clefs);
    EXPECT_EQ(notation("add-key-signature"), Section::KeyAndTimeSignatures);
    EXPECT_EQ(notation("add-time-signature"), Section::KeyAndTimeSignatures);
    EXPECT_EQ(notation("add-dynamic"), Section::Dynamics);
    EXPECT_EQ(notation("increase-dynamic"), Section::Dynamics);
}

TEST_F(PointerTools_CommandSectionsTests, Lines)
{
    for (const char* name : { "add-slur", "add-hairpin", "add-hairpin-reverse", "add-ottava-8va", "add-ottava-8vb",
                              "add-noteline" }) {
        EXPECT_EQ(notation(name), Section::Lines) << name;
    }
}

TEST_F(PointerTools_CommandSectionsTests, Ornaments)
{
    for (const char* name : { "add-trill", "add-mordent", "add-turn", "add-turn-inverted", "add-shake",
                              "add-acciaccatura", "add-grace4" }) {
        EXPECT_EQ(notation(name), Section::Ornaments) << name;
    }
}

TEST_F(PointerTools_CommandSectionsTests, Text)
{
    for (const char* name : { "add-staff-text", "add-system-text", "add-lyrics", "add-tempo", "add-rehearsalmark-text",
                              "add-chord-text", "add-figured-bass", "edittext-toggle-bold", "editlyrics-next-syllable" }) {
        EXPECT_EQ(notation(name), Section::Text) << name;
    }
}

TEST_F(PointerTools_CommandSectionsTests, NotesDurationsVoicesTuplets)
{
    EXPECT_EQ(notation("pitch-up"), Section::Notes);
    EXPECT_EQ(notation("enter-note-a"), Section::Notes);
    EXPECT_EQ(notation("add-interval-plus3"), Section::Notes);
    EXPECT_EQ(notation("toggle-sharp"), Section::Notes);
    EXPECT_EQ(notation("toggle-flat2"), Section::Notes);
    EXPECT_EQ(notation("toggle-nat"), Section::Notes);
    EXPECT_EQ(notation("enter-rest"), Section::Notes);
    EXPECT_EQ(notation("set-duration-quarter"), Section::Durations);
    EXPECT_EQ(notation("toggle-dot"), Section::Durations);
    EXPECT_EQ(notation("use-voice-2"), Section::Voices);
    EXPECT_EQ(notation("swap-voice-x12"), Section::Voices);
    EXPECT_EQ(notation("add-triplet"), Section::Tuplets);
    EXPECT_EQ(notation("add-duplet"), Section::Tuplets);
    EXPECT_EQ(notation("add-quintuplet"), Section::Tuplets);
    EXPECT_EQ(notation("add-tuplet"), Section::Tuplets);
}

TEST_F(PointerTools_CommandSectionsTests, MeasuresLayoutSelectionEditView)
{
    EXPECT_EQ(notation("insert-measure"), Section::Measures);
    EXPECT_EQ(notation("join-measures"), Section::Measures);
    EXPECT_EQ(notation("add-full-measure-rest"), Section::Measures);
    EXPECT_EQ(notation("toggle-system-break"), Section::Layout);
    EXPECT_EQ(notation("toggle-page-break"), Section::Layout);
    EXPECT_EQ(notation("move-measure-to-next-system"), Section::Layout);
    EXPECT_EQ(notation("append-textframe"), Section::Layout);
    EXPECT_EQ(notation("add-frame-text"), Section::Layout);
    EXPECT_EQ(notation("open-edit-style"), Section::Layout);
    EXPECT_EQ(notation("select-all"), Section::Selection);
    EXPECT_EQ(notation("select-notes-in-chord"), Section::Selection);
    EXPECT_EQ(notation("add-to-selection-next-measure"), Section::Selection);
    EXPECT_EQ(notation("goto-next-element"), Section::Selection);
    EXPECT_EQ(notation("copy"), Section::Edit);
    EXPECT_EQ(notation("cut"), Section::Edit);
    EXPECT_EQ(notation("paste"), Section::Edit);
    EXPECT_EQ(notation("delete"), Section::Edit);
    EXPECT_EQ(notation("undo"), Section::Edit);
    EXPECT_EQ(notation("redo"), Section::Edit);
    EXPECT_EQ(notation("flip"), Section::Edit);
    EXPECT_EQ(notation("swap-left"), Section::Edit);
    EXPECT_EQ(notation("show-invisible"), Section::View);
    EXPECT_EQ(notation("show-frames"), Section::View);
    EXPECT_EQ(notation("toggle-visible"), Section::View);
}

TEST_F(PointerTools_CommandSectionsTests, ModuleDecidesFirst)
{
    EXPECT_EQ(sectionOf(Command("command://arrangement/import-audio-track"), "arrangement"), Section::Audio);
    EXPECT_EQ(sectionOf(Command("command://playback/play"), "playback"), Section::Playback);
    EXPECT_EQ(sectionOf(Command("command://project/save"), "project"), Section::General);
    EXPECT_EQ(sectionOf(Command("command://appshell/quit"), "appshell"), Section::General);
    EXPECT_EQ(sectionOf(Command("command://pointertools/open-pointer-menu"), "pointertools"), Section::General);
    EXPECT_EQ(sectionOf(Command("command://palette/toggle-master-palette"), "palette"), Section::View);
}

TEST_F(PointerTools_CommandSectionsTests, UnknownCommandIsOther)
{
    EXPECT_EQ(notation("frobnicate"), Section::Other);
    EXPECT_EQ(sectionOf(Command("command://somewhere/frobnicate"), "somewhere"), Section::Other);
}

TEST_F(PointerTools_CommandSectionsTests, EverySectionHasATitle)
{
    for (int i = static_cast<int>(Section::Articulations); i <= static_cast<int>(Section::Other); ++i) {
        EXPECT_FALSE(sectionTitle(static_cast<Section>(i)).isEmpty()) << i;
    }
    EXPECT_EQ(sectionTitle(Section::BreathsAndPauses), muse::String(u"Breaths and pauses"));
}
