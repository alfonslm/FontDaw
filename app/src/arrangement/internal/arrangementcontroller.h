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

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "../iarrangementcontroller.h"

#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "audio/main/iplayback.h"
#include "project/iprojectsession.h"
#include "interactive/iinteractive.h"
#include "playback/iplaybackcontroller.h"
#include "project/iprojectaudiosettings.h"
#include "notation/inotationsolomutestate.h"

#include "../iarrangement.h"
#include "../iarrangementundo.h"
#include "../iarrangementconfiguration.h"
#include "../iaudioassetstore.h"

namespace mu::engraving {
class Score;
}

namespace mu::arrangement {
class AudioFileDevice;

class ArrangementController : public IArrangementController, public muse::Contextable, public muse::async::Asyncable
{
    muse::GlobalInject<IArrangementConfiguration> configuration;
    muse::ContextInject<IArrangement> arrangement = { this };
    muse::ContextInject<IArrangementUndo> arrangementUndo = { this };
    muse::ContextInject<IAudioAssetStore> assetStore = { this };
    muse::ContextInject<muse::audio::IPlayback> playback = { this };
    muse::ContextInject<playback::IPlaybackController> playbackController = { this };
    muse::ContextInject<project::IProjectSession> projectSession = { this };
    muse::ContextInject<muse::IInteractive> interactive = { this };

public:
    ArrangementController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();
    void deinit();

    std::vector<AudioTrack> audioTracks() const override;
    muse::async::Channel<muse::audio::TrackId> trackAdded() const override;
    muse::async::Channel<muse::audio::TrackId> trackRemoved() const override;

    TrackPlayState trackPlayState(ArrTrackId id) const override;

    bool isLoaded() const override;
    muse::async::Notification isLoadedChanged() const override;

    muse::audio::secs_t contentEnd() const override;
    muse::async::Notification contentEndChanged() const override;

    bool hasSoloTrack(const notation::INotationPtr& notation) const override;
    void updateSoloMuteStates(const notation::INotationPtr& notation, bool hasSolo, bool muteAll = false) override;

    muse::async::Promise<muse::Ret> importAudioTrack(const muse::io::path_t& filePath) override;

    muse::Ret prepareProjectCopy() override;

    //! Projects from before S1 get their audio staves when they open: in the app only, never in a
    //! command-line conversion (ArrangementContext sets it for the GUI app).
    void setAddAudioStavesOnOpen(bool on) { m_addAudioStavesOnOpen = on; }
    bool embedLinkedAudioInCopy() const override;

    //! What the engine should play for a track, from the model alone (no engine, no IO):
    //! every clip whose audio is available, muted ones too (so muting is a list update), keyed by
    //! asset id. Playing when at least one clip sounds; Offline when clips would sound but none of
    //! their audio is available; Empty when no clip sounds.
    struct Plan {
        TrackPlayState state = TrackPlayState::Empty;
        std::map<AssetId, muse::io::path_t> assets; // Playing: the files the clips use
        muse::audio::AudioClipList clips;           // Playing: what the engine plays
    };
    static Plan planTrack(const Track& track, const IAudioAssetStore* store);

private:
    struct EngineTrack {
        muse::audio::TrackId id = -1; // -1 while the engine is still adding it
        std::vector<AssetId> assets;  // the engine decodes its sources when the track is added: another set adds it again
        muse::String name;            // the engine keeps the name it was added with
        uint64_t token = 0;           // tells a late answer for a track that was replaced meanwhile
        muse::audio::AudioClipList clips;                            // what the engine was last given
        muse::async::Channel<muse::audio::AudioClipList> clipsChanged; // new lists for the engine (AudioClipTrackData)
    };

    void onArrangementChanged(const Change& change);
    void onProjectChanged();

    //! Appends the audio parts (audio staves) of these tracks in one undo step of the score's
    //! history and stores their ids in the tracks. False when nothing was added.
    bool addAudioParts(const std::vector<ArrTrackId>& ids, const muse::TranslatableString& actionName);
    //! One part, inside an open edit. False when the score has no measures.
    bool addAudioPartFor(engraving::Score* score, ArrTrackId id);
    void addMissingAudioParts();

    //! The key a track's mixer settings and solo/mute state are kept under (Batch 2 S1).
    static engraving::InstrumentTrackId trackKey(const Track& track);
    engraving::InstrumentTrackId keyOf(ArrTrackId id) const;
    //! Brings m_keys up to date; a key that changed (the track got its part) takes its saved mixer
    //! settings and solo/mute state with it, and the mixer is told to rebuild that strip.
    void refreshKeys();
    void moveTrackSettings(const engraving::InstrumentTrackId& from, const engraving::InstrumentTrackId& to);
    void bindTimeMap();

    //! Makes the engine tracks match the model.
    void syncEngineTracks();
    void addEngineTrack(const Track& track, const Plan& plan);
    void removeEngineTrack(ArrTrackId id);
    void forgetEngineTracks();

    ArrTrackId arrangementTrackOf(muse::audio::TrackId engineTrackId) const;
    project::IProjectAudioSettingsPtr audioSettings() const;
    void storeOutputParams(muse::audio::TrackId engineTrackId, const std::function<void(project::AudioOutputParams&)>& change);

    void updateContentEnd();
    void updateIsLoaded();

    bool m_active = false;
    bool m_addAudioStavesOnOpen = false;

    std::map<ArrTrackId, EngineTrack> m_engineTracks;
    std::map<ArrTrackId, TrackPlayState> m_playStates;
    std::map<ArrTrackId, engraving::InstrumentTrackId> m_keys; // refreshKeys()

    //! Batch 2 03a: the mixer settings of tracks that an undoable edit removed, kept for the
    //! session so an undo can put them back (03a-undo-layer.md, Fable point 2). Not saved.
    struct ParkedSettings {
        project::AudioOutputParams output;
        std::vector<std::pair<std::weak_ptr<notation::INotationSoloMuteState>, notation::INotationSoloMuteState::SoloMuteState> > soloMute;
    };
    void parkSettings(ArrTrackId id);
    void unparkSettings(ArrTrackId id);
    std::map<ArrTrackId, ParkedSettings> m_parked;
    uint64_t m_parkedEpoch = 0;
    std::set<ArrTrackId> m_knownTracks;
    //! Tracks the engine refused, or whose files could not be opened, with the files they had: not added
    //! again (each try reads and decodes every file while the whole mix is silent) until those change or
    //! the sequence is set up again
    std::map<ArrTrackId, std::vector<AssetId> > m_refused;
    uint64_t m_lastToken = 0;
    //! One device per asset of the track, keyed by token; released when the engine answers, never before (IPlayback::addTrack)
    std::map<uint64_t, std::vector<std::shared_ptr<AudioFileDevice> > > m_devicesInFlight;

    muse::async::Channel<muse::audio::TrackId> m_trackAdded;
    muse::async::Channel<muse::audio::TrackId> m_trackRemoved;

    bool m_isLoaded = true;
    muse::async::Notification m_isLoadedChanged;

    muse::audio::secs_t m_contentEnd = 0.0;
    muse::async::Notification m_contentEndChanged;

    bool m_embedLinkedAudioInCopy = false;

    //! Kept so the time map never calls into a score that is gone.
    std::weak_ptr<notation::IMasterNotation> m_timeMapNotation;
};
}
