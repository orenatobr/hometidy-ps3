#include "tasks.h"

#include <algorithm>
#include <cstdio>

#include "badge.h"
#include "fsx.h"
#include "mygames.h"

namespace tasks {
namespace {

constexpr std::size_t kMaxCover = 16u * 1024 * 1024;

std::vector<std::uint8_t> bytesOf(const std::string& s) { return {s.begin(), s.end()}; }

bool readCover(const std::string& path, std::vector<std::uint8_t>& file, badge::Image& image) {
    return fsx::readBytes(path, file, kMaxCover) && badge::decode(file, image);
}

std::string baseName(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

int SegmentPlan::moves() const {
    int n = 0;
    for (const auto& r : rows) n += r.from != r.to;
    return n;
}

std::optional<SortPlan> planSort(const std::string& xml, const std::vector<sorter::Rule>& rules) {
    const auto list = mygames::List::parse(xml);
    if (!list) return std::nullopt;

    SortPlan plan;
    plan.original = xml;
    std::vector<std::vector<std::string>> orders(list->segments().size());
    for (std::size_t s = 0; s < list->segments().size(); ++s) {
        const mygames::Segment& seg = list->segments()[s];
        struct Entry {
            PlanRow row;
            sorter::Key key;
        };
        std::vector<Entry> entries;
        for (std::size_t i = 0; i < seg.games.size(); ++i) {
            Entry e;
            e.row.key = seg.games[i].key;
            e.row.name = sorter::prettyTitle(seg.games[i].title);
            e.row.from = int(i);
            e.key = sorter::makeKey(seg.games[i].title, rules);
            entries.push_back(std::move(e));
        }
        std::stable_sort(entries.begin(), entries.end(),
                         [](const Entry& a, const Entry& b) { return sorter::less(a.key, b.key); });

        SegmentPlan sp;
        sp.id = seg.id;
        sp.label = mygames::platformLabel(seg.platform);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            entries[i].row.to = int(i);
            sp.changed = sp.changed || entries[i].row.from != entries[i].row.to;
            orders[s].push_back(entries[i].row.key);
            sp.rows.push_back(entries[i].row);
        }
        plan.changedSegments += sp.changed ? 1 : 0;
        if (!sp.changed) orders[s].clear();  // leave untouched segments exactly as they are
        plan.segments.push_back(std::move(sp));
    }
    const auto sorted = list->reorderedAll(orders);
    if (!sorted) return std::nullopt;
    plan.sorted = *sorted;
    return plan;
}

bool fileEquals(const std::string& path, const std::string& expected) {
    std::string now;
    return fsx::readFile(path, now) && now == expected;
}

// Re-checks the file, saves a backup of what is there, writes `updated`, reads it back.
Result writeWithBackup(const std::string& path, const std::string& original, const std::string& updated, const std::string& backupDir,
                       const std::string& stamp) {
    Result r;
    std::string now;
    if (!fsx::readFile(path, now)) return {Status::ReadFailed, "", false};
    if (now != original) return {Status::Changed, "", false};
    if (!mygames::balanced(updated)) return {Status::WriteFailed, "", false};

    fsx::makeDirs(backupDir);
    r.backupPath = backupDir + "/mygames_" + stamp + ".xml";
    if (!fsx::writeBytes(r.backupPath, bytesOf(now)) || !fileEquals(r.backupPath, now)) {
        return {Status::BackupFailed, "", false};
    }
    if (!fsx::writeBytes(path, bytesOf(updated))) return {Status::WriteFailed, r.backupPath, false};
    if (!fileEquals(path, updated)) return {Status::VerifyFailed, r.backupPath, false};
    r.status = Status::Ok;
    return r;
}

Result applySort(const std::string& path, const SortPlan& plan, const std::string& backupDir, const std::string& stamp) {
    if (plan.changedSegments == 0) return {Status::NothingToDo, "", false};
    return writeWithBackup(path, plan.original, plan.sorted, backupDir, stamp);
}

std::optional<IconPlan> planIcons(const std::string& xml, const std::function<std::string(const std::string&)>& coverOf) {
    if (!mygames::balanced(xml)) return std::nullopt;
    IconPlan plan;
    plan.original = xml;
    plan.fixed = mygames::relinkIcons(xml, coverOf, &plan.changed);
    if (plan.changed == 0) return std::nullopt;
    return plan;
}

Result applyIcons(const std::string& path, const IconPlan& plan, const std::string& backupDir, const std::string& stamp) {
    if (plan.changed == 0) return {Status::NothingToDo, "", false};
    return writeWithBackup(path, plan.original, plan.fixed, backupDir, stamp);
}

std::string coverBackupPath(const std::string& coverPath, const std::string& backupDir) {
    char hash[16];
    std::snprintf(hash, sizeof hash, "%08x", fsx::fnv1a(coverPath));
    return backupDir + "/covers/" + hash + "_" + baseName(coverPath);
}

CoverState coverState(const std::string& path) {
    std::vector<std::uint8_t> file;
    badge::Image img;
    if (!readCover(path, file, img)) return CoverState::Unreadable;
    return badge::isMarked(img) ? CoverState::Marked : CoverState::Plain;
}

Result markCover(const std::string& path, const std::string& backupDir) {
    std::vector<std::uint8_t> file;
    badge::Image img;
    if (!fsx::readBytes(path, file, kMaxCover)) return {Status::ReadFailed, "", false};
    if (!badge::decode(file, img)) return {Status::Unsupported, "", false};
    if (badge::isMarked(img)) return {Status::NothingToDo, "", false};
    const badge::Format format = badge::sniff(file);
    badge::apply(img);
    std::vector<std::uint8_t> marked;
    if (!badge::encode(img, format, marked)) return {Status::Unsupported, "", false};

    Result r;
    r.backupPath = coverBackupPath(path, backupDir);
    if (!fsx::exists(r.backupPath)) {  // keep the very first original
        fsx::makeDirs(backupDir + "/covers");
        if (!fsx::writeBytes(r.backupPath, file) || !fsx::exists(r.backupPath)) return {Status::BackupFailed, "", false};
        std::vector<std::uint8_t> check;
        if (!fsx::readBytes(r.backupPath, check, kMaxCover) || check != file) return {Status::BackupFailed, "", false};
    }
    if (!fsx::writeBytes(path, marked)) return {Status::WriteFailed, r.backupPath, false};
    std::vector<std::uint8_t> back;
    if (!fsx::readBytes(path, back, kMaxCover) || back != marked) return {Status::VerifyFailed, r.backupPath, false};
    return r;
}

Result unmarkCover(const std::string& path, const std::string& backupDir) {
    Result r;
    r.backupPath = coverBackupPath(path, backupDir);
    std::vector<std::uint8_t> original;
    if (fsx::readBytes(r.backupPath, original, kMaxCover)) {
        if (!fsx::writeBytes(path, original)) return {Status::WriteFailed, r.backupPath, false};
        std::vector<std::uint8_t> back;
        if (!fsx::readBytes(path, back, kMaxCover) || back != original) return {Status::VerifyFailed, r.backupPath, false};
        return r;
    }
    // No saved original (marked by another tool): rebuild the covered pixels instead of failing.
    std::vector<std::uint8_t> file;
    badge::Image img;
    if (!readCover(path, file, img) || !badge::isMarked(img)) return {Status::NoBackup, "", false};
    badge::erase(img);
    std::vector<std::uint8_t> rebuilt;
    if (!badge::encode(img, badge::sniff(file), rebuilt)) return {Status::Unsupported, "", false};
    if (!fsx::writeBytes(path, rebuilt)) return {Status::WriteFailed, "", false};
    std::vector<std::uint8_t> back;
    if (!fsx::readBytes(path, back, kMaxCover) || back != rebuilt) return {Status::VerifyFailed, "", false};
    r.backupPath.clear();
    r.approximate = true;
    return r;
}

}  // namespace tasks
