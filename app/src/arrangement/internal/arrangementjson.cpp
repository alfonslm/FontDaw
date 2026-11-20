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

#include "arrangementjson.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

#include "global/serialization/json.h"
#include "global/translation.h"

#include "log.h"

using namespace mu::arrangement;
using namespace muse;

namespace {
// --- enums <-> strings -------------------------------------------------------------------

const char* toString(AssetStorage s)
{
    return s == AssetStorage::Referenced ? "referenced" : "embedded";
}

const char* toString(TimeBase t)
{
    return t == TimeBase::Absolute ? "absolute" : "musical";
}

const char* toString(FadeCurve c)
{
    return c == FadeCurve::EqualPower ? "equalPower" : "linear";
}

// Ids are JSON numbers. The JSON writer writes whole numbers in full up to 2^53, the largest a
// double holds exactly (FD-028, FD-032); ids are counters, far below that.
double idToJson(uint64_t id)
{
    return static_cast<double>(id);
}

uint64_t idFromJson(const JsonValue& v)
{
    const double d = v.toDouble();
    return d > 0 ? static_cast<uint64_t>(d) : 0;
}

uint64_t uint64FromJson(const JsonValue& v)
{
    if (v.isString()) {
        try {
            return static_cast<uint64_t>(std::stoull(v.toStdString()));
        } catch (...) {
            return 0;
        }
    }
    const double d = v.toDouble();
    return d > 0 ? static_cast<uint64_t>(d) : 0;
}

//! The elements of an array value; none if the value is missing or not an array.
//! (JsonValue::toArray() on anything else asserts in picojson.)
std::vector<JsonValue> elements(const JsonValue& v)
{
    std::vector<JsonValue> result;
    if (!v.isArray()) {
        return result;
    }
    const JsonArray arr = v.toArray();
    for (size_t i = 0; i < arr.size(); ++i) {
        result.push_back(arr.at(i));
    }
    return result;
}

Ret badData(const std::string& detail)
{
    LOGE() << "arrangement.json: " << detail;
    return Ret(static_cast<int>(Ret::Code::BadData),
               trc("arrangement", "The arrangement data in this project is damaged and could not be read."));
}

// --- write ---------------------------------------------------------------------------------

JsonObject fadeToJson(const Fade& f)
{
    JsonObject o;
    o.set("length", f.length.raw());
    o.set("curve", toString(f.curve));
    return o;
}

JsonObject assetToJson(const AudioAsset& a)
{
    JsonObject o;
    o.set("id", a.id);
    o.set("name", a.name);
    o.set("format", a.format);
    o.set("storage", toString(a.storage));
    if (a.storage == AssetStorage::Embedded) {
        o.set("file", a.fileInProject);
    } else {
        o.set("relativePath", a.relativePath.toStdString());
        o.set("absolutePath", a.absolutePath.toStdString());
    }
    if (!a.originalPath.empty()) {
        o.set("originalPath", a.originalPath.toStdString());
    }
    o.set("sampleRate", static_cast<double>(a.sampleRate));
    o.set("channels", a.channels);
    // A number since FD-032 (exact up to 2^53 frames, about 6000 years at 48 kHz). Files written
    // before have a string here: the reader takes both, and so do builds from before the change.
    o.set("frames", static_cast<double>(a.frames));
    return o;
}

JsonObject clipToJson(const mu::arrangement::AudioClip& c, TimeBase timeBase)
{
    JsonObject o;
    o.set("id", idToJson(c.id));
    o.set("asset", c.asset);
    if (!c.name.empty()) {
        o.set("name", c.name);
    }
    if (timeBase == TimeBase::Musical) {
        o.set("startTick", c.startTick.toString());
    } else {
        o.set("startSecs", c.startSecs.raw());
    }
    o.set("sourceOffset", c.sourceOffset.raw());
    o.set("length", c.length.raw());
    o.set("gainDb", static_cast<double>(c.gainDb));
    o.set("fadeIn", fadeToJson(c.fadeIn));
    o.set("fadeOut", fadeToJson(c.fadeOut));
    if (c.muted) {
        o.set("muted", true);
    }
    if (c.sourceTempoBpm > 0.0) {
        o.set("sourceTempoBpm", c.sourceTempoBpm);
    }
    return o;
}

JsonObject trackToJson(const Track& t)
{
    JsonObject o;
    o.set("id", idToJson(t.id));
    o.set("kind", "audio");
    o.set("name", t.name);
    if (t.partId.isValid()) {
        // A string: part ids are 64-bit (FD-032). Optional: readers before S1 ignore it.
        o.set("partId", t.partId.toStdString());
    }
    o.set("timeBase", toString(t.timeBase));
    if (t.recordArmed) {
        o.set("recordArmed", true);
    }
    if (t.inputMonitoring) {
        o.set("inputMonitoring", true);
    }
    if (t.inputChannel != 0) {
        o.set("inputChannel", t.inputChannel);
    }

    JsonArray clips;
    for (const mu::arrangement::AudioClip& c : t.clips) {
        clips.append(clipToJson(c, t.timeBase));
    }
    o.set("clips", clips);
    return o;
}

// --- read ----------------------------------------------------------------------------------

Fade fadeFromJson(const JsonObject& o)
{
    Fade f;
    f.length = std::max(0.0, o.value("length", 0.0).toDouble());
    f.curve = o.value("curve").toStdString() == "equalPower" ? FadeCurve::EqualPower : FadeCurve::Linear;
    return f;
}

RetVal<AudioAsset> assetFromJson(const JsonObject& o)
{
    AudioAsset a;
    a.id = o.value("id").toStdString();
    if (!isValidAssetId(a.id)) {
        return RetVal<AudioAsset>::make_ret(badData("asset id is not a 40-character hex hash"));
    }
    a.name = o.value("name").toString();
    a.format = o.value("format", "wav").toStdString();
    if (!isSupportedAssetFormat(a.format)) {
        return RetVal<AudioAsset>::make_ret(badData("unsupported asset format: " + a.format));
    }

    const std::string storage = o.value("storage").toStdString();
    if (storage == "embedded") {
        a.storage = AssetStorage::Embedded;
        a.fileInProject = o.value("file").toStdString();
        if (a.fileInProject != embeddedFileName(a.id, a.format)) {
            return RetVal<AudioAsset>::make_ret(badData("embedded asset file is not audio/<id>.<format>: " + a.id));
        }
    } else if (storage == "referenced") {
        a.storage = AssetStorage::Referenced;
        a.relativePath = o.value("relativePath").toStdString();
        a.absolutePath = o.value("absolutePath").toStdString();
        if (a.relativePath.empty() && a.absolutePath.empty()) {
            return RetVal<AudioAsset>::make_ret(badData("referenced asset without path: " + a.id));
        }
    } else {
        return RetVal<AudioAsset>::make_ret(badData("unknown asset storage: " + storage));
    }

    a.originalPath = o.value("originalPath").toStdString();
    a.sampleRate = static_cast<uint64_t>(std::max(0.0, o.value("sampleRate", 0).toDouble()));
    a.channels = o.value("channels", 0).toInt();
    a.frames = uint64FromJson(o.value("frames"));
    return RetVal<AudioAsset>::make_ok(a);
}

RetVal<mu::arrangement::AudioClip> clipFromJson(const JsonObject& o, TimeBase timeBase)
{
    mu::arrangement::AudioClip c;
    c.id = idFromJson(o.value("id"));
    c.asset = o.value("asset").toStdString();
    if (c.id == INVALID_CLIP || c.asset.empty()) {
        return RetVal<mu::arrangement::AudioClip>::make_ret(badData("clip without id or asset"));
    }
    c.name = o.value("name").toString();

    if (timeBase == TimeBase::Musical) {
        bool ok = false;
        c.startTick = mu::engraving::Fraction::fromString(o.value("startTick").toString(), &ok);
        if (!ok || !c.startTick.isValid() || c.startTick.negative()) {
            return RetVal<mu::arrangement::AudioClip>::make_ret(badData("clip with a bad startTick"));
        }
        c.startTick.reduce();
    } else {
        c.startSecs = std::max(0.0, o.value("startSecs", 0.0).toDouble());
    }

    c.sourceOffset = std::max(0.0, o.value("sourceOffset", 0.0).toDouble());
    c.length = o.value("length", 0.0).toDouble();
    if (!(c.length > 0.0)) {
        return RetVal<mu::arrangement::AudioClip>::make_ret(badData("clip with no length"));
    }
    c.gainDb = static_cast<float>(o.value("gainDb", 0.0).toDouble());
    c.fadeIn = fadeFromJson(o.value("fadeIn").toObject());
    c.fadeOut = fadeFromJson(o.value("fadeOut").toObject());
    c.muted = o.value("muted", false).toBool();
    c.sourceTempoBpm = std::max(0.0, o.value("sourceTempoBpm", 0.0).toDouble());
    return RetVal<mu::arrangement::AudioClip>::make_ok(c);
}

RetVal<Track> trackFromJson(const JsonObject& o)
{
    Track t;
    t.id = idFromJson(o.value("id"));
    if (t.id == INVALID_TRACK) {
        return RetVal<Track>::make_ret(badData("track without id"));
    }

    const std::string kind = o.value("kind", "audio").toStdString();
    if (kind != "audio") {
        return RetVal<Track>::make_ret(badData("unknown track kind: " + kind));
    }
    t.kind = TrackKind::Audio;
    t.name = o.value("name").toString();
    if (o.contains("partId")) {
        // Parsed here, not with muse::ID(std::string): its std::stoul throws on bad text.
        const JsonValue partIdValue = o.value("partId");
        bool ok = false;
        uint64_t value = 0;
        if (partIdValue.isString()) {
            const std::string text = partIdValue.toStdString();
            const char* end = text.data() + text.size();
            const std::from_chars_result r = std::from_chars(text.data(), end, value);
            ok = r.ec == std::errc() && r.ptr == end && !text.empty() && value != 0;
        }
        if (ok) {
            t.partId = muse::ID(value);
        } else {
            LOGW() << "track " << t.id << ": ignoring malformed partId";
        }
    }
    t.timeBase = o.value("timeBase").toStdString() == "absolute" ? TimeBase::Absolute : TimeBase::Musical;
    t.recordArmed = o.value("recordArmed", false).toBool();
    t.inputMonitoring = o.value("inputMonitoring", false).toBool();
    t.inputChannel = o.value("inputChannel", 0).toInt();

    for (const JsonValue& clip : elements(o.value("clips"))) {
        RetVal<mu::arrangement::AudioClip> c = clipFromJson(clip.toObject(), t.timeBase);
        if (!c.ret) {
            return RetVal<Track>::make_ret(c.ret);
        }
        t.clips.push_back(c.val);
    }
    return RetVal<Track>::make_ok(t);
}
}

int arrangementjson::versionFor(const ArrangementFileData& data)
{
    for (const AudioAsset& a : data.assets) {
        if (a.format != "wav") {
            return FORMAT_VERSION;
        }
    }
    return WAV_ONLY_VERSION;
}

ByteArray arrangementjson::write(const ArrangementFileData& data)
{
    JsonObject root;
    root.set("version", versionFor(data));

    JsonArray assets;
    for (const AudioAsset& a : data.assets) {
        assets.append(assetToJson(a));
    }
    root.set("assets", assets);

    JsonArray tracks;
    for (const Track& t : data.tracks) {
        tracks.append(trackToJson(t));
    }
    root.set("tracks", tracks);

    return JsonDocument(root).toJson(JsonDocument::Format::Indented);
}

RetVal<ArrangementFileData> arrangementjson::read(const ByteArray& json)
{
    std::string err;
    const JsonDocument doc = JsonDocument::fromJson(json, &err);
    if (!err.empty() || !doc.isObject()) {
        return RetVal<ArrangementFileData>::make_ret(badData("not a JSON object: " + err));
    }

    const JsonObject root = doc.rootObject();
    const JsonValue version = root.value("version");
    if (!version.isNumber()) {
        return RetVal<ArrangementFileData>::make_ret(badData("no version"));
    }
    if (version.toInt() > FORMAT_VERSION) {
        return RetVal<ArrangementFileData>::make_ret(
            Ret(static_cast<int>(Ret::Code::NotSupported),
                trc("arrangement", "This project's audio tracks were saved by a newer version of FontDaw. "
                                   "Update FontDaw to open it.")));
    }

    ArrangementFileData data;

    for (const JsonValue& asset : elements(root.value("assets"))) {
        RetVal<AudioAsset> a = assetFromJson(asset.toObject());
        if (!a.ret) {
            return RetVal<ArrangementFileData>::make_ret(a.ret);
        }
        data.assets.push_back(a.val);
    }

    for (const JsonValue& track : elements(root.value("tracks"))) {
        RetVal<Track> t = trackFromJson(track.toObject());
        if (!t.ret) {
            return RetVal<ArrangementFileData>::make_ret(t.ret);
        }
        data.tracks.push_back(t.val);
    }

    return RetVal<ArrangementFileData>::make_ok(data);
}
