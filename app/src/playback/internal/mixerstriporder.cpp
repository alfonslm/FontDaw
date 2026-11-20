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

#include "mixerstriporder.h"

#include <algorithm>

using namespace mu::playback;
using namespace mu::engraving;

static bool isScoreStrip(const MixerStripRef& strip)
{
    return strip.kind == MixerStripRef::Kind::Instrument || strip.kind == MixerStripRef::Kind::ChordSymbols;
}

int mu::playback::mixerStripInsertIndex(const std::vector<MixerStripRef>& strips, const std::vector<InstrumentTrackId>& scoreOrder,
                                        const InstrumentTrackId& newId, MixerStripRef::Kind newKind)
{
    const int count = static_cast<int>(strips.size());

    for (const MixerStripRef& strip : strips) {
        if (strip.kind != MixerStripRef::Kind::Other && strip.id == newId) {
            return -1; // already there
        }
    }

    // The instruments come first, then chord symbols, then the metronome, then the rest
    const auto firstNot = [&strips, count](auto pred) {
        const auto it = std::find_if(strips.cbegin(), strips.cend(), [&pred](const MixerStripRef& s) { return !pred(s); });
        return it == strips.cend() ? count : static_cast<int>(it - strips.cbegin());
    };

    if (newKind == MixerStripRef::Kind::Metronome) {
        return firstNot(isScoreStrip);
    }

    if (newKind == MixerStripRef::Kind::ChordSymbols) {
        return firstNot([](const MixerStripRef& s) { return s.kind == MixerStripRef::Kind::Instrument; });
    }

    if (std::find(scoreOrder.cbegin(), scoreOrder.cend(), newId) == scoreOrder.cend()) {
        return -1;
    }

    // Walk the score's order alongside the instrument strips that are there already
    int index = 0;
    for (const InstrumentTrackId& id : scoreOrder) {
        if (id == newId) {
            return index;
        }

        if (index < count && strips[index].kind == MixerStripRef::Kind::Instrument && strips[index].id == id) {
            ++index;
        }
    }

    return index;
}
