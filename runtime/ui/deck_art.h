// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The touch deck's button art on the GPU: one texture, packed by
// scripts/gen_deck_atlas.py from the NFS.MW.IOS deck's pictures (a dark plate
// with a white glyph), compiled into the game as a PNG and decoded at startup.
//
//   - The art is drawn as it is: the plate's translucency is in its own alpha
//     (the packer has already baked the second pass the handoff's look wanted),
//     so a button needs no glass behind it. A held button is the same picture
//     tinted -- the glyph is white and the plate black, so a tint with the
//     accent turns the glyph and leaves the plate alone, which is the press
//     state with no second set of pictures.
//   - One atlas, one texture, one draw list: the cells and their trimmed
//     bounds are hid/touch_art.h's, packed from the same PNG, so nothing here
//     knows a grid.
//   - Decoded with stb_image (thirdparty/stb_image, PNG only) and handed to the
//     ImGui Vulkan backend as an ImTextureData, which makes the image, the
//     staging copy and the descriptor itself.
#pragma once
#include <cstddef>
#include <cstdint>

#include <imgui.h>

#include <hid/touch_art.h>

namespace ui
{
    struct VulkanTarget;

    namespace deckart
    {
        using Art = hid::touch::art::Id;
        constexpr size_t kCount = hid::touch::art::kCount;

        // Main thread, after the UI's Vulkan backend is up. False: no art, and
        // the deck draws its controls bare.
        bool Initialize(const VulkanTarget& target);
        // Before the backend shuts down (it frees the descriptor).
        void Shutdown();

        bool Ready();
        // The atlas as a draw list texture, null without one.
        ImTextureRef Atlas();
        // A picture's cell, bounds and shape (hid/touch_art.h --
        // hid/touch_art_state.h's table, which a player's picture replaces).
        const hid::touch::art::Info& Info(Art art);

        // The player's own pictures (ui/player_pictures.h): a second sheet,
        // built from touch-pictures/ and installed over the deck's. Called
        // after Initialize and after the editor saves; false while there are
        // none, and the deck's own pictures are what is drawn.
        bool PlayerReady();
        ImTextureRef PlayerAtlas();
        void ReloadPlayerPictures();
        // Between frames: retires the sheet a reload replaced (its last frame
        // has to leave the GPU before the texture goes).
        void Tick();
        // Before the backend shuts down, with Shutdown.
        void ShutdownPlayer();
    }
}
