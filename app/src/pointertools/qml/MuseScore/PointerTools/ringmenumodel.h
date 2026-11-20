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

#include <QObject>
#include <QPointF>
#include <QQmlEngine>
#include <QVariantList>

#include "global/modularity/ioc.h"
#include "global/async/asyncable.h"
#include "rcommand/icommanddispatcher.h"
#include "project/iprojectsession.h"

#include "iringmenuprovider.h"
#include "ipointertoolsconfiguration.h"

namespace mu::pointertools {
//! The ring's segments for QML (at most RING_MAX_SLOTS, "More…" last), and what a chosen one does.
//! The ring's size lives as named values in one place, internal/RingMenu.qml: diameter 184 px,
//! see-through middle 48 px, engraving symbols 22 px, icons with a name 18 px (FD-156, FD-166).
//! The segment under the pointer is the one in the pointer's direction from the centre, even
//! beyond the rim; a segment's name is drawn under its icon when its slot has showName set
//! (everything but the engraving symbols), and each segment has its accessible name.
//!
//! Hook (in NotationView.qml's onShowContextMenuRequested, which every route to the
//! context menu passes: the context-menu event in AbstractNotationPaintView::event for the
//! secondary click, the Menu key and macOS Control+click; Shift+F10 through
//! openContextMenuOfSelection; and handleRightClick before Qt 6.9): when ringMenuEnabled is true,
//! today's model loads its items as now, and they are handed to load() instead of being shown;
//! the ring opens at the same point. In note input the secondary click removes the note and no
//! context-menu request is made (notationviewinputcontroller.cpp, unchanged), so no ring opens.
class RingMenuModel : public QObject, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool ringMenuEnabled READ ringMenuEnabled NOTIFY ringMenuEnabledChanged)
    Q_PROPERTY(QVariantList ringSlots READ ringSlots NOTIFY ringSlotsChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY ringSlotsChanged)

    muse::GlobalInject<IPointerToolsConfiguration> configuration;
    muse::ContextInject<IRingMenuProvider> provider = { this };
    muse::ContextInject<muse::rcommand::ICommandDispatcher> dispatcher = { this };
    muse::ContextInject<project::IProjectSession> projectSession = { this };

public:
    explicit RingMenuModel(QObject* parent = nullptr);

    bool ringMenuEnabled() const;
    QVariantList ringSlots() const;
    QString accessibleName() const;

    //! Subscribes to the current score; call from Component.onCompleted (needs the QML context)
    Q_INVOKABLE void init();
    //! todayMenuItems: today's context menu model's items (NotationContextMenuModel.items).
    Q_INVOKABLE void load(const QVariantList& todayMenuItems);

    //! Runs slot `index`: a command is dispatched; an item of today's menu is handed back through
    //! todayMenuItemRequested (NotationView.qml passes it to contextMenuModel.handleMenuItem);
    //! "More…" emits moreRequested (the layer opens today's menu at the same point).
    Q_INVOKABLE void activate(int index);

    //! Whether the release of the button that opened the ring runs the highlighted segment: only
    //! after the pointer left the dead zone around where the ring opened (internal/ringgeometry.h).
    Q_INVOKABLE bool openingReleaseChooses(const QPointF& openPos, const QPointF& releasePos, qreal holeRadius,
                                           int highlighted) const;
    //! The slot under (dx, dy), measured from the ring's centre, or -1 (internal/ringgeometry.h).
    Q_INVOKABLE int segmentAt(qreal dx, qreal dy, int count, qreal holeRadius, qreal maxRadius) const;
    //! Where slot `index` of `count` is centred, `radius` from the ring's centre (for its icon).
    Q_INVOKABLE QPointF segmentCentre(int index, int count, qreal radius) const;
    //! How wide slot `index`'s name may be, from the room its segment has (FD-156): the chord of the
    //! segment at the top and bottom, the ring's depth at the sides, at most maxWidth
    //! (internal/ringgeometry.h).
    Q_INVOKABLE qreal labelWidth(int index, int count, qreal innerRadius, qreal outerRadius, qreal maxWidth) const;

signals:
    void ringMenuEnabledChanged();
    void ringSlotsChanged();
    void todayMenuItemRequested(const QString& itemId);
    void moreRequested();
    //! The current score changed (tab switch, project closed): an open ring or menu belongs to the
    //! old one and closes
    void currentScoreChanged();

private:
    RingContents m_contents;
};
}
