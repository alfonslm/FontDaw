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
#include "arrangementundo.h"

#include "../iarrangement.h"

#include "engraving/dom/masterscore.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transaction/undostack.h"

#include "log.h"

using namespace mu::arrangement;
using namespace muse;

ArrangementSnapshotCommand::ArrangementSnapshotCommand(std::weak_ptr<IArrangement> arrangement, uint64_t epoch, TrackList before,
                                                       TrackList after)
    : m_arrangement(std::move(arrangement)), m_epoch(epoch), m_before(std::move(before)), m_after(std::move(after))
{
}

void ArrangementSnapshotCommand::undo()
{
    const std::shared_ptr<IArrangement> arrangement = m_arrangement.lock();
    if (arrangement && arrangement->epoch() == m_epoch) {
        arrangement->restoreTracks(m_before);
    }
}

void ArrangementSnapshotCommand::redo()
{
    const std::shared_ptr<IArrangement> arrangement = m_arrangement.lock();
    if (arrangement && arrangement->epoch() == m_epoch) {
        arrangement->restoreTracks(m_after);
    }
}

ArrangementUndo::ArrangementUndo(const muse::modularity::ContextPtr& ctx)
    : muse::Contextable(ctx)
{
    m_barShift = std::make_unique<BarShift>([this]() { return std::shared_ptr<IArrangement>(arrangement()); }, this);
}

ArrangementUndo::~ArrangementUndo()
{
    observe(nullptr);
}

void ArrangementUndo::observe(mu::engraving::MasterScore* score)
{
    // No early return when score == m_observed: a destroyed score's address can come back as a
    // new score's. Removing and adding again is cheap (addObserver ignores a second add).
    // The old score can be reached through the master notation still holding it, or it is the
    // test's score (the test keeps it alive until it passes null). A destroyed score took its
    // observer list with it: nothing to remove then.
    mu::engraving::MasterScore* reachable = nullptr;
    if (m_scoreForTests == m_observed) {
        reachable = m_observed;
    } else if (const mu::notation::IMasterNotationPtr master = m_masterNotation.lock()) {
        reachable = master->masterScore() == m_observed ? m_observed : nullptr;
    }
    if (reachable && reachable->transactionManager()) {
        reachable->transactionManager()->removeObserver(m_barShift.get());
    }

    m_observed = score;
    m_barShift->setScore(score);
    if (score && score->transactionManager()) {
        score->transactionManager()->addObserver(m_barShift.get());
    }
}

void ArrangementUndo::setTarget(std::weak_ptr<mu::notation::IMasterNotation> masterNotation)
{
    const mu::notation::IMasterNotationPtr master = masterNotation.lock();
    observe(master ? master->masterScore() : nullptr); // before m_masterNotation changes: reaches the old score
    m_masterNotation = std::move(masterNotation);
}

void ArrangementUndo::setScoreForTests(mu::engraving::MasterScore* score)
{
    if (score) {
        m_scoreForTests = score;
        observe(score);
    } else {
        observe(nullptr); // m_scoreForTests is still the old score here: it can be reached
        m_scoreForTests = nullptr;
    }
}

bool ArrangementUndo::isRecordingEdit() const
{
    return m_recording > 0;
}

mu::notation::INotationUndoStackPtr ArrangementUndo::undoStackFor(const mu::notation::IMasterNotationPtr& master) const
{
    if (!master) {
        return nullptr;
    }
    if (projectSession() && projectSession()->masterNotation() == master) {
        if (const mu::notation::INotationPtr current = projectSession()->currentNotation()) {
            if (current->undoStack()) {
                return current->undoStack();
            }
        }
    }
    return master->notation() ? master->notation()->undoStack() : nullptr;
}

Ret ArrangementUndo::edit(const TranslatableString& actionName, const std::function<Ret(IArrangement&)>& edit)
{
    using namespace mu::engraving;

    const std::shared_ptr<IArrangement> arr = arrangement();
    if (!arr || !edit) {
        return make_ret(Ret::Code::InternalError);
    }

    const TrackList before = arr->tracks();

    // A failed edit leaves the arrangement as it was, recorded or not
    auto putBack = [&arr, &before]() {
        if (!sameHistory(arr->tracks(), before)) {
            arr->restoreTracks(before);
        }
    };

    MasterScore* score = m_scoreForTests;
    mu::notation::INotationUndoStackPtr stack;
    if (!score) {
        if (const mu::notation::IMasterNotationPtr master = m_masterNotation.lock()) {
            score = master->masterScore();
            stack = undoStackFor(master);
        }
    }

    // Nowhere to record (no project, the converter) or a locked stack: run unrecorded, as a score
    // edit runs while the stack is locked. The controller marks the project unsaved.
    if (!score || !score->transactionManager() || score->undoStack()->isLocked()) {
        const Ret ret = edit(*arr);
        if (!ret) {
            putBack();
        }
        return ret;
    }

    TransactionManager* transactions = score->transactionManager();
    Transaction* transaction = transactions->currentTransaction();
    const bool opened = transaction == nullptr;
    const size_t stepsBefore = score->undoStack()->currentIndex();

    if (opened) {
        if (stack) {
            stack->prepareChanges(actionName);
        } else {
            transactions->beginTransaction(actionName);
        }
        transaction = transactions->currentTransaction();
        if (!transaction) {
            LOGE() << "could not open a transaction for an arrangement edit";
            return make_ret(Ret::Code::InternalError);
        }
    }

    auto close = [&](bool rollback) {
        if (!opened) {
            return;
        }
        if (stack) {
            rollback ? stack->rollbackChanges() : stack->commitChanges();
        } else {
            transactions->endTransaction(rollback);
        }
    };

    ++m_recording;
    const Ret ret = edit(*arr);
    --m_recording;

    // The contract: edit() never opens, commits or rolls back a transaction itself
    if (transactions->currentTransaction() != transaction) {
        // Logged, not asserted, so the test of this path can run in a debug build
        LOGE() << "an arrangement edit opened, committed or rolled back a transaction (IArrangementUndo::edit contract)";
        putBack();
        return make_ret(Ret::Code::InternalError);
    }

    if (!ret) {
        putBack();
        close(/*rollback*/ true);
        return ret;
    }

    const TrackList after = arr->tracks();
    if (sameHistory(before, after)) {
        close(/*rollback*/ false); // keeps any score change edit() made; an empty step is discarded
        return ret;
    }

    transaction->pushWithoutPerforming(new ArrangementSnapshotCommand(arr, arr->epoch(), before, after));

    if (opened) {
        close(/*rollback*/ false);
        // A rollback by the score (read-only, an engraving error) leaves no new step: the unwind
        // undid the command already; make sure, and fail
        if (score->undoStack()->currentIndex() != stepsBefore + 1) {
            putBack();
            return make_ret(Ret::Code::UnknownError);
        }
    }

    return ret;
}
