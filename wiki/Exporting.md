# Exporting

How to get audio, and video, out of FontDaw.

## Audio formats and settings

Choose **File > Export** and pick MP3, WAV, OGG, FLAC or AAC. Each format has its own settings page, with a sample rate and, for the compressed formats, a bit rate. WAV also offers 16-bit, 24-bit and 32-bit float sample formats, and FLAC 16-bit and 24-bit.

A full mixdown includes imported audio tracks. The file lasts as long as the score or the audio, whichever ends later.

## Exporting parts

The export dialog lets you choose which parts to export, with **Select all** and **Clear selection**. Each selected part becomes its own audio file.

Two options control how this is done. Both are on by default:

- **Multi-stem render** works on all selected parts at once. It can be much faster for large scores. Turn it off if a plug-in sounds different or fails to load during export. The result is close to the old way of exporting one part after another, but not identical.
- **Idle instruments until their first note** skips an instrument until shortly before it first plays. Turn it off for instruments that make sound without notes, such as drones.

*Stub: whether imported audio tracks are included in the files of single parts has not been decided.*

## Video

FontDaw can export a score as a video. This needs the FFmpeg libraries. If it is missing, the export page says so and offers **Download FFmpeg** (which points you to download information) and **Locate existing installation**. You can also set its folder in **Preferences > Video**. See [Installing FFmpeg](Installing-FFmpeg). Each selected part becomes its own video file.

## Not available yet

Exporting only a chosen time range, and dither and loudness options.
