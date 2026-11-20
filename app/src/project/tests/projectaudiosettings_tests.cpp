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

#include <gmock/gmock.h>

#include "global/containers.h"
#include "global/io/buffer.h"
#include "engraving/infrastructure/mscreader.h"
#include "engraving/infrastructure/mscwriter.h"
#include "notation/internal/notationsolomutestate.h"

#include "project/internal/projectaudiosettings.h"

#include "playback/tests/mocks/playbackconfigurationmock.h"

using namespace ::testing;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;
using namespace mu::notation;
using namespace mu::project;

namespace mu::project {
//! FontDaw (FD-165): what the project's audio settings keep across a save and reopen
class Project_ProjectAudioSettingsTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_playbackConfig = std::make_shared<NiceMock<playback::PlaybackConfigurationMock> >();
        ON_CALL(*m_playbackConfig, museSoundsProfileName()).WillByDefault(ReturnRef(m_museSounds));
        ON_CALL(*m_playbackConfig, compatMuseSoundsProfileName()).WillByDefault(ReturnRef(m_compatMuseSounds));
    }

    std::shared_ptr<ProjectAudioSettings> makeSettings()
    {
        auto settings = std::shared_ptr<ProjectAudioSettings>(new ProjectAudioSettings(nullptr));
        settings->playbackConfig.set(m_playbackConfig);
        return settings;
    }

    static ByteArray save(ProjectAudioSettings& settings, const INotationSoloMuteStatePtr& masterSoloMute)
    {
        ByteArray data;
        Buffer buf(&data);
        MscWriter::Params params;
        params.device = &buf;
        params.filePath = "audiosettings.mscz";
        params.mode = MscIoMode::Zip;

        MscWriter writer(params);
        writer.open();
        EXPECT_TRUE(settings.write(writer, masterSoloMute));
        writer.close();
        return data;
    }

    static MscReader::Params readerParams(Buffer& buf)
    {
        MscReader::Params params;
        params.device = &buf;
        params.filePath = "audiosettings.mscz";
        params.mode = MscIoMode::Zip;
        return params;
    }

    //! The fixture is the friend: a test body (a subclass) cannot read the map itself
    static bool hasInputParams(const ProjectAudioSettings& settings, const InstrumentTrackId& id)
    {
        return muse::contains(settings.m_trackInputParamsMap, id);
    }

    std::shared_ptr<playback::PlaybackConfigurationMock> m_playbackConfig;
    const playback::SoundProfileName m_museSounds = u"Muse Sounds";
    const playback::SoundProfileName m_compatMuseSounds = u"Compat";
};
}

// FD-165: an audio track (output params only, no sound to choose) keeps its strip and its solo-mute
// state across a save and reopen; an instrument keeps both as before
TEST_F(Project_ProjectAudioSettingsTests, AnAudioTracksMixerSettingsAreSaved)
{
    const InstrumentTrackId instrument { ID(1), u"violin" };
    const InstrumentTrackId audioTrack { ID(998), u"fontdaw-audio-track-1" }; // as arrangement::audioTrackKey(1)

    // [GIVEN] An instrument with input and output params, and an audio track with output params only
    std::shared_ptr<ProjectAudioSettings> settings = makeSettings();
    settings->setActiveSoundProfile(u"Test");

    AudioInputParams instrumentIn;
    settings->setTrackInputParams(instrument, instrumentIn);
    AudioOutputParams instrumentOut;
    instrumentOut.volume = -3.f;
    settings->setTrackOutputParams(instrument, instrumentOut);

    AudioOutputParams audioOut;
    audioOut.volume = -6.f;
    audioOut.balance = 0.25f;
    settings->setTrackOutputParams(audioTrack, audioOut);

    auto masterSoloMute = std::make_shared<NotationSoloMuteState>();
    masterSoloMute->setTrackSoloMuteState(instrument, { /*mute*/ true, /*solo*/ false });
    masterSoloMute->setTrackSoloMuteState(audioTrack, { /*mute*/ false, /*solo*/ true });

    // [WHEN] Saving and reading back
    ByteArray data = save(*settings, masterSoloMute);

    Buffer buf(&data);
    MscReader reader(readerParams(buf));
    ASSERT_TRUE(reader.open());

    std::shared_ptr<ProjectAudioSettings> reread = makeSettings();
    ASSERT_TRUE(reread->read(reader));

    auto rereadSoloMute = std::make_shared<NotationSoloMuteState>();
    ASSERT_TRUE(rereadSoloMute->read(reader));

    // [THEN] The audio track's strip is there, and it has no input params of its own
    ASSERT_TRUE(reread->trackHasExistingOutputParams(audioTrack));
    EXPECT_FALSE(hasInputParams(*reread, audioTrack));
    EXPECT_FLOAT_EQ(reread->trackOutputParams(audioTrack).volume.raw(), -6.f);
    EXPECT_FLOAT_EQ(reread->trackOutputParams(audioTrack).balance.raw(), 0.25f);
    EXPECT_TRUE(rereadSoloMute->trackSoloMuteStateExists(audioTrack));
    EXPECT_TRUE(rereadSoloMute->trackSoloMuteState(audioTrack).solo);

    // [THEN] The instrument as before
    ASSERT_TRUE(reread->trackHasExistingOutputParams(instrument));
    EXPECT_FLOAT_EQ(reread->trackOutputParams(instrument).volume.raw(), -3.f);
    EXPECT_TRUE(rereadSoloMute->trackSoloMuteState(instrument).mute);
}
