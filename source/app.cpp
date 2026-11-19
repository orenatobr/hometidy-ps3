#include "app.h"

#include <algorithm>
#include <cstdio>

#include "badge.h"
#include "covers.h"
#include "default_rules.h"
#include "fsx.h"
#include "texts.h"

using gfx::Align;
using gfx::Size;
namespace col = gfx::theme;
using strings::Id;

namespace {

constexpr int kMargin = 64;   // 5% side margin: keeps everything inside the TV safe area
constexpr int kTop = 36;      // 5% top margin
constexpr int kHintsY = 640;  // top of the button hints row (ends at 674, inside the safe area)
constexpr int kGridCols = 7;
constexpr int kGridRows = 2;
constexpr int kTileW = 148;
constexpr int kTileH = 186;
constexpr int kCoverSize = 124;  // cover square inside a tile, in logical pixels
constexpr int kTileGap = 14;
#ifndef APP_VERSION
#error "APP_VERSION must be set by the build (the release version, read from the VERSION file)"
#endif
constexpr const char* kVersion = "v" APP_VERSION;

const gfx::Color kBlue{125, 159, 214};
const gfx::Color kPink{217, 140, 179};
const gfx::Color kRose{224, 112, 124};
const gfx::Color kMint{91, 185, 140};

std::string fmt(const char* f, int a) {
    char b[256];
    std::snprintf(b, sizeof b, f, a);
    return b;
}

std::string fmtS(const char* f, const std::string& a) {
    char b[512];
    std::snprintf(b, sizeof b, f, a.c_str());
    return b;
}

std::string fmt2(const char* f, int a, int b) {
    char buf[256];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

// "1.2 MB" / "340 KB"
std::string sizeText(long long bytes) {
    char b[32];
    if (bytes >= 1024 * 1024) std::snprintf(b, sizeof b, "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else std::snprintf(b, sizeof b, "%lld KB", (bytes + 1023) / 1024);
    return b;
}

std::string tr(Id id) { return strings::get(id); }

}  // namespace

App::App(gfx::Canvas& canvas, platform::Env env) : c_(canvas), env_(std::move(env)) {}

// ------------------------------------------------------------- lifecycle --

void App::scanGames() {
    games_ = library::scan(env_.roots);
    std::stable_sort(games_.begin(), games_.end(), [&](const library::Game& a, const library::Game& b) {
        return sorter::less(sorter::makeKey(a.name, rules_), sorter::makeKey(b.name, rules_));
    });
    tabs_.clear();
    for (const library::Platform p : {library::Platform::Ps3, library::Platform::Ps2, library::Platform::Psx}) {
        if (std::any_of(games_.begin(), games_.end(), [&](const library::Game& g) { return g.platform == p; })) tabs_.push_back(p);
    }
}

void App::load() {
    std::string user;
    if (fsx::readFile(env_.appDir + "/series.txt", user)) rules_ = sorter::parseRules(user);
    for (const auto& r : sorter::parseRules(kDefaultRules)) rules_.push_back(r);  // user rules win

    if (fsx::readFile(env_.xmlPath, xmlText_)) {
        const auto plan = tasks::planSort(xmlText_, rules_);
        if (plan) {
            for (const auto& s : plan->segments) {
                listGames_ += int(s.rows.size());
                listMoves_ += s.moves();
            }
        }
        platform::log("game list %s: %lu bytes, valid=%d, games=%d, to move=%d", env_.xmlPath.c_str(), static_cast<unsigned long>(xmlText_.size()),
                      plan.has_value(), listGames_, listMoves_);
    } else {
        platform::log("game list %s: not found", env_.xmlPath.c_str());
    }

    scanGames();
    platform::log("loaded: rules=%lu games=%lu tabs=%lu storageOk=%d", static_cast<unsigned long>(rules_.size()),
                  static_cast<unsigned long>(games_.size()), static_cast<unsigned long>(tabs_.size()), env_.storageOk);
    screen_ = Screen::Home;
}

// ----------------------------------------------------------------- input --

bool App::takeDirty() {
    const bool d = dirty_;
    dirty_ = false;
    return d;
}

void App::update(const platform::Input& in) {
    ++frame_;
    if (screen_ != lastLogged_) {
        platform::log("screen -> %d", static_cast<int>(screen_));
        lastLogged_ = screen_;
        dirty_ = true;
    }
    if (in.any()) dirty_ = true;
    if (toastFrames_ > 0 && --toastFrames_ == 0) dirty_ = true;

    if (screen_ == Screen::Loading) {
        if (frame_ >= 3) load();
        dirty_ = true;
        return;
    }
    if (busy_ > 0) {
        dirty_ = true;
        if (--busy_ == 0) applySort();
        return;
    }
    switch (screen_) {
        case Screen::Home: updateHome(in); break;
        case Screen::SortPreview: updateSort(in); break;
        case Screen::SortConfirm: updateConfirm(in); break;
        case Screen::SortDone:
            if (in.a) quit_ = true;
            if (in.b) screen_ = Screen::Home;
            break;
        case Screen::Mark: updateMark(in); break;
        case Screen::Covers: updateCovers(in); break;
        case Screen::Message:
            if (in.a || in.b) screen_ = msgBack_;
            break;
        default: break;
    }
}

void App::updateHome(const platform::Input& in) {
    if (in.left || in.up) homeFocus_ = (homeFocus_ + 2) % 3;
    if (in.right || in.down) homeFocus_ = (homeFocus_ + 1) % 3;
    if (in.b) quit_ = true;
    if (!in.a) return;
    if (!env_.storageOk) {
        toast(tr(Id::NoAccess), col::red);
        return;
    }
    if (homeFocus_ == 0) enterSort();
    else if (homeFocus_ == 1) enterMark();
    else enterCovers();
}

void App::enterSort() {
    std::string xml;
    if (!fsx::readFile(env_.xmlPath, xml)) {
        showMessage(tr(Id::SortTitle), tr(Id::NoList), col::amber, Screen::Home);
        return;
    }
    plan_ = tasks::planSort(xml, rules_);
    platform::log("sort plan: %lu bytes read, valid=%d, segments=%lu, changed=%d", static_cast<unsigned long>(xml.size()), plan_.has_value(),
                  static_cast<unsigned long>(plan_ ? plan_->segments.size() : 0), plan_ ? plan_->changedSegments : 0);
    if (!plan_ || plan_->segments.empty()) {
        showMessage(tr(Id::SortTitle), tr(Id::NoList), col::amber, Screen::Home);
        plan_.reset();
        return;
    }
    segSel_ = 0;
    for (std::size_t i = 0; i < plan_->segments.size(); ++i) {
        if (plan_->segments[i].changed) {
            segSel_ = int(i);
            break;
        }
    }
    rowSel_ = 0;
    screen_ = Screen::SortPreview;
}

void App::updateSort(const platform::Input& in) {
    if (!plan_) {
        screen_ = Screen::Home;
        return;
    }
    const int segs = int(plan_->segments.size());
    if (in.b) {
        screen_ = Screen::Home;
        return;
    }
    if (in.left || in.l) {
        segSel_ = (segSel_ + segs - 1) % segs;
        rowSel_ = 0;
    }
    if (in.right || in.r) {
        segSel_ = (segSel_ + 1) % segs;
        rowSel_ = 0;
    }
    const int rows = int(plan_->segments[segSel_].rows.size());
    if (in.up) rowSel_ = std::max(0, rowSel_ - 1);
    if (in.down) rowSel_ = std::min(rows - 1, rowSel_ + 1);
    if (in.l2) rowSel_ = std::max(0, rowSel_ - 10);
    if (in.r2) rowSel_ = std::min(rows - 1, rowSel_ + 10);
    if (in.a) {
        if (plan_->changedSegments == 0) toast(tr(Id::AlreadySorted), col::green);
        else screen_ = Screen::SortConfirm;
    }
}

void App::updateConfirm(const platform::Input& in) {
    if (in.b) screen_ = Screen::SortPreview;
    if (in.a) busy_ = 3;
}

void App::applySort() {
    sortResult_ = tasks::applySort(env_.xmlPath, *plan_, env_.appDir + "/backup", platform::timestamp());
    platform::log("applySort status=%d backup=%s", int(sortResult_.status), sortResult_.backupPath.c_str());
    if (sortResult_.status != tasks::Status::Ok) {
        showMessage(tr(Id::FailTitle), statusText(sortResult_.status), col::red, Screen::Home);
        return;
    }
    platform::sleepMs(2000);  // webMAN may still rewrite the file; look again
    recheckOk_ = tasks::fileEquals(env_.xmlPath, plan_->sorted);
    platform::log("recheck=%d", recheckOk_);
    listMoves_ = 0;
    screen_ = Screen::SortDone;
}

std::vector<int> App::tabGames() const {
    std::vector<int> idx;
    if (tabs_.empty()) return idx;
    for (std::size_t i = 0; i < games_.size(); ++i) {
        if (games_[i].platform == tabs_[std::size_t(tab_)]) idx.push_back(int(i));
    }
    return idx;
}

const library::Game* App::currentGame() const {
    const auto idx = tabGames();
    if (idx.empty()) return nullptr;
    return &games_[std::size_t(idx[std::size_t(std::clamp(markSel_, 0, int(idx.size()) - 1))])];
}

void App::enterMark() {
    if (games_.empty()) {
        showMessage(tr(Id::MarkTitle), tr(Id::NoGames), col::amber, Screen::Home);
        return;
    }
    tab_ = std::clamp(tab_, 0, int(tabs_.size()) - 1);
    markSel_ = std::clamp(markSel_, 0, int(tabGames().size()) - 1);
    screen_ = Screen::Mark;
}

void App::updateMark(const platform::Input& in) {
    if (in.b) {
        screen_ = Screen::Home;
        return;
    }
    if ((in.l || in.r) && tabs_.size() > 1) {
        tab_ = (tab_ + int(tabs_.size()) + (in.r ? 1 : -1)) % int(tabs_.size());
        markSel_ = 0;
        markScroll_ = 0;
    }
    const int n = int(tabGames().size());
    if (in.left) markSel_ = std::max(0, markSel_ - 1);
    if (in.right) markSel_ = std::min(n - 1, markSel_ + 1);
    if (in.up) markSel_ = std::max(0, markSel_ - kGridCols);
    if (in.down) markSel_ = std::min(n - 1, markSel_ + kGridCols);
    if (in.l2) markSel_ = std::max(0, markSel_ - kGridCols * kGridRows);
    if (in.r2) markSel_ = std::min(n - 1, markSel_ + kGridCols * kGridRows);
    const int row = markSel_ / kGridCols;
    if (row < markScroll_) markScroll_ = row;
    if (row >= markScroll_ + kGridRows) markScroll_ = row - kGridRows + 1;
    if (in.a) toggleMark();
}

void App::toggleMark() {
    const library::Game* g = currentGame();
    if (!g) return;
    if (g->image.empty()) {
        toast(tr(Id::ToastNoCover), col::amber);
        return;
    }
    Cover& cv = cover(*g);
    const std::string dir = env_.appDir + "/backup";
    tasks::Result r;
    if (cv.state == tasks::CoverState::Marked) {
        r = tasks::unmarkCover(g->image, dir);
        if (r.status == tasks::Status::Ok) toast(tr(r.approximate ? Id::ToastUnmarkedApprox : Id::ToastUnmarked), col::green);
    } else {
        r = tasks::markCover(g->image, dir);
        if (r.status == tasks::Status::Ok) toast(tr(Id::ToastMarked), col::green);
    }
    platform::log("mark %s status=%d", g->image.c_str(), int(r.status));
    if (r.status == tasks::Status::NothingToDo) toast(tr(Id::ToastAlready), col::amber);
    else if (r.status != tasks::Status::Ok) toast(statusText(r.status), col::red);
    dropCover(g->image);  // reload the new pixels
}

// ------------------------------------------------------------- utilities --

std::string App::statusText(tasks::Status s) const {
    switch (s) {
        case tasks::Status::ReadFailed: return tr(Id::StReadFailed);
        case tasks::Status::Changed: return tr(Id::StChanged);
        case tasks::Status::BackupFailed: return tr(Id::StBackupFailed);
        case tasks::Status::WriteFailed: return tr(Id::StWriteFailed);
        case tasks::Status::VerifyFailed: return tr(Id::StVerifyFailed);
        case tasks::Status::NothingToDo: return tr(Id::StNothing);
        case tasks::Status::NoBackup: return tr(Id::StNoBackup);
        case tasks::Status::Unsupported: return tr(Id::StUnsupported);
        default: return "";
    }
}

void App::toast(const std::string& text, gfx::Color color) {
    toastText_ = text;
    toastColor_ = color;
    toastFrames_ = 150;
    dirty_ = true;
}

void App::showMessage(const std::string& title, const std::string& body, gfx::Color color, Screen back) {
    msgTitle_ = title;
    msgBody_ = body;
    msgColor_ = color;
    msgBack_ = back;
    screen_ = Screen::Message;
}

App::Cover& App::cover(const library::Game& g, bool urgent) {
    auto it = covers_.find(g.image);
    if (it != covers_.end()) return it->second;
    if (!urgent && decodeBudget_ <= 0) {
        static Cover pending;
        pending.loading = true;
        dirty_ = true;
        return pending;
    }
    if (!urgent) --decodeBudget_;
    Cover cv;
    std::vector<std::uint8_t> file;
    badge::Image img;
    if (!g.image.empty() && fsx::readBytes(g.image, file, 16u * 1024 * 1024) && badge::decode(file, img)) {
        cv.tex = gfx::Canvas::thumbnail(img.rgba, img.w, img.h, c_.pixels(kCoverSize), c_.pixels(kCoverSize));
        cv.state = badge::isMarked(img) ? tasks::CoverState::Marked : tasks::CoverState::Plain;
    }
    if (covers_.size() > 160) covers_.clear();  // keep memory bounded
    return covers_.emplace(g.image, cv).first->second;
}

void App::dropCover(const std::string& path) { covers_.erase(path); }

// -------------------------------------------------------------- drawing --

void App::draw() {
    c_.clear(col::bg);
    switch (screen_) {
        case Screen::Loading: {
            c_.checkBadge(640, 300, 56, col::greenDark, col::text);
            c_.text("HomeTidy", Size::Title, col::text, 640, 380, Align::Center);
            c_.text(tr(Id::Loading), Size::Body, col::muted, 640, 450, Align::Center);
            break;
        }
        case Screen::Home: drawHome(); break;
        case Screen::SortPreview: drawSort(); break;
        case Screen::SortConfirm:
            drawSort();
            drawConfirm();
            break;
        case Screen::SortDone: drawDone(); break;
        case Screen::Mark: drawMark(); break;
        case Screen::Covers: drawCovers(); break;
        case Screen::Message: drawMessage(); break;
    }
    if (busy_ > 0) {
        c_.fill(0, 0, 1280, 720, 0, {0, 0, 0, 190});
        c_.card(440, 300, 400, 120, 20, col::card, 2, col::line);
        c_.text(tr(Id::Busy), Size::Large, col::text, 640, 340, Align::Center);
    }
    toastDraw();
}

void App::header(const std::string& subtitle) {
    c_.checkBadge(kMargin + 26, kTop + 32, 26, col::greenDark, col::text);
    c_.text("HomeTidy", Size::Large, col::text, kMargin + 68, kTop + 8);
    c_.text(subtitle, Size::Body, col::muted, 1280 - kMargin, kTop + 14, Align::Right);
}

int App::drawButton(Btn b, int x, int y) {
    const int d = 32, cx = x + d / 2, cy = y + d / 2;
    Btn face = b;
    if (b == Btn::Confirm) face = env_.circleConfirms ? Btn::Back : Btn::Confirm;  // Btn::Back stands for the circle
    else if (b == Btn::Back) face = env_.circleConfirms ? Btn::Confirm : Btn::Back;
    // After the swap above Btn::Confirm always means "cross" and Btn::Back always means "circle".
    switch (face) {
        case Btn::Confirm:
            c_.circle(cx, cy, d / 2, col::raised);
            c_.line(cx - 6, cy - 6, cx + 6, cy + 6, 3, kBlue);
            c_.line(cx + 6, cy - 6, cx - 6, cy + 6, 3, kBlue);
            return d;
        case Btn::Back:
            c_.circle(cx, cy, d / 2, col::raised);
            c_.ring(cx, cy, 7, 3, kRose);
            return d;
        case Btn::Triangle:
            c_.circle(cx, cy, d / 2, col::raised);
            c_.line(cx, cy - 7, cx + 7, cy + 5, 3, kMint);
            c_.line(cx + 7, cy + 5, cx - 7, cy + 5, 3, kMint);
            c_.line(cx - 7, cy + 5, cx, cy - 7, 3, kMint);
            return d;
        case Btn::Square:
            c_.circle(cx, cy, d / 2, col::raised);
            c_.stroke(cx - 7, cy - 7, 14, 14, 2, 3, kPink);
            return d;
        case Btn::DPad:
            c_.circle(cx, cy, d / 2, col::raised);
            c_.fill(cx - 3, cy - 9, 6, 18, 2, col::muted);
            c_.fill(cx - 9, cy - 3, 18, 6, 2, col::muted);
            return d;
        default: {
            const char* name = b == Btn::L1 ? "L1" : b == Btn::R1 ? "R1" : b == Btn::L2 ? "L2" : "R2";
            const int w = std::max(d + 8, c_.textWidth(name, Size::Small) + 24);
            c_.fill(x, y, w, d, d / 2, col::raised);
            c_.text(name, Size::Small, col::text, x + w / 2, y + 2, Align::Center);
            return w;
        }
    }
}

void App::hints(const std::vector<Hint>& items) {
    int x = kMargin;
    for (const auto& it : items) {
        x += drawButton(it.btn, x, kHintsY) + 10;
        x += c_.text(it.label, Size::Small, col::muted, x, kHintsY + 3) + 30;
    }
    c_.text(kVersion, Size::Small, col::muted, 1280 - kMargin, kHintsY + 3, Align::Right);
}

void App::toastDraw() {
    if (toastFrames_ <= 0) return;
    const int w = std::min(1152, std::max(320, c_.textWidth(toastText_, Size::Body) + 72));
    const int x = (1280 - w) / 2, y = kTop + 76;
    c_.card(x, y, w, 56, 28, col::card, 3, toastColor_);
    c_.text(toastText_, Size::Body, col::text, 640, y + 12, Align::Center, w - 48);
}

void App::drawHome() {
    header(tr(Id::AppTagline));
    const int cardW = 1280 - 2 * kMargin, cardH = 124, gap = 16, top = 118;
    for (int i = 0; i < 3; ++i) {
        const int x = kMargin, y = top + i * (cardH + gap);
        const bool focus = homeFocus_ == i;
        if (focus) c_.fill(x - 6, y - 6, cardW + 12, cardH + 12, 30, col::green);
        c_.card(x, y, cardW, cardH, 24, focus ? col::raised : col::card, 2, col::line);

        const int ix = x + 32, iy = y + 16;
        if (i == 0) {  // bars shrinking = sorted list
            const int widths[4] = {70, 56, 42, 28};
            for (int k = 0; k < 4; ++k) c_.fill(ix, iy + 10 + k * 20, widths[k], 12, 6, k == 0 ? col::green : col::muted);
        } else {
            c_.fill(ix, iy, 92, 92, 16, col::line);
            c_.fill(ix + 12, iy + 12, 68, 68, 10, col::card);
            if (i == 1) {  // cover tile with a check
                c_.checkBadge(ix + 74, iy + 18, 26, col::green, col::bg);
            } else {  // cover tile with a download arrow
                c_.line(ix + 46, iy + 24, ix + 46, iy + 56, 6, col::green);
                c_.line(ix + 30, iy + 44, ix + 46, iy + 60, 6, col::green);
                c_.line(ix + 62, iy + 44, ix + 46, iy + 60, 6, col::green);
            }
        }
        const Id titles[3] = {Id::SortTitle, Id::MarkTitle, Id::CoversTitle};
        const Id descs[3] = {Id::SortDesc, Id::MarkDesc, Id::CoversDesc};
        const int tx = x + 160, textW = 640;
        c_.text(tr(titles[i]), Size::Large, col::text, tx, y + 14, Align::Left, textW);
        const auto lines = c_.wrap(tr(descs[i]), Size::Small, textW);
        for (std::size_t k = 0; k < lines.size() && k < 2; ++k) {
            c_.text(lines[k], Size::Small, col::muted, tx, y + 58 + int(k) * 28);
        }
        const std::string status = i == 0 ? fmt(tr(Id::SortStatus).c_str(), listGames_) : fmt(tr(Id::MarkStatus).c_str(), int(games_.size()));
        const int sw = c_.textWidth(status, Size::Small) + 36;
        const int px = x + cardW - 32 - sw, py = y + 26;
        c_.fill(px, py, sw, 40, 20, focus ? col::card : col::raised);
        c_.text(status, Size::Small, col::text, px + sw / 2, py + 7, Align::Center);
        if (i == 0 && listGames_ > 0) {
            const std::string note = listMoves_ > 0 ? fmt(tr(Id::ListMoves).c_str(), listMoves_) : tr(Id::ListSorted);
            c_.text(note, Size::Small, listMoves_ > 0 ? col::amber : col::green, px + sw / 2, py + 52, Align::Center);
        }
    }
    const int noteY = top + 3 * (cardH + gap) + 4;
    if (!env_.storageOk) c_.text(tr(Id::NoAccess), Size::Small, col::red, kMargin, noteY, Align::Left, 1150);
    else if (listGames_ == 0) c_.text(tr(Id::NoList), Size::Small, col::amber, kMargin, noteY, Align::Left, 1150);
    else c_.text(tr(Id::ListFile) + ": " + env_.xmlPath, Size::Small, col::muted, kMargin, noteY, Align::Left, 1150);
    hints({{Btn::DPad, tr(Id::HintSelect)}, {Btn::Confirm, tr(Id::HintApply)}, {Btn::Back, tr(Id::HintExit)}});
}

void App::drawSort() {
    header(tr(Id::SortTitle));
    if (!plan_) return;
    const int top = 122, h = 500;
    const int lw = 330;
    for (std::size_t i = 0; i < plan_->segments.size() && i < 6; ++i) {
        const auto& s = plan_->segments[i];
        const int y = top + int(i) * 84;
        const bool sel = int(i) == segSel_;
        if (sel) c_.fill(kMargin - 4, y - 4, lw + 8, 78, 18, col::green);
        c_.card(kMargin, y, lw, 70, 14, sel ? col::raised : col::card, 2, col::line);
        c_.text(s.label, Size::Body, col::text, kMargin + 20, y + 6);
        std::string sub = fmt(tr(Id::ListGames).c_str(), int(s.rows.size())) + "  \xC2\xB7  ";
        sub += s.changed ? fmt(tr(Id::ListMoves).c_str(), s.moves()) : tr(Id::ListSorted);
        c_.text(sub, Size::Small, s.changed ? col::amber : col::muted, kMargin + 20, y + 38, Align::Left, lw - 40);
    }
    // new order
    const int px = kMargin + lw + 24, pw = 1280 - kMargin - px;
    c_.card(px, top, pw, h, 20, col::card, 2, col::line);
    const auto& s = plan_->segments[std::size_t(segSel_)];
    c_.text(s.label, Size::Large, col::text, px + 28, top + 14);
    c_.line(px + 20, top + 64, px + pw - 20, top + 64, 2, col::line);

    const int rowH = 38, visible = (h - 84) / rowH;
    const int n = int(s.rows.size());
    const int first = std::clamp(rowSel_ - visible / 2, 0, std::max(0, n - visible));
    for (int i = first; i < std::min(n, first + visible); ++i) {
        const auto& r = s.rows[std::size_t(i)];
        const int y = top + 76 + (i - first) * rowH;
        if (i == rowSel_) c_.fill(px + 12, y - 2, pw - 24, rowH, 10, col::raised);
        c_.text(std::to_string(i + 1), Size::Small, col::muted, px + 64, y + 4, Align::Right);
        c_.text(r.name, Size::Body, col::text, px + 84, y, Align::Left, pw - 84 - 170);
        if (r.from != r.to) c_.text(fmt(tr(Id::WasPosition).c_str(), r.from + 1), Size::Small, col::amber, px + pw - 28, y + 4, Align::Right);
    }
    hints({{Btn::Confirm, tr(Id::HintApply)}, {Btn::Back, tr(Id::HintBack)}, {Btn::L1, tr(Id::HintPlatform)},
           {Btn::DPad, tr(Id::HintScroll)}});
    c_.text(env_.xmlPath, Size::Small, col::muted, 1280 - kMargin, kTop + 46, Align::Right, 700);
}

void App::drawConfirm() {
    c_.fill(0, 0, 1280, 720, 0, {0, 0, 0, 190});
    const int w = 780, h = 320, x = (1280 - w) / 2, y = (720 - h) / 2;
    c_.card(x, y, w, h, 24, col::card, 2, col::line);
    c_.text(tr(Id::ConfirmTitle), Size::Large, col::text, x + 40, y + 36);
    const auto lines = c_.wrap(tr(Id::ConfirmBody), Size::Body, w - 80);
    for (std::size_t i = 0; i < lines.size(); ++i) c_.text(lines[i], Size::Body, col::muted, x + 40, y + 96 + int(i) * 34);
    const int by = y + h - 84;
    c_.fill(x + 40, by, 270, 52, 26, col::greenDark);
    int bx = x + 40 + 20;
    bx += drawButton(Btn::Confirm, bx, by + 10) + 12;
    c_.text(tr(Id::HintApply), Size::Body, col::text, bx, by + 12);
    c_.fill(x + 334, by, 270, 52, 26, col::raised);
    bx = x + 334 + 20;
    bx += drawButton(Btn::Back, bx, by + 10) + 12;
    c_.text(tr(Id::HintCancel), Size::Body, col::text, bx, by + 12);
}

void App::drawDone() {
    header(tr(Id::SortTitle));
    const int w = 960, h = 440, x = (1280 - w) / 2, y = 130;
    c_.card(x, y, w, h, 24, col::card, 2, col::line);
    c_.checkBadge(x + 80, y + 80, 36, col::green, col::bg);
    c_.text(tr(Id::DoneTitle), Size::Large, col::text, x + 140, y + 56);
    int ty = y + 130;
    for (const auto& l : c_.wrap(tr(Id::DoneBackup) + ": " + sortResult_.backupPath, Size::Small, w - 80)) {
        c_.text(l, Size::Small, col::muted, x + 40, ty);
        ty += 28;
    }
    ty += 14;
    for (const auto& l : c_.wrap(tr(recheckOk_ ? Id::DoneRecheckOk : Id::DoneRecheckBad), Size::Body, w - 80)) {
        c_.text(l, Size::Body, recheckOk_ ? col::green : col::red, x + 40, ty);
        ty += 34;
    }
    ty += 16;
    for (const auto& l : c_.wrap(tr(Id::DoneBody), Size::Body, w - 80)) {
        c_.text(l, Size::Body, col::text, x + 40, ty);
        ty += 34;
    }
    hints({{Btn::Confirm, tr(Id::HintClose)}, {Btn::Back, tr(Id::HintBack)}});
}

// ---------------------------------------------------------------- covers --

void App::enterCovers() {
    if (games_.empty()) {
        showMessage(tr(Id::CoversTitle), tr(Id::NoGames), col::amber, Screen::Home);
        return;
    }
    sync_.reset();
    screen_ = Screen::Covers;
}

// webMAN keeps the placeholder icon of a game that had no cover when it built the list. Link the covers that exist now.
void App::linkIcons() {
    iconsLinked_ = 0;
    iconsFailed_ = false;
    std::string xml;
    if (!fsx::readFile(env_.xmlPath, xml)) return;
    const auto coverOf = [](const std::string& base) -> std::string {
        for (const char* ext : {".png", ".jpg", ".jpeg"}) {
            if (fsx::exists(base + ext)) return base + ext;
        }
        return "";
    };
    const auto plan = tasks::planIcons(xml, coverOf);
    if (!plan) return;
    const tasks::Result r = tasks::applyIcons(env_.xmlPath, *plan, env_.appDir + "/backup", platform::timestamp());
    iconsFailed_ = r.status != tasks::Status::Ok;
    if (!iconsFailed_) iconsLinked_ = plan->changed;
    platform::log("icons: linked=%d status=%d", plan->changed, static_cast<int>(r.status));
}

void App::startCovers() {
    if (!http_) http_ = platform::makeHttp(env_);
    sync_ = std::make_unique<coversync::Sync>(*http_, games_, env_.appDir + "/covers.db");
    platform::log("covers: start, %lu games", static_cast<unsigned long>(games_.size()));
}

void App::updateCovers(const platform::Input& in) {
    if (!sync_) {
        if (in.a) startCovers();
        else if (in.b) screen_ = Screen::Home;
        return;
    }
    if (!sync_->done()) {
        dirty_ = true;
        if (in.b) {
            sync_->cancel();
        } else {
            for (int i = 0; i < 2 && !sync_->done(); ++i) sync_->step();  // two steps per frame: hashing a cover is the slow part
        }
        if (sync_->done()) {
            const coversync::Progress& p = sync_->progress();
            platform::log("covers: end added=%d replaced=%d same=%d missing=%d failed=%d fallback=%d indexErrors=%d last=%s", p.counts.added,
                          p.counts.replaced, p.counts.same, p.counts.missing, p.counts.failed, p.counts.fallback, p.indexErrors,
                          p.lastError.c_str());
            covers_.clear();  // new images on disk
            scanGames();
            linkIcons();
        }
        return;
    }
    if (in.a || in.b) {
        sync_.reset();
        screen_ = Screen::Home;
    }
}

void App::drawBar(int x, int y, int w, double fraction, bool unknown) {
    c_.fill(x, y, w, 20, 10, col::raised);
    if (unknown) {  // total size not announced: a block sweeping across
        const int block = w / 5;
        const int pos = (frame_ * 12) % (w + block) - block;
        const int x0 = std::max(0, pos), x1 = std::min(w, pos + block);
        if (x1 > x0) c_.fill(x + x0, y, x1 - x0, 20, 10, col::green);
        return;
    }
    const int fill = static_cast<int>(std::clamp(fraction, 0.0, 1.0) * w);
    if (fill >= 20) c_.fill(x, y, fill, 20, 10, col::green);
    else if (fill > 0) c_.fill(x, y, 20, 20, 10, col::green);
}

void App::drawCovers() {
    header(tr(Id::CoversTitle));
    const int x = kMargin, y = 122, w = 1280 - 2 * kMargin, h = 490;
    c_.card(x, y, w, h, 20, col::card, 2, col::line);
    if (!sync_) {
        const auto lines = c_.wrap(tr(Id::CoversIntro), Size::Body, w - 80);
        for (std::size_t k = 0; k < lines.size() && k < 6; ++k) c_.text(lines[k], Size::Body, col::text, x + 40, y + 36 + int(k) * 36);
        c_.text(tr(Id::CoversSource), Size::Small, col::muted, x + 40, y + 250);
        int ly = y + 284;
        for (const library::Platform p : tabs_) {
            c_.text(std::string(library::label(p)) + "   github.com/" + covers::sourceFor(p).repo, Size::Body, col::text, x + 40, ly);
            ly += 40;
        }
        if (std::find(tabs_.begin(), tabs_.end(), library::Platform::Ps3) != tabs_.end()) {
            c_.text("PS3   art.gametdb.com (fallback)", Size::Body, col::text, x + 40, ly);
        }
        hints({{Btn::Confirm, tr(Id::HintStart)}, {Btn::Back, tr(Id::HintBack)}});
        return;
    }

    const coversync::Progress& p = sync_->progress();
    const bool done = sync_->done();
    const int bw = w - 80;
    // what is happening
    std::string what;
    if (done) what = tr(p.cancelled ? Id::CoversStopped : Id::CoversDone);
    else if (p.phase == coversync::Phase::Downloading) what = p.file;
    else if (p.phase == coversync::Phase::Checking) what = fmtS(tr(Id::CoversChecking).c_str(), p.file);
    else what = fmtS(tr(Id::CoversReading).c_str(), library::label(p.platform));
    c_.text(what, Size::Large, done && !p.cancelled && p.counts.failed == 0 ? col::green : col::text, x + 40, y + 32, Align::Left, bw);

    // this transfer
    if (!done) {
        const bool known = p.bytesTotal > 0;
        drawBar(x + 40, y + 100, bw, known ? double(p.bytes) / double(p.bytesTotal) : 0.0, !known && p.phase != coversync::Phase::Starting);
        std::string sizes = sizeText(p.bytes);
        if (known) sizes += " / " + sizeText(p.bytesTotal);
        c_.text(sizes, Size::Small, col::muted, x + w - 40, y + 130, Align::Right);
    }
    // all games
    const double overall = p.total > 0 ? double(p.index) / double(p.total) : 0.0;
    c_.text(fmt2(tr(Id::CoversOverall).c_str(), p.index, p.total), Size::Body, col::text, x + 40, y + 176);
    drawBar(x + 40, y + 216, bw, done && !p.cancelled ? 1.0 : overall, false);

    // counters
    const std::pair<Id, int> rows[] = {{Id::CvAdded, p.counts.added},     {Id::CvReplaced, p.counts.replaced}, {Id::CvSame, p.counts.same},
                                       {Id::CvFallback, p.counts.fallback}, {Id::CvMissing, p.counts.missing}, {Id::CvFailed, p.counts.failed}};
    for (int i = 0; i < 6; ++i) {
        const int cx = x + 40 + (i % 2) * (bw / 2), cy = y + 268 + (i / 2) * 40;
        const bool bad = rows[i].first == Id::CvFailed && rows[i].second > 0;
        c_.text(fmt(tr(rows[i].first).c_str(), rows[i].second), Size::Body, bad ? col::red : col::muted, cx, cy, Align::Left, bw / 2 - 20);
    }
    if (done && iconsLinked_ > 0) {
        c_.text(fmt(tr(Id::CvLinked).c_str(), iconsLinked_), Size::Body, col::green, x + 40, y + 380, Align::Left, bw);
        c_.text(tr(Id::CoversLinkedHint), Size::Small, col::muted, x + 40, y + 416, Align::Left, bw);
    } else if (done && iconsFailed_) {
        c_.text(tr(Id::FailTitle), Size::Body, col::amber, x + 40, y + 380, Align::Left, bw);
    } else if (p.indexErrors > 0 || (p.counts.failed > 0 && !p.lastError.empty())) {
        c_.text(p.lastError, Size::Small, col::amber, x + 40, y + 396, Align::Left, bw);
        c_.text(tr(Id::CoversNetHint), Size::Small, col::muted, x + 40, y + 428, Align::Left, bw);
    }
    if (done) hints({{Btn::Confirm, tr(Id::HintBack)}});
    else hints({{Btn::Back, tr(Id::HintStop)}});
}

void App::drawMessage() {
    header("");
    const int w = 860, h = 300, x = (1280 - w) / 2, y = 180;
    c_.card(x, y, w, h, 24, col::card, 3, msgColor_);
    c_.text(msgTitle_, Size::Large, col::text, x + 40, y + 36, Align::Left, w - 80);
    int ty = y + 100;
    for (const auto& l : c_.wrap(msgBody_, Size::Body, w - 80)) {
        c_.text(l, Size::Body, col::muted, x + 40, ty);
        ty += 34;
    }
    hints({{Btn::Confirm, tr(Id::HintBack)}});
}

void App::drawMark() {
    header(tr(Id::MarkTitle));
    const auto idx = tabGames();
    const int n = int(idx.size());
    decodeBudget_ = 1;  // one decode per frame keeps the d-pad responsive while covers fill in
    if (const library::Game* cur = currentGame()) cover(*cur);  // the selected one first

    // platform tabs
    int tx = kMargin;
    for (std::size_t t = 0; t < tabs_.size(); ++t) {
        int count = 0;
        for (const auto& g : games_) count += g.platform == tabs_[t];
        const std::string label = std::string(library::label(tabs_[t])) + "  " + std::to_string(count);
        const int tw = c_.textWidth(label, Size::Small) + 36;
        const bool on = int(t) == tab_;
        c_.fill(tx, 106, tw, 36, 18, on ? col::greenDark : col::card);
        c_.text(label, Size::Small, on ? col::text : col::muted, tx + tw / 2, 112, Align::Center);
        tx += tw + 10;
    }
    if (n > 0) c_.text(std::to_string(markSel_ + 1) + " / " + std::to_string(n), Size::Small, col::muted, 1280 - kMargin, 112, Align::Right);

    const int x0 = (1280 - (kGridCols * kTileW + (kGridCols - 1) * kTileGap)) / 2, y0 = 156;
    for (int row = 0; row < kGridRows; ++row) {
        for (int cIdx = 0; cIdx < kGridCols; ++cIdx) {
            const int i = (markScroll_ + row) * kGridCols + cIdx;
            if (i >= n) break;
            const library::Game& g = games_[std::size_t(idx[std::size_t(i)])];
            const int x = x0 + cIdx * (kTileW + kTileGap), y = y0 + row * (kTileH + kTileGap);
            const bool sel = i == markSel_;
            if (sel) c_.fill(x - 5, y - 5, kTileW + 10, kTileH + 10, 20, col::green);
            c_.card(x, y, kTileW, kTileH, 16, sel ? col::raised : col::card, 2, col::line);
            Cover& cv = cover(g, false);
            if (cv.tex) {
                c_.blit(*cv.tex, x + 12, y + 10, kCoverSize, kCoverSize, 8);
            } else if (cv.loading) {
                c_.fill(x + 12, y + 10, kCoverSize, kCoverSize, 8, col::line);
            } else {
                c_.fill(x + 12, y + 10, 124, 124, 8, col::line);
                const auto words = c_.wrap(tr(Id::NoCover), Size::Small, 108);
                const int top = y + 72 - int(words.size()) * 12;
                for (std::size_t k = 0; k < words.size(); ++k) {
                    c_.text(words[k], Size::Small, col::muted, x + 74, top + int(k) * 24, Align::Center, 112);
                }
            }
            if (cv.state == tasks::CoverState::Marked) c_.checkBadge(x + 118, y + 28, 16, col::green, col::bg);
            auto lines = c_.wrap(g.title, Size::Small, kTileW - 16);
            if (lines.size() > 2) {
                lines.resize(2);
                lines[1] += " \xE2\x80\xA6";
            }
            for (std::size_t k = 0; k < lines.size(); ++k) {
                c_.text(lines[k], Size::Small, col::text, x + kTileW / 2, y + 138 + int(k) * 22, Align::Center, kTileW - 12);
            }
        }
    }
    // Warm the rows just off screen, one cover per frame, so the next scroll finds them ready.
    for (int row : {markScroll_ + kGridRows, markScroll_ - 1}) {
        for (int cIdx = 0; row >= 0 && cIdx < kGridCols; ++cIdx) {
            const int i = row * kGridCols + cIdx;
            if (i < n) cover(games_[std::size_t(idx[std::size_t(i)])], false);
        }
    }
    // detail strip
    if (const library::Game* cur = currentGame()) {
        const Cover& cv = cover(*cur);
        const bool marked = cv.state == tasks::CoverState::Marked;
        c_.card(kMargin, 560, 1280 - 2 * kMargin, 56, 28, col::card, 2, col::line);
        c_.text(cur->title, Size::Body, col::text, kMargin + 28, 574, Align::Left, 760);
        if (marked) {
            c_.checkBadge(1280 - kMargin - 176, 588, 14, col::green, col::bg);
            c_.text(tr(Id::Marked), Size::Body, col::green, 1280 - kMargin - 146, 574);
        } else if (cur->image.empty()) {
            c_.text(tr(Id::NoCover), Size::Body, col::amber, 1280 - kMargin - 28, 574, Align::Right);
        } else {
            c_.text(tr(Id::NotMarked), Size::Body, col::muted, 1280 - kMargin - 28, 574, Align::Right);
        }
        std::vector<Hint> hs = {{Btn::Confirm, tr(cv.state == tasks::CoverState::Marked ? Id::HintUnmark : Id::HintMark)},
                                {Btn::Back, tr(Id::HintBack)}, {Btn::DPad, tr(Id::HintSelect)}};
        if (tabs_.size() > 1) hs.push_back({Btn::L1, tr(Id::HintPlatform)});
        hs.push_back({Btn::L2, tr(Id::HintPage)});
        hints(hs);
    }
}
