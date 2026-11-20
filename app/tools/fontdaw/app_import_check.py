#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw
"""App check for Batch 2 workstream 04: projects whose audio is WAV, FLAC, MP3 (LAME tag),
Ogg Vorbis, and FontDaw's own Ogg (Opus) and MP3 exports, exported by the release app to WAV
(the mixdown) and saved again as .mscz.

Checks per format: the app exports the audio (it is in the mixdown), it is in sync with the WAV
project's mixdown (lag by cross-correlation), and a save keeps the embedded bytes byte for byte
and writes arrangement.json version 2 exactly when a non-WAV asset is in it.

Usage: app_import_check.py APP OUTDIR
DISPLAY and an audio sink must be set up as for app_export_check.sh (Xvfb, a PulseAudio null sink).
Needs ffmpeg (libmp3lame, libvorbis) and numpy. Prints a table; exits 0 or 1.
"""
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import zipfile

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools/fontdaw"))
import app_export_check as aec  # noqa: E402

APP, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
SCORE = os.path.join(ROOT, "tools/fontdaw/testdata/export_check/score.mscx")
SR = aec.SR


def ff(*args):
    subprocess.check_call(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", *args])


def pcm(path, rate=48000):
    """Mono float samples at `rate` (ffmpeg), for comparing mixdowns."""
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", path, "-ac", "1", "-ar", str(rate),
                                   "-f", "f32le", "-"])
    return np.frombuffer(raw, dtype=np.float32)


def frames_of(path):
    return len(subprocess.check_output(["ffmpeg", "-v", "error", "-i", path, "-f", "s16le", "-"])) // (
        2 * int(subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries",
                                         "stream=channels", "-of", "csv=p=0", path]).decode().strip()))


def app(output, project, limit=180):
    log = output + ".log"
    with open(log, "w") as f:
        st = subprocess.call(["timeout", "-k", "10", str(limit), APP, "-o", output, project],
                             stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    return st


def write_project(path, score_text, data, fmt, sr, channels, frames, clips):
    aid = hashlib.sha1(data).hexdigest()
    z = zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED)
    z.writestr("META-INF/container.xml",
               '<?xml version="1.0" encoding="UTF-8"?>\n<container>\n  <rootfiles>\n'
               '    <rootfile full-path="score.mscx">\n      </rootfile>\n    </rootfiles>\n  </container>\n')
    z.writestr("score.mscx", score_text)
    arr = {"version": 1 if fmt == "wav" else 2,
           "assets": [{"id": aid, "name": "Audio", "format": fmt, "storage": "embedded",
                       "file": "audio/%s.%s" % (aid, fmt), "sampleRate": sr, "channels": channels,
                       "frames": frames}],
           "tracks": [{"id": 1, "kind": "audio", "name": "Audio", "timeBase": "musical",
                       "clips": [dict(c, id=10 + i, asset=aid, name="Audio", gainDb=0) for i, c in enumerate(clips)]}]}
    z.writestr("arrangement.json", json.dumps(arr, indent=1))
    z.writestr("audio/%s.%s" % (aid, fmt), data, compress_type=zipfile.ZIP_STORED)
    z.close()
    return aid


def lag_and_match(ref, x, max_lag=4800):
    """Lag (frames at 48 kHz, + = x late) with the best cross-correlation, and the correlation there."""
    n = min(len(ref), len(x))
    r, y = ref[:n].astype(np.float64), x[:n].astype(np.float64)
    size = 1 << int(np.ceil(np.log2(2 * n)))
    c = np.fft.irfft(np.fft.rfft(y, size) * np.conj(np.fft.rfft(r, size)), size)
    c = np.concatenate([c[-max_lag:], c[:max_lag + 1]])
    lag = int(np.argmax(c)) - max_lag
    shifted = y[lag:] if lag >= 0 else np.concatenate([np.zeros(-lag), y])
    m = min(len(r), len(shifted))
    corr = float(np.dot(r[:m], shifted[:m]) / (np.linalg.norm(r[:m]) * np.linalg.norm(shifted[:m]) + 1e-12))
    return lag, corr


def main():
    text = open(SCORE, encoding="utf-8").read()
    rests = re.sub(r"<Chord>.*?<durationType>(\w+)</durationType>.*?</Chord>",
                   r"<Rest><durationType>\1</durationType></Rest>", text, flags=re.S)
    kick, kick_frames = aec.kick_wav(0)
    open(os.path.join(OUT, "kick.wav"), "wb").write(kick)
    k = os.path.join(OUT, "kick")
    ff("-i", k + ".wav", "-c:a", "flac", k + ".flac")
    ff("-i", k + ".wav", "-c:a", "libmp3lame", "-b:a", "192k", k + ".mp3")   # with a LAME/Xing tag
    ff("-i", k + ".wav", "-c:a", "libvorbis", "-q:a", "6", k + ".ogg")

    clip_hits = [(bar, beat) for bar, beat, kind in aec.HITS if kind in ("both", "clip")]

    def kick_clips(frames):
        return [{"startTick": aec.clip_tick(bar, beat), "sourceOffset": 0, "length": frames / SR}
                for bar, beat in clip_hits]

    rows = []
    projects = {}
    for fmt, src in (("wav", k + ".wav"), ("flac", k + ".flac"), ("mp3", k + ".mp3"), ("ogg", k + ".ogg")):
        data = open(src, "rb").read()
        p = os.path.join(OUT, "kick_%s.mscz" % fmt)
        write_project(p, rests, data, fmt, SR, 1, kick_frames, kick_clips(kick_frames))
        projects["kick " + fmt] = (p, data, fmt)

    # FontDaw's own OGG (Opus) and MP3 exports of the WAV project, as one clip from the start
    ref_project = projects["kick wav"][0]
    for fmt in ("ogg", "mp3"):
        exported = os.path.join(OUT, "fontdaw_export.%s" % fmt)
        st = app(exported, ref_project)
        if st != 0 or not os.path.exists(exported):
            rows.append(("FontDaw %s export" % fmt, "FAIL", "export exit %d" % st))
            continue
        data = open(exported, "rb").read()
        frames = frames_of(exported)
        sr = int(subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries",
                                          "stream=sample_rate", "-of", "csv=p=0", exported]).decode().strip())
        p = os.path.join(OUT, "fontdaw_%s.mscz" % fmt)
        write_project(p, rests, data, fmt, sr, 2, frames,
                      [{"startTick": "0/1", "sourceOffset": 0, "length": frames / sr}])
        projects["FontDaw-exported " + fmt] = (p, data, fmt)

    mixes = {}
    for name, (p, data, fmt) in projects.items():
        wav = p[:-5] + "_mix.wav"
        st = app(wav, p)
        mixes[name] = wav if st == 0 and os.path.exists(wav) else None
        if mixes[name] is None:
            rows.append((name, "FAIL", "mixdown export exit %d" % st))

    ref = pcm(mixes["kick wav"]) if mixes.get("kick wav") else None
    for name, (p, data, fmt) in projects.items():
        detail = []
        ok = True
        if mixes.get(name) is not None and ref is not None:
            x = pcm(mixes[name])
            level = float(np.max(np.abs(x))) if len(x) else 0.0
            lag, corr = lag_and_match(ref, x)
            detail.append("peak %.2f, lag %+d frames (%.1f ms), correlation %.4f" % (level, lag, lag / 48.0, corr))
            expect_late = name == "FontDaw-exported mp3"   # no LAME tag (FD-110): the delay stays
            if level < 0.1 or corr < 0.95 or (not expect_late and abs(lag) > 2):
                ok = False
        else:
            ok = False

        # Saved again (export to .mscz is a save): audio byte for byte, version as decided
        resaved = p[:-5] + "_resaved.mscz"
        st = app(resaved, p)
        if st == 0 and os.path.exists(resaved):
            z = zipfile.ZipFile(resaved)
            audio = [n for n in z.namelist() if n.startswith("audio/")]
            same = len(audio) == 1 and z.read(audio[0]) == data and audio[0].endswith("." + fmt)
            version = json.loads(z.read("arrangement.json"))["version"]
            want = 1 if fmt == "wav" else 2
            detail.append("resaved: %s, arrangement.json version %d" % ("bytes identical" if same else "BYTES DIFFER", version))
            ok = ok and same and version == want
        else:
            ok = False
            detail.append("resave exit %d" % st)
        rows.append((name, "ok" if ok else "FAIL", "; ".join(detail)))

    print("| Project | Result | Measured |")
    print("|---|---|---|")
    for r in rows:
        print("| %s | %s | %s |" % r)
    return 0 if all(r[1] == "ok" for r in rows) else 1


if __name__ == "__main__":
    sys.exit(main())
