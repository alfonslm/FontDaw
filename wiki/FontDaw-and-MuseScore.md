# FontDaw and MuseScore

FontDaw is based on MuseScore Studio. This page says what is the same, what we removed and why, how files work between the two, and who made what.

## What is the same

The notation side is MuseScore Studio's: note input, engraving, parts, printing, import and export of other score formats, the mixer, SoundFont playback and VST3 plug-ins. If you know MuseScore Studio, the score window will look familiar. We try to keep changes to that code small, so that improvements made in MuseScore can still be brought into FontDaw.

## What is different

FontDaw does not use Muse Group's online services or Muse Sounds. It needs no account and makes no network connections of its own (see [Privacy](Privacy)). It plays with SoundFonts and VST3 instruments instead.

## Files and formats

FontDaw saves scores as `.mscz`, MuseScore's format, and puts its own data (audio and the track layout) inside the file. FontDaw opens MuseScore files.

The other direction is not guaranteed. MuseScore Studio may ignore or drop the audio data in a FontDaw file, so do not rely on it to open a FontDaw project completely. If you need to hand a score to a MuseScore user, send an export such as MusicXML.

We plan a format of FontDaw's own later, so that this is clear from the file name.

## Using both programs

FontDaw is designed to use its own settings, its own documents folder and its own program name, so that MuseScore Studio's settings, recent files and plug-in lists stay as they are. Having both installed on one computer has not been tested yet.

## Credit and licence

FontDaw is based on MuseScore Studio by MuseScore Limited and contributors. FontDaw is not affiliated with or endorsed by MuseScore Limited or Muse Group. FontDaw is free software under the GNU General Public License version 3, like MuseScore Studio. More in [Licence and credits](Licence-and-credits).
