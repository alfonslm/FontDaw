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

#include "arrangementmodule.h"

#include "modularity/ioc.h"

#include "rcommand/icommandsregister.h"
#include "rcommand/icommandsstate.h"

#include "internal/arrangement.h"
#include "internal/arrangementundo.h"
#include "internal/arrangementcommandscontroller.h"
#include "internal/arrangementcommandsregister.h"
#include "internal/audiostaffpainter.h"
#include "internal/arrangementcommandsstate.h"
#include "internal/arrangementconfiguration.h"
#include "internal/arrangementcontroller.h"
#include "internal/audioassetstore.h"

using namespace mu::arrangement;
using namespace muse;
using namespace muse::modularity;

static const std::string mname("arrangement");

std::string ArrangementModule::moduleName() const
{
    return mname;
}

void ArrangementModule::registerExports()
{
    m_configuration = std::make_shared<ArrangementConfiguration>();
    globalIoc()->registerExport<IArrangementConfiguration>(mname, m_configuration);
}

void ArrangementModule::resolveImports()
{
    auto cr = globalIoc()->resolve<muse::rcommand::ICommandsRegister>(mname);
    if (cr) {
        cr->reg(std::make_shared<ArrangementCommandsRegister>());
    }
}

void ArrangementModule::onInit(const IApplication::RunMode&)
{
    m_configuration->init();
}

IContextSetup* ArrangementModule::newContext(const ContextPtr& ctx) const
{
    return new ArrangementContext(ctx);
}

void ArrangementContext::registerExports()
{
    m_assetStore = std::make_shared<AudioAssetStore>(iocContext());
    m_arrangement = std::make_shared<Arrangement>(iocContext());

    ioc()->registerExport<IAudioAssetStore>(mname, m_assetStore);
    ioc()->registerExport<IArrangement>(mname, m_arrangement);

    // Arrangement edits in the score's undo history (Batch 2 03a)
    m_arrangementUndo = std::make_shared<ArrangementUndo>(iocContext());
    ioc()->registerExport<IArrangementUndo>(mname, m_arrangementUndo);

    m_controller = std::make_shared<ArrangementController>(iocContext());
    m_commandsController = std::make_shared<ArrangementCommandsController>(iocContext());

    ioc()->registerExport<IArrangementController>(mname, m_controller);

    // Audio staves in the score (Batch 2 S1): notation calls this after painting the score
    m_audioStaffPainter = std::make_shared<AudioStaffPainter>(iocContext());
    ioc()->registerExport<notation::IAudioStaffPainter>(mname, m_audioStaffPainter);

    // TODO(FontDaw step 7): the arrangement panel's commands (pattern: this module's
    // arrangementcommands.h): add audio track, split clip, delete clip, toggle snap,
    // and default shortcuts in src/app/configs/data/shortcuts*.xml (S = split is the common convention).
}

void ArrangementContext::resolveImports()
{
    auto cs = ioc()->resolve<muse::rcommand::ICommandsState>(mname);
    if (cs) {
        cs->reg(std::make_shared<ArrangementCommandsState>(iocContext()));
    }
}

void ArrangementContext::onInit(const IApplication::RunMode& mode)
{
    // The controller also runs in the converter: a mixdown made from the command line
    // includes the audio tracks. Adding audio staves to older projects is for the app only.
    m_controller->setAddAudioStavesOnOpen(mode == IApplication::RunMode::GuiApp);
    m_controller->init();
    m_audioStaffPainter->init(); // subscribes here, on the GUI thread; it paints on the render thread

    if (mode != IApplication::RunMode::GuiApp) {
        return;
    }

    m_commandsController->init();
}

void ArrangementContext::onDeinit()
{
    m_controller->deinit();
    // The window is closing: its projects are gone or going, and so is the session folder
    m_arrangement->clear();
}
