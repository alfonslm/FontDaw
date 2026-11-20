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

#include "scorepositions.h"

#include <algorithm>

#include "engraving/dom/measure.h"
#include "engraving/dom/page.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tempotimeline.h"

using namespace mu::engraving;

namespace mu::arrangement {
namespace {
double endX(const Measure* measure)
{
    const Segment* endBar = measure->findSegment(SegmentType::EndBarLine, measure->endTick());
    return endBar ? endBar->canvasPos().x() : measure->canvasPos().x() + measure->width();
}
}

std::optional<ScoreX> tickToCanvasX(const Score* score, const Fraction& tick)
{
    if (!score) {
        return std::nullopt;
    }

    const Measure* measure = score->tick2measureMM(tick);
    if (!measure || tick < measure->tick() || tick >= measure->endTick()) {
        return std::nullopt;
    }
    const System* system = measure->system();
    if (!system || !system->page()) {
        return std::nullopt;
    }

    // As PlaybackCursor::resolveCursorRectByTick: linear between visible chord-rest segments,
    // the end bar line after the last one
    for (const Segment* s = measure->first(SegmentType::ChordRest); s;) {
        const Segment* next = s->next(SegmentType::ChordRest);
        while (next && !next->visible()) {
            next = next->next(SegmentType::ChordRest);
        }

        const Fraction startTick = s->tick();
        const Fraction endTick = next ? next->tick() : measure->endTick();
        if (tick >= startTick && tick < endTick) {
            const double startX = s->canvasPos().x();
            const double stopX = next ? next->canvasPos().x() : endX(measure);
            const Fraction duration = endTick - startTick;
            const double t = duration > Fraction(0, 1) ? (tick - startTick).toDouble() / duration.toDouble() : 0.0;
            return ScoreX { startX + (stopX - startX) * t, system };
        }
        s = next;
    }
    return std::nullopt;
}

std::optional<ScoreX> scoreEndX(const Score* score)
{
    const Measure* last = score ? score->lastMeasureMM() : nullptr;
    if (!last || !last->system()) {
        return std::nullopt;
    }
    return ScoreX { endX(last), last->system() };
}

std::optional<Fraction> secsToWrittenTick(const Score* score, double secs, bool expandRepeats)
{
    if (!score) {
        return std::nullopt;
    }
    return SecsToWrittenTick(score, expandRepeats)(secs);
}

SecsToWrittenTick::SecsToWrittenTick(const Score* score, bool expandRepeats)
{
    if (!score || !score->lastMeasure()) {
        return; // m_timeline stays null: every call answers nullopt
    }
    m_timeline = &score->tempoTimeline(expandRepeats);
    m_endPlayedTick = score->lastMeasure()->endTick().ticks();
    if (expandRepeats) {
        m_repeats = &score->expandedRepeatList();
        m_endPlayedTick = m_repeats->ticks();
    }
}

std::optional<Fraction> SecsToWrittenTick::operator()(double secs) const
{
    if (!m_timeline || secs < 0.0) {
        return std::nullopt;
    }
    const int playedTick = m_timeline->utime2utick(secs);
    if (playedTick >= m_endPlayedTick) {
        return std::nullopt;
    }

    // The flattened timeline is already in written ticks; the expanded one maps back
    const int written = m_repeats ? m_repeats->utick2tick(playedTick) : playedTick;
    return Fraction::fromTicks(written);
}

SystemTickToX::SystemTickToX(const System* system)
{
    if (!system || !system->page()) {
        return;
    }
    // The spans tickToCanvasX walks, for every measure of the system, in order
    for (const MeasureBase* mb : system->measures()) {
        if (!mb->isMeasure()) {
            continue;
        }
        const Measure* measure = toMeasure(mb);
        for (const Segment* s = measure->first(SegmentType::ChordRest); s;) {
            const Segment* next = s->next(SegmentType::ChordRest);
            while (next && !next->visible()) {
                next = next->next(SegmentType::ChordRest);
            }
            Span span;
            span.from = s->tick();
            span.to = next ? next->tick() : measure->endTick();
            span.x0 = s->canvasPos().x();
            span.x1 = next ? next->canvasPos().x() : endX(measure);
            if (span.to > span.from) {
                m_spans.push_back(span);
            }
            s = next;
        }
    }
}

std::optional<double> SystemTickToX::operator()(const Fraction& tick) const
{
    // The last span that starts at or before the tick
    auto it = std::upper_bound(m_spans.begin(), m_spans.end(), tick, [](const Fraction& t, const Span& span) {
        return t < span.from;
    });
    if (it == m_spans.begin()) {
        return std::nullopt;
    }
    --it;
    if (tick >= it->to) {
        return std::nullopt;
    }
    const double t = (tick - it->from).toDouble() / (it->to - it->from).toDouble();
    return it->x0 + (it->x1 - it->x0) * t;
}

std::optional<PassEnd> passEndAt(const Score* score, double secs, bool expandRepeats)
{
    if (!score || !expandRepeats || secs < 0.0) {
        return std::nullopt;
    }

    const RepeatList& repeats = score->expandedRepeatList();
    const int utick = score->tempoTimeline(true).utime2utick(secs);
    for (size_t i = 0; i < repeats.size(); ++i) {
        const RepeatSegment* rs = repeats[i];
        if (utick < rs->utick || utick >= rs->utick + rs->len()) {
            continue;
        }
        // Follow segments that continue in written order until one is followed by a jump
        for (size_t j = i; j < repeats.size(); ++j) {
            const RepeatSegment* cur = repeats[j];
            const int writtenEnd = cur->tick + cur->len();
            const bool last = j + 1 == repeats.size();
            if (last) {
                return std::nullopt;
            }
            if (repeats[j + 1]->tick != writtenEnd) {
                PassEnd end;
                end.endSecs = score->tempoTimeline(true).utick2utime(cur->utick + cur->len());
                end.endTick = Fraction::fromTicks(writtenEnd);
                return end;
            }
        }
    }
    return std::nullopt;
}

std::optional<SystemTicks> systemTicks(const System* system)
{
    const Measure* first = system ? system->firstMeasure() : nullptr;
    const Measure* last = system ? system->lastMeasure() : nullptr;
    if (!first || !last) {
        return std::nullopt;
    }
    return SystemTicks { system, first->tick(), last->endTick() };
}

std::vector<ClipPiece> clipPieces(const Score* score, double startSecs, double endSecs, bool expandRepeats, const Page* onlyPage)
{
    std::vector<ClipPiece> pieces;
    if (!score || endSecs <= startSecs) {
        return pieces;
    }

    const std::optional<Fraction> start = secsToWrittenTick(score, startSecs, expandRepeats);
    if (!start) {
        return pieces;
    }

    // The end is cut at the pass end, then at the end of the score
    if (!score->lastMeasure()) {
        return pieces;
    }
    const Fraction scoreEnd = score->lastMeasure()->endTick();
    Fraction endTick = scoreEnd;
    bool cutAtScoreEnd = true;
    if (const std::optional<Fraction> end = secsToWrittenTick(score, endSecs, expandRepeats)) {
        endTick = *end;
        cutAtScoreEnd = false;
    } else {
        // An end exactly at the end of the score lies outside it by one tick, but is not cut there
        const TempoTimeline& timeline = score->tempoTimeline(expandRepeats);
        const int endPlayedTick = expandRepeats ? score->expandedRepeatList().ticks() : scoreEnd.ticks();
        if (endSecs <= timeline.utick2utime(endPlayedTick) + 1e-9) {
            cutAtScoreEnd = false;
        }
    }
    if (const std::optional<PassEnd> pass = passEndAt(score, startSecs, expandRepeats)) {
        if (endSecs > pass->endSecs) {
            endTick = pass->endTick;
            cutAtScoreEnd = false;
        }
    }
    if (endTick > scoreEnd) {
        endTick = scoreEnd;
        cutAtScoreEnd = true;
    }
    if (endTick <= *start) {
        return pieces;
    }

    for (const System* system : onlyPage ? onlyPage->systems() : score->systems()) {
        const std::optional<SystemTicks> ticks = systemTicks(system);
        if (!ticks || !system->page() || ticks->to <= *start || ticks->from >= endTick) {
            continue;
        }

        ClipPiece piece;
        piece.system = system;

        // A continued piece starts where the system's first notes start, after the clefs and key
        // signatures of the other staves (07's page-view picture), as a clip that starts there does
        piece.continuesLeft = *start < ticks->from;
        const std::optional<ScoreX> x0 = tickToCanvasX(score, piece.continuesLeft ? ticks->from : *start);
        if (!x0) {
            continue;
        }
        piece.x0 = x0->x;

        const bool endsHere = endTick <= ticks->to;
        if (!endsHere) {
            piece.x1 = endX(system->lastMeasure());
            piece.continuesRight = true;
        } else if (endTick == ticks->to) {
            // The end is a bar line: take the bar line's own x, so pieces meet it exactly
            piece.x1 = endX(system->lastMeasure());
            piece.continuesRight = cutAtScoreEnd;
        } else if (const Measure* before = score->tick2measureMM(endTick - Fraction::fromTicks(1));
                   before && before->endTick() == endTick) {
            // The end falls on a bar line inside the system (e.g. the end of a repeat pass): use the
            // measure's end bar line, not the start of the next measure, which is further right
            piece.x1 = endX(before);
        } else {
            const std::optional<ScoreX> x = tickToCanvasX(score, endTick);
            if (!x) {
                continue;
            }
            piece.x1 = x->x;
        }
        if (endsHere && cutAtScoreEnd) {
            piece.continuesRight = true;
        }

        pieces.push_back(piece);
    }
    return pieces;
}
}
