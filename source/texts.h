// UI texts in English and Brazilian Portuguese.
#pragma once

namespace strings {

enum class Lang { En, Pt };

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
    HintLanguage,
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
    Count_
};

/// Current language.
Lang current();
void set(Lang lang);

/// Text for the current language.
const char* get(Id id);

}  // namespace strings
