// UI texts (US English).
#pragma once

namespace strings {

enum class Id {
    AppTagline,
    Loading,
    Busy,
    SortTitle,
    SortDesc,
    MarkTitle,
    MarkDesc,
    SortStatus,        // "%d games in the list"
    MarkStatus,        // "%d games found"
    HintSelect,
    HintBack,
    HintExit,
    HintApply,
    HintPlatform,
    HintScroll,
    HintPage,
    HintMark,
    HintUnmark,
    HintCancel,
    HintClose,
    ListGames,         // "%d games"
    ListMoves,         // "%d to move"
    ListSorted,
    WasPosition,       // "was %d"
    AlreadySorted,
    NoList,
    ListFile,
    ConfirmTitle,
    ConfirmBody,
    DoneTitle,
    DoneBody,
    DoneBackup,
    DoneRecheckOk,
    DoneRecheckBad,
    FailTitle,
    StReadFailed,
    StChanged,
    StBackupFailed,
    StWriteFailed,
    StVerifyFailed,
    StNothing,
    StNoBackup,
    StUnsupported,
    Marked,
    NotMarked,
    NoCover,
    ToastMarked,
    ToastUnmarked,
    ToastUnmarkedApprox,
    ToastAlready,
    ToastNoCover,
    NoGames,
    NoAccess,
    CoversTitle,
    CoversDesc,
    CoversIntro,       // what is about to happen
    CoversSource,      // "Source"
    HintStart,
    HintStop,
    CoversReading,     // "Reading the cover list of %s..."
    CoversChecking,    // "Checking %s"
    CvFallback,        // "%d from the fallback server"
    CoversOverall,     // "%d of %d games"
    CoversDone,
    CoversStopped,
    CvAdded,           // "%d new"
    CvReplaced,        // "%d replaced"
    CvSame,            // "%d already up to date"
    CvMissing,         // "%d not in the repository"
    CvFailed,          // "%d failed"
    CoversNetHint,
    CvLinked,          // "%d icons linked in the game list"
    CoversLinkedHint,
    Count_
};

/// Text of a UI string.
const char* get(Id id);

}  // namespace strings
