#include "coversync.h"

#include <algorithm>

#include "fsx.h"
#include "sha1.h"
#include "tasks.h"

namespace coversync {
namespace {

constexpr std::size_t kMaxIndexBytes = 16u * 1024 * 1024;  // the PS1 and PS2 folder lists are about 2.5 MB
constexpr std::size_t kMaxImageBytes = 8u * 1024 * 1024;

}  // namespace

Sync::Sync(http::Client& client, std::vector<library::Game> games) : client_(client), games_(std::move(games)) {
    for (const library::Platform p : {library::Platform::Ps3, library::Platform::Ps2, library::Platform::Psx}) {
        if (std::any_of(games_.begin(), games_.end(), [&](const library::Game& g) { return g.platform == p; })) platforms_.push_back(p);
    }
    progress_.total = static_cast<int>(games_.size());
}

void Sync::step() {
    switch (stage_) {
        case Stage::Next: nextPlatform(); break;
        case Stage::RootTree: pollIndex(false); break;
        case Stage::FolderTree: pollIndex(true); break;
        case Stage::Game: nextGame(); break;
        case Stage::Download: pollDownload(); break;
        case Stage::End: progress_.phase = Phase::Finished; break;
    }
}

void Sync::cancel() {
    if (done()) return;
    client_.cancel();
    progress_.cancelled = true;
    stage_ = Stage::End;
    progress_.phase = Phase::Finished;
}

void Sync::nextPlatform() {
    if (platformPos_ >= platforms_.size()) {
        stage_ = Stage::End;
        progress_.phase = Phase::Finished;
        return;
    }
    const covers::Source& src = covers::sourceFor(platforms_[platformPos_]);
    progress_.platform = platforms_[platformPos_];
    progress_.phase = Phase::ReadingIndex;
    progress_.file.clear();
    progress_.bytes = 0;
    progress_.bytesTotal = -1;
    index_.clear();
    client_.start(covers::treeUrl(src, src.branch), kMaxIndexBytes);
    stage_ = Stage::RootTree;
}

void Sync::indexFailed(const std::string& why) {
    ++progress_.indexErrors;
    progress_.lastError = why;
    const int games = static_cast<int>(std::count_if(games_.begin(), games_.end(), [&](const library::Game& g) {
        return g.platform == platforms_[platformPos_];
    }));
    progress_.counts.failed += games;
    progress_.index += games;
    ++platformPos_;
    stage_ = Stage::Next;
}

void Sync::pollIndex(bool folder) {
    const http::State st = client_.step();
    const http::Progress pr = client_.progress();
    progress_.bytes = pr.done;
    progress_.bytesTotal = pr.total;
    if (st == http::State::Running || st == http::State::Idle) return;
    if (st == http::State::Failed) return indexFailed(client_.error());
    if (client_.status() != 200) return indexFailed("HTTP " + std::to_string(client_.status()));

    const std::vector<std::uint8_t> body = client_.take();
    std::vector<covers::TreeItem> items;
    if (!covers::parseTree(std::string(body.begin(), body.end()), items)) return indexFailed("unreadable cover list");
    const covers::Source& src = covers::sourceFor(platforms_[platformPos_]);

    if (!folder) {
        const std::string sha = covers::findSha(items, src.folder, "tree");
        if (sha.empty()) return indexFailed("cover folder not found");
        progress_.bytes = 0;
        progress_.bytesTotal = -1;
        client_.start(covers::treeUrl(src, sha), kMaxIndexBytes);
        stage_ = Stage::FolderTree;
        return;
    }
    index_ = covers::indexImages(items);
    queue_.clear();
    for (std::size_t i = 0; i < games_.size(); ++i) {
        if (games_[i].platform == platforms_[platformPos_]) queue_.push_back(i);
    }
    queuePos_ = 0;
    stage_ = Stage::Game;
}

void Sync::nextGame() {
    if (queuePos_ >= queue_.size()) {
        ++platformPos_;
        stage_ = Stage::Next;
        return;
    }
    const library::Game& g = games_[queue_[queuePos_++]];
    const covers::Source& src = covers::sourceFor(g.platform);
    progress_.file = g.name + ".png";
    progress_.bytes = 0;
    progress_.bytesTotal = -1;

    const covers::Remote* remote = covers::find(index_, g.name);
    if (!remote) {
        ++progress_.counts.missing;
        ++progress_.index;
        return;
    }
    replacing_ = false;
    if (!g.image.empty()) {
        std::vector<std::uint8_t> local;
        if (fsx::readBytes(g.image, local) && sha1::gitBlob(local) == remote->sha) {
            ++progress_.counts.same;
            ++progress_.index;
            return;
        }
        if (tasks::coverState(g.image) == tasks::CoverState::Marked) {
            ++progress_.counts.marked;  // the completed mark would be lost
            ++progress_.index;
            return;
        }
        replacing_ = true;
    }
    target_ = g.dir + "/" + g.name + ".png";
    expected_ = remote->sha;
    progress_.file = remote->name + ".png";
    progress_.phase = Phase::Downloading;
    client_.start(covers::imageUrl(src, remote->name), kMaxImageBytes);
    stage_ = Stage::Download;
}

void Sync::finishGame() {
    ++progress_.index;
    progress_.phase = Phase::ReadingIndex;
    stage_ = Stage::Game;
}

void Sync::pollDownload() {
    const http::State st = client_.step();
    const http::Progress pr = client_.progress();
    progress_.bytes = pr.done;
    progress_.bytesTotal = pr.total;
    if (st == http::State::Running || st == http::State::Idle) return;

    auto fail = [&](const std::string& why) {
        ++progress_.counts.failed;
        progress_.lastError = progress_.file + ": " + why;
        finishGame();
    };
    if (st == http::State::Failed) return fail(client_.error());
    if (client_.status() != 200) return fail("HTTP " + std::to_string(client_.status()));

    const std::vector<std::uint8_t> body = client_.take();
    if (sha1::gitBlob(body) != expected_) return fail("checksum differs from the repository");
    const std::string part = target_ + ".part";
    if (!fsx::writeBytes(part, body) || !fsx::renameFile(part, target_)) {
        fsx::removeFile(part);
        return fail("could not write the file");
    }
    if (replacing_) ++progress_.counts.replaced;
    else ++progress_.counts.added;
    finishGame();
}

}  // namespace coversync
