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

namespace mu::pointertools {
class PointerToolsConfiguration;
class PointerToolsController;
class CommandIndex;
class RingMenuProvider;

//! Menus that open at the pointer (README.md): the pointer menu with its command index (shortcut
//! work order 08a) and the ring menu on the secondary click (08b). Later: the family pop-ups (09)
//! and repeat-last (10). Same shape as src/arrangement/arrangementmodule.h.
class PointerToolsModule : public muse::modularity::IModuleSetup
{
public:
    std::string moduleName() const override;
    void registerExports() override;
    void resolveImports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;

    muse::modularity::IContextSetup* newContext(const muse::modularity::ContextPtr& ctx) const override;

private:
    std::shared_ptr<PointerToolsConfiguration> m_configuration;
};

class PointerToolsContext : public muse::modularity::IContextSetup
{
public:
    PointerToolsContext(const muse::modularity::ContextPtr& ctx)
        : muse::modularity::IContextSetup(ctx) {}

    void registerExports() override;
    void resolveImports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;

private:
    std::shared_ptr<PointerToolsController> m_controller;
    std::shared_ptr<CommandIndex> m_commandIndex;
    std::shared_ptr<RingMenuProvider> m_ringMenuProvider;
};
}
