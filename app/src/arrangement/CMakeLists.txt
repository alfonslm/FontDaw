# SPDX-License-Identifier: GPL-3.0-only
#
# FontDaw
# The notation-first DAW
#
# Copyright (C) 2026 FontDaw contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Audio tracks, clips and assets next to the score (README.md). Built when
# MUE_BUILD_ARRANGEMENT_MODULE is ON (top-level CMakeLists.txt). Compare src/playback/CMakeLists.txt.
#
# Not built yet (later steps, still drafts): automationtypes.h (build order 6),
# qml/MuseScore/Arrangement (step 7).

muse_create_module(arrangement)

target_sources(arrangement PRIVATE
    arrangementmodule.cpp
    arrangementmodule.h
    arrangementcommands.h
    arrangementtypes.h
    iarrangement.h
    iarrangementconfiguration.h
    iarrangementcontroller.h
    iarrangementundo.h
    iaudioassetstore.h

    internal/arrangement.cpp
    internal/arrangement.h
    internal/arrangementcommandscontroller.cpp
    internal/arrangementcommandscontroller.h
    internal/arrangementcommandsregister.cpp
    internal/arrangementcommandsregister.h
    internal/arrangementcommandsstate.cpp
    internal/arrangementcommandsstate.h
    internal/arrangementconfiguration.cpp
    internal/arrangementconfiguration.h
    internal/arrangementcontroller.cpp
    internal/arrangementcontroller.h
    internal/arrangementjson.cpp
    internal/arrangementjson.h
    internal/arrangementundo.cpp
    internal/arrangementundo.h
    internal/audiopart.cpp
    internal/barshift.cpp
    internal/barshift.h
    internal/audiopart.h
    internal/audiostaffpainter.cpp
    internal/audiostaffpainter.h
    internal/audioassetstore.cpp
    internal/audioassetstore.h
    internal/clipstates.cpp
    internal/clipstates.h
    internal/mappedfile.cpp
    internal/mappedfile.h
    internal/scorepositions.cpp
    internal/scorepositions.h
    internal/waveformpeaks.cpp
    internal/waveformpeaks.h
    internal/workerqueue.cpp
    internal/workerqueue.h
)

# The engine's clip decoders (muse/framework/audio/engine/internal/codecs): import checks and
# waveform peaks decode with the same code as playback (FRAMEWORK_CHANGES F4, FD-050)
target_link_libraries(arrangement PRIVATE muse_audio_codecs)

if (MUE_BUILD_ARRANGEMENT_TESTS)
    add_subdirectory(tests)
endif()
