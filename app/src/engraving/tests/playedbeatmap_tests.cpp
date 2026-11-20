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

#include <vector>

#include "engraving/dom/playedbeatmap.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/types/constants.h"

using namespace mu::engraving;

static constexpr double SECS_ERROR = 1e-9;

class Engraving_PlayedBeatMapTests : public ::testing::Test
{
};

//! `count` measures of the same time signature, back to back from utick `start`
static std::vector<PlayedMeasure> makeMeasures(int count, const Fraction& timeSig, int start = 0)
{
    std::vector<PlayedMeasure> measures;
    for (int i = 0; i < count; ++i) {
        measures.push_back({ start + i * timeSig.ticks(), timeSig.ticks(), timeSig });
    }
    return measures;
}

static PlayedBeatMap buildMap(const TempoTimeline& tl, const std::vector<PlayedMeasure>& measures)
{
    return PlayedBeatMap::build(tl.points(), measures, tl.tempoMultiplier().val);
}

//! At every measure start and every 120 ticks across the piece, the map and the timeline agree on the time
//! (a pause has no extent in uticks, so no utick lies strictly inside one)
static void expectSameTimes(const PlayedBeatMap& map, const TempoTimeline& tl, const std::vector<PlayedMeasure>& measures)
{
    std::vector<int> uticks;
    for (const PlayedMeasure& m : measures) {
        uticks.push_back(m.utick);
    }
    const int end = measures.back().utick + measures.back().ticks;
    for (int u = 0; u <= end; u += 120) {
        uticks.push_back(u);
    }

    for (int u : uticks) {
        EXPECT_NEAR(map.secsAt(map.playedTick(u)), tl.utick2utime(u), SECS_ERROR) << "utick " << u;
    }
}

static bool sameSig(const Fraction& a, const Fraction& b)
{
    return a.numerator() == b.numerator() && a.denominator() == b.denominator();
}

TEST_F(Engraving_PlayedBeatMapTests, NO_PAUSE)
{
    // 4/4 at 120 bpm (2 bps), 4 bars: nothing hidden, one bar point, one tempo point
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap {});
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    EXPECT_TRUE(map.hiddenBeats().empty());
    ASSERT_EQ(map.tempo().size(), 1u);
    EXPECT_EQ(map.tempo()[0].tick, 0);
    EXPECT_NEAR(map.tempo()[0].bps, 2.0, SECS_ERROR);
    ASSERT_EQ(map.bars().size(), 1u);
    EXPECT_EQ(map.bars()[0].tick, 0);
    EXPECT_EQ(map.bars()[0].ticks, 1920);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 0);
    EXPECT_TRUE(map.bars()[0].isRegular());
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(4, 4)));
    EXPECT_EQ(map.playedTick(3000), 3000);
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, BREATH_AT_BAR_LINE)
{
    // Pause 0.5 s at utick 1920 = 0.5 * 2 bps * 480 = 480 ticks = 1 beat, added to bar 1 (it ends there).
    // Hidden beats: played ticks 1920..2400, tempo 480 / 480 / 0.5 = 2 bps, the tempo before: no tempo point
    // is needed for them. Bar 1 = 5/4 (2400 ticks); bar 2 starts at played tick 2400 and gets a point back
    // at 4/4; bars 3 and 4 need none.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.5 } });
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].utick, 1920);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 1920);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 0.5, SECS_ERROR);

    // one tempo point (tick 0, 2 bps, 0 s): points that do not change the tempo are left out
    ASSERT_EQ(map.tempo().size(), 1u);
    EXPECT_NEAR(map.tempo()[0].bps, 2.0, SECS_ERROR);
    EXPECT_NEAR(map.secsAt(1920), 2.0, SECS_ERROR);
    EXPECT_NEAR(map.secsAt(2400), 2.5, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].tick, 0);
    EXPECT_EQ(map.bars()[0].ticks, 2400);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 480);
    EXPECT_FALSE(map.bars()[0].isRegular());
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(5, 4)));
    EXPECT_EQ(map.bars()[1].tick, 2400);
    EXPECT_EQ(map.bars()[1].ticks, 1920);
    EXPECT_TRUE(map.bars()[1].isRegular());
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(4, 4)));

    EXPECT_EQ(map.playedTick(1919), 1919);
    EXPECT_EQ(map.playedTick(1920), 2400);
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, CAESURA_MID_BAR)
{
    // Pause 2 s at utick 960 (bar 1) = 2 * 2 * 480 = 1920 ticks = 4 beats: bar 1 = 3840 ticks = 8/4,
    // hidden tempo 1920 / 480 / 2 = 2 bps. Bar 2 starts at played tick 1920 + 1920 = 3840 (4/4 again).
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 960, 2.0 } });
    const auto measures = makeMeasures(3, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 960);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 1920);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 2.0, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 3840);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 1920);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(8, 4)));
    EXPECT_EQ(map.bars()[1].tick, 3840);
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(4, 4)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, SECTION_BREAK_PAUSE_AT_NEXT_BAR_START)
{
    // Pause 1.5 s at utick 3840, the start of bar 3, so it belongs to bar 2 (ends there):
    // 1.5 * 2 * 480 = 1440 ticks = 3 beats, bar 2 = 3360 ticks = 7/4. Bar points: bar 1 (start, tick 0),
    // bar 2 (tick 1920, 7/4), bar 3 (tick 3840 + 1440 = 5280, 4/4). Bar 4 needs none.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 3840, 1.5 } });
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].utick, 3840);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 3840);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 1440);

    ASSERT_EQ(map.bars().size(), 3u);
    EXPECT_EQ(map.bars()[0].tick, 0);
    EXPECT_TRUE(map.bars()[0].isRegular());
    EXPECT_EQ(map.bars()[1].tick, 1920);
    EXPECT_EQ(map.bars()[1].ticks, 3360);
    EXPECT_EQ(map.bars()[1].hiddenTicks, 1440);
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(7, 4)));
    EXPECT_EQ(map.bars()[2].tick, 5280);
    EXPECT_TRUE(map.bars()[2].isRegular());
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, SHORT_BREATH_USES_SMALLER_UNIT)
{
    // Pause 0.1 s at 2 bps = 96 ticks (0.2 beats). The unit halves from 480 to 240 (96 < 360) and to 120
    // (96 < 180); 96 < 90 is false, so it stays at 120: round(96 / 120) = 1 unit = 120 ticks (a 16th).
    // Bar 1 = 1920 + 120 = 2040 ticks = 17/16. Hidden tempo = 120 / 480 / 0.1 = 2.5 bps, inside 2/3..1 1/3 of 2.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.1 } });
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 120);
    ASSERT_EQ(map.tempo().size(), 3u);
    EXPECT_NEAR(map.tempo()[1].bps, 2.5, SECS_ERROR);
    EXPECT_NEAR(map.tempo()[2].bps, 2.0, SECS_ERROR);
    EXPECT_EQ(map.tempo()[2].tick, 2040);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 2040);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(17, 16)));
    EXPECT_EQ(map.bars()[1].tick, 2040);
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(4, 4)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, PICKUP_WITH_PAUSE)
{
    // First bar is a one-quarter pickup (480 ticks) in 4/4; pause 0.5 s at utick 480 ends it:
    // 480 ticks hidden, so the pickup is 960 ticks = 2/4, hidden tempo 2 bps. The next bar starts at utick 480,
    // played tick 960, with a point back at 4/4. Without the pause the pickup alone would be 1/4.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 480, 0.5 } });
    std::vector<PlayedMeasure> measures = { { 0, 480, Fraction(4, 4) } };
    for (const PlayedMeasure& m : makeMeasures(3, Fraction(4, 4), 480)) {
        measures.push_back(m);
    }

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 480);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].tick, 0);
    EXPECT_EQ(map.bars()[0].ticks, 960);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 480);
    EXPECT_TRUE(sameSig(map.bars()[0].timeSig, Fraction(4, 4)));
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(2, 4)));
    EXPECT_EQ(map.bars()[1].tick, 960);
    EXPECT_TRUE(map.bars()[1].isRegular());
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(4, 4)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, PAUSE_ON_EVERY_REPEAT_PASS)
{
    // Bars A B A B, as a repeat plays them, at uticks 0, 1920, 3840, 5760. Pauses of 0.5 s (480 ticks) at 1920
    // (ends the first A) and 5760 (ends the second A), at 2 bps.
    // Played ticks: A 0..2400 (5/4); B starts at 2400; second A starts at 3840 + 480 = 4320 and is 5/4;
    // second B starts at 5760 + 960 = 6720. Hidden beats: (1920 -> 1920, 480) and (5760 -> 6240, 480).
    // Bar points at 0, 2400, 4320, 6720.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.5 }, { 5760, 0.5 } });
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 2u);
    EXPECT_EQ(map.hiddenBeats()[0].utick, 1920);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 1920);
    EXPECT_EQ(map.hiddenBeats()[1].utick, 5760);
    EXPECT_EQ(map.hiddenBeats()[1].tick, 6240);
    EXPECT_EQ(map.hiddenBeats()[1].ticks, 480);

    ASSERT_EQ(map.bars().size(), 4u);
    EXPECT_EQ(map.bars()[0].tick, 0);
    EXPECT_EQ(map.bars()[0].ticks, 2400);
    EXPECT_EQ(map.bars()[1].tick, 2400);
    EXPECT_TRUE(map.bars()[1].isRegular());
    EXPECT_EQ(map.bars()[2].tick, 4320);
    EXPECT_EQ(map.bars()[2].ticks, 2400);
    EXPECT_TRUE(sameSig(map.bars()[2].playedTimeSig, Fraction(5, 4)));
    EXPECT_EQ(map.bars()[3].tick, 6720);
    EXPECT_TRUE(map.bars()[3].isRegular());
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, TWO_PAUSES_IN_ONE_BAR)
{
    // Pauses at utick 480 (0.5 s = 480 ticks) and 1440 (1 s = 960 ticks), both in bar 1: it is
    // 1920 + 480 + 960 = 3360 ticks = 7/4. Hidden beats: (480 -> 480, 480) and (1440 -> 480 + 480 + 960 = 1920, 960).
    // Bar 2 starts at played tick 1920 + 1440 = 3360.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 480, 0.5 }, { 1440, 1.0 } });
    const auto measures = makeMeasures(2, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 2u);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 480);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);
    EXPECT_EQ(map.hiddenBeats()[1].utick, 1440);
    EXPECT_EQ(map.hiddenBeats()[1].tick, 1920);
    EXPECT_EQ(map.hiddenBeats()[1].ticks, 960);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 3360);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 1440);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(7, 4)));
    EXPECT_EQ(map.bars()[1].tick, 3360);
    EXPECT_TRUE(map.bars()[1].isRegular());
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, PLAYBACK_SPEED_HALF)
{
    // The breath case at 50 % speed: the played ticks are those of the 100 % case (the gap is worked out on
    // the nominal tempo), tempi are halved (1 bps), the pause lasts 0.5 / 0.5 = 1 s as played.
    // Times: 1920 ticks at 1 bps = 4 s, then 480 ticks = 1 s of hidden beats: 5 s = (2 + 0.5) / 0.5.
    // The hidden beats are at the same tempo as the rest, so there is one tempo point.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.5 } });
    tl.setTempoMultiplier(0.5);
    const auto measures = makeMeasures(4, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 1920);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 1.0, SECS_ERROR);

    ASSERT_EQ(map.tempo().size(), 1u);
    EXPECT_NEAR(map.tempo()[0].bps, 1.0, SECS_ERROR);
    EXPECT_NEAR(map.secsAt(2400), 5.0, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 2400);
    EXPECT_EQ(map.bars()[1].tick, 2400);
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, TEMPO_CHANGE_AT_THE_PAUSE)
{
    // 2 bps before utick 1920, 3 bps from there, and a 0.5 s pause at 1920: the hidden beats are sized on the
    // tempo before the pause (the pause interrupts the music at 2 bps): 0.5 * 2 * 480 = 480 ticks = 1 beat,
    // hidden tempo 480 / 480 / 0.5 = 2 bps, which is the tempo before, so it needs no point of its own.
    // The tempo from the pause on, 3 bps, starts after the hidden beats: tempo points (0, 2 bps), (2400, 3 bps).
    // Bar 1 = 2400 ticks = 5/4; bar 2 starts at played tick 2400. Times: 1920 ticks at 2 bps = 2 s,
    // 480 ticks of hidden beats = 0.5 s: 2.5 s at tick 2400.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 }, { 1920, 3.0 } }, PausesMap { { 1920, 0.5 } });
    const auto measures = makeMeasures(3, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 0.5, SECS_ERROR);

    ASSERT_EQ(map.tempo().size(), 2u);
    EXPECT_EQ(map.tempo()[0].tick, 0);
    EXPECT_NEAR(map.tempo()[0].bps, 2.0, SECS_ERROR);
    EXPECT_EQ(map.tempo()[1].tick, 2400);
    EXPECT_NEAR(map.tempo()[1].bps, 3.0, SECS_ERROR);
    EXPECT_NEAR(map.tempo()[1].secs, 2.5, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 2400);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(5, 4)));
    EXPECT_EQ(map.bars()[1].tick, 2400);
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, PAUSE_BEFORE_THE_FIRST_TICK_IS_LEFT_OUT)
{
    // A pause at utick 0 belongs to no bar: nothing is hidden and the bars are regular,
    // but its time is kept, as playback keeps it: the music starts 0.5 s in
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 0, 0.5 } });
    const auto measures = makeMeasures(2, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    EXPECT_TRUE(map.hiddenBeats().empty());
    ASSERT_EQ(map.tempo().size(), 1u);
    EXPECT_EQ(map.tempo()[0].tick, 0);
    EXPECT_NEAR(map.tempo()[0].bps, 2.0, SECS_ERROR);
    ASSERT_EQ(map.bars().size(), 1u);
    EXPECT_TRUE(map.bars()[0].isRegular());
    EXPECT_EQ(map.playedTick(1920), 1920);
    EXPECT_NEAR(map.tempo()[0].secs, 0.5, SECS_ERROR);
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, END_TICK_AT_A_PAUSE)
{
    // Breath 0.5 s at utick 1920 (1 hidden beat, played ticks 1920..2400). Something that starts there
    // starts after the hidden beat; something that ends there ends as HOLD_NOTES_THROUGH_PAUSES says.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.5 } });
    const auto measures = makeMeasures(2, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    EXPECT_EQ(map.playedTick(1920), 2400);
    EXPECT_EQ(map.playedEndTick(1920), HOLD_NOTES_THROUGH_PAUSES ? 2400 : 1920);
    // away from a pause, both are the same
    EXPECT_EQ(map.playedEndTick(960), 960);
    EXPECT_EQ(map.playedEndTick(2880), 3360);
}

TEST_F(Engraving_PlayedBeatMapTests, PAUSE_IN_COMPOUND_METER)
{
    // 6/8: the beat unit of the hidden beats is an eighth (240 ticks), a bar is 1440 ticks. A 0.25 s pause at
    // 2 bps (a quarter lasts 0.5 s) is exactly one eighth: 0.25 * 2 * 480 = 240 ticks, at the end of bar 1
    // (utick 1440). The bar becomes 1680 ticks = 7/8; the hidden tempo is 240 / 480 / 0.25 = 2 bps.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1440, 0.25 } });
    const auto measures = makeMeasures(3, Fraction(6, 8));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].utick, 1440);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 1440);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 240);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 0.25, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 1680);
    EXPECT_EQ(map.bars()[0].hiddenTicks, 240);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(7, 8)));
    EXPECT_EQ(map.bars()[1].tick, 1680);
    EXPECT_TRUE(map.bars()[1].isRegular());
    EXPECT_TRUE(sameSig(map.bars()[1].playedTimeSig, Fraction(6, 8)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, PAUSE_IN_A_RITARDANDO)
{
    // The tempo falls by 0.1 bps every 480 ticks, from 2 bps at utick 0 to 1.4 at 2880, and a 0.5 s pause sits
    // at utick 1920, the end of bar 1. The tempo before the pause is that of the last point before its tick,
    // 1.7 bps (at 1440): 0.5 * 1.7 * 480 = 408 ticks; the unit stays a beat (408 >= 360): round(408 / 480) = 1 beat
    // = 480 ticks, at 480 / 480 / 0.5 = 2 bps, inside 2/3..1 1/3 of 1.7. The tempo points that follow move by 480 ticks:
    // (1920, 2 bps), (2400, 1.6 bps: the tempo from the pause on), (2880, 1.5: utick 2400), (3360, 1.4: utick 2880).
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 }, { 480, 1.9 }, { 960, 1.8 }, { 1440, 1.7 }, { 1920, 1.6 }, { 2400, 1.5 }, { 2880, 1.4 } },
               PausesMap { { 1920, 0.5 } });
    const auto measures = makeMeasures(3, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].tick, 1920);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 480);

    ASSERT_EQ(map.tempo().size(), 8u);
    EXPECT_EQ(map.tempo()[3].tick, 1440);
    EXPECT_NEAR(map.tempo()[3].bps, 1.7, SECS_ERROR);
    EXPECT_EQ(map.tempo()[4].tick, 1920);
    EXPECT_NEAR(map.tempo()[4].bps, 2.0, SECS_ERROR);
    EXPECT_GT(map.tempo()[4].bps, 1.7 * 2.0 / 3.0);
    EXPECT_LT(map.tempo()[4].bps, 1.7 * 4.0 / 3.0);
    EXPECT_EQ(map.tempo()[5].tick, 2400);
    EXPECT_NEAR(map.tempo()[5].bps, 1.6, SECS_ERROR);
    EXPECT_EQ(map.tempo()[6].tick, 2880);
    EXPECT_NEAR(map.tempo()[6].bps, 1.5, SECS_ERROR);
    EXPECT_EQ(map.tempo()[7].tick, 3360);
    EXPECT_NEAR(map.tempo()[7].bps, 1.4, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(5, 4)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, VERY_SHORT_PAUSE_IS_LIMITED_BY_THE_SHORTEST_UNIT)
{
    // 0.01 s at 2 bps = 9.6 ticks, less than a 64th note (30 ticks, the shortest unit): the gap is one 64th, so
    // the hidden tempo is 30 / 480 / 0.01 = 6.25 bps, far outside 2/3..1 1/3 of 2 bps. This is the documented limit:
    // the time is right, the tempo of the hidden beats is not close to the surrounding one. Bar 1 = 1950 ticks = 65/64.
    TempoTimeline tl;
    tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 0.01 } });
    const auto measures = makeMeasures(3, Fraction(4, 4));

    const PlayedBeatMap map = buildMap(tl, measures);

    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].ticks, 30);
    EXPECT_NEAR(map.hiddenBeats()[0].secs, 0.01, SECS_ERROR);

    ASSERT_EQ(map.tempo().size(), 3u);
    EXPECT_EQ(map.tempo()[1].tick, 1920);
    EXPECT_NEAR(map.tempo()[1].bps, 6.25, SECS_ERROR);
    EXPECT_GT(map.tempo()[1].bps, 2.0 * 4.0 / 3.0);
    EXPECT_EQ(map.tempo()[2].tick, 1950);
    EXPECT_NEAR(map.tempo()[2].bps, 2.0, SECS_ERROR);

    ASSERT_EQ(map.bars().size(), 2u);
    EXPECT_EQ(map.bars()[0].ticks, 1950);
    EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(65, 64)));
    expectSameTimes(map, tl, measures);
}

TEST_F(Engraving_PlayedBeatMapTests, LONG_PAUSE_FALLS_BACK_TO_A_LOWER_DENOMINATOR)
{
    // A 4/4 bar lengthened to 284 beats (140 s at 2 bps = 280 hidden beats): the numerator 284 is above 255, so not 284/4;
    // the first lower denominator that fits is 2: 142/2.
    {
        TempoTimeline tl;
        tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 140.0 } });
        const auto measures = makeMeasures(2, Fraction(4, 4));

        const PlayedBeatMap map = buildMap(tl, measures);

        ASSERT_EQ(map.bars().size(), 2u);
        EXPECT_EQ(map.bars()[0].ticks, 284 * 480);
        EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(142, 2)));
        expectSameTimes(map, tl, measures);
    }

    // 600 beats (298 s: 596 hidden beats): 600/4 and 300/2 are above 255, 150/1 fits
    {
        TempoTimeline tl;
        tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 298.0 } });
        const auto measures = makeMeasures(2, Fraction(4, 4));

        const PlayedBeatMap map = buildMap(tl, measures);

        ASSERT_EQ(map.bars().size(), 2u);
        EXPECT_EQ(map.bars()[0].ticks, 600 * 480);
        EXPECT_TRUE(sameSig(map.bars()[0].playedTimeSig, Fraction(150, 1)));
    }

    // 1001 beats (997 hidden beats = 498.5 s): odd, so only 1001/4 would fit and 1001 > 255: no time signature
    {
        TempoTimeline tl;
        tl.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { 1920, 498.5 } });
        const auto measures = makeMeasures(2, Fraction(4, 4));

        const PlayedBeatMap map = buildMap(tl, measures);

        ASSERT_EQ(map.bars().size(), 2u);
        EXPECT_EQ(map.bars()[0].ticks, 1001 * 480);
        EXPECT_FALSE(map.bars()[0].playedTimeSig.isValid());
    }
}
