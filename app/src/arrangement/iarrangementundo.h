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

#include "modularity/imoduleinterface.h"
#include "global/types/ret.h"
#include "global/types/translatablestring.h"

namespace mu::notation {
class IMasterNotation;
}

namespace mu::arrangement {
class IArrangement;

//! Batch 2 03a: arrangement edits in the score's undo history (option (a) of the workstream file
//! docs/fontdaw/batches/batch-02/03a-undo-layer.md). One undo stack, the master score's, holds
//! both: Ctrl+Z undoes the last edit wherever it was made.
//!
//! Every arrangement edit a user makes goes through edit(). Calls made on IArrangement directly
//! (loading, tests, the controller keeping links in step) are not recorded.
class IArrangementUndo : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IArrangementUndo)

public:
    virtual ~IArrangementUndo() = default;

    //! Runs `edit` as one undoable step named `actionName`, shown in the undo history.
    //! - Inside an open score transaction (an import adding its audio part, a transaction
    //!   observer) it joins that transaction, so the score's and the arrangement's changes are one
    //!   step. Otherwise it opens a transaction of its own on the master score's undo stack.
    //! - If `edit` fails, the arrangement is put back as it was and nothing is recorded.
    //! - If `edit` changes nothing in the history fields (sameHistory), nothing is recorded.
    //! - Without a score to record in (no project, the command-line converter), or while the undo
    //!   stack is locked, the edit runs and is not recorded; the project is marked unsaved.
    //! - When this call opened the transaction and the score rolls it back (a read-only score,
    //!   an engraving error), the arrangement is put back and the call fails.
    //!
    //! Contract for `edit` (transactions do not nest): it calls IArrangement and score functions
    //! that push into the current transaction, and never opens, commits or rolls back one
    //! (prepareChanges, commitChanges, beginTransaction, endTransaction). After it returns, edit()
    //! checks that its transaction is still the open one; if not, it puts the arrangement back,
    //! asserts and fails.
    //! Main thread only.
    virtual muse::Ret edit(const muse::TranslatableString& actionName, const std::function<muse::Ret(IArrangement&)>& edit) = 0;

    //! Where edits are recorded: the open project's master notation, held weakly (the controller
    //! sets it when a project opens, and passes an empty pointer when none is open).
    virtual void setTarget(std::weak_ptr<mu::notation::IMasterNotation> masterNotation) = 0;

    //! True while an edit() that will be recorded is running: the arrangement's changes sent now
    //! are in the undo history, so the controller does not mark the project unsaved for them.
    virtual bool isRecordingEdit() const = 0;
};
}
