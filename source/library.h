// The ISO library on the console: games and the cover image next to each one.
#pragma once

#include <string>
#include <vector>

namespace library {

enum class Platform { Ps3, Ps2, Psx };

/// A folder with game images ("/dev_hdd0/PS3ISO") and the platform of the games in it.
struct Root {
    std::string path;
    Platform platform = Platform::Ps3;
};

/// One game image and its cover.
struct Game {
    std::string name;    ///< File name without extension, e.g. "Alpha Protocol (USA) (En,Fr)".
    std::string file;    ///< File name of the game image, e.g. "Alpha Protocol (USA) (En,Fr).iso".
    std::string title;   ///< Name to show: tags removed, " - " turned into ": ".
    std::string image;   ///< Path of the .png/.jpg cover, empty when the game has none.
    std::string dir;     ///< Folder of the game image (and where its cover belongs).
    Platform platform = Platform::Ps3;
};

/// Short label ("PS3", "PS2", "PS1").
const char* label(Platform p);

/// Lists the games of each root.
///
/// A game is a .iso/.bin/.img/.mdf file. Its cover is the image with the same
/// name (.png, .jpg or .jpeg, any letter case), or `<file name>.png/.jpg`.
/// Hidden macOS files ("._*") and extra "(Track NN)" files are ignored.
///
/// Args:
///     roots: Folders to read; missing folders are skipped.
///
/// Returns:
///     Games in the order of the roots, then by file name.
std::vector<Game> scan(const std::vector<Root>& roots);

}  // namespace library
