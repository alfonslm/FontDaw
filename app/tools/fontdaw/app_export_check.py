#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Helper for tools/fontdaw/app_export_check.sh (FD-038). Design, hits, tolerances and the
# expected-failure list: tools/fontdaw/testdata/export_check/README.md.
#
# Subcommands:
#   make-project --score PATH --out DIR [--break-offset MS]
#                                write DIR/full.mscz, score_only.mscz and clicks_only.mscz
#   check --dir DIR --score PATH [--allow-missing-tools] [--break-offset MS]
#                                read the exports in DIR, print the report, exit 0 or 1
#
# Python 3 standard library; ffmpeg and ffprobe for every audio format other than WAV.

import argparse
import array
import hashlib
import io
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import wave
import xml.etree.ElementTree as ET
import zipfile
from fractions import Fraction

# The test project's hits (README.md, "Bars and hits"): bar (0 = pickup), beat (quarters from 1), kind.
# both = score note and clip, note = score note only, clip = clip only.
HITS = [
    (0, 1, "both"),
    (1, 1, "both"), (1, 3, "note"),
    (2, 1, "clip"), (2, 3, "both"),
    (3, 1, "both"), (3, 3, "note"),
    (4, 1, "both"), (4, 3, "note"), (4, 4, "both"),
    (5, 1, "note"), (5, 3, "note"), (5, 4, "both"),
    (6, 1, "note"), (6, 2, "note"), (6, 3, "both"),
    (7, 1, "both"),
]

# Known faults: row name -> issue (README.md, "Known faults").
KNOWN = {
    "mp3.encoder-delay": "FD-110",
    "aac.encoder-delay": "FD-110",
    "brf.export": "FD-111",
    "cli.parts-hang": "FD-112",
    "audio.mp4": "FD-142",
    "mp4.spacing": "FD-142",
    "mei.rit": "FD-113",
    "mnx.rit": "FD-113",
}

# The known faults' signatures as measured on main (README.md, "Known faults"). An expected failure
# must look like this; a different failure in the same row is a FAIL, not hidden behind the known one.
# audio.mp4: the video's 3.0 s lead-in (VideoExportConfiguration::leadingSec) plus encoder delay (FD-142)
KNOWN_DELAY_MS = {"mp3.encoder-delay": 51.2, "aac.encoder-delay": 46.4, "audio.mp4": 3041.8}
# every hit; H01 sits on the file's first sample, where MP3 smears it by ~1.8 ms; in the MP4 it is
# a further 4.6 ms late (mp4.spacing)
KNOWN_DELAY_TOL_MS = {"mp3.encoder-delay": 2.0, "aac.encoder-delay": 2.0, "audio.mp4": 6.0}
# mp4.spacing (FD-142): only the first interval is off, by this much
KNOWN_MP4_FIRST_INTERVAL_MS = -4.65
KNOWN_MP4_FIRST_INTERVAL_TOL_MS = 1.0
KNOWN_STATUS = {"cli.parts-hang": "124", "brf.export": "30"}
# exports whose non-zero exit status is a known fault, reported by its own row
KNOWN_FAILING_EXPORTS = {"full.brf"}

SPACING_TOL_MS = 2.0   # each interval between hits; see README "Tolerances"

SR = 48000  # sample rate of the generated kick
DUR = {"eighth": Fraction(1, 8), "quarter": Fraction(1, 4), "half": Fraction(1, 2)}


# ---------------------------------------------------------------- the score and make-project

def hit_name(i):
    return "H%02d" % (i + 1)


def bb_str(bar, beat):
    beat = Fraction(beat)
    b = str(beat.numerator) if beat.denominator == 1 else "%.2f" % float(beat)
    return "%d.%s" % (bar, b)


def load_expected(path):
    """Expected ms per hit from expected_times.json, checked against HITS."""
    data = json.load(open(path))["hits"]
    if [(h["bar"], h["beat"]) for h in data] != [(b, bt) for b, bt, _ in HITS]:
        raise ValueError("its hits do not match HITS")
    return [float(h["ms"]) for h in data]


def walk_score(path):
    """ChordRests of the first top-level staff of the main score, in order:
    a list of (bar, position in whole notes, is_chord)."""
    root = ET.parse(path).getroot()
    score = root.find("Score")
    staff = None
    for st in score.findall("Staff"):
        if st.get("id") == "1":
            staff = st
            break
    if staff is None:
        raise SystemExit("error: score has no top-level <Staff id=\"1\">")
    events = []
    for bar, meas in enumerate(staff.findall("Measure")):
        pos = Fraction(0)
        for el in meas.find("voice"):
            if el.tag not in ("Chord", "Rest"):
                continue
            dt = el.findtext("durationType")
            if dt == "measure":
                d = Fraction(4, 4)
            elif dt in DUR:
                d = DUR[dt]
            else:
                raise SystemExit("error: unsupported durationType %r in the score" % dt)
            events.append((bar, pos, el.tag == "Chord"))
            pos += d
    return events


def event_index(events, bar, beat):
    for i, (b, pos, _) in enumerate(events):
        if b == bar and pos * 4 + 1 == beat:
            return i
    return None


def verify_score(score_path):
    events = walk_score(score_path)
    got = [(b, int(pos * 4 + 1)) for b, pos, chord in events if chord]
    want = [(b, bt) for b, bt, k in HITS if k in ("note", "both")]
    if got != want:
        raise SystemExit("error: the score's chords do not match the HITS table.\n  score: %s\n  table: %s"
                         % (got, want))
    for b, bt, k in HITS:
        if k == "clip" and event_index(events, b, bt) is None:
            raise SystemExit("error: clip-only hit %d.%d has no rest at that position" % (b, bt))
    return events


def kick_wav(offset_ms):
    """Generated kick: cosine swept 150 to 50 Hz, 60 ms decay, 0.25 s, 48 kHz mono 16 bit."""
    n = int(0.25 * SR)
    pcm = [0] * int(round(offset_ms * SR / 1000.0))
    phase = 0.0
    for i in range(n):
        t = i / SR
        f = 50 + 100 * math.exp(-t / 0.03)
        pcm.append(int(0.8 * math.cos(phase) * math.exp(-t / 0.06) * 32767))
        phase += 2 * math.pi * f / SR
    buf = io.BytesIO()
    w = wave.open(buf, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(b"".join(struct.pack("<h", v) for v in pcm))
    w.close()
    return buf.getvalue(), len(pcm)


def clip_tick(bar, beat):
    """Start of a hit as a whole-note fraction "q/4" (q = quarters from the start)."""
    q = (0 if bar == 0 else 1 + 4 * (bar - 1)) + (beat - 1)
    return "%d/4" % q


def write_mscz(out, score_text, wav=None):
    name = "score.mscx"
    z = zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED)
    z.writestr("META-INF/container.xml",
               '<?xml version="1.0" encoding="UTF-8"?>\n<container>\n  <rootfiles>\n'
               '    <rootfile full-path="%s">\n      </rootfile>\n    </rootfiles>\n  </container>\n' % name)
    z.writestr(name, score_text)
    if wav is not None:
        data, frames = wav
        aid = hashlib.sha1(data).hexdigest()
        clips = []
        n = 0
        for bar, beat, kind in HITS:
            if kind in ("both", "clip"):
                clips.append({"id": 10 + n, "asset": aid, "name": "Kick", "startTick": clip_tick(bar, beat),
                              "sourceOffset": 0, "length": frames / SR, "gainDb": 0})
                n += 1
        arr = {"version": 1,
               "assets": [{"id": aid, "name": "Kick", "format": "wav", "storage": "embedded",
                           "file": "audio/%s.wav" % aid, "sampleRate": SR, "channels": 1,
                           "frames": str(frames)}],
               "tracks": [{"id": 1, "kind": "audio", "name": "Kick", "timeBase": "musical", "clips": clips}]}
        z.writestr("arrangement.json", json.dumps(arr, indent=1))
        z.writestr("audio/%s.wav" % aid, data, compress_type=zipfile.ZIP_STORED)
    z.close()


def make_project(score_path, out_dir, break_offset):
    verify_score(score_path)
    text = open(score_path, encoding="utf-8").read()
    # every chord becomes a rest of the same length (also in the stored part)
    rests = re.sub(r"<Chord>.*?<durationType>(\w+)</durationType>.*?</Chord>",
                   r"<Rest><durationType>\1</durationType></Rest>", text, flags=re.S)
    os.makedirs(out_dir, exist_ok=True)
    write_mscz(os.path.join(out_dir, "full.mscz"), text, kick_wav(break_offset))
    write_mscz(os.path.join(out_dir, "score_only.mscz"), text)
    write_mscz(os.path.join(out_dir, "clicks_only.mscz"), rests, kick_wav(break_offset))
    print("make-project: %d hits, projects written to %s" % (len(HITS), out_dir))


# ---------------------------------------------------------------- audio

def have_tool(name):
    return shutil.which(name) is not None


def decode(path):
    """(sample rate, per-sample peak over the channels as floats)."""
    if path.endswith(".wav"):
        try:
            w = wave.open(path)
            ch, sr, sw = w.getnchannels(), w.getframerate(), w.getsampwidth()
            raw = w.readframes(w.getnframes())
            w.close()
            if sw == 3:
                raw = b"".join(b"\0" + raw[i:i + 3] for i in range(0, len(raw), 3))
                sw = 4
            if sw in (2, 4):
                a = array.array("h" if sw == 2 else "i", raw)
                scale = float(2 ** (8 * sw - 1))
                return sr, peaks(a, ch, scale)
        except (wave.Error, EOFError):
            pass
    out = subprocess.check_output(
        ["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries", "stream=sample_rate,channels",
         "-of", "csv=p=0", path]).decode().strip().split(",")
    sr, ch = int(out[0]), int(out[1])
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", path, "-map", "0:a:0", "-vn", "-f", "f32le", "-"])
    a = array.array("f")
    a.frombytes(raw)
    return sr, peaks(a, ch, 1.0)


def peaks(a, ch, scale):
    if ch == 1:
        return [abs(v) / scale for v in a]
    cols = [a[c::ch] for c in range(ch)]
    return [max(abs(v) for v in fr) / scale for fr in zip(*cols)]


def onsets(sr, m):
    """Onset sample indices, as in README.md "Onset detection"."""
    pk = max(m) if m else 0.0
    if pk <= 1e-6:
        raise ValueError("silent file (peak %.1e)" % pk)
    floor = pk * 10 ** (-40 / 20)
    blk = max(1, sr // 1000)
    env = [max(m[i:i + blk]) for i in range(0, len(m), blk)]
    out = []
    last = -10 ** 9
    for k in range(len(env)):
        prev = max(env[max(0, k - 30):k], default=0.0)
        nxt = max(env[k:k + 3])
        if nxt >= floor and nxt >= 4.0 * prev and k - last >= 100:
            own = max(env[k:k + 30])
            thr = max(prev * 1.5, 0.1 * own)
            i = max(0, (k - 1) * blk)
            while m[i] < thr:
                i += 1
            out.append(i)
            last = k
    return out


class Audio:
    """One decoded file: its rate and onset sample indices."""
    def __init__(self, path):
        self.sr, m = decode(path)
        self.on = onsets(self.sr, m)

    def ms(self, i):
        return self.on[i] / self.sr * 1000.0

    def all_ms(self):
        return [x / self.sr * 1000.0 for x in self.on]


def assign(onsets_ms, hit_ms, expect, window=30.0):
    """Give each onset to the nearest hit time within the window. Returns (per hit: onset ms or None,
    problems: list of strings). expect[i] says whether hit i should have an onset."""
    per = [[] for _ in hit_ms]
    problems = []
    for o in onsets_ms:
        j = min(range(len(hit_ms)), key=lambda k: abs(hit_ms[k] - o))
        if abs(hit_ms[j] - o) <= window:
            per[j].append(o)
        else:
            problems.append("extra onset at %.1f ms (no hit near)" % o)
    res = []
    for i, lst in enumerate(per):
        if expect[i] and not lst:
            problems.append("%s: onset missing" % hit_name(i))
        if not expect[i] and lst:
            problems.append("%s: extra onset" % hit_name(i))
        if len(lst) > 1:
            problems.append("%s: %d onsets" % (hit_name(i), len(lst)))
        res.append(lst[0] if lst else None)
    return res, problems


def median(v):
    v = sorted(v)
    if not v:
        return None
    n = len(v)
    return v[n // 2] if n % 2 else (v[n // 2 - 1] + v[n // 2]) / 2.0


def pair_in_order(aud, ref_ms):
    """Lossy files: pair the onsets with the full WAV's, in order. None if the counts differ."""
    if len(aud.on) != len(ref_ms):
        return None
    return aud.all_ms()


# ---------------------------------------------------------------- MIDI

def vlq(b, i):
    v = 0
    while True:
        c = b[i]
        i += 1
        v = (v << 7) | (c & 0x7f)
        if not c & 0x80:
            return v, i


def parse_midi(path):
    """(ticks per quarter, {tick: microseconds per quarter}, sorted note-on ticks, time signatures)."""
    b = open(path, "rb").read()
    if b[:4] != b"MThd":
        raise ValueError("not a MIDI file")
    hlen = struct.unpack(">I", b[4:8])[0]
    _, ntr, div = struct.unpack(">HHH", b[8:14])
    if div & 0x8000:
        raise ValueError("SMPTE time division is not supported")
    i = 8 + hlen
    tempos, notes, ts = {}, [], []
    for _ in range(ntr):
        n = struct.unpack(">I", b[i + 4:i + 8])[0]
        j, end, t, rs = i + 8, i + 8 + n, 0, None
        while j < end:
            d, j = vlq(b, j)
            t += d
            c = b[j]
            if c == 0xFF:
                ty = b[j + 1]
                ln, k = vlq(b, j + 2)
                data = b[k:k + ln]
                j = k + ln
                if ty == 0x51:
                    tempos[t] = int.from_bytes(data, "big")
                if ty == 0x58:
                    ts.append((t, data[0], 2 ** data[1]))
            elif c in (0xF0, 0xF7):
                ln, k = vlq(b, j + 1)
                j = k + ln
            else:
                if c & 0x80:
                    rs = c
                    j += 1
                st = rs & 0xF0
                nd = 1 if st in (0xC0, 0xD0) else 2
                dd = b[j:j + nd]
                j += nd
                if st == 0x90 and dd[1] > 0:
                    notes.append(t)
        i = end
    # every note-on is kept: an export that writes a note twice must show up as an extra note
    return div, tempos, sorted(notes), sorted(ts)


def midi_secs(tick, div, tempos):
    s, last, us = 0.0, 0, 500000
    for t in sorted(tempos):
        if t >= tick:
            break
        s += (t - last) / div * us / 1e6
        last, us = t, tempos[t]
    return s + (tick - last) / div * us / 1e6


def written_tick(bar, beat, div):
    """Tick of a hit as written: the pickup is one beat, every later bar four."""
    return div * ((0 if bar == 0 else 1 + 4 * (bar - 1)) + beat - 1)


def caesura_style(notes_by_hit, div, tempos):
    """How the MIDI file carries the caesura pause, from the hits after it (bar 6 beat 3 on)."""
    i3 = HITS.index((6, 3, "both"))
    i2 = HITS.index((6, 2, "note"))
    delta = notes_by_hit[i3] - written_tick(6, 3, div)
    if delta == 0:
        slow = [t for t, us in tempos.items() if notes_by_hit[i2] <= t < notes_by_hit[i3] and us >= 3000000]
        return "tempo only" if slow else "unknown"
    if delta % (4 * div) == 0:
        return "inserted bar"
    if delta % div == 0:
        return "lengthened bar"
    return "unknown"


def midi_bar_beat(tick, div, ts):
    """(bar from 0, beat in quarters from 1) for a tick; bars follow the time-signature events."""
    sig, i, pos, bar = (4, 4), 0, 0, 0
    while True:
        while i < len(ts) and ts[i][0] <= pos:
            sig = (ts[i][1], ts[i][2])
            i += 1
        length = div * 4 * sig[0] // sig[1]
        if tick < pos + length:
            return bar, Fraction(tick - pos, div) + 1
        pos += length
        bar += 1


# ---------------------------------------------------------------- notation formats

def ln(tag):
    return tag.rsplit("}", 1)[-1]


def kick_hits(entries):
    """entries: (bar, beat, is_kick_note) in order -> list of (bar, beat) of the kick notes."""
    return [(b, bt) for b, bt, k in entries if k]


def parse_musicxml(data):
    root = ET.fromstring(data)
    part = root.find("part")
    midi_of = {}
    for mi in root.iter("midi-instrument"):
        u = mi.findtext("midi-unpitched")
        if u:
            midi_of[mi.get("id")] = int(u) - 1
    notes, tempos, words, fermata, caesura, dashes = [], [], [], 0, 0, 0
    div = 1
    for bar, meas in enumerate(part.findall("measure")):
        pos = Fraction(0)
        for el in meas:
            tag = el.tag
            if tag == "attributes":
                d = el.findtext("divisions")
                if d:
                    div = int(d)
            elif tag == "note":
                dur = Fraction(int(el.findtext("duration") or 0), div)
                m = None
                if el.find("pitch") is not None:
                    p = el.find("pitch")
                    m = 12 * (int(p.findtext("octave")) + 1) + {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7,
                                                                  "A": 9, "B": 11}[p.findtext("step")]
                elif el.find("unpitched") is not None:
                    inst = el.find("instrument")
                    m = midi_of.get(inst.get("id") if inst is not None else None, 36)
                if m is not None:
                    notes.append((bar, pos + 1, m == 36))
                if el.find("chord") is None:
                    pos += dur
            elif tag == "backup":
                pos -= Fraction(int(el.findtext("duration")), div)
            elif tag == "forward":
                pos += Fraction(int(el.findtext("duration")), div)
        for e in meas.iter():
            if e.tag == "sound" and e.get("tempo"):
                tempos.append(float(e.get("tempo")))
            elif e.tag == "words" and e.text:
                words.append(e.text.strip())
            elif e.tag == "fermata":
                fermata += 1
            elif e.tag == "caesura":
                caesura += 1
            elif e.tag in ("dashes", "wedge"):
                dashes += 1
    rit = any(w.lower().startswith("rit") for w in words)
    return {"hits": kick_hits(notes), "tempo": tempos, "rit": rit,
            "rit_what": "words rit." if rit else "<words> starting rit",
            "fermata": fermata > 0, "caesura": caesura > 0, "caesura_what": "<caesura>"}


def parse_mei(data):
    root = ET.fromstring(data)
    notes, tempos, rit, fermata, caesura = [], [], False, False, False
    pn = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}
    meas = [e for e in root.iter() if ln(e.tag) == "measure"]
    for bar, m in enumerate(meas):
        staff = next(e for e in m if ln(e.tag) == "staff")
        layer = next(e for e in staff if ln(e.tag) == "layer")
        pos = Fraction(0)
        for e in layer.iter():
            t = ln(e.tag)
            if t in ("note", "rest") and e.get("dur"):
                d = Fraction(1, int(e.get("dur")))
                dots = int(e.get("dots") or 0)
                d = d * (2 - Fraction(1, 2 ** dots))
                if t == "note":
                    mid = 12 * (int(e.get("oct")) + 1) + pn[e.get("pname")]
                    notes.append((bar, pos * 4 + 1, mid == 36))
                pos += d
        for e in m.iter():
            t = ln(e.tag)
            txt = "".join(e.itertext()).lower()
            if t == "tempo" and e.get("midi.bpm"):
                tempos.append(float(e.get("midi.bpm")))
            if t in ("tempo", "dir") and ("rit" in txt or "rit" in (e.get("type") or "")):
                rit = True
            if t == "fermata" or e.get("fermata"):
                fermata = True
            if t in ("caesura", "breath"):
                caesura = True
    return {"hits": kick_hits(notes), "tempo": tempos, "rit": rit,
            "rit_what": "<tempo>/<dir> with rit text", "fermata": fermata, "caesura": caesura,
            "caesura_what": "<caesura>"}


def parse_mnx(data):
    d = json.loads(data)
    sounds = d["global"].get("sounds", {})
    part = d["parts"][0]
    base = {"whole": Fraction(1), "half": Fraction(1, 2), "quarter": Fraction(1, 4), "eighth": Fraction(1, 8),
            "16th": Fraction(1, 16)}
    notes = []
    for bar, m in enumerate(part["measures"]):
        pos = Fraction(0)
        seqs = m.get("sequences") or [{}]
        for it in seqs[0].get("content", []):
            dur = it.get("duration")
            if not dur:
                continue
            kick = False
            for kn in it.get("kitNotes", []):
                snd = part.get("kit", {}).get(kn.get("kitComponent"), {}).get("sound")
                if sounds.get(snd, {}).get("midiNumber") == 36:
                    kick = True
            if "kitNotes" in it or "notes" in it:
                notes.append((bar, pos * 4 + 1, kick))
            dots = dur.get("dots", 0)
            pos += base[dur["base"]] * (2 - Fraction(1, 2 ** dots))
    tempos = []
    for m in d["global"]["measures"]:
        for t in m.get("tempos", []):
            tempos.append(float(t["bpm"]))
    text = json.dumps(d).lower()
    rit = bool(re.search(r'"(rit|ritard|gradual|accel)[a-z]*', text))
    return {"hits": kick_hits(notes), "tempo": tempos, "rit": rit,
            "rit_what": "a rit./gradual tempo key or text", "fermata": '"fermata"' in text,
            "caesura": '"caesura"' in text or '"breath"' in text,
            "caesura_what": "caesura or breath marking" + (" (written as breath)" if '"caesura"' not in text and '"breath"' in text else "")}


def read_mxl(path):
    z = zipfile.ZipFile(path)
    cont = ET.fromstring(z.read("META-INF/container.xml"))
    root_file = next(e for e in cont.iter() if ln(e.tag) == "rootfile").get("full-path")
    return z.read(root_file)


# ---------------------------------------------------------------- the report

class Report:
    def __init__(self):
        self.rows = []

    def add(self, name, status, detail=""):
        self.rows.append((name, status, detail))

    def passfail(self, name, problems, ok_detail=""):
        if problems:
            self.add(name, "FAIL", "; ".join(problems[:6]) + (" (+%d more)" % (len(problems) - 6) if len(problems) > 6 else ""))
        else:
            self.add(name, "PASS", ok_detail)

    def known(self, name, faulty, clean, detail):
        """A known fault: faulty -> EXPECTED-FAIL, clean -> UNEXPECTED-PASS, else FAIL."""
        if faulty:
            self.add(name, "EXPECTED-FAIL", "%s: %s" % (KNOWN[name], detail))
        elif clean:
            self.add(name, "UNEXPECTED-PASS", "%s is fixed? %s" % (KNOWN[name], detail))
        else:
            self.add(name, "FAIL", "unexpected: " + detail)


def fmt(v, w=8):
    return ("%+.2f" % v).rjust(w) if isinstance(v, float) else str(v).rjust(w)


def run_check(a):
    d = a.dir
    rep = Report()
    events = verify_score(a.score)
    n_hits = len(HITS)
    kinds = [k for _, _, k in HITS]
    status = {}
    sp = os.path.join(d, "status.txt")
    if os.path.exists(sp):
        for line in open(sp):
            p = line.split()
            if len(p) == 2:
                status[p[0]] = p[1]

    def path(*p):
        return os.path.join(d, *p)

    def status_of(*p):
        key = "/".join(p)
        if key.startswith("parts/"):
            key = "parts/full-*.mp3"
        return status.get(key)

    def nonempty(*p):
        """A usable export: the file exists, is not empty, and its export ended with status 0."""
        st = status_of(*p)
        return os.path.isfile(path(*p)) and os.path.getsize(path(*p)) > 0 and st in (None, "0")

    def why(name, *p):
        return "missing, empty or its export failed (exit status %s)" % (status_of(*p) or status.get(name, "?"))

    # every export that ended non-zero fails, unless a known-fault row reports it
    for name, st in sorted(status.items()):
        if st != "0" and name not in KNOWN_FAILING_EXPORTS:
            rep.add("export." + name, "FAIL", "exit status %s (log: logs/%s.log)" % (st, name.replace("/", "_")))

    if a.break_offset:
        print("self-test: kick shifted by %g ms; the clip checks must fail" % a.break_offset)

    # expected time per hit: stored in the test data, independent of the app (README "Expected times")
    exp_ms = None
    try:
        exp_ms = load_expected(a.expected)
    except (OSError, ValueError, KeyError) as e:
        rep.add("expected.times", "FAIL", "cannot read %s: %s" % (a.expected, e))
    # the app's own segment times (spos) must agree with them
    if exp_ms is not None and nonempty("full.spos"):
        evs = [float(e.get("position")) for e in ET.parse(path("full.spos")).getroot().iter("event")]
        if len(evs) != len(events):
            rep.add("spos.times", "FAIL", "%d events in full.spos, %d ChordRests in the score" % (len(evs), len(events)))
        else:
            sp = [evs[event_index(events, b, bt)] for b, bt, _ in HITS]
            rep.passfail("spos.times", ["%s: spos %.0f ms, expected %.2f" % (hit_name(i), sp[i], exp_ms[i])
                                        for i in range(n_hits) if abs(sp[i] - exp_ms[i]) > 1.0],
                         "%d events; every hit within 1 ms of the expected time" % len(evs))
    elif exp_ms is not None:
        rep.add("spos.times", "FAIL", "full.spos " + why("full.spos", "full.spos"))
    if exp_ms is None:
        print_report(rep, None, {}, {}, {}, {}, None, {}, [])
        return rep

    # --- audio
    tools_ok = have_tool("ffmpeg") and have_tool("ffprobe")
    aud = {}      # name -> Audio
    cells = {}    # name -> per-hit cell strings
    med = {}      # name -> median offset from spos, ms
    ons_ms = {}   # name -> per-hit onset ms (None if no onset)

    def load(name, rel, needs_tools):
        p = path(*rel)
        if not nonempty(*rel):
            return None
        if needs_tools and not tools_ok:
            return None
        try:
            aud[name] = Audio(p)
        except Exception as e:  # decoding problems are a failure of the row
            rep.add("audio." + name, "FAIL", "cannot decode: %s" % e)
            return None
        return aud[name]

    def not_checked(row, fmtname):
        tool = "ffmpeg" if not have_tool("ffmpeg") else "ffprobe"
        rep.add(row, "NOT-CHECKED", "not checked: %s, %s missing" % (fmtname, tool))

    def wav_cells(name, expect, tol=None):
        """Assign onsets to hits by time; fills cells, ons_ms, med; returns problems."""
        a_ = aud[name]
        res, probs = assign(a_.all_ms(), exp_ms, expect)
        ons_ms[name] = res
        cells[name] = []
        for i, o in enumerate(res):
            if o is None:
                cells[name].append("MISSING" if expect[i] else "—")
            else:
                cells[name].append(fmt(o - exp_ms[i]) if expect[i] else "EXTRA " + fmt(o - exp_ms[i]))
        med[name] = median([o - exp_ms[i] for i, o in enumerate(res) if o is not None])
        return res, probs

    exp_clip = [k in ("both", "clip") for k in kinds]
    exp_note = [k in ("both", "note") for k in kinds]
    exp_all = [True] * n_hits

    # clips only
    if load("clips_only.wav", ("clicks_only.wav",), False):
        res, probs = wav_cells("clips_only.wav", exp_clip)
        for i, o in enumerate(res):
            if o is not None and exp_clip[i] and abs(o - exp_ms[i]) > 0.6:
                probs.append("%s: clip onset %+.2f ms from the expected time (limit 0.6)" % (hit_name(i), o - exp_ms[i]))
        rep.passfail("audio.clips-only", probs, "onsets at every clip, none at note-only beats")
    else:
        rep.add("audio.clips-only", "FAIL", "clicks_only.wav " + why("clicks_only.wav", "clicks_only.wav"))

    # score only
    L = None
    if load("score_only.wav", ("score_only.wav",), False):
        res, probs = wav_cells("score_only.wav", exp_note)
        lags = [o - exp_ms[i] for i, o in enumerate(res) if o is not None and exp_note[i]]
        L = median(lags)
        if L is not None:
            if not 0 <= L <= 5:
                probs.append("lag L = %.2f ms is outside 0..5" % L)
            for i, o in enumerate(res):
                if o is not None and exp_note[i] and abs(o - exp_ms[i] - L) > 1.5:
                    probs.append("%s: %+.2f ms, L %.2f +- 1.5" % (hit_name(i), o - exp_ms[i], L))
        rep.passfail("audio.score-only", probs, "L = %.2f ms" % L if L is not None else "")
    else:
        rep.add("audio.score-only", "FAIL", "score_only.wav " + why("score_only.wav", "score_only.wav"))

    # a note and a clip on one beat must coincide: score-only onset - clip onset = L +- 1.5
    if "clips_only.wav" in ons_ms and "score_only.wav" in ons_ms and L is not None:
        probs = []
        for i, k in enumerate(kinds):
            if k == "both":
                c, s = ons_ms["clips_only.wav"][i], ons_ms["score_only.wav"][i]
                if c is None or s is None:
                    continue
                if abs(s - c - L) > 1.5:
                    probs.append("%s: note %+.2f ms after clip, L %.2f +- 1.5" % (hit_name(i), s - c, L))
        rep.passfail("audio.both-coincide", probs, "note and clip agree within L +- 1.5 ms")
    else:
        rep.add("audio.both-coincide", "FAIL", "needs clicks_only.wav and score_only.wav")

    # full WAV
    full = None
    if load("full.wav", ("full.wav",), False):
        full = aud["full.wav"]
        res, probs = wav_cells("full.wav", exp_all)
        for i, k in enumerate(kinds):
            ref = "clips_only.wav" if k in ("both", "clip") else "score_only.wav"
            if ref in aud and aud[ref].sr == full.sr and ref in ons_ms and ons_ms[ref][i] is not None and res[i] is not None:
                diff = (res[i] - ons_ms[ref][i]) / 1000.0 * full.sr
                if abs(diff) > 1.0001:
                    probs.append("%s: %+.1f samples from %s" % (hit_name(i), diff, ref))
        rep.passfail("audio.full", probs, "17 onsets; clip/both = clips-only, note = score-only (+-1 sample)")
    else:
        rep.add("audio.full", "FAIL", "full.wav " + why("full.wav", "full.wav"))

    # lossless and lossy versions of the full mix
    def lossy(name, rel, row, tol, fmtname, expected=None):
        if not nonempty(*rel):
            rep.add(row, "FAIL", "%s %s" % (rel[-1], why(rel[-1], *rel)))
            return
        if not tools_ok:
            not_checked(row, fmtname)
            return
        if full is None:
            rep.add(row, "FAIL", "no full.wav to compare with")
            return
        if load(name, rel, True) is None:
            return
        x = aud[name]
        ref = ons_ms["full.wav"]
        if None in ref or len(x.on) != n_hits:
            cells[name] = ["?"] * n_hits
            rep.add(row, "FAIL",
                    "%d onsets found, expected %d" % (len(x.on), n_hits))
            return
        xm = x.all_ms()
        ons_ms[name] = xm
        cells[name] = [fmt(xm[i] - exp_ms[i]) for i in range(n_hits)]
        med[name] = median([xm[i] - exp_ms[i] for i in range(n_hits)])
        diffs = [xm[i] - ref[i] for i in range(n_hits)]
        off = median(diffs)
        bad = [i for i in range(n_hits) if abs(diffs[i]) > tol]
        detail = "median offset from full.wav %+.2f ms" % off
        if expected is None:
            rep.passfail(row, ["%s: %+.2f ms from full.wav (limit %g)" % (hit_name(i), diffs[i], tol) for i in bad],
                         detail)
        else:
            known = KNOWN_DELAY_MS[row]
            tol_known = KNOWN_DELAY_TOL_MS[row]
            signature = all(abs(d - known) <= tol_known for d in diffs)
            if not signature and bad:
                detail += "; not the known delay of %.1f ms +- %g on every hit" % (known, tol_known)
            rep.known(row, signature, not bad, detail)

    if nonempty("full.flac") and tools_ok and load("full.flac", ("full.flac",), True):
        x = aud["full.flac"]
        ok = full is not None and x.sr == full.sr and x.on == full.on
        cells["full.flac"] = [fmt(x.ms(i) - exp_ms[i]) if i < len(x.on) else "MISSING" for i in range(n_hits)]
        med["full.flac"] = median([x.ms(i) - exp_ms[i] for i in range(min(n_hits, len(x.on)))])
        rep.passfail("audio.flac", [] if ok else ["onset samples differ from full.wav"], "identical onset samples")
    elif not nonempty("full.flac"):
        rep.add("audio.flac", "FAIL", "full.flac " + why("full.flac", "full.flac"))
    elif not tools_ok:
        not_checked("audio.flac", "flac")

    lossy("full.ogg", ("full.ogg",), "audio.ogg", 1.0, "ogg")
    lossy("full.mp3", ("full.mp3",), "mp3.encoder-delay", 1.0, "mp3", expected=True)
    lossy("full.aac", ("full.aac",), "aac.encoder-delay", 1.0, "aac", expected=True)

    # mp4: written since the upstream merge (FD-112 fixed there); its audio starts after the video's
    # lead-in, a known offset until the check compares it shifted by the lead-in (FD-142)
    if nonempty("full.mp4"):
        rep.add("mp4.export", "PASS", "full.mp4 exists")
        lossy("full.mp4", ("full.mp4",), "audio.mp4", 1.0, "mp4", expected=True)
    else:
        rep.add("mp4.export", "FAIL", "full.mp4 " + why("full.mp4", "full.mp4"))

    # spacing of every audio format of the full mix (the owner's direction, FD-038): the intervals
    # between consecutive hits against the expected intervals. It catches jitter, a wrong tempo or a
    # wrong sample rate in any format, lossy ones included, whatever the offset at the start.
    lag = L if L is not None else 0.0
    pred = [exp_ms[i] + (lag if kinds[i] == "note" else 0.0) for i in range(n_hits)]
    for fmtname, name in (("wav", "full.wav"), ("flac", "full.flac"), ("ogg", "full.ogg"),
                          ("mp3", "full.mp3"), ("aac", "full.aac"), ("mp4", "full.mp4")):
        if fmtname == "mp4" and not nonempty("full.mp4"):
            continue
        if name == "full.flac":
            on = aud[name].all_ms() if name in aud and len(aud[name].on) == n_hits else None
        else:
            on = ons_ms.get(name)
        if not on or None in on or len(on) != n_hits:
            rep.add(fmtname + ".spacing", "FAIL", "no complete set of %d onsets in %s" % (n_hits, name))
            continue
        dev = [(on[i + 1] - on[i]) - (pred[i + 1] - pred[i]) for i in range(n_hits - 1)]
        worst = max(range(len(dev)), key=lambda i: abs(dev[i]))
        if fmtname == "mp4":
            # FD-142: only the first interval is off, by the known amount
            off = [i for i in range(len(dev)) if abs(dev[i]) > SPACING_TOL_MS]
            signature = off == [0] and abs(dev[0] - KNOWN_MP4_FIRST_INTERVAL_MS) <= KNOWN_MP4_FIRST_INTERVAL_TOL_MS
            rep.known("mp4.spacing", signature, not off,
                      "; ".join("%s-%s: interval %+.2f ms off" % (hit_name(i), hit_name(i + 1), dev[i]) for i in off)
                      or "%d intervals within %g ms" % (len(dev), SPACING_TOL_MS))
            continue
        rep.passfail(fmtname + ".spacing",
                     ["%s-%s: interval %+.2f ms off" % (hit_name(i), hit_name(i + 1), dev[i])
                      for i in range(len(dev)) if abs(dev[i]) > SPACING_TOL_MS],
                     "%d intervals within %g ms (largest %+.2f, %s-%s)" % (len(dev), SPACING_TOL_MS, dev[worst],
                                                                         hit_name(worst), hit_name(worst + 1)))

    # part: onsets at the note hits, none at the clip-only hit; compared with full.mp3's own onsets
    pp = ("parts", "full-Kick.mp3")
    if not nonempty(*pp):
        rep.add("parts.kick-mp3", "FAIL", "parts/full-Kick.mp3 " + why("parts/full-Kick.mp3", *pp))
    elif not tools_ok:
        not_checked("parts.kick-mp3", "mp3")
    elif load("full-Kick.mp3", pp, True):
        x = aud["full-Kick.mp3"]
        n_note = sum(exp_note)
        probs = []
        cells["full-Kick.mp3"] = ["?"] * n_hits
        if len(x.on) != n_note:
            probs.append("%d onsets, expected %d (note and both hits, none at the clip-only hit)" % (len(x.on), n_note))
        else:
            xm = x.all_ms()
            idx = [i for i in range(n_hits) if exp_note[i]]
            cells["full-Kick.mp3"] = ["—" if not exp_note[i] else fmt(xm[idx.index(i)] - exp_ms[i]) for i in range(n_hits)]
            med["full-Kick.mp3"] = median([xm[k] - exp_ms[i] for k, i in enumerate(idx)])
            # reference: the score-only onsets plus the MP3 delay (full.mp3 against full.wav); at a hit
            # with a clip the full mix shows the clip, not the note, so full.mp3 itself is no reference there
            fm, so = ons_ms.get("full.mp3"), ons_ms.get("score_only.wav")
            if fm is None or so is None or "full.wav" not in ons_ms:
                probs.append("no full.mp3, score_only.wav or full.wav onsets to compare with")
            else:
                delay = median([fm[i] - ons_ms["full.wav"][i] for i in range(n_hits)])
                for k, i in enumerate(idx):
                    if so[i] is None:
                        continue
                    ref = so[i] + delay
                    if abs(xm[k] - ref) > 1.0:
                        probs.append("%s: %+.2f ms from score-only + MP3 delay" % (hit_name(i), xm[k] - ref))
        rep.passfail("parts.kick-mp3", probs, "note hits only; times = score-only onsets + the MP3 delay")

    # -P
    cs = path("cli_parts.status")
    if os.path.exists(cs):
        st = open(cs).read().strip()
        rep.known("cli.parts-hang", st == KNOWN_STATUS["cli.parts-hang"], st == "0",
                  "-P -o full.mp3 %s" % ("timed out (status 124)" if st == "124" else "exit status " + st))
    else:
        rep.add("cli.parts-hang", "FAIL", "cli_parts.status missing")

    # --- MIDI
    midi_t, midi_bb = {}, {}
    mp = path("full.mid")
    if nonempty("full.mid"):
        try:
            div, tempos, notes, ts = parse_midi(mp)
        except Exception as e:
            div = None
            rep.add("midi.time", "FAIL", "cannot read full.mid: %s" % e)
        if div:
            note_hits = [i for i in range(n_hits) if exp_note[i]]
            if len(notes) != len(note_hits):
                rep.add("midi.time", "FAIL", "%d note-ons, expected %d" % (len(notes), len(note_hits)))
                rep.add("midi.bar-beat", "FAIL", "%d note-ons, expected %d" % (len(notes), len(note_hits)))
            else:
                probs = []
                for k, i in enumerate(note_hits):
                    t = midi_secs(notes[k], div, tempos) * 1000.0
                    ref = exp_ms[i]
                    if kinds[i] == "both" and ons_ms.get("clips_only.wav", [None] * n_hits)[i] is not None:
                        ref = ons_ms["clips_only.wav"][i]
                    midi_t[i] = t - ref
                    if abs(t - ref) > 0.6:
                        probs.append("%s: %+.2f ms from %s" % (hit_name(i), t - ref,
                                                                  "clip onset" if kinds[i] == "both" else "expected"))
                    midi_bb[i] = midi_bar_beat(notes[k], div, ts)
                rep.passfail("midi.time", probs, "every note within 0.6 ms of its clip onset or the expected time")
                style = caesura_style({i: notes[k] for k, i in enumerate(note_hits)}, div, tempos)
                rep.add("midi.caesura-style", "INFO", style + " (the owner's rule: lengthened bar)")
                # bar and beat: the known fault is every hit from bar 6 beat 3 on, one bar late
                diff = [i for i in note_hits if midi_bb[i] != (HITS[i][0], Fraction(HITS[i][1]))]
                print_bars = []
                for bar in sorted(set(HITS[i][0] for i in note_hits)):
                    seg = [i for i in note_hits if HITS[i][0] == bar]
                    print_bars.append("bar %d: %s" % (bar, ", ".join(
                        "%s -> %s" % (bb_str(HITS[i][0], HITS[i][1]), bb_str(*midi_bb[i])) for i in seg)))
                rep.bars = print_bars
                # The owner's rule (FD-102, 17 November 2026): a pause is covered by whole extra beats in
                # its own bar ("lengthened bar"): bar numbers stay as written, and only the hits in bar 6
                # after the caesura are later by the same whole number of beats. Today's export inserts a
                # whole bar instead: every hit from 6.3 on is one bar late (the known fault).
                late = [i for i in note_hits if (HITS[i][0], HITS[i][1]) >= (6, 3)]
                in_bar = [i for i in late if HITS[i][0] == 6]
                early = [i for i in note_hits if i not in late]
                early_ok = all(i not in diff for i in early)
                inserted = all(midi_bb[i] == (HITS[i][0] + 1, Fraction(HITS[i][1])) for i in late)
                shifts = set(midi_bb[i][1] - HITS[i][1] for i in in_bar if midi_bb[i][0] == 6)
                lengthened = (style == "lengthened bar" and len(shifts) == 1 and min(shifts) >= 1
                              and all(midi_bb[i][0] == 6 for i in in_bar)
                              and all(i not in diff for i in late if i not in in_bar))
                if early_ok and (inserted or lengthened):
                    rep.add("midi.bar-beat", "PASS", "all hits before the caesura match")
                else:
                    rep.add("midi.bar-beat", "FAIL", "hits off the written bar.beat: " + ", ".join(
                        "%s %s->%s" % (hit_name(i), bb_str(HITS[i][0], HITS[i][1]), bb_str(*midi_bb[i])) for i in diff[:6]))
                if lengthened:
                    detail = "lengthened bar: bar 6 has %s extra beat(s), later bars as written" % min(shifts)
                elif inserted:
                    detail = "inserted bar: %d hits from 6.3 on are one bar late" % len(late)
                else:
                    detail = "style %s, not the owner's rule (lengthened bar)" % style
                if "midi.caesura-bar" in KNOWN:
                    rep.known("midi.caesura-bar", early_ok and inserted, lengthened, detail)
                else:
                    rep.passfail("midi.caesura-bar", [] if lengthened else [detail], detail)
    else:
        rep.add("midi.time", "FAIL", "full.mid " + why("full.mid", "full.mid"))
        rep.add("midi.bar-beat", "FAIL", "full.mid missing")

    # --- notation formats
    want_bb = [(b, bt) for b, bt, k in HITS if k in ("note", "both")]
    want_idx = [i for i in range(n_hits) if exp_note[i]]
    nbb = {}
    sources = []
    if nonempty("full.musicxml"):
        sources.append(("musicxml", "full.musicxml", lambda: parse_musicxml(open(path("full.musicxml"), "rb").read())))
    else:
        rep.add("musicxml.bar-beat", "FAIL", "full.musicxml " + why("full.musicxml", "full.musicxml"))
    if nonempty("full.mxl"):
        sources.append(("mxl", "full.mxl", lambda: parse_musicxml(read_mxl(path("full.mxl")))))
    else:
        rep.add("mxl.bar-beat", "FAIL", "full.mxl " + why("full.mxl", "full.mxl"))
    if nonempty("full.mei"):
        sources.append(("mei", "full.mei", lambda: parse_mei(open(path("full.mei"), "rb").read())))
    else:
        rep.add("mei.bar-beat", "FAIL", "full.mei " + why("full.mei", "full.mei"))
    if nonempty("full.mnx"):
        sources.append(("mnx", "full.mnx", lambda: parse_mnx(open(path("full.mnx"), "rb").read())))
    else:
        rep.add("mnx.bar-beat", "FAIL", "full.mnx " + why("full.mnx", "full.mnx"))
    found_notes = []
    for name, fn, parse in sources:
        try:
            r = parse()
        except Exception as e:
            rep.add(name + ".bar-beat", "FAIL", "cannot read %s: %s" % (fn, e))
            continue
        got = [(b, Fraction(bt)) for b, bt in r["hits"]]
        nbb[name] = got
        if got == [(b, Fraction(bt)) for b, bt in want_bb]:
            rep.add(name + ".bar-beat", "PASS", "%d notes on the written bar.beat" % len(got))
        elif len(got) != len(want_bb):
            rep.add(name + ".bar-beat", "FAIL", "%d kick notes found, expected %d" % (len(got), len(want_bb)))
        else:
            rep.add(name + ".bar-beat", "FAIL", ", ".join(
                "%s %s->%s" % (hit_name(want_idx[k]), bb_str(*want_bb[k]), bb_str(*got[k]))
                for k in range(len(got)) if got[k] != (want_bb[k][0], Fraction(want_bb[k][1])))[:300])
        miss = []
        if [round(t) for t in r["tempo"]] != [120, 90, 90]:
            miss.append("tempo 120, 90, 90 (found %s)" % ([round(t) for t in r["tempo"]],))
        # rit. has its own row: MEI and MNX drop it today (FD-113, expected failures by name)
        known = name in ("mei", "mnx")
        if r["rit"]:
            rep.add(name + ".rit", "UNEXPECTED-PASS" if known else "PASS",
                    ("FD-113: rit. now present; take %s.rit off the expected failures" % name) if known else "rit. present")
        elif known:
            rep.add(name + ".rit", "EXPECTED-FAIL", "FD-113: no rit. (looked for %s)" % r["rit_what"])
        else:
            rep.add(name + ".rit", "FAIL", "NOT-PRESENT: rit. (looked for %s)" % r["rit_what"])
        if not r["fermata"]:
            miss.append("fermata")
        if not r["caesura"]:
            miss.append("caesura (looked for %s)" % r["caesura_what"])
        if miss:
            rep.add(name + ".marks", "FAIL", "NOT-PRESENT: " + "; ".join(miss))
        else:
            rep.add(name + ".marks", "PASS", "tempo 120, 90, 90; fermata; caesura (%s)" % r["caesura_what"])

    # --- other files
    for row, rel in (("file.pdf", "full.pdf"), ("file.svg", "full-1.svg"), ("file.png", "full-1.png"),
                     ("file.mscz", "full.mscz"), ("file.lrc", "full.lrc")):
        if nonempty(rel):
            rep.add(row, "PASS", "%s, %d bytes" % (rel, os.path.getsize(path(rel))))
        else:
            rep.add(row, "FAIL", "%s %s" % (rel, why(rel, rel)))
    if nonempty("full.brf"):
        rep.known("brf.export", False, True, "full.brf exists")
    else:
        st = status.get("full.brf", "?")
        rep.known("brf.export", st == KNOWN_STATUS["brf.export"] and not os.path.exists(path("full.brf")), False,
                  "no full.brf (exit status %s; known: %s)" % (st, KNOWN_STATUS["brf.export"]))

    print_report(rep, exp_ms, cells, med, midi_t, midi_bb, L, nbb, want_idx)
    return rep


def print_report(rep, exp_ms, cells, med, midi_t, midi_bb, L, nbb, want_idx):
    print()
    if exp_ms is not None:
        cols = ["clips_only.wav", "score_only.wav", "full.wav", "full.flac", "full.ogg", "full.mp3", "full.aac",
                "full.mp4", "full-Kick.mp3"]
        cols = [c for c in cols if c in cells]
        print("Hits: onset minus expected time (ms; expected = expected_times.json). — none expected, MISSING, EXTRA, ? = no pairing")
        head = "hit  bar.beat kind  exp ms   " + " ".join(
            c.replace(".wav", "").replace("full.", "").replace("full-", "")[:9].rjust(9) for c in cols)
        head += "  midi dt  midi bb  xml    mxl    mei    mnx"
        print(head)
        pos = {n: {i: k for k, i in enumerate(want_idx)} for n in nbb}
        for i, (b, bt, k) in enumerate(HITS):
            row = "%s  %-8s %-5s %8.0f  " % (hit_name(i), bb_str(b, bt), k, exp_ms[i])
            row += " ".join((cells[c][i] if c in cells else "").rjust(9) for c in cols)
            row += "  " + (fmt(midi_t[i]) if i in midi_t else "—".rjust(8))
            row += "  " + ((bb_str(*midi_bb[i]) + ("*" if midi_bb[i] != (b, Fraction(bt)) else "")) if i in midi_bb else "—").ljust(7)
            for n in ("musicxml", "mxl", "mei", "mnx"):
                if n in nbb and i in pos[n] and pos[n][i] < len(nbb[n]):
                    g = nbb[n][pos[n][i]]
                    row += " " + (bb_str(*g) + ("*" if g != (b, Fraction(bt)) else "")).ljust(6)
                else:
                    row += " " + "—".ljust(6)
            print(row)
        print("(* = differs from the written bar.beat)")
        if getattr(rep, "bars", None):
            print("\nMIDI per bar (written -> from ticks):")
            for line in rep.bars:
                print("  " + line)
        print()
        print("Note lag L (score-only onset minus expected, median): %s" % ("%.2f ms" % L if L is not None else "n/a"))
        for c in cols:
            if med.get(c) is not None:
                print("Median offset from expected, %-15s %+.2f ms" % (c + ":", med[c]))
        print()
    print("Summary")
    for name, st, detail in rep.rows:
        print("  %-22s %-15s %s" % (name, st, detail))
    exp = [(n, KNOWN[n]) for n, st, _ in rep.rows if st == "EXPECTED-FAIL"]
    print()
    print("Expected failures: " + (", ".join("%s (%s)" % e for e in exp) if exp else "none"))


# ---------------------------------------------------------------- self-test with broken files

def midi_rewrite(path, change):
    """Re-encode a MIDI file after change(tracks), where tracks is a list of [abs tick, raw event bytes]."""
    b = open(path, "rb").read()
    hlen = struct.unpack(">I", b[4:8])[0]
    _, ntr, _ = struct.unpack(">HHH", b[8:14])
    i, tracks = 8 + hlen, []
    for _ in range(ntr):
        n = struct.unpack(">I", b[i + 4:i + 8])[0]
        j, end, t, rs, ev = i + 8, i + 8 + n, 0, None, []
        while j < end:
            d, j = vlq(b, j)
            t += d
            s0, c = j, b[j]
            if c == 0xFF:
                ln, k = vlq(b, j + 2)
                j = k + ln
                raw = b[s0:j]
            elif c in (0xF0, 0xF7):
                ln, k = vlq(b, j + 1)
                j = k + ln
                raw = b[s0:j]
            else:
                if c & 0x80:
                    rs = c
                    j += 1
                nd = 1 if rs & 0xF0 in (0xC0, 0xD0) else 2
                raw = bytes([rs]) + b[j:j + nd]
                j += nd
            ev.append([t, raw])
        tracks.append(ev)
        i = end
    change(tracks)
    out = b[:8 + hlen]
    for ev in tracks:
        ev.sort(key=lambda e: (e[0], e[1][:2] == b"\xff\x2f"))
        body, t = b"", 0
        for tt, raw in ev:
            v, enc = tt - t, []
            enc.append(v & 0x7F)
            v >>= 7
            while v:
                enc.append((v & 0x7F) | 0x80)
                v >>= 7
            body += bytes(reversed(enc)) + raw
            t = tt
        out += b"MTrk" + struct.pack(">I", len(body)) + body
    open(path, "wb").write(out)


def note_ons(tracks):
    return [e for ev in tracks for e in ev if e[1][0] & 0xF0 == 0x90 and len(e[1]) == 3 and e[1][2] > 0]


def st_shift_midi_note(d):
    def change(tracks):
        third = sorted(note_ons(tracks), key=lambda e: e[0])[2]   # H03
        third[0] += 240                                           # an eighth late
    midi_rewrite(os.path.join(d, "full.mid"), change)


def st_duplicate_midi_note(d):
    def change(tracks):
        for ev in tracks:
            ons = [e for e in ev if e in note_ons([ev])]
            if ons:
                ev.append([ons[1][0], ons[1][1]])
                return
    midi_rewrite(os.path.join(d, "full.mid"), change)


def st_musicxml_note_late(d):
    p = os.path.join(d, "full.musicxml")
    s = open(p, encoding="utf-8").read()
    div = re.search(r"<divisions>(\d+)</divisions>", s).group(1)
    m = re.search(r'<measure[^>]*number="2"[^>]*>', s)          # bar 1 (the pickup is measure 1)
    k = s.index("<note", m.end())
    s = s[:k] + "<forward><duration>%s</duration></forward>" % div + s[k:]
    open(p, "w", encoding="utf-8").write(s)


def st_silent_wav(d):
    p = os.path.join(d, "score_only.wav")
    w = wave.open(p)
    params, n = w.getparams(), w.getnframes()
    w.close()
    w = wave.open(p, "wb")
    w.setparams(params)
    w.writeframes(b"\0" * (n * params.nchannels * params.sampwidth))
    w.close()


def ffmpeg_delay(src, dst, ms, codec):
    tmp = dst + ".tmp" + os.path.splitext(dst)[1]
    subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", src, "-af", "adelay=%g:all=1" % ms,
                           "-c:a", codec, tmp])
    os.replace(tmp, dst)


def st_ogg_late(d):
    ffmpeg_delay(os.path.join(d, "full.wav"), os.path.join(d, "full.ogg"), 5, "libopus")


def st_ogg_fast(d):
    p = os.path.join(d, "full.ogg")
    tmp = p + ".tmp.ogg"
    subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", os.path.join(d, "full.wav"),
                           "-af", "atempo=1.01", "-c:a", "libopus", tmp])
    os.replace(tmp, p)


def st_mp3_very_late(d):
    ffmpeg_delay(os.path.join(d, "full.wav"), os.path.join(d, "full.mp3"), 300, "libmp3lame")


def set_status(d, name, value):
    p = os.path.join(d, "status.txt")
    lines = [l for l in open(p).read().splitlines() if l.split()[:1] != [name]]
    open(p, "w").write("\n".join(lines + ["%s %s" % (name, value)]) + "\n")


def st_spos_fermata_ignored(d):
    p = os.path.join(d, "full.spos")
    s = open(p).read()

    def later(m):
        v = int(m.group(1))
        return 'position="%d"' % (v - 333 if v >= 12000 else v)   # the fermata's extra time gone
    open(p, "w").write(re.sub(r'position="(\d+)"', later, s))


def st_parts_crash(d):
    open(os.path.join(d, "cli_parts.status"), "w").write("134\n")


def st_failed_export_old_file(d):
    set_status(d, "full.mid", "1")      # the old full.mid stays


def st_brf_other_status(d):
    set_status(d, "full.brf", "1")


SELF_TESTS = [
    ("a MIDI note an eighth late", st_shift_midi_note, ["midi.time"], False),
    ("a MIDI note written twice", st_duplicate_midi_note, ["midi.time"], False),
    ("a MusicXML note a beat late", st_musicxml_note_late, ["musicxml.bar-beat"], False),
    ("a silent score-only WAV", st_silent_wav, ["audio.score-only"], False),
    ("the OGG 5 ms late", st_ogg_late, ["audio.ogg"], True),
    ("the MP3 300 ms late, not the known 51 ms", st_mp3_very_late, ["mp3.encoder-delay"], True),
    ("the OGG 1 % too fast", st_ogg_fast, ["ogg.spacing"], True),
    ("spos without the fermata's extra time", st_spos_fermata_ignored, ["spos.times"], False),
    ("-P crashing instead of hanging", st_parts_crash, ["cli.parts-hang"], False),
    ("a failed MIDI export with an old file left", st_failed_export_old_file, ["export.full.mid", "midi.time"], False),
    ("Braille failing with another status", st_brf_other_status, ["brf.export"], False),
]


def run_self_test(a):
    """Break a copy of a finished run in one way at a time; each must make the named rows FAIL."""
    import contextlib
    import tempfile
    failures = 0
    print("Self-test on copies of %s: each broken file must make the named rows FAIL" % a.dir)
    for name, fn, rows, needs_ffmpeg in SELF_TESTS:
        if needs_ffmpeg and not (have_tool("ffmpeg") and have_tool("ffprobe")):
            print("  NOT RUN  %-45s ffmpeg missing" % name)
            failures += 1
            continue
        with tempfile.TemporaryDirectory() as tmp:
            d = os.path.join(tmp, "run")
            shutil.copytree(a.dir, d, ignore=shutil.ignore_patterns("project", "logs"))
            fn(d)
            ns = argparse.Namespace(dir=d, score=a.score, allow_missing_tools=False, break_offset=0.0,
                                    expected=a.expected)
            with contextlib.redirect_stdout(io.StringIO()):
                rep = run_check(ns)
        got = {r[0]: r[1] for r in rep.rows}
        ok = all(got.get(r) == "FAIL" for r in rows)
        failures += 0 if ok else 1
        print("  %-8s %-45s %s" % ("OK" if ok else "MISSED", name,
                                   ", ".join("%s %s" % (r, got.get(r, "no row")) for r in rows)))
    print()
    print("SELF-TEST: %s" % ("PASS" if not failures else "FAIL (%d)" % failures))
    return 1 if failures else 0


def main(argv):
    ap = argparse.ArgumentParser(description="Export check helper (see the header of this file).")
    sub = ap.add_subparsers(dest="cmd", required=True)
    mp = sub.add_parser("make-project")
    mp.add_argument("--score", required=True)
    mp.add_argument("--out", required=True)
    mp.add_argument("--break-offset", type=float, default=0.0)
    ck = sub.add_parser("check")
    ck.add_argument("--dir", required=True)
    ck.add_argument("--score", required=True)
    ck.add_argument("--allow-missing-tools", action="store_true")
    ck.add_argument("--break-offset", type=float, default=0.0)
    ck.add_argument("--expected", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                       "testdata", "export_check", "expected_times.json"))
    st = sub.add_parser("self-test")
    st.add_argument("--dir", required=True, help="a finished run's output directory")
    st.add_argument("--score", required=True)
    st.add_argument("--expected", default=ck.get_default("expected"))
    a = ap.parse_args(argv)
    if a.cmd == "self-test":
        return run_self_test(a)
    if a.cmd == "make-project":
        make_project(a.score, a.out, a.break_offset)
        return 0
    rep = run_check(a)
    bad = [r for r in rep.rows if r[1] in ("FAIL", "UNEXPECTED-PASS")
           or (r[1] == "NOT-CHECKED" and not a.allow_missing_tools)]
    print()
    if bad:
        print("RESULT: FAIL (%d)" % len(bad))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
