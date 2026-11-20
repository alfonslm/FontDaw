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

#include <vector>

#include "../arrangementtypes.h"

namespace mu::arrangement {
class IAudioAssetStore;

//! What one clip looks like, from the model alone (moved out of the arrangement panel's model,
//! computeClips()).
struct ClipView {
    ClipId id = 0;
    muse::String label;      // the clip's name, or the file name when it has none
    AssetId asset;
    double startSecs = 0.0;  // on the timeline
    double lengthSecs = 0.0;
    double sourceOffsetSecs = 0.0;
    bool muted = false;
    bool missing = false;    // no audio: the file is missing, or its path was refused (FD-031)
};

//! The clips of track as they are drawn, in start order. Times are the model's own (startSecs is
//! kept up to date by the arrangement's time map); no rounding here.
std::vector<ClipView> clipViews(const Track& track, const IAudioAssetStore* store);
}
