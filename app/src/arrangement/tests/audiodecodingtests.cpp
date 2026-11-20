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

#include <gtest/gtest.h>

#include <QFile>

#include "internal/audioassetstore.h"
#include "internal/waveformpeaks.h"

#include "testutils.h"

using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace muse;

namespace {
const std::vector<double> RAMP = { 0.0, 0.5, -0.5, 0.25, -1.0, 0.75 }; // 3 stereo frames

//! Peaks through the engine's decoder, as the asset store builds them
WaveformPeaks peaksOf(const ByteArray& bytes)
{
    RetVal<AudioAssetStore::DecodedFile> decoded = AudioAssetStore::decodeFile(bytes, AudioAssetStore::Limits::Import);
    EXPECT_TRUE(decoded.ret) << decoded.ret.text();
    return decoded.ret && decoded.val.peaks ? *decoded.val.peaks : WaveformPeaks();
}

void expectDecodes(uint16_t bits, bool isFloat, bool extensible)
{
    const ByteArray wav = makeWav(44100, 2, bits, isFloat, RAMP, extensible);

    RetVal<AudioAssetStore::DecodedFile> decoded = AudioAssetStore::decodeFile(wav, AudioAssetStore::Limits::Import);
    ASSERT_TRUE(decoded.ret) << bits << " bit: " << decoded.ret.text();
    EXPECT_EQ(decoded.val.format, "wav");
    EXPECT_EQ(decoded.val.sampleRate, 44100u);
    EXPECT_EQ(decoded.val.channels, 2);
    EXPECT_EQ(decoded.val.frames, 3u);
    ASSERT_TRUE(decoded.val.peaks);
    EXPECT_EQ(decoded.val.peaks->frames, 3u);
}

//! A file of the framework's decoder tests (muse/framework/audio/tests/data/codecs, made by its
//! make_testdata.sh: 0.5 s, 48 kHz stereo; mono24.* is 0.25 s, 44.1 kHz mono)
ByteArray codecTestFile(const std::string& name)
{
    QFile file(QString::fromStdString(std::string(CODEC_TEST_DATA_DIR) + "/" + name));
    if (!file.open(QIODevice::ReadOnly)) {
        ADD_FAILURE() << "cannot read " << name;
        return ByteArray();
    }
    const QByteArray bytes = file.readAll();
    return ByteArray(reinterpret_cast<const uint8_t*>(bytes.constData()), static_cast<size_t>(bytes.size()));
}
}

TEST(Arrangement_AudioDecodingTests, DecodesEveryPcmAndFloatFormat)
{
    expectDecodes(8, false, false);
    expectDecodes(16, false, false);
    expectDecodes(24, false, false);
    expectDecodes(32, false, false);
    expectDecodes(32, true, false);
    expectDecodes(64, true, false);
}

TEST(Arrangement_AudioDecodingTests, DecodesWaveFormatExtensible)
{
    expectDecodes(24, false, true);
    expectDecodes(32, true, true);
}

TEST(Arrangement_AudioDecodingTests, RejectsWhatItCannotRead)
{
    // Not audio at all: an unsupported format
    EXPECT_EQ(AudioAssetStore::decodeFile(ByteArray("not audio at all"), AudioAssetStore::Limits::Import).ret.code(), static_cast<int>(Ret::Code::NotSupported));

    // Too many channels for v1
    EXPECT_EQ(AudioAssetStore::decodeFile(makeWav(48000, 3, 16, false, { 0, 0, 0 }), AudioAssetStore::Limits::Import).ret.code(),
              static_cast<int>(Ret::Code::NotSupported));

    // Sample rate outside 8-192 kHz
    EXPECT_EQ(AudioAssetStore::decodeFile(makeWav(4000, 1, 16, false, { 0 }), AudioAssetStore::Limits::Import).ret.code(),
              static_cast<int>(Ret::Code::NotSupported));

    // No samples
    EXPECT_EQ(AudioAssetStore::decodeFile(makeWav(48000, 1, 16, false, {}), AudioAssetStore::Limits::Import).ret.code(), static_cast<int>(Ret::Code::BadData));

    // The two import limits are FontDaw's, not the decoder's: audio already in a project is judged
    // by the decoder alone, as the engine judges it
    using Limits = AudioAssetStore::Limits;
    RetVal<AudioAssetStore::DecodedFile> threeChannels = AudioAssetStore::decodeFile(makeWav(48000, 3, 16, false, { 0, 0, 0 }), Limits::Decoder);
    ASSERT_TRUE(threeChannels.ret) << threeChannels.ret.text();
    EXPECT_EQ(threeChannels.val.channels, 3);
    RetVal<AudioAssetStore::DecodedFile> lowRate = AudioAssetStore::decodeFile(makeWav(4000, 1, 16, false, { 0 }), Limits::Decoder);
    ASSERT_TRUE(lowRate.ret) << lowRate.ret.text();
    EXPECT_EQ(lowRate.val.sampleRate, 4000u);

    // A WAV the decoder does not read (here: a 12-bit "PCM" stand-in)
    EXPECT_EQ(AudioAssetStore::decodeFile(makeWav(48000, 1, 12, false, {}), AudioAssetStore::Limits::Import).ret.code(), static_cast<int>(Ret::Code::BadData));
}

TEST(Arrangement_AudioDecodingTests, TruncatedWavKeepsTheFramesPresent)
{
    // A recorder that crashed: the data chunk claims more than the file holds.
    ByteArray wav = makeWav(48000, 1, 16, false, { 0.1, 0.2, 0.3, 0.4 });
    std::vector<uint8_t> bytes(wav.constData(), wav.constData() + wav.size());
    bytes.pop_back(); // half a sample missing

    RetVal<AudioAssetStore::DecodedFile> decoded = AudioAssetStore::decodeFile(ByteArray(bytes.data(), bytes.size()), AudioAssetStore::Limits::Import);
    ASSERT_TRUE(decoded.ret);
    EXPECT_EQ(decoded.val.frames, 3u);
}

TEST(Arrangement_AudioDecodingTests, EveryFormatGivesRateChannelsAndFrames)
{
    struct Expected {
        const char* file;
        const char* format;
        uint64_t sampleRate;
        int channels;
        uint64_t frames; // 0: not exact for this codec, checked to be within 1 % of 24000
    };
    for (const Expected& e : { Expected { "reference.wav", "wav", 48000, 2, 24000 },
                               Expected { "reference.flac", "flac", 48000, 2, 24000 },
                               Expected { "mono24.flac", "flac", 44100, 1, 11025 },
                               Expected { "reference.mp3", "mp3", 48000, 2, 0 },
                               Expected { "reference.ogg", "ogg", 48000, 2, 0 },
                               Expected { "reference.opus", "ogg", 48000, 2, 0 } }) {
        RetVal<AudioAssetStore::DecodedFile> decoded = AudioAssetStore::decodeFile(codecTestFile(e.file), AudioAssetStore::Limits::Import);
        ASSERT_TRUE(decoded.ret) << e.file << ": " << decoded.ret.text();
        EXPECT_EQ(decoded.val.format, e.format) << e.file;
        EXPECT_EQ(decoded.val.sampleRate, e.sampleRate) << e.file;
        EXPECT_EQ(decoded.val.channels, e.channels) << e.file;
        if (e.frames) {
            EXPECT_EQ(decoded.val.frames, e.frames) << e.file;
        } else {
            EXPECT_NEAR(static_cast<double>(decoded.val.frames), 24000.0, 240.0) << e.file;
        }
        ASSERT_TRUE(decoded.val.peaks) << e.file;
        EXPECT_EQ(decoded.val.peaks->frames, decoded.val.frames) << e.file;
        // Band peaks for the coloured waveform are built for every format
        EXPECT_EQ(decoded.val.peaks->bandCrossoversHz, (std::vector<double> { 600.0, 4000.0 })) << e.file;
        ASSERT_FALSE(decoded.val.peaks->levels.empty());
        EXPECT_EQ(decoded.val.peaks->levels.front().bands.size(), static_cast<size_t>(e.channels));
    }
}

TEST(Arrangement_AudioDecodingTests, DamagedCompressedFilesAreRefusedNotCrashed)
{
    for (const char* name : { "reference.flac", "reference.mp3", "reference.ogg", "reference.opus" }) {
        const ByteArray whole = codecTestFile(name);
        for (size_t size : { size_t(0), size_t(3), size_t(4), size_t(16), size_t(40), size_t(100), whole.size() / 2 }) {
            const ByteArray cut(whole.constData(), std::min(size, whole.size()));
            RetVal<AudioAssetStore::DecodedFile> decoded = AudioAssetStore::decodeFile(cut, AudioAssetStore::Limits::Import);
            if (decoded.ret) {
                EXPECT_LE(decoded.val.frames, 24000u + 1152u) << name << " cut at " << size;
            }
        }
    }
}

TEST(Arrangement_AudioDecodingTests, PeaksCoverTheFileAtEveryLevel)
{
    const ByteArray wav = makeSineWav(1.0, 48000, 2); // left = +sine, right = -sine, amplitude 0.5
    const WaveformPeaks peaks = peaksOf(wav);
    EXPECT_EQ(peaks.frames, 48000u);
    EXPECT_EQ(peaks.sampleRate, 48000u);
    ASSERT_FALSE(peaks.levels.empty());

    const WaveformPeaks::Level& level0 = peaks.levels.front();
    EXPECT_EQ(level0.framesPerPeak, WaveformPeaks::FRAMES_PER_PEAK);
    ASSERT_EQ(level0.channels.size(), 2u);
    EXPECT_EQ(level0.channels[0].size(), (48000u + 255u) / 256u);

    // Each level halves the previous one, down to a single peak holding the whole file.
    for (size_t i = 1; i < peaks.levels.size(); ++i) {
        EXPECT_EQ(peaks.levels[i].framesPerPeak, peaks.levels[i - 1].framesPerPeak * 2);
        EXPECT_EQ(peaks.levels[i].channels[0].size(), (peaks.levels[i - 1].channels[0].size() + 1) / 2);
    }
    const WaveformPeaks::MinMax whole = peaks.levels.back().channels[0].front();
    EXPECT_EQ(peaks.levels.back().channels[0].size(), 1u);
    EXPECT_NEAR(whole.max, 64, 1); // 0.5 * 127
    EXPECT_NEAR(whole.min, -64, 1);

    // Zoom: the coarsest level that still has at least one peak per pixel
    EXPECT_EQ(peaks.levelFor(100.0), &peaks.levels[0]);
    EXPECT_EQ(peaks.levelFor(256.0), &peaks.levels[0]);
    EXPECT_EQ(peaks.levelFor(600.0), &peaks.levels[1]);
    EXPECT_EQ(peaks.levelFor(1e12), &peaks.levels.back());
}

TEST(Arrangement_AudioDecodingTests, PeaksFollowADcOffset)
{
    // A signal that never crosses 0: the peaks must not be stretched to include 0.
    const ByteArray wav = makeWav(48000, 1, 16, false, std::vector<double>(1024, 0.5));
    const WaveformPeaks peaks = peaksOf(wav);
    for (const WaveformPeaks::MinMax& p : peaks.levels.front().channels[0]) {
        EXPECT_NEAR(p.min, 64, 1);
        EXPECT_NEAR(p.max, 64, 1);
    }
}

namespace {
void expectDominantBand(uint32_t sampleRate, double freq, int dominant)
{
    const ByteArray wav = makeSineWav(1.0, sampleRate, 1, freq, 0.8);
    const WaveformPeaks peaks = peaksOf(wav);
    const std::vector<WaveformPeaks::Bands>& bands = peaks.levels.front().bands[0];
    ASSERT_GT(bands.size(), 20u);
    for (size_t i = 20; i < bands.size(); ++i) {
        const int v[3] = { bands[i].low, bands[i].mid, bands[i].high };
        for (int k = 0; k < 3; ++k) {
            if (k != dominant) {
                EXPECT_GT(v[dominant], 3 * v[k]) << freq << " Hz at " << sampleRate << ", peak " << i << ", band " << k;
            }
        }
    }
}
}

TEST(Arrangement_AudioDecodingTests, PeakBandsFollowTheFrequency)
{
    expectDominantBand(48000, 100.0, 0);
    expectDominantBand(48000, 2000.0, 1);
    expectDominantBand(48000, 8000.0, 2);
    expectDominantBand(44100, 100.0, 0);
    expectDominantBand(44100, 2000.0, 1);
    expectDominantBand(44100, 8000.0, 2);
}

TEST(Arrangement_AudioDecodingTests, PeakBandsAreSafeAtLowSampleRates)
{
    // 8 kHz: the 4 kHz crossover is out of range, so the high band is always 0
    {
        const ByteArray wav = makeSineWav(1.0, 8000, 1, 100.0, 0.8);
        const WaveformPeaks peaks = peaksOf(wav);
        ASSERT_FALSE(peaks.levels.empty());
        const std::vector<WaveformPeaks::Bands>& bands = peaks.levels.front().bands[0];
        ASSERT_GT(bands.size(), 20u);
        for (size_t i = 0; i < bands.size(); ++i) {
            EXPECT_EQ(bands[i].high, 0) << i;
            if (i >= 20) {
                EXPECT_GT(bands[i].low, 3 * bands[i].mid) << i;
            }
        }
        EXPECT_EQ(peaks.bandCrossoversHz, (std::vector<double> { 600.0, 4000.0 }));
    }

    // 8 kHz (the lowest rate FontDaw imports), 2 kHz sine: mid only
    {
        const ByteArray wav = makeSineWav(1.0, 8000, 1, 2000.0, 0.8);
        const WaveformPeaks peaks = peaksOf(wav);
        const std::vector<WaveformPeaks::Bands>& bands = peaks.levels.front().bands[0];
        ASSERT_GT(bands.size(), 20u);
        for (size_t i = 0; i < bands.size(); ++i) {
            EXPECT_EQ(bands[i].high, 0) << i;
            if (i >= 20) {
                EXPECT_GT(bands[i].mid, 0) << i;
                EXPECT_LT(bands[i].low, bands[i].mid) << i;
            }
        }
    }

    // 8 kHz, pseudo-random noise: builds, with the expected peak count
    {
        std::vector<double> samples(8000);
        uint32_t state = 12345;
        for (double& s : samples) {
            state = state * 1664525u + 1013904223u;
            s = (static_cast<double>(state >> 8) / 8388608.0 - 1.0) * 0.9;
        }
        const ByteArray wav = makeWav(8000, 1, 16, false, samples);
        const WaveformPeaks peaks = peaksOf(wav);
        ASSERT_FALSE(peaks.levels.empty());
        const WaveformPeaks::Level& l0 = peaks.levels.front();
        EXPECT_EQ(l0.bands[0].size(), (samples.size() + WaveformPeaks::FRAMES_PER_PEAK - 1) / WaveformPeaks::FRAMES_PER_PEAK);
        EXPECT_EQ(l0.bands[0].size(), l0.channels[0].size());
    }
}

TEST(Arrangement_AudioDecodingTests, PeakBandsHaveTheShapeOfThePeaks)
{
    const ByteArray wav = makeSineWav(1.0, 48000, 2, 440.0, 0.5);
    const WaveformPeaks peaks = peaksOf(wav);

    ASSERT_FALSE(peaks.levels.empty());
    for (const WaveformPeaks::Level& level : peaks.levels) {
        ASSERT_EQ(level.bands.size(), level.channels.size());
        for (size_t ch = 0; ch < level.channels.size(); ++ch) {
            EXPECT_EQ(level.bands[ch].size(), level.channels[ch].size());
        }
    }
    EXPECT_EQ(peaks.levels.back().bands[0].size(), 1u);
    EXPECT_EQ(peaks.bandCrossoversHz, (std::vector<double> { 600.0, 4000.0 }));
}

TEST(Arrangement_AudioDecodingTests, SilenceHasNoBands)
{
    const ByteArray wav = makeWav(48000, 1, 16, false, std::vector<double>(4096, 0.0));
    const WaveformPeaks peaks = peaksOf(wav);
    for (const WaveformPeaks::Level& level : peaks.levels) {
        for (const WaveformPeaks::Bands& b : level.bands[0]) {
            EXPECT_EQ(b, WaveformPeaks::Bands());
        }
    }
}
