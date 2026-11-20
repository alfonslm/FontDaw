/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "commonaudiodriverconfigurationmodel.h"

#include "global/translation.h"

#include "audio/common/audiotypes.h"
#include "audio/common/outputlatency.h"

using namespace mu::preferences;
using namespace muse::audio;

CommonAudioDriverConfigurationModel::CommonAudioDriverConfigurationModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void CommonAudioDriverConfigurationModel::load()
{
    audioDriverController()->currentAudioDriverChanged().onNotify(this, [this]() {
        emit deviceListChanged();
        emit currentDeviceIdChanged();
        emit sampleRateListChanged();
        emit sampleRateChanged();
        emit bufferSizeListChanged();
        emit bufferSizeChanged();
        emit outputLatencyCompensationChanged();
    });

    //! NOTE The compensation is kept per device
    audioConfiguration()->audioOutputDeviceIdChanged().onNotify(this, [this]() {
        emit outputLatencyCompensationChanged();
    });

    audioConfiguration()->outputLatencyCompensationChanged().onNotify(this, [this]() {
        emit outputLatencyCompensationChanged();
    });

    playbackConfiguration()->displayLatencyChanged().onNotify(this, [this]() {
        emit displayLatencyChanged();
    });

    //! NOTE The driver measures its latency on the audio thread, all the time.
    //! There is nothing to subscribe to, so we look at the value while the page is open
    connect(&m_detectedOutputLatencyTimer, &QTimer::timeout, this, [this]() {
        updateDetectedOutputLatency();
    });
    m_detectedOutputLatencyTimer.start(500);
    updateDetectedOutputLatency();

    audioDriverController()->availableOutputDevicesChanged().onNotify(this, [this]() {
        emit deviceListChanged();
        emit currentDeviceIdChanged();
    });

    audioDriverController()->outputDeviceChanged().onNotify(this, [this]() {
        emit currentDeviceIdChanged();
        emit sampleRateListChanged();
        emit sampleRateChanged();
        emit bufferSizeListChanged();
        emit bufferSizeChanged();
    });

    audioDriverController()->outputDeviceSampleRateChanged().onNotify(this, [this]() {
        emit sampleRateChanged();
    });

    audioDriverController()->outputDeviceBufferSizeChanged().onNotify(this, [this]() {
        emit bufferSizeChanged();
    });
}

QString CommonAudioDriverConfigurationModel::currentDeviceId() const
{
    AudioDeviceID device = audioDriverController()->outputDevice();
    return QString::fromStdString(device);
}

QVariantList CommonAudioDriverConfigurationModel::deviceList() const
{
    AudioDeviceList devices = audioDriverController()->availableOutputDevices();

    QVariantList result;
    result.reserve(devices.size());

    for (const AudioDevice& device : devices) {
        QVariantMap obj;
        obj["value"] = QString::fromStdString(device.id);
        obj["text"] = QString::fromStdString(device.name);

        result << obj;
    }

    return result;
}

void CommonAudioDriverConfigurationModel::deviceSelected(const QString& deviceId)
{
    bool ok = audioDriverController()->selectOutputDevice(deviceId.toStdString());
    if (!ok) {
        interactive()->error("",
                             muse::trc("appshell/preferences", "The driver for this device could not be opened."));
    }
}

unsigned int CommonAudioDriverConfigurationModel::bufferSize() const
{
    unsigned int val = audioDriverController()->activeSpec().output.samplesPerChannel;
    return val;
}

QList<unsigned int> CommonAudioDriverConfigurationModel::bufferSizeList() const
{
    std::vector<samples_t> bufferSizes = audioDriverController()->availableOutputDeviceBufferSizes();

    QList<unsigned int> result;
    result.reserve(bufferSizes.size());

    for (samples_t bufferSize : bufferSizes) {
        result << static_cast<unsigned int>(bufferSize);
    }

    return result;
}

void CommonAudioDriverConfigurationModel::bufferSizeSelected(const QString& bufferSizeStr)
{
    audioDriverController()->changeBufferSize(bufferSizeStr.toInt());
}

unsigned int CommonAudioDriverConfigurationModel::sampleRate() const
{
    return audioDriverController()->activeSpec().output.sampleRate;
}

QList<unsigned int> CommonAudioDriverConfigurationModel::sampleRateList() const
{
    std::vector<sample_rate_t> sampleRates = audioDriverController()->availableOutputDeviceSampleRates();

    QList<unsigned int> result;
    result.reserve(sampleRates.size());

    for (sample_rate_t sampleRate : sampleRates) {
        result << static_cast<unsigned int>(sampleRate);
    }

    return result;
}

void CommonAudioDriverConfigurationModel::sampleRateSelected(const QString& sampleRateStr)
{
    audioDriverController()->changeSampleRate(sampleRateStr.toInt());
}

void CommonAudioDriverConfigurationModel::updateDetectedOutputLatency()
{
    int msecs = -1;
    if (OutputLatency::isKnown()) {
        msecs = static_cast<int>((OutputLatency::ns() + 500000) / 1000000);
    }

    if (m_detectedOutputLatency == msecs) {
        return;
    }

    m_detectedOutputLatency = msecs;
    emit detectedOutputLatencyChanged();
}

int CommonAudioDriverConfigurationModel::detectedOutputLatency() const
{
    return m_detectedOutputLatency;
}

int CommonAudioDriverConfigurationModel::outputLatencyCompensation() const
{
    const AudioDeviceID deviceId = audioConfiguration()->audioOutputDeviceId();
    return static_cast<int>(audioConfiguration()->outputLatencyCompensation(deviceId).raw());
}

void CommonAudioDriverConfigurationModel::outputLatencyCompensationEdited(int msecs)
{
    if (msecs == outputLatencyCompensation()) {
        return;
    }

    const AudioDeviceID deviceId = audioConfiguration()->audioOutputDeviceId();
    audioConfiguration()->setOutputLatencyCompensation(deviceId, muse::msecs_t(msecs));
}

int CommonAudioDriverConfigurationModel::displayLatency() const
{
    return static_cast<int>(playbackConfiguration()->displayLatency().raw());
}

void CommonAudioDriverConfigurationModel::displayLatencyEdited(int msecs)
{
    if (msecs == displayLatency()) {
        return;
    }

    playbackConfiguration()->setDisplayLatency(muse::msecs_t(msecs));
}
