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

#include <set>

#include "modularity/ioc.h"
#include "async/asyncable.h"

#include "notation/iaudiostaffpainter.h"
#include "notation/inotationconfiguration.h"

#include "../iarrangement.h"
#include "../iarrangementcontroller.h"
#include "../iaudioassetstore.h"

namespace mu::arrangement {
//! Paints audio staves (Batch 2 S1; what it looks like: docs/fontdaw/batches/batch-02/s1-audio-staff.md,
//! "What the user sees", and 07's pictures):
//! - the zero line on the staff's middle line, the full width of each system;
//! - a short waveform outline where a clef would be;
//! - each clip: a frame (grey, faint fill), its name, its waveform in the RGB profile (three
//!   bands, 600 Hz / 4 kHz) with a 1 pt outline in the text colour;
//! - missing audio: a dashed frame with the clip's name;
//! - a clip past the final bar line: cut there, with an arrow scaled to the notes and the time left.
//! Continuous view (horizontal) on screen only, in S1.
class AudioStaffPainter : public notation::IAudioStaffPainter, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<IArrangement> arrangement = { this };
    muse::ContextInject<IAudioAssetStore> assetStore = { this };
    muse::ContextInject<IArrangementController> controller = { this };
    muse::GlobalInject<notation::INotationConfiguration> notationConfiguration;

public:
    AudioStaffPainter(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    //! GUI thread: subscribes to the arrangement and the asset store.
    void init();

    void paint(muse::draw::Painter* painter, const engraving::Score* score, const engraving::Page* page,
               const Options& opt) const override;
    void prepareForOutput(int timeoutMs = 30000) const override;
    muse::async::Notification changed() const override;

private:
    muse::async::Notification m_changed;

    // prepareForOutput(): what an earlier wait gave up on (GUI thread only)
    mutable std::set<AssetId> m_gaveUpOnPeaks;
    mutable bool m_gaveUpOnLoading = false;
};
}
