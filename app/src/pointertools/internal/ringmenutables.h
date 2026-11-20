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

#include <vector>

#include "rcommand/commandtypes.h"

#include "../pointertoolstypes.h"

namespace mu::pointertools {
//! The ring's contents per item, as data (ringmenutables.cpp), so the owner can change them
//! without touching the provider's code. The commands in ring order (clockwise after "More…");
//! "More…" is not in the tables, the provider adds it last. At most RING_MAX_SLOTS - 1 commands,
//! no duplicates, and every command with an icon of its own, since the ring draws an icon for each
//! (a test checks all three).
//!
//! An entry's icon overrides the command's own icon when set: the ring shows icons only, and some
//! commands have none (crescendo, diminuendo, triplet). iconMirrored draws the icon mirrored left
//! to right: the icon font has a crescendo wedge (HAIRPIN) but no diminuendo wedge.
//! showName: whether the ring writes the command's name under the icon (FD-156, FD-166; owner,
//! 19 November 2026). False for the engraving symbols (accent, staccato, tenuto, tie, slur,
//! crescendo, diminuendo, tuplet): users know those by sight.
struct RingEntry {
    muse::rcommand::Command command;
    muse::ui::IconCode::Code icon = muse::ui::IconCode::Code::NONE;
    bool iconMirrored = false;
    bool showName = true;
};
using RingTable = std::vector<RingEntry>;

//! global.md: accent, staccato, tenuto, tie, slur to the next note, flip, delete, More…
const RingTable& oneNoteRingTable();
//! global.md without the voice entry (owner, 17 November 2026): legato (slur), crescendo,
//! diminuendo, tuplet, copy, delete, More…
const RingTable& severalNotesRingTable();
//! global.md: split at the pointer, fade in, fade out, mute, duplicate, delete, gain, More….
//! Empty for now: the clip commands come with Batch 2 S5 and are added here then.
const RingTable& audioRingTable();
}
