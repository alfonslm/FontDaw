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

#include "pointertoolscontroller.h"

#include "../pointertoolscommands.h"

using namespace muse;
using namespace mu::pointertools;

void PointerToolsController::init()
{
    dispatcher()->onRequest(this, OPEN_POINTER_MENU_COMMAND, [this]() {
        m_pointerMenuRequested.notify();
        return muse::make_ok();
    });
}

async::Notification PointerToolsController::pointerMenuRequested() const
{
    return m_pointerMenuRequested;
}
