#include "coversync.h"

#include <algorithm>
#include <sstream>

#include "frame.h"
#include "fsx.h"
#include "isoinfo.h"
#include "sha1.h"

namespace coversync {
namespace {

constexpr std::size_t kMaxIndexBytes = 16u * 1024 * 1024;  // the PS1 and PS2 folder lists are about 2.5 MB
constexpr std::size_t kMaxImageBytes = 8u * 1024 * 1024;
constexpr std::size_t kMaxLinkBytes = 4096;
constexpr int kMaxHops = 3;

}  // namespace

Sync::Sync(http::Client& client, std::vector<library::Game> games, std::string dbPath)
    : client_(client), games_(std::move(games)), dbPath_(std::move(dbPath)) {
    for (const library::Platform p : {library::Platform::Ps3, library::Platform::Ps2, library::Platform::Psx}) {
        if (std::any_of(games_.begin(), games_.end(), [&](const library::Game& g) { return g.platform == p; })) platforms_.push_back(p);
    }
    progress_.total = static_cast<int>(games_.size());
    loadDb();
}

void Sync::loadDb() {
    std::string text;
    if (dbPath_.empty() || !fsx::readFile(dbPath_, text)) return;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto a = line.find('\t');
        const auto b = a == std::string::npos ? a : line.find('\t', a + 1);
        if (b == std::string::npos) continue;
        db_[line.substr(0, a)] = {line.substr(a + 1, b - a - 1), line.substr(b + 1)};
    }
}

void Sync::saveDb() const {
    if (dbPath_.empty()) return;
    std::string text;
    for (const auto& [path, w] : db_) text += path + "\t" + w.remote + "\t" + w.local + "\n";
    const std::string tmp = dbPath_ + ".new";
    if (fsx::writeBytes(tmp, std::vector<std::uint8_t>(text.begin(), text.end()))) fsx::renameFile(tmp, dbPath_);
}

void Sync::step() {
    switch (stage_) {
        case Stage::Next: nextPlatform(); break;
        case Stage::RootTree: pollIndex(false); break;
        case Stage::FolderTree: pollIndex(true); break;
        case Stage::Game: nextGame(); break;
        case Stage::Link: pollLink(); break;
        case Stage::Download: pollDownload(); break;
        case Stage::Fallback: pollFallback(); break;
        case Stage::End: finish(); break;
    }
}

void Sync::finish() {
    if (progress_.phase == Phase::Finished) return;
    saveDb();
    stage_ = Stage::End;
    progress_.phase = Phase::Finished;
}

void Sync::cancel() {
    if (done()) return;
    client_.cancel();
    progress_.cancelled = true;
    finish();
}

void Sync::nextPlatform() {
    if (platformPos_ >= platforms_.size()) return finish();
    const covers::Source& src = covers::sourceFor(platforms_[platformPos_]);
    progress_.platform = platforms_[platformPos_];
    progress_.phase = Phase::ReadingIndex;
    progress_.file.clear();
    progress_.bytes = 0;
    progress_.bytesTotal = -1;
    index_ = {};
    client_.start(covers::treeUrl(src, src.branch), kMaxIndexBytes);
    stage_ = Stage::RootTree;
}

void Sync::buildQueue() {
    queue_.clear();
    for (std::size_t i = 0; i < games_.size(); ++i) {
        if (games_[i].platform == platforms_[platformPos_]) queue_.push_back(i);
    }
    queuePos_ = 0;
    stage_ = Stage::Game;
}

void Sync::indexFailed(const std::string& why) {
    ++progress_.indexErrors;
    progress_.lastError = why;
    index_ = {};
    if (platforms_[platformPos_] == library::Platform::Ps3) {  // the fallback server still serves PS3 covers
        buildQueue();
        return;
    }
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
    buildQueue();
}

std::string Sync::readLocal() const {
    std::vector<std::uint8_t> local;
    if (game_->image.empty() || !fsx::readBytes(game_->image, local)) return "";
    return sha1::gitBlob(local);
}

void Sync::finishGame() {
    ++progress_.index;
    progress_.phase = Phase::Checking;
    stage_ = Stage::Game;
}

void Sync::failGame(const std::string& why) {
    ++progress_.counts.failed;
    progress_.lastError = progress_.file + ": " + why;
    finishGame();
}

void Sync::nextGame() {
    if (queuePos_ >= queue_.size()) {
        ++platformPos_;
        stage_ = Stage::Next;
        return;
    }
    game_ = &games_[queue_[queuePos_++]];
    target_ = game_->dir + "/" + game_->name + ".png";
    progress_.file = game_->name + ".png";
    progress_.phase = Phase::Checking;
    progress_.bytes = 0;
    progress_.bytesTotal = -1;
    hops_ = 0;
    titleId_.clear();
    regions_.clear();
    regionPos_ = 0;
    localSha_ = readLocal();

    if (const covers::Remote* r = covers::find(index_, game_->name)) return tryRemote(*r);
    if (game_->platform == library::Platform::Ps3) {  // the repository has few PS3 covers: ask the fallback server
        titleId_ = isoinfo::ps3TitleId(game_->dir + "/" + game_->file);
        if (!titleId_.empty()) {
            regions_ = covers::gametdbRegions(titleId_);
            return startFallback();
        }
    }
    afterFallbackMiss();
}

void Sync::afterFallbackMiss() {
    if (const covers::Remote* r = covers::findLoose(index_, game_->name)) return tryRemote(*r);
    ++progress_.counts.missing;
    finishGame();
}

void Sync::tryRemote(const covers::Remote& remote) {
    remote_ = remote;
    if (remote_.symlink) {
        if (++hops_ > kMaxHops) return failGame("too many links");
        const covers::Source& src = covers::sourceFor(game_->platform);
        progress_.file = remote_.name + ".png";
        progress_.phase = Phase::Downloading;
        client_.start(covers::imageUrl(src, remote_.name), kMaxLinkBytes);
        stage_ = Stage::Link;
        return;
    }
    const auto it = db_.find(target_);
    if (!localSha_.empty() && it != db_.end() && it->second.remote == remote_.sha && it->second.local == localSha_) {
        ++progress_.counts.same;
        return finishGame();
    }
    startDownload(remote_);
}

void Sync::startDownload(const covers::Remote& remote) {
    const covers::Source& src = covers::sourceFor(game_->platform);
    progress_.file = remote.name + ".png";
    progress_.phase = Phase::Downloading;
    progress_.bytes = 0;
    progress_.bytesTotal = -1;
    client_.start(covers::imageUrl(src, remote.name), kMaxImageBytes);
    stage_ = Stage::Download;
}

void Sync::pollLink() {
    const http::State st = client_.step();
    if (st == http::State::Running || st == http::State::Idle) return;
    if (st == http::State::Failed) return failGame(client_.error());
    if (client_.status() != 200) return failGame("HTTP " + std::to_string(client_.status()));
    const std::vector<std::uint8_t> body = client_.take();
    if (sha1::gitBlob(body) != remote_.sha) return failGame("checksum differs from the repository");
    const std::string target = covers::linkTarget(std::string(body.begin(), body.end()));
    const covers::Remote* next = target.empty() ? nullptr : covers::findTarget(index_, target);
    if (!next) return failGame("link to a missing image");
    tryRemote(*next);
}

void Sync::store(const std::vector<std::uint8_t>& framed, const std::string& remoteSha, bool fromFallback) {
    const std::string written = sha1::gitBlob(framed);
    if (!localSha_.empty() && localSha_ == written) {  // the cover on the console is already this one
        if (!fromFallback) db_[target_] = {remoteSha, written};
        ++progress_.counts.same;
        return finishGame();
    }
    const std::string part = target_ + ".part";
    if (!fsx::writeBytes(part, framed) || !fsx::renameFile(part, target_)) {
        fsx::removeFile(part);
        return failGame("could not write the file");
    }
    if (!fromFallback) db_[target_] = {remoteSha, written};
    if (localSha_.empty()) ++progress_.counts.added;
    else ++progress_.counts.replaced;
    if (fromFallback) ++progress_.counts.fallback;
    finishGame();
}

void Sync::pollDownload() {
    const http::State st = client_.step();
    const http::Progress pr = client_.progress();
    progress_.bytes = pr.done;
    progress_.bytesTotal = pr.total;
    if (st == http::State::Running || st == http::State::Idle) return;
    if (st == http::State::Failed) return failGame(client_.error());
    if (client_.status() != 200) return failGame("HTTP " + std::to_string(client_.status()));

    const std::vector<std::uint8_t> body = client_.take();
    if (sha1::gitBlob(body) != remote_.sha) return failGame("checksum differs from the repository");
    std::vector<std::uint8_t> framed;
    if (!frame::fit(body, frame::kSize, framed)) return failGame("not a usable image");
    store(framed, remote_.sha, false);
}

void Sync::startFallback() {
    progress_.file = game_->name + ".png";
    progress_.phase = Phase::Downloading;
    progress_.bytes = 0;
    progress_.bytesTotal = -1;
    client_.start(covers::gametdbUrl(titleId_, regions_[regionPos_]), kMaxImageBytes);
    stage_ = Stage::Fallback;
}

void Sync::pollFallback() {
    const http::State st = client_.step();
    const http::Progress pr = client_.progress();
    progress_.bytes = pr.done;
    progress_.bytesTotal = pr.total;
    if (st == http::State::Running || st == http::State::Idle) return;
    if (st == http::State::Failed) return failGame(client_.error());
    if (client_.status() == 404) {  // not in this region folder: try the next, then the repository's other names
        if (++regionPos_ < regions_.size()) return startFallback();
        return afterFallbackMiss();
    }
    if (client_.status() != 200) return failGame("HTTP " + std::to_string(client_.status()));
    const std::vector<std::uint8_t> body = client_.take();
    std::vector<std::uint8_t> framed;
    if (!frame::fit(body, frame::kSize, framed)) return failGame("not a usable image");
    store(framed, "", true);
}

}  // namespace coversync
