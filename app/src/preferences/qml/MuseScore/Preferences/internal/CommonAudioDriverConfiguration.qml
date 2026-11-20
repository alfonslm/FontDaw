/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
import MuseScore.Preferences

Item {
    id: root

    property int columnWidth: 0

    property NavigationPanel navigation: null
    property int navigationOrderStart: 0

    width: parent.width
    height: content.height

    CommonAudioDriverConfigurationModel {
        id: audioDriverModel
    }

    Component.onCompleted: {
        audioDriverModel.load()
    }

    Column {
        id: content

        spacing: 12

        ComboBoxWithTitle {
            title: qsTrc("preferences", "Audio device")
            columnWidth: root.columnWidth

            currentIndex: indexOfValue(audioDriverModel.currentDeviceId)
            model: audioDriverModel.deviceList

            navigationName: "AudioDeviceBox"
            navigationPanel: root.navigation
            navigationRow: root.navigationOrderStart

            onValueEdited: function(newIndex, newValue) {
                audioDriverModel.deviceSelected(newValue)
            }
        }

        ComboBoxWithTitle {
            id: bufferSize

            title: qsTrc("preferences", "Buffer size")
            columnWidth: root.columnWidth

            currentIndex: indexOfValue(audioDriverModel.bufferSize)
            model: audioDriverModel.bufferSizeList

            navigationName: "BufferSizeBox"
            navigationPanel: root.navigation
            navigationRow: root.navigationOrderStart + 1

            onValueEdited: function(newIndex, newValue) {
                audioDriverModel.bufferSizeSelected(newValue)
            }
        }

        ComboBoxWithTitle {
            id: sampleRate

            title: qsTrc("preferences", "Sample rate")
            columnWidth: root.columnWidth

            currentIndex: indexOfValue(audioDriverModel.sampleRate)
            model: audioDriverModel.sampleRateList

            navigationName: "SampleRateBox"
            navigationPanel: root.navigation
            navigationRow: root.navigationOrderStart + 2

            onValueEdited: function(newIndex, newValue) {
                audioDriverModel.sampleRateSelected(newValue)
            }

            visible: Qt.platform.os === "linux"
        }

        // Sync of the playback cursor with the sound

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap

            text: audioDriverModel.detectedOutputLatency >= 0
                  //: %1 is a number of milliseconds
                  ? qsTrc("preferences", "Output latency detected for this device: %1 ms. The playback cursor is adjusted for it automatically.")
                    .arg(audioDriverModel.detectedOutputLatency)
                  : qsTrc("preferences", "The output latency of this device could not be detected. Use the setting below to align the playback cursor with the sound.")
        }

        IncrementalPropertyControlWithTitle {
            title: qsTrc("preferences", "Additional output latency for this device")
            columnWidth: root.columnWidth
            control.width: 112

            currentValue: audioDriverModel.outputLatencyCompensation
            minValue: -500
            maxValue: 2000
            measureUnitsSymbol: qsTrc("global", "ms")

            navigation.name: "OutputLatencyCompensationControl"
            navigation.panel: root.navigation
            navigation.row: root.navigationOrderStart + 3

            onValueEdited: function(newValue) {
                audioDriverModel.outputLatencyCompensationEdited(newValue)
            }
        }

        IncrementalPropertyControlWithTitle {
            title: qsTrc("preferences", "Display latency")
            columnWidth: root.columnWidth
            control.width: 112

            currentValue: audioDriverModel.displayLatency
            minValue: 0
            maxValue: 500
            measureUnitsSymbol: qsTrc("global", "ms")

            navigation.name: "DisplayLatencyControl"
            navigation.panel: root.navigation
            navigation.row: root.navigationOrderStart + 4

            onValueEdited: function(newValue) {
                audioDriverModel.displayLatencyEdited(newValue)
            }
        }
    }
}
