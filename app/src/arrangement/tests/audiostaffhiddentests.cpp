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

#include <map>
#include <vector>

#include "engraving/dom/factory.h"
#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/volta.h"
#include "engraving/tests/utils/scorerw.h"



using namespace mu::engraving;

// Review of #196: isHiddenOnAudioStaff when the audio staff is staff 0, where the system markings
// (stored on the top staff) sit on it. They must stay; the staff's own rests and lines go.
class Arrangement_AudioStaffHiddenTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // data/systems_breaks.mscx has one instrument, on staff 0. Marking it as an audio
        // instrument (what isAudioStaff() reads) makes staff 0 the audio staff without editing the
        // score's structure.
        m_score = ScoreRW::readScore(u"data/systems_breaks.mscx");
        ASSERT_TRUE(m_score);
        ASSERT_EQ(m_score->parts().size(), 1u);
        m_score->parts().front()->instrument()->setId(String(FONTDAW_AUDIO_INSTRUMENT_ID));
        m_score->style().set(Sid::showMeasureNumber, true);
        m_score->style().set(Sid::measureNumberInterval, 1);
        m_score->style().set(Sid::measureNumberSystem, false);
        m_score->style().set(Sid::hideInstrumentNameIfOneInstrument, false);
        ASSERT_TRUE(isAudioStaff(m_score->staff(0)));

        m_bar1 = m_score->firstMeasure();
        m_bar2 = m_bar1->nextMeasure();
        ASSERT_TRUE(m_bar2);
        Segment* first = m_bar1->first(SegmentType::ChordRest);
        ASSERT_TRUE(first);

        // Added as the importers add them (straight to their segment or measure, no undo)
        TempoText* tempo = Factory::createTempoText(first);
        tempo->setXmlText(u"Allegro");
        tempo->setTrack(0);
        first->add(tempo);

        RehearsalMark* mark = Factory::createRehearsalMark(first);
        mark->setXmlText(u"A");
        mark->setTrack(0);
        first->add(mark);

        Volta* volta = Factory::createVolta(m_score->dummy());
        volta->setTick(m_bar1->tick());
        volta->setTick2(m_bar2->endTick());
        volta->setTrack(0);
        volta->setTrack2(0);
        volta->endings().clear();
        volta->endings().push_back(1);
        m_score->addSpanner(volta);

        Jump* jump = Factory::createJump(m_bar2);
        jump->setJumpType(JumpType::DC);
        jump->setTrack(0);
        m_bar2->add(jump);

        Marker* marker = Factory::createMarker(m_bar1);
        marker->setMarkerType(MarkerType::SEGNO);
        marker->setTrack(0);
        m_bar1->add(marker);

        m_score->setLayoutMode(LayoutMode::PAGE);
        m_score->doLayout();
    }

    void TearDown() override
    {
        delete m_score;
    }

    //! Every item of the laid-out score, counted by type, with how many of them are hidden
    struct Count {
        int total = 0;
        int hidden = 0;
    };

    std::map<ElementType, Count> scan() const
    {
        std::map<ElementType, Count> counts;
        m_score->scanElements([&counts](EngravingItem* item) {
            Count& c = counts[item->type()];
            ++c.total;
            c.hidden += isHiddenOnAudioStaff(item) ? 1 : 0;
        });
        return counts;
    }

    MasterScore* m_score = nullptr;
    Measure* m_bar1 = nullptr;
    Measure* m_bar2 = nullptr;
};

TEST_F(Arrangement_AudioStaffHiddenTests, SystemMarkingsOnTheTopAudioStaffStayVisible)
{
    const std::map<ElementType, Count> counts = scan();
    auto expectKept = [&counts](ElementType type, const char* name) {
        const auto it = counts.find(type);
        ASSERT_TRUE(it != counts.end()) << name << " is not in the score";
        EXPECT_GT(it->second.total, 0) << name;
        EXPECT_EQ(it->second.hidden, 0) << name << " is hidden";
    };

    expectKept(ElementType::TEMPO_TEXT, "tempo text");
    expectKept(ElementType::REHEARSAL_MARK, "rehearsal mark");
    expectKept(ElementType::VOLTA_SEGMENT, "volta segment"); // scanElements visits segments, not the volta
    expectKept(ElementType::JUMP, "jump");
    expectKept(ElementType::MARKER, "marker");
    expectKept(ElementType::MEASURE_NUMBER, "measure number");
    expectKept(ElementType::BAR_LINE, "bar line");
    expectKept(ElementType::INSTRUMENT_NAME, "instrument name");

    // not visited by scanElements: checked directly
    EXPECT_FALSE(isHiddenOnAudioStaff(m_bar1));
    for (const auto& s : m_score->spanner()) {
        if (s.second->isVolta()) {
            EXPECT_FALSE(isHiddenOnAudioStaff(s.second));
        }
    }
}

TEST_F(Arrangement_AudioStaffHiddenTests, TheStaffsOwnNotationIsHidden)
{
    const std::map<ElementType, Count> counts = scan();
    auto expectHidden = [&counts](ElementType type, const char* name) {
        const auto it = counts.find(type);
        ASSERT_TRUE(it != counts.end()) << name << " is not in the score";
        EXPECT_GT(it->second.total, 0) << name;
        EXPECT_EQ(it->second.hidden, it->second.total) << name << " is not hidden";
    };

    // the test score's notes and its clef and time signature stand in for an audio staff's
    // invisible rests: everything on the staff that is not a bar line, a name or a system marking
    expectHidden(ElementType::NOTE, "note");
    expectHidden(ElementType::STEM, "stem");
    expectHidden(ElementType::STAFF_LINES, "staff lines");
    expectHidden(ElementType::CLEF, "clef");
    expectHidden(ElementType::TIMESIG, "time signature");
}
