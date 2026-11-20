/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021-2024 MuseScore Limited and others
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
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

import MuseScore.AppShell

StyledDialogView {
    id: root

    title: qsTrc("appshell/about", "About %1").arg(aboutModel.appName())

    contentHeight: 590
    contentWidth: 480

    AboutModel {
        id: aboutModel
    }

    // Empty "url" means the link is not available (FONTDAW_HELP_BASE_URL is empty), so its label is hidden.
    readonly property var helpLink: aboutModel.helpUrl()
    readonly property var contributionLink: aboutModel.contributionUrl()
    readonly property var privacyLink: aboutModel.privacyUrl()
    readonly property var sourceCodeLink: aboutModel.sourceCodeUrl()

    ColumnLayout {
        anchors.fill: parent
        spacing: 30

        ColumnLayout {
            id: content

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 36
            Layout.leftMargin: 40
            Layout.rightMargin: 40

            spacing: 32

            Image {
                id: logo
                Layout.alignment: Qt.AlignHCenter

                source: aboutModel.appIconSource
                sourceSize: Qt.size(100, 100)

                MouseArea {
                    anchors.fill: parent

                    property int clickCount: 0

                    onClicked: {
                        clickCount++

                        if (clickCount % 3 == 0) {
                            aboutModel.toggleDevMode()
                        }
                    }
                }
            }

            Column {
                spacing: 8
                Layout.fillWidth: true

                StyledTextLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTrc("appshell/about", "Version:") + " " + aboutModel.museScoreVersion()
                    font: ui.theme.bodyBoldFont
                }

                Row {
                    spacing: 4
                    anchors.horizontalCenter: parent.horizontalCenter

                    StyledTextLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTrc("appshell/about", "Revision:") + " " + aboutModel.museScoreRevision()
                    }

                    FlatButton {
                        anchors.verticalCenter: parent.verticalCenter
                        icon: IconCode.COPY

                        onClicked: {
                            aboutModel.copyRevisionToClipboard()
                        }
                    }
                }
            }

            StyledTextLabel {
                Layout.fillWidth: true

                text: qsTrc("appshell/about", "Build date: %1")
                      .arg(aboutModel.museScoreBuildDateTime())

                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("appshell/about", "Source code: %1")
                          .arg(`<a href="${root.sourceCodeLink.url}">${root.sourceCodeLink.displayName}</a>`)

                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    visible: root.helpLink.url !== "" || root.contributionLink.url !== ""

                    text: {
                        let hasHelp = root.helpLink.url !== ""
                        let hasContribute = root.contributionLink.url !== ""

                        if (hasHelp && hasContribute) {
                            return qsTrc("appshell/about", "Get <a href=\"%1\">help</a> with the program or <a href=\"%2\">contribute</a> to its development.")
                                   .arg(root.helpLink.url)
                                   .arg(root.contributionLink.url)
                        }

                        if (hasHelp) {
                            return qsTrc("appshell/about", "Get <a href=\"%1\">help</a> with the program.")
                                   .arg(root.helpLink.url)
                        }

                        return qsTrc("appshell/about", "<a href=\"%1\">Contribute</a> to its development.")
                               .arg(root.contributionLink.url)
                    }

                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    visible: root.privacyLink.url !== ""

                    text: qsTrc("appshell/about", "For privacy information, see our <a href=\"%1\">privacy policy</a>.")
                          .arg(root.privacyLink.url)

                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("appshell/about", "Based on MuseScore Studio by MuseScore Limited and contributors, licensed under GPLv3.")

                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("appshell/about", "Copyright © 1999-2026 MuseScore Limited and others.\nCopyright © 2026 FontDaw contributors.\nPublished under the <a href=\"%1\">GNU General Public License version 3</a>.")
                          .arg("https://www.gnu.org/licenses/gpl-3.0.html")
                          .replace(/\n/g, "<br>")

                    wrapMode: Text.WordWrap
                    maximumLineCount: 5
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: 16
            Layout.bottomMargin: 16

            spacing: 12

            FlatButton {
                text: qsTrc("global", "OK")

                onClicked: {
                    root.hide()
                }
            }
        }
    }
}
