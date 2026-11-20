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

#include "commandsections.h"

#include <string_view>

#include "global/types/translatablestring.h"

using namespace muse;
using namespace mu::pointertools;

namespace {
struct ExactEntry {
    std::string_view path;  // "module/name", the part of the command after "command://"
    Section section;
};

struct SubstringRule {
    std::string_view needle;  // looked for in the command's name (the part after "<module>/")
    Section section;
};

// Commands that a substring rule would put elsewhere, or not at all. Checked first.
constexpr ExactEntry EXACT_TABLE[] = {
    // Articulations (the palette's articulations, and the string and wind techniques)
    { "notation/toggle-staccato", Section::Articulations },
    { "notation/toggle-tenuto", Section::Articulations },
    { "notation/toggle-marcato", Section::Articulations },
    { "notation/toggle-sforzato", Section::Articulations },
    { "notation/toggle-laissez-vibrer", Section::Articulations },
    { "notation/add-up-bow", Section::Articulations },
    { "notation/add-down-bow", Section::Articulations },
    { "notation/add-scoop", Section::Articulations },
    { "notation/add-dip", Section::Articulations },
    { "notation/add-dive", Section::Articulations },
    { "notation/add-pre-dive", Section::Articulations },
    { "notation/add-pre-bend", Section::Articulations },
    { "notation/add-slight-bend", Section::Articulations },
    { "notation/add-standard-bend", Section::Articulations },
    { "notation/add-grace-note-bend", Section::Articulations },
    { "notation/add-hammer-on-pull-off", Section::Articulations },

    // Notes
    { "notation/add-parentheses", Section::Notes },
    { "notation/add-note", Section::Notes },
    { "notation/regroup-rhythms", Section::Notes },
    { "notation/realtime-advance", Section::Notes },
    { "notation/slash-fill", Section::Notes },
    { "notation/slash-rhythm", Section::Notes },
    { "notation/extend-to-next-note", Section::Notes },

    // Measures and repeats
    { "notation/unroll-repeats", Section::Measures },

    // Edit
    { "notation/repeat-selection", Section::Edit },
    { "notation/staff-explode", Section::Edit },
    { "notation/staff-implode", Section::Edit },
    { "notation/toggle-insert-mode", Section::Edit },
    { "palette/apply-current-element", Section::Edit },

    // Palette panel
    { "palette/collapse-all", Section::View },
    { "palette/expand-all", Section::View },
    { "palette/search", Section::View },
    { "palette/open-customize-kit", Section::View },
    { "palette/toggle-drag-enabled", Section::View },
    { "palette/toggle-master-palette", Section::View },
    { "palette/toggle-single-click-to-open", Section::View },
    { "palette/toggle-single-palette", Section::View },

    // View
    { "notation/toggle-concert-pitch", Section::View },
};

// Ordered: the first rule whose needle is in the name wins. The order matters, for example
// "add-slur" is Lines and not Notes, "select-…" and "goto-…" come before "note-", frames come
// before text ("add-frame-text").
constexpr SubstringRule RULES[] = {
    // Things shown or hidden (before the others: "show-frames", "toggle-visible")
    { "show-", Section::View },
    { "visible", Section::View },
    { "hide-empty", Section::View },

    // Selection and navigation (before "note-", "measure")
    { "select", Section::Selection },
    { "goto-", Section::Selection },

    // Small families with an obvious word
    { "fermata", Section::Fermatas },
    { "breath", Section::BreathsAndPauses },
    { "caesura", Section::BreathsAndPauses },
    { "clef", Section::Clefs },
    { "key-sig", Section::KeyAndTimeSignatures },
    { "keysig", Section::KeyAndTimeSignatures },
    { "time-sig", Section::KeyAndTimeSignatures },
    { "timesig", Section::KeyAndTimeSignatures },

    // Lines (before Notes: "add-noteline"; before Dynamics: hairpins)
    { "slur", Section::Lines },
    { "hairpin", Section::Lines },
    { "ottava", Section::Lines },
    { "8va", Section::Lines },
    { "8vb", Section::Lines },
    { "pedal", Section::Lines },
    { "noteline", Section::Lines },
    { "glissando", Section::Lines },
    { "volta", Section::Lines },

    { "dynamic", Section::Dynamics },

    // Ornaments, grace notes (before "note-")
    { "trill", Section::Ornaments },
    { "mordent", Section::Ornaments },
    { "add-turn", Section::Ornaments },
    { "shake", Section::Ornaments },
    { "tremblement", Section::Ornaments },
    { "haydn", Section::Ornaments },
    { "grace", Section::Ornaments },
    { "acciaccatura", Section::Ornaments },
    { "appoggiatura", Section::Ornaments },

    // Tuplets: tuplet, triplet, duplet, quadruplet, … all end in "plet"
    { "plet", Section::Tuplets },

    // Voices (before "swap": "swap-voice-x12")
    { "voice", Section::Voices },

    // Layout things that name a measure or a system (before Measures)
    { "move-measure", Section::Layout },
    { "move-system", Section::Layout },

    // Measures
    { "measure", Section::Measures },
    { "barline", Section::Measures },
    { "mmrest", Section::Measures },

    // Frames (before text: "add-frame-text", "append-textframe")
    { "frame", Section::Layout },
    { "hbox", Section::Layout },
    { "vbox", Section::Layout },

    // Text
    { "text", Section::Text },
    { "lyrics", Section::Text },
    { "tempo", Section::Text },
    { "rehearsal", Section::Text },
    { "chord-text", Section::Text },
    { "realizechord", Section::Text },
    { "figured-bass", Section::Text },
    { "roman-numeral", Section::Text },
    { "nashville", Section::Text },
    { "fretboard", Section::Text },

    // Layout
    { "break", Section::Layout },
    { "spacer", Section::Layout },
    { "layout", Section::Layout },
    { "style", Section::Layout },
    { "page", Section::Layout },
    { "system", Section::Layout },
    { "stretch", Section::Layout },
    { "autoplace", Section::Layout },
    { "beam", Section::Layout },
    { "bracket", Section::Layout },
    { "brace", Section::Layout },
    { "lock", Section::Layout },
    { "snap", Section::Layout },
    { "grid", Section::Layout },
    { "image", Section::Layout },

    // Durations (before "note-": "set-duration-…")
    { "duration", Section::Durations },
    { "-dot", Section::Durations },

    // Notes
    { "pitch-", Section::Notes },
    { "note-", Section::Notes },
    { "interval", Section::Notes },
    { "-sharp", Section::Notes },
    { "-flat", Section::Notes },
    { "-nat", Section::Notes },
    { "accidental", Section::Notes },
    { "enharmonic", Section::Notes },
    { "-tie", Section::Notes },
    { "-rest", Section::Notes },
    { "transpose", Section::Notes },
    { "enter-fret", Section::Notes },
    { "drum-note", Section::Notes },

    // Edit
    { "copy", Section::Edit },
    { "cut", Section::Edit },
    { "paste", Section::Edit },
    { "delete", Section::Edit },
    { "undo", Section::Edit },
    { "redo", Section::Edit },
    { "flip", Section::Edit },
    { "swap", Section::Edit },
    { "mirror", Section::Edit },
    { "reset", Section::Edit },
    { "cancel", Section::Edit },
    { "move-", Section::Edit },

    // View
    { "zoom", Section::View },
    { "view", Section::View },
    { "panel", Section::View },
    { "keyboard", Section::View },

    // Playback
    { "play", Section::Playback },
    { "loop", Section::Playback },
    { "metronome", Section::Playback },
    { "count-in", Section::Playback },
};

//! The part of the command after "command://<module>/" is what the rules look at
std::string nameOf(const std::string& path)
{
    const size_t slash = path.find('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
}

Section mu::pointertools::sectionOf(const muse::rcommand::Command& command, const std::string& module)
{
    // Modules that are one section as a whole
    if (module == "arrangement") {
        return Section::Audio;
    }
    if (module == "playback") {
        return Section::Playback;
    }
    if (module == "project" || module == "appshell" || module == "pointertools") {
        return Section::General;
    }
    if (module != "notation" && module != "palette") {
        return Section::Other;
    }

    const std::string& path = command.path();
    const std::string fullPath = module + "/" + nameOf(path);
    for (const ExactEntry& entry : EXACT_TABLE) {
        if (entry.path == fullPath) {
            return entry.section;
        }
    }

    const std::string name = nameOf(path);
    for (const SubstringRule& rule : RULES) {
        if (name.find(rule.needle) != std::string::npos) {
            return rule.section;
        }
    }

    return Section::Other;
}

String mu::pointertools::sectionTitle(Section section)
{
    switch (section) {
    case Section::Articulations: return TranslatableString("pointertools", "Articulations").translated();
    case Section::Ornaments: return TranslatableString("pointertools", "Ornaments").translated();
    case Section::Fermatas: return TranslatableString("pointertools", "Fermatas").translated();
    case Section::BreathsAndPauses: return TranslatableString("pointertools", "Breaths and pauses").translated();
    case Section::Clefs: return TranslatableString("pointertools", "Clefs").translated();
    case Section::KeyAndTimeSignatures: return TranslatableString("pointertools", "Key and time signatures").translated();
    case Section::Lines: return TranslatableString("pointertools", "Lines").translated();
    case Section::Dynamics: return TranslatableString("pointertools", "Dynamics").translated();
    case Section::Text: return TranslatableString("pointertools", "Text").translated();
    case Section::Notes: return TranslatableString("pointertools", "Notes").translated();
    case Section::Durations: return TranslatableString("pointertools", "Durations").translated();
    case Section::Voices: return TranslatableString("pointertools", "Voices").translated();
    case Section::Tuplets: return TranslatableString("pointertools", "Tuplets").translated();
    case Section::Measures: return TranslatableString("pointertools", "Measures").translated();
    case Section::Layout: return TranslatableString("pointertools", "Layout").translated();
    case Section::Selection: return TranslatableString("pointertools", "Selection").translated();
    case Section::Edit: return TranslatableString("pointertools", "Edit").translated();
    case Section::View: return TranslatableString("pointertools", "View").translated();
    case Section::Playback: return TranslatableString("pointertools", "Playback").translated();
    case Section::Audio: return TranslatableString("pointertools", "Audio").translated();
    case Section::General: return TranslatableString("pointertools", "General").translated();
    case Section::Other: return TranslatableString("pointertools", "Other").translated();
    }
    return String();
}
