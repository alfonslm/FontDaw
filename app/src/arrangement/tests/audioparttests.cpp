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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QMimeData>

#include "engraving/dom/fontdawaudiostaff.h"
#include "engraving/editing/paste.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/internal/qmimedataadapter.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/tests/utils/scorerw.h"
#include "engraving/playback/playbackmodel.h"
#include "mpe/tests/mocks/articulationprofilesrepositorymock.h"

#include "global/io/file.h"
#include "global/types/translatablestring.h"

#include "internal/audiopart.h"

using namespace mu::arrangement;
using namespace mu::engraving;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {
MasterScore* readTestScore()
{
    return ScoreRW::readScore(u"data/repeats_tempo.mscx");
}

Part* addInOneCommand(Score* score, const muse::String& name)
{
    score->startCmd(muse::TranslatableString::untranslatable("add audio part"));
    Part* part = addAudioPart(score, name);
    score->endCmd();
    return part;
}

//! What 07-first-step.md, section 2, asks of an audio part; checked after creation and after a
//! save and reload.
void expectAudioPart(const Score* score, const Part* part, const muse::String& longName, const muse::String& shortName)
{
    ASSERT_TRUE(part);
    EXPECT_TRUE(isAudioPart(part));
    EXPECT_EQ(part->instrument()->musicXmlId(), muse::String(FONTDAW_AUDIO_INSTRUMENT_ID));
    EXPECT_EQ(part->longName(), longName);
    EXPECT_EQ(part->shortName(), shortName);
    ASSERT_EQ(part->nstaves(), 1u);

    const Staff* staff = part->staff(0);
    EXPECT_TRUE(isAudioStaff(staff));
    EXPECT_EQ(staff->hideWhenEmpty(), AutoOnOff::OFF);

    const StaffType* st = staff->staffType(Fraction(0, 1));
    ASSERT_TRUE(st);
    EXPECT_EQ(st->group(), StaffGroup::STANDARD);
    EXPECT_EQ(st->lines(), 5);
    EXPECT_FALSE(st->genClef());
    EXPECT_FALSE(st->genKeysig());
    EXPECT_FALSE(st->genTimesig());
    EXPECT_TRUE(st->invisible());
    EXPECT_TRUE(st->showBarlines());

    // One invisible rest per bar and nothing else on the staff
    const track_idx_t track = staff->idx() * VOICES;
    size_t rests = 0;
    for (const Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const EngravingItem* item = s->element(track + v);
            if (!item) {
                continue;
            }
            ASSERT_TRUE(item->isRest());
            EXPECT_FALSE(item->visible());
            ++rests;
        }
    }
    EXPECT_EQ(rests, score->nmeasures());
}
}

TEST(Arrangement_AudioPartTests, ShortNameIsTheTrackNameShortened)
{
    EXPECT_EQ(audioPartShortName(u"Studio take"), u"Studio");
    EXPECT_EQ(audioPartShortName(u"Drums"), u"Drums");
    EXPECT_EQ(audioPartShortName(u"Orchestra"), u"Orche.");
    EXPECT_EQ(audioPartShortName(u"  Vox  "), u"Vox");
    EXPECT_EQ(audioPartShortName(u""), u"");
}

TEST(Arrangement_AudioPartTests, AddsAnAudioStaffAtTheEndFromExistingSettingsOnly)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    const size_t partsBefore = score->parts().size();

    Part* part = addInOneCommand(score, u"Studio take");

    ASSERT_EQ(score->parts().size(), partsBefore + 1);
    EXPECT_EQ(score->parts().back(), part);
    expectAudioPart(score, part, u"Studio take", u"Studio");

    // Only the new part is an audio part
    for (const Part* p : score->parts()) {
        EXPECT_EQ(isAudioPart(p), p == part);
    }

    delete score;
}

TEST(Arrangement_AudioPartTests, ASaveAndReloadKeepsEverySetting)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    addInOneCommand(score, u"Studio take");

    const muse::String path = ScoreRW::rootPath() + u"/audiopart_saved.mscx";
    ASSERT_TRUE(ScoreRW::saveScore(score, path));
    delete score;

    MasterScore* reloaded = ScoreRW::readScore(path, true);
    ASSERT_TRUE(reloaded);
    expectAudioPart(reloaded, reloaded->parts().back(), u"Studio take", u"Studio");
    delete reloaded;
    muse::io::File::remove(path);
}

TEST(Arrangement_AudioPartTests, UndoRemovesThePartAgain)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    const size_t partsBefore = score->parts().size();
    const size_t stavesBefore = score->nstaves();

    addInOneCommand(score, u"Take");
    ASSERT_EQ(score->parts().size(), partsBefore + 1);

    EditData ed;
    score->undoRedo(true, &ed);
    EXPECT_EQ(score->parts().size(), partsBefore);
    EXPECT_EQ(score->nstaves(), stavesBefore);

    delete score;
}

TEST(Arrangement_AudioPartTests, FindsATracksPartOnlyWhenItIsAnAudioPart)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    Part* part = addInOneCommand(score, u"Take");

    Track track;
    EXPECT_FALSE(findAudioPart(score, track)); // no partId

    track.partId = part->id();
    EXPECT_EQ(findAudioPart(score, track), part);

    track.partId = score->parts().front()->id(); // an instrument's part
    EXPECT_FALSE(findAudioPart(score, track));

    delete score;
}

// One mixer strip (07, section 2, item 7): the playback model makes no instrument track for an
// audio part; the arrangement's audio track is its strip.
TEST(Arrangement_AudioPartTests, ThePlaybackModelMakesNoTrackForAnAudioPart)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    Part* audioPart = addInOneCommand(score, u"Take");
    const Part* instrumentPart = score->parts().front();

    auto repository = std::make_shared<NiceMock<muse::mpe::ArticulationProfilesRepositoryMock> >();
    ON_CALL(*repository, defaultProfile(_)).WillByDefault(Return(std::make_shared<muse::mpe::ArticulationsProfile>()));

    PlaybackModel model(muse::modularity::globalCtx());
    model.profilesRepository.set(repository);
    model.load(score);

    const InstrumentTrackIdSet tracks = model.existingTrackIdSet();
    bool hasInstrument = false;
    for (const InstrumentTrackId& id : tracks) {
        EXPECT_NE(id.partId, audioPart->id());
        hasInstrument = hasInstrument || id.partId == instrumentPart->id();
    }
    EXPECT_TRUE(hasInstrument); // the instrument parts still get theirs

    delete score;
}

// The paste guard (notationinteraction.cpp, pasteSelection) rolls a paste back when it put a
// chord on an audio staff. This checks that check against a real paste onto the audio staff,
// and that a paste onto an instrument leaves it false.
TEST(Arrangement_AudioPartTests, APasteOntoTheAudioStaffIsSeenByTheGuard)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    Part* audioPart = addInOneCommand(score, u"Take");
    score->doLayout();
    EXPECT_EQ(chordsOnAudioStaves(score), 0u);

    // Copy bar 1 of the first staff (four quarters)
    Measure* bar1 = score->firstMeasure();
    score->select(bar1, SelectType::SINGLE, 0);
    ASSERT_TRUE(score->selection().canCopy());
    QMimeData* mime = new QMimeData;
    mime->setData(score->selection().mimeType().toQString(), score->selection().mimeData().toQByteArray());
    QMimeDataAdapter adapter(mime);

    auto pasteAt = [&](staff_idx_t staffIdx) {
        EngravingItem* target = bar1->first(SegmentType::ChordRest)->element(staffIdx * VOICES);
        ASSERT_TRUE(target);
        score->select(target);
        score->startCmd(muse::TranslatableString::untranslatable("paste"));
        Paste::paste(score->transactionManager()->currentOrDummyTransaction(), score, &adapter, nullptr);
        score->endCmd();
    };

    // Onto the second instrument: no chord on the audio staff
    pasteAt(1);
    EXPECT_EQ(chordsOnAudioStaves(score), 0u);

    // Onto the audio staff: the guard's check sees it
    pasteAt(audioPart->staff(0)->idx());
    EXPECT_GT(chordsOnAudioStaves(score), 0u);

    delete mime;
    delete score;
}

TEST(Arrangement_AudioPartTests, NoChordsOnAudioStavesWithoutAudioParts)
{
    MasterScore* score = readTestScore();
    ASSERT_TRUE(score);
    EXPECT_EQ(chordsOnAudioStaves(score), 0u); // instruments have chords, but no audio staff exists
    EXPECT_EQ(chordsOnAudioStaves(nullptr), 0u);
    delete score;
}
