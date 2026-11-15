#include "mygames.h"

#include <algorithm>
#include <map>
#include <set>

namespace mygames {
namespace {

const char kOrderTag[] = "<I class=\"type:x-xmb/module-action\"";

// Reads the text between `open` and `close` inside [from, to); empty when absent.
std::string between(const std::string& s, std::size_t from, std::size_t to, const std::string& open,
                    const std::string& close) {
    const auto b = s.find(open, from);
    if (b == std::string::npos || b >= to) return "";
    const auto start = b + open.size();
    const auto e = s.find(close, start);
    if (e == std::string::npos || e > to) return "";
    return s.substr(start, e - start);
}

std::string decodeEntities(const std::string& s) {
    static const struct {
        const char* from;
        const char* to;
    } kEntities[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        bool done = false;
        if (s[i] == '&') {
            for (const auto& e : kEntities) {
                const std::string from = e.from;
                if (s.compare(i, from.size(), from) == 0) {
                    out += e.to;
                    i += from.size();
                    done = true;
                    break;
                }
            }
        }
        if (!done) out += s[i++];
    }
    return out;
}

// Value of attribute `name` inside the tag text [b, e).
std::string attr(const std::string& s, std::size_t b, std::size_t e, const std::string& name) {
    const std::string needle = " " + name + "=\"";
    const auto p = s.find(needle, b);
    if (p == std::string::npos || p >= e) return "";
    const auto start = p + needle.size();
    const auto q = s.find('"', start);
    if (q == std::string::npos || q > e) return "";
    return s.substr(start, q - start);
}

std::string platformOf(const std::string& id) {
    // seg_wm_ps3_items -> ps3
    const std::string prefix = "seg_wm_", suffix = "_items";
    if (id.size() <= prefix.size() + suffix.size()) return "";
    if (id.compare(0, prefix.size(), prefix) != 0) return "";
    if (id.compare(id.size() - suffix.size(), suffix.size(), suffix) != 0) return "";
    return id.substr(prefix.size(), id.size() - prefix.size() - suffix.size());
}

}  // namespace

bool balanced(const std::string& text) {
    std::vector<std::string> stack;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] != '<') {
            ++i;
            continue;
        }
        const auto close = text.find('>', i);
        if (close == std::string::npos) return false;
        const std::string tag = text.substr(i + 1, close - i - 1);  // between < and >
        i = close + 1;
        if (!tag.empty() && (tag[0] == '?' || tag[0] == '!')) continue;
        if (!tag.empty() && tag[0] == '/') {  // closing tag; "</>" closes a "<>"
            const std::string name = tag.substr(1);
            if (stack.empty() || stack.back() != name) return false;
            stack.pop_back();
            continue;
        }
        if (!tag.empty() && tag.back() == '/') continue;  // self-closing
        const auto sp = tag.find_first_of(" \t");
        stack.push_back(sp == std::string::npos ? tag : tag.substr(0, sp));
    }
    return stack.empty();
}

std::string platformLabel(const std::string& platform) {
    if (platform == "ps3") return "PS3";
    if (platform == "ps2") return "PS2";
    if (platform == "psx") return "PS1";
    if (platform == "psp") return "PSP";
    std::string up = platform;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up;
}

std::optional<List> List::parse(const std::string& text) {
    if (!balanced(text)) return std::nullopt;

    List list;
    list.text_ = text;

    // Segment boundaries: every `<V id="...">` up to the next one (or the end).
    struct Span {
        std::string id;
        std::size_t begin, end;
    };
    std::vector<Span> spans;
    const std::string vOpen = "<V id=\"";
    for (auto p = text.find(vOpen); p != std::string::npos; p = text.find(vOpen, p + 1)) {
        const auto idStart = p + vOpen.size();
        const auto idEnd = text.find('"', idStart);
        if (idEnd == std::string::npos) return std::nullopt;
        spans.push_back({text.substr(idStart, idEnd - idStart), p, text.size()});
    }
    for (std::size_t i = 0; i + 1 < spans.size(); ++i) spans[i].end = spans[i + 1].begin;

    for (const Span& sp : spans) {
        const std::string platform = platformOf(sp.id);
        if (platform.empty()) continue;

        // Definitions: <T key="K" include="inc"> ... </T>
        std::map<std::string, Game> defs;
        const std::string tOpen = "<T key=\"";
        for (auto p = text.find(tOpen, sp.begin); p != std::string::npos && p < sp.end; p = text.find(tOpen, p + 1)) {
            const auto tagEnd = text.find('>', p);
            const auto bodyEnd = text.find("</T>", p);
            if (tagEnd == std::string::npos || bodyEnd == std::string::npos || bodyEnd > sp.end) return std::nullopt;
            const std::string key = attr(text, p, tagEnd, "key");
            if (key.empty() || key == "inc") continue;
            Game g;
            g.key = key;
            g.title = decodeEntities(between(text, tagEnd, bodyEnd, "<P key=\"title\"><>", "</>"));
            g.icon = decodeEntities(between(text, tagEnd, bodyEnd, "<P key=\"icon\"><>", "</>"));
            g.action = decodeEntities(between(text, tagEnd, bodyEnd, "<P key=\"module_action\"><>", "</>"));
            defs[key] = g;
        }

        // Display order: <I class="type:x-xmb/module-action" key="K" attr="K"/>
        std::vector<Slot> slots;
        Segment seg;
        seg.id = sp.id;
        seg.platform = platform;
        std::set<std::string> seen;
        for (auto p = text.find(kOrderTag, sp.begin); p != std::string::npos && p < sp.end;
             p = text.find(kOrderTag, p + 1)) {
            const auto close = text.find("/>", p);
            if (close == std::string::npos || close > sp.end) return std::nullopt;
            Slot slot;
            slot.begin = p;
            slot.end = close + 2;
            slot.key = attr(text, p, close, "key");
            const auto it = defs.find(slot.key);
            if (it == defs.end() || !seen.insert(slot.key).second) return std::nullopt;  // unknown or repeated key
            seg.games.push_back(it->second);
            slots.push_back(slot);
        }
        if (seg.games.empty()) continue;
        list.segments_.push_back(std::move(seg));
        list.slots_.push_back(std::move(slots));
    }
    if (list.segments_.empty()) return std::nullopt;
    return list;
}

std::optional<std::string> List::reordered(std::size_t index, const std::vector<std::string>& order) const {
    std::vector<std::vector<std::string>> orders(segments_.size());
    if (index >= segments_.size()) return std::nullopt;
    orders[index] = order;
    return reorderedAll(orders);
}

std::optional<std::string> List::reorderedAll(const std::vector<std::vector<std::string>>& orders) const {
    if (orders.size() != segments_.size()) return std::nullopt;

    // New text of each slot, in slot order of the whole file.
    std::vector<std::pair<std::size_t, std::pair<std::size_t, std::string>>> edits;  // begin -> (end, text)
    for (std::size_t s = 0; s < segments_.size(); ++s) {
        const std::vector<std::string>& order = orders[s];
        if (order.empty()) continue;
        const auto& slots = slots_[s];
        if (order.size() != slots.size()) return std::nullopt;

        std::map<std::string, std::string> tagOf;
        for (const Slot& slot : slots) tagOf[slot.key] = text_.substr(slot.begin, slot.end - slot.begin);
        std::set<std::string> used;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto it = tagOf.find(order[i]);
            if (it == tagOf.end() || !used.insert(order[i]).second) return std::nullopt;
            edits.push_back({slots[i].begin, {slots[i].end, it->second}});
        }
    }
    std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string out;
    out.reserve(text_.size());
    std::size_t cursor = 0;
    for (const auto& e : edits) {
        out.append(text_, cursor, e.first - cursor);
        out += e.second.second;
        cursor = e.second.first;
    }
    out.append(text_, cursor, std::string::npos);
    if (!balanced(out)) return std::nullopt;
    return out;
}

}  // namespace mygames
