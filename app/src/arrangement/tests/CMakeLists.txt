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

set(MODULE_TEST arrangement_test)

set(MODULE_TEST_SRC
    ${PROJECT_SOURCE_DIR}/src/engraving/tests/utils/scorerw.cpp
    ${PROJECT_SOURCE_DIR}/src/engraving/tests/utils/scorerw.h
    ${CMAKE_CURRENT_LIST_DIR}/environment.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mocks/arrangementconfigurationmock.h
    ${CMAKE_CURRENT_LIST_DIR}/mocks/projectaudiosettingsfake.h
    ${CMAKE_CURRENT_LIST_DIR}/testutils.h
    ${CMAKE_CURRENT_LIST_DIR}/arrangementcontrollertests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/arrangementjsontests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/arrangementtests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/arrangementundotests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audioparttests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audiostaffhiddentests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audiostaffpainterbenchmark.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audiostaffpainterpreparetests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audioassetstoretests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mappedfiletests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/paintpagehooktests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scorepositionstests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audiodecodingtests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/barshifttests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/workerqueuetests.cpp
)

set(MODULE_TEST_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/..
)

set(MODULE_TEST_LINK
    arrangement
    notation
    engraving
)

set(MODULE_TEST_DATA_ROOT ${CMAKE_CURRENT_LIST_DIR})

# The framework's decoder test files (FLAC, MP3, Ogg Vorbis, Ogg Opus), used by audiodecodingtests.cpp
set(MODULE_TEST_DEF
    CODEC_TEST_DATA_DIR="${MUSE_FRAMEWORK_PATH}/framework/audio/tests/data/codecs"
)

include(SetupGTest)
