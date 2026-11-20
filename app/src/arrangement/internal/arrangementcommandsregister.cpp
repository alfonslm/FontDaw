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

#include "arrangementcommandsregister.h"

#include "../arrangementcommands.h"

using namespace muse;
using namespace muse::rcommand;
using namespace mu::arrangement;

static const std::vector<CommandInfo> s_arrangementCommandInfos = {
    CommandInfo{
        IMPORT_AUDIO_TRACK_COMMAND,
        TranslatableString("arrangement", "Import audio track…"),
        TranslatableString("arrangement", "Add an audio file to the project as a track that plays with the score"),
        InputSchema(),
        Decoration()
    },
};

std::string ArrangementCommandsRegister::moduleName() const
{
    return "arrangement";
}

const std::vector<Command>& ArrangementCommandsRegister::commandList() const
{
    static std::vector<Command> commands;
    if (commands.empty()) {
        commands.reserve(s_arrangementCommandInfos.size());
        for (const auto& info : s_arrangementCommandInfos) {
            commands.push_back(info.command);
        }
    }
    return commands;
}

const std::vector<CommandInfo>& ArrangementCommandsRegister::commandInfoList() const
{
    return s_arrangementCommandInfos;
}
