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

set(MODULE_TEST appshell_test)

set(MODULE_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/appicontypestests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fontdawshortcutsdefaulttests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fontdawtabcommandmenutests.cpp
)

set(MODULE_TEST_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

set(MODULE_TEST_LINK
    Qt::Core
)

set(MODULE_TEST_DATA_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(SetupGTest)
