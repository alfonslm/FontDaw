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

#include <functional>
#include <memory>
#include <vector>

#include "engraving/editing/transaction/transaction.h"
#include "engraving/types/fraction.h"

#include "../arrangementtypes.h"

namespace mu::engraving {
class MasterScore;
class Measure;
}

namespace mu::arrangement {
class IArrangement;
class IArrangementUndo;

//! FD-151: clips on Musical tracks move with their bars when a score edit inserts, deletes or
//! rewrites bars before or around them. One observer of every score transaction, not a hook per
//! command (plan: docs/fontdaw/batches/batch-02/03a-undo-layer.md, "Plan for FD-151").
//!
//! - began: records every bar (its object, tick and end) and, per clip on a Musical track, the bar
//!   that holds its start (written tick, through the repeat list) and its offset into that bar.
//! - about to commit: computes each clip's new start from its bar, or, if the bar is gone, from the
//!   nearest surviving bars (point A of the plan); if any start moved, records one arrangement
//!   edit in the same transaction, so one undo restores the bars and the clips together.
//! - closed: drops the snapshot.
//! Bar-granular (version 1): a clip keeps its offset into its bar; a bar that got shorter than the
//! offset puts the clip at the bar's new end; a clip past the last bar keeps its distance from the
//! end of the last bar. Clip lengths never change (Q-E: bars deleted under a clip leave it whole).
class BarShift : public mu::engraving::ITransactionObserver
{
public:
    //! `undo` records the shift (it joins the open transaction); it outlives this object.
    BarShift(std::function<std::shared_ptr<IArrangement>()> arrangement, IArrangementUndo* undo);

    //! The score whose transactions are watched; null: none. Not owned.
    void setScore(mu::engraving::MasterScore* score);

    void transactionBegan() override;
    void transactionAboutToCommit(mu::engraving::Transaction& transaction) override;
    void transactionClosed(bool committed) override;

private:
    struct Bar {
        const mu::engraving::Measure* measure = nullptr;
        mu::engraving::Fraction tick;
        mu::engraving::Fraction end;
    };
    struct ClipAnchor {
        ArrTrackId track = INVALID_TRACK;
        ClipId clip = INVALID_CLIP;
        mu::engraving::Fraction playedTick;  // the stored start
        mu::engraving::Fraction writtenTick; // its written position when the transaction began
        size_t barIndex = 0;                 // into m_bars; m_bars.size(): past the last bar
        mu::engraving::Fraction offset;      // into that bar (or past the last bar's end)
        size_t repeatSegment = 0;            // which pass it is in (Q-D: keep the pass)
    };

    //! The new written start of a clip, from the bars as they are now
    mu::engraving::Fraction newWrittenTick(const ClipAnchor& anchor) const;

    std::function<std::shared_ptr<IArrangement>()> m_arrangement;
    IArrangementUndo* m_undo = nullptr;
    mu::engraving::MasterScore* m_score = nullptr;

    bool m_hasSnapshot = false;
    std::vector<Bar> m_bars;
    std::vector<ClipAnchor> m_anchors;
    std::vector<const mu::engraving::Measure*> m_alive; // the bars as they are at "about to commit"
    bool m_shifting = false;
    bool m_shifted = false;
};
}
