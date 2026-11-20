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

#include <string>

#include "ui/uitypes.h"

namespace mu::pointertools {
//! An icon for an item of today's context menu that has none of its own (FD-156, FD-166), so the
//! ring does not have to fall back to two letters. Looked up by the item's id, which is the
//! command's URI; anything after a '?' (the parameters of a command query) is ignored.
//! IconCode::Code::NONE when no icon is known for it.
muse::ui::IconCode::Code ringFallbackIcon(const std::string& commandUri);
}
