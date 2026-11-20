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

#include "mixerpanelmodel.h"

#include "async/notifylist.h"
#include "defer.h"
#include "log.h"
#include "translation.h"

#include "notation/imasternotation.h"
#include "notation/inotationautomation.h"
#include "notation/inotationparts.h"
#include "notation/inotationplayback.h"

#include "project/inotationproject.h"

#include "internal/mixerstriporder.h"

using namespace muse;
using namespace mu::playback;
using namespace muse::audio;
using namespace mu::engraving;
using namespace mu::notation;
using namespace mu::project;

static constexpr int INVALID_INDEX = -1;

static ControlParams withEnvelopesOf(ControlParams params, const ControlParams& current)
{
    // Automated controls are disabled, so an envelope is never the param being edited
    if (current.volume.hasAutomation()) {
        params.volume = current.volume;
    }
    if (current.balance.hasAutomation()) {
        params.balance = current.balance;
    }
    return params;
}

MixerPanelModel::MixerPanelModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void MixerPanelModel::componentComplete()
{
    init();
}

void MixerPanelModel::init()
{
    //! NOTE Must be set from Qml
    DO_ASSERT(m_navigationSection);

    controller()->playbackInitedChanged().onReceive(this, [this](bool) {
        reload();
    });

    controller()->trackAdded().onReceive(this, [this](const TrackId trackId) {
        onTrackAdded(trackId);
    });

    //! FontDaw (FD-161): tracks are added to the engine one by one after playback is set up, in the
    //! order the engine answers. Once all are in, rebuild the list from the full track map, so the
    //! strips never depend on that order. Only when strips were inserted one by one since the last
    //! rebuild: isPlayAllowedChanged also fires on tab switches and part changes, and a needless
    //! rebuild closes pop-ups and drops focus.
    controller()->isPlayAllowedChanged().onReceive(this, [this](bool) {
        if (m_rebuildAfterLoading.shouldRebuild(controller()->isPlaybackInited(), controller()->isLoaded())) {
            reload();
        }
    });

    controller()->trackRemoved().onReceive(this, [this](const TrackId trackId) {
        removeItem(trackId);
    });

    // FontDaw: audio tracks come and go on their own (import, load, remove). Their strips sit
    // between the instruments and the aux channels; rebuilding keeps that order.
    if (arrangementController()) {
        arrangementController()->trackAdded().onReceive(this, [this](const TrackId) {
            reload();
        });

        arrangementController()->trackRemoved().onReceive(this, [this](const TrackId trackId) {
            removeItem(trackId);
        });
    }

    reload();
}

void MixerPanelModel::reload()
{
    TRACEFUNC;

    reloadItems();
}

QVariantMap MixerPanelModel::get(int index)
{
    QVariantMap result;

    QHash<int, QByteArray> names = roleNames();
    QHashIterator<int, QByteArray> i(names);
    while (i.hasNext()) {
        i.next();
        QModelIndex idx = this->index(index, 0);
        QVariant data = idx.data(i.key());
        result[i.value()] = data;
    }

    return result;
}

QVariant MixerPanelModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || role != ChannelItemRole) {
        return QVariant();
    }

    return QVariant::fromValue(m_mixerChannelList.at(index.row()));
}

int MixerPanelModel::rowCount(const QModelIndex&) const
{
    return m_mixerChannelList.count();
}

QHash<int, QByteArray> MixerPanelModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { ChannelItemRole, "channelItem" }
    };

    return roles;
}

void MixerPanelModel::reloadItems()
{
    TRACEFUNC;

    beginResetModel();

    DEFER {
        endResetModel();
        emit rowCountChanged();
    };

    clear();
    m_rebuildAfterLoading.rebuilt(); // FontDaw (FD-161)

    if (!controller()->isPlaybackInited()) {
        return;
    }

    const auto& instrumentTrackIdMap = controller()->instrumentTrackIdMap();

    auto addInstrumentTrack = [this, &instrumentTrackIdMap](const InstrumentTrackId& instrumentTrackId, bool isPrimary = true) {
        auto search = instrumentTrackIdMap.find(instrumentTrackId);
        if (search == instrumentTrackIdMap.cend()) {
            return;
        }

        m_mixerChannelList.push_back(buildInstrumentChannelItem(search->second, instrumentTrackId, isPrimary));
    };

    async::NotifyList<const Part*> partList = masterNotationParts()->partList();
    for (const Part* part : partList) {
        std::string primaryInstrId = part->instrument()->id().toStdString();

        for (const InstrumentTrackId& instrumentTrackId : part->instrumentTrackIdList()) {
            bool isPrimary = instrumentTrackId.instrumentId == primaryInstrId;
            addInstrumentTrack(instrumentTrackId, isPrimary);
        }
    }
    for (auto it = instrumentTrackIdMap.cbegin(); it != instrumentTrackIdMap.cend(); ++it) {
        if (notationPlayback()->isChordSymbolsTrack(it->first)) {
            addInstrumentTrack(it->first);
        }
    }

    addInstrumentTrack(notationPlayback()->metronomeTrackId());

    if (arrangementController()) {
        for (const arrangement::IArrangementController::AudioTrack& audioTrack : arrangementController()->audioTracks()) {
            m_mixerChannelList.push_back(buildAudioChannelItem(audioTrack.engineTrackId, audioTrack.key));
        }
    }

    const auto& auxTrackIdMap = controller()->auxTrackIdMap();
    for (auto it = auxTrackIdMap.cbegin(); it != auxTrackIdMap.cend(); ++it) {
        if (configuration()->isAuxChannelVisible(it->first)) {
            m_mixerChannelList.push_back(buildAuxChannelItem(it->first, it->second));
        }
    }

    m_masterChannelItem = buildMasterChannelItem();
    m_mixerChannelList.append(m_masterChannelItem);

    applySoloForceMute(m_mixerChannelList); // FontDaw (FD-162)

    updateItemsPanelsOrder();
    setupConnections();
}

void MixerPanelModel::onTrackAdded(const TrackId& trackId)
{
    TRACEFUNC;

    //! FontDaw (FD-161): a list built before playback was set up has no master strip; inserting into it
    //! goes wrong. Build the whole list again instead.
    if (!m_masterChannelItem) {
        reload();
        return;
    }

    //! FontDaw (FD-161): already shown, e.g. by the rebuild when loading completes, which runs just
    //! before the last track's trackAdded
    if (findChannelItem(trackId)) {
        return;
    }

    const IPlaybackController::InstrumentTrackIdMap& instrumentTracks = controller()->instrumentTrackIdMap();
    auto instrumentIt = std::find_if(instrumentTracks.cbegin(), instrumentTracks.cend(), [trackId](const auto& pair) {
        return pair.second == trackId;
    });

    if (instrumentIt != instrumentTracks.end()) {
        const InstrumentTrackId& instrumentTrackId = instrumentIt->first;
        const Part* part = masterNotationParts()->part(instrumentTrackId.partId);
        bool isPrimary = part ? part->instrument()->id() == instrumentTrackId.instrumentId : true;
        int index = resolveInsertIndex(instrumentTrackId);
        if (index == INVALID_INDEX) { // FontDaw (FD-161): already there or not found: never insert at -1
            reload();
            return;
        }

        MixerChannelItem* item = buildInstrumentChannelItem(trackId, instrumentTrackId, isPrimary);
        addItem(item, index);
        m_rebuildAfterLoading.stripInserted(controller()->isLoaded()); // FontDaw (FD-161)
        applySoloForceMute(m_mixerChannelList); // FontDaw (FD-162)
        return;
    }

    const IPlaybackController::AuxTrackIdMap& auxTracks = controller()->auxTrackIdMap();
    auto auxIt = std::find_if(auxTracks.begin(), auxTracks.end(), [trackId](const auto& pair) {
        return pair.second == trackId;
    });

    if (auxIt != auxTracks.end()) {
        if (configuration()->isAuxChannelVisible(auxIt->first)) {
            addItem(buildAuxChannelItem(auxIt->first, trackId), masterChannelIndex()); // FontDaw (FD-161): before the master
            m_rebuildAfterLoading.stripInserted(controller()->isLoaded()); // FontDaw (FD-161)
        }
    }
}

void MixerPanelModel::addItem(MixerChannelItem* item, int index)
{
    TRACEFUNC;

    IF_ASSERT_FAILED(item) {
        return;
    }

    beginInsertRows(QModelIndex(), index, index);
    m_mixerChannelList.insert(index, item);
    updateItemsPanelsOrder();
    endInsertRows();

    emit rowCountChanged();
}

void MixerPanelModel::removeItem(const TrackId trackId)
{
    TRACEFUNC;

    int index = indexOf(trackId);
    if (index == INVALID_INDEX) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);

    m_mixerChannelList.removeAt(index);
    updateItemsPanelsOrder();

    endRemoveRows();

    applySoloForceMute(m_mixerChannelList); // FontDaw (FD-162): the removed strip may have been soloed

    updateOutputResourceItemCount();

    emit rowCountChanged();
}

void MixerPanelModel::updateItemsPanelsOrder()
{
    TRACEFUNC;

    for (int i = 0; i < m_mixerChannelList.size(); i++) {
        m_mixerChannelList[i]->setPanelOrder(m_navigationOrderStart + i);
    }
}

void MixerPanelModel::clear()
{
    TRACEFUNC;

    m_masterChannelItem = nullptr;
    qDeleteAll(m_mixerChannelList);
    m_mixerChannelList.clear();
}

void MixerPanelModel::setupConnections()
{
    audioSettings()->auxSoloMuteStateChanged().onReceive(
        this, [this](const aux_channel_idx_t index,
                     notation::INotationSoloMuteState::SoloMuteState newSoloMuteState) {
        const IPlaybackController::AuxTrackIdMap& auxTrackIdMap = controller()->auxTrackIdMap();
        TrackId trackId = muse::value(auxTrackIdMap, index);

        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->loadSoloMuteState(newSoloMuteState);
        }
    });

    //! FontDaw (FD-162): the metronome strip's mute is the metronome switched off, which can change
    //! elsewhere (the toolbar). Other strips ignore the engine's mute: "silenced by a solo" comes from
    //! the strips' solos (applySoloForceMute), not from the engine, which also mutes for other reasons.
    playback()->controlParamsChanged().onReceive(this, [this](const TrackId trackId, const ControlParams& params) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->loadEngineMuted(params.muted);
        }
    });

    playback()->sourceParamsChanged().onReceive(this, [this](const TrackId trackId, const AudioSourceParams& params) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->loadInputParams(params);
        }
    });

    playback()->fxChainParamsChanged().onReceive(this, [this](const TrackId trackId, const AudioFxChain& params) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            AudioOutputParams outParams = audioSettings()->trackOutputParams(item->instrumentTrackId());
            outParams.fxChain = params;
            loadOutputParams(item, outParams);
        }
    });

    playback()->masterFxChainParamsChanged().onReceive(this, [this](const AudioFxChain& params) {
        if (m_masterChannelItem) {
            AudioOutputParams outParams = audioSettings()->masterAudioOutputParams();
            outParams.fxChain = params;
            loadOutputParams(m_masterChannelItem, outParams);
        }
    }, Asyncable::Mode::SetReplace);

    controller()->auxChannelNameChanged().onReceive(this, [this](aux_channel_idx_t index, const std::string& name) {
        for (MixerChannelItem* item : m_mixerChannelList) {
            const QMap<aux_channel_idx_t, AuxSendItem*>& items = item->auxSendItems();
            auto it = items.find(index);

            if (it != items.end()) {
                it.value()->setTitle(QString::fromStdString(name));
            }
        }
    });

    configuration()->isAuxChannelVisibleChanged().onReceive(this, [this](aux_channel_idx_t index, bool visible) {
        const auto& auxMap = controller()->auxTrackIdMap();
        TrackId trackId = muse::value(auxMap, index);
        if (visible) {
            int visibleAuxesOnRight = 0;

            for (const auto& aux : auxMap) {
                if (configuration()->isAuxChannelVisible(aux.first) && (aux.first > index)) {
                    visibleAuxesOnRight++;
                }
            }
            // FontDaw (FD-161): never below 0 (no master strip yet)
            addItem(buildAuxChannelItem(index, trackId), std::max(0, masterChannelIndex() - visibleAuxesOnRight));
        } else {
            removeItem(trackId);
        }
    });

    subscribeOnAutomationChanges();
}

// TODO(FontDaw FEATURES §8): automated volume/pan of instrument tracks arrives here. Audio tracks
// need the same (src/arrangement/automationtypes.h), and both should be editable as timeline lanes.
void MixerPanelModel::subscribeOnAutomationChanges()
{
    if (!currentProject()) {
        return;
    }

    AutomationDataConstPtr automation = currentProject()->masterNotation()->automation()->automationData();
    if (!automation) {
        return;
    }

    automation->changed().onReceive(this, [this](const AutomationChanges& changes) {
        if (changes.isFullReset) {
            for (MixerChannelItem* item : m_mixerChannelList) {
                item->updateHasAutomationFlags();
            }
            return;
        }

        InstrumentTrackIdSet affectedTrackIds;
        bool masterAffected = false;

        for (const AutomationCurveKey& key : changes.affectedKeys) {
            if (key.type != AutomationType::Volume && key.type != AutomationType::Pan) {
                continue;
            }

            if (key.isGlobal()) {
                masterAffected = true;
            } else if (const std::optional<InstrumentTrackId> trackId = key.trackId()) {
                affectedTrackIds.insert(*trackId);
            }
        }

        for (MixerChannelItem* item : m_mixerChannelList) {
            const bool affected = item->type() == MixerChannelItem::Type::Master
                                  ? masterAffected
                                  : muse::contains(affectedTrackIds, item->instrumentTrackId());
            if (affected) {
                item->updateHasAutomationFlags();
            }
        }
    });
}

int MixerPanelModel::resolveInsertIndex(const engraving::InstrumentTrackId& newInstrumentTrackId) const
{
    //! FontDaw (FD-161): upstream's walk read past the end of a list without a master strip;
    //! mixerStripInsertIndex() is the same order, bounds-safe
    const InstrumentTrackId& metronomeTrackId = notationPlayback()->metronomeTrackId();

    auto kindOf = [this, &metronomeTrackId](const InstrumentTrackId& id) {
        if (id == metronomeTrackId) {
            return MixerStripRef::Kind::Metronome;
        }
        return notationPlayback()->isChordSymbolsTrack(id) ? MixerStripRef::Kind::ChordSymbols : MixerStripRef::Kind::Instrument;
    };

    std::vector<MixerStripRef> strips;
    strips.reserve(m_mixerChannelList.size());
    for (const MixerChannelItem* item : m_mixerChannelList) {
        const MixerChannelItem::Type type = item->type();
        const bool isScoreTrack = type == MixerChannelItem::Type::PrimaryInstrument
                                  || type == MixerChannelItem::Type::SecondaryInstrument
                                  || type == MixerChannelItem::Type::Metronome;
        strips.push_back({ isScoreTrack ? kindOf(item->instrumentTrackId()) : MixerStripRef::Kind::Other, item->instrumentTrackId() });
    }

    std::vector<InstrumentTrackId> scoreOrder;
    async::NotifyList<const Part*> partList = masterNotationParts()->partList();
    for (const Part* part : partList) {
        for (const InstrumentTrackId& instrumentTrackId : part->instrumentTrackIdList()) {
            scoreOrder.push_back(instrumentTrackId);
        }
    }

    return mixerStripInsertIndex(strips, scoreOrder, newInstrumentTrackId, kindOf(newInstrumentTrackId));
}

int MixerPanelModel::indexOf(const TrackId trackId) const
{
    for (int i = 0; i < m_mixerChannelList.size(); ++i) {
        if (trackId == m_mixerChannelList[i]->trackId()) {
            return i;
        }
    }

    return INVALID_INDEX;
}

//! FontDaw: the strip of an audio track (src/arrangement): name, volume, pan, mute, solo, inserts
//! and aux sends, like an instrument's, without a sound to choose. Its settings live under the
//! key arrangement::audioTrackKey() where instruments keep theirs.
//! TODO(FontDaw FEATURES §3): record-arm and monitor buttons come with recording.
MixerChannelItem* MixerPanelModel::buildAudioChannelItem(const TrackId trackId, const engraving::InstrumentTrackId& audioTrackKey)
{
    return buildInstrumentChannelItem(trackId, audioTrackKey, true /*isPrimary*/, true /*isAudioTrack*/);
}

MixerChannelItem* MixerPanelModel::buildInstrumentChannelItem(const TrackId trackId,
                                                              const engraving::InstrumentTrackId& instrumentTrackId,
                                                              bool isPrimary, bool isAudioTrack)
{
    MixerChannelItem::Type type = isPrimary ? MixerChannelItem::Type::PrimaryInstrument
                                  : MixerChannelItem::Type::SecondaryInstrument;

    const InstrumentTrackId& metronomeTrackId = notationPlayback()->metronomeTrackId();
    if (instrumentTrackId == metronomeTrackId) {
        type = MixerChannelItem::Type::Metronome;
    }

    if (isAudioTrack) {
        type = MixerChannelItem::Type::Audio;
    }

    MixerChannelItem* item = new MixerChannelItem(this, type, isAudioTrack /*outputOnly*/, trackId);
    item->setInstrumentTrackId(instrumentTrackId);
    item->setPanelSection(m_navigationSection);
    item->loadSoloMuteState(controller()->trackSoloMuteState(instrumentTrackId));

    playback()->params(trackId)
    .onResolve(this, [this, trackId, instrumentTrackId](const TrackParams& params) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->loadInputParams(params.source);

            AudioOutputParams outParams = audioSettings()->trackOutputParams(instrumentTrackId);
            outParams.fxChain = params.fxChain;
            outParams.auxSends = params.auxSends;
            outParams.setControl(params.control);
            loadOutputParams(item, outParams);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to get track output parameters, error code: " << errCode
               << ", " << text;
    });

    playback()->trackName(trackId)
    .onResolve(this, [this, trackId](const RetVal<TrackName>& trackName) {
        if (trackName.ret) {
            if (MixerChannelItem* item = findChannelItem(trackId)) {
                item->setTitle(QString::fromStdString(trackName.val));
            }
        } else {
            LOGE() << "unable to get track name, error: " << trackName.ret.toString();
        }
    });

    playback()->signalChanges(trackId)
    .onResolve(this, [this, trackId](AudioSignalChanges signalChanges) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->subscribeOnAudioSignalChanges(signalChanges);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to subscribe on audio signal changes from mixer channel, error code: " << errCode
               << ", " << text;
    });

    playback()->automatedControlParamsChanges(trackId)
    .onResolve(this, [this, trackId](AutomatedControlParamsChanges changes) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->subscribeOnAutomatedControlParamsChanges(changes);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to subscribe on automated control params changes from mixer channel, error code: " << errCode
               << ", " << text;
    });

    connect(item, &MixerChannelItem::inputParamsChanged, this, [this, trackId](const AudioInputParams& params) {
        playback()->setSourceParams(trackId, params);
    });

    connect(item, &MixerChannelItem::controlParamsChanged, this,
            [this, trackId, instrumentTrackId](const AudioOutputParams& params) {
        const ControlParams current = controller()->currentTrackControlParams(instrumentTrackId);
        ControlParams control = withEnvelopesOf(params.control(), current);
        //! FontDaw (FD-162): the strip's mute is the user's or a solo's; chord symbols are also muted
        //! while their playback is off, which the strip does not show
        if (notationPlayback()->isChordSymbolsTrack(instrumentTrackId)) {
            control.muted = control.muted || current.muted;
        }
        playback()->setControlParams(trackId, control);
    });

    connect(item, &MixerChannelItem::fxChainParamsChanged, this, [this, trackId](const AudioOutputParams& params) {
        playback()->setFxChainParams(trackId, params.fxChain);
    });

    connect(item, &MixerChannelItem::auxSendsParamsChanged, this, [this, trackId](const AudioOutputParams& params) {
        playback()->setAuxSendsParams(trackId, params.auxSends);
    });

    connect(item, &MixerChannelItem::auxSendItemListChanged, this, [this, item]() {
        const QMap<aux_channel_idx_t, AuxSendItem*>& auxSendItems = item->auxSendItems();
        for (auto it = auxSendItems.begin(); it != auxSendItems.end(); ++it) {
            it.value()->setTitle(QString::fromStdString(controller()->auxChannelName(it.key())));
        }
    });

    connect(item, &MixerChannelItem::soloMuteStateChanged, this,
            [this, instrumentTrackId](const notation::INotationSoloMuteState::SoloMuteState& state) {
        controller()->setTrackSoloMuteState(instrumentTrackId, state);
        applySoloForceMute(m_mixerChannelList); // FontDaw (FD-162)
    });

    return item;
}

MixerChannelItem* MixerPanelModel::buildAuxChannelItem(aux_channel_idx_t index, const TrackId trackId)
{
    MixerChannelItem* item = new MixerChannelItem(this, MixerChannelItem::Type::Aux, true /*outputOnly*/, trackId);
    item->setPanelSection(m_navigationSection);
    item->loadSoloMuteState(audioSettings()->auxSoloMuteState(index));

    playback()->trackName(trackId)
    .onResolve(this, [this, trackId](const RetVal<TrackName>& trackName) {
        if (trackName.ret) {
            if (MixerChannelItem* item = findChannelItem(trackId)) {
                item->setTitle(QString::fromStdString(trackName.val));
            }
        } else {
            LOGE() << "unable to get track name, error: " << trackName.ret.toString();
        }
    });

    AudioOutputParams outParams = audioSettings()->auxOutputParams(index);
    loadOutputParams(item, outParams);

    playback()->signalChanges(trackId)
    .onResolve(this, [this, trackId](AudioSignalChanges signalChanges) {
        if (MixerChannelItem* item = findChannelItem(trackId)) {
            item->subscribeOnAudioSignalChanges(signalChanges);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to subscribe on audio signal changes from mixer channel, error code: " << errCode
               << ", " << text;
    });

    connect(item, &MixerChannelItem::controlParamsChanged, this, [this, trackId](const AudioOutputParams& params) {
        playback()->setControlParams(trackId, params.control());
    });
    connect(item, &MixerChannelItem::fxChainParamsChanged, this, [this, trackId](const AudioOutputParams& params) {
        playback()->setFxChainParams(trackId, params.fxChain);
    });
    connect(item, &MixerChannelItem::auxSendsParamsChanged, this, [this, trackId](const AudioOutputParams& params) {
        playback()->setAuxSendsParams(trackId, params.auxSends);
    });

    connect(item, &MixerChannelItem::soloMuteStateChanged, this,
            [this, index](const notation::INotationSoloMuteState::SoloMuteState& state) {
        audioSettings()->setAuxSoloMuteState(index, state);
    });

    return item;
}

MixerChannelItem* MixerPanelModel::buildMasterChannelItem()
{
    MixerChannelItem* item = new MixerChannelItem(this, MixerChannelItem::Type::Master, true /*outputOnly*/, MASTER_TRACK_ID);
    item->setPanelSection(m_navigationSection);
    item->setTitle(muse::qtrc("playback", "Master"));

    AudioOutputParams outParams = audioSettings()->masterAudioOutputParams();
    item->updateHasAutomationFlags();
    loadOutputParams(item, outParams);

    playback()->masterSignalChanges()
    .onResolve(this, [this, item](AudioSignalChanges signalChanges) {
        if (m_masterChannelItem && item == m_masterChannelItem) {
            item->subscribeOnAudioSignalChanges(signalChanges);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to subscribe on audio signal changes from master channel, error code: " << errCode
               << ", " << text;
    });

    playback()->automatedControlParamsChanges(MASTER_TRACK_ID)
    .onResolve(this, [this, item](AutomatedControlParamsChanges changes) {
        if (m_masterChannelItem && item == m_masterChannelItem) {
            item->subscribeOnAutomatedControlParamsChanges(changes);
        }
    })
    .onReject(this, [](int errCode, std::string text) {
        LOGE() << "unable to subscribe on automated control params changes from master channel, error code: " << errCode
               << ", " << text;
    });

    connect(item, &MixerChannelItem::controlParamsChanged, this, [this](const AudioOutputParams& params) {
        const ControlParams current = controller()->currentMasterControlParams();
        playback()->setMasterControlParams(withEnvelopesOf(params.control(), current));
    });

    connect(item, &MixerChannelItem::fxChainParamsChanged, this, [this](const AudioOutputParams& params) {
        playback()->setMasterFxChainParams(params.fxChain);
    });

    connect(item, &MixerChannelItem::auxSendsParamsChanged, this, [this](const AudioOutputParams& params) {
        playback()->setMasterAuxSendsParams(params.auxSends);
    });

    return item;
}

int MixerPanelModel::masterChannelIndex() const
{
    return m_mixerChannelList.size() - 1;
}

MixerChannelItem* MixerPanelModel::findChannelItem(const TrackId& trackId) const
{
    for (MixerChannelItem* item : m_mixerChannelList) {
        if (item->trackId() == trackId) {
            return item;
        }
    }

    return nullptr;
}

void MixerPanelModel::loadOutputParams(MixerChannelItem* item, const AudioOutputParams& params)
{
    IF_ASSERT_FAILED(item) {
        return;
    }

    item->loadOutputParams(params);
    updateOutputResourceItemCount();
}

void MixerPanelModel::updateOutputResourceItemCount()
{
    size_t maxFxCount = 0;

    for (const MixerChannelItem* item : m_mixerChannelList) {
        const AudioFxChain& chain = item->outputParams().fxChain;
        if (chain.empty()) {
            continue;
        }

        AudioFxChainOrder order = std::prev(chain.end())->first;
        maxFxCount = std::max(maxFxCount, static_cast<size_t>(order) + 1);
    }

    for (MixerChannelItem* item : m_mixerChannelList) {
        item->setOutputResourceItemCount(maxFxCount + 1 /* + 1 blank slot */);
    }
}

INotationProjectPtr MixerPanelModel::currentProject() const
{
    return context()->project();
}

IProjectAudioSettingsPtr MixerPanelModel::audioSettings() const
{
    return currentProject() ? currentProject()->audioSettings() : nullptr;
}

INotationPlaybackPtr MixerPanelModel::notationPlayback() const
{
    return currentProject() ? currentProject()->masterNotation()->playback() : nullptr;
}

INotationPartsPtr MixerPanelModel::masterNotationParts() const
{
    return currentProject() ? currentProject()->masterNotation()->parts() : nullptr;
}

muse::ui::NavigationSection* MixerPanelModel::navigationSection() const
{
    return m_navigationSection;
}

void MixerPanelModel::setNavigationSection(muse::ui::NavigationSection* navigationSection)
{
    if (m_navigationSection == navigationSection) {
        return;
    }

    m_navigationSection = navigationSection;
    emit navigationSectionChanged();
}

int MixerPanelModel::navigationOrderStart() const
{
    return m_navigationOrderStart;
}

void MixerPanelModel::setNavigationOrderStart(int navigationOrderStart)
{
    if (m_navigationOrderStart == navigationOrderStart) {
        return;
    }

    m_navigationOrderStart = navigationOrderStart;
    emit navigationOrderStartChanged();

    updateItemsPanelsOrder();
}
