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

#include "../types/fraction.h"

namespace mu::engraving {
class Score;

//! Whether a note, a line or a pedal that ends on a pause's tick is held through the pause (its
//! hidden beats) or ends where the pause starts. One switch for playback (playback/utils/arrangementutils.h)
//! and the MIDI export (PlayedBeatMap::playedEndTick), so that they agree. Not held: the owner's
//! decision of 17 November 2026 (FD-114), which is also upstream's playback. Holding is to become a
//! choice per staff and pause in the hidden-beat view, over this default.
inline constexpr bool HOLD_NOTES_THROUGH_PAUSES = false;
struct TempoTimePoint;

//! A measure as it is played back: one entry per pass through a repeated measure.
struct PlayedMeasure {
    int utick = 0;      // unwound start tick in the score
    int ticks = 0;      // the measure's actual length in the score (a pickup is shorter than its time signature)
    Fraction timeSig;   // the score's time signature for it
};

//! The score's played time as beats and bars: the one beat map that the MIDI export and the position
//! sent to plug-ins both read, so that they cannot disagree (FD-102, "V4"; the hidden beat, FD-114).
//!
//! - Played ticks: tick 0 is the start of the music; repeats are unrolled as the score's tempo timeline
//!   unrolls them. A pickup is a short first bar, so the first full bar starts at the pickup's length.
//! - A pause (caesura, breath, section-break pause) is **hidden beats**: extra ticks added to the bar
//!   the pause ends, at a tempo close to the surrounding one, so that they last exactly the pause.
//!   The bar is longer by them and the bar after it starts a new bar point. The score does not change.
//!   The hidden ticks are whole beats of the bar's time signature, the beat being its denominator's
//!   note (in 6/8 an eighth: a 6/8 bar with one hidden beat becomes 7/8); a pause shorter than about
//!   ¾ of a beat gets half beats, quarter beats and so on, down to a 64th note. They are sized at the
//!   tempo in force before the pause's tick (not at a tempo change on that tick), and their own tempo
//!   is within about ⅔ to 1⅓ of it. Limit: a pause shorter than about ¾ of a 64th note still gets one
//!   64th, so its hidden tempo is faster than that range (0.01 s at ♩ = 120: 6.25 quarters a second).
//! - A pause belongs to the bar it interrupts or, when it sits on a bar line, to the bar that ends
//!   there. A pause on the first tick of the music has no bar before it and cannot be given as
//!   beats: it gets no hidden beats, but its time is kept (the first tempo point's secs is the
//!   played time at tick 0), so that times stay equal to playback's.
//! - Times are played time with the playback speed applied (as the audio export plays it): tempi are
//!   multiplied by the speed, pauses divided by it. The played ticks do not depend on the speed.
//! - Everything at or after a pause's tick moves to after its hidden beats: a note that starts there
//!   starts after them. Whether a note that ends there is held through them is HOLD_NOTES_THROUGH_PAUSES.
//! - For readers that cannot hold a time offset (a MIDI file): a pause on the first tick is not in
//!   the file, which therefore runs earlier than playback by that pause. A bar that is shortened in
//!   the score and made exactly full length by its hidden beats gets no bar point; its hidden ticks
//!   show only in hiddenBeats().
class PlayedBeatMap
{
public:
    //! From this played tick on, the music moves at `bps`.
    struct TempoPoint {
        int tick = 0;        // played tick
        double secs = 0.0;   // played time at `tick`, playback speed applied
        double bps = 0.0;    // quarter notes per second from `tick` on, playback speed applied; never 0
    };

    //! A bar starts at `tick`. Points are given at the start, at every time signature change, at every
    //! bar whose played length differs from its time signature (a pickup, a bar lengthened by a pause,
    //! a bar shortened before a repeat), and at the bar after each of those. Bars between two points
    //! are as long as the earlier point's time signature says.
    struct BarPoint {
        int tick = 0;            // played tick where the bar starts
        int ticks = 0;           // the bar's played length: its length in the score plus its hidden beats
        int hiddenTicks = 0;     // of which hidden beats
        Fraction timeSig;        // the score's time signature for the bar
        Fraction playedTimeSig;  // a time signature `ticks` long, keeping timeSig's denominator where it can
                                 // (4/4 + 2 beats = 6/4; an eighth pickup in 3/4 = 1/8); equal to timeSig for a
                                 // regular bar; invalid (0/0) when a MIDI file cannot express the length:
                                 // no power-of-two denominator up to 64 gives a numerator of at most 255
                                 // (lower denominators are tried too: 600 beats of 4/4 are 150/1), or the
                                 // score's denominator is not a power of two (4/6). Then a reader uses
                                 // timeSig and the length `ticks`; the MIDI export writes timeSig, so
                                 // later bar numbers in the file are off from there.
        bool isRegular() const { return ticks == timeSig.ticks(); }
    };

    //! The hidden beats of one pause, as played.
    struct HiddenBeats {
        int utick = 0;       // where the pause is in the score, unwound
        int tick = 0;        // played tick where the hidden beats start; they end at tick + ticks
        int ticks = 0;       // their length
        double secs = 0.0;   // the pause, playback speed applied: the hidden beats last exactly this
    };

    //! The beat map of a score as it is played: its tempo timeline (score->tempoTimeline(expandRepeats),
    //! with the playback speed currently set) and its measures in playback order (score->repeatList(expandRepeats)).
    static PlayedBeatMap build(const Score* score, bool expandRepeats);

    //! The same on plain data, for tests and for callers that already hold both.
    //! `tempoPoints` as TempoTimeline::points() gives them (nominal time, speed not applied);
    //! `measures` contiguous and in playback order; `tempoMultiplier` the playback speed (1.0 = 100 %).
    static PlayedBeatMap build(const std::vector<TempoTimePoint>& tempoPoints, const std::vector<PlayedMeasure>& measures,
                               double tempoMultiplier);

    //! The played tick of a score utick: utick plus the hidden ticks of every pause at or before it.
    //! Where something that starts at utick starts.
    int playedTick(int utick) const;

    //! Where something that ends at utick ends: playedTick(utick), except on a pause's tick when
    //! HOLD_NOTES_THROUGH_PAUSES is false, where it is the start of that pause's hidden beats.
    //! `lateTicks`: an end written up to that many ticks after a pause's tick still counts as on it
    //! (the MIDI renderer writes a pedal release a tick or two after its line's end).
    int playedEndTick(int utick, int lateTicks = 0) const;

    //! Played time at a played tick, playback speed applied. Equals TempoTimeline::utick2utime(utick)
    //! for playedTick(utick), for every utick that is not inside a pause.
    double secsAt(int playedTick) const;

    const std::vector<TempoPoint>& tempo() const { return m_tempo; }        // sorted by tick, the first at tick 0
    const std::vector<BarPoint>& bars() const { return m_bars; }            // sorted by tick, the first at tick 0
    const std::vector<HiddenBeats>& hiddenBeats() const { return m_hidden; } // sorted by utick

private:
    std::vector<TempoPoint> m_tempo;
    std::vector<BarPoint> m_bars;
    std::vector<HiddenBeats> m_hidden;
};
}
