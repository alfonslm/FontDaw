# Dev diary

What we have done on FontDaw, newest first. One entry for a day with notable progress. We write what changed for a user, and what did not work or is still open. Nothing here is a promise.

## 16 November 2026: the first week

**The goal.** FontDaw is meant to become a notation-first DAW. The score stays at the centre of the project. Around it we want audio tracks, recording and mixing, so that you can write music in standard notation and record, arrange and mix audio next to it in one program. Many DAWs treat notation as an extra. Many notation programs treat audio as an extra. We want to start from the score.

**The groundwork.** So far the work is plain groundwork, and not much of it is visible yet:

- The project builds, and its automated tests run and pass.
- A project can now hold audio files on tracks of their own.
- A first audio file plays in time with the score. Use File > Import audio track to put a WAV file on its own track. It has a mixer strip and is included in the exported mixdown. It always starts at the beginning of the score, and you cannot edit it yet.
- Exporting each part of a score to its own audio file got faster, because the parts are now rendered in one pass. On Linux with four cores, a 12-part demo score took 46 seconds before and about 11 seconds now (exported to MP3 with the basic SoundFont). The result is close to the old export but not identical.
- The logos you see are placeholders. They mark the intent, and proper logos will replace them later.

**What comes next.** A plan is written and work has not started. First a timeline that shows the score's parts and the audio tracks side by side, then editing audio clips on it with undo, then more audio formats and safer saving of large projects. After that we expect beat matching and recording. [Status and roadmap](Status-and-roadmap) has the order.
