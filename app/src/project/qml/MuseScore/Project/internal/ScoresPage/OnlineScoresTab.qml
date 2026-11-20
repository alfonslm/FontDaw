/*
 * SPDX-License-Identifier: GPL-3.0-only
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

import QtQuick

import MuseScore.Project

//! NOTE FontDaw: the "My online scores" tab of ScoresPage.qml, moved into its own file so that it can be
//! loaded by URL (see the note there). Only used when the app is built with the cloud module.
CloudScoresView {
    id: root

    required property ScoresPageModel scoresPageModel
    required property Item searchField
    required property Item refreshButton

    viewType: scoresPageModel.viewType
    searchText: searchField.searchText

    navigationOrder: 4

    onCreateNewScoreRequested: {
        scoresPageModel.createNewScore()
    }

    onOpenScoreRequested: function(scorePath, displayName) {
        Qt.callLater(scoresPageModel.openScore, scorePath, displayName)
    }

    onRevealInFileBrowserRequested: function(scorePath) {
        Qt.callLater(scoresPageModel.revealInFileBrowser, scorePath)
    }

    onViewOnlineRequested: function(scoreId) {
        Qt.callLater(scoresPageModel.viewOnline, scoreId)
    }

    onDeleteScoreRequested: function(scoreId, scoreName) {
        Qt.callLater(scoresPageModel.deleteScore, scoreId, scoreName)
    }

    Connections {
        target: root.refreshButton

        function onClicked() {
            root.refresh()
        }
    }
}
