// Brings the cover images next to the game images up to date from the cover repositories (covers.h).
//
// For each game the repository image is looked up by the game name; for a PS3 game the repository does not
// have, the cover comes from the fallback server (GameTDB) by the title id read from the disc image. A game
// that has no cover anywhere else is looked for by name on TheGamesDB, the second fallback. A
// download is shrunk to the frame the game list uses (frame.h) and written over the local cover.
//
// What is compared is the git blob id the repository lists for the image against the one recorded when the
// local cover was last written (a small database, since the local file is a shrunk copy and its own id is
// not the repository's). A local cover that no longer matches what was written (another tool) or that
// was never written by this app counts as different and is overwritten, a cover marked as completed
// included, unless it is the same picture with only the mark added (the mark stays).
// Every download is checked against the announced id before it is used.
//
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
    Checking,      ///< Comparing the cover of a game (nothing is transferred).
    Downloading,   ///< Fetching one image.
    Finished,
};

struct Counts {
    int added = 0;     ///< No local cover: downloaded.
    int replaced = 0;  ///< Local cover differed: overwritten.
    int same = 0;      ///< Local cover already matched.
    int missing = 0;   ///< Neither the repository nor the fallback server has a cover for the game.
    int failed = 0;    ///< Download or write failed.
    int fallback = 0;  ///< Of added and replaced: covers that came from the fallback server.
};

/// What the screen needs to draw.
struct Progress {
    Phase phase = Phase::Starting;
    library::Platform platform = library::Platform::Ps3;  ///< Repository being read or game being fetched.
    std::string file;       ///< The image being downloaded.
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
    ///     games: Games to look at (their `dir`, `name`, `file`, `image` and `platform` are used).
    ///     dbPath: Where the record of written covers is kept (empty: not kept).
    Sync(http::Client& client, std::vector<library::Game> games, std::string dbPath = "");

    /// Advances the work a little. Cheap to call every frame.
    void step();

    /// Stops after the transfer in progress is dropped.
    void cancel();

    bool done() const { return progress_.phase == Phase::Finished; }
    const Progress& progress() const { return progress_; }

private:
    enum class Stage { Next, RootTree, FolderTree, Game, Link, Download, Search, FallbackHead, Fallback, End };
    struct Written {
        std::string remote;  ///< Repository blob id the local cover was made from.
        std::string local;   ///< Blob id of the local file as written.
    };

    void nextPlatform();
    void pollIndex(bool folder);
    void nextGame();
    void tryRemote(const covers::Remote& remote);
    void startDownload(const covers::Remote& remote);
    void pollLink();
    void pollDownload();
    void pollSearch();
    void pollFallbackHead();
    void pollFallback();
    void useLocalAsSource();
    void startFallback();
    void afterFallbackMiss();
    void noCover();
    std::string fallbackUrl() const;
    void store(const std::vector<std::uint8_t>& framed, const std::string& remoteSha, bool fromFallback);
    bool isMarkedCopyOf(const std::vector<std::uint8_t>& framed) const;
    void buildQueue();
    void indexFailed(const std::string& why);
    void failGame(const std::string& why);
    void finishGame();
    void finish();
    void loadDb();
    void saveDb() const;
    std::string readLocal() const;

    http::Client& client_;
    std::vector<library::Game> games_;
    std::string dbPath_;
    std::map<std::string, Written> db_;
    std::map<std::string, std::string> links_;  ///< Link blob id -> the image it points to (a blob id fixes its content).
    std::vector<library::Platform> platforms_;
    std::size_t platformPos_ = 0;
    Stage stage_ = Stage::Next;
    Progress progress_;

    covers::Index index_;
    std::vector<std::size_t> queue_;  ///< Games of the platform being worked on.
    std::size_t queuePos_ = 0;

    // the game being worked on
    const library::Game* game_ = nullptr;
    std::string target_;           ///< File being written.
    std::string localSha_;         ///< Blob id of the local cover now (empty when there is none).
    covers::Remote remote_;        ///< Image of the repository being fetched.
    int hops_ = 0;                 ///< Symbolic links followed for this game.
    std::string titleId_;          ///< PS3 title id for the fallback server.
    std::vector<std::string> regions_;
    std::size_t regionPos_ = 0;
    std::string searchedUrl_;      ///< Cover found by name on the second fallback server (empty: not asked or none).
    bool searched_ = false;        ///< The second fallback was already asked for this game.
    std::string validator_;        ///< What the fallback server says identifies the image now (ETag and size).
};

}  // namespace coversync
