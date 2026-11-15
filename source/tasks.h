// The two jobs of the app (sort the game list, mark a game as completed), written against plain
// file paths so they run the same on the console and on the host.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "sorter.h"

namespace tasks {

/// Outcome of a write job. The UI turns it into a localized message.
enum class Status {
    Ok,
    ReadFailed,    ///< The file could not be read.
    Changed,       ///< The file changed since it was planned (webMAN wrote it meanwhile).
    BackupFailed,
    WriteFailed,
    VerifyFailed,  ///< Read-back differs from what was written.
    NothingToDo,
    NoBackup,      ///< Unmark requested but nothing says the cover carries a badge.
    Unsupported,   ///< The cover is not a PNG/JPEG of a usable size.
};

struct Result {
    Status status = Status::Ok;
    std::string backupPath;
    bool approximate = false;  ///< unmark without a saved original: pixels were rebuilt
};

/// One game row of a preview.
struct PlanRow {
    std::string key;
    std::string name;  ///< Title to show.
    int from = 0;      ///< Position before, 0-based.
    int to = 0;        ///< Position after, 0-based.
};

/// Preview of one platform segment (PS3, PS2, PS1...).
struct SegmentPlan {
    std::string id;
    std::string label;
    std::vector<PlanRow> rows;  ///< In the new order.
    bool changed = false;
    int moves() const;          ///< Rows whose position changes.
};

/// Result of planning a sort; nothing is written yet.
struct SortPlan {
    std::string original;
    std::string sorted;
    std::vector<SegmentPlan> segments;
    int changedSegments = 0;
};

/// Computes the new order of every segment of a game list.
///
/// Args:
///     xml: Content of mygames.xml.
///     rules: Series table.
///
/// Returns:
///     The plan, or nothing when the text is not a game list this app understands.
std::optional<SortPlan> planSort(const std::string& xml, const std::vector<sorter::Rule>& rules);

/// Writes a planned sort: re-checks the file, saves a backup, writes, reads back.
///
/// Args:
///     path: mygames.xml path.
///     plan: Plan made from the content currently in `path`.
///     backupDir: Directory for the timestamped backup copy.
///     stamp: Text used in the backup name (e.g. "20261110_201500").
Result applySort(const std::string& path, const SortPlan& plan, const std::string& backupDir, const std::string& stamp);

/// True when `path` currently holds exactly `expected` (used for the delayed re-check).
bool fileEquals(const std::string& path, const std::string& expected);

/// State of a cover image.
enum class CoverState { Unreadable, Plain, Marked };

/// Reads the cover and tells whether it carries the badge.
CoverState coverState(const std::string& path);

/// Draws the badge on the cover, saving the original first (once).
Result markCover(const std::string& path, const std::string& backupDir);

/// Restores the original cover saved by markCover(). Without a saved original (a mark made by
/// another tool) the badge area is rebuilt from its surroundings and `approximate` is set.
Result unmarkCover(const std::string& path, const std::string& backupDir);

/// Where the first original of a cover is kept.
std::string coverBackupPath(const std::string& coverPath, const std::string& backupDir);

}  // namespace tasks
