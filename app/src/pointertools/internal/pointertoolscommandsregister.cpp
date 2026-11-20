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

#include "pointertoolscommandsregister.h"

#include "../pointertoolscommands.h"

using namespace muse;
using namespace muse::rcommand;
using namespace mu::pointertools;

static const std::vector<CommandInfo> s_pointerToolsCommandInfos = {
    CommandInfo{
        OPEN_POINTER_MENU_COMMAND,
        TranslatableString("pointertools", "Command menu"),
        TranslatableString("pointertools", "Open a menu at the pointer that lists the commands for this view and finds any command by name"),
        InputSchema(),
        Decoration()
    },
};

std::string PointerToolsCommandsRegister::moduleName() const
{
    return "pointertools";
}

const std::vector<Command>& PointerToolsCommandsRegister::commandList() const
{
    static std::vector<Command> commands;
    if (commands.empty()) {
        commands.reserve(s_pointerToolsCommandInfos.size());
        for (const auto& info : s_pointerToolsCommandInfos) {
            commands.push_back(info.command);
        }
    }
    return commands;
}

const std::vector<CommandInfo>& PointerToolsCommandsRegister::commandInfoList() const
{
    return s_pointerToolsCommandInfos;
}
