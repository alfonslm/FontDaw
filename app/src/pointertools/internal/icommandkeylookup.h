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

#include <memory>

#include "global/types/string.h"
#include "rcommand/commandtypes.h"

namespace mu::pointertools {
//! The keys bound to a command, as the text the menu shows (e.g. "Shift+S"; several joined with
//! ", "). Empty when none is bound.
class ICommandKeyLookup
{
public:
    virtual ~ICommandKeyLookup() = default;

    virtual muse::String keys(const muse::rcommand::CommandInfo& info) const = 0;
};
using ICommandKeyLookupPtr = std::shared_ptr<ICommandKeyLookup>;
}
