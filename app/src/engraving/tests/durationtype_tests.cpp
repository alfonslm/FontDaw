/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "engraving/dom/chord.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/utils.h"

#include "engraving/editing/editduration.h"
#include "engraving/editing/noteinput.h"
#include "engraving/editing/transaction/transaction.h"

#include "utils/scorerw.h"

using namespace mu::engraving;

static const String DURATIONTYPE_DATA_DIR("durationtype_data/");

class Engraving_DurationTypeTests : public ::testing::Test
{
private slots:

    void halfDuration();
    void doubleDuration();
    void decDurationDotted();
    void incDurationDotted();
    void halfDoubleDurationLinkedStaves();
    void augmentationLines();
    void diminutionLines();
};

// Simple tests for command "half-duration" (default shortcut "Q").
// starts with Whole note and repeatedly applies cmdHalfDuration()
TEST_F(Engraving_DurationTypeTests, halfDuration)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_WHOLE);
    score->inputState().setNoteEntryMode(true);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Half duration tests"), [&](Transaction& tx) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);

        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        EXPECT_EQ(c->ticks(), Fraction(1, 1));

        // repeatedly half-duration from V_WHOLE to V_128
        for (int i = 128; i > 1; i /= 2) {
            EditDuration::halfDuration(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(i / 2, 128));
        }
    });

    delete score;
}

// Simple tests for command "double-duration" (default shortcut "W").
// Starts with 128th note and repeatedly applies cmdDoubleDuration() up to Whole note.
TEST_F(Engraving_DurationTypeTests, doubleDuration)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_128TH);
    score->inputState().setNoteEntryMode(true);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Double duration tests"), [&](Transaction& tx) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);

        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        EXPECT_EQ(c->ticks(), Fraction(1, 128));

        // repeatedly double-duration from V_128 to V_WHOLE
        for (int i = 1; i < 128; i *= 2) {
            EditDuration::doubleDuration(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(2 * i, 128));
        }
    });

    delete score;
}

// Simple tests for command "dec-duration-dotted" (default shortcut "Shift+Q").
// Starts with Whole note and repeatedly applies cmdDecDurationDotted() down to 128th note.
TEST_F(Engraving_DurationTypeTests, decDurationDotted)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_WHOLE);
    score->inputState().setNoteEntryMode(true);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Decrease duration dotted tests"), [&](Transaction& tx) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);

        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        EXPECT_EQ(c->ticks(), Fraction(1, 1));

        // repeatedly dec-duration-dotted from V_WHOLE to V_128
        for (int i = 128; i > 1; i /= 2) {
            EditDuration::decDurationDotted(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(i + i / 2, 256));

            EditDuration::decDurationDotted(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(i / 2, 128));
        }
    });

    delete score;
}

// Simple tests for command "inc-duration-dotted" (default shortcut "Shift+W").
// Starts with 128th note and repeatedly applies cmdIncDurationDotted() up to Whole note.
TEST_F(Engraving_DurationTypeTests, incDurationDotted)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_128TH);
    score->inputState().setNoteEntryMode(true);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Increase duration dotted tests"), [&](Transaction& tx) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);

        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        EXPECT_EQ(c->ticks(), Fraction(1, 128));

        // repeatedly inc-duration-dotted from V_128 to V_WHOLE
        for (int i = 1; i < 128; i *= 2) {
            EditDuration::incDurationDotted(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(3 * i, 256));

            EditDuration::incDurationDotted(tx, score);
            c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
            EXPECT_EQ(c->ticks(), Fraction(i, 64));
        }
    });

    delete score;
}

// "half-duration" and "double-duration" on a note selected together with its linked copy
// should change the duration only once.
TEST_F(Engraving_DurationTypeTests, halfDoubleDurationLinkedStaves)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_QUARTER);
    score->inputState().setNoteEntryMode(true);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Linked staves duration tests"), [&](Transaction&) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);

        Staff* staff = score->staff(0);
        Staff* linkedStaff = Factory::createStaff(staff->part());
        linkedStaff->setPart(staff->part());
        score->undoInsertStaff(linkedStaff, 1, false);
        Excerpt::cloneStaff(staff, linkedStaff);
    });

    score->inputState().setNoteEntryMode(false);

    auto applyToBothCopies = [score](void (*func)(Transaction&, Score*)) {
        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        Chord* linkedChord = score->firstMeasure()->findChord(Fraction(0, 1), VOICES);
        ASSERT_TRUE(c);
        ASSERT_TRUE(linkedChord);
        EXPECT_TRUE(c->isLinked(linkedChord));

        score->select(c->upNote(), SelectType::SINGLE);
        score->select(linkedChord->upNote(), SelectType::ADD);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Linked staves duration tests"), [&](Transaction& tx) {
            func(tx, score);
        });
    };

    auto expectTicks = [score](const Fraction& ticks) {
        Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
        Chord* linkedChord = score->firstMeasure()->findChord(Fraction(0, 1), VOICES);
        ASSERT_TRUE(c);
        ASSERT_TRUE(linkedChord);
        EXPECT_EQ(c->ticks(), ticks);
        EXPECT_EQ(linkedChord->ticks(), ticks);
    };

    applyToBothCopies(EditDuration::doubleDuration);
    expectTicks(Fraction(1, 2));

    score->undoRedo(true, nullptr);
    expectTicks(Fraction(1, 4));

    applyToBothCopies(EditDuration::halfDuration);
    expectTicks(Fraction(1, 8));

    delete score;
}

TEST_F(Engraving_DurationTypeTests, augmentationLines)
{
    auto jianpuDurationLines = [](DurationType type, int dots) {
        TDuration duration(type);
        duration.setDots(dots);
        return jianpuAugmentationLinesForDuration(duration);
    };

    EXPECT_EQ(jianpuDurationLines(DurationType::V_HALF, 0), 1);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_HALF, 1), 2);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_HALF, 2), 2);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_WHOLE, 0), 3);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_WHOLE, 1), 5);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_BREVE, 0), 7);
    EXPECT_EQ(jianpuDurationLines(DurationType::V_LONG, 0), 15);
}

TEST_F(Engraving_DurationTypeTests, diminutionLines)
{
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_LONG)), 0);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_BREVE)), 0);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_WHOLE)), 0);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_HALF)), 0);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_QUARTER)), 0);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_EIGHTH)), 1);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_16TH)), 2);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_32ND)), 3);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_64TH)), 4);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_128TH)), 5);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_256TH)), 6);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_512TH)), 7);
    EXPECT_EQ(jianpuDiminutionLinesForDuration(TDuration(DurationType::V_1024TH)), 8);
}

// FontDaw (FD-055): add or remove one augmentation dot on the selection (the FontDaw layouts' Shift+W
// and Shift+Q; pressing twice adds or removes two)
TEST_F(Engraving_DurationTypeTests, stepDotsOnSelection)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    // a quarter note at the start of the first measure
    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_QUARTER);
    score->inputState().setNoteEntryMode(true);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots tests"), [&](Transaction&) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);
    });
    score->inputState().setNoteEntryMode(false);

    auto chordAtStart = [score]() {
        return score->firstMeasure()->findChord(Fraction(0, 1), 0);
    };
    auto step = [score, &chordAtStart](int delta) {
        score->select(chordAtStart(), SelectType::SINGLE);
        // a different note input duration must not matter: the selection decides
        score->inputState().setDuration(DurationType::V_WHOLE);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots"), [&](Transaction& tx) {
            NoteInput::stepDots(tx, score, delta, true);
        });
    };

    ASSERT_TRUE(chordAtStart());
    EXPECT_EQ(chordAtStart()->dots(), 0);

    step(1);
    EXPECT_EQ(chordAtStart()->dots(), 1);
    EXPECT_EQ(chordAtStart()->ticks(), Fraction(3, 8));

    step(1); // pressing twice adds two
    EXPECT_EQ(chordAtStart()->dots(), 2);
    EXPECT_EQ(chordAtStart()->ticks(), Fraction(7, 16));

    step(-1);
    EXPECT_EQ(chordAtStart()->dots(), 1);

    step(-1);
    EXPECT_EQ(chordAtStart()->dots(), 0);
    EXPECT_EQ(chordAtStart()->ticks(), Fraction(1, 4));

    step(-1); // nothing to remove: unchanged
    EXPECT_EQ(chordAtStart()->dots(), 0);
    EXPECT_EQ(chordAtStart()->ticks(), Fraction(1, 4));

    // the note input duration is left as it was
    EXPECT_EQ(score->inputState().duration().type(), DurationType::V_WHOLE);
    EXPECT_EQ(score->inputState().duration().dots(), 0);

    delete score;
}

TEST_F(Engraving_DurationTypeTests, stepDotsLeavesTrillCueNotes)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_QUARTER);
    score->inputState().setNoteEntryMode(true);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots tests"), [&](Transaction&) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);
    });
    score->inputState().setNoteEntryMode(false);

    Chord* chord = score->firstMeasure()->findChord(Fraction(0, 1), 0);
    ASSERT_TRUE(chord);
    chord->setIsTrillCueNote(true);

    // outside note input a selected trill cue note keeps its value (noteValueChangeAllowed)
    score->select(chord, SelectType::SINGLE);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots"), [&](Transaction& tx) {
        NoteInput::stepDots(tx, score, 1, true);
    });
    chord = score->firstMeasure()->findChord(Fraction(0, 1), 0);
    ASSERT_TRUE(chord);
    EXPECT_EQ(chord->dots(), 0);
    EXPECT_EQ(chord->ticks(), Fraction(1, 4));

    delete score;
}

// Fable check of #279: a selection of different lengths keeps them, each one dot longer
TEST_F(Engraving_DurationTypeTests, stepDotsOnMixedSelection)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    // a quarter note at the start of the first measure, a half note at the start of the second
    Measure* m1 = score->firstMeasure();
    Measure* m2 = m1->nextMeasure();
    ASSERT_TRUE(m2);
    score->inputState().setTrack(0);
    score->inputState().setNoteEntryMode(true);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots tests"), [&](Transaction&) {
        score->inputState().setSegment(score->tick2segment(m1->tick(), false, SegmentType::ChordRest));
        score->inputState().setDuration(DurationType::V_QUARTER);
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);
        score->inputState().setSegment(score->tick2segment(m2->tick(), false, SegmentType::ChordRest));
        score->inputState().setDuration(DurationType::V_HALF);
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 44, false, false);
    });
    score->inputState().setNoteEntryMode(false);

    auto quarter = [m1]() { return m1->findChord(m1->tick(), 0); };
    auto half = [score]() {
        Measure* m = score->firstMeasure()->nextMeasure();
        return m->findChord(m->tick(), 0);
    };
    ASSERT_TRUE(quarter() && half());

    auto stepBoth = [&](int delta) {
        score->select(quarter()->upNote(), SelectType::SINGLE);
        score->select(half()->upNote(), SelectType::ADD);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots"), [&](Transaction& tx) {
            NoteInput::stepDots(tx, score, delta, true);
        });
    };

    stepBoth(1);
    EXPECT_EQ(quarter()->ticks(), Fraction(3, 8));
    EXPECT_EQ(half()->ticks(), Fraction(3, 4));

    stepBoth(-1);
    EXPECT_EQ(quarter()->ticks(), Fraction(1, 4));
    EXPECT_EQ(half()->ticks(), Fraction(1, 2));

    delete score;
}

// Fable check of #279: no dot where the duration is too short for one more
TEST_F(Engraving_DurationTypeTests, durationCanHoldDots)
{
    EXPECT_TRUE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_QUARTER), 1));
    EXPECT_TRUE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_QUARTER), 4));
    EXPECT_FALSE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_1024TH), 1));
    EXPECT_FALSE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_512TH), 2));
    EXPECT_TRUE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_512TH), 1));
    EXPECT_FALSE(NoteInput::durationCanHoldDots(TDuration(DurationType::V_QUARTER), 5));
}

// at the maximum number of dots, one more changes nothing
TEST_F(Engraving_DurationTypeTests, stepDotsStopsAtTheMaximum)
{
    MasterScore* score = ScoreRW::readScore(DURATIONTYPE_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);

    score->inputState().setTrack(0);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->inputState().setDuration(DurationType::V_HALF);
    score->inputState().setNoteEntryMode(true);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots tests"), [&](Transaction&) {
        NoteInput::addPitch(score->transactionManager()->currentOrDummyTransaction(), score, 42, false, false);
    });
    score->inputState().setNoteEntryMode(false);

    Chord* c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
    ASSERT_TRUE(c);
    for (int i = 0; i < int(MAX_DOTS) + 2; ++i) {
        score->select(score->firstMeasure()->findChord(Fraction(0, 1), 0), SelectType::SINGLE);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Step dots"), [&](Transaction& tx) {
            NoteInput::stepDots(tx, score, 1, true);
        });
    }
    c = score->firstMeasure()->findChord(Fraction(0, 1), 0);
    ASSERT_TRUE(c);
    EXPECT_EQ(c->dots(), int(MAX_DOTS));

    delete score;
}

