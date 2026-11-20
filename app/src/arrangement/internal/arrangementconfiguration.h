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

#include "../iarrangementconfiguration.h"

#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "iglobalconfiguration.h"

namespace mu::arrangement {
class ArrangementConfiguration : public IArrangementConfiguration, public muse::async::Asyncable
{
    muse::GlobalInject<muse::IGlobalConfiguration> globalConfiguration;

public:
    void init();

    AssetStorage defaultAssetStorage() const override;
    void setDefaultAssetStorage(AssetStorage storage) override;
    muse::async::Channel<AssetStorage> defaultAssetStorageChanged() const override;

    LinkedAudioInCopies linkedAudioInCopies() const override;
    void setLinkedAudioInCopies(LinkedAudioInCopies value) override;

    muse::io::path_t audioSessionsPath() const override;

private:
    muse::async::Channel<AssetStorage> m_defaultAssetStorageChanged;
};
}
