# HomeTidy for PS3

A PS3 homebrew app for consoles with custom firmware and webMAN MOD. It does three chores you would otherwise do over FTP:

1. **Sort game list**: puts the webMAN game list (`mygames.xml`) in order, A to Z. Games of one series go by release date (see [Series table](#series-table)).
2. **Mark as completed**: draws a green check on the cover of a game you finished. You can remove it later.
3. **Update covers**: compares the cover next to each PS3, PS2 and PS1 game with the cover repository and downloads the ones that are missing or different, with a progress bar that names the image being downloaded.

It runs on the console, with the controller. No computer needed. There is a sibling project for the Wii U: [hometidy-wiiu](https://github.com/orenatobr/hometidy-wiiu).

> **Status:** built and tested on a desktop (every screen is rendered by a host preview, the sort and the green check run against real files). **It has not been run on a real PS3 yet.** Back up your game list first; the app also saves its own backups (see below).

## Install

1. Download `hometidy_ps3_v<version>.pkg` (for example `hometidy_ps3_v1.0.2.pkg`) from the [latest release](../../releases/latest). What changed is in the release notes and in [CHANGELOG.md](CHANGELOG.md).
2. Copy it to a USB drive (or to `/dev_hdd0/packages` over FTP) and install it from the XMB: **Game > Package Manager > Install Package Files** (or **Game > Install Package Files**, depending on your firmware).
3. Start **HomeTidy** from the Game column. You can also run `hometidy_ps3_v<version>.self` from a file manager such as multiMAN.

Needs a console with custom firmware and webMAN MOD (written for the setup of the author: Evilnat 4.92.x). The language follows the console (Portuguese or English). Press **Square** on the first screen to switch.

## How it works

| Job | What it touches | Safety |
| --- | --- | --- |
| Sort game list | `/dev_hdd0/xmlhost/game_plugin/mygames.xml`, the game list webMAN shows in the XMB. Only the order of the `<I ... />` tags inside each platform segment (PS3, PS2, PS1) changes; titles, covers and mount paths stay as they are, so the file keeps exactly the same size. | Backup first to `/dev_hdd0/hometidy/backup/`, write, read back, and read again after 2 seconds (webMAN can rewrite the file). |
| Update covers | The cover image next to each game image in `PS3ISO`, `PS2ISO` or `PSXISO` (`<game name>.png`), and the Internet: [libretro-thumbnails](https://github.com/libretro-thumbnails) on GitHub, one repository per platform (`Sony_-_PlayStation_3`, `Sony_-_PlayStation_2`, `Sony_-_PlayStation`), folder `Named_Boxarts`. | The repository tells the git id (SHA-1 of the file) of every image. A local cover with the same id is left alone; a missing or different one is downloaded to `<game name>.png`, checked against the announced id, and only then written (through a `.part` file). A cover marked as completed is never overwritten, so the check survives. An old `.jpg` is not deleted; the new `.png` is the one used. Connections are HTTPS with the certificate checked. |
| Mark as completed | The cover image next to the game image, in `PS3ISO`, `PS2ISO` or `PSXISO` (`.png` or `.jpg`, same name as the `.iso`). Only the top-left corner of the cover changes. | The first original of each cover is saved to `backup/covers/`. Press the confirm button on a marked game to restore it. A check made by another tool has no saved original: its pixels are rebuilt from the surroundings (approximate). |

**Update covers** needs the console online (and the right date and time, because the certificate check uses them). It reads at most four short lists from the GitHub API (two for the PS1 and PS2 repositories are about 2.5 MB each, less compressed), then one file per image from `raw.githubusercontent.com`. A game is matched by its file name: `Radiata Stories (USA).iso` finds `Radiata Stories (USA).png`; characters the repositories write as `_` (`& * / : < > ? \\ |`) are matched, and a tag such as `[BLUS30443]` is ignored when the full name is not found. A game the repository does not have is counted as "not in the repository". Press the stop button to cancel; what was written stays.

The badge is the same one the `ps3tweaks` FTP cover marker draws (a green rounded square with a white check, about 15% of the cover), so covers marked by either tool are recognized by both.

webMAN rebuilds `mygames.xml` when you refresh its list, and that puts its own order back. Run **Sort game list** again after a refresh. Close the app and open the game column again to see the new order.

## Series table

Order inside a series follows release date, and a series sits in the list under its own name without the leading article (every *Ratchet & Clank* game goes under **Ratchet & Clank**, *The Last of Us* under **L**). The built-in table is in `source/default_rules.cpp` and comes from memory, so some months can be off. Titles that match no rule are grouped by the text before ":" or " - " and ordered by their trailing number, so unlisted sequels still sit together.

To change the table without rebuilding, create `/dev_hdd0/hometidy/series.txt`:

```text
# pattern | series | date    (first match wins, your lines win over the built-in ones)
resistance burning skies | resistance | 2012-05
my fan game | zzz fan games | 2020
```

Patterns are matched as whole words against the lowercase title without punctuation, tags such as `(USA) (En,Fr)` or a leading "The". The date is `YYYY` or `YYYY-MM`.

## Files on the console

```text
/dev_hdd0/hometidy/
  backup/mygames_<date>_<time>.xml      game list backups
  backup/covers/<hash>_<name>.png       original covers
  config.txt                            language (lang=en or lang=pt)
  cacert.pem                            trusted root certificates, written at each cover update
  series.txt                            optional, your series rules
  hometidy.log                          what this run did (flushed line by line)
  hometidy.prev.log                     the run before, kept in case of a crash
```

To undo a sort, copy a backup over `mygames.xml` (FTP is enough).

## Build

You need the [ps3dev](https://github.com/ps3dev/ps3dev) toolchain (the prebuilt release for macOS or Linux is fine) and Python 3 with Pillow, which draws the package icon and background with the version in them. The image and font code is vendored under `third_party/`.

The version is in the `VERSION` file (`1.0.2`) and nowhere else: the app, `PARAM.SFO`, the package art and the names of the release files are made from it. To release: put the new version in `VERSION` and its notes in a `## v<version>` section at the top of `CHANGELOG.md`, build, run `tools/dist.sh`, then `gh release create v<version> dist/* --target main --latest --title v<version> --notes-file <(tools/release_notes.sh)`.

```sh
export PS3DEV=$HOME/ps3dev/ps3dev PSL1GHT=$HOME/ps3dev/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin
make pkg          # HomeTidy.self, HomeTidy.pkg and HomeTidy.gnpdrm.pkg
tools/dist.sh     # dist/hometidy_ps3_v<version>.pkg (the finalized package), .self and SHA256SUMS
```

Build from a normal (APFS/ext4) folder, not from an exFAT card: macOS writes `._*` files there and breaks the build.

Host tests (no console needed, address and undefined-behaviour sanitizers on):

```sh
tests/run.sh
# optional: check a real game list (prints the new order, never writes)
HOMETIDY_REAL_XML=path/to/mygames.xml tests/run.sh
```

UI preview without a console (renders every screen into PNG files with the same canvas code the console uses):

```sh
tools/host_preview.sh /tmp/fake_console /tmp/shots   # needs Python 3 with Pillow for the fake files
```

## Layout of the code

| Path | What it is |
| --- | --- |
| `source/sorter.*`, `default_rules.*` | Title cleaning, series table, sort keys. |
| `source/mygames.*` | Reader and editor of the webMAN game list. |
| `source/library.*` | Finds the game images and their covers. |
| `source/badge.*` | Draws, detects and removes the green check on a PNG or JPEG. |
| `source/tasks.*` | The two jobs, with backups and read-back checks. |
| `source/gfx.*`, `app.*`, `texts.*` | Software canvas, screens, English and Portuguese texts. |
| `source/platform_ps3.*`, `main.cpp` | Console only: RSX frame buffer, controllers, system events. |
| `host/` | Desktop preview and platform layer for the tests. |
| `VERSION`, `CHANGELOG.md`, `tools/` | The release version, the notes of each release, and the scripts that prepare the package (`prepare.sh`, `make_art.py`), pack the release files (`dist.sh`) and print the notes (`release_notes.sh`). |

## Credits

Libraries: [PSL1GHT](https://github.com/ps3dev/PSL1GHT) and the ps3dev toolchain, [stb](https://github.com/nothings/stb) (public domain: image decode/encode and TrueType text), [Inter](https://github.com/rsms/inter) (SIL OFL 1.1). Licensed under GPL-2.0. Not affiliated with Sony Interactive Entertainment.
