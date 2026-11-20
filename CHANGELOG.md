# Changelog

Every release lists what changed here, newest first. The section of a version is its release notes: `tools/release_notes.sh` prints it and `tools/dist.sh` refuses to package a version that has none. The version lives in `VERSION`; the app, `PARAM.SFO`, the package art and the file names (`hometidy_ps3_v<version>.pkg`) all read it, and the tag is `v<version>`.

## v1.3.0
- Update covers no longer wipes the completed mark. A cover that is the repository picture with only the mark added used to differ from what the app had written, so it was downloaded and overwritten and the green check disappeared on every run. Now, when the local cover is marked, the app compares it with the new framed picture everywhere except under the badge square (fully transparent pixels count as equal, each channel may differ by 2). Same picture: the cover is kept, counted as "same", and recorded in `covers.db`, so the next run does not even download it. A marked cover of a different picture is still replaced, as in v1.2.0.
- Second fallback for covers that nobody else has (the case was *Sorcery*, `BCUS98203`: not in the libretro repository, not on GameTDB). When the repository, its other regions and GameTDB all miss, the app searches [TheGamesDB](https://thegamesdb.net) by game name on the platform's list (PS3, PS2 or PS1), takes the card whose title is exactly the game's (NTSC-U first, then PAL, then any), and downloads its 300 px box art from `cdn.thegamesdb.net` (the image host is checked; HEAD with ETag and size avoids a second download). Checked on the real site: the search for Sorcery returns the NTSC-U cover 12670.
- Not run on the console yet.

## v1.2.2
- Crash Bandicoot order: *Crash Bandicoot: Warped* came before *Crash Bandicoot 2*. The name has no number, so it was treated as an unlisted title and went ahead of the numbered ones (the same defect as Spyro and Tomb Raider). The series now has dated rules: Crash Bandicoot (1996), 2 (1997), Warped / 3 (1998) and Crash Bash (2000). Crash Team Racing keeps its place after them.
- The cover of Warped was downloaded but webMAN showed its default icon. Cause found by reading the game list on the console: webMAN writes a placeholder icon (`/dev_hdd0/tmp/wm_icons/icon_wm_psx.png`) for a game that has no cover when it builds `mygames.xml`, and keeps it until the next rebuild. Warped was added to the console, webMAN built its entry, and only then the app downloaded the cover. After Update covers the app now checks the game list and links the cover (`<game name>.png`, `.jpg` or `.jpeg` next to the game image) into every entry that still has a placeholder icon. Only placeholder icons of games are changed; a backup of the list is saved first, the write is read back, and the order list is not touched. The summary screen says how many icons were linked. Close the app and open the game column again to see them.
- Checked on the real game list of the console: exactly one icon (Warped) is relinked, and the Crash order comes out right. Not run on the console yet.

## v1.2.1
Update covers no longer downloads what is already here.
- PS3 covers from the fallback server (GameTDB) were fetched again on every run, only to compare them. The server lists no ids, so the app now sends a HEAD request (headers only, no image) and compares the ETag and size with the ones recorded when it wrote the cover. Same image on the server and same file here: nothing is downloaded. A changed ETag fetches the image again.
- A symbolic link of the PS1 and PS2 repositories was read on every run to learn which image it points to. A blob id fixes the content of a link, so the answer is recorded and the link is not read again.
- A cover that is byte for byte the repository image (what v1.1.0 saved, unshrunk) is shrunk in place instead of being downloaded again.
- Measured from a computer on the owner's 177 games with the real repositories: a run where everything is current takes about 3 seconds and downloads no image (before, about 50 seconds). The first run after this version also records the new data, with no image fetched.
- `covers.db` has two more kinds of lines: `gametdb:<etag>|<size>` as the remote id of a fallback cover, and `@<link id>` lines for resolved links. An older `covers.db` keeps working.

## v1.2.0
Found by running v1.1.0 on a console and reading the files it wrote over FTP.
- Covers that did not show in the XMB: the PS1 and PS2 repositories store a repeated image as a symbolic link, and a link is a 26-byte text file holding the name of the real image. v1.1.0 saved that text as the `.png`. Links are now followed to the real image. The 26-byte files v1.1.0 left are replaced by the next update.
- Downloaded covers were far larger than the ones on the console (up to about 800 KB against 160 x 160 and about 50 KB). Each download is now shrunk to a 160 x 160 transparent frame, aspect kept, the same frame as the covers already in the library, and written over the old file.
- No PS3 cover was downloaded: the PS3 repository has few images and names them without the language list, so `Puppeteer (USA) (En,Fr,Es,Pt)` never matched. Names are now also tried without the language list, version, `(Rev N)` and with `(USA, Canada)` cut to `(USA)`, and a title found only in another region is used as a last try.
- New fallback server for PS3: when the repository has no cover, GameTDB is asked by the title id read from `PS3_GAME/PARAM.SFO` inside the `.iso` (several regions are tried). Run on the owner's list of 177 games from a computer: 175 got a cover (29 from GameTDB); the 2 without one are Sorcery (GameTDB has none) and one game whose title id could not be read over FTP.
- A cover with the completed mark is now overwritten when it differs, like any other. What is compared: the repository image id against the one recorded when the app last wrote that cover (`/dev_hdd0/hometidy/covers.db`), and the local file against what was written. A cover edited afterwards, a mark included, differs and is replaced; run Mark as completed again after updating. The old behavior (keep marked covers) is gone.
- The app is in US English only; the Portuguese texts and the language switch (Square) are removed.
- The update screen shows "Checking <game>" while it compares and counts the covers that came from the fallback server.
- Fixed the log lines that printed a literal `zu` (the console's printf has no `%zu`).
- Checked against the real GitHub repositories and GameTDB from a computer with the owner's game names; not run on a console yet after these changes (v1.1.0 did run: its log showed 129 replaced and 43 not found, with no network errors).

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
