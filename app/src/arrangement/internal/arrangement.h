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

#include "../iarrangement.h"

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "../iaudioassetstore.h"

namespace mu::arrangement {
class Arrangement : public IArrangement, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<IAudioAssetStore> assetStore = { this };

public:
    //! Splitting a clip gives both new edges this fade, so the cut does not click.
    static constexpr double SPLIT_FADE_SECS = 0.005;

    Arrangement(const muse::modularity::ContextPtr& ctx);

    const TrackList& tracks() const override;
    const Track* track(ArrTrackId id) const override;

    muse::RetVal<ArrTrackId> addAudioTrack(const muse::String& name, int atIndex = -1, TimeBase timeBase = TimeBase::Musical) override;
    muse::Ret removeTrack(ArrTrackId id) override;
    muse::Ret renameTrack(ArrTrackId id, const muse::String& name) override;
    muse::Ret setTrackPart(ArrTrackId id, const muse::ID& partId) override;
    muse::Ret moveTrack(ArrTrackId id, int toIndex) override;
    muse::Ret setRecordArmed(ArrTrackId id, bool armed) override;
    muse::Ret setInputMonitoring(ArrTrackId id, bool on) override;

    muse::RetVal<ClipId> addClip(ArrTrackId trackId, const AudioClip& clip) override;
    muse::Ret removeClip(ArrTrackId trackId, ClipId clipId) override;
    muse::Ret moveClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack, const mu::engraving::Fraction& newStartTick) override;
    muse::Ret trimClip(ArrTrackId trackId, ClipId clipId, secs_t newSourceOffset, secs_t newLength) override;
    muse::RetVal<ClipId> splitClip(ArrTrackId trackId, ClipId clipId, const mu::engraving::Fraction& atTick) override;
    muse::Ret setClipGain(ArrTrackId trackId, ClipId clipId, float gainDb) override;
    muse::Ret setClipFades(ArrTrackId trackId, ClipId clipId, const Fade& fadeIn, const Fade& fadeOut) override;
    muse::Ret setClipMuted(ArrTrackId trackId, ClipId clipId, bool muted) override;
    muse::RetVal<ClipId> duplicateClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack,
                                       const mu::engraving::Fraction& atTick) override;

    muse::Ret setClipStartTicks(const std::vector<ClipStartTick>& starts) override;

    void restoreTracks(const TrackList& tracks) override;
    uint64_t epoch() const override;

    void setTimeMap(const TimeMap& timeMap) override;
    void onTempoMapChanged() override;

    muse::Ret read(const ReadProjectEntry& readEntry) override;
    muse::Ret write(const WriteProjectEntry& writeEntry, const WriteOptions& options = {}) const override;
    void clear() override;

    void setOwner(const void* owner) override;
    const void* owner() const override;

    muse::async::Channel<Change> changed() const override;

private:
    // Every edit goes through these, so the invariants live in one place.
    Track* mutableTrack(ArrTrackId id);
    AudioClip* mutableClip(Track& track, ClipId clipId);

    //! Always: length > 0, sourceOffset >= 0, fades fit, gain is a number. With `placement`
    //! (edits that add, move or resize a clip) also: known asset, window inside it, start >= 0.
    //! Text fit for the user.
    muse::Ret validateClip(const AudioClip& clip, bool placement) const;
    bool wouldOverlap(const Track& track, const AudioClip& candidate, ClipId ignore) const;
    //! Fills the derived half of the clip's position from the authoritative one.
    void derivePosition(TimeBase timeBase, AudioClip& clip) const;
    //! Places the clip so it starts at `secs` (Absolute) or at the tick nearest to it (Musical).
    void setStartSecs(TimeBase timeBase, AudioClip& clip, secs_t secs) const;
    static void sortClips(Track& track);
    //! Validates the edited copy against the track (ignoring the clip it replaces) and swaps it in.
    //! With `placement`, also checks overlap.
    muse::Ret replaceClip(Track& track, const AudioClip& edited, bool placement);

    void notify(ChangeKind kind, ArrTrackId trackId = INVALID_TRACK);

    TrackList m_tracks;
    ArrTrackId m_lastTrackId = INVALID_TRACK;
    ClipId m_lastClipId = INVALID_CLIP;
    TimeMap m_timeMap;
    const void* m_owner = nullptr;
    uint64_t m_epoch = 0; // Batch 2 03a
    muse::async::Channel<Change> m_changed;
};
}
