/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#ifndef MU_ENGRAVING_PAUSEMAP_H
#define MU_ENGRAVING_PAUSEMAP_H

#include "../../dom/playedbeatmap.h"

/**
 \file
 Definition of class PauseMap.
*/

namespace mu::engraving {
class Score;

//---------------------------------------------------
//   PauseMap
//    MIDI files cannot contain pauses, so each pause is given
//    as hidden beats in the bar it ends (FD-102); the
//    played beat map holds them, see dom/playedbeatmap.h.
//---------------------------------------------------
class PauseMap
{
public:
    void calculate(const engraving::Score* s, bool expandRepeats);
    inline int tickWithPauses(int utick) const { return m_beatMap.playedTick(utick); }
    const PlayedBeatMap& beatMap() const { return m_beatMap; }

private:
    PlayedBeatMap m_beatMap;
};
} // namespace mu::engraving
#endif
