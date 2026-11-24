// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Which picture each control draws, and what is known about it: the table that
// hid/touch_art.h compiles in, with whatever the player has changed on top.
//
// Two things can differ from the generated header, and both are the player's:
//   - A control can draw a picture other than the one hid/touch_spec.h gives it
//     (touch-deck.toml's `icon`, chosen in ui/touch_deck_editor.cpp).
//   - A picture can be replaced by one of the player's own
//     (ui/player_pictures.cpp): its cell moves to a second texture, and its
//     trimmed bounds, aspect and distance field are measured at load.
//
// Header only and free of SDL and ImGui, like the rest of hid/: the deck's
// geometry reads the same table whether a picture came from the atlas or from
// the player's folder, so a shape-aware hit test needs no second code path.
//
// Main thread only, and the table is written between frames (never while a
// HitTest or a draw list is walking it).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <hid/touch_art.h>
#include <hid/touch_control.h>
#include <hid/touch_spec.h>

namespace hid::touch::art
{
    namespace state
    {
        // The generated table, copied so that a picture can be replaced without
        // touching a constexpr one.
        inline std::array<Info, kCount> Bundled()
        {
            std::array<Info, kCount> all{};
            for (size_t i = 0; i < kCount; ++i)
                all[size_t(i)] = kArt[i];
            return all;
        }

        inline std::array<Info, kCount> s_info = Bundled();
        // The distance field of an installed picture, owned here: Info::field
        // points into it, so it must not move or free while the deck uses it.
        inline std::array<std::vector<uint8_t>, kCount> s_field;
        // What each control picked, or Id::Count for "the spec's own".
        inline std::array<Id, kControls> s_chosen = [] {
            std::array<Id, kControls> chosen{};
            chosen.fill(Id::Count);
            return chosen;
        }();
    }

    // What is known about a picture: the bundled one, or the player's.
    inline const Info& InfoOf(Id id)
    {
        const size_t index = size_t(id);
        return state::s_info[index < kCount ? index : 0];
    }

    // The picture a control draws with: its spec's, unless the player chose
    // another.
    inline Id ArtOf(Control control)
    {
        const Id chosen = state::s_chosen[size_t(control)];
        return chosen < Id::Count ? chosen : SpecOf(control).art;
    }

    // Id::Count: back to the spec's picture.
    inline void ChooseArt(Control control, Id id)
    {
        state::s_chosen[size_t(control)] = id < Id::Count ? id : Id::Count;
    }

    // Replace a picture's metadata and field with a loaded one (ui/player_pictures.cpp).
    // `info.field` is taken from `field`, so it is the caller's to fill; `sheet`
    // says which texture its cell is in.
    inline void Install(Id id, const Info& info, std::vector<uint8_t>&& field)
    {
        const size_t index = size_t(id);
        if (index >= kCount)
            return;
        state::s_field[index] = std::move(field);
        state::s_info[index] = info;
        state::s_info[index].field = state::s_field[index].data();
    }

    // Back to the bundled picture (a player's file went away, or is being
    // replaced: nothing may draw a cell of a texture that is about to die).
    inline void Restore(Id id)
    {
        const size_t index = size_t(id);
        if (index >= kCount)
            return;
        state::s_info[index] = kArt[index];
        state::s_field[index] = {};
    }

    inline void RestoreAll()
    {
        for (size_t i = 0; i < kCount; ++i)
            Restore(Id(i));
    }
}
