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

#pragma once

#include <QPointF>

namespace mu::pointertools {
//! The ring's geometry as pure functions, so it can be tested without a window.
//! Screen coordinates (y points down); angles are measured clockwise from straight up.
//! The ring's last slot ("More…") is centred straight down (180°); slot i (0 <= i < count - 1) is
//! centred at 180° + (i + 1) * 360° / count; each segment spans 360° / count.

//! The index of the slot under the point (dx, dy), measured from the ring's centre, or -1 when the
//! point is closer to the centre than holeRadius or farther than maxRadius.
int ringSegmentAt(double dx, double dy, int count, double holeRadius, double maxRadius);

//! The centre of slot `index` at distance `radius` from the ring's centre (for placing its icon).
QPointF ringSegmentCentre(int index, int count, double radius);

//! The width available for slot `index`'s name (FD-156, FD-166), mirroring the approved pictures'
//! renderer (docs/fontdaw/design/shortcuts/images/src/render_ring_labels.js, ring()). With the
//! ring's mid radius mid = (inner + outer) / 2, the chord of the segment at mid is
//! 2 * mid * sin(step / 2) - 6 and the ring's depth is outer - inner - 8; at the top and bottom
//! (where the text runs along the chord) the chord is the room, at the sides (where it runs along
//! the depth) the depth: width = chord + |sin(angle from straight down)| * (depth - chord),
//! at most maxWidth. 0 for count <= 0.
double ringLabelWidth(int index, int count, double innerRadius, double outerRadius, double maxWidth);

//! Whether the release of the button that opened the ring runs a segment. Only when the pointer
//! has left the dead zone (holeRadius) around the point where the ring was opened, and only the
//! highlighted segment. The ring's centre can be moved to keep the ring inside the view while the
//! pointer stays put, so a release where the ring opened must never run anything (review of #195).
bool ringOpeningReleaseChooses(const QPointF& openPos, const QPointF& releasePos, double holeRadius, int highlighted);
}
