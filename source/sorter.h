// Ordering of game titles: alphabetical by series, then by release date inside a series.
#pragma once

#include <string>
#include <vector>

namespace sorter {

/// One line of the series table: titles whose normalized name contains `pattern`
/// (as whole words) belong to `series` and were released on `date`.
struct Rule {
    std::string pattern;  ///< Normalized (see normalize()).
    std::string series;   ///< Normalized series name used as the group key.
    int date = 0;         ///< YYYYMM; the month part is 0 when only the year is known.
};

/// Lowercases, folds Latin accents, drops punctuation and a leading article.
///
/// Args:
///     text: UTF-8 text.
///
/// Returns:
///     Space separated words made of [a-z0-9].
std::string normalize(const std::string& text);

/// Removes the release tags dump names carry: "(USA) (En,Fr) (v02.00) [!]".
///
/// A "(Disc 2)" group is kept because it tells discs of one game apart.
///
/// Args:
///     title: Raw title or file name without extension.
///
/// Returns:
///     The title with single spaces and no surrounding blanks; the input
///     (trimmed) when nothing else would be left.
std::string cleanTitle(const std::string& title);

/// cleanTitle() plus " - " turned into ": " ("God of War - Ascension" -> "God of War: Ascension").
std::string prettyTitle(const std::string& title);

/// Parses a rules file.
///
/// Each line is `pattern | series | date` where date is `YYYY` or `YYYY-MM`;
/// `#` starts a comment. The first matching rule wins, so list specific
/// patterns before generic ones.
std::vector<Rule> parseRules(const std::string& text);

/// Sort key of a title.
struct Key {
    std::string series;
    int date = 0;
    int ordinal = 0;  ///< Trailing number of an unlisted title ("Resistance 2" -> 2).
    int disc = 0;     ///< Disc number, 0 when the title has none.
    std::string name;
};

/// Builds the sort key for a display name or file name.
///
/// A title that matches no rule is grouped by the part before ":" or " - "
/// without its trailing number, so unlisted sequels still sit together.
Key makeKey(const std::string& displayName, const std::vector<Rule>& rules);

/// Strict weak ordering: series, date, ordinal, name, disc; digit runs compare as numbers.
bool less(const Key& a, const Key& b);

}  // namespace sorter
