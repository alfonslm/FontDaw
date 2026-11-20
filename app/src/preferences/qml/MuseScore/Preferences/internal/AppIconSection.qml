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

import QtQuick

import Muse.Ui
import Muse.UiComponents

BaseSection {
    id: root

    title: qsTrc("preferences", "App icon")

    navigation.direction: NavigationPanel.Both

    property bool preferredAppIconIsLight: false
    property bool appIconFollowsSystem: true
    property bool isSystemColorSchemeKnown: true
    property string darkAppIconSource: ""
    property string lightAppIconSource: ""

    signal preferredAppIconIsLightChangeRequested(bool isLight)
    signal appIconFollowsSystemChangeRequested(bool follow)

    Row {
        width: parent.width
        spacing: root.columnSpacing

        Column {
            width: root.columnWidth
            spacing: 8

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 32
                height: 32
                sourceSize: Qt.size(64, 64)
                fillMode: Image.PreserveAspectFit
                source: root.darkAppIconSource
            }

            RoundedRadioButton {
                width: parent.width
                text: qsTrc("preferences", "Dark")
                checked: !root.preferredAppIconIsLight

                navigation.name: "DarkAppIconButton"
                navigation.panel: root.navigation
                navigation.row: 0
                navigation.column: 0

                onToggled: {
                    root.preferredAppIconIsLightChangeRequested(false)
                }
            }
        }

        Column {
            width: root.columnWidth
            spacing: 8

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 32
                height: 32
                sourceSize: Qt.size(64, 64)
                fillMode: Image.PreserveAspectFit
                source: root.lightAppIconSource
            }

            RoundedRadioButton {
                width: parent.width
                text: qsTrc("preferences", "Light")
                checked: root.preferredAppIconIsLight

                navigation.name: "LightAppIconButton"
                navigation.panel: root.navigation
                navigation.row: 0
                navigation.column: 1

                onToggled: {
                    root.preferredAppIconIsLightChangeRequested(true)
                }
            }
        }
    }

    CheckBox {
        width: parent.width

        text: qsTrc("preferences", "Follow system colour scheme")
        checked: root.appIconFollowsSystem

        navigation.name: "FollowSystemAppIconBox"
        navigation.panel: root.navigation
        navigation.row: 1
        navigation.column: 0

        onClicked: {
            root.appIconFollowsSystemChangeRequested(!checked)
        }
    }

    StyledTextLabel {
        width: parent.width
        visible: !root.isSystemColorSchemeKnown

        text: qsTrc("preferences", "Your system's colour scheme can't be read here, so the preferred icon is used.")

        horizontalAlignment: Qt.AlignLeft
        wrapMode: Text.WordWrap
    }
}
