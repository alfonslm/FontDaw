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

#include "modularity/ioc.h"
#include "shortcuts_v2/tests/mocks/commandshortcutsregistermock.h"

#include "internal/commandshortcutskeylookup.h"

using ::testing::NiceMock;
using ::testing::ReturnRef;

using namespace mu::pointertools;
using namespace muse;

namespace {
const std::string TEST_MODULE("pointertools_keylookup_utests");

rcommand::CommandInfo info(const char* uri)
{
    rcommand::CommandInfo i;
    i.command = rcommand::Command(uri);
    return i;
}
}

// FD-143: the Command menu's keys come from shortcuts v2's register, by the command's URI.
class PointerTools_CommandShortcutsKeyLookupTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_register = std::make_shared<NiceMock<shortcuts::CommandShortcutsRegisterMock> >();
        modularity::globalIoc()->registerExport<shortcuts::ICommandShortcutsRegister>(TEST_MODULE, m_register);
    }

    void TearDown() override
    {
        modularity::globalIoc()->unregister<shortcuts::ICommandShortcutsRegister>(TEST_MODULE);
    }

    shortcuts::Shortcut make(const char* uri, std::vector<std::string> sequences)
    {
        shortcuts::Shortcut sc;
        sc.command = rcommand::Command(uri);
        sc.sequences = std::move(sequences);
        return sc;
    }

    std::shared_ptr<NiceMock<shortcuts::CommandShortcutsRegisterMock> > m_register;
    CommandShortcutsKeyLookup m_lookup;
};

TEST_F(PointerTools_CommandShortcutsKeyLookupTests, OneKey)
{
    const shortcuts::Shortcut sc = make("command://notation/add-staccato", { "Shift+S" });
    EXPECT_CALL(*m_register, shortcut(rcommand::Command("command://notation/add-staccato"))).WillOnce(ReturnRef(sc));

    EXPECT_EQ(m_lookup.keys(info("command://notation/add-staccato")), String(u"Shift+S"));
}

TEST_F(PointerTools_CommandShortcutsKeyLookupTests, SeveralKeysJoined)
{
    const shortcuts::Shortcut sc = make("command://notation/set-duration/quarter", { "5", "Num+5" });
    EXPECT_CALL(*m_register, shortcut(rcommand::Command("command://notation/set-duration/quarter"))).WillOnce(ReturnRef(sc));

    EXPECT_EQ(m_lookup.keys(info("command://notation/set-duration/quarter")), String(u"5, Num+5"));
}

TEST_F(PointerTools_CommandShortcutsKeyLookupTests, NoKey_IsEmpty)
{
    const shortcuts::Shortcut sc = make("command://pointertools/open-pointer-menu", {});
    EXPECT_CALL(*m_register, shortcut(rcommand::Command("command://pointertools/open-pointer-menu"))).WillOnce(ReturnRef(sc));

    EXPECT_TRUE(m_lookup.keys(info("command://pointertools/open-pointer-menu")).empty());
}

// A key changed in Preferences shows the next time the menu asks: nothing is cached.
TEST_F(PointerTools_CommandShortcutsKeyLookupTests, ReadsTheRegisterEachTime)
{
    const shortcuts::Shortcut before = make("command://notation/add-staccato", { "Shift+S" });
    const shortcuts::Shortcut after = make("command://notation/add-staccato", { "Ctrl+Alt+S" });
    EXPECT_CALL(*m_register, shortcut(rcommand::Command("command://notation/add-staccato")))
    .WillOnce(ReturnRef(before))
    .WillOnce(ReturnRef(after));

    EXPECT_EQ(m_lookup.keys(info("command://notation/add-staccato")), String(u"Shift+S"));
    EXPECT_EQ(m_lookup.keys(info("command://notation/add-staccato")), String(u"Ctrl+Alt+S"));
}
