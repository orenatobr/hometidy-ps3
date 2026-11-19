#include "sha1.h"

#include <algorithm>
#include <cstring>

namespace sha1 {
namespace {

struct Ctx {
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::uint8_t block[64];
    std::size_t used = 0;
    std::uint64_t total = 0;
};

std::uint32_t rol(std::uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void compress(Ctx& c, const std::uint8_t* p) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) | (std::uint32_t(p[i * 4 + 2]) << 8) |
               std::uint32_t(p[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3], e = c.h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20) f = (b & cc) | (~b & d), k = 0x5A827999u;
        else if (i < 40) f = b ^ cc ^ d, k = 0x6ED9EBA1u;
        else if (i < 60) f = (b & cc) | (b & d) | (cc & d), k = 0x8F1BBCDCu;
        else f = b ^ cc ^ d, k = 0xCA62C1D6u;
        const std::uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d, d = cc, cc = rol(b, 30), b = a, a = t;
    }
    c.h[0] += a, c.h[1] += b, c.h[2] += cc, c.h[3] += d, c.h[4] += e;
}

void update(Ctx& c, const std::uint8_t* data, std::size_t size) {
    c.total += size;
    while (size > 0) {
        const std::size_t n = std::min(size, sizeof c.block - c.used);
        std::memcpy(c.block + c.used, data, n);
        c.used += n, data += n, size -= n;
        if (c.used == sizeof c.block) compress(c, c.block), c.used = 0;
    }
}

std::string finish(Ctx& c) {
    const std::uint64_t bits = c.total * 8;
    const std::uint8_t one = 0x80, zero = 0;
    update(c, &one, 1);
    while (c.used != 56) update(c, &zero, 1);
    std::uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    update(c, len, 8);
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::uint32_t v : c.h) {
        for (int s = 28; s >= 0; s -= 4) out.push_back(digits[(v >> s) & 0xF]);
    }
    return out;
}

}  // namespace

std::string hex(const std::uint8_t* data, std::size_t size) {
    Ctx c;
    update(c, data, size);
    return finish(c);
}

std::string gitBlob(const std::vector<std::uint8_t>& data) {
    Ctx c;
    const std::string head = "blob " + std::to_string(data.size()) + '\0';
    update(c, reinterpret_cast<const std::uint8_t*>(head.data()), head.size());
    update(c, data.data(), data.size());
    return finish(c);
}

}  // namespace sha1
