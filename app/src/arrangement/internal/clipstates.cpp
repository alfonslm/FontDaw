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

#include "clipstates.h"

#include "../iaudioassetstore.h"

namespace mu::arrangement {
std::vector<ClipView> clipViews(const Track& track, const IAudioAssetStore* store)
{
    std::vector<ClipView> result;
    result.reserve(track.clips.size());

    for (const AudioClip& clip : track.clips) {
        const AudioAsset* asset = store ? store->asset(clip.asset) : nullptr;

        ClipView v;
        v.id = clip.id;
        // A clip without a name of its own shows its audio file's name (owner, review of #160)
        v.label = !clip.name.isEmpty() ? clip.name : (asset ? asset->name : muse::String());
        v.asset = clip.asset;
        v.startSecs = clip.startSecs.raw();
        v.lengthSecs = clip.timelineLength().raw();
        v.sourceOffsetSecs = clip.sourceOffset.raw();
        v.muted = clip.muted;
        v.missing = !asset || asset->offline; // blocked implies offline
        result.push_back(std::move(v));
    }
    return result;
}
}
