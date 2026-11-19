// Everything that differs between the console and the host preview build.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "http.h"
#include "library.h"

namespace platform {

/// Controller state for one frame. Directions repeat while held; the other buttons fire once per press.
/// Face buttons are named by role: `a` confirms and `b` goes back (cross and circle, or swapped,
/// following the console's "enter button" setting).
struct Input {
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, x = false, y = false;  ///< x = triangle, y = square
    bool l = false, r = false, l2 = false, r2 = false;

    bool any() const { return up || down || left || right || a || b || x || y || l || r || l2 || r2; }
};

/// Where things live; filled by init().
struct Env {
    std::string appDir;                ///< Backups, config, series.txt and the log live here.
    std::string xmlPath;               ///< The webMAN game list (mygames.xml).
    std::vector<library::Root> roots;  ///< ISO folders to scan for covers.
    bool storageOk = false;            ///< The console storage could be opened.
    bool circleConfirms = false;       ///< Circle is "enter" (Japanese layout) instead of cross.
    bool systemPortuguese = false;     ///< The console language is Portuguese.
};

/// Opens the storage and controllers.
Env init();

/// Opens the network and returns the HTTP client the cover update uses. The client always verifies the server
/// certificate. When the network cannot be opened (or on the host preview) the client fails every request.
std::unique_ptr<http::Client> makeHttp(const Env& env);

/// Releases everything opened by init().
void shutdown();

/// Reads the controllers once per frame.
Input poll();

/// Bytes of the regular or bold TrueType face. Valid until shutdown().
std::vector<std::uint8_t> fontBytes(bool bold);

/// "YYYYMMDD_HHMMSS" in local time.
std::string timestamp();

/// Sleeps for the given time.
void sleepMs(int ms);

/// Logs a line to the app log (and to stderr on the host).
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace platform
