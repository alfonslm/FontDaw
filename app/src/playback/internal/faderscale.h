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

#include <algorithm>

namespace mu::playback {
//! FontDaw (FD-191): the mixer fader's scale, shared by the fader and the level meter beside it,
//! so a meter at 0 dBFS sits level with the fader's 0 dB mark.
//! The fader runs from -60 dB (shown as -inf) to +12 dB; its curve gives -60..-12 dB the lower half
//! of the travel and -12..+12 dB the upper half, so 0 dB sits three quarters of the way up.
//! VolumeSlider.qml draws the same curve (its "convertor"); keep the two in step.
struct FaderScale {
    static constexpr double MIN_DB = -60.0;
    static constexpr double MAX_DB = 12.0;

    //! Position on the fader's travel for a level in dB: 0 at the bottom, 1 at the top
    static constexpr double positionFromDb(double db)
    {
        constexpr double LOGICAL_CENTER_DB = -12.0;
        constexpr double LOCAL_CENTER_DB = -24.0;
        constexpr double HIGH_ACCURACY_STEP = 1.5;
        constexpr double LOW_ACCURACY_STEP = 0.75;

        const double clamped = std::clamp(db, MIN_DB, MAX_DB);
        const double local = clamped > LOGICAL_CENTER_DB
                             ? MAX_DB - (MAX_DB - clamped) * HIGH_ACCURACY_STEP
                             : LOCAL_CENTER_DB - (LOGICAL_CENTER_DB - clamped) * LOW_ACCURACY_STEP;

        return (local - MIN_DB) / (MAX_DB - MIN_DB);
    }
};
}
