// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// touch-deck.toml: where the player put the deck's controls, how big they are,
// which pictures they wear, and the editor's own snapping. hid/touch_deck.h
// has what the values mean; this has the file.
//
// It is its own file, not a table of settings.toml, because a placement is not
// a scalar the settings table can hold (an offset, a size and a picture per
// control), and because settings.Save() writes every option it knows -- it
// would drop these on the floor.
//
// Only what differs from the design is written: an untouched deck's file is
// `version = 1` and nothing else. The file is read at startup (before there is
// a window) and written by the editor; a broken one is kept beside itself as
// .bad and the design's layout is used, rather than throwing the player's work
// away.
#pragma once
#include <filesystem>
#include <string>

#include <hid/touch_deck.h>

namespace touchdeck
{
    // The file, as this run has it: one of these is the game's (Current()); the
    // tests make their own over a temporary folder.
    struct Store
    {
        std::filesystem::path path;
        hid::touch::Overrides overrides;
        hid::touch::Grid grid;
        // Written by a newer SpeedBreaker: read as far as we understand it, and
        // never written back (an older version would drop what it does not know).
        bool readOnly = false;

        // Reads `path`. False when a file was there and could not be used: it
        // is kept as .bad, and the design's layout is in force. A missing file
        // is not a failure -- that is a player who has not moved anything.
        bool Load();

        // Writes what differs from the design, then publishes it here. "" or
        // why it could not be written.
        std::string Save(const hid::touch::Overrides& wanted);
        std::string SetGrid(const hid::touch::Grid& wanted);
    };

    // Where the file and the player's own pictures are (user/touch_deck_paths.cpp:
    // the user's data folder, or on iOS the Documents folder the Files app shows).
    std::filesystem::path Folder();
    std::filesystem::path Path();
    std::filesystem::path PicturesPath();

    // The game's one store, over Path().
    Store& Current();
}
