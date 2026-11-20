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

#include "ringinputarea.h"

#include <QCursor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QWindow>

#include "log.h"
#include <QMouseEvent>
#include <QQuickWindow>

using namespace mu::pointertools;

RingInputArea::RingInputArea(QQuickItem* parent)
    : QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
}

bool RingInputArea::buttonDownAtStart() const
{
    return m_buttonDownAtStart;
}

bool RingInputArea::grabbing() const
{
    return m_grabbing;
}

void RingInputArea::startGrab()
{
    m_endAfterRelease = false;
    m_pressedHere = Qt::NoButton;
    m_buttonDownAtStart = QGuiApplication::mouseButtons() != Qt::NoButton;

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    grabMouse();

    m_grabbing = true;
    emit grabChanged();
}

void RingInputArea::endGrab()
{
    if (!m_grabbing) {
        return;
    }

    if (m_pressedHere != Qt::NoButton) {
        // the matching release ends the grab
        m_endAfterRelease = true;
        return;
    }

    finishGrab();
}

void RingInputArea::finishGrab()
{
    m_endAfterRelease = false;
    m_grabbing = false;

    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
    if (window() && window()->mouseGrabberItem() == this) {
        ungrabMouse();
    }

    emit grabChanged();
}

void RingInputArea::setKeysCaptured(bool captured)
{
    if (m_keysCaptured == captured) {
        return;
    }
    m_keysCaptured = captured;
    if (captured) {
        qApp->installEventFilter(this);
        m_focusWindowConnection = connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow* focusWindow) {
            if (focusWindow != window()) {
                emit focusLost();
            }
        });
        m_appStateConnection = connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
                emit focusLost();
            }
        });
    } else {
        qApp->removeEventFilter(this);
        disconnect(m_focusWindowConnection);
        disconnect(m_appStateConnection);
    }
}

static bool isRingKey(int key)
{
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
    case Qt::Key_Escape:
        return true;
    default:
        return false;
    }
}

bool RingInputArea::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_keysCaptured || (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress)) {
        return QQuickItem::eventFilter(watched, event);
    }

    auto keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() != Qt::NoModifier && keyEvent->modifiers() != Qt::KeypadModifier) {
        return QQuickItem::eventFilter(watched, event);
    }
    if (!isRingKey(keyEvent->key())) {
        return QQuickItem::eventFilter(watched, event);
    }

    // Only keys meant for the ring's own window: a dialog or another window keeps its keys
    if (!window() || QGuiApplication::focusWindow() != window()) {
        return QQuickItem::eventFilter(watched, event);
    }

    // ShortcutOverride is sent to the focused item: accepting it keeps the key from becoming a
    // shortcut. KeyPress reaches the filter once per object it is sent to (window, then items);
    // it is handled once, at the window, and stopped there.
    event->accept();
    if (event->type() == QEvent::KeyPress) {
        if (watched != window()) {
            return true;
        }
        emit keyPressed(keyEvent->key());
    }
    return true;
}

bool RingInputArea::anyButtonDown() const
{
    return QGuiApplication::mouseButtons() != Qt::NoButton;
}

QPointF RingInputArea::cursorPosition() const
{
    return mapFromGlobal(QCursor::pos());
}

void RingInputArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        // The ring closes (QML), and the press goes on to the view under it, whose context-menu
        // event then reopens the ring at the new spot
        if (!m_endAfterRelease) {
            emit pressed(event->position(), static_cast<int>(event->button()));
        }
        event->ignore();
        return;
    }

    m_pressedHere |= event->button();
    event->accept();

    if (!m_endAfterRelease) {
        emit pressed(event->position(), static_cast<int>(event->button()));
    }
}

void RingInputArea::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();

    if (!m_endAfterRelease) {
        emit pointerMoved(event->position());
    }
}

void RingInputArea::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();

    const bool pressedHere = m_pressedHere.testFlag(event->button());
    m_pressedHere &= ~Qt::MouseButtons(event->button());

    if (m_endAfterRelease) {
        if (m_pressedHere == Qt::NoButton) {
            finishGrab();
        }
        return;
    }

    // A release of a button pressed here (a click) was handled with its press; the release of the
    // button that opened the ring has no press here, and is the one the ring reacts to.
    if (!pressedHere) {
        emit released(event->position(), static_cast<int>(event->button()));
    }
}

void RingInputArea::hoverMoveEvent(QHoverEvent* event)
{
    if (!m_endAfterRelease) {
        emit pointerMoved(event->position());
    }
    event->accept();
}

void RingInputArea::mouseUngrabEvent()
{
    // Something else took the mouse (the pop-up of today's menu after "More…", a window change):
    // the release of a button pressed here will not come, so nothing may wait for it, or the next
    // click on the score would be swallowed (review of #195)
    m_pressedHere = Qt::NoButton;
    if (m_endAfterRelease) {
        finishGrab();
        return;
    }
    if (m_grabbing) {
        m_grabbing = false;
        emit grabChanged();
    }
}
