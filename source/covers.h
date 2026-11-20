// Where the cover images come from and how a game name finds its cover there.
//
// The sources are the libretro thumbnail repositories on GitHub: one per platform, with the box art in
// "Named_Boxarts/<game name>.png". GitHub reports the git blob id of every file in a tree, which is what a
// local file is compared with (sha1.h).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "library.h"

namespace covers {

/// One cover repository.
struct Source {
    library::Platform platform;
    const char* repo;    ///< "owner/name".
    const char* branch;
    const char* folder;  ///< Folder with the images, at the top of the repository.
};

/// The repository of a platform.
const Source& sourceFor(library::Platform p);

/// One entry of a GitHub tree.
struct TreeItem {
    std::string path;
    std::string type;  ///< "blob" or "tree".
    std::string mode;  ///< "100644" regular file, "120000" symbolic link, "040000" folder.
    std::string sha;
};

/// Reads the "tree" array of a GitHub git/trees answer.
///
/// Args:
///     json: The response body.
///     out: Receives the entries (cleared first).
///
/// Returns:
///     False when the text is not such an answer or the answer says it was cut short ("truncated": true).
bool parseTree(const std::string& json, std::vector<TreeItem>& out);

/// Id of the entry called `name` (a "tree" entry for a folder), or empty.
std::string findSha(const std::vector<TreeItem>& items, const std::string& name, const char* type);

/// A remote image: the file name without ".png" as it is in the repository, and its blob id.
/// The repositories keep one copy of an image that several names share and make the other names symbolic
/// links; the content of a link is the name of the file it points to (a few bytes, not a picture).
struct Remote {
    std::string name;
    std::string sha;
    bool symlink = false;
};

/// The ".png" entries of a folder tree.
struct Index {
    std::map<std::string, Remote> exact;                         ///< Lowercase name -> image.
    std::map<std::string, std::vector<std::string>> loose;       ///< Title key (see looseKey) -> lowercase names.
};

/// Builds the index of a folder tree (regular files and links; anything else is skipped).
Index indexImages(const std::vector<TreeItem>& items);

/// The names a game may have in the repository, best first. The thumbnail repositories write "_" for
/// the characters & * / : ` < > ? \ | and name the files like the Redump dumps, so after the full name come:
/// without a "[BLUS12345]" tag, without the language list "(En,Fr)" and version "(v02.00)", with the region
/// list cut to its first region ("(USA, Canada)" -> "(USA)"), and without "(Rev N)".
std::vector<std::string> candidateNames(const std::string& gameName);

/// Finds the image of a game by one of its candidate names (nothing when the repository has none).
const Remote* find(const Index& index, const std::string& gameName);

/// Finds the same title under another region or revision ("Tales of Graces f (USA)" -> "(Japan)"), preferring
/// USA, then World, then Europe. The title must match whole (letters and digits only), and so must the disc.
const Remote* findLoose(const Index& index, const std::string& gameName);

/// Finds an image by the name a symbolic link points to ("Some Game (USA).png").
const Remote* findTarget(const Index& index, const std::string& fileName);

/// The text a link blob holds, when it is a plain file name of this folder ("X.png"); empty otherwise.
std::string linkTarget(const std::string& blobText);

/// Region folders of GameTDB to try for a PS3 title id, best first ("BLUS30341" -> US, then EN, JA).
std::vector<std::string> gametdbRegions(const std::string& titleId);

/// GameTDB cover of a PS3 title id (the fallback server, used when the repository has no cover for a game).
std::string gametdbUrl(const std::string& titleId, const std::string& region);

/// Second fallback, for a game neither the repository nor GameTDB has (PS3 "Sorcery"): TheGamesDB, found by
/// name. The search page of a game name (tags like "(USA)" and "[BCUS98203]" dropped), on the platform's list.
std::string tgdbSearchUrl(library::Platform p, const std::string& gameName);

/// Reads a TheGamesDB search page and returns the cover thumbnail of the game whose title is the game's
/// (case and punctuation ignored); NTSC-U first, then PAL, then any region. Empty when there is none.
std::string tgdbCoverUrl(const std::string& html, const std::string& gameName);

/// Address of an image on the raw file server.
std::string imageUrl(const Source& s, const std::string& remoteName);

/// Address of the tree API answer for a ref (a branch name or a tree id).
std::string treeUrl(const Source& s, const std::string& ref);

}  // namespace covers
