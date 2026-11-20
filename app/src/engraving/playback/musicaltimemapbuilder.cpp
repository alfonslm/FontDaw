/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "musicaltimemapbuilder.h"

#include "dom/score.h"
#include "types/constants.h"

#include "log.h"

using namespace mu::engraving;
using namespace muse::audio;

static double toQuarters(int ticks)
{
    return static_cast<double>(ticks) / Constants::DIVISION;
}

MusicalTimeMap MusicalTimeMapBuilder::build(const Score* score, bool expandRepeats)
{
    IF_ASSERT_FAILED(score) {
        return MusicalTimeMap();
    }

    return fromBeatMap(PlayedBeatMap::build(score, expandRepeats));
}

MusicalTimeMap MusicalTimeMapBuilder::build(const std::vector<TempoTimePoint>& tempoPoints, const std::vector<PlayedMeasure>& measures,
                                            double tempoMultiplier)
{
    return fromBeatMap(PlayedBeatMap::build(tempoPoints, measures, tempoMultiplier));
}

MusicalTimeMap MusicalTimeMapBuilder::fromBeatMap(const PlayedBeatMap& beats)
{
    MusicalTimeMap result;

    //! Played ticks are quarters since the start of the music; secs and bps already have the playback speed.
    //! The first point may be later than 0 s (a pause on the first tick): the music then stands at quarter 0 until it.
    result.tempo.reserve(beats.tempo().size());
    for (const PlayedBeatMap::TempoPoint& point : beats.tempo()) {
        result.tempo.push_back({ point.secs, toQuarters(point.tick), point.bps });
    }

    //! A bar's played time signature is what the MIDI export writes for it (4/4 with two hidden beats = 6/4),
    //! so plugins are told the same. Where MIDI cannot express the bar, the score's signature and the bar's length.
    result.bars.reserve(beats.bars().size());
    for (const PlayedBeatMap::BarPoint& bar : beats.bars()) {
        MusicalTimeMap::BarPoint point;
        point.quarters = toQuarters(bar.tick);

        if (bar.isRegular()) {
            point.numerator = bar.timeSig.numerator();
            point.denominator = bar.timeSig.denominator();
        } else if (bar.playedTimeSig.isValid() && bar.playedTimeSig.numerator() > 0) {
            point.numerator = bar.playedTimeSig.numerator();
            point.denominator = bar.playedTimeSig.denominator();
        } else {
            point.numerator = bar.timeSig.numerator();
            point.denominator = bar.timeSig.denominator();
            point.lengthQuarters = toQuarters(bar.ticks);
        }

        result.bars.push_back(point);
    }

    return result;
}
