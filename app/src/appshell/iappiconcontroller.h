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

#include <QString>

#include "modularity/imoduleinterface.h"
#include "async/notification.h"

#include "appicontypes.h"

namespace mu::appshell {
//! The icon FontDaw shows while it runs, resolved from the "App icon" setting and the system colour scheme.
class IAppIconController : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IAppIconController)

public:
    virtual ~IAppIconController() = default;

    //! The variant actually shown now.
    virtual AppIconVariant effectiveVariant() const = 0;

    //! "qrc" URL of the SVG for effectiveVariant(), for QML Image.source.
    virtual QString iconSource() const = 0;

    //! Notifies when effectiveVariant() changes, so QML models can re-read iconSource().
    virtual muse::async::Notification iconChanged() const = 0;
};
}
