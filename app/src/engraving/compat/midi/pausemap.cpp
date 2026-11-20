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

/**
 \file
 Implementation of class PauseMap.
*/

#include "pausemap.h"

#include "dom/score.h"

#include "log.h"

using namespace mu;
using namespace muse;

namespace mu::engraving {
//---------------------------------------------------------
//   calculate
//    MIDI files cannot contain pauses, so each pause is given as hidden beats
//    in the bar it ends. The beat map follows the same repeat list as the
//    rendered events.
//---------------------------------------------------------

void PauseMap::calculate(const Score* s, bool expandRepeats)
{
    IF_ASSERT_FAILED(s) {
        LOGE() << "failed to calculate pause map";
        return;
    }

    m_beatMap = PlayedBeatMap::build(s, expandRepeats);
}
}
