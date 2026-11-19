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
                            std::string* target = field == "path" ? &item.path : field == "type" ? &item.type : field == "mode" ? &item.mode : field == "sha" ? &item.sha : nullptr;
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

namespace {

std::string sanitize(std::string s) {
    for (char& c : s) {
        if (std::strchr("&*/:`<>?\\|", c)) c = '_';
    }
    return s;
}

std::string trim(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

// "Game (USA) (En,Fr) [x]" -> base "Game", groups {"USA", "En,Fr"} (square brackets are dropped).
struct Parts {
    std::string base;
    std::vector<std::string> groups;
};

Parts split(const std::string& name) {
    Parts p;
    std::string cur;
    int square = 0, round = 0;
    bool seenGroup = false;
    for (const char c : name) {
        if (c == '[') {
            ++square;
        } else if (c == ']' && square > 0) {
            --square;
        } else if (square > 0) {
            continue;
        } else if (c == '(' && round == 0) {
            round = 1;
            seenGroup = true;
            cur.clear();
        } else if (c == ')' && round == 1) {
            round = 0;
            p.groups.push_back(cur);
        } else if (round == 1) {
            cur.push_back(c);
        } else if (!seenGroup) {
            p.base.push_back(c);
        }
    }
    p.base = trim(p.base);
    return p;
}

bool isLanguageList(const std::string& g) {  // "En,Fr,De"
    if (g.size() < 2 || (g.size() + 1) % 3 != 0) return false;
    for (std::size_t i = 0; i < g.size(); i += 3) {
        if (!std::isupper(static_cast<unsigned char>(g[i])) || !std::islower(static_cast<unsigned char>(g[i + 1]))) return false;
        if (i + 2 < g.size() && g[i + 2] != ',') return false;
    }
    return true;
}

bool isVersion(const std::string& g) {  // "v02.00"
    return g.size() > 1 && g[0] == 'v' && std::isdigit(static_cast<unsigned char>(g[1]));
}

std::string join(const std::string& base, const std::vector<std::string>& groups) {
    std::string out = base;
    for (const std::string& g : groups) out += " (" + g + ")";
    return out;
}

std::string alnum(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// Title and disc, nothing else: the same game in another region or revision has the same key.
std::string looseKey(const std::string& name) {
    const Parts p = split(name);
    std::string key = alnum(p.base);
    for (const std::string& g : p.groups) {
        if (lower(g).compare(0, 4, "disc") == 0) key += "|" + alnum(g);
    }
    return key;
}

int regionRank(const std::string& lowerName) {
    if (lowerName.find("(usa") != std::string::npos) return 0;
    if (lowerName.find("(world") != std::string::npos) return 1;
    if (lowerName.find("(europe") != std::string::npos) return 2;
    return 3;
}

}  // namespace

Index indexImages(const std::vector<TreeItem>& items) {
    Index index;
    for (const TreeItem& it : items) {
        if (it.type != "blob" || it.path.size() <= 4 || lower(it.path.substr(it.path.size() - 4)) != ".png") continue;
        if (it.mode != "100644" && it.mode != "120000") continue;  // not a file or a link
        if (it.path.find('/') != std::string::npos) continue;
        const std::string stem = it.path.substr(0, it.path.size() - 4);
        const std::string key = lower(stem);
        if (!index.exact.emplace(key, Remote{stem, it.sha, it.mode == "120000"}).second) continue;
        index.loose[looseKey(stem)].push_back(key);
    }
    return index;
}

std::vector<std::string> candidateNames(const std::string& gameName) {
    std::vector<std::string> names;
    auto add = [&](const std::string& n) {
        const std::string s = sanitize(n);
        if (!s.empty() && std::find(names.begin(), names.end(), s) == names.end()) names.push_back(s);
    };
    add(gameName);
    const Parts p = split(gameName);
    add(join(p.base, p.groups));  // the same without "[...]" tags
    std::vector<std::string> noLang;
    for (const std::string& g : p.groups) {
        if (!isLanguageList(g) && !isVersion(g)) noLang.push_back(g);
    }
    add(join(p.base, noLang));
    std::vector<std::string> firstRegion = noLang;
    for (std::string& g : firstRegion) {
        const auto comma = g.find(',');
        if (comma == std::string::npos) continue;
        const std::string first = g.substr(0, comma);
        if (first == "USA" || first == "Europe" || first == "Japan" || first == "World" || first == "Asia") g = first;
    }
    add(join(p.base, firstRegion));
    std::vector<std::string> noRev;
    for (const std::string& g : firstRegion) {
        if (g.compare(0, 4, "Rev ") != 0) noRev.push_back(g);
    }
    add(join(p.base, noRev));
    return names;
}

const Remote* find(const Index& index, const std::string& gameName) {
    for (const std::string& n : candidateNames(gameName)) {
        const auto it = index.exact.find(lower(n));
        if (it != index.exact.end()) return &it->second;
    }
    return nullptr;
}

const Remote* findLoose(const Index& index, const std::string& gameName) {
    const auto it = index.loose.find(looseKey(gameName));
    if (it == index.loose.end()) return nullptr;
    const std::string* best = nullptr;
    for (const std::string& n : it->second) {
        if (!best || regionRank(n) < regionRank(*best)) best = &n;
    }
    return best ? &index.exact.at(*best) : nullptr;
}

const Remote* findTarget(const Index& index, const std::string& fileName) {
    if (fileName.size() <= 4) return nullptr;
    const auto it = index.exact.find(lower(fileName.substr(0, fileName.size() - 4)));
    return it == index.exact.end() ? nullptr : &it->second;
}

std::string linkTarget(const std::string& blobText) {
    std::string t = blobText;
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    if (t.size() <= 4 || t.size() > 255 || lower(t.substr(t.size() - 4)) != ".png") return "";
    if (t.find('/') != std::string::npos || t.find('\0') != std::string::npos) return "";
    return t;
}

std::vector<std::string> gametdbRegions(const std::string& titleId) {
    const std::string prefix = lower(titleId.substr(0, 4));
    std::vector<std::string> regions;
    if (prefix == "blus" || prefix == "bcus" || prefix == "npub" || prefix == "blud") regions = {"US", "EN"};
    else if (prefix == "bles" || prefix == "bces" || prefix == "npeb" || prefix == "bled") regions = {"EN", "US"};
    else if (prefix == "bljs" || prefix == "bljm" || prefix == "bcjs" || prefix == "bcjb" || prefix == "npjb") regions = {"JA", "US", "EN"};
    else regions = {"US", "EN", "JA"};
    return regions;
}

std::string gametdbUrl(const std::string& titleId, const std::string& region) {
    return "https://art.gametdb.com/ps3/cover/" + region + "/" + titleId + ".jpg";
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
