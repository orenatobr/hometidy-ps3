// Console entry point.
#include <sys/process.h>

#include "app.h"
#include "gfx.h"
#include "platform.h"
#include "platform_ps3.h"

SYS_PROCESS_PARAM(1001, 0x200000);

int main(int, char**) {
    platform::Env env = platform::init();
    platform::log("HomeTidy start: storageOk=%d circleConfirms=%d", env.storageOk, env.circleConfirms);

    int width = 0, height = 0;
    if (!ps3::videoInit(width, height)) {
        platform::log("video init FAILED");
        platform::shutdown();
        return 1;
    }
    platform::log("video %dx%d", width, height);

    {
        gfx::Canvas canvas(width, height, platform::fontBytes(false), platform::fontBytes(true));
        platform::log("canvas ready, font=%d", canvas.hasFont());
        App app(canvas, env);
        int idle = 0;
        while (!app.wantsQuit() && !ps3::quitRequested()) {
            ps3::pumpEvents();
            app.update(platform::poll());
            // Draw when something changed; also refresh about twice a second so the system menu can
            // draw over the frame and leave it clean again.
            if (app.takeDirty() || ps3::takeRedraw() || ++idle >= 30) {
                idle = 0;
                app.draw();
                ps3::videoPresent(canvas.pixels());
            }
            platform::sleepMs(16);
        }
    }

    platform::log("main loop ended");
    ps3::videoShutdown();
    platform::shutdown();
    return 0;
}
