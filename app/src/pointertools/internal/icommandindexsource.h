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

#include "global/async/notification.h"

#include "../pointertoolstypes.h"

namespace mu::pointertools {
//! One place commands come from. The index asks every source and merges the results; a command
//! that two sources give is kept once (the first source wins).
//! - RCommandIndexSource: every rcommand command (since the upstream merge, FD-109, every action
//!   is one). Their keys come through ICommandKeyLookup (on shortcuts_v2, FD-143).
//! - Later, a palette-cell source (step 6).
class ICommandIndexSource
{
public:
    virtual ~ICommandIndexSource() = default;

    //! Every command this source has for the view, enabled or not, with section and module set
    //! and keys empty. The index filters by state and fills the keys.
    virtual CommandEntryList entries(ViewKind view) const = 0;

    //! Whether the command can run now (its CommandState::enabled).
    virtual bool isEnabled(const muse::rcommand::Command& command) const = 0;

    //! Runs an entry: dispatches its command with no arguments, on the selection.
    virtual void run(const CommandEntry& entry) const = 0;

    virtual muse::async::Notification entriesChanged() const = 0;
};
using ICommandIndexSourcePtr = std::shared_ptr<ICommandIndexSource>;
}
