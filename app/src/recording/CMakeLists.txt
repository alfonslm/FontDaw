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
# (option MUE_BUILD_RECORDING_MODULE, add_subdirectory, link, register RecordingModule).
# Depends on: arrangement.

muse_create_module(recording)

target_sources(recording PRIVATE
    recordingmodule.cpp
    recordingmodule.h
    recordingtypes.h
    irecordcontroller.h

    internal/recordcontroller.cpp
    internal/recordcontroller.h
    internal/takewriter.h
)
