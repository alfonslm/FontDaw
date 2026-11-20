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

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "global/types/bytearray.h"
#include "global/types/id.h"
#include "global/types/ret.h"
#include "global/types/retval.h"
#include "global/types/secs.h"
#include "global/types/string.h"
#include "global/io/path.h"

#include "engraving/types/fraction.h" // header-only; no link dependency on engraving

namespace mu::arrangement {
using secs_t = muse::secs_t;

// Ids are FontDaw's own and are stored in the project file. They are not engine ids:
// muse::audio::TrackId is handed out per session by the engine and must never be saved, so the
// engine id of a track lives in the controller (C2), not in this model.
using ArrTrackId = uint64_t;
using ClipId = uint64_t;
using AssetId = std::string; // lower-case hex SHA-1 of the audio file bytes

//! Asset ids and formats become file names (<session>/<id>.<format>, audio/<id>.<format>), and
//! they come from project files, which may be crafted: only these shapes are accepted.
inline bool isValidAssetId(const AssetId& id)
{
    if (id.size() != 40) {
        return false;
    }
    for (char c : id) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

inline bool isSupportedAssetFormat(const std::string& format)
{
    // A fixed list: formats become file names (embeddedFileName). "ogg" is Vorbis or Opus: the
    // decoder tells them apart by content (FRAMEWORK_CHANGES F4, Batch 2 workstream 04)
    return format == "wav" || format == "flac" || format == "mp3" || format == "ogg";
}

inline std::string embeddedFileName(const AssetId& id, const std::string& format)
{
    return "audio/" + id + "." + format;
}

inline constexpr ArrTrackId INVALID_TRACK = 0;
inline constexpr ClipId INVALID_CLIP = 0;

enum class TrackKind {
    Audio, // owns audio clips
    // Notation rows (one per score part) are not part of the model: the score owns them, and
    // where they sit among audio tracks is a display question for the arrangement panel (step 7).
    // TODO(FontDaw FEATURES §2, tier 2): Midi (not written as notation), Folder, Bus
};

//! How a track's clips are anchored on the timeline. See README, decision 2.
enum class TimeBase {
    Musical,  // clip start is a tick: stays on its bar when tempo changes (default, FD-015)
    Absolute, // clip start is seconds: stays at its time when tempo changes
};

enum class FadeCurve {
    Linear,
    EqualPower,
};

struct Fade {
    secs_t length = 0.0;
    FadeCurve curve = FadeCurve::Linear;

    bool operator==(const Fade& o) const { return length == o.length && curve == o.curve; }
};

//! Where an asset's bytes live (README, decision 6).
enum class AssetStorage {
    Embedded,   // inside the project container, under audio/
    Referenced, // a file on disk outside the project; the project stores its path
};

//! One imported audio file. Its content is immutable once created (README, decision 4);
//! only where it is stored may change (embed / unembed).
struct AudioAsset {
    AssetId id;                     // content hash: also used to relink moved references
    muse::String name;              // shown to the user; defaults to the original file name
    std::string format;             // file extension of the original bytes, e.g. "wav"
    AssetStorage storage = AssetStorage::Embedded;
    std::string fileInProject;      // Embedded: "audio/<id>.<format>" inside the project container
    muse::io::path_t relativePath;  // Referenced: path relative to the project file's folder
    muse::io::path_t absolutePath;  // Referenced: absolute path, fallback when the relative one fails
    muse::io::path_t originalPath;  // where it was imported from; informational only
    uint64_t sampleRate = 0;
    int channels = 0;
    uint64_t frames = 0;

    // Runtime only, never saved:
    bool offline = false;           // referenced file not found; clips stay, silent
    bool modifiedOnDisk = false;    // referenced file found but its hash no longer matches `id`
    bool blocked = false;           // referenced path is on a network share the project file named:
                                    // not contacted (FD-031). Implies offline.
    bool unreadable = false;        // the file is there, but the decoder refuses it (damaged, or a
                                    // kind of the format not supported). Implies offline.

    secs_t duration() const
    {
        return sampleRate ? secs_t(static_cast<double>(frames) / static_cast<double>(sampleRate)) : secs_t(0.0);
    }
};

using AudioAssetList = std::vector<AudioAsset>;

//! A window onto an asset, placed on a track.
struct AudioClip {
    ClipId id = INVALID_CLIP;
    AssetId asset;
    muse::String name;

    // Position on the timeline. Which one is authoritative depends on the track's TimeBase;
    // the other is derived from the time map and is not saved.
    mu::engraving::Fraction startTick = mu::engraving::Fraction(0, 1);
    secs_t startSecs = 0.0;

    secs_t sourceOffset = 0.0; // where in the asset the clip starts playing
    secs_t length = 0.0;       // source seconds; never longer than the asset allows

    float gainDb = 0.f;
    Fade fadeIn;
    Fade fadeOut;
    bool muted = false;

    // Beat matching (tier 1, FEATURES §4; README decision 2). 0 = not beat-matched: the clip
    // plays at its original speed. Otherwise the tempo the audio was played at; the engine
    // stretches it by projectTempo / sourceTempoBpm with the pitch unchanged. With a
    // stretch, `length` and `sourceOffset` stay in SOURCE seconds and the timeline length
    // is derived; this keeps trims valid when the tempo changes.
    // Stored and round-tripped now; the timeline length ignores it until step 9 (F7).
    double sourceTempoBpm = 0.0;

    //! Length on the timeline. Equal to `length` until beat matching lands (step 9).
    secs_t timelineLength() const { return length; }
    secs_t endSecs() const { return startSecs + timelineLength(); }

    // TODO(FontDaw FEATURES §4, tier 2): follow tempo changes inside a clip, pitchShiftCents, reversed
};

struct Track {
    ArrTrackId id = INVALID_TRACK;
    TrackKind kind = TrackKind::Audio;
    muse::String name;
    TimeBase timeBase = TimeBase::Musical;

    std::vector<AudioClip> clips; // kept sorted by start, non-overlapping (README, decision 3)

    //! The score part that shows this track: an audio staff (internal/audiopart.h, Batch 2 S1).
    //! Invalid for a track made before S1, until the project is opened again.
    muse::ID partId;

    // TODO(FontDaw FEATURES §8, build order 6): automation lanes (automationtypes.h) join here
    // with the clip-list engine API.

    // Recording state (src/recording). Saved, so arming survives reopening a project.
    bool recordArmed = false;
    bool inputMonitoring = false;
    int inputChannel = 0; // first channel of the device input pair; see FRAMEWORK_CHANGES F2
};

using TrackList = std::vector<Track>;

//! Converts between musical and real time. The arrangement gets one from the score's tempo map
//! (C2); until then, and in tests, a constant tempo is used.
struct TimeMap {
    std::function<secs_t(const mu::engraving::Fraction&)> tickToSecs;
    std::function<mu::engraving::Fraction(secs_t)> secsToTick;

    //! Constant tempo, in quarter notes per minute. Fractions are whole notes (4 quarters).
    static TimeMap constantTempo(double quarterBpm);
};

//! Batch 2 03a: whether two track lists hold the same undo history state. Compares the track
//! order and, per track, id, kind, name, timeBase and clips; per clip everything but the derived
//! half of its position (startSecs on a Musical track, startTick on an Absolute one). Ignores the
//! live fields recordArmed, inputMonitoring, inputChannel and partId. Not operator==, on purpose.
bool sameHistory(const TrackList& a, const TrackList& b);

//! FD-151: a clip's new start tick, for IArrangement::setClipStartTicks
struct ClipStartTick {
    ArrTrackId track = INVALID_TRACK;
    ClipId clip = INVALID_CLIP;
    mu::engraving::Fraction tick;
};

//! What changed, for UI models and the controller. Keep it coarse; refine if redraws get slow.
enum class ChangeKind {
    TracksReset,   // everything; reload
    TrackAdded,
    TrackRemoved,
    TrackChanged,  // name, order, arming
    ClipsChanged,  // any clip edit on the given track
    Restored,      // undo or redo put back a whole earlier state (Batch 2 03a): like TracksReset
                   // for views, but an edit for the controller's engine sync and not a load
};

struct Change {
    ChangeKind kind = ChangeKind::TracksReset;
    ArrTrackId trackId = INVALID_TRACK;
};

//! Access to the extra entries of the project container (arrangement.json, audio/*).
//! C2 binds these to engraving::MscReader / MscWriter; tests bind them to a map.
//! A reader returns an empty ByteArray with an ok Ret for an entry that does not exist.
//! A writer must take what it needs from `data` before it returns: for audio it can be a view
//! onto a mapped file that is closed right after the call.
using ReadProjectEntry = std::function<muse::RetVal<muse::ByteArray>(const std::string& name)>;
using WriteProjectEntry = std::function<muse::Ret (const std::string& name, const muse::ByteArray& data)>;

inline constexpr const char* ARRANGEMENT_JSON_NAME = "arrangement.json";
inline constexpr const char* AUDIO_FOLDER_NAME = "audio";

//! How the arrangement is written into a project container (IArrangement::write).
struct WriteOptions {
    //! Also write linked audio into the container and list it as embedded ("embed in the copy",
    //! README decision 6). The open project keeps its links.
    bool embedReferenced = false;
    //! The folder the written project file will be in: links are stored relative to it.
    //! Empty: the folder of the open project (IAudioAssetStore::setProjectPath).
    muse::io::path_t projectDir;
};

//! What to do with linked audio when a copy of the project is saved for someone else.
enum class LinkedAudioInCopies {
    Ask,
    Embed,
    KeepLinks,
};
}
