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

#include "arrangementcommandsstate.h"

#include "../arrangementcommands.h"

#include "log.h"

using namespace muse;
using namespace muse::rcommand;
using namespace mu::arrangement;

std::string ArrangementCommandsState::moduleName() const
{
    return "arrangement";
}

void ArrangementCommandsState::init()
{
    projectSession()->projectChanged().onNotify(this, [this]() {
        updateCommandStates();
    });

    m_moduleRegister = commandsRegister()->moduleRegister(moduleName());
    IF_ASSERT_FAILED(m_moduleRegister) {
        return;
    }

    updateCommandStates();
}

void ArrangementCommandsState::deinit()
{
    projectSession()->projectChanged().disconnect(this);
}

void ArrangementCommandsState::updateCommandStates()
{
    IF_ASSERT_FAILED(m_moduleRegister) {
        return;
    }

    for (const Command& command : m_moduleRegister->commandList()) {
        CommandState newState = commandState(command);
        if (m_commandStates[command] != newState) {
            m_commandStates[command] = newState;
            m_commandStateChanged.send(command, newState);
        }
    }
}

CommandState ArrangementCommandsState::commandState(const Command&) const
{
    // Audio tracks belong to a project
    const bool enabled = projectSession()->project() != nullptr;
    return CommandState(enabled, false);
}

async::Channel<Command, CommandState> ArrangementCommandsState::commandStateChanged() const
{
    return m_commandStateChanged;
}
