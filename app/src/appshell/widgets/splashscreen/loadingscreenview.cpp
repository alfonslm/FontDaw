/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "loadingscreenview.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>
#include <QFontDatabase>

#include "global/internal/baseapplication.h"
#include "appshell/appicontypes.h"
#include "appshell/internal/appiconcontroller.h"

using namespace mu::appshell;

// FontDaw splash: a plain dark background, the FontDaw icon, the name and the version. The icon is
// read straight from the settings (AppIconController::readEarly) because the splash shows before the
// settings system has loaded. There is no website line until FontDaw has a public site to name.

static constexpr QSize loadingScreenSize(800, 380);

static const QColor backgroundColor("#1E1E1E");

static const QColor titleColor("#FFFFFF");
static const QColor versionNumberColor("#C8C8C8"); // 9.9:1 on the background, meets the 4.5:1 minimum

static constexpr qreal iconSize = 128.0;
static constexpr qreal iconTop = 56.0;
static constexpr qreal titleTop = 200.0;
static constexpr qreal titleHeight = 56.0;
static constexpr qreal versionTop = 262.0;
static constexpr qreal versionHeight = 30.0;


LoadingScreenView::LoadingScreenView(QWidget* parent)
    : QWidget(parent),
    m_backgroundRenderer(new QSvgRenderer(QString(appIconSvgResource(AppIconController::readEarly())), this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(loadingScreenSize);
}

bool LoadingScreenView::event(QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(this);
        painter.setLayoutDirection(layoutDirection());
        draw(&painter);
    }

    return QWidget::event(event);
}

void LoadingScreenView::draw(QPainter* painter)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    // Draw background
    painter->fillRect(0, 0, width(), height(), backgroundColor);

    // Draw icon, centred
    const qreal iconLeft = (width() - iconSize) / 2.0;
    m_backgroundRenderer->render(painter, QRectF(iconLeft, iconTop, iconSize, iconSize));

    const QFont baseFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);

    // Draw name
    QFont titleFont = baseFont;
    if (titleFont.pointSizeF() > 0) {
        titleFont.setPointSizeF(baseFont.pointSizeF() * 2.2);
    }
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(titleColor);
    painter->drawText(QRectF(0, titleTop, width(), titleHeight), Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextDontClip,
                      muse::BaseApplication::appTitle().toQString()); // "FontDaw" or "FontDaw Development"

    // Draw version as "n.n.n:build"
    painter->setFont(baseFont);
    painter->setPen(versionNumberColor);
    QString version = muse::BaseApplication::appFullVersion().toString().toQString();
    const QString build = muse::BaseApplication::appBuild().toQString();
    if (!build.isEmpty()) {
        version += ":" + build;
    }
    painter->drawText(QRectF(0, versionTop, width(), versionHeight), Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, version);
}
