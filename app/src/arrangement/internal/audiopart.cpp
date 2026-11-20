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

#include "audiopart.h"

#include <algorithm>

#include "engraving/dom/factory.h"
#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"

using namespace mu::engraving;

namespace mu::arrangement {
muse::String audioPartShortName(const muse::String& trackName)
{
    muse::String name = trackName.trimmed();
    const size_t space = name.indexOf(u' ');
    if (space != muse::nidx && space > 0) {
        name = name.left(space);
    }
    if (name.size() <= MAX_SHORT_NAME) {
        return name;
    }
    return name.left(MAX_SHORT_NAME - 1) + u".";
}

Part* addAudioPart(Score* score, const muse::String& trackName)
{
    if (!score || !score->firstMeasure()) {
        return nullptr;
    }

    const muse::String shortName = audioPartShortName(trackName);

    Instrument instrument(FONTDAW_AUDIO_INSTRUMENT_ID);
    instrument.setMusicXmlId(FONTDAW_AUDIO_INSTRUMENT_ID);
    instrument.setTrackName(trackName);
    instrument.setLongName(trackName);
    instrument.setShortName(shortName);

    Part* part = new Part(score);
    part->setInstrument(instrument);
    part->setLongName(trackName);
    part->setShortName(shortName);

    score->undoInsertPart(part, score->parts().size());

    // Only settings MuseScore already has and saves (07-first-step.md, section 2)
    StaffType staffType = *StaffType::preset(StaffTypes::STANDARD);
    staffType.setGenClef(false);
    staffType.setGenKeysig(false);
    staffType.setGenTimesig(false);
    staffType.setInvisible(true);

    Staff* staff = Factory::createStaff(part);
    staff->setStaffType(Fraction(0, 1), staffType);
    staff->setHideWhenEmpty(AutoOnOff::OFF); // until S3 hides it where no clip sounds

    score->undoInsertStaff(staff, 0, /*createRests*/ true);

    const track_idx_t firstTrack = staff->idx() * VOICES;
    for (Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            EngravingItem* item = s->element(firstTrack + v);
            if (item && item->isRest() && item->visible()) {
                item->undoChangeProperty(Pid::VISIBLE, false);
            }
        }
    }

    return part;
}

const Part* findAudioPart(const Score* score, const Track& track)
{
    if (!score || !track.partId.isValid()) {
        return nullptr;
    }
    const Part* part = score->partById(track.partId);
    return isAudioPart(part) ? part : nullptr;
}

std::vector<const Part*> resolveAudioParts(const Score* score, const TrackList& tracks)
{
    std::vector<const Part*> result;
    result.reserve(tracks.size());
    std::vector<const Part*> taken;
    for (const Track& track : tracks) {
        const Part* part = findAudioPart(score, track);
        if (part && std::find(taken.begin(), taken.end(), part) != taken.end()) {
            part = nullptr; // already shown by an earlier track
        }
        if (part) {
            taken.push_back(part);
        }
        result.push_back(part);
    }
    return result;
}
}
