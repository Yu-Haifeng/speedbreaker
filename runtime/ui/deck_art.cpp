// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See deck_art.h.
#include <stdafx.h>
#include "deck_art.h"
#include "ui.h"

#include <thirdparty/stb_image/stb_image.h>

#include <climits>
#include <memory>
#include <vector>

#include <hid/touch_art_state.h>

#include <user/touch_deck_file.h>

#include "player_pictures.h"

#include <imgui_impl_vulkan.h>

// The atlas, as a PNG compiled in (generated/deck_atlas.inc, written by
// runtime/CMakeLists.txt from ui/touch/deck_atlas.png).
namespace
{
    const unsigned char kAtlasPng[] = {
#include "deck_atlas.inc"
    };
}

namespace ui::deckart
{
    namespace
    {
        // The texture as ImGui 1.92 wants it handed over: the backend makes the
        // image, uploads the pixels and stores the descriptor in it (see
        // imgui_impl_vulkan.cpp's ImGui_ImplVulkan_UpdateTexture). It is ours,
        // not the font atlas's, so it is ours to destroy, too.
        ImTextureData s_texture;
        std::mutex* s_queueMutex = nullptr;

        // The player's sheet (ui/player_pictures.h), and the one a reload
        // replaced -- it stays until its last frame has left the GPU.
        std::unique_ptr<ImTextureData> s_player;
        std::vector<std::unique_ptr<ImTextureData>> s_retiring;

        // Hands `texture` to the backend's destroy path, which frees it only
        // once `UnusedFrames` frames have gone by (imgui_impl_vulkan.cpp: a
        // texture it does not own is not counted for it, which is what Tick is
        // for).
        void Retire(std::unique_ptr<ImTextureData> texture)
        {
            if (!texture)
                return;
            texture->DestroyPixels();
            texture->UnusedFrames = 0;
            texture->SetStatus(ImTextureStatus_WantDestroy);
            s_retiring.push_back(std::move(texture));
        }
    }

    bool Initialize(const VulkanTarget& target)
    {
        if (s_texture.Status == ImTextureStatus_OK)
            return true;  // (already up: a second Initialize would leak the first)

        int width = 0, height = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(kAtlasPng, int(sizeof kAtlasPng), &width, &height, &channels, 4);
        if (!pixels)
        {
            fprintf(stderr, "[touch] the deck's art did not decode: %s\n", stbi_failure_reason());
            return false;
        }
        if (width != hid::touch::art::kAtlasWidth || height != hid::touch::art::kAtlasHeight)
        {
            // (The header is packed from this very PNG, so a mismatch means one
            // of the two is stale: re-run scripts/gen_deck_atlas.py.)
            fprintf(stderr, "[touch] the deck's art is %dx%d, but its header says %dx%d\n", width, height,
                hid::touch::art::kAtlasWidth, hid::touch::art::kAtlasHeight);
            stbi_image_free(pixels);
            return false;
        }

        s_queueMutex = target.queueMutex;
        s_texture.Create(ImTextureFormat_RGBA32, width, height);
        memcpy(s_texture.Pixels, pixels, size_t(width) * size_t(height) * 4);
        stbi_image_free(pixels);

        {
            // (The backend submits the upload on this queue and waits for it.)
            std::lock_guard lock(*s_queueMutex);
            ImGui_ImplVulkan_UpdateTexture(&s_texture);
        }
        if (s_texture.Status != ImTextureStatus_OK)
        {
            fprintf(stderr, "[touch] the deck's art: the upload failed\n");
            return false;
        }
        return true;
    }

    void Shutdown()
    {
        ShutdownPlayer();
        if (s_texture.Status != ImTextureStatus_OK)
            return;
        // The backend only frees a texture it considers unused, and it counts
        // that only for the ones ImGui owns (the font atlas's). This one is
        // past its last frame by the time we are here, so say so.
        s_texture.DestroyPixels();
        s_texture.UnusedFrames = INT_MAX;
        s_texture.SetStatus(ImTextureStatus_WantDestroy);
        if (s_queueMutex)
        {
            std::lock_guard lock(*s_queueMutex);
            ImGui_ImplVulkan_UpdateTexture(&s_texture);
        }
    }

    bool Ready()
    {
        return s_texture.Status == ImTextureStatus_OK;
    }

    ImTextureRef Atlas()
    {
        return Ready() ? s_texture.GetTexRef() : ImTextureRef();
    }

    bool PlayerReady()
    {
        return s_player && s_player->Status == ImTextureStatus_OK;
    }

    ImTextureRef PlayerAtlas()
    {
        return PlayerReady() ? s_player->GetTexRef() : ImTextureRef();
    }

    void ReloadPlayerPictures()
    {
        if (!s_queueMutex)
            return;  // (no GPU yet: Initialize has not run)

        pictures::Sheet sheet;
        std::string why;
        if (!pictures::Build(touchdeck::PicturesPath(), sheet, why))
        {
            fprintf(stderr, "[deck] the pictures in %s: %s\n", touchdeck::PicturesPath().string().c_str(),
                why.c_str());
            return;  // (keep whatever was already up)
        }
        // Nothing may draw a cell of the sheet this replaces: the pictures go
        // back to the deck's own until the new one is up.
        hid::touch::art::RestoreAll();
        Retire(std::move(s_player));
        if (sheet.Empty())
            return;

        auto texture = std::make_unique<ImTextureData>();
        texture->Create(ImTextureFormat_RGBA32, sheet.width, sheet.height);
        memcpy(texture->Pixels, sheet.rgba.data(), sheet.rgba.size());
        {
            std::lock_guard lock(*s_queueMutex);
            ImGui_ImplVulkan_UpdateTexture(texture.get());
        }
        if (texture->Status != ImTextureStatus_OK)
        {
            fprintf(stderr, "[deck] the player's pictures: the upload failed\n");
            texture->DestroyPixels();
            return;
        }
        s_player = std::move(texture);
        for (pictures::Sheet::Entry& entry : sheet.entries)
            hid::touch::art::Install(entry.id, entry.info, std::move(entry.field));
    }

    void Tick()
    {
        if (!s_queueMutex)
            return;
        for (auto it = s_retiring.begin(); it != s_retiring.end();)
        {
            ImTextureData* texture = it->get();
            if (texture->Status == ImTextureStatus_WantDestroy)
                texture->UnusedFrames++;
            {
                std::lock_guard lock(*s_queueMutex);
                ImGui_ImplVulkan_UpdateTexture(texture);
            }
            if (texture->Status == ImTextureStatus_Destroyed)
                it = s_retiring.erase(it);
            else
                ++it;
        }
    }

    void ShutdownPlayer()
    {
        // (Any sheet still retiring holds a Vulkan image too, but the device
        // goes in the same breath: nothing to reap one texture at a time.)
        s_retiring.clear();
        Retire(std::move(s_player));
        for (std::unique_ptr<ImTextureData>& texture : s_retiring)
        {
            texture->UnusedFrames = INT_MAX;
            if (s_queueMutex)
            {
                std::lock_guard lock(*s_queueMutex);
                ImGui_ImplVulkan_UpdateTexture(texture.get());
            }
        }
        s_retiring.clear();
    }

    const hid::touch::art::Info& Info(Art art)
    {
        // (The table, not the generated one: a picture the player supplied is
        // installed over it -- hid/touch_art_state.h.)
        return hid::touch::art::InfoOf(art);
    }
}
