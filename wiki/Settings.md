# Settings

Where to change how FontDaw sounds and looks. Open the **Preferences** dialog. Its pages are General, Appearance, Canvas, Save & publish, Note input, Score, Audio & MIDI, MIDI mappings, Video, Percussion, Import, Shortcuts, Folders, Advanced and Braille. Settings are inherited from MuseScore Studio unless said otherwise.

## Audio and MIDI

- **Audio driver.** Shown only if your computer offers more than one. Then choose the **Audio device**.
- **Buffer size** and, on Linux only, **Sample rate**. The driver decides which values are offered.
- **Output latency.** When FontDaw can detect your device's output latency, it says so and lines the playback cursor up with the sound. If the cursor still looks early or late, use **Additional output latency for this device** (from -500 to 2000 ms). **Display latency** (0 to 500 ms) delays the on-screen cursor.
- **MIDI input** and **MIDI output**, and a tick box for MIDI 2.0 output if the receiver supports it.
- **Playback quality:** a tick box for a warmer instrument sound (basic SoundFont only).
- **Mixer:** a tick box that ties track mute to showing and hiding instruments in the Layout panel.

*Stub: advice on choosing a buffer size for your system.*

## Appearance

- **Theme:** light or dark, or follow the system theme, plus a high-contrast option.
- **Colours** for accents, text and borders.
- **App icon:** **Dark** or **Light**. The tick box **Follow system colour scheme** is on by default, so the icon follows your system. If FontDaw cannot read the system's scheme, it uses the icon you chose. The icons are placeholders for now.
- **Page:** invert score colours, optionally only in the dark theme.
- **Page colour and background**, including a background image.
- **App text:** typeface and size.
- **Reset appearance to default.**

## Video

The **Video converter** section shows whether FFmpeg was found and its version. **Locate existing installation** lets you choose its folder. See [Installing FFmpeg](Installing-FFmpeg).

## Resetting

**Reset preferences**, at the bottom left of the dialog, resets all app preferences and deletes your custom shortcuts. It does not delete scores.

*Stub: exactly which settings survive a reset has not been checked.*
