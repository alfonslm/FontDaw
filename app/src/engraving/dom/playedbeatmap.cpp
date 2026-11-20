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

#include "playedbeatmap.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>

#include "measure.h"
#include "repeatlist.h"
#include "score.h"
#include "tempotimeline.h"

#include "types/constants.h"

#include "global/log.h"

using namespace mu::engraving;

static constexpr int WHOLE_NOTE_TICKS = 4 * Constants::DIVISION;
static constexpr int SHORTEST_GAP_UNIT = Constants::DIVISION / 16; // a 64th note
static constexpr int MAX_MIDI_TIMESIG_NUMERATOR = 255; // stored in one byte
static constexpr int MAX_MIDI_TIMESIG_DENOMINATOR = 64;

static bool isPowerOfTwo(int v)
{
    return v > 0 && (v & (v - 1)) == 0;
}

static int beatTicks(const Fraction& timeSig)
{
    const int den = timeSig.denominator();
    if (!isPowerOfTwo(den) || den > MAX_MIDI_TIMESIG_DENOMINATOR) {
        return Constants::DIVISION;
    }

    return WHOLE_NOTE_TICKS / den;
}

//! How many ticks to insert for a pause, at roughly the surrounding tempo.
//! Whole beats where the pause is long enough, so that the lengthened bar
//! gets a readable time signature (4/4 + 2 beats = 6/4).
static int gapTicks(double pauseSecs, double bps, int beatTicks)
{
    const double exactTicks = pauseSecs * bps * Constants::DIVISION;

    int unit = std::max(beatTicks, SHORTEST_GAP_UNIT);
    while (unit > SHORTEST_GAP_UNIT && unit % 2 == 0 && exactTicks < 0.75 * unit) {
        unit /= 2;
    }

    return static_cast<int>(std::max(1L, std::lround(exactTicks / unit))) * unit;
}

//! The time signature of a bar that is `ticks` long, keeping the original denominator
//! if possible; if not (a numerator above 255), a lower power-of-two denominator that fits
//! (a 4/4 bar of 600 beats is 150/1). Invalid if MIDI cannot express it.
static Fraction timeSigForTicks(int ticks, const Fraction& originalTimeSig)
{
    static const Fraction INVALID(0, 0);

    const int originalDen = originalTimeSig.denominator();
    if (!isPowerOfTwo(originalDen)) {
        return INVALID;
    }

    const auto fits = [ticks](int den, Fraction& result) {
        const long long scaled = static_cast<long long>(ticks) * den;
        if (scaled % WHOLE_NOTE_TICKS != 0) {
            return false;
        }

        const long long num = scaled / WHOLE_NOTE_TICKS;
        if (num < 1 || num > MAX_MIDI_TIMESIG_NUMERATOR) {
            return false;
        }

        result = Fraction(static_cast<int>(num), den);
        return true;
    };

    Fraction result;
    for (int den = originalDen; den <= MAX_MIDI_TIMESIG_DENOMINATOR; den *= 2) {
        if (fits(den, result)) {
            return result;
        }
    }

    for (int den = originalDen / 2; den >= 1; den /= 2) {
        if (fits(den, result)) {
            return result;
        }
    }

    return INVALID;
}

static bool sameBps(double a, double b)
{
    return std::abs(a - b) <= 1e-9 * std::max(a, b);
}

//! Points arrive in tick order: a second point at the same tick replaces the tempo of the first.
//! A point that does not change the tempo is left out.
static void setTempoAt(std::vector<PlayedBeatMap::TempoPoint>& tempo, int tick, double bps)
{
    if (!tempo.empty() && tempo.back().tick == tick) {
        tempo.back().bps = bps;
        if (tempo.size() > 1 && sameBps(tempo[tempo.size() - 2].bps, bps)) {
            tempo.pop_back();
        }
        return;
    }

    if (!tempo.empty() && sameBps(tempo.back().bps, bps)) {
        return;
    }

    PlayedBeatMap::TempoPoint point;
    point.tick = tick;
    point.bps = bps;
    tempo.push_back(point);
}

PlayedBeatMap PlayedBeatMap::build(const Score* score, bool expandRepeats)
{
    IF_ASSERT_FAILED(score) {
        return PlayedBeatMap();
    }

    std::vector<PlayedMeasure> measures;
    for (const RepeatSegment* rs : score->repeatList(expandRepeats)) {
        const int tickOffset = rs->utick - rs->tick;
        for (const Measure* m : rs->measureList()) {
            measures.push_back({ m->tick().ticks() + tickOffset, m->ticks().ticks(), m->timesig() });
        }
    }

    const TempoTimeline& timeline = score->tempoTimeline(expandRepeats);
    return build(timeline.points(), measures, timeline.tempoMultiplier().val);
}

PlayedBeatMap PlayedBeatMap::build(const std::vector<TempoTimePoint>& tempoPoints, const std::vector<PlayedMeasure>& measures,
                                   double tempoMultiplier)
{
    const double mult = (std::isfinite(tempoMultiplier) && tempoMultiplier > 0.0) ? tempoMultiplier : 1.0;

    PlayedBeatMap map;
    std::map<size_t, int> hiddenByMeasure;
    double leadingPauseSecs = 0.0; // nominal; a pause on the first tick gets no beats, but its time is kept

    // The tempo in force before a point's tick: that of the last point at a smaller utick
    bool haveCurrent = false;
    int currentUtick = 0;
    double currentBps = 0.0;
    bool havePrevious = false;
    double previousBps = 0.0;

    for (const TempoTimePoint& point : tempoPoints) {
        const int utick = point.utick;

        if (haveCurrent && utick > currentUtick) {
            previousBps = currentBps;
            havePrevious = true;
        }
        haveCurrent = true;
        currentUtick = utick;
        currentBps = point.bps;

        // A pause belongs to the bar it interrupts, or to the one that ends where it is
        bool isPause = false;
        size_t measureIdx = 0;
        if (point.pause > 0.0) {
            auto it = std::lower_bound(measures.begin(), measures.end(), utick, [](const PlayedMeasure& m, int tick) {
                return m.utick + m.ticks < tick;
            });
            if (it == measures.end() || it->utick >= utick) {
                // a pause on the first tick of the music cannot be given as beats
                leadingPauseSecs += point.pause;
                continue;
            }
            isPause = true;
            measureIdx = static_cast<size_t>(it - measures.begin());
        }

        if (!isPause) {
            setTempoAt(map.m_tempo, map.playedTick(utick), point.bps * mult);
            continue;
        }

        // The hidden beats are sized on the tempo before the pause, not on one that starts with it
        const double sizingBps = havePrevious ? previousBps : point.bps;
        const int gap = gapTicks(point.pause, sizingBps, beatTicks(measures[measureIdx].timeSig));
        const int start = map.playedTick(utick);

        setTempoAt(map.m_tempo, start, static_cast<double>(gap) / Constants::DIVISION / point.pause * mult);
        map.m_hidden.push_back({ utick, start, gap, point.pause / mult });
        setTempoAt(map.m_tempo, start + gap, point.bps * mult);

        hiddenByMeasure[measureIdx] += gap;
    }

    if (map.m_tempo.empty() || map.m_tempo.front().tick > 0) {
        TempoPoint first;
        first.bps = Constants::DEFAULT_TEMPO.val * mult;
        map.m_tempo.insert(map.m_tempo.begin(), first);
    }

    map.m_tempo.front().secs = leadingPauseSecs / mult;
    for (size_t i = 1; i < map.m_tempo.size(); ++i) {
        const TempoPoint& prev = map.m_tempo[i - 1];
        map.m_tempo[i].secs = prev.secs + double(map.m_tempo[i].tick - prev.tick) / (Constants::DIVISION * prev.bps);
    }

    // Bar points: the start, every time signature change, every bar whose played length is not its
    // time signature's, and the bar after such a bar
    bool prevRegular = true;
    for (size_t i = 0; i < measures.size(); ++i) {
        const PlayedMeasure& m = measures[i];
        const auto hiddenIt = hiddenByMeasure.find(i);
        const int hidden = hiddenIt != hiddenByMeasure.end() ? hiddenIt->second : 0;
        const int ticks = m.ticks + hidden;
        const bool regular = (ticks == m.timeSig.ticks());

        bool timeSigChanged = false;
        if (i > 0) {
            const Fraction& prevSig = measures[i - 1].timeSig;
            timeSigChanged = prevSig.numerator() != m.timeSig.numerator() || prevSig.denominator() != m.timeSig.denominator();
        }

        if (i == 0 || timeSigChanged || !regular || !prevRegular) {
            BarPoint bar;
            bar.tick = map.playedTick(m.utick);
            bar.ticks = ticks;
            bar.hiddenTicks = hidden;
            bar.timeSig = m.timeSig;
            bar.playedTimeSig = regular ? m.timeSig : timeSigForTicks(ticks, m.timeSig);
            if (!bar.playedTimeSig.isValid()) {
                LOGW() << "no time signature for the bar of " << ticks << " ticks at utick " << m.utick
                       << ", later bars will not line up";
            }
            map.m_bars.push_back(bar);
        }

        prevRegular = regular;
    }

    return map;
}

int PlayedBeatMap::playedTick(int utick) const
{
    // The last pause at or before utick; the list is sorted by utick
    auto it = std::upper_bound(m_hidden.begin(), m_hidden.end(), utick, [](int u, const HiddenBeats& h) {
        return u < h.utick;
    });
    if (it == m_hidden.begin()) {
        return utick;
    }

    const HiddenBeats& h = *std::prev(it);
    return h.tick + h.ticks + (utick - h.utick);
}

int PlayedBeatMap::playedEndTick(int utick, int lateTicks) const
{
    if (!HOLD_NOTES_THROUGH_PAUSES) {
        // the last pause at or before utick, if utick is on it or at most lateTicks after it
        auto it = std::upper_bound(m_hidden.begin(), m_hidden.end(), utick, [](int u, const HiddenBeats& h) {
            return u < h.utick;
        });
        if (it != m_hidden.begin()) {
            const HiddenBeats& h = *std::prev(it);
            if (utick - h.utick <= std::max(0, lateTicks)) {
                return h.tick;
            }
        }
    }

    return playedTick(utick);
}

double PlayedBeatMap::secsAt(int playedTick) const
{
    if (m_tempo.empty()) {
        return 0.0;
    }

    auto it = std::upper_bound(m_tempo.begin(), m_tempo.end(), playedTick, [](int t, const TempoPoint& p) {
        return t < p.tick;
    });
    const TempoPoint& p = it == m_tempo.begin() ? m_tempo.front() : *std::prev(it);
    return p.secs + double(playedTick - p.tick) / (Constants::DIVISION * p.bps);
}
