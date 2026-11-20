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

#include "modularity/imoduleinterface.h"
#include "async/channel.h"
#include "async/notification.h"
#include "async/promise.h"
#include "audio/common/audiotypes.h"
#include "engraving/types/types.h"
#include "notation/inotation.h"

#include "arrangementtypes.h"

namespace mu::arrangement {
//! Where an audio track's mixer settings and solo/mute state are kept: under the same kind of key
//! as an instrument's (project audio settings, notation solo/mute state), so the mixer, the
//! solo logic and audiosettings.json treat audio tracks like every other track.
//! The part id is one no score part has; the instrument id carries the arrangement track id.
inline constexpr uint64_t AUDIO_TRACK_PART_ID = 998;

//! The key of a track that has no audio part (made before S1, or in tests without a score). A
//! track with a part is keyed by that part's own track id ({ part id, "fontdaw-audio-track" }):
//! see ArrangementController::trackKey. Saved mixer settings move to the new key.
inline engraving::InstrumentTrackId audioTrackKey(ArrTrackId id)
{
    return { muse::ID(AUDIO_TRACK_PART_ID), u"fontdaw-audio-track-" + muse::String::number(static_cast<size_t>(id)) };
}

//! What the engine does with an audio track right now.
enum class TrackPlayState {
    Playing,         // it has an engine track
    Empty,           // no clip that sounds
    Offline,         // clips would sound, but none of their audio is available (missing, or its path refused, FD-031)
};

//! Keeps the audio engine in step with the model:
//!
//! Model (IArrangement) --changed()--> controller --addTrack/removeTrack--> engine (IPlayback)
//!
//! It owns the engine tracks of audio tracks, the same way playback::PlaybackController owns
//! those of instruments, and it does not own the transport: play/stop/seek/loop stay in
//! PlaybackController, so there is one playhead for score and audio.
//!
//! Until the engine takes clip lists (F1 step 2) a track plays when it has exactly one clip
//! that sounds, placed at time zero and covering its whole file. Clip gain and fades are not
//! applied yet. Any other track stays in the model, silent (trackPlayState says why).
class IArrangementController : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IArrangementController)

public:
    virtual ~IArrangementController() = default;

    struct AudioTrack {
        ArrTrackId id = INVALID_TRACK;
        engraving::InstrumentTrackId key;       // the part's track id, or audioTrackKey(id) without a part
        muse::audio::TrackId engineTrackId = -1;
    };

    //! The audio tracks that have an engine track, in arrangement order. For the mixer.
    virtual std::vector<AudioTrack> audioTracks() const = 0;
    virtual muse::async::Channel<muse::audio::TrackId> trackAdded() const = 0;
    virtual muse::async::Channel<muse::audio::TrackId> trackRemoved() const = 0;

    virtual TrackPlayState trackPlayState(ArrTrackId id) const = 0;

    //! True when no audio track is still on its way into the engine. Export waits for this.
    virtual bool isLoaded() const = 0;
    virtual muse::async::Notification isLoadedChanged() const = 0;

    //! End of the last clip, in seconds. PlaybackController uses max(score duration, this) as
    //! the duration of playback and export, or audio after the last bar would be cut.
    virtual muse::audio::secs_t contentEnd() const = 0;
    virtual muse::async::Notification contentEndChanged() const = 0;

    //! Solo and mute are one mechanism for instruments and audio tracks (a solo on either kind
    //! silences the others of both kinds). PlaybackController decides; these are its two hooks.
    //! `muteAll`: every audio track is muted, e.g. while a part is exported (audio tracks belong to no part).
    virtual bool hasSoloTrack(const notation::INotationPtr& notation) const = 0;
    virtual void updateSoloMuteStates(const notation::INotationPtr& notation, bool hasSolo, bool muteAll = false) = 0;

    //! "Import audio track": imports the file (IAudioAssetStore), adds a track named after it
    //! and places the whole file on it at the start of the score.
    virtual muse::async::Promise<muse::Ret> importAudioTrack(const muse::io::path_t& filePath) = 0;

    //! Export / share (README decision 6). Call before saving a copy of the project: when the
    //! project links to audio files and the preference says so, asks "embed them in the copy?".
    //! Returns Cancel when the user cancels. The answer is used by the next copy written.
    virtual muse::Ret prepareProjectCopy() = 0;
    virtual bool embedLinkedAudioInCopy() const = 0;
};
}
