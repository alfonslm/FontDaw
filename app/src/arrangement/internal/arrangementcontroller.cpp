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

#include "arrangementcontroller.h"

#include <algorithm>

#include "global/translation.h"
#include "notation/iexcerptnotation.h"
#include "notation/imasternotation.h"
#include "notation/inotationplayback.h"
#include "notation/inotationundostack.h"
#include "playback/playbacktypes.h"
#include "notation/inotationsolomutestate.h"
#include "project/inotationproject.h"

#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"

#include "audiopart.h"
#include "mappedfile.h"

#include "log.h"

using namespace mu::arrangement;
using namespace mu::project;
using namespace mu::notation;
using namespace muse;
using namespace muse::audio;
using mu::engraving::Fraction;
using mu::engraving::InstrumentTrackId;

// How an audio track reaches the engine (FD-015 question 3)
// ---------------------------------------------------------
// Each audio track is one engine track playing a clip list (IPlayback::addTrack with
// AudioClipTrackData; framework F1 step 2). Its sources are the track's assets, one io::IODevice each,
// sent as pointers over the engine's RPC channel. That works because in every work mode the framework
// builds today the engine runs in this process (its own thread); the web build rejects the call. The
// engine reads the devices while it adds the track and does not use them afterwards, so they only have
// to live until addTrack has answered: m_devicesInFlight holds them.
//
// Later edits send a whole new clip list on the track's stream (EngineTrack::clipsChanged): no answer,
// no device. A list can only use the sources the track was added with, so a track whose set of assets
// changes is removed and added again.
//
// The engine decodes each whole file into memory. Session copies of embedded audio are mapped, not
// read, so this process holds the audio once (decoded), not twice.

void ArrangementController::init()
{
    // Run modes without playback (e.g. the audio plugin scanner) have nothing to keep in step
    if (!playback() || !playbackController() || !arrangement() || !assetStore()) {
        return;
    }
    m_active = true;

    arrangement()->changed().onReceive(this, [this](const Change& change) {
        onArrangementChanged(change);
    });

    //! PlaybackController resets the engine's sequence when the project or the audio system
    //! changes, and removes every track when it sets the sequence up again: ours come after.
    playbackController()->playbackInitedChanged().onReceive(this, [this](bool inited) {
        forgetEngineTracks();
        m_refused.clear(); // another audio system or project: worth a new try
        if (inited) {
            syncEngineTracks();
        }
        updateIsLoaded();
    });

    projectSession()->projectChanged().onNotify(this, [this]() {
        onProjectChanged();
    });

    // Mixer settings of audio tracks are saved where instruments keep theirs (audiosettings.json)
    playback()->controlParamsChanged().onReceive(this, [this](const TrackId trackId, const ControlParams& params) {
        storeOutputParams(trackId, [&params](AudioOutputParams& out) { out.setControl(params); });
    });

    playback()->fxChainParamsChanged().onReceive(this, [this](const TrackId trackId, const AudioFxChain& params) {
        storeOutputParams(trackId, [&params](AudioOutputParams& out) { out.fxChain = params; });
    });

    playback()->auxSendsParamsChanged().onReceive(this, [this](const TrackId trackId, const AuxSendsParams& params) {
        storeOutputParams(trackId, [&params](AudioOutputParams& out) { out.auxSends = params; });
    });

    onProjectChanged();
}

void ArrangementController::deinit()
{
    if (!m_active) {
        return;
    }
    m_active = false;

    forgetEngineTracks();
    // m_devicesInFlight is kept: devices of unanswered requests stay until the controller is destroyed
    async_disconnectAll();
}

// --- Model -> engine ---------------------------------------------------------------------------

namespace {
AudioFadeCurve engineFadeCurve(FadeCurve curve)
{
    switch (curve) {
    case FadeCurve::EqualPower: return AudioFadeCurve::EqualPower;
    case FadeCurve::Linear: break;
    }
    return AudioFadeCurve::Linear;
}
}

ArrangementController::Plan ArrangementController::planTrack(const Track& track, const IAudioAssetStore* store)
{
    Plan plan;

    if (track.kind != TrackKind::Audio) {
        return plan;
    }

    bool sounds = false;
    bool soundsWithAudio = false;

    for (const AudioClip& clip : track.clips) {
        sounds = sounds || !clip.muted;

        const AudioAsset* asset = store ? store->asset(clip.asset) : nullptr;
        if (!asset) {
            continue;
        }

        auto known = plan.assets.find(asset->id);
        if (known == plan.assets.end()) {
            const io::path_t path = store->assetFilePath(asset->id);
            if (path.empty()) {
                continue; // offline: its clips stay silent
            }
            plan.assets.emplace(asset->id, path);
        }

        soundsWithAudio = soundsWithAudio || !clip.muted;

        // Overlapping clips (after a tempo change) are sent as they are: the engine plays the later one
        muse::audio::AudioClip c;
        c.sourceKey = asset->id;
        c.startSecs = clip.startSecs;
        c.sourceOffsetSecs = clip.sourceOffset;
        c.lengthSecs = clip.length;
        c.gainDb = clip.gainDb;
        c.fadeIn = AudioFade { clip.fadeIn.length, engineFadeCurve(clip.fadeIn.curve) };
        c.fadeOut = AudioFade { clip.fadeOut.length, engineFadeCurve(clip.fadeOut.curve) };
        c.muted = clip.muted;
        plan.clips.push_back(std::move(c));
    }

    if (!sounds) {
        return Plan { TrackPlayState::Empty, {}, {} };
    }

    if (!soundsWithAudio) {
        return Plan { TrackPlayState::Offline, {}, {} };
    }

    plan.state = TrackPlayState::Playing;
    return plan;
}

namespace {
std::vector<AssetId> assetIds(const ArrangementController::Plan& plan)
{
    std::vector<AssetId> ids;
    for (const auto& [id, path] : plan.assets) {
        ids.push_back(id); // sorted: a std::map
    }
    return ids;
}
}

void ArrangementController::syncEngineTracks()
{
    if (!m_active) {
        return;
    }

    const TrackList& tracks = arrangement()->tracks();

    std::map<ArrTrackId, Plan> plans;
    for (const Track& track : tracks) {
        plans[track.id] = planTrack(track, assetStore().get());
    }

    m_playStates.clear();
    for (const auto& [id, plan] : plans) {
        m_playStates[id] = plan.state;
    }

    if (!playbackController()->isPlaybackInited()) {
        return;
    }

    // Engine tracks whose arrangement track is gone, cannot play any more, uses other files now
    // (the engine decodes a track's files only when it is added) or was renamed
    std::vector<ArrTrackId> toRemove;
    for (const auto& [id, engineTrack] : m_engineTracks) {
        const Track* track = arrangement()->track(id);
        auto plan = plans.find(id);
        if (!track || plan == plans.end() || plan->second.state != TrackPlayState::Playing
            || assetIds(plan->second) != engineTrack.assets || track->name != engineTrack.name) {
            toRemove.push_back(id);
        }
    }
    for (ArrTrackId id : toRemove) {
        removeEngineTrack(id);
    }

    for (auto it = m_refused.begin(); it != m_refused.end();) {
        it = arrangement()->track(it->first) ? std::next(it) : m_refused.erase(it);
    }

    for (const Track& track : tracks) {
        const Plan& plan = plans[track.id];
        if (plan.state != TrackPlayState::Playing) {
            continue;
        }

        auto it = m_engineTracks.find(track.id);
        if (it == m_engineTracks.end()) {
            auto refused = m_refused.find(track.id);
            if (refused != m_refused.end()) {
                if (refused->second == assetIds(plan)) {
                    m_playStates[track.id] = TrackPlayState::Offline; // same files as when it was refused
                    continue;
                }
                m_refused.erase(refused);
            }
            addEngineTrack(track, plan);
        } else if (it->second.clips != plan.clips) {
            // Moved, trimmed, muted, faded, or moved by the tempo map: a new list, also while the
            // engine is still adding the track (the stream exists from the request on)
            it->second.clips = plan.clips;
            it->second.clipsChanged.send(plan.clips);
        }
    }

    updateIsLoaded();
}

void ArrangementController::addEngineTrack(const Track& track, const Plan& plan)
{
    const ArrTrackId id = track.id;
    const InstrumentTrackId key = keyOf(id);

    AudioClipTrackData data;
    data.clips = plan.clips;

    std::vector<std::shared_ptr<AudioFileDevice> > devices;
    for (const auto& [assetId, path] : plan.assets) {
        const AudioAsset* asset = assetStore()->asset(assetId);
        const MappedFile::Mode mode = asset && asset->storage == AssetStorage::Embedded
                                      ? MappedFile::Mode::Map : MappedFile::Mode::Read;

        auto device = std::make_shared<AudioFileDevice>(path, mode);
        if (!device->open(io::IODevice::ReadOnly)) {
            LOGE() << "cannot open audio for track " << id << ": " << device->errorString();
            m_playStates[id] = TrackPlayState::Offline;
            m_refused[id] = assetIds(plan);
            return;
        }

        data.sources.push_back(AudioClipSource { assetId, reinterpret_cast<uint64_t>(device.get()) });
        devices.push_back(std::move(device));
    }

    AudioOutputParams out;
    const IProjectAudioSettingsPtr settings = audioSettings();
    if (settings && settings->trackHasExistingOutputParams(key)) {
        out = settings->trackOutputParams(key);
    }
    if (out.auxSends.empty()) {
        // A recording gets its aux sends (so the mixer shows them) but no reverb until asked for
        for (aux_channel_idx_t idx = 0; idx < playback::AUX_CHANNEL_NUM; ++idx) {
            out.auxSends.emplace_back(AuxSendParams { 0.f, true });
        }
    }

    TrackParams params;
    params.fxChain = out.fxChain;
    params.auxSends = out.auxSends;
    params.control = out.control();

    const uint64_t token = ++m_lastToken;
    EngineTrack& engineTrack = m_engineTracks[id];
    engineTrack.assets = assetIds(plan);
    engineTrack.name = track.name;
    engineTrack.token = token;
    engineTrack.clips = plan.clips;
    engineTrack.clipsChanged = muse::async::Channel<AudioClipList>();
    data.clipsChanged = engineTrack.clipsChanged;

    m_devicesInFlight[token] = std::move(devices);

    playback()->addTrack(track.name.toStdString(), data, params)
    .onResolve(this, [this, id, key, token, out](const TrackId engineTrackId, const TrackParams& appliedParams) {
        m_devicesInFlight.erase(token);

        auto it = m_engineTracks.find(id);
        if (it == m_engineTracks.end() || it->second.token != token) {
            // Removed or replaced while the engine was adding it
            playback()->removeTrack(engineTrackId);
            return;
        }

        it->second.id = engineTrackId;

        if (const IProjectAudioSettingsPtr settings = audioSettings()) {
            AudioOutputParams applied = out;
            applied.fxChain = appliedParams.fxChain;
            // Not a change by the user: what was loaded (or the defaults of a new import, which
            // marked the project modified already) is what the engine now has
            settings->setTrackOutputParams(key, applied, false /*notifySettingsChanged*/);
        }

        m_trackAdded.send(engineTrackId);
        updateIsLoaded();
    })
    .onReject(this, [this, id, token](int code, const std::string& msg) {
        m_devicesInFlight.erase(token);

        LOGE() << "the audio engine refused the audio of track " << id << ", code: [" << code << "] " << msg;

        auto it = m_engineTracks.find(id);
        if (it != m_engineTracks.end() && it->second.token == token) {
            m_refused[id] = it->second.assets;
            m_engineTracks.erase(it);
            m_playStates[id] = TrackPlayState::Offline;
        }
        updateIsLoaded();
    });
}

void ArrangementController::removeEngineTrack(ArrTrackId id)
{
    auto it = m_engineTracks.find(id);
    if (it == m_engineTracks.end()) {
        return;
    }

    const TrackId engineTrackId = it->second.id;
    m_engineTracks.erase(it); // a track still being added is removed when its answer arrives

    if (engineTrackId != -1) {
        playback()->removeTrack(engineTrackId);
        m_trackRemoved.send(engineTrackId);
    }
}

void ArrangementController::forgetEngineTracks()
{
    std::vector<ArrTrackId> ids;
    for (const auto& [id, engineTrack] : m_engineTracks) {
        ids.push_back(id);
    }
    for (ArrTrackId id : ids) {
        removeEngineTrack(id);
    }
}

void ArrangementController::onArrangementChanged(const Change& change)
{
    const INotationProjectPtr project = projectSession()->project();

    // Settings parked for another epoch (another project in this window) can never come back
    if (arrangement()->epoch() != m_parkedEpoch) {
        m_parked.clear();
        m_parkedEpoch = arrangement()->epoch();
    }

    // Batch 2 03a: a removed track's mixer settings are parked, not deleted, so an undo can put
    // them back; a save leaves them out. On Restored (undo, redo) the id sets are compared.
    std::set<ArrTrackId> current;
    for (const Track& track : arrangement()->tracks()) {
        current.insert(track.id);
    }
    std::vector<ArrTrackId> cameBack;
    if (change.kind == ChangeKind::TrackRemoved) {
        parkSettings(change.trackId);
    } else if (change.kind == ChangeKind::Restored) {
        for (ArrTrackId id : m_knownTracks) {
            if (!muse::contains(current, id)) {
                parkSettings(id);
            }
        }
        for (ArrTrackId id : current) {
            if (!muse::contains(m_knownTracks, id)) {
                cameBack.push_back(id);
            }
        }
    }
    m_knownTracks = std::move(current);

    refreshKeys();

    for (ArrTrackId id : cameBack) {
        unparkSettings(id);
    }

    //! "Modified" comes from the undo stack for recorded edits and for undo and redo (Restored).
    //! The project is marked unsaved only for a change the stack does not see: one made outside
    //! IArrangementUndo::edit() (arming, the part link, an edit while the stack is locked).
    //! TracksReset is a load, a clear or a new tempo map: not an edit.
    const bool recorded = arrangementUndo() && arrangementUndo()->isRecordingEdit();
    if (project && change.kind != ChangeKind::TracksReset && change.kind != ChangeKind::Restored && !recorded) {
        project->markAsUnsaved();
    }

    syncEngineTracks();
    updateContentEnd();
}

void ArrangementController::parkSettings(ArrTrackId id)
{
    const INotationProjectPtr project = projectSession()->project();
    const auto keyIt = m_keys.find(id);
    if (!project || keyIt == m_keys.end()) {
        return;
    }
    const InstrumentTrackId key = keyIt->second;
    m_keys.erase(keyIt);

    ParkedSettings parked;
    const IProjectAudioSettingsPtr settings = project->audioSettings();
    if (settings && settings->trackHasExistingOutputParams(key)) {
        parked.output = settings->trackOutputParams(key);
    }

    auto park = [&parked, &key](const INotationPtr& notation) {
        if (!notation || !notation->soloMuteState()) {
            return;
        }
        const INotationSoloMuteStatePtr state = notation->soloMuteState();
        if (state->trackSoloMuteStateExists(key)) {
            parked.soloMute.emplace_back(state, state->trackSoloMuteState(key));
        }
        state->removeTrackSoloMuteState(key);
    };
    if (const IMasterNotationPtr master = project->masterNotation()) {
        park(master->notation());
        for (const IExcerptNotationPtr& excerpt : master->excerpts()) {
            park(excerpt->notation());
        }
    }

    // As removing an instrument does (PlaybackController::removeTrack); it fires settingsChanged,
    // which the project counts as a change outside the undo stack (03a, re-check (b))
    if (settings) {
        settings->removeTrackParams(key);
    }
    m_parked[id] = std::move(parked);
}

void ArrangementController::unparkSettings(ArrTrackId id)
{
    const auto parkedIt = m_parked.find(id);
    const INotationProjectPtr project = projectSession()->project();
    if (parkedIt == m_parked.end() || !project) {
        return;
    }
    const InstrumentTrackId key = keyOf(id);
    if (const IProjectAudioSettingsPtr settings = project->audioSettings()) {
        settings->setTrackOutputParams(key, parkedIt->second.output, /*notifySettingsChanged*/ false);
    }
    for (const auto& [weakState, state] : parkedIt->second.soloMute) {
        if (const INotationSoloMuteStatePtr soloMute = weakState.lock()) {
            soloMute->setTrackSoloMuteState(key, state);
        }
    }
    m_parked.erase(parkedIt);
}

// --- Project -----------------------------------------------------------------------------------

void ArrangementController::onProjectChanged()
{
    // Batch 2 03a: edits are recorded in the open project's score, held weakly; nothing when none
    // is open. (FD-151's transaction observer registers through ArrangementUndo::setTarget.)
    if (arrangementUndo()) {
        const INotationProjectPtr project = projectSession()->project();
        arrangementUndo()->setTarget(project ? project->masterNotation() : IMasterNotationPtr());
    }
    m_parked.clear();
    m_knownTracks.clear();
    for (const Track& track : arrangement()->tracks()) {
        m_knownTracks.insert(track.id);
    }

    m_embedLinkedAudioInCopy = false;
    m_keys.clear();
    bindTimeMap();
    addMissingAudioParts();
    refreshKeys();
}

// --- Audio staves (Batch 2 S1) -----------------------------------------------------------------

bool ArrangementController::addAudioParts(const std::vector<ArrTrackId>& ids, const muse::TranslatableString& actionName)
{
    const INotationProjectPtr project = projectSession()->project();
    const IMasterNotationPtr master = project ? project->masterNotation() : nullptr;
    if (ids.empty() || !master || !master->masterScore() || !master->notation()) {
        return false;
    }

    // One undo step for all of them
    const INotationPtr notation = master->notation();
    notation->undoStack()->prepareChanges(actionName);
    bool added = false;
    for (ArrTrackId id : ids) {
        added = addAudioPartFor(master->masterScore(), id) || added;
    }
    if (!added) {
        notation->undoStack()->rollbackChanges();
        return false;
    }
    notation->undoStack()->commitChanges();
    notation->notationChanged().send(muse::RectF());
    return true;
}

bool ArrangementController::addAudioPartFor(engraving::Score* score, ArrTrackId id)
{
    const Track* track = arrangement()->track(id);
    if (!track) {
        return false;
    }

    // The id is read while the edit is still open; a failed edit frees the part
    const engraving::Part* part = addAudioPart(score, track->name);
    if (!part) {
        return false;
    }
    const muse::ID partId = part->id();

    // A part id freed by undoing an earlier import is handed out again: a track still naming it
    // holds a broken link and must not share this part (review of #167, finding 1)
    std::vector<ArrTrackId> broken;
    for (const Track& other : arrangement()->tracks()) {
        if (other.id != id && other.partId == partId) {
            broken.push_back(other.id);
        }
    }
    for (ArrTrackId other : broken) {
        arrangement()->setTrackPart(other, muse::ID());
    }
    arrangement()->setTrackPart(id, partId);
    return true;
}

InstrumentTrackId ArrangementController::trackKey(const Track& track)
{
    if (track.partId.isValid()) {
        return { track.partId, engraving::FONTDAW_AUDIO_INSTRUMENT_ID };
    }
    return audioTrackKey(track.id);
}

InstrumentTrackId ArrangementController::keyOf(ArrTrackId id) const
{
    auto it = m_keys.find(id);
    if (it != m_keys.end()) {
        return it->second;
    }
    const Track* track = arrangement()->track(id);
    return track ? trackKey(*track) : audioTrackKey(id);
}

void ArrangementController::refreshKeys()
{
    // With a score, a part shown by two tracks (a broken link) counts for the first one only
    const INotationProjectPtr project = projectSession()->project();
    const IMasterNotationPtr master = project ? project->masterNotation() : nullptr;
    const engraving::Score* score = master ? master->masterScore() : nullptr;
    const TrackList& tracks = arrangement()->tracks();
    const std::vector<const engraving::Part*> parts = score ? resolveAudioParts(score, tracks)
                                                      : std::vector<const engraving::Part*>();

    for (size_t i = 0; i < tracks.size(); ++i) {
        const Track& track = tracks[i];
        InstrumentTrackId key = trackKey(track);
        if (score) {
            key = parts[i] ? InstrumentTrackId { parts[i]->id(), engraving::FONTDAW_AUDIO_INSTRUMENT_ID }
                  : audioTrackKey(track.id);
        }
        auto it = m_keys.find(track.id);
        if (it == m_keys.end()) {
            // First sight of the track: settings saved before S1 are under the old key
            if (key != audioTrackKey(track.id)) {
                moveTrackSettings(audioTrackKey(track.id), key);
            }
            m_keys.emplace(track.id, key);
            continue;
        }
        if (it->second == key) {
            continue;
        }

        const InstrumentTrackId previous = it->second;
        it->second = key;
        moveTrackSettings(previous, key);

        // The mixer keyed this strip by the old key: let it build the strip again
        auto engine = m_engineTracks.find(track.id);
        if (engine != m_engineTracks.end() && engine->second.id != -1) {
            m_trackRemoved.send(engine->second.id);
            m_trackAdded.send(engine->second.id);
        }
    }
}

void ArrangementController::moveTrackSettings(const InstrumentTrackId& from, const InstrumentTrackId& to)
{
    const INotationProjectPtr project = projectSession()->project();
    if (!project || from == to) {
        return;
    }

    if (const IProjectAudioSettingsPtr settings = project->audioSettings()) {
        if (settings->trackHasExistingOutputParams(from)) {
            const AudioOutputParams params = settings->trackOutputParams(from);
            settings->setTrackOutputParams(to, params);
            settings->removeTrackParams(from);
        }
    }

    auto moveSoloMute = [&from, &to](const INotationPtr& notation) {
        if (!notation || !notation->soloMuteState() || !notation->soloMuteState()->trackSoloMuteStateExists(from)) {
            return;
        }
        const INotationSoloMuteState::SoloMuteState state = notation->soloMuteState()->trackSoloMuteState(from);
        notation->soloMuteState()->setTrackSoloMuteState(to, state);
        notation->soloMuteState()->removeTrackSoloMuteState(from);
    };
    if (const IMasterNotationPtr master = project->masterNotation()) {
        moveSoloMute(master->notation());
        for (const IExcerptNotationPtr& excerpt : master->excerpts()) {
            moveSoloMute(excerpt->notation());
        }
    }
}

void ArrangementController::addMissingAudioParts()
{
    // In the app only: a command-line conversion exports the project as it is (FD-095)
    if (!m_addAudioStavesOnOpen) {
        return;
    }

    //! Projects made before S1 have tracks and no audio staves: each track that never had a part
    //! gets one, in track order. A track whose part was deleted keeps its partId and gets none.
    std::vector<ArrTrackId> missing;
    for (const Track& track : arrangement()->tracks()) {
        if (!track.partId.isValid()) {
            missing.push_back(track.id);
        }
    }
    addAudioParts(missing, muse::TranslatableString("undoableAction", "Add audio staves"));
}

void ArrangementController::bindTimeMap()
{
    if (const IMasterNotationPtr previous = m_timeMapNotation.lock()) {
        if (const INotationPlaybackPtr previousPlayback = previous->playback()) {
            previousPlayback->totalPlayTimeChanged().disconnect(this);
        }
        if (previous->notation() && previous->notation()->undoStack()) {
            previous->notation()->undoStack()->undoRedoNotification().disconnect(this);
        }
    }
    m_timeMapNotation.reset();

    const INotationProjectPtr project = projectSession()->project();
    const IMasterNotationPtr master = project ? project->masterNotation() : nullptr;
    const INotationPlaybackPtr notationPlayback = master ? master->playback() : nullptr;
    if (!notationPlayback) {
        return;
    }

    m_timeMapNotation = master;

    // The score is the master of musical time (README): bars and beats come from its tempo map.
    // Without a score (it is closing) fall back to a constant tempo instead of calling into it.
    const std::weak_ptr<IMasterNotation> weak = master;
    const TimeMap fallback = TimeMap::constantTempo(120.0);

    TimeMap timeMap;
    timeMap.tickToSecs = [weak, fallback](const Fraction& tick) {
        const IMasterNotationPtr m = weak.lock();
        const INotationPlaybackPtr p = m ? m->playback() : nullptr;
        return p ? secs_t(p->playedTickToSec(tick.ticks())) : fallback.tickToSecs(tick);
    };
    timeMap.secsToTick = [weak, fallback](secs_t secs) {
        const IMasterNotationPtr m = weak.lock();
        const INotationPlaybackPtr p = m ? m->playback() : nullptr;
        return p ? Fraction::fromTicks(p->secToPlayedTick(secs)) : fallback.secsToTick(secs);
    };
    arrangement()->setTimeMap(timeMap);

    // A new tempo or repeat structure moves what second a bar falls on
    notationPlayback->totalPlayTimeChanged().onReceive(this, [this](const secs_t) {
        arrangement()->onTempoMapChanged();
    });

    // FD-151: an undo or redo can move bars and clips without changing the total play time (a
    // time signature rewrite), and restores ticks only: derive the seconds again after each one
    if (master->notation() && master->notation()->undoStack()) {
        master->notation()->undoStack()->undoRedoNotification().onNotify(this, [this]() {
            arrangement()->onTempoMapChanged();
        });
    }
}

IProjectAudioSettingsPtr ArrangementController::audioSettings() const
{
    const INotationProjectPtr project = projectSession()->project();
    return project ? project->audioSettings() : nullptr;
}

ArrTrackId ArrangementController::arrangementTrackOf(TrackId engineTrackId) const
{
    for (const auto& [id, engineTrack] : m_engineTracks) {
        if (engineTrack.id == engineTrackId) {
            return id;
        }
    }
    return INVALID_TRACK;
}

void ArrangementController::storeOutputParams(TrackId engineTrackId, const std::function<void(AudioOutputParams&)>& change)
{
    const ArrTrackId id = arrangementTrackOf(engineTrackId);
    const IProjectAudioSettingsPtr settings = audioSettings();
    if (id == INVALID_TRACK || !settings) {
        return;
    }

    const InstrumentTrackId key = keyOf(id);
    AudioOutputParams out = settings->trackOutputParams(key);
    change(out);
    settings->setTrackOutputParams(key, out);
}

// --- For the mixer and PlaybackController -------------------------------------------------------

std::vector<IArrangementController::AudioTrack> ArrangementController::audioTracks() const
{
    std::vector<AudioTrack> result;
    for (const Track& track : arrangement()->tracks()) {
        auto it = m_engineTracks.find(track.id);
        if (it != m_engineTracks.end() && it->second.id != -1) {
            result.push_back(AudioTrack { track.id, keyOf(track.id), it->second.id });
        }
    }
    return result;
}

async::Channel<TrackId> ArrangementController::trackAdded() const
{
    return m_trackAdded;
}

async::Channel<TrackId> ArrangementController::trackRemoved() const
{
    return m_trackRemoved;
}

TrackPlayState ArrangementController::trackPlayState(ArrTrackId id) const
{
    auto it = m_playStates.find(id);
    return it != m_playStates.end() ? it->second : TrackPlayState::Empty;
}

bool ArrangementController::isLoaded() const
{
    return m_isLoaded;
}

async::Notification ArrangementController::isLoadedChanged() const
{
    return m_isLoadedChanged;
}

void ArrangementController::updateIsLoaded()
{
    bool loaded = true;
    for (const auto& [id, engineTrack] : m_engineTracks) {
        if (engineTrack.id == -1) {
            loaded = false;
            break;
        }
    }

    if (m_isLoaded != loaded) {
        m_isLoaded = loaded;
        m_isLoadedChanged.notify();
    }
}

secs_t ArrangementController::contentEnd() const
{
    return m_contentEnd;
}

async::Notification ArrangementController::contentEndChanged() const
{
    return m_contentEndChanged;
}

void ArrangementController::updateContentEnd()
{
    secs_t end = 0.0;
    for (const Track& track : arrangement()->tracks()) {
        for (const AudioClip& clip : track.clips) {
            end = std::max(end, clip.endSecs());
        }
    }

    if (std::abs(end.raw() - m_contentEnd.raw()) > 1e-9) {
        m_contentEnd = end;
        m_contentEndChanged.notify();
    }
}

bool ArrangementController::hasSoloTrack(const INotationPtr& notation) const
{
    if (!notation) {
        return false;
    }

    for (const auto& [id, engineTrack] : m_engineTracks) {
        if (engineTrack.id != -1 && notation->soloMuteState()->trackSoloMuteState(keyOf(id)).solo) {
            return true;
        }
    }
    return false;
}

void ArrangementController::updateSoloMuteStates(const INotationPtr& notation, bool hasSolo, bool muteAll)
{
    const IProjectAudioSettingsPtr settings = audioSettings();
    if (!notation || !settings) {
        return;
    }

    for (const auto& [id, engineTrack] : m_engineTracks) {
        if (engineTrack.id == -1) {
            continue;
        }

        const InstrumentTrackId key = keyOf(id);
        const auto& state = notation->soloMuteState()->trackSoloMuteState(key);
        const bool forceMute = muteAll || (hasSolo && !state.solo);

        AudioOutputParams out = settings->trackOutputParams(key);
        out.solo = state.solo;
        out.muted = state.mute || forceMute;
        out.forceMute = forceMute;

        playback()->setControlParams(engineTrack.id, out.control());
    }
}

// --- Import ------------------------------------------------------------------------------------

async::Promise<Ret> ArrangementController::importAudioTrack(const io::path_t& filePath)
{
    return async::make_promise<Ret>([this, filePath](auto resolve) {
        assetStore()->importFile(filePath).onResolve(this, [this, resolve](const RetVal<AudioAsset>& asset) {
            if (!asset.ret) {
                (void)resolve(asset.ret);
                return;
            }

            // Batch 2 03a: the track, its clip and its audio part are one undoable edit, so one
            // Ctrl+Z removes all three. addAudioPartFor pushes into the edit's transaction; it
            // must not open one of its own (IArrangementUndo::edit's contract).
            const INotationProjectPtr project = projectSession()->project();
            const IMasterNotationPtr master = project ? project->masterNotation() : nullptr;
            engraving::MasterScore* score = master ? master->masterScore() : nullptr;

            auto addTrack = [this, &asset, score](IArrangement& model) -> Ret {
                RetVal<ArrTrackId> track = model.addAudioTrack(asset.val.name);
                if (!track.ret) {
                    return track.ret;
                }

                // The whole file, at the start of the score
                AudioClip clip;
                clip.asset = asset.val.id;
                clip.name = asset.val.name;
                clip.startTick = Fraction(0, 1);
                clip.startSecs = 0.0;
                clip.length = asset.val.duration();

                RetVal<ClipId> added = model.addClip(track.val, clip);
                if (!added.ret) {
                    return added.ret;
                }

                // The audio staff that shows it, at the end of the score (Batch 2 S1)
                if (score) {
                    addAudioPartFor(score, track.val);
                }
                return muse::make_ok();
            };

            const muse::TranslatableString actionName("undoableAction", "Add audio track");
            const Ret ret = arrangementUndo() ? arrangementUndo()->edit(actionName, addTrack) : addTrack(*arrangement());
            if (!ret) {
                (void)resolve(ret);
                return;
            }
            if (master && master->notation()) {
                master->notation()->notationChanged().send(muse::RectF());
            }

            (void)resolve(muse::make_ok());
        });

        return async::Promise<Ret>::dummy_result();
    }, async::PromiseType::AsyncByBody);
}

// --- Export / share ----------------------------------------------------------------------------

Ret ArrangementController::prepareProjectCopy()
{
    m_embedLinkedAudioInCopy = false;

    if (!m_active) {
        return muse::make_ok();
    }

    // Linked audio that clips use: only that is saved with a project
    std::set<AssetId> linked;
    for (const Track& track : arrangement()->tracks()) {
        for (const AudioClip& clip : track.clips) {
            const AudioAsset* asset = assetStore()->asset(clip.asset);
            if (asset && asset->storage == AssetStorage::Referenced) {
                linked.insert(asset->id);
            }
        }
    }

    if (linked.empty()) {
        return muse::make_ok();
    }

    if (!interactive()) {
        // No one to ask (command line): the copy keeps its links unless the preference says embed
        m_embedLinkedAudioInCopy = configuration()->linkedAudioInCopies() == LinkedAudioInCopies::Embed;
        return muse::make_ok();
    }

    const LinkedAudioInCopies preference = configuration()->linkedAudioInCopies();
    if (preference != LinkedAudioInCopies::Ask) {
        m_embedLinkedAudioInCopy = preference == LinkedAudioInCopies::Embed;
        return muse::make_ok();
    }

    static constexpr int EMBED_BTN = static_cast<int>(IInteractive::Button::CustomButton) + 1;
    static constexpr int KEEP_BTN = EMBED_BTN + 1;

    const IInteractive::ButtonDatas buttons = {
        interactive()->buttonData(IInteractive::Button::Cancel),
        IInteractive::ButtonData(KEEP_BTN, muse::trc("arrangement", "Keep links")),
        IInteractive::ButtonData(EMBED_BTN, muse::trc("arrangement", "Embed"), true /*accent*/),
    };

    const std::string text = muse::qtrc("arrangement",
                                        "This project links to %n audio file(s) outside it. "
                                        "Embed them in the copy, so it can be opened on another computer?",
                                        nullptr, static_cast<int>(linked.size())).toStdString();

    const IInteractive::Result result = interactive()->questionSync(
        muse::trc("arrangement", "Embed linked audio?"), IInteractive::Text(text), buttons, EMBED_BTN,
        IInteractive::Option::WithIcon | IInteractive::Option::WithDontShowAgainCheckBox);

    if (result.button() != EMBED_BTN && result.button() != KEEP_BTN) {
        return muse::make_ret(Ret::Code::Cancel);
    }

    m_embedLinkedAudioInCopy = result.button() == EMBED_BTN;

    if (!result.showAgain()) {
        configuration()->setLinkedAudioInCopies(m_embedLinkedAudioInCopy ? LinkedAudioInCopies::Embed
                                                : LinkedAudioInCopies::KeepLinks);
    }

    return muse::make_ok();
}

bool ArrangementController::embedLinkedAudioInCopy() const
{
    return m_embedLinkedAudioInCopy;
}
