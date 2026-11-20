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

#include <memory>
#include <set>

#include "modularity/imoduleinterface.h"
#include "async/promise.h"
#include "async/channel.h"

#include "arrangementtypes.h"

namespace mu::arrangement {
struct WaveformPeaks; // waveformpeaks.h

//! The audio files of the current project: import, storage (embedded / referenced), waveform
//! overviews. FEATURES §4: "Import WAV, FLAC, MP3, OGG" and "Waveform display".
//! Main thread only; decoding and peak building run on a small queue of worker threads.
//!
//! Decoding: WAV, FLAC, MP3 and OGG (Vorbis, Opus) through the engine's clip decoders
//! (muse_audio_codecs, FRAMEWORK_CHANGES F4), so import, peaks and playback agree on every file.
//! A file that is there but cannot be decoded is marked `unreadable` (and offline).
//!
//! Embedded assets are kept in a per-project session folder while the project is open and
//! written into the project container on save; referenced assets stay where they are.
class IAudioAssetStore : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IAudioAssetStore)

public:
    virtual ~IAudioAssetStore() = default;

    //! Reads, hashes and decodes the file on a worker thread and resolves on the main thread.
    //! Importing identical bytes twice returns the existing asset (its storage is unchanged).
    //! Rejects unsupported or damaged files with a message fit for the user.
    //! The first overload uses the "When importing audio" preference (README decision 6).
    virtual muse::async::Promise<muse::RetVal<AudioAsset> > importFile(const muse::io::path_t& path) = 0;
    virtual muse::async::Promise<muse::RetVal<AudioAsset> > importFile(const muse::io::path_t& path, AssetStorage storage) = 0;

    virtual const AudioAsset* asset(const AssetId& id) const = 0;
    virtual AudioAssetList assets() const = 0;

    //! Embed a referenced asset (copies its bytes into the project) or reference an embedded one
    //! (only possible while an identical file still exists on disk where it was imported from).
    virtual muse::Ret setStorage(const AssetId& id, AssetStorage storage) = 0;

    //! Peaks for drawing. Null while they are still being built; peaksReady fires after.
    virtual std::shared_ptr<const WaveformPeaks> peaks(const AssetId& id) const = 0;
    virtual muse::async::Channel<AssetId> peaksReady() const = 0;

    //! Path of the asset's bytes on disk, for the engine to open: the session copy of an
    //! embedded asset, the resolved location of a referenced one. Empty when offline.
    virtual muse::io::path_t assetFilePath(const AssetId& id) const = 0;

    //! Drop assets no clip uses (after undo history no longer references them).
    virtual void removeUnused(const std::vector<AssetId>& stillUsed) = 0;

    //! The project file the relative paths of referenced assets are resolved against.
    //! Set before load(), and on "Save as" before save(); empty for a project never saved.
    virtual void setProjectPath(const muse::io::path_t& projectFilePath) = 0;

    //! Persistence (README, "Project file"). The asset table is part of arrangement.json, which
    //! IArrangement reads and writes; these handle the table's runtime state and the audio/ entries.
    //! load(): extracts embedded audio from the container, resolves referenced files (missing ones
    //! become offline, never an error), checks hashes, starts building peaks.
    //! The project file names the paths of referenced audio, so a path on a network share (UNC, or
    //! /net/<host> on macOS and Linux) is not contacted (the asset is offline and `blocked`), unless
    //! the project itself is on that share.
    virtual muse::Ret load(const AudioAssetList& table, const ReadProjectEntry& readEntry) = 0;
    //! save(): writes the embedded audio of the assets in `ids` under audio/ and returns their table
    //! for arrangement.json (relative paths recomputed for the current project path).
    //! With options.embedReferenced, linked audio is written too and listed as embedded; a linked
    //! file that is missing or changed cannot be embedded and keeps its link.
    virtual muse::RetVal<AudioAssetList> save(const WriteProjectEntry& writeEntry, const std::set<AssetId>& ids,
                                              const WriteOptions& options = {}) const = 0;
    virtual void clear() = 0;
};
}
