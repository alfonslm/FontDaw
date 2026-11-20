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

// DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types/string.h"
#include "audio/common/audiotypes.h"

namespace mu::recording {
enum class RecordState {
    Idle,
    CountIn,   // transport runs the count-in, nothing is kept yet
    Recording,
    Finishing, // stop pressed; file being closed and imported
};

//! One selectable input on the current audio device.
//! TODO(FontDaw FD-010): comes from the driver once FRAMEWORK_CHANGES F2 exists.
struct AudioInput {
    std::string deviceId;
    muse::String name; // e.g. "Input 1", "Inputs 1-2"
    int firstChannel = 0;
    int channelCount = 1; // 1 = mono, 2 = stereo
};

using AudioInputList = std::vector<AudioInput>;

//! Peak level of a live input, for the meter on an armed track.
struct InputLevel {
    float peakDb = -144.f;
    bool clipped = false; // any sample at or above full scale since last reset
};

//! What the user is told after a take.
struct TakeResult {
    muse::audio::secs_t duration = 0.0;
    uint64_t droppedFrames = 0; // > 0 means the disk could not keep up
    bool clipped = false;
};
}
