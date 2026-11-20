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

#include "audioassetstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>

#include "global/translation.h"

#include "audio/engine/internal/codecs/audiodecoder.h"

#include "mappedfile.h"

#include "log.h"

using namespace mu::arrangement;
using namespace muse;
using namespace muse::async;

// Threads: the public API runs on the main thread. Decoding, hashing and peak building run on a
// small worker queue (WorkerQueue, FD-030). A job touches no member: it gets its inputs by value
// and sends its result over a member Channel subscribed on the main thread, so the result is
// handled on the main thread (kors channels deliver to the subscriber's thread). clear() drops the
// jobs that have not started and waits for the running ones before the session folder goes, and
// bumps m_generation so results already on their way are dropped.
//
// Memory (FD-030): files are not read into heap buffers where it can be avoided. FontDaw's own
// session copies are mapped; files elsewhere are read, at most as many at a time as the queue has
// threads.

namespace {
Ret fileError(const std::string& text)
{
    return Ret(static_cast<int>(Ret::Code::UnknownError), text);
}

Ret writeWholeFile(const io::path_t& path, const ByteArray& bytes)
{
    QSaveFile file(path.toQString());
    if (!file.open(QIODevice::WriteOnly)) {
        return fileError("cannot write " + path.toStdString());
    }
    const qint64 n = file.write(reinterpret_cast<const char*>(bytes.constData()), static_cast<qint64>(bytes.size()));
    if (n != static_cast<qint64>(bytes.size()) || !file.commit()) {
        return fileError("cannot write " + path.toStdString());
    }
    return make_ok();
}

bool fileExists(const io::path_t& path)
{
    return !path.empty() && QFileInfo(path.toQString()).isFile();
}

//! The asset format (the file extension of its bytes) for what the decoders recognise
std::string formatOf(const ByteArray& bytes)
{
    using muse::audio::codec::AudioFileFormat;
    switch (muse::audio::codec::detectFormat(bytes.constData(), bytes.size())) {
    case AudioFileFormat::Wav: return "wav";
    case AudioFileFormat::Flac: return "flac";
    case AudioFileFormat::Mp3: return "mp3";
    case AudioFileFormat::OggVorbis:
    case AudioFileFormat::OggOpus: return "ogg";
    case AudioFileFormat::Unknown: break;
    }
    return std::string();
}

//! Hash check and peaks for an asset already in the session folder or on disk.
struct PeaksJob {
    AssetId id;
    io::path_t path;
};
}

AudioAssetStore::AudioAssetStore(const modularity::ContextPtr& ctx)
    : Contextable(ctx)
{
    m_importPrepared.onReceive(this, [this](uint64_t jobId, const RetVal<Prepared>& prepared) {
        onImportPrepared(jobId, prepared);
    });
    m_peaksBuilt.onReceive(this, [this](uint64_t generation, const PeaksResult& result) {
        onPeaksBuilt(generation, result);
    });
}

AudioAssetStore::~AudioAssetStore()
{
    m_workers.cancelAndWait();
}

void AudioAssetStore::waitForBackgroundJobs()
{
    m_workers.waitUntilIdle();
}

bool AudioAssetStore::isNetworkPath(const io::path_t& path)
{
    return !networkShare(path).empty();
}

std::string AudioAssetStore::networkShare(const io::path_t& path)
{
    QString s = path.toQString();
    s.replace('\\', '/');

    // macOS (default automounter) and Linux autofs mount <host> over NFS as soon as /net/<host>/...
    // is looked at, so it is a network share too
    if (s.startsWith("/net/")) {
        const QStringList hostParts = s.mid(5).split('/', Qt::SkipEmptyParts);
        if (hostParts.isEmpty()) {
            return std::string();
        }
        return ("/net/" + hostParts.at(0)).toLower().toStdString();
    }

    if (!s.startsWith("//")) {
        return std::string();
    }

    // \\?\C:\... and \\.\... are local; \\?\UNC\server\share\... is not
    if (s.startsWith("//?/") || s.startsWith("//./")) {
        if (!s.mid(4).startsWith("UNC/", Qt::CaseInsensitive)) {
            return std::string();
        }
        s = "//" + s.mid(8);
    }

    const QStringList parts = s.mid(2).split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return std::string();
    }
    const QString share = "//" + parts.at(0) + (parts.size() > 1 ? "/" + parts.at(1) : QString());
    return share.toLower().toStdString();
}

bool AudioAssetStore::mayResolve(const io::path_t& path) const
{
    // A project file chooses the paths of its linked audio. Merely looking at a path on a
    // Windows share (or /net/<host> on macOS and Linux) contacts that server, and a Windows share
    // is offered the user's credentials, so a crafted project must not be able to make FontDaw do
    // that by being opened (FD-031). The share the project itself was opened from is fine: the
    // user is there already.
    const std::string share = networkShare(path);
    return share.empty() || share == networkShare(m_projectPath);
}

AssetId AudioAssetStore::hashBytes(const ByteArray& bytes)
{
    // SHA-1 is plenty: this is identity, not security.
    const QByteArrayView view(reinterpret_cast<const char*>(bytes.constData()), static_cast<qsizetype>(bytes.size()));
    const QByteArray hash = QCryptographicHash::hash(view, QCryptographicHash::Sha1);
    return hash.toHex().toStdString();
}

RetVal<AudioAssetStore::DecodedFile> AudioAssetStore::decodeFile(const ByteArray& bytes, Limits limits)
{
    using namespace muse::audio;

    DecodedFile file;
    file.format = formatOf(bytes);
    if (file.format.empty()) {
        return RetVal<DecodedFile>::make_ret(
            Ret(static_cast<int>(Ret::Code::NotSupported),
                trc("arrangement", "This audio format is not supported. FontDaw imports WAV, FLAC, MP3 and "
                                   "OGG (Vorbis or Opus) files.")));
    }

    const Ret damaged(static_cast<int>(Ret::Code::BadData),
                      trc("arrangement", "The audio file could not be read. It may be damaged, or be a kind of "
                                         "file that FontDaw does not support."));

    RetVal<codec::IAudioDecoderPtr> decoder = codec::makeDecoder(bytes.constData(), bytes.size());
    if (!decoder.ret) {
        LOGW() << "audio file refused by the decoder: " << decoder.ret.toString();
        return RetVal<DecodedFile>::make_ret(damaged);
    }

    const codec::AudioStreamInfo info = decoder.val->info();
    if (limits == Limits::Import && info.channels > MAX_CHANNELS) {
        return RetVal<DecodedFile>::make_ret(
            Ret(static_cast<int>(Ret::Code::NotSupported),
                trc("arrangement", "Only mono and stereo audio files are supported for now.")));
    }
    if (limits == Limits::Import && (info.sampleRate < MIN_SAMPLE_RATE || info.sampleRate > MAX_SAMPLE_RATE)) {
        return RetVal<DecodedFile>::make_ret(
            Ret(static_cast<int>(Ret::Code::NotSupported),
                trc("arrangement", "The sample rate of this file is not supported. "
                                   "Supported sample rates are 8 kHz to 192 kHz.")));
    }

    // One pass through the decoder, the same read loop as the engine's: the frame count is what plays
    RetVal<WaveformPeaks> peaks = WaveformPeaks::build(*decoder.val);
    if (!peaks.ret) {
        LOGW() << "audio file cannot be decoded: " << peaks.ret.toString();
        return RetVal<DecodedFile>::make_ret(damaged);
    }

    file.sampleRate = info.sampleRate;
    file.channels = info.channels;
    file.frames = peaks.val.frames;
    file.peaks = std::make_shared<const WaveformPeaks>(std::move(peaks.val));
    return RetVal<DecodedFile>::make_ok(file);
}

RetVal<AudioAssetStore::Prepared> AudioAssetStore::prepareImport(const io::path_t& path, const io::path_t& copyToDir)
{
    // The user chose this file; it is not ours, so it is read, not mapped (see MappedFile)
    MappedFile file;
    Ret ret = file.open(path, MappedFile::Mode::Read);
    if (!ret) {
        return RetVal<Prepared>::make_ret(ret);
    }
    const ByteArray bytes = file.bytes();

    RetVal<DecodedFile> decoded = decodeFile(bytes, Limits::Import);
    if (!decoded.ret) {
        return RetVal<Prepared>::make_ret(decoded.ret);
    }
    const std::string& format = decoded.val.format;

    Prepared p;
    p.asset.id = hashBytes(bytes);
    p.asset.name = io::completeBasename(path).toString();
    p.asset.format = format;
    p.asset.originalPath = io::absoluteFilePath(path);
    p.asset.sampleRate = decoded.val.sampleRate;
    p.asset.channels = decoded.val.channels;
    p.asset.frames = decoded.val.frames;
    p.peaks = decoded.val.peaks;

    if (!copyToDir.empty()) {
        // Never re-encoded: the original bytes, so lossless stays lossless and import stays fast.
        const io::path_t copy = copyToDir.appendingComponent(p.asset.id + "." + format);
        if (!fileExists(copy)) {
            ret = writeWholeFile(copy, bytes);
            if (!ret) {
                LOGE() << ret.text();
                return RetVal<Prepared>::make_ret(
                    fileError(trc("arrangement", "The audio could not be copied into the project's working folder.")));
            }
        }
    }

    return RetVal<Prepared>::make_ok(p);
}

Promise<RetVal<AudioAsset> > AudioAssetStore::importFile(const io::path_t& path)
{
    const AssetStorage storage = configuration() ? configuration()->defaultAssetStorage() : AssetStorage::Embedded;
    return importFile(path, storage);
}

Promise<RetVal<AudioAsset> > AudioAssetStore::importFile(const io::path_t& path, AssetStorage storage)
{
    return muse::async::make_promise<RetVal<AudioAsset> >([this, path, storage](auto resolve) {
        const io::path_t copyToDir = storage == AssetStorage::Embedded ? sessionDir() : io::path_t();
        const uint64_t jobId = ++m_lastJobId;
        m_pendingImports[jobId] = PendingImport { resolve, storage, m_generation };

        Channel<uint64_t, RetVal<Prepared> > done = m_importPrepared;
        m_workers.post([done, jobId, path, copyToDir]() mutable {
            done.send(jobId, prepareImport(path, copyToDir));
        });

        return Promise<RetVal<AudioAsset> >::dummy_result();
    }, PromiseType::AsyncByBody);
}

void AudioAssetStore::onImportPrepared(uint64_t jobId, const RetVal<Prepared>& prepared)
{
    auto it = m_pendingImports.find(jobId);
    if (it == m_pendingImports.end()) {
        return;
    }

    const PendingImport pending = it->second;
    m_pendingImports.erase(it);

    if (pending.generation != m_generation) {
        (void)pending.resolve(RetVal<AudioAsset>::make_ret(Ret::Code::Cancel));
    } else if (!prepared.ret) {
        (void)pending.resolve(RetVal<AudioAsset>::make_ret(prepared.ret));
    } else {
        (void)pending.resolve(addPrepared(prepared.val, pending.storage));
    }
}

RetVal<AudioAsset> AudioAssetStore::importFileSync(const io::path_t& path, AssetStorage storage)
{
    const io::path_t copyToDir = storage == AssetStorage::Embedded ? sessionDir() : io::path_t();
    RetVal<Prepared> prepared = prepareImport(path, copyToDir);
    if (!prepared.ret) {
        return RetVal<AudioAsset>::make_ret(prepared.ret);
    }
    return addPrepared(prepared.val, storage);
}

RetVal<AudioAsset> AudioAssetStore::addPrepared(const Prepared& prepared, AssetStorage storage)
{
    const AssetId& id = prepared.asset.id;

    auto existing = m_assets.find(id);
    if (existing != m_assets.end()) {
        // Same bytes: one asset. Its storage stays as it is (setStorage changes it explicitly).
        // If it was offline, the file just imported brings it back.
        AudioAsset& a = existing->second;
        if (a.offline && a.storage == AssetStorage::Embedded && fileExists(sessionFilePath(a))) {
            // Embedded audio that was missing from the project: the import just wrote its bytes.
            a.offline = false;
        } else if (a.offline) {
            // Missing audio found as a file on disk (e.g. "locate"): link to it.
            a.storage = AssetStorage::Referenced;
            a.fileInProject.clear();
            a.absolutePath = prepared.asset.originalPath;
            a.relativePath = relativeToProject(a.absolutePath);
            a.offline = false;
            a.modifiedOnDisk = false;
        }
        if (!m_peaks.count(id) && prepared.peaks) {
            m_peaks[id] = prepared.peaks;
            m_peaksReady.send(id);
        }
        return RetVal<AudioAsset>::make_ok(a);
    }

    AudioAsset a = prepared.asset;
    a.storage = storage;
    if (storage == AssetStorage::Embedded) {
        a.fileInProject = embeddedFileName(a.id, a.format);
    } else {
        a.absolutePath = a.originalPath;
        a.relativePath = relativeToProject(a.absolutePath);
    }

    m_assets[id] = a;
    m_peaks[id] = prepared.peaks;
    m_peaksReady.send(id);

    return RetVal<AudioAsset>::make_ok(a);
}

const AudioAsset* AudioAssetStore::asset(const AssetId& id) const
{
    auto it = m_assets.find(id);
    return it != m_assets.end() ? &it->second : nullptr;
}

AudioAssetList AudioAssetStore::assets() const
{
    AudioAssetList list;
    list.reserve(m_assets.size());
    for (const auto& [id, a] : m_assets) {
        list.push_back(a);
    }
    return list;
}

Ret AudioAssetStore::setStorage(const AssetId& id, AssetStorage storage)
{
    auto it = m_assets.find(id);
    if (it == m_assets.end()) {
        return Ret(static_cast<int>(Ret::Code::BadArgs), "unknown asset " + id);
    }

    AudioAsset& a = it->second;
    if (a.storage == storage) {
        return make_ok();
    }

    // TODO(FontDaw): these copy and hash on the main thread; move to a worker when the panel's
    // context menu (step 7) offers them for large files.
    if (storage == AssetStorage::Embedded) {
        if (a.unreadable) {
            // Checked before `offline`, which an unreadable file also sets
            return Ret(static_cast<int>(Ret::Code::BadData),
                       trc("arrangement", "The linked audio file cannot be read (it may be damaged), "
                                          "so it cannot be embedded."));
        }
        if (a.offline || a.modifiedOnDisk) {
            return Ret(static_cast<int>(Ret::Code::BadData),
                       trc("arrangement", "The linked audio file is missing or was changed outside FontDaw, "
                                          "so it cannot be embedded."));
        }
        const io::path_t target = sessionFilePath(a);
        if (!fileExists(target)) {
            MappedFile file;
            Ret ret = file.open(a.absolutePath, MappedFile::Mode::Read);
            if (!ret) {
                return ret;
            }
            if (hashBytes(file.bytes()) != a.id) {
                // Changed since it was loaded or imported: embedding it would store other audio
                // under this id.
                a.modifiedOnDisk = true;
                return Ret(static_cast<int>(Ret::Code::BadData),
                           trc("arrangement", "The linked audio file was changed outside FontDaw since it was "
                                              "added, so it cannot be embedded."));
            }
            ret = writeWholeFile(target, file.bytes());
            if (!ret) {
                return ret;
            }
        }
        a.storage = AssetStorage::Embedded;
        a.fileInProject = embeddedFileName(a.id, a.format);
        a.relativePath = io::path_t();
        a.absolutePath = io::path_t();
        return make_ok();
    }

    // Embedded -> Referenced: link to a file on disk with exactly these bytes.
    for (const io::path_t& candidate : { a.originalPath, a.absolutePath }) {
        // originalPath may come from the project file: same rule as for linked audio (FD-031)
        if (!mayResolve(candidate) || !fileExists(candidate)) {
            continue;
        }
        MappedFile file;
        if (file.open(candidate, MappedFile::Mode::Read) && hashBytes(file.bytes()) == a.id) {
            a.storage = AssetStorage::Referenced;
            a.absolutePath = io::absoluteFilePath(candidate);
            a.relativePath = relativeToProject(a.absolutePath);
            a.fileInProject.clear();
            return make_ok();
        }
    }

    return Ret(static_cast<int>(Ret::Code::BadData),
               trc("arrangement", "The original audio file is no longer on disk unchanged, so the project "
                                  "cannot link to it. The audio stays embedded."));
}

std::shared_ptr<const WaveformPeaks> AudioAssetStore::peaks(const AssetId& id) const
{
    auto it = m_peaks.find(id);
    return it != m_peaks.end() ? it->second : nullptr;
}

Channel<AssetId> AudioAssetStore::peaksReady() const
{
    return m_peaksReady;
}

io::path_t AudioAssetStore::assetFilePath(const AssetId& id) const
{
    const AudioAsset* a = asset(id);
    if (!a || a->offline) {
        return io::path_t();
    }
    return a->storage == AssetStorage::Embedded ? sessionFilePath(*a) : a->absolutePath;
}

// Batch 2 03a, FD-015 check 3, the retention rule: an asset stays in the store for the whole
// session, so undo and redo can always bring its clips back, also after a save. save() writes only
// the assets that clips use, so an undone import adds nothing to the file. removeUnused is not
// called: the store is cleared with the project (clear()), and dropping an asset earlier would
// break a redo. It stays for a later bounded history.
void AudioAssetStore::removeUnused(const std::vector<AssetId>& stillUsed)
{
    const std::set<AssetId> used(stillUsed.begin(), stillUsed.end());
    for (auto it = m_assets.begin(); it != m_assets.end();) {
        if (used.count(it->first)) {
            ++it;
            continue;
        }
        if (it->second.storage == AssetStorage::Embedded) {
            QFile::remove(sessionFilePath(it->second).toQString());
        }
        m_peaks.erase(it->first);
        it = m_assets.erase(it);
    }
}

void AudioAssetStore::setProjectPath(const io::path_t& projectFilePath)
{
    m_projectPath = projectFilePath;
}

Ret AudioAssetStore::load(const AudioAssetList& table, const ReadProjectEntry& readEntry)
{
    clear();

    // The table comes from a project file: ids and formats become file names, so check them
    // here too (the store is its own interface; arrangementjson checks them as well).
    for (const AudioAsset& a : table) {
        if (!isValidAssetId(a.id) || !isSupportedAssetFormat(a.format)
            || (a.storage == AssetStorage::Embedded && a.fileInProject != embeddedFileName(a.id, a.format))) {
            LOGE() << "refusing an audio asset with an unsafe id, format or file name";
            return Ret(static_cast<int>(Ret::Code::BadData),
                       trc("arrangement", "The arrangement data in this project is damaged and could not be read."));
        }
    }

    for (AudioAsset a : table) {
        a.offline = false;
        a.modifiedOnDisk = false;
        a.blocked = false;
        a.unreadable = false;

        if (a.storage == AssetStorage::Embedded) {
            // The container is only readable now, so the bytes are extracted while loading.
            RetVal<ByteArray> bytes = readEntry ? readEntry(a.fileInProject) : RetVal<ByteArray>::make_ok(ByteArray());
            if (!bytes.ret) {
                return bytes.ret;
            }
            if (bytes.val.empty()) {
                LOGE() << "embedded audio missing from the project: " << a.fileInProject;
                a.offline = true;
            } else {
                Ret ret = writeWholeFile(sessionFilePath(a), bytes.val);
                if (!ret) {
                    LOGE() << ret.text();
                    a.offline = true;
                }
            }
        } else {
            // Relative path first (the project folder moved with its recordings), then absolute.
            // A project file chooses these paths: see mayResolve() for the ones not looked at.
            std::vector<io::path_t> candidates;
            if (!projectDir().empty() && !a.relativePath.empty()) {
                candidates.push_back(io::path_t(QDir::cleanPath(QDir(projectDir().toQString())
                                                                .filePath(a.relativePath.toQString()))));
            }
            if (!a.absolutePath.empty()) {
                candidates.push_back(a.absolutePath);
            }

            io::path_t found;
            for (const io::path_t& candidate : candidates) {
                if (!mayResolve(candidate)) {
                    LOGW() << "linked audio on a network share is not opened automatically: " << candidate;
                    a.blocked = true;
                    continue;
                }
                if (fileExists(candidate)) {
                    found = candidate;
                    break;
                }
            }

            if (found.empty()) {
                LOGW() << "linked audio not found, clips stay offline: " << a.absolutePath;
                a.offline = true; // the project still opens; never delete clips for this
            } else {
                a.absolutePath = found;
                a.blocked = false;
            }
        }

        m_assets[a.id] = a;
        if (!a.offline) {
            buildPeaksInBackground(a);
        }
    }

    return make_ok();
}

RetVal<AudioAssetList> AudioAssetStore::save(const WriteProjectEntry& writeEntry, const std::set<AssetId>& ids,
                                             const WriteOptions& options) const
{
    AudioAssetList table;

    // TODO(FontDaw): the container takes whole entries, so a project with much embedded audio is
    // built in memory on every save, autosave included (FD-048). Unchanged entries could be
    // copied from the old container untouched, and stored without compression (README, "Large projects").
    auto writeAudio = [&writeEntry](const AudioAsset& a, const io::path_t& from, MappedFile::Mode mode,
                                    bool checkHash) -> Ret {
        MappedFile file;
        Ret ret = file.open(from, mode);
        if (!ret) {
            return ret;
        }
        if (checkHash && hashBytes(file.bytes()) != a.id) {
            return Ret(static_cast<int>(Ret::Code::BadData), "audio file changed on disk");
        }
        return writeEntry(embeddedFileName(a.id, a.format), file.bytes());
    };

    for (const auto& [id, original] : m_assets) {
        if (!ids.count(id)) {
            continue;
        }

        AudioAsset a = original;
        if (a.storage == AssetStorage::Embedded) {
            const io::path_t sessionFile = sessionFilePath(a);
            if (fileExists(sessionFile)) {
                Ret ret = writeAudio(a, sessionFile, MappedFile::Mode::Map, false /*checkHash*/);
                if (!ret) {
                    return RetVal<AudioAssetList>::make_ret(ret);
                }
            } else if (a.offline) {
                // Its bytes were already missing when the project was loaded: keep the table entry
                // so the clips survive; there is nothing to write.
                LOGW() << "no audio to write for embedded asset " << id;
            } else {
                // The session copy vanished while the project was open. Saving now would rewrite
                // the project without this audio: refuse instead.
                LOGE() << "session copy of embedded audio is missing: " << sessionFile;
                return RetVal<AudioAssetList>::make_ret(
                    fileError(trc("arrangement", "Some embedded audio could not be found in FontDaw's working folder, "
                                                 "so the project was not saved.")));
            }
        } else {
            bool embedded = false;
            if (options.embedReferenced && !a.offline && !a.modifiedOnDisk) {
                // Embed in the copy: only audio that is still exactly what the project used
                Ret ret = writeAudio(a, a.absolutePath, MappedFile::Mode::Read, true /*checkHash*/);
                if (ret) {
                    embedded = true;
                } else {
                    LOGW() << "linked audio could not be embedded, the copy keeps its link: " << a.absolutePath
                           << " (" << ret.text() << ")";
                }
            }

            if (embedded) {
                a.storage = AssetStorage::Embedded;
                a.fileInProject = embeddedFileName(a.id, a.format);
                a.relativePath = io::path_t();
                a.absolutePath = io::path_t();
            } else if (!options.projectDir.empty()) {
                a.relativePath = io::path_t(QDir(options.projectDir.toQString()).relativeFilePath(a.absolutePath.toQString()));
            } else {
                a.relativePath = relativeToProject(a.absolutePath);
            }
        }
        table.push_back(a);
    }

    return RetVal<AudioAssetList>::make_ok(table);
}

void AudioAssetStore::clear()
{
    ++m_generation;

    // No import may still be writing into the session folder when it is removed (FD-030)
    m_workers.cancelAndWait();

    // Imports that were waiting never ran, and the results of those that did are for a project
    // that is gone
    const std::map<uint64_t, PendingImport> pending = std::move(m_pendingImports);
    m_pendingImports.clear();
    for (const auto& [jobId, import] : pending) {
        (void)import.resolve(RetVal<AudioAsset>::make_ret(Ret::Code::Cancel));
    }

    m_assets.clear();
    m_peaks.clear();
    m_sessionDir.reset(); // removes the session copies
}

void AudioAssetStore::buildPeaksInBackground(const AudioAsset& asset)
{
    PeaksJob job { asset.id, asset.storage == AssetStorage::Embedded ? sessionFilePath(asset) : asset.absolutePath };
    const uint64_t generation = m_generation;

    Channel<uint64_t, PeaksResult> done = m_peaksBuilt;
    // Our own session copy can be mapped; a linked file is not ours (see MappedFile)
    const MappedFile::Mode mode = asset.storage == AssetStorage::Embedded ? MappedFile::Mode::Map : MappedFile::Mode::Read;

    m_workers.post([done, job, generation, mode]() mutable {
        PeaksResult result;
        result.id = job.id;

        MappedFile file;
        if (file.open(job.path, mode)) {
            const ByteArray bytes = file.bytes();
            result.hashMatches = hashBytes(bytes) == job.id;
            // The engine's decoder and only its limits: a file it refuses stays silent, so it is
            // marked unreadable; one it plays is drawn, even where import would not take it now
            RetVal<DecodedFile> decoded = decodeFile(bytes, Limits::Decoder);
            if (decoded.ret) {
                result.peaks = decoded.val.peaks;
            } else {
                result.unreadable = true;
            }
        }
        done.send(generation, result);
    });
}

void AudioAssetStore::onPeaksBuilt(uint64_t generation, const PeaksResult& result)
{
    if (generation != m_generation) {
        return;
    }

    auto it = m_assets.find(result.id);
    if (it == m_assets.end()) {
        return;
    }

    if (!result.hashMatches) {
        // Integrity (README): the recording was edited outside FontDaw. The panel (step 7) offers
        // "accept the new contents" or "relink"; until then the asset plays what is on disk.
        LOGW() << "audio file changed since it was added to the project: " << it->second.absolutePath;
        it->second.modifiedOnDisk = true;
    }

    if (result.unreadable) {
        // Shown as missing (dashed, no waveform) and not waited for before printing; the engine
        // refuses the same file, so its clips are silent either way
        LOGW() << "audio file cannot be decoded, its clips stay silent: " << result.id;
        it->second.unreadable = true;
        it->second.offline = true;
        m_peaksReady.send(result.id); // views repaint the clip as missing
        return;
    }

    if (result.peaks) {
        m_peaks[result.id] = result.peaks;
        m_peaksReady.send(result.id);
    }
}

io::path_t AudioAssetStore::sessionDir() const
{
    if (!m_sessionDir) {
        io::path_t base = configuration() ? configuration()->audioSessionsPath() : io::path_t();
        if (base.empty()) {
            base = QDir::tempPath();
        }
        QDir().mkpath(base.toQString());
        m_sessionDir = std::make_unique<QTemporaryDir>(base.toQString() + "/session-XXXXXX");
        if (!m_sessionDir->isValid()) {
            LOGE() << "cannot create the audio session folder in " << base;
        }
    }
    return io::path_t(m_sessionDir->path());
}

io::path_t AudioAssetStore::sessionFilePath(const AudioAsset& asset) const
{
    return sessionDir().appendingComponent(asset.id + "." + asset.format);
}

io::path_t AudioAssetStore::projectDir() const
{
    return m_projectPath.empty() ? io::path_t() : io::absoluteDirpath(m_projectPath);
}

io::path_t AudioAssetStore::relativeToProject(const io::path_t& absolute) const
{
    if (projectDir().empty() || absolute.empty()) {
        return io::path_t();
    }
    return io::path_t(QDir(projectDir().toQString()).relativeFilePath(absolute.toQString()));
}
