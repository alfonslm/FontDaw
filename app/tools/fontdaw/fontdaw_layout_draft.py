#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Build the FontDaw shortcut layout's key file for counting (shortcuts v2 format):
#   docs/fontdaw/design/shortcuts/06-fontdaw-layout/fontdaw-layout-draft.json
# from the resolved MuseScore 4.7 layout (the v2 base with shortcuts_musescore47.json applied, as the
# app resolves it) and the tables below (which follow the per-view files in that folder). The tables
# are written with v1 action codes and fontdaw-* ids; each is translated through
# src/app/configs/data/fontdaw/v1-commands.json. A code with no mapping, a fontdaw-* id and a
# command whose URI is in no source file are commands that don't exist yet: they are reported as
# NEW and not written. Also reports the legacy keys it had to drop because the FontDaw layout uses
# them, the commands left without a key, and two commands of one scope sharing a sequence (exit 1).
# The old fontdaw-layout-draft.xml in that folder is the v1-era output of this script, kept for
# history; it is no longer written.
#
# Usage: tools/fontdaw/fontdaw_layout_draft.py      (from anywhere; writes the file above)
# Python 3 standard library only. Not used by the build or the app.

import json
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "fontdaw", "shortcuts"))
import make_musescore47_preset as m47  # noqa: E402  (replays shortcuts v2's merge, writes presets)

OUT = os.path.join(ROOT, "docs", "fontdaw", "design", "shortcuts", "06-fontdaw-layout",
                   "fontdaw-layout-draft.json")

mapdata = json.loads(m47.MAP.read_text(encoding="utf-8"))
cmap, map_scopes = mapdata["map"], mapdata.get("scopes", {})

# the starting list: the resolved 4.7 layout (Linux/Windows), keyed by command URI
base = m47.read_json(m47.DATA / "shortcuts.json")
resolved = m47.merge(m47.read_json(m47.OUT["other"]), base)
base_scope = {e["command"]: e["scope"] for e in base}

# FontDaw bindings of the score view (outside typing mode) and global keys. First key = counted.
score = {
 "action://undo": ["Z"], "action://redo": ["Shift+Z"], "action://delete": ["X"],
 "repeat-sel": ["Ctrl+D"], "fontdaw-repeat-last": ["V"], "fontdaw-pointer-menu": ["Tab"],
 "nav-next-panel": ["Ctrl+F6"], "nav-prev-panel": ["Ctrl+Shift+F6"],
 "fontdaw-mouse-entry": ["B"],
 "flat": ["Q"], "nat": ["W"], "sharp": ["E"], "flat2": ["Shift+Q"], "sharp2": ["Shift+E"],
 "pitch-up": ["R"], "pitch-down": ["F"], "pitch-up-octave": ["Shift+R"], "pitch-down-octave": ["Shift+F"],
 "notation-move-left": ["A"], "notation-move-right": ["D"],
 "add-slur": ["S"], "tie": ["T"], "rest": ["G"], "flip": ["C"], "time-delete": ["Shift+X"],
 "pad-note-16": ["1"], "pad-note-8": ["2"], "pad-note-4": ["3"], "pad-note-2": ["4"], "pad-note-1": ["5"],
 "pad-note-32": ["Ctrl+1"], "pad-note-64": ["Ctrl+2"], "note-breve": ["Ctrl+5"],
 "fontdaw-pad-note-16-dotted": ["Shift+1"], "fontdaw-pad-note-8-dotted": ["Shift+2"],
 "fontdaw-pad-note-4-dotted": ["Shift+3"], "fontdaw-pad-note-2-dotted": ["Shift+4"],
 "fontdaw-pad-note-1-dotted": ["Shift+5"],
 # families
 "add-dynamic": ["Shift+D, D"],
 "fontdaw-dynamic-pp": ["Shift+D, Q"], "fontdaw-dynamic-p": ["Shift+D, A"], "fontdaw-dynamic-mp": ["Shift+D, S"],
 "fontdaw-dynamic-f": ["Shift+D, F"], "fontdaw-dynamic-ff": ["Shift+D, R"], "fontdaw-dynamic-sfz": ["Shift+D, W"],
 "fontdaw-dynamic-fp": ["Shift+D, E"], "add-hairpin": ["Shift+D, Z"], "add-hairpin-reverse": ["Shift+D, X"],
 "add-sforzato": ["Shift+A, A"], "add-staccato": ["Shift+A, S"], "add-tenuto": ["Shift+A, D"],
 "add-marcato": ["Shift+A, F"], "fontdaw-add-staccatissimo": ["Shift+A, Q"],
 "fontdaw-add-fermata": ["Shift+A, W"], "fontdaw-add-breath": ["Shift+A, E"],
 "fontdaw-family-slur": ["Shift+S, A"], "add-8va": ["Shift+S, W"], "add-8vb": ["Shift+S, S"],
 "fontdaw-add-pedal": ["Shift+S, D"], "fontdaw-add-trill-line": ["Shift+S, F"],
 "fontdaw-add-glissando": ["Shift+S, E"], "fontdaw-add-let-ring": ["Shift+S, Z"],
 "fontdaw-add-palm-mute": ["Shift+S, X"],
 "triplet": ["Shift+T, 3"], "duplet": ["Shift+T, 2"], "quadruplet": ["Shift+T, 4"],
 "quintuplet": ["Shift+T, 5"], "tuplet": ["Shift+T, Q"],
 "acciaccatura": ["Shift+G, A"], "appoggiatura": ["Shift+G, S"], "grace16": ["Shift+G, D"],
 "grace32": ["Shift+G, F"], "grace8after": ["Shift+G, Q"],
 "interval2": ["Shift+C, 2"], "interval3": ["Shift+C, 3"], "interval4": ["Shift+C, 4"],
 "interval5": ["Shift+C, 5"], "interval6": ["Shift+C, Q"], "interval7": ["Shift+C, W"],
 "interval8": ["Shift+C, E"],
 "interval-2": ["Shift+C, Shift+2"], "interval-3": ["Shift+C, Shift+3"], "interval-4": ["Shift+C, Shift+4"],
 "interval-5": ["Shift+C, Shift+5"], "interval-6": ["Shift+C, Shift+Q"], "interval-7": ["Shift+C, Shift+W"],
 "interval-8": ["Shift+C, Shift+E"],
 "voice-1": ["Shift+V, 1"], "voice-2": ["Shift+V, 2"], "voice-3": ["Shift+V, 3"], "voice-4": ["Shift+V, 4"],
 "insert-measure": ["Shift+B, A"], "append-measure": ["Shift+B, S"], "fontdaw-delete-measures": ["Shift+B, D"],
 "system-break": ["Shift+B, Q"], "page-break": ["Shift+B, W"], "section-break": ["Shift+B, E"],
 "fontdaw-double-barline": ["Shift+B, F"],
 "staff-text": ["Shift+W, S"], "tempo": ["Shift+W, T"], "rehearsalmark-text": ["Shift+W, R"],
 "lyrics": ["Shift+W, A"], "chord-text": ["Shift+W, C"], "expression-text": ["Shift+W, E"],
 "system-text": ["Shift+W, W"],
}
# Commands that lose their single key in the FontDaw layout (reached from the Tab menu / M2).
no_key = ["toggle-visible", "mirror-note", "instruments", "toggle-percussion-panel", "toggle-piano-keyboard"]
         
arrangement = {
 "fontdaw-clip-split": ["S"], "fontdaw-clip-split-at-playhead": ["Shift+S"], "fontdaw-clip-glue": ["G"],
 "fontdaw-clip-fade-in-to-pointer": ["Q"], "fontdaw-clip-fade-out-from-pointer": ["E"],
 "fontdaw-clip-trim-start-to-pointer": ["Shift+Q"], "fontdaw-clip-trim-end-to-pointer": ["Shift+E"],
 "fontdaw-clip-gain-up": ["R"], "fontdaw-clip-gain-down": ["F"],
 "fontdaw-clip-prev": ["A"], "fontdaw-clip-next": ["D"], "fontdaw-clip-mute": ["C"],
 "fontdaw-clip-delete": ["X"], "fontdaw-clip-duplicate": ["Ctrl+D"],
 "fontdaw-grid-bar": ["1"], "fontdaw-grid-beat": ["2"], "fontdaw-grid-half-beat": ["3"],
 "fontdaw-grid-quarter-beat": ["4"], "fontdaw-snap-toggle": ["5"], "fontdaw-zoom-to-selection": ["Shift+F"],
}
mixer = {
 "fontdaw-mixer-mute": ["A"], "fontdaw-mixer-solo": ["S"], "fontdaw-mixer-arm": ["D"],
 "fontdaw-mixer-gain-up": ["R"], "fontdaw-mixer-gain-down": ["F"],
 "fontdaw-mixer-gain-up-fine": ["Shift+R"], "fontdaw-mixer-gain-down-fine": ["Shift+F"],
 "fontdaw-mixer-pan-left": ["Q"], "fontdaw-mixer-pan-right": ["E"], "fontdaw-mixer-pan-centre": ["W"],
 "fontdaw-mixer-reset-control": ["X"],
}

def uri_of(code):
    return cmap.get(code)


# a mapped URI that is in no source file doesn't exist either
mapped_missing = set(m47.unknown_uris({u for t in (score, arrangement, mixer, ) for c in t
                                       for u in [uri_of(c)] if u} | {uri_of(c) for c in no_key if uri_of(c)}))


def exists(code):
    u = uri_of(code)
    return bool(u) and u not in mapped_missing


def table_by_uri(table, label):
    out = {}
    for code, seqs in table.items():
        if not exists(code):
            continue
        u = uri_of(code)
        if u in out:
            print(f"ERROR: {label}: {code} and another code map to {u}")
            sys.exit(1)
        out[u] = (code, seqs)
    return out


score_u = table_by_uri(score, "score")
taken = {}  # sequence -> code as written in the score table
for a, seqs in score.items():
    for s in seqs:
        taken.setdefault(s, a)
TAKEN_EXTRA = {"Shift+" + k for k in "ASDTGCVBW"}  # family keys
no_key_u = {uri_of(c): c for c in no_key if exists(c)}
# note-, chord-, fret-, TAB and TEXT commands keep their legacy keys (typing mode, text editing)
exception_u = {u for c, u in cmap.items()
               if c.startswith(("note-", "chord-", "fret-")) or c.endswith("-TAB") or "TEXT" in c}
code_of = {}
for c, u in cmap.items():
    code_of.setdefault(u, c)

dropped, lost, entries, moved = [], [], [], []
for e in resolved:
    cmd, seqs = e["command"], e["sequences"]
    name = score_u[cmd][0] if cmd in score_u else code_of.get(cmd, cmd)
    if cmd in score_u:
        keep = []
        for s in seqs:
            if s in score_u[cmd][1]:
                continue
            if s in taken or s in TAKEN_EXTRA:
                dropped.append((name, s, taken.get(s, "a family key")))
                continue
            keep.append(s)
        new = score_u[cmd][1] + keep
    elif cmd in no_key_u:
        new = []
        lost.append((no_key_u[cmd], seqs))
    else:
        new = []
        scope = e["scope"]
        for s in seqs:
            hit = taken.get(s) or ("a family key" if s in TAKEN_EXTRA else None)
            if hit and cmd not in exception_u:
                dropped.append((name, s, hit))
                continue
            if hit and scope == "NOTATION":
                # A typing-mode command (a letter for a pitch) keeps its key, but only in note input:
                # upstream's NOTATION_NOTE_INPUT scope stands in for the typing-mode scope that the
                # work order's B1 adds (docs/fontdaw/batches/shortcuts/00-v2-plan.md). Outside note
                # input the FontDaw key wins.
                scope = "NOTATION_NOTE_INPUT"
                moved.append((name, s, hit))
            new.append(s)
        e = dict(e, scope=scope)
    entries.append(dict(e, sequences=new))

present = {e["command"] for e in entries}
for table in (score, arrangement, mixer):
    for code, seqs in table.items():
        if not exists(code):
            continue
        u = uri_of(code)
        if u in present:
            continue
        scope = base_scope.get(u) or map_scopes.get(u) or u[len("command://"):].split("/")[0].upper()
        entries.append({"scope": scope, "command": u, "sequences": seqs, "autorepeat": True})
        present.add(u)

m47.write_preset(entries, __import__("pathlib").Path(OUT))

new_ids = sorted({c for t in (score, arrangement, mixer) for c in t if not exists(c)})
print(f"wrote {os.path.relpath(OUT, ROOT)}  ({len(entries)} commands)")
print("NEW", len(new_ids))
print(" ".join(new_ids))
print("DROPPED")
for a, s, by in dropped:
    print(f"{a}\t{s}\t{by}")
print("MOVED to NOTATION_NOTE_INPUT (typing mode until B1's scope)")
for a, s, by in moved:
    print(f"{a}\t{s}\tshares the key with {by}")
print("NOKEY")
for a, s in lost:
    print(a, s)

seen, dups = {}, []
for e in entries:
    for q in e["sequences"]:
        other = seen.setdefault((e["scope"], q), e["command"])
        if other != e["command"]:
            dups.append(f"{q} in scope {e['scope']}: {other} and {e['command']}")
print("DUPLICATES", len(dups))
for d in dups:
    print(d)
sys.exit(1 if dups else 0)
