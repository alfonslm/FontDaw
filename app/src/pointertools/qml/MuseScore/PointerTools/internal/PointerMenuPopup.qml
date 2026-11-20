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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents
import MuseScore.PointerTools

// The pointer menu ("Command menu"): a search field on top and the view's commands below, as the
// pictures in docs/fontdaw/design/shortcuts/images/ show. 440 x 520 px, plain theme colours.
StyledPopupView {
    id: root

    required property PointerMenuModel menuModel

    //! Where the pointer is, in the coordinates of this pop-up's parent; the pop-up's top-left
    //! corner opens a little right of and below it (the pop-up keeps itself on the screen).
    property point openPos: Qt.point(0, 0)

    readonly property int pointerOffset: 8
    readonly property int popupWidth: 440
    readonly property int popupHeight: 520
    readonly property int popupMargins: 8

    readonly property bool searching: searchField.searchText !== ""

    property int currentRow: -1

    // PopupView places itself relative to its parent item, so the layer gives it a zero-size
    // parent at the pointer (as ContextMenuLoader does); x and y stay 0
    showArrow: false

    margins: root.popupMargins
    contentWidth: root.popupWidth - 2 * root.popupMargins
    contentHeight: root.popupHeight - 2 * root.popupMargins

    placementPolicies: PopupView.Default
    closePolicies: PopupView.CloseOnPressOutsideParent
    animationEnabled: false

    function showAt(pos) {
        root.openPos = pos
        root.parent.x = pos.x + root.pointerOffset
        root.parent.y = pos.y + root.pointerOffset

        searchField.clear()
        root.menuModel.searchText = ""
        root.menuModel.load()
        root.currentRow = root.menuModel.rowCount() > 0 ? 0 : -1

        root.open()
    }

    function moveCurrent(delta) {
        var count = list.count
        if (count === 0) {
            return
        }

        root.currentRow = Math.max(0, Math.min(count - 1, root.currentRow + delta))
        list.positionViewAtIndex(root.currentRow, ListView.Contain)
    }

    function runRow(row) {
        if (row < 0 || row >= list.count) {
            return
        }

        // the pop-up closes first, then the command runs
        root.close()
        Qt.callLater(function() {
            root.menuModel.run(row)
        })
    }

    onOpened: {
        Qt.callLater(searchField.navigation.requestActive)
    }

    Connections {
        target: root.menuModel

        function onModelReset() {
            root.currentRow = root.menuModel.rowCount() > 0 ? 0 : -1
        }
    }

    NavigationPanel {
        id: searchPanel
        name: "PointerMenuSearch"
        section: root.navigationSection
        order: 1
        enabled: root.isOpened
        direction: NavigationPanel.Vertical
        //: The name of the pop-up menu that lists commands, opened with Tools > Command menu
        accessible.name: qsTrc("pointertools", "Command menu")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        TextInputField {
            id: searchField

            Layout.fillWidth: true

            property string searchText: ""

            hint: qsTrc("pointertools", "Search commands")
            hintIcon: IconCode.SEARCH
            clearTextButtonVisible: searchText !== ""

            navigation.panel: searchPanel
            navigation.order: 1
            navigation.accessible.name: hint

            onTextChanged: function(newTextValue) {
                searchText = newTextValue.trim()
                root.menuModel.searchText = searchText
            }

            onTextCleared: {
                searchText = ""
                root.menuModel.searchText = ""
            }

            onAccepted: {
                root.runRow(root.currentRow)
            }

            onEscaped: {
                root.close()
            }

            // The arrow keys move the highlight, so they must not move the keyboard navigation
            Keys.onShortcutOverride: function(event) {
                if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                    event.accepted = true
                }
            }

            Keys.onPressed: function(event) {
                switch (event.key) {
                case Qt.Key_Up:
                    root.moveCurrent(-1)
                    event.accepted = true
                    break
                case Qt.Key_Down:
                    root.moveCurrent(1)
                    event.accepted = true
                    break
                case Qt.Key_PageUp:
                    root.moveCurrent(-8)
                    event.accepted = true
                    break
                case Qt.Key_PageDown:
                    root.moveCurrent(8)
                    event.accepted = true
                    break
                }
            }
        }

        StyledListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true

            model: root.menuModel

            navigation.section: root.navigationSection
            navigation.order: 2
            //: The list of commands inside the "Command menu" pop-up
            accessible.name: qsTrc("pointertools", "Commands")

            // Headings only while nothing is typed; a search result is one flat list
            section.property: root.searching ? "" : "sectionTitle"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                id: heading

                required property string section

                width: list.width
                height: 28

                StyledTextLabel {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.topMargin: 4

                    text: heading.section
                    font: ui.theme.bodyBoldFont
                    color: ui.theme.fontPrimaryColor
                        opacity: 0.7
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            delegate: ListItemBlank {
                id: item

                required property int index
                required property string title
                required property string keys
                required property string sectionTitle
                required property int icon

                implicitHeight: 32

                isSelected: item.index === root.currentRow

                navigation.panel: list.navigation
                navigation.row: item.index
                navigation.name: item.title
                navigation.accessible.role: MUAccessible.MenuItem
                navigation.accessible.name: item.keys !== "" ? qsTrc("pointertools", "%1, %2").arg(item.title).arg(item.keys) : item.title

                onClicked: {
                    root.currentRow = item.index
                    root.runRow(item.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 12
                    spacing: 8

                    StyledIconLabel {
                        Layout.preferredWidth: 24
                        iconCode: item.icon
                    }

                    StyledTextLabel {
                        Layout.fillWidth: !root.searching
                        text: item.title
                        horizontalAlignment: Text.AlignLeft
                    }

                    // while searching: where the command lives, small and dim
                    StyledTextLabel {
                        Layout.fillWidth: true
                        visible: root.searching
                        text: item.sectionTitle
                        font.pixelSize: ui.theme.bodyFont.pixelSize - 2
                        color: ui.theme.fontPrimaryColor
                        opacity: 0.7
                        horizontalAlignment: Text.AlignLeft
                    }

                    StyledTextLabel {
                        visible: item.keys !== ""
                        text: item.keys
                        color: ui.theme.fontPrimaryColor
                        opacity: 0.7
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
    }
}
