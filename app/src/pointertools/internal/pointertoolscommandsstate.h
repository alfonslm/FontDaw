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

#include "rcommand/imodulecommandsstate.h"
#include "global/async/asyncable.h"
#include "global/modularity/ioc.h"
#include "project/iprojectsession.h"

namespace mu::pointertools {
//! OPEN_POINTER_MENU_COMMAND is enabled while a score is open (the only view with a pointer menu
//! for now) and no text is being edited in it. While text is edited (lyrics, chord symbols,
//! fingering, staff text), the text editor does not take Tab and no shortcut scope of the text
//! editor binds it, so the command must be disabled for Tab to fall back to next-panel: the
//! shortcuts controller drops disabled commands before it weighs the scopes. Same shape as
//! src/arrangement/internal/arrangementcommandsstate.h.
class PointerToolsCommandsState : public muse::rcommand::IModuleCommandsState, public muse::Contextable,
    public muse::async::Asyncable
{
    muse::ContextInject<project::IProjectSession> projectSession = { this };

public:
    PointerToolsCommandsState(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    std::string moduleName() const override;

    void init() override;
    void deinit() override;

    muse::rcommand::CommandState commandState(const muse::rcommand::Command& command) const override;
    muse::async::Channel<muse::rcommand::Command, muse::rcommand::CommandState> commandStateChanged() const override;

private:
    bool isTextEditing() const;
    void notifyStateChanged();
    void subscribeToTextEditing();

    muse::async::Channel<muse::rcommand::Command, muse::rcommand::CommandState> m_commandStateChanged;
};
}
