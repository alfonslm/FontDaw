# SPDX-License-Identifier: GPL-3.0-only
# MuseScore-Studio-CLA-applies
#
# MuseScore Studio
# Music Composition & Notation
#
# Copyright (C) 2024 MuseScore Limited and others
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

include(GetBuildType)
include(GetPlatformInfo)
include(version)

if (NOT MUSESCORE_BUILD_CONFIGURATION)
    set(MUSESCORE_BUILD_CONFIGURATION "app")
endif()

if (NOT MUSE_APP_BUILD_MODE)
    set(MUSE_APP_BUILD_MODE "dev")
endif()

# Set revision for local builds
# Before need run 'make revision' or 'msvc_build.bat revision'
include(TryUseLocalRevision)

message(STATUS "MUSESCORE_BUILD_CONFIGURATION: ${MUSESCORE_BUILD_CONFIGURATION}")
message(STATUS "MUSE_APP_BUILD_MODE: ${MUSE_APP_BUILD_MODE}")
message(STATUS "MUSESCORE_BUILD_NUMBER: ${CMAKE_BUILD_NUMBER}")

string(TOUPPER ${MUSESCORE_BUILD_CONFIGURATION} BUILD_CONFIGURATION)
string(TOUPPER ${MUSE_APP_BUILD_MODE} BUILD_MODE)

###########################################
# Setup by mode
###########################################
if(BUILD_MODE STREQUAL "DEV")
    set(MUSE_APP_UNSTABLE ON)
    set(MUSE_APP_RELEASE_CHANNEL "dev")
    set(MUSE_APP_IS_PRERELEASE ON)
    set(MUSESCORE_ALLOW_UPDATE_ON_PRERELEASE OFF)
endif()

if(BUILD_MODE STREQUAL "TESTING")
    set(MUSE_APP_UNSTABLE OFF)
    set(MUSE_APP_RELEASE_CHANNEL "Testing")
    set(MUSE_APP_IS_PRERELEASE ON)
    set(MUSESCORE_ALLOW_UPDATE_ON_PRERELEASE ON)
endif()

if(BUILD_MODE STREQUAL "RELEASE")
    set(MUSE_APP_UNSTABLE OFF)
    set(MUSE_APP_IS_PRERELEASE OFF)
    set(MUSESCORE_ALLOW_UPDATE_ON_PRERELEASE OFF)
endif()

if (MUSE_APP_UNSTABLE)
    set(MUSE_APP_GUI_IDENTIFIER "${MUSE_APP_GUI_IDENTIFIER}Development")
endif(MUSE_APP_UNSTABLE)

# Branding for framework executables that stand in for the application.
set(MUSE_APP_ICON_WIN "${PROJECT_SOURCE_DIR}/share/icons/FontDawIcon/fontdaw-icon-dark.ico")

###########################################
# Setup paths
###########################################
if (OS_IS_MAC)
    # FontDaw: the bundle name in one place; the QuickLook extension installs into it
    set(MUSE_APP_INSTALL_BUNDLE_NAME "fontdaw.app")
    set(MUSE_APP_INSTALL_RESOURCES_LOCATION "${MUSE_APP_INSTALL_BUNDLE_NAME}/Contents/Resources")
elseif (OS_IS_WIN)
    set(MUSE_APP_INSTALL_RESOURCES_LOCATION ".")
else()
    set(MUSE_APP_INSTALL_RESOURCES_LOCATION "share/fontdaw${MUSE_APP_INSTALL_SUFFIX}-${MUSE_APP_VERSION_MAJ_MIN}")
endif()

###########################################
# CONFIGURE: Desktop App
###########################################
set(MUE_GENERAL_APP OFF)
set(MUE_ENABLE_SPLASHSCREEN ON)
set(MUE_ENABLE_CONSOLEAPP ON)
if(BUILD_CONFIGURATION STREQUAL "APP")
    set(MUE_CONFIGURATION_IS_APP ON)
    set(MUE_GENERAL_APP ON)
    set(MUSE_MODULE_AUDIO_WORKMODE 3) # driver callback + worker rpc
endif()

if(BUILD_CONFIGURATION STREQUAL "APP-PORTABLE")
    set(MUE_CONFIGURATION_IS_APPPORTABLE ON)
    set(MUE_GENERAL_APP ON)
    set(MUSE_MODULE_AUDIO_WORKMODE 3) # driver callback + worker rpc
    set(WIN_PORTABLE ON)
endif()

###########################################
# FontDaw: Muse Group online services are off (FD-003, FD-004, FD-005, REBRANDING §5).
# This is the one place that decides it, for every build configuration. The framework's stub
# modules (muse/framework/stubs) and src/stubs/musesounds take the place of the real ones.
###########################################
set(MUSE_MODULE_CLOUD OFF)                        # musescore.com account and cloud scores, audio.com upload
set(MUSE_MODULE_CLOUD_MUSESCORECOM OFF)
set(MUSE_MODULE_UPDATE OFF)                       # update checks against MuseScore's servers
set(MUSE_MODULE_LEARN OFF)                        # MuseScore tutorial playlists on the Home page
set(MUSE_MODULE_TOURS OFF)                        # MuseScore feature tours
set(MUSE_MODULE_MUSESAMPLER OFF)                  # loader for the proprietary Muse Sounds library, online sounds
set(MUE_BUILD_MUSESOUNDS_MODULE OFF)              # Muse Hub / Muse Sounds catalogue and update checks
set(MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT OFF)  # crash uploads (Muse Group's Sentry)
set(MUSE_MODULE_DIAGNOSTICS_CRASHREPORT_URL "")
set(MUSE_MODULE_AUDIOPLUGINS_CRASHREPORT_URL "")  # plug-in scan crash reports

# FontDaw (FD-033, decision D10, A3 piece 12a): base URL for FontDaw's help links. Used so far by the
# Help-menu and About links (appshell) and the "file too new" message (project); the other dialog links
# listed in FD-033 still need it. Empty: the links are hidden or shown as plain text. Once FontDaw has
# its own help pages, set it (here or with -DFONTDAW_HELP_BASE_URL=...), e.g.
# https://github.com/alfonslm/FontDaw/wiki. Page list: issues/FD-033.md. Passed only to the targets that
# use it (src/appshell, src/project).
set(FONTDAW_HELP_BASE_URL "" CACHE STRING "FontDaw help pages base URL (empty: help links hidden)")

if (WIN_PORTABLE)
    message(FATAL_ERROR "FontDaw: PortableApps builds are dropped for now (FD-047).")
endif()
if (OS_IS_FBSD)
    message(WARNING "Not building unsupported chrashpad client on FreeBSD")
    set(MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT OFF)
endif()

###########################################
# CONFIGURE: Web App
###########################################
if(BUILD_CONFIGURATION STREQUAL "APP-WEB")
    set(MUE_CONFIGURATION_IS_APPWEB ON)
    set(MUSE_CONFIGURATION_IS_WEB ON)

    set(QT_ADD_LINGUISTTOOLS OFF)
    set(QT_QPROCESS_SUPPORTED OFF)

    if (OS_IS_WASM)
        set(QT_ADD_CONCURRENT OFF)
        set(QT_CONCURRENT_SUPPORTED OFF)
        set(MUSE_THREADS_SUPPORT OFF)
        # The engine runs in the audio worklet, a single thread
        set(MUSE_MODULE_AUDIO_WORKMODE 2) # driver callback
    else()
        set(MUSE_THREADS_SUPPORT ON)
        # The native audio drivers may call back on different threads (e.g. AudioQueue on macOS),
        # the engine needs its own thread, as on desktop
        set(MUSE_MODULE_AUDIO_WORKMODE 3) # driver callback + worker rpc
    endif()

    set(MUSE_MODULE_UI_SYSTEMDRAG_SUPPORTED OFF)
    set(MUSE_MODULE_INTERACTIVE_SYNC_SUPPORTED OFF)

    set(MUE_GENERAL_APP ON)
    set(MUSE_MODULE_SHORTCUTS ON)
    set(MUSE_MODULE_AUDIO ON)
    set(MUSE_MODULE_MPE ON)
    set(MUE_BUILD_APPSHELL_MODULE ON)
    set(MUE_BUILD_PROJECT_MODULE ON)
    set(MUE_BUILD_PROPERTIESPANEL_MODULE ON)
    set(MUE_BUILD_INSTRUMENTSSCENE_MODULE ON)
    set(MUE_BUILD_PALETTE_MODULE ON)
    set(MUE_BUILD_PLAYBACK_MODULE ON)

    set(MUSE_ENABLE_UNIT_TESTS OFF)

    #set(MUSE_MODULE_DIAGNOSTICS OFF)
    set(MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT OFF)
    set(MUSE_MODULE_AUDIO_EXPORT OFF)
    set(MUSE_MODULE_AUDIOPLUGINS OFF)
    set(MUSE_MODULE_AUTOMATION OFF)
    set(MUSE_MODULE_ACCESSIBILITY OFF)
    set(MUSE_MODULE_MIDI OFF)
    set(MUSE_MODULE_MIDIREMOTE OFF)
    set(MUSE_MODULE_MUSESAMPLER OFF)
    set(MUSE_MODULE_NETWORK OFF)
    set(MUSE_MODULE_VST OFF)
    set(MUSE_MODULE_CLOUD OFF)
    set(MUSE_MODULE_LANGUAGES OFF)
    set(MUSE_MODULE_LEARN OFF)
    set(MUSE_MODULE_MULTIWINDOWS OFF)
    set(MUSE_MODULE_EXTENSIONS OFF)
    set(MUSE_MODULE_UPDATE OFF)
    set(MUSE_MODULE_WORKSPACE OFF)
    set(MUSE_MODULE_TESTFLOW OFF)
    set(MUSE_MODULE_TOURS OFF)

    set(MUE_ENABLE_SPLASHSCREEN OFF)
    set(MUE_ENABLE_CONSOLEAPP OFF)
    set(MUE_BUILD_CONVERTER_MODULE OFF)
    set(MUE_BUILD_PRINT_MODULE OFF)
    set(MUE_BUILD_ARRANGEMENT_MODULE OFF) # FontDaw; imports on worker threads
    set(MUE_BUILD_POINTERTOOLS_MODULE OFF) # FontDaw; desktop menus
    set(MUE_BUILD_MUSESOUNDS_MODULE OFF)
    set(MUE_BUILD_BRAILLE_MODULE OFF)
    set(MUE_BUILD_MACOS_INTEGRATION OFF)
    # IMPORT EXPORT MODULES
    set(MUE_BUILD_IMPEXP_BB_MODULE OFF)
    set(MUE_BUILD_IMPEXP_BWW_MODULE OFF)
    set(MUE_BUILD_IMPEXP_CAPELLA_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MIDI_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MLSCORE_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MNX_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MUSEDATA_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MUSICXML_MODULE OFF)
    set(MUE_BUILD_IMPEXP_OVE_MODULE OFF)
    set(MUE_BUILD_IMPEXP_AUDIOEXPORT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_IMAGESEXPORT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_GUITARPRO_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MEI_MODULE OFF)
    set(MUE_BUILD_IMPEXP_VIDEOEXPORT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_TABLEDIT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_LYRICS_MODULE OFF)

    set(MUE_INSTALL_SOUNDFONT OFF)

endif()

###########################################
# CONFIGURE: VTest
###########################################
if(BUILD_CONFIGURATION STREQUAL "VTEST")
    set(MUE_CONFIGURATION_IS_VTEST ON)

    set(MUSE_ENABLE_UNIT_TESTS OFF)
    set(MUSE_MODULE_GLOBAL_LOGGER_DEBUGLEVEL ON)
    set(MUSE_COMPILE_ASAN ON)
    set(MUE_RUN_LRELEASE OFF)

    set(MUSE_MODULE_ACCESSIBILITY OFF)
    set(MUSE_MODULE_AUDIO OFF)
    set(MUSE_MODULE_AUDIOPLUGINS OFF)
    set(MUSE_MODULE_AUTOMATION OFF)
    set(MUSE_MODULE_CLOUD OFF)
    set(MUSE_MODULE_DIAGNOSTICS OFF)
    set(MUSE_MODULE_DOCKWINDOW OFF)
    set(MUSE_MODULE_EXTENSIONS OFF)
    set(MUSE_MODULE_INTERACTIVE OFF)
    set(MUSE_MODULE_LANGUAGES OFF)
    set(MUSE_MODULE_LEARN OFF)
    set(MUSE_MODULE_MIDI OFF)
    set(MUSE_MODULE_MIDIREMOTE OFF)
    set(MUSE_MODULE_MPE OFF)
    set(MUSE_MODULE_MULTIWINDOWS OFF)
    set(MUSE_MODULE_MUSESAMPLER OFF)
    set(MUSE_MODULE_NETWORK OFF)
    set(MUSE_MODULE_SHORTCUTS OFF)
    set(MUSE_MODULE_TESTFLOW OFF)
    set(MUSE_MODULE_TOURS OFF)
    set(MUSE_MODULE_UI ON)
    set(MUSE_MODULE_UI_QML OFF)
    set(MUSE_MODULE_UI_API OFF)
    set(MUSE_MODULE_UPDATE OFF)
    set(MUSE_MODULE_VST OFF)
    set(MUSE_MODULE_WORKSPACE OFF)

    set(MUE_BUILD_APPSHELL_MODULE OFF)
    set(MUE_ENABLE_SPLASHSCREEN OFF) 
    set(MUE_BUILD_BRAILLE_MODULE OFF)
    set(MUE_BUILD_CONVERTER_MODULE ON)
    set(MUE_BUILD_PROPERTIESPANEL_MODULE OFF)
    set(MUE_BUILD_INSTRUMENTSSCENE_MODULE OFF)
    set(MUE_BUILD_MUSESOUNDS_MODULE OFF)
    set(MUE_BUILD_NOTATION_MODULE ON)
    set(MUE_BUILD_NOTATIONSCENE_MODULE OFF)
    set(MUE_BUILD_PALETTE_MODULE OFF)
    set(MUE_BUILD_PLAYBACK_MODULE OFF)
    set(MUE_BUILD_PREFERENCES_MODULE OFF)
    set(MUE_BUILD_PROJECT_MODULE ON)

    # IMPORT EXPORT MODULES
    set(MUE_BUILD_IMPEXP_BB_MODULE OFF)
    set(MUE_BUILD_IMPEXP_BWW_MODULE OFF)
    set(MUE_BUILD_IMPEXP_CAPELLA_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MIDI_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MLSCORE_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MNX_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MUSEDATA_MODULE OFF)
    set(MUE_BUILD_IMPEXP_MUSICXML_MODULE OFF)
    set(MUE_BUILD_IMPEXP_OVE_MODULE OFF)
    set(MUE_BUILD_IMPEXP_AUDIOEXPORT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_IMAGESEXPORT_MODULE ON)
    set(MUE_BUILD_IMPEXP_GUITARPRO_MODULE ON)
    set(MUE_BUILD_IMPEXP_MEI_MODULE OFF)
    set(MUE_BUILD_IMPEXP_VIDEOEXPORT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_TABLEDIT_MODULE OFF)
    set(MUE_BUILD_IMPEXP_LYRICS_MODULE OFF)

    set(MUE_INSTALL_SOUNDFONT OFF)

    set(MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT OFF)

endif()

###########################################
# CONFIGURE: UTest
###########################################
if(BUILD_CONFIGURATION STREQUAL "UTEST")
    set(MUE_CONFIGURATION_IS_UTEST ON)

    set(MUSE_ENABLE_UNIT_TESTS ON)
    set(MUSE_MODULE_GLOBAL_LOGGER_DEBUGLEVEL ON)
    set(MUSE_MODULE_AUDIO ON)
    if (MUSE_ENABLE_UNIT_TESTS_CODE_COVERAGE)
        set(MUSE_COMPILE_ASAN OFF)
    else()
        set(MUSE_COMPILE_ASAN ON)
    endif()

    set(MUSE_MODULE_TESTFLOW OFF)
    set(MUSE_MODULE_UPDATE ON) # FontDaw: off in the app (above); built here only so its unit tests keep running
    set(MUE_ENABLE_SPLASHSCREEN OFF)

    # With code coverage, build every module, so that the ones without tests are counted as uncovered
    if (NOT MUSE_ENABLE_UNIT_TESTS_CODE_COVERAGE)
        message(STATUS "If you added tests to a module that didn't have them yet, make sure that this module is enabled, see SetupConfigure.cmake")
        set(MUSE_MODULE_AUTOMATION OFF)
        set(MUSE_MODULE_LEARN OFF)
        set(MUSE_MODULE_MPE_QML OFF)
        set(MUSE_MODULE_MUSESAMPLER OFF)
        # FontDaw (FD-107): the shortcuts module (shortcuts v2) stays on, so its register and resolver
        # tests run in the unit-test build (and FontDaw's own shortcut tests, as they are added)
        # set(MUSE_MODULE_SHORTCUTS OFF)
        set(MUSE_MODULE_TOURS OFF)
        set(MUSE_MODULE_VST OFF)

        set(MUE_BUILD_APPSHELL_MODULE OFF)
        set(MUE_BUILD_BRAILLE_QML OFF)
        set(MUE_BUILD_PROPERTIESPANEL_MODULE OFF)
        set(MUE_BUILD_PALETTE_MODULE OFF)
        set(MUE_BUILD_PREFERENCES_MODULE OFF)
    endif()
endif()

###########################################
# Configure framework
###########################################
set(MUSE_APP_REVISION ${MUSESCORE_REVISION})
set(MUSE_APP_BUILD_NUMBER ${CMAKE_BUILD_NUMBER})
set(MUSE_APP_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

include(${MUSE_FRAMEWORK_SRC_PATH}/cmake/MuseSetupConfiguration.cmake)

###########################################
# Subsystem
###########################################

if (MUSE_MODULE_AUDIO_JACK)
    if (NOT (OS_IS_LIN OR CC_IS_MINGW))
        set(MUSE_MODULE_AUDIO_JACK OFF)
    endif()
endif()

if (MUSE_COMPILE_ASAN)
    set(MUSE_ENABLE_CUSTOM_ALLOCATOR OFF)
endif()

if (NOT MUE_BUILD_NOTATION_MODULE)
    set(MUE_BUILD_NOTATIONSCENE_MODULE OFF) # hard dependency
    set(MUE_BUILD_PROJECT_MODULE OFF) # hard dependency
    set(MUE_BUILD_PALETTE_MODULE OFF) # hard dependency
endif()

if (NOT MUE_BUILD_NOTATIONSCENE_MODULE)
    set(MUE_BUILD_PROJECT_QML OFF) # hard dependency
endif()

if (NOT MUSE_MODULE_UI)
    set(MUE_BUILD_APPSHELL_MODULE OFF) # hard dependency
endif()

if (NOT MUSE_MODULE_UI_QML)
    set(MUE_BUILD_APPSHELL_QML OFF) # hard dependency
    set(MUE_BUILD_BRAILLE_QML OFF) # hard dependency
    set(MUE_BUILD_ENGRAVING_QML OFF) # hard dependency
    set(MUE_BUILD_PROPERTIESPANEL_QML OFF) # hard dependency
    set(MUE_BUILD_INSTRUMENTSSCENE_QML OFF) # hard dependency
    set(MUE_BUILD_MUSESOUNDS_QML OFF) # hard dependency
    set(MUE_BUILD_NOTATIONSCENE_QML OFF) # hard dependency
    set(MUE_BUILD_PALETTE_QML OFF) # hard dependency
    set(MUE_BUILD_PLAYBACK_QML OFF) # hard dependency
    set(MUE_BUILD_POINTERTOOLS_QML OFF) # hard dependency; FontDaw
    set(MUE_BUILD_PREFERENCES OFF) # hard dependency
    set(MUE_BUILD_PROJECT_QML OFF) # hard dependency
endif()

if (NOT MUE_BUILD_APPSHELL_MODULE)
    set(MUE_BUILD_APPSHELL_QML OFF) # stub does not have QML
endif()

if (NOT MUE_BUILD_POINTERTOOLS_MODULE) # FontDaw
    set(MUE_BUILD_POINTERTOOLS_QML OFF)
    set(MUE_BUILD_POINTERTOOLS_TESTS OFF)
endif()

if (NOT MUE_BUILD_INSTRUMENTSSCENE_MODULE)
    set(MUE_BUILD_INSTRUMENTSSCENE_QML OFF) # stub does not have QML
endif()

if (NOT QT_ADD_LINGUISTTOOLS)
    set(MUE_RUN_LRELEASE OFF) # hard dependency
endif()

if (MUSE_MODULE_NETWORK_WEBSOCKET)
    set(QT_ADD_WEBSOCKET ON)
endif()

###########################################
# Unit tests
###########################################
if (NOT MUSE_ENABLE_UNIT_TESTS)

    set(MUE_BUILD_ARRANGEMENT_TESTS OFF) # FontDaw
    set(MUE_BUILD_POINTERTOOLS_TESTS OFF) # FontDaw
    set(MUE_BUILD_BRAILLE_TESTS OFF)
    set(MUE_BUILD_ENGRAVING_TESTS OFF)
    set(MUE_BUILD_IMPORTEXPORT_TESTS OFF)
    set(MUE_BUILD_INSTRUMENTSSCENE_TESTS OFF)
    set(MUE_BUILD_MUSESOUNDS_TESTS OFF)
    set(MUE_BUILD_NOTATION_TESTS OFF)
    set(MUE_BUILD_NOTATIONSCENE_TESTS OFF)
    set(MUE_BUILD_PLAYBACK_TESTS OFF)
    set(MUE_BUILD_PROJECT_TESTS OFF)
    set(MUE_BUILD_APPSHELL_TESTS OFF) # FontDaw
    set(MUE_BUILD_CONVERTER_TESTS OFF)

endif()

if (MUE_GENERAL_APP)
    if (BUILD_IS_DEBUG)
        set(MUSE_MODULE_GLOBAL_LOGGER_DEBUGLEVEL ON)
    else()
        set(MUSE_MODULE_GLOBAL_LOGGER_DEBUGLEVEL OFF)
    endif()
endif()

add_compile_definitions(KORS_PROFILER_ENABLED)

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/app/app_config.h.in app_config.h )
