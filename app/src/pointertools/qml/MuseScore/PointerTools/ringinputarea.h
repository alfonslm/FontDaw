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

#include <QPointF>
#include <QQmlEngine>
#include <QQuickItem>

namespace mu::pointertools {
//! Takes the mouse for the ring menu: press, move and release go to the ring, not to the view
//! under it. The secondary button is usually still down when the context-menu event opens the
//! ring (Linux and macOS; Windows opens it on release), and its press belongs to the notation
//! view, so the ring grabs the mouse explicitly (startGrab) and gives it back on endGrab.
//! Fills the layer; accepts no button until startGrab, so it never gets in the view's way.
//! Tried in the app (Linux, Xvfb): the grab taken in the context-menu event does not take the
//! held button's moves and release from the score view, so PointerToolsLayer.qml follows them by
//! polling cursorPosition() and anyButtonDown() while the ring is open. Clicks after the release
//! do arrive here. The ring's keys come through setKeysCaptured(), not through focus.
class RingInputArea : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    //! A mouse button was down when the grab started (the one that opened the ring)
    Q_PROPERTY(bool buttonDownAtStart READ buttonDownAtStart NOTIFY grabChanged)
    Q_PROPERTY(bool grabbing READ grabbing NOTIFY grabChanged)

public:
    explicit RingInputArea(QQuickItem* parent = nullptr);

    bool buttonDownAtStart() const;
    bool grabbing() const;

    Q_INVOKABLE void startGrab();
    //! Whether any mouse button is down now (the ring polls it to see the opening button's release)
    Q_INVOKABLE bool anyButtonDown() const;
    //! Gives the mouse back. When a button that was pressed here is still down, the grab ends
    //! with its release, so the view under the ring does not get a release without a press.
    Q_INVOKABLE void endGrab();

    //! The pointer's position in this item's coordinates
    Q_INVOKABLE QPointF cursorPosition() const;

    //! While on, the ring's keys (arrows, Enter, Return, Space, Esc) go to keyPressed when they are
    //! meant for this item's window: the score view keeps the focus when the ring opens, and its
    //! shortcuts would take the arrows and Esc. Keys for any other window pass untouched, and
    //! focusLost is sent when this window stops being the focus window or the application goes
    //! inactive (the ring then closes). An application event filter, removed when off.
    Q_INVOKABLE void setKeysCaptured(bool captured);

signals:
    void grabChanged();
    void pointerMoved(const QPointF& pos);
    void pressed(const QPointF& pos, int button);
    void released(const QPointF& pos, int button);
    void keyPressed(int key);
    void focusLost();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void mouseUngrabEvent() override;

private:
    void finishGrab();

    bool m_keysCaptured = false;
    QMetaObject::Connection m_focusWindowConnection;
    QMetaObject::Connection m_appStateConnection;
    bool m_grabbing = false;
    bool m_buttonDownAtStart = false;
    bool m_endAfterRelease = false;
    Qt::MouseButtons m_pressedHere = Qt::NoButton;
};
}
