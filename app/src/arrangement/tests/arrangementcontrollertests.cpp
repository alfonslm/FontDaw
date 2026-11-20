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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cmath>
#include <map>
#include <set>

#include <QTemporaryDir>

#include "async/processevents.h"
#include "modularity/ioc.h"

#include "internal/arrangement.h"
#include "internal/arrangementundo.h"
#include "internal/arrangementcontroller.h"
#include "internal/audioassetstore.h"
#include "internal/audiopart.h"

#include "project/tests/mocks/projectsessionmock.h"
#include "interactive/tests/mocks/interactivemock.h"
#include "notation/inotationplayback.h"
#include "notation/inotationundostack.h"
#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/tests/utils/scorerw.h"
#include "engraving/editing/transaction/undostack.h"
#include "notation/tests/mocks/masternotationmock.h"
#include "notation/tests/mocks/notationmock.h"
#include "playback/tests/mocks/playbackcontrollermock.h"
#include "playback/tests/mocks/playbackmock.h"
#include "project/tests/mocks/notationprojectmock.h"

#include "mocks/arrangementconfigurationmock.h"
#include "mocks/projectaudiosettingsfake.h"
#include "playback/playbacktypes.h"
#include "testutils.h"

using ::testing::_;
using ::testing::An;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

using namespace mu;
using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace muse;
using namespace muse::audio;
using mu::engraving::Fraction;
using mu::engraving::InstrumentTrackId;

// The model's clip and the engine's clip share a name
using ModelClip = mu::arrangement::AudioClip;
using EngineClip = muse::audio::AudioClip;

static const std::string TEST_MODULE("arrangement_controller_utests");
static const modularity::IoCID TEST_CONTEXT_ID = 3;

// A score whose bars 1-2 are played twice, at 120 bpm in 4/4 (480 ticks per quarter): a bar is 1920 ticks
// and 2 s. Only the time conversions are modelled; everything else does nothing.
class RepeatedScorePlayback : public notation::INotationPlayback
{
public:
    static constexpr double TICKS_PER_SEC = 960.0;
    static constexpr int BAR = 1920;

    muse::audio::secs_t playedTickToSec(muse::midi::tick_t tick) const override { return muse::audio::secs_t(tick / TICKS_PER_SEC); }
    muse::midi::tick_t secToPlayedTick(muse::audio::secs_t sec) const override
    {
        return static_cast<muse::midi::tick_t>(std::llround(sec.raw() * TICKS_PER_SEC));
    }

    //! Written ticks: played bars 3-4 are bars 1-2 again, later bars are shifted back by two bars
    muse::midi::tick_t secToTick(muse::audio::secs_t sec) const override
    {
        const muse::midi::tick_t played = secToPlayedTick(sec);
        return played < 4 * BAR ? played % (2 * BAR) : played - 2 * BAR;
    }

    muse::async::Channel<muse::audio::secs_t> totalPlayTimeChanged() const override { return m_totalPlayTimeChanged; }
    muse::audio::secs_t totalPlayTime() const override { return muse::audio::secs_t(); }
    const muse::audio::MusicalTimeMap& musicalTimeMap() const override { return m_musicalTimeMap; }
    muse::async::Notification musicalTimeMapChanged() const override { return m_musicalTimeMapChanged; }

    void init() override {}
    void reload() override {}
    void setSendEventsOnScoreChange(const InstrumentTrackId&, bool) override {}
    void sendEventsForChangedTracks() override {}
    muse::async::Channel<mu::engraving::InstrumentTrackIdSet> tracksDataChanged() const override { return {}; }
    const InstrumentTrackId& metronomeTrackId() const override { static const InstrumentTrackId id; return id; }
    InstrumentTrackId chordSymbolsTrackId(const muse::ID&) const override { return {}; }
    std::optional<std::string> trackName(const InstrumentTrackId&) const override { return std::nullopt; }
    bool isChordSymbolsTrack(const InstrumentTrackId&) const override { return false; }
    const muse::mpe::PlaybackData& trackPlaybackData(const InstrumentTrackId&) const override
    {
        static const muse::mpe::PlaybackData data;
        return data;
    }
    void triggerEventsForItems(const std::vector<const mu::engraving::EngravingItem*>&, muse::mpe::duration_t, bool) override {}
    void triggerMetronome(muse::midi::tick_t) override {}
    void triggerCountIn(muse::midi::tick_t, muse::secs_t&) override {}
    void triggerControllers(const muse::mpe::ControllerChangeEventList&, mu::engraving::staff_idx_t, int) override {}
    mu::engraving::InstrumentTrackIdSet existingTrackIdSet() const override { return {}; }
    muse::async::Channel<InstrumentTrackId> trackAdded() const override { return {}; }
    muse::async::Channel<InstrumentTrackId> trackRemoved() const override { return {}; }
    muse::RetVal<muse::midi::tick_t> playPositionTickByRawTick(muse::midi::tick_t) const override { return {}; }
    muse::RetVal<muse::midi::tick_t> playPositionTickByElement(const mu::engraving::EngravingItem*) const override { return {}; }
    void addLoopBoundary(notation::LoopBoundaryType, muse::midi::tick_t) override {}
    void setLoopBoundariesEnabled(bool) override {}
    bool isLoopEnabled() const override { return false; }
    muse::async::Channel<bool> loopEnabledChanged() const override { return {}; }
    const notation::LoopBoundaries& loopBoundaries() const override { static const notation::LoopBoundaries b; return b; }
    muse::async::Notification loopBoundariesChanged() const override { return {}; }
    const notation::Tempo& multipliedTempo(muse::midi::tick_t) const override { static const notation::Tempo t; return t; }
    mu::engraving::MeasureBeat beat(muse::midi::tick_t) const override { return {}; }
    muse::midi::tick_t beatToRawTick(int, int) const override { return 0; }
    double tempoMultiplier() const override { return 1.0; }
    void setTempoMultiplier(double) override {}
    void addSoundFlags(const std::vector<mu::engraving::StaffText*>&) override {}
    void removeSoundFlags(const mu::engraving::InstrumentTrackIdSet&) override {}
    bool hasSoundFlags(const mu::engraving::InstrumentTrackIdSet&) override { return false; }

private:
    muse::async::Channel<muse::audio::secs_t> m_totalPlayTimeChanged;
    muse::audio::MusicalTimeMap m_musicalTimeMap;
    muse::async::Notification m_musicalTimeMapChanged;
};

// The model and the asset store are the real ones; the audio engine, the playback controller,
// the project and its notation are mocks.
class Arrangement_ControllerTests : public ::testing::Test, public async::Asyncable
{
protected:
    struct SourceCall {
        std::string key;
        uint64_t device = 0;     // as given to addTrack; the controller must keep it alive until the answer
        ByteArray bytes;         // what the engine could read from the device
    };

    struct AddCall {
        std::string name;
        TrackParams params;
        std::vector<SourceCall> sources;
        AudioClipList clips;                 // as given to addTrack
        std::vector<AudioClipList> updates;  // every list sent on clipsChanged afterwards
        std::function<void(TrackId)> resolve;
        std::function<void()> reject;
    };

    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());

        m_configuration = std::make_shared<NiceMock<ArrangementConfigurationMock> >();
        ON_CALL(*m_configuration, defaultAssetStorage()).WillByDefault(Return(AssetStorage::Embedded));
        ON_CALL(*m_configuration, audioSessionsPath()).WillByDefault(Return(io::path_t(m_dir.filePath("sessions"))));
        ON_CALL(*m_configuration, linkedAudioInCopies()).WillByDefault(Return(LinkedAudioInCopies::Ask));
        modularity::globalIoc()->registerExport<IArrangementConfiguration>(TEST_MODULE, m_configuration);

        m_playback = std::make_shared<NiceMock<PlaybackMock> >();
        m_playbackController = std::make_shared<NiceMock<playback::PlaybackControllerMock> >();
        m_projectSession = std::make_shared<NiceMock<project::ProjectSessionMock> >();
        m_interactive = std::make_shared<NiceMock<InteractiveMock> >();
        m_project = std::make_shared<NiceMock<project::NotationProjectMock> >();
        m_masterNotation = std::make_shared<NiceMock<notation::MasterNotationMock> >();
        m_notation = std::make_shared<NiceMock<notation::NotationMock> >();
        m_audioSettings = std::make_shared<ProjectAudioSettingsFake>();
        m_soloMuteState = std::make_shared<SoloMuteStateFake>();

        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        m_store = std::make_shared<AudioAssetStore>(m_iocCtx);
        modularity::ioc(m_iocCtx)->registerExport<IAudioAssetStore>(TEST_MODULE, m_store);
        m_arrangement = std::make_shared<Arrangement>(m_iocCtx);
        modularity::ioc(m_iocCtx)->registerExport<IArrangement>(TEST_MODULE, m_arrangement);
        m_arrangementUndo = std::make_shared<ArrangementUndo>(m_iocCtx); // Batch 2 03a
        modularity::ioc(m_iocCtx)->registerExport<IArrangementUndo>(TEST_MODULE, m_arrangementUndo);
        modularity::ioc(m_iocCtx)->registerExport<IPlayback>(TEST_MODULE, m_playback);
        modularity::ioc(m_iocCtx)->registerExport<playback::IPlaybackController>(TEST_MODULE, m_playbackController);
        modularity::ioc(m_iocCtx)->registerExport<project::IProjectSession>(TEST_MODULE, m_projectSession);
        modularity::ioc(m_iocCtx)->registerExport<IInteractive>(TEST_MODULE, m_interactive);

        ON_CALL(*m_playbackController, isPlaybackInited()).WillByDefault([this]() { return m_playbackInited; });
        ON_CALL(*m_playbackController, playbackInitedChanged()).WillByDefault(Return(m_playbackInitedChanged));

        ON_CALL(*m_projectSession, projectChanged()).WillByDefault(Return(m_currentProjectChanged));
        ON_CALL(*m_projectSession, project()).WillByDefault(Return(project::INotationProjectPtr(m_project)));

        ON_CALL(*m_project, audioSettings()).WillByDefault(Return(project::IProjectAudioSettingsPtr(m_audioSettings)));
        ON_CALL(*m_project, masterNotation()).WillByDefault(Return(notation::IMasterNotationPtr(m_masterNotation)));
        ON_CALL(*m_masterNotation, notation()).WillByDefault(Return(notation::INotationPtr(m_notation)));
        ON_CALL(*m_masterNotation, excerpts()).WillByDefault(ReturnRef(m_noExcerpts));
        ON_CALL(*m_masterNotation, playback()).WillByDefault(Return(notation::INotationPlaybackPtr()));
        ON_CALL(*m_notation, soloMuteState()).WillByDefault(Return(notation::INotationSoloMuteStatePtr(m_soloMuteState)));

        ON_CALL(*m_playback, controlParamsChanged()).WillByDefault(Return(m_controlParamsChanged));
        ON_CALL(*m_playback, fxChainParamsChanged()).WillByDefault(Return(m_fxChainParamsChanged));
        ON_CALL(*m_playback, auxSendsParamsChanged()).WillByDefault(Return(m_auxSendsParamsChanged));

        // The old overload (one file) must not be used for audio tracks any more
        EXPECT_CALL(*m_playback, addTrack(_, An<io::IODevice*>(), _)).Times(0);

        ON_CALL(*m_playback, addTrack(_, An<const AudioClipTrackData&>(), _))
        .WillByDefault([this](const TrackName& name, const AudioClipTrackData& data, const TrackParams& params) {
            const size_t index = m_addCalls.size();
            AddCall call;
            call.name = name;
            call.params = params;
            call.clips = data.clips;
            for (const AudioClipSource& source : data.sources) {
                SourceCall sourceCall;
                sourceCall.key = source.key;
                sourceCall.device = source.device;
                io::IODevice* device = reinterpret_cast<io::IODevice*>(source.device);
                if (device && device->isOpen() && device->seek(0)) {
                    sourceCall.bytes = ByteArray(device->readData(), device->size());
                }
                call.sources.push_back(sourceCall);
            }
            m_addCalls.push_back(call);

            // The lists the app sends from now on, also while the engine is still adding the track
            async::Channel<AudioClipList> clipsChanged = data.clipsChanged;
            clipsChanged.onReceive(this, [this, index](const AudioClipList& list) {
                m_addCalls[index].updates.push_back(list);
            });

            return async::make_promise<TrackId, TrackParams>([this, index, params](auto resolve, auto reject) {
                m_addCalls[index].resolve = [resolve, params](TrackId id) { (void)resolve(id, params); };
                m_addCalls[index].reject = [reject]() { (void)reject(1, "refused"); };
                return async::Promise<TrackId, TrackParams>::dummy_result();
            }, async::PromiseType::AsyncByBody);
        });

        ON_CALL(*m_playback, removeTrack(_)).WillByDefault([this](const TrackId id) { m_removed.push_back(id); });
        ON_CALL(*m_playback, setControlParams(_, _)).WillByDefault([this](const TrackId id, const ControlParams& params) {
            m_controlParams[id] = params;
        });

        m_controller = std::make_shared<ArrangementController>(m_iocCtx);
        m_controller->init();
        m_controller->trackAdded().onReceive(this, [this](const TrackId id) { m_addedSignals.push_back(id); });
        m_controller->trackRemoved().onReceive(this, [this](const TrackId id) { m_removedSignals.push_back(id); });

        // A 2 s stereo file, imported and embedded
        m_wav = makeSineWav(2.0, 8000, 2);
        m_wavPath = io::path_t(m_dir.filePath("Guitar.wav"));
        ASSERT_TRUE(writeFile(m_wavPath, m_wav));
        RetVal<AudioAsset> asset = m_store->importFileSync(m_wavPath, AssetStorage::Embedded);
        ASSERT_TRUE(asset.ret) << asset.ret.text();
        m_asset = asset.val.id;
    }

    void TearDown() override
    {
        m_controller->deinit();
        m_controller = nullptr;
        m_arrangementUndo = nullptr;
        m_arrangement = nullptr;
        m_store = nullptr;
        pump();

        modularity::globalIoc()->unregister<IArrangementConfiguration>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    static void pump()
    {
        for (int i = 0; i < 10; ++i) {
            async::processMessages();
        }
    }

    void setPlaybackInited(bool inited)
    {
        m_playbackInited = inited;
        m_playbackInitedChanged.send(inited);
        pump();
    }

    //! A track with the whole file at the start of the score: what "Import audio track" makes
    ArrTrackId addWholeFileTrack(const String& name = u"Guitar")
    {
        RetVal<ArrTrackId> track = m_arrangement->addAudioTrack(name);
        EXPECT_TRUE(track.ret);
        ModelClip clip;
        clip.asset = m_asset;
        clip.length = 2.0;
        EXPECT_TRUE(m_arrangement->addClip(track.val, clip).ret);
        pump();
        return track.val;
    }

    //! A second file (1 s, mono), imported and embedded
    AssetId importSecondAsset()
    {
        m_wav2 = makeSineWav(1.0, 8000, 1, 660.0);
        const io::path_t path(m_dir.filePath("Bass.wav"));
        EXPECT_TRUE(writeFile(path, m_wav2));
        RetVal<AudioAsset> asset = m_store->importFileSync(path, AssetStorage::Embedded);
        EXPECT_TRUE(asset.ret) << asset.ret.text();
        return asset.val.id;
    }

    //! A clip of `asset` on `track`; `startTick` places it on a Musical track (a whole note is 2 s at 120 bpm)
    ClipId addClipOf(ArrTrackId track, const AssetId& asset, const Fraction& startTick, double length, double sourceOffset = 0.0)
    {
        ModelClip clip;
        clip.asset = asset;
        clip.startTick = startTick;
        clip.sourceOffset = sourceOffset;
        clip.length = length;
        RetVal<ClipId> added = m_arrangement->addClip(track, clip);
        EXPECT_TRUE(added.ret) << added.ret.text();
        pump();
        return added.val;
    }

    void resolveAdd(size_t index, TrackId engineTrackId)
    {
        ASSERT_LT(index, m_addCalls.size());
        ASSERT_TRUE(m_addCalls[index].resolve);
        m_addCalls[index].resolve(engineTrackId);
        pump();
    }

    QTemporaryDir m_dir;
    ByteArray m_wav;
    ByteArray m_wav2;
    io::path_t m_wavPath;
    AssetId m_asset;

    std::shared_ptr<NiceMock<ArrangementConfigurationMock> > m_configuration;
    std::shared_ptr<NiceMock<PlaybackMock> > m_playback;
    std::shared_ptr<NiceMock<playback::PlaybackControllerMock> > m_playbackController;
    std::shared_ptr<NiceMock<project::ProjectSessionMock> > m_projectSession;
    std::shared_ptr<NiceMock<InteractiveMock> > m_interactive;
    std::shared_ptr<NiceMock<project::NotationProjectMock> > m_project;
    std::shared_ptr<NiceMock<notation::MasterNotationMock> > m_masterNotation;
    std::shared_ptr<NiceMock<notation::NotationMock> > m_notation;
    std::shared_ptr<ProjectAudioSettingsFake> m_audioSettings;
    std::shared_ptr<SoloMuteStateFake> m_soloMuteState;
    notation::ExcerptNotationList m_noExcerpts;

    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<AudioAssetStore> m_store;
    std::shared_ptr<ArrangementUndo> m_arrangementUndo;
    std::shared_ptr<Arrangement> m_arrangement;
    std::shared_ptr<ArrangementController> m_controller;

    bool m_playbackInited = false;
    async::Channel<bool> m_playbackInitedChanged;
    async::Notification m_currentProjectChanged;
    async::Channel<TrackId, ControlParams> m_controlParamsChanged;
    async::Channel<TrackId, AudioFxChain> m_fxChainParamsChanged;
    async::Channel<TrackId, AuxSendsParams> m_auxSendsParamsChanged;

    std::vector<AddCall> m_addCalls;
    std::vector<TrackId> m_removed;
    std::map<TrackId, ControlParams> m_controlParams;
    std::vector<TrackId> m_addedSignals;
    std::vector<TrackId> m_removedSignals;
};

// --- What can play ------------------------------------------------------------------------------

TEST_F(Arrangement_ControllerTests, PlanPlaysOneWholeFileAtTheStart)
{
    Track track;
    track.id = 1;

    // No clip, or only muted ones: nothing to play
    EXPECT_EQ(ArrangementController::planTrack(track, m_store.get()).state, TrackPlayState::Empty);

    ModelClip clip;
    clip.id = 1;
    clip.asset = m_asset;
    clip.length = 2.0;
    track.clips = { clip };

    ArrangementController::Plan plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.assets.size(), 1u);
    EXPECT_EQ(plan.assets.begin()->first, m_asset);
    EXPECT_EQ(plan.assets.begin()->second, m_store->assetFilePath(m_asset));
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].sourceKey, m_asset);
    EXPECT_DOUBLE_EQ(plan.clips[0].startSecs.raw(), 0.0);
    EXPECT_DOUBLE_EQ(plan.clips[0].sourceOffsetSecs.raw(), 0.0);
    EXPECT_DOUBLE_EQ(plan.clips[0].lengthSecs.raw(), 2.0);
    EXPECT_FALSE(plan.clips[0].muted);

    track.clips[0].muted = true;
    EXPECT_EQ(ArrangementController::planTrack(track, m_store.get()).state, TrackPlayState::Empty);
}

TEST_F(Arrangement_ControllerTests, PlanPlaysOtherShapesWhereTheyAre)
{
    ModelClip whole;
    whole.id = 1;
    whole.asset = m_asset;
    whole.length = 2.0;

    Track track;
    track.id = 1;

    // Starts later
    ModelClip later = whole;
    later.startSecs = 1.0;
    track.clips = { later };
    ArrangementController::Plan plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_DOUBLE_EQ(plan.clips[0].startSecs.raw(), 1.0);

    // Trimmed at the front, trimmed at the end
    ModelClip trimmedFront = whole;
    trimmedFront.sourceOffset = 0.5;
    trimmedFront.length = 1.5;
    track.clips = { trimmedFront };
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_DOUBLE_EQ(plan.clips[0].sourceOffsetSecs.raw(), 0.5);
    EXPECT_DOUBLE_EQ(plan.clips[0].lengthSecs.raw(), 1.5);

    ModelClip trimmedEnd = whole;
    trimmedEnd.length = 1.0;
    track.clips = { trimmedEnd };
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_DOUBLE_EQ(plan.clips[0].lengthSecs.raw(), 1.0);

    // Two clips that overlap in time, as a faster tempo can leave them: both go to the engine,
    // which plays the later one where they overlap
    ModelClip second = whole;
    second.id = 2;
    second.startSecs = 1.0;
    track.clips = { whole, second };
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 2u);
    EXPECT_DOUBLE_EQ(plan.clips[0].startSecs.raw(), 0.0);
    EXPECT_DOUBLE_EQ(plan.clips[1].startSecs.raw(), 1.0);
    EXPECT_EQ(plan.assets.size(), 1u);

    // A muted clip is part of the list (muting is a list update), flagged as muted
    track.clips[1].muted = true;
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 2u);
    EXPECT_FALSE(plan.clips[0].muted);
    EXPECT_TRUE(plan.clips[1].muted);

    // Audio that is not there: all clips offline
    ModelClip missing = whole;
    missing.asset = std::string(40, 'a');
    track.clips = { missing };
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Offline);
    EXPECT_TRUE(plan.clips.empty());

    // A clip whose audio is not there is left out of the list; the others play
    track.clips = { missing, whole };
    track.clips[1].id = 2;
    plan = ArrangementController::planTrack(track, m_store.get());
    EXPECT_EQ(plan.state, TrackPlayState::Playing);
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].sourceKey, m_asset);
    EXPECT_EQ(plan.assets.size(), 1u);
}

// --- Engine tracks ------------------------------------------------------------------------------

TEST_F(Arrangement_ControllerTests, AddsTheFileToTheEngineWhenPlaybackIsReady)
{
    const ArrTrackId id = addWholeFileTrack();

    // The sequence is not set up yet: nothing goes to the engine
    EXPECT_TRUE(m_addCalls.empty());
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Playing);

    setPlaybackInited(true);

    ASSERT_EQ(m_addCalls.size(), 1u);
    EXPECT_EQ(m_addCalls[0].name, "Guitar");
    // The engine gets the file's own bytes, as the one source the clip uses
    ASSERT_EQ(m_addCalls[0].sources.size(), 1u);
    EXPECT_EQ(m_addCalls[0].sources[0].key, m_asset);
    EXPECT_EQ(m_addCalls[0].sources[0].bytes, m_wav);
    ASSERT_EQ(m_addCalls[0].clips.size(), 1u);
    EXPECT_EQ(m_addCalls[0].clips[0].sourceKey, m_asset);
    // A recording starts with its aux sends in place and nothing sent to them
    ASSERT_EQ(m_addCalls[0].params.auxSends.size(), static_cast<size_t>(mu::playback::AUX_CHANNEL_NUM));
    EXPECT_FLOAT_EQ(m_addCalls[0].params.auxSends[0].signalAmount, 0.f);

    // Not there for the mixer or the export until the engine has answered
    EXPECT_FALSE(m_controller->isLoaded());
    EXPECT_TRUE(m_controller->audioTracks().empty());

    resolveAdd(0, 42);

    EXPECT_TRUE(m_controller->isLoaded());
    ASSERT_EQ(m_controller->audioTracks().size(), 1u);
    EXPECT_EQ(m_controller->audioTracks()[0].id, id);
    EXPECT_EQ(m_controller->audioTracks()[0].engineTrackId, 42);
    EXPECT_EQ(m_controller->audioTracks()[0].key, audioTrackKey(id));
    EXPECT_EQ(m_addedSignals, std::vector<TrackId> { 42 });

    // Its mixer settings are kept where instruments keep theirs
    EXPECT_TRUE(m_audioSettings->trackHasExistingOutputParams(audioTrackKey(id)));
}

TEST_F(Arrangement_ControllerTests, RemovingTheTrackRemovesItFromTheEngineAndItsSettings)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    resolveAdd(0, 42);

    ASSERT_TRUE(m_arrangement->removeTrack(id));
    pump();

    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_EQ(m_removedSignals, std::vector<TrackId> { 42 });
    EXPECT_TRUE(m_controller->audioTracks().empty());
    EXPECT_FALSE(m_audioSettings->trackHasExistingOutputParams(audioTrackKey(id)));
}

TEST_F(Arrangement_ControllerTests, TrackRemovedWhileTheEngineWasAddingItDoesNotStayInTheEngine)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    ASSERT_EQ(m_addCalls.size(), 1u);

    ASSERT_TRUE(m_arrangement->removeTrack(id));
    pump();
    EXPECT_TRUE(m_removed.empty()); // the engine has not given it an id yet

    resolveAdd(0, 42);

    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_TRUE(m_controller->audioTracks().empty());
    EXPECT_TRUE(m_addedSignals.empty());
    EXPECT_TRUE(m_controller->isLoaded());
}

TEST_F(Arrangement_ControllerTests, DeviceStaysAliveUntilTheEngineAnswers)
{
    setPlaybackInited(true);
    addWholeFileTrack();
    ASSERT_EQ(m_addCalls.size(), 1u);
    ASSERT_EQ(m_addCalls[0].sources.size(), 1u);
    io::IODevice* device = reinterpret_cast<io::IODevice*>(m_addCalls[0].sources[0].device);
    ASSERT_NE(device, nullptr);

    // The request is not answered; the controller goes away from the project (async handlers dropped)
    m_controller->deinit();
    pump();

    // The engine thread may still be reading: the device and its data must still be there
    EXPECT_TRUE(device->isOpen());
    EXPECT_GT(device->size(), 0);
    EXPECT_NE(device->readData(), nullptr);
}

TEST_F(Arrangement_ControllerTests, TracksComeBackWhenTheSequenceIsSetUpAgain)
{
    setPlaybackInited(true);
    addWholeFileTrack();
    resolveAdd(0, 42);

    // PlaybackController resets the sequence (project or audio system changed) ...
    setPlaybackInited(false);
    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_TRUE(m_controller->audioTracks().empty());

    // ... and sets it up again
    setPlaybackInited(true);
    ASSERT_EQ(m_addCalls.size(), 2u);
    resolveAdd(1, 43);

    ASSERT_EQ(m_controller->audioTracks().size(), 1u);
    EXPECT_EQ(m_controller->audioTracks()[0].engineTrackId, 43);
}

TEST_F(Arrangement_ControllerTests, MutingEveryClipTakesTheTrackOutOfTheEngineAndAMoveIsOnlyAnUpdate)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    resolveAdd(0, 42);
    const ClipId clip = m_arrangement->track(id)->clips.front().id;

    // Nothing left that sounds: the track leaves the engine
    ASSERT_TRUE(m_arrangement->setClipMuted(id, clip, true));
    pump();
    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Empty);

    ASSERT_TRUE(m_arrangement->setClipMuted(id, clip, false));
    pump();
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Playing);
    resolveAdd(1, 43);

    // Moved off the start (a whole note is 2 s at 120 bpm): one new list, the track stays
    ASSERT_TRUE(m_arrangement->moveClip(id, clip, id, Fraction(1, 1)));
    pump();
    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_EQ(m_addCalls.size(), 2u);
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Playing);
    ASSERT_EQ(m_addCalls[1].updates.size(), 1u);
    ASSERT_EQ(m_addCalls[1].updates[0].size(), 1u);
    EXPECT_NEAR(m_addCalls[1].updates[0][0].startSecs.raw(), 2.0, 1e-9);
}

TEST_F(Arrangement_ControllerTests, EngineRefusalLeavesTheTrackOffline)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    ASSERT_EQ(m_addCalls.size(), 1u);

    m_addCalls[0].reject();
    pump();

    EXPECT_TRUE(m_controller->isLoaded());
    EXPECT_TRUE(m_controller->audioTracks().empty());
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Offline);
}

// --- Clip lists -----------------------------------------------------------------------------------

TEST_F(Arrangement_ControllerTests, EveryValidTrackBecomesAClipList)
{
    const AssetId second = importSecondAsset();

    // Track 1: three clips of one file (a whole note is 2 s at 120 bpm)
    RetVal<ArrTrackId> drums = m_arrangement->addAudioTrack(u"Drums");
    ASSERT_TRUE(drums.ret);

    ModelClip first;
    first.asset = m_asset;
    first.startTick = Fraction(1, 2); // 1 s
    first.length = 1.0;
    first.gainDb = -3.f;
    first.fadeIn = Fade { 0.1, FadeCurve::EqualPower };
    first.fadeOut = Fade { 0.2, FadeCurve::Linear };
    ASSERT_TRUE(m_arrangement->addClip(drums.val, first).ret);

    ModelClip trimmed;
    trimmed.asset = m_asset;
    trimmed.startTick = Fraction(1, 1); // 2 s
    trimmed.sourceOffset = 0.5;
    trimmed.length = 1.0;
    trimmed.gainDb = 2.f;
    trimmed.fadeOut = Fade { 0.25, FadeCurve::EqualPower };
    ASSERT_TRUE(m_arrangement->addClip(drums.val, trimmed).ret);

    ModelClip muted;
    muted.asset = m_asset;
    muted.startTick = Fraction(3, 2); // 3 s
    muted.length = 2.0;
    muted.gainDb = 1.5f;
    muted.muted = true;
    ASSERT_TRUE(m_arrangement->addClip(drums.val, muted).ret);

    // Track 2: clips from two files
    RetVal<ArrTrackId> mixed = m_arrangement->addAudioTrack(u"Mixed");
    ASSERT_TRUE(mixed.ret);
    addClipOf(mixed.val, m_asset, Fraction(0, 1), 2.0);
    addClipOf(mixed.val, second, Fraction(1, 1), 1.0);
    pump();

    setPlaybackInited(true);

    // One request for each track
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_EQ(m_addCalls[0].name, "Drums");
    EXPECT_EQ(m_addCalls[1].name, "Mixed");

    // One source per distinct file, keyed by the asset id
    ASSERT_EQ(m_addCalls[0].sources.size(), 1u);
    EXPECT_EQ(m_addCalls[0].sources[0].key, m_asset);
    EXPECT_NE(m_addCalls[0].sources[0].device, 0u);
    EXPECT_EQ(m_addCalls[0].sources[0].bytes, m_wav);

    const AudioClipList& list = m_addCalls[0].clips;
    ASSERT_EQ(list.size(), 3u);
    for (const EngineClip& clip : list) {
        EXPECT_EQ(clip.sourceKey, m_asset);
        EXPECT_DOUBLE_EQ(clip.stretchRatio, 1.0);
    }

    EXPECT_NEAR(list[0].startSecs.raw(), 1.0, 1e-9);
    EXPECT_NEAR(list[0].sourceOffsetSecs.raw(), 0.0, 1e-9);
    EXPECT_NEAR(list[0].lengthSecs.raw(), 1.0, 1e-9);
    EXPECT_FLOAT_EQ(list[0].gainDb, -3.f);
    EXPECT_NEAR(list[0].fadeIn.length.raw(), 0.1, 1e-9);
    EXPECT_EQ(list[0].fadeIn.curve, AudioFadeCurve::EqualPower);
    EXPECT_NEAR(list[0].fadeOut.length.raw(), 0.2, 1e-9);
    EXPECT_EQ(list[0].fadeOut.curve, AudioFadeCurve::Linear);
    EXPECT_FALSE(list[0].muted);

    EXPECT_NEAR(list[1].startSecs.raw(), 2.0, 1e-9);
    EXPECT_NEAR(list[1].sourceOffsetSecs.raw(), 0.5, 1e-9);
    EXPECT_NEAR(list[1].lengthSecs.raw(), 1.0, 1e-9);
    EXPECT_FLOAT_EQ(list[1].gainDb, 2.f);
    EXPECT_NEAR(list[1].fadeIn.length.raw(), 0.0, 1e-9);
    EXPECT_NEAR(list[1].fadeOut.length.raw(), 0.25, 1e-9);
    EXPECT_EQ(list[1].fadeOut.curve, AudioFadeCurve::EqualPower);
    EXPECT_FALSE(list[1].muted);

    EXPECT_NEAR(list[2].startSecs.raw(), 3.0, 1e-9);
    EXPECT_NEAR(list[2].lengthSecs.raw(), 2.0, 1e-9);
    EXPECT_FLOAT_EQ(list[2].gainDb, 1.5f);
    EXPECT_TRUE(list[2].muted);

    // The track with two files has two sources
    ASSERT_EQ(m_addCalls[1].sources.size(), 2u);
    std::map<std::string, ByteArray> bytesByKey;
    for (const SourceCall& source : m_addCalls[1].sources) {
        EXPECT_NE(source.device, 0u);
        bytesByKey[source.key] = source.bytes;
    }
    ASSERT_EQ(bytesByKey.size(), 2u);
    EXPECT_EQ(bytesByKey[m_asset], m_wav);
    EXPECT_EQ(bytesByKey[second], m_wav2);

    ASSERT_EQ(m_addCalls[1].clips.size(), 2u);
    EXPECT_EQ(m_addCalls[1].clips[0].sourceKey, m_asset);
    EXPECT_NEAR(m_addCalls[1].clips[0].startSecs.raw(), 0.0, 1e-9);
    EXPECT_EQ(m_addCalls[1].clips[1].sourceKey, second);
    EXPECT_NEAR(m_addCalls[1].clips[1].startSecs.raw(), 2.0, 1e-9);
    EXPECT_NEAR(m_addCalls[1].clips[1].lengthSecs.raw(), 1.0, 1e-9);
}

TEST_F(Arrangement_ControllerTests, AModelEditSendsOneUpdate)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    resolveAdd(0, 42);
    const ClipId clip = m_arrangement->track(id)->clips.front().id;
    ASSERT_EQ(m_addCalls.size(), 1u);
    EXPECT_TRUE(m_addCalls[0].updates.empty());

    // One edit: one new list on the track's channel, the track stays in the engine
    ASSERT_TRUE(m_arrangement->setClipGain(id, clip, -6.f));
    pump();

    EXPECT_EQ(m_addCalls.size(), 1u);
    EXPECT_TRUE(m_removed.empty());
    ASSERT_EQ(m_addCalls[0].updates.size(), 1u);
    ASSERT_EQ(m_addCalls[0].updates[0].size(), 1u);
    EXPECT_FLOAT_EQ(m_addCalls[0].updates[0][0].gainDb, -6.f);
    EXPECT_EQ(m_addCalls[0].updates[0][0].sourceKey, m_asset);

    // An edit that leaves the list as it was sends nothing
    ASSERT_TRUE(m_arrangement->setClipGain(id, clip, -6.f));
    pump();
    EXPECT_EQ(m_addCalls[0].updates.size(), 1u);

    // Neither does a change to something else
    ASSERT_TRUE(m_arrangement->addAudioTrack(u"Empty").ret);
    pump();
    EXPECT_EQ(m_addCalls[0].updates.size(), 1u);
    EXPECT_EQ(m_addCalls.size(), 1u);

    // Another edit, another list
    ASSERT_TRUE(m_arrangement->trimClip(id, clip, 0.5, 1.0));
    pump();
    ASSERT_EQ(m_addCalls[0].updates.size(), 2u);
    EXPECT_NEAR(m_addCalls[0].updates[1][0].sourceOffsetSecs.raw(), 0.5, 1e-9);
    EXPECT_NEAR(m_addCalls[0].updates[1][0].lengthSecs.raw(), 1.0, 1e-9);
    EXPECT_EQ(m_addCalls.size(), 1u);
    EXPECT_TRUE(m_removed.empty());
}

TEST_F(Arrangement_ControllerTests, AnEditWhileTheEngineIsStillAddingTheTrackIsSentToo)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    ASSERT_EQ(m_addCalls.size(), 1u);
    const ClipId clip = m_arrangement->track(id)->clips.front().id;

    // Not answered yet: the list goes on the channel the request carried; the engine applies it once the track exists
    ASSERT_TRUE(m_arrangement->setClipGain(id, clip, -3.f));
    pump();

    EXPECT_EQ(m_addCalls.size(), 1u);
    EXPECT_TRUE(m_removed.empty());
    ASSERT_EQ(m_addCalls[0].updates.size(), 1u);
    ASSERT_EQ(m_addCalls[0].updates[0].size(), 1u);
    EXPECT_FLOAT_EQ(m_addCalls[0].updates[0][0].gainDb, -3.f);

    resolveAdd(0, 42);

    EXPECT_EQ(m_addCalls.size(), 1u);
    EXPECT_TRUE(m_removed.empty());
    EXPECT_EQ(m_addCalls[0].updates.size(), 1u);
    ASSERT_EQ(m_controller->audioTracks().size(), 1u);
    EXPECT_EQ(m_controller->audioTracks()[0].engineTrackId, 42);
}

TEST_F(Arrangement_ControllerTests, ATempoChangeMovesMusicalClipsAndNotAbsoluteOnes)
{
    // A whole note is 2 s at 120 bpm and 4 s at 60 bpm
    RetVal<ArrTrackId> musical = m_arrangement->addAudioTrack(u"Musical", -1, TimeBase::Musical);
    ASSERT_TRUE(musical.ret);
    addClipOf(musical.val, m_asset, Fraction(1, 1), 2.0);

    RetVal<ArrTrackId> absolute = m_arrangement->addAudioTrack(u"Absolute", -1, TimeBase::Absolute);
    ASSERT_TRUE(absolute.ret);
    ModelClip clip;
    clip.asset = m_asset;
    clip.startSecs = 1.0;
    clip.length = 2.0;
    ASSERT_TRUE(m_arrangement->addClip(absolute.val, clip).ret);
    pump();

    setPlaybackInited(true);
    ASSERT_EQ(m_addCalls.size(), 2u);
    ASSERT_EQ(m_addCalls[0].name, "Musical");
    ASSERT_EQ(m_addCalls[1].name, "Absolute");
    EXPECT_NEAR(m_addCalls[0].clips[0].startSecs.raw(), 2.0, 1e-9);
    EXPECT_NEAR(m_addCalls[1].clips[0].startSecs.raw(), 1.0, 1e-9);
    resolveAdd(0, 42);
    resolveAdd(1, 43);

    m_arrangement->setTimeMap(TimeMap::constantTempo(60.0));
    pump();

    // Nothing was removed or added again
    EXPECT_EQ(m_addCalls.size(), 2u);
    EXPECT_TRUE(m_removed.empty());

    ASSERT_EQ(m_addCalls[0].updates.size(), 1u);
    ASSERT_EQ(m_addCalls[0].updates[0].size(), 1u);
    EXPECT_NEAR(m_addCalls[0].updates[0][0].startSecs.raw(), 4.0, 1e-9);
    EXPECT_NEAR(m_addCalls[0].updates[0][0].lengthSecs.raw(), 2.0, 1e-9);

    EXPECT_TRUE(m_addCalls[1].updates.empty());
}

TEST_F(Arrangement_ControllerTests, IsLoadedUntilEveryListIsInTheEngine)
{
    addWholeFileTrack(u"First");
    addWholeFileTrack(u"Second");

    int notified = 0;
    m_controller->isLoadedChanged().onNotify(this, [&notified]() { ++notified; });
    EXPECT_TRUE(m_controller->isLoaded());

    setPlaybackInited(true);
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_FALSE(m_controller->isLoaded());
    EXPECT_EQ(notified, 1);

    resolveAdd(0, 42);
    EXPECT_FALSE(m_controller->isLoaded());
    EXPECT_EQ(notified, 1);

    resolveAdd(1, 43);
    EXPECT_TRUE(m_controller->isLoaded());
    EXPECT_EQ(notified, 2);
}

TEST_F(Arrangement_ControllerTests, AddingAnotherFileReaddsTheTrack)
{
    const AssetId second = importSecondAsset();

    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    resolveAdd(0, 42);
    ASSERT_EQ(m_addCalls.size(), 1u);
    ASSERT_EQ(m_addCalls[0].sources.size(), 1u);

    // The engine decodes a track's files when it adds the track: a new file means a new track
    addClipOf(id, second, Fraction(1, 1), 1.0);

    EXPECT_EQ(m_removed, std::vector<TrackId> { 42 });
    EXPECT_TRUE(m_addCalls[0].updates.empty());
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_EQ(m_addCalls[1].name, "Guitar");
    ASSERT_EQ(m_addCalls[1].sources.size(), 2u);
    std::set<std::string> keys;
    for (const SourceCall& source : m_addCalls[1].sources) {
        keys.insert(source.key);
    }
    EXPECT_EQ(keys, (std::set<std::string> { m_asset, second }));
    ASSERT_EQ(m_addCalls[1].clips.size(), 2u);

    resolveAdd(1, 43);
    ASSERT_EQ(m_controller->audioTracks().size(), 1u);
    EXPECT_EQ(m_controller->audioTracks()[0].engineTrackId, 43);
}

TEST_F(Arrangement_ControllerTests, TimeMapUsesPlayedTicksBothWays)
{
    // The score is bound: its tempo map becomes the arrangement's
    auto notationPlayback = std::make_shared<RepeatedScorePlayback>();
    ON_CALL(*m_masterNotation, playback()).WillByDefault(Return(notation::INotationPlaybackPtr(notationPlayback)));
    m_currentProjectChanged.notify();
    pump();

    // Bar 3 as the second pass through the repeat plays it: tick 2/1 (3840) is 4 s
    RetVal<ArrTrackId> musical = m_arrangement->addAudioTrack(u"Musical", -1, TimeBase::Musical);
    ASSERT_TRUE(musical.ret);
    addClipOf(musical.val, m_asset, Fraction(2, 1), 2.0);
    EXPECT_NEAR(m_arrangement->track(musical.val)->clips[0].startSecs.raw(), 4.0, 1e-9);

    // ... and 4 s on an Absolute track is that same played tick again, not the written tick 0 of bar 1
    RetVal<ArrTrackId> absolute = m_arrangement->addAudioTrack(u"Absolute", -1, TimeBase::Absolute);
    ASSERT_TRUE(absolute.ret);
    ModelClip clip;
    clip.asset = m_asset;
    clip.startSecs = 4.0;
    clip.length = 2.0;
    ASSERT_TRUE(m_arrangement->addClip(absolute.val, clip).ret);
    EXPECT_EQ(m_arrangement->track(absolute.val)->clips[0].startTick, Fraction(2, 1));

    // The written tick of that moment is 0: the engine must not be given that one
    EXPECT_EQ(notationPlayback->secToTick(4.0), 0);
}

// --- Duration, mixer settings, solo and mute -----------------------------------------------------

TEST_F(Arrangement_ControllerTests, ContentEndIsTheEndOfTheLastClip)
{
    int notified = 0;
    m_controller->contentEndChanged().onNotify(this, [&notified]() { ++notified; });

    EXPECT_DOUBLE_EQ(m_controller->contentEnd().raw(), 0.0);

    const ArrTrackId id = addWholeFileTrack();
    EXPECT_NEAR(m_controller->contentEnd().raw(), 2.0, 1e-9);
    EXPECT_EQ(notified, 1);

    ASSERT_TRUE(m_arrangement->removeTrack(id));
    pump();
    EXPECT_DOUBLE_EQ(m_controller->contentEnd().raw(), 0.0);
    EXPECT_EQ(notified, 2);
}

TEST_F(Arrangement_ControllerTests, MixerChangesAreSavedWithTheProjectAndUsedAgain)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    resolveAdd(0, 42);

    ControlParams control;
    control.volume = volume_db_t::make(-6.f);
    control.balance = balance_t::make(0.5f);
    m_controlParamsChanged.send(42, control);
    m_auxSendsParamsChanged.send(42, AuxSendsParams { AuxSendParams { 0.3f, true } });
    // Another track's change is not ours to store
    m_controlParamsChanged.send(7, ControlParams());
    pump();

    const project::AudioOutputParams& stored = m_audioSettings->trackOutputParams(audioTrackKey(id));
    EXPECT_FLOAT_EQ(stored.volume.raw(), -6.f);
    EXPECT_FLOAT_EQ(stored.balance.raw(), 0.5f);
    ASSERT_EQ(stored.auxSends.size(), 1u);
    EXPECT_FLOAT_EQ(stored.auxSends[0].signalAmount, 0.3f);

    // The sequence is set up again (as after reopening the project): the stored settings are used
    setPlaybackInited(false);
    setPlaybackInited(true);
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_FLOAT_EQ(std::get<volume_db_t>(m_addCalls[1].params.control.volume.value()).raw(), -6.f);
    ASSERT_EQ(m_addCalls[1].params.auxSends.size(), 1u);
    EXPECT_FLOAT_EQ(m_addCalls[1].params.auxSends[0].signalAmount, 0.3f);
}

TEST_F(Arrangement_ControllerTests, SoloAndMuteWorkAcrossInstrumentsAndAudioTracks)
{
    setPlaybackInited(true);
    const ArrTrackId first = addWholeFileTrack(u"First");
    const ArrTrackId second = addWholeFileTrack(u"Second");
    resolveAdd(0, 42);
    resolveAdd(1, 43);

    EXPECT_FALSE(m_controller->hasSoloTrack(m_notation));

    // An instrument is soloed: both audio tracks go silent
    m_controller->updateSoloMuteStates(m_notation, true /*hasSolo*/);
    EXPECT_TRUE(m_controlParams[42].muted);
    EXPECT_TRUE(m_controlParams[43].muted);

    // One audio track is soloed: it plays, the other does not
    m_soloMuteState->setTrackSoloMuteState(audioTrackKey(first), { false /*mute*/, true /*solo*/ });
    EXPECT_TRUE(m_controller->hasSoloTrack(m_notation));
    m_controller->updateSoloMuteStates(m_notation, true);
    EXPECT_FALSE(m_controlParams[42].muted);
    EXPECT_TRUE(m_controlParams[43].muted);

    // No solo anywhere; the second one is muted by its own button
    m_soloMuteState->setTrackSoloMuteState(audioTrackKey(first), { false, false });
    m_soloMuteState->setTrackSoloMuteState(audioTrackKey(second), { true, false });
    m_controller->updateSoloMuteStates(m_notation, false);
    EXPECT_FALSE(m_controlParams[42].muted);
    EXPECT_TRUE(m_controlParams[43].muted);
}

TEST_F(Arrangement_ControllerTests, MuteAllSilencesAudioTracksForAPartExport)
{
    setPlaybackInited(true);
    addWholeFileTrack(u"First");
    resolveAdd(0, 42);

    m_controller->updateSoloMuteStates(m_notation, false);
    EXPECT_FALSE(m_controlParams[42].muted);

    // A part is exported: audio tracks belong to no part
    m_controller->updateSoloMuteStates(m_notation, false, true /*muteAll*/);
    EXPECT_TRUE(m_controlParams[42].muted);

    // Back to the full score
    m_controller->updateSoloMuteStates(m_notation, false);
    EXPECT_FALSE(m_controlParams[42].muted);
}

// --- Import -------------------------------------------------------------------------------------

TEST_F(Arrangement_ControllerTests, ImportAudioTrackPlacesTheWholeFileAtTheStart)
{
    setPlaybackInited(true);

    const ByteArray wav = makeSineWav(1.5, 8000, 1);
    const io::path_t file(m_dir.filePath("Vocals take 3.wav"));
    ASSERT_TRUE(writeFile(file, wav));

    EXPECT_CALL(*m_project, markAsUnsaved()).Times(::testing::AtLeast(1));

    bool done = false;
    Ret result;
    m_controller->importAudioTrack(file).onResolve(this, [&](const Ret& ret) {
        result = ret;
        done = true;
    });
    ASSERT_TRUE(waitFor([&done]() { return done; }));
    ASSERT_TRUE(result) << result.text();
    pump();

    ASSERT_EQ(m_arrangement->tracks().size(), 1u);
    const Track& track = m_arrangement->tracks().front();
    EXPECT_EQ(track.name, String(u"Vocals take 3"));
    EXPECT_EQ(track.timeBase, TimeBase::Musical);
    ASSERT_EQ(track.clips.size(), 1u);
    EXPECT_EQ(track.clips[0].startTick, Fraction(0, 1));
    EXPECT_NEAR(track.clips[0].length.raw(), 1.5, 1e-9);

    // ... and it goes to the engine as it is
    ASSERT_EQ(m_addCalls.size(), 1u);
    EXPECT_EQ(m_addCalls[0].name, "Vocals take 3");
    ASSERT_EQ(m_addCalls[0].sources.size(), 1u);
    EXPECT_EQ(m_addCalls[0].sources[0].bytes, wav);
    ASSERT_EQ(m_addCalls[0].clips.size(), 1u);
    EXPECT_NEAR(m_addCalls[0].clips[0].lengthSecs.raw(), 1.5, 1e-9);
}

TEST_F(Arrangement_ControllerTests, ImportOfAnUnreadableFileAddsNothing)
{
    const io::path_t file(m_dir.filePath("notes.wav"));
    ASSERT_TRUE(writeFile(file, ByteArray::fromQByteArray(QByteArray("this is not audio"))));

    bool done = false;
    Ret result;
    m_controller->importAudioTrack(file).onResolve(this, [&](const Ret& ret) {
        result = ret;
        done = true;
    });
    ASSERT_TRUE(waitFor([&done]() { return done; }));

    EXPECT_FALSE(result);
    EXPECT_FALSE(result.text().empty());
    EXPECT_TRUE(m_arrangement->tracks().empty());
}

// --- "Embed linked audio?" when a copy is saved -------------------------------------------------

TEST_F(Arrangement_ControllerTests, CopyWithoutLinkedAudioDoesNotAsk)
{
    addWholeFileTrack(); // embedded

    EXPECT_CALL(*m_interactive, questionSync(_, _, _, _, _, _)).Times(0);

    EXPECT_TRUE(m_controller->prepareProjectCopy());
    EXPECT_FALSE(m_controller->embedLinkedAudioInCopy());
}

TEST_F(Arrangement_ControllerTests, CopyWithLinkedAudioAsksAndRemembersWhenToldTo)
{
    ASSERT_TRUE(m_store->setStorage(m_asset, AssetStorage::Referenced));
    addWholeFileTrack();

    // The dialog's buttons: Cancel, Keep links, Embed (the default)
    int embedButton = 0;
    int keepButton = 0;
    int answer = 0;
    bool showAgain = true;
    ON_CALL(*m_interactive, buttonData(_)).WillByDefault([](IInteractive::Button button) {
        return IInteractive::ButtonData(static_cast<int>(button), "");
    });
    ON_CALL(*m_interactive, questionSync(_, _, _, _, _, _))
    .WillByDefault([&](const std::string&, const IInteractive::Text&, const IInteractive::ButtonDatas& buttons, int defBtn,
                       const IInteractive::Options&, const std::string&) {
        EXPECT_EQ(buttons.size(), 3u);
        keepButton = buttons[1].btn;
        embedButton = buttons[2].btn;
        EXPECT_EQ(defBtn, embedButton);
        const int button = answer == 0 ? static_cast<int>(IInteractive::Button::Cancel) : answer == 1 ? keepButton : embedButton;
        return IInteractive::Result(button, showAgain);
    });

    // Cancel: no copy is made
    answer = 0;
    Ret ret = m_controller->prepareProjectCopy();
    EXPECT_FALSE(ret);
    EXPECT_EQ(ret.code(), static_cast<int>(Ret::Code::Cancel));

    // Keep links
    answer = 1;
    EXPECT_CALL(*m_configuration, setLinkedAudioInCopies(_)).Times(0);
    EXPECT_TRUE(m_controller->prepareProjectCopy());
    EXPECT_FALSE(m_controller->embedLinkedAudioInCopy());
    ::testing::Mock::VerifyAndClearExpectations(m_configuration.get());

    // Embed, and do not ask again
    answer = 2;
    showAgain = false;
    EXPECT_CALL(*m_configuration, setLinkedAudioInCopies(LinkedAudioInCopies::Embed)).Times(1);
    EXPECT_TRUE(m_controller->prepareProjectCopy());
    EXPECT_TRUE(m_controller->embedLinkedAudioInCopy());
}

TEST_F(Arrangement_ControllerTests, CopyFollowsThePreferenceWithoutAsking)
{
    ASSERT_TRUE(m_store->setStorage(m_asset, AssetStorage::Referenced));
    addWholeFileTrack();

    EXPECT_CALL(*m_interactive, questionSync(_, _, _, _, _, _)).Times(0);

    ON_CALL(*m_configuration, linkedAudioInCopies()).WillByDefault(Return(LinkedAudioInCopies::Embed));
    EXPECT_TRUE(m_controller->prepareProjectCopy());
    EXPECT_TRUE(m_controller->embedLinkedAudioInCopy());

    ON_CALL(*m_configuration, linkedAudioInCopies()).WillByDefault(Return(LinkedAudioInCopies::KeepLinks));
    EXPECT_TRUE(m_controller->prepareProjectCopy());
    EXPECT_FALSE(m_controller->embedLinkedAudioInCopy());
}

TEST_F(Arrangement_ControllerTests, ARefusedTrackIsNotAddedAgainUntilItsFilesChange)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    ASSERT_EQ(m_addCalls.size(), 1u);

    m_addCalls[0].reject();
    pump();
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Offline);

    // Edits that keep the same files do not make the engine read and decode them again
    const ClipId clip = m_arrangement->track(id)->clips.front().id;
    ASSERT_TRUE(m_arrangement->setClipGain(id, clip, -3.f));
    ASSERT_TRUE(m_arrangement->addAudioTrack(u"Other").ret);
    pump();
    EXPECT_EQ(m_addCalls.size(), 1u);
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Offline);

    // Another file on the track: worth a new try, with both files
    const AssetId second = importSecondAsset();
    addClipOf(id, second, Fraction(2, 1), 1.0);
    ASSERT_EQ(m_addCalls.size(), 2u);
    EXPECT_EQ(m_addCalls[1].sources.size(), 2u);
    resolveAdd(1, 42);
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Playing);
}

TEST_F(Arrangement_ControllerTests, ARefusedTrackIsTriedAgainWhenTheSequenceIsSetUpAgain)
{
    setPlaybackInited(true);
    const ArrTrackId id = addWholeFileTrack();
    m_addCalls[0].reject();
    pump();
    EXPECT_EQ(m_controller->trackPlayState(id), TrackPlayState::Offline);

    // Another project or audio system
    setPlaybackInited(false);
    setPlaybackInited(true);
    EXPECT_EQ(m_addCalls.size(), 2u);
}

TEST_F(Arrangement_ControllerTests, AOneSampleMoveLateOnTheTimelineSendsAList)
{
    // secs_t's own == is relative (1e-7): at 600 s it would hide a move of less than about 3 samples
    RetVal<ArrTrackId> track = m_arrangement->addAudioTrack(u"Late", -1, TimeBase::Absolute);
    ASSERT_TRUE(track.ret);
    ModelClip clip;
    clip.asset = m_asset;
    clip.startSecs = 600.0;
    clip.sourceOffset = 0.5;
    clip.length = 1.0;
    RetVal<ClipId> added = m_arrangement->addClip(track.val, clip);
    ASSERT_TRUE(added.ret);
    pump();

    setPlaybackInited(true);
    ASSERT_EQ(m_addCalls.size(), 1u);
    resolveAdd(0, 42);

    // Trimming the start by one sample at 48 kHz moves the clip's start by as much
    const double oneSample = 1.0 / 48000.0;
    ASSERT_TRUE(m_arrangement->trimClip(track.val, added.val, 0.5 + oneSample, 1.0 - oneSample));
    pump();

    ASSERT_EQ(m_addCalls[0].updates.size(), 1u);
    ASSERT_EQ(m_addCalls[0].updates[0].size(), 1u);
    EXPECT_NEAR(m_addCalls[0].updates[0][0].startSecs.raw(), 600.0 + oneSample, 1e-9);
}

// --- Audio staves (Batch 2 S1) -----------------------------------------------------------------

namespace {
//! Drives the real score's undo commands, as NotationUndoStack does, without a notation.
class ScoreUndoStackFake : public notation::INotationUndoStack
{
public:
    explicit ScoreUndoStackFake(engraving::Score* score)
        : m_score(score) {}

    bool canUndo() const override { return false; }
    void undo(engraving::EditData*) override {}
    bool canRedo() const override { return false; }
    void redo(engraving::EditData*) override {}
    void undoRedoToIndex(size_t, engraving::EditData*) override {}
    void transaction(const TranslatableString&, std::function<void(engraving::Transaction&)>) override {}
    void prepareChanges(const TranslatableString& name) override { m_score->startCmd(name); ++prepared; }
    void rollbackChanges() override { m_score->endCmd(true); }
    void commitChanges() override { m_score->endCmd(); ++committed; }
    void mergeTransactions(const size_t) override {}
    bool isStackClean() const override { return true; }
    void lock() override {}
    void unlock() override {}
    bool isLocked() const override { return false; }
    const TranslatableString topMostUndoActionName() const override { return {}; }
    const TranslatableString topMostRedoActionName() const override { return {}; }
    size_t undoRedoActionCount() const override { return 0; }
    size_t currentStateIndex() const override { return 0; }
    const TranslatableString lastActionNameAtIdx(size_t) const override { return {}; }
    async::Notification stackChanged() const override { return {}; }
    async::Channel<engraving::ScoreChanges> changesChannel() const override { return {}; }
    async::Notification undoRedoNotification() const override { return {}; }

    int prepared = 0;
    int committed = 0;

private:
    engraving::Score* m_score = nullptr;
};
}

TEST_F(Arrangement_ControllerTests, TracksFromBeforeAudioStavesGetTheirPartWhenTheProjectOpens)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    m_controller->setAddAudioStavesOnOpen(true);
    const size_t partsBefore = score->parts().size();

    // Two tracks as a project from before S1 has them, and one whose part was deleted since
    const ArrTrackId first = m_arrangement->addAudioTrack(u"First take").val;
    const ArrTrackId second = m_arrangement->addAudioTrack(u"Second").val;
    const ArrTrackId deleted = m_arrangement->addAudioTrack(u"Deleted").val;
    ASSERT_TRUE(m_arrangement->setTrackPart(deleted, muse::ID(uint64_t(999999))));

    // Mixer settings and solo/mute saved before S1 are under the old key
    project::AudioOutputParams saved;
    saved.volume = volume_db_t::make(-6.f);
    m_audioSettings->setTrackOutputParams(audioTrackKey(first), saved, false);
    m_soloMuteState->setTrackSoloMuteState(audioTrackKey(first), { true /*mute*/, false });

    m_currentProjectChanged.notify();
    pump();

    // One part each for the first two, in track order, at the end of the score; one undo step each
    ASSERT_EQ(score->parts().size(), partsBefore + 2);
    const engraving::Part* p1 = score->parts()[partsBefore];
    const engraving::Part* p2 = score->parts()[partsBefore + 1];
    EXPECT_TRUE(engraving::isAudioPart(p1));
    EXPECT_TRUE(engraving::isAudioPart(p2));
    EXPECT_EQ(p1->longName(), u"First take");
    EXPECT_EQ(p2->longName(), u"Second");
    EXPECT_EQ(m_arrangement->track(first)->partId, p1->id());
    EXPECT_EQ(m_arrangement->track(second)->partId, p2->id());
    EXPECT_EQ(undo->committed, 1); // one undo step for all of them

    // ... and move to the part's own key, which the mixer now uses
    const engraving::InstrumentTrackId newKey { p1->id(), engraving::FONTDAW_AUDIO_INSTRUMENT_ID };
    EXPECT_FALSE(m_audioSettings->trackHasExistingOutputParams(audioTrackKey(first)));
    ASSERT_TRUE(m_audioSettings->trackHasExistingOutputParams(newKey));
    EXPECT_FLOAT_EQ(m_audioSettings->trackOutputParams(newKey).volume.raw(), -6.f);
    EXPECT_TRUE(m_soloMuteState->trackSoloMuteState(newKey).mute);
    EXPECT_FALSE(m_soloMuteState->trackSoloMuteStateExists(audioTrackKey(first)));

    // The track whose part was deleted keeps its id and gets no new part
    EXPECT_EQ(m_arrangement->track(deleted)->partId, muse::ID(uint64_t(999999)));

    // Opening again adds nothing
    m_currentProjectChanged.notify();
    pump();
    EXPECT_EQ(score->parts().size(), partsBefore + 2);

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

// Review of #167, finding 1: undoing the score step of an import leaves the track naming a part
// that is gone; the next new part gets the same id and must not end up shared
TEST_F(Arrangement_ControllerTests, AnImportAfterAnUndoneOneDoesNotShareItsPart)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    m_controller->setAddAudioStavesOnOpen(true);

    const ArrTrackId first = m_arrangement->addAudioTrack(u"First").val;
    m_currentProjectChanged.notify();
    pump();
    const muse::ID firstPart = m_arrangement->track(first)->partId;
    ASSERT_TRUE(firstPart.isValid());

    // Ctrl+Z: the part goes, the track stays and still names it
    engraving::EditData ed;
    score->undoRedo(true, &ed);
    ASSERT_FALSE(score->partById(firstPart));
    EXPECT_EQ(m_arrangement->track(first)->partId, firstPart);

    // A second track gets a part: the freed id comes back
    const ArrTrackId second = m_arrangement->addAudioTrack(u"Second").val;
    m_currentProjectChanged.notify();
    pump();
    const muse::ID secondPart = m_arrangement->track(second)->partId;
    ASSERT_TRUE(secondPart.isValid());
    EXPECT_EQ(secondPart, firstPart); // the case to guard against

    // ... and the first track no longer names it
    EXPECT_FALSE(m_arrangement->track(first)->partId.isValid());
    const std::vector<const engraving::Part*> parts = resolveAudioParts(score, m_arrangement->tracks());
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], nullptr);
    EXPECT_EQ(parts[1], score->partById(secondPart));

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, AFileWhereTwoTracksNameOnePartGivesItToTheFirstOnly)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    score->startCmd(TranslatableString::untranslatable("add"));
    engraving::Part* part = addAudioPart(score, u"Take");
    score->endCmd();

    TrackList tracks(2);
    tracks[0].id = 1;
    tracks[0].partId = part->id();
    tracks[1].id = 2;
    tracks[1].partId = part->id();
    const std::vector<const engraving::Part*> parts = resolveAudioParts(score, tracks);
    EXPECT_EQ(parts[0], part);
    EXPECT_EQ(parts[1], nullptr);
    delete score;
}

TEST_F(Arrangement_ControllerTests, NoAudioStavesAreAddedOnOpenOutsideTheApp)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    const size_t partsBefore = score->parts().size();

    m_controller->setAddAudioStavesOnOpen(false); // as in a command-line conversion
    const ArrTrackId id = m_arrangement->addAudioTrack(u"Take").val;
    m_currentProjectChanged.notify();
    pump();
    EXPECT_EQ(score->parts().size(), partsBefore);
    EXPECT_FALSE(m_arrangement->track(id)->partId.isValid());

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, WithoutAScoreNoPartIsAddedAndNothingBreaks)
{
    // The default fixture has no score (masterScore() is null)
    const ArrTrackId id = m_arrangement->addAudioTrack(u"Take").val;
    m_currentProjectChanged.notify();
    pump();
    EXPECT_FALSE(m_arrangement->track(id)->partId.isValid());
}

// --- Batch 2 03a, step 5: the controller and the undo history ---------------------------------

TEST_F(Arrangement_ControllerTests, AnImportIsOneStepWithItsAudioPart)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    m_currentProjectChanged.notify();
    pump();
    const size_t partsBefore = score->parts().size();
    const size_t stepsBefore = score->undoStack()->currentIndex();

    bool done = false;
    Ret result;
    m_controller->importAudioTrack(m_wavPath).onResolve(this, [&](const Ret& ret) {
        result = ret;
        done = true;
    });
    ASSERT_TRUE(waitFor([&done]() { return done; }));
    ASSERT_TRUE(result) << result.text();
    pump();

    ASSERT_EQ(m_arrangement->tracks().size(), 1u);
    const muse::ID partId = m_arrangement->tracks().front().partId;
    EXPECT_TRUE(score->partById(partId));
    EXPECT_EQ(score->parts().size(), partsBefore + 1);
    EXPECT_EQ(score->undoStack()->currentIndex(), stepsBefore + 1) << "one step for track, clip and part";
    EXPECT_EQ(undo->prepared, 1);
    EXPECT_EQ(undo->committed, 1);

    // One Ctrl+Z removes all three, one Ctrl+Y brings them back with the same part
    engraving::EditData ed;
    score->undoRedo(true, &ed);
    pump();
    EXPECT_TRUE(m_arrangement->tracks().empty());
    EXPECT_EQ(score->parts().size(), partsBefore);

    score->undoRedo(false, &ed);
    pump();
    ASSERT_EQ(m_arrangement->tracks().size(), 1u);
    EXPECT_EQ(m_arrangement->tracks().front().clips.size(), 1u);
    EXPECT_EQ(m_arrangement->tracks().front().partId, partId);
    EXPECT_TRUE(score->partById(partId));

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, RecordedEditsAndTheirUndoDoNotMarkTheProjectUnsaved)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    const ArrTrackId track = addWholeFileTrack();
    m_currentProjectChanged.notify();
    pump();

    // "Modified" comes from the undo stack for these (the project listens to it)
    EXPECT_CALL(*m_project, markAsUnsaved()).Times(0);
    ASSERT_TRUE(m_arrangementUndo->edit(TranslatableString::untranslatable("rename"),
                                        [track](IArrangement& a) { return a.renameTrack(track, u"Renamed"); }));
    pump();
    engraving::EditData ed;
    score->undoRedo(true, &ed);
    pump();
    EXPECT_EQ(m_arrangement->track(track)->name, String(u"Guitar"));
    score->undoRedo(false, &ed);
    pump();
    ::testing::Mock::VerifyAndClearExpectations(m_project.get());

    // A change the stack does not see is marked
    EXPECT_CALL(*m_project, markAsUnsaved()).Times(::testing::AtLeast(1));
    ASSERT_TRUE(m_arrangement->setRecordArmed(track, true));
    pump();

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, UndoingARemovedTrackBringsBackItsMixerSettings)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    const ArrTrackId track = addWholeFileTrack();
    m_currentProjectChanged.notify();
    pump();

    const engraving::InstrumentTrackId key = audioTrackKey(track); // no part in this test
    project::AudioOutputParams params;
    params.volume = volume_db_t::make(-9.f);
    m_audioSettings->setTrackOutputParams(key, params, false);
    m_soloMuteState->setTrackSoloMuteState(key, { true /*mute*/, false /*solo*/ });

    ASSERT_TRUE(m_arrangementUndo->edit(TranslatableString::untranslatable("remove"),
                                        [track](IArrangement& a) { return a.removeTrack(track); }));
    pump();
    // What a save writes: nothing for the removed track
    EXPECT_FALSE(m_audioSettings->trackHasExistingOutputParams(key));
    EXPECT_FALSE(m_soloMuteState->trackSoloMuteStateExists(key));

    engraving::EditData ed;
    score->undoRedo(true, &ed);
    pump();
    ASSERT_TRUE(m_arrangement->track(track));
    ASSERT_TRUE(m_audioSettings->trackHasExistingOutputParams(key));
    EXPECT_FLOAT_EQ(m_audioSettings->trackOutputParams(key).volume.raw(), -9.f);
    ASSERT_TRUE(m_soloMuteState->trackSoloMuteStateExists(key));
    EXPECT_TRUE(m_soloMuteState->trackSoloMuteState(key).mute);

    // Redo removes them again; a second undo brings them back again
    score->undoRedo(false, &ed);
    pump();
    EXPECT_FALSE(m_audioSettings->trackHasExistingOutputParams(key));
    score->undoRedo(true, &ed);
    pump();
    EXPECT_FLOAT_EQ(m_audioSettings->trackOutputParams(key).volume.raw(), -9.f);

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, ParkedSettingsDoNotOutliveTheArrangementsEpoch)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto undo = std::make_shared<ScoreUndoStackFake>(score);
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(undo)));
    const ArrTrackId track = addWholeFileTrack();
    m_currentProjectChanged.notify();
    pump();
    const engraving::InstrumentTrackId key = audioTrackKey(track);
    project::AudioOutputParams params;
    params.volume = volume_db_t::make(-9.f);
    m_audioSettings->setTrackOutputParams(key, params, false);

    ASSERT_TRUE(m_arrangementUndo->edit(TranslatableString::untranslatable("remove"),
                                        [track](IArrangement& a) { return a.removeTrack(track); }));
    pump();
    m_arrangement->clear(); // another epoch: the old step can no longer bring the track back
    pump();
    engraving::EditData ed;
    score->undoRedo(true, &ed);
    pump();
    EXPECT_FALSE(m_arrangement->track(track));
    EXPECT_FALSE(m_audioSettings->trackHasExistingOutputParams(key));

    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}

TEST_F(Arrangement_ControllerTests, AnEditFromAPartViewUsesThatViewsUndoStack)
{
    engraving::MasterScore* score = engraving::ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(score);
    auto masterStack = std::make_shared<ScoreUndoStackFake>(score);
    auto partStack = std::make_shared<ScoreUndoStackFake>(score); // a part view's: the same score's stack
    auto partNotation = std::make_shared<NiceMock<notation::NotationMock> >();
    ON_CALL(*partNotation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(partStack)));
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(score));
    ON_CALL(*m_notation, undoStack()).WillByDefault(Return(notation::INotationUndoStackPtr(masterStack)));
    ON_CALL(*m_projectSession, masterNotation()).WillByDefault(Return(notation::IMasterNotationPtr(m_masterNotation)));
    ON_CALL(*m_projectSession, currentNotation()).WillByDefault(Return(notation::INotationPtr(partNotation)));
    const ArrTrackId track = addWholeFileTrack();
    m_currentProjectChanged.notify();
    pump();

    ASSERT_TRUE(m_arrangementUndo->edit(TranslatableString::untranslatable("rename"),
                                        [track](IArrangement& a) { return a.renameTrack(track, u"From the part"); }));
    EXPECT_EQ(partStack->prepared, 1);
    EXPECT_EQ(partStack->committed, 1);
    EXPECT_EQ(masterStack->prepared, 0);

    ON_CALL(*m_projectSession, currentNotation()).WillByDefault(Return(notation::INotationPtr()));
    ON_CALL(*m_masterNotation, masterScore()).WillByDefault(Return(nullptr));
    delete score;
}
