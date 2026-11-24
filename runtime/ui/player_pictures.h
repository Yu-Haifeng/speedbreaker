// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The pictures the player supplies: one PNG per control's picture, in
// touch-pictures/ beside touch-deck.toml, named after the picture slot they
// replace (gas.png, camera.png, gear-up.png...: hid/touch_art.h's kArtKey).
//
// They are packed, at load, into a second sheet the size of the deck's own --
// one texture, the same 272-pixel cells -- and measured the way the packer
// measures the deck's (hid/touch_art_analysis.h), so a button whose picture is
// a triangle is pressed where the triangle is. What the deck draws and what a
// finger presses stay the same thing, whichever picture is on the button.
//
// This file is the CPU half: decode, scale, measure, pack. ui/deck_art.cpp
// uploads the result and puts it in the table (hid/touch_art_state.h); it is
// the only part that knows Vulkan or ImGui. Free of both here, so it can be
// reasoned about (and tested) on its own.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <hid/touch_art.h>

namespace ui::pictures
{
    struct Sheet
    {
        struct Entry
        {
            hid::touch::art::Id id = hid::touch::art::Id::Count;
            hid::touch::art::Info info{};
            // (Info::field points here once uploaded -- but the table owns its
            // own copy: hid::touch::art::Install takes it by move.)
            std::vector<uint8_t> field;
        };

        int width = 0, height = 0;
        std::vector<uint8_t> rgba;  // width * height * 4, ready to upload
        std::vector<Entry> entries;

        bool Empty() const { return entries.empty(); }
    };

    // Reads `folder` and packs whatever it finds there into `out`. True with an
    // empty sheet when the player has supplied nothing; false with `why` when
    // the folder holds pictures but none could be used.
    bool Build(const std::filesystem::path& folder, Sheet& out, std::string& why);

    // The names a picture may have, in art::Id order (hid/touch_art.h's
    // kArtKey, which is what touch-deck.toml calls them too).
    const char* NameOf(hid::touch::art::Id id);
}
