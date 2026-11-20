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

#include "mappedfile.h"

#include "global/translation.h"

using namespace mu::arrangement;
using namespace muse;

Ret MappedFile::open(const io::path_t& path, Mode mode, qint64 maxSize)
{
    m_data = nullptr;
    m_size = 0;
    m_read.clear();
    if (m_file.isOpen()) {
        m_file.close();
    }

    m_file.setFileName(path.toQString());
    if (!m_file.open(QIODevice::ReadOnly)) {
        return Ret(static_cast<int>(Ret::Code::UnknownError), trc("arrangement", "The audio file could not be opened."));
    }

    const qint64 size = m_file.size();
    if (size > maxSize) {
        m_file.close();
        return Ret(static_cast<int>(Ret::Code::UnknownError), trc("arrangement", "The audio file is too large."));
    }

    if (size == 0) {
        return make_ok();
    }

    if (mode == Mode::Map) {
        if (const uchar* mapped = m_file.map(0, size)) {
            m_data = mapped;
            m_size = static_cast<size_t>(size);
            return make_ok();
        }
        // Not every file system can map; read instead
    }

    m_read = m_file.readAll();
    if (m_file.error() != QFileDevice::NoError || m_read.size() != size) {
        m_read.clear();
        return Ret(static_cast<int>(Ret::Code::UnknownError), trc("arrangement", "The audio file could not be read."));
    }
    m_file.close();

    m_data = reinterpret_cast<const uint8_t*>(m_read.constData());
    m_size = static_cast<size_t>(m_read.size());
    return make_ok();
}

ByteArray MappedFile::bytes() const
{
    return m_data ? ByteArray::fromRawData(m_data, m_size) : ByteArray();
}

AudioFileDevice::AudioFileDevice(const io::path_t& path, MappedFile::Mode mode)
    : m_path(path), m_mode(mode)
{
}

bool AudioFileDevice::doOpen(OpenMode m)
{
    if (m != OpenMode::ReadOnly) {
        setError(static_cast<int>(Ret::Code::NotSupported), "audio files are opened for reading only");
        return false;
    }

    const Ret ret = m_file.open(m_path, m_mode);
    if (!ret) {
        setError(ret.code(), ret.text());
        return false;
    }
    return true;
}

size_t AudioFileDevice::dataSize() const
{
    return m_file.size();
}

const uint8_t* AudioFileDevice::rawData() const
{
    return m_file.data();
}

bool AudioFileDevice::resizeData(size_t)
{
    return false;
}

size_t AudioFileDevice::writeData(const uint8_t*, size_t)
{
    return 0;
}
