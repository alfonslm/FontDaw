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

#include "arrangement.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "global/translation.h"

#include "arrangementjson.h"

#include "log.h"

using namespace mu::arrangement;
using namespace muse;
using mu::engraving::Fraction;

namespace {
constexpr double EPS = arrangementjson::SECS_EPSILON;

Ret badArgs(const std::string& text)
{
    return Ret(static_cast<int>(Ret::Code::BadArgs), text);
}

Ret noTrack()
{
    return badArgs("unknown track");
}

Ret noClip()
{
    return badArgs("unknown clip");
}

Ret overlapError()
{
    return badArgs(trc("arrangement", "Clips on the same track cannot overlap."));
}

Fade clampFade(const Fade& f, double maxLength)
{
    Fade r = f;
    r.length = std::clamp(f.length.raw(), 0.0, std::max(0.0, maxLength));
    return r;
}
}

TimeMap TimeMap::constantTempo(double quarterBpm)
{
    // A Fraction counts whole notes; a whole note is 4 quarters.
    const double secsPerWhole = 4.0 * 60.0 / quarterBpm;
    TimeMap map;
    map.tickToSecs = [secsPerWhole](const Fraction& tick) {
        return secs_t(tick.toDouble() * secsPerWhole);
    };
    map.secsToTick = [secsPerWhole](secs_t secs) {
        const double ticks = secs.raw() / secsPerWhole * (mu::engraving::Constants::DIVISION * 4);
        return Fraction::fromTicks(static_cast<int>(std::lround(ticks)));
    };
    return map;
}

Arrangement::Arrangement(const modularity::ContextPtr& ctx)
    : Contextable(ctx), m_timeMap(TimeMap::constantTempo(120.0))
{
}

// --- Tracks ------------------------------------------------------------------------------------

const TrackList& Arrangement::tracks() const
{
    return m_tracks;
}

const Track* Arrangement::track(ArrTrackId id) const
{
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(), [id](const Track& t) { return t.id == id; });
    return it != m_tracks.end() ? &(*it) : nullptr;
}

Track* Arrangement::mutableTrack(ArrTrackId id)
{
    return const_cast<Track*>(track(id));
}

AudioClip* Arrangement::mutableClip(Track& t, ClipId clipId)
{
    auto it = std::find_if(t.clips.begin(), t.clips.end(), [clipId](const AudioClip& c) { return c.id == clipId; });
    return it != t.clips.end() ? &(*it) : nullptr;
}

RetVal<ArrTrackId> Arrangement::addAudioTrack(const String& name, int atIndex, TimeBase timeBase)
{
    Track t;
    t.id = ++m_lastTrackId;
    t.kind = TrackKind::Audio;
    t.name = name;
    t.timeBase = timeBase;

    if (atIndex < 0 || atIndex >= static_cast<int>(m_tracks.size())) {
        m_tracks.push_back(t);
    } else {
        m_tracks.insert(m_tracks.begin() + atIndex, t);
    }

    notify(ChangeKind::TrackAdded, t.id);
    return RetVal<ArrTrackId>::make_ok(t.id);
}

Ret Arrangement::removeTrack(ArrTrackId id)
{
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(), [id](const Track& t) { return t.id == id; });
    if (it == m_tracks.end()) {
        return noTrack();
    }
    m_tracks.erase(it);
    notify(ChangeKind::TrackRemoved, id);
    return make_ok();
}

Ret Arrangement::renameTrack(ArrTrackId id, const String& name)
{
    Track* t = mutableTrack(id);
    if (!t) {
        return noTrack();
    }
    t->name = name;
    notify(ChangeKind::TrackChanged, id);
    return make_ok();
}

Ret Arrangement::setTrackPart(ArrTrackId id, const muse::ID& partId)
{
    Track* t = mutableTrack(id);
    if (!t) {
        return noTrack();
    }
    if (t->partId == partId) {
        return make_ok();
    }
    t->partId = partId;
    notify(ChangeKind::TrackChanged, id);
    return make_ok();
}

Ret Arrangement::moveTrack(ArrTrackId id, int toIndex)
{
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(), [id](const Track& t) { return t.id == id; });
    if (it == m_tracks.end()) {
        return noTrack();
    }
    if (toIndex < 0 || toIndex >= static_cast<int>(m_tracks.size())) {
        return badArgs("track index out of range");
    }

    Track t = *it;
    m_tracks.erase(it);
    m_tracks.insert(m_tracks.begin() + toIndex, t);
    notify(ChangeKind::TrackChanged, id);
    return make_ok();
}

Ret Arrangement::setRecordArmed(ArrTrackId id, bool armed)
{
    Track* t = mutableTrack(id);
    if (!t) {
        return noTrack();
    }
    t->recordArmed = armed;
    notify(ChangeKind::TrackChanged, id);
    return make_ok();
}

Ret Arrangement::setInputMonitoring(ArrTrackId id, bool on)
{
    Track* t = mutableTrack(id);
    if (!t) {
        return noTrack();
    }
    t->inputMonitoring = on;
    notify(ChangeKind::TrackChanged, id);
    return make_ok();
}

// --- Clip invariants ---------------------------------------------------------------------------

Ret Arrangement::validateClip(const AudioClip& c, bool placement) const
{
    const double offset = c.sourceOffset.raw();
    const double length = c.length.raw();
    if (!std::isfinite(offset) || !std::isfinite(length) || offset < -EPS || length <= EPS) {
        return badArgs(trc("arrangement", "A clip must have a length."));
    }
    if (c.fadeIn.length.raw() < 0.0 || c.fadeOut.length.raw() < 0.0
        || c.fadeIn.length.raw() + c.fadeOut.length.raw() > length + EPS) {
        return badArgs(trc("arrangement", "The fades are longer than the clip."));
    }
    if (!std::isfinite(c.gainDb)) {
        return badArgs("clip gain is not a number");
    }

    if (!placement) {
        return make_ok();
    }

    const AudioAsset* asset = assetStore() ? assetStore()->asset(c.asset) : nullptr;
    if (!asset) {
        return badArgs("unknown audio asset " + c.asset);
    }
    if (offset + length > asset->duration().raw() + EPS) {
        return badArgs(trc("arrangement", "A clip cannot be longer than its audio."));
    }
    if (c.startSecs.raw() < -EPS || c.startTick.negative()) {
        return badArgs(trc("arrangement", "A clip cannot start before the beginning of the score."));
    }
    return make_ok();
}

bool Arrangement::wouldOverlap(const Track& t, const AudioClip& candidate, ClipId ignore) const
{
    const double start = candidate.startSecs.raw();
    const double end = candidate.endSecs().raw();
    for (const AudioClip& other : t.clips) {
        if (other.id == ignore) {
            continue;
        }
        // Touching is fine: one clip may start exactly where the previous one ends.
        if (start < other.endSecs().raw() - EPS && other.startSecs.raw() < end - EPS) {
            return true;
        }
    }
    return false;
}

void Arrangement::derivePosition(TimeBase timeBase, AudioClip& c) const
{
    if (timeBase == TimeBase::Musical) {
        c.startSecs = m_timeMap.tickToSecs(c.startTick);
    } else {
        c.startTick = m_timeMap.secsToTick(c.startSecs);
    }
}

void Arrangement::setStartSecs(TimeBase timeBase, AudioClip& c, secs_t secs) const
{
    if (timeBase == TimeBase::Musical) {
        c.startTick = m_timeMap.secsToTick(secs);
    } else {
        c.startSecs = secs;
    }
    derivePosition(timeBase, c);
}

void Arrangement::sortClips(Track& t)
{
    std::stable_sort(t.clips.begin(), t.clips.end(), [](const AudioClip& a, const AudioClip& b) {
        return a.startSecs.raw() < b.startSecs.raw();
    });
}

Ret Arrangement::replaceClip(Track& t, const AudioClip& edited, bool placement)
{
    Ret ret = validateClip(edited, placement);
    if (!ret) {
        return ret;
    }
    // Only edits that move or resize a clip check overlap: after a tempo change, Musical clips
    // may overlap (onTempoMapChanged) and must still accept gain and fade edits.
    if (placement && wouldOverlap(t, edited, edited.id)) {
        return overlapError();
    }

    AudioClip* current = mutableClip(t, edited.id);
    IF_ASSERT_FAILED(current) {
        return noClip();
    }
    *current = edited;
    sortClips(t);
    return make_ok();
}

// --- Clips -------------------------------------------------------------------------------------

RetVal<ClipId> Arrangement::addClip(ArrTrackId trackId, const AudioClip& clip)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return RetVal<ClipId>::make_ret(noTrack());
    }

    AudioClip c = clip;
    c.id = INVALID_CLIP;
    if (t->timeBase == TimeBase::Musical) {
        c.startTick.reduce();
    }
    derivePosition(t->timeBase, c);

    Ret ret = validateClip(c, true);
    if (!ret) {
        return RetVal<ClipId>::make_ret(ret);
    }
    if (wouldOverlap(*t, c, INVALID_CLIP)) {
        return RetVal<ClipId>::make_ret(overlapError());
    }

    c.id = ++m_lastClipId;
    t->clips.push_back(c);
    sortClips(*t);

    notify(ChangeKind::ClipsChanged, trackId);
    return RetVal<ClipId>::make_ok(c.id);
}

Ret Arrangement::removeClip(ArrTrackId trackId, ClipId clipId)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return noTrack();
    }
    auto it = std::find_if(t->clips.begin(), t->clips.end(), [clipId](const AudioClip& c) { return c.id == clipId; });
    if (it == t->clips.end()) {
        return noClip();
    }
    t->clips.erase(it);
    notify(ChangeKind::ClipsChanged, trackId);
    return make_ok();
}

Ret Arrangement::moveClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack, const Fraction& newStartTick)
{
    Track* from = mutableTrack(fromTrack);
    Track* to = mutableTrack(toTrack);
    if (!from || !to) {
        return noTrack();
    }
    AudioClip* current = mutableClip(*from, clipId);
    if (!current) {
        return noClip();
    }

    AudioClip moved = *current;
    if (to->timeBase == TimeBase::Musical) {
        moved.startTick = newStartTick.reduced();
    } else {
        moved.startSecs = m_timeMap.tickToSecs(newStartTick);
    }
    derivePosition(to->timeBase, moved);

    Ret ret = validateClip(moved, true);
    if (!ret) {
        return ret;
    }

    if (from == to) {
        ret = replaceClip(*from, moved, true);
        if (ret) {
            notify(ChangeKind::ClipsChanged, fromTrack);
        }
        return ret;
    }

    if (wouldOverlap(*to, moved, INVALID_CLIP)) {
        return overlapError();
    }

    from->clips.erase(std::find_if(from->clips.begin(), from->clips.end(),
                                   [clipId](const AudioClip& c) { return c.id == clipId; }));
    to->clips.push_back(moved);
    sortClips(*to);

    // Two tracks changed: one Change each.
    notify(ChangeKind::ClipsChanged, fromTrack);
    notify(ChangeKind::ClipsChanged, toTrack);
    return make_ok();
}

Ret Arrangement::trimClip(ArrTrackId trackId, ClipId clipId, secs_t newSourceOffset, secs_t newLength)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return noTrack();
    }
    AudioClip* current = mutableClip(*t, clipId);
    if (!current) {
        return noClip();
    }

    AudioClip c = *current;
    const double oldStart = c.startSecs.raw();
    const double oldOffset = c.sourceOffset.raw();
    const double newEnd = oldStart + (newSourceOffset.raw() - oldOffset) + newLength.raw(); // where the right edge goes

    // The audio stays where it is on the timeline: moving the left edge by d moves the source
    // offset by d. A Musical clip starts on a tick, so the left edge snaps to the nearest tick
    // and the offset follows the snapped edge exactly.
    if (std::abs(newSourceOffset.raw() - oldOffset) > EPS) {
        setStartSecs(t->timeBase, c, secs_t(std::max(0.0, oldStart + (newSourceOffset.raw() - oldOffset))));
        const double delta = c.startSecs.raw() - oldStart;
        c.sourceOffset = oldOffset + delta;
        if (c.sourceOffset.raw() < 0.0 && c.sourceOffset.raw() > -EPS) {
            c.sourceOffset = 0.0;
        }
    }
    c.length = newEnd - c.startSecs.raw();

    c.fadeIn = clampFade(c.fadeIn, c.length.raw());
    c.fadeOut = clampFade(c.fadeOut, c.length.raw() - c.fadeIn.length.raw());

    Ret ret = replaceClip(*t, c, true);
    if (ret) {
        notify(ChangeKind::ClipsChanged, trackId);
    }
    return ret;
}

RetVal<ClipId> Arrangement::splitClip(ArrTrackId trackId, ClipId clipId, const Fraction& atTick)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return RetVal<ClipId>::make_ret(noTrack());
    }
    AudioClip* current = mutableClip(*t, clipId);
    if (!current) {
        return RetVal<ClipId>::make_ret(noClip());
    }

    const double splitSecs = m_timeMap.tickToSecs(atTick).raw();
    const double start = current->startSecs.raw();
    const double end = current->endSecs().raw();
    if (splitSecs <= start + EPS || splitSecs >= end - EPS) {
        return RetVal<ClipId>::make_ret(badArgs(trc("arrangement", "The split position must be inside the clip.")));
    }

    AudioClip left = *current;
    AudioClip right = *current;

    left.length = splitSecs - start;
    right.id = m_lastClipId + 1;
    right.sourceOffset = current->sourceOffset.raw() + (splitSecs - start);
    right.length = end - splitSecs;
    if (t->timeBase == TimeBase::Musical) {
        right.startTick = atTick.reduced();
    } else {
        right.startSecs = splitSecs;
    }
    derivePosition(t->timeBase, right);

    // Short fades on both sides of the cut avoid a click; the outer fades stay where they were.
    left.fadeOut = clampFade(Fade { SPLIT_FADE_SECS, FadeCurve::Linear }, left.length.raw() / 2);
    left.fadeIn = clampFade(left.fadeIn, left.length.raw() - left.fadeOut.length.raw());
    right.fadeIn = clampFade(Fade { SPLIT_FADE_SECS, FadeCurve::Linear }, right.length.raw() / 2);
    right.fadeOut = clampFade(current->fadeOut, right.length.raw() - right.fadeIn.length.raw());

    Ret ret = validateClip(left, true);
    if (ret) {
        ret = validateClip(right, true);
    }
    if (!ret) {
        return RetVal<ClipId>::make_ret(ret);
    }

    *current = left;
    ++m_lastClipId;
    t->clips.push_back(right);
    sortClips(*t);

    notify(ChangeKind::ClipsChanged, trackId);
    return RetVal<ClipId>::make_ok(right.id);
}

Ret Arrangement::setClipGain(ArrTrackId trackId, ClipId clipId, float gainDb)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return noTrack();
    }
    AudioClip* current = mutableClip(*t, clipId);
    if (!current) {
        return noClip();
    }

    AudioClip c = *current;
    c.gainDb = gainDb;
    Ret ret = replaceClip(*t, c, false);
    if (ret) {
        notify(ChangeKind::ClipsChanged, trackId);
    }
    return ret;
}

Ret Arrangement::setClipFades(ArrTrackId trackId, ClipId clipId, const Fade& fadeIn, const Fade& fadeOut)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return noTrack();
    }
    AudioClip* current = mutableClip(*t, clipId);
    if (!current) {
        return noClip();
    }

    AudioClip c = *current;
    c.fadeIn = fadeIn;
    c.fadeOut = fadeOut;
    Ret ret = replaceClip(*t, c, false);
    if (ret) {
        notify(ChangeKind::ClipsChanged, trackId);
    }
    return ret;
}

Ret Arrangement::setClipMuted(ArrTrackId trackId, ClipId clipId, bool muted)
{
    Track* t = mutableTrack(trackId);
    if (!t) {
        return noTrack();
    }
    AudioClip* current = mutableClip(*t, clipId);
    if (!current) {
        return noClip();
    }
    current->muted = muted;
    notify(ChangeKind::ClipsChanged, trackId);
    return make_ok();
}

RetVal<ClipId> Arrangement::duplicateClip(ArrTrackId fromTrack, ClipId clipId, ArrTrackId toTrack, const Fraction& atTick)
{
    Track* from = mutableTrack(fromTrack);
    Track* to = mutableTrack(toTrack);
    if (!from || !to) {
        return RetVal<ClipId>::make_ret(noTrack());
    }
    AudioClip* current = mutableClip(*from, clipId);
    if (!current) {
        return RetVal<ClipId>::make_ret(noClip());
    }

    // FontDaw (Batch 2 03a): a copy with a new id (addClip assigns it), placed like moveClip
    AudioClip copy = *current;
    copy.id = INVALID_CLIP;
    copy.startTick = atTick.reduced();
    if (to->timeBase == TimeBase::Absolute) {
        copy.startSecs = m_timeMap.tickToSecs(atTick);
    }
    return addClip(toTrack, copy);
}

Ret Arrangement::setClipStartTicks(const std::vector<ClipStartTick>& starts)
{
    // FD-151: check everything first, so a failure changes nothing
    for (const ClipStartTick& start : starts) {
        Track* track = mutableTrack(start.track);
        if (!track) {
            return noTrack();
        }
        if (track->timeBase != TimeBase::Musical || !mutableClip(*track, start.clip) || start.tick < Fraction(0, 1)) {
            return noClip();
        }
    }

    std::vector<ArrTrackId> changed;
    for (const ClipStartTick& start : starts) {
        Track* track = mutableTrack(start.track);
        AudioClip* clip = mutableClip(*track, start.clip);
        clip->startTick = start.tick.reduced();
        derivePosition(track->timeBase, *clip);
        if (std::find(changed.begin(), changed.end(), track->id) == changed.end()) {
            changed.push_back(track->id);
        }
    }
    for (ArrTrackId id : changed) {
        sortClips(*mutableTrack(id));
        notify(ChangeKind::ClipsChanged, id);
    }
    return muse::make_ok();
}

void Arrangement::restoreTracks(const TrackList& tracks)
{
    // FontDaw (Batch 2 03a): history fields come from `tracks`; the live fields stay with the live
    // track of the same id (a track coming back keeps what the snapshot holds)
    TrackList restored = tracks;
    for (Track& t : restored) {
        auto live = std::find_if(m_tracks.begin(), m_tracks.end(), [&t](const Track& l) { return l.id == t.id; });
        if (live != m_tracks.end()) {
            t.recordArmed = live->recordArmed;
            t.inputMonitoring = live->inputMonitoring;
            t.inputChannel = live->inputChannel;
            t.partId = live->partId;
        }
        m_lastTrackId = std::max(m_lastTrackId, t.id);
        for (AudioClip& c : t.clips) {
            m_lastClipId = std::max(m_lastClipId, c.id);
            derivePosition(t.timeBase, c);
        }
        sortClips(t);
    }
    m_tracks = std::move(restored);
    notify(ChangeKind::Restored);
}

uint64_t Arrangement::epoch() const
{
    return m_epoch;
}

bool mu::arrangement::sameHistory(const TrackList& a, const TrackList& b)
{
    // FontDaw (Batch 2 03a): ignored on purpose: Track::recordArmed, inputMonitoring, inputChannel,
    // partId (live fields); per clip the derived half of the position (startSecs on a Musical
    // track, startTick on an Absolute one). Every other field of Track and AudioClip is compared.
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const Track& ta = a[i];
        const Track& tb = b[i];
        if (ta.id != tb.id || ta.kind != tb.kind || ta.name != tb.name || ta.timeBase != tb.timeBase
            || ta.clips.size() != tb.clips.size()) {
            return false;
        }
        for (size_t j = 0; j < ta.clips.size(); ++j) {
            const AudioClip& ca = ta.clips[j];
            const AudioClip& cb = tb.clips[j];
            const bool samePosition = ta.timeBase == TimeBase::Musical
                                      ? ca.startTick == cb.startTick
                                      : ca.startSecs.raw() == cb.startSecs.raw();
            if (!samePosition || ca.id != cb.id || ca.asset != cb.asset || ca.name != cb.name
                || ca.sourceOffset.raw() != cb.sourceOffset.raw() || ca.length.raw() != cb.length.raw()
                || ca.gainDb != cb.gainDb || !(ca.fadeIn == cb.fadeIn) || !(ca.fadeOut == cb.fadeOut)
                || ca.muted != cb.muted || ca.sourceTempoBpm != cb.sourceTempoBpm) {
                return false;
            }
        }
    }
    return true;
}

// --- Time --------------------------------------------------------------------------------------

void Arrangement::setTimeMap(const TimeMap& timeMap)
{
    IF_ASSERT_FAILED(timeMap.tickToSecs && timeMap.secsToTick) {
        return;
    }
    m_timeMap = timeMap;
    onTempoMapChanged();
}

void Arrangement::onTempoMapChanged()
{
    // Musical clips keep their tick and get new seconds; Absolute clips keep their seconds.
    // Two Musical clips may now overlap in seconds (the tempo got faster): both are kept, the
    // user's audio is never deleted; playback trims the earlier one (C2).
    for (Track& t : m_tracks) {
        for (AudioClip& c : t.clips) {
            derivePosition(t.timeBase, c);
        }
        sortClips(t);
    }
    notify(ChangeKind::TracksReset);
}

// --- Persistence -------------------------------------------------------------------------------

Ret Arrangement::read(const ReadProjectEntry& readEntry)
{
    clear(); // raises the epoch (Batch 2 03a)

    RetVal<ByteArray> json = readEntry(ARRANGEMENT_JSON_NAME);
    if (!json.ret) {
        return json.ret;
    }
    if (json.val.empty()) {
        return make_ok(); // a plain score: no arrangement yet
    }

    RetVal<ArrangementFileData> data = arrangementjson::read(json.val);
    if (!data.ret) {
        return data.ret;
    }

    std::set<ArrTrackId> trackIds;
    std::set<ClipId> clipIds;
    for (const Track& t : data.val.tracks) {
        if (!trackIds.insert(t.id).second) {
            return Ret(static_cast<int>(Ret::Code::BadData),
                       trc("arrangement", "The arrangement data in this project is damaged and could not be read."));
        }
        for (const AudioClip& c : t.clips) {
            if (!clipIds.insert(c.id).second) {
                return Ret(static_cast<int>(Ret::Code::BadData),
                           trc("arrangement", "The arrangement data in this project is damaged and could not be read."));
            }
        }
    }

    if (assetStore()) {
        Ret ret = assetStore()->load(data.val.assets, readEntry);
        if (!ret) {
            return ret;
        }
    }

    // Check and repair the clips before they become the model, so a refused file leaves
    // nothing half-loaded.
    for (Track& t : data.val.tracks) {
        for (AudioClip& c : t.clips) {
            const AudioAsset* asset = assetStore() ? assetStore()->asset(c.asset) : nullptr;
            if (!asset) {
                // Kept, never deleted: it shows as offline and is silent.
                LOGW() << "clip " << c.id << " uses an asset that is not in the project: " << c.asset;
            } else if (asset->duration().raw() > 0.0) {
                // Keep the invariant playback relies on: the window lies inside the asset.
                const double duration = asset->duration().raw();
                if (c.sourceOffset.raw() >= duration - EPS) {
                    LOGE() << "clip " << c.id << " starts after the end of its audio";
                    if (assetStore()) {
                        assetStore()->clear();
                    }
                    return Ret(static_cast<int>(Ret::Code::BadData),
                               trc("arrangement", "The arrangement data in this project is damaged and could not be read."));
                }
                if (c.sourceOffset.raw() + c.length.raw() > duration + EPS) {
                    LOGW() << "clip " << c.id << " is longer than its audio; shortened";
                    c.length = duration - c.sourceOffset.raw();
                }
                c.fadeIn = clampFade(c.fadeIn, c.length.raw());
                c.fadeOut = clampFade(c.fadeOut, c.length.raw() - c.fadeIn.length.raw());
            }
            derivePosition(t.timeBase, c);
        }
        sortClips(t);
    }

    m_tracks = std::move(data.val.tracks);
    m_lastTrackId = trackIds.empty() ? INVALID_TRACK : *trackIds.rbegin();
    m_lastClipId = clipIds.empty() ? INVALID_CLIP : *clipIds.rbegin();

    notify(ChangeKind::TracksReset);
    return make_ok();
}

Ret Arrangement::write(const WriteProjectEntry& writeEntry, const WriteOptions& options) const
{
    if (m_tracks.empty()) {
        // Nothing to add: a project without audio tracks stays a plain MuseScore-compatible file.
        return make_ok();
    }

    // Only assets clips use are saved: an import that was never placed does not bloat the file.
    std::set<AssetId> used;
    for (const Track& t : m_tracks) {
        for (const AudioClip& c : t.clips) {
            used.insert(c.asset);
        }
    }

    ArrangementFileData data;
    if (assetStore()) {
        RetVal<AudioAssetList> assets = assetStore()->save(writeEntry, used, options);
        if (!assets.ret) {
            return assets.ret;
        }
        data.assets = std::move(assets.val);
    }
    data.tracks = m_tracks;

    return writeEntry(ARRANGEMENT_JSON_NAME, arrangementjson::write(data));
}

void Arrangement::clear()
{
    ++m_epoch; // FontDaw (Batch 2 03a)
    m_tracks.clear();
    m_lastTrackId = INVALID_TRACK;
    m_lastClipId = INVALID_CLIP;
    if (assetStore()) {
        assetStore()->clear();
    }
    notify(ChangeKind::TracksReset);
}

void Arrangement::setOwner(const void* owner)
{
    m_owner = owner;
}

const void* Arrangement::owner() const
{
    return m_owner;
}

async::Channel<Change> Arrangement::changed() const
{
    return m_changed;
}

void Arrangement::notify(ChangeKind kind, ArrTrackId trackId)
{
    m_changed.send(Change { kind, trackId });
}
