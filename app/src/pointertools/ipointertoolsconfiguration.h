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

#include "modularity/imoduleinterface.h"
#include "global/async/channel.h"

namespace mu::pointertools {
class IPointerToolsConfiguration : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IPointerToolsConfiguration)

public:
    virtual ~IPointerToolsConfiguration() = default;

    //! Setting "pointertools/ringMenuOnSecondaryClick", on by default. When on, the secondary
    //! click, the Menu key, Shift+F10 and macOS Control+click open the ring menu instead of the
    //! context menu; the ring's "More…" opens the context menu. When off, nothing changes.
    //! Stands in for "the FontDaw layout is active" until the layouts exist (work order 04a):
    //! then this getter asks the layout instead, and the setting is removed or kept as an option.
    virtual bool ringMenuOnSecondaryClick() const = 0;
    virtual void setRingMenuOnSecondaryClick(bool on) = 0;
    virtual muse::async::Channel<bool> ringMenuOnSecondaryClickChanged() const = 0;
};
}
