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
#include "fontdawshortcutsdefault.h"
#include "fontdawshortcutsdefaultrule.h"

#include "global/configreader.h"
#include "global/io/file.h"
#include "settings.h"

#include "log.h"

using namespace mu::appshell;
using namespace muse;

static const Settings::Key DEFAULT_PRESET_APPLIED_KEY("appshell", "fontdaw/shortcuts/defaultPresetApplied");

void FontDawShortcutsDefault::init()
{
    if (!shortcutsConfiguration()) {
        return;
    }

    settings()->setDefaultValue(DEFAULT_PRESET_APPLIED_KEY, Val(false));
    const bool applied = settings()->value(DEFAULT_PRESET_APPLIED_KEY).toBool();
    if (applied) {
        return;
    }

    const Val wanted = ConfigReader::read(":/configs/shortcuts.cfg").value("fontdaw_default_preset");
    // a user file for the base means the user changed the 5.0 keys: keep them
    const io::path_t baseUserFile = shortcutsConfiguration()->commandShortcutsUserAppDataPath(
        shortcutsConfiguration()->defaultShortcutsName());
    const bool hasBaseUserFile = !baseUserFile.empty() && io::File::exists(baseUserFile);

    const std::string current = shortcutsConfiguration()->currentShortcutsPresetName();
    const std::vector<std::string> available = shortcutsConfiguration()->availableShortcutsPresets();
    const std::string preset = fontDawShortcutsPresetToApply(applied, hasBaseUserFile, current, available, wanted.toString());
    if (preset.empty() && !wanted.toString().empty() && !hasBaseUserFile && current.empty()) {
        LOGE() << "default shortcuts preset is not in the presets list: " << wanted.toString();
    }
    if (!preset.empty()) {
        shortcutsConfiguration()->setCurrentShortcutsPresetName(preset);
    }

    settings()->setSharedValue(DEFAULT_PRESET_APPLIED_KEY, Val(true));
}
