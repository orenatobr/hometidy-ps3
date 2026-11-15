#include "sorter.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace sorter {
namespace {

// Folds the UTF-8 Latin-1 letters (U+00C0..U+00FF) to ASCII; '\0' means "drop".
char foldLatin1(unsigned cp) {
    static const char* kMap =
        "aaaaaaaceeeeiiii"   // C0..CF (U+00C6 AE and U+00C7 C handled approximately)
        "dnooooo\0ouuuuyts"  // D0..DF
        "aaaaaaaceeeeiiii"   // E0..EF
        "dnooooo\0ouuuuyty"; // F0..FF
    if (cp < 0xC0 || cp > 0xFF) return '\0';
    return kMap[cp - 0xC0];
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Compares strings treating digit runs as numbers ("mario kart 8" < "mario kart 10").
int naturalCompare(const std::string& a, const std::string& b) {
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (std::isdigit((unsigned char)a[i]) && std::isdigit((unsigned char)b[j])) {
            std::size_t ie = i, je = j;
            while (ie < a.size() && std::isdigit((unsigned char)a[ie])) ++ie;
            while (je < b.size() && std::isdigit((unsigned char)b[je])) ++je;
            const unsigned long long x = strtoull(a.substr(i, ie - i).c_str(), nullptr, 10);
            const unsigned long long y = strtoull(b.substr(j, je - j).c_str(), nullptr, 10);
            if (x != y) return x < y ? -1 : 1;
            i = ie;
            j = je;
        } else {
            if (a[i] != b[j]) return (unsigned char)a[i] < (unsigned char)b[j] ? -1 : 1;
            ++i;
            ++j;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

std::string join(const std::vector<std::string>& w, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count && i < w.size(); ++i) {
        if (i) out += ' ';
        out += w[i];
    }
    return out;
}

// Value of a Roman numeral word that is safe to read as a sequel number (I and X are too ambiguous).
int romanValue(const std::string& w) {
    static const struct {
        const char* text;
        int value;
    } kRoman[] = {{"ii", 2},  {"iii", 3}, {"iv", 4},   {"v", 5},    {"vi", 6},   {"vii", 7},
                  {"viii", 8}, {"ix", 9}, {"xi", 11},  {"xii", 12}, {"xiii", 13}, {"xiv", 14},
                  {"xv", 15},  {"xvi", 16}};
    for (const auto& r : kRoman) {
        if (w == r.text) return r.value;
    }
    return 0;
}

bool allDigits(const std::string& w) {
    if (w.empty() || w.size() > 4) return false;
    for (char c : w) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// True when `needle` occurs in `hay` as whole words.
bool containsWords(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return false;
    for (std::size_t pos = hay.find(needle); pos != std::string::npos; pos = hay.find(needle, pos + 1)) {
        const bool startOk = pos == 0 || hay[pos - 1] == ' ';
        const std::size_t end = pos + needle.size();
        const bool endOk = end == hay.size() || hay[end] == ' ';
        if (startOk && endOk) return true;
    }
    return false;
}

// Reads "(Disc 2)" style groups; returns the disc number or 0.
int discNumber(const std::string& group) {
    const std::string g = lower(trim(group));
    for (const char* prefix : {"disc ", "disk ", "cd "}) {
        const std::string p = prefix;
        if (g.compare(0, p.size(), p) == 0) return std::atoi(g.c_str() + p.size());
    }
    return 0;
}

// Removes the "(Disc N)" group from a cleaned title and returns N.
int takeDisc(std::string& cleaned) {
    const auto open = cleaned.find('(');
    if (open == std::string::npos) return 0;
    const auto close = cleaned.find(')', open);
    if (close == std::string::npos) return 0;
    const int disc = discNumber(cleaned.substr(open + 1, close - open - 1));
    if (disc == 0) return 0;
    cleaned = trim(cleaned.substr(0, open) + " " + cleaned.substr(close + 1));
    return disc;
}

}  // namespace

std::string normalize(const std::string& text) {
    std::string out;
    bool pendingSpace = false;
    auto emit = [&](char c) {
        if (pendingSpace && !out.empty()) out += ' ';
        pendingSpace = false;
        out += c;
    };
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = text[i];
        if (c < 0x80) {
            ++i;
            if (std::isalnum(c)) emit(char(std::tolower(c)));
            else if (c == '\'') continue;  // "Luigi's" -> "luigis"
            else pendingSpace = true;
        } else if (c == 0xC3 && i + 1 < text.size()) {
            const char f = foldLatin1(0xC0u + (static_cast<unsigned char>(text[i + 1]) - 0x80u));
            i += 2;
            if (f) emit(f);
        } else {
            // Any other multi-byte symbol (TM, R, curly quotes...) acts as a separator.
            ++i;
            while (i < text.size() && (text[i] & 0xC0) == 0x80) ++i;
            pendingSpace = true;
        }
    }
    for (const char* article : {"the ", "a ", "an "}) {
        const std::string a = article;
        if (out.compare(0, a.size(), a) == 0 && out.size() > a.size()) {
            out.erase(0, a.size());
            break;
        }
    }
    return out;
}

std::string cleanTitle(const std::string& title) {
    std::string out;
    for (std::size_t i = 0; i < title.size();) {
        const char c = title[i];
        if (c == '(' || c == '[') {
            const char closer = c == '(' ? ')' : ']';
            const auto end = title.find(closer, i + 1);
            if (end == std::string::npos) {  // unbalanced: keep the rest as it is
                out += title.substr(i);
                break;
            }
            const std::string inner = title.substr(i + 1, end - i - 1);
            if (c == '(' && discNumber(inner) > 0) out += title.substr(i, end - i + 1);
            i = end + 1;
        } else {
            out += c;
            ++i;
        }
    }
    // collapse blanks
    std::string squeezed;
    bool space = false;
    for (char c : out) {
        if (c == ' ' || c == '\t') {
            space = true;
            continue;
        }
        if (space && !squeezed.empty()) squeezed += ' ';
        space = false;
        squeezed += c;
    }
    return squeezed.empty() ? trim(title) : squeezed;
}

std::string prettyTitle(const std::string& title) {
    std::string s = cleanTitle(title);
    for (std::size_t pos = s.find(" - "); pos != std::string::npos; pos = s.find(" - ", pos + 2)) {
        s.replace(pos, 3, ": ");
    }
    return s;
}

std::vector<Rule> parseRules(const std::string& text) {
    std::vector<Rule> rules;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        const auto p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        const auto p2 = line.find('|', p1 + 1);
        Rule r;
        r.pattern = normalize(trim(line.substr(0, p1)));
        r.series = normalize(trim(line.substr(p1 + 1, p2 == std::string::npos ? std::string::npos : p2 - p1 - 1)));
        if (p2 != std::string::npos) {
            const std::string d = trim(line.substr(p2 + 1));
            const int year = std::atoi(d.c_str());
            int month = 0;
            const auto dash = d.find_first_of("-/");
            if (dash != std::string::npos) month = std::atoi(d.c_str() + dash + 1);
            if (year >= 1900 && year <= 2999) r.date = year * 100 + (month >= 1 && month <= 12 ? month : 0);
        }
        if (!r.pattern.empty() && !r.series.empty()) rules.push_back(r);
    }
    return rules;
}

Key makeKey(const std::string& displayName, const std::vector<Rule>& rules) {
    Key k;
    std::string cleaned = cleanTitle(displayName);
    k.disc = takeDisc(cleaned);
    k.name = normalize(cleaned);
    k.series = k.name;

    for (const Rule& r : rules) {
        if (containsWords(k.name, r.pattern)) {
            k.series = r.series;
            k.date = r.date;
            return k;
        }
    }

    // Unlisted title: group by the head ("Resistance: Fall of Man" -> "resistance") and read a trailing number.
    std::string head = cleaned;
    for (const char* sep : {":", " - "}) {
        const auto pos = head.find(sep);
        if (pos != std::string::npos && pos > 0) head.erase(pos);
    }
    std::vector<std::string> w = words(normalize(head));
    if (w.size() >= 2) {
        const std::string& last = w.back();
        int n = 0;
        if (allDigits(last)) n = std::atoi(last.c_str());
        else n = romanValue(last);
        if (n > 0) {
            k.ordinal = n;
            w.pop_back();
        }
    }
    if (!w.empty()) k.series = join(w, w.size());
    return k;
}

bool less(const Key& a, const Key& b) {
    if (int c = naturalCompare(a.series, b.series)) return c < 0;
    if (a.date != b.date) return a.date < b.date;
    if (a.ordinal != b.ordinal) return a.ordinal < b.ordinal;
    if (int c = naturalCompare(a.name, b.name)) return c < 0;
    return a.disc < b.disc;
}

}  // namespace sorter
