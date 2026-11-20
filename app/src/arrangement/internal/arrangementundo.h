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

#include <memory>

#include "../iarrangementundo.h"
#include "barshift.h"
#include "../arrangementtypes.h"

#include "modularity/ioc.h"
#include "engraving/editing/transaction/undoablecommand.h"
#include "notation/imasternotation.h"
#include "notation/inotationundostack.h"
#include "project/iprojectsession.h"

namespace mu::engraving {
class MasterScore;
}

namespace mu::arrangement {
//! One recorded arrangement edit: the track list before and after it.
//!
//! A snapshot rather than one command class per model call: every edit the model offers (and
//! any added later) is undoable the same way, and undo gives back exactly the earlier state, ids
//! included. A track list is small (clips are descriptions; the audio is in the asset store and
//! never copied, README decision 4).
//!
//! Records the arrangement's epoch: a command from another epoch (another project opened in the
//! same window, whose one Arrangement outlives every project) does nothing.
class ArrangementSnapshotCommand : public mu::engraving::UndoableCommand
{
public:
    ArrangementSnapshotCommand(std::weak_ptr<IArrangement> arrangement, uint64_t epoch, TrackList before, TrackList after);

    void undo() override;
    void redo() override;

    const char* name() const override { return "ArrangementEdit"; }

    const TrackList& before() const { return m_before; }
    const TrackList& after() const { return m_after; }

private:
    std::weak_ptr<IArrangement> m_arrangement;
    uint64_t m_epoch = 0;
    TrackList m_before;
    TrackList m_after;
};

class ArrangementUndo : public IArrangementUndo, public muse::Contextable
{
    muse::ContextInject<IArrangement> arrangement = { this };
    muse::ContextInject<project::IProjectSession> projectSession = { this };

public:
    ArrangementUndo(const muse::modularity::ContextPtr& ctx);
    ~ArrangementUndo() override;

    //! Where edits are recorded: the open project's master notation, held weakly; the score is
    //! read from it at each edit. The controller sets it when a project opens and passes null
    //! when none is open. Transactions are opened through the current notation's undo stack (a
    //! part view's when one is shown), so that view's Undo menu and history panel update.
    void setTarget(std::weak_ptr<mu::notation::IMasterNotation> masterNotation) override;

    //! Tests only: record in this score, opening transactions on its TransactionManager.
    //! The caller keeps the score alive until it calls this again with null.
    void setScoreForTests(mu::engraving::MasterScore* score);

    muse::Ret edit(const muse::TranslatableString& actionName, const std::function<muse::Ret(IArrangement&)>& edit) override;
    bool isRecordingEdit() const override;

private:
    //! The undo stack to open a transaction through: the current notation's (a part view's when
    //! one is shown, so that view's Undo menu and history panel update), else the master's.
    mu::notation::INotationUndoStackPtr undoStackFor(const mu::notation::IMasterNotationPtr& master) const;

    //! FD-151: watches the target score's transactions; registered on it by setTarget and
    //! setScoreForTests, removed from the old one while that can still be reached
    void observe(mu::engraving::MasterScore* score);

    std::weak_ptr<mu::notation::IMasterNotation> m_masterNotation;
    mu::engraving::MasterScore* m_scoreForTests = nullptr;
    std::unique_ptr<BarShift> m_barShift;
    mu::engraving::MasterScore* m_observed = nullptr; // compared only, never dereferenced unless reachable
    int m_recording = 0; // a counter: an edit can be recorded while another one is (FD-151's shift)
};
}
