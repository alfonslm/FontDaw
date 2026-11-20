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

#include "internal/commandsearch.h"

using namespace mu::pointertools;
using muse::String;

class PointerTools_CommandSearchTests : public ::testing::Test
{
protected:
    static CommandEntry entry(const char16_t* title, const char16_t* description = u"",
                              Section section = Section::Articulations)
    {
        CommandEntry e;
        e.command = muse::rcommand::Command(std::string("command://test/") + String(title).toStdString());
        e.title = String(title);
        e.description = String(description);
        e.section = section;
        return e;
    }

    static std::vector<String> titles(const SearchHitList& hits)
    {
        std::vector<String> result;
        for (const SearchHit& hit : hits) {
            result.push_back(hit.entry->title);
        }
        return result;
    }
};

TEST_F(PointerTools_CommandSearchTests, EmptyInputGivesNoHits)
{
    EXPECT_TRUE(rankEntries({}, String(u"stacc")).empty());
}

TEST_F(PointerTools_CommandSearchTests, StaccFindsStaccatoFirst)
{
    const CommandEntryList entries = { entry(u"Staccatissimo"), entry(u"Staccato"), entry(u"Marcato") };

    const SearchHitList hits = rankEntries(entries, String(u"stacc"));

    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].entry->title, String(u"Staccato"));
    EXPECT_EQ(hits[1].entry->title, String(u"Staccatissimo"));
    EXPECT_EQ(hits[0].matchRank, 0);
}

TEST_F(PointerTools_CommandSearchTests, WordStartRanksOne)
{
    const CommandEntryList entries = { entry(u"Crescendo hairpin"), entry(u"Chair") };

    const SearchHitList hits = rankEntries(entries, String(u"hair"));

    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].entry->title, String(u"Crescendo hairpin"));
    EXPECT_EQ(hits[0].matchRank, 1);
    EXPECT_EQ(hits[1].entry->title, String(u"Chair"));
    EXPECT_EQ(hits[1].matchRank, 2);
}

TEST_F(PointerTools_CommandSearchTests, TitleComesBeforeDescription)
{
    const CommandEntryList entries = {
        entry(u"Accent", u"Adds a fermata-like mark"),
        entry(u"Fermata", u"Hold"),
        entry(u"Mark", u"Not a thing about fer-mata but mentions superfermata"),
    };

    const SearchHitList hits = rankEntries(entries, String(u"fermata"));

    ASSERT_EQ(hits.size(), 3u);
    EXPECT_EQ(hits[0].entry->title, String(u"Fermata"));
    EXPECT_EQ(hits[0].matchRank, 0);
    EXPECT_EQ(hits[1].entry->title, String(u"Accent"));
    EXPECT_EQ(hits[1].matchRank, 3);
    EXPECT_EQ(hits[2].entry->title, String(u"Mark"));
    EXPECT_EQ(hits[2].matchRank, 4);
}

TEST_F(PointerTools_CommandSearchTests, IsCaseInsensitive)
{
    const CommandEntryList entries = { entry(u"Staccato") };

    EXPECT_EQ(rankEntries(entries, String(u"STACC")).size(), 1u);
    EXPECT_EQ(rankEntries(entries, String(u"sTaCcAtO")).size(), 1u);

    const CommandEntryList upper = { entry(u"STACCATO") };
    EXPECT_EQ(rankEntries(upper, String(u"stacc")).size(), 1u);
}

TEST_F(PointerTools_CommandSearchTests, AllWordsMustMatch)
{
    const CommandEntryList entries = { entry(u"Add staccato"), entry(u"Add tenuto"), entry(u"Remove staccato") };

    const SearchHitList hits = rankEntries(entries, String(u"add stacc"));

    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].entry->title, String(u"Add staccato"));
    // "add" starts the title (0), "stacc" starts a word (1): the entry takes the worse
    EXPECT_EQ(hits[0].matchRank, 1);
}

TEST_F(PointerTools_CommandSearchTests, EmptyTextKeepsAllInOrder)
{
    const CommandEntryList entries = { entry(u"B"), entry(u"A", u"", Section::Clefs), entry(u"C") };

    for (const String& text : { String(), String(u"   ") }) {
        const SearchHitList hits = rankEntries(entries, text);
        ASSERT_EQ(hits.size(), 3u);
        EXPECT_EQ(titles(hits), (std::vector<String> { u"B", u"A", u"C" }));
        for (const SearchHit& hit : hits) {
            EXPECT_EQ(hit.matchRank, 0);
        }
    }
}

TEST_F(PointerTools_CommandSearchTests, TieOnRankIsBrokenBySectionThenTitleLengthThenOrder)
{
    const CommandEntryList entries = {
        entry(u"Slur long", u"", Section::Lines),
        entry(u"Slur", u"", Section::Lines),
        entry(u"Slurry", u"", Section::Articulations),
        entry(u"Slurs", u"", Section::Lines),
        entry(u"Slurz", u"", Section::Lines),
    };

    const SearchHitList hits = rankEntries(entries, String(u"slur"));

    // Articulations before Lines; inside Lines the shorter title first; same length: input order
    EXPECT_EQ(titles(hits), (std::vector<String> { u"Slurry", u"Slur", u"Slurs", u"Slurz", u"Slur long" }));
}

TEST_F(PointerTools_CommandSearchTests, WordStartNeedsANonAlphanumericBefore)
{
    const CommandEntryList entries = { entry(u"Add-hairpin"), entry(u"Ahair") };

    const SearchHitList hits = rankEntries(entries, String(u"hair"));

    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].entry->title, String(u"Add-hairpin"));
    EXPECT_EQ(hits[0].matchRank, 1);
    EXPECT_EQ(hits[1].matchRank, 2);
}
