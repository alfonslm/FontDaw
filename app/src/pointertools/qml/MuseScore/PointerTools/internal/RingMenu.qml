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

import Muse.Ui
import Muse.UiComponents
import MuseScore.PointerTools

// The ring menu (docs/fontdaw/design/shortcuts/08-pointer-tools-look.md): one ring cut into equal
// segments, one command per segment: icons, and names under all but the engraving symbols (owner,
// 19 November 2026). The last slot ("More…") is straight down, the others follow clockwise. This item draws the ring and handles the keys; the pointer (grab, move,
// click) is handled by PointerToolsLayer.qml through RingInputArea.
Item {
    id: root

    // ---- The ring's size: named values, in one place, to be tuned after trying a build ----------
    readonly property real diameter: 184
    readonly property real holeDiameter: 48
    readonly property real iconSize: 22
    readonly property real namedIconSize: 18
    readonly property real nameFontSize: 9.5
    // -------------------------------------------------------------------------------------------

    readonly property real rimWidth: 2
    readonly property real gapWidth: 2
    readonly property real holeRadius: holeDiameter / 2
    readonly property real outerRadius: diameter / 2 - rimWidth / 2

    required property RingMenuModel ringModel

    //! The ring's centre, in the coordinates of the parent (the layer)
    property point centre: Qt.point(0, 0)

    readonly property var slots: ringModel.ringSlots
    readonly property int slotCount: slots.length

    //! The segment that is highlighted (by the pointer or the keys), or -1
    property int highlighted: -1

    signal activateRequested(int index)
    signal closeRequested()

    //! The segment in the pointer's direction from the centre, for the press-move-release gesture:
    //! the whole wedge out to the edge of the view counts (the design), or -1 in the hole
    function segmentInDirection(pos) {
        return root.ringModel.segmentAt(pos.x - root.centre.x, pos.y - root.centre.y,
                                        root.slotCount, root.holeRadius, Number.MAX_VALUE)
    }

    //! The segment under a point, for a click once the ring is open: only inside the drawn ring;
    //! -1 in the hole or outside the rim (a click there closes the ring)
    function segmentUnder(pos) {
        return root.ringModel.segmentAt(pos.x - root.centre.x, pos.y - root.centre.y,
                                        root.slotCount, root.holeRadius, root.diameter / 2)
    }

    function moveHighlight(delta) {
        if (root.slotCount === 0) {
            return
        }

        var next = root.highlighted < 0
                ? (delta > 0 ? 0 : root.slotCount - 1)
                : (root.highlighted + delta + root.slotCount) % root.slotCount
        root.highlighted = next

        // let a screen reader follow the highlight
        var control = controls.itemAt(next)
        if (control) {
            control.navigation.requestActive()
        }
    }

    function activateNavigation() {
        navSection.requestActive()
    }

    x: root.centre.x - root.width / 2
    y: root.centre.y - root.height / 2
    width: root.diameter
    height: root.diameter

    // no focus of its own: the score view keeps it (PointerToolsLayer.qml, one focus model)
    activeFocusOnTab: false

    onHighlightedChanged: canvas.requestPaint()
    onVisibleChanged: canvas.requestPaint()
    onSlotCountChanged: canvas.requestPaint()

    Connections {
        target: ui.theme

        function onThemeChanged() {
            canvas.requestPaint()
        }
    }

    //! A key while the ring is open. The keys come from RingInputArea's key capture (the score
    //! view keeps the keyboard focus), not from this item's own focus.
    function handleKey(key) {
        switch (key) {
        case Qt.Key_Right:
        case Qt.Key_Down:
            root.moveHighlight(1)
            break
        case Qt.Key_Left:
        case Qt.Key_Up:
            root.moveHighlight(-1)
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            if (root.highlighted >= 0) {
                root.activateRequested(root.highlighted)
            }
            break
        case Qt.Key_Escape:
            root.closeRequested()
            break
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)

            var n = root.slotCount
            if (n < 1) {
                return
            }

            var cx = width / 2
            var cy = height / 2
            var outer = root.outerRadius
            var inner = root.holeRadius
            var step = 2 * Math.PI / n

            var accent = ui.theme.accentColor
            var accentFill = Qt.rgba(accent.r, accent.g, accent.b, 0.35)

            // slot i is centred at 180 degrees + (i + 1) * step clockwise from up, the last one at 180;
            // the canvas measures its angles clockwise from east, so up is -90 degrees
            function centreAngle(i) {
                var fromUp = (i === n - 1) ? Math.PI : Math.PI + (i + 1) * step
                return fromUp - Math.PI / 2
            }

            for (var i = 0; i < n; ++i) {
                var a = centreAngle(i)
                var start = a - step / 2
                var end = a + step / 2

                ctx.beginPath()
                ctx.arc(cx, cy, outer, start, end, false)
                ctx.arc(cx, cy, inner, end, start, true)
                ctx.closePath()

                ctx.fillStyle = ui.theme.popupBackgroundColor
                ctx.fill()

                if (i === root.highlighted) {
                    ctx.fillStyle = accentFill
                    ctx.fill()
                }
            }

            // the gaps between segments
            if (n > 1) {
                ctx.strokeStyle = ui.theme.strokeColor
                ctx.lineWidth = root.gapWidth
                for (var j = 0; j < n; ++j) {
                    var b = centreAngle(j) + step / 2
                    ctx.beginPath()
                    ctx.moveTo(cx + inner * Math.cos(b), cy + inner * Math.sin(b))
                    ctx.lineTo(cx + outer * Math.cos(b), cy + outer * Math.sin(b))
                    ctx.stroke()
                }
            }

            // the hole stays see-through, only its edge is drawn
            ctx.beginPath()
            ctx.arc(cx, cy, inner, 0, 2 * Math.PI, false)
            ctx.lineWidth = 1
            ctx.strokeStyle = ui.theme.strokeColor
            ctx.stroke()

            // the outer rim
            var rim = ui.theme.fontPrimaryColor
            ctx.beginPath()
            ctx.arc(cx, cy, outer, 0, 2 * Math.PI, false)
            ctx.lineWidth = root.rimWidth
            ctx.strokeStyle = Qt.rgba(rim.r, rim.g, rim.b, 0.7)
            ctx.stroke()
        }
    }

    // The segments' contents: the icon (or two letters) and, when the slot shows a name, the name
    // under it. The whole block is centred on the segment's centre.
    Repeater {
        model: root.slots

        Item {
            id: iconSlot

            required property int index
            required property var modelData

            readonly property point iconCentre: root.ringModel.segmentCentre(
                                                    iconSlot.index, root.slotCount,
                                                    (root.holeDiameter + root.diameter) / 4)
            readonly property bool hasIcon: iconSlot.modelData.icon !== IconCode.NONE
            readonly property bool hasName: iconSlot.modelData.showName === true
            readonly property real glyphSize: iconSlot.hasName ? root.namedIconSize : root.iconSize
            readonly property real nameWidth: root.ringModel.labelWidth(
                                                  iconSlot.index, root.slotCount, root.holeRadius,
                                                  root.diameter / 2, 84)

            width: iconSlot.hasName ? Math.max(iconSlot.nameWidth, iconSlot.glyphSize) : iconSlot.glyphSize
            height: iconBox.height + (iconSlot.hasName ? nameLabel.height + 2 : 0)

            x: root.width / 2 + iconSlot.iconCentre.x - iconSlot.width / 2
            y: root.height / 2 + iconSlot.iconCentre.y - iconSlot.height / 2

            Item {
                id: iconBox

                width: iconSlot.width
                height: iconSlot.glyphSize

                StyledIconLabel {
                    x: (iconBox.width - iconSlot.glyphSize) / 2
                    width: iconSlot.glyphSize
                    height: iconSlot.glyphSize
                    visible: iconSlot.hasIcon
                    iconCode: iconSlot.modelData.icon
                    font.pixelSize: iconSlot.glyphSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    // the diminuendo wedge is the crescendo wedge mirrored (the icon only, not the name)
                    transform: Scale {
                        origin.x: iconSlot.glyphSize / 2
                        xScale: iconSlot.modelData.iconMirrored ? -1 : 1
                    }
                }

                // a command without an icon: the first two letters of its name
                StyledTextLabel {
                    anchors.fill: parent
                    visible: !iconSlot.hasIcon
                    text: String(iconSlot.modelData.title).substring(0, 2)
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            StyledTextLabel {
                id: nameLabel

                y: iconBox.height + 2
                width: iconSlot.width
                visible: iconSlot.hasName
                text: String(iconSlot.modelData.title)
                font.pixelSize: root.nameFontSize
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.85
            }
        }
    }

    // Keyboard navigation and screen readers: the ring is a menu, each segment a menu item named
    // by its title, in ring order
    NavigationSection {
        id: navSection
        name: "RingMenu"
        type: NavigationSection.Exclusive
        enabled: root.visible
        order: 1
    }

    NavigationPanel {
        id: navPanel
        name: "RingMenu"
        section: navSection
        direction: NavigationPanel.Horizontal
        order: 1
        enabled: root.visible
        accessible.name: root.ringModel.accessibleName
    }

    Repeater {
        id: controls

        model: root.slots

        Item {
            id: control

            required property int index
            required property var modelData

            readonly property alias navigation: navControl

            NavigationControl {
                id: navControl
                name: String(control.modelData.title)
                panel: navPanel
                column: control.index
                order: control.index
                enabled: root.visible

                accessible.role: MUAccessible.MenuItem
                accessible.name: String(control.modelData.title)

                onActiveChanged: {
                    if (navControl.active) {
                        root.highlighted = control.index
                    }
                }

                onTriggered: {
                    root.activateRequested(control.index)
                }
            }
        }
    }
}
