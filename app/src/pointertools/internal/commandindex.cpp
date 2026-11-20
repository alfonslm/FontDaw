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

#include "commandindex.h"

#include <algorithm>
#include <unordered_set>

#include "commandsearch.h"

using namespace muse;
using namespace mu::pointertools;

void CommandIndex::addSource(ICommandIndexSourcePtr source)
{
    m_sources.push_back(source);
}

void CommandIndex::setKeyLookup(ICommandKeyLookupPtr lookup)
{
    m_keyLookup = lookup;
}

void CommandIndex::init()
{
    for (const ICommandIndexSourcePtr& source : m_sources) {
        source->entriesChanged().onNotify(this, [this]() {
            m_entriesChanged.notify();
        });
    }
}

CommandEntryList CommandIndex::entries(ViewKind view) const
{
    CommandEntryList result;
    std::unordered_set<muse::rcommand::Command> seen;

    for (const ICommandIndexSourcePtr& source : m_sources) {
        for (CommandEntry& entry : source->entries(view)) {
            if (!seen.insert(entry.command).second) {
                continue; // an earlier source gave it
            }

            if (!source->isEnabled(entry.command)) {
                continue;
            }

            if (m_keyLookup) {
                muse::rcommand::CommandInfo info;
                info.command = entry.command;
                info.title = muse::MnemonicString(muse::TranslatableString::untranslatable(entry.title));
                info.description = muse::TranslatableString::untranslatable(entry.description);
                entry.keys = m_keyLookup->keys(info);
            }

            result.push_back(std::move(entry));
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const CommandEntry& a, const CommandEntry& b) {
        return static_cast<int>(a.section) < static_cast<int>(b.section);
    });

    return result;
}

CommandEntryList CommandIndex::search(ViewKind view, const String& text) const
{
    const CommandEntryList all = entries(view);

    CommandEntryList result;
    for (const SearchHit& hit : rankEntries(all, text)) {
        result.push_back(*hit.entry);
    }
    return result;
}

async::Notification CommandIndex::entriesChanged() const
{
    return m_entriesChanged;
}

void CommandIndex::run(const CommandEntry& entry) const
{
    for (const ICommandIndexSourcePtr& source : m_sources) {
        for (ViewKind view : { ViewKind::Score, ViewKind::Mixer }) {
            for (const CommandEntry& candidate : source->entries(view)) {
                if (candidate.command == entry.command) {
                    source->run(entry);
                    return;
                }
            }
        }
    }
}
