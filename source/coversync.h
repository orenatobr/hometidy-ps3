// Brings the cover images next to the game images up to date from the cover repositories (covers.h).
//
// For each game the cover of the repository is compared with the local one by git blob id. A missing
// cover is downloaded; a different one is replaced; a cover marked as completed is kept, so the mark
// is not lost. Every download is checked against the id the repository announced before it is written.
// The sync is a state machine: call step() once per frame and draw progress().
#pragma once

#include <map>
#include <string>
#include <vector>

#include "covers.h"
#include "http.h"
#include "library.h"

namespace coversync {

enum class Phase {
    Starting,
    ReadingIndex,  ///< Fetching the list of covers of a repository.
    Downloading,   ///< Fetching one image.
    Finished,
};

struct Counts {
    int added = 0;     ///< No local cover: downloaded.
    int replaced = 0;  ///< Local cover differed: overwritten.
    int same = 0;      ///< Local cover already matched.
    int marked = 0;    ///< Kept because it carries the completed mark.
    int missing = 0;   ///< The repository has no cover for the game.
    int failed = 0;    ///< Download or write failed.
};

/// What the screen needs to draw.
struct Progress {
    Phase phase = Phase::Starting;
    library::Platform platform = library::Platform::Ps3;  ///< Repository being read or game being fetched.
    std::string file;       ///< The image being downloaded ("<name>.png").
    int index = 0;          ///< Images handled so far (of `total`).
    int total = 0;          ///< Games to look at.
    long long bytes = 0;    ///< Of the current transfer.
    long long bytesTotal = -1;
    Counts counts;
    int indexErrors = 0;    ///< Repositories that could not be read.
    std::string lastError;  ///< Last problem, for the log and the end screen.
    bool cancelled = false;
};

class Sync {
public:
    /// Args:
    ///     client: HTTP client; must outlive the sync.
    ///     games: Games to look at (their `dir`, `name`, `image` and `platform` are used).
    Sync(http::Client& client, std::vector<library::Game> games);

    /// Advances the work a little. Cheap to call every frame.
    void step();

    /// Stops after the transfer in progress is dropped.
    void cancel();

    bool done() const { return progress_.phase == Phase::Finished; }
    const Progress& progress() const { return progress_; }

private:
    enum class Stage { Next, RootTree, FolderTree, Game, Download, End };

    void nextPlatform();
    void pollIndex(bool folder);
    void nextGame();
    void pollDownload();
    void indexFailed(const std::string& why);
    void finishGame();

    http::Client& client_;
    std::vector<library::Game> games_;
    std::vector<library::Platform> platforms_;
    std::size_t platformPos_ = 0;
    Stage stage_ = Stage::Next;
    Progress progress_;

    std::map<std::string, covers::Remote> index_;
    std::vector<std::size_t> queue_;  ///< Games of the platform being worked on.
    std::size_t queuePos_ = 0;
    std::string target_;     ///< File being written.
    std::string expected_;   ///< Blob id of the image being fetched.
    bool replacing_ = false;
};

}  // namespace coversync
