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

#include "audiostaffpainter.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include "draw/types/font.h"
#include "draw/types/pen.h"

#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/score.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tempotimeline.h"

#include "audiopart.h"
#include "clipstates.h"
#include "scorepositions.h"
#include "waveformpeaks.h"

#include "log.h"

using namespace mu::arrangement;
using namespace mu::engraving;
using namespace muse::draw;

namespace {
//! 1 pt in score canvas units (the canvas is DPI units per inch)
constexpr double POINT = mu::engraving::DPI / 72.0;

//! Clip-name sizes, on screen and on paper (owner, 17 November 2026: S1's sizes are the
//! defaults; kept here so that a later setting can change them)
constexpr double CLIP_NAME_PT = 7.0;
constexpr double MISSING_CLIP_NAME_PT = 9.0;
constexpr double TIME_LEFT_PT = 11.0;

struct Colors {
    muse::Color text;      // the notation's colour: zero line, symbol, outlines, names
    muse::Color frame;     // clip frames (owner: grey with a faint fill, light and dark)
    muse::Color fill;      // the frame's fill; transparent: none
    muse::Color mutedWave;
    bool monochrome = false; // the Black and white profile: the waveform in mid grey
};

//! Screen colours follow the theme (FD-072); print is black on white whatever the theme, and a
//! greyscale printer gets the Black and white profile (owner, 17 November 2026, S4 point 2)
Colors colorsFor(bool invert, bool printing, bool monochrome)
{
    if (printing) {
        invert = false;
    }
    Colors c;
    c.text = invert ? muse::Color(232, 232, 232) : muse::Color::BLACK;
    c.frame = invert ? muse::Color(138, 138, 138) : muse::Color(154, 154, 154);
    c.fill = c.text;
    c.fill.setAlpha(invert ? 15 : 13);
    c.mutedWave = muse::Color(150, 150, 150);
    if (printing && monochrome) {
        c.frame = muse::Color(180, 180, 180);
        c.fill = muse::Color(255, 255, 255, 0);
        c.monochrome = true;
    }
    return c;
}

//! The RGB profile (design 06): low red, mid green, high blue, mixed by band energy and
//! normalised by the largest channel, as Mixxx's RGB renderer does
muse::Color bandColor(const WaveformPeaks::Bands& b)
{
    const int maxBand = std::max({ int(b.low), int(b.mid), int(b.high) });
    if (maxBand == 0) {
        return muse::Color(128, 128, 128);
    }
    return muse::Color(b.low * 255 / maxBand, b.mid * 255 / maxBand, b.high * 255 / maxBand);
}

QString formatSecs(double secs)
{
    const int s = std::max(0, int(std::lround(secs)));
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

//! Where one system shows the audio staff
struct StaffBand {
    const System* system = nullptr;
    double top = 0.0;
    double bottom = 0.0;
    double mid = 0.0;
    double left = 0.0;   // first bar's start
    double right = 0.0;  // last bar's end
    double spatium = 0.0;
};

//! Where a moment of the played audio is drawn: its system (by index, in score order) and x
struct Pos {
    size_t system = 0;
    double x = 0.0;
    bool operator<(const Pos& o) const { return system != o.system ? system < o.system : x < o.x; }
};

using SystemIndex = std::map<const System*, size_t>;

std::optional<Pos> secsToPos(const Score* score, const SystemIndex& systems, const SecsToWrittenTick& toTick, double secs)
{
    const std::optional<Fraction> tick = toTick(secs);
    if (!tick) {
        return std::nullopt;
    }
    const std::optional<ScoreX> x = tickToCanvasX(score, *tick);
    if (!x) {
        return std::nullopt;
    }
    const auto it = systems.find(x->system);
    return it != systems.end() ? std::optional<Pos>(Pos { it->second, x->x }) : std::nullopt;
}

//! One clip's piece in one system (S4: clips are split at system breaks, clipPieces)
void paintPiece(Painter* painter, const Score* score, const SystemIndex& systems, const SecsToWrittenTick& toTick,
                const StaffBand& band, const ClipView& clip, const ClipPiece& piece, const WaveformPeaks* peaks, const Colors& colors,
                double scoreEndSecs, double px, const RectF& visible, bool endMarkUnclipped)
{
    const double sp = band.spatium;
    const double clipEnd = clip.startSecs + clip.lengthSecs;
    double x0 = piece.x0;
    double x1 = piece.x1;
    if (x1 <= x0) {
        return;
    }

    // Nothing to do outside the visible part of the canvas (review of #167, finding 7)
    const bool cull = visible.isValid();
    if (cull && (x1 < visible.left() || x0 > visible.right())) {
        return;
    }

    const RectF frame(x0, band.top - 0.4 * sp, x1 - x0, (band.bottom - band.top) + 0.8 * sp);

    // Frame: grey with a faint fill (no fill in black and white); dashed when the audio is missing
    Pen framePen(colors.frame, std::max(px, 0.08 * sp));
    if (clip.missing) {
        framePen.setDashPattern({ 4.0, 3.0 });
    }
    painter->setPen(framePen);
    // No fill in black and white: clear the brush, which the score's own drawing may have left set
    painter->setBrush(colors.fill.alpha() > 0 ? Brush(colors.fill) : Brush(BrushStyle::NoBrush));
    painter->drawRoundedRect(frame, 0.3 * sp, 0.3 * sp);
    painter->setBrush(BrushStyle::NoBrush);

    Font font(Font::FontFamily(u"Edwin"), Font::Type::Text);
    font.setPointSizeF(clip.missing ? MISSING_CLIP_NAME_PT : CLIP_NAME_PT);
    painter->setFont(font);
    painter->setPen(colors.text);

    if (clip.missing) {
        // The clip's name (or its file's) in each piece of the empty frame, on screen and in print:
        // the name is all that frame shows (07's page-view picture)
        painter->drawText(frame, AlignHCenter | AlignVCenter, TextDontClip, clip.label);
    } else if (peaks && !peaks->levels.empty() && peaks->sampleRate > 0) {
        // The waveform, column by column, warped between note positions: each column's seconds
        // come from sampling the clip's time and mapping it through the score (forward only, S6
        // shows the passes of a repeat)
        const double halfHeight = (band.bottom - band.top) / 2.0 * 0.9;
        const size_t sys = systems.at(band.system);
        const double lastSecs = std::min(clipEnd, scoreEndSecs);

        // The seconds at a point of this system: bisection over the clip's time, ordered by
        // (system, x), which grows with time within a pass
        auto secsAt = [&](double targetX) {
            double lo = clip.startSecs;
            double hi = lastSecs;
            const Pos target { sys, targetX };
            for (int i = 0; i < 48; ++i) {
                const double mid = (lo + hi) / 2.0;
                const std::optional<Pos> p = secsToPos(score, systems, toTick, mid);
                if (p && *p < target) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            return lo;
        };
        const double fromX = cull ? std::max(x0, visible.left() - 2.0 * px) : x0;
        const double toX = cull ? std::min(x1, visible.right() + 2.0 * px) : x1;
        const double fromSecs = (fromX > x0 || piece.continuesLeft) ? secsAt(fromX) : clip.startSecs;
        const double toSecs = (toX < x1 || piece.continuesRight) ? std::min(secsAt(toX), lastSecs) : lastSecs;

        const double width = toX - fromX;
        const int samples = std::clamp(int(width / px * 2.0), 2, 40000);
        const double dt = (toSecs - fromSecs) / samples;

        std::vector<PointF> top;
        std::vector<PointF> bottom;
        top.reserve(size_t(width / px) + 2);
        bottom.reserve(top.capacity());

        // A 1 pt outline in the text colour, so pale parts show on white (owner, review of #160),
        // drawn per stretch without gaps
        auto flushOutline = [&]() {
            if (top.size() > 1) {
                std::vector<PointF> outline(top);
                outline.insert(outline.end(), bottom.rbegin(), bottom.rend());
                outline.push_back(top.front());
                painter->setPen(Pen(colors.text, POINT));
                painter->drawPolyline(outline.data(), outline.size());
            }
            top.clear();
            bottom.clear();
        };

        // FD-167: x within this system from a table made once per piece, not tickToCanvasX per column
        const SystemTickToX systemX(band.system);

        double prevX = fromX;
        double prevSecs = fromSecs;
        for (int i = 1; i <= samples; ++i) {
            const double secs = fromSecs + dt * i;
            std::optional<double> x;
            if (i == samples) {
                x = toX;
            } else if (const std::optional<Fraction> tick = toTick(std::min(secs, lastSecs - 1e-9))) {
                x = systemX(*tick); // nullopt outside this system, as before
            }
            if (!x || *x < prevX + px) {
                if (x && *x < prevX) {
                    prevX = *x; // a jump back (repeat): start again there
                    prevSecs = secs;
                }
                continue;
            }

            // Frames of the source under this column: one value per column, a column being about
            // one device pixel on screen and one output dot in print (owner: one value per dot)
            const double srcFrom = clip.sourceOffsetSecs + (prevSecs - clip.startSecs);
            const double srcTo = clip.sourceOffsetSecs + (secs - clip.startSecs);
            const double framesPerColumn = std::max(1.0, (srcTo - srcFrom) * peaks->sampleRate);
            const WaveformPeaks::Level* level = peaks->levelFor(framesPerColumn);
            if (level && !level->channels.empty()) {
                const size_t first = size_t(std::max(0.0, srcFrom * peaks->sampleRate) / level->framesPerPeak);
                const size_t last = size_t(std::max(0.0, srcTo * peaks->sampleRate) / level->framesPerPeak);
                int lo = 0;
                int hi = 0;
                WaveformPeaks::Bands bands;
                for (size_t ch = 0; ch < level->channels.size(); ++ch) {
                    const auto& mm = level->channels[ch];
                    for (size_t p = first; p <= last && p < mm.size(); ++p) {
                        lo = std::min(lo, int(mm[p].min));
                        hi = std::max(hi, int(mm[p].max));
                        if (ch < level->bands.size() && p < level->bands[ch].size()) {
                            const WaveformPeaks::Bands& b = level->bands[ch][p];
                            bands.low = std::max(bands.low, b.low);
                            bands.mid = std::max(bands.mid, b.mid);
                            bands.high = std::max(bands.high, b.high);
                        }
                    }
                }
                if (*x - prevX > 3.0 * px) {
                    flushOutline(); // a gap: the outline does not bridge it
                }
                const double yTop = band.mid - hi / 127.0 * halfHeight;
                const double yBottom = band.mid - lo / 127.0 * halfHeight;
                // A column is at most about a pixel and a half wide. Where x jumps with almost no
                // time passing (the space after a bar line) the zero line shows, not a wide block.
                const double columnWidth = std::min(*x - prevX, 1.5 * px);
                const double cx = *x - columnWidth / 2.0;
                const muse::Color fill = clip.muted ? colors.mutedWave
                                         : colors.monochrome ? muse::Color(128, 128, 128) : bandColor(bands);
                painter->setPen(Pen(fill, columnWidth * 1.05));
                painter->drawLine(PointF(cx, yTop), PointF(cx, std::max(yBottom, yTop + px)));
                top.emplace_back(cx, yTop);
                bottom.emplace_back(cx, yBottom);
            }
            prevX = *x;
            prevSecs = secs;
        }

        flushOutline();
        painter->setPen(colors.text);
    }

    // A loaded clip's name on its first piece only (owner, 17 November 2026, S4 point 1)
    if (!clip.missing && !piece.continuesLeft) {
        painter->drawText(PointF(x0 + 0.3 * sp, band.top - 0.6 * sp), clip.label);
    }

    // Past the final bar line: cut there, with an arrow scaled to the notes and the time left
    // (the last measure's system: a frame after the last bar is a system of its own). Its place in
    // print is open (FD-124); in continuous view on screen it is drawn unclipped, as in S1.
    const Measure* lastMeasure = score->lastMeasure();
    if (clipEnd > scoreEndSecs && piece.continuesRight && lastMeasure && piece.system == lastMeasure->system()) {
        painter->save();
        if (endMarkUnclipped) {
            painter->setClipping(false);
        }
        const double ax = x1 + 1.0 * sp;
        const double len = 3.0 * sp;
        painter->setPen(Pen(colors.text, 0.18 * sp));
        painter->drawLine(PointF(ax, band.mid), PointF(ax + len, band.mid));
        const PointF head[] = { PointF(ax + len + 0.2 * sp, band.mid), PointF(ax + len - 1.0 * sp, band.mid - 0.7 * sp),
                                PointF(ax + len - 1.0 * sp, band.mid + 0.7 * sp) };
        painter->setBrush(Brush(colors.text));
        painter->drawPolygon(head, 3);
        painter->setBrush(BrushStyle::NoBrush);
        Font timeFont(Font::FontFamily(u"Edwin"), Font::Type::Text);
        timeFont.setPointSizeF(TIME_LEFT_PT);
        painter->setFont(timeFont);
        painter->drawText(PointF(ax + len + 0.8 * sp, band.mid + 0.8 * sp), muse::String::fromQString(formatSecs(clipEnd - scoreEndSecs)));
        painter->restore();
    }
}
}

void AudioStaffPainter::init()
{
    // GUI thread. The paint call itself only reads (render thread, GUI thread blocked).
    if (arrangement()) {
        arrangement()->changed().onReceive(this, [this](const Change&) {
            m_gaveUpOnPeaks.clear(); // a changed arrangement is waited for again
            m_gaveUpOnLoading = false;
            m_changed.notify();
        });
    }
    if (assetStore()) {
        assetStore()->peaksReady().onReceive(this, [this](const AssetId&) {
            m_changed.notify();
        });
    }
}

void AudioStaffPainter::prepareForOutput(int timeoutMs) const
{
    // Once per output, by its writer, on the GUI thread, before the output's painter exists: the
    // peaks are built by the asset store's workers and handed over through the event loop. Off the
    // GUI thread (the video export's frames) there is nothing to wait for and no loop to run.
    if (!arrangement() || !assetStore() || QThread::currentThread() != QCoreApplication::instance()->thread()) {
        return;
    }

    // Clips whose peaks did not come within an earlier wait (a file that opens but cannot be read
    // never gets peaks, and has no failure state) are not waited for again until the arrangement
    // changes; the same for tracks that did not finish loading
    auto pendingAssets = [this]() {
        std::set<AssetId> ids;
        for (const Track& track : arrangement()->tracks()) {
            for (const ClipView& clip : clipViews(track, assetStore().get())) {
                if (!clip.missing && !assetStore()->peaks(clip.asset) && !m_gaveUpOnPeaks.count(clip.asset)) {
                    ids.insert(clip.asset);
                }
            }
        }
        return ids;
    };

    // Also until the audio tracks are loaded: the waiting lets their loading start, and a
    // command-line export that ended in the middle of it would close the project while the engine
    // still decodes its files (FD-123; as the audio writer, wait for isLoaded)
    auto loading = [this]() {
        return !m_gaveUpOnLoading && controller() && !controller()->isLoaded();
    };

    QElapsedTimer timer;
    timer.start();
    while (!pendingAssets().empty() || loading()) {
        if (timer.elapsed() > timeoutMs) {
            const std::set<AssetId> late = pendingAssets();
            LOGW() << "audio not ready after " << timeoutMs << " ms; " << late.size()
                   << " clip(s) print without their waveform" << (loading() ? "; tracks still loading" : "");
            m_gaveUpOnPeaks.insert(late.begin(), late.end());
            m_gaveUpOnLoading = loading();
            return;
        }
        // No user input meanwhile: the user must not edit or close the project under an output
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
        QThread::msleep(5);
    }
}

muse::async::Notification AudioStaffPainter::changed() const
{
    return m_changed;
}

void AudioStaffPainter::paint(Painter* painter, const Score* score, const Page* page, const Options& opt) const
{
    // Every view, on screen and in print (S4; owner's answers 1 and 2, design 05)
    if (!painter || !score || !page || !arrangement()) {
        return;
    }

    // One device pixel, or one output dot in print, in canvas units. In print the page is mapped
    // to the device by the painter's window and viewport, not its world transform.
    double px = 1.0;
    if (opt.isPrinting && opt.deviceDpi > 0) {
        px = mu::engraving::DPI / double(opt.deviceDpi);
    } else if (const double pxScale = painter->worldTransform().m11(); pxScale > 0.0) {
        px = 1.0 / pxScale;
    }
    const Colors colors = colorsFor(opt.invertColors, opt.isPrinting, opt.monochrome);
    const bool expandRepeats = notationConfiguration() ? notationConfiguration()->isPlayRepeatsEnabled() : true;

    // expandedRepeatList() and tempoTimeline() rebuild lazily: from here (the render thread) that
    // is safe only because the GUI thread is blocked during paint(). "Play repeats" is read from the
    // configuration; the arrangement's time map follows the score's own setting (FD-096).

    // Seconds at which the written score ends, for clips that run past it
    const TempoTimeline& timeline = score->tempoTimeline(expandRepeats);
    const int endPlayedTick = expandRepeats ? score->expandedRepeatList().ticks()
                              : (score->lastMeasure() ? score->lastMeasure()->endTick().ticks() : 0);
    const double scoreEndSecs = timeline.utick2utime(endPlayedTick);

    const bool endMarkUnclipped = !opt.isPrinting && score->layoutMode() == LayoutMode::LINE;

    SystemIndex systems;
    for (size_t i = 0; i < score->systems().size(); ++i) {
        systems[score->systems()[i]] = i;
    }
    // FD-167: the tempo timeline and the repeat list once per paint, not once per column
    const SecsToWrittenTick toTick(score, expandRepeats);

    painter->save();
    // Called in the page's coordinates; the code below works in canvas coordinates
    painter->translate(-page->pos());
    painter->setAntialiasing(true);
    const RectF visible = opt.frameRect.isValid() ? opt.frameRect.translated(page->pos()) : RectF();

    const TrackList& tracks = arrangement()->tracks();
    const std::vector<const Part*> parts = resolveAudioParts(score, tracks);
    for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        const Track& track = tracks[trackIndex];
        const Part* part = parts[trackIndex];
        if (!part || part->nstaves() == 0) {
            continue;
        }
        const Staff* staff = part->staff(0);
        const staff_idx_t staffIdx = staff->idx();

        // Each clip's pieces, one per system it shows in (split at system breaks)
        struct ClipToDraw {
            ClipView view;
            std::shared_ptr<const WaveformPeaks> peaks;
            std::vector<ClipPiece> pieces;
        };
        std::vector<ClipToDraw> clips;
        for (const ClipView& clip : clipViews(track, assetStore().get())) {
            if (clip.lengthSecs <= 0.0 || clip.startSecs >= scoreEndSecs) {
                continue; // only what falls inside the score is drawn (07, section 2, item 4)
            }
            ClipToDraw c { clip, clip.missing ? nullptr : assetStore()->peaks(clip.asset),
                           clipPieces(score, clip.startSecs, clip.startSecs + clip.lengthSecs, expandRepeats, page) };
            if (!c.pieces.empty()) {
                clips.push_back(std::move(c));
            }
        }

        // Only this page's systems (continuous view has one page)
        for (const System* system : page->systems()) {
            const SysStaff* ss = system->staff(staffIdx);
            if (!ss || !ss->show() || !system->firstMeasure()) {
                continue;
            }

            StaffBand band;
            band.system = system;
            band.spatium = staff->spatium(Fraction(0, 1));
            band.top = system->staffCanvasYpage(staffIdx);
            band.bottom = band.top + staff->staffHeight(Fraction(0, 1));
            band.mid = (band.top + band.bottom) / 2.0;
            band.left = system->firstMeasure()->canvasPos().x();
            const Measure* last = system->lastMeasure();
            band.right = last ? last->canvasPos().x() + last->width() : band.left;
            const double sp = band.spatium;

            // The zero line, the width of the system: silence looks like a muted audio file
            painter->setPen(Pen(colors.text, std::max(px, 0.11 * sp)));
            painter->drawLine(PointF(band.left, band.mid), PointF(band.right, band.mid));

            // A short waveform outline where a clef would be (owner, 17 November 2026)
            {
                std::vector<PointF> wave;
                const double x0 = band.left + 0.8 * sp;
                const double width = 2.4 * sp;
                const double amp = 1.6 * sp;
                const int n = 48;
                for (int i = 0; i <= n; ++i) {
                    const double t = double(i) / n;
                    const double env = std::exp(-std::pow((t - 0.5) / 0.22, 2.0));
                    wave.emplace_back(x0 + t * width, band.mid - amp * env * std::sin(t * 2.0 * M_PI * 4.5));
                }
                painter->setPen(Pen(colors.text, std::max(px, 0.16 * sp)));
                painter->drawPolyline(wave.data(), wave.size());
            }

            for (const ClipToDraw& c : clips) {
                for (const ClipPiece& piece : c.pieces) {
                    if (piece.system == system) {
                        paintPiece(painter, score, systems, toTick, band, c.view, piece, c.peaks.get(), colors,
                                   scoreEndSecs, px, visible, endMarkUnclipped);
                    }
                }
            }
        }
    }

    painter->restore();
}
