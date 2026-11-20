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

// DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

#include "recordingmodule.h"

#include <memory>

#include "modularity/ioc.h"

#include "internal/recordcontroller.h"

using namespace mu::recording;
using namespace muse::modularity;

static const std::string mname("recording");

std::string RecordingModule::moduleName() const
{
    return mname;
}

IContextSetup* RecordingModule::newContext(const ContextPtr& ctx) const
{
    return new RecordingContext(ctx);
}

void RecordingContext::registerExports()
{
    // TODO(FontDaw FD-010): ioc()->registerExport<IRecordController>(mname, m_controller);
    //
    // TODO(FontDaw): on start-up, look for unfinished takes in the session folder and offer to
    // recover them (README, "Rules that keep recordings safe").
}
