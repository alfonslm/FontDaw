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

#include "modularity/ioc.h"
#include "iappshellconfiguration.h"
#include "iappiconcontroller.h"
#include "update/iupdateconfiguration.h"
#include "global/iglobalconfiguration.h"
#include "global/iapplication.h"
#include "async/asyncable.h"

class QUrl;

namespace mu::appshell {
class AboutModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT

    QML_ELEMENT

    muse::GlobalInject<IAppShellConfiguration> configuration;
    muse::GlobalInject<muse::update::IUpdateConfiguration> updateConfiguration;
    muse::GlobalInject<muse::IGlobalConfiguration> globalConfiguration;
    muse::GlobalInject<muse::IApplication> application;
    muse::GlobalInject<IAppIconController> appIconController;

public:
    explicit AboutModel(QObject* parent = nullptr);

    Q_PROPERTY(QString appIconSource READ appIconSource NOTIFY appIconSourceChanged)

    Q_INVOKABLE QString appName() const;
    Q_INVOKABLE QString museScoreVersion() const;
    Q_INVOKABLE QString museScoreRevision() const;
    Q_INVOKABLE QVariantMap sourceCodeUrl() const;
    Q_INVOKABLE QVariantMap helpUrl() const;
    Q_INVOKABLE QVariantMap contributionUrl() const;
    Q_INVOKABLE QVariantMap privacyUrl() const;
    Q_INVOKABLE QString museScoreBuildDateTime() const;
    Q_INVOKABLE QVariantMap museScoreUrl() const;
    Q_INVOKABLE QVariantMap museScoreForumUrl() const;
    Q_INVOKABLE QVariantMap museScoreContributionUrl() const;
    Q_INVOKABLE QVariantMap museScorePrivacyPolicyUrl() const;

    Q_INVOKABLE QVariantMap musicXMLLicenseUrl() const;
    Q_INVOKABLE QVariantMap musicXMLLicenseDeedUrl() const;

    Q_INVOKABLE void copyRevisionToClipboard() const;

    Q_INVOKABLE void toggleDevMode();

    QString appIconSource() const;

signals:
    void appIconSourceChanged();

private:
    QVariantMap makeUrl(const QUrl& url, bool showPath = true) const;
};
}
