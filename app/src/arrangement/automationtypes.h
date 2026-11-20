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

#pragma once

#include <map>

#include "engraving/automation/automationtypes.h"

// FEATURES §8. Automation is NOT new work from scratch. The inherited code already has:
//   - data:     src/engraving/automation/ (AutomationData, AutomationCurve, AutomationPoint,
//               AutomationType { Dynamics, Tempo, Volume, Pan })
//   - storage:  automation.json in the project file (engraving/rw/mscsaver.cpp, AutomationRW)
//   - editing:  an "automation mode" inside the score view
//               (notation/inotationautomation.h, notationscene/.../notationautomationcontroller.*)
//   - playback: the engine applies volume/pan curves
//               (muse/framework/audio/engine/internal/nodes/automationcontrolnode.*)
// How finished that feature is has not been checked in a running build: check first.
//
// Its curve keys are score-shaped (whole score, instrument, staff/voice). Audio tracks are none
// of those, so their curves are stored here, in the arrangement, using the same curve type.
// That keeps upstream files untouched.

namespace mu::arrangement {
using AutomationType = mu::engraving::AutomationType;   // use Volume and Pan for audio tracks
using AutomationCurve = mu::engraving::AutomationCurve; // utick -> point, shared with the score

struct AutomationLane {
    AutomationCurve curve;
    bool enabled = true;  // "read" on/off
    bool visible = false; // lane shown under the track in the arrangement view
};

using AutomationLanes = std::map<AutomationType, AutomationLane>;

// TODO(FontDaw FEATURES §8):
//  - Serialise curves with the same point format AutomationRW uses, so one reader serves both.
//  - Show instrument (notation) tracks' existing Volume/Pan curves as lanes in the arrangement
//    view too: read/edit them through notation::INotationAutomation, do not copy them.
//  - Tier 2: plugin parameters (needs a stable parameter id from the VST host:
//    muse/framework/vst/internal/vstaudioclient.*), aux send levels, mute.
}
