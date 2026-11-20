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

#include "global/types/id.h"
#include "global/types/string.h"

#include "../arrangementtypes.h"

namespace mu::engraving {
class Part;
class Score;
}

namespace mu::arrangement {
//! The part that shows an audio track in the score (Batch 2 S1; design:
//! docs/fontdaw/design/audio-in-score/07-first-step.md, section 2).
//!
//! An ordinary MuseScore part: instrument id FONTDAW_AUDIO_INSTRUMENT_ID, one staff of the
//! standard staff type with clef, key and time signature off and invisible lines (five line
//! positions, so it is as tall as a default staff), invisible bar rests, "hide when empty" never.
//! Only existing settings, so the .mscx needs nothing new and MuseScore Studio opens it.

//! Adds the audio part for a track named trackName at the end of the score, with undoable
//! engraving edits (the caller groups them into one undo step: INotationUndoStack
//! prepareChanges / commitChanges). Long name = trackName, short name = audioPartShortName().
//! Returns the new part, or nullptr when the score has no measures.
engraving::Part* addAudioPart(engraving::Score* score, const muse::String& trackName);

//! The short name shown on later systems: the track name shortened (owner, 17 November 2026),
//! the way instrument short names are: the first word, cut to MAX_SHORT_NAME characters with a
//! full stop when it was cut.
muse::String audioPartShortName(const muse::String& trackName);
inline constexpr size_t MAX_SHORT_NAME = 6;

//! The score part showing track, or nullptr (no partId, or the part was deleted). Does not check
//! that no other track claims the same part: use resolveAudioParts for that.
const engraving::Part* findAudioPart(const engraving::Score* score, const Track& track);

//! The part of each track, in track order, with every part given to one track only: when two tracks
//! name the same part (a link left broken by undoing an import, then a new import reusing the
//! freed id; or a file saved that way), the first track keeps it and the others get nullptr.
std::vector<const engraving::Part*> resolveAudioParts(const engraving::Score* score, const TrackList& tracks);
}
