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

#include <optional>
#include <vector>

#include "engraving/types/fraction.h"

#include "../arrangementtypes.h"

namespace mu::engraving {
class Page;
class RepeatList;
class Score;
class System;
class TempoTimeline;
}

namespace mu::arrangement {
//! Where a time falls in the laid-out score (Batch 2 S1).

struct ScoreX {
    double x = 0.0;                          // canvas x
    const engraving::System* system = nullptr;
};

//! Canvas x of a written tick: linear between the canvas x of neighbouring chord-rest segments,
//! the end bar line for the last one. The same interpolation as the playback cursor
//! (PlaybackCursor::resolveCursorRectByTick), which keeps its own copy for now. nullopt before
//! the first or after the last measure, or when the measure is not laid out.
std::optional<ScoreX> tickToCanvasX(const engraving::Score* score, const engraving::Fraction& writtenTick);

//! Canvas x of the end of the last measure (the final bar line), with its system.
std::optional<ScoreX> scoreEndX(const engraving::Score* score);

//! Seconds from the start of playback → written tick: through the tempo map to the played tick,
//! then through the repeat list to the written tick (a time in the second pass of a repeat maps
//! to the same written bars as the first). nullopt past the end of the score.
std::optional<engraving::Fraction> secsToWrittenTick(const engraving::Score* score, double secs, bool expandRepeats);

//! secsToWrittenTick for many times in a row (FD-167): the tempo timeline, the repeat list and
//! the score's played end are looked up once, when this is made. Valid while the score is not
//! changed (one paint).
class SecsToWrittenTick
{
public:
    SecsToWrittenTick(const engraving::Score* score, bool expandRepeats);
    std::optional<engraving::Fraction> operator()(double secs) const;

private:
    const engraving::TempoTimeline* m_timeline = nullptr;
    const engraving::RepeatList* m_repeats = nullptr; // null: repeats not expanded
    int m_endPlayedTick = 0;
};

//! tickToCanvasX for the ticks of one system (FD-167): the system's chord-rest spans are listed
//! once, when this is made, and each lookup is a binary search. The same result as tickToCanvasX
//! for a tick in one of the system's measures; nullopt for any other tick. Valid while the score
//! is not changed (one paint).
class SystemTickToX
{
public:
    explicit SystemTickToX(const engraving::System* system);
    std::optional<double> operator()(const engraving::Fraction& tick) const;

private:
    struct Span {
        engraving::Fraction from;
        engraving::Fraction to;
        double x0 = 0.0;
        double x1 = 0.0;
    };
    std::vector<Span> m_spans; // sorted by from, not overlapping
};

//! With repeats played: where the pass that plays at secs ends, when the next thing played jumps
//! back (the end of a repeat). endSecs is the played time of that end, endTick the written tick
//! where its last bar ends. nullopt without repeats, or when no jump back follows.
struct PassEnd {
    double endSecs = 0.0;
    engraving::Fraction endTick;
};
std::optional<PassEnd> passEndAt(const engraving::Score* score, double secs, bool expandRepeats);

//! The written ticks a system shows (Batch 2 S4): from its first measure's tick to its last
//! measure's end tick. nullopt for a system without measures (a frame-only system).
struct SystemTicks {
    const engraving::System* system = nullptr;
    engraving::Fraction from;
    engraving::Fraction to;
};
std::optional<SystemTicks> systemTicks(const engraving::System* system);

//! One system's piece of a clip, in canvas coordinates
struct ClipPiece {
    const engraving::System* system = nullptr;
    double x0 = 0.0;              // where the piece starts: the clip's start, or the system's first measure
    double x1 = 0.0;              // where it ends: the clip's end, or the end of the system's last measure
    bool continuesLeft = false;   // the clip began in an earlier system
    bool continuesRight = false;  // the clip goes on in a later system (or past the end of the score)
};

//! The pieces of a clip that plays from startSecs to endSecs (played seconds, as S1's clips).
//! The clip is cut at the end of the score, and at the end of the repeat pass it starts in
//! (passEndAt), as S1's painter does. Empty when the clip lies outside the score.
//! With `onlyPage`, only the pieces in that page's systems (FD-167: a page is painted on its own).
std::vector<ClipPiece> clipPieces(const engraving::Score* score, double startSecs, double endSecs, bool expandRepeats,
                                  const engraving::Page* onlyPage = nullptr);
}
