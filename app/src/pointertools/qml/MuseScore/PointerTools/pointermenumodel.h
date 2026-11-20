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

#include <QAbstractListModel>
#include <QPointF>
#include <QQmlEngine>
#include <QQuickItem>

#include "global/modularity/ioc.h"
#include "global/async/asyncable.h"

#include "icommandindex.h"
#include "ipointertoolscontroller.h"

namespace mu::pointertools {
//! The pointer menu's list: the index's entries for the view, or the search results while
//! searchText is not empty. Roles: title, keys, section (int), sectionTitle, icon (int, an
//! IconCode). The pop-up (internal/PointerMenuPopup.qml) shows it; PointerToolsLayer.qml opens it
//! when openRequested() comes.
class PointerMenuModel : public QAbstractListModel, public muse::Contextable, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

    muse::ContextInject<ICommandIndex> commandIndex = { this };
    muse::ContextInject<IPointerToolsController> controller = { this };

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        KeysRole,
        SectionRole,
        SectionTitleRole,
        IconRole,
    };

    explicit PointerMenuModel(QObject* parent = nullptr);

    QString searchText() const;
    void setSearchText(const QString& text);

    //! Fills the list for the score view (the only view with a pointer menu for now).
    //! Subscribes to the controller; call from Component.onCompleted (needs the QML context).
    Q_INVOKABLE void init();
    Q_INVOKABLE void load();
    //! Runs the entry at row (Enter runs the highlighted row). The pop-up closes first.
    Q_INVOKABLE void run(int row);

    //! The pointer's position in item's coordinates (the layer opens the menu there).
    Q_INVOKABLE QPointF cursorPosIn(QQuickItem* item) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void searchTextChanged();
    //! The command "Command menu" ran (Tools menu, later Tab): open the pop-up.
    void openRequested();

private:
    void reload();
    void logKeyCoverage() const;

    QString m_searchText;
    CommandEntryList m_all;     //! the entries taken when the menu opened
    CommandEntryList m_entries; //! m_all ranked for searchText
};
}
