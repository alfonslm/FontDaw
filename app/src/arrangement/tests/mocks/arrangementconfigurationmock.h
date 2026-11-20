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

#include <gmock/gmock.h>

#include "iarrangementconfiguration.h"

namespace mu::arrangement {
class ArrangementConfigurationMock : public IArrangementConfiguration
{
public:
    MOCK_METHOD(AssetStorage, defaultAssetStorage, (), (const, override));
    MOCK_METHOD(void, setDefaultAssetStorage, (AssetStorage), (override));
    MOCK_METHOD(muse::async::Channel<AssetStorage>, defaultAssetStorageChanged, (), (const, override));
    MOCK_METHOD(LinkedAudioInCopies, linkedAudioInCopies, (), (const, override));
    MOCK_METHOD(void, setLinkedAudioInCopies, (LinkedAudioInCopies), (override));
    MOCK_METHOD(muse::io::path_t, audioSessionsPath, (), (const, override));
};
}
