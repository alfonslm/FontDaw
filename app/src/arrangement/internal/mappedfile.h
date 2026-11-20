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

#include <QByteArray>
#include <QFile>

#include "global/io/iodevice.h"
#include "global/io/path.h"
#include "global/types/bytearray.h"
#include "global/types/ret.h"

namespace mu::arrangement {
//! The bytes of an audio file, for reading.
//!
//! Map: the file is mapped into memory, so nothing is copied to the heap and the pages are read
//! when they are used (FD-030). Only for files FontDaw owns (the session copies): a mapped file
//! that someone else truncates crashes the reader.
//! Read: the file is read into memory. For files outside FontDaw's folders.
class MappedFile
{
public:
    enum class Mode {
        Map,
        Read,
    };

    //! A RIFF/WAVE file cannot be larger; files named by project files must not exhaust memory
    static constexpr qint64 MAX_FILE_SIZE = qint64(4) * 1024 * 1024 * 1024 + 8;

    //! The Ret text is fit to show to the user. A file larger than `maxSize` is refused.
    muse::Ret open(const muse::io::path_t& path, Mode mode, qint64 maxSize = MAX_FILE_SIZE);

    //! Valid while this object lives. Does not own or copy the bytes.
    muse::ByteArray bytes() const;

    const uint8_t* data() const { return m_data; }
    size_t size() const { return m_size; }

private:
    QFile m_file;
    QByteArray m_read;
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
};

//! An audio file as the io::IODevice the audio engine takes (IPlayback::addTrack).
//! Read-only. The engine reads it on its own thread; keep it alive until addTrack has answered.
class AudioFileDevice : public muse::io::IODevice
{
public:
    AudioFileDevice(const muse::io::path_t& path, MappedFile::Mode mode);

protected:
    bool doOpen(OpenMode m) override;
    size_t dataSize() const override;
    const uint8_t* rawData() const override;
    bool resizeData(size_t size) override;
    size_t writeData(const uint8_t* data, size_t len) override;

private:
    muse::io::path_t m_path;
    MappedFile::Mode m_mode = MappedFile::Mode::Read;
    MappedFile m_file;
};
}
