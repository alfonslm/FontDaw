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

// Batch 2 03a: arrangement edits in the score's undo history. Each test names the step of
// docs/fontdaw/batches/batch-02/03a-undo-layer.md that fills it in; until then it is skipped, so
// the list of what will be checked is visible in every run.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transaction/undoablecommand.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/tests/utils/scorerw.h"

#include <QTemporaryDir>

#include <map>
#include <random>

#include "global/types/translatablestring.h"
#include "async/asyncable.h"
#include "modularity/ioc.h"

#include "internal/arrangement.h"
#include "internal/audioassetstore.h"
#include "internal/arrangementundo.h"

#include "mocks/arrangementconfigurationmock.h"
#include "testutils.h"

using namespace mu::engraving;


namespace {
//! Stands in for an arrangement command in step 1: it touches no score object, as the
//! arrangement's snapshot command does not. Counts its calls.
class NoScoreChangeCommand : public UndoableCommand
{
public:
    explicit NoScoreChangeCommand(int* calls)
        : m_calls(calls) {}
    void undo() override { ++*m_calls; }
    void redo() override { ++*m_calls; }
    const char* name() const override { return "NoScoreChange"; }

private:
    int* m_calls = nullptr;
};

//! FD-015 checks 1 and 2 (step 1), measured on a laid-out score.
//! "Lays nothing out": MasterScore::update() lays out only when the transaction's CmdState has a
//! layout range. The test disturbs a laid-out value (a segment's x) that any layout of the bar
//! recomputes, and checks that the value is still disturbed afterwards. The positive control
//! shows that a transaction asking for a layout does reset it.
class Arrangement_UndoStep1Tests : public ::testing::Test, public muse::async::Asyncable
{
protected:
    void SetUp() override
    {
        m_score = ScoreRW::readScore(u"data/systems_breaks.mscx");
        ASSERT_TRUE(m_score);
        m_score->doLayout();
        Measure* bar3 = m_score->firstMeasure()->nextMeasure()->nextMeasure();
        ASSERT_TRUE(bar3);
        m_segment = bar3->first(SegmentType::ChordRest);
        ASSERT_TRUE(m_segment);
        m_laidOutX = m_segment->ldata()->pos().x();

        m_score->changesChannel().onReceive(this, [this](const ScoreChanges& changes) {
            m_changes.push_back(changes);
        });
    }

    void TearDown() override
    {
        m_score->changesChannel().disconnect(this);
        delete m_score;
    }

    void disturbLayout() { m_segment->mutldata()->setPosX(m_laidOutX + DISTURBANCE); }
    bool layoutStillDisturbed() const { return m_segment->ldata()->pos().x() == m_laidOutX + DISTURBANCE; }

    TransactionManager* transactions() const { return m_score->transactionManager(); }

    void arrangementOnlyTransaction(const char* name)
    {
        transactions()->beginTransaction(muse::TranslatableString::untranslatable(name));
        transactions()->currentTransaction()->pushWithoutPerforming(new NoScoreChangeCommand(&m_commandCalls));
        transactions()->endTransaction();
    }

    static constexpr double DISTURBANCE = 1234.5;

    MasterScore* m_score = nullptr;
    Segment* m_segment = nullptr;
    double m_laidOutX = 0.0;
    int m_commandCalls = 0;
    std::vector<ScoreChanges> m_changes;
};

//! Step 2: writes down what the transaction manager tells it.
class RecordingObserver : public ITransactionObserver
{
public:
    std::vector<std::string> log;
    //! Set to push a command at transactionAboutToCommit (step 2: it joins the same undo step)
    int* pushCalls = nullptr;
    //! Set to remove itself at transactionClosed
    TransactionManager* removeFrom = nullptr;

    void transactionBegan() override { log.push_back("began"); }
    void transactionAboutToCommit(Transaction& transaction) override
    {
        log.push_back("commit");
        if (pushCalls) {
            transaction.pushWithoutPerforming(new NoScoreChangeCommand(pushCalls));
        }
    }
    void transactionClosed(bool committed) override
    {
        log.push_back(committed ? "closed:1" : "closed:0");
        if (removeFrom) {
            removeFrom->removeObserver(this);
        }
    }
};

class Arrangement_UndoStep2Tests : public Arrangement_UndoStep1Tests
{
protected:
    void SetUp() override
    {
        Arrangement_UndoStep1Tests::SetUp();
        transactions()->addObserver(&m_observer);
    }

    void TearDown() override
    {
        transactions()->removeObserver(&m_observer);
        Arrangement_UndoStep1Tests::TearDown();
    }

    RecordingObserver m_observer;
};
}


// --- Step 1: FD-015's checks 1 and 2, measured ---
TEST_F(Arrangement_UndoStep1Tests, TheDisturbanceIsResetByARealLayout)
{
    // Positive control: without it, "still disturbed" below would prove nothing
    disturbLayout();
    transactions()->beginTransaction(muse::TranslatableString::untranslatable("lay out"));
    m_score->setLayoutAll();
    transactions()->endTransaction();
    EXPECT_FALSE(layoutStillDisturbed());
    EXPECT_DOUBLE_EQ(m_segment->ldata()->pos().x(), m_laidOutX);
}

TEST_F(Arrangement_UndoStep1Tests, AnArrangementOnlyTransactionLaysNothingOut)
{
    disturbLayout();
    arrangementOnlyTransaction("Move clip");
    EXPECT_TRUE(layoutStillDisturbed());
    EXPECT_EQ(m_commandCalls, 0) << "pushed without performing: the model change is already made";
}

TEST_F(Arrangement_UndoStep1Tests, AnArrangementOnlyTransactionSendsNoScoreChangesToPlayback)
{
    arrangementOnlyTransaction("Move clip");
    ASSERT_EQ(m_changes.size(), 1u) << "a non-empty transaction sends its changes once";
    // PlaybackModel::shouldSkipChanges() skips changes that are not valid: nothing re-renders
    EXPECT_FALSE(m_changes.front().isValid());
    EXPECT_TRUE(m_changes.front().changedObjects.empty());
    EXPECT_TRUE(m_changes.front().changedTypes.empty());
}

TEST_F(Arrangement_UndoStep1Tests, AnArrangementOnlyTransactionIsNamedInTheHistory)
{
    const size_t before = m_score->undoStack()->size();
    arrangementOnlyTransaction("Move clip");
    ASSERT_EQ(m_score->undoStack()->size(), before + 1) << "an arrangement-only step is kept, not discarded as empty";
    ASSERT_TRUE(m_score->undoStack()->last());
    // The history panel and the Undo menu read this name (NotationUndoStack::topMostUndoActionName)
    EXPECT_EQ(m_score->undoStack()->last()->actionName().str, u"Move clip");
}

TEST_F(Arrangement_UndoStep1Tests, UndoAndRedoOfAnArrangementOnlyStepLayNothingOut)
{
    arrangementOnlyTransaction("Move clip");
    m_changes.clear();

    disturbLayout();
    transactions()->undoRedo(true, nullptr);
    EXPECT_EQ(m_commandCalls, 1) << "undo reached the command";
    EXPECT_TRUE(layoutStillDisturbed()) << "undo laid the score out";

    transactions()->undoRedo(false, nullptr);
    EXPECT_EQ(m_commandCalls, 2) << "redo reached the command";
    EXPECT_TRUE(layoutStillDisturbed()) << "redo laid the score out";

    ASSERT_EQ(m_changes.size(), 2u);
    EXPECT_FALSE(m_changes[0].isValid());
    EXPECT_FALSE(m_changes[1].isValid());
}

// --- Step 2: the transaction observer (engraving) ---
TEST_F(Arrangement_UndoStep2Tests, TheObserverHearsBeganAboutToCommitAndClosed)
{
    arrangementOnlyTransaction("Move clip");
    EXPECT_EQ(m_observer.log, (std::vector<std::string> { "began", "commit", "closed:1" }));
}

TEST_F(Arrangement_UndoStep2Tests, ARolledBackTransactionCallsOnlyClosed)
{
    transactions()->beginTransaction(muse::TranslatableString::untranslatable("Move clip"));
    transactions()->currentTransaction()->pushWithoutPerforming(new NoScoreChangeCommand(&m_commandCalls));
    transactions()->endTransaction(true);
    EXPECT_EQ(m_observer.log, (std::vector<std::string> { "began", "closed:0" }));
}

TEST_F(Arrangement_UndoStep2Tests, ReopenCallsNoBeganButClosed)
{
    arrangementOnlyTransaction("Move clip");
    m_observer.log.clear();

    transactions()->reopen();
    ASSERT_TRUE(transactions()->currentTransaction());
    transactions()->endTransaction();
    EXPECT_EQ(m_observer.log, (std::vector<std::string> { "commit", "closed:1" }));
}

TEST_F(Arrangement_UndoStep2Tests, UndoAndRedoCallNoObserver)
{
    arrangementOnlyTransaction("Move clip");
    m_observer.log.clear();

    transactions()->undoRedo(true, nullptr);
    transactions()->undoRedo(false, nullptr);
    EXPECT_TRUE(m_observer.log.empty());
    EXPECT_EQ(m_commandCalls, 2);
}

TEST_F(Arrangement_UndoStep2Tests, ACommandPushedAtAboutToCommitIsInTheSameUndoStep)
{
    int observerCalls = 0;
    m_observer.pushCalls = &observerCalls;

    const size_t before = m_score->undoStack()->size();
    arrangementOnlyTransaction("Move clip");
    ASSERT_EQ(m_score->undoStack()->size(), before + 1);
    ASSERT_TRUE(m_score->undoStack()->last());
    EXPECT_EQ(m_score->undoStack()->last()->commands().size(), 2u);

    transactions()->undoRedo(true, nullptr);
    EXPECT_EQ(m_commandCalls, 1);
    EXPECT_EQ(observerCalls, 1) << "one undo reached both commands";
    EXPECT_EQ(m_score->undoStack()->size(), before + 1);
}

TEST_F(Arrangement_UndoStep2Tests, AnObserverThatRemovesItselfInClosedIsNotCalledAgain)
{
    m_observer.removeFrom = transactions();
    arrangementOnlyTransaction("First");
    EXPECT_EQ(m_observer.log, (std::vector<std::string> { "began", "commit", "closed:1" }));

    m_observer.log.clear();
    arrangementOnlyTransaction("Second");
    EXPECT_TRUE(m_observer.log.empty());
}

// --- Step 3: the model ---
namespace arr = mu::arrangement;

namespace {
//! Step 3: the model's side of undo (restoreTracks, epoch, duplicateClip, sameHistory).
class Arrangement_UndoStep3Tests : public ::testing::Test, public muse::async::Asyncable
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());

        m_configuration = std::make_shared<testing::NiceMock<arr::ArrangementConfigurationMock> >();
        ON_CALL(*m_configuration, defaultAssetStorage()).WillByDefault(testing::Return(arr::AssetStorage::Embedded));
        ON_CALL(*m_configuration, audioSessionsPath()).WillByDefault(testing::Return(muse::io::path_t(m_dir.filePath("sessions"))));
        muse::modularity::globalIoc()->registerExport<arr::IArrangementConfiguration>(TEST_MODULE, m_configuration);

        m_iocCtx = std::make_shared<muse::modularity::Context>(TEST_CONTEXT_ID);
        m_store = std::make_shared<arr::AudioAssetStore>(m_iocCtx);
        muse::modularity::ioc(m_iocCtx)->registerExport<arr::IAudioAssetStore>(TEST_MODULE, m_store);

        m_arrangement = std::make_shared<arr::Arrangement>(m_iocCtx);
        m_arrangement->changed().onReceive(this, [this](const arr::Change& c) { m_changes.push_back(c); });

        const muse::io::path_t file(m_dir.filePath("ten.wav"));
        ASSERT_TRUE(arr::tests::writeFile(file, arr::tests::makeSineWav(10.0, 8000, 1)));
        muse::RetVal<arr::AudioAsset> a = m_store->importFileSync(file, arr::AssetStorage::Embedded);
        ASSERT_TRUE(a.ret) << a.ret.text();
        m_asset = a.val.id;
    }

    void TearDown() override
    {
        m_arrangement->changed().disconnect(this);
        m_arrangement = nullptr;
        m_store = nullptr;
        muse::modularity::globalIoc()->unregister<arr::IArrangementConfiguration>(TEST_MODULE);
        muse::modularity::removeIoC(m_iocCtx);
    }

    arr::AudioClip clipAt(const Fraction& tick, double length) const
    {
        arr::AudioClip c;
        c.asset = m_asset;
        c.startTick = tick;
        c.startSecs = tick.toDouble() * 2.0; // 120 BPM
        c.length = length;
        return c;
    }

    arr::ArrTrackId addTrack(arr::TimeBase timeBase = arr::TimeBase::Musical)
    {
        muse::RetVal<arr::ArrTrackId> t = m_arrangement->addAudioTrack(u"Audio", -1, timeBase);
        EXPECT_TRUE(t.ret);
        return t.val;
    }

    arr::ClipId addClip(arr::ArrTrackId track, const Fraction& tick, double length)
    {
        muse::RetVal<arr::ClipId> c = m_arrangement->addClip(track, clipAt(tick, length));
        EXPECT_TRUE(c.ret) << c.ret.text();
        return c.val;
    }

    //! A history-equal copy with one clip on one track, for sameHistory
    arr::TrackList oneClipList()
    {
        const arr::ArrTrackId t = addTrack();
        addClip(t, Fraction(0, 1), 1.0);
        return m_arrangement->tracks();
    }

    static constexpr const char* TEST_MODULE = "arrangement_undo_utests";
    static constexpr int TEST_CONTEXT_ID = 4;

    QTemporaryDir m_dir;
    std::shared_ptr<testing::NiceMock<arr::ArrangementConfigurationMock> > m_configuration;
    muse::modularity::ContextPtr m_iocCtx;
    std::shared_ptr<arr::AudioAssetStore> m_store;
    std::shared_ptr<arr::Arrangement> m_arrangement;
    arr::AssetId m_asset;
    std::vector<arr::Change> m_changes;
};
}

// --- Step 3: the model (arrangement) ---
TEST_F(Arrangement_UndoStep3Tests, RestoreTracksPutsBackTracksClipsAndIds)
{
    const arr::ArrTrackId t1 = addTrack();
    const arr::ClipId c1 = addClip(t1, Fraction(0, 1), 1.0);
    const arr::TrackList snapshot = m_arrangement->tracks();

    const arr::ArrTrackId t2 = addTrack();
    addClip(t2, Fraction(1, 1), 2.0);
    ASSERT_TRUE(m_arrangement->removeClip(t1, c1));
    ASSERT_FALSE(arr::sameHistory(snapshot, m_arrangement->tracks()));

    m_changes.clear();
    m_arrangement->restoreTracks(snapshot);

    EXPECT_TRUE(arr::sameHistory(snapshot, m_arrangement->tracks()));
    ASSERT_EQ(m_arrangement->tracks().size(), 1u);
    EXPECT_EQ(m_arrangement->tracks()[0].id, t1);
    ASSERT_EQ(m_arrangement->tracks()[0].clips.size(), 1u);
    EXPECT_EQ(m_arrangement->tracks()[0].clips[0].id, c1);
    EXPECT_FALSE(m_arrangement->track(t2));
    ASSERT_EQ(m_changes.size(), 1u);
    EXPECT_EQ(m_changes[0].kind, arr::ChangeKind::Restored);
}

TEST_F(Arrangement_UndoStep3Tests, RestoreTracksRederivesPositionsFromTheCurrentTimeMap)
{
    const arr::ArrTrackId musical = addTrack(arr::TimeBase::Musical);
    const arr::ArrTrackId absolute = addTrack(arr::TimeBase::Absolute);
    const arr::ClipId cm = addClip(musical, Fraction(1, 1), 1.0);   // 2 s at 120 BPM
    const arr::ClipId ca = addClip(absolute, Fraction(1, 1), 1.0);  // 2 s
    const arr::TrackList snapshot = m_arrangement->tracks();

    m_arrangement->setTimeMap(arr::TimeMap::constantTempo(240.0));
    m_arrangement->restoreTracks(snapshot); // the snapshot holds the 120 BPM seconds

    const arr::AudioClip* m = &m_arrangement->track(musical)->clips[0];
    ASSERT_EQ(m->id, cm);
    EXPECT_EQ(m->startTick, Fraction(1, 1));
    EXPECT_DOUBLE_EQ(m->startSecs.raw(), 1.0) << "the Musical clip's seconds follow the new map";

    const arr::AudioClip* a = &m_arrangement->track(absolute)->clips[0];
    ASSERT_EQ(a->id, ca);
    EXPECT_DOUBLE_EQ(a->startSecs.raw(), 2.0) << "the Absolute clip keeps its seconds";
    EXPECT_EQ(a->startTick, Fraction(2, 1)) << "and its tick follows the new map";
}

TEST_F(Arrangement_UndoStep3Tests, IdsAreNeverReusedAfterARestore)
{
    const arr::ArrTrackId t1 = addTrack();
    const arr::ClipId c1 = addClip(t1, Fraction(0, 1), 1.0);

    m_arrangement->restoreTracks({});
    EXPECT_TRUE(m_arrangement->tracks().empty());

    const arr::ArrTrackId t2 = addTrack();
    EXPECT_GT(t2, t1);
    EXPECT_GT(addClip(t2, Fraction(0, 1), 1.0), c1);

    // A snapshot with higher ids than the live ones raises the counters too
    arr::TrackList foreign = m_arrangement->tracks();
    foreign[0].id = 50;
    foreign[0].clips[0].id = 60;
    m_arrangement->restoreTracks(foreign);
    const arr::ArrTrackId t3 = addTrack();
    EXPECT_GT(t3, 50u);
    EXPECT_GT(addClip(t3, Fraction(0, 1), 1.0), 60u);
}

TEST_F(Arrangement_UndoStep3Tests, RestoreKeepsTheLiveFieldsOfALiveTrack)
{
    const arr::ArrTrackId t = addTrack();
    const arr::TrackList beforeArming = m_arrangement->tracks();
    ASSERT_TRUE(m_arrangement->setRecordArmed(t, true));
    ASSERT_TRUE(m_arrangement->setInputMonitoring(t, true));

    m_arrangement->restoreTracks(beforeArming);
    EXPECT_TRUE(m_arrangement->track(t)->recordArmed) << "a live track keeps its arming";
    EXPECT_TRUE(m_arrangement->track(t)->inputMonitoring);

    // A track coming back takes the live fields from the snapshot
    const arr::TrackList armedSnapshot = m_arrangement->tracks();
    ASSERT_TRUE(m_arrangement->removeTrack(t));
    m_arrangement->restoreTracks(armedSnapshot);
    ASSERT_TRUE(m_arrangement->track(t));
    EXPECT_TRUE(m_arrangement->track(t)->recordArmed);
    EXPECT_TRUE(m_arrangement->track(t)->inputMonitoring);
}

TEST_F(Arrangement_UndoStep3Tests, SameHistoryIgnoresLiveAndDerivedFields)
{
    const arr::TrackList base = oneClipList();
    ASSERT_EQ(base.size(), 1u);
    EXPECT_TRUE(arr::sameHistory(base, base));

    // Ignored: live fields, and the derived half of the position
    arr::TrackList other = base;
    other[0].recordArmed = true;
    other[0].inputMonitoring = true;
    other[0].inputChannel = 3;
    other[0].partId = muse::ID(77);
    other[0].clips[0].startSecs = 99.0; // Musical track: seconds are derived
    EXPECT_TRUE(arr::sameHistory(base, other));

    arr::TrackList absoluteA = base;
    absoluteA[0].timeBase = arr::TimeBase::Absolute;
    arr::TrackList absoluteB = absoluteA;
    absoluteB[0].clips[0].startTick = Fraction(5, 1); // Absolute track: the tick is derived
    EXPECT_TRUE(arr::sameHistory(absoluteA, absoluteB));
    absoluteB[0].clips[0].startSecs = 99.0;
    EXPECT_FALSE(arr::sameHistory(absoluteA, absoluteB));

    // Seen: every history field
    using Mutation = std::function<void(arr::TrackList&)>;
    const std::vector<std::pair<const char*, Mutation> > mutations = {
        { "track count", [](arr::TrackList& l) { l.push_back(arr::Track()); } },
        { "track id", [](arr::TrackList& l) { l[0].id += 1; } },
        { "track name", [](arr::TrackList& l) { l[0].name = u"Other"; } },
        { "track timeBase", [](arr::TrackList& l) { l[0].timeBase = arr::TimeBase::Absolute; } },
        { "clip count", [](arr::TrackList& l) { l[0].clips.push_back(l[0].clips[0]); } },
        { "clip id", [](arr::TrackList& l) { l[0].clips[0].id += 1; } },
        { "clip asset", [](arr::TrackList& l) { l[0].clips[0].asset = "other"; } },
        { "clip name", [](arr::TrackList& l) { l[0].clips[0].name = u"Other"; } },
        { "clip startTick", [](arr::TrackList& l) { l[0].clips[0].startTick = Fraction(3, 1); } },
        { "clip sourceOffset", [](arr::TrackList& l) { l[0].clips[0].sourceOffset = 0.25; } },
        { "clip length", [](arr::TrackList& l) { l[0].clips[0].length = 2.0; } },
        { "clip gainDb", [](arr::TrackList& l) { l[0].clips[0].gainDb = -3.f; } },
        { "clip fadeIn length", [](arr::TrackList& l) { l[0].clips[0].fadeIn.length = 0.1; } },
        { "clip fadeIn curve", [](arr::TrackList& l) { l[0].clips[0].fadeIn.curve = arr::FadeCurve::EqualPower; } },
        { "clip fadeOut length", [](arr::TrackList& l) { l[0].clips[0].fadeOut.length = 0.1; } },
        { "clip fadeOut curve", [](arr::TrackList& l) { l[0].clips[0].fadeOut.curve = arr::FadeCurve::EqualPower; } },
        { "clip muted", [](arr::TrackList& l) { l[0].clips[0].muted = true; } },
        { "clip sourceTempoBpm", [](arr::TrackList& l) { l[0].clips[0].sourceTempoBpm = 100.0; } },
    };
    for (const auto& m : mutations) {
        arr::TrackList changed = base;
        m.second(changed);
        EXPECT_FALSE(arr::sameHistory(base, changed)) << m.first;
    }

    arr::TrackList reordered = base;
    reordered.push_back(base[0]);
    reordered.back().id = base[0].id + 10;
    arr::TrackList swapped = reordered;
    std::swap(swapped[0], swapped[1]);
    EXPECT_FALSE(arr::sameHistory(reordered, swapped)) << "track order";
}

TEST_F(Arrangement_UndoStep3Tests, ReadAndClearRaiseTheEpoch)
{
    const uint64_t start = m_arrangement->epoch();

    m_arrangement->clear();
    const uint64_t afterClear = m_arrangement->epoch();
    EXPECT_GT(afterClear, start);

    const arr::ReadProjectEntry empty = [](const std::string&) { return muse::RetVal<muse::ByteArray>::make_ok(muse::ByteArray()); };
    ASSERT_TRUE(m_arrangement->read(empty));
    EXPECT_GT(m_arrangement->epoch(), afterClear);

    const uint64_t afterRead = m_arrangement->epoch();
    addTrack();
    EXPECT_EQ(m_arrangement->epoch(), afterRead) << "an ordinary edit does not";
}

TEST_F(Arrangement_UndoStep3Tests, DuplicateClipCopiesWithANewId)
{
    const arr::ArrTrackId t = addTrack();
    const arr::ArrTrackId absolute = addTrack(arr::TimeBase::Absolute);
    const arr::ClipId original = addClip(t, Fraction(0, 1), 1.0);

    muse::RetVal<arr::ClipId> copy = m_arrangement->duplicateClip(t, original, t, Fraction(1, 1));
    ASSERT_TRUE(copy.ret) << copy.ret.text();
    EXPECT_NE(copy.val, original);
    ASSERT_EQ(m_arrangement->track(t)->clips.size(), 2u);
    const arr::AudioClip& c = m_arrangement->track(t)->clips[1];
    EXPECT_EQ(c.id, copy.val);
    EXPECT_EQ(c.asset, m_asset);
    EXPECT_DOUBLE_EQ(c.length.raw(), 1.0);
    EXPECT_EQ(c.startTick, Fraction(1, 1));
    EXPECT_DOUBLE_EQ(c.startSecs.raw(), 2.0);
    EXPECT_EQ(m_arrangement->track(t)->clips[0].id, original) << "the original stays";

    // Onto an Absolute track: the seconds come from the tick through the time map
    muse::RetVal<arr::ClipId> onAbsolute = m_arrangement->duplicateClip(t, original, absolute, Fraction(1, 1));
    ASSERT_TRUE(onAbsolute.ret) << onAbsolute.ret.text();
    ASSERT_EQ(m_arrangement->track(absolute)->clips.size(), 1u);
    EXPECT_DOUBLE_EQ(m_arrangement->track(absolute)->clips[0].startSecs.raw(), 2.0);

    EXPECT_FALSE(m_arrangement->duplicateClip(t, 9999, t, Fraction(5, 1)).ret) << "unknown clip";
    EXPECT_FALSE(m_arrangement->duplicateClip(9999, original, t, Fraction(5, 1)).ret) << "unknown track";
}

TEST_F(Arrangement_UndoStep3Tests, DuplicateClipFailsOnOverlap)
{
    const arr::ArrTrackId t = addTrack();
    const arr::ClipId original = addClip(t, Fraction(0, 1), 1.0);
    const arr::TrackList before = m_arrangement->tracks();
    m_changes.clear();

    muse::RetVal<arr::ClipId> copy = m_arrangement->duplicateClip(t, original, t, Fraction(1, 8)); // 0.25 s: inside
    EXPECT_FALSE(copy.ret);
    EXPECT_TRUE(arr::sameHistory(before, m_arrangement->tracks()));
    EXPECT_TRUE(m_changes.empty());

    // The failed copy used no clip id
    const arr::ClipId next = addClip(t, Fraction(1, 1), 1.0);
    EXPECT_EQ(next, original + 1);
}

// --- Step 4: edits as undoable steps ---
// --- Step 4: edits as undoable steps ---

//! The model of step 3 recorded in a real score's undo stack (ArrangementUndo::setScoreForTests).
class Arrangement_UndoStep4Tests : public Arrangement_UndoStep3Tests
{
protected:
    void SetUp() override
    {
        Arrangement_UndoStep3Tests::SetUp();
        m_arrangement->setTimeMap(arr::TimeMap::constantTempo(120.0));
        muse::modularity::ioc(m_iocCtx)->registerExport<arr::IArrangement>(TEST_MODULE, m_arrangement);
        m_undo = std::make_shared<arr::ArrangementUndo>(m_iocCtx);

        m_score = ScoreRW::readScore(u"data/systems_breaks.mscx");
        ASSERT_TRUE(m_score);
        m_score->doLayout();
        m_undo->setScoreForTests(m_score);

        // Two tracks with clips, made directly (not recorded): the starting point of every test
        m_trackA = addTrack();
        m_trackB = addTrack();
        m_clipA1 = addClip(m_trackA, Fraction(0, 1), 1.0);
        m_clipA2 = addClip(m_trackA, Fraction(2, 1), 2.0);
        m_clipB1 = addClip(m_trackB, Fraction(1, 1), 1.0);
    }

    void TearDown() override
    {
        m_undo->setScoreForTests(nullptr);
        delete m_score;
        m_undo = nullptr;
        Arrangement_UndoStep3Tests::TearDown();
    }

    //! arrangement.json as written now ("" when there are no tracks: nothing is written)
    std::string json() const
    {
        std::map<std::string, muse::ByteArray> entries;
        const muse::Ret ret = m_arrangement->write([&entries](const std::string& name, const muse::ByteArray& data) {
            entries[name] = data;
            return muse::make_ok();
        });
        EXPECT_TRUE(ret) << ret.text();
        auto it = entries.find(arr::ARRANGEMENT_JSON_NAME);
        return it == entries.end() ? std::string() : std::string(it->second.constChar(), it->second.size());
    }

    muse::Ret edit(const char* name, const std::function<muse::Ret(arr::IArrangement&)>& fn)
    {
        return m_undo->edit(muse::TranslatableString::untranslatable(name), fn);
    }

    size_t steps() const { return m_score->undoStack()->currentIndex(); }
    void undo() { m_score->transactionManager()->undoRedo(true, nullptr); }
    void redo() { m_score->transactionManager()->undoRedo(false, nullptr); }

    std::shared_ptr<arr::ArrangementUndo> m_undo;
    MasterScore* m_score = nullptr;
    arr::ArrTrackId m_trackA = arr::INVALID_TRACK;
    arr::ArrTrackId m_trackB = arr::INVALID_TRACK;
    arr::ClipId m_clipA1 = arr::INVALID_CLIP;
    arr::ClipId m_clipA2 = arr::INVALID_CLIP;
    arr::ClipId m_clipB1 = arr::INVALID_CLIP;
};

TEST_F(Arrangement_UndoStep4Tests, EachEditUndoesAndRedoesToTheSameJson)
{
    using Fn = std::function<muse::Ret(arr::IArrangement&)>;
    const std::vector<std::pair<const char*, Fn> > edits = {
        { "add track", [](arr::IArrangement& a) { return a.addAudioTrack(u"New", -1).ret; } },
        { "remove track", [this](arr::IArrangement& a) { return a.removeTrack(m_trackB); } },
        { "rename track", [this](arr::IArrangement& a) { return a.renameTrack(m_trackA, u"Renamed"); } },
        { "move track", [this](arr::IArrangement& a) { return a.moveTrack(m_trackB, 0); } },
        { "add clip", [this](arr::IArrangement& a) { return a.addClip(m_trackB, clipAt(Fraction(4, 1), 1.0)).ret; } },
        { "remove clip", [this](arr::IArrangement& a) { return a.removeClip(m_trackA, m_clipA2); } },
        { "move clip", [this](arr::IArrangement& a) { return a.moveClip(m_trackA, m_clipA2, m_trackA, Fraction(3, 1)); } },
        { "move clip to another track",
          [this](arr::IArrangement& a) { return a.moveClip(m_trackA, m_clipA2, m_trackB, Fraction(3, 1)); } },
        { "trim clip", [this](arr::IArrangement& a) { return a.trimClip(m_trackA, m_clipA2, 0.5, 1.0); } },
        { "split clip", [this](arr::IArrangement& a) { return a.splitClip(m_trackA, m_clipA2, Fraction(5, 2)).ret; } },
        { "gain", [this](arr::IArrangement& a) { return a.setClipGain(m_trackA, m_clipA1, -6.f); } },
        { "fades", [this](arr::IArrangement& a) {
              return a.setClipFades(m_trackA, m_clipA1, arr::Fade { 0.1, arr::FadeCurve::Linear },
                                    arr::Fade { 0.2, arr::FadeCurve::EqualPower });
          } },
        { "mute", [this](arr::IArrangement& a) { return a.setClipMuted(m_trackB, m_clipB1, true); } },
        { "duplicate clip",
          [this](arr::IArrangement& a) { return a.duplicateClip(m_trackA, m_clipA1, m_trackB, Fraction(6, 1)).ret; } },
    };

    for (const auto& [name, fn] : edits) {
        SCOPED_TRACE(name);
        const std::string original = json();
        const size_t stepsBefore = steps();

        const muse::Ret ret = edit(name, fn);
        ASSERT_TRUE(ret) << ret.text();
        const std::string edited = json();
        ASSERT_NE(edited, original) << "the edit changed nothing";
        ASSERT_EQ(steps(), stepsBefore + 1) << "one undo step";

        undo();
        EXPECT_EQ(json(), original) << "undo";
        redo();
        EXPECT_EQ(json(), edited) << "redo";
        undo(); // back to the start for the next edit
        ASSERT_EQ(json(), original);
    }
}

TEST_F(Arrangement_UndoStep4Tests, FiftyRandomEditsUndoneGiveTheOriginalJsonByteForByte)
{
    const std::string original = json();
    const size_t stepsBefore = steps();
    std::mt19937 random(20261119);

    auto anyClip = [this, &random](arr::ArrTrackId& track, arr::ClipId& clip) {
        std::vector<std::pair<arr::ArrTrackId, arr::ClipId> > all;
        for (const arr::Track& t : m_arrangement->tracks()) {
            for (const arr::AudioClip& c : t.clips) {
                all.emplace_back(t.id, c.id);
            }
        }
        if (all.empty()) {
            return false;
        }
        std::tie(track, clip) = all[random() % all.size()];
        return true;
    };
    auto anyTrack = [this, &random]() {
        const arr::TrackList& tracks = m_arrangement->tracks();
        return tracks.empty() ? arr::INVALID_TRACK : tracks[random() % tracks.size()].id;
    };

    int recorded = 0;
    for (int i = 0; i < 50; ++i) {
        const int kind = static_cast<int>(random() % 7);
        const Fraction at(static_cast<int>(random() % 16), 2);
        muse::Ret ret = edit("random", [&](arr::IArrangement& a) -> muse::Ret {
            arr::ArrTrackId track;
            arr::ClipId clip;
            switch (kind) {
            case 0: return a.addAudioTrack(u"T", -1).ret;
            case 1: return a.addClip(anyTrack(), clipAt(at, 0.5)).ret;
            case 2: return anyClip(track, clip) ? a.moveClip(track, clip, anyTrack(), at) : muse::make_ret(muse::Ret::Code::UnknownError);
            case 3: return anyClip(track, clip) ? a.setClipGain(track, clip, -float(random() % 12)) : muse::make_ret(muse::Ret::Code::UnknownError);
            case 4: return anyClip(track, clip) ? a.setClipMuted(track, clip, random() % 2) : muse::make_ret(muse::Ret::Code::UnknownError);
            case 5: return anyClip(track, clip) ? a.removeClip(track, clip) : muse::make_ret(muse::Ret::Code::UnknownError);
            default: return a.renameTrack(anyTrack(), u"R" + muse::String::number(i));
            }
        });
        recorded += ret ? 1 : 0; // a refused edit (overlap, nothing to edit) records nothing
    }
    ASSERT_GT(recorded, 20) << "too few edits succeeded for the test to mean much";
    ASSERT_LE(steps() - stepsBefore, static_cast<size_t>(recorded));

    while (steps() > stepsBefore) {
        undo();
    }
    EXPECT_EQ(json(), original);
}

TEST_F(Arrangement_UndoStep4Tests, AFailedEditPutsTheArrangementBackAndRecordsNothing)
{
    const std::string original = json();
    const size_t stepsBefore = steps();
    const muse::Ret ret = edit("half an edit", [](arr::IArrangement& a) {
        a.addAudioTrack(u"Half", -1);
        return muse::make_ret(muse::Ret::Code::UnknownError);
    });
    EXPECT_FALSE(ret);
    EXPECT_EQ(json(), original);
    EXPECT_EQ(steps(), stepsBefore);
}

TEST_F(Arrangement_UndoStep4Tests, AnEditThatChangesNothingRecordsNothing)
{
    const size_t stepsBefore = steps();
    // Arming is a live field: not history
    EXPECT_TRUE(edit("arm", [this](arr::IArrangement& a) { return a.setRecordArmed(m_trackA, true); }));
    EXPECT_EQ(steps(), stepsBefore);
    EXPECT_TRUE(m_arrangement->track(m_trackA)->recordArmed);
}

TEST_F(Arrangement_UndoStep4Tests, AnEditInsideAnOpenTransactionJoinsIt)
{
    const std::string original = json();
    const size_t stepsBefore = steps();
    m_score->transactionManager()->beginTransaction(muse::TranslatableString::untranslatable("outer"));
    ASSERT_TRUE(edit("inner", [](arr::IArrangement& a) { return a.addAudioTrack(u"Joined", -1).ret; }));
    ASSERT_TRUE(m_score->transactionManager()->currentTransaction()) << "edit() closed a transaction it did not open";
    m_score->transactionManager()->endTransaction();

    EXPECT_EQ(steps(), stepsBefore + 1);
    EXPECT_EQ(m_score->undoStack()->last()->actionName().str, u"outer");
    undo();
    EXPECT_EQ(json(), original);
}

TEST_F(Arrangement_UndoStep4Tests, ANoteAndAClipEditUndoInOrder)
{
    Measure* bar1 = m_score->firstMeasure();
    const double stretch = bar1->userStretch();

    m_score->transactionManager()->beginTransaction(muse::TranslatableString::untranslatable("score edit"));
    bar1->undoChangeProperty(Pid::USER_STRETCH, stretch + 1.0);
    m_score->transactionManager()->endTransaction();

    const std::string beforeClipEdit = json();
    ASSERT_TRUE(edit("gain", [this](arr::IArrangement& a) { return a.setClipGain(m_trackA, m_clipA1, -3.f); }));
    const std::string afterClipEdit = json();

    undo(); // the clip edit first
    EXPECT_EQ(json(), beforeClipEdit);
    EXPECT_DOUBLE_EQ(bar1->userStretch(), stretch + 1.0);
    undo(); // then the score edit
    EXPECT_DOUBLE_EQ(bar1->userStretch(), stretch);
    redo();
    redo();
    EXPECT_DOUBLE_EQ(bar1->userStretch(), stretch + 1.0);
    EXPECT_EQ(json(), afterClipEdit);
}

TEST_F(Arrangement_UndoStep4Tests, WithoutAScoreAnEditRunsUnrecorded)
{
    m_undo->setScoreForTests(nullptr);
    const size_t stepsBefore = steps();
    EXPECT_TRUE(edit("rename", [this](arr::IArrangement& a) { return a.renameTrack(m_trackA, u"Unrecorded"); }));
    EXPECT_EQ(m_arrangement->track(m_trackA)->name, u"Unrecorded");
    EXPECT_EQ(steps(), stepsBefore);
}

TEST_F(Arrangement_UndoStep4Tests, ArmingSurvivesUndoAndRedoOfClipEdits)
{
    // The Fable check's example: arm, move a clip twice, undo twice, redo once
    ASSERT_TRUE(m_arrangement->setRecordArmed(m_trackA, true)); // live, set outside edit()
    ASSERT_TRUE(edit("move", [this](arr::IArrangement& a) { return a.moveClip(m_trackA, m_clipA2, m_trackA, Fraction(3, 1)); }));
    ASSERT_TRUE(edit("move", [this](arr::IArrangement& a) { return a.moveClip(m_trackA, m_clipA2, m_trackA, Fraction(4, 1)); }));
    undo();
    undo();
    redo();
    EXPECT_TRUE(m_arrangement->track(m_trackA)->recordArmed);
}

TEST_F(Arrangement_UndoStep4Tests, AnEditThatCommitsItsOwnTransactionIsPutBackAndFails)
{
    const std::string original = json();
    const muse::Ret ret = edit("breaks the contract", [this](arr::IArrangement& a) {
        a.addAudioTrack(u"Broken", -1);
        m_score->transactionManager()->endTransaction(); // not allowed inside edit()
        return muse::make_ok();
    });
    EXPECT_FALSE(ret);
    EXPECT_EQ(json(), original);
    EXPECT_FALSE(m_score->transactionManager()->currentTransaction());
}

TEST_F(Arrangement_UndoStep4Tests, OnALockedStackAnEditRunsUnrecorded)
{
    const size_t stepsBefore = steps();
    m_score->undoStack()->setLocked(true);
    const muse::Ret ret = edit("rename", [this](arr::IArrangement& a) { return a.renameTrack(m_trackA, u"Locked"); });
    m_score->undoStack()->setLocked(false);
    EXPECT_TRUE(ret);
    EXPECT_EQ(m_arrangement->track(m_trackA)->name, u"Locked");
    EXPECT_EQ(steps(), stepsBefore);
}

TEST_F(Arrangement_UndoStep4Tests, ARolledBackEditIsPutBackAndFails)
{
    const std::string original = json();
    const size_t stepsBefore = steps();
    const muse::Ret ret = edit("on a read-only score", [this](arr::IArrangement& a) {
        m_score->setReadOnly(true); // endTransaction rolls back a read-only score's transaction
        return a.addAudioTrack(u"Rolled back", -1).ret;
    });
    m_score->setReadOnly(false);
    EXPECT_FALSE(ret);
    EXPECT_EQ(json(), original);
    EXPECT_EQ(steps(), stepsBefore);
}

TEST_F(Arrangement_UndoStep4Tests, ACommandFromAnotherEpochDoesNothing)
{
    ASSERT_TRUE(edit("add", [](arr::IArrangement& a) { return a.addAudioTrack(u"Old epoch", -1).ret; }));
    m_arrangement->clear(); // another project in the same window
    m_arrangement->addAudioTrack(u"New project", -1);
    const std::string now = json();
    undo();
    EXPECT_EQ(json(), now);
    redo();
    EXPECT_EQ(json(), now);
}

// --- Step 5: the retention rule (FD-015 check 3) ---
// The controller's part of step 5 (the import as one step with its audio part, "modified", the
// parked mixer settings, a part view's undo stack) is tested in arrangementcontrollertests.cpp.
// The project's side (markAsUnsaved and a new project after an undone edit) is an app check:
// no unit test builds a NotationProject.

namespace {
//! Every entry the arrangement writes, by name
std::map<std::string, muse::ByteArray> writeAll(const arr::IArrangement& arrangement)
{
    std::map<std::string, muse::ByteArray> entries;
    const muse::Ret ret = arrangement.write([&entries](const std::string& name, const muse::ByteArray& data) {
        entries[name] = muse::ByteArray(data.constData(), data.size()); // a copy: data can be a view
        return muse::make_ok();
    });
    EXPECT_TRUE(ret) << ret.text();
    return entries;
}

bool writesAudioOf(const std::map<std::string, muse::ByteArray>& entries, const arr::AssetId& id)
{
    for (const auto& [name, data] : entries) {
        if (name.rfind("audio/", 0) == 0 && name.find(id) != std::string::npos) {
            return !data.empty();
        }
    }
    return false;
}
}

class Arrangement_UndoStep5Tests : public Arrangement_UndoStep4Tests
{
protected:
    //! A second file imported and placed on a new track in one recorded edit: an import
    void importSecondFile()
    {
        const muse::io::path_t file(m_dir.filePath("second.wav"));
        ASSERT_TRUE(arr::tests::writeFile(file, arr::tests::makeSineWav(1.0, 8000, 1, 330.0)));
        muse::RetVal<arr::AudioAsset> a = m_store->importFileSync(file, arr::AssetStorage::Embedded);
        ASSERT_TRUE(a.ret) << a.ret.text();
        m_second = a.val.id;
        ASSERT_TRUE(edit("Add audio track", [this](arr::IArrangement& model) -> muse::Ret {
            muse::RetVal<arr::ArrTrackId> t = model.addAudioTrack(u"Second", -1);
            if (!t.ret) {
                return t.ret;
            }
            arr::AudioClip c;
            c.asset = m_second;
            c.length = 1.0;
            return model.addClip(t.val, c).ret;
        }));
    }

    bool clipUses(const arr::AssetId& id) const
    {
        for (const arr::Track& t : m_arrangement->tracks()) {
            for (const arr::AudioClip& c : t.clips) {
                if (c.asset == id) {
                    return true;
                }
            }
        }
        return false;
    }

    arr::AssetId m_second;
};

TEST_F(Arrangement_UndoStep5Tests, AnUndoneImportKeepsItsAssetAndTheRedoBringsTheClipBack)
{
    importSecondFile();
    ASSERT_TRUE(clipUses(m_second));
    undo();
    EXPECT_FALSE(clipUses(m_second));
    EXPECT_TRUE(m_store->asset(m_second)) << "the asset stays in the store while the history can bring its clip back";
    redo();
    EXPECT_TRUE(clipUses(m_second));
}

TEST_F(Arrangement_UndoStep5Tests, SaveAfterAnUndoneImportWritesNoAudioForIt)
{
    importSecondFile();
    ASSERT_TRUE(writesAudioOf(writeAll(*m_arrangement), m_second));
    undo();
    const auto entries = writeAll(*m_arrangement);
    EXPECT_FALSE(writesAudioOf(entries, m_second));
    EXPECT_TRUE(writesAudioOf(entries, m_asset)) << "the audio clips still use is written";
}

TEST_F(Arrangement_UndoStep5Tests, ImportUndoSaveRedoSaveWritesTheAudioAgain)
{
    importSecondFile();
    undo();
    EXPECT_FALSE(writesAudioOf(writeAll(*m_arrangement), m_second)); // the first save
    redo();
    EXPECT_TRUE(writesAudioOf(writeAll(*m_arrangement), m_second)); // the second save
}
