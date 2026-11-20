#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Measure how FontDaw's default shortcuts suit one hand on the keyboard and one on the mouse.
# See docs/fontdaw/design/shortcuts/03-fontdaw-today.md for what the numbers mean.
#
# Usage: tools/fontdaw/shortcut_reach.py [--layout NAME | --file PATH] [--list]
#   --layout NAME  musescore47 (default): the v2 base with the MuseScore 4.7 preset applied,
#                  as the app resolves it; musescore50: the v2 base alone (upstream's keys)
#   --file PATH    any v1 XML file (action codes are mapped to v2 commands through
#                  src/app/configs/data/fontdaw/v1-commands.json) or v2 JSON file (read as a
#                  complete list, no merge)
#   --list         also print every counted binding with its zone
# Bindings are keyed by v2 command URI. A key sequence of several chords ("Shift+D, A") counts
# as its hardest chord. The hand-picked subsets below are written as v1 action codes and
# translated through the map.
#
# Python 3 standard library only. Reads repository files, changes nothing.

import argparse
import os
import json
import pathlib
import sys
import xml.etree.ElementTree as ET
from collections import Counter

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "fontdaw", "shortcuts"))
import make_musescore47_preset as m47  # noqa: E402  (replays shortcuts v2's merge)

DATA = os.path.join(ROOT, "src", "app", "configs", "data")
MAP_FILE = os.path.join(DATA, "fontdaw", "v1-commands.json")
LAYOUTS = ("musescore47", "musescore50")

# Qt QKeySequence::StandardKey values used in shortcuts.xml (<std>), with their Windows/Linux keys.
STD_KEYS = {"3": "Ctrl+O", "4": "Ctrl+W", "5": "Ctrl+S", "11": "Ctrl+Z", "12": "Ctrl+Shift+Z",
            "22": "Ctrl+F"}

# Reach zones for a left keyboard hand resting on A S D F (US key names; same physical keys on
# Nordic keyboards). See 01-yardstick.md.
L1 = set("QWERASDFZXCV") | {"SPACE"}
L2 = set("TGB12345") | {"TAB", "`"}
L3 = {"ESC", "F1", "F2", "F3", "F4", "F5", "Y", "H", "N", "6", "CAPS"}

# Characters that are shifted, AltGr or dead keys on the Swedish layout (xkb "se").
SWEDISH_AWKWARD = {"`": "dead key", "[": "AltGr+8", "]": "AltGr+9", "{": "AltGr+7",
                   "}": "AltGr+0", "\\": "AltGr++", "/": "Shift+7", ";": "Shift+,",
                   ":": "Shift+.", "=": "Shift+0", "~": "AltGr+dead key"}

# Two subsets chosen by hand (reasoning, see 03-fontdaw-today.md).
NOTE_ENTRY = [
    "note-input-by-note-name", "note-a", "note-b", "note-c", "note-d", "note-e", "note-f",
    "note-g", "chord-a", "chord-c", "rest", "pad-note-1", "pad-note-2", "pad-note-4",
    "pad-note-8", "pad-note-16", "pad-note-32", "pad-dot", "sharp", "flat", "nat", "tie",
    "pitch-up", "pitch-down", "pitch-up-octave", "pitch-down-octave", "interval3", "interval5",
    "double-duration", "half-duration", "add-slur", "voice-1", "voice-2", "triplet",
    "realtime-advance", "acciaccatura", "enh-both", "flip", "notation-move-left",
    "notation-move-right", "action://delete", "action://undo"]
MOUSE_SIDE = [
    "action://copy", "action://cut", "action://paste", "action://undo", "action://redo",
    "action://delete", "action://cancel", "notation-select-all", "play", "play-from-selection",
    "pause-and-select", "zoomin", "zoomout", "zoom100", "file-save", "notation-move-left",
    "notation-move-right", "select-next-chord", "select-prev-chord", "select-next-measure",
    "select-prev-measure", "first-element", "last-element", "scr-prev", "scr-next",
    "toggle-mixer", "toggle-timeline", "toggle-instruments", "toggle-palettes",
    "toggle-properties-panel", "time-delete", "insert-measure", "append-measure",
    "notation-paste-half", "notation-paste-double", "notation-swap", "find", "repeat-sel",
    "edit-element", "notation-context-menu", "select-staff-above", "select-staff-below",
    "toggle-visible", "instruments"]


# Keys that mean the same in every view of the FontDaw layout, counted with each view.
GLOBAL_KEYS = ["action://undo", "action://redo", "fontdaw-repeat-last", "fontdaw-pointer-menu", "play",
               "play-from-selection", "action://cancel", "action://copy", "action://cut",
               "action://paste", "file-save"]


def split_seq(seq):
    """Return (modifiers, base key) for a Qt portable-text sequence such as 'Ctrl+Shift+A'."""
    s = seq.strip()
    if s.endswith("++"):
        head, base = s[:-2], "+"
    elif s == "+":
        head, base = "", "+"
    else:
        parts = s.split("+")
        head, base = "+".join(parts[:-1]), parts[-1]
    mods = [m.upper() for m in head.split("+") if m]
    base = base.upper()
    if "NUM" in mods:
        mods = [m for m in mods if m != "NUM"]
        base = "NUM" + base
    base = {"DEL": "DELETE", "ESCAPE": "ESC", "RETURN": "ENTER"}.get(base, base)
    return mods, base


def zone(base):
    if base.startswith("NUM"):
        return "R"
    if base in L1:
        return "L1"
    if base in L2:
        return "L2"
    if base in L3:
        return "L3"
    return "R"


def load_map():
    with open(MAP_FILE, encoding="utf-8") as f:
        return json.load(f)["map"]


def read_xml_bindings(path, cmap):
    """v1 XML -> [(command URI, [sequences])], one row per v1 code (two codes that the map sends
    to one command stay two rows, so the counts equal those of the v1 file); codes the map doesn't
    know keep their code as key. Returns the rows and the sorted unmapped codes."""
    root = ET.parse(path).getroot()
    out, unmapped = [], set()
    for sc in root.iter("SC"):
        code = sc.findtext("key") or ""
        seqs = [s.text for s in sc.findall("seq") if s.text]
        std = sc.findtext("std")
        if std and std in STD_KEYS:
            seqs = [STD_KEYS[std]] + seqs
        key = cmap.get(code)
        if key is None:
            key = code
            unmapped.add(code)
        out.append((key, seqs))
    return out, sorted(unmapped)


def read_json_bindings(path):
    """v2 JSON (a full list) -> [(command URI, [sequences], scope)]"""
    return [(e["command"], e["sequences"], e["scope"]) for e in m47.read_json(pathlib.Path(path))]


def resolve_layout(name):
    """the base, or the base with the 4.7 preset applied, as CommandShortcutsRegister::reload() does"""
    base = m47.read_json(m47.DATA / "shortcuts.json")
    pbase = m47.platform_base(base, "other")
    resolved = pbase if name == "musescore50" else m47.merge(m47.read_json(m47.OUT["other"]), pbase)
    return [(e["command"], e["sequences"], e["scope"]) for e in resolved]


ZONE_ORDER = ["L1", "L2", "L3", "R"]


def split_multi(seq):
    """Like split_seq, but a sequence of chords ('Shift+D, A') counts as its hardest chord:
    the farthest zone and the most modifiers of any chord in it."""
    chords = [split_seq(c) for c in seq.split(", ")] if ", " in seq else [split_seq(seq)]
    mods = max((m for m, _ in chords), key=len)
    base = max((b for _, b in chords), key=lambda b: ZONE_ORDER.index(zone(b)))
    return mods, base


def summary(label, rows):
    n = len(rows)
    z, m, kinds = Counter(), Counter(), Counter()
    for _, seq in rows:
        mods, base = split_multi(seq)
        z[zone(base)] += 1
        m[min(len(mods), 2)] += 1
        if len(mods) == 1:
            kinds[mods[0]] += 1
    pct = lambda c: f"{c} ({100 * c / n:.0f}%)" if n else "0"
    print(f"{label}: {n} bindings")
    print("  zones:     L1 " + pct(z["L1"]) + ", L2 " + pct(z["L2"]) + ", L3 " + pct(z["L3"])
          + ", R " + pct(z["R"]))
    print("  modifiers: none " + pct(m[0]) + ", one " + pct(m[1]) + ", two or more " + pct(m[2])
          + "   (one: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())) + ")")


EXCLUDED = ("command://navigation/", "command://diagnostics/", "command://multiwindows/",
            "nav-", "diagnostic", "multiwindows")  # the last three: unmapped v1 codes kept as keys


def translate(codes, cmap, what):
    """v1 codes -> URIs; codes without a mapping (other than FontDaw's own fontdaw-* ids, which
    stay as they are) are printed."""
    out, missing = [], []
    for c in codes:
        if c in cmap:
            out.append(cmap[c])
        elif c.startswith("fontdaw-"):
            out.append(c)
        else:
            missing.append(c)
    if missing:
        print(f"{what}: no mapping for " + ", ".join(missing))
    return out


def main():
    ap = argparse.ArgumentParser(description="Reach and modifier load of FontDaw's default keys.")
    ap.add_argument("--layout", choices=LAYOUTS, default=None)
    ap.add_argument("--file", default=None)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    if args.file and args.layout:
        ap.error("--layout and --file exclude each other")

    cmap = load_map()
    unmapped, scope_of = [], {}
    if args.file:
        if args.file.endswith(".json"):
            rows = read_json_bindings(args.file)
            bindings = [(k, s) for k, s, _ in rows]
            scope_of = {k: sc for k, _, sc in rows}
        else:
            bindings, unmapped = read_xml_bindings(args.file, cmap)
        source = os.path.relpath(args.file, ROOT)
    else:
        name = args.layout or "musescore47"
        rows = resolve_layout(name)
        bindings = [(k, s) for k, s, _ in rows]
        scope_of = {k: sc for k, _, sc in rows}
        source = f"layout {name} (src/app/configs/data/shortcuts.json" + \
                 ("" if name == "musescore50" else " + shortcuts_musescore47.json") + ")"
    counted = [(k, s[0]) for k, s in bindings if s and not k.startswith(EXCLUDED)]
    print(f"Input: {source}  ({len(bindings)} commands)")
    print("Counted: first sequence of each command; keyboard navigation, diagnostics and")
    print("multi-window commands left out. Zones: L1 resting, L2 one stretch, L3 hand moves, R right side.\n")
    if unmapped:
        print("Unmapped codes (kept under their v1 code): " + ", ".join(unmapped) + "\n")
    summary("All bindings", counted)
    first = dict(counted)
    note_entry = translate(NOTE_ENTRY, cmap, "NOTE_ENTRY")
    mouse_side = translate(MOUSE_SIDE, cmap, "MOUSE_SIDE")
    global_keys = translate(GLOBAL_KEYS, cmap, "GLOBAL_KEYS")
    summary("Note-entry subset", [(k, first[k]) for k in note_entry if k in first])
    summary("Mouse-side subset", [(k, first[k]) for k in mouse_side if k in first])

    # FontDaw layout files (docs/fontdaw/design/shortcuts/06-fontdaw-layout/) add per-view entries.
    views = {"Arrangement view": ("fontdaw-clip-", "fontdaw-grid-", "fontdaw-snap-", "fontdaw-zoom-"),
             "Mixer view": ("fontdaw-mixer-",)}
    if any(k.startswith(p) for p in sum(views.values(), ()) for k in first):
        glob = [(k, first[k]) for k in global_keys if k in first]
        for label, prefixes in views.items():
            rows = [(k, sq) for k, sq in first.items() if k.startswith(prefixes)]
            summary(f"{label}, with the global keys", rows + glob)
        summary("Family commands (two-key sequences)", [(k, sq) for k, sq in first.items() if ", " in sq])

    plain, shifted = {}, {}
    for k, seqs in bindings:
        for s in seqs:
            mods, base = split_seq(s)
            if not mods:
                plain.setdefault(base, []).append(k)
            elif mods == ["SHIFT"]:
                shifted.setdefault(base, []).append(k)
    print("\nLeft-hand keys without a modifier (L1, L2, L3):")
    for b in list("QWERTASDFGZXCVB12345") + ["TAB", "`", "SPACE", "ESC", "Y", "H", "N", "6"]:
        print(f"  {b:6} {', '.join(plain.get(b, ['-- free --']))}"
              f"   | Shift: {', '.join(shifted.get(b, ['free']))}")

    print("\nBindings whose key is shifted, AltGr or dead on a Swedish keyboard, or use Ctrl+Alt:")
    for k, seqs in bindings:
        for s in seqs:
            mods, base = split_seq(s)
            why = []
            if base in SWEDISH_AWKWARD:
                why.append(f"'{base}' is {SWEDISH_AWKWARD[base]} on Swedish")
            if mods == ["SHIFT"] and base in {",", "."}:
                why.append(f"Shift+{base} types '{';' if base == ',' else ':'}' on Swedish")
            if "CTRL" in mods and "ALT" in mods:
                why.append("Ctrl+Alt is AltGr on some Windows layouts")
            if why:
                print(f"  {k:52} {s:24} " + "; ".join(why))

    if scope_of:
        per_scope = Counter(scope_of[k] for k, s in bindings if s)
        print("\nBindings per scope (commands with a key):")
        for c, n in per_scope.most_common():
            print(f"  {c:40} {n}")

    if args.list:
        print("\nEvery counted binding:")
        for k, s in counted:
            mods, base = split_multi(s)
            print(f"  {zone(base):3} {len(mods)}  {s:26} {k}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
