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
#include <gmock/gmock.h>

#include <map>

#include "async/promise.h"
#include "modularity/ioc.h"
#include "rcommand/tests/mocks/commanddispatchermock.h"

#include "internal/rcommandindexsource.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Truly;

using namespace mu::pointertools;
using namespace muse;
using namespace muse::rcommand;

namespace {
const std::string TEST_MODULE("pointertools_rcommandindexsource_utests");
const modularity::IoCID TEST_CONTEXT_ID = 7;

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

    async::Notification commandInfoListChanged() const override { return changed; }

    const std::string& commandModuleName(const Command& command) const override
    {
        static const std::string none;
        auto it = modules.find(command);
        return it != modules.end() ? it->second : none;
    }

    void add(const std::string& module, const std::string& name, const char* title, const char* description = "",
             InputSchema schema = InputSchema())
    {
        CommandInfo info;
        info.command = Command("command://" + module + "/" + name);
        info.title = MnemonicString(TranslatableString("test", title));
        info.description = TranslatableString("test", description);
        info.inputSchema = schema;
        info.decoration = Decoration(ui::IconCode::Code::PLAY);
        infos.push_back(info);
        modules[info.command] = module;
    }

    std::vector<CommandInfo> infos;
    std::map<Command, std::string> modules;
    async::Notification changed;
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
}

class PointerTools_RCommandIndexSourceTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_register = std::make_shared<FakeCommandsRegister>();
        modularity::globalIoc()->registerExport<ICommandsRegister>(TEST_MODULE, m_register);

        m_state = std::make_shared<FakeCommandsState>();
        m_dispatcher = std::make_shared<NiceMock<CommandDispatcherMock> >();

        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        modularity::ioc(m_iocCtx)->registerExport<ICommandsState>(TEST_MODULE, m_state);
        modularity::ioc(m_iocCtx)->registerExport<ICommandDispatcher>(TEST_MODULE, m_dispatcher);

        m_source = std::make_shared<RCommandIndexSource>(m_iocCtx);
    }

    void TearDown() override
    {
        m_source = nullptr;
        modularity::globalIoc()->unregister<ICommandsRegister>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    const CommandEntry* find(const CommandEntryList& list, const std::string& command) const
    {
        for (const CommandEntry& e : list) {
            if (e.command == Command(command)) {
                return &e;
            }
        }
        return nullptr;
    }

    std::shared_ptr<FakeCommandsRegister> m_register;
    std::shared_ptr<FakeCommandsState> m_state;
    std::shared_ptr<NiceMock<CommandDispatcherMock> > m_dispatcher;
    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<RCommandIndexSource> m_source;
};

TEST_F(PointerTools_RCommandIndexSourceTests, OnlyTheModulesOfTheScoreViewAreListed)
{
    for (const char* module : { "notation", "palette", "playback", "arrangement", "project", "appshell" }) {
        m_register->add(module, "one", "One");
    }
    m_register->add("pointertools", "open-pointer-menu", "Command menu");
    m_register->add("somewhere", "else", "Else");

    const CommandEntryList entries = m_source->entries(ViewKind::Score);

    EXPECT_EQ(entries.size(), 6u);
    EXPECT_EQ(find(entries, "command://pointertools/open-pointer-menu"), nullptr);
    EXPECT_EQ(find(entries, "command://somewhere/else"), nullptr);
    EXPECT_NE(find(entries, "command://notation/one"), nullptr);
    EXPECT_NE(find(entries, "command://appshell/one"), nullptr);

    EXPECT_TRUE(m_source->entries(ViewKind::Mixer).empty());
    EXPECT_TRUE(m_source->entries(ViewKind::Undefined).empty());
}

TEST_F(PointerTools_RCommandIndexSourceTests, CommandsWithArgumentsAreLeftOut)
{
    m_register->add("notation", "plain", "Plain");
    m_register->add("notation", "with-args", "With args", "",
                    InputSchema({ { "x", Arg(DataType::Int, String(u"x")) } }));

    const CommandEntryList entries = m_source->entries(ViewKind::Score);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].command, Command("command://notation/plain"));
}

TEST_F(PointerTools_RCommandIndexSourceTests, EntryFieldsAreFilled)
{
    m_register->add("notation", "toggle-staccato", "&Staccato", "Add a staccato");
    m_register->add("arrangement", "import-audio-track", "Import audio track");

    const CommandEntryList entries = m_source->entries(ViewKind::Score);

    const CommandEntry* staccato = find(entries, "command://notation/toggle-staccato");
    ASSERT_NE(staccato, nullptr);
    EXPECT_EQ(staccato->title, String(u"Staccato")); // mnemonic marker removed
    EXPECT_EQ(staccato->description, String(u"Add a staccato"));
    EXPECT_EQ(staccato->icon, ui::IconCode::Code::PLAY);
    EXPECT_EQ(staccato->module, "notation");
    EXPECT_EQ(staccato->section, Section::Articulations);
    EXPECT_TRUE(staccato->keys.isEmpty());

    const CommandEntry* audio = find(entries, "command://arrangement/import-audio-track");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(audio->section, Section::Audio);
}

TEST_F(PointerTools_RCommandIndexSourceTests, IsEnabledAsksTheCommandsState)
{
    m_state->states[Command("command://notation/off")] = CommandState(false);
    m_state->states[Command("command://notation/on")] = CommandState(true);

    EXPECT_FALSE(m_source->isEnabled(Command("command://notation/off")));
    EXPECT_TRUE(m_source->isEnabled(Command("command://notation/on")));
}

TEST_F(PointerTools_RCommandIndexSourceTests, RunDispatchesTheCommand)
{
    const Command command("command://notation/toggle-staccato");
    CommandEntry entry;
    entry.command = command;

    EXPECT_CALL(*m_dispatcher, dispatch(Truly([command](const Request& request) {
        return request.command == command && request.params.empty();
    }), _)).WillOnce(Return(async::make_promise<Response>([](auto resolve, auto) {
        return resolve(Response());
    })));

    m_source->run(entry);
}

TEST_F(PointerTools_RCommandIndexSourceTests, EntriesChangedFollowsTheRegister)
{
    int count = 0;
    m_source->entriesChanged().onNotify(nullptr, [&count]() { ++count; });

    m_register->changed.notify();

    EXPECT_EQ(count, 1);
}
