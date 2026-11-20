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

#include "async/promise.h"
#include "modularity/ioc.h"
#include "rcommand/tests/mocks/commanddispatchermock.h"
#include "shortcuts_v2/tests/mocks/commandshortcutsregistermock.h"
#include "shortcuts_v2/internal/shortcutscontroller.h"
#include "shortcuts_v2/internal/shortcutsresolver.h"
#include "ui/navigationcommands.h"
#include "project/tests/mocks/projectsessionmock.h"
#include "notation/tests/mocks/notationmock.h"
#include "notation/tests/mocks/notationinteractionmock.h"

#include "pointertoolscommands.h"
#include "internal/pointertoolscommandsstate.h"

// Tab over the score: the 4.7 layout binds it to open-pointer-menu in NOTATION scope and the
// base layout to next-panel in NAVIGATION scope (src/appshell/tests/fontdawtabcommandmenutests.cpp
// checks the layout files). These tests send Tab through the real shortcuts controller and
// resolver: in note input and in normal mode it opens the Command menu; while text is edited
// (lyrics, chord symbols, fingering, staff text) the menu command is disabled and Tab moves to the
// next panel, as it did before.

using namespace mu::pointertools;
using namespace muse;
using namespace muse::rcommand;
using namespace muse::shortcuts;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Truly;

namespace {
const std::string TEST_MODULE("pointertools_tabtextediting_utests");
const modularity::IoCID TEST_CONTEXT_ID = 11;

//! The app's commands state for the two commands Tab can reach: next-panel is always enabled,
//! open-pointer-menu asks the real PointerToolsCommandsState.
class TabCommandsState : public ICommandsState
{
public:
    void reg(const IModuleCommandsStatePtr&) override {}
    void unreg(const IModuleCommandsStatePtr&) override {}

    CommandState commandState(const Command& command) const override
    {
        if (command == OPEN_POINTER_MENU_COMMAND) {
            return pointerTools->commandState(command);
        }
        return CommandState(true);
    }

    async::Channel<Command, CommandState> commandStateChanged() const override { return {}; }

    std::shared_ptr<PointerToolsCommandsState> pointerTools;
};

Shortcut tabShortcut(const Command& command, const std::string& scope)
{
    Shortcut sc;
    sc.command = command;
    sc.scope = scope;
    sc.sequences = { "Tab" };
    return sc;
}
}

class PointerTools_TabTextEditingTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);

        m_register = std::make_shared<NiceMock<CommandShortcutsRegisterMock> >();
        modularity::globalIoc()->registerExport<ICommandShortcutsRegister>(TEST_MODULE, m_register);
        ON_CALL(*m_register, shortcutsForSequence("Tab")).WillByDefault(Return(ShortcutList {
            tabShortcut(ui::NEXT_PANEL_COMMAND, "NAVIGATION"),
            tabShortcut(OPEN_POINTER_MENU_COMMAND, "NOTATION"),
        }));

        // the scopes and priorities of src/appshell/internal/appshortcutsscopes.cpp; the score has focus
        m_resolver = std::make_shared<ShortcutsResolver>();
        m_resolver->setScope({ "NAVIGATION", TranslatableString::untranslatable("NAVIGATION"), 1 });
        m_resolver->setScope({ "NOTATION", TranslatableString::untranslatable("NOTATION"), 4, []() { return true; } });
        m_resolver->setScope({ "NOTATION_NOTE_INPUT", TranslatableString::untranslatable("NOTE INPUT"), 8,
                               [this]() { return m_noteInput; } });
        m_resolver->setScope({ "NOTATION_TEXT_EDITING", TranslatableString::untranslatable("TEXT EDITING"), 9,
                               [this]() { return m_textEditing; } });
        modularity::ioc(m_iocCtx)->registerExport<IShortcutsResolver>(TEST_MODULE, m_resolver);

        m_interaction = std::make_shared<NiceMock<mu::notation::NotationInteractionMock> >();
        ON_CALL(*m_interaction, isTextEditingStarted()).WillByDefault([this]() { return m_textEditing; });
        m_notation = std::make_shared<NiceMock<mu::notation::NotationMock> >();
        ON_CALL(*m_notation, interaction()).WillByDefault(Return(m_interaction));
        m_session = std::make_shared<NiceMock<mu::project::ProjectSessionMock> >();
        ON_CALL(*m_session, currentNotation()).WillByDefault(Return(m_notation));
        modularity::ioc(m_iocCtx)->registerExport<mu::project::IProjectSession>(TEST_MODULE, m_session);

        m_state = std::make_shared<TabCommandsState>();
        m_state->pointerTools = std::make_shared<PointerToolsCommandsState>(m_iocCtx);
        modularity::ioc(m_iocCtx)->registerExport<ICommandsState>(TEST_MODULE, m_state);

        m_dispatcher = std::make_shared<NiceMock<CommandDispatcherMock> >();
        modularity::ioc(m_iocCtx)->registerExport<ICommandDispatcher>(TEST_MODULE, m_dispatcher);

        m_controller = std::make_shared<ShortcutsController>(m_iocCtx);
    }

    void TearDown() override
    {
        m_controller = nullptr;
        m_state = nullptr;
        modularity::globalIoc()->unregister<ICommandShortcutsRegister>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    void expectTabDispatches(const Command& expected)
    {
        EXPECT_CALL(*m_dispatcher, dispatch(_, _)).Times(0);
        EXPECT_CALL(*m_dispatcher, dispatch(Truly([expected](const Request& request) {
            return request.command == expected;
        }), _)).WillOnce(Return(async::make_promise<Response>([](auto resolve, auto) {
            return resolve(Response());
        })));
    }

    bool m_noteInput = false;
    bool m_textEditing = false;

    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<NiceMock<CommandShortcutsRegisterMock> > m_register;
    std::shared_ptr<ShortcutsResolver> m_resolver;
    std::shared_ptr<NiceMock<mu::notation::NotationInteractionMock> > m_interaction;
    std::shared_ptr<NiceMock<mu::notation::NotationMock> > m_notation;
    std::shared_ptr<NiceMock<mu::project::ProjectSessionMock> > m_session;
    std::shared_ptr<TabCommandsState> m_state;
    std::shared_ptr<NiceMock<CommandDispatcherMock> > m_dispatcher;
    std::shared_ptr<ShortcutsController> m_controller;
};

TEST_F(PointerTools_TabTextEditingTests, TabOverTheScoreOpensTheCommandMenu)
{
    expectTabDispatches(OPEN_POINTER_MENU_COMMAND);
    m_controller->activate("Tab");
}

TEST_F(PointerTools_TabTextEditingTests, TabInNoteInputOpensTheCommandMenu)
{
    m_noteInput = true;
    expectTabDispatches(OPEN_POINTER_MENU_COMMAND);
    m_controller->activate("Tab");
}

TEST_F(PointerTools_TabTextEditingTests, TabWhileEditingTextMovesToTheNextPanel)
{
    m_textEditing = true;
    EXPECT_FALSE(m_state->pointerTools->commandState(OPEN_POINTER_MENU_COMMAND).enabled);
    expectTabDispatches(ui::NEXT_PANEL_COMMAND);
    m_controller->activate("Tab");
}

TEST_F(PointerTools_TabTextEditingTests, TabWithoutAScoreDoesNotOpenTheCommandMenu)
{
    ON_CALL(*m_session, currentNotation()).WillByDefault(Return(nullptr));
    expectTabDispatches(ui::NEXT_PANEL_COMMAND);
    m_controller->activate("Tab");
}
