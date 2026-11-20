#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw - the notation-first DAW. Copyright (C) 2026 FontDaw contributors.
"""Make the "MuseScore 4.7" shortcut layout as a shortcuts v2 preset (FD-153).

The preset is a diff on the v2 base file (upstream's MuseScore 5.0 keys), the form shortcuts v2
reads presets in. It is generated from the frozen 4.7 key files in musescore-4.7-legacy/ through
the map of v1 action codes to v2 commands (src/app/configs/data/fontdaw/v1-commands.json):

- every command of the base is listed, with its 4.7 keys or with none, so a later change to the
  base's keys never reaches this layout (shortcut work order v2, A3a);
- a command that 4.7 binds and the base doesn't is written with the scope given for it in the map
  file's "scopes" section, chosen by hand; the generator stops if one is missing;
- the commands in the map file's "additions" (reviewed) keep the base's keys: upstream's keys that
  4.7 didn't have and that clash with none of 4.7's;
- the commands in the map file's "fontdaw_keys" (reviewed) are FontDaw's own keys on top of 4.7,
  each with a scope and the sequences it gets, on both platforms (for example Tab over the score
  for the Command menu). They are left out of the comparison with the frozen 4.7 files, and the
  report says so. Each entry needs "scope" and "sequences" or the generator stops; the command
  is added with that scope if the base lacks it (a base command must have the same scope);
- a v1 standard key (<std>) takes the base's keys for that command, upstream's own conversion;
- one file per platform: shortcuts_musescore47.json and shortcuts_musescore47_mac.json (macOS has
  its own platform diff and its own 4.7 file). The build packs the one for its platform.

    make_musescore47_preset.py            write both files, then check them
    make_musescore47_preset.py --check    only check the files in the tree

The check replays CommandShortcutsRegister::reload() of muse/framework/shortcuts_v2 (base file,
platform diff, preset diff; mergeShortcuts: a diff entry keeps the base's scope, and a sequence the
diff gives a command is removed from other commands of the same scope) and compares every command's
keys with the frozen 4.7 files (except the "fontdaw_keys" commands, which it checks to get
exactly their own sequences in their own scope). It also stops on two commands of one scope sharing a sequence and on
a command URI that appears nowhere in the sources. Exit 1 on any problem. Interim until the work
order's A2/A3 make the 4.7 layout a complete key set (docs/fontdaw/batches/shortcuts/00-v2-plan.md).
"""

import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LEGACY = ROOT / "tools/fontdaw/shortcuts/musescore-4.7-legacy"
DATA = ROOT / "src/app/configs/data"
MAP = DATA / "fontdaw/v1-commands.json"
OUT = {"other": DATA / "shortcuts_musescore47.json", "mac": DATA / "shortcuts_musescore47_mac.json"}
XML = {"other": LEGACY / "shortcuts.xml", "mac": LEGACY / "shortcuts_mac.xml"}
PLATFORM_DIFF = {"other": None, "mac": DATA / "shortcuts_mac.json"}


def read_xml(path):
    """code -> (sequences, autorepeat, standard key or None), in file order"""
    result = {}
    for sc in ET.parse(path).getroot().iter("SC"):
        seqs = [s.text for s in sc.findall("seq") if s.text]
        result[sc.findtext("key")] = (seqs, sc.findtext("autorepeat") != "0", sc.findtext("std"))
    return result


def read_json(path):
    out = []
    for group in json.loads(path.read_text(encoding="utf-8")):
        for sc in group["shortcuts"]:
            out.append({"scope": group["scope"], "command": sc["command"],
                        "sequences": list(sc.get("sequences", [])),
                        "autorepeat": bool(sc.get("autorepeat", True))})
    return out


def merge(shortcuts, defaults):
    """mergeShortcuts(shortcuts, defaults) of shortcuts_v2, on copies"""
    shortcuts = [dict(s, sequences=list(s["sequences"])) for s in shortcuts]
    needadd = []
    for d in defaults:
        add = dict(d, sequences=list(d["sequences"]))
        found = False
        for s in shortcuts:
            if s["command"] == d["command"]:
                s["scope"] = d["scope"]
                s["autorepeat"] = d["autorepeat"]
                found = True
            elif s["scope"] == d["scope"]:
                add["sequences"] = [q for q in add["sequences"] if q not in s["sequences"]]
        if not found:
            needadd.append(add)
    return shortcuts + needadd


def platform_base(base, platform):
    diff = PLATFORM_DIFF[platform]
    return merge(read_json(diff), base) if diff else base


def wanted_keys(xml, cmap, base, additions):
    """The 4.7 keys per command. A v1 standard key (<std>, expanded by Qt at run time) takes the
    base's sequences, which are upstream's own conversion of it. The reviewed additions keep their
    base keys."""
    base_seqs = {s["command"]: s["sequences"] for s in base}
    wanted, autorepeat, lost = {}, {}, []
    for code, (seqs, ar, std) in xml.items():
        cmd = cmap.get(code)
        if not cmd:
            if seqs or std:
                lost.append(code)
            continue
        if std:
            if not base_seqs.get(cmd):
                lost.append(f"{code} (standard key {std}: the base has no keys for {cmd})")
            seqs = seqs + base_seqs.get(cmd, [])
        acc = wanted.setdefault(cmd, [])
        acc += [s for s in seqs if s not in acc]
        autorepeat[cmd] = autorepeat.get(cmd, True) and ar
    for cmd in additions:
        if cmd in wanted:
            lost.append(f"{cmd} is a 4.7 command, not an addition")
        wanted[cmd] = list(base_seqs.get(cmd, []))
    return wanted, autorepeat, lost


def make_preset(base, wanted, autorepeat, scopes):
    """every base command, then the 4.7 commands the base lacks"""
    known = {s["command"] for s in base}
    entries = [{"scope": s["scope"], "command": s["command"], "sequences": wanted.get(s["command"], []),
                "autorepeat": autorepeat.get(s["command"], s["autorepeat"])} for s in base]
    missing = []
    for cmd, seqs in wanted.items():
        if cmd in known or not seqs:
            continue
        if cmd not in scopes:
            missing.append(cmd)
            continue
        entries.append({"scope": scopes[cmd], "command": cmd, "sequences": seqs,
                        "autorepeat": autorepeat.get(cmd, True)})
    return entries, missing


def read_fontdaw_keys(data):
    """the validated "fontdaw_keys" section: command -> {"scope", "sequences"}; problems as strings"""
    out, problems = {}, []
    for cmd, e in data.items():
        if not isinstance(e, dict) or not e.get("scope") or not e.get("sequences") \
                or not all(isinstance(q, str) and q for q in e["sequences"]):
            problems.append(f"{cmd}: 'fontdaw_keys' entries need a scope and a non-empty list of sequences")
            continue
        out[cmd] = {"scope": e["scope"], "sequences": list(e["sequences"])}
    return out, problems


def add_fontdaw_keys(entries, base, fontdaw):
    """FontDaw's own keys after the 4.7 ones: a base command gets the sequences (its scope must be
    the given one), another command is added with the given scope. Returns problems."""
    problems = []
    for cmd, e in fontdaw.items():
        row = next((x for x in entries if x["command"] == cmd), None)
        if row:
            if row["scope"] != e["scope"]:
                problems.append(f"{cmd}: 'fontdaw_keys' says scope {e['scope']}, the base has {row['scope']}")
                continue
            row["sequences"] = list(e["sequences"])
        else:
            entries.append({"scope": e["scope"], "command": cmd, "sequences": list(e["sequences"]),
                            "autorepeat": True})
    return problems


def write_preset(entries, path):
    scopes = []
    for e in entries:
        if e["scope"] not in scopes:
            scopes.append(e["scope"])
    lines = ["["]
    for i, scope in enumerate(scopes):
        rows = [e for e in entries if e["scope"] == scope]
        lines += ["    {", f'        "scope": "{scope}",', '        "shortcuts": [']
        for j, e in enumerate(rows):
            seqs = ", ".join(json.dumps(q, ensure_ascii=False) for q in e["sequences"])
            ar = "" if e["autorepeat"] else ', "autorepeat": false'
            comma = "," if j < len(rows) - 1 else ""
            lines.append(f'            {{ "command": "{e["command"]}", "sequences": [{seqs}]{ar} }}{comma}')
        lines += ["        ]", "    }" + ("," if i < len(scopes) - 1 else "")]
    lines.append("]")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def check(base, preset, wanted, label, fontdaw=None):
    fontdaw = fontdaw or {}
    resolved = merge(preset, base)
    problems = []
    by_cmd = {s["command"]: s for s in resolved}
    for cmd, e in fontdaw.items():
        got = by_cmd.get(cmd)
        if not got or got["scope"] != e["scope"] or sorted(got["sequences"]) != sorted(e["sequences"]):
            problems.append(f"{label}: {cmd}: FontDaw key wanted {e['sequences']} in {e['scope']}, "
                            f"gets {got and (got['sequences'], got['scope'])}")
    for cmd, seqs in wanted.items():
        got = by_cmd.get(cmd, {}).get("sequences", [])
        if cmd in fontdaw:
            continue
        if sorted(got) != sorted(seqs):
            problems.append(f"{label}: {cmd}: wanted {seqs}, gets {got}")
    for cmd, s in by_cmd.items():
        if cmd not in wanted and cmd not in fontdaw and s["sequences"]:
            problems.append(f"{label}: {cmd}: not a 4.7 key, gets {s['sequences']}")
    seen = {}
    for s in resolved:
        for q in s["sequences"]:
            other = seen.setdefault((s["scope"], q), s["command"])
            if other != s["command"]:
                problems.append(f"{label}: {q} in scope {s['scope']} on both {other} and {s['command']}")
    return problems


def shortcut_availability():
    """command URI -> True/False: whether the command may have a shortcut (Availability::Shortcut,
    or UI / All, which include it). Read from the sources in the two forms they use:
    command structs (`cprop_decl id = Command("uri")` ... `cprop_decl availabilities = …;`) and
    CommandInfo(...) / CommandInfo{...} blocks in the *commandsregister.cpp files (a constant, then the availability as
    a later argument; constants resolved from `Command NAME("uri")`). A command with no availability
    written is Availability::All, so it may have a shortcut (rcommand/commandtypes.h)."""
    files = subprocess.run(["git", "-C", str(ROOT), "ls-files", "src/*.h", "src/*.cpp"],
                           capture_output=True, text=True).stdout.split()
    files = [ROOT / f for f in files]
    files += [p for p in (ROOT / "muse/framework").rglob("*") if p.suffix in (".h", ".cpp")]

    def allows(expr):
        return any(t in expr for t in ("Availability::Shortcut", "Availability::UI", "Availability::All"))

    result, consts = {}, {}
    for f in files:
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'(\w+)\s*\(\s*"(command://[^"]+)"\s*\)', text):
            consts[m.group(1)] = m.group(2)
        for m in re.finditer(r'cprop_decl\s+id\s*=\s*Command\("(command://[^"]+)"\);(.*?)\n\};', text, re.S):
            av = re.search(r'cprop_decl\s+availabilities\s*=\s*([^;]+);', m.group(2))
            result[m.group(1)] = allows(av.group(1)) if av else True
    for f in files:
        if not f.name.endswith("commandsregister.cpp"):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'CommandInfo[({]\s*(\w+)\s*,(.*?)\n\s*[)}]', text, re.S):
            uri = consts.get(m.group(1))
            if not uri:
                continue
            av = re.findall(r'Availability::\w+(?:\s*\|\s*Availability::\w+)*', m.group(2))
            result.setdefault(uri, allows(av[-1]) if av else True)
    return result


def unknown_uris(uris):
    """URIs that appear in no source file: the commands are registered from string literals"""
    out = []
    for uri in sorted(uris):
        path = uri.split("?", 1)[0]
        found = subprocess.run(["git", "-C", str(ROOT), "grep", "-q", "-F", path, "--", "src", "muse"],
                               capture_output=True).returncode == 0
        if not found:
            found = subprocess.run(["grep", "-rqF", path, str(ROOT / "muse/framework")]).returncode == 0
        if not found:
            out.append(uri)
    return out


def main():
    only_check = "--check" in sys.argv[1:]
    mapdata = json.loads(MAP.read_text(encoding="utf-8"))
    cmap, scopes = mapdata["map"], mapdata.get("scopes", {})
    additions = mapdata.get("additions", {})
    base = read_json(DATA / "shortcuts.json")
    fontdaw, problems = read_fontdaw_keys(mapdata.get("fontdaw_keys", {}))
    lost = set()
    for platform, label in (("other", "Linux/Windows"), ("mac", "macOS")):
        pbase = platform_base(base, platform)
        wanted, autorepeat, lost_here = wanted_keys(read_xml(XML[platform]), cmap, pbase, additions)
        lost |= set(lost_here)
        if not only_check:
            entries, missing = make_preset(pbase, wanted, autorepeat, scopes)
            for cmd in missing:
                problems.append(f"{label}: {cmd} is not in the base; give it a scope in the map's 'scopes'")
            problems += [f"{label}: {p}" for p in add_fontdaw_keys(entries, pbase, fontdaw)]
            write_preset(entries, OUT[platform])
            print(f"wrote {OUT[platform].relative_to(ROOT)}")
        problems += check(pbase, read_json(OUT[platform]), wanted, label, fontdaw)
    for cmd in sorted(fontdaw):
        print(f"note: {cmd} has FontDaw's own keys {fontdaw[cmd]['sequences']} in {fontdaw[cmd]['scope']} "
              f"('fontdaw_keys'): not compared with the frozen 4.7 files")

    availability = shortcut_availability()
    for uri in sorted(set(cmap.values()) | set(additions) | set(scopes) | set(fontdaw)):
        if availability.get(uri) is False:
            problems.append(f"{uri}: not shortcut-available (Availability without Shortcut); move its code to 'unmapped'")
    for uri in sorted(scopes):
        # a scope chosen by hand is for a command the base doesn't list: it must be a command
        # whose definition the scan finds, or the check above would pass it unseen
        if uri not in availability:
            problems.append(f"{uri}: in the map's 'scopes' but no command definition found in the sources")

    for uri in unknown_uris(set(cmap.values()) | set(fontdaw)):
        problems.append(f"{uri}: not found in the sources (renamed or removed?)")

    for code in sorted(lost):
        if " " in code:  # a standard key without base keys, or an addition that is a 4.7 command
            problems.append(code)
        else:
            print(f"note: no v2 command for the 4.7 key of '{code}' (see v1-commands.json 'unmapped')")
    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
