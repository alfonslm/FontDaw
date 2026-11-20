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
#include <set>

#include "internal/commandindex.h"

using namespace mu::pointertools;
using muse::String;
using muse::rcommand::Command;

namespace {
CommandEntry makeEntry(const std::string& name, const char16_t* title, Section section,
                       const char16_t* description = u"")
{
    CommandEntry e;
    e.command = Command("command://test/" + name);
    e.title = String(title);
    e.description = String(description);
    e.section = section;
    e.module = "test";
    return e;
}

class FakeSource : public ICommandIndexSource
{
public:
    CommandEntryList list;
    std::set<Command> disabled;
    mutable std::vector<Command> ran;
    muse::async::Notification changed;

    CommandEntryList entries(ViewKind view) const override
    {
        return view == ViewKind::Score ? list : CommandEntryList();
    }

    bool isEnabled(const Command& command) const override { return disabled.find(command) == disabled.end(); }
    void run(const CommandEntry& entry) const override { ran.push_back(entry.command); }
    muse::async::Notification entriesChanged() const override { return changed; }
};

class FakeKeyLookup : public ICommandKeyLookup
{
public:
    std::map<Command, String> keysByCommand;
    mutable std::vector<muse::rcommand::CommandInfo> asked;

    String keys(const muse::rcommand::CommandInfo& info) const override
    {
        asked.push_back(info);
        auto it = keysByCommand.find(info.command);
        return it != keysByCommand.end() ? it->second : String();
    }
};
}

class PointerTools_CommandIndexTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_first = std::make_shared<FakeSource>();
        m_second = std::make_shared<FakeSource>();
        m_lookup = std::make_shared<FakeKeyLookup>();
        m_index = std::make_unique<CommandIndex>(nullptr);
        m_index->addSource(m_first);
        m_index->addSource(m_second);
        m_index->setKeyLookup(m_lookup);
        m_index->init();
    }

    static std::vector<String> titles(const CommandEntryList& list)
    {
        std::vector<String> result;
        for (const CommandEntry& e : list) {
            result.push_back(e.title);
        }
        return result;
    }

    std::shared_ptr<FakeSource> m_first;
    std::shared_ptr<FakeSource> m_second;
    std::shared_ptr<FakeKeyLookup> m_lookup;
    std::unique_ptr<CommandIndex> m_index;
};

TEST_F(PointerTools_CommandIndexTests, EnabledEntriesAreInAndDisabledAreNot)
{
    m_first->list = { makeEntry("a", u"A", Section::Notes), makeEntry("b", u"B", Section::Notes),
                      makeEntry("c", u"C", Section::Notes) };
    m_first->disabled.insert(Command("command://test/b"));

    EXPECT_EQ(titles(m_index->entries(ViewKind::Score)), (std::vector<String> { u"A", u"C" }));
}

TEST_F(PointerTools_CommandIndexTests, UnknownViewHasNoEntries)
{
    m_first->list = { makeEntry("a", u"A", Section::Notes) };

    EXPECT_TRUE(m_index->entries(ViewKind::Mixer).empty());
}

TEST_F(PointerTools_CommandIndexTests, CommandFromTwoSourcesAppearsOnceFirstWins)
{
    m_first->list = { makeEntry("a", u"From first", Section::Notes) };
    m_second->list = { makeEntry("a", u"From second", Section::Notes), makeEntry("b", u"B", Section::Notes) };

    EXPECT_EQ(titles(m_index->entries(ViewKind::Score)), (std::vector<String> { u"From first", u"B" }));
}

TEST_F(PointerTools_CommandIndexTests, ADisabledCommandOfTheFirstSourceDoesNotComeFromTheSecond)
{
    m_first->list = { makeEntry("a", u"From first", Section::Notes) };
    m_first->disabled.insert(Command("command://test/a"));
    m_second->list = { makeEntry("a", u"From second", Section::Notes) };

    EXPECT_TRUE(m_index->entries(ViewKind::Score).empty());
}

TEST_F(PointerTools_CommandIndexTests, KeysComeFromTheLookup)
{
    m_first->list = { makeEntry("a", u"A", Section::Notes, u"Does a"), makeEntry("b", u"B", Section::Notes) };
    m_lookup->keysByCommand[Command("command://test/a")] = String(u"Shift+S");

    const CommandEntryList entries = m_index->entries(ViewKind::Score);

    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].keys, String(u"Shift+S"));
    EXPECT_TRUE(entries[1].keys.isEmpty());

    ASSERT_EQ(m_lookup->asked.size(), 2u);
    EXPECT_EQ(m_lookup->asked[0].command, Command("command://test/a"));
    EXPECT_EQ(m_lookup->asked[0].title.raw().translated(), String(u"A"));
    EXPECT_EQ(m_lookup->asked[0].description.translated(), String(u"Does a"));
}

TEST_F(PointerTools_CommandIndexTests, WorksWithoutKeyLookup)
{
    CommandIndex index(nullptr);
    index.addSource(m_first);
    m_first->list = { makeEntry("a", u"A", Section::Notes) };

    EXPECT_EQ(index.entries(ViewKind::Score).size(), 1u);
}

TEST_F(PointerTools_CommandIndexTests, SortedBySectionKeepingInputOrderInside)
{
    m_first->list = {
        makeEntry("e1", u"Edit 1", Section::Edit),
        makeEntry("n1", u"Note 1", Section::Notes),
        makeEntry("e2", u"Edit 2", Section::Edit),
        makeEntry("a1", u"Artic 1", Section::Articulations),
        makeEntry("n2", u"Note 2", Section::Notes),
    };
    m_second->list = { makeEntry("a2", u"Artic 2", Section::Articulations), makeEntry("n3", u"Note 3", Section::Notes) };

    EXPECT_EQ(titles(m_index->entries(ViewKind::Score)),
              (std::vector<String> { u"Artic 1", u"Artic 2", u"Note 1", u"Note 2", u"Note 3", u"Edit 1", u"Edit 2" }));
}

TEST_F(PointerTools_CommandIndexTests, SearchRanksStaccatoFirst)
{
    m_first->list = {
        makeEntry("a", u"Staccatissimo", Section::Articulations),
        makeEntry("b", u"Staccato", Section::Articulations),
        makeEntry("c", u"Marcato", Section::Articulations),
    };

    const CommandEntryList hits = m_index->search(ViewKind::Score, String(u"stacc"));

    EXPECT_EQ(titles(hits), (std::vector<String> { u"Staccato", u"Staccatissimo" }));
    EXPECT_EQ(titles(m_index->search(ViewKind::Score, String())).size(), 3u);
}

TEST_F(PointerTools_CommandIndexTests, SearchDoesNotFindDisabledCommands)
{
    m_first->list = { makeEntry("a", u"Staccato", Section::Articulations) };
    m_first->disabled.insert(Command("command://test/a"));

    EXPECT_TRUE(m_index->search(ViewKind::Score, String(u"stacc")).empty());
}

TEST_F(PointerTools_CommandIndexTests, RunReachesTheSourceThatGaveTheEntry)
{
    m_first->list = { makeEntry("a", u"A", Section::Notes) };
    m_second->list = { makeEntry("b", u"B", Section::Notes) };

    m_index->run(m_index->entries(ViewKind::Score)[1]);

    EXPECT_TRUE(m_first->ran.empty());
    ASSERT_EQ(m_second->ran.size(), 1u);
    EXPECT_EQ(m_second->ran[0], Command("command://test/b"));

    m_index->run(makeEntry("a", u"A", Section::Notes));

    ASSERT_EQ(m_first->ran.size(), 1u);
    EXPECT_EQ(m_first->ran[0], Command("command://test/a"));
    EXPECT_EQ(m_second->ran.size(), 1u);
}

TEST_F(PointerTools_CommandIndexTests, RunOfAnUnknownCommandDoesNothing)
{
    m_first->list = { makeEntry("a", u"A", Section::Notes) };

    m_index->run(makeEntry("zzz", u"Z", Section::Notes));

    EXPECT_TRUE(m_first->ran.empty());
    EXPECT_TRUE(m_second->ran.empty());
}

TEST_F(PointerTools_CommandIndexTests, EntriesChangedFiresWhenASourceNotifies)
{
    int count = 0;
    m_index->entriesChanged().onNotify(nullptr, [&count]() { ++count; });

    m_second->changed.notify();
    EXPECT_EQ(count, 1);

    m_first->changed.notify();
    EXPECT_EQ(count, 2);
}
