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

#pragma once

#include <qqmlintegration.h>

#include <QObject>
#include <QTimer>

#include "async/asyncable.h"
#include "modularity/ioc.h"

#include "audio/main/iaudioconfiguration.h"
#include "audio/iaudiodrivercontroller.h"
#include "interactive/iinteractive.h"
#include "playback/iplaybackconfiguration.h"

namespace mu::preferences {
class CommonAudioDriverConfigurationModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT;

    Q_PROPERTY(QString currentDeviceId READ currentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(QVariantList deviceList READ deviceList NOTIFY deviceListChanged)

    Q_PROPERTY(unsigned int bufferSize READ bufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(QList<unsigned int> bufferSizeList READ bufferSizeList NOTIFY bufferSizeListChanged)

    Q_PROPERTY(unsigned int sampleRate READ sampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(QList<unsigned int> sampleRateList READ sampleRateList NOTIFY sampleRateListChanged)

    // Sync of the playback cursor with the sound
    Q_PROPERTY(int detectedOutputLatency READ detectedOutputLatency NOTIFY detectedOutputLatencyChanged)
    Q_PROPERTY(int outputLatencyCompensation READ outputLatencyCompensation NOTIFY outputLatencyCompensationChanged)
    Q_PROPERTY(int displayLatency READ displayLatency NOTIFY displayLatencyChanged)

    muse::GlobalInject<muse::audio::IAudioConfiguration> audioConfiguration;
    muse::GlobalInject<muse::audio::IAudioDriverController> audioDriverController;
    muse::GlobalInject<playback::IPlaybackConfiguration> playbackConfiguration;
    muse::ContextInject<muse::IInteractive> interactive = { this };

public:
    explicit CommonAudioDriverConfigurationModel(QObject* parent = nullptr);

    Q_INVOKABLE void load();

    QString currentDeviceId() const;
    QVariantList deviceList() const;
    Q_INVOKABLE void deviceSelected(const QString& deviceId);

    unsigned int bufferSize() const;
    QList<unsigned int> bufferSizeList() const;
    Q_INVOKABLE void bufferSizeSelected(const QString& bufferSizeStr);

    unsigned int sampleRate() const;
    QList<unsigned int> sampleRateList() const;
    Q_INVOKABLE void sampleRateSelected(const QString& sampleRateStr);

    //! Output latency measured by the audio driver (compensated automatically), msecs. Negative if the driver can't measure it
    int detectedOutputLatency() const;

    //! Output latency of the current device that the driver does not know about, set by the user, msecs
    int outputLatencyCompensation() const;
    Q_INVOKABLE void outputLatencyCompensationEdited(int msecs);

    //! Time between drawing and seeing, set by the user, msecs. The same for all devices
    int displayLatency() const;
    Q_INVOKABLE void displayLatencyEdited(int msecs);

signals:
    void currentDeviceIdChanged();
    void deviceListChanged();

    void sampleRateChanged();
    void sampleRateListChanged();

    void bufferSizeChanged();
    void bufferSizeListChanged();

    void detectedOutputLatencyChanged();
    void outputLatencyCompensationChanged();
    void displayLatencyChanged();

private:
    void updateDetectedOutputLatency();

    QTimer m_detectedOutputLatencyTimer;
    int m_detectedOutputLatency = -1;
};
}
