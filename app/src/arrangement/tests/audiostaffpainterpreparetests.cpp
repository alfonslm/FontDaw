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

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "modularity/ioc.h"

#include "internal/arrangement.h"
#include "internal/audiostaffpainter.h"
#include "internal/waveformpeaks.h"

using namespace mu::arrangement;
using namespace muse;

namespace {
using Clock = std::chrono::steady_clock;

const std::string TEST_MODULE("arrangement_painter_utests");
const modularity::IoCID TEST_CONTEXT_ID = 5;

int msSince(const Clock::time_point& start)
{
    return int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

//! An asset store with one 10 s asset, whose peaks come only when the test says so
class FakeAssetStore : public IAudioAssetStore
{
public:
    FakeAssetStore()
    {
        m_asset.id = "fake-asset";
        m_asset.name = u"Fake";
        m_asset.format = "wav";
        m_asset.sampleRate = 8000;
        m_asset.channels = 1;
        m_asset.frames = 80000;
    }

    async::Promise<RetVal<AudioAsset> > importFile(const io::path_t&) override
    {
        return async::make_promise<RetVal<AudioAsset> >([](auto resolve) { return resolve(RetVal<AudioAsset>()); });
    }

    async::Promise<RetVal<AudioAsset> > importFile(const io::path_t&, AssetStorage) override
    {
        return async::make_promise<RetVal<AudioAsset> >([](auto resolve) { return resolve(RetVal<AudioAsset>()); });
    }

    const AudioAsset* asset(const AssetId& id) const override { return id == m_asset.id ? &m_asset : nullptr; }
    AudioAssetList assets() const override { return { m_asset }; }
    Ret setStorage(const AssetId&, AssetStorage) override { return make_ok(); }
    std::shared_ptr<const WaveformPeaks> peaks(const AssetId&) const override { return m_peaks; }
    async::Channel<AssetId> peaksReady() const override { return m_peaksReady; }
    io::path_t assetFilePath(const AssetId&) const override { return {}; }
    void removeUnused(const std::vector<AssetId>&) override {}
    void setProjectPath(const io::path_t&) override {}
    Ret load(const AudioAssetList&, const ReadProjectEntry&) override { return make_ok(); }
    RetVal<AudioAssetList> save(const WriteProjectEntry&, const std::set<AssetId>&, const WriteOptions&) const override
    {
        return RetVal<AudioAssetList>::make_ok({});
    }

    void clear() override {}

    AudioAsset m_asset;
    std::shared_ptr<const WaveformPeaks> m_peaks;   // null: still being built, and never will be
    async::Channel<AssetId> m_peaksReady;
};

//! A controller that is loaded, or never gets there
class FakeController : public IArrangementController
{
public:
    std::vector<AudioTrack> audioTracks() const override { return {}; }
    async::Channel<audio::TrackId> trackAdded() const override { return {}; }
    async::Channel<audio::TrackId> trackRemoved() const override { return {}; }
    TrackPlayState trackPlayState(ArrTrackId) const override { return TrackPlayState::Empty; }
    bool isLoaded() const override { return m_loaded; }
    async::Notification isLoadedChanged() const override { return {}; }
    audio::secs_t contentEnd() const override { return audio::secs_t(0.0); }
    async::Notification contentEndChanged() const override { return {}; }
    bool hasSoloTrack(const mu::notation::INotationPtr&) const override { return false; }
    void updateSoloMuteStates(const mu::notation::INotationPtr&, bool, bool) override {}
    async::Promise<Ret> importAudioTrack(const io::path_t&) override
    {
        return async::make_promise<Ret>([](auto resolve) { return resolve(make_ok()); });
    }

    Ret prepareProjectCopy() override { return make_ok(); }
    bool embedLinkedAudioInCopy() const override { return false; }

    bool m_loaded = true;
};
}

// Review of #196: prepareForOutput waits for the peaks and the audio tracks, once; what it gave up
// on is not waited for again until the arrangement changes. Off the GUI thread it does not wait.
class Arrangement_AudioStaffPainterPrepareTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        m_store = std::make_shared<FakeAssetStore>();
        m_controller = std::make_shared<FakeController>();
        modularity::ioc(m_iocCtx)->registerExport<IAudioAssetStore>(TEST_MODULE, m_store);
        m_arrangement = std::make_shared<Arrangement>(m_iocCtx);
        modularity::ioc(m_iocCtx)->registerExport<IArrangement>(TEST_MODULE, m_arrangement);
        modularity::ioc(m_iocCtx)->registerExport<IArrangementController>(TEST_MODULE, m_controller);

        const RetVal<ArrTrackId> track = m_arrangement->addAudioTrack(u"Take");
        ASSERT_TRUE(track.ret);
        m_track = track.val;
    }

    void TearDown() override
    {
        m_painter.reset();
        m_arrangement.reset();
        modularity::removeIoC(m_iocCtx);
    }

    //! A clip on the track; its asset is online and its peaks never come
    void addClip()
    {
        AudioClip clip;
        clip.asset = m_store->m_asset.id;
        clip.name = u"Clip";
        clip.length = 4.0;
        ASSERT_TRUE(m_arrangement->addClip(m_track, clip).ret);
    }

    //! The painter subscribes to the arrangement in init(), as the module does
    void makePainter()
    {
        m_painter = std::make_shared<AudioStaffPainter>(m_iocCtx);
        m_painter->init();
    }

    int prepareMs(int timeoutMs)
    {
        const Clock::time_point start = Clock::now();
        m_painter->prepareForOutput(timeoutMs);
        return msSince(start);
    }

    void expectAWait(int ms)
    {
        EXPECT_GE(ms, 150);
        EXPECT_LE(ms, 2000);
    }

    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<FakeAssetStore> m_store;
    std::shared_ptr<FakeController> m_controller;
    std::shared_ptr<Arrangement> m_arrangement;
    std::shared_ptr<AudioStaffPainter> m_painter;
    ArrTrackId m_track = INVALID_TRACK;
};

TEST_F(Arrangement_AudioStaffPainterPrepareTests, PeaksThatNeverComeAreWaitedForOnceUntilTheArrangementChanges)
{
    addClip();
    makePainter();

    expectAWait(prepareMs(200));
    EXPECT_LT(prepareMs(200), 50) << "the second call must not wait again";

    // A change of the arrangement makes it wait again, once
    ASSERT_TRUE(m_arrangement->renameTrack(m_track, u"Renamed"));
    expectAWait(prepareMs(200));
    EXPECT_LT(prepareMs(200), 50);
}

TEST_F(Arrangement_AudioStaffPainterPrepareTests, TracksThatNeverFinishLoadingAreWaitedForOnceUntilTheArrangementChanges)
{
    m_controller->m_loaded = false;
    makePainter();

    expectAWait(prepareMs(200));
    EXPECT_LT(prepareMs(200), 50);

    ASSERT_TRUE(m_arrangement->renameTrack(m_track, u"Renamed"));
    expectAWait(prepareMs(200));
    EXPECT_LT(prepareMs(200), 50);
}

TEST_F(Arrangement_AudioStaffPainterPrepareTests, WithPeaksAndLoadedTracksItReturnsAtOnce)
{
    addClip();
    m_store->m_peaks = std::make_shared<WaveformPeaks>();
    makePainter();

    EXPECT_LT(prepareMs(2000), 50);
}

TEST_F(Arrangement_AudioStaffPainterPrepareTests, AClipWithMissingAudioIsNotWaitedFor)
{
    addClip();
    m_store->m_asset.offline = true;
    makePainter();

    EXPECT_LT(prepareMs(2000), 50);
}

TEST_F(Arrangement_AudioStaffPainterPrepareTests, OffTheGuiThreadItReturnsAtOnce)
{
    addClip();
    m_controller->m_loaded = false;
    makePainter();

    std::atomic<int> ms { -1 };
    std::thread worker([this, &ms]() { ms = prepareMs(2000); });
    worker.join();
    EXPECT_GE(ms.load(), 0);
    EXPECT_LT(ms.load(), 50);
}
