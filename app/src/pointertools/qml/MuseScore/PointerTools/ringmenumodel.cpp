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

#include "ringmenumodel.h"

#include "global/log.h"
#include "uicomponents/qml/Muse/UiComponents/menuitem.h"

#include "internal/ringgeometry.h"

using namespace mu::pointertools;

namespace {
const QString KIND_COMMAND("command");
const QString KIND_TODAY_MENU_ITEM("todayMenuItem");
const QString KIND_MORE("more");

QString kindName(RingSlot::Kind kind)
{
    switch (kind) {
    case RingSlot::Kind::Command: return KIND_COMMAND;
    case RingSlot::Kind::TodayMenuItem: return KIND_TODAY_MENU_ITEM;
    case RingSlot::Kind::More: return KIND_MORE;
    }
    return QString();
}
}

RingMenuModel::RingMenuModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    if (configuration()) {
        configuration()->ringMenuOnSecondaryClickChanged().onReceive(this, [this](bool) {
            emit ringMenuEnabledChanged();
        });
    }
}

void RingMenuModel::init()
{
    if (projectSession()) {
        projectSession()->currentNotationChanged().onNotify(this, [this]() {
            emit currentScoreChanged();
        });
    }
}

bool RingMenuModel::ringMenuEnabled() const
{
    return configuration() && configuration()->ringMenuOnSecondaryClick();
}

QVariantList RingMenuModel::ringSlots() const
{
    QVariantList result;
    for (const RingSlot& slot : m_contents.ringSlots) {
        QVariantMap map;
        map["kind"] = kindName(slot.kind);
        map["title"] = slot.title.toQString();
        map["icon"] = static_cast<int>(slot.icon);
        map["iconMirrored"] = slot.iconMirrored;
        map["showName"] = slot.showName; // FD-156
        result << map;
    }
    return result;
}

QString RingMenuModel::accessibleName() const
{
    return m_contents.accessibleName.toQString();
}

void RingMenuModel::load(const QVariantList& todayMenuItems)
{
    std::vector<IRingMenuProvider::TodayMenuItem> today;
    today.reserve(todayMenuItems.size());

    for (const QVariant& value : todayMenuItems) {
        const auto* item = qobject_cast<muse::uicomponents::MenuItem*>(value.value<QObject*>());
        if (!item) {
            continue;
        }

        IRingMenuProvider::TodayMenuItem copy;
        copy.id = item->id().toStdString();
        copy.title = muse::String::fromQString(item->translatedTitle());
        copy.icon = static_cast<muse::ui::IconCode::Code>(item->icon_property());
        copy.enabled = item->enabled();
        // a separator is an item without an id (AbstractMenuModel::makeSeparator); a submenu has subitems
        copy.isSeparatorOrSubmenu = !item->isValid() || !item->subitems().isEmpty();
        today.push_back(std::move(copy));
    }

    m_contents = provider() ? provider()->scoreRing(today) : RingContents();

    emit ringSlotsChanged();
}

void RingMenuModel::activate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_contents.ringSlots.size())) {
        return;
    }

    const RingSlot slot = m_contents.ringSlots[index];

    switch (slot.kind) {
    case RingSlot::Kind::Command:
        if (dispatcher()) {
            dispatcher()->dispatch(slot.command);
        }
        break;
    case RingSlot::Kind::TodayMenuItem:
        emit todayMenuItemRequested(QString::fromStdString(slot.todayMenuItemId));
        break;
    case RingSlot::Kind::More:
        emit moreRequested();
        break;
    }
}

bool RingMenuModel::openingReleaseChooses(const QPointF& openPos, const QPointF& releasePos, qreal holeRadius,
                                          int highlighted) const
{
    return ringOpeningReleaseChooses(openPos, releasePos, holeRadius, highlighted);
}

int RingMenuModel::segmentAt(qreal dx, qreal dy, int count, qreal holeRadius, qreal maxRadius) const
{
    return ringSegmentAt(dx, dy, count, holeRadius, maxRadius);
}

QPointF RingMenuModel::segmentCentre(int index, int count, qreal radius) const
{
    return ringSegmentCentre(index, count, radius);
}

qreal RingMenuModel::labelWidth(int index, int count, qreal innerRadius, qreal outerRadius, qreal maxWidth) const
{
    return ringLabelWidth(index, count, innerRadius, outerRadius, maxWidth);
}
