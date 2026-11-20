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

# Menus that open at the pointer (README.md): the pointer menu and its command index, the ring
# menu. Built when MUE_BUILD_POINTERTOOLS_MODULE is ON (top-level CMakeLists.txt). Compare
# src/arrangement/CMakeLists.txt.

muse_create_module(pointertools)

target_sources(pointertools PRIVATE
    pointertoolsmodule.cpp
    pointertoolsmodule.h
    pointertoolscommands.h
    pointertoolstypes.h
    icommandindex.h
    ipointertoolsconfiguration.h
    ipointertoolscontroller.h
    iringmenuprovider.h

    internal/commandindex.cpp
    internal/commandindex.h
    internal/commandshortcutskeylookup.cpp
    internal/commandshortcutskeylookup.h
    internal/commandsearch.cpp
    internal/commandsearch.h
    internal/commandsections.cpp
    internal/commandsections.h
    internal/icommandindexsource.h
    internal/icommandkeylookup.h
    internal/pointertoolscommandsregister.cpp
    internal/pointertoolscommandsregister.h
    internal/pointertoolscommandsstate.cpp
    internal/pointertoolscommandsstate.h
    internal/pointertoolsconfiguration.cpp
    internal/pointertoolsconfiguration.h
    internal/pointertoolscontroller.cpp
    internal/pointertoolscontroller.h
    internal/rcommandindexsource.cpp
    internal/rcommandindexsource.h
    internal/ringfallbackicons.cpp
    internal/ringfallbackicons.h
    internal/ringgeometry.cpp
    internal/ringgeometry.h
    internal/ringmenuprovider.cpp
    internal/ringmenuprovider.h
    internal/ringmenutables.cpp
    internal/ringmenutables.h
    internal/ringselection.cpp
    internal/ringselection.h
)

if (MUE_BUILD_POINTERTOOLS_TESTS)
    add_subdirectory(tests)
endif()

if (MUE_BUILD_POINTERTOOLS_QML)
    add_subdirectory(qml/MuseScore/PointerTools)
endif()
