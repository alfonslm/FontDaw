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

#include <QTemporaryDir>

#include "internal/mappedfile.h"

#include "testutils.h"

using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace muse;

class Arrangement_MappedFileTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_wav = makeSineWav(0.25, 8000, 2);
        m_path = io::path_t(m_dir.filePath("a.wav"));
        ASSERT_TRUE(writeFile(m_path, m_wav));
    }

    QTemporaryDir m_dir;
    ByteArray m_wav;
    io::path_t m_path;
};

TEST_F(Arrangement_MappedFileTests, MappedAndReadGiveTheSameBytes)
{
    for (MappedFile::Mode mode : { MappedFile::Mode::Map, MappedFile::Mode::Read }) {
        MappedFile file;
        ASSERT_TRUE(file.open(m_path, mode));
        EXPECT_EQ(file.size(), m_wav.size());
        EXPECT_EQ(file.bytes(), m_wav);
    }
}

TEST_F(Arrangement_MappedFileTests, MissingAndEmptyFiles)
{
    MappedFile missing;
    Ret ret = missing.open(io::path_t(m_dir.filePath("nothing.wav")), MappedFile::Mode::Map);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(ret.text().empty());
    EXPECT_TRUE(missing.bytes().empty());

    const io::path_t emptyPath(m_dir.filePath("empty.wav"));
    ASSERT_TRUE(writeFile(emptyPath, ByteArray()));
    MappedFile empty;
    EXPECT_TRUE(empty.open(emptyPath, MappedFile::Mode::Map));
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_TRUE(empty.bytes().empty());
}

TEST_F(Arrangement_MappedFileTests, RefusesFilesLargerThanTheLimit)
{
    const qint64 size = static_cast<qint64>(m_wav.size());
    for (MappedFile::Mode mode : { MappedFile::Mode::Map, MappedFile::Mode::Read }) {
        MappedFile tooBig;
        Ret ret = tooBig.open(m_path, mode, size - 1);
        EXPECT_FALSE(ret);
        EXPECT_FALSE(ret.text().empty());
        EXPECT_EQ(tooBig.size(), 0u);
        EXPECT_TRUE(tooBig.bytes().empty());

        MappedFile exact;
        ASSERT_TRUE(exact.open(m_path, mode, size));
        EXPECT_EQ(exact.bytes(), m_wav);
    }
}

TEST_F(Arrangement_MappedFileTests, DeviceForTheAudioEngine)
{
    // What the engine does with the device: open if needed, go to the start, take all the bytes
    AudioFileDevice device(m_path, MappedFile::Mode::Map);
    ASSERT_TRUE(device.open(io::IODevice::ReadOnly));
    ASSERT_TRUE(device.seek(0));
    EXPECT_EQ(device.size(), m_wav.size());
    EXPECT_EQ(ByteArray(device.readData(), device.size()), m_wav);

    // Read-only
    AudioFileDevice writable(m_path, MappedFile::Mode::Read);
    EXPECT_FALSE(writable.open(io::IODevice::WriteOnly));

    AudioFileDevice missing(io::path_t(m_dir.filePath("nothing.wav")), MappedFile::Mode::Read);
    EXPECT_FALSE(missing.open(io::IODevice::ReadOnly));
    EXPECT_TRUE(missing.hasError());
}
