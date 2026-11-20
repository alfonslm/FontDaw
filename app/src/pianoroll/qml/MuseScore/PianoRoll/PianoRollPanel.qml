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

// DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

import QtQuick

import Muse.Ui
import Muse.UiComponents
import MuseScore.PianoRoll

// The Piano roll dock panel (FEATURES §5). See src/pianoroll/README.md for what it edits.
//
// Layout:
//
//   +----------+---------------------------------------------+
//   | Part  v  | Ruler (bars) - shared code with Arrangement  |
//   +----------+---------------------------------------------+
//   | piano    | note bars on a pitch grid          playhead  |
//   | keys     |                                              |
//   +----------+---------------------------------------------+
//   | Velocity | one stalk per note                           |
//   +----------+---------------------------------------------+
//
// TODO(FontDaw): follow the build order in the README. Reuse, do not copy, the ruler / zoom /
// snap / playhead pieces from src/arrangement/qml/MuseScore/Arrangement once they exist; move
// them to a shared QML module if both panels need them.

Item {
    id: root

    property NavigationSection navigationSection: null
    property int contentNavigationPanelOrderStart: 1

    PianoRollModel {
        id: pianoRollModel
    }

    Component.onCompleted: {
        pianoRollModel.load()
    }

    StyledTextLabel {
        anchors.centerIn: parent
        text: qsTrc("pianoroll", "Piano roll: not implemented yet")
    }
}
