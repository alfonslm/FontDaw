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
#include <limits>
#include <algorithm>

#include <QTemporaryDir>

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "internal/arrangement.h"
#include "internal/arrangementjson.h"
#include "internal/audioassetstore.h"

#include "mocks/arrangementconfigurationmock.h"
#include "testutils.h"

using ::testing::NiceMock;
using ::testing::Return;

using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace muse;
using mu::engraving::Fraction;

static const std::string TEST_MODULE("arrangement_utests");
static const modularity::IoCID TEST_CONTEXT_ID = 2;

// At the default 120 BPM a quarter note is 0.5 s and a 4/4 bar is 2 s.
class Arrangement_ArrangementTests : public ::testing::Test, public async::Asyncable
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());

        m_configuration = std::make_shared<NiceMock<ArrangementConfigurationMock> >();
        ON_CALL(*m_configuration, defaultAssetStorage()).WillByDefault(Return(AssetStorage::Embedded));
        ON_CALL(*m_configuration, audioSessionsPath()).WillByDefault(Return(io::path_t(m_dir.filePath("sessions"))));
        modularity::globalIoc()->registerExport<IArrangementConfiguration>(TEST_MODULE, m_configuration);

        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        m_store = std::make_shared<AudioAssetStore>(m_iocCtx);
        modularity::ioc(m_iocCtx)->registerExport<IAudioAssetStore>(TEST_MODULE, m_store);

        m_arrangement = std::make_shared<Arrangement>(m_iocCtx);
        m_arrangement->changed().onReceive(this, [this](const Change& c) { m_changes.push_back(c); });

        // A 10 s mono asset
        const io::path_t file(m_dir.filePath("ten.wav"));
        ASSERT_TRUE(writeFile(file, makeSineWav(10.0, 8000, 1)));
        RetVal<AudioAsset> a = m_store->importFileSync(file, AssetStorage::Embedded);
        ASSERT_TRUE(a.ret) << a.ret.text();
        m_asset = a.val.id;
    }

    void TearDown() override
    {
        m_arrangement = nullptr;
        m_store = nullptr;
        modularity::globalIoc()->unregister<IArrangementConfiguration>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    AudioClip clipAt(const Fraction& tick, double length, double offset = 0.0) const
    {
        AudioClip c;
        c.asset = m_asset;
        c.startTick = tick;              // used on Musical tracks
        c.startSecs = tick.toDouble() * 2.0; // used on Absolute tracks (120 BPM)
        c.length = length;
        c.sourceOffset = offset;
        return c;
    }

    ArrTrackId addTrack(TimeBase timeBase = TimeBase::Musical)
    {
        RetVal<ArrTrackId> t = m_arrangement->addAudioTrack(u"Audio", -1, timeBase);
        EXPECT_TRUE(t.ret);
        return t.val;
    }

    ClipId addClip(ArrTrackId track, const Fraction& tick, double length, double offset = 0.0)
    {
        RetVal<ClipId> c = m_arrangement->addClip(track, clipAt(tick, length, offset));
        EXPECT_TRUE(c.ret) << c.ret.text();
        return c.val;
    }

    //! The invariants every edit must keep.
    void expectInvariants() const
    {
        for (const Track& t : m_arrangement->tracks()) {
            for (size_t i = 0; i < t.clips.size(); ++i) {
                const AudioClip& c = t.clips[i];
                const AudioAsset* a = m_store->asset(c.asset);
                ASSERT_TRUE(a);
                EXPECT_GE(c.sourceOffset.raw(), 0.0);
                EXPECT_GT(c.length.raw(), 0.0);
                EXPECT_LE(c.sourceOffset.raw() + c.length.raw(), a->duration().raw() + 1e-9);
                EXPECT_LE(c.fadeIn.length.raw() + c.fadeOut.length.raw(), c.length.raw() + 1e-9);
                if (i > 0) {
                    const AudioClip& prev = t.clips[i - 1];
                    EXPECT_LE(prev.startSecs.raw(), c.startSecs.raw()) << "clips not sorted";
                    EXPECT_LE(prev.endSecs().raw(), c.startSecs.raw() + 1e-9) << "clips overlap";
                }
            }
        }
    }

    const AudioClip* clip(ArrTrackId track, ClipId id) const
    {
        const Track* t = m_arrangement->track(track);
        for (const AudioClip& c : t->clips) {
            if (c.id == id) {
                return &c;
            }
        }
        return nullptr;
    }

    QTemporaryDir m_dir;
    std::shared_ptr<NiceMock<ArrangementConfigurationMock> > m_configuration;
    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<AudioAssetStore> m_store;
    std::shared_ptr<Arrangement> m_arrangement;
    AssetId m_asset;
    std::vector<Change> m_changes;
};

TEST_F(Arrangement_ArrangementTests, TracksAddRemoveMove)
{
    const ArrTrackId a = addTrack();
    const ArrTrackId b = addTrack();
    RetVal<ArrTrackId> first = m_arrangement->addAudioTrack(u"First", 0);
    ASSERT_TRUE(first.ret);

    ASSERT_EQ(m_arrangement->tracks().size(), 3u);
    EXPECT_EQ(m_arrangement->tracks()[0].id, first.val);
    EXPECT_NE(a, b);
    EXPECT_NE(a, INVALID_TRACK);

    ASSERT_TRUE(m_arrangement->moveTrack(first.val, 2));
    EXPECT_EQ(m_arrangement->tracks()[2].id, first.val);
    EXPECT_FALSE(m_arrangement->moveTrack(first.val, 3));

    ASSERT_TRUE(m_arrangement->renameTrack(a, u"Vocals"));
    EXPECT_EQ(m_arrangement->track(a)->name, String(u"Vocals"));

    ASSERT_TRUE(m_arrangement->setRecordArmed(a, true));
    ASSERT_TRUE(m_arrangement->setInputMonitoring(a, true));
    EXPECT_TRUE(m_arrangement->track(a)->recordArmed);

    ASSERT_TRUE(m_arrangement->removeTrack(b));
    EXPECT_FALSE(m_arrangement->track(b));
    EXPECT_FALSE(m_arrangement->removeTrack(b));

    // One Change per successful edit, none for failed ones
    EXPECT_EQ(m_changes.size(), 8u);
    EXPECT_EQ(m_changes.back().kind, ChangeKind::TrackRemoved);
    EXPECT_EQ(m_changes.back().trackId, b);
}

TEST_F(Arrangement_ArrangementTests, ClipsMustFitTheirAsset)
{
    const ArrTrackId t = addTrack();

    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(0, 1), 0.0)).ret);        // no length
    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(0, 1), 10.5)).ret);       // longer than the asset
    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(0, 1), 5.0, 6.0)).ret);   // window past the end
    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(0, 1), 1.0, -1.0)).ret);  // negative offset
    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(-1, 4), 1.0)).ret);       // before the start

    AudioClip unknown = clipAt(Fraction(0, 1), 1.0);
    unknown.asset = "no-such-asset";
    EXPECT_FALSE(m_arrangement->addClip(t, unknown).ret);

    AudioClip fades = clipAt(Fraction(0, 1), 1.0);
    fades.fadeIn.length = 0.6;
    fades.fadeOut.length = 0.6;
    EXPECT_FALSE(m_arrangement->addClip(t, fades).ret);

    EXPECT_FALSE(m_arrangement->addClip(12345, clipAt(Fraction(0, 1), 1.0)).ret);   // no such track

    EXPECT_TRUE(m_arrangement->track(t)->clips.empty());
    EXPECT_EQ(m_changes.size(), 1u); // only TrackAdded

    // The whole asset fits exactly
    EXPECT_TRUE(m_arrangement->addClip(t, clipAt(Fraction(0, 1), 10.0)).ret);
}

TEST_F(Arrangement_ArrangementTests, ClipsStaySortedAndNeverOverlap)
{
    const ArrTrackId t = addTrack();

    const ClipId bar3 = addClip(t, Fraction(2, 1), 1.0);   // 4.0 - 5.0 s
    const ClipId bar1 = addClip(t, Fraction(0, 1), 2.0);   // 0.0 - 2.0 s
    const ClipId touch = addClip(t, Fraction(1, 1), 2.0);  // 2.0 - 4.0 s: touches both neighbours
    expectInvariants();

    const Track* track = m_arrangement->track(t);
    ASSERT_EQ(track->clips.size(), 3u);
    EXPECT_EQ(track->clips[0].id, bar1);
    EXPECT_EQ(track->clips[1].id, touch);
    EXPECT_EQ(track->clips[2].id, bar3);
    EXPECT_DOUBLE_EQ(track->clips[2].startSecs.raw(), 4.0);

    // Overlapping additions and moves are refused and change nothing
    EXPECT_FALSE(m_arrangement->addClip(t, clipAt(Fraction(1, 2), 0.5)).ret);
    EXPECT_FALSE(m_arrangement->moveClip(t, bar3, t, Fraction(3, 4)));
    EXPECT_DOUBLE_EQ(clip(t, bar3)->startSecs.raw(), 4.0);

    // A move that fits re-sorts
    ASSERT_TRUE(m_arrangement->moveClip(t, bar1, t, Fraction(5, 1)));
    EXPECT_EQ(m_arrangement->track(t)->clips.back().id, bar1);
    EXPECT_DOUBLE_EQ(clip(t, bar1)->startSecs.raw(), 10.0);
    expectInvariants();

    // Ids are never reused
    ASSERT_TRUE(m_arrangement->removeClip(t, touch));
    const ClipId next = addClip(t, Fraction(1, 1), 1.0);
    EXPECT_GT(next, bar3);
    EXPECT_NE(next, touch);
}

TEST_F(Arrangement_ArrangementTests, MoveToAnotherTrack)
{
    const ArrTrackId musical = addTrack(TimeBase::Musical);
    const ArrTrackId absolute = addTrack(TimeBase::Absolute);
    const ClipId c = addClip(musical, Fraction(1, 1), 1.0);
    addClip(absolute, Fraction(0, 1), 1.0);
    m_changes.clear();

    // Refused when it would overlap on the target
    EXPECT_FALSE(m_arrangement->moveClip(musical, c, absolute, Fraction(1, 8)));
    EXPECT_EQ(m_arrangement->track(musical)->clips.size(), 1u);

    ASSERT_TRUE(m_arrangement->moveClip(musical, c, absolute, Fraction(3, 1)));
    EXPECT_TRUE(m_arrangement->track(musical)->clips.empty());
    ASSERT_TRUE(clip(absolute, c));
    EXPECT_DOUBLE_EQ(clip(absolute, c)->startSecs.raw(), 6.0);

    // Both tracks report a change
    ASSERT_EQ(m_changes.size(), 2u);
    EXPECT_EQ(m_changes[0].trackId, musical);
    EXPECT_EQ(m_changes[1].trackId, absolute);
    expectInvariants();
}

TEST_F(Arrangement_ArrangementTests, TrimKeepsTheAudioInPlace)
{
    const ArrTrackId t = addTrack();
    const ClipId c = addClip(t, Fraction(1, 1), 4.0, 1.0); // timeline 2..6 s plays source 1..5 s

    // Right edge: shorter
    ASSERT_TRUE(m_arrangement->trimClip(t, c, 1.0, 3.0));
    EXPECT_DOUBLE_EQ(clip(t, c)->startSecs.raw(), 2.0);
    EXPECT_DOUBLE_EQ(clip(t, c)->endSecs().raw(), 5.0);

    // Left edge: half a second later. The clip starts later by the same amount, the end stays.
    ASSERT_TRUE(m_arrangement->trimClip(t, c, 1.5, 2.5));
    const AudioClip* trimmed = clip(t, c);
    EXPECT_NEAR(trimmed->startSecs.raw(), 2.5, 1e-9);
    EXPECT_EQ(trimmed->startTick, Fraction(5, 4));
    EXPECT_NEAR(trimmed->sourceOffset.raw(), 1.5, 1e-9);
    EXPECT_NEAR(trimmed->endSecs().raw(), 5.0, 1e-9);
    // Timeline position minus source offset is constant: the audio did not move.
    EXPECT_NEAR(trimmed->startSecs.raw() - trimmed->sourceOffset.raw(), 1.0, 1e-9);

    // Left edge back to before the audio starts: refused
    EXPECT_FALSE(m_arrangement->trimClip(t, c, -0.5, 4.0));
    // Past the end of the asset: refused
    EXPECT_FALSE(m_arrangement->trimClip(t, c, 1.5, 9.0));
    expectInvariants();
}

TEST_F(Arrangement_ArrangementTests, SplitMakesTwoClipsThatPlayTheSameAudio)
{
    const ArrTrackId t = addTrack();
    AudioClip original = clipAt(Fraction(1, 1), 4.0, 1.0); // 2..6 s, source 1..5 s
    original.fadeIn = { 0.1, FadeCurve::Linear };
    original.fadeOut = { 0.2, FadeCurve::EqualPower };
    original.gainDb = -6.f;
    RetVal<ClipId> left = m_arrangement->addClip(t, original);
    ASSERT_TRUE(left.ret);

    // Outside the clip or on its edge: refused
    EXPECT_FALSE(m_arrangement->splitClip(t, left.val, Fraction(1, 1)).ret);
    EXPECT_FALSE(m_arrangement->splitClip(t, left.val, Fraction(4, 1)).ret);

    RetVal<ClipId> right = m_arrangement->splitClip(t, left.val, Fraction(2, 1)); // at 4 s
    ASSERT_TRUE(right.ret);
    EXPECT_NE(right.val, left.val);

    const AudioClip* l = clip(t, left.val);
    const AudioClip* r = clip(t, right.val);
    ASSERT_TRUE(l && r);

    EXPECT_DOUBLE_EQ(l->startSecs.raw(), 2.0);
    EXPECT_DOUBLE_EQ(l->length.raw(), 2.0);
    EXPECT_DOUBLE_EQ(l->sourceOffset.raw(), 1.0);
    EXPECT_EQ(r->startTick, Fraction(2, 1));
    EXPECT_DOUBLE_EQ(r->startSecs.raw(), 4.0);
    EXPECT_DOUBLE_EQ(r->length.raw(), 2.0);
    EXPECT_DOUBLE_EQ(r->sourceOffset.raw(), 3.0);

    // Outer fades stay, the cut gets short fades; gain carries over
    EXPECT_DOUBLE_EQ(l->fadeIn.length.raw(), 0.1);
    EXPECT_DOUBLE_EQ(r->fadeOut.length.raw(), 0.2);
    EXPECT_EQ(r->fadeOut.curve, FadeCurve::EqualPower);
    EXPECT_DOUBLE_EQ(l->fadeOut.length.raw(), Arrangement::SPLIT_FADE_SECS);
    EXPECT_DOUBLE_EQ(r->fadeIn.length.raw(), Arrangement::SPLIT_FADE_SECS);
    EXPECT_FLOAT_EQ(r->gainDb, -6.f);

    ASSERT_EQ(m_arrangement->track(t)->clips.size(), 2u);
    EXPECT_EQ(m_arrangement->track(t)->clips[0].id, left.val);
    expectInvariants();
}

TEST_F(Arrangement_ArrangementTests, GainFadesMute)
{
    const ArrTrackId t = addTrack();
    const ClipId c = addClip(t, Fraction(0, 1), 1.0);

    ASSERT_TRUE(m_arrangement->setClipGain(t, c, -12.f));
    EXPECT_FLOAT_EQ(clip(t, c)->gainDb, -12.f);
    EXPECT_FALSE(m_arrangement->setClipGain(t, c, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(clip(t, c)->gainDb, -12.f);

    ASSERT_TRUE(m_arrangement->setClipFades(t, c, { 0.3, FadeCurve::Linear }, { 0.7, FadeCurve::EqualPower }));
    EXPECT_FALSE(m_arrangement->setClipFades(t, c, { 0.5, FadeCurve::Linear }, { 0.6, FadeCurve::Linear }));
    EXPECT_FALSE(m_arrangement->setClipFades(t, c, { -0.1, FadeCurve::Linear }, { 0.1, FadeCurve::Linear }));
    EXPECT_DOUBLE_EQ(clip(t, c)->fadeOut.length.raw(), 0.7);

    ASSERT_TRUE(m_arrangement->setClipMuted(t, c, true));
    EXPECT_TRUE(clip(t, c)->muted);

    // Trimming shorter than the fades shortens the fades with it
    ASSERT_TRUE(m_arrangement->trimClip(t, c, 0.0, 0.5));
    expectInvariants();
}

TEST_F(Arrangement_ArrangementTests, TempoChangeKeepsMusicalClipsOnTheirBar)
{
    const ArrTrackId musical = addTrack(TimeBase::Musical);
    const ArrTrackId absolute = addTrack(TimeBase::Absolute);
    const ClipId onBar3 = addClip(musical, Fraction(2, 1), 1.0); // bar 3 at 120 BPM = 4 s
    const ClipId at4s = addClip(absolute, Fraction(2, 1), 1.0);  // 4 s, wherever the bars go

    // A tempo map with a change: 120 BPM for the first bar (2 s), then 60 BPM (4 s per bar).
    TimeMap map;
    map.tickToSecs = [](const Fraction& f) {
        const double wholes = f.toDouble();
        return secs_t(wholes <= 1.0 ? wholes * 2.0 : 2.0 + (wholes - 1.0) * 4.0);
    };
    map.secsToTick = [](secs_t s) {
        const double secs = s.raw();
        const double wholes = secs <= 2.0 ? secs / 2.0 : 1.0 + (secs - 2.0) / 4.0;
        return Fraction::fromTicks(static_cast<int>(std::lround(wholes * mu::engraving::Constants::DIVISION * 4)));
    };
    m_changes.clear();
    m_arrangement->setTimeMap(map);

    // Bar 3 now starts at 2 + 4 = 6 s; the musical clip follows it.
    EXPECT_EQ(clip(musical, onBar3)->startTick, Fraction(2, 1));
    EXPECT_DOUBLE_EQ(clip(musical, onBar3)->startSecs.raw(), 6.0);

    // The absolute clip stays at 4 s, which is now half-way through bar 2.
    EXPECT_DOUBLE_EQ(clip(absolute, at4s)->startSecs.raw(), 4.0);
    EXPECT_EQ(clip(absolute, at4s)->startTick, Fraction(3, 2));

    ASSERT_EQ(m_changes.size(), 1u);
    EXPECT_EQ(m_changes[0].kind, ChangeKind::TracksReset);
}

TEST_F(Arrangement_ArrangementTests, ProjectRoundTrip)
{
    const ArrTrackId vocals = addTrack(TimeBase::Musical);
    const ArrTrackId fx = addTrack(TimeBase::Absolute);
    ASSERT_TRUE(m_arrangement->renameTrack(vocals, u"Vocals"));
    const ClipId a = addClip(vocals, Fraction(5, 4), 2.0, 0.5);
    const ClipId b = addClip(vocals, Fraction(4, 1), 1.0);
    const ClipId c = addClip(fx, Fraction(1, 2), 3.0);
    ASSERT_TRUE(m_arrangement->setClipFades(vocals, a, { 0.01, FadeCurve::Linear }, { 0.05, FadeCurve::EqualPower }));

    FakeProjectFile project;
    ASSERT_TRUE(m_arrangement->write(project.writer()));
    ASSERT_EQ(project.entries.count(ARRANGEMENT_JSON_NAME), 1u);
    ASSERT_EQ(project.entries.count("audio/" + m_asset + ".wav"), 1u);

    const TrackList before = m_arrangement->tracks();

    // Read into a fresh arrangement + store (as when the project is reopened)
    auto ctx2 = std::make_shared<modularity::Context>(TEST_CONTEXT_ID + 10);
    auto store2 = std::make_shared<AudioAssetStore>(ctx2);
    modularity::ioc(ctx2)->registerExport<IAudioAssetStore>(TEST_MODULE, store2);
    Arrangement reopened(ctx2);
    ASSERT_TRUE(reopened.read(project.reader()));

    ASSERT_EQ(reopened.tracks().size(), 2u);
    for (size_t i = 0; i < before.size(); ++i) {
        const Track& x = before[i];
        const Track& y = reopened.tracks()[i];
        EXPECT_EQ(x.id, y.id);
        EXPECT_EQ(x.name, y.name);
        EXPECT_EQ(x.timeBase, y.timeBase);
        ASSERT_EQ(x.clips.size(), y.clips.size());
        for (size_t j = 0; j < x.clips.size(); ++j) {
            EXPECT_EQ(x.clips[j].id, y.clips[j].id);
            EXPECT_EQ(x.clips[j].startTick, y.clips[j].startTick);
            EXPECT_NEAR(x.clips[j].startSecs.raw(), y.clips[j].startSecs.raw(), arrangementjson::SECS_EPSILON);
            EXPECT_NEAR(x.clips[j].length.raw(), y.clips[j].length.raw(), arrangementjson::SECS_EPSILON);
            EXPECT_NEAR(x.clips[j].sourceOffset.raw(), y.clips[j].sourceOffset.raw(), arrangementjson::SECS_EPSILON);
            EXPECT_EQ(x.clips[j].fadeOut.curve, y.clips[j].fadeOut.curve);
        }
    }
    ASSERT_TRUE(store2->asset(m_asset));
    EXPECT_FALSE(store2->asset(m_asset)->offline);

    // New ids continue after the loaded ones
    RetVal<ArrTrackId> t = reopened.addAudioTrack(u"New");
    EXPECT_GT(t.val, std::max(vocals, fx));
    AudioClip more = clipAt(Fraction(10, 1), 1.0);
    RetVal<ClipId> d = reopened.addClip(t.val, more);
    ASSERT_TRUE(d.ret);
    EXPECT_GT(d.val, std::max({ a, b, c }));

    reopened.clear();
    modularity::removeIoC(ctx2);
}

TEST_F(Arrangement_ArrangementTests, PlainScoreHasNoArrangementEntries)
{
    // Nothing added: a saved project must not gain FontDaw entries.
    FakeProjectFile project;
    ASSERT_TRUE(m_arrangement->write(project.writer()));
    EXPECT_TRUE(project.entries.empty());

    // ... and a project without arrangement.json opens as an empty arrangement.
    addTrack();
    ASSERT_TRUE(m_arrangement->read(project.reader()));
    EXPECT_TRUE(m_arrangement->tracks().empty());
}

TEST_F(Arrangement_ArrangementTests, ReadRefusesNewerAndDamagedFiles)
{
    FakeProjectFile newer;
    newer.entries[ARRANGEMENT_JSON_NAME] = ByteArray(R"({ "version": 99, "tracks": [] })");
    Ret ret = m_arrangement->read(newer.reader());
    EXPECT_EQ(ret.code(), static_cast<int>(Ret::Code::NotSupported));
    EXPECT_FALSE(ret.text().empty());

    FakeProjectFile duplicateIds;
    duplicateIds.entries[ARRANGEMENT_JSON_NAME]
        = ByteArray(R"({ "version": 1, "tracks": [
        { "id": 1, "clips": [] }, { "id": 1, "clips": [] } ] })");
    EXPECT_FALSE(m_arrangement->read(duplicateIds.reader()));
    EXPECT_TRUE(m_arrangement->tracks().empty());
}

TEST_F(Arrangement_ArrangementTests, ClipsOfMissingAudioAreKept)
{
    // The asset table lists a referenced file that is gone: the clip stays (offline, silent).
    const std::string json
        =
            R"({ "version": 1,
        "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "storage": "referenced", "absolutePath": "/no/such/file.wav",
                      "sampleRate": 48000, "channels": 1, "frames": 480000 } ],
        "tracks": [ { "id": 1, "kind": "audio", "name": "T", "timeBase": "musical",
                      "clips": [ { "id": 2, "asset": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "startTick": "0/1", "length": 5 } ] } ] })";
    FakeProjectFile project;
    project.entries[ARRANGEMENT_JSON_NAME] = ByteArray(json.c_str());

    ASSERT_TRUE(m_arrangement->read(project.reader()));
    ASSERT_EQ(m_arrangement->tracks().size(), 1u);
    EXPECT_EQ(m_arrangement->tracks()[0].clips.size(), 1u);
    ASSERT_TRUE(m_store->asset("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    EXPECT_TRUE(m_store->asset("da39a3ee5e6b4b0d3255bfef95601890afd80709")->offline);

    // ... and is saved again unchanged.
    FakeProjectFile saved;
    ASSERT_TRUE(m_arrangement->write(saved.writer()));
    RetVal<ArrangementFileData> back = arrangementjson::read(saved.entries[ARRANGEMENT_JSON_NAME]);
    ASSERT_TRUE(back.ret);
    ASSERT_EQ(back.val.assets.size(), 1u);
    EXPECT_EQ(back.val.assets[0].absolutePath, io::path_t("/no/such/file.wav"));
}

// --- Review of PR #120 ---

TEST_F(Arrangement_ArrangementTests, GainAndFadesStillEditableWhenATempoChangeMadeClipsOverlap)
{
    const ArrTrackId t = addTrack(TimeBase::Musical);
    const ClipId first = addClip(t, Fraction(0, 1), 2.0);  // 0..2 s at 120 BPM
    const ClipId second = addClip(t, Fraction(1, 1), 2.0); // 2..4 s

    // Twice as fast: bar 2 starts at 1 s, the first clip (2 s of audio) now runs into the second.
    m_arrangement->setTimeMap(TimeMap::constantTempo(240.0));
    ASSERT_EQ(m_arrangement->track(t)->clips.size(), 2u);
    ASSERT_GT(clip(t, first)->endSecs().raw(), clip(t, second)->startSecs.raw());

    EXPECT_TRUE(m_arrangement->setClipGain(t, first, -6.f));
    EXPECT_TRUE(m_arrangement->setClipFades(t, second, { 0.1, FadeCurve::Linear }, { 0.1, FadeCurve::Linear }));
    EXPECT_TRUE(m_arrangement->setClipMuted(t, first, true));
    EXPECT_FLOAT_EQ(clip(t, first)->gainDb, -6.f);

    // Edits that move or resize still refuse to make (or keep) an overlap.
    EXPECT_FALSE(m_arrangement->trimClip(t, first, 0.0, 1.5));
    ASSERT_TRUE(m_arrangement->trimClip(t, first, 0.0, 1.0)); // shortened until it fits
}

TEST_F(Arrangement_ArrangementTests, LoadedClipsAreKeptInsideTheirAudio)
{
    // The asset from SetUp is 10 s. Save a project, then make a clip longer than its audio.
    const ArrTrackId t = addTrack();
    addClip(t, Fraction(0, 1), 4.0);
    FakeProjectFile project;
    ASSERT_TRUE(m_arrangement->write(project.writer()));

    std::string json(project.entries[ARRANGEMENT_JSON_NAME].constChar(), project.entries[ARRANGEMENT_JSON_NAME].size());
    const std::string from = "\"length\": 4";
    const size_t at = json.find(from);
    ASSERT_NE(at, std::string::npos) << json;

    project.entries[ARRANGEMENT_JSON_NAME] = ByteArray(json.replace(at, from.size(), "\"length\": 25").c_str());
    ASSERT_TRUE(m_arrangement->read(project.reader()));
    EXPECT_NEAR(m_arrangement->tracks()[0].clips[0].length.raw(), 10.0, 1e-9);
    expectInvariants();

    // A clip that starts after the end of its audio is refused.
    json = std::string(project.entries[ARRANGEMENT_JSON_NAME].constChar(), project.entries[ARRANGEMENT_JSON_NAME].size());
    const std::string offset = "\"sourceOffset\": 0";
    const size_t at2 = json.find(offset);
    ASSERT_NE(at2, std::string::npos);
    project.entries[ARRANGEMENT_JSON_NAME] = ByteArray(json.replace(at2, offset.size(), "\"sourceOffset\": 12").c_str());
    EXPECT_FALSE(m_arrangement->read(project.reader()));
    EXPECT_TRUE(m_arrangement->tracks().empty());
}

// --- C2: the arrangement belongs to one project at a time; copies can embed linked audio ---

TEST_F(Arrangement_ArrangementTests, OwnerIsAnOpaqueTokenThatSurvivesEdits)
{
    EXPECT_EQ(m_arrangement->owner(), nullptr);

    int project = 0;
    m_arrangement->setOwner(&project);
    EXPECT_EQ(m_arrangement->owner(), &project);

    ASSERT_TRUE(m_arrangement->addAudioTrack(u"Vocals").ret);
    m_arrangement->clear();
    EXPECT_EQ(m_arrangement->owner(), &project); // clearing empties it; it does not change hands

    m_arrangement->setOwner(nullptr);
    EXPECT_EQ(m_arrangement->owner(), nullptr);
}

TEST_F(Arrangement_ArrangementTests, WriteOptionsReachTheAssetTable)
{
    // A linked asset on a track
    const io::path_t file(m_dir.filePath("linked.wav"));
    const ByteArray wav = makeSineWav(1.0, 8000, 1, 330.0);
    ASSERT_TRUE(writeFile(file, wav));
    RetVal<AudioAsset> linked = m_store->importFileSync(file, AssetStorage::Referenced);
    ASSERT_TRUE(linked.ret);

    const ArrTrackId track = m_arrangement->addAudioTrack(u"Linked").val;
    AudioClip clip;
    clip.asset = linked.val.id;
    clip.length = 1.0;
    ASSERT_TRUE(m_arrangement->addClip(track, clip).ret);

    // An ordinary save keeps the link: no audio entry
    FakeProjectFile saved;
    ASSERT_TRUE(m_arrangement->write(saved.writer()));
    EXPECT_EQ(saved.entries.size(), 1u);
    EXPECT_EQ(saved.entries.count(ARRANGEMENT_JSON_NAME), 1u);

    // A copy with "embed": the audio is in the copy and arrangement.json says embedded
    WriteOptions embed;
    embed.embedReferenced = true;
    FakeProjectFile copy;
    ASSERT_TRUE(m_arrangement->write(copy.writer(), embed));
    ASSERT_EQ(copy.entries.count(embeddedFileName(linked.val.id, "wav")), 1u);
    EXPECT_EQ(copy.entries[embeddedFileName(linked.val.id, "wav")], wav);

    RetVal<ArrangementFileData> data = arrangementjson::read(copy.entries[ARRANGEMENT_JSON_NAME]);
    ASSERT_TRUE(data.ret);
    ASSERT_EQ(data.val.assets.size(), 1u);
    EXPECT_EQ(data.val.assets[0].storage, AssetStorage::Embedded);
}
