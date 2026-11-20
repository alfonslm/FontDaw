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

#include <atomic>
#include <cstdint>

#include "io/path.h"
#include "types/ret.h"

namespace mu::recording {
//! Receives input audio on the audio thread and writes a WAV file on its own thread.
//! See README, "Rules that keep recordings safe" - this class is where they are enforced.
class TakeWriter
{
public:
    //! Creates the file and writes a valid WAV header (32-bit float), then starts the writer
    //! thread. bufferSeconds sizes the ring buffer (2 s is a sane default).
    muse::Ret open(const muse::io::path_t& path, int sampleRate, int channels, double bufferSeconds = 2.0);

    //! AUDIO THREAD. Wait-free: copy into the ring buffer or, if full, count dropped frames.
    void push(const float* interleaved, uint32_t frames) noexcept;

    //! Drains the buffer, fixes the header sizes, joins the thread. Returns frames written.
    uint64_t close();

    uint64_t droppedFrames() const noexcept { return m_dropped.load(std::memory_order_relaxed); }
    bool clipped() const noexcept { return m_clipped.load(std::memory_order_relaxed); }

    // TODO(FontDaw FD-010): implementation.
    //  - Single-producer/single-consumer ring buffer of float, sized once in open().
    //  - Writer thread wakes on a semaphore
    //    (muse/framework/audio/common/lightweightsemaphore.h) or every ~50 ms.
    //  - Flush the header sizes every few seconds so a crash leaves a playable file.
    //  - Past ~4 GB of data: switch to RF64, or stop and report. Decide and test.

private:
    std::atomic<uint64_t> m_dropped { 0 };
    std::atomic<bool> m_clipped { false };
};
}
