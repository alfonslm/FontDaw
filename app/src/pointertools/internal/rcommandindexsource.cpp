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

#include "rcommandindexsource.h"

#include <set>

#include "commandsections.h"

using namespace muse;
using namespace muse::rcommand;
using namespace mu::pointertools;

//! The modules whose commands the menu offers in a score view. Not pointertools: the menu does
//! not list itself.
static bool isModuleOfView(ViewKind view, const std::string& module)
{
    static const std::set<std::string> SCORE_MODULES = {
        "notation", "palette", "playback", "arrangement", "project", "appshell"
    };

    switch (view) {
    case ViewKind::Score: return SCORE_MODULES.find(module) != SCORE_MODULES.end();
    case ViewKind::Undefined:
    case ViewKind::Mixer:
        break;
    }
    return false;
}

CommandEntryList RCommandIndexSource::entries(ViewKind view) const
{
    CommandEntryList result;

    for (const CommandInfo& info : commandsRegister()->commandInfoList()) {
        if (!info.isValid() || !info.inputSchema.args.empty()) {
            continue;
        }

        const std::string& module = commandsRegister()->commandModuleName(info.command);
        if (!isModuleOfView(view, module)) {
            continue;
        }

        CommandEntry entry;
        entry.command = info.command;
        entry.title = String::fromQString(info.title.qTranslatedWithoutMnemonic());
        entry.description = info.description.translated();
        entry.icon = info.decoration.iconCode;
        entry.module = module;
        entry.section = sectionOf(info.command, module);
        result.push_back(std::move(entry));
    }

    return result;
}

bool RCommandIndexSource::isEnabled(const Command& command) const
{
    if (!commandsState()) {
        return false;
    }
    return commandsState()->commandState(command).enabled;
}

void RCommandIndexSource::run(const CommandEntry& entry) const
{
    dispatcher()->dispatch(entry.command);
}

async::Notification RCommandIndexSource::entriesChanged() const
{
    return commandsRegister()->commandInfoListChanged();
}
