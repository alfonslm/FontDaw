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
#include "rcommand/commandable.h"
#include "rcommand/icommanddispatcher.h"

#include "../ipointertoolscontroller.h"

namespace mu::pointertools {
//! Handles the module's commands and tells the QML layer what to open.
class PointerToolsController : public IPointerToolsController, public muse::Contextable, public muse::rcommand::Commandable,
    public muse::async::Asyncable
{
    muse::ContextInject<muse::rcommand::ICommandDispatcher> dispatcher = { this };

public:
    PointerToolsController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();

    muse::async::Notification pointerMenuRequested() const override;

private:
    muse::async::Notification m_pointerMenuRequested;
};
}
