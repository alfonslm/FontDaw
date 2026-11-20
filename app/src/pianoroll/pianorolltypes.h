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

// DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

#pragma once

#include <vector>

#include "engraving/types/fraction.h"

namespace mu::engraving {
class Note;
}

namespace mu::pianoroll {
//! One bar in the piano roll. A snapshot for drawing; the Note in the score stays the truth.
struct NoteBar {
    const mu::engraving::Note* note = nullptr; // valid until the next score change: rebuild then
    int pitch = 60;                            // MIDI pitch as played (after transposition, ottavas)

    mu::engraving::Fraction writtenTick;     // where the note is notated
    mu::engraving::Fraction writtenDuration; // tied notes are shown as one bar

    // As played, from Note::playEvents() (1/1000 of the written duration). See README.
    int ontime = 0;
    int len = 1000;

    int velocity = 80;           // effective velocity (from dynamics) ...
    bool velocityIsUser = false; // ... or Note::userVelocity() when the user overrode it
    bool selected = false;
};

using NoteBars = std::vector<NoteBar>;

// TODO(FontDaw FEATURES §5): effective velocity must come from the same code playback uses
// (src/engraving/playback/), not be recomputed here, or the lane will lie.
}
