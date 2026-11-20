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

#include <QObject>
#include <qqmlintegration.h>

#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "project/iprojectsession.h"

#include "pianoroll/pianorolltypes.h"

namespace mu::pianoroll {
//! Backs PianoRollPanel.qml. Shows one part at a time (the part of the current selection, or
//! the one picked in the panel's header).
class PianoRollModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT;

    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged) // { id, pitch, x, width, velocity, selected }
    Q_PROPERTY(qreal pixelsPerQuarter READ pixelsPerQuarter WRITE setPixelsPerQuarter NOTIFY pixelsPerQuarterChanged)
    Q_PROPERTY(qreal playheadX READ playheadX NOTIFY playheadXChanged)
    Q_PROPERTY(int lowestPitch READ lowestPitch NOTIFY notesChanged)   // for initial vertical scroll
    Q_PROPERTY(int highestPitch READ highestPitch NOTIFY notesChanged)

    muse::ContextInject<project::IProjectSession> projectSession = { this };
    // TODO(FontDaw): inject playback::IPlaybackController for the playhead and for auditioning
    // a note while dragging it (it already has playElements()).

public:
    explicit PianoRollModel(QObject* parent = nullptr);

    Q_INVOKABLE void load();

    // --- Edits. Each is ONE undo step on the notation undo stack. ---
    Q_INVOKABLE void setVelocity(const QVariantList& noteIds, int velocity);   // Note::setUserVelocity
    Q_INVOKABLE void trimStart(const QVariant& noteId, qreal newX);            // NoteEvent ontime
    Q_INVOKABLE void trimEnd(const QVariant& noteId, qreal newRightX);         // NoteEvent len
    Q_INVOKABLE void resetPerformance(const QVariantList& noteIds);            // back to as-written
    Q_INVOKABLE void moveNotes(const QVariantList& noteIds, int pitchDelta, qreal xDelta); // real notation edit
    Q_INVOKABLE void addNote(int pitch, qreal x, qreal width);                 // real notation edit
    Q_INVOKABLE void removeNotes(const QVariantList& noteIds);                 // real notation edit
    Q_INVOKABLE void select(const QVariantList& noteIds, bool addToSelection); // synced with the score view

    // TODO(FontDaw): getters/setters. Rebuild `notes` when the notation reports a change
    // (INotation::notationChanged) and when the selection changes; debounce while dragging.
    // Note ids given to QML must survive a rebuild: use the item's EID
    // (src/engraving/infrastructure/eid.h), never a raw pointer.

signals:
    void notesChanged();
    void pixelsPerQuarterChanged();
    void playheadXChanged();
};
}
