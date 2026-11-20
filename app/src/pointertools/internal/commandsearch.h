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

#include "../pointertoolstypes.h"

namespace mu::pointertools {
//! Ranks entries for a search text. Pure function, tested on its own.
//!
//! Case-insensitive, accents kept. Each entry gets the best of these ranks (lower is better):
//!   0  the title starts with the text            ("stacc" → "Staccato")
//!   1  a word of the title starts with the text  ("hair" → "Crescendo hairpin")
//!   2  the title contains the text
//!   3  a word of the description starts with the text
//!   4  the description contains the text
//! Entries that match none are dropped. Ties: by section order, then the shorter title
//! ("Staccato" before "Staccatissimo"), then the entry's order in the input.
//! Several words in the text must all match (each word ranked; the entry takes the worst).
//! Upstream's Shortcuts page filters by plain substring (muse/framework/shortcuts/…/shortcutsmodel);
//! this adds the ranking a single Enter needs.
SearchHitList rankEntries(const CommandEntryList& entries, const muse::String& text);
}
