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
#include "global/async/notification.h"

namespace mu::pointertools {
//! The pop-up host's C++ side. Commands arrive here; the QML layer in each view listens and opens
//! the pop-up at the pointer. One per window (context interface).
class IPointerToolsController : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IPointerToolsController)

public:
    virtual ~IPointerToolsController() = default;

    //! Sent when OPEN_POINTER_MENU_COMMAND runs. The view that has (or last had) the focus opens
    //! the pointer menu.
    virtual muse::async::Notification pointerMenuRequested() const = 0;

    // TODO(FontDaw shortcuts 09): familyRequested(name), sent by the family commands; the same
    // layer opens the family pop-up at the pointer. M2 cancels an open family pop-up (09's rule).
    // TODO(FontDaw shortcuts 10): lastCommand() and repeatLast(pos): listens to the rcommand
    // dispatcher's post-dispatch notification (overview T7; framework change, asked first).
};
}
