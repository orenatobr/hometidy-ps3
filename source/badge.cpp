#include "badge.h"

#include <algorithm>
#include <cmath>

#include "mathx.h"

#include "stb_image.h"
#include "stb_image_write.h"

namespace badge {
namespace {

constexpr int kFill[4] = {27, 158, 50, 230};      // #1B9E32, alpha 230 (same as the Python marker)
constexpr int kOutline[4] = {255, 255, 255, 240};
constexpr int kCheck[4] = {255, 255, 255, 255};
constexpr int kSamples = 4;                        // 4x4 samples per pixel for the edges

struct Shape {
    Box box;
    int radius = 0;
    int outline = 0;
    int line = 0;
    double p1x, p1y, p2x, p2y, p3x, p3y;  // the check, in pixel-centre coordinates
};

Shape shapeFor(int w, int h) {
    Shape s;
    s.box = boxFor(w, h);
    const int size = s.box.side;
    s.radius = std::max(4, size / 6);
    s.outline = std::max(1, size / 15);
    s.line = std::max(2, size / 9);
    s.p1x = s.box.x + int(size * 0.22) + 0.5;
    s.p1y = s.box.y + int(size * 0.55) + 0.5;
    s.p2x = s.box.x + int(size * 0.43) + 0.5;
    s.p2y = s.box.y + int(size * 0.75) + 0.5;
    s.p3x = s.box.x + int(size * 0.78) + 0.5;
    s.p3y = s.box.y + int(size * 0.30) + 0.5;
    return s;
}

// Distance from point p to segment ab.
double segDist(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay;
    const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    return mathx::hyp(px - (ax + t * dx), py - (ay + t * dy));
}

// Negative inside a rounded rectangle centred at (cx, cy) with half sizes (hx, hy).
double roundBox(double px, double py, double cx, double cy, double hx, double hy, double r) {
    const double qx = std::fabs(px - cx) - (hx - r);
    const double qy = std::fabs(py - cy) - (hy - r);
    const double ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);
    return mathx::hyp(ox, oy) + std::min(std::max(qx, qy), 0.0) - r;
}

bool near(int a, int b, int tol) { return std::abs(a - b) <= tol; }

const std::uint8_t* px(const Image& im, int x, int y) { return &im.rgba[(std::size_t(y) * im.w + x) * 4]; }
std::uint8_t* px(Image& im, int x, int y) { return &im.rgba[(std::size_t(y) * im.w + x) * 4]; }

void writeCb(void* ctx, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(ctx);
    const auto* p = static_cast<const std::uint8_t*>(data);
    out->insert(out->end(), p, p + size);
}

}  // namespace

Format sniff(const std::vector<std::uint8_t>& f) {
    static const std::uint8_t kPng[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (f.size() >= 8 && std::equal(kPng, kPng + 8, f.begin())) return Format::Png;
    if (f.size() >= 3 && f[0] == 0xFF && f[1] == 0xD8 && f[2] == 0xFF) return Format::Jpeg;
    return Format::Unknown;
}

bool decode(const std::vector<std::uint8_t>& file, Image& out) {
    if (sniff(file) == Format::Unknown || file.size() > 32u * 1024 * 1024) return false;
    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(file.data(), int(file.size()), &w, &h, &comp)) return false;
    if (w < kMinSize || h < kMinSize || w > kMaxSize || h > kMaxSize) return false;
    unsigned char* data = stbi_load_from_memory(file.data(), int(file.size()), &w, &h, &comp, 4);
    if (!data) return false;
    out.w = w;
    out.h = h;
    out.rgba.assign(data, data + std::size_t(w) * h * 4);
    stbi_image_free(data);
    return true;
}

bool encode(const Image& image, Format format, std::vector<std::uint8_t>& out) {
    out.clear();
    if (image.w <= 0 || image.h <= 0 || image.rgba.size() != std::size_t(image.w) * image.h * 4) return false;
    int ok = 0;
    if (format == Format::Png) {
        ok = stbi_write_png_to_func(writeCb, &out, image.w, image.h, 4, image.rgba.data(), image.w * 4);
    } else if (format == Format::Jpeg) {
        // JPEG has no alpha: flatten onto black like a viewer without transparency would.
        std::vector<std::uint8_t> rgb(std::size_t(image.w) * image.h * 3);
        for (std::size_t i = 0; i < std::size_t(image.w) * image.h; ++i) {
            const int a = image.rgba[i * 4 + 3];
            for (int c = 0; c < 3; ++c) rgb[i * 3 + c] = std::uint8_t(image.rgba[i * 4 + c] * a / 255);
        }
        ok = stbi_write_jpg_to_func(writeCb, &out, image.w, image.h, 3, rgb.data(), 95);
    }
    if (!ok) out.clear();
    return ok != 0;
}

Box boxFor(int width, int height) {
    const int base = std::min(width, height);
    Box b;
    b.side = std::max(18, int(base * 0.15));
    b.x = b.y = std::max(6, int(base * 0.02));
    return b;
}

void apply(Image& image) {
    if (image.w < kMinSize || image.h < kMinSize) return;
    const Shape s = shapeFor(image.w, image.h);
    const double side = s.box.side + 1.0;  // the square spans x..x+side inclusive
    const double cx = s.box.x + side / 2, cy = s.box.y + side / 2, half = side / 2;
    const double innerHalf = half - s.outline, innerRadius = std::max(0.0, s.radius - double(s.outline));
    const double halfLine = s.line / 2.0;

    const int x0 = std::max(0, s.box.x - 1), x1 = std::min(image.w - 1, s.box.x + s.box.side + 1);
    const int y0 = std::max(0, s.box.y - 1), y1 = std::min(image.h - 1, s.box.y + s.box.side + 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int covered = 0;
            double sum[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const double qx = x + (sx + 0.5) / kSamples, qy = y + (sy + 0.5) / kSamples;
                    if (roundBox(qx, qy, cx, cy, half, half, s.radius) > 0) continue;
                    const int* c = kOutline;
                    if (roundBox(qx, qy, cx, cy, innerHalf, innerHalf, innerRadius) <= 0) c = kFill;
                    if (segDist(qx, qy, s.p1x, s.p1y, s.p2x, s.p2y) <= halfLine ||
                        segDist(qx, qy, s.p2x, s.p2y, s.p3x, s.p3y) <= halfLine) {
                        c = kCheck;
                    }
                    for (int k = 0; k < 4; ++k) sum[k] += c[k];
                    ++covered;
                }
            }
            if (!covered) continue;
            const double cover = covered / double(kSamples * kSamples);
            std::uint8_t* d = px(image, x, y);
            for (int k = 0; k < 4; ++k) {
                const double target = sum[k] / covered;  // the pixel is replaced, not blended, like the Python marker
                d[k] = std::uint8_t(mathx::iround(d[k] * (1 - cover) + target * cover));
            }
        }
    }
}

void erase(Image& image) {
    if (image.w < kMinSize || image.h < kMinSize) return;
    const Box b = boxFor(image.w, image.h);
    const Image src = image;  // sample from the untouched copy

    // Colours of the ring of pixels just outside the badge square.
    struct Sample {
        double x, y, c[4];
    };
    std::vector<Sample> ring;
    const int lo = -3, hi = b.side + 3;  // ring offsets relative to the square
    auto add = [&](int rx, int ry) {
        const int sx = b.x + rx, sy = b.y + ry;
        if (sx < 0 || sy < 0 || sx >= image.w || sy >= image.h) return;
        Sample s{sx + 0.5, sy + 0.5, {0, 0, 0, 0}};
        const std::uint8_t* p = px(src, sx, sy);
        for (int k = 0; k < 4; ++k) s.c[k] = p[k];
        ring.push_back(s);
    };
    for (int i = lo; i <= hi; i += 2) {
        add(i, lo);
        add(i, hi);
        add(lo, i);
        add(hi, i);
    }
    if (ring.empty()) return;

    for (int y = b.y - 1; y <= b.y + b.side + 1; ++y) {
        for (int x = b.x - 1; x <= b.x + b.side + 1; ++x) {
            if (x < 0 || y < 0 || x >= image.w || y >= image.h) continue;
            double sum[4] = {0, 0, 0, 0}, wsum = 0;
            for (const Sample& s : ring) {
                const double d = mathx::hyp(x + 0.5 - s.x, y + 0.5 - s.y);
                const double w = 1.0 / (d * d * d + 1.0);
                for (int k = 0; k < 4; ++k) sum[k] += w * s.c[k];
                wsum += w;
            }
            std::uint8_t* d4 = px(image, x, y);
            for (int k = 0; k < 4; ++k) d4[k] = std::uint8_t(mathx::iround(sum[k] / wsum));
        }
    }
}

bool isMarked(const Image& image) {
    if (image.w < kMinSize || image.h < kMinSize) return false;
    const Box b = boxFor(image.w, image.h);
    const int side = b.side;
    // Spots inside the green fill that the check never covers.
    const int spots[3][2] = {{side / 2, int(side * 0.15)}, {int(side * 0.85), side / 2}, {int(side * 0.15), int(side * 0.40)}};
    for (const auto& sp : spots) {
        const std::uint8_t* p = px(image, b.x + sp[0], b.y + sp[1]);
        if (!(near(p[0], kFill[0], 24) && near(p[1], kFill[1], 24) && near(p[2], kFill[2], 24) && p[3] >= 200)) return false;
    }
    // and the middle of the white check
    const Shape s = shapeFor(image.w, image.h);
    const int mx = int((s.p2x + s.p3x) / 2), my = int((s.p2y + s.p3y) / 2);
    const std::uint8_t* c = px(image, mx, my);
    return c[0] >= 225 && c[1] >= 225 && c[2] >= 225;
}

}  // namespace badge

namespace badge {

bool sameOutsideBadge(const Image& a, const Image& b) {
    if (a.w != b.w || a.h != b.h || a.rgba.size() != b.rgba.size() || a.w < kMinSize || a.h < kMinSize) return false;
    const Box box = boxFor(a.w, a.h);
    const int x0 = box.x - 1, x1 = box.x + box.side + 1, y0 = box.y - 1, y1 = box.y + box.side + 1;  // what apply() may touch
    for (int y = 0; y < a.h; ++y) {
        for (int x = 0; x < a.w; ++x) {
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) continue;
            const std::uint8_t* p = px(a, x, y);
            const std::uint8_t* q = px(b, x, y);
            if (p[3] == 0 && q[3] == 0) continue;
            for (int k = 0; k < 4; ++k) {
                if (!near(p[k], q[k], 2)) return false;
            }
        }
    }
    return true;
}

}  // namespace badge
