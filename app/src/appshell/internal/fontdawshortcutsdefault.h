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

#include "modularity/ioc.h"
#include "shortcuts_v2/ishortcutsconfiguration.h"

namespace mu::appshell {
//! FontDaw (FD-153): the shortcut layout a user has before they choose one.
//!
//! Since the upstream merge (FD-109), shortcuts v2's base file holds upstream's MuseScore 5.0 keys.
//! FontDaw's default stays the MuseScore 4.7 keys it had before, as the preset named by
//! "fontdaw_default_preset" in shortcuts.cfg. It is applied once, at the first start of a build that
//! has it, and only when no other preset is set: a later choice of the base ("Default", the 5.0 keys)
//! is kept, and so is a user who already changed keys of the base (a user file for it exists).
//! The rule itself is fontDawShortcutsPresetToApply() (fontdawshortcutsdefaultrule.h). Interim until
//! the shortcut layouts' A3 (docs/fontdaw/batches/shortcuts/00-v2-plan.md).
class FontDawShortcutsDefault
{
    muse::GlobalInject<muse::shortcuts::IShortcutsConfiguration> shortcutsConfiguration;

public:
    void init();
};
}
