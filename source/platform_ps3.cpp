// Console build of the platform layer (PSL1GHT): RSX frame buffer, controllers, system events, files.
#include <malloc.h>
#include <net/net.h>
#include <ppu-types.h>
#include <rsx/rsx.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <time.h>
#include <unistd.h>

#include <io/pad.h>

#include "cacert_bin.h"
#include "fsx.h"
#include "http_curl.h"
#include "inter_bold_ttf.h"
#include "inter_regular_ttf.h"
#include "platform.h"
#include "platform_ps3.h"

namespace {

constexpr u32 kCommandBuffer = 0x100000;
constexpr u32 kHostSize = 32 * 1024 * 1024;
constexpr u32 kLabel = 255;

// ---- video ----
gcmContextData* g_context = nullptr;
u32 g_width = 0, g_height = 0, g_pitch = 0;
u32 g_offset[2] = {0, 0};
u32* g_buffer[2] = {nullptr, nullptr};
u32 g_back = 0;
bool g_firstFlip = true;
u32 g_label = 1;

// ---- system events ----
volatile bool g_quit = false;
volatile bool g_redraw = false;

void onSysutil(u64 status, u64 /*param*/, void* /*usrdata*/) {
    switch (status) {
        case SYSUTIL_EXIT_GAME: g_quit = true; break;
        case SYSUTIL_DRAW_END:
        case SYSUTIL_MENU_CLOSE: g_redraw = true; break;
        default: break;
    }
}

void waitFinish() {
    rsxSetWriteBackendLabel(g_context, kLabel, g_label);
    rsxFlushBuffer(g_context);
    for (int i = 0; i < 100000 && *(vu32*)gcmGetLabelAddress(kLabel) != g_label; ++i) usleep(30);  // about 3 s at most
    ++g_label;
}

void waitIdle() {
    rsxSetWriteBackendLabel(g_context, kLabel, g_label);
    rsxSetWaitLabel(g_context, kLabel, g_label);
    ++g_label;
    waitFinish();
}

void waitFlip() {
    for (int i = 0; i < 10000 && gcmGetFlipStatus() != 0; ++i) usleep(200);  // about 2 s at most
    gcmResetFlipStatus();
}

// ---- controllers ----
bool g_padReady = false;
struct Held {
    bool now = false;
    int frames = 0;
};
Held g_held[12];  // up down left right a b x y l r l2 r2
bool g_circleConfirms = false;
padData g_lastPad[7];  // ioPadGetData only fills the data when the input changed: keep the last frame per port
bool g_lastValid[7] = {false, false, false, false, false, false, false};
std::string g_appDir;
FILE* g_log = nullptr;

// Directions repeat after 20 frames, then every 5; buttons fire once per press.
bool fire(Held& h, bool down, bool repeat) {
    const bool was = h.now;
    h.now = down;
    if (!down) {
        h.frames = 0;
        return false;
    }
    if (!was) {
        h.frames = 0;
        return true;
    }
    ++h.frames;
    return repeat && h.frames >= 20 && (h.frames - 20) % 5 == 0;
}

}  // namespace

namespace ps3 {

bool videoInit(int& width, int& height) {
    void* host = memalign(1024 * 1024, kHostSize);
    if (!host) return false;
    if (rsxInit(&g_context, kCommandBuffer, kHostSize, host) != 0) return false;

    videoState state;
    if (videoGetState(0, 0, &state) != 0) return false;
    videoResolution res;
    if (videoGetResolution(state.displayMode.resolution, &res) != 0) return false;
    if (res.width < 640 || res.height < 480) return false;

    videoConfiguration config;
    memset(&config, 0, sizeof config);
    config.resolution = state.displayMode.resolution;
    config.format = VIDEO_BUFFER_FORMAT_XRGB;
    config.pitch = res.width * sizeof(u32);

    waitIdle();
    if (videoConfigure(0, &config, nullptr, 0) != 0) return false;
    videoGetState(0, 0, &state);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    g_width = res.width;
    g_height = res.height;
    g_pitch = g_width * sizeof(u32);
    for (int i = 0; i < 2; ++i) {
        g_buffer[i] = static_cast<u32*>(rsxMemalign(64, g_height * g_pitch));
        if (!g_buffer[i]) return false;
        memset(g_buffer[i], 0, g_height * g_pitch);
        rsxAddressToOffset(g_buffer[i], &g_offset[i]);
        gcmSetDisplayBuffer(i, g_offset[i], g_pitch, g_width, g_height);
    }
    width = int(g_width);
    height = int(g_height);
    return true;
}

void videoPresent(const std::uint32_t* pixels) {
    if (!g_context) return;
    if (!g_firstFlip) waitFlip();  // the buffer we are about to write is no longer on screen
    else gcmResetFlipStatus();
    memcpy(g_buffer[g_back], pixels, std::size_t(g_height) * g_pitch);
    gcmSetFlip(g_context, g_back);
    rsxFlushBuffer(g_context);
    gcmSetWaitFlip(g_context);
    g_back ^= 1;
    g_firstFlip = false;
}

void videoShutdown() {
    if (!g_context) return;
    if (g_firstFlip) {  // no flip was ever queued: do not wait for one
        waitFinish();
        return;
    }
    waitFlip();
    gcmSetWaitFlip(g_context);
    rsxFinish(g_context, 1);
}

void pumpEvents() { sysUtilCheckCallback(); }

bool quitRequested() { return g_quit; }

bool takeRedraw() {
    const bool r = g_redraw;
    g_redraw = false;
    return r;
}

}  // namespace ps3

namespace platform {

Env init() {
    Env env;
    env.appDir = "/dev_hdd0/hometidy";
    env.xmlPath = "/dev_hdd0/xmlhost/game_plugin/mygames.xml";
    env.roots = {{"/dev_hdd0/PS3ISO", library::Platform::Ps3},
                 {"/dev_hdd0/PS2ISO", library::Platform::Ps2},
                 {"/dev_hdd0/PSXISO", library::Platform::Psx}};
    // Games kept on an external drive show up too (webMAN mounts them from /dev_usb00x).
    for (int i = 0; i < 4; ++i) {
        char base[32];
        snprintf(base, sizeof base, "/dev_usb00%d", i);
        env.roots.push_back({std::string(base) + "/PS3ISO", library::Platform::Ps3});
        env.roots.push_back({std::string(base) + "/PS2ISO", library::Platform::Ps2});
        env.roots.push_back({std::string(base) + "/PSXISO", library::Platform::Psx});
    }
    env.storageOk = fsx::isDir("/dev_hdd0");

    s32 value = 0;
    if (sysUtilGetSystemParamInt(SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value) == 0) env.circleConfirms = value == 0;
    g_circleConfirms = env.circleConfirms;
    g_appDir = env.appDir;

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, onSysutil, nullptr);
    ioPadInit(7);
    g_padReady = true;

    // Keep the last two runs of the log: a crash leaves the previous one readable.
    fsx::makeDirs(env.appDir);
    fsx::removeFile(env.appDir + "/hometidy.prev.log");
    fsx::renameFile(env.appDir + "/hometidy.log", env.appDir + "/hometidy.prev.log");
    g_log = fopen((env.appDir + "/hometidy.log").c_str(), "w");
    return env;
}

std::unique_ptr<http::Client> makeHttp(const Env& env) {
    static bool netUp = false;
    if (!netUp) netUp = netInitialize() == 0;
    // curl 7.64 reads the trusted roots from a file: write the bundle that is built into the app.
    const std::string ca = env.appDir + "/cacert.pem";
    fsx::makeDirs(env.appDir);
    if (!fsx::writeBytes(ca, std::vector<std::uint8_t>(cacert_bin, cacert_bin + cacert_bin_size))) log("could not write %s", ca.c_str());
    return http::makeCurlClient(ca, std::string("HomeTidy-PS3/") + APP_VERSION);
}

void shutdown() {
    if (g_log) fclose(g_log);
    g_log = nullptr;
    if (g_padReady) ioPadEnd();
    g_padReady = false;
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
}

Input poll() {
    bool up = false, down = false, left = false, right = false;
    bool cross = false, circle = false, tri = false, sq = false, l1 = false, r1 = false, l2 = false, r2 = false;
    if (g_padReady) {
        padInfo info;
        ioPadGetInfo(&info);
        for (int i = 0; i < 7; ++i) {
            if (!info.status[i]) {
                g_lastValid[i] = false;
                continue;
            }
            padData fresh;
            memset(&fresh, 0, sizeof fresh);
            if (ioPadGetData(i, &fresh) == 0 && fresh.len > 0) {
                g_lastPad[i] = fresh;
                g_lastValid[i] = true;
            }
            if (!g_lastValid[i]) continue;  // nothing read yet: treat as neutral
            const padData& d = g_lastPad[i];
            up |= d.BTN_UP;
            down |= d.BTN_DOWN;
            left |= d.BTN_LEFT;
            right |= d.BTN_RIGHT;
            cross |= d.BTN_CROSS;
            circle |= d.BTN_CIRCLE;
            tri |= d.BTN_TRIANGLE;
            sq |= d.BTN_SQUARE;
            l1 |= d.BTN_L1;
            r1 |= d.BTN_R1;
            l2 |= d.BTN_L2;
            r2 |= d.BTN_R2;
            // left stick as a d-pad (centre is about 128)
            left |= d.ANA_L_H < 60;
            right |= d.ANA_L_H > 200;
            up |= d.ANA_L_V < 60;
            down |= d.ANA_L_V > 200;
        }
    }
    Input in;
    in.up = fire(g_held[0], up, true);
    in.down = fire(g_held[1], down, true);
    in.left = fire(g_held[2], left, true);
    in.right = fire(g_held[3], right, true);
    in.a = fire(g_held[4], g_circleConfirms ? circle : cross, false);
    in.b = fire(g_held[5], g_circleConfirms ? cross : circle, false);
    in.x = fire(g_held[6], tri, false);
    in.y = fire(g_held[7], sq, false);
    in.l = fire(g_held[8], l1, false);
    in.r = fire(g_held[9], r1, false);
    in.l2 = fire(g_held[10], l2, true);
    in.r2 = fire(g_held[11], r2, true);
    return in;
}

std::vector<std::uint8_t> fontBytes(bool bold) {
    if (bold) return std::vector<std::uint8_t>(inter_bold_ttf, inter_bold_ttf + inter_bold_ttf_size);
    return std::vector<std::uint8_t>(inter_regular_ttf, inter_regular_ttf + inter_regular_ttf_size);
}

std::string timestamp() {
    s32 tz = 0, summer = 0;
    sysUtilGetSystemParamInt(SYSUTIL_SYSTEMPARAM_ID_TIMEZONE, &tz);      // minutes from UTC
    sysUtilGetSystemParamInt(SYSUTIL_SYSTEMPARAM_ID_SUMMERTIME, &summer);  // 1 = daylight saving time
    const time_t t = time(nullptr) + tz * 60 + (summer ? 3600 : 0);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char buf[32];
    strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", &tmv);
    return buf;
}

void sleepMs(int ms) {
    for (; ms > 0; ms -= 50) {  // keep serving system events (PS button, quit) while waiting
        sysUtilCheckCallback();
        usleep(static_cast<useconds_t>(ms < 50 ? ms : 50) * 1000);
    }
}

void log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

}  // namespace platform
