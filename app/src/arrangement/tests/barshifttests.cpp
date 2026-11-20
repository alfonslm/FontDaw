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

// FD-151: an audio clip on a Musical track moves with its bar when bars are inserted, deleted or
// rewritten before or around it, as notes do (BarShift, src/arrangement/internal/barshift.*,
// registered on the score's transactions by ArrangementUndo). One undo restores the bars and the
// clips together. Plan: docs/fontdaw/batches/batch-02/03a-undo-layer.md, "Plan for FD-151".

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/timesig.h"
#include "engraving/editing/edittimesig.h"
#include "engraving/editing/splitjoinmeasure.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/tests/utils/scorerw.h"

#include "global/types/translatablestring.h"

#include "internal/arrangement.h"
#include "internal/audioassetstore.h"
#include "internal/arrangementundo.h"

#include "mocks/arrangementconfigurationmock.h"
#include "testutils.h"

using ::testing::NiceMock;
using ::testing::Return;

using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace mu::engraving;
using namespace muse;

static const std::string TEST_MODULE("arrangement_barshift_utests");
static const modularity::IoCID TEST_CONTEXT_ID = 6; // 3 is arrangementcontrollertests.cpp's

// data/systems_breaks.mscx: 9 bars of 4/4 at 120 BPM, no repeats, so a played tick is a written
// tick and a constant 120 BPM time map is exactly the score's (the app's map reads the score's
// played time, ArrangementController::updateTimeMap).
class Arrangement_BarShiftTests : public ::testing::Test
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
        m_arrangement->setTimeMap(TimeMap::constantTempo(120.0));
        modularity::ioc(m_iocCtx)->registerExport<IArrangement>(TEST_MODULE, m_arrangement);
        m_undo = std::make_shared<ArrangementUndo>(m_iocCtx);

        const io::path_t file(m_dir.filePath("four.wav"));
        ASSERT_TRUE(writeFile(file, makeSineWav(4.0, 8000, 1)));
        RetVal<AudioAsset> a = m_store->importFileSync(file, AssetStorage::Embedded);
        ASSERT_TRUE(a.ret) << a.ret.text();

        m_score = ScoreRW::readScore(u"data/systems_breaks.mscx");
        ASSERT_TRUE(m_score);
        // FD-151's fix: the bar shift watches this score's transactions (in the app the controller
        // points ArrangementUndo at the open project's score)
        m_undo->setScoreForTests(m_score);
        m_bar3 = m_score->firstMeasure()->nextMeasure()->nextMeasure();
        ASSERT_TRUE(m_bar3);
        ASSERT_EQ(m_bar3->tick(), Fraction(2, 1));

        // A clip at the start of bar 3, 4 s long (bars 3 and 4)
        RetVal<ArrTrackId> t = m_arrangement->addAudioTrack(u"Audio", -1, TimeBase::Musical);
        ASSERT_TRUE(t.ret);
        m_track = t.val;
        AudioClip c;
        c.asset = a.val.id;
        c.startTick = m_bar3->tick();
        c.length = 4.0;
        RetVal<ClipId> id = m_arrangement->addClip(m_track, c);
        ASSERT_TRUE(id.ret) << id.ret.text();
        m_clip = id.val;
    }

    void TearDown() override
    {
        m_undo->setScoreForTests(nullptr);
        delete m_score;
        m_undo = nullptr;
        m_arrangement = nullptr;
        m_store = nullptr;
        modularity::globalIoc()->unregister<IArrangementConfiguration>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    const AudioClip* clip() const
    {
        for (const AudioClip& c : m_arrangement->track(m_track)->clips) {
            if (c.id == m_clip) {
                return &c;
            }
        }
        return nullptr;
    }

    Measure* bar(int number) const
    {
        Measure* m = m_score->firstMeasure();
        for (int i = 1; m && i < number; ++i) {
            m = m->nextMeasure();
        }
        return m;
    }

    size_t steps() const { return m_score->undoStack()->currentIndex(); }
    void undo() { m_score->transactionManager()->undoRedo(true, nullptr); }
    void redo() { m_score->transactionManager()->undoRedo(false, nullptr); }

    void insertBarBefore(Measure* before)
    {
        m_score->startCmd(TranslatableString::untranslatable("insert a bar"));
        m_score->insertMeasure(before);
        m_score->endCmd();
    }

    void deleteBars(Measure* first, Measure* last)
    {
        m_score->startCmd(TranslatableString::untranslatable("delete bars"));
        m_score->deleteMeasures(first, last);
        m_score->endCmd();
    }

    void setTimeSig(Measure* from, int numerator, int denominator)
    {
        m_score->transactionManager()->transaction(TranslatableString::untranslatable("time signature"), [&](Transaction& tx) {
            TimeSig* ts = Factory::createTimeSig(m_score->dummy());
            ts->setSig(Fraction(numerator, denominator));
            EditTimeSig::addTimeSig(tx, m_score, from, 0, ts, false); // takes ts
        });
    }

    QTemporaryDir m_dir;
    std::shared_ptr<NiceMock<ArrangementConfigurationMock> > m_configuration;
    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<AudioAssetStore> m_store;
    std::shared_ptr<Arrangement> m_arrangement;
    MasterScore* m_score = nullptr;
    std::shared_ptr<ArrangementUndo> m_undo;
    Measure* m_bar3 = nullptr;
    ArrTrackId m_track = INVALID_TRACK;
    ClipId m_clip = INVALID_CLIP;
};

TEST_F(Arrangement_BarShiftTests, TheClipStartsAtBar3BeforeAnyEdit)
{
    ASSERT_TRUE(clip());
    EXPECT_EQ(clip()->startTick, m_bar3->tick());
    EXPECT_DOUBLE_EQ(clip()->startSecs.raw(), 4.0);
}

// A bar inserted before bar 3 moves bar 3 one bar later; the clip moves with it
TEST_F(Arrangement_BarShiftTests, FD151_AClipMovesWithItsBarWhenABarIsInsertedBeforeIt)
{
    insertBarBefore(m_bar3);

    ASSERT_EQ(m_bar3->tick(), Fraction(3, 1)) << "the inserted bar should push bar 3 one bar later";
    ASSERT_TRUE(clip());
    EXPECT_EQ(clip()->startTick, m_bar3->tick()) << "the clip should still start at bar 3";
    EXPECT_DOUBLE_EQ(clip()->startSecs.raw(), 6.0);
    EXPECT_DOUBLE_EQ(clip()->length.raw(), 4.0) << "a clip's length never changes";
}

// Bar 1 deleted moves bar 3 one bar earlier; the clip moves with it
TEST_F(Arrangement_BarShiftTests, FD151_AClipMovesWithItsBarWhenABarBeforeItIsDeleted)
{
    deleteBars(m_score->firstMeasure(), m_score->firstMeasure());

    ASSERT_EQ(m_bar3->tick(), Fraction(1, 1)) << "deleting bar 1 should move bar 3 one bar earlier";
    ASSERT_TRUE(clip());
    EXPECT_EQ(clip()->startTick, m_bar3->tick()) << "the clip should still start at bar 3";
    EXPECT_DOUBLE_EQ(clip()->startSecs.raw(), 2.0);
}

TEST_F(Arrangement_BarShiftTests, OneUndoRestoresTheBarsAndTheClipAndRedoMovesBoth)
{
    const size_t before = steps();
    insertBarBefore(m_bar3);
    EXPECT_EQ(steps(), before + 1) << "the bar and the clip are one undo step";

    undo();
    EXPECT_EQ(m_bar3->tick(), Fraction(2, 1));
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));

    redo();
    EXPECT_EQ(m_bar3->tick(), Fraction(3, 1));
    EXPECT_EQ(clip()->startTick, Fraction(3, 1));
}

// Bars inserted inside a clip never split it, and do not move its start (owner, flex sketch Q7)
TEST_F(Arrangement_BarShiftTests, BarsInsertedInsideAClipLeaveItWholeAndInPlace)
{
    insertBarBefore(bar(4)); // the clip covers bars 3 and 4
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
    EXPECT_DOUBLE_EQ(clip()->length.raw(), 4.0);
}

// Bars deleted under the middle of a clip leave it whole, its start where it was (Q-E)
TEST_F(Arrangement_BarShiftTests, BarsDeletedUnderTheMiddleOfAClipLeaveItWhole)
{
    deleteBars(bar(4), bar(4));
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
    EXPECT_DOUBLE_EQ(clip()->length.raw(), 4.0);
}

// The clip's start bar deleted: it stays whole and starts where the deleted range was
TEST_F(Arrangement_BarShiftTests, AClipWhoseStartBarIsDeletedStartsWhereTheRangeWas)
{
    Measure* bar4 = bar(4);
    deleteBars(m_bar3, m_bar3);
    EXPECT_EQ(bar4->tick(), Fraction(2, 1));
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
    EXPECT_DOUBLE_EQ(clip()->length.raw(), 4.0);
}

// A time signature rewrite replaces the bars at the same ticks: the clip keeps its tick (point A)
TEST_F(Arrangement_BarShiftTests, ATimeSignatureChangeOverAClipKeepsItsTick)
{
    setTimeSig(m_bar3, 4, 4); // the same signature, so the rewritten bars have the old ticks
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
}

// No bar survives after the rewritten ones (no S): placed against the new end of the last bar
TEST_F(Arrangement_BarShiftTests, ATimeSignatureChangeWithNoLaterOneKeepsAClipInTheRewrittenBars)
{
    ASSERT_TRUE(m_arrangement->setClipStartTicks({ { m_track, m_clip, Fraction(4, 1) } })); // bar 5
    setTimeSig(m_bar3, 3, 4); // bars 3 to the end become 3/4
    EXPECT_EQ(clip()->startTick, Fraction(4, 1)) << "the clip stays at its time; the bars around it changed";
}

TEST_F(Arrangement_BarShiftTests, AnEditThatMovesNoBarRecordsNoClipMove)
{
    const size_t before = steps();
    m_score->startCmd(TranslatableString::untranslatable("stretch"));
    bar(1)->undoChangeProperty(Pid::USER_STRETCH, 2.0);
    m_score->endCmd();
    ASSERT_EQ(steps(), before + 1);
    for (const UndoableCommand* cmd : m_score->undoStack()->last()->commands()) {
        EXPECT_STRNE(cmd->name(), "ArrangementEdit");
    }
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
}

TEST_F(Arrangement_BarShiftTests, ARolledBackEditMovesNothing)
{
    m_score->startCmd(TranslatableString::untranslatable("insert, then roll back"));
    m_score->insertMeasure(m_bar3);
    m_score->endCmd(/*rollback*/ true);
    EXPECT_EQ(m_bar3->tick(), Fraction(2, 1));
    EXPECT_EQ(clip()->startTick, Fraction(2, 1));
}

TEST_F(Arrangement_BarShiftTests, AClipOnAnAbsoluteTrackIsNeverMoved)
{
    RetVal<ArrTrackId> t = m_arrangement->addAudioTrack(u"Absolute", -1, TimeBase::Absolute);
    ASSERT_TRUE(t.ret);
    AudioClip c;
    c.asset = clip()->asset;
    c.startSecs = 8.0; // bar 5
    c.length = 1.0;
    RetVal<ClipId> id = m_arrangement->addClip(t.val, c);
    ASSERT_TRUE(id.ret) << id.ret.text();

    insertBarBefore(m_bar3);
    EXPECT_DOUBLE_EQ(m_arrangement->track(t.val)->clips.front().startSecs.raw(), 8.0);
}

TEST_F(Arrangement_BarShiftTests, ReopenWithoutASnapshotDoesNothing)
{
    insertBarBefore(m_bar3);
    const Fraction moved = clip()->startTick;
    m_score->transactionManager()->reopen(); // no "began": no snapshot
    m_score->transactionManager()->endTransaction();
    EXPECT_EQ(clip()->startTick, moved);
}

// Q-D: a clip under a repeat keeps its pass. repeats_tempo.mscx: three bars of 4/4, bars 1-2
// repeated; played order 1 2 1 2 3 (repeat segments: bars 1-2 at played 0, bars 1-3 at played
// 2/1). A clip on the second pass of bar 2 (played 3/1) moves to that pass's new place when a bar
// is inserted before bar 2, inside the repeat: bar 2 is written at 2/1 then, the repeated section
// is three bars long, and the second pass of bar 2 is played at 5/1.
TEST_F(Arrangement_BarShiftTests, AClipUnderARepeatKeepsItsPass)
{
    m_undo->setScoreForTests(nullptr);
    delete m_score;
    m_score = ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(m_score);
    m_undo->setScoreForTests(m_score);
    ASSERT_EQ(m_score->expandedRepeatList().utick2tick(Fraction(3, 1).ticks()), Fraction(1, 1).ticks())
        << "played 3/1 is bar 2's second pass";

    Measure* bar2 = bar(2);
    ASSERT_TRUE(m_arrangement->setClipStartTicks({ { m_track, m_clip, Fraction(3, 1) } }));
    insertBarBefore(bar2);

    EXPECT_EQ(bar2->tick(), Fraction(2, 1)) << "bar 2 is written one bar later";
    EXPECT_EQ(clip()->startTick, Fraction(5, 1)) << "still on its second pass";
}
