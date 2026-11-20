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

#include "rcommand/commandtypes.h"

namespace mu::pointertools {
//! Opens the pointer menu ("Command menu") at the pointer, or in the middle of the score view when
//! the pointer is not over it (it is on the menu bar when the command comes from Tools).
//! No key is bound here: Tab comes with the layout work (shortcut work order 08c, 12).
inline static const muse::rcommand::Command OPEN_POINTER_MENU_COMMAND("command://pointertools/open-pointer-menu");

// TODO(FontDaw shortcuts 10): REPEAT_LAST_COMMAND ("command://pointertools/repeat-last").
// TODO(FontDaw shortcuts 09): one command per family pop-up ("command://pointertools/family?name=…").
}
