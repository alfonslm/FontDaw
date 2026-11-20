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

#include "modularity/imoduleinterface.h"
#include "async/channel.h"

#include "arrangementtypes.h"

namespace mu::arrangement {
//! The arrangement of the currently open project: the model the panel shows and edits.
//! Main thread only. One instance per project context.
//!
//! Every mutating call:
//!   - validates its input and returns a Ret (never asserts on user actions),
//!   - changes nothing when it fails,
//!   - keeps every track's clips sorted by start and non-overlapping,
//!   - sends exactly one Change on changed() when it succeeds.
//! Undo (Batch 2 03a): an edit is undoable when it is made through IArrangementUndo::edit(), which
//! records it in the score's undo history. A call made directly is not recorded.
class IArrangement : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IArrangement)

public:
    virtual ~IArrangement() = default;

    // --- Tracks ---
    virtual const TrackList& tracks() const = 0;
    virtual const Track* track(ArrTrackId id) const = 0;

    virtual muse::RetVal<ArrTrackId> addAudioTrack(const muse::String& name, int atIndex = -1, TimeBase timeBase = TimeBase::Musical) = 0;
    virtual muse::Ret removeTrack(ArrTrackId id) = 0;
    virtual muse::Ret renameTrack(ArrTrackId id, const muse::String& name) = 0;
    //! The score part that shows the track (Track::partId). Not an undoable edit of its own: it
    //! follows the part, which the score's undo adds and removes.
    virtual muse::Ret setTrackPart(ArrTrackId id, const muse::ID& partId) = 0;
    virtual muse::Ret moveTrack(ArrTrackId id, int toIndex) = 0;
    virtual muse::Ret setRecordArmed(ArrTrackId id, bool armed) = 0;
    virtual muse::Ret setInputMonitoring(ArrTrackId id, bool on) = 0;

    // --- Clips (FEATURES §4) ---
    //! Places a window of an asset on a track. The clip's position is taken from startTick on
    //! a Musical track and from startSecs on an Absolute one; its id is assigned here.
    //! Fails if the asset is unknown, the window is outside the asset, or it would overlap.
    virtual muse::RetVal<ClipId> addClip(ArrTrackId trackId, const AudioClip& clip) = 0;
    virtual muse::Ret removeClip(ArrTrackId trackId, ClipId clipId) = 0;
    //! Move in time and/or to another audio track. Snapping is the caller's job.
    virtual muse::Ret moveClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack, const mu::engraving::Fraction& newStartTick) = 0;
    //! Change where the clip starts/ends without moving the audio underneath it: changing the
    //! source offset by d moves the clip's start on the timeline by d as well.
    virtual muse::Ret trimClip(ArrTrackId trackId, ClipId clipId, secs_t newSourceOffset, secs_t newLength) = 0;
    //! Cut one clip into two at a timeline position strictly inside it. The left part keeps its
    //! id; returns the id of the right-hand part.
    virtual muse::RetVal<ClipId> splitClip(ArrTrackId trackId, ClipId clipId, const mu::engraving::Fraction& atTick) = 0;
    virtual muse::Ret setClipGain(ArrTrackId trackId, ClipId clipId, float gainDb) = 0;
    virtual muse::Ret setClipFades(ArrTrackId trackId, ClipId clipId, const Fade& fadeIn, const Fade& fadeOut) = 0;
    virtual muse::Ret setClipMuted(ArrTrackId trackId, ClipId clipId, bool muted) = 0;
    //! Batch 2 03a: a copy of the clip with a new id, on the same or another track, starting at
    //! `atTick` (Musical) or at the seconds that tick has (Absolute). Fails like addClip on overlap.
    virtual muse::RetVal<ClipId> duplicateClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack,
                                               const mu::engraving::Fraction& atTick) = 0;

    //! FD-151 (clips follow their bars): sets the start tick of clips on Musical tracks and
    //! re-derives their seconds. No overlap check, as with a tempo change: bars moving under clips
    //! can make them meet, and playback trims the earlier one. Fails, changing nothing, if a clip is
    //! unknown or on an Absolute track. Sends one ClipsChanged per track changed.
    virtual muse::Ret setClipStartTicks(const std::vector<ClipStartTick>& starts) = 0;

    // --- Undo (Batch 2 03a) ---
    //! For undo and redo only: puts back an earlier list of tracks, with their ids, and
    //! re-derives every clip's position from the current time map. Sends one Restored change.
    //! Only the history fields come from `tracks` (see sameHistory): a track that is live keeps
    //! its live fields (arming, monitoring, input, part link); a track coming back takes them
    //! from `tracks`. The id counters never go back, so later tracks and clips never reuse an id.
    virtual void restoreTracks(const TrackList& tracks) = 0;

    //! Goes up with every read() and clear(): an undo command made in another epoch (another
    //! project in the same window) does nothing.
    virtual uint64_t epoch() const = 0;

    // --- Time ---
    //! The score's tempo map (C2 binds it). Recomputes every clip's derived position.
    virtual void setTimeMap(const TimeMap& timeMap) = 0;
    //! Called when the score's tempo map changed: recompute the derived half of every clip
    //! position (startSecs for Musical tracks, startTick for Absolute ones). Clips on a Musical
    //! track that now overlap in seconds are kept; playback trims the earlier one (C2).
    virtual void onTempoMapChanged() = 0;

    // --- Persistence (README, "Project file") ---
    //! Reads arrangement.json (tracks, clips and the asset table) and loads the assets.
    //! A missing arrangement.json is not an error: it means an empty arrangement.
    virtual muse::Ret read(const ReadProjectEntry& readEntry) = 0;
    //! Writes nothing at all when there are no tracks: the file stays a plain MuseScore file.
    virtual muse::Ret write(const WriteProjectEntry& writeEntry, const WriteOptions& options = {}) const = 0;
    virtual void clear() = 0;

    //! The project object this arrangement belongs to (an opaque token; null: none).
    //! A window has one arrangement, but for a moment it can hold two project objects (one
    //! closing while the next loads; the converter loads one after another): a project only
    //! saves and clears the arrangement it loaded or created.
    virtual void setOwner(const void* owner) = 0;
    virtual const void* owner() const = 0;

    // --- Notifications ---
    virtual muse::async::Channel<Change> changed() const = 0;
};
}
