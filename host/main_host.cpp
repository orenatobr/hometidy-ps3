// Host preview: plays a scripted tour of the UI against a fake console tree and saves a PNG per step.
// Usage: HOMETIDY_ROOT=<fake console dir> HOMETIDY_SHOTS=<out dir> hometidy_host [width height]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app.h"
#include "fsx.h"
#include "gfx.h"
#include "platform.h"
#include "stb_image_write.h"

namespace {

struct Step {
    platform::Input in;
    const char* shot;  // file name to save after the step, or nullptr
    int wait;          // extra frames before capturing
};

platform::Input key(char k) {
    platform::Input i;
    switch (k) {
        case 'A': i.a = true; break;
        case 'B': i.b = true; break;
        case 'X': i.x = true; break;
        case 'Y': i.y = true; break;
        case 'L': i.l = true; break;
        case 'R': i.r = true; break;
        case '1': i.l2 = true; break;
        case '2': i.r2 = true; break;
        case 'u': i.up = true; break;
        case 'd': i.down = true; break;
        case 'l': i.left = true; break;
        case 'r': i.right = true; break;
        default: break;
    }
    return i;
}

void save(const gfx::Canvas& c, const std::string& path) {
    std::vector<std::uint8_t> rgb(std::size_t(c.width()) * c.height() * 3);
    for (std::size_t i = 0; i < std::size_t(c.width()) * c.height(); ++i) {
        const std::uint32_t p = c.pixels()[i];
        rgb[i * 3] = std::uint8_t(p >> 16);
        rgb[i * 3 + 1] = std::uint8_t(p >> 8);
        rgb[i * 3 + 2] = std::uint8_t(p);
    }
    std::vector<std::uint8_t> png;
    stbi_write_png_to_func(
        [](void* ctx, void* data, int size) {
            auto* out = static_cast<std::vector<std::uint8_t>*>(ctx);
            out->insert(out->end(), static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + size);
        },
        &png, c.width(), c.height(), 3, rgb.data(), c.width() * 3);
    fsx::writeBytes(path, png);
}

}  // namespace

int main(int argc, char** argv) {
    const int width = argc > 2 ? std::atoi(argv[1]) : 1280, height = argc > 2 ? std::atoi(argv[2]) : 720;
    platform::Env env = platform::init();
    gfx::Canvas canvas(width, height, platform::fontBytes(false), platform::fontBytes(true));
    if (!canvas.hasFont()) {
        std::fprintf(stderr, "font not found (set HOMETIDY_FONTS)\n");
        return 1;
    }
    App app(canvas, env);
    const char* dir = std::getenv("HOMETIDY_SHOTS");
    if (!dir) {
        std::fprintf(stderr, "set HOMETIDY_SHOTS to an output directory\n");
        return 1;
    }
    fsx::makeDirs(dir);
    const std::string out = dir;
    // Tour: home, sort preview, other platform, scroll, confirm, done, mark grid, mark, unmark, Portuguese.
    const std::vector<Step> tour = {
        {{}, "01_home.png", 6},          {key('A'), "02_sort.png", 2},      {key('R'), "03_sort_ps2.png", 1},
        {key('L'), nullptr, 0},          {key('2'), "04_sort_scroll.png", 1}, {key('A'), "05_confirm.png", 1},
        {key('A'), nullptr, 0},          {{}, "06_done.png", 12},           {key('B'), "07_home_after.png", 1},
        {key('r'), nullptr, 0},          {key('A'), "08_mark.png", 2},      {key('A'), "09_marked.png", 2},
        {key('A'), "10_unmarked.png", 2}, {key('R'), "11_mark_ps2.png", 2}, {key('d'), nullptr, 0},
        {key('r'), "12_mark_row2.png", 2}, {key('B'), nullptr, 0},          {key('Y'), "13_home_pt.png", 2},
    };
    for (const Step& s : tour) {
        app.update(s.in);
        for (int i = 0; i < s.wait; ++i) app.update({});
        app.draw();
        if (s.shot) save(canvas, out + "/" + s.shot);
    }
    platform::shutdown();
    return 0;
}
