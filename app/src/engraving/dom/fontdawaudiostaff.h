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

// FontDaw: an audio track is shown in the score as a part with one "audio staff" (Batch 2 S1,
// docs/fontdaw/design/audio-in-score/). The part is an ordinary MuseScore part; what marks it
// is its instrument id. These checks are the one place that knows the mark, so the few hooks in
// engraving and notation (playback model, note input, paste) read the same rule.

#include "instrument.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "spanner.h"
#include "staff.h"

namespace mu::engraving {
//! Instrument id (and MusicXML instrument id) of an audio track's part.
inline constexpr const char16_t* FONTDAW_AUDIO_INSTRUMENT_ID = u"fontdaw-audio-track";

inline bool isAudioInstrument(const Instrument* instrument)
{
    return instrument && instrument->id() == FONTDAW_AUDIO_INSTRUMENT_ID;
}

inline bool isAudioPart(const Part* part)
{
    return part && isAudioInstrument(part->instrument());
}

inline bool isAudioStaff(const Staff* staff)
{
    return staff && isAudioPart(staff->part());
}

//! What an audio staff does not draw: it shows no notation (owner, 17 November 2026), so every
//! item on it is left out of painting and of hit-testing (its invisible lines and rests too, which
//! "Show invisible" would otherwise draw in grey on screen). The audio painter draws the rest
//! (Batch 2 S4). A stopgap: the owner's direction is that an audio staff has no note or rest layer
//! at all (FD-127).
//! Kept: its bar lines (they run through the row, design 05, answer 7), its name, brackets, the
//! measure itself, measure numbers, and system markings (tempo, rehearsal marks, voltas, jumps,
//! system text), which are stored on the top staff and so sit on the audio staff when it is the
//! top one (an audio-only score, a part holding only the audio part).
inline bool isHiddenOnAudioStaff(const EngravingItem* item)
{
    if (!item || item->isBarLine() || item->isInstrumentName() || item->isBracket() || item->isBracketItem()
        || item->isMeasure() || item->isMeasureNumber() || item->isMMRestRange() || item->systemFlag()) {
        return false;
    }
    if (item->isSpannerSegment() && toSpannerSegment(item)->spanner() && toSpannerSegment(item)->spanner()->systemFlag()) {
        return false;
    }
    return isAudioStaff(item->staff());
}

//! How many chords (notes) sit on audio staves. An edit that could put one there (paste)
//! compares this before and after, so a chord that was already there (from a plug-in, or a file
//! edited elsewhere) does not make every later edit fail.
inline size_t chordsOnAudioStaves(const Score* score)
{
    size_t count = 0;
    if (!score) {
        return count;
    }
    for (const Part* part : score->parts()) {
        if (!isAudioPart(part)) {
            continue;
        }
        for (const Staff* staff : part->staves()) {
            const track_idx_t first = staff->idx() * VOICES;
            for (const Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
                for (track_idx_t t = first; t < first + VOICES; ++t) {
                    const EngravingItem* item = s->element(t);
                    if (item && item->isChord()) {
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}
}
