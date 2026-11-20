/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include <QObject>
#include <qqmlintegration.h>

#include "async/asyncable.h"

#include "modularity/ioc.h"
#include "rcommand/icommanddispatcher.h"
#include "project/iprojectsession.h"
#include "interactive/iinteractive.h"
#include "iplaybackconfiguration.h"

#include "ui/qml/Muse/Ui/navigationpanel.h"

#include "audio/common/audiotypes.h"
#include "notation/inotationautomation.h"
#include "project/iprojectaudiosettings.h"
#include "inputresourceitem.h"
#include "outputresourceitem.h"
#include "auxsenditem.h"
#include "expressionmappingmodel.h"

namespace mu::playback {
class MixerChannelItem : public QObject, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT

    Q_PROPERTY(Type type READ type CONSTANT)
    Q_PROPERTY(bool outputOnly READ outputOnly CONSTANT)

    Q_PROPERTY(QString title READ title NOTIFY titleChanged)

    Q_PROPERTY(mu::playback::InputResourceItem * inputResourceItem READ inputResourceItem NOTIFY inputResourceItemChanged)
    Q_PROPERTY(
        QList<mu::playback::OutputResourceItem*> outputResourceItemList READ outputResourceItemList NOTIFY outputResourceItemListChanged)
    Q_PROPERTY(QList<mu::playback::AuxSendItem*> auxSendItemList READ auxSendItemList NOTIFY auxSendItemListChanged)

    Q_PROPERTY(mu::playback::ExpressionMappingModel * expressionMappingModel READ expressionMappingModel CONSTANT)
    Q_PROPERTY(QString keyswitchMapId READ keyswitchMapId WRITE setKeyswitchMapId NOTIFY keyswitchMapIdChanged)

    Q_PROPERTY(float leftChannelPressure READ leftChannelPressure NOTIFY leftChannelPressureChanged)
    Q_PROPERTY(float rightChannelPressure READ rightChannelPressure NOTIFY rightChannelPressureChanged)

    Q_PROPERTY(float volumeLevel READ volumeLevel WRITE setVolumeLevel NOTIFY volumeLevelChanged)
    Q_PROPERTY(float volumeLevelMin READ volumeLevelMin CONSTANT)
    Q_PROPERTY(float volumeLevelMax READ volumeLevelMax CONSTANT)
    Q_PROPERTY(int balance READ balance WRITE setBalance NOTIFY balanceChanged)
    Q_PROPERTY(int balanceMin READ balanceMin CONSTANT)
    Q_PROPERTY(int balanceMax READ balanceMax CONSTANT)
    Q_PROPERTY(bool hasVolumeAutomation READ hasVolumeAutomation NOTIFY hasVolumeAutomationChanged)
    Q_PROPERTY(bool hasBalanceAutomation READ hasBalanceAutomation NOTIFY hasBalanceAutomationChanged)
    Q_PROPERTY(bool solo READ solo WRITE setSolo NOTIFY soloChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool forceMute READ forceMute NOTIFY forceMuteChanged)

    Q_PROPERTY(muse::ui::NavigationPanel * panel READ panel NOTIFY panelChanged)

    QML_ELEMENT;
    QML_UNCREATABLE("Must be created in C++ only")

    muse::GlobalInject<IPlaybackConfiguration> configuration;
    muse::ContextInject<muse::IInteractive> interactive = { this };
    muse::ContextInject<muse::rcommand::ICommandDispatcher> commandDispatcher = { this };
    muse::ContextInject<project::IProjectSession> context = { this };

public:
    enum class Type {
        Unknown,
        PrimaryInstrument,
        SecondaryInstrument,
        Metronome,
        Aux,
        Master,
        Audio, // FontDaw: an audio track (src/arrangement)
    };
    Q_ENUM(Type)

    MixerChannelItem()
        : muse::Contextable(muse::iocCtxForQmlObject(this)) {}
    MixerChannelItem(QObject* parent, Type type, bool outputOnly = false, muse::audio::TrackId trackId = -1);

    ~MixerChannelItem() override;

    Type type() const;

    muse::audio::TrackId trackId() const;

    const engraving::InstrumentTrackId& instrumentTrackId() const;
    void setInstrumentTrackId(const engraving::InstrumentTrackId& instrumentTrackId);

    QString title() const;

    float leftChannelPressure() const;
    float rightChannelPressure() const;

    //! FontDaw (FD-191): where a level in dB sits on the fader's travel (0 bottom, 1 top), so the
    //! level meter is drawn on the fader's scale
    Q_INVOKABLE double faderPosition(double db) const;

    float volumeLevel() const;
    float volumeLevelMin() const;
    float volumeLevelMax() const;
    int balance() const;
    int balanceMin() const;
    int balanceMax() const;
    bool hasVolumeAutomation() const;
    bool hasBalanceAutomation() const;
    bool solo() const;
    bool muted() const;
    bool forceMute() const;

    muse::ui::NavigationPanel* panel() const;
    void setPanelOrder(int panelOrder);
    void setPanelSection(muse::ui::INavigationSection* section);

    void updateHasAutomationFlags();

    void setOutputResourceItemCount(size_t count);

    void loadInputParams(const project::AudioInputParams& newParams);
    void loadOutputParams(const project::AudioOutputParams& newParams);
    void loadSoloMuteState(const notation::INotationSoloMuteState::SoloMuteState& newState);
    //! FontDaw (FD-162): whether the engine silences the track. Only the metronome takes it (its mute
    //! is the metronome switched off); elsewhere it can come from a solo, a range selection or a setting.
    void loadEngineMuted(bool engineMuted);

    //! FontDaw (FD-162): the user's own mute, without a solo's
    bool userMuted() const;
    //! FontDaw (FD-162): whether a solo on another strip silences this one (instruments, audio tracks)
    bool isSilencedBySolo() const;
    //! FontDaw (FD-162): "silenced by a solo", set by the mixer from the strips' solos (applySoloForceMute)
    void setForceMute(bool forceMute);

    void subscribeOnAudioSignalChanges(muse::audio::AudioSignalChanges& audioSignalChanges);
    void subscribeOnAutomatedControlParamsChanges(muse::audio::AutomatedControlParamsChanges& changes);

    bool outputOnly() const;

    const project::AudioInputParams& inputParams() const;
    const project::AudioOutputParams& outputParams() const;

    InputResourceItem* inputResourceItem() const;
    QList<OutputResourceItem*> outputResourceItemList() const;
    QList<AuxSendItem*> auxSendItemList() const;

    const QMap<muse::audio::aux_channel_idx_t, AuxSendItem*>& auxSendItems() const;

    ExpressionMappingModel* expressionMappingModel() const;

    QString keyswitchMapId() const;
    void setKeyswitchMapId(const QString& mapId);

public slots:
    void setTitle(QString title);

    void setLeftChannelPressure(float leftChannelPressure);
    void setRightChannelPressure(float rightChannelPressure);

    void setVolumeLevel(float volumeLevel);
    void setBalance(int balance);
    void setSolo(bool solo);
    void setMuted(bool mute);

signals:
    void titleChanged(QString title);

    void leftChannelPressureChanged(float leftChannelPressure);
    void rightChannelPressureChanged(float rightChannelPressure);

    void volumeLevelChanged(float volumeLevel);
    void balanceChanged(int balance);
    void hasVolumeAutomationChanged();
    void hasBalanceAutomationChanged();
    void soloChanged();
    void mutedChanged();
    void forceMuteChanged();

    void panelChanged(muse::ui::NavigationPanel* panel);

    void inputParamsChanged(const project::AudioInputParams& params);
    void controlParamsChanged(const project::AudioOutputParams& params);
    void fxChainParamsChanged(const project::AudioOutputParams& params);
    void auxSendsParamsChanged(const project::AudioOutputParams& params);
    void soloMuteStateChanged(const notation::INotationSoloMuteState::SoloMuteState& state);

    void inputResourceItemChanged();
    void outputResourceItemListChanged();
    void auxSendItemListChanged();
    void keyswitchMapIdChanged();

protected:
    notation::INotationPlaybackPtr notationPlayback() const;

    void setAudioChannelVolumePressure(const muse::audio::audioch_t chNum, const float newValue);
    void resetAudioChannelsVolumePressure();

    void applyMuteToOutputParams(const bool isMuted);

    void loadOutputResourceItems(const muse::audio::AudioFxChain& fxChain);
    void loadAuxSendItems(const muse::audio::AuxSendsParams& auxSends);

    InputResourceItem* buildInputResourceItem();
    OutputResourceItem* buildOutputResourceItem(const muse::audio::AudioFxParams& fxParams);
    AuxSendItem* buildAuxSendItem(muse::audio::aux_channel_idx_t index, const muse::audio::AuxSendParams& params);

    void addBlankSlots(size_t count);
    void removeBlankSlotsFromEnd(size_t count);

    muse::audio::AudioFxChainOrder resolveNewBlankOutputResourceItemOrder() const;

    void openEditor(AbstractAudioResourceItem* item, const muse::rcommand::CommandQuery& command);
    void closeEditor(AbstractAudioResourceItem* item);

    bool askAboutChangingSound();

    std::optional<notation::AutomationCurveKey> automationCurveKey(notation::AutomationType type) const;
    bool hasAutomationCurve(notation::AutomationType type) const;

    void setDisplayedVolumeLevel(float volumeLevel);
    void setDisplayedBalance(int balance);

    Type m_type = Type::Unknown;

    muse::audio::TrackId m_trackId = -1;
    engraving::InstrumentTrackId m_instrumentTrackId;

    project::AudioInputParams m_inputParams;
    project::AudioOutputParams m_outParams;
    //! FontDaw (FD-162): the user's own mute. m_outParams.muted is what is heard (the user's mute or a
    //! solo elsewhere, then forceMute is set); only the user's mute is ever sent back as the track's
    //! solo-mute state, so a mute caused by a solo is never stored as the user's.
    bool m_userMuted = false;
    bool engineMuteIsUsers() const;   // FontDaw (FD-162): the master and the metronome
    void updateDisplayedMute();

    float m_volumeLevel = 0.f;
    int m_balance = 0;

    bool m_hasVolumeAutomation = false;
    bool m_hasBalanceAutomation = false;

    InputResourceItem* m_inputResourceItem = nullptr;
    ExpressionMappingModel* m_expressionMappingModel = nullptr;
    QMap<muse::audio::AudioFxChainOrder, OutputResourceItem*> m_outputResourceItems;
    QMap<muse::audio::aux_channel_idx_t, AuxSendItem*> m_auxSendItems;

    muse::audio::AudioSignalChanges m_audioSignalChanges;
    muse::audio::AutomatedControlParamsChanges m_automatedControlParamsChanges;

    QString m_title;
    bool m_outputOnly = false;

    float m_leftChannelPressure = 0.0;
    float m_rightChannelPressure = 0.0;

    muse::ui::NavigationPanel* m_panel = nullptr;

    bool m_outputResourceItemsLoading = false;
};

//! FontDaw (FD-162): set "silenced by a solo" on the strips a solo acts on, from their solos
void applySoloForceMute(const QList<MixerChannelItem*>& strips);
}
