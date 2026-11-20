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

#include "../arrangementtypes.h"

namespace mu::arrangement {
//! Everything arrangement.json holds.
struct ArrangementFileData {
    AudioAssetList assets;
    TrackList tracks;
};

//! arrangement.json (README, "Project file"). Pure functions, no state.
//!
//! Only the authoritative half of a clip position is written: startTick on Musical tracks,
//! startSecs on Absolute ones. Runtime-only fields (AudioAsset::offline, modifiedOnDisk) are
//! never written. Unknown keys are ignored, so a newer FontDaw can add optional fields without
//! a format bump; a newer `version` is refused with a message for the user.
//!
//! Versions: 1 holds WAV assets only; 2 adds FLAC, MP3 and OGG (Batch 2 workstream 04). A file is
//! written with the lowest version that describes it, so a project with WAV audio only stays readable
//! by builds from before version 2, and one with other formats is refused by them as "saved by a
//! newer version of FontDaw" rather than as damaged. (A FontDaw format of its own, FD-042, may
//! replace this numbering; nothing here assumes `.mscz`.)
//!
//! Precision: muse::JsonDocument writes numbers so that they read back as the same double (FD-028),
//! so seconds round trip exactly. SECS_EPSILON is the tolerance of edits (arrangement.cpp).
namespace arrangementjson {
inline constexpr int WAV_ONLY_VERSION = 1;
inline constexpr int FORMAT_VERSION = 2; // the newest this build reads and writes
inline constexpr double SECS_EPSILON = 1e-5;

//! The version write() puts in the file: WAV_ONLY_VERSION unless an asset is not WAV
int versionFor(const ArrangementFileData& data);

muse::ByteArray write(const ArrangementFileData& data);
muse::RetVal<ArrangementFileData> read(const muse::ByteArray& json);
}
}
