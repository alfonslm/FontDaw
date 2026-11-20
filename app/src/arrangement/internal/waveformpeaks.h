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

#include <cstdint>
#include <vector>

#include "global/types/retval.h"

namespace muse::audio::codec {
class IAudioDecoder;
}

namespace mu::arrangement {
//! Min/max overview of an asset for drawing waveforms at any zoom without touching the audio.
//!
//! Level 0 holds one (min, max) pair per FRAMES_PER_PEAK frames per channel; each further level
//! halves the resolution, down to a level with a single peak. Drawing picks the level whose
//! frames-per-peak is the largest one not above frames-per-pixel and merges neighbours, so cost
//! is O(pixels), independent of file length.
struct WaveformPeaks {
    static constexpr int FRAMES_PER_PEAK = 256;

    struct MinMax {
        int8_t min = 0; // 8 bits is enough for display and keeps a 1-hour stereo file near 1.3 MB
        int8_t max = 0;

        bool operator==(const MinMax& o) const { return min == o.min && max == o.max; }
    };

    //! Peak magnitude per frequency band, for the RGB waveform colour profile
    //! (docs/fontdaw/design/audio-in-score/06-waveform-colouring.md): low below LOW_MID_HZ,
    //! mid between, high above MID_HIGH_HZ. 0..255 maps to 0..full scale.
    struct Bands {
        uint8_t low = 0;
        uint8_t mid = 0;
        uint8_t high = 0;

        bool operator==(const Bands& o) const { return low == o.low && mid == o.mid && high == o.high; }
    };

    static constexpr double LOW_MID_HZ = 600.0;
    static constexpr double MID_HIGH_HZ = 4000.0;

    struct Level {
        int framesPerPeak = FRAMES_PER_PEAK;
        std::vector<std::vector<MinMax> > channels; // [channel][peak]
        std::vector<std::vector<Bands> > bands;     // [channel][peak], same shape as channels
    };

    uint64_t sampleRate = 0;
    std::vector<double> bandCrossoversHz; // { LOW_MID_HZ, MID_HIGH_HZ } when bands were computed
    uint64_t frames = 0;
    std::vector<Level> levels;

    //! Decodes the whole file once, through codec::readAll (the engine's read loop, so `frames` is
    //! what plays), from the decoder's start. Fails as readAll() does: damaged, too long, no audio.
    //! Runs on a worker thread: no shared state.
    static muse::RetVal<WaveformPeaks> build(muse::audio::codec::IAudioDecoder& decoder);

    const Level* levelFor(double framesPerPixel) const;

    // TODO(FontDaw): on-disk cache next to the session copy (<id>.peaks) so reopening a project
    // with long recordings is instant: small header (magic, version, sampleRate, frames,
    // channels, level count) followed by raw levels; rebuild when the version does not match.
};
}
