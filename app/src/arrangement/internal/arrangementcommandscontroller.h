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

#include "global/types/ret.h"
#include "global/modularity/ioc.h"
#include "async/asyncable.h"
#include "rcommand/commandable.h"
#include "rcommand/icommanddispatcher.h"
#include "rcommand/icommandsstate.h"
#include "interactive/iinteractive.h"
#include "project/iprojectsession.h"

#include "../iarrangementcontroller.h"

namespace mu::arrangement {
//! The module's commands. For now one: File > Import audio track…
class ArrangementCommandsController : public muse::Contextable, public muse::rcommand::Commandable, public muse::async::Asyncable
{
    muse::ContextInject<muse::rcommand::ICommandDispatcher> dispatcher = { this };
    muse::ContextInject<muse::rcommand::ICommandsState> commandsState = { this };
    muse::ContextInject<muse::IInteractive> interactive = { this };
    muse::ContextInject<project::IProjectSession> projectSession = { this };
    muse::ContextInject<IArrangementController> arrangementController = { this };

public:
    ArrangementCommandsController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();

private:
    muse::Ret importAudioTrack();
};
}
