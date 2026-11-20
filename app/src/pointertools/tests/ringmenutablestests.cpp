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

#include <set>
#include <utility>

#include "notationscene/internal/notationcommandsregister.h"
#include "notationscene/notationcommands.h"

#include "internal/ringmenutables.h"

using namespace mu::pointertools;
using namespace mu::notation;
using namespace muse::rcommand;
using namespace muse::ui;

namespace {
const CommandInfo& infoOf(const Command& command)
{
    static const NotationCommandsRegister reg;
    for (const CommandInfo& info : reg.commandInfoList()) {
        if (info.command == command) {
            return info;
        }
    }
    static const CommandInfo none;
    return none;
}

IconCode::Code iconOf(const RingEntry& entry)
{
    return entry.icon != IconCode::Code::NONE ? entry.icon : infoOf(entry.command).decoration.iconCode;
}

std::vector<std::pair<const char*, const RingTable*> > tables()
{
    return { { "one note", &oneNoteRingTable() }, { "several notes", &severalNotesRingTable() }, { "audio", &audioRingTable() } };
}
}

TEST(PointerTools_RingMenuTablesTests, TablesFitTheRingAndHaveNoDuplicates)
{
    for (const auto& [name, table] : tables()) {
        SCOPED_TRACE(name);
        if (table != &audioRingTable()) { // empty until Batch 2 S5
            EXPECT_FALSE(table->empty());
        }
        EXPECT_LE(table->size(), RING_MAX_SLOTS - 1);

        std::set<Command> seen;
        for (const RingEntry& entry : *table) {
            EXPECT_TRUE(seen.insert(entry.command).second) << entry.command.toString();
        }
    }
}

TEST(PointerTools_RingMenuTablesTests, EveryCommandIsRegistered)
{
    for (const auto& [name, table] : tables()) {
        SCOPED_TRACE(name);
        for (const RingEntry& entry : *table) {
            EXPECT_EQ(infoOf(entry.command).command, entry.command) << entry.command.toString();
        }
    }
}

//! The ring shows icons only, so each segment needs an icon and no two may look alike.
TEST(PointerTools_RingMenuTablesTests, IconsAreDistinctAndNeverNone)
{
    for (const auto& [name, table] : tables()) {
        SCOPED_TRACE(name);
        std::set<std::pair<IconCode::Code, bool> > seen;  // an icon and its mirror image look different
        for (const RingEntry& entry : *table) {
            const IconCode::Code icon = iconOf(entry);
            EXPECT_NE(icon, IconCode::Code::NONE) << entry.command.toString();
            EXPECT_TRUE(seen.insert({ icon, entry.iconMirrored }).second) << entry.command.toString();
        }
    }
}

TEST(PointerTools_RingMenuTablesTests, TheThreeCommandsWithoutAnIconHaveAnOverride)
{
    for (const Command& command : { ADD_HAIRPIN_COMMAND, ADD_HAIRPIN_REVERSE_COMMAND, AddTripletCommand::id }) {
        EXPECT_EQ(infoOf(command).decoration.iconCode, IconCode::Code::NONE) << command.toString();
        bool overridden = false;
        for (const RingEntry& entry : severalNotesRingTable()) {
            if (entry.command == command) {
                overridden = entry.icon != IconCode::Code::NONE;
            }
        }
        EXPECT_TRUE(overridden) << command.toString();
    }
}

//! FD-156, FD-166: the engraving symbols are drawn without a name, everything else has one.
TEST(PointerTools_RingMenuTablesTests, OnlyTheEngravingSymbolsHaveNoName)
{
    const std::set<Command> engraving {
        TOGGLE_SFORZATO_COMMAND, TOGGLE_STACCATO_COMMAND, TOGGLE_TENUTO_COMMAND, TOGGLE_TIE_COMMAND,
        ADD_SLUR_COMMAND, ADD_HAIRPIN_COMMAND, ADD_HAIRPIN_REVERSE_COMMAND, AddTripletCommand::id
    };

    int withoutName = 0;
    for (const auto& [name, table] : tables()) {
        SCOPED_TRACE(name);
        for (const RingEntry& entry : *table) {
            const bool isEngraving = engraving.count(entry.command) > 0;
            EXPECT_EQ(entry.showName, !isEngraving) << entry.command.toString();
            withoutName += entry.showName ? 0 : 1;
        }
    }
    // one note: 5 (accent, staccato, tenuto, tie, slur); several notes: 4 (slur, crescendo, diminuendo, tuplet)
    EXPECT_EQ(withoutName, 9);

    for (const RingTable* table : { &oneNoteRingTable(), &severalNotesRingTable() }) {
        for (const RingEntry& entry : *table) {
            if (entry.command == FlipCommand::id || entry.command == CopyCommand::id || entry.command == DeleteCommand::id) {
                EXPECT_TRUE(entry.showName) << entry.command.toString();
            }
        }
    }
}
