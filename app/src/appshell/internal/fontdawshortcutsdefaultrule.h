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

#include <algorithm>
#include <string>
#include <vector>

namespace mu::appshell {
//! FontDaw (FD-153): which shortcut preset to set at first start, or "" for none. Pure and
//! header-only, so it is unit-tested in the unit-test build, where appshell itself is not built
//! (src/appshell/tests). Used by FontDawShortcutsDefault.
//!
//! Nothing is set when the default was applied before, when the user already changed the base's
//! (5.0's) keys, when a preset is already set, when no default is configured, or when the
//! configured default is not a listed preset.
inline std::string fontDawShortcutsPresetToApply(bool alreadyApplied, bool hasBaseUserFile, const std::string& currentPreset,
                                                 const std::vector<std::string>& availablePresets,
                                                 const std::string& wantedPreset)
{
    if (alreadyApplied || hasBaseUserFile || !currentPreset.empty() || wantedPreset.empty()) {
        return std::string();
    }

    if (std::find(availablePresets.begin(), availablePresets.end(), wantedPreset) == availablePresets.end()) {
        return std::string();
    }

    return wantedPreset;
}
}
