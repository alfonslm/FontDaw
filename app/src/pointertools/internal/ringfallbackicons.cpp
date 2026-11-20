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

#include "ringfallbackicons.h"

#include <array>

using namespace muse::ui;

namespace {
struct FallbackIcon {
    const char* uri;
    IconCode::Code icon;
};

// The pointertools module does not depend on notationscene, so the commands are named by URI
// (see notationcommands.h: OPEN_EDIT_STYLE_COMMAND, OPEN_PAGE_SETTINGS_COMMAND, LOAD_STYLE_COMMAND).
// Cut, copy, paste and delete have icons upstream already.
constexpr std::array<FallbackIcon, 3> TABLE { {
    { "command://notation/open-edit-style", IconCode::Code::BRUSH },
    { "command://notation/open-page-settings", IconCode::Code::PAGE },
    { "command://notation/load-style", IconCode::Code::OPEN_FILE },
} };
}

IconCode::Code mu::pointertools::ringFallbackIcon(const std::string& commandUri)
{
    const std::string uri = commandUri.substr(0, commandUri.find('?'));
    for (const FallbackIcon& entry : TABLE) {
        if (uri == entry.uri) {
            return entry.icon;
        }
    }
    return IconCode::Code::NONE;
}
