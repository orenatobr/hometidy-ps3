// Reader and editor for the webMAN MOD game list, `mygames.xml`.
//
// The file is a single line of XML-like text with non-standard `<>` tags. Every platform has a
// segment `<V id="seg_wm_<platform>_items">` that holds one definition per game
//   <T key="0134" include="inc"><P key="title"><>Alpha Protocol</></P>...</T>
// and, after the definitions, the display order as a list of
//   <I class="type:x-xmb/module-action" key="0134" attr="0134"/>
// Only that list is reordered; titles, icons and mount paths are never touched, so a sort is
// a permutation of the `<I .../>` tags and the file keeps exactly the same length.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mygames {

/// One game of a segment, in the current display order.
struct Game {
    std::string key;     ///< Key shared by the definition and the order tag.
    std::string title;   ///< Title as stored (entities decoded).
    std::string icon;    ///< Cover path from the definition, empty when missing.
    std::string action;  ///< Mount action, e.g. /mount_ps3/dev_hdd0/PS3ISO/name.iso
};

/// A platform segment.
struct Segment {
    std::string id;           ///< e.g. "seg_wm_ps3_items"
    std::string platform;     ///< "ps3", "ps2", "psx", "psp", ...
    std::vector<Game> games;  ///< Display order.
};

/// Parsed list. Keeps the original text so `reordered()` can rebuild the file.
class List {
public:
    /// Parses a game list.
    ///
    /// Returns:
    ///     The list, or nothing when the text has no game segment, a game has no
    ///     definition, or the tags are not balanced.
    static std::optional<List> parse(const std::string& text);

    const std::vector<Segment>& segments() const { return segments_; }
    const std::string& text() const { return text_; }

    /// Text of the file with the games of segment `index` shown in `order` (keys).
    ///
    /// Args:
    ///     index: Index into segments().
    ///     order: A permutation of that segment's keys.
    ///
    /// Returns:
    ///     The new text, or nothing when `order` is not a permutation of the keys.
    std::optional<std::string> reordered(std::size_t index, const std::vector<std::string>& order) const;

    /// Same as reordered(), applying one order per segment (empty order = leave the segment as it is).
    std::optional<std::string> reorderedAll(const std::vector<std::vector<std::string>>& orders) const;

private:
    struct Slot {
        std::size_t begin = 0, end = 0;  ///< Span of one `<I .../>` tag in text_.
        std::string key;
    };
    std::string text_;
    std::vector<Segment> segments_;
    std::vector<std::vector<Slot>> slots_;  ///< Parallel to segments_.
};

/// Links the cover of every game whose icon is webMAN's placeholder.
///
/// webMAN writes a placeholder icon (a path under `/wm_icons/`) for a game that has no cover when it builds the
/// list, and keeps it until the next rebuild, so a cover added later is not shown. The game is found from its
/// mount action (`/mount_ps3/dev_hdd0/PSXISO/Crash+Bandicoot+(USA).bin`: URL encoded, the file name without its
/// extension is the cover name). An icon that is not a placeholder, a game without a cover and the platform
/// headers are left as they are.
///
/// Args:
///     text: Content of mygames.xml.
///     coverOf: Gets the path of the game without extension ("/dev_hdd0/PSXISO/Crash Bandicoot (USA)") and returns
///         the cover path, or empty when there is none.
///     changed: Receives how many icons were linked.
///
/// Returns:
///     The new text.
std::string relinkIcons(const std::string& text, const std::function<std::string(const std::string&)>& coverOf, int* changed = nullptr);

/// True when every `<tag>` has its `</tag>` (the file uses `<>` ... `</>` pairs as well).
bool balanced(const std::string& text);

/// Short label of a platform id ("ps3" -> "PS3", "psx" -> "PS1").
std::string platformLabel(const std::string& platform);

}  // namespace mygames
