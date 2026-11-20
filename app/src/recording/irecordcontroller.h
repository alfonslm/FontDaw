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

#include "modularity/imoduleinterface.h"
#include "types/ret.h"
#include "async/channel.h"
#include "async/notification.h"
#include "midi/midievent.h"

#include "arrangement/arrangementtypes.h"
#include "recordingtypes.h"

namespace mu::recording {
//! Starts and stops recording. Does not own transport: it asks playback::IPlaybackController to
//! play and stop, so score, audio tracks and recording share one playhead.
//!
//! Wired to the "record" and "record-arm" commands (see the TODO in
//! src/playback/internal/playbackcommandsregister.cpp).
class IRecordController : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IRecordController)

public:
    virtual ~IRecordController() = default;

    // --- Inputs ---
    virtual AudioInputList availableInputs() const = 0;
    virtual muse::async::Notification availableInputsChanged() const = 0;

    //! Live level of the input assigned to a track. Only meaningful while armed or monitoring.
    virtual muse::async::Channel<arrangement::ArrTrackId, InputLevel> inputLevelChanged() const = 0;

    // --- Recording ---
    //! Refuses, with a message for the user, when: no track is armed, no input device, the
    //! project is read-only, or the disk has less than a safety margin of free space.
    virtual muse::Ret startRecording() = 0;
    //! Stops transport, finishes the file, adds the clip. Safe to call in any state.
    virtual void stopRecording() = 0;

    virtual RecordState state() const = 0;
    virtual muse::async::Channel<RecordState> stateChanged() const = 0;
    virtual muse::async::Channel<TakeResult> takeFinished() const = 0;

    // --- MIDI performance capture (FEATURES §5) ---
    //! Called from notation's MIDI input while recording (see the TODO in
    //! src/notation/internal/notationmidiinput.cpp). Timestamp = transport position when the
    //! event arrived, already corrected for MIDI input latency if the driver reports one.
    virtual void onMidiEvent(const muse::midi::Event& event, muse::audio::secs_t transportPos) = 0;
};
}
