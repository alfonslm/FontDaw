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

#include "ringmenutables.h"

#include "notationscene/notationcommands.h"

using namespace muse::ui;
using namespace mu::notation;
using namespace mu::pointertools;

// Commands in ring order; an icon after the command overrides the command's own.
// FontDaw (FD-156, FD-166): NO_NAME marks the engraving symbols, which the ring draws without a name
// under the icon (users know them); every other entry shows its name.
namespace {
constexpr bool NO_NAME = false;
}

const RingTable& mu::pointertools::oneNoteRingTable()
{
    static const RingTable table {
        { TOGGLE_SFORZATO_COMMAND, IconCode::Code::NONE, false, NO_NAME },  // accent
        { TOGGLE_STACCATO_COMMAND, IconCode::Code::NONE, false, NO_NAME },
        { TOGGLE_TENUTO_COMMAND, IconCode::Code::NONE, false, NO_NAME },
        { TOGGLE_TIE_COMMAND, IconCode::Code::NONE, false, NO_NAME },
        { ADD_SLUR_COMMAND, IconCode::Code::NONE, false, NO_NAME },  // slur to the next note
        { FlipCommand::id },
        { DeleteCommand::id },
    };
    return table;
}

const RingTable& mu::pointertools::severalNotesRingTable()
{
    static const RingTable table {
        { ADD_SLUR_COMMAND, IconCode::Code::NONE, false, NO_NAME },
        // hairpin wedges (work order B8, #220), not the "cresc."/"dim." text icons
        { ADD_HAIRPIN_COMMAND, IconCode::Code::HAIRPIN, false, NO_NAME },
        { ADD_HAIRPIN_REVERSE_COMMAND, IconCode::Code::HAIRPIN, true /*iconMirrored*/, NO_NAME },
        { AddTripletCommand::id, IconCode::Code::NOTE_TUPLET, false, NO_NAME },
        { CopyCommand::id },
        { DeleteCommand::id },
    };
    return table;
}

const RingTable& mu::pointertools::audioRingTable()
{
    // Empty until Batch 2 S5 adds the clip commands: split, fade in, fade out, mute, duplicate,
    // delete, gain. DELETE_COMMAND is not here: it acts on the score's selection, and everything
    // the score holds on an audio staff is hidden there (engraving isHiddenOnAudioStaff, #196),
    // so it would delete nothing the user can see. The ring is then "More…" alone, and the QML
    // opens today's menu directly instead of a one-segment ring.
    static const RingTable table {};
    return table;
}
