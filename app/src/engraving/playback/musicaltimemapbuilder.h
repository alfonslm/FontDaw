/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "audio/common/musicaltimemap.h"

#include "../dom/playedbeatmap.h"

namespace mu::engraving {
class Score;
struct TempoTimePoint;

//! Describes the score to the audio engine as time <-> musical position, so that plugins which follow
//! the host transport know the tempo and which bar and beat they are at.
//!
//! It is made from the PlayedBeatMap, the one beat map the MIDI export also writes, so that a tempo
//! track imported from FontDaw's MIDI file and the position sent to plugins agree bar for bar. A pause
//! is therefore never "music standing still" here: its hidden beats keep the quarters moving, and its
//! bar is longer (FD-102, FD-114). The contract of the result is in muse's musicaltimemap.h.
class MusicalTimeMapBuilder
{
public:
    static muse::audio::MusicalTimeMap build(const Score* score, bool expandRepeats);

    //! The same on plain data (see PlayedBeatMap::build)
    static muse::audio::MusicalTimeMap build(const std::vector<TempoTimePoint>& tempoPoints, const std::vector<PlayedMeasure>& measures,
                                             double tempoMultiplier);

    static muse::audio::MusicalTimeMap fromBeatMap(const PlayedBeatMap& beats);
};
}
