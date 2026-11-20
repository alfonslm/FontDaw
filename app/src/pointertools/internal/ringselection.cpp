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

#include "ringselection.h"

#include <set>

#include "engraving/dom/chord.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/note.h"

using namespace mu::engraving;
using namespace mu::pointertools;

ScoreItemKind mu::pointertools::classifySelection(const std::vector<const EngravingItem*>& elements, bool isRange)
{
    if (elements.empty() && !isRange) {
        return ScoreItemKind::None;
    }

    for (const EngravingItem* item : elements) {
        if (item && isAudioStaff(item->staff())) {
            return ScoreItemKind::Audio;
        }
    }

    if (isRange) {
        return ScoreItemKind::SeveralNotes;
    }

    std::set<const EngravingItem*> chords;
    for (const EngravingItem* item : elements) {
        if (!item || item->type() != ElementType::NOTE) {
            return ScoreItemKind::Other;
        }
        chords.insert(toNote(item)->chord());
    }

    return chords.size() == 1 ? ScoreItemKind::OneNote : ScoreItemKind::SeveralNotes;
}
