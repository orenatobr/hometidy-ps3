// HomeTidy screens and navigation.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "coversync.h"
#include "gfx.h"
#include "library.h"
#include "platform.h"
#include "sorter.h"
#include "tasks.h"

class App {
public:
    enum class Screen { Loading, Home, SortPreview, SortConfirm, SortDone, Mark, Covers, Message };

    /// Args:
    ///     canvas: Drawing surface; must outlive the app.
    ///     env: Storage locations from platform::init().
    App(gfx::Canvas& canvas, platform::Env env);

    /// Handles one frame of input.
    void update(const platform::Input& in);
    /// Draws the current screen.
    void draw();
    /// True once after something changed that needs a new frame.
    bool takeDirty();
    bool wantsQuit() const { return quit_; }
    Screen screen() const { return screen_; }

private:
    struct Cover {
        std::shared_ptr<gfx::Texture> tex;
        tasks::CoverState state = tasks::CoverState::Unreadable;
        bool loading = false;  ///< Not decoded yet (frame budget spent); drawn as a blank tile.
    };
    enum class Btn { Confirm, Back, Triangle, Square, L1, R1, L2, R2, DPad };
    struct Hint {
        Btn btn;
        std::string label;
    };

    // lifecycle
    void load();
    void loadConfig();
    void scanGames();
    void saveConfig();
    // screens: update + draw
    void updateHome(const platform::Input& in);
    void drawHome();
    void enterSort();
    void updateSort(const platform::Input& in);
    void drawSort();
    void updateConfirm(const platform::Input& in);
    void drawConfirm();
    void applySort();
    void drawDone();
    void enterMark();
    void updateMark(const platform::Input& in);
    void drawMark();
    void toggleMark();
    void enterCovers();
    void updateCovers(const platform::Input& in);
    void drawCovers();
    void startCovers();
    void drawBar(int x, int y, int w, double fraction, bool unknown);
    void drawMessage();
    // shared pieces
    void header(const std::string& subtitle);
    void hints(const std::vector<Hint>& items);
    int drawButton(Btn b, int x, int y);
    void toastDraw();
    void toast(const std::string& text, gfx::Color color);
    void showMessage(const std::string& title, const std::string& body, gfx::Color color, Screen back);
    /// Cached cover. A miss decodes the image unless `urgent` is false and this frame's decode budget is spent;
    /// then a blank "loading" cover comes back and the next frame is requested.
    Cover& cover(const library::Game& g, bool urgent = true);
    void dropCover(const std::string& path);
    std::string statusText(tasks::Status s) const;
    const library::Game* currentGame() const;
    std::vector<int> tabGames() const;

    gfx::Canvas& c_;
    platform::Env env_;
    Screen screen_ = Screen::Loading;
    bool quit_ = false;
    bool dirty_ = true;
    int frame_ = 0;
    Screen lastLogged_ = Screen::Message;  ///< Last screen written to the log.
    int busy_ = 0;  ///< Frames to wait so the "Working..." overlay shows before a slow job.

    // data
    std::vector<sorter::Rule> rules_;
    std::string xmlText_;
    int listGames_ = 0;
    int listMoves_ = 0;
    std::vector<library::Game> games_;  ///< Sorted like the list, covers or not.
    std::vector<library::Platform> tabs_;
    std::map<std::string, Cover> covers_;
    int decodeBudget_ = 0;  ///< Cover decodes still allowed in the frame being drawn.

    // home
    int homeFocus_ = 0;
    // sort
    std::optional<tasks::SortPlan> plan_;
    int segSel_ = 0;
    int rowSel_ = 0;
    tasks::Result sortResult_;
    bool recheckOk_ = false;
    // mark
    int tab_ = 0;
    int markSel_ = 0;
    int markScroll_ = 0;
    // covers
    std::unique_ptr<http::Client> http_;
    std::unique_ptr<coversync::Sync> sync_;
    // message / toast
    std::string msgTitle_, msgBody_;
    gfx::Color msgColor_ = gfx::theme::red;
    Screen msgBack_ = Screen::Home;
    std::string toastText_;
    gfx::Color toastColor_ = gfx::theme::green;
    int toastFrames_ = 0;
};
