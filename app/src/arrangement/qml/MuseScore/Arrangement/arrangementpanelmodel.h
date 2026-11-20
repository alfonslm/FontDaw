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

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "arrangement/iarrangement.h"
#include "arrangement/iaudioassetstore.h"

namespace mu::arrangement {
//! Backs ArrangementPanel.qml: one row per track, plus view state (zoom, scroll, snap).
//! Pattern to copy: src/playback/qml/MuseScore/Playback/mixerpanelmodel.h.
class ArrangementPanelModel : public QAbstractListModel, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT;

    // Horizontal scale. The panel draws in "pixels per quarter note" for Musical time so bars
    // have constant width regardless of tempo; an Absolute ruler (min:sec) is drawn from the
    // tempo map on top. TODO(FontDaw): confirm with a tempo-change score that this reads well.
    Q_PROPERTY(qreal pixelsPerQuarter READ pixelsPerQuarter WRITE setPixelsPerQuarter NOTIFY pixelsPerQuarterChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal playheadX READ playheadX NOTIFY playheadXChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)

    muse::ContextInject<IArrangement> arrangement = { this };
    muse::ContextInject<IAudioAssetStore> assetStore = { this };
    // TODO(FontDaw): inject playback::IPlaybackController for playhead position and seek,
    // and project::IProjectSession for the tempo map / time signatures (bar lines on the ruler).

public:
    explicit ArrangementPanelModel(QObject* parent = nullptr);

    enum Roles {
        TrackIdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,        // "notation" | "audio"
        ClipsRole,       // QVariantList of { clipId, x, width, name, assetId, sourceOffset, gainDb, muted }
        RecordArmedRole,
        MonitoringRole,
    };

    // TODO(FontDaw): rowCount/data/roleNames.

    Q_INVOKABLE void load();

    // --- Actions from QML. Positions arrive in pixels; the model converts and snaps. ---
    Q_INVOKABLE void addAudioTrack();
    Q_INVOKABLE void importAudio(const QVariant& trackId, qreal atX); // opens a file dialog
    Q_INVOKABLE void dropFiles(const QVariant& trackId, qreal atX, const QStringList& urls);
    Q_INVOKABLE void moveClip(const QVariant& fromTrackId, const QVariant& clipId, const QVariant& toTrackId, qreal newX);
    Q_INVOKABLE void trimClipStart(const QVariant& trackId, const QVariant& clipId, qreal newX);
    Q_INVOKABLE void trimClipEnd(const QVariant& trackId, const QVariant& clipId, qreal newRightX);
    Q_INVOKABLE void splitClipAt(const QVariant& trackId, const QVariant& clipId, qreal x);
    Q_INVOKABLE void removeClip(const QVariant& trackId, const QVariant& clipId);
    Q_INVOKABLE void seekTo(qreal x);
    Q_INVOKABLE qreal snapX(qreal x) const; // for live drag feedback

    // TODO(FontDaw): getters/setters/signals for the properties above.

signals:
    void pixelsPerQuarterChanged();
    void contentWidthChanged();
    void playheadXChanged();
    void snapEnabledChanged();
};
}
