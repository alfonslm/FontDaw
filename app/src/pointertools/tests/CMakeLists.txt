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

# Unit tests (README.md, "Tests"). The module has no dependency the unit-test configuration
# switches off (appshell, palette and the shortcuts module are not needed), so these run there.

set(MODULE_TEST pointertools_test)

set(MODULE_TEST_SRC
    ${PROJECT_SOURCE_DIR}/src/engraving/tests/utils/scorerw.cpp
    ${PROJECT_SOURCE_DIR}/src/engraving/tests/utils/scorerw.h
    # the real notation commands, for the icon check (notationscene itself is not linked: its
    # unity objects need the whole module)
    ${PROJECT_SOURCE_DIR}/src/notationscene/internal/notationcommandsregister.cpp
    ${CMAKE_CURRENT_LIST_DIR}/environment.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commandindextests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commandsearchtests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commandsectionstests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commandshortcutskeylookuptests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rcommandindexsourcetests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ringfallbackiconstests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ringgeometrytests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ringmenuprovidertests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ringmenutablestests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ringselectiontests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tabtexteditingtests.cpp
)

set(MODULE_TEST_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/..
)

set(MODULE_TEST_LINK
    pointertools
    notation
    muse::shortcuts
    engraving
)

set(MODULE_TEST_DATA_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(SetupGTest)
