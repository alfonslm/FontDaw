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

#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <QFile>

#include "async/processevents.h"
#include "global/types/bytearray.h"
#include "global/io/path.h"

#include "arrangementtypes.h"

namespace mu::arrangement::tests {
//! Builds a RIFF/WAVE file. `samples` are interleaved, in [-1, 1].
inline muse::ByteArray makeWav(uint32_t sampleRate, uint16_t channels, uint16_t bits, bool isFloat,
                               const std::vector<double>& samples, bool extensible = false)
{
    std::vector<uint8_t> out;
    auto u16 = [&out](uint16_t v) { out.push_back(v & 0xff); out.push_back(v >> 8); };
    auto u32 = [&out](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back((v >> (8 * i)) & 0xff);
        }
    };
    auto tag = [&out](const char* t) { out.insert(out.end(), t, t + 4); };

    const uint16_t bytesPerSample = bits / 8;
    const uint16_t blockAlign = channels * bytesPerSample;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() / channels) * blockAlign;
    const uint16_t formatTag = isFloat ? 3 : 1;

    tag("RIFF");
    u32(0); // patched below
    tag("WAVE");

    // An unrelated chunk first: readers must skip what they do not know.
    tag("LIST");
    u32(3);
    out.push_back('a');
    out.push_back('b');
    out.push_back('c');
    out.push_back(0); // pad byte: odd chunk sizes are padded

    tag("fmt ");
    u32(extensible ? 40 : 16);
    u16(extensible ? 0xFFFE : formatTag);
    u16(channels);
    u32(sampleRate);
    u32(sampleRate * blockAlign);
    u16(blockAlign);
    u16(bits);
    if (extensible) {
        u16(22);         // cbSize
        u16(bits);       // valid bits
        u32(channels == 1 ? 0x4 : 0x3); // channel mask
        u16(formatTag);  // sub-format GUID: format tag + fixed tail
        const uint8_t tail[14] = { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
        out.insert(out.end(), tail, tail + 14);
    }

    tag("data");
    u32(dataSize);
    for (double s : samples) {
        if (isFloat && bits == 32) {
            float f = static_cast<float>(s);
            uint32_t u;
            std::memcpy(&u, &f, 4);
            u32(u);
        } else if (isFloat && bits == 64) {
            uint64_t u;
            std::memcpy(&u, &s, 8);
            u32(static_cast<uint32_t>(u));
            u32(static_cast<uint32_t>(u >> 32));
        } else if (bits == 8) {
            out.push_back(static_cast<uint8_t>(std::lround(s * 127.0) + 128));
        } else if (bits == 16) {
            u16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(s * 32767.0))));
        } else if (bits == 24) {
            const int32_t v = static_cast<int32_t>(std::lround(s * 8388607.0));
            out.push_back(v & 0xff);
            out.push_back((v >> 8) & 0xff);
            out.push_back((v >> 16) & 0xff);
        } else if (bits == 32) {
            u32(static_cast<uint32_t>(static_cast<int32_t>(std::lround(s * 2147483647.0))));
        }
    }

    const uint32_t riffSize = static_cast<uint32_t>(out.size() - 8);
    std::memcpy(out.data() + 4, &riffSize, 4); // little-endian host (all CI platforms)

    return muse::ByteArray(out.data(), out.size());
}

//! `seconds` of a sine at `freq` Hz, 16-bit.
inline muse::ByteArray makeSineWav(double seconds, uint32_t sampleRate = 48000, uint16_t channels = 2,
                                   double freq = 440.0, double amplitude = 0.5)
{
    const size_t frames = static_cast<size_t>(seconds * sampleRate);
    std::vector<double> samples;
    samples.reserve(frames * channels);
    for (size_t f = 0; f < frames; ++f) {
        const double v = amplitude * std::sin(2.0 * 3.14159265358979323846 * freq * static_cast<double>(f) / sampleRate);
        for (uint16_t c = 0; c < channels; ++c) {
            samples.push_back(c == 0 ? v : -v);
        }
    }
    return makeWav(sampleRate, channels, 16, false, samples);
}

inline bool writeFile(const muse::io::path_t& path, const muse::ByteArray& bytes)
{
    QFile f(path.toQString());
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    return f.write(reinterpret_cast<const char*>(bytes.constData()), static_cast<qint64>(bytes.size()))
           == static_cast<qint64>(bytes.size());
}

inline muse::ByteArray readFile(const muse::io::path_t& path)
{
    QFile f(path.toQString());
    if (!f.open(QIODevice::ReadOnly)) {
        return muse::ByteArray();
    }
    return muse::ByteArray::fromQByteArray(f.readAll());
}

//! Stands in for the .mscz container: entry name -> bytes.
struct FakeProjectFile {
    std::map<std::string, muse::ByteArray> entries;

    ReadProjectEntry reader() const
    {
        return [this](const std::string& name) {
            auto it = entries.find(name);
            return muse::RetVal<muse::ByteArray>::make_ok(it != entries.end() ? it->second : muse::ByteArray());
        };
    }

    WriteProjectEntry writer()
    {
        return [this](const std::string& name, const muse::ByteArray& data) {
            // `data` is only valid during the call (see WriteProjectEntry): keep our own copy
            entries[name] = muse::ByteArray(data.constData(), data.size());
            return muse::make_ok();
        };
    }
};

//! Runs the main thread's message queue until `done` or a timeout. Results of worker threads
//! arrive through it.
inline bool waitFor(const std::function<bool()>& done, int timeoutMs = 10000)
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        muse::async::processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}
}
