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

#include <map>

#include "notation/inotationsolomutestate.h"
#include "project/iprojectaudiosettings.h"

namespace mu::arrangement::tests {
//! The part of the project's audio settings an audio track uses: output params per track key.
class ProjectAudioSettingsFake : public project::IProjectAudioSettings
{
public:
    using InstrumentTrackId = engraving::InstrumentTrackId;
    using AudioOutputParams = project::AudioOutputParams;

    bool hasAnyAudioSettings() const override { return !m_output.empty(); }

    const AudioOutputParams& masterAudioOutputParams() const override { return m_dummyOutput; }
    void setMasterAudioOutputParams(const AudioOutputParams&) override {}

    bool containsAuxOutputParams(muse::audio::aux_channel_idx_t) const override { return false; }
    const AudioOutputParams& auxOutputParams(muse::audio::aux_channel_idx_t) const override { return m_dummyOutput; }
    void setAuxOutputParams(muse::audio::aux_channel_idx_t, const AudioOutputParams&, bool) override {}

    const project::TrackInputParamsMap& allTrackInputParams() const override { return m_input; }
    const project::AudioInputParams& trackInputParams(const InstrumentTrackId&) const override { return m_dummyInput; }
    void setTrackInputParams(const InstrumentTrackId&, const project::AudioInputParams&, bool) override {}
    void clearTrackInputParams() override {}
    muse::async::Channel<InstrumentTrackId> trackInputParamsChanged() const override { return m_inputChanged; }

    bool trackHasExistingOutputParams(const InstrumentTrackId& trackId) const override { return m_output.count(trackId) > 0; }

    const AudioOutputParams& trackOutputParams(const InstrumentTrackId& trackId) const override
    {
        auto it = m_output.find(trackId);
        return it != m_output.end() ? it->second : m_dummyOutput;
    }

    void setTrackOutputParams(const InstrumentTrackId& trackId, const AudioOutputParams& params, bool) override
    {
        m_output[trackId] = params;
    }

    const SoloMuteState& auxSoloMuteState(muse::audio::aux_channel_idx_t) const override { return m_dummySoloMute; }
    void setAuxSoloMuteState(muse::audio::aux_channel_idx_t, const SoloMuteState&) override {}
    muse::async::Channel<muse::audio::aux_channel_idx_t, SoloMuteState> auxSoloMuteStateChanged() const override
    {
        return m_auxSoloMuteChanged;
    }

    void removeTrackParams(const InstrumentTrackId& trackId) override { m_output.erase(trackId); }

    const playback::SoundProfileName& activeSoundProfile() const override { return m_profile; }
    void setActiveSoundProfile(const playback::SoundProfileName&) override {}

    muse::async::Notification settingsChanged() const override { return m_settingsChanged; }

private:
    std::map<InstrumentTrackId, AudioOutputParams> m_output;
    project::TrackInputParamsMap m_input;
    AudioOutputParams m_dummyOutput;
    project::AudioInputParams m_dummyInput;
    SoloMuteState m_dummySoloMute;
    playback::SoundProfileName m_profile;
    muse::async::Channel<InstrumentTrackId> m_inputChanged;
    muse::async::Channel<muse::audio::aux_channel_idx_t, SoloMuteState> m_auxSoloMuteChanged;
    muse::async::Notification m_settingsChanged;
};

class SoloMuteStateFake : public notation::INotationSoloMuteState
{
public:
    using InstrumentTrackId = engraving::InstrumentTrackId;

    muse::Ret read(const engraving::MscReader&, const muse::io::path_t&) override { return muse::make_ok(); }
    muse::Ret write(muse::io::IODevice*) override { return muse::make_ok(); }

    bool trackSoloMuteStateExists(const InstrumentTrackId& trackId) const override { return m_states.count(trackId) > 0; }

    const SoloMuteState& trackSoloMuteState(const InstrumentTrackId& trackId) const override
    {
        auto it = m_states.find(trackId);
        return it != m_states.end() ? it->second : m_dummy;
    }

    void setTrackSoloMuteState(const InstrumentTrackId& trackId, const SoloMuteState& state) override
    {
        m_states[trackId] = state;
        m_changed.send(trackId, state);
    }

    void initTrackSoloMuteState(const InstrumentTrackId& trackId, const SoloMuteState& state) override { m_states[trackId] = state; }
    void removeTrackSoloMuteState(const InstrumentTrackId& trackId) override { m_states.erase(trackId); }

    muse::async::Channel<InstrumentTrackId, SoloMuteState> trackSoloMuteStateChanged() const override { return m_changed; }

private:
    std::map<InstrumentTrackId, SoloMuteState> m_states;
    SoloMuteState m_dummy;
    muse::async::Channel<InstrumentTrackId, SoloMuteState> m_changed;
};
}
