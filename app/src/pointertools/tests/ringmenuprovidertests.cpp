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

#include "modularity/ioc.h"
#include "notationscene/notationcommands.h"

#include "internal/ringmenuprovider.h"

using namespace mu::pointertools;
using namespace mu::notation;
using namespace muse;
using namespace muse::rcommand;
using namespace muse::ui;

namespace {
const std::string TEST_MODULE("pointertools_ringmenuprovider_utests");
const modularity::IoCID TEST_CONTEXT_ID = 9;

class FakeCommandsRegister : public ICommandsRegister
{
public:
    void reg(const IModuleCommandsRegisterPtr&) override {}
    void unreg(const IModuleCommandsRegisterPtr&) override {}
    IModuleCommandsRegisterPtr moduleRegister(const std::string&) const override { return nullptr; }

    std::vector<CommandInfo> commandInfoList() const override { return infos; }
    const CommandInfo& commandInfo(const Command& command) const override
    {
        for (const CommandInfo& info : infos) {
            if (info.command == command) {
                return info;
            }
        }
        static const CommandInfo none;
        return none;
    }

    async::Notification commandInfoListChanged() const override { return {}; }
    const std::string& commandModuleName(const Command&) const override
    {
        static const std::string none;
        return none;
    }

    void add(const Command& command, const char* title, IconCode::Code icon)
    {
        CommandInfo info;
        info.command = command;
        info.title = MnemonicString(TranslatableString("test", title));
        info.description = TranslatableString("test", title);
        info.decoration = Decoration(icon);
        infos.push_back(info);
    }

    std::vector<CommandInfo> infos;
};

class FakeCommandsState : public ICommandsState
{
public:
    void reg(const IModuleCommandsStatePtr&) override {}
    void unreg(const IModuleCommandsStatePtr&) override {}

    CommandState commandState(const Command& command) const override
    {
        auto it = states.find(command);
        return it != states.end() ? it->second : CommandState(true);
    }

    async::Channel<Command, CommandState> commandStateChanged() const override { return {}; }

    std::map<Command, CommandState> states;
};

std::vector<Command> commandsOf(const RingContents& ring)
{
    std::vector<Command> result;
    for (const RingSlot& slot : ring.ringSlots) {
        if (slot.kind == RingSlot::Kind::Command) {
            result.push_back(slot.command);
        }
    }
    return result;
}
}

class PointerTools_RingMenuProviderTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_register = std::make_shared<FakeCommandsRegister>();
        modularity::globalIoc()->registerExport<ICommandsRegister>(TEST_MODULE, m_register);

        m_state = std::make_shared<FakeCommandsState>();
        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        modularity::ioc(m_iocCtx)->registerExport<ICommandsState>(TEST_MODULE, m_state);

        // every table command, some with an icon of their own, three without
        m_register->add(TOGGLE_SFORZATO_COMMAND, "&Accent", IconCode::Code::ACCENT);
        m_register->add(TOGGLE_STACCATO_COMMAND, "Staccato", IconCode::Code::STACCATO);
        m_register->add(TOGGLE_TENUTO_COMMAND, "Tenuto", IconCode::Code::TENUTO);
        m_register->add(TOGGLE_TIE_COMMAND, "Tie", IconCode::Code::NOTE_TIE);
        m_register->add(ADD_SLUR_COMMAND, "Slur", IconCode::Code::NOTE_SLUR);
        m_register->add(FlipCommand::id, "Flip direction", IconCode::Code::NOTE_FLIP);
        m_register->add(DeleteCommand::id, "De&lete", IconCode::Code::DELETE_TANK);
        m_register->add(ADD_HAIRPIN_COMMAND, "&Crescendo", IconCode::Code::NONE);
        m_register->add(ADD_HAIRPIN_REVERSE_COMMAND, "&Diminuendo", IconCode::Code::NONE);
        m_register->add(AddTripletCommand::id, "Triplet", IconCode::Code::NONE);
        m_register->add(CopyCommand::id, "&Copy", IconCode::Code::COPY);

        m_provider = std::make_shared<RingMenuProvider>(m_iocCtx);
    }

    void TearDown() override
    {
        m_provider = nullptr;
        modularity::globalIoc()->unregister<ICommandsRegister>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    static std::vector<IRingMenuProvider::TodayMenuItem> todayMenu(int count)
    {
        std::vector<IRingMenuProvider::TodayMenuItem> items;
        for (int i = 0; i < count; ++i) {
            IRingMenuProvider::TodayMenuItem item;
            item.id = "item" + std::to_string(i);
            item.title = String::fromStdString(item.id);
            item.icon = IconCode::Code::PLAY;
            items.push_back(item);
        }
        return items;
    }

    std::shared_ptr<FakeCommandsRegister> m_register;
    std::shared_ptr<FakeCommandsState> m_state;
    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<RingMenuProvider> m_provider;
};

TEST_F(PointerTools_RingMenuProviderTests, OneNote)
{
    const RingContents ring = m_provider->ringFor(ScoreItemKind::OneNote, {});

    EXPECT_EQ(ring.accessibleName, String(u"Actions for note"));
    const std::vector<Command> expected {
        TOGGLE_SFORZATO_COMMAND, TOGGLE_STACCATO_COMMAND, TOGGLE_TENUTO_COMMAND, TOGGLE_TIE_COMMAND,
        ADD_SLUR_COMMAND, FlipCommand::id, DeleteCommand::id
    };
    EXPECT_EQ(commandsOf(ring), expected);
    ASSERT_EQ(ring.ringSlots.size(), 8u);
    EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
    EXPECT_EQ(ring.ringSlots.back().icon, IconCode::Code::MENU_THREE_DOTS);
    EXPECT_EQ(ring.ringSlots.back().title, String(u"More…"));

    // title without mnemonic, the command's own icon
    EXPECT_EQ(ring.ringSlots[0].title, String(u"Accent"));
    EXPECT_EQ(ring.ringSlots[0].icon, IconCode::Code::ACCENT);
    EXPECT_EQ(ring.ringSlots[6].title, String(u"Delete"));
    EXPECT_EQ(ring.ringSlots[6].icon, IconCode::Code::DELETE_TANK);
}

TEST_F(PointerTools_RingMenuProviderTests, SeveralNotesUseTheOverrideIcons)
{
    const RingContents ring = m_provider->ringFor(ScoreItemKind::SeveralNotes, {});

    EXPECT_EQ(ring.accessibleName, String(u"Actions for notes"));
    const std::vector<Command> expected {
        ADD_SLUR_COMMAND, ADD_HAIRPIN_COMMAND, ADD_HAIRPIN_REVERSE_COMMAND, AddTripletCommand::id, CopyCommand::id, DeleteCommand::id
    };
    EXPECT_EQ(commandsOf(ring), expected);
    ASSERT_EQ(ring.ringSlots.size(), 7u);
    EXPECT_EQ(ring.ringSlots[0].icon, IconCode::Code::NOTE_SLUR);
    // hairpin wedges: the crescendo wedge, and the same wedge mirrored for diminuendo
    EXPECT_EQ(ring.ringSlots[1].icon, IconCode::Code::HAIRPIN);
    EXPECT_FALSE(ring.ringSlots[1].iconMirrored);
    EXPECT_EQ(ring.ringSlots[2].icon, IconCode::Code::HAIRPIN);
    EXPECT_TRUE(ring.ringSlots[2].iconMirrored);
    EXPECT_FALSE(ring.ringSlots[0].iconMirrored);
    EXPECT_EQ(ring.ringSlots[3].icon, IconCode::Code::NOTE_TUPLET);
    EXPECT_EQ(ring.ringSlots[4].icon, IconCode::Code::COPY);
    EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
}

TEST_F(PointerTools_RingMenuProviderTests, ADisabledCommandLeavesTheRing)
{
    m_state->states[TOGGLE_TENUTO_COMMAND] = CommandState(false);
    m_state->states[FlipCommand::id] = CommandState(false);

    const RingContents ring = m_provider->ringFor(ScoreItemKind::OneNote, {});

    const std::vector<Command> expected {
        TOGGLE_SFORZATO_COMMAND, TOGGLE_STACCATO_COMMAND, TOGGLE_TIE_COMMAND, ADD_SLUR_COMMAND, DeleteCommand::id
    };
    EXPECT_EQ(commandsOf(ring), expected);
    EXPECT_EQ(ring.ringSlots.size(), 6u); // closed up, More… last
    EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
}

TEST_F(PointerTools_RingMenuProviderTests, Audio)
{
    const RingContents ring = m_provider->ringFor(ScoreItemKind::Audio, {});

    EXPECT_EQ(ring.accessibleName, String(u"Actions for audio"));
    // only "More…" until Batch 2 S5's clip commands exist (ringmenutables.cpp)
    EXPECT_TRUE(commandsOf(ring).empty());
    ASSERT_EQ(ring.ringSlots.size(), 1u);
    EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
}

TEST_F(PointerTools_RingMenuProviderTests, OtherTakesTheFirstEnabledItemsOfTodaysMenu)
{
    std::vector<IRingMenuProvider::TodayMenuItem> menu = todayMenu(10);
    menu[0].enabled = false;
    menu[1].isSeparatorOrSubmenu = true;

    const RingContents ring = m_provider->ringFor(ScoreItemKind::Other, menu);

    EXPECT_EQ(ring.accessibleName, String(u"Actions"));
    ASSERT_EQ(ring.ringSlots.size(), RING_MAX_SLOTS);
    for (size_t i = 0; i < RING_MAX_SLOTS - 1; ++i) {
        EXPECT_EQ(ring.ringSlots[i].kind, RingSlot::Kind::TodayMenuItem);
        EXPECT_EQ(ring.ringSlots[i].todayMenuItemId, "item" + std::to_string(i + 2));
        EXPECT_EQ(ring.ringSlots[i].icon, IconCode::Code::PLAY);
    }
    EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
}

TEST_F(PointerTools_RingMenuProviderTests, NoneAndAnEmptyMenuGiveOnlyMore)
{
    for (ScoreItemKind kind : { ScoreItemKind::None, ScoreItemKind::Other }) {
        const RingContents ring = m_provider->ringFor(kind, {});
        EXPECT_EQ(ring.accessibleName, String(u"Actions"));
        ASSERT_EQ(ring.ringSlots.size(), 1u);
        EXPECT_EQ(ring.ringSlots[0].kind, RingSlot::Kind::More);
    }
}

TEST_F(PointerTools_RingMenuProviderTests, MoreIsAlwaysLastAndTheRingNeverExceedsTheMaximum)
{
    for (ScoreItemKind kind : { ScoreItemKind::None, ScoreItemKind::OneNote, ScoreItemKind::SeveralNotes,
                                ScoreItemKind::Audio, ScoreItemKind::Other }) {
        for (int count : { 0, 3, 7, 8, 30 }) {
            const RingContents ring = m_provider->ringFor(kind, todayMenu(count));
            ASSERT_FALSE(ring.ringSlots.empty());
            EXPECT_LE(ring.ringSlots.size(), RING_MAX_SLOTS);
            EXPECT_EQ(ring.ringSlots.back().kind, RingSlot::Kind::More);
            for (size_t i = 0; i + 1 < ring.ringSlots.size(); ++i) {
                EXPECT_NE(ring.ringSlots[i].kind, RingSlot::Kind::More);
            }
        }
    }
}

//! FD-156, FD-166: slots carry whether their name is drawn
TEST_F(PointerTools_RingMenuProviderTests, SlotsCarryShowName)
{
    const RingContents one = m_provider->ringFor(ScoreItemKind::OneNote, {});
    ASSERT_EQ(one.ringSlots.size(), 8u);
    // accent, staccato, tenuto, tie, slur: engraving symbols, no name; flip and delete: names; More…: name
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_FALSE(one.ringSlots[i].showName) << i;
    }
    EXPECT_TRUE(one.ringSlots[5].showName);
    EXPECT_TRUE(one.ringSlots[6].showName);
    EXPECT_TRUE(one.ringSlots[7].showName);
    EXPECT_EQ(one.ringSlots[7].kind, RingSlot::Kind::More);
    EXPECT_FALSE(one.ringSlots[7].title.isEmpty());

    const RingContents several = m_provider->ringFor(ScoreItemKind::SeveralNotes, {});
    ASSERT_EQ(several.ringSlots.size(), 7u);
    // slur, crescendo, diminuendo, tuplet: no name; copy, delete, More…: name
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_FALSE(several.ringSlots[i].showName) << i;
    }
    for (size_t i = 4; i < 7; ++i) {
        EXPECT_TRUE(several.ringSlots[i].showName) << i;
    }

    // today's menu: every item has a name
    for (const RingSlot& slot : m_provider->ringFor(ScoreItemKind::Other, todayMenu(3)).ringSlots) {
        EXPECT_TRUE(slot.showName);
    }
}

TEST_F(PointerTools_RingMenuProviderTests, TodaysMenuItemsWithoutAnIconGetAFallbackIcon)
{
    std::vector<IRingMenuProvider::TodayMenuItem> menu;
    for (const char* id : { "command://notation/open-edit-style", "command://notation/open-page-settings",
                            "command://notation/load-style", "command://notation/something-unknown" }) {
        IRingMenuProvider::TodayMenuItem item;
        item.id = id;
        item.title = String::fromUtf8(id);
        menu.push_back(item);
    }
    // an icon the item has itself is kept
    IRingMenuProvider::TodayMenuItem own;
    own.id = "command://notation/open-page-settings";
    own.title = String(u"Own");
    own.icon = IconCode::Code::PLAY;
    menu.push_back(own);

    const RingContents ring = m_provider->ringFor(ScoreItemKind::Other, menu);

    ASSERT_EQ(ring.ringSlots.size(), 6u);
    EXPECT_EQ(ring.ringSlots[0].icon, IconCode::Code::BRUSH);
    EXPECT_EQ(ring.ringSlots[1].icon, IconCode::Code::PAGE);
    EXPECT_EQ(ring.ringSlots[2].icon, IconCode::Code::OPEN_FILE);
    EXPECT_EQ(ring.ringSlots[3].icon, IconCode::Code::NONE); // letters in QML
    EXPECT_EQ(ring.ringSlots[4].icon, IconCode::Code::PLAY);
    EXPECT_EQ(ring.ringSlots[5].kind, RingSlot::Kind::More);
}
