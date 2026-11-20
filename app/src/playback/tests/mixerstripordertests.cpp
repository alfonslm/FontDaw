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

#include "playback/internal/mixerstriporder.h"

using namespace mu::playback;
using namespace mu::engraving;

namespace {
using Kind = MixerStripRef::Kind;

InstrumentTrackId track(int n)
{
    return InstrumentTrackId { muse::ID(n), muse::String(u"instr") };
}

MixerStripRef instrument(int n)
{
    return { Kind::Instrument, track(n) };
}

MixerStripRef other()
{
    return { Kind::Other, InstrumentTrackId() };
}

const std::vector<InstrumentTrackId> SCORE = { track(1), track(2), track(3), track(4) };
const InstrumentTrackId METRONOME = InstrumentTrackId { muse::ID(999), muse::String(u"metronome") };
}

// The strips of the instruments the engine has added so far, then aux and master; the metronome is
// not there yet (tracks arrive in the engine's order)
TEST(Playback_MixerStripOrderTests, InstrumentBeforeAuxWithoutMetronome)
{
    const std::vector<MixerStripRef> strips = { instrument(1), instrument(3), other() /*aux*/, other() /*master*/ };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(2), Kind::Instrument), 1);
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(4), Kind::Instrument), 2);
}

// The same with a FontDaw audio-track strip in place of the aux
TEST(Playback_MixerStripOrderTests, InstrumentBeforeAudioTrackWithoutMetronome)
{
    const std::vector<MixerStripRef> strips = { instrument(1), other() /*audio track*/, other() /*master*/ };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(2), Kind::Instrument), 1);
}

// FD-161: a list built before playback was set up is empty (no master); upstream's walk read past its end
TEST(Playback_MixerStripOrderTests, EmptyList)
{
    EXPECT_EQ(mixerStripInsertIndex({}, SCORE, track(3), Kind::Instrument), 0);
    EXPECT_EQ(mixerStripInsertIndex({}, SCORE, METRONOME, Kind::Metronome), 0);
}

TEST(Playback_MixerStripOrderTests, OrderOfTheScore)
{
    const std::vector<MixerStripRef> strips = { instrument(1), instrument(4), { Kind::Metronome, METRONOME }, other() };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(2), Kind::Instrument), 1);
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(3), Kind::Instrument), 1);
}

TEST(Playback_MixerStripOrderTests, AlreadyThereOrNotInTheScore)
{
    const std::vector<MixerStripRef> strips = { instrument(1), instrument(2), other() };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(2), Kind::Instrument), -1);
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, track(7), Kind::Instrument), -1);
}

TEST(Playback_MixerStripOrderTests, MetronomeAndChordSymbols)
{
    const InstrumentTrackId chords = InstrumentTrackId { muse::ID(5), muse::String(u"chords") };

    // the metronome goes after the instruments and chord symbols, before aux, audio tracks and master
    std::vector<MixerStripRef> strips = { instrument(1), instrument(2), { Kind::ChordSymbols, chords }, other(), other() };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, METRONOME, Kind::Metronome), 3);

    // chord symbols go after the instruments, before the metronome
    strips = { instrument(1), instrument(2), { Kind::Metronome, METRONOME }, other() };
    EXPECT_EQ(mixerStripInsertIndex(strips, SCORE, chords, Kind::ChordSymbols), 2);
}

// FD-161: tracks added one by one while loading ask for one rebuild once loading is done
TEST(Playback_MixerRebuildAfterLoadingTests, InsertsWhileLoadingAskForOneRebuild)
{
    MixerRebuildAfterLoading rebuild;
    rebuild.stripInserted(/*loaded*/ false);
    rebuild.stripInserted(/*loaded*/ false);

    EXPECT_FALSE(rebuild.shouldRebuild(/*playbackInited*/ true, /*loaded*/ false));
    EXPECT_TRUE(rebuild.shouldRebuild(/*playbackInited*/ true, /*loaded*/ true));

    rebuild.rebuilt();
    EXPECT_FALSE(rebuild.shouldRebuild(true, true)); // a later tab switch rebuilds nothing
}

// FD-161: an instrument added to a loaded score is inserted in place; it asks for no rebuild (the
// finish of its own loading is reported before its track is added)
TEST(Playback_MixerRebuildAfterLoadingTests, AnInstrumentAddedToALoadedScoreAsksForNoRebuild)
{
    MixerRebuildAfterLoading rebuild;
    rebuild.stripInserted(/*loaded*/ true);

    EXPECT_FALSE(rebuild.shouldRebuild(true, true));
}

// FD-161: tab switches and part changes (isPlayAllowedChanged) without inserts rebuild nothing
TEST(Playback_MixerRebuildAfterLoadingTests, NoInsertsNoRebuild)
{
    MixerRebuildAfterLoading rebuild;
    EXPECT_FALSE(rebuild.shouldRebuild(true, true));
}
