/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * FontDaw
 * The notation-first DAW
 *
 * Copyright (C) 2026 FontDaw contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

#include <QtCore/qnamespace.h>

namespace mu::appshell {
//! Which colourway of the FontDaw icon the running app shows (Preferences > Appearance > "App icon").
//! Installed icons (desktop file, .exe, Start menu, macOS bundle) are fixed at install time and are
//! not affected; see share/icons/FontDawIcon/README.md.
enum class AppIconVariant {
    Dark,  //!< white note on the dark tile (the installed icon on Windows and Linux)
    Light, //!< black note on the light tile
};

inline constexpr AppIconVariant DEFAULT_PREFERRED_APP_ICON = AppIconVariant::Dark;
inline constexpr bool DEFAULT_APP_ICON_FOLLOWS_SYSTEM = true;

// Settings keys, also read directly by the splash screen before the settings system is up.
inline constexpr const char* APP_ICON_PREFERRED_KEY = "ui/application/appIcon/preferred";
inline constexpr const char* APP_ICON_FOLLOW_SYSTEM_KEY = "ui/application/appIcon/followSystem";

//! Settings value of a variant: "dark" or "light". Stored as a string so the settings file stays readable.
inline std::string appIconVariantToString(AppIconVariant variant)
{
    return variant == AppIconVariant::Light ? "light" : "dark";
}

//! Inverse of appIconVariantToString(): "light" gives Light, anything else (unknown, empty) gives Dark.
inline AppIconVariant appIconVariantFromString(const std::string& str)
{
    return str == "light" ? AppIconVariant::Light : AppIconVariant::Dark;
}

//! The icon to show: the system's colour scheme when the user follows the system and the scheme is
//! known (Dark or Light), otherwise the preferred variant.
inline AppIconVariant resolveAppIconVariant(AppIconVariant preferred, bool followSystem, Qt::ColorScheme system)
{
    if (followSystem) {
        if (system == Qt::ColorScheme::Dark) {
            return AppIconVariant::Dark;
        }
        if (system == Qt::ColorScheme::Light) {
            return AppIconVariant::Light;
        }
    }
    return preferred;
}

//! Resource path of the SVG of a variant (see qt_add_resources in src/appshell/CMakeLists.txt).
inline const char* appIconSvgResource(AppIconVariant variant)
{
    return variant == AppIconVariant::Light ? ":/fontdawicon/fontdaw-icon-light.svg"
           : ":/fontdawicon/fontdaw-icon-dark.svg";
}

//! Resource path of the square PNG of a variant at the given size, e.g. ":/fontdawicon/fontdaw-icon-dark_32x32.png".
inline std::string appIconPngResource(AppIconVariant variant, int size)
{
    const std::string dim = std::to_string(size);
    return std::string(":/fontdawicon/fontdaw-icon-") + (variant == AppIconVariant::Light ? "light" : "dark")
           + "_" + dim + "x" + dim + ".png";
}
}
