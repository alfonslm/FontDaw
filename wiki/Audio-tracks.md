# Audio tracks

How to put an audio file next to your score. This is the first DAW feature in FontDaw and it is limited. A timeline view and clip editing are planned.

## Importing a file

Choose **File > Import audio track** and pick a WAV, FLAC, MP3 or Ogg file. It gets its own track and shows up in the score as an audio staff with its waveform, in every view and in print. It plays from the start of the score, in time with it. If you insert or delete bars, the audio moves with them.

The audio is stored inside the project file. Linking to the file on disk instead is prepared but has no setting yet.

## Playing, mixing and exporting

The audio track has its own strip in the mixer, so you can set volume and pan, mute or solo it and add effects. It is included when you export a mixdown. It is saved with the project and comes back when you reopen it. The single-file `.mscx` format cannot hold audio, so it is saved without it.

## What does not work yet

- Only WAV, FLAC, MP3 and Ogg (Vorbis or Opus) files, mono or stereo. AAC/M4A, AIFF and others are not supported yet.
- The audio always starts at the beginning of the score. You cannot move, trim or split it yet.
- There is no separate timeline view yet. The waveform is shown on the audio staff in the score.
- The audio is not stretched to the score's tempo (beat matching is planned).
