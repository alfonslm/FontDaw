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

#include "fontdawhoverglow.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QPainter>

#include "draw/painter.h"

#include "engraving/dom/engravingitem.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/system.h"
#include "engraving/rendering/iscorerenderer.h"
#include "engraving/rendering/paintoptions.h"

using namespace muse;
using namespace mu::notation;

namespace {
// One box-blur pass along one axis over a w x h float buffer; samples outside count as 0.
void blurPass(std::vector<float>& data, std::vector<float>& tmp, int w, int h, int radius, bool horizontal)
{
    const int lines = horizontal ? h : w;
    const int len = horizontal ? w : h;
    const float norm = 1.0f / float(2 * radius + 1);

    for (int line = 0; line < lines; ++line) {
        auto at = [&](int i) -> float& {
            return horizontal ? data[size_t(line) * w + i] : data[size_t(i) * w + line];
        };
        auto out = [&](int i) -> float& {
            return horizontal ? tmp[size_t(line) * w + i] : tmp[size_t(i) * w + line];
        };

        float sum = 0.f;
        for (int i = 0; i <= std::min(radius, len - 1); ++i) {
            sum += at(i);
        }
        for (int i = 0; i < len; ++i) {
            out(i) = sum * norm;
            const int add = i + radius + 1;
            const int sub = i - radius;
            if (add < len) {
                sum += at(add);
            }
            if (sub >= 0) {
                sum -= at(sub);
            }
        }
    }
    data.swap(tmp);
}
}

void fontdaw::boxBlurAlpha(QImage& image, int radius)
{
    if (image.isNull() || radius < 1) {
        return;
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    const int w = image.width();
    const int h = image.height();
    std::vector<float> data(size_t(w) * h);
    std::vector<float> tmp(data.size());

    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            data[size_t(y) * w + x] = float(qAlpha(line[x]));
        }
    }

    for (int pass = 0; pass < 3; ++pass) {
        blurPass(data, tmp, w, h, radius, true);
        blurPass(data, tmp, w, h, radius, false);
    }

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = std::clamp(int(std::lround(data[size_t(y) * w + x])), 0, 255);
            line[x] = qRgba(a, a, a, a); // premultiplied white; only the alpha is used afterwards
        }
    }
}

QImage fontdaw::glowFromMask(const QImage& mask, int radius, const QColor& color, double peakAlpha)
{
    if (mask.isNull()) {
        return QImage();
    }

    QImage glow = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    boxBlurAlpha(glow, radius);

    for (int y = 0; y < glow.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(glow.scanLine(y));
        for (int x = 0; x < glow.width(); ++x) {
            const double a = std::min(1.0, qAlpha(line[x]) / 255.0 * HOVER_GLOW_EDGE_GAIN) * peakAlpha;
            line[x] = qRgba(int(std::lround(color.red() * a)), int(std::lround(color.green() * a)),
                            int(std::lround(color.blue() * a)), int(std::lround(a * 255.0)));
        }
    }
    return glow;
}

int fontdaw::hoverGlowMargin(double scale)
{
    return int(std::ceil(3 * HOVER_GLOW_RADIUS * scale));
}

RectF fontdaw::glowArea(const RectF& deviceRect, int margin, const RectF& deviceFrame)
{
    RectF area = deviceRect.adjusted(-margin, -margin, margin, margin);
    if (deviceFrame.isValid()) {
        area = area.intersected(deviceFrame.adjusted(-margin, -margin, margin, margin));
    }
    if (!area.isValid()) {
        return RectF();
    }

    const double left = std::floor(area.left());
    const double top = std::floor(area.top());
    return RectF(left, top, std::ceil(area.right()) - left, std::ceil(area.bottom()) - top);
}

draw::Transform fontdaw::maskTransform(const draw::Transform& pageToDevice, const RectF& area)
{
    // (Transform multiplication applies the left operand first.)
    draw::Transform shift;
    shift.translate(-area.x(), -area.y());
    return pageToDevice * shift;
}

QImage fontdaw::emptyMask(const RectF& area)
{
    QImage mask(std::max(1, int(std::ceil(area.width()))), std::max(1, int(std::ceil(area.height()))),
                QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    return mask;
}

QImage fontdaw::rectMask(const RectF& area, const RectF& deviceRect)
{
    QImage mask = emptyMask(area);
    QPainter p(&mask);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawRoundedRect(QRectF(deviceRect.x() - area.x(), deviceRect.y() - area.y(), deviceRect.width(), deviceRect.height()),
                      2.0, 2.0);
    return mask;
}

QImage fontdaw::itemMask(const engraving::rendering::IScoreRenderer* renderer, const engraving::EngravingItem* item,
                         const draw::Transform& pageToDevice, const RectF& area)
{
    QImage mask = emptyMask(area);
    {
        draw::Painter mp(&mask, "hoverglowmask");
        mp.setWorldTransform(maskTransform(pageToDevice, area));

        // everything the item draws becomes white; only the alpha (the shape) is used
        engraving::rendering::PaintOptions opt;
        opt.isPrinting = false;
        opt.overrideItemColor = [](const engraving::EngravingItem*, Color) {
            return Color::WHITE;
        };
        renderer->paintItem(mp, item, opt);
    }
    return mask;
}

namespace mu::notation::fontdaw {
muse::RectF hoverGlowPageRect(const engraving::EngravingItem* item, size_t staff)
{
    const muse::RectF whole = item->ldata()->bbox().translated(item->pagePos());
    if (!item->isMeasure() || staff == muse::nidx) {
        return whole;
    }

    const engraving::System* system = engraving::toMeasure(item)->system();
    if (!system || staff >= system->staves().size()) {
        return whole;
    }

    const engraving::SysStaff* sysStaff = system->staff(staff);
    if (!sysStaff || !sysStaff->show() || sysStaff->bbox().height() <= 0.0) {
        return whole;
    }

    return muse::RectF(whole.x(), system->staffYpage(staff), whole.width(), sysStaff->bbox().height());
}

bool hoverGlowAllowed(bool isOutput, bool isPrinting)
{
    return !isOutput && !isPrinting;
}
}
