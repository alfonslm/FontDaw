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

#include "global/async/asyncable.h"
#include "global/modularity/ioc.h"
#include "rcommand/icommandsregister.h"
#include "rcommand/icommandsstate.h"
#include "rcommand/icommanddispatcher.h"

#include "icommandindexsource.h"

namespace mu::pointertools {
//! Every command in the rcommand register (ICommandsRegister::commandInfoList()), filtered by
//! which modules belong to the view (score: notation, palette, playback, arrangement, project,
//! appshell, pointertools), sorted into sections by internal/commandsections.cpp. Commands whose
//! InputSchema has arguments are left out: the menu has none to give (they are the "screen-…"
//! commands that need a position, and the like).
class RCommandIndexSource : public ICommandIndexSource, public muse::Contextable, public muse::async::Asyncable
{
    muse::GlobalInject<muse::rcommand::ICommandsRegister> commandsRegister;
    muse::ContextInject<muse::rcommand::ICommandsState> commandsState = { this };
    muse::ContextInject<muse::rcommand::ICommandDispatcher> dispatcher = { this };

public:
    RCommandIndexSource(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    CommandEntryList entries(ViewKind view) const override;
    bool isEnabled(const muse::rcommand::Command& command) const override;
    void run(const CommandEntry& entry) const override;
    muse::async::Notification entriesChanged() const override;
};
}
