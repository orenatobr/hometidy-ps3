// Host build of the platform layer: files live under $HOMETIDY_ROOT, fonts come from data.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

#include "fsx.h"
#include "platform.h"

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
    env.systemPortuguese = std::getenv("HOMETIDY_LANG_PT") != nullptr;
    fsx::makeDirs(env.appDir);
    g_log = std::fopen((env.appDir + "/hometidy.log").c_str(), "w");
    return env;
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
