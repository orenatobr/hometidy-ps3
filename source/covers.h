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
struct Remote {
    std::string name;
    std::string sha;
};

/// Index of the ".png" files of a folder tree, by lowercase name without the extension.
std::map<std::string, Remote> indexImages(const std::vector<TreeItem>& items);

/// The names a game may have in the repository, best first. The thumbnail repositories write "_" for
/// the characters & * / : ` < > ? \ | and name the files like the No-Intro dumps, so a name with a
/// "[BLUS12345]" tag is also tried without it.
std::vector<std::string> candidateNames(const std::string& gameName);

/// Finds the image of a game in an index (nothing when the repository has none).
const Remote* find(const std::map<std::string, Remote>& index, const std::string& gameName);

/// Address of an image on the raw file server.
std::string imageUrl(const Source& s, const std::string& remoteName);

/// Address of the tree API answer for a ref (a branch name or a tree id).
std::string treeUrl(const Source& s, const std::string& ref);

}  // namespace covers
