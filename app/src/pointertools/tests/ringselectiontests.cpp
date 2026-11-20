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

#include "engraving/dom/chord.h"
#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/tests/utils/scorerw.h"

#include "internal/ringselection.h"

using namespace mu::engraving;
using namespace mu::pointertools;

namespace {
using Items = std::vector<const EngravingItem*>;

//! The chords and rests of the first bar in the fixture: note, two-note chord, note, rest.
struct Fixture {
    MasterScore* score = nullptr;
    Chord* single1 = nullptr;
    Chord* twoNotes = nullptr;
    Chord* single2 = nullptr;
    Rest* rest = nullptr;
};

Fixture readFixture()
{
    Fixture f;
    f.score = ScoreRW::readScore(u"data/ring_selection.mscx");
    if (!f.score) {
        return f;
    }
    for (Segment* s = f.score->firstMeasure()->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e->isRest()) {
            f.rest = toRest(e);
        } else if (!f.single1) {
            f.single1 = toChord(e);
        } else if (!f.twoNotes) {
            f.twoNotes = toChord(e);
        } else {
            f.single2 = toChord(e);
        }
    }
    return f;
}
}

TEST(PointerTools_RingSelectionTests, NothingIsNone)
{
    EXPECT_EQ(classifySelection({}, false), ScoreItemKind::None);
}

TEST(PointerTools_RingSelectionTests, OneNote)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.single1 && f.twoNotes && f.single2 && f.rest);
    ASSERT_EQ(f.twoNotes->notes().size(), 2u);

    EXPECT_EQ(classifySelection(Items { f.single1->upNote() }, false), ScoreItemKind::OneNote);

    delete f.score;
}

TEST(PointerTools_RingSelectionTests, TwoNotesOfOneChordAreOneNote)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.twoNotes);

    Items items { f.twoNotes->notes()[0], f.twoNotes->notes()[1] };
    EXPECT_EQ(classifySelection(items, false), ScoreItemKind::OneNote);

    delete f.score;
}

TEST(PointerTools_RingSelectionTests, NotesOfTwoChordsAreSeveralNotes)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.single1 && f.twoNotes);

    Items items { f.single1->upNote(), f.twoNotes->notes()[0] };
    EXPECT_EQ(classifySelection(items, false), ScoreItemKind::SeveralNotes);

    delete f.score;
}

TEST(PointerTools_RingSelectionTests, ARangeIsSeveralNotes)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.single1);

    EXPECT_EQ(classifySelection(Items { f.single1->upNote() }, true), ScoreItemKind::SeveralNotes);
    EXPECT_EQ(classifySelection({}, true), ScoreItemKind::SeveralNotes);

    delete f.score;
}

TEST(PointerTools_RingSelectionTests, ARestIsOther)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.rest && f.single1);

    EXPECT_EQ(classifySelection(Items { f.rest }, false), ScoreItemKind::Other);
    // a note together with a rest is not a set of notes
    EXPECT_EQ(classifySelection(Items { f.single1->upNote(), f.rest }, false), ScoreItemKind::Other);

    delete f.score;
}

TEST(PointerTools_RingSelectionTests, AnElementOnAnAudioStaffIsAudio)
{
    Fixture f = readFixture();
    ASSERT_TRUE(f.score && f.single1 && f.rest);

    // an audio part: what the arrangement module's addAudioPart makes, reduced to the mark
    // (the instrument id), which is what isAudioStaff reads
    Part* part = new Part(f.score);
    Instrument instrument(FONTDAW_AUDIO_INSTRUMENT_ID);
    part->setInstrument(instrument);
    f.score->appendPart(part);
    Staff* staff = Factory::createStaff(part);
    f.score->appendStaff(staff);
    ASSERT_TRUE(isAudioStaff(staff));
    ASSERT_FALSE(isAudioStaff(f.single1->staff()));

    // a rest of the audio staff (any element with that staff)
    Rest* audioRest = Factory::createRest(f.score->dummy(), TDuration(DurationType::V_QUARTER));
    audioRest->setTrack(staff->idx() * VOICES);
    ASSERT_TRUE(isAudioStaff(audioRest->staff()));

    EXPECT_EQ(classifySelection(Items { audioRest }, false), ScoreItemKind::Audio);
    // audio wins over a range and over notes elsewhere
    EXPECT_EQ(classifySelection(Items { f.single1->upNote(), audioRest }, false), ScoreItemKind::Audio);
    EXPECT_EQ(classifySelection(Items { audioRest }, true), ScoreItemKind::Audio);

    delete audioRest;
    delete f.score;
}
