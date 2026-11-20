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

#include "aboutmodel.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUrl>

#include "translation.h"

using namespace mu::appshell;

AboutModel::AboutModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    appIconController()->iconChanged().onNotify(this, [this]() {
        emit appIconSourceChanged();
    });
}

// FontDaw shows its version as "n.n.n:build", e.g. 0.1.0:261115160 (owner's decision, Batch 1 A1).
static QString fontDawDisplayVersion(const muse::IApplication* app)
{
    QString version = app->fullVersion().toString().toQString();
    const QString build = app->build().toQString();
    return build.isEmpty() ? version : version + ":" + build;
}

QString AboutModel::appName() const
{
    return application()->title().toQString();
}

QString AboutModel::museScoreVersion() const
{
    QString version = fontDawDisplayVersion(application().get());
    return application()->unstable()
           ? muse::qtrc("appshell/about", "Unstable prerelease for %1").arg(version)
           : version;
}

QString AboutModel::museScoreRevision() const
{
    return QString::fromStdString(configuration()->museScoreRevision());
}

QString AboutModel::museScoreBuildDateTime() const
{
    auto buildDateISO = []() -> QString {
        // Attempt to convert __DATE__ into ISO 8601 format (YYYY-MM-DD)
        QDate date = QDate::fromString(__DATE__, "MMM dd yyyy");
        if (!date.isValid()) {
            date = QDate::fromString(__DATE__, "MMM  d yyyy");
        }
        return date.isValid() ? date.toString(Qt::ISODate) : __DATE__;
    };

    QString dateTime;
    dateTime += buildDateISO();
    dateTime += " ";
    dateTime += __TIME__;

    return dateTime;
}

QVariantMap AboutModel::sourceCodeUrl() const
{
    return makeUrl(QUrl("https://github.com/alfonslm/FontDaw/releases"));
}

// Empty while FONTDAW_HELP_BASE_URL is empty: makeUrl() then gives an empty "url", which QML tests.
QVariantMap AboutModel::helpUrl() const
{
    QUrl helpUrl(QString::fromStdString(configuration()->askForHelpUrl()));
    return makeUrl(helpUrl);
}

QVariantMap AboutModel::contributionUrl() const
{
    QUrl contributionUrl(QString::fromStdString(configuration()->museScoreContributionUrl()));
    return makeUrl(contributionUrl);
}

QVariantMap AboutModel::privacyUrl() const
{
    QUrl privacyUrl(QString::fromStdString(configuration()->fontDawPrivacyUrl()));
    return makeUrl(privacyUrl);
}

QVariantMap AboutModel::museScoreUrl() const
{
    QUrl museScoreUrl(QString::fromStdString(configuration()->museScoreUrl()));
    return makeUrl(museScoreUrl, false);
}

QVariantMap AboutModel::museScoreForumUrl() const
{
    QUrl museScoreForumUrl(QString::fromStdString(configuration()->museScoreForumUrl()));
    return makeUrl(museScoreForumUrl);
}

QVariantMap AboutModel::museScoreContributionUrl() const
{
    QUrl museScoreContributionUrl(QString::fromStdString(configuration()->museScoreContributionUrl()));
    return makeUrl(museScoreContributionUrl);
}

QVariantMap AboutModel::museScorePrivacyPolicyUrl() const
{
    QUrl museScorePrivacyPolicyUrl(QString::fromStdString(updateConfiguration()->privacyPolicyUrl()));
    return makeUrl(museScorePrivacyPolicyUrl);
}

QVariantMap AboutModel::musicXMLLicenseUrl() const
{
    QUrl musicXMLLicenseUrl(QString::fromStdString(configuration()->musicXMLLicenseUrl()));
    return makeUrl(musicXMLLicenseUrl);
}

QVariantMap AboutModel::musicXMLLicenseDeedUrl() const
{
    QUrl musicXMLLicenseDeedUrl(QString::fromStdString(configuration()->musicXMLLicenseDeedUrl()));
    return makeUrl(musicXMLLicenseDeedUrl);
}

void AboutModel::copyRevisionToClipboard() const
{
    QGuiApplication::clipboard()->setText(
        QString("OS: %1, Arch.: %2, %3 version (%4-bit): %5, revision: %6")
        .arg(QSysInfo::prettyProductName()
             + ((QSysInfo::productType() == "windows" && (QSysInfo::productVersion() == "10" || QSysInfo::productVersion() == "11"))
                ? " or later" : ""))
        .arg(QSysInfo::currentCpuArchitecture())
        .arg(application()->title().toQString())
        .arg(QSysInfo::WordSize)
        .arg(fontDawDisplayVersion(application().get()))
        .arg(application()->revision().toQString()));
}

void AboutModel::toggleDevMode()
{
    globalConfiguration()->setDevModeEnabled(!globalConfiguration()->devModeEnabled());
}

QString AboutModel::appIconSource() const
{
    return appIconController()->iconSource();
}

QVariantMap AboutModel::makeUrl(const QUrl& url, bool showPath) const
{
    QVariantMap urlMap;
    urlMap["url"] = url.toString();
    urlMap["displayName"] = showPath ? url.host() + url.path() : url.host();

    return urlMap;
}
