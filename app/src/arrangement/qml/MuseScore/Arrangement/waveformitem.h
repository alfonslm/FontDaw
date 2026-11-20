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

#pragma once

#include <QQuickPaintedItem>
#include <qqmlintegration.h>

#include "modularity/ioc.h"

#include "arrangement/iaudioassetstore.h"

namespace mu::arrangement {
//! Draws one clip's waveform from WaveformPeaks. One instance per visible clip.
//!
//! TODO(FontDaw): start with QQuickPaintedItem (simple). If scrolling long projects is slow,
//! switch to a QSGGeometryNode-based item; keep the QML API identical so nothing else changes.
class WaveformItem : public QQuickPaintedItem, public muse::Contextable
{
    Q_OBJECT
    QML_ELEMENT;

    Q_PROPERTY(QString assetId READ assetId WRITE setAssetId NOTIFY assetIdChanged)
    Q_PROPERTY(qreal sourceOffset READ sourceOffset WRITE setSourceOffset NOTIFY sourceOffsetChanged) // seconds
    Q_PROPERTY(qreal duration READ duration WRITE setDuration NOTIFY durationChanged)                 // seconds
    Q_PROPERTY(qreal gainDb READ gainDb WRITE setGainDb NOTIFY gainDbChanged) // scales the drawing too
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

    muse::ContextInject<IAudioAssetStore> assetStore = { this };

public:
    explicit WaveformItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    // paint(): only the visible x-range (clip against the viewport: a clip can be 100k px wide).
    // Per pixel column, take min/max over the peaks it covers and draw a vertical line.
    // Stereo: two half-height lanes. While peaks are missing: draw a flat placeholder.

    // TODO(FontDaw): getters/setters; call update() on any change and on peaksReady(assetId).

signals:
    void assetIdChanged();
    void sourceOffsetChanged();
    void durationChanged();
    void gainDbChanged();
    void colorChanged();
};
}
