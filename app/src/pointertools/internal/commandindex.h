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

#include <vector>

#include "global/async/asyncable.h"
#include "global/modularity/ioc.h"

#include "../icommandindex.h"
#include "icommandindexsource.h"
#include "icommandkeylookup.h"

namespace mu::pointertools {
//! Merges the sources, drops disabled commands, fills keys and sections, sorts. Built per call:
//! about 500 commands, filtered on opening the menu and on each typed letter (overview §6 asks
//! for "built once and filtered"; the static part is cached in step 2 if measuring shows a need).
class CommandIndex : public ICommandIndex, public muse::Contextable, public muse::async::Asyncable
{
public:
    CommandIndex(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void addSource(ICommandIndexSourcePtr source);
    void setKeyLookup(ICommandKeyLookupPtr lookup);
    void init();

    CommandEntryList entries(ViewKind view) const override;
    CommandEntryList search(ViewKind view, const muse::String& text) const override;
    muse::async::Notification entriesChanged() const override;

    void run(const CommandEntry& entry) const override;

private:
    std::vector<ICommandIndexSourcePtr> m_sources;
    ICommandKeyLookupPtr m_keyLookup;
    muse::async::Notification m_entriesChanged;
};
}
