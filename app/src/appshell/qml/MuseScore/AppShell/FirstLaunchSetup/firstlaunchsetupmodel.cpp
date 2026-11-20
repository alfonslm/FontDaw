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
#include "firstlaunchsetupmodel.h"

#include "translation.h"
#include "global/async/async.h"

using namespace muse;
using namespace mu;
using namespace mu::appshell;

//! NOTE FontDaw: the Playback page (Muse Hub / Muse Sounds) and the Tutorials page (MuseScore's video
//! playlist, update-check opt-in) are not shown; PlaybackPage.qml and TutorialsPage.qml stay in the tree.
// TODO(FontDaw FEATURES §3, §10): add an audio device (input + output) page.
FirstLaunchSetupModel::FirstLaunchSetupModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    m_pages = {
        Page { "ThemesPage.qml", "musescore://notation" },
    };
}

void FirstLaunchSetupModel::load()
{
    setCurrentPageIndex(0);
}

int FirstLaunchSetupModel::numberOfPages() const
{
    return m_pages.size();
}

int FirstLaunchSetupModel::currentPageIndex() const
{
    return m_currentPageIndex;
}

QVariantMap FirstLaunchSetupModel::Page::toMap() const
{
    return {
        { "url", url },
    };
}

QVariantMap FirstLaunchSetupModel::currentPage() const
{
    if (m_currentPageIndex < 0 || m_currentPageIndex >= m_pages.size()) {
        return {};
    }

    return m_pages.at(m_currentPageIndex).toMap();
}

bool FirstLaunchSetupModel::canGoBack() const
{
    return m_currentPageIndex > 0;
}

bool FirstLaunchSetupModel::canGoForward() const
{
    return m_currentPageIndex < m_pages.size() - 1;
}

bool FirstLaunchSetupModel::canFinish() const
{
    return m_currentPageIndex == m_pages.size() - 1;
}

void FirstLaunchSetupModel::setCurrentPageIndex(int index)
{
    if (index == m_currentPageIndex || index < 0 || index >= m_pages.size()) {
        return;
    }

    m_currentPageIndex = index;
    emit currentPageChanged();

    async::Async::call(this, [this]() {
        interactive()->open(m_pages.at(m_currentPageIndex).backgroundUri);
    });
}

bool FirstLaunchSetupModel::askAboutClosingEarly()
{
    //! NOTE FontDaw: plain confirmation, without the Muse Hub promotion (FD-003, FD-004)
    const std::string title = muse::trc("appshell/gettingstarted", "Are you sure you want to cancel?");

    const IInteractive::ButtonData keepGoingBtn {
        IInteractive::Button::Continue,
        muse::trc("appshell/gettingstarted", "Keep going"),
        true,
        false,
#ifdef Q_OS_WINDOWS
        IInteractive::AcceptRole
#else
        IInteractive::ContinueRole
#endif
    };

    const IInteractive::ButtonDatas buttons {
        interactive()->buttonData(IInteractive::Button::Cancel), keepGoingBtn
    };

    IInteractive::Result result = interactive()->warningSync(title, IInteractive::Text(), buttons, int(IInteractive::Button::Cancel));

    return result.standardButton() == IInteractive::Button::Cancel;
}

void FirstLaunchSetupModel::finish()
{
    configuration()->setHasCompletedFirstLaunchSetup(true);
}
