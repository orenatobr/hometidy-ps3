#include "frame.h"

#include <algorithm>
#include <math.h>

#include "badge.h"

namespace frame {
namespace {

// Area-averaging resize of premultiplied RGBA (every source pixel counts by the part of it inside the target pixel).
std::vector<float> shrink(const badge::Image& in, int tw, int th) {
    // horizontal pass: in.h rows of tw pixels
    std::vector<float> mid(std::size_t(tw) * in.h * 4, 0.0f);
    const double sx = double(in.w) / tw;
    for (int y = 0; y < in.h; ++y) {
        for (int x = 0; x < tw; ++x) {
            const double a = x * sx, b = (x + 1) * sx;
            float* o = &mid[(std::size_t(y) * tw + x) * 4];
            for (int sxp = int(a); sxp < in.w && sxp < b; ++sxp) {
                const float wf = float(std::min<double>(sxp + 1, b) - std::max<double>(sxp, a));
                const std::uint8_t* p = &in.rgba[(std::size_t(y) * in.w + sxp) * 4];
                const float al = p[3] / 255.0f;
                o[0] += p[0] * al * wf, o[1] += p[1] * al * wf, o[2] += p[2] * al * wf, o[3] += 255.0f * al * wf;
            }
        }
    }
    // vertical pass
    std::vector<float> out(std::size_t(tw) * th * 4, 0.0f);
    const double sy = double(in.h) / th;
    for (int y = 0; y < th; ++y) {
        const double a = y * sy, b = (y + 1) * sy;
        for (int syp = int(a); syp < in.h && syp < b; ++syp) {
            const float w = float(std::min<double>(syp + 1, b) - std::max<double>(syp, a));
            for (int x = 0; x < tw; ++x) {
                const float* p = &mid[(std::size_t(syp) * tw + x) * 4];
                float* o = &out[(std::size_t(y) * tw + x) * 4];
                for (int c = 0; c < 4; ++c) o[c] += p[c] * w;
            }
        }
    }
    const float norm = float(1.0 / (sx * sy));
    for (float& v : out) v *= norm;
    return out;
}

}  // namespace

bool fit(const std::vector<std::uint8_t>& image, int size, std::vector<std::uint8_t>& png) {
    badge::Image src;
    if (!badge::decode(image, src)) return false;
    const double scale = std::min(1.0, std::min(double(size) / src.w, double(size) / src.h));
    const int tw = std::max(1, int(::lround(src.w * scale))), th = std::max(1, int(::lround(src.h * scale)));

    badge::Image canvas;
    canvas.w = canvas.h = size;
    canvas.rgba.assign(std::size_t(size) * size * 4, 0);  // transparent
    const int ox = (size - tw) / 2, oy = (size - th) / 2;
    const bool same = tw == src.w && th == src.h;
    const std::vector<float> small = same ? std::vector<float>() : shrink(src, tw, th);
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            std::uint8_t* d = &canvas.rgba[(std::size_t(y + oy) * size + (x + ox)) * 4];
            if (same) {
                const std::uint8_t* s = &src.rgba[(std::size_t(y) * src.w + x) * 4];
                d[0] = s[0], d[1] = s[1], d[2] = s[2], d[3] = s[3];
                continue;
            }
            const float* s = &small[(std::size_t(y) * tw + x) * 4];
            const float alpha = std::clamp(s[3], 0.0f, 255.0f);
            d[3] = static_cast<std::uint8_t>(::lround(alpha));
            if (alpha > 0.5f) {  // back to straight alpha
                for (int c = 0; c < 3; ++c) d[c] = static_cast<std::uint8_t>(std::clamp(::lround(s[c] * 255.0f / alpha), 0L, 255L));
            }
        }
    }
    return badge::encode(canvas, badge::Format::Png, png);
}

}  // namespace frame
