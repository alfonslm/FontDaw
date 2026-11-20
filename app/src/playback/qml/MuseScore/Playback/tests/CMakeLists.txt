#
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
#

# FontDaw (FD-162): tests of the mixer's models (playback_qml)
set(MODULE_TEST playback_qml_tests)

set(MODULE_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/mixerchannelitem_tests.cpp
)

# mixerchannelitem.h includes the playback module's headers by their short names
set(MODULE_TEST_INCLUDE
    ${PROJECT_SOURCE_DIR}/src/playback
)

set(MODULE_TEST_LINK
    playback_qml
    Qt::Qml
    Qt::Quick
    muse::ui_qml
)

include(SetupGTest)
