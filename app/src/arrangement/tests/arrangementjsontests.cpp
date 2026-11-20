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

#include "internal/arrangementjson.h"

using namespace mu::arrangement;
using namespace muse;
using mu::engraving::Fraction;

namespace {
ArrangementFileData sampleData()
{
    ArrangementFileData d;

    AudioAsset embedded;
    embedded.id = "0123456789abcdef0123456789abcdef01234567";
    embedded.name = u"Vocals take 3";
    embedded.format = "wav";
    embedded.storage = AssetStorage::Embedded;
    embedded.fileInProject = "audio/0123456789abcdef0123456789abcdef01234567.wav";
    embedded.originalPath = "/home/me/Takes/vocals 3.wav";
    embedded.sampleRate = 48000;
    embedded.channels = 2;
    embedded.frames = 5000000000ull; // > 2^32: a very long recording must survive the JSON writer
    embedded.offline = true;         // runtime only: must not be saved

    AudioAsset referenced;
    referenced.id = "fedcba9876543210fedcba9876543210fedcba98";
    referenced.name = u"Gitarr – ÅÄÖ";
    referenced.format = "wav";
    referenced.storage = AssetStorage::Referenced;
    referenced.relativePath = "../Recordings/gtr.wav";
    referenced.absolutePath = "/home/me/Recordings/gtr.wav";
    referenced.sampleRate = 44100;
    referenced.channels = 1;
    referenced.frames = 2345678;

    d.assets = { embedded, referenced };

    Track musical;
    musical.id = 1;
    musical.name = u"Vocals";
    musical.timeBase = TimeBase::Musical;
    musical.recordArmed = true;
    musical.inputChannel = 2;

    AudioClip c1;
    c1.id = 10;
    c1.asset = embedded.id;
    c1.startTick = Fraction(5, 4);
    c1.sourceOffset = 0.25;
    c1.length = 12.5;
    c1.gainDb = -3.5f;
    c1.fadeIn = { 0.01, FadeCurve::Linear };
    c1.fadeOut = { 0.05, FadeCurve::EqualPower };

    AudioClip c2;
    c2.id = 11;
    c2.asset = embedded.id;
    c2.name = u"Chorus";
    c2.startTick = Fraction(37, 8);
    c2.sourceOffset = 1.0 / 3.0; // not exact in 6 decimals
    c2.length = 2.0;
    c2.muted = true;
    c2.sourceTempoBpm = 97.5;

    musical.clips = { c1, c2 };

    Track absolute;
    absolute.id = 7;
    absolute.name = u"Sound effects";
    absolute.timeBase = TimeBase::Absolute;
    absolute.inputMonitoring = true;

    AudioClip c3;
    c3.id = 12;
    c3.asset = referenced.id;
    c3.startSecs = 61.125;
    c3.length = 3.0;
    absolute.clips = { c3 };

    d.tracks = { musical, absolute };
    return d;
}

void expectSameClip(const AudioClip& a, const AudioClip& b, TimeBase timeBase)
{
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.asset, b.asset);
    EXPECT_EQ(a.name, b.name);
    if (timeBase == TimeBase::Musical) {
        EXPECT_EQ(a.startTick, b.startTick);
    } else {
        EXPECT_EQ(a.startSecs.raw(), b.startSecs.raw());
    }
    EXPECT_EQ(a.sourceOffset.raw(), b.sourceOffset.raw()); // exact since FD-028 (FD-032)
    EXPECT_EQ(a.length.raw(), b.length.raw()); // exact since FD-028 (FD-032)
    EXPECT_FLOAT_EQ(a.gainDb, b.gainDb);
    EXPECT_EQ(a.fadeIn.length.raw(), b.fadeIn.length.raw()); // exact since FD-028 (FD-032)
    EXPECT_EQ(a.fadeIn.curve, b.fadeIn.curve);
    EXPECT_EQ(a.fadeOut.length.raw(), b.fadeOut.length.raw()); // exact since FD-028 (FD-032)
    EXPECT_EQ(a.fadeOut.curve, b.fadeOut.curve);
    EXPECT_EQ(a.muted, b.muted);
    EXPECT_DOUBLE_EQ(a.sourceTempoBpm, b.sourceTempoBpm);
}
}

TEST(Arrangement_JsonTests, RoundTrip)
{
    const ArrangementFileData original = sampleData();

    const ByteArray json = arrangementjson::write(original);
    RetVal<ArrangementFileData> read = arrangementjson::read(json);
    ASSERT_TRUE(read.ret) << read.ret.text();
    const ArrangementFileData& back = read.val;

    ASSERT_EQ(back.assets.size(), 2u);
    for (size_t i = 0; i < 2; ++i) {
        const AudioAsset& a = original.assets[i];
        const AudioAsset& b = back.assets[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.name, b.name);
        EXPECT_EQ(a.format, b.format);
        EXPECT_EQ(a.storage, b.storage);
        EXPECT_EQ(a.fileInProject, b.fileInProject);
        EXPECT_EQ(a.relativePath, b.relativePath);
        EXPECT_EQ(a.absolutePath, b.absolutePath);
        EXPECT_EQ(a.originalPath, b.originalPath);
        EXPECT_EQ(a.sampleRate, b.sampleRate);
        EXPECT_EQ(a.channels, b.channels);
        EXPECT_EQ(a.frames, b.frames);
        EXPECT_FALSE(b.offline);
        EXPECT_FALSE(b.modifiedOnDisk);
    }

    ASSERT_EQ(back.tracks.size(), 2u);
    for (size_t i = 0; i < 2; ++i) {
        const Track& a = original.tracks[i];
        const Track& b = back.tracks[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.kind, b.kind);
        EXPECT_EQ(a.name, b.name);
        EXPECT_EQ(a.timeBase, b.timeBase);
        EXPECT_EQ(a.recordArmed, b.recordArmed);
        EXPECT_EQ(a.inputMonitoring, b.inputMonitoring);
        EXPECT_EQ(a.inputChannel, b.inputChannel);
        ASSERT_EQ(a.clips.size(), b.clips.size());
        for (size_t j = 0; j < a.clips.size(); ++j) {
            expectSameClip(a.clips[j], b.clips[j], a.timeBase);
        }
    }

    // Writing what was read gives the same file: the format is stable.
    EXPECT_EQ(arrangementjson::write(back), json);
}

TEST(Arrangement_JsonTests, OnlyTheAuthoritativePositionIsWritten)
{
    const ByteArray bytes = arrangementjson::write(sampleData());
    const std::string json(bytes.constChar(), bytes.size());
    EXPECT_NE(json.find("\"startTick\": \"5\\/4\""), std::string::npos) << json; // the writer escapes '/'
    EXPECT_NE(json.find("\"startSecs\": 61.125"), std::string::npos) << json;
    EXPECT_EQ(json.find("offline"), std::string::npos);
    EXPECT_NE(json.find("\"version\": 1"), std::string::npos); // WAV only
}

// Batch 2 S1: the audio staff's part, written as a string (64-bit ids, FD-032) and only when set
TEST(Arrangement_JsonTests, PartIdIsWrittenWhenSetAndReadBack)
{
    ArrangementFileData data;
    Track withPart;
    withPart.id = 1;
    withPart.name = u"With part";
    withPart.partId = muse::ID(uint64_t(12345678901234ull));
    Track withoutPart;
    withoutPart.id = 2;
    withoutPart.name = u"Without";
    data.tracks = { withPart, withoutPart };

    const ByteArray json = arrangementjson::write(data);
    const std::string text(json.constChar(), json.size());
    EXPECT_NE(text.find("\"12345678901234\""), std::string::npos) << text; // a string, not a number
    EXPECT_EQ(text.find("\"partId\"", text.find("\"Without\"")), std::string::npos) << text;

    RetVal<ArrangementFileData> read = arrangementjson::read(json);
    ASSERT_TRUE(read.ret) << read.ret.text();
    ASSERT_EQ(read.val.tracks.size(), 2u);
    EXPECT_EQ(read.val.tracks[0].partId, withPart.partId);
    EXPECT_FALSE(read.val.tracks[1].partId.isValid());
}

TEST(Arrangement_JsonTests, MalformedPartIdIsIgnored)
{
    for (const char* partId : { "\"abc\"", "\"99999999999999999999999\"", "12345", "\"\"", "\"0\"", "\"12x\"" }) {
        const std::string text = std::string(R"({ "version": 1, "tracks": [ { "id": 1, "kind": "audio", "partId": )") + partId + " } ] }";
        RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(text.c_str()));
        ASSERT_TRUE(read.ret) << partId << ": " << read.ret.text();
        ASSERT_EQ(read.val.tracks.size(), 1u) << partId;
        EXPECT_FALSE(read.val.tracks[0].partId.isValid()) << partId;
    }

    RetVal<ArrangementFileData> ok = arrangementjson::read(
        ByteArray(R"({ "version": 1, "tracks": [ { "id": 1, "kind": "audio", "partId": "42" } ] })"));
    ASSERT_TRUE(ok.ret) << ok.ret.text();
    ASSERT_EQ(ok.val.tracks.size(), 1u);
    EXPECT_EQ(ok.val.tracks[0].partId, muse::ID(uint64_t(42)));
}

TEST(Arrangement_JsonTests, UnknownFieldsAreIgnored)
{
    const ByteArray json(
        R"({
        "version": 1,
        "futureTopLevel": { "x": 1 },
        "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "storage": "embedded", "file": "audio/da39a3ee5e6b4b0d3255bfef95601890afd80709.wav", "sampleRate": 48000,
                      "channels": 1, "frames": 480, "colour": "#ff0000" } ],
        "tracks": [ { "id": 3, "kind": "audio", "name": "T", "timeBase": "musical", "height": 120,
                      "clips": [ { "id": 4, "asset": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "startTick": "1/1", "sourceOffset": 0,
                                   "length": 0.01, "warpMarkers": [] } ] } ]
    })");

    RetVal<ArrangementFileData> read = arrangementjson::read(json);
    ASSERT_TRUE(read.ret) << read.ret.text();
    ASSERT_EQ(read.val.tracks.size(), 1u);
    ASSERT_EQ(read.val.tracks[0].clips.size(), 1u);
    EXPECT_EQ(read.val.tracks[0].clips[0].startTick, Fraction(1, 1));
    EXPECT_EQ(read.val.assets[0].frames, 480u);
}

TEST(Arrangement_JsonTests, MissingOrMistypedListsAreEmpty)
{
    // Keys that are missing or not arrays must not trip picojson's type asserts.
    for (const char* json : { R"({ "version": 1 })",
                              R"({ "version": 1, "assets": {}, "tracks": "none" })",
                              R"({ "version": 1, "tracks": [ { "id": 1 } ] })",
                              R"({ "version": 1, "tracks": [ { "id": 1, "clips": 3 } ] })" }) {
        RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(json));
        ASSERT_TRUE(read.ret) << json << ": " << read.ret.text();
        EXPECT_TRUE(read.val.assets.empty());
        for (const Track& t : read.val.tracks) {
            EXPECT_TRUE(t.clips.empty());
        }
    }

    // A clip without fade objects gets no fades
    const std::string noFadeObjects = std::string(R"({ "version": 1, "tracks": [ { "id": 1, "clips": [ )")
                                      + R"({ "id": 2, "asset": "a", "startTick": "1/4", "length": 1, "fadeIn": 7 } ] } ] })";
    RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(noFadeObjects.c_str()));
    ASSERT_TRUE(read.ret);
    EXPECT_DOUBLE_EQ(read.val.tracks[0].clips[0].fadeIn.length.raw(), 0.0);
}

TEST(Arrangement_JsonTests, UnsafeAssetIdsAndFormatsAreRefused)
{
    // Ids and formats become file names: a crafted project must not reach outside its folders.
    const char* unsafe[] = {
        R"({ "version": 1, "assets": [ { "id": "../escaped", "format": "wav", "storage": "embedded",
                                        "file": "audio/../escaped.wav" } ] })",
        R"({ "version": 1, "assets": [ { "id": "../../secret", "format": "txt", "storage": "referenced",
                                        "absolutePath": "/tmp/x" } ] })",
        R"({ "version": 1, "assets": [ { "id": "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709", "storage": "referenced",
                                        "absolutePath": "/tmp/x.wav" } ] })", // upper case
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd807090", "storage": "referenced",
                                        "absolutePath": "/tmp/x.wav" } ] })", // 41 characters
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "format": "../x", "storage": "embedded",
                                        "file": "audio/da39a3ee5e6b4b0d3255bfef95601890afd80709.../x" } ] })",
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "format": "wav", "storage": "embedded",
                                        "file": "audio/other.wav" } ] })", // file name not audio/<id>.<format>
    };

    for (const char* json : unsafe) {
        RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(json));
        EXPECT_FALSE(read.ret) << json;
    }
}

TEST(Arrangement_JsonTests, NewerVersionGivesAClearError)
{
    RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(R"({ "version": 3, "tracks": [] })"));
    EXPECT_EQ(read.ret.code(), static_cast<int>(Ret::Code::NotSupported));
    EXPECT_NE(read.ret.text().find("newer"), std::string::npos) << read.ret.text();
}

TEST(Arrangement_JsonTests, DamagedFilesAreRejectedNotCrashed)
{
    const char* damaged[] = {
        "",
        "not json",
        "[1, 2, 3]",
        R"({ "tracks": [] })", // no version
        R"({ "version": 1, "tracks": [ { "kind": "audio" } ] })", // track without id
        R"({ "version": 1, "tracks": [ { "id": 1, "kind": "midi" } ] })",
        R"({ "version": 1, "tracks": [ { "id": 1, "clips": [ { "id": 2, "asset": "a", "startTick": "x", "length": 1 } ] } ] })",
        R"({ "version": 1, "tracks": [ { "id": 1, "clips": [ { "id": 2, "asset": "a", "startTick": "1/4", "length": 0 } ] } ] })",
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "storage": "cloud" } ] })",
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "storage": "embedded" } ] })",
        R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "storage": "referenced" } ] })",
        R"({ "version": 1, "tracks": [ 5 ] })",
        R"({ "version": 1, "assets": [ "x" ] })",
    };

    for (const char* json : damaged) {
        RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(json));
        EXPECT_FALSE(read.ret) << json;
        EXPECT_FALSE(read.ret.text().empty()) << json;
    }
}

TEST(Arrangement_JsonTests, EveryAudioFormatRoundTrips)
{
    for (const char* format : { "wav", "flac", "mp3", "ogg" }) {
        ArrangementFileData data = sampleData();
        for (AudioAsset& a : data.assets) {
            a.format = format;
            if (a.storage == AssetStorage::Embedded) {
                a.fileInProject = embeddedFileName(a.id, a.format);
            }
        }
        RetVal<ArrangementFileData> read = arrangementjson::read(arrangementjson::write(data));
        ASSERT_TRUE(read.ret) << format << ": " << read.ret.text();
        ASSERT_EQ(read.val.assets.size(), data.assets.size());
        for (size_t i = 0; i < data.assets.size(); ++i) {
            EXPECT_EQ(read.val.assets[i].format, format);
            EXPECT_EQ(read.val.assets[i].fileInProject, data.assets[i].fileInProject);
        }
    }
}

TEST(Arrangement_JsonTests, UnknownAudioFormatsAreStillRefused)
{
    for (const char* format : { "aiff", "m4a", "opus", "WAV", "" }) {
        const std::string json = std::string(R"({ "version": 2, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "format": ")")
                                 + format + R"(", "storage": "referenced", "absolutePath": "/x" } ] })";
        EXPECT_FALSE(arrangementjson::read(ByteArray(json.c_str())).ret) << format;
    }
}

TEST(Arrangement_JsonTests, VersionTwoOnlyWhenAnAssetIsNotWav)
{
    // A WAV-only project stays version 1: builds from before FLAC, MP3 and OGG import still open it
    ArrangementFileData wavOnly = sampleData();
    EXPECT_EQ(arrangementjson::versionFor(wavOnly), 1);
    {
        const ByteArray bytes = arrangementjson::write(wavOnly);
        const std::string json(bytes.constChar(), bytes.size());
        EXPECT_NE(json.find("\"version\": 1"), std::string::npos) << json;
    }

    // One FLAC asset makes it version 2, which older builds refuse as "saved by a newer version"
    // (their FORMAT_VERSION is 1) instead of as damaged
    ArrangementFileData withFlac = sampleData();
    withFlac.assets[1].format = "flac";
    EXPECT_EQ(arrangementjson::versionFor(withFlac), 2);
    const ByteArray bytes = arrangementjson::write(withFlac);
    const std::string json(bytes.constChar(), bytes.size());
    EXPECT_NE(json.find("\"version\": 2"), std::string::npos) << json;
    EXPECT_TRUE(arrangementjson::read(bytes).ret);

    // No assets at all: version 1
    EXPECT_EQ(arrangementjson::versionFor(ArrangementFileData()), 1);

    // This build still reads version 1 files, also one that (wrongly) holds a FLAC asset
    const ByteArray v1Flac(R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "format": "flac", "storage": "referenced", "absolutePath": "/x.flac" } ] })");
    EXPECT_TRUE(arrangementjson::read(v1Flac).ret);
}

TEST(Arrangement_JsonTests, LargeIdsAndSampleRatesRoundTripExactly)
{
    // FD-032: ids and the sample rate went through int, which truncated values of 2^31 and more
    ArrangementFileData data = sampleData();
    data.assets[0].sampleRate = (uint64_t(1) << 31) + 5;
    data.assets[0].frames = (uint64_t(1) << 53) - 1;
    data.tracks[0].id = (uint64_t(1) << 31) + 1;
    data.tracks[0].clips[0].id = (uint64_t(1) << 40) + 3;
    data.tracks[0].clips[1].id = (uint64_t(1) << 53) - 1;

    RetVal<ArrangementFileData> read = arrangementjson::read(arrangementjson::write(data));
    ASSERT_TRUE(read.ret) << read.ret.text();
    EXPECT_EQ(read.val.assets[0].sampleRate, data.assets[0].sampleRate);
    EXPECT_EQ(read.val.assets[0].frames, data.assets[0].frames);
    EXPECT_EQ(read.val.tracks[0].id, data.tracks[0].id);
    EXPECT_EQ(read.val.tracks[0].clips[0].id, data.tracks[0].clips[0].id);
    EXPECT_EQ(read.val.tracks[0].clips[1].id, data.tracks[0].clips[1].id);
}

TEST(Arrangement_JsonTests, FramesAreReadFromAStringOrANumber)
{
    // Files from before FD-032 have "frames" as a string
    for (const char* frames : { R"("5000000000")", "5000000000" }) {
        const std::string json = std::string(R"({ "version": 1, "assets": [ { "id": "da39a3ee5e6b4b0d3255bfef95601890afd80709", "format": "wav", "storage": "referenced", "absolutePath": "/x.wav", "frames": )")
                                 + frames + " } ] }";
        RetVal<ArrangementFileData> read = arrangementjson::read(ByteArray(json.c_str()));
        ASSERT_TRUE(read.ret) << frames;
        ASSERT_EQ(read.val.assets.size(), 1u);
        EXPECT_EQ(read.val.assets[0].frames, 5000000000ull);
    }
}
