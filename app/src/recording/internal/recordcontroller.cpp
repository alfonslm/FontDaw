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

// DRAFT - NOT BUILT YET. Part of the FontDaw handoff: docs/fontdaw/HANDOFF.md

#include "recordcontroller.h"

using namespace mu::recording;

void RecordController::init()
{
    // TODO(FontDaw FD-010): subscribe to playback status changes: if transport stops for any
    // reason while Recording (end of score, user pressed stop/space), finish the take.
}

// Implementation notes.
//
// startRecording():
//   1. Check preconditions (see interface). Collect armed tracks; v1 supports exactly one.
//   2. Open a TakeWriter (takewriter.h) for that track's input.
//   3. Ask the driver to deliver input for that channel pair (FRAMEWORK_CHANGES F2).
//   4. Start playback from the current position; honour count-in. Keep nothing until the
//      count-in ends; m_recordStartPos = transport position of the first kept sample.
//   5. State -> CountIn/Recording.
//
// stopRecording():
//   1. Stop transport, stop input delivery, drain and close the TakeWriter (off the UI thread).
//   2. assetStore()->importFile(takePath) -> AudioAsset.
//   3. Clip start = m_recordStartPos - inputLatency - outputLatency (FRAMEWORK_CHANGES F3).
//      Why both: the player heard the music outputLatency late and the card delivered their
//      playing inputLatency late. Until F3 exists use a user-set "recording offset (ms)"
//      preference and say so in the UI; do not pretend it is compensated.
//      Convert seconds -> tick through the tempo map and call arrangement()->addClip().
//      If the clip would overlap existing clips, v1 trims the old ones (tape behaviour) as
//      part of the same undo step.
//   4. takeFinished.send(result). State -> Idle.
//   The whole take (clip + asset) is one undo step; undo removes the clip but keeps the file
//   until the project is closed, so redo works.
//
// Input monitoring: route the live input through the armed track's mixer channel so the
// performer hears effects. That is engine work (F2 step 3). Warn about feedback when no
// headphones can be assumed: monitoring is OFF by default.
//
// onMidiEvent(): append to an in-memory take { event, transportPos }. On stop hand the take to
// the piano-roll performance layer (src/pianoroll/README.md). Until that exists, drop it.
