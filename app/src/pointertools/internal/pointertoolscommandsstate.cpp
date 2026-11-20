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

#include "pointertoolscommandsstate.h"

#include "notation/inotation.h"
#include "notation/inotationinteraction.h"

#include "../pointertoolscommands.h"

using namespace muse;
using namespace muse::rcommand;
using namespace mu::pointertools;

std::string PointerToolsCommandsState::moduleName() const
{
    return "pointertools";
}

void PointerToolsCommandsState::init()
{
    projectSession()->currentNotationChanged().onNotify(this, [this]() {
        subscribeToTextEditing();
        notifyStateChanged();
    });
    subscribeToTextEditing();
}

void PointerToolsCommandsState::deinit()
{
    projectSession()->currentNotationChanged().disconnect(this);
}

void PointerToolsCommandsState::subscribeToTextEditing()
{
    const mu::notation::INotationPtr notation = projectSession()->currentNotation();
    if (!notation || !notation->interaction()) {
        return;
    }

    const mu::notation::INotationInteractionPtr interaction = notation->interaction();
    interaction->textEditingStarted().onNotify(this, [this]() {
        notifyStateChanged();
    }, async::Asyncable::Mode::SetReplace);
    interaction->textEditingEnded().onReceive(this, [this](mu::engraving::TextBase*) {
        notifyStateChanged();
    }, async::Asyncable::Mode::SetReplace);
}

void PointerToolsCommandsState::notifyStateChanged()
{
    m_commandStateChanged.send(OPEN_POINTER_MENU_COMMAND, commandState(OPEN_POINTER_MENU_COMMAND));
}

bool PointerToolsCommandsState::isTextEditing() const
{
    const mu::notation::INotationPtr notation = projectSession()->currentNotation();
    return notation && notation->interaction() && notation->interaction()->isTextEditingStarted();
}

CommandState PointerToolsCommandsState::commandState(const Command& command) const
{
    if (command == OPEN_POINTER_MENU_COMMAND) {
        return CommandState(projectSession()->currentNotation() != nullptr && !isTextEditing());
    }
    return CommandState();
}

async::Channel<Command, CommandState> PointerToolsCommandsState::commandStateChanged() const
{
    return m_commandStateChanged;
}
