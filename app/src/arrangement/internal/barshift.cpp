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
#include "barshift.h"

#include <algorithm>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/repeatlist.h"

#include "../iarrangement.h"
#include "../iarrangementundo.h"

#include "log.h"

using namespace mu::arrangement;
using namespace mu::engraving;

namespace {
//! The repeat segment (pass) that holds a played tick, and the tick's written position
std::pair<size_t, Fraction> playedToWritten(const RepeatList& list, const Fraction& played)
{
    if (list.empty()) {
        return { 0, played };
    }
    auto it = list.findRepeatSegmentFromUTick(played.ticks());
    if (it == list.end()) {
        it = std::prev(list.end()); // past the end: the last pass
    }
    const RepeatSegment* segment = *it;
    return { static_cast<size_t>(it - list.begin()), Fraction::fromTicks(played.ticks() - segment->utick + segment->tick) };
}

//! The played tick of a written position in the same pass, where that pass still holds it
Fraction writtenToPlayed(const RepeatList& list, size_t segmentIndex, const Fraction& written)
{
    if (list.empty()) {
        return written;
    }
    if (segmentIndex < list.size()) {
        const RepeatSegment* segment = list.at(segmentIndex);
        if (written.ticks() >= segment->tick && written.ticks() < segment->endTick()) {
            return Fraction::fromTicks(written.ticks() - segment->tick + segment->utick);
        }
    }
    return Fraction::fromTicks(list.tick2utick(written.ticks()));
}
}

BarShift::BarShift(std::function<std::shared_ptr<IArrangement>()> arrangement, IArrangementUndo* undo)
    : m_arrangement(std::move(arrangement)), m_undo(undo)
{
}

void BarShift::setScore(MasterScore* score)
{
    m_score = score;
    m_hasSnapshot = false;
}

void BarShift::transactionBegan()
{
    m_hasSnapshot = false;
    m_shifted = false;
    m_bars.clear();
    m_anchors.clear();

    const std::shared_ptr<IArrangement> arrangement = m_arrangement ? m_arrangement() : nullptr;
    if (!m_score || m_shifting || !arrangement) {
        return;
    }

    bool anyMusicalClip = false;
    for (const Track& track : arrangement->tracks()) {
        anyMusicalClip = anyMusicalClip || (track.timeBase == TimeBase::Musical && !track.clips.empty());
    }
    if (!anyMusicalClip) {
        return; // nothing to move: keep score edits free of this work
    }

    for (const Measure* m = m_score->firstMeasure(); m; m = m->nextMeasure()) {
        m_bars.push_back({ m, m->tick(), m->endTick() });
    }
    if (m_bars.empty()) {
        return;
    }

    const RepeatList& repeats = m_score->expandedRepeatList();
    for (const Track& track : arrangement->tracks()) {
        if (track.timeBase != TimeBase::Musical) {
            continue; // Absolute clips keep their seconds; the bars do not concern them
        }
        for (const AudioClip& clip : track.clips) {
            ClipAnchor anchor;
            anchor.track = track.id;
            anchor.clip = clip.id;
            anchor.playedTick = clip.startTick;
            std::tie(anchor.repeatSegment, anchor.writtenTick) = playedToWritten(repeats, clip.startTick);

            auto bar = std::find_if(m_bars.begin(), m_bars.end(), [&anchor](const Bar& b) {
                return anchor.writtenTick >= b.tick && anchor.writtenTick < b.end;
            });
            if (bar == m_bars.end()) {
                anchor.barIndex = m_bars.size(); // past the last bar
                anchor.offset = anchor.writtenTick - m_bars.back().end;
            } else {
                anchor.barIndex = static_cast<size_t>(bar - m_bars.begin());
                anchor.offset = anchor.writtenTick - bar->tick;
            }
            m_anchors.push_back(anchor);
        }
    }
    m_hasSnapshot = !m_anchors.empty();
}

Fraction BarShift::newWrittenTick(const ClipAnchor& anchor) const
{
    auto isAlive = [this](const Measure* m) {
        return std::find(m_alive.begin(), m_alive.end(), m) != m_alive.end();
    };
    const Fraction newLastEnd = m_alive.empty() ? Fraction(0, 1) : m_alive.back()->endTick();

    if (anchor.barIndex >= m_bars.size()) {
        return newLastEnd + anchor.offset; // past the last bar: same distance from its end
    }

    const Bar& bar = m_bars[anchor.barIndex];
    if (isAlive(bar.measure)) {
        // A bar that got shorter than the offset puts the clip at its new end
        return bar.measure->tick() + std::min(anchor.offset, bar.measure->ticks());
    }

    // The bar is gone: replaced (a time signature rewrite, split, join) or deleted. Place the clip
    // from the nearest surviving bars before (P) and after (S) it (point A of the plan): a rewrite
    // keeps its tick, a deletion collapses it to where the range was.
    Fraction pOldEnd(0, 1);
    Fraction pNewEnd(0, 1);
    for (size_t i = anchor.barIndex; i-- > 0;) {
        if (isAlive(m_bars[i].measure)) {
            pOldEnd = m_bars[i].end;
            pNewEnd = m_bars[i].measure->endTick();
            break;
        }
    }
    Fraction sNewTick = newLastEnd;
    for (size_t i = anchor.barIndex + 1; i < m_bars.size(); ++i) {
        if (isAlive(m_bars[i].measure)) {
            sNewTick = m_bars[i].measure->tick();
            break;
        }
    }
    return std::min(pNewEnd + (anchor.writtenTick - pOldEnd), sNewTick);
}

void BarShift::transactionAboutToCommit(Transaction&)
{
    if (!m_hasSnapshot || m_shifting || !m_score || !m_undo) {
        return;
    }
    const std::shared_ptr<IArrangement> arrangement = m_arrangement ? m_arrangement() : nullptr;
    if (!arrangement) {
        return;
    }

    m_alive.clear();
    for (const Measure* m = m_score->firstMeasure(); m; m = m->nextMeasure()) {
        m_alive.push_back(m);
    }

    // The edit may have changed the repeats; endTransaction only invalidates the list later
    m_score->invalidateRepeatList();
    const RepeatList& repeats = m_score->expandedRepeatList();

    std::vector<ClipStartTick> moves;
    for (const ClipAnchor& anchor : m_anchors) {
        const Fraction written = newWrittenTick(anchor);
        const Fraction played = writtenToPlayed(repeats, anchor.repeatSegment, written).reduced();
        if (played != anchor.playedTick) {
            moves.push_back({ anchor.track, anchor.clip, played });
        }
    }
    m_alive.clear();
    if (moves.empty()) {
        return;
    }

    // Joins the open transaction: one undo restores the bars and the clips together
    m_shifting = true;
    const muse::Ret ret = m_undo->edit(muse::TranslatableString("undoableAction", "Move clips with their bars"),
                                       [&moves](IArrangement& model) { return model.setClipStartTicks(moves); });
    m_shifting = false;
    if (!ret) {
        LOGE() << "clips could not follow their bars: " << ret.toString();
        return;
    }
    m_shifted = true;
}

void BarShift::transactionClosed(bool committed)
{
    m_hasSnapshot = false;
    m_bars.clear();
    m_anchors.clear();

    // Ticks only were set; the seconds follow from the score's time map as it is now
    if (committed && m_shifted) {
        if (const std::shared_ptr<IArrangement> arrangement = m_arrangement ? m_arrangement() : nullptr) {
            arrangement->onTempoMapChanged();
        }
    }
    m_shifted = false;
}
