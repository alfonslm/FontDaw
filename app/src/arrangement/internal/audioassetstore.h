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

#include <map>
#include <memory>
#include <set>

#include "../iaudioassetstore.h"
#include "../iarrangementconfiguration.h"

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "waveformpeaks.h"
#include "workerqueue.h"

class QTemporaryDir;

namespace mu::arrangement {
class AudioAssetStore : public IAudioAssetStore, public muse::Contextable, public muse::async::Asyncable
{
    muse::GlobalInject<IArrangementConfiguration> configuration;

public:
    AudioAssetStore(const muse::modularity::ContextPtr& ctx);
    ~AudioAssetStore() override;

    muse::async::Promise<muse::RetVal<AudioAsset> > importFile(const muse::io::path_t& path) override;
    muse::async::Promise<muse::RetVal<AudioAsset> > importFile(const muse::io::path_t& path, AssetStorage storage) override;

    const AudioAsset* asset(const AssetId& id) const override;
    AudioAssetList assets() const override;

    muse::Ret setStorage(const AssetId& id, AssetStorage storage) override;

    std::shared_ptr<const WaveformPeaks> peaks(const AssetId& id) const override;
    muse::async::Channel<AssetId> peaksReady() const override;

    muse::io::path_t assetFilePath(const AssetId& id) const override;

    void removeUnused(const std::vector<AssetId>& stillUsed) override;

    void setProjectPath(const muse::io::path_t& projectFilePath) override;

    muse::Ret load(const AudioAssetList& table, const ReadProjectEntry& readEntry) override;
    muse::RetVal<AudioAssetList> save(const WriteProjectEntry& writeEntry, const std::set<AssetId>& ids,
                                      const WriteOptions& options = {}) const override;
    void clear() override;

    //! Blocking import on the calling (main) thread: decode + add. For tests and command-line use.
    muse::RetVal<AudioAsset> importFileSync(const muse::io::path_t& path, AssetStorage storage);

    //! Work done off the main thread for one import. Touches no store state.
    struct Prepared {
        AudioAsset asset; // id, name, format, sampleRate, channels, frames, originalPath
        std::shared_ptr<const WaveformPeaks> peaks;
    };
    //! Reads, hashes, validates and decodes the file, builds peaks, and if `copyToDir` is not
    //! empty writes the bytes to <copyToDir>/<id>.<format> unless that file exists already.
    static muse::RetVal<Prepared> prepareImport(const muse::io::path_t& path, const muse::io::path_t& copyToDir);

    static AssetId hashBytes(const muse::ByteArray& bytes);

    //! Which limits decodeFile() applies. The decoder's own (what the engine plays) always; at import
    //! also FontDaw's v1 limits (mono or stereo, 8 to 192 kHz), which only decide what a user can
    //! add now. Audio already in a project is judged by the decoder alone, like the engine does.
    enum class Limits {
        Decoder,
        Import,
    };

    //! What the clip decoders (the engine's, FRAMEWORK_CHANGES F4) make of a file. The Ret text is
    //! fit to show to the user; a refusal by the decoder itself is BadData, one by the import
    //! limits NotSupported.
    struct DecodedFile {
        std::string format;     // the asset format: "wav", "flac", "mp3" or "ogg"
        uint64_t sampleRate = 0;
        int channels = 0;
        uint64_t frames = 0;    // what the engine will play: the frames the decoder actually gave
        std::shared_ptr<const WaveformPeaks> peaks;
    };
    static muse::RetVal<DecodedFile> decodeFile(const muse::ByteArray& bytes, Limits limits);

    //! Import limits of v1 (decodeFile explains them to the user)
    static constexpr int MAX_CHANNELS = 2;
    static constexpr uint64_t MIN_SAMPLE_RATE = 8000;
    static constexpr uint64_t MAX_SAMPLE_RATE = 192000;

    //! True for \\server\share\... and //server/share/... (FD-031). Looks at the text only.
    static bool isNetworkPath(const muse::io::path_t& path);
    //! "//server/share" of a network path, lower case; empty for any other path.
    static std::string networkShare(const muse::io::path_t& path);

    //! Returns when no background job is waiting or running; their results are then delivered
    //! by the next round of the main thread's message loop. For tests.
    void waitForBackgroundJobs();

private:
    struct PeaksResult {
        AssetId id;
        std::shared_ptr<const WaveformPeaks> peaks;
        bool hashMatches = true;
        bool unreadable = false; // the file is there, but the decoder refuses it
    };

    struct PendingImport {
        muse::async::Promise<muse::RetVal<AudioAsset> >::Resolve resolve;
        AssetStorage storage = AssetStorage::Embedded;
        uint64_t generation = 0;
    };

    muse::RetVal<AudioAsset> addPrepared(const Prepared& prepared, AssetStorage storage);
    void buildPeaksInBackground(const AudioAsset& asset);
    void onImportPrepared(uint64_t jobId, const muse::RetVal<Prepared>& prepared);
    void onPeaksBuilt(uint64_t generation, const PeaksResult& result);

    muse::io::path_t sessionDir() const;
    muse::io::path_t sessionFilePath(const AudioAsset& asset) const;
    muse::io::path_t projectDir() const;
    muse::io::path_t relativeToProject(const muse::io::path_t& absolute) const;
    //! May the store look at this path, which a project file named?
    bool mayResolve(const muse::io::path_t& path) const;

    std::map<AssetId, AudioAsset> m_assets;
    std::map<AssetId, std::shared_ptr<const WaveformPeaks> > m_peaks;
    muse::async::Channel<AssetId> m_peaksReady;

    muse::io::path_t m_projectPath;
    mutable std::unique_ptr<QTemporaryDir> m_sessionDir;
    uint64_t m_generation = 0; // bumped by clear(): results of older background jobs are dropped

    // Worker -> main thread. Long-lived, so a result sent just before a job's thread ends is
    // still delivered; each worker sends on a copy (copies share one channel).
    muse::async::Channel<uint64_t, muse::RetVal<Prepared> > m_importPrepared; // job id, result
    muse::async::Channel<uint64_t, PeaksResult> m_peaksBuilt;                 // generation, result
    std::map<uint64_t, PendingImport> m_pendingImports;
    uint64_t m_lastJobId = 0;

    //! Declared last: destroyed first, so no job is running while the rest goes away.
    mutable WorkerQueue m_workers;
};
}
