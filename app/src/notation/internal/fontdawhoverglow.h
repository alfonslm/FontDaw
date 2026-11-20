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

#include <QColor>
#include <QImage>

#include "draw/types/geometry.h"
#include "draw/types/transform.h"

namespace mu::engraving {
class EngravingItem;
namespace rendering {
class IScoreRenderer;
}
}

// FD-156: the hover glow, a soft accent-colour halo around what a click would act on. The drawing
// pieces live here, without state, so they can be tested; NotationInteraction::drawHoverGlow
// chooses the items and caches the result.
namespace mu::notation::fontdaw {
// FD-156, owner's choices (19 November 2026); both can be fine-tuned on the Mac build
constexpr double HOVER_GLOW_RADIUS = 3.0;         // blur radius in view pixels (times the gui scaling on screen)
constexpr double HOVER_GLOW_PEAK_ALPHA = 0.9;     // alpha at the shape's edge, fading out
constexpr double HOVER_GLOW_BAR_PEAK_ALPHA = 0.3; // a bar (measure hit) glows fainter
constexpr double HOVER_GLOW_EDGE_GAIN = 2.6;      // blurred alpha is full at the shape's edge

// Three box-blur passes (horizontal then vertical each pass) on the alpha channel of an
// ARGB32_Premultiplied image, radius r pixels; close to a Gaussian.
void boxBlurAlpha(QImage& image, int radius);

// From a mask (alpha = the shape): blur, scale by the edge gain and the peak alpha, tint with the colour.
QImage glowFromMask(const QImage& mask, int radius, const QColor& color, double peakAlpha);

// The margin around the item's rectangle that the glow needs: view pixels, or device pixels when
// `scale` (the screen's gui scaling) is given.
int hoverGlowMargin(double scale = 1.0);

// All the glow geometry is in device pixels: the painter's transform in the page-sheet hook maps page
// coordinates to device pixels (the view applies the matrix and the gui scaling), and the glow is
// placed 1:1 with an identity transform.

// The device-pixel area of one glow image: the item's device rectangle grown by the margin, clipped to
// the visible frame (its device rectangle, also grown by the margin) when the frame is valid, rounded
// out to whole pixels. Empty when nothing of it is visible.
muse::RectF glowArea(const muse::RectF& deviceRect, int margin, const muse::RectF& deviceFrame);

// The transform that paints page coordinates into an image whose pixel (0, 0) is the area's top left.
muse::draw::Transform maskTransform(const muse::draw::Transform& pageToDevice, const muse::RectF& area);

// An empty mask for the area.
QImage emptyMask(const muse::RectF& area);

// The mask of a bar: a rounded rectangle over the item's device rectangle, inside the area.
QImage rectMask(const muse::RectF& area, const muse::RectF& deviceRect);

// The mask of an item: the item painted in white by the score renderer.
QImage itemMask(const engraving::rendering::IScoreRenderer* renderer, const engraving::EngravingItem* item,
                const muse::draw::Transform& pageToDevice, const muse::RectF& area);

// The page rectangle the glow of an item is made from: the item's bounding box; for a measure with a
// known staff, only that staff's part of the bar (the measure's x range, the staff's y range in its
// system). Falls back to the measure's box when the staff is unknown or not shown in the system.
muse::RectF hoverGlowPageRect(const engraving::EngravingItem* item, size_t staff);

// Whether the hover glow may be drawn: on screen only. `isOutput` is true for a real output
// (NotationPainting::paintPdf, paintPrint and paintPng call doPaint with isOutput = true); `isPrinting`
// is the paint option that publish mode and read-only views also set on screen.
bool hoverGlowAllowed(bool isOutput, bool isPrinting);
}
