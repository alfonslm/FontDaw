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

#include "waveformpeaks.h"

#include <algorithm>
#include <cmath>

#include "audio/engine/internal/codecs/audiodecoder.h"

using namespace mu::arrangement;

namespace {
int8_t toPeak(float v)
{
    const float clamped = std::clamp(v, -1.f, 1.f);
    return static_cast<int8_t>(std::lround(clamped * 127.f));
}

uint8_t toBand(double v)
{
    if (!std::isfinite(v)) {
        return 0;
    }
    return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

// RBJ "Audio EQ Cookbook" biquad, direct form II transposed.
struct Biquad {
    double b0 = 1., b1 = 0., b2 = 0., a1 = 0., a2 = 0.;
    double z1 = 0., z2 = 0.;

    static Biquad make(bool highpass, double freq, double sampleRate)
    {
        const double q = 1.0 / std::sqrt(2.0);
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double cw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        Biquad f;
        if (highpass) {
            f.b0 = (1.0 + cw) / 2.0 / a0;
            f.b1 = -(1.0 + cw) / a0;
            f.b2 = f.b0;
        } else {
            f.b0 = (1.0 - cw) / 2.0 / a0;
            f.b1 = (1.0 - cw) / a0;
            f.b2 = f.b0;
        }
        f.a1 = -2.0 * cw / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    double process(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Per-channel band splitter: 4th-order (two cascaded biquads) low, mid and high paths.
// A crossover at or above 0.45 * sampleRate is out of range (the biquad is unstable near Nyquist):
// its filters are bypassed.
struct BandFilter {
    Biquad low[2];
    Biquad midHp[2];
    Biquad midLp[2];
    Biquad high[2];
    bool lowMidInRange = true;
    bool midHighInRange = true;

    static BandFilter make(double sampleRate)
    {
        BandFilter f;
        f.lowMidInRange = WaveformPeaks::LOW_MID_HZ < 0.45 * sampleRate;
        f.midHighInRange = WaveformPeaks::MID_HIGH_HZ < 0.45 * sampleRate;
        for (int i = 0; i < 2; ++i) {
            if (f.lowMidInRange) {
                f.low[i] = Biquad::make(false, WaveformPeaks::LOW_MID_HZ, sampleRate);
                f.midHp[i] = Biquad::make(true, WaveformPeaks::LOW_MID_HZ, sampleRate);
            }
            if (f.midHighInRange) {
                f.midLp[i] = Biquad::make(false, WaveformPeaks::MID_HIGH_HZ, sampleRate);
                f.high[i] = Biquad::make(true, WaveformPeaks::MID_HIGH_HZ, sampleRate);
            }
        }
        return f;
    }

    void process(double x, double& lo, double& mid, double& hi)
    {
        if (!lowMidInRange) {
            lo = x;
            mid = 0.0;
            hi = 0.0;
            return;
        }
        lo = low[1].process(low[0].process(x));
        mid = midHp[1].process(midHp[0].process(x));
        if (midHighInRange) {
            hi = high[1].process(high[0].process(x));
            mid = midLp[1].process(midLp[0].process(mid));
        } else {
            hi = 0.0;
        }
    }
};
}

muse::RetVal<WaveformPeaks> WaveformPeaks::build(muse::audio::codec::IAudioDecoder& decoder)
{
    using namespace muse::audio;

    const codec::AudioStreamInfo info = decoder.info();

    WaveformPeaks result;
    result.sampleRate = info.sampleRate;

    const size_t channels = info.channels;
    Level level0;
    level0.framesPerPeak = FRAMES_PER_PEAK;
    level0.channels.resize(channels);
    level0.bands.resize(channels);

    std::vector<BandFilter> filters;
    if (info.sampleRate > 0) {
        filters.assign(channels, BandFilter::make(static_cast<double>(info.sampleRate)));
        result.bandCrossoversHz = { LOW_MID_HZ, MID_HIGH_HZ };
    }

    // readAll() hands over blocks of FRAMES_PER_PEAK frames (the last one may be shorter): one
    // peak each. The same read as the engine's decodeAll(), so the frame count is what plays.
    muse::RetVal<samples_t> read = codec::readAll(decoder, [&](const float* block, samples_t frames) {
        const size_t n = static_cast<size_t>(frames);
        for (size_t ch = 0; ch < channels; ++ch) {
            // Start from the first sample, not 0: a signal with a DC offset never crosses 0.
            float lo = block[ch];
            float hi = lo;
            for (size_t f = 0; f < n; ++f) {
                const float v = block[f * channels + ch];
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            level0.channels[ch].push_back({ toPeak(lo), toPeak(hi) });

            Bands bands;
            if (!filters.empty()) {
                double bl = 0.0, bm = 0.0, bh = 0.0;
                for (size_t f = 0; f < n; ++f) {
                    double l, m, h;
                    filters[ch].process(static_cast<double>(block[f * channels + ch]), l, m, h);
                    bl = std::max(bl, std::abs(l));
                    bm = std::max(bm, std::abs(m));
                    bh = std::max(bh, std::abs(h));
                }
                bands = { toBand(bl), toBand(bm), toBand(bh) };
            }
            level0.bands[ch].push_back(bands);
        }
        return true;
    }, codec::WavDecoder::MAX_SAMPLES, FRAMES_PER_PEAK);

    if (!read.ret) {
        return muse::RetVal<WaveformPeaks>::make_ret(read.ret);
    }

    result.frames = read.val;
    result.levels.push_back(std::move(level0));

    while (result.levels.back().channels.front().size() > 1) {
        const Level& prev = result.levels.back();
        Level next;
        next.framesPerPeak = prev.framesPerPeak * 2;
        next.channels.resize(channels);
        next.bands.resize(channels);
        for (size_t ch = 0; ch < channels; ++ch) {
            const std::vector<MinMax>& src = prev.channels[ch];
            std::vector<MinMax>& dst = next.channels[ch];
            dst.resize((src.size() + 1) / 2);
            const std::vector<Bands>& bsrc = prev.bands[ch];
            std::vector<Bands>& bdst = next.bands[ch];
            bdst.resize(dst.size());
            for (size_t i = 0; i < dst.size(); ++i) {
                MinMax m = src[2 * i];
                if (2 * i + 1 < src.size()) {
                    m.min = std::min(m.min, src[2 * i + 1].min);
                    m.max = std::max(m.max, src[2 * i + 1].max);
                }
                dst[i] = m;

                Bands b = bsrc[2 * i];
                if (2 * i + 1 < bsrc.size()) {
                    b.low = std::max(b.low, bsrc[2 * i + 1].low);
                    b.mid = std::max(b.mid, bsrc[2 * i + 1].mid);
                    b.high = std::max(b.high, bsrc[2 * i + 1].high);
                }
                bdst[i] = b;
            }
        }
        result.levels.push_back(std::move(next));
    }

    return muse::RetVal<WaveformPeaks>::make_ok(std::move(result));
}

const WaveformPeaks::Level* WaveformPeaks::levelFor(double framesPerPixel) const
{
    const Level* best = nullptr;
    for (const Level& level : levels) {
        if (level.framesPerPeak <= framesPerPixel || !best) {
            best = &level;
        } else {
            break;
        }
    }
    return best;
}
