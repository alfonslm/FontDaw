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

#include "../irecordcontroller.h"

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "arrangement/iarrangement.h"
#include "arrangement/iaudioassetstore.h"

namespace mu::recording {
class RecordController : public IRecordController, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<arrangement::IArrangement> arrangement = { this };
    muse::ContextInject<arrangement::IAudioAssetStore> assetStore = { this };
    // TODO(FontDaw): inject playback::IPlaybackController (play/stop/position, count-in state)
    // and the framework's audio driver controller (muse/framework/audio/iaudiodrivercontroller.h)
    // for inputs and latency once FRAMEWORK_CHANGES F2/F3 exist.

public:
    RecordController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();

    // TODO(FontDaw): declare every IRecordController override here.

private:
    RecordState m_state = RecordState::Idle;
    muse::audio::secs_t m_recordStartPos = 0.0; // transport position when the first kept sample arrived
};
}
