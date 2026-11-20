#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Export check (FD-038): export one small test project from the command line to every format the
# app offers and check that each known hit lands at the same time everywhere.
# Design: tools/fontdaw/testdata/export_check/README.md.
#
# Usage: tools/fontdaw/app_export_check.sh [--app PATH] [--out DIR] [--score PATH] [--allow-missing-tools]
#                                          [--break-offset MS] [--keep]
#   --app PATH             the release app (default: build.release/src/app/fontdaw)
#   --out DIR              where exports and the report go (default: a temporary directory)
#   --allow-missing-tools  a format whose decoder is missing is not a failure
#   --score PATH           the test score (default: testdata/export_check/score.mscx)
#   --break-offset MS      self-test: delay the generated kick by MS, so the clip checks must fail
#   --keep                 keep the output directory (always kept with --out, and on failure)
#   --self-test            afterwards, break copies of the exports one way at a time (a MIDI note late,
#                          a MusicXML note late, a silent WAV, late OGG and MP3 files, other exit
#                          statuses) and check that each makes its rows FAIL
#   --out must be a new or empty directory, so that no old file can pass for a new export.
# Exit status 0 only if every check passed (expected failures listed by name do not fail).
#
# The unit-test build's own binary hangs on audio export (FD-038): use a release build.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
APP="$ROOT/build.release/src/app/fontdaw"
OUT=""
SCORE="$HERE/testdata/export_check/score.mscx"
CHECK_ARGS=()
MAKE_ARGS=()
KEEP=0
SELF_TEST=0

while [ $# -gt 0 ]; do
    case "$1" in
        --app) APP="$2"; shift 2 ;;
        --out) OUT="$2"; KEEP=1; shift 2 ;;
        --score) SCORE="$2"; shift 2 ;;
        --allow-missing-tools) CHECK_ARGS+=(--allow-missing-tools); shift ;;
        --break-offset) CHECK_ARGS+=(--break-offset "$2"); MAKE_ARGS+=(--break-offset "$2"); shift 2 ;;
        --keep) KEEP=1; shift ;;
        --self-test) SELF_TEST=1; shift ;;
        -h|--help) sed -n '2,23p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ ! -x "$APP" ]; then
    echo "app_export_check: app not found: $APP (build a release build, or pass --app)" >&2
    exit 2
fi

# Display and audio device: the app needs xcb and a sound device even for command-line export.
XVFB_PID=""
STARTED_PULSE=0
cleanup() {
    local st=$?
    [ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null || true
    [ "$STARTED_PULSE" = 1 ] && pulseaudio --kill >/dev/null 2>&1 || true
    if [ -n "$OUT" ]; then
        if [ "$KEEP" = 0 ] && [ "$st" = 0 ]; then
            rm -rf "$OUT"
        else
            echo "output kept in $OUT"
        fi
    fi
}
trap cleanup EXIT

if [ -z "${DISPLAY:-}" ]; then
    n=99
    while [ -e "/tmp/.X11-unix/X$n" ] || [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
    Xvfb ":$n" -screen 0 1280x800x24 >/dev/null 2>&1 &
    XVFB_PID=$!
    export DISPLAY=":$n"
    for _ in $(seq 50); do [ -e "/tmp/.X11-unix/X$n" ] && break; sleep 0.1; done
    if [ ! -e "/tmp/.X11-unix/X$n" ]; then echo "app_export_check: Xvfb did not start on :$n" >&2; exit 2; fi
fi
export LANG=C.UTF-8
if ! pactl list short sinks 2>/dev/null | grep -q .; then
    if ! pulseaudio --check >/dev/null 2>&1; then
        pulseaudio --start --exit-idle-time=-1 >/dev/null 2>&1 && STARTED_PULSE=1
    fi
    pactl load-module module-null-sink sink_name=null >/dev/null 2>&1 || true
fi
# without a sound device every audio export runs into its time limit: say so instead
if ! pactl list short sinks 2>/dev/null | grep -q .; then
    echo "app_export_check: no audio sink (PulseAudio with a null sink is needed; see pulseaudio --start)" >&2
    exit 2
fi

# a reused output directory would let an old file pass for a new export
if [ -n "$OUT" ] && [ -d "$OUT" ] && [ -n "$(ls -A "$OUT" 2>/dev/null)" ]; then
    echo "app_export_check: --out $OUT is not empty; give a new or empty directory" >&2
    OUT=""
    exit 2
fi
if [ -z "$OUT" ]; then OUT="$(mktemp -d)"; fi
mkdir -p "$OUT/logs" "$OUT/parts" "$OUT/cli_parts" "$OUT/project"
echo "output directory: $OUT"
: > "$OUT/status.txt"

python3 "$HERE/app_export_check.py" make-project --score "$SCORE" --out "$OUT/project" ${MAKE_ARGS[@]+"${MAKE_ARGS[@]}"}

# export NAME TIMEOUT PROJECT OUTPUT [app options]: one app run; log and exit status recorded
export_one() {
    local name="$1" limit="$2" project="$3" output="$4" st=0
    shift 4
    timeout -k 10 "$limit" "$APP" "$@" -o "$output" "$project" >"$OUT/logs/${name//\//_}.log" 2>&1 </dev/null || st=$?
    echo "$name $st" >> "$OUT/status.txt"
}

P="$OUT/project"
for ext in wav flac ogg mp3 aac mp4 mid musicxml mxl mei mnx brf pdf svg png mscz lrc spos mpos; do
    echo "exporting full.$ext"
    export_one "full.$ext" 180 "$P/full.mscz" "$OUT/full.$ext"
done
echo "exporting score_only.wav, clicks_only.wav"
export_one score_only.wav 180 "$P/score_only.mscz" "$OUT/score_only.wav"
export_one clicks_only.wav 180 "$P/clicks_only.mscz" "$OUT/clicks_only.wav"
echo "exporting parts/full-*.mp3"
export_one "parts/full-*.mp3" 180 "$P/full.mscz" "$OUT/parts/full-*.mp3"
echo "exporting with -P (score and parts)"
st=0
timeout -k 10 60 "$APP" -P -o "$OUT/cli_parts/full.mp3" "$P/full.mscz" >"$OUT/logs/cli_parts.log" 2>&1 </dev/null || st=$?
echo "$st" > "$OUT/cli_parts.status"

result=0
python3 "$HERE/app_export_check.py" check --dir "$OUT" --score "$SCORE" ${CHECK_ARGS[@]+"${CHECK_ARGS[@]}"} || result=$?
if [ "$SELF_TEST" = 1 ]; then
    echo
    python3 "$HERE/app_export_check.py" self-test --dir "$OUT" --score "$SCORE" || result=1
fi
exit "$result"
