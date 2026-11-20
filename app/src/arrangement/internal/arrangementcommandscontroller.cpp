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

#include "arrangementcommandscontroller.h"

#include "global/translation.h"
#include "project/inotationproject.h"

#include "../arrangementcommands.h"

#include "log.h"

using namespace muse;
using namespace mu::arrangement;

void ArrangementCommandsController::init()
{
    dispatcher()->onRequest(this, IMPORT_AUDIO_TRACK_COMMAND, [this]() {
        if (!commandsState()->commandState(IMPORT_AUDIO_TRACK_COMMAND).enabled) {
            return muse::make_ret(Ret::Code::NotSupported);
        }
        return importAudioTrack();
    });
}

Ret ArrangementCommandsController::importAudioTrack()
{
    // The formats the clip decoders read (FRAMEWORK_CHANGES F4); the decoder decides by content
    const std::vector<std::string> filter {
        muse::trc("arrangement", "Audio files") + " (*.wav *.wave *.flac *.mp3 *.ogg *.oga *.opus)",
        muse::trc("arrangement", "WAV audio files") + " (*.wav *.wave)",
        muse::trc("arrangement", "FLAC audio files") + " (*.flac)",
        muse::trc("arrangement", "MP3 audio files") + " (*.mp3)",
        muse::trc("arrangement", "Ogg audio files (Vorbis, Opus)") + " (*.ogg *.oga *.opus)",
        muse::trc("arrangement", "All files") + " (*)"
    };

    // Recordings usually sit next to the project
    const project::INotationProjectPtr project = projectSession()->project();
    const io::path_t dir = project && !project->path().empty() ? io::absoluteDirpath(project->path()) : io::path_t();

    const io::path_t path = interactive()->selectOpeningFileSync(muse::trc("arrangement", "Import audio track"), dir, filter);
    if (path.empty()) {
        return muse::make_ok(); // cancelled
    }

    arrangementController()->importAudioTrack(path).onResolve(this, [this](const Ret& ret) {
        if (ret || ret.code() == static_cast<int>(Ret::Code::Cancel)) {
            return;
        }

        LOGE() << "audio import failed: " << ret.toString();
        interactive()->error(muse::trc("arrangement", "The audio file could not be imported"), IInteractive::Text(ret.text()));
    });

    return muse::make_ok();
}
