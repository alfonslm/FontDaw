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

#include "arrangementconfiguration.h"

#include "settings.h"

using namespace mu::arrangement;
using namespace muse;

static const std::string moduleName("arrangement");

static const Settings::Key DEFAULT_ASSET_STORAGE_KEY(moduleName, "arrangement/import/audioStorage");

static const Settings::Key LINKED_AUDIO_IN_COPIES_KEY(moduleName, "arrangement/export/linkedAudio");

static const char* ASK_VALUE = "ask";
static const char* KEEP_LINKS_VALUE = "keepLinks";
static const char* EMBED_VALUE = "embed";
static const char* REFERENCE_VALUE = "reference";

static AssetStorage storageFromVal(const Val& val)
{
    return val.toString() == REFERENCE_VALUE ? AssetStorage::Referenced : AssetStorage::Embedded;
}

void ArrangementConfiguration::init()
{
    // Owner's decision (FD-015, README decision 6): embed by default.
    settings()->setDefaultValue(DEFAULT_ASSET_STORAGE_KEY, Val(EMBED_VALUE));
    settings()->setDefaultValue(LINKED_AUDIO_IN_COPIES_KEY, Val(ASK_VALUE));
    settings()->valueChanged(DEFAULT_ASSET_STORAGE_KEY).onReceive(this, [this](const Val& val) {
        m_defaultAssetStorageChanged.send(storageFromVal(val));
    });
}

AssetStorage ArrangementConfiguration::defaultAssetStorage() const
{
    return storageFromVal(settings()->value(DEFAULT_ASSET_STORAGE_KEY));
}

void ArrangementConfiguration::setDefaultAssetStorage(AssetStorage storage)
{
    settings()->setSharedValue(DEFAULT_ASSET_STORAGE_KEY,
                               Val(storage == AssetStorage::Referenced ? REFERENCE_VALUE : EMBED_VALUE));
}

async::Channel<AssetStorage> ArrangementConfiguration::defaultAssetStorageChanged() const
{
    return m_defaultAssetStorageChanged;
}

LinkedAudioInCopies ArrangementConfiguration::linkedAudioInCopies() const
{
    const std::string value = settings()->value(LINKED_AUDIO_IN_COPIES_KEY).toString();
    if (value == EMBED_VALUE) {
        return LinkedAudioInCopies::Embed;
    }
    if (value == KEEP_LINKS_VALUE) {
        return LinkedAudioInCopies::KeepLinks;
    }
    return LinkedAudioInCopies::Ask;
}

void ArrangementConfiguration::setLinkedAudioInCopies(LinkedAudioInCopies value)
{
    const char* text = value == LinkedAudioInCopies::Embed ? EMBED_VALUE
                       : value == LinkedAudioInCopies::KeepLinks ? KEEP_LINKS_VALUE : ASK_VALUE;
    settings()->setSharedValue(LINKED_AUDIO_IN_COPIES_KEY, Val(text));
}

io::path_t ArrangementConfiguration::audioSessionsPath() const
{
    // TODO(FontDaw FD-029): remove session folders left behind by a crash. Not at start-up blindly:
    // several FontDaw windows (instances) can be open at once and share this folder.
    return globalConfiguration()->userAppDataPath() + "/audio_sessions";
}
