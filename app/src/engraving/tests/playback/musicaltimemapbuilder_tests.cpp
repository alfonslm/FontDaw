/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "engraving/dom/tempotimeline.h"
#include "engraving/playback/musicaltimemapbuilder.h"

using namespace mu::engraving;
using namespace muse::audio;

//! The builder maps the PlayedBeatMap (tested in playedbeatmap_tests.cpp) to the audio engine's
//! MusicalTimeMap; these tests check the result through MusicalTimeMap::positionAt(), as a plugin sees it.
class Engraving_MusicalTimeMapBuilderTests : public ::testing::Test
{
protected:
    static constexpr int QUARTER = 480;
    static constexpr int MEASURE_4_4 = 4 * QUARTER;
    static constexpr double EPS = 1e-9;

    static std::vector<PlayedMeasure> measures(int count, const Fraction& timeSig, int start = 0)
    {
        std::vector<PlayedMeasure> result;
        const int ticks = timeSig.ticks();
        int utick = start;
        for (int i = 0; i < count; ++i) {
            result.push_back({ utick, ticks, timeSig });
            utick += ticks;
        }
        return result;
    }

    static MusicalTimeMap build(const TempoTimeline& timeline, const std::vector<PlayedMeasure>& spans)
    {
        return MusicalTimeMapBuilder::build(timeline.points(), spans, timeline.tempoMultiplier().val);
    }
};

TEST_F(Engraving_MusicalTimeMapBuilderTests, ConstantTempo)
{
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 } }, PausesMap {});
    const MusicalTimeMap map = build(timeline, measures(3, Fraction(4, 4)));

    MusicalPosition pos = map.positionAt(1.0);
    EXPECT_NEAR(pos.quarters, 2.0, EPS);
    EXPECT_NEAR(pos.bpm, 120.0, EPS);
    EXPECT_TRUE(pos.hasBar);
    EXPECT_NEAR(pos.barStartQuarters, 0.0, EPS);

    pos = map.positionAt(2.5);
    EXPECT_NEAR(pos.quarters, 5.0, EPS);
    EXPECT_NEAR(pos.barStartQuarters, 4.0, EPS);
    EXPECT_EQ(pos.timeSigNumerator, 4);
    EXPECT_EQ(pos.timeSigDenominator, 4);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, TempoChange)
{
    //! 120 bpm for one bar, then 60 bpm
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 }, { MEASURE_4_4, 1.0 } }, PausesMap {});
    const MusicalTimeMap map = build(timeline, measures(3, Fraction(4, 4)));

    EXPECT_NEAR(map.positionAt(2.0).quarters, 4.0, EPS);
    const MusicalPosition pos = map.positionAt(3.0);
    EXPECT_NEAR(pos.quarters, 5.0, EPS);
    EXPECT_NEAR(pos.bpm, 60.0, EPS);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, PauseIsHiddenBeats)
{
    //! A 2 s pause at the end of bar 1 at 120 bpm: hidden beats lengthen bar 1, and the beats keep moving
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 } }, PausesMap { { MEASURE_4_4, 2.0 } });
    const std::vector<PlayedMeasure> spans = measures(3, Fraction(4, 4));
    const MusicalTimeMap map = build(timeline, spans);

    //! Never two points with the same quarters: the music does not stand still
    for (size_t i = 1; i < map.tempo.size(); ++i) {
        EXPECT_LT(map.tempo[i - 1].quarters, map.tempo[i].quarters);
    }

    //! Through the pause (2 s to 4 s) the position moves on, inside bar 1
    const MusicalPosition before = map.positionAt(1.9);
    const MusicalPosition during = map.positionAt(3.0);
    EXPECT_GT(during.quarters, before.quarters);
    EXPECT_NEAR(during.barStartQuarters, 0.0, EPS);

    //! Bar 2 starts where the pause ends, played time and the beat map agreeing
    const PlayedBeatMap beats = PlayedBeatMap::build(timeline.points(), spans, 1.0);
    const int bar2 = beats.playedTick(MEASURE_4_4);
    const double bar2Secs = timeline.utick2utime(MEASURE_4_4);
    EXPECT_NEAR(bar2Secs, 4.0, EPS);
    const MusicalPosition atBar2 = map.positionAt(bar2Secs);
    EXPECT_NEAR(atBar2.quarters, bar2 / double(QUARTER), EPS);
    EXPECT_NEAR(atBar2.barStartQuarters, bar2 / double(QUARTER), EPS);
    EXPECT_EQ(atBar2.timeSigNumerator, 4);
    EXPECT_EQ(atBar2.timeSigDenominator, 4);

    //! Bar 1 is told as its played signature (the one the MIDI export writes)
    const int bar1Quarters = bar2 / QUARTER;
    EXPECT_EQ(map.positionAt(1.0).timeSigNumerator * 4 / map.positionAt(1.0).timeSigDenominator, bar1Quarters);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, PlaybackSpeed)
{
    //! At half speed 120 bpm sounds as 60 bpm; the quarters do not change
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 } }, PausesMap {});
    timeline.setTempoMultiplier(0.5);
    const MusicalTimeMap map = build(timeline, measures(2, Fraction(4, 4)));

    const MusicalPosition pos = map.positionAt(2.0);
    EXPECT_NEAR(pos.quarters, 2.0, EPS);
    EXPECT_NEAR(pos.bpm, 60.0, EPS);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, TimeSignatures)
{
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 } }, PausesMap {});
    std::vector<PlayedMeasure> spans = measures(2, Fraction(4, 4));
    for (const PlayedMeasure& m : measures(2, Fraction(3, 4), 2 * MEASURE_4_4)) {
        spans.push_back(m);
    }
    const MusicalTimeMap map = build(timeline, spans);

    const MusicalPosition pos = map.positionAt(5.0); // quarter 10: in the first 3/4 bar (8 to 11)
    EXPECT_NEAR(pos.quarters, 10.0, EPS);
    EXPECT_NEAR(pos.barStartQuarters, 8.0, EPS);
    EXPECT_EQ(pos.timeSigNumerator, 3);
    EXPECT_EQ(pos.timeSigDenominator, 4);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, PickupIsAShortFirstBar)
{
    //! An eighth pickup in 4/4: quarter 0 is the start of the music, the first full bar starts at 0.5
    TempoTimeline timeline;
    timeline.rebuild(TempoValues { { 0, 2.0 } }, PausesMap {});
    std::vector<PlayedMeasure> spans = { { 0, QUARTER / 2, Fraction(4, 4) } };
    for (const PlayedMeasure& m : measures(2, Fraction(4, 4), QUARTER / 2)) {
        spans.push_back(m);
    }
    const MusicalTimeMap map = build(timeline, spans);

    EXPECT_NEAR(map.positionAt(0.1).barStartQuarters, 0.0, EPS);
    const MusicalPosition pos = map.positionAt(1.0); // quarter 2
    EXPECT_NEAR(pos.barStartQuarters, 0.5, EPS);
    EXPECT_EQ(pos.timeSigNumerator, 4);
    EXPECT_EQ(pos.timeSigDenominator, 4);
}

TEST_F(Engraving_MusicalTimeMapBuilderTests, NoTempoPoints)
{
    const MusicalTimeMap map = MusicalTimeMapBuilder::build({}, measures(1, Fraction(4, 4)), 1.0);
    ASSERT_FALSE(map.tempo.empty());
    EXPECT_TRUE(map.positionAt(0.0).isValid());
}
