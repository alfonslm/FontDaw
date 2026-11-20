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

namespace mu::engraving {
class EngravingItem;
}

namespace mu::pointertools {
//! Which ring table applies to a selection. Split out of the provider so tests can drive it from
//! elements read from a score, without a notation view.
enum class ScoreItemKind {
    None,         // nothing selected
    OneNote,      // one note, or the notes of one chord
    SeveralNotes, // notes of more than one chord, or a range
    Audio,        // anything on an audio staff (Batch 2 S1)
    Other,        // any other element type
};

//! Empty and not a range: None. Any element on an audio staff: Audio. A range: SeveralNotes. All
//! elements notes of one chord: OneNote; all notes of more than one chord: SeveralNotes. Else Other.
ScoreItemKind classifySelection(const std::vector<const mu::engraving::EngravingItem*>& elements, bool isRange);
}
