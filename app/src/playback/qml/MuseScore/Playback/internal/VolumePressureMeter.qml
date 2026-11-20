/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

import Muse.UiComponents
import Muse.Ui

//! FontDaw (FD-191): the meter is drawn on the fader's scale (-60..+12 dB, same curve), so a level
//! at 0 dBFS sits level with the fader's 0 dB mark. Its colours are tied to dBFS, not to height:
//! green, yellow from -18, orange from -6, red from 0 dBFS (the part above full scale).
Canvas {
    id: root

    property real currentVolumePressure: -60.0

    property real minDisplayedVolumePressure: -60.0
    property real maxDisplayedVolumePressure: 12.0

    //! Position (0 bottom, 1 top) of a level in dB on the fader's travel; set from MixerChannelItem
    property var positionOf: function(db) {
        return (db - root.minDisplayedVolumePressure) / (root.maxDisplayedVolumePressure - root.minDisplayedVolumePressure)
    }

    property bool showRuler: false

    property bool isClipping: currentVolumePressure >= 0.0

    width: root.showRuler ? prv.indicatorWidth + prv.rulerWidth : prv.indicatorWidth
    height: prv.indicatorHeight + (prv.capHeight * 2)

    QtObject {
        id: prv

        // The bar spans the same 140 px as the fader's travel (VolumeSlider: height - handleHeight)
        readonly property real indicatorHeight: 140
        readonly property real indicatorWidth: 6
        readonly property real capHeight: 4
        readonly property real rulerWidth: 24

        readonly property color backgroundColor: "#4D4D4D"
        readonly property color overBackgroundColor: "#5C3A3A"
        readonly property color capColor: "#666666"

        //! Colour zones in dBFS, from the bottom up
        readonly property var zones: [
            { from: -1000.0, to: -18.0, color: "#26E386" },
            { from: -18.0, to: -6.0, color: "#CBED41" },
            { from: -6.0, to: 0.0, color: "#FC8226" },
            { from: 0.0, to: 1000.0, color: "#FF1C1C" }
        ]

        //! Ruler marks, at the fader's own label values
        readonly property var labelledMarks: [12, 6, 0, -6, -12, -24, -36, -48, -60]
        readonly property var shortMarks: [-18, -30, -42, -54]

        readonly property color unitTextColor: Utils.colorWithAlpha(ui.theme.fontPrimaryColor, 0.8)
        readonly property string unitTextFont: {
            var pxSize = String('8px')
            var family = String('\'' + ui.theme.bodyFont.family + '\'')

            return pxSize + ' ' + family
        }

        readonly property real strokeHorizontalMargin: 2
        readonly property real longStrokeWidth: 5
        readonly property color longStrokeColor: Utils.colorWithAlpha(ui.theme.fontPrimaryColor, 0.5)
        readonly property real shortStrokeWidth: 2
        readonly property color shortStrokeColor: Utils.colorWithAlpha(ui.theme.fontPrimaryColor, 0.3)

        onUnitTextColorChanged: { prv.rulerNeedsPaint = true; root.requestPaint() }
        onUnitTextFontChanged: { prv.rulerNeedsPaint = true; root.requestPaint() }
        onLongStrokeColorChanged: { prv.rulerNeedsPaint = true; root.requestPaint() }
        onShortStrokeColorChanged: { prv.rulerNeedsPaint = true; root.requestPaint() }

        property bool rulerNeedsPaint: true

        //! Distance from the canvas's bottom edge for a level in dB
        function offsetOf(db) {
            var pos = Math.max(0.0, Math.min(1.0, root.positionOf(db)))
            return prv.capHeight + pos * prv.indicatorHeight
        }

        function textOf(db) {
            if (db === root.minDisplayedVolumePressure) {
                return "-" + "∞" // minus infinity, as on the fader
            }

            return db > 0 ? "+" + db : String(db)
        }
    }

    function drawRuler(ctx) {
        ctx.clearRect(0, prv.indicatorWidth, root.height, root.width - prv.indicatorWidth)
        ctx.font = prv.unitTextFont

        var originHPos = prv.indicatorWidth + prv.strokeHorizontalMargin

        for (var i = 0; i < prv.shortMarks.length; ++i) {
            ctx.fillStyle = prv.shortStrokeColor
            ctx.fillRect(prv.offsetOf(prv.shortMarks[i]), originHPos, 1, prv.shortStrokeWidth)
        }

        for (var j = 0; j < prv.labelledMarks.length; ++j) {
            var db = prv.labelledMarks[j]
            var vPos = prv.offsetOf(db)

            ctx.fillStyle = prv.longStrokeColor
            ctx.fillRect(vPos, originHPos, 1, prv.longStrokeWidth)

            var textHPos = originHPos + prv.longStrokeWidth + prv.strokeHorizontalMargin

            ctx.save()
            ctx.rotate(Math.PI / 2)
            ctx.fillStyle = prv.unitTextColor
            ctx.fillText(prv.textOf(db), textHPos, -vPos + 3)
            ctx.restore()
        }

        prv.rulerNeedsPaint = false
    }

    onPaint: {
        var ctx = root.context

        if (!ctx) {
            ctx = getContext("2d")
            // x runs up from the bottom edge, y runs across the meter
            ctx.translate(0, root.height)
            ctx.rotate(3 * (Math.PI / 2))

            if (ui.currentLanguageLayoutDirection() === Qt.RightToLeft) {
                ctx.textAlign = "end"
            } else {
                ctx.textAlign = "start"
            }
        }

        ctx.clearRect(0, 0, root.height, prv.indicatorWidth)

        // Background, with the part above full scale tinted red
        var zeroOffset = prv.offsetOf(0.0)
        var topOffset = prv.capHeight + prv.indicatorHeight
        ctx.fillStyle = prv.backgroundColor
        ctx.fillRect(prv.capHeight, 0, zeroOffset - prv.capHeight, prv.indicatorWidth)
        ctx.fillStyle = prv.overBackgroundColor
        ctx.fillRect(zeroOffset, 0, topOffset - zeroOffset, prv.indicatorWidth)

        // Clip cap above the bar: lit while the level is at or above 0 dBFS
        ctx.fillStyle = root.isClipping ? "#FF1C1C" : prv.capColor
        ctx.fillRect(topOffset, 0, prv.capHeight, prv.indicatorWidth)

        // The level, coloured zone by zone
        if (root.currentVolumePressure > root.minDisplayedVolumePressure) {
            var levelOffset = prv.offsetOf(root.currentVolumePressure)

            for (var i = 0; i < prv.zones.length; ++i) {
                var zone = prv.zones[i]
                var start = Math.max(prv.capHeight, prv.offsetOf(zone.from))
                var end = Math.min(levelOffset, prv.offsetOf(zone.to))
                if (end > start) {
                    ctx.fillStyle = zone.color
                    ctx.fillRect(start, 0, end - start, prv.indicatorWidth)
                }
            }
        }

        if (prv.rulerNeedsPaint) {
            drawRuler(ctx)
        }
    }

    onCurrentVolumePressureChanged: {
        requestPaint()
    }

    onPositionOfChanged: {
        prv.rulerNeedsPaint = true
        requestPaint()
    }

    Component.onCompleted: {
        prv.rulerNeedsPaint = true
        requestPaint()
    }
}
