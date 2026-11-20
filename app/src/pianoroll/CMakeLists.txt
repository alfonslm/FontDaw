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

# DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

# TODO(FontDaw FEATURES build order): draft; enable exactly like src/arrangement/CMakeLists.txt
# (option MUE_BUILD_PIANOROLL_MODULE, add_subdirectory, link, register PianoRollModule).
# Depends on: notation, playback, arrangement (shared timeline helpers).

muse_create_module(pianoroll)

target_sources(pianoroll PRIVATE
    pianorollmodule.cpp
    pianorollmodule.h
    pianorolltypes.h
)

# TODO(FontDaw): add_subdirectory(qml/MuseScore/PianoRoll) with a qt_add_qml_module, as in
# src/arrangement/qml/MuseScore/Arrangement/CMakeLists.txt.
