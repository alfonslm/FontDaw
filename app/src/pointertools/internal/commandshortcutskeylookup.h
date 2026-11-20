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
#include "shortcuts_v2/icommandshortcutsregister.h"

#include "icommandkeylookup.h"

namespace mu::pointertools {
//! The keys of a command from shortcuts v2's register (FD-143): the sequences bound to the
//! command's URI in the current layout, with the user's changes, as the native text the app's
//! menus show (AbstractMenuModel uses the same register and formatting). Read when the Command
//! menu opens, so a key changed in Preferences shows the next time it opens.
class CommandShortcutsKeyLookup : public ICommandKeyLookup
{
    muse::GlobalInject<muse::shortcuts::ICommandShortcutsRegister> shortcutsRegister;

public:
    muse::String keys(const muse::rcommand::CommandInfo& info) const override;
};
}
