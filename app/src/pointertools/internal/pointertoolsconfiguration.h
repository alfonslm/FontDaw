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

#include "global/async/asyncable.h"

#include "../ipointertoolsconfiguration.h"

namespace mu::pointertools {
class PointerToolsConfiguration : public IPointerToolsConfiguration, public muse::async::Asyncable
{
public:
    void init();

    bool ringMenuOnSecondaryClick() const override;
    void setRingMenuOnSecondaryClick(bool on) override;
    muse::async::Channel<bool> ringMenuOnSecondaryClickChanged() const override;

private:
    muse::async::Channel<bool> m_ringMenuOnSecondaryClickChanged;
};
}
