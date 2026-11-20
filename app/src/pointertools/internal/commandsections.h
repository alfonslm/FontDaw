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

#include "../pointertoolstypes.h"

namespace mu::pointertools {
//! The section a command belongs to in the pointer menu. Data, in commandsections.cpp: a table of
//! commands per section (articulations, fermatas, …) and a fallback by module (arrangement →
//! Audio, project and appshell → General). A command in neither is Section::Other.
Section sectionOf(const muse::rcommand::Command& command, const std::string& module);

//! The section's heading, translated.
muse::String sectionTitle(Section section);
}
