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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

// Tab over the score opens the Command menu (FontDaw's own key on top of the 4.7 layout, the map
// file's "fontdaw_keys"), also in note input. Tab keeps navigating elsewhere because next-panel
// holds it in the NAVIGATION scope (priority 1), which is active everywhere, while the new key is
// in NOTATION (priority 4, active only while the score view has focus). Neither the text-editing
// scope nor the note-input scope binds Tab, so the scopes alone would let it open the menu while
// text is edited in the score: PointerToolsCommandsState disables the menu command then, and Tab
// falls back to next-panel (src/pointertools/tests/tabtexteditingtests.cpp sends Tab through the
// shortcuts controller). These tests read the JSON files from the source tree.
namespace {
const QString OPEN_POINTER_MENU("command://pointertools/open-pointer-menu");
const QString NEXT_PANEL("command://navigation/next-panel");

QStringList sequencesOf(const QString& fileName, const QString& scope, const QString& command)
{
    const QString path = QString(appshell_test_DATA_ROOT) + "/../../app/configs/data/" + fileName;
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << path.toStdString();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    EXPECT_EQ(error.error, QJsonParseError::NoError) << path.toStdString();

    QStringList result;
    for (const QJsonValue& group : doc.array()) {
        if (group.toObject().value("scope").toString() != scope) {
            continue;
        }
        for (const QJsonValue& sc : group.toObject().value("shortcuts").toArray()) {
            if (sc.toObject().value("command").toString() != command) {
                continue;
            }
            for (const QJsonValue& seq : sc.toObject().value("sequences").toArray()) {
                result << seq.toString();
            }
        }
    }
    return result;
}
}

TEST(AppShell_FontDawTabCommandMenuTests, Musescore47_TabOpensTheCommandMenuInNotationScope)
{
    EXPECT_EQ(sequencesOf("shortcuts_musescore47.json", "NOTATION", OPEN_POINTER_MENU), QStringList { "Tab" });
}

TEST(AppShell_FontDawTabCommandMenuTests, Musescore47Mac_TabOpensTheCommandMenuInNotationScope)
{
    EXPECT_EQ(sequencesOf("shortcuts_musescore47_mac.json", "NOTATION", OPEN_POINTER_MENU), QStringList { "Tab" });
}

TEST(AppShell_FontDawTabCommandMenuTests, Base_NextPanelKeepsTabInNavigationScope)
{
    const QStringList keys = sequencesOf("shortcuts.json", "NAVIGATION", NEXT_PANEL);
    EXPECT_TRUE(keys.contains("Tab"));
    EXPECT_TRUE(keys.contains("Ctrl+Tab"));
}

TEST(AppShell_FontDawTabCommandMenuTests, Base_TabIsNotBoundInNotationScope)
{
    // the new key must not clash with a NOTATION command of the base
    EXPECT_TRUE(sequencesOf("shortcuts.json", "NOTATION", OPEN_POINTER_MENU).isEmpty());
}
