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

#include <vector>

#include "engraving/types/types.h"

namespace mu::playback {
//! FontDaw (FD-161): where the mixer puts the strip of a track that the engine adds while the mixer
//! is already shown. Upstream's MixerPanelModel::resolveInsertIndex() walked its strips with no bounds
//! check, relying on the master strip at the end to stop it; in a list without a master (built before
//! playback was set up) it read past the end. It also returned -1 for a track already listed, which
//! the model passed on to beginInsertRows(). This is that walk, bounds-safe and testable.
struct MixerStripRef {
    enum class Kind {
        Instrument,     // an instrument of the score (primary or secondary)
        ChordSymbols,
        Metronome,
        Other,          // aux, FontDaw audio track, master: everything after the instruments
    };

    Kind kind = Kind::Other;
    engraving::InstrumentTrackId id;
};

//! The index at which to insert the strip of newId, or -1 if it is already there or not in the score.
//! - scoreOrder: the instrument tracks in the order of the score's parts (as NotationParts lists them)
//! - the metronome goes before the first non-instrument strip; chord symbols after the instruments
int mixerStripInsertIndex(const std::vector<MixerStripRef>& strips, const std::vector<engraving::InstrumentTrackId>& scoreOrder,
                          const engraving::InstrumentTrackId& newId, MixerStripRef::Kind newKind);

//! Whether the mixer builds its strip list again once playback has loaded. While loading, the engine
//! adds the tracks one by one in the order it answers; once all are in, one rebuild puts the strips in
//! the score's order. A strip inserted into a loaded score (an instrument added later) is already in
//! its place, and isPlayAllowedChanged also fires on tab switches and part changes, where a rebuild
//! would only close pop-ups and drop focus.
class MixerRebuildAfterLoading
{
public:
    void stripInserted(bool loaded)
    {
        if (!loaded) {
            m_pending = true;
        }
    }

    void rebuilt() { m_pending = false; }

    bool shouldRebuild(bool playbackInited, bool loaded) const { return m_pending && playbackInited && loaded; }

private:
    bool m_pending = false;
};
}
