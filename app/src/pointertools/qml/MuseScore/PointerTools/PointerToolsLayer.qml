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

import MuseScore.PointerTools

import "internal"

// The layer each view loads (NotationView.qml loads it by URL, so the score view works without
// this module). It fills the view and owns the pointer menu and the ring menu.
Item {
    id: root

    //! The setting "Ring menu on secondary click" (Preferences > Advanced)
    readonly property bool ringMenuEnabled: ringModel.ringMenuEnabled

    //! True while the pointer menu or the ring is open
    readonly property bool isOpen: pointerMenu.isOpened || prv.ringOpen

    //! An item of today's context menu was chosen in the ring (handle it with that menu's model)
    signal todayMenuItemRequested(string itemId)
    //! "More…" was chosen, or the ring has nothing else: show today's context menu at viewPos
    signal moreRequested(point viewPos)
    signal openedChanged(bool open)
    //! FontDaw (FD-156): the ring opened or closed; fromKeyboard: opened by the keyboard (no pointer)
    signal ringOpenChanged(bool open, bool fromKeyboard)
    //! FontDaw (FD-166): a secondary press while the ring was open: the view selects what is at
    //! viewPos by the press rule (never opens a menu) and the ring then reopens for the new target
    signal secondaryClickRequested(point viewPos)

    //! Opens the ring at viewPos (the view's coordinates). todayMenuItems: today's context menu
    //! items for the item under the pointer, for the ring's fallback and its "More…".
    function showRing(viewPos, todayMenuItems) {
        ringModel.load(todayMenuItems)

        // Only "More…" (an audio staff before clip commands exist): no ring, today's menu at once
        if (ringModel.ringSlots.length <= 1) {
            root.moreRequested(viewPos)
            return
        }

        pointerMenu.close()
        prv.openRing(viewPos)
    }

    function closeAll() {
        prv.closeRing()
        pointerMenu.close()
    }

    onIsOpenChanged: root.openedChanged(root.isOpen)

    QtObject {
        id: prv

        property bool ringOpen: false
        property point ringViewPos: Qt.point(0, 0)
        //! Where the pointer was when the ring opened: the opening release runs a segment only
        //! when the pointer has left the dead zone around it (the centre may have been moved)
        property point openPointerPos: Qt.point(0, 0)
        //! The release of the button that opened the ring is acted on once
        property bool openingReleasePending: false

        // One focus model: the score view keeps the keyboard focus while the ring is open (so the
        // Menu key and a second secondary click reach it and reopen the ring); the ring's keys
        // come through RingInputArea's key capture, the screen reader through the ring's
        // navigation section.
        function openRing(viewPos) {
            if (prv.ringOpen) {
                prv.closeRing()
            }

            prv.ringViewPos = viewPos

            // The pointer is not where the context menu was asked for when it came from the
            // keyboard (Shift+F10, the Menu key): the ring then has no pointer to follow
            var cursor = input.cursorPosition()
            prv.openPointerPos = cursor
            var fromKeyboard = Math.hypot(cursor.x - viewPos.x, cursor.y - viewPos.y) > ring.holeRadius

            // keep the whole ring inside the view
            var half = ring.diameter / 2
            ring.centre = Qt.point(prv.clamp(viewPos.x, half, root.width - half),
                                   prv.clamp(viewPos.y, half, root.height - half))
            ring.highlighted = fromKeyboard ? 0 : -1

            prv.ringOpen = true
            ring.visible = true
            root.ringOpenChanged(true, fromKeyboard)

            input.startGrab()
            input.setKeysCaptured(true)
            prv.openingReleasePending = input.buttonDownAtStart

            if (fromKeyboard) {
                Qt.callLater(ring.activateNavigation)
            }
        }

        function closeRing() {
            if (!prv.ringOpen) {
                return
            }

            prv.ringOpen = false
            root.ringOpenChanged(false, false)
            prv.openingReleasePending = false
            ring.visible = false
            ring.highlighted = -1
            input.setKeysCaptured(false)
            input.endGrab()
        }

        //! Closes the ring and runs the segment
        function choose(index) {
            prv.closeRing()
            ringModel.activate(index)
        }

        function clamp(value, low, high) {
            if (high < low) {
                return (low + high) / 2 // the view is smaller than the ring
            }
            return Math.max(low, Math.min(high, value))
        }
    }

    RingMenuModel {
        id: ringModel

        Component.onCompleted: ringModel.init()

        onTodayMenuItemRequested: function(itemId) {
            root.todayMenuItemRequested(itemId)
        }

        onMoreRequested: {
            root.moreRequested(prv.ringViewPos)
        }

        // the ring's slots and the menu's entries belong to the score they were made for
        onCurrentScoreChanged: root.closeAll()
    }

    RingMenu {
        id: ring

        visible: false
        ringModel: ringModel

        onActivateRequested: function(index) {
            prv.choose(index)
        }

        onCloseRequested: {
            prv.closeRing()
        }
    }

    // While the ring is open, follow the pointer and the button that opened it by reading the cursor
    // and the button state each frame. The score view keeps the pointer it got with the press (a
    // grab taken in the context-menu event does not move it, tried in the app), so the opening
    // drag and its release never reach this layer as events. Clicks after that do (RingInputArea).
    Timer {
        interval: 16
        repeat: true
        running: prv.ringOpen

        property point lastPos: Qt.point(-1, -1)

        onRunningChanged: lastPos = input.cursorPosition()

        onTriggered: {
            var pos = input.cursorPosition()
            // only a pointer that moves changes the highlight, so one set by the keys stays;
            // outside the view nothing is highlighted, so nothing runs there
            if (pos.x !== lastPos.x || pos.y !== lastPos.y) {
                lastPos = pos
                ring.highlighted = input.contains(pos) ? ring.segmentInDirection(pos) : -1
            }

            if (prv.openingReleasePending && !input.anyButtonDown()) {
                // the button that opened the ring is up: it runs the highlighted segment if the
                // pointer was moved out of the dead zone; otherwise the ring stays open
                prv.openingReleasePending = false
                if (ringModel.openingReleaseChooses(prv.openPointerPos, pos, ring.holeRadius, ring.highlighted)) {
                    prv.choose(ring.highlighted)
                }
            }
        }
    }

    // Clicks and keys while the ring is open
    RingInputArea {
        id: input

        anchors.fill: parent

        onKeyPressed: function(key) {
            if (prv.ringOpen) {
                ring.handleKey(key)
            }
        }

        // the ring's window lost the focus or the application went to the background: a dialog
        // or another window must get its keys back
        onFocusLost: prv.closeRing()

        // pointer movement is followed by the Timer above (only real movement changes the
        // highlight); a hover event sent when the ring appears must not clear a keyboard highlight

        onPressed: function(pos, button) {
            if (!prv.ringOpen) {
                return
            }

            // a click inside the drawn ring runs that segment; in the hole, outside the rim or with
            // the secondary button it closes the ring (the secondary press then goes on to the
            // view, which reopens the ring at the new spot)
            var index = button === Qt.RightButton ? -1 : ring.segmentUnder(pos)
            if (index >= 0) {
                prv.choose(index)
            } else {
                prv.closeRing()

                // owner's answer B, FD-158, 20 November 2026: the ring reopens for the new
                // target (the other option is to close it and select only). Undoing this default also
                // needs ringinputarea.cpp: it ignores the secondary press so that it goes on to the
                // view under the ring, and that must change too
                if (button === Qt.RightButton) {
                    root.secondaryClickRequested(pos)
                }
            }
        }
    }

    PointerMenuModel {
        id: menuModel

        Component.onCompleted: menuModel.init()

        onOpenRequested: {
            // every open view has a layer; only the one that is showing opens the menu
            if (!root.visible || root.width <= 0) {
                return
            }

            prv.closeRing()

            // at the pointer when it is over the view, else in the middle of it
            var pos = menuModel.cursorPosIn(root)
            if (pos.x < 0 || pos.y < 0 || pos.x > root.width || pos.y > root.height) {
                pos = Qt.point(root.width / 2, root.height / 2)
            }

            pointerMenu.showAt(pos)
        }
    }

    // zero-size parent at the pointer: PopupView opens below it (or above when there is no room)
    Item {
        id: pointerMenuAnchor

        width: 0
        height: 0

        PointerMenuPopup {
            id: pointerMenu

            menuModel: menuModel
        }
    }
}
