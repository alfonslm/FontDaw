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

#include "arrangementpanelmodel.h"

using namespace mu::arrangement;

ArrangementPanelModel::ArrangementPanelModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

// Implementation notes.
//
// - load(): fill rows from arrangement()->tracks(); subscribe to changed():
//   TracksReset -> beginResetModel/endResetModel; ClipsChanged/TrackChanged -> dataChanged for
//   that row only. Subscribe to peaksReady so clips repaint when their waveform arrives.
// - x <-> tick: x = quarters(tick) * pixelsPerQuarter. Clip width needs the clip's length in
//   seconds converted to ticks through the tempo map at its position (the right edge moves
//   when tempo changes; that is correct).
// - Snap: to the grid implied by zoom (bar, beat, 1/2, 1/4 beat...), and to neighbouring clip
//   edges and the playhead. Hold a modifier to bypass snap (handle in QML, pass a flag).
// - playheadX: follow IPlaybackController's position channel. Do not poll with a timer.
// - Zoom: Ctrl+wheel around the mouse position; keep the tick under the cursor fixed.
// - Notation rows show something useful rather than nothing: v1 can draw one block per
//   non-empty bar (which bars have notes). Double-click jumps the score view there.
// - Drag and drop of audio files from the OS onto a track or onto empty space (new track).
// - All strings through muse::qtrc("arrangement", ...).
// - Accessibility and keyboard navigation: follow NavigationPanel usage in MixerPanel.qml.
//   A panel that only works with a mouse will not be accepted.
