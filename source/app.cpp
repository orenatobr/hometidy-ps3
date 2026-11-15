#include "app.h"

#include <algorithm>
#include <cstdio>

#include "badge.h"
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
constexpr int kTileGap = 14;
constexpr const char* kVersion = "v1.0.0";

const gfx::Color kBlue{125, 159, 214};
const gfx::Color kPink{217, 140, 179};
const gfx::Color kRose{224, 112, 124};
const gfx::Color kMint{91, 185, 140};

std::string fmt(const char* f, int a) {
    char b[256];
    std::snprintf(b, sizeof b, f, a);
    return b;
}

std::string tr(Id id) { return strings::get(id); }

}  // namespace

App::App(gfx::Canvas& canvas, platform::Env env) : c_(canvas), env_(std::move(env)) {}

// ------------------------------------------------------------- lifecycle --

void App::loadConfig() {
    strings::set(env_.systemPortuguese ? strings::Lang::Pt : strings::Lang::En);
    std::string text;
    if (!fsx::readFile(env_.appDir + "/config.txt", text)) return;
    if (text.find("lang=pt") != std::string::npos) strings::set(strings::Lang::Pt);
    if (text.find("lang=en") != std::string::npos) strings::set(strings::Lang::En);
}

void App::saveConfig() {
    const std::string text = strings::current() == strings::Lang::Pt ? "lang=pt\n" : "lang=en\n";
    fsx::makeDirs(env_.appDir);
    fsx::writeBytes(env_.appDir + "/config.txt", std::vector<std::uint8_t>(text.begin(), text.end()));
}

void App::load() {
    loadConfig();

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
        platform::log("game list %s: %zu bytes, valid=%d, games=%d, to move=%d", env_.xmlPath.c_str(), xmlText_.size(),
                      plan.has_value(), listGames_, listMoves_);
    } else {
        platform::log("game list %s: not found", env_.xmlPath.c_str());
    }

    games_ = library::scan(env_.roots);
    std::stable_sort(games_.begin(), games_.end(), [&](const library::Game& a, const library::Game& b) {
        return sorter::less(sorter::makeKey(a.name, rules_), sorter::makeKey(b.name, rules_));
    });
    for (const library::Platform p : {library::Platform::Ps3, library::Platform::Ps2, library::Platform::Psx}) {
        if (std::any_of(games_.begin(), games_.end(), [&](const library::Game& g) { return g.platform == p; })) tabs_.push_back(p);
    }
    platform::log("loaded: rules=%zu games=%zu tabs=%zu storageOk=%d", rules_.size(), games_.size(), tabs_.size(), env_.storageOk);
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
    if (in.y && screen_ == Screen::Home) {
        strings::set(strings::current() == strings::Lang::En ? strings::Lang::Pt : strings::Lang::En);
        saveConfig();
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
        case Screen::Message:
            if (in.a || in.b) screen_ = msgBack_;
            break;
        default: break;
    }
}

void App::updateHome(const platform::Input& in) {
    if (in.left || in.right || in.up || in.down) homeFocus_ = 1 - homeFocus_;
    if (in.b) quit_ = true;
    if (!in.a) return;
    if (!env_.storageOk) {
        toast(tr(Id::NoAccess), col::red);
        return;
    }
    if (homeFocus_ == 0) enterSort();
    else enterMark();
}

void App::enterSort() {
    std::string xml;
    if (!fsx::readFile(env_.xmlPath, xml)) {
        showMessage(tr(Id::SortTitle), tr(Id::NoList), col::amber, Screen::Home);
        return;
    }
    plan_ = tasks::planSort(xml, rules_);
    platform::log("sort plan: %zu bytes read, valid=%d, segments=%zu, changed=%d", xml.size(), plan_.has_value(),
                  plan_ ? plan_->segments.size() : 0, plan_ ? plan_->changedSegments : 0);
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

App::Cover& App::cover(const library::Game& g) {
    auto it = covers_.find(g.image);
    if (it != covers_.end()) return it->second;
    Cover cv;
    std::vector<std::uint8_t> file;
    badge::Image img;
    if (!g.image.empty() && fsx::readBytes(g.image, file, 16u * 1024 * 1024) && badge::decode(file, img)) {
        cv.tex = c_.texture(img.rgba, img.w, img.h);
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
    const int cardW = 560, cardH = 400, y = 130;
    for (int i = 0; i < 2; ++i) {
        const int x = kMargin + i * (cardW + 32);
        const bool focus = homeFocus_ == i;
        if (focus) c_.fill(x - 6, y - 6, cardW + 12, cardH + 12, 30, col::green);
        c_.card(x, y, cardW, cardH, 24, focus ? col::raised : col::card, 2, col::line);

        const int ix = x + 40, iy = y + 40;
        if (i == 0) {  // bars shrinking = sorted list
            const int widths[4] = {88, 70, 52, 34};
            for (int k = 0; k < 4; ++k) c_.fill(ix, iy + k * 24, widths[k], 14, 7, k == 0 ? col::green : col::muted);
        } else {  // cover tile with a check
            c_.fill(ix, iy, 92, 92, 16, col::line);
            c_.fill(ix + 12, iy + 12, 68, 68, 10, col::card);
            c_.checkBadge(ix + 74, iy + 18, 26, col::green, col::bg);
        }
        c_.text(tr(i == 0 ? Id::SortTitle : Id::MarkTitle), Size::Large, col::text, x + 40, y + 152);
        const auto lines = c_.wrap(tr(i == 0 ? Id::SortDesc : Id::MarkDesc), Size::Body, cardW - 80);
        for (std::size_t k = 0; k < lines.size() && k < 4; ++k) {
            c_.text(lines[k], Size::Body, col::muted, x + 40, y + 204 + int(k) * 34);
        }
        const std::string status = i == 0 ? fmt(tr(Id::SortStatus).c_str(), listGames_) : fmt(tr(Id::MarkStatus).c_str(), int(games_.size()));
        const int sw = c_.textWidth(status, Size::Small) + 36;
        c_.fill(x + 40, y + cardH - 70, sw, 40, 20, focus ? col::card : col::raised);
        c_.text(status, Size::Small, col::text, x + 40 + sw / 2, y + cardH - 70 + 7, Align::Center);
        if (i == 0 && listGames_ > 0) {
            const std::string note = listMoves_ > 0 ? fmt(tr(Id::ListMoves).c_str(), listMoves_) : tr(Id::ListSorted);
            c_.text(note, Size::Small, listMoves_ > 0 ? col::amber : col::green, x + 40 + sw + 16, y + cardH - 70 + 7);
        }
    }
    if (!env_.storageOk) c_.text(tr(Id::NoAccess), Size::Small, col::red, kMargin, 558, Align::Left, 1150);
    else if (listGames_ == 0) c_.text(tr(Id::NoList), Size::Small, col::amber, kMargin, 558, Align::Left, 1150);
    else c_.text(tr(Id::ListFile) + ": " + env_.xmlPath, Size::Small, col::muted, kMargin, 558, Align::Left, 1150);
    hints({{Btn::DPad, tr(Id::HintSelect)}, {Btn::Confirm, tr(Id::HintApply)}, {Btn::Square, tr(Id::HintLanguage)},
           {Btn::Back, tr(Id::HintExit)}});
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
            Cover& cv = cover(g);
            if (cv.tex) {
                c_.blit(*cv.tex, x + 12, y + 10, 124, 124, 8);
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
