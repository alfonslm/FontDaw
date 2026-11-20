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

#include "modularity/imoduleinterface.h"

#include "pointertoolstypes.h"

namespace mu::pointertools {
//! What the ring menu holds for the selection, or the item under the pointer, in one kind of view.
class IRingMenuProvider : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IRingMenuProvider)

public:
    virtual ~IRingMenuProvider() = default;

    //! A short copy of one item of today's context menu, for the fallback (below). The QML side
    //! passes today's menu model's items; this keeps the provider free of QML types.
    struct TodayMenuItem {
        std::string id;
        muse::String title;
        muse::ui::IconCode::Code icon = muse::ui::IconCode::Code::NONE;
        bool enabled = true;
        bool isSeparatorOrSubmenu = false;
    };

    //! The ring for the score's current selection (internal/ringmenutables.cpp):
    //! - one note: the "one note" table;
    //! - two or more notes, or a range: the "several notes" table;
    //! - anything on an audio staff (Batch 2 S1): the "audio" table; clip commands come with S5;
    //! - other element types: the first RING_MAX_SLOTS - 1 enabled entries of today's menu for that
    //!   type (todayMenu, in order, separators and submenus skipped).
    //! Commands that are disabled now are left out (the segments close up). "More…" is always last.
    virtual RingContents scoreRing(const std::vector<TodayMenuItem>& todayMenu) const = 0;

    // TODO(FontDaw shortcuts 08b, 11): mixerRing(stripId), once the mixer's commands exist
    // (mute, solo, arm, reset fader, rename, colour); "More…" opens mixerpanelcontextmenumodel's menu.
};
}
