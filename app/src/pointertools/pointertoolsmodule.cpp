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

#include "pointertoolsmodule.h"

#include "modularity/ioc.h"

#include "rcommand/icommandsregister.h"
#include "rcommand/icommandsstate.h"

#include "shortcuts_v2/ishortcutsresolver.h"
#include "types/translatablestring.h"

#include "internal/commandindex.h"
#include "internal/commandshortcutskeylookup.h"
#include "internal/pointertoolscommandsregister.h"
#include "internal/pointertoolscommandsstate.h"
#include "internal/pointertoolsconfiguration.h"
#include "internal/pointertoolscontroller.h"
#include "internal/rcommandindexsource.h"
#include "internal/ringmenuprovider.h"

using namespace mu::pointertools;
using namespace muse;
using namespace muse::modularity;

static const std::string mname("pointertools");

std::string PointerToolsModule::moduleName() const
{
    return mname;
}

void PointerToolsModule::registerExports()
{
    m_configuration = std::make_shared<PointerToolsConfiguration>();
    globalIoc()->registerExport<IPointerToolsConfiguration>(mname, m_configuration);
}

void PointerToolsModule::resolveImports()
{
    auto cr = globalIoc()->resolve<muse::rcommand::ICommandsRegister>(mname);
    if (cr) {
        cr->reg(std::make_shared<PointerToolsCommandsRegister>());
    }
}

void PointerToolsModule::onInit(const IApplication::RunMode&)
{
    m_configuration->init();
}

IContextSetup* PointerToolsModule::newContext(const ContextPtr& ctx) const
{
    return new PointerToolsContext(ctx);
}

void PointerToolsContext::registerExports()
{
    m_controller = std::make_shared<PointerToolsController>(iocContext());
    m_commandIndex = std::make_shared<CommandIndex>(iocContext());
    m_ringMenuProvider = std::make_shared<RingMenuProvider>(iocContext());

    ioc()->registerExport<IPointerToolsController>(mname, m_controller);
    ioc()->registerExport<ICommandIndex>(mname, m_commandIndex);
    ioc()->registerExport<IRingMenuProvider>(mname, m_ringMenuProvider);
}

void PointerToolsContext::resolveImports()
{
    auto cs = ioc()->resolve<muse::rcommand::ICommandsState>(mname);
    if (cs) {
        cs->reg(std::make_shared<PointerToolsCommandsState>(iocContext()));
    }
}

void PointerToolsContext::onInit(const IApplication::RunMode& mode)
{
    if (mode != IApplication::RunMode::GuiApp) {
        return;
    }

    // The index's sources.
    m_commandIndex->addSource(std::make_shared<RCommandIndexSource>(iocContext()));
    // The keys next to each command, from shortcuts v2's register (FD-143).
    m_commandIndex->setKeyLookup(std::make_shared<CommandShortcutsKeyLookup>());
    // TODO(FontDaw pointertools step 6): a palette-cell source (fermatas, breaths, clefs…), only
    // where MUE_BUILD_PALETTE_MODULE is on; the palette module is off in the unit-test build.
    m_commandIndex->init();

    // This module's commands as a scope of their own in Preferences -> Shortcuts, with a
    // translatable heading. Priority 0 and no check, like the app's other global scopes: the
    // "Command menu" command has no default key here; the FontDaw layouts bind Tab to it in the
    // NOTATION scope (shortcut work order v2, A5).
    auto resolver = ioc()->resolve<muse::shortcuts::IShortcutsResolver>(mname);
    if (resolver) {
        //: Heading in Preferences > Shortcuts for the commands of the menus at the pointer
        resolver->setScope({ "POINTERTOOLS", muse::TranslatableString("pointertools", "POINTER MENUS"), 0 });
    }

    m_controller->init();
}
