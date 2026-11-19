#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
#
# FontDaw
# The notation-first DAW
#
# Copyright (C) 2026 FontDaw contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Writes the two drop-downs under the front page's goals ("Planned, not started yet" and
# "The full feature list") from the list below, between the FEATURE-LIST markers in
# web/index.html.
#
#   python3 tools/build_goals.py
#
# Edit the list, run the script, commit both files. Python 3 only, nothing to install.

import html
import pathlib
import sys

W, P, I, N = "works", "partly works", "in progress", "planned"
F = [
 ("Project and timeline", [
  ("Project file holding the score, audio and mixer settings", W),
  ("Tempo changes and time signatures", W),
  ("Autosave and backups", W),
  ("Project templates", W),
  ("Position shown in bars and beats, and in minutes and seconds", P),
  ("Undo and redo for every edit, audio edits included", P),
  ("Markers and regions", P),
  ("A timeline view with tracks and clips", N),
  ("Building a tempo map from a recording", N)]),
 ("Tracks", [
  ("Audio tracks", P),
  ("Audio tracks as parts of their own", I),
  ("Instrument tracks played from the notation", W),
  ("Track templates and sound presets: a plug-in, its sound and its articulation map in one", I),
  ("Folder and group tracks", N),
  ("Freeze or bounce a track to audio", N),
  ("MIDI tracks without notation", N)]),
 ("Recording", [
  ("Metronome and count-in", W),
  ("Recording from the sound card: arm, record, stop, and the clip lands on the beat", N),
  ("Hearing the input while recording", N),
  ("Latency compensation for recordings", N),
  ("Loop recording with takes and comping, pre-roll, punch in and out", N)]),
 ("Audio editing", [
  ("Import WAV, FLAC, MP3, Ogg Vorbis and Opus files", W),
  ("Waveform display", P),
  ("Move, trim, split, duplicate and delete clips", P),
  ("Clip volume", P),
  ("Fades and crossfades", P),
  ("Moving clips directly in the score", N),
  ("Snap to bars and beats", N),
  ("Beat matching: stretch a recording to the score's tempo without changing its pitch, with beat markers", N),
  ("Audio that follows tempo changes and ritardandi", N),
  ("Pitch shift without changing speed", N),
  ("Normalise, reverse, silence", N),
  ("Sample-level and spectral editing", N),
  ("Damaged audio files keep their good parts and show where the damage is", N),
  ("“Locate missing audio” when files have moved", N)]),
 ("MIDI", [
  ("MIDI keyboard input", W),
  ("MIDI file import and export", W),
  ("MIDI export with real pauses: fermatas and caesuras played as written silence", W),
  ("MIDI remote control and MIDI learn", W),
  ("Real-time MIDI recording", P),
  ("MIDI out to hardware", P),
  ("A performance layer: timing and velocity apart from the written notes", P),
  ("Quantise", P),
  ("Articulation maps for sample libraries, with no keyswitch notes in the score", I),
  ("Velocity and controller editing", N),
  ("Piano roll", N)]),
 ("Notation", [
  ("Full score editing, parts, engraving and printing", W),
  ("MusicXML, MEI, Guitar Pro and other import and export", W),
  ("Score and audio in sync, shown together in the score: audio staves in every view and in print", W),
  ("A ring menu on right-click, and a command menu", W),
  ("MuseScore's shortcut layout as the default", W),
  ("Hidden beats: pauses written as note values, held or silent (playback and MIDI export work; editing them in the score is planned)", P),
  ("Clearer menu labels, with icons and names", I),
  ("A new selection system: a soft glow shows what a click will act on", I),
  ("Shortcut layouts of your own", I),
  ("A time view, where one second always takes the same width, with its own zoom", I),
  ("Dark score pages, and a separate print view with a preview of the printed colours", I),
  ("A score view and a timeline view that stay in sync", N),
  ("Editing while the score plays", N),
  ("FontDaw's own shortcut layout", N),
  ("A score repair assistant that brings older scores up to date in one place", N),
  ("A screen type setting per screen (LCD or OLED: black level and contrast)", N)]),
 ("Mixing", [
  ("Volume, pan, mute and solo per channel", W),
  ("Insert effects per channel", W),
  ("Aux sends", W),
  ("Level meters", W),
  ("Master channel", W),
  ("Built-in EQ, compressor, limiter and reverb", P),
  ("Plug-in delay compensation", N),
  ("Buses, submix groups and free routing", N),
  ("Loudness metering (LUFS)", N),
  ("Sidechain inputs", N),
  ("Surround and immersive formats", N)]),
 ("Mastering", [
  ("An editing mode where everything stays live and editable", W),
  ("A mastering chain on the master channel", P),
  ("A mastering mode: all tracks bounced, only the master chain live", N),
  ("Re-rendering only the tracks that changed", N),
  ("Loudness and true-peak meters next to the chain", N)]),
 ("Automation", [
  ("Tempo automation", W),
  ("Automation lanes", P),
  ("Volume and pan automation (not yet confirmed)", P),
  ("Recording automation from fader moves", N),
  ("Plug-in parameter automation", N)]),
 ("Instruments and plug-ins", [
  ("VST3 instruments and effects", W),
  ("Plug-in scanning that survives a crashing plug-in", W),
  ("SoundFont playback", W),
  ("VST instruments that follow the score as it starts, stops and jumps", W),
  ("CLAP, LV2 and Audio Units", N),
  ("A plug-in preset browser", N),
  ("The SFZ sampler format", N),
  ("Built-in synths and drum machine", N)]),
 ("Audio engine and devices", [
  ("Output through WASAPI, ASIO, Core Audio, ALSA and PipeWire", W),
  ("Sample rate and buffer size settings", W),
  ("Smooth playback while editing (large scores still redraw slowly; work on speed has started)", P),
  ("JACK output", N),
  ("Input through the same drivers", N),
  ("Several outputs, such as a headphone mix", N)]),
 ("Transport and sync", [
  ("Play, stop, rewind, seek and loop", W),
  ("A record button", N),
  ("JACK transport, Ableton Link, MIDI clock and timecode, scrubbing", N)]),
 ("Export", [
  ("Mixdown to WAV, FLAC, MP3 and Ogg", W),
  ("Score video export", W),
  ("Stems: one file per track", P),
  ("Bit depth, dither and loudness target (not yet confirmed)", P),
  ("Exporting a time range", N),
  ("Audio staves included when exporting a score", N)]),
 ("Video", [
  ("A video track for scoring to picture", N)]),
]

LABEL = {W: "Works", P: "Partly works", I: "In progress", N: "Planned"}
CLASS = {W: "works", P: "partly", I: "progress", N: "planned"}
PAGE = pathlib.Path(__file__).resolve().parent.parent / "web" / "index.html"
START, END = "<!-- FEATURE-LIST START -->", "<!-- FEATURE-LIST END -->"


def planned():
    out = ['<details class="more">',
           '  <summary>Planned, not started yet</summary>',
           '  <p class="more-note">Things we want FontDaw to do that no work has started on. Nothing here is a promise of a date.</p>']
    for section, items in F:
        todo = [name for name, status in items if status == N]
        if todo:
            out.append('  <h3>%s</h3>' % html.escape(section))
            out.append('  <ul class="plain-list">')
            out += ['    <li>%s</li>' % html.escape(name) for name in todo]
            out.append('  </ul>')
    out.append('</details>')
    return out


def full():
    out = ['<details class="more">',
           '  <summary>The full feature list</summary>',
           '  <p class="more-note">Everything we count as a FontDaw feature, and how far each one has come. '
           '<strong>Works</strong>: usable today. <strong>Partly works</strong>: some of it is usable. '
           '<strong>In progress</strong>: work or design has started. <strong>Planned</strong>: not started.</p>']
    for section, items in F:
        out.append('  <h3>%s</h3>' % html.escape(section))
        out.append('  <ul class="feature-list">')
        for name, status in items:
            out.append('    <li><span class="badge badge-%s">%s</span> <span>%s</span></li>'
                       % (CLASS[status], LABEL[status], html.escape(name)))
        out.append('  </ul>')
    out.append('</details>')
    return out


def main():
    page = PAGE.read_text(encoding="utf-8")
    for marker in (START, END):
        if page.count(marker) != 1:
            sys.exit("%s must contain the marker %s exactly once (found %d)" % (PAGE.name, marker, page.count(marker)))
    head, rest = page.split(START, 1)
    _, tail = rest.split(END, 1)
    body = "\n".join("    " + line for line in planned() + full())
    PAGE.write_text(head + START + "\n" + body + "\n    " + END + tail, encoding="utf-8")
    print("Wrote the feature list into", PAGE.name)


if __name__ == "__main__":
    main()
