# Dev diary

What we have done on FontDaw, newest first. One entry for a day with notable progress. We write what changed for a user, and what did not work or is still open. Nothing here is a promise.

An entry about features that are only in development builds names the development build it describes. Once there are releases, entries name the release's version number instead.

## 20 November 2026: a first version for real work

*Release candidate 0.1.1 RC3 (261120110), 20 November 2026.*

**A version to work in.** FontDaw now has a slow release branch next to the development one. Only tested changes go there, so a build from it can be used for real projects while new features are built elsewhere. Its first version, 0.1.1, is in testing and has been used for a full studio session.

**Your MuseScore files stay safe.** FontDaw no longer saves over a score that was last saved by MuseScore, or by a newer FontDaw. MuseScore does not open a file once FontDaw has saved it, so FontDaw explains why and offers to save a copy next to it instead, and the original still opens in MuseScore. To take work from FontDaw back to MuseScore, export it as MusicXML.

**Smaller things in 0.1.1.**

- The Tab key opens the command menu in the score, but not while you type text such as lyrics.
- The window always opens on the screen at a usable size. Quitting while in full screen could leave it invisible on the next start.
- Full screen is in the View menu and the Dock icon's menu on macOS.
- The mixer's level meters are drawn on the same scale as the faders, so 0 on a meter is level with 0 dB on its fader, and they turn yellow, orange and red by level.

Still open: a large mix can still clip on its way out of the program. A clip light and floating-point output are planned for the next version.

**What comes next.** A new file format for FontDaw's own projects, then finishing the features that are nearly done, such as exporting only a score's tempo map as a MIDI file to line up other programs with the score.

## 19 November 2026: audio in the score

*Development build 0.1.0, 19 November 2026.*

**Audio is now part of the score.** An audio track shows up in the score as an audio staff with its waveform, in every view and in print, and it plays in step with the notes. Insert or delete bars, and the audio moves with them. Besides WAV, you can now import FLAC, MP3, Ogg Vorbis and Opus files. Undo and redo cover audio edits too, in the same history as the score.

![A score for flute, violin and cello with a fourth staff below, labelled Groove, which shows a coloured waveform in each bar](images/2026-11-19-01-audio-staff.png "An audio recording sits in the score as its own staff, right under the instruments, with its waveform drawn bar by bar. You can see where the beats fall against the written notes, so audio and notation line up on the same page. Development build 0.1.0, 19 November 2026.")

![The same score during playback, with a blue vertical cursor in the third bar crossing all four staves, the audio staff included](images/2026-11-19-02-playback.png "The same score during playback. The playback cursor runs through the notated staves and the audio staff together, because the recording plays in time with the score. Development build 0.1.0, 19 November 2026.")

**Quicker commands.** A right-click opens a ring menu around the note. A command menu finds any command when you type its name.

![A round menu around a selected violin note, with eight symbols around it for articulations, ties, delete and more](images/2026-11-19-03-ring-menu.png "Right-clicking a note opens a ring of the most useful actions around the pointer: articulations, note values, ties, delete. Pick one without travelling to a toolbar. The labels will get icons and names soon. Development build 0.1.0, 19 November 2026.")

![A search box over the score with the word tie typed in, and a list of matching commands below it, the first being Tie with its keyboard shortcut](images/2026-11-19-04-command-menu.png "The command menu (Tools, then Command menu) finds any command by typing part of its name, here “tie”, and shows its keyboard shortcut next to it. Handy when you don't remember where something lives in the menus. Development build 0.1.0, 19 November 2026.")

*Because these pictures come from a development build, the top bar shows a DevTools tab and a Diagnostics menu that a released version will not have.*

**Newer foundations.** FontDaw now runs on MuseScore's newest code, including its new shortcut system. MuseScore's current key layout is the default.

**Fixes.**

- Solo is no longer saved as mutes.
- The mixer settings of audio tracks are saved.
- Missing mixer strips are back.
- VST instruments such as VOCALOID now follow the score when you start, stop or jump.
- Quitting after using plug-ins no longer crashes.
- Starting playback in a large score no longer crashes.

**Being designed.**

- A new way of selecting, with a soft glow that shows what a click will act on.
- Articulation maps for sample libraries, so you need no keyswitch notes in the score.
- A Time view where one second always takes the same width.
- Dark score pages, with a separate Print view that shows the printed colours.

**What comes next.** Audio tracks as parts of their own, then moving clips directly in the score.

## 16 November 2026: the first week

**The goal.** FontDaw is meant to become a notation-first DAW. The score stays at the centre of the project. Around it we want audio tracks, recording and mixing, so that you can write music in standard notation and record, arrange and mix audio next to it in one program. Many DAWs treat notation as an extra. Many notation programs treat audio as an extra. We want to start from the score.

**The groundwork.** So far the work is plain groundwork, and not much of it is visible yet:

- The project builds, and its automated tests run and pass.
- A project can now hold audio files on tracks of their own.
- A first audio file plays in time with the score. Use File > Import audio track to put a WAV file on its own track. It has a mixer strip and is included in the exported mixdown. It always starts at the beginning of the score, and you cannot edit it yet.
- Exporting each part of a score to its own audio file got faster, because the parts are now rendered in one pass. On Linux with four cores, a 12-part demo score took 46 seconds before and about 11 seconds now (exported to MP3 with the basic SoundFont). The result is close to the old export but not identical.
- The logos you see are placeholders. They mark the intent, and proper logos will replace them later.

**What comes next.** A plan is written and work has not started. First a timeline that shows the score's parts and the audio tracks side by side, then editing audio clips on it with undo, then more audio formats and safer saving of large projects. After that we expect beat matching and recording. [Status and roadmap](Status-and-roadmap) has the order.
