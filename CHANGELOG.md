# Changelog

Every release lists what changed here, newest first. The section of a version is its release notes: `tools/release_notes.sh` prints it and `tools/dist.sh` refuses to package a version that has none. The version lives in `VERSION`; the app, `PARAM.SFO`, the package art and the file names (`hometidy_ps3_v<version>.pkg`) all read it, and the tag is `v<version>`.

## v1.0.2
- Final Fantasy Tactics (the PS1 game) is no longer ordered with the numbered Final Fantasy games by release date. It is a title of its own and follows the alphabetical order of the series names: it now sits right after Final Fantasy XIV instead of between VII and VIII.
- The release files are named `hometidy_ps3_v<version>.pkg` and `.self` (for example `hometidy_ps3_v1.0.2.pkg`) instead of `HomeTidy.pkg`.
- The version is read from one place, the `VERSION` file. The app shows it at the bottom right of the screen, `PARAM.SFO` carries it (`01.02` for 1.0.2, the second number is minor * 10 + patch), and the XMB icon and background of the package now show it too; they used to show no version at all.
- This changelog is the source of each release's notes.

## v1.0.1
- Faster selection on the Mark as completed screen. Covers decode one per frame (the selected one first), the rows just off screen are prepared ahead, and covers are scaled once to the tile size and copied 1:1 instead of being resampled on every redraw. A new row can show blank tiles for a few frames while its covers load.
- Nothing changed in how files are written: the game list backup and the first original of each marked cover are kept as before.

## v1.0.0
- First release: **Sort game list** (A to Z, games of one series by release date, with `series.txt` to override the table) and **Mark as completed** (a green check on the cover, removable), run from the console with the controller. Backups before every write and a read-back after it.
