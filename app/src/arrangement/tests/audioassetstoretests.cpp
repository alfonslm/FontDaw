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
#include <gmock/gmock.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "internal/audioassetstore.h"
#include "internal/waveformpeaks.h"

#include "mocks/arrangementconfigurationmock.h"
#include "testutils.h"

using ::testing::NiceMock;
using ::testing::Return;

using namespace mu::arrangement;
using namespace mu::arrangement::tests;
using namespace muse;

static const std::string TEST_MODULE("arrangement_utests");
static const modularity::IoCID TEST_CONTEXT_ID = 1;

class Arrangement_AudioAssetStoreTests : public ::testing::Test, public async::Asyncable
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_sessions = path("sessions");

        m_configuration = std::make_shared<NiceMock<ArrangementConfigurationMock> >();
        ON_CALL(*m_configuration, defaultAssetStorage()).WillByDefault(Return(AssetStorage::Embedded));
        ON_CALL(*m_configuration, audioSessionsPath()).WillByDefault(Return(m_sessions));
        modularity::globalIoc()->registerExport<IArrangementConfiguration>(TEST_MODULE, m_configuration);

        m_iocCtx = std::make_shared<modularity::Context>(TEST_CONTEXT_ID);
        m_store = std::make_shared<AudioAssetStore>(m_iocCtx);
    }

    void TearDown() override
    {
        m_store = nullptr;
        modularity::globalIoc()->unregister<IArrangementConfiguration>(TEST_MODULE);
        modularity::removeIoC(m_iocCtx);
    }

    io::path_t path(const QString& relative) const
    {
        return io::path_t(m_dir.filePath(relative));
    }

    io::path_t writeWav(const QString& relative, const ByteArray& bytes) const
    {
        const io::path_t p = path(relative);
        QDir().mkpath(QFileInfo(p.toQString()).absolutePath());
        EXPECT_TRUE(writeFile(p, bytes));
        return p;
    }

    static AssetId sha1(const ByteArray& bytes)
    {
        return QCryptographicHash::hash(bytes.toQByteArrayNoCopy(), QCryptographicHash::Sha1).toHex().toStdString();
    }

    QTemporaryDir m_dir;
    io::path_t m_sessions;
    std::shared_ptr<NiceMock<ArrangementConfigurationMock> > m_configuration;
    modularity::ContextPtr m_iocCtx;
    std::shared_ptr<AudioAssetStore> m_store;
};

TEST_F(Arrangement_AudioAssetStoreTests, ImportReadsFormatAndPeaks)
{
    const ByteArray wav = makeSineWav(0.5, 44100, 2);
    const io::path_t file = writeWav("Vocals take 3.wav", wav);

    RetVal<AudioAsset> imported = m_store->importFileSync(file, AssetStorage::Embedded);
    ASSERT_TRUE(imported.ret) << imported.ret.text();

    const AudioAsset& a = imported.val;
    EXPECT_EQ(a.name, String(u"Vocals take 3"));
    EXPECT_EQ(a.format, "wav");
    EXPECT_EQ(a.sampleRate, 44100u);
    EXPECT_EQ(a.channels, 2);
    EXPECT_EQ(a.frames, 22050u);
    EXPECT_NEAR(a.duration().raw(), 0.5, 1e-9);
    EXPECT_FALSE(a.offline);

    std::shared_ptr<const WaveformPeaks> peaks = m_store->peaks(a.id);
    ASSERT_TRUE(peaks);
    EXPECT_EQ(peaks->frames, 22050u);

    ASSERT_EQ(m_store->assets().size(), 1u);
    ASSERT_TRUE(m_store->asset(a.id));
}

TEST_F(Arrangement_AudioAssetStoreTests, AssetIdIsTheContentHashAndStable)
{
    const ByteArray wav = makeSineWav(0.25);
    const io::path_t first = writeWav("a.wav", wav);
    const io::path_t sameBytesOtherName = writeWav("folder/b.wav", wav);
    const io::path_t otherBytes = writeWav("c.wav", makeSineWav(0.25, 48000, 2, 220.0));

    RetVal<AudioAsset> a = m_store->importFileSync(first, AssetStorage::Embedded);
    RetVal<AudioAsset> again = m_store->importFileSync(first, AssetStorage::Embedded);
    RetVal<AudioAsset> b = m_store->importFileSync(sameBytesOtherName, AssetStorage::Referenced);
    RetVal<AudioAsset> c = m_store->importFileSync(otherBytes, AssetStorage::Embedded);
    ASSERT_TRUE(a.ret && again.ret && b.ret && c.ret);

    // The id is the SHA-1 of the bytes: independent of the file name, the session and the store.
    EXPECT_EQ(a.val.id, sha1(wav));
    EXPECT_EQ(a.val.id.size(), 40u);
    EXPECT_EQ(again.val.id, a.val.id);
    EXPECT_EQ(b.val.id, a.val.id);
    EXPECT_NE(c.val.id, a.val.id);

    // Importing identical bytes twice gives one asset, and keeps its storage.
    EXPECT_EQ(m_store->assets().size(), 2u);
    EXPECT_EQ(b.val.storage, AssetStorage::Embedded);

    // A second store (another session) gives the same id for the same bytes.
    AudioAssetStore other(m_iocCtx);
    EXPECT_EQ(other.importFileSync(sameBytesOtherName, AssetStorage::Embedded).val.id, a.val.id);
}

TEST_F(Arrangement_AudioAssetStoreTests, RejectsUnsupportedFilesWithAMessage)
{
    const io::path_t text = path("notes.txt");
    ASSERT_TRUE(writeFile(text, ByteArray("just some text")));
    RetVal<AudioAsset> r = m_store->importFileSync(text, AssetStorage::Embedded);
    EXPECT_FALSE(r.ret);
    EXPECT_FALSE(r.ret.text().empty());

    const io::path_t surround = writeWav("surround.wav", makeWav(48000, 6, 16, false, std::vector<double>(6, 0.0)));
    r = m_store->importFileSync(surround, AssetStorage::Embedded);
    EXPECT_EQ(r.ret.code(), static_cast<int>(Ret::Code::NotSupported));
    EXPECT_FALSE(r.ret.text().empty());

    r = m_store->importFileSync(path("does-not-exist.wav"), AssetStorage::Embedded);
    EXPECT_FALSE(r.ret);

    EXPECT_TRUE(m_store->assets().empty());
}

TEST_F(Arrangement_AudioAssetStoreTests, EmbeddedAudioIsCopiedAndSavedIntoTheProject)
{
    const ByteArray wav = makeSineWav(0.25);
    const io::path_t file = writeWav("take.wav", wav);

    RetVal<AudioAsset> a = m_store->importFileSync(file, AssetStorage::Embedded);
    ASSERT_TRUE(a.ret);
    EXPECT_EQ(a.val.storage, AssetStorage::Embedded);
    EXPECT_EQ(a.val.fileInProject, "audio/" + a.val.id + ".wav");

    // The engine reads a session copy, not the original: the original may go away.
    const io::path_t sessionCopy = m_store->assetFilePath(a.val.id);
    EXPECT_NE(sessionCopy, file);
    EXPECT_TRUE(sessionCopy.toStdString().rfind(m_sessions.toStdString(), 0) == 0) << sessionCopy.toStdString();
    EXPECT_EQ(readFile(sessionCopy), wav);
    ASSERT_TRUE(QFile::remove(file.toQString()));

    // Save: the original bytes go to audio/<id>.wav, never re-encoded.
    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { a.val.id });
    ASSERT_TRUE(table.ret);
    ASSERT_EQ(table.val.size(), 1u);
    ASSERT_EQ(project.entries.count(a.val.fileInProject), 1u);
    EXPECT_EQ(project.entries[a.val.fileInProject], wav);

    // Load into a fresh store: the asset is back, with its bytes in that store's session folder.
    AudioAssetStore reopened(m_iocCtx);
    ASSERT_TRUE(reopened.load(table.val, project.reader()));
    const AudioAsset* back = reopened.asset(a.val.id);
    ASSERT_TRUE(back);
    EXPECT_FALSE(back->offline);
    EXPECT_EQ(readFile(reopened.assetFilePath(a.val.id)), wav);

    // Peaks are rebuilt in the background.
    EXPECT_TRUE(waitFor([&]() { return reopened.peaks(a.val.id) != nullptr; }));

    // clear() removes the session copies.
    const io::path_t reopenedCopy = reopened.assetFilePath(a.val.id);
    reopened.clear();
    EXPECT_FALSE(QFileInfo::exists(reopenedCopy.toQString()));
    EXPECT_TRUE(reopened.assets().empty());
}

TEST_F(Arrangement_AudioAssetStoreTests, SaveWritesOnlyTheRequestedAssets)
{
    RetVal<AudioAsset> used = m_store->importFileSync(writeWav("used.wav", makeSineWav(0.1)), AssetStorage::Embedded);
    RetVal<AudioAsset> unused = m_store->importFileSync(writeWav("unused.wav", makeSineWav(0.1, 48000, 1)), AssetStorage::Embedded);
    ASSERT_TRUE(used.ret && unused.ret);

    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { used.val.id });
    ASSERT_TRUE(table.ret);
    ASSERT_EQ(table.val.size(), 1u);
    EXPECT_EQ(table.val.front().id, used.val.id);
    EXPECT_EQ(project.entries.size(), 1u);

    m_store->removeUnused({ used.val.id });
    EXPECT_TRUE(m_store->asset(used.val.id));
    EXPECT_FALSE(m_store->asset(unused.val.id));
    EXPECT_FALSE(m_store->peaks(unused.val.id));
}

TEST_F(Arrangement_AudioAssetStoreTests, ReferencedAudioStaysOnDiskWithARelativePath)
{
    // project/Song.mscz and Recordings/gtr.wav next to the project folder
    const ByteArray wav = makeSineWav(0.2, 48000, 1);
    const io::path_t recording = writeWav("Recordings/gtr.wav", wav);
    QDir().mkpath(path("project").toQString());
    m_store->setProjectPath(path("project/Song.mscz"));

    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);
    EXPECT_EQ(a.val.storage, AssetStorage::Referenced);
    EXPECT_TRUE(a.val.fileInProject.empty());
    EXPECT_EQ(a.val.absolutePath, recording);
    EXPECT_EQ(a.val.relativePath, io::path_t("../Recordings/gtr.wav"));
    EXPECT_EQ(m_store->assetFilePath(a.val.id), recording);

    // Nothing was copied into the session folder, nothing is written into the project.
    EXPECT_FALSE(QDir(m_sessions.toQString()).exists());
    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { a.val.id });
    ASSERT_TRUE(table.ret);
    EXPECT_TRUE(project.entries.empty());
    EXPECT_EQ(table.val.front().relativePath, io::path_t("../Recordings/gtr.wav"));

    // Move the whole folder (project + recordings): the relative path finds the file again.
    ASSERT_TRUE(QDir().rename(m_dir.path(), m_dir.path() + "-moved"));
    const QString moved = m_dir.path() + "-moved";
    AudioAssetStore reopened(m_iocCtx);
    reopened.setProjectPath(io::path_t(moved + "/project/Song.mscz"));
    ASSERT_TRUE(reopened.load(table.val, project.reader()));
    const AudioAsset* back = reopened.asset(a.val.id);
    ASSERT_TRUE(back);
    EXPECT_FALSE(back->offline);
    EXPECT_EQ(reopened.assetFilePath(a.val.id), io::path_t(moved + "/Recordings/gtr.wav"));
    EXPECT_TRUE(waitFor([&]() { return reopened.peaks(a.val.id) != nullptr; }));
    EXPECT_FALSE(back->modifiedOnDisk);

    ASSERT_TRUE(QDir().rename(moved, m_dir.path()));
}

TEST_F(Arrangement_AudioAssetStoreTests, MissingReferenceIsOfflineNotAnError)
{
    const io::path_t recording = writeWav("gone.wav", makeSineWav(0.1));
    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { a.val.id });
    ASSERT_TRUE(QFile::remove(recording.toQString()));

    AudioAssetStore reopened(m_iocCtx);
    EXPECT_TRUE(reopened.load(table.val, project.reader()));
    const AudioAsset* back = reopened.asset(a.val.id);
    ASSERT_TRUE(back);
    EXPECT_TRUE(back->offline);
    EXPECT_TRUE(reopened.assetFilePath(a.val.id).empty());

    // Importing the file again (e.g. "locate") brings the asset back online.
    ASSERT_TRUE(writeFile(recording, makeSineWav(0.1)));
    RetVal<AudioAsset> relinked = reopened.importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(relinked.ret);
    EXPECT_FALSE(reopened.asset(a.val.id)->offline);
    EXPECT_EQ(reopened.assetFilePath(a.val.id), recording);
}

TEST_F(Arrangement_AudioAssetStoreTests, ReferenceChangedOnDiskIsFlagged)
{
    const io::path_t recording = writeWav("edited.wav", makeSineWav(0.1));
    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { a.val.id });

    // Edited outside FontDaw
    ASSERT_TRUE(writeFile(recording, makeSineWav(0.1, 48000, 2, 880.0)));

    AudioAssetStore reopened(m_iocCtx);
    ASSERT_TRUE(reopened.load(table.val, project.reader()));
    EXPECT_TRUE(waitFor([&]() { return reopened.asset(a.val.id)->modifiedOnDisk; }));
    EXPECT_FALSE(reopened.asset(a.val.id)->offline);

    // Embedding it now would embed different audio under the old id: refused.
    EXPECT_FALSE(reopened.setStorage(a.val.id, AssetStorage::Embedded));
}

TEST_F(Arrangement_AudioAssetStoreTests, StorageCanBeSwitched)
{
    const ByteArray wav = makeSineWav(0.1);
    const io::path_t file = writeWav("switch.wav", wav);

    RetVal<AudioAsset> a = m_store->importFileSync(file, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    // Referenced -> embedded: the bytes come into the session folder
    ASSERT_TRUE(m_store->setStorage(a.val.id, AssetStorage::Embedded));
    const AudioAsset* e = m_store->asset(a.val.id);
    EXPECT_EQ(e->storage, AssetStorage::Embedded);
    EXPECT_EQ(e->fileInProject, "audio/" + a.val.id + ".wav");
    EXPECT_NE(m_store->assetFilePath(a.val.id), file);
    EXPECT_EQ(readFile(m_store->assetFilePath(a.val.id)), wav);

    // Embedded -> referenced: links to the original, which still has the same bytes
    ASSERT_TRUE(m_store->setStorage(a.val.id, AssetStorage::Referenced));
    EXPECT_EQ(m_store->asset(a.val.id)->storage, AssetStorage::Referenced);
    EXPECT_EQ(m_store->assetFilePath(a.val.id), file);

    // ... which is refused once the original changed
    ASSERT_TRUE(m_store->setStorage(a.val.id, AssetStorage::Embedded));
    ASSERT_TRUE(writeFile(file, makeSineWav(0.1, 48000, 2, 1000.0)));
    Ret ret = m_store->setStorage(a.val.id, AssetStorage::Referenced);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(ret.text().empty());
    EXPECT_EQ(m_store->asset(a.val.id)->storage, AssetStorage::Embedded);
}

TEST_F(Arrangement_AudioAssetStoreTests, AsyncImportUsesTheDefaultStoragePreference)
{
    const io::path_t file = writeWav("async.wav", makeSineWav(0.1));

    for (AssetStorage preference : { AssetStorage::Referenced, AssetStorage::Embedded }) {
        m_store->clear();
        ON_CALL(*m_configuration, defaultAssetStorage()).WillByDefault(Return(preference));

        bool resolved = false;
        RetVal<AudioAsset> result;
        m_store->importFile(file).onResolve(this, [&](const RetVal<AudioAsset>& r) {
            result = r;
            resolved = true;
        });

        ASSERT_TRUE(waitFor([&]() { return resolved; }));
        ASSERT_TRUE(result.ret) << result.ret.text();
        EXPECT_EQ(result.val.storage, preference);
        EXPECT_TRUE(m_store->asset(result.val.id));
        EXPECT_TRUE(m_store->peaks(result.val.id));
    }

    // A failing import resolves with the error, on the main thread.
    bool resolved = false;
    RetVal<AudioAsset> result;
    m_store->importFile(path("missing.wav")).onResolve(this, [&](const RetVal<AudioAsset>& r) {
        result = r;
        resolved = true;
    });
    ASSERT_TRUE(waitFor([&]() { return resolved; }));
    EXPECT_FALSE(result.ret);
}

TEST_F(Arrangement_AudioAssetStoreTests, ClearDuringImportCancelsIt)
{
    const io::path_t file = writeWav("slow.wav", makeSineWav(2.0));

    bool resolved = false;
    RetVal<AudioAsset> result;
    m_store->importFile(file, AssetStorage::Embedded).onResolve(this, [&](const RetVal<AudioAsset>& r) {
        result = r;
        resolved = true;
    });
    m_store->clear(); // project closed while the worker runs

    ASSERT_TRUE(waitFor([&]() { return resolved; }));
    EXPECT_EQ(result.ret.code(), static_cast<int>(Ret::Code::Cancel));
    EXPECT_TRUE(m_store->assets().empty());
}

// --- Review of PR #120: crafted project files, lost session copies, relinking ---

TEST_F(Arrangement_AudioAssetStoreTests, CraftedAssetIdCannotWriteOutsideTheSessionFolder)
{
    FakeProjectFile project;
    project.entries["audio/x.wav"] = ByteArray("PWNED", 5);
    AudioAsset evil;
    evil.id = "../escaped";
    evil.format = "txt";
    evil.storage = AssetStorage::Embedded;
    evil.fileInProject = "audio/x.wav";

    EXPECT_FALSE(m_store->load({ evil }, project.reader()));
    EXPECT_FALSE(QFileInfo(m_sessions.appendingComponent("escaped.txt").toQString()).isFile());
    EXPECT_TRUE(m_store->assets().empty());
}

TEST_F(Arrangement_AudioAssetStoreTests, CraftedAssetIdCannotCopyAnOutsideFileIntoTheProject)
{
    ASSERT_TRUE(writeFile(path("secret.txt"), ByteArray("TOP SECRET", 10)));
    AudioAsset evil;
    evil.id = "../../secret";
    evil.format = "txt";
    evil.storage = AssetStorage::Embedded;
    evil.fileInProject = "audio/leak";

    FakeProjectFile opened;
    EXPECT_FALSE(m_store->load({ evil }, opened.reader()));

    FakeProjectFile saved;
    RetVal<AudioAssetList> table = m_store->save(saved.writer(), { evil.id });
    EXPECT_EQ(saved.entries.count("audio/leak"), 0u);
    EXPECT_TRUE(saved.entries.empty());
}

TEST_F(Arrangement_AudioAssetStoreTests, LoadRefusesUnsafeFormatsAndFileNames)
{
    const AssetId id = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
    AudioAsset badFormat;
    badFormat.id = id;
    badFormat.format = "../wav";
    badFormat.storage = AssetStorage::Referenced;
    badFormat.absolutePath = path("x.wav");
    EXPECT_FALSE(m_store->load({ badFormat }, FakeProjectFile().reader()));

    AudioAsset badFile;
    badFile.id = id;
    badFile.format = "wav";
    badFile.storage = AssetStorage::Embedded;
    badFile.fileInProject = "../../outside.wav";
    EXPECT_FALSE(m_store->load({ badFile }, FakeProjectFile().reader()));
}

TEST_F(Arrangement_AudioAssetStoreTests, SaveFailsWhenEmbeddedAudioVanished)
{
    RetVal<AudioAsset> a = m_store->importFileSync(writeWav("v.wav", makeSineWav(0.1)), AssetStorage::Embedded);
    ASSERT_TRUE(a.ret);

    // The session copy disappears while the project is open: saving must not drop the audio.
    ASSERT_TRUE(QFile::remove(m_store->assetFilePath(a.val.id).toQString()));
    FakeProjectFile project;
    RetVal<AudioAssetList> table = m_store->save(project.writer(), { a.val.id });
    EXPECT_FALSE(table.ret);
    EXPECT_FALSE(table.ret.text().empty());
}

TEST_F(Arrangement_AudioAssetStoreTests, ReimportBringsMissingEmbeddedAudioBack)
{
    const ByteArray wav = makeSineWav(0.1);
    const AssetId id = sha1(wav);

    // A project whose embedded audio entry is missing: the asset is offline.
    AudioAsset missing;
    missing.id = id;
    missing.format = "wav";
    missing.storage = AssetStorage::Embedded;
    missing.fileInProject = embeddedFileName(id, "wav");
    missing.sampleRate = 48000;
    missing.channels = 2;
    missing.frames = 4800;
    ASSERT_TRUE(m_store->load({ missing }, FakeProjectFile().reader()));
    ASSERT_TRUE(m_store->asset(id)->offline);

    // Importing the same bytes (embedded) brings it back, still embedded.
    RetVal<AudioAsset> back = m_store->importFileSync(writeWav("found.wav", wav), AssetStorage::Embedded);
    ASSERT_TRUE(back.ret);
    EXPECT_FALSE(m_store->asset(id)->offline);
    EXPECT_EQ(m_store->asset(id)->storage, AssetStorage::Embedded);
    EXPECT_EQ(readFile(m_store->assetFilePath(id)), wav);

    FakeProjectFile project;
    ASSERT_TRUE(m_store->save(project.writer(), { id }).ret);
    EXPECT_EQ(project.entries[embeddedFileName(id, "wav")], wav);
}

TEST_F(Arrangement_AudioAssetStoreTests, EmbeddingRechecksTheLinkedFile)
{
    const io::path_t file = writeWav("linked.wav", makeSineWav(0.1));
    RetVal<AudioAsset> a = m_store->importFileSync(file, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    // Changed after import, before anything noticed
    ASSERT_TRUE(writeFile(file, makeSineWav(0.1, 48000, 2, 660.0)));
    EXPECT_FALSE(m_store->setStorage(a.val.id, AssetStorage::Embedded));
    EXPECT_EQ(m_store->asset(a.val.id)->storage, AssetStorage::Referenced);
    EXPECT_TRUE(m_store->asset(a.val.id)->modifiedOnDisk);
}

// --- C2: before projects are opened with this (FD-030, FD-031), and "embed in the copy" ---

TEST_F(Arrangement_AudioAssetStoreTests, NetworkPathsAreRecognisedFromTheirText)
{
    EXPECT_TRUE(AudioAssetStore::isNetworkPath(io::path_t("//server/share/take.wav")));
    EXPECT_TRUE(AudioAssetStore::isNetworkPath(io::path_t(QString("\\\\server\\share\\take.wav"))));
    EXPECT_TRUE(AudioAssetStore::isNetworkPath(io::path_t(QString("\\\\?\\UNC\\server\\share\\take.wav"))));
    EXPECT_EQ(AudioAssetStore::networkShare(io::path_t(QString("\\\\Server\\Share\\a\\b.wav"))), "//server/share");
    EXPECT_EQ(AudioAssetStore::networkShare(io::path_t(QString("\\\\?\\UNC\\Server\\Share\\b.wav"))), "//server/share");
    EXPECT_TRUE(AudioAssetStore::isNetworkPath(io::path_t("/net/server/share/a.wav")));
    EXPECT_EQ(AudioAssetStore::networkShare(io::path_t("/net/Server/share/a.wav")), "/net/server");

    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("/net/")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("/network/x.wav")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("/home/net/x.wav")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("/home/me/take.wav")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("C:/Users/me/take.wav")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t(QString("\\\\?\\C:\\Users\\me\\take.wav")))); // long local path
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t("../Recordings/take.wav")));
    EXPECT_FALSE(AudioAssetStore::isNetworkPath(io::path_t()));
}

TEST_F(Arrangement_AudioAssetStoreTests, ProjectFileCannotMakeTheStoreContactANetworkShare)
{
    // A linked asset as a (crafted) project file could list it: every path points at a server
    AudioAsset a;
    a.id = std::string(40, 'b');
    a.format = "wav";
    a.storage = AssetStorage::Referenced;
    a.relativePath = io::path_t("//evil.example/share/x.wav");
    a.absolutePath = io::path_t(QString("\\\\evil.example\\share\\x.wav"));
    a.sampleRate = 48000;
    a.channels = 1;
    a.frames = 48000;

    m_store->setProjectPath(path("project/Song.mscz"));
    ASSERT_TRUE(m_store->load({ a }, FakeProjectFile().reader()));

    // The project opens, the clip's audio is offline, and the path was not looked at
    const AudioAsset* loaded = m_store->asset(a.id);
    ASSERT_TRUE(loaded);
    EXPECT_TRUE(loaded->offline);
    EXPECT_TRUE(loaded->blocked);
    EXPECT_TRUE(m_store->assetFilePath(a.id).empty());

    // An ordinary missing file is offline but not blocked
    AudioAsset b = a;
    b.id = std::string(40, 'c');
    b.relativePath = io::path_t("../Recordings/gone.wav");
    b.absolutePath = path("Recordings/gone.wav");
    ASSERT_TRUE(m_store->load({ b }, FakeProjectFile().reader()));
    EXPECT_TRUE(m_store->asset(b.id)->offline);
    EXPECT_FALSE(m_store->asset(b.id)->blocked);

    // A project that was itself opened from that share may look next to itself
    m_store->setProjectPath(io::path_t("//evil.example/share/project/Song.mscz"));
    ASSERT_TRUE(m_store->load({ a }, FakeProjectFile().reader()));
    EXPECT_TRUE(m_store->asset(a.id)->offline); // not there, of course
    EXPECT_FALSE(m_store->asset(a.id)->blocked);
}

TEST_F(Arrangement_AudioAssetStoreTests, LinkedAudioCanBeEmbeddedInACopyOnly)
{
    const ByteArray wav = makeSineWav(0.2, 48000, 1);
    const io::path_t recording = writeWav("Recordings/gtr.wav", wav);
    QDir().mkpath(path("project").toQString());
    m_store->setProjectPath(path("project/Song.mscz"));
    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    WriteOptions embed;
    embed.embedReferenced = true;

    FakeProjectFile copy;
    RetVal<AudioAssetList> table = m_store->save(copy.writer(), { a.val.id }, embed);
    ASSERT_TRUE(table.ret);

    // The copy holds the audio and says so ...
    ASSERT_EQ(table.val.size(), 1u);
    EXPECT_EQ(table.val[0].storage, AssetStorage::Embedded);
    EXPECT_EQ(table.val[0].fileInProject, embeddedFileName(a.val.id, "wav"));
    EXPECT_TRUE(table.val[0].absolutePath.empty());
    ASSERT_EQ(copy.entries.count(embeddedFileName(a.val.id, "wav")), 1u);
    EXPECT_EQ(copy.entries[embeddedFileName(a.val.id, "wav")], wav);

    // ... and the open project still links to the file
    EXPECT_EQ(m_store->asset(a.val.id)->storage, AssetStorage::Referenced);
    EXPECT_EQ(m_store->assetFilePath(a.val.id), recording);

    // The copy opens on its own
    ASSERT_TRUE(QFile::remove(recording.toQString()));
    AudioAssetStore other(m_iocCtx);
    ASSERT_TRUE(other.load(table.val, copy.reader()));
    EXPECT_FALSE(other.asset(a.val.id)->offline);
    EXPECT_EQ(readFile(other.assetFilePath(a.val.id)), wav);
}

TEST_F(Arrangement_AudioAssetStoreTests, ChangedLinkedAudioIsNotEmbeddedUnderTheOldId)
{
    const io::path_t recording = writeWav("take.wav", makeSineWav(0.2, 48000, 1));
    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    // Edited outside FontDaw after it was linked
    ASSERT_TRUE(writeFile(recording, makeSineWav(0.3, 48000, 1, 880.0)));

    WriteOptions embed;
    embed.embedReferenced = true;
    FakeProjectFile copy;
    RetVal<AudioAssetList> table = m_store->save(copy.writer(), { a.val.id }, embed);

    // The copy is still written; this asset keeps its link
    ASSERT_TRUE(table.ret);
    EXPECT_TRUE(copy.entries.empty());
    ASSERT_EQ(table.val.size(), 1u);
    EXPECT_EQ(table.val[0].storage, AssetStorage::Referenced);
}

TEST_F(Arrangement_AudioAssetStoreTests, LinksAreWrittenRelativeToWhereTheFileIsSaved)
{
    const io::path_t recording = writeWav("Recordings/gtr.wav", makeSineWav(0.1, 48000, 1));
    m_store->setProjectPath(path("project/Song.mscz"));
    RetVal<AudioAsset> a = m_store->importFileSync(recording, AssetStorage::Referenced);
    ASSERT_TRUE(a.ret);

    // "Save as" / "Save a copy" into another folder
    WriteOptions elsewhere;
    elsewhere.projectDir = path("backup/2026/november");
    FakeProjectFile copy;
    RetVal<AudioAssetList> table = m_store->save(copy.writer(), { a.val.id }, elsewhere);
    ASSERT_TRUE(table.ret);
    EXPECT_EQ(table.val[0].relativePath, io::path_t("../../../Recordings/gtr.wav"));
    EXPECT_EQ(table.val[0].absolutePath, recording);
}

TEST_F(Arrangement_AudioAssetStoreTests, ManyImportsGoThroughTheQueue)
{
    // More files than the queue has threads: all arrive, each exactly once
    const int count = 9;
    int resolved = 0;
    std::set<AssetId> ids;
    for (int i = 0; i < count; ++i) {
        const io::path_t file = writeWav(QString("take %1.wav").arg(i), makeSineWav(0.05 + 0.01 * i, 8000, 1));
        m_store->importFile(file, AssetStorage::Embedded).onResolve(this, [&](const RetVal<AudioAsset>& r) {
            EXPECT_TRUE(r.ret) << r.ret.text();
            ids.insert(r.val.id);
            ++resolved;
        });
    }

    ASSERT_TRUE(waitFor([&]() { return resolved == count; }));
    EXPECT_EQ(ids.size(), static_cast<size_t>(count));
    EXPECT_EQ(m_store->assets().size(), static_cast<size_t>(count));
    for (const AssetId& id : ids) {
        EXPECT_TRUE(QFileInfo(m_store->assetFilePath(id).toQString()).isFile());
    }
}

TEST_F(Arrangement_AudioAssetStoreTests, ClearCancelsQueuedImportsAndLeavesNoWriterBehind)
{
    const int count = 8;
    int cancelled = 0;
    int other = 0;
    for (int i = 0; i < count; ++i) {
        const io::path_t file = writeWav(QString("long %1.wav").arg(i), makeSineWav(1.0, 48000, 2, 200.0 + i));
        m_store->importFile(file, AssetStorage::Embedded).onResolve(this, [&](const RetVal<AudioAsset>& r) {
            if (r.ret.code() == static_cast<int>(Ret::Code::Cancel)) {
                ++cancelled;
            } else {
                ++other;
            }
        });
    }

    // The project closes while imports run and wait. clear() returns only when no import is
    // writing any more, so the session folder is really gone and stays gone.
    m_store->clear();
    const QStringList sessionsAfterClear = QDir(m_sessions.toQString()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    EXPECT_TRUE(sessionsAfterClear.isEmpty());

    ASSERT_TRUE(waitFor([&]() { return cancelled + other == count; }));
    EXPECT_EQ(cancelled, count);
    EXPECT_EQ(other, 0);
    EXPECT_TRUE(m_store->assets().empty());

    m_store->waitForBackgroundJobs();
    EXPECT_TRUE(QDir(m_sessions.toQString()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
}

TEST_F(Arrangement_AudioAssetStoreTests, AnEmbeddedFileTheDecoderRefusesIsUnreadableNotWaitedFor)
{
    // A project whose embedded entry is there but is not decodable audio (bytes start like a FLAC
    // file, the rest is damaged): the engine refuses it too, so it is marked unreadable and offline
    // (drawn as missing, and printing does not wait for its peaks)
    const ByteArray junk("fLaC this is not a real FLAC stream", 35);
    const AssetId id = sha1(junk);

    AudioAsset a;
    a.id = id;
    a.format = "flac";
    a.storage = AssetStorage::Embedded;
    a.fileInProject = embeddedFileName(id, "flac");
    a.sampleRate = 48000;
    a.channels = 2;
    a.frames = 4800;

    FakeProjectFile project;
    project.entries[a.fileInProject] = junk;
    ASSERT_TRUE(m_store->load({ a }, project.reader()));
    EXPECT_FALSE(m_store->asset(id)->offline);

    EXPECT_TRUE(waitFor([&]() { return m_store->asset(id)->unreadable; }));
    EXPECT_TRUE(m_store->asset(id)->offline);
    EXPECT_FALSE(m_store->peaks(id));

    // Saving keeps the bytes as they were: nothing is lost
    FakeProjectFile saved;
    ASSERT_TRUE(m_store->save(saved.writer(), { id }).ret);
    EXPECT_EQ(saved.entries[a.fileInProject], junk);
}

TEST_F(Arrangement_AudioAssetStoreTests, ImportsEveryFormatAndEmbedsTheOriginalBytes)
{
    for (const char* name : { "reference.flac", "reference.mp3", "reference.ogg", "reference.opus" }) {
        QFile source(QString(CODEC_TEST_DATA_DIR) + "/" + name);
        ASSERT_TRUE(source.open(QIODevice::ReadOnly)) << name;
        const QByteArray q = source.readAll();
        const ByteArray bytes(reinterpret_cast<const uint8_t*>(q.constData()), static_cast<size_t>(q.size()));

        const io::path_t file = writeWav(QString("in ") + name, bytes); // writes the bytes as given
        RetVal<AudioAsset> imported = m_store->importFileSync(file, AssetStorage::Embedded);
        ASSERT_TRUE(imported.ret) << name << ": " << imported.ret.text();
        EXPECT_EQ(imported.val.sampleRate, 48000u) << name;
        EXPECT_EQ(imported.val.channels, 2) << name;
        EXPECT_NEAR(static_cast<double>(imported.val.frames), 24000.0, 240.0) << name;
        EXPECT_TRUE(m_store->peaks(imported.val.id)) << name;

        // Embedded = the original bytes, never re-encoded; written back byte for byte on save
        EXPECT_EQ(readFile(m_store->assetFilePath(imported.val.id)), bytes) << name;
        FakeProjectFile project;
        RetVal<AudioAssetList> table = m_store->save(project.writer(), { imported.val.id });
        ASSERT_TRUE(table.ret) << name;
        ASSERT_EQ(table.val.size(), 1u);
        EXPECT_EQ(project.entries[embeddedFileName(imported.val.id, imported.val.format)], bytes) << name;
    }
}

TEST_F(Arrangement_AudioAssetStoreTests, AudioOutsideTheImportLimitsIsNotUnreadableInAProject)
{
    // Import takes mono and stereo, 8 to 192 kHz; the engine's decoder plays more (up to 8 channels,
    // 384 kHz). A project holding such audio is drawn and played: only a decoder refusal makes an
    // asset unreadable, so app and engine agree on it
    struct Case {
        ByteArray wav;
        int channels;
        uint64_t sampleRate;
    };
    for (const Case& c : { Case { makeWav(48000, 3, 16, false, std::vector<double>(3 * 4800, 0.25)), 3, 48000 },
                           Case { makeWav(4000, 1, 16, false, std::vector<double>(400, 0.25)), 1, 4000 } }) {
        const AssetId id = sha1(c.wav);
        AudioAsset a;
        a.id = id;
        a.format = "wav";
        a.storage = AssetStorage::Embedded;
        a.fileInProject = embeddedFileName(id, "wav");
        a.sampleRate = c.sampleRate;
        a.channels = c.channels;

        FakeProjectFile project;
        project.entries[a.fileInProject] = c.wav;
        ASSERT_TRUE(m_store->load({ a }, project.reader()));

        EXPECT_TRUE(waitFor([&]() { return m_store->peaks(id) != nullptr; })) << c.channels << " ch, " << c.sampleRate << " Hz";
        EXPECT_FALSE(m_store->asset(id)->unreadable);
        EXPECT_FALSE(m_store->asset(id)->offline);
        ASSERT_TRUE(m_store->peaks(id));
        EXPECT_EQ(m_store->peaks(id)->levels.front().channels.size(), static_cast<size_t>(c.channels));

        // The same file is still refused at import, with the reason
        RetVal<AudioAsset> imported = m_store->importFileSync(writeWav("limits.wav", c.wav), AssetStorage::Embedded);
        EXPECT_EQ(imported.ret.code(), static_cast<int>(Ret::Code::NotSupported));
    }
}

TEST_F(Arrangement_AudioAssetStoreTests, AnUnreadableLinkedFileIsNotEmbeddedWithTheRightReason)
{
    const ByteArray junk("fLaC not a real FLAC stream either", 34);
    const io::path_t file = writeWav("damaged.flac", junk);
    const AssetId id = sha1(junk);

    AudioAsset a;
    a.id = id;
    a.format = "flac";
    a.storage = AssetStorage::Referenced;
    a.absolutePath = file;
    ASSERT_TRUE(m_store->load({ a }, FakeProjectFile().reader()));
    ASSERT_TRUE(waitFor([&]() { return m_store->asset(id)->unreadable; }));

    const Ret ret = m_store->setStorage(id, AssetStorage::Embedded);
    EXPECT_FALSE(ret);
    EXPECT_NE(ret.text().find("cannot be read"), std::string::npos) << ret.text();
    EXPECT_EQ(ret.text().find("missing"), std::string::npos) << ret.text();
}
