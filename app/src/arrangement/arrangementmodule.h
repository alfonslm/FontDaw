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

#include <memory>

#include "modularity/imodulesetup.h"

namespace mu::arrangement {
class ArrangementConfiguration;
class AudioAssetStore;
class Arrangement;
class ArrangementUndo;
class ArrangementController;
class AudioStaffPainter;
class ArrangementCommandsController;

//! Audio tracks, clips and assets next to the score (README): the model, the asset store, the
//! controller that keeps the audio engine in step, and the "Import audio track" command.
//! Same shape as src/playback/playbackmodule.h.
class ArrangementModule : public muse::modularity::IModuleSetup
{
public:
    std::string moduleName() const override;
    void registerExports() override;
    void resolveImports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;

    muse::modularity::IContextSetup* newContext(const muse::modularity::ContextPtr& ctx) const override;

private:
    std::shared_ptr<ArrangementConfiguration> m_configuration;
};

class ArrangementContext : public muse::modularity::IContextSetup
{
public:
    ArrangementContext(const muse::modularity::ContextPtr& ctx)
        : muse::modularity::IContextSetup(ctx) {}

    void registerExports() override;
    void resolveImports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;
    void onDeinit() override;

private:
    std::shared_ptr<AudioAssetStore> m_assetStore;
    std::shared_ptr<Arrangement> m_arrangement;
    std::shared_ptr<ArrangementUndo> m_arrangementUndo; // Batch 2 03a
    std::shared_ptr<ArrangementController> m_controller;
    std::shared_ptr<AudioStaffPainter> m_audioStaffPainter;
    std::shared_ptr<ArrangementCommandsController> m_commandsController;
};
}
