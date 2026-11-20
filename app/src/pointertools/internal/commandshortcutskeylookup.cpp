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
#include "commandshortcutskeylookup.h"

#include "shortcuts_v2/shortcutstypes.h"

using namespace muse;
using namespace mu::pointertools;

String CommandShortcutsKeyLookup::keys(const rcommand::CommandInfo& info) const
{
    if (!shortcutsRegister()) {
        return String();
    }

    const shortcuts::Shortcut& shortcut = shortcutsRegister()->shortcut(info.command);
    if (shortcut.sequences.empty()) {
        return String();
    }

    return String::fromQString(shortcuts::sequencesToNativeText(shortcut.sequences));
}
