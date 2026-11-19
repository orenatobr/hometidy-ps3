#include "isoinfo.h"

#include <strings.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace isoinfo {
namespace {

constexpr long kSector = 2048;
constexpr std::uint32_t kMaxDir = 1u << 20;  // a directory bigger than this is not a game image
constexpr std::uint32_t kMaxSfo = 1u << 16;

std::uint32_t le32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (std::uint32_t(p[3]) << 24); }
std::uint16_t le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

bool readAt(std::FILE* f, std::uint64_t offset, std::vector<std::uint8_t>& out, std::size_t size) {
    out.assign(size, 0);
    if (fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0) return false;
    return std::fread(out.data(), 1, size, f) == size;
}

struct Entry {
    std::uint32_t lba = 0, size = 0;
    bool dir = false;
};

// Looks for `name` (case-insensitive, without ";1") in the directory starting at `lba`.
bool find(std::FILE* f, std::uint32_t lba, std::uint32_t size, const char* name, Entry& out) {
    if (size == 0 || size > kMaxDir) return false;
    std::vector<std::uint8_t> d;
    if (!readAt(f, std::uint64_t(lba) * kSector, d, size)) return false;
    std::size_t i = 0;
    while (i + 34 <= d.size()) {
        const std::size_t len = d[i];
        if (len == 0) {  // records never cross a sector: the rest of this one is padding
            i = (i / kSector + 1) * kSector;
            continue;
        }
        if (i + len > d.size() || len < 34) return false;
        const std::size_t nameLen = d[i + 32];
        if (33 + nameLen <= len) {
            std::string n(reinterpret_cast<const char*>(&d[i + 33]), nameLen);
            const auto semi = n.find(';');
            if (semi != std::string::npos) n.resize(semi);
            if (strcasecmp(n.c_str(), name) == 0) {
                out.lba = le32(&d[i + 2]);
                out.size = le32(&d[i + 10]);
                out.dir = (d[i + 25] & 2) != 0;
                return true;
            }
        }
        i += len;
    }
    return false;
}

// Value of a string entry of a PARAM.SFO.
std::string sfoString(const std::vector<std::uint8_t>& sfo, const char* key) {
    if (sfo.size() < 20 || std::memcmp(sfo.data(), "\0PSF", 4) != 0) return "";
    const std::uint32_t keys = le32(&sfo[8]), data = le32(&sfo[12]), count = le32(&sfo[16]);
    for (std::uint32_t i = 0; i < count && 20 + std::size_t(i + 1) * 16 <= sfo.size(); ++i) {
        const std::uint8_t* e = &sfo[20 + std::size_t(i) * 16];
        const std::size_t keyAt = std::size_t(keys) + le16(e), valAt = std::size_t(data) + le32(e + 12), valLen = le32(e + 4);
        if (keyAt >= sfo.size() || valAt + valLen > sfo.size()) continue;
        if (strncmp(reinterpret_cast<const char*>(&sfo[keyAt]), key, sfo.size() - keyAt) != 0) continue;
        std::string v(reinterpret_cast<const char*>(&sfo[valAt]), valLen);
        v.resize(std::strlen(v.c_str()));
        return v;
    }
    return "";
}

}  // namespace

std::string ps3TitleId(const std::string& isoPath) {
    std::FILE* f = std::fopen(isoPath.c_str(), "rb");
    if (!f) return "";
    std::string id;
    std::vector<std::uint8_t> pvd;
    Entry game, sfo;
    if (readAt(f, 16 * kSector, pvd, kSector) && std::memcmp(&pvd[1], "CD001", 5) == 0) {
        const std::uint32_t rootLba = le32(&pvd[158]), rootSize = le32(&pvd[166]);
        std::vector<std::uint8_t> bytes;
        if (find(f, rootLba, rootSize, "PS3_GAME", game) && game.dir && find(f, game.lba, game.size, "PARAM.SFO", sfo) &&
            sfo.size > 0 && sfo.size <= kMaxSfo && readAt(f, std::uint64_t(sfo.lba) * kSector, bytes, sfo.size)) {
            id = sfoString(bytes, "TITLE_ID");
        }
    }
    std::fclose(f);
    if (id.size() < 8 || id.size() > 12) return "";  // not a title id
    for (const char c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return "";  // it goes into a URL
    }
    return id;
}

}  // namespace isoinfo
