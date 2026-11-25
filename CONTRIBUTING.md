# Contributing to FontDaw

Pull requests are welcome: fixes, features, translations and documentation alike.

The one requirement: **before you open a pull request, check that what your change touches still
works.** Build FontDaw with your change, try the functions it affects, and say in the pull request
what you tried and on which system (macOS, Windows or Linux). A short note is enough, for example
"Exported a score with an audio track to WAV on Windows 11; it plays in sync."

A few things that make a pull request easier to accept:

- Keep it to one subject. Several small pull requests are easier to review than one large one.
- Keep the difference from MuseScore Studio's code small where you can, so that MuseScore's own
  updates can still be merged into FontDaw.
- If the change shows to users, describe what they now see or can do.
- Bugs and ideas are welcome as issues too, with steps to reproduce a bug if you have them.

FontDaw is free software under the GNU General Public License version 3, like MuseScore Studio.
By contributing you agree that your contribution is published under the same licence. There is no
separate contributor licence agreement to sign.

The source code is in the `app/` folder; building it works as for MuseScore Studio.
