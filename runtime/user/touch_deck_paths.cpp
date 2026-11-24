// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Where the deck's file and the player's own pictures live: beside the saves
// and settings (user/paths.h), except on iOS, where the Files app shows the
// app's Documents folder and nothing else -- and iOS is the one platform where
// the player cannot open a terminal to find a file.
//
// Kept apart from touch_deck_file.cpp, which is what the file says: that one
// has no idea where it lives, so tests/touch_deck_file_test.cpp can point it at
// a temporary folder without linking the runtime's paths.
#include "touch_deck_file.h"
#include "paths.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace touchdeck
{
    std::filesystem::path Folder()
    {
#if defined(__APPLE__) && TARGET_OS_IOS
        // On My iPhone/iPad > SpeedBreaker. (GetUserPath() is the app's
        // Library/Application Support, which the Files app does not show.)
        if (const char* home = std::getenv("HOME"); home && *home)
            return std::filesystem::path(home) / "Documents" / kUserFolderName;
#endif
        return GetUserPath();
    }

    std::filesystem::path Path()
    {
        return Folder() / "touch-deck.toml";
    }

    std::filesystem::path PicturesPath()
    {
        return Folder() / "touch-pictures";
    }

    Store& Current()
    {
        static Store store{ Path() };
        return store;
    }
}
