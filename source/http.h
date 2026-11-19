// A small HTTP client seen from the app: one download at a time, advanced a little each frame so the
// screen keeps moving. The console build talks through libcurl (http_curl.cpp); the tests use a fake.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace http {

enum class State {
    Idle,
    Running,
    Done,    ///< The server answered; see status().
    Failed,  ///< No answer (no network, TLS failure, too large, cancelled); see error().
};

struct Progress {
    long long done = 0;
    long long total = -1;  ///< -1 when the server did not say.
};

class Client {
public:
    virtual ~Client() = default;

    /// Starts a GET. `maxBytes` caps the body (the transfer fails when the server sends more).
    virtual void start(const std::string& url, std::size_t maxBytes) = 0;

    /// Starts a HEAD: the status and headers of an address without its body (a few hundred bytes).
    virtual void startHead(const std::string& url) = 0;

    /// Advances the transfer without blocking and returns the state.
    virtual State step() = 0;

    virtual Progress progress() const = 0;

    /// HTTP status of a Done transfer (200, 404, 403...).
    virtual int status() const = 0;

    /// A response header of a Done transfer, by lowercase name ("etag", "content-length"); empty when absent.
    virtual std::string header(const std::string& name) const = 0;

    /// Why a transfer is Failed.
    virtual std::string error() const = 0;

    /// Moves the body out of a Done transfer.
    virtual std::vector<std::uint8_t> take() = 0;

    /// Drops the running transfer.
    virtual void cancel() = 0;
};

}  // namespace http
