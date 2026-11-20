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

#include "modularity/imoduleinterface.h"
#include "global/async/notification.h"

#include "pointertoolstypes.h"

namespace mu::pointertools {
//! Every command the pointer menu can offer, for one kind of view: what the menu lists and what
//! search runs over. Built from sources (internal/icommandindexsource.h); keys are looked up
//! separately (internal/icommandkeylookup.h). One per window, because a command's enabled state
//! belongs to the window's context.
class ICommandIndex : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(ICommandIndex)

public:
    virtual ~ICommandIndex() = default;

    //! The enabled commands for the view, sorted by section then by the order the sources gave.
    //! Commands that are disabled now, or that need arguments the menu cannot give, are left out.
    virtual CommandEntryList entries(ViewKind view) const = 0;

    //! The entries ranked for a search text (internal/commandsearch.h). Empty text: entries().
    virtual CommandEntryList search(ViewKind view, const muse::String& text) const = 0;

    //! Runs an entry through the source that gave it (dispatches its command on the selection).
    virtual void run(const CommandEntry& entry) const = 0;

    //! Sent when a source's command list changes (a module registers commands later).
    virtual muse::async::Notification entriesChanged() const = 0;
};
}
