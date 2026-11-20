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

#include "global/modularity/ioc.h"
#include "project/iprojectsession.h"
#include "rcommand/icommandsregister.h"
#include "rcommand/icommandsstate.h"

#include "../iringmenuprovider.h"
#include "ringselection.h"

namespace mu::pointertools {
//! Picks the table for the selection (see IRingMenuProvider::scoreRing) and turns it into slots:
//! title and icon from the command's CommandInfo, empty where the command is disabled now.
class RingMenuProvider : public IRingMenuProvider, public muse::Contextable
{
    muse::ContextInject<project::IProjectSession> projectSession = { this };
    muse::GlobalInject<muse::rcommand::ICommandsRegister> commandsRegister;
    muse::ContextInject<muse::rcommand::ICommandsState> commandsState = { this };

public:
    RingMenuProvider(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    RingContents scoreRing(const std::vector<TodayMenuItem>& todayMenu) const override;

    //! The ring for a kind of selection. Public so tests can call it without a view.
    RingContents ringFor(ScoreItemKind kind, const std::vector<TodayMenuItem>& todayMenu) const;

private:
    ScoreItemKind currentItemKind() const;
};
}
