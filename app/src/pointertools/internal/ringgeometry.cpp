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

#include "ringgeometry.h"

#include <algorithm>
#include <cmath>

namespace mu::pointertools {
namespace {
constexpr double FULL_TURN = 360.0;
constexpr double HALF_TURN = 180.0;
constexpr double PI = 3.14159265358979323846;
}

int ringSegmentAt(double dx, double dy, int count, double holeRadius, double maxRadius)
{
    if (count <= 0) {
        return -1;
    }

    const double distance = std::hypot(dx, dy);
    if (distance < holeRadius || distance > maxRadius) {
        return -1;
    }

    // clockwise from straight up, 0 <= angle < 360
    double angle = std::atan2(dx, -dy) * HALF_TURN / PI;
    if (angle < 0.0) {
        angle += FULL_TURN;
    }

    // measured from the middle of "More…" (straight down)
    double relative = angle - HALF_TURN;
    if (relative < 0.0) {
        relative += FULL_TURN;
    }

    const double step = FULL_TURN / count;
    const int sector = static_cast<int>(std::floor((relative + step / 2.0) / step)) % count;

    // sector 0 is the last slot, sector 1 is slot 0, and so on
    return (sector - 1 + count) % count;
}

QPointF ringSegmentCentre(int index, int count, double radius)
{
    if (count <= 0) {
        return QPointF();
    }

    const double step = FULL_TURN / count;
    const double angle = (index == count - 1) ? HALF_TURN : HALF_TURN + (index + 1) * step;
    const double radians = angle * PI / HALF_TURN;

    return QPointF(radius * std::sin(radians), -radius * std::cos(radians));
}

double ringLabelWidth(int index, int count, double innerRadius, double outerRadius, double maxWidth)
{
    if (count <= 0) {
        return 0.0;
    }

    // Slot `index` is centred (index + 1) steps clockwise from straight down (the last slot,
    // "More…", at 0 steps), which is the renderer's i; sin is periodic, so no wrap is needed.
    const double step = 2.0 * PI / count;
    const double mid = (innerRadius + outerRadius) / 2.0;
    const double chord = 2.0 * mid * std::sin(step / 2.0) - 6.0;
    const double depth = outerRadius - innerRadius - 8.0;
    const double side = std::abs(std::sin((index + 1) * step));

    return std::min(chord + side * (depth - chord), maxWidth);
}
}

bool mu::pointertools::ringOpeningReleaseChooses(const QPointF& openPos, const QPointF& releasePos, double holeRadius,
                                                 int highlighted)
{
    if (highlighted < 0) {
        return false;
    }
    const QPointF moved = releasePos - openPos;
    return std::hypot(moved.x(), moved.y()) >= holeRadius;
}
