# Features

What FontDaw can do, grouped by area. Each feature is marked **Works** (usable today), **In progress** (part of it exists) or, in the plan only, left out. We list only what exists or is under way; [Status and roadmap](Status-and-roadmap) has what is planned.

Most of the features that work come from MuseScore Studio. We have so far confirmed them by reading the code rather than by testing a FontDaw build, so treat the list as a first draft.

## Project and timeline

- **Works:** a project file that holds the score, imported audio and the mixer state; tempo map and time signatures; autosave and backups; project templates.
- **In progress:** the position display shows bars and beats and minutes and seconds, but there is no time ruler yet. Undo and redo cover the score but not yet changes to audio tracks. Markers and regions are partly there.
- There is no timeline view yet.

## Tracks

- **Works:** instrument tracks driven by the notation.
- **In progress:** audio tracks. You can import a WAV file onto its own track (see [Audio tracks](Audio-tracks)). The track plays from the start of the score as one whole file.

## Audio recording

- **Works:** metronome and count-in.
- Recording audio is not started. See [Status and roadmap](Status-and-roadmap).

## Audio editing

- **In progress:** importing audio (WAV only so far). Clip gain, fades, and moving, trimming and splitting clips are prepared behind the scenes, but you cannot use them yet. There is no waveform display.

## MIDI

- **Works:** MIDI keyboard input; importing and exporting MIDI files; MIDI remote control and MIDI learn.
- **In progress:** real-time MIDI recording, quantising, MIDI output to external hardware, and timing and velocity that differ from the written notes.
- There is no piano roll yet.

## Notation

- **Works:** full score editing, parts, engraving and printing; import and export of MusicXML, MEI, Guitar Pro and other formats.

## Mixing

- **Works:** volume, pan, mute and solo per channel; insert effects; aux sends; level meters; a master channel. Imported audio tracks get a mixer strip.
- **In progress:** built-in EQ, compressor, limiter and reverb.

## Mastering

- **In progress:** an ordered effect chain on the master channel. A separate mastering mode is planned.

## Automation

- **Works:** tempo changes written in the score.
- **In progress:** volume and pan automation, and automation lanes in a timeline. The score already has automation curves for dynamics, tempo, volume and pan; how complete they are has not been checked.

## Instruments and plug-ins

- **Works:** VST3 instruments and effects; SoundFont playback; plug-in scanning that survives a plug-in that crashes.
- FontDaw does not use Muse Sounds.

## Audio engine and devices

- **Works:** audio output through WASAPI and ASIO on Windows, Core Audio on macOS, and ALSA on Linux (and PipeWire in the Linux packages). JACK support is in the code but not switched on. Sample rate and buffer size settings; the sample rate choice is shown on Linux only.
- **In progress:** playback that keeps going while you edit a score. This is not yet tested with audio tracks.
- Audio input from the sound card is not started.

## Transport and sync

- **Works:** play, stop, rewind, seek and loop. The playback cursor follows what you hear, with latency settings.

## Export

- **Works:** mixdown to WAV, FLAC, MP3, Ogg and AAC, including imported audio tracks, with a choice of bit depth for WAV and FLAC. Exporting each part to its own file works on all parts in one pass, which is faster.
- **In progress:** dither and loudness options; stems (one file per track).

## Video

- **Works:** exporting a score as a video.
