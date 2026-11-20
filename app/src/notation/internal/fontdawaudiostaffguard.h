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

// FontDaw (Batch 2 S1): what note input, paste and MIDI input do when an edit would put notes on
// an audio staff (src/engraving/dom/fontdawaudiostaff.h): refuse it and say why.

#include "modularity/ioc.h"
#include "interactive/iinteractive.h"
#include "translation.h"

namespace mu::notation {
inline void showAudioStaffRefusal(const muse::modularity::ContextPtr& ctx)
{
    if (const auto interactive = muse::modularity::ioc(ctx)->resolve<muse::IInteractive>("notation")) {
        interactive->info("", muse::trc("notation", "Notes cannot be added to an audio staff."));
    }
}
}
