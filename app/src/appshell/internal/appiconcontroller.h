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

#include <QIcon>
#include <QObject>
#include <QString>

#include "async/asyncable.h"
#include "async/notification.h"
#include "modularity/ioc.h"

#include "../iappiconcontroller.h"
#include "../iappshellconfiguration.h"
#include "../appicontypes.h"

namespace mu::appshell {
//! Owns the icon FontDaw shows while it runs. One instance for the whole app (window icons are
//! app-wide), created and exported by AppShellModule, so it lives in the global context.
//!
//! What it sets:
//! - the window and taskbar icon of every window: QGuiApplication::setWindowIcon();
//! - on macOS, the Dock icon: Qt passes setWindowIcon() on to the application icon. When the
//!   setting follows the system (and the system's scheme can be read) it sets NOTHING on macOS,
//!   because the installed Icon Composer icon (Default and Dark variants) already follows the
//!   system and a run-time icon would override it (SCREEN_VS_PRINT §A3.5);
//! - the image source used by the About dialog, the splash screen and the Home page
//!   (iconSource(), read by QML through AboutModel and friends).
//!
//! What it does NOT change: the icons installed with the app (desktop file, .exe resource,
//! Start-menu shortcut, macOS bundle). Those are the Dark variant (macOS: both variants).
//!
//! The setting is a preferred icon (Dark or Light) plus "Follow system". If it follows the system
//! and the operating system's colour scheme can be read, the icon follows the system; otherwise the
//! preferred icon is used. The scheme is read with QGuiApplication::styleHints()->colorScheme() and
//! followed through QStyleHints::colorSchemeChanged (Qt 6.5+). Not muse::ui::IPlatformTheme: that
//! interface is internal to the framework and reports "not available" on some Linux desktops.
class AppIconController : public QObject, public IAppIconController, public muse::async::Asyncable
{
    Q_OBJECT

    muse::GlobalInject<IAppShellConfiguration> configuration;

public:
    //! Applies the icon, then listens to:
    //! - configuration()->appIconSettingsChanged() (the user changed the setting);
    //! - QStyleHints::colorSchemeChanged (the OS switched light/dark), only matters when following.
    //! Called from AppShellModule::onInit() after the configuration is initialised.
    void init();

    //! The icon to show before the settings system has loaded (the splash screen), read straight
    //! from the settings file. Nothing stored means the defaults: preferred Dark, follow the system.
    static AppIconVariant readEarly();

    AppIconVariant effectiveVariant() const override;
    QString iconSource() const override;
    muse::async::Notification iconChanged() const override;

private:
    //! Resolves the variant, sets the window icon (not on macOS when following a readable system
    //! scheme) and notifies iconChanged() when the variant changed (always on the first call).
    void apply();

    QIcon makeIcon(AppIconVariant variant) const;

    AppIconVariant m_effective = AppIconVariant::Dark;
    bool m_applied = false;
    muse::async::Notification m_iconChanged;
};
}
