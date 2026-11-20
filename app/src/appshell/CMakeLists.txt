# SPDX-License-Identifier: GPL-3.0-only
# MuseScore-Studio-CLA-applies
#
# MuseScore Studio
# Music Composition & Notation
#
# Copyright (C) 2026 MuseScore Limited and others
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
# along with this program. If not, see <https://www.gnu.org/licenses/>.

muse_create_module(appshell)

target_sources(appshell PRIVATE
    appshellmodule.cpp
    appshellmodule.h
    iappshellconfiguration.h
    iappiconcontroller.h
    appicontypes.h
    appshelltypes.h
    appshellcommands.h

    internal/iappshellcommandscontroller.h
    internal/appshellcommandscontroller.cpp
    internal/appshellcommandscontroller.h
    internal/appshellcommandsregister.cpp
    internal/appshellcommandsregister.h
    internal/appshellcommandsstate.cpp
    internal/appshellcommandsstate.h
    internal/appshellstate.cpp
    internal/appshellstate.h
    internal/appshortcutsscopes.cpp
    internal/appshortcutsscopes.h
    internal/appshellconfiguration.cpp
    internal/appshellconfiguration.h
    internal/fontdawshortcutsdefault.cpp # FontDaw (FD-153)
    internal/fontdawshortcutsdefault.h # FontDaw (FD-153)
    internal/fontdawshortcutsdefaultrule.h # FontDaw (FD-153)
    internal/appiconcontroller.cpp
    internal/appiconcontroller.h
    internal/iappmenumodelhook.h
    internal/isessionsmanager.h
    internal/istartupscenario.h
    internal/sessionsmanager.cpp
    internal/sessionsmanager.h
    internal/startupscenario.cpp
    internal/startupscenario.h
    internal/extensioncontextresolver.cpp
    internal/extensioncontextresolver.h
    widgets/splashscreen/splashscreen.cpp
    widgets/splashscreen/splashscreen.h
    widgets/splashscreen/loadingscreenview.cpp
    widgets/splashscreen/loadingscreenview.h
    widgets/splashscreen/newinstanceloadingscreenview.cpp
    widgets/splashscreen/newinstanceloadingscreenview.h
)

if (OS_IS_MAC)
    target_sources(appshell PRIVATE
        internal/platform/macos/macosappmenumodelhook.mm
        internal/platform/macos/macosappmenumodelhook.h
    )

    set_source_files_properties(
        internal/platform/macos/macosappmenumodelhook.mm
        PROPERTIES
        SKIP_UNITY_BUILD_INCLUSION ON
        SKIP_PRECOMPILE_HEADERS ON
    )
endif(OS_IS_MAC)

qt_add_resources(appshell appshell_resources
    PREFIX
        /
    FILES
        resources/LoadingScreen.svg
        resources/welcomedialog/ExploreTutorials.png
        resources/welcomedialog/MuseScoreCom.png
        resources/welcomedialog/MuseSounds.png
        resources/welcomedialog/MuseSoundsPro.png
        resources/welcomedialog/WhatsNew.png
        resources/win_opengl_buglist.json
)

# FontDaw app icon (AppIconController, About, splash, Home); BASE keeps the file names.
qt_add_resources(appshell appshell_fontdaw_icons
    PREFIX
        /fontdawicon
    BASE
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon
    FILES
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark.svg
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light.svg
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_16x16.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_24x24.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_32x32.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_48x48.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_64x64.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_128x128.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark_256x256.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_16x16.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_24x24.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_32x32.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_48x48.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_64x64.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_128x128.png
        ${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-light_256x256.png
)

target_compile_definitions(appshell PRIVATE FONTDAW_HELP_BASE_URL="${FONTDAW_HELP_BASE_URL}") # FontDaw (FD-033)

target_link_libraries(appshell PRIVATE
    muse::ui
    muse::uicomponents
    Qt::Quick
    Qt::Svg
    Qt::Widgets
)

if (MUSE_MODULE_DOCKWINDOW)
    target_link_libraries(appshell PRIVATE muse::dockwindow)
endif()

if (MUE_BUILD_APPSHELL_QML)
    add_subdirectory(qml/MuseScore/AppShell)
endif()
