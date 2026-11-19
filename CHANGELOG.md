# Changelog

Every release lists what changed here, newest first. The section of a version is its release notes: `tools/release_notes.sh` prints it and `tools/dist.sh` refuses to package a version that has none. The version lives in `VERSION`; the app, `PARAM.SFO`, the package art and the file names (`hometidy_ps3_v<version>.pkg`) all read it, and the tag is `v<version>`.

## v1.1.0
- New: **Update covers**, a third card on the first screen, for PS3, PS2 and PS1 games. It connects to the cover repositories (libretro-thumbnails on GitHub, one per platform) and, for each game, checks whether a cover exists next to the game image. If it does, its git id (SHA-1 of the file) is compared with the id the repository lists; when they differ the repository image overwrites the local one. A missing cover is downloaded.
- A progress screen shows the image being downloaded with its own bar (bytes), a bar for all games, and counters (new, replaced, already up to date, kept, not in the repository, failed). The stop button cancels.
- Safe by construction: HTTPS only with the certificate verified against roots built into the app, every download is checked against the id announced by the repository before it is written (a `.part` file is renamed over the cover), covers marked as completed are kept, and a list the repository cut short is refused.
- The first screen is now three cards stacked, to fit the third one.
- Checked against the live repositories from a computer (the same download code, a game of each platform: added, replaced, matched a name with a `[SERIAL]` tag, second run all up to date). Not run on a console yet: the PS3 network stack (libcurl and mbedTLS from the ps3dev portlibs) was only built, not run. If the app says a list could not be read, check the console date and time and the network.

## v1.0.3
- Tomb Raider is ordered by release date: Tomb Raider, II, III, The Last Revelation, Chronicles, then The Angel of Darkness, Legend, Anniversary, Underworld and Trilogy. Before, The Last Revelation (1999) came before II (1997) and III (1998): a title with no number in its name was treated as an unlisted title and went ahead of the numbered ones.
- The same defect in two more series of the PS1 list is fixed: Spyro (Spyro the Dragon, Spyro 2, Year of the Dragon, which came last-to-first) and Mortal Kombat (Trilogy, 1996, before Mythologies: Sub-Zero, 1997).
- Not run on a console in the release checks: the host tests, the package build and a run of the sorter on the real game list of the owner pass.

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
