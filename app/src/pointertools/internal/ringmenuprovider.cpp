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

#include "ringmenuprovider.h"

#include "global/types/translatablestring.h"
#include "notation/inotation.h"
#include "notation/inotationinteraction.h"
#include "notation/inotationselection.h"

#include "ringfallbackicons.h"
#include "ringmenutables.h"

using namespace muse;
using namespace muse::ui;
using namespace mu::pointertools;

namespace {
const RingTable* tableFor(ScoreItemKind kind)
{
    switch (kind) {
    case ScoreItemKind::OneNote: return &oneNoteRingTable();
    case ScoreItemKind::SeveralNotes: return &severalNotesRingTable();
    case ScoreItemKind::Audio: return &audioRingTable();
    case ScoreItemKind::None:
    case ScoreItemKind::Other:
        break;
    }
    return nullptr;
}

String accessibleNameFor(ScoreItemKind kind)
{
    switch (kind) {
    case ScoreItemKind::OneNote: return TranslatableString("pointertools", "Actions for note").translated();
    case ScoreItemKind::SeveralNotes: return TranslatableString("pointertools", "Actions for notes").translated();
    case ScoreItemKind::Audio: return TranslatableString("pointertools", "Actions for audio").translated();
    case ScoreItemKind::None:
    case ScoreItemKind::Other:
        break;
    }
    return TranslatableString("pointertools", "Actions").translated();
}
}

RingContents RingMenuProvider::scoreRing(const std::vector<TodayMenuItem>& todayMenu) const
{
    return ringFor(currentItemKind(), todayMenu);
}

RingContents RingMenuProvider::ringFor(ScoreItemKind kind, const std::vector<TodayMenuItem>& todayMenu) const
{
    RingContents contents;
    contents.accessibleName = accessibleNameFor(kind);

    if (const RingTable* table = tableFor(kind)) {
        for (const RingEntry& entry : *table) {
            if (contents.ringSlots.size() >= RING_MAX_SLOTS - 1) {
                break;
            }
            if (commandsState() && !commandsState()->commandState(entry.command).enabled) {
                continue;
            }

            const rcommand::CommandInfo& info = commandsRegister()->commandInfo(entry.command);

            RingSlot slot;
            slot.kind = RingSlot::Kind::Command;
            slot.command = entry.command;
            slot.title = String::fromQString(info.title.qTranslatedWithoutMnemonic());
            slot.icon = entry.icon != IconCode::Code::NONE ? entry.icon : info.decoration.iconCode;
            slot.iconMirrored = entry.icon != IconCode::Code::NONE && entry.iconMirrored;
            slot.showName = entry.showName;
            contents.ringSlots.push_back(slot);
        }
    } else {
        for (const TodayMenuItem& item : todayMenu) {
            if (contents.ringSlots.size() >= RING_MAX_SLOTS - 1) {
                break;
            }
            if (!item.enabled || item.isSeparatorOrSubmenu) {
                continue;
            }

            RingSlot slot;
            slot.kind = RingSlot::Kind::TodayMenuItem;
            slot.todayMenuItemId = item.id;
            slot.title = item.title;
            // FD-156: an item without an icon may get one of its own from the fallback table
            slot.icon = item.icon != IconCode::Code::NONE ? item.icon : ringFallbackIcon(item.id);
            slot.showName = true;
            contents.ringSlots.push_back(slot);
        }
    }

    RingSlot more;
    more.kind = RingSlot::Kind::More;
    more.title = TranslatableString("pointertools", "More…").translated();
    more.icon = IconCode::Code::MENU_THREE_DOTS;
    more.showName = true;
    contents.ringSlots.push_back(more);

    return contents;
}

ScoreItemKind RingMenuProvider::currentItemKind() const
{
    const notation::INotationPtr notation = projectSession() ? projectSession()->currentNotation() : nullptr;
    const notation::INotationInteractionPtr interaction = notation ? notation->interaction() : nullptr;
    const notation::INotationSelectionPtr selection = interaction ? interaction->selection() : nullptr;
    if (!selection) {
        return ScoreItemKind::None;
    }

    std::vector<const engraving::EngravingItem*> elements;
    for (const engraving::EngravingItem* item : selection->elements()) {
        elements.push_back(item);
    }
    return classifySelection(elements, selection->isRange());
}
