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

// A benchmark, not a check: how long the audio-staff painter takes for one page of a large score
// (FD-167). Disabled, so normal runs skip it; run it with
//   arrangement_test --gtest_also_run_disabled_tests --gtest_filter='*AudioStaffPainterBenchmark*'
// It generates its score: 26 instruments × 300 bars of quarter notes, an audio part, 16 clips of
// 4 s and one of 120 s that spans many systems. Times are printed, not compared.

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>

#include <QTemporaryDir>

#include "draw/bufferedpaintprovider.h"
#include "draw/painter.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/tests/utils/scorerw.h"
#include "global/types/translatablestring.h"

#include "internal/arrangement.h"
#include "internal/audioassetstore.h"
#include "internal/audiopart.h"
#include "internal/audiostaffpainter.h"
#include "mocks/arrangementconfigurationmock.h"
#include "testutils.h"

using namespace mu::arrangement;
using namespace mu::engraving;
using namespace muse;

namespace {
std::string largeScoreMscx(int instruments, int bars)
{
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<museScore version=\"4.00\">\n<Score>\n<Division>480</Division>\n";
    for (int p = 1; p <= instruments; ++p) {
        const std::string n = std::to_string(p);
        s += "<Part><Staff id=\"" + n + "\"><StaffType group=\"pitched\"><name>stdNormal</name></StaffType></Staff>"
             "<trackName>V" + n + "</trackName><Instrument id=\"violin\"><longName>Violin " + n + "</longName>"
             "<trackName>V" + n + "</trackName><instrumentId>strings.violin</instrumentId>"
             "<Channel><program value=\"40\"/></Channel></Instrument></Part>\n";
    }
    for (int p = 1; p <= instruments; ++p) {
        s += "<Staff id=\"" + std::to_string(p) + "\">\n";
        for (int b = 0; b < bars; ++b) {
            s += "<Measure><voice>";
            if (b == 0) {
                s += "<Clef><concertClefType>G</concertClefType><transposingClefType>G</transposingClefType></Clef>"
                     "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>";
            }
            for (int q = 0; q < 4; ++q) {
                s += "<Chord><durationType>quarter</durationType><Note><pitch>" + std::to_string(60 + (b + q + p) % 12)
                     + "</pitch><tpc>14</tpc></Note></Chord>";
            }
            s += "</voice></Measure>\n";
        }
        s += "</Staff>\n";
    }
    s += "</Score>\n</museScore>\n";
    return s;
}
}

TEST(AudioStaffPainterBenchmark, DISABLED_OnePageOfALargeScore)
{
    using Clock = std::chrono::steady_clock;
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    auto config = std::make_shared<testing::NiceMock<ArrangementConfigurationMock> >();
    ON_CALL(*config, defaultAssetStorage()).WillByDefault(testing::Return(AssetStorage::Embedded));
    ON_CALL(*config, audioSessionsPath()).WillByDefault(testing::Return(io::path_t(dir.filePath("sessions"))));
    modularity::globalIoc()->registerExport<IArrangementConfiguration>("painter_benchmark", config);
    auto ctx = std::make_shared<modularity::Context>(7);
    auto store = std::make_shared<AudioAssetStore>(ctx);
    modularity::ioc(ctx)->registerExport<IAudioAssetStore>("painter_benchmark", store);
    auto arrangement = std::make_shared<Arrangement>(ctx);
    arrangement->setTimeMap(TimeMap::constantTempo(120.0));
    modularity::ioc(ctx)->registerExport<IArrangement>("painter_benchmark", arrangement);

    const io::path_t wav(dir.filePath("long.wav"));
    ASSERT_TRUE(tests::writeFile(wav, tests::makeSineWav(130.0, 8000, 1)));
    RetVal<AudioAsset> asset = store->importFileSync(wav, AssetStorage::Embedded);
    ASSERT_TRUE(asset.ret) << asset.ret.text();
    ASSERT_TRUE(tests::waitFor([&]() { return store->peaks(asset.val.id) != nullptr; }, 60000)) << "no peaks";

    const io::path_t scorePath(dir.filePath("large.mscx"));
    const std::string mscx = largeScoreMscx(26, 300);
    ASSERT_TRUE(tests::writeFile(scorePath, ByteArray(mscx.data(), mscx.size())));
    MasterScore* score = ScoreRW::readScore(String::fromStdString(scorePath.toStdString()), true);
    ASSERT_TRUE(score);

    const ArrTrackId track = arrangement->addAudioTrack(u"Audio", -1, TimeBase::Musical).val;
    score->startCmd(TranslatableString::untranslatable("audio part"));
    Part* part = addAudioPart(score, u"Audio");
    score->endCmd();
    ASSERT_TRUE(part);
    arrangement->setTrackPart(track, part->id());
    AudioClip clip;
    clip.asset = asset.val.id;
    clip.length = 4.0;
    for (int k = 0; k < 16; ++k) {
        clip.startTick = Fraction(20 + 2 * k, 1);
        ASSERT_TRUE(arrangement->addClip(track, clip).ret);
    }
    clip.startTick = Fraction(60, 1);
    clip.length = 120.0;
    ASSERT_TRUE(arrangement->addClip(track, clip).ret);
    score->doLayout();
    ASSERT_GT(score->pages().size(), 20u);

    auto painter = std::make_shared<AudioStaffPainter>(ctx);
    const int rounds = 5;
    for (size_t pageIndex : { size_t(5), size_t(15) }) {
        const Page* page = score->pages()[pageIndex];
        double total = 0.0;
        for (int i = 0; i < rounds; ++i) {
            auto provider = std::make_shared<draw::BufferedPaintProvider>();
            draw::Painter p(provider, "benchmark");
            const auto start = Clock::now();
            painter->paint(&p, score, page, {});
            total += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        }
        std::cout << "BENCHMARK audio-staff painter, page " << pageIndex << " of " << score->pages().size()
                  << ": " << total / rounds << " ms per paint" << std::endl;
    }

    painter = nullptr;
    delete score;
    arrangement = nullptr;
    store = nullptr;
    modularity::globalIoc()->unregister<IArrangementConfiguration>("painter_benchmark");
    modularity::removeIoC(ctx);
}
