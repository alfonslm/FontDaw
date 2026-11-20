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

#include "pointertoolsconfiguration.h"

#include "settings.h"
#include "translation.h"

using namespace mu::pointertools;
using namespace muse;

static const std::string moduleName("pointertools");

//! On by default since 18 November 2026 (the owner's decision): the secondary click opens the ring
//! menu. Listed in Preferences → Advanced under its description so it can be switched off.
static const Settings::Key RING_MENU_ON_SECONDARY_CLICK_KEY(moduleName, "pointertools/ringMenuOnSecondaryClick");

void PointerToolsConfiguration::init()
{
    settings()->setDefaultValue(RING_MENU_ON_SECONDARY_CLICK_KEY, Val(true));
    settings()->setCanBeManuallyEdited(RING_MENU_ON_SECONDARY_CLICK_KEY, true);
    settings()->setDescription(RING_MENU_ON_SECONDARY_CLICK_KEY, muse::trc("pointertools", "Ring menu on secondary click"));
    settings()->valueChanged(RING_MENU_ON_SECONDARY_CLICK_KEY).onReceive(this, [this](const Val& val) {
        m_ringMenuOnSecondaryClickChanged.send(val.toBool());
    });
}

bool PointerToolsConfiguration::ringMenuOnSecondaryClick() const
{
    return settings()->value(RING_MENU_ON_SECONDARY_CLICK_KEY).toBool();
}

void PointerToolsConfiguration::setRingMenuOnSecondaryClick(bool on)
{
    settings()->setSharedValue(RING_MENU_ON_SECONDARY_CLICK_KEY, Val(on));
}

async::Channel<bool> PointerToolsConfiguration::ringMenuOnSecondaryClickChanged() const
{
    return m_ringMenuOnSecondaryClickChanged;
}
