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

#include "modularity/imoduleinterface.h"
#include "async/channel.h"
#include "io/path.h"

#include "arrangementtypes.h"

namespace mu::arrangement {
class IArrangementConfiguration : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IArrangementConfiguration)

public:
    virtual ~IArrangementConfiguration() = default;

    //! "When importing audio: Embed / Reference" (README decision 6). Default: Embed.
    virtual AssetStorage defaultAssetStorage() const = 0;
    virtual void setDefaultAssetStorage(AssetStorage storage) = 0;
    virtual muse::async::Channel<AssetStorage> defaultAssetStorageChanged() const = 0;

    //! "When saving a copy of a project that links to audio: Ask / Embed / Keep links"
    //! (README decision 6). Default: Ask; the prompt's "don't show again" stores the answer.
    virtual LinkedAudioInCopies linkedAudioInCopies() const = 0;
    virtual void setLinkedAudioInCopies(LinkedAudioInCopies value) = 0;

    //! Folder under which each open project keeps the session copies of its embedded audio.
    virtual muse::io::path_t audioSessionsPath() const = 0;
};
}
