#include "covers.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace covers {
namespace {

const Source kSources[] = {
    {library::Platform::Ps3, "libretro-thumbnails/Sony_-_PlayStation_3", "master", "Named_Boxarts"},
    {library::Platform::Ps2, "libretro-thumbnails/Sony_-_PlayStation_2", "master", "Named_Boxarts"},
    {library::Platform::Psx, "libretro-thumbnails/Sony_-_PlayStation", "master", "Named_Boxarts"},
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Minimal JSON reader: just enough for the flat objects of a git tree answer.
struct Reader {
    const std::string& s;
    std::size_t i = 0;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }
    bool eat(char c) {
        ws();
        if (i < s.size() && s[i] == c) return ++i, true;
        return false;
    }
    static int hexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    bool hex4(unsigned& v) {
        if (i + 4 > s.size()) return false;
        v = 0;
        for (int k = 0; k < 4; ++k) {
            const int h = hexVal(s[i + std::size_t(k)]);
            if (h < 0) return false;
            v = v * 16 + unsigned(h);
        }
        i += 4;
        return true;
    }
    bool string(std::string& out) {
        out.clear();
        if (!eat('"')) return false;
        while (i < s.size()) {
            const char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i >= s.size()) return false;
            const char e = s[i++];
            switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        i += 2;
                        unsigned lo = 0;
                        if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: out.push_back(e); break;  // \" \\ \/
            }
        }
        return false;
    }
    // Skips any value (used for the fields this reader does not keep).
    bool skip() {
        ws();
        if (i >= s.size()) return false;
        if (s[i] == '"') {
            std::string tmp;
            return string(tmp);
        }
        if (s[i] == '{' || s[i] == '[') {
            const char open = s[i], close = open == '{' ? '}' : ']';
            ++i;
            if (eat(close)) return true;
            do {
                if (open == '{') {
                    std::string key;
                    if (!string(key) || !eat(':')) return false;
                }
                if (!skip()) return false;
            } while (eat(','));
            return eat(close);
        }
        while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' && s[i] != '\n') ++i;  // number, true, false, null
        return true;
    }
};

}  // namespace

const Source& sourceFor(library::Platform p) {
    for (const Source& s : kSources) {
        if (s.platform == p) return s;
    }
    return kSources[0];
}

bool parseTree(const std::string& json, std::vector<TreeItem>& out) {
    out.clear();
    Reader r{json};
    if (!r.eat('{')) return false;
    bool sawTree = false;
    if (r.eat('}')) return false;
    do {
        std::string key;
        if (!r.string(key) || !r.eat(':')) return false;
        if (key == "truncated") {
            r.ws();
            if (json.compare(r.i, 4, "true") == 0) return false;  // cut short: the index would miss games
            if (!r.skip()) return false;
        } else if (key == "tree") {
            if (!r.eat('[')) return false;
            sawTree = true;
            if (!r.eat(']')) {
                do {
                    if (!r.eat('{')) return false;
                    TreeItem item;
                    if (!r.eat('}')) {
                        do {
                            std::string field;
                            if (!r.string(field) || !r.eat(':')) return false;
                            r.ws();
                            std::string* target = field == "path" ? &item.path : field == "type" ? &item.type : field == "sha" ? &item.sha : nullptr;
                            if (target) {
                                if (!r.string(*target)) return false;
                            } else if (!r.skip()) {
                                return false;
                            }
                        } while (r.eat(','));
                        if (!r.eat('}')) return false;
                    }
                    out.push_back(std::move(item));
                } while (r.eat(','));
                if (!r.eat(']')) return false;
            }
        } else if (!r.skip()) {
            return false;
        }
    } while (r.eat(','));
    return r.eat('}') && sawTree;
}

std::string findSha(const std::vector<TreeItem>& items, const std::string& name, const char* type) {
    for (const TreeItem& it : items) {
        if (it.path == name && it.type == type) return it.sha;
    }
    return "";
}

std::map<std::string, Remote> indexImages(const std::vector<TreeItem>& items) {
    std::map<std::string, Remote> index;
    for (const TreeItem& it : items) {
        if (it.type != "blob" || it.path.size() <= 4 || lower(it.path.substr(it.path.size() - 4)) != ".png") continue;
        if (it.path.find('/') != std::string::npos) continue;
        const std::string stem = it.path.substr(0, it.path.size() - 4);
        index.emplace(lower(stem), Remote{stem, it.sha});
    }
    return index;
}

std::vector<std::string> candidateNames(const std::string& gameName) {
    auto sanitize = [](std::string s) {
        for (char& c : s) {
            if (std::strchr("&*/:`<>?\\|", c)) c = '_';
        }
        return s;
    };
    std::vector<std::string> names{sanitize(gameName)};
    // "Game (USA) [BLUS12345]" -> "Game (USA)"
    std::string bare;
    int depth = 0;
    for (const char c : gameName) {
        if (c == '[') ++depth;
        else if (c == ']' && depth > 0) --depth;
        else if (depth == 0) bare.push_back(c);
    }
    while (!bare.empty() && bare.back() == ' ') bare.pop_back();
    while (!bare.empty() && bare.front() == ' ') bare.erase(bare.begin());
    if (!bare.empty() && bare != gameName) names.push_back(sanitize(bare));
    return names;
}

const Remote* find(const std::map<std::string, Remote>& index, const std::string& gameName) {
    for (const std::string& n : candidateNames(gameName)) {
        const auto it = index.find(lower(n));
        if (it != index.end()) return &it->second;
    }
    return nullptr;
}

namespace {
std::string urlEncode(const std::string& text) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (const char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(ch);
        } else {
            out.push_back('%');
            out.push_back(digits[c >> 4]);
            out.push_back(digits[c & 0xF]);
        }
    }
    return out;
}
}  // namespace

std::string imageUrl(const Source& s, const std::string& remoteName) {
    return std::string("https://raw.githubusercontent.com/") + s.repo + "/" + s.branch + "/" + s.folder + "/" + urlEncode(remoteName) + ".png";
}

std::string treeUrl(const Source& s, const std::string& ref) {
    return std::string("https://api.github.com/repos/") + s.repo + "/git/trees/" + urlEncode(ref);
}

}  // namespace covers
