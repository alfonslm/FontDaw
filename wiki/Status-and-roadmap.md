# Status and roadmap

What works today, what comes next, and what we mean by "1.0". Nothing here is a promise of a date.

## What works today

Most of what works comes from MuseScore Studio, which FontDaw is built on:

- Writing, editing and printing scores, including parts.
- Importing and exporting MusicXML, MIDI and other score formats.
- Playing a score with SoundFonts or VST3 instruments, with a mixer: volume, pan, mute, solo, effects and aux sends.
- Metronome and count-in.
- Exporting a mixdown as WAV, FLAC, MP3, Ogg or AAC.

New in FontDaw:

- **An audio file plays with the score.** Use File > Import audio track to put a WAV, FLAC, MP3 or Ogg file on its own track. It shows up in the score as an audio staff with its waveform, plays in time with the score, has its own mixer strip, is included in the exported mixdown, and is saved in the project.
- **Faster export of parts.** Exporting each part to its own audio file now renders all parts in one pass.
- **FontDaw's own name, settings and look.** It is designed to run next to MuseScore Studio without sharing its settings, and it makes no connection to MuseScore's online services. Some texts in the program still say MuseScore Studio.

Be aware that we have worked out what "works" mostly by reading the code, not by testing every feature in a running build. Some entries may turn out to be less finished than listed.

## What is not there yet

- A separate timeline view with tracks. Audio tracks show up in the score as audio staves, but there is no timeline panel yet.
- Editing audio: moving, trimming, splitting, fades, clip gain.
- Audio formats other than WAV, FLAC, MP3 and Ogg, such as AAC and AIFF.
- Recording from a microphone or instrument.
- Stretching audio to the score's tempo.
- A piano roll.
- Installers and packages. There are no public builds.

## What comes next

The plan, in the order we expect to do it. The order may change.

1. **A timeline** with the score's parts and audio tracks side by side, with waveforms, a moving playhead and click-to-seek.
2. **Audio editing** on that timeline: move, trim, split, fade, clip gain and snapping to the bars.
3. **Safer saving** of projects that contain a lot of audio.
4. **Beat matching:** stretching a recording to the score's tempo without changing its pitch.
5. **Recording:** audio input from the sound card, monitoring, and latency compensation.
6. **Plug-in delay compensation and automation lanes.**
7. **Piano roll** with velocity and controller editing.
8. **Mastering mode:** all tracks are rendered, and only the master channel's effect chain plays live. Editing a note then renders just that track again.

Steps 1 to 3 have a written plan. Parts of step 2 already exist, such as clip volume and fades, and undo already covers audio edits.

## What 1.0 means

We call it FontDaw 1.0 when you can do all of this in one program:

- write a score,
- record and import audio next to it,
- edit that audio on a timeline and beat-match it to the score's tempo,
- edit MIDI in a piano roll,
- mix with plug-ins and automation that stay in time with each other,
- export the result.

Of the 43 things we count as necessary for 1.0, 18 work today, 12 partly and 13 not at all (counted 16 November 2026, mostly from reading the code). There are further features we expect people to look for after 1.0. We list those in [Features](Features) once they exist.

Not planned: a clip-launching "session view" for live performance, a bundled library of synths and sample content, cloud collaboration and accounts, and anything that depends on Muse Group's online sound library.
