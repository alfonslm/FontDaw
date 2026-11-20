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

# The QML side: the two models, the layer each view loads (PointerToolsLayer.qml) and the pop-ups
# (internal/). NotationView.qml loads the layer with a Loader by URL, so the score view does not
# import this module and works without it.

muse_create_qml_module(pointertools_qml FOR pointertools)

qt_add_qml_module(pointertools_qml
    URI MuseScore.PointerTools
    VERSION 1.0
    SOURCES
        pointermenumodel.cpp
        pointermenumodel.h
        ringinputarea.cpp
        ringinputarea.h
        ringmenumodel.cpp
        ringmenumodel.h
    QML_FILES
        PointerToolsLayer.qml
        internal/PointerMenuPopup.qml
        internal/RingMenu.qml
)

# QQuickItem and friends: Qt Quick is linked explicitly, as upstream QML modules do, because
# without precompiled headers (the Windows build) it does not come in through another target.
target_link_libraries(pointertools_qml PRIVATE Qt::Quick)
