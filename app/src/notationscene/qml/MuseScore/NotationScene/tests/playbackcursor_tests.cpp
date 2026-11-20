/*
 * SPDX-License-Identifier: GPL-3.0-only
 * FontDaw
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/system.h"
#include "engraving/tests/utils/scorerw.h"

#include "notation/tests/mocks/notationelementsmock.h"
#include "notation/tests/mocks/notationmock.h"

#include "notationscene/qml/MuseScore/NotationScene/playbackcursor.h"

using namespace mu;
using namespace mu::notation;
using namespace muse;
using ::testing::NiceMock;
using ::testing::Return;

static const String CURSOR_SCORE_PATH(u"data/playbackcursor.mscx");

//! FontDaw (FD-170): the playback cursor caches the system and measure it last drew in
class NotationScene_PlaybackCursorTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_score = engraving::ScoreRW::readScore(CURSOR_SCORE_PATH);
        ASSERT_TRUE(m_score);

        m_elements = std::make_shared<NiceMock<NotationElementsMock> >();
        ON_CALL(*m_elements, msScore()).WillByDefault(Return(m_score));

        m_notation = std::make_shared<NiceMock<NotationMock> >();
        ON_CALL(*m_notation, elements()).WillByDefault(Return(m_elements));
        ON_CALL(*m_notation, viewModeChanged()).WillByDefault(Return(m_viewModeChanged));
    }

    void TearDown() override
    {
        delete m_score;
    }

    engraving::MasterScore* m_score = nullptr;
    std::shared_ptr<NiceMock<NotationElementsMock> > m_elements;
    std::shared_ptr<NiceMock<NotationMock> > m_notation;
    async::Notification m_viewModeChanged;
};

//! A layout outside a transaction sends no changes notification. Here the score is laid out in line
//! mode, which deletes every system of the page layout. The cursor must not draw from its cached
//! system (freed memory, the crash of FD-170), but from the measure's new system.
TEST_F(NotationScene_PlaybackCursorTests, ALayoutWithoutANotificationDoesNotLeaveTheCursorOnAFreedSystem)
{
    // [GIVEN] A cursor in a bar far down the page, on a later system
    PlaybackCursor cursor(nullptr);
    cursor.setNotation(m_notation);

    const engraving::Fraction barTick(40, 1); // bar 41 of 4/4
    const engraving::Measure* measure = m_score->tick2measureMM(barTick);
    ASSERT_TRUE(measure);
    ASSERT_TRUE(measure->system());
    ASSERT_GT(m_score->systems().size(), 2u);
    ASSERT_NE(measure->system(), m_score->systems().front());

    cursor.move(barTick.ticks());
    EXPECT_TRUE(cursor.rect().isValid());

    // [WHEN] The score is laid out again, without a changes notification
    m_score->setLayoutMode(engraving::LayoutMode::LINE);
    m_score->doLayout();

    // [THEN] The cursor follows the new layout: where a new cursor puts it
    cursor.move(barTick.ticks() + 1);

    PlaybackCursor freshCursor(nullptr);
    freshCursor.setNotation(m_notation);
    freshCursor.move(barTick.ticks() + 1);

    EXPECT_TRUE(cursor.rect().isValid());
    EXPECT_EQ(cursor.rect(), freshCursor.rect());
}

//! A tick past the end of the score has no measure: the cursor drops its cache and draws nothing,
//! and comes back where it should for a tick inside the score
TEST_F(NotationScene_PlaybackCursorTests, ATickPastTheEndDropsTheCache)
{
    PlaybackCursor cursor(nullptr);
    cursor.setNotation(m_notation);

    const engraving::Fraction barTick(40, 1);
    cursor.move(barTick.ticks());
    ASSERT_TRUE(cursor.rect().isValid());

    cursor.move(m_score->endTick().ticks() + 1000);
    EXPECT_FALSE(cursor.rect().isValid());

    cursor.move(barTick.ticks() + 1);
    PlaybackCursor freshCursor(nullptr);
    freshCursor.setNotation(m_notation);
    freshCursor.move(barTick.ticks() + 1);
    EXPECT_TRUE(cursor.rect().isValid());
    EXPECT_EQ(cursor.rect(), freshCursor.rect());
}
