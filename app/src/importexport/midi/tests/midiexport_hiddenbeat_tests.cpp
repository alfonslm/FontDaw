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

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QString>

#include "global/types/string.h"

#include "engraving/compat/midi/midicoreevent.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/playedbeatmap.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/types/bps.h"

#include "engraving/tests/utils/scorerw.h"

#include "importexport/midi/internal/midiexport/exportmidi.h"
#include "importexport/midi/internal/midishared/midifile.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::iex::midi;

namespace {
const String MIDI_EXPORT_DATA_DIR(u"midiexport_data");
constexpr double TIME_TOLERANCE = 1e-4; // seconds
constexpr double ONE_TICK_SECS = 1.0 / 960; // at the 120 BPM of the test scores

struct PitchAtTick {
    int tick = 0;
    int pitch = 0;
};

struct TimeSigPoint {
    int numerator = 4;
    int denominator = 4;
};

/// What is read back from an exported file, using nothing but the file itself.
struct ExportedFile {
    int division = 0;
    std::map<int, int> tempoPoints;           // tick -> microseconds per quarter
    std::map<int, TimeSigPoint> timeSigs;     // tick -> time signature
    std::vector<int> noteOnTicks;             // sorted
    std::vector<PitchAtTick> noteOffs;        // sorted by tick (a note-on of velocity 0 or a note-off)
    std::vector<int> sustainReleaseTicks;     // sorted: sustain controller set below 64
    std::vector<int> barStarts;               // ticks, up to the last note

    double secs(int tick) const
    {
        double result = 0.0;
        int lastTick = 0;
        int usPerQuarter = 500000;
        for (const auto& [t, us] : tempoPoints) {
            if (t > tick) {
                break;
            }
            result += double(t - lastTick) * usPerQuarter / 1e6 / division;
            lastTick = t;
            usPerQuarter = us;
        }
        result += double(tick - lastTick) * usPerQuarter / 1e6 / division;
        return result;
    }

    const TimeSigPoint& timeSigAt(int tick) const
    {
        static const TimeSigPoint fallback;
        const TimeSigPoint* result = &fallback;
        for (const auto& [t, ts] : timeSigs) {
            if (t > tick) {
                break;
            }
            result = &ts;
        }
        return *result;
    }
};

ExportedFile readExported(MasterScore* score, bool expandRepeats = true)
{
    ExportedFile result;

    QBuffer buffer;
    EXPECT_TRUE(buffer.open(QIODevice::WriteOnly));
    EXPECT_TRUE(ExportMidi(score).write(&buffer, expandRepeats, /*exportRPNs*/ false));
    buffer.close();
    EXPECT_TRUE(buffer.open(QIODevice::ReadOnly));

    MidiFile file;
    try {
        EXPECT_TRUE(file.read(&buffer));
    } catch (const QString& error) {
        ADD_FAILURE() << "cannot read back the exported MIDI file: " << error.toStdString();
        return result;
    }

    result.division = file.division();
    EXPECT_GT(result.division, 0);
    if (result.division <= 0) {
        return result;
    }

    for (const MidiTrack& track : file.tracks()) {
        for (const auto& [tick, ev] : track.events()) {
            if (ev.type() == ME_NOTEON && ev.velo() > 0) {
                result.noteOnTicks.push_back(tick);
            } else if (ev.type() == ME_NOTEOFF || (ev.type() == ME_NOTEON && ev.velo() == 0)) {
                result.noteOffs.push_back({ tick, ev.pitch() });
            } else if (ev.type() == ME_CONTROLLER && ev.controller() == CTRL_SUSTAIN && ev.value() < 64) {
                result.sustainReleaseTicks.push_back(tick);
            } else if (ev.type() == ME_META && ev.metaType() == META_TEMPO && ev.len() == 3) {
                const uchar* d = ev.edata();
                result.tempoPoints[tick] = (d[0] << 16) | (d[1] << 8) | d[2];
            } else if (ev.type() == ME_META && ev.metaType() == META_TIME_SIGNATURE && ev.len() >= 2) {
                const uchar* d = ev.edata();
                result.timeSigs[tick] = { d[0], 1 << d[1] };
            }
        }
    }
    std::sort(result.noteOnTicks.begin(), result.noteOnTicks.end());
    std::sort(result.noteOffs.begin(), result.noteOffs.end(), [](const PitchAtTick& a, const PitchAtTick& b) {
        return a.tick < b.tick;
    });
    std::sort(result.sustainReleaseTicks.begin(), result.sustainReleaseTicks.end());

    if (result.noteOnTicks.empty()) {
        return result;
    }

    // bar k+1 starts where bar k ends, by the time signature in force at bar k's start
    const int lastNoteTick = result.noteOnTicks.back();
    int barStart = 0;
    while (barStart <= lastNoteTick) {
        result.barStarts.push_back(barStart);
        const TimeSigPoint& ts = result.timeSigAt(barStart);
        barStart += ts.numerator * (4 * result.division / ts.denominator);
    }

    return result;
}

struct ScoreMeasureAt {
    const Measure* measure = nullptr;
    int utick = 0;
};

std::vector<ScoreMeasureAt> playedMeasures(const MasterScore* score, bool expandRepeats = true)
{
    std::vector<ScoreMeasureAt> result;
    for (const RepeatSegment* rs : score->repeatList(expandRepeats)) {
        for (const Measure* m : rs->measureList()) {
            result.push_back({ m, m->tick().ticks() + rs->utick - rs->tick });
        }
    }
    return result;
}

/// Times of all note-ons the score should produce (tied-back notes make none), sorted.
std::vector<double> expectedNoteOnTimes(const MasterScore* score, bool expandRepeats = true)
{
    const TempoTimeline& timeline = score->tempoTimeline(expandRepeats);
    std::vector<double> result;
    for (const RepeatSegment* rs : score->repeatList(expandRepeats)) {
        for (const Measure* m : rs->measureList()) {
            for (const Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
                for (size_t track = 0; track < score->ntracks(); ++track) {
                    const EngravingItem* e = seg->element(track);
                    if (!e || !e->isChord()) {
                        continue;
                    }
                    const int utick = seg->tick().ticks() + rs->utick - rs->tick;
                    for (const Note* note : toChord(e)->notes()) {
                        if (!note->tieBack()) {
                            result.push_back(timeline.utick2utime(utick));
                        }
                    }
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
}

class MidiExportHiddenBeatTests : public ::testing::Test
{
protected:
    struct Result {
        std::unique_ptr<MasterScore> score;
        ExportedFile file;
    };

    /// speed: 1.0 is 100 %
    static Result exportScore(const std::string& name, double speed = 1.0, bool expandRepeats = true)
    {
        Result r;
        r.score.reset(ScoreRW::readScore(MIDI_EXPORT_DATA_DIR + u'/' + String::fromUtf8(name.c_str()) + u".mscx"));
        EXPECT_TRUE(r.score);
        if (!r.score) {
            return r;
        }
        r.score->doLayout();
        r.score->rebuildMidiMapping();

        if (speed != 1.0) {
            // as NotationPlayback::setTempoMultiplier, without the playback model
            r.score->setTempoMultiplier(BeatsPerSecond(speed));
            r.score->updateRepeatListTempo();
        }

        r.file = readExported(r.score.get(), expandRepeats);
        return r;
    }

    /// Where the hidden beats of the pause at `pauseUtick` start, in seconds: playback ends a note that
    /// ends on the pause there (unless HOLD_NOTES_THROUGH_PAUSES), and the pause is over at utick2utime.
    static double hiddenBeatsStartSecs(const MasterScore* score, int pauseUtick)
    {
        const TempoTimeline& timeline = score->tempoTimeline(true);
        for (const TempoTimePoint& point : timeline.points()) {
            if (point.utick == pauseUtick && point.pause > 0.0) {
                return timeline.utick2utime(pauseUtick) - point.pause / timeline.tempoMultiplier().val;
            }
        }
        ADD_FAILURE() << "no pause at utick " << pauseUtick;
        return 0.0;
    }

    /// Bar starts and note-on times of the file equal the times of playback.
    static void checkTimes(const std::string& name, double speed = 1.0, bool expandRepeats = true)
    {
        const Result r = exportScore(name, speed, expandRepeats);
        ASSERT_TRUE(r.score);
        ASSERT_FALSE(r.file.noteOnTicks.empty());

        const TempoTimeline& timeline = r.score->tempoTimeline(expandRepeats);
        const std::vector<ScoreMeasureAt> measures = playedMeasures(r.score.get(), expandRepeats);

        ASSERT_EQ(r.file.barStarts.size(), measures.size()) << name << ": bars in the file vs measures played";

        for (size_t k = 0; k < measures.size(); ++k) {
            EXPECT_NEAR(r.file.secs(r.file.barStarts[k]), timeline.utick2utime(measures[k].utick), TIME_TOLERANCE)
                << name << ": bar " << k << " (measure utick " << measures[k].utick << ")";
        }

        const std::vector<double> expected = expectedNoteOnTimes(r.score.get(), expandRepeats);
        ASSERT_EQ(r.file.noteOnTicks.size(), expected.size()) << name << ": note-ons in the file vs notes played";

        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_NEAR(r.file.secs(r.file.noteOnTicks[i]), expected[i], TIME_TOLERANCE)
                << name << ": note-on " << i << " (file tick " << r.file.noteOnTicks[i] << ")";
        }
    }
};

TEST_F(MidiExportHiddenBeatTests, testPauses)
{
    checkTimes("testPauses");
}

TEST_F(MidiExportHiddenBeatTests, testPausesRepeats)
{
    checkTimes("testPausesRepeats");
}

TEST_F(MidiExportHiddenBeatTests, testPausesTempoTimesigChange)
{
    checkTimes("testPausesTempoTimesigChange");
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeat)
{
    checkTimes("testHiddenBeat");
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeat_50percent)
{
    checkTimes("testHiddenBeat", 0.5);
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeat_150percent)
{
    checkTimes("testHiddenBeat", 1.5);
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeat_barsKeepNumbers)
{
    const Result r = exportScore("testHiddenBeat");
    ASSERT_TRUE(r.score);
    ASSERT_FALSE(r.file.barStarts.empty());

    // the old export inserted a whole bar per pause
    EXPECT_EQ(r.file.barStarts.size(), playedMeasures(r.score.get()).size());

    // bars of the first pass: 0 pickup (1/4 + 0.5 s), 1 caesura 2 s, 2 two breaths (0.5 s + 1 s)
    ASSERT_GE(r.file.barStarts.size(), 3u);
    const struct {
        size_t bar;
        int numerator;
        int denominator;
    } expected[] = { { 0, 2, 4 }, { 1, 8, 4 }, { 2, 7, 4 } };
    for (const auto& e : expected) {
        const TimeSigPoint& ts = r.file.timeSigAt(r.file.barStarts[e.bar]);
        EXPECT_EQ(ts.numerator, e.numerator) << "bar " << e.bar;
        EXPECT_EQ(ts.denominator, e.denominator) << "bar " << e.bar;
    }
}

TEST_F(MidiExportHiddenBeatTests, testPausesRepeats_withoutExpandingRepeats)
{
    // the pause map follows the export's repeat setting: bars and note-ons of the file equal the times of
    // the score's unexpanded tempo timeline and measures of repeatList(false) (FD-102)
    checkTimes("testPausesRepeats", 1.0, false);
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeat_noteOffsBeforeThePause)
{
    // A note that ends at a breath or caesura is released where the pause starts, not after its hidden beats.
    // Playback ends a note one tick before its end (and the instrument's gate time shortens it), so the release
    // is at the pause's start or a little before it, never after it (HOLD_NOTES_THROUGH_PAUSES is false).
    // Pickup: pitch 60, utick 0..480, breath 0.5 s at 480. Bar 3: pitch 72 ends at utick 3840, breath 1 s.
    // The release is within 0.05 s before the pause: a quarter note's gate time is 95 % (0.0125 s at 120 BPM).
    const Result r = exportScore("testHiddenBeat");
    ASSERT_TRUE(r.score);

    const struct {
        int pitch;
        int pauseUtick;
    } cases[] = { { 60, 480 }, { 72, 3840 } };

    for (const auto& c : cases) {
        const auto off = std::find_if(r.file.noteOffs.begin(), r.file.noteOffs.end(), [&c](const PitchAtTick& n) {
            return n.pitch == c.pitch;
        });
        ASSERT_NE(off, r.file.noteOffs.end()) << "pitch " << c.pitch;

        const double hiddenStart = hiddenBeatsStartSecs(r.score.get(), c.pauseUtick);
        const double expectedEnd = HOLD_NOTES_THROUGH_PAUSES ? r.score->tempoTimeline(true).utick2utime(c.pauseUtick) : hiddenStart;
        const double offSecs = r.file.secs(off->tick);

        EXPECT_LE(offSecs, expectedEnd + ONE_TICK_SECS * 0.5) << "pitch " << c.pitch << ", note-off at tick " << off->tick;
        EXPECT_GT(offSecs, expectedEnd - 0.05) << "pitch " << c.pitch << ", note-off at tick " << off->tick;
    }
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeatPedal_releaseAtThePause)
{
    // The pedal ends at the bar line where a 1 s breath sits: it is released where the hidden beats start,
    // not after them (the renderer puts the release one tick after the pedal's end, as upstream does)
    const Result r = exportScore("testHiddenBeatPedal");
    ASSERT_TRUE(r.score);
    ASSERT_EQ(r.file.sustainReleaseTicks.size(), 1u);

    const double hiddenStart = hiddenBeatsStartSecs(r.score.get(), 1920);
    EXPECT_NEAR(r.file.secs(r.file.sustainReleaseTicks.front()), hiddenStart, 1.5 * ONE_TICK_SECS);
}

TEST_F(MidiExportHiddenBeatTests, testHiddenBeatVolta_timesEqualPlayback)
{
    checkTimes("testHiddenBeatVolta");
}

TEST_F(MidiExportHiddenBeatTests, DISABLED_testHiddenBeatVolta_pauseOnlyAfterVolta1)
{
    // Measures: 1 | 1st ending (2, 3, a 1 s breath at the end of 3) | 2nd ending (4, 5). Played: 1 2 3, 1 4 5.
    // The breath belongs after measure 3 once, at utick 5760. Disabled: ScoreAutomationController::collectPauses
    // also records the pause at the start of the measure after the breath in the score (measure 4) on the
    // second pass, utick 7680 (upstream behaviour; the breath is read again as a pause "from the end of the previous measure").
    const Result r = exportScore("testHiddenBeatVolta");
    ASSERT_TRUE(r.score);

    const PlayedBeatMap map = PlayedBeatMap::build(r.score.get(), true);
    ASSERT_EQ(map.hiddenBeats().size(), 1u);
    EXPECT_EQ(map.hiddenBeats()[0].utick, 5760);
}
