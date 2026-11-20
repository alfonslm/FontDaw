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

#include "pointermenumodel.h"

#include "internal/commandsearch.h"

#include <QCursor>

#include "global/log.h"

#include "internal/commandsections.h"

using namespace mu::pointertools;

PointerMenuModel::PointerMenuModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void PointerMenuModel::init()
{
    // Not in the constructor: the window's context (controller()) is known only once QML has
    // set the object's context, i.e. from Component.onCompleted
    if (controller()) {
        controller()->pointerMenuRequested().onNotify(this, [this]() {
            emit openRequested();
        });
    }
}

QString PointerMenuModel::searchText() const
{
    return m_searchText;
}

void PointerMenuModel::setSearchText(const QString& text)
{
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    emit searchTextChanged();
    reload();
}

void PointerMenuModel::load()
{
    static bool coverageLogged = false;
    if (!coverageLogged) {
        coverageLogged = true;
        logKeyCoverage();
    }

    // Each command's enabled state and keys are read once, when the menu opens; typing only
    // re-ranks these (review of #195)
    m_all = commandIndex() ? commandIndex()->entries(ViewKind::Score) : CommandEntryList();
    reload();
}

void PointerMenuModel::run(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size()) || !commandIndex()) {
        return;
    }

    // the caller (the pop-up) has closed itself before this
    commandIndex()->run(m_entries[row]);
}

QPointF PointerMenuModel::cursorPosIn(QQuickItem* item) const
{
    if (!item) {
        return QPointF();
    }

    return item->mapFromGlobal(QCursor::pos());
}

void PointerMenuModel::reload()
{
    beginResetModel();

    m_entries.clear();
    for (const SearchHit& hit : rankEntries(m_all, muse::String::fromQString(m_searchText))) {
        m_entries.push_back(*hit.entry);
    }

    endResetModel();
}

void PointerMenuModel::logKeyCoverage() const
{
    if (!commandIndex()) {
        return;
    }

    const CommandEntryList all = commandIndex()->entries(ViewKind::Score);

    size_t withKeys = 0;
    size_t notationTotal = 0;
    size_t notationWithKeys = 0;
    for (const CommandEntry& entry : all) {
        const bool hasKeys = !entry.keys.isEmpty();
        withKeys += hasKeys ? 1 : 0;
        if (entry.module == "notation") {
            ++notationTotal;
            notationWithKeys += hasKeys ? 1 : 0;
        }
    }

    LOGI() << "pointer menu key coverage: all " << all.size() << " commands, " << withKeys << " with keys, "
           << (all.size() - withKeys) << " without; notation " << notationTotal << " commands, " << notationWithKeys
           << " with keys, " << (notationTotal - notationWithKeys) << " without";

    for (const CommandEntry& entry : all) {
        if (entry.keys.isEmpty()) {
            LOGD() << "pointer menu, no keys: " << entry.command.toString() << " (" << entry.module << ")";
        }
    }
}

int PointerMenuModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant PointerMenuModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size())) {
        return QVariant();
    }

    const CommandEntry& entry = m_entries[index.row()];

    switch (role) {
    case TitleRole: return entry.title.toQString();
    case KeysRole: return entry.keys.toQString();
    case SectionRole: return static_cast<int>(entry.section);
    case SectionTitleRole: return sectionTitle(entry.section).toQString();
    case IconRole: return static_cast<int>(entry.icon);
    default: break;
    }

    return QVariant();
}

QHash<int, QByteArray> PointerMenuModel::roleNames() const
{
    return {
        { TitleRole, "title" },
        { KeysRole, "keys" },
        { SectionRole, "section" },
        { SectionTitleRole, "sectionTitle" },
        { IconRole, "icon" },
    };
}
