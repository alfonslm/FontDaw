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

// FD-156: tests for the selection core (hover target)

#include <gtest/gtest.h>

#include <algorithm>

#include <QRect>

#include "modularity/ioc.h"

#include "notation/inotationinteraction.h"
#include "notation/internal/fontdawhoverglow.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/system.h"
#include "engraving/dom/note.h"
#include "engraving/dom/segment.h"
#include "engraving/rendering/iscorerenderer.h"
#include "engraving/tests/utils/scorerw.h"

using namespace mu::notation;

TEST(FontDawSelectionTests, HoverTarget_Equality)
{
    // Distinct, never dereferenced pointer values
    auto* a = reinterpret_cast<mu::engraving::EngravingItem*>(0x10);
    auto* b = reinterpret_cast<mu::engraving::EngravingItem*>(0x20);

    INotationInteraction::HoverTarget empty;
    EXPECT_EQ(empty, INotationInteraction::HoverTarget {});
    EXPECT_EQ(empty.item, nullptr);
    EXPECT_FALSE(empty.wholeSelection);

    EXPECT_EQ((INotationInteraction::HoverTarget { a, false }), (INotationInteraction::HoverTarget { a, false }));
    EXPECT_NE((INotationInteraction::HoverTarget { a, false }), (INotationInteraction::HoverTarget { a, true }));
    EXPECT_NE((INotationInteraction::HoverTarget { a, false }), (INotationInteraction::HoverTarget { b, false }));
    EXPECT_NE((INotationInteraction::HoverTarget { a, true }), empty);
}

namespace {
QImage blockImage(int size, const QRect& block)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    for (int y = block.top(); y <= block.bottom(); ++y) {
        for (int x = block.left(); x <= block.right(); ++x) {
            img.setPixel(x, y, qRgba(255, 255, 255, 255));
        }
    }
    return img;
}

double totalAlpha(const QImage& img)
{
    double sum = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            sum += qAlpha(img.pixel(x, y));
        }
    }
    return sum;
}
}

TEST(FontDawSelectionTests, BoxBlur_SinglePixelSpreadsSymmetrically)
{
    QImage img = blockImage(41, QRect(20, 20, 1, 1));
    fontdaw::boxBlurAlpha(img, 3);

    // the single pixel's alpha is low after three passes, but the spread must be mirror symmetric
    for (int d = 1; d < 15; ++d) {
        EXPECT_EQ(qAlpha(img.pixel(20 - d, 20)), qAlpha(img.pixel(20 + d, 20))) << d;
        EXPECT_EQ(qAlpha(img.pixel(20, 20 - d)), qAlpha(img.pixel(20, 20 + d))) << d;
        EXPECT_EQ(qAlpha(img.pixel(20 - d, 20)), qAlpha(img.pixel(20, 20 - d))) << d;
    }
    EXPECT_GT(qAlpha(img.pixel(20, 20)), 0);
}

TEST(FontDawSelectionTests, BoxBlur_KeepsTotalAlpha)
{
    QImage img = blockImage(60, QRect(25, 25, 10, 10));
    const double before = totalAlpha(img);
    fontdaw::boxBlurAlpha(img, 3);
    const double after = totalAlpha(img);

    EXPECT_NEAR(after, before, before * 0.02);
    // blurred: the old corner is no longer opaque, and the block spread outwards
    EXPECT_LT(qAlpha(img.pixel(25, 25)), 255);
    EXPECT_GT(qAlpha(img.pixel(22, 30)), 0);
}

TEST(FontDawSelectionTests, GlowFromMask_EmptyMaskIsEmpty)
{
    QImage mask(30, 30, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    const QImage glow = fontdaw::glowFromMask(mask, 3, QColor(10, 120, 240), 0.9);

    ASSERT_FALSE(glow.isNull());
    EXPECT_EQ(totalAlpha(glow), 0.0);

    EXPECT_TRUE(fontdaw::glowFromMask(QImage(), 3, Qt::red, 0.9).isNull());
}

TEST(FontDawSelectionTests, GlowFromMask_PeakAlphaNeverExceeded)
{
    const QImage mask = blockImage(80, QRect(20, 20, 40, 40));
    const double peak = fontdaw::HOVER_GLOW_PEAK_ALPHA;
    const QImage glow = fontdaw::glowFromMask(mask, 3, QColor(10, 120, 240), peak);

    int maxA = 0;
    for (int y = 0; y < glow.height(); ++y) {
        for (int x = 0; x < glow.width(); ++x) {
            const QRgb px = glow.pixel(x, y);
            maxA = std::max(maxA, qAlpha(px));
        }
    }
    EXPECT_LE(maxA, int(std::lround(peak * 255)));
    EXPECT_GE(maxA, int(peak * 255) - 2); // the inside of a big shape reaches the peak

    // tinted with the colour (unpremultiplied)
    const QColor c = QColor::fromRgba(glow.convertToFormat(QImage::Format_ARGB32).pixel(40, 40));
    EXPECT_NEAR(c.red(), 10, 3);
    EXPECT_NEAR(c.green(), 120, 3);
    EXPECT_NEAR(c.blue(), 240, 3);
}

TEST(FontDawSelectionTests, MaskTransform_MapsDeviceAreaIntoImage)
{
    // page -> device: gui scaling 2 on top of zoom 1.5 (scale 3), then offset (30, 40)
    muse::draw::Transform pageToDevice;
    pageToDevice.translate(30.0, 40.0);
    pageToDevice.scale(3.0, 3.0);

    const muse::PointF pagePt(10.0, 20.0);
    const muse::PointF devPt = pageToDevice.map(pagePt);
    ASSERT_DOUBLE_EQ(devPt.x(), 60.0);
    ASSERT_DOUBLE_EQ(devPt.y(), 100.0);

    const muse::RectF deviceRect(devPt.x(), devPt.y(), 40.0, 20.0);
    const int m = fontdaw::hoverGlowMargin(2.0);
    EXPECT_EQ(m, 18); // 3 * radius 3 * scale 2
    EXPECT_EQ(fontdaw::hoverGlowMargin(), 9);

    const muse::RectF area = fontdaw::glowArea(deviceRect, m, muse::RectF());
    EXPECT_DOUBLE_EQ(area.x(), devPt.x() - m);
    EXPECT_DOUBLE_EQ(area.width(), 40.0 + 2 * m);

    const muse::draw::Transform t = fontdaw::maskTransform(pageToDevice, area);
    // the item's top left lands at (margin, margin), in device pixels
    const muse::PointF p = t.map(pagePt);
    EXPECT_DOUBLE_EQ(p.x(), m);
    EXPECT_DOUBLE_EQ(p.y(), m);

    // the bottom right of the device rect: (margin + size)
    const muse::PointF q = t.map(muse::PointF(10.0 + 40.0 / 3.0, 20.0 + 20.0 / 3.0));
    EXPECT_NEAR(q.x(), m + 40.0, 1e-9);
    EXPECT_NEAR(q.y(), m + 20.0, 1e-9);

    const QImage mask = fontdaw::emptyMask(area);
    EXPECT_EQ(mask.width(), 40 + 2 * m);
    EXPECT_EQ(mask.height(), 20 + 2 * m);
}

// FD-156: a large item clipped by a small visible frame gives a small mask
TEST(FontDawSelectionTests, GlowArea_ClippedByTheFrame)
{
    const int m = 9;
    const muse::RectF big(-5000.0, -5000.0, 10000.0, 10000.0);
    const muse::RectF frame(100.0, 200.0, 300.0, 150.0);

    const muse::RectF area = fontdaw::glowArea(big, m, frame);
    ASSERT_TRUE(area.isValid());
    EXPECT_LE(area.width(), frame.width() + 2 * m + 2);   // plus the rounding out
    EXPECT_LE(area.height(), frame.height() + 2 * m + 2);
    EXPECT_GE(area.left(), frame.left() - m - 1);

    // no frame: the item grown by the margin
    const muse::RectF small(10.5, 20.5, 40.0, 20.0);
    const muse::RectF unclipped = fontdaw::glowArea(small, m, muse::RectF());
    EXPECT_LE(unclipped.left(), small.left() - m);
    EXPECT_GE(unclipped.right(), small.right() + m);

    // an item the frame does not reach (not even through the margin): nothing
    EXPECT_FALSE(fontdaw::glowArea(muse::RectF(1000.0, 1000.0, 10.0, 10.0), m, frame).isValid());
}

TEST(FontDawSelectionTests, RectMask_FillsTheDeviceRectInsideTheImage)
{
    const muse::RectF deviceRect(100.0, 200.0, 40.0, 20.0);
    const int m = 9;
    const muse::RectF area = fontdaw::glowArea(deviceRect, m, muse::RectF());
    const QImage mask = fontdaw::rectMask(area, deviceRect);

    EXPECT_EQ(qAlpha(mask.pixel(m + 20, m + 10)), 255); // centre
    EXPECT_EQ(qAlpha(mask.pixel(2, 2)), 0);             // a corner, outside
    EXPECT_EQ(qAlpha(mask.pixel(m - 2, m + 10)), 0);    // just left of the rect
}

TEST(FontDawSelectionTests, ItemMask_NoteLandsInsideTheMask)
{
    mu::engraving::MasterScore* score = mu::engraving::ScoreRW::readScore(u"data/test.mscx");
    ASSERT_TRUE(score);

    const mu::engraving::Segment* seg = score->firstMeasure()->first(mu::engraving::SegmentType::ChordRest);
    ASSERT_TRUE(seg);
    const mu::engraving::EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el);
    const mu::engraving::EngravingItem* item = el->isChord() ? static_cast<const mu::engraving::EngravingItem*>(
        mu::engraving::toChord(el)->upNote()) : el;

    auto renderer = muse::modularity::globalIoc()->resolve<mu::engraving::rendering::IScoreRenderer>("test");
    ASSERT_TRUE(renderer);

    // page -> device pixels as the view sets it: zoom 2 and gui scaling 2, scrolled by (-300, -100)
    muse::draw::Transform pageToDevice;
    pageToDevice.translate(-300.0, -100.0);
    pageToDevice.scale(4.0, 4.0);

    const muse::RectF pageRect = item->ldata()->bbox().translated(item->pagePos());
    const muse::RectF deviceRect = pageToDevice.map(pageRect);
    ASSERT_FALSE(deviceRect.isEmpty());

    const int m = fontdaw::hoverGlowMargin(2.0);
    const muse::RectF area = fontdaw::glowArea(deviceRect, m, muse::RectF());
    const QImage mask = fontdaw::itemMask(renderer.get(), item, pageToDevice, area);

    // bounding box of the painted pixels
    QRect painted;
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            if (qAlpha(mask.pixel(x, y)) > 0) {
                painted = painted.isNull() ? QRect(x, y, 1, 1) : painted.united(QRect(x, y, 1, 1));
            }
        }
    }
    ASSERT_FALSE(painted.isNull()) << "nothing was painted into the mask";

    // inside the image, clear of its edge, and over the item's own rectangle (shifted by the margin)
    const QRect inner(m - 2, m - 2, int(deviceRect.width()) + 4, int(deviceRect.height()) + 4);
    EXPECT_TRUE(QRect(0, 0, mask.width(), mask.height()).adjusted(1, 1, -1, -1).contains(painted));
    EXPECT_TRUE(inner.intersects(painted));
    EXPECT_TRUE(inner.adjusted(-3, -3, 3, 3).contains(painted)) << "painted outside the item's rectangle";

    delete score;
}

// FD-156: the glow is for the screen only
TEST(FontDawSelectionTests, HoverGlowAllowed_OnScreenOnly)
{
    EXPECT_TRUE(fontdaw::hoverGlowAllowed(false, false));
    EXPECT_FALSE(fontdaw::hoverGlowAllowed(true, false)); // paintPdf, paintPrint, paintPng
    EXPECT_FALSE(fontdaw::hoverGlowAllowed(false, true)); // publish mode, read-only view
    EXPECT_FALSE(fontdaw::hoverGlowAllowed(true, true));
}

// FD-156: a bar glows on the staff that was hit only
TEST(FontDawSelectionTests, HoverGlowPageRect_BarCoversOneStaff)
{
    mu::engraving::MasterScore* score = mu::engraving::ScoreRW::readScore(u"data/test.mscx");
    ASSERT_TRUE(score);

    const mu::engraving::Measure* measure = score->firstMeasure();
    ASSERT_TRUE(measure && measure->system());
    const mu::engraving::System* system = measure->system();
    ASSERT_GE(system->staves().size(), size_t(1));

    const muse::RectF whole = fontdaw::hoverGlowPageRect(measure, muse::nidx);
    const muse::RectF first = fontdaw::hoverGlowPageRect(measure, 0);

    EXPECT_FALSE(first.isEmpty());
    EXPECT_DOUBLE_EQ(first.x(), whole.x());
    EXPECT_DOUBLE_EQ(first.width(), whole.width());
    EXPECT_DOUBLE_EQ(first.y(), system->staffYpage(0));
    EXPECT_DOUBLE_EQ(first.height(), system->staff(0)->bbox().height());
    // the staff's part lies inside the bar's box, and with several staves it is smaller than the whole bar
    EXPECT_LE(first.height(), whole.height() + 1e-6);
    if (system->staves().size() > 1) {
        EXPECT_LT(first.height(), whole.height());
        const muse::RectF second = fontdaw::hoverGlowPageRect(measure, 1);
        EXPECT_GE(second.y(), first.y() + first.height() - 1e-6);
    }

    // an unknown staff falls back to the measure's box
    EXPECT_EQ(fontdaw::hoverGlowPageRect(measure, 999), whole);

    delete score;
}
