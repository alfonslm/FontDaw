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
#include "modularity/ioc.h"
#include "async/notification.h"
#include "draw/painter.h"

#include "notationtypes.h"

namespace mu::engraving {
class Score;
class Page;
}

namespace mu::notation {
//! FontDaw: paints what the score does not hold on audio staves (Batch 2 S1): the zero line, the
//! waveform symbol, the clips and their waveforms. The arrangement module implements it and
//! registers it per context; notation only calls it, so notation does not depend on the
//! arrangement. Optional: when nothing is registered (unit tests, other apps) nothing is drawn.
class IAudioStaffPainter : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IAudioStaffPainter)

public:
    virtual ~IAudioStaffPainter() = default;

    struct Options {
        bool isPrinting = false;
        bool invertColors = false; //! the notation is drawn light on dark (FD-072)
        muse::RectF frameRect;     //! the visible part of the page (page coordinates); empty: everything
        bool monochrome = false;   //! black-and-white output: a greyscale printer (S4)
        int deviceDpi = 0;         //! dots per inch of the output when printing; 0: unknown
    };

    //! Called once per page, after the page's items are painted, with the painter in that page's
    //! coordinates (canvas position minus `page->pos()`); `opt.frameRect` is in the same page
    //! coordinates.
    //! On screen this runs on the render thread while the GUI thread is blocked: read only, no
    //! subscriptions, no Qt objects, no notifications.
    virtual void paint(muse::draw::Painter* painter, const engraving::Score* score, const engraving::Page* page,
                       const Options& opt) const = 0;

    //! Before an output (print, PDF, PNG, SVG, video) is painted: called once per output by its
    //! writer, on the GUI thread, before the output's painter is created. Waits until the waveform
    //! peaks of every clip whose audio is present are built (they are made in the background after
    //! a project opens) and the audio tracks are loaded, or until `timeoutMs` has passed; user input
    //! is held back meanwhile. Clips that time out are remembered, so a later output does not wait
    //! for them again. Does nothing off the GUI thread (Batch 2 S4, step 7).
    virtual void prepareForOutput(int timeoutMs = 30000) const = 0;

    //! Something it draws changed (clips, audio found or lost, peaks ready). Notified on the GUI
    //! thread; the view repaints.
    virtual muse::async::Notification changed() const = 0;
};

//! For the writers: prepareForOutput() of the notation's audio painter, if one is registered.
inline void prepareAudioStavesForOutput(const muse::modularity::ContextPtr& ctx)
{
    if (const auto painter = muse::modularity::ioc(ctx)->resolve<IAudioStaffPainter>("notation")) {
        painter->prepareForOutput();
    }
}
}
