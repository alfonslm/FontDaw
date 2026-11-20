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

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/system.h"
#include "engraving/tests/utils/scorerw.h"

#include "internal/scorepositions.h"

using namespace mu::arrangement;
using namespace mu::engraving;

// data/repeats_tempo.mscx: three 4/4 bars, a repeat over bars 1-2, 120 BPM, then 60 BPM in bar 3.
// Part 1 has four quarters in bar 1 and a whole note in bar 3, part 2 a note in every bar.
// Played with repeats: bars 1 2 1 2 3 = 0-4 s, 4-8 s, 8-12 s.
class Arrangement_ScorePositionsTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_score = ScoreRW::readScore(u"data/repeats_tempo.mscx");
        ASSERT_TRUE(m_score);
        m_score->doLayout();
    }

    void TearDown() override
    {
        delete m_score;
    }

    MasterScore* m_score = nullptr;
};

TEST_F(Arrangement_ScorePositionsTests, ATickOnASegmentIsThatSegmentsX)
{
    for (const Segment* s = m_score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
        const std::optional<ScoreX> x = tickToCanvasX(m_score, s->tick());
        ASSERT_TRUE(x) << s->tick().ticks();
        EXPECT_DOUBLE_EQ(x->x, s->canvasPos().x()) << s->tick().ticks();
        EXPECT_EQ(x->system, s->measure()->system());
    }
}

TEST_F(Arrangement_ScorePositionsTests, BetweenSegmentsItIsLinearAndBarsOfUnequalWidthAreFollowed)
{
    // Bar 1 has four quarters (a segment per beat); bar 3 one whole note (beats 2-4 have no segment)
    const Measure* bar1 = m_score->firstMeasure();
    const Measure* bar3 = bar1->nextMeasure()->nextMeasure();
    ASSERT_TRUE(bar3);

    const double beat1 = tickToCanvasX(m_score, bar1->tick())->x;
    const double beat2 = tickToCanvasX(m_score, bar1->tick() + Fraction(1, 4))->x;
    const double halfway = tickToCanvasX(m_score, bar1->tick() + Fraction(1, 8))->x;
    EXPECT_NEAR(halfway, (beat1 + beat2) / 2.0, 1e-9);

    // In bar 3 the whole note's segment runs to the end bar line, so its beats are spread evenly
    const double start3 = tickToCanvasX(m_score, bar3->tick())->x;
    const double beat3of3 = tickToCanvasX(m_score, bar3->tick() + Fraction(2, 4))->x;
    const std::optional<ScoreX> end = scoreEndX(m_score);
    ASSERT_TRUE(end);
    EXPECT_NEAR(beat3of3, (start3 + end->x) / 2.0, 1e-9);
    EXPECT_GT(end->x, start3);
}

TEST_F(Arrangement_ScorePositionsTests, OutsideTheScoreThereIsNoX)
{
    EXPECT_FALSE(tickToCanvasX(m_score, m_score->lastMeasure()->endTick()));
    EXPECT_FALSE(tickToCanvasX(m_score, Fraction(-1, 4)));
    EXPECT_FALSE(tickToCanvasX(nullptr, Fraction(0, 1)));
}

TEST_F(Arrangement_ScorePositionsTests, SecondsMapToWrittenTicksThroughRepeatsAndTempo)
{
    // 1 s at 120 BPM is two quarters into bar 1
    EXPECT_EQ(secsToWrittenTick(m_score, 1.0, true), Fraction(2, 4));
    // 5 s is the second pass, one second into bar 1 again: the same written place
    EXPECT_EQ(secsToWrittenTick(m_score, 5.0, true), Fraction(2, 4));
    // 9 s is one second into bar 3, at 60 BPM: one quarter
    EXPECT_EQ(secsToWrittenTick(m_score, 9.0, true), Fraction(2, 1) + Fraction(1, 4));
    // Past the end of the played score
    EXPECT_FALSE(secsToWrittenTick(m_score, 12.5, true));
    EXPECT_FALSE(secsToWrittenTick(m_score, -0.1, true));

    // Without repeats: bars 1 2 3 = 0-4 s, 4-8 s; 5 s is one quarter into bar 3
    EXPECT_EQ(secsToWrittenTick(m_score, 5.0, false), Fraction(2, 1) + Fraction(1, 4));
    EXPECT_FALSE(secsToWrittenTick(m_score, 8.5, false));
}

// Review of #167, finding 9: a clip that starts in the first pass and runs on into the second is
// drawn to the end of the repeat
TEST_F(Arrangement_ScorePositionsTests, ThePassEndsWhereTheRepeatJumpsBack)
{
    // 1 s is in the first pass of bars 1-2, which ends at 4 s with the jump back
    const std::optional<PassEnd> first = passEndAt(m_score, 1.0, true);
    ASSERT_TRUE(first);
    EXPECT_NEAR(first->endSecs, 4.0, 1e-9);
    EXPECT_EQ(first->endTick, Fraction(2, 1)); // the end of bar 2

    // 5 s is in the second pass, which runs on into bar 3: no jump back follows
    EXPECT_FALSE(passEndAt(m_score, 5.0, true));
    // 9 s is in bar 3, the last
    EXPECT_FALSE(passEndAt(m_score, 9.0, true));
    // Without repeats played nothing jumps back
    EXPECT_FALSE(passEndAt(m_score, 1.0, false));
}

// data/systems_breaks.mscx: one staff, 4/4, 120 BPM (a bar is 2 s), nine bars of quarters, a line
// break after bar 3 and a page break after bar 6: systems 1-3 hold bars 1-3, 4-6 and 7-9, and
// system 3 is on the second page.
namespace {
// The x of a measure's end bar line, as tickToCanvasX takes it
double endBarX(const Measure* m)
{
    const Segment* bar = m->findSegment(SegmentType::EndBarLine, m->endTick());
    return bar ? bar->canvasPos().x() : m->canvasPos().x() + m->width();
}

double lastMeasureEndX(const System* system)
{
    return endBarX(system->lastMeasure());
}
}

class Arrangement_ClipPiecesTests : public ::testing::Test
{
protected:
    void load(LayoutMode mode, const muse::String& file = u"data/systems_breaks.mscx")
    {
        m_score = ScoreRW::readScore(file);
        ASSERT_TRUE(m_score);
        m_score->setLayoutMode(mode);
        m_score->doLayout();
    }

    void TearDown() override
    {
        delete m_score;
    }

    MasterScore* m_score = nullptr;
};

TEST_F(Arrangement_ClipPiecesTests, TheScoreLaysOutAsThreeSystemsOnTwoPages)
{
    load(LayoutMode::PAGE);
    ASSERT_EQ(m_score->systems().size(), 3u);
    EXPECT_EQ(m_score->systems()[0]->page(), m_score->systems()[1]->page());
    EXPECT_NE(m_score->systems()[1]->page(), m_score->systems()[2]->page());

    const std::optional<SystemTicks> second = systemTicks(m_score->systems()[1]);
    ASSERT_TRUE(second);
    EXPECT_EQ(second->from, Fraction(3, 1));
    EXPECT_EQ(second->to, Fraction(6, 1));
    EXPECT_FALSE(systemTicks(nullptr));
}

TEST_F(Arrangement_ClipPiecesTests, AClipInsideOneSystemIsOnePiece)
{
    load(LayoutMode::PAGE);
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 0.5, 3.0, false);
    ASSERT_EQ(pieces.size(), 1u);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_FALSE(pieces[0].continuesRight);
    EXPECT_EQ(pieces[0].system, m_score->systems()[0]);
    EXPECT_DOUBLE_EQ(pieces[0].x0, tickToCanvasX(m_score, *secsToWrittenTick(m_score, 0.5, false))->x);
    EXPECT_DOUBLE_EQ(pieces[0].x1, tickToCanvasX(m_score, *secsToWrittenTick(m_score, 3.0, false))->x);
}

TEST_F(Arrangement_ClipPiecesTests, AClipAcrossASystemBreakIsTwoPieces)
{
    load(LayoutMode::PAGE);
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 4.0, 8.0, false);
    ASSERT_EQ(pieces.size(), 2u);

    const System* s1 = m_score->systems()[0];
    const System* s2 = m_score->systems()[1];
    EXPECT_EQ(pieces[0].system, s1);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_TRUE(pieces[0].continuesRight);
    EXPECT_DOUBLE_EQ(pieces[0].x0, tickToCanvasX(m_score, Fraction(2, 1))->x);
    EXPECT_DOUBLE_EQ(pieces[0].x1, lastMeasureEndX(s1));

    EXPECT_EQ(pieces[1].system, s2);
    EXPECT_TRUE(pieces[1].continuesLeft);
    EXPECT_FALSE(pieces[1].continuesRight);
    // after the system's clefs: where its first notes start
    EXPECT_DOUBLE_EQ(pieces[1].x0, tickToCanvasX(m_score, s2->firstMeasure()->tick())->x);
    EXPECT_GT(pieces[1].x0, s2->firstMeasure()->canvasPos().x());
    // 8 s is the end of bar 4: the piece ends at that bar line, not at the start of bar 5
    EXPECT_DOUBLE_EQ(pieces[1].x1, endBarX(s2->firstMeasure()));
}

TEST_F(Arrangement_ClipPiecesTests, AClipAcrossAllSystemsHasAMiddlePieceWithBothFlags)
{
    load(LayoutMode::PAGE);
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 1.0, 17.0, false);
    ASSERT_EQ(pieces.size(), 3u);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_TRUE(pieces[0].continuesRight);
    EXPECT_TRUE(pieces[1].continuesLeft);
    EXPECT_TRUE(pieces[1].continuesRight);
    EXPECT_TRUE(pieces[2].continuesLeft);
    EXPECT_FALSE(pieces[2].continuesRight);
    EXPECT_EQ(pieces[0].system->page(), pieces[1].system->page());
    EXPECT_NE(pieces[1].system->page(), pieces[2].system->page());
}

TEST_F(Arrangement_ClipPiecesTests, AClipPastTheEndIsCutAtTheEndOfTheScore)
{
    load(LayoutMode::PAGE);
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 16.0, 30.0, false);
    ASSERT_EQ(pieces.size(), 1u);
    EXPECT_EQ(pieces[0].system, m_score->systems()[2]);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_TRUE(pieces[0].continuesRight);
    EXPECT_DOUBLE_EQ(pieces[0].x1, scoreEndX(m_score)->x);

    const std::vector<ClipPiece> wider = clipPieces(m_score, 10.0, 30.0, false);
    ASSERT_EQ(wider.size(), 2u);
    EXPECT_EQ(wider[1].system, m_score->systems()[2]);
    EXPECT_TRUE(wider[1].continuesRight);
    EXPECT_DOUBLE_EQ(wider[1].x1, scoreEndX(m_score)->x);
}

// Review of #196: a clip that ends exactly at the final bar line is not "past the end"
TEST_F(Arrangement_ClipPiecesTests, AClipEndingExactlyAtTheEndOfTheScoreIsOnePieceThatDoesNotContinue)
{
    load(LayoutMode::PAGE);
    ASSERT_EQ(m_score->lastMeasure()->endTick(), Fraction(9, 1)); // 9 bars of 2 s = 18 s
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 15.0, 18.0, false);
    ASSERT_EQ(pieces.size(), 1u);
    EXPECT_EQ(pieces[0].system, m_score->systems()[2]);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_FALSE(pieces[0].continuesRight);
    EXPECT_DOUBLE_EQ(pieces[0].x1, endBarX(m_score->lastMeasure()));
}

// A title frame (or any vertical frame) after the last bar is a system of its own, with no
// measures: a clip's last piece is on the last measure's system, never on the frame's
TEST_F(Arrangement_ClipPiecesTests, ATrailingVerticalFrameGetsNoPiece)
{
    load(LayoutMode::PAGE, u"data/systems_breaks_vbox.mscx");
    ASSERT_FALSE(m_score->systems().empty());
    const System* frameSystem = m_score->systems().back();
    ASSERT_TRUE(frameSystem);
    EXPECT_TRUE(frameSystem->vbox()) << "the frame must be a system of its own";
    const System* lastMeasureSystem = m_score->lastMeasure()->system();
    ASSERT_TRUE(lastMeasureSystem);
    EXPECT_NE(frameSystem, lastMeasureSystem);

    // A clip past the end
    const std::vector<ClipPiece> past = clipPieces(m_score, 16.0, 30.0, false);
    ASSERT_FALSE(past.empty());
    EXPECT_EQ(past.back().system, lastMeasureSystem);
    EXPECT_NE(past.back().system, m_score->systems().back());
    EXPECT_TRUE(past.back().continuesRight);
    EXPECT_DOUBLE_EQ(past.back().x1, scoreEndX(m_score)->x);

    // And one across the whole score, ending exactly at its end
    for (const std::vector<ClipPiece>& pieces : { past, clipPieces(m_score, 0.0, 18.0, false) }) {
        for (const ClipPiece& piece : pieces) {
            EXPECT_NE(piece.system, frameSystem);
        }
    }
}

TEST_F(Arrangement_ClipPiecesTests, AClipOutsideTheScoreHasNoPieces)
{
    load(LayoutMode::PAGE);
    EXPECT_TRUE(clipPieces(m_score, 40.0, 50.0, false).empty());
    EXPECT_TRUE(clipPieces(m_score, 5.0, 5.0, false).empty());
    EXPECT_TRUE(clipPieces(nullptr, 0.0, 1.0, false).empty());
}

TEST_F(Arrangement_ClipPiecesTests, InContinuousViewTheBreaksAreIgnored)
{
    load(LayoutMode::LINE);
    ASSERT_EQ(m_score->systems().size(), 1u);
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 4.0, 8.0, false);
    ASSERT_EQ(pieces.size(), 1u);
    EXPECT_FALSE(pieces[0].continuesLeft);
    EXPECT_FALSE(pieces[0].continuesRight);
}

TEST_F(Arrangement_ClipPiecesTests, AClipIsCutAtTheEndOfTheRepeatPass)
{
    // repeats_tempo.mscx: bars 1-2 repeated, played bars 1 2 1 2 3 = 0-4 s, 4-8 s, 8-12 s
    delete m_score;
    m_score = ScoreRW::readScore(u"data/repeats_tempo.mscx");
    ASSERT_TRUE(m_score);
    m_score->doLayout();

    // From 1 s in the first pass, running on to 10 s: cut at the end of bar 2 (written tick 2 whole notes)
    const std::vector<ClipPiece> pieces = clipPieces(m_score, 1.0, 10.0, true);
    ASSERT_FALSE(pieces.empty());
    const Measure* bar2 = m_score->firstMeasure()->nextMeasure();
    EXPECT_NEAR(pieces.back().x1, endBarX(bar2), 1e-9);
    EXPECT_LT(pieces.back().x1, scoreEndX(m_score)->x);
    EXPECT_FALSE(pieces.back().continuesRight);

    // From the second pass the clip runs on into bar 3, to the end of the score
    const std::vector<ClipPiece> second = clipPieces(m_score, 5.0, 11.0, true);
    ASSERT_FALSE(second.empty());
    EXPECT_GT(second.back().x1, endBarX(bar2));
}

// FD-167: the painter's faster helpers must give exactly what the general functions give

TEST_F(Arrangement_ClipPiecesTests, SystemTickToXGivesTickToCanvasXWithinItsSystem)
{
    load(LayoutMode::PAGE);
    const Fraction end = m_score->lastMeasure()->endTick();
    int compared = 0;
    for (const System* system : m_score->systems()) {
        const SystemTickToX systemX(system);
        // Every 1/64 note over the whole score, and one tick past the end
        for (Fraction tick(0, 1); tick <= end; tick += Fraction(1, 64)) {
            const std::optional<ScoreX> general = tickToCanvasX(m_score, tick);
            const std::optional<double> fast = systemX(tick);
            if (general && general->system == system) {
                ASSERT_TRUE(fast) << "tick " << tick.ticks();
                EXPECT_DOUBLE_EQ(*fast, general->x) << "tick " << tick.ticks();
                ++compared;
            } else {
                EXPECT_FALSE(fast) << "a tick outside the system has no x in it: " << tick.ticks();
            }
        }
    }
    EXPECT_GT(compared, 500);
}

TEST_F(Arrangement_ClipPiecesTests, SecsToWrittenTickGivesTheSameAsTheFunction)
{
    for (const bool expand : { false, true }) {
        delete m_score;
        m_score = ScoreRW::readScore(u"data/repeats_tempo.mscx");
        ASSERT_TRUE(m_score);
        m_score->doLayout();
        const SecsToWrittenTick toTick(m_score, expand);
        for (double secs = -1.0; secs < 16.0; secs += 0.0625) {
            EXPECT_EQ(toTick(secs), secsToWrittenTick(m_score, secs, expand)) << secs << " s, repeats " << expand;
        }
    }
}

TEST_F(Arrangement_ClipPiecesTests, OnePagesPiecesAreThoseOfTheWholeScoreOnThatPage)
{
    load(LayoutMode::PAGE);
    ASSERT_GE(m_score->pages().size(), 2u);
    const std::vector<ClipPiece> all = clipPieces(m_score, 0.5, 30.0, false); // across every system
    ASSERT_GE(all.size(), 3u);
    for (const Page* page : m_score->pages()) {
        const std::vector<ClipPiece> onPage = clipPieces(m_score, 0.5, 30.0, false, page);
        std::vector<ClipPiece> expected;
        for (const ClipPiece& piece : all) {
            if (piece.system->page() == page) {
                expected.push_back(piece);
            }
        }
        ASSERT_EQ(onPage.size(), expected.size());
        for (size_t i = 0; i < onPage.size(); ++i) {
            EXPECT_EQ(onPage[i].system, expected[i].system);
            EXPECT_DOUBLE_EQ(onPage[i].x0, expected[i].x0);
            EXPECT_DOUBLE_EQ(onPage[i].x1, expected[i].x1);
            EXPECT_EQ(onPage[i].continuesLeft, expected[i].continuesLeft);
            EXPECT_EQ(onPage[i].continuesRight, expected[i].continuesRight);
        }
    }
}
