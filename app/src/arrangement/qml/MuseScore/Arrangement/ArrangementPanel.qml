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
import MuseScore.Arrangement

// The Arrangement dock panel (FEATURES §1). Hosted by a DockPanel in
// src/appshell/qml/MuseScore/AppShell/NotationPage/NotationPage.qml (see the TODO there).
// Look and behaviour to match: src/playback/qml/MuseScore/Playback/MixerPanel.qml.
//
// Layout:
//
//   +--------------+---------------------------------------------------+
//   | (corner)     | Ruler: bar numbers; second row min:sec             |
//   +--------------+---------------------------------------------------+
//   | Track header | Lane: clips with waveforms                    |    |
//   | name, arm,   |                                               |    |
//   | monitor,     |                                     playhead->|    |
//   | mute, solo   |                                               |    |
//   +--------------+---------------------------------------------------+
//   | + Add track  | (drop audio files here to create a track)          |
//   +--------------+---------------------------------------------------+
//
// Headers scroll vertically with lanes; ruler scrolls horizontally with lanes.
//
// TODO(FontDaw), in build order:
//   1. Static view: ruler, rows from the model, clips as rectangles with WaveformItem, playhead.
//   2. Click on ruler/empty lane seeks (model.seekTo). Playhead follows playback and the view
//      auto-scrolls while playing, like the score view does.
//   3. Zoom (Ctrl+wheel, pinch) and scroll.
//   4. Clip drag (move), edge drag (trim), split at playhead, delete. Live snap feedback via
//      model.snapX(); commit once on release.
//   5. Fade handles in the clip's top corners; gain handle on the clip's top edge.
//   6. Drop files from the OS. Context menus. Keyboard navigation and accessibility.
//   7. Automation lanes under a track (FEATURES §8).
//
// Use ui.theme colours and fonts only; no hard-coded colours.

Item {
    id: root

    property NavigationSection navigationSection: null
    property int contentNavigationPanelOrderStart: 1

    ArrangementPanelModel {
        id: arrangementModel
    }

    Component.onCompleted: {
        arrangementModel.load()
    }

    StyledTextLabel {
        anchors.centerIn: parent
        text: qsTrc("arrangement", "Arrangement view: not implemented yet")
    }
}
