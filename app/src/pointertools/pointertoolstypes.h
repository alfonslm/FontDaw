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

#pragma once

#include <string>
#include <vector>

#include "global/types/string.h"
#include "global/types/translatablestring.h"
#include "rcommand/commandtypes.h"
#include "ui/uitypes.h"

namespace mu::pointertools {
//! The kind of view under the pointer. Each kind has its own commands in the pointer menu and its
//! own ring provider. Only the score exists in this workstream; the mixer is named so that its
//! provider can be added without changing the interfaces (shortcut work order 08b, 11).
enum class ViewKind {
    Undefined = 0,
    Score,
    Mixer, // TODO(FontDaw shortcuts 11): the mixer's commands are not rcommand commands yet
};

//! A section of the pointer menu, in display order. Grouped like the palettes
//! (docs/fontdaw/design/shortcuts/06-fontdaw-layout/global.md, "The Tab menu").
//! The table that puts commands into sections is data: internal/commandsections.cpp.
enum class Section {
    Articulations = 0,
    Ornaments,
    Fermatas,
    BreathsAndPauses,
    Clefs,
    KeyAndTimeSignatures,
    Lines,
    Dynamics,
    Text,
    Notes,
    Durations,
    Voices,
    Tuplets,
    Measures,
    Layout,
    Selection,
    Edit,
    View,
    Playback,
    Audio,      // the arrangement's commands (audio tracks and clips)
    General,    // project and application commands
    Other,      // a command no table names; listed last, still searchable
    // TODO(FontDaw shortcuts 09): Families, the family pop-ups as entries of their own
};

//! One command as the pointer menu shows it.
struct CommandEntry {
    muse::rcommand::Command command;
    muse::String title;         //! translated, mnemonic stripped
    muse::String description;   //! translated
    muse::ui::IconCode::Code icon = muse::ui::IconCode::Code::NONE;
    Section section = Section::Other;
    muse::String keys;          //! the key sequence(s) as text, empty when none is bound
    std::string module;         //! the rcommand module that registered the command

    bool operator==(const CommandEntry& other) const { return command == other.command; }
};
using CommandEntryList = std::vector<CommandEntry>;

//! One search result: the entry and how well it matched (lower is better).
struct SearchHit {
    const CommandEntry* entry = nullptr;
    int matchRank = 0;
};
using SearchHitList = std::vector<SearchHit>;

//! One segment of the ring menu. A segment runs either a command, or an item of today's context
//! menu (handed back to that menu's model, so nothing it does is duplicated here), or opens "More…".
//! The ring draws an icon per segment and, when showName is set, the title under it (owner,
//! 19 November 2026, FD-156, FD-166); the title is also the segment's accessible name.
struct RingSlot {
    enum class Kind {
        Command = 0,
        TodayMenuItem,
        More,
    };

    Kind kind = Kind::Command;
    muse::rcommand::Command command;    //! Kind::Command
    std::string todayMenuItemId;        //! Kind::TodayMenuItem: the id in today's menu model
    muse::String title;
    muse::ui::IconCode::Code icon = muse::ui::IconCode::Code::NONE;
    bool iconMirrored = false;          //! draw the icon mirrored left to right
    bool showName = true;               //! write the title under the icon (false for engraving symbols)
};

//! What the ring shows (docs/fontdaw/design/shortcuts/08-pointer-tools-look.md). The segments close
//! up: n entries make n equal segments, no empty places. "More…" is always present (nothing of
//! today's context menu is lost) and always LAST in ringSlots; the QML draws the last entry
//! straight down and the others clockwise after it. At most RING_MAX_SLOTS entries, "More…"
//! included.
inline constexpr size_t RING_MAX_SLOTS = 8;

struct RingContents {
    muse::String accessibleName;     //! "Actions for <item>", announced by screen readers
    std::vector<RingSlot> ringSlots; //! 1 to RING_MAX_SLOTS entries, "More…" last
};
}
