/*
 * SPDX-License-Identifier: GPL-3.0-only
 * FontDaw
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

#include <filesystem>
#include <fstream>
#include <iterator>

#include <QTemporaryDir>

#include "modularity/ioc.h"

#include "notation/imasternotation.h"
#include "notation/inotationsolomutestate.h"

#include "project/internal/notationproject.h"

#include "mocks/projectconfigurationmock.h"
#include "notation/tests/mocks/notationconfigurationmock.h"
#include "playback/tests/mocks/playbackconfigurationmock.h"

using namespace ::testing;
using namespace muse;
using namespace mu::project;

namespace {
const char* TEST_MODULE = "project_notationproject_tests";

muse::io::path_t dataPath(const std::string& name)
{
    return muse::io::path_t((std::filesystem::path(project_test_DATA_ROOT) / "notationproject" / name).string());
}
}

//! FontDaw (FD-175): what opening a project counts as a change
class Project_NotationProjectTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_ctx = std::make_shared<modularity::Context>(1);
        m_configuration = std::make_shared<NiceMock<ProjectConfigurationMock> >();
        modularity::globalIoc()->registerExport<IProjectConfiguration>(TEST_MODULE, m_configuration);

        m_notationConfiguration = std::make_shared<NiceMock<mu::notation::NotationConfigurationMock> >();
        ON_CALL(*m_notationConfiguration, canvasOrientation()).WillByDefault(Return(m_canvasOrientation));
        modularity::globalIoc()->registerExport<mu::notation::INotationConfiguration>(TEST_MODULE, m_notationConfiguration);

        //! NOTE Reading the stored audio settings asks for the sound profiles' names
        m_playbackConfiguration = std::make_shared<NiceMock<mu::playback::PlaybackConfigurationMock> >();
        ON_CALL(*m_playbackConfiguration, museSoundsProfileName()).WillByDefault(ReturnRef(m_museSounds));
        ON_CALL(*m_playbackConfiguration, compatMuseSoundsProfileName()).WillByDefault(ReturnRef(m_compatMuseSounds));
        modularity::globalIoc()->registerExport<mu::playback::IPlaybackConfiguration>(TEST_MODULE, m_playbackConfiguration);
    }

    void TearDown() override
    {
        modularity::globalIoc()->unregister<IProjectConfiguration>(TEST_MODULE);
        modularity::globalIoc()->unregister<mu::notation::INotationConfiguration>(TEST_MODULE);
        modularity::globalIoc()->unregister<mu::playback::IPlaybackConfiguration>(TEST_MODULE);
        modularity::removeIoC(m_ctx);
    }

    std::shared_ptr<NotationProject> open(const std::string& name, bool disablePlayback = true)
    {
        auto project = std::make_shared<NotationProject>(m_ctx);
        OpenParams params;
        params.disablePlayback = disablePlayback;
        EXPECT_TRUE(project->load(dataPath(name), params));

        //! NOTE What the project session does when the score is shown (ProjectSession::doSetCurrentNotation)
        project->masterNotation()->notation()->setIsOpen(true);
        return project;
    }

    modularity::ContextPtr m_ctx;
    std::shared_ptr<NiceMock<ProjectConfigurationMock> > m_configuration;
    std::shared_ptr<NiceMock<mu::notation::NotationConfigurationMock> > m_notationConfiguration;
    muse::ValCh<muse::Orientation> m_canvasOrientation;
    std::shared_ptr<NiceMock<mu::playback::PlaybackConfigurationMock> > m_playbackConfiguration;
    const mu::playback::SoundProfileName m_museSounds = u"Muse Sounds";
    const mu::playback::SoundProfileName m_compatMuseSounds = u"Compat";
};

// A file that stores no open state, view or audio settings (an older or imported one) is not
// "modified" just because it was opened and shown
TEST_F(Project_NotationProjectTests, OpeningAProjectWithoutStoredSettingsLeavesItSaved)
{
    std::shared_ptr<NotationProject> project = open("without_stored_settings.mscx");

    EXPECT_FALSE(project->isNeedSave());
}

// A project the app saved, with its audio and view settings, opened and shown without a change, is
// not "modified" either (the case of most files)
TEST_F(Project_NotationProjectTests, AProjectWithStoredSettingsOpenedUnchangedLeavesItSaved)
{
    std::shared_ptr<NotationProject> project = open("with_stored_settings.mscz", /*disablePlayback*/ false);

    EXPECT_FALSE(project->isNeedSave());
}

// The starting solo-mute state of a track (set up while loading) is not a change either
TEST_F(Project_NotationProjectTests, ASetUpSoloMuteStateLeavesItSaved)
{
    std::shared_ptr<NotationProject> project = open("without_stored_settings.mscx");

    const mu::engraving::InstrumentTrackId flute { muse::ID(1), u"wind.flutes.flute" };
    project->masterNotation()->notation()->soloMuteState()->initTrackSoloMuteState(flute, { /*mute*/ false, /*solo*/ false });

    EXPECT_FALSE(project->isNeedSave());
}

// A solo or mute the user sets after opening still marks the project
TEST_F(Project_NotationProjectTests, ALaterSoloChangeMarksItUnsaved)
{
    std::shared_ptr<NotationProject> project = open("without_stored_settings.mscx");
    ASSERT_FALSE(project->isNeedSave());

    const mu::engraving::InstrumentTrackId flute { muse::ID(1), u"wind.flutes.flute" };
    project->masterNotation()->notation()->soloMuteState()->setTrackSoloMuteState(flute, { /*mute*/ false, /*solo*/ true });

    EXPECT_TRUE(project->isNeedSave());
}

// FontDaw (FD-183): a score MuseScore saved last is marked, so that FontDaw never overwrites it
TEST_F(Project_NotationProjectTests, AScoreSavedByMuseScoreIsMarked)
{
    std::shared_ptr<NotationProject> project = open("saved_by_musescore.mscx");

    EXPECT_TRUE(project->isSavedByMuseScore());
    EXPECT_FALSE(project->isNeedSave());
}

TEST_F(Project_NotationProjectTests, AScoreSavedByFontDawIsNotMarked)
{
    std::shared_ptr<NotationProject> project = open("saved_by_fontdaw.mscx");

    EXPECT_FALSE(project->isSavedByMuseScore());
}

TEST_F(Project_NotationProjectTests, TheRuleTellsFontDawFromMuseScore)
{
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(500, u"0.1.0"));
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(460, u"0.1.0"));
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(500, u"2.9"));
    EXPECT_TRUE(NotationProject::isSavedByMuseScore(460, u"4.6.3"));
    EXPECT_TRUE(NotationProject::isSavedByMuseScore(302, u"3.6.2"));
    EXPECT_TRUE(NotationProject::isSavedByMuseScore(500, u"5.0.0"));
    EXPECT_TRUE(NotationProject::isSavedByMuseScore(460, u""));
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(500, u""));
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(460, u"abc"));
}

TEST_F(Project_NotationProjectTests, SavingAsANewFileClearsTheMark)
{
    const muse::io::path_t original = dataPath("saved_by_musescore.mscx");
    auto readBytes = [](const muse::io::path_t& path) {
        std::ifstream in(path.toStdString(), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const std::string before = readBytes(original);

    std::shared_ptr<NotationProject> project = open("saved_by_musescore.mscx");
    ASSERT_TRUE(project->isSavedByMuseScore());

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const muse::io::path_t copy = muse::io::path_t(dir.path()).appendingComponent("copy.mscz");

    EXPECT_TRUE(project->save(copy, SaveMode::SaveAs));
    EXPECT_FALSE(project->isSavedByMuseScore());
    EXPECT_EQ(before, readBytes(original));
}

// FontDaw (FD-183): a recovered autosave (written by FontDaw) takes the marks from the original
TEST_F(Project_NotationProjectTests, ARecoveredAutosaveKeepsTheOriginalsMarkFromMuseScore)
{
    std::shared_ptr<NotationProject> project = open("saved_by_fontdaw.mscx"); // stands in for the autosave
    EXPECT_FALSE(project->isSavedByMuseScore());

    const muse::io::path_t original = dataPath("saved_by_musescore.mscx");
    project->setPath(original);
    project->takeOverwriteProtectionFrom(original);

    EXPECT_TRUE(project->isSavedByMuseScore());
}

TEST_F(Project_NotationProjectTests, ReadOverwriteProtectionReadsTheHeader)
{
    auto p = NotationProject::readOverwriteProtection(dataPath("saved_by_musescore.mscx"), muse::Version(0, 1, 0));
    EXPECT_TRUE(p.savedByMuseScore);
    EXPECT_TRUE(p.newerFontDawVersion.isEmpty());

    p = NotationProject::readOverwriteProtection(dataPath("saved_by_fontdaw.mscx"), muse::Version(0, 1, 0));
    EXPECT_FALSE(p.savedByMuseScore);
    EXPECT_TRUE(p.newerFontDawVersion.isEmpty());

    p = NotationProject::readOverwriteProtection(dataPath("saved_by_fontdaw.mscx"), muse::Version(0, 0, 9));
    EXPECT_FALSE(p.savedByMuseScore);
    EXPECT_EQ(p.newerFontDawVersion, u"0.1.0");

    p = NotationProject::readOverwriteProtection(dataPath("does_not_exist.mscx"), muse::Version(0, 1, 0));
    EXPECT_FALSE(p.savedByMuseScore);
}

// FontDaw (FD-183): a crafted programVersion must not crash (muse::Version throws or overflows on it)
TEST_F(Project_NotationProjectTests, ACraftedProgramVersionIsHarmless)
{
    const muse::Version app(0, 1, 0);
    const std::vector<muse::String> crafted = {
        u"0.1.0.1", u"1.2.3.4.5", u"99999999999999999999.0", u"0.99999999999.0", u"0..1", u".1", u"1.", u"a.b.c",
        u"1234567890.1234567890.1234567890.12345678", u"12.34.56.78.90.12.34.56.78.90.12.34.56.78.90",
    };
    for (const muse::String& v : crafted) {
        EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(500, v, app)) << v.toStdString();
    }
    EXPECT_FALSE(NotationProject::isSavedByMuseScore(460, u"99999999999999999999.0"));
}

// FontDaw (FD-183): a score saved by a newer FontDaw is protected the same way
TEST_F(Project_NotationProjectTests, TheRuleTellsANewerFontDaw)
{
    const muse::Version app(0, 1, 0);
    EXPECT_TRUE(NotationProject::isSavedByNewerFontDaw(500, u"0.2.0", app));   // newer minor
    EXPECT_TRUE(NotationProject::isSavedByNewerFontDaw(500, u"0.10.0", app));  // compared as numbers
    EXPECT_TRUE(NotationProject::isSavedByNewerFontDaw(500, u"1.0.0", app));   // newer major
    EXPECT_TRUE(NotationProject::isSavedByNewerFontDaw(500, u"0.1.1", app));   // a newer patch is newer too
    EXPECT_TRUE(NotationProject::isSavedByNewerFontDaw(500, u"0.1.10", muse::Version(0, 1, 9))); // compared as numbers
    EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(500, u"0.1.0", app));  // same
    EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(500, u"0.0.9", app));  // older
    EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(500, u"0.2.0", muse::Version(0, 2, 3))); // older patch
    EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(460, u"4.6.3", app));
    EXPECT_FALSE(NotationProject::isSavedByNewerFontDaw(500, u"", app));
}
