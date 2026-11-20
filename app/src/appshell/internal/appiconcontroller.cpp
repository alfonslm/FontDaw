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

#include "appiconcontroller.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

#include "settings.h"

using namespace mu::appshell;

static const int PNG_SIZES[] = { 16, 24, 32, 48, 64, 128, 256 };

void AppIconController::init()
{
    apply();

    configuration()->appIconSettingsChanged().onNotify(this, [this]() {
        apply();
    });

    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        apply();
    });
}

AppIconVariant AppIconController::readEarly()
{
    muse::settings(); // sets QSettings' default format and path

    QSettings s;
    const AppIconVariant preferred = appIconVariantFromString(
        s.value(APP_ICON_PREFERRED_KEY, QString::fromStdString(appIconVariantToString(DEFAULT_PREFERRED_APP_ICON))).toString().toStdString());
    // Same rule as the settings system (only "true" is true), so the splash and the app agree.
    const bool follow = s.value(APP_ICON_FOLLOW_SYSTEM_KEY, DEFAULT_APP_ICON_FOLLOWS_SYSTEM ? "true" : "false").toString() == "true";

    return resolveAppIconVariant(preferred, follow, QGuiApplication::styleHints()->colorScheme());
}

AppIconVariant AppIconController::effectiveVariant() const
{
    return m_effective;
}

QString AppIconController::iconSource() const
{
    return "qrc" + QString(appIconSvgResource(effectiveVariant()));
}

muse::async::Notification AppIconController::iconChanged() const
{
    return m_iconChanged;
}

void AppIconController::apply()
{
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    const bool follow = configuration()->appIconFollowsSystem();
    const AppIconVariant variant = resolveAppIconVariant(configuration()->preferredAppIcon(), follow, scheme);

#ifdef Q_OS_MACOS
    if (follow && scheme != Qt::ColorScheme::Unknown) {
        // The installed icon follows the system by itself
        QGuiApplication::setWindowIcon(QIcon());
    } else {
        QGuiApplication::setWindowIcon(makeIcon(variant));
    }
#else
    QGuiApplication::setWindowIcon(makeIcon(variant));
#endif

    const bool changed = !m_applied || variant != m_effective;
    m_effective = variant;
    m_applied = true;

    if (changed) {
        m_iconChanged.notify();
    }
}

QIcon AppIconController::makeIcon(AppIconVariant variant) const
{
    QIcon icon;
    for (int size : PNG_SIZES) {
        icon.addFile(QString::fromStdString(appIconPngResource(variant, size)), QSize(size, size));
    }
    return icon;
}
