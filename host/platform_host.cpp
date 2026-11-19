// Host build of the platform layer: files live under $HOMETIDY_ROOT, fonts come from data.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

#include <functional>
#include <map>

#include "badge.h"
#include "covers.h"
#include "fsx.h"
#include "platform.h"
#include "sha1.h"

namespace platform {
namespace {
std::string g_root;
FILE* g_log = nullptr;
}  // namespace

Env init() {
    const char* root = std::getenv("HOMETIDY_ROOT");
    g_root = root ? root : "/tmp/hometidy_fake";
    Env env;
    env.appDir = g_root + "/dev_hdd0/hometidy";
    env.xmlPath = g_root + "/dev_hdd0/xmlhost/game_plugin/mygames.xml";
    env.roots = {{g_root + "/dev_hdd0/PS3ISO", library::Platform::Ps3},
                 {g_root + "/dev_hdd0/PS2ISO", library::Platform::Ps2},
                 {g_root + "/dev_hdd0/PSXISO", library::Platform::Psx}};
    env.storageOk = fsx::isDir(g_root + "/dev_hdd0");
    env.circleConfirms = std::getenv("HOMETIDY_CIRCLE") != nullptr;
    fsx::makeDirs(env.appDir);
    g_log = std::fopen((env.appDir + "/hometidy.log").c_str(), "w");
    return env;
}

namespace {
// The host preview has no network: every transfer fails, which is what the screen shows.
class OfflineClient : public http::Client {
public:
    void start(const std::string&, std::size_t) override { failed_ = true; }
    http::State step() override { return failed_ ? http::State::Failed : http::State::Idle; }
    http::Progress progress() const override { return {}; }
    int status() const override { return 0; }
    std::string error() const override { return "no network on the host"; }
    std::vector<std::uint8_t> take() override { return {}; }
    void cancel() override { failed_ = false; }

private:
    bool failed_ = false;
};
}  // namespace

// HOMETIDY_DEMO_NET=1: serves a made-up repository for the games of the fake console, slowly, so the preview
// can show the progress screen. The images are not real pictures.
class DemoClient : public http::Client {
public:
    explicit DemoClient(std::vector<library::Root> roots) : roots_(std::move(roots)) {}
    void start(const std::string& url, std::size_t) override {
        steps_ = 0;
        body_.clear();
        total_ = 0;
        const auto tree = url.find("/git/trees/");
        if (tree != std::string::npos) {
            const std::string ref = url.substr(tree + 11);
            std::string json;
            if (ref == "master") {
                json = R"({"tree":[{"path":"Named_Boxarts","type":"tree","sha":"demo"}],"truncated":false})";
            } else {
                json = R"({"tree":[)";
                bool first = true;
                for (const auto& root : roots_) {
                    if (url.find(covers::sourceFor(root.platform).repo) == std::string::npos) continue;
                    for (const std::string& n : fsx::listDir(root.path)) {
                        if (n.size() < 5 || n.substr(n.size() - 4) != ".iso") continue;
                        const std::string stem = n.substr(0, n.size() - 4);
                        json += std::string(first ? "" : ",") + R"({"path":")" + stem + R"(.png","mode":"100644","type":"blob","sha":")" + sha1::gitBlob(image(stem)) + R"("})";
                        first = false;
                    }
                }
                json += R"(],"truncated":false})";
            }
            body_.assign(json.begin(), json.end());
            total_ = static_cast<long long>(body_.size());
            need_ = 4;
        } else {
            std::string name = url.substr(url.rfind('/') + 1);
            name = decode(name.substr(0, name.size() - 4));
            body_ = image(name);
            total_ = static_cast<long long>(body_.size());
            need_ = 14;
        }
        state_ = http::State::Running;
    }
    http::State step() override {
        if (state_ == http::State::Running && ++steps_ >= need_) state_ = http::State::Done;
        return state_;
    }
    http::Progress progress() const override { return {total_ * std::min(steps_, need_) / std::max(need_, 1), total_}; }
    int status() const override { return 200; }
    std::string error() const override { return ""; }
    std::vector<std::uint8_t> take() override { return std::move(body_); }
    void cancel() override { state_ = http::State::Idle; }

private:
    static std::vector<std::uint8_t> image(const std::string& name) {
        badge::Image img;
        img.w = 320, img.h = 440;
        img.rgba.resize(std::size_t(img.w) * img.h * 4);
        const unsigned seed = static_cast<unsigned>(std::hash<std::string>{}(name));
        for (int y = 0; y < img.h; ++y) {
            for (int x = 0; x < img.w; ++x) {
                std::uint8_t* p = &img.rgba[(std::size_t(y) * img.w + x) * 4];
                p[0] = static_cast<std::uint8_t>(seed + x), p[1] = static_cast<std::uint8_t>((seed >> 8) + y), p[2] = static_cast<std::uint8_t>((seed >> 16) + x / 2 + y / 3), p[3] = 255;
            }
        }
        std::vector<std::uint8_t> out;
        badge::encode(img, badge::Format::Png, out);
        return out;
    }
    static std::string decode(const std::string& s) {
        std::string out;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 2 < s.size()) {
                out.push_back(static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16)));
                i += 2;
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }
    std::vector<library::Root> roots_;
    http::State state_ = http::State::Idle;
    std::vector<std::uint8_t> body_;
    long long total_ = 0;
    int steps_ = 0, need_ = 1;
};

std::unique_ptr<http::Client> makeHttp(const Env& env) {
    if (std::getenv("HOMETIDY_DEMO_NET")) return std::make_unique<DemoClient>(env.roots);
    return std::make_unique<OfflineClient>();
}

void shutdown() {
    if (g_log) std::fclose(g_log);
    g_log = nullptr;
}

Input poll() { return {}; }

std::vector<std::uint8_t> fontBytes(bool bold) {
    const char* dir = std::getenv("HOMETIDY_FONTS");
    std::vector<std::uint8_t> bytes;
    fsx::readBytes(std::string(dir ? dir : "data") + (bold ? "/inter_bold.ttf" : "/inter_regular.ttf"), bytes);
    return bytes;
}

std::string timestamp() {
    const std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char line[512];
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[hometidy] %s\n", line);
    if (g_log) {
        std::fprintf(g_log, "%s\n", line);
        std::fflush(g_log);
    }
}

}  // namespace platform
